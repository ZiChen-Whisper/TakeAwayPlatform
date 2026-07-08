#include <iostream>
#include <thread>
#include <future>

#include "rest_server.h"


namespace TakeAwayPlatform
{
    RestServer::RestServer(const std::string& configPath) : threadPool(std::thread::hardware_concurrency()) 
    {
        std::cout << "RestServer starting." << std::endl;
        std::cout.flush();
        logger.info("RestServer::RestServer starting");

        Json::Value config = load_config(configPath);

        std::cout << "RestServer load config success." << std::endl;
        std::cout.flush();
        
        // 初始化数据库连接池
        init_db_pool(config["database"]);

        std::cout << "RestServer instance created." << std::endl;
        std::cout.flush();
    }

    RestServer::~RestServer() 
    {
        stop();
    }

    void RestServer::start(int port) 
    {
        if (isRunning) {
            std::cerr << "Server is already running." << std::endl;
            std::cout.flush();
            return;
        }
        
        isRunning = true;
        stopRequested = false;
        
        // 成员变量保存线程
        serverThread = std::thread([this, port] {
            this->run_server(port);
        });
        
        std::cout << "Server starting on port " << port << "..." << std::endl;
        std::cout.flush();
    }

    void RestServer::run_server(int port) 
    {
        try 
        {
            // 设置路由
            setup_routes();
            
            std::cout << "HTTP server listening on port " << port << std::endl;
            std::cout.flush();

            if (!server.listen("0.0.0.0", port)) {
                std::cerr << "Failed to start server on port " << port << std::endl;
            }
            
            std::cout << "HTTP server exited listen loop." << std::endl;
        } 
        catch (const std::exception& e) 
        {
            std::cerr << "Server error in worker thread: " << e.what() << std::endl;
            std::cout.flush();
        }
        
        // 服务器已停止，更新状态
        isRunning = false;
        
        // 通知等待的线程
        stopCv.notify_one();
        std::cout << "Server worker thread exiting." << std::endl;
        std::cout.flush();
    }

    void RestServer::stop() 
    {
        if (!isRunning) {
            std::cout << "Server already stop." << std::endl;
            std::cout.flush();
            return;
        }
        
        std::cout << "Requesting server stop..." << std::endl;
        std::cout.flush();
        stopRequested = true;
        
        // 通知服务器停止
        server.stop();
        
        // 等待服务器实际停止
        std::unique_lock<std::mutex> lock(stopMtx);
        if (stopCv.wait_for(lock, std::chrono::seconds(5), [this] {
            return !isRunning;
        }))
        {
            if (serverThread.joinable()) 
            {
                serverThread.join();
            }

            std::cout << "Server stopped successfully." << std::endl;
            std::cout.flush();
        } 
        else 
        {
            std::cerr << "Warning: Server did not stop within timeout." << std::endl;
            std::cout.flush();

            if (serverThread.joinable()) 
            {
                serverThread.detach(); // 最后手段，避免死锁
            }
        }

        // 清理数据库连接池
        std::lock_guard<std::mutex> dbLock(dbPoolMutex);
        while (!dbPool.empty()) {
            dbPool.pop();
        }
    }

    bool RestServer::is_running() const 
    { 
        return isRunning;
    }

    void RestServer::init_db_pool(const Json::Value& config) 
    {
        std::cout << "host: " << config["host"].asString() << std::endl;
        std::cout << "port: " << config["port"].asInt() << std::endl;
        std::cout << "user: " << config["user"].asString() << std::endl;
        std::cout << "password: " << config["password"].asString() << std::endl;
        std::cout << "name: " << config["name"].asString() << std::endl;
        std::cout.flush();
        
        int pool_size = config.get("pool_size", 10).asInt();

        dbConfig.push_back({
            config["host"].asString(),
            config["port"].asInt(),
            config["user"].asString(),
            config["password"].asString(),
            config["name"].asString()
        });

        for (int index = 0; index < pool_size; ++index) {
            dbPool.push(create_db_handler());
        }
    }

    std::unique_ptr<DatabaseHandler> RestServer::create_db_handler() 
    {
        return std::make_unique<DatabaseHandler>(dbConfig[0]);
    }

    std::unique_ptr<DatabaseHandler> RestServer::acquire_db_handler() 
    {
        std::lock_guard<std::mutex> lock(dbPoolMutex);
        if (dbPool.empty()) {
            return create_db_handler();
        }
        
        auto handler = std::move(dbPool.front());
        dbPool.pop();
        return handler;
    }

    void RestServer::release_db_handler(std::unique_ptr<DatabaseHandler> handler) 
    {
        std::lock_guard<std::mutex> lock(dbPoolMutex);
        dbPool.push(std::move(handler));
    }

    void RestServer::setup_routes() 
    {
        // 设置路由
        server.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("TakeAwayPlatform is running!", "text/plain");
        });
        
        server.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
            if (this->is_running() && !this->stopRequested) {
                res.set_content("OK", "text/plain");
            } else {
                res.set_content("SHUTTING_DOWN", "text/plain");
                res.status = 503; // Service Unavailable
            }
        });

        // 示例路由：获取所有菜品（使用线程池处理）
        server.Get("/menu", [&](const httplib::Request&, httplib::Response& res) 
        {
            threadPool.enqueue([this, &res] {
                auto db_handler = acquire_db_handler();
                Json::Value menu = db_handler->query("SELECT * FROM menu_items");
                release_db_handler(std::move(db_handler));
                
                res.set_content(menu.toStyledString(), "application/json");
            });
        });

        // 示例路由：创建订单（使用线程池处理）
        server.Post("/order", [&](const httplib::Request& req, httplib::Response& res) 
        {
            // 包装任务
            auto task_ptr = std::make_shared<std::packaged_task<std::string()>>([this, body = req.body]() {
                std::cout << "/order request body: " << body << std::endl;

                Json::Value order = parse_json(body);
                std::string userName = order["user_name"].asString();
                std::string orderId = order["order_id"].asString();
                std::string productName = order["product_name"].asString();
                double price = order["price"].asDouble();

                std::cout << "/order userName: " << userName << std::endl;
                std::cout << "/order orderId: " << orderId << std::endl;
                std::cout << "/order productName: " << productName << std::endl;
                std::cout << "/order price: " << price << std::endl;

                // todo：数据库处理

                // 直接返回订单数据
                Json::Value response;
                response["user_name"] = userName;
                response["orderId"] = orderId;
                response["productName"] = productName;
                response["price"] = price;
                std::cout << "/order response: " << response.toStyledString() <<std::endl;
                
                return response.toStyledString();
            });

            // 获取future并提交任务
            std::future<std::string> result_future = task_ptr->get_future();
            // 投入线程池
            threadPool.enqueue([task_ptr] {
                (*task_ptr)();
            });

            // 获取结果并设置响应
            try {
                std::string result = result_future.get();
                std::cout << "/order task result: " << result << std::endl;
                res.set_content(result, "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
            }
        });
    }

    Json::Value RestServer::parse_json(const std::string& jsonStr) 
    {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream json_stream(jsonStr);
        
        if (!Json::parseFromStream(builder, json_stream, &root, &errors)) 
        {
            throw std::runtime_error("JSON parse error: " + errors);
        }

        return root;
    }
}