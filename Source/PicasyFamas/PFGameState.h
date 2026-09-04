// Estado publico de la partida, replicado a todos los clientes.
// El historial global de intentos (Modulo del Enigma) es un FFastArraySerializer: cuando se revela un
// Encriptar o se corrige un Senuelo viaja SOLO ese elemento, no el array completo.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "PFTypes.h"
#include "PFGameState.generated.h"

class APFGameState;

USTRUCT(BlueprintType)
struct FPFGuessEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Seq = 0;                 // id monotono; clave de UI y de Sospechar
	UPROPERTY(BlueprintReadOnly) uint8 PlayerIndex = 255;       // asiento (0..7)
	UPROPERTY(BlueprintReadOnly) uint8 TeamIndex = 255;
	UPROPERTY(BlueprintReadOnly) int32 PackedGuess = 0;         // digitos empaquetados; -1 (0xFFFFFFFF) si enmascarado
	UPROPERTY(BlueprintReadOnly) uint8 Famas = 0;               // valores PUBLICOS (0 si oculto; falsos si senuelo)
	UPROPERTY(BlueprintReadOnly) uint8 Picas = 0;
	UPROPERTY(BlueprintReadOnly) uint8 Flags = 0;               // EPFGuessFlags
	UPROPERTY(BlueprintReadOnly) uint8 InfoBitsX10 = 0;         // bits de informacion x10 (0 hasta revelar)
	UPROPERTY(BlueprintReadOnly) float ServerTime = 0.f;
	UPROPERTY(BlueprintReadOnly) float RevealServerTime = 0.f;  // > 0 mientras hay una revelacion pendiente

	// Callbacks de FastArray en cliente (animar entrada nueva / correccion in situ).
	void PostReplicatedAdd(const struct FPFGuessHistory& InArraySerializer);
	void PostReplicatedChange(const struct FPFGuessHistory& InArraySerializer);
};

USTRUCT(BlueprintType)
struct FPFGuessHistory : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TArray<FPFGuessEntry> Items;

	UPROPERTY(NotReplicated) TObjectPtr<APFGameState> Owner = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FPFGuessEntry, FPFGuessHistory>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FPFGuessHistory> : public TStructOpsTypeTraitsBase2<FPFGuessHistory>
{
	enum { WithNetDeltaSerializer = true };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPFOnGuessEntryEvent, int32, Seq);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPFOnPhaseChanged, EPFRoundPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPFOnAlert, uint8, PlayerIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPFOnHistoryReset);

UCLASS()
class PICASYFAMAS_API APFGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	APFGameState();

	// ---------- Replicado ----------
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	FPFGuessHistory GuessHistory;

	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "PicasyFamas")
	EPFRoundPhase Phase = EPFRoundPhase::Lobby;

	// Fin de la fase actual (Countdown / Reward / MatchEnd) en tiempo de servidor. 0 si no aplica.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	float PhaseEndServerTime = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	uint8 RoundIndex = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	uint8 NumRounds = 3;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	uint8 CodeLength = 4;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	bool bTeamMode = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	float RoundStartServerTime = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	float RoundCapServerTime = 0.f;

	// 0 = Muerte Sudada inactiva.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	float SuddenDeathEndServerTime = 0.f;

	// Jugador que disparo la alerta de N-1 Famas. 255 = ninguno.
	UPROPERTY(ReplicatedUsing = OnRep_Alert, BlueprintReadOnly, Category = "PicasyFamas")
	uint8 AlertPlayerIndex = 255;

	// Codigo revelado al terminar la ronda. -1 mientras la ronda esta activa.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	int32 RevealedCode = -1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	EPFRoundEndReason LastRoundEndReason = EPFRoundEndReason::None;

	// Bitmask de asientos ganadores de la ultima ronda (foto-finish => varios bits).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PicasyFamas")
	uint8 LastRoundWinnerMask = 0;

	// ---------- Eventos para UI ----------
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnGuessEntryEvent OnGuessEntryAdded;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnGuessEntryEvent OnGuessEntryChanged;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnPhaseChanged    OnPhaseChanged;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnAlert           OnAlertChanged;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnHistoryReset    OnHistoryReset;

	// ---------- Consultas (cliente y servidor) ----------
	UFUNCTION(BlueprintPure, Category = "PicasyFamas")
	bool FindEntry(int32 Seq, FPFGuessEntry& OutEntry) const;

	UFUNCTION(BlueprintPure, Category = "PicasyFamas")
	float GetSecondsRemainingInPhase() const;

	UFUNCTION(BlueprintPure, Category = "PicasyFamas")
	float GetSuddenDeathSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "PicasyFamas")
	class APFPlayerState* FindPlayerStateBySeat(uint8 SeatIndex) const;

	// ---------- Solo servidor ----------
	void ServerResetHistory();
	FPFGuessEntry& ServerAddEntry(const FPFGuessEntry& Entry);
	FPFGuessEntry* ServerFindEntryMutable(int32 Seq);
	void ServerMarkEntryDirty(FPFGuessEntry& Entry);
	void ServerSetPhase(EPFRoundPhase NewPhase, float PhaseEnd);
	void ServerSetAlert(uint8 PlayerIndex, float SuddenDeathEnd);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION() void OnRep_Phase();
	UFUNCTION() void OnRep_Alert();
};
