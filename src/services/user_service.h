#pragma once

#include <string>
#include <functional>
#include <memory>
#include "db_handler.h"
#include "password_utils.h"
#include "jwt_utils.h"
#include "response_utils.h"

namespace TakeAwayPlatform {

// 获取 DB handler 的函数类型
using AcquireDbFunc = std::function<std::unique_ptr<DatabaseHandler>()>;

class UserService {
public:
    // 用户注册
    static Json::Value registerUser(AcquireDbFunc acquireDb, const Json::Value& body,
                                     const std::string& jwtSecret) {
        std::string username = body["username"].asString();
        std::string password = body["password"].asString();
        std::string phone = body.get("phone", "").asString();
        int role = body.get("role", 0).asInt();

        if (username.empty() || password.empty()) {
            return errorResponse(400, "用户名和密码不能为空");
        }

        auto db = acquireDb();

        // 检查用户名是否已存在
        Json::Value existing = db->queryPrepared(
            "SELECT id FROM user WHERE username = ?", {username});
        if (existing.size() > 0) {
            return errorResponse(400, "用户名已存在");
        }

        // 密码哈希
        std::string hash = PasswordUtils::hashPassword(password);

        // 插入用户
        int rows = db->executePrepared(
            "INSERT INTO user (username, password_hash, phone, role) VALUES (?, ?, ?, ?)",
            {username, hash, phone, std::to_string(role)});

        if (rows <= 0) {
            return errorResponse(500, "注册失败");
        }

        int64_t userId = db->getLastInsertId();

        Json::Value data;
        data["id"] = static_cast<Json::Int64>(userId);
        data["username"] = username;
        data["role"] = role;
        return successResponse("注册成功", data);
    }

    // 用户登录
    static Json::Value login(AcquireDbFunc acquireDb, const Json::Value& body,
                              const std::string& jwtSecret, int expireHours) {
        std::string username = body["username"].asString();
        std::string password = body["password"].asString();

        if (username.empty() || password.empty()) {
            return errorResponse(400, "用户名和密码不能为空");
        }

        auto db = acquireDb();

        Json::Value users = db->queryPrepared(
            "SELECT id, username, password_hash, phone, role, status FROM user WHERE username = ?",
            {username});

        if (users.size() == 0) {
            return errorResponse(401, "用户名或密码错误");
        }

        Json::Value user = users[0];

        // 检查账号状态
        if (user["status"].asInt() == 0) {
            return errorResponse(403, "账号已被冻结");
        }

        // 验证密码
        if (!PasswordUtils::verifyPassword(password, user["password_hash"].asString())) {
            return errorResponse(401, "用户名或密码错误");
        }

        int userId = user["id"].asInt();
        int role = user["role"].asInt();

        // 生成 JWT
        std::string token = JwtUtils::generateToken(userId, role, jwtSecret, expireHours);

        Json::Value userInfo;
        userInfo["id"] = userId;
        userInfo["username"] = user["username"].asString();
        userInfo["phone"] = user["phone"].asString();
        userInfo["role"] = role;

        Json::Value data;
        data["token"] = token;
        data["user"] = userInfo;
        return successResponse("登录成功", data);
    }

    // 获取个人信息
    static Json::Value getProfile(AcquireDbFunc acquireDb, int userId) {
        auto db = acquireDb();
        Json::Value users = db->queryPrepared(
            "SELECT id, username, phone, avatar, role, status, create_time FROM user WHERE id = ?",
            {std::to_string(userId)});

        if (users.size() == 0) {
            return errorResponse(404, "用户不存在");
        }

        Json::Value user = users[0];
        user.removeMember("password_hash"); // 安全：永远不返回密码
        return successResponse(user);
    }

    // 修改个人信息
    static Json::Value updateProfile(AcquireDbFunc acquireDb, int userId, const Json::Value& body) {
        auto db = acquireDb();

        // 构建动态更新 SQL
        std::vector<std::string> setClauses;
        std::vector<std::string> params;

        if (body.isMember("phone")) {
            setClauses.push_back("phone = ?");
            params.push_back(body["phone"].asString());
        }
        if (body.isMember("avatar")) {
            setClauses.push_back("avatar = ?");
            params.push_back(body["avatar"].asString());
        }

        if (setClauses.empty()) {
            return errorResponse(400, "没有需要更新的字段");
        }

        std::string sql = "UPDATE user SET ";
        for (size_t i = 0; i < setClauses.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += setClauses[i];
        }
        sql += " WHERE id = ?";
        params.push_back(std::to_string(userId));

        int rows = db->executePrepared(sql, params);
        if (rows <= 0) {
            return errorResponse(500, "更新失败");
        }

        return successResponse(std::string("更新成功"));
    }
};

} // namespace TakeAwayPlatform
