import { useEffect, useState } from "preact/hooks";
import { fetchWifi, runCommand } from "../api/endpoints";
import { t } from "../i18n";
import { loadSettings } from "../state/settings";
import { pollOnce, status } from "../state/status";

/** A risky change waits for confirmation; the board reverts it on its own when the time runs out. */
export function RevertBanner() {
  const [busy, setBusy] = useState(false);
  const revert = status.value?.revert;
  if (!revert) return null;
  const act = async (name: "keep" | "revert") => {
    setBusy(true);
    await runCommand(name);
    await Promise.all([pollOnce(), loadSettings()]);
    setBusy(false);
  };
  return (
    // Sticky, so a change made low on a phone's page still shows its countdown. The question is
    // announced once; the ticking countdown is for the eyes only.
    <div class="banner banner-warn banner-sticky">
      <span class="sr-only" role="alert">
        {t("revert.ask")}
      </span>
      <span aria-hidden="true">{t("revert.countdown", { seconds: revert.secondsLeft })}</span>
      <span class="banner-actions">
        <button type="button" class="primary" disabled={busy} onClick={() => act("keep")}>
          {t("common.keep")}
        </button>
        <button type="button" disabled={busy} onClick={() => act("revert")}>
          {t("common.revert")}
        </button>
      </span>
    </div>
  );
}

export function SafeModeBanner() {
  if (!status.value?.safeMode) return null;
  return (
    <div class="banner banner-bad" role="status">
      {t("status.safeMode")}
    </div>
  );
}

/**
 * On the board's own setup network (no Wi-Fi joined yet), every page points to the Network page:
 * a phone that lands on Status would otherwise not know what to do next.
 */
export function SetupBanner() {
  const [setup, setSetup] = useState(false);
  useEffect(() => {
    fetchWifi()
      .then((w) => setSetup(w.mode !== "joined" && w.ap.active))
      .catch(() => setSetup(false));
  }, []);
  if (!setup) return null;
  return (
    <div class="banner banner-warn">
      <span>{t("setup.banner")}</span>
      <a class="button primary" href="#network">
        {t("setup.link")}
      </a>
    </div>
  );
}
