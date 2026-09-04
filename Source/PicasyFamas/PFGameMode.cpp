#include "PFGameMode.h"
#include "PicasyFamas.h"
#include "PFGameState.h"
#include "PFPlayerState.h"
#include "PFPlayerController.h"
#include "PFTuningDataAsset.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

APFGameMode::APFGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	GameStateClass        = APFGameState::StaticClass();
	PlayerStateClass      = APFPlayerState::StaticClass();
	PlayerControllerClass = APFPlayerController::StaticClass();
	DefaultPawnClass      = nullptr;      // juego de UI: sin pawn

	bDelayedStart = true;                 // nuestra FSM decide cuando empieza la partida
	InactivePlayerStateLifeSpan = 20.f;   // gracia de reconexion (se sincroniza con el Tuning en BeginPlay)

	ListenerBridge = MakeUnique<FPFRoundListenerBridge>(this);
	Engine = MakeUnique<PF::FRoundEngine>();
	Engine->SetListener(ListenerBridge.Get());
}

void FPFRoundListenerBridge::OnRoundEvent(const PF::FRoundEvent& Event)
{
	if (Owner) Owner->OnRoundEvent(Event);
}

void APFGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!Tuning)
	{
		Tuning = NewObject<UPFTuningDataAsset>(this, TEXT("DefaultTuning"));
		UE_LOG(LogPicasyFamas, Warning, TEXT("APFGameMode sin Tuning asignado: usando valores por defecto."));
	}
	InactivePlayerStateLifeSpan = Tuning->ReconnectGraceSeconds;

	if (APFGameState* GS = PFGameState())
	{
		GS->NumRounds = static_cast<uint8>(FMath::Clamp(Tuning->RoundsPerMatch, 1, 9));
		GS->ServerSetPhase(EPFRoundPhase::Lobby, 0.f);
	}
}

// ------------------------------------------------------------------------------------------------
// Tick / FSM de fases
// ------------------------------------------------------------------------------------------------

void APFGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APFGameState* GS = PFGameState();
	if (!GS) return;

	const double Now = ServerNow();
	switch (GS->Phase)
	{
	case EPFRoundPhase::Lobby:     UpdateLobby(Now);     break;
	case EPFRoundPhase::Countdown: UpdateCountdown(Now); break;
	case EPFRoundPhase::Playing:   UpdatePlaying(Now);   break;
	case EPFRoundPhase::Reward:    UpdateReward(Now);    break;
	case EPFRoundPhase::MatchEnd:  UpdateMatchEnd(Now);  break;
	}
}

void APFGameMode::UpdateLobby(double Now)
{
	SeatWaitingPlayers();
	if (CountSeatedConnected() >= EffectiveMinPlayers())
	{
		PFGameState()->ServerSetPhase(EPFRoundPhase::Countdown, static_cast<float>(Now + Tuning->LobbyCountdownSeconds));
	}
}

void APFGameMode::UpdateCountdown(double Now)
{
	APFGameState* GS = PFGameState();
	SeatWaitingPlayers();
	if (CountSeatedConnected() < EffectiveMinPlayers())
	{
		GS->ServerSetPhase(EPFRoundPhase::Lobby, 0.f);
		return;
	}
	if (Now >= GS->PhaseEndServerTime)
	{
		GS->RoundIndex = 0;
		ResetMatchScores();
		StartRound(Now);
	}
}

void APFGameMode::UpdatePlaying(double Now)
{
	Engine->Tick(Now);
	if (!Engine->IsRoundActive()) return;   // RoundEnded ya cambio la fase

	// Umbral de sala: con menos de 2 conectados el enigma pierde sentido.
	if (CountSeatedConnected() < 2)
	{
		Engine->ForceEndRound(PF::ERoundEndReason::NotEnoughPlayers, Now);
	}
}

void APFGameMode::UpdateReward(double Now)
{
	APFGameState* GS = PFGameState();
	if (Now < GS->PhaseEndServerTime) return;

	if (GS->LastRoundEndReason == EPFRoundEndReason::NotEnoughPlayers || CountSeatedConnected() < 2)
	{
		GS->ServerSetPhase(EPFRoundPhase::MatchEnd, static_cast<float>(Now + Tuning->MatchEndSeconds));
		return;
	}

	if (GS->RoundIndex + 1 < GS->NumRounds)
	{
		GS->RoundIndex++;
		StartRound(Now);
	}
	else
	{
		GS->ServerSetPhase(EPFRoundPhase::MatchEnd, static_cast<float>(Now + Tuning->MatchEndSeconds));
	}
}

