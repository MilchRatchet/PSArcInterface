#include "psarc_compression.hpp"

#include <cstring>
#include <iostream>

#include "LzmaDec.h"
#include "LzmaEnc.h"
#include "zlib.h"

static void* lzmaAlloc(ISzAllocPtr, size_t size) {
  return new byte[size];
}

static void lzmaFree(ISzAllocPtr, void* ptr) {
  if (!ptr)
    return;

  delete[] reinterpret_cast<byte*>(ptr);
}

static ISzAlloc lzmaAllocFuncs = {.Alloc = lzmaAlloc, .Free = lzmaFree};

#define LZMA_HEADER_SIZE 13

void PSArc::LZMACompress(
  std::vector<byte>& dst, const std::vector<byte>& src, std::vector<size_t>& compressedBlockSizes, size_t maxUncompressedBlockSize,
  size_t maxCompressedBlockSize) {
  CLzmaEncProps props;
  LzmaEncProps_Init(&props);

  props.level      = 9;
  props.numThreads = 1;
  props.algo       = 1;  // Fast algo produces much larger output which doesn't match with reference.
  // Explicitly set dictSize to the block size so the 5-byte props header encodes the correct
  // dictionary size. Without this, LzmaEncProps_Normalize sets dictSize = 64 MB (level 9 default)
  // which may or may not be clamped to srcLen before being written to the props bytes depending on
  // the SDK version.  Reference PSArc LZMA blocks use dictSize == blockSize (typically 65536).
  props.dictSize = (UInt32) maxUncompressedBlockSize;

  SizeT propsSize = 5;
  byte propsEncoded[5];

  SizeT uncompressedSize = src.size();

  if (maxUncompressedBlockSize == 0) {
    std::cout << "Fatal Error in compression: maxUncompressedBlockSize was 0." << std::endl;
    return;
  }

  SizeT totalCompressedSize = 0;
  SizeT totalProcessedSize  = 0;

  compressedBlockSizes.clear();

  while (totalProcessedSize < uncompressedSize) {
    SizeT processSize         = std::min((SizeT) maxUncompressedBlockSize, uncompressedSize - totalProcessedSize);
    SizeT compressedBlockSize = maxCompressedBlockSize;

    dst.resize(totalCompressedSize + compressedBlockSize);

    SizeT compressedOutputSize = compressedBlockSize - LZMA_HEADER_SIZE;

    SizeT actualBlockSize = compressedBlockSize;  // how many bytes are actually written to dst

    SRes lzmaStatus = LzmaEncode(
      dst.data() + totalCompressedSize + LZMA_HEADER_SIZE, &compressedOutputSize, src.data() + totalProcessedSize, processSize, &props,
      propsEncoded, &propsSize, 0, nullptr, &lzmaAllocFuncs, &lzmaAllocFuncs);

    if (lzmaStatus == SZ_OK) {
      // All good — write the 13-byte LZMA header followed by the compressed payload.
      actualBlockSize     = compressedOutputSize + LZMA_HEADER_SIZE;
      compressedBlockSize = actualBlockSize;

      std::memcpy(dst.data() + totalCompressedSize, propsEncoded, 5);

      // This is funky, PSArc LZMA block header requires a uint64_t but LZMA uses size_t which is not necessarily that.
      // TODO: Header requires little endian, make it so that libpsarc also works on big endian machines.
      std::memcpy(dst.data() + totalCompressedSize + 5, &processSize, 8);
    }
    else if (lzmaStatus == SZ_ERROR_OUTPUT_EOF) {
      // Compression did not reduce file size, hence we store this block uncompressed (no LZMA header).
      std::memcpy(dst.data() + totalCompressedSize, src.data() + totalProcessedSize, processSize);
      // A full block stored uncompressed is signalled by a block table entry of 0.
      // Partial (last) blocks keep their real size so the reader knows how many bytes to consume.
      // actualBlockSize always tracks the real bytes written, regardless of the table entry.
      actualBlockSize     = processSize;
      compressedBlockSize = (processSize == maxUncompressedBlockSize) ? 0 : processSize;
    }
    else {
      // Probably wanna avoid exceptions and use flags instead.
      std::cout << "Fatal Error in compression: Unhandled LZMA error code (" << lzmaStatus << ")." << std::endl;
      return;
    }

    compressedBlockSizes.push_back(compressedBlockSize);

    totalCompressedSize += actualBlockSize;
    totalProcessedSize += processSize;
  }

  dst.resize(totalCompressedSize);
}

