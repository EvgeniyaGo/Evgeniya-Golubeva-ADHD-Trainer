import { useCallback, useEffect, useRef, useState } from "react";

// --- NUS UUIDs (ESP32 Nordic UART Service) ---
const NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // write / writeWithoutResponse
const NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // notify

// Commands your firmware should understand (text lines keep things easy)
const CMD_START = "START\n";
const CMD_STOP = "STOP\n";

type GattStuff = {
  device: BluetoothDevice;
  server: BluetoothRemoteGATTServer;
  service: BluetoothRemoteGATTService;
  rx: BluetoothRemoteGATTCharacteristic; // write here
  tx: BluetoothRemoteGATTCharacteristic; // notifications from ESP
};

// ---------------- Adaptive difficulty ----------------

const MIN_ROUND_MS = 1200;
const MAX_ROUND_MS = 9000;

const MIN_PAUSE_MS = 2000;
const MAX_PAUSE_MS = 12000;



export default function App() {
  // ─── Connection / cube control state ──────────────────────────────
  const [status, setStatus] = useState<
    "idle" | "connecting" | "connected" | "disconnected"
  >("idle");
  const [name, setName] = useState<string>("");
  const [moving, setMoving] = useState(false);
  const [log, setLog] = useState<string[]>([]);
  const [menuOpen, setMenuOpen] = useState(true);
  const CMD_BEEP1 = "BEEP1\n";
  const CMD_BEEP2 = "BEEP2\n";
  const [gameOn, setGameOn] = useState(false);


  // ─── Packet test state ────────────────────────────────────────────
  const [testRunning, setTestRunning] = useState(false);
  const [nextSeq, setNextSeq] = useState(1);
  const [sentCount, setSentCount] = useState(0);
  const [recvCount, setRecvCount] = useState(0);
  const [avgRttMs, setAvgRttMs] = useState<number | null>(null);

  // ─── Manual command state ─────────────────────────────────────────
  const [command, setCommand] = useState("");
  const RoundPhase = {
    IDLE: "IDLE",
    WAIT_BALANCE: "WAIT_BALANCE",
    PLAYING: "PLAYING",
  } as const;
  type RoundPhase = (typeof RoundPhase)[keyof typeof RoundPhase];
  const [pendingRound, setPendingRound] = useState<PendingRound | null>(null);
  const writeQueueRef = useRef<Promise<void>>(Promise.resolve());

  const [roundPhase, setRoundPhase] = useState<RoundPhase>(RoundPhase.IDLE);
  type RoundType = "ARROW" | "PAUSE"; // later add "OPPOSITE"

  type PendingRound =
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

  const oppositeFace: Record<FaceId, FaceId> = {
    TOP: "BOTTOM",
    BOTTOM: "TOP",
    LEFT: "RIGHT",
    RIGHT: "LEFT",
    FRONT: "BACK",
    BACK: "FRONT",
  };

  // refs
  const gattRef = useRef<GattStuff | null>(null);
  const logBoxRef = useRef<HTMLDivElement | null>(null);
  const pendingRef = useRef<Map<number, number>>(new Map()); // seq -> send timestamp
  const writeBusyRef = useRef(false); // NEW: prevent overlapping GATT writes
  const remainingRoundsRef = useRef<number>(0);
  const pendingRoundRef = useRef<PendingRound | null>(null);
  const roundPhaseRef = useRef<RoundPhase>(RoundPhase.IDLE);
  const roundDurationRef = useRef<number>(800); // default fallback
  const baseDurationRef = useRef<number>(0);   // set on GAME START
  const successStreakRef = useRef<number>(0);
  const failStreakRef = useRef<number>(0);

  type EndRoundFailData = {
    face: FaceId;
    time?: number;
    reason?: string;
  };



  const pushLog = useCallback((line: string) => {
    setLog((prev) => {
      const next = [...prev, `[${new Date().toLocaleTimeString()}] ${line}`];
      return next.slice(-500);
    });
    queueMicrotask(() => {
      const el = logBoxRef.current;
      if (el) el.scrollTop = el.scrollHeight;
    });
  }, []);

  const onDisconnected = useCallback(() => {
    setStatus("disconnected");
    setMoving(false);
    setTestRunning(false);
    pendingRef.current.clear();
    pushLog("[BLE] Disconnected");
    gattRef.current = null;
  }, [pushLog]);

  const connect = useCallback(async () => {
    try {
      if (!navigator.bluetooth) {
        alert("Web Bluetooth not supported in this browser.");
        return;
      }
      setStatus("connecting");

      const device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: "ADHD" }],
        optionalServices: [NUS_SERVICE],
      });

      setName(device.name || "(no name)");
      device.addEventListener("gattserverdisconnected", onDisconnected);

      const server = await device.gatt!.connect();
      const service = await server.getPrimaryService(NUS_SERVICE);
      const tx = await service.getCharacteristic(NUS_TX);
      const rx = await service.getCharacteristic(NUS_RX);

      // Subscribe to notifications from ESP (NUS TX)
      await tx.startNotifications();
      tx.addEventListener("characteristicvaluechanged", async (ev: Event) => {
        const dv = (ev.target as BluetoothRemoteGATTCharacteristic).value!;
        try {
          // Safer decode (respect byteOffset/byteLength)
          const bytes = new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength);
          const text = new TextDecoder().decode(bytes).trimEnd();
          pushLog(`[ESP →] ${text}`);



          // Split just in case multiple lines come in one notification
          for (const line of text.split(/\r?\n/)) {
            if (!line) continue;

            // ───────────────── ROUND BALANCE ─────────────────

            if (line.startsWith("ROUND BALANCE")) {
              const round = pendingRoundRef.current;
              if (!round) {
                console.warn("[SRV] BALANCE but no pending round");
                continue;
              }

              setRoundPhase(RoundPhase.PLAYING);
              roundPhaseRef.current = RoundPhase.PLAYING;

              const parts = line.split(/\s+/);
              const sidePart = parts.find(p => p.toLowerCase().startsWith("side="));
              if (!sidePart) continue;

              const balancedFace = sidePart.split("=")[1] as FaceId;

              await writeLine("CLEAR ALL\n");

              if (round.type === "ARROW") {
                // draw arrow on balancedFace, and target on "to"
                const intended = pendingRoundRef.current;
                const isOpposite = intended?.type === "ARROW" && intended.mode === "OPPOSITE";
                //    const visualTarget = isOpposite
                //      ? oppositeFace[round.to]
                //      : round.to;

                // Arrow still drawn on balanced face, but points wrong
                await writeLine(`DRAW SHAPE ${balancedFace} ${round.arrow} COLOR_BLUE\n`);

                // Circle lies only in opposite mode
                await writeLine(
                  //      `DRAW SHAPE ${visualTarget} SHAPE_CIRCLE_6X6 COLOR_GREEN\n`
                  `DRAW SHAPE ${round.to} SHAPE_CIRCLE_6X6 COLOR_GREEN\n`
                );
                if (isOpposite) {
                  // deceptive / darker cue
                  await writeLine("BEEP freq=1000 dur=400\n");
                } else {
                  // normal cue
                  await writeLine("BEEP freq=1200 dur=200\n");
                }

              } else {
                // PAUSE: no arrow, no circle
                // countdown is handled by ESP firmware display_control on lock/start
                console.log(`[SRV] PAUSE round active on ${balancedFace} for ${round.duration}ms`);
              }

              //              pendingRoundRef.current = null;
              //              setPendingRound(null);
              continue;
            }

            // ───────────────── END ROUND ─────────────────
            if (line.startsWith("END ROUND")) {
              const data = parseEndRound(line);
              if (!data) {
                console.warn("[SRV] Bad END ROUND format");
                continue;
              }

              if (roundPhaseRef.current !== RoundPhase.PLAYING && roundPhaseRef.current !== RoundPhase.WAIT_BALANCE) {
                console.warn("[SRV] Ignoring END ROUND (not playing yet)");
                continue;
              }

              if (data.result === "SUCCESS") {
                console.log(
                  `[SRV] ROUND SUCCESS face=${data.face} time=${data.time}`
                );
                updateAdaptiveDuration("SUCCESS");
                await handleEndRound(data.face);
              } else {
                console.log(
                  `[SRV] ROUND FAIL face=${data.face} reason=${data.reason}`
                );
                updateAdaptiveDuration("FAIL");
                await handleRoundFail(data);
              }

              continue;
            }

            if (line.startsWith("OK GAME START")) {
              const parts = line.split(/\s+/);
              const facePart = parts.find(p => p.toLowerCase().startsWith("face="));
              if (!facePart) return;

              const startFace = facePart.split("=")[1] as FaceId;

              console.log("[SRV] GAME START anchored on", startFace);

              // Start first round from THIS face
              const firstRound = chooseNextRound(
                startFace,
                remainingRoundsRef.current
              );

              pendingRoundRef.current = firstRound;
              setPendingRound(firstRound);

              roundPhaseRef.current = RoundPhase.WAIT_BALANCE;
              setRoundPhase(RoundPhase.WAIT_BALANCE);

              await writeLine(roundStartLine(firstRound));
              return;
            }


            // ───────────────── PONG ─────────────────
            if (line.startsWith("PONG ")) {
              const seqStr = line.substring(5).trim();
              const seq = Number(seqStr);
              if (!Number.isNaN(seq)) {
                const sentAt = pendingRef.current.get(seq);
                if (sentAt != null) {
                  const rtt = performance.now() - sentAt;
                  pendingRef.current.delete(seq);

                  setRecvCount(prevRecv => {
                    const newRecv = prevRecv + 1;
                    setAvgRttMs(prevAvg =>
                      prevAvg == null
                        ? rtt
                        : prevAvg + (rtt - prevAvg) / newRecv
                    );
                    return newRecv;
                  });
                }
              }
            }
          }


        } catch {
          const hex = Array.from(
            new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength)
          )
            .map((b) => b.toString(16).padStart(2, "0"))
            .join(" ");
          pushLog(`[ESP →] [${hex}]`);
        }
      });

      gattRef.current = { device, server, service, rx, tx };
      setStatus("connected");
      pushLog(`[BLE] Connected to ${device.name || "(no name)"}`);
    } catch (e: any) {
      setStatus("idle");
      pushLog(`[ERR] ${e?.message || e}`);
    }
  }, [pushLog, onDisconnected]);

  const disconnect = useCallback(async () => {
    try {
      const g = gattRef.current;
      setMoving(false);
      setTestRunning(false);
      pendingRef.current.clear();
      if (g?.device.gatt?.connected) g.device.gatt.disconnect();
      gattRef.current = null;
      setStatus("idle");
      pushLog("[BLE] Manual disconnect");
    } catch (e: any) {
      pushLog(`[ERR] ${e?.message || e}`);
    }
  }, [pushLog]);

  // Write helper (handles writeWith/WithoutResponse differences)
  // Returns true if the write actually happened, false on failure / busy
  const writeLine = useCallback((line: string): Promise<void> => {
    const g = gattRef.current;
    if (!g) {
      pushLog("Write failed: not connected");
      return Promise.resolve();
    }

    const data = new TextEncoder().encode(line);

    writeQueueRef.current = writeQueueRef.current.then(async () => {
      try {
        const rx: any = g.rx;

        if (typeof rx.writeValueWithoutResponse === "function") {
          await rx.writeValueWithoutResponse(data);
        } else if (typeof rx.writeValueWithResponse === "function") {
          await rx.writeValueWithResponse(data);
        } else {
          await rx.writeValue(data);
        }

        pushLog(`[→ ESP] ${JSON.stringify(line.trimEnd())}`);
      } catch (e: any) {
        pushLog(`[ERR] ${e?.message || e}`);
      }
    });

    return writeQueueRef.current;
  }, [pushLog]);

  const toggleMovement = useCallback(async () => {
    const next = !moving;
    setMoving(next);
    await writeLine(next ? CMD_START : CMD_STOP);
  }, [moving, writeLine]);

  const adjacency: Record<FaceId, FaceId[]> = {
    TOP: ["FRONT", "BACK", "LEFT", "RIGHT"],
    BOTTOM: ["FRONT", "BACK", "LEFT", "RIGHT"],
    LEFT: ["TOP", "BOTTOM", "FRONT", "BACK"],
    RIGHT: ["TOP", "BOTTOM", "FRONT", "BACK"],
    FRONT: ["TOP", "BOTTOM", "LEFT", "RIGHT"],
    BACK: ["TOP", "BOTTOM", "LEFT", "RIGHT"],
  };

  const arrowFromToShort = useCallback((from: FaceId, to: FaceId): ShapeId => {
    return arrowFromTo(from, to);
  }, []);

  const randInt = (min: number, max: number) =>
    Math.floor(min + Math.random() * (max - min + 1));

  function chooseNextRound(from: FaceId, remaining: number): PendingRound {
    const choosePause = Math.random() < 0.5;

    if (choosePause) {
      const base = baseDurationRef.current || 3000;

      const pauseMax = Math.min(
        MAX_PAUSE_MS,
        Math.max(
          MIN_PAUSE_MS,
          base + base * (Math.random() * 0.5 - 0.2) // -20% .. +30%
        )
      );

      const pauseDuration = Math.round(
        MIN_PAUSE_MS + Math.random() * (pauseMax - MIN_PAUSE_MS)
      );

      console.log(
        `[SRV] PAUSE duration=${pauseDuration}ms (base=${base}ms)`
      );

      return {
        type: "PAUSE",
        duration: pauseDuration,
        remaining,
      };
    }

    const options = adjacency[from];
    const to = options[Math.floor(Math.random() * options.length)];
    const arrow = arrowFromTo(from, to);

    const mode: "NORMAL" | "OPPOSITE" =
      Math.random() < 0.5 ? "OPPOSITE" : "NORMAL";

    return {
      type: "ARROW",
      mode,
      from,
      to,
      arrow,
      duration: baseDurationRef.current,
      remaining,
    };
  }

  function updateAdaptiveDuration(result: "SUCCESS" | "FAIL") {
    const base = baseDurationRef.current || 3000;

    // random 5–15%
    const pct = 0.05 + Math.random() * 0.10;

    let streakBonus = 0;

    if (result === "SUCCESS") {
      successStreakRef.current++;
      failStreakRef.current = 0;
      streakBonus = Math.min(successStreakRef.current * 0.02, 0.10);
    } else {
      failStreakRef.current++;
      successStreakRef.current = 0;
      streakBonus = Math.min(failStreakRef.current * 0.02, 0.10);
    }

    const effectivePct = pct + streakBonus;

    let next =
      result === "SUCCESS"
        ? base * (1 - effectivePct)
        : base * (1 + effectivePct);

    next = Math.round(
      Math.min(MAX_ROUND_MS, Math.max(MIN_ROUND_MS, next))
    );

    console.log(
      `[SRV] ADAPT ${result}: ${base}ms → ${next}ms (pct=${Math.round(
        effectivePct * 100
      )}%)`
    );

    baseDurationRef.current = next;
  }

  function roundStartLine(round: PendingRound): string {
    if (round.type === "PAUSE") {
      return `ROUND START type=PAUSE duration=${round.duration} remaining=${round.remaining}\n`;
    }

    // ARROW
    const isOpposite =
      round.type === "ARROW" && round.mode === "OPPOSITE";

    const expected = isOpposite
      ? oppositeFace[round.to]
      : round.to;

    return `ROUND START type=ARROW from=${round.from} to=${round.to} expected=${expected} duration=${round.duration} remaining=${round.remaining}\n`;
  }

  function onRoundBalanced(balancedFace: FaceId) {
    const round = pendingRoundRef.current;
    if (!round) {
      console.warn("[SRV] BALANCE but no pending round");
      return;
    }

    // Clear any previous visuals
    writeLine("CLEAR ALL\n");

    // ---- 1. Balance indicator (neutral, always shown) ----
    writeLine(
      `DRAW SHAPE ${balancedFace} SHAPE_SQUARE_2X2 COLOR_WHITE\n`
    );

    // ---- 2. Draw round-specific visuals ----
    if (round.type === "ARROW") {
      const isOpposite = round.mode === "OPPOSITE";

      // Arrow always on balanced face
      writeLine(
        `DRAW SHAPE ${balancedFace} ${round.arrow} COLOR_BLUE\n`
      );

      // Circle ALWAYS on visual intent (round.to)
      writeLine(
        `DRAW SHAPE ${round.to} SHAPE_CIRCLE_6X6 COLOR_GREEN\n`
      );

      // Optional audio cue (app-driven)
      if (isOpposite) {
        writeLine("BEEP freq=600 dur=300\n");
      } else {
        writeLine("BEEP freq=1200 dur=120\n");
      }
    }

    // PAUSE:
    // no arrow, no circle, just ESP countdown running
  }


  const handleEndRound = useCallback(
    async (from: FaceId) => {
      // Decrease remaining rounds
      remainingRoundsRef.current -= 1;

      if (remainingRoundsRef.current <= 0) {
        console.log("[SRV] Game finished");
        setRoundPhase(RoundPhase.IDLE);
        roundPhaseRef.current = RoundPhase.IDLE;
        pendingRoundRef.current = null;
        setPendingRound(null);
        await writeLine("GAME END\n");
        return;
      }

      // Choose next target
      const options = adjacency[from];
      if (!options || options.length === 0) {
        console.warn("[SRV] No adjacency options for", from);
        setRoundPhase(RoundPhase.IDLE);
        roundPhaseRef.current = RoundPhase.IDLE;
        return;
      }

      const to = options[Math.floor(Math.random() * options.length)];
      const arrow = arrowFromToShort(from, to);

      const nextRound = chooseNextRound(from, remainingRoundsRef.current);

      console.log(`[SRV] NEXT ROUND type=${nextRound.type} remaining=${remainingRoundsRef.current}`);

      pendingRoundRef.current = nextRound;
      setPendingRound(nextRound);

      setRoundPhase(RoundPhase.WAIT_BALANCE);
      roundPhaseRef.current = RoundPhase.WAIT_BALANCE;

      await writeLine(roundStartLine(nextRound));
    },
    [writeLine, arrowFromToShort]
  );


  // ─── Manual command sender ──────────────────────────────────────
  const sendCommand = useCallback(async () => {
    const raw = command.trim();
    if (!raw) return;

    const upper = raw.toUpperCase();



    // ── INTERCEPT SEMANTIC ARROW ─────────────────────────
    // Expected:
    // DRAW ARROW FACE_TOP FACE_LEFT
    if (upper.startsWith("ROUND ARROW")) {
      // DO NOT send to ESP
      await handleRoundArrow(upper);
      setCommand("");
      return;
    }
    // ── INTERCEPT GAME START (AUTHORITATIVE) ───────────────
    if (upper.startsWith("GAME START")) {
      const params = Object.fromEntries(
        upper.split(/\s+/).slice(2).map(p => {
          const [k, v] = p.split("=");
          return [k.toLowerCase(), v];
        })
      );

      const remaining = params.remaining
        ? Number(params.remaining)
        : 1;

      const duration = params.duration
        ? Number(params.duration)
        : roundDurationRef.current;

      // Store server-owned state
      remainingRoundsRef.current = remaining;
      roundDurationRef.current = duration;

      baseDurationRef.current = duration; // duration from GAME START
      successStreakRef.current = 0;
      failStreakRef.current = 0;

      console.log(`[SRV] GAME START baseDuration=${duration}ms`);


      console.log(
        "[SRV] GAME START remaining=",
        remaining,
        "duration=",
        duration
      );

      // Forward simplified command to ESP
      await writeLine("GAME START type=SIMON\n");

      setCommand("");
      return;
    }

    // ── INTERCEPT ROUND START (AUTHORITATIVE) ───────────
    if (upper.startsWith("ROUND START")) {
      const params = Object.fromEntries(
        upper.split(/\s+/).slice(2).map(p => {
          const [k, v] = p.split("=");
          return [k.toLowerCase(), v];
        })
      );

      const remaining = params.remaining ? Number(params.remaining) : 1;
      const duration = params.duration ? Number(params.duration) : baseDurationRef.current;

      remainingRoundsRef.current = remaining;
      roundDurationRef.current = duration;

      if (params.type === "PAUSE") {
        const round: PendingRound = { type: "PAUSE", duration, remaining };
        pendingRoundRef.current = round;
        //    setPendingRound(null);
        setPendingRound(round);
        setRoundPhase(RoundPhase.WAIT_BALANCE);
        roundPhaseRef.current = RoundPhase.WAIT_BALANCE;
      }

      if (params.type === "ARROW" && params.from && params.to) {
        const from = params.from as FaceId;
        const to = params.to as FaceId;
        const arrow = arrowFromTo(from, to);
        const mode =
          params.mode === "OPPOSITE" ? "OPPOSITE" : "NORMAL";

        const round: PendingRound = { type: "ARROW", mode, from, to, arrow, duration, remaining };
        pendingRoundRef.current = round;
        setPendingRound(round);
        setRoundPhase(RoundPhase.WAIT_BALANCE);
        roundPhaseRef.current = RoundPhase.WAIT_BALANCE;
      }

      const line = raw.endsWith("\n") ? raw : raw + "\n";
      await writeLine(line);
      setCommand("");
      return;
    }

    if (upper.startsWith("DRAW ARROW")) {
      const parts = upper.split(/\s+/);

      if (parts.length !== 4) {
        pushLog("[ERR] Bad DRAW ARROW format");
        return;
      }

      const from = parts[2] as FaceId;
      const to = parts[3] as FaceId;

      const arrow = arrowFromTo(from, to);

      // Send ONLY explicit commands to ESP
      console.log(`[SRV] QUEUE NEW ROUND ${from} → ${to} (${arrow})`);

      // Store intent, DO NOT DRAW YET
      // Store intent, DO NOT DRAW YET (manual ARROW round)
      const round: PendingRound = {
        type: "ARROW",
        mode: "NORMAL",
        from,
        to,
        arrow,
        duration: baseDurationRef.current,
        remaining: remainingRoundsRef.current || 1,
      };

      pendingRoundRef.current = round;

      // UI arrow preview ONLY
      setPendingRound(round);

      // Ask ESP to start balancing for next round
      await writeLine(
        `ROUND START type=SIMON duration=${baseDurationRef.current} want_locked=1 allow_side_change=0\n`
      );

      pushLog(`[SRV] DRAW ARROW ${from} → ${to} (${arrow})`);
      setCommand("");
      return;
    }


    // ── DEFAULT PASSTHROUGH ──────────────────────────────
    const line = raw.endsWith("\n") ? raw : raw + "\n";
    await writeLine(line);
    setCommand("");
  }, [command, writeLine, pushLog]);


  // ----- Calculating faces for arrows
  type FaceId =
    | "TOP"
    | "BOTTOM"
    | "LEFT"
    | "RIGHT"
    | "FRONT"
    | "BACK";

  type ShapeId =
    | "SHAPE_ARROW_UP"
    | "SHAPE_ARROW_DOWN"
    | "SHAPE_ARROW_LEFT"
    | "SHAPE_ARROW_RIGHT"
    | "SHAPE_CIRCLE_6X6";
  type Vec3 = { x: number; y: number; z: number };

