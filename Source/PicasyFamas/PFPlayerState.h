// Estado publico de cada jugador (asiento, mejor marca, reloj, fichas) + historial privado del dueno.
// Replicacion Push Model: el servidor marca sucias las propiedades explicitamente tras cada cambio.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PFTypes.h"
#include "PFPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPFOnPlayerStateUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPFOnPrivateAttemptAdded, const FPFPrivateAttempt&, Attempt);

UCLASS()
class PICASYFAMAS_API APFPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	APFPlayerState();

	// ---------- Publico (a todos) ----------
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") uint8 SeatIndex = 255;
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") uint8 TeamIndex = 255;
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") uint8 BestFamas = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") uint8 BestPicas = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") uint8 Attempts = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") int32 RoundScore = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") bool bInactive = false;
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") bool bHasEncryptToken = false;
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") bool bHasDecoyToken = false;
	// Fin del reloj de intento en tiempo de servidor. 0 = reloj parado.
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") float AttemptDeadlineServerTime = 0.f;
	// Espectador: llego a mitad de ronda o fue expulsado por inactividad. Entra en la siguiente ronda si hay asiento.
	UPROPERTY(ReplicatedUsing = OnRep_Public, BlueprintReadOnly, Category = "PicasyFamas") bool bWaitingForSeat = false;

	// El total de partida vive en APlayerState::Score (GetScore / SetScore).

	// ---------- Solo dueno ----------
	UPROPERTY(ReplicatedUsing = OnRep_PrivateHistory, BlueprintReadOnly, Category = "PicasyFamas")
	TArray<FPFPrivateAttempt> PrivateHistory;

	// ---------- Eventos UI ----------
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnPlayerStateUpdated   OnPublicStateUpdated;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnPrivateAttemptAdded  OnPrivateAttemptAdded;

	UFUNCTION(BlueprintPure, Category = "PicasyFamas")
	float GetAttemptSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "PicasyFamas")
	bool HasSeat() const { return SeatIndex != 255; }

	// ---------- Servidor ----------
	void ServerSetSeat(uint8 InSeat, uint8 InTeam);
	void ServerClearSeat();
	void ServerSyncFromSlot(const PF::FPlayerSlot& Slot);
	void ServerAddPrivateAttempt(const FPFPrivateAttempt& Attempt);
	void ServerResetForNewRound();
	void ServerAddMatchScore(int32 Delta);

	// Reconexion: AGameMode conserva el PlayerState 20 s y lo vuelca en el nuevo con OverrideWith.
	virtual void OverrideWith(APlayerState* PlayerState) override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION() void OnRep_Public();
	UFUNCTION() void OnRep_PrivateHistory();

private:
	void MarkPublicDirty();
	int32 LastNotifiedPrivateCount = 0;
};
