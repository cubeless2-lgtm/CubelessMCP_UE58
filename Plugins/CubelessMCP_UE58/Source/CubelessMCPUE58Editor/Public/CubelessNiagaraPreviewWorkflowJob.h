#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CubelessNiagaraPreviewWorkflowJob.generated.h"
namespace Cubeless::UE58::NiagaraPreviewWorkflows { struct FJobState; }
UCLASS(Transient, meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewWorkflowJob : public UObject
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintReadOnly, Category="Cubeless|Niagara|Preview") bool bIsTerminal=false;
 UFUNCTION(BlueprintPure, Category="Cubeless|Niagara|Preview", meta=(AIIgnore,ScriptName="get_snapshot_json")) FString GetSnapshotJson() const;
 UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(AIIgnore,ScriptName="cancel_job")) bool CancelJob(const FString& Reason);
 virtual void BeginDestroy() override;
private:
 friend struct Cubeless::UE58::NiagaraPreviewWorkflows::FJobState;
 UPROPERTY(Transient) FString SnapshotJson;
};
