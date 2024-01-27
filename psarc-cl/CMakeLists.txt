file(GLOB psarc_source "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

set(GIT_COMMIT_DATE "Unknown")
set(GIT_BRANCH_NAME "Unknown")
set(GIT_COMMIT_HASH "Unknown")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE GIT_COMMIT_DATE)

    string (REGEX REPLACE "\n" ""  GIT_COMMIT_DATE ${GIT_COMMIT_DATE})

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE GIT_BRANCH_NAME)

    string (REGEX REPLACE "\n" ""  GIT_BRANCH_NAME ${GIT_BRANCH_NAME})

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE GIT_COMMIT_HASH)

    string (REGEX REPLACE "\n" ""  GIT_COMMIT_HASH ${GIT_COMMIT_HASH})
else()
    message(SEND_ERROR "Failed to find git.")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp")

add_executable(psarc-cl ${psarc_source})
target_link_libraries(psarc-cl LibPSArc::Static)
target_include_directories(psarc-cl PRIVATE "$<TARGET_PROPERTY:LibPSArc::Static,INTERFACE_INCLUDE_DIRECTORIES>")
