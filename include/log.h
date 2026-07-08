// ============================================================
// 【本文件概述】
// 这是日志系统（Logger）的头文件
// 
// 什么是日志（Log）？
// 日志就是程序运行时写下的"日记"，记录发生了什么
// 比如：用户登录了、数据库连接成功了、出错了等等
// 日志对排查问题非常重要——程序崩溃后，看日志就知道崩溃前发生了什么
//
// 【设计模式：单例模式（Singleton Pattern）】
// 整个程序只需要一个日志对象，所有地方都用同一个 Logger 写日志
// 这样保证：
// 1. 所有日志写进同一个文件，方便查看
// 2. 避免多个日志对象争抢文件导致的混乱
// 3. 节省内存（只创建一个对象）
//
// 【本文件是 "Header-Only" 设计】
// 所有函数实现都写在头文件里了（包括函数体 { ... }）
// src/log/log.cpp 只是一个占位文件，实际不需要
// 这样做的好处：不需要单独的 .cpp 文件，使用更简单
// 代价：头文件改动会导致所有包含它的文件重新编译
// ============================================================

#pragma once  // 防止头文件重复包含（详见 common.h 中的解释）

/*
 * 包含需要的标准库头文件
 */
#include <iostream>       // 标准输入输出流：std::cout（控制台输出）、std::cerr（错误输出）
#include <fstream>        // 文件流：用于读写文件（std::ofstream 写文件）
#include <mutex>          // 互斥锁：保证多线程安全（多个线程同时写日志不会乱）
#include <string>         // 字符串类：std::string
#include <ctime>          // C语言时间库：time() 获取当前时间戳
#include <filesystem>     // C++17 文件系统库：检查文件是否存在、创建文件等


namespace TakeAwayPlatform 
{
    /*
     * Logger 类 —— 日志记录器
     *
     * 【类的概念】
     * class 是 C++ 面向对象编程的核心
     * 类 = 数据（属性）+ 行为（方法），像一个"模板"或"蓝图"
     * 根据类创建出来的具体东西叫"对象"（或"实例"）
     * 比如："人"是一个类，"张三"是人的一个对象
     *
     * 【public vs private】
     * public:    外部代码可以访问（所有人都能用）
     * private:   只有类内部的函数能访问（外部看不到，保护数据安全）
     * 
     * 一般规则：数据成员放 private，对外接口放 public
     */
    class Logger 
    {
    public:
        /*
         * 【删除拷贝构造函数和赋值操作符】
         * 
         * = delete 是 C++11 引入的语法，意思是"禁用"这个函数
         * 
         * 为什么要禁用？
         * Logger 是单例模式——整个程序只有一个实例
         * 如果允许拷贝，就可能出现两个 Logger 对象，破坏了单例
         * 
         * 拷贝构造函数：Logger logger2 = logger1;  // 用已有对象创建新对象
         * 赋值操作符：   Logger logger2; logger2 = logger1;  // 赋值
         * 
         * = delete 就像挂了个"此路不通"的牌子
         */
        Logger(const Logger&) = delete;          // 禁止拷贝构造
        Logger& operator=(const Logger&) = delete; // 禁止赋值操作

        /*
         * 【获取单例实例 —— 单例模式的核心】
         * 
         * static（静态）的含义：
         * 这个函数属于"类本身"，不属于"某个对象"
         * 调用方式：Logger::getInstance()  ——不需要先创建一个 Logger 对象
         * 
         * 函数体内的 static 局部变量 instance：
         * 只有第一次调用 getInstance() 时才会创建
         * 之后每次调用都返回同一个对象
         * C++11 起，标准保证 static 局部变量的初始化是线程安全的
         * （多个线程同时第一次调用也不会出问题）
         */
        static Logger& getInstance() {
            static Logger instance;   // 静态局部变量，程序生命周期内只创建一次
            return instance;          // 返回这个唯一实例的引用（& 表示返回引用而非拷贝）
        }

