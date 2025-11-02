import React, { useCallback, useEffect, useRef, useState } from "react";

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

  return (
    <div className="app-frame">
      <div className="board">
        <header className="board-header">
          <div className="badge">
            <span className={`dot ${isConnected ? "on" : ""}`} />
            <span className="kicker">CUBE CONTROLLER</span>
          </div>
        </header>

        <main className="board-content">
          <div className="stack">
            <div className="status">{statusText}{name && isConnected ? ` — ${name}` : ""}</div>
            <div className="lead">
              Connect to your cube, then start or stop LED movement. Live results appear below.
            </div>

            <div className="actions">
              {!isConnected ? (
                <button className="btn btn-primary" onClick={connect} disabled={status==="connecting"}>
                  {status === "connecting" ? "Connecting…" : "Connect to ESP"}
                </button>
              ) : (
                <button className="btn btn-neutral" onClick={disconnect}>
                  Disconnect
                </button>
              )}
              <button className="btn btn-accent" onClick={toggleMovement} disabled={!isConnected}>
                {moving ? "Stop LED movement" : "Start LED movement"}
              </button>
            </div>

            <div className="card">
              <h3>Results from ESP</h3>
              <div ref={logBoxRef} className="log">
                {log.length ? log.join("\n") : "— No messages yet —"}
              </div>
            </div>

            <div className="lead" style={{marginTop: 2}}>
              Tip: firmware should interpret <b>START</b> / <b>STOP</b> on NUS RX and send text status over NUS TX.
            </div>
          </div>
        </main>
      </div>
    </div>
  );
}
