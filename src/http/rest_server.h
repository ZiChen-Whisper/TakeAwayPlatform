#pragma once

#include <httplib.h>

#include "log.h"
#include "common.h"
#include "thread_pool.h"
#include "db_handler.h"


namespace TakeAwayPlatform
{
    class RestServer 
    {
    public:
        RestServer(const std::string& configPath);
        ~RestServer();

        void start(int port);
        void stop();
        bool is_running() const;
        

    private:
        void init_db_pool(const Json::Value& config);  // 初始化数据库连接池

        void run_server(int port);  // 运行服务器

        std::unique_ptr<DatabaseHandler> create_db_handler();  // 创建数据库处理程序

        std::unique_ptr<DatabaseHandler> acquire_db_handler();  // 获取数据库处理程序

        void release_db_handler(std::unique_ptr<DatabaseHandler> handler);  // 释放数据库处理程序

        void setup_routes();  // 设置路由

        Json::Value parse_json(const std::string& jsonStr);  // 解析 JSON 字符串


    private:
        httplib::Server server;
        ThreadPool threadPool;
        std::vector<DBConfig> dbConfig;
        std::mutex dbPoolMutex;
        std::queue<std::unique_ptr<DatabaseHandler>> dbPool;

        std::atomic<bool> isRunning {false};
        std::atomic<bool> stopRequested {false};

        // 用于等待服务器停止的同步对象
        std::mutex stopMtx;
        std::condition_variable stopCv;

        std::thread serverThread;

        // 获取日志实例
        TakeAwayPlatform::Logger& logger = Logger::getInstance();
    };

}