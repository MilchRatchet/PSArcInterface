#include <iostream>
#include <string>

#include "psarc.hpp"

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
  handle.Upsync();

  std::filesystem::path outputPath(output);

  if (!std::filesystem::is_directory(outputPath)) {
    std::cout << "Output path is not a directory" << std::endl;
    return -1;
  }

  size_t fileCount         = archive.GetFileCount();
  size_t currentFileNumber = 0;

  std::for_each(archive.begin(), archive.end(), [outputPath, fileCount, &currentFileNumber](PSArc::File* file) {
    std::filesystem::path fileOutputPath(outputPath);

    fileOutputPath += file->path;

    PSArc::FileHandle fileOutputHandle(fileOutputPath, true);

    if (fileOutputHandle.IsValid()) {
      std::cout << "\r\e[K[" << currentFileNumber << "/" << fileCount << "] " << file->path.generic_string();

      fileOutputHandle.Write(file->GetUncompressedBytes(), file->GetUncompressedSize());
    }
    else {
      std::cout << "\r\e[K"
                << "Failed to write file " << file->path.generic_string() << std::endl;
    }

    currentFileNumber++;
  });

  std::cout << std::endl;

  return 0;
}
