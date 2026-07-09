// ===== 通用 API 响应 =====
export interface ApiResponse<T = unknown> {
  code: number;
  message: string;
  data: T;
}

export interface PaginatedData<T> {
  list: T[];
  total: number;
  page: number;
  pageSize: number;
}

// ===== 用户 =====
export interface User {
  id: number;
  username: string;
  phone: string;
  avatar?: string;
  role: number; // 0=用户 1=商家 2=管理员
  status: number;
  create_time?: string;
}

export interface LoginResult {
  token: string;
  user: User;
}

// ===== 分类 =====
export interface Category {
  id: number;
  name: string;
  sortWeight?: number;
  merchant_id?: number;
}

// ===== 菜品 =====
export interface Dish {
  id: number;
  merchant_id: number;
  category_id: number;
  name: string;
  image?: string;
  description?: string;
  price: number;
  stock: number;
  status: number; // 0=下架 1=上架
  sort_weight?: number;
  create_time?: string;
  category_name?: string;
  merchant_name?: string;
}

export interface DishFormData {
  name: string;
  categoryId: number;
  price: number;
  description?: string;
  image?: string;
  stock: number;
}

// ===== 购物车 =====
export interface CartItem {
  id: number;
  dish_id: number;
  dish_name?: string;
  name?: string;
  quantity: number;
  dish_price: number;
  image?: string;
}

export interface Cart {
  cartId: number;
  items: CartItem[];
  totalPrice: number;
}

// ===== 地址 =====
export interface Address {
  id: number;
  user_id?: number;
  name: string;
  phone: string;
  province: string;
  city: string;
  district: string;
  detail: string;
  is_default: number;
  isDefault?: boolean;
}

export interface AddressFormData {
  name: string;
  phone: string;
  province: string;
  city: string;
  district: string;
  detail: string;
  isDefault: boolean;
}

// ===== 订单 =====
export interface OrderItem {
  id: number;
  order_id: number;
  dish_id: number;
  dish_name: string;
  dish_price: number;
  quantity: number;
}

export interface Order {
  id: number;
  user_id: number;
  address_snapshot?: string;
  remark?: string;
  total_amount: number;
  totalPrice?: number;
  status: number; // 0待支付 1待接单 2配送中 3已完成 4已取消
  statusName?: string;
  create_time: string;
  pay_time?: string;
  accept_time?: string;
  complete_time?: string;
  items?: OrderItem[];
}

export interface CreateOrderData {
  addressId: number;
  remark?: string;
}

// ===== 管理端 =====
export interface MerchantRecord {
  id: number;
  user_id: number;
  name: string;
  announcement?: string;
  business_hours?: string;
  status: number;
  username?: string;
}

export interface DashboardData {
  todayOrders: number;
  todayRevenue: number;
  todayNewUsers: number;
}

export interface SystemConfig {
  config_key: string;
  config_value: string;
}
