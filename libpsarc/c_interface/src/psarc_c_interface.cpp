#include "psarc.h"
#include "psarc.hpp"

enum LibPSArcType { LIBPSARC_TYPE_ARCHIVE, LIBPSARC_TYPE_PSARCHANDLE, LIBPSARC_TYPE_FILEHANDLE };

struct LibPSArcArchive {
  LibPSArcType type;
  PSArc::Archive archive;
};

struct LibPSArcPSArcHandle {
  LibPSArcType type;
  PSArc::PSArcHandle handle;
};

struct LibPSArcFileHandle {
  LibPSArcType type;
  PSArc::FileHandle handle;
};
