#include "archive.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "compression.hpp"

PSArc::File::File(std::string name, std::vector<byte> data) : uncompressedBytes(data), path(name) {
}

PSArc::File::File(std::string name, FileSourceProvider* provider) : source(provider), path(name) {
  this->compressedSource = (provider != nullptr && provider->GetCompressionType() != CompressionType::NONE) ? true : false;
}

void PSArc::File::LoadCompressedBytes(CompressionType preferredType) {
  if (this->compressedBytes.has_value())
    return;

  if (this->source != nullptr) {
    if (this->compressedSource) {
      this->compressedBytes = this->source->GetData();
      return;
    }
    else if (preferredType != CompressionType::NONE) {
      LoadUncompressedBytes();
    }
  }

  if (this->uncompressedBytes.has_value() && preferredType != CompressionType::NONE) {
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

const std::vector<byte>& PSArc::File::GetCompressedBytes() {
  if (!this->compressedBytes.has_value()) {
    LoadCompressedBytes();
  }

  return this->compressedBytes.value().bytes;
}

const std::vector<byte>& PSArc::File::GetUncompressedBytes() {
  if (!this->uncompressedBytes.has_value()) {
    LoadUncompressedBytes();
  }

  return this->uncompressedBytes.value().bytes;
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

  this->uncompressedBytes.emplace(FileData{});

  PSArc::Decompress(this->uncompressedBytes.value(), this->compressedBytes.value());
}

bool PSArc::Archive::AddFile(File file) {
  std::reference_wrapper<Directory> current = std::ref(this->rootDirectory);

  bool parsePath = false;

  for (auto it = file.path.begin(); it != file.path.end(); it++) {
    auto pathElement            = (*it);
    std::string pathElementName = pathElement.generic_string();

    Directory& curr = current.get();

    if (parsePath) {
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
    else {
      if (pathElementName[0] == '/') {
        parsePath = true;
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

size_t PSArc::Archive::GetFileCount() {
  return this->fileCount;
}

size_t PSArc::File::GetUncompressedSize() {
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

std::vector<uint32_t>& PSArc::File::GetCompressedBlockSizes() {
  if (this->compressedBytes.has_value()) {
    return this->compressedBytes.value().compressedBlockSizes;
  }
  else {
    this->LoadCompressedBytes();
  }

  return this->compressedBytes.value().compressedBlockSizes;
}
