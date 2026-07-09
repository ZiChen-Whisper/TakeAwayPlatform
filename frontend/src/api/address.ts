import client from './client';
import type { ApiResponse, Address } from '../types';

export async function getAddresses() {
  const { data } = await client.get<ApiResponse<Address[]>>('/api/addresses');
  return data;
}

export async function addAddress(body: {
  name: string; phone: string; province: string; city: string;
  district: string; detail: string; isDefault: boolean;
}) {
  const { data } = await client.post<ApiResponse<{ addressId: number }>>('/api/addresses', body);
  return data;
}

export async function updateAddress(id: number, body: Record<string, unknown>) {
  const { data } = await client.put<ApiResponse<null>>(`/api/addresses/${id}`, body);
  return data;
}

export async function deleteAddress(id: number) {
  const { data } = await client.delete<ApiResponse<null>>(`/api/addresses/${id}`);
  return data;
}

export async function setDefaultAddress(id: number) {
  const { data } = await client.put<ApiResponse<null>>(`/api/addresses/${id}/default`);
  return data;
}
