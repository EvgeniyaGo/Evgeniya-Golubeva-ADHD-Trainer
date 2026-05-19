import type { FaceId, SessionResult } from "./types";

function paramsFromParts(parts: string[]) {
  const params: Record<string, string> = {};

  for (const p of parts) {
    const [k, v] = p.split("=");
    if (k && v !== undefined) params[k] = v;
  }

  return params;
}

function requiredNumber(params: Record<string, string>, key: string) {
  const value = Number(params[key]);
  return Number.isFinite(value) ? value : null;
}

function parseDifficulty(value: string): string | number {
  const asNumber = Number(value);
  return Number.isFinite(asNumber) && value.trim() !== "" ? asNumber : value;
}

export function parseEndRound(line: string): {
  result: "SUCCESS" | "FAIL";
  face: FaceId;
  time?: number;
  reason?: string;
} | null {
  const parts = line.split(/\s+/);

  if (parts.length === 3) {
    return {
      result: "SUCCESS",
      face: parts[2] as FaceId,
    };
  }

  const params = paramsFromParts(parts.slice(2));

  if (!params.face || !params.result) return null;

  return {
    result: params.result as "SUCCESS" | "FAIL",
    face: params.face as FaceId,
    time: params.time ? Number(params.time) : undefined,
    reason: params.reason,
  };
}

export function parseSessionEnd(line: string): SessionResult | null {
  const parts = line.trim().split(/\s+/);

  if (parts[0] !== "SESSION" || parts[1] !== "END") return null;

  const params = paramsFromParts(parts.slice(2));

  if (params.type === "SIMON") {
    const durationMs = requiredNumber(params, "durationMs");
    const omissionErrors = requiredNumber(params, "omissionErrors");
    const commissionErrors = requiredNumber(params, "commissionErrors");
    const meanReactionMs = requiredNumber(params, "meanReactionMs");
    const reactionStdMs = requiredNumber(params, "reactionStdMs");
    const accuracyPct = requiredNumber(params, "accuracyPct");
    const longestFocusStreak = requiredNumber(params, "longestFocusStreak");
    const rounds = requiredNumber(params, "rounds");

    if (
      durationMs == null ||
      params.difficulty == null ||
      omissionErrors == null ||
      commissionErrors == null ||
      meanReactionMs == null ||
      reactionStdMs == null ||
      accuracyPct == null ||
      longestFocusStreak == null ||
      rounds == null
    ) {
      return null;
    }

    return {
      type: "SIMON",
      durationMs,
      difficulty: parseDifficulty(params.difficulty),
      omissionErrors,
      commissionErrors,
      meanReactionMs,
      reactionStdMs,
      accuracyPct,
      longestFocusStreak,
      rounds,
    };
  }

  if (params.type === "SNAKE") {
    const durationMs = requiredNumber(params, "durationMs");
    const speedMs = requiredNumber(params, "speedMs");
    const finalScore = requiredNumber(params, "finalScore");
    const apples = requiredNumber(params, "apples");
    const avgAppleMs = requiredNumber(params, "avgAppleMs");

    if (
      durationMs == null ||
      speedMs == null ||
      finalScore == null ||
      apples == null ||
      avgAppleMs == null ||
      !params.deathType
    ) {
      return null;
    }

    return {
      type: "SNAKE",
      durationMs,
      speedMs,
      finalScore,
      apples,
      avgAppleMs,
      deathType: params.deathType,
    };
  }

  return null;
}
