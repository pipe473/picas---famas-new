"use client";

import { memo } from "react";
import { Dots } from "@/components/Dots";
import { CrownIcon, LockIcon, MaskIcon, TrophyIcon } from "@/components/Icons";
import { useNow } from "@/lib/clock";
import { COLORS, type GameState, type Player } from "@/lib/types";

const DeadlineBar = memo(function DeadlineBar({
  deadline,
  attemptSeconds,
  sdAttemptSeconds,
  alert,
}: {
  deadline: number;
  attemptSeconds: number;
  sdAttemptSeconds: number;
  alert: boolean;
}) {
  const n = useNow();
  const left = deadline > 0 ? Math.max(0, deadline - n) : 0;
  const total = alert ? sdAttemptSeconds || 6 : attemptSeconds || 10;
  const pct = deadline > 0 ? Math.min(100, (left / total) * 100) : 0;
  const closing = left > 0 && left < 3;
  return (
    <>
      {/* Anillo de "a punto de responder": vive aquí para que solo este nodo se repinte a 60 fps. */}
      {closing ? <span className="closing-ring" aria-hidden="true" /> : null}
      <div className="bar">
        <i className={closing ? "hot" : ""} style={{ width: `${pct}%` }} />
      </div>
    </>
  );
});

const PlayerCard = memo(function PlayerCard({
  p,
  S,
  leader,
  wins,
}: {
  p: Player;
  S: GameState;
  leader: boolean;
  wins: number;
}) {
  const isTurn = S.turnPlayer === p.seat && S.phase === "playing";
  // "live": sigue en la ronda (ni resuelto ni inactivo). Enciende el LED de su color en el canto.
  const isLive = S.phase === "playing" && !p.inactive && !p.solved;
  const cls = [
    "player",
    isLive ? "live" : "",
    leader ? "leader" : "",
    p.seat === S.humanSeat ? "me" : "",
    S.alertPlayer === p.seat ? "alert" : "",
    p.solved ? "solved" : "",
    p.inactive ? "inactive" : "",
    isTurn ? "turn" : "",
  ]
    .filter(Boolean)
    .join(" ");
  let badge = p.solved ? (
    <span className="badge gold">ACIERTO</span>
  ) : S.alertPlayer === p.seat ? (
    <span className="badge red">{S.len - 1} FAMAS</span>
  ) : p.inactive ? (
    <span className="badge grey">INACTIVO</span>
  ) : null;
  if (isTurn) {
    badge = (
      <>
        <span className="badge green">{p.seat === S.humanSeat ? "TU TURNO" : "SU TURNO"}</span>
        {badge}
      </>
    );
  }
  const orderIdx = S.turnOrder ? S.turnOrder.indexOf(p.seat) : -1;
  return (
    <div className={cls} style={{ ["--c" as string]: COLORS[p.seat] }}>
      <div className="top">
        <div className="id">
          <span className="name">
            {p.name}
            {leader ? (
              <span className="crown" title="Líder de la partida">
                <CrownIcon />
              </span>
            ) : null}
            {wins > 0 ? (
              <span className="wins" title={`${wins} ${wins === 1 ? "partida ganada" : "partidas ganadas"} en esta sala`}>
                <TrophyIcon />
                {wins}
              </span>
            ) : null}
          </span>
          <span className="prof">
            {p.profile === "Tu" ? "humano" : p.profile}
            {orderIdx >= 0 ? <span title="orden de turno"> · {orderIdx + 1}º</span> : null}
          </span>
        </div>
        <div className="score">
          {p.matchScore} <small>pts</small>
        </div>
      </div>
      <div className="row2">
        <span className="stat">
          <Dots f={p.bestF} p={p.bestP} len={S.len} />
          <span className="att">{p.attempts} int.</span>
        </span>
        <span className="state">
          {badge}
          <span className="tokens" aria-label={`Encriptar ${p.encrypt ? "disponible" : "gastado"}, señuelo ${p.decoy ? "disponible" : "gastado"}`}>
            <span className={`tok${p.encrypt ? "" : " used"}`} title={p.encrypt ? "Encriptar disponible" : "Encriptar gastado"}>
              <LockIcon />
            </span>
            <span className={`tok${p.decoy ? "" : " used"}`} title={p.decoy ? "Señuelo disponible" : "Señuelo gastado"}>
              <MaskIcon />
            </span>
          </span>
        </span>
      </div>
      <DeadlineBar
        deadline={p.deadline}
        attemptSeconds={S.attemptSeconds}
        sdAttemptSeconds={S.sdAttemptSeconds}
        alert={S.alertPlayer >= 0}
      />
    </div>
  );
});

export const PlayerList = memo(function PlayerList({ S }: { S: GameState }) {
  // Líder: máxima puntuación de partida, solo si ya hay puntos y no hay empate arriba.
  const top = Math.max(0, ...S.players.map((p) => p.matchScore));
  const leaders = S.players.filter((p) => p.matchScore === top && top > 0);
  const leaderSeat = leaders.length === 1 ? leaders[0].seat : -1;
  // Victorias acumuladas en la sala (solo humanos, por nombre): pequeño trofeo junto al nombre.
  const winsByName = new Map((S.roomRanking ?? []).map((r) => [r.name, r.wins]));
  return (
    <section className="panel">
      <h2>Jugadores</h2>
      <div className="players">
        {S.players.map((p) => (
          <PlayerCard key={p.seat} p={p} S={S} leader={p.seat === leaderSeat} wins={p.profile === "humano" ? (winsByName.get(p.name) ?? 0) : 0} />
        ))}
      </div>
      <div className="legend">
        <span className="lg">
          <span className="dots">
            <span className="dot f" />
          </span>
          Fama (dígito y posición)
        </span>
        <span className="lg">
          <span className="dots">
            <span className="dot p" />
          </span>
          Pica (dígito, otra posición)
        </span>
      </div>
    </section>
  );
});
