import { useState, useEffect, type FormEvent } from 'react';
import { useNavigate, useParams } from 'react-router-dom';
import * as addressApi from '../../api/address';
import './Customer.css';

export default function AddressFormPage() {
  const { id } = useParams<{ id: string }>();
  const navigate = useNavigate();
  const isEdit = !!id;
  const [form, setForm] = useState({ name: '', phone: '', province: '', city: '', district: '', detail: '', isDefault: false });
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    if (!id) return;
    addressApi.getAddresses().then(res => {
      if (res.code === 200) {
        const addr = res.data.find((a: { id: number }) => a.id === Number(id));
        if (addr) setForm({ name: addr.name, phone: addr.phone, province: addr.province, city: addr.city, district: addr.district, detail: addr.detail, isDefault: !!addr.is_default });
      }
    });
  }, [id]);

  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault();
    if (!form.name || !form.phone || !form.detail) { alert('请填写必填项'); return; }
    setLoading(true);
    try {
      if (isEdit) {
        await addressApi.updateAddress(Number(id), form);
      } else {
        await addressApi.addAddress(form);
      }
      navigate('/addresses');
    } catch (err: unknown) {
      alert(err instanceof Error ? err.message : '保存失败');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="page-container page-container--narrow">
      <button className="back-link" onClick={() => navigate(-1)}>&larr; 返回</button>
      <h1 className="page-title">{isEdit ? '编辑地址' : '新增地址'}</h1>
      <form onSubmit={handleSubmit}>
        <div className="form-group"><label>联系人</label><input className="form-input" value={form.name} onChange={e => setForm({ ...form, name: e.target.value })} required /></div>
        <div className="form-group"><label>电话</label><input className="form-input" value={form.phone} onChange={e => setForm({ ...form, phone: e.target.value })} required /></div>
        <div className="flex gap-3">
          <div className="form-group flex-1"><label>省</label><input className="form-input" value={form.province} onChange={e => setForm({ ...form, province: e.target.value })} /></div>
          <div className="form-group flex-1"><label>市</label><input className="form-input" value={form.city} onChange={e => setForm({ ...form, city: e.target.value })} /></div>
          <div className="form-group flex-1"><label>区</label><input className="form-input" value={form.district} onChange={e => setForm({ ...form, district: e.target.value })} /></div>
        </div>
        <div className="form-group"><label>详细地址</label><input className="form-input" value={form.detail} onChange={e => setForm({ ...form, detail: e.target.value })} required /></div>
        <label className="flex items-center gap-2 mb-4"><input type="checkbox" checked={form.isDefault} onChange={e => setForm({ ...form, isDefault: e.target.checked })} /> 设为默认地址</label>
        <button className="btn btn-primary w-full" disabled={loading}>{loading ? '保存中...' : '保存'}</button>
      </form>
    </div>
  );
}
