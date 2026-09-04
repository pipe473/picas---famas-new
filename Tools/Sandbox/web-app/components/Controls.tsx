"use client";

import { memo, useCallback, useEffect, useState } from "react";
import { Dots } from "@/components/Dots";
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
  const spectator = me < 0;
  const p = me >= 0 ? S.players[me] : null;
  const myTurn = S.turnMode === "simultaneous" || S.turnPlayer === me;
  const playing = S.phase === "playing" && !!p && !p.solved && myTurn;
  const waiting = S.phase === "playing" && S.turnMode !== "simultaneous" && !myTurn;

  useEffect(() => {
    if (S.phase === "playing") {
      setDigits("");
      setMode("plain");
    }
  }, [S.phase, S.round]);

  useEffect(() => {
    if (mode === "encrypt" && p && !p.encrypt) setMode("plain");
    if (mode === "decoy" && p && !p.decoy) setMode("plain");
  }, [mode, p]);

  const pushDigit = useCallback(
    (v: string) => {
      setDigits((d) => (d.length < S.len && !d.includes(v) ? d + v : d));
    },
    [S.len],
  );

  const backspace = useCallback(() => setDigits((d) => d.slice(0, -1)), []);

  const send = useCallback(async () => {
    if (digits.length !== S.len) return;
    const r = await sendGuess(digits, mode, fakeF, fakeP);
    if (r && !r.ok) {
      onToast({ text: r.error ?? "error", cls: "bad" });
      sfx.bad();
      return;
    }
    setDigits("");
    setMode("plain");
  }, [digits, S.len, mode, fakeF, fakeP, onToast]);

  useEffect(() => {
    if (spectator) return;
    const onKey = (e: KeyboardEvent) => {
      if (/^[0-9]$/.test(e.key)) pushDigit(e.key);
      else if (e.key === "Backspace") backspace();
      else if (e.key === "Enter") {
        if (S.phase === "playing") void send();
      } else if (e.key.toLowerCase() === "e" && p?.encrypt) setMode((m) => (m === "encrypt" ? "plain" : "encrypt"));
      else if (e.key.toLowerCase() === "s" && p?.decoy) setMode((m) => (m === "decoy" ? "plain" : "decoy"));
    };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [spectator, pushDigit, backspace, send, S.phase, p]);

  const sendLabel = waiting
    ? `TURNO DE ${(S.players[S.turnPlayer] ?? { name: "…" }).name.toUpperCase()}`
    : mode === "plain"
      ? "ENVIAR"
      : mode === "encrypt"
        ? "ENVIAR ENCRIPTADO 🔒"
        : "ENVIAR SEÑUELO 🎭";

  return (
    <section className="panel" id="right">
      {!spectator && p ? (
        <div>
          <h2>Tu intento</h2>
          <div className="display">
            {Array.from({ length: S.len }, (_, i) => (
              <div key={i} className={`tile ${i < digits.length ? "" : "empty"}`}>
                {digits[i] ?? "·"}
              </div>
            ))}
          </div>
          <div className="keypad">
            {["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"].map((v) => (
              <button
                key={v}
                type="button"
                className="key"
                disabled={!playing || digits.includes(v) || digits.length >= S.len}
                onClick={() => pushDigit(v)}
              >
                {v}
              </button>
            ))}
            <button type="button" className="key wide" onClick={backspace}>
              ⌫
            </button>
          </div>
          <div className="modes">
            <button type="button" className={`mode${mode === "plain" ? " on" : ""}`} onClick={() => setMode("plain")}>
              Normal
            </button>
            <button
              type="button"
              className={`mode${mode === "encrypt" ? " on violet" : ""}`}
              disabled={!p.encrypt}
              onClick={() => setMode("encrypt")}
            >
              🔒 Encriptar
            </button>
            <button
              type="button"
              className={`mode${mode === "decoy" ? " on red" : ""}`}
              disabled={!p.decoy}
              onClick={() => setMode("decoy")}
            >
              🎭 Señuelo
            </button>
          </div>
          {mode === "decoy" ? (
            <div className="fake">
              Fingir{" "}
              <select value={fakeF} onChange={(e) => setFakeF(Number(e.target.value))}>
                {Array.from({ length: 5 }, (_, i) => (
                  <option key={i}>{i}</option>
                ))}
              </select>{" "}
              Famas{" "}
              <select value={fakeP} onChange={(e) => setFakeP(Number(e.target.value))}>
                {Array.from({ length: 5 }, (_, i) => (
                  <option key={i}>{i}</option>
                ))}
              </select>{" "}
              Picas
            </div>
          ) : null}
          <button type="button" className="send" disabled={!playing || digits.length !== S.len} onClick={() => void send()}>
            {sendLabel}
          </button>
        </div>
      ) : null}

      {!spectator ? (
        <>
          <h2 style={{ marginTop: 14 }}>
            Tus resultados reales <span style={{ color: "var(--violet)" }}>· privado</span>
          </h2>
          <div className="priv">
            {S.private.length === 0 ? (
              <div className="sub">Aquí verás la verdad de tus intentos, aunque farolees.</div>
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

      <h2 style={{ marginTop: 8 }}>Eventos</h2>
      <div className="events">
        {[...S.events].reverse().map((e) => {
          const hot = /ACIERTA|RELAMPAGO|ALERTA|engano/i.test(e.text);
          const bad = /PILLADO|rechazado|sin razon|INACTIVO/i.test(e.text);
          return (
            <div key={e.id} className={`ev ${hot ? "hot" : ""} ${bad ? "bad" : ""}`}>
              {e.text}
            </div>
          );
        })}
      </div>
    </section>
  );
});
