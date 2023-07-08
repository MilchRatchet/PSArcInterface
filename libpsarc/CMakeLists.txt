# This file sets up the libpsarc build and install process.
# The code is based on the SDL 3 build process (https://github.com/libsdl-org/SDL).

file(GLOB LIBPSARC_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(LIBPSARC_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(GLOB LIBPSARC_INCLUDE_FILES "${LIBPSARC_INCLUDE}/*.hpp")

add_subdirectory(compression)

include(GNUInstallDirs)

# setup headers target
add_library(Headers INTERFACE)
add_library(LibPSArc::Headers ALIAS Headers)
set_target_properties(Headers PROPERTIES EXPORT_NAME "Headers")
target_include_directories(Headers INTERFACE "$<BUILD_INTERFACE:${LIBPSARC_INCLUDE}>")
target_include_directories(Headers INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/LibPSArc>")

# setup shared target
add_library(Shared SHARED ${LIBPSARC_SOURCE} ${LZMA_SOURCE} ${ZLIB_SOURCE})
add_library(LibPSArc::Shared ALIAS Shared)
set_target_properties(Shared PROPERTIES
  OUTPUT_NAME "psarc"
  POSITION_INDEPENDENT_CODE TRUE
)
target_include_directories(Shared PRIVATE
  "$<BUILD_INTERFACE:${LIBPSARC_INCLUDE}>"
  "$<BUILD_INTERFACE:${LZMA_INCLUDE}>"
  "$<BUILD_INTERFACE:${ZLIB_INCLUDE}>"
)
target_link_libraries(Shared PUBLIC $<TARGET_NAME:LibPSArc::Headers>)

# setup static target
add_library(Static STATIC ${LIBPSARC_SOURCE} ${LZMA_SOURCE} ${ZLIB_SOURCE})
add_library(LibPSArc::Static ALIAS Static)
set_target_properties(Static PROPERTIES
  OUTPUT_NAME "psarc"
  POSITION_INDEPENDENT_CODE TRUE
)
target_include_directories(Static PRIVATE
  "$<BUILD_INTERFACE:${LIBPSARC_INCLUDE}>"
  "$<BUILD_INTERFACE:${LZMA_INCLUDE}>"
  "$<BUILD_INTERFACE:${ZLIB_INCLUDE}>"
)
target_link_libraries(Static PUBLIC $<TARGET_NAME:LibPSArc::Headers>)

# install
set(LIBPSARC_INSTALL_CMAKEDIR_ROOT "${CMAKE_INSTALL_DATADIR}/cmake" CACHE STRING "Root folder where to install LibPSArcConfig.cmake related files")

set(LIBPSARC_INSTALL_CMAKEDIR "${LIBPSARC_INSTALL_CMAKEDIR_ROOT}/libpsarc")
set(LIBPSARC_INSTALL_HEADERSDIR "${CMAKE_INSTALL_INCLUDEDIR}/libpsarc")
set(LIBPSARC_INSTALL_RESOURCEDIR ".")

export(TARGETS Headers NAMESPACE "LibPSArc::" FILE "LibPSArcHeadersTargets.cmake")
export(TARGETS Shared NAMESPACE "LibPSArc::" FILE "LibPSArcSharedTargets.cmake")
export(TARGETS Static NAMESPACE "LibPSArc::" FILE "LibPSArcStaticTargets.cmake")

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/LibPSArcConfig.cmake.in LibPSArcConfig.cmake
  PATH_VARS CMAKE_INSTALL_PREFIX
  INSTALL_DESTINATION ${LIBPSARC_INSTALL_CMAKEDIR})
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/LibPSArcConfigVersion.cmake" COMPATIBILITY AnyNewerVersion)

install(TARGETS Headers EXPORT LibPSArcHeadersTargets)

install(TARGETS Shared EXPORT LibPSArcSharedTargets
  PUBLIC_HEADER DESTINATION "${LIBPSARC_INSTALL_HEADERSDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  RESOURCE DESTINATION "${LIBPSARC_INSTALL_RESOURCEDIR}"
)

install(TARGETS Static EXPORT LibPSArcStaticTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RESOURCE DESTINATION "${LIBPSARC_INSTALL_RESOURCEDIR}"
)

install(EXPORT LibPSArcHeadersTargets
FILE "LibPSArcHeadersTargets.cmake"
NAMESPACE LibPSArc::
DESTINATION "${LIBPSARC_INSTALL_CMAKEDIR}"
)

install(EXPORT LibPSArcSharedTargets
  FILE "LibPSArcSharedTargets.cmake"
  NAMESPACE LibPSArc::
  DESTINATION "${LIBPSARC_INSTALL_CMAKEDIR}"
)

install(EXPORT LibPSArcStaticTargets
  FILE "LibPSArcStaticTargets.cmake"
  NAMESPACE LibPSArc::
  DESTINATION "${LIBPSARC_INSTALL_CMAKEDIR}"
)

install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/LibPSArcConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/LibPSArcConfigVersion.cmake
    DESTINATION "${LIBPSARC_INSTALL_CMAKEDIR}"
  )

install(FILES ${LIBPSARC_INCLUDE_FILES}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LibPSArc"
)



