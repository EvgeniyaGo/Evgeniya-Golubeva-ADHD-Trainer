import { useBLE } from "../state/BLEContext";

export default function Home(){
  const { activeMode, elapsedMs } = useBLE();
  return (
    <div className="layered">
      <div className="bg" />
      <section className="card">
        <h2 style={{ marginTop: 0 }}>Welcome</h2>
        <p>Connect your Cube, pick a mode, and start a playful training session.</p>
        <div style={{display:"flex",gap:8,marginTop:12}}>
          <a className="btn primary" href="/connect">Connect Cube</a>
          <a className="btn" href="/modes">Choose Mode</a>
        </div>
      </section>

      <section className="card">
        <h3 style={{ marginTop: 0 }}>Current status</h3>
        {activeMode ? (
          <p>Session running: <b>{activeMode}</b> • Elapsed: <b>{Math.floor(elapsedMs/1000)}s</b></p>
        ) : (
          <p>No session in progress.</p>
        )}
      </section>
    </div>
  );
}
