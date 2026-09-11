# PICAS Y FAMAS: EL ENIGMA ÚNICO EN TIEMPO REAL — Documento de Diseño

| Campo | Valor |
|---|---|
| Versión | 0.2 (Arquitectura de gameplay y técnica) |
| Motor | Unreal Engine 5.4+ · C++ · Dedicated Servers (Linux) |
| Jugadores | 3–8 (Speed Race) · 2v2 / 3v3 / 4v4 (Equipos) |
| Duración | Ronda 60–120 s · Partida al mejor de 3 rondas (≈ 4–5 min) |
| Principio rector | **Toda la información es pública y, al final, verdadera.** Lo único que se compra con habilidad es *tiempo*: ocultar 5 s, engañar 8 s, deducir 3 s antes que el resto. |

---

## 0. Resumen ejecutivo

Un solo código secreto por ronda, generado por el servidor. De 3 a 8 jugadores lo atacan simultáneamente y **cada intento de cada jugador, con su resultado de Picas y Famas, aparece en un Módulo del Enigma público**. La deducción de tu rival es tu pista; la tuya es la suya.

El dilema que sostiene el juego:

> *¿Sondeo con combinaciones arriesgadas para extraer información (y regalarla a la sala), o espero a que los demás fallen para robarles la deducción y dar el golpe final?*

Tres decisiones de diseño lo mantienen honesto con la esencia lógica del original:

1. **Puntuación por información aportada.** El servidor mide cuántos *bits* de incertidumbre elimina cada intento (reducción del conjunto de códigos candidatos). Sondear se paga. Esperar se paga. Ninguna de las dos estrategias domina.
2. **Faroles con fecha de caducidad.** Ocultar o falsear un resultado dura segundos, nunca la ronda. Un farol torpe genera una contradicción lógica que un buen jugador detecta *sin ninguna herramienta*: el propio razonamiento es la contramedida.
3. **Pista Clave y Deducción Relámpago.** El servidor sabe en qué instante el código queda lógicamente determinado por la información pública. Quien lo escriba en los 3 s siguientes cobra el combo. Esto premia leer el tablero, no teclear rápido al azar.

---

## 1. Mecánicas Core Multijugador

### 1.1 Reglas base (compartidas por todos los modos)

- Código de **4 dígitos distintos** (0–9) → 5 040 candidatos. En salas de 6–8 jugadores se recomienda **5 dígitos** (30 240 candidatos) para que la información no se agote demasiado pronto (ver §6).
- **Fama** = dígito correcto en posición correcta. **Pica** = dígito correcto en posición incorrecta.
- El resultado de un intento se publica en el Módulo del Enigma para toda la sala en el mismo tick del servidor en que se valida.
- Los intentos repetidos (idénticos a uno ya público) están permitidos pero aportan 0 bits y no puntúan.

### 1.2 Modo «Speed Race» (todos contra todos, 3–8 jugadores)

**Interacción simultánea, no por turnos.** Cada jugador tiene su propio **reloj de intento de 10 s**. Se reinicia al enviar. Si expira, el servidor registra un *Paso* (−5 pts) y reinicia el reloj. Dos pasos consecutivos marcan al jugador como *Inactivo* (§5.2). No existe bloqueo: nadie espera a nadie.

**Estructura de ronda:**

| Fase | Disparador | Qué cambia |
|---|---|---|
| Sondeo | 0 s | Relojes de 10 s. Módulo del Enigma abierto. |
| Alerta 3 Famas | Un jugador alcanza 3 Famas (o *N−1* Famas con 5 dígitos) | Alerta global visual y sonora. Se activa **Muerte Sudada**: cuenta atrás de 20 s para toda la sala; los relojes de intento bajan a **6 s**. |
| Resolución | Alguien acierta, o expira la Muerte Sudada, o se alcanza el cap de 120 s | Fin de ronda. Puntuación. 8 s de repaso del código y de la jugada ganadora. |

**Cierre sin acierto:** si la Muerte Sudada expira, gana la ronda quien tenga más Famas en su mejor intento; empate → menos intentos totales; empate → el que llegó antes a ese mejor intento (timestamp de servidor).

**Puntuación por ronda:**

| Evento | Puntos | Justificación |
|---|---|---|
| Acierto del código | +100 | Objetivo primario |
| Deducción Relámpago (acierto ≤ 3 s tras la Pista Clave) | +50 | Premia leer el tablero |
| Deducción Rápida (3–6 s) | +25 | |
| Bits de información aportados | +10 por bit (≈ 3–4 bits en un buen sondeo inicial) | Premia al que sondea y regala pistas |
| Pista Clave (tu intento dejó el código lógicamente determinado) | +30 | Aunque otro te lo robe, tu deducción vale |
| Engaño efectivo (un rival hace un intento inconsistente con la verdad pero consistente con tu Señuelo) | +15 por rival engañado | El farol sólo puntúa si funciona |
| Farol detectado por *Sospechar* | −30 al farolero · +15 al que sospechó | |
| Sospecha infundada | −10 | Evita el spam de sospechas |
| Paso (reloj expirado) | −5 | |

Una partida son **3 rondas**; gana la suma. El diseño hace viable ganar una partida *sin acertar ningún código* si aportas la mayoría de la información — y viable ganar acertando siempre a costa del trabajo ajeno. Ambos perfiles son legítimos y se odian mutuamente, que es lo que queremos en una sala.

#### 1.2.1 Variante «Por turnos» (opcional, misma ronda)

Para mesas que prefieren un ritmo de sobremesa, la sala puede activar el **modo por turnos** (`ETurnMode`):

| Modo | Orden | Cronómetro |
|---|---|---|
| `Simultaneous` (por defecto) | Nadie espera: todos envían a la vez | Un reloj por jugador, se reinicia con cada intento |
| `SeatOrder` | Rotación fija por asiento | **Solo corre para quien tiene el turno**; arranca al recibirlo |
| `RandomOrder` | Se baraja al empezar **cada ronda** (Fisher-Yates con la semilla de la ronda: reproducible en replay) | Igual que arriba |

Reglas del turno:

- Solo el jugador con el turno puede enviar; el resto recibe `NotYourTurn` (la UI bloquea el teclado y muestra "Turno de X").
- El turno se consume con un **intento** o con un **Paso** (reloj expirado, −5). En ambos casos pasa al siguiente y su reloj arranca en ese instante.
- Desconectados, Inactivos (2 Pasos seguidos) y caídos **se saltan** en la rotación; si nadie puede jugar, la ronda espera al cap. Un Inactivo que intenta jugar fuera de turno vuelve a entrar en la rotación en la siguiente vuelta.
- Sospechar y leer el tablero se permiten en cualquier momento: la deducción no se detiene aunque no sea tu turno.
- La Muerte Sudada sigue igual (20 s globales) y acorta el reloj del turno a 6 s: con 4 jugadores hay 2–3 turnos para cerrar, lo que en simulación hace que expire en un 4–7 % de las rondas (en Speed Race casi nunca).

El servidor emite `TurnChanged(Player, TurnNumber, Deadline)`; `APFGameState` replica `TurnMode`, `TurnPlayerIndex`, `TurnNumber` y `TurnOrder`, y el reloj del turno viaja en el `AttemptDeadlineServerTime` del `PlayerState` correspondiente.

#### 1.2.2 Variante «Tiempo libre» (opcional, combinable con cualquier modo de turnos)

