-- TakeAwayPlatform 数据库初始化脚本
-- 基于需求说明书和设计说明书的 7.5 数据字典

CREATE DATABASE IF NOT EXISTS TakeAwayDatabase
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE TakeAwayDatabase;

-- ============================================
-- 1. 用户表 (user)
-- 角色: 0=普通用户, 1=商家, 2=管理员
-- 状态: 1=正常, 0=冻结
-- ============================================
CREATE TABLE IF NOT EXISTS `user` (
    `id`            BIGINT          NOT NULL AUTO_INCREMENT,
    `username`      VARCHAR(64)     NOT NULL,
    `password_hash` VARCHAR(256)    NOT NULL,
    `phone`         VARCHAR(20)     DEFAULT '',
    `avatar`        VARCHAR(512)    DEFAULT '',
    `role`          TINYINT         NOT NULL DEFAULT 0 COMMENT '0=用户,1=商家,2=管理员',
    `status`        TINYINT         NOT NULL DEFAULT 1 COMMENT '1=正常,0=冻结',
    `create_time`   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 2. 商家表 (merchant)
-- ============================================
CREATE TABLE IF NOT EXISTS `merchant` (
    `id`                BIGINT      NOT NULL AUTO_INCREMENT,
    `user_id`           BIGINT      NOT NULL COMMENT '关联用户ID',
    `name`              VARCHAR(128) NOT NULL,
    `logo`              VARCHAR(512) DEFAULT '',
    `announcement`      VARCHAR(1024) DEFAULT '',
    `business_hours`    VARCHAR(128) DEFAULT '09:00-22:00',
    `status`            TINYINT     NOT NULL DEFAULT 0 COMMENT '0=待审核,1=营业中,2=已打烊,3=审核拒绝',
    `create_time`       DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_id` (`user_id`),
    CONSTRAINT `fk_merchant_user` FOREIGN KEY (`user_id`) REFERENCES `user`(`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 3. 菜品分类表 (category)
-- ============================================
CREATE TABLE IF NOT EXISTS `category` (
    `id`            BIGINT      NOT NULL AUTO_INCREMENT,
    `merchant_id`   BIGINT      NOT NULL,
    `name`          VARCHAR(64) NOT NULL,
    `sort_weight`   INT         NOT NULL DEFAULT 0 COMMENT '排序权重，越大越靠前',
    `create_time`   DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    CONSTRAINT `fk_category_merchant` FOREIGN KEY (`merchant_id`) REFERENCES `merchant`(`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 4. 菜品表 (dish)
-- 状态: 1=上架, 0=下架
-- ============================================
CREATE TABLE IF NOT EXISTS `dish` (
    `id`            BIGINT          NOT NULL AUTO_INCREMENT,
    `merchant_id`   BIGINT          NOT NULL,
    `category_id`   BIGINT          NOT NULL,
    `name`          VARCHAR(128)    NOT NULL,
    `image`         VARCHAR(512)    DEFAULT '',
    `description`   VARCHAR(1024)   DEFAULT '',
    `price`         DECIMAL(10,2)   NOT NULL,
    `stock`         INT             NOT NULL DEFAULT 0 COMMENT '库存数量',
    `status`        TINYINT         NOT NULL DEFAULT 1 COMMENT '1=上架,0=下架',
    `sort_weight`   INT             NOT NULL DEFAULT 0,
    `create_time`   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    CONSTRAINT `fk_dish_merchant` FOREIGN KEY (`merchant_id`) REFERENCES `merchant`(`id`),
    CONSTRAINT `fk_dish_category` FOREIGN KEY (`category_id`) REFERENCES `category`(`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 5. 购物车表 (cart)
-- ============================================
CREATE TABLE IF NOT EXISTS `cart` (
    `id`            BIGINT          NOT NULL AUTO_INCREMENT,
    `user_id`       BIGINT          NOT NULL,
    `total_price`   DECIMAL(10,2)   NOT NULL DEFAULT 0.00,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_id` (`user_id`),
    CONSTRAINT `fk_cart_user` FOREIGN KEY (`user_id`) REFERENCES `user`(`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 6. 购物车明细表 (cart_item)
-- ============================================
CREATE TABLE IF NOT EXISTS `cart_item` (
    `id`            BIGINT          NOT NULL AUTO_INCREMENT,
    `cart_id`       BIGINT          NOT NULL,
    `dish_id`       BIGINT          NOT NULL,
    `quantity`      INT             NOT NULL DEFAULT 1,
    `dish_price`    DECIMAL(10,2)   NOT NULL COMMENT '加入时的价格快照',
    PRIMARY KEY (`id`),
    CONSTRAINT `fk_cart_item_cart` FOREIGN KEY (`cart_id`) REFERENCES `cart`(`id`),
    CONSTRAINT `fk_cart_item_dish` FOREIGN KEY (`dish_id`) REFERENCES `dish`(`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 7. 地址簿表 (address)
-- ============================================
CREATE TABLE IF NOT EXISTS `address` (
    `id`            BIGINT          NOT NULL AUTO_INCREMENT,
    `user_id`       BIGINT          NOT NULL,
    `name`          VARCHAR(32)     NOT NULL COMMENT '收货人姓名',
    `phone`         VARCHAR(20)     NOT NULL COMMENT '收货人电话',
    `province`      VARCHAR(32)     NOT NULL DEFAULT '',
    `city`          VARCHAR(32)     NOT NULL DEFAULT '',
    `district`      VARCHAR(32)     NOT NULL DEFAULT '',
    `detail`        VARCHAR(256)    NOT NULL COMMENT '详细地址',
    `is_default`    TINYINT         NOT NULL DEFAULT 0 COMMENT '1=默认地址',
    PRIMARY KEY (`id`),
    CONSTRAINT `fk_address_user` FOREIGN KEY (`user_id`) REFERENCES `user`(`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 8. 订单表 (order)
-- 状态: 0=待支付, 1=待接单, 2=配送中, 3=已完成, 4=已取消
-- ============================================
CREATE TABLE IF NOT EXISTS `order` (
    `id`                BIGINT          NOT NULL AUTO_INCREMENT,
    `user_id`           BIGINT          NOT NULL,
    `address_snapshot`  JSON            NOT NULL COMMENT '下单时的地址快照',
    `remark`            VARCHAR(256)    DEFAULT '' COMMENT '备注',
    `total_amount`      DECIMAL(10,2)   NOT NULL,
    `status`            TINYINT         NOT NULL DEFAULT 0 COMMENT '0=待支付,1=待接单,2=配送中,3=已完成,4=已取消',
    `create_time`       DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `pay_time`          DATETIME        DEFAULT NULL,
    `accept_time`       DATETIME        DEFAULT NULL,
    `complete_time`     DATETIME        DEFAULT NULL,
    PRIMARY KEY (`id`),
    CONSTRAINT `fk_order_user` FOREIGN KEY (`user_id`) REFERENCES `user`(`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 9. 订单明细表 (order_item)
-- ============================================
CREATE TABLE IF NOT EXISTS `order_item` (
    `id`            BIGINT          NOT NULL AUTO_INCREMENT,
    `order_id`      BIGINT          NOT NULL,
    `dish_id`       BIGINT          NOT NULL,
    `dish_name`     VARCHAR(128)    NOT NULL COMMENT '菜品名称快照',
    `dish_price`    DECIMAL(10,2)   NOT NULL COMMENT '下单时的单价快照',
    `quantity`      INT             NOT NULL DEFAULT 1,
    PRIMARY KEY (`id`),
    CONSTRAINT `fk_order_item_order` FOREIGN KEY (`order_id`) REFERENCES `order`(`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================
-- 10. 系统配置表 (system_config)
-- ============================================
CREATE TABLE IF NOT EXISTS `system_config` (
    `id`            BIGINT          NOT NULL AUTO_INCREMENT,
    `config_key`    VARCHAR(64)     NOT NULL,
    `config_value`  VARCHAR(512)    NOT NULL DEFAULT '',
    `description`   VARCHAR(256)    DEFAULT '',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_config_key` (`config_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 插入默认系统配置
INSERT INTO `system_config` (`config_key`, `config_value`, `description`) VALUES
    ('delivery_fee', '5.00', '配送费（元）'),
    ('commission_rate', '0.20', '平台抽成比例')
ON DUPLICATE KEY UPDATE `config_value` = VALUES(`config_value`);
