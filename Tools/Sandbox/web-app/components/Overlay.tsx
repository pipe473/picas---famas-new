"use client";

import { memo, useEffect, useRef, useState } from "react";
import { sfx } from "@/lib/audio";
import { configRoom, joinRoom, roomCodeFromUrl, setToken, shareUrl, startMatch, startSolo } from "@/lib/api";
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
      {S.isHost ? (
        <button type="button" className="btn" onClick={onAgain}>
          UNA MÁS
        </button>
      ) : (
        <div className="sub" style={{ marginTop: 16 }}>
          Esperando a que el anfitrión abra otra partida…
        </div>
      )}
    </>
  );
});

function CopyLink({ code }: { code: string }) {
  const [ok, setOk] = useState(false);
  const copy = async () => {
    const url = shareUrl(code);
    try {
      await navigator.clipboard.writeText(url);
    } catch {
      window.prompt("Copia este enlace", url);
    }
    setOk(true);
    window.setTimeout(() => setOk(false), 1600);
  };
  return (
    <button type="button" className="btn ghost" onClick={() => void copy()}>
      {ok ? "ENLACE COPIADO" : `COPIAR ENLACE · ${code}`}
    </button>
  );
}

function Join({ S }: { S: GameState }) {
  const invited = !!roomCodeFromUrl();
  const busy = S.phase !== "lobby" && S.phase !== "none";
  const [name, setName] = useState("");
  const [bots, setBots] = useState(3);
  const [err, setErr] = useState("");

  const finish = (r: { ok?: boolean; token?: string; error?: string } | null) => {
    if (!r?.ok || !r.token) {
      setErr(r?.error ?? "no se pudo entrar");
      return false;
    }
    setToken(r.token);
    return true;
  };

  const enterFriends = async () => {
    setErr("");
    setToken("");
    const r = await joinRoom(name.trim() || "Jugador", roomCodeFromUrl() || undefined);
    finish(r);
  };

  const playSolo = async () => {
    setErr("");
    setToken("");
    const r = await startSolo(name.trim() || "Jugador", bots);
    finish(r);
  };

  return (
    <>
      <h3>{invited ? "Te han invitado" : "Cómo quieres jugar"}</h3>
      <div className="big" style={{ fontSize: 36 }}>
        {invited ? S.roomCode : "Picas y Famas"}
      </div>
      <div className="sub">
        {invited
          ? "Entras a la sala de un amigo. Todos atacáis el mismo código."
          : busy
            ? "Hay una partida en pantalla (a menudo una prueba colgada). Empieza una tuya: solo contra bots, o sala para amigos."
            : "Solo: tú contra bots. Amigos: creas una sala y compartes el enlace (Madrid, Barcelona, etc.)."}
      </div>
      <div className="opts">
        <input
          className="namein"
          maxLength={16}
          placeholder="Tu nombre"
          value={name}
          onChange={(e) => setName(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter") void (invited ? enterFriends() : playSolo());
          }}
        />
      </div>
      {err ? (
        <div className="sub" style={{ color: "var(--red)", marginTop: 8 }}>
          {err}
        </div>
      ) : null}
      {invited ? (
        <button type="button" className="btn" onClick={() => void enterFriends()}>
          ENTRAR A LA SALA
        </button>
      ) : (
        <>
          <div className="opts">
            Bots{" "}
            <select value={bots} onChange={(e) => setBots(Number(e.target.value))}>
              {[1, 2, 3, 4, 5, 6, 7].map((b) => (
                <option key={b} value={b}>
                  {b}
                </option>
              ))}
            </select>
          </div>
          <button type="button" className="btn" onClick={() => void playSolo()}>
            JUGAR SOLO vs bots
          </button>
          <button type="button" className="btn ghost" onClick={() => void enterFriends()}>
            CREAR SALA PARA AMIGOS
          </button>
        </>
      )}
    </>
  );
}

