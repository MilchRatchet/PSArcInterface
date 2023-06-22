#include "psarc.hpp"

PSArc::PSArcHandle::SetParsingEndpoint(std::optional<InputMemoryHandle&> memHandle) {
  if (memHandle.has_value()) {
    this->parsingEndpoint = memHandle;
    this->hasEndpoint     = true;
  }
  else {
    this->parsingEndpoint.reset();
    this->hasEndpoint = (this->serializationEndpoint.has_value());
  }
}

PSArc::PSArcHandle::SetSerializationEndpoint(std::optional<OutputMemoryHandle&> memHandle) {
  if (memHandle.has_value()) {
    this->serializationEndpoint = memHandle;
    this->hasEndpoint           = true;
  }
  else {
    this->serializationEndpoint.reset();
    this->hasEndpoint = (this->parsingEndpoint.has_value());
  }
}

PSArc::PSArcHandle::SetSourceArchive(std::optional<Archive&> archive) {
  this->archive = archive;
}
