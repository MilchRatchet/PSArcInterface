#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "archive.hpp"
#include "memory.hpp"

namespace PSArc {

/*
 * Interface of a virtual archive to a psarc file.
 */
class PSArcHandle : public ArchiveInterface {
private:
  bool hasEndpoint;
  std::optional<Archive&> archive;
  std::optional<InputMemoryHandle&> parsingEndpoint;
  std::optional<OutputMemoryHandle&> serializationEndpoint;

public:
  PSArcHandle();
  void SetParsingEndpoint(std::optional<InputMemoryHandle&>);
  void SetSerializationEndpoint(std::optional<OutputMemoryHandle&>);
  void SetSourceArchive(std::optional<OutputMemoryHandle&>);
}

}  // namespace PSArc
