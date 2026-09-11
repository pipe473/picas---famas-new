# Picas y Famas: El Enigma Único en Tiempo Real

Deducción lógica competitiva de 3 a 8 jugadores sobre Unreal Engine 5 con Dedicated Server.
El diseño completo está en [`Docs/GDD_Express_PicasyFamas.md`](Docs/GDD_Express_PicasyFamas.md).

## Estructura

```
Source/PicasyFamas/
  Core/                    C++ puro, sin dependencias de Unreal (compilable con clang/gcc/msvc)
    PFCodeMath.h           empaquetado, validación, evaluación O(1) de Picas/Famas, RNG determinista
    PFCandidateSet.h       conjunto de códigos consistentes: bits de información y Pista Clave
    PFRoundTypes.h         configuración, flags, eventos
    PFRoundEngine.h/.cpp   motor de ronda: cola por tick, desempates por latencia, foto-finish,
                           Encriptar/Señuelo/Sospechar, relojes, Muerte Sudada, AFK, reconexión
  PFGameMode.h/.cpp        adaptador del motor: fases, asientos, eventos -> replicación
  PFGameState.h/.cpp       historial global (FFastArraySerializer), fase, relojes de sala
  PFPlayerState.h/.cpp     estado público por jugador (Push Model) + historial privado del dueño
  PFPlayerController.*     Server RPCs (encolan) y Client RPCs (resultado privado, combos)
  PFTuningDataAsset.*      todas las palancas de balanceo
  PFTypes.*                enums Blueprint y utilidades de (des)empaquetado
  Tests/PFCoreTests.cpp    Automation Tests
Tools/Sandbox/             entorno local sin Unreal: el motor de ronda real con bots, jugable en consola
```

## Probar sin Unreal (sandbox local)

`Tools/Sandbox` ejecuta el mismo `PFRoundEngine` que usará el servidor, con bots de cuatro perfiles
(Cerrador, Sondeador, Farolero, Paciente) que deducen sobre la vista pública del tablero.

```
./Tools/Sandbox/build.sh                       # requiere clang++ (Xcode CLT) o g++
./Tools/Sandbox/pf_sandbox play 3              # tú contra 3 bots, en tiempo real
./Tools/Sandbox/pf_sandbox sim 200 5 --human   # 200 partidas con 5 bots humanizados + estadísticas de balance
./Tools/Sandbox/pf_sandbox serve 3 --port 8080 # interfaz web en http://127.0.0.1:8080 (sala, ritmo, reloj, turnos)
```

El HUD web es una app **Next.js** (`Tools/Sandbox/web-app`) exportada a HTML/JS estático. `build.sh` la compila y el binario C++ la sirve junto a `/api/*`. Para iterar el UI sin recompilar C++:

```
./Tools/Sandbox/pf_sandbox serve 3 --port 8080
cd Tools/Sandbox/web-app && npm install && npm run dev   # http://127.0.0.1:3000 → API en :8080
```

Requiere Node 20+ para el frontend. El modo `play` / `sim` de consola no lo necesita.

**Sonido.** El botón `♫` de la cabecera abre el panel de sonido con tres modos: **Música** (un reproductor de
Spotify flotante; pega el enlace de *Compartir* de cualquier playlist, álbum o canción y queda guardado en el
navegador), **Efectos** (solo los pitidos del juego) y **Silencio** (modo descanso: ni pitidos ni música). El
reproductor sigue sonando al plegarlo, ocultarlo o cambiar de fase. No hace falta cuenta de desarrollador ni
claves: sin sesión en Spotify se oyen 30 s por tema; iniciando sesión desde el propio reproductor, completo.

**Rankings.** Hay tres: uno por modo de juego y un global que suma los dos:

- *Ranking individual* (partidas de **un humano contra bots**): tabla de las 100 mejores partidas, ordenada por
  puntos de partida (tres rondas). Cada fila guarda nombre, puntos, número de bots, ritmo, modo de turnos, rondas
  ganadas y fecha. Se ve desde la pantalla de inicio (desplegable *Rankings*), en el menú y al terminar la
  partida, donde se indica el puesto conseguido. Persiste en `ranking_solo.tsv` dentro del directorio de datos
  (`--data DIR`, variable `PF_DATA_DIR` o, por defecto, `Tools/Sandbox/data/`), así que sobrevive a reinicios del
  servidor; en Render sin disco persistente se pierde al redesplegar.
