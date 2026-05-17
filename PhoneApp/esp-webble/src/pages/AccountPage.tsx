import { useCallback, useEffect, useMemo, useState, type ReactNode } from "react";
import { useAuth } from "../auth/useAuth";
import { AuthModal } from "../components/auth/AuthModal";
import { AppFrame } from "../components/layout/AppFrame";
import {
  createMockSession,
  getUserSessions,
} from "../services/sessionService";
import type { GameType, StoredSession } from "../sessions/types";

function roleLabel(role?: string) {
  if (role === "expert") return "Medical expert";
  if (role === "parent") return "Parent";
  return "Not available";
}

function gameLabel(gameType?: GameType) {
  if (gameType === "goNoGo") return "Go/No-Go";
  if (gameType === "snake") return "Snake";
  return "No session";
}

function formatDate(session?: StoredSession) {
  const date = session?.createdAt?.toDate();

  if (!date) return "Not available";

  return new Intl.DateTimeFormat("en", {
    month: "short",
    day: "numeric",
    year: "numeric",
  }).format(date);
}

function formatDuration(durationSec?: number) {
  if (durationSec == null) return "Not available";

  const minutes = Math.floor(durationSec / 60);
  const seconds = durationSec % 60;

  if (minutes === 0) return `${seconds} sec`;
  if (seconds === 0) return `${minutes} min`;

  return `${minutes} min ${seconds} sec`;
}

type TrendLabel = "Improving" | "Stable" | "Needs attention";

type GraphMetricConfig = {
  key: string;
  label: string;
  unit: string;
  lowerIsBetter: boolean;
  getValue: (session: StoredSession) => number | null;
};

const goNoGoGraphMetrics: GraphMetricConfig[] = [
  {
    key: "omissionErrors",
    label: "Omission Errors",
    unit: "",
    lowerIsBetter: true,
    getValue: (session) =>
      session.gameType === "goNoGo" ? session.metrics.omissionErrors : null,
  },
  {
    key: "commissionErrors",
    label: "Commission Errors",
    unit: "",
    lowerIsBetter: true,
    getValue: (session) =>
      session.gameType === "goNoGo" ? session.metrics.commissionErrors : null,
  },
  {
    key: "meanReactionTimeMs",
    label: "Mean Reaction Time",
    unit: " ms",
    lowerIsBetter: true,
    getValue: (session) =>
      session.gameType === "goNoGo"
        ? session.metrics.meanReactionTimeMs
        : null,
  },
  {
    key: "accuracyPercent",
    label: "Accuracy",
    unit: "%",
    lowerIsBetter: false,
    getValue: (session) =>
      session.gameType === "goNoGo" ? session.metrics.accuracyPercent : null,
  },
  {
    key: "reactionTimeVariabilityMs",
    label: "Reaction Time Variability",
    unit: " ms",
    lowerIsBetter: true,
    getValue: (session) =>
      session.gameType === "goNoGo"
        ? session.metrics.reactionTimeVariabilityMs
        : null,
  },
  {
    key: "longestFocusStreak",
    label: "Longest Focus Streak",
    unit: "",
    lowerIsBetter: false,
    getValue: (session) =>
      session.gameType === "goNoGo" ? session.metrics.longestFocusStreak : null,
  },
];

const snakeGraphMetrics: GraphMetricConfig[] = [
  {
    key: "survivalTimeSec",
    label: "Survival Time",
    unit: " sec",
    lowerIsBetter: false,
    getValue: (session) =>
      session.gameType === "snake" ? session.metrics.survivalTimeSec : null,
  },
  {
    key: "finalScore",
    label: "Final Score",
    unit: "",
    lowerIsBetter: false,
    getValue: (session) =>
      session.gameType === "snake" ? session.metrics.finalScore : null,
  },
  {
    key: "applesCollected",
    label: "Apples Collected",
    unit: "",
    lowerIsBetter: false,
    getValue: (session) =>
      session.gameType === "snake" ? session.metrics.applesCollected : null,
  },
  {
    key: "averageTimeBetweenApplesSec",
    label: "Average Time Between Apples",
    unit: " sec",
    lowerIsBetter: true,
    getValue: (session) =>
      session.gameType === "snake"
        ? session.metrics.averageTimeBetweenApplesSec
        : null,
  },
];

