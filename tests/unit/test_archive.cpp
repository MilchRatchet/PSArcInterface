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

// ---------------------------------------------------------------------------
// File — path formatting
// ---------------------------------------------------------------------------

TEST(File, GetPathStringIgnoreCase) {
  File f("Dir/Sub/File.Txt", MakeBytes("data"));
  EXPECT_FALSE(f.GetPathString(PathType::PSARC_PATH_TYPE_IGNORECASE).empty());
}

TEST(File, GetPathStringAbsolute) {
  File f("/absolute/path/file.txt", MakeBytes("data"));
  EXPECT_FALSE(f.GetPathString(PathType::PSARC_PATH_TYPE_ABSOLUTE).empty());
}

// ---------------------------------------------------------------------------
// File — compressed/uncompressed size queries
// ---------------------------------------------------------------------------

TEST(File, UncompressedSizeMatchesDataLength) {
  std::vector<byte> data = MakeBytes("twelve bytes");
  File f("size_test.txt", data);
  EXPECT_EQ(f.GetUncompressedSize(), data.size());
}

TEST(File, IsUncompressedSizeAvailableAfterConstruction) {
  File f("avail.txt", MakeBytes("available"));
  EXPECT_TRUE(f.IsUncompressedSizeAvailable());
}

TEST(File, CompressedSizeAvailableAfterCompress) {
  std::vector<byte> data(2048);
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<byte>(i % 32);
  File f("compress_size.bin", data);
  f.Compress(CompressionType::PSARC_COMPRESSION_TYPE_ZLIB, 65536);
  EXPECT_TRUE(f.IsCompressedSizeAvailable());
  EXPECT_GT(f.GetCompressedSize(), 0u);
}

TEST(File, CompressedSizeSmallerThanUncompressedForCompressibleData) {
  std::vector<byte> data(4096, static_cast<byte>(0xAA));  // highly compressible
  File f("compressible.bin", data);
  f.Compress(CompressionType::PSARC_COMPRESSION_TYPE_ZLIB, 65536);
  EXPECT_LT(f.GetCompressedSize(), data.size());
}

// ---------------------------------------------------------------------------
// File — clear state helpers
// ---------------------------------------------------------------------------

TEST(File, ClearCompressedBytesDoesNotAffectUncompressed) {
  std::vector<byte> data = MakeBytes("clear test");
  File f("clear.txt", data);
  f.Compress(CompressionType::PSARC_COMPRESSION_TYPE_ZLIB, 65536);
  f.ClearCompressedBytes();
  // Uncompressed bytes should still be accessible.
  auto bytes = f.GetUncompressedBytes();
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(*bytes, data);
}

// ---------------------------------------------------------------------------
// Archive — large number of files
// ---------------------------------------------------------------------------

TEST(Archive, AddManyFilesAndCountThem) {
  Archive archive;
  const size_t N = 100;
  for (size_t i = 0; i < N; ++i)
    archive.AddFile(File("file_" + std::to_string(i) + ".txt", MakeBytes("x")));
  EXPECT_EQ(archive.GetFileCount(), N);
}

TEST(Archive, IteratorCountMatchesGetFileCount) {
  Archive archive;
  for (int i = 0; i < 10; ++i)
    archive.AddFile(File("f" + std::to_string(i) + ".txt", MakeBytes("data")));

  size_t itCount = 0;
  for (auto it = archive.begin(); *it != nullptr; ++it)
    ++itCount;

  EXPECT_EQ(itCount, archive.GetFileCount());
}

// ---------------------------------------------------------------------------
// File — manifest detection
// ---------------------------------------------------------------------------

TEST(File, ManifestFileIsDetected) {
  // The known manifest filename should be recognised as a manifest.
  File f("PSArcManifest.bin", MakeBytes("manifest data"));
  EXPECT_TRUE(f.IsManifest());
}

