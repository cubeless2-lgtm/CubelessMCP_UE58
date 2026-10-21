#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraPreviewLabLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewLabLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="get_niagara_preview_lab_state"))
    static FString GetNiagaraPreviewLabState(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="cleanup_niagara_preview_lab"))
    static FString CleanupNiagaraPreviewLab(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="capture_niagara_preview_lab_view"))
    static FString CaptureNiagaraPreviewLabView(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="preview_niagara_system_in_preview_lab"))
    static FString PreviewNiagaraSystemInPreviewLab(const FString& ParamsJson);
};
