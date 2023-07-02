#include "psarc.hpp"

#include <iostream>

static bool isPSArcFile(std::vector<byte>& header) {
  return (header[0] == 'P') && (header[1] == 'S') && (header[2] == 'A') && (header[3] == 'R');
}

static bool isEndianMismatch(std::vector<byte>& header) {
  uint16_t majorVersion = readScalar<uint16_t>(header.data(), 0x04);

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

void PSArc::PSArcHandle::AddFilesToArchive(std::vector<TocEntry>& entries) {
  if (this->parsingEndpoint == nullptr) {
    return;
  }

  if (this->archiveEndpoint == nullptr) {
    return;
  }

  TocEntry manifest = entries[0];

  if (manifest.uncompressedSize != 0) {
    this->parsingEndpoint->Seek(manifest.fileOffset);

    uint32_t blockOffset = manifest.blockOffset;

    PSArc::PSArcFile* manifestFileSource = new PSArcFile(*this, manifest);
    PSArc::File manifestFile(std::string("/PSArcManifest.bin"), manifestFileSource);

    this->archiveEndpoint->AddFile(manifestFile);

    for (uint32_t i = 1; i < entries.size(); i++) {
      PSArc::PSArcFile* fileSource = new PSArcFile(*this, entries[i]);
      PSArc::File file(std::string("/Unknown.bin"), fileSource);

      this->archiveEndpoint->AddFile(file);
    }
  }
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
  uint32_t tocLength              = readScalar<uint32_t>(header.data(), 0x0C, endianMismatch);
  uint32_t tocEntrySize           = readScalar<uint32_t>(header.data(), 0x10, endianMismatch);
  uint32_t tocEntriesCount        = readScalar<uint32_t>(header.data(), 0x14, endianMismatch);
  uint32_t blockSize              = readScalar<uint32_t>(header.data(), 0x18, endianMismatch);
  PathType pathType               = static_cast<PathType>(readScalar<uint32_t>(header.data(), 0x1C, endianMismatch));

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
        blocks[i] = readScalar<uint16_t>(header.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
    case 3:
      for (uint32_t i = 0; i < numBlocks; i++) {
        blocks[i] = readScalar<uint24_t>(header.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
    case 4:
      for (uint32_t i = 0; i < numBlocks; i++) {
        blocks[i] = readScalar<uint32_t>(header.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
  }

  AddFilesToArchive(tocEntries);

  return true;
}

std::vector<byte> PSArc::PSArcFile::GetBytes() {
  return std::vector<byte>();
}

CompressionType PSArc::PSArcFile::GetCompressionType() {
  return CompressionType::NONE;
}
