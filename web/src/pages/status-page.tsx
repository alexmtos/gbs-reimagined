import type { Status } from "../api/types";
import type { MessageKey } from "../i18n";
import { formatNumber, t, valueLabel } from "../i18n";
import { status } from "../state/status";

function headline(s: Status): string {
  switch (s.headline) {
    case "noSignal":
      return t("headline.noSignal", { input: valueLabel("input.port", s.source.port) });
    case "scaling":
      return t("headline.scaling", {
        source: s.source.standard || t("common.unknown"),
        output: valueLabel("output.resolution", s.output.mode),
      });
    default:
      return t(`headline.${s.headline}` as MessageKey);
  }
}

function Row({ label, value }: { label: string; value: string }) {
  return (
    <div class="row">
      <dt>{label}</dt>
      <dd>{value}</dd>
    </div>
  );
}

export function StatusPage() {
  const s = status.value;
  if (!s) {
    return <p class="muted">{t("conn.reconnecting")}</p>;
  }
  const lock =
    s.output.frameLock === "locked"
      ? t("common.on")
      : s.output.frameLock === "off"
        ? t("common.off")
        : t((s.output.frameLockReason as MessageKey | undefined) ?? "common.unknown");
  return (
    <section>
      <h1 class={`headline headline-${s.headline}`}>{headline(s)}</h1>
      {s.profile && (
        <p class="muted">
          {t("status.profile")}: {s.profile.slot} · {s.profile.name}
          {s.profile.modified && ` — ${t("profiles.modified")}`}
        </p>
      )}
      <div class="cards">
        <dl class="card">
          <h2>{t("status.source")}</h2>
          <Row label={t("status.lines")} value={s.source.lines ? String(s.source.lines) : "—"} />
          <Row label={t("status.hfreq")} value={`${formatNumber(s.source.hFreqKHz, 2)} kHz`} />
          <Row label={t("status.vfreq")} value={`${formatNumber(s.source.vFreqHz, 3)} Hz`} />
        </dl>
        <dl class="card">
          <h2>{t("status.output")}</h2>
          <Row
            label={t("status.pixelClock")}
            value={`${formatNumber(s.output.pixelClockMHz, 3)} MHz`}
          />
          <Row label={t("status.vfreq")} value={`${formatNumber(s.output.rateHz, 3)} Hz`} />
          <Row label={t("status.frameLock")} value={lock} />
        </dl>
        <dl class="card">
          <h2>{t("status.board")}</h2>
          <Row label={t("status.version")} value={s.board.version} />
          <Row label={t("status.heap")} value={`${formatNumber(s.board.heapFree)} B`} />
          <Row label={t("status.uptime")} value={`${formatNumber(s.board.uptimeS)} s`} />
        </dl>
      </div>
    </section>
  );
}
