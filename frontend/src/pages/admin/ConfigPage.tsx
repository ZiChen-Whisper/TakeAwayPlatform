import { useState, useEffect, type FormEvent } from 'react';
import * as adminApi from '../../api/admin';
import './Admin.css';

export default function ConfigPage() {
  const [config, setConfig] = useState<Record<string, string>>({});
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    adminApi.getConfig().then(res => {
      if (res.code === 200 && res.data) setConfig(res.data);
    }).catch(() => {}).finally(() => setLoading(false));
  }, []);

  const handleSave = async (e: FormEvent) => {
    e.preventDefault();
    setSaving(true);
    try {
      for (const [key, value] of Object.entries(config)) {
        await adminApi.updateConfig(key, value);
      }
      alert('保存成功');
    } catch (err: unknown) { alert(err instanceof Error ? err.message : '保存失败'); }
    finally { setSaving(false); }
  };

  if (loading) return <div className="spinner" />;

  return (
    <div className="page-container page-container--narrow">
      <h1 className="page-title">系统配置</h1>
      <form onSubmit={handleSave}>
        {Object.entries(config).map(([key, value]) => (
          <div className="form-group" key={key}>
            <label>{key}</label>
            <input className="form-input" value={value} onChange={e => setConfig({ ...config, [key]: e.target.value })} />
          </div>
        ))}
        <button className="btn btn-primary w-full" disabled={saving}>{saving ? '保存中...' : '保存配置'}</button>
      </form>
    </div>
  );
}