function sessionTime(session: StoredSession) {
  return session.createdAt?.toDate().getTime() ?? 0;
}

function formatMetricValue(value: number | null | undefined, unit: string) {
  if (value == null) return "Not available";

  return `${value}${unit}`;
}

function deathTypeInterpretation(deathType?: string) {
  if (deathType === "wallCollision") {
    return "Wall collisions may reflect rushed movement or reduced spatial planning.";
  }

  if (deathType === "selfCollision") {
    return "Self-collisions may reflect difficulty tracking recent movement patterns.";
  }

  if (deathType === "deadZoneCollision") {
    return "Dead-zone collisions may reflect attention lapses during higher-load moments.";
  }

  if (deathType === "timeout") {
    return "The session ended by time limit, which can reflect sustained play until completion.";
  }

  if (deathType === "manualStop") {
    return "The session was stopped manually, so performance interpretation may be limited.";
  }

  return "Not available";
}

function calculateTrend(
  values: number[],
  lowerIsBetter: boolean
): TrendLabel {
  if (values.length < 2) return "Stable";

  const first = values[0];
  const latest = values[values.length - 1];
  const baseline = Math.max(Math.abs(first), 1);
  const changeRatio = Math.abs(latest - first) / baseline;

  if (changeRatio < 0.05) return "Stable";

  const movedBetter = lowerIsBetter ? latest < first : latest > first;
  return movedBetter ? "Improving" : "Needs attention";
}

function createGraphPoints(values: number[]) {
  const width = 320;
  const height = 180;
  const padding = 24;
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min;

  return values.map((value, index) => {
    const x =
      values.length === 1
        ? width / 2
        : padding + (index * (width - padding * 2)) / (values.length - 1);
    const normalized = range === 0 ? 0.5 : (value - min) / range;
    const y = height - padding - normalized * (height - padding * 2);

    return { x, y };
  });
}

function sessionExportLines(session: StoredSession) {
  const lines = [
    `Session: ${formatDate(session)}`,
    `Duration: ${formatDuration(session.durationSec)}`,
  ];

  if (session.gameType === "goNoGo") {
    lines.push(
      `Omission Errors: ${formatMetricValue(session.metrics.omissionErrors, "")}`,
      `Commission Errors: ${formatMetricValue(session.metrics.commissionErrors, "")}`,
      `Mean Reaction Time: ${formatMetricValue(session.metrics.meanReactionTimeMs, " ms")}`,
      `Reaction Time Variability: ${formatMetricValue(session.metrics.reactionTimeVariabilityMs, " ms")}`,
      `Accuracy: ${formatMetricValue(session.metrics.accuracyPercent, "%")}`,
      `Longest Focus Streak: ${formatMetricValue(session.metrics.longestFocusStreak, "")}`
    );
  }

  if (session.gameType === "snake") {
    lines.push(
      `Survival Time: ${formatMetricValue(session.metrics.survivalTimeSec, " sec")}`,
      `Final Score: ${formatMetricValue(session.metrics.finalScore, "")}`,
      `Apples Collected: ${formatMetricValue(session.metrics.applesCollected, "")}`,
      `Average Time Between Apples: ${formatMetricValue(session.metrics.averageTimeBetweenApplesSec, " sec")}`,
      `Death Type: ${session.metrics.deathType || "Not available"}`,
      `Death Type Interpretation: ${deathTypeInterpretation(session.metrics.deathType)}`
    );
  }

  return lines;
}

