#include "PFRoundEngine.h"

namespace PF
{
	namespace
	{
		inline double ClampD(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
		inline int32_t RoundToInt(float V) { return static_cast<int32_t>(V + (V >= 0.f ? 0.5f : -0.5f)); }
		inline uint8_t Bit(uint8_t Player) { return static_cast<uint8_t>(1u << Player); }
	}

	FRoundEngine::FRoundEngine()
	{
	}

	// ------------------------------------------------------------------------------------------------
	// Sala
	// ------------------------------------------------------------------------------------------------

	bool FRoundEngine::AddPlayer(uint8_t Player, uint8_t Team)
	{
		if (Player >= kMaxPlayers) return false;
		FPlayerSlot& S = Players[Player];
		S = FPlayerSlot{};
		S.bPresent = true;
		S.bConnected = true;
		S.TeamIndex = Team;
		return true;
	}

	void FRoundEngine::RemovePlayer(uint8_t Player)
	{
		if (Player >= kMaxPlayers) return;
		Players[Player] = FPlayerSlot{};
	}

	void FRoundEngine::SetPlayerConnected(uint8_t Player, bool bConnected, double Now)
	{
		if (!ValidPlayer(Player)) return;
		FPlayerSlot& S = Players[Player];
		if (S.bConnected == bConnected) return;

		S.bConnected = bConnected;
		if (!bConnected)
		{
			S.DisconnectTime = Now;
			S.AttemptDeadline = 0.0;           // el reloj se para: desconectar no acumula Pasos
			if (IsTurnBased() && Player == CurrentTurn) AdvanceTurn(Now);
		}
		else
		{
			S.bDropped = false;
			S.DisconnectTime = 0.0;
			if (bRoundActive && !S.bInactive)
			{
				if (!IsTurnBased()) ResetAttemptClock(Player, Now);
				else if (CurrentTurn == kNoPlayer) AdvanceTurn(Now);   // nadie podia jugar: retoma la rotacion
			}
		}
		EmitSimple(EEventType::PlayerStatsChanged, Player, Now);
	}

	void FRoundEngine::NotePlayerAction(uint8_t Player, double Now)
	{
		if (!ValidPlayer(Player)) return;
		FPlayerSlot& S = Players[Player];
		S.ConsecutivePasses = 0;
		if (S.bInactive)
		{
			S.bInactive = false;
			EmitSimple(EEventType::PlayerActive, Player, Now);
			if (bRoundActive && S.bConnected)
			{
				if (!IsTurnBased()) ResetAttemptClock(Player, Now);
				else if (CurrentTurn == kNoPlayer) AdvanceTurn(Now);
			}
			EmitSimple(EEventType::PlayerStatsChanged, Player, Now);
		}
	}

	int32_t FRoundEngine::NumPresentPlayers() const
	{
		int32_t N = 0;
		for (int32_t i = 0; i < kMaxPlayers; ++i) N += Players[i].bPresent ? 1 : 0;
		return N;
	}

	int32_t FRoundEngine::NumConnectedActivePlayers() const
	{
		int32_t N = 0;
		for (int32_t i = 0; i < kMaxPlayers; ++i)
		{
			const FPlayerSlot& S = Players[i];
			N += (S.bPresent && S.bConnected && !S.bInactive) ? 1 : 0;
		}
		return N;
	}

	// ------------------------------------------------------------------------------------------------
	// Ronda
	// ------------------------------------------------------------------------------------------------

