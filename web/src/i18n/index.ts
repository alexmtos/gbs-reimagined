import { computed, signal } from "@preact/signals";
import en from "../gen/i18n/en.gen.json";
import ptBR from "../gen/i18n/pt-BR.gen.json";
import type { SettingGroup, SettingId } from "../gen/settings.gen";

// Strings come from schema/i18n/*.yaml through the generator, so the web page, the OLED and the
// docs say the same thing. English is the reference; the generator fails on a missing key.
type Catalogue = typeof en;
export type MessageKey = keyof Catalogue["messages"];
export type Locale = "en" | "pt-BR";

const catalogues: Record<Locale, Catalogue> = { en, "pt-BR": ptBR as Catalogue };

export const LOCALES: readonly { id: Locale; name: string }[] = [
  { id: "en", name: en.name },
  { id: "pt-BR", name: ptBR.name },
];

function initialLocale(): Locale {
  try {
    const stored = localStorage.getItem("gbsr.locale");
    if (stored === "en" || stored === "pt-BR") return stored;
  } catch {
    // storage unavailable: fall through to the browser language
  }
  return navigator.language.toLowerCase().startsWith("pt") ? "pt-BR" : "en";
}

export const locale = signal<Locale>(initialLocale());
const catalogue = computed(() => catalogues[locale.value]);

export function setLocale(next: Locale): void {
  locale.value = next;
  document.documentElement.lang = next;
  try {
    localStorage.setItem("gbsr.locale", next);
  } catch {
    // not persisted; the page still switches
  }
}

/** A message with `{name}` parameters substituted. */
export function t(key: MessageKey, params: Record<string, string | number> = {}): string {
  const template = catalogue.value.messages[key] ?? en.messages[key] ?? key;
  return template.replace(/\{(\w+)\}/g, (_, name: string) =>
    name in params ? String(params[name]) : `{${name}}`,
  );
}

type SettingStrings = {
  label: string;
  hint: string;
  help: string;
  values?: Record<string, string>;
};

export function settingText(id: SettingId): SettingStrings {
  const all = catalogue.value.settings as Record<string, SettingStrings>;
  return (
    all[id] ??
    (en.settings as Record<string, SettingStrings>)[id] ?? {
      label: id,
      hint: "",
      help: "",
    }
  );
}

export function valueLabel(id: SettingId, value: string): string {
  return settingText(id).values?.[value] ?? value;
}

export function groupLabel(group: SettingGroup): string {
  return (catalogue.value.groups as Record<string, string>)[group] ?? group;
}

export function formatNumber(value: number, fractionDigits = 0): string {
  return new Intl.NumberFormat(locale.value, {
    minimumFractionDigits: fractionDigits,
    maximumFractionDigits: fractionDigits,
  }).format(value);
}
