import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import ConsolePage from "./console/ConsolePage";
import AccountPage from "./pages/AccountPage";
import GoNoGoPage from "./pages/GoNoGoPage";
import GoNoGoResultsPage from "./pages/GoNoGoResultsPage";
import MainPage from "./pages/MainPage";
import SnakePage from "./pages/SnakePage";
import SnakeResultsPage from "./pages/SnakeResultsPage";

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Navigate to="/main" replace />} />
        <Route path="/main" element={<MainPage />} />
        <Route path="/account" element={<AccountPage />} />
        <Route path="/console" element={<ConsolePage />} />
        <Route path="/games/go-no-go" element={<GoNoGoPage />} />
        <Route path="/games/go-no-go/results" element={<GoNoGoResultsPage />} />
        <Route path="/games/snake" element={<SnakePage />} />
        <Route path="/games/snake/results" element={<SnakeResultsPage />} />
      </Routes>
    </BrowserRouter>
  );
}
