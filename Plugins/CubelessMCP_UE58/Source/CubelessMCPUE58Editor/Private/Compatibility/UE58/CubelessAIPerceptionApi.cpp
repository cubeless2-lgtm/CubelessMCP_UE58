// Original native11 ConfigureAIControllerPerception; public AIModule/SCS/UnrealEd only.
// SensesConfig/listener caches are never written directly. No custom runtime class or asset call node.
#include "Compatibility/UE58/CubelessAIPerceptionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AIController.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Components/ActorComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::AIPerception
{
namespace
{
constexpr int32 MaxProperties = 256, MaxText = 1048576, MaxNodes = 128, MaxGraphs = 64, MaxPins = 4096, MaxPackages = 4096, MaxSenses = 16;
constexpr int64 MaxDiskBytes = 32 * 1024 * 1024;
struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false;
    bool bCompileRequested = false, bCompileAttempted = false, bCompileKnown = false, bCompiled = false;
    bool bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool Fail(FResult& R, const TCHAR* Code, const FString& Message, bool bUnknown = false)
{ R.Code = Code; R.Error = Message; R.bSuccess = false; R.bPost = false; if (bUnknown) { R.bKnown = false; } return false; }
void Init(FResult& R, bool bConfigure, bool bSave)
{
    R.Operation = bConfigure ? TEXT("configure_ai_controller_perception") : TEXT("inspect_ai_controller_perception"); R.bCompileRequested = bConfigure; R.bSaveRequested = bSave;
    for (const TCHAR* K : { TEXT("component_created"), TEXT("sight_config_created"), TEXT("source_preserved"), TEXT("unselected_state_preserved"),
        TEXT("observations_complete"), TEXT("readback_confirmed"), TEXT("blueprint_saved"), TEXT("uses_unrealmcp_classes"), TEXT("runtime_sight_detection_verified"),
        TEXT("implicit_engine_work_measured"), TEXT("implicit_skeleton_compile_possible"), TEXT("implicit_world_callback_effects_possible"), TEXT("asset_load_dispatched") }) { R.Data->SetBoolField(K, false); }
    for (const TCHAR* K : { TEXT("component_allocation_count"), TEXT("sight_config_allocation_count"), TEXT("modify_calls"), TEXT("configure_sense_calls"),
        TEXT("set_dominant_sense_calls"), TEXT("listener_update_calls"), TEXT("structural_refresh_calls"), TEXT("full_compile_attempts"), TEXT("package_dirty_mark_calls"), TEXT("save_attempt_count"), TEXT("asset_load_count") }) { R.Data->SetNumberField(K, 0); }
    R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public AIModule, Engine SCS, UnrealEd compiler and EditorAssetLibrary; no official AI authoring delegate or ServerRPC"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("loaded AIController Blueprint, exact own local unregistered native perception template and Sight config; current clean generated parents are read-only; structural skeleton regeneration and one explicit full compile can reinstance selected classes/defaults and dispatch callbacks; no actor sensing/navigation/gameplay execution"));
    R.Data->SetStringField(TEXT("read_scope"), TEXT("bounded reflected native defaults, local SCS/template/sense config identities and values, stored graphs/declarations, target disk and other package dirty states; private listener/perceptual caches and all implicit callbacks are not exhaustively observed"));
}
FString Encode(FResult& R)
{
    R.Data->SetBoolField(TEXT("save_blueprint_requested"), R.bSaveRequested); R.Data->SetBoolField(TEXT("blueprint_saved"), R.bSaved);
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), R.Operation); J->SetStringField(TEXT("stage"), R.Stage); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("success"), R.bSuccess); J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown); J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.bPost); J->SetBoolField(TEXT("compile_requested"), R.bCompileRequested); J->SetBoolField(TEXT("compile_attempted"), R.bCompileAttempted);
    if (R.bCompileKnown) { J->SetBoolField(TEXT("compiled"), R.bCompiled); } else { J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false); J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
// YO: use only installed public resolved-path/shallow equality APIs. Get()
// is a tracked read; it is not a raw/no-callback pointer extraction operation.
template<typename T>
UObject* PerceptionObservedPointer(const TObjectPtr<T>& Ref)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
    if(!IsEditorGameThread()||!Ref.IsResolved()||Ref==nullptr)return nullptr;
    const FString ObjectPath=Ref.GetPathName();UObject* Found=StaticFindObject(nullptr,nullptr,*ObjectPath);
    return Loaded(Found)&&Ref==Found?Found:nullptr;
#else
    return nullptr;
