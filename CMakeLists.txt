# ============================================================
# 【本文件概述】
# CMake 构建配置文件
# 
# 什么是 CMake？
# CMake 是一个跨平台的构建系统生成器
# 它不直接编译代码，而是生成编译规则文件（如 Makefile）
# 然后由 make（或 ninja、MSBuild 等）按规则实际编译
# 
# CMake 的工作流程：
# 1. cmake .       → 读取 CMakeLists.txt，生成 Makefile
# 2. make          → 按照 Makefile 的规则编译代码
# 3. make install  → （可选）把编译产物安装到系统目录
# 
# CMake 语法要点（小白必看）：
# - # 开头的是注释
# - 命令不区分大小写（但习惯用小写）
# - 变量用 ${变量名} 引用
# - 每条命令自成一行
# - 参数之间用空格分隔
# - 字符串可以用双引号 "" 包裹（空格需要）
# ============================================================

# ---------- 第 1 步：声明 CMake 最低版本要求 ----------
# cmake_minimum_required(VERSION 3.12)
# 告诉 CMake："我的脚本至少需要 3.12 版本才能正确解析"
# 如果用户安装的 CMake 版本低于 3.12，会报错并拒绝执行
# 3.12 是 2018 年的版本，选择它是因为它支持 C++17 和相关特性
cmake_minimum_required(VERSION 3.12)

# ---------- 第 2 步：定义项目 ----------
# project(项目名)
# 设置项目名称，同时会创建一些有用的变量：
# - ${PROJECT_NAME}    = TakeAwayPlatform（项目名）
# - ${PROJECT_SOURCE_DIR} = CMakeLists.txt 所在目录
# - ${PROJECT_BINARY_DIR} = 编译输出目录（build/）
project(TakeAwayPlatform)

# ---------- 第 3 步：设置 C++ 标准 ----------
# set(变量名 值)
# set 是 CMake 中最常用的命令，用于设置变量

# CMAKE_CXX_STANDARD 17
# 告诉编译器使用 C++17 标准
# C++17 相比 C++11/14 增加了：
# - std::filesystem（文件系统操作）
# - std::optional, std::variant
# - if constexpr 等
set(CMAKE_CXX_STANDARD 17)

# CMAKE_CXX_STANDARD_REQUIRED ON
# 要求编译器必须支持 C++17
# 如果不支持，配置阶段就报错（而不是编译到一半才报错）
# ON/OFF 是 CMake 中的布尔值（相当于 true/false）
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- 第 4 步：定义项目目录变量 ----------
# 用变量来管理路径，方便后续引用和修改
# 这是一种"DRY 原则"（Don't Repeat Yourself）

# ${CMAKE_SOURCE_DIR} 是 CMake 内置变量
# 指向顶层 CMakeLists.txt 所在的目录（项目根目录）
set(PROJECT_ROOT ${CMAKE_SOURCE_DIR})          # 项目根目录

set(SOURCE_DIR ${PROJECT_ROOT}/src)            # 源文件目录（所有 .cpp/.h 在这里）
set(INCLUDE_DIR ${PROJECT_ROOT}/include)       # 公共头文件目录
set(CONFIG_DIR ${PROJECT_ROOT}/config)         # 配置文件目录（如 config.json）

# ---------- 第 5 步：设置头文件搜索路径 ----------
# include_directories(路径1 路径2 ...)
# 告诉编译器："去这些目录找头文件"
# 
# 相当于 g++ 的 -I 参数
# 设置后，代码里可以写 #include "common.h"（不需要写完整路径）
# 
# 这里添加了三个目录：
# - include/         公共头文件（common.h, log.h, task_queue.h, thread_pool.h）
# - src/database/    db_handler.h
# - src/http/        rest_server.h
include_directories(
    ${INCLUDE_DIR}
    ${SOURCE_DIR}/database
    ${SOURCE_DIR}/http
    ${SOURCE_DIR}/services
    ${SOURCE_DIR}/middleware
    ${SOURCE_DIR}/utils
)

# ---------- 第 6 步：添加第三方库 cpp-httplib ----------
# cpp-httplib 是一个轻量级的 C++ HTTP 库
# 它只需要一个头文件 httplib.h，不需要链接额外的库
# 所以只设置 include 路径即可

set(CPP_HTTPLIB_INCLUDE_DIR ${PROJECT_ROOT}/lib/cpp-httplib/include)
include_directories(${CPP_HTTPLIB_INCLUDE_DIR})

# ---------- 第 7 步：添加 MySQL Connector/C++ ----------
# MySQL 官方提供的 C++ 连接库
# 需要设置头文件路径（编译时）和库文件路径（链接时）

set(MYSQL_CONNECTOR_ROOT ${PROJECT_ROOT}/lib/mysql-connector)

# include 路径：告诉编译器去哪里找 mysql 的头文件
# jdbc 是 Java 数据库连接标准，MySQL C++ 连接器也沿用了这个命名
# mysqlx 是 MySQL X Protocol 相关（一种新的 MySQL 协议）
include_directories(
    ${MYSQL_CONNECTOR_ROOT}/include
    ${MYSQL_CONNECTOR_ROOT}/include/jdbc
    ${MYSQL_CONNECTOR_ROOT}/include/mysqlx
)

# ---------- 第 8 步：添加 jsoncpp（JSON 解析库）----------
# jsoncpp 是一个 C++ JSON 库，用于解析和生成 JSON 数据
#
# 和 httplib 不同，jsoncpp 需要编译（有 .cpp 源文件）
# 所以需要：
# 1. 设置头文件路径（include_directories）
# 2. 收集源文件（file GLOB）
# 3. 编译成静态库（add_library）