        /*
         * 【设置日志文件路径】
         * 
         * 参数 fileName：日志文件的路径（如 "/opt/TakeAwayPlatform/logs/takeaway.log"）
         * 如果传入空字符串 ""，则使用默认路径
         */
        void setLogFile(const std::string& fileName) {
            /*
             * std::lock_guard 是 C++ 的"自动锁"
             * 
             * 它做了什么？
             * 1. 创建 lock_guard 对象时：自动锁定互斥锁 mtx
             * 2. lock_guard 对象销毁时：自动解锁互斥锁 mtx
             * 
             * 为什么需要锁？
             * 多个线程可能同时调用 setLogFile 修改 logFile 变量
             * 不加锁会导致"竞态条件"（race condition）——数据错乱
             * 
             * lock_guard 比手动 lock()/unlock() 更安全
             * 因为即使函数中途异常退出，lock_guard 也会自动解锁
             * 这种模式叫 RAII（Resource Acquisition Is Initialization）
             * 翻译：资源的获取就是初始化——用对象的生命周期管理资源
             */
            std::lock_guard<std::mutex> lock(mtx);
            
            /*
             * 三元运算符 ? :  —— C++ 中的条件判断简写
             * 语法：条件 ? 值1 : 值2
             * 含义：如果条件为 true，结果=值1；否则结果=值2
             * 
             * 等价于：
             *   if (fileName.empty()) {
             *       finalPath = "/opt/TakeAwayPlatform/logs/takeaway.log";
             *   } else {
             *       finalPath = fileName;
             *   }
             */
            std::string finalPath = fileName.empty() 
                ? "/opt/TakeAwayPlatform/logs/takeaway.log"   // 默认日志路径
                : fileName;  // 用户指定的路径

            /*
             * std::filesystem::exists() 检查文件/目录是否存在
             * 这是 C++17 引入的现代文件操作方式
             * !exists(...) 前面的 ! 是取反，意思是"如果不存在"
             */
            if (!std::filesystem::exists(finalPath)) {
                try {
                    /*
                     * try-catch 是 C++ 的异常处理机制
                     * 
                     * try { ... } 里面放可能出错的代码
                     * catch (...) { ... } 如果出错，就执行 catch 里的代码
                     * 这样程序不会崩溃，而是"优雅地"处理错误
                     */
                    
                    /*
                     * std::ofstream 是"输出文件流"
                     * 用于向文件写入数据
                     * 
                     * 构造参数解释：
                     * - finalPath:     要创建/打开的文件路径
                     * - std::ios::app: 打开模式
                     *   - ios  = input/output stream（输入输出流）
                     *   - app  = append（追加模式，写的内容加到文件末尾，不覆盖原有内容）
                     * 如果没有文件，app 模式也会创建新文件
                     */
                    std::ofstream testFile(finalPath, std::ios::app);
                    
                    /*
                     * is_open() 检查文件是否成功打开
                     * !is_open() —— 前面的 ! 表示"取反"，即"如果没有成功打开"
                     */
                    if (!testFile.is_open()) {
                        // std::cerr 是标准错误输出流（通常也显示在终端，但概念上用于错误信息）
                        // std::endl 是换行符（end line），同时刷新缓冲区
                        std::cerr << "Failed to create log file: " << finalPath << std::endl;
                        return;  // 提前返回，不继续执行后面的代码
                    }
                    testFile.close();  // 关闭文件（好习惯：用完就关）
                    
                } catch (const std::exception& e) {
                    // 如果上面的代码抛出了异常，就跳到这里
                    // e.what() 返回异常的描述信息（字符串）
                    std::cerr << "Error creating log file: " << e.what() << std::endl;
                    return;
                }
            }
            
            // 更新日志文件路径（成员变量 logFile 记录当前使用的文件路径）
            logFile = finalPath;
            std::cout << "Log file set to: " << logFile << std::endl;
        }

        /*
         * 【日志级别枚举】
         * 
         * enum class 是 C++11 引入的"强类型枚举"
         * 
         * 什么是枚举？
         * 枚举就是把一组相关的常量放在一起，给它们起名字
         * 比如一周七天：Monday, Tuesday, ...
         * 用名字代替数字，代码更好读
         * 
         * enum class vs 传统 enum：
         * enum class 更安全，不能隐式转换为 int
         * 使用时必须写 Level::DEBUG（不能只写 DEBUG）
         * 
         * 日志级别从低到高：
         * DEBUG   (调试信息，最详细) → INFO → WARNING → ERROR (错误信息，最严重)
         * 
         * 设置了日志级别后，比这个级别低的日志就不会输出
         * 比如设为 WARNING 级别，DEBUG 和 INFO 就不输出了
         */
        enum class Level { DEBUG, INFO, WARNING, ERROR };

        // 设置日志级别（低于此级别的日志不输出）
        void setLogLevel(Level level) {
            std::lock_guard<std::mutex> lock(mtx);  // 加锁，保护 logLevel 变量
            logLevel = level;
        }

