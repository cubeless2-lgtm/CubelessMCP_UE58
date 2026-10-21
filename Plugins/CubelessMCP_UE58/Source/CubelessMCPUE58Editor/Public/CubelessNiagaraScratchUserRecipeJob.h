#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CubelessNiagaraScratchUserRecipeJob.generated.h"
namespace Cubeless::UE58::NiagaraScratchUserRecipeAsync { struct FJobAccess; }
// One original leaf invocation. Never serialized into a target asset.
UCLASS(Transient, meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeJob : public UObject
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
    friend struct Cubeless::UE58::NiagaraScratchUserRecipeAsync::FJobAccess;
    UPROPERTY(Transient)
    FString SnapshotJson;
};
