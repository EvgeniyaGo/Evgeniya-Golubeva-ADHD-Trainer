import {
  createContext,
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";
import { useBle } from "../ble/useBle";
import { useBleWriter } from "../ble/useBleWriter";
import {
  MAX_PAUSE_MS,
  MAX_ROUND_MS,
  MIN_PAUSE_MS,
  MIN_ROUND_MS,
} from "../console/protocol/constants";
import { arrowFromTo } from "../console/protocol/faces";
import { parseEndRound, parseSessionEnd } from "../console/protocol/parsing";
import { roundStartLine } from "../console/protocol/rounds";
import {
  RoundPhase,
  type FaceId,
  type PendingRound,
  type SessionResult,
} from "../console/protocol/types";

type ActiveGame = "goNoGo" | "snake";

type GameSessionContextValue = {
  activeGame: ActiveGame | null;
  latestSessionResult: SessionResult | null;
  latestSessionReceivedAt: Date | null;
  lastCompletedGame: ActiveGame | null;
  sessionError: string;
  startGoNoGoSession: () => Promise<void>;
  startSnakeSession: () => Promise<void>;
  clearSessionError: () => void;
};

export const GameSessionContext =
  createContext<GameSessionContextValue | null>(null);

type GameSessionProviderProps = {
  children: ReactNode;
};

const adjacency: Record<FaceId, FaceId[]> = {
  TOP: ["FRONT", "BACK", "LEFT", "RIGHT"],
  BOTTOM: ["FRONT", "BACK", "LEFT", "RIGHT"],
  LEFT: ["TOP", "BOTTOM", "FRONT", "BACK"],
  RIGHT: ["TOP", "BOTTOM", "FRONT", "BACK"],
  FRONT: ["TOP", "BOTTOM", "LEFT", "RIGHT"],
  BACK: ["TOP", "BOTTOM", "LEFT", "RIGHT"],
};

function resultGame(result: SessionResult): ActiveGame {
  return result.type === "SIMON" ? "goNoGo" : "snake";
}

export function GameSessionProvider({ children }: GameSessionProviderProps) {
  const { gatt, isConnected } = useBle();
  const { writeLine } = useBleWriter();
  const [activeGame, setActiveGame] = useState<ActiveGame | null>(null);
  const [latestSessionResult, setLatestSessionResult] =
    useState<SessionResult | null>(null);
  const [latestSessionReceivedAt, setLatestSessionReceivedAt] =
    useState<Date | null>(null);
  const [lastCompletedGame, setLastCompletedGame] = useState<ActiveGame | null>(
    null
  );
  const [sessionError, setSessionError] = useState("");

  const activeGameRef = useRef<ActiveGame | null>(null);
  const pendingRoundRef = useRef<PendingRound | null>(null);
  const roundPhaseRef = useRef<RoundPhase>(RoundPhase.IDLE);
  const remainingRoundsRef = useRef(0);
  const baseDurationRef = useRef(3000);
  const successStreakRef = useRef(0);
  const failStreakRef = useRef(0);

  const setActiveGameState = useCallback((game: ActiveGame | null) => {
    activeGameRef.current = game;
    setActiveGame(game);
  }, []);

  const chooseNextRound = useCallback((from: FaceId, remaining: number) => {
    const choosePause = Math.random() < 0.5;

    if (choosePause) {
      const base = baseDurationRef.current || 3000;
      const pauseMax = Math.min(
        MAX_PAUSE_MS,
        Math.max(MIN_PAUSE_MS, base + base * (Math.random() * 0.5 - 0.2))
      );
      const duration = Math.round(
        MIN_PAUSE_MS + Math.random() * (pauseMax - MIN_PAUSE_MS)
      );

      return { type: "PAUSE", duration, remaining } satisfies PendingRound;
    }

    const options = adjacency[from];
    const to = options[Math.floor(Math.random() * options.length)];
    const mode: "NORMAL" | "OPPOSITE" =
      Math.random() < 0.5 ? "OPPOSITE" : "NORMAL";

    return {
      type: "ARROW",
      mode,
      from,
      to,
      arrow: arrowFromTo(from, to),
      duration: baseDurationRef.current,
      remaining,
    } satisfies PendingRound;
  }, []);

  const updateAdaptiveDuration = useCallback((result: "SUCCESS" | "FAIL") => {
    const base = baseDurationRef.current || 3000;
    const pct = 0.05 + Math.random() * 0.1;
    let streakBonus = 0;

    if (result === "SUCCESS") {
      successStreakRef.current++;
      failStreakRef.current = 0;
      streakBonus = Math.min(successStreakRef.current * 0.02, 0.1);
    } else {
      failStreakRef.current++;
      successStreakRef.current = 0;
      streakBonus = Math.min(failStreakRef.current * 0.02, 0.1);
    }

    const effectivePct = pct + streakBonus;
    const next =
      result === "SUCCESS"
        ? base * (1 - effectivePct)
        : base * (1 + effectivePct);

    baseDurationRef.current = Math.round(
      Math.min(MAX_ROUND_MS, Math.max(MIN_ROUND_MS, next))
    );
  }, []);

  const startNextRound = useCallback(
    async (from: FaceId) => {
      remainingRoundsRef.current -= 1;

      if (remainingRoundsRef.current <= 0) {
        pendingRoundRef.current = null;
        roundPhaseRef.current = RoundPhase.IDLE;
        await writeLine("GAME END\n");
        return;
      }

      const nextRound = chooseNextRound(from, remainingRoundsRef.current);
      pendingRoundRef.current = nextRound;
      roundPhaseRef.current = RoundPhase.WAIT_BALANCE;
      await writeLine(roundStartLine(nextRound));
    },
    [chooseNextRound, writeLine]
  );

  const startGoNoGoSession = useCallback(async () => {
    if (!isConnected) {
      setSessionError("Connect to the cube before starting.");
      return;
    }

    setSessionError("");
    setLastCompletedGame(null);
    remainingRoundsRef.current = 10;
    baseDurationRef.current = 3000;
    successStreakRef.current = 0;
    failStreakRef.current = 0;
    pendingRoundRef.current = null;
    roundPhaseRef.current = RoundPhase.IDLE;
    setActiveGameState("goNoGo");
    try {
      await writeLine("GAME START type=SIMON\n");
    } catch (error) {
      setActiveGameState(null);
      setSessionError(error instanceof Error ? error.message : String(error));
    }
  }, [isConnected, setActiveGameState, writeLine]);

  const startSnakeSession = useCallback(async () => {
    if (!isConnected) {
      setSessionError("Connect to the cube before starting.");
      return;
    }

    setSessionError("");
    setLastCompletedGame(null);
    pendingRoundRef.current = null;
    roundPhaseRef.current = RoundPhase.IDLE;
    setActiveGameState("snake");
    try {
      await writeLine("GAME START type=SNAKE\n");
    } catch (error) {
      setActiveGameState(null);
      setSessionError(error instanceof Error ? error.message : String(error));
    }
  }, [isConnected, setActiveGameState, writeLine]);

  useEffect(() => {
    if (!gatt?.tx) return;

    const tx = gatt.tx;
    let active = true;

    const handleNotification = async (ev: Event) => {
      const dv = (ev.target as BluetoothRemoteGATTCharacteristic).value!;
      const bytes = new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength);
      const text = new TextDecoder().decode(bytes).trimEnd();

      for (const line of text.split(/\r?\n/)) {
        if (!line) continue;

        if (line.startsWith("SESSION END")) {
          const result = parseSessionEnd(line);

          if (!result) {
            console.warn("[SESSION] Could not parse SESSION END", line);
            continue;
          }

          const receivedAt = new Date();
          setLatestSessionResult(result);
          setLatestSessionReceivedAt(receivedAt);
          setLastCompletedGame(resultGame(result));
          setActiveGameState(null);
          pendingRoundRef.current = null;
          roundPhaseRef.current = RoundPhase.IDLE;
          continue;
        }

        if (activeGameRef.current !== "goNoGo") {
          continue;
        }

        if (line.startsWith("OK GAME START")) {
          const facePart = line
            .split(/\s+/)
            .find((part) => part.toLowerCase().startsWith("face="));
          if (!facePart) continue;

          const startFace = facePart.split("=")[1] as FaceId;
          const firstRound = chooseNextRound(
            startFace,
            remainingRoundsRef.current
          );

          pendingRoundRef.current = firstRound;
          roundPhaseRef.current = RoundPhase.WAIT_BALANCE;
          await writeLine(roundStartLine(firstRound));
          continue;
        }

        if (line.startsWith("ROUND BALANCE")) {
          const round = pendingRoundRef.current;
          if (!round) continue;

          roundPhaseRef.current = RoundPhase.PLAYING;

          const sidePart = line
            .split(/\s+/)
            .find((part) => part.toLowerCase().startsWith("side="));
          if (!sidePart) continue;

          const balancedFace = sidePart.split("=")[1] as FaceId;
          await writeLine("CLEAR ALL\n");

          if (round.type === "ARROW") {
            const isOpposite = round.mode === "OPPOSITE";
            await writeLine(
              `DRAW SHAPE ${balancedFace} ${round.arrow} COLOR_BLUE\n`
            );
            await writeLine(
              `DRAW SHAPE ${round.to} SHAPE_CIRCLE_6X6 COLOR_GREEN\n`
            );
            await writeLine(
              isOpposite ? "BEEP freq=1000 dur=400\n" : "BEEP freq=1200 dur=200\n"
            );
          }

          continue;
        }

        if (line.startsWith("END ROUND")) {
          const data = parseEndRound(line);
          if (!data) continue;

          if (
            roundPhaseRef.current !== RoundPhase.PLAYING &&
            roundPhaseRef.current !== RoundPhase.WAIT_BALANCE
          ) {
            continue;
          }

          updateAdaptiveDuration(data.result === "SUCCESS" ? "SUCCESS" : "FAIL");
          await startNextRound(data.face);
        }
      }
    };

    void tx.startNotifications().then(() => {
      if (active) {
        tx.addEventListener("characteristicvaluechanged", handleNotification);
      }
    });

    return () => {
      active = false;
      tx.removeEventListener("characteristicvaluechanged", handleNotification);
    };
  }, [
    chooseNextRound,
    gatt?.tx,
    setActiveGameState,
    startNextRound,
    updateAdaptiveDuration,
    writeLine,
  ]);

  const clearSessionError = useCallback(() => {
    setSessionError("");
  }, []);

  const value = useMemo<GameSessionContextValue>(
    () => ({
      activeGame,
      latestSessionResult,
      latestSessionReceivedAt,
      lastCompletedGame,
      sessionError,
      startGoNoGoSession,
      startSnakeSession,
      clearSessionError,
    }),
    [
      activeGame,
      clearSessionError,
      lastCompletedGame,
      latestSessionResult,
      latestSessionReceivedAt,
      sessionError,
      startGoNoGoSession,
      startSnakeSession,
    ]
  );

  return (
    <GameSessionContext.Provider value={value}>
      {children}
    </GameSessionContext.Provider>
  );
}
