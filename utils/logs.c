// logs.c
#include <stdio.h>
#include <stdarg.h>
#include "timestamp.h"

#define DEBUG_LOG_INFO
#define DEBUG_LOG_ERRORS


#define LOG_MIN_FILE_LEN  9
#define LOG_MAX_FILE_LEN  LOG_MIN_FILE_LEN
#define LOG_MIN_SOURCE_LEN  8
#define LOG_MAX_SOURCE_LEN  LOG_MIN_SOURCE_LEN
#define LOG_MIN_TYPE_LEN  5
#define LOG_MAX_TYPE_LEN  LOG_MIN_TYPE_LEN

/// @brief Works like printf, outputs to stdout. Gives a log time in nanoseconds and allows a source and message type.
/// @param source This is normally the filename
/// @param type Free text. Normally the type of message to aid in sorting.
/// @param format Printf style formatter
/// @param VARGS Any additional variables required by the format.
void LOGI(const char* source, const char* type, const char *format, ...) {
  #ifdef DEBUG_LOG_INFO
  va_list args;
  va_start(args, format);

  fprintf(stdout, "%16.16" PRIu64 ",  INFO: %*.*s, %*.*s, %*.*s, ", nanos(), LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type);
  vfprintf(stdout, format, args);
  va_end(args);
  #endif
}

/// @brief Works like printf except outputs to stderr. Gives a log time in nanoseconds and allows a source and message type.
/// @param source This is normally the filename
/// @param type Free text. Normally the type of message to aid in sorting.
/// @param format Printf style formatter
/// @param VARGS Any additional variables required by the format.
void LOGE(const char* source, const char* type, const char *format, ...) {
  #ifdef DEBUG_LOG_ERRORS
  va_list args;
  va_start(args, format);

  fprintf(stderr, "%16.16" PRIu64 ", ERROR: %*.*s, %*.*s, %*.*s, ", nanos(), LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type);
  vfprintf(stderr, format, args);
  va_end(args);
  #endif
}

