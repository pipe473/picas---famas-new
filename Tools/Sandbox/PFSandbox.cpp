// Sandbox local de Picas y Famas: ejecuta el motor de ronda real (Source/PicasyFamas/Core) fuera de Unreal,
// con bots que deducen, farolean y compiten. Sirve para probar el juego sin el editor y para medir balance.
//
//   pf_sandbox play [bots] [--seed N]                 partida interactiva en tiempo real (tu + bots)
//   pf_sandbox sim  [partidas] [jugadores] [--seed N] simulacion acelerada con estadisticas
//   opciones:  --turns seat|random|simultaneous   modo por turnos (el reloj corre solo para quien tiene el turno)
//              --pace fast|normal|slow            realismo de los bots (por defecto slow en serve)
//              --attempt N                        segundos por intento (por defecto 10)
//              --human         bots con latencia de lectura (3 s) y tiempos de pensar x2.5
//              --decoy-cap N   tope de rivales que puntuan por un senuelo (0 = sin tope, GDD v0.2)
//              --quiet         sin progreso en sim
//
// Comandos en modo play:  1234        intento
//                         e1234       intento encriptado (5 s)
//                         d1234 2 1   senuelo: finge 2 Famas 1 Pica (8 s)
//                         s 12        sospechar de la entrada #12
//                         q           salir
#include <algorithm>
#include <cctype>
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
#include <signal.h>
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
static double gSloppiness  = 0.0;   // prob. de que el bot NO procese una pista ajena hasta pasados 12 s (humanos no leen todo)
static int    gMemory      = 0;     // cuantas pistas AJENAS recientes retiene el bot (0 = todas). Una persona maneja 3-5.
static double gAttemptSeconds = 0;  // reloj por intento (0 = el del motor, 10 s). Palanca de diseno del ritmo.
static ETurnMode gTurnMode = ETurnMode::Simultaneous;   // simultaneo | por turnos (asiento) | por turnos (aleatorio)

static ETurnMode ParseTurnMode(const std::string& S)
{
	if (S == "seat" || S == "order")  return ETurnMode::SeatOrder;
	if (S == "random" || S == "rand") return ETurnMode::RandomOrder;
	return ETurnMode::Simultaneous;
}
static const char* TurnModeName(ETurnMode M)
{
	return M == ETurnMode::SeatOrder ? "seat" : (M == ETurnMode::RandomOrder ? "random" : "simultaneous");
}
static int    gDecoyCap    = 0;     // tope de rivales que puntuan por senuelo (0 = sin tope)

// Ritmo de la partida (afecta solo a los bots; las reglas del motor no cambian salvo el reloj si se pide).
static void SetPace(const std::string& Pace)
{
	if (Pace == "fast")        { gReadLatency = 0.0; gThinkScale = 1.0; gSloppiness = 0.0;  gMemory = 0; }   // deductores perfectos
	else if (Pace == "normal") { gReadLatency = 3.0; gThinkScale = 2.5; gSloppiness = 0.30; gMemory = 6; }
	else                       { gReadLatency = 5.0; gThinkScale = 4.0; gSloppiness = 0.55; gMemory = 3; }   // "slow": ritmo de mesa
}

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
	bool     bTurnArmed = false;
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
			// Memoria limitada: solo las ultimas gMemory pistas ajenas "caben en la cabeza".
			int FirstRemembered = 0;
			if (gMemory > 0)
			{
				int Count = 0;
				for (int i = E.NumEntries() - 1; i >= 0; --i)
				{
					if (E.EntryAt(i).Player == Seat) continue;
					if (++Count > gMemory) { FirstRemembered = i + 1; break; }
				}
			}
			for (int i = 0; i < E.NumEntries(); ++i)
			{
				const FPublicEntry P = MakePublic(E.EntryAt(i), true);
				if (P.Flags & GuessFlags::ResultHidden) continue;
				if (P.Player != Seat && i < FirstRemembered) continue;
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
				// Despiste humano: algunas pistas ajenas se le escapan durante un rato (decision estable por entrada).
				const uint32_t H = (uint32_t(P.Seq) * 2654435761u) ^ (uint32_t(Seat) * 40503u + 7u);
				if (Age < 12.0 && double(H % 1000) < gSloppiness * 1000.0) continue;
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
		// Por turnos: mientras no sea mi turno solo observo. Al recibirlo, "empiezo a pensar" desde ese momento.
		if (E.IsTurnBased())
		{
			if (E.GetCurrentTurnPlayer() != Seat) { bTurnArmed = false; return A; }
			if (!bTurnArmed) { bTurnArmed = true; NextThinkTime = Now + Uniform(1.0, 3.0) * gThinkScale * 0.6; }
		}

		const double TimeLeft = S.AttemptDeadline > 0 ? S.AttemptDeadline - Now : 1e9;
		// Como una persona: si el reloj se acaba, envia lo mejor que tenga aunque no haya terminado de pensar.
		if (Now < NextThinkTime && TimeLeft > 1.2) return A;
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
	struct FLogItem { int Id; std::string Text; };
	std::vector<FLogItem> Log;
	int LogSeq = 0;
	std::map<int32_t, FGuessResult> HumanTruth;                 // modo consola (un humano)
	std::map<int32_t, FGuessResult> SeatTruth[kMaxPlayers];     // verdad privada por asiento (web)
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
		if (gAttemptSeconds > 0) Config.AttemptSeconds = gAttemptSeconds;
		Config.TurnMode = gTurnMode;
		Round = FRoundStats{};
		RoundStart = Now;
		KeyClueTime = -1;
		Log.clear();
		HumanTruth.clear();
		for (int i = 0; i < kMaxPlayers; ++i) SeatTruth[i].clear();
		LastPrivate.clear();
		BoardRevision++;
		for (auto& B : Bots) B->ResetForRound(Now, Config.CodeLength);
		Engine.StartRound(Config, Seed, Now);   // por turnos: emite el primer TurnChanged
	}

	void Push(const std::string& S) { Log.push_back({ ++LogSeq, S }); if (Log.size() > 8) Log.erase(Log.begin()); }

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
			if (E.Player < kMaxPlayers) SeatTruth[E.Player][E.Seq] = FGuessResult{ E.Aux0, E.Aux1 };
			if ((int)E.Player == HumanSeat)
			{
				HumanTruth[E.Seq] = FGuessResult{ E.Aux0, E.Aux1 };
				LastPrivate = "#" + std::to_string(E.Seq) + "  " + CodeStr(E.Guess, Config.CodeLength) + "  -> " + std::to_string(E.Aux0) + " Famas, " + std::to_string(E.Aux1) + " Picas";
			}
			break;
		case EEventType::GuessRejected:
		{
			static const char* R[] = { "", "ronda no activa", "jugador desconocido", "desconectado", "intento invalido", "demasiado rapido", "reloj expirado (Paso)", "ronda terminada", "historial lleno", "no es tu turno" };
			const std::string Why = (E.Value >= 0 && E.Value < 10) ? R[E.Value] : "?";
			if (E.Player < kMaxPlayers && Profiles[E.Player] == EProfile::Human) Push(Who + ": intento rechazado (" + Why + ")");
			break;
		}
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
		case EEventType::TurnChanged:
			if (E.Player != kNoPlayer && Engine.IsRoundActive()) Push("Turno " + std::to_string(E.Value) + ": " + Who);
			BoardRevision++;
			break;
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
	for (const auto& L : T.Log) Out += "  - " + L.Text + "\n";

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

