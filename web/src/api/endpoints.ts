import { ApiError, getJson, postJson } from "./client";
import type {
  CommandName,
  CommandResult,
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
