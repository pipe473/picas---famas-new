import type { ApiResult, GameState } from "./types";

// En `next dev` apunta al sandbox C++. El export estático se sirve en el mismo origen.
export const BASE = process.env.NEXT_PUBLIC_API_BASE ?? "";
const TOKEN_KEY = "pf_token";

export function getToken(): string {
  if (typeof window === "undefined") return "";
  return localStorage.getItem(TOKEN_KEY) ?? "";
}

export function setToken(token: string) {
  if (typeof window === "undefined") return;
  if (token) localStorage.setItem(TOKEN_KEY, token);
  else localStorage.removeItem(TOKEN_KEY);
  window.dispatchEvent(new Event("pf-token"));
}

export function roomCodeFromUrl(): string {
  if (typeof window === "undefined") return "";
  return new URLSearchParams(window.location.search).get("sala")?.toUpperCase() ?? "";
}

function withTok(path: string) {
  const t = getToken();
  if (!t) return path;
  return `${path}${path.includes("?") ? "&" : "?"}token=${encodeURIComponent(t)}`;
}

export async function api<T = ApiResult>(path: string): Promise<T | null> {
  try {
    const r = await fetch(`${BASE}${withTok(path)}`);
    return (await r.json()) as T;
  } catch {
    return null;
  }
}

export const fetchState = () => api<GameState>("/api/state");
export const startMatch = () => api("/api/start");
export const suspect = (seq: number) => api(`/api/suspect?seq=${seq}`);
export const leaveRoom = () => api("/api/leave");
export const resetRoom = () => api("/api/reset");

export function startSolo(name: string, bots = 3) {
  const q = new URLSearchParams({ name, bots: String(bots) });
  return api<JoinResult>(`/api/solo?${q.toString()}`);
}

export type JoinResult = ApiResult & { token?: string; code?: string; host?: boolean };

export function joinRoom(name: string, code?: string) {
  const q = new URLSearchParams({ name });
  if (code) q.set("code", code);
  return api<JoinResult>(`/api/join?${q.toString()}`);
}

export function configRoom(opts: { bots?: number; pace?: string; attempt?: number; turns?: string }) {
  const q = new URLSearchParams();
  if (opts.bots !== undefined) q.set("bots", String(opts.bots));
  if (opts.pace) q.set("pace", opts.pace);
  if (opts.attempt !== undefined) q.set("attempt", String(opts.attempt));
  if (opts.turns) q.set("turns", opts.turns);
  return api(`/api/config?${q.toString()}`);
}

export function sendGuess(digits: string, mode: string, fakeF?: number, fakeP?: number) {
  let q = `/api/guess?d=${digits}&mode=${mode}`;
  if (mode === "decoy") q += `&f=${fakeF ?? 0}&p=${fakeP ?? 0}`;
  return api(q);
}

export function shareUrl(code: string) {
  if (typeof window === "undefined") return code;
  const u = new URL(window.location.href);
  u.searchParams.set("sala", code);
  return u.toString();
}

export function streamUrl() {
  return `${BASE}${withTok("/api/stream")}`;
}
