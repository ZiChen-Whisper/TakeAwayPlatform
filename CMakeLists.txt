cmake_minimum_required(VERSION 3.12)
# 编译出来的软件名字
project(TakeAwayPlatform)

# 版本
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置项目目录
set(PROJECT_ROOT ${CMAKE_SOURCE_DIR})  # 项目根目录
set(SOURCE_DIR ${PROJECT_ROOT}/src)  # 源文件目录
set(INCLUDE_DIR ${PROJECT_ROOT}/include)  # 头文件目录
set(CONFIG_DIR ${PROJECT_ROOT}/config)  # 配置文件目录

# 设置头文件路径
include_directories(
    ${INCLUDE_DIR}  # 头文件目录
    ${SOURCE_DIR}/database  # 数据库源文件目录
    ${SOURCE_DIR}/http  # HTTP源文件目录
)

# 添加 cpp-httplib
set(CPP_HTTPLIB_INCLUDE_DIR ${PROJECT_ROOT}/lib/cpp-httplib/include)
include_directories(${CPP_HTTPLIB_INCLUDE_DIR})

# 添加 MySQL Connector
set(MYSQL_CONNECTOR_ROOT ${PROJECT_ROOT}/lib/mysql-connector)
include_directories(
    ${MYSQL_CONNECTOR_ROOT}/include
    ${MYSQL_CONNECTOR_ROOT}/include/jdbc
    ${MYSQL_CONNECTOR_ROOT}/include/mysqlx
)

# 添加 JSONCPP
set(JSONCPP_ROOT ${PROJECT_ROOT}/lib/jsoncpp)
include_directories(${JSONCPP_ROOT}/src/include)

# 添加 JSONCPP 源文件
file(GLOB_RECURSE JSONCPP_SOURCES
    ${JSONCPP_ROOT}/src/src/lib_json/*.cpp
    ${JSONCPP_ROOT}/src/src/lib_json/*.h
)

# 创建 JSONCPP 静态库
add_library(jsoncpp_lib STATIC ${JSONCPP_SOURCES})

# 添加 MySQL Connector 库路径
# link_directories(${MYSQL_CONNECTOR_ROOT}/lib)
# 优化MySQL connector 库文件路径
link_directories(/opt/TakeAwayPlatform/bin/lib)

# 添加源文件
file(GLOB_RECURSE SOURCES 
    ${SOURCE_DIR}/*.cpp
    ${SOURCE_DIR}/utils/*.cpp
    ${SOURCE_DIR}/*.h
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})  # PROJECT_NAME 是项目名称，SOURCES 是源文件列表

# 链接库
target_link_libraries(${PROJECT_NAME}
    PRIVATE 
        jsoncpp_lib  # 链接 JSONCPP 库
        mysqlcppconn
        ssl
        crypto
        z
        pthread     # 链接 cpp-httplib 所需的 pthread 库
)

# 设置运行时库路径
set(CMAKE_INSTALL_RPATH "${MYSQL_CONNECTOR_ROOT}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# 安装配置文件
install(DIRECTORY ${CONFIG_DIR}/ DESTINATION ${CMAKE_INSTALL_PREFIX}/config)