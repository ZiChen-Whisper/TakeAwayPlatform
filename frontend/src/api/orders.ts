import client from './client';
import type { ApiResponse, PaginatedData, Order, CreateOrderData } from '../types';

export async function createOrder(body: CreateOrderData) {
  const { data } = await client.post<ApiResponse<Order>>('/api/orders', body);
  return data;
}

export async function payOrder(orderId: number) {
  const { data } = await client.post<ApiResponse<{ status: number; statusName: string }>>(`/api/orders/${orderId}/pay`);
  return data;
}

export async function getUserOrders(params: { status?: number; page?: number; pageSize?: number }) {
  const { data } = await client.get<ApiResponse<PaginatedData<Order>>>('/api/orders', { params });
  return data;
}

export async function getOrderDetail(orderId: number) {
  const { data } = await client.get<ApiResponse<{ order: Order; items: Order['items'] }>>(`/api/orders/${orderId}`);
  return data;
}

// 商家端
export async function getMerchantOrders(params: { status?: number; page?: number; pageSize?: number }) {
  const { data } = await client.get<ApiResponse<PaginatedData<Order>>>('/api/merchant/orders', { params });
  return data;
}

export async function acceptOrder(orderId: number) {
  const { data } = await client.put<ApiResponse<{ status: number; statusName: string }>>(`/api/merchant/orders/${orderId}/accept`);
  return data;
}

export async function completeOrder(orderId: number) {
  const { data } = await client.put<ApiResponse<{ status: number; statusName: string }>>(`/api/merchant/orders/${orderId}/complete`);
  return data;
}
