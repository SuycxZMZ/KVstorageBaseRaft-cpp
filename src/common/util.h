#ifndef UTIL_H
#define UTIL_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <condition_variable>  // pthread_condition_t
#include <functional>
#include <iostream>
#include <mutex>  // pthread_mutex_t
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include "config.h"

#define Dprintf_Debug 0

template <class F>
class DeferClass {
   public:
    DeferClass(F&& f) : m_func(std::forward<F>(f)) {}
    DeferClass(const F& f) : m_func(f) {}
    ~DeferClass() { m_func(); }

    DeferClass(const DeferClass& e) = delete;
    DeferClass& operator=(const DeferClass& e) = delete;

   private:
    F m_func;
};

#define _CONCAT(a, b) a##b
#define _MAKE_DEFER_(line) DeferClass _CONCAT(defer_placeholder, line) = [&]()

#undef DEFER
#define DEFER _MAKE_DEFER_(__LINE__)

void DPrintf(const char* format, ...);

void myAssert(bool condition, std::string message = "Assertion failed!");

template <typename... Args>
std::string format(const char* format_str, Args... args) {
    std::stringstream ss;
    int _[] = {((ss << args), 0)...};
    (void)_;
    return ss.str();
}

std::chrono::_V2::system_clock::time_point now();

std::chrono::milliseconds getRandomizedElectionTimeout();
void sleepNMilliseconds(int N);

template <typename T>
class LockQueue {
public:
    // 向队列中推送数据（线程安全）
    void Push(const T& data) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);  
            m_queue.push(data);
        }
        m_condvariable.notify_one();  // 释放锁后通知等待的线程
    }

    // 弹出队列中的数据，如果队列为空则阻塞等待（线程安全）
    T Pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condvariable.wait(lock, [this]{ return !m_queue.empty(); });  // 阻塞直到队列非空
        T data = std::move(m_queue.front());  // 使用 move 构造，避免不必要的拷贝
        m_queue.pop();
        return data;
    }

    /**
     * @brief 带超时的 Pop 操作，如果超时未获取数据返回 false（线程安全）
     * 
     * @param timeout 超时时间
     * @param ResData 弹出值的指针，要提前申请好
     * @return true 正常弹出
     * @return false 没东西可弹或者一直没拿到锁，超时，ResData无效
     */
    bool timeOutPop(int timeout, T* ResData) {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        
        // 超时等待队列不为空
        if (!m_condvariable.wait_until(lock, now + std::chrono::milliseconds(timeout), [this]{ return !m_queue.empty(); })) {
            return false;  // 超时未获取数据
        }

        *ResData = std::move(m_queue.front());
        m_queue.pop();
        return true;  // 成功获取数据
    }

    // 获取队列中的第一个元素，但不弹出（线程安全）
    T front() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condvariable.wait(lock, [this]{ return !m_queue.empty(); });  // 阻塞直到队列非空
        return m_queue.front();  // 返回队列头部的元素
    }

    // 获取队列的大小（线程安全）
    size_t size() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    // 检查队列是否为空（线程安全）
    bool empty() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    std::queue<T> m_queue;  // 底层使用 std::queue 存储数据
    std::mutex m_mutex;  // 保护队列操作的互斥锁
    std::condition_variable m_condvariable;  // 用于管理阻塞等待的条件变量
};

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

    bool parseFromString(std::string str) {
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

///////////////////////////////////////////////kvserver reply err to clerk

const std::string OK = "OK";
const std::string ErrNoKey = "ErrNoKey";
const std::string ErrWrongLeader = "ErrWrongLeader";

////////////////////////////////////获取可用端口

bool isReleasePort(unsigned short usPort);

bool getReleasePort(short& port);

#endif  //  UTIL_H