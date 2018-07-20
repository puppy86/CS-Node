#include <vector>
#include <thread>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>

#include <csnode/Node.hpp>

#include "net/Logger.hpp"
#include "net/SessionIO.hpp"

#include "Solver/ISolver.hpp"

const char PUBLIC_KEY_FILENAME[] = "PublicKey.txt";
const size_t PURE_PUBLIC_KEY_LENGTH = 44;

const unsigned CLOSE_TIMEOUT_SEC = 10;

const unsigned MAX_REDIRECT = 1;

std::atomic_bool SessionIO::AwaitingRegistration{true};
std::function<void(PacketPtr*)> PacketPtr::freeFunc = [](PacketPtr*) { };

using namespace std::placeholders;
SessionIO::SessionIO() : InputServiceResolver_(io_service_client_), 
						 OutputServiceResolver_(io_service_client_) {
	if (!Initialization()) {
		std::cerr << "Cannot initialize session due to critical errors. The node will be closed in " << CLOSE_TIMEOUT_SEC << " seconds..." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(CLOSE_TIMEOUT_SEC));
		exit(1);
	}
}

SessionIO::~SessionIO() {
	free(m_combinedData);

	m_taskman.stop();
	m_senderThread.join();
}

bool SessionIO::Initialization() {
	boost::property_tree::ptree config;
	boost::property_tree::read_ini("Configure.ini", config);

	const boost::property_tree::ptree & host_Input = config.get_child("hostInput");
	udp::resolver::query query_send(udp::v4(), host_Input.get<std::string>("ip"), host_Input.get<std::string>("port", "9001"));
	InputServiceRecvEndpoint_ = *InputServiceResolver_.resolve(query_send);
	InputServiceSocket_ = new udp::socket(io_service_client_, InputServiceRecvEndpoint_);
	boost::asio::ip::udp::socket::receive_buffer_size recvBuff(65536);
	InputServiceSocket_->set_option(boost::asio::ip::udp::socket::reuse_address(true));
	InputServiceSocket_->set_option(recvBuff);

	const boost::property_tree::ptree & host_Output = config.get_child("hostOutput");
	udp::resolver::query query_recv(udp::v4(), host_Output.get<std::string>("ip"), host_Output.get<std::string>("port", "9000"));
	OutputServiceRecvEndpoint_ = *OutputServiceResolver_.resolve(query_recv);
	OutputServiceSocket_ = new udp::socket(io_service_client_, OutputServiceRecvEndpoint_);
	boost::asio::ip::udp::socket::send_buffer_size sendBuff(65536);
	OutputServiceSocket_->set_option(boost::asio::ip::udp::socket::reuse_address(true));
	InputServiceSocket_->set_option(sendBuff);

	const boost::property_tree::ptree & server = config.get_child("server");
	udp::resolver::query query_serv(udp::v4(), server.get<std::string>("ip"), server.get<std::string>("port", "6000"));
	OutputServiceServerEndpoint_ = *OutputServiceResolver_.resolve(query_serv);

	signalServerAddr = OutputServiceServerEndpoint_.address();


	m_combinedData = (char*)malloc(MAX_PART * max_length);
	if (!m_combinedData) return false;

	MyIp_ = InputServiceRecvEndpoint_.address();
	if (!GenerationHash()) return false;

	node_ = std::make_unique<Credits::Node>(MyIp_, MyPublicKey_, this);
	if (!node_ || !node_->isGood()) return false;

	return true;
}

void SessionIO::InitConnection() {
	ReceiveRegistration();

	std::cerr << "Connecting to the Signal Server... " << std::endl;

	std::thread t(&SessionIO::RegistrationToServer, this);
	t.detach();
}

