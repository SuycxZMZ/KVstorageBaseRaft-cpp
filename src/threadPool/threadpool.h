/**
 * @file threadpool.h
 * @author Suycx
 * @brief 线程池实现
 * @details 动态伸缩：支持在最小线程和最大线程数之间伸缩
 *          //参考 https://github.com/lzpong/
 * @version 0.1
 * @date 2024-08-01
 */
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>
// #include <iostream>

namespace sylar {
// 线程池最大容量,应尽量设小一点
#define THREADPOOL_MAX_NUM 16
// 线程池是否可以自动增长(如果需要,且不超过 THREADPOOL_MAX_NUM)
#define THREADPOOL_AUTO_GROW

// 线程池,可以提交变参函数或拉姆达表达式的匿名函数执行,可以获取执行返回值
// 不直接支持类成员函数, 支持类静态成员函数或全局函数,Opteron()函数等
class threadpool {
    unsigned short _initSize;            // 初始化线程数量
    using Task = std::function<void()>;  // 定义类型
    std::vector<std::thread> _pool;      // 线程池
    std::queue<Task> _tasks;             // 任务队列
    std::mutex _lock;                    // 任务队列同步锁
#ifdef THREADPOOL_AUTO_GROW
    std::mutex _lockGrow;              // 线程池增长同步锁
#endif                                 // !THREADPOOL_AUTO_GROW
    std::condition_variable _task_cv;  // 条件阻塞
    std::atomic<bool> _run{true};      // 线程池是否执行
    std::atomic<int> _idlThrNum{0};    // 空闲线程数量

   public:
    inline threadpool(unsigned short size = 4) {
        _initSize = size;
        addThread(size);
        // std::cout << "------------- thread pool create --- size=" << size << "-------------\n";
    }
    inline ~threadpool() {
        _run = false;
        _task_cv.notify_all();  // 唤醒所有线程执行
        for (std::thread& thread : _pool) {
            // thread.detach(); // 让线程“自生自灭”
            if (thread.joinable()) thread.join();  // 等待任务结束， 前提：线程一定会执行完
        }
    }

   public:
    // 提交一个任务
    // 调用.get()获取返回值会等待任务执行完,获取返回值
    // 有两种方法可以实现调用类成员，
    // 一种是使用   bind： .commit(std::bind(&Dog::sayHello, &dog));
    // 一种是用   mem_fn： .commit(std::mem_fn(&Dog::sayHello), this)
    template <class F, class... Args>
    auto commit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        if (!_run) throw std::runtime_error("commit on ThreadPool is stopped.");

        using RetType = decltype(f(args...));  // typename std::result_of<F(Args...)>::type, 函数 f 的返回值类型
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));  // 把函数入口及参数,打包(绑定)
        std::future<RetType> future = task->get_future();
        {                                             // 添加任务到队列
            std::lock_guard<std::mutex> lock{_lock};  // 对当前块的语句加锁
            _tasks.emplace([task]() { (*task)(); });
        }
#ifdef THREADPOOL_AUTO_GROW
        if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM) addThread(1);
#endif                          // !THREADPOOL_AUTO_GROW
        _task_cv.notify_one();  // 唤醒一个线程执行

        return future;
    }
    // 提交一个无参任务, 且无返回值
    template <class F>
    void commit(F&& task) {
        if (!_run) return;
        {
            std::lock_guard<std::mutex> lock{_lock};
            _tasks.emplace(std::forward<F>(task));
        }
#ifdef THREADPOOL_AUTO_GROW
        if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM) addThread(1);
#endif  // !THREADPOOL_AUTO_GROW
        _task_cv.notify_one();
    }
    // 空闲线程数量
    int idlCount() { return _idlThrNum; }
    // 线程数量
    int thrCount() { return _pool.size(); }

#ifndef THREADPOOL_AUTO_GROW
   private:
