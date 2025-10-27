import { useState } from "react";
import { useBLE } from "../state/BLEContext";

function fmt(ms: number) {
  const s = Math.floor(ms / 1000);
  const m = Math.floor(s / 60);
  const r = s % 60;
  return `${String(m).padStart(2,"0")}:${String(r).padStart(2,"0")}`;
}

export default function Session(){
  const { activeMode, elapsedMs, endSession } = useBLE();
  const [confirm, setConfirm] = useState(false);

  return (
    <section className="card">
      <div style={{ display:"flex", alignItems:"center", justifyContent:"space-between" }}>
        <h2 style={{ marginTop: 0 }}>{activeMode ? `${activeMode.toUpperCase()} Mode` : "No active session"}</h2>
        <button className="btn" onClick={() => setConfirm(true)} disabled={!activeMode}>Exit</button>
      </div>

      <div className="timerWrap">
        <div className="timerCircle">
          <div className="timerText">{fmt(elapsedMs)}</div>
        </div>
      </div>

      <p className="subtle">Stopwatch counts elapsed time. Ending will send <code>END_GAME</code> to the ESP.</p>

      {confirm && (
        <div className="modalBackdrop" onClick={() => setConfirm(false)}>
          <div className="modal" onClick={(e) => e.stopPropagation()}>
            <h3 style={{ margin: 0 }}>End session?</h3>
            <p style={{ marginTop: 8 }}>Are you sure you want to end the current session ({fmt(elapsedMs)})?</p>
            <div style={{ display:"flex", gap:8, justifyContent:"flex-end", marginTop:12 }}>
              <button className="btn" onClick={() => setConfirm(false)}>Cancel</button>
              <button className="btn danger" onClick={async () => { await endSession(); setConfirm(false); }}>End & Send</button>
            </div>
          </div>
        </div>
      )}
    </section>
  );
}
