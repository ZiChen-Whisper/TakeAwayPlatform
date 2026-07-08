#pragma once

#include <httplib.h>
#include <string>
#include "jwt_utils.h"
#include "response_utils.h"

namespace TakeAwayPlatform {

// JWT 鉴权中间件
class AuthMiddleware {
public:
    // 从请求头提取并验证 JWT Token
    // 成功返回 payload，失败返回空的 Json::Value 并设置 401 响应
    static Json::Value authenticate(const httplib::Request& req, httplib::Response& res,
                                     const std::string& jwtSecret) {
        // 从 Authorization 头提取 token
        std::string authHeader = req.get_header_value("Authorization");
        if (authHeader.empty()) {
            Json::Value err = errorResponse(401, "未提供认证令牌");
            res.set_content(err.toStyledString(), "application/json");
            res.status = 401;
            return Json::Value(Json::nullValue);
        }

        // 格式: "Bearer <token>"
        if (authHeader.size() < 8 || authHeader.substr(0, 7) != "Bearer ") {
            Json::Value err = errorResponse(401, "认证令牌格式错误");
            res.set_content(err.toStyledString(), "application/json");
            res.status = 401;
            return Json::Value(Json::nullValue);
        }

        std::string token = authHeader.substr(7);

        // 验证 token
        Json::Value payload = JwtUtils::verifyToken(token, jwtSecret);
        if (payload.isNull()) {
            Json::Value err = errorResponse(401, "认证令牌无效或已过期");
            res.set_content(err.toStyledString(), "application/json");
            res.status = 401;
            return Json::Value(Json::nullValue);
        }

        return payload;
    }

    // RBAC 角色校验：payload 中 role 必须匹配
    static bool requireRole(const Json::Value& payload, httplib::Response& res,
                            const std::vector<int>& allowedRoles) {
        if (!payload.isMember("role")) {
            Json::Value err = errorResponse(403, "无权限");
            res.set_content(err.toStyledString(), "application/json");
            res.status = 403;
            return false;
        }

        int role = payload["role"].asInt();
        for (int allowed : allowedRoles) {
            if (role == allowed) return true;
        }

        Json::Value err = errorResponse(403, "无权限，需要更高权限");
        res.set_content(err.toStyledString(), "application/json");
        res.status = 403;
        return false;
    }
};

} // namespace TakeAwayPlatform
