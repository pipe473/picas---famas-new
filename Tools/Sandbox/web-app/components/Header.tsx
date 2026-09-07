"use client";

import { memo } from "react";
import { useNow } from "@/lib/clock";
import { fmt } from "@/lib/format";
import { abortMatch, goHome } from "@/lib/api";
import { COLORS, PHASE_LABEL, type GameState } from "@/lib/types";

const TurnChip = memo(function TurnChip({ S }: { S: GameState }) {
  const n = useNow();
  if (!S.turnMode || S.turnMode === "simultaneous") return null;
  const who = S.turnPlayer >= 0 ? S.players[S.turnPlayer] : null;
  const left = who && who.deadline > 0 ? Math.max(0, who.deadline - n).toFixed(1) : null;
  return (
    <div className="chip">
      Turnos {S.turnMode === "random" ? "aleatorios" : "por asiento"}
      {who ? (
        <>
          {" · "}
          <b style={{ color: COLORS[who.seat] }}>{who.name}</b>
          {left !== null ? (
            <>
              {" "}
              <b>{left}s</b>
            </>
          ) : null}
        </>
      ) : null}
    </div>
  );
});

const Timer = memo(function Timer({ S }: { S: GameState }) {
  const n = useNow();
  const text =
    S.phase === "playing"
      ? fmt(n - S.roundStart)
      : S.phase === "summary"
        ? fmt(S.phaseEnd - n)
        : "—";
  return <div className="timer">{text}</div>;
});

export const Header = memo(function Header({ S }: { S: GameState }) {
  return (
    <header>
      <h1>
        PICAS <span>y</span> FAMAS
        <span className="tag">El Enigma Único en Tiempo Real</span>
      </h1>
      <div className="hud">
        <div className="chip">
          Ronda <b>{S.round}/{S.rounds}</b>
        </div>
        <div className="chip">
          Código de <b>{S.len}</b> dígitos
        </div>
        <TurnChip S={S} />
        <div className="chip">
          <b>{PHASE_LABEL[S.phase] ?? S.phase}</b>
        </div>
        {S.roomCode ? (
          <div className="chip">
            Sala <b>{S.roomCode}</b>
          </div>
        ) : null}
        <Timer S={S} />
        {S.joined && S.isHost && S.phase !== "lobby" && S.phase !== "none" && S.phase !== "matchend" ? (
          <button type="button" className="menu-btn" onClick={() => void abortMatch()}>
            Terminar
          </button>
        ) : null}
        {S.joined ? (
          <button type="button" className="menu-btn" onClick={() => void goHome()}>
            Menú
          </button>
        ) : null}
      </div>
    </header>
  );
});
