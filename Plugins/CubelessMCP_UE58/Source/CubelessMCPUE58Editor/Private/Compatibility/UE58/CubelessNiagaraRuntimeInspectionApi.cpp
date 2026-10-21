// GO dedicated Editor-only live Niagara/runtime inspection. Source candidate; no Engine execution evidence.
#include "Compatibility/UE58/CubelessNiagaraRuntimeInspectionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemSimulation.h"
#include "NiagaraConstants.h"
#include "NiagaraParameterStore.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraDataInterfaceGrid2DCollection.h"
#include "NiagaraTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UnrealClient.h"
#include "RHI.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/GarbageCollection.h"
namespace Cubeless::UE58::NiagaraRuntimeInspection
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
UWorld* SelectedWorld(){if(!GEditor)return nullptr;auto* P=GEditor->GetPIEWorldContext();UWorld* W=P?P->World():nullptr;return W?W:GEditor->GetEditorWorldContext().World();}
struct FIdentity{TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path;explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()){}bool Exact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}};
struct FComponentIdentity{TWeakObjectPtr<UNiagaraComponent> Component;TWeakObjectPtr<AActor> Owner;TWeakObjectPtr<UNiagaraSystem> Asset;FNiagaraSystemInstanceControllerPtr Controller;bool ControllerValid=false;FNiagaraSystemInstanceID Id;FNiagaraSystemInstance* PausedSolo=nullptr;FNiagaraSystemSimulationPtr SoloSimulation;FNiagaraSystemInstance* PausedShared=nullptr;TWeakPtr<FNiagaraSystemSimulation,ESPMode::ThreadSafe> SharedSimulation;FNiagaraSystemSimulation* SharedSimulationIdentity=nullptr;};
// UJ holds non-UObject runtime refs until before the held controller is released.
struct FEmitterRuntimeRole
{
 FNiagaraEmitterInstance* Pointer=nullptr;FNiagaraEmitterInstancePtr Instance;bool Stateful=false;int32 HandleIndex=INDEX_NONE;FGuid Id;FName Name;bool Enabled=false;ENiagaraEmitterMode Mode=ENiagaraEmitterMode::Standard;FVersionedNiagaraEmitter Version,HandleVersion;
};
struct FEmitterRuntimeInventory
{
 TWeakObjectPtr<UNiagaraComponent> Component;FNiagaraSystemInstance* Solo=nullptr;bool SharedPaused=false;TWeakObjectPtr<UNiagaraSystem> Asset;int32 HandleCount=0;TArray<FEmitterRuntimeRole> Rows;
};
struct FWork
{
 FResult R;FString FailedStage;UEditorEngine* SoloEditor=nullptr;TWeakObjectPtr<UWorld> World;bool WorldCaptured=false;TArray<TUniquePtr<FIdentity>> Objects;TArray<FComponentIdentity> Components;TArray<TUniquePtr<FEmitterRuntimeInventory>> EmitterInventories;TWeakObjectPtr<UTextureRenderTarget2D> RT;FTextureRenderTargetResource* Resource=nullptr;int32 SizeX=0,SizeY=0;EPixelFormat Format=PF_Unknown;int64 Slots=0,Text=0;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Prefix;
 FWork(){R.Compiled=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);}
 bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
 TSharedPtr<FJsonObject> Error(const FString& S){if(R.Error.IsEmpty())Fail(TEXT("niagara_runtime_inspection_failed"),S);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),S);return O;}
 bool Budget(int64 N,int64 Chars=0){Slots+=N;Text+=Chars;return (Slots<=262144&&Text<=32ll*1024*1024)||Fail(TEXT("niagara_runtime_output_budget_exceeded"),TEXT("Bounded output/capture exceeded; prefix retained and no retry."));}
 // Only a currently owned, already-paused non-Solo instance is read. Its
 // simulation may tick other Running instances, never this paused instance.
 bool SharedPausedCurrent(const FComponentIdentity& C,UNiagaraComponent* O)
 {
  if(!C.Controller||!C.ControllerValid||!C.Controller->IsValid()||C.Controller->GetSystemInstanceID()!=C.Id
      ||C.Controller->IsSolo()||!C.Controller->IsPaused())return Fail(TEXT("niagara_runtime_shared_paused_owner_changed"),TEXT("Current controller/ID/no-Solo/already-paused admission changed; no Unsafe getter or retained instance access follows."));
  FNiagaraSystemInstance* S=C.Controller->GetSystemInstance_Unsafe();
  if(S!=C.PausedShared)return Fail(TEXT("niagara_runtime_shared_instance_changed"),TEXT("Current controller no longer owns the captured paused shared instance; no old instance getter follows."));
  if(!S||!S->IsPaused()||S->IsSolo()||S->HasPendingFinalize()||S->GetId()!=C.Id)return Fail(TEXT("niagara_runtime_shared_phase_changed"),TEXT("Captured shared instance is no longer already paused/finalized; no wait, finalize or retry."));
  // This fresh local ref is released while S still owns the same simulation.
  // Holding a shared simulation across a callback could postpone its destructor
  // (Destroy waits/deactivates), so the retained expectation is weak only.
  const FNiagaraSystemSimulationPtr Simulation=S->GetSystemSimulation();
  if(!Simulation||!C.SharedSimulation.IsValid()||Simulation.Get()!=C.SharedSimulationIdentity||!Simulation->IsValid()||Simulation->GetIsSolo()
      ||Simulation->GetSystem()!=C.Asset.Get()||Simulation->GetWorld()!=World.Get()||S->GetSystem()!=C.Asset.Get()||S->GetWorld()!=World.Get()||S->GetAttachComponent()!=O)
   return Fail(TEXT("niagara_runtime_shared_simulation_changed"),TEXT("Current paused shared simulation/System/world/component roles changed; no other instance inventory is read."));
  return true;
 }
 bool Admit(const FString& Stage){R.Stage=Stage;if(!R.Error.IsEmpty())return false;if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||GIsSavingPackage)return Fail(TEXT("niagara_runtime_context_unavailable"),TEXT("Live Editor game thread outside GC/exit/save is required."));if(SoloEditor&&GEditor!=SoloEditor)return Fail(TEXT("niagara_runtime_editor_changed"),TEXT("Held Solo observer Editor changed across observation."));if(WorldCaptured&&(!Live(World.Get())||World->bIsTearingDown||SelectedWorld()!=World.Get()))return Fail(TEXT("niagara_runtime_world_changed"),TEXT("Selected PIE-first/editor-fallback world changed or is tearing down."));for(const auto& I:Objects)if(!I->Exact())return Fail(TEXT("niagara_runtime_identity_changed"),TEXT("Captured object class/outer/package/path changed."));if(RT.IsValid()&&(RT->SizeX!=SizeX||RT->SizeY!=SizeY||RT->GetFormat()!=Format||RT->GameThread_GetRenderTargetResource()!=Resource))return Fail(TEXT("render_target_resource_changed"),TEXT("Captured RT resource/dimensions/format changed across SDK call."));for(const auto& C:Components){auto* O=C.Component.Get();if(!Live(O)||O->GetOwner()!=C.Owner.Get()||!C.Owner->GetComponents().Contains(O)||O->GetWorld()!=World.Get()||O->GetAsset()!=C.Asset.Get()||O->GetSystemInstanceController()!=C.Controller)return Fail(TEXT("niagara_runtime_component_rebound"),TEXT("Owner/world/System/controller changed across SDK call."));if(C.Controller){if(C.Controller->IsValid()!=C.ControllerValid)return Fail(TEXT("niagara_runtime_instance_changed"),TEXT("System instance availability changed."));if(C.ControllerValid&&(C.Controller->GetSystemInstanceID()!=C.Id))return Fail(TEXT("niagara_runtime_instance_changed"),TEXT("Captured solo instance identity changed; no stale instance access."));if(C.PausedSolo){if(!C.Controller->IsSolo()||C.Controller->GetSoloSystemInstance()!=C.PausedSolo)return Fail(TEXT("niagara_runtime_solo_instance_changed"),TEXT("Held controller no longer owns the captured Solo instance; no old instance getter follows."));auto* S=C.Controller->GetSoloSystemInstance();if(!S->IsPaused()||S->HasPendingFinalize()||S->GetSystemSimulation()!=C.SoloSimulation||!C.SoloSimulation||!C.SoloSimulation->IsValid()||!C.SoloSimulation->GetIsSolo()||C.SoloSimulation->GetSystem()!=C.Asset.Get()||C.SoloSimulation->GetWorld()!=World.Get()||S->GetSystem()!=C.Asset.Get()||S->GetWorld()!=World.Get()||S->GetAttachComponent()!=O)return Fail(TEXT("niagara_runtime_solo_phase_changed"),TEXT("Already-paused Solo instance/simulation/asset/world/attachment changed; no wait, finalize or retry."));}if(C.PausedShared&&!SharedPausedCurrent(C,O))return false;}}return EmitterCurrent();}
 bool Pin(UObject* O){if(!Admit(TEXT("pin_object"))||!Live(O))return Fail(TEXT("niagara_runtime_object_unavailable"),TEXT("Expected loaded live object."));for(const auto& I:Objects)if(I->Object.Get()==O)return I->Exact();if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
 bool SDK(const TCHAR* Stage){if(!Admit(Stage)||!Budget(1,FCString::Strlen(Stage)))return false;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);return true;}
 bool After(const TCHAR* Stage){return Admit(FString(TEXT("after_"))+Stage);}
 template<typename F>bool Read(const TCHAR* Stage,F&& Fn){if(!SDK(Stage))return false;Fn();return After(Stage);}
 bool CaptureWorld(){if(!Admit(TEXT("select_world")))return false;auto* W=SelectedWorld();if(!Live(W)||W->bIsTearingDown)return Fail(TEXT("niagara_runtime_world_unavailable"),TEXT("No live PIE-first/editor-fallback world."));World=W;WorldCaptured=true;return Pin(W);}
 bool Component(UNiagaraComponent* O){if(!Pin(O)||!Live(O->GetOwner())||!Pin(O->GetOwner())||O->GetWorld()!=World.Get())return false;for(const auto& C:Components)if(C.Component.Get()==O)return true;FComponentIdentity C;C.Component=O;C.Owner=O->GetOwner();C.Asset=O->GetAsset();if(C.Asset.IsValid()&&!Pin(C.Asset.Get()))return false;C.Controller=O->GetSystemInstanceController();C.ControllerValid=C.Controller&&C.Controller->IsValid();if(C.ControllerValid)C.Id=C.Controller->GetSystemInstanceID();Components.Add(MoveTemp(C));return Admit(TEXT("component_captured"));}

 // Admit has already closed controller/instance/paused-Solo ownership before this
 // nonrecursive lookup. Never dereference a retained instance after losing that owner.
 bool EmitterCurrent()
 {
  for(const auto& Inventory:EmitterInventories)
  {
   const auto& E=*Inventory;
   if(!Components.ContainsByPredicate([&](const FComponentIdentity& C){return C.Component==E.Component&&(E.SharedPaused?C.PausedShared==E.Solo:C.PausedSolo==E.Solo)&&C.Asset==E.Asset;}))return Fail(TEXT("niagara_runtime_emitter_inventory_owner_changed"),TEXT("Captured emitter inventory lost its selected paused instance/controller ownership."));
   const auto Fresh=E.Solo->GetEmitters();const auto& Handles=E.Asset->GetEmitterHandles();
   if(Fresh.Num()!=E.Rows.Num()||Handles.Num()!=E.HandleCount)return Fail(TEXT("niagara_runtime_emitter_inventory_changed"),TEXT("Captured emitter or System handle count changed; no old emitter getter follows."));
   for(int32 I=0;I<E.Rows.Num();++I)
   {
    const auto& Row=E.Rows[I];
    if(&Fresh[I].Get()!=Row.Pointer||!Handles.IsValidIndex(Row.HandleIndex))return Fail(TEXT("niagara_runtime_emitter_slot_changed"),TEXT("Fresh emitter ref/handle slot differs; retained ref is not read."));
    if(Row.Instance.IsValid()&&Row.Instance.Get()!=Row.Pointer)return Fail(TEXT("niagara_runtime_emitter_held_ref_changed"),TEXT("Held stateful ref differs from its current slot."));
    const auto& Instance=Fresh[I].Get();
    if(Instance.GetParentSystemInstance()!=E.Solo||Instance.GetEmitterID().ID!=Row.HandleIndex)return Fail(TEXT("niagara_runtime_emitter_parent_changed"),TEXT("Emitter parent or handle index changed."));
    const auto Version=Instance.GetVersionedEmitter();const auto& Handle=Handles[Row.HandleIndex];const auto HandleVersion=Handle.GetInstance();
    if(!Version.Emitter.IsResolved()||!HandleVersion.Emitter.IsResolved()||Version!=Row.Version||HandleVersion!=Row.HandleVersion||Handle.GetId()!=Row.Id||!Handle.GetName().ToString().Equals(Row.Name.ToString(),ESearchCase::CaseSensitive)||Handle.GetIsEnabled()!=Row.Enabled||Handle.GetEmitterMode()!=Row.Mode)return Fail(TEXT("niagara_runtime_emitter_routing_changed"),TEXT("Actual emitter source/version or System handle identity/name/enabled/mode changed."));
   }
  }
  return true;
 }

};
FWork* Current=nullptr;

// TN: a shared controller keeps ownership, but Solo alone is not a phase barrier.
// SDK SetPaused publishes the paused state only after existing work completes.
// This observer never creates that state and never waits for it.
// VA uses the public IsPaused shim before the deprecated raw pointer getter.
// SDK SetPaused waits/finalizes, then PauseInstance waits and moves the instance
// out of Running/PendingSpawn before IsPaused can report its paused state.
// The observer never establishes that barrier or waits for it.
bool CapturePausedShared(FComponentIdentity& C,FNiagaraSystemInstance*& Out,FString& Reason)
{
 auto& W=*Current;Out=nullptr;
 if(C.PausedShared){if(!W.Admit(TEXT("shared_paused_reuse")))return false;Out=C.PausedShared;return true;}
 if(!C.Controller||!C.ControllerValid||!C.Controller->IsValid()||C.Controller->GetSystemInstanceID()!=C.Id||C.Controller->IsSolo())return W.Fail(TEXT("niagara_runtime_shared_controller_changed"),TEXT("Captured live shared controller/ID required before paused/raw instance access."));
 if(!C.Controller->IsPaused()){Reason=TEXT("Running non-Solo instance remains unavailable; observer does not Pause, wait, tick or finalize.");return true;}
 if(!W.SDK(TEXT("Shared_GetPausedSystemInstance")))return false;
 // SDK publishes paused state on the GT after its existing work completes.
 // This is not a general permission to inspect a concurrent raw instance.
 FNiagaraSystemInstance* S=C.Controller->GetSystemInstance_Unsafe();
 W.R.Data->SetBoolField(TEXT("shared_paused_instance_getter_returned"),true);
 if(!S||!S->IsPaused()||S->IsSolo()||S->HasPendingFinalize()||S->GetId()!=C.Id)return W.Fail(TEXT("niagara_runtime_shared_phase_unavailable"),TEXT("Actual shared instance is not already paused/finalized with the captured ID; no follow-up getters."));
 {
  const FNiagaraSystemSimulationPtr Simulation=S->GetSystemSimulation();
  if(!Simulation||!Simulation->IsValid()||Simulation->GetIsSolo()||Simulation->GetSystem()!=C.Asset.Get()||Simulation->GetWorld()!=W.World.Get()||S->GetSystem()!=C.Asset.Get()||S->GetWorld()!=W.World.Get()||S->GetAttachComponent()!=C.Component.Get())return W.Fail(TEXT("niagara_runtime_shared_owner_invalid"),TEXT("Already-paused shared simulation/System/world/component roles do not agree."));
  C.SharedSimulation=Simulation;C.SharedSimulationIdentity=Simulation.Get();C.PausedShared=S;
 } // No new strong simulation ref crosses Pin/After or any emitter/DI callback.
 if(!W.Pin(GEditor))return false;W.SoloEditor=GEditor;
 if(!W.After(TEXT("Shared_GetPausedSystemInstance")))return false;
 W.R.Data->SetBoolField(TEXT("shared_paused_no_observer_barrier_calls"),true);Out=S;return true;
}

