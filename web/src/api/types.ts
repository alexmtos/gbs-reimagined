// Shape of GET /api/status. Draft contract for M1; it moves to schema/api.yaml with the firmware
// implementation (docs/design/architecture.md, "HTTP API").

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
  source: SourceStatus;
  output: OutputStatus;
  board: BoardStatus;
  revert?: { secondsLeft: number };
}