// ------------------------------------------------------------------------------------------------
// Modo serve: servidor HTTP local minimo + interfaz web (export Next.js en Tools/Sandbox/web)
// ------------------------------------------------------------------------------------------------

static std::string JsonStr(const std::string& S)
{
	std::string O = "\"";
	for (unsigned char c : S)
	{
		if (c == '"') O += "\\\""; else if (c == '\\') O += "\\\\"; else if (c < 0x20) O += ' '; else O += char(c);
	}
	return O + "\"";
}

static std::string ReadFile(const std::string& Path)
{
	FILE* F = std::fopen(Path.c_str(), "rb");
	if (!F) return "";
	std::string S; char Buf[4096]; size_t N;
	while ((N = std::fread(Buf, 1, sizeof Buf, F)) > 0) S.append(Buf, N);
	std::fclose(F);
	return S;
}

static std::string UrlDecode(const std::string& S)
{
	std::string O;
	for (size_t i = 0; i < S.size(); ++i)
	{
		if (S[i] == '%' && i + 2 < S.size()) { O += char(std::strtol(S.substr(i + 1, 2).c_str(), nullptr, 16)); i += 2; }
		else if (S[i] == '+') O += ' ';
		else O += S[i];
	}
	return O;
}

static std::map<std::string, std::string> ParseQuery(const std::string& Q)
{
	std::map<std::string, std::string> M;
	std::stringstream SS(Q); std::string KV;
	while (std::getline(SS, KV, '&'))
	{
		const size_t Eq = KV.find('=');
		if (Eq == std::string::npos) M[UrlDecode(KV)] = ""; else M[UrlDecode(KV.substr(0, Eq))] = UrlDecode(KV.substr(Eq + 1));
	}
	return M;
}

static const char* kFirstNames[] = { "Lucia", "Mateo", "Sofia", "Diego", "Valeria", "Andres", "Camila", "Tomas" };
static constexpr int kMinPlayers = 2;

