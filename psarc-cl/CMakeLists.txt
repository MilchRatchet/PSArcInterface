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

    string(REGEX REPLACE "\n" ""  GIT_COMMIT_DATE "${GIT_COMMIT_DATE}")

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE GIT_BRANCH_NAME)

    string(REGEX REPLACE "\n" ""  GIT_BRANCH_NAME "${GIT_BRANCH_NAME}")

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE GIT_COMMIT_HASH)

    string(REGEX REPLACE "\n" ""  GIT_COMMIT_HASH "${GIT_COMMIT_HASH}")
else()
    message(SEND_ERROR "Failed to find git.")
endif()

if(DEFINED PSARC_CL_RELEASE_BUILD_VERSION)
  set(PSARC_CL_RELEASE_BUILD ON)
else()
  unset(PSARC_CL_RELEASE_BUILD)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp")

add_executable(psarc-cl ${psarc_source})
target_link_libraries(psarc-cl LibPSArc::Static)
target_include_directories(psarc-cl PRIVATE "$<TARGET_PROPERTY:LibPSArc::Static,INTERFACE_INCLUDE_DIRECTORIES>")

# Determine the platform-specific CI output name
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(PSARC_CL_CI_BUILD_OUTPUT_NAME "psarc-cl-win64")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(PSARC_CL_CI_BUILD_OUTPUT_NAME "psarc-cl-macos")
else()
  set(PSARC_CL_CI_BUILD_OUTPUT_NAME "psarc-cl-linux")
endif()

# CI binaries are named based on platform; clean up the stale alternative on each build
if(PSARCINTERFACE_CI_BUILD)
  set_target_properties(psarc-cl PROPERTIES OUTPUT_NAME "${PSARC_CL_CI_BUILD_OUTPUT_NAME}")
  set(PSARC_CL_STALE_NAME "psarc-cl")
else()
  set(PSARC_CL_STALE_NAME "${PSARC_CL_CI_BUILD_OUTPUT_NAME}")
endif()

add_custom_command(TARGET psarc-cl POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E remove -f "$<TARGET_FILE_DIR:psarc-cl>/${PSARC_CL_STALE_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
  COMMENT "Cleaning up stale executable: ${PSARC_CL_STALE_NAME}"
)


