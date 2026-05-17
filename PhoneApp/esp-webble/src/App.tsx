import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import ConsolePage from "./console/ConsolePage";
import AccountPage from "./pages/AccountPage";
import MainPage from "./pages/MainPage";

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Navigate to="/main" replace />} />
        <Route path="/main" element={<MainPage />} />
        <Route path="/account" element={<AccountPage />} />
        <Route path="/console" element={<ConsolePage />} />
      </Routes>
    </BrowserRouter>
  );
}