void APFGameMode::UpdateMatchEnd(double Now)
{
	APFGameState* GS = PFGameState();
	if (Now >= GS->PhaseEndServerTime)
	{
		EnterLobby(Now);
	}
}

void APFGameMode::StartRound(double Now)
{
	APFGameState* GS = PFGameState();

	ReleaseDroppedSeats();
	SeatWaitingPlayers();

	const int32 NumPlayers = Engine->NumPresentPlayers();
	const PF::FRoundConfig Config = Tuning->ToRoundConfig(NumPlayers, /*bTeamMode*/ false);

	GS->ServerResetHistory();
	GS->CodeLength = static_cast<uint8>(Config.CodeLength);
	GS->bTeamMode = Config.bTeamMode;
	GS->RoundStartServerTime = static_cast<float>(Now);
	GS->RoundCapServerTime = static_cast<float>(Now + Config.RoundCapSeconds);
	GS->RevealedCode = -1;
	GS->LastRoundEndReason = EPFRoundEndReason::None;
	GS->LastRoundWinnerMask = 0;
	GS->ServerSetAlert(255, 0.f);

	for (uint8 Seat = 0; Seat < PF::kMaxPlayers; ++Seat)
	{
		if (APFPlayerState* PS = SeatPS(Seat)) PS->ServerResetForNewRound();
	}

	// Semilla por ronda: con ella la partida es reproducible en un replay del servidor.
	const uint64 Seed = FPlatformTime::Cycles64() ^ (static_cast<uint64>(FMath::Rand()) << 32) ^ static_cast<uint64>(FMath::Rand());
	GS->TurnMode = static_cast<EPFTurnMode>(Config.TurnMode);
	GS->TurnOrder.Reset();
	Engine->StartRound(Config, Seed, Now);   // emite TurnChanged si es por turnos
	for (int32 i = 0; i < Engine->GetTurnOrderCount(); ++i) GS->TurnOrder.Add(Engine->GetTurnOrderAt(i));

	GS->ServerSetPhase(EPFRoundPhase::Playing, 0.f);

	if (!bMatchStarted)
	{
		bMatchStarted = true;
		StartMatch();
	}

	UE_LOG(LogPicasyFamas, Log, TEXT("Ronda %d/%d iniciada: %d jugadores, %d digitos, %d candidatos."),
		GS->RoundIndex + 1, GS->NumRounds, NumPlayers, Config.CodeLength, Engine->NumCandidates());
#if !UE_BUILD_SHIPPING
	UE_LOG(LogPicasyFamas, Verbose, TEXT("[DEBUG] Codigo secreto: %s"),
		*UPFCodeLibrary::GuessToString(static_cast<int32>(Engine->GetSecretCode()), Config.CodeLength));
#endif
}

void APFGameMode::EnterLobby(double Now)
{
	APFGameState* GS = PFGameState();
	ReleaseDroppedSeats();
	GS->RoundIndex = 0;
	GS->RevealedCode = -1;
	GS->ServerResetHistory();
	GS->ServerSetAlert(255, 0.f);
	ResetMatchScores();
	GS->ServerSetPhase(EPFRoundPhase::Lobby, 0.f);
	(void)Now;
}

void APFGameMode::ResetMatchScores()
{
	for (uint8 Seat = 0; Seat < PF::kMaxPlayers; ++Seat)
	{
		if (APFPlayerState* PS = SeatPS(Seat))
		{
			PS->SetScore(0.f);
			PS->ServerResetForNewRound();
		}
	}
}

// ------------------------------------------------------------------------------------------------
// Login / Logout / asientos
// ------------------------------------------------------------------------------------------------

void APFGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	APFPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<APFPlayerState>() : nullptr;
	if (!PS) return;

	const double Now = ServerNow();

	// Reconexion: AGameMode ya volco el PlayerState inactivo (OverrideWith) y conserva el asiento.
	if (PS->HasSeat() && PS->SeatIndex < PF::kMaxPlayers)
	{
		const PF::FPlayerSlot& Slot = Engine->GetPlayer(PS->SeatIndex);
		const APFPlayerState* Current = SeatPS(PS->SeatIndex);
		if (Slot.bPresent && !Slot.bConnected && (Current == nullptr || Current == PS))
		{
			Seats[PS->SeatIndex] = PS;
			Engine->SetPlayerConnected(PS->SeatIndex, true, Now);
			SyncSeat(PS->SeatIndex);
			UE_LOG(LogPicasyFamas, Log, TEXT("%s reconectado en el asiento %d."), *PS->GetPlayerName(), PS->SeatIndex);
			return;
		}
		// El asiento ya no es suyo (gracia agotada y liberado): entra como nuevo.
		PS->ServerClearSeat();
	}

	const EPFRoundPhase Phase = PFGameState()->Phase;
	if (Phase == EPFRoundPhase::Playing)
	{
		// Late join a mitad de ronda: espectador hasta la siguiente ronda.
		PS->ServerClearSeat();
		return;
	}
	AssignSeat(PS);
}

