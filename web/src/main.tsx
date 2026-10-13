import { render } from "preact";
import { App } from "./app";
import { locale } from "./i18n";
import { startPolling } from "./state/status";
import "./styles.css";

document.documentElement.lang = locale.value;

async function start(): Promise<void> {
  // `pnpm dev` without a board: answer the API from the mock device (MSW), never in production.
  if (import.meta.env.DEV && !import.meta.env.VITE_BOARD) {
    const { worker } = await import("./mocks/browser");
    await worker.start({ onUnhandledFrame: "bypass" });
  }
  const root = document.getElementById("app");
  if (root) render(<App />, root);
  startPolling();
}

void start();