size_t PSArc::LZMADecompress(
  std::vector<byte>& dst, const std::vector<byte>& src, const std::vector<size_t>& compressedBlockSizes,
  const std::vector<bool>& blockIsCompressed) {
  SizeT totalOutputSize = 0;

  SizeT uncompressedOffset = 0;

  SizeT remainingInput = src.size();
  SizeT inputOffset    = 0;

  size_t blockNum = 0;

  ELzmaStatus lzmaStatus;

  while (remainingInput > 0) {
    // When there is no block information, we simply will have no idea but we can simply guess that it must be the last block and that it is
    // not compressed.
    bool isCompressed = (blockNum < blockIsCompressed.size()) ? blockIsCompressed[blockNum] : false;

    if (isCompressed) {
      if (remainingInput < LZMA_HEADER_SIZE) {
        // Compressed blocks must have at least a 13-byte LZMA header.
        std::cout << "Fatal Error in decompression: Encountered non LZMA compliant header." << std::endl;
        return 0;
      }

      SizeT uncompressedSize;
      std::memcpy(&uncompressedSize, src.data() + inputOffset + 5, 8);

      byte propsEncoded[5];
      std::memcpy(propsEncoded, src.data() + inputOffset, 5);

      totalOutputSize += uncompressedSize;

      dst.resize(totalOutputSize);

      SizeT processedInput = compressedBlockSizes[blockNum];

      SRes status = LzmaDecode(
        dst.data() + uncompressedOffset, &uncompressedSize, src.data() + inputOffset + LZMA_HEADER_SIZE, &processedInput,
        src.data() + inputOffset, 5, LZMA_FINISH_END, &lzmaStatus, &lzmaAllocFuncs);

      uncompressedOffset += uncompressedSize;

      remainingInput -= processedInput + LZMA_HEADER_SIZE;
      inputOffset += processedInput + LZMA_HEADER_SIZE;

      if (status != SZ_OK) {
        // What should we do on error?
        std::cout << "Fatal Error in decompression: Encountered unhandled LZMA error code (" << status << ")." << std::endl;
        return 0;
      }
    }
    else {
      // This block is not compressed
      size_t sizeOfBlock = (blockNum < blockIsCompressed.size()) ? compressedBlockSizes[blockNum] : remainingInput;

      totalOutputSize += sizeOfBlock;
      dst.resize(totalOutputSize);
      std::memcpy(dst.data() + uncompressedOffset, src.data() + inputOffset, sizeOfBlock);

      uncompressedOffset += sizeOfBlock;
      remainingInput -= sizeOfBlock;
      inputOffset += sizeOfBlock;
    }

    blockNum++;
  }

  dst.resize(totalOutputSize);
  return totalOutputSize;
}

