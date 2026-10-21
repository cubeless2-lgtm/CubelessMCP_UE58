// Original BehaviorTree Create/Set/Inspect through public Editor APIs; no gameplay helper.
#include "Compatibility/UE58/CubelessBehaviorTreeApi.h"
#include "Compatibility/UE58/CubelessBlackboardApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AISystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_CompositeDecorator.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTreeDecoratorGraph.h"
#include "BehaviorTreeEditorTypes.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "GameFramework/Actor.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTree/Tasks/BTTask_MoveDirectlyToward.h"
#include "BehaviorTree/Tasks/BTTask_RotateToFaceBBEntry.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "Internationalization/Text.h"
#include "Misc/CoreMisc.h"
#include "UObject/GarbageCollection.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::BehaviorTree
{
namespace
{
constexpr int32 MaxNodes = 1000, MaxDepth = 64, MaxPins = 8192, MaxLinks = 16384, MaxReferences = 128;
constexpr int32 MaxProperties = 128, MaxText = 262144, MaxPackages = 4096;
constexpr int64 MaxDiskBytes = 16 * 1024 * 1024;
struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
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
    if (R.bKnown) { R.Data->SetBoolField(TEXT("changed"), R.bChanged); } else { R.Data->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    R.Data->SetBoolField(TEXT("save_requested"), R.bSaveRequested); R.Data->SetBoolField(TEXT("saved"), R.bSaved);
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), R.Operation); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetBoolField(TEXT("success"), R.bSuccess); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown); J->SetBoolField(TEXT("postcondition_passed"), R.bPost);
    J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false); J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
void Init(FResult& R, const TCHAR* Operation, bool bSave)
{
    R.Operation = Operation; R.bSaveRequested = bSave;
    for (const TCHAR* K : { TEXT("asset_load_dispatched"), TEXT("implicit_postload_possible"), TEXT("graph_creation_attempted"), TEXT("graph_created"),
        TEXT("graph_reused"), TEXT("blackboard_assignment_attempted"), TEXT("asset_registry_publication_dispatched"), TEXT("asset_saved"),
        TEXT("implicit_engine_work_measured"), TEXT("runtime_execution_verified"), TEXT("custom_runtime_nodes_supported"), TEXT("nested_decorator_graphs_supported") }) { R.Data->SetBoolField(K, false); }
    for (const TCHAR* K : { TEXT("asset_load_count"), TEXT("delegated_blackboard_inspection_count"), TEXT("direct_update_blackboard_calls"),
        TEXT("direct_update_asset_calls"), TEXT("direct_initialize_root_calls"), TEXT("unlock_update_calls"), TEXT("save_attempt_count") }) { R.Data->SetNumberField(K, 0); }
    R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetBoolField(TEXT("save_asset_requested"), bSave);
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: exported AIModule/BehaviorTreeEditor/AIGraph/UnrealEd and EditorAssetLibrary; no official AI Toolset delegate or ServerRPC"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("native BehaviorTree asset/editor graph and stock native AIModule hierarchy; explicit resolution may run PostLoad; default root can temporarily select a configured/already-loaded Blackboard; Apply and UnlockUpdates rebuild/refresh native runtime caches; no gameplay execution, compile, private factory, runtime class or automatic rollback"));
    R.Data->SetStringField(TEXT("changed_observation_scope"), TEXT("bounded actual graph/runtime/reflected states, existing native defaults, referenced Blackboards, target disk and other loaded-package dirty states after initial resolution; callback/world/loader work is not fully instrumented"));
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"), O ? O->GetPathName() : TEXT("")); J->SetStringField(TEXT("class"), O ? O->GetClass()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("outer"), O && O->GetOuter() ? O->GetOuter()->GetPathName() : TEXT("")); J->SetStringField(TEXT("object_flags"), O ? FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags())) : TEXT("0")); return J;
}
bool Native(UClass* C, const TCHAR* Package, bool bAbstract = false)
{ return Loaded(C) && C->HasAnyClassFlags(CLASS_Native) && !C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
    && (bAbstract || !C->HasAnyClassFlags(CLASS_Abstract)) && C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)
    && C->GetOutermost()->GetName() == Package && !C->GetOutermost()->IsDirty(); }
bool RDCurrent(const TCHAR* Stage);
bool RDField(UObject* O, FProperty* P, FName Name);
bool Properties(UObject* O, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(O)) { return false; } int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; const FName FieldName = P->GetFName(); if (Skip.Contains(FieldName)) { continue; } if (!RDField(O, P, FieldName)) { return false; }
        if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64) { return false; }
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I)
        {
            FString S; const void* V = P->ContainerPtrToValuePtr<void>(O, I); const bool Exported = P->ExportText_Direct(S, V, V, O, PPF_None); if (!RDField(O, P, FieldName) || !Exported) { return false; }
            Chars += S.Len(); if (Chars > MaxText) { return false; } Values.Add(MakeShared<FJsonValueString>(S));
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName());
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(P->GetPropertyFlags()))); Row->SetNumberField(TEXT("array_dim"), P->ArrayDim); Row->SetArrayField(TEXT("values"), Values); J->SetObjectField(P->GetName(), Row);
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
bool Dirty(const FString& Excluded, TSharedRef<FJsonObject>& J)
{
    int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == Excluded) { continue; }
        if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetPathName(), It->IsDirty()); } return true;
}
bool EditorState(FResult& R)
{ return (IsEditorGameThread() && !IsPlaySessionActive()) || Fail(R, TEXT("behavior_tree_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); }
bool ExactPath(const FString& Input, bool bMutable, FString& Path, FResult& R)
{
    Path = FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd(); Path.TrimQuotesInline(); Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!Path.Contains(TEXT("."))) { Path += TEXT(".") + FPackageName::GetLongPackageAssetName(Path); }
    FString PackageName, ObjectName;
    if (Path.Len() > 1024 || !FPackageName::IsValidObjectPath(Path) || !Path.Split(TEXT("."), &PackageName, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
        || ObjectName != FPackageName::GetLongPackageAssetName(PackageName) || !(PackageName.StartsWith(TEXT("/Game/")) || (!bMutable && PackageName.StartsWith(TEXT("/Engine/")))))
    { return Fail(R, TEXT("behavior_tree_asset_path_invalid"), TEXT("Use an exact /Game asset object path or package; source/read-only paths also permit /Engine. Other mounts/subobjects remain unsupported.")); } return true;
}
UObject* Resolve(const FString& Path, FResult& R)
{
    UObject* O = FindObject<UObject>(nullptr, *Path);
    if (!O) { R.Data->SetBoolField(TEXT("asset_load_dispatched"), true); R.Data->SetBoolField(TEXT("implicit_postload_possible"), true);
        R.Data->SetNumberField(TEXT("asset_load_count"), R.Data->GetNumberField(TEXT("asset_load_count")) + 1); R.Data->SetStringField(TEXT("last_explicit_load_path"), Path); O = LoadObject<UObject>(nullptr, *Path); }
    if (!Loaded(O) || O->GetPathName() != Path || O->GetOuter() != O->GetOutermost() || O->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        || O->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor)) { Fail(R, TEXT("behavior_tree_native_asset_required"), TEXT("Requires a readable ordinary exact native asset; generated/custom/transient/package variants are incomplete.")); return nullptr; } return O;
}
bool SimpleName(const FString& S)
{ if (S.IsEmpty() || S.Len() > 128 || FName(*S).IsNone()) { return false; } for (TCHAR C : S) { if (C < 33 || C == 127 || FString(INVALID_OBJECTNAME_CHARACTERS).Contains(FString::Chr(C))) { return false; } } return true; }
struct FContext
{
    TArray<TStrongObjectPtr<UBlackboardData>> Blackboards;
};
bool AddReference(UBlackboardData* B, FContext& C, FResult& R)
{
    if (!B) { return true; }
    for (const auto& Existing : C.Blackboards) { if (Existing.Get() == B) { return true; } }
    FString Path; if (!Loaded(B) || B->GetClass() != UBlackboardData::StaticClass() || C.Blackboards.Num() >= MaxReferences || !ExactPath(B->GetPathName(), false, Path, R))
    { return Fail(R, TEXT("behavior_tree_blackboard_reference_unsupported"), TEXT("All current/requested/default-selection Blackboards must be bounded exact native /Game or /Engine assets.")); }
    C.Blackboards.Emplace(B); return true;
}
bool GatherReferences(UBehaviorTree* B, UBlackboardData* Requested, bool bMayCreateGraph, FContext& C, FResult& R)
{
    if (!AddReference(Requested, C, R) || (B && !AddReference(B->BlackboardAsset, C, R))) { return false; }
    if (B && B->BTGraph)
    {
        auto* G = Cast<UBehaviorTreeGraph>(B->BTGraph); if (!Loaded(G) || G->Nodes.Num() > MaxNodes) { return Fail(R, TEXT("behavior_tree_graph_unsupported"), TEXT("Existing editor graph is unreadable or exceeds the bound.")); }
        for (UEdGraphNode* N : G->Nodes) { if (auto* Root = Cast<UBehaviorTreeGraphNode_Root>(N)) { if (!AddReference(Root->BlackboardAsset, C, R)) { return false; } } }
    }
    if (bMayCreateGraph)
    {
        // The Engine root callback can choose the first already-loaded Blackboard. Retain all candidates, without loading any here.
        for (TObjectIterator<UBlackboardData> It; It; ++It) { if (It->HasAnyFlags(RF_ClassDefaultObject)) { continue; } if (!AddReference(*It, C, R)) { return false; } }
    }
    return true;
}
bool BlackboardObservation(UBlackboardData* B, TSharedRef<FJsonObject>& J, FResult& R)
{
    R.Data->SetNumberField(TEXT("delegated_blackboard_inspection_count"), R.Data->GetNumberField(TEXT("delegated_blackboard_inspection_count")) + 1);
    TSharedPtr<FJsonObject> Raw; const FString Encoded = Blackboard::InspectAsset(B->GetPathName());
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Encoded), Raw) || !Raw.IsValid() || !Raw->GetBoolField(TEXT("success")) || !Raw->GetBoolField(TEXT("change_state_known"))
        || Raw->GetBoolField(TEXT("mutation_attempted")) || Raw->GetBoolField(TEXT("changed"))) { return false; }
    const auto Data = Raw->GetObjectField(TEXT("data")); const auto After = Data->GetObjectField(TEXT("after"));
    // Global dirty-state protection excludes the selected Tree package; avoid double-counting that package through nested Blackboard inspectors.
    for (const TCHAR* K : { TEXT("state"), TEXT("native_sources"), TEXT("disk"), TEXT("parent_states") }) { J->SetObjectField(K, After->GetObjectField(K)); } return true;
}
bool RequirePersistedBlackboard(UBlackboardData* B, const TSharedRef<FJsonObject>& References, FResult& R)
{
    if (!R.bSaveRequested || !B) { return true; }
    const TSharedPtr<FJsonValue> Found = References->TryGetField(B->GetPathName());
    if (!Found.IsValid() || Found->Type != EJson::Object) { return Fail(R, TEXT("behavior_tree_blackboard_persistence_unobservable"), TEXT("Selected Blackboard persistence was not observable before the requested save; no save dispatched.")); }
    const auto Reference = Found->AsObject(); auto Checked = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("referenced_blackboard_persistence"), Checked);
    auto Check = [&](const FString& Path, const TSharedRef<FJsonObject>& Row)
    {
        const auto DiskState = Row->GetObjectField(TEXT("disk")); Checked->SetObjectField(Path, DiskState);
        if (!DiskState->GetBoolField(TEXT("exists")) || DiskState->GetNumberField(TEXT("bytes")) <= 0)
        { R.Data->SetStringField(TEXT("unpersisted_blackboard_path"), Path); return Fail(R, TEXT("behavior_tree_blackboard_not_persisted"), TEXT("Requested target save requires the selected Blackboard and its parents to already have nonempty persisted files; no tree write/save is retried.")); }
        return true;
    };
    if (!Check(B->GetPathName(), Reference.ToSharedRef())) { return false; }
    const auto Parents = Reference->GetObjectField(TEXT("parent_states"));
    for (const auto& Pair : Parents->Values) { if (!Check(FString(*Pair.Key), Pair.Value->AsObject().ToSharedRef())) { return false; } }
    R.Data->SetBoolField(TEXT("referenced_blackboard_persisted"), true); return true;
}
bool NativeSources(const TSet<UClass*>& Classes, TSharedRef<FJsonObject>& J)
{
    TArray<UClass*> Ordered = Classes.Array(); Ordered.Sort([](const UClass& A, const UClass& B) { return A.GetPathName() < B.GetPathName(); });
    for (UClass* C : Ordered)
    {
        auto Row = Identity(C); J->SetObjectField(C->GetPathName(), Row);
        if (!(Native(C, TEXT("/Script/AIModule"), true) || Native(C, TEXT("/Script/BehaviorTreeEditor"), true) || Native(C, TEXT("/Script/AIGraph"), true)))
        { J->SetStringField(TEXT("failed_class"), C->GetPathName()); return false; }
        Row->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags()))); Row->SetBoolField(TEXT("class_editor_only"), C->IsEditorOnly()); Row->SetBoolField(TEXT("package_dirty"), C->GetOutermost()->IsDirty());
        UObject* CDO = C->GetDefaultObject(false); Row->SetBoolField(TEXT("cdo_exists"), CDO != nullptr);
        if (CDO)
        {
            if (!Loaded(CDO) || CDO->GetClass() != C || CDO->GetOuter() != C->GetOuter() || !CDO->HasAnyFlags(RF_ClassDefaultObject)) { return false; }
            auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); Row->SetObjectField(TEXT("existing_cdo_properties"), P); if (!Properties(CDO, {}, P)) { return false; }
        }
    }
    return true;
}
TSet<UClass*> FixedClasses()
{ return { UBehaviorTree::StaticClass(), UBTNode::StaticClass(), UBlackboardData::StaticClass(), UAISystem::StaticClass(), UBehaviorTreeGraph::StaticClass(),
    UBehaviorTreeGraphNode::StaticClass(), UBehaviorTreeGraphNode_Root::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass(), UAIGraph::StaticClass(), UAIGraphNode::StaticClass() }; }
