#include "psarc_impl.hpp"

#include <atomic>
#include <iostream>
#include <sstream>

#include "md5.h"

static bool isPSArcFile(std::vector<byte>& header) {
  return std::memcmp(header.data(), "PSAR", 4) == 0;
}

static bool isDSARFile(std::vector<byte>& header) {
  return std::memcmp(header.data(), "DSAR", 4) == 0;
}

static void writePSArcMagic(std::vector<byte>& header) {
  std::memcpy(header.data(), "PSAR", 4);
}

static bool isEndianMismatch(std::vector<byte>& header) {
  uint16_t majorVersion = PSArc::readScalar<uint16_t>(header.data(), 0x04);

  // majorVersion larger than 255 means that the byte order is most likely the wrong way around as such a high major version shouldn't
  // exist.
  return (majorVersion > 255);
}

static uint32_t getBlockByteCountSize(size_t blockSize) {
  uint32_t blockByteCountSize = 2;
  if (blockSize > 0x10000ull)
    blockByteCountSize++;
  if (blockSize > 0x1000000ull)
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
      std::memcpy(header.data() + offset, "zlib", 4);
      break;
    default:
      std::cout << "Error: Encountered invalid compression type during packing, defaulting to LZMA." << std::endl;
      [[fallthrough]];
    case PSArc::CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
      std::memcpy(header.data() + offset, "lzma", 4);
      break;
  }
}

static std::vector<std::string> GetStringsFromManifest(const std::string& s) {
  std::vector<std::string> res;
  size_t posStart = 0, posEnd;

  while ((posEnd = s.find('\n', posStart)) != std::string::npos) {
    res.push_back(s.substr(posStart, posEnd - posStart));
    posStart = posEnd + 1;
  }

  if (posStart < s.size())
    res.push_back(s.substr(posStart));

  return res;
}

PSArc::PSArcHandle::PSArcHandle() {
}

void PSArc::PSArcHandle::SetParsingEndpoint(InputMemoryHandle* memHandle) {
  this->parsingEndpoint = memHandle;
}

