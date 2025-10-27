import { useBLE } from "../state/BLEContext";

export default function Connect(){
  const { status, deviceName, connect, disconnect, logs } = useBLE();
  return (
    <>
      <section className="card">
        <h2 style={{ marginTop: 0 }}>Connect your Cube</h2>
        <p>Status: <b>{status}</b> {deviceName && status !== "idle" ? <> • Device: <b>{deviceName}</b></> : null}</p>
        <div style={{ display: "flex", gap: 8, marginTop: 8 }}>
          <button className="btn primary" onClick={connect} disabled={status === "connecting" || status === "connected"}>
            {status === "connecting" ? "Connecting…" : (status === "connected" ? "Connected" : "Connect")}
          </button>
          <button className="btn" onClick={disconnect} disabled={status === "idle"}>Disconnect</button>
        </div>
        <ul style={{ marginTop: 12, color: "#64748b" }}>
          <li>Use Chrome/Edge on Android or Desktop. Requires HTTPS or localhost.</li>
          <li>Click “Connect”, then pick your ESP (may show as “Unknown”).</li>
          <li>Keep this tab foregrounded; web reconnects are limited.</li>
        </ul>
      </section>

      <section className="card">
        <h3 style={{ marginTop: 0 }}>Log</h3>
        <pre className="log">{logs.join("\n")}</pre>
      </section>
    </>
  );
}