void PSArc::ZLIBCompress(
  std::vector<byte>& dst, const std::vector<byte>& src, std::vector<size_t>& compressedBlockSizes, size_t maxUncompressedBlockSize,
  size_t maxCompressedBlockSize) {
  SizeT uncompressedSize = src.size();

  if (maxUncompressedBlockSize == 0) {
    std::cout << "Fatal Error in compression: maxUncompressedBlockSize was 0." << std::endl;
    return;
  }

  SizeT totalCompressedSize = 0;
  SizeT totalProcessedSize  = 0;

  compressedBlockSizes.clear();

  while (totalProcessedSize < uncompressedSize) {
    uLong processSize         = (uLong) std::min((SizeT) maxUncompressedBlockSize, uncompressedSize - totalProcessedSize);
    uLong compressedBlockSize = (uLong) maxCompressedBlockSize;

    dst.resize(totalCompressedSize + compressedBlockSize);

    uLong actualBlockSize = compressedBlockSize;  // how many bytes are actually written to dst

    int status =
      compress((Bytef*) (dst.data() + totalCompressedSize), &compressedBlockSize, (Bytef*) (src.data() + totalProcessedSize), processSize);

    if (status == Z_OK) {
      // All good; compressedBlockSize was updated by compress() to actual compressed size.
      actualBlockSize = compressedBlockSize;
    }
    else if (status == Z_BUF_ERROR) {
      // Compression did not reduce file size, hence we store this block uncompressed.
      std::memcpy(dst.data() + totalCompressedSize, src.data() + totalProcessedSize, processSize);
      // A full block stored uncompressed is signalled by a block table entry of 0.
      // Partial (last) blocks keep their real size so the reader knows how many bytes to consume.
      // actualBlockSize always tracks the real bytes written, regardless of the table entry.
      actualBlockSize     = processSize;
      compressedBlockSize = (processSize == (uLong) maxUncompressedBlockSize) ? 0 : processSize;
    }
    else {
      // Probably wanna avoid exceptions and use flags instead.
      std::cout << "Fatal Error in compression: Unhandled ZLIB error code (" << status << ")." << std::endl;
      return;
    }

    compressedBlockSizes.push_back(compressedBlockSize);

    totalCompressedSize += actualBlockSize;
    totalProcessedSize += processSize;
  }

  dst.resize(totalCompressedSize);
}

size_t PSArc::ZLIBDecompress(
  std::vector<byte>& dst, const std::vector<byte>& src, const std::vector<size_t>& compressedBlockSizes,
  const std::vector<bool>& blockIsCompressed) {
  SizeT totalOutputSize = 0;

  SizeT uncompressedOffset = 0;

  SizeT remainingInput = src.size();
  SizeT inputOffset    = 0;

  size_t blockNum = 0;

  while (remainingInput > 0) {
    // When there is no block information, we simply will have no idea but we can simply guess that it must be the last block and that it is
    // not compressed.
    bool isCompressed = (blockNum < blockIsCompressed.size()) ? blockIsCompressed[blockNum] : false;

    if (isCompressed) {
      uLongf processedInput = uLongf(compressedBlockSizes[blockNum]);

      // We don't know the size of the uncompressed block, hence just assume 2x compression and if that wasn't enough,
      // try again with more memory.
      SizeT initialTotalOutputSize = totalOutputSize;
      uLongf uncompressedSize      = processedInput * 2;
      int status;

      while (true) {
        totalOutputSize = initialTotalOutputSize + uncompressedSize;
        dst.resize(totalOutputSize);

        status = uncompress(
          (Bytef*) (dst.data() + uncompressedOffset), &uncompressedSize, (Bytef*) (src.data() + inputOffset), (uLong) processedInput);

        if (status == Z_BUF_ERROR) {
          uncompressedSize *= 2;
        }
        else {
          break;
        }
      }

      totalOutputSize = initialTotalOutputSize + uncompressedSize;
      uncompressedOffset += uncompressedSize;

      remainingInput -= processedInput;
      inputOffset += processedInput;

      if (status != Z_OK) {
        // What should we do on error?
        std::cout << "Fatal Error in decompression: Encountered unhandled ZLIB error code (" << status << ")." << std::endl;
        return 0;
      }

      // Shrink dst to the actual decompressed size for this block.
      totalOutputSize = initialTotalOutputSize + uncompressedSize;
      dst.resize(totalOutputSize);
    }
    else {
      // This block is not compressed
      size_t sizeOfBlock = (blockNum < blockIsCompressed.size()) ? compressedBlockSizes[blockNum] : remainingInput;

      totalOutputSize += sizeOfBlock;
      dst.resize(totalOutputSize);
      std::memcpy(dst.data() + uncompressedOffset, src.data() + inputOffset, sizeOfBlock);

      uncompressedOffset += sizeOfBlock;
      remainingInput -= sizeOfBlock;
      inputOffset += sizeOfBlock;
    }

    blockNum++;
  }

  return totalOutputSize;
}
