// Sandbox local de Picas y Famas: ejecuta el motor de ronda real (Source/PicasyFamas/Core) fuera de Unreal,
// con bots que deducen, farolean y compiten. Sirve para probar el juego sin el editor y para medir balance.
//
//   pf_sandbox play [bots] [--seed N]                 partida interactiva en tiempo real (tu + bots)
//   pf_sandbox sim  [partidas] [jugadores] [--seed N] simulacion acelerada con estadisticas
//   opciones:  --human         bots con latencia de lectura (3 s) y tiempos de pensar x2.5
//              --decoy-cap N   tope de rivales que puntuan por un senuelo (0 = sin tope, GDD v0.2)
//              --quiet         sin progreso en sim
//
// Comandos en modo play:  1234        intento
//                         e1234       intento encriptado (5 s)
//                         d1234 2 1   senuelo: finge 2 Famas 1 Pica (8 s)
//                         s 12        sospechar de la entrada #12
//                         q           salir
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <unistd.h>

#include "Core/PFCodeMath.h"
#include "Core/PFCandidateSet.h"
#include "Core/PFRoundEngine.h"

using namespace PF;

// ------------------------------------------------------------------------------------------------
// Utilidades
// ------------------------------------------------------------------------------------------------

static std::string CodeStr(PackedCode C, int Len)
{
	std::string S;
	for (int i = 0; i < Len; ++i) { S += char('0' + DigitAt(C, i)); if (i + 1 < Len) S += ' '; }
	return S;
}

static std::string Dots(int Famas, int Picas, int Len)
{
	std::string S;
	for (int i = 0; i < Famas; ++i) S += "\xe2\x97\x8f";            // ●
	for (int i = 0; i < Picas; ++i) S += "\xe2\x97\x8b";            // ○
	for (int i = Famas + Picas; i < Len; ++i) S += "\xc2\xb7";      // ·
	return S;
}

static std::string Clock(double Seconds)
{
	if (Seconds < 0) Seconds = 0;
	const int M = int(Seconds) / 60, S = int(Seconds) % 60;
	char Buf[16]; std::snprintf(Buf, sizeof Buf, "%d:%02d", M, S);
	return Buf;
}

// ------------------------------------------------------------------------------------------------
// Bots
// ------------------------------------------------------------------------------------------------

enum class EProfile { Human, Sondeador, Cerrador, Farolero, Paciente };

// Realismo de los bots. Por defecto son deductores perfectos e instantaneos (cota superior de habilidad).
// --human: tardan en LEER cada entrada nueva del tablero y piensan mas despacio.
static double gReadLatency = 0.0;   // s que una entrada publica tarda en ser incorporada por el bot
static double gThinkScale  = 1.0;   // multiplicador de los tiempos de pensar
static int    gDecoyCap    = 0;     // tope de rivales que puntuan por senuelo (0 = sin tope)

static const char* ProfileName(EProfile P)
{
	switch (P)
	{
	case EProfile::Human:     return "Tu";
	case EProfile::Sondeador: return "Sondeador";
	case EProfile::Cerrador:  return "Cerrador";
	case EProfile::Farolero:  return "Farolero";
	case EProfile::Paciente:  return "Paciente";
	}
	return "?";
}

struct FBot
{
	uint8_t  Seat = kNoPlayer;
	EProfile Profile = EProfile::Sondeador;
	std::mt19937_64 Rng;
	FCandidateSet View;                    // lo que el bot deduce del tablero PUBLICO
	std::map<int32_t, FGuessResult> MyTruth;
	int      SeenRevision = -1;
	double   NextThinkTime = 0.0;
	double   LastRebuildTime = -1.0;
	bool     bDecoyUsed = false, bEncryptUsed = false;
	int      Contradictions = 0;

	double Uniform(double A, double B) { return std::uniform_real_distribution<double>(A, B)(Rng); }
	int    RandInt(int A, int B)       { return std::uniform_int_distribution<int>(A, B)(Rng); }

	void ResetForRound(double Now, int Len)
	{
		View.Reset(Len);
		MyTruth.clear();
		SeenRevision = -1;
		bDecoyUsed = bEncryptUsed = false;
		NextThinkTime = Now + Uniform(1.5, 4.0);
	}