static std::mt19937_64& WebRng()
{
	static std::mt19937_64 R{ (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count() };
	return R;
}

static std::string RandomToken()
{
	char Buf[33];
	for (int i = 0; i < 16; ++i) std::snprintf(Buf + i * 2, 3, "%02x", (unsigned)(WebRng()() & 0xff));
	return std::string(Buf, 32);
}

static std::string MakeRoomCode()
{
	static const char* A = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
	std::string S;
	for (int i = 0; i < 4; ++i) S += A[WebRng()() % 32];
	return S;
}

static std::string SanitizeName(std::string S)
{
	std::string O;
	for (unsigned char C : S)
	{
		if (O.size() >= 16) break;
		if (std::isalnum(C) || C == ' ' || C == '-' || C == '_') O += char(C);
	}
	while (!O.empty() && O.front() == ' ') O.erase(O.begin());
	while (!O.empty() && O.back() == ' ') O.pop_back();
	return O.empty() ? "Jugador" : O;
}

struct FWebSession
{
	enum class EPhase { Lobby, Countdown, Playing, Summary, MatchEnd };
	struct FClient { std::string Token, Name; int Seat = -1; bool bHost = false; };

	std::unique_ptr<FTable> T;
	std::vector<FClient> Clients;
	std::string RoomCode;
	std::string PendingSetCookie;
	EPhase Phase = EPhase::Lobby;
	int RoundIndex = 0, NumRounds = 3;
	double PhaseEnd = 0;
	int MatchScore[kMaxPlayers] = {};
	std::string WebNames[kMaxPlayers];
	FRoundStats LastRound;
	uint64_t Seed = 1;
	int Bots = 0;
	int RoundsPlayed = 0;
	std::string PaceName = "slow";
	double SessionAttempt = 0;
	ETurnMode SessionTurns = ETurnMode::Simultaneous;

	void ApplyGlobals() const
	{
		SetPace(PaceName);
		gAttemptSeconds = SessionAttempt;
		gTurnMode = SessionTurns;
	}

	FClient* FindClient(const std::string& Token)
	{
		if (Token.empty()) return nullptr;
		for (auto& C : Clients) if (C.Token == Token) return &C;
		return nullptr;
	}
	const FClient* FindClient(const std::string& Token) const
	{
		if (Token.empty()) return nullptr;
		for (const auto& C : Clients) if (C.Token == Token) return &C;
		return nullptr;
	}

	int TotalPlayers() const { return int(Clients.size()) + Bots; }

	void ClampBots()
	{
		const int Room = kMaxPlayers - int(Clients.size());
		if (Bots > Room) Bots = std::max(0, Room);
		if (Bots < 0) Bots = 0;
	}

	void OpenLobby(int DefaultBots, bool bNewCode = true)
	{
		const std::string Keep = RoomCode;
		if (bNewCode || RoomCode.empty()) RoomCode = MakeRoomCode();
		else RoomCode = Keep;
		Clients.clear();
		T.reset();
		Bots = std::max(0, std::min(kMaxPlayers - kMinPlayers, DefaultBots));
		Phase = EPhase::Lobby;
		PhaseEnd = 0;
		RoundIndex = 0;
		RoundsPlayed = 0;
		for (int i = 0; i < kMaxPlayers; ++i) MatchScore[i] = 0;
	}

	bool AllHumansInactive() const
	{
		if (Phase == EPhase::Lobby) return Clients.empty();
		if (!T) return true;
		for (const auto& C : Clients)
		{
			if (C.Seat < 0 || C.Seat >= kMaxPlayers) continue;
			const FPlayerSlot& P = T->Engine.GetPlayer(uint8_t(C.Seat));
			if (P.bPresent && !P.bInactive) return false;
		}
		return true;
	}

	bool CanReset(const FClient* You) const
	{
		if (You && You->bHost) return true;
		if (Phase == EPhase::Lobby) return true;
		if (Phase == EPhase::MatchEnd) return true;
		return AllHumansInactive();
	}

	void BuildTable()
	{
		ClampBots();
		T = std::make_unique<FTable>();
		T->bVerbose = false;
		Seed = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
		int Seat = 0;
		for (auto& C : Clients)
		{
			T->AddPlayer(EProfile::Human, Seed + uint64_t(Seat) * 17);
			C.Seat = Seat;
			WebNames[Seat] = C.Name;
			T->Names[Seat] = C.Name;
			++Seat;
		}
		for (int i = 0; i < Bots && Seat < kMaxPlayers; ++i)
		{
			T->AddPlayer(kBotRotation[i % 4], Seed + uint64_t(Seat) * 31 + 1);
			WebNames[Seat] = kFirstNames[Seat % 8];
			T->Names[Seat] = WebNames[Seat];
			++Seat;
		}
		for (int i = 0; i < kMaxPlayers; ++i) MatchScore[i] = 0;
		RoundIndex = 0;
		RoundsPlayed = 0;
	}

	void BeginCountdown(double Now) { Phase = EPhase::Countdown; PhaseEnd = Now + 3.0; }

	bool TryStart(double Now, std::string& Err)
	{
		ClampBots();
		if (TotalPlayers() < kMinPlayers) { Err = "hacen falta al menos 2 jugadores (humanos o bots)"; return false; }
		if (TotalPlayers() > kMaxPlayers) { Err = "maximo 8 jugadores"; return false; }
		BuildTable();
		BeginCountdown(Now);
		return true;
	}

	void Tick(double Now)
	{
		ApplyGlobals();
		if (Phase == EPhase::Lobby || !T) return;
		switch (Phase)
		{
		case EPhase::Countdown:
			if (Now >= PhaseEnd) { T->StartRound(Seed * 31 + RoundIndex, Now); Phase = EPhase::Playing; }
			break;
		case EPhase::Playing:
			T->TickBots(Now);
			T->Engine.Tick(Now);
			if (!T->Engine.IsRoundActive())
			{
				LastRound = T->Round; RoundsPlayed++;
				for (int i = 0; i < T->NumPlayers; ++i) MatchScore[i] += T->Round.RoundScore[i];
				Phase = EPhase::Summary; PhaseEnd = Now + 12.0;
			}
			break;
		case EPhase::Summary:
			if (Now >= PhaseEnd)
			{
				if (++RoundIndex < NumRounds) BeginCountdown(Now);
				else { Phase = EPhase::MatchEnd; PhaseEnd = Now + 15.0; }
			}
			break;
		default: break;
		}
	}

	void AppendMeta(std::string& J, const FClient* You, int HumanSeat, int Len, double Now) const
	{
		static const char* PhaseNames[] = { "lobby", "countdown", "playing", "summary", "matchend" };
		char Buf[384];
		const bool bJoined = You != nullptr;
		const bool bHost = You && You->bHost;
		std::snprintf(Buf, sizeof Buf,
			"\"phase\":\"%s\",\"round\":%d,\"rounds\":%d,\"len\":%d,\"now\":%.3f,\"phaseEnd\":%.3f,\"roundStart\":%.3f,"
			"\"spectator\":%s,\"humanSeat\":%d,\"joined\":%s,\"isHost\":%s,\"roomCode\":%s,\"bots\":%d,"
			"\"minPlayers\":%d,\"maxPlayers\":%d,",
			PhaseNames[(int)Phase], RoundIndex + 1, NumRounds, Len, Now, PhaseEnd, T ? T->RoundStart : 0.0,
			bJoined ? "false" : "true", HumanSeat, bJoined ? "true" : "false", bHost ? "true" : "false",
			JsonStr(RoomCode).c_str(), Bots, kMinPlayers, kMaxPlayers);
		J += Buf;
		J += "\"pace\":" + JsonStr(PaceName) + ",";
		J += "\"attemptSeconds\":" + std::to_string(int(gAttemptSeconds > 0 ? gAttemptSeconds : FRoundConfig{}.AttemptSeconds)) + ",";
		J += "\"sdAttemptSeconds\":" + std::to_string(int(FRoundConfig{}.SuddenDeathAttemptSeconds)) + ",";
		J += "\"turnMode\":" + JsonStr(TurnModeName(gTurnMode)) + ",";
	}

	std::string StateJson(double Now, const std::string& Token) const
	{
		ApplyGlobals();
		const FClient* You = FindClient(Token);
		std::string J = "{";
		char Buf[384];

		if (Phase == EPhase::Lobby || !T)
		{
			const int Humans = int(Clients.size());
			const int Total = Humans + Bots;
			const int Len = Total >= 6 ? 5 : 4;
			int HumanSeat = -1;
			if (You) { for (int i = 0; i < Humans; ++i) if (Clients[size_t(i)].Token == You->Token) HumanSeat = i; }
			AppendMeta(J, You, HumanSeat, Len, Now);
			J += "\"turnPlayer\":-1,\"turnNumber\":0,\"turnOrder\":[],\"secret\":null,\"winner\":-1,\"alertPlayer\":-1,\"suddenDeathEnd\":0,";
			J += "\"players\":[";
			for (int i = 0; i < Humans; ++i)
			{
				std::snprintf(Buf, sizeof Buf, "%s{\"seat\":%d,\"name\":%s,\"profile\":\"humano\",\"bestF\":0,\"bestP\":0,\"attempts\":0,\"roundScore\":0,\"matchScore\":0,\"deadline\":0,\"encrypt\":true,\"decoy\":true,\"inactive\":false,\"solved\":false}",
					i ? "," : "", i, JsonStr(Clients[size_t(i)].Name).c_str());
				J += Buf;
			}
			for (int i = 0; i < Bots; ++i)
			{
				const int Seat = Humans + i;
				std::snprintf(Buf, sizeof Buf, "%s{\"seat\":%d,\"name\":%s,\"profile\":\"%s\",\"bestF\":0,\"bestP\":0,\"attempts\":0,\"roundScore\":0,\"matchScore\":0,\"deadline\":0,\"encrypt\":true,\"decoy\":true,\"inactive\":false,\"solved\":false}",
					(Humans + i) ? "," : "", Seat, JsonStr(kFirstNames[Seat % 8]).c_str(), ProfileName(kBotRotation[i % 4]));
				J += Buf;
			}
			J += "],\"entries\":[],\"private\":[],\"events\":[]}";
			return J;
		}

		const FRoundEngine& E = T->Engine;
		const int Len = T->Config.CodeLength ? T->Config.CodeLength : (T->NumPlayers >= 6 ? 5 : 4);
		const bool bRoundActive = E.IsRoundActive();
		const int HumanSeat = You && You->Seat >= 0 ? You->Seat : -1;
		AppendMeta(J, You, HumanSeat, Len, Now);
		J += "\"turnPlayer\":" + std::to_string(bRoundActive && E.IsTurnBased() && E.GetCurrentTurnPlayer() != kNoPlayer ? (int)E.GetCurrentTurnPlayer() : -1) + ",";
		J += "\"turnNumber\":" + std::to_string(E.GetTurnNumber()) + ",";
		J += "\"turnOrder\":[";
		for (int i = 0; i < E.GetTurnOrderCount(); ++i) J += (i ? "," : "") + std::to_string((int)E.GetTurnOrderAt(i));
		J += "],";

		const bool bShowSecret = (Phase == EPhase::Summary || Phase == EPhase::MatchEnd) && RoundsPlayed > 0;
		J += "\"secret\":" + (bShowSecret ? JsonStr(CodeStr(E.GetSecretCode(), Len)) : std::string("null")) + ",";
		J += "\"winner\":" + std::to_string(bShowSecret ? (int)(int8_t)(LastRound.Winner == kNoPlayer ? -1 : LastRound.Winner) : -1) + ",";
		std::snprintf(Buf, sizeof Buf, "\"alertPlayer\":%d,\"suddenDeathEnd\":%.3f,", bRoundActive && E.GetAlertPlayer() != kNoPlayer ? E.GetAlertPlayer() : -1, E.GetSuddenDeathEndTime());
		J += Buf;

		J += "\"players\":[";
		for (int i = 0; i < T->NumPlayers; ++i)
		{
			const FPlayerSlot& S = E.GetPlayer(uint8_t(i));
			const char* Prof = T->Profiles[i] == EProfile::Human ? "humano" : ProfileName(T->Profiles[i]);
			std::snprintf(Buf, sizeof Buf, "%s{\"seat\":%d,\"name\":%s,\"profile\":\"%s\",\"bestF\":%d,\"bestP\":%d,\"attempts\":%d,\"roundScore\":%d,\"matchScore\":%d,\"deadline\":%.3f,\"encrypt\":%s,\"decoy\":%s,\"inactive\":%s,\"solved\":%s}",
				i ? "," : "", i, JsonStr(WebNames[i]).c_str(), Prof, S.BestFamas, S.BestPicas, S.Attempts, S.RoundScore, MatchScore[i] + (bRoundActive ? S.RoundScore : 0),
				S.AttemptDeadline, S.bHasEncryptToken ? "true" : "false", S.bHasDecoyToken ? "true" : "false", S.bInactive ? "true" : "false", S.BestFamas >= Len ? "true" : "false");
			J += Buf;
		}
		J += "],\"entries\":[";
		for (int i = 0; i < E.NumEntries(); ++i)
		{
			const FPublicEntry P = MakePublic(E.EntryAt(i), bRoundActive);
			std::snprintf(Buf, sizeof Buf, "%s{\"seq\":%d,\"player\":%d,\"guess\":%s,\"f\":%d,\"p\":%d,\"hidden\":%s,\"decoyRevealed\":%s,\"suspected\":%s,\"keyClue\":%s,\"solved\":%s,\"bits\":%.1f,\"t\":%.3f,\"reveal\":%.3f}",
				i ? "," : "", P.Seq, P.Player, JsonStr(CodeStr(P.Guess, Len)).c_str(), P.Famas, P.Picas,
				(P.Flags & GuessFlags::ResultHidden) ? "true" : "false", (P.Flags & GuessFlags::DecoyRevealed) ? "true" : "false",
				(P.Flags & GuessFlags::Suspected) ? "true" : "false", (P.Flags & GuessFlags::KeyClue) ? "true" : "false", (P.Flags & GuessFlags::Solved) ? "true" : "false",
				P.InfoBitsX10 / 10.0, P.ServerTime, P.RevealTime);
			J += Buf;
		}
		J += "],\"private\":[";
		bool bFirst = true;
		if (HumanSeat >= 0 && HumanSeat < kMaxPlayers)
		{
			for (const auto& KV : T->SeatTruth[HumanSeat])
			{
				const FGuessEntry* G = E.FindEntry(KV.first);
				if (!G) continue;
				std::snprintf(Buf, sizeof Buf, "%s{\"seq\":%d,\"guess\":%s,\"f\":%d,\"p\":%d}", bFirst ? "" : ",", KV.first, JsonStr(CodeStr(G->Guess, Len)).c_str(), KV.second.Famas, KV.second.Picas);
				J += Buf; bFirst = false;
			}
		}
		J += "],\"events\":[";
		for (size_t i = 0; i < T->Log.size(); ++i)
		{
			J += (i ? "," : "") + std::string("{\"id\":") + std::to_string(T->Log[i].Id) + ",\"text\":" + JsonStr(T->Log[i].Text) + "}";
		}
		J += "]}";
		return J;
	}

	std::string HandleApi(const std::string& Path, const std::map<std::string, std::string>& Q, const std::string& TokenIn, double Now)
	{
		ApplyGlobals();
		PendingSetCookie.clear();
		const std::string Token = Q.count("token") ? Q.at("token") : TokenIn;
		FClient* You = FindClient(Token);

		if (Path == "/api/state" || Path == "/api/stream") return StateJson(Now, Token);

		if (Path == "/api/join")
		{
			if (Phase != EPhase::Lobby && Phase != EPhase::MatchEnd) return "{\"ok\":false,\"error\":\"la partida ya empezo\"}";
			if (You) return "{\"ok\":true,\"token\":" + JsonStr(You->Token) + ",\"code\":" + JsonStr(RoomCode) + ",\"host\":" + std::string(You->bHost ? "true" : "false") + "}";
			if (Q.count("code") && !Q.at("code").empty() && Q.at("code") != RoomCode) return "{\"ok\":false,\"error\":\"codigo de sala incorrecto\"}";
			if (int(Clients.size()) >= kMaxPlayers) return "{\"ok\":false,\"error\":\"sala llena\"}";
			FClient C;
			C.Token = RandomToken();
			C.Name = SanitizeName(Q.count("name") ? Q.at("name") : "");
			C.bHost = Clients.empty();
			C.Seat = -1;
			Clients.push_back(C);
			ClampBots();
			PendingSetCookie = C.Token;
			return "{\"ok\":true,\"token\":" + JsonStr(C.Token) + ",\"code\":" + JsonStr(RoomCode) + ",\"host\":" + std::string(C.bHost ? "true" : "false") + "}";
		}

		if (Path == "/api/reset")
		{
			if (!CanReset(You)) return "{\"ok\":false,\"error\":\"hay una partida activa\"}";
			OpenLobby(0, false);
			return "{\"ok\":true,\"code\":" + JsonStr(RoomCode) + "}";
		}

		if (Path == "/api/solo")
		{
			if (!CanReset(You)) return "{\"ok\":false,\"error\":\"hay una partida activa con amigos\"}";
			const int WantBots = Q.count("bots") ? std::atoi(Q.at("bots").c_str()) : 3;
			OpenLobby(WantBots, false);
			FClient C;
			C.Token = RandomToken();
			C.Name = SanitizeName(Q.count("name") ? Q.at("name") : "");
			C.bHost = true;
			C.Seat = -1;
			Clients.push_back(C);
			Bots = std::max(1, std::min(kMaxPlayers - 1, WantBots));
			std::string Err;
			if (!TryStart(Now, Err)) return "{\"ok\":false,\"error\":" + JsonStr(Err) + "}";
			PendingSetCookie = C.Token;
			return "{\"ok\":true,\"token\":" + JsonStr(C.Token) + ",\"code\":" + JsonStr(RoomCode) + ",\"host\":true}";
		}

		if (Path == "/api/leave")
		{
			if (!You) return "{\"ok\":false,\"error\":\"no estas en la sala\"}";
			if (Phase != EPhase::Lobby) return "{\"ok\":false,\"error\":\"solo puedes salir en la sala\"}";
			const bool bWasHost = You->bHost;
			Clients.erase(Clients.begin() + (You - Clients.data()));
			if (bWasHost && !Clients.empty()) Clients[0].bHost = true;
			ClampBots();
			return "{\"ok\":true}";
		}

		if (Path == "/api/config" || Path == "/api/new")
		{
			if (!You || !You->bHost) return "{\"ok\":false,\"error\":\"solo el anfitrion configura la sala\"}";
			if (Phase != EPhase::Lobby && Phase != EPhase::MatchEnd) return "{\"ok\":false,\"error\":\"la partida ya empezo\"}";
			if (Q.count("bots")) Bots = std::atoi(Q.at("bots").c_str());
			if (Q.count("pace")) { PaceName = Q.at("pace"); SetPace(PaceName); }
			if (Q.count("attempt")) { const int A = std::atoi(Q.at("attempt").c_str()); SessionAttempt = (A >= 5 && A <= 60) ? A : 0; gAttemptSeconds = SessionAttempt; }
			if (Q.count("turns")) { SessionTurns = ParseTurnMode(Q.at("turns")); gTurnMode = SessionTurns; }
			ClampBots();
			return "{\"ok\":true,\"bots\":" + std::to_string(Bots) + "}";
		}

		if (Path == "/api/start")
		{
			if (!You || !You->bHost) return "{\"ok\":false,\"error\":\"solo el anfitrion empieza\"}";
			if (Phase == EPhase::MatchEnd)
			{
				std::string Err;
				if (!TryStart(Now, Err)) return "{\"ok\":false,\"error\":" + JsonStr(Err) + "}";
				return "{\"ok\":true}";
			}
			if (Phase != EPhase::Lobby) return "{\"ok\":false,\"error\":\"la partida ya empezo\"}";
			std::string Err;
			if (!TryStart(Now, Err)) return "{\"ok\":false,\"error\":" + JsonStr(Err) + "}";
			return "{\"ok\":true}";
		}

		if (!You || You->Seat < 0) return "{\"ok\":false,\"error\":\"no estas sentado en esta ronda\"}";
		if (!T) return "{\"ok\":false,\"error\":\"sin partida\"}";
		const uint8_t Seat = uint8_t(You->Seat);
		if (Path == "/api/suspect")
		{
			const int Seq = Q.count("seq") ? std::atoi(Q.at("seq").c_str()) : -1;
			if (Seq >= 0) { T->Engine.Suspect(Seat, Seq, Now); T->Push(You->Name + " sospecha de #" + std::to_string(Seq)); }
			return "{\"ok\":true}";
		}
		if (Path == "/api/guess")
		{
			if (Phase != EPhase::Playing) return "{\"ok\":false,\"error\":\"la ronda no esta en juego\"}";
			const std::string D = Q.count("d") ? Q.at("d") : "";
			const std::string Mode = Q.count("mode") ? Q.at("mode") : "plain";
			const int Len = T->Config.CodeLength;
			if ((int)D.size() != Len) return "{\"ok\":false,\"error\":\"hacen falta " + std::to_string(Len) + " digitos\"}";
			uint8_t Dg[kMaxCodeLength] = {};
			for (int i = 0; i < Len; ++i) { if (!std::isdigit((unsigned char)D[i])) return "{\"ok\":false,\"error\":\"solo digitos\"}"; Dg[i] = uint8_t(D[i] - '0'); }
			const PackedCode G = PackDigits(Dg, Len);
			if (!IsValidCode(G, Len)) return "{\"ok\":false,\"error\":\"digitos repetidos\"}";
			uint8_t Flags = 0, DF = 0, DP = 0;
			if (Mode == "encrypt") Flags = GuessFlags::ResultHidden;
			if (Mode == "decoy")
			{
				Flags = GuessFlags::Decoy;
				DF = uint8_t(Q.count("f") ? std::atoi(Q.at("f").c_str()) : 0);
				DP = uint8_t(Q.count("p") ? std::atoi(Q.at("p").c_str()) : 0);
				if (DF + DP > Len || DF >= Len) return "{\"ok\":false,\"error\":\"resultado falso imposible\"}";
			}
			T->Engine.Enqueue(Seat, G, Flags, DF, DP, Now, 0.0);
			return "{\"ok\":true}";
		}
		return "{\"ok\":false,\"error\":\"ruta desconocida\"}";
	}
};

static std::string NormCode(std::string S)
{
	for (char& C : S) C = char(std::toupper((unsigned char)C));
	return S;
}

static std::string LandingJson(double Now)
{
	char Buf[512];
	std::snprintf(Buf, sizeof Buf,
		"{\"phase\":\"none\",\"round\":1,\"rounds\":3,\"len\":4,\"now\":%.3f,\"phaseEnd\":0,\"roundStart\":0,"
		"\"spectator\":true,\"humanSeat\":-1,\"joined\":false,\"isHost\":false,\"roomCode\":\"\",\"bots\":0,"
		"\"minPlayers\":%d,\"maxPlayers\":%d,\"pace\":\"slow\",\"attemptSeconds\":10,\"sdAttemptSeconds\":6,"
		"\"turnMode\":\"simultaneous\",\"turnPlayer\":-1,\"turnNumber\":0,\"turnOrder\":[],\"secret\":null,"
		"\"winner\":-1,\"alertPlayer\":-1,\"suddenDeathEnd\":0,\"players\":[],\"entries\":[],\"private\":[],\"events\":[]}",
		Now, kMinPlayers, kMaxPlayers);
	return Buf;
}

struct FRoomHub
{
	std::map<std::string, std::unique_ptr<FWebSession>> ByCode;
	std::map<std::string, std::string> TokenRoom;
	std::string LastCookie;
	int DefaultBots = 0;

	FWebSession* NewRoom()
	{
		auto S = std::make_unique<FWebSession>();
		S->OpenLobby(DefaultBots);
		int Guard = 0;
		while (ByCode.count(S->RoomCode) && Guard++ < 32) S->RoomCode = MakeRoomCode();
		FWebSession* P = S.get();
		ByCode[P->RoomCode] = std::move(S);
		return P;
	}

	FWebSession* Find(const std::string& Token, const std::map<std::string, std::string>& Q)
	{
		if (!Token.empty())
		{
			auto It = TokenRoom.find(Token);
			if (It != TokenRoom.end())
			{
				auto R = ByCode.find(It->second);
				if (R != ByCode.end()) return R->second.get();
			}
		}
		if (Q.count("code") && !Q.at("code").empty())
		{
			auto R = ByCode.find(NormCode(Q.at("code")));
			if (R != ByCode.end()) return R->second.get();
		}
		return nullptr;
	}

	void Track(FWebSession* R)
	{
		if (!R) return;
		LastCookie = R->PendingSetCookie;
		if (!R->PendingSetCookie.empty()) TokenRoom[R->PendingSetCookie] = R->RoomCode;
		for (const auto& C : R->Clients) TokenRoom[C.Token] = R->RoomCode;
	}

	void TickAll(double Now)
	{
		for (auto& KV : ByCode) KV.second->Tick(Now);
	}

	std::string Handle(const std::string& Path, std::map<std::string, std::string> Q, const std::string& Token, double Now)
	{
		LastCookie.clear();
		if (Q.count("code")) Q["code"] = NormCode(Q["code"]);
		const bool bHasCode = Q.count("code") && !Q["code"].empty();
		if (Path == "/api/solo" || Path == "/api/create" || (Path == "/api/join" && !bHasCode))
		{
			FWebSession* R = NewRoom();
			const std::string P = (Path == "/api/create") ? "/api/join" : Path;
			std::string Body = R->HandleApi(P, Q, "", Now);
			Track(R);
			return Body;
		}
		FWebSession* R = Find(Token, Q);
		if (!R)
		{
			if (Path == "/api/state" || Path == "/api/stream") return LandingJson(Now);
			// Cookie/token de una instancia anterior (Render se duerme): crear sala nueva.
			if (Path == "/api/join" && !bHasCode)
			{
				R = NewRoom();
				std::string Body = R->HandleApi("/api/join", Q, "", Now);
				Track(R);
				return Body;
			}
			if (Path == "/api/join") return "{\"ok\":false,\"error\":\"esa sala no existe\"}";
			return "{\"ok\":false,\"error\":\"crea una sala o entra con el enlace\"}";
		}
		std::string Body = R->HandleApi(Path, Q, Token, Now);
		Track(R);
		return Body;
	}
};

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static void SendAll(int Fd, const std::string& S)
{
	size_t Off = 0;
	while (Off < S.size()) { const ssize_t N = send(Fd, S.data() + Off, S.size() - Off, MSG_NOSIGNAL); if (N <= 0) break; Off += size_t(N); }
}

static bool EndsWith(const std::string& S, const char* Ext)
{
	const size_t N = std::strlen(Ext);
	return S.size() >= N && std::strcmp(S.c_str() + (S.size() - N), Ext) == 0;
}

static const char* MimeFor(const std::string& Path)
{
	if (EndsWith(Path, ".html"))  return "text/html; charset=utf-8";
	if (EndsWith(Path, ".js"))    return "application/javascript; charset=utf-8";
	if (EndsWith(Path, ".css"))   return "text/css; charset=utf-8";
	if (EndsWith(Path, ".json") || EndsWith(Path, ".map")) return "application/json; charset=utf-8";
	if (EndsWith(Path, ".txt"))   return "text/plain; charset=utf-8";
	if (EndsWith(Path, ".svg"))   return "image/svg+xml";
	if (EndsWith(Path, ".ico"))   return "image/x-icon";
	if (EndsWith(Path, ".png"))   return "image/png";
	if (EndsWith(Path, ".woff2")) return "font/woff2";
	return "application/octet-stream";
}

static std::string HttpResponse(const std::string& Body, const char* Type, int Code = 200, bool bImmutable = false, const std::string& Extra = "")
{
	return "HTTP/1.1 " + std::to_string(Code) + (Code == 200 ? " OK" : " Not Found") + "\r\nContent-Type: " + Type +
		"\r\nContent-Length: " + std::to_string(Body.size()) +
		"\r\nCache-Control: " + (bImmutable ? "public, max-age=31536000, immutable" : "no-store") +
		"\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: *\r\nConnection: close" + Extra + "\r\n\r\n" + Body;
}

static std::string TokenFromRequest(const std::string& Req, const std::map<std::string, std::string>& Q)
{
	if (Q.count("token") && !Q.at("token").empty()) return Q.at("token");
	size_t P = Req.find("\r\nCookie:");
	if (P == std::string::npos) P = Req.find("\nCookie:");
	if (P == std::string::npos) return "";
	size_t End = Req.find("\r\n", P + 2);
	if (End == std::string::npos) End = Req.find('\n', P + 1);
	const std::string Cookies = Req.substr(P, End == std::string::npos ? std::string::npos : End - P);
	const size_t T = Cookies.find("pf=");
	if (T == std::string::npos) return "";
	size_t B = T + 3, E = Cookies.find(';', B);
	std::string Tok = Cookies.substr(B, E == std::string::npos ? std::string::npos : E - B);
	while (!Tok.empty() && (Tok.back() == '\r' || Tok.back() == ' ')) Tok.pop_back();
	return Tok;
}

static std::string ServeStatic(const std::string& WebDir, std::string Path)
{
	if (Path.empty() || Path == "/") Path = "/index.html";
	if (Path.find("..") != std::string::npos || Path[0] != '/') return "";
	return ReadFile(WebDir + Path);
}

struct FSseClient { int Fd; std::string Token; };

static bool SendSse(int Fd, const std::string& Json)
{
	const std::string Frame = "data: " + Json + "\n\n";
	size_t Off = 0;
	while (Off < Frame.size())
	{
		const ssize_t N = send(Fd, Frame.data() + Off, Frame.size() - Off, MSG_NOSIGNAL);
		if (N <= 0) return false;
		Off += size_t(N);
	}
	return true;
}

static int RunServe(int Port, int DefaultBots, const std::string& WebDir)
{
	if (ReadFile(WebDir + "/index.html").empty())
	{
		std::fprintf(stderr, "No encuentro %s/index.html\n", WebDir.c_str());
		std::fprintf(stderr, "Compila el frontend: (cd Tools/Sandbox/web-app && npm install && npm run build)\n");
		return 1;
	}

	signal(SIGPIPE, SIG_IGN);
	const int L = socket(AF_INET, SOCK_STREAM, 0);
	int One = 1; setsockopt(L, SOL_SOCKET, SO_REUSEADDR, &One, sizeof One);
	sockaddr_in Addr{}; Addr.sin_family = AF_INET; Addr.sin_port = htons(uint16_t(Port)); Addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(L, (sockaddr*)&Addr, sizeof Addr) != 0) { std::perror("bind"); return 1; }
	listen(L, 32);
	fcntl(L, F_SETFL, fcntl(L, F_GETFL) | O_NONBLOCK);

	using ClockT = std::chrono::steady_clock;
	const auto T0 = ClockT::now();
	auto NowFn = [&]() { return 1000.0 + std::chrono::duration<double>(ClockT::now() - T0).count(); };

	FRoomHub Hub;
	Hub.DefaultBots = DefaultBots;
	std::vector<FSseClient> Sse;
	std::printf("Picas y Famas sandbox web: http://127.0.0.1:%d/\n", Port);
	std::printf("Salas bajo demanda · minimo %d, maximo %d jugadores\n", kMinPlayers, kMaxPlayers);
	std::printf("Crea sala en el navegador e invita con el enlace. Ctrl+C para parar.\n");
	std::fflush(stdout);

	double LastSse = 0;
	for (;;)
	{
		const double Now = NowFn();
		Hub.TickAll(Now);

		if (Now - LastSse >= 0.12)
		{
			LastSse = Now;
			for (size_t i = 0; i < Sse.size();)
			{
				if (!SendSse(Sse[i].Fd, Hub.Handle("/api/state", {}, Sse[i].Token, Now)))
				{
					close(Sse[i].Fd);
					Sse.erase(Sse.begin() + int(i));
				}
				else ++i;
			}
		}

		for (int k = 0; k < 32; ++k)
		{
			const int C = accept(L, nullptr, nullptr);
			if (C < 0) break;
			fcntl(C, F_SETFL, fcntl(C, F_GETFL) & ~O_NONBLOCK);
#ifdef SO_NOSIGPIPE
			int NoPipe = 1; setsockopt(C, SOL_SOCKET, SO_NOSIGPIPE, &NoPipe, sizeof NoPipe);
#endif
			timeval Tv{ 0, 200000 }; setsockopt(C, SOL_SOCKET, SO_RCVTIMEO, &Tv, sizeof Tv);
			std::string Req; char Buf[2048];
			while (Req.find("\r\n\r\n") == std::string::npos && Req.size() < 65536)
			{
				const ssize_t N = recv(C, Buf, sizeof Buf, 0);
				if (N <= 0) break;
				Req.append(Buf, size_t(N));
			}
			std::string Method, Target;
			{ std::stringstream SS(Req); SS >> Method >> Target; }
			std::string Path = Target, Query;
			const size_t Qm = Target.find('?');
			if (Qm != std::string::npos) { Path = Target.substr(0, Qm); Query = Target.substr(Qm + 1); }
			auto Q = ParseQuery(Query);
			const std::string Tok = TokenFromRequest(Req, Q);

			if (Path == "/api/stream")
			{
				const char* Head =
					"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=utf-8\r\nCache-Control: no-store\r\n"
					"Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\nX-Accel-Buffering: no\r\n\r\n";
				SendAll(C, Head);
				SendSse(C, Hub.Handle("/api/state", Q, Tok, NowFn()));
				Sse.push_back({ C, Tok });
				continue;
			}

			std::string Extra;
			std::string Body;
			if (Path.rfind("/api/", 0) == 0)
			{
				Body = Hub.Handle(Path, Q, Tok, NowFn());
				if (!Hub.LastCookie.empty())
					Extra = "\r\nSet-Cookie: pf=" + Hub.LastCookie + "; Path=/; SameSite=Lax; Max-Age=86400";
			}
			std::string Resp;
			if (Path.rfind("/api/", 0) == 0) Resp = HttpResponse(Body, "application/json; charset=utf-8", 200, false, Extra);
			else
			{
				const std::string File = ServeStatic(WebDir, Path);
				if (File.empty()) Resp = HttpResponse("not found", "text/plain", 404);
				else Resp = HttpResponse(File, MimeFor(Path == "/" ? std::string("/index.html") : Path), 200, Path.rfind("/_next/", 0) == 0);
			}
			SendAll(C, Resp);
			close(C);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

int main(int Argc, char** Argv)
{
	std::string Mode = Argc > 1 ? Argv[1] : "play";
	uint64_t Seed = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
	bool bQuiet = false;
	int Port = 8080;
	bool bPortSet = false;
	std::vector<int> Nums;
	for (int i = 2; i < Argc; ++i)
	{
		if (!std::strcmp(Argv[i], "--seed") && i + 1 < Argc) { Seed = std::strtoull(Argv[++i], nullptr, 10); continue; }
		if (!std::strcmp(Argv[i], "--quiet")) { bQuiet = true; continue; }
		if (!std::strcmp(Argv[i], "--spectator")) { continue; }
		if (!std::strcmp(Argv[i], "--port") && i + 1 < Argc) { Port = std::atoi(Argv[++i]); bPortSet = true; continue; }
		if (!std::strcmp(Argv[i], "--human")) { SetPace("normal"); continue; }
		if (!std::strcmp(Argv[i], "--pace") && i + 1 < Argc) { SetPace(Argv[++i]); continue; }
		if (!std::strcmp(Argv[i], "--attempt") && i + 1 < Argc) { gAttemptSeconds = std::atof(Argv[++i]); continue; }
		if (!std::strcmp(Argv[i], "--turns") && i + 1 < Argc) { gTurnMode = ParseTurnMode(Argv[++i]); continue; }
		if (!std::strcmp(Argv[i], "--decoy-cap") && i + 1 < Argc) { gDecoyCap = std::atoi(Argv[++i]); continue; }
		Nums.push_back(std::atoi(Argv[i]));
	}
	if (Mode == "play") return RunPlay(Nums.size() > 0 ? std::max(2, std::min(7, Nums[0])) : 3, Seed);
	if (Mode == "sim")  return RunSim(Nums.size() > 0 ? Nums[0] : 200, Nums.size() > 1 ? Nums[1] : 5, Seed, bQuiet);
	if (Mode == "serve")
	{
		if (!bPortSet)
		{
			if (const char* Env = std::getenv("PORT"))
			{
				const int P = std::atoi(Env);
				if (P > 0) Port = P;
			}
		}
		if (gReadLatency == 0.0) SetPace("slow");
		std::string Dir = Argv[0]; const size_t Slash = Dir.find_last_of('/');
		Dir = Slash == std::string::npos ? "." : Dir.substr(0, Slash);
		return RunServe(Port, Nums.size() > 0 ? std::max(0, std::min(6, Nums[0])) : 0, Dir + "/web");
	}
	std::printf("uso: pf_sandbox play [bots] [--seed N] | sim [partidas] [jugadores] [--seed N] [--quiet] | serve [bots] [--port P]\n");
	std::printf("     share.sh arranca serve + tunel gratis (Cloudflare) para jugar con amigos\n");
	return 1;
}