#endif
}
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"), O ? O->GetPathName() : TEXT("")); J->SetStringField(TEXT("class"), O ? O->GetClass()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("outer"), O && O->GetOuter() ? O->GetOuter()->GetPathName() : TEXT("")); J->SetStringField(TEXT("object_flags"), O ? FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags())) : TEXT("0")); return J;
}
bool Properties(UObject* O, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(O)) { return false; } int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (Skip.Contains(P->GetFName())) { continue; } if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64) { return false; }
        auto Row = MakeShared<FJsonObject>(); J->SetObjectField(P->GetName(), Row); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName());
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(P->GetPropertyFlags()))); TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I) { FString S; const void* V = P->ContainerPtrToValuePtr<void>(O, I); if (!P->ExportText_Direct(S, V, V, O, PPF_None) || (Chars += S.Len()) > MaxText) { return false; } Values.Add(MakeShared<FJsonValueString>(S)); }
        Row->SetArrayField(TEXT("values"), Values);
    }
    return true;
}
bool Name(const FString& S)
{ FText Reason; return !S.IsEmpty() && S.Len() < NAME_SIZE && !FName(*S).IsNone() && !S.Contains(TEXT(" ")) && FName::IsValidXName(S, INVALID_OBJECTNAME_CHARACTERS, &Reason); }
FString Path(FString S)
{ S.TrimStartAndEndInline(); S.ReplaceInline(TEXT("\\"), TEXT("/")); if (!S.Contains(TEXT("."))) { S += TEXT(".") + FPackageName::GetShortName(S); } return S; }
bool Native(UClass* C, const FString& Package)
{ return Loaded(C) && C->HasAnyClassFlags(CLASS_Native) && !C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) && C->GetOutermost()->GetName() == Package && C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn) && !C->GetOutermost()->IsDirty(); }
bool Sources(TSharedRef<FJsonObject>& J)
{
    const TCHAR* Paths[] = { TEXT("/Script/AIModule.AIController"), TEXT("/Script/AIModule.AIPerceptionComponent"), TEXT("/Script/AIModule.AISenseConfig_Sight"), TEXT("/Script/AIModule.AISense_Sight"),
        TEXT("/Script/Engine.Controller"), TEXT("/Script/Engine.Actor"), TEXT("/Script/Engine.ActorComponent"), TEXT("/Script/Engine.Blueprint"), TEXT("/Script/Engine.SimpleConstructionScript"), TEXT("/Script/Engine.SCS_Node") };
    for (const TCHAR* ClassPath : Paths)
    {
        UClass* C = FindObject<UClass>(nullptr, ClassPath); auto Row = Identity(C); J->SetObjectField(ClassPath, Row);
        if (!C || !Native(C, FPackageName::ObjectPathToPackageName(FString(ClassPath)))) { J->SetStringField(TEXT("failed_class"), ClassPath); J->SetStringField(TEXT("failed_stage"), TEXT("native_class")); return false; }
        Row->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags()))); UObject* CDO = C->GetDefaultObject(false); Row->SetBoolField(TEXT("cdo_exists"), CDO != nullptr);
        // Find-only: absent native defaults are recorded, never warmed by this reader.
        if (!CDO) { continue; }
        auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); Row->SetObjectField(TEXT("existing_cdo_properties"), P);
        if (!Loaded(CDO) || CDO->GetClass() != C || CDO->GetOuter() != C->GetOuter() || !CDO->HasAnyFlags(RF_ClassDefaultObject) || !Properties(CDO, {}, P))
        { J->SetStringField(TEXT("failed_class"), ClassPath); J->SetStringField(TEXT("failed_stage"), TEXT("existing_cdo_properties")); return false; }
    }
    return true;
}
bool Disk(const FString& PackageName, TSharedRef<FJsonObject>& J)
{
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename); J->SetStringField(TEXT("path"), Filename); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size));
    if (Size < 0) { J->SetStringField(TEXT("sha1"), TEXT("")); return true; } TArray<uint8> Bytes;
    if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; } uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& PackageName, TSharedRef<FJsonObject>& J)
{ int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == PackageName) { continue; } if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetPathName(), It->IsDirty()); } return true; }
bool CleanDependencyText(const FString& S)
{ return !S.Contains(TEXT("/Script/Cubeless"), ESearchCase::IgnoreCase) && !S.Contains(TEXT("/Script/UnrealMCP"), ESearchCase::IgnoreCase) && !S.Contains(TEXT("/Engine/PythonTypes"), ESearchCase::IgnoreCase) && !S.Contains(TEXT("/Game/UnrealMCP"), ESearchCase::IgnoreCase); }
bool ReadSight(const UAISenseConfig_Sight* S, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(S) || S->GetClass() != UAISenseConfig_Sight::StaticClass() || S->GetSenseImplementation() != UAISense_Sight::StaticClass()) { return false; }
    auto* RawAge = FindFProperty<FFloatProperty>(S->GetClass(), TEXT("MaxAge")); if (!RawAge || RawAge->ArrayDim != 1 || !RawAge->HasAnyPropertyFlags(CPF_Edit)) { return false; }
    const float Stored = RawAge->GetPropertyValue_InContainer(S); if (!FMath::IsFinite(Stored) || Stored < 0 || !FMath::IsFinite(S->GetMaxAge())) { return false; }
    J->SetObjectField(TEXT("identity"), Identity(S)); J->SetStringField(TEXT("implementation"), S->GetSenseImplementation()->GetPathName()); J->SetNumberField(TEXT("sight_radius"), S->SightRadius); J->SetNumberField(TEXT("lose_sight_radius"), S->LoseSightRadius);
    J->SetNumberField(TEXT("peripheral_vision_angle_degrees"), S->PeripheralVisionAngleDegrees); J->SetNumberField(TEXT("stored_max_age"), Stored); J->SetNumberField(TEXT("effective_max_age"), S->GetMaxAge()); J->SetBoolField(TEXT("max_age_means_never"), Stored == 0);
    J->SetBoolField(TEXT("starts_enabled"), S->GetStartsEnabled()); J->SetNumberField(TEXT("auto_success_range_from_last_seen_location"), S->AutoSuccessRangeFromLastSeenLocation);
    auto A = MakeShared<FJsonObject>(); A->SetBoolField(TEXT("detect_enemies"), S->DetectionByAffiliation.bDetectEnemies); A->SetBoolField(TEXT("detect_neutrals"), S->DetectionByAffiliation.bDetectNeutrals); A->SetBoolField(TEXT("detect_friendlies"), S->DetectionByAffiliation.bDetectFriendlies); J->SetObjectField(TEXT("detection_by_affiliation"), A);
    auto Other = MakeShared<FJsonObject>(); if (!Properties(const_cast<UAISenseConfig_Sight*>(S), { TEXT("Implementation"), TEXT("SightRadius"), TEXT("LoseSightRadius"), TEXT("PeripheralVisionAngleDegrees"), TEXT("DetectionByAffiliation"), TEXT("AutoSuccessRangeFromLastSeenLocation"), TEXT("MaxAge"), TEXT("bStartsEnabled") }, Other)) { return false; }
    J->SetObjectField(TEXT("unselected_properties"), Other); return CleanDependencyText(Text(Other));
}
bool ReadComponent(UAIPerceptionComponent* C, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(C) || C->GetClass() != UAIPerceptionComponent::StaticClass() || C->IsRegistered() || C->GetOwner() || C->GetWorld()) { return false; }
    J->SetObjectField(TEXT("identity"), Identity(C)); J->SetBoolField(TEXT("registered"), false); J->SetStringField(TEXT("dominant_sense"), C->GetDominantSense() ? C->GetDominantSense()->GetPathName() : TEXT(""));
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<const UAISenseConfig*> Unique; int32 SightCount = 0; J->SetArrayField(TEXT("sense_configs"), Rows);
    // No live sense-array iterator spans reflected ExportText/read callbacks.
    // Strong holds protect copied entries; actual ordered membership is checked
    // again before the observation is accepted, not fixed across compilation.
    // GetSensesConfigIterator itself uses the SDK ToRawPtrTArrayUnsafe view:
    // its internal tracked reads/callback atomicity are not proven by this copy.
    TStrongObjectPtr<UAIPerceptionComponent> KeepComponent(C);TArray<TStrongObjectPtr<const UAISenseConfig>> SenseHolds;
    TArray<const UAISenseConfig*> SenseRows;
    for(auto It=C->GetSensesConfigIterator();It;++It)
    {if(SenseRows.Num()>=MaxSenses)return false;SenseRows.Add(*It);}
    for(const auto* Sense:SenseRows){if(!Loaded(Sense))return false;SenseHolds.Emplace(Sense);}
    for (const UAISenseConfig* S:SenseRows)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("identity"), Identity(S)); Rows.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("sense_configs"), Rows);
        if (!Loaded(S) || S->GetOuter() != C || Unique.Contains(S) || Unique.Num() >= MaxSenses || S->GetClass()->GetOutermost()->GetName() != TEXT("/Script/AIModule") || !S->GetClass()->HasAnyClassFlags(CLASS_Native)) { return false; } Unique.Add(S);
        if (S->GetClass() == UAISenseConfig_Sight::StaticClass()) { if (++SightCount > 1 || !ReadSight(CastChecked<UAISenseConfig_Sight>(S), Row)) { return false; } }
        else { auto Props = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("properties"), Props); if (!Properties(const_cast<UAISenseConfig*>(S), {}, Props) || !CleanDependencyText(Text(Props))) { return false; } }
    }
    J->SetNumberField(TEXT("sense_config_count"), Rows.Num()); J->SetBoolField(TEXT("sight_exists"), SightCount == 1);
    auto Props = MakeShared<FJsonObject>(); if (!Properties(C, { TEXT("SensesConfig"), TEXT("DominantSense") }, Props)) { return false; } J->SetObjectField(TEXT("unselected_properties"), Props);
    if(!Loaded(C)||C->GetClass()!=UAIPerceptionComponent::StaticClass()||C->IsRegistered()||C->GetOwner()||C->GetWorld())return false;
    int32 At=0;for(auto It=C->GetSensesConfigIterator();It;++It,++At)
    {if(!SenseRows.IsValidIndex(At)||*It!=SenseRows[At]||!Loaded(SenseRows[At])||SenseRows[At]->GetOuter()!=C)return false;}
    return At==SenseRows.Num();
}
// XA: request-local observation of ordinary editor instances affected by the original compiler.
// No live setting writer, construction override, level save or runtime helper class is introduced.
struct FLivePerceptionInstances
{
    struct FActorRole{TStrongObjectPtr<UObject> Current;TStrongObjectPtr<ULevel> Level;TStrongObjectPtr<UPackage> Package,External,LevelPackage;FName Name;FString Path,LevelPath;FGuid Guid;FTransform Transform;TSharedPtr<FJsonObject> Authored;};
    FResult& R;TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UClass> CurrentClass;TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    TArray<FActorRole> Actors;TArray<TStrongObjectPtr<UObject>> Watched,Retained;TSet<UObject*> KnownKeys;TStrongObjectPtr<UPackage> BlueprintPackage;TStrongObjectPtr<UClass> BlueprintParent;FString BlueprintPath;TSet<FString> DirtyPackages;FDelegateHandle ReplacedHandle,ReinstancedHandle;TAtomic<bool> Lost{false};bool CompilerWindow=false,FailureReported=false;int32 ReplacementCount=0,ReinstancedCount=0,RetiredChildCount=0;FString ComponentVariable;
    explicit FLivePerceptionInstances(FResult& In):R(In){}
    ~FLivePerceptionInstances(){if(ReplacedHandle.IsValid())FCoreUObjectDelegates::OnObjectsReplaced.Remove(ReplacedHandle);if(ReinstancedHandle.IsValid())FCoreUObjectDelegates::OnObjectsReinstanced.Remove(ReinstancedHandle);}
    bool Stop(const TCHAR* Why)
    {if(!FailureReported){FailureReported=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("reason"),Why);R.Data->SetObjectField(TEXT("live_instance_first_failure"),J);}Lost.Store(true);R.Data->SetNumberField(TEXT("live_instance_replacement_map_entries"),ReplacementCount);R.Data->SetNumberField(TEXT("live_instance_reinstanced_notifications"),ReinstancedCount);R.Data->SetNumberField(TEXT("live_instance_retired_child_map_entries"),RetiredChildCount);R.Data->SetBoolField(TEXT("live_instance_followup_stopped"),true);return Fail(R,TEXT("perception_live_instance_current_lost"),Why,R.bAttempted);}
    bool Context()
    {return IsEditorGameThread()&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage()&&!IsPlaySessionActive()&&Loaded(Editor.Get())&&GEditor==Editor.Get()&&Loaded(World.Get())&&World->WorldType==EWorldType::Editor&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get()&&Loaded(Blueprint.Get())&&Blueprint->GetClass()==UBlueprint::StaticClass()&&Loaded(BlueprintPackage.Get())&&Loaded(BlueprintParent.Get())&&Blueprint->GetOutermost()==BlueprintPackage.Get()&&Blueprint->GetPathName()==BlueprintPath&&Blueprint->ParentClass==BlueprintParent.Get()&&!Blueprint->bBeingCompiled&&!Blueprint->bQueuedForCompilation&&!Blueprint->bIsRegeneratingOnLoad;}
    bool KeepMapped(UObject* O)
    {if(!O||KnownKeys.Contains(O))return true;if(Retained.Num()>=8192){Lost.Store(true);return false;}KnownKeys.Add(O);Retained.Emplace(O);return true;}
    void Map(const FCoreUObjectDelegates::FReplacementObjectMap& Pairs,bool Reinstanced)
    {
        if(!IsInGameThread()){Lost.Store(true);return;} // No UObject queries on an unexpected callback thread.
        bool Relevant=false;for(auto* Key:KnownKeys)if(const auto* Next=Pairs.Find(Key))Relevant|=*Next!=Key;
        if(!Relevant)return;if(!CompilerWindow){Lost.Store(true);return;}if(Reinstanced)++ReinstancedCount;
        // Map collection/strong holds only: replaced notifications precede external reference fixup.
        for(auto& A:Actors)if(const auto* Next=Pairs.Find(A.Current.Get()))if(*Next!=A.Current.Get())
        {if(!*Next||!KeepMapped(*Next)){Lost.Store(true);return;}A.Current.Reset(*Next);++ReplacementCount;}
        for(auto& O:Watched)if(O.Get())if(const auto* Next=Pairs.Find(O.Get()))if(*Next!=O.Get())
        {if(!*Next){++RetiredChildCount;O.Reset();continue;}if(!KeepMapped(*Next))return;O.Reset(*Next);++ReplacementCount;}
    }
    bool Current()
    {
        if(Lost.Load())return Stop(TEXT("Replacement was null/outside the admitted compiler window or callback thread."));
        if(!Context()||!Loaded(CurrentClass.Get())||!Blueprint->GeneratedClass.GetGCPtr().IsResolved()||Blueprint->GeneratedClass.GetGCPtr().Get()!=CurrentClass.Get()||!CurrentClass->ClassGeneratedBy.IsResolved()||CurrentClass->ClassGeneratedBy.Get()!=Blueprint.Get()||CurrentClass->GetSuperClass()!=BlueprintParent.Get())return Stop(TEXT("Editor/world/selected Blueprint generated class role changed."));
        for(const auto& E:Actors)
        {
            auto* O=E.Current.Get();if(!Loaded(O)||O->GetClass()!=CurrentClass.Get()||O->GetOuter()!=E.Level.Get()||O->GetFName()!=E.Name||O->GetPathName()!=E.Path||O->GetPackage()!=E.Package.Get()||!Loaded(E.Level.Get())||!Loaded(E.LevelPackage.Get())||E.Level->GetPackage()!=E.LevelPackage.Get()||E.Level->GetPathName()!=E.LevelPath||!Loaded(E.Package.Get())||(E.External.IsValid()&&!Loaded(E.External.Get())))return Stop(TEXT("Observed current instance class/name/path/level/package changed outside the original replacement role."));
            auto* A=static_cast<AAIController*>(O);if(A->IsActorBeingDestroyed()||!E.Level->Actors.Contains(A)||A->IsTemplate()||A->GetWorld()!=World.Get()||A->GetLevel()!=E.Level.Get()||E.Level->GetWorld()!=World.Get()||A->GetExternalPackage()!=E.External.Get()||A->GetActorGuid()!=E.Guid||A->GetPawn()||A->GetOwner()||A->GetInstigator()||A->GetAttachParentActor()||A->IsChildActor()||!static_cast<AActor*>(A)->GetActorTransform().Equals(E.Transform,0))return Stop(TEXT("Editor AIController authored placement/possession/ownership role changed; no further SDK."));
        }return true;
    }
    bool CompilerReturned()
    {
        if(Lost.Load()||!Context())return Stop(TEXT("Compiler returned without the captured live instance context."));auto Handle=Blueprint->GeneratedClass;if(!Handle.GetGCPtr().IsResolved())return Stop(TEXT("Compiler generated class slot is unresolved."));auto* K=Handle.GetGCPtr().Get();
        if(!Loaded(K)||K->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=Blueprint.Get()||K->GetOuter()!=Blueprint->GetOutermost()||K->GetSuperClass()!=BlueprintParent.Get())return Stop(TEXT("Compiler returned class is not the selected Blueprint's generated role."));CurrentClass.Reset(K);return Current();
    }
    bool Authored(AAIController* A,TSharedRef<FJsonObject>& Out)
    {
        if(!Current())return false;TStrongObjectPtr<UClass> Type(A->GetClass());TArray<FProperty*> Fields;for(TFieldIterator<FProperty> It(Type.Get(),EFieldIteratorFlags::IncludeSuper);It;++It)Fields.Add(*It);if(Fields.Num()>MaxProperties)return Stop(TEXT("Instance property inventory exceeds the existing finite bound."));int32 Chars=0;
        for(auto* P:Fields)
        {
            if(!Current())return false;
            if(!P->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditConst|CPF_InstancedReference|CPF_ContainsInstancedReference)||CastField<FObjectPropertyBase>(P)||P->GetFName()==FName(*ComponentVariable)||(P->GetOwnerStruct()==AAIController::StaticClass()&&P->GetFName()==TEXT("PerceptionComponent")))continue;
            if(P->ArrayDim<1||P->ArrayDim>64||P->HasSetterOrGetter())return Stop(TEXT("Instance authored property layout/accessor is outside this profile."));const FString Key=P->GetName();TArray<TSharedPtr<FJsonValue>> Values;
            const int32 Dim=P->ArrayDim,Offset=P->GetOffset_ForInternal(),Size=P->GetSize();const auto Flags=P->GetPropertyFlags();const auto* Kind=P->GetClass();
            for(int32 I=0;I<Dim;++I)
            {
                FString Value;const bool OK=P->ExportText_Direct(Value,P->ContainerPtrToValuePtr<void>(A,I),nullptr,A,PPF_None);if(!Current())return false;
                TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(Type.Get(),EFieldIteratorFlags::IncludeSuper);It;++It)Now.Add(*It);if(Now!=Fields)return Stop(TEXT("Instance authored descriptor inventory changed during export."));
                if(P->ArrayDim!=Dim||P->GetOffset_ForInternal()!=Offset||P->GetSize()!=Size||P->GetClass()!=Kind||P->GetPropertyFlags()!=Flags||P->HasSetterOrGetter())return Stop(TEXT("Instance authored descriptor role changed during export."));
                if(!OK||(Chars+=Value.Len())>MaxText)return Stop(TEXT("Instance authored property export is incomplete."));Values.Add(MakeShared<FJsonValueString>(Value));
            }Out->SetArrayField(Key,Values);
        }return Current();
    }
    bool Observe(const TCHAR* Phase)
    {
        if(Actors.IsEmpty())return true;if(!Current())return false;TArray<TSharedPtr<FJsonValue>> Rows;
        for(auto& E:Actors)
        {
            auto* A=static_cast<AAIController*>(E.Current.Get());auto Properties=MakeShared<FJsonObject>();if(!Authored(A,Properties))return false;
            if(E.Authored.IsValid()&&Text(E.Authored.ToSharedRef())!=Text(Properties))
            {
                auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("actor"),E.Path);M->SetStringField(TEXT("stage"),R.Stage);
                for(const auto& P:E.Authored->Values){const auto Actual=Properties->TryGetField(FString(P.Key));if(!Actual||!FJsonValue::CompareEqual(*P.Value,*Actual)){M->SetStringField(TEXT("property"),FString(P.Key));M->SetField(TEXT("expected"),P.Value);M->SetField(TEXT("actual"),Actual?Actual:MakeShared<FJsonValueNull>());break;}}R.Data->SetObjectField(TEXT("live_instance_first_authored_mismatch"),M);return Stop(TEXT("Instance authored properties changed outside the selected perception/compiler-derived roles."));
            }if(!E.Authored.IsValid())E.Authored=Properties;
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),E.Path);J->SetStringField(TEXT("level"),E.Level->GetPathName());J->SetStringField(TEXT("package"),E.Package->GetName());J->SetBoolField(TEXT("package_dirty"),E.Package->IsDirty());J->SetBoolField(TEXT("level_package_dirty"),E.Level->GetPackage()->IsDirty());J->SetObjectField(TEXT("unselected_authored_properties"),Properties);
            TInlineComponentArray<UActorComponent*> Components(A);TArray<TSharedPtr<FJsonValue>> Perceptions;for(auto* C:Components)if(Loaded(C)&&C->GetClass()==UAIPerceptionComponent::StaticClass()){auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),C->GetName());P->SetBoolField(TEXT("sight_observed"),false);P->SetStringField(TEXT("sight_observation_scope"),TEXT("Optional live Sight values are not exported or required for success; original template/SCS readback remains authoritative. Per-instance overrides and sensing behavior require usage validation."));Perceptions.Add(MakeShared<FJsonValueObject>(P));}if(!Current())return false;J->SetArrayField(TEXT("actual_perception_components"),Perceptions);J->SetBoolField(TEXT("live_values_forced_to_template"),false);Rows.Add(MakeShared<FJsonValueObject>(J));
        }
        const TSharedPtr<FJsonObject>* Existing=nullptr;TSharedPtr<FJsonObject> Reports=R.Data->TryGetObjectField(TEXT("live_instance_observations"),Existing)?*Existing:MakeShared<FJsonObject>();Reports->SetArrayField(Phase,Rows);R.Data->SetObjectField(TEXT("live_instance_observations"),Reports);R.Data->SetNumberField(TEXT("live_instance_replacement_map_entries"),ReplacementCount);R.Data->SetNumberField(TEXT("live_instance_reinstanced_notifications"),ReinstancedCount);R.Data->SetNumberField(TEXT("live_instance_retired_child_map_entries"),RetiredChildCount);return true;
    }
    bool Bind(UBlueprint* B,const FString& Variable)
    {
        Blueprint.Reset(B);BlueprintPackage.Reset(B->GetOutermost());BlueprintParent.Reset(B->ParentClass);BlueprintPath=B->GetPathName();CurrentClass.Reset(B->GeneratedClass);Editor.Reset(GEditor);World.Reset(GEditor?GEditor->GetEditorWorldContext().World():nullptr);ComponentVariable=Variable;
        for(TObjectIterator<AActor> It;It;++It)
        {
            auto* A=*It;if(!Loaded(A)||A->IsTemplate()||!A->IsA(CurrentClass.Get()))continue;
            if(Actors.Num()>=32||A->GetClass()!=CurrentClass.Get()||!Context()||A->IsActorBeingDestroyed()||A->GetWorld()!=World.Get()||!Loaded(A->GetLevel())||A->GetOuter()!=A->GetLevel()||!A->GetLevel()->Actors.Contains(A))return Stop(TEXT("Live actor must be an exact selected-class ordinary current Editor-world AIController; broader contexts remain incomplete."));
            auto* Controller=static_cast<AAIController*>(A);if(Controller->GetPawn()||A->GetOwner()||A->GetInstigator()||A->GetAttachParentActor()||A->IsChildActor()||!A->GetActorGuid().IsValid())return Stop(TEXT("Possessed/owned/attached/child-actor instance profiles remain incomplete."));
            FActorRole E;E.Current.Reset(A);E.Level.Reset(A->GetLevel());E.LevelPackage.Reset(E.Level->GetPackage());E.LevelPath=E.Level->GetPathName();E.Package.Reset(A->GetPackage());E.External.Reset(A->GetExternalPackage());if(!Loaded(E.Package.Get())||(E.External.IsValid()&&!Loaded(E.External.Get())))return Stop(TEXT("Loaded actor/level/external package roles required."));E.Name=A->GetFName();E.Path=A->GetPathName();E.Guid=A->GetActorGuid();E.Transform=A->GetActorTransform();DirtyPackages.Add(E.Package->GetName());DirtyPackages.Add(E.Level->GetPackage()->GetName());if(E.External.IsValid())DirtyPackages.Add(E.External->GetName());Actors.Add(MoveTemp(E));if(!KeepMapped(A))return Stop(TEXT("Live instance replacement hold bound exceeded."));
            TArray<UObject*> Children;GetObjectsWithOuter(A,Children,EGetObjectsFlags::IncludeNestedObjects);if(Watched.Num()+Children.Num()>4096)return Stop(TEXT("Live instance subobject observation exceeds4096."));for(auto* O:Children){if(!Loaded(O))return Stop(TEXT("Live instance nested object is unavailable."));Watched.Emplace(O);if(!KeepMapped(O))return Stop(TEXT("Live instance replacement hold bound exceeded."));}
        }
        if(!Actors.IsEmpty()){ReplacedHandle=FCoreUObjectDelegates::OnObjectsReplaced.AddRaw(this,&FLivePerceptionInstances::Replaced);ReinstancedHandle=FCoreUObjectDelegates::OnObjectsReinstanced.AddRaw(this,&FLivePerceptionInstances::Reinstanced);R.Data->SetNumberField(TEXT("live_instance_count"),Actors.Num());R.Data->SetStringField(TEXT("live_instance_scope"),TEXT("Up to32 exact selected-class unpossessed/unowned/unattached ordinary current Editor-world AIControllers. Compiler replacement maps and authored placement/properties are observed; caches/component addresses and equal-to-template live overrides are not invariants. Object-reference/instanced-subobject override values are observed only through compiler maps, not certified by the non-object authored projection. Other contexts/types/custom instance effects remain incomplete. Blueprint save only; level/instance packages are not saved."));}
        return Actors.IsEmpty()||Current();
    }
    void Replaced(const FCoreUObjectDelegates::FReplacementObjectMap& P){Map(P,false);}void Reinstanced(const FCoreUObjectDelegates::FReplacementObjectMap& P){Map(P,true);}
    bool DirtyEqual(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,const TCHAR* Phase)
    {
        if(Actors.IsEmpty())return Text(Before)==Text(After);if(!Current()||Before->Values.Num()!=After->Values.Num())return false;TArray<TSharedPtr<FJsonValue>> Changes;
        for(const auto& P:Before->Values){const auto V=After->TryGetField(FString(P.Key));if(!V||P.Value->Type!=EJson::Boolean||V->Type!=EJson::Boolean)return false;if(P.Value->AsBool()!=V->AsBool()){if(!DirtyPackages.Contains(FString(P.Key)))return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("package"),FString(P.Key));J->SetBoolField(TEXT("before"),P.Value->AsBool());J->SetBoolField(TEXT("after"),V->AsBool());J->SetBoolField(TEXT("saved_by_plugin"),false);Changes.Add(MakeShared<FJsonValueObject>(J));}}
        const TSharedPtr<FJsonObject>* Existing=nullptr;TSharedPtr<FJsonObject> Reports=R.Data->TryGetObjectField(TEXT("live_instance_package_dirty_transitions"),Existing)?*Existing:MakeShared<FJsonObject>();Reports->SetArrayField(Phase,Changes);R.Data->SetObjectField(TEXT("live_instance_package_dirty_transitions"),Reports);return true;
    }
};
struct FParentIdentity
{
    TStrongObjectPtr<UObject> Object;
    UObject* Outer = nullptr; UClass* Class = nullptr; UPackage* Package = nullptr; UPackage* Effective = nullptr;
    FString ObjectPath; EObjectFlags Flags = RF_NoFlags;
    explicit FParentIdentity(UObject* O) : Object(O), Outer(O->GetOuter()), Class(O->GetClass()), Package(O->GetOutermost()), Effective(O->GetPackage()), ObjectPath(O->GetPathName()), Flags(O->GetFlags()) {}
};
struct FParentRole
{
    TStrongObjectPtr<UBlueprint> Blueprint;
    TStrongObjectPtr<UClass> Class, Skeleton, Super;
    TStrongObjectPtr<AActor> CDO;
    TStrongObjectPtr<USimpleConstructionScript> SCS;
    UClass* SkeletonSuper = nullptr; EClassFlags ClassFlags = CLASS_None, SkeletonFlags = CLASS_None;
    TArray<USCS_Node*> Nodes;
    TArray<UObject*> Templates; TArray<UClass*> NodeClasses; TArray<FGuid> NodeGuids;
    TArray<UObject*> CdoChildren;
};
struct FContext
{
    TStrongObjectPtr<UBlueprint> Blueprint; USCS_Node* Selected = nullptr; FString Package, RequestedName;
    TSharedPtr<FLivePerceptionInstances> LiveInstances;
    bool GeneratedParents = false;
    TWeakObjectPtr<UEditorEngine> Editor; TWeakObjectPtr<UWorld> World;
    UClass* NativeRoot = nullptr;
    TArray<FParentRole> Parents;
    TArray<TUniquePtr<FParentIdentity>> ParentObjects, ChildObjects;
    TSharedPtr<FJsonObject> ParentBaseline;
    TStrongObjectPtr<UClass> ExpectedGenerated, ExpectedSkeleton;
    TStrongObjectPtr<UObject> ExpectedCDO;
    TStrongObjectPtr<USimpleConstructionScript> ExpectedSCS;
    TStrongObjectPtr<USCS_Node> ExpectedNode;
    TStrongObjectPtr<UAIPerceptionComponent> ExpectedTemplate;
    TStrongObjectPtr<UAISenseConfig_Sight> ExpectedSight;
    TArray<USCS_Node*> ExpectedNodes;
    bool SightAttached = false;
    FGuid ExpectedNodeGuid; FName ExpectedNodeName; EClassFlags GeneratedFlags = CLASS_None, SkeletonFlags = CLASS_None;
};
bool ParentSafe(const FContext& C)
{
    return IsEditorGameThread() && GEditor && GEditor == C.Editor.Get() && !GExitPurge && !IsEngineExitRequested()
        && !IsGarbageCollecting() && !UE::IsSavingPackage() && !IsPlaySessionActive() && C.World.IsValid()
        && GEditor->GetEditorWorldContext().World() == C.World.Get() && !C.World->bIsTearingDown;
}
bool ParentKeep(FContext& C, UObject* O)
{
    if (!Loaded(O)) { return false; }
    for (const auto& I : C.ParentObjects) { if (I->Object.Get() == O) { return true; } }
    if (C.ParentObjects.Num() >= 4096 || !Loaded(O->GetClass()) || !Loaded(O->GetOutermost()) || !Loaded(O->GetPackage())
        || (O->GetOuter() && !Loaded(O->GetOuter())) || !CleanDependencyText(O->GetPathName()) || !CleanDependencyText(O->GetClass()->GetPathName())) { return false; }
    C.ParentObjects.Add(MakeUnique<FParentIdentity>(O)); return true;
}
bool ParentAdmission(const FContext& C, FResult& R)
{
    if (!C.GeneratedParents) { return true; }
    if (!ParentSafe(C)) { return Fail(R, TEXT("perception_parent_context_changed"), TEXT("Captured Editor/world or GT/PIE/GC/exit/save admission changed; no next target SDK or retry."), true); }
    for (const auto& I : C.ParentObjects)
    {
        UObject* O = I->Object.Get();
        if (!Loaded(O) || !Loaded(I->Class) || !Loaded(I->Package) || !Loaded(I->Effective) || (I->Outer && !Loaded(I->Outer))
            || O->GetClass() != I->Class || O->GetOuter() != I->Outer || O->GetOutermost() != I->Package || O->GetPackage() != I->Effective || O->GetPathName() != I->ObjectPath || O->GetFlags() != I->Flags)
        { return Fail(R, TEXT("perception_parent_identity_changed"), TEXT("Captured parent authoring object identity changed; parent replacement is not adopted."), true); }
    }
    if (!Loaded(C.Blueprint.Get()) || C.Parents.IsEmpty() || C.Blueprint->ParentClass != C.Parents[0].Class.Get())
    { return Fail(R, TEXT("perception_parent_chain_changed"), TEXT("Selected Blueprint immediate parent changed."), true); }
    for (const auto& P : C.Parents)
    {
        auto* B = P.Blueprint.Get(); auto* K = P.Class.Get(); auto* S = P.Skeleton.Get();
        if (!Loaded(B) || !Loaded(K) || !Loaded(S) || !Loaded(P.Super.Get()) || !Loaded(P.CDO.Get()) || B->GeneratedClass != K || B->SkeletonGeneratedClass != S
            || B->ParentClass != P.Super.Get() || K->GetSuperClass() != P.Super.Get() || K->ClassGeneratedBy != B || S->ClassGeneratedBy != B
            || S->GetSuperClass() != P.SkeletonSuper || K->GetClassFlags() != P.ClassFlags || S->GetClassFlags() != P.SkeletonFlags
            || K->GetDefaultObject(false) != P.CDO.Get() || P.CDO->GetClass() != K || B->SimpleConstructionScript != P.SCS.Get()
            || B->bBeingCompiled || B->bQueuedForCompilation || B->bIsRegeneratingOnLoad || B->bSuppressStructurallyModified || B->GetOutermost()->IsDirty()
            || (B->Status != BS_UpToDate && B->Status != BS_UpToDateWithWarnings)
            || (P.SCS.IsValid() && P.SCS->GetAllNodes() != P.Nodes))
        { return Fail(R, TEXT("perception_parent_role_changed"), TEXT("Current parent Blueprint/class/CDO/SCS/compile-readiness role changed; no inherited mutation, reinstancing adoption or retry."), true); }
        for (int32 N = 0; N < P.Nodes.Num(); ++N)
        {
            USCS_Node* Node = P.Nodes[N];
            if (Node->ComponentTemplate != P.Templates[N] || Node->ComponentClass != P.NodeClasses[N] || Node->VariableGuid != P.NodeGuids[N])
            { return Fail(R, TEXT("perception_parent_template_changed"), TEXT("Captured parent SCS node/template/class/GUID role changed; no new inherited object is read or adopted."), true); }
        }
    }
    return true;
}
bool ChildIdentity(const FContext& C, const FParentIdentity& I, FResult& R)
{
    UObject* O = I.Object.Get();
    if (!Loaded(O) || !Loaded(I.Class) || !Loaded(I.Package) || !Loaded(I.Effective) || (I.Outer && !Loaded(I.Outer))
        || O->GetClass() != I.Class || O->GetOuter() != I.Outer || O->GetOutermost() != I.Package || O->GetPackage() != I.Effective || O->GetPathName() != I.ObjectPath)
    { R.Data->SetStringField(TEXT("first_child_identity_failure"), I.ObjectPath); return Fail(R, TEXT("perception_child_identity_changed"), TEXT("Captured selected child class/outer/effective package/outermost package/path changed; no next SDK or retry."), true); }
    const EObjectFlags AddedSaveBits = RF_WasLoaded | RF_LoadCompleted;
    const bool SavedLoadFlags = R.bSaveAttempted && R.bSaved && I.Package->GetName() == C.Package && !EnumHasAnyFlags(I.Flags, RF_Transient)
        && O->GetFlags() == (I.Flags | AddedSaveBits);
    if (O->GetFlags() != I.Flags && !SavedLoadFlags)
    { R.Data->SetStringField(TEXT("first_child_identity_failure"), I.ObjectPath); return Fail(R, TEXT("perception_child_flags_changed"), TEXT("Selected child flags changed outside exact successful owned-save added load bits; no flags are cleared or normalized."), true); }
    return true;
}
bool NativeControllerRoot(UClass* C) { return C == AAIController::StaticClass() && Native(C, TEXT("/Script/AIModule")); }
bool NativeLocalComponent(UClass* C) { return Loaded(C) && (Native(C, TEXT("/Script/Engine")) || Native(C, TEXT("/Script/AIModule"))); }
bool PrepareParents(FContext& C, FResult& R)
{
    UClass* Parent = C.Blueprint->ParentClass.Get(); C.NativeRoot = Parent;
    if (NativeControllerRoot(Parent)) { return true; }
    if (!IsEditorGameThread() || !GEditor || GExitPurge || IsEngineExitRequested() || IsGarbageCollecting() || UE::IsSavingPackage() || IsPlaySessionActive())
    { return Fail(R, TEXT("perception_parent_editor_context_required"), TEXT("Safe Editor context is required before capturing a Blueprint parent world.")); }
    C.Editor = GEditor; C.World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!ParentSafe(C)) { return Fail(R, TEXT("perception_parent_editor_context_required"), TEXT("Safe captured Editor/world required for a Blueprint-parent input.")); }
    TSet<UClass*> Seen;
    for (int32 Depth = 0; !NativeControllerRoot(Parent); ++Depth)
    {
        if (Depth >= 32 || !Loaded(Parent) || Parent->GetClass() != UBlueprintGeneratedClass::StaticClass() || !Parent->IsChildOf(AAIController::StaticClass()) || Seen.Contains(Parent)
            || Parent->HasAnyClassFlags(CLASS_Native | CLASS_Deprecated | CLASS_NewerVersionExists) || !CleanDependencyText(Parent->GetPathName()))
        { return Fail(R, TEXT("perception_parent_class_unsupported"), TEXT("At most 32 exact current runtime AIController Blueprint parent classes ending at an native AIModule AIController class are supported.")); }
        Seen.Add(Parent); UBlueprint* B = Cast<UBlueprint>(Parent->ClassGeneratedBy);
        if (!Loaded(B) || B == C.Blueprint.Get() || B->GetClass() != UBlueprint::StaticClass() || B->BlueprintType != BPTYPE_Normal || B->GeneratedClass != Parent
            || !Loaded(B->SkeletonGeneratedClass) || B->SkeletonGeneratedClass->ClassGeneratedBy != B || !Loaded(B->ParentClass) || Parent->GetSuperClass() != B->ParentClass
            || B->GetOuter() != B->GetOutermost() || Parent->GetOuter() != B->GetOutermost() || B->SkeletonGeneratedClass->GetOutermost() != B->GetOutermost()
            || (!B->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) && !B->GetOutermost()->GetName().StartsWith(TEXT("/Engine/"))) || B->GetOutermost()->IsDirty()
            || B->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor) || B->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
            || B->bBeingCompiled || B->bQueuedForCompilation || B->bSuppressStructurallyModified || B->bIsRegeneratingOnLoad || (B->Status != BS_UpToDate && B->Status != BS_UpToDateWithWarnings))
        { return Fail(R, TEXT("perception_parent_blueprint_unsupported"), TEXT("Loaded clean completed current normal Game/Engine Blueprint parent required; no parent load, compile, dirty reset or reparenting.")); }
        AActor* D = Cast<AActor>(Parent->GetDefaultObject(false));
        if (!Loaded(D) || D->GetClass() != Parent || D->GetOuter() != Parent->GetOuter() || !D->HasAllFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
        { return Fail(R, TEXT("perception_parent_cdo_unsupported"), TEXT("Existing exact parent AIController CDO required; no CDO allocation or parent modification.")); }
        FParentRole P; P.Blueprint.Reset(B); P.Class.Reset(Parent); P.Skeleton.Reset(B->SkeletonGeneratedClass); P.Super.Reset(B->ParentClass); P.CDO.Reset(D); P.SCS.Reset(B->SimpleConstructionScript); P.SkeletonSuper = B->SkeletonGeneratedClass->GetSuperClass(); P.ClassFlags = Parent->GetClassFlags(); P.SkeletonFlags = B->SkeletonGeneratedClass->GetClassFlags();
        if (!ParentKeep(C, B) || !ParentKeep(C, Parent) || !ParentKeep(C, B->SkeletonGeneratedClass) || !ParentKeep(C, B->ParentClass) || !ParentKeep(C, P.SkeletonSuper) || !ParentKeep(C, D)) { return Fail(R, TEXT("perception_parent_capture_incomplete"), TEXT("Bounded parent identity capture failed before target mutation.")); }
        GetObjectsWithOuter(D, P.CdoChildren, EGetObjectsFlags::IncludeNestedObjects);
        if (P.CdoChildren.Num() > MaxNodes) { return Fail(R, TEXT("perception_parent_cdo_bound"), TEXT("Bounded parent CDO default-subobject inventory required.")); }
        for (UObject* O : P.CdoChildren) { if (!Loaded(O) || !O->IsIn(D) || O->GetOutermost() != B->GetOutermost() || !ParentKeep(C, O)) { return Fail(R, TEXT("perception_parent_cdo_subobject_unsupported"), TEXT("Exact parent CDO-owned default subtree required.")); } }
        if (P.SCS.IsValid())
        {
            if (!ParentKeep(C, P.SCS.Get()) || P.SCS->GetOuter() != Parent || P.SCS->GetAllNodes().Num() > MaxNodes) { return Fail(R, TEXT("perception_parent_scs_unsupported"), TEXT("Bounded own parent SCS required.")); }
            P.Nodes = P.SCS->GetAllNodes();
            for (USCS_Node* N : P.Nodes) { if (!Loaded(N) || N->GetOuter() != P.SCS.Get() || !N->VariableGuid.IsValid() || !NativeLocalComponent(N->ComponentClass) || !Loaded(N->ComponentTemplate) || N->ComponentTemplate->GetOuter() != Parent || N->ComponentTemplate->GetClass() != N->ComponentClass || !ParentKeep(C, N) || !ParentKeep(C, N->ComponentTemplate)) { return Fail(R, TEXT("perception_parent_scs_unsupported"), TEXT("Parent local SCS identities must remain exact and read-only; inherited template editing is not enabled.")); } P.Templates.Add(N->ComponentTemplate); P.NodeClasses.Add(N->ComponentClass); P.NodeGuids.Add(N->VariableGuid);
                TArray<UObject*> Nested; GetObjectsWithOuter(N->ComponentTemplate, Nested, EGetObjectsFlags::IncludeNestedObjects);
                if (Nested.Num() > MaxNodes) { return Fail(R, TEXT("perception_parent_template_bound"), TEXT("Bounded parent local template-owned configuration required.")); }
                for (UObject* O : Nested) { if (!Loaded(O) || !O->IsIn(N->ComponentTemplate) || !ParentKeep(C, O)) { return Fail(R, TEXT("perception_parent_template_subobject_unsupported"), TEXT("Parent template-owned configuration must be live and exact.")); } } }
        }
        TArray<UEdGraph*> GraphList; B->GetAllGraphs(GraphList);
        if (GraphList.Num() > MaxGraphs) { return Fail(R, TEXT("perception_parent_graph_bound"), TEXT("Bounded parent source graphs required.")); }
        int32 NodeCount = 0;
        for (UEdGraph* G : GraphList) { if (!Loaded(G) || G->GetOutermost() != B->GetOutermost() || !ParentKeep(C, G)) { return Fail(R, TEXT("perception_parent_graph_unsupported"), TEXT("Exact parent-owned source graph required.")); } for (const auto& N : G->Nodes) { if (++NodeCount > 4096 || !ParentKeep(C, N.Get())) { return Fail(R, TEXT("perception_parent_graph_unsupported"), TEXT("Exact bounded parent source nodes required.")); } } }
        C.Parents.Add(MoveTemp(P)); Parent = B->ParentClass.Get();
    }
    if (!NativeControllerRoot(Parent)) { return Fail(R, TEXT("perception_parent_native_root_unsupported"), TEXT("Current parent chain must end at the existing exact native AIModule AIController policy.")); }
    C.NativeRoot = Parent; C.GeneratedParents = true;
    R.Data->SetNumberField(TEXT("generated_parent_count"), C.Parents.Num()); R.Data->SetStringField(TEXT("parent_authoring_scope"), TEXT("Exact current clean Blueprint parents are read-only; only selected child own local SCS perception configuration is explicitly written; its classes/CDO may be regenerated by the compiler; no inherited component or parent-default override is edited."));
    return ParentAdmission(C, R);
}
bool KeepChild(FContext& C, UObject* O, FResult& R)
{
    if (!C.GeneratedParents || !O) { return true; }
    for (const auto& I : C.ChildObjects) { if (I->Object.Get() == O) { return true; } }
    if (!Loaded(O) || C.ChildObjects.Num() >= 4096 || !Loaded(O->GetClass()) || !Loaded(O->GetOutermost()) || !Loaded(O->GetPackage()) || (O->GetOuter() && !Loaded(O->GetOuter())) || O->GetOutermost()->GetName() != C.Package)
    { return Fail(R, TEXT("perception_child_capture_incomplete"), TEXT("Exact bounded selected-child authoring identity required."), R.bAttempted); }
    C.ChildObjects.Add(MakeUnique<FParentIdentity>(O)); return true;
}
bool FreezeChild(FContext& C, FResult& R, bool bBeforePlannedAdd = false)
{
    if (!C.GeneratedParents) { return true; }
    if (!ParentAdmission(C, R)) { return false; }
    UBlueprint* B = C.Blueprint.Get();
    C.ExpectedGenerated.Reset(B->GeneratedClass); C.ExpectedSkeleton.Reset(B->SkeletonGeneratedClass); C.ExpectedSCS.Reset(B->SimpleConstructionScript);
    C.ExpectedCDO.Reset(B->GeneratedClass->GetDefaultObject(false)); C.ExpectedNode.Reset(C.Selected);
    C.GeneratedFlags = B->GeneratedClass->GetClassFlags(); C.SkeletonFlags = B->SkeletonGeneratedClass->GetClassFlags();
    C.ExpectedNodeGuid = C.Selected ? C.Selected->VariableGuid : FGuid(); C.ExpectedNodeName = C.Selected ? C.Selected->GetVariableName() : NAME_None;
    C.ExpectedTemplate.Reset(C.Selected ? Cast<UAIPerceptionComponent>(C.Selected->ComponentTemplate) : nullptr);
    C.ExpectedSight.Reset(C.ExpectedTemplate.IsValid() ? C.ExpectedTemplate->GetSenseConfig<UAISenseConfig_Sight>() : nullptr); C.SightAttached = C.ExpectedSight.IsValid();
    if (!Loaded(C.ExpectedCDO.Get()) || C.ExpectedCDO->GetClass() != B->GeneratedClass || !C.ExpectedCDO->HasAnyFlags(RF_ClassDefaultObject))
    { return Fail(R, TEXT("perception_child_cdo_unavailable"), TEXT("Existing current selected child CDO required; no lazy CDO creation."), R.bAttempted); }
    C.ChildObjects.Reset();
    const TArray<UObject*> Core = { B, C.ExpectedGenerated.Get(), C.ExpectedSkeleton.Get(), C.ExpectedCDO.Get(), C.ExpectedSCS.Get(), C.ExpectedNode.Get(), C.ExpectedTemplate.Get(), C.ExpectedSight.Get() };
    for (UObject* O : Core) { if (!KeepChild(C, O, R)) { return false; } }
    C.ExpectedNodes = C.ExpectedSCS->GetAllNodes();
    if (bBeforePlannedAdd && C.Selected && !C.ExpectedNodes.Contains(C.Selected)) { C.ExpectedNodes.Add(C.Selected); }
    for (USCS_Node* N : C.ExpectedNodes)
    {
        if (!Loaded(N) || !Loaded(N->ComponentTemplate) || !KeepChild(C, N, R) || !KeepChild(C, N->ComponentTemplate, R)) { return false; }
        TArray<UObject*> Nested; GetObjectsWithOuter(N->ComponentTemplate, Nested, EGetObjectsFlags::IncludeNestedObjects);
        if (Nested.Num() > MaxNodes) { return Fail(R, TEXT("perception_child_template_bound"), TEXT("Selected child local configuration inventory exceeds the finite bound."), R.bAttempted); }
        for (UObject* O : Nested) { if (!Loaded(O) || !O->IsIn(N->ComponentTemplate) || !KeepChild(C, O, R)) { return false; } }
    }
    return true;
}
bool ChildAsset(FContext& C, FResult& R)
{
    return !C.GeneratedParents || (ParentAdmission(C, R) && !C.ChildObjects.IsEmpty() && ChildIdentity(C, *C.ChildObjects[0], R));
}
bool ChildAdmission(FContext& C, FResult& R)
{
    if (!C.GeneratedParents) { return true; }
    if (!ParentAdmission(C, R)) { return false; }
    for (const auto& I : C.ChildObjects) { if (!ChildIdentity(C, *I, R)) { return false; } }
    UBlueprint* B = C.Blueprint.Get();
    if (B->GeneratedClass != C.ExpectedGenerated.Get() || B->SkeletonGeneratedClass != C.ExpectedSkeleton.Get() || B->SimpleConstructionScript != C.ExpectedSCS.Get()
        || !Loaded(C.ExpectedGenerated.Get()) || !Loaded(C.ExpectedSkeleton.Get()) || !Loaded(C.ExpectedSCS.Get()) || !Loaded(C.ExpectedCDO.Get())
        || B->GeneratedClass->ClassGeneratedBy != B || B->GeneratedClass->GetSuperClass() != B->ParentClass || B->GeneratedClass->GetDefaultObject(false) != C.ExpectedCDO.Get()
        || B->GeneratedClass->GetClassFlags() != C.GeneratedFlags || B->SkeletonGeneratedClass->GetClassFlags() != C.SkeletonFlags
        || C.ExpectedSCS->GetAllNodes() != C.ExpectedNodes || C.Selected != C.ExpectedNode.Get())
    { return Fail(R, TEXT("perception_child_role_changed"), TEXT("Selected child class/CDO/local SCS changed outside an explicit compiler-return adoption boundary."), true); }
    if (C.Selected)
    {
        if (!C.ExpectedNodes.Contains(C.Selected) || C.Selected->GetOuter() != C.ExpectedSCS.Get() || C.Selected->VariableGuid != C.ExpectedNodeGuid || C.Selected->GetVariableName() != C.ExpectedNodeName || C.Selected->ComponentTemplate != C.ExpectedTemplate.Get()
            || !Loaded(C.ExpectedTemplate.Get()) || C.ExpectedTemplate->GetOuter() != B->GeneratedClass || C.ExpectedTemplate->GetClass() != UAIPerceptionComponent::StaticClass()
            || C.ExpectedTemplate->IsRegistered() || C.ExpectedTemplate->GetOwner() || C.ExpectedTemplate->GetWorld() || !ParentSafe(C)
            || C.ExpectedTemplate->GetSenseConfig<UAISenseConfig_Sight>() != (C.SightAttached ? C.ExpectedSight.Get() : nullptr))
        { return Fail(R, TEXT("perception_child_template_role_changed"), TEXT("Selected own local perception template/Sight role changed; inherited or replacement objects are not adopted."), true); }
    }
    return true;
}

