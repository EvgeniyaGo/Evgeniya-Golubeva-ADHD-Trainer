type PacketTestCardProps = {
  sentCount: number;
  recvCount: number;
  avgRttMs: number | null;
  inFlightCount: number;
  isConnected: boolean;
  testRunning: boolean;
  onRestartPing: () => void;
  onToggleTest: () => void;
};

export function PacketTestCard({
  sentCount,
  recvCount,
  avgRttMs,
  inFlightCount,
  isConnected,
  testRunning,
  onRestartPing,
  onToggleTest,
}: PacketTestCardProps) {
  return (
    <div className="card">
      <h3>Packet test</h3>

      <div className="metrics">
        <div>Total sent: {sentCount}</div>
        <div>Total received: {recvCount}</div>
        <div>
          Lost: {sentCount - recvCount}{" "}
          {sentCount > 0 && (
            <>
              ({(((sentCount - recvCount) / sentCount) * 100).toFixed(2)}%)
            </>
          )}
        </div>
        <div>
          Avg RTT: {avgRttMs != null ? `${avgRttMs.toFixed(1)} ms` : "-"}
        </div>
        <div>In-flight: {inFlightCount}</div>
      </div>

      <div className="actions subtle" style={{ marginTop: "0.75rem" }}>
        <button
          className="btn btn-neutral"
          disabled={!isConnected}
          onClick={onRestartPing}
        >
          Restart ping (ESP + App)
        </button>
      </div>
      <div className="actions subtle">
        <button
          className="btn btn-neutral"
          disabled={!isConnected}
          onClick={onToggleTest}
        >
          {testRunning ? "Stop test" : "Start packet test"}
        </button>
      </div>
    </div>
  );
}
