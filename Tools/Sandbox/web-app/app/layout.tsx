import type { Metadata } from "next";
import type { ReactNode } from "react";
import "./globals.css";

export const metadata: Metadata = {
  title: "Picas y Famas · El Enigma Único",
};

const themeBoot = `(function(){try{var t=localStorage.getItem("pf-theme");if(t!=="day"&&t!=="night")t="night";document.documentElement.setAttribute("data-theme",t);}catch(e){document.documentElement.setAttribute("data-theme","night");}})();`;

export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="es" data-theme="night" suppressHydrationWarning>
      <head>
        <script dangerouslySetInnerHTML={{ __html: themeBoot }} />
      </head>
      <body>{children}</body>
    </html>
  );
}
