#ifndef __SERVER_ASSERT_H
#define __SERVER_ASSERT_H

/* This file shouldn't have any dependecies to any other Tinyhttpd2 code. */

#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define tinyhttpd_unreachable __builtin_unreachable
#else
#include <stdlib.h>
#define tinyhttpd_unreachable abort
#endif

#if __GNUC__ >= 3
#define likely(x) __builtin_expect(!!(x), 1)
#else
#define likely(x) (x)
#endif

#ifdef assert
#undef assert
#endif

void _serverAssert(const char *estr, const char *file, int line);
void _serverPanic(const char *file, int line, const char *msg, ...);

#define assert(_e) (likely((_e)) ? (void) 0 : (_serverAssert(#_e, __FILE__, __LINE__), tinyhttpd_unreachable()))
#define panic(...) _serverPanic(__FILE__, __LINE__, __VA_ARGS__), tinyhttpd_unreachable()

#ifndef static_assert
#define static_assert _Static_assert
#endif

#endif