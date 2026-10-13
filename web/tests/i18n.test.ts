import { describe, expect, it } from "vitest";
import en from "../src/gen/i18n/en.gen.json";
import ptBR from "../src/gen/i18n/pt-BR.gen.json";
import { SETTINGS } from "../src/gen/settings.gen";
import { setLocale, settingText, t, valueLabel } from "../src/i18n";

describe("i18n", () => {
  it("substitutes parameters", () => {
    setLocale("en");
    expect(t("headline.noSignal", { input: "RGB" })).toBe("No signal on RGB");
    setLocale("pt-BR");
    expect(t("headline.noSignal", { input: "RGB" })).toBe("Sem sinal em RGB");
  });

  it("keeps an unknown parameter visible instead of dropping it", () => {
    setLocale("en");
    expect(t("headline.noSignal")).toBe("No signal on {input}");
  });

  it("labels every enum value of every setting in both locales", () => {
    for (const lang of ["en", "pt-BR"] as const) {
      setLocale(lang);
      for (const meta of SETTINGS) {
        expect(settingText(meta.id).label.length).toBeGreaterThan(0);
        for (const value of meta.values ?? []) {
          expect(valueLabel(meta.id, value)).not.toBe(value === "off" ? "" : undefined);
        }
      }
    }
  });

  it("has the same message keys in both catalogues", () => {
    expect(Object.keys(ptBR.messages).sort()).toEqual(Object.keys(en.messages).sort());
  });
});
