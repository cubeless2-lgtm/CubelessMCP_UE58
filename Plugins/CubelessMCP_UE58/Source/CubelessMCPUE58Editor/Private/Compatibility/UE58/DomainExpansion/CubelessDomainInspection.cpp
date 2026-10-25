#include "Registration/CubelessDomainExpansionLibrary.h"
#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#include "Misc/EngineVersionComparison.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Misc/SecureHash.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif
#if !UE_VERSION_OLDER_THAN(5, 8, 0)
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimNode_StateMachine.h"
#include "ControlRigRuntimeAsset.h"
#include "ControlRigEditorAsset.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowVertexAttributeEditableNode.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowEngine.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#endif
#endif

namespace CubelessDomainInspection
{
using FObject = TSharedPtr<FJsonObject>;
FObject Result() { FObject R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"),true); R->SetBoolField(TEXT("read_only"),true); R->SetBoolField(TEXT("saved"),false); R->SetBoolField(TEXT("change_state_known"),true); return R; }
FObject Error(const FString& Message) { FObject R=Result(); R->SetBoolField(TEXT("success"),false); R->SetStringField(TEXT("error"),Message); return R; }
FString String(const FObject& P,const TCHAR* Key) { FString V; P->TryGetStringField(Key,V); return V; }
FString Serialize(const FObject& R) { FString Text; auto Writer=TJsonWriterFactory<>::Create(&Text); FJsonSerializer::Serialize(R.ToSharedRef(),Writer); return Text; }
bool OnlyFields(const FObject& P,const TSet<FString>& Fields) { for(const auto& Pair:P->Values) { if(!Fields.Contains(FString(*Pair.Key))) { return false; } } return true; }
int32 Limit(const FObject& P) { double V=128; P->TryGetNumberField(TEXT("max_results"),V); return FMath::Clamp(static_cast<int32>(FMath::Clamp(V,1.0,512.0)),1,512); }
template<typename T> T* Asset(const FObject& P) { const FString Path=String(P,TEXT("asset_path")); return Path.Len()<=1024 && Path.StartsWith(TEXT("/")) ? LoadObject<T>(nullptr,*Path) : nullptr; }

#if !UE_VERSION_OLDER_THAN(5, 8, 0)
bool RestorePackageAfterRollback(UPackage& Package,bool DirtyBefore,bool ValuesRestored)
{
    if(ValuesRestored) { Package.SetDirtyFlag(DirtyBefore); }
    return ValuesRestored && Package.IsDirty()==DirtyBefore;
}
FObject PCGCounts(const FObject& P,bool ForceReadbackFailure=false)
{
    const FString Path=String(P,TEXT("asset_path")), Name=String(P,TEXT("node_name"));
    if(!Path.StartsWith(TEXT("/Game/_MCP_Temp/")) || !FPackageName::IsValidLongPackageName(Path) || Path.Contains(TEXT("//")) || Name.IsEmpty()) { return Error(TEXT("Canonical temp graph path and exact node_name required")); }
    UPCGGraph* Graph=Asset<UPCGGraph>(P);
    if(!Graph || Graph->GetOutermost()->GetName()!=Path) { return Error(TEXT("Exact nonredirected PCG graph required")); }
    UPCGNode* Match=nullptr;
    for(UPCGNode* Node:Graph->GetNodes()) { if(Node && Node->GetName()==Name) { if(Match) { return Error(TEXT("Ambiguous node")); } Match=Node; } }
    UPCGSettings* Settings=Match ? Match->GetSettings() : nullptr;
    if(!Settings || Settings->GetClass()->GetPathName()!=TEXT("/Script/PCG.PCGCustomHLSLSettings")) { return Error(TEXT("Custom HLSL settings required")); }
    if(!Settings->IsIn(Graph) || Settings->GetOutermost()!=Graph->GetOutermost()) { return Error(TEXT("Shared or externally owned settings are not writable")); }
    const TCHAR* Keys[]={TEXT("num_elements"),TEXT("num_elements_x"),TEXT("num_elements_y"),TEXT("num_elements_z")};
    const TCHAR* Fields[]={TEXT("NumElements"),TEXT("NumElementsX"),TEXT("NumElementsY"),TEXT("NumElementsZ")};
    FIntProperty* Properties[4]; int32 Before[4], After[4]; FObject BeforeJSON=MakeShared<FJsonObject>();
    for(int32 I=0;I<4;++I) { Properties[I]=FindFProperty<FIntProperty>(Settings->GetClass(),Fields[I]); if(!Properties[I]) { return Error(TEXT("UE5.8 element count fields unavailable")); } Before[I]=After[I]=Properties[I]->GetPropertyValue_InContainer(Settings); BeforeJSON->SetNumberField(Keys[I],Before[I]); }
    bool DryRun=true;
    if(P->HasField(TEXT("dry_run")) && !P->TryGetBoolField(TEXT("dry_run"),DryRun)) { return Error(TEXT("dry_run must be boolean")); }
    const TSharedPtr<FJsonObject>* Counts=nullptr;
    if(!P->TryGetObjectField(TEXT("counts"),Counts) || !Counts || (*Counts)->Values.IsEmpty() || !OnlyFields(*Counts,{Keys[0],Keys[1],Keys[2],Keys[3]})) { return Error(TEXT("Allowlisted counts required")); }
    for(int32 I=0;I<4;++I) { if((*Counts)->HasField(Keys[I])) { double V; const auto Value=(*Counts)->TryGetField(Keys[I]); if(!Value || Value->Type!=EJson::Number || !Value->TryGetNumber(V) || !FMath::IsFinite(V) || V<1 || V>16777216 || FMath::FloorToDouble(V)!=V) { return Error(TEXT("Counts must be integers in 1..16777216")); } After[I]=static_cast<int32>(V); } }
    UPackage* Package=Graph->GetOutermost(); const bool DirtyBefore=Package->IsDirty();
    FObject R=Result(), Proposed=MakeShared<FJsonObject>();
    for(int32 I=0;I<4;++I) { Proposed->SetNumberField(Keys[I],After[I]); }
    R->SetObjectField(TEXT("before"),BeforeJSON); R->SetObjectField(TEXT("after"),BeforeJSON); R->SetObjectField(TEXT("proposed"),Proposed); R->SetBoolField(TEXT("dry_run"),DryRun); R->SetBoolField(TEXT("read_only"),DryRun); R->SetBoolField(TEXT("mutation_performed"),false); R->SetBoolField(TEXT("package_dirty_before"),DirtyBefore); R->SetBoolField(TEXT("package_dirty_after"),DirtyBefore);
    if(DryRun) { return R; }
    const TSharedPtr<FJsonObject>* Expected=nullptr;
    if(!P->TryGetObjectField(TEXT("expected"),Expected) || !Expected || (*Expected)->Values.Num()!=4) { return Error(TEXT("All four expected counts required")); }
    for(int32 I=0;I<4;++I) { double V; const auto Value=(*Expected)->TryGetField(Keys[I]); if(!Value || Value->Type!=EJson::Number || !Value->TryGetNumber(V) || V!=Before[I]) { return Error(TEXT("expected_current mismatch; inspect again")); } }
    if(GEditor->PlayWorld || DirtyBefore) { return Error(TEXT("Apply requires a clean graph package outside PIE")); }
    bool Changed=false; for(int32 I=0;I<4;++I) { Changed|=After[I]!=Before[I]; }
    if(!Changed) { R->SetBoolField(TEXT("no_op"),true); R->SetBoolField(TEXT("postcondition_verified"),true); return R; }
    TStrongObjectPtr<UPCGGraph> GraphHold(Graph);TStrongObjectPtr<UPCGNode> NodeHold(Match);TStrongObjectPtr<UPCGSettings> SettingsHold(Settings);TStrongObjectPtr<UPackage> PackageHold(Package);
    const FString GraphPath=Graph->GetPathName(),NodePath=Match->GetPathName(),SettingsPath=Settings->GetPathName();
    auto CurrentSettings=[&]{return Cubeless::UE58::DomainExpansion::SafeContext()&&IsValid(Graph)&&IsValid(Match)&&IsValid(Settings)&&Graph->GetPackage()==PackageHold.Get()&&Graph->GetPathName()==GraphPath&&Match->GetPathName()==NodePath&&Settings->GetPathName()==SettingsPath&&Graph->GetNodes().Contains(Match)&&Match->GetSettings()==Settings&&Settings->IsIn(Graph);};
    auto Unknown=[&]{R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("mutation_performed"),true);R->SetBoolField(TEXT("do_not_retry"),true);R->SetStringField(TEXT("error"),TEXT("PCG graph/settings identity changed during callback; stop without rollback or further SDK"));return R;};
    FScopedTransaction Transaction(NSLOCTEXT("CubelessDomainExpansion","UE58PCGCounts","Configure PCG Custom HLSL Elements")); Graph->Modify();if(!CurrentSettings())return Unknown(); Settings->Modify();if(!CurrentSettings())return Unknown();
    auto Write=[&](const int32* Values) { for(int32 I=0;I<4;++I) { if(!CurrentSettings())return false;Properties[I]->SetPropertyValue_InContainer(Settings,Values[I]); FPropertyChangedEvent Event(Properties[I]); static_cast<UObject*>(Settings)->PostEditChangeProperty(Event);if(!CurrentSettings())return false; }return true; };
    auto Verify=[&](const int32* Values) { for(int32 I=0;I<4;++I) { if(Properties[I]->GetPropertyValue_InContainer(Settings)!=Values[I]) { return false; } } return true; };
    if(!Write(After))return Unknown(); const bool Verified=Verify(After) && !ForceReadbackFailure;
    if(!Verified) { if(!Write(Before))return Unknown(); const bool Restored=RestorePackageAfterRollback(*Package,DirtyBefore,Verify(Before)); if(Restored) { Transaction.Cancel(); } R->SetBoolField(TEXT("success"),false); R->SetStringField(TEXT("error"),TEXT("Readback failed; inspect rollback before writing again")); R->SetBoolField(TEXT("do_not_retry"),true); R->SetBoolField(TEXT("rollback_verified"),Restored); }
    else { Graph->MarkPackageDirty();if(!CurrentSettings())return Unknown(); }
    FObject Current=MakeShared<FJsonObject>(); for(int32 I=0;I<4;++I) { Current->SetNumberField(Keys[I],Properties[I]->GetPropertyValue_InContainer(Settings)); }
    R->SetObjectField(TEXT("after"),Current); R->SetBoolField(TEXT("mutation_performed"),true); R->SetBoolField(TEXT("postcondition_verified"),Verified); R->SetBoolField(TEXT("package_dirty_after"),Package->IsDirty()); return R;
}

void ApplyBodySettings(FBodyInstance& B, uint8 Position, uint8 Velocity, uint8 Projection, bool Override, bool Sleep)
{
    B.SetPositionSolverIterationCount(Position);
    B.SetVelocitySolverIterationCount(Velocity);
    B.SetProjectionSolverIterationCount(Projection);
    B.SetOverrideIterationCounts(Override);
    B.SetAllowPartialIslandSleep(Sleep);
}
FObject BodyState(const FBodyInstance& B)
{
    FObject R=MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("override_iterations"),B.GetPositionSolverIterationCount()>=0);
    R->SetBoolField(TEXT("allow_partial_island_sleep"),B.GetAllowPartialIslandSleep());
    R->SetBoolField(TEXT("physics_actor_valid"),B.IsValidBodyInstance());
    R->SetBoolField(TEXT("runtime_application_confirmed"),false);
    R->SetNumberField(TEXT("position_iterations"),B.PositionSolverIterationCount);
    R->SetNumberField(TEXT("velocity_iterations"),B.VelocitySolverIterationCount);
    R->SetNumberField(TEXT("projection_iterations"),B.ProjectionSolverIterationCount);
    R->SetNumberField(TEXT("effective_position_iterations"),B.GetPositionSolverIterationCount());
    R->SetNumberField(TEXT("effective_velocity_iterations"),B.GetVelocitySolverIterationCount());
    R->SetNumberField(TEXT("effective_projection_iterations"),B.GetProjectionSolverIterationCount());
    const FString Canonical=FString::Printf(TEXT("%d:%d:%u:%u:%u"),B.GetPositionSolverIterationCount()>=0,B.GetAllowPartialIslandSleep(),B.PositionSolverIterationCount,B.VelocitySolverIterationCount,B.ProjectionSolverIterationCount);
    R->SetStringField(TEXT("state_hash"),FMD5::HashAnsiString(*Canonical));
    return R;
}
UPrimitiveComponent* FindBodyComponent(const FObject& P,FString& ErrorText)
{
    UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if(!World) { ErrorText=TEXT("No editor world"); return nullptr; }
    const FString ActorPath=String(P,TEXT("actor_path"));
    const FString ComponentName=String(P,TEXT("component_name"));
    if(ActorPath.IsEmpty() || ComponentName.IsEmpty()) { ErrorText=TEXT("Exact actor_path and component_name are required"); return nullptr; }
    UPrimitiveComponent* Match=nullptr;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        if(It->GetPathName()!=ActorPath) { continue; }
        TInlineComponentArray<UPrimitiveComponent*> Components; It->GetComponents(Components);
        for(UPrimitiveComponent* C:Components)
        {
            if(C && C->GetName()==ComponentName) { if(Match) { ErrorText=TEXT("Ambiguous component"); return nullptr; } Match=C; }
        }
    }
    if(!Match) { ErrorText=TEXT("Exact primitive component not found in editor world"); }
    return Match;
}
FObject Body(const FObject& P,bool bConfigure)
{
    FString Message; UPrimitiveComponent* C=FindBodyComponent(P,Message);
    if(!C) { return Error(Message); }
    // Default body only. Per-bone skeletal/welded/instanced bodies are not inferred.
    if(C->IsA<USkinnedMeshComponent>() || C->IsA<UInstancedStaticMeshComponent>()) { return Error(TEXT("Only non-skinned, non-instanced primitive components are supported")); }
    FBodyInstance* B=C->GetBodyInstance(NAME_None,false);
    if(!B || B->WeldParent || B->OwnerComponent.Get()!=C) { return Error(TEXT("A unique unwelded body instance owned by this component is required")); }
    FObject R=Result(); R->SetStringField(TEXT("actor_path"),C->GetOwner()->GetPathName()); R->SetStringField(TEXT("component_name"),C->GetName());
    FObject Before=BodyState(*B); R->SetObjectField(TEXT("before"),Before); R->SetObjectField(TEXT("after"),Before);
    if(!bConfigure) { return R; }
    if(!OnlyFields(P,{TEXT("operation"),TEXT("actor_path"),TEXT("component_name"),TEXT("max_results"),TEXT("dry_run"),TEXT("expected_state_hash"),TEXT("settings")})) { return Error(TEXT("Unknown request field")); }
    bool bDryRun=true; if(P->HasField(TEXT("dry_run")) && !P->TryGetBoolField(TEXT("dry_run"),bDryRun)) { return Error(TEXT("dry_run must be boolean")); }
    const TSharedPtr<FJsonObject>* Settings=nullptr;
    if(!P->TryGetObjectField(TEXT("settings"),Settings) || !Settings || (*Settings)->Values.IsEmpty() || !OnlyFields(*Settings,{TEXT("override_iterations"),TEXT("allow_partial_island_sleep"),TEXT("position_iterations"),TEXT("velocity_iterations"),TEXT("projection_iterations")})) { return Error(TEXT("Nonempty allowlisted settings object required")); }
    bool Override=B->GetPositionSolverIterationCount()>=0, Sleep=B->GetAllowPartialIslandSleep();
    int32 Position=B->PositionSolverIterationCount, Velocity=B->VelocitySolverIterationCount, Projection=B->ProjectionSolverIterationCount;
    for(const auto& Pair:(*Settings)->Values)
    {
        if(Pair.Key==TEXT("override_iterations") || Pair.Key==TEXT("allow_partial_island_sleep"))
        {
            bool Value; if(!Pair.Value.IsValid() || Pair.Value->Type!=EJson::Boolean || !Pair.Value->TryGetBool(Value)) { return Error(TEXT("Boolean settings require JSON booleans")); }
            if(Pair.Key==TEXT("override_iterations")) { Override=Value; } else { Sleep=Value; }
        }
        else
        {
            double Value; if(!Pair.Value.IsValid() || Pair.Value->Type!=EJson::Number || !Pair.Value->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value<0 || Value>255 || FMath::FloorToDouble(Value)!=Value) { return Error(TEXT("Iterations must be integers in 0..255")); }
            if(Pair.Key==TEXT("position_iterations")) { Position=static_cast<int32>(Value); } else if(Pair.Key==TEXT("velocity_iterations")) { Velocity=static_cast<int32>(Value); } else { Projection=static_cast<int32>(Value); }
        }
    }
    R->SetBoolField(TEXT("read_only"),bDryRun); R->SetBoolField(TEXT("dry_run"),bDryRun); R->SetBoolField(TEXT("mutation_performed"),false);
    FObject Proposed=MakeShared<FJsonObject>();
    Proposed->SetBoolField(TEXT("override_iterations"),Override); Proposed->SetBoolField(TEXT("allow_partial_island_sleep"),Sleep);
    Proposed->SetNumberField(TEXT("position_iterations"),Position); Proposed->SetNumberField(TEXT("velocity_iterations"),Velocity); Proposed->SetNumberField(TEXT("projection_iterations"),Projection);
    R->SetObjectField(TEXT("proposed"),Proposed);
    if(bDryRun) { return R; }
    UWorld* World=C->GetWorld();
    if(GEditor->PlayWorld || !World || !World->GetOutermost()->GetName().StartsWith(TEXT("/Game/_MCP_Temp/"))) { return Error(TEXT("Apply requires an editor temp map outside PIE")); }
    if(String(P,TEXT("expected_state_hash"))!=String(Before,TEXT("state_hash"))) { return Error(TEXT("state_hash mismatch; inspect again before applying")); }
    if((B->GetPositionSolverIterationCount()>=0)==Override && B->GetAllowPartialIslandSleep()==Sleep && B->PositionSolverIterationCount==Position && B->VelocitySolverIterationCount==Velocity && B->ProjectionSolverIterationCount==Projection)
    {
        R->SetBoolField(TEXT("postcondition_verified"),true); R->SetBoolField(TEXT("no_op"),true); return R;
    }
    const bool OldOverride=B->GetPositionSolverIterationCount()>=0, OldSleep=B->GetAllowPartialIslandSleep();
    const uint8 OldPosition=B->PositionSolverIterationCount, OldVelocity=B->VelocitySolverIterationCount, OldProjection=B->ProjectionSolverIterationCount;
    UPackage* Package=C->GetOutermost(); const bool DirtyBefore=Package->IsDirty();
    R->SetBoolField(TEXT("package_dirty_before"),DirtyBefore);
    TStrongObjectPtr<UPrimitiveComponent> ComponentHold(C);TStrongObjectPtr<AActor> OwnerHold(C->GetOwner());TStrongObjectPtr<UPackage> PackageHold(Package);
    const FString ComponentPath=C->GetPathName();
    auto CurrentBody=[&]{return Cubeless::UE58::DomainExpansion::SafeContext()&&IsValid(C)&&IsValid(OwnerHold.Get())&&C->GetOwner()==OwnerHold.Get()&&C->GetPackage()==PackageHold.Get()&&C->GetPathName()==ComponentPath&&C->GetBodyInstance(NAME_None,false)==B&&!B->WeldParent&&B->OwnerComponent.Get()==C;};
    auto Unknown=[&]{R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("mutation_performed"),true);R->SetBoolField(TEXT("do_not_retry"),true);R->SetStringField(TEXT("error"),TEXT("Body identity/context changed across a callback; stop without follow-up SDK"));return R;};
    FScopedTransaction Transaction(NSLOCTEXT("CubelessDomainExpansion","UE58BodySettings","Configure Body Solver Settings"));
    C->Modify();if(!CurrentBody())return Unknown();C->GetOwner()->Modify();if(!CurrentBody())return Unknown();
    ApplyBodySettings(*B,static_cast<uint8>(Position),static_cast<uint8>(Velocity),static_cast<uint8>(Projection),Override,Sleep);if(!CurrentBody())return Unknown();
    const bool Verified=(B->GetPositionSolverIterationCount()>=0)==Override && B->GetAllowPartialIslandSleep()==Sleep && B->PositionSolverIterationCount==Position && B->VelocitySolverIterationCount==Velocity && B->ProjectionSolverIterationCount==Projection;
    if(!Verified)
    {
        ApplyBodySettings(*B,OldPosition,OldVelocity,OldProjection,OldOverride,OldSleep);if(!CurrentBody())return Unknown();
        R->SetBoolField(TEXT("success"),false); R->SetStringField(TEXT("error"),TEXT("Body readback failed; inspect rollback state before further writes")); R->SetBoolField(TEXT("do_not_retry"),true);
        const bool Restored=(B->GetPositionSolverIterationCount()>=0)==OldOverride && B->GetAllowPartialIslandSleep()==OldSleep && B->PositionSolverIterationCount==OldPosition && B->VelocitySolverIterationCount==OldVelocity && B->ProjectionSolverIterationCount==OldProjection;
        const bool RollbackVerified=RestorePackageAfterRollback(*Package,DirtyBefore,Restored);
        if(RollbackVerified) { Transaction.Cancel(); }
        R->SetBoolField(TEXT("rollback_verified"),RollbackVerified);
    }
    if(Verified) { C->MarkPackageDirty();if(!CurrentBody())return Unknown(); }
    R->SetBoolField(TEXT("package_dirty_after"),Package->IsDirty()); R->SetBoolField(TEXT("mutation_performed"),true); R->SetBoolField(TEXT("postcondition_verified"),Verified); R->SetObjectField(TEXT("after"),BodyState(*B));
    return R;
}