void SessionIO::ReceiveRegistration() {
	PacketPtr nextPack = m_pacman.getFreePack();

	InputServiceSocket_->async_receive_from(
		boost::asio::buffer(nextPack.get(), sizeof(Packet)),
		InputServiceSendEndpoint_,
		[this, nextPack] (const boost::system::error_code& error, std::size_t bytes_transferred) {
			LOG_IN_PACK(nextPack, bytes_transferred);

			bool entered = false;

			if (nextPack->command == CommandList::Registration) {
				std::cerr << "Connect... OK" << std::endl;
				AwaitingRegistration = false;

				if (nextPack->subcommand == SubCommandList::RegistrationLevelNode) {
					std::cerr << "Connected to the running network" << std::endl;
					node_->getInitRing(nextPack->data, bytes_transferred - Packet::headerLength());
					entered = true;
				}
			}
			else if (nextPack->command == CommandList::Redirect && nextPack->subcommand == SubCommandList::RegistrationLevelNode) {
				std::cerr << "Round started" << std::endl;
				node_->getInitRing(nextPack->data, bytes_transferred - Packet::headerLength());
				entered = true;
			}
			else if (nextPack->command == CommandList::RegistrationConnectionRefused) {
				std::cerr << "Connection REFUSED (bad client version)" << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(10'000'000));
			}

			addToRingBuffer(InputServiceSendEndpoint_.address());

			if (entered)
				StartReceive();
			else
				ReceiveRegistration();
		});
}

void SessionIO::StartReceive() {
	PacketPtr nextPack = m_pacman.getFreePack();

	InputServiceSocket_->async_receive_from(
			boost::asio::buffer(nextPack.get(), sizeof(Packet)),
			InputServiceSendEndpoint_,
			[this, nextPack] (const boost::system::error_code& error, std::size_t bytes_transferred) {
				LOG_IN_PACK(nextPack, bytes_transferred);
				InputServiceHandleReceive(nextPack, error, bytes_transferred);
				StartReceive();
			});
}

inline void SessionIO::InputServiceHandleReceive(PacketPtr message, const boost::system::error_code& error, std::size_t bytes_transferred) {
	if (error) {
		std::cerr << "Receive error: " << error << std::endl;
		return;
	}
	
	addToRingBuffer(InputServiceSendEndpoint_.address());

	bool multiPack = false;
	char* dataPtr = message->data;
	std::size_t size = bytes_transferred - Packet::headerLength();

	if (message->countHeader > 0) {
		if (message->countHeader > MAX_PART || message->header >= message->countHeader)
			return; 

		if (message->command == CommandList::Redirect)
			RunRedirect(message, size);

		auto packResult = m_packets.append(message, size);
		if (!packResult.second) return;

		if (packResult.first->left != 0) return;

		PacketPtr* last = packResult.first->packets + message->countHeader - 1;
		char* writePtr = m_combinedData;
		size_t total = packResult.first->totalSize;
		for (PacketPtr* ptr = packResult.first->packets; ptr != last; ++ptr) {
			memcpy(writePtr, (*ptr)->data, max_length);
			total -= max_length;
			writePtr += max_length;
		}

		memcpy(writePtr, (*last)->data, total);

		multiPack = true;
		dataPtr = m_combinedData;
		size = packResult.first->totalSize;
	}

	if (message->command != CommandList::Redirect && getBackDataCounter(message) > 1)
		return;

	switch (message->command) {
		case CommandList::Redirect:	
		{
			if (!multiPack && !RunRedirect(message, size))
				return;

			switch (message->subcommand) {
				case SubCommandList::SGetIpTable:
				{
					node_->getRoundTable(dataPtr, size);
					break;
				}
				case SubCommandList::GetBlock:
				{
					node_->getBlock(dataPtr, size, ip::make_address_v4(message->origin_ip));
					break;
				}
				case SubCommandList::RegistrationLevelNode: { break; }
				default:
				{
					LOG_WARN("Unknown command received: " << (int)message->command << ":" << (int)message->subcommand << " from " << ip::make_address_v4(message->origin_ip));
					break;
				}
			}
			break;
		}
		case CommandList::GetBlockCandidate:
		{
			node_->getTransactionsList(dataPtr, size);
			break;
		}
		case CommandList::GetTransaction:
		{
			node_->getTransaction(dataPtr, size);
			break;
		}
		case CommandList::GetFirstTransaction:
		{
			node_->getFirstTransaction(dataPtr, size);
			break;
		}
		case CommandList::GetVector:
		{
			node_->getVector(dataPtr, size, ip::make_address_v4(message->origin_ip));
			break;
		}
		case CommandList::GetMatrix:
		{
			node_->getMatrix(dataPtr, size, ip::make_address_v4(message->origin_ip));
			break;
		}
		case CommandList::GetHash:
		{
			node_->getHash(dataPtr, size, ip::make_address_v4(message->origin_ip));
			break;
		}
		case CommandList::SinhroPacket: { break; }
		default:
		{
			LOG_WARN("Unknown command received: " << (int)message->command << ":" << (int)message->subcommand << " from " << ip::make_address_v4(message->origin_ip));
			break;
		}
	}
}

