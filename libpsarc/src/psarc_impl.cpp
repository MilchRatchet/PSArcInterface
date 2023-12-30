#include "psarc_impl.hpp"

#include <iostream>

#include "md5.h"

static bool isPSArcFile(std::vector<byte>& header) {
  return (header[0] == 'P') && (header[1] == 'S') && (header[2] == 'A') && (header[3] == 'R');
}

static void writePSArcMagic(std::vector<byte>& header, size_t offset) {
  header[offset + 0] = 'P';
  header[offset + 1] = 'S';
  header[offset + 2] = 'A';
  header[offset + 3] = 'R';
}

static bool isEndianMismatch(std::vector<byte>& header) {
  uint16_t majorVersion = PSArc::readScalar<uint16_t>(header.data(), 0x04);

  // majorVersion larger than 255 means that the byte order is most likely the wrong way around as such a high major version shouldn't
  // exist.
  return (majorVersion > 255);
}

static uint32_t getBlockByteCountSize(uint32_t blockSize) {
  uint32_t blockByteCountSize = 2;
  if (blockSize > 0x10000)
    blockByteCountSize++;
  if (blockSize > 0x1000000)
    blockByteCountSize++;

  return blockByteCountSize;
}

static PSArc::CompressionType getCompressionType(byte* ptr) {
  switch (*ptr) {
    case 'z':
      return PSArc::CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;
    case 'l':
      return PSArc::CompressionType::PSARC_COMPRESSION_TYPE_LZMA;
    default:
      return PSArc::CompressionType::PSARC_COMPRESSION_TYPE_NONE;
  }
}

