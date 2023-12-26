#pragma once

#include <string.h>

#include "psarc_archive.hpp"

namespace PSArc {
class FileStructureHandle {
private:
  std::string rootPath;

public:
  FileStructureHandle(std::string);
};

}  // namespace PSArc
