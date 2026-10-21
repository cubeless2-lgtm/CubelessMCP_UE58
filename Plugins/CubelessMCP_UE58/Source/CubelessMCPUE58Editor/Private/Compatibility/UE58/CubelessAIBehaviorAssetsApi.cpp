// Standard patrol/chase trees through public AIModule/Editor APIs; no runtime helper class.
// Legacy UnrealMCPAICommands.cpp:1920-2232 supplies the native11 contract and three shapes.
// Installed BehaviorTreeGraph.h:42-57 / .cpp:189,989 supply public runtime-to-editor graph generation.
// BlackboardData.h public refresh APIs and reflected/native task/decorator fields initialize only new owned objects.
// AIGraph::UnlockUpdates dispatches UpdateAsset; exact BB save first, then locked BT save and conditional true+dirty second save.
// IZ: isolated saved native-pair ordered replacement; actual AI task execution remains unverified.
#include "Compatibility/UE58/CubelessAIBehaviorAssetsApi.h"
#include "Compatibility/UE58/CubelessBlackboardApi.h"
#include "Compatibility/UE58/CubelessBehaviorTreeApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AISystem.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/ReferencerFinder.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BehaviorTree/Tasks/BTTask_MoveDirectlyToward.h"
#include "BehaviorTree/Tasks/BTTask_RotateToFaceBBEntry.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::AIBehaviorAssets
{
namespace
{
constexpr int32 MaxProperties = 256, MaxText = 1048576, MaxPackages = 4096;
constexpr int64 MaxDiskBytes = 16 * 1024 * 1024;
struct FResult
{
    FString Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false;
    bool bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.bSuccess = false; R.Code = Code; R.Error = Error; return false; }
FString Encode(const FResult& R)
{
    const bool BothSaved = R.Data->GetBoolField(TEXT("blackboard_saved")) && R.Data->GetBoolField(TEXT("behavior_tree_saved"));
    R.Data->SetBoolField(TEXT("save_assets_requested"), R.bSaveRequested); R.Data->SetBoolField(TEXT("asset_saved"), BothSaved);
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), TEXT("create_ai_patrol_chase_behavior_assets")); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetBoolField(TEXT("success"), R.bSuccess); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown); J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.bPost); J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false); J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
void Init(FResult& R, bool bSave, bool bReplace)
{
    R.bSaveRequested = bSave;
    for (const TCHAR* K : { TEXT("blackboard_created"), TEXT("behavior_tree_created"), TEXT("blackboard_saved"), TEXT("behavior_tree_saved"),
        TEXT("editor_graph_created"), TEXT("asset_registry_publication_dispatched"), TEXT("package_fully_load_dispatched"), TEXT("implicit_package_load_possible"),
        TEXT("runtime_subtree_verified"), TEXT("editor_subtree_verified"), TEXT("runtime_execution_verified"), TEXT("implicit_engine_work_measured"),
        TEXT("uses_unrealmcp_classes"), TEXT("existing_target_replacement_supported"), TEXT("blackboard_keys_reset_dispatched"),
        TEXT("implicit_root_default_selection_possible"), TEXT("implicit_persistent_key_creation_possible"), TEXT("implicit_property_callback_effects_possible"),
        TEXT("construction_graph_locked_retained"), TEXT("midspawn_rebuild_suppressed_by_lock"), TEXT("construction_raw_before_unlock_complete"), TEXT("construction_raw_after_unlock_complete"),
        TEXT("construction_raw_topology_preserved"), TEXT("construction_runtime_semantic_preserved"), TEXT("construction_single_is_set_logic_normalized") }) { R.Data->SetBoolField(K, false); }
    for (const TCHAR* K : { TEXT("package_allocation_count"), TEXT("asset_allocation_count"), TEXT("runtime_node_allocation_count"), TEXT("key_allocation_count"),
        TEXT("blackboard_cache_refresh_sequences"), TEXT("factory_keys_reset_count"), TEXT("save_attempt_count"), TEXT("asset_delete_count"), TEXT("asset_registry_publication_count"), TEXT("unlock_update_calls"), TEXT("editable_property_notification_count"),
        TEXT("construction_lock_calls"), TEXT("construction_unlock_update_calls"), TEXT("total_graph_unlock_update_calls") }) { R.Data->SetNumberField(K, 0); }
    R.Data->SetBoolField(TEXT("replace_existing_requested"), bReplace); R.Data->SetArrayField(TEXT("delete_calls"), {}); R.Data->SetArrayField(TEXT("save_calls"), {});
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public AIModule, BehaviorTreeEditor, AIGraph, UnrealEd, AssetRegistry and EditorAssetLibrary. No official AI Toolset authoring delegate, ServerRPC or custom runtime class."));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("two native assets plus standard owned keys/runtime subtree/editor graph; cache callbacks, editable BasicOperation notification, graph node default selection, locked graph construction followed by validated public unlock/rebuild, and save/unlock rebuilding are explicit; no AI execution, compile, rollback or writer retry"));
    R.Data->SetStringField(TEXT("replacement_limit"), TEXT("Fresh targets accept replace_existing=true with zero deletions. Existing saved clean exact native isolated targets use original tree-then-board force deletion before recreation. Delete SDK may reset undo and run GC/editor/global callbacks; no atomic rollback, original identity restoration or global purity proof."));
    R.Data->SetStringField(TEXT("unlock_update_scope"), TEXT("unlock_update_calls counts save-time graph unlock only; construction_unlock_update_calls records the separate one-time post-spawn public unlock/rebuild. AddSubNode mid-spawn callbacks are suppressed by the public construction lock."));
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"), O ? O->GetPathName() : TEXT("")); J->SetStringField(TEXT("class"), O ? O->GetClass()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("outer"), O && O->GetOuter() ? O->GetOuter()->GetPathName() : TEXT("")); J->SetStringField(TEXT("object_flags"), O ? FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags())) : TEXT("0")); return J;
}
bool Properties(UObject* O, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(O)) { return false; } int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64) { return false; }
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I)
        {
            FString S; const void* V = P->ContainerPtrToValuePtr<void>(O, I); if (!P->ExportText_Direct(S, V, V, O, PPF_None)) { return false; }
            Chars += S.Len(); if (Chars > MaxText) { return false; } Values.Add(MakeShared<FJsonValueString>(S));
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName());
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(P->GetPropertyFlags()))); Row->SetArrayField(TEXT("values"), Values); J->SetObjectField(P->GetName(), Row);
    }
    return true;
}
TArray<UClass*> Classes()
{
    return { UBlackboardData::StaticClass(), UBlackboardKeyType_Object::StaticClass(), UBlackboardKeyType_Vector::StaticClass(), UBlackboardKeyType_Name::StaticClass(),
        UBehaviorTree::StaticClass(), UAISystem::StaticClass(), UBTComposite_Sequence::StaticClass(), UBTComposite_Selector::StaticClass(),
        UBTTask_MoveDirectlyToward::StaticClass(), UBTTask_RotateToFaceBBEntry::StaticClass(), UBTTask_Wait::StaticClass(), UBTDecorator_Blackboard::StaticClass(),
        UBehaviorTreeGraph::StaticClass(), UBehaviorTreeGraphNode::StaticClass(), UBehaviorTreeGraphNode_Root::StaticClass(), UBehaviorTreeGraphNode_Composite::StaticClass(),
        UBehaviorTreeGraphNode_Task::StaticClass(), UBehaviorTreeGraphNode_Decorator::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass(), UAIGraph::StaticClass(), UAIGraphNode::StaticClass() };
}
bool Sources(TSharedRef<FJsonObject>& J)
{
    auto Ordered = Classes(); Ordered.Sort([](const UClass& A, const UClass& B) { return A.GetPathName() < B.GetPathName(); });
    for (UClass* C : Ordered)
    {
        auto Row = Identity(C); J->SetObjectField(C->GetPathName(), Row); const FString Package = C->GetOutermost()->GetName();
        if (!Loaded(C) || !C->HasAnyClassFlags(CLASS_Native) || C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
            || !C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn) || C->GetOutermost()->IsDirty()
            || (Package != TEXT("/Script/AIModule") && Package != TEXT("/Script/BehaviorTreeEditor") && Package != TEXT("/Script/AIGraph"))) { J->SetStringField(TEXT("failed_class"), C->GetPathName()); return false; }
        Row->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags()))); Row->SetBoolField(TEXT("package_dirty"), false);
        UObject* CDO = C->GetDefaultObject(false); Row->SetBoolField(TEXT("cdo_exists"), CDO != nullptr);
        if (CDO)
        {
            auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); Row->SetObjectField(TEXT("existing_cdo_properties"), P);
            if (!Loaded(CDO) || CDO->GetClass() != C || CDO->GetOuter() != C->GetOuter() || !CDO->HasAnyFlags(RF_ClassDefaultObject) || !Properties(CDO, P)) { J->SetStringField(TEXT("failed_class"), C->GetPathName()); return false; }
        }
    }
    return true;
}
bool ProtectSources(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, bool bAllowLifecycleCDO, TArray<TSharedPtr<FJsonValue>>& Created)
{
    if (Before->Values.Num() != After->Values.Num()) { return false; }
    for (const auto& Pair : Before->Values)
    {
        const auto Found = After->TryGetField(Pair.Key.ToView()); if (!Found.IsValid() || Found->Type != EJson::Object) { return false; }
        const auto A = Pair.Value->AsObject(), B = Found->AsObject(); auto Expected = MakeShared<FJsonObject>(); for (const auto& Field : A->Values) { Expected->SetField(Field.Key, Field.Value); }
        if (!A->GetBoolField(TEXT("cdo_exists")) && B->GetBoolField(TEXT("cdo_exists")))
        {
            if (!bAllowLifecycleCDO) { return false; } Expected->SetBoolField(TEXT("cdo_exists"), true); Expected->SetObjectField(TEXT("existing_cdo"), B->GetObjectField(TEXT("existing_cdo")));
            Expected->SetObjectField(TEXT("existing_cdo_properties"), B->GetObjectField(TEXT("existing_cdo_properties"))); Created.Add(MakeShared<FJsonValueString>(FString(*Pair.Key)));
        }
        if (Text(Expected) != Text(B.ToSharedRef())) { return false; }
    }
    return true;
}
bool Disk(const FString& PackageName, TSharedRef<FJsonObject>& J)
{
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    J->SetStringField(TEXT("path"), Filename); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size));
    if (Size < 0) { J->SetStringField(TEXT("sha1"), TEXT("")); return true; }
    TArray<uint8> Bytes; if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; }
    uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& BBPackage, const FString& BTPackage, TSharedRef<FJsonObject>& J)
{
    int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == BBPackage || It->GetName() == BTPackage) { continue; }
        if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetPathName(), It->IsDirty()); } return true;
}
bool Name(const FString& S)
{ FText Reason; return !S.IsEmpty() && S.Len() < NAME_SIZE && !FName(*S).IsNone() && FName::IsValidXName(S, INVALID_OBJECTNAME_CHARACTERS, &Reason) && !S.Contains(TEXT(" ")); }
bool Read(const FString& Path, bool bTree, TSharedRef<FJsonObject>& O, FResult& R, const TCHAR* Label)
{
    const FString Encoded = bTree ? BehaviorTree::InspectAsset(Path) : Blackboard::InspectAsset(Path); TSharedPtr<FJsonObject> Raw;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Encoded), Raw) || !Raw.IsValid()) { return false; }
    R.Data->SetObjectField(Label, Raw.ToSharedRef()); bool OK = false, Known = false, Attempted = true;
    if (!Raw->TryGetBoolField(TEXT("success"), OK) || !Raw->TryGetBoolField(TEXT("change_state_known"), Known) || !Raw->TryGetBoolField(TEXT("mutation_attempted"), Attempted) || !OK || !Known || Attempted) { return false; }
    const auto Data = Raw->GetObjectField(TEXT("data")); O = Data->GetObjectField(TEXT("after")).ToSharedRef(); return true;
}
void Refresh(UBlackboardData* B, FResult& R)
{ B->UpdateParentKeys(); B->UpdateKeyIDs(); B->UpdateIfHasSynchronizedKeys(); R.Data->SetNumberField(TEXT("blackboard_cache_refresh_sequences"), R.Data->GetNumberField(TEXT("blackboard_cache_refresh_sequences")) + 1); }
FStructProperty* SelectorProperty(UClass* C)
{
    auto* P = FindFProperty<FStructProperty>(C, TEXT("BlackboardKey")); auto* S = FindObject<UScriptStruct>(nullptr, TEXT("/Script/AIModule.BlackboardKeySelector"));
    return P && P->ArrayDim == 1 && Loaded(S) && P->Struct == S && S->GetStructureSize() == sizeof(FBlackboardKeySelector) && P->HasAnyPropertyFlags(CPF_Edit) ? P : nullptr;
}
bool SetSelector(UBTNode* N, UBlackboardData* B, const FName Key)
{
    auto* P = SelectorProperty(N->GetClass()); if (!P) { return false; } auto* Value = P->ContainerPtrToValuePtr<FBlackboardKeySelector>(N);
    Value->SelectedKeyName = Key; Value->ResolveSelectedKey(*B); return Value->GetSelectedKeyID() != FBlackboard::InvalidKey;
}
FStructProperty* PrecisionProperty()
{
    auto* P = FindFProperty<FStructProperty>(UBTTask_RotateToFaceBBEntry::StaticClass(), TEXT("Precision"));
    auto* S = FindObject<UScriptStruct>(nullptr, TEXT("/Script/AIModule.ValueOrBBKey_Float"));
    return P && P->ArrayDim == 1 && Loaded(S) && P->Struct == S && S->GetStructureSize() == sizeof(FValueOrBBKey_Float)
        && P->HasAnyPropertyFlags(CPF_Edit) && !P->HasAnyPropertyFlags(CPF_EditConst) ? P : nullptr;
}
bool SetPrecision(UBTTask_RotateToFaceBBEntry* N, float Value)
{
    auto* P = PrecisionProperty(); if (!P || !Loaded(N) || N->GetClass() != UBTTask_RotateToFaceBBEntry::StaticClass()) { return false; }
    // Initialize only this new owned task's verified editable struct through its public reflected property.
    *P->ContainerPtrToValuePtr<FValueOrBBKey_Float>(N) = FValueOrBBKey_Float(Value); return true;
}
bool PrecisionEquals(const UBTTask_RotateToFaceBBEntry* N, float Value)
{
    auto* P = PrecisionProperty(); return P && Loaded(N) && N->GetClass() == UBTTask_RotateToFaceBBEntry::StaticClass()
        && P->ContainerPtrToValuePtr<FValueOrBBKey_Float>(N)->ToString() == FValueOrBBKey_Float(Value).ToString();
}
FProperty* ByteProperty(UClass* C, const FName Name)
{
    FProperty* P = FindFProperty<FProperty>(C, Name); if (!P || P->ArrayDim != 1) { return nullptr; }
    if (CastField<FByteProperty>(P)) { return P; } auto* Enum = CastField<FEnumProperty>(P);
    return Enum && Enum->GetUnderlyingProperty() && Enum->GetUnderlyingProperty()->GetSize() == sizeof(uint8) ? P : nullptr;
}
bool SetByte(UObject* O, const FName Name, uint8 Value)
{
    FProperty* P = ByteProperty(O->GetClass(), Name); if (!P || !P->HasAnyPropertyFlags(CPF_Edit)) { return false; }
    if (auto* Byte = CastField<FByteProperty>(P)) { Byte->SetPropertyValue_InContainer(O, Value); return true; }
    auto* Enum = CastField<FEnumProperty>(P); Enum->GetUnderlyingProperty()->SetIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(O), static_cast<uint64>(Value)); return true;
}
bool ByteEquals(UObject* O, const FName Name, uint8 Expected)
{
    FProperty* P = ByteProperty(O->GetClass(), Name); if (!P) { return false; }
    if (auto* Byte = CastField<FByteProperty>(P)) { return Byte->GetPropertyValue_InContainer(O) == Expected; }
    auto* Enum = CastField<FEnumProperty>(P); return Enum->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(O)) == Expected;
}
template<typename T> T* Node(UBehaviorTree* B, const FString& Label, FResult& R)
{
    T* N = NewObject<T>(B, NAME_None, RF_Transactional); if (N) { N->NodeName = Label; R.Data->SetNumberField(TEXT("runtime_node_allocation_count"), R.Data->GetNumberField(TEXT("runtime_node_allocation_count")) + 1); } return N;
}
bool Move(UBehaviorTree* B, UBTCompositeNode* Parent, FName Key, const TCHAR* Label, float Radius, FResult& R)
{
    auto* T = Node<UBTTask_MoveDirectlyToward>(B, Label, R); if (!Loaded(T)) { return false; }
    T->bUsePathfinding = false; T->bAllowPartialPath = FValueOrBBKey_Bool(true); T->bRequireNavigableEndLocation = FValueOrBBKey_Bool(false);
    T->bProjectGoalLocation = FValueOrBBKey_Bool(false); T->bTrackMovingGoal = FValueOrBBKey_Bool(true); T->AcceptableRadius = FValueOrBBKey_Float(Radius);
    if (!SetSelector(T, B->BlackboardAsset, Key)) { return false; } static_cast<UBTNode*>(T)->InitializeFromAsset(*B); Parent->Children.Emplace_GetRef().ChildTask = T; return true;
}
bool Build(UBehaviorTree* B, const FString& Shape, FName Target, FName Patrol, float Radius, float Wait, float Precision, FResult& R)
{
    UBTCompositeNode* Root = Shape == TEXT("sequence_patrol_chase") ? static_cast<UBTCompositeNode*>(Node<UBTComposite_Sequence>(B, TEXT("PatrolThenChase"), R))
        : static_cast<UBTCompositeNode*>(Node<UBTComposite_Selector>(B, Shape == TEXT("selector_alert_chase_patrol") ? TEXT("AlertChaseOrPatrol") : TEXT("ChaseOrPatrol"), R));
    if (!Loaded(Root)) { return false; } B->RootNode = Root; Root->InitializeFromAsset(*B);
    if (Shape == TEXT("sequence_patrol_chase")) { return Move(B, Root, Patrol, TEXT("MoveToPatrolLocation"), Radius, R) && Move(B, Root, Target, TEXT("MoveToTargetActor"), Radius, R); }
    if (Shape == TEXT("selector_alert_chase_patrol"))
    {
        auto* Alert = Node<UBTComposite_Sequence>(B, TEXT("FaceThenChaseTarget"), R); if (!Loaded(Alert)) { return false; } Alert->InitializeFromAsset(*B); Root->Children.Emplace_GetRef().ChildComposite = Alert;
        auto* Face = Node<UBTTask_RotateToFaceBBEntry>(B, FString::Printf(TEXT("Face%s"), *Target.ToString()), R); if (!Loaded(Face)) { return false; }
        if (!SetPrecision(Face, Precision) || !SetSelector(Face, B->BlackboardAsset, Target)) { return false; } static_cast<UBTNode*>(Face)->InitializeFromAsset(*B); Alert->Children.Emplace_GetRef().ChildTask = Face;
        auto* Delay = Node<UBTTask_Wait>(B, TEXT("AlertReactionDelay"), R); if (!Loaded(Delay)) { return false; }
        Delay->WaitTime = FValueOrBBKey_Float(Wait); Delay->RandomDeviation = FValueOrBBKey_Float(0); static_cast<UBTNode*>(Delay)->InitializeFromAsset(*B); Alert->Children.Emplace_GetRef().ChildTask = Delay;
        if (!Move(B, Alert, Target, TEXT("ChaseTargetActor"), Radius, R)) { return false; }
    }
    else if (!Move(B, Root, Target, TEXT("ChaseTargetActor"), Radius, R)) { return false; }
    if (!Move(B, Root, Patrol, TEXT("MoveToPatrolLocation"), Radius, R)) { return false; }
    auto* Decorator = Node<UBTDecorator_Blackboard>(B, Target.ToString() + TEXT(" Is Set"), R); if (!Loaded(Decorator) || !SetSelector(Decorator, B->BlackboardAsset, Target)
        || !SetByte(Decorator, TEXT("BasicOperation"), static_cast<uint8>(EBasicKeyOperation::Set))
        || !SetByte(Decorator, TEXT("NotifyObserver"), static_cast<uint8>(EBTBlackboardRestart::ValueChange))
        || !SetByte(Decorator, TEXT("FlowAbortMode"), static_cast<uint8>(EBTFlowAbortMode::Both))) { return false; }
    static_cast<UBTNode*>(Decorator)->InitializeFromAsset(*B);
    FProperty* BasicProperty = ByteProperty(Decorator->GetClass(), TEXT("BasicOperation"));
    if (!BasicProperty || !BasicProperty->HasAnyPropertyFlags(CPF_Edit)) { return false; }
    FPropertyChangedEvent Changed(BasicProperty, EPropertyChangeType::ValueSet);
    R.Data->SetBoolField(TEXT("implicit_property_callback_effects_possible"), true);
    R.Data->SetNumberField(TEXT("editable_property_notification_count"), R.Data->GetNumberField(TEXT("editable_property_notification_count")) + 1);
    // Public base virtual notification computes protected OperationType in the engine; no direct cache write.
    static_cast<UObject*>(Decorator)->PostEditChangeProperty(Changed);
    Root->Children[0].Decorators.Add(Decorator); Root->Children[0].DecoratorOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test, 0)); return true;
}
bool AppendKey(UBlackboardData* B, const FName Key, UClass* C, FResult& R)
{
    FBlackboardEntry E; E.EntryName = Key; E.KeyType = NewObject<UBlackboardKeyType>(B, C, NAME_None, RF_Transactional);
    if (E.KeyType) { R.Data->SetNumberField(TEXT("key_allocation_count"), R.Data->GetNumberField(TEXT("key_allocation_count")) + 1); }
    if (!Loaded(E.KeyType)) { return false; }
    if (auto* Object = Cast<UBlackboardKeyType_Object>(E.KeyType)) { Object->BaseClass = AActor::StaticClass(); }
    B->Keys.Add(E); return true;
}
bool SingleIsSetLogic(const FBTCompositeChild& Child, bool bCanonical)
{
    if (Child.Decorators.Num() == 0) { return Child.DecoratorOps.Num() == 0; }
    if (Child.Decorators.Num() != 1 || !Loaded(Child.Decorators[0]) || Child.Decorators[0]->GetClass() != UBTDecorator_Blackboard::StaticClass()) { return false; }
    if (bCanonical) { return Child.DecoratorOps.Num() == 0; }
    return Child.DecoratorOps.Num() == 1 && Child.DecoratorOps[0].Operation == EBTDecoratorLogic::Test && Child.DecoratorOps[0].Number == 0;
}
bool Semantic(UBehaviorTree* B, UBlackboardData* BB, const FString& Shape, FName Target, FName Patrol, float Radius, float Wait, float Precision, bool bCanonical = false)
{
    if (!Loaded(B) || !Loaded(BB) || B->BlackboardAsset != BB || BB->Parent || (BB->Keys.Num() != 4 && BB->Keys.Num() != 5)
        || !Loaded(B->RootNode) || B->RootNode->GetOuter() != B || B->RootNode->Children.Num() != 2 || B->RootNode->Services.Num() || B->RootDecorators.Num() || B->RootDecoratorOps.Num()) { return false; }
    const FName Names[] = { Target, Patrol, TEXT("LastKnownTargetLocation"), TEXT("AIState") };
    UClass* Types[] = { UBlackboardKeyType_Object::StaticClass(), UBlackboardKeyType_Vector::StaticClass(), UBlackboardKeyType_Vector::StaticClass(), UBlackboardKeyType_Name::StaticClass() };
    for (int32 I = 0; I < 4; ++I) { const auto& E = BB->Keys[I]; if (E.EntryName != Names[I] || !Loaded(E.KeyType) || E.KeyType->GetClass() != Types[I] || E.KeyType->GetOuter() != BB || E.bInstanceSynced || BB->GetKeyID(Names[I]) != FBlackboard::FKey(I)) { return false; } }
    auto* Object = Cast<UBlackboardKeyType_Object>(BB->Keys[0].KeyType); auto* FirstVector = Cast<UBlackboardKeyType_Vector>(BB->Keys[1].KeyType); auto* SecondVector = Cast<UBlackboardKeyType_Vector>(BB->Keys[2].KeyType); auto* NameKey = Cast<UBlackboardKeyType_Name>(BB->Keys[3].KeyType);
    if (Object->BaseClass != AActor::StaticClass() || Object->DefaultValue || FirstVector->bUseDefaultValue || SecondVector->bUseDefaultValue || FirstVector->DefaultValue != FVector::ZeroVector || SecondVector->DefaultValue != FVector::ZeroVector || !NameKey->DefaultValue.IsNone()) { return false; }
    if (BB->Keys.Num() == 5) { const auto& E = BB->Keys[4]; auto* Self = Cast<UBlackboardKeyType_Object>(E.KeyType); if (E.EntryName != FBlackboard::KeySelf || !Loaded(Self) || Self->GetClass() != UBlackboardKeyType_Object::StaticClass() || Self->GetOuter() != BB || Self->BaseClass != AActor::StaticClass() || Self->DefaultValue || E.bInstanceSynced) { return false; } }
    const auto Key = [&](UBTNode* N, const FName Expected) { auto* P = SelectorProperty(N->GetClass()); if (!P) { return false; } const auto* Value = P->ContainerPtrToValuePtr<FBlackboardKeySelector>(N); return Value->SelectedKeyName == Expected && Value->GetSelectedKeyID() == BB->GetKeyID(Expected) && Value->SelectedKeyType == BB->GetKeyType(Value->GetSelectedKeyID()); };
    const auto CheckMove = [&](UBTTaskNode* N, FName Expected, const TCHAR* Label)
    { auto* T = Cast<UBTTask_MoveDirectlyToward>(N); return Loaded(T) && T->GetClass() == UBTTask_MoveDirectlyToward::StaticClass() && T->GetOuter() == B && T->NodeName == Label && T->Services.Num() == 0 && !T->bUsePathfinding && Key(T, Expected)
        && T->AcceptableRadius.ToString() == FValueOrBBKey_Float(Radius).ToString() && T->bAllowPartialPath.ToString() == FValueOrBBKey_Bool(true).ToString()
        && T->bRequireNavigableEndLocation.ToString() == FValueOrBBKey_Bool(false).ToString() && T->bProjectGoalLocation.ToString() == FValueOrBBKey_Bool(false).ToString() && T->bTrackMovingGoal.ToString() == FValueOrBBKey_Bool(true).ToString(); };
    const auto Plain = [](const FBTCompositeChild& C) { return C.Decorators.Num() == 0 && C.DecoratorOps.Num() == 0; };
    auto* Root = B->RootNode.Get(); if (Shape == TEXT("sequence_patrol_chase")) { return Root->GetClass() == UBTComposite_Sequence::StaticClass() && Root->NodeName == TEXT("PatrolThenChase") && !Root->Children[0].ChildComposite && !Root->Children[1].ChildComposite && Plain(Root->Children[0]) && Plain(Root->Children[1]) && CheckMove(Root->Children[0].ChildTask, Patrol, TEXT("MoveToPatrolLocation")) && CheckMove(Root->Children[1].ChildTask, Target, TEXT("MoveToTargetActor")); }
    if (Root->GetClass() != UBTComposite_Selector::StaticClass() || Root->NodeName != (Shape == TEXT("selector_alert_chase_patrol") ? TEXT("AlertChaseOrPatrol") : TEXT("ChaseOrPatrol"))
        || Root->Children[1].ChildComposite || !Plain(Root->Children[1]) || !CheckMove(Root->Children[1].ChildTask, Patrol, TEXT("MoveToPatrolLocation")) || Root->Children[0].Decorators.Num() != 1 || !SingleIsSetLogic(Root->Children[0], bCanonical)) { return false; }
    auto* D = Cast<UBTDecorator_Blackboard>(Root->Children[0].Decorators[0]); if (!Loaded(D) || D->GetClass() != UBTDecorator_Blackboard::StaticClass() || D->GetOuter() != B || D->NodeName != Target.ToString() + TEXT(" Is Set") || !Key(D, Target) || D->GetFlowAbortMode() != EBTFlowAbortMode::Both
        || !ByteEquals(D, TEXT("OperationType"), static_cast<uint8>(EBasicKeyOperation::Set)) || !ByteEquals(D, TEXT("BasicOperation"), static_cast<uint8>(EBasicKeyOperation::Set)) || !ByteEquals(D, TEXT("NotifyObserver"), static_cast<uint8>(EBTBlackboardRestart::ValueChange))) { return false; }
    if (Shape == TEXT("selector_chase_patrol")) { return !Root->Children[0].ChildComposite && CheckMove(Root->Children[0].ChildTask, Target, TEXT("ChaseTargetActor")); }
    auto* Alert = Root->Children[0].ChildComposite.Get(); if (Root->Children[0].ChildTask || !Loaded(Alert) || Alert->GetClass() != UBTComposite_Sequence::StaticClass() || Alert->GetOuter() != B || Alert->NodeName != TEXT("FaceThenChaseTarget") || Alert->Services.Num() || Alert->Children.Num() != 3) { return false; }
    for (const auto& C : Alert->Children) { if (C.ChildComposite || !Plain(C)) { return false; } }
    auto* Face = Cast<UBTTask_RotateToFaceBBEntry>(Alert->Children[0].ChildTask); auto* Delay = Cast<UBTTask_Wait>(Alert->Children[1].ChildTask);
    return Loaded(Face) && Face->GetClass() == UBTTask_RotateToFaceBBEntry::StaticClass() && Face->GetOuter() == B && Face->NodeName == FString::Printf(TEXT("Face%s"), *Target.ToString()) && Face->Services.Num() == 0 && Key(Face, Target) && PrecisionEquals(Face, Precision)
        && Loaded(Delay) && Delay->GetClass() == UBTTask_Wait::StaticClass() && Delay->GetOuter() == B && Delay->NodeName == TEXT("AlertReactionDelay") && Delay->Services.Num() == 0 && Delay->WaitTime.ToString() == FValueOrBBKey_Float(Wait).ToString() && Delay->RandomDeviation.ToString() == FValueOrBBKey_Float(0).ToString() && CheckMove(Alert->Children[2].ChildTask, Target, TEXT("ChaseTargetActor"));
}
// AIGraphNode::AddSubNode invokes UpdateAsset during SpawnMissingNodes. Validate a locked,
// completed native graph without invoking the shared observer (which correctly rejects locked graphs).
bool RawConstruction(UBehaviorTree* B, UBehaviorTreeGraph* G, UBTCompositeNode* RetainedRoot, bool bLocked, TSharedRef<FJsonObject>& J)
{
    J->SetObjectField(TEXT("graph"), Identity(G)); J->SetStringField(TEXT("retained_runtime_root"), RetainedRoot ? RetainedRoot->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("actual_runtime_root"), B && B->RootNode ? B->RootNode->GetPathName() : TEXT(""));
    TArray<TSharedPtr<FJsonValue>> NodeRows, RuntimeRows; J->SetArrayField(TEXT("nodes"), NodeRows); J->SetArrayField(TEXT("runtime_nodes"), RuntimeRows);
    const auto Refuse = [&](const TCHAR* Stage, const FString& Object)
    { J->SetStringField(TEXT("failed_stage"), Stage); J->SetStringField(TEXT("failed_object"), Object); J->SetNumberField(TEXT("observed_editor_row_count"), NodeRows.Num()); J->SetNumberField(TEXT("observed_runtime_row_count"), RuntimeRows.Num()); J->SetBoolField(TEXT("complete"), false); return false; };
    if (!Loaded(B) || !Loaded(G) || G->GetOuter() != B || G->GetClass() != UBehaviorTreeGraph::StaticClass() || G->Schema != UEdGraphSchema_BehaviorTree::StaticClass()
        || G->RootNodeClass != UBehaviorTreeGraphNode_Root::StaticClass() || G->IsLocked() != bLocked || G->SubGraphs.Num() || !Loaded(RetainedRoot) || B->RootNode != RetainedRoot || G->Nodes.Num() < 1 || G->Nodes.Num() > 8)
    { return Refuse(TEXT("graph_or_retained_root"), G ? G->GetPathName() : TEXT("")); }
    J->SetBoolField(TEXT("locked"), G->IsLocked()); J->SetNumberField(TEXT("mod_counter"), G->ModCounter); J->SetNumberField(TEXT("top_level_node_count"), G->Nodes.Num());
    TSet<UBTNode*> Runtime; TMap<UBTNode*, TArray<UBTDecorator*>> ExpectedDecorators;
    TFunction<bool(UBTNode*, int32)> Visit = [&](UBTNode* N, int32 Depth)
    {
        auto Row = Identity(N); RuntimeRows.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("runtime_nodes"), RuntimeRows);
        if (!Loaded(N) || N->GetOuter() != B || N->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || Runtime.Contains(N) || Runtime.Num() >= 7 || Depth > 4)
        { return Refuse(TEXT("runtime_identity_or_bound"), N ? N->GetPathName() : TEXT("")); }
        Runtime.Add(N); Row->SetStringField(TEXT("node_name"), N->NodeName); auto Props = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("properties"), Props);
        if (!Properties(N, Props)) { return Refuse(TEXT("runtime_properties"), N->GetPathName()); }
        if (auto* C = Cast<UBTCompositeNode>(N))
        {
            if ((C->GetClass() != UBTComposite_Sequence::StaticClass() && C->GetClass() != UBTComposite_Selector::StaticClass()) || C->Services.Num() || C->Children.Num() > 3)
            { return Refuse(TEXT("runtime_composite"), C->GetPathName()); }
            TArray<TSharedPtr<FJsonValue>> ChildRows; Row->SetArrayField(TEXT("children"), ChildRows); int32 ChildIndex = 0;
            for (const auto& Child : C->Children)
            {
                auto CR = MakeShared<FJsonObject>(); CR->SetNumberField(TEXT("index"), ChildIndex++); CR->SetStringField(TEXT("child_composite"), Child.ChildComposite ? Child.ChildComposite->GetPathName() : TEXT("")); CR->SetStringField(TEXT("child_task"), Child.ChildTask ? Child.ChildTask->GetPathName() : TEXT(""));
                CR->SetNumberField(TEXT("decorator_count"), Child.Decorators.Num()); CR->SetNumberField(TEXT("decorator_op_count"), Child.DecoratorOps.Num());
                ChildRows.Add(MakeShared<FJsonValueObject>(CR)); Row->SetArrayField(TEXT("children"), ChildRows);
                if (Child.Decorators.Num() > 1 || Child.DecoratorOps.Num() > 1) { return Refuse(TEXT("runtime_child_logic_bound"), C->GetPathName()); }
                TArray<TSharedPtr<FJsonValue>> DPaths, Ops;
                for (auto D : Child.Decorators) { DPaths.Add(MakeShared<FJsonValueString>(D ? D->GetPathName() : TEXT(""))); }
                for (const auto& Op : Child.DecoratorOps) { auto ORow = MakeShared<FJsonObject>(); ORow->SetNumberField(TEXT("operation"), static_cast<int32>(Op.Operation)); ORow->SetNumberField(TEXT("number"), Op.Number); Ops.Add(MakeShared<FJsonValueObject>(ORow)); }
                CR->SetArrayField(TEXT("decorators"), DPaths); CR->SetArrayField(TEXT("decorator_ops"), Ops);
                if ((!Child.ChildComposite && !Child.ChildTask) || (Child.ChildComposite && Child.ChildTask) || Child.Decorators.Num() > 1 || !SingleIsSetLogic(Child, !bLocked))
                { return Refuse(TEXT("runtime_child"), C->GetPathName()); }
                UBTNode* Main = Child.ChildComposite ? static_cast<UBTNode*>(Child.ChildComposite.Get()) : static_cast<UBTNode*>(Child.ChildTask.Get());
                if (!Visit(Main, Depth + 1)) { return false; }
                auto& Decorators = ExpectedDecorators.FindOrAdd(Main);
                for (auto D : Child.Decorators) { if (!Visit(D, Depth + 1)) { return false; } Decorators.Add(D); }
            }
        }
        else if (auto* T = Cast<UBTTaskNode>(N))
        {
            if (T->Services.Num() || (T->GetClass() != UBTTask_MoveDirectlyToward::StaticClass() && T->GetClass() != UBTTask_RotateToFaceBBEntry::StaticClass() && T->GetClass() != UBTTask_Wait::StaticClass()))
            { return Refuse(TEXT("runtime_task"), T->GetPathName()); }
        }
        else if (N->GetClass() != UBTDecorator_Blackboard::StaticClass()) { return Refuse(TEXT("runtime_decorator"), N->GetPathName()); }
        return true;
    };
    if (B->RootDecorators.Num() || B->RootDecoratorOps.Num()) { return Refuse(TEXT("runtime_root_decorators"), RetainedRoot->GetPathName()); }
    if (!Visit(RetainedRoot, 0)) { return false; }
    TArray<UBehaviorTreeGraphNode*> Nodes; TSet<UBehaviorTreeGraphNode*> Seen; TSet<FGuid> Guids; TSet<const UEdGraphPin*> Pins;
    TMap<const UEdGraphPin*, TSharedPtr<FJsonObject>> PinObservations;
    TMap<UBTNode*, UBehaviorTreeGraphNode*> Instances; TMap<UBehaviorTreeGraphNode*, UEdGraphPin*> Inputs, Outputs; UBehaviorTreeGraphNode_Root* FactoryRoot = nullptr;
    for (UEdGraphNode* N : G->Nodes) { auto* Member = Cast<UBehaviorTreeGraphNode>(N); if (!Member || Seen.Contains(Member)) { return Refuse(TEXT("top_level_membership"), N ? N->GetPathName() : TEXT("")); } Seen.Add(Member); Nodes.Add(Member); }
    for (int32 I = 0; I < Nodes.Num(); ++I)
    {
        auto* N = Nodes[I]; auto Row = Identity(N); NodeRows.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("nodes"), NodeRows);
        if (!Loaded(N) || N->GetOuter() != G || N->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || !N->NodeGuid.IsValid() || Guids.Contains(N->NodeGuid))
        { return Refuse(TEXT("editor_identity_or_guid"), N->GetPathName()); }
        Guids.Add(N->NodeGuid); Row->SetStringField(TEXT("node_guid"), N->NodeGuid.ToString()); Row->SetNumberField(TEXT("x"), N->NodePosX); Row->SetNumberField(TEXT("y"), N->NodePosY);
        Row->SetStringField(TEXT("node_instance"), N->NodeInstance ? N->NodeInstance->GetPathName() : TEXT(""));
        if (N->GetClass() == UBehaviorTreeGraphNode_Root::StaticClass())
        {
            if (FactoryRoot || N->NodeInstance || N->Decorators.Num() || N->SubNodes.Num() || N->Services.Num()) { return Refuse(TEXT("factory_root"), N->GetPathName()); }
            FactoryRoot = CastChecked<UBehaviorTreeGraphNode_Root>(N); if (FactoryRoot->BlackboardAsset != B->BlackboardAsset) { return Refuse(TEXT("factory_blackboard"), N->GetPathName()); }
        }
        else
        {
            auto* Instance = Cast<UBTNode>(N->NodeInstance); if (!Instance || !Runtime.Contains(Instance) || Instances.Contains(Instance)) { return Refuse(TEXT("editor_runtime_membership"), N->GetPathName()); }
            const bool Composite = Cast<UBTCompositeNode>(Instance) != nullptr, Decorator = Cast<UBTDecorator>(Instance) != nullptr;
            if (N->GetClass() != (Composite ? UBehaviorTreeGraphNode_Composite::StaticClass() : Decorator ? UBehaviorTreeGraphNode_Decorator::StaticClass() : UBehaviorTreeGraphNode_Task::StaticClass()))
            { return Refuse(TEXT("editor_runtime_class"), N->GetPathName()); }
            Instances.Add(Instance, N);
        }
        if (N->Services.Num() || N->SubNodes.Num() != N->Decorators.Num() || N->Decorators.Num() > 1) { return Refuse(TEXT("subnode_arrays"), N->GetPathName()); }
        TArray<TSharedPtr<FJsonValue>> Decorators;
        for (int32 D = 0; D < N->Decorators.Num(); ++D)
        {
            auto* Sub = N->Decorators[D].Get(); if (!Sub || N->SubNodes[D].Get() != Sub || Sub->ParentNode.Get() != N || Seen.Contains(Sub) || Seen.Num() >= 8)
            { return Refuse(TEXT("subnode_parent_or_membership"), N->GetPathName()); }
            Seen.Add(Sub); Nodes.Add(Sub); Decorators.Add(MakeShared<FJsonValueString>(Sub->GetPathName()));
        }
        Row->SetArrayField(TEXT("decorators"), Decorators); TArray<TSharedPtr<FJsonValue>> PinRows; TSet<FGuid> PinIds; Row->SetArrayField(TEXT("pins"), PinRows);
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P || P->GetOwningNodeUnchecked() != N || !P->PinId.IsValid() || PinIds.Contains(P->PinId) || P->bOrphanedPin || Pins.Contains(P) || Pins.Num() >= 16 || P->LinkedTo.Num() > 3)
            { return Refuse(TEXT("pin_identity_or_bound"), N->GetPathName()); }
            Pins.Add(P); PinIds.Add(P->PinId); auto PR = MakeShared<FJsonObject>(); PR->SetStringField(TEXT("id"), P->PinId.ToString()); PR->SetStringField(TEXT("name"), P->PinName.ToString()); PR->SetNumberField(TEXT("direction"), P->Direction);
            PR->SetStringField(TEXT("category"), P->PinType.PinCategory.ToString()); PR->SetNumberField(TEXT("link_count"), P->LinkedTo.Num()); PR->SetArrayField(TEXT("links"), {});
            PinObservations.Add(P, PR); PinRows.Add(MakeShared<FJsonValueObject>(PR)); Row->SetArrayField(TEXT("pins"), PinRows);
            auto& Map = P->Direction == EGPD_Input ? Inputs : Outputs;
            if ((P->Direction != EGPD_Input && P->Direction != EGPD_Output) || Map.Contains(N)) { return Refuse(TEXT("pin_direction_or_duplicate"), N->GetPathName()); } Map.Add(N, P);
        }
    }
    // Capture peer identities only after every admissible owned pin has been collected.
    // A foreign, stale or nonreciprocal peer leaves the already-published bounded rows intact.
    for (const UEdGraphPin* P : Pins)
    {
        TArray<TSharedPtr<FJsonValue>> Links; TSet<const UEdGraphPin*> Unique;
        for (const UEdGraphPin* Other : P->LinkedTo)
        {
            if (!Other || !Pins.Contains(Other) || Other == P || Unique.Contains(Other) || Other->Direction == P->Direction || !Other->LinkedTo.Contains(P))
            { return Refuse(TEXT("pin_peer_membership_or_reciprocity"), P->GetOwningNodeUnchecked()->GetPathName()); }
            Unique.Add(Other); Links.Add(MakeShared<FJsonValueString>(Other->GetOwningNodeUnchecked()->GetPathName() + TEXT(":") + Other->PinId.ToString()));
        }
        PinObservations.FindChecked(P)->SetArrayField(TEXT("links"), Links);
    }
    if (!FactoryRoot || Instances.Num() != Runtime.Num() || Nodes.Num() != Runtime.Num() + 1 || !Outputs.Contains(FactoryRoot) || Inputs.Contains(FactoryRoot) || FactoryRoot->Pins.Num() != 1)
    { return Refuse(TEXT("complete_editor_runtime_mapping"), G->GetPathName()); }
    int32 Connections = 0;
    const auto Link = [&](UEdGraphPin* Out, UEdGraphPin* In)
    { return Out && In && Pins.Contains(Out) && Pins.Contains(In) && Out->Direction == EGPD_Output && In->Direction == EGPD_Input && Out->LinkedTo.Contains(In) && In->LinkedTo.Num() == 1 && In->LinkedTo[0] == Out; };
    auto* RootGraph = Instances.FindRef(RetainedRoot); auto* RootOut = Outputs.FindRef(FactoryRoot);
    if (!RootGraph || RootOut->LinkedTo.Num() != 1 || !Link(RootOut, Inputs.FindRef(RootGraph))) { return Refuse(TEXT("completed_factory_root_link"), FactoryRoot->GetPathName()); } ++Connections;
    for (const auto& Pair : Instances)
    {
        auto* Instance = Pair.Key; auto* N = Pair.Value; const auto* Decorators = ExpectedDecorators.Find(Instance); const int32 DecoratorCount = Decorators ? Decorators->Num() : 0;
        if (N->Decorators.Num() != DecoratorCount) { return Refuse(TEXT("expected_decorator_count"), N->GetPathName()); }
        for (int32 D = 0; D < DecoratorCount; ++D) { if (N->Decorators[D].Get() != Instances.FindRef((*Decorators)[D])) { return Refuse(TEXT("expected_decorator_identity"), N->GetPathName()); } }
        if (auto* C = Cast<UBTCompositeNode>(Instance))
        {
            auto* Out = Outputs.FindRef(N); if (N->Pins.Num() != 2 || !Inputs.Contains(N) || !Out || Out->LinkedTo.Num() != C->Children.Num()) { return Refuse(TEXT("composite_pin_shape"), N->GetPathName()); }
            for (int32 I = 0; I < C->Children.Num(); ++I)
            {
                const auto& Child = C->Children[I]; UBTNode* Main = Child.ChildComposite ? static_cast<UBTNode*>(Child.ChildComposite.Get()) : static_cast<UBTNode*>(Child.ChildTask.Get());
                auto* ChildGraph = Instances.FindRef(Main); auto* In = Inputs.FindRef(ChildGraph);
                if (!ChildGraph || Out->LinkedTo[I] != In || !Link(Out, In)) { return Refuse(TEXT("ordered_child_link"), N->GetPathName()); } ++Connections;
            }
        }
        else if (Cast<UBTDecorator>(Instance)) { if (N->Pins.Num() != 0) { return Refuse(TEXT("decorator_pin_shape"), N->GetPathName()); } }
        else if (N->Pins.Num() != 1 || !Inputs.Contains(N) || Outputs.Contains(N)) { return Refuse(TEXT("task_pin_shape"), N->GetPathName()); }
    }
    auto Topology = MakeShared<FJsonObject>(); Topology->SetArrayField(TEXT("nodes"), NodeRows); Topology->SetStringField(TEXT("runtime_root"), RetainedRoot->GetPathName());
    J->SetObjectField(TEXT("topology"), Topology); J->SetNumberField(TEXT("runtime_object_count"), Runtime.Num()); J->SetNumberField(TEXT("all_node_count"), Nodes.Num()); J->SetNumberField(TEXT("physical_connection_count"), Connections);
    J->SetBoolField(TEXT("complete"), true); return true;
}
void OwnedPaths(const TSharedPtr<FJsonValue>& V, const FString& AssetPath, TSet<FString>& Out)
{
    if (!V.IsValid()) { return; } if (V->Type == EJson::Array) { for (const auto& Item : V->AsArray()) { OwnedPaths(Item, AssetPath, Out); } }
    else if (V->Type == EJson::Object)
    { const auto O = V->AsObject(); FString Path, Class, Outer; if (O->TryGetStringField(TEXT("path"), Path) && O->TryGetStringField(TEXT("class"), Class) && O->TryGetStringField(TEXT("outer"), Outer) && O->HasField(TEXT("object_flags")) && (Path == AssetPath || Path.StartsWith(AssetPath + TEXT(":")))) { Out.Add(Path); }
        for (const auto& Pair : O->Values) { OwnedPaths(Pair.Value, AssetPath, Out); } }
}
bool SavedEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& Path, const TSet<FString>& Owned, bool bTree, int32& FlagTransitions)
{
    if (!A.IsValid() || !B.IsValid() || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object)
    {
        const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num()) { return false; }
        for (const auto& Pair : X->Values)
        {
            const auto V = Y->TryGetField(Pair.Key.ToView()); if (!V.IsValid()) { return false; } const FString Field = FString(*Pair.Key), P = Path + TEXT("/") + Field;
            if (Field == TEXT("object_flags") && Pair.Value->Type == EJson::String && V->Type == EJson::String)
            {
                FString ObjectPath; const bool bOwnedIdentity = X->TryGetStringField(TEXT("path"), ObjectPath) && Owned.Contains(ObjectPath);
                if (!bOwnedIdentity && Pair.Value->AsString() != V->AsString()) { return false; }
                if (bOwnedIdentity)
                {
                    FString Class, Outer; if (!X->TryGetStringField(TEXT("class"), Class) || !X->TryGetStringField(TEXT("outer"), Outer)
                        || Y->GetStringField(TEXT("path")) != ObjectPath || Y->GetStringField(TEXT("class")) != Class || Y->GetStringField(TEXT("outer")) != Outer) { return false; }
                    const FString OldText = Pair.Value->AsString(), NewText = V->AsString(); const uint64 Old = FCString::Strtoui64(*OldText, nullptr, 10), Now = FCString::Strtoui64(*NewText, nullptr, 10);
                    if (OldText != FString::Printf(TEXT("%llu"), Old) || NewText != FString::Printf(TEXT("%llu"), Now) || Now != (Old | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; }
                    if (Old != Now) { ++FlagTransitions; } continue;
                }
            }
            if (!SavedEqual(Pair.Value, V, P, Owned, bTree, FlagTransitions)) { return false; }
        }
        return true;
    }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!SavedEqual(X[I], Y[I], Path + TEXT("/") + FString::FromInt(I), Owned, bTree, FlagTransitions)) { return false; } } return true; }
    if (A->Type == EJson::String) { if (bTree && Path == TEXT("/graph/properties/ModCounter/values/0")) { return A->AsString().IsNumeric() && B->AsString() == FString::FromInt(FCString::Atoi(*A->AsString()) + 1); } return A->AsString() == B->AsString(); }
    if (A->Type == EJson::Number) { return bTree && Path == TEXT("/graph/mod_counter") ? B->AsNumber() == A->AsNumber() + 1 : A->AsNumber() == B->AsNumber(); }
    if (A->Type == EJson::Boolean) { return Path == TEXT("/package_dirty") ? !B->AsBool() : A->AsBool() == B->AsBool(); }
    return A->Type == EJson::Null;
}
bool SaveOne(UObject* Asset, bool bTree, const FString& BBPackage, const FString& BTPackage,
    const TSharedRef<FJsonObject>& Expected, const TSharedRef<FJsonObject>& SourcesBefore,
    const TSharedRef<FJsonObject>& DirtyBefore, TArray<TSharedPtr<FJsonValue>>& Calls, FResult& R)
{
    auto* G = bTree ? Cast<UBehaviorTreeGraph>(CastChecked<UBehaviorTree>(Asset)->BTGraph) : nullptr;
    if (bTree && (!Loaded(G) || G->IsLocked() || G->ModCounter == MAX_int32)) { return Fail(R, TEXT("behavior_assets_save_graph_invalid"), TEXT("The same audited unlocked graph and bounded rebuild counter are required before any target save.")); }
    if (bTree)
    {
        const auto References = Expected->GetObjectField(TEXT("blackboard_references")); const auto* B = CastChecked<UBehaviorTree>(Asset);
        TSharedPtr<FJsonValue> Found; if (B->BlackboardAsset) { Found = References->TryGetField(B->BlackboardAsset->GetPathName()); }
        if (!Found.IsValid() || Found->Type != EJson::Object || !Found->AsObject()->GetObjectField(TEXT("disk"))->GetBoolField(TEXT("exists")) || Found->AsObject()->GetObjectField(TEXT("disk"))->GetNumberField(TEXT("bytes")) <= 0)
        { return Fail(R, TEXT("behavior_assets_blackboard_not_persisted"), TEXT("The created referenced Blackboard must have a nonempty observed file before any tree lock/save; prior Blackboard save prefix is retained.")); }
    }
    R.Stage = bTree ? TEXT("save_behavior_tree") : TEXT("save_blackboard"); if (G) { G->LockUpdates(); }
    R.bSaveAttempted = true; auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("asset_path"), Asset->GetPathName()); Row->SetStringField(TEXT("asset_type"), bTree ? TEXT("behavior_tree") : TEXT("blackboard"));
    Row->SetNumberField(TEXT("attempt_index"), Calls.Num() + 1); Row->SetBoolField(TEXT("package_dirty_before"), Asset->GetOutermost()->IsDirty()); Row->SetBoolField(TEXT("graph_locked_during_save"), G && G->IsLocked());
    bool Last = UEditorAssetLibrary::SaveLoadedAsset(Asset, false); R.bSaved |= Last; Row->SetBoolField(TEXT("success"), Last); Row->SetBoolField(TEXT("package_dirty_after_call"), Asset->GetOutermost()->IsDirty());
    if (G) { G->UnlockUpdates(); R.Data->SetNumberField(TEXT("unlock_update_calls"), 1); R.Data->SetNumberField(TEXT("total_graph_unlock_update_calls"), R.Data->GetNumberField(TEXT("construction_unlock_update_calls")) + 1); } Row->SetBoolField(TEXT("unlock_update_dispatched"), G != nullptr); Row->SetBoolField(TEXT("package_dirty_after"), Asset->GetOutermost()->IsDirty());
    Calls.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("save_calls"), Calls); R.Data->SetNumberField(TEXT("save_attempt_count"), Calls.Num());
    if (G && Last && Asset->GetOutermost()->IsDirty())
    {
        auto Second = MakeShared<FJsonObject>(); Second->SetStringField(TEXT("asset_path"), Asset->GetPathName()); Second->SetStringField(TEXT("asset_type"), TEXT("behavior_tree")); Second->SetNumberField(TEXT("attempt_index"), Calls.Num() + 1);
        Second->SetBoolField(TEXT("package_dirty_before"), true); Second->SetBoolField(TEXT("graph_locked_during_save"), false); Last = UEditorAssetLibrary::SaveLoadedAsset(Asset, false); R.bSaved |= Last;
        Second->SetBoolField(TEXT("success"), Last); Second->SetBoolField(TEXT("package_dirty_after_call"), Asset->GetOutermost()->IsDirty()); Second->SetBoolField(TEXT("package_dirty_after"), Asset->GetOutermost()->IsDirty()); Second->SetBoolField(TEXT("unlock_update_dispatched"), false);
        Calls.Add(MakeShared<FJsonValueObject>(Second)); R.Data->SetArrayField(TEXT("save_calls"), Calls); R.Data->SetNumberField(TEXT("save_attempt_count"), Calls.Num());
    }
    auto After = MakeShared<FJsonObject>(); const bool Observed = Read(Asset->GetPathName(), bTree, After, R, bTree ? TEXT("behavior_tree_after_save_native_result") : TEXT("blackboard_after_save_native_result"));
    if (Observed) { R.Data->SetObjectField(bTree ? TEXT("behavior_tree_after_save") : TEXT("blackboard_after_save"), After); }
    if (!Observed || !Last) { R.bKnown = false; R.bPost = false; return Fail(R, !Last ? TEXT("behavior_assets_save_failed") : TEXT("behavior_assets_save_state_unobservable"), TEXT("Actual save/unlock prefix retained; false save or incomplete observer leaves disk outcome uncertified. No failed-save retry or rollback.")); }
    const auto State = After->GetObjectField(TEXT("state")); const bool Saved = !Asset->GetOutermost()->IsDirty() && After->GetObjectField(TEXT("disk"))->GetBoolField(TEXT("exists")) && After->GetObjectField(TEXT("disk"))->GetNumberField(TEXT("bytes")) > 0;
    R.Data->SetBoolField(bTree ? TEXT("behavior_tree_saved") : TEXT("blackboard_saved"), Saved);
    R.bSaved |= R.Data->GetBoolField(TEXT("blackboard_saved")) && R.Data->GetBoolField(TEXT("behavior_tree_saved"));
    TSet<FString> Owned; OwnedPaths(MakeShared<FJsonValueObject>(Expected->GetObjectField(TEXT("state"))), Asset->GetPathName(), Owned); int32 FlagTransitions = 0;
    const bool Same = SavedEqual(MakeShared<FJsonValueObject>(Expected->GetObjectField(TEXT("state"))), MakeShared<FJsonValueObject>(State), TEXT(""), Owned, bTree, FlagTransitions);
    auto SourceNow = MakeShared<FJsonObject>(), DirtyNow = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> NewCDOs;
    const bool ReferencesPreserved = !bTree || Text(Expected->GetObjectField(TEXT("blackboard_references")).ToSharedRef()) == Text(After->GetObjectField(TEXT("blackboard_references")).ToSharedRef());
    const bool SourceObserved = Sources(SourceNow), DirtyObserved = Dirty(BBPackage, BTPackage, DirtyNow), Complete = SourceObserved && DirtyObserved;
    const bool Preserved = Complete && ProtectSources(SourcesBefore, SourceNow, false, NewCDOs) && ReferencesPreserved && Text(DirtyBefore) == Text(DirtyNow);
    R.Data->SetObjectField(bTree ? TEXT("behavior_tree_save_native_sources") : TEXT("blackboard_save_native_sources"), SourceNow); R.Data->SetObjectField(bTree ? TEXT("behavior_tree_save_other_dirty") : TEXT("blackboard_save_other_dirty"), DirtyNow);
    R.Data->SetBoolField(bTree ? TEXT("behavior_tree_save_observations_complete") : TEXT("blackboard_save_observations_complete"), Complete);
    R.Data->SetBoolField(bTree ? TEXT("behavior_tree_save_blackboard_preserved") : TEXT("blackboard_save_references_preserved"), ReferencesPreserved);
    R.Data->SetBoolField(bTree ? TEXT("behavior_tree_save_state_preserved") : TEXT("blackboard_save_state_preserved"), Same); R.Data->SetBoolField(bTree ? TEXT("behavior_tree_save_sources_preserved") : TEXT("blackboard_save_sources_preserved"), Preserved);
    R.Data->SetNumberField(bTree ? TEXT("behavior_tree_save_flag_transitions") : TEXT("blackboard_save_flag_transitions"), FlagTransitions);
    if (!Complete) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("behavior_assets_save_state_unobservable"), TEXT("Actual successful save prefix and target file/state are retained, but protected source/dirty observations are incomplete; no writer retry.")); }
    if (!Saved || !Same || !Preserved) { R.bPost = false; return Fail(R, TEXT("behavior_assets_save_postcondition_failed"), TEXT("Save returned true; actual saved bytes and raw states are retained, but exact owned-state/source preservation did not pass. Do not replay the writer.")); }
    return true;
}

