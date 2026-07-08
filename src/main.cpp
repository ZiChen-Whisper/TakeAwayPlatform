// ============================================================
// 【本文件概述】
// 程序的入口点！整个项目从这里开始执行
//
// 每个 C/C++ 程序都必须有一个 int main() 函数
// 它是操作系统启动程序时调度的第一个函数
// 
// 执行流程（小白跟着这个顺序看代码）：
// 1. main() 函数开始
// 2. 注册信号处理函数（捕获 Ctrl+C 等终止信号）
// 3. 初始化日志系统
// 4. 创建 RestServer 对象 → 加载配置 → 初始化数据库连接池
// 5. 启动 HTTP 服务器（在单独线程中）
// 6. 主线程等待终止信号（Ctrl+C 或 kill 命令）
// 7. 收到信号后，优雅关闭服务器
// 8. 退出程序，返回 0 表示正常退出
//
// 【相关文件索引】（阅读顺序推荐）
// main.cpp → rest_server.h/cpp → db_handler.h/cpp
//          → thread_pool.h → task_queue.h
//          → common.h → json_utils.cpp
//          → log.h
// ============================================================

/*
 * 头文件包含
 * 
 * <csignal>  是 C 语言 <signal.h> 的 C++ 版本
 * csignal 提供信号处理相关函数
 * 
 * 信号（Signal）是什么？
 * 信号是操作系统发给进程的"消息"
 * 比如：
 * - SIGINT:  用户按了 Ctrl+C（中断信号）
 * - SIGTERM: 用户用 kill 命令终止进程
 * - SIGQUIT: 用户按了 Ctrl+\（退出信号）
 * - SIGKILL: 强制杀死进程（这个信号不能被捕获！）
 * 
 * std::atomic 提供原子操作类型
 * atomic 保证多线程下对该变量的读写是安全且不可分割的
 * 
 * std::thread 提供线程支持
 * std::condition_variable 提供线程间条件通知机制
 */
#include <csignal>
#include <atomic>
#include <thread>
#include <condition_variable>

#include "log.h"          // 日志系统
#include "common.h"       // 公共定义
#include "rest_server.h"  // REST 服务器


/*
 * 【全局变量】
 *
 * 全局变量（Global Variable）定义在所有函数之外
 * 整个程序的所有代码都能访问它们
 *
 * 一般应尽量少用全局变量，但这里只有 3 个，用于信号处理和主线程同步
 * 这是合理的用法
 */

/*
 * std::atomic<bool> running(true);
 *
 * 原子布尔变量，表示程序是否应该继续运行
 * 初始值为 true（程序正在运行）
 *
 * 为什么用 atomic？
 * 这个变量在信号处理函数（signal_handler）和主线程（main）中都会读写
 * 普通 bool 在多线程并发访问时可能出问题
 * atomic<bool> 保证安全性
 */
std::atomic<bool> running(true);

/*
 * std::mutex mtx;
 * 互斥锁，配合条件变量使用
 */
std::mutex mtx;

/*
 * std::condition_variable cv;
 * 条件变量，主线程用它来等待信号（避免忙等待占用 CPU）
 */
std::condition_variable cv;


/*
 * 【信号处理函数】
 *
 * 参数 signal：接收到的信号编号（如 SIGINT=2，SIGTERM=15）
 * 
 * 这个函数在收到信号时被操作系统调用
 * 注意：它运行在信号处理上下文中，不是普通线程
 * 所以里面能做的事情有限（尽量不要调用非信号安全的函数）
 *
 * 这里只做了最安全的操作：
 * 1. 设置 running = false
 * 2. 唤醒主线程
 */
void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    std::cout.flush();

    running = false;  // 通知主循环：可以停止了

    cv.notify_all();  // 唤醒可能阻塞的主线程
                       // 如果主线程正在 cv.wait() 中休眠
                       // 这会让它醒过来，检查 running 是否变为 false
}


/*
 * 【主函数 —— 程序入口】
 *
 * int main() 的返回值：
 * - 0:   程序正常退出
 * - 非0: 程序异常退出（错误码）
 *
 * 操作系统或调用者可以根据返回值判断程序是否正常结束
 */