bool CapturePausedSolo(UNiagaraComponent* Component,FNiagaraSystemInstance*& Out,FString& Reason,bool AllowSharedPaused=false)
{
 Out=nullptr;auto& W=*Current;if(!W.Admit(TEXT("solo_observer_admission")))return false;
 for(auto& C:W.Components)if(C.Component.Get()==Component)
 {
  if(!C.Controller||!C.ControllerValid){Reason=TEXT("No live system instance.");return true;}
  if(!C.Controller->IsSolo()){if(AllowSharedPaused)return CapturePausedShared(C,Out,Reason);Reason=TEXT("Live non-Solo instance remains unsupported in this reader; no Unsafe getter or implicit ForceSolo.");return true;}
  if(!W.SDK(TEXT("GetSoloSystemInstance")))return false;
  FNiagaraSystemInstance* S=C.Controller->GetSoloSystemInstance();
  if(!S)return W.Fail(TEXT("niagara_runtime_solo_instance_unavailable"),TEXT("Solo controller returned no instance."));
  if(!S->IsPaused()||S->HasPendingFinalize()){Reason=TEXT("Live running/pending-finalize Solo instance remains unsupported; observer does not Pause, tick, wait or finalize.");return W.After(TEXT("GetSoloSystemInstance"));}
  FNiagaraSystemSimulationPtr Simulation=S->GetSystemSimulation();
  if(!Simulation||!Simulation->IsValid()||!Simulation->GetIsSolo()||Simulation->GetSystem()!=C.Asset.Get()||Simulation->GetWorld()!=W.World.Get()||S->GetSystem()!=C.Asset.Get()||S->GetWorld()!=W.World.Get()||S->GetAttachComponent()!=Component)
   return W.Fail(TEXT("niagara_runtime_solo_owner_invalid"),TEXT("Paused Solo simulation/System/world/component roles do not agree."));
  if(!W.Pin(GEditor))return false;W.SoloEditor=GEditor;C.PausedSolo=S;C.SoloSimulation=MoveTemp(Simulation);
  if(!W.After(TEXT("GetSoloSystemInstance")))return false;Out=S;return true;
 }
 return W.Fail(TEXT("niagara_runtime_component_not_captured"),TEXT("Component must be captured before Solo access."));
}

bool ContinueRead(){return Current&&Current->Admit(TEXT("continuation"));}
FString GetStringParam(const TSharedPtr<FJsonObject>& O,const TCHAR* K,const FString& D){FString V=D;if(O)O->TryGetStringField(K,V);return V;}
bool GetBoolParam(const TSharedPtr<FJsonObject>& O,const TCHAR* K,bool D){bool V=D;if(O)O->TryGetBoolField(K,V);return V;}
double GetNumberParam(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double D){double V=D;if(O)O->TryGetNumberField(K,V);return V;}
int32 BoundInt(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double D,int32 Min,int32 Max,bool Floor=false){const double V=GetNumberParam(O,K,D);if(!FMath::IsFinite(V)||V<MIN_int32||V>MAX_int32){Current->Fail(TEXT("niagara_runtime_numeric_range_invalid"),FString(K)+TEXT(" requires a finite safe int32 conversion."));return Min;}return FMath::Clamp(Floor?FMath::FloorToInt(V):static_cast<int32>(V),Min,Max);}
bool NoEmbeddedNul(const FString& S)
{for(int32 I=0;I<S.Len();++I){if(S[I]==TCHAR(0)){return false;}}return true;}
struct FExactKeys:DefaultKeyFuncs<FString>
{static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}};
bool Finite(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Count)
{if(!V.IsValid()||Depth>64||++Count>262144){return false;}if(V->Type==EJson::Number){double N=0;return V->TryGetNumber(N)&&FMath::IsFinite(N);}if(V->Type==EJson::String){return NoEmbeddedNul(V->AsString());}if(V->Type==EJson::Array){for(const auto& X:V->AsArray()){if(!Finite(X,Depth+1,Count)){return false;}}}else if(V->Type==EJson::Object){for(const auto& X:V->AsObject()->Values){if(!NoEmbeddedNul(FString(X.Key))||!Finite(X.Value,Depth+1,Count)){return false;}}}return V->Type!=EJson::None;}
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>1048576||!NoEmbeddedNul(Text)){return false;}
    int32 Depth=0;bool InString=false,Escape=false;for(int32 I=0;I<Text.Len();++I){const TCHAR C=Text[I];if(InString){if(Escape){Escape=false;}else if(C=='\\'){Escape=true;}else if(C=='"'){InString=false;}}else if(C=='"'){InString=true;}else if(C=='{'||C=='['){if(++Depth>64){return false;}}else if(C=='}'||C==']'){if(--Depth<0){return false;}}}if(InString||Depth!=0){return false;}
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));TArray<TSet<FString,FExactKeys>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Tokens=0;
    while(Reader->ReadNext(Token)){if(Token==EJsonNotation::Error||++Tokens>262144){return false;}if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()){const FString K(Reader->GetIdentifier());if(!NoEmbeddedNul(K)||Keys.Last().Contains(K)){return false;}Keys.Last().Add(K);}if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart){Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();}else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd){if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}}
    TArray<TSharedPtr<FJsonValue>> A;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),A)||A.Num()!=1||!A[0].IsValid()||A[0]->Type!=EJson::Object){return false;}int32 Count=0;if(!Finite(A[0],0,Count)){return false;}Out=A[0]->AsObject();return Out.IsValid();
}



struct FStoreSnapshot
{
 TArray<FNiagaraVariableWithOffset> Variables;TArray<uint8> Bytes;TArray<UNiagaraDataInterface*> Interfaces;TArray<UObject*> UObjects;
 bool ParametersDirty=false,InterfacesDirty=false,UObjectsDirty=false,PositionDirty=false;uint32 Layout=0;
 TArrayView<const FNiagaraVariableWithOffset> ReadParameterVariables()const{return MakeArrayView(Variables);}
 const TArray<uint8>& GetParameterDataArray()const{return Bytes;}const TArray<UNiagaraDataInterface*>& GetDataInterfaces()const{return Interfaces;}const TArray<UObject*>& GetUObjects()const{return UObjects;}
 bool GetParametersDirty()const{return ParametersDirty;}bool GetInterfacesDirty()const{return InterfacesDirty;}bool GetUObjectsDirty()const{return UObjectsDirty;}bool GetPositionDataDirty()const{return PositionDirty;}uint32 GetLayoutVersion()const{return Layout;}
 bool Valid(const FNiagaraVariableWithOffset& P)const{const bool Good=P.IsDataInterface()?Interfaces.IsValidIndex(P.Offset):P.IsUObject()?UObjects.IsValidIndex(P.Offset):(P.Offset>=0&&P.GetSizeInBytes()>=0&&static_cast<int64>(P.Offset)+P.GetSizeInBytes()<=Bytes.Num());return Good||Current->Fail(TEXT("niagara_runtime_store_layout_invalid"),TEXT("Typed parameter offset is outside captured byte/object/DI snapshot."));}
 UNiagaraDataInterface* GetDataInterface(int32 I)const{return Interfaces.IsValidIndex(I)?Interfaces[I]:nullptr;}UObject* GetUObject(int32 I)const{return UObjects.IsValidIndex(I)?UObjects[I]:nullptr;}
 const uint8* GetParameterData(int32 I,int32 N)const{return I>=0&&N>=0&&static_cast<int64>(I)+N<=Bytes.Num()?Bytes.GetData()+I:nullptr;}
 template<typename T>T GetParameterValueFromOffset(int32 I)const{T V{};if(I<0||static_cast<int64>(I)+sizeof(T)>Bytes.Num()){Current->Fail(TEXT("niagara_runtime_typed_value_size_invalid"),TEXT("Typed value exceeds captured parameter bytes."));return V;}FMemory::Memcpy(&V,Bytes.GetData()+I,sizeof(T));return V;}
};
bool CaptureStore(const FNiagaraParameterStore& S,FStoreSnapshot& Out)
{
 auto& W=*Current;if(!W.SDK(TEXT("ParameterStore_Snapshot")))return false;TArray<FNiagaraVariableWithOffset> Variables;if(!W.Read(TEXT("ParameterStore_ReadVariables"),[&]{auto View=S.ReadParameterVariables();if(View.Num()>65536){W.Fail(TEXT("niagara_runtime_store_snapshot_budget"),TEXT("Declaration inventory exceeds bound."));return;}Variables.Append(View.GetData(),View.Num());}))return false;const int64 Count=Variables.Num(),ByteCount=S.GetParameterDataArray().Num(),DI=S.GetDataInterfaces().Num(),Obj=S.GetUObjects().Num();if(Count>65536||ByteCount>64ll*1024*1024||DI>65536||Obj>65536||!W.Budget(Count+DI+Obj))return W.Fail(TEXT("niagara_runtime_store_snapshot_budget"),TEXT("Store snapshot exceeds declaration/object/64MiB byte budget."));
 Out.Variables=MoveTemp(Variables);Out.Bytes=S.GetParameterDataArray();for(auto* O:S.GetDataInterfaces())Out.Interfaces.Add(O);for(UObject* O:S.GetUObjects())Out.UObjects.Add(O);Out.ParametersDirty=S.GetParametersDirty();Out.InterfacesDirty=S.GetInterfacesDirty();Out.UObjectsDirty=S.GetUObjectsDirty();Out.PositionDirty=S.GetPositionDataDirty();Out.Layout=S.GetLayoutVersion();
 if(!W.After(TEXT("ParameterStore_Snapshot")))return false;for(auto* O:Out.Interfaces)if(O&&!W.Pin(O))return false;for(auto* O:Out.UObjects)if(O&&!W.Pin(O))return false;for(const auto& V:Out.Variables){if(V.GetType().GetStruct()&&!W.Pin(V.GetType().GetStruct()))return false;if(V.GetType().GetClass()&&!W.Pin(V.GetType().GetClass()))return false;if(V.GetType().GetEnum()&&!W.Pin(V.GetType().GetEnum()))return false;}return true;
}

