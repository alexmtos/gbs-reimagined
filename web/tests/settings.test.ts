import { setupServer } from "msw/node";
import { afterAll, afterEach, beforeAll, beforeEach, describe, expect, it } from "vitest";
import { runCommand, scanWifi } from "../src/api/endpoints";
import { handlers, resetDevice } from "../src/mocks/handlers";
import { changeSetting, fieldState, loadSettings, values } from "../src/state/settings";
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
    expect(values.value["output.aspect"]).toBeUndefined(); // M2
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
});
