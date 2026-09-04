import type { NextConfig } from "next";

// Export estático: el sandbox C++ sirve HTML/JS/CSS. En `next dev` la API vive en :8080.
const nextConfig: NextConfig = {
  output: "export",
  images: { unoptimized: true },
  eslint: { ignoreDuringBuilds: true },
  trailingSlash: false,
};

export default nextConfig;
