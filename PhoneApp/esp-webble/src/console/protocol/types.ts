export type GattStuff = {
  device: BluetoothDevice;
  server: BluetoothRemoteGATTServer;
  service: BluetoothRemoteGATTService;
  rx: BluetoothRemoteGATTCharacteristic;
  tx: BluetoothRemoteGATTCharacteristic;
};

export type ConnectionStatus =
  | "idle"
  | "connecting"
  | "connected"
  | "disconnected";

export type FaceId =
  | "TOP"
  | "BOTTOM"
  | "LEFT"
  | "RIGHT"
  | "FRONT"
  | "BACK";

export type ShapeId =
  | "SHAPE_ARROW_UP"
  | "SHAPE_ARROW_DOWN"
  | "SHAPE_ARROW_LEFT"
  | "SHAPE_ARROW_RIGHT"
  | "SHAPE_CIRCLE_6X6";

export type Vec3 = { x: number; y: number; z: number };

export const RoundPhase = {
  IDLE: "IDLE",
  WAIT_BALANCE: "WAIT_BALANCE",
  PLAYING: "PLAYING",
} as const;

export type RoundPhase = (typeof RoundPhase)[keyof typeof RoundPhase];

export type PendingRound =
  | {
      type: "ARROW";
      mode?: "NORMAL" | "OPPOSITE";
      from: FaceId;
      to: FaceId;
      arrow: ShapeId;
      duration: number;
      remaining: number;
    }
  | { type: "PAUSE"; duration: number; remaining: number };

export type EndRoundFailData = {
  face: FaceId;
  time?: number;
  reason?: string;
};

export type SimonSessionResult = {
  type: "SIMON";
  durationMs: number;
  difficulty: string | number;
  omissionErrors: number;
  commissionErrors: number;
  meanReactionMs: number;
  reactionStdMs: number;
  accuracyPct: number;
  longestFocusStreak: number;
  rounds: number;
};

export type SnakeSessionResult = {
  type: "SNAKE";
  durationMs: number;
  speedMs: number;
  finalScore: number;
  apples: number;
  avgAppleMs: number;
  deathType: string;
};

export type SessionResult = SimonSessionResult | SnakeSessionResult;
