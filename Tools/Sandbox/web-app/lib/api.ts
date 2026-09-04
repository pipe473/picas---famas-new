import type { ApiResult, GameState } from "./types";

// En `next dev` apunta al sandbox C++. El export estático se sirve en el mismo origen.
const BASE = process.env.NEXT_PUBLIC_API_BASE ?? "";

export async function api<T = ApiResult>(path: string): Promise<T | null> {
  try {
    const r = await fetch(`${BASE}${path}`);
    return (await r.json()) as T;
  } catch {
    return null;
  }
}

export const fetchState = () => api<GameState>("/api/state");
export const startMatch = () => api("/api/start");
export const suspect = (seq: number) => api(`/api/suspect?seq=${seq}`);

export function newMatch(opts: {
  bots: number;
  human: boolean;
  pace: string;
  attempt: number;
  turns: string;
}) {
  const q = new URLSearchParams({
    bots: String(opts.bots),
    human: opts.human ? "1" : "0",
    pace: opts.pace,
    attempt: String(opts.attempt),
    turns: opts.turns,
  });
  return api(`/api/new?${q.toString()}`);
}

export function sendGuess(digits: string, mode: string, fakeF?: number, fakeP?: number) {
  let q = `/api/guess?d=${digits}&mode=${mode}`;
  if (mode === "decoy") q += `&f=${fakeF ?? 0}&p=${fakeP ?? 0}`;
  return api(q);
}
