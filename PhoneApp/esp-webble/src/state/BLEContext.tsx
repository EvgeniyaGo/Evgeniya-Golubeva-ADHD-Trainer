import React, { createContext, useCallback, useContext, useEffect, useRef, useState } from "react";

const NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_RX      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_TX      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

type GattStuff = {
  device: BluetoothDevice;
  server: BluetoothRemoteGATTServer;
  service: BluetoothRemoteGATTService;
  rx: BluetoothRemoteGATTCharacteristic;
  tx: BluetoothRemoteGATTCharacteristic;
};

export type ModeId = "focus" | "memory" | "time";
type BLEStatus = "idle" | "connecting" | "connected" | "disconnected";

type BLEContextType = {
  status: BLEStatus;
  deviceName: string;
  logs: string[];
  connect: () => Promise<void>;
  disconnect: () => Promise<void>;
  sendRaw: (text: string) => Promise<void>;
  sendCommand: (type: string, payload?: Record<string, any>) => Promise<void>;
  activeMode: ModeId | null;
  elapsedMs: number;
  startSession: (mode: ModeId) => Promise<void>;
  endSession: () => Promise<void>;
};

const BLEContext = createContext<BLEContextType | null>(null);
export const useBLE = () => { const c = useContext(BLEContext); if (!c) throw new Error("useBLE must be used within BLEProvider"); return c; };

export const BLEProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [status, setStatus] = useState<BLEStatus>("idle");
  const [deviceName, setDeviceName] = useState("");
  const [logs, setLogs] = useState<string[]>([]);
  const gattRef = useRef<GattStuff | null>(null);
  const bufferRef = useRef<string>("");

  const [activeMode, setActiveMode] = useState<ModeId | null>(null);
  const [elapsedMs, setElapsedMs] = useState(0);
  const t0Ref = useRef<number | null>(null);
  const rafRef = useRef<number | null>(null);

  const log = useCallback((...a: any[]) => setLogs(prev => [...prev.slice(-300), a.join(" ")]), []);

  const onDisconnected = useCallback(() => {
    setStatus("disconnected");
    log("[BLE] Disconnected");
    gattRef.current = null;
    if (rafRef.current) cancelAnimationFrame(rafRef.current);
    rafRef.current = null;
    t0Ref.current = null;
    setActiveMode(null);
    setElapsedMs(0);
  }, [log]);

  const connect = useCallback(async () => {
    try {
      if (!navigator.bluetooth) { alert("Web Bluetooth not supported in this browser."); return; }
      setStatus("connecting");
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [NUS_SERVICE] }],
        optionalServices: [NUS_SERVICE],
      });
      setDeviceName(device.name || "(no name)");
      device.addEventListener("gattserverdisconnected", onDisconnected);

      const server = await device.gatt!.connect();
      const service = await server.getPrimaryService(NUS_SERVICE);
      const tx = await service.getCharacteristic(NUS_TX);
      const rx = await service.getCharacteristic(NUS_RX);
      await tx.startNotifications();
      tx.addEventListener("characteristicvaluechanged", (ev: Event) => {
        const dv = (ev.target as BluetoothRemoteGATTCharacteristic).value!;
        bufferRef.current += new TextDecoder().decode(dv.buffer);
        let idx;
        while ((idx = bufferRef.current.indexOf("\n")) >= 0) {
          const line = bufferRef.current.slice(0, idx).trimEnd();
          bufferRef.current = bufferRef.current.slice(idx + 1);
          if (line) log("[ESP →]", line);
        }
      });

      gattRef.current = { device, server, service, rx, tx };
      setStatus("connected");
      log("[BLE] Connected to", device.name || "(no name)");
    } catch (e: any) {
      setStatus("idle");
      log("[ERR]", e?.message || e);
    }
  }, [log, onDisconnected]);

  const disconnect = useCallback(async () => {
    try {
      const g = gattRef.current;
      if (g?.device.gatt?.connected) g.device.gatt.disconnect();
      gattRef.current = null;
      setStatus("idle");
      log("[BLE] Manual disconnect");
    } catch (e: any) { log("[ERR]", e?.message || e); }
  }, [log]);

  const sendRaw = useCallback(async (text: string) => {
    const g = gattRef.current; if (!g) throw new Error("Not connected");
    const data = new TextEncoder().encode(text);
    const rx: any = g.rx;
    if (typeof rx.writeValueWithoutResponse === "function") await rx.writeValueWithoutResponse(data);
    else if (typeof rx.writeValueWithResponse === "function") await rx.writeValueWithResponse(data);
    else if (typeof rx.writeValue === "function") await rx.writeValue(data);
    else throw new Error("Characteristic is not writable.");
    log("[→ ESP]", JSON.stringify(text.trimEnd()));
  }, [log]);

  const sendCommand = useCallback(async (type: string, payload: Record<string, any> = {}) => {
    const msg = JSON.stringify({ type, ...payload }) + "\n";
    await sendRaw(msg);
  }, [sendRaw]);

  const tick = useCallback((tNow: number) => {
    if (t0Ref.current == null) return;
    setElapsedMs(tNow - t0Ref.current);
    rafRef.current = requestAnimationFrame(tick);
  }, []);

  const startSession = useCallback(async (mode: ModeId) => {
    if (status !== "connected") { alert("Connect to your cube first."); return; }
    await sendCommand("SET_MODE", { mode });
    setActiveMode(mode);
    setElapsedMs(0);
    t0Ref.current = performance.now();
    if (rafRef.current) cancelAnimationFrame(rafRef.current);
    rafRef.current = requestAnimationFrame(tick);
  }, [sendCommand, status, tick]);

  const endSession = useCallback(async () => {
    const mode = activeMode;
    try {
      if (mode) await sendCommand("END_GAME", { mode, durationMs: elapsedMs });
    } catch (e: any) { log("[ERR]", e?.message || e); }
    finally {
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
      rafRef.current = null; t0Ref.current = null;
      setActiveMode(null); setElapsedMs(0);
    }
  }, [activeMode, elapsedMs, sendCommand, log]);

  useEffect(() => () => {
    try { gattRef.current?.device.removeEventListener("gattserverdisconnected", onDisconnected); } catch {}
    if (rafRef.current) cancelAnimationFrame(rafRef.current);
  }, [onDisconnected]);

  return (
    <BLEContext.Provider value={{
      status, deviceName, logs, connect, disconnect, sendRaw, sendCommand,
      activeMode, elapsedMs, startSession, endSession
    }}>
      {children}
    </BLEContext.Provider>
  );
};