void AddVectorField(TSharedPtr<FJsonObject> Object, const TCHAR* FieldName, const FVector& Value)
    {
        if(!FMath::IsFinite(Value.X)||!FMath::IsFinite(Value.Y)||!FMath::IsFinite(Value.Z)){Current->Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Observed vector unavailable; not a numeric zero."));return;}
    TSharedPtr<FJsonObject> VectorObject = MakeShared<FJsonObject>();
        VectorObject->SetNumberField(TEXT("x"), Value.X);
        VectorObject->SetNumberField(TEXT("y"), Value.Y);
        VectorObject->SetNumberField(TEXT("z"), Value.Z);
        Object->SetObjectField(FieldName, VectorObject);
    }

void AddRotatorField(TSharedPtr<FJsonObject> Object, const TCHAR* FieldName, const FRotator& Value)
    {
        if(!FMath::IsFinite(Value.Pitch)||!FMath::IsFinite(Value.Yaw)||!FMath::IsFinite(Value.Roll)){Current->Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Observed rotation unavailable; not a numeric zero."));return;}
    TSharedPtr<FJsonObject> RotatorObject = MakeShared<FJsonObject>();
        RotatorObject->SetNumberField(TEXT("pitch"), Value.Pitch);
        RotatorObject->SetNumberField(TEXT("yaw"), Value.Yaw);
        RotatorObject->SetNumberField(TEXT("roll"), Value.Roll);
        Object->SetObjectField(FieldName, RotatorObject);
    }

void AddBoxField(TSharedPtr<FJsonObject> Object, const TCHAR* FieldName, const FBox& Value)
    {
        TSharedPtr<FJsonObject> BoxObject = MakeShared<FJsonObject>();
        BoxObject->SetBoolField(TEXT("is_valid"), Value.IsValid != 0);
        AddVectorField(BoxObject, TEXT("min"), Value.Min);
        AddVectorField(BoxObject, TEXT("max"), Value.Max);
        if (Value.IsValid)
        {
            AddVectorField(BoxObject, TEXT("center"), Value.GetCenter());
            AddVectorField(BoxObject, TEXT("extent"), Value.GetExtent());
            AddVectorField(BoxObject, TEXT("size"), Value.GetSize());
        }
        Object->SetObjectField(FieldName, BoxObject);
    }

TArray<TSharedPtr<FJsonValue>> NumberArrayToJson(const TArray<double>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Array;
        Array.Reserve(Values.Num());
        for (double Value : Values)
        {
            if(!FMath::IsFinite(Value)){Current->Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Non-finite observed value is unavailable, not zero."));return {};}Array.Add(MakeShared<FJsonValueNumber>(Value));
        }
        return Array;
    }

FString NiagaraRuntimeStateObjectPath(const UObject* Object)
    {
        return Object ? Object->GetPathName() : FString();
    }

FString NiagaraRuntimeStateClassName(const UObject* Object)
    {
        return Object && Object->GetClass() ? Object->GetClass()->GetPathName() : FString();
    }

FString NiagaraExecutionStateToString(ENiagaraExecutionState State)
    {
        if (const UEnum* Enum = StaticEnum<ENiagaraExecutionState>())
        {
            return Enum->GetNameStringByValue(static_cast<int64>(State));
        }
        return FString::FromInt(static_cast<int32>(State));
    }

FString NiagaraSimTargetToString(ENiagaraSimTarget Target)
    {
        if (const UEnum* Enum = StaticEnum<ENiagaraSimTarget>())
        {
            return Enum->GetNameStringByValue(static_cast<int64>(Target));
        }
        return FString::FromInt(static_cast<int32>(Target));
    }

bool NiagaraRuntimeStateMatchesFilter(const FString& Value, const FString& Filter)
    {
        return Filter.IsEmpty() || Value.Contains(Filter, ESearchCase::IgnoreCase);
    }

FString NiagaraRuntimeStateBytesToHex(const uint8* Data, int32 ByteCount)
    {
        FString Hex;
        Hex.Reserve(FMath::Max(0, ByteCount) * 2);
        for (int32 ByteIndex = 0; ByteIndex < ByteCount; ++ByteIndex)
        {
            Hex += FString::Printf(TEXT("%02x"), Data[ByteIndex]);
        }
        return Hex;
    }

void AddNiagaraRuntimeStateRawBytes(
        TSharedPtr<FJsonObject> ParameterObject,
        const FStoreSnapshot& Store,
        const FNiagaraVariableWithOffset& Parameter,
        bool bIncludeRawBytes,
        int32 MaxRawBytes)
    {
        const int32 SizeInBytes = Parameter.GetSizeInBytes();
        ParameterObject->SetNumberField(TEXT("size_bytes"), SizeInBytes);
        ParameterObject->SetStringField(TEXT("value_type"), TEXT("raw_bytes"));
        if (!bIncludeRawBytes || SizeInBytes <= 0)
        {
            ParameterObject->SetBoolField(TEXT("raw_bytes_included"), false);
            return;
        }

        const int32 StoreByteCount = Store.GetParameterDataArray().Num();
        if (Parameter.Offset < 0 || static_cast<int64>(Parameter.Offset) + SizeInBytes > StoreByteCount)
        {
            ParameterObject->SetBoolField(TEXT("raw_bytes_included"), false);
            ParameterObject->SetStringField(TEXT("raw_bytes_error"), TEXT("Parameter data offset is outside the store data buffer"));
            return;
        }

        const int32 ClampedByteCount = FMath::Clamp(MaxRawBytes, 0, SizeInBytes);
        if(!Current->Budget(1,static_cast<int64>(ClampedByteCount)*2))return;
    const uint8* RawData = Store.GetParameterData(Parameter.Offset, SizeInBytes);
        ParameterObject->SetBoolField(TEXT("raw_bytes_included"), ClampedByteCount > 0);
        ParameterObject->SetNumberField(TEXT("raw_byte_count"), ClampedByteCount);
        ParameterObject->SetNumberField(TEXT("raw_total_byte_count"), SizeInBytes);
        if (ClampedByteCount > 0)
        {
            ParameterObject->SetStringField(TEXT("raw_bytes_hex"), NiagaraRuntimeStateBytesToHex(RawData, ClampedByteCount));
        }
        ParameterObject->SetBoolField(TEXT("raw_bytes_truncated"), ClampedByteCount < SizeInBytes);
    }

FString TruncateNiagaraRuntimeStateString(const FString& Value, int32 MaxCharacters)
    {
        const int32 ClampedMaxCharacters = FMath::Clamp(MaxCharacters, 0, 16384);
        if (ClampedMaxCharacters <= 0 || Value.Len() <= ClampedMaxCharacters)
        {
            return Value;
        }
        return Value.Left(ClampedMaxCharacters) + TEXT("...");
    }


FString ExportNiagaraRuntimeStatePropertyValue(FProperty* Property,UObject* Owner,int32 MaxCharacters,int32 ArrayIndex=INDEX_NONE,int32 ExpectedArrayCount=0)
{
 auto& W=*Current;FString Out;if(!W.Admit(TEXT("property_export_admission"))||!W.Pin(Owner))return Out;const FName Name=Property->GetFName();auto* CurrentProperty=FindFProperty<FProperty>(Owner->GetClass(),Name);if(CurrentProperty!=Property){W.Fail(TEXT("niagara_runtime_property_replaced"),TEXT("Exact reflected property changed before export."));return Out;}
 const void* Data=Property->ContainerPtrToValuePtr<void>(Owner);FProperty* ExportProperty=Property;
 if(ArrayIndex!=INDEX_NONE){auto* A=CastField<FArrayProperty>(Property);if(!A||!A->Inner){W.Fail(TEXT("niagara_runtime_property_type_changed"),TEXT("Array property changed."));return Out;}FScriptArrayHelper H(A,Data);if(H.Num()!=ExpectedArrayCount||ArrayIndex<0||ArrayIndex>=H.Num()){W.Fail(TEXT("niagara_runtime_array_changed"),TEXT("Array count/index changed across reflection callback."));return Out;}Data=H.GetRawPtr(ArrayIndex);ExportProperty=A->Inner;}
 if(!W.Read(TEXT("Property_ExportText"),[&]{ExportProperty->ExportTextItem_Direct(Out,Data,nullptr,Owner,PPF_None);}))return {};
 if(FindFProperty<FProperty>(Owner->GetClass(),Name)!=Property){W.Fail(TEXT("niagara_runtime_property_replaced"),TEXT("Property changed during export callback."));return {};}
 if(ArrayIndex!=INDEX_NONE){auto* A=CastField<FArrayProperty>(Property);FScriptArrayHelper H(A,Property->ContainerPtrToValuePtr<void>(Owner));if(H.Num()!=ExpectedArrayCount){W.Fail(TEXT("niagara_runtime_array_changed"),TEXT("Array changed during export callback; no stale pointer reused."));return {};}}
 Out=TruncateNiagaraRuntimeStateString(Out,MaxCharacters);if(!W.Budget(1,Out.Len()))return {};return Out;
}
TSharedPtr<FJsonObject> BuildNiagaraRuntimeStateObjectPropertyPreview(UObject* Object,int32 MaxProperties,int32 MaxArrayElements,int32 MaxPropertyValueChars)
{
 auto& W=*Current;auto Preview=MakeShared<FJsonObject>();if(!W.Admit(TEXT("property_preview")))return Preview;Preview->SetStringField(TEXT("object_path"),NiagaraRuntimeStateObjectPath(Object));Preview->SetStringField(TEXT("object_class"),NiagaraRuntimeStateClassName(Object));TArray<TSharedPtr<FJsonValue>> Rows;int32 Total=0;
 if(Object){if(!W.Pin(Object))return Preview;TArray<FName> Names;for(TFieldIterator<FProperty> It(Object->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)if(!It->HasAnyPropertyFlags(CPF_Deprecated)){if(!W.Budget(1))return Preview;Names.Add(It->GetFName());}Total=Names.Num();
  for(int32 I=0;I<FMath::Min(Names.Num(),MaxProperties);++I){if(!W.Admit(TEXT("property_preview_row")))return Preview;auto* P=FindFProperty<FProperty>(Object->GetClass(),Names[I]);if(!P){W.Fail(TEXT("niagara_runtime_property_replaced"),TEXT("Requested property disappeared."));return Preview;}auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),P->GetName());J->SetStringField(TEXT("cpp_type"),P->GetCPPType());J->SetStringField(TEXT("class_name"),P->GetClass()->GetName());Rows.Add(MakeShared<FJsonValueObject>(J));
   if(auto* A=CastField<FArrayProperty>(P)){FScriptArrayHelper H(A,A->ContainerPtrToValuePtr<void>(Object));const int32 N=H.Num();if(N<0){W.Fail(TEXT("niagara_runtime_array_shape_invalid"),TEXT("Negative reflected array count."));return Preview;}J->SetBoolField(TEXT("is_array"),true);J->SetStringField(TEXT("inner_cpp_type"),A->Inner?A->Inner->GetCPPType():FString());J->SetNumberField(TEXT("array_count"),N);J->SetBoolField(TEXT("array_truncated"),N>MaxArrayElements);TArray<TSharedPtr<FJsonValue>> Values;for(int32 K=0;K<FMath::Min(N,MaxArrayElements);++K){FString V=ExportNiagaraRuntimeStatePropertyValue(P,Object,MaxPropertyValueChars,K,N);if(!W.Admit(TEXT("property_array_exported")))return Preview;Values.Add(MakeShared<FJsonValueString>(V));}J->SetArrayField(TEXT("array_samples"),Values);}
   else{if(auto* OP=CastField<FObjectPropertyBase>(P)){UObject* V=nullptr;if(!W.Read(TEXT("ObjectProperty_Value"),[&]{V=OP->GetObjectPropertyValue_InContainer(Object);}))return Preview;if(V&&!W.Pin(V))return Preview;J->SetBoolField(TEXT("is_object"),true);J->SetStringField(TEXT("object_path"),NiagaraRuntimeStateObjectPath(V));J->SetStringField(TEXT("object_class"),NiagaraRuntimeStateClassName(V));}else J->SetBoolField(TEXT("is_array"),false);const FString V=ExportNiagaraRuntimeStatePropertyValue(P,Object,MaxPropertyValueChars);if(!W.Admit(TEXT("property_exported")))return Preview;J->SetStringField(TEXT("export_text"),V);}
  }
 }
 Preview->SetNumberField(TEXT("total_property_count"),Total);Preview->SetNumberField(TEXT("returned_property_count"),Rows.Num());Preview->SetBoolField(TEXT("truncated"),Rows.Num()<Total);Preview->SetNumberField(TEXT("max_array_elements"),MaxArrayElements);Preview->SetNumberField(TEXT("max_property_value_chars"),MaxPropertyValueChars);Preview->SetArrayField(TEXT("properties"),Rows);Preview->SetBoolField(TEXT("atomic_object_snapshot_verified"),false);return Preview;
}

void AddNiagaraRuntimeStateParameterValue(
        TSharedPtr<FJsonObject> ParameterObject,
        const FStoreSnapshot& Store,
        const FNiagaraVariableWithOffset& Parameter,
        bool bIncludeRawBytes,
        int32 MaxRawBytes,
        bool bIncludeParameterObjectProperties,
        int32 MaxObjectProperties,
        int32 MaxArrayElements,
        int32 MaxPropertyValueChars)
    {
        if(!ContinueRead()||!Store.Valid(Parameter)){ParameterObject->SetBoolField(TEXT("value_available"),false);return;}
    const FNiagaraTypeDefinition& Type = Parameter.GetType();
        if (Parameter.IsDataInterface())
        {
            UNiagaraDataInterface* DataInterface = Store.GetDataInterface(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("data_interface"));
            ParameterObject->SetStringField(TEXT("object_path"), NiagaraRuntimeStateObjectPath(DataInterface));
            ParameterObject->SetStringField(TEXT("object_class"), NiagaraRuntimeStateClassName(DataInterface));
            if (bIncludeParameterObjectProperties)
            {
                ParameterObject->SetObjectField(
                    TEXT("object_properties"),
                    BuildNiagaraRuntimeStateObjectPropertyPreview(DataInterface, MaxObjectProperties, MaxArrayElements, MaxPropertyValueChars));
            }
            return;
        }

        if (Parameter.IsUObject())
        {
            UObject* Object = Store.GetUObject(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("uobject"));
            ParameterObject->SetStringField(TEXT("object_path"), NiagaraRuntimeStateObjectPath(Object));
            ParameterObject->SetStringField(TEXT("object_class"), NiagaraRuntimeStateClassName(Object));
            if (bIncludeParameterObjectProperties)
            {
                ParameterObject->SetObjectField(
                    TEXT("object_properties"),
                    BuildNiagaraRuntimeStateObjectPropertyPreview(Object, MaxObjectProperties, MaxArrayElements, MaxPropertyValueChars));
            }
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetFloatDef())
        {
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("float"));
            const float V=Store.GetParameterValueFromOffset<float>(Parameter.Offset);if(!FMath::IsFinite(V)){Current->Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Observed float is unavailable, not zero."));ParameterObject->SetBoolField(TEXT("value_available"),false);return;}ParameterObject->SetNumberField(TEXT("value"),V);
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetIntDef())
        {
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("int"));
            ParameterObject->SetNumberField(TEXT("value"), Store.GetParameterValueFromOffset<int32>(Parameter.Offset));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetBoolDef())
        {
            const FNiagaraBool Value = Store.GetParameterValueFromOffset<FNiagaraBool>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("bool"));
            ParameterObject->SetBoolField(TEXT("value"), Value.GetValue());
            ParameterObject->SetNumberField(TEXT("raw_bool_value"), Value.GetRawValue());
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetVec2Def())
        {
            const FVector2f Value = Store.GetParameterValueFromOffset<FVector2f>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("vec2"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetVec3Def())
        {
            const FVector3f Value = Store.GetParameterValueFromOffset<FVector3f>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("vec3"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y, Value.Z}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetPositionDef())
        {
            const FNiagaraPosition Value = Store.GetParameterValueFromOffset<FNiagaraPosition>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("position"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y, Value.Z}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetVec4Def())
        {
            const FVector4f Value = Store.GetParameterValueFromOffset<FVector4f>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("vec4"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y, Value.Z, Value.W}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetColorDef())
        {
            const FLinearColor Value = Store.GetParameterValueFromOffset<FLinearColor>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("linear_color"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.R, Value.G, Value.B, Value.A}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetQuatDef())
        {
            const FQuat4f Value = Store.GetParameterValueFromOffset<FQuat4f>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("quat"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y, Value.Z, Value.W}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetMatrix4Def())
        {
            const FMatrix44f Value = Store.GetParameterValueFromOffset<FMatrix44f>(Parameter.Offset);
            TArray<double> Values;
            Values.Reserve(16);
            for (int32 Row = 0; Row < 4; ++Row)
            {
                for (int32 Column = 0; Column < 4; ++Column)
                {
                    Values.Add(Value.M[Row][Column]);
                }
            }
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("matrix4"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(Values));
            return;
        }

        AddNiagaraRuntimeStateRawBytes(ParameterObject, Store, Parameter, bIncludeRawBytes, MaxRawBytes);
    }
TArray<TSharedPtr<FJsonValue>> BuildNiagaraRuntimeStateParameterArray(
        const FStoreSnapshot& Store,
        const FString& ParameterNameContains,
        int32 MaxParameters,
        bool bIncludeParameterValues,
        bool bIncludeRawBytes,
        int32 MaxRawBytes,
        bool bIncludeParameterObjectProperties,
        int32 MaxObjectProperties,
        int32 MaxArrayElements,
        int32 MaxPropertyValueChars,
        int32& OutTotalParameterCount,
        int32& OutReturnedParameterCount,
        int32& OutFilteredParameterCount,
        bool& bOutTruncated)
    {
        TArray<TSharedPtr<FJsonValue>> Parameters;
        OutTotalParameterCount = 0;
        OutReturnedParameterCount = 0;
        OutFilteredParameterCount = 0;
        bOutTruncated = false;

        const TArrayView<const FNiagaraVariableWithOffset> ParameterVariables = Store.ReadParameterVariables();
        OutTotalParameterCount = ParameterVariables.Num();
        const int32 ClampedMaxParameters = FMath::Clamp(MaxParameters, 0, 4096);
        for (const FNiagaraVariableWithOffset& Parameter : ParameterVariables)
        {
            if(!ContinueRead()||!Current->Budget(1,Parameter.GetName().ToString().Len()))return Parameters;
        const FString ParameterName = Parameter.GetName().ToString();
            if (!NiagaraRuntimeStateMatchesFilter(ParameterName, ParameterNameContains))
            {
                ++OutFilteredParameterCount;
                continue;
            }

            if (OutReturnedParameterCount >= ClampedMaxParameters)
            {
                bOutTruncated = true;
                continue;
            }

            TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
            const FNiagaraTypeDefinition& Type = Parameter.GetType();
            ParameterObject->SetStringField(TEXT("name"), ParameterName);
            ParameterObject->SetStringField(TEXT("type_name"), Type.GetName());
            ParameterObject->SetStringField(TEXT("type_display_name"), Type.GetNameText().ToString());
            ParameterObject->SetNumberField(TEXT("offset"), Parameter.Offset);
            ParameterObject->SetNumberField(TEXT("size_bytes"), Parameter.GetSizeInBytes());
            ParameterObject->SetBoolField(TEXT("is_data_interface"), Parameter.IsDataInterface());
            ParameterObject->SetBoolField(TEXT("is_uobject"), Parameter.IsUObject());
            ParameterObject->SetBoolField(TEXT("is_enum"), Type.IsEnum());
            ParameterObject->SetBoolField(TEXT("is_static"), Type.IsStatic());
            if (UStruct* Struct = Type.GetStruct())
            {
                ParameterObject->SetStringField(TEXT("struct_path"), Struct->GetPathName());
            }
            if (UClass* Class = Type.GetClass())
            {
                ParameterObject->SetStringField(TEXT("class_path"), Class->GetPathName());
            }
            if (UEnum* Enum = Type.GetEnum())
            {
                ParameterObject->SetStringField(TEXT("enum_path"), Enum->GetPathName());
            }

            if (bIncludeParameterValues)
            {
                AddNiagaraRuntimeStateParameterValue(
                    ParameterObject,
                    Store,
                    Parameter,
                    bIncludeRawBytes,
                    MaxRawBytes,
                    bIncludeParameterObjectProperties,
                    MaxObjectProperties,
                    MaxArrayElements,
                    MaxPropertyValueChars);
            }

            Parameters.Add(MakeShared<FJsonValueObject>(ParameterObject));if(!ContinueRead())return Parameters;
            ++OutReturnedParameterCount;
        }

        return Parameters;
    }
void AddNiagaraRuntimeStateStore(
        TSharedPtr<FJsonObject> Target,
        const TCHAR* FieldName,
        const FNiagaraParameterStore& Store,
        const FString& ParameterNameContains,
        int32 MaxParameters,
        bool bIncludeParameterValues,
        bool bIncludeRawBytes,
        int32 MaxRawBytes,
        bool bIncludeParameterObjectProperties,
        int32 MaxObjectProperties,
        int32 MaxArrayElements,
        int32 MaxPropertyValueChars)
    {
        FStoreSnapshot Snapshot;if(!CaptureStore(Store,Snapshot))return;
    int32 TotalParameterCount = 0;
        int32 ReturnedParameterCount = 0;
        int32 FilteredParameterCount = 0;
        bool bTruncated = false;
        TArray<TSharedPtr<FJsonValue>> Parameters = BuildNiagaraRuntimeStateParameterArray(
            Snapshot,
            ParameterNameContains,
            MaxParameters,
            bIncludeParameterValues,
            bIncludeRawBytes,
            MaxRawBytes,
            bIncludeParameterObjectProperties,
            MaxObjectProperties,
            MaxArrayElements,
            MaxPropertyValueChars,
            TotalParameterCount,
            ReturnedParameterCount,
            FilteredParameterCount,
            bTruncated);

        TSharedPtr<FJsonObject> StoreObject = MakeShared<FJsonObject>();
        StoreObject->SetNumberField(TEXT("total_parameter_count"), TotalParameterCount);
        StoreObject->SetNumberField(TEXT("returned_parameter_count"), ReturnedParameterCount);
        StoreObject->SetNumberField(TEXT("filtered_parameter_count"), FilteredParameterCount);
        StoreObject->SetBoolField(TEXT("truncated"), bTruncated);
        StoreObject->SetBoolField(TEXT("parameters_dirty"), Snapshot.GetParametersDirty());
        StoreObject->SetBoolField(TEXT("interfaces_dirty"), Snapshot.GetInterfacesDirty());
        StoreObject->SetBoolField(TEXT("uobjects_dirty"), Snapshot.GetUObjectsDirty());
        StoreObject->SetBoolField(TEXT("position_data_dirty"), Snapshot.GetPositionDataDirty());
        StoreObject->SetNumberField(TEXT("layout_version"), Snapshot.GetLayoutVersion());
        StoreObject->SetNumberField(TEXT("data_interface_count"), Snapshot.GetDataInterfaces().Num());
        StoreObject->SetNumberField(TEXT("uobject_count"), Snapshot.GetUObjects().Num());
        StoreObject->SetNumberField(TEXT("parameter_data_byte_count"), Snapshot.GetParameterDataArray().Num());
        StoreObject->SetArrayField(TEXT("parameters"), Parameters);
        StoreObject->SetBoolField(TEXT("owned_value_snapshot"),true);StoreObject->SetBoolField(TEXT("atomic_cross_store_frame_snapshot_verified"),false);Target->SetObjectField(FieldName,StoreObject);
    }

// VA extends UJ counters to already-paused shared instances; no observer establishes the barrier.
TSharedPtr<FJsonObject> BuildNiagaraRuntimeStateEmitterInstances(UNiagaraComponent* Component,int32 Max)
{
    auto O=MakeShared<FJsonObject>();auto& W=*Current;
    if(!W.Admit(TEXT("emitter_runtime_begin")))return O;
    auto Prefix=MakeShared<FJsonObject>();Prefix->SetStringField(TEXT("component_path"),Component->GetPathName());Prefix->SetObjectField(TEXT("emitter_runtime"),O);W.Prefix.Add(MakeShared<FJsonValueObject>(Prefix));
    const auto Controller=Component->GetSystemInstanceController();
    O->SetBoolField(TEXT("read_only"),true);O->SetBoolField(TEXT("has_component"),true);
    O->SetBoolField(TEXT("has_system_instance_controller"),Controller.IsValid());
    O->SetBoolField(TEXT("system_instance_controller_valid"),Controller&&Controller->IsValid());
    O->SetBoolField(TEXT("has_system_instance"),Controller&&Controller->IsValid());
    O->SetNumberField(TEXT("max_emitter_instances"),Max);O->SetArrayField(TEXT("emitters"),{});
    O->SetNumberField(TEXT("returned_emitter_instance_count"),0);O->SetBoolField(TEXT("runtime_counters_available"),false);
    O->SetBoolField(TEXT("runtime_snapshot_complete"),false);O->SetBoolField(TEXT("numeric_zero_is_runtime_measurement"),false);
    O->SetBoolField(TEXT("observer_changed_runtime_state"),false);O->SetBoolField(TEXT("GPU_frame_completion_verified"),false);
    O->SetStringField(TEXT("access_note"),TEXT("Actual public system/emitter getters from an already-paused Solo or shared instance. The deprecated raw getter is used only after current shared controller/ID/IsPaused admission; no observer Pause/ForceSolo/tick/wait/finalize. GPU particle count follows the SDK fence-dependent latent count or spawned-particle estimate; it is not current GPU completion. Stateless-specific counters remain incomplete."));
    FNiagaraSystemInstance* Solo=nullptr;FString Reason;
    if(!CapturePausedSolo(Component,Solo,Reason,true))return O;
    O->SetBoolField(TEXT("already_paused_solo"),Solo&&Controller->IsSolo());O->SetBoolField(TEXT("already_paused_shared"),Solo&&!Controller->IsSolo());O->SetBoolField(TEXT("already_paused_instance"),Solo!=nullptr);O->SetStringField(TEXT("unavailable_reason"),Reason);
    if(!Solo)return O;

    // Capture raw identities first. Stateless refs must never outlive their System owner.
    // Only the public runtime AsStateful result permits a retained shared ref, after current.
    if(!W.SDK(TEXT("Solo_GetEmitters")))return O;
    TArray<FNiagaraEmitterInstance*> Pointers;
    {const auto View=Solo->GetEmitters();if(!W.Budget(View.Num()))return O;for(const auto& Ref:View)Pointers.Add(&Ref.Get());}
    if(!W.After(TEXT("Solo_GetEmitters")))return O;
    auto Inventory=MakeUnique<FEmitterRuntimeInventory>();Inventory->Component=Component;Inventory->Solo=Solo;Inventory->SharedPaused=!Controller->IsSolo();Inventory->Asset=Component->GetAsset();
    if(!Live(Inventory->Asset.Get())){W.Fail(TEXT("niagara_runtime_emitter_asset_unavailable"),TEXT("Paused System asset unavailable."));return O;}
    Inventory->HandleCount=Inventory->Asset->GetEmitterHandles().Num();
    Inventory->Rows.Reserve(Pointers.Num());
    for(int32 Slot=0;Slot<Pointers.Num();++Slot)
    {
        if(!W.Admit(TEXT("emitter_runtime_capture_role")))return O;
        const auto Fresh=Solo->GetEmitters();
        if(Fresh.Num()!=Pointers.Num()||&Fresh[Slot].Get()!=Pointers[Slot]){W.Fail(TEXT("niagara_runtime_emitter_capture_changed"),TEXT("Fresh runtime membership changed before raw metadata capture."));return O;}
        auto* Ref=Pointers[Slot];
        if(Ref->GetParentSystemInstance()!=Solo){W.Fail(TEXT("niagara_runtime_emitter_parent_changed"),TEXT("Emitter shared reference has a different parent System instance."));return O;}
        const int32 HandleIndex=Ref->GetEmitterID().ID;
        if(!Inventory->Asset->GetEmitterHandles().IsValidIndex(HandleIndex)){W.Fail(TEXT("niagara_runtime_emitter_handle_index"),TEXT("Actual emitter ID is outside current System handle array; GetEmitterHandle was not called."));return O;}
        FEmitterRuntimeRole Role;Role.Pointer=Ref;Role.HandleIndex=HandleIndex;
        const auto& Handle=Inventory->Asset->GetEmitterHandles()[HandleIndex];
        Role.Id=Handle.GetId();Role.Name=Handle.GetName();Role.Enabled=Handle.GetIsEnabled();Role.Mode=Handle.GetEmitterMode();Role.HandleVersion=Handle.GetInstance();Role.Version=Ref->GetVersionedEmitter();
        if(!Role.HandleVersion.Emitter.IsResolved()||!Role.Version.Emitter.IsResolved()){W.Fail(TEXT("niagara_runtime_emitter_source_unresolved"),TEXT("Emitter source slots are unresolved; no implicit source resolution or counter dispatch."));return O;}
        // A running instance may legitimately retain a different source/version than its current handle.
        // Capture and protect both actual roles instead of assuming that they are equal.
        TStrongObjectPtr<UNiagaraEmitter> InstanceSource(Role.Version.Emitter.Get()),HandleSource(Role.HandleVersion.Emitter.Get());
        if(!W.Admit(TEXT("emitter_runtime_source_return")))return O;
        if(InstanceSource&&!W.Pin(InstanceSource.Get()))return O;
        if(HandleSource&&!W.Pin(HandleSource.Get()))return O;
        Inventory->Rows.Add(MoveTemp(Role));
    }
    auto* Captured=Inventory.Get();W.EmitterInventories.Add(MoveTemp(Inventory));
    if(!W.Admit(TEXT("emitter_runtime_roles_captured")))return O;
    auto Classifications=MakeShared<FJsonObject>();O->SetObjectField(TEXT("runtime_ref_classification"),Classifications);
    for(int32 Index=0;Index<Captured->Rows.Num();++Index)
    {
        auto& Role=Captured->Rows[Index];
        if(!W.SDK(TEXT("Emitter_AsStateful")))return O;
        Role.Stateful=Role.Pointer->AsStateful()!=nullptr;
        Classifications->SetBoolField(FString::FromInt(Index),Role.Stateful);
        if(!W.After(TEXT("Emitter_AsStateful")))return O;
        // Current has revalidated actual array membership before copying this shared ref.
        // A changed Handle.Mode alone cannot classify a retained old runtime implementation.
        if(Role.Stateful)Role.Instance=Solo->GetEmitters()[Index];
        if(!W.Admit(TEXT("emitter_runtime_ref_held")))return O;
    }
    O->SetNumberField(TEXT("emitter_instance_count"),Captured->Rows.Num());
    O->SetBoolField(TEXT("emitters_truncated"),Captured->Rows.Num()>Max);
    O->SetBoolField(TEXT("runtime_counters_available"),true);O->SetBoolField(TEXT("numeric_zero_is_runtime_measurement"),true);

    // Each exported/virtual getter returns a copied scalar before its current fence.
    // The JSON prefix remains available even if a later callback invalidates the observation.
    if(!W.Read(TEXT("Solo_GetAge"),[&]{const double Age=Solo->GetAge();if(FMath::IsFinite(Age))O->SetNumberField(TEXT("age"),Age);else W.Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Observed System age is non-finite."));}))return O;
    if(!W.Read(TEXT("Solo_RequestedExecutionState"),[&]{O->SetStringField(TEXT("requested_execution_state"),NiagaraExecutionStateToString(Solo->GetRequestedExecutionState()));}))return O;
    if(!W.Read(TEXT("Solo_ActualExecutionState"),[&]{O->SetStringField(TEXT("actual_execution_state"),NiagaraExecutionStateToString(Solo->GetActualExecutionState()));}))return O;
    if(!W.Read(TEXT("Solo_Flags"),[&]{O->SetBoolField(TEXT("is_complete"),Solo->IsComplete());O->SetBoolField(TEXT("is_disabled"),Solo->IsDisabled());O->SetBoolField(TEXT("is_solo"),Solo->IsSolo());O->SetBoolField(TEXT("needs_gpu_tick"),Solo->NeedsGPUTick());O->SetNumberField(TEXT("active_gpu_emitter_count"),Solo->ActiveGPUEmitterCount);O->SetNumberField(TEXT("total_gpu_param_size"),Solo->TotalGPUParamSize);}))return O;
    if(!W.Read(TEXT("Solo_LocalBounds"),[&]{const FBox Value=Solo->GetLocalBounds();AddBoxField(O,TEXT("local_bounds"),Value);}))return O;
    if(!W.Read(TEXT("Solo_SystemFixedBounds"),[&]{const FBox Value=Solo->GetSystemFixedBounds();AddBoxField(O,TEXT("system_fixed_bounds"),Value);}))return O;

    int64 TotalNumParticles=0,TotalSpawnedParticles=0;bool Complete=true;int32 CounterRows=0;
    TArray<TSharedPtr<FJsonValue>> Values;
    auto Publish=[&]{O->SetArrayField(TEXT("emitters"),Values);O->SetNumberField(TEXT("returned_emitter_instance_count"),Values.Num());O->SetNumberField(TEXT("returned_emitter_counter_count"),CounterRows);};
    const int32 Count=FMath::Min(Captured->Rows.Num(),Max);
    for(int32 Index=0;Index<Count;++Index)
    {
        if(!W.Admit(TEXT("emitter_runtime_row")))return O;
        const auto& Role=Captured->Rows[Index];
        auto Row=MakeShared<FJsonObject>();Values.Add(MakeShared<FJsonValueObject>(Row));Publish();
        Row->SetNumberField(TEXT("index"),Index);Row->SetStringField(TEXT("emitter_name"),Role.Name.ToString());Row->SetBoolField(TEXT("handle_enabled"),Role.Enabled);
        Row->SetBoolField(TEXT("runtime_counters_available"),false);
        if(!Role.Stateful)
        {
            Complete=false;Row->SetStringField(TEXT("runtime_unavailable_reason"),TEXT("Stateless emitter-specific instance state/layout remains incomplete; no native raw cast or synthetic counters."));continue;
        }
        const auto Ref=Role.Instance;
        if(!W.Read(TEXT("Emitter_AssetPath"),[&]{Row->SetStringField(TEXT("emitter_asset_path"),NiagaraRuntimeStateObjectPath(Role.Version.Emitter.Get()));}))return O;
        if(!W.Read(TEXT("Emitter_ExecutionState"),[&]{Row->SetStringField(TEXT("execution_state"),NiagaraExecutionStateToString(Ref->GetExecutionState()));}))return O;
        if(!W.Read(TEXT("Emitter_Flags"),[&]{Row->SetBoolField(TEXT("is_active"),Ref->IsActive());Row->SetBoolField(TEXT("is_inactive"),Ref->IsInactive());Row->SetBoolField(TEXT("is_complete"),Ref->IsComplete());Row->SetBoolField(TEXT("is_disabled"),Ref->IsDisabled());Row->SetBoolField(TEXT("is_local_space"),Ref->IsLocalSpace());Row->SetBoolField(TEXT("is_deterministic"),Ref->IsDeterministic());Row->SetBoolField(TEXT("needs_partial_depth_texture"),Ref->NeedsPartialDepthTexture());Row->SetBoolField(TEXT("needs_early_view_uniform_buffer"),Ref->NeedsEarlyViewUniformBuffer());Row->SetBoolField(TEXT("has_gpu_context"),Ref->GetGPUContext()!=nullptr);Row->SetBoolField(TEXT("has_compute_data_buffer_interface"),Ref->GetComputeDataBufferInterface()!=nullptr);Row->SetBoolField(TEXT("bounds_dynamic"),Ref->AreBoundsDynamic());}))return O;
        ENiagaraSimTarget Target=ENiagaraSimTarget::CPUSim;
        if(!W.Read(TEXT("Emitter_SimTarget"),[&]{Target=Ref->GetSimTarget();Row->SetStringField(TEXT("sim_target"),NiagaraSimTargetToString(Target));}))return O;
        int32 Particles=0,Spawned=0;
        if(!W.Read(TEXT("Emitter_GetNumParticles"),[&]{Particles=Ref->GetNumParticles();Row->SetNumberField(TEXT("num_particles"),Particles);}))return O;
        if(!W.Read(TEXT("Emitter_GetTotalSpawnedParticles"),[&]{Spawned=Ref->GetTotalSpawnedParticles();Row->SetNumberField(TEXT("total_spawned_particles"),Spawned);}))return O;
        if(!W.Read(TEXT("Emitter_ShouldTick"),[&]{Row->SetBoolField(TEXT("should_tick"),Ref->ShouldTick());}))return O;
        if(!W.Read(TEXT("Emitter_GpuCountBufferEstimate"),[&]{const uint32 Estimate=Ref->GetGpuCountBufferEstimate();Row->SetNumberField(TEXT("gpu_count_buffer_estimate"),Estimate);}))return O;
        if(!W.Read(TEXT("Emitter_Bounds"),[&]{const FBox Value=Ref->GetBounds();AddBoxField(Row,TEXT("bounds"),Value);}))return O;
        Row->SetStringField(TEXT("fixed_bounds_note"),TEXT("Emitter fixed bounds are intentionally omitted because FNiagaraEmitterInstance::GetFixedBounds is not exported from the Niagara module in UE 5.6."));
        Row->SetStringField(TEXT("particle_count_scope"),Target==ENiagaraSimTarget::GPUComputeSim?TEXT("SDK GPU particle-count read fence selects latent current count or TotalSpawnedParticles estimate; not current GPU execution/readback."):TEXT("SDK CPU current dataset instance count (zero when no current buffer), sampled while already paused (Solo or shared); not a fresh simulation step."));
        Row->SetBoolField(TEXT("gpu_count_buffer_estimate_is_sdk_estimate"),true);Row->SetBoolField(TEXT("runtime_counters_available"),true);
        TotalNumParticles+=int64(Particles);TotalSpawnedParticles+=int64(Spawned);++CounterRows;Publish();
    }
    if(!W.Admit(TEXT("emitter_runtime_complete")))return O;
    // Never publish an apparently complete zero total when any requested row was unavailable.
    O->SetBoolField(TEXT("returned_particle_totals_complete"),Complete);
    if(Complete){O->SetNumberField(TEXT("total_num_particles_returned_emitters"),double(TotalNumParticles));O->SetNumberField(TEXT("total_spawned_particles_returned_emitters"),double(TotalSpawnedParticles));}
    else{O->SetField(TEXT("total_num_particles_returned_emitters"),MakeShared<FJsonValueNull>());O->SetField(TEXT("total_spawned_particles_returned_emitters"),MakeShared<FJsonValueNull>());O->SetStringField(TEXT("unavailable_reason"),TEXT("At least one requested emitter counter row is unavailable; partial rows and System counters are preserved."));}
    O->SetBoolField(TEXT("runtime_snapshot_complete"),Complete);O->SetBoolField(TEXT("atomic_cross_emitter_frame_snapshot_verified"),false);
    return O;
}

TSharedPtr<FJsonObject> HandleRuntime(const TSharedPtr<FJsonObject>& Params)
{
    FWork& W=*Current;if(!W.CaptureWorld())return W.Error(W.R.Error);UWorld* World=W.World.Get();const bool bPlaySessionActive=GEditor->GetPIEWorldContext()!=nullptr;const bool bUsingPlayWorld=GEditor->GetPIEWorldContext()&&GEditor->GetPIEWorldContext()->World()==World;
    TSharedPtr<FJsonObject> EffectiveParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();
    const FString ActorNameFilter = GetStringParam(EffectiveParams, TEXT("actor_name"), FString());
    const FString ActorLabelFilter = GetStringParam(EffectiveParams, TEXT("actor_label"), FString());
    const FString ComponentNameFilter = GetStringParam(EffectiveParams, TEXT("component_name"), FString());
    const FString SystemPathFilter = GetStringParam(EffectiveParams, TEXT("system_path"), FString());
    const FString ParameterNameContains = GetStringParam(EffectiveParams, TEXT("parameter_name_contains"), FString());
    const bool bIncludeParameterValues = GetBoolParam(EffectiveParams, TEXT("include_parameter_values"), true);
    const bool bIncludeRawBytes = GetBoolParam(EffectiveParams, TEXT("include_raw_bytes"), false);
    const bool bIncludeAssetExposedParameters = GetBoolParam(EffectiveParams, TEXT("include_asset_exposed_parameters"), false);
    const bool bIncludeInstanceParameters = GetBoolParam(EffectiveParams, TEXT("include_instance_parameters"), false);
    const bool bIncludeParameterObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_parameter_object_properties"), false);
    const bool bIncludeEmitterRuntime = GetBoolParam(EffectiveParams, TEXT("include_emitter_runtime"), true);
    const int32 MaxComponents = BoundInt(EffectiveParams,TEXT("max_components"),32.0,1,512);
    const int32 MaxParameters = BoundInt(EffectiveParams,TEXT("max_parameters"),256.0,0,4096);
    const int32 MaxEmitterInstances = BoundInt(EffectiveParams,TEXT("max_emitter_instances"),128.0,0,256);
    const int32 MaxRawBytes = BoundInt(EffectiveParams,TEXT("max_raw_bytes"),64.0,0,4096);
    const int32 MaxObjectProperties = BoundInt(EffectiveParams,TEXT("max_object_properties"),80.0,0,512);
    const int32 MaxArrayElements = BoundInt(EffectiveParams,TEXT("max_array_elements"),16.0,0,256);
    const int32 MaxPropertyValueChars = BoundInt(EffectiveParams,TEXT("max_property_value_chars"),512.0,0,16384);

    TArray<AActor*> AllActors;
    if(!W.Admit(TEXT("runtime_numeric_admission"))||!W.Read(TEXT("GetAllActors"),[&]{UGameplayStatics::GetAllActorsOfClass(World,AActor::StaticClass(),AllActors);})||!W.Budget(AllActors.Num()))return W.Error(W.R.Error);
    for(auto* A:AllActors)if(A&&!W.Pin(A))return W.Error(W.R.Error);

    int32 ScannedNiagaraComponentCount = 0;
    int32 ReturnedNiagaraComponentCount = 0;
    bool bTruncated = false;
    bool bInstanceSnapshotsComplete = true;int32 InstanceSnapshotsAvailable=0;bool bEmitterSnapshotsComplete=true;int32 EmitterSnapshotsAvailable=0;
    TArray<TSharedPtr<FJsonValue>> ComponentResults;

    for (AActor* Actor : AllActors)
    {
        if(!W.Admit(TEXT("runtime_actor")))return W.Error(W.R.Error);
        if (!Actor)
        {
            continue;
        }

        const FString ActorLabel = Actor->GetActorLabel();
        const FString ActorName = Actor->GetName();
        const FString ActorPath = Actor->GetPathName();if(!W.Budget(0,ActorName.Len()+ActorLabel.Len()+ActorPath.Len()))return W.Error(W.R.Error);
        const FString ActorSearchText = FString::Printf(TEXT("%s|%s|%s"), *ActorName, *ActorLabel, *ActorPath);
        if (!NiagaraRuntimeStateMatchesFilter(ActorSearchText, ActorNameFilter))
        {
            continue;
        }
        if (!NiagaraRuntimeStateMatchesFilter(ActorLabel, ActorLabelFilter))
        {
            continue;
        }

        TArray<UNiagaraComponent*> NiagaraComponents;
        if(!W.Read(TEXT("Actor_GetNiagaraComponents"),[&]{Actor->GetComponents<UNiagaraComponent>(NiagaraComponents);})||!W.Budget(NiagaraComponents.Num()))return W.Error(W.R.Error);
        for(auto* C:NiagaraComponents)if(C&&!W.Component(C))return W.Error(W.R.Error);
        for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
        {
            if(!W.Admit(TEXT("runtime_component")))return W.Error(W.R.Error);
            if (!NiagaraComponent)
            {
                continue;
            }

            ++ScannedNiagaraComponentCount;
            const FString ComponentName = NiagaraComponent->GetName();
            const FString ComponentPath = NiagaraComponent->GetPathName();
            const FString ComponentSearchText = FString::Printf(TEXT("%s|%s"), *ComponentName, *ComponentPath);
            if (!NiagaraRuntimeStateMatchesFilter(ComponentSearchText, ComponentNameFilter))
            {
                continue;
            }

            UNiagaraSystem* Asset = NiagaraComponent->GetAsset();
            const FString AssetPath = NiagaraRuntimeStateObjectPath(Asset);
            if (!NiagaraRuntimeStateMatchesFilter(AssetPath, SystemPathFilter))
            {
                continue;
            }

            if (ReturnedNiagaraComponentCount >= MaxComponents)
            {
                bTruncated = true;
                continue;
            }

            TSharedPtr<FJsonObject> ComponentObject=MakeShared<FJsonObject>();W.Prefix.Add(MakeShared<FJsonValueObject>(ComponentObject));
            ComponentObject->SetStringField(TEXT("actor_name"), ActorName);
            ComponentObject->SetStringField(TEXT("actor_label"), ActorLabel);
            ComponentObject->SetStringField(TEXT("actor_path"), ActorPath);
            ComponentObject->SetStringField(TEXT("component_name"), ComponentName);
            ComponentObject->SetStringField(TEXT("component_path"), ComponentPath);
            ComponentObject->SetStringField(TEXT("component_class"), NiagaraRuntimeStateClassName(NiagaraComponent));
            ComponentObject->SetBoolField(TEXT("registered"), NiagaraComponent->IsRegistered());
            ComponentObject->SetBoolField(TEXT("active"), NiagaraComponent->IsActive());
            ComponentObject->SetBoolField(TEXT("visible"), NiagaraComponent->IsVisible());
            ComponentObject->SetBoolField(TEXT("component_tick_enabled"), NiagaraComponent->IsComponentTickEnabled());
            ENiagaraExecutionState Execution,Requested;if(!W.Read(TEXT("Component_ExecutionState"),[&]{Execution=NiagaraComponent->GetExecutionState();})||!W.Read(TEXT("Component_RequestedExecutionState"),[&]{Requested=NiagaraComponent->GetRequestedExecutionState();}))return W.Error(W.R.Error);ComponentObject->SetStringField(TEXT("execution_state"),NiagaraExecutionStateToString(Execution));ComponentObject->SetStringField(TEXT("requested_execution_state"),NiagaraExecutionStateToString(Requested));
            ComponentObject->SetStringField(TEXT("asset_path"), AssetPath);
            ComponentObject->SetStringField(TEXT("asset_class"), NiagaraRuntimeStateClassName(Asset));
            FBox FixedBounds;if(!W.Read(TEXT("Component_FixedBounds"),[&]{FixedBounds=NiagaraComponent->GetSystemFixedBounds();}))return W.Error(W.R.Error);AddBoxField(ComponentObject,TEXT("component_system_fixed_bounds"),FixedBounds);
            if (Asset)
            {
                ComponentObject->SetBoolField(TEXT("asset_fixed_bounds_enabled"), Asset->bFixedBounds);
                AddBoxField(ComponentObject, TEXT("asset_fixed_bounds"), Asset->GetFixedBounds());if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);
            }
            if (USceneComponent* AttachParent = NiagaraComponent->GetAttachParent())
            {
                ComponentObject->SetStringField(TEXT("attach_parent_path"), AttachParent->GetPathName());
            }
            AddVectorField(ComponentObject, TEXT("world_location"), NiagaraComponent->GetComponentLocation());if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);
            AddRotatorField(ComponentObject, TEXT("world_rotation"), NiagaraComponent->GetComponentRotation());if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);
            AddVectorField(ComponentObject, TEXT("world_scale"), NiagaraComponent->GetComponentScale());if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);
            AddVectorField(ComponentObject, TEXT("relative_location"), NiagaraComponent->GetRelativeLocation());if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);
            AddRotatorField(ComponentObject, TEXT("relative_rotation"), NiagaraComponent->GetRelativeRotation());if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);
            AddVectorField(ComponentObject, TEXT("relative_scale"), NiagaraComponent->GetRelativeScale3D());if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);

            const FBoxSphereBounds& Bounds = NiagaraComponent->Bounds;
            AddVectorField(ComponentObject, TEXT("bounds_origin"), Bounds.Origin);if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);
            AddVectorField(ComponentObject, TEXT("bounds_extent"), Bounds.BoxExtent);if(!W.Admit(TEXT("component_numeric_projection")))return W.Error(W.R.Error);
            if(!FMath::IsFinite(Bounds.SphereRadius))return W.Error(TEXT("Non-finite observed bounds radius is unavailable"));ComponentObject->SetNumberField(TEXT("bounds_sphere_radius"),Bounds.SphereRadius);

            if (bIncludeEmitterRuntime)
            {
                auto EmitterRuntime=BuildNiagaraRuntimeStateEmitterInstances(NiagaraComponent,MaxEmitterInstances);
                ComponentObject->SetObjectField(TEXT("emitter_runtime"),EmitterRuntime);
                if(!W.Admit(TEXT("after_emitter_availability")))return W.Error(W.R.Error);
                bool Available=false,Complete=false;EmitterRuntime->TryGetBoolField(TEXT("runtime_counters_available"),Available);EmitterRuntime->TryGetBoolField(TEXT("runtime_snapshot_complete"),Complete);
                if(Available)++EmitterSnapshotsAvailable;bEmitterSnapshotsComplete&=Complete;
            }

            AddNiagaraRuntimeStateStore(
                ComponentObject,
                TEXT("override_parameters"),
                NiagaraComponent->GetOverrideParameters(),
                ParameterNameContains,
                MaxParameters,
                bIncludeParameterValues,
                bIncludeRawBytes,
                MaxRawBytes,
                bIncludeParameterObjectProperties,
                MaxObjectProperties,
                MaxArrayElements,
                MaxPropertyValueChars);

            if(!W.Admit(TEXT("after_override_snapshot_before_instance_status")))return W.Error(W.R.Error);
            if (bIncludeInstanceParameters)
            {
                TSharedPtr<FJsonObject> InstanceParameterStatus = MakeShared<FJsonObject>();
                InstanceParameterStatus->SetBoolField(TEXT("requested"), true);
                const FNiagaraSystemInstanceControllerPtr Controller = NiagaraComponent->GetSystemInstanceController();
                InstanceParameterStatus->SetBoolField(TEXT("has_system_instance_controller"), Controller.IsValid());
                InstanceParameterStatus->SetBoolField(TEXT("system_instance_controller_valid"), Controller.IsValid() && Controller->IsValid());

                InstanceParameterStatus->SetBoolField(TEXT("has_system_instance"),Controller&&Controller->IsValid());
                FNiagaraSystemInstance* Solo=nullptr;FString Reason;
                if(!CapturePausedSolo(NiagaraComponent,Solo,Reason,true))return W.Error(W.R.Error);
                InstanceParameterStatus->SetBoolField(TEXT("available"),Solo!=nullptr);
                InstanceParameterStatus->SetBoolField(TEXT("already_paused_solo"),Solo&&Controller->IsSolo());
                InstanceParameterStatus->SetBoolField(TEXT("already_paused_shared"),Solo&&!Controller->IsSolo());
                InstanceParameterStatus->SetBoolField(TEXT("already_paused_instance"),Solo!=nullptr);
                InstanceParameterStatus->SetStringField(TEXT("unavailable_reason"),Reason);
                InstanceParameterStatus->SetBoolField(TEXT("observer_changed_runtime_state"),false);
                ComponentObject->SetObjectField(TEXT("instance_parameter_status"),InstanceParameterStatus);
                if(Solo)
                {
                    AddNiagaraRuntimeStateStore(ComponentObject,TEXT("instance_parameters"),Solo->GetInstanceParameters(),ParameterNameContains,MaxParameters,bIncludeParameterValues,bIncludeRawBytes,MaxRawBytes,bIncludeParameterObjectProperties,MaxObjectProperties,MaxArrayElements,MaxPropertyValueChars);
                    if(!W.Admit(TEXT("after_solo_instance_store")))return W.Error(W.R.Error);++InstanceSnapshotsAvailable;
                }
                else bInstanceSnapshotsComplete=false;
            }

            if(!W.Admit(TEXT("after_override_snapshot")))return W.Error(W.R.Error);
            if (bIncludeAssetExposedParameters && Asset)
            {
                AddNiagaraRuntimeStateStore(
                    ComponentObject,
                    TEXT("asset_exposed_parameters"),
                    Asset->GetExposedParameters(),
                    ParameterNameContains,
                    MaxParameters,
                    bIncludeParameterValues,
                    bIncludeRawBytes,
                    MaxRawBytes,
                    bIncludeParameterObjectProperties,
                    MaxObjectProperties,
                    MaxArrayElements,
                    MaxPropertyValueChars);
            }

            if(!W.Admit(TEXT("runtime_component_finished")))return W.Error(W.R.Error);
            ComponentResults.Add(MakeShared<FJsonValueObject>(ComponentObject));
            ++ReturnedNiagaraComponentCount;
        }
    }

    if(!W.Admit(TEXT("runtime_result")))return W.Error(W.R.Error);
    const FString CurrentMapPackage = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
    const bool bMapDirty = World->GetOutermost() ? World->GetOutermost()->IsDirty() : false;

    TSharedPtr<FJsonObject> Filters = MakeShared<FJsonObject>();
    Filters->SetStringField(TEXT("actor_name"), ActorNameFilter);
    Filters->SetStringField(TEXT("actor_label"), ActorLabelFilter);
    Filters->SetStringField(TEXT("component_name"), ComponentNameFilter);
    Filters->SetStringField(TEXT("system_path"), SystemPathFilter);
    Filters->SetStringField(TEXT("parameter_name_contains"), ParameterNameContains);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("sampled_world_type"), bUsingPlayWorld ? TEXT("play") : TEXT("editor"));
    ResultObj->SetBoolField(TEXT("is_play_session_active"), bPlaySessionActive);
    ResultObj->SetStringField(TEXT("current_map_package"), CurrentMapPackage);
    ResultObj->SetBoolField(TEXT("map_dirty"), bMapDirty);
    ResultObj->SetObjectField(TEXT("filters"), Filters);
    ResultObj->SetNumberField(TEXT("actor_count_scanned"), AllActors.Num());
    ResultObj->SetNumberField(TEXT("niagara_component_count_scanned"), ScannedNiagaraComponentCount);
    ResultObj->SetNumberField(TEXT("niagara_component_count"), ReturnedNiagaraComponentCount);
    ResultObj->SetNumberField(TEXT("max_components"), MaxComponents);
    ResultObj->SetNumberField(TEXT("max_parameters"), MaxParameters);
    ResultObj->SetBoolField(TEXT("include_parameter_values"), bIncludeParameterValues);
    ResultObj->SetBoolField(TEXT("include_raw_bytes"), bIncludeRawBytes);
    ResultObj->SetBoolField(TEXT("include_asset_exposed_parameters"), bIncludeAssetExposedParameters);
    ResultObj->SetBoolField(TEXT("include_instance_parameters"), bIncludeInstanceParameters);
    ResultObj->SetBoolField(TEXT("include_parameter_object_properties"), bIncludeParameterObjectProperties);
    ResultObj->SetBoolField(TEXT("include_emitter_runtime"), bIncludeEmitterRuntime);
    ResultObj->SetNumberField(TEXT("max_emitter_instances"), MaxEmitterInstances);
    ResultObj->SetNumberField(TEXT("max_object_properties"), MaxObjectProperties);
    ResultObj->SetNumberField(TEXT("max_array_elements"), MaxArrayElements);
    ResultObj->SetNumberField(TEXT("max_property_value_chars"), MaxPropertyValueChars);
    ResultObj->SetBoolField(TEXT("truncated"), bTruncated);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetBoolField(TEXT("loads_or_saves_packages"),false);ResultObj->SetStringField(TEXT("loads_or_saves_packages_scope"),TEXT("Explicit loader/saver calls only; SDK getter/export indirect effects are unknown."));
    ResultObj->SetArrayField(TEXT("components"), ComponentResults);
    ResultObj->SetStringField(TEXT("safety_note"), TEXT("Inspects supported public component/owned stores and actual already-paused Solo/shared InstanceParameters/system-emitter counters when requested. CPU particle counts are current dataset samples; GPU counts retain SDK latent/estimate semantics, not GPU completion. Explicit create/Modify/load/save/reload/compile and observer Pause/tick/wait/finalize calls are zero; running/nonpaused-complete/stateless-specific internals and SDK global effects remain incomplete."));
    ResultObj->SetNumberField(TEXT("instance_parameter_snapshots_available"),InstanceSnapshotsAvailable);const bool InstanceComplete=!bIncludeInstanceParameters||(bInstanceSnapshotsComplete&&InstanceSnapshotsAvailable>0);const bool EmitterComplete=!bIncludeEmitterRuntime||(bEmitterSnapshotsComplete&&EmitterSnapshotsAvailable>0);ResultObj->SetNumberField(TEXT("emitter_runtime_snapshots_available"),EmitterSnapshotsAvailable);ResultObj->SetBoolField(TEXT("requested_emitter_runtime_snapshot_complete"),EmitterComplete);ResultObj->SetBoolField(TEXT("partial_observation"),!InstanceComplete||!EmitterComplete);ResultObj->SetBoolField(TEXT("requested_internal_instance_snapshot_complete"),InstanceComplete&&EmitterComplete);ResultObj->SetStringField(TEXT("observation_scope"),TEXT("Public GT component and owned stores, plus actual InstanceParameters/system-emitter rows copied only from already-paused Solo/shared instances. Original emitter order/limit/truncation and returned-row sums are retained. Running and nonpaused-complete instances and stateless-specific counters remain incomplete. No observer state change/wait, atomic cross-store/frame or GPU completion claim."));W.R.Data->SetBoolField(TEXT("typed_observation_complete"),true);return ResultObj;
}
TSharedPtr<FJsonObject> HandleParameterStore(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> StoreParams = Params.IsValid() ? MakeShared<FJsonObject>(*Params) : MakeShared<FJsonObject>();
    StoreParams->SetBoolField(TEXT("include_parameter_values"), true);
    StoreParams->SetBoolField(TEXT("include_instance_parameters"), true);
    StoreParams->SetBoolField(TEXT("include_asset_exposed_parameters"), false);
    StoreParams->SetBoolField(TEXT("include_parameter_object_properties"), false);
    StoreParams->SetBoolField(TEXT("include_emitter_runtime"), false);
    if (!StoreParams->HasField(TEXT("max_components")))
    {
        StoreParams->SetNumberField(TEXT("max_components"), 8);
    }
    if (!StoreParams->HasField(TEXT("max_parameters")))
    {
        StoreParams->SetNumberField(TEXT("max_parameters"), 64);
    }
    if (!StoreParams->HasField(TEXT("max_emitter_instances")))
    {
        StoreParams->SetNumberField(TEXT("max_emitter_instances"), 0);
    }

    TSharedPtr<FJsonObject> Result = HandleRuntime(StoreParams);
    if (Result.IsValid())
    {
        Result->SetStringField(TEXT("inspection_mode"), TEXT("parameter_store"));
        Result->SetStringField(TEXT("parameter_store_note"), TEXT("Compares owned OverrideParameters with actual InstanceParameters when the existing controller owns an already-paused Solo/shared instance. Running and nonpaused-complete instances remain unavailable; no Pause, ForceSolo, wait or finalize is performed. GPU CombinedParamStore is not read by this command. Forced flags/defaults are preserved."));
    }
    return Result;
}

