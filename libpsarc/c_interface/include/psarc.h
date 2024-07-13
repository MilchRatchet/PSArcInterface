#ifndef LIBPSARC_H
#define LIBPSARC_H

#include <stdint.h>

typedef void* libpsarc_archive;
typedef void* libpsarc_psarchandle;
typedef void* libpsarc_filehandle;
typedef void* libpsarc_file;

enum LibPSArcCompressionType { LIBPSARC_COMPRESSION_TYPE_NONE = 0, LIBPSARC_COMPRESSION_TYPE_ZLIB = 1, LIBPSARC_COMPRESSION_TYPE_LZMA = 2 };

enum LibPSArcPathType { LIBPSARC_PATH_TYPE_RELATIVE = 0, LIBPSARC_PATH_TYPE_IGNORECASE = 1, LIBPSARC_PATH_TYPE_ABSOLUTE = 2 };

enum LibPSArcEndianness { LIBPSARC_ENDIANNESS_NATIVE = 0, LIBPSARC_ENDIANNESS_LITTLE = 1, LIBPSARC_ENDIANNESS_BIG = 2 };

struct libpsarc_settings {
  uint16_t version_major;
  uint16_t version_minor;
  enum LibPSArcCompressionType compressionType;
  uint32_t block_size;
  uint32_t toc_entry_size;
  enum LibPSArcPathType pathType;
  enum LibPSArcEndianness endianness;
};

int libpsarc_archive_init(libpsarc_archive* archive);
int libpsarc_archive_file_count(libpsarc_archive* archive, uint32_t* file_count);
int libpsarc_archive_get_file(libpsarc_archive* archive, uint32_t file_id, libpsarc_file* file);
int libpsarc_archive_clear(libpsarc_archive* archive);

int libpsarc_psarchandle_init(libpsarc_psarchandle* handle);
int libpsarc_psarchandle_clear(libpsarc_psarchandle* handle);
int libpsarc_psarchandle_set_archive(libpsarc_psarchandle* handle, libpsarc_archive* archive);
int libpsarc_psarchandle_set_parsing_endpoint(libpsarc_psarchandle* handle, libpsarc_filehandle* file);
int libpsarc_psarchandle_upsync(libpsarc_psarchandle* handle);
int libpsarc_psarchandle_downsync(libpsarc_psarchandle* handle, libpsarc_settings settings);

int libpsarc_filehandle_init(libpsarc_filehandle* handle);
int libpsarc_filehandle_open_file(libpsarc_filehandle* handle, const char* path);
bool libpsarc_filehandle_is_valid(libpsarc_filehandle* handle);
int libpsarc_filehandle_write(libpsarc_filehandle* handle, const void* ptr, size_t size);
int libpsarc_filehandle_read(libpsarc_filehandle* handle, void* buffer, size_t buffer_size);
int libpsarc_filehandle_clear(libpsarc_filehandle* handle);

int libpsarc_file_init(libpsarc_file* file);
int libpsarc_file_alloc(libpsarc_file* file, const char* relative_path, const void* ptr, size_t size);
int libpsarc_file_get_relative_path(libpsarc_file* file, char* buffer, size_t buffer_size);
int libpsarc_file_get_uncompressed_size(libpsarc_file* file, size_t* size);
int libpsarc_file_get_uncompressed_bytes(libpsarc_file* file, void* buffer, size_t buffer_size);
int libpsarc_file_clear(libpsarc_file* file);

#endif /* LIBPSARC_H */
