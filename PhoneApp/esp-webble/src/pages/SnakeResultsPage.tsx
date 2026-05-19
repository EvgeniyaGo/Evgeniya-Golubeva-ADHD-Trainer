import { Link } from "react-router-dom";
import { useState } from "react";
import { useAuth } from "../auth/useAuth";
import { AppFrame } from "../components/layout/AppFrame";
import { useGameSession } from "../gameSession/useGameSession";
import {
  createMockSession,
  createSession,
  createSessionDataFromResult,
} from "../services/sessionService";
import type { SnakeSessionResult } from "../console/protocol/types";

const mockResult: SnakeSessionResult = {
  type: "SNAKE",
  durationMs: 132000,
  speedMs: 400,
  finalScore: 24,
  apples: 18,
  avgAppleMs: 7300,
  deathType: "wallCollision",
};

function formatDuration(ms: number) {
  const totalSeconds = Math.round(ms / 1000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;

  if (minutes <= 0) return `${seconds} sec`;
  if (seconds === 0) return `${minutes} min`;
  return `${minutes} min ${seconds} sec`;
}

function formatDateTime(date: Date | null) {
  if (!date) return "Mock fallback";

  return new Intl.DateTimeFormat("en", {
    month: "short",
    day: "numeric",
    year: "numeric",
    hour: "2-digit",
    minute: "2-digit",
  }).format(date);
}

function buildMetrics(result: SnakeSessionResult) {
  return [
    {
      label: "Survival Time",
      value: formatDuration(result.durationMs),
      detail: "How long the player stayed active.",
    },
    {
      label: "Speed",
      value: `${result.speedMs} ms`,
      detail: "Movement interval configured by the cube.",
    },
    {
      label: "Final Score",
      value: String(result.finalScore),
      detail: "Score reached before the session ended.",
    },
    {
      label: "Apples Collected",
      value: String(result.apples),
      detail: "Targets collected during the session.",
    },
    {
      label: "Average Time Between Apples",
      value: `${(result.avgAppleMs / 1000).toFixed(1)} sec`,
      detail: "Average time needed to collect each apple.",
    },
    {
      label: "Death Type",
      value: result.deathType,
      detail: "How the session ended.",
    },
  ];
}

function deathTypeText(type: string) {
  if (type === "wallCollision") {
    return "Wall collision may reflect rushed movement or reduced spatial planning.";
  }

  if (type === "selfCollision") {
    return "Self collision may reflect difficulty tracking recent movement patterns.";
  }

  if (type === "deadZoneCollision") {
    return "Dead-zone collision may reflect attention lapses during higher-load moments.";
  }

  if (type === "timeout") {
    return "Session ended by time limit.";
  }

  return "Manual stop interpretation may be limited.";
}

export default function SnakeResultsPage() {
  const { firebaseUser } = useAuth();
  const { latestSessionResult, latestSessionReceivedAt } = useGameSession();
  const [saveMessage, setSaveMessage] = useState("");
  const realResult =
    latestSessionResult?.type === "SNAKE" ? latestSessionResult : null;
  const realReceivedAt = realResult ? latestSessionReceivedAt : null;
  const result = realResult ?? mockResult;
  const metrics = buildMetrics(result);

  const handleSaveSession = async () => {
    if (!firebaseUser) {
      setSaveMessage("Log in to save this session.");
      return;
    }

    try {
      if (realResult && realReceivedAt) {
        await createSession(
          firebaseUser.uid,
          createSessionDataFromResult(realResult, realReceivedAt)
        );
        setSaveMessage("Snake session saved.");
        return;
      }

      await createMockSession(firebaseUser.uid, "snake");
      setSaveMessage("Mock Snake session saved.");
    } catch {
      setSaveMessage("Could not save session. Try again.");
    }
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
              this {realResult ? "cube" : "mock"} session.
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
              <strong>{formatDuration(result.durationMs)}</strong>
            </div>
            <div>
              <span>Speed</span>
              <strong>{result.speedMs} ms</strong>
            </div>
            <div>
              <span>Date/time</span>
              <strong>{formatDateTime(realReceivedAt)}</strong>
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
          <p className="result-interpretation">
            {deathTypeText(result.deathType)}
          </p>
        </section>

        <div className="result-actions">
          <button
            className="export-button"
            type="button"
            onClick={() => void handleSaveSession()}
          >
            {realResult ? "Save session" : "Save mock session"}
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
