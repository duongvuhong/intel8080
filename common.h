#ifndef _COMMON_H
# define _COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
# define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
# define GCC_VERSION 0
#endif

#ifndef UNUSED
# if GCC_VERSION >= 20700
#  define UNUSED __attribute__((unused))
# else
#  define UNUSED
# endif
#endif

#if defined(TRUE) || defined(FALSE)
# undef TRUE
# undef FALSE
#endif
#define TRUE 1
#define FALSE !TRUE

#if defined(bool)
# undef bool
#endif
#define bool int

#if GCC_VERSION >= 30400
# define __must_check   __attribute__((warn_unused_result))
# define __malloc       __attribute__((__malloc))
#else
# define __must_check
# define __malloc
#endif

#define __init __attribute__((constructor))
#define __exit __attribute__((destructor))

/*
 * count number of elements of an array,
 * it should be a compile error if passing a pointer as an argument
 */
#define ARRAY_COUNT(x) ((sizeof (x) / sizeof (0[x])) \
                        / ((size_t) !(sizeof (x) % sizeof(0[x]))))

/* some useful string macros */
#define STRING_EQUAL(s1, s2)  (strcmp(s1, s2) == 0)
#define STRING_CONTAIN(s, ss) (strstr(s, ss) != NULL)
#define STRING_EMPTY(s1)      (strlen(s1) == 0)

#define RANDOM_BYTE ((uint8_t)(rand() % 256))

#ifdef CC_VERBOSE
#define CC_INFO(format, ...) \
    fprintf(stdout, "INFO: %s:%i:%s(): " format, \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__);

#define CC_WARNING(format, ...) \
    fprintf(stdout, "WARNING: %s:%i:%s(): " format, \
            __FILE__, __LINE__, __func__, ##__VA_ARGS__);

#define CC_ERROR(format, ...) \
    fprintf(stderr, "ERROR: %s:%i:%s(): " format, \
            __FILE__, __LINE__, __func__, ##__VA_ARGS__);
#else
#define CC_INFO(format, ...)
#define CC_WARNING(format, ...)
#define CC_ERROR(format, ...)
#endif

#define RETURN_WITH_ERRMSG_IF(exp, ret, format, ...) \
do {                                                 \
    if (exp) {                                       \
        CC_ERROR(format, ##__VA_ARGS__);             \
        return (ret);                                \
    }                                                \
} while (0)

#define ASSERT_WITH_ERRMSG_IF(exp, ret, format, ...) \
do {                                                 \
    if (exp) {                                       \
        CC_ERROR(format, ##__VA_ARGS__);             \
        exit (ret);                                  \
    }                                                \
} while (0)

#endif
