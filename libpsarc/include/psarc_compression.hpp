#pragma once

#include <vector>

#include "psarc_archive.hpp"
#include "psarc_types.hpp"

namespace PSArc {

void Compress(FileData& dst, const FileData& src);
void Decompress(FileData& dst, const FileData& src);

}  // namespace PSArc
