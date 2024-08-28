#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cassert>

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t  i8;

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

/* ----------------------------- */

static_assert(sizeof(u64) == 8, "unexpected sizeof(u64)");
static_assert(sizeof(u32) == 4, "unexpected sizeof(u32)");
static_assert(sizeof(u16) == 2, "unexpected sizeof(u16)");
static_assert(sizeof(u8)  == 1, "unexpected sizeof(u8)");

/* ----------------------------- */

typedef std::vector<std::string> strs_t;
typedef std::vector<strs_t> strs2D_t;

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#define DECLARE_EXTERN(lower, upper) \
  const u64 upper##S_CAP = 1024; \
  extern lower##_t upper##S[upper##S_CAP]; \
  extern lower##_id_t upper##S_SIZE; \

#define DECLARE_STATIC(lower, upper) \
  lower##_t upper##S[upper##S_CAP]; \
  lower##_id_t upper##S_SIZE = 0; \

#define ASSERT(c_) assert(c_)
#define TODO(what) ASSERT(0 && "TODO: " what)
#define UNREACHABLE __builtin_unreachable()

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
