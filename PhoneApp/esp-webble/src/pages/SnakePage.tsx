import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { useBle } from "../ble/useBle";
import { AppFrame } from "../components/layout/AppFrame";
import { useGameSession } from "../gameSession/useGameSession";

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
  const navigate = useNavigate();
  const { isConnected } = useBle();
  const {
    activeGame,
    latestSessionResult,
    lastCompletedGame,
    sessionError,
    startSnakeSession,
  } = useGameSession();
  const [waitingForResult, setWaitingForResult] = useState(false);

  useEffect(() => {
    if (
      waitingForResult &&
      lastCompletedGame === "snake" &&
      latestSessionResult?.type === "SNAKE"
    ) {
      navigate("/games/snake/results");
    }
  }, [lastCompletedGame, latestSessionResult, navigate, waitingForResult]);

  useEffect(() => {
    if (sessionError) {
      setWaitingForResult(false);
    }
  }, [sessionError]);

  const handleStart = async () => {
    setWaitingForResult(true);
    await startSnakeSession();
  };

  const isRunning = activeGame === "snake";

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
