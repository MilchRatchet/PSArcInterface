# MD5
file(GLOB _MD5_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/md5-c/*.c")
set(MD5_SOURCE ${_MD5_SOURCE} PARENT_SCOPE)
set(MD5_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/md5-c PARENT_SCOPE)
