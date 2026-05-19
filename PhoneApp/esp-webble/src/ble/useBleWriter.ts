import { useCallback, useRef } from "react";
import { useBle } from "./useBle";

export function useBleWriter(onWrite?: (line: string) => void) {
  const { gatt } = useBle();
  const writeQueueRef = useRef<Promise<void>>(Promise.resolve());

  const writeLine = useCallback(
    (line: string): Promise<void> => {
      if (!gatt) {
        return Promise.reject(new Error("BLE is not connected."));
      }

      const data = new TextEncoder().encode(line);

      writeQueueRef.current = writeQueueRef.current.then(async () => {
        const rx: any = gatt.rx;

        if (typeof rx.writeValueWithoutResponse === "function") {
          await rx.writeValueWithoutResponse(data);
        } else if (typeof rx.writeValueWithResponse === "function") {
          await rx.writeValueWithResponse(data);
        } else {
          await rx.writeValue(data);
        }

        onWrite?.(line);
      });

      return writeQueueRef.current;
    },
    [gatt, onWrite]
  );

  return { writeLine };
}
