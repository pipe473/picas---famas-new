"use client";

import { memo, type CSSProperties } from "react";
import { LockIcon, MaskIcon } from "@/components/Icons";
import type { GuessMode } from "@/lib/types";

const ORDER: GuessMode[] = ["plain", "encrypt", "decoy"];

/**
 * Selector segmentado: la pastilla (`.thumb`) se desliza con `--i` y hereda el acento
 * del modo activo via `data-mode`; la etiqueta activa brilla con ese mismo acento.
 */
export const ModeTabs = memo(function ModeTabs({
  mode,
  canEncrypt,
  canDecoy,
  onChange,
}: {
  mode: GuessMode;
  canEncrypt: boolean;
  canDecoy: boolean;
  onChange: (m: GuessMode) => void;
}) {
  const i = ORDER.indexOf(mode);
  return (
    <div className="modes" role="radiogroup" aria-label="Modo de envío" data-mode={mode} style={{ ["--i" as string]: i } as CSSProperties}>
      <span className="thumb" aria-hidden="true" />
      <button type="button" role="radio" aria-checked={mode === "plain"} className={`mode${mode === "plain" ? " on" : ""}`} onClick={() => onChange("plain")}>
        Normal
      </button>
      <button
        type="button"
        role="radio"
        aria-checked={mode === "encrypt"}
        className={`mode${mode === "encrypt" ? " on" : ""}`}
        disabled={!canEncrypt}
        onClick={() => onChange("encrypt")}
        title="Tecla E · el resultado queda oculto un tiempo"
      >
        <LockIcon /> Encriptar
      </button>
      <button
        type="button"
        role="radio"
        aria-checked={mode === "decoy"}
        className={`mode${mode === "decoy" ? " on" : ""}`}
        disabled={!canDecoy}
        onClick={() => onChange("decoy")}
        title="Tecla S · finge un resultado falso"
      >
        <MaskIcon /> Señuelo
      </button>
    </div>
  );
});
