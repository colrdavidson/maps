#ifndef COMMON_H
#define COMMON_H

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#endif
