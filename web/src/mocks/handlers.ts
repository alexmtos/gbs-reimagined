import { HttpResponse, http } from "msw";
import type {
  ScanResult,
  SetOutcome,
  SettingsResponse,
  SettingsResult,
  Status,
  WifiStatus,
} from "../api/types";
import { SCHEMA_HASH, SETTINGS } from "../gen/settings.gen";

// The mock device used by `pnpm dev` (browser worker) and by the tests (node server). It answers
// like the firmware (firmware/src/core/api.cpp) on a board scaling a 240p source, so every page
// works without hardware.
const REVERT_MS = 20_000;
let seq = 0;
const bootedAt = Date.now();

function defaultText(id: string): string {
  const meta = SETTINGS.find((m) => m.id === id);
  if (!meta) return "";
  if (meta.type === "bool") return meta.default ? "on" : "off";
  const special = meta.type === "int" ? meta.special?.[String(meta.default)] : undefined;
  if (special !== undefined) return special;
  return String(meta.default);
}

function freshValues(): Record<string, string> {
  return Object.fromEntries(
    SETTINGS.filter((m) => m.available).map((m) => [m.id, defaultText(m.id)]),
  );
}

const device = {
  values: freshValues(),
  snapshot: null as Record<string, string> | null,
  revertAt: 0,
  wifi: {
    mode: "setup",
    ssid: "",
    ip: "0.0.0.0",
    rssi: 0,
    hostname: "gbs.local",
    lastFailure: "",
    ap: { active: true, ssid: "GBS-EAC4", ip: "192.168.4.1" },
  } as WifiStatus,
  scanPolls: 0,
};

/** Back to a fresh board (tests call it between cases). */
export function resetDevice(): void {
  device.values = freshValues();
  device.snapshot = null;
  device.revertAt = 0;
  device.scanPolls = 0;
  device.wifi = { ...device.wifi, mode: "setup", ssid: "", ip: "0.0.0.0" };
}

function revertLeft(): { secondsLeft: number } | undefined {
  if (!device.snapshot) return undefined;
  const left = device.revertAt - Date.now();
  if (left <= 0) {
    device.values = device.snapshot;
    device.snapshot = null;
    return undefined;
  }
  return { secondsLeft: Math.ceil(left / 1000) };
}

function validate(id: string, raw: unknown): string | null {
  const meta = SETTINGS.find((m) => m.id === id);
  if (!meta) return null;
  const text = typeof raw === "boolean" ? (raw ? "on" : "off") : String(raw);
  if (meta.type === "bool") return text === "on" || text === "off" ? text : null;
  if (meta.type === "enum") return meta.values?.includes(text) ? text : null;
  const n = Number(text);
  return Number.isInteger(n) && n >= (meta.min ?? 0) && n <= (meta.max ?? 0) ? text : null;
}

export function sampleStatus(overrides: Partial<Status> = {}): Status {
  seq += 1;
  const revert = revertLeft();
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
      sog: 13,
      phase: 16,
      syncQuality: 100,
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
    ...(revert ? { revert } : {}),
    ...overrides,
  };
}

export const handlers = [
  http.get("/api/status", () => HttpResponse.json(sampleStatus())),

  http.get("/api/settings", () => {
    const revert = revertLeft();
    const body: SettingsResponse = {
      schema: SCHEMA_HASH,
      values: device.values,
      ...(revert ? { revert } : {}),
    };
    return HttpResponse.json(body);
  }),

  http.post("/api/settings", async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    if (typeof body !== "object" || body === null || Array.isArray(body)) {
      return HttpResponse.json({ error: "expected an object of setting values" }, { status: 400 });
    }
    const results: Record<string, SetOutcome> = {};
    for (const [id, raw] of Object.entries(body)) {
      const meta = SETTINGS.find((m) => m.id === id);
      if (!meta) {
        results[id] = "unknown";
        continue;
      }
      if (!meta.available) {
        results[id] = "unavailable";
        continue;
      }
      const text = validate(id, raw);
      if (text === null) {
        results[id] = "invalid";
      } else if (device.values[id] === text) {
        results[id] = "unchanged";
      } else if (meta.risky) {
        device.snapshot ??= { ...device.values };
        device.values = { ...device.values, [id]: text };
        device.revertAt = Date.now() + REVERT_MS;
        results[id] = "revertArmed";
      } else {
        device.values = { ...device.values, [id]: text };
        results[id] = "applied";
      }
    }
    const revert = revertLeft();
    const answer: SettingsResult = { results, ...(revert ? { revert } : {}) };
    return HttpResponse.json(answer);
  }),

  http.post("/api/command", async ({ request }) => {
    const { name } = (await request.json()) as { name?: string };
    switch (name) {
      case "keep":
        device.snapshot = null;
        break;
      case "revert":
        if (device.snapshot) device.values = device.snapshot;
        device.snapshot = null;
        break;
      case "factoryReset":
        resetDevice();
        break;
      case "calibrate":
      case "reboot":
        break;
      default:
        return HttpResponse.json({ ok: false, reason: "unknownCommand" }, { status: 404 });
    }
    return HttpResponse.json({ ok: true });
  }),

  http.get("/api/wifi", () => HttpResponse.json(device.wifi)),

  http.post("/api/wifi", async ({ request }) => {
    const { ssid } = (await request.json()) as { ssid?: string };
    if (!ssid) return HttpResponse.json({ ok: false, reason: "expectedSsid" }, { status: 400 });
    device.wifi = { ...device.wifi, mode: "joining", ssid };
    return HttpResponse.json({ ok: true });
  }),

  http.get("/api/wifi/scan", () => {
    device.scanPolls += 1;
    const body: ScanResult =
      device.scanPolls < 2
        ? { scanning: true }
        : {
            scanning: false,
            networks: [
              { ssid: "Home", rssi: -48, secure: true },
              { ssid: "Cafe", rssi: -71, secure: false },
            ],
          };
    return HttpResponse.json(body);
  }),
];
