// Autoridad de la partida. Adaptador fino sobre PF::FRoundEngine:
//   RPCs -> Engine.Enqueue / Suspect / NotePlayerAction
//   Tick -> Engine.Tick
//   eventos del motor -> replicacion (GameState / PlayerState) y Client RPCs
// Ademas gestiona asientos, fases (Lobby -> Countdown -> Playing -> Reward -> ... -> MatchEnd),
// reconexion (via AGameMode::InactivePlayerArray) y late join entre rondas.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/PFRoundTypes.h"
#include "Core/PFRoundEngine.h"
#include "PFTypes.h"
#include "PFGameMode.generated.h"

class APFGameState;
class APFPlayerState;
class APFPlayerController;
class UPFTuningDataAsset;
struct FPFGuessEntry;

// Puente no-UObject entre el motor de ronda y el GameMode (UHT no admite bases con namespace en UCLASS).
class FPFRoundListenerBridge final : public PF::IRoundListener
{
public:
	explicit FPFRoundListenerBridge(class APFGameMode* InOwner) : Owner(InOwner) {}
	virtual void OnRoundEvent(const PF::FRoundEvent& Event) override;
private:
	class APFGameMode* Owner;
};

UCLASS()
class PICASYFAMAS_API APFGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	APFGameMode();

	// Asset de balanceo. Si es nulo se usan los valores por defecto de UPFTuningDataAsset.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PicasyFamas")
	TObjectPtr<UPFTuningDataAsset> Tuning;

	// 0 = usar Tuning->MinPlayersToStart. Util para probar en PIE con 1-2 jugadores.
	UPROPERTY(EditDefaultsOnly, Category = "PicasyFamas|Debug")
	int32 MinPlayersOverride = 0;

	// ---------- Entrada desde APFPlayerController (servidor) ----------
	void HandleSubmitGuess(APFPlayerController* PC, PF::PackedCode Guess, uint8 Flags, uint8 DecoyFamas, uint8 DecoyPicas);
	void HandleSuspect(APFPlayerController* PC, int32 Seq);
	void HandleNoteAction(APFPlayerController* PC);

	// ---------- Consola (servidor / PIE) ----------
	UFUNCTION(Exec) void PFForceStart();
	UFUNCTION(Exec) void PFEndRound();

	// ---------- AGameMode ----------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual bool ReadyToStartMatch_Implementation() override { return false; }   // lo controla nuestra FSM
	virtual bool ReadyToEndMatch_Implementation() override { return false; }

	// ---------- Eventos del motor (via FPFRoundListenerBridge) ----------
	void OnRoundEvent(const PF::FRoundEvent& Event);

protected:
	// Fases
	void UpdateLobby(double Now);
	void UpdateCountdown(double Now);
	void UpdatePlaying(double Now);
	void UpdateReward(double Now);
	void UpdateMatchEnd(double Now);
	void StartRound(double Now);
	void EnterLobby(double Now);
	void ResetMatchScores();

	// Asientos
	int32 FindFreeSeat() const;
	bool  AssignSeat(APFPlayerState* PS);
	void  ReleaseSeat(uint8 Seat);
	void  SeatWaitingPlayers();
	void  ReleaseDroppedSeats();
	int32 CountSeatedConnected() const;
	int32 EffectiveMinPlayers() const;

	// Utilidades
	double ServerNow() const;
	APFGameState*        PFGameState() const;
	APFPlayerState*      SeatPS(uint8 Seat) const;
	APFPlayerController* SeatPC(uint8 Seat) const;
	void SyncSeat(uint8 Seat);
	void SyncAllSeats();
	void FillPublicEntry(const PF::FGuessEntry& In, FPFGuessEntry& Out) const;

private:
	TUniquePtr<FPFRoundListenerBridge> ListenerBridge;
	TUniquePtr<PF::FRoundEngine> Engine;
	TWeakObjectPtr<APFPlayerState> Seats[PF::kMaxPlayers];
	uint8 PendingSeatRelease = 0;   // bitmask: asientos de jugadores Dropped, se liberan al fin de ronda
	bool  bMatchStarted = false;
};
