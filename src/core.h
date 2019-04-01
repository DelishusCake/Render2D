#ifndef CORE_H
#define CORE_H

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <math.h>
#include <float.h>
#include <string.h>

#include <xmmintrin.h>

#if DEBUG
#include <assert.h>
#else
#define assert(...)
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

#define kilobytes(n)	((n)<<10)
#define megabytes(n)	((n)<<20)
#define gigabytes(n)	((n)<<30)

#define U8_MAX		(0xFF)
#define U16_MAX		(0xFFFF)
#define U32_MAX		(0xFFFFFFFF)
#define U64_MAX		(0xFFFFFFFFFFFFFFFF)

#define PI_32		(3.14159f)

#define radians(f)	((f) * (PI_32 / 180.f))
#define degrees(f)	((f) * (180.f / PI_32))

#define sign(v)	(((v) < 0) ? -1 : (((v) > 0) ? 1 : 0))

#define static_len(a) (sizeof(a) / sizeof((a)[0]))

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#define clamp(v, l, h)	max(l, min(v, h))

#define swap(TYPE, A, B) { TYPE tmp = (A); (A) = (B); (B) = tmp; }

#define decl_struct(NAME)			struct NAME; typedef struct NAME NAME;
#define member_size(TYPE, MEMBER)	sizeof(((TYPE *)0)->MEMBER)

// Heap index calculations
#define heap_parent(i)	((i - 1) >> 1)
#define heap_left(i)	((i << 1) + 1)
#define heap_right(i)	((i << 1) + 2)

// Bit operations
// Sets the i'th bit in v to 1
#define bit_set(v, i)	((v) |= (1 << (i)))
// Sets the i'th bit in v to 0
#define bit_clear(v, i)	((v) &= ~(1 << (i)))

// Floating point functions
inline f32 f32_abs(f32 v)         { return fabsf(v); };
inline f32 f32_sqrt(f32 v)        { return sqrtf(v); };
inline f32 f32_pow(f32 v, f32 p)  { return powf(v, p); };
inline f32 f32_isqrt(f32 v)       { return (1.f / sqrtf(v)); };

inline f32 f32_sin(f32 v)         { return sinf(v); }
inline f32 f32_cos(f32 v)         { return cosf(v); }
inline f32 f32_atan(f32 v)        { return atanf(v); }

// Atomic operations
inline u32 atomic_inc(volatile u32 *value)
{
	return __sync_fetch_and_add(value, 1);
};
inline u32 atomic_dec(volatile u32 *value)
{
	return __sync_fetch_and_add(value, -1);
};

#endif