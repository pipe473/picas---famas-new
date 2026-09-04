"use client";

import { memo } from "react";
import { Dots } from "@/components/Dots";
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
  return (
    <div className="bar">
      <i className={left > 0 && left < 3 ? "hot" : ""} style={{ width: `${pct}%` }} />
    </div>
  );
});

const PlayerCard = memo(function PlayerCard({
  p,
  S,
}: {
  p: Player;
  S: GameState;
}) {
  const isTurn = S.turnPlayer === p.seat && S.phase === "playing";
  const cls = [
    "player",
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
        <span className="badge green">{p.seat === S.humanSeat ? "TU TURNO" : "SU TURNO"}</span> {badge}
      </>
    );
  }
  const orderIdx = S.turnOrder ? S.turnOrder.indexOf(p.seat) : -1;
  return (
    <div className={cls} style={{ ["--c" as string]: COLORS[p.seat] }}>
      <div className="top">
        <div>
          <span className="name">{p.name}</span>
          <span className="prof">{p.profile === "Tu" ? "humano" : p.profile}</span>
          {orderIdx >= 0 ? (
            <span className="prof" title="orden de turno">
              · {orderIdx + 1}º
            </span>
          ) : null}
        </div>
        <div className="score">
          {p.matchScore} <small>pts</small>
        </div>
      </div>
      <div className="row2">
        <span>
          <Dots f={p.bestF} p={p.bestP} len={S.len} />{" "}
          <span style={{ marginLeft: 6 }}>{p.attempts} int.</span>
        </span>
        <span>
          {badge}{" "}
          <span className="tokens">
            {p.encrypt ? "🔒" : <s>🔒</s>} {p.decoy ? "🎭" : <s>🎭</s>}
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
  return (
    <section className="panel">
      <h2>Jugadores</h2>
      <div>
        {S.players.map((p) => (
          <PlayerCard key={p.seat} p={p} S={S} />
        ))}
      </div>
      <div className="legend">
        <span className="dots">
          <span className="dot f" />
        </span>{" "}
        Fama (dígito y posición){" "}
        <span className="dots">
          <span className="dot p" />
        </span>{" "}
        Pica (dígito, otra posición)
      </div>
    </section>
  );
});
