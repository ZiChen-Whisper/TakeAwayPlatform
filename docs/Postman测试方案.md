# TakeAwayPlatform 后端 API Postman 测试方案

> 测试环境：localhost:9090  
> 前置条件：数据库已初始化，服务器已启动

---

## 一、Postman 环境变量设置

### 1.1 创建环境 "TakeAwayPlatform-Dev"

| 变量名 | 初始值 | 说明 |
|--------|--------|------|
| `baseUrl` | `http://localhost:9090` | API 基础地址 |
| `userToken` | *(登录后自动填充)* | 普通用户 Token |
| `merchantToken` | *(登录后自动填充)* | 商家 Token |
| `adminToken` | *(登录后自动填充)* | 管理员 Token |

### 1.2 登录接口自动设置 Token (Tests 脚本)

在 `/api/user/login` 接口的 **Tests** 标签中添加：

```javascript
var jsonData = pm.response.json();
if (jsonData.code === 200) {
    var role = jsonData.data.user.role;
    if (role === 0) pm.environment.set("userToken", jsonData.data.token);
    if (role === 1) pm.environment.set("merchantToken", jsonData.data.token);
    if (role === 2) pm.environment.set("adminToken", jsonData.data.token);
}
```

---

## 二、测试用例集

### 2.1 服务健康检查

| 序号 | 方法 | URL | Headers | Body | 预期结果 | 验证内容 |
|------|------|-----|---------|------|----------|----------|
| TC-001 | GET | `{{baseUrl}}/` | - | - | 200, `TakeAwayPlatform is running!` | 服务存活 |
| TC-002 | GET | `{{baseUrl}}/health` | - | - | 200, `OK` | 健康检查 |

---

### 2.2 用户注册与登录

#### TC-010：注册普通用户
```
POST {{baseUrl}}/api/user/register
Content-Type: application/json

{
    "username": "test001",
    "password": "123456",
    "phone": "13800138000",
    "role": 0
}
```
**预期：** `{"code":200, "message":"注册成功", "data":{"id":1, "username":"test001"}}`

#### TC-011：重复注册（异常测试）
```
POST {{baseUrl}}/api/user/register
Content-Type: application/json

{
    "username": "test001",
    "password": "123456"
}
```
**预期：** `{"code":400, "message":"用户名已存在"}`

#### TC-012：注册商家用户
```
POST {{baseUrl}}/api/user/register
Content-Type: application/json

{
    "username": "merchant01",
    "password": "123456",
    "phone": "13900139000",
    "role": 1
}
```
**预期：** `{"code":200, "message":"注册成功"}`

#### TC-013：注册管理员
```
POST {{baseUrl}}/api/user/register
Content-Type: application/json

{
    "username": "admin01",
    "password": "123456",
    "phone": "13700137000",
    "role": 2
}
```
**预期：** `{"code":200, "message":"注册成功"}`

#### TC-020：用户登录
```
POST {{baseUrl}}/api/user/login
Content-Type: application/json

{
    "username": "test001",
    "password": "123456"
}
```
**预期：** `{"code":200, "message":"登录成功", "data":{"token":"eyJ...","user":{...}}}`

#### TC-021：密码错误登录（异常测试）
```
POST {{baseUrl}}/api/user/login
Content-Type: application/json

{
    "username": "test001",
    "password": "wrong"
}
```
**预期：** `{"code":401, "message":"用户名或密码错误"}`

#### TC-022：商家登录
```
POST {{baseUrl}}/api/user/login
Content-Type: application/json

{
    "username": "merchant01",
    "password": "123456"
}
```
**预期：** `{"code":200}`  
*注意：Tests 脚本会自动将 token 存入 `merchantToken`*

#### TC-023：管理员登录
```
POST {{baseUrl}}/api/user/login
Content-Type: application/json

{
    "username": "admin01",
    "password": "123456"
}
```
**预期：** `{"code":200}`  
*注意：Tests 脚本会自动将 token 存入 `adminToken`*

