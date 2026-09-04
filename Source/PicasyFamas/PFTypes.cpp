#include "PFTypes.h"
#include "Core/PFCodeMath.h"

int32 UPFCodeLibrary::PackDigits(const TArray<uint8>& Digits)
{
	const int32 Len = FMath::Min(Digits.Num(), PF::kMaxCodeLength);
	uint8 Buf[PF::kMaxCodeLength] = {};
	for (int32 i = 0; i < Len; ++i) Buf[i] = Digits[i];
	return static_cast<int32>(PF::PackDigits(Buf, Len));
}

TArray<uint8> UPFCodeLibrary::UnpackDigits(int32 PackedGuess, int32 CodeLength)
{
	TArray<uint8> Out;
	const int32 Len = FMath::Clamp(CodeLength, PF::kMinCodeLength, PF::kMaxCodeLength);
	Out.SetNumUninitialized(Len);
	PF::UnpackDigits(static_cast<PF::PackedCode>(PackedGuess), Len, Out.GetData());
	return Out;
}

bool UPFCodeLibrary::IsValidGuess(int32 PackedGuess, int32 CodeLength)
{
	return PF::IsValidCode(static_cast<PF::PackedCode>(PackedGuess), CodeLength);
}

FString UPFCodeLibrary::GuessToString(int32 PackedGuess, int32 CodeLength)
{
	if (IsMaskedGuess(PackedGuess))
	{
		return FString::ChrN(CodeLength, TEXT('*'));
	}
	FString S;
	const int32 Len = FMath::Clamp(CodeLength, PF::kMinCodeLength, PF::kMaxCodeLength);
	for (int32 i = 0; i < Len; ++i)
	{
		S.AppendChar(static_cast<TCHAR>(TEXT('0') + PF::DigitAt(static_cast<PF::PackedCode>(PackedGuess), i)));
	}
	return S;
}
