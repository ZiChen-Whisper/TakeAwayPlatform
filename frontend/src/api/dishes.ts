import client from './client';
import type { ApiResponse, PaginatedData, Dish, Category } from '../types';

export async function searchDishes(params: {
  keyword?: string;
  categoryId?: number;
  page?: number;
  pageSize?: number;
}) {
  const { data } = await client.get<ApiResponse<PaginatedData<Dish>>>('/api/dishes', { params });
  return data;
}

export async function getDishDetail(id: number) {
  const { data } = await client.get<ApiResponse<Dish>>(`/api/dishes/${id}`);
  return data;
}

export async function getCategories(merchantId?: number) {
  const { data } = await client.get<ApiResponse<Category[]>>('/api/categories', { params: { merchantId } });
  return data;
}

// 商家端
export async function getMerchantDishes(params: { categoryId?: number; page?: number; pageSize?: number }) {
  const { data } = await client.get<ApiResponse<PaginatedData<Dish>>>('/api/merchant/dishes', { params });
  return data;
}

export async function addDish(body: {
  name: string; price: number; categoryId: number;
  description?: string; image?: string; stock?: number;
}) {
  const { data } = await client.post<ApiResponse<{ dishId: number }>>('/api/merchant/dishes', body);
  return data;
}

export async function updateDish(id: number, body: Record<string, unknown>) {
  const { data } = await client.put<ApiResponse<null>>(`/api/merchant/dishes/${id}`, body);
  return data;
}

export async function deleteDish(id: number) {
  const { data } = await client.delete<ApiResponse<null>>(`/api/merchant/dishes/${id}`);
  return data;
}

export async function setDishStatus(id: number, status: number) {
  const { data } = await client.patch<ApiResponse<null>>(`/api/merchant/dishes/${id}/status`, { status });
  return data;
}
