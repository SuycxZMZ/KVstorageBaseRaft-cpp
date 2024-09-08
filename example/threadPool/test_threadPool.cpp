#include "thirdParty/threadPool/threadpool.h"
#include <iostream>
#include <string>
#include <thread>
#include <future>

void fun1(int slp)
{
    std::cout << "  hello, fun1 !  " << std::this_thread::get_id() << std::endl;
    if (slp > 0) {
        std::cout << " ======= fun1 sleep " << slp << "  =========  " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(slp));
    }
}

struct gfun {
    int operator()(int n) {
        std::cout << n << "  hello, gfun !  " << std::this_thread::get_id() << std::endl;
        return 42;
    }
};

class A {    //函数必须是 static 的才能使用线程池
public:
    static int Afun(int n = 0) {
        std::cout << n << "  hello, Afun !  " << std::this_thread::get_id() << std::endl;
        return n;
    }

    static std::string Bfun(int n, std::string str, char c) {
        std::cout << n << "  hello, Bfun !  " << str << "  " << static_cast<int>(c) << "  " << std::this_thread::get_id() << std::endl;
        return str;
    }
};

int main()
try {
    sylar::threadpool executor{ 50 };
    A a;
    std::future<void> ff = executor.commit(fun1, 0);
    std::future<int> fg = executor.commit(gfun{}, 0);
    std::future<int> gg = executor.commit(A::Afun, 9999); // 使用类名调用静态成员函数
    std::future<std::string> gh = executor.commit(A::Bfun, 9998, "mult args", 123);

    // 这个很奇怪，会报模板参数不匹配
    // std::future<std::string> fh = executor.commit([]() -> std::string { 
    //     std::cout << "hello, fh !  " << std::this_thread::get_id() << std::endl; 
    //     return std::string("hello,fh ret !"); 
    // });

    std::cout << " =======  sleep ========= " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(900));

    for (int i = 0; i < 50; i++) {
        executor.commit(fun1, i * 100);
    }
    std::cout << " =======  commit all ========= " << std::this_thread::get_id() << " idlsize=" << executor.idlCount() << std::endl;

    std::cout << " =======  sleep ========= " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    ff.get(); //调用.get()获取返回值会等待线程执行完,获取返回值
    // std::cout << fg.get() << "  " << fh.get() << "  " << std::this_thread::get_id() << std::endl;

    std::cout << " =======  sleep ========= " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << " =======  fun1,55 ========= " << std::this_thread::get_id() << std::endl;
    executor.commit(fun1, 55).get();    //调用.get()获取返回值会等待线程执行完

    std::cout << "end... " << std::this_thread::get_id() << std::endl;


    sylar::threadpool pool(4);
    for (int i = 0; i < 8; ++i) {
        pool.commit([i]() -> int {
            std::cout << "hello " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "world " << i << std::endl;
            return i * i;
        });
    }
    std::cout << " =======  commit all2 ========= " << std::this_thread::get_id() << std::endl;

    // for (auto&& result : results)
    //     std::cout << result.get() << ' ';
    std::cout << std::endl;

    return 0;
}
catch (std::exception& e) {
    std::cout << "some unhappy happened...  " << std::this_thread::get_id() << " " << e.what() << std::endl;
}