void APFGameMode::Logout(AController* Exiting)
{
	if (APFPlayerState* PS = Exiting ? Exiting->GetPlayerState<APFPlayerState>() : nullptr)
	{
		if (PS->HasSeat() && PS->SeatIndex < PF::kMaxPlayers)
		{
			Engine->SetPlayerConnected(PS->SeatIndex, false, ServerNow());
			// Seats[] queda apuntando a un PS que se destruira; el inactivo lo restaura en PostLogin.
			UE_LOG(LogPicasyFamas, Log, TEXT("%s desconectado (asiento %d). Gracia de %.0f s."),
				*PS->GetPlayerName(), PS->SeatIndex, Tuning ? Tuning->ReconnectGraceSeconds : 20.f);
		}
	}
	Super::Logout(Exiting);
}

int32 APFGameMode::FindFreeSeat() const
{
	const int32 Max = Tuning ? FMath::Clamp(Tuning->MaxPlayers, 2, PF::kMaxPlayers) : PF::kMaxPlayers;
	for (int32 Seat = 0; Seat < Max; ++Seat)
	{
		if (!Engine->GetPlayer(static_cast<uint8>(Seat)).bPresent) return Seat;
	}
	return INDEX_NONE;
}

bool APFGameMode::AssignSeat(APFPlayerState* PS)
{
	const int32 Seat = FindFreeSeat();
	if (Seat == INDEX_NONE)
	{
		PS->ServerClearSeat();   // sala llena: espera
		return false;
	}
	Engine->AddPlayer(static_cast<uint8>(Seat), PF::kNoTeam);
	Seats[Seat] = PS;
	PS->ServerSetSeat(static_cast<uint8>(Seat), PF::kNoTeam);
	SyncSeat(static_cast<uint8>(Seat));
	return true;
}

void APFGameMode::ReleaseSeat(uint8 Seat)
{
	if (APFPlayerState* PS = SeatPS(Seat)) PS->ServerClearSeat();
	Seats[Seat] = nullptr;
	Engine->RemovePlayer(Seat);
}

void APFGameMode::SeatWaitingPlayers()
{
	if (PFGameState()->Phase == EPFRoundPhase::Playing) return;
	for (APlayerState* Base : GameState->PlayerArray)
	{
		APFPlayerState* PS = Cast<APFPlayerState>(Base);
		if (PS && !PS->HasSeat())
		{
			if (!AssignSeat(PS)) break;   // sala llena
		}
	}
}

void APFGameMode::ReleaseDroppedSeats()
{
	for (uint8 Seat = 0; Seat < PF::kMaxPlayers; ++Seat)
	{
		const PF::FPlayerSlot& Slot = Engine->GetPlayer(Seat);
		const bool bPending = (PendingSeatRelease & (1u << Seat)) != 0;
		if (Slot.bPresent && (Slot.bDropped || bPending))
		{
			ReleaseSeat(Seat);
		}
	}
	PendingSeatRelease = 0;
}

int32 APFGameMode::CountSeatedConnected() const
{
	int32 N = 0;
	for (uint8 Seat = 0; Seat < PF::kMaxPlayers; ++Seat)
	{
		const PF::FPlayerSlot& Slot = Engine->GetPlayer(Seat);
		N += (Slot.bPresent && Slot.bConnected) ? 1 : 0;
	}
	return N;
}

int32 APFGameMode::EffectiveMinPlayers() const
{
	if (MinPlayersOverride > 0) return MinPlayersOverride;
	return Tuning ? FMath::Max(1, Tuning->MinPlayersToStart) : 3;
}

// ------------------------------------------------------------------------------------------------
// Entrada desde PlayerController
// ------------------------------------------------------------------------------------------------