	// Reconstruye la deduccion a partir de la vista publica. El Paciente no se fia de resultados con < 8 s de vida.
	void RebuildView(const FRoundEngine& E, double Now, int Len)
	{
		auto Build = [&](bool bOnlyTrusted)
		{
			View.Reset(Len);
			for (int i = 0; i < E.NumEntries(); ++i)
			{
				const FPublicEntry P = MakePublic(E.EntryAt(i), true);
				if (P.Flags & GuessFlags::ResultHidden) continue;
				if (P.Player == Seat)
				{
					auto It = MyTruth.find(P.Seq);
					if (It != MyTruth.end()) View.Filter(P.Guess, It->second);
					continue;
				}
				const double Age = Now - P.ServerTime;
				const bool bAged = Age >= 8.0 || (P.Flags & GuessFlags::DecoyRevealed);
				if ((bOnlyTrusted || Profile == EProfile::Paciente) && !bAged) continue;
				if (Age < gReadLatency) continue;   // aun no lo ha leido
				View.Filter(P.Guess, FGuessResult{ P.Famas, P.Picas });
			}
		};
		Build(false);
		if (View.Num == 0)            // contradiccion logica: alguien miente
		{
			++Contradictions;
			Build(true);
			if (View.Num == 0) View.Reset(Len);
		}
	}

	struct FAction { bool bGuess = false; PackedCode Guess = 0; uint8_t Flags = 0; uint8_t DF = 0, DP = 0; int32_t SuspectSeq = -1; };

	FAction Think(const FRoundEngine& E, double Now, int Len, int BoardRevision)
	{
		FAction A;
		const FPlayerSlot& S = E.GetPlayer(Seat);
		if (!S.bPresent || !S.bConnected) return A;
		if (SeenRevision != BoardRevision || ((gReadLatency > 0.0 || Profile == EProfile::Paciente) && Now - LastRebuildTime >= 0.5))
		{
			RebuildView(E, Now, Len); SeenRevision = BoardRevision; LastRebuildTime = Now;
		}
		if (Now < NextThinkTime) return A;

		const double TimeLeft = S.AttemptDeadline > 0 ? S.AttemptDeadline - Now : 1e9;
		const bool bAlert = E.GetAlertPlayer() != kNoPlayer;
		const int N = View.Num;
		auto Pick = [&]() { return View.Codes[RandInt(0, View.Num - 1)]; };

		// Farolero: sospecha de entradas rivales llamativas.
		if (Profile == EProfile::Farolero && RandInt(0, 9) < 3)
		{
			for (int i = E.NumEntries() - 1; i >= 0 && i >= E.NumEntries() - 6; --i)
			{
				const FPublicEntry P = MakePublic(E.EntryAt(i), true);
				if (P.Player != Seat && P.Famas >= 2 && !(P.Flags & (GuessFlags::Suspected | GuessFlags::ResultHidden | GuessFlags::DecoyRevealed)))
				{
					A.SuspectSeq = P.Seq; break;
				}
			}
		}

		bool bShouldGuess = false;
		switch (Profile)
		{
		case EProfile::Sondeador:
			bShouldGuess = true;
			break;
		case EProfile::Cerrador:
		case EProfile::Paciente:
			bShouldGuess = (N <= 2) || (TimeLeft < 1.5) || (bAlert && N <= 8) || (Profile == EProfile::Paciente && N <= 20 && TimeLeft < 4.0);
			break;
		case EProfile::Farolero:
			bShouldGuess = true;
			break;
		case EProfile::Human:
			break;
		}
		if (!bShouldGuess)
		{
			NextThinkTime = Now + 0.4;
			return A;
		}

		A.bGuess = true;
		A.Guess = Pick();
		if (Profile == EProfile::Farolero && !bDecoyUsed && N > 30 && S.bHasDecoyToken)
		{
			A.Flags = GuessFlags::Decoy; A.DF = uint8_t(std::min(Len - 1, 2)); A.DP = 1; bDecoyUsed = true;
		}
		else if (Profile == EProfile::Paciente && !bEncryptUsed && N <= 3 && S.bHasEncryptToken)
		{
			A.Flags = GuessFlags::ResultHidden; bEncryptUsed = true;   // esconde la pista clave 5 s
		}

		double Think = 1.0;
		switch (Profile)
		{
		case EProfile::Sondeador: Think = Uniform(2.5, 6.0); break;
		case EProfile::Cerrador:  Think = Uniform(0.8, 2.2); break;
		case EProfile::Farolero:  Think = Uniform(2.0, 5.0); break;
		case EProfile::Paciente:  Think = Uniform(1.5, 3.5); break;
		default: break;
		}
		NextThinkTime = Now + Think * gThinkScale;
		return A;
	}
};

