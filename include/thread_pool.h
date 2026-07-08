// ============================================================
// 【本文件概述】
// 线程池（Thread Pool）的实现
//
// 什么是线程池？
// 想象一个"工人团队"：提前招聘好 N 个工人（线程），
// 有活干的时候从池子里取一个工人，干完活工人回到池子里等下一单
// 而不是每来一单就临时招一个工人（新建线程），这样开销太大
//
// 线程池的优势：
// 1. 重用线程：避免频繁创建/销毁线程的开销
// 2. 控制并发数：限制同时执行的任务数量，防止系统资源耗尽
// 3. 异步处理：把任务丢进线程池后，主线程可以继续做别的事
//
// 本项目中的使用场景：
// HTTP 请求来了 → 生成一个"处理此请求的任务" → 丢进线程池
// → 某个工作线程取出任务 → 执行（查数据库、处理数据等） → 返回结果
// ============================================================

#pragma once  // 防止头文件重复包含

#include <vector>    // std::vector —— 动态数组，用来存放工作线程
#include <thread>    // std::thread —— C++ 的线程类
#include <atomic>    // std::atomic —— 原子操作，保证多线程下数据一致性
#include "task_queue.h"  // 我们自己写的线程安全任务队列


namespace TakeAwayPlatform
{
    /*
     * ThreadPool 类 —— 线程池
     *
     * 工作流程：
     * 1. 构造函数创建 N 个工作线程
     * 2. 每个工作线程在循环中不断从 TaskQueue 取任务并执行
     * 3. enqueue() 把新任务加入 TaskQueue
     * 4. 析构函数通知所有线程停止，并等待它们结束
     */
    class ThreadPool 
    {
    public:
        /*
         * 【构造函数 —— 创建线程池】
         *
         * explicit 关键字：
         * 防止隐式类型转换
         * 比如没有 explicit 的话，ThreadPool tp = 4; 会意外创建一个4线程的线程池
         * 有了 explicit，只能显式地写 ThreadPool tp(4);
         *
         * 默认参数：
         * size_t thread_count = std::thread::hardware_concurrency()
         * 如果调用者不传参数，默认使用 CPU 核心数
         * std::thread::hardware_concurrency() 返回当前系统的 CPU 逻辑核心数
         * 比如 8 核 CPU 就返回 8，意味着一共创建 8 个工作线程
         *
         * size_t 是什么类型？
         * size_t 是 C++ 中表示"大小"的无符号整数类型
         * 在 64 位系统上是 64 位的，保证能表示任意对象的大小
         */
        explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency()) 
            /*
             * 初始化列表（Constructor Initializer List）
             * 
             * 冒号 : 后面跟着的是"初始化列表"
             * running(true) 意思是：把成员变量 running 初始化为 true
             * 
             * 为什么用初始化列表而不是在 { } 里赋值？
             * 1. 效率更高（直接构造，而不是先默认构造再赋值）
             * 2. 有些类型（如引用、const 成员）只能用初始化列表
             */
            : running(true) {

            /*
             * workers.reserve(thread_count);
             * 
             * vector 的 reserve() 方法：
             * 预先分配足够的内存空间，避免后续 push_back 时反复扩容
             * 
             * 如果不 reserve，vector 从 0 开始，每次满了就翻倍扩容
             * 扩容意味着：申请新内存 → 拷贝旧数据 → 释放旧内存
             * 对于线程对象，这个开销很大
             */
            workers.reserve(thread_count);

            /*
             * for 循环创建指定数量的工作线程
             * 
             * 语法解析：
             * for (初始化; 条件; 迭代) { 循环体 }
             * - size_t index = 0:  计数器从 0 开始
             * - index < thread_count: 当 index 小于线程数时继续循环
             * - ++index: 每次循环 index 加 1（前置递增，效率略高于 index++）
             */
            for (size_t index = 0; index < thread_count; ++index) {

                /*
                 * workers.emplace_back([this] { worker_thread(); });
                 * 
                 * emplace_back vs push_back：
                 * - push_back:  先创建对象，再拷贝/移动到 vector 里
                 * - emplace_back: 直接在 vector 内部构造对象，更高效
                 * 
                 * [this] { worker_thread(); } 是一个 lambda 表达式：
                 * - [this] 捕获当前对象的 this 指针（让 lambda 内部能调用成员函数）
                 * - { worker_thread(); } 是 lambda 的函数体：执行 work_thread() 成员函数
                 * 
                 * 每个 std::thread 对象在创建时会启动一个新线程，
                 * 新线程执行传入的 lambda（即调用 worker_thread()）
                 * 
                 * 新线程和主线程是"并发"执行的（操作系统负责调度）
                 */
                workers.emplace_back([this] { worker_thread(); });
            }
        }

