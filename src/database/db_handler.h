// ============================================================
// 【本文件概述】
// 数据库处理器（Database Handler）的头文件
//
// 这个类负责和 MySQL 数据库交互：
// - 建立数据库连接
// - 执行 SQL 查询
// - 把查询结果转成 JSON 格式（方便返回给前端）
//
// 【MySQL Connector/C++】
// MySQL 官方提供了 C++ 连接器，让我们在 C++ 代码中操作数据库
// 本项目使用的是 MySQL Connector/C++ 8.0 版本
// 相关头文件在 lib/mysql-connector/include/ 下
// ============================================================

#pragma once  // 防止头文件重复包含

#include "common.h"              // 我们自己的公共头文件（包含 DBConfig 结构体定义）
#include <mysql_connection.h>    // MySQL 连接类（sql::Connection）
#include <cppconn/statement.h>   // SQL 语句执行类（sql::Statement）
#include <cppconn/resultset.h>   // 查询结果集类（sql::ResultSet）
#include <memory>                // C++ 智能指针（std::unique_ptr）


namespace TakeAwayPlatform
{
    /*
     * DatabaseHandler 类 —— 数据库操作助手
     *
     * 每个 DatabaseHandler 对象持有：
     * - 一个数据库连接（sql::Connection）
     * - 一份数据库配置（DBConfig）
     *
     * 主要功能：
     * 1. 连接数据库
     * 2. 执行查询并返回 JSON 结果
     * 3. 检查连接状态
     * 4. 断线重连
     */
    class DatabaseHandler 
    {
    public:
        /*
         * 【构造函数】
         * 
         * 参数：const DBConfig& config
         * - DBConfig 是我们自定义的结构体（在 common.h 中定义）
         * - const & 表示常量引用：不会修改 config，也不拷贝（高效）
         * 
         * 构造函数做的事情：调用 connect() 连接数据库
         * 具体实现在 db_handler.cpp 中（这里只是声明）
         */
        DatabaseHandler(const DBConfig& config);

        /*
         * 【执行 SQL 查询】
         * 
         * 参数：const std::string& sql —— 要执行的 SQL 语句
         * 返回值：Json::Value —— 查询结果（JSON 数组格式）
         * 
         * 例如：
         * 输入 "SELECT * FROM menu_items"
         * 输出 [{id:1, name:"宫保鸡丁", price:28.0}, {id:2, name:"鱼香肉丝", price:25.0}]
         * 
         * 注意：返回值是 JSON 数组（每行是一个 JSON 对象）
         */
        Json::Value query(const std::string& sql);

        /*
         * 【检查是否已连接】
         * 
         * const 放在函数后面表示这是一个"常量成员函数"
         * 意思：调用这个函数不会修改对象的任何成员变量
         * 
         * 返回值：true = 已连接，false = 未连接/已断开
         */
        bool is_connected() const;

        /*
         * 【重新连接数据库】
         * 
         * 当数据库断开后（网络波动、数据库重启等），调用此函数重新连接
         * 内部流程：断开旧连接 → 使用保存的配置重新连接
         */
        void reconnect();

        
    private:
        /*
         * 【私有方法：建立数据库连接】
         * 
         * 参数：const DBConfig& config —— 数据库连接配置
         * 
         * 这是内部函数，只在本类内部调用
         * 外部不能直接调用 connect()，只能通过构造函数或 reconnect() 触发
         */
        void connect(const DBConfig& config);

        /*
         * 【私有方法：解析查询结果】
         * 
         * 参数：sql::ResultSet* result —— MySQL 返回的查询结果集指针
         * 返回值：Json::Value —— 转换后的 JSON 数据
         * 
         * MySQL 返回的 ResultSet 是数据库专属格式
         * 我们需要把它转成通用的 JSON 格式，方便 HTTP 响应返回
         * 
         * 这个函数遍历结果集的每一行每一列，构建对应的 JSON 对象
         * 比如：
         *   MySQL 结果集:  id=1 | name="张三" 
         *   转成 JSON:     {"id":1, "name":"张三"}
         */
        Json::Value parse_result(sql::ResultSet* result);


    private:
        /*
         * 【成员变量】
         */
        DBConfig dbConfig;  // 数据库配置（保存一份，供 reconnect() 时重新连接使用）

        /*
         * std::unique_ptr<sql::Connection> connection;
         * 
         * std::unique_ptr 是 C++11 的"独占式智能指针"
         * 
         * 智能指针（Smart Pointer）解决了什么？
         * C++ 中 new 出来的对象必须手动 delete，否则会内存泄漏
         * 智能指针自动管理内存：当指针离开作用域时自动释放对象
         * 
         * unique_ptr 的特点：
         * - "独占"：同一时间只有一个 unique_ptr 指向某个对象
         * - 不能被拷贝（不能两个指针指向同一对象）
         * - 可以被移动（所有权转移）
         * - 零开销（和原始指针一样快）
         * 
         * 还有另一种智能指针 shared_ptr：
         * - "共享"：多个指针可以指向同一对象
         * - 引用计数：最后一个指针销毁时才释放对象
         * - 有少量额外开销
         */
        std::unique_ptr<sql::Connection> connection;
    };

}  // namespace TakeAwayPlatform 结束