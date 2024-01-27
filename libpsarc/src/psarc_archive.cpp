#include "psarc_archive.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "psarc_compression.hpp"

PSArc::File::File(std::string name, std::vector<byte> data) : path(name) {
  this->uncompressedBytes.emplace();
  FileData& fileData = this->uncompressedBytes.value();

  fileData.bytes                    = data;
  fileData.uncompressedMaxBlockSize = 65536;
  fileData.uncompressedTotalSize    = data.size();
  fileData.compressedMaxBlockSize   = 65536;
}

PSArc::File::File(std::string name, FileSourceProvider* provider) : source(provider), path(name) {
  this->compressedSource =
    (provider != nullptr && provider->GetCompressionType() != CompressionType::PSARC_COMPRESSION_TYPE_NONE) ? true : false;
}

void PSArc::File::LoadCompressedBytes(CompressionType preferredType) {
  if (this->compressedBytes.has_value())
    return;

  if (this->source != nullptr) {
    if (this->compressedSource) {
      this->compressedBytes = this->source->GetData();
      return;
    }
    else if (preferredType != CompressionType::PSARC_COMPRESSION_TYPE_NONE) {
      LoadUncompressedBytes();
    }
  }

  if (this->uncompressedBytes.has_value() && preferredType != CompressionType::PSARC_COMPRESSION_TYPE_NONE) {
    Compress(preferredType, this->uncompressedBytes.value().uncompressedMaxBlockSize);
    return;
  }

  this->compressedBytes.emplace(FileData{});
}

void PSArc::File::LoadUncompressedBytes() {
  if (this->uncompressedBytes.has_value())
    return;

  if (this->source != nullptr) {
    if (this->compressedSource) {
      LoadCompressedBytes();
      Decompress();
      return;
    }
    else {
      this->uncompressedBytes = this->source->GetData();
      return;
    }
  }

  if (this->compressedBytes.has_value()) {
    Decompress();
    return;
  }

  this->uncompressedBytes.emplace(std::vector<byte>());
}

const byte* PSArc::File::GetCompressedBytes() {
  if (!this->compressedBytes.has_value()) {
    LoadCompressedBytes();
  }

  return this->compressedBytes.value().bytes.data();
}

const byte* PSArc::File::GetUncompressedBytes() {
  if (!this->uncompressedBytes.has_value()) {
    LoadUncompressedBytes();
  }

  return this->uncompressedBytes.value().bytes.data();
}

void PSArc::File::ClearCompressedBytes() {
  if (this->compressedBytes.has_value()) {
    this->compressedBytes.reset();
  }
}

void PSArc::File::ClearUncompressedBytes() {
  if (this->uncompressedBytes.has_value()) {
    this->uncompressedBytes.reset();
  }
}

void PSArc::File::Compress(CompressionType type, size_t blockSize) {
  if (!this->uncompressedBytes.has_value()) {
    return;
  }

  FileData toCompress;
  toCompress.compressionType          = type;
  toCompress.uncompressedMaxBlockSize = blockSize;
  toCompress.compressedMaxBlockSize   = blockSize;

  this->compressedBytes.emplace(toCompress);

  this->uncompressedBytes.value().Compress(this->compressedBytes.value());
}

void PSArc::File::Decompress() {
  if (!this->compressedBytes.has_value()) {
    return;
  }

  FileData uncompressedData;
  this->uncompressedBytes.emplace(uncompressedData);

  this->compressedBytes.value().Decompress(this->uncompressedBytes.value());
}

bool PSArc::Archive::AddFile(File file) {
  if (file.path.generic_string() == "/PSArcManifest.bin") {
    this->manifest.emplace(file);
    return true;
  }

  std::reference_wrapper<Directory> current = std::ref(this->rootDirectory);

  bool parsePath    = true;
  bool fileInserted = false;

  for (auto it = file.path.begin(); it != file.path.end(); it++) {
    std::filesystem::path pathElement = (*it);
    std::string pathElementName       = pathElement.generic_string();

    Directory& curr = current.get();

    if (pathElementName[0] == '/') {
      parsePath = true;
    }
    else if (parsePath) {
      if (it == --file.path.end()) {
        // Is File
        curr.files.push_back(file);
        this->fileCount++;
        fileInserted = true;
        break;
      }
      else {
        // Is Directory
        bool dirFound = false;
        for (PSArc::Directory& dir : curr.subDirectories) {
          if (dir.name == pathElementName) {
            current  = std::ref(dir);
            dirFound = true;
            break;
          }
        }

        if (!dirFound) {
          Directory newDir(pathElementName);
          curr.subDirectories.push_back(newDir);
          current = std::ref(curr.subDirectories.back());
        }
      }
    }
  }

  return fileInserted;
}

