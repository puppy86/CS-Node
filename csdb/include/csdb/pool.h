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

//Getting a hash from a string representation
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
  char* to_byte_stream(uint32_t&);
  ::csdb::internal::byte_array to_byte_stream_for_sig();

  Pool meta_from_byte_stream(const char*, size_t);
  static Pool from_lz4_byte_stream(const char*, size_t, size_t);


  bool clear()  noexcept;

  bool is_valid() const noexcept;
  bool is_read_only() const noexcept;
  PoolHash previous_hash() const noexcept;
  sequence_t sequence() const noexcept;
  Storage storage() const noexcept;
  size_t transactions_count() const noexcept;
  std::vector<uint8_t> writer_public_key() const noexcept;

  void set_previous_hash(PoolHash previous_hash) noexcept;
  void set_sequence(sequence_t sequence) noexcept;
  void set_storage(Storage storage) noexcept;
  void set_writer_public_key(std::vector<uint8_t> writer_public_key) noexcept;
  std::vector<csdb::Transaction>& transactions();

  //Adds a transaction to the pool
  bool add_transaction(Transaction transaction
#ifdef CSDB_UNIT_TEST
                       , bool skip_check
#endif
                       );

  //Finish pooling
  bool compose();

  //Hash pool
  PoolHash hash() const noexcept;
  void recount() noexcept;

  //Binary pool representation
  ::csdb::internal::byte_array to_binary() const noexcept;

  //Save pool to storage
  bool save(Storage storage = Storage());

  //Adds an optional extra field to the pool.
  bool add_user_field(user_field_id_t id, UserField field) noexcept;

  //Returns an extra field
  UserField user_field(user_field_id_t id) const noexcept;

  //List of identifiers of extra fields
  ::std::set<user_field_id_t> user_field_ids() const noexcept;

  //deprecated (will be excluded in future versions)
  Transaction transaction(size_t index ) const;

  //Get a transaction by ID
  Transaction transaction(TransactionID id) const;

  //Get the latest transaction at the source address
  Transaction get_last_by_source(Address source) const noexcept;

  //Get the latest transaction at destination
  Transaction get_last_by_target(Address target) const noexcept;

  void sign(std::vector<uint8_t> private_key);
  bool verify_signature();

  friend class Storage;
};

inline bool PoolHash::operator !=(const PoolHash &other) const noexcept
{
  return !operator ==(other);
}

} // namespace csdb

#endif // _CREDITS_CSDB_POOL_H_INCLUDED_
