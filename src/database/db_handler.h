#pragma once

#include "common.h"
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <memory>

namespace TakeAwayPlatform
{

    class DatabaseHandler 
    {
    public:
        DatabaseHandler(const DBConfig& config);

        Json::Value query(const std::string& sql);

        bool is_connected() const;

        void reconnect();

        
    private:
        void connect(const DBConfig& config);

        Json::Value parse_result(sql::ResultSet* result);


    private:
        DBConfig dbConfig;
        std::unique_ptr<sql::Connection> connection;
    };

}