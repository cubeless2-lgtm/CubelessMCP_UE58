#pragma once
#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "CubelessLoadedLevelInstancePackingLibrary.generated.h"

/** Editor-only binding; Toolset registration and operation journaling remain in Python. */
UCLASS()
class UCubelessLoadedLevelInstancePackingLibrary : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, DevelopmentOnly, ScriptName="execute_packing_request"))
    static FString ExecutePackingRequest(const FString& RequestJson);
};
