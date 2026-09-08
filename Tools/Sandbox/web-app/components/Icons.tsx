import type { SVGProps } from "react";

// Iconos de trazo (24×24, stroke 2) en lugar de emojis: mismo peso visual en todos los sistemas
// y heredan el color del texto, así responden a los temas y a los estados.
type IconProps = SVGProps<SVGSVGElement>;

function Base({ children, className, ...rest }: IconProps) {
  return (
    <svg
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="2"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      focusable="false"
      className={className ? `icon ${className}` : "icon"}
      {...rest}
    >
      {children}
    </svg>
  );
}

export function LockIcon(p: IconProps) {
  return (
    <Base {...p}>
      <rect x="5" y="11" width="14" height="10" rx="2.5" />
      <path d="M8 11V7.5a4 4 0 0 1 8 0V11" />
    </Base>
  );
}

export function MaskIcon(p: IconProps) {
  return (
    <Base {...p}>
      <path d="M4 5.5c2.6 1.1 5.3 1.7 8 1.7s5.4-.6 8-1.7V12c0 4.7-3.5 8.3-8 9.5C7.5 20.3 4 16.7 4 12V5.5z" />
      <path d="M8.3 11.6c.9-.7 2-.7 2.9 0M12.8 11.6c.9-.7 2-.7 2.9 0M9 15.3c1.8 1.4 4.2 1.4 6 0" />
    </Base>
  );
}

export function TrophyIcon(p: IconProps) {
  return (
    <Base {...p}>
      <path d="M8 3h8v6a4 4 0 0 1-8 0V3z" />
      <path d="M8 5H5.5a2.5 2.5 0 0 0 2.6 4M16 5h2.5a2.5 2.5 0 0 1-2.6 4" />
      <path d="M12 13v3M9 21h6M10 16h4v5h-4z" />
    </Base>
  );
}

export function BackspaceIcon(p: IconProps) {
  return (
    <Base {...p}>
      <path d="M9 5h11a1 1 0 0 1 1 1v12a1 1 0 0 1-1 1H9l-6-7 6-7z" />
      <path d="M12.5 9.5l5 5M17.5 9.5l-5 5" />
    </Base>
  );
}

export function MusicIcon(p: IconProps) {
  return (
    <Base {...p}>
      <path d="M9 18V5.5l11-2V16" />
      <circle cx="6" cy="18" r="3" />
      <circle cx="17" cy="16" r="3" />
    </Base>
  );
}

export function MutedIcon(p: IconProps) {
  return (
    <Base {...p}>
      <path d="M11 5 6 9H3v6h3l5 4V5z" />
      <path d="M22 9l-6 6M16 9l6 6" />
    </Base>
  );
}

export function AlertIcon(p: IconProps) {
  return (
    <Base {...p}>
      <path d="M10.3 3.9 1.9 18a2 2 0 0 0 1.7 3h16.8a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z" />
      <path d="M12 9v4M12 17h.01" />
    </Base>
  );
}

export function CrownIcon(p: IconProps) {
  return (
    <Base {...p}>
      <path d="M3 8l4.5 4L12 5l4.5 7L21 8l-2 11H5L3 8z" />
      <path d="M5 19h14" />
    </Base>
  );
}

export function SendIcon(p: IconProps) {
  return (
    <Base {...p}>
      <path d="M5 12h14M13 6l6 6-6 6" />
    </Base>
  );
}
