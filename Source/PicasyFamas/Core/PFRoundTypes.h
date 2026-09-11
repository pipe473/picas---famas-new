// Tipos compartidos del motor de ronda. C++ puro, sin dependencias de Unreal.
#pragma once

#include <cstdint>
#include "PFCodeMath.h"

namespace PF
{
	constexpr int32_t kMaxPlayers = 8;
	constexpr uint8_t kNoPlayer   = 0xFF;
	constexpr uint8_t kNoTeam     = 0xFF;

	// Flags de una entrada del Modulo del Enigma (bitmask en uint8_t).
	namespace GuessFlags
	{
		constexpr uint8_t None          = 0;
		constexpr uint8_t ResultHidden  = 1 << 0;  // Encriptar activo: Famas/Picas publicos = 0 hasta revelar
		constexpr uint8_t Decoy         = 1 << 1;  // Senuelo activo: Famas/Picas publicos son falsos
		constexpr uint8_t DecoyRevealed = 1 << 2;  // Senuelo corregido: ya son verdad; UI muestra el falso tachado
		constexpr uint8_t Suspected     = 1 << 3;  // Alguien pulso Sospechar sobre esta entrada
		constexpr uint8_t KeyClue       = 1 << 4;  // Este intento dejo un unico candidato
		constexpr uint8_t Solved        = 1 << 5;  // Acierto
		constexpr uint8_t PhotoFinish   = 1 << 6;  // Acierto empatado (< 2 ms)
		constexpr uint8_t LateSolve     = 1 << 7;  // Acierto tardio (misma pasada, >= 2 ms)
	}

	// Flags que el cliente puede solicitar al enviar. El resto los decide el servidor.
	constexpr uint8_t kClientRequestableFlags = GuessFlags::ResultHidden | GuessFlags::Decoy;

	enum class ERejectReason : uint8_t
	{
		None = 0,
		RoundNotActive,
		UnknownPlayer,
		PlayerDisconnected,
		InvalidGuess,
		TooFast,           // por debajo del intervalo minimo entre intentos
		DeadlineExpired,   // llego fuera del reloj + margen; se registra como Paso
		RoundOver,         // la ronda ya estaba resuelta en una pasada anterior
		HistoryFull,
		NotYourTurn,       // modo por turnos: solo el jugador con el turno puede enviar
	};

	// Como se reparten los intentos dentro de la ronda.
	enum class ETurnMode : uint8_t
	{
		Simultaneous = 0,  // Speed Race: todos a la vez, un reloj por jugador
		SeatOrder,         // por turnos, en orden de asiento; el reloj solo corre para quien tiene el turno
		RandomOrder,       // por turnos, orden barajado al empezar cada ronda (determinista por semilla)
	};

	enum class EScoreReason : uint8_t
	{
		Solve = 0,
		LateSolve,
		PositionWin,
		LightningDeduction,
		FastDeduction,
		InfoBits,
		KeyClue,
		DecoyEffective,
		DecoyCaught,
		SuspicionHit,
		SuspicionMiss,
		Pass,
	};

	enum class ERoundEndReason : uint8_t
	{
		None = 0,
		Solved,
		SuddenDeathExpired,
		TimeCap,
		NotEnoughPlayers,
		Aborted,
	};

	struct FScoringConfig
	{
		int32_t Solve              = 100;
		int32_t LateSolve          = 50;
		int32_t PositionWin        = 40;   // ronda cerrada sin acierto (Muerte Sudada / cap): mejor posicion
		int32_t LightningDeduction = 50;
		int32_t FastDeduction      = 25;
		int32_t PointsPerBit       = 10;
		int32_t KeyClue            = 30;
		int32_t DecoyEffective     = 15;   // por rival enganado
		int32_t DecoyEffectiveMaxTargets = 0;   // tope de rivales que puntuan por senuelo (0 = sin tope)
		int32_t DecoyCaught        = -30;
		int32_t SuspicionHit       = 15;
		int32_t SuspicionMiss      = -10;
		int32_t Pass               = -5;
		double  LightningWindow    = 3.0;  // s desde la Pista Clave
		double  FastWindow         = 6.0;
	};