inline bool SessionIO::RunRedirect(PacketPtr message, std::size_t dataSize) {
	auto counter = getBackDataCounter(message);

	const bool needProcessing = (counter == 1);
	if (counter > MAX_REDIRECT)
		return needProcessing;

	memcpy(message->hash, MyHash_.str, hash_length);
	memcpy(message->publicKey, MyPublicKey_.str, publicKey_length);

	outSendPack(message, dataSize, nullptr);

	return needProcessing;
}

inline uint32_t SessionIO::getBackDataCounter(PacketPtr message) {
	Hash key{ message->HashBlock };
	*((uint16_t*)(key.str)) = message->header;

	return m_backData.pushAndIncrease(key);
}

void SessionIO::addToRingBuffer(const boost::asio::ip::address& addr) {
	udp::endpoint regEndPoint(addr, addr == signalServerAddr ? signalServerPort : nodePort);
	bool addedNew = m_nodesRing.place(std::move(regEndPoint));
	if (addedNew) SendGreetings();
}

TaskId SessionIO::addTaskDirect(std::vector<PacketPtr>&& packets, const CommandList cmd, const SubCommandList subcmd, const size_t lastSize, const ip::address& ip) {
	createSendTasks(packets, cmd, subcmd, lastSize);
	udp::endpoint regEndPoint(ip, ip == signalServerAddr ? signalServerPort : nodePort);

	Task t(std::move(packets), lastSize, std::move(regEndPoint));
	return m_taskman.add(std::move(t));
}

TaskId SessionIO::addTaskBroadcast(std::vector<PacketPtr>&& packets, const SubCommandList subcmd, const size_t lastSize) {
	createSendTasks(packets, CommandList::Redirect, subcmd, lastSize);

	Task t(std::move(packets), lastSize, m_nodesRing.getEndPoints());
	return m_taskman.add(std::move(t));
}

inline void SessionIO::createSendTasks(const std::vector<PacketPtr>& packets, const CommandList cmd, const SubCommandList subcmd, const size_t lastSize) {
	if (packets.empty()) return;

	outFrmPack(packets.front(), cmd, subcmd, Version::version_1, packets.size() == 1 ? lastSize : max_length);
	packets.front()->header = 0;

	if (packets.size() == 1)
		packets.front()->countHeader = 0;
	else {
		packets.front()->countHeader = (uint16_t)packets.size();
		for (size_t i = 1; i < packets.size(); ++i) {
			memcpy(packets[i].get(), packets.front().get(), Packet::headerLength());
			packets[i]->header = (uint16_t)i;
		}
	}
}

inline void SessionIO::outFrmPack(const PacketPtr packet, const CommandList cmd, const SubCommandList sub_cmd, const Version ver, const size_t size_data) {
	m_hasher.nextHash(packet->data, size_data, packet->HashBlock);
	packet->origin_ip = MyIp_.to_v4().to_uint();

	packet->command = cmd;
	packet->subcommand = sub_cmd;
	packet->version = ver;

	memcpy(packet->hash, MyHash_.str, hash_length);
	memcpy(packet->publicKey, MyPublicKey_.str, publicKey_length);
}

