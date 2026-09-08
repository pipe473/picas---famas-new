"use client";

import { memo, useEffect, useState } from "react";
import { BackspaceIcon } from "@/components/Icons";

const ROWS = ["1", "2", "3", "4", "5", "6", "7", "8", "9"];

/**
 * Teclado 3×4 tipo teléfono. `pressed` replica el hundido de la tecla cuando el dígito
 * llega por teclado físico, así el feedback es el mismo con pulgar o con teclas.
 */
export const Keypad = memo(function Keypad({
  digits,
  len,
  enabled,
  onDigit,
  onBackspace,
  onClear,
}: {
  digits: string;
  len: number;
  enabled: boolean;
  onDigit: (d: string) => void;
  onBackspace: () => void;
  onClear: () => void;
}) {
  const [pressed, setPressed] = useState<string | null>(null);

  useEffect(() => {
    if (!enabled) return;
    const down = (e: KeyboardEvent) => {
      if (/^[0-9]$/.test(e.key)) setPressed(e.key);
      else if (e.key === "Backspace") setPressed("back");
    };
    const up = () => setPressed(null);
    document.addEventListener("keydown", down);
    document.addEventListener("keyup", up);
    return () => {
      document.removeEventListener("keydown", down);
      document.removeEventListener("keyup", up);
    };
  }, [enabled]);

  const full = digits.length >= len;
  const key = (v: string) => (
    <button
      key={v}
      type="button"
      className={`key${pressed === v ? " pressed" : ""}`}
      disabled={!enabled || digits.includes(v) || full}
      onClick={() => onDigit(v)}
      aria-label={`Dígito ${v}`}
    >
      {v}
    </button>
  );

  return (
    <div className="keypad" role="group" aria-label="Teclado numérico">
      {ROWS.map(key)}
      <button
        type="button"
        className={`key fn${pressed === "back" ? " pressed" : ""}`}
        onClick={onBackspace}
        disabled={!digits}
        aria-label="Borrar último dígito"
      >
        <BackspaceIcon />
      </button>
      {key("0")}
      <button type="button" className="key fn" onClick={onClear} disabled={!digits} aria-label="Vaciar intento">
        Limpiar
      </button>
    </div>
  );
});
