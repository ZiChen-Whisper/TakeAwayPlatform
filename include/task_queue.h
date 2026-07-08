// ============================================================
// 【本文件概述】
// 线程安全的任务队列（Task Queue）
// 
// 什么是任务队列？
// 它就像一个"待办事项清单"，任务被放进去排队，工作线程从里面取任务执行
// 
// 核心概念——生产者-消费者模型：
// - 生产者（Producer）：往里添加任务的人（比如 HTTP 请求来了，生成一个处理任务）
// - 消费者（Consumer）：从里面取任务执行的人（工作线程）
// - 队列（Queue）：生产者和消费者之间的"缓冲区"
// 
// 现实类比：
// 餐厅厨房的"订单队列"——服务员（生产者）把订单贴上去，
// 厨师（消费者）按顺序取订单做菜
// 
// 【线程安全】
// 整个队列用互斥锁（mutex）和条件变量（condition_variable）保护，
// 多个线程同时操作也不会出问题
// ============================================================

#pragma once  // 防止头文件重复包含

/*
 * 需要的头文件
 */
#include <queue>               // C++ 标准库的队列容器（FIFO：先进先出）
#include <mutex>               // 互斥锁（保护共享数据不被多个线程同时修改）
#include <condition_variable>  // 条件变量（让线程等待某个条件满足再继续）
#include <functional>          // std::function —— 可以"包装"任意可调用对象


namespace TakeAwayPlatform
{
    /*
     * TaskQueue 类 —— 线程安全的任务队列
     * 
     * 关键特性：
     * 1. push() 时自动唤醒一个等待的消费者线程
     * 2. pop() 时如果队列为空，自动阻塞等待直到有任务
     * 3. 所有操作都是线程安全的
     */
    class TaskQueue 
    {
    public:
        /*
         * 【类型别名：using Task = std::function<void()>】
         * 
         * using 在这里不是"使用"的意思，而是定义"类型别名"
         * 类似古代的 typedef，但 using 语法更清晰
         * 
         * std::function<void()> 是什么？
         * std::function 是一个"万能函数包装器"，可以存储任何"可调用对象"
         * 包括：普通函数、lambda 表达式、函数对象等
         * 
         * 模板参数 <void()> 的含义：
         * - void  = 返回值类型（无返回值）
         * - ()    = 参数列表（无参数）
         * 
         * 所以 Task 就是一个"无参数、无返回值"的可调用对象
         * 以后写 Task 就等于写 std::function<void()>，更简洁
         */
        using Task = std::function<void()>;

        /*
         * 【push —— 往队列添加任务（生产者操作）】
         * 
         * 参数 task 的类型是 Task（即 std::function<void()>）
         * 参数名就叫 task，表示"要执行的任务"
         */
        void push(Task task) {
            /*
             * 这里有一对额外的大括号 {}，它的作用是：
             * 创建一个"作用域"，让 lock_guard 的生命周期只在这个 {} 内
             * 
             * 为什么需要这个？
             * lock_guard 在构造时加锁，在离开作用域（遇到 }）时自动解锁
             * 我们希望 notify_one() 在解锁之后调用
             * 如果 notify_one() 在锁内调用，被唤醒的线程可能又要等锁，浪费性能
             * 这种技巧叫"缩小锁的粒度"
             */
            {
                /*
                 * std::lock_guard<std::mutex> lock(mtx);
                 * lock_guard 是 RAII 风格的自动锁（详见 log.h 的注释）
                 * 构造时自动 lock(mtx)，析构时自动 unlock(mtx)
                 * 
                 * 这里必须加锁，因为 push 会修改 tasks 队列
                 * 如果多个线程同时 push，不加锁会导致数据损坏
                 */
                std::lock_guard<std::mutex> lock(mtx);

                /*
                 * std::move(task) —— 移动语义（C++11 重要特性）
                 * 
                 * 普通的赋值/传参会"拷贝"一份数据，产生额外开销
                 * std::move 是"移动"，把数据的所有权转移过去，不拷贝
                 * 
                 * 类比：搬家时把家具搬过去（move），而不是照着家具做一套新的（copy）
                 * 
                 * 用了 move 之后，原来的 task 就不应该再使用了（内容已经被"掏空"）
                 */
                tasks.push(std::move(task));

                // lock_guard 在这里自动析构，mtx 自动解锁
            }

            /*
             * condition.notify_one() —— 唤醒一个等待的线程
             * 
             * condition 是条件变量（condition_variable）
             * 当队列从空变成非空时，需要通知消费者："有任务了，快来取！"
             * 
             * notify_one() 唤醒一个等待的线程
             * notify_all() 唤醒所有等待的线程（线程池析构时用）
             * 
             * 这里只需要唤醒一个，因为一个任务只需要一个线程处理
             */
            condition.notify_one();
        }