bool ProtectNative(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, bool bCreatedGraph, TArray<TSharedPtr<FJsonValue>>& NewCDOs)
{
    if (Before->Values.Num() != After->Values.Num()) { return false; }
    for (const auto& Pair : Before->Values)
    {
        const TSharedPtr<FJsonValue> Found = After->TryGetField(Pair.Key.ToView()); if (!Found.IsValid() || Found->Type != EJson::Object) { return false; }
        const auto A = Pair.Value->AsObject(), B = Found->AsObject(); auto Expected = MakeShared<FJsonObject>(); for (const auto& Field : A->Values) { Expected->SetField(Field.Key, Field.Value); }
        if (!A->GetBoolField(TEXT("cdo_exists")) && B->GetBoolField(TEXT("cdo_exists")))
        {
            if (!bCreatedGraph) { return false; } Expected->SetBoolField(TEXT("cdo_exists"), true); Expected->SetObjectField(TEXT("existing_cdo"), B->GetObjectField(TEXT("existing_cdo")));
            Expected->SetObjectField(TEXT("existing_cdo_properties"), B->GetObjectField(TEXT("existing_cdo_properties"))); NewCDOs.Add(MakeShared<FJsonValueString>(FString(*Pair.Key)));
        }
        if (Text(Expected) != Text(B.ToSharedRef())) { return false; }
    }
    return true;
}
struct FTraversal
{
    UBehaviorTree* Tree; int32 NodeCount = 0; TSet<const UBTNode*> Seen; TSet<UClass*> Classes;
    TSharedRef<FJsonObject> Objects = MakeShared<FJsonObject>(), Topology = MakeShared<FJsonObject>();
};
bool RuntimeNode(UBTNode* N, FTraversal& T, int32 Depth, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(N) || N->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || N->GetOuter() != T.Tree || !Native(N->GetClass(), TEXT("/Script/AIModule")) || N->GetClass()->IsEditorOnly() || T.Seen.Contains(N) || T.Seen.Num() >= MaxNodes || Depth > MaxDepth) { return false; }
    T.Seen.Add(N); T.Classes.Add(N->GetClass()); J = Identity(N); J->SetStringField(TEXT("node_class"), N->GetClass()->GetName()); J->SetStringField(TEXT("node_name"), N->NodeName);
    J->SetNumberField(TEXT("execution_index"), N->GetExecutionIndex()); J->SetNumberField(TEXT("memory_offset"), N->GetMemoryOffset()); J->SetNumberField(TEXT("tree_depth"), N->GetTreeDepth());
    auto P = MakeShared<FJsonObject>(); if (!Properties(N, {}, P)) { return false; } J->SetObjectField(TEXT("properties"), P); T.Objects->SetObjectField(N->GetPathName(), J);
    auto Stable = MakeShared<FJsonObject>(); Stable->SetStringField(TEXT("path"), N->GetPathName()); Stable->SetStringField(TEXT("class"), N->GetClass()->GetPathName()); Stable->SetStringField(TEXT("outer"), N->GetOuter()->GetPathName()); Stable->SetStringField(TEXT("node_name"), N->NodeName);
    TArray<TSharedPtr<FJsonValue>> Children, ChildTopology, Services;
    const auto AddServices = [&](const auto& List)
    {
        if (List.Num() > MaxNodes) { return false; }
        for (auto Service : List) { auto S = MakeShared<FJsonObject>(); if (!RuntimeNode(Service, T, Depth + 1, S)) { return false; }
            S->SetStringField(TEXT("service_class"), Service->GetClass()->GetName()); S->SetStringField(TEXT("service_class_path"), Service->GetClass()->GetPathName()); S->SetStringField(TEXT("service_name"), Service->NodeName); Services.Add(MakeShared<FJsonValueObject>(S)); } return true;
    };
    if (auto* Composite = Cast<UBTCompositeNode>(N))
    {
        ++T.NodeCount; J->SetStringField(TEXT("node_kind"), TEXT("composite")); if (Composite->Children.Num() > MaxNodes || !AddServices(Composite->Services)) { return false; }
        for (const FBTCompositeChild& Child : Composite->Children)
        {
            if ((!Child.ChildComposite && !Child.ChildTask) || (Child.ChildComposite && Child.ChildTask) || Child.Decorators.Num() > MaxNodes || Child.DecoratorOps.Num() > MaxNodes) { return false; }
            UBTNode* Main = Child.ChildComposite ? static_cast<UBTNode*>(Child.ChildComposite.Get()) : static_cast<UBTNode*>(Child.ChildTask.Get());
            auto Row = MakeShared<FJsonObject>(), ChildStable = MakeShared<FJsonObject>(); if (!RuntimeNode(Main, T, Depth + 1, Row)) { return false; }
            TArray<TSharedPtr<FJsonValue>> Decorators, Ops; ChildStable->SetStringField(TEXT("node"), Main->GetPathName());
            for (auto Decorator : Child.Decorators)
            {
                auto D = MakeShared<FJsonObject>(); if (!RuntimeNode(Decorator, T, Depth + 1, D)) { return false; }
                D->SetStringField(TEXT("decorator_class"), Decorator->GetClass()->GetName()); D->SetStringField(TEXT("decorator_name"), Decorator->NodeName); D->SetStringField(TEXT("flow_abort_mode"), UBehaviorTreeTypes::DescribeFlowAbortMode(Decorator->GetFlowAbortMode())); Decorators.Add(MakeShared<FJsonValueObject>(D));
            }
            for (const auto& Op : Child.DecoratorOps) { auto O = MakeShared<FJsonObject>(); O->SetNumberField(TEXT("operation"), static_cast<int32>(Op.Operation)); O->SetNumberField(TEXT("number"), Op.Number); Ops.Add(MakeShared<FJsonValueObject>(O)); }
            Row->SetArrayField(TEXT("decorators"), Decorators); Row->SetNumberField(TEXT("decorator_count"), Decorators.Num()); Row->SetArrayField(TEXT("decorator_ops"), Ops);
            TArray<TSharedPtr<FJsonValue>> DecoratorPaths; for (auto D : Child.Decorators) { DecoratorPaths.Add(MakeShared<FJsonValueString>(D->GetPathName())); }
            ChildStable->SetArrayField(TEXT("decorators"), DecoratorPaths); ChildStable->SetArrayField(TEXT("decorator_ops"), Ops); ChildTopology.Add(MakeShared<FJsonValueObject>(ChildStable)); Children.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    else if (auto* Task = Cast<UBTTaskNode>(N)) { ++T.NodeCount; J->SetStringField(TEXT("node_kind"), TEXT("task")); if (!AddServices(Task->Services)) { return false; } }
    else if (Cast<UBTDecorator>(N)) { J->SetStringField(TEXT("node_kind"), TEXT("decorator")); }
    else if (Cast<UBTService>(N)) { J->SetStringField(TEXT("node_kind"), TEXT("service")); }
    else { return false; }
    if (auto* KeyTask = Cast<UBTTask_BlackboardBase>(N)) { J->SetStringField(TEXT("selected_blackboard_key"), KeyTask->GetSelectedBlackboardKey().ToString()); }
    if (auto* KeyDecorator = Cast<UBTDecorator_BlackboardBase>(N)) { J->SetStringField(TEXT("selected_blackboard_key"), KeyDecorator->GetSelectedBlackboardKey().ToString()); }
    if (auto* Move = Cast<UBTTask_MoveTo>(N)) { J->SetBoolField(TEXT("use_pathfinding"), Move->bUsePathfinding); J->SetStringField(TEXT("allow_partial_path"), Move->bAllowPartialPath.ToString()); J->SetStringField(TEXT("require_navigable_end_location"), Move->bRequireNavigableEndLocation.ToString()); J->SetStringField(TEXT("project_goal_location"), Move->bProjectGoalLocation.ToString()); }
    J->SetArrayField(TEXT("children"), Children); J->SetArrayField(TEXT("services"), Services); J->SetNumberField(TEXT("child_count"), Children.Num()); J->SetNumberField(TEXT("service_count"), Services.Num());
    Stable->SetArrayField(TEXT("children"), ChildTopology); TArray<TSharedPtr<FJsonValue>> ServicePaths; for (const auto& V : Services) { ServicePaths.Add(MakeShared<FJsonValueString>(V->AsObject()->GetStringField(TEXT("path")))); }
    Stable->SetArrayField(TEXT("services"), ServicePaths); T.Topology->SetObjectField(N->GetPathName(), Stable); return true;
}
// RD/TQ/UG: bounded native BT topology, Composite expressions, and native patrol task selectors.
// No new authoring API. Only the original Blackboard callback chain is dispatched.
struct FRDComposite
{
    struct FField
    {
        FName Name; FProperty* Pointer = nullptr; UStruct* Owner = nullptr;
        FFieldClass* Class = nullptr; int32 Offset = 0, Size = 0, Dim = 0; uint64 Flags = 0;
        FString Before, Planned; bool Selector = false, Blackboard = false, Execution = false;
        UScriptStruct* Struct = nullptr; UClass* ObjectClass = nullptr; UClass* MetaClass = nullptr;
    };
    struct FObject
    {
        TStrongObjectPtr<UObject> Hold; UClass* Class = nullptr; UObject* Outer = nullptr;
        FString Path; TArray<FField> Fields;
    };
    struct FPin
    {
        FGuid Id; FName Name; EEdGraphPinDirection Direction; FEdGraphPinType Type;
        FString Raw, Auto; FText Text, Friendly; UObject* Default = nullptr;
        bool Hidden = false, NotConnectable = false, DefaultReadOnly = false;
        TArray<FString> Links;
    };
    struct FNode
    {
        UEdGraphNode* Pointer = nullptr; UEdGraph* Graph = nullptr; FGuid Id;
        int32 X = 0, Y = 0; TArray<FPin> Pins;
    };
    struct FCompositeRole
    {
        UBehaviorTreeGraphNode_CompositeDecorator* Node=nullptr; UBehaviorTreeDecoratorGraph* Graph=nullptr;
        UBehaviorTreeGraphNode* Host=nullptr; TArray<UEdGraphNode*> Members;
        TArray<UBTDecorator*> Tests; TArray<FBTDecoratorLogic> Logic;
        uint16 FirstBefore=0, LastBefore=0, First=0, Last=0; uint8 Child=0;
        UBTCompositeNode* ParentBefore=nullptr; UBTCompositeNode* Parent=nullptr;
    };
    struct FMainRole
    {
        UBehaviorTreeGraphNode* Node=nullptr; UBTNode* Instance=nullptr;
        TArray<UBehaviorTreeGraphNode*> Children;
        TArray<UBTDecorator*> Tests; TArray<FBTDecoratorLogic> Logic;
    };
    // UpdateAsset's editor projection is distinct from SetDecoratorData's runtime parent.
    struct FEditorExecutionRole
    {
        UBehaviorTreeGraphNode* Node=nullptr; FString Path, ParentPath, PlannedParentPath;
        UAIGraphNode* BeforeParent=nullptr; UAIGraphNode* PlannedParent=nullptr; UObject* Instance=nullptr;
        bool BeforeRoot=false, PlannedRoot=false, BeforeObserver=false;
        FString ClassName, Deprecated, Error; bool ErrorUpdated=false, CompositeRole=false;
    };
    TArray<FEditorExecutionRole> EditorExecutionRoles;
    // UG opts in only within the existing Composite contract; plain generic trees keep their route.
    bool PatrolMixed=false;
    struct FPlainDecorator { UBehaviorTreeGraphNode_Decorator* Node=nullptr; UBehaviorTreeGraphNode* Host=nullptr; UBTDecorator* Instance=nullptr; };
    TArray<FPlainDecorator> PlainDecorators;
    static bool PatrolTask(const UClass* C)
    { return C==UBTTask_MoveDirectlyToward::StaticClass() || C==UBTTask_RotateToFaceBBEntry::StaticClass(); }
    static bool MainTask(const UClass* C) { return C==UBTTask_Wait::StaticClass() || PatrolTask(C); }
    static bool SelectorOwner(const UClass* C) { return C==UBTDecorator_Blackboard::StaticClass() || PatrolTask(C); }
    static bool KeyClass(const UClass* C)
    { return C==UBlackboardKeyType_Bool::StaticClass() || C==UBlackboardKeyType_Object::StaticClass() || C==UBlackboardKeyType_Vector::StaticClass() || C==UBlackboardKeyType_Name::StaticClass(); }
    bool ReadEditorParent(UBehaviorTreeGraphNode* Node,const FString& Path,const TCHAR* Stage,UAIGraphNode*& Parent)
    {
        const FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==Node;});
        const FField* F=O?O->Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("ParentNode");}):nullptr;
        if (!O || !F || !FieldCurrent(*O,*F) || F->Owner!=UAIGraphNode::StaticClass() || F->Dim!=1 || F->Size!=sizeof(TObjectPtr<UAIGraphNode>))
        { return Stop(Stage,Path,TEXT("editor_ParentNode.binding"),TEXT("captured AIGraphNode native object field"),TEXT("changed")); }
        auto* P=CastField<FObjectProperty>(F->Pointer);
        if (!P || P->PropertyClass!=UAIGraphNode::StaticClass() || !Node->ParentNode.IsResolved())
        { return Stop(Stage,Path,TEXT("editor_ParentNode.slot"),TEXT("resolved nullable native AIGraphNode field"),TEXT("changed/unresolved")); }
        Parent=Node->ParentNode.Get(); return Hold(Parent);
    }
    static TSharedRef<FJsonObject> EditorExecutionJson(const FEditorExecutionRole& E,bool Planned)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("object"),E.Path);
        J->SetStringField(TEXT("editor_parent"),Planned?E.PlannedParentPath:E.ParentPath);
        J->SetBoolField(TEXT("root_level"),Planned?E.PlannedRoot:E.BeforeRoot);
        J->SetBoolField(TEXT("observer_error"),Planned&&E.CompositeRole?false:E.BeforeObserver);
        J->SetStringField(TEXT("error_message"),E.Error); return J;
    }
    bool EditorExecutionCurrent(const TCHAR* Stage,TArray<TSharedPtr<FJsonValue>>* Receipt=nullptr)
    {
        for (const auto& E : EditorExecutionRoles)
        {
            UAIGraphNode* Parent=nullptr; if (!ReadEditorParent(E.Node,E.Path,Stage,Parent)) { return false; }
            if (!E.Node->NodeInstance.IsResolved() || E.Node->NodeInstance.Get()!=E.Instance)
            { return Stop(Stage,E.Path,TEXT("editor_NodeInstance"),TEXT("captured resolved runtime/null instance"),TEXT("changed/unresolved")); }
            UAIGraphNode* Expected=ExecutionRebuilt?E.PlannedParent:E.BeforeParent;
            const bool RootValue=ExecutionRebuilt?E.PlannedRoot:E.BeforeRoot;
            const bool Observer=ExecutionRebuilt&&E.CompositeRole?false:E.BeforeObserver;
            if (Parent!=Expected || static_cast<bool>(E.Node->bRootLevel)!=RootValue || static_cast<bool>(E.Node->bHasObserverError)!=Observer)
            {
                auto Actual=MakeShared<FJsonObject>(); Actual->SetStringField(TEXT("editor_parent"),Parent?Parent->GetPathName():FString());
                Actual->SetBoolField(TEXT("root_level"),static_cast<bool>(E.Node->bRootLevel)); Actual->SetBoolField(TEXT("observer_error"),static_cast<bool>(E.Node->bHasObserverError));
                return Stop(Stage,E.Path,TEXT("editor_execution_role"),Text(EditorExecutionJson(E,ExecutionRebuilt)),Text(Actual));
            }
            // These public logical copies are not physical weak-class identity invariants.
            const FString ClassName=E.Node->ClassData.GetClassName(), Deprecated=E.Node->ClassData.GetDeprecatedMessage();
            if (!ClassName.Equals(E.ClassName,ESearchCase::CaseSensitive) || !Deprecated.Equals(E.Deprecated,ESearchCase::CaseSensitive)
                || !E.Node->ErrorMessage.Equals(E.Error,ESearchCase::CaseSensitive))
            { return Stop(Stage,E.Path,TEXT("editor_error_source"),E.ClassName+TEXT("/")+E.Deprecated+TEXT("/")+E.Error,ClassName+TEXT("/")+Deprecated+TEXT("/")+E.Node->ErrorMessage); }
            if (Receipt) { Receipt->Add(MakeShared<FJsonValueObject>(EditorExecutionJson(E,ExecutionRebuilt))); }
        }
        for (const auto& Role : ExecutionRoles) if (Role.Node->GetClass()==UBTDecorator_Blackboard::StaticClass())
        {
            const auto* D=static_cast<const UBTDecorator*>(Role.Node); const auto Mode=D->GetFlowAbortMode();
            const auto* Parent=Role.Planned.Parent;
            if (!Parent || (Parent->GetClass()==UBTComposite_Sequence::StaticClass() && Mode!=EBTFlowAbortMode::None && Mode!=EBTFlowAbortMode::Self)
                || static_cast<uint8>(Mode)>static_cast<uint8>(EBTFlowAbortMode::Both))
            { return Stop(Stage,Role.Path,TEXT("flow_abort_mode"),TEXT("normalized native mode supported by the planned parent"),LexToString(static_cast<int32>(Mode))); }
        }
        return true;
    }
    bool PlanEditorExecution()
    {
        TArray<UBehaviorTreeGraphNode*> Ordered { Root };
        for (const auto& M : MainRoles) { Ordered.Add(M.Node); }
        for (const auto& C : Composites) { Ordered.Add(C.Node); }
        for (const auto& D : PlainDecorators) { Ordered.Add(D.Node); }
        for (auto* Node : Ordered)
        {
            FEditorExecutionRole E; E.Node=Node; E.CompositeRole=IsComposite(Node); E.Path=Node->GetPathName();
            if (!ReadEditorParent(Node,E.Path,TEXT("editor_execution_before"),E.BeforeParent) || !Node->NodeInstance.IsResolved()) { return false; }
            E.Instance=Node->NodeInstance.Get(); if (!Hold(E.Instance)) { return false; }
            E.ParentPath=E.BeforeParent?E.BeforeParent->GetPathName():FString();
            const FCompositeRole* C=Composites.FindByPredicate([&](const FCompositeRole& V){return V.Node==Node;});
            const FPlainDecorator* Plain=PlainDecorators.FindByPredicate([&](const FPlainDecorator& V){return V.Node==Node;});
            E.PlannedParent=C?C->Host:Plain?Plain->Host:nullptr; E.PlannedParentPath=E.PlannedParent?E.PlannedParent->GetPathName():FString();
            E.BeforeRoot=static_cast<bool>(Node->bRootLevel); E.PlannedRoot=E.Instance==ExecutionRoot; E.BeforeObserver=static_cast<bool>(Node->bHasObserverError);
            E.ClassName=Node->ClassData.GetClassName(); E.Deprecated=Node->ClassData.GetDeprecatedMessage(); E.Error=Node->ErrorMessage;
            E.ErrorUpdated=TopNodes.Contains(Node); for (auto* Top : TopNodes) { E.ErrorUpdated|=static_cast<UBehaviorTreeGraphNode*>(Top)->SubNodes.Contains(Node); }
            FString ExpectedError=E.Error;
            if (E.ErrorUpdated && E.Instance)
            {
                if (E.Instance->GetClass()!=UBTComposite_Sequence::StaticClass() && E.Instance->GetClass()!=UBTComposite_Selector::StaticClass() && !MainTask(E.Instance->GetClass()) && E.Instance->GetClass()!=UBTDecorator_Blackboard::StaticClass()) { return false; }
                const FString NativeDeprecated=FGraphNodeClassHelper::GetDeprecationMessage(E.Instance->GetClass()); ExpectedError=NativeDeprecated.IsEmpty()?E.Deprecated:NativeDeprecated;
            }
            else if (E.ErrorUpdated) { FString Stored=E.ClassName; Stored.RemoveFromEnd(TEXT("_C")); if (!Stored.IsEmpty()) { ExpectedError=Stored+TEXT(" class missing. Referenced by ")+Node->GetFullName(); } }
            if (ExpectedError!=E.Error) { return Stop(TEXT("editor_execution_admission"),E.Path,TEXT("ErrorMessage"),ExpectedError,E.Error); }
            EditorExecutionRoles.Add(MoveTemp(E));
        }
        for (const auto& E : EditorExecutionRoles)
        {
            FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==E.Node;});
            FField* F=O?O->Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("ParentNode");}):nullptr;
            if (!F) { return false; } TObjectPtr<UAIGraphNode> Value(E.PlannedParent); F->Planned.Reset(); if (!Export(*O,*F,F->Planned,&Value)) { return false; } F->Execution=true;
        }
        TArray<TSharedPtr<FJsonValue>> Before,Planned; for (const auto& E : EditorExecutionRoles) { Before.Add(MakeShared<FJsonValueObject>(EditorExecutionJson(E,false))); Planned.Add(MakeShared<FJsonValueObject>(EditorExecutionJson(E,true))); }
        R.Data->SetArrayField(TEXT("editor_execution_metadata_before"),Before); R.Data->SetArrayField(TEXT("editor_execution_metadata_plan"),Planned); return EditorExecutionCurrent(TEXT("editor_execution_plan_complete"));
    }

    // Exact finite CreateBTFromGraph projection; no runtime writer or late baseline adoption.
    struct FExecutionValue
    {
        uint16 Index=0, Memory=0; uint8 Depth=0;
        UBTCompositeNode* Parent=nullptr; UBehaviorTree* Asset=nullptr;
        FString ParentPath, AssetPath; int32 Last=-1, Child=-1;
    };
    struct FExecutionRole { UBTNode* Node=nullptr; FString Path; FExecutionValue Before, Planned; };
    TArray<FExecutionRole> ExecutionRoles;
    bool ExecutionRebuilt=false;
    uint16 RangeFirstBefore=0, RangeLastBefore=0;
    UBTCompositeNode* EditorParentBefore=nullptr; UBTCompositeNode* ExecutionRoot=nullptr;
    bool ReadExecution(UBTNode* Node,const FString& Path,const TCHAR* Stage,FExecutionValue& V)
    {
        const FObject* O=Objects.FindByPredicate([&](const FObject& Item){return Item.Hold.Get()==Node;});
        if (!O || !Loaded(Node)) { return Stop(Stage,Path,TEXT("execution_owner"),TEXT("held captured runtime node"),TEXT("unavailable")); }
        FObjectProperty* ParentField=nullptr; FObjectProperty* AssetField=nullptr;
        for (FName Name : { FName(TEXT("ExecutionIndex")), FName(TEXT("MemoryOffset")), FName(TEXT("TreeDepth")), FName(TEXT("ParentNode")), FName(TEXT("TreeAsset")) })
        {
            const FField* F=O->Fields.FindByPredicate([&](const FField& Item){return Item.Name==Name;});
            if (!F || !FieldCurrent(*O,*F) || F->Owner!=UBTNode::StaticClass() || F->Dim!=1)
            { return Stop(Stage,Path,Name.ToString(),TEXT("captured native BTNode field role"),TEXT("changed")); }
            if (Name==TEXT("ExecutionIndex") || Name==TEXT("MemoryOffset"))
            { if (F->Class!=FUInt16Property::StaticClass() || F->Size!=sizeof(uint16)) { return Stop(Stage,Path,Name.ToString(),TEXT("native uint16"),TEXT("unsupported")); } }
            else if (Name==TEXT("TreeDepth"))
            { if (F->Class!=FByteProperty::StaticClass() || F->Size!=sizeof(uint8)) { return Stop(Stage,Path,Name.ToString(),TEXT("native uint8"),TEXT("unsupported")); } }
            else
            {
                auto* P=CastField<FObjectProperty>(F->Pointer);
                UClass* ExpectedClass=Name==TEXT("ParentNode")?UBTCompositeNode::StaticClass():UBehaviorTree::StaticClass();
                if (!P || P->PropertyClass!=ExpectedClass || F->Size!=sizeof(TObjectPtr<UObject>)) { return Stop(Stage,Path,Name.ToString(),TEXT("native object field binding"),TEXT("unsupported")); }
                const auto* Slot=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(Node));
                if (!Slot->IsResolved()) { return Stop(Stage,Path,Name.ToString(),TEXT("resolved nullable object slot before public getter"),TEXT("unresolved")); }
                UObject* Value=Slot->Get(); if (!Hold(Value)) { return false; }
                if (Name==TEXT("ParentNode")) { ParentField=P; } else { AssetField=P; }
            }
        }
        // All five native descriptors and object slots were checked before these public inline getters.
        V.Index=Node->GetExecutionIndex(); V.Memory=Node->GetMemoryOffset(); V.Depth=Node->GetTreeDepth();
        V.Parent=Node->GetParentNode(); V.Asset=Node->GetTreeAsset();
        if (!ParentField || !AssetField || !Hold(V.Parent) || !Hold(V.Asset)) { return false; }
        V.ParentPath=V.Parent?V.Parent->GetPathName():FString(); V.AssetPath=V.Asset?V.Asset->GetPathName():FString();
        if (Node->GetClass()==UBTComposite_Sequence::StaticClass() || Node->GetClass()==UBTComposite_Selector::StaticClass()) { V.Last=static_cast<UBTCompositeNode*>(Node)->GetLastExecutionIndex(); }
        if (Node->GetClass()==UBTDecorator_Blackboard::StaticClass()) { V.Child=static_cast<UBTDecorator*>(Node)->GetChildIndex(); }
        return true;
    }
    static bool ExecutionSame(const FExecutionValue& A,const FExecutionValue& B)
    { return A.Index==B.Index && A.Memory==B.Memory && A.Depth==B.Depth && A.Parent==B.Parent && A.Asset==B.Asset && A.Last==B.Last && A.Child==B.Child; }
    static TSharedRef<FJsonObject> ExecutionJson(const FExecutionRole& Role,const FExecutionValue& V)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("object"),Role.Path);
        J->SetNumberField(TEXT("execution_index"),V.Index); J->SetNumberField(TEXT("memory_offset"),V.Memory); J->SetNumberField(TEXT("tree_depth"),V.Depth);
        J->SetStringField(TEXT("parent"),V.ParentPath); J->SetStringField(TEXT("tree_asset"),V.AssetPath);
        if (V.Last>=0) { J->SetNumberField(TEXT("last_execution_index"),V.Last); } if (V.Child>=0) { J->SetNumberField(TEXT("child_index"),V.Child); }
        return J;
    }
    bool ReadEditorExecutionParent(FCompositeRole& C,const TCHAR* Stage,UBTCompositeNode*& Value)
    {
        const FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==C.Node;});
        const FField* F=O?O->Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("ParentNodeInstance");}):nullptr;
        auto* P=F&&FieldCurrent(*O,*F)?CastField<FObjectProperty>(F->Pointer):nullptr;
        if (!P || F->Owner!=UBehaviorTreeGraphNode_CompositeDecorator::StaticClass() || P->PropertyClass!=UBTCompositeNode::StaticClass() || F->Size!=sizeof(TObjectPtr<UBTCompositeNode>) || F->Dim!=1)
        { return Stop(Stage,TEXT("composite"),TEXT("runtime_parent_binding"),TEXT("captured native object field role"),TEXT("changed")); }
        const auto* Slot=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(C.Node));
        if (!Slot->IsResolved()) { return Stop(Stage,O->Path,TEXT("ParentNodeInstance"),TEXT("resolved nullable slot"),TEXT("unresolved")); }
        UObject* A=Slot->Get(); if (!Hold(A)) { return false; }
        if (A && A->GetClass()!=UBTComposite_Sequence::StaticClass() && A->GetClass()!=UBTComposite_Selector::StaticClass()) { return Stop(Stage,O->Path,TEXT("runtime_parent_class"),TEXT("native Sequence/Selector or null"),TEXT("unsupported")); }
        Value=static_cast<UBTCompositeNode*>(A); return true;
    }
    bool ExecutionCurrent(const TCHAR* Stage,TArray<TSharedPtr<FJsonValue>>* Receipt=nullptr)
    {
        for (const auto& Role : ExecutionRoles)
        {
            FExecutionValue Actual; if (!ReadExecution(Role.Node,Role.Path,Stage,Actual)) { return false; }
            const FExecutionValue& Expected=ExecutionRebuilt?Role.Planned:Role.Before;
            if (!ExecutionSame(Actual,Expected)) { return Stop(Stage,Role.Path,TEXT("execution_role"),Text(ExecutionJson(Role,Expected)),Text(ExecutionJson(Role,Actual))); }
            if (Receipt) { Receipt->Add(MakeShared<FJsonValueObject>(ExecutionJson(Role,Actual))); }
        }
        if (!ExecutionRoles.IsEmpty()) for (auto& C : Composites)
        {
            UBTCompositeNode* Parent=nullptr; if (!ReadEditorExecutionParent(C,Stage,Parent)) { return false; }
            const uint16 First=ExecutionRebuilt?C.First:C.FirstBefore, Last=ExecutionRebuilt?C.Last:C.LastBefore;
            UBTCompositeNode* Expected=ExecutionRebuilt?C.Parent:C.ParentBefore;
            if (C.Node->FirstExecutionIndex!=First || C.Node->LastExecutionIndex!=Last || Parent!=Expected)
            { return Stop(Stage,C.Node->GetPathName(),TEXT("execution_range/parent"),LexToString(First)+TEXT("/")+LexToString(Last)+TEXT("/")+(Expected?Expected->GetPathName():FString()),LexToString(C.Node->FirstExecutionIndex)+TEXT("/")+LexToString(C.Node->LastExecutionIndex)+TEXT("/")+(Parent?Parent->GetPathName():FString())); }
        }
        return true;
    }
    bool PlanExecutionFields(FExecutionRole& Role)
    {
        FObject* O=Objects.FindByPredicate([&](const FObject& Item){return Item.Hold.Get()==Role.Node;});
        if (!O) { return false; }
        for (auto& F : O->Fields)
        {
            const void* Planned=nullptr; TObjectPtr<UBTCompositeNode> Parent(Role.Planned.Parent); TObjectPtr<UBehaviorTree> Asset(Role.Planned.Asset);
            if (F.Name==TEXT("ExecutionIndex")) { Planned=&Role.Planned.Index; }
            else if (F.Name==TEXT("MemoryOffset")) { Planned=&Role.Planned.Memory; }
            else if (F.Name==TEXT("TreeDepth")) { Planned=&Role.Planned.Depth; }
            else if (F.Name==TEXT("ParentNode")) { Planned=&Parent; }
            else if (F.Name==TEXT("TreeAsset")) { Planned=&Asset; }
            if (Planned) { F.Planned.Reset(); if (!Export(*O,F,F.Planned,Planned)) { return false; } F.Execution=true; }
        }
        return true;
    }
    bool PlanExecution()
    {
        ExecutionRoot=Tree->RootNode.Get(); int32 Next=0; TSet<UBTNode*> Seen;
        const auto Add=[&](UBTNode* Node,UBTCompositeNode* Parent,uint8 Depth,int32 Child)
        {
            if (!Node || Seen.Contains(Node) || Next>=MAX_uint16) { return false; } Seen.Add(Node);
            FExecutionRole Role; Role.Node=Node; Role.Path=Node->GetPathName();
            if (!ReadExecution(Node,Role.Path,TEXT("execution_before"),Role.Before)) { return false; }
            if (Role.Before.Asset!=Tree) { return Stop(TEXT("execution_admission"),Role.Path,TEXT("TreeAsset"),TEXT("existing normalized held Tree"),Role.Before.AssetPath); }
            Role.Planned=Role.Before; Role.Planned.Index=static_cast<uint16>(Next++); Role.Planned.Memory=0; Role.Planned.Depth=Depth;
            Role.Planned.Parent=Parent; Role.Planned.ParentPath=Parent?Parent->GetPathName():FString();
            if (Node->GetClass()==UBTDecorator_Blackboard::StaticClass()) { Role.Planned.Child=Child; }
            ExecutionRoles.Add(MoveTemp(Role)); return true;
        };
        TFunction<bool(FMainRole*,UBTCompositeNode*,uint8)> Visit;
        Visit=[&](FMainRole* M,UBTCompositeNode* Parent,uint8 Depth)
        {
            if (!M || Depth>=MaxDepth || !Add(M->Instance,Parent,Depth,-1)) { return false; }
            if (MainTask(M->Instance->GetClass())) { return M->Children.IsEmpty(); }
            auto* Runtime=static_cast<UBTCompositeNode*>(M->Instance);
            for (int32 I=0;I<M->Children.Num();++I)
            {
                auto* Child=Main(M->Children[I]); if (!Child || I>MAX_uint8) { return false; }
                for (auto* D : Child->Tests) if (!Add(D,Runtime,Depth+1,I)) { return false; }
                for (auto& C : Composites) if (C.Host==Child->Node)
                {
                    C.Parent=Runtime; C.Child=static_cast<uint8>(I);
                    const FExecutionRole* A=ExecutionRoles.FindByPredicate([&](const FExecutionRole& V){return V.Node==C.Tests[0];});
                    const FExecutionRole* B=ExecutionRoles.FindByPredicate([&](const FExecutionRole& V){return V.Node==C.Tests.Last();});
                    if (!A || !B) { return false; } C.First=A->Planned.Index; C.Last=B->Planned.Index;
                }
                if (!Visit(Child,Runtime,Depth+1)) { return false; }
            }
            FExecutionRole* Role=ExecutionRoles.FindByPredicate([&](const FExecutionRole& V){return V.Node==Runtime;});
            if (!Role) { return false; } Role->Planned.Last=Next-1; return true;
        };
        FMainRole* Entry=nullptr; for (auto& M : MainRoles) if (M.Instance==ExecutionRoot) { Entry=&M; }
        if (!Entry || !Visit(Entry,nullptr,0) || Seen.Num()!=MainRoles.Num()+Decorators.Num()) { return Stop(TEXT("execution_plan"),TEXT("tree"),TEXT("reachable_roles"),TEXT("all native roles exactly once"),TEXT("unsupported topology")); }
        for (auto& C : Composites)
        {
            C.FirstBefore=C.Node->FirstExecutionIndex; C.LastBefore=C.Node->LastExecutionIndex;
            if (!ReadEditorExecutionParent(C,TEXT("execution_before"),C.ParentBefore)) { return false; }
        }
        for (auto& Role : ExecutionRoles) if (!PlanExecutionFields(Role)) { return false; }
        for (auto& C : Composites)
        {
            FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==C.Node;}); if (!O) { return false; }
            for (auto& F : O->Fields)
            {
                const void* Value=nullptr; TObjectPtr<UBTCompositeNode> Parent(C.Parent);
                if (F.Name==TEXT("ParentNodeInstance")) { Value=&Parent; }
                if (Value) { F.Planned.Reset(); if (!Export(*O,F,F.Planned,Value)) { return false; } F.Execution=true; }
            }
        }
        TArray<TSharedPtr<FJsonValue>> Before,Planned;
        for (const auto& Role : ExecutionRoles) { Before.Add(MakeShared<FJsonValueObject>(ExecutionJson(Role,Role.Before))); Planned.Add(MakeShared<FJsonValueObject>(ExecutionJson(Role,Role.Planned))); }
        R.Data->SetArrayField(TEXT("execution_metadata_before"),Before); R.Data->SetArrayField(TEXT("execution_metadata_plan"),Planned);
        R.Data->SetStringField(TEXT("execution_metadata_authority"),TEXT("bounded SDK ordered graph child traversal and public CollectDecoratorData; per-parent decorator prefix, native child index, recursive depth/last-index, memory0; strict returned UpdateAsset plan"));
        return PlanEditorExecution() && ExecutionCurrent(TEXT("execution_plan_complete"));
    }

    FResult& R; UBehaviorTree* Tree = nullptr; UBehaviorTreeGraph* Graph = nullptr;
    UBehaviorTreeGraphNode_Root* Root = nullptr; UBehaviorTreeGraphNode_CompositeDecorator* Composite = nullptr;
    UBehaviorTreeDecoratorGraph* Inner = nullptr; UBlackboardData* OldBB = nullptr; UBlackboardData* Requested = nullptr;
    TArray<FObject> Objects; TArray<FNode> Nodes; TArray<TStrongObjectPtr<UObject>> References;
    TArray<UEdGraphNode*> TopNodes, InnerNodes; TSet<UClass*> Classes;
    TArray<UBTDecorator*> Decorators; TArray<FBTDecoratorLogic> Ops;
    TArray<FCompositeRole> Composites; TArray<FMainRole> MainRoles;
    UClass* LogicClass=nullptr; UClass* TestClass=nullptr;
    bool IsComposite(const UObject* Node) const
    { for (const auto& C : Composites) if (C.Node==Node) { return true; } return false; }
    static bool SameOps(const TArray<FBTDecoratorLogic>& A,const TArray<FBTDecoratorLogic>& B)
    { if (A.Num()!=B.Num()) { return false; } for (int32 I=0;I<A.Num();++I) if (A[I].Operation!=B[I].Operation || A[I].Number!=B[I].Number) { return false; } return true; }
    FMainRole* Main(UBehaviorTreeGraphNode* Node)
    { return MainRoles.FindByPredicate([&](const FMainRole& V){return V.Node==Node;}); }
    // Public Collect uses ordered input pins and the first peer. Close the whole
    // owned reciprocal inventory before deriving exactly that finite expression.
    bool Expression(const FCompositeRole& C,TArray<UBTDecorator*>& Tests,TArray<FBTDecoratorLogic>& Logic)
    {
        TSet<UEdGraphNode*> Seen; UEdGraphNode* Sink=nullptr;
        const auto Mode=[&](UEdGraphNode* N,uint8& Out)
        {
            const FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==N;});
            const FField* F=O?O->Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("LogicMode");}):nullptr;
            if (!F || !FieldCurrent(*O,*F) || F->Class!=FByteProperty::StaticClass() || F->Size!=1 || F->Dim!=1 || F->Owner!=LogicClass) { return false; }
            Out=static_cast<FByteProperty*>(F->Pointer)->GetPropertyValue_InContainer(N); return true;
        };
        for (auto* N : C.Members) if (N->GetClass()==LogicClass)
        { uint8 M=0; if (!Mode(N,M)) { return false; } if (M==0) { if (Sink) { return false; } Sink=N; } }
        if (!Sink) { return false; }
        TFunction<bool(UEdGraphNode*,int32)> Visit;
        Visit=[&](UEdGraphNode* N,int32 Depth)
        {
            if (!N || !C.Members.Contains(N) || Seen.Contains(N) || Depth>MaxDepth) { return false; }
            Seen.Add(N); TArray<UEdGraphNode*> Inputs; int32 Outputs=0;
            for (auto* P : N->Pins)
            {
                if (P->LinkedTo.Num()!=1) { return false; }
                if (P->Direction==EGPD_Output) { ++Outputs; continue; }
                const UEdGraphPin* Peer=P->LinkedTo[0];
                const FNode* Owner=Nodes.FindByPredicate([&](const FNode& V){return V.Graph==C.Graph && V.Pointer->Pins.Contains(Peer);});
                if (!Owner || !C.Members.Contains(Owner->Pointer)) { return false; }
                Inputs.Add(Owner->Pointer);
            }
            if (N->GetClass()==TestClass)
            {
                if (!Inputs.IsEmpty() || Outputs!=1) { return false; }
                const FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==N;});
                const FField* F=O?O->Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("NodeInstance");}):nullptr;
                auto* P=F&&FieldCurrent(*O,*F)?CastField<FObjectProperty>(F->Pointer):nullptr;
                if (!P) { return false; } const auto* Slot=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(N));
                if (!Slot->IsResolved()) { return false; } UObject* D=Slot->Get();
                if (!Decorators.Contains(static_cast<UBTDecorator*>(D))) { return false; }
                Logic.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test,static_cast<uint16>(Tests.Add(static_cast<UBTDecorator*>(D))))); return true;
            }
            if (N->GetClass()!=LogicClass) { return false; }
            uint8 M=0; if (!Mode(N,M) || M>3 || Outputs!=(M==0?0:1)) { return false; }
            if ((M==0 || M==3) ? Inputs.Num()!=1 : Inputs.Num()<2) { return false; }
            if (M) { Logic.Add(FBTDecoratorLogic(M==1?EBTDecoratorLogic::And:M==2?EBTDecoratorLogic::Or:EBTDecoratorLogic::Not,static_cast<uint16>(Inputs.Num()))); }
            for (auto* Child : Inputs) if (!Visit(Child,Depth+1)) { return false; }
            return true;
        };
        return Visit(Sink,0) && Seen.Num()==C.Members.Num();
    }
    struct FClass { UClass* Pointer; UObject* CDO; uint32 Flags; };
    TArray<FClass> NativeClasses; TArray<FField> SelectorFields;
    // Each raw role is backed by References/AddObject strong ownership, not an array element address.
    struct FSelfKeyRole { UBlackboardData* Blackboard; UBlackboardKeyType_Object* Key; UClass* Base; int32 Id; bool Synced; };
    TArray<FSelfKeyRole> SelfKeyRoles;
    struct FBlackboardKeyRole { FName Name; FString DisplayName; UBlackboardKeyType* Type=nullptr; bool Synced=false; int32 Id=0; EBlackboardKeyOperation::Type Operation=EBlackboardKeyOperation::Basic; };
    struct FBlackboardRole { UBlackboardData* Data=nullptr; int32 First=0; TArray<FBlackboardKeyRole> Keys; };
    TArray<FBlackboardRole> BlackboardRoles;
    bool BlackboardRolesCurrent(const TCHAR* Stage)
    {
        for (const auto& Role : BlackboardRoles)
        {
            if (!Role.Data->Parent.IsResolved() || Role.Data->Parent.Get()!=nullptr || !Role.Data->ParentKeys.IsEmpty()
                || Role.Data->Keys.Num()!=Role.Keys.Num() || static_cast<int32>(Role.Data->GetFirstKeyID())!=Role.First)
            { return Stop(Stage,Role.Data->GetPathName(),TEXT("Blackboard.source_inventory"),TEXT("captured parentless own key count/first ID"),TEXT("changed or unresolved")); }
            // Close every current row before any SDK lookup can dereference a KeyType or parent.
            for (int32 I=0;I<Role.Keys.Num();++I)
            {
                const auto& Actual=Role.Data->Keys[I]; const auto& Expected=Role.Keys[I];
                if (Actual.EntryName!=Expected.Name || Actual.EntryName.ToString()!=Expected.DisplayName || Actual.bInstanceSynced!=Expected.Synced
                    || !Actual.KeyType.IsResolved() || Actual.KeyType.Get()!=Expected.Type || Expected.Type->GetTestOperation()!=Expected.Operation)
                { return Stop(Stage,Role.Data->GetPathName(),TEXT("Keys[")+FString::FromInt(I)+TEXT("].name/sync/type"),Expected.DisplayName+TEXT("/captured sync/resolved key object"),Actual.EntryName.ToString()+TEXT("/changed or unresolved")); }
            }
            for (const auto& Key : Role.Keys) if (static_cast<int32>(Role.Data->GetKeyID(Key.Name))!=Key.Id)
            { return Stop(Stage,Role.Data->GetPathName(),TEXT("key_id"),LexToString(Key.Id),LexToString(static_cast<int32>(Role.Data->GetKeyID(Key.Name)))); }
        }
        return true;
    }

    // Text export intentionally remains PPF_None for legacy receipts. It is not the transient cache authority.
    struct FSelectorValue
    {
        FName Name; int32 Id = 0, GetterId = 0; UClass* Type = nullptr; FString TypePath; bool NoneAllowed = false;
        TArray<UBlackboardKeyType*> Filters; TArray<FString> FilterPaths;
    };
    struct FSelectorRole
    {
        UObject* Owner = nullptr; FStructProperty* Property = nullptr; FString Path;
        FSelectorValue Before, Planned; bool HasPlan = false;
    };
    TArray<FSelectorRole> TypedSelectors; TSet<UObject*> SelectorFilters;
    struct FFilterReference { UObject* Owner; FName Name; FObjectProperty* Field; int32 Index; UObject* Value; };
    TArray<FFilterReference> FilterReferences;
    FIntProperty* SelectorId = nullptr; FBoolProperty* SelectorNone = nullptr; FArrayProperty* SelectorAllowed = nullptr;
    FObjectProperty* SelectorInner = nullptr; FClassProperty* SelectorType = nullptr;
    FField SelectorInnerRole; uint8 NoneByteOffset = 0, NoneByteMask = 0, NoneFieldMask = 0;
    bool BindSelectorCache()
    {
        UScriptStruct* S = FBlackboardKeySelector::StaticStruct();
        SelectorId = FindFProperty<FIntProperty>(S, TEXT("SelectedKeyID")); SelectorNone = FindFProperty<FBoolProperty>(S, TEXT("bNoneIsAllowedValue"));
        SelectorAllowed = FindFProperty<FArrayProperty>(S, TEXT("AllowedTypes")); SelectorType = FindFProperty<FClassProperty>(S, TEXT("SelectedKeyType"));
        if (!SelectorId || SelectorId->GetClass() != FIntProperty::StaticClass() || SelectorId->GetSize() != sizeof(int32) || SelectorId->ArrayDim != 1
            || !SelectorNone || SelectorNone->ArrayDim != 1 || !SelectorAllowed || SelectorAllowed->ArrayDim != 1 || !SelectorType || SelectorType->ArrayDim != 1)
        { return Stop(TEXT("selector_binding"), S->GetPathName(), TEXT("typed_cache_fields"), TEXT("public reflected int32/bool/Array/object Class fields"), TEXT("unsupported layout")); }
        SelectorInner = CastField<FObjectProperty>(SelectorAllowed->Inner);
        if (!SelectorInner || SelectorInner->GetOwner<FProperty>() != SelectorAllowed || SelectorInner->GetOwnerStruct() != S || SelectorInner->ArrayDim != 1
            || SelectorInner->GetSize() != sizeof(TObjectPtr<UBlackboardKeyType>) || SelectorInner->PropertyClass != UBlackboardKeyType::StaticClass()
            || SelectorAllowed->ArrayFlags != EArrayPropertyFlags::None || SelectorType->MetaClass != UBlackboardKeyType::StaticClass())
        { return Stop(TEXT("selector_binding"), S->GetPathName(), TEXT("AllowedTypes.Inner"), TEXT("owned native BlackboardKeyType object Inner"), TEXT("unsupported binding")); }
        FProperty* P = SelectorInner; FField& F = SelectorInnerRole;
        F.Name=P->GetFName(); F.Pointer=P; F.Owner=P->GetOwnerStruct(); F.Class=P->GetClass(); F.Offset=P->GetOffset_ForInternal(); F.Size=P->ElementSize; F.Dim=P->ArrayDim; F.Flags=static_cast<uint64>(P->GetPropertyFlags()); F.ObjectClass=SelectorInner->PropertyClass;
        NoneByteOffset=SelectorNone->GetByteOffset(); NoneByteMask=SelectorNone->GetByteMask(); NoneFieldMask=SelectorNone->GetFieldMask();
        return Hold(S) && Hold(SelectorInner->PropertyClass) && Hold(SelectorType->MetaClass);
    }
    bool SelectorBindingCurrent(const TCHAR* Stage)
    {
        if (!SelectorId) { return true; }
        UScriptStruct* S=FBlackboardKeySelector::StaticStruct();
        // SelectorFields were re-found and checked before this typed/nested binding is read.
        if (FindFProperty<FIntProperty>(S,TEXT("SelectedKeyID"))!=SelectorId || FindFProperty<FBoolProperty>(S,TEXT("bNoneIsAllowedValue"))!=SelectorNone
            || FindFProperty<FArrayProperty>(S,TEXT("AllowedTypes"))!=SelectorAllowed || FindFProperty<FClassProperty>(S,TEXT("SelectedKeyType"))!=SelectorType
            || SelectorAllowed->Inner!=SelectorInner || SelectorAllowed->ArrayFlags!=EArrayPropertyFlags::None)
        { return Stop(Stage,TEXT("BlackboardKeySelector"),TEXT("typed_bindings"),TEXT("captured root/Inner field identity"),TEXT("changed; old Inner not read")); }
        const FField& F=SelectorInnerRole; FProperty* P=SelectorAllowed->Inner;
        if (!P || P->GetClass()!=F.Class || P->GetOwnerStruct()!=F.Owner || P->GetOwner<FProperty>()!=SelectorAllowed || P->GetFName()!=F.Name
            || P->GetOffset_ForInternal()!=F.Offset || P->ElementSize!=F.Size || P->ArrayDim!=F.Dim || static_cast<uint64>(P->GetPropertyFlags())!=F.Flags
            || SelectorInner->PropertyClass!=F.ObjectClass || SelectorType->MetaClass!=UBlackboardKeyType::StaticClass()
            || SelectorNone->GetByteOffset()!=NoneByteOffset || SelectorNone->GetByteMask()!=NoneByteMask || SelectorNone->GetFieldMask()!=NoneFieldMask)
        { return Stop(Stage,TEXT("BlackboardKeySelector"),TEXT("typed_layout"),TEXT("captured owner/layout/masks/Class binding"),TEXT("changed")); }
        return true;
    }
    bool CaptureFilterReferences()
    {
        for (UObject* Filter : SelectorFilters)
        {
            const FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==Filter;});
            if (!O) { return false; }
            for (const auto& F : O->Fields) if (auto* P=CastField<FObjectProperty>(F.Pointer))
            {
                if (!FieldCurrent(*O,F)) { return Stop(TEXT("filter_capture"),O->Path,F.Name.ToString(),TEXT("current native field"),TEXT("changed")); }
                for (int32 I=0;I<F.Dim;++I)
                {
                    const auto* Slot=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(Filter,I));
                    if (!Slot->IsResolved()) { return Stop(TEXT("filter_capture"),O->Path,F.Name.ToString(),TEXT("resolved nullable filter reference"),TEXT("unresolved")); }
                    UObject* Value=Slot->Get(); if (!Hold(Value)) { return false; }
                    FilterReferences.Add({ Filter,F.Name,P,I,Value });
                }
            }
        }
        return true;
    }
    bool FilterReferencesCurrent(const TCHAR* Stage)
    {
        for (const auto& F : FilterReferences)
        {
            // Objects/FieldCurrent were closed first; compare the returned field address before reading it.
            FProperty* P=FindFProperty<FProperty>(F.Owner->GetClass(),F.Name);
            if (P!=F.Field) { return Stop(Stage,TEXT("filter"),F.Name.ToString(),TEXT("captured object field"),TEXT("changed")); }
            const auto* Slot=F.Field->GetObjectPtrPropertyValuePtr(F.Field->ContainerPtrToValuePtr<void>(F.Owner,F.Index));
            if (!Slot->IsResolved() || Slot->Get()!=F.Value) { return Stop(Stage,TEXT("filter"),F.Name.ToString(),TEXT("captured held reference"),TEXT("changed or unresolved")); }
        }
        return true;
    }
    bool ReadSelector(FBlackboardKeySelector& Key,const FString& Path,const TCHAR* Stage,FSelectorValue& Out)
    {
        if (!SelectorBindingCurrent(Stage)) { return false; }
        // Read protected storage only through the public native reflection descriptor. Never write it.
        const int32 Raw=SelectorId->GetPropertyValue_InContainer(&Key);
        if (Raw<0 || Raw>MAX_uint16) { return Stop(Stage,Path,TEXT("SelectedKeyID.raw"),TEXT("0..65535 before checked uint16 getter"),LexToString(Raw)); }
        const int32 PublicId=static_cast<int32>(Key.GetSelectedKeyID());
        if (PublicId!=Raw) { return Stop(Stage,Path,TEXT("SelectedKeyID.getter"),LexToString(Raw),LexToString(PublicId)); }
        if (!Key.SelectedKeyType.GetGCPtr().IsResolved()) { return Stop(Stage,Path,TEXT("SelectedKeyType"),TEXT("resolved nullable class slot"),TEXT("unresolved")); }
        UClass* CacheType=Key.SelectedKeyType.Get(); if (!Hold(CacheType)) { return false; }
        if (Key.AllowedTypes.Num()>8) { return Stop(Stage,Path,TEXT("AllowedTypes.count"),TEXT("<=8"),LexToString(Key.AllowedTypes.Num())); }
        Out.Name=Key.SelectedKeyName; Out.Id=Raw; Out.GetterId=PublicId; Out.Type=CacheType; Out.TypePath=CacheType?CacheType->GetPathName():FString();
        Out.NoneAllowed=SelectorNone->GetPropertyValue_InContainer(&Key); Out.Filters.Reset(); Out.FilterPaths.Reset();
        for (auto Filter : Key.AllowedTypes)
        {
            if (!Filter.IsResolved()) { return Stop(Stage,Path,TEXT("AllowedTypes.slot"),TEXT("resolved existing filter"),TEXT("unresolved")); }
            UBlackboardKeyType* Actual=Filter.Get(); if (!Actual || !Hold(Actual)) { return Stop(Stage,Path,TEXT("AllowedTypes.object"),TEXT("held loaded filter"),TEXT("null or unavailable")); }
            Out.Filters.Add(Actual); Out.FilterPaths.Add(Actual->GetPathName());
        }
        return true;
    }
    static TSharedRef<FJsonObject> SelectorJson(const FSelectorRole& Role,const FSelectorValue& Value)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("object"),Role.Path); J->SetStringField(TEXT("key"),Value.Name.ToString());
        J->SetNumberField(TEXT("raw_selected_key_id"),Value.Id); J->SetNumberField(TEXT("public_selected_key_id"),Value.GetterId);
        J->SetStringField(TEXT("selected_key_type"),Value.TypePath); J->SetBoolField(TEXT("none_allowed"),Value.NoneAllowed);
        TArray<TSharedPtr<FJsonValue>> Filters; for (const auto& Path : Value.FilterPaths) { Filters.Add(MakeShared<FJsonValueString>(Path)); }
        J->SetArrayField(TEXT("allowed_types"),Filters); return J;
    }
    static bool CacheSame(const FSelectorValue& A,const FSelectorValue& B) { return A.Id==B.Id && A.Type==B.Type; }
    bool TypedCurrent(const TCHAR* Stage,TArray<TSharedPtr<FJsonValue>>* Receipt=nullptr)
    {
        if (!SelectorBindingCurrent(Stage)) { return false; }
        for (const auto& Role : TypedSelectors)
        {
            const FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==Role.Owner;});
            const FField* F=O?O->Fields.FindByPredicate([&](const FField& V){return V.Name==TEXT("BlackboardKey");}):nullptr;
            if (!O || !F || !SelectorProperty(*O,*F) || F->Pointer!=Role.Property)
            { return Stop(Stage,Role.Path,TEXT("BlackboardKey.binding"),TEXT("current owned native selector struct field"),TEXT("changed")); }
            FBlackboardKeySelector Actual=*Role.Property->ContainerPtrToValuePtr<FBlackboardKeySelector>(Role.Owner); FSelectorValue Value;
            if (!ReadSelector(Actual,Role.Path,Stage,Value)) { return false; }
            if (Value.Name!=Role.Before.Name || Value.NoneAllowed!=Role.Before.NoneAllowed || Value.Filters!=Role.Before.Filters)
            { return Stop(Stage,Role.Path,TEXT("selector_name_none_filters"),Role.Before.Name.ToString()+TEXT("/captured none/ordered filters"),Value.Name.ToString()+TEXT("/changed typed values")); }
            const FSelectorValue& Expected=Role.HasPlan&&Resolved?Role.Planned:Role.Before;
            const bool CacheOK=CacheSame(Value,Expected)||(Role.HasPlan&&Resolving&&!Resolved&&CacheSame(Value,Role.Planned));
            if (!CacheOK) { return Stop(Stage,Role.Path,TEXT("selector_cache"),LexToString(Expected.Id)+TEXT("/")+Expected.TypePath,LexToString(Value.Id)+TEXT("/")+Value.TypePath); }
            if (Receipt) { Receipt->Add(MakeShared<FJsonValueObject>(SelectorJson(Role,Value))); }
        }
        return true;
    }
    UStruct* SelectorDeclaringOwner(const UClass* C) const
    { return C==UBTDecorator_Blackboard::StaticClass()?UBTDecorator_BlackboardBase::StaticClass():PatrolTask(C)?UBTTask_BlackboardBase::StaticClass():nullptr; }
    bool SelectorProperty(const FObject& O,const FField& F) const
    {
        auto* P=F.Class==FStructProperty::StaticClass()?static_cast<FStructProperty*>(F.Pointer):nullptr;
        return P && FieldCurrent(O,F) && F.Owner==SelectorDeclaringOwner(O.Class)
            && P->Struct.IsResolved() && P->Struct.Get()==FBlackboardKeySelector::StaticStruct()
            && F.Dim==1 && F.Size==sizeof(FBlackboardKeySelector);
    }
    bool CaptureTypedBefore(UObject* Owner,FStructProperty* P,const FString& Path)
    {
        FBlackboardKeySelector Actual=*P->ContainerPtrToValuePtr<FBlackboardKeySelector>(Owner); FSelectorRole Role;
        Role.Owner=Owner; Role.Property=P; Role.Path=Path; if (!ReadSelector(Actual,Path,TEXT("selector_typed_before"),Role.Before)) { return false; }
        Role.Planned=Role.Before; TypedSelectors.Add(MoveTemp(Role)); return true;
    }
    bool CaptureTypedPlan(UObject* Owner,FBlackboardKeySelector& Copy,int32 RequestedId,UClass* RequestedType)
    {
        FSelectorRole* Role=TypedSelectors.FindByPredicate([&](const FSelectorRole& V){return V.Owner==Owner;});
        if (!Role || !ReadSelector(Copy,Role->Path,TEXT("selector_typed_scratch_plan"),Role->Planned)) { return false; }
        if (Role->Planned.Id!=RequestedId || Role->Planned.Type!=RequestedType || Role->Planned.Name!=Role->Before.Name
            || Role->Planned.NoneAllowed!=Role->Before.NoneAllowed || Role->Planned.Filters!=Role->Before.Filters)
        { return Stop(TEXT("selector_typed_scratch_plan"),Role->Path,TEXT("scratch_result"),LexToString(RequestedId),LexToString(Role->Planned.Id)); }
        Role->HasPlan=true; return true;
    }
    struct FDescriptionEnum
    { UEnum* Enum=nullptr; FName Name; FString Path; int64 Operation=0; FName RawName; FText Display; };
    TArray<FDescriptionEnum> DescriptionEnums;
    bool DescriptionEnumsCurrent(const TCHAR* Stage)
    {
        for (const auto& E : DescriptionEnums)
        {
            if (!Loaded(E.Enum) || E.Enum->GetClass()!=UEnum::StaticClass() || E.Enum->GetFName()!=E.Name || E.Enum->GetPathName()!=E.Path
                || E.Enum->GetNameByValue(E.Operation)!=E.RawName || !SameText(E.Enum->GetDisplayNameTextByValue(E.Operation),E.Display))
            { return Stop(Stage,E.Path,TEXT("description_enum"),TEXT("captured native operation/display metadata"),TEXT("changed")); }
        }
        return true;
    }
    bool PlanMixedDescription(FObject& O,const FBlackboardKeySelector& Key,UClass* KeyType)
    {
        auto* Description=O.Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("CachedDescription");});
        auto* Operation=O.Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("OperationType");});
        auto* String=O.Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("StringValue");});
        const auto NativeField=[&](const FField* F,const FFieldClass* C,int32 Size)
        { return F && FieldCurrent(O,*F) && F->Owner==UBTDecorator_Blackboard::StaticClass() && F->Class==C && F->Dim==1 && F->Size==Size; };
        if (!NativeField(Description,FStrProperty::StaticClass(),sizeof(FString)) || !NativeField(Operation,FByteProperty::StaticClass(),sizeof(uint8))
            || !NativeField(String,FStrProperty::StaticClass(),sizeof(FString)))
        { return Stop(TEXT("description_plan"),O.Path,TEXT("native_description_fields"),TEXT("exact declared FString/uint8/FString fields"),TEXT("unsupported")); }
        FString Value=TEXT("invalid");
        if (KeyType)
        {
            const FBlackboardEntry* Entry=Requested->GetKey(Key.GetSelectedKeyID());
            if (!Pure(TEXT("description_key_return"))) { return false; }
            if (!Entry || !Entry->KeyType.IsResolved() || !Entry->KeyType || Entry->KeyType->GetClass()!=KeyType)
            { return Stop(TEXT("description_key_return"),O.Path,TEXT("entry"),TEXT("captured requested selected key"),TEXT("missing or changed")); }
            const FString EntryName=Entry->EntryName.ToString();
            const EBlackboardKeyOperation::Type Mode=Entry->KeyType->GetTestOperation();
            const uint8 Op=static_cast<FByteProperty*>(Operation->Pointer)->GetPropertyValue_InContainer(O.Hold.Get());
            const FString Param=static_cast<FStrProperty*>(String->Pointer)->GetPropertyValue_InContainer(O.Hold.Get());
            if (Mode!=EBlackboardKeyOperation::Basic && Mode!=EBlackboardKeyOperation::Text)
            { return Stop(TEXT("description_plan"),O.Path,TEXT("key_operation"),TEXT("Basic or Text of bounded native key"),LexToString(static_cast<int32>(Mode))); }
            UEnum* Enum=Mode==EBlackboardKeyOperation::Basic?StaticEnum<EBasicKeyOperation::Type>():StaticEnum<ETextKeyOperation::Type>();
            if (!Hold(Enum) || !Pure(TEXT("description_enum_return"))) { return false; }
            if (!Enum || Enum->GetClass()!=UEnum::StaticClass() || Enum->GetOutermost()->GetName()!=TEXT("/Script/AIModule"))
            { return Stop(TEXT("description_enum_return"),O.Path,TEXT("native_enum"),TEXT("native AIModule operation enum"),TEXT("unsupported")); }
            FDescriptionEnum Role; Role.Enum=Enum; Role.Name=Enum->GetFName(); Role.Path=Enum->GetPathName(); Role.Operation=Op;
            Role.RawName=Enum->GetNameByValue(Op); Role.Display=Enum->GetDisplayNameTextByValue(Op);
            if (!Pure(TEXT("description_display_return"))) { return false; }
            const FString Display=Role.Display.ToString(); if (!Pure(TEXT("description_display_string_return"))) { return false; }
            DescriptionEnums.Add(MoveTemp(Role));
            Value=Mode==EBlackboardKeyOperation::Basic?FString::Printf(TEXT("%s is %s"),*EntryName,*Display)
                :FString::Printf(TEXT("%s %s [%s]"),*EntryName,*Display,*Param);
        }
        Description->Planned.Reset(); if (!Export(O,*Description,Description->Planned,&Value)) { return false; } Description->Selector=true;
        return true;
    }
    void PublishTypedPlan()
    {
        TArray<TSharedPtr<FJsonValue>> Before,Planned;
        for (const auto& Role : TypedSelectors) { Before.Add(MakeShared<FJsonValueObject>(SelectorJson(Role,Role.Before))); Planned.Add(MakeShared<FJsonValueObject>(SelectorJson(Role,Role.Planned))); }
        R.Data->SetArrayField(TEXT("selector_typed_before"),Before); R.Data->SetArrayField(TEXT("selector_typed_scratch_plan"),Planned);
        R.Data->SetStringField(TEXT("selector_cache_observation_authority"),TEXT("typed public reflection raw int32 range, then public getter; resolved class/allowed-filter identities and reflected filter values; PPF_None text is not transient-cache proof"));
    }

    bool Enabled = false, Ready = false, Bad = false, Assigned = false, Resolving = false, Resolved = false, AfterUnlock = false;
    explicit FRDComposite(FResult& In) : R(In) {}
    bool Stop(const TCHAR* Stage, const FString& Object, const FString& Property, const FString& Expected, const FString& Actual)
    {
        if (!Bad)
        {
            auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("stage"), Stage); J->SetStringField(TEXT("object"), Object);
            J->SetStringField(TEXT("property"), Property); J->SetStringField(TEXT("expected"), Expected); J->SetStringField(TEXT("actual"), Actual);
            R.Data->SetObjectField(TEXT("composite_decorator_first_mismatch"), J);
        }
        Bad = true; if (Ready || R.bAttempted) { R.bKnown = false; } R.bPost = false;
        return Fail(R, TEXT("behavior_tree_composite_current_failed"), TEXT("Composite decorator observation or callback boundary failed; actual prefix retained and further work stopped."));
    }
    bool Hold(UObject* O)
    {
        if (!O) { return true; } if (!Loaded(O)) { return Stop(TEXT("capture"), TEXT("unavailable"), TEXT("reference"), TEXT("loaded"), TEXT("unavailable")); }
        for (const auto& H : References) { if (H.Get() == O) { return true; } }
        if (References.Num() >= 512) { return Stop(TEXT("capture"), TEXT("references"), TEXT("bound"), TEXT("<=512"), TEXT("exceeded")); }
        References.Emplace(O); return true;
    }
    bool Type(const FEdGraphPinType& T)
    {
        if (T.PinSubCategoryObject.IsStale() || T.PinValueType.TerminalSubCategoryObject.IsStale()) { return Stop(TEXT("type"), TEXT("pin"), TEXT("weak_subtype"), TEXT("not stale"), TEXT("stale")); }
        return Hold(T.PinSubCategoryObject.Get()) && Hold(T.PinValueType.TerminalSubCategoryObject.Get());
    }
    static bool Optional(const TOptional<FString>& A, const TOptional<FString>& B)
    { return A.IsSet() == B.IsSet() && (!A.IsSet() || A.GetValue().Equals(B.GetValue(), ESearchCase::CaseSensitive)); }
    static bool SameText(const FText& A, const FText& B)
    {
        const FString* X = FTextInspector::GetSourceString(A); const FString* Y = FTextInspector::GetSourceString(B);
        return (X != nullptr) == (Y != nullptr) && (!X || X->Equals(*Y, ESearchCase::CaseSensitive))
            && FTextInspector::GetFlags(A) == FTextInspector::GetFlags(B) && Optional(FTextInspector::GetNamespace(A), FTextInspector::GetNamespace(B)) && Optional(FTextInspector::GetKey(A), FTextInspector::GetKey(B));
    }
    bool FieldCurrent(const FObject& O, const FField& F) const
    {
        FProperty* P = FindFProperty<FProperty>(O.Class, F.Name);
        return P == F.Pointer && P && P->GetOwnerStruct() == F.Owner && P->GetClass() == F.Class && P->GetOffset_ForInternal() == F.Offset
            && P->ElementSize == F.Size && P->ArrayDim == F.Dim && static_cast<uint64>(P->GetPropertyFlags()) == F.Flags
            && (!F.Struct || (CastField<FStructProperty>(P) && CastFieldChecked<FStructProperty>(P)->Struct == F.Struct))
            && (!F.ObjectClass || (CastField<FObjectPropertyBase>(P) && CastFieldChecked<FObjectPropertyBase>(P)->PropertyClass == F.ObjectClass))
            && (!F.MetaClass || (CastField<FClassProperty>(P) && CastFieldChecked<FClassProperty>(P)->MetaClass == F.MetaClass));
    }
    bool Pure(const TCHAR* Stage)
    {
        if (Bad) { return false; }
        if (!IsEditorGameThread() || IsPlaySessionActive() || IsGarbageCollecting() || GIsSavingPackage || IsEngineExitRequested()) { return Stop(Stage, TEXT("editor"), TEXT("context"), TEXT("idle Editor thread"), TEXT("unavailable")); }
        for (const auto& H : References) { if (!Loaded(H.Get())) { return Stop(Stage, TEXT("held reference"), TEXT("live"), TEXT("loaded"), TEXT("unavailable")); } }
        for (const auto& O : Objects)
        {
            UObject* A = O.Hold.Get(); if (!Loaded(A) || A->GetClass() != O.Class || A->GetOuter() != O.Outer || !A->GetPathName().Equals(O.Path, ESearchCase::CaseSensitive)) { return Stop(Stage, O.Path, TEXT("object_role"), TEXT("captured class/owner/path"), TEXT("changed or unavailable")); }
            int32 Count = 0; for (TFieldIterator<FProperty> It(O.Class, EFieldIteratorFlags::IncludeSuper); It; ++It) { ++Count; }
            if (Count != O.Fields.Num()) { return Stop(Stage, O.Path, TEXT("field_count"), LexToString(O.Fields.Num()), LexToString(Count)); }
            for (const auto& F : O.Fields) { if (!FieldCurrent(O, F)) { return Stop(Stage, O.Path, F.Name.ToString(), TEXT("captured descriptor owner/layout/flags"), TEXT("changed")); } }
        }
        for (const auto& C : NativeClasses)
        {
            if (!Loaded(C.Pointer) || !C.Pointer->ClassDefaultObject.IsResolved() || C.Pointer->ClassDefaultObject.Get() != C.CDO || static_cast<uint32>(C.Pointer->GetClassFlags()) != C.Flags || C.Pointer->GetOutermost()->IsDirty()) { return Stop(Stage, TEXT("native class"), TEXT("default_slot/flags/package"), TEXT("captured nullable default and native role"), TEXT("changed")); }
        }
        if (!BlackboardRolesCurrent(Stage)) { return false; }
        for (const auto& Role : SelfKeyRoles)
        {
            const FBlackboardEntry* Current = nullptr;
            for (const auto& Entry : Role.Blackboard->Keys) if (Entry.EntryName == FBlackboard::KeySelf)
            { if (Current) { return Stop(Stage, Role.Blackboard->GetPathName(), TEXT("SelfActor.membership"), TEXT("one captured reserved key"), TEXT("duplicate")); } Current = &Entry; }
            if (!Current || !Current->KeyType.IsResolved() || Current->KeyType.Get() != Role.Key || Current->bInstanceSynced != Role.Synced
                || Role.Key->GetClass() != UBlackboardKeyType_Object::StaticClass() || Role.Key->GetOuter() != Role.Blackboard
                || !Role.Key->BaseClass.IsResolved() || Role.Key->BaseClass.Get() != Role.Base
                || !Role.Key->DefaultValue.IsResolved() || Role.Key->DefaultValue.Get() != nullptr
                || static_cast<int32>(Role.Blackboard->GetKeyID(FBlackboard::KeySelf)) != Role.Id)
            { return Stop(Stage, Role.Blackboard->GetPathName(), TEXT("SelfActor.role"), TEXT("captured exact Object/owner/Actor base/null default/sync/id"), TEXT("changed or unavailable")); }
        }
        int32 SelectorCount = 0; for (TFieldIterator<FProperty> It(FBlackboardKeySelector::StaticStruct(), EFieldIteratorFlags::IncludeSuper); It; ++It) { ++SelectorCount; }
        if (!SelectorFields.IsEmpty() && SelectorCount != SelectorFields.Num()) { return Stop(Stage, TEXT("BlackboardKeySelector"), TEXT("field_count"), LexToString(SelectorFields.Num()), LexToString(SelectorCount)); }
        for (const auto& F : SelectorFields)
        {
            FProperty* P = FindFProperty<FProperty>(FBlackboardKeySelector::StaticStruct(), F.Name);
            if (P != F.Pointer || !P || P->GetOwnerStruct() != F.Owner || P->GetClass() != F.Class || P->GetOffset_ForInternal() != F.Offset || P->ElementSize != F.Size || P->ArrayDim != F.Dim || static_cast<uint64>(P->GetPropertyFlags()) != F.Flags
                || (F.ObjectClass && (!CastField<FObjectPropertyBase>(P) || CastFieldChecked<FObjectPropertyBase>(P)->PropertyClass != F.ObjectClass))
                || (F.MetaClass && (!CastField<FClassProperty>(P) || CastFieldChecked<FClassProperty>(P)->MetaClass != F.MetaClass))) { return Stop(Stage, TEXT("BlackboardKeySelector"), F.Name.ToString(), TEXT("native nested descriptor binding/layout"), TEXT("changed")); }
        }
        for (const auto& D : PlainDecorators)
        {
            if (!D.Node->NodeInstance.IsResolved() || D.Node->NodeInstance.Get()!=D.Instance || !D.Host->Decorators.Contains(D.Node) || !D.Host->SubNodes.Contains(D.Node))
            { return Stop(Stage,D.Node->GetPathName(),TEXT("plain_decorator_role"),TEXT("captured host membership and exact runtime instance"),TEXT("changed")); }
        }
        if (!DescriptionEnumsCurrent(Stage) || !TypedCurrent(Stage) || !FilterReferencesCurrent(Stage) || !ExecutionCurrent(Stage) || !EditorExecutionCurrent(Stage)) { return false; }
        if (!Ready) { return true; }
        if (!Tree->BTGraph.IsResolved() || !Tree->BlackboardAsset.IsResolved() || !Root->BlackboardAsset.IsResolved()) { return Stop(Stage,TEXT("tree"),TEXT("asset_slots"),TEXT("resolved held graph/Blackboard slots"),TEXT("unresolved")); }
        for (auto N : Graph->Nodes) if (!N.IsResolved()) { return Stop(Stage,TEXT("graph"),TEXT("node_slot"),TEXT("resolved inventory before membership"),TEXT("unresolved")); }
        for (auto G : Graph->SubGraphs) if (!G.IsResolved()) { return Stop(Stage,TEXT("graph"),TEXT("subgraph_slot"),TEXT("resolved inventory before membership"),TEXT("unresolved")); }
        for (const auto& C : Composites) for (auto N : C.Graph->Nodes) if (!N.IsResolved()) { return Stop(Stage,TEXT("composite"),TEXT("node_slot"),TEXT("resolved inventory before membership"),TEXT("unresolved")); }
        if (Tree->BTGraph != Graph || Graph->SubGraphs.Num()!=Composites.Num() || Graph->Nodes.Num()!=TopNodes.Num()) { return Stop(Stage,Graph->GetPathName(),TEXT("graph_ownership"),TEXT("captured top/owned subgraphs"),TEXT("changed")); }
        for (const auto& C : Composites)
        {
            if (!C.Node->BoundGraph.IsResolved() || C.Node->BoundGraph.Get()!=C.Graph || !Graph->SubGraphs.Contains(C.Graph) || C.Graph->Nodes.Num()!=C.Members.Num()) { return Stop(Stage,TEXT("composite"),TEXT("graph_membership"),TEXT("captured owned BoundGraph"),TEXT("changed")); }
            for (auto* N : C.Members) if (!C.Graph->Nodes.Contains(N)) { return Stop(Stage,TEXT("composite"),TEXT("inner_node_membership"),TEXT("captured member"),TEXT("changed")); }
        }
        for (auto* N : TopNodes) { if (!Graph->Nodes.Contains(N)) { return Stop(Stage, Graph->GetPathName(), TEXT("top_nodes"), TEXT("captured membership"), TEXT("changed")); } }
        UBlackboardData* Expected = Assigned ? Requested : OldBB;
        if (Tree->BlackboardAsset != Expected || Root->BlackboardAsset != Expected) { return Stop(Stage, Tree->GetPathName(), TEXT("blackboard_slots"), Expected ? Expected->GetPathName() : TEXT("null"), TEXT("different")); }
        for (const auto& E : Nodes)
        {
            auto* N = E.Pointer;
            if (N->NodeGuid != E.Id || N->GetOuter() != E.Graph || N->NodePosX != E.X || N->NodePosY != E.Y || N->Pins.Num() != E.Pins.Num()) { return Stop(Stage, N->GetPathName(), TEXT("node_role"), E.Id.ToString(), N->NodeGuid.ToString()); }
            TSet<FGuid> IDs;
            for (UEdGraphPin* P : N->Pins)
            {
                if (!P || P->WasTrashed() || P->GetOwningNodeUnchecked() != N || !P->PinId.IsValid() || IDs.Contains(P->PinId)) { return Stop(Stage, N->GetPathName(), TEXT("pin_inventory"), TEXT("unique current owned pins"), TEXT("invalid")); }
                IDs.Add(P->PinId); const FPin* Old = E.Pins.FindByPredicate([&](const FPin& V) { return V.Id == P->PinId; });
                if (!Old) { return Stop(Stage, N->GetPathName(), TEXT("pin_guid"), TEXT("captured semantic pin ID"), P->PinId.ToString()); }
                if (!P->DefaultObject.IsResolved()) { return Stop(Stage, N->GetPathName(), TEXT("pin_default_object"), TEXT("resolved nullable slot"), TEXT("unresolved")); }
                if (!Type(P->PinType) || !Hold(P->DefaultObject.Get())) { return false; }
                if (P->PinName != Old->Name || P->Direction != Old->Direction || !(P->PinType == Old->Type) || P->DefaultValue != Old->Raw || P->AutogeneratedDefaultValue != Old->Auto
                    || P->DefaultObject != Old->Default || !SameText(P->DefaultTextValue, Old->Text) || !SameText(P->PinFriendlyName, Old->Friendly)
                    || P->bHidden != Old->Hidden || P->bNotConnectable != Old->NotConnectable || P->bDefaultValueIsReadOnly != Old->DefaultReadOnly
                    || P->bOrphanedPin || P->ParentPin || !P->SubPins.IsEmpty() || P->LinkedTo.Num() != Old->Links.Num()) { return Stop(Stage, N->GetPathName(), TEXT("pin_semantics"), Old->Name.ToString() + TEXT("/") + Old->Raw, P->PinName.ToString() + TEXT("/") + P->DefaultValue); }
                TArray<FString> Links;
                for (UEdGraphPin* Other : P->LinkedTo)
                {
                    // Establish candidate membership by address before reading any peer pin.
                    const FNode* Owner = nullptr; for (const auto& Row : Nodes) { if (Row.Pointer->Pins.Contains(Other)) { Owner = &Row; break; } }
                    if (!Other || !Owner || Other->WasTrashed() || Other->GetOwningNodeUnchecked() != Owner->Pointer || Other->Direction == P->Direction || !Other->LinkedTo.Contains(P)) { return Stop(Stage, N->GetPathName(), TEXT("reciprocal_link"), TEXT("owned reciprocal opposite pin"), TEXT("invalid")); }
                    Links.Add(Owner->Id.ToString() + TEXT(":") + Other->PinId.ToString());
                }
                Links.Sort(); if (Links != Old->Links) { return Stop(Stage, N->GetPathName(), TEXT("links"), FString::Join(Old->Links, TEXT(",")), FString::Join(Links, TEXT(","))); }
            }
        }
        for (const auto& C : Composites)
        {
            TArray<UBTDecorator*> Tests; TArray<FBTDecoratorLogic> Logic;
            if (!Expression(C,Tests,Logic) || Tests!=C.Tests || !SameOps(Logic,C.Logic)) { return Stop(Stage,C.Graph->GetPathName(),TEXT("ordered_expression"),TEXT("captured ordered public Collect semantics"),TEXT("changed")); }
        }
        return true;
    }
    bool AddObject(UObject* A)
    {
        if (!A || !Hold(A) || !Hold(A->GetClass()) || !Hold(A->GetOuter())) { return false; }
        for (const auto& O : Objects) { if (O.Hold.Get() == A) { return true; } }
        if (Objects.Num() >= 256) { return Stop(TEXT("capture"), A->GetPathName(), TEXT("object_bound"), TEXT("<=256"), TEXT("exceeded")); }
        FObject O; O.Hold.Reset(A); O.Class = A->GetClass(); O.Outer = A->GetOuter(); O.Path = A->GetPathName();
        for (TFieldIterator<FProperty> It(O.Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
        {
            FProperty* P = *It; if (O.Fields.Num() >= MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64 || !Hold(P->GetOwnerStruct())) { return false; }
            FField F; F.Name = P->GetFName(); F.Pointer = P; F.Owner = P->GetOwnerStruct(); F.Class = P->GetClass(); F.Offset = P->GetOffset_ForInternal(); F.Size = P->ElementSize; F.Dim = P->ArrayDim; F.Flags = static_cast<uint64>(P->GetPropertyFlags());
            if (auto* S = CastField<FStructProperty>(P)) { F.Struct = S->Struct; if (!Hold(F.Struct)) { return false; } }
            if (auto* S = CastField<FObjectPropertyBase>(P)) { F.ObjectClass = S->PropertyClass; if (!Hold(F.ObjectClass)) { return false; } }
            if (auto* S = CastField<FClassProperty>(P)) { F.MetaClass = S->MetaClass; if (!Hold(F.MetaClass)) { return false; } }
            O.Fields.Add(MoveTemp(F));
        }
        Classes.Add(O.Class); Objects.Add(MoveTemp(O)); return true;
    }
    bool AddNode(UEdGraphNode* N, UEdGraph* G)
    {
        if (!Loaded(N) || N->GetOuter() != G || !N->NodeGuid.IsValid() || !AddObject(N)) { return Stop(TEXT("capture"), G->GetPathName(), TEXT("node"), TEXT("loaded owned valid GUID"), TEXT("invalid")); }
        for (const auto& E : Nodes) { if (E.Pointer == N || E.Id == N->NodeGuid) { return Stop(TEXT("capture"), N->GetPathName(), TEXT("node_guid"), TEXT("unique"), N->NodeGuid.ToString()); } }
        FNode E; E.Pointer = N; E.Graph = G; E.Id = N->NodeGuid; E.X = N->NodePosX; E.Y = N->NodePosY;
        if (Nodes.Num() >= 128 || N->Pins.Num() > 64) { return false; }
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P || P->WasTrashed() || P->GetOwningNodeUnchecked() != N || !P->PinId.IsValid() || !P->DefaultObject.IsResolved() || !Type(P->PinType) || !Hold(P->DefaultObject.Get()) || P->ParentPin || !P->SubPins.IsEmpty() || P->bOrphanedPin) { return false; }
            FPin V; V.Id = P->PinId; V.Name = P->PinName; V.Direction = P->Direction; V.Type = P->PinType; V.Raw = P->DefaultValue; V.Auto = P->AutogeneratedDefaultValue; V.Text = P->DefaultTextValue; V.Friendly = P->PinFriendlyName;
            V.Default = P->DefaultObject; V.Hidden = P->bHidden; V.NotConnectable = P->bNotConnectable; V.DefaultReadOnly = P->bDefaultValueIsReadOnly; E.Pins.Add(MoveTemp(V));
        }
        Nodes.Add(MoveTemp(E)); return true;
    }
    bool Export(FObject& O, FField& F, FString& Out, const void* Scratch = nullptr)
    {
        if (!Pure(TEXT("before_property_export"))) { return false; }
        if (!FieldCurrent(O, F)) { return Stop(TEXT("before_property_export"), O.Path, F.Name.ToString(), TEXT("captured field binding"), TEXT("changed")); }
        FProperty* P = F.Pointer;
        for (int32 I = 0; I < F.Dim; ++I)
        {
            FString Value; const void* Data = Scratch ? Scratch : P->ContainerPtrToValuePtr<void>(O.Hold.Get(), I);
            const int32 PortFlags = SelectorFilters.Contains(O.Hold.Get()) ? PPF_IncludeTransient : PPF_None;
            const bool OK = P->ExportText_Direct(Value, Data, Data, O.Hold.Get(), PortFlags);
            if (!Pure(TEXT("property_export_return"))) { return false; }
            if (!OK || Value.Len() > MaxText) { return Stop(TEXT("property_export_return"), O.Path, F.Name.ToString(), TEXT("bounded value"), TEXT("export unavailable")); }
            Out += FString::FromInt(Value.Len()) + TEXT(":") + Value;
        }
        return true;
    }
    bool Audit(const TCHAR* Stage)
    {
        if (!Enabled) { return true; } if (!Pure(Stage)) { return false; }
        for (auto& O : Objects) for (auto& F : O.Fields)
        {
            // Node arrays are checked as logical membership above; pin physical order is not a contract.
            if (F.Name == TEXT("Nodes") || F.Name == TEXT("SubGraphs")) { continue; }
            FString Now; if (!Export(O, F, Now)) { return false; }
            const bool Counter = O.Hold.Get() == Graph && F.Name == TEXT("ModCounter");
            const FString& Expected = (F.Blackboard && Assigned) || (F.Selector && Resolved) || (F.Execution && ExecutionRebuilt) || (Counter && AfterUnlock) ? F.Planned : F.Before;
            if (Now != Expected && !(F.Selector && Resolving && Now == F.Planned)) { return Stop(Stage, O.Path, F.Name.ToString(), Expected, Now); }
        }
        TArray<TSharedPtr<FJsonValue>> Typed;
        if (!TypedCurrent(Stage, &Typed) || !FilterReferencesCurrent(Stage)) { return false; }
        R.Data->SetArrayField(TEXT("selector_typed_current"), Typed); R.Data->SetStringField(TEXT("selector_typed_current_stage"), Stage);
        R.Data->SetBoolField(TEXT("selector_cache_resolution_verified"), Resolved);
        if (Resolved) { R.Data->SetArrayField(TEXT("selector_typed_after_resolved"), Typed); }
        TArray<TSharedPtr<FJsonValue>> Execution;
        if (!ExecutionCurrent(Stage,&Execution)) { return false; }
        R.Data->SetArrayField(TEXT("execution_metadata_current"),Execution); R.Data->SetStringField(TEXT("execution_metadata_current_stage"),Stage);
        R.Data->SetBoolField(TEXT("execution_metadata_rebuilt_verified"),ExecutionRebuilt);
        if (ExecutionRebuilt) { R.Data->SetArrayField(TEXT("execution_metadata_after_update_asset"),Execution); }
        TArray<TSharedPtr<FJsonValue>> EditorExecution;
        if (!EditorExecutionCurrent(Stage,&EditorExecution)) { return false; }
        R.Data->SetArrayField(TEXT("editor_execution_metadata_current"),EditorExecution);
        if (ExecutionRebuilt) { R.Data->SetArrayField(TEXT("editor_execution_metadata_after_update_asset"),EditorExecution); }
        return true;
    }
    bool Begin(UBehaviorTree* B, UBlackboardData* BB)
    {
        Tree=B; if (!B->BTGraph.IsResolved() || !B->BlackboardAsset.IsResolved() || !B->RootNode.IsResolved()) { return Stop(TEXT("admission"),TEXT("tree"),TEXT("resolved_slots"),TEXT("resolved graph/BB/root"),TEXT("unresolved")); }
        Graph=Cast<UBehaviorTreeGraph>(B->BTGraph.Get()); if (!Graph) { return true; }
        if (!Loaded(Graph) || Graph->Nodes.Num()>MaxNodes) { return Stop(TEXT("admission"),TEXT("graph"),TEXT("inventory"),TEXT("bounded loaded graph"),TEXT("unavailable")); }
        // Detect the extension without broadening the old non-composite route.
        TArray<UBehaviorTreeGraphNode*> Walk; TSet<UBehaviorTreeGraphNode*> Seen;
        for (auto N : Graph->Nodes) { if (!N.IsResolved() || !Loaded(N.Get())) { return false; } auto* A=Cast<UBehaviorTreeGraphNode>(N.Get()); if (!A) { return false; } Walk.Add(A); }
        for (int32 I=0;I<Walk.Num();++I)
        {
            auto* N=Walk[I]; if (Seen.Contains(N)) { continue; } Seen.Add(N); if (Seen.Num()>MaxNodes) { return false; }
            if (N->GetClass()==UBehaviorTreeGraphNode_CompositeDecorator::StaticClass()) { Enabled=true; }
            const auto AppendChildren=[&](const auto& List)
            {
                for (auto V : List)
                {
                    if (!V.IsResolved() || !Loaded(V.Get())) { return false; }
                    auto* ChildNode=Cast<UBehaviorTreeGraphNode>(V.Get()); if (!ChildNode) { return false; }
                    if (!Seen.Contains(ChildNode) && !Walk.Contains(ChildNode)) { Walk.Add(ChildNode); }
                }
                return true;
            };
            if (!AppendChildren(N->SubNodes) || !AppendChildren(N->Decorators) || !AppendChildren(N->Services)) { return false; }
        }
        if (!Enabled) { return true; } OldBB=B->BlackboardAsset.Get(); Requested=BB?BB:OldBB;
        for (auto* N : Walk)
        {
            if (!N->NodeInstance.IsResolved()) { return Stop(TEXT("admission"),N->GetPathName(),TEXT("NodeInstance"),TEXT("resolved nullable slot"),TEXT("unresolved")); }
            UObject* InitialInstance=N->NodeInstance.Get();
            if (InitialInstance && (!Loaded(InitialInstance) || !Hold(InitialInstance))) { return false; }
            PatrolMixed|=(InitialInstance && PatrolTask(InitialInstance->GetClass())) || N->GetClass()==UBehaviorTreeGraphNode_Decorator::StaticClass();
        }
        if (Graph->Nodes.Num()>64 || Walk.Num()>128) { return Stop(TEXT("admission"),TEXT("graph"),TEXT("composite_inventory"),TEXT("top64/all128 bound"),TEXT("exceeded")); }
        R.Data->SetBoolField(TEXT("native_composite_decorator_contract"),true); R.Data->SetBoolField(TEXT("nested_decorator_graphs_supported"),true);
        R.Data->SetBoolField(TEXT("native_patrol_composite_contract"),PatrolMixed);
        R.Data->SetStringField(TEXT("composite_decorator_scope"),PatrolMixed?TEXT("bounded native Sequence/Selector with Wait/MoveDirectlyToward/RotateToFaceBBEntry; owned Composite logical and plain Blackboard decorators; exact Bool/ObjectActor/Vector/Name Blackboard sources, named selectors including SDK missing-key invalidation; saved missing/filter-rejected selector-name transitions remain pending; no custom/services/parentBB/live execution"):TEXT("bounded normalized native Sequence/Selector/Wait tree; multiple owned closed Sink/AND/OR/NOT Bool expressions; no custom/services/parent Blackboard/nonBool source or live execution"));
        if (Graph->GetClass()!=UBehaviorTreeGraph::StaticClass() || Graph->GetOuter()!=B || Graph->IsLocked() || !Loaded(OldBB) || !Loaded(Requested)
            || !B->RootDecorators.IsEmpty() || !B->RootDecoratorOps.IsEmpty() || !AddObject(B) || !AddObject(Graph) || !Hold(OldBB) || !Hold(Requested)) { return false; }
        LogicClass=FindObject<UClass>(nullptr,TEXT("/Script/BehaviorTreeEditor.BehaviorTreeDecoratorGraphNode_Logic"));
        TestClass=FindObject<UClass>(nullptr,TEXT("/Script/BehaviorTreeEditor.BehaviorTreeDecoratorGraphNode_Decorator"));
        if (!Native(LogicClass,TEXT("/Script/BehaviorTreeEditor")) || !Native(TestClass,TEXT("/Script/BehaviorTreeEditor")) || !Hold(LogicClass) || !Hold(TestClass)) { return false; }
        for (UBlackboardData* SourceBB : { OldBB, Requested })
        {
            if (SourceBB->GetClass() != UBlackboardData::StaticClass() || !SourceBB->Parent.IsResolved() || SourceBB->Parent
                || SourceBB->Keys.Num() < 1 || SourceBB->Keys.Num() > 33 || !AddObject(SourceBB))
            { return Stop(TEXT("admission"), SourceBB->GetPathName(), TEXT("blackboard_source"), TEXT("bounded own Bool keys/no parent plus optional reserved SelfActor"), LexToString(SourceBB->Keys.Num())); }
            FBlackboardRole Captured; Captured.Data=SourceBB; Captured.First=static_cast<int32>(SourceBB->GetFirstKeyID());
            for (const auto& Entry : SourceBB->Keys)
            {
                if (!Entry.KeyType.IsResolved() || !Loaded(Entry.KeyType.Get()) || !Hold(Entry.KeyType.Get())) { return false; }
                Captured.Keys.Add({Entry.EntryName,Entry.EntryName.ToString(),Entry.KeyType.Get(),static_cast<bool>(Entry.bInstanceSynced),static_cast<int32>(SourceBB->GetKeyID(Entry.EntryName)),Entry.KeyType->GetTestOperation()});
            }
            BlackboardRoles.Add(MoveTemp(Captured));
            int32 BoolCount = 0, AuthoredCount = 0, SelfCount = 0; TSet<FName> Names;
            for (const FBlackboardEntry& Key : SourceBB->Keys)
            {
                if (Key.EntryName.IsNone() || Names.Contains(Key.EntryName) || !Key.KeyType.IsResolved() || !Loaded(Key.KeyType.Get()))
                { return Stop(TEXT("admission"), SourceBB->GetPathName(), TEXT("key_declaration"), TEXT("unique named/resolved native key"), TEXT("unavailable")); }
                Names.Add(Key.EntryName);
                if (Key.EntryName == FBlackboard::KeySelf)
                {
                    if (++SelfCount > 1 || Key.KeyType->GetClass() != UBlackboardKeyType_Object::StaticClass() || Key.KeyType->GetOuter() != SourceBB)
                    { return Stop(TEXT("admission"), SourceBB->GetPathName(), TEXT("SelfActor.type_owner"), TEXT("one exact native owned Object key"), TEXT("unsupported")); }
                    auto* SelfType = static_cast<UBlackboardKeyType_Object*>(Key.KeyType.Get());
                    if (!SelfType->BaseClass.IsResolved() || SelfType->BaseClass.Get() != AActor::StaticClass()
                        || !SelfType->DefaultValue.IsResolved() || SelfType->DefaultValue.Get() != nullptr || !Hold(SelfType->BaseClass.Get()))
                    { return Stop(TEXT("admission"), SourceBB->GetPathName(), TEXT("SelfActor.base_default"), TEXT("resolved Actor base/null default"), TEXT("unsupported")); }
                    bool SelfRoleCaptured = false; for (const auto& Role : SelfKeyRoles) { if (Role.Blackboard == SourceBB) { SelfRoleCaptured = true; } }
                    if (!SelfRoleCaptured) { SelfKeyRoles.Add({SourceBB, SelfType, SelfType->BaseClass.Get(), static_cast<int32>(SourceBB->GetKeyID(FBlackboard::KeySelf)), static_cast<bool>(Key.bInstanceSynced)}); }
                }
                else if (PatrolMixed && KeyClass(Key.KeyType->GetClass()))
                {
                    ++AuthoredCount;
                    if (Key.KeyType->GetOuter()!=SourceBB) { return Stop(TEXT("admission"),SourceBB->GetPathName(),TEXT("key_owner"),TEXT("own native key"),Key.KeyType->GetPathName()); }
                    if (Key.KeyType->GetClass()==UBlackboardKeyType_Object::StaticClass())
                    {
                        auto* ObjectKey=static_cast<UBlackboardKeyType_Object*>(Key.KeyType.Get());
                        if (!ObjectKey->BaseClass.IsResolved() || ObjectKey->BaseClass.Get()!=AActor::StaticClass()
                            || !ObjectKey->DefaultValue.IsResolved() || ObjectKey->DefaultValue.Get()!=nullptr || !Hold(ObjectKey->BaseClass.Get()))
                        { return Stop(TEXT("admission"),SourceBB->GetPathName(),TEXT("ObjectKey.role"),TEXT("Actor base/null default as native creator"),TEXT("outside bounded role")); }
                    }
                    SelectorFilters.Add(Key.KeyType.Get()); // reuse resolved-reference preservation; do not rewrite filters
                }
                else if (Key.KeyType->GetClass() == UBlackboardKeyType_Bool::StaticClass()) { ++BoolCount; }
                else { return Stop(TEXT("admission"), SourceBB->GetPathName(), TEXT("authored_key_type"), TEXT("native Bool selectors"), Key.KeyType->GetClass()->GetPathName()); }
                if (!AddObject(Key.KeyType.Get())) { return false; }
            }
            const int32 Authored=PatrolMixed?AuthoredCount:BoolCount;
            if (Authored < 1 || Authored > 32) { return Stop(TEXT("admission"), SourceBB->GetPathName(), TEXT("authored_key_count"), TEXT("1..32 supported own keys"), LexToString(Authored)); }
        }
        R.Data->SetNumberField(TEXT("preserved_reserved_SelfActor_sources"), SelfKeyRoles.Num());
        for (auto N : Graph->Nodes)
        {
            auto* A=static_cast<UBehaviorTreeGraphNode*>(N.Get());
            if (!Native(A->GetClass(),TEXT("/Script/BehaviorTreeEditor")) || !A->NodeInstance.IsResolved() || !A->Services.IsEmpty() || A->bInjectedNode || !AddNode(A,Graph)) { return false; }
            TopNodes.Add(A);
            if (A->GetClass()==UBehaviorTreeGraphNode_Root::StaticClass()) { if (Root || A->NodeInstance || !A->Decorators.IsEmpty() || !A->SubNodes.IsEmpty()) { return false; } Root=static_cast<UBehaviorTreeGraphNode_Root*>(A); continue; }
            UObject* V=A->NodeInstance.Get();
            if (!Loaded(V) || (V->GetClass()!=UBTComposite_Sequence::StaticClass() && V->GetClass()!=UBTComposite_Selector::StaticClass() && !MainTask(V->GetClass()))
                || V->GetOuter()!=B || static_cast<UBTNode*>(V)->GetTreeAsset()!=B || !AddObject(V)) { return Stop(TEXT("admission"),A->GetPathName(),TEXT("runtime_node"),TEXT("normalized exact native Tree-owned Sequence/Selector/Wait"),TEXT("unsupported")); }
            if (A->GetClass()!=(MainTask(V->GetClass())?UBehaviorTreeGraphNode_Task::StaticClass():UBehaviorTreeGraphNode_Composite::StaticClass())) { return false; }
            for (const auto& M : MainRoles) if (M.Instance==V) { return false; }
            if (MainTask(V->GetClass())) { if (!static_cast<UBTTaskNode*>(V)->Services.IsEmpty()) { return false; } }
            else if (!static_cast<UBTCompositeNode*>(V)->Services.IsEmpty()) { return false; }
            FMainRole M; M.Node=A; M.Instance=static_cast<UBTNode*>(V); MainRoles.Add(MoveTemp(M));
            TSet<UAIGraphNode*> Unique;
            for (auto D : A->Decorators)
            {
                if (!D.IsResolved() || !Loaded(D.Get()) || Unique.Contains(D.Get()) || D->bInjectedNode || !D->Services.IsEmpty() || !D->Decorators.IsEmpty() || !D->SubNodes.IsEmpty() || !D->NodeInstance.IsResolved() || !AddNode(D.Get(),Graph)) { return false; }
                Unique.Add(D.Get());
                if (D->GetClass()==UBehaviorTreeGraphNode_Decorator::StaticClass())
                {
                    UObject* Instance=D->NodeInstance.Get();
                    if (!Loaded(Instance) || Instance->GetClass()!=UBTDecorator_Blackboard::StaticClass() || Instance->GetOuter()!=B
                        || static_cast<UBTNode*>(Instance)->GetTreeAsset()!=B || Decorators.Contains(static_cast<UBTDecorator*>(Instance)) || !AddObject(Instance)) { return false; }
                    auto* Decorator=static_cast<UBTDecorator*>(Instance); Decorators.Add(Decorator);
                    PlainDecorators.Add({static_cast<UBehaviorTreeGraphNode_Decorator*>(D.Get()),A,Decorator}); continue;
                }
                if (D->GetClass()!=UBehaviorTreeGraphNode_CompositeDecorator::StaticClass() || D->NodeInstance) { return false; }
                auto* C=static_cast<UBehaviorTreeGraphNode_CompositeDecorator*>(D.Get());
                if (!C->BoundGraph.IsResolved()) { return false; } auto* G=Cast<UBehaviorTreeDecoratorGraph>(C->BoundGraph.Get());
                if (!Loaded(G) || G->GetClass()!=UBehaviorTreeDecoratorGraph::StaticClass() || G->GetOuter()!=C || !Graph->SubGraphs.Contains(G) || !G->SubGraphs.IsEmpty() || G->Nodes.Num()<2 || G->Nodes.Num()>64 || !AddObject(G)) { return false; }
                FCompositeRole Role; Role.Node=C; Role.Host=A; Role.Graph=G;
                for (auto VNode : G->Nodes)
                {
                    if (!VNode.IsResolved() || !Loaded(VNode.Get()) || !AddNode(VNode.Get(),G)) { return false; } auto* InnerNode=VNode.Get(); Role.Members.Add(InnerNode);
                    if (InnerNode->GetClass()==TestClass)
                    {
                        auto* P=FindFProperty<FObjectProperty>(TestClass,TEXT("NodeInstance"));
                        if (!P || P->GetOwnerStruct()!=TestClass || P->ArrayDim!=1 || P->ElementSize!=sizeof(TObjectPtr<UObject>)) { return false; }
                        const auto* Slot=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(InnerNode)); if (!Slot->IsResolved()) { return false; }
                        UObject* Test=Slot->Get(); if (!Loaded(Test) || Test->GetClass()!=UBTDecorator_Blackboard::StaticClass() || Test->GetOuter()!=B || static_cast<UBTNode*>(Test)->GetTreeAsset()!=B || Decorators.Contains(static_cast<UBTDecorator*>(Test)) || !AddObject(Test)) { return false; }
                        Decorators.Add(static_cast<UBTDecorator*>(Test));
                    }
                    else if (InnerNode->GetClass()!=LogicClass) { return false; }
                }
                Composites.Add(MoveTemp(Role));
            }
            if (A->SubNodes.Num()!=A->Decorators.Num()) { return false; } TSet<UAIGraphNode*> SubSeen;
            for (auto D : A->SubNodes) { if (!D.IsResolved() || !Unique.Contains(D.Get()) || SubSeen.Contains(D.Get())) { return false; } SubSeen.Add(D.Get()); }
        }
        if (!Root || Root->BlackboardAsset!=OldBB || Composites.IsEmpty() || Graph->SubGraphs.Num()!=Composites.Num()) { return false; }
        Composite=Composites[0].Node; Inner=Composites[0].Graph;
        // Check every peer by membership before dereferencing it. Link order is
        // later interpreted exactly as the SDK does, never globally canonicalized.
        for (auto& E : Nodes) for (auto& V : E.Pins)
        {
            auto* P=E.Pointer->Pins.FindByPredicate([&](const UEdGraphPin* A){return A && A->PinId==V.Id;}); if (!P || (*P)->LinkedTo.Num()>64) { return false; }
            TSet<const UEdGraphPin*> Unique;
            for (auto* Peer : (*P)->LinkedTo)
            {
                const FNode* Owner=Nodes.FindByPredicate([&](const FNode& A){return A.Graph==E.Graph && A.Pointer->Pins.Contains(Peer);});
                if (!Peer || !Owner || Unique.Contains(Peer) || Peer->WasTrashed() || Peer->GetOwningNodeUnchecked()!=Owner->Pointer || Peer->Direction==(*P)->Direction || !Peer->LinkedTo.Contains(*P)) { return false; }
                Unique.Add(Peer); V.Links.Add(Owner->Id.ToString()+TEXT(":")+Peer->PinId.ToString());
            }
            V.Links.Sort();
        }
        for (auto& C : Composites) if (!Expression(C,C.Tests,C.Logic)) { return Stop(TEXT("admission"),C.Graph->GetPathName(),TEXT("closed_expression"),TEXT("one owned Sink/acyclic AND/OR/NOT tree/all tests reachable once"),TEXT("unsupported")); }
        // The native physical input/output order and exact X-sort determine the
        // child order. Capture normalized source arrays before any writer.
        TSet<UBehaviorTreeGraphNode*> Reachable;
        TFunction<bool(UBehaviorTreeGraphNode*,int32)> WalkMain;
        WalkMain=[&](UBehaviorTreeGraphNode* N,int32 Depth)
        {
            if (!N || Reachable.Contains(N) || Depth>MaxDepth) { return false; } Reachable.Add(N);
            auto* M=Main(N); if (!M) { return false; } int32 Input=0,Output=0;
            for (auto* P : N->Pins)
            {
                if (P->Direction==EGPD_Input) { ++Input; if (P->LinkedTo.Num()!=1) { return false; } continue; } ++Output;
                TArray<UEdGraphPin*> Sorted=P->LinkedTo;
                Sorted.Sort(FCompareNodeXLocation());
                for (auto* Peer : Sorted) { auto* Child=static_cast<UBehaviorTreeGraphNode*>(Peer->GetOwningNodeUnchecked()); if (!Main(Child)) { return false; } M->Children.Add(Child); }
            }
            const bool Wait=MainTask(M->Instance->GetClass());
            if (Input!=1 || Output!=(Wait?0:1) || (!Wait && M->Children.IsEmpty())) { return false; }
            // Merge public Collect outputs in host decorator order, including
            // the SDK's leading implicit AND when several groups coexist.
            bool HasComposite=false;
            for (auto D : N->Decorators)
            {
                const FCompositeRole* C=Composites.FindByPredicate([&](const FCompositeRole& V){return V.Node==D.Get();});
                const FPlainDecorator* Plain=PlainDecorators.FindByPredicate([&](const FPlainDecorator& V){return V.Node==D.Get();});
                if (!C && !Plain) { return false; }
                const int32 Offset=M->Tests.Num();
                if (C)
                {
                    HasComposite=true; M->Tests.Append(C->Tests);
                    for (auto Op : C->Logic) { if (Op.Operation==EBTDecoratorLogic::Test) { Op.Number+=Offset; } M->Logic.Add(Op); }
                }
                else { M->Tests.Add(Plain->Instance); M->Logic.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test,static_cast<uint16>(Offset))); }
            }
            // SDK emits ops only when at least one Composite group exists on this host.
            if (!HasComposite) { M->Logic.Reset(); }
            else if (N->Decorators.Num()>1) { M->Logic.Insert(FBTDecoratorLogic(EBTDecoratorLogic::And,static_cast<uint16>(N->Decorators.Num())),0); }
            if (!Wait)
            {
                const auto* Runtime=static_cast<UBTCompositeNode*>(M->Instance); if (Runtime->Children.Num()!=M->Children.Num()) { return false; }
                for (int32 I=0;I<M->Children.Num();++I)
                {
                    auto* C=Main(M->Children[I]); const auto& ExistingChild=Runtime->Children[I];
                    if (!C || !ExistingChild.ChildTask.IsResolved() || !ExistingChild.ChildComposite.IsResolved() || (MainTask(C->Instance->GetClass())?ExistingChild.ChildTask.Get()!=C->Instance || ExistingChild.ChildComposite!=nullptr:ExistingChild.ChildComposite.Get()!=C->Instance || ExistingChild.ChildTask!=nullptr)) { return false; }
                    if (!WalkMain(C->Node,Depth+1)) { return false; }
                    if (ExistingChild.Decorators.Num()!=C->Tests.Num() || !SameOps(ExistingChild.DecoratorOps,C->Logic)) { return false; }
                    for (int32 K=0;K<ExistingChild.Decorators.Num();++K) if (!ExistingChild.Decorators[K].IsResolved() || ExistingChild.Decorators[K].Get()!=C->Tests[K]) { return false; }
                }
            }
            return true;
        };
        if (Root->Pins.Num()!=1 || Root->Pins[0]->Direction!=EGPD_Output || Root->Pins[0]->LinkedTo.Num()!=1) { return false; }
        auto* RootMain=static_cast<UBehaviorTreeGraphNode*>(Root->Pins[0]->LinkedTo[0]->GetOwningNodeUnchecked()); auto* M=Main(RootMain);
        if (!M || M->Instance!=B->RootNode.Get() || MainTask(M->Instance->GetClass()) || !M->Node->Decorators.IsEmpty() || !WalkMain(RootMain,0) || Reachable.Num()!=MainRoles.Num()) { return Stop(TEXT("admission"),B->GetPathName(),TEXT("normalized_topology"),TEXT("one closed native runtime-matching Sequence/Selector/Wait tree"),TEXT("unsupported or rebuild needed")); }
        for (TFieldIterator<FProperty> It(FBlackboardKeySelector::StaticStruct(), EFieldIteratorFlags::IncludeSuper); It; ++It)
        { FProperty* P = *It; FField F; F.Name=P->GetFName(); F.Pointer=P; F.Owner=P->GetOwnerStruct(); F.Class=P->GetClass(); F.Offset=P->GetOffset_ForInternal(); F.Size=P->ElementSize; F.Dim=P->ArrayDim; F.Flags=static_cast<uint64>(P->GetPropertyFlags());
            if (auto* C=CastField<FObjectPropertyBase>(P)) { F.ObjectClass=C->PropertyClass; if (!Hold(F.ObjectClass)) { return false; } }
            if (auto* C=CastField<FClassProperty>(P)) { F.MetaClass=C->MetaClass; if (!Hold(F.MetaClass)) { return false; } }
            SelectorFields.Add(MoveTemp(F)); }
        if (!BindSelectorCache()) { return false; }
        TArray<UBTNode*> SelectorNodes; for (auto* D : Decorators) { SelectorNodes.Add(D); }
        for (const auto& MRole : MainRoles) if (PatrolTask(MRole.Instance->GetClass())) { SelectorNodes.Add(MRole.Instance); }
        for (auto* D : SelectorNodes)
        {
            const FObject* O=Objects.FindByPredicate([&](const FObject& V){return V.Hold.Get()==D;});
            const FField* F=O?O->Fields.FindByPredicate([](const FField& V){return V.Name==TEXT("BlackboardKey");}):nullptr;
            if (!O || !F || !SelectorProperty(*O,*F)) { return Stop(TEXT("admission"),D->GetPathName(),TEXT("BlackboardKey.declaring_owner"),TEXT("exact native task/decorator base selector descriptor"),TEXT("unsupported or changed")); }
            auto* P=static_cast<FStructProperty*>(F->Pointer);
            const FBlackboardKeySelector& Key = *P->ContainerPtrToValuePtr<FBlackboardKeySelector>(D);
            if (Key.AllowedTypes.Num() > 8) { return false; }
            for (auto V : Key.AllowedTypes) { if (!V.IsResolved() || !Loaded(V.Get()) || !Native(V->GetClass(), TEXT("/Script/AIModule")) || !AddObject(V.Get())) { return false; } SelectorFilters.Add(V.Get()); }
        }
        if (!CaptureFilterReferences()) { return false; }
        // Existing native defaults only. Never create a CDO to satisfy this profile.
        const TArray<UClass*> SourceClasses = Classes.Array();
        for (UClass* C : SourceClasses)
        {
            if (!Loaded(C) || !C->ClassDefaultObject.IsResolved()) { return false; }
            UObject* D = C->ClassDefaultObject.Get(); if (!Hold(D)) { return false; }
            if (D && (D->GetClass() != C || D->GetOuter() != C->GetOuter() || !D->HasAnyFlags(RF_ClassDefaultObject) || !AddObject(D))) { return false; }
            NativeClasses.Add({ C, D, static_cast<uint32>(C->GetClassFlags()) });
        }
        Ready = true;
        // Capture complete reflected cells. Only Blackboard slots and the selector's resolved cache receive a plan.
        for (auto& O : Objects) for (auto& F : O.Fields)
        {
            if (F.Name == TEXT("Nodes") || F.Name == TEXT("SubGraphs")) { continue; }
            if (!Export(O, F, F.Before)) { return false; } F.Planned = F.Before;
        }
        for (auto& O : Objects) for (auto& F : O.Fields)
        {
            if (F.Name == TEXT("Nodes") || F.Name == TEXT("SubGraphs")) { continue; }
            if (O.Hold.Get() == Graph && F.Name == TEXT("ModCounter"))
            {
                if (Graph->ModCounter == MAX_int32 || !CastField<FIntProperty>(F.Pointer)) { return false; }
                const int32 Next = Graph->ModCounter + 1; F.Planned.Reset(); if (!Export(O, F, F.Planned, &Next)) { return false; }
            }
            if ((O.Hold.Get() == B || O.Hold.Get() == Root) && F.Name == TEXT("BlackboardAsset"))
            {
                FObjectProperty* P = CastField<FObjectProperty>(F.Pointer); if (!P) { return false; }
                TObjectPtr<UBlackboardData> Value(Requested); F.Planned.Reset(); if (!Export(O, F, F.Planned, &Value)) { return false; } F.Blackboard = true;
            }
            if (PatrolMixed && !O.Hold->HasAnyFlags(RF_ClassDefaultObject) && SelectorOwner(O.Class) && F.Name==TEXT("BlackboardKey"))
            {
                if (!SelectorProperty(O,F)) { return false; } auto* P=static_cast<FStructProperty*>(F.Pointer);
                if (!CaptureTypedBefore(O.Hold.Get(),P,O.Path)) { return false; }
                FBlackboardKeySelector Copy=*P->ContainerPtrToValuePtr<FBlackboardKeySelector>(O.Hold.Get());
                if (Copy.SelectedKeyName.IsNone()) { return Stop(TEXT("admission"),O.Path,TEXT("selector_name"),TEXT("existing explicit key name; auto-selection remains pending"),TEXT("None")); }
                const FBlackboard::FKey Id=Requested->GetKeyID(Copy.SelectedKeyName);
                if (!Pure(TEXT("mixed_key_id_return"))) { return false; }
                TSubclassOf<UBlackboardKeyType> KeyType=Requested->GetKeyType(Id);
                if (!KeyType.GetGCPtr().IsResolved()) { return Stop(TEXT("mixed_key_type_return"),O.Path,TEXT("key_type"),TEXT("resolved nullable native class"),TEXT("unresolved")); }
                UClass* PlannedType=KeyType.Get(); if (!Hold(PlannedType) || !Pure(TEXT("mixed_key_type_return"))) { return false; }
                if (PlannedType && !KeyClass(PlannedType)) { return Stop(TEXT("admission"),O.Path,TEXT("selected_key_type"),TEXT("native Bool/Object/Vector/Name or missing-key invalidation"),PlannedType->GetPathName()); }
                // SDK resolves a named key without applying AllowedTypes; missing keys remain named with Invalid/null cache.
                Copy.ResolveSelectedKey(*Requested); if (!Pure(TEXT("mixed_scratch_resolve_return"))) { return false; }
                if (!CaptureTypedPlan(O.Hold.Get(),Copy,static_cast<int32>(Id),PlannedType)) { return false; }
                F.Planned.Reset(); if (!Export(O,F,F.Planned,&Copy)) { return false; } F.Selector=true;
                if (O.Class==UBTDecorator_Blackboard::StaticClass() && !PlanMixedDescription(O,Copy,PlannedType)) { return false; }
                auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("object"),O.Path); Row->SetStringField(TEXT("key"),Copy.SelectedKeyName.ToString());
                Row->SetStringField(TEXT("declaring_owner"),F.Owner->GetPathName()); Row->SetNumberField(TEXT("requested_id"),static_cast<int32>(Id));
                Row->SetStringField(TEXT("requested_type"),PlannedType?PlannedType->GetPathName():FString()); Row->SetBoolField(TEXT("sdk_missing_key_invalidated"),Id==FBlackboard::InvalidKey);
                R.Data->SetObjectField(TEXT("selector_plan_")+FString::FromInt(PlanCount++),Row);
            }
            else if (!O.Hold->HasAnyFlags(RF_ClassDefaultObject) && O.Class == UBTDecorator_Blackboard::StaticClass() && F.Name == TEXT("BlackboardKey"))
            {
                FStructProperty* P = CastField<FStructProperty>(F.Pointer); if (!P || P->Struct != FBlackboardKeySelector::StaticStruct() || F.Size != sizeof(FBlackboardKeySelector) || F.Dim != 1) { return false; }
                if (!CaptureTypedBefore(O.Hold.Get(), P, O.Path)) { return false; }
                FBlackboardKeySelector Copy = *P->ContainerPtrToValuePtr<FBlackboardKeySelector>(O.Hold.Get());
                if (Copy.SelectedKeyName.IsNone() || !Copy.SelectedKeyType.GetGCPtr().IsResolved() || !Hold(Copy.SelectedKeyType.Get())) { return false; }
                const FSelectorValue& ActualBefore = TypedSelectors.Last().Before;
                const bool ConstructorCache = ActualBefore.Type == nullptr && ActualBefore.Id == static_cast<int32>(FBlackboard::InvalidKey) && ActualBefore.GetterId == static_cast<int32>(FBlackboard::InvalidKey);
                if (Copy.SelectedKeyType != UBlackboardKeyType_Bool::StaticClass() && !ConstructorCache) { return false; }
                if (ConstructorCache)
                {
                    // Preserve the actual null/Invalid pair. Source declaration, not an invented cached type, admits it.
                    const FBlackboard::FKey SourceId = OldBB->GetKeyID(Copy.SelectedKeyName);
                    if (!Pure(TEXT("ctor_source_key_id_return")) || SourceId == FBlackboard::InvalidKey) { return false; }
                    TSubclassOf<UBlackboardKeyType> SourceType = OldBB->GetKeyType(SourceId);
                    if (!SourceType.GetGCPtr().IsResolved()) { return Stop(TEXT("ctor_source_key_type_return"), O.Path, TEXT("source_key_type"), TEXT("resolved native Bool class"), TEXT("unresolved")); }
                    UClass* SourceClass = SourceType.Get();
                    if (!Hold(SourceClass) || !Pure(TEXT("ctor_source_key_type_return")) || SourceClass != UBlackboardKeyType_Bool::StaticClass()) { return false; }
                }
                for (auto V : Copy.AllowedTypes) { if (!V.IsResolved() || !Hold(V.Get())) { return false; } }
                const FBlackboard::FKey Id = Requested->GetKeyID(Copy.SelectedKeyName); if (!Pure(TEXT("key_id_return")) || Id == FBlackboard::InvalidKey) { return false; }
                TSubclassOf<UBlackboardKeyType> KeyType = Requested->GetKeyType(Id);
                if (!Hold(KeyType.Get()) || !Pure(TEXT("key_type_return")) || KeyType != UBlackboardKeyType_Bool::StaticClass()) { return Stop(TEXT("admission"), O.Path, TEXT("selected_key"), TEXT("requested native Bool key by existing name"), Copy.SelectedKeyName.ToString()); }
                Copy.ResolveSelectedKey(*Requested); if (!Pure(TEXT("scratch_resolve_return"))) { return false; }
                if (!CaptureTypedPlan(O.Hold.Get(), Copy, static_cast<int32>(Id), KeyType.Get())) { return false; }
                F.Planned.Reset(); if (!Export(O, F, F.Planned, &Copy)) { return false; } F.Selector = true;
                auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("object"), O.Path); Row->SetStringField(TEXT("key"), Copy.SelectedKeyName.ToString()); Row->SetNumberField(TEXT("requested_id"), static_cast<int32>(Id)); Row->SetStringField(TEXT("requested_type"), KeyType->GetPathName());
                R.Data->SetObjectField(TEXT("selector_plan_") + FString::FromInt(PlanCount++), Row);
            }
        }
        if (PlanCount!=SelectorNodes.Num() || TypedSelectors.Num()!=SelectorNodes.Num()) { return false; }
        PublishTypedPlan();
        for (auto& C : Composites)
        {
            TArray<UBTDecorator*> Collected; TArray<FBTDecoratorLogic> Actual;
            C.Node->CollectDecoratorData(Collected,Actual); for (auto* D : Collected) if (!Hold(D)) { return false; }
            if (!Audit(TEXT("collect_decorator_data_return")) || Collected!=C.Tests || !SameOps(Actual,C.Logic)) { return Stop(TEXT("collect_decorator_data_return"),C.Graph->GetPathName(),TEXT("ordered_collect"),TEXT("planned held test/operation sequence"),TEXT("changed")); }
        }
        for (const auto& D : PlainDecorators)
        {
            TArray<UBTDecorator*> Collected; TArray<FBTDecoratorLogic> Actual;
            D.Node->CollectDecoratorData(Collected,Actual); for (auto* V : Collected) if (!Hold(V)) { return false; }
            TArray<FBTDecoratorLogic> Expected; Expected.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test,0));
            if (!Audit(TEXT("collect_plain_decorator_return")) || Collected.Num()!=1 || Collected[0]!=D.Instance || !SameOps(Actual,Expected))
            { return Stop(TEXT("collect_plain_decorator_return"),D.Node->GetPathName(),TEXT("ordered_collect"),TEXT("own exact Blackboard decorator/Test0"),TEXT("changed")); }
        }
        if (!PlanExecution()) { return false; } R.Data->SetNumberField(TEXT("composite_selector_count"),PlanCount); R.Data->SetNumberField(TEXT("composite_graph_count"),Composites.Num()); return true;
    }
    int32 PlanCount = 0;
    bool Apply()
    {
        R.Stage = TEXT("assign_blackboard");
        if (!Audit(TEXT("before_assignment"))) { return false; }
        R.Data->SetBoolField(TEXT("graph_reused"), true); R.bAttempted = true; R.Data->SetBoolField(TEXT("blackboard_assignment_attempted"), true);
        Tree->Modify(); if (!Audit(TEXT("tree_modify_return"))) { return false; }
        Graph->Modify(); if (!Audit(TEXT("graph_modify_return"))) { return false; }
        Root->Modify(); if (!Audit(TEXT("root_modify_return"))) { return false; }
        Root->BlackboardAsset = Requested; Tree->BlackboardAsset = Requested; Assigned = true;
        if (!Audit(TEXT("slots_assigned"))) { return false; }
        Resolving = true; R.Data->SetNumberField(TEXT("direct_update_blackboard_calls"), 1); Graph->UpdateBlackboardChange();
        Resolved = true; Resolving = false; if (!Audit(TEXT("update_blackboard_return"))) { return false; }
        R.Data->SetNumberField(TEXT("direct_update_asset_calls"), 1); R.Data->SetBoolField(TEXT("execution_update_asset_entered"),true);
        Graph->UpdateAsset(UBehaviorTreeGraph::KeepRebuildCounter);
        R.Data->SetBoolField(TEXT("execution_update_asset_returned"),true); ExecutionRebuilt=true;
        if (!Audit(TEXT("update_asset_return"))) { return false; }
        Tree->BlackboardAsset = Requested;
        UBTCompositeNode* RuntimeRoot = Tree->RootNode; if (!Hold(RuntimeRoot) || !Pure(TEXT("root_slot_current"))) { return false; }
        if (RuntimeRoot) { R.Data->SetNumberField(TEXT("direct_initialize_root_calls"), 1); RuntimeRoot->InitializeFromAsset(*Tree); if (!Audit(TEXT("initialize_root_return"))) { return false; } }
        Tree->MarkPackageDirty(); if (!Audit(TEXT("tree_dirty_return"))) { return false; }
        Graph->MarkPackageDirty(); return Audit(TEXT("graph_dirty_return"));
    }
    TSharedRef<FJsonObject> Snapshot() const
    {
        auto J = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Rows;
        J->SetStringField(TEXT("graph"), Inner->GetPathName()); J->SetStringField(TEXT("composite"), Composite->GetPathName());
        for (const auto& E : Nodes) if (E.Graph == Inner)
        {
            auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("guid"), E.Id.ToString()); Row->SetStringField(TEXT("path"), E.Pointer->GetPathName()); Row->SetStringField(TEXT("class"), E.Pointer->GetClass()->GetPathName());
            TArray<TSharedPtr<FJsonValue>> Pins; for (const auto& P : E.Pins)
            { auto Pin = MakeShared<FJsonObject>(); Pin->SetStringField(TEXT("guid"), P.Id.ToString()); Pin->SetStringField(TEXT("name"), P.Name.ToString()); Pin->SetNumberField(TEXT("direction"), P.Direction); Pin->SetStringField(TEXT("raw"), P.Raw); TArray<TSharedPtr<FJsonValue>> Links; for (const auto& L : P.Links) { Links.Add(MakeShared<FJsonValueString>(L)); } Pin->SetArrayField(TEXT("links"), Links); Pins.Add(MakeShared<FJsonValueObject>(Pin)); }
            Row->SetArrayField(TEXT("pins"), Pins); Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        J->SetArrayField(TEXT("nodes"), Rows);
        TArray<TSharedPtr<FJsonValue>> Graphs;
        for (const auto& C : Composites)
        {
            auto Group=MakeShared<FJsonObject>(); Group->SetStringField(TEXT("graph"),C.Graph->GetPathName()); Group->SetStringField(TEXT("composite"),C.Node->GetPathName());
            TArray<TSharedPtr<FJsonValue>> Members; for (auto* N : C.Members) { Members.Add(MakeShared<FJsonValueString>(N->NodeGuid.ToString())); } Group->SetArrayField(TEXT("node_guids"),Members);
            TArray<TSharedPtr<FJsonValue>> Logic; for (const auto& Op : C.Logic) { auto Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("operation"),static_cast<uint8>(Op.Operation)); Row->SetNumberField(TEXT("number"),Op.Number); Logic.Add(MakeShared<FJsonValueObject>(Row)); } Group->SetArrayField(TEXT("ordered_logic"),Logic); Graphs.Add(MakeShared<FJsonValueObject>(Group));
        }
        J->SetArrayField(TEXT("graphs"),Graphs); return J;
    }
};
FRDComposite* RDActive = nullptr;
bool RDCurrent(const TCHAR* Stage) { return !RDActive || !RDActive->Enabled || RDActive->Audit(Stage); }
bool RDField(UObject* O, FProperty* P, FName Name)
{
    if (!RDActive || !RDActive->Enabled) { return true; }
    if (!RDActive->Pure(TEXT("export_source_current"))) { return false; }
    for (const auto& E : RDActive->Objects) if (E.Hold.Get() == O)
    { for (const auto& F : E.Fields) if (F.Pointer == P) { return RDActive->FieldCurrent(E, F); } return false; }
    // Native source rows are separately compared by Observe; reject lost descriptor membership before any retained field access.
    return Loaded(O) && P && FindFProperty<FProperty>(O->GetClass(), Name) == P;
}

