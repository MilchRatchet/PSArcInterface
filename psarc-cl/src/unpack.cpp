#include <iostream>
#include <string>

#include "psarc.hpp"

#define RESET_LINE "\r\033[K"

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
