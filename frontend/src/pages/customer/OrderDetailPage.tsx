import { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import StatusBadge from '../../components/StatusBadge';
import * as orderApi from '../../api/orders';
import type { Order, OrderItem } from '../../types';
import './Customer.css';

export default function OrderDetailPage() {
  const { id } = useParams<{ id: string }>();
  const navigate = useNavigate();
  const [order, setOrder] = useState<Order | null>(null);
  const [items, setItems] = useState<OrderItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [paying, setPaying] = useState(false);

  useEffect(() => {
    if (!id) return;
    setLoading(true);
    orderApi.getOrderDetail(Number(id))
      .then(res => {
        if (res.code === 200) {
          setOrder(res.data.order);
          setItems(res.data.items || []);
        }
      })
      .catch(() => {})
      .finally(() => setLoading(false));
  }, [id]);

  const handlePay = async () => {
    if (!order) return;
    setPaying(true);
    try {
      const res = await orderApi.payOrder(order.id);
      if (res.code === 200) {
        setOrder({ ...order, status: res.data.status, statusName: res.data.statusName });
      } else {
        alert(res.message);
      }
    } catch (err: unknown) {
      alert(err instanceof Error ? err.message : '支付失败');
    } finally {
      setPaying(false);
    }
  };

  if (loading) return <div className="spinner" />;
  if (!order) return <div className="page-container"><div className="empty-state"><p>订单不存在</p></div></div>;

  const addr = order.address_snapshot ? (() => { try { return JSON.parse(order.address_snapshot); } catch { return null; } })() : null;

  return (
    <div className="page-container">
      <button className="back-link" onClick={() => navigate(-1)}>&larr; 返回</button>
      <div className="card mb-4">
        <div className="flex justify-between items-center mb-3">
          <h2 className="page-title" style={{ marginBottom: 0 }}>订单 #{order.id}</h2>
          <StatusBadge status={order.status} />
        </div>
        <div className="text-sm text-secondary mb-2">下单时间：{order.create_time?.slice(0, 16)}</div>
        {order.pay_time && <div className="text-sm text-secondary mb-2">支付时间：{order.pay_time?.slice(0, 16)}</div>}
        <div className="text-lg font-bold mt-2">合计：&yen;{Number(order.total_amount).toFixed(2)}</div>
        {order.remark && <div className="text-sm text-secondary mt-2">备注：{order.remark}</div>}
      </div>

      {addr && (
        <div className="card mb-4">
          <h3 className="font-bold mb-2">收货信息</h3>
          <div className="text-sm">{addr.name} {addr.phone}</div>
          <div className="text-sm text-secondary">{addr.province}{addr.city}{addr.district} {addr.detail}</div>
        </div>
      )}

      <div className="card mb-4">
        <h3 className="font-bold mb-2">商品明细</h3>
        {items.map(item => (
          <div key={item.id} className="flex justify-between text-sm" style={{ padding: '8px 0', borderBottom: '1px solid var(--border)' }}>
            <span>{item.dish_name} x{item.quantity}</span>
            <span>&yen;{(item.dish_price * item.quantity).toFixed(2)}</span>
          </div>
        ))}
      </div>

      {order.status === 0 && (
        <button className="btn btn-success w-full" disabled={paying} onClick={handlePay}>
          {paying ? '支付中...' : '去支付'}
        </button>
      )}
    </div>
  );
}
