import type { Timestamp } from "firebase/firestore";

export type GameType = "goNoGo" | "snake";

export type GoNoGoMetrics = {
  omissionErrors: number;
  commissionErrors: number;
  meanReactionTimeMs: number;
  reactionTimeVariabilityMs: number;
  accuracyPercent: number;
  longestFocusStreak: number;
};

export type SnakeMetrics = {
  survivalTimeSec: number;
  finalScore: number;
  applesCollected: number;
  averageTimeBetweenApplesSec: number | null;
  deathType: string;
};

export type SessionRawData = Record<string, unknown>;

export type BaseSessionData = {
  durationSec: number;
  rawData?: SessionRawData;
};

export type CreateGoNoGoSessionData = BaseSessionData & {
  gameType: "goNoGo";
  metrics: GoNoGoMetrics;
};

export type CreateSnakeSessionData = BaseSessionData & {
  gameType: "snake";
  metrics: SnakeMetrics;
};

export type CreateSessionData =
  | CreateGoNoGoSessionData
  | CreateSnakeSessionData;

export type StoredGoNoGoSession = CreateGoNoGoSessionData & {
  id: string;
  userId: string;
  createdAt?: Timestamp;
};

export type StoredSnakeSession = CreateSnakeSessionData & {
  id: string;
  userId: string;
  createdAt?: Timestamp;
};

export type StoredSession = StoredGoNoGoSession | StoredSnakeSession;
