#pragma once

#include <filesystem>
#include <fstream>

#include "types.hpp"

namespace PSArc {

class MemoryHandle {
public:
  virtual bool Seek(size_t, SeekType = SeekType::PSARC_SEEK_TYPE_START) = 0;
  virtual size_t Tell()                                                 = 0;
};

class InputMemoryHandle : public MemoryHandle {
public:
  virtual bool Read(byte* buf, size_t bytes_to_read) = 0;
};

class OutputMemoryHandle : public MemoryHandle {
public:
  virtual bool Write(const byte* buf, size_t bytes_to_write) = 0;
};

class InOutMemoryHandle : public InputMemoryHandle, public OutputMemoryHandle {};

/*
 * A handle for a physical file.
 */
class FileHandle : public InOutMemoryHandle {
private:
  std::fstream fileStream;
  bool validFileStream = false;

public:
  FileHandle(std::string);
  FileHandle(std::filesystem::path, bool overrideExistingFile = false);
  bool Read(byte* buf, size_t bytes_to_read) override;
  bool Write(const byte* buf, size_t bytes_to_write) override;
  bool Seek(size_t, SeekType = SeekType::PSARC_SEEK_TYPE_START) override;
  size_t Tell() override;
  bool IsValid() const {
    return this->validFileStream;
  };
};

}  // namespace PSArc
