#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraSystemAnalysisLibrary.generated.h"

// Internal metadata/SDK option normalization only; the original public aggregate stays in Python.
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraSystemAnalysisLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="inspect_niagara_system_analysis_metadata"))
    static FString InspectNiagaraSystemAnalysisMetadata(const FString& ParamsJson);
};
