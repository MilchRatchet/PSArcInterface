#pragma once

#include <vector>

#include "types.hpp"

namespace PSArc {

void Compress(
  std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type, std::vector<uint32_t>& compressedBlockSizes,
  uint32_t blockSize = 0);
void Compress(std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type);
void Decompress(std::vector<byte>& dst, const std::vector<byte>& src, CompressionType type);

}  // namespace PSArc
