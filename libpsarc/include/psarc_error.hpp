#pragma once

namespace PSArc {

enum PSArcStatus {
  PSARC_STATUS_OK                  = 0,
  PSARC_STATUS_ERROR_COMPRESSION   = 1,
  PSARC_STATUS_ERROR_DECOMPRESSION = 2,
  PSARC_STATUS_ERROR_MANIFEST      = 3,
  PSARC_STATUS_ERROR_ENDPOINT      = 4,
  PSARC_STATUS_ERROR_HEADER        = 5,
  PSARC_STATUS_ERROR_INSERT        = 6,
  PSARC_STATUS_ERROR_DSAR_FILE     = 7,
  PSARC_STATUS_ERROR_MISC          = 255
};

const char* PSArcStatusToString(PSArcStatus status);

};  // namespace PSArc
