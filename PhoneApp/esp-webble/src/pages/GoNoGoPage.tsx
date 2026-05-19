import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { useBle } from "../ble/useBle";
import { AppFrame } from "../components/layout/AppFrame";
import { useGameSession } from "../gameSession/useGameSession";

const rules = [
  "React only to target signals",
  "Ignore distractor signals",
  "Missed target responses count as omission errors",
  "Wrong responses to distractors count as commission errors",
];

const metrics = [
  "Omission Errors",
  "Commission Errors",
  "Mean Reaction Time",
  "Reaction Time Variability",
  "Accuracy",
  "Longest Focus Streak",
];

export default function GoNoGoPage() {
  const navigate = useNavigate();
  const { isConnected } = useBle();
  const {
    activeGame,
    latestSessionResult,
    lastCompletedGame,
    sessionError,
    startGoNoGoSession,
  } = useGameSession();
  const [waitingForResult, setWaitingForResult] = useState(false);

  useEffect(() => {
    if (
      waitingForResult &&
      lastCompletedGame === "goNoGo" &&
      latestSessionResult?.type === "SIMON"
    ) {
      navigate("/games/go-no-go/results");
    }
  }, [lastCompletedGame, latestSessionResult, navigate, waitingForResult]);

  useEffect(() => {
    if (sessionError) {
      setWaitingForResult(false);
    }
  }, [sessionError]);

  const handleStart = async () => {
    setWaitingForResult(true);
    await startGoNoGoSession();
  };

  const isRunning = activeGame === "goNoGo";

  return (
    <AppFrame ariaLabel="Go/No-Go setup" activePage="home">
      <div className="game-setup-page">
        <section className="game-setup-hero" aria-labelledby="go-no-go-title">
          <div>
            <span className="section-kicker">Focus + self-control</span>
            <h1 id="go-no-go-title" className="game-setup-title">
              Go/No-Go
            </h1>
            <p className="game-setup-purpose">
              Train impulse control, attention, and response inhibition.
            </p>
          </div>
          <button
            className="game-start-button result-link"
            disabled={!isConnected || isRunning}
            type="button"
            onClick={() => void handleStart()}
          >
            {isRunning ? "Waiting for results" : "Start session"}
          </button>
        </section>
        {!isConnected && (
          <p className="copy-message">Connect to the cube before starting.</p>
        )}
        {sessionError && <p className="copy-message">{sessionError}</p>}

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
              <span>Duration</span>
              <div className="range-tabs">
                <button type="button">1 min</button>
                <button className="is-active" type="button">
                  3 min
                </button>
                <button type="button">5 min</button>
              </div>
            </div>
            <div className="game-setting-group">
              <span>Difficulty</span>
              <div className="range-tabs">
                <button type="button">Easy</button>
                <button className="is-active" type="button">
                  Normal
                </button>
                <button type="button">Hard</button>
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