bool GraphState(UBehaviorTree* B, const FTraversal& Runtime, TSharedRef<FJsonObject>& J, TSharedRef<FJsonObject>& Topology, TSet<UClass*>& Classes)
{
    J->SetBoolField(TEXT("exists"), B->BTGraph != nullptr); if (!B->BTGraph) { return true; }
    auto* G = Cast<UBehaviorTreeGraph>(B->BTGraph);
    if (!Loaded(G) || G->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || G->GetClass() != UBehaviorTreeGraph::StaticClass() || G->GetOuter() != B || G->Schema != UEdGraphSchema_BehaviorTree::StaticClass()
        || G->RootNodeClass != UBehaviorTreeGraphNode_Root::StaticClass() || G->IsLocked() || ((!RDActive || !RDActive->Enabled) && G->SubGraphs.Num() != 0) || G->Nodes.Num() < 1 || G->Nodes.Num() > MaxNodes) { return false; }
    J->SetObjectField(TEXT("identity"), Identity(G)); J->SetStringField(TEXT("schema"), G->Schema->GetPathName()); J->SetNumberField(TEXT("mod_counter"), G->ModCounter); J->SetBoolField(TEXT("locked"), G->IsLocked());
    auto GP = MakeShared<FJsonObject>(); if (!Properties(G, { TEXT("Nodes"), TEXT("SubGraphs") }, GP)) { return false; } J->SetObjectField(TEXT("properties"), GP);
    TArray<UBehaviorTreeGraphNode*> Nodes; TSet<UBehaviorTreeGraphNode*> Seen; TSet<FGuid> Guids; TSet<const UEdGraphPin*> AllPins; int32 RootCount = 0, Links = 0;
    for (UEdGraphNode* N : G->Nodes) { auto* Member = Cast<UBehaviorTreeGraphNode>(N); if (!Member || Seen.Contains(Member)) { return false; } Seen.Add(Member); Nodes.Add(Member); }
    for (int32 I = 0; I < Nodes.Num(); ++I)
    {
        auto* N = Nodes[I]; if (!Loaded(N) || N->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || N->GetOuter() != G || !Native(N->GetClass(), TEXT("/Script/BehaviorTreeEditor")) || (N->GetClass()->GetName() == TEXT("BehaviorTreeGraphNode_CompositeDecorator") && (!RDActive || !RDActive->Enabled || !RDActive->IsComposite(N))) || !N->NodeGuid.IsValid() || Guids.Contains(N->NodeGuid)) { return false; }
        Guids.Add(N->NodeGuid); Classes.Add(N->GetClass()); if (N->GetClass() == UBehaviorTreeGraphNode_Root::StaticClass()) { ++RootCount; if (N->NodeInstance || N->Pins.Num() != 1 || !N->Pins[0] || N->Pins[0]->Direction != EGPD_Output || N->Pins[0]->LinkedTo.Num() > 1) { return false; } }
        else if (RDActive && RDActive->Enabled && RDActive->IsComposite(N)) { if (N->NodeInstance) { return false; } }
        else if (!N->NodeInstance || !Runtime.Seen.Contains(Cast<UBTNode>(N->NodeInstance))) { return false; }
        const auto Add = [&](const auto& List) { if (List.Num() > MaxNodes) { return false; } for (auto Sub : List) { auto* Child = Cast<UBehaviorTreeGraphNode>(Sub); if (!Child) { return false; } if (!Seen.Contains(Child)) { if (Seen.Num() >= MaxNodes) { return false; } Seen.Add(Child); Nodes.Add(Child); } } return true; };
        if (!Add(N->SubNodes) || !Add(N->Decorators) || !Add(N->Services)) { return false; }
        for (UEdGraphPin* Pin : N->Pins) { if (!Pin || Pin->GetOwningNodeUnchecked() != N || !Pin->PinId.IsValid() || Pin->bOrphanedPin || AllPins.Contains(Pin) || AllPins.Num() >= MaxPins) { return false; } AllPins.Add(Pin); }
    }
    if (RootCount != 1) { return false; }
    // UpdateAsset recursively follows these links. Reject cycles / excessive graph depth before invoking that callback.
    TMap<const UEdGraphNode*, int32> InDegree, Depths; TMap<const UEdGraphNode*, TArray<const UEdGraphNode*>> Outgoing;
    for (auto* N : Nodes) { InDegree.Add(N, 0); Depths.Add(N, 0); }
    for (auto* N : Nodes)
    {
        for (UEdGraphPin* Pin : N->Pins)
        {
            if (Pin->Direction != EGPD_Output) { continue; }
            for (const UEdGraphPin* Other : Pin->LinkedTo)
            {
                if (!Other || !AllPins.Contains(Other) || Other->Direction != EGPD_Input || !Other->LinkedTo.Contains(Pin)) { return false; }
                const auto* Target = Other->GetOwningNodeUnchecked(); if (!InDegree.Contains(Target)) { return false; }
                Outgoing.FindOrAdd(N).Add(Target); ++InDegree.FindChecked(Target);
            }
        }
    }
    TArray<const UEdGraphNode*> Queue; for (auto* N : Nodes) { if (InDegree.FindChecked(N) == 0) { Queue.Add(N); } }
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        const auto* N = Queue[I]; const auto* Next = Outgoing.Find(N); if (!Next) { continue; }
        for (const auto* Target : *Next) { Depths.FindChecked(Target) = FMath::Max(Depths.FindChecked(Target), Depths.FindChecked(N) + 1); if (Depths.FindChecked(Target) > MaxDepth) { return false; } if (--InDegree.FindChecked(Target) == 0) { Queue.Add(Target); } }
    }
    if (Queue.Num() != Nodes.Num()) { return false; } TArray<TSharedPtr<FJsonValue>> Rows, Structural;
    for (auto* N : Nodes)
    {
        auto Row = Identity(N), Stable = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("node_guid"), N->NodeGuid.ToString()); Row->SetNumberField(TEXT("x"), N->NodePosX); Row->SetNumberField(TEXT("y"), N->NodePosY);
        Row->SetStringField(TEXT("node_instance"), N->NodeInstance ? N->NodeInstance->GetPathName() : TEXT("")); auto NP = MakeShared<FJsonObject>(); if (!Properties(N, {}, NP)) { return false; } Row->SetObjectField(TEXT("properties"), NP);
        for (const TCHAR* K : { TEXT("path"), TEXT("class"), TEXT("outer"), TEXT("node_guid"), TEXT("node_instance") }) { Stable->SetStringField(K, Row->GetStringField(K)); } Stable->SetNumberField(TEXT("x"), N->NodePosX); Stable->SetNumberField(TEXT("y"), N->NodePosY);
        for (const auto& Pair : { TPair<FString, const TArray<TObjectPtr<UBehaviorTreeGraphNode>>*>(TEXT("decorators"), &N->Decorators), TPair<FString, const TArray<TObjectPtr<UBehaviorTreeGraphNode>>*>(TEXT("services"), &N->Services) })
        { TArray<TSharedPtr<FJsonValue>> Paths; for (auto Child : *Pair.Value) { Paths.Add(MakeShared<FJsonValueString>(Child->GetPathName())); } Row->SetArrayField(Pair.Key, Paths); Stable->SetArrayField(Pair.Key, Paths); }
        TArray<TSharedPtr<FJsonValue>> SubPaths; for (auto Sub : N->SubNodes) { SubPaths.Add(MakeShared<FJsonValueString>(Sub->GetPathName())); } Stable->SetArrayField(TEXT("subnodes"), SubPaths);
        if (auto* Root = Cast<UBehaviorTreeGraphNode_Root>(N)) { Row->SetStringField(TEXT("blackboard"), Root->BlackboardAsset ? Root->BlackboardAsset->GetPathName() : TEXT("")); J->SetStringField(TEXT("blackboard"), Row->GetStringField(TEXT("blackboard"))); J->SetStringField(TEXT("root_node_path"), N->GetPathName()); }
        TArray<TSharedPtr<FJsonValue>> Pins; TSet<FGuid> PinIds;
        for (UEdGraphPin* Pin : N->Pins)
        {
            if (PinIds.Contains(Pin->PinId) || Pin->LinkedTo.Num() > MaxNodes) { return false; } PinIds.Add(Pin->PinId);
            auto PR = MakeShared<FJsonObject>(); PR->SetStringField(TEXT("id"), Pin->PinId.ToString()); PR->SetStringField(TEXT("name"), Pin->PinName.ToString()); PR->SetStringField(TEXT("friendly_name"), Pin->PinFriendlyName.ToString()); PR->SetNumberField(TEXT("direction"), Pin->Direction);
            PR->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString()); PR->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString()); PR->SetStringField(TEXT("type_object"), Pin->PinType.PinSubCategoryObject.IsValid() ? Pin->PinType.PinSubCategoryObject->GetPathName() : TEXT(""));
            PR->SetNumberField(TEXT("container"), static_cast<int32>(Pin->PinType.ContainerType)); PR->SetBoolField(TEXT("reference"), Pin->PinType.bIsReference); PR->SetBoolField(TEXT("const"), Pin->PinType.bIsConst); PR->SetStringField(TEXT("default_value"), Pin->DefaultValue);
            PR->SetStringField(TEXT("default_object"), Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : TEXT("")); PR->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString()); PR->SetBoolField(TEXT("hidden"), Pin->bHidden);
            TArray<TSharedPtr<FJsonValue>> Connections; TSet<const UEdGraphPin*> Unique;
            for (const UEdGraphPin* Other : Pin->LinkedTo)
            {
                if (!Other || !AllPins.Contains(Other) || Other == Pin || Unique.Contains(Other) || Other->Direction == Pin->Direction || !Other->LinkedTo.Contains(Pin) || ++Links > MaxLinks) { return false; }
                Unique.Add(Other); Connections.Add(MakeShared<FJsonValueString>(Other->GetOwningNodeUnchecked()->NodeGuid.ToString() + TEXT(":") + Other->PinId.ToString()));
            }
            if (RDActive && RDActive->Enabled) { Connections.Sort([](const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B){return A->AsString()<B->AsString();}); }
            PR->SetArrayField(TEXT("links"), Connections); Pins.Add(MakeShared<FJsonValueObject>(PR));
        }
        Row->SetArrayField(TEXT("pins"), Pins); Stable->SetArrayField(TEXT("pins"), Pins); Rows.Add(MakeShared<FJsonValueObject>(Row)); Structural.Add(MakeShared<FJsonValueObject>(Stable));
    }
    J->SetNumberField(TEXT("top_level_node_count"), G->Nodes.Num()); J->SetNumberField(TEXT("all_node_count"), Nodes.Num()); J->SetNumberField(TEXT("physical_connection_count"), Links / 2); J->SetArrayField(TEXT("nodes"), Rows);
    if (RDActive && RDActive->Enabled) { if (!RDCurrent(TEXT("nested_graph_snapshot"))) { return false; } auto Nested = RDActive->Snapshot(); J->SetObjectField(TEXT("nested_decorator_graph"), Nested); Topology->SetObjectField(TEXT("nested_decorator_graph"), Nested); Classes.Append(RDActive->Classes); }
    Topology->SetStringField(TEXT("path"), G->GetPathName()); Topology->SetArrayField(TEXT("nodes"), Structural); TArray<TSharedPtr<FJsonValue>> TopNodes; for (UEdGraphNode* N : G->Nodes) { TopNodes.Add(MakeShared<FJsonValueString>(N->GetPathName())); } Topology->SetArrayField(TEXT("top_level_nodes"), TopNodes); return true;
}
struct FObservation
{
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), Sources = MakeShared<FJsonObject>(), References = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>();
};
bool Observe(UBehaviorTree* B, const FContext& Context, FObservation& O, FResult& R)
{
    if (!RDCurrent(TEXT("observe_begin")) || !Loaded(B) || B->GetClass() != UBehaviorTree::StaticClass() || B->GetOuter() != B->GetOutermost()) { return false; }
    O.State = Identity(B); O.State->SetStringField(TEXT("asset_type"), TEXT("behavior_tree")); O.State->SetStringField(TEXT("asset_path"), B->GetPathName()); O.State->SetBoolField(TEXT("package_dirty"), B->GetOutermost()->IsDirty());
    O.State->SetStringField(TEXT("blackboard_path"), B->BlackboardAsset ? B->BlackboardAsset->GetPathName() : TEXT(""));
    for (const auto& Ref : Context.Blackboards) { auto J = MakeShared<FJsonObject>(); if (!BlackboardObservation(Ref.Get(), J, R)) { return false; } if (!RDCurrent(TEXT("blackboard_inspection_return"))) { return false; } O.References->SetObjectField(Ref->GetPathName(), J); }
    auto P = MakeShared<FJsonObject>(); if (!Properties(B, { TEXT("RootNode"), TEXT("RootDecorators"), TEXT("RootDecoratorOps"), TEXT("BTGraph") }, P)) { return false; } O.State->SetObjectField(TEXT("other_properties"), P);
    FTraversal T{ B }; T.Classes = FixedClasses(); auto Root = MakeShared<FJsonObject>();
    if (B->RootNode && !RuntimeNode(B->RootNode, T, 0, Root)) { return false; }
    TArray<TSharedPtr<FJsonValue>> RootDecorators, RootOps, RootDecoratorPaths;
    if (B->RootDecorators.Num() > MaxNodes || B->RootDecoratorOps.Num() > MaxNodes) { return false; }
    for (auto D : B->RootDecorators) { auto Row = MakeShared<FJsonObject>(); if (!RuntimeNode(D, T, 1, Row)) { return false; } RootDecorators.Add(MakeShared<FJsonValueObject>(Row)); RootDecoratorPaths.Add(MakeShared<FJsonValueString>(D->GetPathName())); }
    for (const auto& Op : B->RootDecoratorOps) { auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("operation"), static_cast<int32>(Op.Operation)); Row->SetNumberField(TEXT("number"), Op.Number); RootOps.Add(MakeShared<FJsonValueObject>(Row)); }
    O.State->SetObjectField(TEXT("runtime_root"), Root); O.State->SetObjectField(TEXT("runtime_objects"), T.Objects); O.State->SetArrayField(TEXT("root_decorators"), RootDecorators); O.State->SetArrayField(TEXT("root_decorator_ops"), RootOps);
    auto Graph = MakeShared<FJsonObject>(), GraphTopology = MakeShared<FJsonObject>(), Topology = MakeShared<FJsonObject>();
    if (!GraphState(B, T, Graph, GraphTopology, T.Classes) || !NativeSources(T.Classes, O.Sources) || !Disk(B->GetOutermost()->GetName(), O.DiskState) || !Dirty(B->GetOutermost()->GetName(), O.OtherDirty)) { return false; }
    O.State->SetObjectField(TEXT("graph"), Graph); Topology->SetObjectField(TEXT("graph"), GraphTopology); Topology->SetObjectField(TEXT("runtime_objects"), T.Topology);
    Topology->SetStringField(TEXT("runtime_root"), B->RootNode ? B->RootNode->GetPathName() : TEXT("")); Topology->SetArrayField(TEXT("root_decorators"), RootDecoratorPaths); Topology->SetArrayField(TEXT("root_decorator_ops"), RootOps); O.State->SetObjectField(TEXT("topology"), Topology);
    O.State->SetBoolField(TEXT("root_node_loaded"), B->RootNode != nullptr); O.State->SetStringField(TEXT("root_node_class"), B->RootNode ? B->RootNode->GetClass()->GetName() : TEXT("")); O.State->SetStringField(TEXT("root_node_name"), B->RootNode ? B->RootNode->NodeName : TEXT(""));
    O.State->SetNumberField(TEXT("root_child_count"), B->RootNode ? B->RootNode->Children.Num() : 0); O.State->SetNumberField(TEXT("root_service_count"), B->RootNode ? B->RootNode->Services.Num() : 0); O.State->SetNumberField(TEXT("root_decorator_count"), B->RootDecorators.Num());
    O.State->SetArrayField(TEXT("behavior_tree_children"), B->RootNode ? Root->GetArrayField(TEXT("children")) : TArray<TSharedPtr<FJsonValue>>{}); O.State->SetArrayField(TEXT("root_services"), B->RootNode ? Root->GetArrayField(TEXT("services")) : TArray<TSharedPtr<FJsonValue>>{});
    O.State->SetNumberField(TEXT("node_count"), T.NodeCount); O.State->SetNumberField(TEXT("runtime_object_count"), T.Seen.Num()); O.State->SetBoolField(TEXT("traversal_truncated"), false); O.State->SetBoolField(TEXT("observations_complete"), true);
    O.State->SetBoolField(TEXT("editor_graph_loaded"), B->BTGraph != nullptr); O.State->SetNumberField(TEXT("editor_graph_node_count"), B->BTGraph ? B->BTGraph->Nodes.Num() : 0);
    FString Shape = TEXT("sequence_patrol_chase"); if (B->RootNode && B->RootNode->IsA<UBTComposite_Selector>() && B->RootNode->Children.Num() == 2) { auto First = B->RootNode->Children[0].ChildComposite; Shape = First && First->IsA<UBTComposite_Sequence>() && First->Children.Num() >= 3 ? TEXT("selector_alert_chase_patrol") : TEXT("selector_chase_patrol"); } O.State->SetStringField(TEXT("tree_shape"), Shape);
    return RDCurrent(TEXT("observe_complete"));
}
TSharedRef<FJsonObject> Observation(const FObservation& O)
{ auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.Sources); J->SetObjectField(TEXT("blackboard_references"), O.References); J->SetObjectField(TEXT("disk"), O.DiskState); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); return J; }
void Publish(FResult& R, const FObservation& O, const TCHAR* Label)
{
    R.Data->SetObjectField(Label, Observation(O));
    for (const auto& P : O.State->Values) { R.Data->SetField(P.Key, P.Value); }
    R.Data->SetStringField(TEXT("behavior_tree_path"), O.State->GetStringField(TEXT("asset_path"))); R.Data->SetBoolField(TEXT("behavior_tree_loaded"), true); R.Data->SetStringField(TEXT("behavior_tree_class"), TEXT("BehaviorTree"));
    const FString BBPath = O.State->GetStringField(TEXT("blackboard_path")); R.Data->SetBoolField(TEXT("blackboard_loaded"), !BBPath.IsEmpty()); R.Data->SetStringField(TEXT("blackboard_class"), BBPath.IsEmpty() ? TEXT("") : TEXT("BlackboardData"));
    TArray<TSharedPtr<FJsonValue>> Empty; R.Data->SetArrayField(TEXT("blackboard_keys"), Empty); R.Data->SetArrayField(TEXT("blackboard_inherited_keys"), Empty); R.Data->SetStringField(TEXT("blackboard_parent"), TEXT(""));
    if (!BBPath.IsEmpty()) { const auto Ref = O.References->GetObjectField(BBPath)->GetObjectField(TEXT("state")); R.Data->SetArrayField(TEXT("blackboard_keys"), Ref->GetArrayField(TEXT("own_keys"))); R.Data->SetArrayField(TEXT("blackboard_inherited_keys"), Ref->GetArrayField(TEXT("inherited_keys"))); R.Data->SetStringField(TEXT("blackboard_parent"), Ref->GetStringField(TEXT("parent_blackboard"))); }
    R.Data->SetBoolField(TEXT("readback_confirmed"), true); R.Data->SetBoolField(TEXT("tree_shape_fallback_is_execution_proof"), false);
}
UBehaviorTreeGraphNode_Root* FindRoot(UBehaviorTreeGraph* G)
{ UBehaviorTreeGraphNode_Root* Found = nullptr; if (!Loaded(G) || G->Nodes.Num() > MaxNodes) { return nullptr; } for (UEdGraphNode* N : G->Nodes) { if (auto* Root = Cast<UBehaviorTreeGraphNode_Root>(N)) { if (Found) { return nullptr; } Found = Root; } } return Found; }
UBehaviorTreeGraph* EnsureGraph(UBehaviorTree* B, FResult& R)
{
    if (B->BTGraph) { auto* G = Cast<UBehaviorTreeGraph>(B->BTGraph); R.Data->SetBoolField(TEXT("graph_reused"), true); return G; }
    R.Stage = TEXT("create_graph"); R.bAttempted = true; R.Data->SetBoolField(TEXT("graph_creation_attempted"), true);
    auto* G = Cast<UBehaviorTreeGraph>(FBlueprintEditorUtils::CreateNewGraph(B, TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass()));
    if (!Loaded(G) || G->GetClass() != UBehaviorTreeGraph::StaticClass() || G->GetOuter() != B || G->Schema != UEdGraphSchema_BehaviorTree::StaticClass() || G->RootNodeClass != UBehaviorTreeGraphNode_Root::StaticClass()) { Fail(R, TEXT("behavior_tree_graph_creation_failed"), TEXT("Graph allocation did not yield the exact public native graph/schema/root defaults; retained prefix remains.")); return nullptr; }
    B->BTGraph = G; R.bChanged = true; R.Data->SetBoolField(TEXT("graph_created"), true);
    const UEdGraphSchema* Schema = G->GetSchema(); if (!Loaded(Schema) || Schema->GetClass() != UEdGraphSchema_BehaviorTree::StaticClass()) { Fail(R, TEXT("behavior_tree_schema_unobservable"), TEXT("Allocated graph retained but native schema CDO is unobservable.")); return nullptr; }
    Schema->CreateDefaultNodesForGraph(*G); G->OnCreated(); G->Initialize();
    auto* Root = FindRoot(G); if (!Loaded(Root) || Root->GetClass() != UBehaviorTreeGraphNode_Root::StaticClass()) { Fail(R, TEXT("behavior_tree_graph_root_missing"), TEXT("Public graph lifecycle did not produce one readable exact native root; no rollback/retry.")); return nullptr; }
    R.Data->SetStringField(TEXT("factory_graph_blackboard_path"), Root->BlackboardAsset ? Root->BlackboardAsset->GetPathName() : TEXT("")); R.Data->SetBoolField(TEXT("implicit_root_default_selection_possible"), true); return G;
}
bool Apply(UBehaviorTree* B, UBlackboardData* Requested, FResult& R)
{
    if (RDActive && RDActive->Enabled) { return RDActive->Apply(); }
    auto* G = EnsureGraph(B, R); auto* Root = FindRoot(G); if (!Loaded(G) || !Loaded(Root) || G->IsLocked()) { return Fail(R, TEXT("behavior_tree_graph_root_missing"), TEXT("Requires an unlocked exact editor graph with one native root.")); }
    R.Stage = TEXT("assign_blackboard"); R.bAttempted = true; R.Data->SetBoolField(TEXT("blackboard_assignment_attempted"), true); B->Modify(); G->Modify(); Root->Modify(); Root->BlackboardAsset = Requested; B->BlackboardAsset = Requested;
    R.Data->SetNumberField(TEXT("direct_update_blackboard_calls"), 1); G->UpdateBlackboardChange(); R.Data->SetNumberField(TEXT("direct_update_asset_calls"), 1); G->UpdateAsset(UBehaviorTreeGraph::KeepRebuildCounter); B->BlackboardAsset = Requested;
    if (B->RootNode) { R.Data->SetNumberField(TEXT("direct_initialize_root_calls"), 1); B->RootNode->InitializeFromAsset(*B); }
    B->MarkPackageDirty(); G->MarkPackageDirty(); return B->BlackboardAsset == Requested && Root->BlackboardAsset == Requested;
}
bool Retained(UBehaviorTree* B, const FContext& C, const FObservation& Before, FObservation& After, FResult& R, bool bExpectSameTopology)
{
    if (!Observe(B, C, After, R)) { R.bKnown = false; R.bPost = false; R.Data->SetBoolField(TEXT("observations_complete"), false); return Fail(R, TEXT("behavior_tree_retained_state_unobservable"), TEXT("Retained runtime/graph/reference state is incomplete; no further mutation/save/retry.")); }
    Publish(R, After, TEXT("after")); R.bChanged |= Text(Before.State) != Text(After.State) || Text(Before.Sources) != Text(After.Sources)
        || Text(Before.References) != Text(After.References) || Text(Before.OtherDirty) != Text(After.OtherDirty) || Text(Before.DiskState) != Text(After.DiskState);
    TArray<TSharedPtr<FJsonValue>> NewCDOs; const bool NativeOK = ProtectNative(Before.Sources, After.Sources, R.Data->GetBoolField(TEXT("graph_created")), NewCDOs);
    const bool ReferencesOK = Text(Before.References) == Text(After.References), OtherOK = Text(Before.OtherDirty) == Text(After.OtherDirty), DiskOK = Text(Before.DiskState) == Text(After.DiskState);
    const bool TopologyOK = !bExpectSameTopology || Text(Before.State->GetObjectField(TEXT("topology")).ToSharedRef()) == Text(After.State->GetObjectField(TEXT("topology")).ToSharedRef());
    R.Data->SetBoolField(TEXT("existing_native_defaults_preserved"), NativeOK); R.Data->SetArrayField(TEXT("implicit_native_cdo_creation_observed"), NewCDOs); R.Data->SetBoolField(TEXT("blackboard_sources_preserved"), ReferencesOK); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), OtherOK); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), DiskOK); R.Data->SetBoolField(TEXT("prior_topology_preserved"), TopologyOK);
    if (!NativeOK || !ReferencesOK || !OtherOK || !DiskOK || !TopologyOK) { R.bPost = false; return Fail(R, TEXT("behavior_tree_preservation_failed"), TEXT("Actual target retained but protected defaults/Blackboards/other dirty states/disk/topology changed; save suppressed.")); } return true;
}
// SavePackage can add these normal persistence flags to the exact already-audited owned objects.
bool SavedValueEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& Path, int32& FlagTransitions)
{
    if (!A.IsValid() || !B.IsValid() || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object)
    {
        const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num()) { return false; }
        for (const auto& Pair : X->Values)
        {
            auto V = Y->TryGetField(Pair.Key.ToView()); const FString Key(*Pair.Key), P = Path + TEXT("/") + Key;
            if (Key == TEXT("object_flags") && X->HasField(TEXT("path")) && X->HasField(TEXT("class")) && X->HasField(TEXT("outer")) && V.IsValid() && Pair.Value->Type == EJson::String && V->Type == EJson::String)
            {
                const FString Left = Pair.Value->AsString(), Right = V->AsString(); if (!Left.IsNumeric() || !Right.IsNumeric()) { return false; }
                const uint64 Old = FCString::Strtoui64(*Left, nullptr, 10), Now = FCString::Strtoui64(*Right, nullptr, 10);
                if (Left != FString::Printf(TEXT("%llu"), Old) || Right != FString::Printf(TEXT("%llu"), Now)
                    || Now != (Old | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; } if (Old != Now) { ++FlagTransitions; } continue;
            }
            if (!SavedValueEqual(Pair.Value, V, P, FlagTransitions)) { return false; }
        }
        return true;
    }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!SavedValueEqual(X[I], Y[I], Path + TEXT("/") + FString::FromInt(I), FlagTransitions)) { return false; } } return true; }
    if (A->Type == EJson::String)
    {
        if (Path == TEXT("/graph/properties/ModCounter/values/0")) { return A->AsString().IsNumeric() && B->AsString() == FString::FromInt(FCString::Atoi(*A->AsString()) + 1); }
        return A->AsString() == B->AsString();
    }
    if (A->Type == EJson::Number) { return Path == TEXT("/graph/mod_counter") ? B->AsNumber() == A->AsNumber() + 1 : A->AsNumber() == B->AsNumber(); }
    if (A->Type == EJson::Boolean) { return Path == TEXT("/package_dirty") ? !B->AsBool() : A->AsBool() == B->AsBool(); }
    return A->Type == EJson::Null;
}
bool SaveComposite(UBehaviorTree* B, const FContext& C, const FObservation& Expected, FResult& R)
{
    if (!RDCurrent(TEXT("before_requested_save"))) { return false; } if (!R.bSaveRequested) { return true; } if (!RequirePersistedBlackboard(B->BlackboardAsset, Expected.References, R)) { return false; } auto* G = Cast<UBehaviorTreeGraph>(B->BTGraph);
    if (!Loaded(G) || G->IsLocked() || G->ModCounter == MAX_int32) { return Fail(R, TEXT("behavior_tree_save_graph_locked"), TEXT("Save requires the same audited unlocked native graph and a nonoverflowing rebuild counter.")); }
    R.Stage = TEXT("save"); TArray<TSharedPtr<FJsonValue>> Calls;
    R.Data->SetBoolField(TEXT("lock_updates_entered"), true); G->LockUpdates(); R.Data->SetBoolField(TEXT("lock_updates_returned"), true); if (!RDCurrent(TEXT("lock_updates_return"))) { return false; } R.bSaveAttempted = true; auto First = MakeShared<FJsonObject>(); First->SetNumberField(TEXT("attempt_index"), 1); First->SetBoolField(TEXT("package_dirty_before"), B->GetOutermost()->IsDirty()); First->SetBoolField(TEXT("graph_locked_during_save"), G->IsLocked());
    First->SetBoolField(TEXT("entered"), true); First->SetBoolField(TEXT("returned"), false); Calls.Add(MakeShared<FJsonValueObject>(First)); R.Data->SetArrayField(TEXT("save_calls"), Calls); R.Data->SetNumberField(TEXT("save_attempt_count"), 1);
    const bool FirstSucceeded = UEditorAssetLibrary::SaveLoadedAsset(B, false); First->SetBoolField(TEXT("returned"), true); First->SetBoolField(TEXT("success"), FirstSucceeded); if (!RDCurrent(TEXT("first_save_return"))) { R.Data->SetBoolField(TEXT("unlock_skipped_after_unknown"), true); return false; } First->SetBoolField(TEXT("package_dirty_after_call"), B->GetOutermost()->IsDirty());
    First->SetBoolField(TEXT("unlock_update_dispatched"), true); RDActive->AfterUnlock = true; G->UnlockUpdates(); First->SetBoolField(TEXT("unlock_update_returned"), true); R.Data->SetNumberField(TEXT("unlock_update_calls"), 1); if (!RDCurrent(TEXT("unlock_update_return"))) { return false; } First->SetBoolField(TEXT("package_dirty_after"), B->GetOutermost()->IsDirty());
    R.Data->SetArrayField(TEXT("save_calls"), Calls); R.Data->SetNumberField(TEXT("save_attempt_count"), Calls.Num()); bool LastSucceeded = FirstSucceeded;
    // Original conditional second exact save: never after a false first return.
    if (FirstSucceeded && B->GetOutermost()->IsDirty())
    {
        auto Second = MakeShared<FJsonObject>(); Second->SetNumberField(TEXT("attempt_index"), 2); Second->SetBoolField(TEXT("package_dirty_before"), true); Second->SetBoolField(TEXT("graph_locked_during_save"), G->IsLocked());
        Second->SetBoolField(TEXT("entered"), true); Second->SetBoolField(TEXT("returned"), false); Calls.Add(MakeShared<FJsonValueObject>(Second)); R.Data->SetArrayField(TEXT("save_calls"), Calls); R.Data->SetNumberField(TEXT("save_attempt_count"), 2);
        LastSucceeded = UEditorAssetLibrary::SaveLoadedAsset(B, false); Second->SetBoolField(TEXT("returned"), true); Second->SetBoolField(TEXT("success"), LastSucceeded); if (!RDCurrent(TEXT("second_save_return"))) { return false; } Second->SetBoolField(TEXT("package_dirty_after_call"), B->GetOutermost()->IsDirty()); Second->SetBoolField(TEXT("package_dirty_after"), B->GetOutermost()->IsDirty()); Second->SetBoolField(TEXT("unlock_update_dispatched"), false);
        R.Data->SetArrayField(TEXT("save_calls"), Calls); R.Data->SetNumberField(TEXT("save_attempt_count"), Calls.Num());
    }
    FObservation After; if (!Observe(B, C, After, R)) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("behavior_tree_save_state_unobservable"), TEXT("Save/unlock prefix retained; current RAM/disk/source state is incomplete and must not be replayed.")); } Publish(R, After, TEXT("after_save"));
    if (!LastSucceeded) { R.bKnown = false; R.bPost = false; R.Data->SetBoolField(TEXT("save_disk_outcome_known"), false); return Fail(R, TEXT("behavior_tree_save_failed"), TEXT("An exact save returned false; no failed-save retry occurred. RAM prefix retained and disk transaction is uncertified.")); }
    int32 Flags = 0; const bool StateOK = SavedValueEqual(MakeShared<FJsonValueObject>(Expected.State), MakeShared<FJsonValueObject>(After.State), TEXT(""), Flags);
    TArray<TSharedPtr<FJsonValue>> NewCDOs; const bool SourceOK = ProtectNative(Expected.Sources, After.Sources, false, NewCDOs) && Text(Expected.References) == Text(After.References) && Text(Expected.OtherDirty) == Text(After.OtherDirty);
    R.Data->SetBoolField(TEXT("save_state_preserved"), StateOK); R.Data->SetBoolField(TEXT("save_source_preserved"), SourceOK); R.Data->SetNumberField(TEXT("normal_save_object_flag_transitions"), Flags); R.Data->SetBoolField(TEXT("save_disk_outcome_known"), true);
    R.Data->SetStringField(TEXT("save_state_comparison_scope"), TEXT("Exact audited owned identities predict RF_WasLoaded|RF_LoadCompleted OR additions after successful exported-object finalization; only selected package_dirty=false and one UnlockUpdates ModCounter increment are predicted. All raw states and all other fields/defaults/Blackboards/dirty states remain unchanged in the comparison."));
    R.bSaved = !B->GetOutermost()->IsDirty() && After.DiskState->GetBoolField(TEXT("exists")); R.Data->SetBoolField(TEXT("asset_saved"), R.bSaved);
    if (!R.bSaved || !StateOK || !SourceOK) { R.bPost = false; return Fail(R, TEXT("behavior_tree_save_postcondition_failed"), TEXT("Exact saves returned true but clean target/state/source preservation was not certified; saved bytes and retained actual snapshots are preserved.")); } return true;
}

