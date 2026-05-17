import { Link } from "react-router-dom";
import { useState } from "react";
import { useAuth } from "../auth/useAuth";
import { AppFrame } from "../components/layout/AppFrame";
import { createMockSession } from "../services/sessionService";

const deathType = "wallCollision";

const metrics = [
  {
    label: "Survival Time",
    value: "2 min 12 sec",
    detail: "How long the player stayed active.",
  },
  {
    label: "Final Score",
    value: "24",
    detail: "Score reached before the session ended.",
  },
  {
    label: "Apples Collected",
    value: "18",
    detail: "Targets collected during the session.",
  },
  {
    label: "Average Time Between Apples",
    value: "7.3 sec",
    detail: "Average time needed to collect each apple.",
  },
  {
    label: "Death Type",
    value: deathType,
    detail: "How the session ended.",
  },
];

function deathTypeText(type: string) {
  if (type === "wallCollision") {
    return "Wall collisions may reflect rushed movement or reduced spatial planning.";
  }

  if (type === "selfCollision") {
    return "Self-collisions may reflect difficulty tracking recent movement patterns.";
  }

  if (type === "deadZoneCollision") {
    return "Dead-zone collisions may reflect attention lapses during higher-load moments.";
  }

  if (type === "timeout") {
    return "The session ended by time limit.";
  }

  return "The session was stopped manually, so interpretation may be limited.";
}

export default function SnakeResultsPage() {
  const { firebaseUser } = useAuth();
  const [saveMessage, setSaveMessage] = useState("");

  const handleSaveMockSession = async () => {
    if (!firebaseUser) {
      setSaveMessage("Log in to save a mock session.");
      return;
    }

    await createMockSession(firebaseUser.uid, "snake");
    setSaveMessage("Mock Snake session saved.");
  };

  return (
    <AppFrame ariaLabel="3D Snake results" activePage="home">
      <div className="game-setup-page">
        <section className="game-setup-hero" aria-labelledby="snake-results-title">
          <div>
            <span className="section-kicker">Session complete</span>
            <h1 id="snake-results-title" className="game-setup-title">
              3D Snake Results
            </h1>
            <p className="game-setup-purpose">
              Review movement control, planning, and sustained attention from
              this mock session.
            </p>
          </div>
        </section>

        <section className="account-card" aria-label="Session summary">
          <div className="account-section-head compact-head">
            <h2>Session summary</h2>
          </div>
          <div className="selected-session-card">
            <div>
              <span>Survival time</span>
              <strong>2 min 12 sec</strong>
            </div>
            <div>
              <span>Speed</span>
              <strong>Normal</strong>
            </div>
            <div>
              <span>Date/time</span>
              <strong>Today, placeholder</strong>
            </div>
          </div>
        </section>

        <section className="account-card" aria-label="Snake metrics">
          <div className="account-section-head compact-head">
            <h2>Metrics</h2>
          </div>
          <div className="metric-grid snake-grid">
            {metrics.map((metric) => (
              <article
                className={`analysis-card${
                  metric.label === "Death Type" ? " death-type-card" : ""
                }`}
                key={metric.label}
              >
                <span className="analysis-label">{metric.label}</span>
                <strong>{metric.value}</strong>
                <p>{metric.detail}</p>
              </article>
            ))}
          </div>
        </section>

        <section className="account-card" aria-label="Death type interpretation">
          <div className="account-section-head compact-head">
            <h2>Death type interpretation</h2>
          </div>
          <p className="result-interpretation">{deathTypeText(deathType)}</p>
        </section>

        <div className="result-actions">
          <button
            className="export-button"
            type="button"
            onClick={() => void handleSaveMockSession()}
          >
            Save mock session
          </button>
          <Link className="export-button result-link" to="/games/snake">
            Back to game setup
          </Link>
          <Link className="export-button result-link" to="/main">
            Back to main
          </Link>
        </div>
        {saveMessage && <p className="copy-message">{saveMessage}</p>}
      </div>
    </AppFrame>
  );
}
