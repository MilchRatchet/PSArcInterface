#pragma once

#include <cstring>
#include <vector>

#include "psarc_memory.hpp"
#include "psarc_types.hpp"

// In-memory I/O handles useful for round-trip testing without touching disk.

class VectorOutputHandle : public PSArc::OutputMemoryHandle {
public:
  std::vector<byte> data;
  size_t cursor = 0;

  bool Write(const byte* buf, size_t bytes_to_write) override {
    if (cursor + bytes_to_write > data.size())
      data.resize(cursor + bytes_to_write);
    std::memcpy(data.data() + cursor, buf, bytes_to_write);
    cursor += bytes_to_write;
    return true;
  }

  bool Seek(size_t offset, PSArc::SeekType type = PSArc::SeekType::PSARC_SEEK_TYPE_START) override {
    switch (type) {
      case PSArc::SeekType::PSARC_SEEK_TYPE_START:
        cursor = offset;
        break;
      case PSArc::SeekType::PSARC_SEEK_TYPE_CURRENT:
        cursor += offset;
        break;
      case PSArc::SeekType::PSARC_SEEK_TYPE_END:
        cursor = data.size() + offset;
        break;
    }
    return true;
  }

  size_t Tell() override {
    return cursor;
  }
};

class VectorInputHandle : public PSArc::InputMemoryHandle {
public:
  const std::vector<byte>& data;
  size_t cursor = 0;

  explicit VectorInputHandle(const std::vector<byte>& src) : data(src) {
  }

  bool Read(byte* buf, size_t bytes_to_read) override {
    if (cursor + bytes_to_read > data.size())
      return false;
    std::memcpy(buf, data.data() + cursor, bytes_to_read);
    cursor += bytes_to_read;
    return true;
  }

  bool Seek(size_t offset, PSArc::SeekType type = PSArc::SeekType::PSARC_SEEK_TYPE_START) override {
    switch (type) {
      case PSArc::SeekType::PSARC_SEEK_TYPE_START:
        cursor = offset;
        break;
      case PSArc::SeekType::PSARC_SEEK_TYPE_CURRENT:
        cursor += offset;
        break;
      case PSArc::SeekType::PSARC_SEEK_TYPE_END:
        cursor = data.size() + offset;
        break;
    }
    return true;
  }

  size_t Tell() override {
    return cursor;
  }
};
