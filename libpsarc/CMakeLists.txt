file(GLOB libpsarc_source "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(psarc STATIC ${libpsarc_source})
target_include_directories(psarc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(compression)
