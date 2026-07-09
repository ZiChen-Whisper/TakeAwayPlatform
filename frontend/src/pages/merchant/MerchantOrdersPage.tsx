import { useState, useEffect, useCallback } from 'react';
import StatusBadge from '../../components/StatusBadge';
import * as orderApi from '../../api/orders';
import type { Order } from '../../types';
import './Merchant.css';

export default function MerchantOrdersPage() {
  const [orders, setOrders] = useState<Order[]>([]);
  const [status, setStatus] = useState(1); // 默认待接单
  const [loading, setLoading] = useState(false);

  const loadOrders = useCallback(async () => {
    setLoading(true);
    try {
      const res = await orderApi.getMerchantOrders({ status, page: 1, pageSize: 50 });
      if (res.code === 200) setOrders(res.data.list);
    } catch { /* ignore */ }
    finally { setLoading(false); }
  }, [status]);

  useEffect(() => { loadOrders(); }, [loadOrders]);

  const handleAccept = async (orderId: number) => {
    try {
      const res = await orderApi.acceptOrder(orderId);
      if (res.code === 200) loadOrders();
      else alert(res.message);
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '操作失败'); }
  };

  const handleComplete = async (orderId: number) => {
    try {
      const res = await orderApi.completeOrder(orderId);
      if (res.code === 200) loadOrders();
      else alert(res.message);
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '操作失败'); }
  };

  return (
    <div className="page-container">
      <h1 className="page-title">订单管理</h1>
      <div className="home-cats mb-4">
        <button className={`home-cat ${status === 1 ? 'home-cat--active' : ''}`} onClick={() => setStatus(1)}>待接单</button>
        <button className={`home-cat ${status === 2 ? 'home-cat--active' : ''}`} onClick={() => setStatus(2)}>配送中</button>
        <button className={`home-cat ${status === 3 ? 'home-cat--active' : ''}`} onClick={() => setStatus(3)}>已完成</button>
      </div>
      {loading ? <div className="spinner" /> : orders.length === 0 ? (
        <div className="empty-state"><p>暂无订单</p></div>
      ) : (
        <div className="flex flex-col gap-3">
          {orders.map(order => (
            <div key={order.id} className="card">
              <div className="flex justify-between items-center mb-2">
                <div>
                  <strong>订单 #{order.id}</strong>
                  <span className="text-sm text-secondary ml-3">{order.create_time?.slice(0, 16)}</span>
                </div>
                <StatusBadge status={order.status} />
              </div>
              <div className="text-sm text-secondary mb-2">用户ID: {order.user_id}</div>
              <div className="text-lg font-bold mb-3">&yen;{Number(order.total_amount).toFixed(2)}</div>
              <div className="flex gap-2">
                {order.status === 1 && (
                  <button className="btn btn-primary btn-sm" onClick={() => handleAccept(order.id)}>确认接单</button>
                )}
                {order.status === 2 && (
                  <button className="btn btn-success btn-sm" onClick={() => handleComplete(order.id)}>标记完成</button>
                )}
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
