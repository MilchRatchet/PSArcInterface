cmake_minimum_required(VERSION 3.26)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)

# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Do not install gtest alongside the main project
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# Unit tests (libpsarc only, no CLI dependency)
# ---------------------------------------------------------------------------
add_executable(psarc-unit-tests
  unit/test_types.cpp
  unit/test_compression.cpp
  unit/test_archive.cpp
)

target_link_libraries(psarc-unit-tests PRIVATE
  LibPSArc::Static
  GTest::gtest_main
)

target_include_directories(psarc-unit-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/libpsarc/include
)

# ---------------------------------------------------------------------------
# Integration tests (round-trip via library + CLI process tests)
# ---------------------------------------------------------------------------
add_executable(psarc-integration-tests
  integration/test_roundtrip.cpp
  integration/test_cli.cpp
)

target_link_libraries(psarc-integration-tests PRIVATE
  LibPSArc::Static
  GTest::gtest_main
)

target_include_directories(psarc-integration-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/libpsarc/include
)

# Pass the path to the psarc-cl binary and the fixtures directory as compile-time
# constants so that integration tests can locate them without hard-coding paths.
target_compile_definitions(psarc-integration-tests PRIVATE
  PSARC_CLI_BINARY_PATH="$<TARGET_FILE:psarc-cl>"
  PSARC_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

add_dependencies(psarc-integration-tests psarc-cl)

# ---------------------------------------------------------------------------
# CTest registration
# ---------------------------------------------------------------------------
include(GoogleTest)
gtest_discover_tests(psarc-unit-tests DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(psarc-integration-tests DISCOVERY_MODE PRE_TEST)
