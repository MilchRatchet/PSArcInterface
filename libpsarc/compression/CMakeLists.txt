# LZMA
file(GLOB lzma_source "${CMAKE_CURRENT_SOURCE_DIR}/lzma/*.c")
target_sources(psarc PUBLIC "${lzma_source}")

target_include_directories(psarc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lzma")

# ZLIB
file(GLOB zlib_source "${CMAKE_CURRENT_SOURCE_DIR}/zlib/*.c")
target_sources(psarc PUBLIC "${zlib_source}")

target_include_directories(psarc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/zlib")

