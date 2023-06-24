file(GLOB psarc_source "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(psarc-cl ${psarc_source})
target_link_libraries(psarc-cl psarc)
target_include_directories(psarc-cl PRIVATE "$<TARGET_PROPERTY:psarc,INTERFACE_INCLUDE_DIRECTORIES>")
file(GENERATE OUTPUT test.txt CONTENT "$<TARGET_PROPERTY:psarc,INTERFACE_INCLUDE_DIRECTORIES>")
