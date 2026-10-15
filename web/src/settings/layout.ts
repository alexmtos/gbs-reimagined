import type { Status } from "../api/types";
import type { SettingGroup, SettingId, SettingMeta } from "../gen/settings.gen";
import type { MessageKey } from "../i18n";

// How the settings are laid out on the web page: in sections, some shown elsewhere, and with a
// reason when one has no effect right now (docs/design/ux.md, principle 4).

export interface Section {
  title: MessageKey;
  ids: readonly SettingId[];
}

/** Sections of each page, in order. A setting in none of them comes first, without a title. */
export const SECTIONS: Readonly<Partial<Record<SettingGroup, readonly Section[]>>> = {
  output: [
    { title: "section.signal", ids: ["output.resolution", "output.frameLock", "output.force60"] },
    {
      title: "section.shape",
      ids: [
        "output.aspect",
        "output.integerScale",
        "output.hPosition",
        "output.vPosition",
        "output.hSize",
        "output.vSize",
      ],
    },
  ],
  picture: [
    {
      title: "section.colour",
      ids: ["picture.brightness", "picture.contrast", "picture.saturation", "picture.sharpness"],
    },
    {
      title: "section.scanlines",
      ids: ["picture.scanlines", "picture.scanlineStrength", "picture.deinterlace"],
    },
  ],
  system: [
    { title: "section.boardScreen", ids: ["system.oledSaver", "system.encoderReverse"] },
    { title: "section.changes", ids: ["system.revertSeconds", "system.showAdvanced"] },
  ],
};

/** Chosen elsewhere on the page: the language in the header, the startup profile on Profiles. */
export const ELSEWHERE: ReadonlySet<string> = new Set(["system.language", "system.startupProfile"]);

const PICTURE_AND_SHAPE: ReadonlySet<string> = new Set([
  "picture.brightness",
  "picture.contrast",
  "picture.saturation",
  "picture.sharpness",
  "picture.scanlines",
  "picture.scanlineStrength",
  "picture.deinterlace",
  "output.aspect",
  "output.integerScale",
  "output.hPosition",
  "output.vPosition",
  "output.hSize",
  "output.vSize",
]);
const NOT_AT_15KHZ: ReadonlySet<string> = new Set([
  "output.aspect",
  "output.integerScale",
  "output.hSize",
  "output.vSize",
  "picture.scanlines",
  "picture.scanlineStrength",
]);

/**
 * Why a setting has no effect right now, or null. From what the board reports (the mode in effect,
 * the source) and the other settings; with no source the page cannot tell, and says nothing.
 * Firmware: video/engine.cpp (bypass, scanlines), video/geometry.cpp (15 kHz, integer scale).
 */
export function idleReason(
  id: string,
  s: Status | null,
  values: Readonly<Record<string, string>>,
): MessageKey | null {
  const mode = s?.output.mode;
  if (mode === "passthrough" && PICTURE_AND_SHAPE.has(id)) return "idle.passthrough";
  if (mode === "r15khz" && NOT_AT_15KHZ.has(id)) return "idle.crt";
  if (id === "picture.scanlineStrength" && values["picture.scanlines"] === "off") {
    return "idle.scanlinesOff";
  }
  if (
    id === "picture.scanlines" &&
    values["picture.scanlines"] === "on" &&
    s?.source.standard &&
    s.source.standard !== "240p" &&
    s.source.standard !== "288p"
  ) {
    return "idle.scanlinesSource";
  }
  if (id === "output.vSize" && values["output.integerScale"] === "on") return "idle.integerScale";
  return null;
}

/** A value the list offers: Passthrough only with the advanced settings, or when in use. */
export function offered(id: string, value: string, current: string, advanced: boolean): boolean {
  return id !== "output.resolution" || value !== "passthrough" || advanced || current === value;
}

/** The value text of a setting's default, as the board writes values. */
export function defaultText(meta: SettingMeta): string {
  if (meta.type === "bool") return meta.default ? "on" : "off";
  if (meta.type === "int") {
    return meta.special?.[String(meta.default)] ?? String(meta.default);
  }
  return String(meta.default);
}
