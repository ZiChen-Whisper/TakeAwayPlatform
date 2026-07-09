import { Navigate, Outlet, useLocation } from 'react-router-dom';
import { useAuth } from '../contexts/AuthContext';
import type { ReactNode } from 'react';

interface Props {
  role?: number;
  children?: ReactNode;
}

export default function ProtectedRoute({ role, children }: Props) {
  const { isAuthenticated, user, loading } = useAuth();
  const location = useLocation();

  if (loading) {
    return <div className="spinner" />;
  }

  if (!isAuthenticated) {
    return <Navigate to="/login" state={{ from: location }} replace />;
  }

  if (role !== undefined && user?.role !== role) {
    return (
      <div className="page-container">
        <div className="empty-state">
          <h2>403 无权限</h2>
          <p>您没有权限访问此页面（需要{role === 1 ? '商家' : role === 2 ? '管理员' : '更高'}权限）</p>
        </div>
      </div>
    );
  }

  // 如果有 children 则渲染 children，否则渲染 Outlet（用于布局路由）
  return children ? <>{children}</> : <Outlet />;
}