PSArc::File* PSArc::Archive::FindFile(std::string name) {
  if (name == "/PSArcManifest.bin") {
    if (!this->manifest.has_value())
      return nullptr;

    return &this->manifest.value();
  }

  std::reference_wrapper<Directory> current = std::ref(this->rootDirectory);

  std::filesystem::path path((name));

  bool parsePath = false;

  for (auto it = path.begin(); it != path.end(); it++) {
    auto pathElement            = (*it);
    std::string pathElementName = pathElement.generic_string();

    Directory& curr = current.get();

    if (parsePath) {
      if (it == --path.end()) {
        // Is File
        for (PSArc::File& file : curr.files) {
          if (file.path == path)
            return std::addressof(file);
        }

        return nullptr;
      }
      else {
        // Is Directory
        bool dirFound = false;
        for (PSArc::Directory& dir : curr.subDirectories) {
          if (dir.name == pathElementName) {
            current  = std::ref(dir);
            dirFound = true;
            break;
          }
        }

        if (!dirFound) {
          return nullptr;
        }
      }
    }
    else {
      if (pathElementName[0] == '/') {
        parsePath = true;
      }
    }
  }

  return nullptr;
}

size_t PSArc::Archive::GetFileCount() const noexcept {
  return this->fileCount + 1;
}

size_t PSArc::File::GetUncompressedSize() const noexcept {
  if (this->uncompressedBytes.has_value()) {
    return this->uncompressedBytes.value().uncompressedTotalSize;
  }

  if (this->compressedBytes.has_value()) {
    return this->compressedBytes.value().uncompressedTotalSize;
  }

  if (this->source->HasUncompressedSize()) {
    return this->source->GetUncompressedSize();
  }

  return 0;
}

size_t PSArc::File::GetCompressedSize() {
  if (!this->compressedBytes.has_value()) {
    this->LoadCompressedBytes();
  }

  return this->compressedBytes.value().bytes.size();
}

bool PSArc::File::IsUncompressedSizeAvailable() const noexcept {
  if (this->uncompressedBytes.has_value()) {
    return true;
  }

  if (this->compressedBytes.has_value()) {
    return true;
  }

  if (this->source->HasUncompressedSize()) {
    return true;
  }

  return false;
}

bool PSArc::File::IsCompressedSizeAvailable() const noexcept {
  return this->compressedBytes.has_value();
}

std::vector<size_t>& PSArc::File::GetCompressedBlockSizes() {
  if (this->compressedBytes.has_value()) {
    return this->compressedBytes.value().compressedBlockSizes;
  }
  else {
    this->LoadCompressedBytes();
  }

  return this->compressedBytes.value().compressedBlockSizes;
}

void PSArc::FileData::Compress(FileData& dst) {
  switch (dst.compressionType) {
    case CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
      LZMACompress(dst.bytes, this->bytes, dst.compressedBlockSizes, dst.uncompressedMaxBlockSize, dst.compressedMaxBlockSize);
      break;
    case CompressionType::PSARC_COMPRESSION_TYPE_NONE:
      dst = *this;
      break;
    default:
      break;
  }
}

void PSArc::FileData::Decompress(FileData& dst) {
  switch (this->compressionType) {
    case CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
      dst.uncompressedTotalSize = LZMADecompress(dst.bytes, this->bytes, this->compressedBlockSizes, this->blockIsCompressed);
      break;
    case CompressionType::PSARC_COMPRESSION_TYPE_NONE:
      dst = *this;
      break;
    default:
      break;
  }
}
