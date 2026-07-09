import client from './client';
import type { ApiResponse, Category } from '../types';

export async function getMerchantCategories() {
  const { data } = await client.get<ApiResponse<Category[]>>('/api/merchant/categories');
  return data;
}

export async function addCategory(name: string, sortWeight?: number) {
  const { data } = await client.post<ApiResponse<{ categoryId: number }>>('/api/merchant/categories', { name, sortWeight });
  return data;
}

export async function updateCategory(id: number, body: { name?: string; sortWeight?: number }) {
  const { data } = await client.put<ApiResponse<null>>(`/api/merchant/categories/${id}`, body);
  return data;
}
