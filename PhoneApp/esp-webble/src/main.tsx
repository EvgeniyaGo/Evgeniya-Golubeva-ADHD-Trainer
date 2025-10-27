import { createRoot } from "react-dom/client";
import { createBrowserRouter, RouterProvider } from "react-router-dom";
import { BLEProvider } from "./state/BLEContext";
import AppShell from "./shell/AppShell";

import Home from "./pages/Home";
import Connect from "./pages/Connect";
import Modes from "./pages/Modes";
import Session from "./pages/Session";
import Sessions from "./pages/Sessions";
import Insights from "./pages/Insights";
import Profiles from "./pages/Profiles";
import Settings from "./pages/Settings";
import Help from "./pages/Help";

import "./App.css";
import "./shell/shell.css";

const router = createBrowserRouter([
  {
    path: "/",
    element: <AppShell />,
    children: [
      { index: true, element: <Home /> },
      { path: "connect", element: <Connect /> },
      { path: "modes", element: <Modes /> },
      { path: "session", element: <Session /> },
      { path: "sessions", element: <Sessions /> },
      { path: "insights", element: <Insights /> },
      { path: "profiles", element: <Profiles /> },
      { path: "settings", element: <Settings /> },
      { path: "help", element: <Help /> },
    ],
  },
]);

createRoot(document.getElementById("root")!).render(
  <BLEProvider>
    <RouterProvider router={router} />
  </BLEProvider>
);
