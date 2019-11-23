#ifndef _COMMON_H
#define _COMMON_H 1

#include <stdio.h>
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

#ifdef CC_PRINT
# if CC_PRINT_LEVEL >= 2
#  define CC_INFO(format, ...) \
	fprintf(stdout, "info: %s:%s():%d: " format, \
			__FILE__, __func__, __LINE__, ##__VA_ARGS__)
#  define CC_WARNING(format, ...) \
	fprintf(stdout, "warning: %s:%s():%d: " format, \
			__FILE__, __func__, __LINE__, ##__VA_ARGS__)
#  define CC_ERROR(format, ...) \
	fprintf(stderr, "error: %s:%s():%d: " format, \
			__FILE__, __func__, __LINE__, ##__VA_ARGS__)
# else
#  define CC_INFO(format, ...) fprintf(stdout, format, ##__VA_ARGS__)
#  define CC_WARNING(format, ...) fprintf(stdout, format, ##__VA_ARGS__)
#  define CC_ERROR(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
# endif /* CC_PRINT_LEVEL >= 2 */
#else
# define CC_INFO(format, ...)
# define CC_WARNING(format, ...)
# define CC_ERROR(format, ...)
#endif /* CC_PRINT */

#endif
