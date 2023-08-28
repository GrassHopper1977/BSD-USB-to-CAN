// #ifndef __LOGS_H__
// #define __LOGS_H__

// /// @brief Works like printf, outputs to stdout. Gives a log time in nanoseconds and allows a source and message type.
// /// @param source This is normally the filename
// /// @param type Free text. Normally the type of message to aid in sorting.
// /// @param format Printf style formatter
// /// @param VARGS Any additional variables required by the format.
// extern void LOGI(const char* source, const char* type, const char *format, ...);

// /// @brief Works like printf except outputs to stderr. Gives a log time in nanoseconds and allows a source and message type.
// /// @param source This is normally the filename
// /// @param type Free text. Normally the type of message to aid in sorting.
// /// @param format Printf style formatter
// /// @param VARGS Any additional variables required by the format.
// extern void LOGE(const char* source, const char* type, const char *format, ...);

// #endif // __LOGS_H__

// logs.h

#ifndef __LOGS_H__
#define __LOGS_H__
#include <stdio.h>      /* Standard input/output definitions */
#include <stdarg.h>     // Needed for variable argument handling __VA_ARGS__, etc.

#include "timestamp.h"

#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 2
#define LOG_LEVEL_WARN  1
#define LOG_LEVEL_ERROR 0

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_ERROR
#endif  // LOG_LEVEL

#define LOG_MIN_FILE_LEN  9
#define LOG_MAX_FILE_LEN  LOG_MIN_FILE_LEN
#define LOG_MIN_SOURCE_LEN  8
#define LOG_MAX_SOURCE_LEN  LOG_MIN_SOURCE_LEN
#define LOG_MIN_TYPE_LEN  5
#define LOG_MAX_TYPE_LEN  LOG_MIN_TYPE_LEN

#define LOCAL_LOG_LEVEL(level, source, type, format, ...) do {\
    if(level == LOG_LEVEL_ERROR)      { fprintf(stderr, "%16.16" PRIu64 ", ERROR: %*.*s, %*.*s, %*.*s, " format "", nanos(), LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type __VA_OPT__(,) __VA_ARGS__); } \
    else if(level == LOG_LEVEL_WARN)  { fprintf(stdout, "%16.16" PRIu64 ",  WARN: %*.*s, %*.*s, %*.*s, " format "", nanos(), LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type __VA_OPT__(,) __VA_ARGS__); } \
    else if(level == LOG_LEVEL_DEBUG) { fprintf(stdout, "%16.16" PRIu64 ", DEBUG: %*.*s, %*.*s, %*.*s, " format "", nanos(), LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type __VA_OPT__(,) __VA_ARGS__); } \
    else if(level == LOG_LEVEL_INFO)  { fprintf(stdout, "%16.16" PRIu64 ",  INFO: %*.*s, %*.*s, %*.*s, " format "", nanos(), LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type __VA_OPT__(,) __VA_ARGS__); } \
  } while (0)

#define LOG_LEVEL_LOCAL(level, source, type, format, ...) do {\
                if(LOG_LEVEL >= level) LOCAL_LOG_LEVEL(level, source, type, format __VA_OPT__(,) __VA_ARGS__); \
        } while (0)

/// @brief Works like printf, outputs to stdout. Gives a log time in nanoseconds and allows a source and message type.
/// @param source This is normally the filename
/// @param type Free text. Normally the type of message to aid in sorting.
/// @param format Printf style formatter
/// @param VARGS Any additional variables required by the format.
#define LOGI(source, type, format, ...)\
        LOG_LEVEL_LOCAL(LOG_LEVEL_INFO, source, type, format __VA_OPT__(,) __VA_ARGS__)

/// @brief Works like printf, outputs to stdout. Gives a log time in nanoseconds and allows a source and message type.
/// @param source This is normally the filename
/// @param type Free text. Normally the type of message to aid in sorting.
/// @param format Printf style formatter
/// @param VARGS Any additional variables required by the format.
#define LOGD(source, type, format, ...)\
        LOG_LEVEL_LOCAL(LOG_LEVEL_DEBUG, source, type, format __VA_OPT__(,) __VA_ARGS__)

/// @brief Works like printf, outputs to stdout. Gives a log time in nanoseconds and allows a source and message type.
/// @param source This is normally the filename
/// @param type Free text. Normally the type of message to aid in sorting.
/// @param format Printf style formatter
/// @param VARGS Any additional variables required by the format.
#define LOGW(source, type, format, ...)\
        LOG_LEVEL_LOCAL(LOG_LEVEL_WARN, source, type, format __VA_OPT__(,) __VA_ARGS__)

/// @brief Works like printf except outputs to stderr. Gives a log time in nanoseconds and allows a source and message type.
/// @param source This is normally the filename
/// @param type Free text. Normally the type of message to aid in sorting.
/// @param format Printf style formatter
/// @param VARGS Any additional variables required by the format.
#define LOGE(source, type, format, ...)\
        LOG_LEVEL_LOCAL(LOG_LEVEL_ERROR, source, type, format __VA_OPT__(,) __VA_ARGS__)

#endif // __LOG_H__