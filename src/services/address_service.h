#pragma once

#include <string>
#include <functional>
#include <memory>
#include "db_handler.h"
#include "response_utils.h"

namespace TakeAwayPlatform {

class AddressService {
public:
    // 获取地址列表
    static Json::Value getAddresses(AcquireDbFunc acquireDb, int userId) {
        auto db = acquireDb();
        Json::Value result = db->queryPrepared(
            "SELECT * FROM address WHERE user_id = ? ORDER BY is_default DESC, id DESC",
            {std::to_string(userId)});
        return successResponse(result);
    }

    // 新增地址
    static Json::Value addAddress(AcquireDbFunc acquireDb, int userId, const Json::Value& body) {
        std::string name = body["name"].asString();
        std::string phone = body["phone"].asString();
        std::string province = body.get("province", "").asString();
        std::string city = body.get("city", "").asString();
        std::string district = body.get("district", "").asString();
        std::string detail = body["detail"].asString();
        bool isDefault = body.get("isDefault", false).asBool();

        if (name.empty() || phone.empty() || detail.empty()) {
            return errorResponse(400, "姓名、电话和详细地址不能为空");
        }

        auto db = acquireDb();

        // 如果设为默认，先取消其他默认地址
        if (isDefault) {
            db->beginTransaction();
            db->executePrepared("UPDATE address SET is_default = 0 WHERE user_id = ?",
                                {std::to_string(userId)});
        }

        db->executePrepared(
            "INSERT INTO address (user_id, name, phone, province, city, district, detail, is_default) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            {std::to_string(userId), name, phone, province, city, district, detail, isDefault ? "1" : "0"});

        if (isDefault) {
            db->commit();
        }

        Json::Value data;
        data["addressId"] = static_cast<Json::Int64>(db->getLastInsertId());
        return successResponse("新增成功", data);
    }

    // 修改地址
    static Json::Value updateAddress(AcquireDbFunc acquireDb, int userId, int addressId,
                                      const Json::Value& body) {
        auto db = acquireDb();

        // 校验归属
        Json::Value addr = db->queryPrepared(
            "SELECT id FROM address WHERE id = ? AND user_id = ?",
            {std::to_string(addressId), std::to_string(userId)});
        if (addr.size() == 0) return errorResponse(404, "地址不存在");

        std::vector<std::string> sets;
        std::vector<std::string> params;
        if (body.isMember("name")) { sets.push_back("name = ?"); params.push_back(body["name"].asString()); }
        if (body.isMember("phone")) { sets.push_back("phone = ?"); params.push_back(body["phone"].asString()); }
        if (body.isMember("province")) { sets.push_back("province = ?"); params.push_back(body["province"].asString()); }
        if (body.isMember("city")) { sets.push_back("city = ?"); params.push_back(body["city"].asString()); }
        if (body.isMember("district")) { sets.push_back("district = ?"); params.push_back(body["district"].asString()); }
        if (body.isMember("detail")) { sets.push_back("detail = ?"); params.push_back(body["detail"].asString()); }

        if (sets.empty()) return errorResponse(400, "无更新内容");

        std::string sql = "UPDATE address SET ";
        for (size_t i = 0; i < sets.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += sets[i];
        }
        sql += " WHERE id = ?";
        params.push_back(std::to_string(addressId));

        db->executePrepared(sql, params);
        return successResponse(std::string("更新成功"));
    }

    // 删除地址
    static Json::Value deleteAddress(AcquireDbFunc acquireDb, int userId, int addressId) {
        auto db = acquireDb();
        Json::Value addr = db->queryPrepared(
            "SELECT id FROM address WHERE id = ? AND user_id = ?",
            {std::to_string(addressId), std::to_string(userId)});
        if (addr.size() == 0) return errorResponse(404, "地址不存在");

        db->executePrepared("DELETE FROM address WHERE id = ?", {std::to_string(addressId)});
        return successResponse(std::string("删除成功"));
    }

    // 设为默认地址
    static Json::Value setDefault(AcquireDbFunc acquireDb, int userId, int addressId) {
        auto db = acquireDb();
        Json::Value addr = db->queryPrepared(
            "SELECT id FROM address WHERE id = ? AND user_id = ?",
            {std::to_string(addressId), std::to_string(userId)});
        if (addr.size() == 0) return errorResponse(404, "地址不存在");

        db->beginTransaction();
        db->executePrepared("UPDATE address SET is_default = 0 WHERE user_id = ?",
                            {std::to_string(userId)});
        db->executePrepared("UPDATE address SET is_default = 1 WHERE id = ?",
                            {std::to_string(addressId)});
        db->commit();
        return successResponse(std::string("设置成功"));
    }
};

} // namespace TakeAwayPlatform
