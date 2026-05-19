export type BleConnectionStatus =
  | "idle"
  | "connecting"
  | "connected"
  | "disconnected";

export type BleGattConnection = {
  device: BluetoothDevice;
  server: BluetoothRemoteGATTServer;
  service: BluetoothRemoteGATTService;
  rx: BluetoothRemoteGATTCharacteristic;
  tx: BluetoothRemoteGATTCharacteristic;
};

export type BleContextValue = {
  status: BleConnectionStatus;
  statusText: string;
  isConnected: boolean;
  deviceName: string;
  gatt: BleGattConnection | null;
  connect: () => Promise<void>;
  disconnect: () => Promise<void>;
  toggleConnection: () => Promise<void>;
};