void APFGameMode::HandleSubmitGuess(APFPlayerController* PC, PF::PackedCode Guess, uint8 Flags, uint8 DecoyFamas, uint8 DecoyPicas)
{
	APFPlayerState* PS = PC ? PC->GetPlayerState<APFPlayerState>() : nullptr;
	if (!PS || !PS->HasSeat())
	{
		if (PC) PC->Client_GuessRejected(EPFRejectReason::UnknownPlayer);
		return;
	}
	// El RPC solo encola. La resolucion ocurre en Tick, ordenada por tiempo ajustado por latencia.
	Engine->Enqueue(PS->SeatIndex, Guess, Flags, DecoyFamas, DecoyPicas, ServerNow(), PC->GetServerMeasuredRTT());
}

void APFGameMode::HandleSuspect(APFPlayerController* PC, int32 Seq)
{
	APFPlayerState* PS = PC ? PC->GetPlayerState<APFPlayerState>() : nullptr;
	if (!PS || !PS->HasSeat()) return;
	Engine->Suspect(PS->SeatIndex, Seq, ServerNow());
}

void APFGameMode::HandleNoteAction(APFPlayerController* PC)
{
	APFPlayerState* PS = PC ? PC->GetPlayerState<APFPlayerState>() : nullptr;
	if (!PS || !PS->HasSeat()) return;
	Engine->NotePlayerAction(PS->SeatIndex, ServerNow());
}

void APFGameMode::PFForceStart()
{
	APFGameState* GS = PFGameState();
	if (!GS || GS->Phase == EPFRoundPhase::Playing) return;
	SeatWaitingPlayers();
	GS->RoundIndex = 0;
	ResetMatchScores();
	StartRound(ServerNow());
}

void APFGameMode::PFEndRound()
{
	if (Engine->IsRoundActive()) Engine->ForceEndRound(PF::ERoundEndReason::Aborted, ServerNow());
}

// ------------------------------------------------------------------------------------------------
// Eventos del motor -> replicacion
// ------------------------------------------------------------------------------------------------

void APFGameMode::OnRoundEvent(const PF::FRoundEvent& E)
{
	APFGameState* GS = PFGameState();
	if (!GS) return;

	using PF::EEventType;
	switch (E.Type)
	{
	case EEventType::EntryAdded:
	{
		if (const PF::FGuessEntry* In = Engine->FindEntry(E.Seq))
		{
			FPFGuessEntry Out;
			FillPublicEntry(*In, Out);
			GS->ServerAddEntry(Out);
		}
		break;
	}
	case EEventType::EntryUpdated:
	{
		const PF::FGuessEntry* In = Engine->FindEntry(E.Seq);
		FPFGuessEntry* Out = GS->ServerFindEntryMutable(E.Seq);
		if (In && Out)
		{
			FillPublicEntry(*In, *Out);
			GS->ServerMarkEntryDirty(*Out);
		}
		break;
	}
	case EEventType::GuessResult:
	{
		if (APFPlayerController* PC = SeatPC(E.Player))
		{
			PC->Client_GuessResult(E.Seq, static_cast<int32>(E.Guess), E.Aux0, E.Aux1);
		}
		if (APFPlayerState* PS = SeatPS(E.Player))
		{
			FPFPrivateAttempt A;
			A.Seq = E.Seq; A.PackedGuess = static_cast<int32>(E.Guess); A.Famas = E.Aux0; A.Picas = E.Aux1;
			A.ServerTime = static_cast<float>(E.Time);
			PS->ServerAddPrivateAttempt(A);
		}
		break;
	}
	case EEventType::GuessRejected:
	{
		if (APFPlayerController* PC = SeatPC(E.Player))
		{
			PC->Client_GuessRejected(static_cast<EPFRejectReason>(E.Value));
		}
		break;
	}
	case EEventType::Score:
	{
		if (APFPlayerState* PS = SeatPS(E.Player))
		{
			PS->ServerAddMatchScore(E.Value);
			PS->ServerSyncFromSlot(Engine->GetPlayer(E.Player));
		}
		if (APFPlayerController* PC = SeatPC(E.Player))
		{
			PC->Client_ShowScore(static_cast<EPFScoreReason>(E.Aux0), E.Value);
		}
		break;
	}
	case EEventType::Pass:
	case EEventType::PlayerStatsChanged:
	case EEventType::PlayerInactive:
	case EEventType::PlayerActive:
		SyncSeat(E.Player);
		break;

	case EEventType::PlayerDropped:
		PendingSeatRelease |= static_cast<uint8>(1u << E.Player);
		UE_LOG(LogPicasyFamas, Log, TEXT("Asiento %d: gracia de reconexion agotada, se libera al fin de ronda."), E.Player);
		break;

	case EEventType::Alert:
		GS->ServerSetAlert(E.Player, static_cast<float>(Engine->GetSuddenDeathEndTime()));
		break;

	case EEventType::SuddenDeathStarted:
		GS->SuddenDeathEndServerTime = static_cast<float>(E.Time);
		break;

	case EEventType::KeyClue:
		UE_LOG(LogPicasyFamas, Verbose, TEXT("Pista Clave: asiento %d, seq %d, t=%.3f"), E.Player, E.Seq, E.Time);
		break;

	case EEventType::Solved:
	case EEventType::PhotoFinish:
		GS->LastRoundWinnerMask = Engine->GetWinnerMask();
		break;

	case EEventType::Combo:
		if (APFPlayerController* PC = SeatPC(E.Player)) PC->Client_ShowCombo(static_cast<float>(E.Time), E.Value);
		break;

	case EEventType::Intuition:
		if (APFPlayerController* PC = SeatPC(E.Player)) PC->Client_ShowIntuition();
		break;

	case EEventType::RoundStarted:
		break;

	case EEventType::TurnChanged:
		GS->ServerSetTurn(E.Player, E.Value);   // el reloj del turno viaja en el PlayerState (PlayerStatsChanged)
		break;

	case EEventType::RoundEnded:
	{
		GS->ServerSetTurn(255, Engine->GetTurnNumber());
		GS->RevealedCode = static_cast<int32>(E.Guess);
		GS->LastRoundEndReason = static_cast<EPFRoundEndReason>(E.Value);
		GS->LastRoundWinnerMask = Engine->GetWinnerMask();
		GS->ServerSetAlert(GS->AlertPlayerIndex, 0.f);
		SyncAllSeats();
		GS->ServerSetPhase(EPFRoundPhase::Reward, static_cast<float>(E.Time + Tuning->RewardSeconds));
		UE_LOG(LogPicasyFamas, Log, TEXT("Ronda %d terminada (%d). Ganador: asiento %d. Codigo: %s"),
			GS->RoundIndex + 1, E.Value, E.Player,
			*UPFCodeLibrary::GuessToString(GS->RevealedCode, GS->CodeLength));
		break;
	}
	}
}

