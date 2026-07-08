#pragma once

#include <string>
#include <functional>
#include <memory>
#include <sstream>
#include "db_handler.h"
#include "response_utils.h"

namespace TakeAwayPlatform {

class DishService {
public:
    // 用户端：搜索/筛选/分页查询菜品
    static Json::Value searchDishes(AcquireDbFunc acquireDb,
                                     int categoryId, const std::string& keyword,
                                     int page, int pageSize) {
        auto db = acquireDb();

        std::ostringstream sql;
        sql << "SELECT d.id, d.merchant_id, d.category_id, d.name, d.image, d.description, "
            << "d.price, d.stock, d.status, d.sort_weight, d.create_time, "
            << "c.name as category_name "
            << "FROM dish d "
            << "JOIN category c ON d.category_id = c.id "
            << "WHERE d.status = 1 ";

        std::vector<std::string> params;

        if (categoryId > 0) {
            sql << "AND d.category_id = ? ";
            params.push_back(std::to_string(categoryId));
        }
        if (!keyword.empty()) {
            sql << "AND d.name LIKE ? ";
            params.push_back("%" + keyword + "%");
        }

        // 查询总数
        std::ostringstream countSql;
        countSql << "SELECT COUNT(*) as total FROM dish d WHERE d.status = 1 ";
        if (categoryId > 0) countSql << "AND d.category_id = ? ";
        if (!keyword.empty()) countSql << "AND d.name LIKE ? ";

        Json::Value countResult;
        if (params.empty()) {
            countResult = db->query(countSql.str());
        } else {
            countResult = db->queryPrepared(countSql.str(), params);
        }
        int total = countResult.size() > 0 ? countResult[0]["total"].asInt() : 0;

        // 排序和分页
        sql << "ORDER BY d.sort_weight DESC, d.create_time DESC "
            << "LIMIT ? OFFSET ?";
        params.push_back(std::to_string(pageSize));
        params.push_back(std::to_string((page - 1) * pageSize));

        Json::Value list = db->queryPrepared(sql.str(), params);

        Json::Value data;
        data["list"] = list;
        data["total"] = total;
        data["page"] = page;
        data["pageSize"] = pageSize;
        return successResponse(data);
    }

    // 获取菜品详情
    static Json::Value getDishDetail(AcquireDbFunc acquireDb, int dishId) {
        auto db = acquireDb();
        Json::Value result = db->queryPrepared(
            "SELECT d.id, d.merchant_id, d.category_id, d.name, d.image, d.description, "
            "d.price, d.stock, d.status, d.sort_weight, d.create_time, "
            "c.name as category_name, m.name as merchant_name "
            "FROM dish d "
            "JOIN category c ON d.category_id = c.id "
            "JOIN merchant m ON d.merchant_id = m.id "
            "WHERE d.id = ?",
            {std::to_string(dishId)});

        if (result.size() == 0) {
            return errorResponse(404, "菜品不存在");
        }
        return successResponse(result[0]);
    }

    // 商家端：新增菜品
    static Json::Value addDish(AcquireDbFunc acquireDb, int merchantId, const Json::Value& body) {
        std::string name = body["name"].asString();
        double price = body["price"].asDouble();
        int categoryId = body["categoryId"].asInt();
        std::string description = body.get("description", "").asString();
        std::string image = body.get("image", "").asString();
        int stock = body.get("stock", 0).asInt();

        if (name.empty() || price <= 0 || categoryId <= 0) {
            return errorResponse(400, "菜品名称、价格和分类不能为空");
        }

        auto db = acquireDb();
        int rows = db->executePrepared(
            "INSERT INTO dish (merchant_id, category_id, name, image, description, price, stock) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            {std::to_string(merchantId), std::to_string(categoryId), name, image, description,
             std::to_string(price), std::to_string(stock)});

        if (rows <= 0) {
            return errorResponse(500, "新增菜品失败");
        }

        Json::Value data;
        data["dishId"] = static_cast<Json::Int64>(db->getLastInsertId());
        return successResponse("新增成功", data);
    }

