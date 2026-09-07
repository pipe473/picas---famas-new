import { useSyncExternalStore } from "react";

/**
 * Ajustes de sonido del HUD: efectos del juego (pitidos) y música de Spotify.
 * Todo vive en el navegador (localStorage); la sala no sabe nada de esto.
 */

export type SpotifyKind = "playlist" | "album" | "track" | "artist" | "show" | "episode";
export type SpotifySource = { kind: SpotifyKind; id: string; label: string };
export type SoundMode = "music" | "sfx" | "silent";

export type SoundSettings = {
  sfx: boolean;
  music: boolean;
  sources: SpotifySource[];
  currentId: string | null;
};

/** open: panel completo · mini: barra con reproductor compacto · hidden: nada visible (la música sigue). */
export type DockView = "open" | "mini" | "hidden";
export type SoundState = SoundSettings & { view: DockView };

export const SOUND_KEY = "pf-sound";

export const KIND_LABEL: Record<SpotifyKind, string> = {
  playlist: "Playlist",
  album: "Álbum",
  track: "Canción",
  artist: "Artista",
  show: "Podcast",
  episode: "Episodio",
};

/** Listas editoriales de Spotify para empezar; el usuario puede borrarlas y añadir las suyas. */
export const SUGGESTED_SOURCES: SpotifySource[] = [
  { kind: "playlist", id: "37i9dQZF1DWZeKCadgRdKQ", label: "Deep Focus" },
  { kind: "playlist", id: "37i9dQZF1DWWQRwui0ExPn", label: "Lofi Beats" },
  { kind: "playlist", id: "37i9dQZF1DX4sWSpwq3LiO", label: "Peaceful Piano" },
];

const DEFAULTS: SoundSettings = {
  sfx: true,
  music: false,
  sources: SUGGESTED_SOURCES,
  currentId: SUGGESTED_SOURCES[0]?.id ?? null,
};

const SERVER_STATE: SoundState = { ...DEFAULTS, view: "hidden" };

const KINDS = new Set<string>(Object.keys(KIND_LABEL));
const ID_RE = /^[A-Za-z0-9]{10,40}$/;

/**
 * Acepta enlaces de "Compartir" (`open.spotify.com/playlist/ID?si=…`, con o sin `intl-xx/`),
 * enlaces de embed y URIs `spotify:playlist:ID`. Los acortadores `spotify.link` no se pueden
 * resolver desde el navegador.
 */
