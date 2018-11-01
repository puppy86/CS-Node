#include "csdb/storage.h"

#include <sstream>
#include <cstdarg>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cassert>
#include <stdexcept>

#include "csdb/address.h"
#include "csdb/wallet.h"
#include "csdb/pool.h"
#include "csdb/database.h"
#include "csdb/internal/shared_data_ptr_implementation.h"
#include "csdb/database_berkeleydb.h"
#include "csdb/internal/utils.h"
#include "binary_streams.h"

namespace {
struct last_error_struct {
  ::csdb::Storage::Error last_error_ = ::csdb::Storage::NoError;
  std::string last_error_message_;
};

last_error_struct& last_error_map(const void* p)
{
  static thread_local ::std::map<const void*, last_error_struct> last_errors_;
  return last_errors_[p];
}
} // namespace;

namespace csdb {

namespace
{

struct head_info_t
{
  size_t len_;        // The number of blocks in the chain
  PoolHash next_;     // next pool hash, or empty string for the first pool in the chain
  
};
using heads_t = std::map<PoolHash, head_info_t>;
using tails_t = std::map<PoolHash, PoolHash>;

void update_heads_and_tails(heads_t &heads, tails_t &tails, const PoolHash &cur_hash, const PoolHash &prev_hash)
{
  auto ith = heads.find(prev_hash);
  auto itt = tails.find(cur_hash);
  bool eith = (heads.end() != ith);
  bool eitt = (tails.end() != itt);
  if (eith && eitt) {
    // connect two subchains.
    assert(1 == heads.count(itt->second));
    head_info_t& ith1 = heads[itt->second];
    ith1.next_ = ith->second.next_;
    ith1.len_ += (1 + ith->second.len_);
    if (!ith->second.next_.is_empty()) {
      tails[ith->second.next_] = itt->second;
    }
    heads.erase(ith);
    tails.erase(cur_hash);
  } else if (eith && (!eitt)) {
    // Add to the beginning of the chain.
    if (!ith->second.next_.is_empty()) {
      tails[ith->second.next_] = cur_hash;
    }
    assert(0 == heads.count(cur_hash));
    heads.emplace(cur_hash, head_info_t{ith->second.len_ + 1, ith->second.next_} );
    heads.erase(prev_hash);
  } else if ((!eith) && eitt) {
    // Add to the end of the chain.
    assert(1 == heads.count(itt->second));
    head_info_t& ith1 = heads[itt->second];
    ith1.next_ = prev_hash;
    ++ith1.len_;
    if (!prev_hash.is_empty()) {
      tails.emplace(prev_hash, itt->second);
    }
    tails.erase(cur_hash);
  } else {
    assert(0 == heads.count(cur_hash));
    heads.emplace(cur_hash, head_info_t{1, prev_hash});
    if (!prev_hash.is_empty()) {
      tails.emplace(prev_hash, cur_hash);
    }
  }
}

}

class Storage::priv
{
public:
  priv()
  {
    last_error_map(this) = last_error_struct();
  }

  ~priv()
  {
    if (write_thread.joinable()) {
      quit = true;
      write_cond_var.notify_one();
      write_thread.join();
    }
  }

private:
  bool rescan(Storage::OpenCallback callback);
  void write_routine();

  std::shared_ptr<Database> db = nullptr;
  PoolHash last_hash;           // last pool hash
  size_t count_pool = 0;        // Number of transaction pools in the repository

  void set_last_error(Storage::Error error = Storage::NoError, const ::std::string& message = ::std::string());
  void set_last_error(Storage::Error error, const char* message, ...);

  std::thread write_thread;
  bool quit = false;

  std::queue<Pool> write_queue;
  std::mutex write_lock;
  std::condition_variable write_cond_var;


