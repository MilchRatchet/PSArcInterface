#include <gtest/gtest.h>

#include "psarc_types.hpp"

using namespace PSArc;

// ---------------------------------------------------------------------------
// uint40_t
// ---------------------------------------------------------------------------

TEST(uint40_t, ZeroRoundTrip) {
  uint64_t val = 0;
  EXPECT_EQ(static_cast<uint64_t>(uint40_t::FromUint64(val)), val);
}

TEST(uint40_t, MaxValueRoundTrip) {
  uint64_t val = 0xFF'FFFF'FFFFull;  // max 40-bit value
  EXPECT_EQ(static_cast<uint64_t>(uint40_t::FromUint64(val)), val);
}

TEST(uint40_t, ArbitraryValueRoundTrip) {
  uint64_t val = 0x12'3456'789Aull;
  EXPECT_EQ(static_cast<uint64_t>(uint40_t::FromUint64(val)), val);
}

// ---------------------------------------------------------------------------
// uint24_t
// ---------------------------------------------------------------------------

TEST(uint24_t, ZeroRoundTrip) {
  uint32_t val = 0;
  EXPECT_EQ(static_cast<uint32_t>(uint24_t::From(val)), val);
}

TEST(uint24_t, MaxValueRoundTrip) {
  uint32_t val = 0x00FF'FFFFu;
  EXPECT_EQ(static_cast<uint32_t>(uint24_t::From(val)), val);
}

TEST(uint24_t, ArbitraryValueRoundTrip) {
  uint32_t val = 0x00AB'CDEFu;
  EXPECT_EQ(static_cast<uint32_t>(uint24_t::From(val)), val);
}

// ---------------------------------------------------------------------------
// swapEndian
// ---------------------------------------------------------------------------

TEST(swapEndian, Uint16) {
  EXPECT_EQ(swapEndian<uint16_t>(0x0102u), 0x0201u);
}

TEST(swapEndian, Uint32) {
  EXPECT_EQ(swapEndian<uint32_t>(0x01020304u), 0x04030201u);
}

TEST(swapEndian, Uint64) {
  EXPECT_EQ(swapEndian<uint64_t>(0x0102030405060708ull), 0x0807060504030201ull);
}

TEST(swapEndian, DoubleSwapIsIdentity) {
  uint32_t val = 0xDEADBEEFu;
  EXPECT_EQ(swapEndian(swapEndian(val)), val);
}

TEST(swapEndian, Uint8Identity) {
  // Swapping a single byte is a no-op.
  EXPECT_EQ(swapEndian<uint8_t>(0xABu), 0xABu);
}

// ---------------------------------------------------------------------------
// uint40_t — boundary values
// ---------------------------------------------------------------------------

TEST(uint40_t, OneRoundTrip) {
  uint64_t val = 1;
  EXPECT_EQ(static_cast<uint64_t>(uint40_t::FromUint64(val)), val);
}

TEST(uint40_t, HighByteBoundary) {
  // Value using only the high byte of the 40-bit range.
  uint64_t val = 0xFF00'0000'00ull;
  EXPECT_EQ(static_cast<uint64_t>(uint40_t::FromUint64(val)), val);
}

TEST(uint40_t, LowByteBoundary) {
  uint64_t val = 0x00'0000'00FFull;
  EXPECT_EQ(static_cast<uint64_t>(uint40_t::FromUint64(val)), val);
}

// ---------------------------------------------------------------------------
// uint24_t — boundary values
// ---------------------------------------------------------------------------

TEST(uint24_t, OneRoundTrip) {
  uint32_t val = 1;
  EXPECT_EQ(static_cast<uint32_t>(uint24_t::From(val)), val);
}

TEST(uint24_t, LowByteBoundary) {
  uint32_t val = 0x0000'00FFu;
  EXPECT_EQ(static_cast<uint32_t>(uint24_t::From(val)), val);
}

TEST(uint24_t, HighByteBoundary) {
  uint32_t val = 0x00FF'0000u;
  EXPECT_EQ(static_cast<uint32_t>(uint24_t::From(val)), val);
}

// ---------------------------------------------------------------------------
// readScalar / writeScalar
// ---------------------------------------------------------------------------

TEST(ReadWriteScalar, Uint32RoundTrip) {
  byte buf[4] = {};
  writeScalar<uint32_t>(buf, 0, 0xDEADBEEFu);
  EXPECT_EQ(readScalar<uint32_t>(buf, 0), 0xDEADBEEFu);
}