	// Relojes: cualquier valor <= 0 desactiva ese reloj.
	//   AttemptSeconds <= 0      -> "tiempo libre": nadie tiene reloj de intento; no hay Pasos por expiracion
	//                               (ni Inactivos por ellos) y, por turnos, el turno dura hasta que su dueno tira.
	//   SuddenDeathSeconds <= 0  -> la alerta de N-1 Famas se emite pero sin cuenta atras ni relojes recortados.
	//   RoundCapSeconds <= 0     -> la ronda no tiene tope: solo acaba por acierto (o si todos se caen).
	// MakeFreeTime() aplica los tres a la vez, que es lo que espera una mesa "sin prisas".
	struct FRoundConfig
	{
		int32_t CodeLength                = 4;
		double  AttemptSeconds            = 10.0;
		double  SuddenDeathAttemptSeconds = 6.0;
		double  SuddenDeathSeconds        = 20.0;
		double  RoundCapSeconds           = 120.0;
		double  MinAttemptInterval        = 0.35;   // anti-spam
		double  MaxLatencyCompensation    = 0.100;  // tope de RTT/2 descontado
		double  PhotoFinishEpsilon        = 0.002;  // < 2 ms = empate tecnico
		double  DeadlineGrace             = 0.150;  // jitter tolerado al validar relojes
		double  HideSeconds               = 5.0;    // Encriptar
		double  DecoySeconds              = 8.0;    // Senuelo
		double  ReconnectGraceSeconds     = 20.0;
		uint8_t PassesUntilInactive       = 2;
		bool    bTeamMode                 = false;
		ETurnMode TurnMode                = ETurnMode::Simultaneous;
		FScoringConfig Scoring;

		bool HasAttemptClock() const     { return AttemptSeconds > 0.0; }
		bool HasSuddenDeathTimer() const { return SuddenDeathSeconds > 0.0; }
		bool HasRoundCap() const         { return RoundCapSeconds > 0.0; }
		bool IsFreeTime() const          { return !HasAttemptClock(); }

		// Misma configuracion sin ningun reloj: tiempo libre entre tiro y tiro.
		FRoundConfig MakeFreeTime() const
		{
			FRoundConfig C = *this;
			C.AttemptSeconds = 0.0;
			C.SuddenDeathSeconds = 0.0;
			C.RoundCapSeconds = 0.0;
			return C;
		}
	};

	struct FPlayerSlot
	{
		bool    bPresent          = false;
		bool    bConnected        = false;
		bool    bInactive         = false;   // relojes parados; excluido de desempates
		bool    bDropped          = false;   // supero la gracia de reconexion; liberar asiento al fin de ronda
		bool    bHasEncryptToken  = false;
		bool    bHasDecoyToken    = false;
		uint8_t TeamIndex         = kNoTeam;
		uint8_t BestFamas         = 0;
		uint8_t BestPicas         = 0;       // Picas del intento con mejor (Famas, Famas+Picas)
		uint8_t Attempts          = 0;
		uint8_t ConsecutivePasses = 0;
		int32_t RoundScore        = 0;
		double  AttemptDeadline   = 0.0;     // 0 = reloj parado
		double  LastAttemptTime   = -1.0e9;
		double  BestAttemptTime   = 0.0;     // para desempates
		double  DisconnectTime    = 0.0;
	};

	struct FGuessEntry
	{
		int32_t    Seq            = 0;
		uint8_t    Player         = kNoPlayer;
		uint8_t    Team           = kNoTeam;
		PackedCode Guess          = 0;       // digitos reales (el adaptador decide si los enmascara)
		uint8_t    Famas          = 0;       // valores PUBLICOS (0 si oculto, falsos si senuelo)
		uint8_t    Picas          = 0;
		uint8_t    TrueFamas      = 0;       // verdad, solo servidor
		uint8_t    TruePicas      = 0;
		uint8_t    Flags          = GuessFlags::None;
		uint8_t    InfoBitsX10    = 0;       // para un senuelo activo: bits que "aparenta" (calculados con el resultado falso)
		double     ServerTime     = 0.0;
		double     RevealTime     = 0.0;     // > 0 mientras hay una revelacion pendiente
		int32_t    ProvisionalInfoScore = 0; // solo servidor: puntos de bits ya acreditados a un senuelo (se corrigen al revelar)
	};

