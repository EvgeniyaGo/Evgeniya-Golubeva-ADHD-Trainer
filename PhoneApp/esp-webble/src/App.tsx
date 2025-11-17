import { useCallback, useEffect, useRef, useState } from "react";

// --- NUS UUIDs (ESP32 Nordic UART Service) ---
const NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // write / writeWithoutResponse
const NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // notify

// Commands your firmware should understand (text lines keep things easy)
const CMD_START = "START\n";
const CMD_STOP = "STOP\n";

type GattStuff = {
  device: BluetoothDevice;
  server: BluetoothRemoteGATTServer;
  service: BluetoothRemoteGATTService;
  rx: BluetoothRemoteGATTCharacteristic; // write here
  tx: BluetoothRemoteGATTCharacteristic; // notifications from ESP
};

export default function App() {
  // ─── Connection / cube control state ──────────────────────────────
  const [status, setStatus] = useState<
    "idle" | "connecting" | "connected" | "disconnected"
  >("idle");
  const [name, setName] = useState<string>("");
  const [moving, setMoving] = useState(false);
  const [log, setLog] = useState<string[]>([]);
  const [menuOpen, setMenuOpen] = useState(true);

  // ─── Packet test state ────────────────────────────────────────────
  const [testRunning, setTestRunning] = useState(false);
  const [nextSeq, setNextSeq] = useState(1);
  const [sentCount, setSentCount] = useState(0);
  const [recvCount, setRecvCount] = useState(0);
  const [avgRttMs, setAvgRttMs] = useState<number | null>(null);

  // ─── Manual command state ─────────────────────────────────────────
  const [command, setCommand] = useState("");

  // refs
  const gattRef = useRef<GattStuff | null>(null);
  const logBoxRef = useRef<HTMLDivElement | null>(null);
  const pendingRef = useRef<Map<number, number>>(new Map()); // seq -> send timestamp
  const writeBusyRef = useRef(false); // NEW: prevent overlapping GATT writes

  const pushLog = useCallback((line: string) => {
    setLog((prev) => {
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
    setTestRunning(false);
    pendingRef.current.clear();
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
        try {
          // Safer decode (respect byteOffset/byteLength)
          const bytes = new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength);
          const text = new TextDecoder().decode(bytes).trimEnd();
          pushLog(`[ESP →] ${text}`);

          // Split just in case multiple lines come in one notification
          text.split(/\r?\n/).forEach((line) => {
            if (!line) return;

            // Handle PONG <seq> for packet test
            if (line.startsWith("PONG ")) {
              const seqStr = line.substring(5).trim();
              const seq = Number(seqStr);
              if (!Number.isNaN(seq)) {
                const sentAt = pendingRef.current.get(seq);
                if (sentAt != null) {
                  const rtt = performance.now() - sentAt;
                  pendingRef.current.delete(seq);

                  setRecvCount((prevRecv) => {
                    const newRecv = prevRecv + 1;
                    setAvgRttMs((prevAvg) =>
                      prevAvg == null
                        ? rtt
                        : prevAvg + (rtt - prevAvg) / newRecv
                    );
                    return newRecv;
                  });
                }
              }
            }
          });
        } catch {
          const hex = Array.from(
            new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength)
          )
            .map((b) => b.toString(16).padStart(2, "0"))
            .join(" ");
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
      setTestRunning(false);
      pendingRef.current.clear();
      if (g?.device.gatt?.connected) g.device.gatt.disconnect();
      gattRef.current = null;
      setStatus("idle");
      pushLog("[BLE] Manual disconnect");
    } catch (e: any) {
      pushLog(`[ERR] ${e?.message || e}`);
    }
  }, [pushLog]);

  // Write helper (handles writeWith/WithoutResponse differences)
  // Returns true if the write actually happened, false on failure / busy
  const writeLine = useCallback(
    async (line: string): Promise<boolean> => {
      const g = gattRef.current;
      if (!g) {
        pushLog("Write failed: not connected");
        return false;
      }

      if (writeBusyRef.current) {
        // Optionally log, but it can get noisy:
        // pushLog("[WARN] Skipping send, GATT busy");
        return false;
      }

      const data = new TextEncoder().encode(line);
      const rx: any = g.rx;

      writeBusyRef.current = true;
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
        return true;
      } catch (e: any) {
        pushLog(`[ERR] ${e?.message || e}`);
        return false;
      } finally {
        writeBusyRef.current = false;
      }
    },
    [pushLog]
  );

  const toggleMovement = useCallback(async () => {
    const next = !moving;
    setMoving(next);
    await writeLine(next ? CMD_START : CMD_STOP);
  }, [moving, writeLine]);

  // ─── Manual command sender ──────────────────────────────────────
  const sendCommand = useCallback(async () => {
    const raw = command.trim();
    if (!raw) return;
    const line = raw.endsWith("\n") ? raw : raw + "\n";
    await writeLine(line);
    setCommand("");
  }, [command, writeLine]);

  // ─── Packet test: send PING <seq>\n periodically while running ───
  useEffect(() => {
    if (!testRunning || status !== "connected") return;

    const intervalMs = 100; // a bit less aggressive; can tune later
    const id = window.setInterval(() => {
      // Optional safety: don't overload if many are in-flight
      if (pendingRef.current.size > 10) {
        return;
      }

      setNextSeq((prevSeq) => {
        const seq = prevSeq;
        const line = `PING ${seq}\n`;

        (async () => {
          const ok = await writeLine(line);
          if (!ok) return; // don't count / track failed writes

          pendingRef.current.set(seq, performance.now());
          setSentCount((c) => c + 1);
        })();

        return seq + 1;
      });
    }, intervalMs);

    return () => window.clearInterval(id);
  }, [testRunning, status, writeLine]);

  // tidy up on unmount
  useEffect(
    () => () => {
      try {
        gattRef.current?.device.removeEventListener(
          "gattserverdisconnected",
          onDisconnected
        );
      } catch {
        // ignore
      }
    },
    [onDisconnected]
  );

  const isConnected = status === "connected";
  const statusText =
    status === "connecting"
      ? "CONNECTING…"
      : isConnected
      ? "CONNECTED"
      : status === "disconnected"
      ? "DISCONNECTED"
      : "NOT CONNECTED";

  // helper: restart ping both on app + ESP
  const restartPingAll = useCallback(() => {
    // reset app-side stats
    setSentCount(0);
    setRecvCount(0);
    setAvgRttMs(null);
    pendingRef.current.clear();
    setNextSeq(1);
    // ask ESP to reset its counters
    if (isConnected) {
      void writeLine("RESTART PING\n");
    }
  }, [isConnected, writeLine]);

  // ─── UI ────────────────────────────────────────────────────────────
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
              <path
                d="M3 7h18M3 12h18M3 17h18"
                stroke="#697586"
                strokeWidth="2"
                strokeLinecap="round"
              />
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
                {statusText}
                {name && isConnected ? ` — ${name}` : ""}
              </span>
            </div>

            <button
              className="menu-toggle top-icon account-btn"
              aria-label="Account"
              title="Account"
            >
              <svg width="26" height="26" viewBox="0 0 24 24" fill="none">
                <circle
                  cx="12"
                  cy="8"
                  r="4"
                  stroke="#697586"
                  strokeWidth="2"
                />
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
                    <path
                      d="M3 13h8V3H3v10zm10 8h8v-6h-8v6zM3 21h8v-6H3v6zm10-8h8V3h-8v10z"
                      fill="#697586"
                    />
                  </svg>
                )}
              </button>

              <button className="nav-item" title="Connect">
                {menuOpen ? (
                  "Connect"
                ) : (
                  <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                    <path
                      d="M7 8h5a4 4 0 110 8H7"
                      stroke="#697586"
                      strokeWidth="2"
                      strokeLinecap="round"
                    />
                    <path
                      d="M7 12h6"
                      stroke="#697586"
                      strokeWidth="2"
                      strokeLinecap="round"
                    />
                  </svg>
                )}
              </button>

              <button className="nav-item" title="LED Control">
                {menuOpen ? (
                  "LED Control"
                ) : (
                  <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                    <circle
                      cx="12"
                      cy="12"
                      r="5"
                      stroke="#697586"
                      strokeWidth="2"
                    />
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
                    <path
                      d="M12 15.5a3.5 3.5 0 100-7 3.5 3.5 0 000 7z"
                      stroke="#697586"
                      strokeWidth="2"
                    />
                    <path
                      d="M19.4 15a7.97 7.97 0 00.1-2l2-1-2-3-2 1a8.14 8.14 0 00-1.7-1l-.3-2.3h-4l-.3-2.3a8.14 8.14 0 00-1.7 1l-2-1-2 3 2 1a8.5 8.5 0 000 2l-2 1 2 3 2-1c.5.4 1.1.7 1.7 1l.3 2.3h4l.3-2.3c.6-.3 1.2-.6 1.7-1l2 1 2-3-2-1z"
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
                    <circle
                      cx="12"
                      cy="12"
                      r="10"
                      stroke="#697586"
                      strokeWidth="2"
                    />
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
                {statusText}
                {name && isConnected ? ` — ${name}` : ""}
              </div>

              <div className="lead">
                Connect to your cube, then start or stop LED movement. Live
                results, packet test, and manual commands appear below.
              </div>

              <div className="actions subtle">
                <button
                  className="btn btn-neutral"
                  onClick={toggleMovement}
                  disabled={!isConnected}
                >
                  {moving ? "Stop LED movement" : "Start LED movement"}
                </button>
              </div>

              <div className="card">
                <h3>Results from ESP</h3>
                <div ref={logBoxRef} className="log">
                  {log.length ? log.join("\n") : "— No messages yet —"}
                </div>
              </div>

              <div className="card">
                <h3>Packet test</h3>
                <div className="actions subtle">
                  <button
                    className="btn btn-neutral"
                    disabled={!isConnected}
                    onClick={() => {
                      if (!testRunning) {
                        // reset stats on start
                        setSentCount(0);
                        setRecvCount(0);
                        setAvgRttMs(null);
                        pendingRef.current.clear();
                        setNextSeq(1);
                      }
                      setTestRunning((r) => !r);
                    }}
                  >
                    {testRunning ? "Stop test" : "Start packet test"}
                  </button>
                </div>

                <div className="metrics">
                  <div>Total sent: {sentCount}</div>
                  <div>Total received: {recvCount}</div>
                  <div>
                    Lost: {sentCount - recvCount}{" "}
                    {sentCount > 0 && (
                      <>
                        (
                        {(
                          ((sentCount - recvCount) / sentCount) *
                          100
                        ).toFixed(2)}
                        %)
                      </>
                    )}
                  </div>
                  <div>
                    Avg RTT:{" "}
                    {avgRttMs != null ? `${avgRttMs.toFixed(1)} ms` : "—"}
                  </div>
                  <div>In-flight: {pendingRef.current.size}</div>
                </div>

                <div className="actions subtle" style={{ marginTop: "0.75rem" }}>
                  <button
                    className="btn btn-neutral"
                    disabled={!isConnected}
                    onClick={restartPingAll}
                  >
                    Restart ping (ESP + App)
                  </button>
                </div>
              </div>

              <div className="card">
                <h3>Manual command</h3>
                <p className="muted">
                  Send raw commands like <code>SET 0 1</code>,{" "}
                  <code>SET 3 0</code>, <code>RESTART PING</code>, etc.
                </p>
                <div className="command-row">
                  <input
                    type="text"
                    className="command-input"
                    placeholder='e.g. SET 0 1 or RESTART PING'
                    value={command}
                    onChange={(e) => setCommand(e.target.value)}
                    disabled={!isConnected}
                    onKeyDown={(e) => {
                      if (e.key === "Enter") {
                        e.preventDefault();
                        void sendCommand();
                      }
                    }}
                  />
                  <button
                    className="btn btn-neutral"
                    onClick={sendCommand}
                    disabled={!isConnected || !command.trim()}
                  >
                    Send
                  </button>
                </div>
              </div>
            </div>
          </main>
        </div>
      </div>
    </div>
  );
}