struct FTNGridUsage
{
 FString Name;TStrongObjectPtr<UNiagaraDataInterface> Hold{nullptr};
 FTNGridUsage(const FString& N,UNiagaraDataInterface* D):Name(N),Hold(D){}
};
bool BuildPausedSoloGrid(UNiagaraComponent* Component,bool IncludeInternal,int32 Limit,TArray<TSharedPtr<FJsonValue>>& Values,int32& Total,bool& Truncated,bool& Complete,FString& Reason)
{
 auto& W=*Current;FNiagaraSystemInstance* Solo=nullptr;
 if(!CapturePausedSolo(Component,Solo,Reason))return false;Complete=Solo!=nullptr;
 TArray<TUniquePtr<FTNGridUsage>> Usages;
 if(Solo)
 {
  // No arbitrary observer/callback is invoked while SDK-owned stores are enumerated.
  // The callback takes immediate GC holds; all projection follows the return fence.
  bool Overflow=false;if(!W.SDK(TEXT("Solo_ForEachDataInterface")))return false;
  FNiagaraDataInterfaceUtilities::ForEachDataInterface(Solo,[&](const FNiagaraDataInterfaceUtilities::FDataInterfaceUsageContext& Usage)->bool
  {
   if(!IncludeInternal&&Usage.Variable.IsInNameSpace(FNiagaraConstants::InternalNamespaceString))return true;
   if(Usages.Num()>=65536){Overflow=true;return false;}
   Usages.Add(MakeUnique<FTNGridUsage>(Usage.Variable.GetName().ToString(),Usage.DataInterface));return true;
  });
  if(!W.After(TEXT("Solo_ForEachDataInterface")))return false;
  if(Overflow||!W.Budget(Usages.Num()))return W.Fail(TEXT("niagara_runtime_interface_inventory_budget"),TEXT("Actual DI usage inventory exceeds 65536 copied entries; no truncation is reported as complete."));
 }
 else
 {
  // Preserve GO's explicitly partial override metadata fallback, not a live snapshot.
  FStoreSnapshot Snapshot;if(!CaptureStore(Component->GetOverrideParameters(),Snapshot))return false;
  for(const auto& Variable:Snapshot.Variables)
  {
   if(!Variable.IsDataInterface()||(!IncludeInternal&&Variable.IsInNameSpace(FNiagaraConstants::InternalNamespaceString)))continue;
   if(!Snapshot.Valid(Variable))return false;
   Usages.Add(MakeUnique<FTNGridUsage>(Variable.GetName().ToString(),Snapshot.GetDataInterface(Variable.Offset)));
  }
 }
 for(const auto& Usage:Usages)
 {
  if(!W.Admit(TEXT("grid_usage_projection")))return false;auto* DI=Usage->Hold.Get();
  if(!DI)continue;if(!W.Pin(DI))return false;
  auto* Grid=Cast<UNiagaraDataInterfaceGrid2D>(DI);if(!Grid)continue;
  if(Values.Num()>=Limit){Truncated=true;continue;}
  if(!W.Budget(1,Usage->Name.Len()))return false;
  auto Row=MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("variable_name"),Usage->Name);
  Row->SetStringField(TEXT("data_interface_class"),NiagaraRuntimeStateClassName(Grid));
  Row->SetStringField(TEXT("data_interface_path"),Grid->GetPathName());
  const bool Collection=Grid->IsA<UNiagaraDataInterfaceGrid2DCollection>();
  Row->SetBoolField(TEXT("is_grid2d_collection"),Collection);
  Row->SetNumberField(TEXT("asset_num_cells_x"),Grid->NumCellsX);Row->SetNumberField(TEXT("asset_num_cells_y"),Grid->NumCellsY);
  Row->SetNumberField(TEXT("asset_num_cells_max_axis"),Grid->NumCellsMaxAxis);Row->SetBoolField(TEXT("asset_set_grid_from_max_axis"),Grid->SetGridFromMaxAxis);
  if(!FMath::IsFinite(Grid->WorldBBoxSize.X)||!FMath::IsFinite(Grid->WorldBBoxSize.Y))return W.Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Grid asset bounds are nonfinite; no numeric zero substitution."));
  Row->SetArrayField(TEXT("asset_world_bbox_size"),NumberArrayToJson({Grid->WorldBBoxSize.X,Grid->WorldBBoxSize.Y}));
  bool HasData=false;
  if(Solo&&Collection&&Grid->GetClass()==UNiagaraDataInterfaceGrid2DCollection::StaticClass())
  {
   if(!W.SDK(TEXT("Solo_Grid2DInstanceData")))return false;
   // Exact native DI determines the instance-data layout. Never keep this pointer
   // over a callback; copy scalar GT values before any post-return observation.
   const auto* Data=Solo->FindTypedDataInterfaceInstanceData<FGrid2DCollectionRWInstanceData_GameThread>(DI);
   FIntPoint Cells=FIntPoint::ZeroValue;int32 Attributes=0;FVector2D CellSize=FVector2D::ZeroVector,Box=FVector2D::ZeroVector;bool Clear=false,Realloc=false;
   HasData=Data!=nullptr;
   if(Data){Cells=Data->NumCells;Attributes=Data->NumAttributes;CellSize=Data->CellSize;Box=Data->WorldBBoxSize;Clear=Data->ClearBeforeNonIterationStage;Realloc=Data->NeedsRealloc;}
   if(!W.After(TEXT("Solo_Grid2DInstanceData")))return false;
   // Reacquire after the return fence; stale instance-data bytes are never read.
   const auto* Fresh=Solo->FindTypedDataInterfaceInstanceData<FGrid2DCollectionRWInstanceData_GameThread>(DI);
   if(Fresh!=Data)return W.Fail(TEXT("niagara_runtime_grid_instance_data_changed"),TEXT("Typed DI instance-data slot changed after copy; no stale data getter or retry."));
   if(!HasData){Complete=false;Reason=TEXT("Exact native collection has no initialized typed instance-data slot; observer does not initialize or synthesize values.");Row->SetStringField(TEXT("runtime_unavailable_reason"),Reason);}
   if(HasData)
   {
    if(!FMath::IsFinite(CellSize.X)||!FMath::IsFinite(CellSize.Y)||!FMath::IsFinite(Box.X)||!FMath::IsFinite(Box.Y))return W.Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Observed Grid runtime dimensions are nonfinite; not numeric zero."));
    Row->SetArrayField(TEXT("runtime_num_cells"),NumberArrayToJson({double(Cells.X),double(Cells.Y)}));Row->SetNumberField(TEXT("runtime_num_attributes"),Attributes);
    Row->SetArrayField(TEXT("runtime_cell_size"),NumberArrayToJson({CellSize.X,CellSize.Y}));Row->SetArrayField(TEXT("runtime_world_bbox_size"),NumberArrayToJson({Box.X,Box.Y}));
    Row->SetBoolField(TEXT("runtime_clear_before_non_iteration_stage"),Clear);Row->SetBoolField(TEXT("runtime_needs_realloc"),Realloc);
   }
  }
  else if(Solo&&Collection)
  {
   Complete=false;Reason=TEXT("Derived Grid2D collection instance-data layout remains incomplete.");Row->SetStringField(TEXT("runtime_unavailable_reason"),TEXT("Derived Grid2D collection instance-data layout is not proven by the exact native collection role; valid wider scope remains incomplete."));
  }
  if(Collection)Row->SetBoolField(TEXT("has_runtime_instance_data"),HasData);
  Row->SetBoolField(TEXT("runtime_instance_data_available"),HasData);
  Row->SetBoolField(TEXT("already_paused_solo"),Solo!=nullptr);
  Row->SetStringField(TEXT("observation_scope"),Solo?TEXT("Copied already-paused Solo GT data; no GPU contents or frame freshness proof."):TEXT("Partial component override metadata only; no current instance bytes."));
  Values.Add(MakeShared<FJsonValueObject>(Row));W.Prefix.Add(MakeShared<FJsonValueObject>(Row));++Total;
  if(!W.Admit(TEXT("after_grid_row")))return false;
 }
 return W.Admit(TEXT("after_grid_usage_snapshot"));
}

