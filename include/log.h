#pragma once

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