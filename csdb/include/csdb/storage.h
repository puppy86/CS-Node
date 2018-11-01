#pragma once
#ifndef _CREDITS_CSDB_STORAGE_H_INCLUDED_
#define _CREDITS_CSDB_STORAGE_H_INCLUDED_

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "csdb/transaction.h"
#include "csdb/database.h"

namespace csdb {

class Pool;
class PoolHash;
class Address;
class Wallet;
class Transaction;
class TransactionID;

//Storage object
class Storage final
{
private:
  class priv;

public:
  using WeakPtr = ::std::weak_ptr<priv>;

  enum Error {
    NoError = 0,
    NotOpen = 1,
    DatabaseError = 2,
    ChainError = 3,
    DataIntegrityError = 4,
    UserCancelled = 5,
    InvalidParameter = 6,
    UnknownError = 255,
  };

public:
  Error last_error() const;
  std::string last_error_message() const;
  Database::Error db_last_error() const;
  std::string db_last_error_message() const;

public:
  Storage();
  ~Storage();

public:
  inline Storage(const Storage &) noexcept = default;
  inline Storage(Storage &&) noexcept = default;
  inline Storage& operator=(const Storage &) noexcept = default;
  inline Storage& operator=(Storage &&) noexcept = default;

  explicit Storage(WeakPtr ptr) noexcept;
  WeakPtr weak_ptr() const noexcept;

public:
  struct OpenOptions
  {
    // Экземпляр драйвера базы данных
    ::std::shared_ptr<Database> db;
  };

  struct OpenProgress
  {
    uint64_t poolsProcessed;
  };

  //Callback for opening operations
  typedef ::std::function<bool(const OpenProgress&)> OpenCallback;

  //Opens a storage for a set of parameters
  bool open(const OpenOptions &opt, OpenCallback callback = nullptr);

  //Opens a storage on the storage path.
  bool open(const ::std::string& path_to_base = ::std::string{}, OpenCallback callback = nullptr);

  
  //Creating a storage for a set of parameters
  static inline Storage get(const OpenOptions &opt, OpenCallback callback = nullptr);

  //Creating a storage on the storage path
  static inline Storage get(const ::std::string& path_to_base = ::std::string{}, OpenCallback callback = nullptr);

  //Checks if storage is open
  bool isOpen() const;

  //Closing storage
  void close();

  //Last block hash
  PoolHash last_hash() const noexcept;
  void set_last_hash(const PoolHash&) noexcept;
  void set_size(const size_t) noexcept;

  //Writes a pool to the storage
  bool pool_save(Pool pool);

  //Loads a pool from storage
  Pool pool_load(const PoolHash &hash) const;
  Pool pool_load_meta(const PoolHash &hash, size_t& cnt) const;

  //Getting a transaction by ID
  Transaction transaction(const TransactionID &id) const;

  //Get the latest transaction at the source address
  Transaction get_last_by_source(Address source) const noexcept;

  //Get the latest transaction at destination
  Transaction get_last_by_target(Address target) const noexcept;

  //size returns the number of pools in the storage.
  size_t size() const noexcept;

  //Get a wallet for the specified address
  Wallet wallet(const Address &addr) const;

  //Get a list of transactions for the specified address
  std::vector<Transaction> transactions(const Address &addr, size_t limit = 100, const TransactionID &offset = TransactionID()) const;

  //Returns true if the transaction with addr and innerId is in blockchain
  bool get_from_blockchain(const Address &addr /*input*/, const int64_t &InnerId /*input*/, Transaction &trx/*output*/) const;

private:
  ::std::shared_ptr<priv> d;
};

inline Storage Storage::get(const OpenOptions &opt, OpenCallback callback)
{
  Storage s;
  s.open(opt, callback);
  return s;
}

inline Storage Storage::get(const std::string& path_to_base, OpenCallback callback)
{
  Storage s;
  s.open(path_to_base, callback);
  return s;
}

} // namespace csdb

#endif // _CREDITS_CSDB_STORAGE_H_INCLUDED_