---

### 2.3 用户个人信息

#### TC-030：获取个人信息（需鉴权）
```
GET {{baseUrl}}/api/user/profile
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":200, "data":{"id":1, "username":"test001", ...}}`  
**注意：** 返回数据中不应包含 `password_hash` 字段

#### TC-031：修改个人信息
```
PUT {{baseUrl}}/api/user/profile
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "phone": "13900139000"
}
```
**预期：** `{"code":200, "message":"更新成功"}`

#### TC-032：无 Token 访问（异常测试）
```
GET {{baseUrl}}/api/user/profile
```
**预期：** `{"code":401, "message":"未提供认证令牌"}`

---

### 2.4 菜品与分类管理（商家端）

> **前置条件：** 需要先为商家用户创建商家记录
> ```sql
> INSERT INTO merchant (user_id, name, status) VALUES (2, '美味餐厅', 1);
> ```

#### TC-040：新增菜品分类
```
POST {{baseUrl}}/api/merchant/categories
Authorization: Bearer {{merchantToken}}
Content-Type: application/json

{
    "name": "热销",
    "sortWeight": 100
}
```
**预期：** `{"code":200, "data":{"categoryId":1}}`

#### TC-041：新增第二个分类
```
POST {{baseUrl}}/api/merchant/categories
Authorization: Bearer {{merchantToken}}
Content-Type: application/json

{
    "name": "主食",
    "sortWeight": 90
}
```
**预期：** `{"code":200}`

#### TC-042：重复分类名（异常测试）
```
POST {{baseUrl}}/api/merchant/categories
Authorization: Bearer {{merchantToken}}
Content-Type: application/json

{
    "name": "热销"
}
```
**预期：** `{"code":400, "message":"分类名已存在"}`

#### TC-050：新增菜品
```
POST {{baseUrl}}/api/merchant/dishes
Authorization: Bearer {{merchantToken}}
Content-Type: application/json

{
    "name": "宫保鸡丁",
    "price": 28.00,
    "categoryId": 1,
    "description": "经典川菜，麻辣鲜香",
    "image": "/images/gongbao.jpg",
    "stock": 100
}
```
**预期：** `{"code":200, "data":{"dishId":1}}`

#### TC-051：再新增一个菜品
```
POST {{baseUrl}}/api/merchant/dishes
Authorization: Bearer {{merchantToken}}
Content-Type: application/json

{
    "name": "鱼香肉丝",
    "price": 25.00,
    "categoryId": 1,
    "description": "酸甜可口",
    "stock": 50
}
```
**预期：** `{"code":200, "data":{"dishId":2}}`

#### TC-052：查看商家菜品列表
```
GET {{baseUrl}}/api/merchant/dishes?page=1&pageSize=10
Authorization: Bearer {{merchantToken}}
```
**预期：** `{"code":200, "data":{"list":[...], "page":1}}`

#### TC-053：修改菜品信息
```
PUT {{baseUrl}}/api/merchant/dishes/1
Authorization: Bearer {{merchantToken}}
Content-Type: application/json

{
    "price": 30.00
}
```
**预期：** `{"code":200, "message":"更新成功"}`

#### TC-054：菜品下架
```
PATCH {{baseUrl}}/api/merchant/dishes/1/status
Authorization: Bearer {{merchantToken}}
Content-Type: application/json

{
    "status": 0
}
```
**预期：** `{"code":200, "message":"状态更新成功"}`

#### TC-055：菜品上架
```
PATCH {{baseUrl}}/api/merchant/dishes/1/status
Authorization: Bearer {{merchantToken}}
Content-Type: application/json

{
    "status": 1
}
```
**预期：** `{"code":200, "message":"状态更新成功"}`

