// Shapes of the board's JSON API (firmware/src/core/api.cpp, core/status.cpp, net/web.cpp;
// docs/design/architecture.md, "HTTP API").

export type HeadlineCode =
  | "scalerMissing"
  | "noSignal"
  | "syncUnstable"
  | "settling"
  | "locking"
  | "scaling"
  | "paused";

export type InputPort = "rgb" | "component" | "none";
export type SyncType = "hv" | "csync" | "sog" | "none";
export type FrameLockState = "locked" | "inactive" | "off";

export interface SourceStatus {
  port: InputPort;
  sync: SyncType;
  /** Human-readable standard, e.g. "240p", "480i", "576p". Empty when unknown. */
  standard: string;
  lines: number;
  interlaced: boolean;
  hFreqKHz: number;
  vFreqHz: number;
  /** Sync-on-green threshold in effect. */
  sog: number;
  /** ADC sampling phase in effect. */
  phase: number;
  /** Share of the last ~1.3 s with the line PLL locked, 0..100; null when unknown. */
  syncQuality: number | null;
}

export interface OutputStatus {
  /** Value id of output.resolution actually in effect (never "auto"). */
  mode: string;
  pixelClockMHz: number;
  rateHz: number;
  frameLock: FrameLockState;
  /** Message key explaining why frame lock is inactive, when it is. */
  frameLockReason?: string;
}

export interface BoardStatus {
  version: string;
  schema: string;
  uptimeS: number;
  heapFree: number;
  heapMin: number;
  rssi: number;
  ip: string;
  scaler: boolean;
  clockGen: boolean;
}

export interface Status {
  seq: number;
  headline: HeadlineCode;
  /** Booted with defaults; nothing is saved. */
  safeMode?: boolean;
  source: SourceStatus;
  output: OutputStatus;
  board: BoardStatus;
  revert?: { secondsLeft: number };
}

/** GET /api/settings: every setting of this firmware's milestone, as value text. */
export interface SettingsResponse {
  schema: string;
  values: Record<string, string>;
  revert?: { secondsLeft: number };
}

export type SetOutcome =
  | "applied"
  | "unchanged"
  | "invalid"
  | "unavailable"
  | "unknown"
  | "revertArmed";

/** POST /api/settings answer. */
export interface SettingsResult {
  results: Record<string, SetOutcome>;
  revert?: { secondsLeft: number };
}

export type CommandName = "keep" | "revert" | "calibrate" | "reboot" | "factoryReset";

export interface CommandResult {
  ok: boolean;
  reason?: string;
}

export type WifiMode = "joining" | "joined" | "setup";

/** GET /api/wifi */
export interface WifiStatus {
  mode: WifiMode;
  ssid: string;
  ip: string;
  rssi: number;
  hostname: string;
  /** Why the last join did not succeed ("" when it did, or none was tried). */
  lastFailure: "" | "refused" | "notFound" | "noAnswer";
  ap: { active: boolean; ssid: string; ip: string };
}

/** GET /api/wifi/scan: polled until `scanning` is false. */
export interface ScanResult {
  scanning: boolean;
  networks?: { ssid: string; rssi: number; secure: boolean }[];
}
