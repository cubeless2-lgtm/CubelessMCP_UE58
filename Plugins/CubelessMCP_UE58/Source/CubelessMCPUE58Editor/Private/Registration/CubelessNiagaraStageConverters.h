#pragma once
#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"

class UClass;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }
namespace Cubeless::Registration
{
TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeNiagaraStageElementCountConverter();
bool PrepareNiagaraStageArrayDefaults(UClass* ToolsetClass, FString& Error, bool& bNormalized);
}