    // 商家端：修改菜品
    static Json::Value updateDish(AcquireDbFunc acquireDb, int dishId, int merchantId,
                                   const Json::Value& body) {
        auto db = acquireDb();

        // 校验归属
        Json::Value dish = db->queryPrepared(
            "SELECT merchant_id FROM dish WHERE id = ?", {std::to_string(dishId)});
        if (dish.size() == 0) return errorResponse(404, "菜品不存在");
        if (dish[0]["merchant_id"].asInt() != merchantId) {
            return errorResponse(403, "无权操作该菜品");
        }

        // 动态构建更新
        std::vector<std::string> sets;
        std::vector<std::string> params;
        if (body.isMember("name")) { sets.push_back("name = ?"); params.push_back(body["name"].asString()); }
        if (body.isMember("price")) { sets.push_back("price = ?"); params.push_back(std::to_string(body["price"].asDouble())); }
        if (body.isMember("categoryId")) { sets.push_back("category_id = ?"); params.push_back(std::to_string(body["categoryId"].asInt())); }
        if (body.isMember("description")) { sets.push_back("description = ?"); params.push_back(body["description"].asString()); }
        if (body.isMember("image")) { sets.push_back("image = ?"); params.push_back(body["image"].asString()); }
        if (body.isMember("stock")) { sets.push_back("stock = ?"); params.push_back(std::to_string(body["stock"].asInt())); }

        if (sets.empty()) return errorResponse(400, "没有需要更新的字段");

        std::string sql = "UPDATE dish SET ";
        for (size_t i = 0; i < sets.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += sets[i];
        }
        sql += " WHERE id = ?";
        params.push_back(std::to_string(dishId));

        db->executePrepared(sql, params);
        return successResponse(std::string("更新成功"));
    }

    // 商家端：上下架
    static Json::Value setDishStatus(AcquireDbFunc acquireDb, int dishId, int merchantId, int status) {
        auto db = acquireDb();
        Json::Value dish = db->queryPrepared(
            "SELECT merchant_id FROM dish WHERE id = ?", {std::to_string(dishId)});
        if (dish.size() == 0) return errorResponse(404, "菜品不存在");
        if (dish[0]["merchant_id"].asInt() != merchantId) {
            return errorResponse(403, "无权操作该菜品");
        }

        db->executePrepared("UPDATE dish SET status = ? WHERE id = ?",
                            {std::to_string(status), std::to_string(dishId)});
        return successResponse(std::string("状态更新成功"));
    }

    // 商家端：删除菜品
    static Json::Value deleteDish(AcquireDbFunc acquireDb, int dishId, int merchantId) {
        auto db = acquireDb();
        Json::Value dish = db->queryPrepared(
            "SELECT merchant_id FROM dish WHERE id = ?", {std::to_string(dishId)});
        if (dish.size() == 0) return errorResponse(404, "菜品不存在");
        if (dish[0]["merchant_id"].asInt() != merchantId) {
            return errorResponse(403, "无权操作该菜品");
        }

        db->executePrepared("DELETE FROM dish WHERE id = ?", {std::to_string(dishId)});
        return successResponse(std::string("删除成功"));
    }

    // 商家端：查询自己的菜品列表
    static Json::Value getMerchantDishes(AcquireDbFunc acquireDb, int merchantId,
                                          int categoryId, int page, int pageSize) {
        auto db = acquireDb();

        std::ostringstream sql;
        sql << "SELECT d.id, d.merchant_id, d.category_id, d.name, d.image, d.description, "
            << "d.price, d.stock, d.status, d.sort_weight, d.create_time, c.name as category_name FROM dish d "
            << "JOIN category c ON d.category_id = c.id "
            << "WHERE d.merchant_id = ? ";
        std::vector<std::string> params = {std::to_string(merchantId)};

        if (categoryId > 0) {
            sql << "AND d.category_id = ? ";
            params.push_back(std::to_string(categoryId));
        }

        sql << "ORDER BY d.create_time DESC LIMIT ? OFFSET ?";
        params.push_back(std::to_string(pageSize));
        params.push_back(std::to_string((page - 1) * pageSize));

        Json::Value list = db->queryPrepared(sql.str(), params);
        Json::Value data;
        data["list"] = list;
        data["page"] = page;
        data["pageSize"] = pageSize;
        return successResponse(data);
    }
};

} // namespace TakeAwayPlatform
