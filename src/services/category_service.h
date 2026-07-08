#pragma once

#include <string>
#include <functional>
#include <memory>
#include "db_handler.h"
#include "response_utils.h"

namespace TakeAwayPlatform {

class CategoryService {
public:
    // 用户端：获取某商家的分类列表（只返回有上架菜品的分类）
    static Json::Value getCategories(AcquireDbFunc acquireDb, int merchantId) {
        auto db = acquireDb();
        Json::Value result = db->queryPrepared(
            "SELECT DISTINCT c.id, c.name, c.sort_weight "
            "FROM category c "
            "JOIN dish d ON c.id = d.category_id "
            "WHERE c.merchant_id = ? AND d.status = 1 "
            "ORDER BY c.sort_weight DESC",
            {std::to_string(merchantId)});
        return successResponse(result);
    }

    // 商家端：新增分类
    static Json::Value addCategory(AcquireDbFunc acquireDb, int merchantId, const Json::Value& body) {
        std::string name = body["name"].asString();
        int sortWeight = body.get("sortWeight", 0).asInt();

        if (name.empty()) return errorResponse(400, "分类名不能为空");

        auto db = acquireDb();

        // 检查同名
        Json::Value existing = db->queryPrepared(
            "SELECT id FROM category WHERE merchant_id = ? AND name = ?",
            {std::to_string(merchantId), name});
        if (existing.size() > 0) return errorResponse(400, "分类名已存在");

        db->executePrepared(
            "INSERT INTO category (merchant_id, name, sort_weight) VALUES (?, ?, ?)",
            {std::to_string(merchantId), name, std::to_string(sortWeight)});

        Json::Value data;
        data["categoryId"] = static_cast<Json::Int64>(db->getLastInsertId());
        return successResponse("新增成功", data);
    }

    // 商家端：修改分类
    static Json::Value updateCategory(AcquireDbFunc acquireDb, int categoryId, int merchantId,
                                       const Json::Value& body) {
        auto db = acquireDb();
        Json::Value cat = db->queryPrepared(
            "SELECT merchant_id FROM category WHERE id = ?", {std::to_string(categoryId)});
        if (cat.size() == 0) return errorResponse(404, "分类不存在");
        if (cat[0]["merchant_id"].asInt() != merchantId) return errorResponse(403, "无权操作");

        std::vector<std::string> sets;
        std::vector<std::string> params;
        if (body.isMember("name")) { sets.push_back("name = ?"); params.push_back(body["name"].asString()); }
        if (body.isMember("sortWeight")) { sets.push_back("sort_weight = ?"); params.push_back(std::to_string(body["sortWeight"].asInt())); }

        if (sets.empty()) return errorResponse(400, "无更新内容");

        std::string sql = "UPDATE category SET ";
        for (size_t i = 0; i < sets.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += sets[i];
        }
        sql += " WHERE id = ?";
        params.push_back(std::to_string(categoryId));

        db->executePrepared(sql, params);
        return successResponse(std::string("更新成功"));
    }

    // 商家端：获取自己的全部分类
    static Json::Value getMerchantCategories(AcquireDbFunc acquireDb, int merchantId) {
        auto db = acquireDb();
        Json::Value result = db->queryPrepared(
            "SELECT id, name, sort_weight FROM category WHERE merchant_id = ? ORDER BY sort_weight DESC",
            {std::to_string(merchantId)});
        return successResponse(result);
    }
};

} // namespace TakeAwayPlatform
