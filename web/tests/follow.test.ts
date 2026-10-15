import { setupServer } from "msw/node";
import { afterAll, afterEach, beforeAll, beforeEach, describe, expect, it, vi } from "vitest";
import { saveSettings } from "../src/api/endpoints";
import { setLocale } from "../src/i18n";
import { handlers, resetDevice, sampleStatus } from "../src/mocks/handlers";
import { headline } from "../src/pages/status-page";
import { followSettings, loadSettings, values } from "../src/state/settings";
import { connection, followFirmware, pollOnce, status } from "../src/state/status";

const server = setupServer(...handlers);

beforeAll(() => {
  server.listen({ onUnhandledFrame: "error" });
  setLocale("en");
});
beforeEach(() => resetDevice());
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

describe("the page follows the board", () => {
  it("reloads the settings when they change elsewhere", async () => {
    const stop = followSettings();
    try {
      await pollOnce();
      await vi.waitFor(() => expect(values.value["picture.sharpness"]).toBe("off"));
      // Changed by something other than this page (the knob, a profile loading by itself…).
      await saveSettings({ "picture.sharpness": "high" });
      await pollOnce();
      await vi.waitFor(() => expect(values.value["picture.sharpness"]).toBe("high"));
    } finally {
      stop();
    }
  });

  it("reloads the page when the board comes back with other firmware", () => {
    const reload = vi.fn();
    status.value = sampleStatus();
    const stop = followFirmware(reload);
    try {
      status.value = sampleStatus();
      expect(reload).not.toHaveBeenCalled();
      status.value = sampleStatus({ board: { ...sampleStatus().board, version: "9.9.9" } });
      expect(reload).toHaveBeenCalledTimes(1);
    } finally {
      stop();
    }
  });
});

describe("the headline without a signal", () => {
  const noSignal = () =>
    sampleStatus({ headline: "noSignal", source: { ...sampleStatus().source, port: "none" } });

  it("names the input the user chose, or none while the board watches all of them", async () => {
    await loadSettings();
    expect(connection.value).not.toBe("mismatch");
    expect(headline(noSignal(), "auto")).toBe("No signal");
    expect(headline(noSignal(), undefined)).toBe("No signal");
    expect(headline(noSignal(), "rgb")).toBe("No signal on RGB (SCART/RGBS/VGA)");
  });

  it("never prints the board's internal word for no port", () => {
    expect(headline(noSignal(), "component")).not.toContain("none");
  });
});
