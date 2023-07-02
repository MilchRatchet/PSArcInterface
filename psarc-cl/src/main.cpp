#include <algorithm>
#include <iostream>

#include "memory.hpp"
#include "psarc.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "No path to psarc file was given!" << std::endl;
    return -1;
  }

  PSArc::PSArcHandle handle;
  PSArc::FileHandle file((std::string(argv[1])));
  PSArc::Archive archive;

  handle.SetParsingEndpoint(&file);
  handle.SetArchive(&archive);
  handle.Upsync();

  std::for_each(archive.begin(), archive.end(), [](PSArc::File& file) { std::cout << file.path.generic_string() << std::endl; });

  return 0;
}
