#include "memory.hpp"
#include "psarc.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2)
    return -1;

  PSArc::PSArcHandle handle;
  PSArc::FileHandle file((std::string(argv[1])));
  PSArc::Archive archive;

  handle.SetParsingEndpoint(&file);
  handle.SetArchive(&archive);
  handle.Upsync();

  return 0;
}
