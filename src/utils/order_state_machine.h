#pragma once

#include <string>
#include <set>

namespace TakeAwayPlatform {

// 订单状态枚举
enum class OrderStatus {
    PENDING_PAYMENT = 0,    // 待支付
    PENDING_ACCEPTANCE = 1,  // 待接单
    DELIVERING = 2,          // 配送中
    COMPLETED = 3,           // 已完成
    CANCELLED = 4            // 已取消
};

class OrderStateMachine {
public:
    // 检查状态转换是否合法
    static bool isValidTransition(int from, int to) {
        static const std::set<std::pair<int, int>> validTransitions = {
            {0, 1}, // 待支付 → 待接单（支付成功）
            {0, 4}, // 待支付 → 已取消（支付失败/取消订单）
            {1, 2}, // 待接单 → 配送中（商家接单）
            {1, 4}, // 待接单 → 已取消（商家拒单）
            {2, 3}, // 配送中 → 已完成（确认收货）
        };
        return validTransitions.count({from, to}) > 0;
    }

    // 判断是否为终态
    static bool isFinalState(int status) {
        return status == 3 || status == 4;
    }

    // 获取状态名称
    static std::string getStatusName(int status) {
        switch (status) {
            case 0: return "待支付";
            case 1: return "待接单";
            case 2: return "配送中";
            case 3: return "已完成";
            case 4: return "已取消";
            default: return "未知";
        }
    }
};

} // namespace TakeAwayPlatform
