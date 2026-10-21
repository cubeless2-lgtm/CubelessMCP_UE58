// Original paired BT/requested-Blackboard reader, through bounded existing native readers.
#include "Compatibility/UE58/CubelessAIBehaviorAssetsInspectionApi.h"
#include "Compatibility/UE58/CubelessBehaviorTreeApi.h"
#include "Compatibility/UE58/CubelessBlackboardApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_RotateToFaceBBEntry.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::AIBehaviorAssetsInspection
{
namespace
{
using FResult = Function::FResult;
constexpr int32 MaxReaderChars = 16 * 1024 * 1024;
constexpr int32 MaxAuditPackages = 256, MaxExports = 16384, MaxPropertyValues = 65536, MaxReferences = 16384;
const FName TargetKey(TEXT("TargetActor")), PatrolKey(TEXT("PatrolLocation"));

bool Fail(FResult& R, const TCHAR* Code, const FString& Message)
{ R.bSuccess = false; R.ErrorCode = Code; R.Error = Message; return false; }

bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }

TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"), O ? O->GetPathName() : FString());
    J->SetStringField(TEXT("class"), O ? O->GetClass()->GetPathName() : FString());
    J->SetStringField(TEXT("outer"), O && O->GetOuter() ? O->GetOuter()->GetPathName() : FString());
    J->SetStringField(TEXT("object_flags"), O ? FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags())) : TEXT("0"));
    return J;
}

FString JsonText(const TSharedRef<FJsonObject>& J)
{ FString Text; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Text)); return Text; }

TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{ TArray<TSharedPtr<FJsonValue>> Out; for (const FString& Value : Values) { Out.Add(MakeShared<FJsonValueString>(Value)); } return Out; }

bool ExactPath(const FString& Input, FString& Path, FResult& R)
{
    Path = FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd(); Path.TrimQuotesInline(); Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!Path.Contains(TEXT("."))) { Path += TEXT(".") + FPackageName::GetLongPackageAssetName(Path); }
    FString Package, Asset;
    if (Path.Len() > 1024 || !FPackageName::IsValidObjectPath(Path)
        || !Path.Split(TEXT("."), &Package, &Asset, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
        || Asset != FPackageName::GetLongPackageAssetName(Package)
        || !(Package.StartsWith(TEXT("/Game/")) || Package.StartsWith(TEXT("/Engine/"))))
    { return Fail(R, TEXT("behavior_assets_path_invalid"), TEXT("Use exact /Game or /Engine asset object paths/packages; other mounts, custom subobjects and malformed paths are unsupported.")); }
    return true;
}

bool Ordinary(const UObject* O, const FString& Path, UClass* Class)
{
    return Loaded(O) && O->GetClass() == Class && O->GetPathName() == Path && O->GetOuter() == O->GetOutermost()
        && !O->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        && !O->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor);
}

void LoadReceipt(FResult& R, const TCHAR* Role, const FString& Path, const UObject* Before, const UObject* After)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("role"), Role); Row->SetStringField(TEXT("requested_path"), Path);
    Row->SetBoolField(TEXT("already_loaded_before_call"), Before != nullptr); Row->SetBoolField(TEXT("load_object_dispatched"), true);
    Row->SetObjectField(TEXT("resolved_object"), Identity(After));
    TArray<TSharedPtr<FJsonValue>> Calls = R.Data->GetArrayField(TEXT("asset_load_calls"));
    Calls.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("asset_load_calls"), Calls);
    R.Data->SetNumberField(TEXT("asset_load_call_count"), Calls.Num());
    R.Data->SetBoolField(TEXT("implicit_postload_possible"), true);
}