bool Save(UBehaviorTree* B, const FContext& C, const FObservation& Expected, FResult& R)
{
    if (RDActive && RDActive->Enabled) { return SaveComposite(B, C, Expected, R); }
    if (!R.bSaveRequested) { return true; } if (!RequirePersistedBlackboard(B->BlackboardAsset, Expected.References, R)) { return false; } auto* G = Cast<UBehaviorTreeGraph>(B->BTGraph);
    if (!Loaded(G) || G->IsLocked() || G->ModCounter == MAX_int32) { return Fail(R, TEXT("behavior_tree_save_graph_locked"), TEXT("Save requires the same audited unlocked native graph and a nonoverflowing rebuild counter.")); }
    R.Stage = TEXT("save"); TArray<TSharedPtr<FJsonValue>> Calls;
    G->LockUpdates(); R.bSaveAttempted = true; auto First = MakeShared<FJsonObject>(); First->SetNumberField(TEXT("attempt_index"), 1); First->SetBoolField(TEXT("package_dirty_before"), B->GetOutermost()->IsDirty()); First->SetBoolField(TEXT("graph_locked_during_save"), G->IsLocked());
    const bool FirstSucceeded = UEditorAssetLibrary::SaveLoadedAsset(B, false); First->SetBoolField(TEXT("success"), FirstSucceeded); First->SetBoolField(TEXT("package_dirty_after_call"), B->GetOutermost()->IsDirty());
    G->UnlockUpdates(); R.Data->SetNumberField(TEXT("unlock_update_calls"), 1); First->SetBoolField(TEXT("unlock_update_dispatched"), true); First->SetBoolField(TEXT("package_dirty_after"), B->GetOutermost()->IsDirty()); Calls.Add(MakeShared<FJsonValueObject>(First));
    R.Data->SetArrayField(TEXT("save_calls"), Calls); R.Data->SetNumberField(TEXT("save_attempt_count"), Calls.Num()); bool LastSucceeded = FirstSucceeded;
    // Original conditional second exact save: never after a false first return.
    if (FirstSucceeded && B->GetOutermost()->IsDirty())
    {
        auto Second = MakeShared<FJsonObject>(); Second->SetNumberField(TEXT("attempt_index"), 2); Second->SetBoolField(TEXT("package_dirty_before"), true); Second->SetBoolField(TEXT("graph_locked_during_save"), G->IsLocked());
        LastSucceeded = UEditorAssetLibrary::SaveLoadedAsset(B, false); Second->SetBoolField(TEXT("success"), LastSucceeded); Second->SetBoolField(TEXT("package_dirty_after_call"), B->GetOutermost()->IsDirty()); Second->SetBoolField(TEXT("package_dirty_after"), B->GetOutermost()->IsDirty()); Second->SetBoolField(TEXT("unlock_update_dispatched"), false); Calls.Add(MakeShared<FJsonValueObject>(Second));
        R.Data->SetArrayField(TEXT("save_calls"), Calls); R.Data->SetNumberField(TEXT("save_attempt_count"), Calls.Num());
    }
    FObservation After; if (!Observe(B, C, After, R)) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("behavior_tree_save_state_unobservable"), TEXT("Save/unlock prefix retained; current RAM/disk/source state is incomplete and must not be replayed.")); } Publish(R, After, TEXT("after_save"));
    if (!LastSucceeded) { R.bKnown = false; R.bPost = false; R.Data->SetBoolField(TEXT("save_disk_outcome_known"), false); return Fail(R, TEXT("behavior_tree_save_failed"), TEXT("An exact save returned false; no failed-save retry occurred. RAM prefix retained and disk transaction is uncertified.")); }
    int32 Flags = 0; const bool StateOK = SavedValueEqual(MakeShared<FJsonValueObject>(Expected.State), MakeShared<FJsonValueObject>(After.State), TEXT(""), Flags);
    TArray<TSharedPtr<FJsonValue>> NewCDOs; const bool SourceOK = ProtectNative(Expected.Sources, After.Sources, false, NewCDOs) && Text(Expected.References) == Text(After.References) && Text(Expected.OtherDirty) == Text(After.OtherDirty);
    R.Data->SetBoolField(TEXT("save_state_preserved"), StateOK); R.Data->SetBoolField(TEXT("save_source_preserved"), SourceOK); R.Data->SetNumberField(TEXT("normal_save_object_flag_transitions"), Flags); R.Data->SetBoolField(TEXT("save_disk_outcome_known"), true);
    R.Data->SetStringField(TEXT("save_state_comparison_scope"), TEXT("Exact audited owned identities predict RF_WasLoaded|RF_LoadCompleted OR additions after successful exported-object finalization; only selected package_dirty=false and one UnlockUpdates ModCounter increment are predicted. All raw states and all other fields/defaults/Blackboards/dirty states remain unchanged in the comparison."));
    R.bSaved = !B->GetOutermost()->IsDirty() && After.DiskState->GetBoolField(TEXT("exists")); R.Data->SetBoolField(TEXT("asset_saved"), R.bSaved);
    if (!R.bSaved || !StateOK || !SourceOK) { R.bPost = false; return Fail(R, TEXT("behavior_tree_save_postcondition_failed"), TEXT("Exact saves returned true but clean target/state/source preservation was not certified; saved bytes and retained actual snapshots are preserved.")); } return true;
}
}

