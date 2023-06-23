#pragma once

#include <fstream>

#include "types.hpp"

namespace PSArc {

class MemoryHandle {
public:
  virtual bool Seek(SeekType, size_t) = 0;
  virtual size_t Tell()               = 0;
};

class InputMemoryHandle : public MemoryHandle {
public:
  virtual bool Read(byte* buf, size_t bytes_to_read) = 0;
};

class OutputMemoryHandle : public MemoryHandle {
  virtual bool Write(byte* buf, size_t bytes_to_write) = 0;
};

class InOutMemoryHandle : public InputMemoryHandle, public OutputMemoryHandle {};

class FileHandle : public InOutMemoryHandle {
private:
  std::fstream fileStream;

public:
  FileHandle(std::string);
  bool Read(byte* buf, size_t bytes_to_read) override;
  bool Write(byte* buf, size_t bytes_to_write) override;
  bool Seek(SeekType, size_t) override;
  size_t Tell() override;
};

}  // namespace PSArc
