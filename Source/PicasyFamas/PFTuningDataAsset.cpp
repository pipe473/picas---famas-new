#include "PFTuningDataAsset.h"

PF::FRoundConfig UPFTuningDataAsset::ToRoundConfig(int32 NumPlayers, bool bTeamMode) const
{
	PF::FRoundConfig C;
	C.CodeLength                = CodeLengthForPlayers(NumPlayers);
	C.AttemptSeconds            = AttemptSeconds;
	C.SuddenDeathAttemptSeconds = SuddenDeathAttemptSeconds;
	C.SuddenDeathSeconds        = SuddenDeathSeconds;
	C.RoundCapSeconds           = RoundCapSeconds;
	C.MinAttemptInterval        = MinAttemptInterval;
	C.MaxLatencyCompensation    = MaxLatencyCompensationSeconds;
	C.PhotoFinishEpsilon        = PhotoFinishEpsilonSeconds;
	C.DeadlineGrace             = DeadlineGraceSeconds;
	C.HideSeconds               = HideSeconds;
	C.DecoySeconds              = DecoySeconds;
	C.ReconnectGraceSeconds     = ReconnectGraceSeconds;
	C.PassesUntilInactive       = static_cast<uint8_t>(FMath::Clamp(PassesUntilInactive, 0, 255));
	C.bTeamMode                 = bTeamMode;

	PF::FScoringConfig& S = C.Scoring;
	S.Solve              = SolvePoints;
	S.LateSolve          = LateSolvePoints;
	S.PositionWin        = PositionWinPoints;
	S.LightningDeduction = LightningDeductionPoints;
	S.FastDeduction      = FastDeductionPoints;
	S.PointsPerBit       = PointsPerBit;
	S.KeyClue            = KeyCluePoints;
	S.DecoyEffective     = DecoyEffectivePoints;
	S.DecoyCaught        = DecoyCaughtPoints;
	S.SuspicionHit       = SuspicionHitPoints;
	S.SuspicionMiss      = SuspicionMissPoints;
	S.Pass               = PassPoints;
	S.LightningWindow    = LightningWindowSeconds;
	S.FastWindow         = FastWindowSeconds;
	return C;
}
