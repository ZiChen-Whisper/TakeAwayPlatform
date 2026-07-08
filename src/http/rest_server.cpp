// ============================================================
// 【本文件概述】
// REST 服务器（RestServer）的实现文件
//
// 这个文件包含了 HTTP 服务器的全部核心逻辑：
// - 服务器生命周期管理（启动、运行、停止）
// - 数据库连接池管理
// - HTTP 路由设置（URL → 处理函数）
// - JSON 解析
// ============================================================

#include <iostream>  // 标准输入输出
#include <thread>    // 线程支持（std::thread、sleep_for 等）
#include <future>    // 异步结果（std::future、std::packaged_task）

#include "rest_server.h"  // 本文件对应的头文件

// 服务层头文件
#include "user_service.h"
#include "dish_service.h"
#include "category_service.h"
#include "cart_service.h"
#include "address_service.h"
#include "order_service.h"
#include "auth_middleware.h"
#include "response_utils.h"


namespace TakeAwayPlatform
{
    /*
     * 【构造函数 —— 初始化服务器】
     *
     * 初始化列表 : threadPool(std::thread::hardware_concurrency())
     * 表示在构造函数的 { 执行之前，先用硬件并发数初始化线程池
     * hardware_concurrency() 返回 CPU 核心数（如 8 核就返回 8）
     * 意味着创建 8 个工作线程
     * 
     * 参数 configPath：配置文件路径
     */
    RestServer::RestServer(const std::string& configPath) : threadPool(std::thread::hardware_concurrency()) 
    {
        std::cout << "RestServer starting." << std::endl;
        std::cout.flush();  // 强制刷新输出缓冲区
        logger.info("RestServer::RestServer starting");  // 同时写入日志

        // 加载配置文件（调用 json_utils.cpp 中的 load_config 函数）
        Json::Value config = load_config(configPath);

        std::cout << "RestServer load config success." << std::endl;
        std::cout.flush();
        
        // 初始化数据库连接池（使用配置中的 "database" 部分）
        // config["database"] 获取 JSON 中 database 键对应的子对象
        init_db_pool(config["database"]);

        // 读取 JWT 配置
        if (config.isMember("jwt")) {
            jwtSecret_ = config["jwt"].get("secret", "default-secret").asString();
            jwtExpireHours_ = config["jwt"].get("expire_hours", 24).asInt();
        }

        std::cout << "RestServer instance created." << std::endl;
        std::cout.flush();
    }

    /*
     * 【析构函数 —— 清理资源】
     *
     * 析构时自动调用 stop() 停止服务器
     * 这样即使忘记手动 stop()，对象销毁时也会自动停止
     */
    RestServer::~RestServer() 
    {
        stop();
    }

    /*
     * 【启动服务器】
     *
     * 参数 port：监听的端口号
     * 
     * 做了什么：
     * 1. 检查是否已在运行（防止重复启动）
     * 2. 创建新线程，在该线程中启动 HTTP 服务器
     * 3. 主线程立即返回（非阻塞）
     */
    void RestServer::start(int port) 
    {
        // 防止重复启动
        if (isRunning) {
            std::cerr << "Server is already running." << std::endl;
            std::cout.flush();
            return;  // 直接返回，不做任何操作
        }
        
        // 设置运行状态
        isRunning = true;
        stopRequested = false;  // 重置停止请求标志
        
        /*
         * serverThread = std::thread([this, port] {
         *     this->run_server(port);
         * });
         *
         * 创建一个新线程来运行 HTTP 服务器
         *
         * [this, port] 是 lambda 的捕获列表：
         * - this:  捕获当前对象的指针（让 lambda 能访问成员变量和函数）
         * - port:  捕获 port 参数的值（按值捕获，复制一份）
         *
         * { this->run_server(port); } 是 lambda 的函数体：
         * 调用 run_server 成员函数
         *
         * serverThread = ... 把线程对象保存到成员变量
         * 这样后续可以 join() 或 detach()
         */
        serverThread = std::thread([this, port] {
            this->run_server(port);
        });
        
        std::cout << "Server starting on port " << port << "..." << std::endl;
        std::cout.flush();
    }