#### TC-056：普通用户尝试新增菜品（权限测试）
```
POST {{baseUrl}}/api/merchant/dishes
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "name": "测试菜品",
    "price": 10.00,
    "categoryId": 1
}
```
**预期：** `{"code":403, "message":"无权限，需要更高权限"}`

#### TC-057：删除菜品
```
DELETE {{baseUrl}}/api/merchant/dishes/2
Authorization: Bearer {{merchantToken}}
```
**预期：** `{"code":200, "message":"删除成功"}`

---

### 2.5 菜品搜索（用户端）

#### TC-060：分页搜索所有菜品
```
GET {{baseUrl}}/api/dishes?page=1&pageSize=10
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":200, "data":{"list":[...], "total":N, "page":1}}`

#### TC-061：按关键词搜索
```
GET {{baseUrl}}/api/dishes?keyword=宫保&page=1&pageSize=10
Authorization: Bearer {{userToken}}
```
**预期：** 只返回名称含"宫保"的菜品

#### TC-062：按分类筛选
```
GET {{baseUrl}}/api/dishes?categoryId=1&page=1&pageSize=10
Authorization: Bearer {{userToken}}
```
**预期：** 只返回分类ID=1的菜品

#### TC-063：菜品详情
```
GET {{baseUrl}}/api/dishes/1
Authorization: Bearer {{userToken}}
```
**预期：** 返回菜品详细信息（含分类名、商家名）

#### TC-064：获取分类列表
```
GET {{baseUrl}}/api/categories?merchantId=1
Authorization: Bearer {{userToken}}
```
**预期：** 返回有上架菜品的分类列表

---

### 2.6 购物车

#### TC-070：加入购物车
```
POST {{baseUrl}}/api/cart/add
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "dishId": 1,
    "quantity": 2
}
```
**预期：** `{"code":200, "data":{"totalPrice":56.00, "items":[...]}}`

#### TC-071：重复加购同一菜品（数量累加）
```
POST {{baseUrl}}/api/cart/add
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "dishId": 1,
    "quantity": 1
}
```
**预期：** 数量变为3，总价变为84.00

#### TC-072：获取购物车
```
GET {{baseUrl}}/api/cart
Authorization: Bearer {{userToken}}
```
**预期：** 返回购物车详情，含菜品信息和总价

#### TC-073：修改购物车数量
```
PUT {{baseUrl}}/api/cart/item/1
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "quantity": 3
}
```
**预期：** 总价相应变化

#### TC-074：移出购物车
```
DELETE {{baseUrl}}/api/cart/item/1
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":200, "message":"已移除"}`

#### TC-075：加购已下架菜品（异常测试）
```
POST {{baseUrl}}/api/cart/add
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "dishId": 999,
    "quantity": 1
}
```
**预期：** `{"code":404, "message":"菜品不存在"}`

---

### 2.7 地址簿

#### TC-080：新增地址
```
POST {{baseUrl}}/api/addresses
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "name": "张三",
    "phone": "13800138000",
    "province": "湖北省",
    "city": "武汉市",
    "district": "洪山区",
    "detail": "珞喻路152号",
    "isDefault": true
}
```
**预期：** `{"code":200, "data":{"addressId":1}}`

#### TC-081：新增第二个地址
```
POST {{baseUrl}}/api/addresses
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "name": "李四",
    "phone": "13900139000",
    "detail": "珞喻路100号"
}
```
**预期：** `{"code":200, "data":{"addressId":2}}`

#### TC-082：获取地址列表
```
GET {{baseUrl}}/api/addresses
Authorization: Bearer {{userToken}}
```
**预期：** 返回地址列表，默认地址排在最前

#### TC-083：修改地址
```
PUT {{baseUrl}}/api/addresses/1
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "phone": "13600136000"
}
```
**预期：** `{"code":200, "message":"更新成功"}`

#### TC-084：设置默认地址
```
PUT {{baseUrl}}/api/addresses/2/default
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":200, "message":"设置成功"}`  
*验证：再次获取地址列表，地址2应变为默认*

