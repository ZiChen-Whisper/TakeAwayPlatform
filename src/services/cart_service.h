#pragma once

#include <string>
#include <functional>
#include <memory>
#include <cmath>
#include "db_handler.h"
#include "response_utils.h"

namespace TakeAwayPlatform {

class CartService {
private:
    // 获取或创建用户的购物车
    static int getOrCreateCart(AcquireDbFunc acquireDb, int userId) {
        auto db = acquireDb();
        Json::Value result = db->queryPrepared(
            "SELECT id FROM cart WHERE user_id = ?", {std::to_string(userId)});
        if (result.size() > 0) {
            return result[0]["id"].asInt();
        }
        db->executePrepared(
            "INSERT INTO cart (user_id, total_price) VALUES (?, 0.00)",
            {std::to_string(userId)});
        return static_cast<int>(db->getLastInsertId());
    }

    // 重算购物车总价
    static void recalculateTotal(std::unique_ptr<DatabaseHandler>& db, int cartId) {
        Json::Value items = db->queryPrepared(
            "SELECT quantity, dish_price FROM cart_item WHERE cart_id = ?",
            {std::to_string(cartId)});
        double total = 0.0;
        for (auto& item : items) {
            total += item["quantity"].asInt() * item["dish_price"].asDouble();
        }
        total = std::round(total * 100) / 100.0;
        db->executePrepared(
            "UPDATE cart SET total_price = ? WHERE id = ?",
            {std::to_string(total), std::to_string(cartId)});
    }

public:
    // 获取购物车详情
    static Json::Value getCart(AcquireDbFunc acquireDb, int userId) {
        auto db = acquireDb();
        int cartId = getOrCreateCart(acquireDb, userId);

        Json::Value items = db->queryPrepared(
            "SELECT ci.id, ci.dish_id, ci.quantity, ci.dish_price, d.name, d.image "
            "FROM cart_item ci JOIN dish d ON ci.dish_id = d.id "
            "WHERE ci.cart_id = ?",
            {std::to_string(cartId)});

        Json::Value cartInfo = db->queryPrepared(
            "SELECT id, total_price FROM cart WHERE id = ?", {std::to_string(cartId)});
        double totalPrice = cartInfo.size() > 0 ? cartInfo[0]["total_price"].asDouble() : 0.0;

        Json::Value data;
        data["cartId"] = cartId;
        data["items"] = items;
        data["totalPrice"] = totalPrice;
        return successResponse(data);
    }

    // 加入购物车
    static Json::Value addToCart(AcquireDbFunc acquireDb, int userId, const Json::Value& body) {
        int dishId = body["dishId"].asInt();
        int quantity = body.get("quantity", 1).asInt();

        if (dishId <= 0 || quantity <= 0) return errorResponse(400, "参数错误");

        auto db = acquireDb();

        // 检查菜品是否上架且有库存
        Json::Value dish = db->queryPrepared(
            "SELECT id, name, price, stock, status FROM dish WHERE id = ?", {std::to_string(dishId)});
        if (dish.size() == 0) return errorResponse(404, "菜品不存在");
        if (dish[0]["status"].asInt() != 1) return errorResponse(400, "菜品已下架");
        if (dish[0]["stock"].asInt() < quantity) return errorResponse(400, "库存不足");

        int cartId = getOrCreateCart(acquireDb, userId);
        double dishPrice = dish[0]["price"].asDouble();

        // 检查是否已在购物车中
        Json::Value existing = db->queryPrepared(
            "SELECT id, quantity FROM cart_item WHERE cart_id = ? AND dish_id = ?",
            {std::to_string(cartId), std::to_string(dishId)});

        if (existing.size() > 0) {
            // 已存在，更新数量
            int newQty = existing[0]["quantity"].asInt() + quantity;
            db->executePrepared(
                "UPDATE cart_item SET quantity = ?, dish_price = ? WHERE id = ?",
                {std::to_string(newQty), std::to_string(dishPrice), std::to_string(existing[0]["id"].asInt())});
        } else {
            // 新增
            db->executePrepared(
                "INSERT INTO cart_item (cart_id, dish_id, quantity, dish_price) VALUES (?, ?, ?, ?)",
                {std::to_string(cartId), std::to_string(dishId), std::to_string(quantity), std::to_string(dishPrice)});
        }

        recalculateTotal(db, cartId);

        // 返回更新后的购物车
        return getCart(acquireDb, userId);
    }

    // 修改购物车菜品数量
    static Json::Value updateQuantity(AcquireDbFunc acquireDb, int userId, int itemId,
                                       const Json::Value& body) {
        int quantity = body["quantity"].asInt();
        if (quantity <= 0) return errorResponse(400, "数量必须大于0");

        auto db = acquireDb();
        int cartId = getOrCreateCart(acquireDb, userId);

        // 校验归属
        Json::Value item = db->queryPrepared(
            "SELECT id, dish_id FROM cart_item WHERE id = ? AND cart_id = ?",
            {std::to_string(itemId), std::to_string(cartId)});
        if (item.size() == 0) return errorResponse(404, "购物车项不存在");

        // 更新价格快照和数量
        Json::Value dish = db->queryPrepared(
            "SELECT price FROM dish WHERE id = ?", {std::to_string(item[0]["dish_id"].asInt())});
        double price = dish.size() > 0 ? dish[0]["price"].asDouble() : 0;

        db->executePrepared(
            "UPDATE cart_item SET quantity = ?, dish_price = ? WHERE id = ?",
            {std::to_string(quantity), std::to_string(price), std::to_string(itemId)});

        recalculateTotal(db, cartId);

        Json::Value cartInfo = db->queryPrepared(
            "SELECT total_price FROM cart WHERE id = ?", {std::to_string(cartId)});
        Json::Value data;
        data["totalPrice"] = cartInfo.size() > 0 ? cartInfo[0]["total_price"].asDouble() : 0.0;
        return successResponse(data);
    }

    // 移出购物车
    static Json::Value removeItem(AcquireDbFunc acquireDb, int userId, int itemId) {
        auto db = acquireDb();
        int cartId = getOrCreateCart(acquireDb, userId);

        db->executePrepared(
            "DELETE FROM cart_item WHERE id = ? AND cart_id = ?",
            {std::to_string(itemId), std::to_string(cartId)});

        recalculateTotal(db, cartId);
        return successResponse(std::string("已移除"));
    }
};

} // namespace TakeAwayPlatform