function buildExportText({
  name,
  email,
  role,
  securityCode,
  sessions,
}: {
  name: string;
  email: string;
  role: string;
  securityCode: string;
  sessions: StoredSession[];
}) {
  if (sessions.length === 0) {
    return "No sessions available to export.";
  }

  const goNoGoSessions = sessions.filter(
    (session) => session.gameType === "goNoGo"
  );
  const snakeSessions = sessions.filter(
    (session) => session.gameType === "snake"
  );
  const lines = [
    "ADHD Cube Session Export",
    "",
    "Profile",
    `Name: ${name}`,
    `Email: ${email}`,
    `Role: ${role}`,
    `Security Code: ${securityCode}`,
    "",
    `Total Sessions: ${sessions.length}`,
    "",
    "Go/No-Go",
  ];

  if (goNoGoSessions.length === 0) {
    lines.push("No Go/No-Go sessions available.");
  } else {
    goNoGoSessions.forEach((session, index) => {
      lines.push("", `Go/No-Go Session ${index + 1}`);
      lines.push(...sessionExportLines(session));
    });
  }

  lines.push("", "Snake");

  if (snakeSessions.length === 0) {
    lines.push("No Snake sessions available.");
  } else {
    snakeSessions.forEach((session, index) => {
      lines.push("", `Snake Session ${index + 1}`);
      lines.push(...sessionExportLines(session));
    });
  }

  return lines.join("\n");
}

