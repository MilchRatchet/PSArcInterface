#include "archive.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

PSArc::File::File(std::string name, FileSourceProvider* provider) : source(provider), path(name) {
  this->uncompressedSize = 0;
  this->compressedSize   = 0;
  this->compressionType  = (provider != nullptr) ? provider->GetCompressionType() : CompressionType::NONE;
}

bool PSArc::Archive::AddFile(File file) {
  Directory& current = this->rootDirectory;

  bool parsePath = false;

  for (auto it = file.path.begin(); it != file.path.end(); it++) {
    auto pathElement            = (*it);
    std::string pathElementName = pathElement.generic_string();

    if (parsePath) {
      if (pathElement.has_filename()) {
        // Is File
        current.files.push_back(file);
      }
      else {
        // Is Directory
        if (auto found = std::find_if(
              current.subDirectories.begin(), current.subDirectories.end(),
              [&pathElementName](const Directory& dir) { return dir.name == pathElementName; });
            found != std::end(current.subDirectories)) {
          current = *found;
        }
        else {
          Directory newDir(pathElementName);
          current.subDirectories.push_back(newDir);
          current = current.subDirectories.back();
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
