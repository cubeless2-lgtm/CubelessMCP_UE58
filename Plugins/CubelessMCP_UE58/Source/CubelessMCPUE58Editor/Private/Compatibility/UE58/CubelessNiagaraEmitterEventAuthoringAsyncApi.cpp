// HV: exactly two existing Editor leaf controllers, no transport or generic worker.
#include "Compatibility/UE58/CubelessNiagaraEmitterEventAuthoringAsyncApi.h"
#include "CubelessNiagaraEmitterEventAuthoringJob.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Misc/CoreDelegates.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include <atomic>
namespace Cubeless::UE58::NiagaraEmitterConfiguration
{bool HasPersistentIdsContinuationLease();UCubelessNiagaraEmitterEventAuthoringJob* StartPersistentIdsContinuation(const FString&);bool CancelPersistentIdsContinuation(UCubelessNiagaraEmitterEventAuthoringJob*,const FString&);void ShutdownPersistentIdsContinuation(const FString&);}
namespace Cubeless::UE58::NiagaraEventStage
{bool HasEventHandlerContinuationLease();UCubelessNiagaraEmitterEventAuthoringJob* StartEventHandlerContinuation(const FString&);bool CancelEventHandlerContinuation(UCubelessNiagaraEmitterEventAuthoringJob*,const FString&);void ShutdownEventHandlerContinuation(const FString&);}
namespace Cubeless::UE58::NiagaraSimulationStage
{bool HasSimulationStageContinuationLease();UCubelessNiagaraEmitterEventAuthoringJob* StartSimulationStageAddContinuation(const FString&);UCubelessNiagaraEmitterEventAuthoringJob* StartSimulationStageSettingsContinuation(const FString&);bool CancelSimulationStageContinuation(UCubelessNiagaraEmitterEventAuthoringJob*,const FString&);void ShutdownSimulationStageContinuation(const FString&);}
namespace Cubeless::UE58::NiagaraEmitterEventAuthoringAsync
{
namespace
{
std::atomic<bool> Stopping{false};bool Starting=false;FDelegateHandle PreExit;
bool Safe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage;}
struct FRecord{EJobKind Kind=EJobKind::PersistentIds;FGuid Id;TWeakObjectPtr<UCubelessNiagaraEmitterEventAuthoringJob> Object;FString Snapshot;bool Terminal=false,Registered=false;};
TMap<const UCubelessNiagaraEmitterEventAuthoringJob*,FRecord> Records;
bool CanStart()
{
 if(!Safe()||Starting||Stopping.load())return false;
 if(NiagaraEmitterConfiguration::HasPersistentIdsContinuationLease()||NiagaraEventStage::HasEventHandlerContinuationLease()||NiagaraSimulationStage::HasSimulationStageContinuationLease())return false;
 for(auto It=Records.CreateIterator();It;++It)if(It.Value().Terminal&&!It.Value().Object.IsValid())It.RemoveCurrent();
 if(Records.Num()>=256)return false;
 for(const auto& Pair:Records)if(!Pair.Value.Terminal)return false;
 return true;
}
}
struct FJobAccess
{
 static void Set(UCubelessNiagaraEmitterEventAuthoringJob* Job,const FString& Snapshot,bool Terminal)
 {Job->SnapshotJson=Snapshot;Job->bIsTerminal=Terminal;}
};
bool IsStopping(){return Stopping.load();}
void RegisterJob(UCubelessNiagaraEmitterEventAuthoringJob* Job,EJobKind Kind,const FGuid& Id)
{
 if(!Safe()||!IsValid(Job)||Job->GetClass()!=UCubelessNiagaraEmitterEventAuthoringJob::StaticClass())return;
 FRecord Record;Record.Kind=Kind;Record.Id=Id;Record.Object=Job;Record.Registered=true;Records.Add(Job,MoveTemp(Record));
 if(!PreExit.IsValid())PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([]{Shutdown(TEXT("Engine pre-exit stopped the two concrete HV continuations without replay."));});
}
void Publish(UCubelessNiagaraEmitterEventAuthoringJob* Job,const FString& Snapshot,bool Terminal,bool ObjectSafe)
{
 if(!IsInGameThread())return;FRecord* Record=Records.Find(Job);
 if(!Record){FRecord Plain;Plain.Snapshot=Snapshot;Records.Add(Job,MoveTemp(Plain));return;}
 if(Record->Terminal)return;
 Record->Snapshot=Snapshot;
 if(!Record->Registered||!ObjectSafe||!Safe()||Record->Object.Get()!=Job||!IsValid(Job)||Job->GetClass()!=UCubelessNiagaraEmitterEventAuthoringJob::StaticClass())return;
 FJobAccess::Set(Job,Snapshot,Terminal);Record->Terminal=Terminal;
}
FString SnapshotJson(const UCubelessNiagaraEmitterEventAuthoringJob* Job)
{if(!IsInGameThread())return FString();const FRecord* Record=Records.Find(Job);return Record?Record->Snapshot:FString();}
bool Cancel(UCubelessNiagaraEmitterEventAuthoringJob* Job,const FString& Reason)
{
 if(!IsInGameThread())return false;const FRecord* Record=Records.Find(Job);if(!Record||!Record->Registered||Record->Terminal)return false;
 if(Record->Kind==EJobKind::PersistentIds)return NiagaraEmitterConfiguration::CancelPersistentIdsContinuation(Job,Reason);
 if(Record->Kind==EJobKind::EventHandler)return NiagaraEventStage::CancelEventHandlerContinuation(Job,Reason);
 if(Record->Kind==EJobKind::SimulationStageAdd||Record->Kind==EJobKind::SimulationStageSettings)return NiagaraSimulationStage::CancelSimulationStageContinuation(Job,Reason);
 return false;
}
UCubelessNiagaraEmitterEventAuthoringJob* StartPersistentIds(const FString& Params)
{if(!CanStart())return nullptr;TGuardValue<bool> Guard(Starting,true);return NiagaraEmitterConfiguration::StartPersistentIdsContinuation(Params);}
UCubelessNiagaraEmitterEventAuthoringJob* StartEventHandler(const FString& Params)
{if(!CanStart())return nullptr;TGuardValue<bool> Guard(Starting,true);return NiagaraEventStage::StartEventHandlerContinuation(Params);}
UCubelessNiagaraEmitterEventAuthoringJob* StartSimulationStageAdd(const FString& Params)
{if(!CanStart())return nullptr;TGuardValue<bool> Guard(Starting,true);return NiagaraSimulationStage::StartSimulationStageAddContinuation(Params);}
UCubelessNiagaraEmitterEventAuthoringJob* StartSimulationStageSettings(const FString& Params)
{if(!CanStart())return nullptr;TGuardValue<bool> Guard(Starting,true);return NiagaraSimulationStage::StartSimulationStageSettingsContinuation(Params);}
void Shutdown(const FString& Reason)
{
 Stopping.store(true);if(!IsInGameThread())return;
 if(PreExit.IsValid()){FCoreDelegates::OnEnginePreExit.Remove(PreExit);PreExit.Reset();}
 NiagaraEmitterConfiguration::ShutdownPersistentIdsContinuation(Reason);
 NiagaraEventStage::ShutdownEventHandlerContinuation(Reason);
 NiagaraSimulationStage::ShutdownSimulationStageContinuation(Reason);
}
}