// ------------------------------------------------------------------------------------------------
// Utilidades
// ------------------------------------------------------------------------------------------------

double APFGameMode::ServerNow() const
{
	const AGameStateBase* GS = GameState;
	return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

APFGameState* APFGameMode::PFGameState() const
{
	return GetGameState<APFGameState>();
}

APFPlayerState* APFGameMode::SeatPS(uint8 Seat) const
{
	return Seat < PF::kMaxPlayers ? Seats[Seat].Get() : nullptr;
}

APFPlayerController* APFGameMode::SeatPC(uint8 Seat) const
{
	const APFPlayerState* PS = SeatPS(Seat);
	return PS ? Cast<APFPlayerController>(PS->GetPlayerController()) : nullptr;
}

void APFGameMode::SyncSeat(uint8 Seat)
{
	if (APFPlayerState* PS = SeatPS(Seat))
	{
		PS->ServerSyncFromSlot(Engine->GetPlayer(Seat));
	}
}

void APFGameMode::SyncAllSeats()
{
	for (uint8 Seat = 0; Seat < PF::kMaxPlayers; ++Seat) SyncSeat(Seat);
}

void APFGameMode::FillPublicEntry(const PF::FGuessEntry& In, FPFGuessEntry& Out) const
{
	// La vista publica la define el nucleo (PF::MakePublic): sin TrueFamas/TruePicas, sin el bit Decoy,
	// sin KeyClue mientras la ronda esta activa y sin la cuenta atras de un senuelo.
	const PF::FPublicEntry P = PF::MakePublic(In, Engine->IsRoundActive(), /*bMaskDigits*/ false);   // Equipos (futuro): enmascarar al rival
	Out.Seq              = P.Seq;
	Out.PlayerIndex      = P.Player;
	Out.TeamIndex        = P.Team;
	Out.PackedGuess      = static_cast<int32>(P.Guess);
	Out.Famas            = P.Famas;
	Out.Picas            = P.Picas;
	Out.Flags            = P.Flags;
	Out.InfoBitsX10      = P.InfoBitsX10;
	Out.ServerTime       = static_cast<float>(P.ServerTime);
	Out.RevealServerTime = static_cast<float>(P.RevealTime);
}
