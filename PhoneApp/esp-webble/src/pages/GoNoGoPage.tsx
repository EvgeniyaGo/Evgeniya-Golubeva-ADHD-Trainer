import { Link } from "react-router-dom";
import { AppFrame } from "../components/layout/AppFrame";

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
          <Link className="game-start-button result-link" to="/games/go-no-go/results">
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
