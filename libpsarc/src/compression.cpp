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

static void lzmaCompress(std::vector<byte>& dst, const std::vector<byte>& src) {
  CLzmaEncProps props;
  LzmaEncProps_Init(&props);

  // props.dictSize = (src.size() >= (1 << 20)) ? 1 << 20 : src.size();
  // props.fb       = 40;

  SizeT propsSize = 5;
  byte propsEncoded[5];

  SizeT uncompressedSize = src.size();

  // Figure out how to determine this
  SizeT outputSize = uncompressedSize * 1.2 + LZMA_HEADER_SIZE;

  dst.resize(outputSize);

  int lzmaStatus = LzmaEncode(
    dst.data() + LZMA_HEADER_SIZE, &outputSize, src.data(), uncompressedSize, &props, propsEncoded, &propsSize, 0, nullptr, &lzmaAllocFuncs,
    &lzmaAllocFuncs);

  dst.resize(outputSize + LZMA_HEADER_SIZE);

  if (lzmaStatus == SZ_OK) {
    std::memcpy(dst.data(), propsEncoded, 5);

    // This is funky, LZMA header requires a uint64_t but LZMA uses size_t which is not necessarily that.
    // TODO: Header requires little endian, make it so that libpsarc also works on big endian machines.
    std::memcpy(dst.data() + 5, &uncompressedSize, 8);
  }
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

void PSArc::Compress(std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type) {
  switch (type) {
    case CompressionType::LZMA:
      lzmaCompress(dst, src);
      break;
    default:
      break;
  }
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
