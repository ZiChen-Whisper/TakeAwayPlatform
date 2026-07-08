#pragma once

#include <string>
#include <functional>
#include <memory>
#include <sstream>
#include "db_handler.h"
#include "response_utils.h"
#include "order_state_machine.h"

namespace TakeAwayPlatform {

class OrderService {
public:
    // 创建订单（事务：校验菜品、锁定库存、创建订单、清空购物车）
    static Json::Value createOrder(AcquireDbFunc acquireDb, int userId, const Json::Value& body) {
        int addressId = body["addressId"].asInt();
        std::string remark = body.get("remark", "").asString();

        if (addressId <= 0) return errorResponse(400, "请选择收货地址");

        auto db = acquireDb();

        // 查询地址快照
        Json::Value addr = db->queryPrepared(
            "SELECT * FROM address WHERE id = ? AND user_id = ?",
            {std::to_string(addressId), std::to_string(userId)});
        if (addr.size() == 0) return errorResponse(404, "地址不存在");

        // 查询购物车
        Json::Value cart = db->queryPrepared(
            "SELECT id FROM cart WHERE user_id = ?", {std::to_string(userId)});
        if (cart.size() == 0) return errorResponse(400, "购物车为空");

        int cartId = cart[0]["id"].asInt();

        // 查询购物车明细
        Json::Value items = db->queryPrepared(
            "SELECT ci.id, ci.dish_id, ci.quantity, d.name, d.price, d.stock, d.status, d.merchant_id "
            "FROM cart_item ci JOIN dish d ON ci.dish_id = d.id "
            "WHERE ci.cart_id = ?",
            {std::to_string(cartId)});

        if (items.size() == 0) return errorResponse(400, "购物车为空");

        // 校验菜品状态和库存
        double totalAmount = 0.0;
        for (auto& item : items) {
            if (item["status"].asInt() != 1) {
                return errorResponse(400, "菜品【" + item["name"].asString() + "】已下架");
            }
            if (item["stock"].asInt() < item["quantity"].asInt()) {
                return errorResponse(400, "菜品【" + item["name"].asString() + "】库存不足");
            }
            totalAmount += item["quantity"].asInt() * item["price"].asDouble();
        }
        totalAmount = std::round(totalAmount * 100) / 100.0;

        // 开启事务
        db->beginTransaction();

        try {
            // 锁定库存（FOR UPDATE）并扣减
            for (auto& item : items) {
                // 先锁定行
                Json::Value locked = db->queryPrepared(
                    "SELECT stock FROM dish WHERE id = ? FOR UPDATE",
                    {std::to_string(item["dish_id"].asInt())});
                if (locked.size() == 0 || locked[0]["stock"].asInt() < item["quantity"].asInt()) {
                    db->rollback();
                    return errorResponse(400, "菜品【" + item["name"].asString() + "】库存不足");
                }
                // 扣减库存
                db->executePrepared(
                    "UPDATE dish SET stock = stock - ? WHERE id = ?",
                    {std::to_string(item["quantity"].asInt()), std::to_string(item["dish_id"].asInt())});
            }

            // 创建订单主记录
            // 将地址序列化为 JSON 快照
            Json::StreamWriterBuilder writer;
            writer["indentation"] = "";
            std::string addressSnapshot = Json::writeString(writer, addr[0]);

            db->executePrepared(
                "INSERT INTO `order` (user_id, address_snapshot, remark, total_amount, status) "
                "VALUES (?, ?, ?, ?, 0)",
                {std::to_string(userId), addressSnapshot, remark, std::to_string(totalAmount)});

            int64_t orderId = db->getLastInsertId();

            // 创建订单明细
            for (auto& item : items) {
                db->executePrepared(
                    "INSERT INTO order_item (order_id, dish_id, dish_name, dish_price, quantity) "
                    "VALUES (?, ?, ?, ?, ?)",
                    {std::to_string(orderId),
                     std::to_string(item["dish_id"].asInt()),
                     item["name"].asString(),
                     std::to_string(item["price"].asDouble()),
                     std::to_string(item["quantity"].asInt())});
            }

            // 清空购物车
            db->executePrepared("DELETE FROM cart_item WHERE cart_id = ?", {std::to_string(cartId)});
            db->executePrepared("UPDATE cart SET total_price = 0 WHERE id = ?", {std::to_string(cartId)});

            // 提交事务
            db->commit();

            Json::Value data;
            data["orderId"] = static_cast<Json::Int64>(orderId);
            data["totalPrice"] = totalAmount;
            data["status"] = 0;
            data["statusName"] = OrderStateMachine::getStatusName(0);
            return successResponse("下单成功", data);

        } catch (const std::exception& e) {
            db->rollback();
            return errorResponse(500, std::string("下单失败: ") + e.what());
        }
    }