    /*
     * 【运行 HTTP 服务器 —— 在独立线程中执行】
     *
     * 这个函数被 serverThread 调用，是 HTTP 服务器的主循环
     * 它会阻塞当前线程，持续等待客户端连接
     * 直到 server.stop() 被调用才会返回
     */
    void RestServer::run_server(int port) 
    {
        try 
        {
            // 首先设置路由（URL 到处理函数的映射）
            setup_routes();
            
            std::cout << "HTTP server listening on port " << port << std::endl;
            std::cout.flush();

            /*
             * server.listen("0.0.0.0", port);
             *
             * 开始监听端口！
             * 
             * "0.0.0.0" 是什么意思？
             * 这是一个特殊的 IP 地址，表示"监听本机所有网络接口"
             * 如果用 "127.0.0.1"，只能本机访问
             * 用 "0.0.0.0"，局域网内其他设备也能访问（如手机、其他电脑）
             *
             * listen() 会阻塞当前线程，直到 server.stop() 被调用
             * 返回值：true = 正常退出，false = 启动失败
             */
            if (!server.listen("0.0.0.0", port)) {
                std::cerr << "Failed to start server on port " << port << std::endl;
            }
            
            std::cout << "HTTP server exited listen loop." << std::endl;
        } 
        catch (const std::exception& e)  // 捕获所有标准异常
        {
            std::cerr << "Server error in worker thread: " << e.what() << std::endl;
            std::cout.flush();
        }
        
        // 服务器已停止（listen 返回了），更新状态
        isRunning = false;
        
        /*
         * stopCv.notify_one();
         *
         * 通知可能在 stop() 中等待的主线程：
         * "我已经停下来了，你可以继续了"
         */
        stopCv.notify_one();
        std::cout << "Server worker thread exiting." << std::endl;
        std::cout.flush();
    }

    /*
     * 【停止服务器】
     *
     * 这是"优雅关闭"（Graceful Shutdown）的实现
     * 
     * 步骤：
     * 1. 检查是否在运行
     * 2. 调用 server.stop() 通知 httplib 停止监听
     * 3. 等待服务器线程结束（最多 5 秒）
     * 4. 超时则强制 detach（断开线程）
     * 5. 清理数据库连接池
     */
    void RestServer::stop() 
    {
        if (!isRunning) {
            std::cout << "Server already stop." << std::endl;
            std::cout.flush();
            return;
        }
        
        std::cout << "Requesting server stop..." << std::endl;
        std::cout.flush();
        stopRequested = true;  // 标记已请求停止（健康检查接口会用到）
        
        // 通知 httplib 停止监听（这会让 server.listen() 返回）
        server.stop();
        
        /*
         * 等待服务器线程停止（使用条件变量，最多等 5 秒）
         *
         * std::unique_lock<std::mutex> lock(stopMtx);
         * 创建 unique_lock 并锁定 stopMtx
         *
         * stopCv.wait_for(lock, std::chrono::seconds(5), [this] {
         *     return !isRunning;
         * })
         *
         * wait_for 的含义：
         * - 等待条件 !isRunning 成立（即服务器已停止）
         * - 如果 5 秒后条件还没成立，也继续执行
         *
         * 返回值：
         * - true:  条件成立（服务器正常停止了）
         * - false: 超时（5秒到了服务器还没停）
         *
         * std::chrono::seconds(5) 表示 5 秒的时间段
         * chrono 是 C++11 引入的时间库
         */
        std::unique_lock<std::mutex> lock(stopMtx);
        if (stopCv.wait_for(lock, std::chrono::seconds(5), [this] {
            return !isRunning;
        }))
        {
            // 条件满足：服务器正常停止了
            // joinable() 检查线程是否可加入（还有效）
            if (serverThread.joinable()) 
            {
                /*
                 * serverThread.join();
                 *
                 * join() 等待线程结束
                 * 如果线程还在运行，join() 会阻塞直到线程退出
                 *
                 * join 之后线程对象不再管理任何线程
                 * 不能再 join 或 detach
                 */
                serverThread.join();
            }

            std::cout << "Server stopped successfully." << std::endl;
            std::cout.flush();
        } 
        else 
        {
            // 超时！服务器没有在 5 秒内停止
            std::cerr << "Warning: Server did not stop within timeout." << std::endl;
            std::cout.flush();

            if (serverThread.joinable()) 
            {
                /*
                 * serverThread.detach();
                 *
                 * detach() 把线程和线程对象"断开"
                 * 线程继续在后台运行，但线程对象不再管理它
                 *
                 * 这是"最后手段"，尽量避免使用
                 * detach 后线程会变成"孤儿线程"
                 * 程序退出时操作系统会回收它
                 */
                serverThread.detach();
            }
        }

        // 清理数据库连接池（加锁保护）
        std::lock_guard<std::mutex> dbLock(dbPoolMutex);

        // 清空连接池
        // !dbPool.empty() 检查队列是否非空
        while (!dbPool.empty()) {
            dbPool.pop();  // 弹出并销毁池中的每个连接
                           // unique_ptr 自动释放 DatabaseHandler 对象
        }
    }