Para mesas que no quieren cronómetro, la sala puede activar el **tiempo libre** (`UPFTuningDataAsset::bFreeTime`; en el sandbox `--attempt free` o *Reloj → Libre* en el HUD web). En el motor equivale a `FRoundConfig::MakeFreeTime()`: `AttemptSeconds`, `SuddenDeathSeconds` y `RoundCapSeconds` a 0. Cada uno de los tres relojes es independiente (`<= 0` lo desactiva), pero la variante los quita todos porque un tope de 120 s o una Muerte Sudada de 20 s no tienen sentido cuando nadie tiene prisa.

| Regla | Con reloj (por defecto) | Tiempo libre |
|---|---|---|
| Reloj de intento | 10 s (6 s en Muerte Sudada), se reinicia con cada intento / turno | **No hay**: `AttemptDeadline = 0` siempre. Nadie recibe `DeadlineExpired` |
| Paso e Inactivo | Reloj expirado → Paso (−5); 2 seguidos → Inactivo | No existen por expiración (un desconectado sigue saltándose) |
| Turno (`SeatOrder` / `RandomOrder`) | Se consume con intento **o** Paso | Solo se consume con un **intento** (o al desconectarse su dueño): el turno dura lo que quiera quien lo tiene |
| Alerta N-1 Famas | Muerte Sudada: 20 s globales, relojes a 6 s | Se emite `Alert` (la UI avisa), pero **sin** `SuddenDeathStarted` ni cuenta atrás |
| Fin de ronda | Acierto, Muerte Sudada expirada o cap de 120 s | **Solo acierto**. Salvaguarda: si todos los presentes se caen y agotan la gracia de reconexión, la ronda acaba por `NotEnoughPlayers` |
| Puntuación | — | Igual (combos Relámpago/Rápida siguen midiendo segundos desde la Pista Clave) |

`APFGameState` replica `bFreeTime` y `RoundCapServerTime = 0`; la UI oculta arcos de reloj y cuentas atrás. Los bots del sandbox se imponen un ritmo propio (~10 s, `kBotSelfClockSeconds`) para que la mesa no dependa de un Cerrador que solo tira con ≤ 2 candidatos.

### 1.3 Faroleo y Ocultación

Cada jugador dispone por ronda de **1 ficha de Encriptar** y **1 ficha de Señuelo** (en Equipos, las fichas son del equipo, ver §1.4). No se recargan: gastarlas pronto es perder una herramienta para la Muerte Sudada.

| Habilidad | Efecto público | Duración | Contramedida |
|---|---|---|---|
| **Encriptar** | Tu intento aparece con los dígitos visibles pero el resultado como `??` con un candado y una cuenta atrás. | 5 s, luego se revela la verdad | Ninguna: es información retrasada, no falsa. Te da 5 s de ventaja si tu intento fue la Pista Clave. |
| **Señuelo** | Tu intento aparece con un resultado de Picas/Famas **falso que tú eliges** (no puede ser 4 Famas). | 8 s, luego se corrige y se estampa `SEÑUELO` en rojo | **Sospechar**: cualquier rival puede tocar la entrada. Si es un Señuelo se revela al instante. Si era verdad, el sospechoso pierde 10. |
| **Sospechar** | Marca pública sobre una entrada rival. | Instantáneo | — |

Regla de oro del faroleo: **el servidor nunca miente al dueño del intento** (recibe su resultado real por RPC privado) y **la verdad siempre acaba en el tablero**. El Módulo del Enigma es una fuente de verdad *eventual*, no *instantánea*. Un jugador que espere 8 s tiene un tablero perfecto; un jugador que actúe en caliente asume riesgo. Ese es el precio del ritmo.

Nota de diseño: un Señuelo que contradiga intentos ya públicos (p. ej., declarar 2 Famas para un dígito que otra entrada demostró ausente) es detectable por lógica pura. La habilidad de farolear *bien* es una habilidad de deducción, no de suerte.

### 1.4 Modo Equipos (2v2 / 3v3 / 4v4)

Mismo código para ambos equipos. Gana el equipo cuyo miembro acierte primero; los puntos de información se suman al equipo.

**Qué ve cada jugador:**

| Fuente | Dígitos | Resultado |
|---|---|---|
| Aliados | Visibles, con borde del color del aliado | Visible |
| Rivales | **Cifrados** (`▪▪▪▪`) | Sólo el número de Famas |

Cifrar los dígitos rivales es lo que hace que el equipo exista: si vieras sus intentos completos, el modo sería un Speed Race con banderines. Ver *sólo sus Famas* mantiene la tensión (sabes si van por delante) sin regalarles tu trabajo ni robarles el suyo.

**Compartir historial sin saturar (regla de las tres capas):**

1. **Capa de intentos** — El Módulo del Enigma muestra sólo los intentos del equipo, en una única columna cronológica, con chip de color por aliado. En 4v4 (hasta 4 intentos por cada 10 s) el módulo colapsa por defecto los intentos con **0 bits** en una fila fina «+2 repetidos» expandible.
2. **Capa de Pizarra** — Una rejilla compartida de **10 dígitos × N posiciones** donde cualquier aliado marca manualmente `✓` (confirmado en posición), `~` (presente, posición desconocida) o `✗` (descartado). Las marcas se replican en tiempo real con el color del autor; si dos aliados se contradicen, la celda parpadea en ámbar. **La Pizarra nunca deduce sola**: es papel cuadriculado compartido, no un solver. Ahí vive la esencia del juego.
3. **Capa de pings** — Un aliado puede *pinear* una entrada del historial (icono ⚑ + su color) para decir «mirad esta». Tres pings rápidos lanzan un preset de voz corto («Pista clave», «Es señuelo», «Yo voy»). Sin chat de texto en ronda: no hay tiempo para leerlo y rompe el ritmo.

**Fichas de equipo:** 1 Encriptar + 1 Señuelo por equipo y ronda, independientemente del tamaño. Usarlas requiere que ningún aliado esté en medio de un envío (evita pisarse). El equipo decide quién farolea: eso también es coordinación.

**Roles emergentes (sin imponerlos):** en playtests de papel aparecen de forma natural un *sondeador* (abre con intentos de máxima información), un *cerrador* (espera al candidato único y teclea) y un *vigía* (mira las Famas rivales y decide cuándo forzar). El diseño no asigna roles; los premia con títulos post-partida (§7).

---

## 2. Sistemas de Adicción y Tensión Compartida

### 2.1 Indicador de Cercanía Global

Cuando cualquier jugador (o equipo) alcanza **3 Famas** por primera vez en la ronda:

- **Visual:** banner rojo de 1.5 s «⚠ 3 FAMAS — Jugador X». El avatar de X pulsa en rojo lo que quede de ronda. El borde de la pantalla adquiere una viñeta que respira al ritmo de la cuenta atrás.
- **Sonoro:** *stinger* de alarma + la música pierde el bajo y entra un latido a 90 bpm que acelera hasta 140 bpm en los últimos 5 s.
- **Mecánico:** se activa **Muerte Sudada**: 20 s de cuenta atrás global, relojes de intento a 6 s. Cuando expira, la ronda termina y se resuelve por §1.2.

