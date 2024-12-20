#include "psarc_impl.hpp"

#include <atomic>
#include <iostream>
#include <sstream>

#include "md5.h"

static bool isPSArcFile(std::vector<byte>& header) {
  return (header[0] == 'P') && (header[1] == 'S') && (header[2] == 'A') && (header[3] == 'R');
}

static bool isDSARFile(std::vector<byte>& header) {
  return (header[0] == 'D') && (header[1] == 'S') && (header[2] == 'A') && (header[3] == 'R');
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
      header[offset + 0x00] = 'z';
      header[offset + 0x01] = 'l';
      header[offset + 0x02] = 'i';
      header[offset + 0x03] = 'b';
      break;
    default:
      std::cout << "Error: Encountered invalid compression type during packing, defaulting to LZMA." << std::endl;
      [[fallthrough]];
    case PSArc::CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
      header[offset + 0x00] = 'l';
      header[offset + 0x01] = 'z';
      header[offset + 0x02] = 'm';
      header[offset + 0x03] = 'a';
      break;
  }
}

static std::vector<std::string> GetStringsFromManifest(std::string s, std::string delimiter, size_t totalSize) {
  size_t posStart = 0, posEnd, delimLen = delimiter.length();
  std::string token;
  std::vector<std::string> res;

  while ((posEnd = s.find(delimiter, posStart)) != std::string::npos) {
    token    = s.substr(posStart, posEnd - posStart);
    posStart = posEnd + delimLen;
    res.push_back(token);
  }

  res.push_back(s.substr(posStart, totalSize - posStart));

  // Zero terminate the last string.
  res.back().push_back('\0');

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
    // Manifest file is not list in the manifest.
    if (*it == manifestFile)
      continue;

    sortedFiles.push_back(*it);
  }

  // If a manifest file already existed, sort the files according to the original manifest.
  // Some games require the files to come in a very specific order.
  if (manifestFile != nullptr) {
    const std::shared_ptr<std::vector<byte>> manifestBytes = manifestFile->GetUncompressedBytes();

    std::string fileNames                        = std::string(manifestBytes->begin(), manifestBytes->end());
    const std::vector<std::string> listFileNames = GetStringsFromManifest(fileNames, std::string("\n"), manifestBytes->size());

    uint32_t index = 0;

    for (uint32_t i = 0; i < sortedFiles.size(); i++) {
      const std::string& fileName = listFileNames[i];

      File* file = this->archiveEndpoint->FindFile(fileName, this->pathType);

      if (file != nullptr) {
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
  }

  // Now we can safely remove the manifest file
  this->archiveEndpoint->RemoveManifestFile();

  // Generate new manifest file
  std::vector<byte> manifestFileBytes;
  for (auto it = sortedFiles.begin(); it != sortedFiles.end(); it++) {
    std::string filePath = (*it)->GetPathString(settings.pathType);
    filePath += "\n";

    std::vector<byte> filePathBytes(filePath.begin(), filePath.end());
    manifestFileBytes.insert(manifestFileBytes.end(), filePathBytes.begin(), filePathBytes.end());
  }

  // Add the new manifest file
  this->archiveEndpoint->AddFile(File("PSArcManifest.bin", manifestFileBytes));

  size_t tocEntriesCount = this->archiveEndpoint->GetFileCount();

  std::vector<byte> header = std::vector<byte>(0x20);

  writePSArcMagic(header, 0x00);
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

  std::vector<File*> files;

  // PSArcHandle iterator is slow and not parallelizable. Hence we first gather all files
  // and then do compression in parallel.
  for (auto it = this->archiveEndpoint->begin(); it != this->archiveEndpoint->end(); it++) {
    files.push_back(*it);
  }

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

  size_t tocLength = settings.tocEntrySize * this->archiveEndpoint->GetFileCount() + numBlocks * blockByteCountSize;
  writeScalar<uint32_t>(header.data(), 0x0C, uint32_t(tocLength), endianMismatch);

  this->serializationEndpoint->Seek(0);
  this->serializationEndpoint->Write(header.data(), 0x20);

  size_t dataOffset = 0x20 + tocLength;

  std::vector<TocEntry> tocEntries = std::vector<TocEntry>();
  size_t* blockCompressedSizes     = new size_t[numBlocks];
  size_t blockOffset               = 0;

  this->serializationEndpoint->Seek(dataOffset);

  for (auto it = this->archiveEndpoint->begin(); it != this->archiveEndpoint->end(); it++) {
    if (callbackFunc)
      callbackFunc(tocEntries.size(), (*it)->GetPathString(settings.pathType));

    std::vector<size_t>& fileBlockSizes                          = (*it)->GetCompressedBlockSizes();
    const std::shared_ptr<std::vector<byte>> fileCompressedBytes = (*it)->GetCompressedBytes();
    size_t fileCompressedBytesSize                               = (*it)->GetCompressedSize();

    TocEntry entry = TocEntry(uint32_t(blockOffset), uint64_t((*it)->GetUncompressedSize()), dataOffset);

    // Compute MD5 hash
    if ((*it)->IsManifest()) {
      // Manifest seems to have a hash of 0.
      std::memset(entry.md5Hash, 0, 16);
    }
    else {
      MD5Context md5Context;
      md5Init(&md5Context);
      md5Update(&md5Context, fileCompressedBytes->data(), fileCompressedBytesSize);
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

  byte* blockCompressedSizesBytes = new byte[blockByteCountSize * numBlocks];

  switch (blockByteCountSize) {
    case 2:
      for (size_t i = 0; i < numBlocks; i++) {
        writeScalar<uint16_t>(blockCompressedSizesBytes, i * blockByteCountSize, uint16_t(blockCompressedSizes[i]), endianMismatch);
      }
      break;
    case 3:
      for (size_t i = 0; i < numBlocks; i++) {
        writeScalar<uint24_t>(
          blockCompressedSizesBytes, i * blockByteCountSize, uint24_t::From(uint32_t(blockCompressedSizes[i])), endianMismatch);
      }
      break;
    case 4:
      for (size_t i = 0; i < numBlocks; i++) {
        writeScalar<uint32_t>(blockCompressedSizesBytes, i * blockByteCountSize, uint32_t(blockCompressedSizes[i]), endianMismatch);
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
    const std::vector<std::string> listFileNames = GetStringsFromManifest(fileNames, std::string("\n"), manifestBytes->size());

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
