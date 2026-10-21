#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraPromotionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPromotionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="audit_niagara_promotion_candidate"))
    static FString AuditNiagaraPromotionCandidate(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="promote_niagara_system_candidate"))
    static FString PromoteNiagaraSystemCandidate(const FString& ParamsJson);
};