Por qué 3 Famas y no «candidato único»: 3 Famas es un umbral que *todos* entienden sin cálculo y que, en la práctica, deja el código a uno o dos intentos de distancia. La sala pasa de «pensar» a «correr», y al que está a 3 Famas le tiembla la mano porque ahora todos usan *su* intento para cerrar.

En salas de 5 dígitos el umbral es *N−1* Famas.

### 2.2 Combo por Velocidad de Deducción

El servidor mantiene el **conjunto de candidatos consistentes** con toda la información pública *verdadera* (excluye Señuelos mientras están activos e incluye lo Encriptado sólo cuando se revela). Cuando ese conjunto llega a **1 elemento**, ese instante es la **Pista Clave** (`T_key`) y el intento que lo provocó recibe +30.

| Acierto enviado | Combo |
|---|---|
| `T_key` + ≤ 3 s | **Deducción Relámpago** +50. La UI muestra ⚡ y el tiempo exacto («1.84 s»). |
| `T_key` + 3–6 s | Deducción Rápida +25 |
| Después de 6 s | Sin combo |
| **Antes** de `T_key` (había más de un candidato) | Sin combo. El acierto es *intuición o suerte*, se paga a 100, y se etiqueta «Golpe de intuición». No se penaliza: apostar es legítimo. |

El cronómetro del combo es **invisible para los jugadores**. Si mostráramos «el código ya está determinado», mataríamos la deducción. Lo que ven es el tablero, y quien mejor lo lea, cobra.

### 2.3 Micro-tensión continua

- **Reloj de intento** dibujado como un arco alrededor del botón de enviar; los últimos 3 s cambian a rojo y suena un tic.
- **Mejor Fama de la sala** siempre visible en la esquina superior: «●●○○ — J3». Al subir, un *tick* de xilófono en la posición espacial de ese jugador (§3.2).
- **Ola de intentos:** el Módulo agrupa visualmente los intentos por ventanas de 10 s; una ola con 5 entradas simultáneas produce un sonido de cascada — la sala «siente» que todos han disparado.
- **Fin de ronda (8 s):** el código se revela dígito a dígito con el intento ganador superpuesto; debajo, el desglose «Bits aportados por jugador» en barras. Los sondeadores ven que su trabajo cuenta. El botón «SIGUIENTE RONDA» ya está en pantalla con cuenta atrás de 5 s.

---

## 3. Interfaz de Usuario para Múltiples Jugadores

### 3.1 Principios

1. **Un solo foco:** el Módulo del Enigma ocupa el centro. Todo lo demás es periferia.
2. **Color = jugador, forma = resultado.** Cada jugador tiene un color y un chip; Famas son `●` dorados, Picas `○` plateados. Nunca se usa el color para el resultado, así el daltonismo no rompe la lectura.
3. **Densidad adaptativa:** filas de 36 px con 3–4 jugadores, 28 px con 5–6, 24 px con 7–8. Dígitos siempre en monoespaciada.
4. **La verdad no se mueve:** las entradas nunca se reordenan. Una corrección (Encriptar revelado, Señuelo corregido) ocurre *in situ* con una animación de 300 ms para que el ojo la localice.

### 3.2 Ejemplo conceptual — Speed Race, 6 jugadores, 4 dígitos (1920×1080)

```
┌──────────────────────────────────────────────────────────────────────────────────────────────┐
│ RONDA 2/3     ⏱ 0:47     MEJOR DE LA SALA  ●●●○  J3 (Ana)      ⚠ MUERTE SUDADA  00:14       │
├────────────────┬─────────────────────────────────────────────────────────┬───────────────────┤
│ SALA           │ MÓDULO DEL ENIGMA                 [Todos ▾] [Sólo Famas] │ MI PIZARRA        │
│                │                                                          │      P1 P2 P3 P4  │
│ ● J1 Tú   ●●○○ │ ▌J3  7 2 9 4   ●●●   ○            +2.1b   0:45  ⚑       │  0   ✗  ✗  ✗  ✗  │
│   3 int.       │ ▌J5  1 0 6 3   ??  🔒 3s                   0:44         │  1   ✗  ✗  ✗  ✗  │
│ ● J2 Luis ●●○○ │ ▌J1  7 2 4 8   ●●    ○            +1.3b   0:43         │  2      ✓         │
│   4 int.  🔒   │ ▌J2  5 2 9 8   ●●    ○○           +0.9b   0:41         │  3   ✗  ✗  ✗  ✗  │
│ ● J3 Ana  ●●●○ │ ▌J6  3 8 1 6   ○                  +0.7b   0:40         │  4            ~   │
│   3 int.  ⚠    │ ▌J4  0 1 5 6   ●     SEÑUELO ✗ → ○○        0:38  Sosp. │  5   ✗  ✗  ✗  ✗  │
│ ● J4 Marta●○○○ │ ── ola 0:30 ──────────────────────────────────────────  │  6   ✗  ✗  ✗  ✗  │
│   3 int.  ✗    │ ▌J3  7 5 2 8   ●     ○○           +2.8b   0:29         │  7   ✓            │
│ ● J5 Iker ●○○○ │ ▌J1  0 3 6 9   ○○                 +2.4b   0:28         │  8         ~      │
│   2 int.       │ ▌J2  1 4 5 8   ○                  +1.9b   0:27         │  9         ~  ~   │
│ ● J6 Sam  ○○○○ │  + 3 intentos con 0 bits (repetidos)              [ver] │                   │
│   2 int.  zZ   │                                                          │  Notas: 9 no en P1│
├────────────────┴─────────────────────────────────────────────────────────┴───────────────────┤
│         [7] [2] [9] [_]         ┌───┐                  1  2  3      ENCRIPTAR ◉   SEÑUELO ◉  │
│      tu intento en curso        │ ↵ │ ◔ 4.2 s          4  5  6                               │
│                                 └───┘                  7  8  9  0   ⌫                        │
└──────────────────────────────────────────────────────────────────────────────────────────────┘
```

Leyenda del mockup:

- **Columna SALA (izquierda):** un chip por jugador con su color, mejor resultado (`●●●○`), número de intentos y estado: `🔒` tiene un intento encriptado activo, `⚠` disparó la alerta de 3 Famas, `✗` fue pillado con un Señuelo, `zZ` inactivo. En 8 jugadores la columna pasa a dos filas de chips compactos.
- **Módulo del Enigma (centro):** una entrada por intento, cronológico descendente, con borde izquierdo (`▌`) del color del autor. Columnas: autor, dígitos, resultado, bits aportados (`+2.1b`, sólo visible tras la revelación), timestamp, acciones (`⚑` pin en Equipos, `Sosp.` en Speed Race). Las entradas Encriptadas muestran `??` con candado y cuenta atrás. Los Señuelos corregidos muestran el falso tachado y la verdad al lado. Separadores de «ola» cada 10 s. Los intentos de 0 bits se colapsan.
- **Filtros:** `Todos / Sólo Famas / Jugador X`. El filtro nunca oculta la Pista Clave ni los Señuelos activos.
- **Mi Pizarra (derecha):** rejilla 10 × 4 personal (compartida en Equipos). `✓` confirmado, `~` presente sin posición, `✗` descartado. Marcado manual con un toque; toque largo para borrar. Sin autocompletado.
- **Teclado (abajo):** numérico, dígitos ya usados en el intento en curso se atenúan. El arco `◔` alrededor de `↵` es el reloj de 10 s. Fichas de Encriptar/Señuelo como toggles que se aplican al *siguiente* envío.

