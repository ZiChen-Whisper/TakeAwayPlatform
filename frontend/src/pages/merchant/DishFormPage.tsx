import { useState, useEffect, type FormEvent } from 'react';
import { useNavigate, useParams } from 'react-router-dom';
import * as dishesApi from '../../api/dishes';
import * as categoriesApi from '../../api/categories';
import type { Category } from '../../types';
import './Merchant.css';

export default function DishFormPage() {
  const { id } = useParams<{ id: string }>();
  const navigate = useNavigate();
  const isEdit = !!id;
  const [categories, setCategories] = useState<Category[]>([]);
  const [form, setForm] = useState({ name: '', categoryId: 0, price: '', description: '', image: '', stock: '0' });
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    categoriesApi.getMerchantCategories().then(res => { if (res.code === 200) setCategories(res.data); });
    if (!id) return;
    // 加载菜品数据（简化：从商家菜品列表获取）
    dishesApi.getMerchantDishes({ page: 1, pageSize: 200 }).then(res => {
      if (res.code === 200) {
        const dish = res.data.list.find((d: { id: number }) => d.id === Number(id));
        if (dish) setForm({
          name: dish.name, categoryId: dish.category_id,
          price: String(dish.price), description: dish.description || '',
          image: dish.image || '', stock: String(dish.stock)
        });
      }
    });
  }, [id]);

  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault();
    if (!form.name || !form.categoryId || !form.price) { alert('请填写必填项'); return; }
    setLoading(true);
    try {
      const body = { name: form.name, categoryId: form.categoryId, price: Number(form.price), description: form.description, image: form.image, stock: Number(form.stock) };
      if (isEdit) {
        await dishesApi.updateDish(Number(id), body);
      } else {
        await dishesApi.addDish(body);
      }
      navigate('/merchant/dishes');
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '保存失败'); }
    finally { setLoading(false); }
  };

  return (
    <div className="page-container page-container--narrow">
      <button className="back-link" onClick={() => navigate(-1)}>&larr; 返回</button>
      <h1 className="page-title">{isEdit ? '编辑菜品' : '新增菜品'}</h1>
      <form onSubmit={handleSubmit}>
        <div className="form-group"><label>菜名 *</label><input className="form-input" value={form.name} onChange={e => setForm({ ...form, name: e.target.value })} required /></div>
        <div className="form-group">
          <label>分类 *</label>
          <select className="form-select" value={form.categoryId} onChange={e => setForm({ ...form, categoryId: Number(e.target.value) })} required>
            <option value={0}>请选择分类</option>
            {categories.map(c => <option key={c.id} value={c.id}>{c.name}</option>)}
          </select>
        </div>
        <div className="form-group"><label>价格 *</label><input className="form-input" type="number" step="0.01" value={form.price} onChange={e => setForm({ ...form, price: e.target.value })} required /></div>
        <div className="form-group"><label>库存 *</label><input className="form-input" type="number" value={form.stock} onChange={e => setForm({ ...form, stock: e.target.value })} required /></div>
        <div className="form-group"><label>描述</label><textarea className="form-input" rows={3} value={form.description} onChange={e => setForm({ ...form, description: e.target.value })} /></div>
        <div className="form-group"><label>图片URL</label><input className="form-input" value={form.image} onChange={e => setForm({ ...form, image: e.target.value })} placeholder="https://..." /></div>
        <button className="btn btn-primary w-full" disabled={loading}>{loading ? '保存中...' : '保存'}</button>
      </form>
    </div>
  );
}
