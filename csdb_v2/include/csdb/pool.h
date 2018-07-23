#pragma once
#ifndef _CREDITS_CSDB_POOL_H_INCLUDED_
#define _CREDITS_CSDB_POOL_H_INCLUDED_

#include <cinttypes>
#include <vector>
#include <array>
#include <string>

#include "csdb/transaction.h"
#include "csdb/storage.h"
#include "csdb/user_field.h"
#include "csdb/internal/shared_data.h"
#include "csdb/internal/types.h"

namespace csdb {

class Transaction;
class TransactionID;

namespace priv {
class obstream;
class ibstream;
} // namespace priv

class PoolHash
{
  SHARED_DATA_CLASS_DECLARE(PoolHash)

public:
  bool is_empty() const noexcept;
  size_t size() const noexcept;
  std::string to_string() const noexcept;

 //Receiving a hash from a string representation
  static PoolHash from_string(const ::std::string& str);

  ::csdb::internal::byte_array to_binary() const noexcept;
  static PoolHash from_binary(const ::csdb::internal::byte_array& data);

  bool operator ==(const PoolHash &other) const noexcept;
  inline bool operator !=(const PoolHash &other) const noexcept;

  bool operator < (const PoolHash &other) const noexcept;

  static PoolHash calc_from_data(const internal::byte_array& data);

private:
  void put(::csdb::priv::obstream&) const;
  bool get(::csdb::priv::ibstream&);
  friend class ::csdb::priv::obstream;
  friend class ::csdb::priv::ibstream;
  friend class Storage;
};

class Pool
{
  SHARED_DATA_CLASS_DECLARE(Pool)
public:
  typedef uint64_t sequence_t;

public:
  Pool(PoolHash previous_hash, sequence_t sequence, Storage storage = Storage());

  static Pool from_binary(const ::csdb::internal::byte_array& data);
  static Pool meta_from_binary(const ::csdb::internal::byte_array& data, size_t& cnt);
  static Pool load(PoolHash hash, Storage storage = Storage());

  static Pool from_byte_stream(const char* data, size_t size);
  char* to_byte_stream(size_t& size);

  bool clear()  noexcept;

  bool is_valid() const noexcept;
  bool is_read_only() const noexcept;
  PoolHash previous_hash() const noexcept;
  sequence_t sequence() const noexcept;
  Storage storage() const noexcept;
  size_t transactions_count() const noexcept;

  void set_previous_hash(PoolHash previous_hash) noexcept;
  void set_sequence(sequence_t sequence) noexcept;
  void set_storage(Storage storage) noexcept;
  std::vector<csdb::Transaction>& transactions();
 
 //Adds a transaction to the pool
  bool add_transaction(Transaction transaction
#ifdef CSDB_UNIT_TEST
                       , bool skip_check
#endif
                       );

 
//Finish pool formation
  bool compose();

 //PoolHash
  PoolHash hash() const noexcept;

//Binary view of the pool
  ::csdb::internal::byte_array to_binary() const noexcept;

 
//Save the pool to the Storage
  bool save(Storage storage = Storage());

  
//Adds an optional random field to the pool
  bool add_user_field(user_field_id_t id, UserField field) noexcept;

  
//Returns an additional field
  UserField user_field(user_field_id_t id) const noexcept;

//List of additional field identifiers
  ::std::set<user_field_id_t> user_field_ids() const noexcept;

  
  Transaction transaction(size_t index ) const;

 
//Get transaction by ID
  Transaction transaction(TransactionID id) const;

//Get the last transaction at the source address
  Transaction get_last_by_source(Address source) const noexcept;

  
//Get the last transaction at the destination address
  Transaction get_last_by_target(Address target) const noexcept;

  friend class Storage;
};

inline bool PoolHash::operator !=(const PoolHash &other) const noexcept
{
  return !operator ==(other);
}

} // namespace csdb

#endif // _CREDITS_CSDB_POOL_H_INCLUDED_
