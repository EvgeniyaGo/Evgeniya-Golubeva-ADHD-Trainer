import { Outlet, NavLink, useNavigate, useLocation } from "react-router-dom";
import BLEChip from "./BLEChip";
import { useBLE } from "../state/BLEContext";

function fmt(ms: number) {
  const s = Math.floor(ms / 1000);
  const m = Math.floor(s / 60);
  const r = s % 60;
  return `${String(m).padStart(2,"0")}:${String(r).padStart(2,"0")}`;
}

export default function AppShell() {
  const { activeMode, elapsedMs, endSession } = useBLE();
  const nav = useNavigate();
  const loc = useLocation();
  const inSession = !!activeMode;

  const onEnd = async () => { await endSession(); nav("/"); };

  return (
    <div className="sh-app">
      <header className="sh-topbar">
        <div className="brand" style={{ display:"flex", alignItems:"center", gap:10 }}>
          <span style={{ fontWeight: 800 }}>ADHD Cube</span>
        </div>
        <input className="top-search" placeholder="Search…" />
        <div className="top-actions">
          <BLEChip />
          <button className="avatar" title="Account">JG</button>
        </div>
      </header>

      <aside className="sh-sidenav">
        <NavLink to="/" end>Home</NavLink>
        <NavLink to="/connect">Connect</NavLink>
        <NavLink to="/modes">Modes</NavLink>
        <NavLink to="/session" className={({ isActive }) => (isActive || loc.pathname === "/session" ? "active" : "")}>
          Session
        </NavLink>
        <NavLink to="/sessions">Sessions</NavLink>
        <NavLink to="/insights">Insights</NavLink>
        <NavLink to="/profiles">Profiles</NavLink>
        <NavLink to="/settings">Settings</NavLink>
        <NavLink to="/help">Help</NavLink>
      </aside>

      <aside className="sh-rail">
        <section className="card">
          <h3 style={{ marginTop: 0 }}>Quick Actions</h3>
          <div style={{ display: "grid", gap: 8 }}>
            <a className="btn primary" href="/connect">Connect Cube</a>
            <a className="btn" href="/modes">Choose Mode</a>
          </div>
        </section>
      </aside>

      <main className="sh-main">
        <Outlet />
      </main>

      <footer className="sh-dock">
        <div className="dock-left">
          <span className={`pill ${inSession ? "success" : ""}`}>{inSession ? "Active" : "Idle"}</span>
          <span className="dock-title">{inSession ? `${activeMode!.toUpperCase()} Mode` : "No session"}</span>
        </div>
        <div className="dock-center">
          <span className="dock-timer">{inSession ? fmt(elapsedMs) : "--:--"}</span>
        </div>
        <div className="dock-right">
          <button className="btn ghost" onClick={() => nav("/session")} disabled={!inSession}>Open Session</button>
          <button className="btn danger" onClick={onEnd} disabled={!inSession}>End Session</button>
        </div>
      </footer>
    </div>
  );
}
