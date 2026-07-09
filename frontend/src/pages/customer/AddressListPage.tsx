import { useState, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import * as addressApi from '../../api/address';
import type { Address } from '../../types';
import './Customer.css';

export default function AddressListPage() {
  const [addresses, setAddresses] = useState<Address[]>([]);
  const [loading, setLoading] = useState(true);
  const navigate = useNavigate();

  const loadAddresses = async () => {
    setLoading(true);
    try {
      const res = await addressApi.getAddresses();
      if (res.code === 200) setAddresses(res.data);
    } catch { /* ignore */ }
    finally { setLoading(false); }
  };

  useEffect(() => { loadAddresses(); }, []);

  const handleDelete = async (id: number) => {
    if (!confirm('确定删除？')) return;
    await addressApi.deleteAddress(id);
    loadAddresses();
  };

  const handleSetDefault = async (id: number) => {
    await addressApi.setDefaultAddress(id);
    loadAddresses();
  };

  if (loading) return <div className="spinner" />;

  return (
    <div className="page-container">
      <div className="page-header">
        <h1 className="page-title" style={{ marginBottom: 0 }}>收货地址</h1>
        <Link to="/addresses/new" className="btn btn-primary">新增地址</Link>
      </div>
      {addresses.length === 0 ? (
        <div className="empty-state"><p>暂无地址</p></div>
      ) : (
        <div className="flex flex-col gap-3">
          {addresses.map(addr => (
            <div key={addr.id} className={`card ${addr.is_default === 1 ? 'addr-default' : ''}`}>
              <div className="flex justify-between items-center mb-2">
                <div>
                  <strong>{addr.name}</strong>
                  <span className="text-sm text-secondary ml-2">{addr.phone}</span>
                  {addr.is_default === 1 && <span className="badge badge-info ml-2">默认</span>}
                </div>
              </div>
              <div className="text-sm text-secondary mb-3">
                {addr.province}{addr.city}{addr.district} {addr.detail}
              </div>
              <div className="flex gap-2">
                <button className="btn btn-sm" onClick={() => navigate(`/addresses/${addr.id}/edit`)}>编辑</button>
                {addr.is_default !== 1 && <button className="btn btn-sm" onClick={() => handleSetDefault(addr.id)}>设为默认</button>}
                <button className="btn btn-sm btn-danger" onClick={() => handleDelete(addr.id)}>删除</button>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
