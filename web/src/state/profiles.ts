import { effect, signal } from "@preact/signals";
import { fetchProfiles, profileAction, restoreLevels, saveSettings } from "../api/endpoints";
import type {
  BackupDocument,
  ProfileAction,
  ProfileAnswer,
  ProfileContent,
  ProfileSource,
  ProfilesResponse,
} from "../api/types";
import type { MessageKey } from "../i18n";
import { t, valueLabel } from "../i18n";
import { loadSettings } from "./settings";
import { status } from "./status";

export const SLOTS = 16;

export const profiles = signal<ProfilesResponse | null>(null);

export async function loadProfiles(): Promise<void> {
  try {
    profiles.value = await fetchProfiles();
  } catch {
    // The connection pill already says the board is unreachable.
  }
}

/** Reloads the list when the status shows another profile, a change to it, or another source. */
export function followProfiles(): () => void {
  let last = "";
  return effect(() => {
    const s = status.value;
    if (!s) return;
    const key = JSON.stringify([
      s.profile ?? null,
      s.headline,
      s.source.port,
      s.source.standard,
      s.source.lines,
    ]);
    if (key !== last) {
      last = key;
      void loadProfiles();
    }
  });
}

/** Runs an action, then refreshes the list, and the settings when a load changed them. */
export async function runProfile(action: ProfileAction): Promise<ProfileAnswer> {
  const answer = await profileAction(action);
  await Promise.all([loadProfiles(), action.action === "load" ? loadSettings() : undefined]);
  return answer;
}

export function reasonKey(answer: ProfileAnswer): MessageKey {
  switch (answer.reason) {
    case "revertPending":
      return "profiles.reason.revertPending";
    case "invalidName":
      return "profiles.reason.invalidName";
    case "noSource":
      return "profiles.reason.noSource";
    case "safeMode":
      return "profiles.reason.safeMode";
    default:
      return "action.failed";
  }
}

const STANDARD_KEY: Record<string, MessageKey> = {
  ntsc: "profiles.std.ntsc",
  pal: "profiles.std.pal",
  edtv60: "profiles.std.edtv60",
  edtv50: "profiles.std.edtv50",
  hd720p: "profiles.std.hd720p",
  hd1080i: "profiles.std.hd1080i",
  hd1080p: "profiles.std.hd1080p",
  medres: "profiles.std.medres",
  pcOnYpbpr: "profiles.std.pc",
  rgbhvScaled: "profiles.std.pc",
  rgbhvBypass: "profiles.std.pc",
};

/** "RGB (SCART/RGBS/VGA) · csync · 240p/480i · 262 lines" */
export function describeSource(s: ProfileSource): string {
  return [
    valueLabel("input.port", s.port),
    t(s.sync === "csync" ? "profiles.sync.csync" : "profiles.sync.hv"),
    t(STANDARD_KEY[s.standard] ?? "profiles.std.unknown"),
    t("profiles.lines", { lines: s.lines }),
  ].join(" · ");
}

export function profileName(slot: number): string | undefined {
  return profiles.value?.profiles.find((p) => p.slot === slot)?.name;
}

/** The lowest slot without a profile; 0 when all are in use. */
export function firstFreeSlot(): number {
  const used = new Set(profiles.value?.profiles.map((p) => p.slot));
  for (let slot = 1; slot <= SLOTS; slot++) {
    if (!used.has(slot)) return slot;
  }
  return 0;
}

function isContent(v: unknown): v is ProfileContent {
  const c = v as ProfileContent;
  return (
    typeof c === "object" &&
    c !== null &&
    typeof c.name === "string" &&
    typeof c.values === "object" &&
    c.values !== null
  );
}

/** A profile export file's content, or null when the file is not one. */
export function parseProfileFile(text: string): ProfileContent | null {
  try {
    const doc = JSON.parse(text) as { format?: string } & ProfileContent;
    if (doc.format !== "gbsr-profile" || !isContent(doc)) return null;
    return { name: doc.name, source: doc.source ?? null, values: doc.values };
  } catch {
    return null;
  }
}

/** A backup file, or null when the file is not one. */
export function parseBackupFile(text: string): BackupDocument | null {
  try {
    const doc = JSON.parse(text) as BackupDocument;
    const ok =
      doc.format === "gbsr-backup" &&
      typeof doc.settings === "object" &&
      typeof doc.profiles === "object" &&
      Array.isArray(doc.levels?.component) &&
      Array.isArray(doc.levels?.rgb) &&
      Object.values(doc.profiles).every(isContent);
    return ok ? doc : null;
  } catch {
    return null;
  }
}

export interface RestoreReport {
  profiles: number;
  /** Settings the board refused, and profile fields this firmware does not know. */
  refused: string[];
  /** A restored output setting waits for "Keep", like any risky change. */
  revertArmed: boolean;
}

const KEPT: readonly string[] = ["applied", "unchanged", "revertArmed"];

/**
 * Puts a backup back piece by piece (ADR 0014): the settings, the level calibration, then one
 * profile per request. Profiles in slots the backup does not mention stay. Returns what was
 * restored and what was not, or null when a step failed.
 */
export async function restoreBackup(backup: BackupDocument): Promise<RestoreReport | null> {
  try {
    const settings = await saveSettings(backup.settings);
    const outcomes = Object.entries(settings.results);
    const refused = outcomes.filter(([, o]) => !KEPT.includes(o)).map(([id]) => id);
    const revertArmed = outcomes.some(([, o]) => o === "revertArmed");
    await restoreLevels(backup.levels);
    let count = 0;
    for (const [key, content] of Object.entries(backup.profiles)) {
      const slot = Number(key);
      if (!Number.isInteger(slot) || slot < 1 || slot > SLOTS) continue;
      const answer = await profileAction({ action: "import", slot, profile: content });
      if (!answer.ok) return null;
      refused.push(...(answer.skipped ?? []).map((id) => `${slot}: ${id}`));
      count += 1;
    }
    return { profiles: count, refused, revertArmed };
  } catch {
    return null;
  } finally {
    await Promise.all([loadSettings(), loadProfiles()]);
  }
}
