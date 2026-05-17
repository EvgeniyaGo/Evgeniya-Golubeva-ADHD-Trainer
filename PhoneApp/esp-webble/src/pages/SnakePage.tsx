import { Link } from "react-router-dom";
import { AppFrame } from "../components/layout/AppFrame";

const rules = [
  "Guide the snake across cube faces",
  "Collect apples",
  "Avoid wall, self, and dead-zone collisions",
  "The game ends after collision or manual stop",
];

const metrics = [
  "Survival Time",
  "Final Score",
  "Apples Collected",
  "Average Time Between Apples",
  "Death Type",
];

export default function SnakePage() {
  return (
    <AppFrame ariaLabel="3D Snake setup" activePage="home">
      <div className="game-setup-page">
        <section className="game-setup-hero" aria-labelledby="snake-title">
          <div>
            <span className="section-kicker">Planning + attention</span>
            <h1 id="snake-title" className="game-setup-title">
              3D Snake
            </h1>
            <p className="game-setup-purpose">
              Train sustained attention, spatial planning, and controlled
              movement.
            </p>
          </div>
          <Link className="game-start-button result-link" to="/games/snake/results">
            Start session
          </Link>
        </section>

        <div className="game-setup-grid">
          <section className="account-card" aria-label="Rules">
            <div className="account-section-head compact-head">
              <h2>Rules</h2>
            </div>
            <ul className="game-rule-list">
              {rules.map((rule) => (
                <li key={rule}>{rule}</li>
              ))}
            </ul>
          </section>

          <section className="account-card" aria-label="Settings">
            <div className="account-section-head compact-head">
              <h2>Settings</h2>
            </div>
            <div className="game-setting-group">
              <span>Speed</span>
              <div className="range-tabs">
                <button type="button">Slow</button>
                <button className="is-active" type="button">
                  Normal
                </button>
                <button type="button">Fast</button>
              </div>
            </div>
            <div className="game-setting-group">
              <span>Dead zones</span>
              <div className="range-tabs">
                <button type="button">Off</button>
                <button className="is-active" type="button">
                  On
                </button>
              </div>
            </div>
          </section>
        </div>

        <section className="account-card" aria-label="Metrics preview">
          <div className="account-section-head compact-head">
            <h2>Metrics preview</h2>
          </div>
          <div className="game-metric-preview">
            {metrics.map((metric) => (
              <span key={metric}>{metric}</span>
            ))}
          </div>
        </section>
      </div>
    </AppFrame>
  );
}
