#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// These macros are injected by CMake (see tests/CMakeLists.txt).
#ifndef PSARC_CLI_BINARY_PATH
#error "PSARC_CLI_BINARY_PATH must be defined by the build system"
#endif
#ifndef PSARC_FIXTURES_DIR
#error "PSARC_FIXTURES_DIR must be defined by the build system"
#endif

namespace {

// Run a command and return its exit code.
int RunCommand(const std::string& cmd) {
  return std::system(cmd.c_str());
}

// Quote a path so it is safe to embed in a shell command string.
std::string QuotePath(const fs::path& p) {
  return "\"" + p.string() + "\"";
}

// Read an entire file into a string.
std::string ReadFile(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// Compare two directory trees file-by-file (content only, ignoring the manifest).
// Returns a human-readable error string, or empty string on success.
std::string CompareDirectories(const fs::path& expected, const fs::path& actual) {
  for (const auto& entry : fs::recursive_directory_iterator(expected)) {
    if (!entry.is_regular_file())
      continue;

    fs::path relative    = fs::relative(entry.path(), expected);
    fs::path counterpart = actual / relative;

    if (!fs::exists(counterpart))
      return "Missing file in output: " + relative.string();

    if (ReadFile(entry.path()) != ReadFile(counterpart))
      return "Content mismatch for: " + relative.string();
  }
  return {};
}

// RAII helper that removes a temporary directory on scope exit.
struct TempDir {
  fs::path path;

  explicit TempDir(std::string suffix) {
    path = fs::temp_directory_path() / ("psarc_test_" + suffix);
    fs::create_directories(path);
  }

  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);  // best-effort cleanup
  }

  TempDir(const TempDir&)            = delete;
  TempDir& operator=(const TempDir&) = delete;
};

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Pack → Unpack round-trip using the CLI binary
// ---------------------------------------------------------------------------

TEST(CliRoundTrip, FixturesPackAndUnpack) {
  const fs::path fixtures(PSARC_FIXTURES_DIR);
  const fs::path cli(PSARC_CLI_BINARY_PATH);

  ASSERT_TRUE(fs::exists(fixtures)) << "Fixtures dir not found: " << fixtures;
  ASSERT_TRUE(fs::exists(cli)) << "CLI binary not found: " << cli;

  TempDir workDir("roundtrip");
  fs::path archivePath = workDir.path / "test.psarc";
  fs::path unpackDir   = workDir.path / "unpacked";
  fs::create_directories(unpackDir);

  // Pack
  std::string packCmd = QuotePath(cli) + " pack " + QuotePath(fixtures) + " " + QuotePath(archivePath);
  ASSERT_EQ(RunCommand(packCmd), 0) << "Pack command failed: " << packCmd;
  ASSERT_TRUE(fs::exists(archivePath)) << "Archive was not created";

  // Unpack
  std::string unpackCmd = QuotePath(cli) + " unpack " + QuotePath(archivePath) + " " + QuotePath(unpackDir);
  ASSERT_EQ(RunCommand(unpackCmd), 0) << "Unpack command failed: " << unpackCmd;

  // Compare
  std::string diff = CompareDirectories(fixtures, unpackDir);
  EXPECT_TRUE(diff.empty()) << diff;
}

// ---------------------------------------------------------------------------
// Invalid argument handling
// ---------------------------------------------------------------------------

TEST(CliArguments, NoArgumentsReturnsNonZero) {
  const fs::path cli(PSARC_CLI_BINARY_PATH);
  ASSERT_TRUE(fs::exists(cli));
  std::string cmd = QuotePath(cli);
  EXPECT_NE(RunCommand(cmd), 0);
}

TEST(CliArguments, UnknownSubcommandReturnsNonZero) {
  const fs::path cli(PSARC_CLI_BINARY_PATH);
  ASSERT_TRUE(fs::exists(cli));
  std::string cmd = QuotePath(cli) + " frobinate input output";
  EXPECT_NE(RunCommand(cmd), 0);
}

TEST(CliArguments, MissingInputFileReturnsNonZero) {
  const fs::path cli(PSARC_CLI_BINARY_PATH);
  ASSERT_TRUE(fs::exists(cli));

  TempDir workDir("missing_input");
  fs::path nonexistent = workDir.path / "no_such_file.psarc";
  fs::path outDir      = workDir.path / "out";

  std::string cmd = QuotePath(cli) + " unpack " + QuotePath(nonexistent) + " " + QuotePath(outDir);
  EXPECT_NE(RunCommand(cmd), 0);
}
