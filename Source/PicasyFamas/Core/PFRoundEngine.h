// Motor de ronda: unica autoridad sobre el codigo secreto y la resolucion de intentos.
// C++ puro. APFGameMode es un adaptador fino que traduce RPCs -> Enqueue/Tick y eventos -> replicacion.
//
// Flujo por tick del servidor:
//   1) Tick(Now) resuelve la cola de intentos ORDENADA por tiempo ajustado por latencia (no por llegada).
//   2) Relojes de intento expirados -> Paso (y, si se encadenan, Inactivo).
//   3) Revelaciones diferidas (Encriptar / Senuelo) -> filtran candidatos y acreditan bits con retraso.
//   4) Fin de Muerte Sudada / cap de ronda.
#pragma once

#include <cstdint>
#include "PFCodeMath.h"
#include "PFCandidateSet.h"
#include "PFRoundTypes.h"

namespace PF
{
	class FRoundEngine
	{
	public:
		static constexpr int32_t kMaxEntries = 1024;
		static constexpr int32_t kMaxPending = 64;

		FRoundEngine();

		void SetListener(IRoundListener* InListener) { Listener = InListener; }

		// ---- Sala (puede llamarse en cualquier fase) ----
		bool AddPlayer(uint8_t Player, uint8_t Team = kNoTeam);
		void RemovePlayer(uint8_t Player);
		void SetPlayerConnected(uint8_t Player, bool bConnected, double Now);
		// Marcar la Pizarra, sospechar, etc.: cuenta como actividad y saca al jugador de Inactivo.
		void NotePlayerAction(uint8_t Player, double Now);

		// ---- Ronda ----
		void StartRound(const FRoundConfig& InConfig, uint64_t Seed, double Now);
		void ForceEndRound(ERoundEndReason Reason, double Now);
		bool IsRoundActive() const { return bRoundActive; }

		// Encola un intento. Devuelve false (y emite GuessRejected) si se rechaza antes de encolar.
		bool Enqueue(uint8_t Player, PackedCode Guess, uint8_t RequestedFlags, uint8_t DecoyFamas, uint8_t DecoyPicas,
		             double ReceiveTime, double RoundTripTime);

		// Sospechar sobre una entrada rival.
		void Suspect(uint8_t Caller, int32_t Seq, double Now);

		// Debe llamarse una vez por tick del servidor.
		void Tick(double Now);

		// ---- Consultas ----
		const FRoundConfig& GetConfig() const { return Config; }
		const FPlayerSlot&  GetPlayer(uint8_t Player) const { return Players[Player]; }
		int32_t             NumPresentPlayers() const;
		int32_t             NumConnectedActivePlayers() const;

		int32_t             NumEntries() const { return EntryCount; }
		const FGuessEntry&  EntryAt(int32_t Index) const { return Entries[Index]; }
		const FGuessEntry*  FindEntry(int32_t Seq) const;

		PackedCode          GetSecretCode() const { return SecretCode; }   // solo para revelar al fin de ronda
		int32_t             NumCandidates() const { return Candidates.Num; }
		double              GetKeyClueTime() const { return KeyClueTime; }
		double              GetRoundStartTime() const { return RoundStartTime; }
		double              GetSuddenDeathEndTime() const { return SuddenDeathEndTime; }
		uint8_t             GetAlertPlayer() const { return AlertPlayer; }
		uint8_t             GetWinnerMask() const { return WinnerMask; }
		ERoundEndReason     GetEndReason() const { return EndReason; }

	private:
		// Resolucion
		void ResolvePending(double Now);
		void ResolveOne(FPendingGuess& P, double Now);
		void RevealEntry(FGuessEntry& E, double Now, bool bForcedBySuspicion);
		void ApplyPublicTruth(FGuessEntry& E, double Now);   // filtra candidatos, bits, Pista Clave
		void ProcessDeadlines(double Now);
		void ProcessReveals(double Now);
		void ProcessDisconnects(double Now);
		void RegisterPass(uint8_t Player, double Now, bool bCountTowardsInactivity);
		void ResetAttemptClock(uint8_t Player, double Now);
		double CurrentAttemptSeconds() const;
		void StartSuddenDeath(uint8_t TriggerPlayer, double Now);
		void EndRound(ERoundEndReason Reason, double Now);
		uint8_t PickWinnerByPosition() const;
		void AddScore(uint8_t Player, EScoreReason Reason, int32_t Delta, double Now);
		FGuessEntry* FindEntryMutable(int32_t Seq);
		void Emit(const FRoundEvent& E);
		void EmitSimple(EEventType Type, uint8_t Player, double Now, int32_t Seq = -1, int32_t Value = 0);

		bool ValidPlayer(uint8_t Player) const { return Player < kMaxPlayers && Players[Player].bPresent; }

	private:
		IRoundListener* Listener = nullptr;
		FRoundConfig    Config;

		FPlayerSlot     Players[kMaxPlayers];

		FGuessEntry     Entries[kMaxEntries];
		int32_t         EntryCount = 0;
		int32_t         NextSeq = 1;

		FPendingGuess   Pending[kMaxPending];
		int32_t         PendingCount = 0;

		FCandidateSet   Candidates;
		FCandidateSet   Scratch;       // copia de trabajo para calcular los bits "aparentes" de un senuelo
		PackedCode      SecretCode = 0;
		uint16_t        SecretMask = 0;

		bool            bRoundActive = false;
		double          RoundStartTime = 0.0;
		double          KeyClueTime = -1.0;
		int32_t         KeyClueSeq = -1;
		bool            bRoundSolved = false;
		double          FirstSolveAdjustedTime = 0.0;
		int32_t         FirstSolveSeq = -1;
		uint8_t         FirstWinner = kNoPlayer;
		uint8_t         WinnerMask = 0;
		uint8_t         AlertPlayer = kNoPlayer;
		double          SuddenDeathEndTime = 0.0;   // 0 = inactiva
		ERoundEndReason EndReason = ERoundEndReason::None;
	};
}
