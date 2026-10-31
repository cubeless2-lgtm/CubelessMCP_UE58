#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::InstanceVertexColors
{
// Internal count-only preflight, also exercised by the Editor regression test.
bool ProjectCounts(const TArray<int32>& Overrides, const TArray<int32>& Caches,
 int32 Selected, int32 Wanted, bool Clear, bool Save, int64& TotalOverrides, int64& TotalCaches);
FString Inspect(UObject* Object, int32 LOD, int32 Start, int32 Count, bool Positions);
FString Apply(UObject* Object, int32 LOD, const FString& ColorsJson, bool Clear, bool Execute, bool AllowReplace, bool Save, bool AllowSaveDirtyPackage);
}