TSharedPtr<FJsonObject> HandleGrid(const TSharedPtr<FJsonObject>& Params)
{
    FWork& W=*Current;if(!W.CaptureWorld())return W.Error(W.R.Error);UWorld* World=W.World.Get();const bool bUsingPlayWorld=GEditor->GetPIEWorldContext()&&GEditor->GetPIEWorldContext()->World()==World;
    TSharedPtr<FJsonObject> EffectiveParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();
    const FString ActorNameFilter = GetStringParam(EffectiveParams, TEXT("actor_name"), FString());
    const FString ActorLabelFilter = GetStringParam(EffectiveParams, TEXT("actor_label"), FString());
    const FString ComponentNameFilter = GetStringParam(EffectiveParams, TEXT("component_name"), FString());
    const FString SystemPathFilter = GetStringParam(EffectiveParams, TEXT("system_path"), FString());
    const int32 MaxComponents = BoundInt(EffectiveParams,TEXT("max_components"),16.0,1,256);
    const int32 MaxGridInterfaces = BoundInt(EffectiveParams,TEXT("max_grid_interfaces"),16.0,1,256);
    const bool bIncludeInternal = GetBoolParam(EffectiveParams, TEXT("include_internal"), true);

    TArray<AActor*> AllActors;
    if(!W.Admit(TEXT("grid_numeric_admission"))||!W.Read(TEXT("Grid_GetAllActors"),[&]{UGameplayStatics::GetAllActorsOfClass(World,AActor::StaticClass(),AllActors);})||!W.Budget(AllActors.Num()))return W.Error(W.R.Error);for(auto* A:AllActors)if(A&&!W.Pin(A))return W.Error(W.R.Error);

    int32 ScannedNiagaraComponentCount = 0;
    int32 ReturnedNiagaraComponentCount = 0;
    int32 TotalGridInterfaceCount = 0;
    bool bComponentsTruncated = false;
    bool bGridInterfacesTruncated = false;
    bool AllGridSnapshotsComplete=true;int32 GridSnapshotsAvailable=0;
    TArray<TSharedPtr<FJsonValue>> ComponentResults;



    for (AActor* Actor : AllActors)
    {
        if(!W.Admit(TEXT("grid_actor")))return W.Error(W.R.Error);
        if (!Actor)
        {
            continue;
        }

        const FString ActorLabel = Actor->GetActorLabel();
        const FString ActorName = Actor->GetName();
        const FString ActorPath = Actor->GetPathName();
        const FString ActorSearchText = FString::Printf(TEXT("%s|%s|%s"), *ActorName, *ActorLabel, *ActorPath);
        if (!NiagaraRuntimeStateMatchesFilter(ActorSearchText, ActorNameFilter) ||
            !NiagaraRuntimeStateMatchesFilter(ActorLabel, ActorLabelFilter))
        {
            continue;
        }

        TArray<UNiagaraComponent*> NiagaraComponents;
        if(!W.Read(TEXT("Grid_GetComponents"),[&]{Actor->GetComponents<UNiagaraComponent>(NiagaraComponents);})||!W.Budget(NiagaraComponents.Num()))return W.Error(W.R.Error);for(auto* C:NiagaraComponents)if(C&&!W.Component(C))return W.Error(W.R.Error);
        for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
        {
            if(!W.Admit(TEXT("grid_component")))return W.Error(W.R.Error);
            if (!NiagaraComponent)
            {
                continue;
            }

            ++ScannedNiagaraComponentCount;
            const FString ComponentName = NiagaraComponent->GetName();
            const FString ComponentPath = NiagaraComponent->GetPathName();
            const FString ComponentSearchText = FString::Printf(TEXT("%s|%s"), *ComponentName, *ComponentPath);
            if (!NiagaraRuntimeStateMatchesFilter(ComponentSearchText, ComponentNameFilter))
            {
                continue;
            }

            UNiagaraSystem* Asset = NiagaraComponent->GetAsset();
            const FString AssetPath = NiagaraRuntimeStateObjectPath(Asset);
            if (!NiagaraRuntimeStateMatchesFilter(AssetPath, SystemPathFilter))
            {
                continue;
            }

            if (ReturnedNiagaraComponentCount >= MaxComponents)
            {
                bComponentsTruncated = true;
                continue;
            }

            const FNiagaraSystemInstanceControllerPtr Controller = NiagaraComponent->GetSystemInstanceController();
            const bool HasSystemInstance=Controller&&Controller->IsValid();
            TArray<TSharedPtr<FJsonValue>> GridValues;bool GridComplete=false;FString GridReason;
            if(!BuildPausedSoloGrid(NiagaraComponent,bIncludeInternal,MaxGridInterfaces,GridValues,TotalGridInterfaceCount,bGridInterfacesTruncated,GridComplete,GridReason))return W.Error(W.R.Error);
            AllGridSnapshotsComplete &= GridComplete;if(GridComplete)++GridSnapshotsAvailable;
            TSharedPtr<FJsonObject> ComponentObject = MakeShared<FJsonObject>();
            ComponentObject->SetStringField(TEXT("actor_name"), ActorName);
            ComponentObject->SetStringField(TEXT("actor_label"), ActorLabel);
            ComponentObject->SetStringField(TEXT("actor_path"), ActorPath);
            ComponentObject->SetStringField(TEXT("component_name"), ComponentName);
            ComponentObject->SetStringField(TEXT("component_path"), ComponentPath);
            ComponentObject->SetStringField(TEXT("asset_path"), AssetPath);
            ComponentObject->SetBoolField(TEXT("active"), NiagaraComponent->IsActive());
            ComponentObject->SetBoolField(TEXT("registered"), NiagaraComponent->IsRegistered());
            ComponentObject->SetBoolField(TEXT("has_system_instance_controller"), Controller.IsValid());
            ComponentObject->SetBoolField(TEXT("has_system_instance"), HasSystemInstance);
            ComponentObject->SetNumberField(TEXT("grid2d_interface_count"), GridValues.Num());
            ComponentObject->SetArrayField(TEXT("grid2d_interfaces"), GridValues);
            ComponentObject->SetBoolField(TEXT("runtime_observation_available"),GridComplete);
            ComponentObject->SetStringField(TEXT("runtime_unavailable_reason"),GridReason);
            ComponentObject->SetBoolField(TEXT("runtime_instance_snapshot_complete"),GridComplete);
            ComponentObject->SetBoolField(TEXT("include_internal_filter_equivalent"),true);
            ComponentResults.Add(MakeShared<FJsonValueObject>(ComponentObject));
            ++ReturnedNiagaraComponentCount;
        }
    }

    if(!W.Admit(TEXT("grid_result")))return W.Error(W.R.Error);TSharedPtr<FJsonObject> ResultObj=MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetStringField(TEXT("world_kind"), bUsingPlayWorld ? TEXT("play_world") : TEXT("editor_world"));
    ResultObj->SetStringField(TEXT("world_name"), World->GetName());
    ResultObj->SetNumberField(TEXT("actor_count_scanned"), AllActors.Num());
    ResultObj->SetNumberField(TEXT("niagara_component_count_scanned"), ScannedNiagaraComponentCount);
    ResultObj->SetNumberField(TEXT("niagara_component_count"), ReturnedNiagaraComponentCount);
    ResultObj->SetNumberField(TEXT("grid2d_interface_count"), TotalGridInterfaceCount);
    ResultObj->SetBoolField(TEXT("components_truncated"), bComponentsTruncated);
    ResultObj->SetBoolField(TEXT("grid_interfaces_truncated"), bGridInterfacesTruncated);
    ResultObj->SetBoolField(TEXT("include_internal"), bIncludeInternal);
    ResultObj->SetArrayField(TEXT("components"), ComponentResults);
    ResultObj->SetStringField(TEXT("safety_note"), TEXT("Already-paused Solo GT instance data only; running Solo/non-Solo remain unavailable with partial override metadata. No observer Pause/ForceSolo/tick/wait/finalize/setter/compile/save or GPU pixel readback."));
    const bool GridComplete=AllGridSnapshotsComplete&&GridSnapshotsAvailable>0;ResultObj->SetNumberField(TEXT("runtime_snapshots_available"),GridSnapshotsAvailable);ResultObj->SetBoolField(TEXT("partial_observation"),!GridComplete);ResultObj->SetBoolField(TEXT("runtime_instance_snapshot_complete"),GridComplete);ResultObj->SetBoolField(TEXT("include_internal_filter_equivalent"),true);ResultObj->SetStringField(TEXT("enumeration_scope"),TEXT("Already-paused Solo: original ordered/multiplicity-preserving override/system/emitter/GPU parameter-store DI enumeration, with the original Internal namespace filter. Exact native Grid2DCollection GT scalar data only; derived collection layout, running Solo/non-Solo and GPU contents remain incomplete. Otherwise GO override metadata fallback is explicitly partial."));W.R.Data->SetBoolField(TEXT("typed_observation_complete"),true);return ResultObj;
}

