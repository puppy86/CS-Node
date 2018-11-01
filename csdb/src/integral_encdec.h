#pragma once
#ifndef _CREDITS_CSDB_PRIVATE_INTEGRAL_ENCDEC_H_INCLUDED_
#define _CREDITS_CSDB_PRIVATE_INTEGRAL_ENCDEC_H_INCLUDED_

#include <cinttypes>
#include <type_traits>

namespace csdb {
namespace priv {
enum {
  MAX_INTEGRAL_ENCODED_SIZE = sizeof(uint64_t) + 1,
};

//Compact coding of integer type variable
template<typename T>
typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value)
                        && (sizeof(T) <= sizeof(uint64_t)), std::size_t>::type
encode(void *buf, T value)
{
  return encode<uint64_t>(buf, static_cast<uint64_t>(value));
}

//Integer type decoding
template<typename T>
typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value)
                         && (sizeof(T) <= sizeof(uint64_t)), std::size_t>::type
decode(const void *buf, std::size_t size, T& value)
{
  uint64_t v;
  std::size_t res = decode<uint64_t>(buf, size, v);
  if (0 != res) {
    value = static_cast<T>(v);
  }
  return res;
}

template<>
std::size_t encode(void *buf, bool value);

template<>
std::size_t encode(void *buf, uint64_t value);

template<>
std::size_t decode(const void *buf, std::size_t size, bool& value);

template<>
std::size_t decode(const void *buf, std::size_t size, uint64_t& value);

} // namespace priv
} // namespace csdb

#endif // _CREDITS_CSDB_PRIVATE_INTEGRAL_ENCDEC_H_INCLUDED_
