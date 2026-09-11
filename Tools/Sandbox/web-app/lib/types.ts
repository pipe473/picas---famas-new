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

/** Fila de la clasificación de la sala: acumulada entre partidas con amigos, por nombre. `seat` = -1 si ya no está. */
export type RoomStanding = {
  pos: number;
  name: string;
  seat: number;
  matches: number;
  wins: number;
  podiums: number;
  points: number;
  best: number;
  roundsWon: number;
  lastRank: number;
};

/** Partida del ranking individual global (contra bots). `ts` en segundos Unix. */
export type SoloEntry = {
  pos: number;
  id: number;
  name: string;
  score: number;
  bots: number;
  pace: string;
  turns: string;
  rounds: number;
  roundsWon: number;
  len: number;
  ts: number;
};

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
  /** La última partida fue individual (1 humano + bots) y se registró en el ranking global. */
  solo: boolean;
  soloRank: number;
  soloId: number;
  roomMatches: number;
  roomRanking: RoomStanding[];
};

export type ApiResult = { ok: boolean; error?: string };
export type RankingResponse = ApiResult & { total: number; solo: SoloEntry[]; mine: SoloEntry | null };
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