// IZ concrete destructive phase. Deleted slots become plain metadata BEFORE the
// public force-delete call; no old asset/subobject getters are used afterwards.
bool CreatingBehaviorPair=false,ReplacementQuarantined=false;
struct FReplacement
{
    FResult& R;TWeakObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    FString WorldPath,Packages[2],Paths[2];
    bool Exists[2]={false,false},Captured[2]={false,false},Retired[2]={false,false},Stopped=false,Recreating=false;
    struct FOwned{TStrongObjectPtr<UObject> Hold;TWeakObjectPtr<UObject> Weak;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;int32 Slot=0;};
    TArray<FOwned> Owned;TSharedPtr<FJsonObject> BeforeState[2],BeforeDisk[2];
    TStrongObjectPtr<UPackage> NewPackages[2];EObjectFlags NewPackageFlags[2]={RF_NoFlags,RF_NoFlags};
    TSharedRef<FJsonObject> Log=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls;
    FReplacement(FResult& In,const FString& BT,const FString& BB,const FString& BTO,const FString& BBO):R(In)
    {
        Packages[0]=BT;Packages[1]=BB;Paths[0]=BTO;Paths[1]=BBO;Editor=GEditor;
        if(IsInGameThread()&&Loaded(GEditor)&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage())
        {World.Reset(GEditor->GetEditorWorldContext().World());if(Loaded(World.Get()))WorldPath=World->GetPathName();}
        R.Data->SetObjectField(TEXT("replacement"),Log);
        Log->SetStringField(TEXT("scope"),TEXT("Clean saved exact native pair, no external saved/loaded UObject referencers; original tree-then-board DeleteAsset then existing fresh builder. SDK global/undo/GC/editor effects are not pure or atomic."));
        for(const TCHAR* K:{TEXT("prior_asset_deleted"),TEXT("behavior_tree_deleted"),TEXT("blackboard_deleted")})R.Data->SetBoolField(K,false);
        R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);
    }
    bool Stop(const TCHAR* Stage,const TCHAR* Code,const FString& Error)
    {
        if(!Stopped){Log->SetStringField(TEXT("first_failure_stage"),Stage);Log->SetStringField(TEXT("first_failure_code"),Code);}
        Stopped=true;R.bKnown=false;R.bPost=false;R.Stage=Stage;return Fail(R,Code,Error);
    }
    bool Context(const TCHAR* Stage)
    {
        if(Stopped)return false;
        if(!IsInGameThread()||!Loaded(GEditor)||GEditor!=Editor.Get()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage())
            return Stop(Stage,TEXT("behavior_assets_replacement_context_changed"),TEXT("Same live Editor outside GC/exit/save required; actual prefix retained, no further observation/replay."));
        if(GEditor->PlayWorld||GEditor->GetPIEWorldContext()||!Loaded(World.Get())||World->bIsTearingDown||World->WorldType!=EWorldType::Editor
            ||GEditor->GetEditorWorldContext().World()!=World.Get()||!World->GetPathName().Equals(WorldPath,ESearchCase::CaseSensitive))
            return Stop(Stage,TEXT("behavior_assets_replacement_world_changed"),TEXT("Exact idle Editor world changed during replacement."));
        return true;
    }
    UObject* Root(int32 Slot) const
    {for(const auto& I:Owned)if(I.Slot==Slot&&I.Path.Equals(Paths[Slot],ESearchCase::CaseSensitive))return I.Hold.Get();return nullptr;}
    bool Check(const TCHAR* Stage)
    {
        if(!Context(Stage))return false;
        for(const auto& I:Owned)
        {
            if(Retired[I.Slot])continue;UObject* O=I.Hold.Get();
            if(!Loaded(O)||!Loaded(I.Class)||!Loaded(I.Package)||(I.Outer&&!Loaded(I.Outer))||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer
                ||O->GetPackage()!=I.Package||O->GetOutermost()!=I.Package||O->GetFlags()!=I.Flags||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive))
                return Stop(Stage,TEXT("behavior_assets_replacement_identity_changed"),TEXT("Retained selected export identity/flags changed; no next delete/recreate."));
        }
        for(int32 I=0;I<2;++I)
        {
            if(!Captured[I]||Retired[I]||Recreating)continue;
            UObject* Now=FindObject<UObject>(nullptr,*Paths[I]);
            if((Exists[I]&&(!Loaded(Now)||Now!=Root(I)))||(!Exists[I]&&(Now||FindPackage(nullptr,*Packages[I]))))
                return Stop(Stage,TEXT("behavior_assets_replacement_target_changed"),TEXT("Selected target changed or appeared during callback."));
        }
        return true;
    }
    bool Hold(UObject* O,int32 Slot)
    {
        if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter()))||Owned.Num()>=4096)
            return Stop(TEXT("replacement_capture"),TEXT("behavior_assets_replacement_identity_unavailable"),TEXT("Bounded live selected export identities required."));
        FOwned I;I.Hold.Reset(O);I.Weak=O;I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();I.Slot=Slot;Owned.Add(MoveTemp(I));return true;
    }
    bool Existing(int32 Slot,const TSharedRef<FJsonObject>& File)
    {
        BeforeDisk[Slot]=File;
        if(Paths[Slot].Len()>=NAME_SIZE)return Fail(R,TEXT("behavior_assets_replacement_path_bound"),TEXT("Expanded object path must remain shorter than NAME_SIZE before LoadObject."));
        if(!Check(TEXT("replacement_before_load")))return false;
        auto& Registry=FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        const bool Registered=Registry.GetAssetByObjectPath(FSoftObjectPath(Paths[Slot])).IsValid();
        if(!Check(TEXT("replacement_after_registry_lookup")))return false;
        Exists[Slot]=File->GetBoolField(TEXT("exists"))||Registered||FindObject<UObject>(nullptr,*Paths[Slot])!=nullptr;
        if(!Exists[Slot])
        {
            if(FindPackage(nullptr,*Packages[Slot]))return Fail(R,TEXT("behavior_assets_replacement_empty_package_unsupported"),TEXT("Existing empty/unsaved package is outside persisted-pair replacement."));
            Captured[Slot]=true;return true;
        }
        if(!File->GetBoolField(TEXT("exists"))||!Registered||IFileManager::Get().IsReadOnly(*File->GetStringField(TEXT("path"))))
            return Fail(R,TEXT("behavior_assets_replacement_persisted_target_required"),TEXT("Existing targets must be registered writable saved assets; unsaved/read-only variants are not deleted."));
        R.bKnown=false;
        auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("path"),Paths[Slot]);Call->SetBoolField(TEXT("entered"),true);Log->SetObjectField(Slot==0?TEXT("tree_load"):TEXT("board_load"),Call);
        UObject* O=Slot==0?static_cast<UObject*>(LoadObject<UBehaviorTree>(nullptr,*Paths[Slot])):static_cast<UObject*>(LoadObject<UBlackboardData>(nullptr,*Paths[Slot]));Call->SetBoolField(TEXT("returned"),true);
        if(!Context(TEXT("replacement_after_load")))return false;
        UClass* Expected=Slot==0?UBehaviorTree::StaticClass():UBlackboardData::StaticClass();
        if(!Loaded(O)||O->GetClass()!=Expected||!Loaded(O->GetPackage())||O->GetOuter()!=O->GetPackage()||O->GetPackage()!=O->GetOutermost()
            ||!O->GetPackage()->GetName().Equals(Packages[Slot],ESearchCase::CaseSensitive)||O->IsPackageExternal()||O->IsTemplate()||O->HasAnyFlags(RF_Transient)
            ||O->GetPackage()->IsDirty()||!O->GetPackage()->IsFullyLoaded()||O->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_PlayInEditor|PKG_NewlyCreated))
            return Stop(TEXT("replacement_loaded_target"),TEXT("behavior_assets_replacement_target_unsupported"),TEXT("Target is not a clean fully loaded exact native owned asset."));
        if(!Hold(O,Slot))return false;Captured[Slot]=true;
        TArray<UObject*> Children;GetObjectsWithOuter(O,Children,EGetObjectsFlags::IncludeNestedObjects);
        if(!Check(TEXT("replacement_after_export_enumeration")))return false;
        for(UObject* Child:Children)
        {if(!Loaded(Child)||!Child->IsIn(O)||Child->GetPackage()!=O->GetPackage()||!Hold(Child,Slot))return Stop(TEXT("replacement_capture_exports"),TEXT("behavior_assets_replacement_export_unsupported"),TEXT("Selected subtree is unavailable or crosses package ownership."));}
        auto* Editors=GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();if(!Check(TEXT("replacement_asset_editor_admission")))return false;
        if(!Loaded(Editors))return Stop(TEXT("replacement_asset_editor_admission"),TEXT("behavior_assets_replacement_editor_subsystem_unavailable"),TEXT("Loaded asset-editor subsystem required before isolated deletion."));
        const bool Open=Editors->FindEditorForAsset(O,false)!=nullptr;
        if(!Check(TEXT("replacement_after_editor_query")))return false;
        if(Open)return Fail(R,TEXT("behavior_assets_replacement_editor_open"),TEXT("Close selected target editors before isolated replacement; Cubeless does not close them."));
        return true;
    }
    bool ObserveRemaining(const TCHAR* Stage)
    {
        if(!Check(Stage))return false;
        for(int32 I=0;I<2;++I)if(Exists[I]&&!Retired[I])
        {
            auto O=MakeShared<FJsonObject>();const bool Complete=Read(Paths[I],I==0,O,R,I==0?TEXT("replacement_tree_observation"):TEXT("replacement_board_observation"));
            if(!Check(TEXT("replacement_after_target_observation")))return false;
            if(!Complete)return Stop(Stage,TEXT("behavior_assets_replacement_target_unobservable"),TEXT("Complete native target observation required before the next delete."));
            const auto State=O->GetObjectField(TEXT("state"));
            if(!BeforeState[I]){BeforeState[I]=State;Log->SetObjectField(I==0?TEXT("tree_before"):TEXT("board_before"),O);}
            else if(!Text(BeforeState[I].ToSharedRef()).Equals(Text(State.ToSharedRef()),ESearchCase::CaseSensitive))
                return Stop(Stage,TEXT("behavior_assets_replacement_retained_state_changed"),TEXT("A not-yet-deleted selected asset changed during callback."));
        }
        return Check(Stage);
    }
    bool References(const TCHAR* Stage)
    {
        if(!ObserveRemaining(Stage))return false;
        auto& Registry=FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        if(Registry.IsLoadingAssets())return Stop(Stage,TEXT("behavior_assets_replacement_registry_busy"),TEXT("Completed saved reference discovery required."));
        TArray<TSharedPtr<FJsonValue>> Foreign;
        for(int32 I=0;I<2;++I)if(Exists[I]&&!Retired[I])
        {
            TArray<FName> Refs;const bool Complete=Registry.GetReferencers(FName(*Packages[I]),Refs,UE::AssetRegistry::EDependencyCategory::Package);
            if(!Check(TEXT("replacement_after_saved_referencers")))return false;
            if(!Complete||Refs.Num()>MaxPackages)return Stop(Stage,TEXT("behavior_assets_replacement_references_incomplete"),TEXT("Saved referencer discovery incomplete."));
            for(FName Ref:Refs)if(!Ref.ToString().Equals(Packages[I],ESearchCase::CaseSensitive)&&!(I==1&&!Retired[0]&&Ref.ToString().Equals(Packages[0],ESearchCase::CaseSensitive)))Foreign.Add(MakeShared<FJsonValueString>(Ref.ToString()));
        }
        TArray<UObject*> Roots;TSet<UObject*> Ignore;
        for(const auto& I:Owned)if(!Retired[I.Slot]){Roots.Add(I.Hold.Get());Ignore.Add(I.Hold.Get());Ignore.Add(I.Package);}
        // Installed 5.8 TStrongObjectPtr uses FUObjectItem refcounts, not a
        // referencing UObject returned by this GC-schema scanner. Keep every
        // unretired selected strong hold across the public scan and admission.
        const auto LoadedRefs=FReferencerFinder::GetAllReferencers(Roots,&Ignore,EReferencerFinderFlags::SkipWeakReferences);
        if(!Check(TEXT("replacement_after_loaded_referencers")))return false;
        if(LoadedRefs.Num()>MaxPackages)return Stop(Stage,TEXT("behavior_assets_replacement_references_incomplete"),TEXT("Loaded referencer bound exceeded."));
        for(UObject* Ref:LoadedRefs){if(!Loaded(Ref))return Stop(Stage,TEXT("behavior_assets_replacement_references_incomplete"),TEXT("Loaded referencer unavailable; no further observation."));Foreign.Add(MakeShared<FJsonValueString>(Ref->GetPathName()));}
        Log->SetArrayField(TEXT("external_referencers"),Foreign);
        if(!ObserveRemaining(TEXT("replacement_after_referencer_observation")))return false;
        return true;
    }
    bool Fence(const TSharedRef<FJsonObject>& Source,const TSharedRef<FJsonObject>& Other,const TCHAR* Stage)
    {
        if(!ObserveRemaining(Stage))return false;
        auto S=MakeShared<FJsonObject>(),D=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> NewCDOs;
        const bool Complete=Sources(S)&&Check(TEXT("replacement_after_native_sources"))&&Dirty(Packages[1],Packages[0],D)&&Check(TEXT("replacement_after_other_dirty"));
        Log->SetObjectField(TEXT("native_sources_latest"),S);Log->SetObjectField(TEXT("other_dirty_latest"),D);
        if(!Complete)return Stop(Stage,TEXT("behavior_assets_replacement_fence_incomplete"),TEXT("Native-source/dirty fence incomplete."));
        if(!ProtectSources(Source,S,true,NewCDOs)||!Text(Other).Equals(Text(D),ESearchCase::CaseSensitive))return Stop(Stage,TEXT("behavior_assets_replacement_fence_changed"),TEXT("Native defaults or other package dirty state changed."));
        for(int32 I=0;I<2;++I)if(!Retired[I])
        {
            auto File=MakeShared<FJsonObject>();const bool CompleteFile=Disk(Packages[I],File);
            if(!Check(TEXT("replacement_after_retained_disk")))return false;
            if(!CompleteFile||!BeforeDisk[I]||!Text(BeforeDisk[I].ToSharedRef()).Equals(Text(File),ESearchCase::CaseSensitive))
                return Stop(Stage,TEXT("behavior_assets_replacement_retained_disk_changed"),TEXT("Not-yet-deleted selected file changed or became unobservable."));
        }
        return true;
    }
    bool Delete(int32 Slot,const TSharedRef<FJsonObject>& Source,const TSharedRef<FJsonObject>& Other,TSharedRef<FJsonObject>& File)
    {
        if(!Exists[Slot])return true;
        if(!References(TEXT("replacement_before_delete"))||!Fence(Source,Other,TEXT("replacement_before_delete_fence")))return false;
        if(Log->GetArrayField(TEXT("external_referencers")).Num()>0)
        {R.bKnown=true;return Fail(R,TEXT("behavior_assets_replacement_external_references"),TEXT("External saved/loaded subtree referencers are outside isolated replacement; complete selected state/source/disk fence retained, no next delete."));}
        Retired[Slot]=true;
        // ForceDelete/GC owns this slot. Release our selected object refcounts
        // so retired objects are not kept alive across package unload/GC. No SDK
        // is entered between plain release/receipt preparation and DeleteAsset.
        for(auto& I:Owned)if(I.Slot==Slot)I.Hold.Reset();
        R.Stage=Slot==0?TEXT("delete_existing_behavior_tree"):TEXT("delete_existing_blackboard");R.bAttempted=true;R.bKnown=false;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),Paths[Slot]);Row->SetNumberField(TEXT("order"),Calls.Num()+1);Row->SetBoolField(TEXT("entered"),true);Row->SetBoolField(TEXT("returned"),false);
        Calls.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("delete_calls"),Calls);
        const bool Deleted=UEditorAssetLibrary::DeleteAsset(Packages[Slot]);Row->SetBoolField(TEXT("returned"),true);Row->SetBoolField(TEXT("success"),Deleted);
        if(Deleted){R.bChanged=true;R.Data->SetBoolField(TEXT("prior_asset_deleted"),true);R.Data->SetBoolField(Slot==0?TEXT("behavior_tree_deleted"):TEXT("blackboard_deleted"),true);R.Data->SetNumberField(TEXT("asset_delete_count"),R.Data->GetNumberField(TEXT("asset_delete_count"))+1);}
        if(!Check(TEXT("replacement_after_delete")))return false;
        auto After=MakeShared<FJsonObject>();const bool FileKnown=Disk(Packages[Slot],After);Row->SetObjectField(TEXT("disk_after"),After);
        if(!Check(TEXT("replacement_after_delete_disk")))return false;
        auto& Registry=FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();const bool Registered=Registry.GetAssetByObjectPath(FSoftObjectPath(Paths[Slot])).IsValid();
        if(!Check(TEXT("replacement_after_delete_registry")))return false;
        const bool MemoryPresent=FindObject<UObject>(nullptr,*Paths[Slot])!=nullptr;
        Row->SetBoolField(TEXT("registry_present_after"),Registered);Row->SetBoolField(TEXT("memory_present_after"),MemoryPresent);Row->SetBoolField(TEXT("absence_observed"),FileKnown&&!After->GetBoolField(TEXT("exists"))&&!Registered&&!MemoryPresent);
        if(!Deleted||!FileKnown||After->GetBoolField(TEXT("exists"))||Registered||MemoryPresent)return Stop(TEXT("replacement_delete_result"),TEXT("behavior_assets_replacement_delete_failed"),TEXT("DeleteAsset failed or selected absence incomplete; actual prefix retained, no recreate/restore/retry."));
        if(!Fence(Source,Other,TEXT("replacement_after_delete_fence")))return false;
        File=After;R.bKnown=true;return true;
    }
    bool Prepare(const TSharedRef<FJsonObject>& Source,const TSharedRef<FJsonObject>& Other,TSharedRef<FJsonObject>& BBFile,TSharedRef<FJsonObject>& BTFile)
    {
        if(!Context(TEXT("replacement_preflight"))||!Existing(0,BTFile)||!Existing(1,BBFile)||!ObserveRemaining(TEXT("replacement_baseline"))||!Fence(Source,Other,TEXT("replacement_baseline_fence")))return false;
        Log->SetBoolField(TEXT("baseline_complete"),true);R.bKnown=true;R.Data->SetBoolField(TEXT("existing_target_replacement_supported"),true);
        if(!Delete(0,Source,Other,BTFile)||!Delete(1,Source,Other,BBFile))return false;
        Log->SetBoolField(TEXT("ordered_deletion_complete"),true);Log->SetBoolField(TEXT("old_identity_read_after_delete"),false);
        R.Data->SetObjectField(TEXT("blackboard_disk_before_recreate"),BBFile);R.Data->SetObjectField(TEXT("behavior_tree_disk_before_recreate"),BTFile);
        if(!Check(TEXT("replacement_before_recreate")))return false;
        Recreating=true;return true;
    }
    bool AdoptPackage(UPackage* P,int32 Slot,const TCHAR* Stage)
    {
        if(!Context(Stage))return false;
        if(!Loaded(P))return Stop(Stage,TEXT("behavior_assets_replacement_package_unavailable"),TEXT("CreatePackage did not return a live selected package."));
        NewPackages[Slot].Reset(P); // Immediate strong return ownership, before further SDK.
        if(P->GetClass()!=UPackage::StaticClass()||P->GetOuter()!=nullptr||P->GetPackage()!=P||P->GetOutermost()!=P
            ||!P->GetName().Equals(Packages[Slot],ESearchCase::CaseSensitive)||FindPackage(nullptr,*Packages[Slot])!=P
            ||P->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||P->IsPackageExternal()
            ||P->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_PlayInEditor)||P->IsDirty())
            return Stop(Stage,TEXT("behavior_assets_replacement_package_identity_changed"),TEXT("Returned new selected package has unexpected current identity/class/owner/flags/dirty role."));
        NewPackageFlags[Slot]=P->GetFlags();return true;
    }
    bool Recreate(UBlackboardData* BB,UBehaviorTree* BT,const TCHAR* Stage)
    {
        if(!Check(Stage))return false;
        UObject* Expected[2]={BT,BB};
        for(int32 I=0;I<2;++I)
        {
            UPackage* CurrentPackage=FindPackage(nullptr,*Packages[I]);UPackage* Planned=NewPackages[I].Get();
            if(CurrentPackage!=Planned||(Planned&&(!Loaded(Planned)||Planned->GetClass()!=UPackage::StaticClass()||Planned->GetOuter()!=nullptr
                ||Planned->GetPackage()!=Planned||Planned->GetOutermost()!=Planned||Planned->GetFlags()!=NewPackageFlags[I]
                ||!Planned->GetName().Equals(Packages[I],ESearchCase::CaseSensitive)||Planned->IsPackageExternal()
                ||Planned->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_PlayInEditor))))
                return Stop(Stage,TEXT("behavior_assets_replacement_package_changed"),TEXT("Selected package reappeared before planned allocation or current strong package role changed; no reuse/load/allocation."));
            UObject* Now=FindObject<UObject>(nullptr,*Paths[I]);
            if(Now!=Expected[I])return Stop(Stage,TEXT("behavior_assets_replacement_recreate_collision"),TEXT("Selected object appeared or changed after deletion; no overwrite allocation is dispatched."));
            if(Now&&(!Loaded(Now)||!Loaded(Now->GetPackage())||Now->GetClass()!=(I==0?UBehaviorTree::StaticClass():UBlackboardData::StaticClass())
                ||Now->GetOuter()!=Planned||Now->GetPackage()!=Planned||Now->GetOutermost()!=Planned||!Now->GetPackage()->GetName().Equals(Packages[I],ESearchCase::CaseSensitive)
                ||Now->IsPackageExternal()||Now->IsTemplate()||Now->HasAnyFlags(RF_Transient)))
                return Stop(Stage,TEXT("behavior_assets_replacement_recreate_identity_changed"),TEXT("New selected root has unexpected class/owner/package or asset role."));
            auto File=MakeShared<FJsonObject>();const bool Complete=Disk(Packages[I],File);
            if(!Check(TEXT("replacement_recreate_after_disk")))return false;
            if(!Complete||File->GetBoolField(TEXT("exists")))return Stop(Stage,TEXT("behavior_assets_replacement_recreate_disk_collision"),TEXT("Selected file reappeared before explicit save; no next allocation."));
        }
        return true;
    }
};
void RetainFailedReplacement(TUniquePtr<FReplacement>& Scope)
{
    if(!Scope||!Scope->Stopped)return;
    // No new UObject getters or release after a failed admission. The abandoned
    // R reference is never read again; this is process-lifetime retention only.
    static auto* Retained=new TArray<TUniquePtr<FReplacement>>();
    Scope->Log->SetBoolField(TEXT("remaining_strong_holds_retained_until_process_exit"),true);
    ReplacementQuarantined=true;Retained->Add(MoveTemp(Scope));
}
}

