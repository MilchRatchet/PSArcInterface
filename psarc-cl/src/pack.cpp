#include <iostream>
#include <string>

#include "psarc.hpp"

int PackPSArc(std::string& input, std::string& output) {
  std::filesystem::path inputPath(input);

  if (!std::filesystem::is_directory(inputPath)) {
    std::cout << "Input path is not a directory" << std::endl;
    return -1;
  }

  PSArc::PSArcHandle handle;

  std::filesystem::path outputPath(output);
  PSArc::FileHandle outputHandle(outputPath, true);

  if (!outputHandle.IsValid()) {
    std::cout << "Failed to create file: " << output << std::endl;
    return -1;
  }

  handle.SetSerializationEndpoint(&outputHandle);

  PSArc::Archive archive;
  handle.SetArchive(&archive);

  std::filesystem::recursive_directory_iterator fileIterator(inputPath);

  // end of iterator is defined as a default constructed one
  static std::filesystem::recursive_directory_iterator end;

  size_t currentFileNumber = 0;

  while (fileIterator != end) {
    std::filesystem::path filePath = fileIterator->path();
    ++fileIterator;

    if (std::filesystem::is_directory(filePath))
      continue;

    PSArc::FileHandle fileHandle(filePath);

    if (fileHandle.IsValid()) {
      std::cout << "\r\e[K[" << currentFileNumber << "] " << filePath.generic_string();

      std::uintmax_t fileSize = std::filesystem::file_size(filePath);

      std::filesystem::path relativeFilePath = std::filesystem::relative(filePath, inputPath);

      std::vector<byte> bytes(fileSize);
      fileHandle.Read(bytes.data(), fileSize);

      PSArc::File file(relativeFilePath.generic_string(), bytes);
      archive.AddFile(file);
    }
    else {
      std::cout << "\r\e[K"
                << "Failed to read file: " << filePath.generic_string() << std::endl;
    }

    currentFileNumber++;
  }

  PSArc::PSArcSettings settings;
  settings.endianness = std::endian::big;

  std::cout << "\r\e[KPacking files into: " << outputPath.generic_string() << std::endl;
  handle.Downsync(settings, [currentFileNumber](size_t numFilesPacked, std::string name) -> void {
    std::cout << "\r\e[K[" << numFilesPacked << "/" << currentFileNumber << "] " << name;
  });

  return 0;
}
