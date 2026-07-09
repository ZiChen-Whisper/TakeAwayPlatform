import { useState, useEffect, useCallback } from 'react';
import { Link } from 'react-router-dom';
import * as dishesApi from '../../api/dishes';
import * as categoriesApi from '../../api/categories';
import type { Dish, Category } from '../../types';
import './Merchant.css';

export default function DishManagePage() {
  const [dishes, setDishes] = useState<Dish[]>([]);
  const [categories, setCategories] = useState<Category[]>([]);
  const [categoryId, setCategoryId] = useState(0);
  const [loading, setLoading] = useState(false);

  const loadDishes = useCallback(async () => {
    setLoading(true);
    try {
      const res = await dishesApi.getMerchantDishes({ categoryId: categoryId || undefined, page: 1, pageSize: 100 });
      if (res.code === 200) setDishes(res.data.list);
    } catch { /* ignore */ }
    finally { setLoading(false); }
  }, [categoryId]);

  const loadCategories = useCallback(async () => {
    try {
      const res = await categoriesApi.getMerchantCategories();
      if (res.code === 200) setCategories(res.data);
    } catch { /* ignore */ }
  }, []);

  useEffect(() => { loadDishes(); }, [loadDishes]);
  useEffect(() => { loadCategories(); }, [loadCategories]);

  const handleToggleStatus = async (dish: Dish) => {
    const newStatus = dish.status === 1 ? 0 : 1;
    try {
      await dishesApi.setDishStatus(dish.id, newStatus);
      loadDishes();
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '操作失败'); }
  };

  const handleDelete = async (id: number) => {
    if (!confirm('确定删除？')) return;
    try {
      await dishesApi.deleteDish(id);
      loadDishes();
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '删除失败'); }
  };

  return (
    <div className="page-container">
      <div className="page-header">
        <h1 className="page-title" style={{ marginBottom: 0 }}>菜品管理</h1>
        <Link to="/merchant/dishes/new" className="btn btn-primary">新增菜品</Link>
      </div>
      <div className="home-cats mb-4">
        <button className={`home-cat ${categoryId === 0 ? 'home-cat--active' : ''}`} onClick={() => setCategoryId(0)}>全部</button>
        {categories.map(cat => (
          <button key={cat.id} className={`home-cat ${categoryId === cat.id ? 'home-cat--active' : ''}`} onClick={() => setCategoryId(cat.id)}>{cat.name}</button>
        ))}
      </div>
      {loading ? <div className="spinner" /> : dishes.length === 0 ? (
        <div className="empty-state"><p>暂无菜品</p></div>
      ) : (
        <div className="table-wrap">
          <table className="table">
            <thead>
              <tr><th>菜名</th><th>分类</th><th>价格</th><th>库存</th><th>状态</th><th>操作</th></tr>
            </thead>
            <tbody>
              {dishes.map(dish => (
                <tr key={dish.id}>
                  <td className="font-bold">{dish.name}</td>
                  <td className="text-secondary">{dish.category_name || '-'}</td>
                  <td>&yen;{Number(dish.price).toFixed(2)}</td>
                  <td>{dish.stock}</td>
                  <td><button className={`btn btn-sm ${dish.status === 1 ? 'btn-success' : 'btn'}`} onClick={() => handleToggleStatus(dish)}>{dish.status === 1 ? '已上架' : '已下架'}</button></td>
                  <td>
                    <div className="flex gap-2">
                      <Link to={`/merchant/dishes/${dish.id}/edit`} className="btn btn-sm">编辑</Link>
                      <button className="btn btn-sm btn-danger" onClick={() => handleDelete(dish.id)}>删除</button>
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