	void FRoundEngine::StartRound(const FRoundConfig& InConfig, uint64_t Seed, double Now)
	{
		Config = InConfig;
		if (Config.CodeLength < kMinCodeLength) Config.CodeLength = kMinCodeLength;
		if (Config.CodeLength > kMaxCodeLength) Config.CodeLength = kMaxCodeLength;

		Candidates.Reset(Config.CodeLength);
		FRng Rng(Seed);
		SecretCode = GenerateCode(Rng, Config.CodeLength);
		SecretMask = DigitMaskOf(SecretCode, Config.CodeLength);

		EntryCount = 0;
		PendingCount = 0;
		bRoundActive = true;
		RoundStartTime = Now;
		KeyClueTime = -1.0;
		KeyClueSeq = -1;
		bRoundSolved = false;
		FirstSolveAdjustedTime = 0.0;
		FirstSolveSeq = -1;
		FirstWinner = kNoPlayer;
		WinnerMask = 0;
		AlertPlayer = kNoPlayer;
		SuddenDeathEndTime = 0.0;
		EndReason = ERoundEndReason::None;

		for (int32_t i = 0; i < kMaxPlayers; ++i)
		{
			FPlayerSlot& S = Players[i];
			if (!S.bPresent) continue;
			S.bInactive = false;
			S.bHasEncryptToken = true;
			S.bHasDecoyToken = true;
			S.BestFamas = 0;
			S.BestPicas = 0;
			S.Attempts = 0;
			S.ConsecutivePasses = 0;
			S.RoundScore = 0;
			S.LastAttemptTime = -1.0e9;
			S.BestAttemptTime = 0.0;
			// Simultaneo: todos los relojes arrancan ya. Por turnos: solo el de quien tenga el turno (AdvanceTurn).
			S.AttemptDeadline = (!IsTurnBased() && S.bConnected) ? Now + Config.AttemptSeconds : 0.0;
		}

		TurnCount = 0; TurnCursor = -1; CurrentTurn = kNoPlayer; TurnNumber = 0;
		if (IsTurnBased()) BuildTurnOrder(Rng);   // misma semilla que el codigo: orden reproducible

		FRoundEvent Ev; Ev.Type = EEventType::RoundStarted; Ev.Time = Now; Ev.Value = Config.CodeLength;
		Emit(Ev);
		for (int32_t i = 0; i < kMaxPlayers; ++i)
		{
			if (Players[i].bPresent) EmitSimple(EEventType::PlayerStatsChanged, static_cast<uint8_t>(i), Now);
		}
		if (IsTurnBased()) AdvanceTurn(Now);
	}

	// ------------------------------------------------------------------------------------------------
	// Modo por turnos
	// ------------------------------------------------------------------------------------------------

	void FRoundEngine::BuildTurnOrder(FRng& Rng)
	{
		TurnCount = 0;
		for (int32_t i = 0; i < kMaxPlayers; ++i)
		{
			if (Players[i].bPresent) TurnOrder[TurnCount++] = static_cast<uint8_t>(i);
		}
		if (Config.TurnMode == ETurnMode::RandomOrder)
		{
			// Fisher-Yates con el RNG determinista de la ronda.
			for (int32_t i = TurnCount - 1; i > 0; --i)
			{
				const int32_t j = static_cast<int32_t>(Rng.Next64() % static_cast<uint64_t>(i + 1));
				const uint8_t Tmp = TurnOrder[i]; TurnOrder[i] = TurnOrder[j]; TurnOrder[j] = Tmp;
			}
		}
	}

	bool FRoundEngine::CanTakeTurn(uint8_t Player) const
	{
		const FPlayerSlot& S = Players[Player];
		return S.bPresent && S.bConnected && !S.bInactive && !S.bDropped;
	}

	void FRoundEngine::AdvanceTurn(double Now)
	{
		if (!bRoundActive || !IsTurnBased()) return;

		const uint8_t Prev = CurrentTurn;
		if (Prev != kNoPlayer) Players[Prev].AttemptDeadline = 0.0;   // su reloj se para
		CurrentTurn = kNoPlayer;

		// Siguiente elegible en el orden circular (desconectados, inactivos y caidos se saltan).
		for (int32_t k = 0; k < TurnCount; ++k)
		{
			TurnCursor = (TurnCursor + 1) % TurnCount;
			const uint8_t Cand = TurnOrder[TurnCursor];
			if (CanTakeTurn(Cand)) { CurrentTurn = Cand; break; }
		}
		++TurnNumber;

		if (CurrentTurn != kNoPlayer) ResetAttemptClock(CurrentTurn, Now);

		FRoundEvent Ev; Ev.Type = EEventType::TurnChanged; Ev.Player = CurrentTurn; Ev.Value = TurnNumber;
		Ev.Time = CurrentTurn != kNoPlayer ? Players[CurrentTurn].AttemptDeadline : Now;
		Emit(Ev);
		if (Prev != kNoPlayer && Prev != CurrentTurn) EmitSimple(EEventType::PlayerStatsChanged, Prev, Now);
		if (CurrentTurn != kNoPlayer) EmitSimple(EEventType::PlayerStatsChanged, CurrentTurn, Now);
	}

	void FRoundEngine::OnAttemptConsumed(uint8_t Player, double Now)
	{
		if (IsTurnBased())
		{
			if (Player == CurrentTurn) AdvanceTurn(Now);
		}
		else
		{
			ResetAttemptClock(Player, Now);
		}
	}

	void FRoundEngine::ForceEndRound(ERoundEndReason Reason, double Now)
	{
		if (bRoundActive) EndRound(Reason, Now);
	}

