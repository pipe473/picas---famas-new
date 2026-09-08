import { memo, type CSSProperties } from "react";

/**
 * Resultado como puntos: Famas rellenas, Picas huecas. Con `reveal`, cada punto entra
 * escalonado (retardo por `--i`), que es el momento de tensión del juego.
 */
export const Dots = memo(function Dots({
  f,
  p,
  len,
  big,
  reveal,
}: {
  f: number;
  p: number;
  len: number;
  big?: boolean;
  reveal?: boolean;
}) {
  return (
    <span className={`dots${big ? " big" : ""}${reveal ? " reveal" : ""}`}>
      {Array.from({ length: len }, (_, i) => (
        <span
          key={i}
          className={`dot ${i < f ? "f" : i < f + p ? "p" : ""}`}
          style={reveal ? ({ ["--i" as string]: i } as CSSProperties) : undefined}
        />
      ))}
    </span>
  );
});
