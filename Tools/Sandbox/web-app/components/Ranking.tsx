"use client";

import { memo, useEffect, useState, type CSSProperties, type ReactNode } from "react";
import { TrophyIcon } from "@/components/Icons";
import { fetchRanking } from "@/lib/api";
import { COLORS, type GameState, type GlobalEntry, type RankingResponse, type SoloEntry } from "@/lib/types";

const PACE_LABEL: Record<string, string> = { slow: "tranquilo", normal: "normal", fast: "frenético" };
const TURNS_LABEL: Record<string, string> = { simultaneous: "a la vez", seat: "por turnos", random: "turnos aleatorios" };

/** Hora si la partida es de hoy; si no, día y mes. Suficiente para distinguir "esta tarde" de "el mes pasado". */
function when(ts: number): string {
  if (!ts) return "";
  const d = new Date(ts * 1000);
  const sameDay = d.toDateString() === new Date().toDateString();
  return sameDay
    ? d.toLocaleTimeString("es", { hour: "2-digit", minute: "2-digit" })
    : d.toLocaleDateString("es", { day: "numeric", month: "short" });
}

function Pos({ n }: { n: number }) {
  return (
    <span className={`rank-pos${n >= 1 && n <= 3 ? ` top t${n}` : ""}`} aria-label={`puesto ${n}`}>
      {n}
    </span>
  );
}

export function RankHead({ children, sub }: { children: ReactNode; sub?: string }) {
  return (
    <div className="rank-head">
      <TrophyIcon />
      <h4>{children}</h4>
      {sub ? <small>{sub}</small> : null}
    </div>
  );
}

/**
 * Ranking individual (global, persistente en el servidor): las mejores partidas contra bots.
 * `highlightId` resalta la partida propia; si queda fuera del top se añade al final con su puesto real.
 */
