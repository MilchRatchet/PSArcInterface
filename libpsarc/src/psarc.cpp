#include "psarc.hpp"

#include <bit>
#include <cstring>
#include <iostream>

struct uint40_t {
  byte data[5];

  operator uint64_t() const {
    if constexpr (std::endian::native == std::endian::little) {
      uint64_t v = 0;
      v |= static_cast<uint64_t>(data[0]) << 0;
      v |= static_cast<uint64_t>(data[1]) << 8;
      v |= static_cast<uint64_t>(data[2]) << 16;
      v |= static_cast<uint64_t>(data[3]) << 24;
      v |= static_cast<uint64_t>(data[4]) << 32;

      return v;
    }
    else {
      uint64_t v = 0;
      v |= static_cast<uint64_t>(data[4]) << 0;
      v |= static_cast<uint64_t>(data[3]) << 8;
      v |= static_cast<uint64_t>(data[2]) << 16;
      v |= static_cast<uint64_t>(data[1]) << 24;
      v |= static_cast<uint64_t>(data[0]) << 32;

      return v;
    }
  }
};

struct uint24_t {
  byte data[3];

  operator uint32_t() const {
    if constexpr (std::endian::native == std::endian::little) {
      uint32_t v = 0;
      v |= static_cast<uint32_t>(data[0]) << 0;
      v |= static_cast<uint32_t>(data[1]) << 8;
      v |= static_cast<uint32_t>(data[2]) << 16;

      return v;
    }
    else {
      uint32_t v = 0;
      v |= static_cast<uint32_t>(data[2]) << 0;
      v |= static_cast<uint32_t>(data[1]) << 8;
      v |= static_cast<uint32_t>(data[0]) << 16;

      return v;
    }
  }
};

template <typename T>
static inline T swapEndian(T val) {
  static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

  union {
    T val;
    byte bytes[sizeof(T)];
  } src, dst;

  src.val = val;

  for (size_t i = 0; i < sizeof(T); i++) {
    dst.bytes[i] = src.bytes[sizeof(T) - i - 1];
  }

  return dst.val;
}

template <typename T>
static inline T readInteger(const byte* ptr, size_t offset, bool endianMismatch = false) {
  if (endianMismatch) {
    return swapEndian(reinterpret_cast<const T*>(ptr + offset)[0]);
  }
  else {
    return reinterpret_cast<const T*>(ptr + offset)[0];
  }
}

static bool isPSArcFile(std::vector<byte>& header) {
  return (header[0] == 'P') && (header[1] == 'S') && (header[2] == 'A') && (header[3] == 'R');
}

static bool isEndianMismatch(std::vector<byte>& header) {
  uint16_t majorVersion = readInteger<uint16_t>(header.data(), 0x04);

  // majorVersion larger than 255 means that the byte order is most likely the wrong way around as such a high major version shouldn't
  // exist.
  return (majorVersion > 255);
}

static CompressionType getCompressionType(byte* ptr) {
  switch (*ptr) {
    case 'z':
      return CompressionType::ZLIB;
    case 'l':
      return CompressionType::LZMA;
    default:
      return CompressionType::NONE;
  }
}

class TocEntry {
public:
  TocEntry(byte* ptr, size_t offset, bool endianMismatch = false) {
    std::memcpy(md5Hash, ptr + offset + 0x00, 0x10);
    blockOffset      = readInteger<uint32_t>(ptr, offset + 0x10, endianMismatch);
    uncompressedSize = readInteger<uint40_t>(ptr, offset + 0x14, endianMismatch);
    fileOffset       = readInteger<uint40_t>(ptr, offset + 0x19, endianMismatch);
  }

  byte md5Hash[16];
  uint32_t blockOffset;
  uint64_t uncompressedSize;
  uint64_t fileOffset;
};

PSArc::PSArcHandle::PSArcHandle() {
}

void PSArc::PSArcHandle::SetParsingEndpoint(InputMemoryHandle* memHandle) {
  if (memHandle != nullptr) {
    this->parsingEndpoint = memHandle;
    this->hasEndpoint     = true;
  }
  else {
    this->parsingEndpoint = nullptr;
    this->hasEndpoint     = (this->serializationEndpoint != nullptr);
  }
}

void PSArc::PSArcHandle::SetSerializationEndpoint(OutputMemoryHandle* memHandle) {
  if (memHandle != nullptr) {
    this->serializationEndpoint = memHandle;
    this->hasEndpoint           = true;
  }
  else {
    this->serializationEndpoint = nullptr;
    this->hasEndpoint           = (this->parsingEndpoint != nullptr);
  }
}

void PSArc::PSArcHandle::SetArchive(Archive* archive) {
  this->archiveEndpoint = archive;
}

bool PSArc::PSArcHandle::Downsync() {
  if (this->serializationEndpoint == nullptr) {
    return false;
  }

  return true;
}

bool PSArc::PSArcHandle::Upsync() {
  if (this->parsingEndpoint == nullptr) {
    return false;
  }

  if (this->archiveEndpoint == nullptr) {
    return false;
  }

  std::vector<byte> header = std::vector<byte>(0x20);
  this->parsingEndpoint->Read(header.data(), 0x20);

  if (!isPSArcFile(header)) {
    return false;
  }

  bool endianMismatch = isEndianMismatch(header);

  CompressionType compressionType = getCompressionType(header.data() + 0x08);
  uint32_t tocLength              = readInteger<uint32_t>(header.data(), 0x0C, endianMismatch);
  uint32_t tocEntrySize           = readInteger<uint32_t>(header.data(), 0x10, endianMismatch);
  uint32_t tocEntriesCount        = readInteger<uint32_t>(header.data(), 0x14, endianMismatch);
  uint32_t blockSize              = readInteger<uint32_t>(header.data(), 0x18, endianMismatch);
  PathType pathType               = static_cast<PathType>(readInteger<uint32_t>(header.data(), 0x1C, endianMismatch));

  std::vector<byte> toc = std::vector<byte>(tocLength);
  this->parsingEndpoint->Read(toc.data(), tocLength);

  std::vector<TocEntry> tocEntries = std::vector<TocEntry>();

  for (uint32_t i = 0; i < tocEntriesCount; i++) {
    tocEntries.push_back(TocEntry(toc.data(), i * tocEntrySize, endianMismatch));
  }

  uint32_t blockPtrSize = 2;
  if (blockSize > 0xFFFF)
    blockPtrSize++;
  if (blockSize > 0xFFFFFF)
    blockPtrSize++;

  uint32_t numBlocks = (tocLength - tocEntrySize * tocEntriesCount) / blockPtrSize;
  uint32_t* blocks   = new uint32_t[numBlocks];

  switch (blockPtrSize) {
    case 2:
      for (uint32_t i = 0; i < numBlocks; i++) {
        blocks[i] = readInteger<uint16_t>(header.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
    case 3:
      for (uint32_t i = 0; i < numBlocks; i++) {
        blocks[i] = readInteger<uint24_t>(header.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
    case 4:
      for (uint32_t i = 0; i < numBlocks; i++) {
        blocks[i] = readInteger<uint32_t>(header.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
  }

  return true;
}
