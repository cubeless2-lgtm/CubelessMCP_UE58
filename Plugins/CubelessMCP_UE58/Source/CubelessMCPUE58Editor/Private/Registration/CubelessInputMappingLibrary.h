#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessInputMappingLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessInputMappingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Public Python contract is three strings; the four bools retain legacy internal command modifiers.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Project", meta=(AIIgnore, ScriptName="create_input_mapping"))
    static FString CreateInputMapping(const FString& ActionName, const FString& Key, const FString& InputType,
        bool bShift, bool bCtrl, bool bAlt, bool bCmd);
};
