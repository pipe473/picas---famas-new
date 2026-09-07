import { shareUrl } from "./api";

/** Texto de la invitación: código a mano por si el enlace no se abre, y el enlace para entrar de un toque. */
export function inviteText(code: string) {
  const url = shareUrl(code);
  return `¡Juega conmigo a Picas y Famas! 🔢\nCódigo de sala: ${code}\nEntra aquí: ${url}`;
}

/**
 * `wa.me` abre la app en móvil y WhatsApp Web en escritorio, y deja elegir el contacto o grupo.
 * Sin número fijo: la invitación es a quien el anfitrión quiera.
 */
export function whatsappUrl(text: string) {
  return `https://wa.me/?text=${encodeURIComponent(text)}`;
}

export function telegramUrl(code: string) {
  const q = new URLSearchParams({ url: shareUrl(code), text: `¡Juega conmigo a Picas y Famas! Código de sala: ${code}` });
  return `https://t.me/share/url?${q.toString()}`;
}

/** Hoja de compartir del sistema (móvil y algunos escritorios). Solo en HTTPS o localhost. */
export function canNativeShare() {
  return typeof navigator !== "undefined" && typeof navigator.share === "function";
}

/** Devuelve `false` si el usuario canceló o el navegador no lo permite, para caer al portapapeles. */
export async function nativeShare(code: string): Promise<boolean> {
  if (!canNativeShare()) return false;
  try {
    await navigator.share({ title: "Picas y Famas", text: `Código de sala: ${code}`, url: shareUrl(code) });
    return true;
  } catch {
    return false;
  }
}

export async function copyText(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch {
    window.prompt("Copia este texto", text);
    return false;
  }
}
