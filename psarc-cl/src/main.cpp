#include <algorithm>
#include <iostream>

#include "config.hpp"
#include "pack.hpp"
#include "psarc.hpp"
#include "unpack.hpp"

int main(int argc, char* argv[]) {
  std::cout << "PSArc-cl - " << PSARC_CL_VERSION_DATE << " (" << PSARC_CL_BRANCH_NAME << " " << PSARC_CL_VERSION_HASH << ")" << std::endl;
  std::cout << PSARC_CL_OS << " " << PSARC_CL_COMPILER << std::endl;

  if (argc != 4 && argc != 1) {
    std::cout << "OVERVIEW: psarc-cl PSArc Interfacing Commandline Executable" << std::endl;
    std::cout << std::endl;
#ifdef WIN32
    std::cout << "USAGE: psarc-cl.exe mode input-path output-path" << std::endl;
#else
    std::cout << "USAGE: psarc-cl mode input-path output-path" << std::endl;
#endif
    std::cout << std::endl;
    std::cout << "MODE:" << std::endl;
    std::cout << "  pack      Pack all files in a directory into a PSArc file." << std::endl;
    std::cout << "  unpack    Unpack all files in a PSArc file into a directory." << std::endl;
    return -1;
  }

  std::string modeString((argc == 1) ? "unpack" : argv[1]);
  std::string inputString((argc == 1) ? "./PS3arc.psarc" : argv[2]);
  std::string outputString((argc == 1) ? "./PSArcContent" : argv[3]);

  bool isPackMode   = modeString.compare("pack") == 0;
  bool isUnpackMode = modeString.compare("unpack") == 0;

  if (isPackMode) {
    return PackPSArc(inputString, outputString);
  }
  else if (isUnpackMode) {
    return UnpackPSArc(inputString, outputString);
  }
  else {
    std::cout << "No valid mode was specified (pack or unpack)" << std::endl;
    return -1;
  }
}
