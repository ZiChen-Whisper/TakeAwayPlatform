import { useState, useEffect, useCallback } from 'react';
import { Link } from 'react-router-dom';
import { useCart } from '../../contexts/CartContext';
import * as dishesApi from '../../api/dishes';
import type { Dish, Category } from '../../types';
import './Customer.css';

export default function HomePage() {
  const [dishes, setDishes] = useState<Dish[]>([]);
  const [categories, setCategories] = useState<Category[]>([]);
  const [keyword, setKeyword] = useState('');
  const [categoryId, setCategoryId] = useState(0);
  const [page, setPage] = useState(1);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(false);
  const { addToCart, refreshCart } = useCart();

  const pageSize = 12;

  const loadDishes = useCallback(async () => {
    setLoading(true);
    try {
      const res = await dishesApi.searchDishes({ keyword, categoryId: categoryId || undefined, page, pageSize });
      if (res.code === 200) {
        setDishes(res.data.list);
        setTotal(res.data.total);
      }
    } catch { /* ignore */ }
    finally { setLoading(false); }
  }, [keyword, categoryId, page]);

  const loadCategories = useCallback(async () => {
    try {
      const res = await dishesApi.getCategories();
      if (res.code === 200) setCategories(res.data);
    } catch { /* ignore */ }
  }, []);

  useEffect(() => { loadDishes(); }, [loadDishes]);
  useEffect(() => { loadCategories(); refreshCart(); }, [loadCategories, refreshCart]);

  const handleAddToCart = async (dish: Dish, e: React.MouseEvent) => {
    e.stopPropagation();
    e.preventDefault();
    try {
      await addToCart(dish.id, 1);
    } catch (err: unknown) {
      alert(err instanceof Error ? err.message : '添加失败');
    }
  };

  const totalPages = Math.ceil(total / pageSize);

  return (
    <div className="page-container">
      {/* 搜索栏 */}
      <div className="home-search">
        <input
          className="form-input"
          placeholder="搜索菜品..."
          value={keyword}
          onChange={e => { setKeyword(e.target.value); setPage(1); }}
        />
      </div>

      {/* 分类筛选 */}
      <div className="home-cats">
        <button className={`home-cat ${categoryId === 0 ? 'home-cat--active' : ''}`} onClick={() => { setCategoryId(0); setPage(1); }}>全部</button>
        {categories.map(cat => (
          <button key={cat.id} className={`home-cat ${categoryId === cat.id ? 'home-cat--active' : ''}`} onClick={() => { setCategoryId(cat.id); setPage(1); }}>{cat.name}</button>
        ))}
      </div>

      {/* 菜品列表 */}
      {loading ? <div className="spinner" /> : (
        dishes.length === 0 ? (
          <div className="empty-state"><p>暂无菜品</p></div>
        ) : (
          <>
            <div className="home-grid">
              {dishes.map(dish => (
                <Link to={`/dish/${dish.id}`} key={dish.id} className="dish-card card">
                  <div className="dish-card__img">{dish.name[0]}</div>
                  <div className="dish-card__info">
                    <h3 className="dish-card__name">{dish.name}</h3>
                    {dish.description && <p className="dish-card__desc">{dish.description}</p>}
                    <div className="flex justify-between items-center">
                      <span className="dish-card__price">&yen;{Number(dish.price).toFixed(2)}</span>
                      <button className="dish-card__add" onClick={(e) => handleAddToCart(dish, e)} title="加入购物车">+</button>
                    </div>
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
        )
      )}
    </div>
  );
}