int main() {
    std::cout << "Entry main.." << std::endl;
    std::cout.flush();

    /*
     * 【注册信号处理函数】
     *
     * struct sigaction sa;
     * sigaction 是 POSIX 标准的结构体，用于描述"如何处理信号"
     *
     * sa.sa_handler = signal_handler;
     * 设置信号的处理函数为我们上面定义的 signal_handler
     *
     * sigemptyset(&sa.sa_mask);
     * 清空信号屏蔽集
     * sa_mask 表示在处理信号期间要屏蔽的额外信号
     * 这里设为空，不屏蔽任何额外信号
     * 
     * sa.sa_flags = 0;
     * 信号处理的标志位
     * 0 表示使用默认行为（无特殊标志）
     *
     * 常见的 sa_flags：
     * - SA_RESTART: 被信号中断的系统调用会自动重启
     *   （本项目中曾因 SA_RESTART 导致 condition_variable::wait 无法被 notify 唤醒的 bug）
     * - SA_NOCLDSTOP: 子进程停止时不产生 SIGCHLD
     * - SA_SIGINFO: 使用 sa_sigaction 而非 sa_handler（可获取更多信息）
     */
    struct sigaction sa;
    sa.sa_handler = signal_handler;     // 设置处理函数
    sigemptyset(&sa.sa_mask);           // 清空信号屏蔽集
    sa.sa_flags = 0;                    // 无特殊标志
    
    /*
     * sigaction(SIGINT, &sa, nullptr);
     *
     * sigaction 函数注册信号处理
     * 参数：信号编号, 处理方式, 旧处理方式（nullptr 表示不关心）
     *
     * SIGINT = Signal Interrupt（信号中断）
     * 用户按 Ctrl+C 时产生
     */
    sigaction(SIGINT, &sa, nullptr);
    
    /*
     * SIGTERM = Signal Terminate（信号终止）
     * kill 命令的默认信号
     * 优雅关闭的标准方式
     */
    sigaction(SIGTERM, &sa, nullptr);
    
    /*
     * SIGQUIT = Signal Quit（信号退出）
     * 用户按 Ctrl+\ 时产生
     * 和 SIGINT 类似但会生成 core dump
     */
    sigaction(SIGQUIT, &sa, nullptr);

    /*
     * try-catch 异常处理
     *
     * try 块中的代码如果有异常抛出
     * 会在对应的 catch 块中处理
     *
     * 这是程序的"安全网"，防止未捕获的异常导致程序崩溃
     */
    try 
    {
        /*
         * 【初始化日志系统】
         *
         * TakeAwayPlatform::Logger& logger = TakeAwayPlatform::Logger::getInstance();
         *
         * 获取 Logger 单例的引用
         * 整个程序只有这一个 Logger 实例
         *
         * & 表示 logger 是引用（别名），不是独立的对象
         * 操作 logger 就是在操作那个唯一的 Logger 实例
         */
        TakeAwayPlatform::Logger& logger = TakeAwayPlatform::Logger::getInstance();

        /*
         * logger.setLogFile("");
         *
         * 设置日志文件路径
         * 传入空字符串 "" 使用默认路径：/opt/TakeAwayPlatform/logs/takeaway.log
         * 也可以传入自定义路径如 "/var/log/myapp.log"
         */
        logger.setLogFile("");

        /*
         * logger.setLogLevel(TakeAwayPlatform::Logger::Level::DEBUG);
         *
         * 设置日志级别为 DEBUG（最低级别，所有日志都输出）
         *
         * 生产环境建议设为 INFO 或 WARNING：
         * logger.setLogLevel(TakeAwayPlatform::Logger::Level::INFO);
         */
        logger.setLogLevel(TakeAwayPlatform::Logger::Level::DEBUG);

        /*
         * 【创建 REST 服务器】
         *
         * TakeAwayPlatform::RestServer restSrv("/opt/TakeAwayPlatform/config/config.json");
         *
         * 创建 RestServer 对象，传入配置文件路径
         *
         * 构造过程中会：
         * 1. 加载并解析配置文件
         * 2. 创建线程池（线程数 = CPU 核心数）
         * 3. 初始化数据库连接池（创建 pool_size 个数据库连接）
         *
         * 如果构造失败（如配置文件不存在），会抛出异常
         */
        TakeAwayPlatform::RestServer restSrv("/opt/TakeAwayPlatform/config/config.json");

        std::cout << "Starting server on port 9090..." << std::endl;
        std::cout.flush();
        logger.debug("Starting server on port 9090...");
        
        /*
         * 【启动服务器】
         *
         * restSrv.start(9090);
         *
         * 在独立的线程中启动 HTTP 服务器，监听 9090 端口
         *
         * start() 是"非阻塞"的：
         * 它创建线程后立即返回，HTTP 服务器在后台运行
         * 所以 main 函数可以继续执行后面的代码
         */
        restSrv.start(9090);
        
        /*
         * 【等待终止信号（使用条件变量，避免忙等待）】
         *
         * 这里有一段独立的 { } 代码块，目的是：
         * 让 lock 的生命周期仅限于这个 {} 内
         * 出了这个 {} 后 lock 自动解锁并销毁
         *
         * 如果不加 {}，锁会一直持有到 main 函数末尾
         * 可能影响 restSrv.stop() 中的同步操作
         */
        {
            /*
             * std::unique_lock<std::mutex> lock(mtx);
             *
             * 创建 unique_lock 并锁定 mutex
             * unique_lock 可以和条件变量配合使用
             */
            std::unique_lock<std::mutex> lock(mtx);

            /*
             * cv.wait(lock, [&]{ return !running || !restSrv.is_running(); });
             *
             * 等待条件满足：
             * - !running:        信号处理函数设置了 running = false
             * - !restSrv.is_running(): 服务器意外停止了
             *
             * [&] 捕获列表：按引用捕获所有外部变量
             * 可以访问 running 和 restSrv
             *
             * wait() 会释放锁并休眠，直到：
             * 1. 被 cv.notify_all() 唤醒（信号处理函数调用）
             * 2. 被"虚假唤醒"（操作系统原因，很小概率）
             *
             * 唤醒后重新获取锁，检查条件：
             * - 条件为 true → 继续执行
             * - 条件为 false → 继续等待
             */
            cv.wait(lock, [&]{
                return !running || !restSrv.is_running();
            });
        }
        // 锁在这里自动释放（unique_lock 析构）
        
        /*
         * 检测服务器是否意外停止
         *
         * 如果 !running 还是 true（没有收到信号）
         * 但 restSrv 已经停了 → 说明服务器崩溃了
         */
        if (!restSrv.is_running()) {
            std::cerr << "Server thread has stopped unexpectedly!" << std::endl;
        }
        
        /*
         * 【优雅关闭服务器】
         */
        std::cout << "Shutting down server..." << std::endl;
        std::cout.flush();
        restSrv.stop();  // 停止 HTTP 服务器
        
        /*
         * 【等待服务器完全停止】
         *
         * 轮询等待：每秒检查一次，最多等 10 秒
         *
         * auto 自动推导类型
         * std::chrono::steady_clock 是单调时钟（不受系统时间调整影响）
         * std::chrono::seconds(10) 创建 10 秒的时间段
         */
        const auto timeout = std::chrono::seconds(10);      // 超时时间：10 秒
        const auto start = std::chrono::steady_clock::now(); // 记录开始时间
        
        /*
         * while 循环：只要服务器还在运行 且 还没超时
         *
         * && 是"逻辑与"：两个条件都成立才继续循环
         */
        while (restSrv.is_running() && 
               (std::chrono::steady_clock::now() - start) < timeout) 
        {
            /*
             * std::this_thread::sleep_for(std::chrono::milliseconds(100));
             *
             * 让当前线程休眠 100 毫秒（0.1 秒）
             *
             * 如果不 sleep，这个 while 循环会疯狂执行，CPU 100%
             * sleep 让出 CPU 给其他线程/进程，这是"友好"的做法
             */
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 如果超时后还在运行
        if (restSrv.is_running()) {
            std::cerr << "Warning: Server did not stop within timeout" << std::endl;
        }
    } catch (const std::exception& e) {
        /*
         * 捕获所有标准异常
         *
         * 如果 RestServer 构造函数或运行时抛出异常
         * 会在这里捕获并打印错误信息
         *
         * return 1; 表示程序异常退出
         * shell 中可以用 echo $? 查看上一个程序的退出码
         */
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;  // 返回非零值表示异常退出
    }

    std::cout << "Server shutdown complete." << std::endl;
    std::cout.flush();
    
    /*
     * return 0;
     *
     * main 函数的最后一行
     * 返回 0 表示程序正常退出
     * 这是操作系统约定：0 = 一切正常
     */
    return 0;
}