#pragma once

#include <cstdint>

typedef uint8_t byte;

enum CompressionType { NONE = 0, ZLIB = 1, LZMA = 2 };

enum PathType { RELATIVE = 0, IGNORECASE = 1, ABSOLUTE = 2 };

enum SeekType { START = 0, CURRENT = 1, END = 2 };