        /*
         * 【核心日志记录方法】
         * 
         * 参数：
         * - level:   日志级别（DEBUG/INFO/WARNING/ERROR）
         * - message: 要记录的日志内容（字符串）
         */
        void log(Level level, const std::string& message) {
            // 如果当前消息级别 < 设置的最低级别，直接跳过不记录
            // 这叫"日志过滤"：开发时设 DEBUG 看所有日志，上线后设 WARNING 只看重要的
            if (level < logLevel) return;

            std::lock_guard<std::mutex> lock(mtx);  // 加锁，保证写日志是原子操作
            std::string levelStr = levelToString(level);   // 把枚举值转成字符串（如 Level::ERROR → "ERROR"）
            std::string timestamp = getCurrentTime();       // 获取当前时间字符串
            
            // 拼接日志条目，格式：[时间戳] [级别] 消息内容
            // 例如：[2025-07-08 14:30:00] [INFO] Server started
            std::string logEntry = "[" + timestamp + "] [" + levelStr + "] " + message;

            // 输出到控制台（std::cout 把内容打印到终端）
            std::cout << logEntry << std::endl;

            // 如果日志文件路径不为空，同时写入文件
            if (!logFile.empty()) {
                // std::ofstream 打开文件，ios::app 表示追加模式
                std::ofstream file(logFile, std::ios::app);
                if (file.is_open()) {
                    file << logEntry << std::endl;  // 写入一行日志
                }
                // file 对象离开作用域时自动关闭文件（RAII 机制）
            }
        }

        // 以下四个是便捷方法，分别对应四个日志级别
        // 它们内部都调用 log() 方法，只是自动填好了级别参数

        void debug(const std::string& msg)   // 调试日志：开发时用，查看变量值、程序流程
        { 
            log(Level::DEBUG, msg); 
        }

        void info(const std::string& msg)    // 信息日志：记录正常运行的关键节点（如"服务启动"）
        { 
            log(Level::INFO, msg);
        }

        void warning(const std::string& msg) // 警告日志：不是错误但需要关注（如"内存使用率超过80%"）
        { 
            log(Level::WARNING, msg); 
        }

        void error(const std::string& msg)   // 错误日志：出错了（如"数据库连接失败"）
        { 
            log(Level::ERROR, msg); 
        }


    private:
        /*
         * 【私有（private）部分】
         * 只有类内部的函数可以访问下面的内容
         * 外部代码无法直接操作这些成员，保证了数据安全
         */

        /*
         * 私有构造函数
         * 
         * 构造函数的特点：
         * - 函数名和类名相同（Logger）
         * - 没有返回值类型（连 void 都没有）
         * - 对象创建时自动调用
         * 
         * 为什么是 private？
         * 这是单例模式的关键！外部代码不能直接 new Logger()
         * 只能通过 getInstance() 获取唯一实例
         * 
         * : logLevel(Level::INFO) 是"初始化列表"
         * 在构造函数体 { } 执行之前，先用指定的值初始化成员变量
         * 这里把 logLevel 初始化为 INFO 级别（默认显示 INFO 及以上的日志）
         */
        Logger() : logLevel(Level::INFO) {}

        /*
         * 【辅助方法：把日志级别转成字符串】
         * 
         * switch-case 语法：
         * switch(变量) {
         *     case 值1: 做某事; break;
         *     case 值2: 做某事; break;
         *     default:  以上都不匹配时执行;
         * }
         * 
         * break 很重要！没有 break 会"穿透"到下一个 case
         * default 是可选的，处理所有未列出的情况
         */
        std::string levelToString(Level level) {
            switch(level) {
                case Level::DEBUG:   
                {
                    return "DEBUG";
                }
                
                case Level::INFO:    
                {
                    return "INFO";
                }

                case Level::WARNING:
                {
                    return "WARNING";
                }

                case Level::ERROR:   
                {
                    return "ERROR";
                }

                default:            // 如果以上都不是（一般不会出现）
                {
                    return "WARNING";  // 默认返回 WARNING
                } 
            }
        }

        /*
         * 【辅助方法：获取当前时间字符串】
         * 
         * 返回格式："2025-07-08 14:30:00"
         * 
         * time_t:     C 语言的时间类型（本质是整数，表示从1970年1月1日以来的秒数）
         * time(nullptr): 获取当前时间戳
         * localtime():   把时间戳转成本地时间（带时区）
         * strftime():    把时间格式化成字符串
         *   %Y = 四位年份 (2025)
         *   %m = 两位月份 (07)
         *   %d = 两位日期 (08)
         *   %H = 24小时制小时 (14)
         *   %M = 分钟 (30)
         *   %S = 秒 (00)
         */
        std::string getCurrentTime() {
            time_t now = time(nullptr);                     // 获取当前时间戳
            char buf[80];                                   // 字符数组，用来存放格式化后的时间字符串
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            return buf;                                     // 返回格式化后的字符串
        }

