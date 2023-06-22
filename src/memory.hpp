#pragma once

#include "types.hpp"

class MemoryHandle {
public:
  virtual bool writable();
  virtual bool readable();
}

class InputMemoryHandle : public MemoryHandle {
public:
  bool read(byte* buf, size_t bytes_to_read);
  uint32_t read_u32();
  uint64_t read_u40();
}

class OutputMemoryHandle : public MemoryHandle {
  bool write(byte* buf, size_t bytes_to_write);
  void write_u32(uint32_t);
  void write_u40(uint32_t);
}

class InOutMemoryHandle : public InputMemoryHandle,
                          public OutputMemoryHandle {
}