TEST(File, CaseVariantIsNotManifest) {
  // Filename differing only in case must not be treated as the manifest.
  File f("psarcmanifest.bin", MakeBytes("data"));
  EXPECT_FALSE(f.IsManifest());
}

// ---------------------------------------------------------------------------
// File — GetPathString content checks
// ---------------------------------------------------------------------------

TEST(File, RelativePathHasNoLeadingSlash) {
  File f("some/path/file.txt", MakeBytes("data"));
  std::string p = f.GetPathString(PathType::PSARC_PATH_TYPE_RELATIVE);
  EXPECT_FALSE(p.empty());
  EXPECT_NE(p.front(), '/');
}

TEST(File, AbsolutePathHasLeadingSlash) {
  File f("some/path/file.txt", MakeBytes("data"));
  std::string p = f.GetPathString(PathType::PSARC_PATH_TYPE_ABSOLUTE);
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(p.front(), '/');
}

TEST(File, RelativeAndAbsolutePathsDifferBySlash) {
  File f("a/b/c.txt", MakeBytes("data"));
  std::string rel = f.GetPathString(PathType::PSARC_PATH_TYPE_RELATIVE);
  std::string abs = f.GetPathString(PathType::PSARC_PATH_TYPE_ABSOLUTE);
  // Absolute should be the relative path with a '/' prepended.
  EXPECT_EQ(abs, '/' + rel);
}

// ---------------------------------------------------------------------------
// File — ClearUncompressedBytes / reload
// ---------------------------------------------------------------------------

TEST(File, ClearUncompressedBytesCompressedStillAvailable) {
  std::vector<byte> data(1024);
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<byte>(i % 64);

  File f("clear_unc.bin", data);
  f.Compress(CompressionType::PSARC_COMPRESSION_TYPE_ZLIB, 65536);
  f.ClearUncompressedBytes();
  // Compressed bytes should still be accessible after clearing uncompressed.
  auto compressed = f.GetCompressedBytes();
  ASSERT_NE(compressed, nullptr);
  EXPECT_FALSE(compressed->empty());
}

// ---------------------------------------------------------------------------
// File — GetCompressedBlockSizes
// ---------------------------------------------------------------------------

TEST(File, BlockSizesNonEmptyAfterCompress) {
  std::vector<byte> data(4096);
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<byte>(i % 32);

  File f("blocks.bin", data);
  f.Compress(CompressionType::PSARC_COMPRESSION_TYPE_ZLIB, 1024);
  EXPECT_FALSE(f.GetCompressedBlockSizes().empty());
}

TEST(File, BlockCountMatchesExpectedBlocks) {
  // 4 KiB data with 1 KiB block size → 4 blocks.
  std::vector<byte> data(4 * 1024);
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<byte>(i % 32);

  File f("block_count.bin", data);
  f.Compress(CompressionType::PSARC_COMPRESSION_TYPE_ZLIB, 1024);
  EXPECT_EQ(f.GetCompressedBlockSizes().size(), 4u);
}

// ---------------------------------------------------------------------------
// Archive — FindFile with path type
// ---------------------------------------------------------------------------

TEST(Archive, FindFileByAbsolutePath) {
  Archive archive;
  archive.AddFile(File("dir/file.txt", MakeBytes("data")));
  EXPECT_NE(archive.FindFile("/dir/file.txt", PathType::PSARC_PATH_TYPE_ABSOLUTE), nullptr);
}

TEST(Archive, FindFileAbsoluteReturnsSameFile) {
  Archive archive;
  std::vector<byte> expected = MakeBytes("find me");
  archive.AddFile(File("lookup.txt", expected));

  File* rel = archive.FindFile("lookup.txt", PathType::PSARC_PATH_TYPE_RELATIVE);
  File* abs = archive.FindFile("/lookup.txt", PathType::PSARC_PATH_TYPE_ABSOLUTE);
  ASSERT_NE(rel, nullptr);
  ASSERT_NE(abs, nullptr);
  EXPECT_EQ(rel, abs);  // both pointers point to the same File object
}