**Variante Equipos (3v3):** la columna SALA se divide en «EQUIPO» (arriba, chips completos) y «RIVALES» (abajo, sólo `●●○○` y contador de intentos). El Módulo muestra sólo intentos aliados; una pestaña «Rivales (cifrado)» muestra `▪▪▪▪ ●●` por entrada. La Pizarra pasa a ser compartida con marcas de color por autor.

### 3.3 Feedback sonoro diferenciado (MetaSounds)

Cada asiento de la sala (1–8) tiene una **posición espacial fija** en un semicírculo frente al jugador (−75° … +75° de azimut) y un **timbre propio**. El jugador siempre se oye a sí mismo al frente, un poco más alto y seco.

| Asiento | Azimut | Timbre (MetaSound Patch) |
|---|---|---|
| Tú | 0° | Piano eléctrico |
| J2 … J8 | ±25°, ±50°, ±75° alternados | Marimba, pluck sintético, campana tubular, kalimba, vibráfono, clavinet, flauta de pan |

| Evento | Diseño sonoro |
|---|---|
| Rival obtiene Picas | *Tick* corto en su timbre y azimut, una nota por Pica en escala pentatónica ascendente |
| Rival obtiene Famas | Acorde mayor en su timbre; 1 Fama = tríada grave, 2 = añade quinta alta, 3 = arpegio completo + reverb larga. **Se distingue cuántas Famas ha sacado sin mirar.** |
| Rival encripta | Chasquido de candado en su azimut, con filtro paso-bajo (suena «tapado») |
| Señuelo revelado | Glitch corto + acorde disminuido en el azimut del farolero |
| Alerta 3 Famas | *Stinger* global mono (rompe el espacio: es un evento de sala) + la música pierde el bajo |
| Deducción Relámpago | Barrido ascendente + campanas; el timbre del ganador se convierte en lead durante el resumen |
| Equipos | Aliados con reverb corta (cerca), rivales con reverb larga y filtro (lejos). Los Famas rivales suenan **detrás** del jugador (azimut 180° ± 30°) |

Implementación: un `MetaSound Source` por asiento (`MS_SeatVoice`) con entradas `Azimuth`, `Timbre (enum)`, `Famas (int)`, `Picas (int)`, `bAllied`, disparado desde `UPFAudioDirector` con *quantization* en Quartz al siguiente octavo, para que una ola de 6 intentos suene como un arpegio y no como ruido.

---

## 4. Arquitectura Técnica en Unreal Engine 5

### 4.1 Topología y responsabilidades

```
              ┌──────────────────── DEDICATED SERVER (autoridad) ────────────────────┐
              │  APFGameMode                                                          │
              │   · genera el código  · cola de intentos por tick  · candidatos       │
              │   · Pista Clave / combos · Muerte Sudada · desempates · AFK/reconexión│
              │  APFGameState (replicado a todos)                                     │
              │   · FPFGuessHistory (FastArray)  · fase/relojes  · marcadores         │
              │  APFTeamState ×2 (relevante sólo para su equipo)                      │
              │   · dígitos completos de aliados · Pizarra compartida · fichas        │
              │  APFPlayerState ×N (replicado a todos)                                │
              │   · mejor Famas/Picas · nº intentos · deadline · flags · puntos       │
              └───────────────────────────────────────────────────────────────────────┘
                     ▲ Server RPC (fiable)                 │ replicación / Client RPC
   Server_SubmitGuess(Guess, Flags)                        ▼
   Server_Suspect(Seq)                        ┌─────────────────────────┐
   Server_MarkBoard(Digit, Pos, Mark)         │ CLIENTE                 │
                                              │  UPFEnigmaViewModel     │
                                              │  UPFAudioDirector       │
                                              │  UMG / CommonUI         │
                                              └─────────────────────────┘
```

| Clase | Rol |
|---|---|
| `APFGameMode` | Único punto de validación. Recibe intentos en una cola, los resuelve **una vez por tick** ordenados por tiempo ajustado (§4.2). Mantiene el conjunto de candidatos, detecta `T_key`, gestiona relojes por jugador, Muerte Sudada, puntuación y casos borde. |
| `APFGameState` | `FPFGuessHistory` (historial global público), `ERoundPhase`, `RoundEndServerTime`, `SuddenDeathEndServerTime`, `AlertPlayerIndex`, `SecretLength`, `RoundIndex`, marcadores. `NetUpdateFrequency = 30`. |
| `APFTeamState` | Actor replicado con `IsNetRelevantFor` restringido a los miembros del equipo. Contiene los dígitos completos de los intentos aliados y la Pizarra compartida. En Speed Race no se instancia. |
| `APFPlayerState` | Estado público del jugador. Push Model. Historial privado real (`COND_OwnerOnly`). |
| `APFPlayerController` | RPCs de entrada. Predicción cosmética: al enviar, la entrada aparece en gris «enviando…» y se sustituye por la replicada. |
| `PFCodeMath` | Evaluación O(1) de intentos y filtrado del conjunto de candidatos. |

### 4.2 Server Authority: intentos simultáneos y race conditions

**El problema.** Dos jugadores envían el código correcto con 3 ms de diferencia. Sus paquetes pueden llegar en el mismo tick del servidor y, dentro de él, el orden de procesamiento de `UNetConnection` no es una garantía de justicia. Además, el jugador con 80 ms de ping envió *antes* que el de 15 ms aunque llegue después.

**La solución en tres capas:**

1. **Cola por tick.** Los RPC `Server_SubmitGuess` no resuelven nada: encolan `FPendingGuess` con `ServerReceiveTime` (reloj monotónico del servidor) y el RTT actual de la conexión. Al final del tick, `ResolvePendingGuesses()` ordena y procesa. Así, «mismo tick» se convierte en «orden por timestamp», no «orden de llegada del socket».
2. **Tiempo ajustado por latencia, con tope.** `AdjustedTime = ServerReceiveTime − clamp(RTT / 2, 0, 0.100)`. Compensa hasta 100 ms de latencia de ida (un jugador con 300 ms de ping no obtiene 150 ms de ventaja: eso sería explotable simulando lag). El RTT se toma de `UNetConnection::AvgLag` suavizado, nunca del cliente.
3. **Umbral de empate.** Si dos aciertos tienen `|AdjustedTime_a − AdjustedTime_b| < 2 ms`, es un **empate técnico**: ambos cobran el acierto completo y la UI lo celebra como «FOTO-FINISH». Por debajo de 2 ms la diferencia es ruido de red, y decidir por ruido se percibe como injusticia. Por encima, gana el menor `AdjustedTime`; el segundo recibe «Llegó tarde» (+50, la mitad).

El código secreto vive **sólo** en el servidor (`APFGameMode::SecretCode`). Nunca se replica ni se escribe en logs de cliente. Los clientes reciben resultados, no verdades comprobables.

### 4.3 Replicación del historial global

