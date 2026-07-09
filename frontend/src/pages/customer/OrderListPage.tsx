import { useState, useEffect, useCallback } from 'react';
import { Link } from 'react-router-dom';
import StatusBadge from '../../components/StatusBadge';
import * as orderApi from '../../api/orders';
import type { Order } from '../../types';
import './Customer.css';

const STATUS_TABS = [
  { label: '全部', value: -1 },
  { label: '待支付', value: 0 },
  { label: '待接单', value: 1 },
  { label: '配送中', value: 2 },
  { label: '已完成', value: 3 },
];

export default function OrderListPage() {
  const [orders, setOrders] = useState<Order[]>([]);
  const [status, setStatus] = useState(-1);
  const [page, setPage] = useState(1);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(false);
  const pageSize = 10;

  const loadOrders = useCallback(async () => {
    setLoading(true);
    try {
      const res = await orderApi.getUserOrders({ status: status >= 0 ? status : undefined, page, pageSize });
      if (res.code === 200) {
        setOrders(res.data.list);
        setTotal(res.data.total);
      }
    } catch { /* ignore */ }
    finally { setLoading(false); }
  }, [status, page]);

  useEffect(() => { loadOrders(); }, [loadOrders]);

  const totalPages = Math.ceil(total / pageSize);

  return (
    <div className="page-container">
      <h1 className="page-title">我的订单</h1>
      <div className="home-cats mb-4">
        {STATUS_TABS.map(tab => (
          <button key={tab.value} className={`home-cat ${status === tab.value ? 'home-cat--active' : ''}`} onClick={() => { setStatus(tab.value); setPage(1); }}>{tab.label}</button>
        ))}
      </div>
      {loading ? <div className="spinner" /> : orders.length === 0 ? (
        <div className="empty-state"><p>暂无订单</p></div>
      ) : (
        <>
          <div className="flex flex-col gap-3">
            {orders.map(order => (
              <Link to={`/orders/${order.id}`} key={order.id} className="card" style={{ textDecoration: 'none', color: 'inherit' }}>
                <div className="flex justify-between items-center mb-2">
                  <span className="text-xs text-secondary">订单 #{order.id}</span>
                  <StatusBadge status={order.status} />
                </div>
                <div className="flex justify-between items-center">
                  <span className="text-sm text-secondary">{order.create_time?.slice(0, 16)}</span>
                  <span className="font-bold">&yen;{Number(order.total_amount).toFixed(2)}</span>
                </div>
              </Link>
            ))}
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
