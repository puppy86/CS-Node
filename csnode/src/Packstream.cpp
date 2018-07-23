#include <csnode/Node.hpp>
#include <csnode/Packstream.hpp>

template <>
IPackStream& IPackStream::operator>>(Credits::NodeId& cont) {
    uint32_t uintIp;
    *this >> uintIp;

    if (good_)
        cont = boost::asio::ip::make_address_v4(uintIp);

    return *this;
}

template <>
IPackStream& IPackStream::operator>>(std::string& str) {
    str = std::string(ptr_, end_);
    ptr_ = end_;
    return *this;
}

template <>
IPackStream& IPackStream::operator>>(csdb::Transaction& cont) {
    cont = csdb::Transaction::from_byte_stream(ptr_, end_ - ptr_);
    ptr_ = end_;
    return *this;
}

template <>
IPackStream& IPackStream::operator>>(csdb::Pool& pool) {
    pool = csdb::Pool::from_byte_stream(ptr_, end_ - ptr_);
    ptr_ = end_;
    return *this;
}

template <>
OPackStream& OPackStream::operator<<(const Credits::NodeId& d) {
    *this << (uint32_t)d.to_v4().to_uint();
    return *this;
}

template <>
OPackStream& OPackStream::operator<<(const std::string& str) {
    insertBytes(str.data(), str.size());
    return *this;
}

template <>
OPackStream& OPackStream::operator<<(const csdb::Transaction& trans) {
    auto byteArray = trans.to_byte_stream();
    insertBytes((char*)byteArray.data(), byteArray.size());
    return *this;
}

template <>
OPackStream& OPackStream::operator<<(const csdb::Pool& pool) {
    size_t bSize;
    char* data = const_cast<csdb::Pool&>(pool).to_byte_stream(bSize);
    insertBytes((char*)data, bSize);
    return *this;
}