export default function AccountPage() {
  const { firebaseUser, profile, loading } = useAuth();
  const [authOpen, setAuthOpen] = useState(false);
  const [sessions, setSessions] = useState<StoredSession[]>([]);
  const [sessionsLoading, setSessionsLoading] = useState(false);
  const [sessionsError, setSessionsError] = useState("");
  const [selectedSessionId, setSelectedSessionId] = useState("");
  const [addingMockType, setAddingMockType] = useState<GameType | null>(null);
  const [selectedGraphGame, setSelectedGraphGame] =
    useState<GameType>("goNoGo");
  const [selectedGraphMetricKey, setSelectedGraphMetricKey] =
    useState("omissionErrors");
  const [exportText, setExportText] = useState("Session export will appear here.");
  const [copyMessage, setCopyMessage] = useState("");
  const isLoggedIn = Boolean(firebaseUser);
  const uid = firebaseUser?.uid;

  const profileName = profile?.name || firebaseUser?.displayName || "Not available";
  const profileEmail = profile?.email || firebaseUser?.email || "Not available";
  const profileRole = roleLabel(profile?.role);
  const profileSecurityCode = profile?.securityCode || "Not available";

  const loadSessions = useCallback(async () => {
    if (!uid) {
      setSessions([]);
      setSelectedSessionId("");
      return;
    }

    setSessionsLoading(true);
    setSessionsError("");

    try {
      const loadedSessions = await getUserSessions(uid);
      setSessions(loadedSessions);
      setSelectedSessionId((currentId) => {
        if (loadedSessions.some((session) => session.id === currentId)) {
          return currentId;
        }

        return loadedSessions[0]?.id ?? "";
      });
    } catch {
      setSessionsError("Could not load sessions. Please try again.");
    } finally {
      setSessionsLoading(false);
    }
  }, [uid]);

  useEffect(() => {
    if (!loading && isLoggedIn) {
      void loadSessions();
    }

    if (!isLoggedIn) {
      setSessions([]);
      setSelectedSessionId("");
    }
  }, [isLoggedIn, loadSessions, loading]);

  const selectedSession = useMemo(
    () =>
      sessions.find((session) => session.id === selectedSessionId) ??
      sessions[0],
    [selectedSessionId, sessions]
  );

  const goNoGoMetrics = useMemo(() => {
    const metrics =
      selectedSession?.gameType === "goNoGo" ? selectedSession.metrics : null;

    return [
      {
        label: "Omission Errors",
        value: formatMetricValue(metrics?.omissionErrors, ""),
        detail: "Missed go prompts can indicate lapses in sustained attention.",
        trend: metrics ? "Loaded session" : "Add a Go/No-Go session",
      },
      {
        label: "Commission Errors",
        value: formatMetricValue(metrics?.commissionErrors, ""),
        detail: "Responses on no-go prompts may reflect impulse-control load.",
        trend: metrics ? "Loaded session" : "Add a Go/No-Go session",
      },
      {
        label: "Mean Reaction Time",
        value: formatMetricValue(metrics?.meanReactionTimeMs, " ms"),
        detail: "Average response speed across correct go prompts.",
        trend: metrics ? "Loaded session" : "Add a Go/No-Go session",
      },
      {
        label: "Reaction Time Variability",
        value: formatMetricValue(metrics?.reactionTimeVariabilityMs, " ms"),
        detail: "Lower variation can indicate more consistent responding.",
        trend: metrics ? "Loaded session" : "Add a Go/No-Go session",
      },
      {
        label: "Accuracy",
        value: formatMetricValue(metrics?.accuracyPercent, "%"),
        detail: "Correct responses compared with total prompts.",
        trend: metrics ? "Loaded session" : "Add a Go/No-Go session",
      },
      {
        label: "Longest Focus Streak",
        value: formatMetricValue(metrics?.longestFocusStreak, ""),
        detail: "Longest run of correct responses without interruption.",
        trend: metrics ? "Loaded session" : "Add a Go/No-Go session",
      },
    ];
  }, [selectedSession]);

  const snakeMetrics = useMemo(() => {
    const metrics =
      selectedSession?.gameType === "snake" ? selectedSession.metrics : null;

    return [
      {
        label: "Survival Time",
        value: formatMetricValue(metrics?.survivalTimeSec, " sec"),
        detail: "How long the player stayed active before the round ended.",
      },
      {
        label: "Final Score",
        value: formatMetricValue(metrics?.finalScore, ""),
        detail: "Combined result from survival, movement, and collected items.",
      },
      {
        label: "Apples Collected",
        value: formatMetricValue(metrics?.applesCollected, ""),
        detail: "Collected targets during the session.",
      },
      {
        label: "Average Time Between Apples",
        value: formatMetricValue(metrics?.averageTimeBetweenApplesSec, " sec"),
        detail: "Shorter times can indicate faster planning and execution.",
      },
      {
        label: "Death Type",
        value: metrics?.deathType ?? "Not available",
        detail: deathTypeInterpretation(metrics?.deathType),
      },
    ];
  }, [selectedSession]);

  const graphMetricOptions =
    selectedGraphGame === "goNoGo" ? goNoGoGraphMetrics : snakeGraphMetrics;
  const selectedGraphMetric =
    (graphMetricOptions.find(
      (metric) => metric.key === selectedGraphMetricKey
    ) ?? graphMetricOptions[0]) as GraphMetricConfig;

  const graphSessions = useMemo(
    () =>
      [...sessions]
        .filter((session) => session.gameType === selectedGraphGame)
        .sort((a, b) => sessionTime(a) - sessionTime(b)),
    [selectedGraphGame, sessions]
  );

  const graphValues = useMemo(
    () =>
      graphSessions
        .map((session) => selectedGraphMetric.getValue(session))
        .filter((value): value is number => value != null),
    [graphSessions, selectedGraphMetric]
  );

  const graphPoints = graphValues.length > 0 ? createGraphPoints(graphValues) : [];
  const graphPointList = graphPoints
    .map((point) => `${point.x.toFixed(1)},${point.y.toFixed(1)}`)
    .join(" ");
  const latestGraphValue = graphValues[graphValues.length - 1];
  const graphTrend = calculateTrend(
    graphValues,
    selectedGraphMetric.lowerIsBetter
  );

  const handleAddMockSession = async (gameType: GameType) => {
    if (!uid) return;

    setAddingMockType(gameType);
    setSessionsError("");

    try {
      await createMockSession(uid, gameType);
      await loadSessions();
    } catch {
      setSessionsError("Could not add mock session. Please try again.");
    } finally {
      setAddingMockType(null);
    }
  };

  const handleGraphGameChange = (gameType: GameType) => {
    setSelectedGraphGame(gameType);
    setSelectedGraphMetricKey(
      gameType === "goNoGo"
        ? goNoGoGraphMetrics[0]?.key ?? "omissionErrors"
        : snakeGraphMetrics[0]?.key ?? "survivalTimeSec"
    );
  };

  const handleExportAll = () => {
    setCopyMessage("");
    setExportText(
      buildExportText({
        name: profileName,
        email: profileEmail,
        role: profileRole,
        securityCode: profileSecurityCode,
        sessions,
      })
    );
  };

  const handleCopyExport = async () => {
    if (!exportText.trim()) return;

    try {
      await navigator.clipboard.writeText(exportText);
      setCopyMessage("Copied.");
    } catch {
      setCopyMessage("Copy failed.");
    }
  };

  const appFrame = (children: ReactNode) => (
    <AppFrame
      ariaLabel="Account"
      activePage="account"
    >
      {children}
    </AppFrame>
  );

  if (loading) {
    return appFrame(
      <div className="account-home">
        <section className="account-header" aria-labelledby="account-page-title">
          <div>
            <h1 id="account-page-title" className="account-title">
              Account
            </h1>
            <p className="account-subtitle">Loading your profile...</p>
          </div>
        </section>
      </div>
    );
  }

  if (!isLoggedIn) {
    return appFrame(
      <>
        <div className="account-home">
          <section
            className="account-header logged-out-account"
            aria-labelledby="account-page-title"
          >
            <div>
              <h1 id="account-page-title" className="account-title">
                Account
              </h1>
              <p className="account-subtitle">
                Log in to view your profile and session statistics.
              </p>
            </div>
            <button
              className="export-button"
              type="button"
              onClick={() => setAuthOpen(true)}
            >
              Log in
            </button>
          </section>
        </div>
        {authOpen && <AuthModal onClose={() => setAuthOpen(false)} />}
      </>
    );
  }

  return (
    <AppFrame
      ariaLabel="Account"
      activePage="account"
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
          <div className="mock-session-actions" aria-label="Mock session tools">
            <button
              className="export-button"
              type="button"
              disabled={addingMockType !== null}
              onClick={() => void handleAddMockSession("goNoGo")}
            >
              {addingMockType === "goNoGo"
                ? "Adding..."
                : "Add mock Go/No-Go session"}
            </button>
            <button
              className="export-button"
              type="button"
              disabled={addingMockType !== null}
              onClick={() => void handleAddMockSession("snake")}
            >
              {addingMockType === "snake"
                ? "Adding..."
                : "Add mock Snake session"}
            </button>
          </div>
        </section>

        <section className="account-card session-section" aria-label="Session breakdown">
          <div className="account-section-head">
            <div>
              <span className="section-kicker">Session breakdown</span>
              <h2>Selected session</h2>
            </div>
            <select
              className="session-select"
              aria-label="Select session"
              disabled={sessionsLoading || sessions.length === 0}
              value={selectedSession?.id ?? ""}
              onChange={(event) => setSelectedSessionId(event.target.value)}
            >
              {sessions.length === 0 ? (
                <option>No sessions</option>
              ) : (
                sessions.map((session) => (
                  <option key={session.id} value={session.id}>
                    {gameLabel(session.gameType)} - {formatDate(session)}
                  </option>
                ))
              )}
            </select>
          </div>

          {sessionsLoading && (
            <p className="account-session-state">Loading sessions...</p>
          )}

          {sessionsError && (
            <p className="auth-message account-session-state">{sessionsError}</p>
          )}

          {!sessionsLoading && sessions.length === 0 ? (
            <div className="export-output account-empty-state">
              No sessions yet. Add a mock session to test the account dashboard.
            </div>
          ) : (
            <>
              <div className="session-list" aria-label="Recent sessions">
                {sessions.map((session) => (
                  <button
                    className={`session-pill${
                      session.id === selectedSession?.id ? " is-active" : ""
                    }`}
                    type="button"
                    key={session.id}
                    onClick={() => setSelectedSessionId(session.id)}
                  >
                    <strong>{gameLabel(session.gameType)}</strong>
                    <span>{formatDate(session)}</span>
                  </button>
                ))}
              </div>

              <div className="selected-session-card">
                <div>
                  <span>Game type</span>
                  <strong>{gameLabel(selectedSession?.gameType)}</strong>
                </div>
                <div>
                  <span>Date</span>
                  <strong>{formatDate(selectedSession)}</strong>
                </div>
                <div>
                  <span>Duration</span>
                  <strong>{formatDuration(selectedSession?.durationSec)}</strong>
                </div>
              </div>
            </>
          )}

          {selectedSession?.gameType === "goNoGo" && (
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
          )}

          {selectedSession?.gameType === "snake" && (
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
          )}
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

          <div className="metric-selector" aria-label="Game selector">
            {(["goNoGo", "snake"] as const).map((gameType) => (
              <button
                className={selectedGraphGame === gameType ? "is-active" : ""}
                type="button"
                key={gameType}
                onClick={() => handleGraphGameChange(gameType)}
              >
                {gameLabel(gameType)}
              </button>
            ))}
          </div>

          <div className="metric-selector" aria-label="Metric selector">
            {graphMetricOptions.map((metric) => (
              <button
                className={
                  selectedGraphMetric.key === metric.key ? "is-active" : ""
                }
                type="button"
                key={metric.key}
                onClick={() => setSelectedGraphMetricKey(metric.key)}
              >
                {metric.label}
              </button>
            ))}
          </div>

          <div className="progress-layout">
            <div className="graph-card" aria-label="Session trend graph">
              <div className="graph-lines">
                <span />
                <span />
                <span />
                <span />
              </div>
              {graphValues.length === 0 ? (
                <p className="graph-empty">
                  No {gameLabel(selectedGraphGame)} data for this metric.
                </p>
              ) : (
                <svg
                  className="session-graph"
                  viewBox="0 0 320 180"
                  role="img"
                  aria-label={`${selectedGraphMetric.label} over time`}
                >
                  <polyline points={graphPointList} />
                  {graphPoints.map((point, index) => (
                    <circle
                      key={`${point.x}-${point.y}-${index}`}
                      cx={point.x}
                      cy={point.y}
                      r="4"
                    />
                  ))}
                </svg>
              )}
            </div>
            <aside className="trend-summary">
              <span>{selectedGraphMetric.label}</span>
              <strong>
                {formatMetricValue(latestGraphValue, selectedGraphMetric.unit)}
              </strong>
              <p>{graphTrend}</p>
              <p>
                {graphValues.length} {gameLabel(selectedGraphGame)} values in
                chronological order.
              </p>
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
                  <dt>Name</dt>
                  <dd>{profileName}</dd>
                </div>
                <div>
                  <dt>Email</dt>
                  <dd>{profileEmail}</dd>
                </div>
                <div>
                  <dt>Role</dt>
                  <dd>{profileRole}</dd>
                </div>
                <div>
                  <dt>Security Code</dt>
                  <dd>{profileSecurityCode}</dd>
                </div>
              </dl>
              <p className="profile-note">
                Share this code with a medical expert to link your session data.
              </p>
            </div>

            <div className="export-panel">
              <div className="account-section-head compact-head">
                <h2>Export</h2>
                <div className="export-actions">
                  <button
                    className="export-button"
                    type="button"
                    onClick={handleExportAll}
                  >
                    Export all
                  </button>
                  <button
                    className="export-button"
                    type="button"
                    onClick={() => void handleCopyExport()}
                  >
                    Copy
                  </button>
                </div>
              </div>
              <pre className="export-output">{exportText}</pre>
              {copyMessage && <p className="copy-message">{copyMessage}</p>}
            </div>
          </div>
        </section>

      </div>
    </AppFrame>
  );
}
