#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "psarc_compression.hpp"
#include "psarc_types.hpp"

using namespace PSArc;

namespace {

// Build a repeatably compressible byte buffer of the requested length.
std::vector<byte> MakeCompressibleBuffer(size_t size) {
  std::vector<byte> buf(size);
  for (size_t i = 0; i < size; ++i)
    buf[i] = static_cast<byte>(i % 64);  // low entropy — compresses well
  return buf;
}

// Build a nominally incompressible buffer (pseudo-random bytes).
std::vector<byte> MakeIncompressibleBuffer(size_t size) {
  std::vector<byte> buf(size);
  uint32_t state = 0x12345678u;
  for (size_t i = 0; i < size; ++i) {
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
    buf[i] = static_cast<byte>(state & 0xFF);
  }
  return buf;
}

void RunZlibRoundTrip(const std::vector<byte>& original, size_t blockSize) {
  std::vector<byte> compressed;
  std::vector<size_t> compressedBlockSizes;
  ZLIBCompress(compressed, original, compressedBlockSizes, blockSize, blockSize * 2);

  std::vector<byte> decompressed;
  std::vector<bool> blockIsCompressed(compressedBlockSizes.size(), true);
  ZLIBDecompress(decompressed, compressed, compressedBlockSizes, blockIsCompressed);

  ASSERT_EQ(decompressed.size(), original.size());
  EXPECT_EQ(decompressed, original);
}

void RunLzmaRoundTrip(const std::vector<byte>& original, size_t blockSize) {
  std::vector<byte> compressed;
  std::vector<size_t> compressedBlockSizes;
  LZMACompress(compressed, original, compressedBlockSizes, blockSize, blockSize * 2);

  std::vector<byte> decompressed;
  std::vector<bool> blockIsCompressed(compressedBlockSizes.size(), true);
  LZMADecompress(decompressed, compressed, compressedBlockSizes, blockIsCompressed);

  ASSERT_EQ(decompressed.size(), original.size());
  EXPECT_EQ(decompressed, original);
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// ZLIB
// ---------------------------------------------------------------------------

TEST(ZlibCompression, SmallCompressibleRoundTrip) {
  auto buf = MakeCompressibleBuffer(1024);
  RunZlibRoundTrip(buf, 65536);
}

TEST(ZlibCompression, LargeCompressibleRoundTrip) {
  auto buf = MakeCompressibleBuffer(256 * 1024);
  RunZlibRoundTrip(buf, 65536);
}

TEST(ZlibCompression, SingleByteRoundTrip) {
  std::vector<byte> buf = {0xAB};
  RunZlibRoundTrip(buf, 65536);
}

TEST(ZlibCompression, IncompressibleDataRoundTrip) {
  auto buf = MakeIncompressibleBuffer(32 * 1024);
  RunZlibRoundTrip(buf, 65536);
}

TEST(ZlibCompression, MultiBlockRoundTrip) {
  // Force multiple blocks by using a block size smaller than the input.
  auto buf = MakeCompressibleBuffer(4 * 1024);
  RunZlibRoundTrip(buf, 1024);  // 4 blocks
}

// ---------------------------------------------------------------------------
// LZMA
// ---------------------------------------------------------------------------

TEST(LzmaCompression, SmallCompressibleRoundTrip) {
  auto buf = MakeCompressibleBuffer(1024);
  RunLzmaRoundTrip(buf, 65536);
}

TEST(LzmaCompression, LargeCompressibleRoundTrip) {
  auto buf = MakeCompressibleBuffer(256 * 1024);
  RunLzmaRoundTrip(buf, 65536);
}

TEST(LzmaCompression, SingleByteRoundTrip) {
  std::vector<byte> buf = {0x42};
  RunLzmaRoundTrip(buf, 65536);
}

TEST(LzmaCompression, IncompressibleDataRoundTrip) {
  auto buf = MakeIncompressibleBuffer(32 * 1024);
  RunLzmaRoundTrip(buf, 65536);
}

TEST(LzmaCompression, MultiBlockRoundTrip) {
  auto buf = MakeCompressibleBuffer(4 * 1024);
  RunLzmaRoundTrip(buf, 1024);
}
