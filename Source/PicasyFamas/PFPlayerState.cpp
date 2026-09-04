#include "PFPlayerState.h"
#include "PFGameState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

APFPlayerState::APFPlayerState()
{
	// Indicadores de tension: prioridad alta de replicacion durante la ronda.
	NetUpdateFrequency = 30.f;
	MinNetUpdateFrequency = 10.f;
}

void APFPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Push;
	Push.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, SeatIndex, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, TeamIndex, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, BestFamas, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, BestPicas, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, Attempts, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, RoundScore, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, bInactive, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, bHasEncryptToken, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, bHasDecoyToken, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, AttemptDeadlineServerTime, Push);
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, bWaitingForSeat, Push);

	FDoRepLifetimeParams OwnerOnly = Push;
	OwnerOnly.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(APFPlayerState, PrivateHistory, OwnerOnly);
}

float APFPlayerState::GetAttemptSecondsRemaining() const
{
	if (AttemptDeadlineServerTime <= 0.f) return 0.f;
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS) return 0.f;
	return FMath::Max(0.f, AttemptDeadlineServerTime - static_cast<float>(GS->GetServerWorldTimeSeconds()));
}

// ------------------------------------------------------------------------------------------------
// Servidor
// ------------------------------------------------------------------------------------------------

void APFPlayerState::ServerSetSeat(uint8 InSeat, uint8 InTeam)
{
	SeatIndex = InSeat;
	TeamIndex = InTeam;
	bWaitingForSeat = false;
	MarkPublicDirty();
}

void APFPlayerState::ServerClearSeat()
{
	SeatIndex = 255;
	TeamIndex = 255;
	bWaitingForSeat = true;
	AttemptDeadlineServerTime = 0.f;
	MarkPublicDirty();
}

void APFPlayerState::ServerSyncFromSlot(const PF::FPlayerSlot& Slot)
{
	BestFamas = Slot.BestFamas;
	BestPicas = Slot.BestPicas;
	Attempts = Slot.Attempts;
	RoundScore = Slot.RoundScore;
	bInactive = Slot.bInactive;
	bHasEncryptToken = Slot.bHasEncryptToken;
	bHasDecoyToken = Slot.bHasDecoyToken;
	AttemptDeadlineServerTime = static_cast<float>(Slot.AttemptDeadline);
	MarkPublicDirty();
}

void APFPlayerState::ServerAddPrivateAttempt(const FPFPrivateAttempt& Attempt)
{
	PrivateHistory.Add(Attempt);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, PrivateHistory, this);
	OnRep_PrivateHistory();   // listen server / standalone
}

void APFPlayerState::ServerResetForNewRound()
{
	BestFamas = 0;
	BestPicas = 0;
	Attempts = 0;
	RoundScore = 0;
	bInactive = false;
	bHasEncryptToken = true;
	bHasDecoyToken = true;
	AttemptDeadlineServerTime = 0.f;
	PrivateHistory.Reset();
	LastNotifiedPrivateCount = 0;
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, PrivateHistory, this);
	MarkPublicDirty();
}

void APFPlayerState::ServerAddMatchScore(int32 Delta)
{
	SetScore(GetScore() + static_cast<float>(Delta));
}

void APFPlayerState::MarkPublicDirty()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, SeatIndex, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, TeamIndex, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, BestFamas, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, BestPicas, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, Attempts, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, RoundScore, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, bInactive, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, bHasEncryptToken, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, bHasDecoyToken, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, AttemptDeadlineServerTime, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, bWaitingForSeat, this);
	OnRep_Public();   // listen server / standalone
}

// ------------------------------------------------------------------------------------------------
// Reconexion / seamless travel
// ------------------------------------------------------------------------------------------------

void APFPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);
	if (const APFPlayerState* Other = Cast<APFPlayerState>(PlayerState))
	{
		SeatIndex = Other->SeatIndex;
		TeamIndex = Other->TeamIndex;
		BestFamas = Other->BestFamas;
		BestPicas = Other->BestPicas;
		Attempts = Other->Attempts;
		RoundScore = Other->RoundScore;
		bInactive = Other->bInactive;
		bHasEncryptToken = Other->bHasEncryptToken;
		bHasDecoyToken = Other->bHasDecoyToken;
		bWaitingForSeat = Other->bWaitingForSeat;
		PrivateHistory = Other->PrivateHistory;
		SetScore(Other->GetScore());
		MARK_PROPERTY_DIRTY_FROM_NAME(APFPlayerState, PrivateHistory, this);
		MarkPublicDirty();
	}
}

void APFPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (APFPlayerState* Other = Cast<APFPlayerState>(PlayerState))
	{
		Other->SeatIndex = SeatIndex;
		Other->TeamIndex = TeamIndex;
		Other->bWaitingForSeat = bWaitingForSeat;
		Other->SetScore(GetScore());
		Other->MarkPublicDirty();
	}
}

// ------------------------------------------------------------------------------------------------
// OnRep
// ------------------------------------------------------------------------------------------------

void APFPlayerState::OnRep_Public()
{
	OnPublicStateUpdated.Broadcast();
}

void APFPlayerState::OnRep_PrivateHistory()
{
	// Notifica solo las entradas nuevas (el array llega completo pero solo cambia por append o reset).
	if (PrivateHistory.Num() < LastNotifiedPrivateCount) LastNotifiedPrivateCount = 0;
	for (int32 i = LastNotifiedPrivateCount; i < PrivateHistory.Num(); ++i)
	{
		OnPrivateAttemptAdded.Broadcast(PrivateHistory[i]);
	}
	LastNotifiedPrivateCount = PrivateHistory.Num();
}
