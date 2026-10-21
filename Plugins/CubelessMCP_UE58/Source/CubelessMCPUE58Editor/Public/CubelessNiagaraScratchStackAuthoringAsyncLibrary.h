#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraScratchStackAuthoringAsyncLibrary.generated.h"
class UCubelessNiagaraScratchStackAuthoringJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchStackAuthoringAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_add_niagara_scratch_pad_module_input_job"))
    static UCubelessNiagaraScratchStackAuthoringJob* StartAddNiagaraScratchPadModuleInputJob(const FString& ParamsJson);
};
