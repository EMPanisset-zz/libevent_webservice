#ifndef _TIGERA_LOGGER__H__
#define _TIGERA_LOGGER__H__

#if defined(DEBUG)

// Choose output system for errors
#define _LOG(msg, ...)   fprintf(stderr, msg, ## __VA_ARGS__)
#define _ERROR(msg, ...) fprintf(stderr,  msg, ## __VA_ARGS__)

// Generic log macros
#   define LOG(str, ...) \
        do { \
            struct timeval tv; \
            evutil_gettimeofday(&tv, NULL); \
            _LOG("[LOG] %ld.%06ld %s: "str"\n", \
                (long)tv.tv_sec, (long)tv.tv_usec, __func__, ## __VA_ARGS__); \
        } while (0)

#   define ERROR(str, ...) \
        do { \
            struct timeval tv; \
            evutil_gettimeofday(&tv, NULL); \
            _ERROR("[ERROR] %ld.%06ld %s: "str"\n", \
                (long)tv.tv_sec, (long)tv.tv_usec, __func__, ## __VA_ARGS__); \
        } while (0)

#else
# define LOG(...)
# define ERROR(...)
#endif

#endif /*_TIGERA_LOGGER__H__ */