bool ReadCommon(const FString& Text, const TCHAR* Role, FResult& R, TSharedPtr<FJsonObject>& Snapshot, bool bRetainSnapshot = true)
{
    auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("role"), Role); Receipt->SetNumberField(TEXT("returned_chars"), Text.Len());
    TArray<TSharedPtr<FJsonValue>> Receipts = R.Data->GetArrayField(TEXT("inspection_receipts"));
    Receipts.Add(MakeShared<FJsonValueObject>(Receipt)); R.Data->SetArrayField(TEXT("inspection_receipts"), Receipts);
    R.Data->SetNumberField(TEXT("delegated_inspection_count"), Receipts.Num());
    uint8 Digest[FSHA1::DigestSize]; FSHA1::HashBuffer(*Text, Text.Len() * sizeof(TCHAR), Digest);
    Receipt->SetStringField(TEXT("returned_utf16_sha1"), BytesToHex(Digest, FSHA1::DigestSize));
    TSharedPtr<FJsonObject> Envelope;
    if (Text.Len() > MaxReaderChars || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Envelope) || !Envelope.IsValid())
    { R.bChangeStateKnown = false; return Fail(R, TEXT("behavior_assets_reader_envelope_unobservable"), TEXT("The bounded common native inspector receipt could not be decoded; no writer was invoked.")); }
    for (const TCHAR* Key : { TEXT("success"), TEXT("mutation_attempted"), TEXT("change_state_known"), TEXT("postcondition_passed"),
        TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved") })
    {
        const auto Value = Envelope->TryGetField(Key);
        if (!Value.IsValid() || Value->Type != EJson::Boolean)
        { R.Data->SetObjectField(TEXT("failed_inspection_receipt"), Envelope); R.bChangeStateKnown = false; return Fail(R, TEXT("behavior_assets_reader_contract_invalid"), TEXT("A common native inspector returned an invalid typed result.")); }
        Receipt->SetField(Key, Value);
    }
    const auto Changed = Envelope->TryGetField(TEXT("changed"));
    if (Changed.IsValid()) { Receipt->SetField(TEXT("changed"), Changed); }
    if (!Envelope->GetBoolField(TEXT("success")))
    {
        R.Data->SetObjectField(TEXT("failed_inspection_receipt"), Envelope);
        R.bChangeStateKnown = Envelope->GetBoolField(TEXT("change_state_known"));
        if (R.bChangeStateKnown && Changed.IsValid() && Changed->Type == EJson::Boolean) { R.bChanged = Changed->AsBool(); }
        return Fail(R, TEXT("behavior_assets_shared_inspection_failed"), TEXT("A complete common BT/Blackboard observation is unavailable; the actual returned failure prefix is retained."));
    }
    if (Envelope->GetBoolField(TEXT("mutation_attempted")) || !Envelope->GetBoolField(TEXT("change_state_known"))
        || !Changed.IsValid() || Changed->Type != EJson::Boolean || Changed->AsBool() || !Envelope->GetBoolField(TEXT("postcondition_passed"))
        || Envelope->GetBoolField(TEXT("compile_requested")) || Envelope->GetBoolField(TEXT("compile_attempted"))
        || Envelope->GetBoolField(TEXT("save_requested")) || Envelope->GetBoolField(TEXT("save_attempted")) || Envelope->GetBoolField(TEXT("saved")))
    { R.Data->SetObjectField(TEXT("failed_inspection_receipt"), Envelope); R.bChangeStateKnown = false; return Fail(R, TEXT("behavior_assets_reader_contract_invalid"), TEXT("Common readback must retain known unchanged complete state with no explicit writer/compile/save.")); }
    const auto DataValue = Envelope->TryGetField(TEXT("data"));
    if (!DataValue.IsValid() || DataValue->Type != EJson::Object)
    { R.Data->SetObjectField(TEXT("failed_inspection_receipt"), Envelope); R.bChangeStateKnown = false; return Fail(R, TEXT("behavior_assets_reader_snapshot_missing"), TEXT("Common inspector data is absent.")); }
    for (const TCHAR* Key : { TEXT("asset_load_count"), TEXT("asset_load_dispatched"), TEXT("implicit_postload_possible"), TEXT("delegated_blackboard_inspection_count") })
    { const auto Value = DataValue->AsObject()->TryGetField(Key); if (Value.IsValid()) { Receipt->SetField(Key, Value); } }
    const auto AfterValue = DataValue->AsObject()->TryGetField(TEXT("after"));
    if (!AfterValue.IsValid() || AfterValue->Type != EJson::Object)
    { R.Data->SetObjectField(TEXT("failed_inspection_receipt"), Envelope); R.bChangeStateKnown = false; return Fail(R, TEXT("behavior_assets_reader_snapshot_missing"), TEXT("Common inspector complete after snapshot is absent.")); }
    Snapshot = AfterValue->AsObject();
    const auto StateValue = Snapshot->TryGetField(TEXT("state"));
    if (!StateValue.IsValid() || StateValue->Type != EJson::Object)
    { R.Data->SetObjectField(TEXT("failed_inspection_receipt"), Envelope); R.bChangeStateKnown = false; return Fail(R, TEXT("behavior_assets_reader_snapshot_missing"), TEXT("Common inspector state is absent.")); }
    Receipt->SetBoolField(TEXT("retained_snapshot_is_complete_after_readback"), true);
    // One snapshot per successful common reader; its repeated equal before/raw envelope
    // is represented by the actual typed receipt and digest rather than duplicated.
    if (bRetainSnapshot) { R.Data->SetObjectField(Role, Snapshot); }
    return true;
}

bool Forbidden(const FString& Token)
{
    for (const TCHAR* Part : { TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"), TEXT("CubelessControlRigTools") })
    { if (Token.Contains(Part, ESearchCase::IgnoreCase)) { return true; } }
    for (const TCHAR* Plugin : { TEXT("CubelessMCP_UE58"), TEXT("ModelContextProtocol"), TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin"), TEXT("EditorScriptingUtilities") })
    { if (Token.StartsWith(FString::Printf(TEXT("/%s/"), Plugin), ESearchCase::IgnoreCase)) { return true; } }
    const FString Package = FPackageName::ObjectPathToPackageName(Token);
    for (const TCHAR* Module : { TEXT("CubelessMCPUE58Editor"), TEXT("ModelContextProtocol"), TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin"), TEXT("EditorScriptingUtilities") })
    { if (Package.Equals(FString::Printf(TEXT("/Script/%s"), Module), ESearchCase::IgnoreCase)) { return true; } }
    return false;
}

bool Authored(const FString& Package)
{ return Package.StartsWith(TEXT("/")) && !Package.StartsWith(TEXT("/Script/")) && !Package.StartsWith(TEXT("/Engine/"))
    && !Package.StartsWith(TEXT("/Memory/")) && !Package.StartsWith(TEXT("/Temp/")); }

struct FAudit
{
    TArray<FString> Issues, Packages;
    int32 Exports = 0, Values = 0, References = 0;
    bool bLiveComplete = true;
};

void AuditToken(const FString& Token, const FString& Source, FAudit& Audit)
{
    if (Token.IsEmpty()) { return; }
    if (Forbidden(Token)) { Audit.Issues.AddUnique(FString::Printf(TEXT("MCP-only dependency token: %s -> %s"), *Source, *Token)); }
    const FString Package = FPackageName::ObjectPathToPackageName(FPackageName::ExportTextPathToObjectPath(Token));
    if (Authored(Package)) { Audit.Packages.AddUnique(Package); }
}

void AuditObject(const UObject* O, FAudit& Audit)
{
    if (!O) { return; }
    if (!Loaded(O)) { Audit.bLiveComplete = false; Audit.Issues.AddUnique(TEXT("A serialized referenced object is unreadable.")); return; }
    AuditToken(O->GetPathName(), TEXT("loaded object"), Audit);
    int32 Depth = 0;
    for (const UClass* Class = O->GetClass(); Class; Class = Class->GetSuperClass())
    { if (++Depth > 128) { Audit.bLiveComplete = false; Audit.Issues.AddUnique(TEXT("Loaded class ancestry exceeded the audit bound.")); break; } AuditToken(Class->GetPathName(), O->GetPathName(), Audit); }
    if (const UClass* ClassObject = Cast<UClass>(O))
    { Depth = 0; for (const UClass* Class = ClassObject; Class; Class = Class->GetSuperClass())
        { if (++Depth > 128) { Audit.bLiveComplete = false; Audit.Issues.AddUnique(TEXT("Referenced class ancestry exceeded the audit bound.")); break; } AuditToken(Class->GetPathName(), O->GetPathName(), Audit); } }
}

bool AuditProperties(UObject* O, FAudit& Audit)
{
    for (TPropertyValueIterator<const FProperty> It(O->GetClass(), O, EPropertyValueIteratorFlags::FullRecursion, EFieldIteratorFlags::IncludeDeprecated); It; ++It)
    {
        if (++Audit.Values > MaxPropertyValues) { return false; }
        const FProperty* Property = It.Key(); const void* Value = It.Value();
        if (!Property || !Value) { continue; }
        if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient | CPF_SkipSerialization))
        { It.SkipRecursiveProperty(); continue; }
        FSoftObjectPath Soft;
        TArray<const UObject*> Objects;
        bool bReference = false;
        if (const FSoftObjectProperty* SoftProperty = CastField<FSoftObjectProperty>(Property))
        { Soft = SoftProperty->GetPropertyValue(Value).ToSoftObjectPath(); bReference = true; }
        else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            const FString StructPath = StructProperty->Struct ? StructProperty->Struct->GetPathName() : FString();
            if (StructPath == TEXT("/Script/CoreUObject.SoftObjectPath") || StructPath == TEXT("/Script/CoreUObject.SoftClassPath")
                || StructPath == TEXT("/Script/CoreUObject.TopLevelAssetPath"))
            {
                if (StructPath == TEXT("/Script/CoreUObject.TopLevelAssetPath"))
                {
                    if (StructProperty->Struct->GetStructureSize() != sizeof(FTopLevelAssetPath)) { return false; }
                    AuditToken(static_cast<const FTopLevelAssetPath*>(Value)->ToString(), O->GetPathName() + TEXT(".") + Property->GetName(), Audit);
                }
                else
                {
                    if (StructProperty->Struct->GetStructureSize() != sizeof(FSoftObjectPath)) { return false; }
                    Soft = *static_cast<const FSoftObjectPath*>(Value);
                }
                It.SkipRecursiveProperty(); bReference = true;
            }
        }
        else if (const FWeakObjectProperty* WeakProperty = CastField<FWeakObjectProperty>(Property))
        { Objects.Add(WeakProperty->GetObjectPropertyValue(Value)); bReference = true; }
        else if (const FLazyObjectProperty* LazyProperty = CastField<FLazyObjectProperty>(Property))
        { Objects.Add(LazyProperty->GetObjectPropertyValue(Value)); bReference = true; }
        else if (CastField<FDelegateProperty>(Property))
        { Objects.Add(static_cast<const FScriptDelegate*>(Value)->GetUObject()); bReference = true; }
        else if (const FMulticastDelegateProperty* MulticastProperty = CastField<FMulticastDelegateProperty>(Property))
        { if (const auto* Delegates = MulticastProperty->GetMulticastDelegate(Value))
            { for (const UObject* Object : Delegates->GetAllObjectsEvenIfUnreachable()) { Objects.Add(Object); } } bReference = true; }
        if (bReference && ++Audit.References > MaxReferences) { return false; }
        if (!Soft.IsNull()) { AuditToken(Soft.ToString(), O->GetPathName() + TEXT(".") + Property->GetName(), Audit); AuditObject(Soft.ResolveObject(), Audit); }
        for (const UObject* Object : Objects) { AuditObject(Object, Audit); }
    }
    return true;
}

bool Exists(const FString& Package)
{ FString Filename; return FPackageName::TryConvertLongPackageNameToFilename(Package, Filename, FPackageName::GetAssetPackageExtension()) && IFileManager::Get().FileExists(*Filename); }

void AuditPair(UBehaviorTree* Tree, UBlackboardData* Requested, FResult& R)
{
    FAudit Audit;
    for (const UObject* Root : { static_cast<const UObject*>(Tree), static_cast<const UObject*>(Tree->BlackboardAsset.Get()), static_cast<const UObject*>(Requested) })
    { if (Root) { Audit.Packages.AddUnique(Root->GetOutermost()->GetName()); AuditObject(Root, Audit); } }
    // Existing inspectors have already validated the full board ancestry/type sources.
    // Include independent requested and assigned parents in RequirePersisted roots.
    for (UBlackboardData* Board : { Tree->BlackboardAsset.Get(), Requested })
    { int32 Count = 0; for (UBlackboardData* Parent = Board; Parent; Parent = Parent->Parent.Get())
        { if (++Count > 64) { Audit.bLiveComplete = false; Audit.Issues.AddUnique(TEXT("Blackboard parent audit exceeded its bound.")); break; } Audit.Packages.AddUnique(Parent->GetOutermost()->GetName()); } }
    for (int32 Index = 0; Index < Audit.Packages.Num(); ++Index)
    {
        if (Index >= MaxAuditPackages || Audit.Packages.Num() > MaxAuditPackages)
        { Audit.bLiveComplete = false; Audit.Issues.AddUnique(TEXT("Loaded dependency package closure exceeded 256 packages.")); break; }
        const FString PackageName = Audit.Packages[Index];
        if (!Exists(PackageName)) { Audit.Issues.AddUnique(TEXT("RequirePersisted dependency file missing: ") + PackageName); }
        UPackage* Package = FindPackage(nullptr, *PackageName);
        if (!Package) { continue; }
        // For an external already saved clean package, on-disk registry closure below
        // supplies dependencies. Root assets and unsaved/dirty external packages also
        // receive the original live serialized/property-reference audit.
        const bool RootPackage = PackageName == Tree->GetOutermost()->GetName() || PackageName == Requested->GetOutermost()->GetName()
            || (Tree->BlackboardAsset && PackageName == Tree->BlackboardAsset->GetOutermost()->GetName());
        if (!RootPackage && !Package->IsDirty() && Exists(PackageName)) { continue; }
        TArray<UObject*> Exports;
        GetObjectsWithPackage(Package, Exports, EGetObjectsFlags::IncludeNestedObjects, RF_Transient, EInternalObjectFlags::Garbage);
        Audit.Exports += Exports.Num();
        if (Audit.Exports > MaxExports) { Audit.bLiveComplete = false; Audit.Issues.AddUnique(TEXT("Serialized package exports exceeded 16384.")); break; }
        for (UObject* Object : Exports)
        {
            AuditObject(Object, Audit);
            if (!Loaded(Object)) { Audit.bLiveComplete = false; continue; }
            TArray<UObject*> References;
            FReferenceFinder Finder(References, nullptr, false, true, false, true); Finder.FindReferences(Object);
            Audit.References += References.Num();
            if (Audit.References > MaxReferences || !AuditProperties(Object, Audit))
            { Audit.bLiveComplete = false; Audit.Issues.AddUnique(TEXT("Serialized reference/property audit bound or export observation failed.")); break; }
            for (UObject* Reference : References) { AuditObject(Reference, Audit); }
        }
        if (!Audit.bLiveComplete) { break; }
    }
    const auto Registry = ReadPackageDependencies(Audit.Packages, true, 4096);
    R.Data->SetObjectField(TEXT("persisted_dependency_observation"), Registry);
    bool RegistryPass = false;
    if (!Registry->TryGetBoolField(TEXT("passed"), RegistryPass) || !RegistryPass)
    { Audit.Issues.AddUnique(TEXT("RequirePersisted AssetRegistry dependency audit is incomplete or contains forbidden references; see its actual observation.")); }
    auto Scope = MakeShared<FJsonObject>(); Scope->SetNumberField(TEXT("loaded_packages_requested"), Audit.Packages.Num());
    Scope->SetNumberField(TEXT("serialized_exports_visited"), Audit.Exports); Scope->SetNumberField(TEXT("property_values_visited"), Audit.Values);
    Scope->SetNumberField(TEXT("reference_values_visited"), Audit.References); Scope->SetBoolField(TEXT("live_serialized_observations_complete"), Audit.bLiveComplete);
    Scope->SetBoolField(TEXT("asset_registry_refresh_dispatched"), false); Scope->SetBoolField(TEXT("packaging_verified"), false);
    Scope->SetStringField(TEXT("scope"), TEXT("RequirePersisted selected BT/assigned BB/requested BB and parents; live serialized/soft/weak/delegate references with bounded unsaved loaded closure plus current saved package dependency registry. No registry rescan, external load, cooking or gameplay execution."));
    R.Data->SetObjectField(TEXT("dependency_audit_scope"), Scope);
    R.Data->SetArrayField(TEXT("mcp_dependency_audit_issues"), Strings(Audit.Issues));
    R.Data->SetBoolField(TEXT("mcp_dependency_audit_pass"), Audit.bLiveComplete && RegistryPass && Audit.Issues.IsEmpty());
}

bool Move(const FBTCompositeChild& Child, const FName Key)
{ const UBTTask_MoveTo* Task = Cast<UBTTask_MoveTo>(Child.ChildTask.Get()); return Task && Task->GetSelectedBlackboardKey() == Key; }
bool Rotate(const FBTCompositeChild& Child)
{ const UBTTask_RotateToFaceBBEntry* Task = Cast<UBTTask_RotateToFaceBBEntry>(Child.ChildTask.Get()); return Task && Task->GetSelectedBlackboardKey() == TargetKey; }

TSharedRef<FJsonObject> Checks(UBehaviorTree* Tree, UBlackboardData* Requested, bool AuditPass)
{
    const UBTCompositeNode* Root = Tree->RootNode.Get();
    const bool Selector = Root && Root->IsA<UBTComposite_Selector>();
    const UBTCompositeNode* Alert = Root && Root->Children.Num() >= 1 ? Root->Children[0].ChildComposite.Get() : nullptr;
    bool Abort = false;
    if (Root && Root->Children.Num() >= 1)
    { for (const auto& Item : Root->Children[0].Decorators)
        { const UBTDecorator_BlackboardBase* Decorator = Cast<UBTDecorator_BlackboardBase>(Item.Get());
            if (Decorator && Decorator->GetSelectedBlackboardKey() == TargetKey
                && (Decorator->GetFlowAbortMode() == EBTFlowAbortMode::LowerPriority || Decorator->GetFlowAbortMode() == EBTFlowAbortMode::Both)) { Abort = true; break; } } }
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("behavior_tree_loaded"), true); J->SetBoolField(TEXT("blackboard_loaded"), true);
    J->SetBoolField(TEXT("root_sequence"), Root && Root->IsA<UBTComposite_Sequence>()); J->SetBoolField(TEXT("root_selector"), Selector);
    J->SetBoolField(TEXT("two_children"), Root && Root->Children.Num() == 2);
    J->SetBoolField(TEXT("selector_chase_patrol"), Selector && Root->Children.Num() == 2 && Move(Root->Children[0], TargetKey) && Move(Root->Children[1], PatrolKey));
    J->SetBoolField(TEXT("selector_alert_chase_patrol"), Selector && Root->Children.Num() == 2 && Alert && Alert->IsA<UBTComposite_Sequence>()
        && Alert->Children.Num() >= 3 && Rotate(Alert->Children[0]) && Alert->Children[1].ChildTask && Alert->Children[1].ChildTask->IsA<UBTTask_Wait>()
        && Move(Alert->Children[2], TargetKey) && Move(Root->Children[1], PatrolKey));
    J->SetBoolField(TEXT("chase_target_abort_lower_priority"), Abort);
    J->SetBoolField(TEXT("target_actor_key"), Requested->GetKeyID(TargetKey) != FBlackboard::InvalidKey);
    J->SetBoolField(TEXT("patrol_location_key"), Requested->GetKeyID(PatrolKey) != FBlackboard::InvalidKey);
    J->SetBoolField(TEXT("mcp_dependency_audit"), AuditPass);
    J->SetBoolField(TEXT("editor_graph_present"), Tree->BTGraph && Tree->BTGraph->Nodes.Num() >= 4); return J;
}
}

Function::FResult InspectBehaviorAssets(const FString& BehaviorTreePath, const FString& BlackboardPath)
{
    FResult R; R.Data->SetStringField(TEXT("asset_type"), TEXT("behavior_assets")); R.Data->SetArrayField(TEXT("asset_load_calls"), {});
    R.Data->SetNumberField(TEXT("asset_load_call_count"), 0); R.Data->SetArrayField(TEXT("inspection_receipts"), {}); R.Data->SetNumberField(TEXT("delegated_inspection_count"), 0);
    R.Data->SetBoolField(TEXT("runtime_execution_verified"), false); R.Data->SetBoolField(TEXT("explicit_cache_refresh_dispatched"), false);
    R.Data->SetBoolField(TEXT("implicit_postload_possible"), false); R.Data->SetBoolField(TEXT("load_side_effects_measured"), false);
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public AIModule/CoreUObject/AssetRegistry and existing finite BT/BB inspectors; hidden Editor registration adapter, no ToolsetAPI delegation or ServerRPC."));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Original ordered BT then independent requested BB LoadObject calls, followed by strict existing bounded shared readbacks and dependency observation. No explicit mutation/cache refresh/compile/save/evaluation. PostLoad, first wrapped library CDO and registry caches are not whole-call zero-work proof."));
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("behavior_assets_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); return R; }
    FString TreePath, BoardPath;
    if (!ExactPath(BehaviorTreePath, TreePath, R) || !ExactPath(BlackboardPath, BoardPath, R)) { return R; }
    R.Data->SetStringField(TEXT("behavior_tree_path"), TreePath); R.Data->SetStringField(TEXT("blackboard_path"), BoardPath);
    R.Stage = TEXT("resolve_pair");
    UObject* BeforeTree = FindObject<UObject>(nullptr, *TreePath);
    UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *TreePath); TStrongObjectPtr<UBehaviorTree> KeepTree(Tree);
    LoadReceipt(R, TEXT("behavior_tree"), TreePath, BeforeTree, Tree);
    UObject* BeforeBoard = FindObject<UObject>(nullptr, *BoardPath);
    UBlackboardData* Requested = LoadObject<UBlackboardData>(nullptr, *BoardPath); TStrongObjectPtr<UBlackboardData> KeepBoard(Requested);
    LoadReceipt(R, TEXT("requested_blackboard"), BoardPath, BeforeBoard, Requested);
    // Both original loads precede either failure check. Type/owner admission remains
    // at least as strict as the current common readers; a second load is not fallback.
    if (!Ordinary(Tree, TreePath, UBehaviorTree::StaticClass()))
    { Fail(R, TEXT("behavior_assets_behavior_tree_unavailable"), TEXT("The exact native ordinary BehaviorTree could not be resolved; both ordered load receipts are retained.")); return R; }
    if (!Ordinary(Requested, BoardPath, UBlackboardData::StaticClass()))
    { Fail(R, TEXT("behavior_assets_blackboard_unavailable"), TEXT("The exact native ordinary requested Blackboard could not be resolved; both ordered load receipts are retained.")); return R; }
    R.Data->SetStringField(TEXT("assigned_blackboard_path"), Tree->BlackboardAsset ? Tree->BlackboardAsset->GetPathName() : FString());
    R.Data->SetBoolField(TEXT("requested_blackboard_matches_assigned"), Tree->BlackboardAsset.Get() == Requested);
    R.Stage = TEXT("shared_readback"); TSharedPtr<FJsonObject> TreeSnapshot, BoardSnapshot;
    if (!ReadCommon(BehaviorTree::InspectAsset(TreePath), TEXT("behavior_tree_observation"), R, TreeSnapshot)
        || !ReadCommon(Blackboard::InspectAsset(BoardPath), TEXT("requested_blackboard_observation"), R, BoardSnapshot)) { return R; }
    const auto TreeState = TreeSnapshot->GetObjectField(TEXT("state")), BoardState = BoardSnapshot->GetObjectField(TEXT("state"));
    for (const TCHAR* Key : { TEXT("root_node_loaded"), TEXT("root_node_class"), TEXT("root_node_name"), TEXT("tree_shape"), TEXT("root_child_count"),
        TEXT("root_service_count"), TEXT("root_decorator_count"), TEXT("root_services"), TEXT("behavior_tree_children"), TEXT("node_count"),
        TEXT("traversal_truncated"), TEXT("editor_graph_loaded"), TEXT("editor_graph_node_count") })
    { const auto Value = TreeState->TryGetField(Key); if (Value.IsValid()) { R.Data->SetField(Key, Value); } }
    R.Data->SetBoolField(TEXT("behavior_tree_loaded"), true); R.Data->SetBoolField(TEXT("blackboard_loaded"), true);
    R.Data->SetStringField(TEXT("behavior_tree_class"), Tree->GetClass()->GetName()); R.Data->SetStringField(TEXT("blackboard_class"), Requested->GetClass()->GetName());
    R.Data->SetArrayField(TEXT("blackboard_keys"), BoardState->GetArrayField(TEXT("own_keys")));
    R.Data->SetArrayField(TEXT("blackboard_inherited_keys"), BoardState->GetArrayField(TEXT("inherited_keys")));
    R.Data->SetStringField(TEXT("blackboard_parent"), BoardState->GetStringField(TEXT("parent_blackboard")));
    R.Data->SetBoolField(TEXT("common_initial_observations_complete"), true);
    R.Stage = TEXT("dependency_audit"); AuditPair(Tree, Requested, R);
    R.Stage = TEXT("final_readback"); TSharedPtr<FJsonObject> FinalTree, FinalBoard;
    if (!ReadCommon(BehaviorTree::InspectAsset(TreePath), TEXT("behavior_tree_final_readback"), R, FinalTree, false)
        || !ReadCommon(Blackboard::InspectAsset(BoardPath), TEXT("requested_blackboard_final_readback"), R, FinalBoard, false)) { return R; }
    const bool TreePreserved = JsonText(TreeSnapshot.ToSharedRef()) == JsonText(FinalTree.ToSharedRef());
    const bool BoardPreserved = JsonText(BoardSnapshot.ToSharedRef()) == JsonText(FinalBoard.ToSharedRef());
    R.Data->SetBoolField(TEXT("behavior_tree_observation_preserved"), TreePreserved);
    R.Data->SetBoolField(TEXT("requested_blackboard_observation_preserved"), BoardPreserved);
    if (!TreePreserved || !BoardPreserved)
    {
        // Only retain a second full snapshot when it contains an actual difference.
        if (!TreePreserved) { R.Data->SetObjectField(TEXT("behavior_tree_changed_observation"), FinalTree); }
        if (!BoardPreserved) { R.Data->SetObjectField(TEXT("requested_blackboard_changed_observation"), FinalBoard); }
        R.bChanged = true;
        Fail(R, TEXT("behavior_assets_inspection_state_changed"), TEXT("Complete shared observations changed during the dependency read; exact changed snapshots are retained without an explicit writer.")); return R;
    }
    R.Data->SetObjectField(TEXT("checks"), Checks(Tree, Requested, R.Data->GetBoolField(TEXT("mcp_dependency_audit_pass"))));
    R.Data->SetBoolField(TEXT("readback_confirmed"), true); R.Data->SetBoolField(TEXT("read_only_after_initial_resolution"), true);
    R.Data->SetStringField(TEXT("verification"), TEXT("bounded shared native BT and independently requested Blackboard observations; checkFalse/auditFalse are inspection data, not top-level errors"));
    R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}