// ------------------------------------------------------------------------------------------------
// Mesa: listener + estado de presentacion + estadisticas
// ------------------------------------------------------------------------------------------------

struct FRoundStats
{
	double Duration = 0; ERoundEndReason Reason = ERoundEndReason::None; uint8_t Winner = kNoPlayer;
	double KeyClueToSolve = -1; int Attempts = 0; int Decoys = 0, DecoysCaught = 0, DecoyEffective = 0;
	int Passes = 0, Inactives = 0, Suspicions = 0; bool bAlert = false; bool bLightning = false; bool bIntuition = false;
	int RoundScore[kMaxPlayers] = {};
};

struct FTable : public IRoundListener
{
	FRoundEngine Engine;
	FRoundConfig Config;
	std::vector<std::unique_ptr<FBot>> Bots;     // indice = asiento
	std::string Names[kMaxPlayers];
	EProfile Profiles[kMaxPlayers];
	int NumPlayers = 0;
	int HumanSeat = -1;
	bool bVerbose = true;

	int BoardRevision = 0;
	std::vector<std::string> Log;
	std::map<int32_t, FGuessResult> HumanTruth;
	FRoundStats Round;
	double RoundStart = 0;
	double KeyClueTime = -1;
	std::string LastPrivate;

	FTable() { Engine.SetListener(this); }

	void AddPlayer(EProfile P, uint64_t Seed)
	{
		const uint8_t Seat = uint8_t(NumPlayers++);
		Engine.AddPlayer(Seat);
		Profiles[Seat] = P;
		char Buf[32];
		if (P == EProfile::Human) { HumanSeat = Seat; std::snprintf(Buf, sizeof Buf, "Tu"); }
		else std::snprintf(Buf, sizeof Buf, "%s-%d", ProfileName(P), Seat);
		Names[Seat] = Buf;
		auto B = std::make_unique<FBot>(); B->Seat = Seat; B->Profile = P; B->Rng.seed(Seed * 977 + Seat);
		Bots.push_back(std::move(B));
	}

	void StartRound(uint64_t Seed, double Now)
	{
		Config.CodeLength = NumPlayers >= 6 ? 5 : 4;
		Config.Scoring.DecoyEffectiveMaxTargets = gDecoyCap;
		Engine.StartRound(Config, Seed, Now);
		Round = FRoundStats{};
		RoundStart = Now;
		KeyClueTime = -1;
		Log.clear();
		HumanTruth.clear();
		LastPrivate.clear();
		BoardRevision++;
		for (auto& B : Bots) B->ResetForRound(Now, Config.CodeLength);
	}

	void Push(const std::string& S) { Log.push_back(S); if (Log.size() > 8) Log.erase(Log.begin()); }

