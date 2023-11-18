#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace PSArc {

class ArchiveSyncSettings {};

class ArchiveInterface {
public:
  virtual bool Upsync()   = 0;
  virtual bool Downsync() = 0;
};

struct FileData {
  std::vector<byte> bytes;
  std::vector<uint32_t> compressedBlockSizes;
  std::vector<bool> blockIsCompressed;
  CompressionType compressionType   = CompressionType::PSARC_COMPRESSION_TYPE_NONE;
  uint32_t uncompressedMaxBlockSize = 0;
  uint32_t uncompressedTotalSize    = 0;
};

class FileSourceProvider {
public:
  virtual FileData GetData()                   = 0;
  virtual CompressionType GetCompressionType() = 0;
  virtual bool HasUncompressedSize()           = 0;
  virtual size_t GetUncompressedSize()         = 0;
};

class File {
private:
  std::optional<FileData> uncompressedBytes;
  std::optional<FileData> compressedBytes;
  FileSourceProvider* source;
  bool compressedSource = false;

public:
  File(std::string, std::vector<byte>);
  File(std::string, FileSourceProvider*);
  void LoadCompressedBytes(CompressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA);
  void LoadUncompressedBytes();
  const byte* GetCompressedBytes();
  const byte* GetUncompressedBytes();
  void ClearCompressedBytes();
  void ClearUncompressedBytes();
  void Compress(CompressionType, uint32_t = 0);
  void Decompress();
  /* Returns the size of the uncompressed file. Note that this may cause file loads or decompression calls. */
  size_t GetUncompressedSize() const noexcept;
  /* Returns the size of the compressed file. Note that this may cause file loads or compression calls. */
  size_t GetCompressedSize();
  bool IsUncompressedSizeAvailable() const noexcept;
  bool IsCompressedSizeAvailable() const noexcept;
  std::vector<uint32_t>& GetCompressedBlockSizes();
  std::filesystem::path path;
  bool operator==(const File& rhs) {
    return this->path == rhs.path;
  }
};

class Directory {
public:
  Directory(std::string _name) : name(_name){};
  std::string name;
  std::vector<Directory> subDirectories;
  std::vector<File> files;
  bool operator<(const Directory& c) {
    return this->name < c.name;
  }
  bool operator>(const Directory& c) {
    return this->name > c.name;
  }
  bool operator==(const Directory& rhs) {
    return (this->name == rhs.name) && (this->subDirectories.size() == rhs.subDirectories.size())
           && (this->files.size() == rhs.files.size());
  }
};

class Archive {
private:
  Directory rootDirectory;
  size_t fileCount = 0;

public:
  class Iterator {
  private:
    std::queue<Directory*> dirQueue;
    std::queue<File*> fileQueue;

  public:
    Iterator(){};
    Iterator(Directory& root) : dirQueue(), fileQueue() {
      std::for_each(
        root.subDirectories.begin(), root.subDirectories.end(), [this](Directory& dir) { this->dirQueue.push(std::addressof(dir)); });
      std::for_each(root.files.begin(), root.files.end(), [this](File& file) { this->fileQueue.push(std::addressof(file)); });
    };
    const Iterator& operator++() {
      while (this->fileQueue.empty() && !this->dirQueue.empty()) {
        Directory* dir = this->dirQueue.front();
        this->dirQueue.pop();

        std::for_each(dir->files.begin(), dir->files.end(), [this](File& file) { this->fileQueue.push(std::addressof(file)); });
      }

      if (!this->fileQueue.empty()) {
        this->fileQueue.pop();
      }

      return *this;
    };
    Iterator operator++(int) {
      Iterator result = *this;
      ++(*this);
      return result;
    };
    File* operator*() {
      while (this->fileQueue.empty() && !this->dirQueue.empty()) {
        Directory* dir = this->dirQueue.front();
        this->dirQueue.pop();

        std::for_each(dir->subDirectories.begin(), dir->subDirectories.end(), [this](Directory& subdir) {
          this->dirQueue.push(std::addressof(subdir));
        });
        std::for_each(dir->files.begin(), dir->files.end(), [this](File& file) { this->fileQueue.push(std::addressof(file)); });
      }

      if (this->fileQueue.empty())
        return nullptr;

      return this->fileQueue.front();
    }
    bool operator==(const Iterator& rhs) {
      // This should be changed, what makes an iterator be equal to another and how can we do that efficiently?
      bool foundDifference = false;

      if (this->dirQueue.empty() || rhs.dirQueue.empty())
        foundDifference = foundDifference || (this->dirQueue.empty() != rhs.dirQueue.empty());

      if (this->fileQueue.empty() || rhs.fileQueue.empty())
        foundDifference = foundDifference || (this->fileQueue.empty() != rhs.fileQueue.empty());

      if (foundDifference)
        return false;

      if (!this->dirQueue.empty())
        foundDifference = foundDifference || (this->dirQueue.front() != rhs.dirQueue.front());

      if (!this->fileQueue.empty())
        foundDifference = foundDifference || (this->fileQueue.front() != rhs.fileQueue.front());

      return !foundDifference;
    }
    bool operator!=(const Iterator& rhs) {
      return !this->operator==(rhs);
    }
  };
  Archive() : rootDirectory("root"){};
  bool AddFile(File);
  File* FindFile(std::string);
  Directory* FindDirectory(std::string);
  size_t GetFileCount() const noexcept;
  Iterator begin() {
    return Iterator(rootDirectory);
  };
  Iterator end() {
    return Iterator();
  };
};
}  // namespace PSArc
