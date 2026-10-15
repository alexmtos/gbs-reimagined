import { REQUEST_TIMEOUT_MS } from "../api/client";
import { sha256Hex } from "./sha256";
import { compareVersions } from "./version";

// Firmware updates (ADR 0012). The board takes an update file (.ota.bin.gz) on /api/update and
// checks that it names this board before writing anything. "Check for updates" is done here: the
// browser reads latest.json from the installer site (GitHub Pages serves it with CORS), downloads
// the file, checks its SHA-256 and sends it to the board.

/** Where the installer site and latest.json live once the project is public (ADR 0012). */
export const UPDATE_SITE = "https://alexmtos.github.io/gbs-reimagined/";
/** What update files call this board (hal::BoardProfile::id). */
export const BOARD_ID = "gbsc";

export interface UpdateFile {
  /** Path of the .ota.bin.gz, relative to the site. */
  ota: string;
  sha256: string;
  size: number;
}

export interface Channel {
  version: string;
  /** Release notes (the GitHub release page). */
  notes?: string;
  files: Record<string, UpdateFile>;
}

/** latest.json on the installer site. */
export interface Latest {
  format: "gbsr-latest";
  version: 1;
  stable?: Channel;
  beta?: Channel;
}

export interface Offer {
  version: string;
  notes?: string;
  file: UpdateFile;
}

export type UpdateReason =
  | "notGzip"
  | "noBoardId"
  | "wrongBoard"
  | "short"
  | "noSpace"
  | "writeFailed"
  | "verifyFailed"
  | "refused"
  | "aborted"
  | "noFile"
  | "failed";

export interface UpdateAnswer {
  ok: boolean;
  version?: string;
  reason?: UpdateReason;
}

export async function fetchLatest(site = UPDATE_SITE): Promise<Latest> {
  const response = await fetch(new URL("latest.json", site), {
    cache: "no-store",
    signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS),
  });
  if (!response.ok) throw new Error(`latest.json: HTTP ${response.status}`);
  const latest = (await response.json()) as Latest;
  if (latest.format !== "gbsr-latest") throw new Error("latest.json: unknown format");
  return latest;
}

/** The newest version for this board above `current`, among stable (and beta) releases. */
export function pickOffer(latest: Latest, current: string, includeBeta: boolean): Offer | null {
  const channels = [latest.stable, includeBeta ? latest.beta : undefined];
  let best: Offer | null = null;
  for (const c of channels) {
    const file = c?.files[BOARD_ID];
    if (!c || !file || compareVersions(c.version, current) <= 0) continue;
    if (!best || compareVersions(c.version, best.version) > 0) {
      best = { version: c.version, file, ...(c.notes ? { notes: c.notes } : {}) };
    }
  }
  return best;
}

export class ChecksumError extends Error {}

/** The update file, downloaded and checked against its size and SHA-256. */
export async function downloadOffer(offer: Offer, site = UPDATE_SITE): Promise<Blob> {
  const response = await fetch(new URL(offer.file.ota, site), {
    signal: AbortSignal.timeout(120_000),
  });
  if (!response.ok) throw new Error(`${offer.file.ota}: HTTP ${response.status}`);
  const bytes = new Uint8Array(await response.arrayBuffer());
  if (bytes.length !== offer.file.size || sha256Hex(bytes) !== offer.file.sha256.toLowerCase()) {
    throw new ChecksumError(offer.file.ota);
  }
  return new Blob([bytes], { type: "application/gzip" });
}

/**
 * Sends an update file to the board. Never throws: a refusal or a lost connection comes back as a
 * reason. `onProgress` gets the share sent, 0..1.
 */
export function uploadFirmware(
  file: Blob,
  name: string,
  onProgress: (share: number) => void = () => {},
): Promise<UpdateAnswer> {
  return new Promise((resolve) => {
    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/api/update");
    xhr.timeout = 180_000;
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && e.total > 0) onProgress(e.loaded / e.total);
    };
    xhr.onload = () => {
      try {
        resolve(JSON.parse(xhr.responseText) as UpdateAnswer);
      } catch {
        resolve({ ok: false, reason: "failed" });
      }
    };
    const fail = () => resolve({ ok: false, reason: "failed" });
    xhr.onerror = fail;
    xhr.ontimeout = fail;
    const form = new FormData();
    form.append("file", file, name);
    xhr.send(form);
  });
}
