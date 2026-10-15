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
/** Why frame lock is inactive while its setting is on (firmware core::FrameLockReason). */
export type FrameLockReason =
  | "noSignal"
  | "passthrough"
  | "force60"
  | "noClockGen"
  | "outOfRange"
  | "locking";
/** The last "calibrate levels" (firmware video::GainCalibrator::Outcome). */
export type LevelsOutcome = "none" | "running" | "done" | "notWhite" | "tooHot" | "aborted";

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
  /** Why frame lock is inactive, when it is. */
  frameLockReason?: FrameLockReason;
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
  /** Moves whenever a setting changes, wherever it was changed: the page reloads its values. */
  settingsSeq: number;
  levels: LevelsOutcome;
  source: SourceStatus;
  output: OutputStatus;
  board: BoardStatus;
  /** The profile in use, null when the settings did not come from one. */
  profile: ActiveProfile | null;
  revert?: { secondsLeft: number };
}

export interface ActiveProfile {
  slot: number;
  name: string;
  /** Settings changed since the profile was loaded or saved. */
  modified: boolean;
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

// Profiles (ADR 0014)

/** What a settled source looks like; a profile with one loads by itself for that source. */
export interface ProfileSource {
  port: "rgb" | "component";
  sync: "csync" | "hv";
  /** "ntsc", "pal", "edtv60", "edtv50", "hd720p", "hd1080i", "hd1080p", "medres", "unknown", … */
  standard: string;
  lines: number;
}

export interface ProfileSummary {
  slot: number;
  name: string;
  source: ProfileSource | null;
}

/** GET /api/profiles: the slots in use, the active one (0: none) and the settled source. */
export interface ProfilesResponse {
  active: number;
  modified: boolean;
  source: ProfileSource | null;
  profiles: ProfileSummary[];
}

/** A profile's content: in an export file, an import, and a backup. */
export interface ProfileContent {
  name: string;
  source: ProfileSource | null;
  values: Record<string, string>;
}

/** GET /api/profile?slot=n, and the file a profile exports to. */
export interface ProfileDocument extends ProfileContent {
  format: "gbsr-profile";
  version: number;
  schema: string;
}

export type ProfileAction =
  | { action: "load"; slot: number }
  | { action: "save"; slot: number; name?: string; source?: "keep" | "this" | "none" }
  | { action: "rename"; slot: number; name: string }
  | { action: "source"; slot: number; on: boolean }
  | { action: "delete"; slot: number }
  | { action: "import"; slot: number; profile: ProfileContent };

export type ProfileReason =
  | "invalidSlot"
  | "invalidName"
  | "noSource"
  | "revertPending"
  | "safeMode"
  | "noProfile"
  | "expectedProfile"
  | "invalidSource"
  | "unknownAction"
  | "failed";

/** POST /api/profile answer. */
export interface ProfileAnswer {
  ok: boolean;
  result?: "loaded" | "revertArmed" | "unchanged";
  reason?: ProfileReason;
  /** Import: setting ids this firmware did not take. */
  skipped?: string[];
  revert?: { secondsLeft: number };
}

export interface Levels {
  component: [number, number, number];
  rgb: [number, number, number];
}

/** GET /api/backup: everything but the network credentials. */
export interface BackupDocument {
  format: "gbsr-backup";
  version: number;
  firmware: string;
  schema: string;
  settings: Record<string, string>;
  levels: Levels;
  profiles: Record<string, ProfileContent>;
}