FString CreatePatrolChaseAssets(const FString& AssetRoot, const FString& BehaviorTreeName, const FString& BlackboardName,
    const FString& TargetActorKey, const FString& PatrolLocationKey, const FString& TreeShape,
    double AcceptableRadius, double AlertWaitSeconds, double FacePrecisionDegrees, bool bReplaceExisting, bool bSaveAssets)
{
    FResult R; Init(R, bSaveAssets, bReplaceExisting);
    if(CreatingBehaviorPair||ReplacementQuarantined){Fail(R,TEXT("behavior_assets_reentrant_or_retained_request"),TEXT("A behavior-pair writer or unsafe retained replacement is active; no nested/replay dispatch."));return Encode(R);}
    TGuardValue<bool> WriterLease(CreatingBehaviorPair,true);
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("behavior_assets_editor_state_invalid"), TEXT("Requires Editor game thread outside PIE/SIE.")); return Encode(R); }
    FString Root = AssetRoot; Root.ReplaceInline(TEXT("\\"), TEXT("/")); while (Root.EndsWith(TEXT("/"))) { Root.LeftChopInline(1); } FString Shape = TreeShape.ToLower();
    const TSet<FName> KeyNames = { FName(*TargetActorKey), FName(*PatrolLocationKey), TEXT("LastKnownTargetLocation"), TEXT("AIState") };
    if (!Name(BehaviorTreeName) || !Name(BlackboardName) || BehaviorTreeName.Equals(BlackboardName, ESearchCase::IgnoreCase) || !Name(TargetActorKey) || !Name(PatrolLocationKey) || KeyNames.Num() != 4 || KeyNames.Contains(FBlackboard::KeySelf)
        || !(Root == TEXT("/Game") || Root.StartsWith(TEXT("/Game/"))) || !FPackageName::IsValidLongPackageName(Root) || (Shape != TEXT("sequence_patrol_chase") && Shape != TEXT("selector_chase_patrol") && Shape != TEXT("selector_alert_chase_patrol")))
    { Fail(R, TEXT("behavior_assets_inputs_invalid"), TEXT("Use distinct simple asset names, four distinct non-SelfActor key names, a /Game root and one original exact tree shape.")); return Encode(R); }
    for (double Number : { AcceptableRadius, AlertWaitSeconds, FacePrecisionDegrees }) { if (!FMath::IsFinite(Number) || FMath::Abs(Number) > MAX_flt) { Fail(R, TEXT("behavior_assets_number_invalid"), TEXT("Only finite float-representable numeric values are admitted before mutation.")); return Encode(R); } }
    const float Radius = FMath::Max(0.0f, static_cast<float>(AcceptableRadius)), Wait = FMath::Max(0.0f, static_cast<float>(AlertWaitSeconds)), Precision = FMath::Clamp(static_cast<float>(FacePrecisionDegrees), 0.0f, 180.0f);
    const FString BBPackage = Root + TEXT("/") + BlackboardName, BTPackage = Root + TEXT("/") + BehaviorTreeName, BBPath = BBPackage + TEXT(".") + BlackboardName, BTPath = BTPackage + TEXT(".") + BehaviorTreeName;
    if (BBPackage.Len() >= NAME_SIZE || BTPackage.Len() >= NAME_SIZE || !FPackageName::IsValidLongPackageName(BBPackage) || !FPackageName::IsValidLongPackageName(BTPackage) || !FPackageName::IsValidObjectPath(BBPath) || !FPackageName::IsValidObjectPath(BTPath))
    { Fail(R, TEXT("behavior_assets_inputs_invalid"), TEXT("Computed target packages must remain valid below the original Unreal package-name bound; no observation/allocation/deletion/save dispatched.")); return Encode(R); }
    R.Data->SetStringField(TEXT("blackboard_path"), BBPath); R.Data->SetStringField(TEXT("behavior_tree_path"), BTPath); R.Data->SetStringField(TEXT("blackboard_package"), BBPackage); R.Data->SetStringField(TEXT("behavior_tree_package"), BTPackage);
    R.Data->SetStringField(TEXT("tree_shape"), Shape); R.Data->SetNumberField(TEXT("acceptable_radius"), Radius); R.Data->SetNumberField(TEXT("alert_wait_seconds"), Wait); R.Data->SetNumberField(TEXT("face_precision_degrees"), Precision);
    auto SourceBefore = MakeShared<FJsonObject>(), DirtyBefore = MakeShared<FJsonObject>(), BBDisk = MakeShared<FJsonObject>(), BTDisk = MakeShared<FJsonObject>();
    R.Data->SetObjectField(TEXT("native_sources_before"), SourceBefore); R.Data->SetObjectField(TEXT("other_package_dirty_before"), DirtyBefore); R.Data->SetObjectField(TEXT("blackboard_disk_before"), BBDisk); R.Data->SetObjectField(TEXT("behavior_tree_disk_before"), BTDisk);
    if (!Sources(SourceBefore) || !Dirty(BBPackage, BTPackage, DirtyBefore) || !Disk(BBPackage, BBDisk) || !Disk(BTPackage, BTDisk)) { Fail(R, TEXT("behavior_assets_preflight_unobservable"), TEXT("Bounded native defaults, external dirty state or exact target disks are incomplete; no allocation.")); return Encode(R); }
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    const bool Existing = BBDisk->GetBoolField(TEXT("exists")) || BTDisk->GetBoolField(TEXT("exists")) || FindObject<UObject>(nullptr, *BBPath) || FindObject<UObject>(nullptr, *BTPath)
        || FindPackage(nullptr, *BBPackage) || FindPackage(nullptr, *BTPackage) || Registry.GetAssetByObjectPath(FSoftObjectPath(BBPath)).IsValid() || Registry.GetAssetByObjectPath(FSoftObjectPath(BTPath)).IsValid();
    TUniquePtr<FReplacement> Replacement;
    if (Existing)
    {
        if(!bReplaceExisting){Fail(R,TEXT("behavior_assets_target_exists"),TEXT("At least one exact target already exists; replace_existing=false preserves both targets."));return Encode(R);}
        Replacement=MakeUnique<FReplacement>(R,BTPackage,BBPackage,BTPath,BBPath);
        if(!Replacement->Prepare(SourceBefore,DirtyBefore,BBDisk,BTDisk)){RetainFailedReplacement(Replacement);return Encode(R);}
    }
    if (!SelectorProperty(UBTTask_MoveDirectlyToward::StaticClass()) || !SelectorProperty(UBTTask_RotateToFaceBBEntry::StaticClass()) || !SelectorProperty(UBTDecorator_Blackboard::StaticClass())
        || !ByteProperty(UBTDecorator_Blackboard::StaticClass(), TEXT("OperationType")))
    { Fail(R, TEXT("behavior_assets_native_fields_unsupported"), TEXT("Exact public reflected key-selector/decorator field contract is unavailable; no allocation.")); return Encode(R); }
    for (const FName Name : { FName(TEXT("BasicOperation")), FName(TEXT("NotifyObserver")), FName(TEXT("FlowAbortMode")) })
    {
        FProperty* P = ByteProperty(UBTDecorator_Blackboard::StaticClass(), Name);
        if (!P || !P->HasAnyPropertyFlags(CPF_Edit)) { Fail(R, TEXT("behavior_assets_native_fields_unsupported"), TEXT("Exact editable decorator field contract is unavailable; no allocation.")); return Encode(R); }
    }
    if (Shape == TEXT("selector_alert_chase_patrol") && !PrecisionProperty())
    { Fail(R, TEXT("behavior_assets_native_fields_unsupported"), TEXT("Exact loaded editable Precision value-struct contract is unavailable; no allocation.")); return Encode(R); }
    R.Stage = TEXT("allocate_assets"); R.bAttempted = true;
    auto RecreateAdmission=[&](UBlackboardData* CurrentBB,UBehaviorTree* CurrentBT,const TCHAR* Stage)
    {if(!Replacement||Replacement->Recreate(CurrentBB,CurrentBT,Stage))return true;RetainFailedReplacement(Replacement);return false;};
    if(!RecreateAdmission(nullptr,nullptr,TEXT("replacement_before_package_allocation")))return Encode(R);
    TStrongObjectPtr<UPackage> BBPkg(CreatePackage(*BBPackage));
    R.Data->SetNumberField(TEXT("package_allocation_count"),BBPkg.IsValid()?1:0);
    if(Replacement&&!Replacement->AdoptPackage(BBPkg.Get(),1,TEXT("replacement_adopt_blackboard_package"))){RetainFailedReplacement(Replacement);return Encode(R);}
    if(!RecreateAdmission(nullptr,nullptr,TEXT("replacement_after_blackboard_package")))return Encode(R);
    TStrongObjectPtr<UPackage> BTPkg(CreatePackage(*BTPackage)); R.Data->SetNumberField(TEXT("package_allocation_count"), (BBPkg.IsValid() ? 1 : 0) + (BTPkg.IsValid() ? 1 : 0));
    if(Replacement&&!Replacement->AdoptPackage(BTPkg.Get(),0,TEXT("replacement_adopt_tree_package"))){RetainFailedReplacement(Replacement);return Encode(R);}
    if(!RecreateAdmission(nullptr,nullptr,TEXT("replacement_after_tree_package")))return Encode(R);
    if (!BBPkg.IsValid() || !BTPkg.IsValid()) { R.bKnown = false; Fail(R, TEXT("behavior_assets_package_allocation_failed"), TEXT("Package allocation prefix is retained; no rollback or retry.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("package_fully_load_dispatched"), true); R.Data->SetBoolField(TEXT("implicit_package_load_possible"), true); BBPkg->FullyLoad();
    if(!RecreateAdmission(nullptr,nullptr,TEXT("replacement_after_blackboard_fully_load")))return Encode(R);
    BTPkg->FullyLoad();if(!RecreateAdmission(nullptr,nullptr,TEXT("replacement_after_tree_fully_load")))return Encode(R);
    TStrongObjectPtr<UBlackboardData> BB(NewObject<UBlackboardData>(BBPkg.Get(), *BlackboardName, RF_Public | RF_Standalone | RF_Transactional));
    R.Data->SetBoolField(TEXT("blackboard_created"),BB.IsValid());R.Data->SetNumberField(TEXT("asset_allocation_count"),BB.IsValid()?1:0);
    if(!RecreateAdmission(BB.Get(),nullptr,TEXT("replacement_after_blackboard_allocation")))return Encode(R);
    TStrongObjectPtr<UBehaviorTree> BT(NewObject<UBehaviorTree>(BTPkg.Get(), *BehaviorTreeName, RF_Public | RF_Standalone | RF_Transactional));
    R.Data->SetBoolField(TEXT("behavior_tree_created"),BT.IsValid());R.Data->SetNumberField(TEXT("asset_allocation_count"),(BB.IsValid()?1:0)+(BT.IsValid()?1:0));
    if(!RecreateAdmission(BB.Get(),BT.Get(),TEXT("replacement_after_tree_allocation")))return Encode(R);
    R.Data->SetNumberField(TEXT("asset_allocation_count"), (BB.IsValid() ? 1 : 0) + (BT.IsValid() ? 1 : 0)); R.Data->SetBoolField(TEXT("blackboard_created"), BB.IsValid()); R.Data->SetBoolField(TEXT("behavior_tree_created"), BT.IsValid());
    if (!Loaded(BB.Get()) || !Loaded(BT.Get()) || BB->GetPathName() != BBPath || BT->GetPathName() != BTPath || BB->GetOuter() != BBPkg.Get() || BT->GetOuter() != BTPkg.Get()) { R.bKnown = false; Fail(R, TEXT("behavior_assets_asset_allocation_failed"), TEXT("Native asset allocation prefix retained; exact identity could not be certified.")); return Encode(R); }
    R.bChanged = true; auto FactoryBB = MakeShared<FJsonObject>(); if (!Read(BBPath, false, FactoryBB, R, TEXT("blackboard_factory_native_result"))) { R.bKnown = false; Fail(R, TEXT("behavior_assets_factory_blackboard_unobservable"), TEXT("New native factory defaults could not be observed; both assets retained without further mutation.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("blackboard_factory_default_state"), FactoryBB);
    if (BB->Parent || BB->Keys.Num() > 1 || (BB->Keys.Num() == 1 && BB->Keys[0].EntryName != FBlackboard::KeySelf)) { Fail(R, TEXT("behavior_assets_factory_keys_unsupported"), TEXT("Unexpected engine-provided Blackboard defaults retained; only the observed normal optional SelfActor baseline is supported.")); return Encode(R); }
    R.Stage = TEXT("build_blackboard");
    R.Data->SetNumberField(TEXT("factory_keys_reset_count"), BB->Keys.Num()); R.Data->SetBoolField(TEXT("blackboard_keys_reset_dispatched"), true); R.Data->SetBoolField(TEXT("implicit_persistent_key_creation_possible"), true); BB->Keys.Reset();
    if (!AppendKey(BB.Get(), FName(*TargetActorKey), UBlackboardKeyType_Object::StaticClass(), R) || !AppendKey(BB.Get(), FName(*PatrolLocationKey), UBlackboardKeyType_Vector::StaticClass(), R)
        || !AppendKey(BB.Get(), TEXT("LastKnownTargetLocation"), UBlackboardKeyType_Vector::StaticClass(), R) || !AppendKey(BB.Get(), TEXT("AIState"), UBlackboardKeyType_Name::StaticClass(), R))
    { R.bKnown = false; Fail(R, TEXT("behavior_assets_key_allocation_failed"), TEXT("Actual ordered key allocation prefix retained; no cleanup/retry.")); return Encode(R); }
    Refresh(BB.Get(), R); BB->MarkPackageDirty(); BT->BlackboardAsset = BB.Get(); R.Stage = TEXT("build_runtime_tree");
    if (!Build(BT.Get(), Shape, FName(*TargetActorKey), FName(*PatrolLocationKey), Radius, Wait, Precision, R)) { R.bKnown = false; Fail(R, TEXT("behavior_assets_runtime_build_failed"), TEXT("Actual native subtree prefix retained; no graph/save dispatched and no retry.")); return Encode(R); }
    R.Stage = TEXT("create_editor_graph"); auto* G = Cast<UBehaviorTreeGraph>(FBlueprintEditorUtils::CreateNewGraph(BT.Get(), TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass()));
    if (!Loaded(G) || G->GetOuter() != BT.Get() || G->GetClass() != UBehaviorTreeGraph::StaticClass() || G->Schema != UEdGraphSchema_BehaviorTree::StaticClass() || G->RootNodeClass != UBehaviorTreeGraphNode_Root::StaticClass()) { R.bKnown = false; Fail(R, TEXT("behavior_assets_graph_allocation_failed"), TEXT("Runtime subtree and graph prefix retained; exact public graph/schema/root allocation failed.")); return Encode(R); }
    BT->BTGraph = G; const UEdGraphSchema* Schema = G->GetSchema(); if (!Loaded(Schema) || Schema->GetClass() != UEdGraphSchema_BehaviorTree::StaticClass()) { R.bKnown = false; Fail(R, TEXT("behavior_assets_schema_unobservable"), TEXT("Native schema CDO unavailable after allocation; no fabricated graph lifecycle.")); return Encode(R); }
    Schema->CreateDefaultNodesForGraph(*G); R.Data->SetBoolField(TEXT("implicit_root_default_selection_possible"), true);
    auto* GraphRoot = G->Nodes.Num() == 1 ? Cast<UBehaviorTreeGraphNode_Root>(G->Nodes[0]) : nullptr;
    if (!Loaded(GraphRoot) || GraphRoot->GetClass() != UBehaviorTreeGraphNode_Root::StaticClass() || GraphRoot->GetOuter() != G || !GraphRoot->NodeGuid.IsValid() || GraphRoot->NodeInstance || GraphRoot->Pins.Num() != 1 || !GraphRoot->Pins[0] || GraphRoot->Pins[0]->Direction != EGPD_Output || GraphRoot->Pins[0]->GetOwningNodeUnchecked() != GraphRoot || GraphRoot->Pins[0]->LinkedTo.Num() != 0)
    { R.bKnown = false; Fail(R, TEXT("behavior_assets_default_root_invalid"), TEXT("Default graph-root prefix retained; exact one-root/public output-pin admission failed before SpawnMissingNodes.")); return Encode(R); }
    R.Data->SetStringField(TEXT("factory_graph_blackboard_path"), GraphRoot->BlackboardAsset ? GraphRoot->BlackboardAsset->GetPathName() : TEXT(""));
    // PostPlacedNewNode can temporarily select another loaded Blackboard. Restore the requested native asset before spawning the runtime body into editor nodes.
    GraphRoot->BlackboardAsset = BB.Get(); BT->BlackboardAsset = BB.Get();
    TStrongObjectPtr<UBTCompositeNode> ConstructionRoot(BT->RootNode.Get());
    if (!Loaded(ConstructionRoot.Get()) || G->IsLocked()) { R.bKnown = false; Fail(R, TEXT("behavior_assets_construction_lock_invalid"), TEXT("The native runtime root and initially unlocked graph must be retained before public construction locking; prefix remains without replay.")); return Encode(R); }
    // AddSubNode invokes UpdateAsset before SpawnMissingNodes attaches the final factory-root link.
    // Public locking suppresses that incomplete rebuild. Never clear bLockUpdates directly.
    R.Data->SetNumberField(TEXT("construction_lock_calls"), 1); G->LockUpdates(); R.Data->SetBoolField(TEXT("construction_graph_locked_retained"), G->IsLocked());
    if (!G->IsLocked()) { R.bKnown = false; Fail(R, TEXT("behavior_assets_construction_lock_failed"), TEXT("Public construction lock could not be observed; no spawn/unlock/save dispatched.")); return Encode(R); }
    G->OnCreated(); G->Initialize(); R.Data->SetBoolField(TEXT("editor_graph_created"), true); BT->MarkPackageDirty();
    auto RawBeforeUnlock = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("construction_raw_before_unlock"), RawBeforeUnlock);
    const bool RawBeforeComplete = RawConstruction(BT.Get(), G, ConstructionRoot.Get(), true, RawBeforeUnlock);
    const bool BeforeSemantic = Semantic(BT.Get(), BB.Get(), Shape, FName(*TargetActorKey), FName(*PatrolLocationKey), Radius, Wait, Precision);
    R.Data->SetBoolField(TEXT("construction_raw_before_unlock_complete"), RawBeforeComplete); R.Data->SetBoolField(TEXT("construction_runtime_semantic_preserved"), BeforeSemantic);
    R.Data->SetBoolField(TEXT("construction_graph_locked_retained"), G->IsLocked()); R.Data->SetBoolField(TEXT("midspawn_rebuild_suppressed_by_lock"), RawBeforeComplete && BeforeSemantic && G->IsLocked());
    if (!RawBeforeComplete || !BeforeSemantic || G->ModCounter == MAX_int32)
    { R.bKnown = false; Fail(R, TEXT("behavior_assets_construction_preunlock_unobservable"), TEXT("Bounded raw runtime/editor prefix retained before any unlock/rebuild. The still-locked incomplete or mismatched graph is not passed to UpdateAsset; no publication/save/retry.")); return Encode(R); }
    const int32 ConstructionCounter = G->ModCounter;
    R.Stage = TEXT("construction_unlock_update"); R.Data->SetNumberField(TEXT("construction_unlock_update_calls"), 1); R.Data->SetNumberField(TEXT("total_graph_unlock_update_calls"), 1); G->UnlockUpdates();
    R.Data->SetBoolField(TEXT("construction_graph_locked_retained"), G->IsLocked());
    auto RawAfterUnlock = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("construction_raw_after_unlock"), RawAfterUnlock);
    const bool RawAfterComplete = RawConstruction(BT.Get(), G, ConstructionRoot.Get(), false, RawAfterUnlock);
    const bool AfterSemantic = Semantic(BT.Get(), BB.Get(), Shape, FName(*TargetActorKey), FName(*PatrolLocationKey), Radius, Wait, Precision, true);
    const bool RawTopologyPreserved = RawAfterComplete && Text(RawBeforeUnlock->GetObjectField(TEXT("topology")).ToSharedRef()) == Text(RawAfterUnlock->GetObjectField(TEXT("topology")).ToSharedRef());
    R.Data->SetBoolField(TEXT("construction_raw_after_unlock_complete"), RawAfterComplete); R.Data->SetBoolField(TEXT("construction_raw_topology_preserved"), RawTopologyPreserved);
    R.Data->SetBoolField(TEXT("construction_runtime_semantic_preserved"), BeforeSemantic && AfterSemantic);
    R.Data->SetBoolField(TEXT("construction_single_is_set_logic_normalized"), Shape != TEXT("sequence_patrol_chase") && RawBeforeComplete && RawAfterComplete && BeforeSemantic && AfterSemantic);
    if (!RawAfterComplete) { R.bKnown = false; Fail(R, TEXT("behavior_assets_construction_postunlock_unobservable"), TEXT("Actual construction unlock/rebuild was dispatched; bounded post-unlock prefix is retained but incomplete, with no publication/save/replay.")); return Encode(R); }
    if (!RawTopologyPreserved || !AfterSemantic || G->ModCounter != ConstructionCounter + 1)
    { Fail(R, TEXT("behavior_assets_construction_postunlock_changed"), TEXT("Complete actual unlocked prefix differs from the validated runtime/graph identity or one expected rebuild; no publication/save/replay.")); return Encode(R); }
    R.Stage = TEXT("create_editor_graph");
    auto GraphBuilt = MakeShared<FJsonObject>(); if (!Read(BTPath, true, GraphBuilt, R, TEXT("behavior_tree_graph_built_native_result"))) { R.bKnown = false; Fail(R, TEXT("behavior_assets_graph_unobservable"), TEXT("Native SpawnMissingNodes output is not a bounded reciprocal runtime/editor subtree; prefix retained and no update/save.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("before_graph_update"), GraphBuilt); G->UpdateBlackboardChange(); G->UpdateAsset(UBehaviorTreeGraph::KeepRebuildCounter);
    auto BBFinal = MakeShared<FJsonObject>(), BTFinal = MakeShared<FJsonObject>(); const bool Observed = Read(BBPath, false, BBFinal, R, TEXT("blackboard_after_native_result")) && Read(BTPath, true, BTFinal, R, TEXT("behavior_tree_after_native_result"));
    if (!Observed) { R.bKnown = false; Fail(R, TEXT("behavior_assets_poststate_unobservable"), TEXT("Updated subtree/keys prefix retained; full native observers are incomplete, so no save.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("blackboard_after"), BBFinal); R.Data->SetObjectField(TEXT("behavior_tree_after"), BTFinal);
    const auto State = BTFinal->GetObjectField(TEXT("state")); const auto PriorState = GraphBuilt->GetObjectField(TEXT("state"));
    const bool SemanticOK = Semantic(BT.Get(), BB.Get(), Shape, FName(*TargetActorKey), FName(*PatrolLocationKey), Radius, Wait, Precision, true);
    const bool GraphOK = Text(State->GetObjectField(TEXT("topology")).ToSharedRef()) == Text(PriorState->GetObjectField(TEXT("topology")).ToSharedRef());
    auto SourceAfter = MakeShared<FJsonObject>(), DirtyAfter = MakeShared<FJsonObject>(), BBDiskAfter = MakeShared<FJsonObject>(), BTDiskAfter = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> NewCDOs;
    const bool SourceObserved = Sources(SourceAfter), DirtyObserved = Dirty(BBPackage, BTPackage, DirtyAfter), BBFileObserved = Disk(BBPackage, BBDiskAfter), BTFileObserved = Disk(BTPackage, BTDiskAfter);
    const bool Complete = SourceObserved && DirtyObserved && BBFileObserved && BTFileObserved;
    const bool Preserved = Complete && ProtectSources(SourceBefore, SourceAfter, true, NewCDOs) && Text(DirtyBefore) == Text(DirtyAfter) && Text(BBDisk) == Text(BBDiskAfter) && Text(BTDisk) == Text(BTDiskAfter);
    R.Data->SetObjectField(TEXT("native_sources_after"), SourceAfter); R.Data->SetObjectField(TEXT("other_package_dirty_after"), DirtyAfter); R.Data->SetArrayField(TEXT("implicit_native_cdo_creations_observed"), NewCDOs);
    R.Data->SetObjectField(TEXT("blackboard_disk_after_build"), BBDiskAfter); R.Data->SetObjectField(TEXT("behavior_tree_disk_after_build"), BTDiskAfter); R.Data->SetBoolField(TEXT("build_observations_complete"), Complete);
    R.Data->SetBoolField(TEXT("runtime_subtree_verified"), SemanticOK); R.Data->SetBoolField(TEXT("editor_subtree_verified"), GraphOK); R.Data->SetBoolField(TEXT("source_and_disk_preserved_before_save"), Preserved);
    if (!Complete) { R.bKnown = false; Fail(R, TEXT("behavior_assets_poststate_unobservable"), TEXT("Created target/subtree states are retained but protected source/dirty/disk observations are incomplete; publication/save suppressed.")); return Encode(R); }
    if (!SemanticOK || !GraphOK || !Preserved) { Fail(R, TEXT("behavior_assets_build_postcondition_failed"), TEXT("Both assets/subtree are retained and observed, but exact semantic/editor/source preservation failed; no save or retry.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("asset_registry_publication_dispatched"), true); FAssetRegistryModule::AssetCreated(BB.Get()); R.Data->SetNumberField(TEXT("asset_registry_publication_count"), 1);
    FAssetRegistryModule::AssetCreated(BT.Get()); R.Data->SetNumberField(TEXT("asset_registry_publication_count"), 2);
    auto PublishedBB = MakeShared<FJsonObject>(), PublishedBT = MakeShared<FJsonObject>();
    if (!Read(BBPath, false, PublishedBB, R, TEXT("blackboard_after_publication_native_result")) || !Read(BTPath, true, PublishedBT, R, TEXT("behavior_tree_after_publication_native_result")))
    { R.bKnown = false; Fail(R, TEXT("behavior_assets_publication_unobservable"), TEXT("Both published assets retained; callback-bearing registry publication could not be observed completely, so no save.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("blackboard_after_publication"), PublishedBB); R.Data->SetObjectField(TEXT("behavior_tree_after_publication"), PublishedBT);
    auto PublishedSources = MakeShared<FJsonObject>(), PublishedDirty = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> PublicationCDOs;
    const bool PublishedSourceObserved = Sources(PublishedSources), PublishedDirtyObserved = Dirty(BBPackage, BTPackage, PublishedDirty), PublicationComplete = PublishedSourceObserved && PublishedDirtyObserved;
    const bool PublicationPreserved = PublicationComplete && Text(BBFinal) == Text(PublishedBB) && Text(BTFinal) == Text(PublishedBT) && ProtectSources(SourceAfter, PublishedSources, false, PublicationCDOs) && Text(DirtyBefore) == Text(PublishedDirty);
    R.Data->SetObjectField(TEXT("native_sources_after_publication"), PublishedSources); R.Data->SetObjectField(TEXT("other_package_dirty_after_publication"), PublishedDirty); R.Data->SetBoolField(TEXT("publication_observations_complete"), PublicationComplete);
    R.Data->SetBoolField(TEXT("registry_publication_preserved"), PublicationPreserved);
    if (!PublicationComplete) { R.bKnown = false; Fail(R, TEXT("behavior_assets_publication_unobservable"), TEXT("Published target states are retained but protected source/dirty observations are incomplete; save suppressed.")); return Encode(R); }
    if (!PublicationPreserved) { Fail(R, TEXT("behavior_assets_publication_changed"), TEXT("Actual publication prefix retained, but target/native/default/external dirty state changed; save suppressed without retry.")); return Encode(R); }
    R.bPost = true; R.Data->SetStringField(TEXT("root_node_class"), State->GetStringField(TEXT("root_node_class"))); R.Data->SetStringField(TEXT("root_node_name"), State->GetStringField(TEXT("root_node_name")));
    R.Data->SetNumberField(TEXT("root_child_count"), State->GetNumberField(TEXT("root_child_count"))); R.Data->SetNumberField(TEXT("editor_graph_node_count"), State->GetNumberField(TEXT("editor_graph_node_count")));
    R.Data->SetNumberField(TEXT("node_count"), State->GetNumberField(TEXT("node_count"))); R.Data->SetNumberField(TEXT("runtime_object_count"), State->GetNumberField(TEXT("runtime_object_count"))); R.Data->SetBoolField(TEXT("traversal_truncated"), false);
    R.Data->SetArrayField(TEXT("behavior_tree_children"), State->GetArrayField(TEXT("behavior_tree_children"))); R.Data->SetArrayField(TEXT("blackboard_keys"), BBFinal->GetObjectField(TEXT("state"))->GetArrayField(TEXT("own_keys")));
    R.Data->SetBoolField(TEXT("readback_confirmed"), true); R.Data->SetBoolField(TEXT("mcp_dependency_audit_pass"), true); R.Data->SetArrayField(TEXT("mcp_dependency_audit_issues"), {});
    if (bSaveAssets)
    {
        TArray<TSharedPtr<FJsonValue>> Calls; if (!SaveOne(BB.Get(), false, BBPackage, BTPackage, BBFinal, SourceAfter, DirtyBefore, Calls, R)) { return Encode(R); }
        auto TreeBeforeSave = MakeShared<FJsonObject>(); if (!Read(BTPath, true, TreeBeforeSave, R, TEXT("behavior_tree_before_save_native_result"))) { R.bKnown = false; R.bPost = false; Fail(R, TEXT("behavior_assets_tree_save_prestate_unobservable"), TEXT("Blackboard save prefix retained; Tree save suppressed because exact current observer failed.")); return Encode(R); }
        R.Data->SetObjectField(TEXT("behavior_tree_before_save"), TreeBeforeSave); if (!SaveOne(BT.Get(), true, BBPackage, BTPackage, TreeBeforeSave, SourceAfter, DirtyBefore, Calls, R)) { return Encode(R); }
        R.bSaved |= R.Data->GetBoolField(TEXT("blackboard_saved")) && R.Data->GetBoolField(TEXT("behavior_tree_saved"));
        if (!Semantic(BT.Get(), BB.Get(), Shape, FName(*TargetActorKey), FName(*PatrolLocationKey), Radius, Wait, Precision, true)) { R.bPost = false; Fail(R, TEXT("behavior_assets_saved_subtree_changed"), TEXT("Actual save/unlock completed but intended native subtree did not survive; saved result retained, no replay.")); return Encode(R); }
    }
    R.Stage = TEXT("complete"); R.bSuccess = true; return Encode(R);
}
}