	void OnRoundEvent(const FRoundEvent& E) override
	{
		const std::string Who = E.Player < kMaxPlayers ? Names[E.Player] : "?";
		switch (E.Type)
		{
		case EEventType::EntryAdded:
		case EEventType::EntryUpdated:
			BoardRevision++;
			if (E.Type == EEventType::EntryAdded) Round.Attempts++;
			if (E.Type == EEventType::EntryAdded) { if (const FGuessEntry* G = Engine.FindEntry(E.Seq)) if (G->Flags & GuessFlags::Decoy) Round.Decoys++; }
			break;
		case EEventType::GuessResult:
			if (E.Player < kMaxPlayers && Bots[E.Player]) { Bots[E.Player]->MyTruth[E.Seq] = FGuessResult{ E.Aux0, E.Aux1 }; Bots[E.Player]->SeenRevision = -1; }
			if ((int)E.Player == HumanSeat)
			{
				HumanTruth[E.Seq] = FGuessResult{ E.Aux0, E.Aux1 };
				LastPrivate = "#" + std::to_string(E.Seq) + "  " + CodeStr(E.Guess, Config.CodeLength) + "  -> " + std::to_string(E.Aux0) + " Famas, " + std::to_string(E.Aux1) + " Picas";
			}
			break;
		case EEventType::GuessRejected:
			if ((int)E.Player == HumanSeat)
			{
				static const char* R[] = { "", "ronda no activa", "jugador desconocido", "desconectado", "intento invalido", "demasiado rapido", "reloj expirado (Paso)", "ronda terminada", "historial lleno" };
				Push("Intento rechazado: " + std::string(R[E.Value]));
			}
			break;
		case EEventType::Pass:            Round.Passes++; break;
		case EEventType::PlayerInactive:  Round.Inactives++; Push(Who + " esta INACTIVO"); break;
		case EEventType::Score:
			if (E.Player < kMaxPlayers) Round.RoundScore[E.Player] += E.Value;
			if (E.Aux0 == (uint8_t)EScoreReason::DecoyCaught)    { Round.DecoysCaught++; Push(Who + " PILLADO con un senuelo (" + std::to_string(E.Value) + ")"); }
			if (E.Aux0 == (uint8_t)EScoreReason::DecoyEffective) { Round.DecoyEffective++; Push(Who + " engano a alguien con su senuelo (+" + std::to_string(E.Value) + ")"); }
			if (E.Aux0 == (uint8_t)EScoreReason::SuspicionMiss)  { Round.Suspicions++; Push(Who + " sospecho sin razon (" + std::to_string(E.Value) + ")"); }
			if (E.Aux0 == (uint8_t)EScoreReason::SuspicionHit)   { Round.Suspicions++; }
			if (E.Aux0 == (uint8_t)EScoreReason::LightningDeduction) Round.bLightning = true;
			break;
		case EEventType::Alert:
			Round.bAlert = true; Push("ALERTA: " + Who + " tiene " + std::to_string(Config.CodeLength - 1) + " FAMAS. Muerte Sudada: 20 s, relojes a 6 s"); break;
		case EEventType::KeyClue:
			KeyClueTime = E.Time; break;   // invisible para los jugadores; solo telemetria
		case EEventType::Solved:
			Push(Who + " ACIERTA el codigo"); Round.Winner = E.Player;
			if (KeyClueTime >= 0) Round.KeyClueToSolve = E.Time - KeyClueTime;
			break;
		case EEventType::PhotoFinish:     Push("FOTO-FINISH: " + Who + " tambien acierta"); break;
		case EEventType::Combo:
		{
			const char* Label = E.Time <= Config.Scoring.LightningWindow ? " DEDUCCION RELAMPAGO +" : (E.Time <= Config.Scoring.FastWindow ? " deduccion rapida +" : " acierta +");
			Push(Who + Label + std::to_string(E.Value) + " (" + std::to_string(int(E.Time * 100) / 100.0).substr(0, 4) + " s tras la pista clave)");
			break;
		}
		case EEventType::Intuition:       Round.bIntuition = true; Push(Who + ": golpe de intuicion (habia varios candidatos)"); break;
		case EEventType::RoundEnded:
			Round.Duration = E.Time - RoundStart; Round.Reason = (ERoundEndReason)E.Value;
			if (E.Player != kNoPlayer && Round.Winner == kNoPlayer) Round.Winner = E.Player;
			break;
		default: break;
		}
	}

	void TickBots(double Now)
	{
		for (auto& B : Bots)
		{
			if (B->Profile == EProfile::Human) continue;
			const FBot::FAction A = B->Think(Engine, Now, Config.CodeLength, BoardRevision);
			if (A.SuspectSeq >= 0) Engine.Suspect(B->Seat, A.SuspectSeq, Now);
			if (A.bGuess)
			{
				const double Rtt = B->Uniform(0.010, 0.120);
				Engine.Enqueue(B->Seat, A.Guess, A.Flags, A.DF, A.DP, Now, Rtt);
			}
		}
	}
};

// ------------------------------------------------------------------------------------------------
// Render (modo play)
// ------------------------------------------------------------------------------------------------

