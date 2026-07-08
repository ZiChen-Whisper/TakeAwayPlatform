// ============================================================
// 【本文件概述】
// 数据库处理器（DatabaseHandler）的实现文件
//
// 这是 db_handler.h 中声明的所有函数的具体实现
// .cpp 文件也叫"源文件"（Source File），包含函数体 { ... }
//
// 【C++ 编译过程简要说明】
// 1. 预处理（Preprocess）：处理 #include, #define 等指令
// 2. 编译（Compile）：   .cpp → .o（目标文件，机器码）
// 3. 链接（Link）：      多个 .o + 库文件 → 可执行文件
//
// 所以 .h 和 .cpp 分开写，.h 声明，.cpp 实现
// 各自编译成 .o，最后链接在一起
// ============================================================

#include <iostream>  // 标准输入输出（std::cout, std::cerr, std::endl）

#include "db_handler.h"  // 本文件对应的头文件（包含类声明）

/*
 * MySQL Connector/C++ 需要用到的头文件
 */
#include <mysql_driver.h>              // MySQL 驱动，获取 MySQL_Driver 实例
#include <cppconn/exception.h>         // MySQL 异常类（sql::SQLException）
#include <cppconn/resultset_metadata.h> // 结果集元数据（列名、列类型等信息）


namespace TakeAwayPlatform
{
    /*
     * 【构造函数 —— 创建对象时自动调用】
     *
     * 语法：类名::函数名(参数)
     * :: 叫"作用域解析运算符"（Scope Resolution Operator）
     * DatabaseHandler::DatabaseHandler 的意思是：
     * "DatabaseHandler 类中的 DatabaseHandler 构造函数"
     * 
     * 这里构造函数只做一件事：调用 connect() 建立数据库连接
     */
    DatabaseHandler::DatabaseHandler(const DBConfig& config)
    {
        connect(config);  // 建立数据库连接
    }

    /*
     * 【执行 SQL 查询】
     *
     * 这是数据库操作的核心函数
     * 
     * 流程：
     * 1. 通过连接创建 Statement 对象（用于发送 SQL）
     * 2. 执行 SQL 查询，得到 ResultSet（结果集）
     * 3. 将 ResultSet 转成 JSON 格式返回
     * 4. 如果出错，返回空 JSON 对象
     */
    Json::Value DatabaseHandler::query(const std::string& sql) 
    {
        try
        {
            std::cout << "DatabaseHandler::query sql:" << sql << std::endl;
            std::cout.flush();

            std::unique_ptr<sql::Statement> stmt(connection->createStatement());
            std::unique_ptr<sql::ResultSet> result(stmt->executeQuery(sql));
            return parse_result(result.get());
        } 
        catch (const sql::SQLException& e)
        {
            std::cerr << "Database error: " << e.what() << std::endl;
            return Json::Value(Json::objectValue);
        }
    }

    // 执行 INSERT/UPDATE/DELETE，返回影响行数
    int DatabaseHandler::execute(const std::string& sql)
    {
        try
        {
            std::cout << "DatabaseHandler::execute sql:" << sql << std::endl;
            std::cout.flush();

            std::unique_ptr<sql::Statement> stmt(connection->createStatement());
            return stmt->executeUpdate(sql);
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "Database execute error: " << e.what() << std::endl;
            return -1;
        }
    }

    // 预编译查询（SELECT）
    Json::Value DatabaseHandler::queryPrepared(const std::string& sql, const std::vector<std::string>& params)
    {
        try
        {
            std::cout << "DatabaseHandler::queryPrepared sql:" << sql << std::endl;
            std::cout.flush();

            std::unique_ptr<sql::PreparedStatement> pstmt(connection->prepareStatement(sql));
            for (size_t i = 0; i < params.size(); ++i) {
                pstmt->setString(i + 1, params[i]);
            }
            std::unique_ptr<sql::ResultSet> result(pstmt->executeQuery());
            return parse_result(result.get());
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "Database queryPrepared error: " << e.what() << std::endl;
            return Json::Value(Json::objectValue);
        }
    }

    // 预编译更新（INSERT/UPDATE/DELETE）
    int DatabaseHandler::executePrepared(const std::string& sql, const std::vector<std::string>& params)
    {
        try
        {
            std::cout << "DatabaseHandler::executePrepared sql:" << sql << std::endl;
            std::cout.flush();

            std::unique_ptr<sql::PreparedStatement> pstmt(connection->prepareStatement(sql));
            for (size_t i = 0; i < params.size(); ++i) {
                pstmt->setString(i + 1, params[i]);
            }
            return pstmt->executeUpdate();
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "Database executePrepared error: " << e.what() << std::endl;
            return -1;
        }
    }

