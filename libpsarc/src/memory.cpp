#include "memory.hpp"

static std::ios_base::seekdir SeekTypeToSeekDir(PSArc::SeekType type) {
  switch (type) {
    case PSArc::SeekType::START:
    default:
      return std::ios_base::seekdir::_S_beg;
    case PSArc::SeekType::CURRENT:
      return std::ios_base::seekdir::_S_cur;
    case PSArc::SeekType::END:
      return std::ios_base::seekdir::_S_end;
  }
}

PSArc::FileHandle::FileHandle(std::string path)
  : fileStream(path.data(), std::ios_base::openmode::_S_in | std::ios_base::openmode::_S_out | std::ios_base::openmode::_S_bin) {
  if (!fileStream.fail()) {
    this->validFileStream = true;
  }
}

bool PSArc::FileHandle::Seek(size_t offset, SeekType type) {
  this->fileStream.seekg(offset, SeekTypeToSeekDir(type));

  return true;
}

size_t PSArc::FileHandle::Tell() {
  return this->fileStream.tellg();
}

bool PSArc::FileHandle::Read(byte* buf, size_t bytes_to_read) {
  this->fileStream.read(reinterpret_cast<char*>(buf), bytes_to_read);

  return true;
}

bool PSArc::FileHandle::Write(const byte* buf, size_t bytes_to_write) {
  this->fileStream.write(reinterpret_cast<const char*>(buf), bytes_to_write);

  return true;
}
