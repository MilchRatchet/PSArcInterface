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

// ---------------------------------------------------------------------------
// Cross-codec: compressed output is different between ZLIB and LZMA
// ---------------------------------------------------------------------------

TEST(CompressionCodecs, ZlibAndLzmaProduceDifferentCompressedBytes) {
  auto original = MakeCompressibleBuffer(8 * 1024);

  std::vector<byte> zlibOut;
  std::vector<size_t> zlibSizes;
  ZLIBCompress(zlibOut, original, zlibSizes, 65536, 65536 * 2);

  std::vector<byte> lzmaOut;
  std::vector<size_t> lzmaSizes;
  LZMACompress(lzmaOut, original, lzmaSizes, 65536, 65536 * 2);

  EXPECT_NE(zlibOut, lzmaOut);
}

// ---------------------------------------------------------------------------
// Block-size boundary: exact multiple vs one byte over
// ---------------------------------------------------------------------------

TEST(ZlibCompression, ExactBlockSizeMultipleRoundTrip) {
  // Input is exactly 3 blocks.
  auto buf = MakeCompressibleBuffer(3 * 512);
  RunZlibRoundTrip(buf, 512);
}

TEST(LzmaCompression, ExactBlockSizeMultipleRoundTrip) {
  auto buf = MakeCompressibleBuffer(3 * 512);
  RunLzmaRoundTrip(buf, 512);
}

TEST(ZlibCompression, OneByteOverBlockBoundaryRoundTrip) {
  // Input is 2 full blocks + 1 byte — the last block is a partial block.
  auto buf = MakeCompressibleBuffer(2 * 512 + 1);
  RunZlibRoundTrip(buf, 512);
}

TEST(LzmaCompression, OneByteOverBlockBoundaryRoundTrip) {
  auto buf = MakeCompressibleBuffer(2 * 512 + 1);
  RunLzmaRoundTrip(buf, 512);
}

// ---------------------------------------------------------------------------
// All-zero buffer (maximally compressible)
// ---------------------------------------------------------------------------

TEST(ZlibCompression, AllZeroesRoundTrip) {
  std::vector<byte> buf(16 * 1024, static_cast<byte>(0x00));
  RunZlibRoundTrip(buf, 65536);
}

TEST(LzmaCompression, AllZeroesRoundTrip) {
  std::vector<byte> buf(16 * 1024, static_cast<byte>(0x00));
  RunLzmaRoundTrip(buf, 65536);
}

// ---------------------------------------------------------------------------
// All-same-byte buffer
// ---------------------------------------------------------------------------

TEST(ZlibCompression, AllSameByteRoundTrip) {
  std::vector<byte> buf(8 * 1024, static_cast<byte>(0xFF));
  RunZlibRoundTrip(buf, 65536);
}

TEST(LzmaCompression, AllSameByteRoundTrip) {
  std::vector<byte> buf(8 * 1024, static_cast<byte>(0xFF));
  RunLzmaRoundTrip(buf, 65536);
}

// ---------------------------------------------------------------------------
// Block size equals input size (single block)
// ---------------------------------------------------------------------------

TEST(ZlibCompression, BlockSizeEqualsInputSize) {
  auto buf = MakeCompressibleBuffer(2048);
  RunZlibRoundTrip(buf, 2048);  // exactly one block
}

TEST(LzmaCompression, BlockSizeEqualsInputSize) {
  auto buf = MakeCompressibleBuffer(2048);
  RunLzmaRoundTrip(buf, 2048);
}

// ---------------------------------------------------------------------------
// Block size larger than input (still single block)
// ---------------------------------------------------------------------------

TEST(ZlibCompression, BlockSizeLargerThanInput) {
  auto buf = MakeCompressibleBuffer(256);
  RunZlibRoundTrip(buf, 65536);  // block capacity >> input
}

TEST(LzmaCompression, BlockSizeLargerThanInput) {
  auto buf = MakeCompressibleBuffer(256);
  RunLzmaRoundTrip(buf, 65536);
}

// ---------------------------------------------------------------------------
// Compressed block sizes are all non-zero
// ---------------------------------------------------------------------------

TEST(ZlibCompression, CompressedBlockSizesNonZero) {
  auto original = MakeCompressibleBuffer(4 * 1024);
  std::vector<byte> compressed;
  std::vector<size_t> blockSizes;
  ZLIBCompress(compressed, original, blockSizes, 1024, 1024 * 2);
  for (size_t sz : blockSizes)
    EXPECT_GT(sz, 0u);
}

TEST(LzmaCompression, CompressedBlockSizesNonZero) {
  auto original = MakeCompressibleBuffer(4 * 1024);
  std::vector<byte> compressed;
  std::vector<size_t> blockSizes;
  LZMACompress(compressed, original, blockSizes, 1024, 1024 * 2);
  for (size_t sz : blockSizes)
    EXPECT_GT(sz, 0u);
}

// ---------------------------------------------------------------------------
// Block count matches ceil(input / blockSize)
// ---------------------------------------------------------------------------

TEST(ZlibCompression, BlockCountMatchesCeil) {
  const size_t blockSize = 1024;
  const size_t inputSize = 3 * blockSize + 1;  // 4 blocks expected
  auto original          = MakeCompressibleBuffer(inputSize);
  std::vector<byte> compressed;
  std::vector<size_t> blockSizes;
  ZLIBCompress(compressed, original, blockSizes, blockSize, blockSize * 2);
  EXPECT_EQ(blockSizes.size(), 4u);
}

TEST(LzmaCompression, BlockCountMatchesCeil) {
  const size_t blockSize = 1024;
  const size_t inputSize = 3 * blockSize + 1;
  auto original          = MakeCompressibleBuffer(inputSize);
  std::vector<byte> compressed;
  std::vector<size_t> blockSizes;
  LZMACompress(compressed, original, blockSizes, blockSize, blockSize * 2);
  EXPECT_EQ(blockSizes.size(), 4u);
}

// ---------------------------------------------------------------------------
// Partial-block decompression: blockIsCompressed flag set to false
// ---------------------------------------------------------------------------

TEST(ZlibCompression, UncompressedBlockPassthrough) {
  // Build a single raw block and pass it through with blockIsCompressed=false.
  std::vector<byte> original          = MakeCompressibleBuffer(512);
  std::vector<size_t> blockSizes      = {original.size()};
  std::vector<bool> blockIsCompressed = {false};

  std::vector<byte> decompressed;
  ZLIBDecompress(decompressed, original, blockSizes, blockIsCompressed);

  EXPECT_EQ(decompressed, original);
}
