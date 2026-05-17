import { AccountIcon, HomeIcon } from "../navIcons";
import { SidebarNavItem, type SidebarNavItemConfig } from "./SidebarNavItem";

export type ActivePage = "home" | "account";

type SidebarNavProps = {
  title?: string;
  menuOpen: boolean;
  activePage?: ActivePage;
};

function createNavItems(activePage?: ActivePage): SidebarNavItemConfig[] {
  return [
    {
      label: "Home",
      icon: <HomeIcon label="Home" />,
      to: "/main",
      active: activePage === "home",
    },
    {
      label: "Account",
      icon: <AccountIcon label="Account" />,
      to: "/account",
      active: activePage === "account",
    },
  ];
}

export function SidebarNav({
  title = "Menu",
  menuOpen,
  activePage,
}: SidebarNavProps) {
  return (
    <aside className={`sidebar ${menuOpen ? "open" : "closed"}`}>
      <div className="sidebar-head">{title}</div>
      <nav className="nav" aria-label="Main navigation">
        {createNavItems(activePage).map((item) => (
          <SidebarNavItem key={item.label} menuOpen={menuOpen} {...item} />
        ))}
      </nav>
    </aside>
  );
}
