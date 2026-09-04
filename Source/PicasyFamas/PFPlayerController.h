// Entrada del jugador. Los Server RPC solo encolan en el motor de ronda: nada se decide aqui.
// Los Client RPC llevan lo privado (resultado real del intento) y lo cosmetico (combos, rechazos).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PFTypes.h"
#include "PFPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FPFOnGuessResult, int32, Seq, int32, PackedGuess, uint8, Famas, uint8, Picas);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPFOnGuessRejected, EPFRejectReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPFOnCombo, float, SecondsSinceKeyClue, int32, Bonus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPFOnIntuition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPFOnScore, EPFScoreReason, Reason, int32, Delta);

UCLASS()
class PICASYFAMAS_API APFPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APFPlayerController();

	// ---------- API para UMG (cliente) ----------

	// Envia un intento. Digits: CodeLength digitos 0-9 sin repetir. bEncrypt/bDecoy consumen ficha si la hay.
	UFUNCTION(BlueprintCallable, Category = "PicasyFamas")
	void SubmitGuess(const TArray<uint8>& Digits, bool bEncrypt, bool bDecoy, uint8 DecoyFamas, uint8 DecoyPicas);

	UFUNCTION(BlueprintCallable, Category = "PicasyFamas")
	void SuspectEntry(int32 Seq);

	// Marcar la Pizarra, etc.: mantiene al jugador Activo sin enviar intento.
	UFUNCTION(BlueprintCallable, Category = "PicasyFamas")
	void NotifyBoardAction();

	// ---------- Eventos UI ----------
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnGuessResult   OnGuessResult;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnGuessRejected OnGuessRejected;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnCombo         OnCombo;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnIntuition     OnIntuition;
	UPROPERTY(BlueprintAssignable, Category = "PicasyFamas|Eventos") FPFOnScore         OnScore;

	// ---------- Client RPC (los invoca el GameMode) ----------
	UFUNCTION(Client, Reliable)   void Client_GuessResult(int32 Seq, int32 PackedGuess, uint8 Famas, uint8 Picas);
	UFUNCTION(Client, Reliable)   void Client_GuessRejected(EPFRejectReason Reason);
	UFUNCTION(Client, Unreliable) void Client_ShowCombo(float SecondsSinceKeyClue, int32 Bonus);
	UFUNCTION(Client, Unreliable) void Client_ShowIntuition();
	UFUNCTION(Client, Unreliable) void Client_ShowScore(EPFScoreReason Reason, int32 Delta);

	// RTT medido por el servidor (nunca confiar en un valor enviado por el cliente).
	float GetServerMeasuredRTT() const;

protected:
	// ---------- Server RPC ----------
	UFUNCTION(Server, Reliable, WithValidation) void Server_SubmitGuess(int32 PackedGuess, uint8 Flags, uint8 DecoyFamas, uint8 DecoyPicas);
	UFUNCTION(Server, Reliable, WithValidation) void Server_Suspect(int32 Seq);
	UFUNCTION(Server, Reliable)                 void Server_NoteAction();

private:
	// Anti-spam de acciones cosmeticas (NoteAction) en cliente.
	double LastNoteActionTime = -1000.0;
};
