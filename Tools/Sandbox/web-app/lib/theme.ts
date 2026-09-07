export type Theme = "day" | "night";

export const THEME_KEY = "pf-theme";

export function isTheme(value: unknown): value is Theme {
  return value === "day" || value === "night";
}

export function readStoredTheme(): Theme | null {
  if (typeof window === "undefined") return null;
  try {
    const raw = localStorage.getItem(THEME_KEY);
    return isTheme(raw) ? raw : null;
  } catch {
    return null;
  }
}

export function applyTheme(theme: Theme): void {
  document.documentElement.setAttribute("data-theme", theme);
  try {
    localStorage.setItem(THEME_KEY, theme);
  } catch {
    /* ignore quota / private mode */
  }
}

export function resolveInitialTheme(): Theme {
  return readStoredTheme() ?? "night";
}
