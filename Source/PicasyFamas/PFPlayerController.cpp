#include "PFPlayerController.h"
#include "PFGameMode.h"
#include "PFGameState.h"
#include "Core/PFCodeMath.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"

APFPlayerController::APFPlayerController()
{
	bShowMouseCursor = true;   // UI de teclado en pantalla
}

// ------------------------------------------------------------------------------------------------
// API cliente
// ------------------------------------------------------------------------------------------------

void APFPlayerController::SubmitGuess(const TArray<uint8>& Digits, bool bEncrypt, bool bDecoy, uint8 DecoyFamas, uint8 DecoyPicas)
{
	const APFGameState* GS = GetWorld() ? GetWorld()->GetGameState<APFGameState>() : nullptr;
	const int32 Len = GS ? GS->CodeLength : 4;

	if (Digits.Num() != Len)
	{
		OnGuessRejected.Broadcast(EPFRejectReason::InvalidGuess);
		return;
	}

	const int32 Packed = UPFCodeLibrary::PackDigits(Digits);
	if (!PF::IsValidCode(static_cast<PF::PackedCode>(Packed), Len))
	{
		OnGuessRejected.Broadcast(EPFRejectReason::InvalidGuess);   // feedback inmediato sin viaje al servidor
		return;
	}

	uint8 Flags = PF::GuessFlags::None;
	if (bEncrypt) Flags |= PF::GuessFlags::ResultHidden;
	if (bDecoy)   Flags |= PF::GuessFlags::Decoy;

	Server_SubmitGuess(Packed, Flags, DecoyFamas, DecoyPicas);
}

void APFPlayerController::SuspectEntry(int32 Seq)
{
	Server_Suspect(Seq);
}

void APFPlayerController::NotifyBoardAction()
{
	const double Now = FPlatformTime::Seconds();
	if (Now - LastNoteActionTime < 1.0) return;   // como mucho una por segundo
	LastNoteActionTime = Now;
	Server_NoteAction();
}

float APFPlayerController::GetServerMeasuredRTT() const
{
	if (const UNetConnection* Conn = GetNetConnection())
	{
		return static_cast<float>(Conn->AvgLag);
	}
	return 0.f;   // listen server / standalone
}

// ------------------------------------------------------------------------------------------------
// Server RPC
// ------------------------------------------------------------------------------------------------

bool APFPlayerController::Server_SubmitGuess_Validate(int32 PackedGuess, uint8 Flags, uint8 DecoyFamas, uint8 DecoyPicas)
{
	// Basura estructural => desconexion (RPC validation). Los rechazos de reglas van por el motor.
	if ((Flags & ~PF::kClientRequestableFlags) != 0) return false;
	if (DecoyFamas > PF::kMaxCodeLength || DecoyPicas > PF::kMaxCodeLength) return false;
	if ((static_cast<uint32>(PackedGuess) >> (4 * PF::kMaxCodeLength)) != 0) return false;
	return true;
}

void APFPlayerController::Server_SubmitGuess_Implementation(int32 PackedGuess, uint8 Flags, uint8 DecoyFamas, uint8 DecoyPicas)
{
	if (APFGameMode* GM = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		GM->HandleSubmitGuess(this, static_cast<PF::PackedCode>(PackedGuess), Flags, DecoyFamas, DecoyPicas);
	}
}

bool APFPlayerController::Server_Suspect_Validate(int32 Seq)
{
	return Seq > 0;
}

void APFPlayerController::Server_Suspect_Implementation(int32 Seq)
{
	if (APFGameMode* GM = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		GM->HandleSuspect(this, Seq);
	}
}

void APFPlayerController::Server_NoteAction_Implementation()
{
	if (APFGameMode* GM = GetWorld()->GetAuthGameMode<APFGameMode>())
	{
		GM->HandleNoteAction(this);
	}
}

// ------------------------------------------------------------------------------------------------
// Client RPC
// ------------------------------------------------------------------------------------------------

void APFPlayerController::Client_GuessResult_Implementation(int32 Seq, int32 PackedGuess, uint8 Famas, uint8 Picas)
{
	OnGuessResult.Broadcast(Seq, PackedGuess, Famas, Picas);
}

void APFPlayerController::Client_GuessRejected_Implementation(EPFRejectReason Reason)
{
	OnGuessRejected.Broadcast(Reason);
}

void APFPlayerController::Client_ShowCombo_Implementation(float SecondsSinceKeyClue, int32 Bonus)
{
	OnCombo.Broadcast(SecondsSinceKeyClue, Bonus);
}

void APFPlayerController::Client_ShowIntuition_Implementation()
{
	OnIntuition.Broadcast();
}

void APFPlayerController::Client_ShowScore_Implementation(EPFScoreReason Reason, int32 Delta)
{
	OnScore.Broadcast(Reason, Delta);
}
