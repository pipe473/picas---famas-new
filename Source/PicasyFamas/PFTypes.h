// Tipos expuestos a Blueprint/UMG. Espejan los del nucleo (Core/PFRoundTypes.h) con static_asserts
// para que no se desincronicen.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/PFRoundTypes.h"
#include "PFTypes.generated.h"

UENUM(BlueprintType)
enum class EPFRoundPhase : uint8
{
	Lobby       UMETA(DisplayName = "Lobby"),
	Countdown   UMETA(DisplayName = "Cuenta atras"),
	Playing     UMETA(DisplayName = "Jugando"),
	Reward      UMETA(DisplayName = "Resumen de ronda"),
	MatchEnd    UMETA(DisplayName = "Fin de partida"),
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EPFGuessFlags : uint8
{
	None          = 0            UMETA(Hidden),
	ResultHidden  = 1 << 0       UMETA(DisplayName = "Encriptado"),
	Decoy         = 1 << 1       UMETA(DisplayName = "Senuelo activo"),
	DecoyRevealed = 1 << 2       UMETA(DisplayName = "Senuelo corregido"),
	Suspected     = 1 << 3       UMETA(DisplayName = "Sospechado"),
	KeyClue       = 1 << 4       UMETA(DisplayName = "Pista clave"),
	Solved        = 1 << 5       UMETA(DisplayName = "Acierto"),
	PhotoFinish   = 1 << 6       UMETA(DisplayName = "Foto-finish"),
	LateSolve     = 1 << 7       UMETA(DisplayName = "Acierto tardio"),
};
ENUM_CLASS_FLAGS(EPFGuessFlags);

static_assert(static_cast<uint8>(EPFGuessFlags::ResultHidden)  == PF::GuessFlags::ResultHidden,  "flags desincronizados");
static_assert(static_cast<uint8>(EPFGuessFlags::Decoy)         == PF::GuessFlags::Decoy,         "flags desincronizados");
static_assert(static_cast<uint8>(EPFGuessFlags::DecoyRevealed) == PF::GuessFlags::DecoyRevealed, "flags desincronizados");
static_assert(static_cast<uint8>(EPFGuessFlags::Suspected)     == PF::GuessFlags::Suspected,     "flags desincronizados");
static_assert(static_cast<uint8>(EPFGuessFlags::KeyClue)       == PF::GuessFlags::KeyClue,       "flags desincronizados");
static_assert(static_cast<uint8>(EPFGuessFlags::Solved)        == PF::GuessFlags::Solved,        "flags desincronizados");
static_assert(static_cast<uint8>(EPFGuessFlags::PhotoFinish)   == PF::GuessFlags::PhotoFinish,   "flags desincronizados");
static_assert(static_cast<uint8>(EPFGuessFlags::LateSolve)     == PF::GuessFlags::LateSolve,     "flags desincronizados");

UENUM(BlueprintType)
enum class EPFRejectReason : uint8
{
	None = 0,
	RoundNotActive,
	UnknownPlayer,
	PlayerDisconnected,
	InvalidGuess,
	TooFast,
	DeadlineExpired,
	RoundOver,
	HistoryFull,
};
static_assert(static_cast<uint8>(EPFRejectReason::HistoryFull) == static_cast<uint8>(PF::ERejectReason::HistoryFull), "enum desincronizado");

UENUM(BlueprintType)
enum class EPFScoreReason : uint8
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
static_assert(static_cast<uint8>(EPFScoreReason::Pass) == static_cast<uint8>(PF::EScoreReason::Pass), "enum desincronizado");

UENUM(BlueprintType)
enum class EPFRoundEndReason : uint8
{
	None = 0,
	Solved,
	SuddenDeathExpired,
	TimeCap,
	NotEnoughPlayers,
	Aborted,
};
static_assert(static_cast<uint8>(EPFRoundEndReason::Aborted) == static_cast<uint8>(PF::ERoundEndReason::Aborted), "enum desincronizado");

// Intento privado del dueno: digitos + resultado real (aunque publicamente este encriptado o sea senuelo).
USTRUCT(BlueprintType)
struct FPFPrivateAttempt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Seq = 0;
	UPROPERTY(BlueprintReadOnly) int32 PackedGuess = 0;
	UPROPERTY(BlueprintReadOnly) uint8 Famas = 0;
	UPROPERTY(BlueprintReadOnly) uint8 Picas = 0;
	UPROPERTY(BlueprintReadOnly) float ServerTime = 0.f;
};

// Utilidades Blueprint para (des)empaquetar digitos.
UCLASS()
class UPFCodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "PicasyFamas|Code")
	static int32 PackDigits(const TArray<uint8>& Digits);

	UFUNCTION(BlueprintPure, Category = "PicasyFamas|Code")
	static TArray<uint8> UnpackDigits(int32 PackedGuess, int32 CodeLength);

	UFUNCTION(BlueprintPure, Category = "PicasyFamas|Code")
	static bool IsValidGuess(int32 PackedGuess, int32 CodeLength);

	UFUNCTION(BlueprintPure, Category = "PicasyFamas|Code")
	static bool IsMaskedGuess(int32 PackedGuess) { return static_cast<uint32>(PackedGuess) == PF::kMaskedCode; }

	UFUNCTION(BlueprintPure, Category = "PicasyFamas|Code")
	static bool HasFlag(uint8 Flags, EPFGuessFlags Flag) { return (Flags & static_cast<uint8>(Flag)) != 0; }

	UFUNCTION(BlueprintPure, Category = "PicasyFamas|Code")
	static FString GuessToString(int32 PackedGuess, int32 CodeLength);
};
