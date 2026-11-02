#pragma once
#include "CoreMinimal.h"

namespace RXLODTooltips
{
    FText Label(const FString& Name);
    FText Property(const FName& Owner, const FName& Name);
    bool HasProperty(const FName& Owner, const FName& Name);
}
