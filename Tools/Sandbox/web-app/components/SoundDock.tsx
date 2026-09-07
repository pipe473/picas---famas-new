"use client";

import { memo, useEffect, useState, type FormEvent } from "react";
import {
  KIND_LABEL,
  MODE_LABEL,
  embedUrl,
  openUrl,
  parseSpotifyLink,
  sound,
  soundModeOf,
  useSound,
  type SoundMode,
} from "@/lib/sound";

const MODE_HELP: Record<SoundMode, string> = {
  music: "Tu Spotify suena de fondo mientras juegas. Los pitidos del juego se pueden apagar más abajo.",
  sfx: "Solo los efectos del juego: intentos, alertas y cuenta atrás. Sin música.",
  silent: "Modo descanso: ni pitidos ni música. Todo lo importante sigue avisando en pantalla.",
};

const MODES: SoundMode[] = ["music", "sfx", "silent"];

/** Botón de la cabecera: muestra el modo actual y abre o pliega el panel. */
export const SoundToggle = memo(function SoundToggle() {
  const s = useSound();
  const mode = soundModeOf(s);
  const open = s.view === "open";
  return (
    <button
      type="button"
      className={`menu-btn sound-btn${open ? " on" : ""}`}
      onClick={() => sound.toggle()}
      aria-label="Ajustes de sonido"
      aria-expanded={open}
      title="Sonido: música de Spotify, solo efectos o silencio"
    >
      <span aria-hidden="true">{mode === "silent" ? "🔇" : "♫"}</span> {MODE_LABEL[mode]}
    </button>
  );
});

function AddSource() {
  const [link, setLink] = useState("");
  const [name, setName] = useState("");
  const [err, setErr] = useState("");

  const submit = (e: FormEvent) => {
    e.preventDefault();
    const parsed = parseSpotifyLink(link);
    if (!parsed) {
      setErr(
        /spotify\.link/i.test(link)
          ? "Los enlaces cortos spotify.link no valen: en Spotify usa Compartir → Copiar enlace."
          : "No parece un enlace de Spotify. Vale un enlace de playlist, álbum, canción, artista o podcast.",
      );
      return;
    }
    const label = name.trim() || `Mi ${KIND_LABEL[parsed.kind].toLowerCase()}`;
    sound.add({ ...parsed, label: label.slice(0, 40) });
    setLink("");
    setName("");
    setErr("");
  };

  return (
    <form className="sound-add" onSubmit={submit}>
      <input
        type="text"
        inputMode="url"
        autoComplete="off"
        spellCheck={false}
        placeholder="Pega un enlace de Spotify (playlist, álbum, canción…)"
        value={link}
        onChange={(e) => {
          setLink(e.target.value);
          setErr("");
        }}
        aria-label="Enlace de Spotify"
      />
      <div className="sound-add-row">
        <input
          type="text"
          maxLength={40}
          placeholder="Nombre (opcional)"
          value={name}
          onChange={(e) => setName(e.target.value)}
          aria-label="Nombre de la lista"
        />
        <button type="submit" className="sound-add-btn" disabled={!link.trim()}>
          AÑADIR
        </button>
      </div>
      {err ? <div className="sound-err">{err}</div> : null}
    </form>
  );
}

function MusicPanel() {
  const s = useSound();
  const current = s.sources.find((x) => x.id === s.currentId) ?? null;
  return (
    <>
      {s.sources.length ? (
        <div className="sound-list" role="listbox" aria-label="Tus listas">
          {s.sources.map((src) => {
            const active = src.id === s.currentId;
            return (
              <div key={src.id} className={`src${active ? " on" : ""}`} role="option" aria-selected={active}>
                <button type="button" className="src-pick" onClick={() => sound.select(src.id)} title={openUrl(src)}>
                  <b>{src.label}</b>
                  <small>{KIND_LABEL[src.kind]}</small>
                </button>
                <button
                  type="button"
                  className="src-del"
                  onClick={() => sound.remove(src.id)}
                  aria-label={`Quitar ${src.label}`}
                  title="Quitar de la lista"
                >
                  ×
                </button>
              </div>
            );
          })}
        </div>
      ) : (
        <div className="sub sound-empty">
          No tienes listas guardadas.{" "}
          <button type="button" className="linkish" onClick={() => sound.restoreSuggested()}>
            Recuperar las sugeridas
          </button>
        </div>
      )}
      <AddSource />
      <label className="sound-check">
        <input type="checkbox" checked={s.sfx} onChange={(e) => sound.setSfx(e.target.checked)} />
        Mantener los efectos del juego (alertas, cuenta atrás)
      </label>
      <div className="sub sound-hint">
        Sin sesión en Spotify solo suenan 30 s por canción. Entra desde el propio reproductor para escuchar completo.
        {current ? (
          <>
            {" "}
            <a href={openUrl(current)} target="_blank" rel="noreferrer noopener">
              Abrir en Spotify ↗
            </a>
          </>
        ) : null}
      </div>
    </>
  );
}

/**
 * Panel flotante de sonido. El iframe de Spotify vive aquí y no se desmonta al plegar u ocultar
 * el panel, así la música no se corta al cambiar de fase, abrir la sala o volver al menú.
 */
export const SoundDock = memo(function SoundDock() {
  const s = useSound();
  const mode = soundModeOf(s);
  const current = s.music ? (s.sources.find((x) => x.id === s.currentId) ?? null) : null;
  const open = s.view === "open";

  useEffect(() => {
    if (!open) return;
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") sound.close();
    };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [open]);

  if (!open && !current) return null;

  const view = open ? "open" : s.view === "hidden" ? "hidden" : "mini";

  return (
    <aside className={`sound-dock ${view}`} aria-label="Sonido" aria-hidden={view === "hidden"}>
      {open ? (
        <div className="sound-panel">
          <div className="sound-head">
            <h2>Sonido</h2>
            <button type="button" className="src-del" onClick={() => sound.close()} aria-label="Cerrar panel de sonido">
              ×
            </button>
          </div>
          <div className="sound-modes" role="radiogroup" aria-label="Modo de sonido">
            {MODES.map((m) => (
              <button
                key={m}
                type="button"
                role="radio"
                aria-checked={mode === m}
                className={`mode${mode === m ? " on" : ""}`}
                onClick={() => sound.setMode(m)}
              >
                {MODE_LABEL[m]}
              </button>
            ))}
          </div>
          <div className="sub sound-help">{MODE_HELP[mode]}</div>
          {mode === "music" ? <MusicPanel /> : null}
        </div>
      ) : view === "mini" ? (
        <div className="sound-mini">
          <button type="button" className="sound-mini-open" onClick={() => sound.open()} title="Abrir el panel de sonido">
            <span aria-hidden="true">♫</span>
            <b>{current?.label}</b>
            <small>Spotify</small>
          </button>
          <button
            type="button"
            className="src-del"
            onClick={() => sound.hide()}
            aria-label="Ocultar el reproductor (la música sigue)"
            title="Ocultar: la música sigue; vuelve desde el botón de la cabecera"
          >
            ▾
          </button>
        </div>
      ) : null}
      {current ? (
        <div className="sound-player">
          <iframe
            key={current.id}
            title={`Spotify · ${current.label}`}
            src={embedUrl(current)}
            allow="autoplay; clipboard-write; encrypted-media; fullscreen; picture-in-picture"
            loading="lazy"
          />
        </div>
      ) : null}
    </aside>
  );
});
