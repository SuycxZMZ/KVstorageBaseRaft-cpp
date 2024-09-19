// Copyright (C) 2023 Andrei Avram

#ifndef MSD_CHANNEL_HPP_
#define MSD_CHANNEL_HPP_

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <type_traits>

#include "blocking_iterator.hpp"

namespace msd {

#if (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
#define NODISCARD [[nodiscard]]
#else
#define NODISCARD
#endif

/**
 * @brief Exception thrown if trying to write on closed channel.
 */
class closed_channel : public std::runtime_error {
   public:
    explicit closed_channel(const char* msg) : std::runtime_error{msg} {}
};

/**
 * @brief Thread-safe container for sharing data between threads.
 *
 * Implements a blocking input iterator.
 *
 * @tparam T The type of the elements.
 */
template <typename T>
class channel {
   public:
    using value_type [[maybe_unused]] = T;
    using iterator = blocking_iterator<channel<T>>;
    using size_type = std::size_t;

    /**
     * Creates an unbuffered channel.
     */
    constexpr channel() = default;

    /**
     * Creates a buffered channel.
     *
     * @param capacity Number of elements the channel can store before blocking.
     */
    explicit constexpr channel(size_type capacity);

    /**
     * Pushes an element into the channel.
     *
     * @throws closed_channel if channel is closed.
     */
    template <typename Type>
    friend channel<typename std::decay<Type>::type>& operator<<(channel<typename std::decay<Type>::type>&, Type&&);

    /**
     * Pops an element from the channel.
     *
     * @tparam Type The type of the elements
     */
    template <typename Type>
    friend channel<Type>& operator>>(channel<Type>&, Type&);

    /**
     * Returns the number of elements in the channel.
     */
    NODISCARD inline size_type constexpr size() const noexcept;

    /**
     * Returns true if there are no elements in channel.
     */
    NODISCARD inline bool constexpr empty() const noexcept;

    /**
     * Closes the channel.
     */
    [[maybe_unused]] inline void close() noexcept;

    /**
     * Returns true if the channel is closed.
     */
    NODISCARD inline bool closed() const noexcept;

    /**
     * Iterator
     */
    iterator begin() noexcept;
    iterator end() noexcept;

    /**
     * @brief 尝试在给定时间内从channel中获取一个元素
     *
     * @param out 保存取出的元素
     * @param timeout_duration 超时时间ms
     * @return true 如果成功获取
     * @return false 如果超时
     */
    bool try_pop_with_timeout(T& out, int timeout_duration);

    /**
     * Channel cannot be copied or moved.
     */
    channel(const channel&) = delete;
    channel& operator=(const channel&) = delete;
    channel(channel&&) = delete;
    channel& operator=(channel&&) = delete;
    virtual ~channel() = default;

   private:
    const size_type cap_{0};
    std::queue<T> queue_;
    std::atomic<std::size_t> size_{0};
    std::mutex mtx_;
    std::condition_variable cnd_;
    std::atomic<bool> is_closed_{false};

    inline void waitBeforeRead(std::unique_lock<std::mutex>&);
    inline void waitBeforeWrite(std::unique_lock<std::mutex>&);
    friend class blocking_iterator<channel>;
};

}  // namespace msd

// Copyright (C) 2023 Andrei Avram

#include <chrono>
namespace msd {

template <typename T>
constexpr channel<T>::channel(const size_type capacity) : cap_{capacity} {}

template <typename T>
channel<typename std::decay<T>::type>& operator<<(channel<typename std::decay<T>::type>& ch, T&& in) {
    if (ch.closed()) {
        throw closed_channel{"cannot write on closed channel"};
    }

    {
        std::unique_lock<std::mutex> lock{ch.mtx_};
        ch.waitBeforeWrite(lock);

        ch.queue_.push(std::forward<T>(in));
        ++ch.size_;
    }

    ch.cnd_.notify_one();

    return ch;
}

template <typename T>
channel<T>& operator>>(channel<T>& ch, T& out) {
    if (ch.closed() && ch.empty()) {
        return ch;
    }

    {
        std::unique_lock<std::mutex> lock{ch.mtx_};
        ch.waitBeforeRead(lock);

        if (!ch.empty()) {
            out = std::move(ch.queue_.front());
            ch.queue_.pop();
            --ch.size_;
        }
    }

    ch.cnd_.notify_one();

    return ch;
}

template <typename T>
constexpr typename channel<T>::size_type channel<T>::size() const noexcept {
    return size_;
}

template <typename T>
constexpr bool channel<T>::empty() const noexcept {
    return size_ == 0;
}

template <typename T>
[[maybe_unused]] void channel<T>::close() noexcept {
    {
        std::unique_lock<std::mutex> lock{mtx_};
        is_closed_.store(true);
    }
    cnd_.notify_all();
}

template <typename T>
bool channel<T>::closed() const noexcept {
    return is_closed_.load();
}

template <typename T>
blocking_iterator<channel<T>> channel<T>::begin() noexcept {
    return blocking_iterator<channel<T>>{*this};
}

template <typename T>
blocking_iterator<channel<T>> channel<T>::end() noexcept {
    return blocking_iterator<channel<T>>{*this};
}

template <typename T>
void channel<T>::waitBeforeRead(std::unique_lock<std::mutex>& lock) {
    cnd_.wait(lock, [this]() { return !empty() || closed(); });
}

template <typename T>
void channel<T>::waitBeforeWrite(std::unique_lock<std::mutex>& lock) {
    if (cap_ > 0 && size_ == cap_) {
        cnd_.wait(lock, [this]() { return size_ < cap_; });
    }
}

template <typename T>
bool channel<T>::try_pop_with_timeout(T& out, int timeout_duration) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (!cnd_.wait_for(lock, std::chrono::milliseconds(timeout_duration),
                       [this]() { return !queue_.empty() || is_closed_; })) {
        return false;  // 超时未能获取元素
    }

    if (queue_.empty()) {
        return false;  // 在等待期间channel被关闭
    }

    out = std::move(queue_.front());
    queue_.pop();
    --size_;
    return true;
}

}  // namespace msd

#endif  // MSD_CHANNEL_HPP_
