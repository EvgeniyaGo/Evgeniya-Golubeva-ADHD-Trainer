import {
  addDoc,
  collection,
  getDocs,
  orderBy,
  query,
  serverTimestamp,
  where,
} from "firebase/firestore";
import { db } from "../firebase";
import type {
  CreateSessionData,
  GameType,
  StoredSession,
} from "../sessions/types";

function userSessionsCollection(userId: string) {
  return collection(db, "users", userId, "sessions");
}

function sessionFromDoc(
  userId: string,
  id: string,
  data: Record<string, unknown>
): StoredSession {
  return {
    id,
    userId,
    gameType: data.gameType,
    createdAt: data.createdAt,
    durationSec: data.durationSec,
    metrics: data.metrics,
    rawData: data.rawData,
  } as StoredSession;
}

export async function createSession(
  userId: string,
  sessionData: CreateSessionData
) {
  const docRef = await addDoc(userSessionsCollection(userId), {
    ...sessionData,
    createdAt: serverTimestamp(),
  });

  return docRef.id;
}

export async function getUserSessions(userId: string) {
  const sessionsQuery = query(
    userSessionsCollection(userId),
    orderBy("createdAt", "desc")
  );
  const snapshot = await getDocs(sessionsQuery);

  return snapshot.docs.map((sessionDoc) =>
    sessionFromDoc(userId, sessionDoc.id, sessionDoc.data())
  );
}

export async function getSessionsByGameType(userId: string, gameType: GameType) {
  const sessionsQuery = query(
    userSessionsCollection(userId),
    where("gameType", "==", gameType),
    orderBy("createdAt", "desc")
  );
  const snapshot = await getDocs(sessionsQuery);

  return snapshot.docs.map((sessionDoc) =>
    sessionFromDoc(userId, sessionDoc.id, sessionDoc.data())
  );
}

export const mockGoNoGoSession: CreateSessionData = {
  gameType: "goNoGo",
  durationSec: 180,
  metrics: {
    omissionErrors: 6,
    commissionErrors: 3,
    meanReactionTimeMs: 480,
    reactionTimeVariabilityMs: 92,
    accuracyPercent: 84,
    longestFocusStreak: 18,
  },
  rawData: {
    source: "mock",
    rounds: [],
  },
};

export const mockSnakeSession: CreateSessionData = {
  gameType: "snake",
  durationSec: 165,
  metrics: {
    survivalTimeSec: 165,
    finalScore: 145,
    applesCollected: 11,
    averageTimeBetweenApplesSec: 14.8,
    deathType: "Wall collision",
  },
  rawData: {
    source: "mock",
    events: [],
  },
};

function randomInt(min: number, max: number) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

function randomFloat(min: number, max: number, decimals = 1) {
  const value = Math.random() * (max - min) + min;
  return Number(value.toFixed(decimals));
}

function createMockGoNoGoSession(): CreateSessionData {
  return {
    gameType: "goNoGo",
    durationSec: randomInt(60, 300),
    metrics: {
      omissionErrors: randomInt(0, 12),
      commissionErrors: randomInt(0, 10),
      meanReactionTimeMs: randomInt(450, 1400),
      reactionTimeVariabilityMs: randomInt(80, 500),
      accuracyPercent: randomInt(55, 99),
      longestFocusStreak: randomInt(3, 25),
    },
    rawData: {
      source: "mock",
      generatedAt: new Date().toISOString(),
      rounds: [],
    },
  };
}

function createMockSnakeSession(): CreateSessionData {
  const durationSec = randomInt(20, 240);
  const finalScore = randomInt(1, 40);
  const applesCollected = randomInt(0, Math.min(35, finalScore));
  const deathTypes = [
    "wallCollision",
    "selfCollision",
    "deadZoneCollision",
    "timeout",
    "manualStop",
  ];

  return {
    gameType: "snake",
    durationSec,
    metrics: {
      survivalTimeSec: durationSec,
      finalScore,
      applesCollected,
      averageTimeBetweenApplesSec:
        applesCollected === 0 ? null : randomFloat(3, 25),
      deathType: deathTypes[randomInt(0, deathTypes.length - 1)],
    },
    rawData: {
      source: "mock",
      generatedAt: new Date().toISOString(),
      events: [],
    },
  };
}

export async function createMockSession(
  userId: string,
  gameType: GameType = "goNoGo"
) {
  const sessionData =
    gameType === "goNoGo"
      ? createMockGoNoGoSession()
      : createMockSnakeSession();

  return createSession(userId, sessionData);
}
