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

  this->compressionType    = getCompressionType(header.data() + 0x08);
  uint32_t tocLength       = readScalar<uint32_t>(header.data(), 0x0C, endianMismatch);
  uint32_t tocEntrySize    = readScalar<uint32_t>(header.data(), 0x10, endianMismatch);
  uint32_t tocEntriesCount = readScalar<uint32_t>(header.data(), 0x14, endianMismatch);
  this->blockSize          = readScalar<uint32_t>(header.data(), 0x18, endianMismatch);
  this->pathType           = static_cast<PathType>(readScalar<uint32_t>(header.data(), 0x1C, endianMismatch));

  std::vector<byte> toc = std::vector<byte>(tocLength);
  this->parsingEndpoint->Read(toc.data(), tocLength);

  std::vector<TocEntry> tocEntries = std::vector<TocEntry>();

  for (uint32_t i = 0; i < tocEntriesCount; i++) {
    tocEntries.push_back(TocEntry(toc.data(), i * tocEntrySize, endianMismatch));
  }

  uint32_t blockPtrSize = 2;
  if (blockSize > 0x10000)
    blockPtrSize++;
  if (blockSize > 0x1000000)
    blockPtrSize++;

  uint32_t numBlocks = (tocLength - tocEntrySize * tocEntriesCount) / blockPtrSize;

  this->blocks = new uint32_t[numBlocks];

  switch (blockPtrSize) {
    case 2:
      for (uint32_t i = 0; i < numBlocks; i++) {
        this->blocks[i] = readScalar<uint16_t>(toc.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
    case 3:
      for (uint32_t i = 0; i < numBlocks; i++) {
        this->blocks[i] = readScalar<uint24_t>(toc.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
    case 4:
      for (uint32_t i = 0; i < numBlocks; i++) {
        this->blocks[i] = readScalar<uint32_t>(toc.data(), tocEntrySize * tocEntriesCount + i * blockPtrSize, endianMismatch);
      }
      break;
  }

  TocEntry manifest = tocEntries[0];

  if (manifest.uncompressedSize != 0) {
    this->parsingEndpoint->Seek(manifest.fileOffset);

    PSArc::PSArcFile* manifestFileSource = new PSArcFile(*this, manifest, this->compressionType);
    this->archiveEndpoint->AddFile(PSArc::File(std::string("/PSArcManifest.bin"), manifestFileSource));

    PSArc::File* manifestFile = this->archiveEndpoint->FindFile("/PSArcManifest.bin");

    if (manifestFile != nullptr) {
      std::vector<byte>& manifest = manifestFile->GetUncompressedBytes();

      std::stringstream fileNames((reinterpret_cast<char*>(manifest.data())));
      std::string fileName;

      for (uint32_t i = 1; i < tocEntries.size(); i++) {
        std::getline(fileNames, fileName, '\n');
        PSArc::PSArcFile* fileSource = new PSArcFile(*this, tocEntries[i], this->compressionType);
        PSArc::File file(fileName, fileSource);

        this->archiveEndpoint->AddFile(file);
      }
    }
  }

  return true;
}

std::vector<byte> PSArc::PSArcFile::GetBytes() {
  if (this->psarcHandle.parsingEndpoint == nullptr) {
    return std::vector<byte>();
  }

  uint64_t uncompressedSize = this->entry.uncompressedSize;
  uint64_t uncompressedRead = 0;
  uint32_t blockOffset      = this->entry.blockOffset;
  uint64_t outputOffset     = 0;
  uint32_t blockSize        = this->psarcHandle.blockSize;
  uint32_t outputSize       = 0;

  std::vector<byte> output = std::vector<byte>();

  this->psarcHandle.parsingEndpoint->Seek(this->entry.fileOffset);

  do {
    uint32_t entrySize = this->psarcHandle.blocks[blockOffset];
    outputSize += entrySize;
    output.resize(outputSize);

    if (entrySize == 0) {
      this->psarcHandle.parsingEndpoint->Read(output.data() + outputOffset, blockSize);
      outputOffset += blockSize;
    }
    else {
      this->psarcHandle.parsingEndpoint->Read(output.data() + outputOffset, entrySize);
      outputOffset += entrySize;
    }
    uncompressedRead += blockSize;
    blockOffset++;
  } while (uncompressedRead < uncompressedSize);

  return output;
}

CompressionType PSArc::PSArcFile::GetCompressionType() {
  return this->compressionType;
}