#### TC-085：删除地址
```
DELETE {{baseUrl}}/api/addresses/2
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":200, "message":"删除成功"}`

---

### 2.8 订单与支付（核心流程）

> **前置条件：** 购物车中需有菜品，地址簿中需有地址

#### TC-090：提交订单
```
POST {{baseUrl}}/api/orders
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "addressId": 1,
    "remark": "少放辣，多加葱"
}
```
**预期：** `{"code":200, "data":{"orderId":1, "totalPrice":..., "status":0, "statusName":"待支付"}}`  
**验证：** 购物车应被清空，菜品库存应减少

#### TC-091：空购物车下单（异常测试）
```
POST {{baseUrl}}/api/orders
Authorization: Bearer {{userToken}}
Content-Type: application/json

{
    "addressId": 1
}
```
**预期：** `{"code":400, "message":"购物车为空"}`

#### TC-092：模拟支付成功
```
POST {{baseUrl}}/api/orders/1/pay
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":200, "data":{"status":1, "statusName":"待接单"}}`

#### TC-093：重复支付（异常测试）
```
POST {{baseUrl}}/api/orders/1/pay
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":400, "message":"当前订单状态不允许支付"}`

#### TC-094：查询订单列表
```
GET {{baseUrl}}/api/orders?status=1&page=1&pageSize=10
Authorization: Bearer {{userToken}}
```
**预期：** 返回待接单的订单列表

#### TC-095：查询订单详情
```
GET {{baseUrl}}/api/orders/1
Authorization: Bearer {{userToken}}
```
**预期：** 返回订单基本信息 + 明细列表 + 地址快照

#### TC-096：查询所有状态订单
```
GET {{baseUrl}}/api/orders?page=1&pageSize=10
Authorization: Bearer {{userToken}}
```
**预期：** 返回所有订单（不限状态）

---

### 2.9 商家端订单处理

#### TC-100：查看待接单订单
```
GET {{baseUrl}}/api/merchant/orders?status=1&page=1&pageSize=10
Authorization: Bearer {{merchantToken}}
```
**预期：** 返回该商家相关的待接单订单列表

#### TC-101：普通用户访问商家接口（权限测试）
```
GET {{baseUrl}}/api/merchant/orders
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":403}`

#### TC-102：商家确认接单
```
PUT {{baseUrl}}/api/merchant/orders/1/accept
Authorization: Bearer {{merchantToken}}
```
**预期：** `{"code":200, "data":{"status":2, "statusName":"配送中"}}`

#### TC-103：商家标记完成
```
PUT {{baseUrl}}/api/merchant/orders/1/complete
Authorization: Bearer {{merchantToken}}
```
**预期：** `{"code":200, "data":{"status":3, "statusName":"已完成"}}`

#### TC-104：对已完成订单再次操作（异常测试）
```
PUT {{baseUrl}}/api/merchant/orders/1/accept
Authorization: Bearer {{merchantToken}}
```
**预期：** `{"code":400, "message":"当前状态不允许接单"}`

---

### 2.10 管理端功能

#### TC-110：普通用户访问管理接口（权限测试）
```
GET {{baseUrl}}/api/admin/users
Authorization: Bearer {{userToken}}
```
**预期：** `{"code":403}`

#### TC-111：管理员查看用户列表
```
GET {{baseUrl}}/api/admin/users?page=1&pageSize=20
Authorization: Bearer {{adminToken}}
```
**预期：** `{"code":200, "data":{"list":[...], "page":1}}`

#### TC-112：冻结用户
```
PUT {{baseUrl}}/api/admin/users/1/status
Authorization: Bearer {{adminToken}}
Content-Type: application/json

{
    "status": 0
}
```
**预期：** `{"code":200, "message":"操作成功"}`  
**验证：** 用户1再次登录应返回 `{"code":403, "message":"账号已被冻结"}`

