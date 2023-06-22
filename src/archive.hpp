#pragma once

#include <string>
#include <vector>

namespace PSArc {

class ArchiveInterface {
public:
  virtual bool upsync()   = 0;
  virtual bool downsync() = 0;
}

class File {
private:
  std::vector<byte> uncompressedBytes;
  std::vector<byte> compressedBytes;
  CompressionType compressionType;
  std::string fullPath;

public:
  File(std::string, std::vector<byte>);
  std::string name;
}

class Directory {
public:
  std::string name;
  std::vector<Directory> subDirectories;
  std::vector<File> files;
}

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
}
}  // namespace PSArc