set(JSONCPP_ROOT ${PROJECT_ROOT}/lib/jsoncpp)
include_directories(${JSONCPP_ROOT}/src/include)

# file(GLOB_RECURSE 变量名 通配符模式)
# 
# GLOB_RECURSE 递归收集匹配的文件
# 和 GLOB 的区别：GLOB 只搜索当前目录，GLOB_RECURSE 递归搜索所有子目录
# 
# *.cpp 匹配所有 .cpp 结尾的文件
# 这里收集 jsoncpp 的所有源文件，用于编译
file(GLOB_RECURSE JSONCPP_SOURCES
    ${JSONCPP_ROOT}/src/src/lib_json/*.cpp
    ${JSONCPP_ROOT}/src/src/lib_json/*.h
)

# add_library(库名 STATIC 源文件列表)
# 
# STATIC 表示编译成"静态库"（.a 文件）
# 
# 静态库 vs 动态库：
# - 静态库 (.a)：编译时嵌入到可执行文件中，执行文件更大但不依赖外部库
# - 动态库 (.so)：运行时加载，可执行文件更小但需要 .so 文件在系统路径中
# 
# 这里用静态库的原因是简单：
# 不需要担心运行时找不到 .so 文件
add_library(jsoncpp_lib STATIC ${JSONCPP_SOURCES})

# ---------- 第 9 步：添加 MySQL Connector 库路径 ----------
# link_directories(路径)
# 告诉链接器："去这个目录找库文件（.so 或 .a）"
# 相当于 g++ 的 -L 参数

# 被注释掉的旧路径（库在项目目录下）
# link_directories(${MYSQL_CONNECTOR_ROOT}/lib)

# 优化后的路径：MySQL connector 库文件实际安装在系统路径下
# /opt/TakeAwayPlatform/bin/lib/ 包含 libmysqlcppconn8.so 等库文件
link_directories(/opt/TakeAwayPlatform/bin/lib)

# ---------- 第 10 步：收集项目源文件 ----------
# file(GLOB_RECURSE SOURCES ...)
# 
# 收集所有需要编译的源文件
# ${SOURCE_DIR}/*.cpp    匹配 src/ 下的所有 .cpp 文件
# ${SOURCE_DIR}/utils/*.cpp 匹配 src/utils/ 下的 .cpp 文件
# ${SOURCE_DIR}/*.h      匹配 src/ 下的所有 .h 文件
# 
# 注意：头文件 (.h) 通常不需要加入编译
# 但加入 GLOB 可以确保头文件改动也触发重新编译（某些 IDE 需要）
file(GLOB_RECURSE SOURCES 
    ${SOURCE_DIR}/*.cpp
    ${SOURCE_DIR}/*.h
)

# ---------- 第 11 步：创建可执行文件 ----------
# add_executable(可执行文件名 源文件列表)
# 
# 告诉 CMake："把这些源文件编译链接成一个可执行程序"
# ${PROJECT_NAME} = TakeAwayPlatform
# 
# 编译后会在 build/ 目录生成 TakeAwayPlatform 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# ---------- 第 12 步：链接库 ----------
# target_link_libraries(目标名 可见性 库名1 库名2 ...)
# 
# 告诉链接器："这个目标需要链接这些库"
# 
# PRIVATE 可见性的含义：
# - PRIVATE:   只有当前目标需要这个库（依赖不传递）
# - PUBLIC:    当前目标和依赖它的目标都需要（依赖传递）
# - INTERFACE: 只有依赖它的目标需要（当前目标不需要）
# 
# 这里全部用 PRIVATE，因为只有 TakeAwayPlatform 这个可执行文件需要
target_link_libraries(${PROJECT_NAME}
    PRIVATE 
        jsoncpp_lib    # 我们自己编译的 JSON 解析库（静态库）
        mysqlcppconn   # MySQL C++ 连接器库
        ssl            # OpenSSL 加密库（MySQL 安全连接需要）
        crypto         # OpenSSL 加密算法库（MySQL 安全连接需要）
        z              # zlib 压缩库（MySQL 协议压缩需要）
        pthread        # POSIX 线程库（cpp-httplib 多线程需要）
)

# ---------- 第 13 步：设置运行时库路径 ----------
# RPATH（Runtime Path）是嵌入在可执行文件中的库搜索路径
# 程序运行时会去 RPATH 指定的目录找 .so 文件
# 
# CMAKE_INSTALL_RPATH
# 设置安装后的可执行文件的 RPATH
set(CMAKE_INSTALL_RPATH "${MYSQL_CONNECTOR_ROOT}/lib")

# CMAKE_BUILD_WITH_INSTALL_RPATH TRUE
# 让构建目录中的可执行文件也使用相同的 RPATH
# 这样不安装也能在 build/ 目录直接运行
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# ---------- 第 14 步：安装配置文件 ----------
# install(DIRECTORY 源目录 DESTINATION 目标目录)
# 
# "安装"的意思是把编译产物和相关文件复制到系统目录
# 执行 make install 时生效
# 
# 这里把 config/ 目录下的所有文件复制到安装目录的 config/ 子目录
# ${CMAKE_INSTALL_PREFIX} 默认是 /usr/local/
# 可以通过 cmake -DCMAKE_INSTALL_PREFIX=/opt/TakeAwayPlatform 修改
install(DIRECTORY ${CONFIG_DIR}/ DESTINATION ${CMAKE_INSTALL_PREFIX}/config)