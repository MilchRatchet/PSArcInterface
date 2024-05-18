#include "psarc_error.hpp"

const char* PSArc::PSArcStatusToString(PSArc::PSArcStatus status) {
  switch (status) {
    case PSARC_STATUS_OK:
      return "Success";
    case PSARC_STATUS_ERROR_COMPRESSION:
      return "Error during compression occurred";
    case PSARC_STATUS_ERROR_DECOMPRESSION:
      return "Error during decompression occurred";
    case PSARC_STATUS_ERROR_MANIFEST:
      return "Manifest file caused error";
    case PSARC_STATUS_ERROR_ENDPOINT:
      return "Error due to missing or invalid endpoint";
    case PSARC_STATUS_ERROR_HEADER:
      return "Error due to invalid archive header";
    case PSARC_STATUS_ERROR_INSERT:
      return "Failed to insert file into archive";
    case PSARC_STATUS_ERROR_DSAR_FILE:
      return "Archive is contained in a DSAR file which is not supported";
    case PSARC_STATUS_ERROR_MISC:
      return "Miscellaneous error occurred";
    default:
      return "Unknown status code";
  }
}
