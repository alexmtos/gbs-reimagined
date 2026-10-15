// The short message under a setting or row: the board's answer to the last thing done there. Only
// a question the user must answer (warn) or a failure (bad) is coloured; news is plain text.

export type Tone = "info" | "warn" | "bad";

export interface Notice {
  text: string;
  tone: Tone;
}

export const info = (text: string): Notice => ({ text, tone: "info" });
export const warn = (text: string): Notice => ({ text, tone: "warn" });
export const bad = (text: string): Notice => ({ text, tone: "bad" });

export function Note({ notice }: { notice: Notice | null }) {
  // role=status: screen readers announce it politely when it appears or changes.
  return notice ? (
    <p class={`setting-note ${notice.tone}`} role="status">
      {notice.text}
    </p>
  ) : null;
}
