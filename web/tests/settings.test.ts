import { setupServer } from "msw/node";
import { afterAll, afterEach, beforeAll, beforeEach, describe, expect, it } from "vitest";
import { runCommand, scanWifi } from "../src/api/endpoints";
import { locale } from "../src/i18n";
import { handlers, resetDevice } from "../src/mocks/handlers";
import {
  changeSetting,
  chooseLanguage,
  fieldState,
  followLanguage,
  loadSettings,
  setShowAdvanced,
  showAdvanced,
  values,
} from "../src/state/settings";
import { pollOnce, status } from "../src/state/status";

const server = setupServer(...handlers);

beforeAll(() => {
  server.listen({ onUnhandledFrame: "error" });
});
beforeEach(() => resetDevice());
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

describe("settings", () => {
  it("loads this milestone's settings as text", async () => {
    await loadSettings();
    expect(values.value["output.resolution"]).toBe("auto");
    expect(values.value["picture.sharpness"]).toBe("off");
    expect(values.value["output.aspect"]).toBe("a4x3");
    expect(values.value["system.startupProfile"]).toBe("lastUsed");
  });

  it("keeps an accepted change and marks it saved", async () => {
    await loadSettings();
    expect(await changeSetting("picture.sharpness", "medium")).toBe("applied");
    expect(values.value["picture.sharpness"]).toBe("medium");
    expect(fieldState.value["picture.sharpness"]).toBe("saved");
  });

  it("rolls back a refused change and marks it failed", async () => {
    await loadSettings();
    expect(await changeSetting("picture.contrast", "99")).toBe("invalid");
    expect(values.value["picture.contrast"]).toBe("0");
    expect(fieldState.value["picture.contrast"]).toBe("failed");
  });

  it("a risky change shows the revert countdown until it is kept", async () => {
    await loadSettings();
    expect(await changeSetting("output.resolution", "r1080p")).toBe("revertArmed");
    await pollOnce();
    expect(status.value?.revert?.secondsLeft).toBeGreaterThan(15);
    expect((await runCommand("keep")).ok).toBe(true);
    await pollOnce();
    expect(status.value?.revert).toBeUndefined();
    await loadSettings();
    expect(values.value["output.resolution"]).toBe("r1080p");
  });

  it("revert puts the previous value back", async () => {
    await loadSettings();
    await changeSetting("output.frameLock", "off");
    await runCommand("revert");
    await loadSettings();
    expect(values.value["output.frameLock"]).toBe("on");
  });
});

describe("commands and network", () => {
  it("an unknown command is a refusal, not an exception", async () => {
    // The client only sends known names; force one through the endpoint helper's fallback path.
    const r = await runCommand("bogus" as never);
    expect(r.ok).toBe(false);
  });

  it("a scan is polled until it has results", async () => {
    expect((await scanWifi()).scanning).toBe(true);
    const done = await scanWifi();
    expect(done.scanning).toBe(false);
    expect(done.networks?.[0]?.ssid).toBe("Home");
  });

  it("the advanced switch is the board's setting: off by default, kept once turned on", async () => {
    await loadSettings();
    expect(showAdvanced.value).toBe(false);
    expect(await setShowAdvanced(true)).toBe("applied");
    expect(showAdvanced.value).toBe(true);
    await loadSettings();
    expect(values.value["system.showAdvanced"]).toBe("on");
    expect(showAdvanced.value).toBe(true);
  });

  it("the language is the board's: the header sets it, and the page follows it", async () => {
    const stop = followLanguage();
    try {
      await loadSettings();
      expect(values.value["system.language"]).toBe("en");
      expect(locale.value).toBe("en");
      expect(await chooseLanguage("pt-BR")).toBe("applied");
      expect(locale.value).toBe("pt-BR");
      await loadSettings();
      expect(values.value["system.language"]).toBe("ptBR");
      // Set elsewhere (the board's menu, another browser): the page follows on the next load.
      await changeSetting("system.language", "en");
      expect(locale.value).toBe("en");
    } finally {
      stop();
    }
  });
});
