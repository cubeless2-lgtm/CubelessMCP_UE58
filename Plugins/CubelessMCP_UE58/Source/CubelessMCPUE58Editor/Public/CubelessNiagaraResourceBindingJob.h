#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CubelessNiagaraResourceBindingJob.generated.h"
namespace Cubeless::UE58::NiagaraResourceBindingAsync { struct FJobState; }
// Editor-only transient controller for the two existing Niagara resource binding operations.
UCLASS(Transient, meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraResourceBindingJob : public UObject
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
    friend struct Cubeless::UE58::NiagaraResourceBindingAsync::FJobState;
    UPROPERTY(Transient)
    FString SnapshotJson;
};
