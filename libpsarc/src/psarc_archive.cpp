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
}

const std::shared_ptr<std::vector<byte>> PSArc::File::GetCompressedBytes() {
  if (!this->compressedBytes.has_value()) {
    LoadCompressedBytes();
  }

  return std::make_shared<std::vector<byte>>(this->compressedBytes.value_or(FileData()).bytes);
}

const std::shared_ptr<std::vector<byte>> PSArc::File::GetUncompressedBytes() {
  if (!this->uncompressedBytes.has_value()) {
    LoadUncompressedBytes();
  }

  return std::make_shared<std::vector<byte>>(this->uncompressedBytes.value_or(FileData()).bytes);
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
  if (file.IsManifest()) {
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
        const std::string fileRelPath = file.GetPathString(PSARC_PATH_TYPE_RELATIVE);
        for (PSArc::File& existing : curr.files) {
          if (existing.GetPathString(PSARC_PATH_TYPE_RELATIVE) == fileRelPath) {
            existing     = std::move(file);
            fileInserted = true;
            return true;
          }
        }
        curr.files.push_back(std::move(file));
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

PSArc::File* PSArc::Archive::FindFile(const std::string& name, PathType pathType) {
  if (name == "PSArcManifest.bin") {
    if (!this->manifest.has_value())
      return nullptr;

    return &this->manifest.value();
  }

  std::reference_wrapper<Directory> current = std::ref(this->rootDirectory);

  std::filesystem::path path((name));

  // Normalize the search name to the same form GetPathString produces for the
  // given pathType, so the leaf comparison is not sensitive to whether the
  // caller supplied a leading '/' or not.
  std::string normalizedName = path.generic_string();
  switch (pathType) {
    case PSARC_PATH_TYPE_RELATIVE:
    case PSARC_PATH_TYPE_IGNORECASE:
      if (!normalizedName.empty() && normalizedName.front() == '/')
        normalizedName = normalizedName.substr(1);
      break;
    case PSARC_PATH_TYPE_ABSOLUTE:
      if (!normalizedName.empty() && normalizedName.front() != '/')
        normalizedName = "/" + normalizedName;
      break;
    default:
      break;
  }

  // If the path starts with "/" (has a root_directory), the first iterator
  // element is "/" itself — skip it before parsing real components.
  // If there is no root_directory (truly relative path), start parsing immediately.
  bool parsePath = path.root_directory().empty();

  for (auto it = path.begin(); it != path.end(); it++) {
    auto pathElement            = (*it);
    std::string pathElementName = pathElement.generic_string();

    Directory& curr = current.get();

    if (parsePath) {
      if (it == --path.end()) {
        // Is File
        for (PSArc::File& file : curr.files) {
          if (file.GetPathString(pathType) == normalizedName)
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

std::vector<size_t>& PSArc::File::GetCompressedBlockSizes() {
  if (this->compressedBytes.has_value()) {
    return this->compressedBytes.value().compressedBlockSizes;
  }
  else {
    this->LoadCompressedBytes();
  }

  return this->compressedBytes.value().compressedBlockSizes;
}

bool PSArc::File::IsManifest() const noexcept {
  return this->GetPathString(PathType::PSARC_PATH_TYPE_RELATIVE) == "PSArcManifest.bin";
}

std::string PSArc::File::GetPathString(PathType pathType) const noexcept {
  std::string filePath = this->path.generic_string();

  switch (pathType) {
    case PSARC_PATH_TYPE_RELATIVE:
      if (filePath.front() == '/') {
        filePath = filePath.substr(1);
      }
      break;
    case PSARC_PATH_TYPE_IGNORECASE:
      break;
    case PSARC_PATH_TYPE_ABSOLUTE:
      if (filePath.front() != '/') {
        filePath = "/" + filePath;
      }
      break;
    default:
      break;
  }

  return filePath;
}

void PSArc::FileData::Compress(FileData& dst) {
  switch (dst.compressionType) {
    case CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
      LZMACompress(dst.bytes, this->bytes, dst.compressedBlockSizes, dst.uncompressedMaxBlockSize, dst.compressedMaxBlockSize);
      break;
    case CompressionType::PSARC_COMPRESSION_TYPE_ZLIB:
      ZLIBCompress(dst.bytes, this->bytes, dst.compressedBlockSizes, dst.uncompressedMaxBlockSize, dst.compressedMaxBlockSize);
      break;
    case CompressionType::PSARC_COMPRESSION_TYPE_NONE:
      dst = *this;
      break;
    default:
      break;
  }

  // Populate blockIsCompressed: a compressedBlockSize of 0 signals a full-size
  // uncompressed block (PSArc convention); all other blocks are compressed.
  if (dst.compressionType != CompressionType::PSARC_COMPRESSION_TYPE_NONE) {
    dst.blockIsCompressed.resize(dst.compressedBlockSizes.size());
    for (size_t i = 0; i < dst.compressedBlockSizes.size(); ++i)
      dst.blockIsCompressed[i] = (dst.compressedBlockSizes[i] != 0);
  }
}

void PSArc::FileData::Decompress(FileData& dst) {
  switch (this->compressionType) {
    case CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
      dst.uncompressedTotalSize = LZMADecompress(dst.bytes, this->bytes, this->compressedBlockSizes, this->blockIsCompressed);
      break;
    case CompressionType::PSARC_COMPRESSION_TYPE_ZLIB:
      dst.uncompressedTotalSize = ZLIBDecompress(dst.bytes, this->bytes, this->compressedBlockSizes, this->blockIsCompressed);
      break;
    case CompressionType::PSARC_COMPRESSION_TYPE_NONE:
      dst = *this;
      break;
    default:
      break;
  }
}
