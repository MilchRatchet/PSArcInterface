#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "memory_handles.hpp"
#include "psarc_archive.hpp"
#include "psarc_error.hpp"
#include "psarc_impl.hpp"
#include "psarc_types.hpp"

using namespace PSArc;

namespace {

std::vector<byte> MakeBytes(std::string s) {
  return std::vector<byte>(s.begin(), s.end());
}

// Packs an archive to memory, then unpacks it and returns the reconstituted archive.
// Settings are forwarded to Downsync so tests can vary compression type, block size, etc.
Archive RoundTrip(Archive& source, PSArcSettings settings) {
  // --- Downsync: archive → bytes ---
  VectorOutputHandle output;
  PSArcHandle writer;
  writer.SetArchive(&source);
  writer.SetSerializationEndpoint(&output);
  PSArcStatus status = writer.Downsync(settings);
  EXPECT_EQ(status, PSArcStatus::PSARC_STATUS_OK) << PSArcStatusToString(status);

  // --- Upsync: bytes → archive ---
  VectorInputHandle input(output.data);
  Archive result;
  PSArcHandle reader;
  reader.SetParsingEndpoint(&input);
  reader.SetArchive(&result);
  status = reader.Upsync();
  EXPECT_EQ(status, PSArcStatus::PSARC_STATUS_OK) << PSArcStatusToString(status);

  return result;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Round-trip with LZMA compression (default)
// ---------------------------------------------------------------------------

TEST(RoundTrip, SingleFileLzma) {
  std::vector<byte> content = MakeBytes("Hello, PSArc round-trip test!");

  Archive source;
  source.AddFile(File("hello.txt", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("hello.txt");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}

TEST(RoundTrip, SingleFileZlib) {
  std::vector<byte> content = MakeBytes("Hello from ZLIB!");

  Archive source;
  source.AddFile(File("zlib.txt", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("zlib.txt");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}

TEST(RoundTrip, SingleFileNoCompression) {
  std::vector<byte> content = MakeBytes("Uncompressed data.");

  Archive source;
  source.AddFile(File("raw.txt", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_NONE;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("raw.txt");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}

TEST(RoundTrip, MultipleFiles) {
  std::vector<byte> contentA = MakeBytes("File A contents");
  std::vector<byte> contentB = MakeBytes("File B contents");
  std::vector<byte> contentC = MakeBytes("File C contents");

  Archive source;
  source.AddFile(File("a.txt", contentA));
  source.AddFile(File("b.txt", contentB));
  source.AddFile(File("c.txt", contentC));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;

  Archive result = RoundTrip(source, settings);
  EXPECT_EQ(result.GetFileCount(), 3u);

  struct {
    const char* name;
    const std::vector<byte>* expected;
  } checks[] = {
    {"a.txt", &contentA},
    {"b.txt", &contentB},
    {"c.txt", &contentC},
  };

  for (auto& check : checks) {
    File* f = result.FindFile(check.name);
    ASSERT_NE(f, nullptr) << "Missing: " << check.name;
    auto bytes = f->GetUncompressedBytes();
    ASSERT_NE(bytes, nullptr);
    EXPECT_EQ(*bytes, *check.expected) << "Content mismatch for: " << check.name;
  }
}

TEST(RoundTrip, BinaryData) {
  std::vector<byte> content(1024);
  for (size_t i = 0; i < content.size(); ++i)
    content[i] = static_cast<byte>(i & 0xFF);

  Archive source;
  source.AddFile(File("binary.bin", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("binary.bin");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}

TEST(RoundTrip, SmallBlockSize) {
  std::vector<byte> content(8192);
  for (size_t i = 0; i < content.size(); ++i)
    content[i] = static_cast<byte>(i % 64);

  Archive source;
  source.AddFile(File("multi_block.bin", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;
  settings.blockSize       = 1024;  // forces multiple blocks

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("multi_block.bin");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}
