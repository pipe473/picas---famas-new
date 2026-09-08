"use client";

import { memo, useEffect, useRef, useState, type CSSProperties } from "react";
import { sfx } from "@/lib/audio";
import { abortMatch, configRoom, createRoom, goHome, joinRoom, roomCodeFromUrl, setToken, startMatch, startSolo } from "@/lib/api";
import { useNow } from "@/lib/clock";
import { ShareInvite } from "@/components/ShareInvite";
import { TrophyIcon } from "@/components/Icons";
import { COLORS, type GameState, type Pace, type TurnMode } from "@/lib/types";

const tone = (seat: number): CSSProperties => ({ ["--tone" as string]: COLORS[seat] });

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
        Ronda <span className="num">{S.round}</span> de <span className="num">{S.rounds}</span>
      </h3>
      {/* La key reinicia la animación de entrada con cada número. */}
      <div className="count" key={k} aria-live="assertive">
        {k}
      </div>
      <p className="lead">
        Código de <b>{S.len} dígitos</b> · todos a por el mismo enigma
      </p>
      <BackActions S={S} allowAbort />
    </>
  );
});

const Summary = memo(function Summary({ S }: { S: GameState }) {
  const n = useNow();
  const w = S.winner >= 0 ? S.players[S.winner] : null;
  const rows = [...S.players]
    .sort((a, b) => b.roundScore - a.roundScore)
    .map((p) => (
      <tr key={p.seat} style={tone(p.seat)}>
        <td className="who">{p.name}</td>
        <td className="n">
          {p.roundScore >= 0 ? "+" : ""}
          {p.roundScore}
        </td>
        <td className="n total">{p.matchScore} total</td>
      </tr>
    ));
  return (
    <>
      <h3>
        Ronda <span className="num">{S.round}</span> · el código era
      </h3>
      <div className="code">
        {(S.secret ?? "").split(" ").map((d, i) => (
          <div className="tile" key={i}>
            {d}
          </div>
        ))}
      </div>
      <div className="title small tone" style={w ? tone(w.seat) : { ["--tone" as string]: "var(--muted)" }}>
        {w ? (
          <>
            <TrophyIcon />
            {w.name}
          </>
        ) : (
          "Nadie lo descifró"
        )}
      </div>
      <table>
        <tbody>{rows}</tbody>
      </table>
      <p className="sub next-in">Siguiente ronda en {Math.ceil(Math.max(0, S.phaseEnd - n))} s</p>
      <BackActions S={S} allowAbort />
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
            <div className={`pod p${[2, 1, 3][i]}`} key={p.seat} style={tone(p.seat)}>
              <div className="who" style={{ color: "var(--tone)" }}>
                {p.name}
              </div>
              <small>{p.matchScore} pts</small>
              <span className="rank">{[2, 1, 3][i]}</span>
            </div>
          ) : null,
        )}
      </div>
      {order.length > 3 ? (
        <table>
          <tbody>
            {order.slice(3).map((p) => (
              <tr key={p.seat} style={tone(p.seat)}>
                <td className="who">{p.name}</td>
                <td className="n">{p.matchScore}</td>
              </tr>
            ))}
          </tbody>
        </table>
      ) : null}
      {S.isHost ? (
        <button type="button" className="btn" onClick={onAgain}>
          Una más
        </button>
      ) : (
        <p className="sub next-in">Esperando a que el anfitrión abra otra partida…</p>
      )}
      <BackActions S={S} />
    </>
  );
});

function BackActions({ S, allowAbort }: { S: GameState; allowAbort?: boolean }) {
  return (
    <div className="back-row">
      {allowAbort && S.isHost ? (
        <button type="button" className="btn ghost" onClick={() => void abortMatch()}>
          Terminar partida
        </button>
      ) : null}
      <button type="button" className="btn ghost" onClick={() => void goHome()}>
        Volver al menú
      </button>
    </div>
  );
}

const BOT_CHOICES = [1, 2, 3, 4, 5, 6, 7];

