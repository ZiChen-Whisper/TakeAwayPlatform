// ============================================================
// 【本文件概述】
// RESTful HTTP 服务器（RestServer）的头文件
//
// 什么是 REST（Representational State Transfer）？
// REST 是一种设计 Web API 的风格，使用 HTTP 的各种方法操作资源：
// - GET    /menu     → 获取菜单（查询）
// - POST   /order    → 创建订单（新增）
// - PUT    /order/1  → 更新订单（修改）
// - DELETE /order/1  → 删除订单（删除）
//
// 本项目目前实现了 GET /、GET /health、GET /menu、POST /order 四个接口
//
// 【HTTP 协议基础】
// HTTP（HyperText Transfer Protocol）超文本传输协议
// 是浏览器和服务器之间通信的标准协议
// - 请求（Request）：客户端发给服务器（如"给我菜单"）
// - 响应（Response）：服务器返回给客户端（如"这是菜单数据"）
//
// 【什么是 API？】
// API（Application Programming Interface）应用程序编程接口
// 简单说：前端（网页/App）和后端的"约定"
// 前端说"我要 GET /menu"，后端就返回菜单数据
// ============================================================

#pragma once  // 防止头文件重复包含

#include <httplib.h>  // cpp-httplib —— 一个轻量级的 C++ HTTP 库
                       // 只需要一个头文件！作者是 yhirose
                       // 支持 HTTP/HTTPS、路由、静态文件等

#include "log.h"         // 日志系统
#include "common.h"      // 公共定义（DBConfig 结构体等）
#include "thread_pool.h" // 线程池（异步处理请求）
#include "db_handler.h"  // 数据库操作（执行 SQL 查询）


namespace TakeAwayPlatform
{
    /*
     * RestServer 类 —— HTTP REST 服务器
     *
     * 这个类是整个项目的"大总管"，负责：
     * 1. 启动 HTTP 服务器，监听端口
     * 2. 管理数据库连接池（多个 DB Handler 轮换使用）
     * 3. 管理线程池（异步处理 HTTP 请求）
     * 4. 设置路由（URL 到处理函数的映射）
     * 5. 优雅关闭（等待所有请求处理完再停机）
     */
    class RestServer 
    {
    public:
        /*
         * 【构造函数】
         * 参数：configPath —— 配置文件路径（如 "/opt/TakeAwayPlatform/config/config.json"）
         *
         * 构造时做的事情：
         * 1. 加载配置文件
         * 2. 初始化数据库连接池
         * 3. 创建线程池
         */
        RestServer(const std::string& configPath);

        /*
         * 【析构函数】
         * 对象销毁时自动停止服务器并清理资源
         */
        ~RestServer();

        /*
         * 【启动服务器】
         * 参数：port —— 监听的端口号（如 9090）
         *
         * 启动后会创建一个新线程来运行 HTTP 服务器
         * 主线程不会阻塞，可以继续做其他事
         */
        void start(int port);

        /*
         * 【停止服务器】
         * 优雅关闭：先通知服务器停止接受新请求，等待已有请求处理完毕
         * 超时后强制停止
         */
        void stop();

        /*
         * 【查询服务器是否在运行】
         * 返回 true = 正在运行，false = 已停止
         */
        bool is_running() const;
        

    private:
        /*
         * 【初始化数据库连接池】
         * 参数：config —— JSON 配置对象中的 "database" 部分
         *
         * 数据库连接池是什么？
         * 预先创建一批数据库连接（比如 10 个），放在一个队列里
         * 需要的时候从池中"借"一个，用完了"还"回去
         * 好处：
         * 1. 避免频繁创建/销毁连接（开销大）
         * 2. 限制同时的数据库连接数（保护数据库）
         */
        void init_db_pool(const Json::Value& config);

        /*
         * 【运行 HTTP 服务器】
         * 这个函数在独立线程中执行，调用 httplib 库的 listen() 方法
         * listen() 会阻塞当前线程，持续等待客户端连接
         */
        void run_server(int port);

