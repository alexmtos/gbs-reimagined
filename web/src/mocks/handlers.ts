import { HttpResponse, http } from "msw";
import type {
  BackupDocument,
  Levels,
  ProfileAction,
  ProfileAnswer,
  ProfileContent,
  ProfileDocument,
  ProfileSource,
  ProfilesResponse,
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

/** The settled 240p source the mock board is scaling. */
const SOURCE: ProfileSource = { port: "rgb", sync: "csync", standard: "ntsc", lines: 262 };
const NAME_OK = /^[\x20-\x7e\u00a0-\u00ff]{0,23}$/;

const inProfile = (id: string) => !id.startsWith("system.");

const device = {
  values: freshValues(),
  snapshot: null as Record<string, string> | null,
  /** The profile state a pending revert puts back. */
  snapshotProfile: null as { active: number; modified: boolean } | null,
  revertAt: 0,
  profiles: new Map<number, ProfileContent>(),
  active: 0,
  modified: false,
  levels: { component: [123, 123, 123], rgb: [123, 123, 123] } as Levels,
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
  device.snapshotProfile = null;
  device.revertAt = 0;
  device.profiles = new Map();
  device.active = 0;
  device.modified = false;
  device.levels = { component: [123, 123, 123], rgb: [123, 123, 123] };
  device.scanPolls = 0;
  device.wifi = { ...device.wifi, mode: "setup", ssid: "", ip: "0.0.0.0" };
}

function revertLeft(): { secondsLeft: number } | undefined {
  if (!device.snapshot) return undefined;
  const left = device.revertAt - Date.now();
  if (left <= 0) {
    revertNow();
    return undefined;
  }
  return { secondsLeft: Math.ceil(left / 1000) };
}

function revertNow(): void {
  if (device.snapshot) device.values = device.snapshot;
  if (device.snapshotProfile) {
    device.active = device.snapshotProfile.active;
    device.modified = device.snapshotProfile.modified;
  }
  device.snapshot = null;
  device.snapshotProfile = null;
}

function validate(id: string, raw: unknown): string | null {
  const meta = SETTINGS.find((m) => m.id === id);
  if (!meta) return null;
  const text = typeof raw === "boolean" ? (raw ? "on" : "off") : String(raw);
  if (meta.type === "bool") return text === "on" || text === "off" ? text : null;
  if (meta.type === "enum") return meta.values?.includes(text) ? text : null;
  if (meta.special && Object.values(meta.special).includes(text)) return text;
  const n = Number(text);
  if (!Number.isInteger(n) || n < (meta.min ?? 0) || n > (meta.max ?? 0)) return null;
  return meta.special?.[String(n)] ?? text;
}

function answer(body: ProfileAnswer, status = 200) {
  return HttpResponse.json(body, { status });
}

function profileAction(a: ProfileAction) {
  if (a.action === "load") {
    const p = device.profiles.get(a.slot);
    if (!p) return answer({ ok: false, reason: "noProfile" }, 404);
    const before = { ...device.values };
    let risky = false;
    for (const [id, text] of Object.entries(p.values)) {
      const meta = SETTINGS.find((m) => m.id === id);
      if (!meta?.available || device.values[id] === text) continue;
      device.values = { ...device.values, [id]: text };
      risky ||= meta.risky;
    }
    if (risky) {
      device.snapshot ??= before;
      device.snapshotProfile ??= { active: device.active, modified: device.modified };
      device.revertAt = Date.now() + REVERT_MS;
    }
    device.active = a.slot;
    device.modified = false;
    const revert = revertLeft();
    return answer({
      ok: true,
      result: risky ? "revertArmed" : "loaded",
      ...(revert ? { revert } : {}),
    });
  }
  if (!Number.isInteger(a.slot) || a.slot < 1 || a.slot > 16) {
    return answer({ ok: false, reason: "invalidSlot" }, 400);
  }
  const existing = device.profiles.get(a.slot);
  switch (a.action) {
    case "save": {
      const name = a.name ?? "";
      if (device.snapshot) return answer({ ok: false, reason: "revertPending" }, 409);
      if (!NAME_OK.test(name)) return answer({ ok: false, reason: "invalidName" }, 400);
      const source =
        a.source === "this" ? SOURCE : a.source === "none" ? null : (existing?.source ?? null);
      device.profiles.set(a.slot, {
        name: name || existing?.name || `Profile ${a.slot}`,
        source,
        values: Object.fromEntries(Object.entries(device.values).filter(([id]) => inProfile(id))),
      });
      device.active = a.slot;
      device.modified = false;
      return answer({ ok: true });
    }
    case "rename":
      if (!existing) return answer({ ok: false, reason: "noProfile" }, 404);
      if (!a.name || !NAME_OK.test(a.name))
        return answer({ ok: false, reason: "invalidName" }, 400);
      existing.name = a.name;
      return answer({ ok: true });
    case "source":
      if (!existing) return answer({ ok: false, reason: "noProfile" }, 404);
      existing.source = a.on ? SOURCE : null;
      return answer({ ok: true });
    case "delete":
      if (!existing) return answer({ ok: false, reason: "noProfile" }, 404);
      device.profiles.delete(a.slot);
      if (device.active === a.slot) {
        device.active = 0;
        device.modified = false;
      }
      return answer({ ok: true });
    case "import": {
      const p = a.profile;
      if (typeof p?.values !== "object")
        return answer({ ok: false, reason: "expectedProfile" }, 400);
      if (!NAME_OK.test(p.name ?? "")) return answer({ ok: false, reason: "invalidName" }, 400);
      const values: Record<string, string> = {};
      const skipped: string[] = [];
      for (const [id, raw] of Object.entries(p.values)) {
        const text = inProfile(id) ? validate(id, raw) : null;
        if (text === null) skipped.push(id);
        else values[id] = text;
      }
      device.profiles.set(a.slot, {
        name: p.name || `Profile ${a.slot}`,
        source: p.source ?? null,
        values,
      });
      if (device.active === a.slot) {
        device.active = 0;
        device.modified = false;
      }
      return answer({ ok: true, skipped });
    }
  }
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
    profile:
      device.active === 0
        ? null
        : {
            slot: device.active,
            name: device.profiles.get(device.active)?.name ?? "",
            modified: device.modified,
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
      if (results[id] !== "unchanged" && device.active !== 0 && inProfile(id)) {
        device.modified = true;
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
        device.snapshotProfile = null;
        break;
      case "revert":
        revertNow();
        break;
      case "factoryReset": {
        // Like the board: settings and levels to defaults, the profiles stay, none in use.
        const kept = device.profiles;
        resetDevice();
        device.profiles = kept;
        break;
      }
      case "calibrate":
      case "reboot":
        break;
      default:
        return HttpResponse.json({ ok: false, reason: "unknownCommand" }, { status: 404 });
    }
    return HttpResponse.json({ ok: true });
  }),

  http.get("/api/profiles", () => {
    const body: ProfilesResponse = {
      active: device.active,
      modified: device.modified,
      source: SOURCE,
      profiles: [...device.profiles.entries()]
        .sort(([a], [b]) => a - b)
        .map(([slot, p]) => ({ slot, name: p.name, source: p.source })),
    };
    return HttpResponse.json(body);
  }),

  http.get("/api/profile", ({ request }) => {
    const slot = Number(new URL(request.url).searchParams.get("slot"));
    const p = device.profiles.get(slot);
    if (!p) return answer({ ok: false, reason: "noProfile" }, 404);
    const doc: ProfileDocument = { format: "gbsr-profile", version: 1, schema: SCHEMA_HASH, ...p };
    return HttpResponse.json(doc);
  }),

  http.post("/api/profile", async ({ request }) => {
    const body = (await request.json()) as ProfileAction;
    if (typeof body?.action !== "string") {
      return HttpResponse.json({ ok: false, reason: "expectedAction" }, { status: 400 });
    }
    return profileAction(body) ?? answer({ ok: false, reason: "unknownAction" }, 404);
  }),

  http.get("/api/backup", () => {
    const doc: BackupDocument = {
      format: "gbsr-backup",
      version: 1,
      firmware: "0.1.0-mock",
      schema: SCHEMA_HASH,
      settings: device.values,
      levels: device.levels,
      profiles: Object.fromEntries(
        [...device.profiles.entries()].map(([slot, p]) => [String(slot), p]),
      ),
    };
    return HttpResponse.json(doc);
  }),

  http.post("/api/levels", async ({ request }) => {
    const body = (await request.json()) as Levels;
    const ok = [body?.component, body?.rgb].every(
      (g) =>
        Array.isArray(g) &&
        g.length === 3 &&
        g.every((v) => Number.isInteger(v) && v >= 0 && v <= 255),
    );
    if (!ok) return HttpResponse.json({ ok: false, reason: "expectedLevels" }, { status: 400 });
    device.levels = body;
    return HttpResponse.json({ ok: true });
  }),

  http.post("/api/svs", async ({ request }) => {
    const line = (await request.text()).trim();
    const m = /^(?:SVS (NEW|CURRENT) INPUT=|remote prof)(\d+)$/i.exec(line);
    const slot = Number(m?.[2]);
    if (!m || slot < 1 || slot > 16) {
      return HttpResponse.json({ ok: false, reason: "expectedSwitcherLine" }, { status: 400 });
    }
    if (m[1]?.toUpperCase() === "CURRENT" && device.active === slot && !device.modified) {
      return HttpResponse.json({ ok: true, result: "unchanged" });
    }
    if (!device.profiles.has(slot)) return answer({ ok: false, reason: "noProfile" }, 404);
    const p = device.profiles.get(slot) as ProfileContent;
    device.values = { ...device.values, ...p.values };
    device.active = slot;
    device.modified = false;
    return HttpResponse.json({ ok: true, result: "loaded" });
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
