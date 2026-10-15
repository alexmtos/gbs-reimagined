import { ApiError, getJson, postForAnswer, postJson } from "./client";
import type {
  BackupDocument,
  CommandName,
  CommandResult,
  Levels,
  ProfileAction,
  ProfileAnswer,
  ProfileDocument,
  ProfilesResponse,
  ScanResult,
  SettingsResponse,
  SettingsResult,
  WifiStatus,
} from "./types";

export const fetchSettings = () => getJson<SettingsResponse>("/api/settings");

export const saveSettings = (values: Record<string, string>) =>
  postJson<SettingsResult>("/api/settings", values);

/** Never throws: a refusal comes back as a reason (409 means no stable picture to calibrate). */
export async function runCommand(name: CommandName): Promise<CommandResult> {
  try {
    return await postJson<CommandResult>("/api/command", { name });
  } catch (e) {
    if (e instanceof ApiError && e.status === 409) {
      return { ok: false, reason: "noStablePicture" };
    }
    return { ok: false, reason: "failed" };
  }
}

export const fetchWifi = () => getJson<WifiStatus>("/api/wifi");

export const scanWifi = () => getJson<ScanResult>("/api/wifi/scan");

export const joinWifi = (ssid: string, password: string) =>
  postJson<{ ok: boolean }>("/api/wifi", { ssid, password });

export const fetchProfiles = () => getJson<ProfilesResponse>("/api/profiles");

/** Never throws: a refusal or a lost connection comes back as a reason. */
export async function profileAction(action: ProfileAction): Promise<ProfileAnswer> {
  try {
    return await postForAnswer<ProfileAnswer>("/api/profile", action);
  } catch {
    return { ok: false, reason: "failed" };
  }
}

export const exportProfile = (slot: number) =>
  getJson<ProfileDocument>(`/api/profile?slot=${slot}`);

export const fetchBackup = () => getJson<BackupDocument>("/api/backup");

export const restoreLevels = (levels: Levels) => postJson<{ ok: boolean }>("/api/levels", levels);