	bool FRoundEngine::Enqueue(uint8_t Player, PackedCode Guess, uint8_t RequestedFlags, uint8_t DecoyFamas, uint8_t DecoyPicas,
	                           double ReceiveTime, double RoundTripTime)
	{
		ERejectReason Reject = ERejectReason::None;
		if (!bRoundActive)                                   Reject = ERejectReason::RoundNotActive;
		else if (!ValidPlayer(Player))                       Reject = ERejectReason::UnknownPlayer;
		else if (!Players[Player].bConnected)                Reject = ERejectReason::PlayerDisconnected;
		else if (!IsValidCode(Guess, Config.CodeLength))     Reject = ERejectReason::InvalidGuess;
		else if (IsTurnBased() && Player != CurrentTurn)     Reject = ERejectReason::NotYourTurn;
		else if (PendingCount >= kMaxPending)                Reject = ERejectReason::HistoryFull;

		if (Reject != ERejectReason::None)
		{
			// Intentar jugar fuera de turno sigue siendo actividad: un Inactivo vuelve a entrar en la rotacion.
			if (Reject == ERejectReason::NotYourTurn && Players[Player].bInactive) NotePlayerAction(Player, ReceiveTime);
			EmitSimple(EEventType::GuessRejected, Player, ReceiveTime, -1, static_cast<int32_t>(Reject));
			return false;
		}

		// Compensacion de latencia con tope: un ping alto no compra ventaja ilimitada.
		const double HalfRtt = ClampD(RoundTripTime * 0.5, 0.0, Config.MaxLatencyCompensation);

		FPendingGuess& P = Pending[PendingCount++];
		P.Player = Player;
		P.Guess = Guess;
		P.Flags = RequestedFlags & kClientRequestableFlags;
		P.DecoyFamas = DecoyFamas;
		P.DecoyPicas = DecoyPicas;
		P.ReceiveTime = ReceiveTime;
		P.AdjustedTime = ReceiveTime - HalfRtt;
		return true;
	}