static void Render(const FTable& T, double Now, int RoundIndex, int NumRounds, const std::string& Phase, const int* MatchScore)
{
	const FRoundEngine& E = T.Engine;
	const int Len = T.Config.CodeLength;
	std::string Out;
	Out += "\033[2J\033[H";
	char Buf[256];

	std::snprintf(Buf, sizeof Buf, "PICAS Y FAMAS  |  ronda %d/%d  |  t=%s  |  %d digitos  |  %s\n", RoundIndex + 1, NumRounds, Clock(Now - T.RoundStart).c_str(), Len, Phase.c_str());
	Out += Buf;
	if (E.GetAlertPlayer() != kNoPlayer && E.IsRoundActive())
	{
		std::snprintf(Buf, sizeof Buf, "!! ALERTA %d FAMAS: %s  |  MUERTE SUDADA %s !!\n", Len - 1, T.Names[E.GetAlertPlayer()].c_str(), Clock(E.GetSuddenDeathEndTime() - Now).c_str());
		Out += Buf;
	}
	Out += "\n  #  Jugador        Mejor    Int  Ronda  Partida  Reloj   Fichas  Estado\n";
	for (int i = 0; i < T.NumPlayers; ++i)
	{
		const FPlayerSlot& S = E.GetPlayer(uint8_t(i));
		std::string Clk = S.AttemptDeadline > 0 ? (std::to_string(int((S.AttemptDeadline - Now) * 10) / 10.0).substr(0, 4) + "s") : "  -  ";
		std::string Tokens = std::string(S.bHasEncryptToken ? "E" : "-") + (S.bHasDecoyToken ? "S" : "-");
		std::string St = S.bInactive ? "zZ" : (E.GetAlertPlayer() == i ? "!!" : "");
		std::snprintf(Buf, sizeof Buf, "  %d  %-13s  %s  %3d  %5d  %7d  %-6s  %s      %s\n", i, T.Names[i].c_str(),
			Dots(S.BestFamas, S.BestPicas, Len).c_str(), S.Attempts, S.RoundScore, MatchScore[i] + S.RoundScore, Clk.c_str(), Tokens.c_str(), St.c_str());
		Out += Buf;
	}

	Out += "\n  MODULO DEL ENIGMA (publico)\n";
	const int First = std::max(0, E.NumEntries() - 14);
	for (int i = E.NumEntries() - 1; i >= First; --i)
	{
		const FPublicEntry P = MakePublic(E.EntryAt(i), E.IsRoundActive());
		std::string Res;
		if (P.Flags & GuessFlags::ResultHidden) Res = "??  [ENCRIPTADO " + std::to_string(int(std::max(0.0, P.RevealTime - Now))) + "s]";
		else Res = Dots(P.Famas, P.Picas, Len) + (P.Flags & GuessFlags::DecoyRevealed ? "  [ERA SENUELO]" : "") + (P.Flags & GuessFlags::Suspected ? " (sospechado)" : "");
		if (P.Flags & GuessFlags::Solved) Res += "  <<< ACIERTO";
		if (P.Flags & GuessFlags::KeyClue) Res += "  [pista clave]";
		std::string Bits = (P.Flags & GuessFlags::ResultHidden) ? "" : ("+" + std::to_string(P.InfoBitsX10 / 10) + "." + std::to_string(P.InfoBitsX10 % 10) + "b");
		std::snprintf(Buf, sizeof Buf, "  #%-3d %-13s %s   %-32s %6s  %s\n", P.Seq, T.Names[P.Player].c_str(), CodeStr(P.Guess, Len).c_str(), Res.c_str(), Bits.c_str(), Clock(P.ServerTime - T.RoundStart).c_str());
		Out += Buf;
	}

	if (T.HumanSeat >= 0)
	{
		Out += "\n  TUS RESULTADOS REALES (privado)\n";
		int Shown = 0;
		for (auto It = T.HumanTruth.rbegin(); It != T.HumanTruth.rend() && Shown < 5; ++It, ++Shown)
		{
			const FGuessEntry* G = E.FindEntry(It->first);
			if (!G) continue;
			std::snprintf(Buf, sizeof Buf, "  #%-3d %s   %d Famas  %d Picas\n", It->first, CodeStr(G->Guess, Len).c_str(), It->second.Famas, It->second.Picas);
			Out += Buf;
		}
	}

	Out += "\n  EVENTOS\n";
	for (const auto& L : T.Log) Out += "  - " + L + "\n";

	Out += "\n> 1234 intento | e1234 encriptar | d1234 F P senuelo | s <#> sospechar | q salir\n> ";
	std::fwrite(Out.data(), 1, Out.size(), stdout);
	std::fflush(stdout);
}

static bool StdinReady()
{
	pollfd P{ STDIN_FILENO, POLLIN, 0 };
	return poll(&P, 1, 0) > 0 && (P.revents & POLLIN);
}

