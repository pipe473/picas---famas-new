"use client";

import { memo, useEffect, useRef } from "react";
import { Dots } from "@/components/Dots";
import { LockIcon } from "@/components/Icons";
import { Tiles } from "@/components/Tiles";
import { sfx } from "@/lib/audio";
import { useNow } from "@/lib/clock";
import { fmt } from "@/lib/format";
import { COLORS, type BoardEntry, type GameState } from "@/lib/types";

const EncryptedLabel = memo(function EncryptedLabel({ reveal }: { reveal: number }) {
  const n = useNow();
  return (
    <span className="lock">
      <LockIcon /> Encriptado · {Math.max(0, reveal - n).toFixed(0)}s
    </span>
  );
});

const EntryRow = memo(function EntryRow({
  e,
  S,
  isNew,
  onSuspect,
}: {
  e: BoardEntry;
  S: GameState;
  isNew: boolean;
  onSuspect: (seq: number) => void;
}) {
  const p = S.players[e.player];
  const me = S.humanSeat;
  const canSuspect = me >= 0 && e.player !== me && !e.hidden && !e.decoyRevealed && !e.suspected && S.phase === "playing";
  const cls = [
    "entry",
    isNew ? "new" : "",
    e.solved ? "solved" : "",
    e.hidden ? "hidden" : "",
    e.decoyRevealed ? "decoy" : "",
    e.player === me ? "mine" : "",
  ]
    .filter(Boolean)
    .join(" ");
  return (
    <div className={cls} style={{ ["--c" as string]: COLORS[e.player] }}>
      <div className="seq">#{e.seq}</div>
      <div className="who">{p ? p.name : "?"}</div>
      <Tiles code={e.guess} />
      <div className="result">
        {e.hidden ? <EncryptedLabel reveal={e.reveal} /> : <Dots f={e.f} p={e.p} len={S.len} big reveal={isNew} />}
        <span className="lbl">
          {e.hidden ? "resultado oculto" : `${e.f} Fama${e.f !== 1 ? "s" : ""} · ${e.p} Pica${e.p !== 1 ? "s" : ""}`}
          {!e.hidden ? (
            <span className="bits" style={{ marginLeft: 8 }}>
              <b>+{e.bits.toFixed(1)}</b> bits
            </span>
          ) : null}{" "}
          {e.decoyRevealed ? <span className="badge red">ERA SEÑUELO</span> : null}
          {e.suspected ? <span className="badge violet">SOSPECHADO</span> : null}
          {e.keyClue ? <span className="badge blue">PISTA CLAVE</span> : null}
          {e.solved ? <span className="badge gold">¡CÓDIGO!</span> : null}
          {canSuspect ? (
            <button className="suspect" type="button" onClick={() => onSuspect(e.seq)}>
              Sospechar
            </button>
          ) : null}
        </span>
      </div>
      <div className="t">{fmt(e.t - S.roundStart)}</div>
    </div>
  );
});

/** Una fila se considera "nueva" durante este tiempo: cubre entrada + revelado de puntos aunque lleguen más polls. */
const NEW_MS = 1400;

export const Board = memo(function Board({ S, onSuspect }: { S: GameState; onSuspect: (seq: number) => void }) {
  // seq → instante en que se vio por primera vez (0 = ya estaba al montar, sin animación).
  const seen = useRef(new Map<number, number>());
  const first = useRef(true);
  const now = Date.now();
  const rows = [...S.entries].reverse();
  for (const e of S.entries) {
    if (!seen.current.has(e.seq)) seen.current.set(e.seq, first.current ? 0 : now);
  }
  const isNew = (seq: number) => {
    const t = seen.current.get(seq) ?? 0;
    return t > 0 && now - t < NEW_MS;
  };

  const played = useRef(new Set<number>());
  useEffect(() => {
    for (const e of S.entries) {
      if (played.current.has(e.seq)) continue;
      played.current.add(e.seq);
      if ((seen.current.get(e.seq) ?? 0) === 0) continue;
      if (e.solved) sfx.solved();
      else if (e.f >= S.len - 1 && !e.hidden) sfx.fama();
      else sfx.entry(e.f);
    }
    first.current = false;
  }, [S.entries, S.len]);

  return (
    <section className="panel">
      <h2>Módulo del Enigma · todos los intentos son públicos</h2>
      <div className="board">
        {rows.length === 0 ? (
          <div className="empty">
            <b>Aún no hay intentos</b>
            {S.phase === "playing" ? "El primero en enviar abre el tablero para todos." : "Los intentos de la ronda aparecerán aquí."}
          </div>
        ) : (
          rows.map((e) => (
            <EntryRow key={e.seq} e={e} S={S} isNew={isNew(e.seq)} onSuspect={onSuspect} />
          ))
        )}
      </div>
    </section>
  );
});
