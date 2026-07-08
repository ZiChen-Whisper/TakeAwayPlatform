#include <iostream>

#include "db_handler.h"
#include <mysql_driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset_metadata.h>


namespace TakeAwayPlatform
{
    DatabaseHandler::DatabaseHandler(const DBConfig& config)
    {
        connect(config);
    }

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

    bool DatabaseHandler::is_connected() const 
    {
        if (!connection || connection->isClosed()) {
            return false;
        }

        try {
            std::unique_ptr<sql::Statement> stmt(connection->createStatement());
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT 1"));
            return res->next();
        } catch (const sql::SQLException& e) {
            return false;
        }
    }

    void DatabaseHandler::reconnect() 
    {
        connection.reset();
        connect(dbConfig);
    }

    void DatabaseHandler::connect(const DBConfig& config)
    {
        dbConfig = config;
        
        try 
        {
            std::cout << "DatabaseHandler::connect host:" << config.host << std::endl;
            std::cout << "DatabaseHandler::connect port:" << config.port << std::endl;
            std::cout << "DatabaseHandler::connect user:" << config.user << std::endl;
            std::cout << "DatabaseHandler::connect password:" << config.password << std::endl;
            std::cout.flush();

            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
            std::string hostUrl = "tcp://" + config.host + ":" + std::to_string(config.port);
            connection.reset(driver->connect(hostUrl, config.user, config.password));
            connection->setSchema(config.database);

            std::cout << "Database connection successful." << std::endl;
            std::cout.flush();
        } 
        catch (const sql::SQLException& e) 
        {
            std::cerr << "Database connection failed: " << e.what() << std::endl;
            connection.reset();
        }
    }

    Json::Value DatabaseHandler::parse_result(sql::ResultSet* result) 
    {
        Json::Value json_result(Json::arrayValue);
        sql::ResultSetMetaData* meta = result->getMetaData();
        uint32_t colCount = meta->getColumnCount();
        
        while (result->next()) 
        {
            Json::Value json_row;
            for (uint32_t index = 1; index <= colCount; ++index) 
            {
                std::string column_name = meta->getColumnName(index);

                switch (meta->getColumnType(index)) 
                {
                    case sql::DataType::VARCHAR:
                    case sql::DataType::CHAR:
                    case sql::DataType::LONGVARCHAR:
                        json_row[column_name] = static_cast<std::string>(result->getString(index));
                        break;

                    case sql::DataType::TINYINT:
                    case sql::DataType::SMALLINT:
                    case sql::DataType::INTEGER:
                        json_row[column_name] = result->getInt(index);
                        break;

                    case sql::DataType::BIGINT:
                        json_row[column_name] = static_cast<Json::Int64>(result->getInt64(index));
                        break;

                    case sql::DataType::REAL:
                    case sql::DataType::DOUBLE:
                    case sql::DataType::DECIMAL:
                        json_row[column_name] = static_cast<double>(result->getDouble(index));
                        break;

                    case sql::DataType::BIT:
                        json_row[column_name] = result->getBoolean(index);
                        break;

                    default:
                        json_row[column_name] = static_cast<std::string>(result->getString(index));
                        break;
                }
            }

            json_result.append(json_row);
        }

        return json_result;
    }
}