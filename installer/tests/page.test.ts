import { describe, expect, it } from "vitest";
import { offers } from "../src/channels";
import { format, LOCALES, pickLocale } from "../src/i18n";

describe("pickLocale", () => {
  it("keeps the language chosen before", () => {
    expect(pickLocale("pt-BR", ["en-US"]).locale).toBe("pt-BR");
  });

  it("otherwise follows the browser, matching on the language alone", () => {
    expect(pickLocale(null, ["pt-PT", "en"]).locale).toBe("pt-BR");
    expect(pickLocale(null, ["fr-FR", "en-GB"]).locale).toBe("en");
  });

  it("falls back to English", () => {
    expect(pickLocale("xx", ["de-DE"]).locale).toBe("en");
  });

  it("every locale has every string", () => {
    const keys = Object.keys(LOCALES[0]?.strings ?? {}).sort();
    for (const l of LOCALES) {
      expect(Object.keys(l.strings).sort()).toEqual(keys);
      for (const text of Object.values(l.strings)) expect(text.trim()).not.toBe("");
    }
  });
});

describe("format", () => {
  it("fills placeholders and leaves unknown ones", () => {
    expect(format("Stable {version}", { version: "1.0.0" })).toBe("Stable 1.0.0");
    expect(format("{a} {b}", { a: "x" })).toBe("x {b}");
  });
});

describe("offers", () => {
  const file = { gbsc: { ota: "firmware/x.ota.bin.gz", sha256: "00", size: 1 } };

  it("lists the staged channels, stable first, with their notes", () => {
    const latest = {
      format: "gbsr-latest",
      version: 1,
      beta: { version: "1.1.0-beta.1", files: file },
      stable: { version: "1.0.0", notes: "https://example.org/v1.0.0", files: file },
    };
    expect(offers(latest)).toEqual([
      {
        channel: "stable",
        manifest: "manifest.json",
        version: "1.0.0",
        notes: "https://example.org/v1.0.0",
      },
      { channel: "beta", manifest: "manifest-beta.json", version: "1.1.0-beta.1" },
    ]);
  });

  it("offers nothing for a missing or foreign file", () => {
    expect(offers(null)).toEqual([]);
    expect(offers({ format: "other", stable: { version: "1.0.0" } })).toEqual([]);
    expect(offers({ format: "gbsr-latest", version: 1 })).toEqual([]);
  });
});
