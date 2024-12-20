#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "psarc_archive.hpp"
#include "psarc_error.hpp"
#include "psarc_memory.hpp"
#include "psarc_types.hpp"

namespace PSArc {

class TocEntry {
public:
  TocEntry(byte* ptr, size_t offset, bool endianMismatch = false) {
    std::memcpy(md5Hash, ptr + offset + 0x00, 0x10);
    blockOffset      = readScalar<uint32_t>(ptr, offset + 0x10, endianMismatch);
    uncompressedSize = readScalar<uint40_t>(ptr, offset + 0x14, endianMismatch);
    fileOffset       = readScalar<uint40_t>(ptr, offset + 0x19, endianMismatch);
  }

  TocEntry(uint32_t _blockOffset, uint64_t _uncompressedSize, uint64_t _fileOffset)
    : blockOffset(_blockOffset), uncompressedSize(_uncompressedSize), fileOffset(_fileOffset) {
    std::memset(md5Hash, 0, 0x10);
  }

  void ToByteArray(byte* ptr, bool endianMismatch = false) {
    std::memcpy(ptr + 0x00, md5Hash, 0x10);
    writeScalar<uint32_t>(ptr, 0x10, blockOffset, endianMismatch);
    writeScalar<uint40_t>(ptr, 0x14, uint40_t::FromUint64(uncompressedSize), endianMismatch);
    writeScalar<uint40_t>(ptr, 0x19, uint40_t::FromUint64(fileOffset), endianMismatch);
  }

  byte md5Hash[16];
  uint32_t blockOffset;
  uint64_t uncompressedSize;
  uint64_t fileOffset;
};

class PSArcSettings : public ArchiveSyncSettings {
public:
  uint16_t versionMajor           = 1;
  uint16_t versionMinor           = 4;
  CompressionType compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;
  uint32_t blockSize              = 65536;
  uint32_t tocEntrySize           = 30;
  PathType pathType               = PathType::PSARC_PATH_TYPE_ABSOLUTE;
  std::endian endianness          = std::endian::native;
};

/*
 * Interface of a virtual psarc archive to a psarc file.
 */
class PSArcHandle : public ArchiveInterface {
private:
  Archive* archiveEndpoint;

public:
  InputMemoryHandle* parsingEndpoint        = nullptr;
  OutputMemoryHandle* serializationEndpoint = nullptr;
  size_t* blocks                            = nullptr;
  size_t blockSize;
  PathType pathType               = PathType::PSARC_PATH_TYPE_RELATIVE;
  CompressionType compressionType = CompressionType::PSARC_COMPRESSION_TYPE_NONE;

  PSArcHandle();
  ~PSArcHandle() {
    if (this->blocks != nullptr)
      delete[] this->blocks;
  }

  void SetParsingEndpoint(InputMemoryHandle* memHandle);
  void SetSerializationEndpoint(OutputMemoryHandle* memHandle);
  void SetArchive(Archive* archive);
  PSArcStatus Upsync() override;
  PSArcStatus Downsync() override;
  PSArcStatus Downsync(std::function<void(size_t, std::string)> callbackFunc = {});
  PSArcStatus Downsync(PSArcSettings settings, std::function<void(size_t, std::string)> callbackFunc = {});
};

/*
 * A reference to a file in a virtual psarc archive.
 */
class PSArcFile : public FileSourceProvider {
private:
  PSArcHandle& psarcHandle;
  TocEntry entry;
  CompressionType compressionType;

public:
  PSArcFile(PSArcHandle& _psarcHandle, TocEntry _entry, CompressionType _compressionType)
    : psarcHandle(_psarcHandle), entry(_entry), compressionType(_compressionType) {};
  FileData GetData() override;
  CompressionType GetCompressionType() override;
  bool HasUncompressedSize() override;
  size_t GetUncompressedSize() override;
};

}  // namespace PSArc
