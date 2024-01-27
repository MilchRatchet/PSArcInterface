#include "psarc_memory.hpp"

static std::ios::seekdir SeekTypeToSeekDir(PSArc::SeekType type) {
  switch (type) {
    case PSArc::SeekType::PSARC_SEEK_TYPE_START:
    default:
      return std::ios::beg;
    case PSArc::SeekType::PSARC_SEEK_TYPE_CURRENT:
      return std::ios::cur;
    case PSArc::SeekType::PSARC_SEEK_TYPE_END:
      return std::ios::end;
  }
}

PSArc::FileHandle::FileHandle(std::string path) : fileStream(path.data(), std::ios::in | std::ios::out | std::ios::binary) {
  if (!this->fileStream.fail()) {
    this->validFileStream = true;
  }
}

PSArc::FileHandle::FileHandle(std::filesystem::path path, bool overrideExistingFile)
  : fileStream(path, (overrideExistingFile ? std::ios::trunc | std::ios::in : std::ios::in) | std::ios::out | std::ios::binary) {
  // On failure, create the directories and try again.
  if (this->fileStream.fail()) {
    std::filesystem::path dirPath(path);
    dirPath.remove_filename();

    std::filesystem::create_directories(dirPath);

    this->fileStream.clear();
    this->fileStream.open(path, (overrideExistingFile ? std::ios::trunc | std::ios::in : std::ios::in) | std::ios::out | std::ios::binary);
  }

  if (!this->fileStream.fail()) {
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
  if (!this->validFileStream)
    return false;

  this->fileStream.read(reinterpret_cast<char*>(buf), bytes_to_read);

  return true;
}

bool PSArc::FileHandle::Write(const byte* buf, size_t bytes_to_write) {
  if (!this->validFileStream)
    return false;

  this->fileStream.write(reinterpret_cast<const char*>(buf), bytes_to_write);

  return true;
}
