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

PSArc::FileHandle::FileHandle(std::string path)
  : fileStream(path.data(), std::ios_base::openmode::_S_in | std::ios_base::openmode::_S_out | std::ios_base::openmode::_S_bin) {
}

bool PSArc::FileHandle::Seek(SeekType type, size_t offset) {
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

bool PSArc::FileHandle::Write(byte* buf, size_t bytes_to_write) {
  this->fileStream.write(reinterpret_cast<char*>(buf), bytes_to_write);

  return true;
}
