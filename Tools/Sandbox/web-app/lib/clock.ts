"use client";

import { useSyncExternalStore } from "react";

// Reloj de servidor interpolado. Solo los componentes que llaman useNow() se
// re-renderizan a ~60 fps; el tablero y la sala se quedan quietos entre polls.
//
// React 19 exige que getSnapshot() sea estable entre notificaciones. Si
// devolvemos performance.now() crudo, dos lecturas seguidas no coinciden y
// salta "Maximum update depth exceeded" (error #185).

type Listener = () => void;
const listeners = new Set<Listener>();

let serverNow = 0;
let clientOrigin = 0;
let synced = false;
let raf = 0;
let snapshot = 0;

function interpolated(): number {
  if (!synced) return 0;
  return serverNow + (performance.now() / 1000 - clientOrigin);
}

export function syncClock(serverTime: number) {
  serverNow = serverTime;
  clientOrigin = performance.now() / 1000;
  synced = true;
  snapshot = serverTime;
}

export function now(): number {
  return snapshot;
}

function emit() {
  listeners.forEach((l) => l());
}

function loop() {
  raf = requestAnimationFrame(loop);
  const next = interpolated();
  if (next === snapshot) return;
  snapshot = next;
  emit();
}

function subscribe(onChange: Listener) {
  listeners.add(onChange);
  if (!raf) raf = requestAnimationFrame(loop);
  return () => {
    listeners.delete(onChange);
    if (listeners.size === 0 && raf) {
      cancelAnimationFrame(raf);
      raf = 0;
    }
  };
}

export function useNow(): number {
  return useSyncExternalStore(subscribe, now, () => 0);
}
