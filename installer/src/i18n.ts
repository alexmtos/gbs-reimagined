import en from "./gen/i18n/en.gen.json";
import ptBR from "./gen/i18n/pt-BR.gen.json";

// The page's strings come from schema/i18n/*.yaml (`installer:`), like the web UI's.

export type Key = keyof typeof en.strings;

export interface Locale {
  locale: string;
  name: string;
  strings: Record<Key, string>;
}

export const LOCALES: readonly Locale[] = [en, ptBR];

/** The locale to start with: the one chosen before, else the browser's first match, else English. */
export function pickLocale(saved: string | null, languages: readonly string[]): Locale {
  const exact = LOCALES.find((l) => l.locale === saved);
  if (exact) return exact;
  for (const language of languages) {
    const base = language.toLowerCase().split("-")[0];
    const match = LOCALES.find((l) => l.locale.toLowerCase().split("-")[0] === base);
    if (match) return match;
  }
  return en;
}

/** Fills {name} placeholders. */
export function format(text: string, values: Readonly<Record<string, string>>): string {
  return text.replace(/\{(\w+)\}/g, (all, name: string) => values[name] ?? all);
}
