#pragma once

#include <string>
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
  std::vector<byte> uncompressedBytes;
  std::vector<byte> compressedBytes;
  size_t uncompressedSize;
  CompressionType compressionType;
  std::string fullPath;
  FileSourceProvider* source;

public:
  File(std::string, std::vector<byte>);
  File(std::string, FileSourceProvider&);
  std::string name;
};

class Directory {
public:
  std::string name;
  std::vector<Directory> subDirectories;
  std::vector<File> files;
};

class Archive {
private:
  uint16_t versionMajor;
  uint16_t versionMinor;
  PathType pathType;

public:
  Directory rootDirectory;
  bool AddFile(File);
  File FindFile(std::string) const;
  Directory FindDirectory(std::string) const;
};
}  // namespace PSArc
