#!/usr/bin/env sh
# Compila el sandbox de consola (no requiere Unreal). Salida: Tools/Sandbox/pf_sandbox
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../Source/PicasyFamas"
CXX="${CXX:-clang++}"
"$CXX" -std=c++20 -O2 -Wall -Wextra -I"$SRC" "$HERE/PFSandbox.cpp" "$SRC/Core/PFRoundEngine.cpp" -o "$HERE/pf_sandbox"
echo "ok: $HERE/pf_sandbox"
echo "  $HERE/pf_sandbox play 3          # tu contra 3 bots"
echo "  $HERE/pf_sandbox sim 200 5       # 200 partidas con 5 bots"
