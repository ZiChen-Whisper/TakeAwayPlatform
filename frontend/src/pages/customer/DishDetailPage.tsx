import { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { useCart } from '../../contexts/CartContext';
import QuantityControl from '../../components/QuantityControl';
import * as dishesApi from '../../api/dishes';
import type { Dish } from '../../types';
import './Customer.css';

export default function DishDetailPage() {
  const { id } = useParams<{ id: string }>();
  const navigate = useNavigate();
  const { addToCart } = useCart();
  const [dish, setDish] = useState<Dish | null>(null);
  const [quantity, setQuantity] = useState(1);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (!id) return;
    setLoading(true);
    dishesApi.getDishDetail(Number(id))
      .then(res => { if (res.code === 200) setDish(res.data); })
      .catch(() => {})
      .finally(() => setLoading(false));
  }, [id]);

  const handleAddToCart = async () => {
    if (!dish) return;
    try {
      await addToCart(dish.id, quantity);
      alert('已加入购物车');
    } catch (err: unknown) {
      alert(err instanceof Error ? err.message : '添加失败');
    }
  };

  if (loading) return <div className="spinner" />;
  if (!dish) return <div className="page-container"><div className="empty-state"><p>菜品不存在</p></div></div>;

  return (
    <div className="page-container">
      <button className="back-link" onClick={() => navigate(-1)}>&larr; 返回</button>
      <div className="dish-detail">
        <div className="dish-detail__img">{dish.name[0]}</div>
        <div className="dish-detail__info">
          <h1 className="dish-detail__name">{dish.name}</h1>
          {dish.description && <p className="dish-detail__desc">{dish.description}</p>}
          <div className="dish-detail__meta">
            <span className="dish-detail__price">&yen;{Number(dish.price).toFixed(2)}</span>
            <span className={`dish-detail__stk ${dish.stock <= 0 ? 'dish-detail__stk--out' : ''}`}>
              {dish.stock > 0 ? `库存 ${dish.stock}` : '已售罄'}
            </span>
          </div>
          {dish.category_name && <p className="text-sm text-secondary">分类：{dish.category_name}</p>}
          {dish.merchant_name && <p className="text-sm text-secondary mt-2">商家：{dish.merchant_name}</p>}
          {dish.stock > 0 && (
            <div className="dish-detail__actions">
              <QuantityControl quantity={quantity} onChange={setQuantity} max={dish.stock} />
              <button className="btn btn-primary" onClick={handleAddToCart} style={{ marginLeft: 16 }}>
                加入购物车 &yen;{(dish.price * quantity).toFixed(2)}
              </button>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
