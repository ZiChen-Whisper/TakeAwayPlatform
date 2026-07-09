import { Link, useNavigate } from 'react-router-dom';
import { useAuth } from '../contexts/AuthContext';
import './Navbar.css';

export default function Navbar() {
  const { user, logout, isMerchant, isAdmin } = useAuth();
  const navigate = useNavigate();

  const handleLogout = () => {
    logout();
    navigate('/login');
  };

  return (
    <nav className="navbar">
      <div className="navbar__inner">
        <Link to="/" className="navbar__logo">TakeAway</Link>
        <div className="navbar__links">
          {isAdmin ? (
            <>
              <Link to="/admin/dashboard">数据看板</Link>
              <Link to="/admin/users">用户管理</Link>
              <Link to="/admin/merchants">商家管理</Link>
              <Link to="/admin/config">系统配置</Link>
            </>
          ) : isMerchant ? (
            <>
              <Link to="/merchant/orders">订单管理</Link>
              <Link to="/merchant/dishes">菜品管理</Link>
              <Link to="/merchant/categories">分类管理</Link>
            </>
          ) : (
            <>
              <Link to="/">首页</Link>
              <Link to="/cart">购物车</Link>
              <Link to="/orders">我的订单</Link>
              <Link to="/addresses">地址管理</Link>
            </>
          )}
        </div>
        <div className="navbar__user">
          {user ? (
            <>
              <span className="navbar__username">{user.username}</span>
              <button onClick={handleLogout} className="btn btn-sm">退出</button>
            </>
          ) : (
            <Link to="/login" className="btn btn-primary btn-sm">登录</Link>
          )}
        </div>
      </div>
    </nav>
  );
}
