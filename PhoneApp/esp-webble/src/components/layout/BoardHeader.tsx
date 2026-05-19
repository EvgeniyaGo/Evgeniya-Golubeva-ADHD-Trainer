import { useState, type ReactNode } from "react";
import { useNavigate } from "react-router-dom";
import { useAuth } from "../../auth/useAuth";
import { AuthModal } from "../auth/AuthModal";
import { AccountIcon } from "../navIcons";

type BoardHeaderProps = {
  menuOpen: boolean;
  rightContent?: ReactNode;
  onToggleMenu: () => void;
};

export function BoardHeader({
  menuOpen,
  rightContent,
  onToggleMenu,
}: BoardHeaderProps) {
  const navigate = useNavigate();
  const { firebaseUser, loading, logout } = useAuth();
  const [authOpen, setAuthOpen] = useState(false);
  const isLoggedIn = Boolean(firebaseUser);

  const handleAccountClick = () => {
    if (loading) return;

    if (isLoggedIn) {
      navigate("/account");
      return;
    }

    setAuthOpen(true);
  };

  const handleLogout = () => {
    void logout();
  };

  return (
    <>
      <header className="board-header">
        <button
          className="menu-toggle top-icon"
          type="button"
          aria-pressed={menuOpen}
          aria-label={menuOpen ? "Collapse menu" : "Expand menu"}
          title={menuOpen ? "Collapse menu" : "Expand menu"}
          onClick={onToggleMenu}
        >
          <svg width="26" height="26" viewBox="0 0 24 24" fill="none">
            <path
              d="M3 7h18M3 12h18M3 17h18"
              stroke="#697586"
              strokeWidth="2"
              strokeLinecap="round"
            />
          </svg>
        </button>

        <div className="top-right">
          {rightContent}
          <button
            className="menu-toggle top-icon"
            type="button"
            aria-label={isLoggedIn ? "Go to account" : "Open account"}
            title={loading ? "Loading account" : "Account"}
            disabled={loading}
            onClick={handleAccountClick}
          >
            {loading ? (
              <span className="account-loading">Loading...</span>
            ) : (
              <AccountIcon label="Account" />
            )}
          </button>
          {isLoggedIn && !loading && (
            <button
              className="account-logout"
              type="button"
              onClick={handleLogout}
            >
              Log out
            </button>
          )}
        </div>
      </header>
      {authOpen && !isLoggedIn && (
        <AuthModal onClose={() => setAuthOpen(false)} />
      )}
    </>
  );
}
