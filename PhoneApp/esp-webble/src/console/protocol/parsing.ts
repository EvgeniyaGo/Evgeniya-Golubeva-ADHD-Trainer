import type { FaceId } from "./types";

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

  const params: Record<string, string> = {};
  for (const p of parts.slice(2)) {
    const [k, v] = p.split("=");
    if (k && v) params[k] = v;
  }

  if (!params.face || !params.result) return null;

  return {
    result: params.result as "SUCCESS" | "FAIL",
    face: params.face as FaceId,
    time: params.time ? Number(params.time) : undefined,
    reason: params.reason,
  };
}
