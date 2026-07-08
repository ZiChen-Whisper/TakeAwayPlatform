/*
 * Copyright (C), 2025-2030, 华中师范大学计算机学院
 * FileName: common.h
 * Author: 汪浩
 * Date: 2025-5-25
 * Description: 通用配置文件类 —— 定义项目中公用的数据结构
 * History:
 * <author>   <time>      <version>    <desc>
 * 汪浩        2025-2-25   1.0          初始文件
 */

// ============================================================
// 【头文件基础知识】
// .h 文件叫做"头文件"（Header File），用于声明函数、类、结构体等
// 头文件通常只做"声明"（告诉编译器这个东西存在），不写具体"实现"
// 把声明和实现分开的好处：多个 .cpp 文件可以共用同一个头文件
// ============================================================

/*
 * #pragma once 是一个预处理指令（preprocessor directive）
 * 
 * 它的作用是：防止同一个头文件被重复包含（include）
 * 
 * 举个例子，a.h include 了 common.h，b.h 也 include 了 common.h，
 * 然后 main.cpp 同时 include 了 a.h 和 b.h，这样 common.h 就会被包含两次
 * 如果没有 #pragma once，编译会报错"重复定义"
 * 
 * 等价于传统写法（但更简洁）：
 *   #ifndef COMMON_H
 *   #define COMMON_H
 *   ...头文件内容...
 *   #endif
 * 
 * 语法上这叫"头文件保护"（Header Guard）
 */
#pragma once


/*
 * #include 也是预处理指令
 * 意思是"把另一个文件的内容复制粘贴到这里"
 * 
 * 尖括号 <> 和双引号 "" 的区别：
 * - <> 表示从系统标准路径查找（如 /usr/include/）
 * - "" 表示先从当前项目目录查找，找不到再去系统路径
 * 
 * 一般规则：系统/第三方库用 <>，自己写的头文件用 ""
 */
#include <string>       // C++ 标准库的字符串类 std::string
#include <json/json.h>  // jsoncpp 第三方库，用于解析和生成 JSON 数据


/*
 * namespace 叫做"命名空间"
 * 
 * 作用：避免名字冲突
 * 比如你的代码里有个 max 函数，别人的库也有个 max 函数，
 * 如果不用命名空间，编译器就不知道用哪个，会报"命名冲突"错误
 * 
 * TakeAwayPlatform 是我们项目的专属命名空间，
 * 所有项目代码都放在这个命名空间里，不会和外部的代码冲突
 * 
 * 语法：namespace 命名空间名 { ... }
 * 使用：TakeAwayPlatform::DBConfig  或者  using namespace TakeAwayPlatform;
 */
namespace TakeAwayPlatform 
{
    /*
     * 函数声明（Function Declaration）
     * 
     * 这里只声明了 load_config 函数的"签名"（signature），没有写函数体
     * 函数体/实现在 src/utils/json_utils.cpp 里
     * 
     * 逐词解析：
     * - Json::Value  返回值类型，表示返回一个 JSON 对象
     *                 Json::Value 是 jsoncpp 库的核心类型，可以表示任意 JSON 值
     * - load_config  函数名（驼峰命名法：首单词小写，后面单词首字母大写）
     * - const        表示参数 path 在函数内部不会被修改（const = 常量）
     * - std::string& & 是引用符号，表示传的是原变量的别名而非拷贝，效率更高
     *                 不加 & 的话会在传参时复制一份字符串，浪费内存和时间
     * - path         参数名，表示配置文件的路径
     */
    Json::Value load_config(const std::string& path);


    /*
     * 结构体（Struct）
     * 
     * struct 是 C++ 中用来"组合多个相关数据"的工具
     * 你可以把它想象成一个"表格的列定义"——每一行数据都有这些字段
     * 
     * struct 和 class 的区别：
     * - struct 默认所有成员是 public（外部可以访问）
     * - class 默认所有成员是 private（外部不能访问）
     * 除此之外，两者几乎完全一样
     * 
     * 这里 DBConfig 就是把数据库连接需要的 5 个信息打包在一起
     */
    struct DBConfig {
        std::string host;      // 数据库服务器地址（如 "127.0.0.1" 或 "192.168.1.100"）
        int port;              // 端口号（整数，如 3306）
        std::string user;      // 数据库用户名
        std::string password;  // 数据库密码
        std::string database;  // 数据库名称
    };

}  // namespace TakeAwayPlatform 结束（这里的分号可以省略，但加上更清晰）