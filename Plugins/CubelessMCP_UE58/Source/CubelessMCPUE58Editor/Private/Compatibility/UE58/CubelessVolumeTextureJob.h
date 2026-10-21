#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CubelessVolumeTextureJob.generated.h"
namespace Cubeless::UE58::VolumeTexture { struct FJobState; }
// This transient Editor wrapper is never assigned to a runtime asset.
UCLASS(Transient)
class CUBELESSMCPUE58EDITOR_API UCubelessVolumeTextureJob : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly,Category="Cubeless|Texture")
    bool bIsTerminal=false;
    UFUNCTION(BlueprintPure,Category="Cubeless|Texture",meta=(AIIgnore,ScriptName="get_snapshot_json"))
    FString GetSnapshotJson() const;
    UFUNCTION(BlueprintCallable,Category="Cubeless|Texture",meta=(AIIgnore,ScriptName="cancel_job"))
    bool CancelJob(const FString& Reason);
    virtual void BeginDestroy() override;
private:
    friend struct Cubeless::UE58::VolumeTexture::FJobState;
    UPROPERTY(Transient)
    FString SnapshotJson;
};
