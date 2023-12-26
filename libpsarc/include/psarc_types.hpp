#pragma once

#include <bit>
#include <climits>
#include <cstdint>

typedef uint8_t byte;

namespace PSArc {

enum CompressionType { PSARC_COMPRESSION_TYPE_NONE = 0, PSARC_COMPRESSION_TYPE_ZLIB = 1, PSARC_COMPRESSION_TYPE_LZMA = 2 };

enum PathType { PSARC_PATH_TYPE_RELATIVE = 0, PSARC_PATH_TYPE_IGNORECASE = 1, PSARC_PATH_TYPE_ABSOLUTE = 2 };

enum SeekType { PSARC_SEEK_TYPE_START = 0, PSARC_SEEK_TYPE_CURRENT = 1, PSARC_SEEK_TYPE_END = 2 };

struct uint40_t {
  byte data[5];

  static uint40_t FromUint64(uint64_t v) {
    uint40_t result;
    if constexpr (std::endian::native == std::endian::little) {
      result.data[0] = (v >> 0) & 0xFF;
      result.data[1] = (v >> 8) & 0xFF;
      result.data[2] = (v >> 16) & 0xFF;
      result.data[3] = (v >> 24) & 0xFF;
      result.data[4] = (v >> 32) & 0xFF;
    }
    else {
      result.data[4] = (v >> 0) & 0xFF;
      result.data[3] = (v >> 8) & 0xFF;
      result.data[2] = (v >> 16) & 0xFF;
      result.data[1] = (v >> 24) & 0xFF;
      result.data[0] = (v >> 32) & 0xFF;
    }

    return result;
  }

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

  // This must be a plain old data struct, hence no custom constructors allowed.
  static uint24_t From(uint32_t v) {
    uint24_t result;
    if constexpr (std::endian::native == std::endian::little) {
      result.data[0] = static_cast<byte>(v >> 0);
      result.data[1] = static_cast<byte>(v >> 8);
      result.data[2] = static_cast<byte>(v >> 16);
    }
    else {
      result.data[0] = static_cast<byte>(v >> 24);
      result.data[1] = static_cast<byte>(v >> 16);
      result.data[2] = static_cast<byte>(v >> 8);
    }
    return result;
  }

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

template <typename T>
static inline void writeScalar(byte* ptr, size_t offset, T val, bool endianMismatch = false) {
  *(reinterpret_cast<T*>(ptr + offset)) = (endianMismatch) ? swapEndian(val) : val;
}

}  // namespace PSArc