        /*
         * 【成员变量（类的"属性"）】
         * 这些是 Logger 对象内部保存的数据
         */
        std::mutex mtx;                 // 互斥锁，保证多线程环境下日志不会互相穿插
        std::string logFile;            // 当前日志文件的完整路径
        Level logLevel;                 // 当前日志级别（低于此级别的日志不输出）
    };

}  // namespace TakeAwayPlatform 结束#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <ctime>
#include <filesystem>


namespace TakeAwayPlatform 
{
    class Logger 
    {
    public:
        // 删除拷贝构造函数和赋值操作符
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        // 获取单例实例
        // C++11 保证静态局部变量线程安全
        static Logger& getInstance() {
            static Logger instance;  
            return instance;
        }

        // 设置日志文件路径
        void setLogFile(const std::string& fileName) {
            std::lock_guard<std::mutex> lock(mtx);
            
            // 确定最终日志文件路径
            std::string finalPath = fileName.empty() 
                ? "/opt/TakeAwayPlatform/logs/takeaway.log" 
                : fileName;  // 兜底！如果文件名不生效，有一个兜底的位置
            
            // 检查文件是否存在，不存在则创建
            if (!std::filesystem::exists(finalPath)) {
                try {
                    // 尝试直接创建文件
                    std::ofstream testFile(finalPath, std::ios::app);
                    if (!testFile.is_open()) {
                        std::cerr << "Failed to create log file: " << finalPath << std::endl;
                        return;
                    }
                    testFile.close(); // 立即关闭文件
                } catch (const std::exception& e) {
                    std::cerr << "Error creating log file: " << e.what() << std::endl;
                    return;
                }
            }
            
            // 更新日志文件路径
            logFile = finalPath;
            std::cout << "Log file set to: " << logFile << std::endl;
        }

        // 日志级别枚举
        enum class Level { DEBUG, INFO, WARNING, ERROR };

        // 设置日志级别
        void setLogLevel(Level level) {
            std::lock_guard<std::mutex> lock(mtx);
            logLevel = level;
        }

        // 日志记录方法
        void log(Level level, const std::string& message) {
            if (level < logLevel) return;  // 过滤低级别日志 
            // error-一般错误 warning-警告 info-信息 debug-调试

            std::lock_guard<std::mutex> lock(mtx);
            std::string levelStr = levelToString(level);
            std::string timestamp = getCurrentTime();
            
            // 格式化日志条目
            std::string logEntry = "[" + timestamp + "] [" + levelStr + "] " + message;

            // 输出到控制台
            std::cout << logEntry << std::endl;

            // 输出到文件
            if (!logFile.empty()) {
                std::ofstream file(logFile, std::ios::app);
                if (file.is_open()) {
                    file << logEntry << std::endl;
                }
            }
        }

        // debug级别日志记录
        void debug(const std::string& msg) 
        { 
            log(Level::DEBUG, msg); 
        }

        // info级别日志记录
        void info(const std::string& msg) 
        { 
            log(Level::INFO, msg);
        }

        // warning级别日志记录
        void warning(const std::string& msg) 
        { 
            log(Level::WARNING, msg); 
        }

        // error级别日志记录
        void error(const std::string& msg) 
        { 
            log(Level::ERROR, msg); 
        }


    private:
        // 私有构造函数
        Logger() : logLevel(Level::INFO) {}

        // 转换日志级别为字符串
        std::string levelToString(Level level) {
            switch(level) {
                case Level::DEBUG:   
                {
                    return "DEBUG";
                }
                
                case Level::INFO:    
                {
                    return "INFO";
                }

                case Level::WARNING:
                {
                    return "WARNING";
                }

                case Level::ERROR:   
                {
                    return "ERROR";
                }

                default:            
                {
                    return "WARNING";
                } 
            }
        }

        // 获取当前时间字符串
        std::string getCurrentTime() {
            time_t now = time(nullptr);
            char buf[80];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            return buf;
        }

        std::mutex mtx;                 // 互斥锁保证线程安全
        std::string logFile;            // 日志文件路径
        Level logLevel;                 // 当前日志级别
    };

}