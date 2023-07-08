#pragma once

#include <vector>

#include "archive.hpp"
#include "types.hpp"

namespace PSArc {

void Compress(FileData& dst, const FileData& src);
void Decompress(FileData& dst, const FileData& src);

}  // namespace PSArc
