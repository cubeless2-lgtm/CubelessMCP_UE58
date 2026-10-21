// Original blank native UEnvQuery creation; no query execution or editor-graph authoring.
#include "Compatibility/UE58/CubelessEnvQueryApi.h"
#include "Compatibility/UE58/CubelessBehaviorTreeApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::EnvQuery
{
namespace
{
constexpr int32 MaxNodes = 1000, MaxProperties = 128, MaxText = 262144, MaxPackages = 4096;
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
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), R.Operation); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetBoolField(TEXT("success"), R.bSuccess); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); R.Data->SetBoolField(TEXT("changed"), R.bChanged); }
    else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); R.Data->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown); J->SetBoolField(TEXT("postcondition_passed"), R.bPost);
    J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false);
    R.Data->SetBoolField(TEXT("save_requested"), R.bSaveRequested); R.Data->SetBoolField(TEXT("save_asset_requested"), R.bSaveRequested); R.Data->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
void Init(FResult& R, const TCHAR* Operation, bool bSave)
{
    R.Operation = Operation; R.bSaveRequested = bSave;
    for (const TCHAR* K : { TEXT("asset_load_dispatched"), TEXT("implicit_postload_possible"), TEXT("asset_registry_publication_dispatched"),
        TEXT("asset_allocation_attempted"), TEXT("package_fully_load_dispatched"), TEXT("implicit_package_load_possible"), TEXT("created"), TEXT("asset_saved"), TEXT("graph_creation_attempted"),
        TEXT("option_creation_attempted"), TEXT("runtime_execution_verified"), TEXT("query_execution_dispatched"), TEXT("implicit_engine_work_measured") }) { R.Data->SetBoolField(K, false); }
    R.Data->SetNumberField(TEXT("asset_load_count"), 0); R.Data->SetNumberField(TEXT("save_attempt_count"), 0); R.Data->SetArrayField(TEXT("save_calls"), {});
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public CoreUObject/AIModule/AssetRegistry/EditorAssetLibrary; no EQS Editor factory, official Toolset delegate or ServerRPC"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Fresh exact native UEnvQuery only; intrinsic PostInitProperties assigns QueryName, explicit asset resolution may run PostLoad. No options, generator/test authoring, editor graph creation, query execution or compile. Allocation/publication/save prefixes are retained without rollback or replay."));
    R.Data->SetStringField(TEXT("inspection_scope"), TEXT("Bounded actual native query/options/editor graph identity and reflected summary after initial resolution. Summary is not complete nonblank dependency-independence, generator/test correctness, graph-authoring or runtime proof."));
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"), O ? O->GetPathName() : TEXT("")); J->SetStringField(TEXT("class"), O ? O->GetClass()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("outer"), O && O->GetOuter() ? O->GetOuter()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("object_flags"), O ? FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags())) : TEXT("0")); return J;
}
bool Native(UClass* C, const TCHAR* Package, bool bAbstract = false)
{ return Loaded(C) && C->HasAnyClassFlags(CLASS_Native) && !C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
    && (bAbstract || !C->HasAnyClassFlags(CLASS_Abstract)) && C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)
    && C->GetOutermost()->GetName() == Package && !C->GetOutermost()->IsDirty(); }
