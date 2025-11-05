import { useCallback, useEffect, useRef, useState } from "react";

// --- NUS UUIDs (ESP32 Nordic UART Service) ---
const NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_RX      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // write / writeWithoutResponse
const NUS_TX      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // notify

// Commands your firmware should understand (text lines keep things easy)
const CMD_START = "START\n";
const CMD_STOP  = "STOP\n";

type GattStuff = {
  device: BluetoothDevice;
  server: BluetoothRemoteGATTServer;
  service: BluetoothRemoteGATTService;
  rx: BluetoothRemoteGATTCharacteristic; // write here
  tx: BluetoothRemoteGATTCharacteristic; // notifications from ESP
};

export default function App() {
  const [status, setStatus] = useState<"idle"|"connecting"|"connected"|"disconnected">("idle");
  const [name, setName] = useState<string>("");
  const [moving, setMoving] = useState(false);
  const [log, setLog] = useState<string[]>([]);
  const gattRef = useRef<GattStuff | null>(null);
  const logBoxRef = useRef<HTMLDivElement | null>(null);

  const pushLog = useCallback((line: string) => {
    setLog(prev => {
      const next = [...prev, `[${new Date().toLocaleTimeString()}] ${line}`];
      return next.slice(-500);
    });
    queueMicrotask(() => {
      const el = logBoxRef.current;
      if (el) el.scrollTop = el.scrollHeight;
    });
  }, []);

  const onDisconnected = useCallback(() => {
    setStatus("disconnected");
    setMoving(false);
    pushLog("[BLE] Disconnected");
    gattRef.current = null;
  }, [pushLog]);

  const connect = useCallback(async () => {
    try {
      if (!navigator.bluetooth) {
        alert("Web Bluetooth not supported in this browser.");
        return;
      }
      setStatus("connecting");

      const device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [NUS_SERVICE] }],
        optionalServices: [NUS_SERVICE],
      });

      setName(device.name || "(no name)");
      device.addEventListener("gattserverdisconnected", onDisconnected);

      const server = await device.gatt!.connect();
      const service = await server.getPrimaryService(NUS_SERVICE);
      const tx = await service.getCharacteristic(NUS_TX);
      const rx = await service.getCharacteristic(NUS_RX);

      // Subscribe to notifications from ESP (NUS TX)
      await tx.startNotifications();
      tx.addEventListener("characteristicvaluechanged", (ev: Event) => {
        const dv = (ev.target as BluetoothRemoteGATTCharacteristic).value!;
        // try UTF-8 text; fall back to hex
        try {
          const text = new TextDecoder().decode(dv.buffer).trimEnd();
          pushLog(`[ESP →] ${text}`);
        } catch {
          const hex = Array.from(new Uint8Array(dv.buffer)).map(b => b.toString(16).padStart(2,"0")).join(" ");
          pushLog(`[ESP →] [${hex}]`);
        }
      });

      gattRef.current = { device, server, service, rx, tx };
      setStatus("connected");
      pushLog(`[BLE] Connected to ${device.name || "(no name)"}`);
    } catch (e: any) {
      setStatus("idle");
      pushLog(`[ERR] ${e?.message || e}`);
    }
  }, [pushLog, onDisconnected]);

  const disconnect = useCallback(async () => {
    try {
      const g = gattRef.current;
      setMoving(false);
      if (g?.device.gatt?.connected) g.device.gatt.disconnect();
      gattRef.current = null;
      setStatus("idle");
      pushLog("[BLE] Manual disconnect");
    } catch (e: any) {
      pushLog(`[ERR] ${e?.message || e}`);
    }
  }, [pushLog]);

  // Write helper (handles writeWith/WithoutResponse differences)
  const writeLine = useCallback(async (line: string) => {
    const g = gattRef.current;
    if (!g) return pushLog("Write failed: not connected");
    const data = new TextEncoder().encode(line);
    const rx: any = g.rx;
    try {
      if (typeof rx.writeValueWithoutResponse === "function") {
        await rx.writeValueWithoutResponse(data);
      } else if (typeof rx.writeValueWithResponse === "function") {
        await rx.writeValueWithResponse(data);
      } else if (typeof rx.writeValue === "function") {
        await rx.writeValue(data);
      } else {
        throw new Error("Characteristic is not writable.");
      }
      pushLog(`[→ ESP] ${JSON.stringify(line.trimEnd())}`);
    } catch (e: any) {
      pushLog(`[ERR] ${e?.message || e}`);
    }
  }, [pushLog]);

  const toggleMovement = useCallback(async () => {
    const next = !moving;
    setMoving(next);
    await writeLine(next ? CMD_START : CMD_STOP);
  }, [moving, writeLine]);

  // tidy up
  useEffect(() => () => {
    try { gattRef.current?.device.removeEventListener("gattserverdisconnected", onDisconnected); } catch {}
  }, [onDisconnected]);
  const isConnected = status === "connected";
  const statusText =
    status === "connecting" ? "CONNECTING…" :
    isConnected ? "CONNECTED" :
    status === "disconnected" ? "DISCONNECTED" : "NOT CONNECTED";

    const [menuOpen, setMenuOpen] = useState(true);          // ← right menu state (default open)

