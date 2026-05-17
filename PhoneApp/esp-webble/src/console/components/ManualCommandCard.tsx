type ManualCommandCardProps = {
  command: string;
  isConnected: boolean;
  onCommandChange: (command: string) => void;
  onSendCommand: () => void;
};

export function ManualCommandCard({
  command,
  isConnected,
  onCommandChange,
  onSendCommand,
}: ManualCommandCardProps) {
  return (
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
          placeholder='e.g. GAME START ...'
          value={command}
          onChange={(e) => onCommandChange(e.target.value)}
          disabled={!isConnected}
          onKeyDown={(e) => {
            if (e.key === "Enter") {
              e.preventDefault();
              onSendCommand();
            }
          }}
        />
        <button
          className="btn btn-neutral"
          onClick={onSendCommand}
          disabled={!isConnected || !command.trim()}
        >
          Send
        </button>
      </div>
    </div>
  );
}
