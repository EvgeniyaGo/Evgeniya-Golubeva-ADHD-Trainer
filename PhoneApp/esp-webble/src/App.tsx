import { useCallback, useEffect, useRef, useState } from "react";
                     
const NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_RX      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // Write / WriteWithoutResponse
const NUS_TX      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // Notify

type GattStuff = {
  device: BluetoothDevice;
  server: BluetoothRemoteGATTServer;
  service: BluetoothRemoteGATTService;
  rx: BluetoothRemoteGATTCharacteristic;
  tx: BluetoothRemoteGATTCharacteristic;
};

export default function App() {
  const [logs, setLogs] = useState<string[]>([]);
  const [status, setStatus] = useState<"idle"|"connecting"|"connected"|"disconnected">("idle");
  const [name, setName] = useState<string>("");
  const [outText, setOutText] = useState<string>("hello\n");
  const gattRef = useRef<GattStuff | null>(null);

  const log = useCallback((...a: any[]) => {
    setLogs(prev => [...prev.slice(-300), a.join(" ")]);
  }, []);

  const onDisconnected = useCallback(() => {
    setStatus("disconnected");
    log("[BLE] Disconnected");
    gattRef.current = null;
  }, [log]);

  const connect = useCallback(async () => {
    try {
      if (!navigator.bluetooth) {
        alert("Web Bluetooth not supported in this browser.");
        return;
      }
      setStatus("connecting");
      // You can filter by namePrefix OR by service UUID (more universal)
      const device = await navigator.bluetooth.requestDevice({
        // filters: [{ namePrefix: "Jeni-ESP32" }],
        acceptAllDevices: false,
        filters: [{ services: [NUS_SERVICE] }],
        optionalServices: [NUS_SERVICE],
      });

      setName(device.name || "(no name)");
      device.addEventListener("gattserverdisconnected", onDisconnected);

      const server = await device.gatt!.connect();
      const service = await server.getPrimaryService(NUS_SERVICE);
      const tx = await service.getCharacteristic(NUS_TX);
      const rx = await service.getCharacteristic(NUS_RX);

      // Subscribe to notifications
      await tx.startNotifications();
      tx.addEventListener("characteristicvaluechanged", (ev: Event) => {
        const dv = (ev.target as BluetoothRemoteGATTCharacteristic).value!;
        const text = new TextDecoder().decode(dv.buffer);
        log("[ESP →] ", text.trimEnd());
        // OPTIONAL: forward to backend
        // fetch("/api/telemetry", { method: "POST", headers: { "Content-Type": "application/json" },
        //   body: JSON.stringify({ deviceId: device.id, payload: text }) });
      });

      gattRef.current = { device, server, service, rx, tx };
      setStatus("connected");
      log("[BLE] Connected to", device.name || "(no name)");
    } catch (e: any) {
      setStatus("idle");
      log("[ERR] ", e?.message || e);
    }
  }, [log, onDisconnected]);

  const disconnect = useCallback(async () => {
    try {
      const g = gattRef.current;
      if (g?.device.gatt?.connected) {
        g.device.gatt.disconnect();
      }
      gattRef.current = null;
      setStatus("idle");
      log("[BLE] Manual disconnect");
    } catch (e: any) {
      log("[ERR] ", e?.message || e);
    }
  }, [log]);

  const send = useCallback(async () => {
    try {
      const g = gattRef.current;
      if (!g) throw new Error("Not connected");

      const data = new TextEncoder().encode(outText);

      // normalize across browsers / typings
      const rx: any = g.rx;

      if (typeof rx.writeValueWithoutResponse === "function") {
        await rx.writeValueWithoutResponse(data);
      } else if (typeof rx.writeValueWithResponse === "function") {
        await rx.writeValueWithResponse(data);
      } else if (typeof rx.writeValue === "function") {
        await rx.writeValue(data);
      } else {
        throw new Error("This characteristic is not writable.");
      }

      log("[→ ESP] ", JSON.stringify(outText));
    } catch (e: any) {
      log("[ERR] ", e?.message || e);
    }
  }, [outText, log]);

  // Clean up listener on unmount
  useEffect(() => () => {
    try { gattRef.current?.device.removeEventListener("gattserverdisconnected", onDisconnected); } catch {}
  }, [onDisconnected]);

  return (
    <div style={{ fontFamily: "system-ui, sans-serif", padding: 16, maxWidth: 820, margin: "0 auto" }}>
      <h1>ESP32 ↔ Web Bluetooth (NUS)</h1>
      <p>
        Status: <b>{status}</b> {name && status !== "idle" ? <span> • Device: <b>{name}</b></span> : null}
      </p>

      <div style={{ display: "flex", gap: 8, marginBottom: 12 }}>
        <button onClick={connect} disabled={status === "connecting" || status === "connected"}>
          {status === "connecting" ? "Connecting…" : "Connect"}
        </button>
        <button onClick={disconnect} disabled={status !== "connected" && status !== "disconnected"}>
          Disconnect
        </button>
      </div>

      <div style={{ display: "grid", gap: 8, gridTemplateColumns: "1fr auto" }}>
        <input
          value={outText}
          onChange={(e) => setOutText(e.target.value)}
          placeholder="Type text to send to ESP…"
        />
        <button onClick={send} disabled={status !== "connected"}>Send</button>
      </div>

      <h3 style={{ marginTop: 16 }}>Log</h3>
      <pre style={{ background: "#111", color: "#0f0", padding: 12, minHeight: 240, maxHeight: 360, overflow: "auto" }}>
        {logs.join("\n")}
      </pre>

      <details style={{ marginTop: 12 }}>
        <summary>Troubleshooting</summary>
        <ul>
          <li>Use Chrome/Edge on Android or desktop. Web Bluetooth needs HTTPS or localhost.</li>
          <li>Click “Connect” (user gesture required), then pick your ESP (it may show as “Unknown”).</li>
          <li>Keep the tab in the foreground; background reconnects aren’t supported on the web.</li>
          <li>If you don’t see your device: toggle Bluetooth, ensure phone Location (Android) is ON, and stand close (≤1 m).</li>
        </ul>
      </details>
    </div>
  );
}
