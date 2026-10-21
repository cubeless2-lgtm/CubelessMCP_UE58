#include "CubelessNiagaraScratchUserRecipeAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraScratchUserRecipeAsyncApi.h"
UCubelessNiagaraScratchUserRecipeJob* UCubelessNiagaraScratchUserRecipeAsyncLibrary::StartCompileStatusJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchUserRecipeAsync::StartCompileStatus(ParamsJson);}
UCubelessNiagaraScratchUserRecipeJob* UCubelessNiagaraScratchUserRecipeAsyncLibrary::StartScalarBindingJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchUserRecipeAsync::StartScalarBinding(ParamsJson);}
UCubelessNiagaraScratchUserRecipeJob* UCubelessNiagaraScratchUserRecipeAsyncLibrary::StartArrayBindingJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchUserRecipeAsync::StartArrayBinding(ParamsJson);}

UCubelessNiagaraScratchUserRecipeJob* UCubelessNiagaraScratchUserRecipeAsyncLibrary::StartRemoveNiagaraUnusedModuleInputPathJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchUserRecipeAsync::StartRemoveUnusedPath(ParamsJson);}

UCubelessNiagaraScratchUserRecipeJob* UCubelessNiagaraScratchUserRecipeAsyncLibrary::StartRemapNiagaraScriptParameterCollectionReferencesJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchUserRecipeAsync::StartCollectionScriptRemap(ParamsJson);}

UCubelessNiagaraScratchUserRecipeJob* UCubelessNiagaraScratchUserRecipeAsyncLibrary::StartCleanupNiagaraRenderersJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchUserRecipeAsync::StartRendererCleanup(ParamsJson);}
