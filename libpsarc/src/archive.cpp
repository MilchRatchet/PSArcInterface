#include "archive.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "compression.hpp"

PSArc::File::File(std::string name, std::vector<byte> data) : uncompressedBytes(data), path(name) {
}

PSArc::File::File(std::string name, FileSourceProvider* provider) : source(provider), path(name) {
  this->compressionType  = (provider != nullptr) ? provider->GetCompressionType() : CompressionType::NONE;
  this->compressedSource = (provider != nullptr && provider->GetCompressionType() != CompressionType::NONE) ? true : false;
}

void PSArc::File::LoadCompressedBytes() {
  if (this->compressedBytes.has_value())
    return;

  if (this->source != nullptr) {
    if (this->compressedSource) {
      this->compressedBytes = this->source->GetBytes();
      return;
    }
    else if (this->compressionType != CompressionType::NONE) {
      LoadUncompressedBytes();
    }
  }

  if (this->uncompressedBytes.has_value() && this->compressionType != CompressionType::NONE) {
    Compress(this->compressionType);
    return;
  }

  this->compressedBytes.emplace(std::vector<byte>());
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
      this->uncompressedBytes = this->source->GetBytes();
      return;
    }
  }

  if (this->compressedBytes.has_value()) {
    Decompress();
    return;
  }

  this->uncompressedBytes.emplace(std::vector<byte>());
}

std::vector<byte>& PSArc::File::GetCompressedBytes() {
  if (!this->compressedBytes.has_value()) {
    LoadCompressedBytes();
  }

  return this->compressedBytes.value();
}

std::vector<byte>& PSArc::File::GetUncompressedBytes() {
  if (!this->uncompressedBytes.has_value()) {
    LoadUncompressedBytes();
  }

  return this->uncompressedBytes.value();
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

  if (type == this->compressionType && this->compressedBytes.has_value() && this->compressedMaxBlockSize == blockSize) {
    return;
  }

  this->compressionType        = type;
  this->compressedMaxBlockSize = blockSize;

  if (type == CompressionType::NONE) {
    ClearCompressedBytes();
    return;
  }

  if (!this->compressedBytes.has_value()) {
    this->compressedBytes.emplace(std::vector<byte>());
  }

  PSArc::Compress(this->compressedBytes.value(), this->uncompressedBytes.value(), type, this->compressedBlockSizes, blockSize);
}

void PSArc::File::Decompress() {
  if (!this->compressedBytes.has_value()) {
    ClearUncompressedBytes();
    return;
  }

  if (!this->uncompressedBytes.has_value()) {
    this->uncompressedBytes.emplace(std::vector<byte>());
  }

  PSArc::Decompress(this->uncompressedBytes.value(), this->compressedBytes.value(), this->compressionType);
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
        this->files.insert(std::pair<std::string, PSArc::File&>(file.path.generic_string(), curr.files.back()));
      }
      else {
        // Is Directory
        if (auto found = std::find_if(
              curr.subDirectories.begin(), curr.subDirectories.end(),
              [&pathElementName](const Directory& dir) { return dir.name == pathElementName; });
            found != std::end(curr.subDirectories)) {
          current = std::ref(*found);
        }
        else {
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
  auto result = files.find(name);

  if (result == std::end(files))
    return nullptr;

  PSArc::File& file = (*result).second;

  // Apparently this is the address of the actual object behind the reference.
  return std::addressof(file);
}

size_t PSArc::Archive::GetFileCount() {
  return files.size();
}

size_t PSArc::File::GetUncompressedSize() {
  if (this->uncompressedBytes.has_value()) {
    return this->uncompressedBytes.value().size();
  }

  if (this->uncompressedSize.has_value()) {
    return this->uncompressedSize.value();
  }

  if (this->source->HasUncompressedSize()) {
    this->uncompressedSize.emplace(this->source->GetUncompressedSize());
    return this->uncompressedSize.value();
  }

  if (!this->uncompressedBytes.has_value()) {
    this->LoadUncompressedBytes();
  }

  return this->uncompressedBytes.value().size();
}

std::vector<uint32_t>& PSArc::File::GetCompressedBlockSizes() {
  return this->compressedBlockSizes;
}
