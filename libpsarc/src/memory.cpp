#include "memory.hpp"

static std::ios_base::seekdir SeekTypeToSeekDir(SeekType type) {
  switch (type) {
    case SeekType::START:
    default:
      return std::ios_base::seekdir::_S_beg;
    case SeekType::CURRENT:
      return std::ios_base::seekdir::_S_cur;
    case SeekType::END:
      return std::ios_base::seekdir::_S_end;
  }
}

PSArc::FileHandle::FileHandle(std::string path) : fileStream(path.data()) {
}

bool PSArc::FileHandle::Seek(SeekType type, size_t offset) {
  this->fileStream.seekg(offset, SeekTypeToSeekDir(type));
}

size_t PSArc::FileHandle::Tell() {
  return this->fileStream.tellg();
}
