import { useState, useEffect, useCallback } from 'react';
import * as adminApi from '../../api/admin';
import type { User } from '../../types';
import './Admin.css';

export default function UserManagePage() {
  const [users, setUsers] = useState<User[]>([]);
  const [page, setPage] = useState(1);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(false);
  const pageSize = 20;

  const loadUsers = useCallback(async () => {
    setLoading(true);
    try {
      const res = await adminApi.getUsers(page, pageSize);
      if (res.code === 200) {
        setUsers(res.data.list);
        setTotal(res.data.total);
      }
    } catch { /* ignore */ }
    finally { setLoading(false); }
  }, [page]);

  useEffect(() => { loadUsers(); }, [loadUsers]);

  const handleToggleStatus = async (userId: number, currentStatus: number) => {
    const newStatus = currentStatus === 1 ? 0 : 1;
    try {
      await adminApi.updateUserStatus(userId, newStatus);
      loadUsers();
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '操作失败'); }
  };

  const totalPages = Math.ceil(total / pageSize);
  const ROLE_MAP: Record<number, string> = { 0: '用户', 1: '商家', 2: '管理员' };

  return (
    <div className="page-container">
      <h1 className="page-title">用户管理</h1>
      {loading ? <div className="spinner" /> : (
        <>
          <div className="table-wrap">
            <table className="table">
              <thead><tr><th>ID</th><th>用户名</th><th>手机号</th><th>角色</th><th>状态</th><th>操作</th></tr></thead>
              <tbody>
                {users.map(u => (
                  <tr key={u.id}>
                    <td>{u.id}</td>
                    <td className="font-bold">{u.username}</td>
                    <td className="text-secondary">{u.phone || '-'}</td>
                    <td>{ROLE_MAP[u.role] || u.role}</td>
                    <td><span className={`badge ${u.status === 1 ? 'badge-success' : 'badge-danger'}`}>{u.status === 1 ? '正常' : '冻结'}</span></td>
                    <td>
                      <button className={`btn btn-sm ${u.status === 1 ? 'btn-danger' : 'btn-success'}`} onClick={() => handleToggleStatus(u.id, u.status)}>
                        {u.status === 1 ? '冻结' : '解冻'}
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          {totalPages > 1 && (
            <div className="pagination">
              <button disabled={page <= 1} onClick={() => setPage(p => p - 1)}>上一页</button>
              <span>{page} / {totalPages}</span>
              <button disabled={page >= totalPages} onClick={() => setPage(p => p + 1)}>下一页</button>
            </div>
          )}
        </>
      )}
    </div>
  );
}
