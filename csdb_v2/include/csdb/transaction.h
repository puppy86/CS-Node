#pragma once
#ifndef _CREDITS_CSDB_TRANSACTION_H_INCLUDED_
#define _CREDITS_CSDB_TRANSACTION_H_INCLUDED_

#include <set>

#include "csdb/user_field.h"

#include "csdb/internal/shared_data.h"
#include "csdb/internal/types.h"


namespace csdb {

namespace priv {
class obstream;
class ibstream;
} // namespace priv

class Address;
class Amount;
class Currency;
class PoolHash;
class Pool;

//Class of unique transaction identifier in the database
class TransactionID
{
  SHARED_DATA_CLASS_DECLARE(TransactionID)
public:

  using sequence_t = size_t;


  TransactionID(PoolHash poolHash, sequence_t index);

  bool is_valid() const noexcept;
  PoolHash pool_hash() const noexcept;


  sequence_t index() const noexcept;

  std::string to_string() const noexcept;

//Getting the transaction ID from a string representation
  static TransactionID from_string(const ::std::string& str);

  bool operator ==(const TransactionID &other) const noexcept;
  bool operator !=(const TransactionID &other) const noexcept;
  bool operator < (const TransactionID &other) const noexcept;

private:
  void put(::csdb::priv::obstream&) const;
  bool get(::csdb::priv::ibstream&);
  friend class ::csdb::priv::obstream;
  friend class ::csdb::priv::ibstream;
  friend class Transaction;
  friend class Pool;
};

class Transaction
{
  SHARED_DATA_CLASS_DECLARE(Transaction)

public:
  Transaction(Address source, Address target, Currency currency, Amount amount);
  Transaction(Address source, Address target, Currency currency, Amount amount, Amount balance);

  bool is_valid() const noexcept;
  bool is_read_only() const noexcept;

  TransactionID id() const noexcept;

  Address source() const noexcept;
  Address target() const noexcept;
  Currency currency() const noexcept;
  Amount amount() const noexcept;
  Amount balance() const noexcept;

  void set_source(Address source);
  void set_target(Address target);
  void set_currency(Currency currency);
  void set_amount(Amount amount);
  void set_balance(Amount balance);

  ::csdb::internal::byte_array to_binary();
  static Transaction from_binary(const ::csdb::internal::byte_array data);

  static Transaction from_byte_stream(const char* data, size_t m_size);
  std::vector<uint8_t> to_byte_stream() const;

  //Adds an optional custom field to the transaction
  bool add_user_field(user_field_id_t id, UserField field) noexcept;

  //Returns an additional field
  UserField user_field(user_field_id_t id) const noexcept;

  //List of additional field identifiers
  ::std::set<user_field_id_t> user_field_ids() const noexcept;

private:
  void put(::csdb::priv::obstream&) const;
  bool get(::csdb::priv::ibstream&);
  friend class ::csdb::priv::obstream;
  friend class ::csdb::priv::ibstream;
  friend class Pool;
};

} // namespace csdb

#endif // _CREDITS_CSDB_TRANSACTION_H_INCLUDED_
