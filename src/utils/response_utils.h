#pragma once

#include <json/json.h>
#include <string>

namespace TakeAwayPlatform {

// 统一 JSON 响应格式工具
// 所有 API 均返回: {"code": xxx, "message": "xxx", "data": {...}}

// 只有 data（默认 message="success"）
inline Json::Value successResponse(const Json::Value& data) {
    Json::Value resp;
    resp["code"] = 200;
    resp["message"] = "success";
    resp["data"] = data;
    return resp;
}

// 只有 message（data 为空对象）
inline Json::Value successResponse(const std::string& message) {
    Json::Value resp;
    resp["code"] = 200;
    resp["message"] = message;
    resp["data"] = Json::Value(Json::objectValue);
    return resp;
}

// message + data
inline Json::Value successResponse(const std::string& message, const Json::Value& data) {
    Json::Value resp;
    resp["code"] = 200;
    resp["message"] = message;
    resp["data"] = data;
    return resp;
}

inline Json::Value errorResponse(int code, const std::string& message) {
    Json::Value resp;
    resp["code"] = code;
    resp["message"] = message;
    resp["data"] = Json::Value(Json::nullValue);
    return resp;
}

} // namespace TakeAwayPlatform