TSharedPtr<FJsonObject> HandleRT(const TSharedPtr<FJsonObject>& Params)
{
    FWork& W=*Current;if(!W.CaptureWorld())return W.Error(W.R.Error);
    TSharedPtr<FJsonObject> EffectiveParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();

    FString RenderTargetPath = GetStringParam(EffectiveParams, TEXT("render_target_path"), FString());
    if (RenderTargetPath.TrimStartAndEnd().IsEmpty())
    {
        return W.Error(TEXT("Missing 'render_target_path' parameter"));
    }

    FString RenderTargetObjectPath = RenderTargetPath.TrimStartAndEnd();
    if (!RenderTargetObjectPath.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(RenderTargetObjectPath))
    {
        RenderTargetObjectPath = FString::Printf(TEXT("%s.%s"), *RenderTargetObjectPath, *FPackageName::GetShortName(RenderTargetObjectPath));
    }

    if(!W.SDK(TEXT("Load_RenderTarget")))return W.Error(W.R.Error);UTextureRenderTarget2D* RenderTarget=LoadObject<UTextureRenderTarget2D>(nullptr,*RenderTargetObjectPath,nullptr,LOAD_None,nullptr);W.R.Data->SetBoolField(TEXT("render_target_load_returned_nonnull"),RenderTarget!=nullptr);if(!W.After(TEXT("Load_RenderTarget")))return W.Error(W.R.Error);if(RenderTarget&&(!W.Pin(RenderTarget)||!RenderTarget->GetPathName().Equals(RenderTargetObjectPath,ESearchCase::CaseSensitive)))return W.Error(TEXT("Render target returned an unexpected exact path/class identity"));
    if (!RenderTarget)
    {
        return W.Error(FString::Printf(TEXT("Failed to load TextureRenderTarget2D: %s"), *RenderTargetObjectPath));
    }

    FTextureRenderTargetResource* Resource=nullptr;if(!W.Read(TEXT("GameThread_GetRenderTargetResource"),[&]{Resource=RenderTarget->GameThread_GetRenderTargetResource();}))return W.Error(W.R.Error);
    if (!Resource)
    {
        return W.Error(FString::Printf(TEXT("Render target has no game-thread resource: %s"), *RenderTargetObjectPath));
    }

    const int32 SizeX = RenderTarget->SizeX;
    const int32 SizeY = RenderTarget->SizeY;
    if (SizeX <= 0 || SizeY <= 0)
    {
        return W.Error(TEXT("Render target dimensions are invalid"));
    }

    const int64 PixelBudget=4ll*1024*1024,Product=static_cast<int64>(SizeX)*SizeY;if(Product>PixelBudget){W.Fail(TEXT("render_target_async_readback_required"),TEXT("Full RT readback exceeds4M pixels/64MiB staging budget. A domain async GPU readback job is not implemented; no synchronous read was issued."));return W.Error(W.R.Error);}W.RT=RenderTarget;W.Resource=Resource;W.SizeX=SizeX;W.SizeY=SizeY;W.Format=RenderTarget->GetFormat();if(!W.Admit(TEXT("render_resource_captured")))return W.Error(W.R.Error);
    const int32 RequestedX0=BoundInt(EffectiveParams,TEXT("x0"),0.0,MIN_int32,MAX_int32,true);
    const int32 RequestedY0 = BoundInt(EffectiveParams,TEXT("y0"),0.0,MIN_int32,MAX_int32,true);
    const int32 RequestedX1 = BoundInt(EffectiveParams,TEXT("x1"),static_cast<double>(SizeX-1),MIN_int32,MAX_int32,true);
    const int32 RequestedY1 = BoundInt(EffectiveParams,TEXT("y1"),static_cast<double>(SizeY-1),MIN_int32,MAX_int32,true);
    const int32 X0 = FMath::Clamp(FMath::Min(RequestedX0, RequestedX1), 0, SizeX - 1);
    const int32 X1 = FMath::Clamp(FMath::Max(RequestedX0, RequestedX1), 0, SizeX - 1);
    const int32 Y0 = FMath::Clamp(FMath::Min(RequestedY0, RequestedY1), 0, SizeY - 1);
    const int32 Y1 = FMath::Clamp(FMath::Max(RequestedY0, RequestedY1), 0, SizeY - 1);
    const FString Channel = GetStringParam(EffectiveParams, TEXT("channel"), TEXT("max")).ToLower();
    const double Threshold = GetNumberParam(EffectiveParams, TEXT("threshold"), 0.01);
    const int32 MaxFirstHits = FMath::Clamp(BoundInt(EffectiveParams,TEXT("max_first_hits"),8.0,MIN_int32,MAX_int32,true), 0, 128);
    const bool bHasProbe = EffectiveParams->HasField(TEXT("probe_x")) && EffectiveParams->HasField(TEXT("probe_y"));
    const int32 ProbeRadius = FMath::Max(0, BoundInt(EffectiveParams,TEXT("probe_radius"),8.0,MIN_int32,MAX_int32,true));
    const int32 ProbeX = FMath::Clamp(BoundInt(EffectiveParams,TEXT("probe_x"),0.0,MIN_int32,MAX_int32,true), 0, SizeX - 1);
    const int32 ProbeY = FMath::Clamp(BoundInt(EffectiveParams,TEXT("probe_y"),0.0,MIN_int32,MAX_int32,true), 0, SizeY - 1);
    const int32 ProbeX0 = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(ProbeX)-ProbeRadius,X0,X1));
    const int32 ProbeX1 = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(ProbeX)+ProbeRadius,X0,X1));
    const int32 ProbeY0 = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(ProbeY)-ProbeRadius,Y0,Y1));
    const int32 ProbeY1 = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(ProbeY)+ProbeRadius,Y0,Y1));

    if(!FMath::IsFinite(Threshold)||!W.Admit(TEXT("render_numeric_admission")))return W.Error(TEXT("Finite region/threshold and exact render resource required"));
    TArray<FLinearColor> Pixels;
    FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
    ReadFlags.SetLinearToGamma(false);
    bool ReadOK=false;const double ReadStart=FPlatformTime::Seconds();if(!W.Read(TEXT("ReadLinearColorPixels"),[&]{ReadOK=Resource->ReadLinearColorPixels(Pixels,ReadFlags,FIntRect(0,0,SizeX,SizeY));W.R.Data->SetBoolField(TEXT("render_readback_SDK_returned"),true);W.R.Data->SetBoolField(TEXT("render_readback_SDK_success"),ReadOK);W.R.Data->SetNumberField(TEXT("render_readback_pixel_count"),Pixels.Num());W.R.Data->SetNumberField(TEXT("render_readback_SDK_wall_seconds"),FPlatformTime::Seconds()-ReadStart);}))return W.Error(W.R.Error);
    if (!ReadOK)
    {
        return W.Error(TEXT("ReadLinearColorPixels failed"));
    }
    if (Pixels.Num() < Product)
    {
        return W.Error(FString::Printf(TEXT("Readback returned %d pixels for %dx%d render target"), Pixels.Num(), SizeX, SizeY));
    }

    auto SampleValue = [&Channel](const FLinearColor& Color) -> double
    {
        if (Channel == TEXT("r") || Channel == TEXT("red"))
        {
            return Color.R;
        }
        if (Channel == TEXT("g") || Channel == TEXT("green"))
        {
            return Color.G;
        }
        if (Channel == TEXT("b") || Channel == TEXT("blue"))
        {
            return Color.B;
        }
        if (Channel == TEXT("a") || Channel == TEXT("alpha"))
        {
            return Color.A;
        }
        if (Channel == TEXT("luma") || Channel == TEXT("luminance"))
        {
            return (0.2126 * Color.R) + (0.7152 * Color.G) + (0.0722 * Color.B);
        }
        return FMath::Max(FMath::Max(static_cast<double>(Color.R), static_cast<double>(Color.G)), FMath::Max(static_cast<double>(Color.B), static_cast<double>(Color.A)));
    };

    int32 Count = 0;
    double Sum = 0.0;
    double MaxValue = 0.0;
    double WeightedX = 0.0;
    double WeightedY = 0.0;
    int32 HitMinX = X1;
    int32 HitMaxX = X0;
    int32 HitMinY = Y1;
    int32 HitMaxY = Y0;
    int32 ProbeCount = 0;
    int32 ProbePixelCount = 0;
    double ProbeSum = 0.0;
    double ProbeMaxValue = 0.0;
    double ProbeWeightedX = 0.0;
    double ProbeWeightedY = 0.0;
    TArray<TSharedPtr<FJsonValue>> FirstHits;

    for (int32 Y = Y0; Y <= Y1; ++Y)
    {
        const int64 RowOffset=static_cast<int64>(Y)*SizeX;
        for (int32 X = X0; X <= X1; ++X)
        {
            const FLinearColor& Color = Pixels[RowOffset + X];
            if(!FMath::IsFinite(Color.R)||!FMath::IsFinite(Color.G)||!FMath::IsFinite(Color.B)||!FMath::IsFinite(Color.A)){W.Fail(TEXT("render_target_nonfinite_pixel"),TEXT("Observed non-finite RGBA cannot be reported as valid numeric zero."));return W.Error(W.R.Error);}const double Value=SampleValue(Color);
            MaxValue = FMath::Max(MaxValue, Value);
            const bool bInsideProbe = bHasProbe && X >= ProbeX0 && X <= ProbeX1 && Y >= ProbeY0 && Y <= ProbeY1;
            if (bInsideProbe)
            {
                ++ProbePixelCount;
                ProbeMaxValue = FMath::Max(ProbeMaxValue, Value);
            }
            if (Value > Threshold)
            {
                ++Count;
                Sum += Value;
                WeightedX += static_cast<double>(X) * Value;
                WeightedY += static_cast<double>(Y) * Value;
                HitMinX = FMath::Min(HitMinX, X);
                HitMaxX = FMath::Max(HitMaxX, X);
                HitMinY = FMath::Min(HitMinY, Y);
                HitMaxY = FMath::Max(HitMaxY, Y);

                if (bInsideProbe)
                {
                    ++ProbeCount;
                    ProbeSum += Value;
                    ProbeWeightedX += static_cast<double>(X) * Value;
                    ProbeWeightedY += static_cast<double>(Y) * Value;
                }

                if (FirstHits.Num() < MaxFirstHits)
                {
                    TSharedPtr<FJsonObject> Hit = MakeShared<FJsonObject>();
                    Hit->SetNumberField(TEXT("x"), X);
                    Hit->SetNumberField(TEXT("y"), Y);
                    Hit->SetNumberField(TEXT("value"), Value);
                    TArray<TSharedPtr<FJsonValue>> RGBA;
                    RGBA.Add(MakeShared<FJsonValueNumber>(Color.R));
                    RGBA.Add(MakeShared<FJsonValueNumber>(Color.G));
                    RGBA.Add(MakeShared<FJsonValueNumber>(Color.B));
                    RGBA.Add(MakeShared<FJsonValueNumber>(Color.A));
                    Hit->SetArrayField(TEXT("rgba"), RGBA);
                    FirstHits.Add(MakeShared<FJsonValueObject>(Hit));
                }
            }
        }
    }

    TArray<TSharedPtr<FJsonValue>> RequestedRegion;
    RequestedRegion.Add(MakeShared<FJsonValueNumber>(RequestedX0));
    RequestedRegion.Add(MakeShared<FJsonValueNumber>(RequestedX1));
    RequestedRegion.Add(MakeShared<FJsonValueNumber>(RequestedY0));
    RequestedRegion.Add(MakeShared<FJsonValueNumber>(RequestedY1));

    TArray<TSharedPtr<FJsonValue>> ClampedRegion;
    ClampedRegion.Add(MakeShared<FJsonValueNumber>(X0));
    ClampedRegion.Add(MakeShared<FJsonValueNumber>(X1));
    ClampedRegion.Add(MakeShared<FJsonValueNumber>(Y0));
    ClampedRegion.Add(MakeShared<FJsonValueNumber>(Y1));

    TArray<TSharedPtr<FJsonValue>> SizeArray;
    SizeArray.Add(MakeShared<FJsonValueNumber>(SizeX));
    SizeArray.Add(MakeShared<FJsonValueNumber>(SizeY));

    if(!W.Admit(TEXT("render_stats_final")))return W.Error(W.R.Error);TSharedPtr<FJsonObject> ResultObj=MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetStringField(TEXT("render_target_path"), RenderTargetObjectPath);
    ResultObj->SetArrayField(TEXT("size"), SizeArray);
    ResultObj->SetArrayField(TEXT("requested_region"), RequestedRegion);
    ResultObj->SetArrayField(TEXT("region"), ClampedRegion);
    ResultObj->SetStringField(TEXT("channel"), Channel);
    ResultObj->SetNumberField(TEXT("threshold"), Threshold);
    ResultObj->SetNumberField(TEXT("pixel_count"),static_cast<double>(static_cast<int64>(X1-X0+1)*(Y1-Y0+1)));
    ResultObj->SetNumberField(TEXT("nonzero_count"), Count);
    ResultObj->SetNumberField(TEXT("sum"), Sum);
    ResultObj->SetNumberField(TEXT("max"), MaxValue);
    ResultObj->SetArrayField(TEXT("first_hits"), FirstHits);
    if (Count > 0)
    {
        TArray<TSharedPtr<FJsonValue>> HitBBox;
        HitBBox.Add(MakeShared<FJsonValueNumber>(HitMinX));
        HitBBox.Add(MakeShared<FJsonValueNumber>(HitMinY));
        HitBBox.Add(MakeShared<FJsonValueNumber>(HitMaxX));
        HitBBox.Add(MakeShared<FJsonValueNumber>(HitMaxY));
        ResultObj->SetArrayField(TEXT("hit_bbox"), HitBBox);
        ResultObj->SetNumberField(TEXT("hit_width"), HitMaxX - HitMinX + 1);
        ResultObj->SetNumberField(TEXT("hit_height"), HitMaxY - HitMinY + 1);
        ResultObj->SetNumberField(TEXT("hit_radius_px"), FMath::Max(HitMaxX - HitMinX + 1, HitMaxY - HitMinY + 1) * 0.5);
    }
    if (Sum > 0.0)
    {
        TArray<TSharedPtr<FJsonValue>> Centroid;
        Centroid.Add(MakeShared<FJsonValueNumber>(WeightedX / Sum));
        Centroid.Add(MakeShared<FJsonValueNumber>(WeightedY / Sum));
        ResultObj->SetArrayField(TEXT("centroid"), Centroid);
    }
    if (bHasProbe)
    {
        TSharedPtr<FJsonObject> ProbeObj = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> ProbeRegion;
        ProbeRegion.Add(MakeShared<FJsonValueNumber>(ProbeX0));
        ProbeRegion.Add(MakeShared<FJsonValueNumber>(ProbeX1));
        ProbeRegion.Add(MakeShared<FJsonValueNumber>(ProbeY0));
        ProbeRegion.Add(MakeShared<FJsonValueNumber>(ProbeY1));
        ProbeObj->SetArrayField(TEXT("region"), ProbeRegion);
        ProbeObj->SetNumberField(TEXT("pixel_count"), ProbePixelCount);
        ProbeObj->SetNumberField(TEXT("nonzero_count"), ProbeCount);
        ProbeObj->SetNumberField(TEXT("sum"), ProbeSum);
        ProbeObj->SetNumberField(TEXT("max"), ProbeMaxValue);
        if (ProbeSum > 0.0)
        {
            TArray<TSharedPtr<FJsonValue>> ProbeCentroid;
            ProbeCentroid.Add(MakeShared<FJsonValueNumber>(ProbeWeightedX / ProbeSum));
            ProbeCentroid.Add(MakeShared<FJsonValueNumber>(ProbeWeightedY / ProbeSum));
            ProbeObj->SetArrayField(TEXT("centroid"), ProbeCentroid);
        }
        ResultObj->SetObjectField(TEXT("probe"), ProbeObj);
    }
    ResultObj->SetStringField(TEXT("readback"), TEXT("FTextureRenderTargetResource::ReadLinearColorPixels"));
    ResultObj->SetStringField(TEXT("safety_note"), TEXT("Read-only render-target analysis. Loads the render target and reads GPU contents; does not save, compile, or mutate assets."));
    ResultObj->SetBoolField(TEXT("partial_observation"),false);ResultObj->SetBoolField(TEXT("wrapper_nonblocking"),false);ResultObj->SetBoolField(TEXT("SDK_internal_render_flush"),true);ResultObj->SetStringField(TEXT("metric_scope"),TEXT("One actual RCM_MinMax, LinearToGammaFalse full-target SDK readback, then inclusive row-major region/probe statistics from the same owned pixel buffer. No Niagara/GPU-global completion or frame freshness claim."));W.R.Data->SetBoolField(TEXT("typed_observation_complete"),true);return ResultObj;
}


FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
 bool Success=false,Typed=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),Success);W.R.Data->TryGetBoolField(TEXT("typed_observation_complete"),Typed);
 if(!Success&&W.R.Error.IsEmpty()){FString E;if(Legacy)Legacy->TryGetStringField(TEXT("error"),E);W.Fail(TEXT("niagara_runtime_inspection_failed"),E.IsEmpty()?TEXT("Dedicated observation failed; actual prefix retained."):E);}
 const bool Complete=Success&&Typed&&W.R.Error.IsEmpty()&&W.Admit(TEXT("final_exact_observation_admission"));W.R.bSuccess=Complete;W.R.bPostconditionPassed=Complete;
 if(Success&&!Complete&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_runtime_postcondition_incomplete"),TEXT("Dedicated observation did not finish its supported typed scope."));
 W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("supported_observation_complete"),Complete);W.R.Data->SetObjectField(TEXT("native_call_counts"),W.Counts);W.R.Data->SetArrayField(TEXT("actual_observation_prefix"),W.Prefix);W.R.Data->SetBoolField(TEXT("explicit_mutation_attempted"),false);W.R.Data->SetBoolField(TEXT("atomic_frame_snapshot_verified"),false);W.R.Data->SetBoolField(TEXT("instance_concurrent_completion_waited"),false);W.R.Data->SetBoolField(TEXT("GPU_global_completion_verified"),false);W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
 W.R.Data->SetStringField(TEXT("observer_scope"),TEXT("Supported public GT/asset/override observation, already-paused Solo/shared InstanceParameters/system-emitter counters and already-paused Solo exact native Grid2D GT copies, or bounded actual RT SDK readback. Running/nonpaused-complete/stateless-specific internal snapshots, non-Solo Grid2D, SDK global purity and every GPU frame remain unverified. Explicit Modify/compile/save/transaction and observer Pause/tick/wait/finalize are zero."));
 if(!W.R.Error.IsEmpty())W.R.Data->SetBoolField(TEXT("post_failure_live_projection_skipped"),true);W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(const TSharedPtr<FJsonObject>&);
