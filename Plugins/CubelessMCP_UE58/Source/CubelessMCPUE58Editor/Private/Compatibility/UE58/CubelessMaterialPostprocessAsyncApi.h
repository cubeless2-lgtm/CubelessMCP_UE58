#pragma once
#include "CoreMinimal.h"
class UCubelessMaterialPostprocessJob;
namespace Cubeless::UE58::MaterialPostprocessAsync
{
    enum class EJobKind:uint8 { CompileAndSave, CacheRefresh, ReplaceCollections, ReplaceTextures, ReplaceCollectionParameters };
    struct FJobAccess;
    CUBELESSMCPUE58EDITOR_API UCubelessMaterialPostprocessJob* StartCompileAndSave(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessMaterialPostprocessJob* StartCacheRefresh(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessMaterialPostprocessJob* StartReplaceCollections(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessMaterialPostprocessJob* StartReplaceTextures(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessMaterialPostprocessJob* StartReplaceCollectionParameters(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessMaterialPostprocessJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessMaterialPostprocessJob* Job,const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
    // Internal concrete-controller publication/dispatch registry, not a public tool surface.
    void RegisterJob(UCubelessMaterialPostprocessJob* Job,EJobKind Kind,const FGuid& Id);
    void Publish(UCubelessMaterialPostprocessJob* Job,const FString& Snapshot,bool bTerminal,bool bObjectPublicationSafe);
    bool IsStopping();
}
