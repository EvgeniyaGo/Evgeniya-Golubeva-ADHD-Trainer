import { Link } from "react-router-dom";
import { useState } from "react";
import { useAuth } from "../auth/useAuth";
import { AppFrame } from "../components/layout/AppFrame";
import { createMockSession } from "../services/sessionService";

const metrics = [
  { label: "Omission Errors", value: "4", detail: "Missed target responses." },
  {
    label: "Commission Errors",
    value: "2",
    detail: "Responses made to distractor signals.",
  },
  {
    label: "Mean Reaction Time",
    value: "620 ms",
    detail: "Average response speed on correct target responses.",
  },
  {
    label: "Reaction Time Variability",
    value: "118 ms",
    detail: "Variation in response timing across the session.",
  },
  {
    label: "Accuracy",
    value: "88%",
    detail: "Correct responses compared with total prompts.",
  },
  {
    label: "Longest Focus Streak",
    value: "16",
    detail: "Longest run of correct responses without interruption.",
  },
];

export default function GoNoGoResultsPage() {
  const { firebaseUser } = useAuth();
  const [saveMessage, setSaveMessage] = useState("");

  const handleSaveMockSession = async () => {
    if (!firebaseUser) {
      setSaveMessage("Log in to save a mock session.");
      return;
    }

    await createMockSession(firebaseUser.uid, "goNoGo");
    setSaveMessage("Mock Go/No-Go session saved.");
  };

  return (
    <AppFrame ariaLabel="Go/No-Go results" activePage="home">
      <div className="game-setup-page">
        <section className="game-setup-hero" aria-labelledby="go-no-go-results-title">
          <div>
            <span className="section-kicker">Session complete</span>
            <h1 id="go-no-go-results-title" className="game-setup-title">
              Go/No-Go Results
            </h1>
            <p className="game-setup-purpose">
              Review response accuracy, timing, and attention consistency from
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
              <span>Duration</span>
              <strong>3 min</strong>
            </div>
            <div>
              <span>Difficulty</span>
              <strong>Normal</strong>
            </div>
            <div>
              <span>Date/time</span>
              <strong>Today, placeholder</strong>
            </div>
          </div>
        </section>

        <section className="account-card" aria-label="Go/No-Go metrics">
          <div className="account-section-head compact-head">
            <h2>Metrics</h2>
          </div>
          <div className="metric-grid">
            {metrics.map((metric) => (
              <article className="analysis-card" key={metric.label}>
                <span className="analysis-label">{metric.label}</span>
                <strong>{metric.value}</strong>
                <p>{metric.detail}</p>
              </article>
            ))}
          </div>
        </section>

        <section className="account-card" aria-label="Interpretation">
          <div className="account-section-head compact-head">
            <h2>Interpretation</h2>
          </div>
          <p className="result-interpretation">
            Missed targets may indicate moments of reduced sustained attention.
            Responses to distractors may reflect higher impulse-control load.
            Reaction time variability can indicate how consistent responses were
            during the session.
          </p>
        </section>

        <div className="result-actions">
          <button
            className="export-button"
            type="button"
            onClick={() => void handleSaveMockSession()}
          >
            Save mock session
          </button>
          <Link className="export-button result-link" to="/games/go-no-go">
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
