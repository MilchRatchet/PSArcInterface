#include <iostream>
#include <string>

#include "psarc.hpp"

int UnpackPSArc(std::string& input, std::string& output) {
  PSArc::PSArcHandle handle;
  PSArc::FileHandle file(input);

  if (!file.IsValid()) {
    std::cout << "Failed to open file: " << input << std::endl;
    return -1;
  }

  PSArc::Archive archive;

  handle.SetParsingEndpoint(&file);
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

    std::filesystem::path dirOutputPath(fileOutputPath);
    dirOutputPath.remove_filename();

    std::filesystem::create_directories(dirOutputPath);

    std::fstream fileStream(fileOutputPath, std::ios_base::openmode::_S_out | std::ios_base::openmode::_S_bin);

    if (!fileStream.fail()) {
      std::cout << "\r\e[K[" << currentFileNumber << "/" << fileCount << "] " << file->path.generic_string();

      fileStream.write(reinterpret_cast<const char*>(file->GetUncompressedBytes()), file->GetUncompressedSize());
    }
    else {
      std::cout << "Failed to write file " << file->path.generic_string() << std::endl;
    }

    currentFileNumber++;
  });

  std::cout << std::endl;

  return 0;
}
