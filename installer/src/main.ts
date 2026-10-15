// The bundle ESP Web Tools publishes for the web, with its dependencies inside: the same code its
// project ships and tests (the plain modules need a Material Web older than npm now resolves).
import "esp-web-tools/dist/web/install-button.js";
import "./style.css";
import { type ChannelName, fetchOffers, type Offer } from "./channels";
import { format, type Key, LOCALES, type Locale, pickLocale } from "./i18n";

const LOCALE_KEY = "gbsr-installer-locale";
const SOURCE = "https://github.com/alexmtos/gbs-reimagined";
const GUIDE = `${SOURCE}/blob/main/docs/user-guide/`;

// Storage can be missing or refuse (private windows): the page works without it.
function load(key: string): string | null {
  try {
    return localStorage.getItem(key);
  } catch {
    return null;
  }
}

function save(key: string, value: string): void {
  try {
    localStorage.setItem(key, value);
  } catch {
    // not remembered: fine
  }
}

type Child = Node | string;

function h(tag: string, props: Record<string, string> = {}, ...children: Child[]): HTMLElement {
  const el = document.createElement(tag);
  for (const [name, value] of Object.entries(props)) el.setAttribute(name, value);
  el.append(...children);
  return el;
}

function link(href: string, text: string): HTMLElement {
  return h("a", { href, target: "_blank", rel: "noopener" }, text);
}

let locale: Locale = pickLocale(load(LOCALE_KEY), navigator.languages);
let available: Offer[] | null = null; // null: latest.json not read yet
let chosen: ChannelName = "stable";

function channelPicker(t: Record<Key, string>, offers: Offer[]): HTMLElement {
  const label = (o: Offer) => format(t[o.channel], { version: o.version });
  const list =
    offers.length > 1
      ? offers.map((o) => {
          const input = h("input", {
            type: "radio",
            name: "channel",
            value: o.channel,
          }) as HTMLInputElement;
          input.checked = o.channel === chosen;
          input.addEventListener("change", () => {
            chosen = o.channel;
            render();
          });
          return h("label", {}, input, ` ${label(o)}`);
        })
      : offers.map(label);
  return h(
    "div",
    { class: "channels" },
    h("span", { class: "channels-label" }, t.channel),
    h("div", { class: "channel-list" }, ...list),
  );
}

function installBox(t: Record<Key, string>): HTMLElement {
  const offers = available ?? [];
  const offer = offers.find((o) => o.channel === chosen) ?? offers[0];
  const activate = h("button", { slot: "activate", type: "button", class: "activate" }, t.install);
  (activate as HTMLButtonElement).disabled = offer === undefined;
  const button = h(
    "esp-web-install-button",
    { manifest: offer?.manifest ?? "manifest.json" },
    activate,
    h("p", { slot: "unsupported", class: "warning" }, t.unsupported),
    h("p", { slot: "not-allowed", class: "warning" }, t.notAllowed),
  );
  const box = h("section", { class: "install" });
  if (offers.length > 0) box.append(channelPicker(t, offers));
  box.append(button);
  if (available !== null && offer === undefined) box.append(h("p", { class: "warning" }, t.none));
  if (offer?.notes) box.append(link(offer.notes, t.notes));
  return box;
}

function render(): void {
  const t = locale.strings;
  document.documentElement.lang = locale.locale;
  document.title = t.title;

  const languages = h("nav", { class: "languages", "aria-label": t.language });
  for (const l of LOCALES) {
    const button = h(
      "button",
      { type: "button", lang: l.locale, "aria-pressed": String(l === locale) },
      l.name,
    );
    button.addEventListener("click", () => {
      locale = l;
      save(LOCALE_KEY, l.locale);
      render();
    });
    languages.append(button);
  }

  const steps = (["step1", "step2", "step3", "step4", "step5"] as const).map((k) =>
    h("li", {}, t[k]),
  );
  const trouble = (["troubleNoPort", "troubleBusy", "troubleWifi", "troubleStopped"] as const).map(
    (k) => h("li", {}, t[k]),
  );
  document
    .getElementById("page")
    ?.replaceChildren(
      h("header", {}, h("h1", {}, t.title), languages),
      h(
        "main",
        {},
        h("p", { class: "lead" }, t.lead),
        installBox(t),
        h("section", {}, h("h2", {}, t.steps), h("ol", {}, ...steps)),
        h("section", {}, h("h2", {}, t.update), h("p", {}, t.updateBody)),
        h("section", {}, h("h2", {}, t.trouble), h("ul", {}, ...trouble)),
      ),
      h(
        "footer",
        {},
        h(
          "p",
          {},
          link(`${GUIDE}${locale.locale}/README.md`, t.guide),
          " · ",
          link(SOURCE, t.source),
        ),
        h("p", {}, t.licence),
      ),
    );
}

render();
void fetchOffers().then((found) => {
  available = found;
  render();
});
