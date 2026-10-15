// Every request carries a deadline: a request the board never answers must not stall the poll
// loop forever (reference-project lesson P17).
export const REQUEST_TIMEOUT_MS = 8000;

export class ApiError extends Error {
  constructor(
    message: string,
    readonly status: number,
  ) {
    super(message);
    this.name = "ApiError";
  }
}

export async function getJson<T>(path: string, timeoutMs = REQUEST_TIMEOUT_MS): Promise<T> {
  const response = await fetch(path, {
    headers: { Accept: "application/json" },
    signal: AbortSignal.timeout(timeoutMs),
  });
  if (!response.ok) {
    throw new ApiError(`${path}: HTTP ${response.status}`, response.status);
  }
  return (await response.json()) as T;
}

export async function postJson<T>(
  path: string,
  body: unknown,
  timeoutMs = REQUEST_TIMEOUT_MS,
): Promise<T> {
  const response = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json", Accept: "application/json" },
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(timeoutMs),
  });
  if (!response.ok) {
    throw new ApiError(`${path}: HTTP ${response.status}`, response.status);
  }
  return (await response.json()) as T;
}

/**
 * Like postJson, but a refusal that carries a JSON body (a reason) is an answer, not an error.
 * Throws only when the board does not answer or answers something that is not JSON.
 */
export async function postForAnswer<T>(
  path: string,
  body: unknown,
  timeoutMs = REQUEST_TIMEOUT_MS,
): Promise<T> {
  const response = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json", Accept: "application/json" },
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(timeoutMs),
  });
  try {
    return (await response.json()) as T;
  } catch {
    throw new ApiError(`${path}: HTTP ${response.status}`, response.status);
  }
}
