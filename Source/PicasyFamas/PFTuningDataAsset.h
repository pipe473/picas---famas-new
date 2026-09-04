// Todas las palancas de balanceo en un solo asset. El servidor lo lee al empezar cada ronda,
// asi que un cambio en el editor (o un hot-reload del asset en el servidor) aplica en la siguiente ronda.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/PFRoundTypes.h"
#include "PFTuningDataAsset.generated.h"

UCLASS(BlueprintType)
class PICASYFAMAS_API UPFTuningDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---------- Sala ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sala", meta = (ClampMin = 2, ClampMax = 8))
	int32 MinPlayersToStart = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sala", meta = (ClampMin = 2, ClampMax = 8))
	int32 MaxPlayers = 8;

	// A partir de este numero de jugadores el codigo pasa a 5 digitos.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sala", meta = (ClampMin = 3, ClampMax = 9))
	int32 PlayersForFiveDigits = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sala", meta = (ClampMin = 1, ClampMax = 9))
	int32 RoundsPerMatch = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sala", meta = (ClampMin = 0.0))
	float LobbyCountdownSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sala", meta = (ClampMin = 0.0))
	float RewardSeconds = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sala", meta = (ClampMin = 0.0))
	float MatchEndSeconds = 15.f;

	// ---------- Ronda ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ronda", meta = (ClampMin = 3.0))
	float AttemptSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ronda", meta = (ClampMin = 2.0))
	float SuddenDeathAttemptSeconds = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ronda", meta = (ClampMin = 5.0))
	float SuddenDeathSeconds = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ronda", meta = (ClampMin = 30.0))
	float RoundCapSeconds = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ronda", meta = (ClampMin = 0.0))
	float MinAttemptInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ronda", meta = (ClampMin = 0, ClampMax = 10))
	int32 PassesUntilInactive = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ronda", meta = (ClampMin = 0.0))
	float ReconnectGraceSeconds = 20.f;

	// ---------- Red / justicia ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red", meta = (ClampMin = 0.0, ClampMax = 0.5))
	float MaxLatencyCompensationSeconds = 0.100f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red", meta = (ClampMin = 0.0, ClampMax = 0.05))
	float PhotoFinishEpsilonSeconds = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DeadlineGraceSeconds = 0.150f;

	// ---------- Faroleo ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faroleo", meta = (ClampMin = 0.0))
	float HideSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faroleo", meta = (ClampMin = 0.0))
	float DecoySeconds = 8.f;

	// ---------- Puntuacion ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 SolvePoints = 100;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 LateSolvePoints = 50;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 PositionWinPoints = 40;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 LightningDeductionPoints = 50;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 FastDeductionPoints = 25;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 PointsPerBit = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 KeyCluePoints = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 DecoyEffectivePoints = 15;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 DecoyCaughtPoints = -30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 SuspicionHitPoints = 15;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 SuspicionMissPoints = -10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion") int32 PassPoints = -5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion", meta = (ClampMin = 0.0)) float LightningWindowSeconds = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puntuacion", meta = (ClampMin = 0.0)) float FastWindowSeconds = 6.f;

	int32 CodeLengthForPlayers(int32 NumPlayers) const
	{
		return NumPlayers >= PlayersForFiveDigits ? 5 : 4;
	}

	PF::FRoundConfig ToRoundConfig(int32 NumPlayers, bool bTeamMode) const;
};
