import { useState, useEffect } from 'react';
import { useNavigate, Link } from 'react-router-dom';
import { useCart } from '../../contexts/CartContext';
import * as addressApi from '../../api/address';
import * as orderApi from '../../api/orders';
import type { Address } from '../../types';
import './Customer.css';

export default function CheckoutPage() {
  const { cart, refreshCart, clearCart } = useCart();
  const navigate = useNavigate();
  const [addresses, setAddresses] = useState<Address[]>([]);
  const [selectedAddr, setSelectedAddr] = useState<number>(0);
  const [remark, setRemark] = useState('');
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    refreshCart();
    addressApi.getAddresses().then(res => {
      if (res.code === 200) {
        setAddresses(res.data);
        const def = res.data.find((a: Address) => a.is_default === 1);
        if (def) setSelectedAddr(def.id);
        else if (res.data.length > 0) setSelectedAddr(res.data[0].id);
      }
    });
  }, [refreshCart]);

  const handleSubmit = async () => {
    if (!selectedAddr) { alert('请选择收货地址'); return; }
    if (!cart || cart.items.length === 0) { alert('购物车为空'); return; }
    setLoading(true);
    try {
      const res = await orderApi.createOrder({ addressId: selectedAddr, remark });
      if (res.code === 200) {
        clearCart();
        navigate(`/orders/${res.data.id}`, { replace: true });
      } else {
        alert(res.message);
      }
    } catch (err: unknown) {
      alert(err instanceof Error ? err.message : '下单失败');
    } finally {
      setLoading(false);
    }
  };

  if (!cart || cart.items.length === 0) {
    return <div className="page-container"><div className="empty-state"><p>购物车为空 <Link to="/">去逛逛</Link></p></div></div>;
  }

  return (
    <div className="page-container">
      <h1 className="page-title">确认订单</h1>
      {/* 地址 */}
      <div className="card mb-4">
        <h3 className="font-bold mb-2">收货地址</h3>
        {addresses.length === 0 ? (
          <p className="text-sm text-secondary">暂无地址，<Link to="/addresses/new">去添加</Link></p>
        ) : (
          <div className="flex flex-col gap-2">
            {addresses.map(addr => (
              <label key={addr.id} className={`checkout-addr ${selectedAddr === addr.id ? 'checkout-addr--active' : ''}`}>
                <input type="radio" name="addr" checked={selectedAddr === addr.id} onChange={() => setSelectedAddr(addr.id)} />
                <div>
                  <strong>{addr.name}</strong> {addr.phone}
                  <div className="text-sm text-secondary">{addr.province}{addr.city}{addr.district} {addr.detail}</div>
                </div>
              </label>
            ))}
          </div>
        )}
      </div>
      {/* 商品 */}
      <div className="card mb-4">
        <h3 className="font-bold mb-2">商品明细</h3>
        {cart.items.map(item => (
          <div key={item.id} className="flex justify-between text-sm" style={{ padding: '8px 0', borderBottom: '1px solid var(--border)' }}>
            <span>{item.dish_name || item.name} x{item.quantity}</span>
            <span>&yen;{(item.dish_price * item.quantity).toFixed(2)}</span>
          </div>
        ))}
      </div>
      {/* 备注 */}
      <div className="card mb-4">
        <h3 className="font-bold mb-2">备注</h3>
        <textarea className="form-input" rows={2} value={remark} onChange={e => setRemark(e.target.value)} placeholder="如有特殊需求请备注" />
      </div>
      {/* 合计 */}
      <div className="card flex justify-between items-center">
        <span className="text-lg">合计：<strong>&yen;{Number(cart.totalPrice).toFixed(2)}</strong></span>
        <button className="btn btn-primary" disabled={loading || !selectedAddr} onClick={handleSubmit}>
          {loading ? '提交中...' : '提交订单'}
        </button>
      </div>
    </div>
  );
}
