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
  const isInteractive = Boolean(onToggleConnection);

  return (
    <div
      className={`status-chip${isInteractive ? " is-interactive" : ""}`}
      role={isInteractive ? "button" : "status"}
      tabIndex={isInteractive ? 0 : undefined}
      onClick={onToggleConnection}
      onKeyDown={(e) => {
        if ((e.key === "Enter" || e.key === " ") && onToggleConnection) {
          e.preventDefault();
          onToggleConnection();
        }
      }}
      title={isInteractive ? (isConnected ? "Disconnect" : "Connect") : undefined}
    >
      <span className={`dot ${isConnected ? "on" : ""}`} />
      <span className="status-text">
        {statusText}
        {name && isConnected ? ` - ${name}` : ""}
      </span>
    </div>
  );
}
