#!/usr/bin/env sh
# Compila el sandbox. Salida: Tools/Sandbox/pf_sandbox
# El frontend Next.js se exporta a Tools/Sandbox/web (HTML/JS estático que sirve el binario).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../Source/PicasyFamas"
CXX="${CXX:-clang++}"

if command -v npm >/dev/null 2>&1; then
  (cd "$HERE/web-app" && { [ -d node_modules ] || npm install; } && npm run build)
else
  echo "aviso: npm no está; se reutiliza Tools/Sandbox/web si ya existe un export"
fi

"$CXX" -std=c++20 -O2 -Wall -Wextra -I"$SRC" "$HERE/PFSandbox.cpp" "$SRC/Core/PFRoundEngine.cpp" -o "$HERE/pf_sandbox"
echo "ok: $HERE/pf_sandbox"
echo "  $HERE/pf_sandbox play 3          # tu contra 3 bots"
echo "  $HERE/pf_sandbox sim 200 5       # 200 partidas con 5 bots"
echo "  $HERE/pf_sandbox serve 3 --port 8080"
echo "  frontend en caliente: (cd Tools/Sandbox/web-app && NEXT_PUBLIC_API_BASE=http://127.0.0.1:8080 npm run dev)"
