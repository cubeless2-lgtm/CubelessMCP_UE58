#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FProperty;
class USCS_Node;

namespace Cubeless::UE58::Component
{
struct FReadResult
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    FString BlueprintPath;
    bool bLoadedDuringRead = false;
    bool bDirtyObserved = false;
    bool bDirtyAfterObserved = false;
    bool bDirtyBefore = false;
    bool bDirtyAfter = false;
    int32 StatusBefore = INDEX_NONE;
    int32 StatusAfter = INDEX_NONE;
    int32 NodeVisits = 0;
    // Per-call handles for authoring preflight; never retained across callbacks/compile.
    TMap<FString, TWeakObjectPtr<USCS_Node>> LocalNodes;
};

bool ReadComponentValue(const FProperty* Property, const void* Address, int32 MaxChars,
    TSharedPtr<FJsonValue>& Out, FString& Encoding, FString& Error);

bool ReadLocalComponents(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName,
    bool bReadProperty, int32 MaxComponents, int32 MaxValueChars, FReadResult& Out, FString& Code, FString& Error);
}
