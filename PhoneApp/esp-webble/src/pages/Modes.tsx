import { useNavigate } from "react-router-dom";
import { useBLE } from "../state/BLEContext";
import type { ModeId } from "../state/BLEContext";

const MODES: { id: ModeId; title: string; desc: string; hue: number }[] = [
  { id: "focus",  title: "Focus",  desc: "Go/No-Go, impulse control", hue: 160 },
  { id: "memory", title: "Memory", desc: "N-Back, sequences",        hue: 220 },
  { id: "time",   title: "Time",   desc: "Estimation & pacing",       hue: 280 },
];

export default function Modes(){
  const { status, startSession } = useBLE();
  const nav = useNavigate();
  const begin = async (m: ModeId) => { await startSession(m); nav("/session"); };

  return (
    <section className="card">
      <h2 style={{ marginTop: 0 }}>Choose Game Mode</h2>
      <p className="subtle">A command will be sent to the cube to start the session.</p>
      <div style={{ display: "grid", gap: 12, marginTop: 12 }}>
        {MODES.map(m => (
          <button
            key={m.id}
            className="modeBtn"
            onClick={() => begin(m.id)}
            disabled={status !== "connected"}
            style={{ background: `hsl(${m.hue} 80% 95%)`, borderColor: `hsl(${m.hue} 80% 80%)` }}
          >
            <div style={{ fontWeight: 700 }}>{m.title}</div>
            <div className="modeDesc">{m.desc}</div>
          </button>
        ))}
      </div>
      {status !== "connected" && <p className="subtle" style={{ marginTop: 12 }}>Connect first to enable modes.</p>}
    </section>
  );
}
