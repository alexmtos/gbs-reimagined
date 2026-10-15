import { setupServer } from "msw/node";
import { afterAll, afterEach, beforeAll, beforeEach, describe, expect, it } from "vitest";
import { exportProfile, fetchBackup, runCommand } from "../src/api/endpoints";
import { handlers, resetDevice } from "../src/mocks/handlers";
import {
  describeSource,
  firstFreeSlot,
  loadProfiles,
  parseBackupFile,
  parseProfileFile,
  profiles,
  reasonKey,
  restoreBackup,
  runProfile,
} from "../src/state/profiles";
import { changeSetting, loadSettings, values } from "../src/state/settings";
import { pollOnce, status } from "../src/state/status";

const server = setupServer(...handlers);

beforeAll(() => {
  server.listen({ onUnhandledFrame: "error" });
});
beforeEach(async () => {
  resetDevice();
  await Promise.all([loadSettings(), loadProfiles()]);
});
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

describe("profiles", () => {
  it("saves the current settings and shows the profile in use", async () => {
    expect(profiles.value?.profiles).toEqual([]);
    expect(firstFreeSlot()).toBe(1);
    await changeSetting("picture.brightness", "5");
    const a = await runProfile({ action: "save", slot: 1, name: "SNES", source: "this" });
    expect(a.ok).toBe(true);
    expect(profiles.value?.active).toBe(1);
    expect(profiles.value?.profiles[0]).toMatchObject({ slot: 1, name: "SNES" });
    expect(firstFreeSlot()).toBe(2);
    await pollOnce();
    expect(status.value?.profile).toEqual({ slot: 1, name: "SNES", modified: false });
  });

  it("a change after loading marks the profile modified, and loading restores it", async () => {
    await changeSetting("picture.brightness", "5");
    await runProfile({ action: "save", slot: 2, source: "none" });
    await changeSetting("picture.brightness", "-3");
    await loadProfiles();
    expect(profiles.value?.modified).toBe(true);
    expect((await runProfile({ action: "load", slot: 2 })).result).toBe("loaded");
    expect(values.value["picture.brightness"]).toBe("5");
    expect(profiles.value?.modified).toBe(false);
  });

  it("a load that changes the output waits for keep, like a risky setting", async () => {
    await changeSetting("output.resolution", "r1080p");
    await runCommand("keep");
    await runProfile({ action: "save", slot: 1 });
    await changeSetting("output.resolution", "r480p");
    await runCommand("keep");
    const a = await runProfile({ action: "load", slot: 1 });
    expect(a.result).toBe("revertArmed");
    await runCommand("revert");
    await Promise.all([loadSettings(), loadProfiles()]);
    expect(values.value["output.resolution"]).toBe("r480p");
  });

  it("refusals come back with a reason the page can say", async () => {
    const bad = await runProfile({ action: "save", slot: 1, name: "€uro" });
    expect(bad.ok).toBe(false);
    expect(reasonKey(bad)).toBe("profiles.reason.invalidName");
    const missing = await runProfile({ action: "load", slot: 9 });
    expect(missing.reason).toBe("noProfile");
    expect(reasonKey(missing)).toBe("action.failed");
  });

  it("renames, stops loading by itself, and deletes", async () => {
    await runProfile({ action: "save", slot: 3, source: "this" });
    expect(profiles.value?.profiles[0]?.source).not.toBeNull();
    await runProfile({ action: "rename", slot: 3, name: "Mega Drive" });
    await runProfile({ action: "source", slot: 3, on: false });
    expect(profiles.value?.profiles[0]).toMatchObject({ name: "Mega Drive", source: null });
    await runProfile({ action: "delete", slot: 3 });
    expect(profiles.value?.profiles).toEqual([]);
    expect(profiles.value?.active).toBe(0);
  });

  it("an export file imports into another slot", async () => {
    await changeSetting("picture.scanlines", "on");
    await runProfile({ action: "save", slot: 1, name: "Saturn" });
    const doc = await exportProfile(1);
    const content = parseProfileFile(JSON.stringify(doc));
    if (!content) throw new Error("the export did not parse");
    expect(content.name).toBe("Saturn");
    const a = await runProfile({ action: "import", slot: 4, profile: content });
    expect(a).toMatchObject({ ok: true, skipped: [] });
    expect(profiles.value?.profiles.map((p) => p.slot)).toEqual([1, 4]);
    expect(parseProfileFile('{"format":"something-else"}')).toBeNull();
    expect(parseProfileFile("not json")).toBeNull();
  });

  it("a backup restores settings, levels and profiles, and leaves other slots alone", async () => {
    await changeSetting("picture.contrast", "4");
    await runProfile({ action: "save", slot: 1, name: "One" });
    const backup = parseBackupFile(JSON.stringify(await fetchBackup()));
    if (!backup) throw new Error("the backup did not parse");
    expect(backup.profiles["1"]?.name).toBe("One");

    resetDevice();
    await runProfile({ action: "save", slot: 7, name: "Seven" });
    expect(await restoreBackup(backup)).toEqual({ profiles: 1, refused: [], revertArmed: false });
    expect(values.value["picture.contrast"]).toBe("4");
    expect(profiles.value?.profiles.map((p) => p.name)).toEqual(["One", "Seven"]);
    expect(parseBackupFile('{"format":"gbsr-profile"}')).toBeNull();
  });

  it("a restore says what the board refused and what waits for Keep", async () => {
    const backup = parseBackupFile(JSON.stringify(await fetchBackup()));
    if (!backup) throw new Error("the backup did not parse");
    backup.settings["picture.contrast"] = "99"; // out of range: refused
    backup.settings["output.resolution"] = "r1080p"; // risky: waits for Keep
    const report = await restoreBackup(backup);
    expect(report?.refused).toEqual(["picture.contrast"]);
    expect(report?.revertArmed).toBe(true);
  });

  it("a factory reset keeps the profiles", async () => {
    await runProfile({ action: "save", slot: 2 });
    await runCommand("factoryReset");
    await loadProfiles();
    expect(profiles.value?.profiles.map((p) => p.slot)).toEqual([2]);
    expect(profiles.value?.active).toBe(0);
  });

  it("describes a source in words", () => {
    expect(describeSource({ port: "rgb", sync: "csync", standard: "ntsc", lines: 262 })).toBe(
      "RGB (SCART/RGBS/VGA) · csync · 240p/480i · 262 lines",
    );
  });
});