bool Prepare(const FString& InputPath, const FString& ComponentName, FContext& C, FResult& R,const TSharedPtr<FLivePerceptionInstances>& ExistingLive=nullptr)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("perception_editor_context_required"), TEXT("Editor game thread without PIE/SIE is required; no mutation or hidden load.")); }
    const FString ObjectPath = Path(InputPath); C.Package = FPackageName::ObjectPathToPackageName(ObjectPath); C.RequestedName = ComponentName;
    if (!FPackageName::IsValidObjectPath(ObjectPath) || !C.Package.StartsWith(TEXT("/Game/")) || !Name(ComponentName) || !CleanDependencyText(ObjectPath)) { return Fail(R, TEXT("perception_inputs_invalid"), TEXT("Exact mutable Game Blueprint path and original simple component name are required.")); }
    C.Blueprint.Reset(FindObject<UBlueprint>(nullptr, *ObjectPath)); UBlueprint* B = C.Blueprint.Get(); R.Data->SetStringField(TEXT("controller_blueprint_path"), ObjectPath); R.Data->SetStringField(TEXT("requested_component_name"), ComponentName);
    if (!Loaded(B) || B->GetClass() != UBlueprint::StaticClass() || B->GetOuter() != B->GetOutermost() || B->BlueprintType != BPTYPE_Normal || (!Loaded(B->ParentClass) || !B->ParentClass->IsChildOf(AAIController::StaticClass()))
        || !Loaded(B->GeneratedClass) || !Loaded(B->SkeletonGeneratedClass) || B->GeneratedClass->ClassGeneratedBy != B || B->SkeletonGeneratedClass->ClassGeneratedBy != B
        || B->GeneratedClass->GetSuperClass() != B->ParentClass || B->GeneratedClass->GetOutermost() != B->GetOutermost() || B->SkeletonGeneratedClass->GetOutermost() != B->GetOutermost()
        || !Loaded(B->SimpleConstructionScript) || B->SimpleConstructionScript->GetOuter() != B->GeneratedClass || B->Status == BS_Error || B->bBeingCompiled || B->bQueuedForCompilation || B->bSuppressStructurallyModified)
    { return Fail(R, TEXT("perception_blueprint_unsupported"), TEXT("Loaded normal AIController Blueprint/current classes/own local SCS required; inherited template editing/custom/unfinished variants remain incomplete.")); }
    if (!PrepareParents(C, R)) { return false; }
    if(ExistingLive.IsValid()){C.LiveInstances=ExistingLive;if(!C.LiveInstances->Current())return false;}
    for (TObjectIterator<UBlueprintGeneratedClass> It; It; ++It) { if (Loaded(*It) && *It != B->GeneratedClass && *It != B->SkeletonGeneratedClass && It->IsChildOf(B->GeneratedClass)) { return Fail(R, TEXT("perception_derived_blueprint_unsupported"), TEXT("Loaded derived Blueprint classes are outside the finite compile/propagation scope.")); } }
    USimpleConstructionScript* SCS = B->SimpleConstructionScript.Get(); const auto& Nodes = SCS->GetAllNodes(); if (Nodes.Num() > MaxNodes) { return Fail(R, TEXT("perception_scs_bound"), TEXT("Local SCS exceeds the finite node bound.")); }
    USCS_Node* Named = SCS->FindSCSNode(FName(*ComponentName)); TArray<USCS_Node*> Matching; TSet<USCS_Node*> Seen;
    for (USCS_Node* N : Nodes)
    {
        if (!Loaded(N) || N->GetOuter() != SCS || Seen.Contains(N) || !N->VariableGuid.IsValid() || !Loaded(N->ComponentClass) || !N->ComponentClass->HasAnyClassFlags(CLASS_Native) || (N->ComponentClass->GetOutermost()->GetName() != TEXT("/Script/Engine") && N->ComponentClass->GetOutermost()->GetName() != TEXT("/Script/AIModule")) || !Loaded(N->ComponentTemplate) || N->ComponentTemplate->GetClass() != N->ComponentClass || N->ComponentTemplate->GetOuter() != B->GeneratedClass || !N->ComponentTemplate->HasAnyFlags(RF_ArchetypeObject))
        { return Fail(R, TEXT("perception_scs_ownership_unsupported"), TEXT("Exact local node/GUID/native template/class owner admission failed before mutation.")); } Seen.Add(N);
        if (N->ComponentClass->IsChildOf(UAIPerceptionComponent::StaticClass())) { Matching.Add(N); }
    }
    if (Named && (!Seen.Contains(Named) || Named->ComponentClass != UAIPerceptionComponent::StaticClass())) { return Fail(R, TEXT("perception_named_component_wrong_class"), TEXT("Original named-node priority selected an unsupported/non-native perception component; no fallback mutation.")); }
    // YO: UE56 selects by explicit name first, otherwise the first matching
    // local node in the original physical SCS inventory; multiplicity is valid.
    for(auto* Match:Matching)if(Match->ComponentClass!=UAIPerceptionComponent::StaticClass())
        return Fail(R,TEXT("perception_component_selection_unsupported"),TEXT("Custom native perception templates remain outside the exact native observation profile."));
    C.Selected = Named ? Named : !Matching.IsEmpty() ? Matching[0] : nullptr;
    R.Data->SetNumberField(TEXT("matching_perception_component_count"),Matching.Num());
    R.Data->SetStringField(TEXT("perception_component_selection"),Named?TEXT("requested_local_name"):C.Selected?TEXT("first_matching_physical_local_scs_node"):TEXT("create_missing_local_component"));
    if (!C.Selected && FindFProperty<FProperty>(B->GeneratedClass, FName(*ComponentName))) { return Fail(R, TEXT("perception_component_name_collision"), TEXT("Existing generated member collision is outside this finite creation slice; original auto-renaming remains incomplete.")); }
    if(!ExistingLive.IsValid()){C.LiveInstances=MakeShared<FLivePerceptionInstances>(R);if(!C.LiveInstances->Bind(B,C.Selected?C.Selected->GetVariableName().ToString():ComponentName))return false;if(C.LiveInstances->Actors.IsEmpty())C.LiveInstances.Reset();}
    return FreezeChild(C, R);
}
bool Graphs(UBlueprint* B, TSharedRef<FJsonObject>& J)
{
    TArray<UEdGraph*> Graphs; B->GetAllGraphs(Graphs); if (Graphs.Num() > MaxGraphs) { return false; } TSet<UEdGraph*> Unique; TArray<TSharedPtr<FJsonValue>> Rows; int32 NodeCount = 0, PinCount = 0;
    for (UEdGraph* G : Graphs)
    {
        if (!Loaded(G) || G->GetOutermost() != B->GetOutermost() || Unique.Contains(G) || !G->GraphGuid.IsValid()) { return false; } Unique.Add(G);
        TSet<UEdGraphNode*> OwnedNodes; TSet<const UEdGraphPin*> OwnedPins;
        for (UEdGraphNode* N : G->Nodes)
        {
            if (!Loaded(N) || N->GetOuter() != G || OwnedNodes.Contains(N) || !N->NodeGuid.IsValid() || ++NodeCount > 1024) { return false; } OwnedNodes.Add(N);
            for (const UEdGraphPin* P : N->Pins) { if (!P || P->GetOwningNodeUnchecked() != N || !P->PinId.IsValid() || OwnedPins.Contains(P) || ++PinCount > MaxPins || P->LinkedTo.Num() > MaxPins) { return false; } OwnedPins.Add(P); }
        }
        auto Row = Identity(G); Row->SetStringField(TEXT("guid"), G->GraphGuid.ToString()); Row->SetStringField(TEXT("schema"), G->Schema ? G->Schema->GetPathName() : TEXT("")); TArray<TSharedPtr<FJsonValue>> Nodes;
        for (UEdGraphNode* N : G->Nodes)
        {
            auto NR = Identity(N); NR->SetStringField(TEXT("guid"), N->NodeGuid.ToString()); NR->SetNumberField(TEXT("x"), N->NodePosX); NR->SetNumberField(TEXT("y"), N->NodePosY);
            auto NP = MakeShared<FJsonObject>(); if (!Properties(N, {}, NP)) { return false; } NR->SetObjectField(TEXT("properties"), NP); TArray<TSharedPtr<FJsonValue>> Pins;
            for (const UEdGraphPin* P : N->Pins)
            {
                auto PR = MakeShared<FJsonObject>(); PR->SetStringField(TEXT("id"), P->PinId.ToString()); PR->SetStringField(TEXT("name"), P->PinName.ToString()); PR->SetNumberField(TEXT("direction"), P->Direction);
                PR->SetStringField(TEXT("category"), P->PinType.PinCategory.ToString()); PR->SetStringField(TEXT("subcategory"), P->PinType.PinSubCategory.ToString()); PR->SetStringField(TEXT("type_object"), P->PinType.PinSubCategoryObject.IsValid() ? P->PinType.PinSubCategoryObject->GetPathName() : TEXT(""));
                PR->SetNumberField(TEXT("container"), static_cast<int32>(P->PinType.ContainerType)); PR->SetBoolField(TEXT("reference"), P->PinType.bIsReference); PR->SetBoolField(TEXT("const"), P->PinType.bIsConst); PR->SetStringField(TEXT("default_value"), P->DefaultValue);
                PR->SetStringField(TEXT("default_object"), P->DefaultObject ? P->DefaultObject->GetPathName() : TEXT("")); PR->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString()); TArray<TSharedPtr<FJsonValue>> Links;
                TSet<const UEdGraphPin*> Peers; for (const UEdGraphPin* Other : P->LinkedTo)
                {
                    if (!Other || !OwnedPins.Contains(Other) || Other == P || Peers.Contains(Other) || !OwnedNodes.Contains(Other->GetOwningNodeUnchecked()) || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(P))) { return false; } Peers.Add(Other);
                    Links.Add(MakeShared<FJsonValueString>(Other->GetOwningNodeUnchecked()->GetPathName() + TEXT(":") + Other->PinId.ToString()));
                }
                PR->SetArrayField(TEXT("links"), Links); Pins.Add(MakeShared<FJsonValueObject>(PR));
            }
            NR->SetArrayField(TEXT("pins"), Pins); Nodes.Add(MakeShared<FJsonValueObject>(NR));
        }
        Row->SetArrayField(TEXT("nodes"), Nodes); Rows.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("graphs"), Rows);
    }
    J->SetArrayField(TEXT("graphs"), Rows); J->SetNumberField(TEXT("graph_count"), Rows.Num()); J->SetNumberField(TEXT("node_count"), NodeCount); J->SetNumberField(TEXT("pin_count"), PinCount); return CleanDependencyText(Text(J));
}
bool ParentSources(FContext& C, TSharedRef<FJsonObject>& Sources, FResult& R)
{
    if (!C.GeneratedParents) { return true; }
    if (!ParentAdmission(C, R)) { return false; }
    auto All = MakeShared<FJsonObject>(); Sources->SetObjectField(TEXT("generated_parent_blueprints"), All);
    for (const auto& P : C.Parents)
    {
        UBlueprint* B = P.Blueprint.Get(); auto Row = MakeShared<FJsonObject>(); All->SetObjectField(B->GetPathName(), Row);
        Row->SetObjectField(TEXT("blueprint"), Identity(B)); Row->SetObjectField(TEXT("class"), Identity(P.Class.Get())); Row->SetObjectField(TEXT("skeleton"), Identity(P.Skeleton.Get())); Row->SetObjectField(TEXT("cdo"), Identity(P.CDO.Get()));
        Row->SetStringField(TEXT("super"), P.Super->GetPathName()); Row->SetBoolField(TEXT("dirty"), B->GetOutermost()->IsDirty()); Row->SetNumberField(TEXT("status"), static_cast<uint8>(B->Status.GetValue()));
        auto D = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("disk"), D); if (!Disk(B->GetOutermost()->GetName(), D)) { return false; }
        auto Cdo = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("cdo_properties"), Cdo); if (!Properties(P.CDO.Get(), {}, Cdo) || !ParentAdmission(C, R)) { return false; }
        TArray<UObject*> CurrentChildren; GetObjectsWithOuter(P.CDO.Get(), CurrentChildren, EGetObjectsFlags::IncludeNestedObjects);
        if (CurrentChildren.Num() != P.CdoChildren.Num()) { return false; }
        auto Children = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("cdo_owned_subobjects"), Children);
        for (UObject* O : CurrentChildren) { if (!P.CdoChildren.Contains(O) || !Loaded(O)) { return false; } auto Item = MakeShared<FJsonObject>(); Children->SetObjectField(O->GetPathName(), Item); Item->SetObjectField(TEXT("identity"), Identity(O)); auto Props = MakeShared<FJsonObject>(); Item->SetObjectField(TEXT("properties"), Props); if (!Properties(O, {}, Props) || !ParentAdmission(C, R)) { return false; } }
        auto Declarations = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("declarations"), Declarations);
        for (const FName Field : { FName(TEXT("NewVariables")), FName(TEXT("ImplementedInterfaces")), FName(TEXT("ParentClass")) }) { FProperty* Prop = FindFProperty<FProperty>(B->GetClass(), Field); FString Value; if (!Prop || !Prop->ExportText_Direct(Value, Prop->ContainerPtrToValuePtr<void>(B), Prop->ContainerPtrToValuePtr<void>(B), B, PPF_None) || Value.Len() > MaxText || !CleanDependencyText(Value) || !ParentAdmission(C, R)) { return false; } Declarations->SetStringField(Field.ToString(), Value); }
        auto Graph = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("graphs"), Graph); if (!Graphs(B, Graph) || !ParentAdmission(C, R)) { return false; }
        auto Nodes = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("local_scs"), Nodes);
        if (P.SCS.IsValid())
        {
            Row->SetObjectField(TEXT("scs"), Identity(P.SCS.Get()));
            for (USCS_Node* N : P.Nodes) { auto Item = MakeShared<FJsonObject>(); Nodes->SetObjectField(N->VariableGuid.ToString(), Item); Item->SetObjectField(TEXT("identity"), Identity(N)); Item->SetObjectField(TEXT("template"), Identity(N->ComponentTemplate)); auto NP = MakeShared<FJsonObject>(), TP = MakeShared<FJsonObject>(); Item->SetObjectField(TEXT("node_properties"), NP); Item->SetObjectField(TEXT("template_properties"), TP); if (!Properties(N, {}, NP) || !ParentAdmission(C, R) || !Properties(N->ComponentTemplate, {}, TP) || !ParentAdmission(C, R)) { return false; } }
        }
        auto Nested = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("local_scs_template_owned_configuration"), Nested);
        for (UObject* Template : P.Templates)
        {
            TArray<UObject*> Current; GetObjectsWithOuter(Template, Current, EGetObjectsFlags::IncludeNestedObjects);
            if (Current.Num() > MaxNodes) { return false; }
            for (UObject* O : Current)
            {
                bool Captured = false; for (const auto& I : C.ParentObjects) { if (I->Object.Get() == O) { Captured = true; break; } }
                if (!Captured || !Loaded(O) || !O->IsIn(Template)) { return false; }
                auto Item = MakeShared<FJsonObject>(); Nested->SetObjectField(O->GetPathName(), Item); Item->SetObjectField(TEXT("identity"), Identity(O));
                auto Props = MakeShared<FJsonObject>(); Item->SetObjectField(TEXT("properties"), Props);
                if (!Properties(O, {}, Props) || !ParentAdmission(C, R)) { return false; }
            }
        }
    }
    return ParentAdmission(C, R) && CleanDependencyText(Text(Sources));
}
// Parent authoring observations are immutable even when a child compiler may
// reinstance only the selected child classes/CDO/templates.
bool ParentFence(FContext& C, FResult& R)
{
    if (!C.GeneratedParents) { return true; }
    if (!ParentAdmission(C, R)) { return false; }
    if (C.ParentBaseline.IsValid())
    {
        auto Current = MakeShared<FJsonObject>();
        if (!ParentSources(C, Current, R) || Text(Current) != Text(C.ParentBaseline.ToSharedRef()))
        { return Fail(R, TEXT("perception_parent_source_changed"), TEXT("Read-only parent CDO/local SCS/config/source/declarations/disk observation changed or became incomplete; no next writer/compile/save."), true); }
    }
    return true;
}
bool GeneratedGuard(FContext& C, FResult& R)
{
    if(C.LiveInstances.IsValid()&&!C.LiveInstances->Current())return false;
    const bool Good=!C.GeneratedParents || (ParentFence(C, R) && ChildAdmission(C, R));
    return Good&&(!C.LiveInstances.IsValid()||C.LiveInstances->Current());
}
bool CompilerReturnedAsset(FContext& C, FResult& R)
{
    return !C.GeneratedParents || (ParentFence(C, R) && ChildAsset(C, R));
}
bool AdoptCompilerChild(FContext& C, FResult& R, const FGuid& Guid)
{
    if (!C.GeneratedParents) { return true; }
    if (!CompilerReturnedAsset(C, R)) { return false; }
    FContext Current;
    if (!Prepare(C.Blueprint->GetPathName(), C.RequestedName, Current, R,C.LiveInstances) || Current.Blueprint.Get() != C.Blueprint.Get() || !Current.Selected || Current.Selected->VariableGuid != Guid || Current.Selected->GetVariableName() != C.ExpectedNodeName)
    { return Fail(R, TEXT("perception_postcompile_child_unobservable"), TEXT("Explicit compiler returned but current selected child class/CDO/local SCS GUID/name cannot be reacquired; no stale template observation/save/retry."), true); }
    // Keep the original parent identities and original source baseline; no parent
    // replacement is accepted by creating a fresh child context.
    Current.Parents = MoveTemp(C.Parents); Current.ParentObjects = MoveTemp(C.ParentObjects); Current.ParentBaseline = C.ParentBaseline;
    Current.Editor = C.Editor; Current.World = C.World; C = MoveTemp(Current);
    return GeneratedGuard(C, R);
}
bool Observe(FContext& C, TSharedRef<FJsonObject>& J)
{
    UBlueprint* B = C.Blueprint.Get(); if (!Loaded(B) || !Loaded(B->GeneratedClass) || !Loaded(B->SkeletonGeneratedClass) || !Loaded(B->SimpleConstructionScript)) { return false; }
    J->SetObjectField(TEXT("identity"), Identity(B)); J->SetObjectField(TEXT("generated_class"), Identity(B->GeneratedClass)); J->SetObjectField(TEXT("skeleton_class"), Identity(B->SkeletonGeneratedClass)); J->SetObjectField(TEXT("scs_identity"), Identity(B->SimpleConstructionScript));
    J->SetBoolField(TEXT("package_dirty"), B->GetOutermost()->IsDirty()); J->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(B->Status.GetValue()));
    auto G = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("stored_graphs"), G); if (!Graphs(B, G)) { return false; }
    auto* Variables = FindFProperty<FArrayProperty>(B->GetClass(), TEXT("NewVariables")); if (!Variables || Variables->ArrayDim != 1) { return false; }
    FString VariableText; const void* VariableData = Variables->ContainerPtrToValuePtr<void>(B); FScriptArrayHelper VarArray(Variables, VariableData);
    if (VarArray.Num() > MaxNodes || !Variables->ExportText_Direct(VariableText, VariableData, VariableData, B, PPF_None) || VariableText.Len() > MaxText) { return false; }
    J->SetNumberField(TEXT("declared_variable_count"), VarArray.Num()); J->SetStringField(TEXT("declared_variables"), VariableText); if (!CleanDependencyText(VariableText)) { return false; }
    TArray<TSharedPtr<FJsonValue>> Nodes; TSet<FGuid> Guids; USimpleConstructionScript* SCS = B->SimpleConstructionScript;
    TStrongObjectPtr<USimpleConstructionScript> KeepSCS(SCS);const auto NodeRows=SCS->GetAllNodes();TArray<TStrongObjectPtr<USCS_Node>> NodeHolds;
    if(NodeRows.Num()>MaxNodes)return false;for(auto* Node:NodeRows){if(!Loaded(Node))return false;NodeHolds.Emplace(Node);}
    // Snapshot iteration owns its local node list across reflected read callbacks.
    for (USCS_Node* N : NodeRows)
    {
        auto Row = Identity(N); Nodes.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("scs_nodes"), Nodes);
        if (!Loaded(N) || N->GetOuter() != SCS || !N->VariableGuid.IsValid() || Guids.Contains(N->VariableGuid) || !Loaded(N->ComponentTemplate) || N->ComponentTemplate->GetClass() != N->ComponentClass || N->ComponentTemplate->GetOuter() != B->GeneratedClass || Nodes.Num() > MaxNodes) { return false; }
        Guids.Add(N->VariableGuid); Row->SetStringField(TEXT("variable_guid"), N->VariableGuid.ToString()); Row->SetStringField(TEXT("variable_name"), N->GetVariableName().ToString()); Row->SetObjectField(TEXT("template"), Identity(N->ComponentTemplate));
        TArray<TSharedPtr<FJsonValue>> Children; for (USCS_Node* Child : N->GetChildNodes()) { if (!Loaded(Child) || Child->GetOuter() != SCS) { return false; } Children.Add(MakeShared<FJsonValueString>(Child->VariableGuid.ToString())); } Row->SetArrayField(TEXT("children"), Children);
        if (N == C.Selected)
        {
            auto Component = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("perception"), Component); if (!ReadComponent(Cast<UAIPerceptionComponent>(N->ComponentTemplate), Component)) { return false; }
            J->SetStringField(TEXT("selected_node_guid"), N->VariableGuid.ToString()); J->SetStringField(TEXT("selected_component_name"), N->GetVariableName().ToString()); J->SetObjectField(TEXT("selected_perception"), Component);
        }
        else
        {
            // Preserve every unselected template as before. Perception configs
            // also need authored nested values, rather than reference text only.
            const auto TemplateSlot=N->ComponentTemplate;const auto ClassSlot=N->ComponentClass;
            UActorComponent* Template=static_cast<UActorComponent*>(PerceptionObservedPointer(TemplateSlot));
            UClass* TemplateClass=static_cast<UClass*>(PerceptionObservedPointer(ClassSlot));
            UClass* GeneratedClass=static_cast<UClass*>(PerceptionObservedPointer(B->GeneratedClass.GetGCPtr()));
            TStrongObjectPtr<USCS_Node> KeepNode(N);TStrongObjectPtr<UActorComponent> KeepTemplate(Template);
            TStrongObjectPtr<UClass> KeepClass(TemplateClass),KeepGeneratedClass(GeneratedClass);
            const FGuid NodeGuid=N->VariableGuid;const FName VariableName=N->GetVariableName();
            const auto CurrentTemplate=[&]()->bool
            {
                return Loaded(B)&&Loaded(SCS)&&B->SimpleConstructionScript.IsResolved()&&B->SimpleConstructionScript==SCS
                    &&Loaded(N)&&N->GetOuter()==SCS&&SCS->GetAllNodes().Contains(N)&&N->VariableGuid==NodeGuid&&N->GetVariableName()==VariableName
                    &&N->ComponentTemplate.IsResolved()&&N->ComponentTemplate==Template&&N->ComponentClass.IsResolved()&&N->ComponentClass==TemplateClass
                    &&B->GeneratedClass.GetGCPtr().IsResolved()&&B->GeneratedClass.GetGCPtr()==GeneratedClass&&Loaded(GeneratedClass)&&Loaded(TemplateClass)
                    &&Loaded(Template)&&Template->GetClass()==TemplateClass&&Template->GetOuter()==GeneratedClass
                    &&B->SimpleConstructionScript.IsResolved()&&B->SimpleConstructionScript==SCS;
            };
            // Fence every unselected template before the first reflected export,
            // including ordinary non-Perception components, and again on return.
            if(!CurrentTemplate())return false;
            auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("template_properties"), P);
            if (!Properties(Template, {}, P) || !CurrentTemplate() || !CleanDependencyText(Text(P))) { return false; }
            if(TemplateClass==UAIPerceptionComponent::StaticClass())
            {
                if(!CurrentTemplate())return false;
                auto Other=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("unselected_perception"),Other);
                if(!ReadComponent(static_cast<UAIPerceptionComponent*>(Template),Other)||!CurrentTemplate())return false;
            }
        }
    }
    J->SetArrayField(TEXT("scs_nodes"), Nodes); TArray<TSharedPtr<FJsonValue>> Roots; for (USCS_Node* N : SCS->GetRootNodes()) { if (!Loaded(N) || N->GetOuter() != SCS || !Guids.Contains(N->VariableGuid)) { return false; } Roots.Add(MakeShared<FJsonValueString>(N->VariableGuid.ToString())); } J->SetArrayField(TEXT("scs_roots"), Roots);
    UObject* CDO = B->GeneratedClass->GetDefaultObject(false); J->SetObjectField(TEXT("existing_generated_cdo"), Identity(CDO)); auto P = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("unselected_generated_cdo_properties"), P);
    const FName SelectedName = C.Selected ? C.Selected->GetVariableName() : FName(*C.RequestedName);
    if (!Loaded(CDO) || !CDO->HasAnyFlags(RF_ClassDefaultObject) || CDO->GetClass() != B->GeneratedClass || CDO->GetOuter() != B->GeneratedClass->GetOuter() || !Properties(CDO, { SelectedName }, P) || !CleanDependencyText(Text(P))) { return false; }
    if(!Loaded(SCS)||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript!=SCS||SCS->GetAllNodes()!=NodeRows
        ||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript!=SCS)return false;
    J->SetBoolField(TEXT("complete"), true); return true;
}
const TSharedPtr<FJsonObject> SelectedSight(const TSharedRef<FJsonObject>& State)
{
    const auto Component = State->TryGetField(TEXT("selected_perception")); if (!Component.IsValid() || Component->Type != EJson::Object) { return nullptr; }
    for (const auto& Row : Component->AsObject()->GetArrayField(TEXT("sense_configs"))) { if (Row->AsObject()->HasField(TEXT("stored_max_age"))) { return Row->AsObject(); } } return nullptr;
}
bool Preserved(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, const FGuid SelectedGuid, bool bCreated)
{
    if (Text(Before->GetObjectField(TEXT("stored_graphs")).ToSharedRef()) != Text(After->GetObjectField(TEXT("stored_graphs")).ToSharedRef())
        || Before->GetStringField(TEXT("declared_variables")) != After->GetStringField(TEXT("declared_variables"))
        || Text(Before->GetObjectField(TEXT("unselected_generated_cdo_properties")).ToSharedRef()) != Text(After->GetObjectField(TEXT("unselected_generated_cdo_properties")).ToSharedRef())) { return false; }
    TArray<TSharedPtr<FJsonValue>> OldNodes, NewNodes, OldRoots, NewRoots;
    const FString Guid = SelectedGuid.ToString();
    for (const auto& N : Before->GetArrayField(TEXT("scs_nodes"))) { if (N->AsObject()->GetStringField(TEXT("variable_guid")) != Guid) { OldNodes.Add(N); } }
    for (const auto& N : After->GetArrayField(TEXT("scs_nodes"))) { if (N->AsObject()->GetStringField(TEXT("variable_guid")) != Guid) { NewNodes.Add(N); } }
    for (const auto& N : Before->GetArrayField(TEXT("scs_roots"))) { if (N->AsString() != Guid) { OldRoots.Add(N); } }
    for (const auto& N : After->GetArrayField(TEXT("scs_roots"))) { if (N->AsString() != Guid) { NewRoots.Add(N); } }
    auto Old = MakeShared<FJsonObject>(), New = MakeShared<FJsonObject>(); Old->SetArrayField(TEXT("nodes"), OldNodes); New->SetArrayField(TEXT("nodes"), NewNodes); Old->SetArrayField(TEXT("roots"), OldRoots); New->SetArrayField(TEXT("roots"), NewRoots);
    if (Text(Old) != Text(New) || After->GetArrayField(TEXT("scs_nodes")).Num() != Before->GetArrayField(TEXT("scs_nodes")).Num() + (bCreated ? 1 : 0)) { return false; }
    const auto OldComponent = Before->TryGetField(TEXT("selected_perception")); if (!OldComponent.IsValid()) { return bCreated; }
    const auto A = OldComponent->AsObject(), B = After->GetObjectField(TEXT("selected_perception"));
    if (Text(A->GetObjectField(TEXT("unselected_properties")).ToSharedRef()) != Text(B->GetObjectField(TEXT("unselected_properties")).ToSharedRef())) { return false; }
    TArray<TSharedPtr<FJsonValue>> OldSenses, NewSenses;
    for (const auto& S : A->GetArrayField(TEXT("sense_configs"))) { if (!S->AsObject()->HasField(TEXT("stored_max_age"))) { OldSenses.Add(S); } }
    for (const auto& S : B->GetArrayField(TEXT("sense_configs"))) { if (!S->AsObject()->HasField(TEXT("stored_max_age"))) { NewSenses.Add(S); } }
    Old->Values.Reset(); New->Values.Reset(); Old->SetArrayField(TEXT("senses"), OldSenses); New->SetArrayField(TEXT("senses"), NewSenses);
    if (Text(Old) != Text(New)) { return false; }
    const auto OldSight = SelectedSight(Before), NewSight = SelectedSight(After);
    const auto& Prior = A->GetArrayField(TEXT("sense_configs")); const auto& Current = B->GetArrayField(TEXT("sense_configs"));
    if (Current.Num() != Prior.Num() + (OldSight.IsValid() ? 0 : 1)) { return false; }
    for (int32 I = 0; I < Prior.Num(); ++I) { if (Text(Prior[I]->AsObject()->GetObjectField(TEXT("identity")).ToSharedRef()) != Text(Current[I]->AsObject()->GetObjectField(TEXT("identity")).ToSharedRef())) { return false; } }
    return !OldSight.IsValid() || (NewSight.IsValid() && Text(OldSight->GetObjectField(TEXT("identity")).ToSharedRef()) == Text(NewSight->GetObjectField(TEXT("identity")).ToSharedRef()) && Text(OldSight->GetObjectField(TEXT("unselected_properties")).ToSharedRef()) == Text(NewSight->GetObjectField(TEXT("unselected_properties")).ToSharedRef()));
}
bool ValuesMatch(const TSharedRef<FJsonObject>& State, float Radius, float LoseRadius, float Angle, float Age, bool Enemies, bool Neutrals, bool Friendlies)
{
    const auto S = SelectedSight(State); if (!S.IsValid()) { return false; } const auto A = S->GetObjectField(TEXT("detection_by_affiliation"));
    return State->GetObjectField(TEXT("selected_perception"))->GetStringField(TEXT("dominant_sense")) == TEXT("/Script/AIModule.AISense_Sight")
        && S->GetStringField(TEXT("implementation")) == TEXT("/Script/AIModule.AISense_Sight") && S->GetNumberField(TEXT("sight_radius")) == Radius && S->GetNumberField(TEXT("lose_sight_radius")) == LoseRadius
        && S->GetNumberField(TEXT("peripheral_vision_angle_degrees")) == Angle && S->GetNumberField(TEXT("stored_max_age")) == Age
        && S->GetNumberField(TEXT("effective_max_age")) == (Age == 0 ? FAIStimulus::NeverHappenedAge : Age) && S->GetBoolField(TEXT("starts_enabled")) && S->GetNumberField(TEXT("auto_success_range_from_last_seen_location")) == -1.0
        && A->GetBoolField(TEXT("detect_enemies")) == Enemies && A->GetBoolField(TEXT("detect_neutrals")) == Neutrals && A->GetBoolField(TEXT("detect_friendlies")) == Friendlies;
}
void LegacyFields(FResult& R, FContext& C, const TSharedRef<FJsonObject>& State)
{
    R.Data->SetStringField(TEXT("controller_generated_class"), C.Blueprint->GeneratedClass->GetPathName()); R.Data->SetStringField(TEXT("component_name"), C.Selected ? C.Selected->GetVariableName().ToString() : TEXT(""));
    R.Data->SetBoolField(TEXT("perception_component_configured"), C.Selected != nullptr); R.Data->SetStringField(TEXT("component_class"), C.Selected ? C.Selected->ComponentClass->GetName() : TEXT("")); R.Data->SetStringField(TEXT("component_class_path"), C.Selected ? C.Selected->ComponentClass->GetPathName() : TEXT(""));
    const auto S = SelectedSight(State); R.Data->SetBoolField(TEXT("sight_configured"), S.IsValid());
    if (S.IsValid())
    {
        R.Data->SetStringField(TEXT("dominant_sense"), State->GetObjectField(TEXT("selected_perception"))->GetStringField(TEXT("dominant_sense")));
        for (const TCHAR* K : { TEXT("sight_radius"), TEXT("lose_sight_radius"), TEXT("peripheral_vision_angle_degrees"), TEXT("stored_max_age"), TEXT("effective_max_age") }) { R.Data->SetNumberField(K, S->GetNumberField(K)); }
        R.Data->SetNumberField(TEXT("max_age"), S->GetNumberField(TEXT("effective_max_age"))); R.Data->SetBoolField(TEXT("max_age_means_never"), S->GetBoolField(TEXT("max_age_means_never"))); R.Data->SetBoolField(TEXT("starts_enabled"), S->GetBoolField(TEXT("starts_enabled"))); R.Data->SetObjectField(TEXT("detection_by_affiliation"), S->GetObjectField(TEXT("detection_by_affiliation")));
    }
}
struct FObservation
{
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), NativeSources = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), ParentState = MakeShared<FJsonObject>();
    bool bComplete = false;
};
bool Capture(FContext& C, FObservation& O, FResult& R, const TCHAR* Key)
{
    if (!C.GeneratedParents)
    {
    
        auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.NativeSources); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); J->SetObjectField(TEXT("disk"), O.DiskState); R.Data->SetObjectField(Key, J);
        const bool StateOK = Observe(C, O.State), SourceOK = Sources(O.NativeSources), DirtyOK = Dirty(C.Package, O.OtherDirty), DiskOK = Disk(C.Package, O.DiskState);
        J->SetBoolField(TEXT("state_complete"), StateOK); J->SetBoolField(TEXT("native_sources_complete"), SourceOK); J->SetBoolField(TEXT("other_dirty_complete"), DirtyOK); J->SetBoolField(TEXT("disk_complete"), DiskOK);
        O.bComplete = StateOK && SourceOK && DirtyOK && DiskOK&&(!C.LiveInstances.IsValid()||C.LiveInstances->Observe(Key));if(O.bComplete&&C.LiveInstances.IsValid())R.bKnown=true;J->SetBoolField(TEXT("complete"), O.bComplete); return O.bComplete;
    
    }
    auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.NativeSources); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); J->SetObjectField(TEXT("disk"), O.DiskState); R.Data->SetObjectField(Key, J);
    if (!GeneratedGuard(C, R)) { J->SetBoolField(TEXT("complete"), false); return false; }
    const bool StateOK = Observe(C, O.State) && GeneratedGuard(C, R);
    const bool SourceOK = StateOK && Sources(O.NativeSources) && GeneratedGuard(C, R);
    const bool DirtyOK = SourceOK && Dirty(C.Package, O.OtherDirty) && GeneratedGuard(C, R);
    const bool DiskOK = DirtyOK && Disk(C.Package, O.DiskState) && GeneratedGuard(C, R);
    const bool ParentOK = DiskOK && ParentSources(C, O.ParentState, R) && GeneratedGuard(C, R);
    J->SetBoolField(TEXT("state_complete"), StateOK); J->SetBoolField(TEXT("native_sources_complete"), SourceOK); J->SetBoolField(TEXT("other_dirty_complete"), DirtyOK); J->SetBoolField(TEXT("disk_complete"), DiskOK);
    if (C.GeneratedParents) { J->SetObjectField(TEXT("generated_parent_sources"), O.ParentState); J->SetBoolField(TEXT("generated_parent_sources_complete"), ParentOK); }
    O.bComplete = StateOK && SourceOK && DirtyOK && DiskOK && ParentOK&&(!C.LiveInstances.IsValid()||C.LiveInstances->Observe(Key)); J->SetBoolField(TEXT("complete"), O.bComplete);
    if(O.bComplete&&C.LiveInstances.IsValid())R.bKnown=true;
    if (O.bComplete && C.GeneratedParents) { if (!C.ParentBaseline.IsValid()) { C.ParentBaseline = O.ParentState; } R.bKnown = true; }
    return O.bComplete;
}
bool ProtectSources(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, TArray<TSharedPtr<FJsonValue>>& Created)
{
    if (A->Values.Num() != B->Values.Num()) { return false; }
    for (const auto& Pair : A->Values)
    {
        const auto Value = B->TryGetField(Pair.Key.ToView()); if (!Value.IsValid() || Pair.Value->Type != EJson::Object || Value->Type != EJson::Object) { return false; }
        const auto X = Pair.Value->AsObject(), Y = Value->AsObject();
        if (X->GetBoolField(TEXT("cdo_exists"))) { if (Text(X.ToSharedRef()) != Text(Y.ToSharedRef())) { return false; } }
        else
        {
            for (const TCHAR* K : { TEXT("path"), TEXT("class"), TEXT("outer"), TEXT("object_flags"), TEXT("class_flags") }) { if (X->GetStringField(K) != Y->GetStringField(K)) { return false; } }
            if (Y->GetBoolField(TEXT("cdo_exists"))) { Created.Add(MakeShared<FJsonValueString>(Y->GetObjectField(TEXT("existing_cdo"))->GetStringField(TEXT("path")))); }
        }
    }
    return true;
}
bool SelectedIdentityPreserved(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, const FGuid Guid, bool bCreated)
{
    const FString Id = Guid.ToString(); TSharedPtr<FJsonObject> A, B;
    for (const auto& Row : Before->GetArrayField(TEXT("scs_nodes"))) { if (Row->AsObject()->GetStringField(TEXT("variable_guid")) == Id) { A = Row->AsObject(); } }
    for (const auto& Row : After->GetArrayField(TEXT("scs_nodes"))) { if (Row->AsObject()->GetStringField(TEXT("variable_guid")) == Id) { B = Row->AsObject(); } }
    if (!B.IsValid() || B->GetObjectField(TEXT("template"))->GetStringField(TEXT("class")) != TEXT("/Script/AIModule.AIPerceptionComponent")) { return false; }
    if (bCreated) { return !A.IsValid() && B->GetArrayField(TEXT("children")).IsEmpty(); }
    if (!A.IsValid()) { return false; }
    for (const TCHAR* K : { TEXT("path"), TEXT("class"), TEXT("outer"), TEXT("variable_guid"), TEXT("variable_name") }) { if (A->GetStringField(K) != B->GetStringField(K)) { return false; } }
    if (Text(A->GetObjectField(TEXT("template")).ToSharedRef()) != Text(B->GetObjectField(TEXT("template")).ToSharedRef())) { return false; }
    auto X = MakeShared<FJsonObject>(), Y = MakeShared<FJsonObject>(); X->SetArrayField(TEXT("children"), A->GetArrayField(TEXT("children"))); Y->SetArrayField(TEXT("children"), B->GetArrayField(TEXT("children"))); return Text(X) == Text(Y);
}
bool Fences(FContext& C, const FObservation& Before, const FObservation& After, FResult& R, const FGuid Guid, bool bCreated)
{
    R.bChanged = Text(Before.State) != Text(After.State) || Text(Before.DiskState) != Text(After.DiskState) || Text(Before.OtherDirty) != Text(After.OtherDirty) || Text(Before.NativeSources) != Text(After.NativeSources) || Text(Before.ParentState) != Text(After.ParentState);
    if(C.LiveInstances.IsValid()){const bool Replaced=C.LiveInstances->ReplacementCount>0||C.LiveInstances->RetiredChildCount>0;R.bChanged|=Replaced;R.Data->SetBoolField(TEXT("live_instance_engine_replacement_observed"),Replaced);}
    const bool ParentOK = Text(Before.ParentState) == Text(After.ParentState); if (C.GeneratedParents) { R.Data->SetBoolField(TEXT("generated_parent_sources_preserved"), ParentOK); }
    TArray<TSharedPtr<FJsonValue>> CreatedCDOs; const bool SourceOK = ProtectSources(Before.NativeSources, After.NativeSources, CreatedCDOs);
    const bool StateOK = Preserved(Before.State, After.State, Guid, bCreated) && SelectedIdentityPreserved(Before.State, After.State, Guid, bCreated);
    R.Data->SetArrayField(TEXT("implicit_native_cdo_creation_observed"), CreatedCDOs); R.Data->SetBoolField(TEXT("source_preserved"), SourceOK); R.Data->SetBoolField(TEXT("unselected_state_preserved"), StateOK);
    const bool OtherOK=C.LiveInstances.IsValid()?C.LiveInstances->DirtyEqual(Before.OtherDirty,After.OtherDirty,TEXT("before_to_retained")):Text(Before.OtherDirty)==Text(After.OtherDirty);
    R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), OtherOK); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), Text(Before.DiskState) == Text(After.DiskState)); R.Data->SetBoolField(TEXT("observations_complete"), true);
    if (!ParentOK || !SourceOK || !StateOK || !OtherOK || Text(Before.DiskState) != Text(After.DiskState)) { return Fail(R, TEXT("perception_preservation_failed"), TEXT("Complete actual retained observations failed native source/unselected state/disk/other dirty preservation; save suppressed.")); }
    LegacyFields(R, C, After.State); return true;
}
bool Retained(FContext& C, const FObservation& Before, FObservation& After, FResult& R, const FGuid Guid, bool bCreated)
{
    if (!Capture(C, After, R, TEXT("after"))) { return Fail(R, TEXT("perception_retained_state_unobservable"), TEXT("Actual retained Blueprint/template/config/source prefix is incomplete; no further compile/save/retry."), true); }
    return Fences(C, Before, After, R, Guid, bCreated);
}
// Only exact same-path/class/outer audited target-owned identities may acquire normal exported-object save flags.
bool SavedEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& Package, const FString& At, TArray<TSharedPtr<FJsonValue>>& Transitions)
{
    if (!A.IsValid() || !B.IsValid() || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object)
    {
        const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num()) { return false; }
        for (const auto& Pair : X->Values)
        {
            const FString Key(*Pair.Key); const auto V = Y->TryGetField(Pair.Key.ToView());
            if (Key == TEXT("object_flags") && X->HasField(TEXT("path")) && X->HasField(TEXT("class")) && X->HasField(TEXT("outer")) && V.IsValid() && Pair.Value->Type == EJson::String && V->Type == EJson::String && X->GetStringField(TEXT("path")).StartsWith(Package + TEXT(".")))
            {
                const FString Left = Pair.Value->AsString(), Right = V->AsString(); if (Left == Right) { continue; }
                const uint64 Old = FCString::Strtoui64(*Left, nullptr, 10), Now = FCString::Strtoui64(*Right, nullptr, 10);
                if (Left != FString::Printf(TEXT("%llu"), Old) || Right != FString::Printf(TEXT("%llu"), Now) || Now != (Old | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; }
                auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), X->GetStringField(TEXT("path"))); Row->SetStringField(TEXT("before"), Left); Row->SetStringField(TEXT("after"), Right); Transitions.Add(MakeShared<FJsonValueObject>(Row)); continue;
            }
            if (!SavedEqual(Pair.Value, V, Package, At + TEXT("/") + Key, Transitions)) { return false; }
        }
        return true;
    }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!SavedEqual(X[I], Y[I], Package, At + TEXT("/") + FString::FromInt(I), Transitions)) { return false; } } return true; }
    if (A->Type == EJson::String) { return A->AsString() == B->AsString(); }
    if (A->Type == EJson::Number) { return A->AsNumber() == B->AsNumber(); }
    if (A->Type == EJson::Boolean) { return At == TEXT("/package_dirty") ? !B->AsBool() : A->AsBool() == B->AsBool(); }
    return A->Type == EJson::Null;
}
bool Save(FContext& C, const FObservation& Expected, FResult& R)
{
    if (!R.bSaveRequested) { return true; }
    if (!GeneratedGuard(C, R)) { return false; }
    const auto Prefix = R.Data->GetObjectField(TEXT("after"));
    for (const TCHAR* K : { TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("observations_complete"), TEXT("other_package_dirty_preserved"), TEXT("disk_preserved_before_save"), TEXT("readback_confirmed") }) { Prefix->SetBoolField(K, R.Data->GetBoolField(K)); }
    Prefix->SetBoolField(TEXT("protected_prefix_preserved"), true); Prefix->SetArrayField(TEXT("implicit_native_cdo_creation_observed"), R.Data->GetArrayField(TEXT("implicit_native_cdo_creation_observed")));
    if (C.GeneratedParents) { Prefix->SetBoolField(TEXT("generated_parent_sources_preserved"), R.Data->GetBoolField(TEXT("generated_parent_sources_preserved"))); R.Data->SetBoolField(TEXT("generated_parent_sources_preserved"), false); }
    for (const TCHAR* K : { TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("observations_complete"), TEXT("other_package_dirty_preserved"), TEXT("readback_confirmed") }) { R.Data->SetBoolField(K, false); }
    R.Data->RemoveField(TEXT("disk_preserved_before_save")); R.Data->SetArrayField(TEXT("implicit_native_cdo_creation_observed"), {});
    R.Stage = TEXT("save_blueprint"); R.bSaveAttempted = true; R.Data->SetNumberField(TEXT("save_attempt_count"), 1);
    auto Receipt = MakeShared<FJsonObject>(); Receipt->SetNumberField(TEXT("attempt_index"), 1); Receipt->SetStringField(TEXT("asset_path"), C.Blueprint->GetPathName()); Receipt->SetBoolField(TEXT("package_dirty_before"), C.Blueprint->GetOutermost()->IsDirty());
    if (C.GeneratedParents) { R.bKnown = false; }
    R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Receipt) });
    const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(C.Blueprint.Get(), false); R.bSaved = R.bSaved || Saved; Receipt->SetBoolField(TEXT("success"), Saved);
    // Adopt a genuine returned True before a callback/identity observer can fail.
    if (!GeneratedGuard(C, R)) { Receipt->SetBoolField(TEXT("post_save_observation_skipped_after_unsafe_callback"), true); return false; }
    Receipt->SetBoolField(TEXT("package_dirty_after"), C.Blueprint->GetOutermost()->IsDirty());
    FObservation AfterSave; const bool Complete = Capture(C, AfterSave, R, TEXT("after_save")); R.Data->SetBoolField(TEXT("save_observations_complete"), Complete);
    R.Data->SetBoolField(TEXT("observations_complete"), Complete);
    if (!Complete) { return Fail(R, TEXT("perception_save_state_unobservable"), TEXT("Actual exact save result is retained; post-save observer incomplete, no writer retry."), true); }
    if (!Saved) { return Fail(R, TEXT("perception_save_failed"), TEXT("Exact forced Blueprint save returned false; disk prefix retained and no save retry is permitted."), true); }
    TArray<TSharedPtr<FJsonValue>> Transitions; const bool StateOK = SavedEqual(MakeShared<FJsonValueObject>(Expected.State), MakeShared<FJsonValueObject>(AfterSave.State), C.Package, TEXT(""), Transitions);
    const bool SourceOK = Text(Expected.NativeSources) == Text(AfterSave.NativeSources) && Text(Expected.ParentState) == Text(AfterSave.ParentState), OtherOK = C.LiveInstances.IsValid()?C.LiveInstances->DirtyEqual(Expected.OtherDirty,AfterSave.OtherDirty,TEXT("retained_to_save")):Text(Expected.OtherDirty)==Text(AfterSave.OtherDirty);
    const bool DiskOK = AfterSave.DiskState->GetBoolField(TEXT("exists")) && AfterSave.DiskState->GetNumberField(TEXT("bytes")) > 0 && !C.Blueprint->GetOutermost()->IsDirty();
    R.Data->SetArrayField(TEXT("save_flag_transitions"), Transitions); R.Data->SetBoolField(TEXT("save_state_preserved"), StateOK); R.Data->SetBoolField(TEXT("save_sources_preserved"), SourceOK); R.Data->SetBoolField(TEXT("save_other_dirty_preserved"), OtherOK); R.Data->SetBoolField(TEXT("persistence_verified"), DiskOK);
    R.Data->SetBoolField(TEXT("source_preserved"), SourceOK); R.Data->SetBoolField(TEXT("unselected_state_preserved"), StateOK); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), OtherOK); R.Data->SetBoolField(TEXT("readback_confirmed"), StateOK && Prefix->GetBoolField(TEXT("readback_confirmed")));
    if (C.GeneratedParents) { R.Data->SetBoolField(TEXT("generated_parent_sources_preserved"), Text(Expected.ParentState) == Text(AfterSave.ParentState)); }
    R.Data->SetStringField(TEXT("save_state_comparison_scope"), TEXT("Full same-reader state except target package_dirty=false and unchanged-or-exact RF_WasLoaded|RF_LoadCompleted additions on same-path/class/outer already-audited target-owned identities; no flag masking or value normalization. All raw states remain retained."));
    if (!StateOK || !SourceOK || !OtherOK || !DiskOK) { return Fail(R, TEXT("perception_save_postcondition_failed"), TEXT("Successful exact save is retained but complete source/state/disk/other-dirty verification failed; no writer retry.")); } return true;
}
bool EditableFields()
{
    UClass* Sight = UAISenseConfig_Sight::StaticClass();
    for (const TCHAR* K : { TEXT("Implementation"), TEXT("SightRadius"), TEXT("LoseSightRadius"), TEXT("PeripheralVisionAngleDegrees"), TEXT("DetectionByAffiliation"), TEXT("AutoSuccessRangeFromLastSeenLocation"), TEXT("MaxAge"), TEXT("bStartsEnabled") })
    { FProperty* P = FindFProperty<FProperty>(Sight, K); if (!P || P->ArrayDim != 1 || !P->HasAnyPropertyFlags(CPF_Edit) || P->HasAnyPropertyFlags(CPF_EditConst)) { return false; } }
    return true;
}
}

