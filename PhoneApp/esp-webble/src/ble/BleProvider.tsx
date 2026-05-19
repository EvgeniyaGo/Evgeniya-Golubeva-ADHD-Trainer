import {
  createContext,
  useCallback,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";
import {
  NUS_RX,
  NUS_SERVICE,
  NUS_TX,
} from "../console/protocol/constants";
import type {
  BleConnectionStatus,
  BleContextValue,
  BleGattConnection,
} from "./types";

export const BleContext = createContext<BleContextValue | null>(null);

type BleProviderProps = {
  children: ReactNode;
};

export function BleProvider({ children }: BleProviderProps) {
  const [status, setStatus] = useState<BleConnectionStatus>("idle");
  const [deviceName, setDeviceName] = useState("");
  const [gatt, setGatt] = useState<BleGattConnection | null>(null);
  const gattRef = useRef<BleGattConnection | null>(null);

  const onDisconnected = useCallback(() => {
    setStatus("disconnected");
    setGatt(null);
    gattRef.current = null;
  }, []);

  const connect = useCallback(async () => {
    try {
      if (!navigator.bluetooth) {
        alert("Web Bluetooth not supported in this browser.");
        return;
      }

      setStatus("connecting");

      const device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: "ADHD" }],
        optionalServices: [NUS_SERVICE],
      });

      setDeviceName(device.name || "(no name)");
      device.addEventListener("gattserverdisconnected", onDisconnected);

      const server = await device.gatt!.connect();
      const service = await server.getPrimaryService(NUS_SERVICE);
      const tx = await service.getCharacteristic(NUS_TX);
      const rx = await service.getCharacteristic(NUS_RX);
      const nextGatt = { device, server, service, rx, tx };

      gattRef.current = nextGatt;
      setGatt(nextGatt);
      setStatus("connected");
    } catch {
      setStatus("idle");
      setGatt(null);
      gattRef.current = null;
    }
  }, [onDisconnected]);

  const disconnect = useCallback(async () => {
    const currentGatt = gattRef.current;

    try {
      if (currentGatt?.device.gatt?.connected) {
        currentGatt.device.gatt.disconnect();
      }
    } finally {
      if (currentGatt) {
        currentGatt.device.removeEventListener(
          "gattserverdisconnected",
          onDisconnected
        );
      }

      setStatus("idle");
      setGatt(null);
      gattRef.current = null;
    }
  }, [onDisconnected]);

  const isConnected = status === "connected";
  const statusText =
    status === "connecting"
      ? "CONNECTING..."
      : isConnected
        ? "CONNECTED"
        : status === "disconnected"
          ? "DISCONNECTED"
          : "NOT CONNECTED";

  const toggleConnection = useCallback(async () => {
    if (isConnected) {
      await disconnect();
      return;
    }

    await connect();
  }, [connect, disconnect, isConnected]);

  const value = useMemo<BleContextValue>(
    () => ({
      status,
      statusText,
      isConnected,
      deviceName,
      gatt,
      connect,
      disconnect,
      toggleConnection,
    }),
    [
      connect,
      deviceName,
      disconnect,
      gatt,
      isConnected,
      status,
      statusText,
      toggleConnection,
    ]
  );

  return <BleContext.Provider value={value}>{children}</BleContext.Provider>;
}
