import { useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useCart } from '../../contexts/CartContext';
import QuantityControl from '../../components/QuantityControl';
import './Customer.css';

export default function CartPage() {
  const { cart, loading, refreshCart, updateQuantity, removeItem } = useCart();
  const navigate = useNavigate();

  useEffect(() => { refreshCart(); }, [refreshCart]);

  if (loading) return <div className="spinner" />;
  if (!cart || cart.items.length === 0) {
    return (
      <div className="page-container">
        <div className="empty-state">
          <h2>购物车是空的</h2>
          <p><Link to="/">去逛逛</Link></p>
        </div>
      </div>
    );
  }

  return (
    <div className="page-container">
      <h1 className="page-title">购物车</h1>
      <div className="cart-list">
        {cart.items.map(item => (
          <div key={item.id} className="cart-item card flex justify-between items-center">
            <div className="flex-1">
              <div className="font-bold">{item.dish_name || item.name || `菜品#${item.dish_id}`}</div>
              <div className="text-sm text-secondary">&yen;{Number(item.dish_price).toFixed(2)}</div>
            </div>
            <div className="flex items-center gap-4">
              <QuantityControl
                quantity={item.quantity}
                onChange={qty => { if (qty > 0) updateQuantity(item.id, qty); else removeItem(item.id); }}
              />
              <span className="font-bold">&yen;{(item.dish_price * item.quantity).toFixed(2)}</span>
              <button className="btn btn-sm btn-danger" onClick={() => removeItem(item.id)}>删除</button>
            </div>
          </div>
        ))}
      </div>
      <div className="cart-footer card flex justify-between items-center">
        <span className="text-lg">
          合计：<strong>&yen;{Number(cart.totalPrice).toFixed(2)}</strong>
        </span>
        <button className="btn btn-primary" onClick={() => navigate('/checkout')}>去结算</button>
      </div>
    </div>
  );
}