    /*
     * 【查询运行状态】
     *
     * const 放在函数后面，表示不修改成员变量
     * 
     * 直接返回原子变量 isRunning 的值
     * atomic 变量的读取是线程安全的
     */
    bool RestServer::is_running() const 
    { 
        return isRunning;
    }

    /*
     * 【初始化数据库连接池】
     *
     * 参数 config：JSON 中 "database" 子对象
     * 例如：
     * {
     *     "host": "127.0.0.1",
     *     "port": 3306,
     *     "user": "root",
     *     "password": "123",
     *     "name": "TakeAwayDatabase",
     *     "pool_size": 10
     * }
     */
    void RestServer::init_db_pool(const Json::Value& config) 
    {
        // 调试输出：打印数据库连接参数
        std::cout << "host: " << config["host"].asString() << std::endl;
        std::cout << "port: " << config["port"].asInt() << std::endl;
        std::cout << "user: " << config["user"].asString() << std::endl;
        std::cout << "password: " << config["password"].asString() << std::endl;
        std::cout << "name: " << config["name"].asString() << std::endl;
        std::cout.flush();
        
        /*
         * config.get("pool_size", 10).asInt();
         *
         * get("pool_size", 10) 的含义：
         * - 获取 pool_size 字段的值
         * - 如果字段不存在，使用默认值 10
         *
         * 这是一种"防御性编程"：配置文件可能不完整，给个默认值保证程序能运行
         */
        int pool_size = config.get("pool_size", 10).asInt();

        /*
         * dbConfig.push_back({...})
         *
         * push_back 往 vector 末尾添加一个元素
         * {...} 是"列表初始化"（C++11）
         * 编译器会根据 DBConfig 结构体的字段顺序自动匹配
         *
         * 等价于：
         * DBConfig cfg;
         * cfg.host = config["host"].asString();
         * cfg.port = config["port"].asInt();
         * ...
         * dbConfig.push_back(cfg);
         */
        dbConfig.push_back({
            config["host"].asString(),
            config["port"].asInt(),
            config["user"].asString(),
            config["password"].asString(),
            config["name"].asString()
        });

        /*
         * 创建 pool_size 个数据库连接，放入连接池
         */
        for (int index = 0; index < pool_size; ++index) {
            dbPool.push(create_db_handler());  // 创建并加入池中
        }
    }

    /*
     * 【创建数据库处理程序 —— 工厂方法】
     *
     * std::make_unique<DatabaseHandler>(dbConfig[0])
     *
     * make_unique 是 C++14 引入的工具函数
     * 它创建一个 unique_ptr，并自动用给定参数构造对象
     *
     * 等价于：
     * std::unique_ptr<DatabaseHandler>(new DatabaseHandler(dbConfig[0]))
     * 但 make_unique 更简洁、更安全
     *
     * dbConfig[0] 取第一个数据库配置（当前只有一个）
     */
    std::unique_ptr<DatabaseHandler> RestServer::create_db_handler() 
    {
        return std::make_unique<DatabaseHandler>(dbConfig[0]);
    }

    /*
     * 【从连接池获取数据库处理程序】
     *
     * 返回 unique_ptr<DatabaseHandler>
     * 调用者拥有返回对象的所有权，用完后需归还
     *
     * 如果不能归还（如连接已失效），unique_ptr 会自动销毁对象
     */
    std::unique_ptr<DatabaseHandler> RestServer::acquire_db_handler() 
    {
        std::lock_guard<std::mutex> lock(dbPoolMutex);  // 加锁保护连接池

        // 如果池空了，创建一个新的（可能需要归还，取决于使用方式）
        if (dbPool.empty()) {
            return create_db_handler();
        }
        
        /*
         * auto handler = std::move(dbPool.front());
         *
         * std::move 把池中最前面的连接"移动"出来
         * 移动后池中就少了一个连接
         * front() 返回队列第一个元素的引用
         */
        auto handler = std::move(dbPool.front());
        dbPool.pop();  // 从队列中移除该元素
        return handler;
    }

    /*
     * 【归还数据库处理程序到连接池】
     *
     * 把用完的连接放回池中，供下次请求使用
     *
     * 参数 handler：要归还的数据库连接
     * 使用 std::move 转移所有权到池中
     */
    void RestServer::release_db_handler(std::unique_ptr<DatabaseHandler> handler) 
    {
        std::lock_guard<std::mutex> lock(dbPoolMutex);  // 加锁保护
        dbPool.push(std::move(handler));  // 把连接放回池中
    }

