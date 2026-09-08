"use client";

import { memo, type ReactNode } from "react";
import { AlertIcon, LockIcon, MaskIcon, SendIcon, TrophyIcon } from "@/components/Icons";
import type { ToastItem } from "@/lib/types";

const KIND: Record<string, { label: string; icon: ReactNode }> = {
  bad: { label: "Alerta", icon: <AlertIcon /> },
  gold: { label: "Relámpago", icon: <TrophyIcon /> },
  hot: { label: "Jugada", icon: <SendIcon /> },
  trick: { label: "Engaño", icon: <MaskIcon /> },
  info: { label: "Aviso", icon: <LockIcon /> },
};

/** Banners de evento: entran desde la derecha, cada tipo con su acento y su icono. */
export const Toasts = memo(function Toasts({ items }: { items: ToastItem[] }) {
  return (
    <div className="toasts" role="status" aria-live="polite">
      {items.map((t) => {
        const k = KIND[t.cls ?? "hot"] ?? KIND.hot;
        return (
          <div key={t.id} className={`tst ${t.cls ?? "hot"}${t.leaving ? " out" : ""}`}>
            <span className="tst-ic" aria-hidden="true">
              {k.icon}
            </span>
            <span className="tst-txt">
              <span className="tst-kind">{k.label}</span>
              {t.text}
            </span>
          </div>
        );
      })}
    </div>
  );
});
