#ifndef INTNS_IO_IOTYPES_HPP
#define INTNS_IO_IOTYPES_HPP

#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace intns::io {

enum class Endianness : uint8_t {
  kLittle = 0,  // Little-endian byte order
  kBig          // Big-endian byte order
};

inline uint16_t bswap_16(uint16_t x) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap16(x);
#elif defined(_MSC_VER)
  return _byteswap_ushort(x);
#else
  return (x << 8) | (x >> 8);
#endif
}

inline uint32_t bswap_32(uint32_t x) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap32(x);
#elif defined(_MSC_VER)
  return _byteswap_ulong(x);
#else
  return ((x << 24) & 0xFF000000U) | ((x << 8) & 0x00FF0000U) |
         ((x >> 8) & 0x0000FF00U) | ((x >> 24) & 0x000000FFU);
#endif
}

inline uint64_t bswap_64(uint64_t x) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap64(x);
#elif defined(_MSC_VER)
  return _byteswap_uint64(x);
#else
  return ((x << 56) & 0xFF00000000000000ULL) |
         ((x << 40) & 0x00FF000000000000ULL) |
         ((x << 24) & 0x0000FF0000000000ULL) |
         ((x << 8) & 0x000000FF00000000ULL) |
         ((x >> 8) & 0x00000000FF000000ULL) |
         ((x >> 24) & 0x0000000000FF0000ULL) |
         ((x >> 40) & 0x000000000000FF00ULL) |
         ((x >> 56) & 0x00000000000000FFULL);
#endif
}

}  // namespace intns::io

#endif