    /*
     * 【设置 HTTP 路由】
     *
     * 路由（Route）：定义"URL → 处理函数"的映射关系
     *
     * 格式：server.方法("路径", 处理函数)
     * 方法可以是 Get、Post、Put、Delete 等
     *
     * 处理函数是一个 lambda 表达式（或函数），接收：
     * - Request:  客户端发来的请求（包含 URL、headers、body 等）
     * - Response: 要返回给客户端的响应（可以设置内容和状态码）
     */
    void RestServer::setup_routes() 
    {
        // ==========================================
        // 公共路由
        // ==========================================
        server.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("TakeAwayPlatform is running!", "text/plain");
        });
        
        server.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
            if (this->is_running() && !this->stopRequested) {
                res.set_content("OK", "text/plain");
            } else {
                res.set_content("SHUTTING_DOWN", "text/plain");
                res.status = 503;
            }
        });

        // ==========================================
        // 阶段一：用户与鉴权模块
        // ==========================================

        // POST /api/user/register - 用户注册
        server.Post("/api/user/register", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                Json::Value body = parse_json(req.body);
                Json::Value resp = UserService::registerUser(
                    [this]() { return acquire_db_handler(); }, body, jwtSecret_);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // POST /api/user/login - 用户登录
        server.Post("/api/user/login", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                Json::Value body = parse_json(req.body);
                Json::Value resp = UserService::login(
                    [this]() { return acquire_db_handler(); }, body, jwtSecret_, jwtExpireHours_);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // GET /api/user/profile - 获取个人信息
        server.Get("/api/user/profile", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int userId = payload["user_id"].asInt();
            Json::Value resp = UserService::getProfile(
                [this]() { return acquire_db_handler(); }, userId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // PUT /api/user/profile - 修改个人信息
        server.Put("/api/user/profile", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            try {
                Json::Value body = parse_json(req.body);
                int userId = payload["user_id"].asInt();
                Json::Value resp = UserService::updateProfile(
                    [this]() { return acquire_db_handler(); }, userId, body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // ==========================================
        // 阶段二：菜品与分类模块
        // ==========================================

        // GET /api/dishes - 搜索/筛选菜品
        server.Get("/api/dishes", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int categoryId = req.has_param("categoryId") ? std::stoi(req.get_param_value("categoryId")) : 0;
            std::string keyword = req.has_param("keyword") ? req.get_param_value("keyword") : "";
            int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
            int pageSize = req.has_param("pageSize") ? std::stoi(req.get_param_value("pageSize")) : 10;

            Json::Value resp = DishService::searchDishes(
                [this]() { return acquire_db_handler(); }, categoryId, keyword, page, pageSize);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // GET /api/dishes/{id} - 菜品详情
        server.Get(R"(/api/dishes/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int dishId = std::stoi(req.matches[1]);
            Json::Value resp = DishService::getDishDetail(
                [this]() { return acquire_db_handler(); }, dishId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // GET /api/categories - 分类列表
        server.Get("/api/categories", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int merchantId = req.has_param("merchantId") ? std::stoi(req.get_param_value("merchantId")) : 0;
            Json::Value resp = CategoryService::getCategories(
                [this]() { return acquire_db_handler(); }, merchantId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // 商家端：POST /api/merchant/dishes - 新增菜品
        server.Post("/api/merchant/dishes", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            try {
                Json::Value body = parse_json(req.body);
                int userId = payload["user_id"].asInt();
                // 查询商家ID
                auto db = acquire_db_handler();
                Json::Value merchant = db->queryPrepared(
                    "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
                if (merchant.size() == 0) {
                    Json::Value err = errorResponse(403, "您不是商家");
                    res.set_content(err.toStyledString(), "application/json");
                    return;
                }
                int merchantId = merchant[0]["id"].asInt();
                Json::Value resp = DishService::addDish(
                    [this]() { return acquire_db_handler(); }, merchantId, body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // 商家端：PUT /api/merchant/dishes/{id} - 修改菜品
        server.Put(R"(/api/merchant/dishes/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            try {
                Json::Value body = parse_json(req.body);
                int dishId = std::stoi(req.matches[1]);
                int userId = payload["user_id"].asInt();
                auto db = acquire_db_handler();
                Json::Value merchant = db->queryPrepared(
                    "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
                if (merchant.size() == 0) {
                    Json::Value err = errorResponse(403, "您不是商家");
                    res.set_content(err.toStyledString(), "application/json");
                    return;
                }
                Json::Value resp = DishService::updateDish(
                    [this]() { return acquire_db_handler(); }, dishId, merchant[0]["id"].asInt(), body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // 商家端：DELETE /api/merchant/dishes/{id} - 删除菜品
        server.Delete(R"(/api/merchant/dishes/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            int dishId = std::stoi(req.matches[1]);
            int userId = payload["user_id"].asInt();
            auto db = acquire_db_handler();
            Json::Value merchant = db->queryPrepared(
                "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
            if (merchant.size() == 0) {
                Json::Value err = errorResponse(403, "您不是商家");
                res.set_content(err.toStyledString(), "application/json");
                return;
            }
            Json::Value resp = DishService::deleteDish(
                [this]() { return acquire_db_handler(); }, dishId, merchant[0]["id"].asInt());
            res.set_content(resp.toStyledString(), "application/json");
        });

        // 商家端：PATCH /api/merchant/dishes/{id}/status - 上下架
        server.Patch(R"(/api/merchant/dishes/(\d+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            try {
                Json::Value body = parse_json(req.body);
                int dishId = std::stoi(req.matches[1]);
                int status = body["status"].asInt();
                int userId = payload["user_id"].asInt();
                auto db = acquire_db_handler();
                Json::Value merchant = db->queryPrepared(
                    "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
                if (merchant.size() == 0) {
                    Json::Value err = errorResponse(403, "您不是商家");
                    res.set_content(err.toStyledString(), "application/json");
                    return;
                }
                Json::Value resp = DishService::setDishStatus(
                    [this]() { return acquire_db_handler(); }, dishId, merchant[0]["id"].asInt(), status);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // 商家端：GET /api/merchant/dishes - 商家菜品列表
        server.Get("/api/merchant/dishes", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            int userId = payload["user_id"].asInt();
            auto db = acquire_db_handler();
            Json::Value merchant = db->queryPrepared(
                "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
            if (merchant.size() == 0) {
                Json::Value err = errorResponse(403, "您不是商家");
                res.set_content(err.toStyledString(), "application/json");
                return;
            }
            int categoryId = req.has_param("categoryId") ? std::stoi(req.get_param_value("categoryId")) : 0;
            int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
            int pageSize = req.has_param("pageSize") ? std::stoi(req.get_param_value("pageSize")) : 10;

            Json::Value resp = DishService::getMerchantDishes(
                [this]() { return acquire_db_handler(); }, merchant[0]["id"].asInt(), categoryId, page, pageSize);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // 商家端：GET /api/merchant/categories - 商家分类列表
        server.Get("/api/merchant/categories", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            int userId = payload["user_id"].asInt();
            auto db = acquire_db_handler();
            Json::Value merchant = db->queryPrepared(
                "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
            if (merchant.size() == 0) {
                Json::Value err = errorResponse(403, "您不是商家");
                res.set_content(err.toStyledString(), "application/json");
                return;
            }
            Json::Value resp = CategoryService::getMerchantCategories(
                [this]() { return acquire_db_handler(); }, merchant[0]["id"].asInt());
            res.set_content(resp.toStyledString(), "application/json");
        });

        // 商家端：POST /api/merchant/categories - 新增分类
        server.Post("/api/merchant/categories", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            try {
                Json::Value body = parse_json(req.body);
                int userId = payload["user_id"].asInt();
                auto db = acquire_db_handler();
                Json::Value merchant = db->queryPrepared(
                    "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
                if (merchant.size() == 0) {
                    Json::Value err = errorResponse(403, "您不是商家");
                    res.set_content(err.toStyledString(), "application/json");
                    return;
                }
                Json::Value resp = CategoryService::addCategory(
                    [this]() { return acquire_db_handler(); }, merchant[0]["id"].asInt(), body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // 商家端：PUT /api/merchant/categories/{id} - 修改分类
        server.Put(R"(/api/merchant/categories/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            try {
                Json::Value body = parse_json(req.body);
                int categoryId = std::stoi(req.matches[1]);
                int userId = payload["user_id"].asInt();
                auto db = acquire_db_handler();
                Json::Value merchant = db->queryPrepared(
                    "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
                if (merchant.size() == 0) {
                    Json::Value err = errorResponse(403, "您不是商家");
                    res.set_content(err.toStyledString(), "application/json");
                    return;
                }
                Json::Value resp = CategoryService::updateCategory(
                    [this]() { return acquire_db_handler(); }, categoryId, merchant[0]["id"].asInt(), body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // ==========================================
        // 阶段三：购物车模块
        // ==========================================

        // GET /api/cart - 获取购物车
        server.Get("/api/cart", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int userId = payload["user_id"].asInt();
            Json::Value resp = CartService::getCart(
                [this]() { return acquire_db_handler(); }, userId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // POST /api/cart/add - 加入购物车
        server.Post("/api/cart/add", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            try {
                Json::Value body = parse_json(req.body);
                int userId = payload["user_id"].asInt();
                Json::Value resp = CartService::addToCart(
                    [this]() { return acquire_db_handler(); }, userId, body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // PUT /api/cart/item/{id} - 修改购物车数量
        server.Put(R"(/api/cart/item/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            try {
                Json::Value body = parse_json(req.body);
                int itemId = std::stoi(req.matches[1]);
                int userId = payload["user_id"].asInt();
                Json::Value resp = CartService::updateQuantity(
                    [this]() { return acquire_db_handler(); }, userId, itemId, body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // DELETE /api/cart/item/{id} - 移出购物车
        server.Delete(R"(/api/cart/item/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int itemId = std::stoi(req.matches[1]);
            int userId = payload["user_id"].asInt();
            Json::Value resp = CartService::removeItem(
                [this]() { return acquire_db_handler(); }, userId, itemId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // ==========================================
        // 阶段四：地址簿模块
        // ==========================================

        // GET /api/addresses - 地址列表
        server.Get("/api/addresses", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int userId = payload["user_id"].asInt();
            Json::Value resp = AddressService::getAddresses(
                [this]() { return acquire_db_handler(); }, userId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // POST /api/addresses - 新增地址
        server.Post("/api/addresses", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            try {
                Json::Value body = parse_json(req.body);
                int userId = payload["user_id"].asInt();
                Json::Value resp = AddressService::addAddress(
                    [this]() { return acquire_db_handler(); }, userId, body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // PUT /api/addresses/{id} - 修改地址
        server.Put(R"(/api/addresses/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            try {
                Json::Value body = parse_json(req.body);
                int addressId = std::stoi(req.matches[1]);
                int userId = payload["user_id"].asInt();
                Json::Value resp = AddressService::updateAddress(
                    [this]() { return acquire_db_handler(); }, userId, addressId, body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // DELETE /api/addresses/{id} - 删除地址
        server.Delete(R"(/api/addresses/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int addressId = std::stoi(req.matches[1]);
            int userId = payload["user_id"].asInt();
            Json::Value resp = AddressService::deleteAddress(
                [this]() { return acquire_db_handler(); }, userId, addressId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // PUT /api/addresses/{id}/default - 设为默认
        server.Put(R"(/api/addresses/default/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int addressId = std::stoi(req.matches[1]);
            int userId = payload["user_id"].asInt();
            Json::Value resp = AddressService::setDefault(
                [this]() { return acquire_db_handler(); }, userId, addressId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // ==========================================
        // 阶段五：订单与支付模块
        // ==========================================

        // POST /api/orders - 提交订单
        server.Post("/api/orders", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            try {
                Json::Value body = parse_json(req.body);
                int userId = payload["user_id"].asInt();
                Json::Value resp = OrderService::createOrder(
                    [this]() { return acquire_db_handler(); }, userId, body);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // POST /api/orders/{id}/pay - 模拟支付
        server.Post(R"(/api/orders/(\d+)/pay)", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int orderId = std::stoi(req.matches[1]);
            int userId = payload["user_id"].asInt();
            Json::Value resp = OrderService::mockPayment(
                [this]() { return acquire_db_handler(); }, userId, orderId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // GET /api/orders - 用户订单列表
        server.Get("/api/orders", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int userId = payload["user_id"].asInt();
            int status = req.has_param("status") ? std::stoi(req.get_param_value("status")) : -1;
            int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
            int pageSize = req.has_param("pageSize") ? std::stoi(req.get_param_value("pageSize")) : 10;

            Json::Value resp = OrderService::getUserOrders(
                [this]() { return acquire_db_handler(); }, userId, status, page, pageSize);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // GET /api/orders/{id} - 订单详情
        server.Get(R"(/api/orders/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;

            int orderId = std::stoi(req.matches[1]);
            int userId = payload["user_id"].asInt();
            Json::Value resp = OrderService::getOrderDetail(
                [this]() { return acquire_db_handler(); }, userId, orderId);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // ==========================================
        // 阶段六：商家端订单处理模块
        // ==========================================

        // GET /api/merchant/orders - 商家订单列表
        server.Get("/api/merchant/orders", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

            int userId = payload["user_id"].asInt();
            auto db = acquire_db_handler();
            Json::Value merchant = db->queryPrepared(
                "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
            if (merchant.size() == 0) {
                Json::Value err = errorResponse(403, "您不是商家");
                res.set_content(err.toStyledString(), "application/json");
                return;
            }

            int status = req.has_param("status") ? std::stoi(req.get_param_value("status")) : -1;
            int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
            int pageSize = req.has_param("pageSize") ? std::stoi(req.get_param_value("pageSize")) : 10;

            Json::Value resp = OrderService::getMerchantOrders(
                [this]() { return acquire_db_handler(); }, merchant[0]["id"].asInt(), status, page, pageSize);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // PUT /api/merchant/orders/accept/{id} - 商家接单（regex在末尾）
        server.Put(R"(/api/merchant/orders/accept/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
                if (payload.isNull()) return;
                if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

                int orderId = std::stoi(req.matches[1]);
                int userId = payload["user_id"].asInt();
                auto db = acquire_db_handler();
                Json::Value merchant = db->queryPrepared(
                    "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
                if (merchant.size() == 0) {
                    Json::Value err = errorResponse(403, "您不是商家");
                    res.set_content(err.toStyledString(), "application/json");
                    return;
                }
                Json::Value resp = OrderService::acceptOrder(
                    [this]() { return acquire_db_handler(); }, merchant[0]["id"].asInt(), orderId);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(500, std::string("服务器错误: ") + e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // PUT /api/merchant/orders/complete/{id} - 商家完成（regex在末尾）
        server.Put(R"(/api/merchant/orders/complete/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
                if (payload.isNull()) return;
                if (!AuthMiddleware::requireRole(payload, res, {1, 2})) return;

                int orderId = std::stoi(req.matches[1]);
                int userId = payload["user_id"].asInt();
                auto db = acquire_db_handler();
                Json::Value merchant = db->queryPrepared(
                    "SELECT id FROM merchant WHERE user_id = ?", {std::to_string(userId)});
                if (merchant.size() == 0) {
                    Json::Value err = errorResponse(403, "您不是商家");
                    res.set_content(err.toStyledString(), "application/json");
                    return;
                }
                Json::Value resp = OrderService::completeOrder(
                    [this]() { return acquire_db_handler(); }, merchant[0]["id"].asInt(), orderId);
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(500, std::string("服务器错误: ") + e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // ==========================================
        // 阶段七：管理端模块
        // ==========================================

        // GET /api/admin/users - 管理员查看用户列表
        server.Get("/api/admin/users", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {2})) return;

            int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
            int pageSize = req.has_param("pageSize") ? std::stoi(req.get_param_value("pageSize")) : 20;

            auto db = acquire_db_handler();
            Json::Value list = db->queryPrepared(
                "SELECT id, username, phone, role, status, create_time FROM user ORDER BY id DESC LIMIT ? OFFSET ?",
                {std::to_string(pageSize), std::to_string((page - 1) * pageSize)});
            Json::Value data;
            data["list"] = list;
            data["page"] = page;
            Json::Value resp = successResponse(data);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // PUT /api/admin/users/{id}/status - 冻结/解冻用户
        server.Put(R"(/api/admin/users/status/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {2})) return;

            try {
                Json::Value body = parse_json(req.body);
                int userId = std::stoi(req.matches[1]);
                int status = body["status"].asInt();
                auto db = acquire_db_handler();
                db->executePrepared("UPDATE user SET status = ? WHERE id = ?",
                                    {std::to_string(status), std::to_string(userId)});
                Json::Value resp = successResponse(std::string("操作成功"));
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // GET /api/admin/merchants - 商家审核列表
        server.Get("/api/admin/merchants", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {2})) return;

            auto db = acquire_db_handler();
            Json::Value list = db->query(
                "SELECT m.*, u.username FROM merchant m JOIN user u ON m.user_id = u.id ORDER BY m.create_time DESC");
            Json::Value resp = successResponse(list);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // PUT /api/admin/merchants/{id}/audit - 审核商家
        server.Put(R"(/api/admin/merchants/audit/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {2})) return;

            try {
                Json::Value body = parse_json(req.body);
                int merchantId = std::stoi(req.matches[1]);
                int status = body["status"].asInt(); // 1=通过, 3=拒绝
                auto db = acquire_db_handler();
                db->executePrepared("UPDATE merchant SET status = ? WHERE id = ?",
                                    {std::to_string(status), std::to_string(merchantId)});
                Json::Value resp = successResponse(std::string("审核完成"));
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });

        // GET /api/admin/dashboard - 数据看板
        server.Get("/api/admin/dashboard", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {2})) return;

            auto db = acquire_db_handler();
            Json::Value todayOrders = db->query(
                "SELECT COUNT(*) as cnt FROM `order` WHERE DATE(create_time) = CURDATE()");
            Json::Value todayRevenue = db->query(
                "SELECT COALESCE(SUM(total_amount),0) as total FROM `order` WHERE DATE(create_time) = CURDATE() AND status IN(1,2,3)");
            Json::Value activeUsers = db->query(
                "SELECT COUNT(*) as cnt FROM user WHERE DATE(create_time) = CURDATE()");

            Json::Value data;
            data["todayOrders"] = todayOrders.size() > 0 ? todayOrders[0]["cnt"] : 0;
            data["todayRevenue"] = todayRevenue.size() > 0 ? todayRevenue[0]["total"] : 0;
            data["todayNewUsers"] = activeUsers.size() > 0 ? activeUsers[0]["cnt"] : 0;
            Json::Value resp = successResponse(data);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // GET /api/admin/config - 系统配置
        server.Get("/api/admin/config", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {2})) return;

            auto db = acquire_db_handler();
            Json::Value configs = db->query("SELECT config_key, config_value FROM system_config");
            Json::Value resp = successResponse(configs);
            res.set_content(resp.toStyledString(), "application/json");
        });

        // PUT /api/admin/config - 修改系统配置
        server.Put("/api/admin/config", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Value payload = AuthMiddleware::authenticate(req, res, jwtSecret_);
            if (payload.isNull()) return;
            if (!AuthMiddleware::requireRole(payload, res, {2})) return;

            try {
                Json::Value body = parse_json(req.body);
                std::string key = body["config_key"].asString();
                std::string value = body["config_value"].asString();
                auto db = acquire_db_handler();
                db->executePrepared(
                    "INSERT INTO system_config (config_key, config_value) VALUES (?, ?) "
                    "ON DUPLICATE KEY UPDATE config_value = VALUES(config_value)",
                    {key, value});
                Json::Value resp = successResponse(std::string("配置更新成功"));
                res.set_content(resp.toStyledString(), "application/json");
            } catch (const std::exception& e) {
                Json::Value err = errorResponse(400, e.what());
                res.set_content(err.toStyledString(), "application/json");
            }
        });
    }

    /*
     * 【解析 JSON 字符串】
     *
     * 把 HTTP 请求体中的字符串解析成 JSON 对象
     *
     * 参数 jsonStr：JSON 格式的字符串（如 '{"name":"张三","age":25}'）
     * 返回：解析后的 Json::Value 对象
     * 失败时抛出 std::runtime_error 异常
     */
    Json::Value RestServer::parse_json(const std::string& jsonStr) 
    {
        Json::Value root;  // 用于存放解析结果的 JSON 对象

        /*
         * Json::CharReaderBuilder builder;
         *
         * CharReaderBuilder 是 jsoncpp 的"解析器构建器"
         * 它按照 JSON 标准配置来构建解析器
         * 可以设置是否允许注释、单引号等（默认严格模式）
         */
        Json::CharReaderBuilder builder;

        std::string errors;  // 存放解析错误信息

        /*
         * std::istringstream json_stream(jsonStr);
         *
         * istringstream 是"输入字符串流"
         * 它把一个字符串包装成"流"的形式
         * 
         * 为什么需要转成流？
         * jsoncpp 的 parseFromStream 需要输入流
         * 而我们的数据是字符串，用 istringstream 做适配
         */
        std::istringstream json_stream(jsonStr);
        
        /*
         * Json::parseFromStream(builder, json_stream, &root, &errors)
         *
         * 从流中解析 JSON 数据
         *
         * 参数：
         * - builder:       解析器配置
         * - json_stream:   输入流
         * - &root:         解析结果写入这里（传入地址）
         * - &errors:       错误信息写入这里
         *
         * 返回值：true = 解析成功，false = 解析失败
         *
         * ! 取反：如果解析失败...
         */
        if (!Json::parseFromStream(builder, json_stream, &root, &errors)) 
        {
            /*
             * throw std::runtime_error("JSON parse error: " + errors);
             *
             * throw 是抛出异常的关键字
             * 它会中断当前函数的执行，向上层（调用者）传播
             * 上层的 catch 块会捕获这个异常
             *
             * std::runtime_error 是标准库的运行时错误异常类
             */
            throw std::runtime_error("JSON parse error: " + errors);
        }

        return root;  // 返回解析好的 JSON 对象
    }
}  // namespace TakeAwayPlatform 结束