	void FRoundEngine::Tick(double Now)
	{
		if (!bRoundActive)
		{
			PendingCount = 0;
			return;
		}

		ResolvePending(Now);
		if (!bRoundActive) return;   // resuelta en esta pasada

		ProcessDeadlines(Now);
		ProcessReveals(Now);
		ProcessDisconnects(Now);

		if (SuddenDeathEndTime > 0.0 && Now >= SuddenDeathEndTime)
		{
			EndRound(ERoundEndReason::SuddenDeathExpired, Now);
		}
		else if (Now >= RoundStartTime + Config.RoundCapSeconds)
		{
			EndRound(ERoundEndReason::TimeCap, Now);
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Resolucion de intentos
	// ------------------------------------------------------------------------------------------------

	void FRoundEngine::ResolvePending(double Now)
	{
		if (PendingCount == 0) return;

		// Orden justo: por tiempo ajustado, no por orden de llegada al socket. Insercion estable (N <= 64).
		for (int32_t i = 1; i < PendingCount; ++i)
		{
			FPendingGuess Key = Pending[i];
			int32_t j = i - 1;
			while (j >= 0 && Pending[j].AdjustedTime > Key.AdjustedTime)
			{
				Pending[j + 1] = Pending[j];
				--j;
			}
			Pending[j + 1] = Key;
		}

		const int32_t Count = PendingCount;
		for (int32_t i = 0; i < Count; ++i)
		{
			ResolveOne(Pending[i], Now);
		}
		PendingCount = 0;

		if (bRoundSolved) EndRound(ERoundEndReason::Solved, Now);
	}

	void FRoundEngine::ResolveOne(FPendingGuess& P, double Now)
	{
		FPlayerSlot& S = Players[P.Player];
		if (!S.bPresent || !S.bConnected)
		{
			EmitSimple(EEventType::GuessRejected, P.Player, Now, -1, static_cast<int32_t>(ERejectReason::PlayerDisconnected));
			return;
		}

		// Enviar es una accion: saca al jugador de Inactivo.
		if (S.bInactive)
		{
			S.bInactive = false;
			S.ConsecutivePasses = 0;
			EmitSimple(EEventType::PlayerActive, P.Player, Now);
		}

		// Por turnos: el turno pudo cambiar entre el encolado y la resolucion (p. ej. dos envios en el mismo tick).
		if (IsTurnBased() && P.Player != CurrentTurn)
		{
			EmitSimple(EEventType::GuessRejected, P.Player, Now, -1, static_cast<int32_t>(ERejectReason::NotYourTurn));
			return;
		}

		// Reloj de intento con margen de jitter. Si expiro, es un Paso, no un intento.
		if (S.AttemptDeadline > 0.0 && P.ReceiveTime > S.AttemptDeadline + Config.DeadlineGrace)
		{
			RegisterPass(P.Player, Now, /*bCountTowardsInactivity*/ false);
			EmitSimple(EEventType::GuessRejected, P.Player, Now, -1, static_cast<int32_t>(ERejectReason::DeadlineExpired));
			return;
		}

		if (P.ReceiveTime - S.LastAttemptTime < Config.MinAttemptInterval)
		{
			EmitSimple(EEventType::GuessRejected, P.Player, Now, -1, static_cast<int32_t>(ERejectReason::TooFast));
			return;
		}

		if (EntryCount >= kMaxEntries)
		{
			EmitSimple(EEventType::GuessRejected, P.Player, Now, -1, static_cast<int32_t>(ERejectReason::HistoryFull));
			return;
		}

		// Verdad autoritativa. El codigo no sale de aqui.
		const FGuessResult Truth = Evaluate(SecretCode, SecretMask, P.Guess, Config.CodeLength);
		const bool bSolved = (Truth.Famas == Config.CodeLength);

		// Ronda ya resuelta en esta misma pasada: solo se atienden otros aciertos (foto-finish o tardio).
		if (bRoundSolved && !bSolved)
		{
			EmitSimple(EEventType::GuessRejected, P.Player, Now, -1, static_cast<int32_t>(ERejectReason::RoundOver));
			return;
		}

		S.LastAttemptTime = P.ReceiveTime;
		S.Attempts++;
		S.ConsecutivePasses = 0;

		// Fichas. Encriptar y Senuelo son excluyentes (gana Senuelo). Un acierto no se puede ocultar.
		uint8_t Flags = P.Flags;
		if (bSolved) Flags = GuessFlags::None;
		if (Flags & GuessFlags::Decoy) Flags &= static_cast<uint8_t>(~GuessFlags::ResultHidden);
		if (Flags & GuessFlags::ResultHidden)
		{
			if (S.bHasEncryptToken) S.bHasEncryptToken = false;
			else Flags &= static_cast<uint8_t>(~GuessFlags::ResultHidden);
		}
		if (Flags & GuessFlags::Decoy)
		{
			const bool bPlausible = P.DecoyFamas < Config.CodeLength
			                     && (P.DecoyFamas + P.DecoyPicas) <= Config.CodeLength
			                     && !(P.DecoyFamas == Truth.Famas && P.DecoyPicas == Truth.Picas);   // "fingir" la verdad no es farol
			if (S.bHasDecoyToken && bPlausible) S.bHasDecoyToken = false;
			else Flags &= static_cast<uint8_t>(~GuessFlags::Decoy);
		}

		// Mejor marca personal (orden lexicografico Famas, Picas).
		if (Truth.Famas > S.BestFamas || (Truth.Famas == S.BestFamas && Truth.Picas > S.BestPicas))
		{
			S.BestFamas = Truth.Famas;
			S.BestPicas = Truth.Picas;
			S.BestAttemptTime = P.AdjustedTime;
		}

		// Entrada publica. Lo que no debe verse, no se escribe en los campos publicos.
		FGuessEntry& E = Entries[EntryCount++];
		E = FGuessEntry{};
		E.Seq = NextSeq++;
		E.Player = P.Player;
		E.Team = S.TeamIndex;
		E.Guess = P.Guess;
		E.TrueFamas = Truth.Famas;
		E.TruePicas = Truth.Picas;
		E.Flags = Flags;
		E.ServerTime = Now;
		if (Flags & GuessFlags::ResultHidden)
		{
			E.Famas = 0; E.Picas = 0;
			E.RevealTime = Now + Config.HideSeconds;
		}
		else if (Flags & GuessFlags::Decoy)
		{
			E.Famas = P.DecoyFamas; E.Picas = P.DecoyPicas;
			E.RevealTime = Now + Config.DecoySeconds;

			// Un senuelo debe parecer un intento normal: publica los bits que APARENTA (segun el resultado falso)
			// y acredita provisionalmente esos puntos. Al revelar se corrige la diferencia. Sin esto, un intento
			// con 0 bits y sin puntos delataria el farol al instante.
			Scratch = Candidates;
			const float FakeBits = Scratch.Filter(P.Guess, FGuessResult{ P.DecoyFamas, P.DecoyPicas });
			const int32_t FakeBitsX10 = RoundToInt(FakeBits * 10.f);
			E.InfoBitsX10 = static_cast<uint8_t>(FakeBitsX10 > 255 ? 255 : FakeBitsX10);
			E.ProvisionalInfoScore = RoundToInt(FakeBits * static_cast<float>(Config.Scoring.PointsPerBit));
			if (E.ProvisionalInfoScore != 0) AddScore(P.Player, EScoreReason::InfoBits, E.ProvisionalInfoScore, Now);
		}
		else
		{
			E.Famas = Truth.Famas; E.Picas = Truth.Picas;
		}

		// El dueno siempre recibe la verdad.
		{
			FRoundEvent Ev; Ev.Type = EEventType::GuessResult; Ev.Player = P.Player; Ev.Seq = E.Seq;
			Ev.Guess = P.Guess; Ev.Aux0 = Truth.Famas; Ev.Aux1 = Truth.Picas; Ev.Time = Now;
			Emit(Ev);
		}

		if (bSolved)
		{
			E.Flags |= GuessFlags::Solved;
			if (!bRoundSolved)
			{
				bRoundSolved = true;
				FirstSolveAdjustedTime = P.AdjustedTime;
				FirstSolveSeq = E.Seq;
				FirstWinner = P.Player;
				WinnerMask |= Bit(P.Player);
				AddScore(P.Player, EScoreReason::Solve, Config.Scoring.Solve, Now);
				EmitSimple(EEventType::Solved, P.Player, Now, E.Seq);

				if (KeyClueTime >= 0.0)
				{
					double Delta = P.AdjustedTime - KeyClueTime;
					if (Delta < 0.0) Delta = 0.0;
					int32_t Bonus = 0;
					EScoreReason Reason = EScoreReason::FastDeduction;
					if (Delta <= Config.Scoring.LightningWindow) { Bonus = Config.Scoring.LightningDeduction; Reason = EScoreReason::LightningDeduction; }
					else if (Delta <= Config.Scoring.FastWindow) { Bonus = Config.Scoring.FastDeduction; }
					if (Bonus > 0) AddScore(P.Player, Reason, Bonus, Now);
					FRoundEvent Ev; Ev.Type = EEventType::Combo; Ev.Player = P.Player; Ev.Seq = E.Seq; Ev.Value = Bonus; Ev.Time = Delta;
					Emit(Ev);
				}
				else
				{
					EmitSimple(EEventType::Intuition, P.Player, Now, E.Seq);
				}
			}
			else if ((P.AdjustedTime - FirstSolveAdjustedTime) < Config.PhotoFinishEpsilon)
			{
				// Empate tecnico: ambos cobran el acierto completo; ninguno cobra combo.
				E.Flags |= GuessFlags::PhotoFinish;
				if (FGuessEntry* First = FindEntryMutable(FirstSolveSeq))
				{
					First->Flags |= GuessFlags::PhotoFinish;
					EmitSimple(EEventType::EntryUpdated, First->Player, Now, First->Seq);
				}
				WinnerMask |= Bit(P.Player);
				AddScore(P.Player, EScoreReason::Solve, Config.Scoring.Solve, Now);
				EmitSimple(EEventType::PhotoFinish, P.Player, Now, E.Seq);
			}
			else
			{
				E.Flags |= GuessFlags::LateSolve;
				AddScore(P.Player, EScoreReason::LateSolve, Config.Scoring.LateSolve, Now);
			}

			EmitSimple(EEventType::EntryAdded, P.Player, Now, E.Seq);
			EmitSimple(EEventType::PlayerStatsChanged, P.Player, Now);
			return;
		}

		// Verdad publica inmediata: filtra candidatos y acredita bits. Encriptar/Senuelo lo posponen a la revelacion.
		if ((E.Flags & (GuessFlags::ResultHidden | GuessFlags::Decoy)) == 0)
		{
			ApplyPublicTruth(E, P.AdjustedTime);
		}
		EmitSimple(EEventType::EntryAdded, P.Player, Now, E.Seq);

		// Alerta global de cercania (N-1 Famas) -> Muerte Sudada.
		if (Truth.Famas == Config.CodeLength - 1 && AlertPlayer == kNoPlayer)
		{
			StartSuddenDeath(P.Player, Now);
		}

		OnAttemptConsumed(P.Player, Now);
		EmitSimple(EEventType::PlayerStatsChanged, P.Player, Now);
	}

	void FRoundEngine::ApplyPublicTruth(FGuessEntry& E, double TimeForKeyClue)
	{
		const FGuessResult Truth{ E.TrueFamas, E.TruePicas };
		const float Bits = Candidates.Filter(E.Guess, Truth);
		const int32_t BitsX10 = RoundToInt(Bits * 10.f);
		E.InfoBitsX10 = static_cast<uint8_t>(BitsX10 > 255 ? 255 : BitsX10);

		// Puntos por informacion, descontando lo ya acreditado provisionalmente si fue un senuelo.
		const int32_t RealScore = RoundToInt(Bits * static_cast<float>(Config.Scoring.PointsPerBit));
		const int32_t Delta = RealScore - E.ProvisionalInfoScore;
		E.ProvisionalInfoScore = 0;
		if (Delta != 0)
		{
			AddScore(E.Player, EScoreReason::InfoBits, Delta, TimeForKeyClue);
		}

		if (Candidates.Num == 1 && KeyClueTime < 0.0)
		{
			KeyClueTime = TimeForKeyClue;
			KeyClueSeq = E.Seq;
			E.Flags |= GuessFlags::KeyClue;
			AddScore(E.Player, EScoreReason::KeyClue, Config.Scoring.KeyClue, TimeForKeyClue);
			FRoundEvent Ev; Ev.Type = EEventType::KeyClue; Ev.Player = E.Player; Ev.Seq = E.Seq; Ev.Time = TimeForKeyClue;
			Emit(Ev);
		}
	}

	void FRoundEngine::RevealEntry(FGuessEntry& E, double Now, bool bForcedBySuspicion)
	{
		const bool bWasDecoy = (E.Flags & GuessFlags::Decoy) != 0;
		const FGuessResult Fake{ E.Famas, E.Picas };
		const FGuessResult Truth{ E.TrueFamas, E.TruePicas };

		E.Famas = E.TrueFamas;
		E.Picas = E.TruePicas;
		E.Flags &= static_cast<uint8_t>(~(GuessFlags::ResultHidden | GuessFlags::Decoy));
		if (bWasDecoy) E.Flags |= GuessFlags::DecoyRevealed;
		E.RevealTime = 0.0;

		// Engano efectivo: rivales que, durante la ventana del Senuelo, probaron una hipotesis
		// consistente con el resultado falso pero inconsistente con el verdadero.
		if (bWasDecoy && !bForcedBySuspicion)
		{
			int32_t Misled = 0;
			for (int32_t i = 0; i < EntryCount; ++i)
			{
				const FGuessEntry& X = Entries[i];
				if (X.Seq <= E.Seq || X.Player == E.Player) continue;
				if (Config.bTeamMode && X.Team == E.Team) continue;
				// Tratamos el intento del rival como codigo hipotetico y miramos que habria puntuado el nuestro contra el.
				const FGuessResult Hyp = Evaluate(X.Guess, E.Guess, Config.CodeLength);
				if (Hyp == Fake && Hyp != Truth) ++Misled;
			}
			if (Config.Scoring.DecoyEffectiveMaxTargets > 0 && Misled > Config.Scoring.DecoyEffectiveMaxTargets)
			{
				Misled = Config.Scoring.DecoyEffectiveMaxTargets;
			}
			if (Misled > 0)
			{
				AddScore(E.Player, EScoreReason::DecoyEffective, Misled * Config.Scoring.DecoyEffective, Now);
			}
		}

		ApplyPublicTruth(E, Now);   // T_key se fija en la revelacion, no en el envio
		EmitSimple(EEventType::EntryUpdated, E.Player, Now, E.Seq);
	}

	void FRoundEngine::Suspect(uint8_t Caller, int32_t Seq, double Now)
	{
		if (!bRoundActive || !ValidPlayer(Caller)) return;
		FGuessEntry* E = FindEntryMutable(Seq);
		if (!E || E->Player == Caller) return;
		if (Config.bTeamMode && E->Team == Players[Caller].TeamIndex) return;
		if (E->Flags & GuessFlags::Suspected) return;                                   // una sospecha por entrada
		if (E->Flags & GuessFlags::ResultHidden) return;                                // no hay nada visible que poner en duda
		if (E->Flags & (GuessFlags::DecoyRevealed | GuessFlags::Solved)) return;       // ya es verdad publica

		NotePlayerAction(Caller, Now);
		E->Flags |= GuessFlags::Suspected;

		if (E->Flags & GuessFlags::Decoy)
		{
			RevealEntry(*E, Now, /*bForcedBySuspicion*/ true);
			AddScore(E->Player, EScoreReason::DecoyCaught, Config.Scoring.DecoyCaught, Now);
			AddScore(Caller, EScoreReason::SuspicionHit, Config.Scoring.SuspicionHit, Now);
		}
		else
		{
			AddScore(Caller, EScoreReason::SuspicionMiss, Config.Scoring.SuspicionMiss, Now);
			EmitSimple(EEventType::EntryUpdated, E->Player, Now, E->Seq);
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Relojes, revelaciones, desconexiones
	// ------------------------------------------------------------------------------------------------

	void FRoundEngine::ProcessDeadlines(double Now)
	{
		for (int32_t i = 0; i < kMaxPlayers; ++i)
		{
			FPlayerSlot& S = Players[i];
			if (!S.bPresent || !S.bConnected || S.bInactive || S.AttemptDeadline <= 0.0) continue;
			if (Now > S.AttemptDeadline + Config.DeadlineGrace)
			{
				RegisterPass(static_cast<uint8_t>(i), Now, /*bCountTowardsInactivity*/ true);
			}
		}
	}

	void FRoundEngine::RegisterPass(uint8_t Player, double Now, bool bCountTowardsInactivity)
	{
		FPlayerSlot& S = Players[Player];
		AddScore(Player, EScoreReason::Pass, Config.Scoring.Pass, Now);
		EmitSimple(EEventType::Pass, Player, Now);

		if (bCountTowardsInactivity)
		{
			S.ConsecutivePasses++;
			if (S.ConsecutivePasses >= Config.PassesUntilInactive)
			{
				S.bInactive = true;
				S.AttemptDeadline = 0.0;   // relojes parados: no acumula mas Pasos
				EmitSimple(EEventType::PlayerInactive, Player, Now);
				EmitSimple(EEventType::PlayerStatsChanged, Player, Now);
				if (IsTurnBased() && Player == CurrentTurn) AdvanceTurn(Now);   // el turno no se queda colgado
				return;
			}
		}
		OnAttemptConsumed(Player, Now);
		EmitSimple(EEventType::PlayerStatsChanged, Player, Now);
	}

	void FRoundEngine::ResetAttemptClock(uint8_t Player, double Now)
	{
		Players[Player].AttemptDeadline = Now + CurrentAttemptSeconds();
	}

	double FRoundEngine::CurrentAttemptSeconds() const
	{
		return SuddenDeathEndTime > 0.0 ? Config.SuddenDeathAttemptSeconds : Config.AttemptSeconds;
	}

	void FRoundEngine::ProcessReveals(double Now)
	{
		for (int32_t i = 0; i < EntryCount; ++i)
		{
			FGuessEntry& E = Entries[i];
			if (E.RevealTime > 0.0 && Now >= E.RevealTime)
			{
				RevealEntry(E, Now, false);
			}
		}
	}

	void FRoundEngine::ProcessDisconnects(double Now)
	{
		for (int32_t i = 0; i < kMaxPlayers; ++i)
		{
			FPlayerSlot& S = Players[i];
			if (!S.bPresent || S.bConnected || S.bDropped) continue;
			if (Now - S.DisconnectTime >= Config.ReconnectGraceSeconds)
			{
				S.bDropped = true;
				EmitSimple(EEventType::PlayerDropped, static_cast<uint8_t>(i), Now);
			}
		}
	}

	void FRoundEngine::StartSuddenDeath(uint8_t TriggerPlayer, double Now)
	{
		AlertPlayer = TriggerPlayer;
		SuddenDeathEndTime = Now + Config.SuddenDeathSeconds;

		for (int32_t i = 0; i < kMaxPlayers; ++i)
		{
			FPlayerSlot& S = Players[i];
			if (!S.bPresent || !S.bConnected || S.bInactive || S.AttemptDeadline <= 0.0) continue;
			const double Shorter = Now + Config.SuddenDeathAttemptSeconds;
			if (S.AttemptDeadline > Shorter)
			{
				S.AttemptDeadline = Shorter;
				EmitSimple(EEventType::PlayerStatsChanged, static_cast<uint8_t>(i), Now);
			}
		}

		EmitSimple(EEventType::Alert, TriggerPlayer, Now);
		FRoundEvent Ev; Ev.Type = EEventType::SuddenDeathStarted; Ev.Player = TriggerPlayer; Ev.Time = SuddenDeathEndTime;
		Emit(Ev);
	}

	// ------------------------------------------------------------------------------------------------
	// Fin de ronda
	// ------------------------------------------------------------------------------------------------

	void FRoundEngine::EndRound(ERoundEndReason Reason, double Now)
	{
		if (!bRoundActive) return;
		bRoundActive = false;
		EndReason = Reason;
		PendingCount = 0;

		// La verdad siempre acaba en el tablero.
		for (int32_t i = 0; i < EntryCount; ++i)
		{
			if (Entries[i].RevealTime > 0.0) RevealEntry(Entries[i], Now, false);
		}
		// Con la ronda cerrada, la Pista Clave deja de ser secreta (el resumen la muestra).
		for (int32_t i = 0; i < EntryCount; ++i)
		{
			if (Entries[i].Flags & GuessFlags::KeyClue) EmitSimple(EEventType::EntryUpdated, Entries[i].Player, Now, Entries[i].Seq);
		}

		uint8_t Winner = kNoPlayer;
		if (Reason == ERoundEndReason::Solved)
		{
			Winner = FirstWinner;
		}
		else if (Reason == ERoundEndReason::SuddenDeathExpired || Reason == ERoundEndReason::TimeCap)
		{
			Winner = PickWinnerByPosition();
			if (Winner != kNoPlayer)
			{
				WinnerMask |= Bit(Winner);
				AddScore(Winner, EScoreReason::PositionWin, Config.Scoring.PositionWin, Now);
			}
		}

		for (int32_t i = 0; i < kMaxPlayers; ++i)
		{
			if (Players[i].bPresent) Players[i].AttemptDeadline = 0.0;
		}

		FRoundEvent Ev; Ev.Type = EEventType::RoundEnded; Ev.Value = static_cast<int32_t>(Reason);
		Ev.Player = Winner; Ev.Guess = SecretCode; Ev.Time = Now;
		Emit(Ev);
	}

	uint8_t FRoundEngine::PickWinnerByPosition() const
	{
		// Mas Famas -> menos intentos -> antes en llegar a esa marca. Los Inactivos solo cuentan si no hay nadie mas.
		auto Better = [](const FPlayerSlot& A, const FPlayerSlot& B) -> bool
		{
			if (A.BestFamas != B.BestFamas) return A.BestFamas > B.BestFamas;
			if (A.BestPicas != B.BestPicas) return A.BestPicas > B.BestPicas;
			if (A.Attempts != B.Attempts)   return A.Attempts < B.Attempts;
			return A.BestAttemptTime < B.BestAttemptTime;
		};

		for (int32_t Pass = 0; Pass < 2; ++Pass)
		{
			uint8_t Best = kNoPlayer;
			for (int32_t i = 0; i < kMaxPlayers; ++i)
			{
				const FPlayerSlot& S = Players[i];
				if (!S.bPresent || S.Attempts == 0) continue;
				if (Pass == 0 && (S.bInactive || !S.bConnected)) continue;
				if (Best == kNoPlayer || Better(S, Players[Best])) Best = static_cast<uint8_t>(i);
			}
			if (Best != kNoPlayer) return Best;
		}
		return kNoPlayer;
	}

	// ------------------------------------------------------------------------------------------------
	// Utilidades
	// ------------------------------------------------------------------------------------------------

	void FRoundEngine::AddScore(uint8_t Player, EScoreReason Reason, int32_t Delta, double Now)
	{
		if (Delta == 0 || !ValidPlayer(Player)) return;
		Players[Player].RoundScore += Delta;
		FRoundEvent Ev; Ev.Type = EEventType::Score; Ev.Player = Player; Ev.Value = Delta;
		Ev.Aux0 = static_cast<uint8_t>(Reason); Ev.Time = Now;
		Emit(Ev);
	}

	const FGuessEntry* FRoundEngine::FindEntry(int32_t Seq) const
	{
		for (int32_t i = EntryCount - 1; i >= 0; --i) if (Entries[i].Seq == Seq) return &Entries[i];
		return nullptr;
	}

	FGuessEntry* FRoundEngine::FindEntryMutable(int32_t Seq)
	{
		for (int32_t i = EntryCount - 1; i >= 0; --i) if (Entries[i].Seq == Seq) return &Entries[i];
		return nullptr;
	}

	void FRoundEngine::Emit(const FRoundEvent& E)
	{
		if (Listener) Listener->OnRoundEvent(E);
	}

	void FRoundEngine::EmitSimple(EEventType Type, uint8_t Player, double Now, int32_t Seq, int32_t Value)
	{
		FRoundEvent Ev; Ev.Type = Type; Ev.Player = Player; Ev.Seq = Seq; Ev.Value = Value; Ev.Time = Now;
		Emit(Ev);
	}
}