FString CreateAsset(const FString& AssetName, const FString& AssetRoot, const FString& BlackboardPath, bool bSaveAsset)
{
    FResult R; Init(R, TEXT("create_ai_behavior_tree_asset"), bSaveAsset); if (RDActive && RDActive->Enabled) { Fail(R, TEXT("behavior_tree_request_running"), TEXT("A Composite Blackboard request owns the callback boundary; reentrant work was not dispatched.")); return Encode(R); } if (!EditorState(R)) { return Encode(R); }
    FString Root = AssetRoot; while (Root.EndsWith(TEXT("/"))) { Root.LeftChopInline(1); }
    if (!SimpleName(AssetName) || !(Root == TEXT("/Game") || Root.StartsWith(TEXT("/Game/"))) || !FPackageName::IsValidLongPackageName(Root)) { Fail(R, TEXT("behavior_tree_create_path_invalid"), TEXT("Use a simple asset name and valid /Game asset_root; existing targets are never overwritten.")); return Encode(R); }
    const FString PackageName = Root + TEXT("/") + AssetName, Path = PackageName + TEXT(".") + AssetName; FString BBPath;
    UBlackboardData* Requested = nullptr;
    if (!BlackboardPath.IsEmpty()) { if (!ExactPath(BlackboardPath, false, BBPath, R)) { return Encode(R); } UObject* O = Resolve(BBPath, R); Requested = Cast<UBlackboardData>(O); if (!Requested || Requested->GetClass() != UBlackboardData::StaticClass()) { Fail(R, TEXT("behavior_tree_blackboard_required"), TEXT("Requested source must be exact native BlackboardData.")); return Encode(R); } }
    FContext Context; if (!GatherReferences(nullptr, Requested, true, Context, R)) { return Encode(R); }
    auto NativeBefore = MakeShared<FJsonObject>(), DiskBefore = MakeShared<FJsonObject>(), DirtyBefore = MakeShared<FJsonObject>(), ReferencesBefore = MakeShared<FJsonObject>();
    R.Data->SetObjectField(TEXT("native_sources_before"), NativeBefore); R.Data->SetObjectField(TEXT("disk_before"), DiskBefore); R.Data->SetObjectField(TEXT("other_package_dirty_before"), DirtyBefore); R.Data->SetObjectField(TEXT("blackboard_references_before"), ReferencesBefore);
    if (!FPackageName::IsValidObjectPath(Path) || !NativeSources(FixedClasses(), NativeBefore) || !Disk(PackageName, DiskBefore) || !Dirty(PackageName, DirtyBefore)) { Fail(R, TEXT("behavior_tree_native_source_unobservable"), TEXT("Exact native class/default/destination observations were incomplete before allocation; partial rows retained.")); return Encode(R); }
    for (const auto& Ref : Context.Blackboards) { auto J = MakeShared<FJsonObject>(); if (!BlackboardObservation(Ref.Get(), J, R)) { Fail(R, TEXT("behavior_tree_blackboard_source_unobservable"), TEXT("Current/default-selection Blackboard source was not fully observable before allocation.")); return Encode(R); } ReferencesBefore->SetObjectField(Ref->GetPathName(), J); }
    if (FindPackage(nullptr, *PackageName) || FindObject<UObject>(nullptr, *Path) || DiskBefore->GetBoolField(TEXT("exists")) || FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path)).IsValid()) { Fail(R, TEXT("behavior_tree_target_exists"), TEXT("Destination exists; no replacement or creation attempted.")); return Encode(R); }
    if (!RequirePersistedBlackboard(Requested, ReferencesBefore, R)) { return Encode(R); }
    R.Stage = TEXT("create_asset"); R.bAttempted = true; UPackage* Package = CreatePackage(*PackageName); TStrongObjectPtr<UPackage> KeepPackage(Package);
    auto* B = Package ? NewObject<UBehaviorTree>(Package, UBehaviorTree::StaticClass(), *AssetName, RF_Public | RF_Standalone | RF_Transactional) : nullptr; TStrongObjectPtr<UBehaviorTree> Keep(B); R.bChanged = Package != nullptr;
    if (!Loaded(B)) { R.bKnown = false; Fail(R, TEXT("behavior_tree_create_failed"), TEXT("Allocation prefix retained; no cleanup/rollback/retry is claimed.")); return Encode(R); }
    FObservation Before; if (!Observe(B, Context, Before, R)) { R.bKnown = false; Fail(R, TEXT("behavior_tree_factory_unobservable"), TEXT("Allocated bare native tree retained but its factory state is incomplete.")); return Encode(R); } Publish(R, Before, TEXT("factory_default_state"));
    bool Applied = Apply(B, Requested, R); FObservation After;
    if (!Retained(B, Context, Before, After, R, false)) { return Encode(R); }
    TArray<TSharedPtr<FJsonValue>> NewCDOs; if (!ProtectNative(NativeBefore, After.Sources, true, NewCDOs) || Text(ReferencesBefore) != Text(After.References) || Text(DirtyBefore) != Text(After.OtherDirty) || Text(DiskBefore) != Text(After.DiskState)) { Fail(R, TEXT("behavior_tree_creation_source_changed"), TEXT("Created prefix retained but pre-allocation source/disk/other-state preservation failed.")); return Encode(R); }
    R.Data->SetArrayField(TEXT("implicit_native_cdo_creation_observed"), NewCDOs);
    if (!Applied || B->BlackboardAsset != Requested || After.State->GetObjectField(TEXT("graph"))->GetStringField(TEXT("blackboard")) != (Requested ? Requested->GetPathName() : TEXT(""))) { Fail(R, TEXT("behavior_tree_assignment_failed"), TEXT("Actual retained runtime/editor Blackboard pointers did not match the requested value.")); return Encode(R); }
    B->MarkPackageDirty(); R.Data->SetBoolField(TEXT("asset_registry_publication_dispatched"), true); FAssetRegistryModule::AssetCreated(B);
    FObservation Published; if (!Retained(B, Context, After, Published, R, true)) { return Encode(R); }
    R.Data->SetBoolField(TEXT("created"), true); R.bChanged = true; R.bPost = true; if (!Save(B, Context, Published, R)) { return Encode(R); }
    R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
