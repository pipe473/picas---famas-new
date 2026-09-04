import { memo } from "react";

export const Dots = memo(function Dots({ f, p, len, big }: { f: number; p: number; len: number; big?: boolean }) {
  return (
    <span className={`dots${big ? " big" : ""}`}>
      {Array.from({ length: len }, (_, i) => (
        <span key={i} className={`dot ${i < f ? "f" : i < f + p ? "p" : ""}`} />
      ))}
    </span>
  );
});
