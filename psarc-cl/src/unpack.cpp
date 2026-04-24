#include <fstream>
#include <iostream>
#include <string>

#include "psarc.hpp"

#define RESET_LINE "\r\033[K"

static const char* compressionTypeToString(PSArc::CompressionType ct) {
  switch (ct) {
    case PSArc::CompressionType::PSARC_COMPRESSION_TYPE_ZLIB:
      return "zlib";
    case PSArc::CompressionType::PSARC_COMPRESSION_TYPE_LZMA:
      return "lzma";
    default:
      return "none";
  }
}

static const char* pathTypeToString(PSArc::PathType pt) {
  switch (pt) {
    case PSArc::PathType::PSARC_PATH_TYPE_IGNORECASE:
      return "ignorecase";
    case PSArc::PathType::PSARC_PATH_TYPE_ABSOLUTE:
      return "absolute";
    default:
      return "relative";
  }
}

static const char* endiannessToString(std::endian e) {
  if (e == std::endian::big)
    return "big";
  if (e == std::endian::little)
    return "little";
  return "native";
}

int UnpackPSArc(std::string& input, std::string& output) {
  PSArc::PSArcHandle handle;
  PSArc::FileHandle inputFileHandle(input);

  if (!inputFileHandle.IsValid()) {
    std::cout << "Failed to open file: " << input << std::endl;
    return -1;
  }

  PSArc::Archive archive;

  handle.SetParsingEndpoint(&inputFileHandle);
  handle.SetArchive(&archive);

  PSArc::PSArcStatus upsyncStatus = handle.Upsync();

  if (upsyncStatus != PSArc::PSARC_STATUS_OK) {
    std::cout << "Failed to synchronize with source archive." << std::endl;
    std::cout << "Error: " << PSArc::PSArcStatusToString(upsyncStatus) << std::endl;
    return -1;
  }

  std::filesystem::path outputPath(output);

  if (outputPath.generic_string().back() != '/')
    outputPath += '/';

  if (!std::filesystem::is_directory(outputPath) && !std::filesystem::create_directory(outputPath)) {
    std::cout << "Output path \"" << outputPath << "\" is not a directory and failed to create it." << std::endl;
    return -1;
  }

  // Write archive settings so pack can reproduce the original format
  {
    std::filesystem::path settingsPath = outputPath / ".psarc-cl-settings";
    std::ofstream settingsFile(settingsPath);
    if (settingsFile.is_open()) {
      settingsFile << "compressionType=" << compressionTypeToString(handle.compressionType) << "\n";
      settingsFile << "blockSize=" << handle.blockSize << "\n";
      settingsFile << "pathType=" << pathTypeToString(handle.pathType) << "\n";
      settingsFile << "endianness=" << endiannessToString(handle.endianness) << "\n";
    }
    else {
      std::cout << "Warning: could not write .psarc-cl-settings to: " << std::filesystem::absolute(settingsPath).generic_string()
                << std::endl;
    }
  }

  size_t fileCount         = archive.GetFileCount();
  size_t currentFileNumber = 0;

  std::for_each(archive.begin(), archive.end(), [outputPath, fileCount, &currentFileNumber](PSArc::File* file) {
    std::filesystem::path fileOutputPath(outputPath);
    fileOutputPath += file->path.relative_path();

    PSArc::FileHandle fileOutputHandle(fileOutputPath, true);

    if (fileOutputHandle.IsValid()) {
      std::cout << RESET_LINE "[" << currentFileNumber << "/" << fileCount << "] " << file->path.generic_string();

      fileOutputHandle.Write(file->GetUncompressedBytes()->data(), file->GetUncompressedSize());
    }
    else {
      std::cout << RESET_LINE << "Failed to write file " << file->path.generic_string() << std::endl;
    }

    currentFileNumber++;
  });

  std::cout << RESET_LINE "[" << fileCount << "/" << fileCount << "] Done." << std::endl;

  return 0;
}