#endif  // !THREADPOOL_AUTO_GROW
    // 添加指定数量的线程
    void addThread(unsigned short size) {
#ifdef THREADPOOL_AUTO_GROW
        if (!_run)  // stoped ??
            throw std::runtime_error("Grow on ThreadPool is stopped.");
        std::unique_lock<std::mutex> lockGrow{_lockGrow};  // 自动增长锁
#endif                                                     // !THREADPOOL_AUTO_GROW
        for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0;
             --size) {                   // 增加线程数量,但不超过 预定义数量 THREADPOOL_MAX_NUM
            _pool.emplace_back([this] {  // 工作线程函数
                while (true)             // 防止 _run==false 时立即结束,此时任务队列可能不为空
                {
                    Task task;  // 获取一个待执行的 task
                    {
                        // unique_lock 相比 lock_guard 的好处是：可以随时 unlock() 和 lock()
                        std::unique_lock<std::mutex> lock{_lock};
                        _task_cv.wait(lock, [this] {  // wait 直到有 task, 或需要停止
                            return !_run || !_tasks.empty();
                        });
                        if (!_run && _tasks.empty()) return;
                        _idlThrNum--;
                        task = move(_tasks.front());  // 按先进先出从队列取一个 task
                        _tasks.pop();
                    }
                    task();  // 执行任务
#ifdef THREADPOOL_AUTO_GROW
                    if (_idlThrNum > 0 && _pool.size() > _initSize)  // 支持自动释放空闲线程,避免峰值过后大量空闲线程
                        return;
#endif  // !THREADPOOL_AUTO_GROW
                    {
                        std::unique_lock<std::mutex> lock{_lock};
                        _idlThrNum++;
                    }
                }
            });
            {
                std::unique_lock<std::mutex> lock{_lock};
                _idlThrNum++;
            }
        }
    }

    // 带超时时间的提交
    template <class F, class... Args>
    auto commit_with_timeout(F&& f, Args&&... args, int timeout_ms) -> std::future<decltype(f(args...))> {
        if (!_run) throw std::runtime_error("commit on ThreadPool is stopped.");

        using RetType = decltype(f(args...));
        auto task =
            std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<RetType> future = task->get_future();

        auto start_time = std::chrono::steady_clock::now();  // 记录任务提交时间
        std::chrono::milliseconds timeout(timeout_ms);       // 将 int 转换为 std::chrono::milliseconds

        {  // 添加任务到队列
            std::lock_guard<std::mutex> lock{_lock};
            _tasks.emplace([task, timeout, start_time]() {
                auto now = std::chrono::steady_clock::now();
                if (now - start_time < timeout) {
                    (*task)();
                } else {
                    // Task is considered expired, do nothing
                }
            });
        }

#ifdef THREADPOOL_AUTO_GROW
        if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM) addThread(1);
#endif                          // !THREADPOOL_AUTO_GROW
        _task_cv.notify_one();  // 唤醒一个线程执行

        return future;
    }

    // 带超时时间且无参无返回值的提交
    template <class F>
    void commit_with_timeout(F&& task, int timeout_ms) {
        if (!_run) throw std::runtime_error("commit on ThreadPool is stopped.");

        auto start_time = std::chrono::steady_clock::now();  // 记录任务提交时间
        std::chrono::milliseconds timeout(timeout_ms);       // 将 int 转换为 std::chrono::milliseconds

        {  // 添加任务到队列
            std::lock_guard<std::mutex> lock{_lock};
            _tasks.emplace([task, timeout, start_time]() {
                auto now = std::chrono::steady_clock::now();
                if (now - start_time < timeout) {
                    task();  // 任务未超时，执行任务
                } else {
                    // 任务超时，不执行任务
                }
            });
        }

#ifdef THREADPOOL_AUTO_GROW
        if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM) addThread(1);
#endif                          // !THREADPOOL_AUTO_GROW
        _task_cv.notify_one();  // 唤醒一个线程执行任务
    }
};

}  // namespace sylar

#endif