    // 模拟支付
    static Json::Value mockPayment(AcquireDbFunc acquireDb, int userId, int orderId) {
        auto db = acquireDb();

        // 查询订单
        Json::Value order = db->queryPrepared(
            "SELECT * FROM `order` WHERE id = ? AND user_id = ?",
            {std::to_string(orderId), std::to_string(userId)});
        if (order.size() == 0) return errorResponse(404, "订单不存在");

        int currentStatus = order[0]["status"].asInt();

        if (currentStatus != 0) {
            return errorResponse(400, "当前订单状态不允许支付");
        }

        // 模拟支付：90% 成功，10% 失败
        bool paySuccess = (rand() % 100) < 90;

        if (paySuccess) {
            db->executePrepared(
                "UPDATE `order` SET status = 1, pay_time = NOW() WHERE id = ?",
                {std::to_string(orderId)});

            Json::Value data;
            data["status"] = 1;
            data["statusName"] = OrderStateMachine::getStatusName(1);
            return successResponse("支付成功", data);
        } else {
            // 支付失败：取消订单，恢复库存
            db->beginTransaction();
            try {
                // 恢复库存
                Json::Value items = db->queryPrepared(
                    "SELECT dish_id, quantity FROM order_item WHERE order_id = ?",
                    {std::to_string(orderId)});
                for (auto& item : items) {
                    db->executePrepared(
                        "UPDATE dish SET stock = stock + ? WHERE id = ?",
                        {std::to_string(item["quantity"].asInt()), std::to_string(item["dish_id"].asInt())});
                }

                db->executePrepared(
                    "UPDATE `order` SET status = 4 WHERE id = ?",
                    {std::to_string(orderId)});
                db->commit();

                Json::Value data;
                data["status"] = 4;
                data["statusName"] = OrderStateMachine::getStatusName(4);
                return successResponse("支付失败，订单已取消", data);
            } catch (const std::exception& e) {
                db->rollback();
                return errorResponse(500, "处理失败");
            }
        }
    }

    // 查询用户订单列表
    static Json::Value getUserOrders(AcquireDbFunc acquireDb, int userId,
                                      int status, int page, int pageSize) {
        auto db = acquireDb();

        std::ostringstream sql;
        sql << "SELECT * FROM `order` WHERE user_id = ? ";
        std::vector<std::string> params = {std::to_string(userId)};

        if (status >= 0 && status <= 4) {
            sql << "AND status = ? ";
            params.push_back(std::to_string(status));
        }

        sql << "ORDER BY create_time DESC LIMIT ? OFFSET ?";
        params.push_back(std::to_string(pageSize));
        params.push_back(std::to_string((page - 1) * pageSize));

        Json::Value list = db->queryPrepared(sql.str(), params);

        // 查询总数
        std::ostringstream countSql;
        countSql << "SELECT COUNT(*) as total FROM `order` WHERE user_id = ? ";
        if (status >= 0 && status <= 4) countSql << "AND status = ? ";

        std::vector<std::string> countParams = {std::to_string(userId)};
        if (status >= 0 && status <= 4) countParams.push_back(std::to_string(status));

        Json::Value countResult = db->queryPrepared(countSql.str(), countParams);
        int total = countResult.size() > 0 ? countResult[0]["total"].asInt() : 0;

        Json::Value data;
        data["list"] = list;
        data["total"] = total;
        data["page"] = page;
        data["pageSize"] = pageSize;
        return successResponse(data);
    }

