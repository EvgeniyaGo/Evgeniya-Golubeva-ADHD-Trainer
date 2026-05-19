import type { ReactNode } from "react";
import { Link } from "react-router-dom";

export type SidebarNavItemConfig = {
  label: string;
  icon: ReactNode;
  to: string;
  active?: boolean;
  title?: string;
};

type SidebarNavItemProps = SidebarNavItemConfig & {
  menuOpen: boolean;
};

export function SidebarNavItem({
  label,
  icon,
  to,
  active = false,
  title,
  menuOpen,
}: SidebarNavItemProps) {
  return (
    <Link
      className={`nav-item${active ? " is-active" : ""}`}
      to={to}
      title={title ?? label}
      aria-current={active ? "page" : undefined}
    >
      {menuOpen ? (
        label
      ) : (
        <span className="nav-item__icon" aria-hidden="true">
          {icon}
        </span>
      )}
    </Link>
  );
}
