#pragma once

#include <cstdint>

typedef uint8_t byte;

enum CompressionType {
  UNKNOWN = 0,
  ZLIB    = 1,
  LZMA    = 2
}

enum PathType {
  RELATIVE   = 0,
  IGNORECASE = 1,
  ABSOLUTE   = 2
}