        /*
         * 【创建数据库处理程序】
         * 工厂方法：创建一个新的 DatabaseHandler 对象
         * 用于初始化连接池或池空时创建临时连接
         */
        std::unique_ptr<DatabaseHandler> create_db_handler();

        /*
         * 【从连接池获取数据库处理程序】
         * 如果池中有可用连接，取出一个
         * 如果池空，创建一个新的
         *
         * 注意：使用完毕后要调用 release_db_handler() 归还
         * 否则连接会泄漏（连接一直被占用，最终用光）
         */
        std::unique_ptr<DatabaseHandler> acquire_db_handler();

        /*
         * 【归还数据库处理程序到连接池】
         * 把用完的 handler 放回池中，供后续请求使用
         */
        void release_db_handler(std::unique_ptr<DatabaseHandler> handler);

        /*
         * 【设置 HTTP 路由】
         * 
         * 路由（Route）就是 URL 路径到处理函数的映射
         * 例如：
         * - GET  /       → 返回欢迎信息
         * - GET  /health → 健康检查
         * - GET  /menu   → 查询菜单
         * - POST /order  → 创建订单
         */
        void setup_routes();

        /*
         * 【解析 JSON 字符串】
         * 参数：jsonStr —— JSON 格式的字符串
         * 返回：解析后的 Json::Value 对象
         *
         * HTTP 请求的 body 是字符串，需要解析成 JSON 对象才能使用
         * 如果解析失败，抛出异常
         */
        Json::Value parse_json(const std::string& jsonStr);

        // JWT 配置访问器
        const std::string& getJwtSecret() const { return jwtSecret_; }
        int getJwtExpireHours() const { return jwtExpireHours_; }


    private:
        /*
         * 【成员变量 —— 服务器核心组件】
         */

        httplib::Server server;    // cpp-httplib 的 HTTP 服务器对象
                                   // 负责监听端口、接收请求、返回响应

        ThreadPool threadPool;     // 线程池（用于异步处理耗时的数据库请求）
                                   // 避免 HTTP 处理线程阻塞，提高并发能力

        /*
         * 【成员变量 —— 数据库连接池相关】
         */

        std::vector<DBConfig> dbConfig;  // 数据库配置列表
                                         // 虽然当前只有一个配置，但设计为列表便于扩展

        std::mutex dbPoolMutex;  // 保护连接池的互斥锁
                                 // 连接池被多个线程共享，加锁保证安全

        std::queue<std::unique_ptr<DatabaseHandler>> dbPool;  // 数据库连接池
                                                               // 存放预先创建的 DatabaseHandler 对象

        /*
         * 【成员变量 —— 状态控制】
         */

        std::atomic<bool> isRunning {false};     // 原子变量：服务器是否正在运行
                                                  // {false} 是 C++11 的"统一初始化"
                                                  // 相当于 isRunning(false)

        std::atomic<bool> stopRequested {false}; // 原子变量：是否请求停止服务器
                                                  // 用于健康检查接口判断服务状态

        /*
         * 【成员变量 —— 线程同步相关】
         * 用于主线程等待服务器线程停止的同步机制
         */

        std::mutex stopMtx;                // 停止等待的互斥锁
        std::condition_variable stopCv;    // 停止等待的条件变量

        std::thread serverThread;  // 运行 HTTP 服务器的独立线程对象
                                   // 服务器在这个线程中运行，不阻塞主线程

        /*
         * 【日志实例的引用】
         *
         * TakeAwayPlatform::Logger& logger = Logger::getInstance();
         *
         * 这是一个"引用成员变量"
         * & 表示这是个引用（别名），不是独立的对象
         * Logger::getInstance() 获取日志单例
         *
         * 为什么用引用？
         * 整个程序只有这一个 Logger，用引用指向它，不需要拷贝
         *
         * 这里在类定义中初始化（C++11 允许非静态成员变量有默认值）
         */
        TakeAwayPlatform::Logger& logger = Logger::getInstance();

        // JWT 配置
        std::string jwtSecret_;
        int jwtExpireHours_ = 24;
    };

}  // namespace TakeAwayPlatform 结束