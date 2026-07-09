import { useState, useEffect } from 'react';
import * as adminApi from '../../api/admin';
import type { DashboardData } from '../../types';
import './Admin.css';

export default function DashboardPage() {
  const [data, setData] = useState<DashboardData | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    adminApi.getDashboard().then(res => {
      if (res.code === 200) setData(res.data);
    }).catch(() => {}).finally(() => setLoading(false));
  }, []);

  if (loading) return <div className="spinner" />;

  return (
    <div className="page-container">
      <h1 className="page-title">数据看板</h1>
      <div className="grid-3">
        <div className="card admin-dash-card">
          <span className="admin-dash-card__val">{data?.todayOrders ?? 0}</span>
          <span className="admin-dash-card__label">今日订单数</span>
        </div>
        <div className="card admin-dash-card">
          <span className="admin-dash-card__val">&yen;{Number(data?.todayRevenue ?? 0).toFixed(2)}</span>
          <span className="admin-dash-card__label">今日交易额</span>
        </div>
        <div className="card admin-dash-card">
          <span className="admin-dash-card__val">{data?.todayNewUsers ?? 0}</span>
          <span className="admin-dash-card__label">今日新增用户</span>
        </div>
      </div>
    </div>
  );
}