  friend class ::csdb::Storage;
};

void Storage::priv::set_last_error(Storage::Error error, const ::std::string& message)
{
  last_error_struct &les = last_error_map(this);
  les.last_error_ = error;
  les.last_error_message_ = message;
}

void Storage::priv::set_last_error(Storage::Error error, const char* message, ...)
{
  last_error_struct &les = last_error_map(this);
  les.last_error_ = error;
  if (nullptr != message) {
    va_list args1;
    va_start(args1, message);
    va_list args2;
    va_copy(args2, args1);
    les.last_error_message_.resize(std::vsnprintf(NULL, 0, message, args1) + 1);
    va_end(args1);
    std::vsnprintf(&(les.last_error_message_[0]), les.last_error_message_.size(), message, args2);
    va_end(args2);
    les.last_error_message_.resize(les.last_error_message_.size() - 1);
  } else {
    les.last_error_message_.clear();
  }
}

bool Storage::priv::rescan(Storage::OpenCallback callback)
{
  uint32_t lastSeq = 0;

  last_hash = {};
  count_pool = 0;

  heads_t heads;
  tails_t tails;

  Database::IteratorPtr it = db->new_iterator();
  assert(it);

  std::vector<std::pair<PoolHash, PoolHash>> als;

  Storage::OpenProgress progress{0};
  for(it->seek_to_first(); it->is_valid(); it->next())
  {
    const ::csdb::internal::byte_array v = it->value();

    // Does the hash in the key match the real hash of the block?
    PoolHash real_hash = PoolHash::calc_from_data(v);

    Pool p = Pool::from_binary(v);
    if(!p.is_valid())
    {
      set_last_error(Storage::DataIntegrityError, "Data integrity error: Corrupted pool for key '%s'.",
                     real_hash.to_string().c_str());
      return false;
    }

    if(p.hash() != real_hash)
    {
      set_last_error(Storage::DataIntegrityError, "Data integrity error: key does not match real hash "
                     "(key: '%s'; real hash: '%s')", p.hash().to_string().c_str(), real_hash.to_string().c_str());
      return false;
    }

    count_pool++;
    progress.poolsProcessed++;
    if (nullptr != callback) {
      if(callback(progress)) {
        set_last_error(Storage::UserCancelled);
        return false;
      }
    }

    if (p.sequence() >= lastSeq) {
      lastSeq = p.sequence();
      als.resize(lastSeq + 1);
    }

    als[p.sequence()] = std::make_pair(real_hash, p.previous_hash());
  }

  // Now check integrity
  if (!als.size()) return true;

  last_hash = als[0].first;
  for (uint32_t i = 1; i < als.size(); ++i) {
    if (i > 0 && als[i].second != als[i-1].first) break;
    last_hash = als[i].first;
  }

  return true;
}

void Storage::priv::write_routine() {
  std::unique_lock<std::mutex> lock(write_lock);
  while (!quit) {
    write_cond_var.wait(lock);
    while (!write_queue.empty()) {
      Pool &pool = write_queue.front();
      if (!pool.is_read_only()) {
        pool.compose();
      }
      const PoolHash hash = pool.hash();

      db->put(hash.to_binary(), static_cast<uint32_t>(pool.sequence()), pool.to_binary());
      ++count_pool;
      if (last_hash == pool.previous_hash()) {
        last_hash = hash;
      }
      write_queue.pop();
    }
  }
}

Storage::Storage() :
  d(::std::make_shared<priv>())
{
}

Storage::~Storage()
{
}

Storage::Storage(WeakPtr ptr) noexcept :
  d(ptr.lock())
{
  if (!d) {
    d = ::std::make_shared<priv>();
  }
}

Storage::WeakPtr Storage::weak_ptr() const noexcept
{
  return d;
}

Storage::Error Storage::last_error() const
{
  return last_error_map(d.get()).last_error_;
}

::std::string Storage::last_error_message() const
{
  const last_error_struct &les = last_error_map(d.get());
  if (!les.last_error_message_.empty()) {
    return les.last_error_message_;
  }
  switch (les.last_error_) {
  case NoError: return "No error";
  case NotOpen: return "Storage is not open";
  case DatabaseError: return "Database error: " + db_last_error_message();
  case ChainError: return "Chain integrity error";
  case DataIntegrityError: return "Data integrity error";
  case UserCancelled: return "Operation cancalled by user";
  case InvalidParameter: return "Invalid parameter passed to method.";
  default: return "Unknown error";
  }
}

Database::Error Storage::db_last_error() const
{
  if (d->db) {
    return d->db->last_error();
  }
  return Database::NotOpen;
}

::std::string Storage::db_last_error_message() const
{
  if (d->db) {
    return d->db->last_error_message();
  }
  return ::std::string{"Database not specified"};
}

bool Storage::open(const OpenOptions &opt, OpenCallback callback)
{
  if (!opt.db) {
    d->set_last_error(DatabaseError, "No valid database driver specified.");
    return false;
  }

  d->db = opt.db;

  if (!d->db->is_open()) {
    d->set_last_error(DatabaseError, "Error open database: %s", d->db->last_error_message().c_str());
    return false;
  }

  if(!d->rescan(callback)) {
    d->db.reset();
    return false;
  }

  d->set_last_error();
  return true;
}

bool Storage::open(const ::std::string& path_to_base, OpenCallback callback)
{
  ::std::string path{path_to_base};
  if (path.empty()) {
    path = ::csdb::internal::app_data_path() + "/CREDITS";
  }

  auto db{::std::make_shared<::csdb::DatabaseBerkeleyDB>()};
  db->open(path);

  d->write_thread = std::thread(&Storage::priv::write_routine, d.get());

  return open(OpenOptions{db}, callback);
}

void Storage::close()
{
  d->db.reset();
  d->set_last_error();
}

bool Storage::isOpen() const
{
  return ((d->db) && (d->db->is_open()));
}

void Storage::set_last_hash(const csdb::PoolHash& h) noexcept
{
  d->last_hash = h;
}

void Storage::set_size(const size_t size) noexcept
{
  d->count_pool = size;
}

PoolHash Storage::last_hash() const noexcept
{
  return d->last_hash;
}

size_t Storage::size() const noexcept
{
  return d->count_pool;
}

bool Storage::pool_save(Pool pool)
{
  if (!isOpen()) {
    d->set_last_error(NotOpen);
    return false;
  }

  if(!pool.is_valid()) {
    d->set_last_error(InvalidParameter, "%s: Invalid pool passed", __func__);
    return false;
  }

  const PoolHash hash = pool.hash();

  if(d->db->get(hash.to_binary()))
  {
    d->set_last_error(InvalidParameter, "%s: Pool already pressent [hash: %s]", __func__, hash.to_string().c_str());
    return false;
  }

  std::unique_lock<std::mutex> lock(d->write_lock);
  d->write_queue.push(pool);
  d->write_cond_var.notify_one();

  d->set_last_error();
  return true;
}

Pool Storage::pool_load(const PoolHash &hash) const
{
  if (!isOpen()) {
    d->set_last_error(NotOpen);
    return Pool{};
  }

  if(hash.is_empty())
  {
    d->set_last_error(InvalidParameter, "%s: Empty hash passed", __func__);
    return Pool{};
  }

  ::csdb::internal::byte_array data;
  if (!d->db->get(hash.to_binary(), &data)) {
    d->set_last_error(DatabaseError);
    return Pool{};
  }

  Pool res = Pool::from_binary(data);
  if (!res.is_valid()) {
    d->set_last_error(DataIntegrityError, "%s: Error decoding pool [hash: %s]", __func__, hash.to_string().c_str());
  }
  else {
    d->set_last_error();
  }
  return res;
}

Pool Storage::pool_load_meta(const PoolHash &hash, size_t& cnt) const
{
  if (!isOpen()) {
    d->set_last_error(NotOpen);
    return Pool{};
  }

  if (hash.is_empty())
  {
    d->set_last_error(InvalidParameter, "%s: Empty hash passed", __func__);
    return Pool{};
  }

  ::csdb::internal::byte_array data;
  if (!d->db->get(hash.to_binary(), &data)) {
    d->set_last_error(DatabaseError);
    return Pool{};
  }

  Pool res = Pool::meta_from_binary(data, cnt);
  if (!res.is_valid()) {
    d->set_last_error(DataIntegrityError, "%s: Error decoding pool [hash: %s]", __func__, hash.to_string().c_str());
  }
  else {
    d->set_last_error();
  }

  return res;
}

Wallet Storage::wallet(const Address &addr) const
{
  return Wallet::get(addr);
}

bool Storage::get_from_blockchain(const Address &addr /*input*/, const int64_t &InnerId /*input*/, Transaction &trx/*output*/) const {
  Pool curPool;
  TransactionID::sequence_t curIdx = -1;
  TransactionID last_trx_id = get_last_by_source(addr).id();
  bool is_in_blockchain = false;

  if (last_trx_id.is_valid()) {
    curPool = pool_load(last_trx_id.pool_hash());
    if (curPool.is_valid() && last_trx_id.index() < curPool.transactions_count())
      curIdx = last_trx_id.index();
  }

  auto nextIt = [this, &curPool, &curIdx]() -> bool {
    if (curPool.is_valid()) {
      if (curIdx) {
        curIdx--;
        return true;
      }
      else {
        do {
          curPool = pool_load(curPool.previous_hash());
        } while (curPool.is_valid() && !(curPool.transactions_count()));
        if (curPool.is_valid()) {
          curIdx = static_cast<TransactionID::sequence_t>(curPool.transactions_count() - 1);
          return true;
        }
      }
    }
    else {
      curPool = pool_load(last_hash());
      while (curPool.is_valid() && !(curPool.transactions_count())) {
        curPool = pool_load(curPool.previous_hash());
      }
      if (curPool.is_valid()) {
        curIdx = static_cast<TransactionID::sequence_t>(curPool.transactions_count() - 1);
        return true;
      }
    }
    return false;
  };

  if (curIdx == -1)
    return false;

  do {
    const Transaction trx_curr = curPool.transaction(curIdx);
    if (trx_curr.source() == addr && trx_curr.innerID() == InnerId) {
      is_in_blockchain = true;
      trx = trx_curr;
      break;
    }
  } while (nextIt());

  return is_in_blockchain;
}

std::vector<Transaction> Storage::transactions(const Address &addr, size_t limit, const TransactionID &offset) const
{
  std::vector<Transaction> res;
  res.reserve(limit);

  Pool curPool;
  TransactionID::sequence_t curIdx = 0;

  auto seekIt = [this, &curPool, &curIdx](const TransactionID &id) -> bool {
    if(id.is_valid())
    {
      curPool = pool_load(id.pool_hash());
      if(curPool.is_valid() && id.index() < curPool.transactions_count())
      {
        curIdx = id.index();
        return true;
      }
    }
    return false;
  };

  auto nextIt = [this, &curPool, &curIdx]() -> bool {
    if(curPool.is_valid())
    {
      if(curIdx)
      {
        curIdx--;
        return true;
      }
      else
      {
        do {
          curPool = pool_load(curPool.previous_hash());
        } while (curPool.is_valid() && !(curPool.transactions_count()));
        if(curPool.is_valid())
        {
          curIdx = static_cast<TransactionID::sequence_t>(curPool.transactions_count() - 1);
          return true;
        }
      }
    }
    else
    {
      curPool = pool_load(last_hash());
      while (curPool.is_valid() && !(curPool.transactions_count())) {
        curPool = pool_load(curPool.previous_hash());
      }
      if(curPool.is_valid())
      {
        curIdx = static_cast<TransactionID::sequence_t>(curPool.transactions_count() - 1);
        return true;
      }
    }
    return false;
  };

  if(offset.is_valid())
    if(!seekIt(offset))
      return res;

  while(res.size() < limit && nextIt())
  {
    const Transaction t = curPool.transaction(curIdx);
    if((t.source() == addr) || (t.target() == addr))
      res.push_back(t);
  }

  return res;
}

Transaction Storage::transaction(const TransactionID &id) const
{
  if(!id.is_valid()) {
    d->set_last_error(InvalidParameter, "%s: Transaction id is not valid", __func__);
    return Transaction{};
  }

  return pool_load(id.pool_hash()).transaction(id);
}

Transaction Storage::get_last_by_source(Address source) const noexcept
{
  Pool curr = pool_load(last_hash());

  while (curr.is_valid())
  {
    const auto& t = curr.get_last_by_source(source);
    if (t.is_valid())
      return t;

    curr = pool_load(curr.previous_hash());
  }

  return Transaction{};
}

Transaction Storage::get_last_by_target(Address target) const noexcept
{
  Pool curr = pool_load(last_hash());

  while (curr.is_valid())
  {
    const auto& t = curr.get_last_by_target(target);
    if (t.is_valid())
      return t;

    curr = pool_load(curr.previous_hash());
  }

  return Transaction{};
}

}