// HZ: five concrete existing Editor Material continuations; HY publication semantics retained.
#include "Compatibility/UE58/CubelessMaterialPostprocessAsyncApi.h"
#include "CubelessMaterialPostprocessJob.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Misc/CoreDelegates.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include <atomic>

namespace Cubeless::UE58::MaterialCompilation
{
UCubelessMaterialPostprocessJob* StartCompileAndSaveContinuation(const FString&);
bool CancelCompileAndSaveContinuation(UCubelessMaterialPostprocessJob*,const FString&);
void ShutdownCompileAndSaveContinuation(const FString&);
bool HasCompileAndSaveContinuationLease();
}
namespace Cubeless::UE58::MaterialCache
{
UCubelessMaterialPostprocessJob* StartCacheRefreshContinuation(const FString&);
bool CancelCacheRefreshContinuation(UCubelessMaterialPostprocessJob*,const FString&);
void ShutdownCacheRefreshContinuation(const FString&);
bool HasCacheRefreshContinuationLease();
}
namespace Cubeless::UE58::MaterialReference
{
UCubelessMaterialPostprocessJob* StartReplaceCollectionsContinuation(const FString&);
UCubelessMaterialPostprocessJob* StartReplaceTexturesContinuation(const FString&);
bool CancelReplaceCollectionsContinuation(UCubelessMaterialPostprocessJob*,const FString&);
bool CancelReplaceTexturesContinuation(UCubelessMaterialPostprocessJob*,const FString&);
void ShutdownReplaceCollectionsContinuation(const FString&);
void ShutdownReplaceTexturesContinuation(const FString&);
bool HasReplaceCollectionsContinuationLease();
bool HasReplaceTexturesContinuationLease();
}
namespace Cubeless::UE58::MaterialCollectionParameters
{
UCubelessMaterialPostprocessJob* StartReplaceCollectionParametersContinuation(const FString&);
bool CancelReplaceCollectionParametersContinuation(UCubelessMaterialPostprocessJob*,const FString&);
void ShutdownReplaceCollectionParametersContinuation(const FString&);
bool HasReplaceCollectionParametersContinuationLease();
}
namespace Cubeless::UE58::MaterialPostprocessAsync
{
namespace
{
std::atomic<bool> Stopping{false};
bool Starting=false;
FDelegateHandle PreExit;
bool Safe()
{
    return IsInGameThread() && GEditor && !IsEngineExitRequested() && !GExitPurge
        && !IsGarbageCollecting() && !UE::IsSavingPackage();
}
struct FRecord
{
    EJobKind Kind=EJobKind::CompileAndSave;
    FGuid Id;
    TWeakObjectPtr<UCubelessMaterialPostprocessJob> Object;
    FString Snapshot;
    bool Registered=false;
    bool Terminal=false;
};
// A key is an opaque identity only. In particular, pre-registration unsafe
// publication never constructs/queries a weak UObject handle or dereferences it.
TMap<const UCubelessMaterialPostprocessJob*,FRecord> Records;
bool CanStart()
{
    if (!Safe() || Starting || Stopping.load()) return false;
    // Child leases precede RegisterJob; checking just Records misses interrupted
    // NewObject callbacks (including the opposite concrete operation).
    if (MaterialCompilation::HasCompileAndSaveContinuationLease()
        || MaterialCache::HasCacheRefreshContinuationLease()
        || MaterialReference::HasReplaceCollectionsContinuationLease()
        || MaterialReference::HasReplaceTexturesContinuationLease()
        || MaterialCollectionParameters::HasReplaceCollectionParametersContinuationLease()) return false;
    for (auto It=Records.CreateIterator();It;++It)
        if (It.Value().Terminal && !It.Value().Object.IsValid()) It.RemoveCurrent();
    if (Records.Num()>=256) return false;
    for (const auto& Pair:Records) if (!Pair.Value.Terminal) return false;
    return true;
}
}
struct FJobAccess
{
    static void Set(UCubelessMaterialPostprocessJob* Job,const FString& Snapshot,bool Terminal)
    {
        Job->SnapshotJson=Snapshot;
        Job->bIsTerminal=Terminal;
    }
};
bool IsStopping() { return Stopping.load(); }
void RegisterJob(UCubelessMaterialPostprocessJob* Job,EJobKind Kind,const FGuid& Id)
{
    if (!Safe() || !IsValid(Job) || !Id.IsValid()
        || Job->GetClass()!=UCubelessMaterialPostprocessJob::StaticClass()) return;
    FRecord& Record=Records.FindOrAdd(Job);
    if (Record.Registered)
    {
        // No replacement of a retained generation or previously sealed receipt.
        if (Record.Id!=Id || Record.Kind!=Kind || Record.Object.Get()!=Job) return;
    }
    else
    {
        Record.Kind=Kind;
        Record.Id=Id;
        Record.Object=Job;
        Record.Registered=true;
    }
    if (!PreExit.IsValid())
        PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([]
        { Shutdown(TEXT("Engine pre-exit stopped Material continuations without compiler cancellation or replay.")); });
}
void Publish(UCubelessMaterialPostprocessJob* Job,const FString& Snapshot,bool Terminal,bool ObjectSafe)
{
    if (!IsInGameThread()) return;
    FRecord& Record=Records.FindOrAdd(Job);
    if (Record.Terminal) return;
    Record.Snapshot=Snapshot;
    if (!ObjectSafe || !Safe() || !Record.Registered || Record.Object.Get()!=Job
        || !IsValid(Job) || Job->GetClass()!=UCubelessMaterialPostprocessJob::StaticClass()) return;
    FJobAccess::Set(Job,Snapshot,Terminal);
    Record.Terminal=Terminal;
}
FString SnapshotJson(const UCubelessMaterialPostprocessJob* Job)
{
    if (!IsInGameThread()) return FString();
    const FRecord* Record=Records.Find(Job);
    return Record?Record->Snapshot:FString();
}
bool Cancel(UCubelessMaterialPostprocessJob* Job,const FString& Reason)
{
    if (!IsInGameThread()) return false;
    const FRecord* Record=Records.Find(Job);
    if (!Record || !Record->Registered || Record->Terminal) return false;
    switch(Record->Kind)
    {
    case EJobKind::CompileAndSave:return MaterialCompilation::CancelCompileAndSaveContinuation(Job,Reason);
    case EJobKind::CacheRefresh:return MaterialCache::CancelCacheRefreshContinuation(Job,Reason);
    case EJobKind::ReplaceCollections:return MaterialReference::CancelReplaceCollectionsContinuation(Job,Reason);
    case EJobKind::ReplaceTextures:return MaterialReference::CancelReplaceTexturesContinuation(Job,Reason);
    case EJobKind::ReplaceCollectionParameters:return MaterialCollectionParameters::CancelReplaceCollectionParametersContinuation(Job,Reason);
    default:return false;
    }
}
UCubelessMaterialPostprocessJob* StartCompileAndSave(const FString& Params)
{
    if (!CanStart()) return nullptr;
    TGuardValue<bool> Guard(Starting,true);
    return MaterialCompilation::StartCompileAndSaveContinuation(Params);
}
UCubelessMaterialPostprocessJob* StartCacheRefresh(const FString& Params)
{
    if (!CanStart()) return nullptr;
    TGuardValue<bool> Guard(Starting,true);
    return MaterialCache::StartCacheRefreshContinuation(Params);
}
UCubelessMaterialPostprocessJob* StartReplaceCollections(const FString& Params)
{
    if(!CanStart())return nullptr;TGuardValue<bool> Guard(Starting,true);
    return MaterialReference::StartReplaceCollectionsContinuation(Params);
}
UCubelessMaterialPostprocessJob* StartReplaceTextures(const FString& Params)
{
    if(!CanStart())return nullptr;TGuardValue<bool> Guard(Starting,true);
    return MaterialReference::StartReplaceTexturesContinuation(Params);
}
UCubelessMaterialPostprocessJob* StartReplaceCollectionParameters(const FString& Params)
{
    if(!CanStart())return nullptr;TGuardValue<bool> Guard(Starting,true);
    return MaterialCollectionParameters::StartReplaceCollectionParametersContinuation(Params);
}
void Shutdown(const FString& Reason)
{
    Stopping.store(true);
    if (!IsInGameThread()) return;
    if (PreExit.IsValid()) { FCoreDelegates::OnEnginePreExit.Remove(PreExit);PreExit.Reset(); }
    MaterialCompilation::ShutdownCompileAndSaveContinuation(Reason);
    MaterialCache::ShutdownCacheRefreshContinuation(Reason);
    MaterialReference::ShutdownReplaceCollectionsContinuation(Reason);
    MaterialReference::ShutdownReplaceTexturesContinuation(Reason);
    MaterialCollectionParameters::ShutdownReplaceCollectionParametersContinuation(Reason);
}
}
