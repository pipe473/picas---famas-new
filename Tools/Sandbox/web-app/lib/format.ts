export function fmt(t: number): string {
  const s = Math.max(0, t);
  return `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, "0")}`;
}

export function splitCode(code: string): string[] {
  return code.split(" ").filter(Boolean);
}
