#include "psarc_compression.hpp"

#include <cstring>
#include <iostream>

#include "LzmaDec.h"
#include "LzmaEnc.h"

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
  std::vector<byte>& dst, const std::vector<byte>& src, std::vector<uint32_t>& compressedBlockSizes, uint32_t maxUncompressedBlockSize,
  uint32_t maxCompressedBlockSize) {
  CLzmaEncProps props;
  LzmaEncProps_Init(&props);

  props.level = 9;

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

    SRes lzmaStatus = LzmaEncode(
      dst.data() + totalCompressedSize + LZMA_HEADER_SIZE, &compressedOutputSize, src.data() + totalProcessedSize, processSize, &props,
      propsEncoded, &propsSize, 0, nullptr, &lzmaAllocFuncs, &lzmaAllocFuncs);

    if (lzmaStatus == SZ_OK) {
      // All good
    }
    else if (lzmaStatus == SZ_ERROR_OUTPUT_EOF) {
      // Compression did not reduce file size, hence we store this block uncompressed.
      std::memcpy(dst.data() + totalCompressedSize + LZMA_HEADER_SIZE, src.data() + totalProcessedSize, compressedOutputSize);
    }
    else {
      // Probably wanna avoid exceptions and use flags instead.
      std::cout << "Fatal Error in compression: Unhandled LZMA error code (" << lzmaStatus << ")." << std::endl;
      return;
    }
    compressedBlockSize = compressedOutputSize + LZMA_HEADER_SIZE;

    std::memcpy(dst.data() + totalCompressedSize, propsEncoded, 5);

    // This is funky, PSArc LZMA block header requires a uint64_t but LZMA uses size_t which is not necessarily that.
    // TODO: Header requires little endian, make it so that libpsarc also works on big endian machines.
    std::memcpy(dst.data() + totalCompressedSize + 5, &processSize, 8);

    compressedBlockSizes.push_back(compressedBlockSize);

    totalCompressedSize += compressedBlockSize;
    totalProcessedSize += processSize;
  }

  dst.resize(totalCompressedSize);
}

size_t PSArc::LZMADecompress(
  std::vector<byte>& dst, const std::vector<byte>& src, const std::vector<uint32_t>& compressedBlockSizes,
  const std::vector<bool>& blockIsCompressed) {
  SizeT totalOutputSize = 0;

  SizeT uncompressedOffset = 0;

  SizeT remainingInput = src.size();
  SizeT inputOffset    = 0;

  uint32_t blockNum = 0;

  ELzmaStatus lzmaStatus;

  while (remainingInput > 0) {
    if (remainingInput < LZMA_HEADER_SIZE) {
      // What should we do? This is clearly not a valid LZMA encoded string.
      std::cout << "Fatal Error in decompression: Encountered non LZMA compliant header." << std::endl;
      return 0;
    }

    // When there is no block information, we simply will have no idea but we can simply guess that it must be the last block and that it is
    // not compressed.
    bool isCompressed = (blockNum < blockIsCompressed.size()) ? blockIsCompressed[blockNum] : false;

    if (isCompressed) {
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
      uint32_t sizeOfBlock = (blockNum < blockIsCompressed.size()) ? compressedBlockSizes[blockNum] : remainingInput;

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