#### TC-113：解冻用户
```
PUT {{baseUrl}}/api/admin/users/1/status
Authorization: Bearer {{adminToken}}
Content-Type: application/json

{
    "status": 1
}
```
**预期：** `{"code":200}`

#### TC-114：查看商家审核列表
```
GET {{baseUrl}}/api/admin/merchants
Authorization: Bearer {{adminToken}}
```
**预期：** 返回所有商家及其关联用户名

#### TC-115：审核商家（通过）
```
PUT {{baseUrl}}/api/admin/merchants/1/audit
Authorization: Bearer {{adminToken}}
Content-Type: application/json

{
    "status": 1
}
```
**预期：** `{"code":200, "message":"审核完成"}`

#### TC-116：数据看板
```
GET {{baseUrl}}/api/admin/dashboard
Authorization: Bearer {{adminToken}}
```
**预期：** `{"code":200, "data":{"todayOrders":N, "todayRevenue":..., "todayNewUsers":N}}`

#### TC-117：查看系统配置
```
GET {{baseUrl}}/api/admin/config
Authorization: Bearer {{adminToken}}
```
**预期：** 返回 delivery_fee 和 commission_rate

#### TC-118：修改系统配置
```
PUT {{baseUrl}}/api/admin/config
Authorization: Bearer {{adminToken}}
Content-Type: application/json

{
    "config_key": "delivery_fee",
    "config_value": "6.00"
}
```
**预期：** `{"code":200, "message":"配置更新成功"}`

---

## 三、并发测试（进阶）

### 3.1 库存防超卖测试

使用 Apache Bench 或 Postman Collection Runner：

```bash
# 准备下单数据
echo '{"addressId":1,"remark":"并发测试"}' > /tmp/order.json

# 50并发 200请求
ab -n 200 -c 50 -p /tmp/order.json -T application/json \
   -H "Authorization: Bearer YOUR_USER_TOKEN" \
   http://localhost:9090/api/orders
```

**验证：** 查询数据库 `SELECT stock FROM dish WHERE id=1`，库存不应为负数。

---

## 四、Postman Collection 导入指南

### 4.1 推荐组织结构

```
TakeAwayPlatform API Tests/
├── 01-Health Check/
│   ├── TC-001 Root
│   └── TC-002 Health
├── 02-User Auth/
│   ├── TC-010 Register User
│   ├── TC-011 Register Duplicate
│   ├── TC-012 Register Merchant
│   ├── TC-013 Register Admin
│   ├── TC-020 Login User
│   ├── TC-021 Login Wrong Password
│   ├── TC-022 Login Merchant
│   ├── TC-023 Login Admin
│   ├── TC-030 Get Profile
│   ├── TC-031 Update Profile
│   └── TC-032 No Token
├── 03-Dishes & Categories/
│   ├── TC-040~042 Categories
│   ├── TC-050~057 Dishes
│   └── TC-060~064 Search
├── 04-Cart/
│   └── TC-070~075 Cart
├── 05-Address/
│   └── TC-080~085 Address
├── 06-Orders/
│   └── TC-090~096 Orders
├── 07-Merchant Orders/
│   └── TC-100~104 Merchant
└── 08-Admin/
    └── TC-110~118 Admin
```

### 4.2 运行顺序

1. 先执行 `02-User Auth` 组（注册+登录，获取所有 Token）
2. 再执行 `03-Dishes & Categories`（商家创建分类和菜品）
3. 然后执行 `04-Cart`（加入购物车）
4. 再执行 `05-Address`（创建地址）
5. 然后执行 `06-Orders`（下单+支付）
6. 再执行 `07-Merchant Orders`（商家接单完成）
7. 最后执行 `08-Admin`（管理功能）

---

> 测试方案版本：v1.0  
> 适用版本：TakeAwayPlatform v1.0
