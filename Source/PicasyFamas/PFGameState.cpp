#include "PFGameState.h"
#include "PFPlayerState.h"
#include "Net/UnrealNetwork.h"

// ------------------------------------------------------------------------------------------------
// FastArray callbacks (cliente)
// ------------------------------------------------------------------------------------------------

void FPFGuessEntry::PostReplicatedAdd(const FPFGuessHistory& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->OnGuessEntryAdded.Broadcast(Seq);
	}
}

void FPFGuessEntry::PostReplicatedChange(const FPFGuessHistory& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->OnGuessEntryChanged.Broadcast(Seq);
	}
}

// ------------------------------------------------------------------------------------------------

APFGameState::APFGameState()
{
	GuessHistory.Owner = this;
	NetUpdateFrequency = 30.f;
	MinNetUpdateFrequency = 10.f;
}

void APFGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APFGameState, GuessHistory);
	DOREPLIFETIME(APFGameState, Phase);
	DOREPLIFETIME(APFGameState, PhaseEndServerTime);
	DOREPLIFETIME(APFGameState, RoundIndex);
	DOREPLIFETIME(APFGameState, NumRounds);
	DOREPLIFETIME(APFGameState, CodeLength);
	DOREPLIFETIME(APFGameState, bTeamMode);
	DOREPLIFETIME(APFGameState, RoundStartServerTime);
	DOREPLIFETIME(APFGameState, RoundCapServerTime);
	DOREPLIFETIME(APFGameState, SuddenDeathEndServerTime);
	DOREPLIFETIME(APFGameState, AlertPlayerIndex);
	DOREPLIFETIME(APFGameState, TurnMode);
	DOREPLIFETIME(APFGameState, TurnPlayerIndex);
	DOREPLIFETIME(APFGameState, TurnNumber);
	DOREPLIFETIME(APFGameState, TurnOrder);
	DOREPLIFETIME(APFGameState, RevealedCode);
	DOREPLIFETIME(APFGameState, LastRoundEndReason);
	DOREPLIFETIME(APFGameState, LastRoundWinnerMask);
}

// ------------------------------------------------------------------------------------------------
// Consultas
// ------------------------------------------------------------------------------------------------

bool APFGameState::FindEntry(int32 Seq, FPFGuessEntry& OutEntry) const
{
	for (int32 i = GuessHistory.Items.Num() - 1; i >= 0; --i)
	{
		if (GuessHistory.Items[i].Seq == Seq)
		{
			OutEntry = GuessHistory.Items[i];
			return true;
		}
	}
	return false;
}

float APFGameState::GetSecondsRemainingInPhase() const
{
	if (PhaseEndServerTime <= 0.f) return 0.f;
	return FMath::Max(0.f, PhaseEndServerTime - static_cast<float>(GetServerWorldTimeSeconds()));
}

float APFGameState::GetSuddenDeathSecondsRemaining() const
{
	if (SuddenDeathEndServerTime <= 0.f) return 0.f;
	return FMath::Max(0.f, SuddenDeathEndServerTime - static_cast<float>(GetServerWorldTimeSeconds()));
}

APFPlayerState* APFGameState::FindPlayerStateBySeat(uint8 SeatIndex) const
{
	for (APlayerState* PS : PlayerArray)
	{
		APFPlayerState* PFPS = Cast<APFPlayerState>(PS);
		if (PFPS && PFPS->SeatIndex == SeatIndex)
		{
			return PFPS;
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
// Servidor
// ------------------------------------------------------------------------------------------------

void APFGameState::ServerResetHistory()
{
	GuessHistory.Items.Reset();
	GuessHistory.MarkArrayDirty();
}

FPFGuessEntry& APFGameState::ServerAddEntry(const FPFGuessEntry& Entry)
{
	FPFGuessEntry& Added = GuessHistory.Items.Add_GetRef(Entry);
	GuessHistory.MarkItemDirty(Added);
	// En el servidor (o en standalone) no hay PostReplicatedAdd: notificamos a mano para la UI local.
	OnGuessEntryAdded.Broadcast(Added.Seq);
	return Added;
}

FPFGuessEntry* APFGameState::ServerFindEntryMutable(int32 Seq)
{
	for (int32 i = GuessHistory.Items.Num() - 1; i >= 0; --i)
	{
		if (GuessHistory.Items[i].Seq == Seq) return &GuessHistory.Items[i];
	}
	return nullptr;
}

void APFGameState::ServerMarkEntryDirty(FPFGuessEntry& Entry)
{
	GuessHistory.MarkItemDirty(Entry);
	OnGuessEntryChanged.Broadcast(Entry.Seq);
}

void APFGameState::ServerSetPhase(EPFRoundPhase NewPhase, float PhaseEnd)
{
	const bool bChanged = (Phase != NewPhase);
	Phase = NewPhase;
	PhaseEndServerTime = PhaseEnd;
	if (bChanged)
	{
		OnRep_Phase();   // servidor listen/standalone
	}
}

void APFGameState::ServerSetAlert(uint8 PlayerIndex, float SuddenDeathEnd)
{
	const bool bChanged = (AlertPlayerIndex != PlayerIndex);
	AlertPlayerIndex = PlayerIndex;
	SuddenDeathEndServerTime = SuddenDeathEnd;
	if (bChanged)
	{
		OnRep_Alert();   // servidor listen/standalone
	}
}

// ------------------------------------------------------------------------------------------------
// OnRep
// ------------------------------------------------------------------------------------------------

void APFGameState::OnRep_Phase()
{
	if (Phase == EPFRoundPhase::Playing)
	{
		OnHistoryReset.Broadcast();   // ronda nueva: el Modulo del Enigma empieza vacio
	}
	OnPhaseChanged.Broadcast(Phase);
}

void APFGameState::OnRep_Alert()
{
	OnAlertChanged.Broadcast(AlertPlayerIndex);
}

void APFGameState::ServerSetTurn(uint8 PlayerIndex, int32 InTurnNumber)
{
	const bool bChanged = (TurnPlayerIndex != PlayerIndex);
	TurnPlayerIndex = PlayerIndex;
	TurnNumber = InTurnNumber;
	if (bChanged)
	{
		OnRep_Turn();   // servidor listen/standalone
	}
}

void APFGameState::OnRep_Turn()
{
	OnTurnChanged.Broadcast(TurnPlayerIndex);
}
