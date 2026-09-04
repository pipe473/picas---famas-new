import { memo } from "react";
import { splitCode } from "@/lib/format";

export const Tiles = memo(function Tiles({ code, empty }: { code: string; empty?: boolean }) {
  const digits = splitCode(code);
  return (
    <div className="tiles">
      {digits.map((d, i) => (
        <div key={i} className={`tile${empty ? " empty" : ""}`}>
          {d}
        </div>
      ))}
    </div>
  );
});
