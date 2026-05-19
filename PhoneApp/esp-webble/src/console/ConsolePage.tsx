import { useCallback, useEffect, useRef, useState } from "react";
import { useBle } from "../ble/useBle";
import { AppFrame } from "../components/layout/AppFrame";
import { LogCard } from "./components/LogCard";
import { ManualCommandCard } from "./components/ManualCommandCard";
import { PacketTestCard } from "./components/PacketTestCard";
import {
  MAX_PAUSE_MS,
  MAX_ROUND_MS,
  MIN_PAUSE_MS,
  MIN_ROUND_MS,
} from "./protocol/constants";
import { arrowFromTo } from "./protocol/faces";
import { parseEndRound, parseSessionEnd } from "./protocol/parsing";
import { roundStartLine } from "./protocol/rounds";
import {
  RoundPhase,
  type EndRoundFailData,
  type FaceId,
  type PendingRound,
  type SessionResult,
  type ShapeId,
} from "./protocol/types";

export default function ConsolePage() {
  const { gatt, status, deviceName } = useBle();
  const [log, setLog] = useState<string[]>([]);


  // Packet test state
  const [testRunning, setTestRunning] = useState(false);
  const [, setNextSeq] = useState(1);
  const [sentCount, setSentCount] = useState(0);
  const [recvCount, setRecvCount] = useState(0);
  const [avgRttMs, setAvgRttMs] = useState<number | null>(null);

  // Command state
  const [command, setCommand] = useState("");
  const [, setPendingRound] = useState<PendingRound | null>(null);
  const writeQueueRef = useRef<Promise<void>>(Promise.resolve());

  // Round/game state
  const [, setRoundPhase] = useState<RoundPhase>(RoundPhase.IDLE);
  const [, setLatestSessionResult] = useState<SessionResult | null>(null);

  // Runtime refs
  const logBoxRef = useRef<HTMLDivElement | null>(null);
  const pendingRef = useRef<Map<number, number>>(new Map()); // seq -> send timestamp
  const packetTestIntervalRef = useRef<number | null>(null);
  const nextSeqRef = useRef(1);
  const remainingRoundsRef = useRef<number>(0);
  const pendingRoundRef = useRef<PendingRound | null>(null);
  const roundPhaseRef = useRef<RoundPhase>(RoundPhase.IDLE);
  const roundDurationRef = useRef<number>(800); // default fallback
  const baseDurationRef = useRef<number>(0);   // set on GAME START
  const successStreakRef = useRef<number>(0);
  const failStreakRef = useRef<number>(0);
  const requestedGameRef = useRef<"goNoGo" | "snake" | null>(null);
  const latestSessionResultRef = useRef<SessionResult | null>(null);

  // BLE helpers
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

  /*
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

      // Notification handling
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

            // Round balance notification
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

                // Arrow still drawn on balanced face, but points wrong
                await writeLine(`DRAW SHAPE ${balancedFace} ${round.arrow} COLOR_BLUE\n`);

                // Circle lies only in opposite mode
                await writeLine(
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

              continue;
            }

            // End round notification
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


            // Packet test response
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
  */

  // Write helper (handles writeWith/WithoutResponse differences)
  // Returns true if the write actually happened, false on failure / busy
  const writeLine = useCallback((line: string): Promise<void> => {
    if (!gatt) {
      pushLog("Write failed: not connected");
      return Promise.resolve();
    }

    const data = new TextEncoder().encode(line);

    writeQueueRef.current = writeQueueRef.current.then(async () => {
      try {
        const rx: any = gatt.rx;

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
  }, [gatt, pushLog]);

  // Round/game helpers
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

  const writeGameStart = useCallback(
    async (game: "goNoGo" | "snake") => {
      const line =
        game === "snake"
          ? "GAME START type=SNAKE\n"
          : "GAME START type=SIMON\n";

      pushLog(
        `[DBG] requestedGameRef=${requestedGameRef.current ?? "null"} outgoing=${JSON.stringify(
          line.trimEnd()
        )}`
      );
      await writeLine(line);
    },
    [pushLog, writeLine]
  );

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


  // Command handlers
  const sendCommand = useCallback(async () => {
    const raw = command.trim();
    if (!raw) return;

    const upper = raw.toUpperCase();



    // Intercept semantic arrow command
    // Expected:
    // DRAW ARROW FACE_TOP FACE_LEFT
    if (upper.startsWith("ROUND ARROW")) {
      // DO NOT send to ESP
      await handleRoundArrow(upper);
      setCommand("");
      return;
    }
    // Intercept authoritative game start command
    if (upper.startsWith("GAME START")) {
      const params = Object.fromEntries(
        upper.split(/\s+/).slice(2).map(p => {
          const [k, v] = p.split("=");
          return [k.toLowerCase(), v];
        })
      );

      if (params.type === "SNAKE") {
        requestedGameRef.current = "snake";
        setRoundPhase(RoundPhase.IDLE);
        roundPhaseRef.current = RoundPhase.IDLE;
        pendingRoundRef.current = null;
        setPendingRound(null);

        await writeGameStart("snake");
        setCommand("");
        return;
      }

      const remaining = params.remaining
        ? Number(params.remaining)
        : 1;

      const duration = params.duration
        ? Number(params.duration)
        : roundDurationRef.current;

      // Store server-owned state
      remainingRoundsRef.current = remaining;
      roundDurationRef.current = duration;
      requestedGameRef.current = "goNoGo";

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
      await writeGameStart("goNoGo");

      setCommand("");
      return;
    }

    // Intercept authoritative round start command
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


    // Default passthrough command
    const line = raw.endsWith("\n") ? raw : raw + "\n";
    await writeLine(line);
    setCommand("");
  }, [command, writeLine, writeGameStart, pushLog]);

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

  const startGoNoGoSession = useCallback(async () => {
    const remaining = 10;
    const duration = 3000;

    remainingRoundsRef.current = remaining;
    roundDurationRef.current = duration;
    baseDurationRef.current = duration;
    successStreakRef.current = 0;
    failStreakRef.current = 0;
    requestedGameRef.current = "goNoGo";

    setRoundPhase(RoundPhase.IDLE);
    roundPhaseRef.current = RoundPhase.IDLE;
    pendingRoundRef.current = null;
    setPendingRound(null);

    pushLog(
      `[SRV] Manual Go/No-Go start requested remaining=${remaining} duration=${duration}`
    );
    await writeGameStart("goNoGo");
  }, [pushLog, writeGameStart]);

  const startSnakeSession = useCallback(async () => {
    requestedGameRef.current = "snake";
    setRoundPhase(RoundPhase.IDLE);
    roundPhaseRef.current = RoundPhase.IDLE;
    pendingRoundRef.current = null;
    setPendingRound(null);

    pushLog(
      "[SRV] Manual Snake start requested; firmware Snake BLE start protocol is incomplete"
    );
    await writeGameStart("snake");
  }, [pushLog, writeGameStart]);

  useEffect(() => {
    if (!gatt?.tx) return;

    const tx = gatt.tx;
    let active = true;

    const handleNotification = async (ev: Event) => {
      const dv = (ev.target as BluetoothRemoteGATTCharacteristic).value!;

      try {
        const bytes = new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength);
        const text = new TextDecoder().decode(bytes).trimEnd();
        pushLog(`[ESP] ${text}`);

        for (const line of text.split(/\r?\n/)) {
          if (!line) continue;

          if (line.startsWith("SESSION END")) {
            const sessionResult = parseSessionEnd(line);

            if (!sessionResult) {
              pushLog(`[SESSION] Could not parse SESSION END: ${line}`);
              continue;
            }

            latestSessionResultRef.current = sessionResult;
            setLatestSessionResult(sessionResult);

            if (sessionResult.type === "SIMON") {
              pushLog(
                `[SESSION] SIMON result captured rounds=${sessionResult.rounds} accuracy=${sessionResult.accuracyPct}% durationMs=${sessionResult.durationMs}`
              );
            } else {
              pushLog(
                `[SESSION] SNAKE result captured score=${sessionResult.finalScore} apples=${sessionResult.apples} deathType=${sessionResult.deathType}`
              );
            }

            continue;
          }

          if (line.startsWith("ROUND BALANCE")) {
            const round = pendingRoundRef.current;

            if (!round) {
              console.warn("[SRV] BALANCE but no pending round");
              continue;
            }

            setRoundPhase(RoundPhase.PLAYING);
            roundPhaseRef.current = RoundPhase.PLAYING;

            const parts = line.split(/\s+/);
            const sidePart = parts.find((p) =>
              p.toLowerCase().startsWith("side=")
            );
            if (!sidePart) continue;

            const balancedFace = sidePart.split("=")[1] as FaceId;

            await writeLine("CLEAR ALL\n");

            if (round.type === "ARROW") {
              const intended = pendingRoundRef.current;
              const isOpposite =
                intended?.type === "ARROW" && intended.mode === "OPPOSITE";

              await writeLine(
                `DRAW SHAPE ${balancedFace} ${round.arrow} COLOR_BLUE\n`
              );
              await writeLine(
                `DRAW SHAPE ${round.to} SHAPE_CIRCLE_6X6 COLOR_GREEN\n`
              );
              await writeLine(
                isOpposite
                  ? "BEEP freq=1000 dur=400\n"
                  : "BEEP freq=1200 dur=200\n"
              );
            } else {
              console.log(
                `[SRV] PAUSE round active on ${balancedFace} for ${round.duration}ms`
              );
            }

            continue;
          }

          if (line.startsWith("END ROUND")) {
            const data = parseEndRound(line);

            if (!data) {
              console.warn("[SRV] Bad END ROUND format");
              continue;
            }

            if (
              roundPhaseRef.current !== RoundPhase.PLAYING &&
              roundPhaseRef.current !== RoundPhase.WAIT_BALANCE
            ) {
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
            if (requestedGameRef.current === "snake") {
              pushLog(
                "[SRV] Snake start ack received; not starting Simon rounds"
              );
              return;
            }

            const parts = line.split(/\s+/);
            const facePart = parts.find((p) =>
              p.toLowerCase().startsWith("face=")
            );
            if (!facePart) return;

            const startFace = facePart.split("=")[1] as FaceId;

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

          if (line.startsWith("PONG ")) {
            const seq = Number(line.substring(5).trim());

            if (!Number.isNaN(seq)) {
              const sentAt = pendingRef.current.get(seq);

              if (sentAt != null) {
                const rtt = performance.now() - sentAt;
                pendingRef.current.delete(seq);

                setRecvCount((prevRecv) => {
                  const nextRecv = prevRecv + 1;
                  setAvgRttMs((prevAvg) =>
                    prevAvg == null
                      ? rtt
                      : prevAvg + (rtt - prevAvg) / nextRecv
                  );
                  return nextRecv;
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
        pushLog(`[ESP] [${hex}]`);
      }
    };

    void tx.startNotifications().then(() => {
      if (active) {
        tx.addEventListener("characteristicvaluechanged", handleNotification);
      }
    });

    pushLog(`[BLE] Connected to ${deviceName || "(no name)"}`);

    return () => {
      active = false;
      tx.removeEventListener("characteristicvaluechanged", handleNotification);
    };
  }, [
    deviceName,
    gatt?.tx,
    handleEndRound,
    handleRoundFail,
    pushLog,
    writeLine,
  ]);

  // Packet test effects
  useEffect(() => {
    if (packetTestIntervalRef.current != null) {
      window.clearInterval(packetTestIntervalRef.current);
      packetTestIntervalRef.current = null;
    }

    if (!testRunning || status !== "connected") {
      return undefined;
    }

    const intervalMs = 100; // a bit less aggressive; can tune later
    packetTestIntervalRef.current = window.setInterval(() => {
      // Optional safety: don't overload if many are in-flight
      if (pendingRef.current.size > 10) {
        return;
      }

      const seq = nextSeqRef.current;
      nextSeqRef.current = seq + 1;
      setNextSeq(nextSeqRef.current);

      const line = `PING ${seq}\n`;

      (async () => {
        await writeLine(line);

        pendingRef.current.set(seq, performance.now());
        setSentCount((c) => c + 1);
      })();
    }, intervalMs);

    return () => {
      if (packetTestIntervalRef.current != null) {
        window.clearInterval(packetTestIntervalRef.current);
        packetTestIntervalRef.current = null;
      }
    };
  }, [testRunning, status, writeLine]);

  // Derived UI values
  const isConnected = status === "connected";
  const statusText =
    status === "connecting"
      ? "CONNECTING…"
      : isConnected
        ? "CONNECTED"
        : status === "disconnected"
          ? "DISCONNECTED"
          : "NOT CONNECTED";

  // Derived UI callbacks
  const restartPingAll = useCallback(() => {
    // reset app-side stats
    setSentCount(0);
    setRecvCount(0);
    setAvgRttMs(null);
    pendingRef.current.clear();
    nextSeqRef.current = 1;
    setNextSeq(1);
    // ask ESP to reset its counters
    if (isConnected) {
      void writeLine("RESTART PING\n");
    }
  }, [isConnected, writeLine]);

  const togglePacketTest = useCallback(() => {
    if (!testRunning) {
      if (packetTestIntervalRef.current != null) {
        window.clearInterval(packetTestIntervalRef.current);
        packetTestIntervalRef.current = null;
      }

      // reset stats on start
      setSentCount(0);
      setRecvCount(0);
      setAvgRttMs(null);
      pendingRef.current.clear();
      nextSeqRef.current = 1;
      setNextSeq(1);
    } else if (packetTestIntervalRef.current != null) {
      window.clearInterval(packetTestIntervalRef.current);
      packetTestIntervalRef.current = null;
    }
    setTestRunning((r) => !r);
  }, [testRunning]);

  // JSX return
  return (
    <AppFrame ariaLabel="Console">
      <div className="stack center">
        <div className="status">
          {statusText}
          {deviceName && isConnected ? ` - ${deviceName}` : ""}
        </div>

        <LogCard log={log} logBoxRef={logBoxRef} />

        <PacketTestCard
          sentCount={sentCount}
          recvCount={recvCount}
          avgRttMs={avgRttMs}
          inFlightCount={pendingRef.current.size}
          isConnected={isConnected}
          testRunning={testRunning}
          onRestartPing={restartPingAll}
          onToggleTest={togglePacketTest}
        />

        <div className="card">
          <h3>Manual game start</h3>
          <p className="muted">
            Start protocol-level sessions through the existing BLE writer.
          </p>
          <div className="actions subtle">
            <button
              className="btn btn-neutral"
              disabled={!isConnected}
              onClick={() => void startGoNoGoSession()}
            >
              Start Go/No-Go
            </button>
            <button
              className="btn btn-neutral"
              disabled={!isConnected}
              onClick={() => void startSnakeSession()}
            >
              Start Snake
            </button>
          </div>
        </div>

        <ManualCommandCard
          command={command}
          isConnected={isConnected}
          onCommandChange={setCommand}
          onSendCommand={() => void sendCommand()}
        />
        <p className="muted">DEBUG BUILD: SNAKE COMMAND FIX ACTIVE</p>
      </div>
    </AppFrame>
  );
}
