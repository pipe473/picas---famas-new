"use client";

import { memo } from "react";
import type { ToastItem } from "@/lib/types";

export const Toasts = memo(function Toasts({ items }: { items: ToastItem[] }) {
  return (
    <div className="toasts" role="status" aria-live="polite">
      {items.map((t) => (
        <div key={t.id} className={`tst ${t.cls ?? ""}${t.leaving ? " out" : ""}`}>
          {t.text}
        </div>
      ))}
    </div>
  );
});
