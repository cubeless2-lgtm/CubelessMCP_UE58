// HN: two concrete existing leaf controllers; no public/command dispatcher or transport.
#include "Compatibility/UE58/CubelessNiagaraScratchUserRecipeAsyncApi.h"
#include "CubelessNiagaraScratchUserRecipeJob.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include <atomic>
namespace Cubeless::UE58::NiagaraAuthoringInspection
{UCubelessNiagaraScratchUserRecipeJob* StartCompileStatusContinuation(const FString&);bool CancelCompileStatusContinuation(UCubelessNiagaraScratchUserRecipeJob*,const FString&);void ShutdownCompileStatusContinuation(const FString&);}
namespace Cubeless::UE58::NiagaraModuleInputPaths
{UCubelessNiagaraScratchUserRecipeJob* StartRemoveUnusedPathContinuation(const FString&);UCubelessNiagaraScratchUserRecipeJob* StartArrayBindingContinuation(const FString&);UCubelessNiagaraScratchUserRecipeJob* StartScalarBindingContinuation(const FString&);bool CancelScalarBindingContinuation(UCubelessNiagaraScratchUserRecipeJob*,const FString&);void ShutdownScalarBindingContinuation(const FString&);}
namespace Cubeless::UE58::NiagaraParameterCollections
{UCubelessNiagaraScratchUserRecipeJob* StartScriptRemapContinuation(const FString&);bool CancelScriptRemapContinuation(UCubelessNiagaraScratchUserRecipeJob*,const FString&);void ShutdownScriptRemapContinuation(const FString&);bool HasScriptRemapContinuationLease();}
namespace Cubeless::UE58::NiagaraRenderers
{UCubelessNiagaraScratchUserRecipeJob* StartCleanupContinuation(const FString&);bool CancelCleanupContinuation(UCubelessNiagaraScratchUserRecipeJob*,const FString&);void ShutdownCleanupContinuation(const FString&);bool HasCleanupContinuationLease();}
namespace Cubeless::UE58::NiagaraScratchUserRecipeAsync
{
namespace
{
std::atomic<bool> Stopping{false};bool NormalPreExit=false,Starting=false;FDelegateHandle PreExitHandle;
bool Safe(){return IsInGameThread()&&GEditor&&!GExitPurge&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&(!IsEngineExitRequested()||NormalPreExit);}
struct FRecord{EJobKind Kind=EJobKind::CompileStatus;FGuid Id;TWeakObjectPtr<UCubelessNiagaraScratchUserRecipeJob> Object;FString Snapshot;bool Terminal=false,Rooted=false,Registered=false;};
TMap<const UCubelessNiagaraScratchUserRecipeJob*,FRecord> Records;
void EditorExit(){TGuardValue<bool> Scope(NormalPreExit,true);Shutdown(TEXT("Normal Editor pre-exit; stop the two concrete HN continuations without asset replay."));}
}
struct FJobAccess
{
    static void Set(UCubelessNiagaraScratchUserRecipeJob* Job,const FString& Snapshot,bool Terminal){Job->SnapshotJson=Snapshot;Job->bIsTerminal=Terminal;}
};
bool IsStopping(){return Stopping.load();}
// Internal lifecycle query used only by these two concrete controllers.
bool CanPublishJobObject(){return Safe();}
void RegisterJob(UCubelessNiagaraScratchUserRecipeJob* Job,EJobKind Kind,const FGuid& Id)
{
    if(!Safe()||!IsValid(Job)||Job->GetClass()!=UCubelessNiagaraScratchUserRecipeJob::StaticClass())return;
    FRecord Record;Record.Registered=true;Record.Kind=Kind;Record.Id=Id;Record.Object=Job;Job->AddToRoot();Record.Rooted=true;Records.Add(Job,MoveTemp(Record));
    if(!PreExitHandle.IsValid())PreExitHandle=FEditorDelegates::OnEditorPreExit.AddStatic(&EditorExit);
}
void Publish(UCubelessNiagaraScratchUserRecipeJob* Job,const FString& Snapshot,bool Terminal,bool ObjectSafe)
{
    if(!IsInGameThread())return;FRecord* Record=Records.Find(Job);if(!Record&&!ObjectSafe){FRecord Unregistered;Unregistered.Snapshot=Snapshot;Records.Add(Job,MoveTemp(Unregistered));return;}if(!Record||Record->Terminal)return;
    // Plain metadata cache remains readable when publication to UObject fields is unsafe.
    Record->Snapshot=Snapshot;if(!ObjectSafe||!Safe())return;
    if(!Record->Registered||Record->Object.Get()!=Job||!IsValid(Job)||Job->GetClass()!=UCubelessNiagaraScratchUserRecipeJob::StaticClass())return;
    FJobAccess::Set(Job,Snapshot,Terminal);Record->Terminal=Terminal;
    if(Terminal&&Record->Rooted){Job->RemoveFromRoot();Record->Rooted=false;}
}
FString SnapshotJson(const UCubelessNiagaraScratchUserRecipeJob* Job)
{if(!IsInGameThread())return FString();const FRecord* Record=Records.Find(Job);return Record?Record->Snapshot:FString();}
bool Cancel(UCubelessNiagaraScratchUserRecipeJob* Job,const FString& Reason)
{
    if(!IsInGameThread())return false;const FRecord* Record=Records.Find(Job);if(!Record||!Record->Registered||Record->Terminal)return false;const EJobKind Kind=Record->Kind;
    if(Kind==EJobKind::CompileStatus)return NiagaraAuthoringInspection::CancelCompileStatusContinuation(Job,Reason);
    if(Kind==EJobKind::ScalarBinding||Kind==EJobKind::ArrayBinding||Kind==EJobKind::RemoveUnusedPath)return NiagaraModuleInputPaths::CancelScalarBindingContinuation(Job,Reason);
    if(Kind==EJobKind::CollectionScriptRemap)return NiagaraParameterCollections::CancelScriptRemapContinuation(Job,Reason);
    if(Kind==EJobKind::RendererCleanup)return NiagaraRenderers::CancelCleanupContinuation(Job,Reason);
    return false;
}
bool CanStartConcrete()
{if(!Safe()||IsStopping()||Starting||NiagaraParameterCollections::HasScriptRemapContinuationLease()||NiagaraRenderers::HasCleanupContinuationLease())return false;for(const auto& Pair:Records)if(!Pair.Value.Terminal)return false;return true;}
UCubelessNiagaraScratchUserRecipeJob* StartCompileStatus(const FString& Params)
{if(!CanStartConcrete())return nullptr;TGuardValue<bool> Lease(Starting,true);return NiagaraAuthoringInspection::StartCompileStatusContinuation(Params);}
UCubelessNiagaraScratchUserRecipeJob* StartScalarBinding(const FString& Params)
{if(!CanStartConcrete())return nullptr;TGuardValue<bool> Lease(Starting,true);return NiagaraModuleInputPaths::StartScalarBindingContinuation(Params);}
UCubelessNiagaraScratchUserRecipeJob* StartArrayBinding(const FString& Params)
{if(!CanStartConcrete())return nullptr;TGuardValue<bool> Lease(Starting,true);return NiagaraModuleInputPaths::StartArrayBindingContinuation(Params);}
UCubelessNiagaraScratchUserRecipeJob* StartRemoveUnusedPath(const FString& Params)
{if(!CanStartConcrete())return nullptr;TGuardValue<bool> Lease(Starting,true);return NiagaraModuleInputPaths::StartRemoveUnusedPathContinuation(Params);}
UCubelessNiagaraScratchUserRecipeJob* StartCollectionScriptRemap(const FString& Params)
{if(!CanStartConcrete())return nullptr;TGuardValue<bool> Lease(Starting,true);return NiagaraParameterCollections::StartScriptRemapContinuation(Params);}
UCubelessNiagaraScratchUserRecipeJob* StartRendererCleanup(const FString& Params)
{if(!CanStartConcrete())return nullptr;TGuardValue<bool> Lease(Starting,true);return NiagaraRenderers::StartCleanupContinuation(Params);}
void Shutdown(const FString& Reason)
{
    Stopping=true;if(!IsInGameThread())return;if(PreExitHandle.IsValid()){FEditorDelegates::OnEditorPreExit.Remove(PreExitHandle);PreExitHandle.Reset();}
    NiagaraAuthoringInspection::ShutdownCompileStatusContinuation(Reason);NiagaraModuleInputPaths::ShutdownScalarBindingContinuation(Reason);NiagaraParameterCollections::ShutdownScriptRemapContinuation(Reason);NiagaraRenderers::ShutdownCleanupContinuation(Reason);
}
}
