import { useState, useEffect, useCallback } from 'react';
import * as categoriesApi from '../../api/categories';
import type { Category } from '../../types';
import './Merchant.css';

export default function CategoryManagePage() {
  const [categories, setCategories] = useState<Category[]>([]);
  const [loading, setLoading] = useState(false);
  const [newName, setNewName] = useState('');
  const [newWeight, setNewWeight] = useState('100');
  const [editingId, setEditingId] = useState<number | null>(null);
  const [editForm, setEditForm] = useState({ name: '', sortWeight: '100' });

  const loadCategories = useCallback(async () => {
    setLoading(true);
    try {
      const res = await categoriesApi.getMerchantCategories();
      if (res.code === 200) setCategories(res.data);
    } catch { /* ignore */ }
    finally { setLoading(false); }
  }, []);

  useEffect(() => { loadCategories(); }, [loadCategories]);

  const handleAdd = async () => {
    if (!newName.trim()) return;
    try {
      await categoriesApi.addCategory(newName.trim(), Number(newWeight));
      setNewName('');
      loadCategories();
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '添加失败'); }
  };

  const handleUpdate = async (id: number) => {
    try {
      await categoriesApi.updateCategory(id, { name: editForm.name, sortWeight: Number(editForm.sortWeight) });
      setEditingId(null);
      loadCategories();
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '更新失败'); }
  };

  const startEdit = (cat: Category) => {
    setEditingId(cat.id);
    setEditForm({ name: cat.name, sortWeight: String(cat.sortWeight || 100) });
  };

  return (
    <div className="page-container">
      <h1 className="page-title">分类管理</h1>
      {/* 新增 */}
      <div className="card mb-4 flex gap-3 items-end">
        <div className="form-group flex-1" style={{ marginBottom: 0 }}>
          <label>名称</label>
          <input className="form-input" value={newName} onChange={e => setNewName(e.target.value)} placeholder="分类名称" />
        </div>
        <div className="form-group" style={{ width: 100, marginBottom: 0 }}>
          <label>权重</label>
          <input className="form-input" type="number" value={newWeight} onChange={e => setNewWeight(e.target.value)} />
        </div>
        <button className="btn btn-primary" onClick={handleAdd}>添加</button>
      </div>
      {/* 列表 */}
      {loading ? <div className="spinner" /> : categories.length === 0 ? (
        <div className="empty-state"><p>暂无分类</p></div>
      ) : (
        <div className="table-wrap">
          <table className="table">
            <thead><tr><th>名称</th><th>权重</th><th>操作</th></tr></thead>
            <tbody>
              {categories.map(cat => editingId === cat.id ? (
                <tr key={cat.id}>
                  <td><input className="form-input" value={editForm.name} onChange={e => setEditForm({ ...editForm, name: e.target.value })} /></td>
                  <td><input className="form-input" type="number" value={editForm.sortWeight} onChange={e => setEditForm({ ...editForm, sortWeight: e.target.value })} style={{ width: 80 }} /></td>
                  <td>
                    <div className="flex gap-2">
                      <button className="btn btn-primary btn-sm" onClick={() => handleUpdate(cat.id)}>保存</button>
                      <button className="btn btn-sm" onClick={() => setEditingId(null)}>取消</button>
                    </div>
                  </td>
                </tr>
              ) : (
                <tr key={cat.id}>
                  <td className="font-bold">{cat.name}</td>
                  <td className="text-secondary">{cat.sortWeight || 100}</td>
                  <td><button className="btn btn-sm" onClick={() => startEdit(cat)}>编辑</button></td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
