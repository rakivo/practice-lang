#ifndef COMMON_H_
#define COMMON_H_

#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

// #define PRINT_TOKENS
// #define PRINT_ASTS
// #define PRINT_STACK
// #define MEM_PRINT 1

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t  i8;

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

#define MAIN_FUNCTION "main"

#define DECLARE_STATIC(lower, upper) \
	lower##_t upper##S[upper##S_CAP]; \
	lower##_id_t upper##S_SIZE = 0 \

void
main_deinit(void);

// Report error, release memory and exit
#define report_error(fmt, ...) do { \
	report_error_noexit(__func__, __FILE__, __LINE__, fmt, __VA_ARGS__); \
	main_deinit(); \
	exit(EXIT_FAILURE); \
} while (0)

/* ------------------------------------------------------- */

// Copyright (c) 2019 Christoffer Lerno. All rights reserved.
// Use of this source code is governed by the GNU LGPLv3.0 license
// a copy of which can be found in the LICENSE file.

/* ------------------------------------------------------- */

#define NO_ARENA 0
#define MAX_VECTOR_WIDTH 65536

#define MAX_ARRAY_SIZE INT64_MAX
#define MAX_SOURCE_LOCATION_LEN 255
#define PROJECT_JSON "project.json"
#define PROJECT_JSON5 "project.json5"

#if defined( _WIN32 ) || defined( __WIN32__ ) || defined( _WIN64 )
#define PLATFORM_WINDOWS 1
#define PLATFORM_POSIX 0
#else
#define PLATFORM_WINDOWS 0
#define PLATFORM_POSIX 1
#endif

#ifndef USE_PTHREAD
#if PLATFORM_POSIX
#define USE_PTHREAD 1
#else
#define USE_PTHREAD 0
#endif
#endif

#if CURL_FOUND || PLATFORM_WINDOWS
#define FETCH_AVAILABLE 1
#else
#define FETCH_AVAILABLE 0
#endif

#define IS_GCC 0
#define IS_CLANG 0
#ifdef __GNUC__
#ifdef __clang__
#undef IS_CLANG
#define IS_CLANG 1
#else
#undef IS_GCC
#define IS_GCC 1
#endif
#endif

#ifndef __printflike
#define __printflike(x, y)
#endif

#ifndef __unused
#define __unused
#endif

#define UNUSED_VAR(var) ((void) (var))

#if (defined(__GNUC__) && __GNUC__ >= 7) || defined(__clang__)
	#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
	#define UNUSED __attribute__((unused))
	#define NORETURN __attribute__((noreturn))
	#define INLINE __attribute__((always_inline)) static inline
#elif defined(_MSC_VER)
	#define INLINE static __forceinline
	#define NORETURN __declspec(noreturn)
	#define UNUSED
	#define PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#else
	#define PACK(__Declaration__) __Declaration__
	#define INLINE static inline
	#define FALLTHROUGH ((void)0 )
	#define UNUSED
	#define NORETURN
#endif

#define INFO_LOG(_string, ...) \
	do {                          \
	if (!debug_log) break; \
	printf("-- INFO: "); printf(_string, ##__VA_ARGS__); printf("\n"); \
	} while (0)
#ifdef NDEBUG
#define REMINDER(_string, ...) do {} while (0)
#define DEBUG_LOG(_string, ...) do {} while (0)
#else
#define REMINDER(_string, ...) do { if (!debug_log) break; printf("TODO: %s -> in %s @ %s:%d\n", _string, __func__, __FILE__, __LINE__ , ##__VA_ARGS__); } while(0)
#define DEBUG_LOG(_string, ...) \
	do {                          \
	if (!debug_log) break; \
	printf("-- DEBUG: "); printf(_string, ##__VA_ARGS__); printf("\n"); \
	} while (0)
#endif

#define FATAL_ERROR(_string, ...) do { error_exit("FATAL ERROR %s -> in %s @ in %s:%d ", _string, __func__, __FILE__, __LINE__, ##__VA_ARGS__); } while(0)

#define ASSERT(_condition, _string, ...) while (!(_condition)) { FATAL_ERROR(_string, ##__VA_ARGS__); }
#define WARNING(_string, ...) do { eprintf("WARNING: "); eprintf(_string, ##__VA_ARGS__); eprintf("\n"); } while(0)
#define UNREACHABLE FATAL_ERROR("Should be unreachable");

#define unlikely(x) (__builtin_expect(!!(x), 0))

#define TODO FATAL_ERROR("TODO reached");
#define UNSUPPORTED do { error_exit("Unsupported functionality"); } while (0)

#define TEST_ASSERT(condition_, string_) while (!(condition_)) { FATAL_ERROR(string_); }
#define TEST_ASSERTF(condition_, string_, ...) while (!(condition_)) { char* str_ = str_printf(string_, __VA_ARGS__); FATAL_ERROR(str_); }

#define EXPECT(_string, _value, _expected) do { long long __tempval1 = _value; long long __tempval2 = _expected; TEST_ASSERT(__tempval1 == __tempval2, "Checking " _string ": expected %lld but was %lld.", __tempval2, __tempval1); } while (0)

void eprintf(const char *format, ...);
void evprintf(const char *format, va_list list);
void report_error_noexit(const char *func, const char *file, const size_t line, const char *format, ...);
NORETURN void report_error_(const char *func, const char *file, const size_t line, const char *format, ...);
NORETURN void error_exit(const char *format, ...) ;

#endif // COMMON_H_
