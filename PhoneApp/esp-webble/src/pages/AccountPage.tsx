import { AppFrame } from "../components/layout/AppFrame";
import { TopBarConnectionStatus } from "../components/TopBarConnectionStatus";

const sessions = [
  { id: "S-104", game: "Go/No-Go", date: "May 14, 2026", duration: "12 min" },
  { id: "S-103", game: "Snake", date: "May 12, 2026", duration: "9 min" },
  { id: "S-102", game: "Go/No-Go", date: "May 10, 2026", duration: "10 min" },
];

const goNoGoMetrics = [
  {
    label: "Omission Errors",
    value: "6",
    detail: "Missed go prompts can indicate lapses in sustained attention.",
    trend: "-2 vs prior",
  },
  {
    label: "Commission Errors",
    value: "3",
    detail: "Responses on no-go prompts may reflect impulse-control load.",
    trend: "Stable",
  },
  {
    label: "Mean Reaction Time",
    value: "480 ms",
    detail: "Average response speed across correct go prompts.",
    trend: "-35 ms",
  },
  {
    label: "Reaction Time Variability",
    value: "92 ms",
    detail: "Lower variation can indicate more consistent responding.",
    trend: "Improving",
  },
  {
    label: "Accuracy",
    value: "84%",
    detail: "Correct responses compared with total prompts.",
    trend: "+4%",
  },
  {
    label: "Longest Focus Streak",
    value: "18",
    detail: "Longest run of correct responses without interruption.",
    trend: "+3",
  },
];

const snakeMetrics = [
  {
    label: "Survival Time",
    value: "2:45",
    detail: "How long the player stayed active before the round ended.",
  },
  {
    label: "Final Score",
    value: "145",
    detail: "Combined result from survival, movement, and collected items.",
  },
  {
    label: "Apples Collected",
    value: "11",
    detail: "Collected targets during the session.",
  },
  {
    label: "Average Time Between Apples",
    value: "14.8 s",
    detail: "Shorter times can indicate faster planning and execution.",
  },
  {
    label: "Death Type",
    value: "Wall collision",
    detail:
      "Wall collisions may reflect rushed movement or reduced spatial planning.",
  },
];

const trendMetrics = [
  "Omission Errors",
  "Reaction Time",
  "Accuracy",
  "Survival Time",
  "Final Score",
];

export default function AccountPage() {
  return (
    <AppFrame
      ariaLabel="Account"
      activePage="account"
      topBarRight={
        <TopBarConnectionStatus
          isConnected={false}
          statusText="NOT CONNECTED"
          onToggleConnection={() => undefined}
        />
      }
    >
      <div className="account-home">
        <section className="account-header" aria-labelledby="account-page-title">
          <div>
            <h1 id="account-page-title" className="account-title">
              Session analysis
            </h1>
            <p className="account-subtitle">
              Review session details, spot patterns over time, and prepare
              exports for a medical expert.
            </p>
          </div>
        </section>

        <section className="account-card session-section" aria-label="Session breakdown">
          <div className="account-section-head">
            <div>
              <span className="section-kicker">Session breakdown</span>
              <h2>Selected session</h2>
            </div>
            <select className="session-select" aria-label="Select session">
              {sessions.map((session) => (
                <option key={session.id}>
                  {session.id} - {session.game}
                </option>
              ))}
            </select>
          </div>

          <div className="session-list" aria-label="Recent sessions">
            {sessions.map((session, index) => (
              <button
                className={`session-pill${index === 0 ? " is-active" : ""}`}
                type="button"
                key={session.id}
              >
                <strong>{session.game}</strong>
                <span>{session.date}</span>
              </button>
            ))}
          </div>

          <div className="selected-session-card">
            <div>
              <span>Game type</span>
              <strong>Go/No-Go</strong>
            </div>
            <div>
              <span>Date</span>
              <strong>May 14, 2026</strong>
            </div>
            <div>
              <span>Duration</span>
              <strong>12 min</strong>
            </div>
          </div>

          <div className="metric-block">
            <div className="metric-block-head">
              <h3>Go/No-Go metrics</h3>
              <span>Attention + response inhibition</span>
            </div>
            <div className="metric-grid">
              {goNoGoMetrics.map((metric) => (
                <article className="analysis-card" key={metric.label}>
                  <span className="analysis-label">{metric.label}</span>
                  <strong>{metric.value}</strong>
                  <p>{metric.detail}</p>
                  <small>{metric.trend}</small>
                </article>
              ))}
            </div>
          </div>

          <div className="metric-block">
            <div className="metric-block-head">
              <h3>Snake metrics</h3>
              <span>Planning + attention stability</span>
            </div>
            <div className="metric-grid snake-grid">
              {snakeMetrics.map((metric) => (
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
          </div>
        </section>

        <section className="account-card progress-section" aria-label="Progress over time">
          <div className="account-section-head">
            <div>
              <span className="section-kicker">Progress over time</span>
              <h2>Long-term trends</h2>
            </div>
            <div className="range-tabs" aria-label="Time range">
              <button className="is-active" type="button">
                Week
              </button>
              <button type="button">Month</button>
              <button type="button">All time</button>
            </div>
          </div>

          <div className="metric-selector" aria-label="Metric selector">
            {trendMetrics.map((metric, index) => (
              <button className={index === 1 ? "is-active" : ""} type="button" key={metric}>
                {metric}
              </button>
            ))}
          </div>

          <div className="progress-layout">
            <div className="graph-card" aria-label="Placeholder trend graph">
              <div className="graph-lines">
                <span />
                <span />
                <span />
                <span />
              </div>
              <div className="graph-path" />
            </div>
            <aside className="trend-summary">
              <span>Trend direction</span>
              <strong>Improving</strong>
              <p>Reaction consistency improved over the last 5 sessions.</p>
              <p>Attention stability remained relatively constant.</p>
            </aside>
          </div>
        </section>

        <section className="account-card profile-system-section" aria-label="Profile and export">
          <div className="profile-system-grid">
            <div>
              <div className="account-section-head compact-head">
                <h2>Profile</h2>
                <span>Parent access</span>
              </div>
              <dl className="profile-list">
                <div>
                  <dt>Email</dt>
                  <dd>parent@example.com</dd>
                </div>
                <div>
                  <dt>Role</dt>
                  <dd>Parent</dd>
                </div>
                <div>
                  <dt>Security Code</dt>
                  <dd>CUBE-4821</dd>
                </div>
              </dl>
              <p className="profile-note">
                Share this code with a medical expert to link your session data.
              </p>
            </div>

            <div className="export-panel">
              <div className="account-section-head compact-head">
                <h2>Export</h2>
                <button className="export-button" type="button">
                  Export all
                </button>
              </div>
              <div className="export-output">Session export will appear here.</div>
            </div>
          </div>
        </section>

      </div>
    </AppFrame>
  );
}
