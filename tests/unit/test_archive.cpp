#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "psarc_archive.hpp"
#include "psarc_error.hpp"
#include "psarc_types.hpp"

using namespace PSArc;

namespace {

std::vector<byte> MakeBytes(std::string s) {
  return std::vector<byte>(s.begin(), s.end());
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Archive — basic file management
// ---------------------------------------------------------------------------

TEST(Archive, AddAndFindFile) {
  Archive archive;
  File f("hello.txt", MakeBytes("hello world"));
  ASSERT_TRUE(archive.AddFile(std::move(f)));
  EXPECT_NE(archive.FindFile("hello.txt"), nullptr);
}

TEST(Archive, FindMissingFileReturnsNull) {
  Archive archive;
  EXPECT_EQ(archive.FindFile("does_not_exist.txt"), nullptr);
}

TEST(Archive, GetFileCountEmpty) {
  Archive archive;
  EXPECT_EQ(archive.GetFileCount(), 0u);
}

TEST(Archive, GetFileCountAfterAdd) {
  Archive archive;
  archive.AddFile(File("a.txt", MakeBytes("aaa")));
  archive.AddFile(File("b.txt", MakeBytes("bbb")));
  EXPECT_EQ(archive.GetFileCount(), 2u);
}

TEST(Archive, DuplicatePathNotAdded) {
  Archive archive;
  ASSERT_TRUE(archive.AddFile(File("dup.txt", MakeBytes("first"))));
  // Adding the same path a second time should fail (returns false).
  EXPECT_FALSE(archive.AddFile(File("dup.txt", MakeBytes("second"))));
  EXPECT_EQ(archive.GetFileCount(), 1u);
}

// ---------------------------------------------------------------------------
// Archive — iterator
// ---------------------------------------------------------------------------

TEST(Archive, IteratorVisitsAllFiles) {
  Archive archive;
  archive.AddFile(File("a.txt", MakeBytes("aaa")));
  archive.AddFile(File("b.txt", MakeBytes("bbb")));
  archive.AddFile(File("c.txt", MakeBytes("ccc")));

  size_t count = 0;
  for (auto it = archive.begin(); *it != nullptr; ++it)
    ++count;

  EXPECT_EQ(count, 3u);
}

TEST(Archive, IteratorEmptyArchive) {
  Archive archive;
  auto it = archive.begin();
  EXPECT_EQ(*it, nullptr);
}

// ---------------------------------------------------------------------------
// File — content and state
// ---------------------------------------------------------------------------

TEST(File, GetUncompressedBytesMatchesInput) {
  std::vector<byte> data = MakeBytes("test content");
  File f("test.txt", data);
  auto bytes = f.GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, data);
}

TEST(File, CompressAndDecompressZlib) {
  std::vector<byte> data(4096);
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<byte>(i % 64);

  File f("data.bin", data);
  f.Compress(CompressionType::PSARC_COMPRESSION_TYPE_ZLIB, 65536);
  EXPECT_TRUE(f.IsCompressedSizeAvailable());

  f.Decompress();
  auto decompressed = f.GetUncompressedBytes();
  ASSERT_NE(decompressed, nullptr);
  EXPECT_EQ(*decompressed, data);
}

TEST(File, CompressAndDecompressLzma) {
  std::vector<byte> data(4096);
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<byte>(i % 64);

  File f("data.bin", data);
  f.Compress(CompressionType::PSARC_COMPRESSION_TYPE_LZMA, 65536);
  EXPECT_TRUE(f.IsCompressedSizeAvailable());

  f.Decompress();
  auto decompressed = f.GetUncompressedBytes();
  ASSERT_NE(decompressed, nullptr);
  EXPECT_EQ(*decompressed, data);
}

TEST(File, IsManifestDetectedByName) {
  // The manifest is identified by a known name. Verify a regular file is not a manifest.
  File f("regular.txt", MakeBytes("data"));
  EXPECT_FALSE(f.IsManifest());
}

TEST(File, PathStringRelative) {
  File f("subdir/file.txt", MakeBytes("data"));
  EXPECT_FALSE(f.GetPathString(PathType::PSARC_PATH_TYPE_RELATIVE).empty());
}

// ---------------------------------------------------------------------------
// PSArcStatus string conversion
// ---------------------------------------------------------------------------

TEST(PSArcStatus, OkHasNonEmptyString) {
  EXPECT_FALSE(std::string(PSArcStatusToString(PSArcStatus::PSARC_STATUS_OK)).empty());
}

TEST(PSArcStatus, AllCodesHaveNonEmptyString) {
  const PSArcStatus codes[] = {
    PSArcStatus::PSARC_STATUS_OK,
    PSArcStatus::PSARC_STATUS_ERROR_COMPRESSION,
    PSArcStatus::PSARC_STATUS_ERROR_DECOMPRESSION,
    PSArcStatus::PSARC_STATUS_ERROR_MANIFEST,
    PSArcStatus::PSARC_STATUS_ERROR_ENDPOINT,
    PSArcStatus::PSARC_STATUS_ERROR_HEADER,
    PSArcStatus::PSARC_STATUS_ERROR_INSERT,
    PSArcStatus::PSARC_STATUS_ERROR_DSAR_FILE,
    PSArcStatus::PSARC_STATUS_ERROR_MISC,
  };
  for (auto code : codes)
    EXPECT_FALSE(std::string(PSArcStatusToString(code)).empty()) << "Missing string for status " << static_cast<int>(code);
}