function Lobby({ S }: { S: GameState }) {
  const humans = S.players.filter((p) => p.profile === "humano").length;
  const [bots, setBots] = useState(S.bots > 0 ? S.bots : humans <= 1 ? 3 : 0);
  const [pace, setPace] = useState<Pace>(S.pace || "slow");
  const [attempt, setAttempt] = useState(S.attemptSeconds || 10);
  const [turns, setTurns] = useState<TurnMode>(S.turnMode || "simultaneous");
  const [err, setErr] = useState("");
  const total = humans + bots;
  const canStart = total >= (S.minPlayers ?? 2) && total <= (S.maxPlayers ?? 8);
  const maxBots = Math.max(0, (S.maxPlayers ?? 8) - humans);

  useEffect(() => {
    if ((S.bots ?? 0) > 0) setBots(S.bots);
  }, [S.bots]);

  const apply = async (next?: { bots?: number; pace?: Pace; attempt?: number; turns?: TurnMode }) => {
    const b = next?.bots ?? bots;
    const p = next?.pace ?? pace;
    const a = next?.attempt ?? attempt;
    const t = next?.turns ?? turns;
    await configRoom({ bots: b, pace: p, attempt: a, turns: t });
  };

  const begin = async () => {
    setErr("");
    await apply();
    const r = await startMatch();
    if (r && !r.ok) setErr(r.error ?? "no se pudo empezar");
  };

  return (
    <>
      <h3>Sala {S.roomCode}</h3>
      <div className="big" style={{ fontSize: 40 }}>
        {total} / {S.maxPlayers ?? 8}
      </div>
      <div className="sub">
        Todos descifráis el <b>mismo código</b>.{" "}
        {humans <= 1
          ? "Estás solo: añade bots y pulsa Jugar solo, o copia el enlace y espera a un amigo."
          : S.isHost
            ? "Eres el anfitrión. Cuando estéis listos, empieza."
            : "Esperando a que el anfitrión empiece…"}
      </div>
      <div className="seats">
        {S.players.map((p) => (
          <span key={p.seat} className="seat" style={{ borderColor: COLORS[p.seat], color: COLORS[p.seat] }}>
            {p.name}
            {p.profile === "humano" && p.seat === S.humanSeat ? " · tú" : ""}
            {p.profile !== "humano" ? " · bot" : ""}
          </span>
        ))}
      </div>
      <CopyLink code={S.roomCode} />
      {S.isHost ? (
        <>
          <div className="opts">
            Bots{" "}
            <select
              value={bots}
              onChange={(e) => {
                const v = Number(e.target.value);
                setBots(v);
                void apply({ bots: v });
              }}
            >
              {Array.from({ length: maxBots + 1 }, (_, i) => (
                <option key={i} value={i}>
                  {i}
                </option>
              ))}
            </select>
            Ritmo{" "}
            <select
              value={pace}
              onChange={(e) => {
                const v = e.target.value as Pace;
                setPace(v);
                void apply({ pace: v });
              }}
            >
              <option value="slow">Tranquilo (mesa)</option>
              <option value="normal">Normal</option>
              <option value="fast">Frenético (bots perfectos)</option>
            </select>
            Reloj{" "}
            <select
              value={attempt}
              onChange={(e) => {
                const v = Number(e.target.value);
                setAttempt(v);
                void apply({ attempt: v });
              }}
            >
              {[10, 15, 20, 30].map((a) => (
                <option key={a}>{a}</option>
              ))}
            </select>{" "}
            s/intento Turnos{" "}
            <select
              value={turns}
              onChange={(e) => {
                const v = e.target.value as TurnMode;
                setTurns(v);
                void apply({ turns: v });
              }}
            >
              <option value="simultaneous">Todos a la vez</option>
              <option value="seat">Por orden de asiento</option>
              <option value="random">Orden aleatorio</option>
            </select>
          </div>
          {err ? <div className="sub" style={{ color: "var(--red)", marginTop: 8 }}>{err}</div> : null}
          <button type="button" className="btn" disabled={!canStart} onClick={() => void begin()}>
            {humans <= 1 ? (canStart ? "JUGAR SOLO vs bots" : "Elige al menos 1 bot") : canStart ? "EMPEZAR CON AMIGOS" : "Mínimo 2 jugadores"}
          </button>
        </>
      ) : null}
    </>
  );
}

export const Overlay = memo(function Overlay({ S }: { S: GameState }) {
  if (S.joined && S.phase === "playing") return null;
  return (
    <div className="overlay on">
      <div className="card">
        {!S.joined ? <Join S={S} /> : null}
        {S.joined && S.phase === "lobby" ? <Lobby S={S} /> : null}
        {S.joined && S.phase === "countdown" ? <Countdown S={S} /> : null}
        {S.joined && S.phase === "summary" ? <Summary S={S} /> : null}
        {S.joined && S.phase === "matchend" ? (
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