```cpp
// PFGameState.h

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EPFGuessFlags : uint8
{
    None          = 0,
    ResultHidden  = 1 << 0,  // Encriptar activo: Famas/Picas no enviados
    Decoy         = 1 << 1,  // Señuelo activo: Famas/Picas son los falsos
    DecoyRevealed = 1 << 2,  // Señuelo corregido: Famas/Picas ya son verdad, mostrar tachado
    Suspected     = 1 << 3,  // Alguien pulsó Sospechar
    KeyClue       = 1 << 4,  // Este intento dejó un único candidato
    Solved        = 1 << 5,
    TeamMasked    = 1 << 6,  // Equipos: PackedGuess = 0xFFFF para el equipo contrario
};
ENUM_CLASS_FLAGS(EPFGuessFlags);

USTRUCT()
struct FPFGuessEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY() int32  Seq = 0;              // id monótono, clave de UI y de Sospechar
    UPROPERTY() uint8  PlayerIndex = 0;      // índice en GameState->PlayerArray (estable por ronda)
    UPROPERTY() uint8  TeamIndex = 0xFF;
    UPROPERTY() uint16 PackedGuess = 0;      // 4-5 nibbles; 0xFFFF si TeamMasked
    UPROPERTY() uint8  Famas = 0;
    UPROPERTY() uint8  Picas = 0;
    UPROPERTY() uint8  Flags = 0;            // EPFGuessFlags
    UPROPERTY() uint8  InfoBitsX10 = 0;      // bits × 10, entero para ahorrar ancho de banda
    UPROPERTY() float  ServerTime = 0.f;     // GetServerWorldTimeSeconds() al resolver
    UPROPERTY() float  RevealServerTime = 0.f; // cuándo termina Encriptar/Señuelo (cliente dibuja la cuenta atrás)

    void PostReplicatedAdd(const struct FPFGuessHistory& Owner);
    void PostReplicatedChange(const struct FPFGuessHistory& Owner);
};
```

> **Regla de "vista pública" (aprendida en el sandbox).** Un señuelo debe ser indistinguible de un intento
> honesto hasta que se revela; de lo contrario, un cliente modificado (o simplemente atento) lo detecta.
> Lo que el servidor replica pasa siempre por `PF::MakePublic`, que garantiza:
> - el bit `Decoy` **nunca** se replica (solo `DecoyRevealed`, ya con la verdad);
> - `RevealServerTime` solo se replica para *Encriptar* (la cuenta atrás de un señuelo es secreta);
> - `InfoBitsX10` de un señuelo activo son los bits que *aparenta* su resultado falso, y el jugador
>   cobra provisionalmente esos puntos (se corrige la diferencia al revelar) para que el marcador no delate;
> - `KeyClue` no se replica mientras la ronda está activa (el cronómetro del Relámpago es invisible);
>   se publica al cerrar la ronda para el resumen.

```cpp

USTRUCT()
struct FPFGuessHistory : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY() TArray<FPFGuessEntry> Items;
    class APFGameState* OwnerState = nullptr;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& Params)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FPFGuessEntry, FPFGuessHistory>(Items, Params, *this);
    }
};

template<>
struct TStructOpsTypeTraits<FPFGuessHistory> : public TStructOpsTypeTraitsBase2<FPFGuessHistory>
{
    enum { WithNetDeltaSerializer = true };
};

UCLASS()
class APFGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated) FPFGuessHistory GuessHistory;

    UPROPERTY(ReplicatedUsing = OnRep_Phase) EPFRoundPhase Phase = EPFRoundPhase::Lobby;
    UPROPERTY(Replicated) uint8  RoundIndex = 0;
    UPROPERTY(Replicated) uint8  SecretLength = 4;
    UPROPERTY(Replicated) float  RoundEndServerTime = 0.f;
    UPROPERTY(Replicated) float  SuddenDeathEndServerTime = 0.f;   // 0 = inactiva
    UPROPERTY(ReplicatedUsing = OnRep_Alert) uint8 AlertPlayerIndex = 0xFF;
    UPROPERTY(Replicated) TArray<int32> TeamScores;

    // Servidor
    FPFGuessEntry& AddEntry(const FPFGuessEntry& E) { auto& R = GuessHistory.Items.Add_GetRef(E); GuessHistory.MarkItemDirty(R); return R; }
    void UpdateEntry(FPFGuessEntry& E)               { GuessHistory.MarkItemDirty(E); }
};
```

**Por qué FastArray y no `TArray<FPFGuessEntry>` con `UPROPERTY(Replicated)`:** un `TArray` replicado normal reenvía el array completo cuando cambia cualquier elemento. Con 8 jugadores y ~40 entradas por ronda, cada revelación de un Encriptar reenviaría 40 structs. `FFastArraySerializer` envía **sólo el elemento modificado** (delta por `ReplicationID`), y dispara `PostReplicatedAdd/Change` en cliente para animar la entrada nueva o la corrección *in situ*.

**Ocultación real, no cosmética:** mientras `ResultHidden` está activo, `Famas` y `Picas` se replican a **cero**; el servidor escribe los valores reales al revelar. Un cliente modificado no puede leer lo que no se le envía. El dueño recibe su resultado real por `Client_GuessResult` (RPC al owner).

**Equipos y dígitos cifrados:** `GameState` replica `PackedGuess = 0xFFFF` con `TeamMasked` para todos. Los dígitos reales de los aliados viajan en `APFTeamState`, cuyo `IsNetRelevantFor()` devuelve `true` sólo para conexiones cuyo `PlayerState->TeamIndex` coincide. El cliente fusiona ambas fuentes por `Seq`.

### 4.4 Sincronización de relojes

Todos los deadlines se expresan en **tiempo de servidor** (`AGameStateBase::GetServerWorldTimeSeconds()`, que UE sincroniza con `ServerWorldTimeSecondsDelta`). El cliente dibuja el arco del reloj restando su estimación local; nunca cuenta hacia atrás por su cuenta. El servidor aplica un margen de gracia de **150 ms** al validar deadlines (`ServerReceiveTime ≤ Deadline + 0.150`) para no castigar a nadie por jitter.

### 4.5 Presupuesto

| Operación | Coste |
|---|---|
| `Evaluate()` | < 50 ns |
| Filtrar 5 040 candidatos tras un intento | ≈ 0.1 ms (30 240 con 5 dígitos: ≈ 0.6 ms) |
| `ResolvePendingGuesses` con 8 intentos en un tick | < 1 ms |
| Ancho de banda por cliente (8 jugadores, ronda activa) | < 3 KB/s |
| Tick de servidor | 30 Hz (suficiente: la granularidad de justicia la da el timestamp, no el tick) |

---

## 5. Casos Borde y Balanceo

### 5.1 Desempates

| Situación | Resolución |
|---|---|
| Dos aciertos, `Δ AdjustedTime ≥ 2 ms` | Gana el menor. El segundo: «Llegó tarde» +50. |
| Dos aciertos, `Δ < 2 ms` | **Foto-finish**: ambos +100. Sin combo Relámpago para ninguno (evita reclamar el ⚡ por azar). |
| Acierto e intento no-acierto en el mismo tick | Se procesan en orden de `AdjustedTime`. Si el no-acierto iba antes y fue la Pista Clave, se le acredita **aunque la ronda termine en el mismo tick**. |
| Muerte Sudada expira sin acierto | Más Famas en mejor intento → menos intentos → menor `ServerTime` del mejor intento. |
| Acierto recibido tras `RoundEndServerTime` pero dentro del margen de 150 ms | Se acepta y se resuelve con el resto. Fuera del margen: se descarta con `Client_GuessRejected(Reason::RoundOver)`. |
| Intento recibido con el reloj del jugador ya expirado (> 150 ms) | Se registra como *Paso* y el intento se descarta. |