static void HandleHumanCommand(FTable& T, const std::string& LineIn, double Now, bool& bQuit)
{
	std::string Line = LineIn;
	Line.erase(std::remove(Line.begin(), Line.end(), ' '), Line.end());
	if (Line.empty()) return;
	if (Line == "q" || Line == "Q") { bQuit = true; return; }
	const int Len = T.Config.CodeLength;
	const uint8_t Seat = uint8_t(T.HumanSeat);

	if (Line[0] == 's' || Line[0] == 'S')
	{
		const int Seq = std::atoi(Line.c_str() + 1);
		T.Engine.Suspect(Seat, Seq, Now);
		T.Push("Sospechas de la entrada #" + std::to_string(Seq));
		return;
	}
	uint8_t Flags = 0, DF = 0, DP = 0;
	size_t Pos = 0;
	if (Line[0] == 'e' || Line[0] == 'E') { Flags = GuessFlags::ResultHidden; Pos = 1; }
	if (Line[0] == 'd' || Line[0] == 'D') { Flags = GuessFlags::Decoy; Pos = 1; }
	if ((int)Line.size() < (int)Pos + Len) { T.Push("Necesito " + std::to_string(Len) + " digitos"); return; }
	uint8_t D[kMaxCodeLength] = {};
	for (int i = 0; i < Len; ++i)
	{
		if (!std::isdigit((unsigned char)Line[Pos + i])) { T.Push("Solo digitos"); return; }
		D[i] = uint8_t(Line[Pos + i] - '0');
	}
	Pos += Len;
	if (Flags == GuessFlags::Decoy)
	{
		if (Line.size() >= Pos + 2) { DF = uint8_t(Line[Pos] - '0'); DP = uint8_t(Line[Pos + 1] - '0'); }
		else { T.Push("Senuelo: d1234 F P  (ej. d1234 2 1)"); return; }
	}
	const PackedCode G = PackDigits(D, Len);
	if (!IsValidCode(G, Len)) { T.Push("Intento invalido: digitos 0-9 sin repetir"); return; }
	T.Engine.Enqueue(Seat, G, Flags, DF, DP, Now, 0.0);
}

// ------------------------------------------------------------------------------------------------
// Partida (comun a play y sim)
// ------------------------------------------------------------------------------------------------

struct FMatchResult { FRoundStats Rounds[9]; int NumRounds = 0; int MatchScore[kMaxPlayers] = {}; bool bQuit = false; };

static FMatchResult RunMatch(FTable& T, uint64_t Seed, int NumRounds, bool bRealtime)
{
	FMatchResult R; R.NumRounds = NumRounds;
	using ClockT = std::chrono::steady_clock;
	const auto T0 = ClockT::now();
	double Now = 1000.0;
	auto Advance = [&]()
	{
		if (bRealtime)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(33));
			Now = 1000.0 + std::chrono::duration<double>(ClockT::now() - T0).count();
		}
		else Now += 1.0 / 30.0;
	};

	for (int Round = 0; Round < NumRounds && !R.bQuit; ++Round)
	{
		T.StartRound(Seed * 31 + Round, Now);
		double LastRender = -1;
		int LastRevision = -1;
		while (T.Engine.IsRoundActive())
		{
			T.TickBots(Now);
			if (T.HumanSeat >= 0 && StdinReady())
			{
				std::string Line; if (!std::getline(std::cin, Line)) { R.bQuit = true; break; }
				HandleHumanCommand(T, Line, Now, R.bQuit);
				if (R.bQuit) break;
			}
			T.Engine.Tick(Now);
			if (bRealtime && (Now - LastRender > 0.25 || T.BoardRevision != LastRevision))
			{
				Render(T, Now, Round, NumRounds, "JUGANDO", R.MatchScore);
				LastRender = Now; LastRevision = T.BoardRevision;
			}
			Advance();
		}
		if (R.bQuit) break;
		R.Rounds[Round] = T.Round;
		for (int i = 0; i < T.NumPlayers; ++i) R.MatchScore[i] += T.Round.RoundScore[i];

		if (bRealtime)
		{
			const double End = Now + 8.0;
			while (Now < End)
			{
				std::string Ph = "RESUMEN  codigo: " + CodeStr(T.Engine.GetSecretCode(), T.Config.CodeLength) + "  ganador: " + (T.Round.Winner != kNoPlayer ? T.Names[T.Round.Winner] : "nadie") + "  siguiente en " + std::to_string(int(End - Now)) + "s";
				Render(T, Now, Round, NumRounds, Ph, R.MatchScore);
				if (StdinReady()) { std::string L; std::getline(std::cin, L); if (L == "q") { R.bQuit = true; break; } }
				for (int k = 0; k < 8; ++k) Advance();
			}
		}
	}
	return R;
}

