import { signal } from "@preact/signals";
import type { MessageKey } from "./i18n";
import { LOCALES, locale, setLocale, t } from "./i18n";
import { SettingsPage } from "./pages/settings-page";
import { StatusPage } from "./pages/status-page";
import { connection } from "./state/status";

type PageId = "status" | "picture" | "output" | "input" | "system";
const PAGES: readonly PageId[] = ["status", "picture", "output", "input", "system"];

function pageFromHash(): PageId {
  const id = location.hash.replace("#", "");
  return (PAGES as readonly string[]).includes(id) ? (id as PageId) : "status";
}

const page = signal<PageId>(pageFromHash());
window.addEventListener("hashchange", () => {
  page.value = pageFromHash();
});

function ConnectionPill() {
  const state = connection.value;
  const key: MessageKey =
    state === "live" ? "conn.live" : state === "mismatch" ? "conn.mismatch" : "conn.reconnecting";
  return <span class={`pill pill-${state}`}>{t(key)}</span>;
}

export function App() {
  const current = page.value;
  return (
    <div class="shell">
      <header class="topbar">
        <span class="brand">{t("app.title")}</span>
        <ConnectionPill />
        <select
          class="lang"
          aria-label="Language"
          value={locale.value}
          onChange={(e) => setLocale(e.currentTarget.value as typeof locale.value)}
        >
          {LOCALES.map((l) => (
            <option key={l.id} value={l.id}>
              {l.name}
            </option>
          ))}
        </select>
      </header>
      <nav class="tabs">
        {PAGES.map((id) => (
          <a key={id} href={`#${id}`} class={id === current ? "active" : ""}>
            {t(`nav.${id}` as MessageKey)}
          </a>
        ))}
      </nav>
      <main>{current === "status" ? <StatusPage /> : <SettingsPage group={current} />}</main>
    </div>
  );
}
