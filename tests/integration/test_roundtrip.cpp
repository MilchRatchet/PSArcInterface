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

TEST(RoundTrip, EmptyFilePreserved) {
  std::vector<byte> content;  // zero bytes

  Archive source;
  source.AddFile(File("empty.bin", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("empty.bin");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_TRUE(bytes->empty());
}

TEST(RoundTrip, LargeFileZlib) {
  // 1 MiB of patterned data to stress multi-block paths.
  std::vector<byte> content(1024 * 1024);
  for (size_t i = 0; i < content.size(); ++i)
    content[i] = static_cast<byte>(i % 251);  // prime modulus for variety

  Archive source;
  source.AddFile(File("large.bin", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;
  settings.blockSize       = 65536;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("large.bin");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}

TEST(RoundTrip, LargeFileLzma) {
  std::vector<byte> content(1024 * 1024);
  for (size_t i = 0; i < content.size(); ++i)
    content[i] = static_cast<byte>(i % 251);

  Archive source;
  source.AddFile(File("large.bin", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;
  settings.blockSize       = 65536;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("large.bin");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}

TEST(RoundTrip, FileCountPreservedAcrossRoundTrip) {
  Archive source;
  for (int i = 0; i < 20; ++i)
    source.AddFile(File("file_" + std::to_string(i) + ".txt", MakeBytes("content " + std::to_string(i))));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;

  Archive result = RoundTrip(source, settings);
  EXPECT_EQ(result.GetFileCount(), 20u);
}

TEST(RoundTrip, FilesWithSubdirectoryPaths) {
  std::vector<byte> contentA = MakeBytes("deep file A");
  std::vector<byte> contentB = MakeBytes("deep file B");

  Archive source;
  source.AddFile(File("dir/subdir/a.txt", contentA));
  source.AddFile(File("dir/subdir/b.txt", contentB));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;

  Archive result = RoundTrip(source, settings);

  File* fa = result.FindFile("dir/subdir/a.txt");
  File* fb = result.FindFile("dir/subdir/b.txt");
  ASSERT_NE(fa, nullptr);
  ASSERT_NE(fb, nullptr);
  EXPECT_EQ(*fa->GetUncompressedBytes(), contentA);
  EXPECT_EQ(*fb->GetUncompressedBytes(), contentB);
}

TEST(RoundTrip, AllZeroBytesFile) {
  std::vector<byte> content(4096, static_cast<byte>(0x00));

  Archive source;
  source.AddFile(File("zeros.bin", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("zeros.bin");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}

TEST(RoundTrip, MixedCompressionTypes) {
  // Pack with LZMA; read back and verify every file regardless of codec.
  Archive source;
  source.AddFile(File("alpha.txt", MakeBytes("alpha content")));
  source.AddFile(File("beta.txt", MakeBytes("beta content")));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;

  Archive result = RoundTrip(source, settings);
  EXPECT_EQ(result.GetFileCount(), 2u);
  EXPECT_EQ(*result.FindFile("alpha.txt")->GetUncompressedBytes(), MakeBytes("alpha content"));
  EXPECT_EQ(*result.FindFile("beta.txt")->GetUncompressedBytes(), MakeBytes("beta content"));
}

TEST(RoundTrip, SmallBlockSizeLzma) {
  std::vector<byte> content(8192);
  for (size_t i = 0; i < content.size(); ++i)
    content[i] = static_cast<byte>(i % 64);

  Archive source;
  source.AddFile(File("multi_block_lzma.bin", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;
  settings.blockSize       = 1024;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("multi_block_lzma.bin");
  ASSERT_NE(f, nullptr);
  auto bytes = f->GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, content);
}

TEST(RoundTrip, FileNamesArePreserved) {
  // Verify that the exact file name (including extension) survives a round-trip.
  const char* name          = "unusual.name.with.dots.bin";
  std::vector<byte> content = MakeBytes("content");

  Archive source;
  source.AddFile(File(name, content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;

  Archive result = RoundTrip(source, settings);
  EXPECT_NE(result.FindFile(name), nullptr);
}

TEST(RoundTrip, IdenticalContentsInDifferentFiles) {
  // Two files with identical content should both round-trip independently.
  std::vector<byte> content = MakeBytes("same content");

  Archive source;
  source.AddFile(File("copy1.txt", content));
  source.AddFile(File("copy2.txt", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_ZLIB;

  Archive result = RoundTrip(source, settings);

  File* f1 = result.FindFile("copy1.txt");
  File* f2 = result.FindFile("copy2.txt");
  ASSERT_NE(f1, nullptr);
  ASSERT_NE(f2, nullptr);
  EXPECT_EQ(*f1->GetUncompressedBytes(), content);
  EXPECT_EQ(*f2->GetUncompressedBytes(), content);
}

TEST(RoundTrip, BinaryDataNoCompression) {
  // Random-like binary data stored without compression should survive unchanged.
  std::vector<byte> content(512);
  uint32_t state = 0xABCD1234u;
  for (size_t i = 0; i < content.size(); ++i) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    content[i] = static_cast<byte>(state & 0xFF);
  }

  Archive source;
  source.AddFile(File("random.bin", content));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_NONE;

  Archive result = RoundTrip(source, settings);

  File* f = result.FindFile("random.bin");
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(*f->GetUncompressedBytes(), content);
}

// ---------------------------------------------------------------------------
// Regression: TOC header fields must include the manifest entry
//
// Previously GetFileCount() excluded the manifest, causing Downsync to write
// tocEntriesCount and tocLength one short, producing a malformed archive.
// This test reads those fields directly from the serialized bytes and verifies
// they account for all files including the manifest.
// ---------------------------------------------------------------------------

TEST(RoundTrip, TocHeaderIncludesManifestEntry) {
  const size_t kNumFiles = 3;

  Archive source;
  for (size_t i = 0; i < kNumFiles; ++i)
    source.AddFile(File("file_" + std::to_string(i) + ".txt", MakeBytes("content " + std::to_string(i))));

  PSArcSettings settings;
  settings.compressionType = CompressionType::PSARC_COMPRESSION_TYPE_LZMA;
  settings.endianness      = std::endian::little;  // native; makes scalar reading trivial

  VectorOutputHandle output;
  PSArcHandle writer;
  writer.SetArchive(&source);
  writer.SetSerializationEndpoint(&output);
  ASSERT_EQ(writer.Downsync(settings), PSArcStatus::PSARC_STATUS_OK);

  const std::vector<byte>& raw = output.data;
  ASSERT_GE(raw.size(), 0x20u) << "Serialized archive is shorter than the minimum header size";

  // PSArc header layout (all uint32_t, little-endian here):
  //   0x00  magic "PSAR"
  //   0x04  versionMajor (uint16)
  //   0x06  versionMinor (uint16)
  //   0x08  compressionType (4 bytes)
  //   0x0C  tocLength  — header(0x20) + tocEntrySize*tocEntries + blockTable
  //   0x10  tocEntrySize
  //   0x14  tocEntriesCount  ← must be kNumFiles + 1 (manifest)
  //   0x18  blockSize
  //   0x1C  pathType

  uint32_t tocEntriesCount;
  std::memcpy(&tocEntriesCount, raw.data() + 0x14, sizeof(uint32_t));
  EXPECT_EQ(tocEntriesCount, kNumFiles + 1u) << "tocEntriesCount in header does not include the manifest entry";

  uint32_t tocLength;
  std::memcpy(&tocLength, raw.data() + 0x0C, sizeof(uint32_t));

  uint32_t tocEntrySize;
  std::memcpy(&tocEntrySize, raw.data() + 0x10, sizeof(uint32_t));

  // Minimum tocLength must accommodate the header + all TOC entries.
  EXPECT_GE(tocLength, 0x20u + tocEntrySize * tocEntriesCount) << "tocLength in header is too small to hold all TOC entries";

  // The archive must also be parseable — a well-formed tocLength means
  // Upsync can locate file data correctly.
  VectorInputHandle input(raw);
  Archive result;
  PSArcHandle reader;
  reader.SetParsingEndpoint(&input);
  reader.SetArchive(&result);
  ASSERT_EQ(reader.Upsync(), PSArcStatus::PSARC_STATUS_OK);
  EXPECT_EQ(result.GetFileCount(), kNumFiles);
}
