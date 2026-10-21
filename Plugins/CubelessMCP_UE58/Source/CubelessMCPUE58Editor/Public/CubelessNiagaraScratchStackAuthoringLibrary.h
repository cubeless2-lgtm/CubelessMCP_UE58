#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraScratchStackAuthoringLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchStackAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="add_niagara_scratch_pad_module_input"))
    static FString AddNiagaraScratchPadModuleInput(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="add_niagara_scratch_pad_assignment_module_to_stack"))
    static FString AddNiagaraScratchPadAssignmentModuleToStack(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="wrap_niagara_scratch_pad_output_with_stack_context"))
    static FString WrapNiagaraScratchPadOutputWithStackContext(const FString& ParamsJson);
};