        /*
         * 【析构函数 —— 销毁线程池】
         * 
         * 析构函数（Destructor）：
         * - 函数名是 ~类名（波浪号 + 类名）
         * - 对象被销毁时自动调用
         * - 用于做"清理工作"（释放资源、关闭文件、等待线程结束等）
         * 
         * 这里的析构函数做了什么：
         * 1. 通知所有线程"该下班了"
         * 2. 等待所有线程结束
         */
        ~ThreadPool() {
            /*
             * running = false;
             * 
             * std::atomic<bool> running 是原子变量
             * 原子（atomic）的意思是：对这个变量的读写是不可分割的
             * 多个线程同时读写 running 也不会出问题
             * 
             * 把 running 设为 false，工作线程检查到后就会退出循环
             */
            running = false;

            /*
             * condition.notify_all();
             * 
             * 唤醒所有在 TaskQueue::pop() 中等待的线程
             * 如果不 notify，那些线程会一直阻塞在 wait() 上
             * 
             * 注意：这里用的是 ThreadPool 自己的 condition_variable
             * 而不是 TaskQueue 里的 —— 这两者是分开的
             */
            condition.notify_all();

            /*
             * 遍历所有工作线程，等待它们结束
             * 
             * for (auto& worker : workers) 是 C++11 的"范围 for 循环"
             * auto& 自动推导类型为引用，效率高
             */
            for (auto& worker : workers) {
                /*
                 * if (worker.joinable()) worker.join();
                 * 
                 * joinable() 检查线程是否"可加入"（即线程是否还在运行）
                 * join() 等待线程执行完毕
                 * 
                 * 必须先检查 joinable()，否则对一个已结束的线程调用 join() 会崩溃
                 */
                if (worker.joinable()) worker.join();
            }
        }

        /*
         * 【enqueue —— 向线程池提交任务】
         * 
         * template<typename F> 是什么？
         * 这是 C++ 的"模板"（Template）
         * 模板让你写一次代码，适配多种类型
         * 
         * 比如这里：
         * enqueue(一个普通函数)   → F 被推导为函数指针类型
         * enqueue(一个 lambda)     → F 被推导为 lambda 的闭包类型
         * enqueue(一个函数对象)    → F 被推导为函数对象的类型
         * 
         * F&& task 中的 && 是什么？
         * 这是"万能引用"（Universal Reference / Forwarding Reference）
         * 配合模板使用时，&& 不是右值引用，而是万能引用
         * 它可以接收左值或右值，保持参数原有的值类别
         */
        template<typename F>
        void enqueue(F&& task) {
            /*
             * taskQueue.push(std::forward<F>(task));
             * 
             * std::forward —— "完美转发"
             * 把参数原封不动地转发给另一个函数
             * 如果 task 是左值，forward 后仍是左值
             * 如果 task 是右值，forward 后仍是右值
             * 
             * 这和 std::move 不一样：
             * - std::move:  无条件转为右值（不管原来是什么）
             * - std::forward: 有条件转发（保持原来的值类别）
             */
            taskQueue.push(std::forward<F>(task));

            // 通知一个等待的工作线程
            condition.notify_one();
        }

    private:
        /*
         * 【worker_thread —— 工作线程的主循环】
         * 
         * 每个工作线程都在执行这个函数
         * 它是一个"无限循环"，不断从队列取任务然后执行
         * 只有当 running 变为 false 时才退出
         */
        void worker_thread() {
            /*
             * while (running) —— 只要 running 为 true，就一直循环
             * 这是工作线程的"主循环"，也叫"事件循环"（Event Loop）
             */
            while (running) {
                /*
                 * auto task = taskQueue.pop();
                 * 
                 * auto 是 C++11 的"类型自动推导"
                 * 编译器根据返回值自动确定 task 的类型
                 * 等价于：Task task = taskQueue.pop();
                 * 
                 * pop() 会阻塞：如果队列为空，当前线程会在这里休眠等待
                 * 当有新任务 push 进来时，被唤醒并取出任务
                 */
                auto task = taskQueue.pop();

                /*
                 * if (task) task();
                 * 
                 * std::function 对象可以像 bool 一样检查：
                 * - 如果包装了有效函数，返回 true
                 * - 如果为空（nullptr），返回 false
                 * 
                 * task() 是调用这个任务（执行函数/ lambda）
                 * 就像调用普通函数一样
                 */
                if (task) task();
            }
        }

    private:
        /*
         * 【成员变量】
         */
        std::vector<std::thread> workers;  // 工作线程的集合
                                           // vector 就像一个"动态数组"，可以随时添加元素

        TaskQueue taskQueue;  // 任务队列（我们自己写的线程安全队列）
                              // 所有工作线程共享这个队列

        std::atomic<bool> running;  // 是否正在运行（原子变量）
                                    // true  = 继续工作
                                    // false = 停止所有线程
                                    // atomic 保证多线程读写安全

        std::condition_variable condition;  // 条件变量，用于通知工作线程有新任务

        std::mutex mtx;  // 互斥锁（虽然当前未使用，但保留用于扩展）
    };

}  // namespace TakeAwayPlatform 结束

