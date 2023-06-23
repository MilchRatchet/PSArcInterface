#include "psarc.hpp"

#include <bit>
#include <cstring>

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

template <typename T>
static T swapEndian(T val) {
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
static inline T readInteger(byte* ptr, size_t offset, bool endianMismatch = false) {
  return reinterpret_cast<T*>(ptr + offset)[0];
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

void PSArc::PSArcHandle::SetParsingEndpoint(std::optional<InputMemoryHandle&> memHandle) {
  if (memHandle.has_value()) {
    this->parsingEndpoint = memHandle;
    this->hasEndpoint     = true;
  }
  else {
    this->parsingEndpoint.reset();
    this->hasEndpoint = (this->serializationEndpoint.has_value());
  }
}

void PSArc::PSArcHandle::SetSerializationEndpoint(std::optional<OutputMemoryHandle&> memHandle) {
  if (memHandle.has_value()) {
    this->serializationEndpoint = memHandle;
    this->hasEndpoint           = true;
  }
  else {
    this->serializationEndpoint.reset();
    this->hasEndpoint = (this->parsingEndpoint.has_value());
  }
}

void PSArc::PSArcHandle::SetArchive(std::optional<Archive&> archive) {
  this->archiveEndpoint = archive;
}

bool PSArc::PSArcHandle::Downsync() {
  if (!this->serializationEndpoint.has_value()) {
    return false;
  }
}

bool PSArc::PSArcHandle::Upsync() {
  if (!this->parsingEndpoint.has_value()) {
    return false;
  }

  if (!this->archiveEndpoint.has_value()) {
    return false;
  }

  InputMemoryHandle& file = this->parsingEndpoint.value();
  Archive& archive        = this->archiveEndpoint.value();

  std::vector<byte> header = std::vector<byte>(0x20);
  file.Read(header.data(), 0x20);

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
  file.Read(toc.data(), tocLength);

  std::vector<TocEntry> tocEntries = std::vector<TocEntry>(tocEntriesCount);

  for (uint32_t i = 0; i < tocEntriesCount; i++) {
    tocEntries.push_back(TocEntry(toc.data(), i * tocEntrySize, endianMismatch));
  }
}
