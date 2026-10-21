#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosCinematicSequenceAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosCinematicSequenceAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_cinematic_sequence_toolset"))
    static FString InstallChaosCinematicSequenceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_cinematic_sequence_toolset"))
    static FString VerifyChaosCinematicSequenceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_cinematic_sequence_toolset"))
    static FString RemoveChaosCinematicSequenceToolset(UClass* ToolsetClass);
};
