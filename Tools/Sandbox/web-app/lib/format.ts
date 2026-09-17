export function fmt(t: number): string {
  const s = Math.max(0, t);
  return `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, "0")}`;
}

export function splitCode(code: string): string[] {
  return code.split(" ").filter(Boolean);
}

/** Fecha/hora local legible para citas (p. ej. "18/09 a las 21:00"). */
export function formatWhen(ts: number): string {
  if (!ts) return "";
  const d = new Date(ts * 1000);
  const dd = String(d.getDate()).padStart(2, "0");
  const mm = String(d.getMonth() + 1).padStart(2, "0");
  const hh = String(d.getHours()).padStart(2, "0");
  const mi = String(d.getMinutes()).padStart(2, "0");
  return `${dd}/${mm} a las ${hh}:${mi}`;
}

/** Valor inicial para `<input type="datetime-local">`: dentro de una hora, minutos redondeados a 5. */
export function defaultScheduleLocal(): string {
  const d = new Date(Date.now() + 60 * 60 * 1000);
  d.setMinutes(Math.ceil(d.getMinutes() / 5) * 5, 0, 0);
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

export function localInputToUnix(value: string): number {
  const t = Date.parse(value);
  return Number.isFinite(t) ? Math.floor(t / 1000) : 0;
}
