#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CubelessNiagaraInputParameterJob.generated.h"
namespace Cubeless::UE58::NiagaraInputParameterAsync { struct FJobState; }
// Editor-only transient controller for the three existing Niagara input writer operations.
UCLASS(Transient, meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraInputParameterJob : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="Cubeless|Niagara")
    bool bIsTerminal=false;
    UFUNCTION(BlueprintPure, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="get_snapshot_json"))
    FString GetSnapshotJson() const;
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="cancel_job"))
    bool CancelJob(const FString& Reason);
    virtual void BeginDestroy() override;
private:
    friend struct Cubeless::UE58::NiagaraInputParameterAsync::FJobState;
    UPROPERTY(Transient)
    FString SnapshotJson;
};
