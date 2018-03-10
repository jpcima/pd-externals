#include "../byte_order"

namespace jsl {

#if defined(__GNUC__)
# define JSL_ORDER_LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
# define JSL_ORDER_BIG_ENDIAN __ORDER_BIG_ENDIAN__
#else
# define JSL_ORDER_LITTLE_ENDIAN 1234
# define JSL_ORDER_BIG_ENDIAN 4321
#endif

#if defined(__GNUC__)
# define JSL_BYTE_ORDER __BYTE_ORDER__
#elif defined(_MSC_VER) &&                                              \
    (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_X64) ||        \
     defined(_M_ARM) || defined(_M_ARM64))
# define JSL_BYTE_ORDER JSL_ORDER_LITTLE_ENDIAN
#endif

#if defined(__GNUC__)
inline constexpr uint16_t byteswap16(uint16_t x)
    { return __builtin_bswap16(x); }
inline constexpr uint32_t byteswap32(uint32_t x)
    { return __builtin_bswap32(x); }
inline constexpr uint64_t byteswap64(uint64_t x)
    { return __builtin_bswap64(x); }
#else
inline constexpr uint16_t byteswap16(uint16_t x)
    { return (x >> 8) | (x << 8); }
inline constexpr uint32_t byteswap32(uint32_t x)
    { return ((uint32_t)byteswap16(x) << 16) | ((uint32_t)byteswap16(x >> 16)); }
inline constexpr uint64_t byteswap64(uint64_t x)
    { return ((uint64_t)byteswap32(x) << 32) | ((uint64_t)byteswap32(x >> 32)); }
#endif

#if JSL_BYTE_ORDER == JSL_ORDER_LITTLE_ENDIAN
inline constexpr uint16_t int_of_le16(uint16_t x) { return x; }
inline constexpr uint32_t int_of_le32(uint32_t x) { return x; }
inline constexpr uint64_t int_of_le64(uint64_t x) { return x; }
inline constexpr uint16_t int_of_be16(uint16_t x) { return byteswap16(x); }
inline constexpr uint32_t int_of_be32(uint32_t x) { return byteswap32(x); }
inline constexpr uint64_t int_of_be64(uint64_t x) { return byteswap64(x); }
inline constexpr uint16_t le16_of_int(uint16_t x) { return x; }
inline constexpr uint32_t le32_of_int(uint32_t x) { return x; }
inline constexpr uint64_t le64_of_int(uint64_t x) { return x; }
inline constexpr uint16_t be16_of_int(uint16_t x) { return byteswap16(x); }
inline constexpr uint32_t be32_of_int(uint32_t x) { return byteswap32(x); }
inline constexpr uint64_t be64_of_int(uint64_t x) { return byteswap64(x); }
#elif JSL_BYTE_ORDER == JSL_ORDER_BIG_ENDIAN
inline constexpr uint16_t int_of_le16(uint16_t x) { return byteswap16(x); }
inline constexpr uint32_t int_of_le32(uint32_t x) { return byteswap32(x); }
inline constexpr uint64_t int_of_le64(uint64_t x) { return byteswap64(x); }
inline constexpr uint16_t int_of_be16(uint16_t x) { return x; }
inline constexpr uint32_t int_of_be32(uint32_t x) { return x; }
inline constexpr uint64_t int_of_be64(uint64_t x) { return x; }
inline constexpr uint16_t le16_of_int(uint16_t x) { return byteswap16(x); }
inline constexpr uint32_t le32_of_int(uint32_t x) { return byteswap32(x); }
inline constexpr uint64_t le64_of_int(uint64_t x) { return byteswap64(x); }
inline constexpr uint16_t be16_of_int(uint16_t x) { return x; }
inline constexpr uint32_t be32_of_int(uint32_t x) { return x; }
inline constexpr uint64_t be64_of_int(uint64_t x) { return x; }
#endif

}  // namespace jsl
