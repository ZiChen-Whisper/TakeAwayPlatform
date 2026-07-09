import client from './client';
import type { ApiResponse, PaginatedData, User, MerchantRecord, DashboardData } from '../types';

export async function getUsers(page = 1, pageSize = 20) {
  const { data } = await client.get<ApiResponse<PaginatedData<User>>>('/api/admin/users', { params: { page, pageSize } });
  return data;
}

export async function updateUserStatus(userId: number, status: number) {
  const { data } = await client.put<ApiResponse<null>>(`/api/admin/users/status/${userId}`, { status });
  return data;
}

export async function getMerchants() {
  const { data } = await client.get<ApiResponse<MerchantRecord[]>>('/api/admin/merchants');
  return data;
}

export async function auditMerchant(merchantId: number, status: number) {
  const { data } = await client.put<ApiResponse<null>>(`/api/admin/merchants/audit/${merchantId}`, { status });
  return data;
}

export async function getDashboard() {
  const { data } = await client.get<ApiResponse<DashboardData>>('/api/admin/dashboard');
  return data;
}

export async function getConfig() {
  const { data } = await client.get<ApiResponse<Record<string, string>>>('/api/admin/config');
  return data;
}

export async function updateConfig(configKey: string, configValue: string) {
  const { data } = await client.put<ApiResponse<null>>('/api/admin/config', { config_key: configKey, config_value: configValue });
  return data;
}