### 5.2 Jugadores inactivos

| Estado | Disparador | Consecuencia |
|---|---|---|
| Pasivo | 1 reloj expirado | −5 pts. Chip con reloj atenuado. |
| **Inactivo** (`zZ`) | 2 relojes consecutivos expirados sin ninguna otra acción (marcar Pizarra o Sospechar cuentan como acción) | Sus relojes dejan de correr (no acumula −5). Excluido de los desempates de Muerte Sudada. En Equipos, sus fichas pasan al equipo. |
| Expulsado | Inactivo durante una ronda completa | Al final de la ronda se le mueve a espectador; su asiento se libera para *late join* entre rondas. Conserva los puntos ganados. |

Un solo intento, marca o sospecha devuelve al jugador de Inactivo a Activo al instante, sin penalización adicional: el objetivo es proteger el ritmo de la sala, no castigar una pausa.

### 5.3 Desconexiones

- **Gracia de reconexión: 20 s.** El `APFPlayerState` se conserva en `InactivePlayerArray` (`InactivePlayerStateLifeSpan = 20`). Si reconecta, recupera asiento, puntos, historial privado y fichas; el Módulo del Enigma se reconstruye desde `GuessHistory` (que está en `GameState`, así que la reconexión no cuesta nada al servidor).
- **Durante la gracia** el jugador cuenta como Inactivo (sin −5). Su chip muestra un icono de conexión.
- **Tras 20 s** se aplica la regla de Expulsado.
- **Umbral de sala:** si quedan **< 2 jugadores activos** (o un equipo entero desconectado), la ronda termina de inmediato con la puntuación acumulada; la partida se da por concluida si no hay rival. Con ≥ 2 activos la ronda continúa: el enigma no depende del número de jugadores.
- **Late join:** permitido entre rondas hasta completar la sala. El recién llegado entra con 0 puntos y no ve el historial de rondas anteriores (no lo necesita: cada ronda es un código nuevo).
- **Desconexión del que acaba de acertar:** el acierto ya está resuelto en el servidor; se le acreditan los puntos aunque no vea el resumen.

### 5.4 Balanceo

| Palanca | Regla | Motivo |
|---|---|---|
| Longitud del código | 3–5 jugadores: 4 dígitos · 6–8: 5 dígitos | Con 8 jugadores y 4 dígitos, la información pública agota los candidatos en ~4 olas; la ronda se decide por reflejos, no por lógica. Con 5 dígitos el conjunto inicial es 6× mayor. |
| Reloj de intento | 10 s base · 6 s en Muerte Sudada · +2 s en salas «Relax» | 10 s permite una deducción real; menos convierte el juego en teclear. |
| Fichas | 1 Encriptar + 1 Señuelo por ronda | Dos Señuelos por jugador ya hacen ilegible el tablero en salas de 8. |
| Puntos por bit | 10 | Calibrado para que un buen sondeador saque ≈ 60–80 pts por ronda frente a 100 del acierto. Si los datos muestran que nadie sondea, subir a 12; si nadie cierra, bajar a 8. |
| Umbral de alerta | 3 Famas (N−1) | Se puede subir a «N−1 Famas y ≤ 3 candidatos» en salas competitivas para evitar alertas prematuras. |
| Handicap de líder | En la ronda 3, quien lidere la partida por > 80 pts tiene reloj de 8 s | Mantiene viva la remontada sin tocar la lógica. |
| Bits en Equipos | Se acreditan al equipo, no al individuo | Evita que un aliado sondee «para sí». |

Todas las palancas viven en `UPFTuningDataAsset` y se recargan en caliente en el servidor.

---

## 6. Pseudo-código C++ — Gestión de intentos simultáneos en el servidor

### 6.1 Evaluación y conjunto de candidatos

```cpp
// PFCodeMath.h
namespace PFCodeMath
{
    struct FResult { uint8 Famas = 0, Picas = 0; };

    FORCEINLINE uint16 MaskOf(uint16 Packed, int32 Len)
    {
        uint16 M = 0;
        for (int32 i = 0; i < Len; ++i) M |= uint16(1u << ((Packed >> (4 * i)) & 0xF));
        return M;
    }

    FORCEINLINE FResult Evaluate(uint16 Code, uint16 CodeMask, uint16 Guess, int32 Len)
    {
        FResult R;
        const uint16 Diff = Code ^ Guess;
        for (int32 i = 0; i < Len; ++i) R.Famas += (((Diff >> (4 * i)) & 0xF) == 0);
        R.Picas = uint8(FMath::CountBits(CodeMask & MaskOf(Guess, Len))) - R.Famas;
        return R;
    }

    // Conjunto de códigos aún consistentes con la información pública verdadera.
    struct FCandidateSet
    {
        TArray<uint16> Codes;   // 5 040 (4 dígitos) o 30 240 (5 dígitos)
        int32 Len = 4;

        void Reset(int32 InLen);                        // genera todas las permutaciones

        // Elimina los códigos que no habrían producido exactamente (Famas, Picas) para Guess.
        // Devuelve bits de información: log2(antes / después).
        float Filter(uint16 Guess, FResult Observed)
        {
            const int32 Before = Codes.Num();
            Codes.RemoveAllSwap([&](uint16 C)
            {
                const FResult R = Evaluate(C, MaskOf(C, Len), Guess, Len);
                return R.Famas != Observed.Famas || R.Picas != Observed.Picas;
            });
            return Before > 0 && Codes.Num() > 0 ? FMath::Log2(float(Before) / float(Codes.Num())) : 0.f;
        }
    };
}
```

### 6.2 Cola de intentos y resolución por tick

```cpp
// PFGameMode.h
struct FPendingGuess
{
    TWeakObjectPtr<APFPlayerController> PC;
    uint16 Guess = 0;
    uint8  Flags = 0;             // EPFGuessFlags solicitados por el cliente (Encriptar / Señuelo)
    uint8  DecoyFamas = 0, DecoyPicas = 0;
    double ServerReceiveTime = 0; // FPlatformTime::Seconds() en el RPC
    double AdjustedTime = 0;      // ServerReceiveTime - clamp(RTT/2, 0, 0.1)
};

UCLASS()
class APFGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    void Tick(float Dt) override;
    void EnqueueGuess(APFPlayerController* PC, uint16 Guess, uint8 Flags, uint8 DecoyF, uint8 DecoyP);

private:
    // Estado de ronda (solo servidor)
    uint16 SecretCode = 0, SecretMask = 0;
    int32  SecretLength = 4;
    PFCodeMath::FCandidateSet Candidates;
    double KeyClueServerTime = -1.0;     // instante en que Candidates.Num() == 1
    bool   bRoundSolved = false;
    double FirstSolveAdjustedTime = 0.0;
    int32  NextSeq = 1;

    TArray<FPendingGuess> Pending;

    void ResolvePendingGuesses();
    void ResolveOne(FPendingGuess& P, double Now);
    void ScheduleReveal(int32 Seq, float Delay);
    void OnPlayerReached3Famas(APFPlayerState* PS);
    void EndRound(EPFRoundEndReason Reason);

    static constexpr double MaxLatencyCompensation = 0.100;  // s
    static constexpr double PhotoFinishEpsilon     = 0.002;  // s
    static constexpr double DeadlineGrace          = 0.150;  // s
};
```

