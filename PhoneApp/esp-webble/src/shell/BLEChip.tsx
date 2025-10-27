import { useBLE } from "../state/BLEContext";

export default function BLEChip() {
  const { status, deviceName, connect, disconnect } = useBLE();
  const color =
    status === "connected" ? "#16a34a" :
    status === "connecting" ? "#f59e0b" :
    status === "disconnected" ? "#ef4444" : "#94a3b8";

  return (
    <button
      className="btn"
      onClick={() => (status === "connected" ? disconnect() : connect())}
      title={status === "connected" ? "Disconnect" : "Connect"}
      style={{ display: "flex", alignItems: "center", gap: 8 }}
    >
      <span style={{ width:10, height:10, borderRadius:999, background: color,
        boxShadow: status === "connecting" ? "0 0 0 6px rgba(245,158,11,0.15)" : "none" }} />
      {status === "connected" ? (deviceName || "Connected") : status}
    </button>
  );
}
