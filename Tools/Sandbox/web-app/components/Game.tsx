"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { AlertBar } from "@/components/AlertBar";
import { Board } from "@/components/Board";
import { Controls } from "@/components/Controls";
import { Header } from "@/components/Header";
import { Overlay } from "@/components/Overlay";
import { PlayerList } from "@/components/PlayerList";
import { RankingDialog } from "@/components/Ranking";
import { SoundDock } from "@/components/SoundDock";
import { Toasts } from "@/components/Toasts";
import { fetchState, getToken, startMatch, streamUrl, suspect } from "@/lib/api";
import { sfx } from "@/lib/audio";
import { syncClock } from "@/lib/clock";
import type { GameState, ToastItem } from "@/lib/types";

export default function Game() {
  const [S, setS] = useState<GameState | null>(null);
  const [toasts, setToasts] = useState<ToastItem[]>([]);
  const [rankOpen, setRankOpen] = useState(false);
  const openRank = useCallback(() => setRankOpen(true), []);
  const closeRank = useCallback(() => setRankOpen(false), []);
  const knownEv = useRef(new Set<number>());
  const first = useRef(true);
  const toastId = useRef(0);
  const token = typeof window !== "undefined" ? getToken() : "";
  const [tok, setTok] = useState(token);

  const pushToast = useCallback((t: Omit<ToastItem, "id">) => {
    const id = ++toastId.current;
    setToasts((xs) => [...xs, { id, ...t }]);
    // Se marca como saliente antes de quitarlo para que la animación de salida llegue a verse.
    setTimeout(() => setToasts((xs) => xs.map((x) => (x.id === id ? { ...x, leaving: true } : x))), 2400);
    setTimeout(() => setToasts((xs) => xs.filter((x) => x.id !== id)), 2650);
  }, []);

  const ingest = useCallback(
    (st: GameState) => {
      syncClock(st.now);
      setS(st);
      if (!first.current) {
        for (const e of st.events) {
          if (knownEv.current.has(e.id)) continue;
          knownEv.current.add(e.id);
          if (/ALERTA/.test(e.text)) {
            sfx.alert();
            pushToast({ text: e.text, cls: "bad" });
          } else if (/RELAMPAGO|ACIERTA/.test(e.text)) pushToast({ text: e.text, cls: "gold" });
          else if (/PILLADO|rechazado/.test(e.text)) {
            sfx.bad();
            pushToast({ text: e.text, cls: "bad" });
          } else if (/engano|senuelo|SEÑUELO/i.test(e.text)) pushToast({ text: e.text, cls: "trick" });
          else if (/encript/i.test(e.text)) pushToast({ text: e.text, cls: "info" });
          else if (/FOTO-FINISH|intuicion/.test(e.text)) pushToast({ text: e.text, cls: "hot" });
        }
      } else {
        for (const e of st.events) knownEv.current.add(e.id);
        first.current = false;
      }
    },
    [pushToast],
  );

  useEffect(() => {
    const sync = () => setTok(getToken());
    window.addEventListener("pf-token", sync);
    const id = window.setInterval(sync, 400);
    return () => {
      window.removeEventListener("pf-token", sync);
      window.clearInterval(id);
    };
  }, []);

  useEffect(() => {
    let stop = false;
    let es: EventSource | null = null;
    let poll = 0;

    const startPoll = () => {
      const tick = async () => {
        const st = await fetchState();
        if (!stop && st) ingest(st);
        if (!stop) poll = window.setTimeout(tick, 150);
      };
      void tick();
    };

    try {
      es = new EventSource(streamUrl());
      es.onmessage = (ev) => {
        try {
          const st = JSON.parse(ev.data) as GameState;
          if (!stop && st) ingest(st);
        } catch {
          /* ignore */
        }
      };
      es.onerror = () => {
        es?.close();
        es = null;
        if (!stop) startPoll();
      };
    } catch {
      startPoll();
    }

    return () => {
      stop = true;
      es?.close();
      if (poll) window.clearTimeout(poll);
    };
  }, [ingest, tok]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Enter" && S?.phase === "matchend" && S.isHost) void startMatch();
    };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [S]);

  if (!S) {
    return (
      <div className="overlay on">
        <div className="card" role="status" aria-live="polite">
          <h3>Conectando</h3>
          <div className="loading" aria-hidden="true">
            <i />
            <i />
            <i />
          </div>
          <p className="lead">Esperando al sandbox en /api/state…</p>
        </div>
      </div>
    );
  }

  return (
    <>
      <Header S={S} onRanking={openRank} />
      <AlertBar S={S} />
      <main className={S.humanSeat < 0 || !S.joined ? "spectator" : ""}>
        <PlayerList S={S} />
        <Board S={S} onSuspect={(seq) => void suspect(seq)} />
        <Controls S={S} onToast={pushToast} />
      </main>
      <Toasts items={toasts} />
      <Overlay S={S} />
      {rankOpen ? <RankingDialog S={S} onClose={closeRank} /> : null}
      <SoundDock />
    </>
  );
}
