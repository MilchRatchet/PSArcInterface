#pragma once

#include <vector>

#include "types.hpp"

namespace PSArc {

void Compress(std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type);
void Decompress(std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type);

}  // namespace PSArc
