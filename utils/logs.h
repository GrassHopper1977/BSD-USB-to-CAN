#ifndef __LOGS_H__
#define __LOGS_H__

/// @brief Works like printf, outputs to stdout. Gives a log time in nanoseconds and allows a source and message type.
/// @param source This is normally the filename
/// @param type Free text. Normally the type of message to aid in sorting.
/// @param format Printf style formatter
/// @param VARGS Any additional variables required by the format.
extern void LOGI(const char* source, const char* type, const char *format, ...);

/// @brief Works like printf except outputs to stderr. Gives a log time in nanoseconds and allows a source and message type.
/// @param source This is normally the filename
/// @param type Free text. Normally the type of message to aid in sorting.
/// @param format Printf style formatter
/// @param VARGS Any additional variables required by the format.
extern void LOGE(const char* source, const char* type, const char *format, ...);

#endif // __LOGS_H__