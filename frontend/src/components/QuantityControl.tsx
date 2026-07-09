import './QuantityControl.css';

interface Props {
  quantity: number;
  onChange: (qty: number) => void;
  min?: number;
  max?: number;
}

export default function QuantityControl({ quantity, onChange, min = 1, max = 999 }: Props) {
  return (
    <div className="qty-control">
      <button
        className="qty-control__btn"
        onClick={() => onChange(quantity - 1)}
        disabled={quantity <= min}
      >-</button>
      <span className="qty-control__val">{quantity}</span>
      <button
        className="qty-control__btn"
        onClick={() => onChange(quantity + 1)}
        disabled={quantity >= max}
      >+</button>
    </div>
  );
}
