#ifndef UTIL_H
#define UTIL_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <chrono>
#include <cstdarg>
#include <iostream>
#include <sstream>

// ---------------------- DEBUG ---------------------- //
template <typename... Args>
std::string format(const char* format_str, Args... args) {
    std::stringstream ss;
    int _[] = {((ss << args), 0)...};
    (void)_;
    return ss.str();
}

std::chrono::_V2::system_clock::time_point now();
std::chrono::milliseconds getRandomizedElectionTimeout();

// ---------------------- DEBUG ---------------------- //

/**
 * @brief 这个Op是kvServer传递给raft的command
 *            std::string Operation;  // "Get" "Put" "Append"
              std::string Key;
              std::string Value;
              std::string ClientId;  
              int RequestId;         
 */
class Op {
public:
    // Your definitions here.
    // Field names must start with capital letters,
    // otherwise RPC will break.
    std::string Operation;  // "Get" "Put" "Append"
    std::string Key;
    std::string Value;
    std::string ClientId;  // 客户端号码
    int RequestId;         // 客户端号码请求的Request的序列号，为了保证线性一致性

public:
    // todo
    // 为了协调raftRPC中的command只设置成了string,这个的限制就是正常字符中不能包含|
    // 当然后期可以换成更高级的序列化方法，比如protobuf
    std::string asString() const {
        std::stringstream ss;
        boost::archive::text_oarchive oa(ss);

        // write class instance to archive
        oa << *this;
        // close archive

        return ss.str();
    }

    bool parseFromString(std::string& str) {
        std::stringstream iss(str);
        boost::archive::text_iarchive ia(iss);
        // read class state from archive
        ia >> *this;
        return true;  // todo : 解析失败如何处理，要看一下boost库
    }

public:
    friend std::ostream& operator<<(std::ostream& os, const Op& obj) {
        os << "[MyClass:Operation{" + obj.Operation + "},Key{" + obj.Key + "},Value{" + obj.Value + "},ClientId{" +
                  obj.ClientId + "},RequestId{" + std::to_string(obj.RequestId) + "}";  // 在这里实现自定义的输出格式
        return os;
    }

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & Operation;
        ar & Key;
        ar & Value;
        ar & ClientId;
        ar & RequestId;
    }
};

//kvserver reply err to clerk
const std::string OK = "OK";
const std::string ErrNoKey = "ErrNoKey";
const std::string ErrWrongLeader = "ErrWrongLeader";

#endif  //  UTIL_H