const STATUS_MAP: Record<number, { label: string; className: string }> = {
  0: { label: '待支付', className: 'badge-warning' },
  1: { label: '待接单', className: 'badge-info' },
  2: { label: '配送中', className: 'badge-info' },
  3: { label: '已完成', className: 'badge-success' },
  4: { label: '已取消', className: 'badge-neutral' },
};

export default function StatusBadge({ status }: { status: number }) {
  const info = STATUS_MAP[status] || { label: '未知', className: 'badge-neutral' };
  return <span className={`badge ${info.className}`}>{info.label}</span>;
}
