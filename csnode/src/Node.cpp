#include "Solver/ISolver.hpp"
#include "Solver/SolverFactory.hpp"

#include <net/Logger.hpp>

#include "csnode/Node.hpp"

#include <snappy.h>

const char PATH_TO_DB[] = "test_db";

const unsigned MIN_CONFIDANTS = 3;
const unsigned MAX_CONFIDANTS = 3;

namespace Credits {

Node::Node(const NodeId& myId, const PublicKey& pk, SessionIO* net)
  : myId_(myId)
  , myPublicKey_(pk)
  , bc_(PATH_TO_DB)
  , ostream_(net)
  , net_(net)
  , solver_(
      Credits::SolverFactory().createSolver(Credits::solver_type::real, this))
  , stats(bc_)
  , api(bc_, solver_.get())
{
  good_ = init();
}

bool
Node::init()
{
  if (!bc_.isGood())
    return false;

  // Create solver

  if (!solver_)
    return false;
  solver_->initApi();
  solver_->addInitialBalance();

  return true;
}

/* Requests */

void
Node::getRoundTable(const char* data, const size_t size)
{
  istream_.init(data, size);

  if (!readRoundData(false))
    return;

  if (!istream_.good()) {
    LOG_WARN("Bad round table format, ignoring");
    return;
  }

  onRoundStart();
  net_->removeAllTasks();
}

void
Node::sendRoundTable()
{
  ostream_.init();
  ostream_ << roundNum_ << mainNode_;

  for (auto& conf : confidantNodes_)
    ostream_ << conf;

  LOG_EVENT("Sending round table");

  net_->removeAllTasks();
  net_->addTaskBroadcast(std::move(ostream_.get()),
                         SubCommandList::SGetIpTable,
                         ostream_.lastSize());
}

void
Node::getTransaction(const char* data, const size_t size)
{
  if (myLevel_ != NodeLevel::Main && myLevel_ != NodeLevel::Writer) {
    return;
  }

  istream_.init(data, size);

  while (istream_.good() && !istream_.end()) {
    csdb::Transaction trans;
    istream_ >> trans;
    solver_->gotTransaction(std::move(trans));
  }

  if (!istream_.good()) {
    LOG_WARN("Bad transaction packet format");
    return;
  }
}

void
Node::sendTransaction(const csdb::Transaction& trans)
{
#ifndef TESTING
  if (myLevel_ == NodeLevel::Main) {
    LOG_ERROR("Main node cannot send transactions");
    return;
  }
#endif

  ostream_.init();
  ostream_ << trans;

  LOG_EVENT("Sending transaction to " << mainNode_);
  net_->addTaskDirect(std::move(ostream_.get()),
                      CommandList::GetTransaction,
                      SubCommandList::Empty,
                      ostream_.lastSize(),
                      mainNode_);
}

void
Node::sendTransaction(std::vector<csdb::Transaction>&& transactions)
{
#ifndef TESTING
  if (myLevel_ == NodeLevel::Main) {
    LOG_ERROR("Main node cannot send transactions");
    return;
  }
#endif

  ostream_.init();

  for (auto& tr : transactions)
    ostream_ << tr;

  LOG_EVENT("Sending transactions to " << mainNode_);
  net_->addTaskDirect(std::move(ostream_.get()),
                      CommandList::GetTransaction,
                      SubCommandList::Empty,
                      ostream_.lastSize(),
                      mainNode_);
}

void
Node::getFirstTransaction(const char* data, const size_t size)
{
  if (myLevel_ != NodeLevel::Confidant) {
    return;
  }

  istream_.init(data, size);

  csdb::Transaction trans;
  istream_ >> trans;

  if (!istream_.good() || !istream_.end()) {
    LOG_WARN("Bad transaction packet format");
    return;
  }

  LOG_EVENT("Got first transaction, initializing consensus...");


  solver_->gotTransactionList(std::move(trans));
}

void
Node::sendFirstTransaction(const csdb::Transaction& trans)
{
  if (myLevel_ != NodeLevel::Main) {
    LOG_ERROR("Only main nodes can initialize the consensus procedure");
    return;
  }

  ostream_.init();
  ostream_ << trans;

  sendByConfidants(CommandList::GetFirstTransaction, SubCommandList::Empty);
}

void
Node::getTransactionsList(const char* data, const size_t size)
{
  if (myLevel_ != NodeLevel::Confidant && myLevel_ != NodeLevel::Writer) {
    return;
  }

  istream_.init(data, size);

  csdb::Pool pool;
  istream_ >> pool;

  if (!istream_.good() || !istream_.end()) {
    LOG_WARN("Bad transactions list packet format");
    return;
  }

  LOG_EVENT("Got full transactions list of " << pool.transactions_count());
  solver_->gotBlockCandidate(std::move(pool));
}

void
Node::sendTransactionList(const csdb::Pool& pool, const NodeId& target)
{
  if (myLevel_ != NodeLevel::Main) {
    LOG_ERROR("Only main nodes can send transaction lists");
    return;
  }

  ostream_.init();
  ostream_ << pool;

  net_->addTaskDirect(std::move(ostream_.get()),
                      CommandList::GetBlockCandidate,
                      SubCommandList::Empty,
                      ostream_.lastSize(),
                      target);
}

void
Node::getVector(const char* data, const size_t size, const NodeId& sender)
{
  if (myLevel_ != NodeLevel::Confidant) {
    return;
  }

  istream_.init(data, size);

  Vector vec;
  istream_ >> vec;

  if (!istream_.good() || !istream_.end()) {
    LOG_WARN("Bad vector packet format");
    return;
  }

  LOG_EVENT("Got vector from " << sender);
  solver_->gotVector(std::move(vec), sender);
}

void
Node::sendVector(const Vector& vector)
{
  if (myLevel_ != NodeLevel::Confidant) {
    LOG_ERROR("Only confidant nodes can send vectors");
    return;
  }

  ostream_.init();
  ostream_ << vector;

  vectorTasks_.clear();
  vectorTasks_.reserve(confidantNodes_.size());

  sendByConfidants(CommandList::GetVector, SubCommandList::Empty, vectorTasks_);
}

void
Node::getMatrix(const char* data, const size_t size, const NodeId& sender)
{
  if (myLevel_ != NodeLevel::Confidant) {
    return;
  }

  istream_.init(data, size);

  Matrix mat;
  istream_ >> mat;

  if (!istream_.good() || !istream_.end()) {
    LOG_WARN("Bad matrix packet format");
    return;
  }

  LOG_EVENT("Got matrix from " << sender);

  std::size_t taskIdx = 0;
  for (auto& conf : confidantNodes_) {
    if (conf == sender)
      break;
    if (conf != myId_)
      ++taskIdx;
  }

  // Block after matrix <= change level
  solver_->gotMatrix(std::move(mat), sender);
}

void
Node::sendMatrix(const Matrix& matrix)
{
  if (myLevel_ != NodeLevel::Confidant) {
    LOG_ERROR("Only confidant nodes can send matrices");
    return;
  }

  ostream_.init();
  ostream_ << matrix;

  sendByConfidants(CommandList::GetMatrix, SubCommandList::Empty);
}

void
Node::getBlock(const char* data, const size_t size, const NodeId& sender)
{
  if (myLevel_ == NodeLevel::Writer) {
    return;
  }

  myLevel_ = NodeLevel::Normal;

#ifdef NET_COMPRESSION
  std::string decompressed;
  ::snappy::Uncompress(data, size, &decompressed);
  istream_.init(decompressed.data(), decompressed.size());
#else
  istream_.init(data, size);
#endif

  csdb::Pool pool;
  istream_ >> pool;

  if (!istream_.good() || !istream_.end()) {
    LOG_WARN("Bad block packet format");
    return;
  }

  LOG_EVENT("Got block of " << pool.transactions_count());

  solver_->gotBlock(std::move(pool), sender);
}

void
Node::sendBlock(const csdb::Pool& pool)
{
  if (myLevel_ != NodeLevel::Writer) {
    LOG_ERROR("Only writer nodes can send blocks");
    return;
  }

  ostream_.init();
  size_t bSize;
#ifdef NET_COMPRESSION
  char* data = const_cast<csdb::Pool&>(pool).to_byte_stream(bSize);
  std::string compressed;
  ::snappy::Compress(data, bSize, &compressed);
  ostream_ << compressed;
#else
  ostream_ << pool;
#endif

  LOG_EVENT("Sending block of " << pool.transactions_count());
  net_->addTaskBroadcast(
    std::move(ostream_.get()), SubCommandList::GetBlock, ostream_.lastSize());
}

void
Node::getHash(const char* data, const size_t size, const NodeId& sender)
{
  if (myLevel_ != NodeLevel::Writer) {
    return;
  }

  istream_.init(data, size);

  Hash hash;
  istream_ >> hash;

  if (!istream_.good() || !istream_.end()) {
    LOG_WARN("Bad hash packet format");
    return;
  }

  LOG_EVENT("Got hash from " << sender);
  solver_->gotHash(std::move(hash), sender);
}

void
Node::sendHash(const Hash& hash, const NodeId& target)
{
  if (myLevel_ == NodeLevel::Writer) {
    LOG_ERROR("Writer node shouldn't send hashes");
    return;
  }

  ostream_.init();
  ostream_ << hash;

  net_->addTaskDirect(std::move(ostream_.get()),
                      CommandList::GetHash,
                      SubCommandList::Empty,
                      ostream_.lastSize(),
                      target);
}

void
Node::getInitRing(const char* data, const size_t size)
{
  istream_.init(data, size);

  if (!readRoundData(true))
    return;

  while (istream_.good() && !istream_.end()) {
    NodeId forRing;
    istream_ >> forRing;
    net_->addToRingBuffer(forRing);
  }

  if (!istream_.good() || !istream_.end()) {
    LOG_WARN("Bad init round format, ignoring");
    return;
  }

  onRoundStart();
  net_->removeAllTasks();
}

void
Node::becomeWriter()
{
  if (myLevel_ != NodeLevel::Main && myLevel_ != NodeLevel::Confidant)
    LOG_WARN("Logically impossible to become a writer right now");

  myLevel_ = NodeLevel::Writer;
}

void
Node::onRoundStart()
{
  if (mainNode_ == myId_)
    myLevel_ = NodeLevel::Main;
  else {
    bool found = false;
    for (auto& conf : confidantNodes_) {
      if (conf == myId_) {
        myLevel_ = NodeLevel::Confidant;
        found = true;
        break;
      }
    }

    if (!found)
      myLevel_ = NodeLevel::Normal;
  }

  solver_->nextRound();

  std::cerr << "Round " << roundNum_ << " started. Mynode_type:=" << myLevel_
            << ", General: " << mainNode_ << ", Confidants: ";
  for (auto& e : confidantNodes_)
    std::cerr << e << " ";
  std::cerr << std::endl;
}

void
Node::initNextRound(const NodeId& mainNode,
                    std::vector<NodeId>&& confidantNodes)
{
  if (myLevel_ != NodeLevel::Writer) {
    LOG_ERROR(
      "Trying to initialize a new round without the required privileges");
    return;
  }

  ++roundNum_;
  mainNode_ = mainNode;
  std::swap(confidantNodes, confidantNodes_);
  onRoundStart();
}

inline void
Node::sendByConfidants(CommandList cmd,
                       SubCommandList scmd,
                       std::vector<TaskId>& taskIds)
{
  taskIds.clear();

  for (auto& conf : confidantNodes_) {
    if (conf == myId_)
      continue;

    std::vector<PacketPtr> toSend(ostream_.get().begin(), ostream_.get().end());
    taskIds.push_back(net_->addTaskDirect(
      std::move(toSend), cmd, scmd, ostream_.lastSize(), conf));
  }
}

inline void
Node::sendByConfidants(CommandList cmd, SubCommandList scmd)
{
  for (auto& conf : confidantNodes_) {
    if (conf == myId_)
      continue;

    std::vector<PacketPtr> toSend(ostream_.get().begin(), ostream_.get().end());
    net_->addTaskDirect(
      std::move(toSend), cmd, scmd, ostream_.lastSize(), conf);
  }
}

inline bool
Node::readRoundData(const bool tail)
{
  uint32_t roundNum;
  NodeId mainNode;
  std::vector<NodeId> confidants;
  confidants.reserve(MAX_CONFIDANTS);

  istream_ >> roundNum;
  if (roundNum <= roundNum_) {
    LOG_NOTICE("Old round info received: " << roundNum_ << " >= " << roundNum
                                           << ". Ignoring...");
    return false;
  }

  istream_ >> mainNode;
  while (istream_) {
    confidants.push_back(NodeId());
    istream_ >> confidants.back();

    if (confidants.size() == MAX_CONFIDANTS && !istream_.end()) {
      if (tail)
        break;
      else {
        LOG_WARN("Too many confidant nodes received");
        return false;
      }
    }
  }

  if (!istream_.good() || confidants.size() < MIN_CONFIDANTS) {
    LOG_WARN("Bad round table format, ignoring");
    return false;
  }

  roundNum_ = roundNum;
  mainNode_ = mainNode;
  std::swap(confidants, confidantNodes_);

  net_->addToRingBuffer(mainNode_);
  for (auto& conf : confidantNodes_)
    net_->addToRingBuffer(conf);

  return true;
}

} // namespace Credits
