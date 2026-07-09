import { useState, type FormEvent } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useAuth } from '../../contexts/AuthContext';
import './Auth.css';

export default function RegisterPage() {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [phone, setPhone] = useState('');
  const [role, setRole] = useState(0);
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);
  const { register } = useAuth();
  const navigate = useNavigate();

  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault();
    setError('');
    if (!username || !password) { setError('请填写用户名和密码'); return; }
    setLoading(true);
    try {
      await register(username, password, phone, role);
      navigate('/', { replace: true });
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : '注册失败');
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
        <h2 className="auth-form__title">创建账户</h2>
        <p className="auth-form__sub">注册新账户开始点餐</p>
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
          <div className="form-group">
            <label>手机号</label>
            <input className="form-input" value={phone} onChange={e => setPhone(e.target.value)} placeholder="请输入手机号" />
          </div>
          <div className="form-group">
            <label>角色</label>
            <div className="auth-roles">
              {[
                { value: 0, label: '普通用户' },
                { value: 1, label: '商家' },
                { value: 2, label: '管理员' },
              ].map(r => (
                <label key={r.value} className={`auth-role ${role === r.value ? 'auth-role--active' : ''}`}>
                  <input type="radio" name="role" value={r.value} checked={role === r.value} onChange={() => setRole(r.value)} />
                  {r.label}
                </label>
              ))}
            </div>
          </div>
          <button className="btn btn-primary w-full" disabled={loading} style={{ width: '100%', marginTop: 8 }}>
            {loading ? '注册中...' : '注册'}
          </button>
        </form>
        <p className="auth-switch">
          已有账户？<Link to="/login">立即登录</Link>
        </p>
      </div>
    </div>
  );
}
