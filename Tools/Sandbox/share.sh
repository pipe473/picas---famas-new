#!/usr/bin/env sh
# Arranca el sandbox y un túnel HTTPS gratis (Cloudflare) para jugar con amigos
# en Madrid, Barcelona, etc. Sin VPS ni cuenta. El Mac tiene que seguir encendido.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
PORT="${PORT:-8080}"

if [ ! -x "$HERE/pf_sandbox" ]; then
  echo "No está compilado. Ejecuta: $HERE/build.sh"
  exit 1
fi

"$HERE/pf_sandbox" serve --port "$PORT" &
GAME_PID=$!
trap 'kill $GAME_PID 2>/dev/null; wait $GAME_PID 2>/dev/null' EXIT INT TERM

i=0
while [ "$i" -lt 80 ]; do
  if command -v curl >/dev/null 2>&1 && curl -sf "http://127.0.0.1:$PORT/api/state" >/dev/null 2>&1; then
    break
  fi
  i=$((i + 1))
  sleep 0.1
done

echo ""
echo "Local:  http://127.0.0.1:$PORT/"
echo ""

if command -v cloudflared >/dev/null 2>&1; then
  echo "Túnel gratis: copia la URL https://….trycloudflare.com y mándala por WhatsApp."
  echo "Ctrl+C para parar el juego y el túnel."
  echo ""
  cloudflared tunnel --url "http://127.0.0.1:$PORT"
  exit 0
fi

echo "No está cloudflared (túnel gratis). Instálalo con:"
echo "  brew install cloudflared"
echo ""
echo "O abre un túnel SSH gratis en otra terminal:"
echo "  ssh -R 80:localhost:$PORT nokey@localhost.run"
echo ""
echo "Mientras, el juego sigue en http://127.0.0.1:$PORT/"
echo "Ctrl+C para parar."
wait "$GAME_PID"
