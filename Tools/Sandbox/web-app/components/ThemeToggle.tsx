"use client";

import { memo, useEffect, useState } from "react";
import { applyTheme, resolveInitialTheme, type Theme } from "@/lib/theme";

export const ThemeToggle = memo(function ThemeToggle() {
  const [theme, setTheme] = useState<Theme>("night");

  useEffect(() => {
    const initial = resolveInitialTheme();
    setTheme(initial);
    applyTheme(initial);
  }, []);

  const toggle = () => {
    const next: Theme = theme === "night" ? "day" : "night";
    setTheme(next);
    applyTheme(next);
  };

  const isNight = theme === "night";

  return (
    <button
      type="button"
      className="menu-btn theme-btn"
      onClick={toggle}
      aria-label={isNight ? "Cambiar a modo día" : "Cambiar a modo noche"}
      title={isNight ? "Modo día" : "Modo noche"}
    >
      {isNight ? "Día" : "Noche"}
    </button>
  );
});
