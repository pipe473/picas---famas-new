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
```

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
