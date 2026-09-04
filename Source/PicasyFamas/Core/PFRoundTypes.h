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
		int32_t DecoyCaught        = -30;
		int32_t SuspicionHit       = 15;
		int32_t SuspicionMiss      = -10;
		int32_t Pass               = -5;
		double  LightningWindow    = 3.0;  // s desde la Pista Clave
		double  FastWindow         = 6.0;
	};

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
		FScoringConfig Scoring;
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
		uint8_t    InfoBitsX10    = 0;
		double     ServerTime     = 0.0;
		double     RevealTime     = 0.0;     // > 0 mientras hay una revelacion pendiente
	};

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
