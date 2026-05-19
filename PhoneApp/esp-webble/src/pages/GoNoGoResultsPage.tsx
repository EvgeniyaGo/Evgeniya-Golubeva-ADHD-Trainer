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
import type { SimonSessionResult } from "../console/protocol/types";

const mockResult: SimonSessionResult = {
  type: "SIMON",
  durationMs: 180000,
  difficulty: "Normal",
  omissionErrors: 4,
  commissionErrors: 2,
  meanReactionMs: 620,
  reactionStdMs: 118,
  accuracyPct: 88,
  longestFocusStreak: 16,
  rounds: 22,
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

function buildMetrics(result: SimonSessionResult) {
  return [
    {
      label: "Omission Errors",
      value: String(result.omissionErrors),
      detail: "Missed target responses.",
    },
    {
      label: "Commission Errors",
      value: String(result.commissionErrors),
      detail: "Responses made to distractor signals.",
    },
    {
      label: "Mean Reaction Time",
      value: `${result.meanReactionMs} ms`,
      detail: "Average response speed on correct target responses.",
    },
    {
      label: "Reaction Time Variability",
      value: `${result.reactionStdMs} ms`,
      detail: "Variation in response timing across the session.",
    },
    {
      label: "Accuracy",
      value: `${result.accuracyPct}%`,
      detail: "Correct responses compared with total prompts.",
    },
    {
      label: "Longest Focus Streak",
      value: String(result.longestFocusStreak),
      detail: "Longest run of correct responses without interruption.",
    },
    {
      label: "Rounds",
      value: String(result.rounds),
      detail: "Total rounds completed in the session.",
    },
  ];
}

export default function GoNoGoResultsPage() {
  const { firebaseUser } = useAuth();
  const { latestSessionResult, latestSessionReceivedAt } = useGameSession();
  const [saveMessage, setSaveMessage] = useState("");
  const realResult =
    latestSessionResult?.type === "SIMON" ? latestSessionResult : null;
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
        setSaveMessage("Go/No-Go session saved.");
        return;
      }

      await createMockSession(firebaseUser.uid, "goNoGo");
      setSaveMessage("Mock Go/No-Go session saved.");
    } catch {
      setSaveMessage("Could not save session. Try again.");
    }
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
              <span>Duration</span>
              <strong>{formatDuration(result.durationMs)}</strong>
            </div>
            <div>
              <span>Difficulty</span>
              <strong>{String(result.difficulty)}</strong>
            </div>
            <div>
              <span>Date/time</span>
              <strong>{formatDateTime(realReceivedAt)}</strong>
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
            onClick={() => void handleSaveSession()}
          >
            {realResult ? "Save session" : "Save mock session"}
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
