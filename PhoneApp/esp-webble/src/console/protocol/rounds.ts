import type { FaceId, PendingRound } from "./types";

const oppositeFace: Record<FaceId, FaceId> = {
  TOP: "BOTTOM",
  BOTTOM: "TOP",
  LEFT: "RIGHT",
  RIGHT: "LEFT",
  FRONT: "BACK",
  BACK: "FRONT",
};

export function roundStartLine(round: PendingRound): string {
  if (round.type === "PAUSE") {
    return `ROUND START type=PAUSE duration=${round.duration} remaining=${round.remaining}\n`;
  }

  const isOpposite = round.type === "ARROW" && round.mode === "OPPOSITE";

  const expected = isOpposite ? oppositeFace[round.to] : round.to;

  return `ROUND START type=ARROW from=${round.from} to=${round.to} expected=${expected} duration=${round.duration} remaining=${round.remaining}\n`;
}
