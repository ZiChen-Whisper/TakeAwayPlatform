import { useState, useEffect, useCallback } from 'react';
import * as adminApi from '../../api/admin';
import type { MerchantRecord } from '../../types';
import './Admin.css';

export default function MerchantManagePage() {
  const [merchants, setMerchants] = useState<MerchantRecord[]>([]);
  const [loading, setLoading] = useState(false);

  const loadMerchants = useCallback(async () => {
    setLoading(true);
    try {
      const res = await adminApi.getMerchants();
      if (res.code === 200) setMerchants(res.data);
    } catch { /* ignore */ }
    finally { setLoading(false); }
  }, []);

  useEffect(() => { loadMerchants(); }, [loadMerchants]);

  const handleAudit = async (merchantId: number, status: number) => {
    try {
      await adminApi.auditMerchant(merchantId, status);
      loadMerchants();
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '操作失败'); }
  };

  return (
    <div className="page-container">
      <h1 className="page-title">商家管理</h1>
      {loading ? <div className="spinner" /> : merchants.length === 0 ? (
        <div className="empty-state"><p>暂无商家</p></div>
      ) : (
        <div className="table-wrap">
          <table className="table">
            <thead><tr><th>ID</th><th>商家名</th><th>用户名</th><th>状态</th><th>操作</th></tr></thead>
            <tbody>
              {merchants.map(m => (
                <tr key={m.id}>
                  <td>{m.id}</td>
                  <td className="font-bold">{m.name}</td>
                  <td className="text-secondary">{m.username || '-'}</td>
                  <td><span className={`badge ${m.status === 1 ? 'badge-success' : m.status === 0 ? 'badge-warning' : 'badge-danger'}`}>{m.status === 1 ? '已通过' : m.status === 0 ? '待审核' : '已拒绝'}</span></td>
                  <td>
                    <div className="flex gap-2">
                      {m.status !== 1 && <button className="btn btn-sm btn-success" onClick={() => handleAudit(m.id, 1)}>通过</button>}
                      {m.status !== 2 && <button className="btn btn-sm btn-danger" onClick={() => handleAudit(m.id, 2)}>拒绝</button>}
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
