#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "archive.hpp"
#include "memory.hpp"
#include "types.hpp"

namespace PSArc {

class TocEntry {
public:
  TocEntry(byte* ptr, size_t offset, bool endianMismatch = false) {
    std::memcpy(md5Hash, ptr + offset + 0x00, 0x10);
    blockOffset      = readScalar<uint32_t>(ptr, offset + 0x10, endianMismatch);
    uncompressedSize = readScalar<uint40_t>(ptr, offset + 0x14, endianMismatch);
    fileOffset       = readScalar<uint40_t>(ptr, offset + 0x19, endianMismatch);
  }

  byte md5Hash[16];
  uint32_t blockOffset;
  uint64_t uncompressedSize;
  uint64_t fileOffset;
};

/*
 * Interface of a virtual archive to a psarc file.
 */
class PSArcHandle : public ArchiveInterface {
private:
  bool hasEndpoint;
  Archive* archiveEndpoint;
  OutputMemoryHandle* serializationEndpoint;

public:
  InputMemoryHandle* parsingEndpoint = nullptr;
  uint32_t* blocks                   = nullptr;
  uint32_t blockSize;
  PathType pathType               = PathType::RELATIVE;
  CompressionType compressionType = CompressionType::NONE;
  PSArcHandle();
  void SetParsingEndpoint(InputMemoryHandle*);
  void SetSerializationEndpoint(OutputMemoryHandle*);
  void SetArchive(Archive*);
  bool Upsync() override;
  bool Downsync() override;
};

class PSArcFile : public FileSourceProvider {
private:
  PSArcHandle& psarcHandle;
  TocEntry entry;
  CompressionType compressionType;

public:
  PSArcFile(PSArcHandle& _psarcHandle, TocEntry _entry, CompressionType _compressionType)
    : psarcHandle(_psarcHandle), entry(_entry), compressionType(_compressionType){};
  std::vector<byte> GetBytes() override;
  CompressionType GetCompressionType() override;
};

}  // namespace PSArc