FString Run(const FString& Text,const TCHAR* Operation,FHandler Handler)
{
 FWork W;TSharedPtr<FJsonObject> Params;if(!Parse(Text,Params)){W.Fail(TEXT("niagara_runtime_invalid_params_json"),TEXT("Finite JSON object within1Mi UTF16/depth64/262144 notation and values, no duplicate case-sensitive keys or embedded NUL required."));return Finish(W,W.Error(W.R.Error),Operation);}
 if(!IsInGameThread()){W.Fail(TEXT("niagara_runtime_game_thread_required"),TEXT("Editor game thread required; no global scope accessed."));return Finish(W,W.Error(W.R.Error),Operation);}if(Running){W.Fail(TEXT("niagara_runtime_reentrant_request"),TEXT("Dedicated observation already active; no replay."));return Finish(W,W.Error(W.R.Error),Operation);}
 TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);W.R.Data->SetObjectField(TEXT("requested_params"),Params);if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);return Finish(W,Handler(Params),Operation);
}
}
FString InspectRuntimeState(const FString& ParamsJson){return Run(ParamsJson,TEXT("inspect_niagara_component_runtime_state"),&HandleRuntime);}
FString InspectParameterStore(const FString& ParamsJson){return Run(ParamsJson,TEXT("inspect_niagara_component_parameter_store"),&HandleParameterStore);}
FString InspectGrid2DRuntime(const FString& ParamsJson){return Run(ParamsJson,TEXT("inspect_niagara_component_grid2d_runtime"),&HandleGrid);}
FString SampleRenderTargetRegionStats(const FString& ParamsJson){return Run(ParamsJson,TEXT("sample_render_target2d_region_stats"),&HandleRT);}
}
