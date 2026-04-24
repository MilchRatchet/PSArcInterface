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
