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
        /*
         * 【路由 1：根路径 GET /】
         *
         * 访问 http://localhost:9090/ 时返回欢迎信息
         *
         * lambda 捕获列表 [](...)：空列表，不需要访问外部变量
         *
         * 参数名用了占位（未命名）：const httplib::Request&
         * 因为我们不需要使用 Request 对象，只需要 Response
         * 不给参数名可以避免"未使用变量"的编译器警告
         *
         * res.set_content("内容", "内容类型")
         * "text/plain" 表示纯文本
         * 其他常见类型：
         *   "text/html"        HTML 网页
         *   "application/json" JSON 数据
         *   "image/png"        PNG 图片
         */
        server.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("TakeAwayPlatform is running!", "text/plain");
        });
        
        /*
         * 【路由 2：健康检查 GET /health】
         *
         * 健康检查是什么？
         * 运维人员/监控系统定期访问这个接口，确认服务是否正常
         * 如果返回 "OK" 说明服务正常，"SHUTTING_DOWN" 说明正在关闭
         *
         * 这个接口也常用于负载均衡器（如 Nginx）
         * 负载均衡器定期检查后端服务是否健康，不健康就停止转发流量
         *
         * [this] 捕获 this 指针，因为需要访问 is_running() 和 stopRequested
         */
        server.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
            if (this->is_running() && !this->stopRequested) {
                res.set_content("OK", "text/plain");
            } else {
                res.set_content("SHUTTING_DOWN", "text/plain");
                /*
                 * res.status = 503;
                 *
                 * HTTP 状态码（Status Code）：
                 * 200 = OK          请求成功
                 * 404 = Not Found   资源不存在
                 * 500 = Internal Server Error  服务器内部错误
                 * 503 = Service Unavailable    服务暂时不可用
                 *
                 * 这里返回 503 告诉客户端/负载均衡器：
                 * "我还在，但现在不能处理请求"
                 */
                res.status = 503; // Service Unavailable
            }
        });

        /*
         * 【路由 3：获取菜单 GET /menu】
         *
         * 访问 http://localhost:9090/menu 返回所有菜品
         *
         * 这个路由使用线程池异步处理：
         * HTTP 请求来了 → 生成任务 → 丢进线程池 → 响应
         * 避免阻塞 HTTP 处理线程
         *
         * [&] 捕获列表：按引用捕获所有外部变量
         * 需要访问 threadPool 成员
         */
        server.Get("/menu", [&](const httplib::Request&, httplib::Response& res) 
        {
            /*
             * threadPool.enqueue([this, &res] { ... });
             *
             * 把处理任务丢进线程池
             * enqueue 后 HTTP 处理函数就返回了（非阻塞）
             * 线程池中的工作线程会实际执行数据库查询和响应设置
             *
             * 警告：[&res] 按引用捕获 res 有风险！
             * 因为 enqueue 返回后，HTTP 处理函数就结束了
             * res 可能被销毁，这时工作线程再访问 res 就出问题
             * 
             * 正确的做法应该用 shared_ptr 延长生命周期（见 /order 路由的做法）
             */
            threadPool.enqueue([this, &res] {
                // 从连接池获取一个数据库连接
                auto db_handler = acquire_db_handler();

                // 执行 SQL 查询，获取菜单数据
                // SELECT * FROM menu_items 意思是"查询 menu_items 表的所有行和列"
                Json::Value menu = db_handler->query("SELECT * FROM menu_items");

                // 归还数据库连接到池中（好借好还~）
                release_db_handler(std::move(db_handler));
                
                // 设置 HTTP 响应内容
                // toStyledString() 把 JSON 对象转成格式化的字符串（带缩进和换行）
                res.set_content(menu.toStyledString(), "application/json");
            });
        });

        /*
         * 【路由 4：创建订单 POST /order】
         *
         * 访问 POST http://localhost:9090/order 
         * 请求体（body）中包含 JSON 格式的订单数据
         *
         * 这个路由展示了正确的异步处理方式：
         * 使用 std::packaged_task + std::future 在线程池中执行，
         * 然后等待结果返回
         *
         * std::packaged_task 是什么？
         * 它把"可调用对象"包装成一个"异步任务"
         * 可以通过 get_future() 获取一个 future 对象
         * future 对象可以在以后获取任务的返回值
         *
         * 类比：你点外卖
         * - packaged_task = 餐厅接到订单（任务被包装好）
         * - future = 外卖订单号（你可以凭借它获取结果）
         * - future.get() = 外卖送到（你拿到结果）
         */
        server.Post("/order", [&](const httplib::Request& req, httplib::Response& res) 
        {
            /*
             * auto task_ptr = std::make_shared<std::packaged_task<std::string()>>(...)
             *
             * 拆解这个复杂声明：
             * - std::make_shared<T>(...) 创建 shared_ptr<T>（共享智能指针）
             * - T = std::packaged_task<std::string()>
             *   packaged_task 的模板参数 <std::string()> 表示：
             *   这个任务返回 std::string，不接收参数
             *
             * 为什么用 make_shared 而非 unique_ptr？
             * 因为 lambda 中捕获了 task_ptr，enqueue 的线程也需要 task_ptr
             * 两个地方都需要持有它，只能用 shared_ptr（共享所有权）
             *
             * [this, body = req.body] 捕获列表：
             * - this: 访问成员函数
             * - body = req.body: 按值捕获 req.body（复制一份，免得 req 被销毁后访问出错）
             */
            auto task_ptr = std::make_shared<std::packaged_task<std::string()>>([this, body = req.body]() {
                std::cout << "/order request body: " << body << std::endl;

                // 解析 HTTP 请求体中的 JSON 数据
                Json::Value order = parse_json(body);

                // 从 JSON 中提取订单信息
                // asString() 把 JSON 值转为字符串
                // asDouble() 把 JSON 值转为浮点数
                std::string userName = order["user_name"].asString();
                std::string orderId = order["order_id"].asString();
                std::string productName = order["product_name"].asString();
                double price = order["price"].asDouble();

                // 调试输出
                std::cout << "/order userName: " << userName << std::endl;
                std::cout << "/order orderId: " << orderId << std::endl;
                std::cout << "/order productName: " << productName << std::endl;
                std::cout << "/order price: " << price << std::endl;

                // todo：数据库处理
                // 目前只是原样返回订单数据，没有存入数据库

                // 构建响应 JSON
                Json::Value response;
                response["user_name"] = userName;
                response["orderId"] = orderId;
                response["productName"] = productName;
                response["price"] = price;
                std::cout << "/order response: " << response.toStyledString() <<std::endl;
                
                return response.toStyledString();  // 返回 JSON 字符串
            });

            /*
             * result_future = task_ptr->get_future();
             *
             * 从 packaged_task 获取一个 future 对象
             * future 就是"未来会有的结果"
             * 当任务执行完毕后，可以通过 future.get() 获取返回值
             */
            std::future<std::string> result_future = task_ptr->get_future();

            /*
             * 将任务投入线程池执行
             *
             * [task_ptr] { (*task_ptr)(); }
             * - 捕获 task_ptr 的 shared_ptr 副本
             * - (*task_ptr)() 调用 packaged_task，执行里面的任务
             *
             * 由于 task_ptr 是 shared_ptr，当它被线程池的 lambda 捕获后
             * 引用计数 +1，确保任务对象在所有持有者释放前不会被销毁
             */
            threadPool.enqueue([task_ptr] {
                (*task_ptr)();  // 执行订单处理任务
            });

            // 等待任务执行完毕，获取结果
            try {
                /*
                 * std::string result = result_future.get();
                 *
                 * future.get() 阻塞当前线程，直到任务执行完毕
                 * 如果任务抛出了异常，get() 会重新抛出该异常
                 * 
                 * 注意：get() 只能调用一次！
                 * 第二次调用会抛出 std::future_error 异常
                 */
                std::string result = result_future.get();
                std::cout << "/order task result: " << result << std::endl;
                res.set_content(result, "application/json");
            } catch (const std::exception& e) {
                // 任务执行出错
                res.status = 500;  // Internal Server Error
                res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
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
}  // namespace TakeAwayPlatform 结束#include <iostream>
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