inline void SessionIO::outSendPack(PacketPtr message, std::size_t dataSize, const udp::endpoint* endpoint) {
	const auto size_pck = dataSize + Packet::headerLength();

	if (endpoint == nullptr)
		for (auto& ep : m_nodesRing.getEndPoints())
			handleSend(message, size_pck, ep);
	else
		handleSend(message, size_pck, *endpoint);
}

inline void SessionIO::handleSend(PacketPtr message, std::size_t size_pck, const udp::endpoint& endpoint) {
	OutputServiceSocket_->async_send_to(boost::asio::buffer((char*)message.get(), size_pck),
		endpoint,
		boost::bind(&SessionIO::outputHandleSend, this, message,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void SessionIO::outputHandleSend(PacketPtr message, const boost::system::error_code& error, std::size_t bytes_transferred) {
	LOG_OUT_PACK(message, bytes_transferred);
	if (error || !bytes_transferred) {
		LOG_ERROR("Cannot send package (transferred " << bytes_transferred << "): " << error);
	}
}

void SessionIO::senderThreadRoutine() {
	m_taskman.run([this] (const Task& task) {
		size_t cntr = 0;
		for (auto& pack : task.packets) {
			++cntr;
			for (auto& recv : task.receivers) {
				handleSend(pack, (cntr == task.packets.size() ? task.lastSize : Packet::headerLength() + max_length), recv);
			}
		}
	});
}

inline void SessionIO::RegistrationToServer() {
	while (AwaitingRegistration) { 
		std::string version = std::to_string(CURRENT_VERSION);
		auto pack = m_pacman.getFreePack();
		memcpy(pack->data, version.c_str(), version.size());
		outFrmPack(pack, CommandList::Registration, SubCommandList::Empty, Version::version_1, version.size());
		outSendPack(pack, version.size(), &OutputServiceServerEndpoint_);
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
}

inline void SessionIO::SendSinhroPacket() {
	
}

inline void SessionIO::SendGreetings() {
	
}

bool SessionIO::GenerationHash() { 
	char buf[sizeof(ip::address_v4::uint_type) + PURE_PUBLIC_KEY_LENGTH + 1];
	*((ip::address_v4::uint_type*)buf) = MyIp_.to_v4().to_uint();

	std::ifstream fin(PUBLIC_KEY_FILENAME);
	if (!fin.is_open()) {
		std::cerr << "Cannot open '" << PUBLIC_KEY_FILENAME << "'!" << std::endl;
		return false;
	}

	fin.getline(buf + sizeof(ip::address_v4::uint_type), PURE_PUBLIC_KEY_LENGTH + 1);
	fin.close();

	const auto pkZerosLength = publicKey_length - BLAKE2_HASH_LENGTH;
	memset(MyPublicKey_.str, 0, pkZerosLength);
	blake2s(MyPublicKey_.str + pkZerosLength, BLAKE2_HASH_LENGTH, buf + sizeof(ip::address_v4::uint_type), PURE_PUBLIC_KEY_LENGTH, nullptr, 0);
	std::cerr << "Key:" << byteStreamToHex(MyPublicKey_.str, publicKey_length) << std::endl;

	m_hasher.init(MyPublicKey_);

	const auto hsZerosLength = hash_length - BLAKE2_HASH_LENGTH;
	blake2s(MyHash_.str + hsZerosLength, BLAKE2_HASH_LENGTH, buf, sizeof(buf) - 1, nullptr, 0);

	return true;
}

void SessionIO::Run() {
	InitConnection();

	while (true) {
		io_service_client_.poll();
		senderThreadRoutine();
	}
}

PublicKey getHashedPublicKey(const char* str) {
	PublicKey result;
	blake2s(result.str, BLAKE2_HASH_LENGTH, str, PURE_PUBLIC_KEY_LENGTH, nullptr, 0);
	return result;
}
