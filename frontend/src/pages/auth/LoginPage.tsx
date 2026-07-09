import { useState, type FormEvent } from 'react';
import { Link, useNavigate, useLocation } from 'react-router-dom';
import { useAuth } from '../../contexts/AuthContext';
import './Auth.css';

export default function LoginPage() {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);
  const { login } = useAuth();
  const navigate = useNavigate();
  const location = useLocation();
  const from = (location.state as { from?: { pathname: string } })?.from?.pathname || '/';

  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault();
    setError('');
    if (!username || !password) { setError('请填写用户名和密码'); return; }
    setLoading(true);
    try {
      await login(username, password);
      navigate(from, { replace: true });
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : '登录失败');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="auth-page">
      <div className="auth-brand">
        <h1 className="auth-brand__title">TakeAway</h1>
        <p className="auth-brand__sub">美味外卖，即刻送达</p>
      </div>
      <div className="auth-form-wrap">
        <h2 className="auth-form__title">欢迎回来</h2>
        <p className="auth-form__sub">登录您的账户</p>
        {error && <div className="auth-error">{error}</div>}
        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label>用户名</label>
            <input className="form-input" value={username} onChange={e => setUsername(e.target.value)} placeholder="请输入用户名" />
          </div>
          <div className="form-group">
            <label>密码</label>
            <input className="form-input" type="password" value={password} onChange={e => setPassword(e.target.value)} placeholder="请输入密码" />
          </div>
          <button className="btn btn-primary w-full" disabled={loading} style={{ width: '100%', marginTop: 8 }}>
            {loading ? '登录中...' : '登录'}
          </button>
        </form>
        <p className="auth-switch">
          还没有账户？<Link to="/register">立即注册</Link>
        </p>
      </div>
    </div>
  );
}
