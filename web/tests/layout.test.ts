import { beforeAll, describe, expect, it } from "vitest";
import { SETTINGS } from "../src/gen/settings.gen";
import { setLocale } from "../src/i18n";
import { sampleStatus } from "../src/mocks/handlers";
import { uptimeText } from "../src/pages/status-page";
import { defaultText, ELSEWHERE, idleReason, offered, SECTIONS } from "../src/settings/layout";

beforeAll(() => setLocale("en"));

const status = (mode: string, standard = "240p") =>
  sampleStatus({
    output: { ...sampleStatus().output, mode },
    source: { ...sampleStatus().source, standard },
  });
const on = { "picture.scanlines": "on", "output.integerScale": "off" };

describe("settings with no effect right now", () => {
  it("passthrough leaves the picture and its shape alone", () => {
    expect(idleReason("picture.brightness", status("passthrough"), on)).toBe("idle.passthrough");
    expect(idleReason("output.hPosition", status("passthrough"), on)).toBe("idle.passthrough");
    expect(idleReason("output.frameLock", status("passthrough"), on)).toBeNull();
  });

  it("at 15 kHz only the position works, and there are no scanlines", () => {
    expect(idleReason("output.aspect", status("r15khz"), on)).toBe("idle.crt");
    expect(idleReason("picture.scanlines", status("r15khz"), on)).toBe("idle.crt");
    expect(idleReason("output.hPosition", status("r15khz"), on)).toBeNull();
  });

  it("scanlines pause on interlaced sources, and their strength needs them on", () => {
    expect(idleReason("picture.scanlines", status("r1080p", "480i"), on)).toBe(
      "idle.scanlinesSource",
    );
    expect(idleReason("picture.scanlines", status("r1080p", "240p"), on)).toBeNull();
    expect(
      idleReason("picture.scanlineStrength", status("r1080p"), { "picture.scanlines": "off" }),
    ).toBe("idle.scanlinesOff");
  });

  it("vertical size waits while integer scale is on; nothing is said without a status", () => {
    expect(idleReason("output.vSize", status("r1080p"), { "output.integerScale": "on" })).toBe(
      "idle.integerScale",
    );
    expect(idleReason("picture.brightness", null, on)).toBeNull();
  });
});

describe("page layout", () => {
  it("every section names real settings, each in one section only", () => {
    const ids = new Set(SETTINGS.map((m) => m.id));
    const seen = new Set<string>();
    for (const sections of Object.values(SECTIONS)) {
      for (const section of sections ?? []) {
        for (const id of section.ids) {
          expect(ids.has(id)).toBe(true);
          expect(seen.has(id)).toBe(false);
          seen.add(id);
        }
      }
    }
    for (const id of ELSEWHERE) expect(ids.has(id as (typeof SETTINGS)[number]["id"])).toBe(true);
  });

  it("offers Passthrough with the advanced settings, or when it is in use", () => {
    expect(offered("output.resolution", "passthrough", "auto", false)).toBe(false);
    expect(offered("output.resolution", "passthrough", "auto", true)).toBe(true);
    expect(offered("output.resolution", "passthrough", "passthrough", false)).toBe(true);
    expect(offered("output.resolution", "r1080p", "auto", false)).toBe(true);
  });

  it("writes defaults as the board writes values", () => {
    const meta = (id: string) => {
      const m = SETTINGS.find((x) => x.id === id);
      if (!m) throw new Error(id);
      return m;
    };
    expect(defaultText(meta("output.frameLock"))).toBe("on");
    expect(defaultText(meta("output.resolution"))).toBe("auto");
    expect(defaultText(meta("input.syncThreshold"))).toBe("auto");
    expect(defaultText(meta("output.hPosition"))).toBe("0");
  });

  it("says how long the board has been on in words", () => {
    expect(uptimeText(45)).toBe("45 s");
    expect(uptimeText(200)).toBe("3 min 20 s");
    expect(uptimeText(3720)).toBe("1 h 02 min");
  });
});
