import { createContext, useContext, useState, useCallback, type ReactNode } from 'react';
import type { Cart } from '../types';
import * as cartApi from '../api/cart';

interface CartState {
  cart: Cart | null;
  loading: boolean;
  refreshCart: () => Promise<void>;
  addToCart: (dishId: number, quantity: number) => Promise<void>;
  updateQuantity: (itemId: number, quantity: number) => Promise<void>;
  removeItem: (itemId: number) => Promise<void>;
  clearCart: () => void;
}

const CartContext = createContext<CartState | null>(null);

export function CartProvider({ children }: { children: ReactNode }) {
  const [cart, setCart] = useState<Cart | null>(null);
  const [loading, setLoading] = useState(false);

  const refreshCart = useCallback(async () => {
    try {
      setLoading(true);
      const res = await cartApi.getCart();
      if (res.code === 200) {
        setCart(res.data);
      }
    } catch {
      // 未登录时忽略
    } finally {
      setLoading(false);
    }
  }, []);

  const addToCart = useCallback(async (dishId: number, quantity: number) => {
    const res = await cartApi.addToCart(dishId, quantity);
    if (res.code === 200) {
      setCart(res.data);
    } else {
      throw new Error(res.message);
    }
  }, []);

  const updateQuantity = useCallback(async (itemId: number, quantity: number) => {
    const res = await cartApi.updateQuantity(itemId, quantity);
    if (res.code === 200 && cart) {
      // 更新本地购物车
      const newItems = cart.items.map(item =>
        item.id === itemId ? { ...item, quantity } : item
      ).filter(item => item.quantity > 0);
      setCart({ ...cart, items: newItems, totalPrice: res.data.totalPrice });
    }
  }, [cart]);

  const removeItem = useCallback(async (itemId: number) => {
    const res = await cartApi.removeItem(itemId);
    if (res.code === 200 && cart) {
      const newItems = cart.items.filter(item => item.id !== itemId);
      const newTotal = newItems.reduce((sum, item) => sum + item.dish_price * item.quantity, 0);
      setCart({ ...cart, items: newItems, totalPrice: Math.round(newTotal * 100) / 100 });
    }
  }, [cart]);

  const clearCart = useCallback(() => {
    setCart(null);
  }, []);

  return (
    <CartContext.Provider value={{ cart, loading, refreshCart, addToCart, updateQuantity, removeItem, clearCart }}>
      {children}
    </CartContext.Provider>
  );
}

export function useCart() {
  const ctx = useContext(CartContext);
  if (!ctx) throw new Error('useCart must be used within CartProvider');
  return ctx;
}
