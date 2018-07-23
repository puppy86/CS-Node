#pragma once

#include <memory>
#include <boost/asio.hpp>

#include <net/SessionIO.hpp>

#include "Blockchain.hpp"

#include <csstats.h>
#include <csconnector/csconnector.h>

using namespace boost::asio;

#include "Packstream.hpp"

namespace Credits {

enum NodeLevel {
	Normal,
	Confidant,
	Main,
	Writer
};

class Node {
public:
	Node(const NodeId&, const PublicKey&, SessionIO*);

	/* Incoming requests processing */
	void getInitRing(const char*, const size_t);
	void getRoundTable(const char*, const size_t);
	void getTransaction(const char*, const size_t);
	void getFirstTransaction(const char*, const size_t);
	void getTransactionsList(const char*, const size_t);
	void getVector(const char*, const size_t, const NodeId&);
	void getMatrix(const char*, const size_t, const NodeId&);
	void getBlock(const char*, const size_t, const NodeId&);
	void getHash(const char*, const size_t, const NodeId&);

	/* Outcoming requests forming */
	void sendRoundTable();
	void sendTransaction(const csdb::Transaction&);
	void sendTransaction(std::vector<csdb::Transaction>&&);
	void sendFirstTransaction(const csdb::Transaction&);
	void sendTransactionList(const csdb::Pool&, const NodeId&);
	void sendVector(const Vector&);
	void sendMatrix(const Matrix&);
	void sendBlock(const csdb::Pool&);
	void sendHash(const Hash&, const NodeId&);

	void becomeWriter();
	void initNextRound(const NodeId& mainNode, std::vector<NodeId>&& confidantNodes);

	template <typename CallBack, typename... Args>
	void runAfter(const std::chrono::milliseconds& timeout, CallBack cb, Args... args) {
		net_->waitOnTimer(timeout, cb, args...);
	}

	bool isGood() const { return good_; }

	const NodeId& getMyId() const { return myId_; }
	const PublicKey& getMyPublicKey() const { return myPublicKey_; }

	NodeLevel getMyLevel() const { return myLevel_; }
	const std::vector<NodeId>& getConfidants() const { return confidantNodes_; }

	BlockChain& getBlockChain() { return bc_; }
	const BlockChain& getBlockChain() const { return bc_; }

private:
	bool init();

	inline bool readRoundData(bool);
	void onRoundStart();

	inline void sendByConfidants(CommandList, SubCommandList, std::vector<TaskId>&);
	inline void sendByConfidants(CommandList, SubCommandList);

	// Info
	const NodeId myId_;
	const PublicKey myPublicKey_;
	bool good_ = true;

	// Current round state
	uint32_t roundNum_ = 0;
	NodeLevel myLevel_;
	NodeId mainNode_;

	std::vector<NodeId> confidantNodes_;

	// Working mem
	std::vector<TaskId> vectorTasks_;

	// Resources
	BlockChain bc_;
    csstats::csstats stats;

	SessionIO* net_;
	std::unique_ptr<ISolver> solver_;
	
    csconnector::csconnector api;

	IPackStream istream_;
	OPackStream ostream_;
};

} // namespace Credits
