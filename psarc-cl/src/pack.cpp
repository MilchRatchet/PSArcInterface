#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "psarc.hpp"

#define RESET_LINE "\r\033[K"

static const std::string k_SettingsFileName = ".psarc-cl-settings";

static PSArc::PSArcSettings LoadSettingsFromFile(const std::filesystem::path& settingsPath) {
  PSArc::PSArcSettings settings;

  std::ifstream file(settingsPath);
  if (!file.is_open())
    return settings;

  std::string line;
  while (std::getline(file, line)) {
    auto sep = line.find('=');
    if (sep == std::string::npos)
      continue;

    std::string key   = line.substr(0, sep);
    std::string value = line.substr(sep + 1);

    if (key == "compressionType") {
      if (value == "zlib")
        settings.compressionType = PSArc::CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;
      else if (value == "lzma")
        settings.compressionType = PSArc::CompressionType::PSARC_COMPRESSION_TYPE_LZMA;
      else
        settings.compressionType = PSArc::CompressionType::PSARC_COMPRESSION_TYPE_NONE;
    }
    else if (key == "blockSize") {
      bool valid = !value.empty();
      for (char c : value) {
        if (c < '0' || c > '9') {
          valid = false;
          break;
        }
      }
      if (valid)
        settings.blockSize = static_cast<uint32_t>(std::stoul(value));
    }
    else if (key == "pathType") {
      if (value == "ignorecase")
        settings.pathType = PSArc::PathType::PSARC_PATH_TYPE_IGNORECASE;
      else if (value == "absolute")
        settings.pathType = PSArc::PathType::PSARC_PATH_TYPE_ABSOLUTE;
      else
        settings.pathType = PSArc::PathType::PSARC_PATH_TYPE_RELATIVE;
    }
    else if (key == "endianness") {
      if (value == "big")
        settings.endianness = std::endian::big;
      else if (value == "little")
        settings.endianness = std::endian::little;
      else
        settings.endianness = std::endian::native;
    }
  }

  return settings;
}

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

  std::filesystem::path settingsPath = inputPath / k_SettingsFileName;
  PSArc::PSArcSettings settings      = LoadSettingsFromFile(settingsPath);

  std::filesystem::recursive_directory_iterator fileIterator(inputPath);

  // end of iterator is defined as a default constructed one
  static std::filesystem::recursive_directory_iterator end;

  size_t currentFileNumber = 0;

  while (fileIterator != end) {
    std::filesystem::path filePath = fileIterator->path();
    ++fileIterator;

    if (std::filesystem::is_directory(filePath))
      continue;

    if (filePath.filename() == k_SettingsFileName)
      continue;

    PSArc::FileHandle fileHandle(filePath);

    if (fileHandle.IsValid()) {
      std::cout << RESET_LINE "[" << currentFileNumber << "] " << filePath.generic_string();

      std::uintmax_t fileSize = std::filesystem::file_size(filePath);

      std::filesystem::path relativeFilePath = std::filesystem::relative(filePath, inputPath);

      std::vector<byte> bytes(fileSize);
      fileHandle.Read(bytes.data(), fileSize);

      PSArc::File file(relativeFilePath.generic_string(), bytes);
      archive.AddFile(file);
    }
    else {
      std::cout << RESET_LINE << "Failed to read file: " << filePath.generic_string() << std::endl;
    }

    currentFileNumber++;
  }

  std::cout << RESET_LINE "Packing files into: " << outputPath.generic_string() << std::endl;
  PSArc::PSArcStatus status = handle.Downsync(settings, [currentFileNumber](size_t numFilesPacked, std::string name) -> void {
    std::stringstream msg;
    msg << RESET_LINE "[" << numFilesPacked << "/" << currentFileNumber << "] " << name;
    std::cout << msg.str();
  });

  if (status == PSArc::PSARC_STATUS_OK) {
    std::cout << RESET_LINE "Done" << std::endl;
  }
  else {
    std::cout << RESET_LINE "Failed to pack archive." << std::endl;
  }

  return 0;
}