FString Inspect(const FString& ControllerBlueprintPath, const FString& ComponentName)
{
    FResult R; Init(R, false, false); FContext C; if (!Prepare(ControllerBlueprintPath, ComponentName, C, R)) { return Encode(R); }
    FObservation Before, After; if (!Capture(C, Before, R, TEXT("before")) || !Capture(C, After, R, TEXT("after"))) { Fail(R, TEXT("perception_inspection_unobservable"), TEXT("Find-only local native Blueprint/template/config observation exceeded bounds or failed an ownership/source fence.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("observations_complete"), true); const bool Same = Text(Before.State) == Text(After.State) && Text(Before.NativeSources) == Text(After.NativeSources) && Text(Before.OtherDirty) == Text(After.OtherDirty) && Text(Before.DiskState) == Text(After.DiskState) && Text(Before.ParentState) == Text(After.ParentState);
    R.bChanged = !Same; R.Data->SetBoolField(TEXT("source_preserved"), Same); R.Data->SetBoolField(TEXT("unselected_state_preserved"), Same); LegacyFields(R, C, After.State);
    if (!Same) { Fail(R, TEXT("perception_inspection_changed"), TEXT("Repeated find-only same-reader observations differ; no configuration/compile/save was dispatched.")); return Encode(R); }
    R.Stage = TEXT("complete"); R.bSuccess = true; R.bPost = true; return Encode(R);
}
bool ConfigureBusy=false;
FString Configure(const FString& ControllerBlueprintPath, const FString& ComponentName, double SightRadius, double LoseSightRadius,
    double PeripheralVisionAngleDegrees, double MaxAge, bool bDetectEnemies, bool bDetectNeutrals, bool bDetectFriendlies, bool bReplaceExistingConfig, bool bSaveBlueprint)
{
    FResult R; Init(R, true, bSaveBlueprint);if(ConfigureBusy){Fail(R,TEXT("perception_configuration_reentrant"),TEXT("A perception configuration request is already executing; no nested writer/retry."));return Encode(R);}TGuardValue<bool> ConfigureGuard(ConfigureBusy,true); R.Data->SetBoolField(TEXT("replace_existing_config"), bReplaceExistingConfig);
    for (const double Number : { SightRadius, LoseSightRadius, PeripheralVisionAngleDegrees, MaxAge }) { if (!FMath::IsFinite(Number) || FMath::Abs(Number) > static_cast<double>(MAX_flt)) { Fail(R, TEXT("perception_numeric_inputs_invalid"), TEXT("Finite values representable by original native float fields are required before mutation.")); return Encode(R); } }
    const float Radius = FMath::Max(0.f, static_cast<float>(SightRadius)), LoseRadius = FMath::Max(Radius, static_cast<float>(LoseSightRadius)), Angle = FMath::Clamp(static_cast<float>(PeripheralVisionAngleDegrees), 0.f, 180.f), Age = FMath::Max(0.f, static_cast<float>(MaxAge));
    auto Requested = MakeShared<FJsonObject>(); Requested->SetNumberField(TEXT("sight_radius"), Radius); Requested->SetNumberField(TEXT("lose_sight_radius"), LoseRadius); Requested->SetNumberField(TEXT("peripheral_vision_angle_degrees"), Angle); Requested->SetNumberField(TEXT("stored_max_age"), Age); Requested->SetBoolField(TEXT("detect_enemies"), bDetectEnemies); Requested->SetBoolField(TEXT("detect_neutrals"), bDetectNeutrals); Requested->SetBoolField(TEXT("detect_friendlies"), bDetectFriendlies); R.Data->SetObjectField(TEXT("requested_config"), Requested);
    FContext C; if (!Prepare(ControllerBlueprintPath, ComponentName, C, R)) { return Encode(R); } FObservation Before;
    if (!Capture(C, Before, R, TEXT("before"))) { Fail(R, TEXT("perception_preflight_unobservable"), TEXT("Complete loaded native source/template/config/graph/dirty/disk observation is required before Modify/allocation.")); return Encode(R); }
    if (!EditableFields()) { Fail(R, TEXT("perception_editable_fields_unsupported"), TEXT("Exact original editable native Sight field permission metadata is required before mutation.")); return Encode(R); }
    if (!bReplaceExistingConfig && SelectedSight(Before.State).IsValid()) { Fail(R, TEXT("perception_sight_config_exists"), TEXT("Sight configuration exists and replace_existing_config is false; known prewrite refusal.")); return Encode(R); }
    R.Stage = TEXT("configure_perception"); UBlueprint* B = C.Blueprint.Get(); USimpleConstructionScript* SCS = B->SimpleConstructionScript;
    if (!GeneratedGuard(C, R)) { return Encode(R); } R.bAttempted = true; if (C.LiveInstances.IsValid())R.bKnown=false;if (C.GeneratedParents) { R.bKnown = false; }
    B->Modify(); R.Data->SetNumberField(TEXT("modify_calls"), 1); if (!GeneratedGuard(C, R)) { return Encode(R); }
    SCS->Modify(); R.Data->SetNumberField(TEXT("modify_calls"), 2); if (!GeneratedGuard(C, R)) { return Encode(R); }
    const bool bCreated = C.Selected == nullptr;
    if (bCreated)
    {
        R.Data->SetNumberField(TEXT("component_allocation_count"), 1); USCS_Node* NewNode = SCS->CreateNode(UAIPerceptionComponent::StaticClass(), FName(*ComponentName));
        if (!GeneratedGuard(C, R)) { R.Data->SetBoolField(TEXT("allocated_component_observation_skipped_after_unsafe_callback"), true); return Encode(R); } C.Selected = NewNode;
        R.Data->SetObjectField(TEXT("allocated_component_node"), Identity(C.Selected));
        if (!Loaded(C.Selected) || C.Selected->GetOuter() != SCS || !C.Selected->VariableGuid.IsValid() || C.Selected->GetVariableName() != FName(*ComponentName) || !Loaded(C.Selected->ComponentTemplate) || C.Selected->ComponentTemplate->GetClass() != UAIPerceptionComponent::StaticClass() || C.Selected->ComponentTemplate->GetOuter() != B->GeneratedClass)
        { FObservation Partial; Capture(C, Partial, R, TEXT("after")); Fail(R, TEXT("perception_component_creation_failed"), TEXT("Public SCS factory actual allocated prefix failed exact name/GUID/template owner admission; no further mutation/compile/save."), true); return Encode(R); }
        if (!FreezeChild(C, R, true)) { return Encode(R); }
        SCS->AddNode(C.Selected); R.Data->SetBoolField(TEXT("component_created"), true); if (!GeneratedGuard(C, R)) { return Encode(R); }
    }
    const FGuid Guid = C.Selected->VariableGuid; TStrongObjectPtr<USCS_Node> KeepNode(C.Selected); auto* Component = Cast<UAIPerceptionComponent>(C.Selected->ComponentTemplate); TStrongObjectPtr<UAIPerceptionComponent> KeepComponent(Component);
    if (!Loaded(Component) || Component->GetClass() != UAIPerceptionComponent::StaticClass() || Component->IsRegistered() || Component->GetOwner() || Component->GetWorld())
    { FObservation Partial; Capture(C, Partial, R, TEXT("after")); Fail(R, TEXT("perception_component_retained_unsupported"), TEXT("Actual retained component is outside the exact unregistered template mutation scope."), true); return Encode(R); }
    Component->Modify(); R.Data->SetNumberField(TEXT("modify_calls"), 3); if (!GeneratedGuard(C, R)) { return Encode(R); } auto* Sight = Component->GetSenseConfig<UAISenseConfig_Sight>();
    if (!Sight)
    {
        R.Data->SetNumberField(TEXT("sight_config_allocation_count"), 1); Sight = NewObject<UAISenseConfig_Sight>(Component, NAME_None, RF_Transactional);
        if (!GeneratedGuard(C, R)) { R.Data->SetBoolField(TEXT("allocated_sight_observation_skipped_after_unsafe_callback"), true); return Encode(R); }
        R.Data->SetObjectField(TEXT("allocated_sight_config"), Identity(Sight)); R.Data->SetBoolField(TEXT("sight_config_created"), Sight != nullptr);
    }
    TStrongObjectPtr<UAISenseConfig_Sight> KeepSight(Sight);
    if (!Loaded(Sight) || Sight->GetClass() != UAISenseConfig_Sight::StaticClass() || Sight->GetOuter() != Component)
    { FObservation Partial; Capture(C, Partial, R, TEXT("after")); Fail(R, TEXT("perception_sight_creation_failed"), TEXT("Actual Sight allocation/selection failed exact native owner/type admission; partial prefix retained."), true); return Encode(R); }
    if (!KeepChild(C, Sight, R)) { return Encode(R); }
    Sight->Modify(); R.Data->SetNumberField(TEXT("modify_calls"), 4); if (!GeneratedGuard(C, R)) { return Encode(R); } Sight->Implementation = UAISense_Sight::StaticClass(); Sight->SightRadius = Radius; Sight->LoseSightRadius = LoseRadius; Sight->PeripheralVisionAngleDegrees = Angle; Sight->AutoSuccessRangeFromLastSeenLocation = -1.f;
    Sight->DetectionByAffiliation.bDetectEnemies = bDetectEnemies; Sight->DetectionByAffiliation.bDetectNeutrals = bDetectNeutrals; Sight->DetectionByAffiliation.bDetectFriendlies = bDetectFriendlies; Sight->SetMaxAge(Age); Sight->SetStartsEnabled(true);
    if (C.GeneratedParents) { C.ExpectedSight.Reset(Sight); C.SightAttached = true; }
    R.Data->SetNumberField(TEXT("configure_sense_calls"), 1); Component->ConfigureSense(*Sight); if (!GeneratedGuard(C, R)) { return Encode(R); }
    R.Data->SetNumberField(TEXT("set_dominant_sense_calls"), 1); Component->SetDominantSense(UAISense_Sight::StaticClass()); if (!GeneratedGuard(C, R)) { return Encode(R); }
    R.Data->SetNumberField(TEXT("listener_update_calls"), 1); R.Data->SetBoolField(TEXT("implicit_world_callback_effects_possible"), true); Component->RequestStimuliListenerUpdate(); if (!GeneratedGuard(C, R)) { return Encode(R); }
    FObservation Configured; if (!Capture(C, Configured, R, TEXT("after_configuration"))) { Fail(R, TEXT("perception_configuration_unobservable"), TEXT("Configured actual template/config prefix incomplete; structural/full compilation and save suppressed."), true); return Encode(R); }
    if (!ValuesMatch(Configured.State, Radius, LoseRadius, Angle, Age, bDetectEnemies, bDetectNeutrals, bDetectFriendlies)) { R.bChanged = Text(Before.State) != Text(Configured.State); Fail(R, TEXT("perception_configuration_readback_failed"), TEXT("Complete actual public configuration readback differs; compilation/save suppressed.")); return Encode(R); }
    // This callback prefix is already completely observed: apply the same fences before dispatching any compiler work.
    if (!Fences(C, Before, Configured, R, Guid, bCreated)) { return Encode(R); }
    const auto Prefix = R.Data->GetObjectField(TEXT("after_configuration")); Prefix->SetBoolField(TEXT("source_preserved"), R.Data->GetBoolField(TEXT("source_preserved"))); Prefix->SetBoolField(TEXT("unselected_state_preserved"), R.Data->GetBoolField(TEXT("unselected_state_preserved")));
    Prefix->SetBoolField(TEXT("other_package_dirty_preserved"), R.Data->GetBoolField(TEXT("other_package_dirty_preserved"))); Prefix->SetBoolField(TEXT("disk_preserved_before_save"), R.Data->GetBoolField(TEXT("disk_preserved_before_save"))); Prefix->SetBoolField(TEXT("protected_prefix_preserved"), true); Prefix->SetArrayField(TEXT("implicit_native_cdo_creation_observed"), R.Data->GetArrayField(TEXT("implicit_native_cdo_creation_observed")));
    if (C.GeneratedParents) { Prefix->SetBoolField(TEXT("generated_parent_sources_preserved"), R.Data->GetBoolField(TEXT("generated_parent_sources_preserved"))); R.Data->SetBoolField(TEXT("generated_parent_sources_preserved"), false); }
    // Subsequent compiler work has not yet been observed. Prefix proof must not masquerade as final preservation.
    for (const TCHAR* K : { TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("observations_complete"), TEXT("other_package_dirty_preserved"), TEXT("disk_preserved_before_save") }) { R.Data->SetBoolField(K, false); } R.Data->SetArrayField(TEXT("implicit_native_cdo_creation_observed"), {});
    R.Stage = TEXT("structural_refresh"); R.Data->SetNumberField(TEXT("structural_refresh_calls"), 1); R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"), true);
    if (!GeneratedGuard(C, R)) { return Encode(R); } if (C.GeneratedParents) { R.bKnown = false; }
    {bool NoLiveWindow=false;TGuardValue<bool> Window(C.LiveInstances.IsValid()?C.LiveInstances->CompilerWindow:NoLiveWindow,true);if(C.LiveInstances.IsValid())R.bKnown=false;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);}
    if(C.LiveInstances.IsValid()&&!C.LiveInstances->CompilerReturned())return Encode(R);
    if (!AdoptCompilerChild(C, R, Guid)) { return Encode(R); }
    R.Stage = TEXT("compile_blueprint"); FCompilerResultsLog Log;
    if (!GeneratedGuard(C, R)) { return Encode(R); }
    R.bCompileAttempted = true; R.Data->SetNumberField(TEXT("full_compile_attempts"), 1);
    {bool NoLiveWindow=false;TGuardValue<bool> Window(C.LiveInstances.IsValid()?C.LiveInstances->CompilerWindow:NoLiveWindow,true);if(C.LiveInstances.IsValid())R.bKnown=false;FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);}
    if(C.LiveInstances.IsValid()&&!C.LiveInstances->CompilerReturned())return Encode(R);
    R.Data->SetNumberField(TEXT("compile_error_count"), Log.NumErrors); R.Data->SetNumberField(TEXT("compile_warning_count"), Log.NumWarnings);
    if (!AdoptCompilerChild(C, R, Guid)) { return Encode(R); }
    R.Data->SetNumberField(TEXT("compile_error_count"), Log.NumErrors); R.Data->SetNumberField(TEXT("compile_warning_count"), Log.NumWarnings); TArray<TSharedPtr<FJsonValue>> Messages; for (const auto& Message : Log.Messages) { if (Messages.Num() >= 128) { break; } Messages.Add(MakeShared<FJsonValueString>(Message->ToText().ToString())); } R.Data->SetArrayField(TEXT("compile_messages"), Messages);
    R.bCompileKnown = Loaded(B) && !B->bBeingCompiled && !B->bQueuedForCompilation; R.bCompiled = R.bCompileKnown && Log.NumErrors == 0 && (B->Status == BS_UpToDate || B->Status == BS_UpToDateWithWarnings);
    if (R.bCompiled) { B->MarkPackageDirty(); R.Data->SetNumberField(TEXT("package_dirty_mark_calls"), 1); if (!GeneratedGuard(C, R)) { return Encode(R); } }
    FContext Current; if (C.GeneratedParents) { Current = MoveTemp(C); } else if (!Prepare(ControllerBlueprintPath, ComponentName, Current, R,C.LiveInstances)) { FObservation Partial; Capture(C, Partial, R, TEXT("after")); Fail(R, TEXT("perception_postcompile_context_unobservable"), TEXT("One explicit full compile returned but current generated classes/SCS selection could not be admitted; actual prefix retained, no save/retry."), true); return Encode(R); }
    FObservation After; if (!Retained(Current, Before, After, R, Guid, bCreated)) { return Encode(R); }
    if (!R.bCompileKnown) { Fail(R, TEXT("perception_compile_outcome_unobservable"), TEXT("Compile was attempted once; final compile state remains unobserved, no save/retry."), true); return Encode(R); }
    if (!R.bCompiled) { Fail(R, TEXT("perception_compile_failed"), TEXT("One explicit full compile reported errors or a nonready status; complete retained state and compiler messages remain, no save.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("readback_confirmed"), ValuesMatch(After.State, Radius, LoseRadius, Angle, Age, bDetectEnemies, bDetectNeutrals, bDetectFriendlies));
    if (!R.Data->GetBoolField(TEXT("readback_confirmed"))) { Fail(R, TEXT("perception_postcompile_readback_failed"), TEXT("Complete compiled template/config readback differs from clamped original configuration; save suppressed.")); return Encode(R); }
    if (!Save(Current, After, R)) { return Encode(R); } R.Stage = TEXT("complete"); R.bSuccess = true; R.bPost = true; return Encode(R);
}
}