function Join({ S }: { S: GameState }) {
  const invited = !!roomCodeFromUrl();
  const busy = S.phase !== "lobby" && S.phase !== "none";
  const [name, setName] = useState("");
  const [bots, setBots] = useState(3);
  const [err, setErr] = useState("");

  const finish = (r: { ok?: boolean; token?: string; error?: string } | null) => {
    if (!r?.ok || !r.token) {
      setErr(r?.error ?? "No se pudo entrar. Inténtalo de nuevo.");
      return false;
    }
    setToken(r.token);
    return true;
  };

  const enterFriends = async () => {
    setErr("");
    setToken("");
    const code = roomCodeFromUrl();
    const r = code
      ? await joinRoom(name.trim() || "Jugador", code)
      : await createRoom(name.trim() || "Jugador");
    finish(r);
  };

  const playSolo = async () => {
    setErr("");
    setToken("");
    const r = await startSolo(name.trim() || "Jugador", bots);
    finish(r);
  };

  const lead = invited
    ? "Entras a la sala de un amigo. Todos atacáis el mismo código."
    : busy
      ? "Hay una partida en marcha en esta mesa. Empieza la tuya: contra bots o en una sala para amigos."
      : "Todos descifráis el mismo código en tiempo real. Juega contra bots o crea una sala y comparte el enlace.";

  return (
    <>
      <h3>{invited ? "Te han invitado" : "Deducción en tiempo real"}</h3>
      {invited ? (
        <div className="title code-title">{S.roomCode}</div>
      ) : (
        <div className="title">
          Picas <em>y</em> Famas
        </div>
      )}
      <p className="lead">{lead}</p>

      <div className="form">
        <label className="field">
          <span>Tu nombre</span>
          <input
            className="namein"
            maxLength={16}
            placeholder="Jugador"
            autoComplete="nickname"
            value={name}
            onChange={(e) => setName(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter") void (invited ? enterFriends() : playSolo());
            }}
          />
        </label>
        {!invited ? (
          <div className="field">
            <span id="bots-lbl">Rivales (bots)</span>
            <div className="seg" role="radiogroup" aria-labelledby="bots-lbl">
              {BOT_CHOICES.map((b) => (
                <button
                  key={b}
                  type="button"
                  role="radio"
                  aria-checked={bots === b}
                  className={bots === b ? "on" : ""}
                  onClick={() => setBots(b)}
                >
                  {b}
                </button>
              ))}
            </div>
          </div>
        ) : null}
      </div>

      {err ? (
        <div className="form-err" role="alert">
          {err}
        </div>
      ) : null}

      <div className="cta">
        {invited ? (
          <button type="button" className="btn" onClick={() => void enterFriends()}>
            Entrar a la sala
          </button>
        ) : (
          <>
            <button type="button" className="btn" onClick={() => void playSolo()}>
              Jugar contra {bots} {bots === 1 ? "bot" : "bots"}
            </button>
            <button type="button" className="btn ghost" onClick={() => void enterFriends()}>
              Crear sala para amigos
            </button>
          </>
        )}
      </div>
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
    if (r && !r.ok) setErr(r.error ?? "No se pudo empezar.");
  };

  const startLabel =
    humans <= 1
      ? canStart
        ? "Jugar contra bots"
        : "Elige al menos 1 bot"
      : canStart
        ? "Empezar con amigos"
        : "Mínimo 2 jugadores";

  return (
    <>
      <h3>Sala {S.roomCode}</h3>
      <div className="big" aria-label={`${total} de ${S.maxPlayers ?? 8} jugadores`}>
        {total}
        <small>/ {S.maxPlayers ?? 8}</small>
      </div>
      <p className="lead">
        Todos descifráis el <b>mismo código</b>.{" "}
        {humans <= 1
          ? "Estás solo: añade bots o invita a un amigo y espera a que entre."
          : S.isHost
            ? "Eres el anfitrión. Cuando estéis listos, empieza."
            : "Esperando a que el anfitrión empiece…"}
      </p>
      <div className="seats">
        {S.players.map((p) => (
          <span key={p.seat} className="seat" style={{ borderColor: COLORS[p.seat], color: COLORS[p.seat] }}>
            {p.name}
            {p.profile === "humano" && p.seat === S.humanSeat ? " · tú" : ""}
            {p.profile !== "humano" ? " · bot" : ""}
          </span>
        ))}
      </div>
      <ShareInvite code={S.roomCode} />
      {S.isHost ? (
        <>
          <div className="opts">
            <label>
              Bots
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
            </label>
            <label>
              Ritmo
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
            </label>
            <label>
              Reloj
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
              </select>
              s/intento
            </label>
            <label>
              Turnos
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
            </label>
          </div>
          {err ? (
            <div className="form-err" role="alert">
              {err}
            </div>
          ) : null}
          <div className="cta">
            <button type="button" className="btn" disabled={!canStart} onClick={() => void begin()}>
              {startLabel}
            </button>
          </div>
        </>
      ) : null}
      <BackActions S={S} />
    </>
  );
}

export const Overlay = memo(function Overlay({ S }: { S: GameState }) {
  if (S.joined && S.phase === "playing") return null;
  return (
    <div className="overlay on">
      <div className="card" role="dialog" aria-modal="true">
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
