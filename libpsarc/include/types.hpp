#pragma once

#include <bit>
#include <cstdint>

typedef uint8_t byte;

enum CompressionType { NONE = 0, ZLIB = 1, LZMA = 2 };

enum PathType { RELATIVE = 0, IGNORECASE = 1, ABSOLUTE = 2 };

enum SeekType { START = 0, CURRENT = 1, END = 2 };

struct uint40_t {
  byte data[5];

  operator uint64_t() const {
    if constexpr (std::endian::native == std::endian::little) {
      uint64_t v = 0;
      v |= static_cast<uint64_t>(data[0]) << 0;
      v |= static_cast<uint64_t>(data[1]) << 8;
      v |= static_cast<uint64_t>(data[2]) << 16;
      v |= static_cast<uint64_t>(data[3]) << 24;
      v |= static_cast<uint64_t>(data[4]) << 32;

      return v;
    }
    else {
      uint64_t v = 0;
      v |= static_cast<uint64_t>(data[4]) << 0;
      v |= static_cast<uint64_t>(data[3]) << 8;
      v |= static_cast<uint64_t>(data[2]) << 16;
      v |= static_cast<uint64_t>(data[1]) << 24;
      v |= static_cast<uint64_t>(data[0]) << 32;

      return v;
    }
  }
};

struct uint24_t {
  byte data[3];

  operator uint32_t() const {
    if constexpr (std::endian::native == std::endian::little) {
      uint32_t v = 0;
      v |= static_cast<uint32_t>(data[0]) << 0;
      v |= static_cast<uint32_t>(data[1]) << 8;
      v |= static_cast<uint32_t>(data[2]) << 16;

      return v;
    }
    else {
      uint32_t v = 0;
      v |= static_cast<uint32_t>(data[2]) << 0;
      v |= static_cast<uint32_t>(data[1]) << 8;
      v |= static_cast<uint32_t>(data[0]) << 16;

      return v;
    }
  }
};

template <typename T>
static inline T swapEndian(T val) {
  static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

  union {
    T val;
    byte bytes[sizeof(T)];
  } src, dst;

  src.val = val;

  for (size_t i = 0; i < sizeof(T); i++) {
    dst.bytes[i] = src.bytes[sizeof(T) - i - 1];
  }

  return dst.val;
}

template <typename T>
static inline T readScalar(const byte* ptr, size_t offset, bool endianMismatch = false) {
  if (endianMismatch) {
    return swapEndian(reinterpret_cast<const T*>(ptr + offset)[0]);
  }
  else {
    return reinterpret_cast<const T*>(ptr + offset)[0];
  }
}
