#include "Compatibility/UE58/CubelessNiagaraParticlePositionsApi.h"
#include "Editor.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataSet.h"
#include "NiagaraDataSetAccessor.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/GarbageCollection.h"

namespace Cubeless::UE58::NiagaraParticlePositions
{
FString Inspect(const FString& ComponentPath,int32 EmitterIndex,int32 MaxParticles)
{
 auto R=MakeShared<FJsonObject>();
 R->SetStringField(TEXT("operation"),TEXT("inspect_niagara_particle_positions"));
 R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("complete"),false);
 R->SetStringField(TEXT("stage"),TEXT("preflight"));R->SetBoolField(TEXT("mutation_attempted"),false);
 R->SetBoolField(TEXT("changed"),false);R->SetBoolField(TEXT("save_attempted"),false);
 R->SetBoolField(TEXT("automatic_retry"),false);R->SetBoolField(TEXT("gpu_readback_requested"),false);
 R->SetBoolField(TEXT("simulation_advanced"),false);R->SetBoolField(TEXT("simulation_pause_changed"),false);R->SetBoolField(TEXT("simulation_mode_changed"),false);
 auto Encode=[&](){FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));return Text;};
 auto Fail=[&](const TCHAR* Code,const TCHAR* Error){R->SetStringField(TEXT("error_code"),Code);R->SetStringField(TEXT("error"),Error);return Encode();};
 if(!IsInGameThread()||!GEditor||IsGarbageCollecting()||IsEngineExitRequested()||UE::IsSavingPackage())
  return Fail(TEXT("context_unavailable"),TEXT("Live Editor game thread without GC/save/exit required"));
 if(ComponentPath.IsEmpty()||ComponentPath.Len()>4096||EmitterIndex<INDEX_NONE||MaxParticles<1||MaxParticles>65536)
  return Fail(TEXT("invalid_input"),TEXT("Exact loaded component path, emitter_index>=-1 and max_particles1..65536 required"));
 auto* Component=FindObject<UNiagaraComponent>(nullptr,*ComponentPath);
 if(!IsValid(Component)||Component->GetPathName()!=ComponentPath||!Component->IsRegistered())
  return Fail(TEXT("component_unavailable"),TEXT("Exact live registered NiagaraComponent required; no object is loaded or spawned"));
 auto* World=Component->GetWorld();
 auto* EditorWorld=GEditor->GetEditorWorldContext().World();
 auto* PIEContext=GEditor->GetPIEWorldContext();auto* PIEWorld=PIEContext?PIEContext->World():nullptr;
 if(!IsValid(World)||World->bIsTearingDown||(World!=EditorWorld&&World!=PIEWorld))
  return Fail(TEXT("foreign_world"),TEXT("Component must belong to the current Editor or PIE world"));
 TStrongObjectPtr<UNiagaraComponent> HoldComponent(Component);TStrongObjectPtr<UWorld> HoldWorld(World);
 auto* System=Component->GetAsset();if(!IsValid(System))return Fail(TEXT("system_unavailable"),TEXT("Component has no live system"));
 TStrongObjectPtr<UNiagaraSystem> HoldSystem(System);
 R->SetStringField(TEXT("component_path"),ComponentPath);R->SetStringField(TEXT("world"),World->GetPathName());R->SetStringField(TEXT("system_path"),System->GetPathName());
 auto Controller=Component->GetSystemInstanceController();
 if(!Controller.IsValid()||!Controller->IsValid())return Fail(TEXT("instance_unavailable"),TEXT("No live system instance; this is not evidence of an empty particle result"));
 if(!Controller->IsSolo())return Fail(TEXT("solo_snapshot_required"),TEXT("CPU snapshot requires an already-solo system. Unsafe concurrent instance access and automatic force-solo are not performed"));
 // Solo systems may still have asynchronous component-tick work. UE5.8 SetPaused
 // waits/finalizes and PauseInstance removes it from Running before reporting paused.
 // This inspector never performs that transition or waits on the caller's behalf.
 if(!Controller->IsPaused())return Fail(TEXT("paused_snapshot_required"),TEXT("An already-paused solo system is required; this inspector does not pause, wait, finalize or advance simulation"));
 auto* Instance=Controller->GetSoloSystemInstance();
 if(!Instance||Instance->GetSystem()!=System)return Fail(TEXT("instance_identity_changed"),TEXT("Solo instance or system changed"));
 if(Instance->HasPendingFinalize())return Fail(TEXT("pending_finalize"),TEXT("Solo instance has pending finalize work; no particle buffer read and no implicit wait"));
 const int32 TickCount=Instance->GetTickCount();
 R->SetNumberField(TEXT("snapshot_tick_count"),TickCount);R->SetNumberField(TEXT("snapshot_age_seconds"),Instance->GetAge());
 R->SetStringField(TEXT("freshness"),TEXT("paused_last_completed_solo_tick_not_advanced_or_waited"));
 if(TickCount<=0)return Fail(TEXT("simulation_not_ticked"),TEXT("No completed simulation tick; no empty-result claim"));
 const auto Emitters=Instance->GetEmitters();
 if(Emitters.Num()>1024||EmitterIndex>=Emitters.Num())return Fail(TEXT("emitter_selector_invalid"),TEXT("Emitter index out of range or emitter count exceeds1024"));
 TArray<TSharedPtr<FJsonValue>> Rows;bool Complete=true;int32 Returned=0;
 const FTransform Transform=Instance->GetWorldTransform();const auto Converter=Instance->GetLWCConverter(false);
 for(int32 Index=0;Index<Emitters.Num();++Index)
 {
  if(EmitterIndex!=INDEX_NONE&&EmitterIndex!=Index)continue;
  auto* Emitter=&Emitters[Index].Get();auto Row=MakeShared<FJsonObject>();
  Row->SetNumberField(TEXT("emitter_index"),Index);Row->SetStringField(TEXT("emitter_name"),Emitter->GetEmitterHandle().GetName().ToString());
  Row->SetBoolField(TEXT("local_space"),Emitter->IsLocalSpace());Row->SetStringField(TEXT("position_space"),TEXT("world_LWC_converted"));
  Row->SetBoolField(TEXT("positions_observed"),false);Rows.Add(MakeShared<FJsonValueObject>(Row));R->SetArrayField(TEXT("emitters"),Rows);
  if(Emitter->GetSimTarget()!=ENiagaraSimTarget::CPUSim)
  {Row->SetStringField(TEXT("status"),TEXT("gpu_readback_not_requested"));Complete=false;continue;}
  if(!Emitter->AsStateful())
  {Row->SetStringField(TEXT("status"),TEXT("stateless_snapshot_not_implemented"));Complete=false;continue;}
  const auto& Data=Emitter->GetParticleData();const auto* Buffer=Data.GetCurrentData();
  if(!Buffer){Row->SetStringField(TEXT("status"),TEXT("cpu_buffer_unavailable"));Complete=false;continue;}
  const int32 Num=Buffer->GetNumInstances();Row->SetNumberField(TEXT("particle_count"),Num);
  FNiagaraDataSetAccessor<FNiagaraPosition> Accessor(Data,FName(TEXT("Position")));
  // Attribute presence is a compiled-layout property, independent of live count.
  if(!Accessor.IsValid()){Row->SetStringField(TEXT("status"),TEXT("position_attribute_unavailable"));Complete=false;continue;}
  if(Num==0)
  {Row->SetStringField(TEXT("status"),TEXT("observed_empty_cpu_buffer"));Row->SetBoolField(TEXT("positions_observed"),true);Row->SetArrayField(TEXT("positions"),TArray<TSharedPtr<FJsonValue>>{});continue;}
  auto Reader=Accessor.GetReader(Data);
  if(!Reader.IsValid()){Row->SetStringField(TEXT("status"),TEXT("position_attribute_unavailable"));Complete=false;continue;}
  TArray<TSharedPtr<FJsonValue>> Positions;
  const int32 Count=FMath::Min(Num,MaxParticles-Returned);
  for(int32 I=0;I<Count;++I)
  {
   const FVector SimulationPosition=Converter.ConvertSimulationPositionToWorld(Reader.Get(I));
   const FVector P=Emitter->IsLocalSpace()?Transform.TransformPosition(SimulationPosition):SimulationPosition;
   if(P.ContainsNaN())return Fail(TEXT("nonfinite_position"),TEXT("Observed nonfinite particle position; partial rows retained"));
   Positions.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}));
  }
  Returned+=Count;Row->SetArrayField(TEXT("positions"),Positions);Row->SetBoolField(TEXT("positions_observed"),true);
  Row->SetBoolField(TEXT("truncated"),Count<Num);Row->SetNumberField(TEXT("returned_count"),Count);Row->SetStringField(TEXT("status"),Count<Num?TEXT("cpu_snapshot_truncated"):TEXT("cpu_snapshot"));
  if(Count<Num)Complete=false;
 }
 if(!IsValid(Component)||Component->GetWorld()!=World||Component->GetAsset()!=System||Component->GetSystemInstanceController()!=Controller||!Controller->IsValid()||!Controller->IsSolo()||!Controller->IsPaused()||Controller->GetSoloSystemInstance()!=Instance||Instance->HasPendingFinalize()||Instance->GetTickCount()!=TickCount)
  return Fail(TEXT("snapshot_identity_changed"),TEXT("Component/controller/instance/tick changed; partial result is not complete"));
 R->SetNumberField(TEXT("returned_particle_count"),Returned);R->SetBoolField(TEXT("complete"),Complete);R->SetBoolField(TEXT("success"),Complete);R->SetStringField(TEXT("stage"),TEXT("observed"));
 if(!Complete){R->SetStringField(TEXT("error_code"),TEXT("snapshot_incomplete"));R->SetStringField(TEXT("error"),TEXT("Some selected data is unavailable or truncated; inspect per-emitter status, not an empty-success result"));}
 return Encode();
}
}
