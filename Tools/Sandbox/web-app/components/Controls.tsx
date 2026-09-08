"use client";

import { memo, useCallback, useEffect, useRef, useState } from "react";
import { Dots } from "@/components/Dots";
import { GuessDisplay, type Pulse } from "@/components/GuessDisplay";
import { Keypad } from "@/components/Keypad";
import { ModeTabs } from "@/components/ModeTabs";
import { SendButton } from "@/components/SendButton";
import { sfx } from "@/lib/audio";
import { sendGuess } from "@/lib/api";
import type { GameState, GuessMode, ToastItem } from "@/lib/types";

// ToastItem is declared in Game; keep a local shape to avoid a cycle.
type ToastFn = (t: Omit<ToastItem, "id">) => void;

export const Controls = memo(function Controls({
  S,
  onToast,
}: {
  S: GameState;
  onToast: ToastFn;
}) {
  const me = S.humanSeat;
  const [digits, setDigits] = useState("");
  const [mode, setMode] = useState<GuessMode>("plain");
  const [fakeF, setFakeF] = useState(2);
  const [fakeP, setFakeP] = useState(1);
  const [busy, setBusy] = useState(false);
  const [pulse, setPulse] = useState<Pulse>(null);
  const pulseTimer = useRef(0);
  const spectator = me < 0;
  const p = me >= 0 ? S.players[me] : null;
  const myTurn = S.turnMode === "simultaneous" || S.turnPlayer === me;
  const playing = S.phase === "playing" && !!p && !p.solved && myTurn;
  const waiting = S.phase === "playing" && S.turnMode !== "simultaneous" && !myTurn;
  const complete = digits.length === S.len;

  useEffect(() => {
    if (S.phase === "playing") {
      setDigits("");
      setMode("plain");
    }
  }, [S.phase, S.round]);

  const canEncrypt = !!p?.encrypt;
  const canDecoy = !!p?.decoy;

  useEffect(() => {
    if (mode === "encrypt" && !canEncrypt) setMode("plain");
    if (mode === "decoy" && !canDecoy) setMode("plain");
  }, [mode, canEncrypt, canDecoy]);

  useEffect(() => () => window.clearTimeout(pulseTimer.current), []);

  const fire = useCallback((kind: Exclude<Pulse, null>) => {
    window.clearTimeout(pulseTimer.current);
    setPulse(kind);
    pulseTimer.current = window.setTimeout(() => setPulse(null), 500);
  }, []);

  const pushDigit = useCallback(
    (v: string) => {
      setDigits((d) => {
        if (d.length >= S.len || d.includes(v)) return d;
        sfx.tick();
        return d + v;
      });
    },
    [S.len],
  );

  const backspace = useCallback(() => setDigits((d) => d.slice(0, -1)), []);
  const clear = useCallback(() => setDigits(""), []);

  const send = useCallback(async () => {
    if (!complete || busy) return;
    setBusy(true);
    const r = await sendGuess(digits, mode, fakeF, fakeP);
    setBusy(false);
    if (r && !r.ok) {
      onToast({ text: r.error ?? "error", cls: "bad" });
      sfx.bad();
      fire("shake");
      return;
    }
    fire("flash");
    setDigits("");
    setMode("plain");
  }, [complete, digits, mode, fakeF, fakeP, busy, onToast, fire]);

  useEffect(() => {
    if (spectator) return;
    const onKey = (e: KeyboardEvent) => {
      if (/^[0-9]$/.test(e.key)) {
        if (playing) pushDigit(e.key);
      } else if (e.key === "Backspace") backspace();
      else if (e.key === "Escape") clear();
      else if (e.key === "Enter") {
        if (S.phase === "playing") void send();
      } else if (e.key.toLowerCase() === "e" && p?.encrypt) setMode((m) => (m === "encrypt" ? "plain" : "encrypt"));
      else if (e.key.toLowerCase() === "s" && p?.decoy) setMode((m) => (m === "decoy" ? "plain" : "decoy"));
    };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [spectator, playing, pushDigit, backspace, clear, send, S.phase, p]);

  return (
    <section className="panel" id="right">
      {!spectator && p ? (
        <div className="attempt">
          <h2>Tu intento</h2>
          <GuessDisplay digits={digits} len={S.len} active={playing} pulse={pulse} />
          <Keypad digits={digits} len={S.len} enabled={playing} onDigit={pushDigit} onBackspace={backspace} onClear={clear} />
          <ModeTabs mode={mode} canEncrypt={canEncrypt} canDecoy={canDecoy} onChange={setMode} />
          {mode === "decoy" ? (
            <div className="fake">
              Fingir{" "}
              <select value={fakeF} onChange={(e) => setFakeF(Number(e.target.value))} aria-label="Famas fingidas">
                {Array.from({ length: 5 }, (_, i) => (
                  <option key={i}>{i}</option>
                ))}
              </select>{" "}
              Famas{" "}
              <select value={fakeP} onChange={(e) => setFakeP(Number(e.target.value))} aria-label="Picas fingidas">
                {Array.from({ length: 5 }, (_, i) => (
                  <option key={i}>{i}</option>
                ))}
              </select>{" "}
              Picas
            </div>
          ) : null}
          <SendButton
            mode={mode}
            ready={complete}
            busy={busy}
            disabled={!playing || !complete}
            waitingFor={waiting ? (S.players[S.turnPlayer] ?? { name: "…" }).name : null}
            onClick={() => void send()}
          />
        </div>
      ) : null}

      {!spectator ? (
        <>
          <h2 className="gap">
            Tus resultados reales <span className="priv-tag">· privado</span>
          </h2>
          <div className="priv">
            {S.private.length === 0 ? (
              <div className="empty tight">Aquí verás la verdad de tus intentos, aunque farolees.</div>
            ) : (
              [...S.private].reverse().map((r) => (
                <div className="pr" key={r.seq}>
                  <span>#{r.seq}</span>
                  <b>{r.guess.replace(/ /g, "")}</b>
                  <span>
                    <Dots f={r.f} p={r.p} len={S.len} />
                  </span>
                  <span>
                    {r.f}F {r.p}P
                  </span>
                </div>
              ))
            )}
          </div>
        </>
      ) : null}

      <h2 className="gap">Eventos</h2>
      <div className="events" aria-live="polite">
        {S.events.length === 0 ? (
          <div className="empty tight">Sin eventos todavía.</div>
        ) : (
          [...S.events].reverse().map((e) => {
            const hot = /ACIERTA|RELAMPAGO|ALERTA|engano/i.test(e.text);
            const bad = /PILLADO|rechazado|sin razon|INACTIVO/i.test(e.text);
            return (
              <div key={e.id} className={`ev ${hot ? "hot" : ""} ${bad ? "bad" : ""}`}>
                {e.text}
              </div>
            );
          })
        )}
      </div>
    </section>
  );
});
