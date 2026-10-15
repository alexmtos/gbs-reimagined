import { computed, effect, signal } from "@preact/signals";
import { fetchSettings, saveSettings } from "../api/endpoints";
import type { SetOutcome } from "../api/types";
import { type Locale, locale, setLocale } from "../i18n";
import { status } from "./status";

/** Per control: unchanged since load (saved), sent and not yet answered (pending), or refused. */
export type FieldState = "saved" | "pending" | "failed";

export const values = signal<Record<string, string>>({});
export const fieldState = signal<Record<string, FieldState>>({});
export const loaded = signal(false);

export async function loadSettings(): Promise<void> {
  try {
    const r = await fetchSettings();
    values.value = r.values;
    fieldState.value = {};
    loaded.value = true;
  } catch {
    // The connection pill already says the board is unreachable.
  }
}

const ACCEPTED: readonly SetOutcome[] = ["applied", "unchanged", "revertArmed"];

/** Shows the new value at once and rolls it back if the board refuses it. */
export async function changeSetting(id: string, value: string): Promise<SetOutcome | "failed"> {
  const previous = values.value[id];
  values.value = { ...values.value, [id]: value };
  fieldState.value = { ...fieldState.value, [id]: "pending" };
  let outcome: SetOutcome | "failed" = "failed";
  try {
    const r = await saveSettings({ [id]: value });
    outcome = r.results[id] ?? "unknown";
  } catch {
    // network failure: reported as failed below
  }
  const ok = outcome !== "failed" && ACCEPTED.includes(outcome);
  if (!ok && previous !== undefined) {
    values.value = { ...values.value, [id]: previous };
  }
  fieldState.value = { ...fieldState.value, [id]: ok ? "saved" : "failed" };
  return outcome;
}

/**
 * Whether the advanced settings are shown: one setting on the board (system.showAdvanced), so the
 * switch on every page, the System page and the board's own menu always agree, and it is kept.
 */
export const showAdvanced = computed(() => values.value["system.showAdvanced"] === "on");

export function setShowAdvanced(on: boolean): Promise<SetOutcome | "failed"> {
  return changeSetting("system.showAdvanced", on ? "on" : "off");
}

// The language is one setting on the board (system.language): the board's screen uses it, and the
// page follows it, so every browser shows the board's language. The header chooses it.
const LANGUAGE_VALUE: Readonly<Record<Locale, string>> = { en: "en", "pt-BR": "ptBR" };

export function followLanguage(): () => void {
  return effect(() => {
    const v = values.value["system.language"];
    const next = (Object.keys(LANGUAGE_VALUE) as Locale[]).find((l) => LANGUAGE_VALUE[l] === v);
    if (next && next !== locale.peek()) setLocale(next);
  });
}

/** Switches the page at once and sets the board's language; a refusal brings the old one back. */
export function chooseLanguage(next: Locale): Promise<SetOutcome | "failed"> {
  setLocale(next);
  return changeSetting("system.language", LANGUAGE_VALUE[next]);
}

/** When a pending revert ends (kept, reverted, or timed out), the values may have moved back. */
export function followReverts(): () => void {
  let wasPending = false;
  return effect(() => {
    const pending = status.value?.revert !== undefined;
    if (wasPending && !pending) void loadSettings();
    wasPending = pending;
  });
}
