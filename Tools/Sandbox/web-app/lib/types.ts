export type Phase = "lobby" | "countdown" | "playing" | "summary" | "matchend" | "none";
export type Pace = "slow" | "normal" | "fast";
export type TurnMode = "simultaneous" | "seat" | "random";
export type GuessMode = "plain" | "encrypt" | "decoy";

export type Player = {
  seat: number;
  name: string;
  profile: string;
  bestF: number;
  bestP: number;
  attempts: number;
  roundScore: number;
  matchScore: number;
  deadline: number;
  encrypt: boolean;
  decoy: boolean;
  inactive: boolean;
  solved: boolean;
};

export type BoardEntry = {
  seq: number;
  player: number;
  guess: string;
  f: number;
  p: number;
  hidden: boolean;
  decoyRevealed: boolean;
  suspected: boolean;
  keyClue: boolean;
  solved: boolean;
  bits: number;
  t: number;
  reveal: number;
};

export type PrivateEntry = { seq: number; guess: string; f: number; p: number };
export type LogEvent = { id: number; text: string };

export type GameState = {
  phase: Phase;
  round: number;
  rounds: number;
  len: number;
  now: number;
  phaseEnd: number;
  roundStart: number;
  spectator: boolean;
  humanSeat: number;
  joined: boolean;
  isHost: boolean;
  roomCode: string;
  bots: number;
  minPlayers: number;
  maxPlayers: number;
  pace: Pace;
  /** Segundos por intento. 0 = tiempo libre (sin reloj). */
  attemptSeconds: number;
  sdAttemptSeconds: number;
  /** Tiempo libre: sin reloj de intento, sin tope de ronda ni cuenta atrás de Muerte Sudada. */
  freeTime: boolean;
  turnMode: TurnMode;
  turnPlayer: number;
  turnNumber: number;
  turnOrder: number[];
  secret: string | null;
  winner: number;
  alertPlayer: number;
  suddenDeathEnd: number;
  players: Player[];
  entries: BoardEntry[];
  private: PrivateEntry[];
  events: LogEvent[];
};

export type ApiResult = { ok: boolean; error?: string };
export type ToastItem = { id: number; text: string; cls?: string; leaving?: boolean };

export const COLORS = [
  "var(--c0)",
  "var(--c1)",
  "var(--c2)",
  "var(--c3)",
  "var(--c4)",
  "var(--c5)",
  "var(--c6)",
  "var(--c7)",
] as const;

export const PHASE_LABEL: Record<string, string> = {
  lobby: "Sala",
  countdown: "Preparados…",
  playing: "EN JUEGO",
  summary: "Resumen",
  matchend: "Fin",
};
