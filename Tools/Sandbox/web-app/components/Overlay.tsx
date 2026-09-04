"use client";

import { memo, useEffect, useRef, useState } from "react";
import { sfx } from "@/lib/audio";
import { newMatch, startMatch } from "@/lib/api";
import { useNow } from "@/lib/clock";
import { COLORS, type GameState, type Pace, type TurnMode } from "@/lib/types";

const Countdown = memo(function Countdown({ S }: { S: GameState }) {
  const n = useNow();
  const k = Math.ceil(Math.max(0.01, S.phaseEnd - n));
  const prev = useRef(k);
  useEffect(() => {
    if (prev.current !== k) {
      prev.current = k;
      sfx.tick();
    }
  }, [k]);
  return (
    <>
      <h3>
        Ronda {S.round} de {S.rounds}
      </h3>
      <div className="count">{k}</div>
      <div className="sub">
        Código de {S.len} dígitos · todos a por el mismo enigma
      </div>
    </>
  );
});

const Summary = memo(function Summary({ S }: { S: GameState }) {
  const n = useNow();
  const w = S.winner >= 0 ? S.players[S.winner] : null;
  const rows = [...S.players]
    .sort((a, b) => b.roundScore - a.roundScore)
    .map((p) => (
      <tr key={p.seat}>
        <td style={{ color: COLORS[p.seat], fontWeight: 800 }}>{p.name}</td>
        <td className="n">
          {p.roundScore >= 0 ? "+" : ""}
          {p.roundScore}
        </td>
        <td className="n" style={{ color: "var(--muted)" }}>
          {p.matchScore} total
        </td>
      </tr>
    ));
  return (
    <>
      <h3>
        Ronda {S.round} · el código era
      </h3>
      <div className="code">
        {(S.secret ?? "").split(" ").map((d, i) => (
          <div className="tile" key={i}>
            {d}
          </div>
        ))}
      </div>
      <div className="big" style={{ fontSize: 30, color: w ? COLORS[w.seat] : "var(--muted)" }}>
        {w ? `🏆 ${w.name}` : "Nadie lo descifró"}
      </div>
      <table>
        <tbody>{rows}</tbody>
      </table>
      <div className="sub" style={{ marginTop: 12 }}>
        Siguiente ronda en {Math.ceil(Math.max(0, S.phaseEnd - n))} s
      </div>
    </>
  );
});

const MatchEnd = memo(function MatchEnd({ S, onAgain }: { S: GameState; onAgain: () => void }) {
  const order = [...S.players].sort((a, b) => b.matchScore - a.matchScore);
  const pod = [order[1], order[0], order[2]];
  return (
    <>
      <h3>Fin de la partida</h3>
      <div className="podium">
        {pod.map((p, i) =>
          p ? (
            <div className={`pod p${[2, 1, 3][i]}`} key={p.seat}>
              <div style={{ color: COLORS[p.seat] }}>{p.name}</div>
              <small>{p.matchScore} pts</small>
              <div style={{ fontSize: 26 }}>{["🥈", "🥇", "🥉"][i]}</div>
            </div>
          ) : null,
        )}
      </div>
      <table>
        <tbody>
          {order.slice(3).map((p) => (
            <tr key={p.seat}>
              <td style={{ color: COLORS[p.seat] }}>{p.name}</td>
              <td className="n">{p.matchScore}</td>
            </tr>
          ))}
        </tbody>
      </table>
      <button type="button" className="btn" onClick={onAgain}>
        {S.spectator ? "OTRA PARTIDA" : "UNA MÁS"}
      </button>
    </>
  );
});

function Lobby({ S }: { S: GameState }) {
  const [bots, setBots] = useState(Math.max(2, S.players.length - (S.spectator ? 0 : 1)));
  const [pace, setPace] = useState<Pace>(S.pace || "slow");
  const [attempt, setAttempt] = useState(S.attemptSeconds || 10);
  const [turns, setTurns] = useState<TurnMode>(S.turnMode || "simultaneous");
  const [spec, setSpec] = useState(S.spectator);

  const begin = async () => {
    await newMatch({ bots, human: !spec, pace, attempt, turns });
    await startMatch();
  };

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Enter") void begin();
    };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
    // begin captura el estado actual de los selects.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [bots, pace, attempt, turns, spec]);

  return (
    <>
      <h3>Sala lista</h3>
      <div className="big">{S.players.length} jugadores</div>
      <div className="sub">
        Todos descifráis el <b>mismo código</b> de {S.len} dígitos distintos. Cada intento es público: las pistas de
        tus rivales también son tuyas.
        <br />
        {attempt} s por intento · con {S.len - 1} Famas se activa la Muerte Sudada.
      </div>
      <div className="opts">
        Bots{" "}
        <select value={bots} onChange={(e) => setBots(Number(e.target.value))}>
          {[2, 3, 4, 5, 6, 7].map((b) => (
            <option key={b}>{b}</option>
          ))}
        </select>
        Ritmo{" "}
        <select value={pace} onChange={(e) => setPace(e.target.value as Pace)}>
          <option value="slow">Tranquilo (mesa)</option>
          <option value="normal">Normal</option>
          <option value="fast">Frenético (bots perfectos)</option>
        </select>
        Reloj{" "}
        <select value={attempt} onChange={(e) => setAttempt(Number(e.target.value))}>
          {[10, 15, 20, 30].map((a) => (
            <option key={a}>{a}</option>
          ))}
        </select>{" "}
        s/intento Turnos{" "}
        <select value={turns} onChange={(e) => setTurns(e.target.value as TurnMode)}>
          <option value="simultaneous">Todos a la vez</option>
          <option value="seat">Por orden de asiento</option>
          <option value="random">Orden aleatorio</option>
        </select>
        <label>
          <input type="checkbox" checked={spec} onChange={(e) => setSpec(e.target.checked)} /> solo bots
        </label>
      </div>
      <button type="button" className="btn" onClick={() => void begin()}>
        {spec ? "VER PARTIDA" : "EMPEZAR"}
      </button>
    </>
  );
}

export const Overlay = memo(function Overlay({ S }: { S: GameState }) {
  if (S.phase === "playing") return null;
  return (
    <div className="overlay on">
      <div className="card">
        {S.phase === "lobby" ? <Lobby S={S} /> : null}
        {S.phase === "countdown" ? <Countdown S={S} /> : null}
        {S.phase === "summary" ? <Summary S={S} /> : null}
        {S.phase === "matchend" ? (
          <MatchEnd
            S={S}
            onAgain={async () => {
              await startMatch();
            }}
          />
        ) : null}
      </div>
    </div>
  );
});
