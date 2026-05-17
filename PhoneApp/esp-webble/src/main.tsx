import React from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import { AuthProvider } from "./auth/AuthProvider";
import { BleProvider } from "./ble/BleProvider";
import "./index.css";

createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <BleProvider>
      <AuthProvider>
        <App />
      </AuthProvider>
    </BleProvider>
  </React.StrictMode>
);