return (
  <div className={`app-frame ${menuOpen ? "menu-open" : "menu-closed"}`}>
    <div className="board">
      {/* ─── TOP MENU BAR ─────────────────────────────── */}
      <header className="board-header">
        {/* LEFT: menu icon */}
        <button
          className="menu-toggle top-icon"
          aria-pressed={menuOpen}
          aria-label={menuOpen ? "Collapse menu" : "Expand menu"}
          title={menuOpen ? "Collapse menu" : "Expand menu"}
          onClick={() => setMenuOpen((m) => !m)}
        >
          <svg width="26" height="26" viewBox="0 0 24 24" fill="none">
            <path d="M3 7h18M3 12h18M3 17h18" stroke="#697586" strokeWidth="2" strokeLinecap="round" />
          </svg>
        </button>

        {/* RIGHT: status chip + account icon */}
        <div className="top-right">
          <div
            className="status-chip"
            role="button"
            tabIndex={0}
            onClick={() => (isConnected ? disconnect() : connect())}
            onKeyDown={(e) => {
              if (e.key === "Enter" || e.key === " ") {
                e.preventDefault();
                isConnected ? disconnect() : connect();
              }
            }}
            title={isConnected ? "Disconnect" : "Connect"}
          >
            <span className={`dot ${isConnected ? "on" : ""}`} />
            <span className="status-text">
              {status === "connected"
                ? "CONNECTED"
                : status === "connecting"
                ? "CONNECTING…"
                : "NOT CONNECTED"}
              {name && isConnected ? ` — ${name}` : ""}
            </span>
          </div>

          <button className="menu-toggle top-icon account-btn" aria-label="Account" title="Account">
            <svg width="26" height="26" viewBox="0 0 24 24" fill="none">
              <circle cx="12" cy="8" r="4" stroke="#697586" strokeWidth="2" />
              <path
                d="M4 20c1.8-3.2 4.7-5 8-5s6.2 1.8 8 5"
                stroke="#697586"
                strokeWidth="2"
                strokeLinecap="round"
              />
            </svg>
          </button>
        </div>
      </header>
    {/* Backdrop for mobile overlay menu */}
    <div
      className={`backdrop ${menuOpen ? "show" : ""}`}
      onClick={() => setMenuOpen(false)}
      aria-hidden="true"
    />

      {/* ─── BODY: sidebar (left) + main content ───────── */}
      <div className="board-shell">
        <aside className={`sidebar ${menuOpen ? "open" : "closed"}`}>
          <div className="sidebar-head">Menu</div>
          <nav className="nav">
            <button className="nav-item is-active" title="Dashboard">
              {menuOpen ? (
                "Dashboard"
              ) : (
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                  <path d="M3 13h8V3H3v10zm10 8h8v-6h-8v6zM3 21h8v-6H3v6zm10-8h8V3h-8v10z" fill="#697586" />
                </svg>
              )}
            </button>

            <button className="nav-item" title="Connect">
              {menuOpen ? (
                "Connect"
              ) : (
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                  <path d="M7 8h5a4 4 0 110 8H7" stroke="#697586" strokeWidth="2" strokeLinecap="round" />
                  <path d="M7 12h6" stroke="#697586" strokeWidth="2" strokeLinecap="round" />
                </svg>
              )}
            </button>

            <button className="nav-item" title="LED Control">
              {menuOpen ? (
                "LED Control"
              ) : (
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                  <circle cx="12" cy="12" r="5" stroke="#697586" strokeWidth="2" />
                  <path
                    d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M19.1 4.9L17 7M7 17l-2.1 2.1"
                    stroke="#697586"
                    strokeWidth="2"
                    strokeLinecap="round"
                  />
                </svg>
              )}
            </button>

            <button className="nav-item" title="Logs">
              {menuOpen ? (
                "Logs"
              ) : (
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                  <path
                    d="M5 4h14M5 8h14M5 12h10M5 16h8"
                    stroke="#697586"
                    strokeWidth="2"
                    strokeLinecap="round"
                  />
                </svg>
              )}
            </button>

            <button className="nav-item" title="Settings">
              {menuOpen ? (
                "Settings"
              ) : (
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                  <path d="M12 15.5a3.5 3.5 0 100-7 3.5 3.5 0 000 7z" stroke="#697586" strokeWidth="2" />
                  <path
                    d="M19.4 15a7.97 7.97 0 00.1-2l2-1-2-3-2 1a8.14 8.14 0 00-1.7-1l-.3-2.3h-4l-.3 2.3a8.14 8.14 0 00-1.7 1l-2-1-2 3 2 1a8.5 8.5 0 000 2l-2 1 2 3 2-1c.5.4 1.1.7 1.7 1l.3 2.3h4l.3-2.3c.6-.3 1.2-.6 1.7-1l2 1 2-3-2-1z"
                    stroke="#697586"
                    strokeWidth="2"
                    strokeLinecap="round"
                    strokeLinejoin="round"
                  />
                </svg>
              )}
            </button>

            <div className="divider" />

            <button className="nav-item" title="About">
              {menuOpen ? (
                "About"
              ) : (
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                  <circle cx="12" cy="12" r="10" stroke="#697586" strokeWidth="2" />
                  <path
                    d="M12 8h.01M11 12h2v5h-2z"
                    stroke="#697586"
                    strokeWidth="2"
                    strokeLinecap="round"
                  />
                </svg>
              )}
            </button>
          </nav>
        </aside>

        {/* ─── MAIN CONTENT ─────────────────────────────── */}
        <main className="board-content">
          <div className="stack center">
            <div className="status">
              {status === "connected"
                ? "CONNECTED"
                : status === "connecting"
                ? "CONNECTING…"
                : "NOT CONNECTED"}
              {name && isConnected ? ` — ${name}` : ""}
            </div>

            <div className="lead">
              Connect to your cube, then start or stop LED movement. Live results appear below.
            </div>

            <div className="actions subtle">
              <button className="btn btn-neutral" onClick={toggleMovement} disabled={!isConnected}>
                {moving ? "Stop LED movement" : "Start LED movement"}
              </button>
            </div>

            <div className="card">
              <h3>Results from ESP</h3>
              <div ref={logBoxRef} className="log">
                {log.length ? log.join("\n") : "— No messages yet —"}
              </div>
            </div>
          </div>
        </main>
      </div>
    </div>
  </div>
);
}