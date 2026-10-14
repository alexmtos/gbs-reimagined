import { useState } from "preact/hooks";
import { runCommand } from "../api/endpoints";
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
    <div class="banner banner-warn" role="alert">
      <span>{t("revert.countdown", { seconds: revert.secondsLeft })}</span>
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