// ------------------------------------------------------------------------------------------------
// Modos
// ------------------------------------------------------------------------------------------------

static const EProfile kBotRotation[] = { EProfile::Cerrador, EProfile::Sondeador, EProfile::Farolero, EProfile::Paciente };

static int RunPlay(int NumBots, uint64_t Seed)
{
	auto T = std::make_unique<FTable>();
	T->AddPlayer(EProfile::Human, Seed);
	for (int i = 0; i < NumBots; ++i) T->AddPlayer(kBotRotation[i % 4], Seed + i + 1);

	std::printf("Sala de %d jugadores. Codigo de %d digitos. Pulsa Enter para empezar...\n", T->NumPlayers, T->NumPlayers >= 6 ? 5 : 4);
	std::string L; std::getline(std::cin, L);

	const FMatchResult R = RunMatch(*T, Seed, 3, true);
	std::printf("\033[2J\033[H=== FIN DE PARTIDA ===\n");
	std::vector<int> Order(T->NumPlayers); for (int i = 0; i < T->NumPlayers; ++i) Order[i] = i;
	std::sort(Order.begin(), Order.end(), [&](int A, int B) { return R.MatchScore[A] > R.MatchScore[B]; });
	for (int i : Order) std::printf("  %-13s %5d pts\n", T->Names[i].c_str(), R.MatchScore[i]);
	return 0;
}

