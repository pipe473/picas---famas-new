"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { AlertBar } from "@/components/AlertBar";
import { Board } from "@/components/Board";
import { Controls } from "@/components/Controls";
import { Header } from "@/components/Header";
import { Overlay } from "@/components/Overlay";
import { PlayerList } from "@/components/PlayerList";
import { Toasts } from "@/components/Toasts";
import { fetchState, getToken, startMatch, streamUrl, suspect } from "@/lib/api";
import { sfx } from "@/lib/audio";
import { syncClock } from "@/lib/clock";
import type { GameState, ToastItem } from "@/lib/types";

export default function Game() {
  const [S, setS] = useState<GameState | null>(null);
  const [toasts, setToasts] = useState<ToastItem[]>([]);
  const knownEv = useRef(new Set<number>());
  const first = useRef(true);
  const toastId = useRef(0);
  const token = typeof window !== "undefined" ? getToken() : "";
  const [tok, setTok] = useState(token);

  const pushToast = useCallback((t: Omit<ToastItem, "id">) => {
    const id = ++toastId.current;
    setToasts((xs) => [...xs, { id, ...t }]);
    setTimeout(() => setToasts((xs) => xs.filter((x) => x.id !== id)), 2600);
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
          } else if (/RELAMPAGO/.test(e.text)) pushToast({ text: e.text, cls: "gold" });
          else if (/PILLADO|rechazado/.test(e.text)) {
            sfx.bad();
            pushToast({ text: e.text, cls: "bad" });
          } else if (/engano|FOTO-FINISH|intuicion/.test(e.text)) pushToast({ text: e.text });
        }
      } else {
        for (const e of st.events) knownEv.current.add(e.id);
        first.current = false;
      }
    },
    [pushToast],
  );

  useEffect(() => {
    const id = window.setInterval(() => setTok(getToken()), 400);
    return () => window.clearInterval(id);
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
        <div className="card">
          <h3>Conectando</h3>
          <div className="sub">Esperando al sandbox en /api/state…</div>
        </div>
      </div>
    );
  }

  return (
    <>
      <Header S={S} />
      <AlertBar S={S} />
      <main className={S.humanSeat < 0 || !S.joined ? "spectator" : ""}>
        <PlayerList S={S} />
        <Board S={S} onSuspect={(seq) => void suspect(seq)} />
        <Controls S={S} onToast={pushToast} />
      </main>
      <Toasts items={toasts} />
      <Overlay S={S} />
    </>
  );
}
