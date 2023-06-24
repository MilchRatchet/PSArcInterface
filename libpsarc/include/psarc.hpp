#pragma once

#include <cstdint>
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
  Archive* archiveEndpoint;
  InputMemoryHandle* parsingEndpoint;
  OutputMemoryHandle* serializationEndpoint;

public:
  PSArcHandle();
  void SetParsingEndpoint(InputMemoryHandle*);
  void SetSerializationEndpoint(OutputMemoryHandle*);
  void SetArchive(Archive*);
  bool Upsync() override;
  bool Downsync() override;
};

class PSArcFile : public FileSourceProvider {
private:
  size_t uncompressedSize;
  PSArcHandle& psarcHandle;

public:
  PSArcFile(PSArcHandle& psarcHandle);
  std::vector<byte> GetBytes() override;
  CompressionType GetCompressionType() override;
};

}  // namespace PSArc