```cpp
// PFPlayerController.cpp — el RPC sólo encola. Nada se decide aquí.
void APFPlayerController::Server_SubmitGuess_Implementation(uint16 Guess, uint8 Flags, uint8 DecoyF, uint8 DecoyP)
{
    if (APFGameMode* GM = GetWorld()->GetAuthGameMode<APFGameMode>())
    {
        GM->EnqueueGuess(this, Guess, Flags, DecoyF, DecoyP);
    }
}

bool APFPlayerController::Server_SubmitGuess_Validate(uint16 Guess, uint8 Flags, uint8, uint8)
{
    // Un cliente que envíe basura estructural se desconecta (RPC validation).
    return PFCodeMath::IsValidGuess(Guess, GetGameState<APFGameState>()->SecretLength)
        && (Flags & ~uint8(EPFGuessFlags::ResultHidden | EPFGuessFlags::Decoy)) == 0;
}
```

```cpp
// PFGameMode.cpp
void APFGameMode::EnqueueGuess(APFPlayerController* PC, uint16 Guess, uint8 Flags, uint8 DecoyF, uint8 DecoyP)
{
    const double Now = FPlatformTime::Seconds();

    // RTT medido por el servidor (nunca confiar en un valor enviado por el cliente).
    double HalfRtt = 0.0;
    if (const UNetConnection* Conn = PC->GetNetConnection())
    {
        HalfRtt = FMath::Clamp(double(Conn->AvgLag) * 0.5, 0.0, MaxLatencyCompensation);
    }

    FPendingGuess P;
    P.PC = PC; P.Guess = Guess; P.Flags = Flags; P.DecoyFamas = DecoyF; P.DecoyPicas = DecoyP;
    P.ServerReceiveTime = Now;
    P.AdjustedTime      = Now - HalfRtt;
    Pending.Add(MoveTemp(P));
}

void APFGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    if (Pending.Num() > 0) ResolvePendingGuesses();
    // ... relojes por jugador, Muerte Sudada, reveals programados (omitido)
}

void APFGameMode::ResolvePendingGuesses()
{
    // 1) Orden justo: por tiempo ajustado, no por orden de llegada al socket.
    Pending.Sort([](const FPendingGuess& A, const FPendingGuess& B) { return A.AdjustedTime < B.AdjustedTime; });

    const double Now = FPlatformTime::Seconds();
    for (FPendingGuess& P : Pending)
    {
        if (!P.PC.IsValid()) continue;
        ResolveOne(P, Now);
    }
    Pending.Reset();

    if (bRoundSolved) EndRound(EPFRoundEndReason::Solved);
}

void APFGameMode::ResolveOne(FPendingGuess& P, double Now)
{
    APFPlayerController* PC = P.PC.Get();
    APFPlayerState* PS = PC->GetPlayerState<APFPlayerState>();
    APFGameState*   GS = GetGameState<APFGameState>();
    if (!PS || GS->Phase == EPFRoundPhase::Reward) return;

    // 2) Reloj del jugador (con margen de jitter). Si expiró, es un Paso, no un intento.
    if (P.ServerReceiveTime > PS->AttemptDeadlineServerTime + DeadlineGrace)
    {
        PS->RegisterPass();
        PC->Client_GuessRejected(EPFRejectReason::DeadlineExpired);
        return;
    }

    // 3) Ronda ya resuelta en un tick anterior → tarde, salvo foto-finish (mismo tick, ver abajo).
    if (bRoundSolved && (P.AdjustedTime - FirstSolveAdjustedTime) >= PhotoFinishEpsilon)
    {
        PC->Client_GuessRejected(EPFRejectReason::RoundOver);
        return;
    }

    // 4) Fichas: sólo se consumen si están disponibles; si no, el intento sigue como normal.
    uint8 Flags = P.Flags;
    if ((Flags & uint8(EPFGuessFlags::ResultHidden)) && !PS->ConsumeToken(EPFToken::Encrypt)) Flags &= ~uint8(EPFGuessFlags::ResultHidden);
    if ((Flags & uint8(EPFGuessFlags::Decoy))        && !PS->ConsumeToken(EPFToken::Decoy))   Flags &= ~uint8(EPFGuessFlags::Decoy);
    if ((Flags & uint8(EPFGuessFlags::Decoy)) && P.DecoyFamas >= SecretLength) Flags &= ~uint8(EPFGuessFlags::Decoy); // no se puede fingir 4 Famas

    // 5) Verdad autoritativa. El código nunca sale de esta función.
    const PFCodeMath::FResult Truth = PFCodeMath::Evaluate(SecretCode, SecretMask, P.Guess, SecretLength);
    const bool bSolved = (Truth.Famas == SecretLength);

    // 6) Información aportada. El conjunto de candidatos SÓLO se filtra con verdad pública:
    //    Encriptar y Señuelo posponen el filtrado hasta la revelación (ScheduleReveal lo hace).
    float Bits = 0.f;
    const bool bPublicTruthNow = (Flags & uint8(EPFGuessFlags::ResultHidden | EPFGuessFlags::Decoy)) == 0;
    if (bPublicTruthNow && !bSolved)
    {
        Bits = Candidates.Filter(P.Guess, Truth);
        if (Candidates.Codes.Num() == 1 && KeyClueServerTime < 0.0)
        {
            KeyClueServerTime = P.AdjustedTime;      // ← T_key: el código queda lógicamente determinado
            Flags |= uint8(EPFGuessFlags::KeyClue);
            PS->AddScore(EPFScore::KeyClue, 30);
        }
        PS->AddScore(EPFScore::InfoBits, FMath::RoundToInt(Bits * 10.f));
    }

    // 7) Entrada pública. Lo que NO debe verse, no se envía.
    FPFGuessEntry E;
    E.Seq = NextSeq++;
    E.PlayerIndex = PS->SeatIndex;
    E.TeamIndex   = PS->TeamIndex;
    E.PackedGuess = (GS->bTeamMode) ? 0xFFFF : P.Guess;    // Equipos: dígitos via APFTeamState
    E.Flags       = Flags | (GS->bTeamMode ? uint8(EPFGuessFlags::TeamMasked) : 0);
    E.InfoBitsX10 = uint8(FMath::Min(255, FMath::RoundToInt(Bits * 10.f)));
    E.ServerTime  = GS->GetServerWorldTimeSeconds();

    if (Flags & uint8(EPFGuessFlags::ResultHidden))      { E.Famas = 0; E.Picas = 0; E.RevealServerTime = E.ServerTime + 5.f; ScheduleReveal(E.Seq, 5.f); }
    else if (Flags & uint8(EPFGuessFlags::Decoy))        { E.Famas = P.DecoyFamas; E.Picas = P.DecoyPicas; E.RevealServerTime = E.ServerTime + 8.f; ScheduleReveal(E.Seq, 8.f); }
    else                                                 { E.Famas = Truth.Famas; E.Picas = Truth.Picas; }

    GS->AddEntry(E);
    if (GS->bTeamMode) GetTeamState(PS->TeamIndex)->AddAlliedDigits(E.Seq, P.Guess);

    // 8) El dueño siempre recibe la verdad (RPC al owner). Historial privado.
    PS->RecordPrivateAttempt(P.Guess, Truth, E.Seq);
    PC->Client_GuessResult(E.Seq, P.Guess, Truth.Famas, Truth.Picas);
    PS->ResetAttemptClock(GS->SuddenDeathEndServerTime > 0.f ? 6.f : 10.f);

    // 9) Alerta global de cercanía.
    if (Truth.Famas == SecretLength - 1 && GS->AlertPlayerIndex == 0xFF && !bSolved)
    {
        OnPlayerReached3Famas(PS);   // AlertPlayerIndex, SuddenDeathEndServerTime = now + 20, relojes a 6 s
    }

    // 10) Acierto: primero, foto-finish o tarde.
    if (bSolved)
    {
        if (!bRoundSolved)
        {
            bRoundSolved = true;
            FirstSolveAdjustedTime = P.AdjustedTime;
            PS->AddScore(EPFScore::Solve, 100);

            // Combo por velocidad de deducción, sólo si el código ya estaba determinado.
            if (KeyClueServerTime >= 0.0)
            {
                const double Delta = P.AdjustedTime - KeyClueServerTime;
                if      (Delta <= 3.0) PS->AddScore(EPFScore::LightningDeduction, 50);
                else if (Delta <= 6.0) PS->AddScore(EPFScore::FastDeduction, 25);
                PC->Client_ShowCombo(float(Delta));
            }
            else
            {
                PC->Client_ShowIntuition();      // acertó con >1 candidato: «Golpe de intuición»
            }
        }
        else
        {
            // Mismo tick y Δ < 2 ms: foto-finish. Ambos cobran; ninguno cobra ⚡.
            PS->AddScore(EPFScore::Solve, 100);
            GS->MarkPhotoFinish(FirstSolveSeq, E.Seq);
        }
        GS->GuessHistory.Items.Last().Flags |= uint8(EPFGuessFlags::Solved);
        GS->UpdateEntry(GS->GuessHistory.Items.Last());
    }
}
```

