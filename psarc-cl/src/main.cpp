#include <algorithm>
#include <iostream>

#include "pack.hpp"
#include "psarc.hpp"
#include "unpack.hpp"

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cout << "Usage:" << std::endl;
    std::cout << "[pack,unpack]" << std::endl;
    std::cout << "Input path" << std::endl;
    std::cout << "Output path" << std::endl;
    return -1;
  }

  std::string modeString(argv[1]);
  std::string inputString(argv[2]);
  std::string outputString(argv[3]);

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

  /*
    PSArc::FileHandle test("Test.psarc");
    PSArc::PSArcHandle handleOut;

    handleOut.SetSerializationEndpoint(&test);
    handleOut.SetArchive(&archive);
    handleOut.Downsync();
  */
}
