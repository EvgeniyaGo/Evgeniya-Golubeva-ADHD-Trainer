import { useContext } from "react";
import { BleContext } from "./BleProvider";

export function useBle() {
  const context = useContext(BleContext);

  if (!context) {
    throw new Error("useBle must be used within a BleProvider.");
  }

  return context;
}
