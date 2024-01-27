#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "psarc_error.hpp"
#include "psarc_types.hpp"

namespace PSArc {

class ArchiveSyncSettings {};

/*
 * Abstract template for synchronizing the state of an archive from its
 * parsing endpoint (Upsync) or to its serialization endpoint.
 *
 * [Parsing endpoint] --Upsync-> [Archive] --Downsync-> [Serialization endpoint]
 */
class ArchiveInterface {
public:
  virtual PSArcStatus Upsync()   = 0;
  virtual PSArcStatus Downsync() = 0;
};

struct FileData {
  std::vector<byte> bytes;
  std::vector<size_t> compressedBlockSizes;
  std::vector<bool> blockIsCompressed;
  CompressionType compressionType = CompressionType::PSARC_COMPRESSION_TYPE_NONE;
  size_t uncompressedMaxBlockSize = 0;
  size_t uncompressedTotalSize    = 0;
  size_t compressedMaxBlockSize   = 0;

  void Compress(FileData& dst);
  void Decompress(FileData& dst);
};

/*
 * Abstract template for a class that provides access to a file's content.
 */
class FileSourceProvider {
public:
  virtual FileData GetData()                   = 0;
  virtual CompressionType GetCompressionType() = 0;
  virtual bool HasUncompressedSize()           = 0;
  virtual size_t GetUncompressedSize()         = 0;
};

/*
 * A generic file that handles the content of a file in both compressed and uncompressed state.
 * The content of the file does not have to reside in memory in either state.
 * The only guarantee is that an instance of this class can access the content in either state.
 */
class File {
private:
  std::optional<FileData> uncompressedBytes;
  std::optional<FileData> compressedBytes;
  FileSourceProvider* source = nullptr;
  bool compressedSource      = false;

public:
  File(std::string name, std::vector<byte> data);
  File(std::string name, FileSourceProvider* provider);
  void LoadCompressedBytes(CompressionType preferredType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA);
  void LoadUncompressedBytes();
  const byte* GetCompressedBytes();
  const byte* GetUncompressedBytes();
  void ClearCompressedBytes();
  void ClearUncompressedBytes();
  void Compress(CompressionType type, size_t blockSize);
  void Decompress();
  /* Returns the size of the uncompressed file. Note that this may cause file loads or decompression calls. */
  size_t GetUncompressedSize() const noexcept;
  /* Returns the size of the compressed file. Note that this may cause file loads or compression calls. */
  size_t GetCompressedSize();
  bool IsUncompressedSizeAvailable() const noexcept;
  bool IsCompressedSizeAvailable() const noexcept;
  std::vector<size_t>& GetCompressedBlockSizes();

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

/*
 * A virtual psarc archive.
 */
class Archive {
protected:
  Directory rootDirectory;
  std::optional<File> manifest;
  size_t fileCount = 0;

public:
  class Iterator {
  private:
    std::queue<Directory*> dirQueue;
    std::queue<File*> fileQueue;

  public:
    Iterator(){};
    Iterator(Archive* archive) : dirQueue(), fileQueue() {
      // It is important that the manifest file is iterated over first.
      if (archive->manifest.has_value())
        this->fileQueue.push(std::addressof(archive->manifest.value()));

      std::for_each(archive->rootDirectory.subDirectories.begin(), archive->rootDirectory.subDirectories.end(), [this](Directory& dir) {
        this->dirQueue.push(std::addressof(dir));
      });
      std::for_each(archive->rootDirectory.files.begin(), archive->rootDirectory.files.end(), [this](File& file) {
        this->fileQueue.push(std::addressof(file));
      });
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
  bool AddFile(File file);
  File* FindFile(std::string name);
  size_t GetFileCount() const noexcept;
  void RemoveManifestFile() noexcept {
    this->manifest.reset();
  };
  Iterator begin() {
    return Iterator(this);
  };
  Iterator end() {
    return Iterator();
  };
};
}  // namespace PSArc
