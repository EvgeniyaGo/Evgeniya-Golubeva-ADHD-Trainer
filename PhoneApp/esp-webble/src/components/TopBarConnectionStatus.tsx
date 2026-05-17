type TopBarConnectionStatusProps = {
  isConnected: boolean;
  statusText: string;
  name?: string;
  onToggleConnection?: () => void;
};

export function TopBarConnectionStatus({
  isConnected,
  statusText,
  name = "",
  onToggleConnection,
}: TopBarConnectionStatusProps) {
  return (
    <div
      className="status-chip"
      role="button"
      tabIndex={0}
      onClick={onToggleConnection}
      onKeyDown={(e) => {
        if ((e.key === "Enter" || e.key === " ") && onToggleConnection) {
          e.preventDefault();
          onToggleConnection();
        }
      }}
      title={isConnected ? "Disconnect" : "Connect"}
    >
      <span className={`dot ${isConnected ? "on" : ""}`} />
      <span className="status-text">
        {statusText}
        {name && isConnected ? ` - ${name}` : ""}
      </span>
    </div>
  );
}
