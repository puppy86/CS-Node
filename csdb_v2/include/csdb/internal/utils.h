#pragma once
#ifndef _CREDITS_CSDB_UTILS_H_INCLUDED_
#define _CREDITS_CSDB_UTILS_H_INCLUDED_

#include "csdb/internal/types.h"

namespace csdb {
namespace internal {

template<typename Iterator>
byte_array from_hex(Iterator beg, Iterator end)
{
  auto digit_from_hex = [](char val, uint8_t &result)
  {
    val = toupper( val );

    if((val >= '0') && (val <= '9'))
      result = static_cast<uint8_t>(val - '0');
    else if((val >= 'A') && (val <= 'F'))
      result = static_cast<uint8_t>(val - 'A' + 10);
    else
      return false;

    return true;
  };

  byte_array res;
  res.reserve(std::distance( beg, end ) / 2);
  for(Iterator it = beg;;)
  {
    uint8_t hi, lo;

    if(it == end || !digit_from_hex(*it++, hi))
      break;

    if(it == end || !digit_from_hex(*it++, lo))
      break;

    res.push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return res;
}

template<typename Iterator>
std::string to_hex(Iterator beg, Iterator end)
{
  auto digit_to_hex = [](uint8_t val)
  {
      return static_cast<char>((val < 10) ? (val + '0') : (val - 10 + 'A'));
  };

  std::string res;
  res.reserve(std::distance(beg, end) * 2);
  for(Iterator it = beg; it != end; it++)
  {
      res.push_back( digit_to_hex((*it >> 4) & 0x0F) );
      res.push_back( digit_to_hex(*it & 0x0F) );
  }
  return res;
}

inline byte_array from_hex(const std::string &val)
{
  return from_hex(val.begin(), val.end());
}

inline std::string to_hex(const byte_array &val)
{
  return to_hex(val.begin(), val.end());
}
//Adds an end separator to the path
std::string path_add_separator(const std::string &path);

//The path to the folder for storing application data.
std::string app_data_path();

//file size
size_t file_size(const std::string &name);

//Function checks the existence of a file
inline bool file_exists(const std::string &path)
{
  return (static_cast<uint64_t>(-1) != file_size(path));
}

//Function deletes a file
bool file_remove(const std::string &path);

//Function checks the existence of a directory
bool dir_exists(const std::string &path);

//Function creates a directory
bool dir_make(const std::string &path);

//Delete directory
bool dir_remove(const std::string &path);


//Function creates the full path to the specified directory
bool path_make(const std::string &path);


//Function deletes the directory on the specified path
bool path_remove(const std::string &path);

//Folder Size
size_t path_size(const std::string &path);

} // namespace internal
} // namespace csdb

#endif // _CREDITS_CSDB_UTILS_H_INCLUDED_
