import { useState, type ReactNode } from "react";
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
  const [authOpen, setAuthOpen] = useState(false);

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
            aria-label="Open account"
            title="Account"
            onClick={() => setAuthOpen(true)}
          >
            <AccountIcon label="Account" />
          </button>
        </div>
      </header>
      {authOpen && <AuthModal onClose={() => setAuthOpen(false)} />}
    </>
  );
}