export const SoloRanking = memo(function SoloRanking({ limit = 10, highlightId = 0 }: { limit?: number; highlightId?: number }) {
  const [data, setData] = useState<RankingResponse | null | undefined>(undefined);

  useEffect(() => {
    let alive = true;
    setData(undefined);
    void fetchRanking(limit, highlightId).then((r) => {
      if (alive) setData(r && r.ok ? r : null);
    });
    return () => {
      alive = false;
    };
  }, [limit, highlightId]);

  if (data === undefined) {
    return (
      <div className="loading" aria-label="Cargando ranking">
        <i />
        <i />
        <i />
      </div>
    );
  }
  if (data === null) return <div className="rank-empty">No se pudo cargar el ranking.</div>;
  if (!data.solo.length) {
    return (
      <div className="rank-empty">
        Todavía no hay partidas individuales. <b>Juega contra bots</b> y estrena el ranking.
      </div>
    );
  }

  const rows: SoloEntry[] = [...data.solo];
  if (data.mine && !rows.some((e) => e.id === data.mine!.id)) rows.push(data.mine);

  return (
    <table className="rank-table" aria-label="Ranking individual">
      <thead>
        <tr>
          <th>#</th>
          <th>Jugador</th>
          <th className="n">Puntos</th>
          <th className="n">Rondas</th>
          <th className="n opt">Cuándo</th>
        </tr>
      </thead>
      <tbody>
        {rows.map((e) => (
          <tr key={e.id} className={e.id === highlightId ? "me" : ""}>
            <td>
              <Pos n={e.pos} />
            </td>
            <td className="who">
              {e.name}
              <small>
                {e.bots} {e.bots === 1 ? "bot" : "bots"} · {PACE_LABEL[e.pace] ?? e.pace}
                {e.turns && e.turns !== "simultaneous" ? ` · ${TURNS_LABEL[e.turns] ?? e.turns}` : ""}
              </small>
            </td>
            <td className="n pts">{e.score}</td>
            <td className="n" title="rondas ganadas / jugadas">
              {e.roundsWon}/{e.rounds}
            </td>
            <td className="n opt muted">{when(e.ts)}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
});

/** Clasificación de la sala: acumulada entre las partidas con amigos jugadas en esta misma sala. */
export const RoomRanking = memo(function RoomRanking({ S }: { S: GameState }) {
  const rows = S.roomRanking ?? [];
  if (!rows.length) {
    return (
      <div className="rank-empty">
        La clasificación de la sala se rellena al <b>terminar cada partida</b> con al menos dos amigos. Cuenta victorias, podios y puntos.
      </div>
    );
  }
  return (
    <table className="rank-table" aria-label="Clasificación de la sala">
      <thead>
        <tr>
          <th>#</th>
          <th>Jugador</th>
          <th className="n">Victorias</th>
          <th className="n opt">Podios</th>
          <th className="n">Puntos</th>
          <th className="n">Partidas</th>
        </tr>
      </thead>
      <tbody>
        {rows.map((r) => {
          const me = r.seat >= 0 && r.seat === S.humanSeat;
          const style: CSSProperties | undefined = r.seat >= 0 ? ({ ["--tone" as string]: COLORS[r.seat] } as CSSProperties) : undefined;
          return (
            <tr key={r.name} className={`${me ? "me" : ""}${r.seat < 0 ? " gone" : ""}`} style={style}>
              <td>
                <Pos n={r.pos} />
              </td>
              <td className="who tone">
                {r.name}
                {me ? <small>tú</small> : r.seat < 0 ? <small>ya no está en la sala</small> : null}
              </td>
              <td className="n pts">{r.wins}</td>
              <td className="n opt">{r.podiums}</td>
              <td className="n">{r.points}</td>
              <td className="n muted">{r.matches}</td>
            </tr>
          );
        })}
      </tbody>
    </table>
  );
});

/**
 * Ranking global (persistente): la trayectoria de cada jugador sumando todas sus partidas, individuales
 * y con amigos. `highlightName` resalta al jugador propio y lo añade al final si queda fuera del top.
 */
export const GlobalRanking = memo(function GlobalRanking({ limit = 10, highlightName = "" }: { limit?: number; highlightName?: string }) {
  const [data, setData] = useState<RankingResponse | null | undefined>(undefined);

  useEffect(() => {
    let alive = true;
    setData(undefined);
    void fetchRanking(limit, 0, highlightName).then((r) => {
      if (alive) setData(r && r.ok ? r : null);
    });
    return () => {
      alive = false;
    };
  }, [limit, highlightName]);

  if (data === undefined) {
    return (
      <div className="loading" aria-label="Cargando ranking">
        <i />
        <i />
        <i />
      </div>
    );
  }
  if (data === null) return <div className="rank-empty">No se pudo cargar el ranking.</div>;
  if (!data.global?.length) {
    return (
      <div className="rank-empty">
        Todavía no hay jugadores en el ranking global. <b>Termina una partida</b>, contra bots o con amigos, y aparecerás aquí.
      </div>
    );
  }

  const rows: GlobalEntry[] = [...data.global];
  if (data.me && !rows.some((e) => e.name === data.me!.name)) rows.push(data.me);

  return (
    <table className="rank-table" aria-label="Ranking global">
      <thead>
        <tr>
          <th>#</th>
          <th>Jugador</th>
          <th className="n">Puntos</th>
          <th className="n">Victorias</th>
          <th className="n">Partidas</th>
          <th className="n opt">Media</th>
        </tr>
      </thead>
      <tbody>
        {rows.map((e) => (
          <tr key={e.name} className={highlightName && e.name === highlightName ? "me" : ""}>
            <td>
              <Pos n={e.pos} />
            </td>
            <td className="who">
              {e.name}
              <small>
                {e.solo} {e.solo === 1 ? "individual" : "individuales"} · {e.room} con amigos
                {e.best > 0 ? ` · mejor ${e.best}` : ""}
              </small>
            </td>
            <td className="n pts">{e.points}</td>
            <td className="n">{e.wins}</td>
            <td className="n muted">{e.matches}</td>
            <td className="n opt muted" title="puntos por partida">
              {e.matches ? Math.round(e.points / e.matches) : 0}
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  );
});

type Tab = "solo" | "room" | "global";

/** Nombre con el que juega quien mira (para resaltarlo en el ranking global), o "" si aún no está sentado. */
export function myName(S: GameState): string {
  if (!S.joined) return "";
  return S.players.find((p) => p.seat === S.humanSeat)?.name ?? "";
}

/** Los tres rankings con pestañas. La de sala solo tiene sentido dentro de una sala. */
export function RankingTabs({ S, initial }: { S: GameState; initial?: Tab }) {
  const inRoom = S.joined && !!S.roomCode;
  const [tab, setTab] = useState<Tab>(initial ?? (inRoom && S.roomRanking.length ? "room" : "solo"));
  const active: Tab = tab === "room" && !inRoom ? "solo" : tab;
  const tabBtn = (t: Tab, label: ReactNode) => (
    <button type="button" role="tab" aria-selected={active === t} className={active === t ? "on" : ""} onClick={() => setTab(t)}>
      {label}
    </button>
  );
  return (
    <>
      <div className="rank-tabs" role="tablist" aria-label="Tipo de ranking">
        {tabBtn("solo", "Individual")}
        {inRoom
          ? tabBtn(
              "room",
              <>
                Sala {S.roomCode}
                {S.roomMatches > 0 ? <span className="rank-count">{S.roomMatches}</span> : null}
              </>,
            )
          : null}
        {tabBtn("global", "Global")}
      </div>
      {active === "solo" ? (
        <>
          <p className="sub rank-note">Mejores partidas contra bots de todos los jugadores de este servidor.</p>
          <SoloRanking limit={10} highlightId={S.solo ? S.soloId : 0} />
        </>
      ) : active === "room" ? (
        <>
          <p className="sub rank-note">
            {S.roomMatches > 0
              ? `${S.roomMatches} ${S.roomMatches === 1 ? "partida jugada" : "partidas jugadas"} en esta sala. Gana quien más partidas gana; a igualdad, más puntos.`
              : "Acumula las partidas con amigos de esta sala."}
          </p>
          <RoomRanking S={S} />
        </>
      ) : (
        <>
          <p className="sub rank-note">
            Trayectoria de cada jugador sumando <b>todas</b> sus partidas, contra bots y con amigos. Ordena por puntos totales; a igualdad, más victorias.
          </p>
          <GlobalRanking limit={10} highlightName={myName(S)} />
        </>
      )}
    </>
  );
}

/** Diálogo de ranking abierto desde el menú: por encima de todo, se cierra con Escape o el aspa. */
export function RankingDialog({ S, onClose }: { S: GameState; onClose: () => void }) {
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") onClose();
    };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [onClose]);

  return (
    <div
      className="overlay on rank-overlay"
      onPointerDown={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div className="card rank-card" role="dialog" aria-modal="true" aria-labelledby="rank-title">
        <button type="button" className="rank-close" aria-label="Cerrar ranking" onClick={onClose}>
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.4" strokeLinecap="round" aria-hidden="true">
            <path d="M6 6l12 12M18 6L6 18" />
          </svg>
        </button>
        <h3 id="rank-title">Ranking</h3>
        <div className="title small">
          <TrophyIcon /> Clasificaciones
        </div>
        <RankingTabs S={S} />
      </div>
    </div>
  );
}
