import { render } from "preact";
import { afterEach, describe, expect, it } from "vitest";
import { bad, info, Note, warn } from "../src/components/note";

describe("Note", () => {
  afterEach(() => {
    render(null, document.body);
  });

  it("shows news as plain text, and colours only questions and failures", () => {
    for (const [notice, tone] of [
      [info("This is the latest version."), "info"],
      [warn("Press again to confirm."), "warn"],
      [bad("Did not work."), "bad"],
    ] as const) {
      render(<Note notice={notice} />, document.body);
      const p = document.body.querySelector("p.setting-note");
      expect(p?.textContent).toBe(notice.text);
      expect(p?.classList.contains(tone)).toBe(true);
      expect(p?.classList.length).toBe(2);
    }
  });

  it("shows nothing without a notice", () => {
    render(<Note notice={null} />, document.body);
    expect(document.body.querySelector("p")).toBeNull();
  });
});
