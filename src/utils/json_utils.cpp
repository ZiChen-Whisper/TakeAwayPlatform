// ============================================================
// 【本文件概述】
// JSON 工具函数 —— 配置文件的加载器
//
// 这个文件实现了 load_config 函数（在 common.h 中声明）
// 功能：读取 JSON 配置文件，解析并返回 JSON 对象
//
// 【文件读写基础】
// C++ 中读写文件需要使用"文件流"：
// - ifstream（input file stream）:  读取文件
// - ofstream（output file stream）: 写入文件
// - fstream（file stream）:         读写文件
//
// 流（Stream）的概念：
// 把数据当作"水流"一样处理，可以源源不断地读取或写入
// ============================================================

#include <fstream>   // 文件流：std::ifstream 用于读取文件
#include <sstream>   // 字符串流：std::stringstream 用于在内存中读写字符串
#include <iostream>  // 标准输入输出：std::cout, std::cerr, std::flush

#include "common.h"  // 公共头文件（包含 load_config 的声明和 Json::Value 类型）


namespace TakeAwayPlatform
{
    /*
     * 【加载配置文件】
     *
     * 这是读取配置文件的核心函数
     * 
     * 参数 path：配置文件路径（如 "/opt/TakeAwayPlatform/config/config.json"）
     * 返回：解析后的 Json::Value 对象
     * 失败时抛出 std::runtime_error 异常
     *
     * 流程：
     * 1. 打开文件
     * 2. 读取全部内容到字符串缓冲区
     * 3. 解析 JSON
     * 4. 返回结果
     */
    Json::Value load_config(const std::string& path) 
    {
        // 调试输出：显示正在加载的配置文件路径
        std::cout << "Loading config, path:" << path << std::endl;
        std::cout.flush();  // 强制刷新输出（避免程序崩溃时日志丢失）

        /*
         * std::ifstream configFile(path);
         *
         * 创建一个输入文件流对象，尝试打开指定路径的文件
         *
         * ifstream 构造函数会自动尝试打开文件
         * 如果文件存在且有读取权限，打开成功
         * 如果文件不存在或权限不足，打开失败
         *
         * 和 C 语言的 fopen 不同，C++ 的 ifstream 用对象管理文件
         * 对象销毁时自动关闭文件（RAII 机制）
         */
        std::ifstream configFile(path);

        /*
         * if (!configFile.is_open())
         *
         * is_open() 检查文件是否成功打开
         * ! 是"逻辑非"运算符，!true = false，!false = true
         *
         * 所以这里的意思是"如果文件打开失败"
         */
        if (!configFile.is_open()) {
            /*
             * throw std::runtime_error("Failed to open config file: " + path);
             *
             * throw 抛出异常，中断当前函数的执行
             *
             * 这里用到的 C++ 特性：
             * - std::string 的 + 运算符可以连接字符串
             * - 错误消息包含了具体的文件路径，方便调试
             *
             * 调用者（main.cpp 中的 catch 块）会捕获并处理这个异常
             */
            throw std::runtime_error("Failed to open config file: " + path);
        }

        std::cout << "Opening config success." << std::endl;
        std::cout.flush();

        /*
         * std::stringstream buffer;
         *
         * stringstream 是一个"内存中的流"
         * 它就像一块内存白板，可以在上面读写数据
         *
         * 这里用它来暂存从文件读取的全部内容
         */
        std::stringstream buffer;

        /*
         * buffer << configFile.rdbuf();
         *
         * << 是"流插入运算符"（把右边的数据塞进左边的流）
         *
         * configFile.rdbuf() 返回文件流的底层缓冲区指针
         * 这个操作把整个文件的内容一次性读入 buffer
         *
         * 这是一种高效的文件读取方式
         * 等价于：逐行读取然后拼接到字符串，但更快
         */
        buffer << configFile.rdbuf();

        /*
         * configFile.close();
         *
         * 显式关闭文件
         * 虽然 ifstream 析构时会自动关闭，但手动关闭是好习惯
         */
        configFile.close();

        // 创建一个空的 JSON 对象，用于存放解析结果
        Json::Value root;

        /*
         * Json::CharReaderBuilder builder;
         *
         * CharReaderBuilder 是 jsoncpp 库的"解析器构建器"
         * 它负责创建 JSON 解析器，可以配置解析选项
         * 比如是否允许注释、单引号、尾随逗号等
         *
         * 默认配置是 JSON 严格模式（符合 JSON 标准）
         */
        Json::CharReaderBuilder builder;

        /*
         * JSONCPP_STRING errors;
         *
         * JSONCPP_STRING 是 jsoncpp 定义的字符串类型
         * 通常是 std::string 的别名
         * 用于接收解析过程中的错误信息
         */
        JSONCPP_STRING errors;
        
        /*
         * Json::parseFromStream(builder, buffer, &root, &errors)
         *
         * 从流中解析 JSON 数据
         *
         * 参数说明：
         * - builder:  解析器配置对象
         * - buffer:   输入流（包含 JSON 文本）
         * - &root:    输出参数，解析结果存入这个 Json::Value 对象
         *              & 是"取地址"运算符，传的是 root 变量的地址（指针）
         * - &errors:  输出参数，错误信息存入这个字符串
         *
         * 返回值：bool 类型
         * - true:  解析成功
         * - false: 解析失败（语法错误、格式不对等）
         *
         * ! 取反：如果解析失败...
         */
        if (!Json::parseFromStream(builder, buffer, &root, &errors)) {
            // 抛出异常，包含具体的错误信息
            throw std::runtime_error("JSON parse error: " + errors);
        }
        
        return root;  // 返回解析好的 JSON 对象
    }
}  // namespace TakeAwayPlatform 结束#include <fstream>
#include <sstream>
#include <iostream>

#include "common.h"


namespace TakeAwayPlatform
{
    Json::Value load_config(const std::string& path) 
    {
        std::cout << "Loading config, path:" << path << std::endl;
        std::cout.flush();

        std::ifstream configFile(path);
        if (!configFile.is_open()) {
            throw std::runtime_error("Failed to open config file: " + path);
        }

        std::cout << "Opening config success." << std::endl;
        std::cout.flush();

        std::stringstream buffer;
        buffer << configFile.rdbuf();
        configFile.close();

        Json::Value root;
        Json::CharReaderBuilder builder;
        JSONCPP_STRING errors;
        
        if (!Json::parseFromStream(builder, buffer, &root, &errors)) {
            throw std::runtime_error("JSON parse error: " + errors);
        }
        
        return root;
    }
}

