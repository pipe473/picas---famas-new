"use client";

import { memo } from "react";
import type { ToastItem } from "@/lib/types";

export const Toasts = memo(function Toasts({ items }: { items: ToastItem[] }) {
  return (
    <div className="toasts">
      {items.map((t) => (
        <div key={t.id} className={`tst ${t.cls ?? ""}`}>
          {t.text}
        </div>
      ))}
    </div>
  );
});
