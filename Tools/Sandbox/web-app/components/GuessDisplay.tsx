"use client";

import { memo } from "react";

/** Feedback del display tras enviar: destello verde si entra, sacudida si el servidor lo rechaza. */
export type Pulse = "flash" | "shake" | null;

/**
 * Slots del intento. Cada slot rellenado monta con la clase `filled`, cuya animación
 * `slot-bounce` solo se dispara al aparecer: un dígito nuevo rebota, los anteriores no.
 */
export const GuessDisplay = memo(function GuessDisplay({
  digits,
  len,
  active,
  pulse,
}: {
  digits: string;
  len: number;
  active: boolean;
  pulse: Pulse;
}) {
  return (
    <div className={`display${pulse ? ` ${pulse}` : ""}`} aria-label={`Intento: ${digits || "vacío"}`}>
      {Array.from({ length: len }, (_, i) => {
        const filled = i < digits.length;
        const next = active && i === digits.length;
        return (
          <div key={filled ? `${i}-${digits[i]}` : i} className={`tile ${filled ? "filled" : "empty"}${next ? " next" : ""}`}>
            {digits[i] ?? ""}
          </div>
        );
      })}
    </div>
  );
});
