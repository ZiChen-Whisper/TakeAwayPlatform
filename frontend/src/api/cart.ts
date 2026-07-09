import client from './client';
import type { ApiResponse, Cart } from '../types';

export async function getCart() {
  const { data } = await client.get<ApiResponse<Cart>>('/api/cart');
  return data;
}

export async function addToCart(dishId: number, quantity: number) {
  const { data } = await client.post<ApiResponse<Cart>>('/api/cart/add', { dishId, quantity });
  return data;
}

export async function updateQuantity(itemId: number, quantity: number) {
  const { data } = await client.put<ApiResponse<{ totalPrice: number }>>(`/api/cart/item/${itemId}`, { quantity });
  return data;
}

export async function removeItem(itemId: number) {
  const { data } = await client.delete<ApiResponse<null>>(`/api/cart/item/${itemId}`);
  return data;
}
