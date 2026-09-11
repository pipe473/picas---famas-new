"use client";

import { memo, useEffect, useRef, useState } from "react";
import { useNow } from "@/lib/clock";
import { fmt } from "@/lib/format";
import { abortMatch, goHome } from "@/lib/api";
import { ThemeToggle } from "@/components/ThemeToggle";
import { SoundToggle } from "@/components/SoundDock";
import { ShareInvite } from "@/components/ShareInvite";
import { COLORS, PHASE_LABEL, type GameState } from "@/lib/types";

const PHASE_CLASS: Record<string, string> = {
  countdown: "warm",
  playing: "live",
  summary: "cool",
  matchend: "gold",
};

/** Hay una partida en marcha: abandonar o terminar afecta a la mesa. */
function inMatch(S: GameState) {
  return S.phase === "countdown" || S.phase === "playing" || S.phase === "summary";
}

const Timer = memo(function Timer({ S }: { S: GameState }) {
  const n = useNow();
  let value: string;
  let caption: string;
  if (S.phase === "playing") {
    value = fmt(n - S.roundStart);
    caption = "tiempo de ronda";
  } else if (S.phase === "summary") {
    value = fmt(S.phaseEnd - n);
    caption = "siguiente ronda";
  } else if (S.phase === "countdown") {
    value = String(Math.ceil(Math.max(0, S.phaseEnd - n)));
    caption = "empieza en";
  } else {
    return null;
  }
  return (
    <div className="timer" aria-label={`${caption}: ${value}`}>
      <b>{value}</b>
      <small>{caption}</small>
    </div>
  );
});

const TurnInfo = memo(function TurnInfo({ S }: { S: GameState }) {
  const n = useNow();
  if (!S.turnMode || S.turnMode === "simultaneous" || S.phase !== "playing") return null;
  const who = S.turnPlayer >= 0 ? S.players[S.turnPlayer] : null;
  if (!who) return <span>Turnos {S.turnMode === "random" ? "aleatorios" : "por asiento"}</span>;
  const left = who.deadline > 0 ? Math.max(0, who.deadline - n).toFixed(1) : null;
  const me = who.seat === S.humanSeat;
  return (
    <span>
      {me ? (
        <b className="turn-me">Tu turno</b>
      ) : (
        <>
          Turno de <b style={{ color: COLORS[who.seat] }}>{who.name}</b>
        </>
      )}
      {left !== null ? <> · {left}s</> : null}
    </span>
  );
});

const Status = memo(function Status({ S }: { S: GameState }) {
  const label = PHASE_LABEL[S.phase] ?? S.phase;
  const showRound = S.round > 0 && S.phase !== "lobby";
  return (
    <div className="status">
      <Timer S={S} />
      <div className="status-text">
        <div className="status-main">
          <span className={`phase ${PHASE_CLASS[S.phase] ?? ""}`}>{label}</span>
          {showRound ? (
            <span>
              Ronda <b>{S.round}</b> de {S.rounds}
            </span>
          ) : (
            <span>
              {S.players.length} {S.players.length === 1 ? "jugador" : "jugadores"}
            </span>
          )}
        </div>
        <div className="status-sub">
          <span>
            Código de <b>{S.len}</b> dígitos
          </span>
          {S.roomCode ? (
            <span>
              Sala <b>{S.roomCode}</b>
            </span>
          ) : null}
          {S.freeTime && S.phase !== "lobby" ? (
            <span title="Sin reloj de intento: cada cual tira cuando quiere">
              Tiempo <b>libre</b>
            </span>
          ) : null}
          <TurnInfo S={S} />
        </div>
      </div>
    </div>
  );
});

type Pending = "abort" | "leave" | null;

function HeaderMenu({ S }: { S: GameState }) {
  const [open, setOpen] = useState(false);
  const [pending, setPending] = useState<Pending>(null);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!open) return;
    const onDown = (e: PointerEvent) => {
      if (!ref.current?.contains(e.target as Node)) {
        setOpen(false);
        setPending(null);
      }
    };
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        setOpen(false);
        setPending(null);
      }
    };
    document.addEventListener("pointerdown", onDown);
    document.addEventListener("keydown", onKey);
    return () => {
      document.removeEventListener("pointerdown", onDown);
      document.removeEventListener("keydown", onKey);
    };
  }, [open]);

  const close = () => {
    setOpen(false);
    setPending(null);
  };

  const busy = inMatch(S);
  const canAbort = S.isHost && busy;

  const run = (what: Exclude<Pending, null>) => {
    // Cortar una partida en marcha se confirma en dos toques; en la sala de espera no hace falta.
    if (busy && pending !== what) {
      setPending(what);
      return;
    }
    close();
    void (what === "abort" ? abortMatch() : goHome());
  };

  return (
    <div className="menu" ref={ref}>
      <button
        type="button"
        className="menu-btn"
        aria-haspopup="menu"
        aria-expanded={open}
        onClick={() => (open ? close() : setOpen(true))}
      >
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" aria-hidden="true">
          <path d="M4 7h16M4 12h16M4 17h16" />
        </svg>
        Menú
      </button>
      {open ? (
        <div className="menu-pop" role="menu">
          {S.roomCode ? (
            <div className="menu-room">
              <div>
                <span className="lbl">Sala</span>
                <b className="code">{S.roomCode}</b>
              </div>
              <ShareInvite code={S.roomCode} compact />
            </div>
          ) : null}

          {canAbort ? (
            pending === "abort" ? (
              <div className="menu-confirm" role="alertdialog" aria-label="Confirmar terminar partida">
                <span>La partida termina para todos los jugadores. ¿Seguro?</span>
                <div className="row">
                  <button type="button" onClick={() => setPending(null)}>
                    No, seguir
                  </button>
                  <button type="button" className="yes" onClick={() => run("abort")}>
                    Sí, terminar
                  </button>
                </div>
              </div>
            ) : (
              <button type="button" role="menuitem" className="menu-item danger" onClick={() => run("abort")}>
                Terminar la partida
                <small>Para todos · solo el anfitrión</small>
              </button>
            )
          ) : null}

          {pending === "leave" ? (
            <div className="menu-confirm" role="alertdialog" aria-label="Confirmar salir de la sala">
              <span>Hay una partida en marcha. Si sales, sigue sin ti. ¿Seguro?</span>
              <div className="row">
                <button type="button" onClick={() => setPending(null)}>
                  No, seguir
                </button>
                <button type="button" className="yes" onClick={() => run("leave")}>
                  Sí, salir
                </button>
              </div>
            </div>
          ) : (
            <button type="button" role="menuitem" className="menu-item" onClick={() => run("leave")}>
              Salir de la sala
              <small>Volver a la pantalla de inicio</small>
            </button>
          )}
        </div>
      ) : null}
    </div>
  );
}

export const Header = memo(function Header({ S }: { S: GameState }) {
  return (
    <header>
      <div className="brand">
        <h1>
          PICAS <span>y</span> FAMAS
        </h1>
        <span className="tag">El Enigma Único en Tiempo Real</span>
      </div>
      {S.joined ? <Status S={S} /> : <div className="status" />}
      <div className="actions">
        <SoundToggle />
        <ThemeToggle />
        {S.joined ? <HeaderMenu S={S} /> : null}
      </div>
    </header>
  );
});
