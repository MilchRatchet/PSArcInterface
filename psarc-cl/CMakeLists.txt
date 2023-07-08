file(GLOB psarc_source "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(psarc-cl ${psarc_source})
target_link_libraries(psarc-cl LibPSArc::Static)
target_include_directories(psarc-cl PRIVATE "$<TARGET_PROPERTY:LibPSArc::Static,INTERFACE_INCLUDE_DIRECTORIES>")