void PSArc::PSArcHandle::SetSerializationEndpoint(OutputMemoryHandle* memHandle) {
  this->serializationEndpoint = memHandle;
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

  File* manifestFile = this->archiveEndpoint->FindFile(std::string("PSArcManifest.bin"), this->pathType);

  // Rewrite manifest file.
  std::vector<File*> sortedFiles;
  for (auto it = this->archiveEndpoint->begin(); it != this->archiveEndpoint->end(); it++) {
    // Manifest file is not listed in the manifest.
    if (*it == manifestFile)
      continue;

    sortedFiles.push_back(*it);
  }

  // If a manifest file already existed, sort the files according to the original manifest.
  // Some games require the files to come in a very specific order.
  if (manifestFile != nullptr) {
    const std::shared_ptr<std::vector<byte>> manifestBytes = manifestFile->GetUncompressedBytes();

    std::string fileNames                        = std::string(manifestBytes->begin(), manifestBytes->end());
    const std::vector<std::string> listFileNames = GetStringsFromManifest(fileNames);

    uint32_t index = 0;

    for (uint32_t i = 0; i < listFileNames.size(); i++) {
      File* file = this->archiveEndpoint->FindFile(listFileNames[i], this->pathType);

      // File was listed in original manifest but seems to have been removed.
      if (file == nullptr)
        continue;

      uint32_t originalFileIndex;
      for (originalFileIndex = index; originalFileIndex < sortedFiles.size(); originalFileIndex++) {
        if (sortedFiles[originalFileIndex] == file)
          break;
      }

      if (originalFileIndex != sortedFiles.size()) {
        if (originalFileIndex != index) {
          sortedFiles[originalFileIndex] = sortedFiles[index];
          sortedFiles[index]             = file;
        }

        index++;
      }
    }
  }

  // Now we can safely remove the manifest file
  this->archiveEndpoint->RemoveManifestFile();

  // Generate new manifest file
  std::vector<byte> manifestFileBytes;
  for (auto it = sortedFiles.begin(); it != sortedFiles.end(); it++) {
    std::string filePath = (*it)->GetPathString(settings.pathType);
    if (std::next(it) != sortedFiles.end())
      filePath += '\n';
    manifestFileBytes.insert(manifestFileBytes.end(), filePath.begin(), filePath.end());
  }

  // Add the new manifest file
  this->archiveEndpoint->AddFile(File("PSArcManifest.bin", manifestFileBytes));

  // Build the ordered file list: manifest first, then sortedFiles.
  // This must be used for all subsequent loops so that TOC entry indices
  // match the order recorded in the manifest.
  File* newManifestFile = this->archiveEndpoint->FindFile(std::string("PSArcManifest.bin"), this->pathType);
  std::vector<File*> files;
  if (newManifestFile != nullptr)
    files.push_back(newManifestFile);
  for (File* f : sortedFiles)
    files.push_back(f);

  size_t tocEntriesCount = files.size();

  std::vector<byte> header = std::vector<byte>(0x20);

  writePSArcMagic(header);
  writeScalar<uint16_t>(header.data(), 0x04, settings.versionMajor, endianMismatch);
  writeScalar<uint16_t>(header.data(), 0x06, settings.versionMinor, endianMismatch);
  writeCompressionType(header, 0x08, settings.compressionType);
  writeScalar<uint32_t>(header.data(), 0x10, settings.tocEntrySize, endianMismatch);
  writeScalar<uint32_t>(header.data(), 0x14, uint32_t(tocEntriesCount), endianMismatch);
  writeScalar<uint32_t>(header.data(), 0x18, settings.blockSize, endianMismatch);
  writeScalar<uint32_t>(header.data(), 0x1C, settings.pathType, endianMismatch);

  uint32_t blockByteCountSize = getBlockByteCountSize(settings.blockSize);

  size_t numFilesCompressed     = 0;
  std::atomic<size_t> numBlocks = 0;

  const int file_count = int(files.size());

  // Dynamic schedule is important as compression time depends heavily on
  // file size and can thus vary greatly.
  // Note: MSVC requires int type for iterator variable so we can't use C++ iterators here.
#pragma omp parallel for schedule(dynamic, 1) shared(file_count, files, settings, callbackFunc)
  for (int i = 0; i < file_count; i++) {
    File* file = files[i];

    if (callbackFunc)
      callbackFunc(numFilesCompressed++, file->GetPathString(settings.pathType));

    file->Compress(settings.compressionType, settings.blockSize);

    std::vector<size_t>& fileBlockSizes = file->GetCompressedBlockSizes();
    numBlocks += fileBlockSizes.size();
  }

  // tocLength field stores the total size: header (0x20) + TOC entries + block table.
  size_t tocLength = 0x20 + settings.tocEntrySize * files.size() + numBlocks * blockByteCountSize;
  writeScalar<uint32_t>(header.data(), 0x0C, uint32_t(tocLength), endianMismatch);

  this->serializationEndpoint->Seek(0);
  this->serializationEndpoint->Write(header.data(), 0x20);

  // File data starts immediately after the header + TOC entries + block table,
  // which is exactly tocLength bytes from the start of the file.
  size_t dataOffset = tocLength;

  std::vector<TocEntry> tocEntries;
  std::vector<size_t> blockCompressedSizes(numBlocks);
  size_t blockOffset = 0;

  this->serializationEndpoint->Seek(dataOffset);

  for (File* file : files) {
    if (callbackFunc)
      callbackFunc(tocEntries.size(), file->GetPathString(settings.pathType));

    std::vector<size_t>& fileBlockSizes                          = file->GetCompressedBlockSizes();
    const std::shared_ptr<std::vector<byte>> fileCompressedBytes = file->GetCompressedBytes();
    size_t fileCompressedBytesSize                               = file->GetCompressedSize();

    TocEntry entry = TocEntry(uint32_t(blockOffset), uint64_t(file->GetUncompressedSize()), dataOffset);

    // Compute MD5 hash
    if (file->IsManifest()) {
      // Manifest seems to have a hash of 0.
      std::memset(entry.md5Hash, 0, 16);
    }
    else {
      // Hash is computed from the file path (as ASCII bytes).
      std::string filePath = file->GetPathString(settings.pathType);

      MD5Context md5Context;
      md5Init(&md5Context);
      md5Update(&md5Context, reinterpret_cast<const uint8_t*>(filePath.data()), filePath.size());
      md5Finalize(&md5Context);

      std::memcpy(entry.md5Hash, md5Context.digest, 16);
    }

    tocEntries.push_back(entry);

    this->serializationEndpoint->Write(fileCompressedBytes->data(), fileCompressedBytesSize);
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

  std::vector<byte> blockCompressedSizesBytes(blockByteCountSize * numBlocks);

  switch (blockByteCountSize) {
    case 2:
      for (size_t i = 0; i < numBlocks; i++) {
        writeScalar<uint16_t>(blockCompressedSizesBytes.data(), i * blockByteCountSize, uint16_t(blockCompressedSizes[i]), endianMismatch);
      }
      break;
    case 3:
      for (size_t i = 0; i < numBlocks; i++) {
        writeScalar<uint24_t>(
          blockCompressedSizesBytes.data(), i * blockByteCountSize, uint24_t::From(uint32_t(blockCompressedSizes[i])), endianMismatch);
      }
      break;
    case 4:
      for (size_t i = 0; i < numBlocks; i++) {
        writeScalar<uint32_t>(blockCompressedSizesBytes.data(), i * blockByteCountSize, uint32_t(blockCompressedSizes[i]), endianMismatch);
      }
      break;
  }

  this->serializationEndpoint->Write(blockCompressedSizesBytes.data(), blockCompressedSizesBytes.size());

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

  // Some archives have a DSAR header which contains the offset to the actual PSArc file.
  if (isDSARFile(header)) {
    return PSARC_STATUS_ERROR_DSAR_FILE;
#if 0
    // TODO: Determine endianness of DSAR files.
    uint32_t psarcFileStart = readScalar<uint32_t>(header.data(), 0x0C, false);

    // TODO: Figure out why we need 0x02 and if that number has to be determined by something.
    this->parsingEndpoint->Seek(psarcFileStart + 0x02);
    this->parsingEndpoint->Read(header.data(), 0x20);
#endif
  }

  if (!isPSArcFile(header)) {
    return PSARC_STATUS_ERROR_HEADER;
  }

  bool endianMismatch = isEndianMismatch(header);

  this->endianness =
    endianMismatch ? (std::endian::native == std::endian::little ? std::endian::big : std::endian::little) : std::endian::native;
  this->compressionType    = getCompressionType(header.data() + 0x08);
  uint32_t tocLength       = readScalar<uint32_t>(header.data(), 0x0C, endianMismatch);
  uint32_t tocEntrySize    = readScalar<uint32_t>(header.data(), 0x10, endianMismatch);
  uint32_t tocEntriesCount = readScalar<uint32_t>(header.data(), 0x14, endianMismatch);
  this->blockSize          = readScalar<uint32_t>(header.data(), 0x18, endianMismatch);
  this->pathType           = static_cast<PathType>(readScalar<uint32_t>(header.data(), 0x1C, endianMismatch));

  // tocLength stores: header (0x20) + entries + block table. Subtract the header to get the actual TOC bytes.
  uint32_t tocActualLength = tocLength - 0x20;
  std::vector<byte> toc    = std::vector<byte>(tocActualLength);
  this->parsingEndpoint->Read(toc.data(), tocActualLength);

  std::vector<TocEntry> tocEntries;

  for (uint32_t i = 0; i < tocEntriesCount; i++) {
    tocEntries.push_back(TocEntry(toc.data(), i * tocEntrySize, endianMismatch));
  }

  uint32_t blockByteCountSize = getBlockByteCountSize(blockSize);
  uint32_t numBlocks          = (tocActualLength - tocEntrySize * tocEntriesCount) / blockByteCountSize;

  this->blocks = new size_t[numBlocks];
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
  this->archiveEndpoint->AddFile(PSArc::File(std::string("PSArcManifest.bin"), manifestFileSource));

  PSArc::File* manifestFile = this->archiveEndpoint->FindFile("PSArcManifest.bin", this->pathType);

  if (manifestFile != nullptr) {
    const std::shared_ptr<std::vector<byte>> manifestBytes = manifestFile->GetUncompressedBytes();

    std::string fileNames                        = std::string(manifestBytes->begin(), manifestBytes->end());
    const std::vector<std::string> listFileNames = GetStringsFromManifest(fileNames);

    for (uint32_t i = 1; i < tocEntries.size(); i++) {
      const std::string& fileName  = listFileNames[i - 1];
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
  size_t blockSize          = this->psarcHandle.blockSize;
  size_t outputSize         = 0;

  FileData output;
  output.uncompressedTotalSize    = this->entry.uncompressedSize;
  output.compressionType          = this->psarcHandle.compressionType;
  output.uncompressedMaxBlockSize = this->psarcHandle.blockSize;
  output.compressedMaxBlockSize   = this->psarcHandle.blockSize;

  this->psarcHandle.parsingEndpoint->Seek(this->entry.fileOffset);

  do {
    size_t entrySize = this->psarcHandle.blocks[blockOffset];

    outputSize += entrySize;
    output.bytes.resize(outputSize);

    uint64_t maxPossibleUncompressedSize = std::min((uint64_t) blockSize, uncompressedSize - uncompressedRead);

    this->psarcHandle.parsingEndpoint->Read(output.bytes.data() + outputOffset, entrySize);

    bool blockIsCompressed;
    switch (this->psarcHandle.compressionType) {
      case CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
        // LZMA has no real magic, 0x5d and 0x2c are the most common first bytes of LZMA props.
        // Another heuristic is to make sure that the data is actually smaller than the uncompressed part.
        // However, sometimes compression may lead to no reduction in size which would cause a fail here aswell.
        blockIsCompressed = (output.bytes.data()[outputOffset] == 0x5d || output.bytes.data()[outputOffset] == 0x2c)
                            && entrySize != maxPossibleUncompressedSize;
        break;
      case CompressionType::PSARC_COMPRESSION_TYPE_ZLIB: {
        uint16_t zlib_magic = readScalar<uint16_t>(output.bytes.data(), outputOffset);
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
