import { delay, HttpResponse, http } from "msw";
import { setupServer } from "msw/node";
import { afterAll, afterEach, beforeAll, describe, expect, it } from "vitest";
import { getJson } from "../src/api/client";
import { handlers, sampleStatus } from "../src/mocks/handlers";
import { connection, pollOnce, status } from "../src/state/status";

const server = setupServer(...handlers);
const base = "http://board.test";

beforeAll(() => {
  server.listen({ onUnhandledFrame: "error" });
});
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

describe("status polling", () => {
  it("publishes the board's status and goes live", async () => {
    await pollOnce();
    expect(connection.value).toBe("live");
    expect(status.value?.source.standard).toBe("240p");
  });

  it("flags a firmware whose settings schema differs from the page", async () => {
    server.use(
      http.get(`${base}/api/status`, () =>
        HttpResponse.json(sampleStatus({ board: { ...sampleStatus().board, schema: "deadbeef" } })),
      ),
    );
    await pollOnce();
    expect(connection.value).toBe("mismatch");
  });

  it("reports reconnecting when the board stops answering", async () => {
    server.use(http.get(`${base}/api/status`, () => HttpResponse.error()));
    await pollOnce();
    expect(connection.value).toBe("reconnecting");
  });
});

describe("api client", () => {
  it("gives up after its deadline instead of hanging", async () => {
    server.use(
      http.get(`${base}/api/slow`, async () => {
        await delay(2000);
        return HttpResponse.json({});
      }),
    );
    await expect(getJson(`${base}/api/slow`, 50)).rejects.toThrow();
  });
});