    // 查询订单详情
    static Json::Value getOrderDetail(AcquireDbFunc acquireDb, int userId, int orderId) {
        auto db = acquireDb();

        Json::Value order = db->queryPrepared(
            "SELECT * FROM `order` WHERE id = ? AND user_id = ?",
            {std::to_string(orderId), std::to_string(userId)});
        if (order.size() == 0) return errorResponse(404, "订单不存在");

        Json::Value items = db->queryPrepared(
            "SELECT * FROM order_item WHERE order_id = ?",
            {std::to_string(orderId)});

        Json::Value data;
        data["order"] = order[0];
        data["items"] = items;
        return successResponse(data);
    }

    // 商家端：查询待处理订单
    static Json::Value getMerchantOrders(AcquireDbFunc acquireDb, int merchantId,
                                          int status, int page, int pageSize) {
        auto db = acquireDb();

        // 查询该商家菜品相关的订单
        std::ostringstream sql;
        sql << "SELECT DISTINCT o.* FROM `order` o "
            << "JOIN order_item oi ON o.id = oi.order_id "
            << "JOIN dish d ON oi.dish_id = d.id "
            << "WHERE d.merchant_id = ? ";
        std::vector<std::string> params = {std::to_string(merchantId)};

        if (status >= 0 && status <= 4) {
            sql << "AND o.status = ? ";
            params.push_back(std::to_string(status));
        }

        sql << "ORDER BY o.create_time DESC LIMIT ? OFFSET ?";
        params.push_back(std::to_string(pageSize));
        params.push_back(std::to_string((page - 1) * pageSize));

        Json::Value list = db->queryPrepared(sql.str(), params);
        Json::Value data;
        data["list"] = list;
        data["page"] = page;
        data["pageSize"] = pageSize;
        return successResponse(data);
    }

    // 商家接单
    static Json::Value acceptOrder(AcquireDbFunc acquireDb, int merchantId, int orderId) {
        auto db = acquireDb();

        // 校验订单中有该商家的菜品
        Json::Value check = db->queryPrepared(
            "SELECT COUNT(*) as cnt FROM order_item oi "
            "JOIN dish d ON oi.dish_id = d.id "
            "WHERE oi.order_id = ? AND d.merchant_id = ?",
            {std::to_string(orderId), std::to_string(merchantId)});
        if (check.size() == 0 || check[0]["cnt"].asInt() == 0) {
            return errorResponse(403, "无权操作该订单");
        }

        Json::Value order = db->queryPrepared(
            "SELECT status FROM `order` WHERE id = ?", {std::to_string(orderId)});
        if (order.size() == 0) return errorResponse(404, "订单不存在");

        int current = order[0]["status"].asInt();
        if (current != 1) return errorResponse(400, "当前状态不允许接单");

        db->executePrepared(
            "UPDATE `order` SET status = 2, accept_time = NOW() WHERE id = ?",
            {std::to_string(orderId)});

        Json::Value data;
        data["status"] = 2;
        data["statusName"] = OrderStateMachine::getStatusName(2);
        return successResponse("接单成功", data);
    }

    // 商家标记完成
    static Json::Value completeOrder(AcquireDbFunc acquireDb, int merchantId, int orderId) {
        auto db = acquireDb();

        Json::Value check = db->queryPrepared(
            "SELECT COUNT(*) as cnt FROM order_item oi "
            "JOIN dish d ON oi.dish_id = d.id "
            "WHERE oi.order_id = ? AND d.merchant_id = ?",
            {std::to_string(orderId), std::to_string(merchantId)});
        if (check.size() == 0 || check[0]["cnt"].asInt() == 0) {
            return errorResponse(403, "无权操作该订单");
        }

        Json::Value order = db->queryPrepared(
            "SELECT status FROM `order` WHERE id = ?", {std::to_string(orderId)});
        if (order.size() == 0) return errorResponse(404, "订单不存在");

        int current = order[0]["status"].asInt();
        if (current != 2) return errorResponse(400, "当前状态不允许完成");

        db->executePrepared(
            "UPDATE `order` SET status = 3, complete_time = NOW() WHERE id = ?",
            {std::to_string(orderId)});

        Json::Value data;
        data["status"] = 3;
        data["statusName"] = OrderStateMachine::getStatusName(3);
        return successResponse("订单已完成", data);
    }
};

} // namespace TakeAwayPlatform
