"use client";

import { memo } from "react";
import { useNow } from "@/lib/clock";
import type { GameState } from "@/lib/types";

export const AlertBar = memo(function AlertBar({ S }: { S: GameState }) {
  const n = useNow();
  const on = S.alertPlayer >= 0 && S.phase === "playing";
  const who = on ? S.players[S.alertPlayer]?.name.toUpperCase() : "";
  return (
    <div className={`alert-bar${on ? " on" : ""}`}>
      <span>
        ⚠ ALERTA · <span>{who}</span> TIENE <span>{S.len - 1}</span> FAMAS · MUERTE SUDADA
      </span>
      <span className="sd">{Math.max(0, S.suddenDeathEnd - n).toFixed(1)}</span>
    </div>
  );
});
