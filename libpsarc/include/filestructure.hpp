#pragma once

#include <string.h>

#include "archive.hpp"

namespace PSArc {
class FileStructureHandle {
private:
  std::string rootPath;

public:
  FileStructureHandle(std::string);
};
}  // namespace PSArc