function faceNormal(f: FaceId): Vec3 {
  switch (f) {
    case "TOP":    return { x: 0, y: 0, z: 1 };
    case "BOTTOM": return { x: 0, y: 0, z: -1 };
    case "FRONT":  return { x: 1, y: 0, z: 0 };
    case "BACK":   return { x: -1, y: 0, z: 0 };
    case "LEFT":   return { x: 0, y: 1, z: 0 };
    case "RIGHT":  return { x: 0, y: -1, z: 0 };
  }
}

function faceBasis(f: FaceId): { up: Vec3; right: Vec3 } {
  switch (f) {
    case "TOP":
      // Looking down +Z
      return {
        up:    { x: 0, y: 1, z: 0 },   // +Y → LEFT
        right: { x: 1, y: 0, z: 0 },   // +X → FRONT
      };

    case "BOTTOM":
      // Looking down -Z
      return {
        up:    { x: 0, y: -1, z: 0 },   // still +Y
        right: { x: -1, y: 0, z: 0 },  // flipped X
      };

    case "FRONT":
      // Looking down +X
      return {
        up:    { x: 0, y: 0, z: 1 },   // +Z → UP
        right: { x: 0, y: -1, z: 0 },  // -Y → RIGHT
      };

    case "BACK":
      // Looking down -X
      return {
        up:    { x: 0, y: 0, z: 1 },   // +Z → UP
        right: { x: 0, y: 1, z: 0 },   // +Y → LEFT
      };

    case "LEFT":
      // Looking down +Y
      return {
        up:    { x: 0, y: 0, z: 1 },   // +Z → UP
        right: { x: 1, y: 0, z: 0 },   // +X → FRONT
      };

    case "RIGHT":
      // Looking down -Y
      return {
        up:    { x: 0, y: 0, z: 1 },   // +Z → UP
        right: { x: -1, y: 0, z: 0 },  // -X → BACK
      };
  }
}
  function arrowFromTo(from: FaceId, to: FaceId): ShapeId {
    const n = faceNormal(to);
    const { up, right } = faceBasis(from);

    const du = n.x * up.x + n.y * up.y + n.z * up.z;
    const dr = n.x * right.x + n.y * right.y + n.z * right.z;

    if (du === 1) return "SHAPE_ARROW_UP";
    if (du === -1) return "SHAPE_ARROW_DOWN";
    if (dr === 1) return "SHAPE_ARROW_RIGHT";
    if (dr === -1) return "SHAPE_ARROW_LEFT";

    throw new Error(`Unreachable arrow ${from} → ${to}`);
  }

  function parseEndRound(line: string): {
    result: "SUCCESS" | "FAIL";
    face: FaceId;
    time?: number;
    reason?: string;
  } | null {
    const parts = line.split(/\s+/);

    // Legacy: END ROUND RIGHT
    if (parts.length === 3) {
      return {
        result: "SUCCESS",
        face: parts[2] as FaceId,
      };
    }

    // New: key=value format
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

  const handleRoundFail = useCallback(
    async (data: EndRoundFailData) => {
      console.log(
        `[SRV] HANDLE FAIL face=${data.face} time=${data.time} reason=${data.reason}`
      );

      // Consume one round
      remainingRoundsRef.current -= 1;

      if (remainingRoundsRef.current <= 0) {
        console.log("[SRV] Game finished (after FAIL)");
        roundPhaseRef.current = RoundPhase.IDLE;
        setRoundPhase(RoundPhase.IDLE);
        pendingRoundRef.current = null;
        setPendingRound(null);
        await writeLine("GAME END\n");
        return;
      }

      // Choose next round (same logic as SUCCESS)
      const options = adjacency[data.face];
      if (!options || options.length === 0) {
        console.warn("[SRV] No adjacency options for", data.face);
        roundPhaseRef.current = RoundPhase.IDLE;
        setRoundPhase(RoundPhase.IDLE);
        return;
      }

      const to = options[Math.floor(Math.random() * options.length)];
      const arrow = arrowFromToShort(data.face, to);

      const nextRound = chooseNextRound(data.face, remainingRoundsRef.current);

      pendingRoundRef.current = nextRound;
      setPendingRound(nextRound);

      roundPhaseRef.current = RoundPhase.WAIT_BALANCE;
      setRoundPhase(RoundPhase.WAIT_BALANCE);

      await writeLine(roundStartLine(nextRound));
    },
    [writeLine, arrowFromToShort]
  );

  async function handleRoundArrow(line: string) {
    const parts = line.split(/\s+/);

    if (parts.length !== 4) {
      console.error("Bad ROUND ARROW format");
      return;
    }

    const from = parts[2] as FaceId;
    const to = parts[3] as FaceId;

    const arrow = arrowFromTo(from, to);

    // Store intent, DO NOT DRAW YET (manual ARROW round)
    const round: PendingRound = {
      type: "ARROW",
      from,
      to,
      arrow,
      duration: baseDurationRef.current,
      remaining: remainingRoundsRef.current || 1,
    };

    pendingRoundRef.current = round;

    // UI arrow preview ONLY
    setPendingRound(round);

    // Tell ESP to start balancing
    await writeLine(
      `ROUND START type=SIMON duration=${baseDurationRef.current} want_locked=1 allow_side_change=0\n`
    );

    console.log(`[SRV] MANUAL ROUND START ${from} → ${to} (${arrow})`);
  }

  // ─── Packet test: send PING <seq>\n periodically while running ───
  useEffect(() => {
    if (!testRunning || status !== "connected") return;

    const intervalMs = 100; // a bit less aggressive; can tune later
    const id = window.setInterval(() => {
      // Optional safety: don't overload if many are in-flight
      if (pendingRef.current.size > 10) {
        return;
      }

      setNextSeq((prevSeq) => {
        const seq = prevSeq;
        const line = `PING ${seq}\n`;

        (async () => {
          await writeLine(line);

          pendingRef.current.set(seq, performance.now());
          setSentCount((c) => c + 1);
        })();

        return seq + 1;
      });
    }, intervalMs);

    return () => window.clearInterval(id);
  }, [testRunning, status, writeLine]);

  // tidy up on unmount
  useEffect(
    () => () => {
      try {
        gattRef.current?.device.removeEventListener(
          "gattserverdisconnected",
          onDisconnected
        );
      } catch {
        // ignore
      }
    },
    [onDisconnected]
  );

  const isConnected = status === "connected";
  const statusText =
    status === "connecting"
      ? "CONNECTING…"
      : isConnected
        ? "CONNECTED"
        : status === "disconnected"
          ? "DISCONNECTED"
          : "NOT CONNECTED";

  // helper: restart ping both on app + ESP
  const restartPingAll = useCallback(() => {
    // reset app-side stats
    setSentCount(0);
    setRecvCount(0);
    setAvgRttMs(null);
    pendingRef.current.clear();
    setNextSeq(1);
    // ask ESP to reset its counters
    if (isConnected) {
      void writeLine("RESTART PING\n");
    }
  }, [isConnected, writeLine]);

  // ─── UI ────────────────────────────────────────────────────────────
  return (
    <div className={`app-frame ${menuOpen ? "menu-open" : "menu-closed"}`}>
      <div className="board">
        {/* ─── TOP MENU BAR ─────────────────────────────── */}
        <header className="board-header">
          {/* LEFT: menu icon */}
          <button
            className="menu-toggle top-icon"
            aria-pressed={menuOpen}
            aria-label={menuOpen ? "Collapse menu" : "Expand menu"}
            title={menuOpen ? "Collapse menu" : "Expand menu"}
            onClick={() => setMenuOpen((m) => !m)}
          >
            <svg width="26" height="26" viewBox="0 0 24 24" fill="none">
              <path
                d="M3 7h18M3 12h18M3 17h18"
                stroke="#697586"
                strokeWidth="2"
                strokeLinecap="round"
              />
            </svg>
          </button>

          {/* RIGHT: status chip + account icon */}
          <div className="top-right">
            <div
              className="status-chip"
              role="button"
              tabIndex={0}
              onClick={() => (isConnected ? disconnect() : connect())}
              onKeyDown={(e) => {
                if (e.key === "Enter" || e.key === " ") {
                  e.preventDefault();
                  isConnected ? disconnect() : connect();
                }
              }}
              title={isConnected ? "Disconnect" : "Connect"}
            >
              <span className={`dot ${isConnected ? "on" : ""}`} />
              <span className="status-text">
                {statusText}
                {name && isConnected ? ` — ${name}` : ""}
              </span>
            </div>

            <button
              className="menu-toggle top-icon account-btn"
              aria-label="Account"
              title="Account"
            >
              <svg width="26" height="26" viewBox="0 0 24 24" fill="none">
                <circle
                  cx="12"
                  cy="8"
                  r="4"
                  stroke="#697586"
                  strokeWidth="2"
                />
                <path
                  d="M4 20c1.8-3.2 4.7-5 8-5s6.2 1.8 8 5"
                  stroke="#697586"
                  strokeWidth="2"
                  strokeLinecap="round"
                />
              </svg>
            </button>
          </div>
        </header>

        {/* ─── BODY: sidebar (left) + main content ───────── */}
        <div className="board-shell">
          <aside className={`sidebar ${menuOpen ? "open" : "closed"}`}>
            <div className="sidebar-head">Menu</div>
            <nav className="nav">
              <button className="nav-item is-active" title="Dashboard">
                {menuOpen ? (
                  "Dashboard"
                ) : (
                  <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                    <path
                      d="M3 13h8V3H3v10zm10 8h8v-6h-8v6zM3 21h8v-6H3v6zm10-8h8V3h-8v10z"
                      fill="#697586"
                    />
                  </svg>
                )}
              </button>

              <button className="nav-item" title="Connect">
                {menuOpen ? (
                  "Connect"
                ) : (
                  <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                    <path
                      d="M7 8h5a4 4 0 110 8H7"
                      stroke="#697586"
                      strokeWidth="2"
                      strokeLinecap="round"
                    />
                    <path
                      d="M7 12h6"
                      stroke="#697586"
                      strokeWidth="2"
                      strokeLinecap="round"
                    />
                  </svg>
                )}
              </button>

              <button className="nav-item" title="LED Control">
                {menuOpen ? (
                  "LED Control"
                ) : (
                  <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                    <circle
                      cx="12"
                      cy="12"
                      r="5"
                      stroke="#697586"
                      strokeWidth="2"
                    />
                    <path
                      d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M19.1 4.9L17 7M7 17l-2.1 2.1"
                      stroke="#697586"
                      strokeWidth="2"
                      strokeLinecap="round"
                    />
                  </svg>
                )}
              </button>

              <button className="nav-item" title="Logs">
                {menuOpen ? (
                  "Logs"
                ) : (
                  <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                    <path
                      d="M5 4h14M5 8h14M5 12h10M5 16h8"
                      stroke="#697586"
                      strokeWidth="2"
                      strokeLinecap="round"
                    />
                  </svg>
                )}
              </button>

              <button className="nav-item" title="Settings">
                {menuOpen ? (
                  "Settings"
                ) : (
                  <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                    <path
                      d="M12 15.5a3.5 3.5 0 100-7 3.5 3.5 0 000 7z"
                      stroke="#697586"
                      strokeWidth="2"
                    />
                    <path
                      d="M19.4 15a7.97 7.97 0 00.1-2l2-1-2-3-2 1a8.14 8.14 0 00-1.7-1l-.3-2.3h-4l-.3-2.3a8.14 8.14 0 00-1.7 1l-2-1-2 3 2 1a8.5 8.5 0 000 2l-2 1 2 3 2-1c.5.4 1.1.7 1.7 1l.3 2.3h4l.3-2.3c.6-.3 1.2-.6 1.7-1l2 1 2-3-2-1z"
                      stroke="#697586"
                      strokeWidth="2"
                      strokeLinecap="round"
                      strokeLinejoin="round"
                    />
                  </svg>
                )}
              </button>

              <div className="divider" />

              <button className="nav-item" title="About">
                {menuOpen ? (
                  "About"
                ) : (
                  <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
                    <circle
                      cx="12"
                      cy="12"
                      r="10"
                      stroke="#697586"
                      strokeWidth="2"
                    />
                    <path
                      d="M12 8h.01M11 12h2v5h-2z"
                      stroke="#697586"
                      strokeWidth="2"
                      strokeLinecap="round"
                    />
                  </svg>
                )}
              </button>
            </nav>
          </aside>

          {/* ─── MAIN CONTENT ─────────────────────────────── */}
          <main className="board-content">
            <div className="stack center">
              <div className="status">
                {statusText}
                {name && isConnected ? ` — ${name}` : ""}
              </div>

              <div className="lead">
                Connect to your cube, then start or stop LED movement. Live
                results, packet test, and manual commands appear below.
              </div>

              <div className="actions subtle">
                <button
                  className="btn btn-neutral"
                  onClick={toggleMovement}
                  disabled={!isConnected}
                >
                  {moving ? "Stop LED movement" : "Start LED movement"}
                </button>

                <button
                  className="btn btn-neutral"
                  onClick={() => writeLine(CMD_BEEP1)}
                  disabled={!isConnected}
                  title="Play 880 Hz beep"
                >
                  Beep 1
                </button>

                <button
                  className="btn btn-neutral"
                  onClick={() => writeLine(CMD_BEEP2)}
                  disabled={!isConnected}
                  title="Play 1760 Hz beep"
                >
                  Beep 2
                </button>
                <br></br>
                <button
                  className="btn btn-neutral"
                  disabled={!isConnected}
                  onClick={() => {
                    const next = !gameOn;
                    setGameOn(next);
                    writeLine(`GAME ${next ? 1 : 0}`);
                  }}
                >
                  {gameOn ? "Stop game" : "Start game"}
                </button>

                <button
                  className="btn btn-neutral"
                  disabled={!isConnected || !gameOn}
                  onClick={() => writeLine("NEW_FACE")}
                >
                  New face
                </button>

              </div>


              <div className="card">
                <h3>Results from ESP</h3>
                <div ref={logBoxRef} className="log">
                  {log.length ? log.join("\n") : "— No messages yet —"}
                </div>
              </div>

              <div className="card">
                <h3>Packet test</h3>
                <div className="actions subtle">
                  <button
                    className="btn btn-neutral"
                    disabled={!isConnected}
                    onClick={() => {
                      if (!testRunning) {
                        // reset stats on start
                        setSentCount(0);
                        setRecvCount(0);
                        setAvgRttMs(null);
                        pendingRef.current.clear();
                        setNextSeq(1);
                      }
                      setTestRunning((r) => !r);
                    }}
                  >
                    {testRunning ? "Stop test" : "Start packet test"}
                  </button>
                </div>

                <div className="metrics">
                  <div>Total sent: {sentCount}</div>
                  <div>Total received: {recvCount}</div>
                  <div>
                    Lost: {sentCount - recvCount}{" "}
                    {sentCount > 0 && (
                      <>
                        (
                        {(
                          ((sentCount - recvCount) / sentCount) *
                          100
                        ).toFixed(2)}
                        %)
                      </>
                    )}
                  </div>
                  <div>
                    Avg RTT:{" "}
                    {avgRttMs != null ? `${avgRttMs.toFixed(1)} ms` : "—"}
                  </div>
                  <div>In-flight: {pendingRef.current.size}</div>
                </div>

                <div className="actions subtle" style={{ marginTop: "0.75rem" }}>
                  <button
                    className="btn btn-neutral"
                    disabled={!isConnected}
                    onClick={restartPingAll}
                  >
                    Restart ping (ESP + App)
                  </button>
                </div>
              </div>

              <div className="card">
                <h3>Manual command</h3>
                <p className="muted">
                  Send raw commands like <code>SET 0 1</code>,{" "}
                  <code>SET 3 0</code>, <code>RESTART PING</code>, etc.
                </p>
                <div className="command-row">
                  <input
                    type="text"
                    className="command-input"
                    placeholder='e.g. SET 0 1 or RESTART PING'
                    value={command}
                    onChange={(e) => setCommand(e.target.value)}
                    disabled={!isConnected}
                    onKeyDown={(e) => {
                      if (e.key === "Enter") {
                        e.preventDefault();
                        void sendCommand();
                      }
                    }}
                  />
                  <button
                    className="btn btn-neutral"
                    onClick={sendCommand}
                    disabled={!isConnected || !command.trim()}
                  >
                    Send
                  </button>
                </div>
              </div>
            </div>
          </main>
        </div>
      </div>
    </div>
  );
}
