import { setupServer } from "msw/node";
import { render } from "preact";
import { act } from "preact/test-utils";
import { afterAll, afterEach, beforeAll, describe, expect, it, vi } from "vitest";
import { setLocale } from "../src/i18n";
import { handlers, sampleStatus } from "../src/mocks/handlers";
import { InputActions } from "../src/pages/settings-page";
import { status } from "../src/state/status";

const server = setupServer(...handlers);

beforeAll(() => {
  server.listen({ onUnhandledFrame: "error" });
  setLocale("en");
});
afterEach(() => {
  server.resetHandlers();
  render(null, document.body);
});
afterAll(() => server.close());

const note = () => document.body.querySelector("p.setting-note")?.textContent ?? "";

describe("Calibrate levels", () => {
  it("reports a refusal that ended between two polls, never seen running", async () => {
    status.value = sampleStatus({ levels: "none" });
    await act(() => {
      render(<InputActions />, document.body);
    });
    const button = document.body.querySelector("button");
    await act(async () => {
      button?.click();
    });
    await vi.waitFor(() => expect(note()).toContain("keep the white screen"));
    await act(() => {
      status.value = sampleStatus({ levels: "notWhite" });
    });
    await vi.waitFor(() => expect(note()).toBe("That is not a white screen: nothing was changed"));
    expect(document.body.querySelector("p.setting-note")?.classList.contains("bad")).toBe(true);
  });

  it("does not report the outcome of an earlier run on opening", async () => {
    status.value = sampleStatus({ levels: "done" });
    await act(() => {
      render(<InputActions />, document.body);
    });
    expect(note()).toBe("");
  });
});
