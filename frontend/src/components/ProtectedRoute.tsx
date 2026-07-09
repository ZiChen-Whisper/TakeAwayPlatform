import { Navigate, Outlet, useLocation } from 'react-router-dom';
import { useAuth } from '../contexts/AuthContext';

interface Props {
  role?: number; // 0=用户 1=商家 2=管理员
}

export default function ProtectedRoute({ role }: Props) {
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
          <p>您没有权限访问此页面</p>
        </div>
      </div>
    );
  }

  return <Outlet />;
}