TEST(ReadWriteScalar, Uint32WithOffset) {
  byte buf[8] = {};
  writeScalar<uint32_t>(buf, 4, 0x01020304u);
  EXPECT_EQ(readScalar<uint32_t>(buf, 4), 0x01020304u);
  // First 4 bytes should be untouched.
  EXPECT_EQ(readScalar<uint32_t>(buf, 0), 0x00000000u);
}

TEST(ReadWriteScalar, Uint16RoundTrip) {
  byte buf[2] = {};
  writeScalar<uint16_t>(buf, 0, 0xABCDu);
  EXPECT_EQ(readScalar<uint16_t>(buf, 0), 0xABCDu);
}

TEST(ReadWriteScalar, EndianMismatchSwaps) {
  byte buf[4] = {};
  writeScalar<uint32_t>(buf, 0, 0x01020304u, /*endianMismatch=*/true);
  // Reading back without mismatch should yield the byte-swapped value.
  uint32_t raw = readScalar<uint32_t>(buf, 0, /*endianMismatch=*/false);
  EXPECT_EQ(raw, swapEndian<uint32_t>(0x01020304u));
}

TEST(ReadWriteScalar, EndianMismatchRoundTrip) {
  byte buf[4]       = {};
  uint32_t original = 0xCAFEBABEu;
  writeScalar<uint32_t>(buf, 0, original, /*endianMismatch=*/true);
  EXPECT_EQ(readScalar<uint32_t>(buf, 0, /*endianMismatch=*/true), original);
}

TEST(ReadWriteScalar, Uint64RoundTrip) {
  byte buf[8] = {};
  writeScalar<uint64_t>(buf, 0, 0xDEADBEEFCAFEBABEull);
  EXPECT_EQ(readScalar<uint64_t>(buf, 0), 0xDEADBEEFCAFEBABEull);
}

TEST(ReadWriteScalar, Uint8RoundTrip) {
  byte buf[1] = {};
  writeScalar<uint8_t>(buf, 0, 0xA5u);
  EXPECT_EQ(readScalar<uint8_t>(buf, 0), 0xA5u);
}

TEST(ReadWriteScalar, OverwritingPreviousValue) {
  byte buf[4] = {};
  writeScalar<uint32_t>(buf, 0, 0x11223344u);
  writeScalar<uint32_t>(buf, 0, 0xAABBCCDDu);
  EXPECT_EQ(readScalar<uint32_t>(buf, 0), 0xAABBCCDDu);
}

// ---------------------------------------------------------------------------
// swapEndian — known bit patterns
// ---------------------------------------------------------------------------

TEST(swapEndian, Uint16KnownPattern) {
  EXPECT_EQ(swapEndian<uint16_t>(0xFF00u), 0x00FFu);
}

TEST(swapEndian, Uint32AllSameByte) {
  // Swapping a value made of the same repeated byte should be unchanged.
  EXPECT_EQ(swapEndian<uint32_t>(0xAAAAAAAAu), 0xAAAAAAAAu);
}

TEST(swapEndian, Uint64HighLowSwap) {
  // Only high and low bytes differ; after swap they should exchange.
  uint64_t val    = 0xAB00000000000000ull;
  uint64_t expect = 0x00000000000000ABull;
  EXPECT_EQ(swapEndian(val), expect);
}

// ---------------------------------------------------------------------------
// uint40_t — arithmetic consistency
// ---------------------------------------------------------------------------

TEST(uint40_t, IncrementalValues) {
  for (uint64_t v = 0; v <= 0xFF; ++v)
    EXPECT_EQ(static_cast<uint64_t>(uint40_t::FromUint64(v)), v);
}

TEST(uint40_t, MidRangeValue) {
  uint64_t val = 0x80'0000'0000ull;
  EXPECT_EQ(static_cast<uint64_t>(uint40_t::FromUint64(val)), val);
}

// ---------------------------------------------------------------------------
// uint24_t — arithmetic consistency
// ---------------------------------------------------------------------------

TEST(uint24_t, IncrementalValues) {
  for (uint32_t v = 0; v <= 0xFF; ++v)
    EXPECT_EQ(static_cast<uint32_t>(uint24_t::From(v)), v);
}

TEST(uint24_t, MidRangeValue) {
  uint32_t val = 0x007F'FFFFu;
  EXPECT_EQ(static_cast<uint32_t>(uint24_t::From(val)), val);
}
