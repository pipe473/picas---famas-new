// Picas y Famas - nucleo de reglas.
// Este archivo es C++ puro (sin dependencias de Unreal) para poder compilarlo y testearlo fuera del motor.
#pragma once

#include <cstdint>

namespace PF
{
	// Longitud del codigo secreto: 4 digitos (3-5 jugadores) o 5 digitos (6-8 jugadores).
	constexpr int32_t kMinCodeLength = 3;
	constexpr int32_t kMaxCodeLength = 5;

	// Un intento o codigo se empaqueta en 4 bits por digito: el digito i vive en los bits [4i, 4i+3].
	// Con 5 digitos caben en 20 bits, por eso el tipo es uint32_t. 0xFFFFFFFF = "oculto" (modo Equipos).
	using PackedCode = uint32_t;
	constexpr PackedCode kMaskedCode = 0xFFFFFFFFu;

	struct FGuessResult
	{
		uint8_t Famas = 0;   // digito correcto en posicion correcta
		uint8_t Picas = 0;   // digito correcto en posicion incorrecta

		bool operator==(const FGuessResult& O) const { return Famas == O.Famas && Picas == O.Picas; }
		bool operator!=(const FGuessResult& O) const { return !(*this == O); }
	};

	inline uint8_t DigitAt(PackedCode Packed, int32_t Index)
	{
		return static_cast<uint8_t>((Packed >> (4 * Index)) & 0xFu);
	}

	inline PackedCode PackDigits(const uint8_t* Digits, int32_t Len)
	{
		PackedCode P = 0;
		for (int32_t i = 0; i < Len; ++i)
		{
			P |= static_cast<PackedCode>(Digits[i] & 0xFu) << (4 * i);
		}
		return P;
	}

	inline void UnpackDigits(PackedCode Packed, int32_t Len, uint8_t* OutDigits)
	{
		for (int32_t i = 0; i < Len; ++i)
		{
			OutDigits[i] = DigitAt(Packed, i);
		}
	}

	// Mascara de presencia: bit d activo si el digito d aparece en el codigo.
	inline uint16_t DigitMaskOf(PackedCode Packed, int32_t Len)
	{
		uint16_t Mask = 0;
		for (int32_t i = 0; i < Len; ++i)
		{
			Mask |= static_cast<uint16_t>(1u << DigitAt(Packed, i));
		}
		return Mask;
	}

	inline int32_t PopCount16(uint16_t V)
	{
		int32_t C = 0;
		while (V) { V &= static_cast<uint16_t>(V - 1); ++C; }
		return C;
	}

	// Valido = Len digitos, cada uno 0..9, sin repetir, y sin basura en los bits superiores.
	inline bool IsValidCode(PackedCode Packed, int32_t Len)
	{
		if (Len < kMinCodeLength || Len > kMaxCodeLength) return false;
		if ((Packed >> (4 * Len)) != 0) return false;
		uint16_t Seen = 0;
		for (int32_t i = 0; i < Len; ++i)
		{
			const uint8_t D = DigitAt(Packed, i);
			if (D > 9) return false;
			const uint16_t Bit = static_cast<uint16_t>(1u << D);
			if (Seen & Bit) return false;
			Seen |= Bit;
		}
		return true;
	}

	// Evaluacion O(1): sin bucles anidados ni asignaciones.
	//  - Famas: nibbles iguales quedan a cero tras el XOR.
	//  - Picas: digitos compartidos (popcount de la interseccion de mascaras) menos las Famas.
	inline FGuessResult Evaluate(PackedCode Code, uint16_t CodeMask, PackedCode Guess, int32_t Len)
	{
		FGuessResult R;
		const PackedCode Diff = Code ^ Guess;
		for (int32_t i = 0; i < Len; ++i)
		{
			R.Famas += (((Diff >> (4 * i)) & 0xFu) == 0) ? 1 : 0;
		}
		const uint16_t Shared = static_cast<uint16_t>(CodeMask & DigitMaskOf(Guess, Len));
		R.Picas = static_cast<uint8_t>(PopCount16(Shared) - R.Famas);
		return R;
	}

	inline FGuessResult Evaluate(PackedCode Code, PackedCode Guess, int32_t Len)
	{
		return Evaluate(Code, DigitMaskOf(Code, Len), Guess, Len);
	}

	// 10 * 9 * 8 * ... (Len factores)
	inline int32_t NumPossibleCodes(int32_t Len)
	{
		int32_t N = 1;
		for (int32_t i = 0; i < Len; ++i) N *= (10 - i);
		return N;
	}

	// RNG determinista (SplitMix64). El servidor lo siembra por ronda; con la semilla se reproduce la partida.
	struct FRng
	{
		uint64_t State;

		explicit FRng(uint64_t Seed) : State(Seed) {}

		uint64_t Next64()
		{
			uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}

		// Entero uniforme en [Min, Max], ambos inclusive.
		int32_t RangeInclusive(int32_t Min, int32_t Max)
		{
			const uint64_t Span = static_cast<uint64_t>(Max - Min) + 1u;
			return Min + static_cast<int32_t>(Next64() % Span);
		}
	};

	// Fisher-Yates parcial sobre 0..9: toma los Len primeros.
	inline PackedCode GenerateCode(FRng& Rng, int32_t Len)
	{
		uint8_t Digits[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
		for (int32_t i = 0; i < Len; ++i)
		{
			const int32_t J = Rng.RangeInclusive(i, 9);
			const uint8_t T = Digits[i]; Digits[i] = Digits[J]; Digits[J] = T;
		}
		return PackDigits(Digits, Len);
	}
}
