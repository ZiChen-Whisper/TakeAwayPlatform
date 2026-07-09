import client from './client';
import type { ApiResponse, User, LoginResult } from '../types';

export async function register(username: string, password: string, phone: string, role: number) {
  const { data } = await client.post<ApiResponse<User>>('/api/user/register', { username, password, phone, role });
  return data;
}

export async function login(username: string, password: string) {
  const { data } = await client.post<ApiResponse<LoginResult>>('/api/user/login', { username, password });
  return data;
}

export async function getProfile() {
  const { data } = await client.get<ApiResponse<User>>('/api/user/profile');
  return data;
}

export async function updateProfile(updates: { phone?: string; avatar?: string }) {
  const { data } = await client.put<ApiResponse<null>>('/api/user/profile', updates);
  return data;
}
