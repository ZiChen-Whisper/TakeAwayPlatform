import { createContext, useContext, useState, useEffect, useCallback, type ReactNode } from 'react';
import type { User } from '../types';
import * as authApi from '../api/auth';

interface AuthState {
  user: User | null;
  token: string | null;
  loading: boolean;
  login: (username: string, password: string) => Promise<void>;
  register: (username: string, password: string, phone: string, role: number) => Promise<void>;
  logout: () => void;
  isAuthenticated: boolean;
  isMerchant: boolean;
  isAdmin: boolean;
}

const AuthContext = createContext<AuthState | null>(null);

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<User | null>(null);
  const [token, setToken] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  // 启动时从 localStorage 恢复
  useEffect(() => {
    const savedToken = localStorage.getItem('token');
    const savedUser = localStorage.getItem('user');
    if (savedToken && savedUser) {
      try {
        setToken(savedToken);
        setUser(JSON.parse(savedUser));
      } catch {
        localStorage.removeItem('token');
        localStorage.removeItem('user');
      }
    }
    setLoading(false);
  }, []);

  const login = useCallback(async (username: string, password: string) => {
    const res = await authApi.login(username, password);
    if (res.code === 200) {
      const { token: newToken, user: loginUser } = res.data;
      localStorage.setItem('token', newToken);
      localStorage.setItem('user', JSON.stringify(loginUser));
      setToken(newToken);
      setUser(loginUser);
    } else {
      throw new Error(res.message);
    }
  }, []);

  const register = useCallback(async (username: string, password: string, phone: string, role: number) => {
    const res = await authApi.register(username, password, phone, role);
    if (res.code === 200) {
      // 注册成功后自动登录
      await login(username, password);
    } else {
      throw new Error(res.message);
    }
  }, [login]);

  const logout = useCallback(() => {
    localStorage.removeItem('token');
    localStorage.removeItem('user');
    setToken(null);
    setUser(null);
  }, []);

  const isAuthenticated = !!token && !!user;
  const isMerchant = user?.role === 1;
  const isAdmin = user?.role === 2;

  return (
    <AuthContext.Provider value={{ user, token, loading, login, register, logout, isAuthenticated, isMerchant, isAdmin }}>
      {children}
    </AuthContext.Provider>
  );
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used within AuthProvider');
  return ctx;
}