	// ---- Vista publica: lo UNICO que puede salir del servidor hacia los clientes ----
	// Mientras la ronda esta activa, ni el bit Decoy ni el bit KeyClue son publicos: el primero delataria
	// el farol y el segundo revelaria que el codigo ya esta determinado (el cronometro del combo es invisible).
	inline uint8_t PublicFlags(uint8_t Flags, bool bRoundActive)
	{
		uint8_t F = static_cast<uint8_t>(Flags & ~GuessFlags::Decoy);
		if (bRoundActive) F = static_cast<uint8_t>(F & ~GuessFlags::KeyClue);
		return F;
	}

	// Solo un Encriptar muestra su cuenta atras. La revelacion pendiente de un senuelo es secreta.
	inline double PublicRevealTime(const FGuessEntry& E)
	{
		return (E.Flags & GuessFlags::ResultHidden) ? E.RevealTime : 0.0;
	}

	struct FPublicEntry
	{
		int32_t    Seq         = 0;
		uint8_t    Player      = kNoPlayer;
		uint8_t    Team        = kNoTeam;
		PackedCode Guess       = 0;
		uint8_t    Famas       = 0;
		uint8_t    Picas       = 0;
		uint8_t    Flags       = GuessFlags::None;
		uint8_t    InfoBitsX10 = 0;
		double     ServerTime  = 0.0;
		double     RevealTime  = 0.0;
	};

	inline FPublicEntry MakePublic(const FGuessEntry& E, bool bRoundActive, bool bMaskDigits = false)
	{
		FPublicEntry P;
		P.Seq = E.Seq; P.Player = E.Player; P.Team = E.Team;
		P.Guess = bMaskDigits ? kMaskedCode : E.Guess;
		P.Famas = E.Famas; P.Picas = E.Picas;
		P.Flags = PublicFlags(E.Flags, bRoundActive);
		P.InfoBitsX10 = E.InfoBitsX10;
		P.ServerTime = E.ServerTime;
		P.RevealTime = PublicRevealTime(E);
		return P;
	}

	struct FPendingGuess
	{
		uint8_t    Player       = kNoPlayer;
		PackedCode Guess        = 0;
		uint8_t    Flags        = GuessFlags::None;
		uint8_t    DecoyFamas   = 0;
		uint8_t    DecoyPicas   = 0;
		double     ReceiveTime  = 0.0;   // reloj del servidor al recibir el RPC
		double     AdjustedTime = 0.0;   // ReceiveTime - clamp(RTT/2, 0, MaxLatencyCompensation)
	};

	enum class EEventType : uint8_t
	{
		EntryAdded = 0,      // Seq
		EntryUpdated,        // Seq
		GuessResult,         // privado al dueno: Player, Seq, Guess, Aux0 = Famas, Aux1 = Picas
		GuessRejected,       // Player, Value = ERejectReason
		Pass,                // Player
		Score,               // Player, Value = delta, Aux0 = EScoreReason
		PlayerStatsChanged,  // Player (Best*, Attempts, tokens, deadline)
		PlayerInactive,      // Player
		PlayerActive,        // Player
		PlayerDropped,       // Player
		Alert,               // Player alcanzo N-1 Famas
		SuddenDeathStarted,  // Time = fin de la cuenta atras
		KeyClue,             // Seq, Player, Time (invisible para clientes; telemetria)
		Solved,              // Player, Seq (ganador principal)
		PhotoFinish,         // Player, Seq (co-ganador)
		Combo,               // Player, Value = bonus, Time = segundos desde la Pista Clave
		Intuition,           // Player (acerto con > 1 candidato)
		RoundStarted,        // Time
		RoundEnded,          // Value = ERoundEndReason, Player = ganador (o kNoPlayer), Guess = codigo
		TurnChanged,         // modo por turnos: Player = quien tiene el turno (kNoPlayer si nadie), Value = numero de turno, Time = fin de su reloj
	};

	struct FRoundEvent
	{
		EEventType Type   = EEventType::EntryAdded;
		uint8_t    Player = kNoPlayer;
		int32_t    Seq    = -1;
		int32_t    Value  = 0;
		uint8_t    Aux0   = 0;
		uint8_t    Aux1   = 0;
		PackedCode Guess  = 0;
		double     Time   = 0.0;
	};

	class IRoundListener
	{
	public:
		virtual ~IRoundListener() = default;
		virtual void OnRoundEvent(const FRoundEvent& Event) = 0;
	};
}
