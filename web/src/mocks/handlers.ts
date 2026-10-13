import { HttpResponse, http } from "msw";
import type { Status } from "../api/types";
import { SCHEMA_HASH } from "../gen/settings.gen";

// The mock device used by `pnpm dev` (browser worker) and by the tests (node server). It answers
// like a board scaling a 240p source, so every page renders without hardware.
let seq = 0;
const bootedAt = Date.now();

export function sampleStatus(overrides: Partial<Status> = {}): Status {
  seq += 1;
  return {
    seq,
    headline: "scaling",
    source: {
      port: "rgb",
      sync: "csync",
      standard: "240p",
      lines: 262,
      interlaced: false,
      hFreqKHz: 15.734,
      vFreqHz: 59.826,
    },
    output: { mode: "r960p", pixelClockMHz: 108.0, rateHz: 59.826, frameLock: "locked" },
    board: {
      version: "0.1.0-mock",
      schema: SCHEMA_HASH,
      uptimeS: Math.round((Date.now() - bootedAt) / 1000),
      heapFree: 26_112,
      heapMin: 24_800,
      rssi: -58,
      ip: "192.168.1.50",
      scaler: true,
      clockGen: true,
    },
    ...overrides,
  };
}

export const handlers = [http.get("/api/status", () => HttpResponse.json(sampleStatus()))];