### 6.3 Revelación diferida y filtrado retrasado

```cpp
void APFGameMode::ScheduleReveal(int32 Seq, float Delay)
{
    FTimerHandle H;
    GetWorldTimerManager().SetTimer(H, [this, Seq]()
    {
        APFGameState* GS = GetGameState<APFGameState>();
        FPFGuessEntry* E = GS->GuessHistory.Items.FindByPredicate([Seq](const FPFGuessEntry& X) { return X.Seq == Seq; });
        if (!E) return;

        const uint16 Guess = PrivateGuessBySeq[Seq];              // el servidor guarda los dígitos reales
        const PFCodeMath::FResult Truth = PFCodeMath::Evaluate(SecretCode, SecretMask, Guess, SecretLength);

        const bool bWasDecoy = (E->Flags & uint8(EPFGuessFlags::Decoy)) != 0;
        E->Famas = Truth.Famas; E->Picas = Truth.Picas;
        E->Flags &= ~uint8(EPFGuessFlags::ResultHidden | EPFGuessFlags::Decoy);
        if (bWasDecoy) E->Flags |= uint8(EPFGuessFlags::DecoyRevealed);

        // Ahora la verdad es pública: filtra candidatos y acredita bits al autor (con retraso).
        const float Bits = Candidates.Filter(Guess, Truth);
        E->InfoBitsX10 = uint8(FMath::Min(255, FMath::RoundToInt(Bits * 10.f)));
        if (APFPlayerState* PS = FindPlayerBySeat(E->PlayerIndex)) PS->AddScore(EPFScore::InfoBits, FMath::RoundToInt(Bits * 10.f));
        if (Candidates.Codes.Num() == 1 && KeyClueServerTime < 0.0)
        {
            KeyClueServerTime = FPlatformTime::Seconds();          // T_key se fija en la revelación, no en el envío
            E->Flags |= uint8(EPFGuessFlags::KeyClue);
        }

        GS->UpdateEntry(*E);   // FastArray: sólo viaja este elemento
    }, Delay, false);
}

void APFGameMode::Server_Suspect(APFPlayerController* Caller, int32 Seq)
{
    FPFGuessEntry* E = /* buscar por Seq */;
    if (!E || E->PlayerIndex == Caller->GetPlayerState<APFPlayerState>()->SeatIndex) return;
    if (E->Flags & uint8(EPFGuessFlags::Suspected)) return;         // una sospecha por entrada
    E->Flags |= uint8(EPFGuessFlags::Suspected);

    if (E->Flags & uint8(EPFGuessFlags::Decoy))
    {
        GetWorldTimerManager().ClearTimer(RevealTimers[Seq]);
        RevealNow(Seq);                                             // misma ruta que ScheduleReveal, sin espera
        FindPlayerBySeat(E->PlayerIndex)->AddScore(EPFScore::DecoyCaught, -30);
        Caller->GetPlayerState<APFPlayerState>()->AddScore(EPFScore::SuspicionHit, +15);
    }
    else
    {
        Caller->GetPlayerState<APFPlayerState>()->AddScore(EPFScore::SuspicionMiss, -10);
    }
    GetGameState<APFGameState>()->UpdateEntry(*E);
}
```

**Detalles que importan en producción (no en el pseudo-código):**

- `Candidates.Filter` con 30 240 códigos tarda ≈ 0.6 ms; si el tick del servidor va justo, se mueve a un `FAsyncTask` y `T_key` se fija con el timestamp del intento, no con el de finalización de la tarea.
- `PrivateGuessBySeq` es un `TMap<int32, uint16>` sólo servidor; se limpia al final de la ronda.
- Todos los `AddScore` marcan `Push Model` en `APFPlayerState::Score`.
- `Client_GuessResult` es `Reliable`; `Client_ShowCombo` es `Unreliable` (cosmético).

---

## 7. Post-partida y progresión (breve)

El resumen de partida (12 s) muestra tres columnas: **Aciertos**, **Bits aportados**, **Faroles**. Títulos dinámicos por perfil: *El Sondeador* (más bits de la partida), *El Cerrador* (2+ aciertos), *Relámpago* (Deducción ≤ 1.5 s), *Detector* (2 Señuelos pillados), *Ilusionista* (Señuelo que engañó a 3 rivales), *Paciente* (ganó una ronda sin gastar fichas). Progresión cosmética únicamente (timbres de asiento, skins de teclado, marcos), obtenida jugando. Sin loot con dinero real: en un juego de lógica competitivo, la percepción de justicia es el producto.

---

## 8. Roadmap MVP (10 semanas)

| Semana | Entregable |
|---|---|
| 1–2 | `PFCodeMath` + `FCandidateSet` con tests · GameMode/GameState/PlayerState · cola por tick y desempates · sala de 8 en dedicated local |
| 3–4 | `FPFGuessHistory` FastArray · Módulo del Enigma UMG (densidad adaptativa) · relojes por jugador sincronizados |
| 5 | Encriptar / Señuelo / Sospechar · revelación diferida · Pista Clave y combos |
| 6 | Alerta 3 Famas · Muerte Sudada · MetaSounds por asiento con Quartz |
| 7 | Modo Equipos: `APFTeamState`, relevancia, Pizarra compartida, pings |
| 8 | AFK, reconexión (20 s), late join, umbral de sala · telemetría |
| 9 | Resumen de partida, títulos, puntuación de 3 rondas · pase de tuning en `UPFTuningDataAsset` |
| 10 | Playtest cerrado (150 jugadores, salas de 3/5/8) · medir ratio sondeo/cierre · ajustar puntos por bit |