bool Properties(UObject* O, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(O)) { return false; } int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (Skip.Contains(P->GetFName())) { continue; } if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64) { return false; }
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I)
        {
            FString S; const void* V = P->ContainerPtrToValuePtr<void>(O, I); if (!P->ExportText_Direct(S, V, V, O, PPF_None)) { return false; }
            Chars += S.Len(); if (Chars > MaxText) { return false; } Values.Add(MakeShared<FJsonValueString>(S));
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName());
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(P->GetPropertyFlags()))); Row->SetNumberField(TEXT("array_dim"), P->ArrayDim); Row->SetArrayField(TEXT("values"), Values); J->SetObjectField(P->GetName(), Row);
    }
    return true;
}
bool NativeSource(TSharedRef<FJsonObject>& J)
{
    UClass* C = UEnvQuery::StaticClass(); const auto Id = Identity(C); for (const auto& P : Id->Values) { J->SetField(P.Key, P.Value); } J->SetBoolField(TEXT("native_identity_verified"), false);
    if (!Native(C, TEXT("/Script/AIModule")) || C->GetPathName() != TEXT("/Script/AIModule.EnvQuery") || C->IsEditorOnly()) { return false; }
    J->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags()))); J->SetBoolField(TEXT("package_dirty"), C->GetOutermost()->IsDirty());
    UObject* CDO = C->GetDefaultObject(false); J->SetBoolField(TEXT("cdo_exists"), CDO != nullptr);
    if (CDO)
    {
        J->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); auto P = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("existing_cdo_properties"), P);
        if (!Loaded(CDO) || CDO->GetClass() != C || CDO->GetOuter() != C->GetOuter() || !CDO->HasAnyFlags(RF_ClassDefaultObject) || !Properties(CDO, {}, P)) { return false; }
    }
    J->SetBoolField(TEXT("native_identity_verified"), true); return true;
}
bool ProtectSource(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, bool bAllowLifecycleCDO, bool& bNewCDO)
{
    auto Expected = MakeShared<FJsonObject>(); for (const auto& P : Before->Values) { Expected->SetField(P.Key, P.Value); } bNewCDO = false;
    if (!Before->GetBoolField(TEXT("cdo_exists")) && After->GetBoolField(TEXT("cdo_exists")))
    {
        if (!bAllowLifecycleCDO) { return false; } Expected->SetBoolField(TEXT("cdo_exists"), true); Expected->SetObjectField(TEXT("existing_cdo"), After->GetObjectField(TEXT("existing_cdo")));
        Expected->SetObjectField(TEXT("existing_cdo_properties"), After->GetObjectField(TEXT("existing_cdo_properties"))); bNewCDO = true;
    }
    return Text(Expected) == Text(After);
}
bool Disk(const FString& PackageName, TSharedRef<FJsonObject>& J)
{
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    J->SetStringField(TEXT("path"), Filename); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size));
    if (Size < 0) { J->SetStringField(TEXT("sha1"), TEXT("")); return true; } TArray<uint8> Bytes;
    if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; }
    uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& Excluded, TSharedRef<FJsonObject>& J)
{
    int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == Excluded) { continue; } if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetPathName(), It->IsDirty()); } return true;
}
bool ExactPath(const FString& Input, FString& Path, FResult& R)
{
    Path = FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd(); Path.TrimQuotesInline(); Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!Path.Contains(TEXT("."))) { Path += TEXT(".") + FPackageName::GetLongPackageAssetName(Path); } FString PackageName, ObjectName;
    if (Path.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(Path) || !Path.Split(TEXT("."), &PackageName, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
        || ObjectName != FPackageName::GetLongPackageAssetName(PackageName) || !(PackageName.StartsWith(TEXT("/Game/")) || PackageName.StartsWith(TEXT("/Engine/"))))
    { return Fail(R, TEXT("eqs_asset_path_invalid"), TEXT("Use a bounded exact ordinary /Game or /Engine asset object path or package; other mounts/subobjects remain unsupported.")); } return true;
}
bool EditorState(FResult& R)
{ return (IsEditorGameThread() && !IsPlaySessionActive()) || Fail(R, TEXT("eqs_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); }
UEnvQuery* Resolve(const FString& Path, FResult& R)
{
    UObject* O = FindObject<UObject>(nullptr, *Path);
    if (!O) { R.Data->SetBoolField(TEXT("asset_load_dispatched"), true); R.Data->SetBoolField(TEXT("implicit_postload_possible"), true); R.Data->SetNumberField(TEXT("asset_load_count"), 1); R.Data->SetStringField(TEXT("last_explicit_load_path"), Path); O = LoadObject<UObject>(nullptr, *Path); }
    auto* Q = Cast<UEnvQuery>(O);
    if (!Loaded(Q) || Q->GetClass() != UEnvQuery::StaticClass() || Q->GetPathName() != Path || Q->GetOuter() != Q->GetOutermost()
        || Q->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || Q->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor))
    { Fail(R, TEXT("eqs_native_asset_required"), TEXT("Requires an ordinary readable exact native UEnvQuery; custom/generated/transient query variants remain incomplete.")); return nullptr; } return Q;
}
bool QueryState(UEnvQuery* Q, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(Q) || Q->GetClass() != UEnvQuery::StaticClass() || Q->GetOuter() != Q->GetOutermost() || Q->GetOptions().Num() > MaxNodes) { return false; }
    J = Identity(Q); J->SetStringField(TEXT("asset_type"), TEXT("eqs")); J->SetStringField(TEXT("asset_path"), Q->GetPathName()); J->SetStringField(TEXT("query_name"), Q->GetQueryName().ToString());
    J->SetBoolField(TEXT("package_dirty"), Q->GetOutermost()->IsDirty()); J->SetBoolField(TEXT("strip_from_client_builds"), Q->bStripFromClientBuilds != 0); J->SetNumberField(TEXT("option_count"), Q->GetOptions().Num());
    auto P = MakeShared<FJsonObject>(); if (!Properties(Q, {}, P)) { return false; } J->SetObjectField(TEXT("query_properties"), P);
    TArray<TSharedPtr<FJsonValue>> Options; TSet<const UObject*> Seen; int32 ObjectCount = 0;
    for (UEnvQueryOption* Option : Q->GetOptions())
    {
        if (!Loaded(Option) || Option->GetClass() != UEnvQueryOption::StaticClass() || Option->GetTypedOuter<UEnvQuery>() != Q || Seen.Contains(Option) || ++ObjectCount > MaxNodes) { return false; } Seen.Add(Option);
        auto Row = Identity(Option); auto OP = MakeShared<FJsonObject>(); if (!Properties(Option, {}, OP)) { return false; } Row->SetObjectField(TEXT("properties"), OP);
        if (Option->Generator)
        {
            UEnvQueryGenerator* G = Option->Generator;
            if (!Loaded(G) || !Native(G->GetClass(), TEXT("/Script/AIModule")) || G->GetTypedOuter<UEnvQuery>() != Q || Seen.Contains(G) || ++ObjectCount > MaxNodes) { return false; } Seen.Add(G);
            auto GP = MakeShared<FJsonObject>(); auto GR = Identity(G); if (!Properties(G, {}, GP)) { return false; } GR->SetObjectField(TEXT("properties"), GP); Row->SetObjectField(TEXT("generator"), GR);
        }
        else { Row->SetObjectField(TEXT("generator"), MakeShared<FJsonObject>()); }
        if (Option->Tests.Num() > MaxNodes) { return false; } TArray<TSharedPtr<FJsonValue>> Tests;
        for (UEnvQueryTest* T : Option->Tests)
        {
            if (!Loaded(T) || !Native(T->GetClass(), TEXT("/Script/AIModule")) || T->GetTypedOuter<UEnvQuery>() != Q || Seen.Contains(T) || ++ObjectCount > MaxNodes) { return false; } Seen.Add(T);
            auto TP = MakeShared<FJsonObject>(); auto TR = Identity(T); if (!Properties(T, {}, TP)) { return false; } TR->SetObjectField(TEXT("properties"), TP); Tests.Add(MakeShared<FJsonValueObject>(TR));
        }
        Row->SetArrayField(TEXT("tests"), Tests); Options.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("options"), Options); auto Graph = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("editor_graph_present"), Q->EdGraph != nullptr); J->SetNumberField(TEXT("editor_graph_node_count"), Q->EdGraph ? Q->EdGraph->Nodes.Num() : 0);
    if (Q->EdGraph)
    {
        UEdGraph* G = Q->EdGraph;
        if (!Loaded(G) || G->GetOuter() != Q || !Native(G->GetClass(), TEXT("/Script/EnvironmentQueryEditor")) || G->Nodes.Num() > MaxNodes) { return false; }
        Graph = Identity(G); auto GP = MakeShared<FJsonObject>(); if (!Properties(G, {}, GP)) { return false; } Graph->SetObjectField(TEXT("properties"), GP); TArray<TSharedPtr<FJsonValue>> Nodes;
        for (UEdGraphNode* N : G->Nodes)
        {
            if (!Loaded(N) || N->GetOuter() != G || !Native(N->GetClass(), TEXT("/Script/EnvironmentQueryEditor")) || Seen.Contains(N)) { return false; } Seen.Add(N);
            auto NR = Identity(N); NR->SetStringField(TEXT("node_guid"), N->NodeGuid.ToString()); auto NP = MakeShared<FJsonObject>(); if (!Properties(N, {}, NP)) { return false; } NR->SetObjectField(TEXT("properties"), NP); Nodes.Add(MakeShared<FJsonValueObject>(NR));
        }
        Graph->SetArrayField(TEXT("nodes"), Nodes);
    }
    J->SetObjectField(TEXT("editor_graph"), Graph); J->SetBoolField(TEXT("observations_complete"), true); return true;
}
struct FObservation
{ TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), Source = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>(); };
bool Observe(UEnvQuery* Q, FObservation& O)
{ return QueryState(Q, O.State) && NativeSource(O.Source) && Disk(Q->GetOutermost()->GetName(), O.DiskState) && Dirty(Q->GetOutermost()->GetName(), O.OtherDirty); }
TSharedRef<FJsonObject> Observation(const FObservation& O)
{ auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_source"), O.Source); J->SetObjectField(TEXT("disk"), O.DiskState); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); return J; }
void Publish(FResult& R, const FObservation& O, const TCHAR* Label)
{
    R.Data->SetObjectField(Label, Observation(O)); for (const auto& P : O.State->Values) { R.Data->SetField(P.Key, P.Value); }
    const bool Blank = O.State->GetNumberField(TEXT("option_count")) == 0 && !O.State->GetBoolField(TEXT("editor_graph_present"));
    R.Data->SetBoolField(TEXT("readback_confirmed"), true); R.Data->SetBoolField(TEXT("blank_native_dependencies_verified"), Blank);
    R.Data->SetBoolField(TEXT("complete_nonblank_dependency_audit"), false); R.Data->SetBoolField(TEXT("graph_authoring_verified"), false);
    TArray<TSharedPtr<FJsonValue>> Issues; if (!Blank) { Issues.Add(MakeShared<FJsonValueString>(TEXT("Nonblank option/provider/editor-graph dependency independence is not fully audited by this finite summary."))); }
    const bool Persisted = O.DiskState->GetBoolField(TEXT("exists")) && O.DiskState->GetNumberField(TEXT("bytes")) > 0;
    if (R.Operation == TEXT("inspect_ai_asset_structure") && !Persisted) { Issues.Add(MakeShared<FJsonValueString>(TEXT("Query file is not persisted; in-memory summary remains available."))); }
    R.Data->SetBoolField(TEXT("persistence_verified"), Persisted); R.Data->SetArrayField(TEXT("audit_issues"), Issues); R.Data->SetBoolField(TEXT("mcp_dependency_audit"), Issues.IsEmpty());
    R.Data->SetArrayField(TEXT("mcp_dependency_audit_issues"), Issues); R.Data->SetBoolField(TEXT("mcp_dependency_audit_pass"), Issues.IsEmpty());
    R.Data->SetStringField(TEXT("dependency_audit_scope"), TEXT("Exact native blank query has no option/graph references; creation uses in-memory admission before optional save, inspection additionally reports absent persistence. Nonblank dependency inventories remain incomplete."));
}
bool Retained(UEnvQuery* Q, const FObservation& Before, FObservation& After, FResult& R, bool bAllowCDO)
{
    if (!Observe(Q, After)) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("eqs_retained_state_unobservable"), TEXT("Retained target/source/disk state is incomplete; do not replay allocation or save.")); } Publish(R, After, TEXT("after"));
    R.bChanged |= Text(Before.State) != Text(After.State); bool NewCDO = false;
    const bool SourceOK = ProtectSource(Before.Source, After.Source, bAllowCDO, NewCDO), DirtyOK = Text(Before.OtherDirty) == Text(After.OtherDirty), DiskOK = Text(Before.DiskState) == Text(After.DiskState);
    R.Data->SetBoolField(TEXT("existing_native_defaults_preserved"), SourceOK); R.Data->SetBoolField(TEXT("implicit_native_cdo_creation_observed"), NewCDO || (R.Data->HasField(TEXT("implicit_native_cdo_creation_observed")) && R.Data->GetBoolField(TEXT("implicit_native_cdo_creation_observed")))); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), DirtyOK); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), DiskOK);
    if (!SourceOK || !DirtyOK || !DiskOK) { R.bPost = false; return Fail(R, TEXT("eqs_preservation_failed"), TEXT("Actual target retained but protected native defaults/other dirty states/disk changed; save suppressed.")); } return true;
}
bool Save(UEnvQuery* Q, const FObservation& Before, FResult& R)
{
    if (!R.bSaveRequested) { return true; } R.Stage = TEXT("save"); R.bSaveAttempted = true;
    auto Call = MakeShared<FJsonObject>(); Call->SetNumberField(TEXT("attempt_index"), 1); Call->SetBoolField(TEXT("package_dirty_before"), Q->GetOutermost()->IsDirty());
    const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(Q, false); Call->SetBoolField(TEXT("success"), Saved); Call->SetBoolField(TEXT("package_dirty_after"), Q->GetOutermost()->IsDirty());
    R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Call) }); R.Data->SetNumberField(TEXT("save_attempt_count"), 1);
    FObservation After; if (!Observe(Q, After)) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("eqs_save_state_unobservable"), TEXT("Save prefix retained; current RAM/source/disk state is incomplete and must not be replayed.")); } Publish(R, After, TEXT("after_save"));
    if (!Saved) { R.bKnown = false; R.bPost = false; R.Data->SetBoolField(TEXT("save_disk_outcome_known"), false); return Fail(R, TEXT("eqs_save_failed"), TEXT("The one exact save returned false; disk transaction uncertified, no save retry or cleanup occurred.")); }
    auto Expected = MakeShared<FJsonObject>(); for (const auto& P : Before.State->Values) { Expected->SetField(P.Key, P.Value); }
    const FString Flags = Before.State->GetStringField(TEXT("object_flags")); const uint64 Old = FCString::Strtoui64(*Flags, nullptr, 10);
    if (Flags != FString::Printf(TEXT("%llu"), Old)) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("eqs_save_flags_unobservable"), TEXT("Saved bytes retained; before-save identity flag text was noncanonical.")); }
    Expected->SetStringField(TEXT("object_flags"), FString::Printf(TEXT("%llu"), Old | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))); Expected->SetBoolField(TEXT("package_dirty"), false);
    bool NewCDO = false; const bool SourceOK = ProtectSource(Before.Source, After.Source, false, NewCDO) && Text(Before.OtherDirty) == Text(After.OtherDirty), StateOK = Text(Expected) == Text(After.State);
    R.bSaved = !Q->GetOutermost()->IsDirty() && After.DiskState->GetBoolField(TEXT("exists")) && After.DiskState->GetNumberField(TEXT("bytes")) > 0;
    R.Data->SetBoolField(TEXT("asset_saved"), R.bSaved); R.Data->SetBoolField(TEXT("save_state_preserved"), StateOK); R.Data->SetBoolField(TEXT("save_source_preserved"), SourceOK); R.Data->SetBoolField(TEXT("save_disk_outcome_known"), true);
    R.Data->SetStringField(TEXT("save_state_comparison_scope"), TEXT("Only exact audited query object flags predict before OR RF_WasLoaded|RF_LoadCompleted, and selected package dirty=false. Query properties/options/graph/native defaults/other dirty states remain exact; raw snapshots are retained."));
    if (!R.bSaved || !SourceOK || !StateOK) { R.bPost = false; return Fail(R, TEXT("eqs_save_postcondition_failed"), TEXT("Exact save returned true but clean file/state/source preservation was not certified; saved bytes and actual snapshots retained without replay.")); } return true;
}
}
FString CreateAsset(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset)
{
    FResult R; Init(R, TEXT("create_ai_eqs_query"), bSaveAsset); if (!EditorState(R)) { return Encode(R); }
    FString Root = AssetRoot.TrimStartAndEnd(); Root.TrimQuotesInline(); Root.ReplaceInline(TEXT("\\"), TEXT("/")); while (Root.EndsWith(TEXT("/"))) { Root.LeftChopInline(1); } FText Reason;
    if (AssetName.IsEmpty() || AssetName.Len() >= NAME_SIZE || FName(*AssetName).IsNone() || !FName::IsValidXName(AssetName, INVALID_OBJECTNAME_CHARACTERS, &Reason)
        || !(Root == TEXT("/Game") || Root.StartsWith(TEXT("/Game/"))) || !FPackageName::IsValidLongPackageName(Root))
    { Fail(R, TEXT("eqs_create_path_invalid"), TEXT("Use a valid bounded simple asset_name and /Game asset_root; existing destinations are never overwritten.")); return Encode(R); }
    const FString PackageName = Root + TEXT("/") + AssetName, Path = PackageName + TEXT(".") + AssetName; auto SourceBefore = MakeShared<FJsonObject>(), DiskBefore = MakeShared<FJsonObject>(), DirtyBefore = MakeShared<FJsonObject>();
    R.Data->SetObjectField(TEXT("native_source_before"), SourceBefore); R.Data->SetObjectField(TEXT("disk_before"), DiskBefore); R.Data->SetObjectField(TEXT("other_package_dirty_before"), DirtyBefore);
    if (Path.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(Path) || !NativeSource(SourceBefore) || !Disk(PackageName, DiskBefore) || !Dirty(PackageName, DirtyBefore))
    { Fail(R, TEXT("eqs_native_source_unobservable"), TEXT("Exact native class/default/destination observations incomplete before allocation; partial rows retained.")); return Encode(R); }
    if (FindPackage(nullptr, *PackageName) || FindObject<UObject>(nullptr, *Path) || DiskBefore->GetBoolField(TEXT("exists")) || FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path)).IsValid())
    { Fail(R, TEXT("eqs_target_exists"), TEXT("Destination exists; no replacement/allocation/save attempted.")); return Encode(R); }
    R.Stage = TEXT("create_asset"); R.bAttempted = true; R.Data->SetBoolField(TEXT("asset_allocation_attempted"), true);
    UPackage* Package = CreatePackage(*PackageName); TStrongObjectPtr<UPackage> KeepPackage(Package);
    if (Package) { R.Data->SetBoolField(TEXT("package_fully_load_dispatched"), true); R.Data->SetBoolField(TEXT("implicit_package_load_possible"), true); Package->FullyLoad(); }
    auto* Q = Package ? NewObject<UEnvQuery>(Package, UEnvQuery::StaticClass(), *AssetName, RF_Public | RF_Standalone | RF_Transactional) : nullptr; TStrongObjectPtr<UEnvQuery> Keep(Q); R.bChanged = Package != nullptr;
    if (!Loaded(Q)) { R.bKnown = false; Fail(R, TEXT("eqs_create_failed"), TEXT("Allocation prefix retained; no atomic cleanup/rollback/retry claimed.")); return Encode(R); }
    FObservation Factory; if (!Observe(Q, Factory)) { R.bKnown = false; Fail(R, TEXT("eqs_factory_state_unobservable"), TEXT("Allocated native query retained but factory/source state incomplete.")); return Encode(R); } Publish(R, Factory, TEXT("factory_default_state"));
    bool NewCDO = false; if (!ProtectSource(SourceBefore, Factory.Source, true, NewCDO) || Text(DiskBefore) != Text(Factory.DiskState) || Text(DirtyBefore) != Text(Factory.OtherDirty)
        || Q->GetPathName() != Path || Q->GetQueryName() != FName(*AssetName) || !Q->GetOptions().IsEmpty() || Q->EdGraph)
    { Fail(R, TEXT("eqs_factory_postcondition_failed"), TEXT("Allocated prefix retained but blank native factory identity/options/graph/source/defaults were not certified; publication/save suppressed.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("implicit_native_cdo_creation_observed"), NewCDO);
    Q->MarkPackageDirty(); Package->MarkPackageDirty(); R.Data->SetBoolField(TEXT("asset_registry_publication_dispatched"), true); FAssetRegistryModule::AssetCreated(Q);
    FObservation After; if (!Retained(Q, Factory, After, R, false)) { return Encode(R); }
    R.Data->SetBoolField(TEXT("created"), true); R.bChanged = true; R.bPost = true; if (!Save(Q, After, R)) { return Encode(R); } R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
FString InspectAsset(const FString& AssetPath)
{
    FResult R; Init(R, TEXT("inspect_ai_asset_structure"), false); FString Path; if (!EditorState(R) || !ExactPath(AssetPath, Path, R)) { return Encode(R); }
    // Preserve the existing BT/Blackboard dispatcher, including its loader diagnostics. No non-EQS resolution or CDO warming here.
    UObject* Existing = FindObject<UObject>(nullptr, *Path);
    if (Existing && Existing->GetClass()->GetPathName() != TEXT("/Script/AIModule.EnvQuery")) { return BehaviorTree::InspectAsset(AssetPath); }
    if (!Existing)
    {
        const FAssetData Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path));
        if (!Asset.IsValid() || Asset.AssetClassPath.ToString() != TEXT("/Script/AIModule.EnvQuery")) { return BehaviorTree::InspectAsset(AssetPath); }
    }
    UEnvQuery* Q = Resolve(Path, R); TStrongObjectPtr<UEnvQuery> Keep(Q); if (!Q) { return Encode(R); } FObservation Before, After;
    if (!Observe(Q, Before)) { Fail(R, TEXT("eqs_inspection_unobservable"), TEXT("Native bounded summary failed; custom/generated/nonowned/incomplete query/options/graphs remain unsupported.")); return Encode(R); } Publish(R, Before, TEXT("before"));
    if (!Retained(Q, Before, After, R, false)) { return Encode(R); }
    if (R.bChanged) { Fail(R, TEXT("eqs_inspection_state_changed"), TEXT("Repeated native summary changed after initial resolution; no explicit graph/option/cache/query/write/save was dispatched.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("read_only_after_initial_resolution"), true); R.bPost = true; R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
}
