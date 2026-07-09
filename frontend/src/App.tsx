import { Routes, Route, Navigate } from 'react-router-dom';
import { AuthProvider } from './contexts/AuthContext';
import { CartProvider } from './contexts/CartContext';
import Layout from './components/Layout';
import ProtectedRoute from './components/ProtectedRoute';
import LoginPage from './pages/auth/LoginPage';
import RegisterPage from './pages/auth/RegisterPage';
import HomePage from './pages/customer/HomePage';
import DishDetailPage from './pages/customer/DishDetailPage';
import CartPage from './pages/customer/CartPage';
import CheckoutPage from './pages/customer/CheckoutPage';
import AddressListPage from './pages/customer/AddressListPage';
import AddressFormPage from './pages/customer/AddressFormPage';
import OrderListPage from './pages/customer/OrderListPage';
import OrderDetailPage from './pages/customer/OrderDetailPage';
import MerchantOrdersPage from './pages/merchant/MerchantOrdersPage';
import DishManagePage from './pages/merchant/DishManagePage';
import DishFormPage from './pages/merchant/DishFormPage';
import CategoryManagePage from './pages/merchant/CategoryManagePage';
import AdminDashboard from './pages/admin/DashboardPage';
import AdminUsers from './pages/admin/UserManagePage';
import AdminMerchants from './pages/admin/MerchantManagePage';
import AdminConfig from './pages/admin/ConfigPage';

export default function App() {
  return (
    <AuthProvider>
      <CartProvider>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          <Route path="/register" element={<RegisterPage />} />
          <Route element={<ProtectedRoute />}>
            <Route element={<Layout />}>
              {/* 用户端 */}
              <Route path="/" element={<HomePage />} />
              <Route path="/dish/:id" element={<DishDetailPage />} />
              <Route path="/cart" element={<CartPage />} />
              <Route path="/checkout" element={<CheckoutPage />} />
              <Route path="/addresses" element={<AddressListPage />} />
              <Route path="/addresses/new" element={<AddressFormPage />} />
              <Route path="/addresses/:id/edit" element={<AddressFormPage />} />
              <Route path="/orders" element={<OrderListPage />} />
              <Route path="/orders/:id" element={<OrderDetailPage />} />
              {/* 商家端 */}
              <Route path="/merchant/orders" element={<ProtectedRoute role={1}><MerchantOrdersPage /></ProtectedRoute>} />
              <Route path="/merchant/dishes" element={<ProtectedRoute role={1}><DishManagePage /></ProtectedRoute>} />
              <Route path="/merchant/dishes/new" element={<ProtectedRoute role={1}><DishFormPage /></ProtectedRoute>} />
              <Route path="/merchant/dishes/:id/edit" element={<ProtectedRoute role={1}><DishFormPage /></ProtectedRoute>} />
              <Route path="/merchant/categories" element={<ProtectedRoute role={1}><CategoryManagePage /></ProtectedRoute>} />
              {/* 管理端 */}
              <Route path="/admin/dashboard" element={<ProtectedRoute role={2}><AdminDashboard /></ProtectedRoute>} />
              <Route path="/admin/users" element={<ProtectedRoute role={2}><AdminUsers /></ProtectedRoute>} />
              <Route path="/admin/merchants" element={<ProtectedRoute role={2}><AdminMerchants /></ProtectedRoute>} />
              <Route path="/admin/config" element={<ProtectedRoute role={2}><AdminConfig /></ProtectedRoute>} />
            </Route>
          </Route>
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </CartProvider>
    </AuthProvider>
  );
}
