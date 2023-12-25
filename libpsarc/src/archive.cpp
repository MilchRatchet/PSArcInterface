#include "archive.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "compression.hpp"

PSArc::File::File(std::string name, std::vector<byte> data) : path(name) {
  this->uncompressedBytes.emplace();
  FileData& fileData = this->uncompressedBytes.value();

  fileData.bytes                    = data;
  fileData.uncompressedMaxBlockSize = 65535;
  fileData.uncompressedTotalSize    = data.size();
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
    Compress(preferredType);
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

void PSArc::File::Compress(CompressionType type, uint32_t blockSize) {
  if (!this->uncompressedBytes.has_value()) {
    return;
  }

  FileData toCompress;
  toCompress.compressionType          = type;
  toCompress.uncompressedMaxBlockSize = blockSize;

  this->compressedBytes.emplace(toCompress);

  PSArc::Compress(this->compressedBytes.value(), this->uncompressedBytes.value());
}

void PSArc::File::Decompress() {
  if (!this->compressedBytes.has_value()) {
    return;
  }

  FileData uncompressedData;

  PSArc::Decompress(uncompressedData, this->compressedBytes.value());

  this->uncompressedBytes.emplace(uncompressedData);
}

bool PSArc::Archive::AddFile(File file) {
  std::reference_wrapper<Directory> current = std::ref(this->rootDirectory);

  bool parsePath = true;

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

  return true;
}

PSArc::File* PSArc::Archive::FindFile(std::string name) {
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
  return this->fileCount;
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

std::vector<uint32_t>& PSArc::File::GetCompressedBlockSizes() {
  if (this->compressedBytes.has_value()) {
    return this->compressedBytes.value().compressedBlockSizes;
  }
  else {
    this->LoadCompressedBytes();
  }

  return this->compressedBytes.value().compressedBlockSizes;
}