static int RunSim(int Matches, int Players, uint64_t Seed, bool bQuiet)
{
	Players = std::max(3, std::min(8, Players));
	struct FAgg { int Rounds = 0; double Dur = 0; int Solved = 0, SD = 0, Cap = 0, Alerts = 0, Lightning = 0, Intuition = 0; double KeyToSolve = 0; int KeyToSolveN = 0;
	              int Attempts = 0, Decoys = 0, DecoysCaught = 0, DecoyEffective = 0, Passes = 0, Inactives = 0, Suspicions = 0; double MaxDur = 0, MinDur = 1e9; int Contradictions = 0; } A;
	std::map<EProfile, std::pair<double, int>> ScoreByProfile;   // suma puntos por ronda, n
	std::map<EProfile, int> WinsByProfile;
	int Histogram[13] = {};   // duracion en tramos de 10 s

	for (int m = 0; m < Matches; ++m)
	{
		auto T = std::make_unique<FTable>();
		T->bVerbose = false;
		for (int i = 0; i < Players; ++i) T->AddPlayer(kBotRotation[(i + m) % 4], Seed + m * 100 + i);
		const FMatchResult R = RunMatch(*T, Seed + m, 3, false);
		for (int r = 0; r < R.NumRounds; ++r)
		{
			const FRoundStats& S = R.Rounds[r];
			A.Rounds++; A.Dur += S.Duration; A.MaxDur = std::max(A.MaxDur, S.Duration); A.MinDur = std::min(A.MinDur, S.Duration);
			Histogram[std::min(12, int(S.Duration / 10))]++;
			if (S.Reason == ERoundEndReason::Solved) A.Solved++;
			if (S.Reason == ERoundEndReason::SuddenDeathExpired) A.SD++;
			if (S.Reason == ERoundEndReason::TimeCap) A.Cap++;
			if (S.bAlert) A.Alerts++;
			if (S.bLightning) A.Lightning++;
			if (S.bIntuition) A.Intuition++;
			if (S.KeyClueToSolve >= 0) { A.KeyToSolve += S.KeyClueToSolve; A.KeyToSolveN++; }
			A.Attempts += S.Attempts; A.Decoys += S.Decoys; A.DecoysCaught += S.DecoysCaught; A.DecoyEffective += S.DecoyEffective;
			A.Passes += S.Passes; A.Inactives += S.Inactives; A.Suspicions += S.Suspicions;
			for (int i = 0; i < Players; ++i) { auto& P = ScoreByProfile[T->Profiles[i]]; P.first += S.RoundScore[i]; P.second++; }
			if (S.Winner != kNoPlayer) WinsByProfile[T->Profiles[S.Winner]]++;
		}
		for (auto& B : T->Bots) A.Contradictions += B->Contradictions;
		if (!bQuiet && (m + 1) % 50 == 0) std::printf("  ... %d partidas\n", m + 1);
	}

	std::printf("\n=== SIMULACION: %d partidas x 3 rondas, %d jugadores (%d digitos), semilla %llu ===\n", Matches, Players, Players >= 6 ? 5 : 4, (unsigned long long)Seed);
	std::printf("Rondas: %d   duracion media %.1f s  (min %.1f, max %.1f)\n", A.Rounds, A.Dur / A.Rounds, A.MinDur, A.MaxDur);
	std::printf("Fin por: acierto %.0f%%  |  Muerte Sudada expirada %.0f%%  |  cap 120 s %.0f%%\n", 100.0 * A.Solved / A.Rounds, 100.0 * A.SD / A.Rounds, 100.0 * A.Cap / A.Rounds);
	std::printf("Rondas con alerta N-1 Famas: %.0f%%   Relampago: %.0f%%   Golpe de intuicion: %.0f%%\n", 100.0 * A.Alerts / A.Rounds, 100.0 * A.Lightning / A.Rounds, 100.0 * A.Intuition / A.Rounds);
	if (A.KeyToSolveN) std::printf("Pista clave -> acierto: %.2f s de media (%d rondas)\n", A.KeyToSolve / A.KeyToSolveN, A.KeyToSolveN);
	std::printf("Intentos por ronda: %.1f (%.1f por jugador)   Pasos por ronda: %.2f   Inactivos por ronda: %.2f\n", (double)A.Attempts / A.Rounds, (double)A.Attempts / A.Rounds / Players, (double)A.Passes / A.Rounds, (double)A.Inactives / A.Rounds);
	std::printf("Senuelos: %d  (pillados %d, efectivos %d)   Sospechas: %d   Contradicciones detectadas por bots: %d\n", A.Decoys, A.DecoysCaught, A.DecoyEffective, A.Suspicions, A.Contradictions);
	std::printf("\nDuracion de ronda (histograma, tramos de 10 s):\n");
	for (int i = 0; i <= 12; ++i) { if (!Histogram[i]) continue; std::printf("  %3d-%3ds  %-40s %d\n", i * 10, i * 10 + 10, std::string(std::min(40, Histogram[i] * 40 / A.Rounds + (Histogram[i] ? 1 : 0)), '#').c_str(), Histogram[i]); }
	std::printf("\nPuntos medios por ronda y victorias por perfil:\n");
	for (auto& KV : ScoreByProfile)
	{
		std::printf("  %-10s  %6.1f pts/ronda   %3d victorias de ronda\n", ProfileName(KV.first), KV.second.first / KV.second.second, WinsByProfile[KV.first]);
	}
	return 0;
}

int main(int Argc, char** Argv)
{
	std::string Mode = Argc > 1 ? Argv[1] : "play";
	uint64_t Seed = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
	bool bQuiet = false;
	std::vector<int> Nums;
	for (int i = 2; i < Argc; ++i)
	{
		if (!std::strcmp(Argv[i], "--seed") && i + 1 < Argc) { Seed = std::strtoull(Argv[++i], nullptr, 10); continue; }
		if (!std::strcmp(Argv[i], "--quiet")) { bQuiet = true; continue; }
		if (!std::strcmp(Argv[i], "--human")) { gReadLatency = 3.0; gThinkScale = 2.5; continue; }
		if (!std::strcmp(Argv[i], "--decoy-cap") && i + 1 < Argc) { gDecoyCap = std::atoi(Argv[++i]); continue; }
		Nums.push_back(std::atoi(Argv[i]));
	}
	if (Mode == "play") return RunPlay(Nums.size() > 0 ? std::max(2, std::min(7, Nums[0])) : 3, Seed);
	if (Mode == "sim")  return RunSim(Nums.size() > 0 ? Nums[0] : 200, Nums.size() > 1 ? Nums[1] : 5, Seed, bQuiet);
	std::printf("uso: pf_sandbox play [bots] [--seed N] | sim [partidas] [jugadores] [--seed N] [--quiet]\n");
	return 1;
}
