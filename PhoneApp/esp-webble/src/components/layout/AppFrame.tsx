import { useState, type ReactNode } from "react";
import { BoardHeader } from "./BoardHeader";
import { SidebarNav, type ActivePage } from "./SidebarNav";

type AppFrameProps = {
  children: ReactNode;
  activePage?: ActivePage;
  topBarRight?: ReactNode;
  ariaLabel?: string;
  sidebarTitle?: string;
  initialMenuOpen?: boolean;
};

export function AppFrame({
  children,
  activePage,
  topBarRight,
  ariaLabel = "App screen",
  sidebarTitle = "Menu",
  initialMenuOpen = true,
}: AppFrameProps) {
  const [menuOpen, setMenuOpen] = useState(initialMenuOpen);

  return (
    <div className={`app-frame ${menuOpen ? "menu-open" : "menu-closed"}`}>
      <div className="board" aria-label={ariaLabel}>
        <BoardHeader
          menuOpen={menuOpen}
          rightContent={topBarRight}
          onToggleMenu={() => setMenuOpen((open) => !open)}
        />
        <div className="board-shell">
          <SidebarNav
            title={sidebarTitle}
            menuOpen={menuOpen}
            activePage={activePage}
          />
          <main className="board-content">{children}</main>
        </div>
      </div>
    </div>
  );
}