    // 事务控制
    void DatabaseHandler::beginTransaction()
    {
        try
        {
            connection->setAutoCommit(false);
            std::cout << "Transaction started." << std::endl;
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "beginTransaction error: " << e.what() << std::endl;
        }
    }

    void DatabaseHandler::commit()
    {
        try
        {
            connection->commit();
            connection->setAutoCommit(true);
            std::cout << "Transaction committed." << std::endl;
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "commit error: " << e.what() << std::endl;
        }
    }

    void DatabaseHandler::rollback()
    {
        try
        {
            connection->rollback();
            connection->setAutoCommit(true);
            std::cout << "Transaction rolled back." << std::endl;
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "rollback error: " << e.what() << std::endl;
        }
    }

    // 获取最后插入的自增ID
    int64_t DatabaseHandler::getLastInsertId()
    {
        try
        {
            std::unique_ptr<sql::Statement> stmt(connection->createStatement());
            std::unique_ptr<sql::ResultSet> result(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
            if (result->next()) {
                return result->getInt64(1);
            }
            return 0;
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "getLastInsertId error: " << e.what() << std::endl;
            return -1;
        }
    }

    /*
     * 【检查数据库是否已连接】
     *
     * 检测策略：
     * 1. 先检查连接对象是否存在且未关闭
     * 2. 再执行一条简单 SQL（SELECT 1）验证连接是否真正有效
     *
     * 为什么需要第二步？
     * 连接对象可能状态是"打开"，但实际网络已断开（被动断开）
     * 执行 SELECT 1 才能真正验证连接还在不在
     * 
     * SELECT 1 的含义：
     * 这条 SQL 只返回数字 1，几乎不消耗数据库资源
     * 如果连接有效就能成功，否则就出异常
     */
    bool DatabaseHandler::is_connected() const 
    {
        // 第一步：检查连接对象是否存在
        // connection 是 unique_ptr，如果是空指针或连接已关闭，返回 false
        // || 是"逻辑或"：只要有一个条件为 true，整个表达式就为 true
        if (!connection || connection->isClosed()) {
            return false;
        }

        try {
            // 第二步：执行测试查询验证连接是否真的有效
            std::unique_ptr<sql::Statement> stmt(connection->createStatement());
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT 1"));

            // res->next() 将游标移动到第一行，如果有一行数据返回 true，否则 false
            return res->next();
        } catch (const sql::SQLException& e) {
            // 查询失败说明连接已断开
            return false;
        }
    }

    /*
     * 【重新连接数据库】
     *
     * connection.reset() 做什么？
     * unique_ptr 的 reset() 方法：
     * - 如果 unique_ptr 管理着一个对象，先 delete 掉它
     * - 然后把 unique_ptr 置为 nullptr（空指针）
     *
     * 简单说：关闭旧连接，释放资源
     *
     * 然后 connect(dbConfig) 用保存的配置重新连接
     * dbConfig 是在 connect() 时保存的（见下面的 connect 函数）
     */
    void DatabaseHandler::reconnect() 
    {
        connection.reset();   // 释放旧连接
        connect(dbConfig);    // 用保存的配置重新连接
    }

    /*
     * 【建立数据库连接 —— 内部私有方法】
     *
     * 这是真正和数据库建立连接的函数
     * 只在构造函数和 reconnect() 中调用
     */
    void DatabaseHandler::connect(const DBConfig& config)
    {
        // 保存配置，方便后续 reconnect() 时使用
        dbConfig = config;
        
        try 
        {
            // 调试输出：打印连接参数（方便排查连接问题）
            // 注意：生产环境不应输出密码！
            std::cout << "DatabaseHandler::connect host:" << config.host << std::endl;
            std::cout << "DatabaseHandler::connect port:" << config.port << std::endl;
            std::cout << "DatabaseHandler::connect user:" << config.user << std::endl;
            std::cout << "DatabaseHandler::connect password:" << config.password << std::endl;
            std::cout.flush();

            /*
             * sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
             *
             * 获取 MySQL 驱动的单例实例
             * MySQL_Driver 负责创建数据库连接
             *
             * sql::mysql:: 是嵌套命名空间
             * MySQL_Driver 在 sql 命名空间下的 mysql 子命名空间中
             * 
             * get_mysql_driver_instance() 返回驱动单例
             * 和我们的 Logger 一样也是单例模式
             */
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();

            /*
             * 拼接连接 URL（Uniform Resource Locator，统一资源定位符）
             *
             * 格式：tcp://主机地址:端口号
             * 例如：tcp://127.0.0.1:3306
             *
             * tcp:// 是协议名（Transmission Control Protocol 传输控制协议）
             * TCP 是互联网最基础的可靠传输协议
             *
             * std::to_string(config.port) 把整数端口转成字符串
             * 因为 URL 必须是字符串，不能直接拼接整数
             * + 是字符串连接运算符
             */
            std::string hostUrl = "tcp://" + config.host + ":" + std::to_string(config.port);

            /*
             * connection.reset(driver->connect(hostUrl, config.user, config.password));
             *
             * driver->connect(url, user, password) 建立数据库连接
             * 返回一个 sql::Connection* 原始指针
             *
             * connection.reset(原始指针) 让 unique_ptr 接管这个指针
             * 之后由 unique_ptr 负责释放内存（不用手动 delete）
             *
             * 如果连接失败（如密码错误、网络不通），connect 会抛出 SQLException
             */
            connection.reset(driver->connect(hostUrl, config.user, config.password));

            /*
             * connection->setSchema(config.database);
             *
             * 设置要使用的数据库（Schema）
             * MySQL 中 Schema 和 Database 是同义词
             * 相当于在 MySQL 命令行中执行 USE TakeAwayDatabase;
             *
             * 设置后，后续的 SQL 查询就默认在这个数据库中操作
             */
            connection->setSchema(config.database);

            std::cout << "Database connection successful." << std::endl;
            std::cout.flush();
        } 
        catch (const sql::SQLException& e) 
        {
            // 连接失败的处理
            std::cerr << "Database connection failed: " << e.what() << std::endl;

            // 重置连接指针为 nullptr
            // 不带参数的 reset() 释放管理的对象并将指针置空
            connection.reset();
        }
    }

    /*
     * 【解析查询结果 —— 把 MySQL 结果集转成 JSON】
     *
     * 这是数据库和 HTTP 接口之间的"翻译官"
     * MySQL 返回的结果是 ResultSet 格式
     * 前端需要 JSON 格式
     * 这个函数负责格式转换
     *
     * 返回值是 Json::Value，实际是一个 JSON 数组
     * 数组的每个元素是一个 JSON 对象（一行数据）
     */
    Json::Value DatabaseHandler::parse_result(sql::ResultSet* result) 
    {
        /*
         * Json::Value json_result(Json::arrayValue);
         *
         * 创建一个空的 JSON 数组 []
         * Json::arrayValue 告诉 jsoncpp："我要创建的是数组类型"
         *
         * 最终结果类似：
         * [
         *   {"id": 1, "name": "宫保鸡丁", "price": 28.00},
         *   {"id": 2, "name": "鱼香肉丝", "price": 25.00}
         * ]
         */
        Json::Value json_result(Json::arrayValue);

        /*
         * result->getMetaData() 获取结果集的"元数据"
         *
         * 元数据（Metadata）就是"关于数据的数据"
         * 这里包括：有多少列？每列叫什么？每列是什么类型？
         *
         * ResultSetMetaData 提供：
         * - getColumnCount(): 列数
         * - getColumnName(i): 第 i 列的名字
         * - getColumnType(i): 第 i 列的数据类型（INTEGER、VARCHAR 等）
         */
        sql::ResultSetMetaData* meta = result->getMetaData();

        /*
         * uint32_t colCount = meta->getColumnCount();
         *
         * uint32_t 是无符号 32 位整数（范围 0 ~ 4,294,967,295）
         * 列数不可能是负数，所以用无符号类型
         */
        uint32_t colCount = meta->getColumnCount();
        
        /*
         * result->next() 将游标移动到下一行
         * 首次调用移到第一行，返回 true 表示有数据，false 表示没数据了
         *
         * while (result->next()) 的含义：
         * "只要还有下一行数据，就一直循环处理"
         *
         * 这是遍历数据库结果集的"标准写法"
         * 类似于读取文件时的 while(getline()) 模式
         */
        while (result->next()) 
        {
            Json::Value json_row;  // 创建一个空的 JSON 对象，代表当前这一行

            /*
             * 遍历当前行的每一列
             *
             * 注意：SQL 列索引从 1 开始（不是 0！）
             * 这是数据库的传统，MySQL Connector 遵循了这个传统
             *
             * index <= colCount 是 <= 而不是 <，因为索引从 1 开始
             */
            for (uint32_t index = 1; index <= colCount; ++index) 
            {
                // 使用 getColumnLabel 获取别名（如 c.name as category_name 返回 category_name）
                // getColumnName 返回原始列名会导致别名丢失，同名列互相覆盖
                std::string column_name = meta->getColumnLabel(index);

                /*
                 * 根据列的数据类型，用不同的方式读取数据
                 *
                 * switch-case 语句：根据变量的值，跳转到对应的处理分支
                 * 这里根据列的数据类型选择合适的读取方法
                 *
                 * 为什么要分类型？
                 * 数据库中 VARCHAR 返回字符串，INTEGER 返回整数
                 * jsoncpp 的 Json::Value 需要知道存的是什么类型
                 * 所以我们需要用正确的方法读取，然后赋给 JSON 对象
                 */
                switch (meta->getColumnType(index)) 
                {
                    /*
                     * 字符串类型：VARCHAR、CHAR、LONGVARCHAR
                     *
                     * VARCHAR = 可变长度字符串（如 "张三"）
                     * CHAR    = 固定长度字符串（如 "M" 或 "F"）
                     * LONGVARCHAR = 长文本（如商品描述）
                     *
                     * result->getString(index) 返回 sql::SQLString 类型
                     * 需要 static_cast<std::string>() 转成 C++ 标准字符串
                     * static_cast 是 C++ 的显式类型转换（安全，编译时检查）
                     */
                    case sql::DataType::VARCHAR:
                    case sql::DataType::CHAR:
                    case sql::DataType::LONGVARCHAR:
                        json_row[column_name] = static_cast<std::string>(result->getString(index));
                        break;  // break 跳出 switch，不再执行后面的 case

                    /*
                     * 整数类型：TINYINT、SMALLINT、INTEGER
                     *
                     * TINYINT  = 很小的整数（1字节，-128 ~ 127）
                     * SMALLINT = 小整数（2字节，-32768 ~ 32767）
                     * INTEGER  = 标准整数（4字节，约 ±21亿）
                     *
                     * result->getInt(index) 返回 int32_t 类型
                     * jsoncpp 可以自动处理整数赋值
                     */
                    case sql::DataType::TINYINT:
                    case sql::DataType::SMALLINT:
                    case sql::DataType::INTEGER:
                        json_row[column_name] = result->getInt(index);
                        break;

                    /*
                     * 大整数类型：BIGINT
                     *
                     * BIGINT = 大整数（8字节，约 ±9×10^18）
                     *
                     * getInt64(index) 返回 int64_t 类型
                     * 需要转换为 Json::Int64（jsoncpp 定义的 64 位整数类型）
                     */
                    case sql::DataType::BIGINT:
                        json_row[column_name] = static_cast<Json::Int64>(result->getInt64(index));
                        break;

                    /*
                     * 浮点数类型：REAL、DOUBLE、DECIMAL
                     *
                     * REAL    = 单精度浮点数（4字节）
                     * DOUBLE  = 双精度浮点数（8字节，更精确）
                     * DECIMAL = 精确小数（用于金额等对精度要求高的场景）
                     *
                     * 注意：价格应该用 DECIMAL 而非 DOUBLE
                     * 因为 DOUBLE 有精度问题，如 0.1 + 0.2 可能不等于 0.3
                     */
                    case sql::DataType::REAL:
                    case sql::DataType::DOUBLE:
                    case sql::DataType::DECIMAL:
                        json_row[column_name] = static_cast<double>(result->getDouble(index));
                        break;

                    /*
                     * 布尔类型：BIT
                     *
                     * BIT = 位类型，通常存 0 或 1
                     * getBoolean(index) 返回 bool 类型（true/false）
                     */
                    case sql::DataType::BIT:
                        json_row[column_name] = result->getBoolean(index);
                        break;

                    /*
                     * default 分支：处理所有未列出的数据类型
                     *
                     * 策略：全部当作字符串处理
                     * 这是"兜底"策略，确保不会因为未知类型崩溃
                     */
                    default:
                        json_row[column_name] = static_cast<std::string>(result->getString(index));
                        break;
                }
            }

            /*
             * json_result.append(json_row);
             *
             * 把当前行（一个 JSON 对象）添加到结果数组中
             *
             * 例如处理第一行 {"id":1, "name":"宫保鸡丁"} 后
             * json_result 变成 [{"id":1, "name":"宫保鸡丁"}]
             *
             * 处理完第二行后变成
             * [{"id":1, "name":"宫保鸡丁"}, {"id":2, "name":"鱼香肉丝"}]
             */
            json_result.append(json_row);
        }

        return json_result;  // 返回完整的 JSON 数组
    }
}  // namespace TakeAwayPlatform 结束