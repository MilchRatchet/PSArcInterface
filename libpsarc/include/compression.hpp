#pragma once

#include "types.hpp"

namespace PSArc {

void Compress(byte* dst, byte* src, CompressionType type);
void Decompress(byte* dst, byte* src, CompressionType type);

}  // namespace PSArc
