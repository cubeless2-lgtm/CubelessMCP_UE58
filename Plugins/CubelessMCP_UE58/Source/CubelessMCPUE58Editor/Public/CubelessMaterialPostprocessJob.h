#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CubelessMaterialPostprocessJob.generated.h"
namespace Cubeless::UE58::MaterialPostprocessAsync { struct FJobAccess; }
UCLASS(Transient,meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessMaterialPostprocessJob:public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly,Category="Cubeless|Material")
    bool bIsTerminal=false;
    UFUNCTION(BlueprintPure,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="get_snapshot_json"))
    FString GetSnapshotJson() const;
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="cancel_job"))
    bool CancelJob(const FString& Reason);
    virtual void BeginDestroy() override;
private:
    friend struct Cubeless::UE58::MaterialPostprocessAsync::FJobAccess;
    UPROPERTY(Transient)
    FString SnapshotJson;
};
