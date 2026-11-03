#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace RXImpostorDirections
{
    inline bool IsValid(int32 Count) { return Count == 2 || Count == 4; }
    inline double Angle(int32 Plane, int32 Count) { return UE_PI * double(Plane) / double(Count); }
    inline bool Read(const FJsonObject& Root, int32 Version, int32& Count)
    {
        if (Version < 5) { Count = 4; return true; }
        double Value = 0;
        if (!Root.TryGetNumberField(TEXT("impostor_directions"), Value) || (Value != 2.0 && Value != 4.0)) { return false; }
        Count = int32(Value); return true;
    }
}
