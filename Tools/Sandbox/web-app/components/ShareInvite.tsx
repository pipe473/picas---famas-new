"use client";

import { useEffect, useState } from "react";
import { scheduleShareUrl, shareUrl } from "@/lib/api";
import {
  canNativeShare,
  copyText,
  inviteText,
  nativeShare,
  scheduleInviteText,
  telegramUrl,
  whatsappUrl,
} from "@/lib/share";

type Feedback = "link" | "code" | "shared" | null;

function WhatsAppIcon() {
  return (
    <svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
      <path d="M12 2a10 10 0 0 0-8.6 15.1L2 22l5-1.3A10 10 0 1 0 12 2Zm0 1.8a8.2 8.2 0 1 1-4.2 15.3l-.3-.2-3 .8.8-2.9-.2-.3A8.2 8.2 0 0 1 12 3.8Zm-3.3 4.4c-.2 0-.5.1-.7.3-.3.3-1 1-1 2.3s1 2.7 1.1 2.9c.2.2 2 3.1 4.8 4.2 2.4.9 2.9.8 3.4.7.5 0 1.7-.7 1.9-1.3.2-.7.2-1.2.2-1.3-.1-.1-.3-.2-.5-.3l-1.9-.9c-.3-.1-.4-.1-.6.1l-.9 1.1c-.2.2-.3.2-.6.1-.3-.1-1.2-.4-2.2-1.4-.8-.7-1.4-1.6-1.5-1.9-.2-.3 0-.4.1-.6l.4-.5.3-.5c.1-.2 0-.3 0-.5l-.9-2c-.2-.5-.4-.5-.6-.5h-.5Z" />
    </svg>
  );
}

function TelegramIcon() {
  return (
    <svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
      <path d="M21.5 3.4 2.9 10.6c-1.1.4-1.1 1.1-.2 1.4l4.7 1.5 1.8 5.6c.2.6.1.9.8.9.5 0 .7-.2 1-.5l2.3-2.3 4.8 3.6c.9.5 1.5.2 1.8-.8l3.2-15c.3-1.3-.5-1.9-1.6-1.6ZM8.6 13.2l9.9-6.3c.5-.3.9-.1.5.2l-8.4 7.6-.3 3.5-1.7-5Z" />
    </svg>
  );
}

function ShareIcon() {
  return (
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
      <path d="M12 3v12M7 8l5-5 5 5M5 13v6a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-6" />
    </svg>
  );
}

function LinkIcon() {
  return (
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
      <path d="M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1" />
    </svg>
  );
}

/**
 * Invitar a la sala o a una cita agendada: WhatsApp/Telegram con mensaje listo,
 * "Compartir…" con la hoja del sistema, y copiar enlace o código.
 */
export function ShareInvite({
  code,
  compact,
  onInvite,
  kind = "room",
  whenLabel,
}: {
  code: string;
  compact?: boolean;
  onInvite?: () => void;
  kind?: "room" | "schedule";
  whenLabel?: string;
}) {
  const [fb, setFb] = useState<Feedback>(null);
  const [native, setNative] = useState(false);

  useEffect(() => setNative(canNativeShare()), []);

  useEffect(() => {
    if (!fb) return;
    const t = window.setTimeout(() => setFb(null), 1600);
    return () => window.clearTimeout(t);
  }, [fb]);

  const isSchedule = kind === "schedule";
  const url = isSchedule ? scheduleShareUrl(code) : shareUrl(code);
  const text = isSchedule ? scheduleInviteText(code, whenLabel || "pronto") : inviteText(code);
  const tgHref = isSchedule
    ? `https://t.me/share/url?${new URLSearchParams({ url, text: `¿Jugamos a Picas y Famas${whenLabel ? ` el ${whenLabel}` : ""}? Código: ${code}` }).toString()}`
    : telegramUrl(code);
  const sent = () => onInvite?.();

  const copyLink = async () => {
    await copyText(url);
    setFb("link");
    sent();
  };
  const copyCode = async () => {
    await copyText(code);
    setFb("code");
    sent();
  };
  const share = async () => {
    if (isSchedule) {
      if (!canNativeShare()) return;
      try {
        await navigator.share({ title: "Picas y Famas", text, url });
        setFb("shared");
        sent();
      } catch {
        /* cancelado */
      }
      return;
    }
    if (await nativeShare(code)) {
      setFb("shared");
      sent();
    }
  };

  return (
    <div className={`share${compact ? " compact" : ""}`} aria-label={isSchedule ? "Invitar a la cita" : "Invitar jugadores"}>
      <a className="share-btn wa" href={whatsappUrl(text)} target="_blank" rel="noopener noreferrer" onClick={sent}>
        <WhatsAppIcon />
        WhatsApp
      </a>
      <a className="share-btn tg" href={tgHref} target="_blank" rel="noopener noreferrer" onClick={sent}>
        <TelegramIcon />
        Telegram
      </a>
      {native ? (
        <button type="button" className="share-btn" onClick={() => void share()}>
          <ShareIcon />
          {fb === "shared" ? "Enviado ✓" : "Compartir…"}
        </button>
      ) : null}
      <button type="button" className="share-btn" onClick={() => void copyLink()}>
        <LinkIcon />
        {fb === "link" ? "Enlace copiado ✓" : "Copiar enlace"}
      </button>
      <button type="button" className="share-btn" onClick={() => void copyCode()}>
        {fb === "code" ? "Código copiado ✓" : `Copiar código · ${code}`}
      </button>
    </div>
  );
}