export function parseSpotifyLink(input: string): { kind: SpotifyKind; id: string } | null {
  const raw = input.trim();
  if (!raw) return null;

  const uri = /^spotify:([a-z]+):([A-Za-z0-9]+)$/i.exec(raw);
  if (uri) {
    const kind = uri[1].toLowerCase();
    return KINDS.has(kind) && ID_RE.test(uri[2]) ? { kind: kind as SpotifyKind, id: uri[2] } : null;
  }

  let url: URL;
  try {
    url = new URL(/^https?:\/\//i.test(raw) ? raw : `https://${raw}`);
  } catch {
    return null;
  }
  if (!/(^|\.)spotify\.com$/i.test(url.hostname)) return null;

  const parts = url.pathname.split("/").filter(Boolean);
  if (parts[0]?.startsWith("intl-")) parts.shift();
  if (parts[0] === "embed" || parts[0] === "embed-podcast") parts.shift();
  const kind = parts[0]?.toLowerCase();
  const id = parts[1];
  if (!kind || !id || !KINDS.has(kind) || !ID_RE.test(id)) return null;
  return { kind: kind as SpotifyKind, id };
}

/** El embed toma el color de la portada, así que encaja igual en modo día y noche. */
export function embedUrl(src: SpotifySource): string {
  return `https://open.spotify.com/embed/${src.kind}/${src.id}?utm_source=generator`;
}

export function openUrl(src: SpotifySource): string {
  return `https://open.spotify.com/${src.kind}/${src.id}`;
}

export function soundModeOf(s: Pick<SoundSettings, "sfx" | "music">): SoundMode {
  if (s.music) return "music";
  return s.sfx ? "sfx" : "silent";
}

export const MODE_LABEL: Record<SoundMode, string> = {
  music: "Música",
  sfx: "Efectos",
  silent: "Silencio",
};

// ---------------------------------------------------------------------------
// Store mínimo (sin dependencias) con persistencia en localStorage.

let state: SoundState | null = null;
const listeners = new Set<() => void>();

function isSource(v: unknown): v is SpotifySource {
  if (!v || typeof v !== "object") return false;
  const o = v as Record<string, unknown>;
  return typeof o.kind === "string" && KINDS.has(o.kind) && typeof o.id === "string" && ID_RE.test(o.id) && typeof o.label === "string";
}

function load(): SoundState {
  if (typeof window === "undefined") return SERVER_STATE;
  try {
    const raw = localStorage.getItem(SOUND_KEY);
    if (!raw) return { ...DEFAULTS, view: "hidden" };
    const o = JSON.parse(raw) as Partial<SoundSettings>;
    const sources = Array.isArray(o.sources) ? o.sources.filter(isSource) : DEFAULTS.sources;
    const currentId =
      typeof o.currentId === "string" && sources.some((s) => s.id === o.currentId)
        ? o.currentId
        : (sources[0]?.id ?? null);
    const music = typeof o.music === "boolean" ? o.music : DEFAULTS.music;
    return {
      sfx: typeof o.sfx === "boolean" ? o.sfx : DEFAULTS.sfx,
      music,
      sources,
      currentId,
      // Al recargar, la música arranca parada (el navegador exige un gesto), así que se muestra la barra
      // para que el play quede a un toque.
      view: music && currentId ? "mini" : "hidden",
    };
  } catch {
    return { ...DEFAULTS, view: "hidden" };
  }
}

function persist(s: SoundState) {
  try {
    const settings: SoundSettings = { sfx: s.sfx, music: s.music, sources: s.sources, currentId: s.currentId };
    localStorage.setItem(SOUND_KEY, JSON.stringify(settings));
  } catch {
    /* cuota / modo privado */
  }
}

export function getSoundState(): SoundState {
  if (!state) state = load();
  return state;
}

function setSoundState(patch: Partial<SoundState>) {
  const prev = getSoundState();
  state = { ...prev, ...patch };
  persist(state);
  listeners.forEach((fn) => fn());
}

function subscribe(fn: () => void) {
  listeners.add(fn);
  return () => {
    listeners.delete(fn);
  };
}

export function useSound(): SoundState {
  return useSyncExternalStore(subscribe, getSoundState, () => SERVER_STATE);
}

/** Los efectos consultan esto en cada pitido: sin re-render ni suscripción. */
export function sfxEnabled(): boolean {
  return getSoundState().sfx;
}

export const sound = {
  setMode(mode: SoundMode) {
    if (mode === "music") setSoundState({ music: true, sfx: true });
    else if (mode === "sfx") setSoundState({ music: false, sfx: true });
    else setSoundState({ music: false, sfx: false });
  },
  setSfx(sfx: boolean) {
    setSoundState({ sfx });
  },
  open() {
    setSoundState({ view: "open" });
  },
  /** Pliega el panel: a barra si hay música que controlar, a nada si no. */
  close() {
    const s = getSoundState();
    setSoundState({ view: s.music && s.currentId ? "mini" : "hidden" });
  },
  hide() {
    setSoundState({ view: "hidden" });
  },
  toggle() {
    if (getSoundState().view === "open") sound.close();
    else sound.open();
  },
  select(id: string) {
    setSoundState({ currentId: id, music: true });
  },
  add(src: SpotifySource) {
    const rest = getSoundState().sources.filter((s) => s.id !== src.id);
    setSoundState({ sources: [src, ...rest], currentId: src.id, music: true });
  },
  remove(id: string) {
    const s = getSoundState();
    const sources = s.sources.filter((x) => x.id !== id);
    const currentId = s.currentId === id ? (sources[0]?.id ?? null) : s.currentId;
    setSoundState({ sources, currentId });
  },
  restoreSuggested() {
    const s = getSoundState();
    const have = new Set(s.sources.map((x) => x.id));
    const sources = [...s.sources, ...SUGGESTED_SOURCES.filter((x) => !have.has(x.id))];
    setSoundState({ sources, currentId: s.currentId ?? sources[0]?.id ?? null });
  },
};
