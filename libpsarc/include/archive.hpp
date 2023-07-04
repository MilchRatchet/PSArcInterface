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

class ArchiveInterface {
public:
  virtual bool Upsync()   = 0;
  virtual bool Downsync() = 0;
};

class FileSourceProvider {
public:
  virtual std::vector<byte> GetBytes()         = 0;
  virtual CompressionType GetCompressionType() = 0;
};

class File {
private:
  std::optional<std::vector<byte>> uncompressedBytes;
  std::optional<std::vector<byte>> compressedBytes;
  CompressionType compressionType = CompressionType::NONE;
  FileSourceProvider* source;
  bool compressedSource = false;

public:
  File(std::string, std::vector<byte>);
  File(std::string, FileSourceProvider*);
  void LoadCompressedBytes();
  void LoadUncompressedBytes();
  std::vector<byte>& GetCompressedBytes();
  std::vector<byte>& GetUncompressedBytes();
  void ClearCompressedBytes();
  void ClearUncompressedBytes();
  void Compress(CompressionType);
  void Decompress();
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
  uint16_t versionMajor;
  uint16_t versionMinor;
  PathType pathType;
  Directory rootDirectory;
  std::unordered_map<std::string, File&> files;

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
  Iterator begin() {
    return Iterator(rootDirectory);
  };
  Iterator end() {
    return Iterator();
  };
};
}  // namespace PSArc