        /*
         * 【pop —— 从队列取出任务（消费者操作）】
         * 
         * 返回值类型是 Task，即取出的任务
         * 
         * 关键行为：如果队列为空，这个函数会"阻塞"（block）
         * ——当前线程暂停执行，直到有任务被 push 进来
         */
        Task pop() {
            /*
             * std::unique_lock —— 比 lock_guard 更灵活的锁
             * 
             * 和 lock_guard 的区别：
             * 1. unique_lock 可以手动 lock() 和 unlock()
             * 2. unique_lock 可以和条件变量配合使用
             * 3. unique_lock 稍微重一点（内存和性能开销略大）
             * 
             * 条件变量 wait() 必须用 unique_lock，不能用 lock_guard
             */
            std::unique_lock<std::mutex> lock(mtx);

            /*
             * condition.wait(lock, 条件) —— 条件变量的核心操作
             * 
             * 它做了什么？
             * 1. 检查"条件"是否为 true
             * 2. 如果条件为 true：继续执行（不阻塞）
             * 3. 如果条件为 false：
             *    a. 自动 unlock(mtx)   ——释放锁，让其他线程能操作队列
             *    b. 当前线程休眠        ——等待 notify
             *    c. 被唤醒后自动 lock(mtx) ——重新获取锁
             *    d. 再次检查条件         ——防止"虚假唤醒"
             * 
             * 这里的条件是：!tasks.empty() —— 队列不为空
             * 
             * [this] { return !tasks.empty(); } 是一个 lambda 表达式：
             * - [this] 是"捕获列表"：让 lambda 内部能访问当前对象的成员
             * - { return !tasks.empty(); } 是函数体：返回队列是否非空
             */
            condition.wait(lock, [this] { return !tasks.empty(); });
            
            /*
             * 执行到这里时，队列一定不为空（因为 wait 保证了这一点）
             * 
             * tasks.front() 返回队列的第一个元素（最早进入的）
             * std::move() 把任务"移动"出来，避免拷贝
             */
            Task task = std::move(tasks.front());
            tasks.pop();    // 从队列中移除这个任务
            return task;    // 返回任务给调用者（线程池的 worker_thread 会执行它）
        }

        /*
         * 【empty —— 查询队列是否为空】
         * 
         * const 放在函数后面（不是前面！）表示这个函数是"const 成员函数"
         * const 成员函数保证不会修改对象的任何成员变量
         * 这样的函数可以安全地被多个线程同时调用
         */
        bool empty() const {
            std::lock_guard<std::mutex> lock(mtx);  // 读操作也要加锁！保证读到的是最新数据
            return tasks.empty();
        }

    private:
        /*
         * 【成员变量】
         */
        std::queue<Task> tasks;  // 任务队列，底层是 std::queue<Task>
                                 // queue 是 FIFO（First In First Out）容器
                                 // 先进先出：先来的任务先被处理

        /*
         * mutable std::mutex mtx;
         * 
         * mutable 关键字是什么意思？
         * 正常情况下，const 成员函数不能修改任何成员变量
         * 但 mutable 修饰的变量例外——即使在 const 函数中也能修改
         * 
         * 为什么 mtx 需要 mutable？
         * empty() 是 const 函数，但它需要 lock(mtx)，lock 操作会修改 mtx 的内部状态
         * 如果没有 mutable，编译器会报错
         * 这是一个"逻辑上 const，物理上非 const"的典型案例
         */
        mutable std::mutex mtx;

        /*
         * std::condition_variable —— 条件变量
         * 
         * 用于线程间的"通知-等待"机制
         * 一个线程可以 wait()（等待条件满足）
         * 另一个线程可以 notify_one() 或 notify_all()（通知条件已满足）
         */
        std::condition_variable condition;
    };

}  // namespace TakeAwayPlatform 结束#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>


namespace TakeAwayPlatform
{

    class TaskQueue 
    {
    public:
        using Task = std::function<void()>;

        void push(Task task) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                tasks.push(std::move(task));
            }
            condition.notify_one();
        }

        Task pop() {
            std::unique_lock<std::mutex> lock(mtx);
            condition.wait(lock, [this] { return !tasks.empty(); });
            
            Task task = std::move(tasks.front());
            tasks.pop();
            return task;
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mtx);
            return tasks.empty();
        }

    private:
        std::queue<Task> tasks;
        mutable std::mutex mtx;
        std::condition_variable condition;
    };

} 