FObject Niagara(const FObject& P)
{
    UNiagaraSystem* System=Asset<UNiagaraSystem>(P); if(!System) { return Error(TEXT("Niagara system not found")); }
    FObject R=Result(); TArray<TSharedPtr<FJsonValue>> Rows;
    for(const FNiagaraEmitterHandle& Handle:System->GetEmitterHandles())
    {
        if(Rows.Num()>=Limit(P)) { break; }
        FObject Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"),Handle.GetName().ToString()); Row->SetBoolField(TEXT("enabled"),Handle.GetIsEnabled());
        Row->SetStringField(TEXT("mode"),Handle.GetEmitterMode()==ENiagaraEmitterMode::Stateless ? TEXT("stateless") : TEXT("standard"));
        const FObjectPropertyBase* StatelessProperty=FindFProperty<FObjectPropertyBase>(FNiagaraEmitterHandle::StaticStruct(),TEXT("StatelessEmitter"));
        UObject* Emitter=StatelessProperty ? StatelessProperty->GetObjectPropertyValue_InContainer(&Handle) : nullptr;
        if(Handle.GetEmitterMode()==ENiagaraEmitterMode::Stateless && !Emitter) { return Error(TEXT("Stateless emitter object/reflection unavailable")); }
        if(Handle.GetEmitterMode()==ENiagaraEmitterMode::Stateless && Emitter)
        {
            Row->SetStringField(TEXT("asset_path"),Emitter->GetPathName());
            const FArrayProperty* SpawnProperty=FindFProperty<FArrayProperty>(Emitter->GetClass(),TEXT("SpawnInfos"));
            const FArrayProperty* ModulesProperty=FindFProperty<FArrayProperty>(Emitter->GetClass(),TEXT("Modules"));
            if(!SpawnProperty || !ModulesProperty) { return Error(TEXT("Stateless emitter reflection contract unavailable")); }
            FScriptArrayHelper Spawn(SpawnProperty,SpawnProperty->ContainerPtrToValuePtr<void>(Emitter));
            FScriptArrayHelper ModuleArray(ModulesProperty,ModulesProperty->ContainerPtrToValuePtr<void>(Emitter));
            const FObjectPropertyBase* ModuleProperty=CastField<FObjectPropertyBase>(ModulesProperty->Inner);
            if(!ModuleProperty) { return Error(TEXT("Stateless module object array unavailable")); }
            Row->SetNumberField(TEXT("spawn_info_count"),Spawn.Num());
            Row->SetNumberField(TEXT("module_count"),ModuleArray.Num());
            TArray<TSharedPtr<FJsonValue>> Modules;
            for(int32 Index=0;Index<ModuleArray.Num() && Modules.Num()<Limit(P);++Index) { if(UObject* Module=ModuleProperty->GetObjectPropertyValue(ModuleArray.GetRawPtr(Index))) { Modules.Add(MakeShared<FJsonValueString>(Module->GetClass()->GetPathName())); } }
            Row->SetArrayField(TEXT("module_classes"),Modules);
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetNumberField(TEXT("emitter_count"),System->GetEmitterHandles().Num()); R->SetBoolField(TEXT("truncated"),Rows.Num()<System->GetEmitterHandles().Num()); R->SetArrayField(TEXT("emitters"),Rows); return R;
}

FObject AnimBP(const FObject& P)
{
    UAnimBlueprint* BP=Asset<UAnimBlueprint>(P); if(!BP || !BP->GeneratedClass) { return Error(TEXT("Compiled AnimBlueprint required; this tool never compiles")); }
    const IAnimClassInterface* Interface=IAnimClassInterface::GetFromClass(BP->GeneratedClass); if(!Interface) { return Error(TEXT("No animation class interface")); }
    FObject R=Result(); TArray<TSharedPtr<FJsonValue>> Rows;
    const auto& Properties=Interface->GetAnimNodeProperties(); const auto& Machines=Interface->GetBakedStateMachines();
    for(int32 Index=0;Index<Properties.Num() && Rows.Num()<Limit(P);++Index)
    {
        const FStructProperty* Property=Properties[Index]; if(!Property || !Property->Struct) { continue; }
        FObject Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("property_index"),Index); Row->SetNumberField(TEXT("runtime_node_index"),Properties.Num()-1-Index);
        Row->SetStringField(TEXT("property"),Property->GetName()); Row->SetStringField(TEXT("struct_path"),Property->Struct->GetPathName());
        if(Property->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()))
        {
            const auto* Machine=Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(BP->GeneratedClass->GetDefaultObject());
            Row->SetNumberField(TEXT("baked_machine_index"),Machine->StateMachineIndexInClass);
            if(Machines.IsValidIndex(Machine->StateMachineIndexInClass)) { Row->SetStringField(TEXT("machine_name"),Machines[Machine->StateMachineIndexInClass].MachineName.ToString()); }
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetNumberField(TEXT("node_count"),Properties.Num()); R->SetNumberField(TEXT("baked_machine_count"),Machines.Num()); R->SetBoolField(TEXT("truncated"),Rows.Num()<Properties.Num()); R->SetArrayField(TEXT("nodes"),Rows); return R;
}

FObject Rig(const FObject& P)
{
    UObject* Object=Asset<UObject>(P); UControlRigRuntimeAsset* Runtime=Cast<UControlRigRuntimeAsset>(Object);
    UObject* Editor=Runtime ? Runtime->GetEditorAsset() : Object;
    if(!Editor || !Editor->GetClass()->ImplementsInterface(UControlRigEditorAssetInterface::StaticClass())) { return Error(TEXT("ControlRig RuntimeAsset, EditorAsset or legacy Blueprint required")); }
    FControlRigAssetInterfacePtr Interface(Editor); FObject R=Result();
    if(!Interface->GetRigVMClient()) { return Error(TEXT("ControlRig RigVM client unavailable")); }
    R->SetStringField(TEXT("asset_kind"),Runtime ? TEXT("runtime_asset") : (Cast<UBlueprint>(Object) ? TEXT("legacy_blueprint") : TEXT("editor_asset")));
    R->SetStringField(TEXT("rig_reference"),Interface->GetControlRigAssetReference().GetPathName()); R->SetStringField(TEXT("editor_asset"),Editor->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Rows; int32 Total=0;
    for(URigVMGraph* Graph:Interface->GetRigVMClient()->GetAllModels(true,false))
    {
        if(!Graph) { continue; }
        for(URigVMNode* Node:Graph->GetNodes())
        {
            if(!Node) { continue; }
            URigVMUnitNode* Unit=Cast<URigVMUnitNode>(Node); const UScriptStruct* Struct=Unit ? Unit->GetScriptStruct() : nullptr;
            const FString Type=Struct ? Struct->GetPathName() : Node->GetClass()->GetPathName();
            const FString Title=Node->GetNodeTitle();
            if(!Type.Contains(TEXT("Physics")) && !Type.Contains(TEXT("Dynamics")) && !Title.Contains(TEXT("Physics")) && !Title.Contains(TEXT("Dynamics"))) { continue; }
            ++Total; if(Rows.Num()>=Limit(P)) { continue; }
            FObject Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("graph"),Graph->GetNodePath()); Row->SetStringField(TEXT("node"),Node->GetNodePath()); Row->SetStringField(TEXT("title"),Title); Row->SetStringField(TEXT("type"),Type); Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    R->SetStringField(TEXT("scope"),TEXT("Physics/Dynamics named nodes in local recursive RigVM models; external function bodies are not expanded")); R->SetNumberField(TEXT("matching_count"),Total); R->SetBoolField(TEXT("truncated"),Total>Rows.Num()); R->SetArrayField(TEXT("nodes"),Rows); return R;
}

FObject ClothSolver(const FObject& P)
{
    const FString ActorPath=String(P,TEXT("actor_path")), Name=String(P,TEXT("component_name"));
    UWorld* World=GEditor->GetEditorWorldContext().World(); if(!World || ActorPath.IsEmpty() || Name.IsEmpty()) { return Error(TEXT("Exact actor_path and component_name required")); }
    UChaosClothComponent* Component=nullptr;
    for(TActorIterator<AActor> It(World);It;++It) { if(It->GetPathName()!=ActorPath) { continue; } TInlineComponentArray<UChaosClothComponent*> Components; It->GetComponents(Components); for(auto* C:Components) { if(C && C->GetName()==Name) { Component=C; } } }
    if(!Component) { return Error(TEXT("Chaos Cloth component not found in editor world")); }
    FObject R=Result(); R->SetBoolField(TEXT("simulate_in_editor"),Component->GetSimulateInEditor()); R->SetBoolField(TEXT("simulation_enabled"),Component->IsSimulationEnabled()); R->SetBoolField(TEXT("simulation_suspended"),Component->IsSimulationSuspended());
    // The stock getter directly indexes model zero; verify the reflected backing array first.
    const FArrayProperty* ModelsProperty=FindFProperty<FArrayProperty>(Component->GetClass(),TEXT("ClothSimulationProperties"));
    const FStructProperty* ModelProperty=ModelsProperty ? CastField<FStructProperty>(ModelsProperty->Inner) : nullptr;
    if(!ModelsProperty || !ModelProperty || ModelProperty->Struct!=FChaosClothSimulationProperties::StaticStruct()) { return Error(TEXT("Cloth simulation property reflection unavailable")); }
    FScriptArrayHelper Models(ModelsProperty,ModelsProperty->ContainerPtrToValuePtr<void>(Component));
    if(!Component->IsRegistered() || !Component->GetAsset() || Models.Num()==0)
    {
        R->SetBoolField(TEXT("solver_properties_initialized"),false); R->SetNumberField(TEXT("lod_count"),0); R->SetNumberField(TEXT("property_count"),0); R->SetBoolField(TEXT("truncated"),false); R->SetArrayField(TEXT("properties"),{}); return R;
    }
    R->SetBoolField(TEXT("solver_properties_initialized"),true);
    TArray<TSharedPtr<FJsonValue>> Rows; int32 Available=0, Lod=0;
    for(const auto& Collection:Component->GetSolverPropertyCollections())
    {
        if(Collection)
        {
            Chaos::Softs::FCollectionPropertyConstFacade Facade(Collection);
            if(Facade.IsValid()) { Available+=Facade.Num(); for(int32 Index=0;Index<Facade.Num() && Rows.Num()<Limit(P);++Index) { FObject Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("lod_index"),Lod); Row->SetStringField(TEXT("key"),Facade.GetKeyName(Index).ToString()); Row->SetNumberField(TEXT("flags"),static_cast<uint8>(Facade.GetFlags(Index))); Row->SetStringField(TEXT("low"),Facade.GetLowValue<FVector3f>(Index).ToString()); Row->SetStringField(TEXT("high"),Facade.GetHighValue<FVector3f>(Index).ToString()); Row->SetStringField(TEXT("string"),Facade.GetStringValue(Index)); Rows.Add(MakeShared<FJsonValueObject>(Row)); } }
        }
        ++Lod;
    }
    R->SetNumberField(TEXT("lod_count"),Lod); R->SetNumberField(TEXT("property_count"),Available); R->SetBoolField(TEXT("truncated"),Rows.Num()<Available); R->SetArrayField(TEXT("properties"),Rows); return R;
}

FObject ClothSnapshots(const FObject& P)
{
    UDataflow* Dataflow=Asset<UDataflow>(P); if(!Dataflow || !Dataflow->GetDataflowGraph()) { return Error(TEXT("Dataflow asset required")); }
    const FString Name=String(P,TEXT("node_name")); if(Name.IsEmpty()) { return Error(TEXT("Exact node_name required")); }
    TSharedPtr<FDataflowNode> Match;
    for(const auto& Node:Dataflow->GetDataflowGraph()->GetNodes()) { if(Node && Node->GetName().ToString()==Name) { if(Match) { return Error(TEXT("Ambiguous node")); } Match=Node; } }
    if(!Match || !Match->TypedScriptStruct() || !Match->TypedScriptStruct()->IsChildOf(FDataflowVertexAttributeEditableNode::StaticStruct())) { return Error(TEXT("Vertex attribute editable node required")); }
    const auto* Editable=static_cast<const FDataflowVertexAttributeEditableNode*>(Match.Get()); FObject R=Result();
    const auto Snapshots=Editable->GetSnapshots(); const auto* Active=Editable->GetActiveSnapshot(); TArray<TSharedPtr<FJsonValue>> Rows;
    for(int32 Index=0;Index<Snapshots.Num() && Index<Limit(P);++Index)
    {
        const auto& Snapshot=Snapshots[Index]; FObject Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("index"),Index); Row->SetBoolField(TEXT("active"),&Snapshot==Active); Row->SetBoolField(TEXT("locked"),Snapshot.IsLocked()); Row->SetStringField(TEXT("name"),Snapshot.Name); Row->SetStringField(TEXT("description"),Snapshot.Description); Row->SetStringField(TEXT("date"),Snapshot.GetDate().ToIso8601());
        const auto* Weights=Snapshot.Data.FindAttributeTyped<float>(TEXT("Weights"),TEXT("Vertices")); Row->SetNumberField(TEXT("weight_count"),Weights ? Weights->Num() : 0);
        const auto* Positions=Snapshot.Data.FindAttributeTyped<FVector3f>(TEXT("Positions"),TEXT("Vertices")); const auto* Indices=Snapshot.Data.FindAttributeTyped<FIntVector>(TEXT("Indices"),TEXT("Faces")); Row->SetBoolField(TEXT("has_topology"),Positions && Indices); Row->SetNumberField(TEXT("vertex_count"),Positions ? Positions->Num() : 0); Row->SetNumberField(TEXT("face_count"),Indices ? Indices->Num() : 0); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetNumberField(TEXT("snapshot_count"),Snapshots.Num()); R->SetNumberField(TEXT("active_index"),Active ? Active-Snapshots.GetData() : INDEX_NONE); R->SetBoolField(TEXT("truncated"),Rows.Num()<Snapshots.Num()); R->SetArrayField(TEXT("snapshots"),Rows); return R;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubelessDomainInspectionPCGRollback,
    "CubelessMCP.DomainExpansion.PCGRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCubelessDomainInspectionPCGRollback::RunTest(const FString& Parameters)
{
    UPackage* Package=CreatePackage(*FString::Printf(TEXT("/Game/_MCP_Temp/Automation/PCG_%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UPCGGraph* Graph=NewObject<UPCGGraph>(Package,*FPackageName::GetLongPackageAssetName(Package->GetName()),RF_Public|RF_Standalone|RF_Transactional);
    UClass* Class=LoadClass<UPCGSettings>(nullptr,TEXT("/Script/PCG.PCGCustomHLSLSettings")); UPCGSettings* Settings=nullptr;
    if(!TestNotNull(TEXT("Custom HLSL class"),Class)) { return false; }
    UPCGNode* Node=Graph->AddNodeOfType(Class,Settings);
    if(!TestNotNull(TEXT("Custom HLSL node"),Node)) { return false; }
    Package->SetDirtyFlag(false);
    FObject P=MakeShared<FJsonObject>(), Counts=MakeShared<FJsonObject>(); P->SetStringField(TEXT("asset_path"),Package->GetName()); P->SetStringField(TEXT("node_name"),Node->GetName()); P->SetBoolField(TEXT("dry_run"),true); Counts->SetNumberField(TEXT("num_elements"),512); P->SetObjectField(TEXT("counts"),Counts);
    FObject Before=PCGCounts(P); if(!TestTrue(TEXT("Dry run succeeds"),Before->GetBoolField(TEXT("success")))) { return false; }
    P->SetObjectField(TEXT("expected"),Before->GetObjectField(TEXT("before"))); P->SetBoolField(TEXT("dry_run"),false);
    const FObject Failed=PCGCounts(P,true); // Internal fault injection is not admitted by the public JSON route.
    TestFalse(TEXT("Forced readback failure"),Failed->GetBoolField(TEXT("success")));
    TestTrue(TEXT("Actual rollback branch restores values and package dirty state"),Failed->GetBoolField(TEXT("rollback_verified")));
    TestFalse(TEXT("Clean package restored"),Package->IsDirty());
    TestEqual(TEXT("Restored counts"),Serialize(Failed->GetObjectField(TEXT("after"))),Serialize(Before->GetObjectField(TEXT("before"))));
    const FObject Applied=PCGCounts(P); TestTrue(TEXT("Normal apply succeeds"),Applied->GetBoolField(TEXT("success"))); TestTrue(TEXT("Normal apply dirty"),Package->IsDirty());
    UPackage* ExternalPackage=CreatePackage(*FString::Printf(TEXT("/Game/_MCP_Temp/Automation/Shared_%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UPCGSettings* External=NewObject<UPCGSettings>(ExternalPackage,Class);
    UPCGSettingsInstance* Instance=NewObject<UPCGSettingsInstance>(Node); Instance->SetSettings(External); Node->SetSettingsInterface(Instance,false); ExternalPackage->SetDirtyFlag(false);
    const FObject Denied=PCGCounts(P);
    TestFalse(TEXT("External shared settings rejected"),Denied->GetBoolField(TEXT("success")));
    TestFalse(TEXT("External package untouched"),ExternalPackage->IsDirty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubelessDomainInspectionStatelessInspection,
    "CubelessMCP.DomainExpansion.StatelessInspection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCubelessDomainInspectionStatelessInspection::RunTest(const FString& Parameters)
{
    UNiagaraSystem* System=NewObject<UNiagaraSystem>(GetTransientPackage());
    UClass* Class=LoadClass<UObject>(nullptr,TEXT("/Script/Niagara.NiagaraStatelessEmitter"));
    if(!TestNotNull(TEXT("Stock stateless class"),Class)) { return false; }
    UObject* Emitter=NewObject<UObject>(System,Class);
    FNiagaraEmitterHandle Handle;
    FObjectPropertyBase* Property=FindFProperty<FObjectPropertyBase>(FNiagaraEmitterHandle::StaticStruct(),TEXT("StatelessEmitter"));
    FEnumProperty* Mode=FindFProperty<FEnumProperty>(FNiagaraEmitterHandle::StaticStruct(),TEXT("EmitterMode"));
    if(!TestNotNull(TEXT("Emitter object property"),Property) || !TestNotNull(TEXT("Mode property"),Mode)) { return false; }
    Property->SetObjectPropertyValue_InContainer(&Handle,Emitter);
    Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(&Handle),static_cast<uint64>(ENiagaraEmitterMode::Stateless));
    Handle.SetIsEnabled(false,*System,false); System->AddEmitterHandleDirect(Handle);
    FObject P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("asset_path"),System->GetPathName());
    FObject R=Niagara(P);
    if(!TestTrue(TEXT("Stateless inspector succeeds"),R->GetBoolField(TEXT("success")))) { return false; }
    const auto Row=R->GetArrayField(TEXT("emitters"))[0]->AsObject();
    TestEqual(TEXT("Stateless mode"),String(Row,TEXT("mode")),FString(TEXT("stateless")));
    TestFalse(TEXT("Disabled handle included"),Row->GetBoolField(TEXT("enabled")));
    TestTrue(TEXT("Stateless module count present"),Row->HasField(TEXT("module_count")));
    // Preserve the old stateless object while reverting the execution mode, as stock does.
    auto& Stored=System->GetEmitterHandles()[0];
    Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(&Stored),static_cast<uint64>(ENiagaraEmitterMode::Standard));
    R=Niagara(P);
    const auto Standard=R->GetArrayField(TEXT("emitters"))[0]->AsObject();
    TestEqual(TEXT("Returned mode standard"),String(Standard,TEXT("mode")),FString(TEXT("standard")));
    TestFalse(TEXT("Stale stateless modules excluded"),Standard->HasField(TEXT("module_count")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubelessDomainInspectionSnapshotInspection,
    "CubelessMCP.DomainExpansion.SnapshotInspection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCubelessDomainInspectionSnapshotInspection::RunTest(const FString& Parameters)
{
    FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("ChaosClothAssetDataflowNodes"));
    UDataflow* Dataflow=NewObject<UDataflow>(GetTransientPackage());
    UE::Dataflow::FNewNodeParameters Params{FGuid::NewGuid(),TEXT("FChaosClothAssetWeightMapNode"),TEXT("SnapshotProbe"),Dataflow};
    auto Node=UE::Dataflow::FNodeFactory::GetInstance()->NewNodeFromRegisteredType(*Dataflow->GetDataflowGraph(),Params);
    if(!TestTrue(TEXT("Stock weight node"),Node.IsValid())) { return false; }
    TSharedRef<FManagedArrayCollection> Collection=MakeShared<FManagedArrayCollection>();
    UE::Chaos::ClothAsset::FCollectionClothFacade Cloth(Collection); Cloth.DefineSchema();
    Collection->AddElements(3,TEXT("SimVertices3D")); Collection->AddElements(1,TEXT("SimFaces"));
    Cloth.GetSimPosition3D()[0]=FVector3f(0,0,0); Cloth.GetSimPosition3D()[1]=FVector3f(1,0,0); Cloth.GetSimPosition3D()[2]=FVector3f(0,1,0); Cloth.GetSimIndices3D()[0]=FIntVector(0,1,2);
    const FStructProperty* Input=FindFProperty<FStructProperty>(Node->TypedScriptStruct(),TEXT("Collection"));
    if(!TestNotNull(TEXT("Collection input"),Input)) { return false; }
    *Input->ContainerPtrToValuePtr<FManagedArrayCollection>(Node.Get())=*Collection;
    auto* Editable=static_cast<FDataflowVertexAttributeEditableNode*>(Node.Get()); UE::Dataflow::FEngineContext Context(Dataflow);
    Editable->SetVertexAttributeValues(Context,TArray<float>{0.1f,0.2f,0.3f},{});
    Editable->SetVertexAttributeValues(Context,TArray<float>{0.4f,0.5f,0.6f},{});
    FObject P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("asset_path"),Dataflow->GetPathName()); P->SetStringField(TEXT("node_name"),TEXT("SnapshotProbe"));
    FObject R=ClothSnapshots(P);
    if(!TestTrue(TEXT("Snapshot inspector succeeds"),R->GetBoolField(TEXT("success")))) { return false; }
    TestEqual(TEXT("Two history entries"),R->GetIntegerField(TEXT("snapshot_count")),2);
    TestTrue(TEXT("Active selection available"),R->GetIntegerField(TEXT("active_index"))>=0);
    for(const auto& Value:R->GetArrayField(TEXT("snapshots"))) { const auto Row=Value->AsObject(); TestEqual(TEXT("Three weights"),Row->GetIntegerField(TEXT("weight_count")),3); TestTrue(TEXT("Topology retained"),Row->GetBoolField(TEXT("has_topology"))); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubelessDomainInspectionBodyOverridesRoundTrip,
    "CubelessMCP.DomainExpansion.BodyOverridesRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCubelessDomainInspectionBodyOverridesRoundTrip::RunTest(const FString& Parameters)
{
    FBodyInstance Body;
    ApplyBodySettings(Body,7,3,2,false,false);
    const FObject Before=BodyState(Body);
    TestEqual(TEXT("Disabled override sentinel"),Body.GetPositionSolverIterationCount(),-1);
    TestEqual(TEXT("Hidden raw value retained"),Body.PositionSolverIterationCount,static_cast<uint8>(7));
    ApplyBodySettings(Body,11,5,1,true,true);
    TestEqual(TEXT("Enabled override value"),Body.GetPositionSolverIterationCount(),11);
    TestTrue(TEXT("Partial island sleep configured"),Body.GetAllowPartialIslandSleep());
    TestNotEqual(TEXT("State hash changes"),String(Before,TEXT("state_hash")),String(BodyState(Body),TEXT("state_hash")));
    ApplyBodySettings(Body,7,3,2,false,false); // Same native helper used by the production rollback branch.
    TestEqual(TEXT("Rollback restores all raw settings and flags"),String(BodyState(Body),TEXT("state_hash")),String(Before,TEXT("state_hash")));
    TestEqual(TEXT("Rollback restores effective sentinel"),Body.GetProjectionSolverIterationCount(),-1);
    UPackage* Package=CreatePackage(*FString::Printf(TEXT("/Game/_MCP_Temp/Automation/Body_%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    Package->SetDirtyFlag(true);
    TestTrue(TEXT("Verified rollback restores clean state"),RestorePackageAfterRollback(*Package,false,true));
    TestFalse(TEXT("Package clean after verified rollback"),Package->IsDirty());
    TestTrue(TEXT("Preexisting dirty state preserved"),RestorePackageAfterRollback(*Package,true,true));
    TestFalse(TEXT("Unverified rollback never clears dirty state"),RestorePackageAfterRollback(*Package,false,false));
    TestTrue(TEXT("Dirty retained after unverified rollback"),Package->IsDirty());
    return true;
}
#endif
#endif
}


void UCubelessDomainExpansionLibrary::Deinitialize()
{
    Cubeless::UE58::DomainExpansion::ShutdownNiagara();
    Cubeless::UE58::DomainExpansion::Shutdown();
    Super::Deinitialize();
}

FString UCubelessDomainExpansionLibrary::ExecuteDomainRequest(const FString& RequestJson)
{
    using namespace CubelessDomainInspection;
    static bool Running=false;
    if(Running || !IsInGameThread() || IsGarbageCollecting() || IsEngineExitRequested() || !GEditor || IsRunningCommandlet() || UE::IsSavingPackage())
        return CubelessDomainInspection::Serialize(CubelessDomainInspection::Error(TEXT("An idle, non-reentrant Editor game thread is required")));
    TGuardValue<bool> Active(Running,true);
    if(RequestJson.Len()>65536) return CubelessDomainInspection::Serialize(CubelessDomainInspection::Error(TEXT("Request too large")));
    FObject P;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RequestJson),P) || !P)
        return CubelessDomainInspection::Serialize(CubelessDomainInspection::Error(TEXT("Invalid JSON object")));
    // Validate all path carriers before LoadObject/CreatePackage can intern an FName.
    TFunction<bool(const TSharedPtr<FJsonValue>&,int32)> BoundedPaths;
    BoundedPaths=[&](const TSharedPtr<FJsonValue>& V,int32 Depth)
    {
        if(!V.IsValid() || Depth>32) return false;
        if(V->Type==EJson::String)
        {
            const FString S=V->AsString();
            for(TCHAR C:S) if(C==0) return false;
            // LoadObject accepts Class'/Package.Object'. Strip that purely textual
            // carrier before checking the path token; no FName/engine lookup here.
            const FString Path=FPackageName::ExportTextPathToObjectPath(S);
            if(Path.StartsWith(TEXT("/")))
            {
                if(Path.Len()>=NAME_SIZE-2) return false;
                if(!Path.Contains(TEXT(".")) && (Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path)).Len()>=NAME_SIZE-1) return false;
            }
        }
        else if(V->Type==EJson::Object) { for(const auto& E:V->AsObject()->Values) if(!BoundedPaths(E.Value,Depth+1)) return false; }
        else if(V->Type==EJson::Array) { for(const auto& E:V->AsArray()) if(!BoundedPaths(E,Depth+1)) return false; }
        return true;
    };
    if(!BoundedPaths(MakeShared<FJsonValueObject>(P),0)) return CubelessDomainInspection::Serialize(CubelessDomainInspection::Error(TEXT("Bounded non-NUL paths and nesting required; no engine lookup attempted")));
    if(P->HasField(TEXT("dry_run")) && !P->HasTypedField<EJson::Boolean>(TEXT("dry_run"))) return CubelessDomainInspection::Serialize(CubelessDomainInspection::Error(TEXT("dry_run must be boolean")));
    if(P->HasField(TEXT("max_results")))
    {
        double Count=0;
        if(!P->TryGetNumberField(TEXT("max_results"),Count) || !FMath::IsFinite(Count) || Count<1 || Count>512 || FMath::FloorToDouble(Count)!=Count)
            return CubelessDomainInspection::Serialize(CubelessDomainInspection::Error(TEXT("max_results must be an integer in 1..512")));
    }
    const FString Op=String(P,TEXT("operation")); FObject R;
    if(Op.StartsWith(TEXT("authoring."))) R=Cubeless::UE58::DomainExpansion::Execute(P);
    else if(Op==TEXT("controlrig.catalog") || Op==TEXT("controlrig.recipe"))
    {
        P->SetStringField(TEXT("operation"),Op==TEXT("controlrig.catalog")?TEXT("catalog"):TEXT("recipe"));
        R=Cubeless::UE58::DomainExpansion::ExecuteRig(P);
    }
    else if(Op==TEXT("pcg_custom_hlsl_elements")) R=PCGCounts(P);
    else if(Op==TEXT("niagara_emitter_execution")) R=Niagara(P);
    else if(Op==TEXT("animbp_compiled_layout")) R=AnimBP(P);
    else if(Op==TEXT("controlrig_physics_units")) R=Rig(P);
    else if(Op==TEXT("chaos_body")) R=Body(P,false);
    else if(Op==TEXT("chaos_body_configure")) R=Body(P,true);
    else if(Op==TEXT("cloth_solver")) R=ClothSolver(P);
    else if(Op==TEXT("cloth_snapshots")) R=ClothSnapshots(P);
    else R=CubelessDomainInspection::Error(TEXT("Unsupported domain operation"));
    R->SetStringField(TEXT("operation"),Op); R->SetStringField(TEXT("engine_api"),TEXT("UE5.8.3"));
    return CubelessDomainInspection::Serialize(R);
}
