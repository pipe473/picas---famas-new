"use client";

import { memo } from "react";
import { LockIcon, MaskIcon, SendIcon } from "@/components/Icons";
import type { GuessMode } from "@/lib/types";

/**
 * Pieza central del panel. Tres estados visibles: apagado (faltan dígitos o no es tu turno),
 * `ready` (código completo: respira y brilla) y `busy` (esperando al servidor).
 */
export const SendButton = memo(function SendButton({
  mode,
  ready,
  busy,
  disabled,
  waitingFor,
  onClick,
}: {
  mode: GuessMode;
  ready: boolean;
  busy: boolean;
  disabled: boolean;
  waitingFor: string | null;
  onClick: () => void;
}) {
  const label = waitingFor ? (
    <>Turno de {waitingFor}</>
  ) : mode === "plain" ? (
    <>
      Enviar <SendIcon />
    </>
  ) : mode === "encrypt" ? (
    <>
      <LockIcon /> Enviar encriptado
    </>
  ) : (
    <>
      <MaskIcon /> Enviar señuelo
    </>
  );
  return (
    <button
      type="button"
      className={`send${ready && !disabled ? " ready" : ""}${busy ? " busy" : ""}`}
      disabled={disabled}
      aria-busy={busy}
      onClick={onClick}
    >
      {label}
    </button>
  );
});
