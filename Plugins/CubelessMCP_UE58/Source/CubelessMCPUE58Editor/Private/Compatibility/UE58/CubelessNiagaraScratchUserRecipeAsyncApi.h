#pragma once
#include "CoreMinimal.h"
class UCubelessNiagaraScratchUserRecipeJob;
namespace Cubeless::UE58::NiagaraScratchUserRecipeAsync
{
    enum class EJobKind : uint8 { CompileStatus, ScalarBinding, ArrayBinding, RemoveUnusedPath, CollectionScriptRemap, RendererCleanup };
    struct FJobAccess;
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeJob* StartCompileStatus(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeJob* StartScalarBinding(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeJob* StartArrayBinding(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeJob* StartRemoveUnusedPath(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeJob* StartCollectionScriptRemap(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeJob* StartRendererCleanup(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraScratchUserRecipeJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraScratchUserRecipeJob* Job, const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
    // Internal cache publication for the existing compile-status and shared scalar/array binding controllers only.
    CUBELESSMCPUE58EDITOR_API void RegisterJob(UCubelessNiagaraScratchUserRecipeJob* Job, EJobKind Kind, const FGuid& JobId);
    CUBELESSMCPUE58EDITOR_API void Publish(UCubelessNiagaraScratchUserRecipeJob* Job, const FString& Snapshot, bool bTerminal, bool bObjectPublicationSafe);
    CUBELESSMCPUE58EDITOR_API bool IsStopping();
}
