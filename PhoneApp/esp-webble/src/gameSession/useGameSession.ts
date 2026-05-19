import { useContext } from "react";
import { GameSessionContext } from "./GameSessionProvider";

export function useGameSession() {
  const value = useContext(GameSessionContext);

  if (!value) {
    throw new Error("useGameSession must be used within a GameSessionProvider.");
  }

  return value;
}
