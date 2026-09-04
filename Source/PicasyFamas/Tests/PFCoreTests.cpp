// Automation Tests del nucleo de reglas. Corren dentro del editor/servidor:
//   UnrealEditor-Cmd PicasyFamas.uproject -ExecCmds="Automation RunTests PicasyFamas" -unattended -nullrhi
// La misma logica se cubre fuera del motor con clang (ver Docs/GDD_Express_PicasyFamas.md, seccion de tests).
#include "Misc/AutomationTest.h"
#include <initializer_list>
#include "Core/PFCodeMath.h"
#include "Core/PFCandidateSet.h"
#include "Core/PFRoundEngine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags::Type kFlags = static_cast<EAutomationTestFlags::Type>(
		EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter);

	PF::PackedCode MakeCode(std::initializer_list<int32> Digits)
	{
		uint8 D[PF::kMaxCodeLength] = {};
		int32 i = 0;
		for (int32 V : Digits) D[i++] = static_cast<uint8>(V);
		return PF::PackDigits(D, static_cast<int32_t>(Digits.size()));
	}

	// Codigo distinto del secreto que comparte exactamente K posiciones.
	PF::PackedCode WithFamas(PF::PackedCode Secret, int32 Len, int32 K)
	{
		uint8 D[PF::kMaxCodeLength];
		PF::UnpackDigits(Secret, Len, D);
		uint16 Used = PF::DigitMaskOf(Secret, Len);
		for (int32 i = K; i < Len; ++i)
		{
			for (uint8 d = 0; d <= 9; ++d)
			{
				if (!(Used & (1u << d))) { D[i] = d; Used |= (1u << d); break; }
			}
		}
		return PF::PackDigits(D, Len);
	}

	struct FRecorder : public PF::IRoundListener
	{
		TArray<PF::FRoundEvent> Events;
		virtual void OnRoundEvent(const PF::FRoundEvent& E) override { Events.Add(E); }
		int32 Count(PF::EEventType T) const
		{
			int32 N = 0; for (const auto& E : Events) if (E.Type == T) ++N; return N;
		}
		int32 ScoreOf(uint8 Player, PF::EScoreReason R) const
		{
			int32 S = 0;
			for (const auto& E : Events) if (E.Type == PF::EEventType::Score && E.Player == Player && E.Aux0 == static_cast<uint8>(R)) S += E.Value;
			return S;
		}
		const PF::FRoundEvent* Last(PF::EEventType T) const
		{
			for (int32 i = Events.Num() - 1; i >= 0; --i) if (Events[i].Type == T) return &Events[i];
			return nullptr;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPFEvaluateMatchesReference, "PicasyFamas.Core.EvaluateCoincideConReferencia", kFlags)
bool FPFEvaluateMatchesReference::RunTest(const FString& Parameters)
{
	PF::FRng Rng(42);
	for (int32 Len = 3; Len <= 5; ++Len)
	{
		for (int32 n = 0; n < 5000; ++n)
		{
			const PF::PackedCode C = PF::GenerateCode(Rng, Len);
			const PF::PackedCode G = PF::GenerateCode(Rng, Len);
			TestTrue(TEXT("codigo generado valido"), PF::IsValidCode(C, Len));

			uint8 CD[5], GD[5]; PF::UnpackDigits(C, Len, CD); PF::UnpackDigits(G, Len, GD);
			int32 Famas = 0, Picas = 0;
			for (int32 i = 0; i < Len; ++i) if (CD[i] == GD[i]) ++Famas;
			for (int32 i = 0; i < Len; ++i) for (int32 j = 0; j < Len; ++j) if (i != j && CD[i] == GD[j]) ++Picas;

			const PF::FGuessResult R = PF::Evaluate(C, G, Len);
			if (R.Famas != Famas || R.Picas != Picas)
			{
				AddError(FString::Printf(TEXT("Evaluate difiere de la referencia: Len=%d C=%05x G=%05x"), Len, C, G));
				return false;
			}
		}
	}
	TestFalse(TEXT("digito repetido invalido"), PF::IsValidCode(MakeCode({1, 1, 2, 3}), 4));
	TestFalse(TEXT("digito > 9 invalido"), PF::IsValidCode(0x0000A321u, 4));
	TestEqual(TEXT("4 Picas"), static_cast<int32>(PF::Evaluate(MakeCode({1, 2, 3, 4}), MakeCode({4, 3, 2, 1}), 4).Picas), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPFCandidateSetConverges, "PicasyFamas.Core.ConjuntoDeCandidatosConverge", kFlags)
bool FPFCandidateSetConverges::RunTest(const FString& Parameters)
{
	TUniquePtr<PF::FCandidateSet> Set = MakeUnique<PF::FCandidateSet>();
	Set->Reset(4); TestEqual(TEXT("5040 candidatos con 4 digitos"), Set->Num, 5040);
	Set->Reset(5); TestEqual(TEXT("30240 candidatos con 5 digitos"), Set->Num, 30240);

	PF::FRng Rng(7);
	for (int32 r = 0; r < 50; ++r)
	{
		const PF::PackedCode Secret = PF::GenerateCode(Rng, 4);
		Set->Reset(4);
		int32 Guesses = 0;
		while (Set->Num > 1 && Guesses < 20)
		{
			const PF::PackedCode G = Set->Codes[Rng.RangeInclusive(0, Set->Num - 1)];
			Set->Filter(G, PF::Evaluate(Secret, G, 4));
			TestTrue(TEXT("el secreto nunca se elimina"), Set->Contains(Secret));
			++Guesses;
		}
		TestTrue(TEXT("converge al secreto"), Set->Num == 1 && Set->Single() == Secret);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPFPhotoFinishAndLatency, "PicasyFamas.Engine.FotoFinishYLatencia", kFlags)
bool FPFPhotoFinishAndLatency::RunTest(const FString& Parameters)
{
	{
		FRecorder Rec; PF::FRoundEngine Engine; Engine.SetListener(&Rec);
		Engine.AddPlayer(0); Engine.AddPlayer(1);
		PF::FRoundConfig Cfg; Engine.StartRound(Cfg, 1, 1000.0);
		// J0 llega antes al socket con 10 ms de ping; J1 llega 30 ms despues con 120 ms de ping -> J1 envio antes.
		Engine.Enqueue(0, Engine.GetSecretCode(), 0, 0, 0, 1000.000, 0.010);
		Engine.Enqueue(1, Engine.GetSecretCode(), 0, 0, 0, 1000.030, 0.120);
		Engine.Tick(1000.033);
		const PF::FRoundEvent* End = Rec.Last(PF::EEventType::RoundEnded);
		TestTrue(TEXT("gana quien envio antes segun tiempo ajustado"), End && End->Player == 1);
		TestEqual(TEXT("el segundo cobra acierto tardio"), Rec.ScoreOf(0, PF::EScoreReason::LateSolve), 50);
	}
	{
		FRecorder Rec; PF::FRoundEngine Engine; Engine.SetListener(&Rec);
		Engine.AddPlayer(0); Engine.AddPlayer(1);
		PF::FRoundConfig Cfg; Engine.StartRound(Cfg, 2, 1000.0);
		Engine.Enqueue(0, Engine.GetSecretCode(), 0, 0, 0, 1000.000, 0.0);
		Engine.Enqueue(1, Engine.GetSecretCode(), 0, 0, 0, 1000.001, 0.0);
		Engine.Tick(1000.033);
		TestEqual(TEXT("foto-finish: ambos cobran"), Rec.ScoreOf(0, PF::EScoreReason::Solve) + Rec.ScoreOf(1, PF::EScoreReason::Solve), 200);
		TestEqual(TEXT("mascara de ganadores"), static_cast<int32>(Engine.GetWinnerMask()), 0b11);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPFSuddenDeathAndBluff, "PicasyFamas.Engine.MuerteSudadaYFaroles", kFlags)
bool FPFSuddenDeathAndBluff::RunTest(const FString& Parameters)
{
	FRecorder Rec; PF::FRoundEngine Engine; Engine.SetListener(&Rec);
	for (uint8 i = 0; i < 4; ++i) Engine.AddPlayer(i);
	PF::FRoundConfig Cfg; Engine.StartRound(Cfg, 3, 1000.0);
	const PF::PackedCode Secret = Engine.GetSecretCode();

	// Senuelo: publico falso, verdad privada, no filtra candidatos hasta revelar.
	Engine.Enqueue(0, WithFamas(Secret, 4, 1), PF::GuessFlags::Decoy, 3, 0, 1000.0, 0.0);
	Engine.Tick(1000.033);
	const PF::FGuessEntry& E = Engine.EntryAt(0);
	TestEqual(TEXT("publico: 3 Famas falsas"), static_cast<int32>(E.Famas), 3);
	TestEqual(TEXT("verdad: 1 Fama"), static_cast<int32>(E.TrueFamas), 1);
	TestEqual(TEXT("un senuelo no dispara la alerta"), static_cast<int32>(Engine.GetAlertPlayer()), static_cast<int32>(PF::kNoPlayer));
	TestEqual(TEXT("candidatos intactos"), Engine.NumCandidates(), 5040);

	// Sospecha acertada: revelacion inmediata y puntuacion.
	Engine.Suspect(1, E.Seq, 1001.0);
	TestTrue(TEXT("senuelo corregido"), (E.Flags & PF::GuessFlags::DecoyRevealed) != 0);
	TestEqual(TEXT("-30 al farolero"), Rec.ScoreOf(0, PF::EScoreReason::DecoyCaught), -30);
	TestEqual(TEXT("+15 al que sospecho"), Rec.ScoreOf(1, PF::EScoreReason::SuspicionHit), 15);
	TestTrue(TEXT("ahora si filtra"), Engine.NumCandidates() < 5040);

	// 3 Famas reales -> alerta + Muerte Sudada con relojes a 6 s.
	Engine.Enqueue(2, WithFamas(Secret, 4, 3), 0, 0, 0, 1002.0, 0.0);
	Engine.Tick(1002.033);
	TestEqual(TEXT("alerta del asiento 2"), static_cast<int32>(Engine.GetAlertPlayer()), 2);
	TestTrue(TEXT("reloj recortado"), FMath::IsNearlyEqual(Engine.GetPlayer(3).AttemptDeadline, 1002.033 + 6.0, 1e-6));

	// Expira: gana por posicion el de 3 Famas.
	Engine.Tick(1030.0);
	TestFalse(TEXT("ronda terminada"), Engine.IsRoundActive());
	TestEqual(TEXT("motivo"), static_cast<int32>(Engine.GetEndReason()), static_cast<int32>(PF::ERoundEndReason::SuddenDeathExpired));
	const PF::FRoundEvent* End = Rec.Last(PF::EEventType::RoundEnded);
	TestTrue(TEXT("ganador por posicion"), End && End->Player == 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
