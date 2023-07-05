#include "compression.hpp"

#include <cstring>
#include <exception>

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

static void lzmaCompress(
  std::vector<byte>& dst, const std::vector<byte>& src, std::vector<uint32_t>& compressedBlockSizes, uint32_t blockSize) {
  CLzmaEncProps props;
  LzmaEncProps_Init(&props);

  SizeT propsSize = 5;
  byte propsEncoded[5];

  SizeT uncompressedSize = src.size();

  if (blockSize == 0) {
    blockSize = uncompressedSize;
  }

  SizeT totalCompressedSize = 0;
  SizeT totalProcessedSize  = 0;

  compressedBlockSizes.clear();

  while (totalProcessedSize < uncompressedSize) {
    SizeT processSize         = std::min((SizeT) blockSize, uncompressedSize - totalProcessedSize);
    SizeT compressedBlockSize = processSize * 1.2;

    dst.resize(totalCompressedSize + compressedBlockSize);

    int lzmaStatus = LzmaEncode(
      dst.data() + totalCompressedSize + LZMA_HEADER_SIZE, &compressedBlockSize, src.data() + totalProcessedSize, processSize, &props,
      propsEncoded, &propsSize, 0, nullptr, &lzmaAllocFuncs, &lzmaAllocFuncs);

    if (lzmaStatus == SZ_OK) {
      std::memcpy(dst.data() + totalCompressedSize, propsEncoded, 5);

      // This is funky, LZMA header requires a uint64_t but LZMA uses size_t which is not necessarily that.
      // TODO: Header requires little endian, make it so that libpsarc also works on big endian machines.
      std::memcpy(dst.data() + totalCompressedSize + 5, &processSize, 8);
    }

    compressedBlockSizes.push_back(compressedBlockSize + 13);

    totalCompressedSize += compressedBlockSize + 13;
    totalProcessedSize += processSize;
  }

  dst.resize(totalCompressedSize);
}

static void lzmaDecompress(std::vector<byte>& dst, const std::vector<byte>& src) {
  SizeT totalOutputSize = 0;

  SizeT uncompressedOffset = 0;

  SizeT remainingInput = src.size();
  SizeT inputOffset    = 0;

  ELzmaStatus lzmaStatus;

  while (remainingInput > 0) {
    if (remainingInput < 13) {
      // What should we do? This is clearly not a valid LZMA encoded string.
      throw std::exception();
    }
    SizeT uncompressedSize;
    std::memcpy(&uncompressedSize, src.data() + inputOffset + 5, 8);

    totalOutputSize += uncompressedSize;

    dst.resize(totalOutputSize);

    SizeT processedInput = remainingInput;

    int status = LzmaDecode(
      dst.data() + uncompressedOffset, &uncompressedSize, src.data() + inputOffset + 13, &processedInput, src.data() + inputOffset, 5,
      LZMA_FINISH_END, &lzmaStatus, &lzmaAllocFuncs);

    uncompressedOffset += uncompressedSize;

    remainingInput -= processedInput + 13;
    inputOffset += processedInput + 13;

    if (status != SZ_OK) {
      // What should we do on error?
    }
  }
}

void PSArc::Compress(
  std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type, std::vector<uint32_t>& compressedBlockSizes,
  uint32_t blockSize) {
  switch (type) {
    case CompressionType::LZMA:
      lzmaCompress(dst, src, compressedBlockSizes, blockSize);
      break;
    default:
      break;
  }
}

void PSArc::Compress(std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type) {
  std::vector<uint32_t> _unused = std::vector<uint32_t>();
  Compress(dst, src, type, _unused);
}

void PSArc::Decompress(std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type) {
  switch (type) {
    case CompressionType::LZMA:
      lzmaDecompress(dst, src);
      break;
    default:
      break;
  }
}
