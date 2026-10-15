import { signal } from "@preact/signals";
import { RevertBanner, SafeModeBanner, SetupBanner } from "./components/banners";
import type { MessageKey } from "./i18n";
import { LOCALES, locale, settingText, t } from "./i18n";
import { NetworkPage } from "./pages/network-page";
import { ProfilesPage } from "./pages/profiles-page";
import { SettingsPage } from "./pages/settings-page";
import { StatusPage } from "./pages/status-page";
import { chooseLanguage } from "./state/settings";
import { connection } from "./state/status";

type PageId = "status" | "picture" | "output" | "input" | "profiles" | "system" | "network";
// The board's menu order (Output, Input, Picture, Profiles, Network, System), after Status.
const PAGES: readonly PageId[] = [
  "status",
  "output",
  "input",
  "picture",
  "profiles",
  "network",
  "system",
];

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
          aria-label={settingText("system.language").label}
          value={locale.value}
          onChange={(e) => void chooseLanguage(e.currentTarget.value as typeof locale.value)}
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
      <SafeModeBanner />
      {current !== "network" && <SetupBanner />}
      <RevertBanner />
      <main>
        {current === "status" ? (
          <StatusPage />
        ) : current === "network" ? (
          <NetworkPage />
        ) : current === "profiles" ? (
          <ProfilesPage />
        ) : (
          <SettingsPage group={current} />
        )}
      </main>
    </div>
  );
}
