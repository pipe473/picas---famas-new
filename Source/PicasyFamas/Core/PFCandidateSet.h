// Conjunto de codigos aun consistentes con la informacion publica verdadera.
// Sirve para dos cosas: medir cuanta informacion aporta cada intento (bits) y detectar el instante
// en que el codigo queda logicamente determinado (Pista Clave). Vive solo en el servidor.
#pragma once

#include <cstdint>
#include <cmath>
#include "PFCodeMath.h"

namespace PF
{
	struct FCandidateSet
	{
		// 10 * 9 * 8 * 7 * 6 = 30 240 (codigo de 5 digitos). Memoria fija: sin asignaciones en caliente.
		static constexpr int32_t kMaxCandidates = 30240;

		PackedCode Codes[kMaxCandidates];
		int32_t    Num = 0;
		int32_t    Len = 4;

		void Reset(int32_t InLen)
		{
			Len = InLen;
			Num = 0;
			uint8_t Digits[kMaxCodeLength] = {};
			Enumerate(Digits, 0, 0);
		}

		// Elimina los codigos que no habrian producido exactamente Observed para Guess.
		// Devuelve la informacion aportada en bits: log2(antes / despues). 0 si no elimina nada.
		float Filter(PackedCode Guess, FGuessResult Observed)
		{
			const int32_t Before = Num;
			int32_t Write = 0;
			for (int32_t i = 0; i < Num; ++i)
			{
				const PackedCode C = Codes[i];
				if (Evaluate(C, Guess, Len) == Observed)
				{
					Codes[Write++] = C;
				}
			}
			Num = Write;
			if (Before <= 0 || Num <= 0 || Num == Before) return 0.f;
			return static_cast<float>(std::log2(static_cast<double>(Before) / static_cast<double>(Num)));
		}

		bool Contains(PackedCode Code) const
		{
			for (int32_t i = 0; i < Num; ++i) if (Codes[i] == Code) return true;
			return false;
		}

		// Solo tiene sentido cuando Num == 1.
		PackedCode Single() const { return Num == 1 ? Codes[0] : kMaskedCode; }

	private:
		void Enumerate(uint8_t* Digits, int32_t Depth, uint16_t Used)
		{
			if (Depth == Len)
			{
				Codes[Num++] = PackDigits(Digits, Len);
				return;
			}
			for (uint8_t D = 0; D <= 9; ++D)
			{
				const uint16_t Bit = static_cast<uint16_t>(1u << D);
				if (Used & Bit) continue;
				Digits[Depth] = D;
				Enumerate(Digits, Depth + 1, static_cast<uint16_t>(Used | Bit));
			}
		}
	};
}
