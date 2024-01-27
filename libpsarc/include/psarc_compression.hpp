#pragma once

#include <vector>

#include "psarc_archive.hpp"
#include "psarc_types.hpp"

namespace PSArc {

void LZMACompress(
  std::vector<byte>& dst, const std::vector<byte>& src, std::vector<size_t>& compressedBlockSizes, size_t maxUncompressedBlockSize,
  size_t maxCompressedBlockSize);
size_t LZMADecompress(
  std::vector<byte>& dst, const std::vector<byte>& src, const std::vector<size_t>& compressedBlockSizes,
  const std::vector<bool>& blockIsCompressed);

}  // namespace PSArc