- *Clasificación de la sala* (partidas con **dos o más amigos**): acumulada por nombre mientras la sala siga viva,
  partida tras partida con *Una más*. Cuenta victorias (1.º de la mesa, bots incluidos), podios, puntos y partidas;
  ordena por victorias y, a igualdad, por puntos. Aparece al final de cada partida, en la sala de espera, en el menú
  (pestaña *Sala*) y como un pequeño trofeo con el número de victorias junto al nombre de cada jugador. Si dos amigos
  entran con el mismo nombre, al segundo se le añade un sufijo (`Bea 2`) para que no se fundan en la tabla.
- *Ranking global* (**todas** las partidas, contra bots y con amigos): trayectoria acumulada por nombre de jugador.
  Suma puntos, victorias, podios, partidas (desglosadas en individuales y con amigos), rondas ganadas y mejor
  partida; ordena por puntos totales, a igualdad por victorias y después por menos partidas. Se guardan hasta 500
  jugadores en `ranking_global.tsv`, en el mismo directorio de datos. Tiene su pestaña *Global* en el desplegable de
  inicio y en el diálogo del menú, y al terminar cada partida se indica el puesto global del jugador.

`GET /api/ranking?limit=N&id=I&name=X` expone los dos rankings persistentes en JSON: `solo` (con `mine` si `id`
coincide con una partida) y `global` (con `me` si `name` coincide con un jugador).

Comandos en partida: `1234` intento · `e1234` encriptar · `d1234 2 1` señuelo (finge 2F 1P) · `s 12` sospechar de #12 · `q` salir.
Opciones: `--seed N`, `--pace slow|normal|fast`, `--attempt N|free` (segundos por intento; `free` = **tiempo libre**),
`--turns simultaneous|seat|random` (por turnos, el cronómetro corre solo para quien tiene el turno; `random` baraja el
orden en cada ronda), `--decoy-cap N`, `--data DIR` (directorio de datos de los rankings en `serve`).

**Tiempo libre** (`--attempt free` o `--free`; en el HUD web, *Reloj → Libre (sin reloj)*): nadie tiene reloj de intento,
cada cual tira cuando quiere y, por turnos, el turno dura hasta que su dueño envía. No hay Pasos por expiración, la ronda
no tiene tope de tiempo (solo acaba por acierto) y la alerta de N-1 Famas se muestra pero sin Muerte Sudada cronometrada.
Los bots se imponen un ritmo propio (~10 s) para no bloquear la mesa.

## Requisitos

- Unreal Engine 5.4 (Windows, macOS o Linux). Cambiar `EngineAssociation` en el `.uproject` si se usa otra 5.x.
- Para el target `PicasyFamasServer` hace falta un motor compilado desde código fuente (los binarios del Launcher no incluyen targets de servidor).

## Compilar y ejecutar

1. Clic derecho en `PicasyFamas.uproject` → *Generate project files*, o desde consola:
   `<UE>/Engine/Build/BatchFiles/<Plataforma>/GenerateProjectFiles.sh -project=<ruta>/PicasyFamas.uproject -game`
2. Abrir el proyecto en el editor. Compila el módulo `PicasyFamas`.
3. Crear un mapa vacío (`Content/Maps/L_Sala`), asignarlo como *Editor Startup Map* y *Game Default Map*.
   El GameMode por defecto ya es `PFGameMode` (`Config/DefaultEngine.ini`).
4. *Play* con *Number of Players* = 3 y *Net Mode* = *Play As Listen Server* (o *Client* con dedicated server).
   Para probar en solitario: en el Blueprint hijo de `PFGameMode` poner `MinPlayersOverride = 1`, o en consola `PFForceStart`.

La UI (UMG) no está en este commit: la lógica expone delegados Blueprint (`OnGuessEntryAdded`,
`OnPhaseChanged`, `OnGuessResult`, `OnCombo`...) y `APFPlayerController::SubmitGuess(...)` para conectarla.

## Tests

**Dentro de Unreal** (Session Frontend → Automation → `PicasyFamas.*`), o en consola:

```
UnrealEditor-Cmd <ruta>/PicasyFamas.uproject -ExecCmds="Automation RunTests PicasyFamas; Quit" -unattended -nullrhi -nosplash
```

**Sin Unreal** (el núcleo es C++ estándar; el harness vive fuera del repo, por ejemplo en `/tmp`):

```
clang++ -std=c++20 -O1 -Wall -Wextra -I Source/PicasyFamas <harness>.cpp Source/PicasyFamas/Core/PFRoundEngine.cpp -o test_core && ./test_core
```

## Dedicated Server

```
RunUAT BuildCookRun -project=<ruta>/PicasyFamas.uproject -platform=Linux -server -serverplatform=Linux -noclient -build -cook -stage -pak
PicasyFamasServer L_Sala -log -port=7777
```

El servidor corre a 30 Hz (`NetServerMaxTickRate`); la justicia entre intentos simultáneos la da el
timestamp ajustado por latencia, no la frecuencia del tick.
