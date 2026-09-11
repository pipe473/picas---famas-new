"use client";

import { memo } from "react";
import { AlertIcon } from "@/components/Icons";
import { useNow } from "@/lib/clock";
import type { GameState } from "@/lib/types";

export const AlertBar = memo(function AlertBar({ S }: { S: GameState }) {
  const n = useNow();
  const on = S.alertPlayer >= 0 && S.phase === "playing";
  const who = on ? S.players[S.alertPlayer]?.name.toUpperCase() : "";
  // Con tiempo libre la alerta es solo informativa: no hay Muerte Sudada ni cuenta atrás.
  const timed = S.suddenDeathEnd > 0;
  return (
    <div className={`alert-bar${on ? " on" : ""}`} role="alert" aria-live="assertive">
      <span>
        <AlertIcon />
        ALERTA · <span>{who}</span> TIENE <span>{S.len - 1}</span> FAMAS{timed ? " · MUERTE SUDADA" : " · ALGUIEN ESTÁ A UNA"}
      </span>
      {timed ? <span className="sd">{Math.max(0, S.suddenDeathEnd - n).toFixed(1)}</span> : null}
    </div>
  );
});