static void writeCompressionType(std::vector<byte>& header, size_t offset, PSArc::CompressionType type) {
  switch (type) {
    case PSArc::CompressionType::PSARC_COMPRESSION_TYPE_ZLIB:
      header[offset + 0x00] = 'z';
      header[offset + 0x01] = 'l';
      header[offset + 0x02] = 'i';
      header[offset + 0x03] = 'b';
      break;
    case PSArc::CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
      header[offset + 0x00] = 'l';
      header[offset + 0x01] = 'z';
      header[offset + 0x02] = 'm';
      header[offset + 0x03] = 'a';
      break;
    default:
      throw std::exception();
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

PSArc::PSArcStatus PSArc::PSArcHandle::Downsync(PSArcSettings settings, std::function<void(size_t, std::string)> callbackFunc) {
  if (this->serializationEndpoint == nullptr) {
    return PSARC_STATUS_ERROR_ENDPOINT;
  }

  if (this->archiveEndpoint == nullptr) {
    return PSARC_STATUS_ERROR_ENDPOINT;
  }

  bool endianMismatch = (std::endian::native != settings.endianness);

  // Rewrite manifest file.
  this->archiveEndpoint->RemoveManifestFile();

  std::vector<byte> manifestFileBytes;
  for (auto it = this->archiveEndpoint->begin(); it != this->archiveEndpoint->end(); it++) {
    std::string filePath = (*it)->path.generic_string();
    filePath += "\n";

    std::vector<byte> filePathBytes(filePath.begin(), filePath.end());
    manifestFileBytes.insert(manifestFileBytes.end(), filePathBytes.begin(), filePathBytes.end());
  }

  this->archiveEndpoint->AddFile(File("/PSArcManifest.bin", manifestFileBytes));

  uint32_t tocEntriesCount = this->archiveEndpoint->GetFileCount();

  std::vector<byte> header = std::vector<byte>(0x20);

  writePSArcMagic(header, 0x00);
  writeScalar<uint16_t>(header.data(), 0x04, settings.versionMajor, endianMismatch);
  writeScalar<uint16_t>(header.data(), 0x06, settings.versionMinor, endianMismatch);
  writeCompressionType(header, 0x08, settings.compressionType);
  writeScalar<uint32_t>(header.data(), 0x10, settings.tocEntrySize, endianMismatch);
  writeScalar<uint32_t>(header.data(), 0x14, tocEntriesCount, endianMismatch);
  writeScalar<uint32_t>(header.data(), 0x18, settings.blockSize, endianMismatch);
  writeScalar<uint32_t>(header.data(), 0x1C, settings.pathType, endianMismatch);

  uint32_t blockByteCountSize = getBlockByteCountSize(settings.blockSize);

  uint32_t numFilesCompressed = 0;
  uint32_t numBlocks          = 0;
  for (auto it = this->archiveEndpoint->begin(); it != this->archiveEndpoint->end(); it++) {
    if (callbackFunc)
      callbackFunc(numFilesCompressed++, (*it)->path.generic_string());

    (*it)->Compress(settings.compressionType, settings.blockSize);

    std::vector<uint32_t>& fileBlockSizes = (*it)->GetCompressedBlockSizes();
    numBlocks += fileBlockSizes.size();
  }

  uint32_t tocLength = settings.tocEntrySize * this->archiveEndpoint->GetFileCount() + numBlocks * blockByteCountSize;
  writeScalar<uint32_t>(header.data(), 0x0C, tocLength, endianMismatch);

  this->serializationEndpoint->Seek(0);
  this->serializationEndpoint->Write(header.data(), 0x20);

  size_t dataOffset = 0x20 + tocLength;

  std::vector<TocEntry> tocEntries = std::vector<TocEntry>();
  uint32_t* blockCompressedSizes   = new uint32_t[numBlocks];
  uint32_t blockOffset             = 0;

  this->serializationEndpoint->Seek(dataOffset);

  for (auto it = this->archiveEndpoint->begin(); it != this->archiveEndpoint->end(); it++) {
    if (callbackFunc)
      callbackFunc(tocEntries.size(), (*it)->path.generic_string());

    std::vector<uint32_t>& fileBlockSizes = (*it)->GetCompressedBlockSizes();
    const byte* fileCompressedBytes       = (*it)->GetCompressedBytes();
    size_t fileCompressedBytesSize        = (*it)->GetCompressedSize();

    TocEntry entry = TocEntry(blockOffset, (*it)->GetUncompressedSize(), dataOffset);

    // Compute MD5 hash
    MD5Context md5Context;
    md5Init(&md5Context);
    md5Update(&md5Context, fileCompressedBytes, fileCompressedBytesSize);
    md5Finalize(&md5Context);

    std::memcpy(entry.md5Hash, md5Context.digest, 16);

    tocEntries.push_back(entry);

    this->serializationEndpoint->Write(fileCompressedBytes, fileCompressedBytesSize);
    dataOffset += fileCompressedBytesSize;

    for (size_t i = 0; i < fileBlockSizes.size(); i++) {
      blockCompressedSizes[blockOffset++] = fileBlockSizes[i];
    }
  }

  this->serializationEndpoint->Seek(0x20);
  std::vector<byte> tocBytes = std::vector<byte>(settings.tocEntrySize);

  for (size_t i = 0; i < tocEntries.size(); i++) {
    tocEntries[i].ToByteArray(tocBytes.data(), endianMismatch);
    this->serializationEndpoint->Write(tocBytes.data(), settings.tocEntrySize);
  }

  byte* blockCompressedSizesBytes = new byte[blockByteCountSize * numBlocks];

  switch (blockByteCountSize) {
    case 2:
      for (uint32_t i = 0; i < numBlocks; i++) {
        writeScalar<uint16_t>(blockCompressedSizesBytes, i * blockByteCountSize, blockCompressedSizes[i], endianMismatch);
      }
      break;
    case 3:
      for (uint32_t i = 0; i < numBlocks; i++) {
        writeScalar<uint24_t>(blockCompressedSizesBytes, i * blockByteCountSize, uint24_t::From(blockCompressedSizes[i]), endianMismatch);
      }
      break;
    case 4:
      for (uint32_t i = 0; i < numBlocks; i++) {
        writeScalar<uint32_t>(blockCompressedSizesBytes, i * blockByteCountSize, blockCompressedSizes[i], endianMismatch);
      }
      break;
  }

  this->serializationEndpoint->Write(blockCompressedSizesBytes, blockByteCountSize * numBlocks);

  delete[] blockCompressedSizes;
  delete[] blockCompressedSizesBytes;

  return PSARC_STATUS_OK;
}

PSArc::PSArcStatus PSArc::PSArcHandle::Downsync(std::function<void(size_t, std::string)> callbackFunc) {
  return this->Downsync(PSArcSettings(), callbackFunc);
}

PSArc::PSArcStatus PSArc::PSArcHandle::Downsync() {
  return this->Downsync(PSArcSettings());
}

PSArc::PSArcStatus PSArc::PSArcHandle::Upsync() {
  if (this->parsingEndpoint == nullptr) {
    return PSARC_STATUS_ERROR_ENDPOINT;
  }

  if (this->archiveEndpoint == nullptr) {
    return PSARC_STATUS_ERROR_ENDPOINT;
  }

  std::vector<byte> header = std::vector<byte>(0x20);
  this->parsingEndpoint->Read(header.data(), 0x20);

  if (!isPSArcFile(header)) {
    return PSARC_STATUS_ERROR_HEADER;
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

  uint32_t blockByteCountSize = getBlockByteCountSize(blockSize);
  uint32_t numBlocks          = (tocLength - tocEntrySize * tocEntriesCount) / blockByteCountSize;

  this->blocks = new uint32_t[numBlocks];
  switch (blockByteCountSize) {
    case 2:
      for (uint32_t i = 0; i < numBlocks; i++) {
        this->blocks[i] = readScalar<uint16_t>(toc.data(), tocEntrySize * tocEntriesCount + i * blockByteCountSize, endianMismatch);
      }
      break;
    case 3:
      for (uint32_t i = 0; i < numBlocks; i++) {
        this->blocks[i] = readScalar<uint24_t>(toc.data(), tocEntrySize * tocEntriesCount + i * blockByteCountSize, endianMismatch);
      }
      break;
    case 4:
      for (uint32_t i = 0; i < numBlocks; i++) {
        this->blocks[i] = readScalar<uint32_t>(toc.data(), tocEntrySize * tocEntriesCount + i * blockByteCountSize, endianMismatch);
      }
      break;
  }

  // Blocks of size 0 are actually maximum block size
  for (uint32_t i = 0; i < numBlocks; i++) {
    this->blocks[i] = (this->blocks[i] > 0) ? this->blocks[i] : blockSize;
  }

  TocEntry manifest = tocEntries[0];

  if (manifest.uncompressedSize == 0)
    return PSARC_STATUS_ERROR_MANIFEST;

  this->parsingEndpoint->Seek(manifest.fileOffset);

  PSArc::PSArcFile* manifestFileSource = new PSArcFile(*this, manifest, this->compressionType);
  this->archiveEndpoint->AddFile(PSArc::File(std::string("/PSArcManifest.bin"), manifestFileSource));

  PSArc::File* manifestFile = this->archiveEndpoint->FindFile("/PSArcManifest.bin");

  if (manifestFile != nullptr) {
    const byte* manifest = manifestFile->GetUncompressedBytes();

    std::stringstream fileNames((reinterpret_cast<const char*>(manifest)));
    std::string fileName;

    for (uint32_t i = 1; i < tocEntries.size(); i++) {
      std::getline(fileNames, fileName, '\n');
      PSArc::PSArcFile* fileSource = new PSArcFile(*this, tocEntries[i], this->compressionType);
      PSArc::File file(fileName, fileSource);

      if (!this->archiveEndpoint->AddFile(file)) {
        return PSARC_STATUS_ERROR_INSERT;
      }
    }
  }

  return PSARC_STATUS_OK;
}

PSArc::FileData PSArc::PSArcFile::GetData() {
  if (this->psarcHandle.parsingEndpoint == nullptr) {
    return FileData{};
  }

  uint64_t uncompressedSize = this->entry.uncompressedSize;
  uint64_t uncompressedRead = 0;
  uint32_t blockOffset      = this->entry.blockOffset;
  uint64_t outputOffset     = 0;
  uint32_t blockSize        = this->psarcHandle.blockSize;
  uint32_t outputSize       = 0;

  FileData output;
  output.uncompressedTotalSize    = this->entry.uncompressedSize;
  output.compressionType          = this->psarcHandle.compressionType;
  output.uncompressedMaxBlockSize = this->psarcHandle.blockSize;
  output.compressedMaxBlockSize   = this->psarcHandle.blockSize;

  this->psarcHandle.parsingEndpoint->Seek(this->entry.fileOffset);

  do {
    uint32_t entrySize = this->psarcHandle.blocks[blockOffset];

    outputSize += entrySize;
    output.bytes.resize(outputSize);

    uint32_t maxPossibleUncompressedSize = std::min((uint64_t) blockSize, uncompressedSize - uncompressedRead);

    this->psarcHandle.parsingEndpoint->Read(output.bytes.data() + outputOffset, entrySize);

    bool blockIsCompressed;
    switch (this->psarcHandle.compressionType) {
      case CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
        // LZMA has no real magic, 0x5d is very common though. This can and will fail in some cases.
        // Another heuristic is to make sure that the data is actually smaller than the uncompressed part.
        // However, sometimes compression may lead to no reduction in size which would cause a fail here aswell.
        blockIsCompressed = output.bytes.data()[0] == 0x5d && entrySize != maxPossibleUncompressedSize;
        break;
      case CompressionType::PSARC_COMPRESSION_TYPE_ZLIB: {
        uint16_t zlib_magic = readScalar<uint16_t>(output.bytes.data(), 0x00);
        blockIsCompressed   = zlib_magic == 0x78da || zlib_magic == 0xda78 || zlib_magic == 0x789c || zlib_magic == 0x9c78
                            || zlib_magic == 0x7801 || zlib_magic == 0x0178;
      } break;
      default:
        blockIsCompressed = false;
        break;
    }
    output.blockIsCompressed.emplace_back(blockIsCompressed);

    outputOffset += entrySize;
    output.compressedBlockSizes.emplace_back(entrySize);

    uncompressedRead += blockSize;
    blockOffset++;
  } while (uncompressedRead < uncompressedSize);

  return output;
}

PSArc::CompressionType PSArc::PSArcFile::GetCompressionType() {
  return this->compressionType;
}

bool PSArc::PSArcFile::HasUncompressedSize() {
  return true;
}

size_t PSArc::PSArcFile::GetUncompressedSize() {
  return entry.uncompressedSize;
}
