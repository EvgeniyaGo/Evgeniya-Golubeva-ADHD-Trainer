type StatusChipProps = {
  label: string;
  connected?: boolean;
};

export function StatusChip({ label, connected = false }: StatusChipProps) {
  return (
    <span className={`app-status-chip ${connected ? "is-connected" : ""}`}>
      <span className="app-status-chip__dot" />
      {label}
    </span>
  );
}
