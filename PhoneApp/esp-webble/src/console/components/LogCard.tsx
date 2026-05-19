import type { Ref } from "react";

type LogCardProps = {
  log: string[];
  logBoxRef: Ref<HTMLDivElement>;
};

export function LogCard({ log, logBoxRef }: LogCardProps) {
  return (
    <div className="card">
      <h3>Results from ESP</h3>
      <div ref={logBoxRef} className="log">
        {log.length ? log.join("\n") : "- No messages yet -"}
      </div>
    </div>
  );
}