FString SetBlackboard(const FString& BehaviorTreePath, const FString& BlackboardPath, bool bSaveAsset)
{
    FResult R; Init(R, TEXT("set_ai_behavior_tree_blackboard"), bSaveAsset); if (RDActive && RDActive->Enabled) { Fail(R, TEXT("behavior_tree_request_running"), TEXT("A Composite Blackboard request owns the callback boundary; reentrant work was not dispatched.")); return Encode(R); } FString Path, BBPath;
    if (!EditorState(R) || !ExactPath(BehaviorTreePath, true, Path, R) || !ExactPath(BlackboardPath, false, BBPath, R)) { return Encode(R); }
    auto* B = Cast<UBehaviorTree>(Resolve(Path, R)); auto* Requested = Cast<UBlackboardData>(Resolve(BBPath, R)); TStrongObjectPtr<UBehaviorTree> Keep(B); TStrongObjectPtr<UBlackboardData> KeepBB(Requested);
    if (!B || B->GetClass() != UBehaviorTree::StaticClass() || !Requested || Requested->GetClass() != UBlackboardData::StaticClass()) { Fail(R, TEXT("behavior_tree_native_assets_required"), TEXT("Requires exact native BehaviorTree and BlackboardData assets.")); return Encode(R); }
    FContext Context; if (!GatherReferences(B, Requested, !B->BTGraph, Context, R)) { return Encode(R); }
    FRDComposite CompositeWork(R); if (!CompositeWork.Begin(B, Requested)) { if (R.Code.IsEmpty()) { Fail(R, TEXT("behavior_tree_composite_profile_unsupported"), TEXT("The existing composite graph is outside the finite first profile; no writer was dispatched.")); } return Encode(R); }
    TGuardValue<FRDComposite*> CompositeRequest(RDActive, CompositeWork.Enabled ? &CompositeWork : nullptr); FObservation Before;
    if (!Observe(B, Context, Before, R)) { Fail(R, TEXT("behavior_tree_preflight_unobservable"), TEXT("Existing native tree/editor graph/defaults/references exceed the bounded complete supported state; custom/nested/disconnected variants remain incomplete.")); return Encode(R); } Publish(R, Before, TEXT("before"));
    auto* ExistingGraph = Cast<UBehaviorTreeGraph>(B->BTGraph); auto* ExistingRoot = FindRoot(ExistingGraph);
    if (B->BlackboardAsset == Requested && ExistingRoot && ExistingRoot->BlackboardAsset == Requested)
    {
        FObservation After; if (!Retained(B, Context, Before, After, R, true)) { return Encode(R); }
        if (R.bChanged) { Fail(R, TEXT("behavior_tree_repeated_observation_changed"), TEXT("Same-assignment observations changed without explicit assignment/save.")); return Encode(R); }
        R.Data->SetBoolField(TEXT("already_assigned"), true); R.Data->SetBoolField(TEXT("save_asset_requested"), false); R.bSaveRequested = false; R.bPost = true; R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
    }
    if (!RequirePersistedBlackboard(Requested, Before.References, R)) { return Encode(R); }
    R.Data->SetStringField(TEXT("previous_blackboard_path"), Before.State->GetStringField(TEXT("blackboard_path"))); const bool Applied = Apply(B, Requested, R); if (RDActive && RDActive->Bad) { return Encode(R); } FObservation After;
    if (!Retained(B, Context, Before, After, R, ExistingGraph != nullptr)) { return Encode(R); }
    if (!Applied || B->BlackboardAsset != Requested || After.State->GetObjectField(TEXT("graph"))->GetStringField(TEXT("blackboard")) != Requested->GetPathName()) { Fail(R, TEXT("behavior_tree_assignment_failed"), TEXT("Retained runtime and editor root Blackboard pointers do not match the request.")); return Encode(R); }
    R.bChanged = true; R.bPost = true; if (!Save(B, Context, After, R)) { return Encode(R); } R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
FString InspectAsset(const FString& AssetPath)
{
    FResult R; Init(R, TEXT("inspect_ai_asset_structure"), false); if (RDActive && RDActive->Enabled) { Fail(R, TEXT("behavior_tree_request_running"), TEXT("A Composite Blackboard request owns the callback boundary; reentrant work was not dispatched.")); return Encode(R); } FString Path; if (!EditorState(R) || !ExactPath(AssetPath, false, Path, R)) { return Encode(R); }
    // Route the Blackboard branch before loading so its original decoder/load diagnostics remain authoritative and unchanged.
    UObject* Existing = FindObject<UObject>(nullptr, *Path);
    if (Existing && Existing->GetClass() == UBlackboardData::StaticClass()) { return Blackboard::InspectAsset(Path); }
    if (!Existing)
    {
        const FAssetData Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path));
        if (Asset.IsValid() && Asset.AssetClassPath.ToString() == TEXT("/Script/AIModule.BlackboardData")) { return Blackboard::InspectAsset(Path); }
    }
    UObject* O = Resolve(Path, R); TStrongObjectPtr<UObject> Keep(O); if (!O) { return Encode(R); }
    if (O->GetClass() == UBlackboardData::StaticClass()) { return Blackboard::InspectAsset(Path); }
    auto* B = Cast<UBehaviorTree>(O); if (!B || B->GetClass() != UBehaviorTree::StaticClass()) { Fail(R, TEXT("behavior_tree_inspector_branch_incomplete"), TEXT("Only exact native BehaviorTree and delegated Blackboard branches are implemented; EQS/custom assets remain incomplete.")); return Encode(R); }
    FContext Context; if (!GatherReferences(B, nullptr, false, Context, R)) { return Encode(R); }
    FRDComposite CompositeWork(R); if (!CompositeWork.Begin(B, nullptr)) { if (R.Code.IsEmpty()) { Fail(R, TEXT("behavior_tree_composite_profile_unsupported"), TEXT("The existing composite graph is outside the finite first profile; no writer was dispatched.")); } return Encode(R); }
    TGuardValue<FRDComposite*> CompositeRequest(RDActive, CompositeWork.Enabled ? &CompositeWork : nullptr); FObservation Before, After;
    if (!Observe(B, Context, Before, R)) { Fail(R, TEXT("behavior_tree_inspection_unobservable"), TEXT("Bounded complete native runtime/editor graph/reference inspection failed; custom, nested decorator and disconnected variants remain incomplete.")); return Encode(R); } Publish(R, Before, TEXT("before"));
    if (!Retained(B, Context, Before, After, R, true)) { return Encode(R); }
    if (R.bChanged) { Fail(R, TEXT("behavior_tree_inspection_state_changed"), TEXT("Repeated observations differ after initial resolution; no explicit graph update/cache refresh/write/save was invoked.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("read_only_after_initial_resolution"), true); R.bPost = true; R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
}
