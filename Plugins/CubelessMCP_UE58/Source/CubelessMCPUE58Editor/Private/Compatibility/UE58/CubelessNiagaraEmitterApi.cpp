#include "Compatibility/UE58/CubelessNiagaraEmitterApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraDataInterfaceArrayInt.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Misc/SecureHash.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterBase.h"
#include "NiagaraCommon.h"
#include "HAL/PlatformProperties.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::NiagaraEmitter
{
namespace
{
using Function::FResult;
constexpr int32 MaxEmitters = 128, MaxSources = 512, MaxVersions = 32, MaxPrivateScripts = 128;
constexpr int32 MaxNodes = 1024, MaxPins = 8192, MaxLinkRefs = 32768;
FString Path(const UObject* O) { return O ? O->GetPathName() : FString(); }
FString Guid(const FGuid& G) { return G.ToString(EGuidFormats::DigitsWithHyphens); }
FString Text(const TSharedRef<FJsonObject>& J)
{ FString Out; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out)); return Out; }
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects |
    RF_BeginDestroyed | RF_FinishDestroyed | RF_ClassDefaultObject | RF_ArchetypeObject); }
bool Fail(FResult& R, const TCHAR* Code, const TCHAR* Error)
{ if (R.ErrorCode.IsEmpty()) { R.ErrorCode = Code; R.Error = Error; } return false; }
bool Bounded(const FString& V, int32 Limit = 1024)
{ if (V.Len() > Limit) { return false; } for (TCHAR C : V) { if (C < 32 || C == 127) { return false; } } return true; }
bool ObjectPath(const FString& In, FString& Out)
{
    if (!Bounded(In)) { return false; } Out = In.TrimStartAndEnd();
    if (!Out.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(Out))
    { Out += TEXT(".") + FPackageName::GetLongPackageAssetName(Out); }
    return FPackageName::IsValidObjectPath(Out);
}
bool OptionalScalar(const FString& Json, TSharedPtr<FJsonValue>& Out)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    if (Json.Len() > 64 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Rows) ||
        Rows.Num() != 1 || !Rows[0].IsValid()) { return false; } Out = Rows[0]; return true;
}
bool Index(const FString& Json, int32& Out)
{
    TSharedPtr<FJsonValue> V; if (!OptionalScalar(Json, V)) { return false; }
    if (V->Type == EJson::Null) { Out = INDEX_NONE; return true; }
    double N; if (V->Type != EJson::Number || !V->TryGetNumber(N) || !FMath::IsFinite(N) ||
        N != FMath::TruncToDouble(N) || N < static_cast<double>(INDEX_NONE) || N > MAX_int32) { return false; }
    Out = static_cast<int32>(N); return true;
}
TSharedRef<FJsonObject> CompileState(UNiagaraSystem* S)
{
    auto J = MakeShared<FJsonObject>(); const bool Outstanding = S->HasOutstandingCompilationRequests(true), Needs = S->NeedsRequestCompile();
    J->SetBoolField(TEXT("outstanding_including_gpu"), Outstanding); J->SetBoolField(TEXT("needs_request_compile"), Needs);
    J->SetBoolField(TEXT("ready_to_run"), S->IsReadyToRun()); J->SetBoolField(TEXT("ready_to_save"), !Outstanding && !Needs);
    J->SetBoolField(TEXT("compile_success_verified"), false); J->SetBoolField(TEXT("poll_or_flush_invoked"), false); return J;
}
void Initialize(FResult& R, const FString& Target, bool bSave = false)
{
    R.Data->SetStringField(TEXT("requested_target_system_path"), Target); R.Data->SetBoolField(TEXT("asset_load_attempted"), false);
    R.Data->SetNumberField(TEXT("asset_load_count"),0);R.Data->SetArrayField(TEXT("asset_load_calls"),{});R.Data->SetArrayField(TEXT("primary_asset_resolutions"),{});
    R.Data->SetBoolField(TEXT("already_loaded_assets_required"), true); R.Data->SetBoolField(TEXT("compile_requested"), false);
    R.Data->SetBoolField(TEXT("compile_attempted"), false); R.Data->SetBoolField(TEXT("save_requested"), bSave);
    R.Data->SetBoolField(TEXT("save_attempted"), false); R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetBoolField(TEXT("save_deferred"), false);
    R.Data->SetBoolField(TEXT("blocking_wait_invoked"), false); R.Data->SetBoolField(TEXT("atomic_rollback_available"), false);
    R.Data->SetBoolField(TEXT("auto_retry_safe"), false); R.Data->SetBoolField(TEXT("explicit_user_parameter_writes"), false);
    R.Data->SetBoolField(TEXT("implicit_notification_callbacks_possible"), true); R.Data->SetBoolField(TEXT("full_legacy_niagara_parity"), false);
    R.Data->SetBoolField(TEXT("copy_source_user_parameters_supported"), false); R.Data->SetBoolField(TEXT("recursive_asset_dependency_audit_performed"), false);
    constexpr const TCHAR* UserCounts[] = {TEXT("source_user_parameter_count"), TEXT("copied_user_parameter_count"), TEXT("added_user_parameter_count"),
        TEXT("updated_user_parameter_count"), TEXT("copied_user_data_interface_parameter_count"), TEXT("created_user_data_interface_parameter_count"),
        TEXT("skipped_user_data_interface_parameter_count"), TEXT("skipped_user_missing_data_parameter_count"), TEXT("skipped_user_private_uobject_parameter_count")};
    for (const TCHAR* Field : UserCounts) { R.Data->SetNumberField(Field, 0); }
    R.Data->SetArrayField(TEXT("copied_user_parameters"), {}); R.Data->SetArrayField(TEXT("skipped_user_parameters"), {});
    R.Data->SetNumberField(TEXT("localized_private_scratch_pad_count"), 0); R.Data->SetNumberField(TEXT("localized_private_scratch_pad_reference_count"), 0);
    R.Data->SetArrayField(TEXT("localized_private_scratch_pad_scripts"), {});
    R.Data->SetStringField(TEXT("selector_policy"), TEXT("legacy first matching source index/name; emitter asset takes precedence"));
    R.Data->SetStringField(TEXT("refresh_basis"), TEXT("UE58 AddEmitterToSystem rebuilds system emitter nodes; engine async compile completion rebuilds compiled aliases/data/execution order; no removed RefreshSystemParametersFromEmitter call"));
}
FString PinId(const UEdGraphPin* P)
{ return P ? Path(P->GetOwningNode()) + TEXT(".") + P->PinName.ToString() + TEXT("/") + Guid(P->PinId) : FString(); }
TSharedRef<FJsonObject> GraphSnapshot(UNiagaraGraph* G, bool& Complete)
{
    Complete = false; auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("graph_path"), Path(G));
    if (!Loaded(G) || G->GetClass() != UNiagaraGraph::StaticClass() || G->Nodes.Num() > MaxNodes) { return J; }
    TSet<const UEdGraphPin*> Pins; TSet<const UEdGraphNode*> Nodes; int32 LinkRefs = 0;
    for (UEdGraphNode* N : G->Nodes)
    {
        if (!Loaded(N) || N->GetOuter() != G || !N->GetClass()->HasAllClassFlags(CLASS_Native) ||
            N->GetClass()->GetOutermost()->GetName() != TEXT("/Script/NiagaraEditor") || !N->NodeGuid.IsValid() ||
            Nodes.Contains(N) || N->Pins.Num() > MaxPins || Pins.Num() > MaxPins - N->Pins.Num()) { return J; }
        Nodes.Add(N);
        for (const UEdGraphPin* P : N->Pins)
        { if (!P || P->GetOwningNode() != N || !P->PinId.IsValid() || Pins.Contains(P)) { return J; } Pins.Add(P); }
    }
    TArray<TSharedPtr<FJsonValue>> NodeRows;
    for (UEdGraphNode* N : G->Nodes)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("node_path"), Path(N)); Row->SetStringField(TEXT("node_class"), Path(N->GetClass()));
        Row->SetStringField(TEXT("node_guid"), Guid(N->NodeGuid)); Row->SetNumberField(TEXT("position_x"), N->NodePosX); Row->SetNumberField(TEXT("position_y"), N->NodePosY);
        if (const auto* Call = Cast<UNiagaraNodeFunctionCall>(N))
        {
            Row->SetStringField(TEXT("function_script_path"), Path(Call->FunctionScript)); Row->SetStringField(TEXT("selected_script_version"), Guid(Call->SelectedScriptVersion));
            Row->SetStringField(TEXT("function_name"), Call->GetFunctionName()); Row->SetStringField(TEXT("function_script_asset_object_path"), Call->FunctionScriptAssetObjectPath.ToString());
        }
        if (const auto* Output = Cast<UNiagaraNodeOutput>(N))
        { Row->SetNumberField(TEXT("usage"), static_cast<int32>(Output->GetUsage())); Row->SetStringField(TEXT("usage_id"), Guid(Output->GetUsageId())); }
        TArray<TSharedPtr<FJsonValue>> PinRows;
        for (const UEdGraphPin* P : N->Pins)
        {
            if (P->LinkedTo.Num() > MaxLinkRefs || LinkRefs > MaxLinkRefs - P->LinkedTo.Num()) { return J; }
            LinkRefs += P->LinkedTo.Num(); TArray<FString> Links; TSet<const UEdGraphPin*> Seen;
            for (const UEdGraphPin* Q : P->LinkedTo)
            { if (!Q || !Pins.Contains(Q) || Seen.Contains(Q) || Q->Direction == P->Direction || !Q->LinkedTo.Contains(P)) { return J; } Seen.Add(Q); Links.Add(PinId(Q)); }
            Links.Sort(); auto Pin = MakeShared<FJsonObject>(); Pin->SetStringField(TEXT("pin_id"), PinId(P)); Pin->SetStringField(TEXT("pin_guid"), Guid(P->PinId));
            Pin->SetStringField(TEXT("name"), P->PinName.ToString()); Pin->SetNumberField(TEXT("direction"), static_cast<int32>(P->Direction));
            Pin->SetStringField(TEXT("category"), P->PinType.PinCategory.ToString()); Pin->SetStringField(TEXT("subcategory"), P->PinType.PinSubCategory.ToString());
            Pin->SetStringField(TEXT("subcategory_object_path"), Path(P->PinType.PinSubCategoryObject.Get())); Pin->SetNumberField(TEXT("container_type"), static_cast<int32>(P->PinType.ContainerType));
            Pin->SetBoolField(TEXT("is_const"), P->PinType.bIsConst); Pin->SetBoolField(TEXT("is_reference"), P->PinType.bIsReference);
            Pin->SetStringField(TEXT("raw_default_value"), P->DefaultValue); Pin->SetStringField(TEXT("autogenerated_default_value"), P->AutogeneratedDefaultValue);
            Pin->SetStringField(TEXT("default_object_path"), Path(P->DefaultObject)); Pin->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString());
            Pin->SetBoolField(TEXT("hidden"), P->bHidden); Pin->SetBoolField(TEXT("orphaned"), P->bOrphanedPin); Pin->SetBoolField(TEXT("default_value_is_ignored"), P->bDefaultValueIsIgnored);
            TArray<TSharedPtr<FJsonValue>> LinkRows; for (const FString& L : Links) { LinkRows.Add(MakeShared<FJsonValueString>(L)); }
            Pin->SetArrayField(TEXT("links"), LinkRows); PinRows.Add(MakeShared<FJsonValueObject>(Pin));
        }
        Row->SetArrayField(TEXT("pins"), PinRows); NodeRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("nodes"), NodeRows); J->SetNumberField(TEXT("node_count"), Nodes.Num()); J->SetNumberField(TEXT("pin_count"), Pins.Num());
    J->SetNumberField(TEXT("physical_link_count"), LinkRefs / 2); J->SetStringField(TEXT("graph_change_id"), Guid(G->GetChangeID()));
    J->SetBoolField(TEXT("observation_complete"), true); Complete = true; return J;
}
struct FSources
{
    TArray<UNiagaraScriptSource*> Sources; TSet<UNiagaraScript*> Scripts;
    bool AddSource(UNiagaraScriptSourceBase* Base)
    {
        auto* Source = Cast<UNiagaraScriptSource>(Base);
        if (!Loaded(Source) || Source->GetClass() != UNiagaraScriptSource::StaticClass() || Source->bIsCompilationCopy || Source->bIsReleased || !Loaded(Source->NodeGraph)) { return false; }
        Sources.AddUnique(Source); return Sources.Num() <= MaxSources;
    }
    bool AddScript(UNiagaraScript* Script)
    {
        if (!Loaded(Script) || Script->GetClass() != UNiagaraScript::StaticClass()) { return false; }
        if (Scripts.Contains(Script)) { return true; } Scripts.Add(Script); if (Scripts.Num() > MaxSources) { return false; }
        const auto Versions = Script->GetAllAvailableVersions(); if (Versions.IsEmpty() || Versions.Num() > MaxVersions) { return false; }
        for (const auto& V : Versions) { if (!Script->GetScriptData(V.VersionGuid) || !AddSource(Script->GetSource(V.VersionGuid))) { return false; } } return true;
    }
    bool AddContainer(UNiagaraScratchPadContainer* Container)
    { if (!Container) { return true; } if (!Loaded(Container) || Container->Scripts.Num() > MaxPrivateScripts) { return false; }
        for (auto Script : Container->Scripts) { if (!AddScript(Script)) { return false; } } return true; }
    bool AddEmitter(UNiagaraEmitter* Emitter, const FGuid& Version)
    {
        if (!Loaded(Emitter) || Emitter->GetClass() != UNiagaraEmitter::StaticClass()) { return false; }
        auto* Data = Emitter->GetEmitterData(Version);
        return Data && AddSource(Data->GraphSource) && AddContainer(Data->ScratchPads) && AddContainer(Data->ParentScratchPads);
    }
    bool AddSystem(UNiagaraSystem* System)
    {
        if (!Loaded(System) || System->GetEmitterHandles().Num() > MaxEmitters || System->ScratchPadScripts.Num() > MaxPrivateScripts ||
            !AddScript(System->GetSystemSpawnScript()) || !AddScript(System->GetSystemUpdateScript())) { return false; }
        for (auto Script : System->ScratchPadScripts) { if (!AddScript(Script)) { return false; } }
        for (const auto& H : System->GetEmitterHandles()) { const auto I = H.GetInstance(); if (H.GetEmitterMode() != ENiagaraEmitterMode::Standard || !AddEmitter(I.Emitter, I.Version)) { return false; } }
        return true;
    }
};
bool PrivateExternal(const UNiagaraSystem* Target, const UNiagaraScript* Script)
{ return Script && Script->GetOutermost() != Target->GetOutermost() && !Script->IsAsset() && !Script->HasAnyFlags(RF_Public | RF_Standalone); }
bool ExpandPrivateClosure(UNiagaraSystem* Target, FSources& Sources, TArray<UNiagaraScript*>& Private)
{
    for (int32 I = 0; I < Sources.Sources.Num(); ++I)
    {
        bool Complete; GraphSnapshot(Sources.Sources[I]->NodeGraph, Complete); if (!Complete) { return false; }
        for (UEdGraphNode* N : Sources.Sources[I]->NodeGraph->Nodes)
        {
            const auto* Call = Cast<UNiagaraNodeFunctionCall>(N); UNiagaraScript* Script = Call ? Call->FunctionScript.Get() : nullptr;
            if (PrivateExternal(Target, Script))
            { Private.AddUnique(Script); if (Private.Num() > MaxPrivateScripts || !Sources.AddScript(Script)) { return false; } }
        }
    }
    return true;
}
bool MapPin(UEdGraphNode* Node, EEdGraphPinDirection Direction, UEdGraphPin*& Out)
{
    Out = nullptr;
    for (UEdGraphPin* P : Node->Pins)
    {
        if (P && P->Direction == Direction && !P->bOrphanedPin && P->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType &&
            P->PinType.PinSubCategoryObject.Get() == FNiagaraTypeDefinition::GetParameterMapDef().GetScriptStruct() &&
            UEdGraphSchema_Niagara::PinToTypeDefinition(P) == FNiagaraTypeDefinition::GetParameterMapDef())
        { if (Out) { return false; } Out = P; }
    }
    return true;
}
bool SimpleSystemMapStack(UNiagaraGraph* Graph, UNiagaraNodeOutput* Output)
{
    UEdGraphPin* In = nullptr; if (!MapPin(Output, EGPD_Input, In) || !In) { return false; }
    TSet<UEdGraphNode*> Seen; Seen.Add(Output);
    for (int32 I = 0; I < MaxNodes; ++I)
    {
        if (In->LinkedTo.Num() != 1) { return false; } UEdGraphPin* Out = In->LinkedTo[0];
        UEdGraphNode* Node = Out ? Out->GetOwningNode() : nullptr; UEdGraphPin* ActualOut = nullptr;
        if (!Loaded(Node) || Node->GetOuter() != Graph || Seen.Contains(Node) || !MapPin(Node, EGPD_Output, ActualOut) ||
            ActualOut != Out || Out->LinkedTo.Num() != 1 || Out->LinkedTo[0] != In) { return false; }
        Seen.Add(Node); if (Cast<UNiagaraNodeInput>(Node)) { return true; }
        if (!MapPin(Node, EGPD_Input, In) || !In) { return false; }
    }
    return false;
}
TSharedRef<FJsonObject> SourceSnapshot(const FSources& Sources, bool& Complete)
{
    Complete = true; auto J = MakeShared<FJsonObject>(); TArray<UNiagaraScriptSource*> Ordered = Sources.Sources;
    Ordered.Sort([](const UNiagaraScriptSource& A, const UNiagaraScriptSource& B) { return A.GetPathName() < B.GetPathName(); });
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (auto* S : Ordered)
    {
        bool Ok; auto G = GraphSnapshot(S->NodeGraph, Ok); Complete &= Ok;
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("source_path"), Path(S)); Row->SetStringField(TEXT("source_class"), Path(S->GetClass()));
        Row->SetStringField(TEXT("package_name"), S->GetOutermost()->GetName()); Row->SetBoolField(TEXT("package_dirty"), S->GetOutermost()->IsDirty());
        Row->SetObjectField(TEXT("graph"), G); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("sources"), Rows); J->SetNumberField(TEXT("source_count"), Rows.Num()); J->SetBoolField(TEXT("observation_complete"), Complete); return J;
}
TSharedRef<FJsonObject> HandleRow(const FNiagaraEmitterHandle& H, int32 Index)
{
    auto J = MakeShared<FJsonObject>(); const auto I = H.GetInstance(); const auto* D = H.GetEmitterData();
    J->SetNumberField(TEXT("index"), Index); J->SetStringField(TEXT("id"), Guid(H.GetId())); J->SetStringField(TEXT("name"), H.GetName().ToString());
    J->SetBoolField(TEXT("enabled"), H.GetIsEnabled()); J->SetStringField(TEXT("unique_instance_name"), H.GetUniqueInstanceName());
    J->SetStringField(TEXT("emitter_path"), Path(I.Emitter)); J->SetStringField(TEXT("version"), Guid(I.Version));
    J->SetStringField(TEXT("parent_emitter_path"), D ? Path(D->VersionedParent.Emitter) : FString());
    J->SetStringField(TEXT("last_merge_emitter_path"), D ? Path(D->VersionedParentAtLastMerge.Emitter) : FString()); return J;
}
TSharedRef<FJsonObject> TargetSnapshot(UNiagaraSystem* System, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); FSources Sources; Complete = Sources.AddSystem(System);
    if(!Complete){J->SetBoolField(TEXT("observation_complete"),false);return J;}
    bool SourcesOk; J->SetObjectField(TEXT("graphs"), SourceSnapshot(Sources, SourcesOk)); Complete &= SourcesOk;
    if(!Complete){J->SetBoolField(TEXT("observation_complete"),false);return J;}
    TArray<TSharedPtr<FJsonValue>> Rows; int32 I = 0; for (const auto& H : System->GetEmitterHandles()) { Rows.Add(MakeShared<FJsonValueObject>(HandleRow(H, I++))); }
    J->SetArrayField(TEXT("emitters"), Rows); J->SetNumberField(TEXT("emitter_count"), Rows.Num()); J->SetBoolField(TEXT("package_dirty"), System->GetOutermost()->IsDirty());
    J->SetStringField(TEXT("system_path"), Path(System)); J->SetBoolField(TEXT("observation_complete"), Complete); return J;
}
struct FContext
{
    UNiagaraSystem* Target = nullptr; UNiagaraSystem* SourceSystem = nullptr; UNiagaraEmitter* SourceEmitter = nullptr;
    UNiagaraSystemEditorData* EditorData = nullptr; FGuid SourceVersion; FString SourceKind, SourceObject, SourceName, DesiredName;
    int32 SourceIndex = INDEX_NONE; bool bSourceEnabled = true; FSources OriginalSources; TArray<UNiagaraScript*> PrivateClosure;
    TSharedRef<FJsonObject> SourceBefore = MakeShared<FJsonObject>(), Before = MakeShared<FJsonObject>();
    // Only Attach uses these primary-load anchors; internal inspection remains find-only.
    struct FPrimaryRole{TStrongObjectPtr<UObject> Object;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString ObjectPath,PackageName;EClassFlags ClassFlags=CLASS_None;int32 ClassBytes=0;};
    TArray<FPrimaryRole> PrimaryRoles;TStrongObjectPtr<UEditorEngine> PrimaryEditor;TStrongObjectPtr<UWorld> PrimaryWorld;
    TStrongObjectPtr<UClass> PrimaryEditorClass,PrimaryWorldClass;TStrongObjectPtr<UPackage> PrimaryWorldPackage;
    TStrongObjectPtr<UNiagaraSystemEditorData> PrimaryTargetData;bool PrimaryEnabled=false,PrimaryStopped=false,PrimaryTargetSnapshotReady=false;
    TSharedRef<FJsonObject> PrimaryTargetBefore=MakeShared<FJsonObject>();
};
TSharedRef<FJsonObject> SourceObservation(const FContext& C, bool& Complete)
{
    auto J = SourceSnapshot(C.OriginalSources, Complete); auto E = MakeShared<FJsonObject>(); const auto* D = C.SourceEmitter->GetEmitterData(C.SourceVersion);
    if (!Loaded(C.SourceEmitter) || !D) { Complete = false; }
    E->SetStringField(TEXT("emitter_path"), Path(C.SourceEmitter)); E->SetStringField(TEXT("outer_path"), Path(C.SourceEmitter->GetOuter()));
    E->SetStringField(TEXT("unique_name"), C.SourceEmitter->GetUniqueEmitterName()); E->SetStringField(TEXT("selected_version"), Guid(C.SourceVersion));
    E->SetBoolField(TEXT("package_dirty"), C.SourceEmitter->GetOutermost()->IsDirty()); E->SetBoolField(TEXT("versioning_enabled"), C.SourceEmitter->IsVersioningEnabled());
    E->SetStringField(TEXT("parent_emitter_path"), D ? Path(D->VersionedParent.Emitter) : FString());
    E->SetStringField(TEXT("last_merge_emitter_path"), D ? Path(D->VersionedParentAtLastMerge.Emitter) : FString()); J->SetObjectField(TEXT("source_emitter"), E);
    if (C.SourceSystem)
    { TArray<TSharedPtr<FJsonValue>> Rows; int32 Index = 0; for (const auto& H : C.SourceSystem->GetEmitterHandles()) { Rows.Add(MakeShared<FJsonValueObject>(HandleRow(H, Index++))); }
        J->SetArrayField(TEXT("source_system_handles"), Rows); }
    J->SetBoolField(TEXT("observation_complete"), Complete); return J;
}
// TT: primary asset resolution only. Internal/source/graph checks remain find-only.
bool EmitterPrimaryLoadActive=false;
bool PrimaryStop(FResult& R,FContext& C,const TCHAR* Code,const TCHAR* Why)
{
    C.PrimaryStopped=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;
    R.Data->SetBoolField(TEXT("primary_resolution_continuation_safe"),false);
    R.Data->SetBoolField(TEXT("outcome_unknown"),true);return Fail(R,Code,Why);
}
bool PrimaryContext(FResult& R,FContext& C)
{
    if(C.PrimaryStopped)return false;
    if(!IsEditorGameThread()||!Loaded(C.PrimaryEditor.Get())||GEditor!=C.PrimaryEditor.Get()
        ||!Loaded(C.PrimaryEditorClass.Get())||C.PrimaryEditor->GetClass()!=C.PrimaryEditorClass.Get()
        ||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()||GIsImportingT3D
        ||!Loaded(C.PrimaryWorld.Get())||!Loaded(C.PrimaryWorldClass.Get())||C.PrimaryWorld->GetClass()!=C.PrimaryWorldClass.Get()
        ||!Loaded(C.PrimaryWorldPackage.Get())||C.PrimaryWorld->GetPackage()!=C.PrimaryWorldPackage.Get()
        ||C.PrimaryEditor->GetEditorWorldContext().World()!=C.PrimaryWorld.Get()||C.PrimaryWorld->WorldType!=EWorldType::Editor||C.PrimaryWorld->bIsTearingDown)
        return PrimaryStop(R,C,TEXT("niagara_emitter_load_context_changed"),TEXT("Primary load returned outside its held idle Editor/world/GC/save boundary; no next object getter, SDK or retry."));
    return true;
}
bool PrimaryRolesCurrent(FResult& R,FContext& C)
{
    if(!PrimaryContext(R,C))return false;
    for(const auto& LoadRole:C.PrimaryRoles)
    {
        UObject* LoadObjectRole=LoadRole.Object.Get();
        if(!Loaded(LoadObjectRole)||!Loaded(LoadRole.Class.Get())||!Loaded(LoadRole.Package.Get()))
            return PrimaryStop(R,C,TEXT("niagara_emitter_load_role_unavailable"),TEXT("Previously resolved root/class/package became incomplete or retired; no stale root getter or next loader."));
        if(LoadObjectRole->GetClass()!=LoadRole.Class.Get()||LoadObjectRole->GetOuter()!=LoadRole.Package.Get()||LoadObjectRole->GetPackage()!=LoadRole.Package.Get()
            ||LoadRole.Class->GetClassFlags()!=LoadRole.ClassFlags||LoadRole.Class->GetPropertiesSize()!=LoadRole.ClassBytes
            ||LoadRole.Package->GetClass()!=UPackage::StaticClass()||LoadRole.Package->GetName()!=LoadRole.PackageName
            ||!LoadObjectRole->GetPathName().Equals(LoadRole.ObjectPath,ESearchCase::CaseSensitive)||FindObject<UObject>(nullptr,*LoadRole.ObjectPath)!=LoadObjectRole)
            return PrimaryStop(R,C,TEXT("niagara_emitter_load_role_changed"),TEXT("Previously resolved exact asset/class/package/path identity changed across another primary load; no substitution, rebaseline or retry."));
    }
    if(C.PrimaryTargetData.Get()&&(!Loaded(C.PrimaryTargetData.Get())||C.PrimaryTargetData->GetClass()!=UNiagaraSystemEditorData::StaticClass()
        ||C.PrimaryTargetData->GetOuter()!=C.Target||C.PrimaryTargetData->GetPackage()!=C.Target->GetPackage()))
        return PrimaryStop(R,C,TEXT("niagara_emitter_load_target_editor_role_changed"),TEXT("Pre-source target editor-data role became unavailable or changed owner; no stale editor-data read."));
    return true;
}
bool PrimaryTargetCurrent(FResult& R,FContext& C)
{
    if(!C.PrimaryTargetSnapshotReady)return true;
    if(!PrimaryRolesCurrent(R,C))return false;
    bool LoadTargetComplete=false;auto LoadTargetNow=TargetSnapshot(C.Target,LoadTargetComplete);
    R.Data->SetObjectField(TEXT("target_after_source_load"),LoadTargetNow);
    if(!PrimaryRolesCurrent(R,C))return false;
    if(!LoadTargetComplete||Text(C.PrimaryTargetBefore)!=Text(LoadTargetNow))
        return PrimaryStop(R,C,TEXT("niagara_emitter_source_load_target_changed"),TEXT("Source loader changed the prior target handles/graphs/dirty projection or made it unobservable; prior target baseline retained, no writer or retry."));
    return true;
}
bool BeginPrimary(FResult& R,FContext& C)
{
    if(EmitterPrimaryLoadActive)return Fail(R,TEXT("niagara_emitter_primary_load_reentry"),TEXT("Nested emitter attachment during a primary loader callback is rejected before load or writer."));
    if(!IsEditorGameThread()||!Loaded(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()||GIsImportingT3D)
        return Fail(R,TEXT("niagara_emitter_load_editor_required"),TEXT("Idle Editor game thread required before primary asset resolution."));
    C.PrimaryEditor.Reset(GEditor);C.PrimaryEditorClass.Reset(GEditor->GetClass());
    C.PrimaryWorld.Reset(GEditor->GetEditorWorldContext().World());
    if(!Loaded(C.PrimaryWorld.Get()))return Fail(R,TEXT("niagara_emitter_load_world_required"),TEXT("Current live Editor world required before primary asset resolution."));
    C.PrimaryWorldClass.Reset(C.PrimaryWorld->GetClass());C.PrimaryWorldPackage.Reset(C.PrimaryWorld->GetPackage());
    C.PrimaryEnabled=true;R.Data->SetBoolField(TEXT("already_loaded_assets_required"),false);
    R.Data->SetBoolField(TEXT("primary_resolution_continuation_safe"),false);
    R.Data->SetBoolField(TEXT("load_full_effect_state_known"),false);
    R.Data->SetStringField(TEXT("asset_resolution_policy"),TEXT("Attach primary target then selected emitter asset/source system: loaded fast path0 or one original typed StaticLoadObject per missing root. Inspector/internal rechecks remain find-only; incomplete loaded roots are not reloaded. Full PostLoad/compiler/CDO/global effects are not certified."));
    return PrimaryContext(R,C);
}
bool PrimaryAsset(FResult& R,FContext& C,const FString& Requested,UClass* ExpectedClass,const TCHAR* Which,UObject*& Resolved)
{
    Resolved=nullptr;if(!PrimaryRolesCurrent(R,C))return false;
    if(!Loaded(ExpectedClass)||!ExpectedClass->HasAnyClassFlags(CLASS_Native))
        return PrimaryStop(R,C,TEXT("niagara_emitter_load_class_unavailable"),TEXT("Exact native expected asset class must already be live before original loader dispatch."));
    TStrongObjectPtr<UClass> KeepExpectedClass(ExpectedClass);
    UObject* LoadFound=FindObject<UObject>(nullptr,*Requested);TStrongObjectPtr<UObject> KeepLoadFound(LoadFound);
    auto LoadReceipt=MakeShared<FJsonObject>();auto LoadReceipts=R.Data->GetArrayField(TEXT("primary_asset_resolutions"));
    LoadReceipts.Add(MakeShared<FJsonValueObject>(LoadReceipt));R.Data->SetArrayField(TEXT("primary_asset_resolutions"),LoadReceipts);
    LoadReceipt->SetStringField(TEXT("role"),Which);LoadReceipt->SetStringField(TEXT("requested_path"),Requested);
    LoadReceipt->SetBoolField(TEXT("already_present"),LoadFound!=nullptr);LoadReceipt->SetBoolField(TEXT("attempted"),false);LoadReceipt->SetBoolField(TEXT("returned"),false);
    if(!LoadFound)
    {
        if(EmitterPrimaryLoadActive)return Fail(R,TEXT("niagara_emitter_primary_load_reentry"),TEXT("Nested primary loader rejected without a second load or writer."));
        auto LoadCalls=R.Data->GetArrayField(TEXT("asset_load_calls"));LoadCalls.Add(MakeShared<FJsonValueObject>(LoadReceipt));R.Data->SetArrayField(TEXT("asset_load_calls"),LoadCalls);
        R.Data->SetNumberField(TEXT("asset_load_count"),R.Data->GetNumberField(TEXT("asset_load_count"))+1);R.Data->SetBoolField(TEXT("asset_load_attempted"),true);
        LoadReceipt->SetStringField(TEXT("route"),TEXT("StaticLoadObject(expected native asset class)"));LoadReceipt->SetBoolField(TEXT("attempted"),true);
        R.Stage=FString(TEXT("load_"))+Which;R.bChangeStateKnown=false;
        TGuardValue<bool> LoadActive(EmitterPrimaryLoadActive,true);
        UObject* LoadReturned=StaticLoadObject(ExpectedClass,nullptr,*Requested);TStrongObjectPtr<UObject> KeepLoadReturned(LoadReturned);
        // Copy the actual return prefix before context checks and before any returned-object getter.
        LoadReceipt->SetBoolField(TEXT("returned"),true);LoadReceipt->SetBoolField(TEXT("returned_nonnull"),LoadReturned!=nullptr);
        if(!PrimaryRolesCurrent(R,C)||!PrimaryTargetCurrent(R,C))return false;
        KeepLoadFound=MoveTemp(KeepLoadReturned);LoadFound=LoadReturned;
        if(!LoadFound){R.bChangeStateKnown=true;return Fail(R,TEXT("niagara_emitter_primary_asset_not_found"),TEXT("Original primary asset load returned null; actual load prefix retained, no writer or automatic retry."));}
    }
    if(!Loaded(LoadFound))return PrimaryStop(R,C,TEXT("niagara_emitter_primary_asset_unavailable"),TEXT("Primary asset is incomplete/retired; no getter, forced reload, writer or retry."));
    TStrongObjectPtr<UClass> LoadClass(LoadFound->GetClass());if(!Loaded(LoadClass.Get()))return PrimaryStop(R,C,TEXT("niagara_emitter_primary_class_unavailable"),TEXT("Returned class must be live before classification."));
    TStrongObjectPtr<UObject> LoadOuter(LoadFound->GetOuter());if(!Loaded(LoadOuter.Get()))return PrimaryStop(R,C,TEXT("niagara_emitter_primary_outer_unavailable"),TEXT("Returned outer must be held/live before package classification."));
    if(LoadClass.Get()!=ExpectedClass||LoadOuter->GetClass()!=UPackage::StaticClass())
    {R.bChangeStateKnown=true;return Fail(R,TEXT("niagara_emitter_primary_native_asset_required"),TEXT("Exact native top-level system/emitter asset required; original subclass/nested/redirected variants remain incomplete."));}
    TStrongObjectPtr<UPackage> LoadPackage(static_cast<UPackage*>(LoadOuter.Get()));
    const FString RequestedPackage=FPackageName::ObjectPathToPackageName(Requested);
    if(LoadFound->GetPackage()!=LoadPackage.Get()||LoadPackage->GetName()!=RequestedPackage||!LoadFound->GetPathName().Equals(Requested,ESearchCase::CaseSensitive)||FindObject<UObject>(nullptr,*Requested)!=LoadFound)
        return PrimaryStop(R,C,TEXT("niagara_emitter_primary_exact_path_required"),TEXT("Load result must remain the exact requested native top-level object/package, including redirect checks; no substitute or retry."));
    FContext::FPrimaryRole LoadRole;LoadRole.Object.Reset(LoadFound);LoadRole.Class.Reset(LoadClass.Get());LoadRole.Package.Reset(LoadPackage.Get());
    LoadRole.ClassFlags=LoadClass->GetClassFlags();LoadRole.ClassBytes=LoadClass->GetPropertiesSize();LoadRole.ObjectPath=Requested;LoadRole.PackageName=RequestedPackage;
    LoadReceipt->SetStringField(TEXT("resolved_path"),Requested);LoadReceipt->SetStringField(TEXT("resolved_class"),LoadClass->GetPathName());
    C.PrimaryRoles.Add(MoveTemp(LoadRole));if(!PrimaryRolesCurrent(R,C))return false;
    R.bChangeStateKnown=true;Resolved=LoadFound;return true;
}

bool Prepare(FResult& R, FContext& C, const FString& TargetPath, const FString& EmitterPath, const FString& SourcePath,
    const FString& IndexJson, const FString& SourceName, const FString& NewName, bool bWrite, bool bAllowSourceEdit)
{
    const auto& V = FEngineVersion::Current();
    if (!IsEditorGameThread() || IsPlaySessionActive() || V.GetMajor() != 5 || V.GetMinor() != 8)
    { return Fail(R, TEXT("niagara_emitter_editor_state_invalid"), TEXT("Requires UE 5.8.x editor game thread outside PIE/SIE.")); }
    int32 RequestedIndex; FString TargetObject;
    if (!Index(IndexJson, RequestedIndex) || !Bounded(SourceName, 256) || !Bounded(NewName, 256) || !ObjectPath(TargetPath, TargetObject))
    { return Fail(R, TEXT("niagara_emitter_selector_invalid"), TEXT("Use precise asset paths, bounded names and a null/-1/nonnegative int32 source index.")); }
    if(bWrite)
    {
        // Preserve original obvious refusal before a new load, and retain the actual-owner gate below.
        const FString RequestedTargetPackage=FPackageName::ObjectPathToPackageName(TargetObject);
        const bool RequestedTemp=RequestedTargetPackage.StartsWith(TEXT("/Game/CubelessMCPTests/"))||RequestedTargetPackage.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/"));
        if(!RequestedTargetPackage.StartsWith(TEXT("/Game/"))||(!RequestedTemp&&!bAllowSourceEdit))return Fail(R,TEXT("niagara_emitter_write_scope_forbidden"),TEXT("Writes require generated/test roots or explicit /Game source-edit opt-in."));
        if(EmitterPath.IsEmpty()&&SourcePath.IsEmpty())return Fail(R,TEXT("niagara_emitter_source_path_invalid"),TEXT("Provide source_emitter_path or an exact source system asset path."));
        if(!BeginPrimary(R,C))return false;UObject* ResolvedTarget=nullptr;
        if(!PrimaryAsset(R,C,TargetObject,UNiagaraSystem::StaticClass(),TEXT("target_system"),ResolvedTarget))return false;
        C.Target=static_cast<UNiagaraSystem*>(ResolvedTarget);
    }
    else C.Target = FindObject<UNiagaraSystem>(nullptr, *TargetObject);
    if (!Loaded(C.Target) || C.Target->GetClass() != UNiagaraSystem::StaticClass() || !C.Target->IsAsset() ||
        C.Target->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn) || C.Target->GetEmitterHandles().Num() >= MaxEmitters)
    { return Fail(R, TEXT("niagara_loaded_emitter_target_required"), TEXT("An ordinary already-loaded native system with fewer than 128 handles is required.")); }
    const FString Package = C.Target->GetOutermost()->GetName(); const bool Temp = Package.StartsWith(TEXT("/Game/CubelessMCPTests/")) || Package.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/"));
    if (bWrite && (!Package.StartsWith(TEXT("/Game/")) || (!Temp && !bAllowSourceEdit)))
    { return Fail(R, TEXT("niagara_emitter_write_scope_forbidden"), TEXT("Writes require generated/test roots or explicit /Game source-edit opt-in.")); }
    if(bWrite)
    {
        if(!PrimaryRolesCurrent(R,C))return false;
        UObject* InitialEditorData=C.Target->GetEditorData();TStrongObjectPtr<UObject> KeepInitialEditorData(InitialEditorData);
        if(!PrimaryRolesCurrent(R,C))return false;
        if(!Loaded(InitialEditorData)||InitialEditorData->GetClass()!=UNiagaraSystemEditorData::StaticClass())return PrimaryStop(R,C,TEXT("niagara_emitter_load_target_editor_data_unavailable"),TEXT("Loaded target must have existing live exact native editor data; no creation/repair."));
        C.EditorData=static_cast<UNiagaraSystemEditorData*>(InitialEditorData);C.PrimaryTargetData.Reset(C.EditorData);
    }
    else C.EditorData = Cast<UNiagaraSystemEditorData>(C.Target->GetEditorData());
    if (!Loaded(C.EditorData) || C.EditorData->GetClass() != UNiagaraSystemEditorData::StaticClass() || C.EditorData->GetOwningSystemIsPlaceholder())
    { return Fail(R, TEXT("niagara_emitter_placeholder_unsupported"), TEXT("Target must have already-created native editor data and be a real system, not an emitter placeholder.")); }
    if(bWrite)
    {
        if(!PrimaryRolesCurrent(R,C))return false;bool BeforeSourceComplete=false;
        C.PrimaryTargetBefore=TargetSnapshot(C.Target,BeforeSourceComplete);
        R.Data->SetObjectField(TEXT("target_before_source_load"),C.PrimaryTargetBefore);
        if(!PrimaryRolesCurrent(R,C))return false;
        if(!BeforeSourceComplete)return PrimaryStop(R,C,TEXT("niagara_emitter_before_source_target_unobservable"),TEXT("Prior target handles/graphs/dirty projection must be observed before source loading; no source load or writer."));
        C.PrimaryTargetSnapshotReady=true;
    }
    if (!EmitterPath.IsEmpty())
    {
        FString Object; if (!ObjectPath(EmitterPath, Object)) { return Fail(R, TEXT("niagara_emitter_source_path_invalid"), TEXT("Invalid emitter asset path.")); }
        if(bWrite)
        {
            UObject* ResolvedEmitter=nullptr;if(!PrimaryAsset(R,C,Object,UNiagaraEmitter::StaticClass(),TEXT("source_emitter"),ResolvedEmitter))return false;
            C.SourceEmitter=static_cast<UNiagaraEmitter*>(ResolvedEmitter);
        }
        else C.SourceEmitter = FindObject<UNiagaraEmitter>(nullptr, *Object);
        if (!Loaded(C.SourceEmitter) || C.SourceEmitter->GetClass() != UNiagaraEmitter::StaticClass() || !C.SourceEmitter->IsAsset())
        { return Fail(R, TEXT("niagara_loaded_source_emitter_required"), TEXT("Load the exact native emitter asset before this find-only operation.")); }
        C.SourceVersion = C.SourceEmitter->IsVersioningEnabled() && C.SourceEmitter->VersionToOpenInEditor.IsValid() ?
            C.SourceEmitter->VersionToOpenInEditor : C.SourceEmitter->GetExposedVersion().VersionGuid;
        C.SourceKind = TEXT("emitter_asset"); C.SourceObject = Path(C.SourceEmitter); C.SourceName = C.SourceEmitter->GetUniqueEmitterName();
    }
    else
    {
        FString Object; if (!ObjectPath(SourcePath, Object)) { return Fail(R, TEXT("niagara_emitter_source_path_invalid"), TEXT("Provide source_emitter_path or an exact source system asset path.")); }
        if(bWrite)
        {
            UObject* ResolvedSystem=nullptr;if(!PrimaryAsset(R,C,Object,UNiagaraSystem::StaticClass(),TEXT("source_system"),ResolvedSystem))return false;
            C.SourceSystem=static_cast<UNiagaraSystem*>(ResolvedSystem);
        }
        else C.SourceSystem = FindObject<UNiagaraSystem>(nullptr, *Object);
        if (!Loaded(C.SourceSystem) || C.SourceSystem->GetClass() != UNiagaraSystem::StaticClass() || !C.SourceSystem->IsAsset() ||
            C.SourceSystem == C.Target || C.SourceSystem->GetEmitterHandles().Num() > MaxEmitters || C.SourceSystem->HasOutstandingCompilationRequests(true))
        { return Fail(R, TEXT("niagara_loaded_source_system_required"), TEXT("An already-loaded distinct native source system with no outstanding compilation is required.")); }
        for (int32 I = 0; I < C.SourceSystem->GetEmitterHandles().Num(); ++I)
        {
            const auto& H = C.SourceSystem->GetEmitterHandles()[I]; const FString Name = H.GetName().ToString();
            if ((RequestedIndex == INDEX_NONE || RequestedIndex == I) && (SourceName.IsEmpty() || SourceName.Equals(Name, ESearchCase::IgnoreCase)))
            {
                if (H.GetEmitterMode() != ENiagaraEmitterMode::Standard) { return Fail(R, TEXT("niagara_stateless_emitter_unsupported"), TEXT("This legacy path requires a standard scripted emitter.")); }
                const auto Instance = H.GetInstance(); C.SourceEmitter = Instance.Emitter; C.SourceVersion = Instance.Version;
                C.SourceIndex = I; C.SourceName = Name; C.bSourceEnabled = H.GetIsEnabled(); break;
            }
        }
        C.SourceKind = TEXT("system_emitter_handle"); C.SourceObject = Path(C.SourceSystem);
    }
    if (!Loaded(C.SourceEmitter) || C.SourceEmitter->GetClass() != UNiagaraEmitter::StaticClass() || !C.SourceEmitter->GetEmitterData(C.SourceVersion) ||
        C.SourceEmitter->GetOutermost() == C.Target->GetOutermost() || C.SourceEmitter->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor | PKG_CompiledIn))
    { return Fail(R, TEXT("niagara_emitter_source_version_invalid"), TEXT("The first matching loaded native source emitter/version must be valid and outside the target package.")); }
    if(bWrite)
    {
        if(!PrimaryRolesCurrent(R,C))return false;
        UObject* FreshTargetData=C.Target->GetEditorData();TStrongObjectPtr<UObject> KeepFreshTargetData(FreshTargetData);
        if(!PrimaryRolesCurrent(R,C))return false;
        if(FreshTargetData!=C.PrimaryTargetData.Get())return PrimaryStop(R,C,TEXT("niagara_emitter_load_target_editor_data_changed"),TEXT("Existing target editor-data slot changed across source load; no stale editor-data getter or writer."));
    }
    C.DesiredName = NewName.IsEmpty() ? C.SourceName : NewName;
    if (bWrite)
    {
        if (C.Target->HasOutstandingCompilationRequests(true)) { return Fail(R, TEXT("niagara_emitter_compile_outstanding"), TEXT("Inspect readiness before editing; no wait is performed here.")); }
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        { if (Loaded(*It) && (*It)->GetAsset() == C.Target && (*It)->GetWorld())
            { return Fail(R, TEXT("niagara_emitter_target_has_world_instances"), TEXT("Native attachment kills referencing system instances; this bounded authoring leaf refuses live world references.")); } }
    }
    if (!C.OriginalSources.AddEmitter(C.SourceEmitter, C.SourceVersion) || !ExpandPrivateClosure(C.Target, C.OriginalSources, C.PrivateClosure))
    { return Fail(R, TEXT("niagara_emitter_source_closure_unsupported"), TEXT("Already-loaded native source/private-script graphs must satisfy bounded version/node/pin/link observations before mutation.")); }
    FSources TargetSources; TArray<UNiagaraScript*> TargetPrivate;
    if (!TargetSources.AddSystem(C.Target) || !ExpandPrivateClosure(C.Target, TargetSources, TargetPrivate))
    { return Fail(R, TEXT("niagara_emitter_target_graph_unsupported"), TEXT("Existing target scripted emitters/graphs/private calls must be fully loaded and bounded.")); }
    for (auto* Private : TargetPrivate) { if (!C.OriginalSources.AddScript(Private)) { return Fail(R, TEXT("niagara_emitter_private_closure_bound"), TEXT("External target private script observation exceeded its bound.")); } }
    for (auto* Private : TargetPrivate) { C.PrivateClosure.AddUnique(Private); }
    if (C.PrivateClosure.Num() > MaxPrivateScripts)
    { return Fail(R, TEXT("niagara_emitter_private_closure_bound"), TEXT("Combined source/target external private script closure exceeds 128 before mutation.")); }
    auto* SystemSource = Cast<UNiagaraScriptSource>(C.Target->GetSystemSpawnScript()->GetLatestSource());
    if (!SystemSource || C.Target->GetSystemUpdateScript()->GetLatestSource() != SystemSource)
    { return Fail(R, TEXT("niagara_emitter_system_graph_mismatch"), TEXT("Native rebuild requires the existing shared spawn/update source graph.")); }
    UClass* Schema = FindObject<UClass>(nullptr, TEXT("/Script/NiagaraEditor.EdGraphSchema_Niagara"));
    if (!Schema || Schema != UEdGraphSchema_Niagara::StaticClass() || !IsValid(Schema->GetDefaultObject(false)) || SystemSource->NodeGraph->Schema.Get() != Schema)
    { return Fail(R, TEXT("niagara_emitter_schema_not_loaded"), TEXT("Exact native Niagara schema/default must already be loaded; no default object creation.")); }
    int32 Spawn = 0, Update = 0, MapInputs = 0; bool StacksSafe = true;
    for (UEdGraphNode* N : SystemSource->NodeGraph->Nodes)
    {
        if (auto* O = Cast<UNiagaraNodeOutput>(N))
        { Spawn += O->GetUsage() == ENiagaraScriptUsage::SystemSpawnScript; Update += O->GetUsage() == ENiagaraScriptUsage::SystemUpdateScript;
            if (O->GetUsage() == ENiagaraScriptUsage::SystemSpawnScript || O->GetUsage() == ENiagaraScriptUsage::SystemUpdateScript)
            { StacksSafe &= SimpleSystemMapStack(SystemSource->NodeGraph, O); } }
        if (const auto* In = Cast<UNiagaraNodeInput>(N)) { MapInputs += In->Input.GetType() == FNiagaraTypeDefinition::GetParameterMapDef(); }
    }
    if (Spawn != 1 || Update != 1 || MapInputs < 2 || !StacksSafe)
    { return Fail(R, TEXT("niagara_emitter_system_outputs_required"), TEXT("Existing spawn/update outputs and separate simple reciprocal parameter-map stacks are required; no implicit output creation/branched-stack repair.")); }
    bool SourceOk, TargetOk; C.SourceBefore = SourceObservation(C, SourceOk); C.Before = TargetSnapshot(C.Target, TargetOk);
    if (!SourceOk || !TargetOk) { return Fail(R, TEXT("niagara_emitter_preflight_unobservable"), TEXT("Pre-edit source/target inventory is incomplete.")); }
    if(bWrite)
    {
        if(!PrimaryRolesCurrent(R,C))return false;
        if(Text(C.PrimaryTargetBefore)!=Text(C.Before))return PrimaryStop(R,C,TEXT("niagara_emitter_preflight_target_changed"),TEXT("Target pre-source baseline changed during source/preflight observations; no writer or rebaseline."));
        R.Data->SetBoolField(TEXT("target_preserved_across_source_resolution"),true);
    }
    R.Data->SetStringField(TEXT("system_path"), Path(C.Target)); R.Data->SetStringField(TEXT("target_system_path"), Path(C.Target));
    R.Data->SetStringField(TEXT("source_kind"), C.SourceKind); R.Data->SetStringField(TEXT("source_path"), C.SourceObject);
    R.Data->SetStringField(TEXT("source_emitter_path"), Path(C.SourceEmitter)); R.Data->SetStringField(TEXT("source_emitter_version"), Guid(C.SourceVersion));
    R.Data->SetStringField(TEXT("source_emitter_name"), C.SourceName); R.Data->SetNumberField(TEXT("source_emitter_index"), C.SourceIndex);
    R.Data->SetBoolField(TEXT("source_handle_enabled"), C.bSourceEnabled); R.Data->SetStringField(TEXT("requested_new_emitter_name"), NewName);
    R.Data->SetStringField(TEXT("desired_new_emitter_name"), C.DesiredName); R.Data->SetStringField(TEXT("source_emitter_index_json"), IndexJson);
    R.Data->SetObjectField(TEXT("before"), C.Before); R.Data->SetObjectField(TEXT("source_before"), C.SourceBefore);
    R.Data->SetNumberField(TEXT("emitter_count_before"), C.Target->GetEmitterHandles().Num()); R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.Target));
    if(bWrite&&!PrimaryRolesCurrent(R,C))return false;R.Data->SetBoolField(TEXT("primary_resolution_continuation_safe"),bWrite);R.Stage=TEXT("preflight");R.Data->SetBoolField(TEXT("native_preflight_verified"), true); R.Data->SetStringField(TEXT("write_scope"), Temp ? TEXT("target_temp_system_emitter_attach") : TEXT("source_system_emitter_attach")); return true;
}
// Dispatch the public exported base virtual. No private NiagaraGraph header or
// non-exported derived symbol is called; the native override still receives it.
FORCENOINLINE void NotifyGraph(UEdGraph& Graph) { Graph.NotifyGraphChanged(); }
bool Localize(FResult& R, FContext& C)
{
    TMap<UNiagaraScript*, UNiagaraScript*> Copies; int32 Replaced = 0; TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Iteration = 0; Iteration < MaxPrivateScripts; ++Iteration)
    {
        FSources Current; TArray<UNiagaraScript*> Private;
        if (!Current.AddSystem(C.Target) || !ExpandPrivateClosure(C.Target, Current, Private))
        { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_emitter_localization_unobservable"), TEXT("Attachment/localization dispatched but actual private closure is incomplete; stop without retry.")); }
        if (Private.IsEmpty())
        {
            R.Data->SetNumberField(TEXT("localized_private_scratch_pad_count"), Copies.Num()); R.Data->SetNumberField(TEXT("localized_private_scratch_pad_reference_count"), Replaced);
            R.Data->SetArrayField(TEXT("localized_private_scratch_pad_scripts"), Rows); R.Data->SetBoolField(TEXT("external_private_function_scripts_remaining"), false); return true;
        }
        for (UNiagaraScript* Source : Private)
        {
            UNiagaraScript* Local = Copies.FindRef(Source);
            if (!Local)
            {
                if (Copies.Num() >= MaxPrivateScripts) { return Fail(R, TEXT("niagara_emitter_localization_bound"), TEXT("At most 128 external private scripts may be duplicated; partial work retained.")); }
                R.Stage = TEXT("localize_private_script"); const FName Name = MakeUniqueObjectName(C.Target, UNiagaraScript::StaticClass(), FName(*(TEXT("Local_") + Source->GetName())));
                Local = Cast<UNiagaraScript>(StaticDuplicateObject(Source, C.Target, Name, RF_Transactional));
                if (!Loaded(Local) || Local->GetClass() != UNiagaraScript::StaticClass() || Local->GetOuter() != C.Target)
                { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_emitter_private_duplicate_failed"), TEXT("Native duplicate did not return the expected owned script; any unobserved native partial creation is uncertain and never retried.")); }
                Local->ClearFlags(RF_Public | RF_Standalone); Local->SetFlags(RF_Transactional); C.Target->ScratchPadScripts.Add(Local); Copies.Add(Source, Local);
                auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("source_script_path"), Path(Source)); Row->SetStringField(TEXT("localized_script_path"), Path(Local));
                Row->SetStringField(TEXT("localized_script_name"), Local->GetName()); Row->SetNumberField(TEXT("usage"), static_cast<int32>(Local->GetUsage())); Rows.Add(MakeShared<FJsonValueObject>(Row));
                R.Data->SetNumberField(TEXT("localized_private_scratch_pad_count"), Copies.Num()); R.Data->SetArrayField(TEXT("localized_private_scratch_pad_scripts"), Rows);
            }
            FSources Owned; if (!Owned.AddSystem(C.Target)) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_emitter_owned_graphs_unobservable"), TEXT("Owned graph inventory became incomplete after duplication.")); }
            for (UNiagaraScriptSource* S : Owned.Sources)
            {
                bool Changed = false;
                for (UEdGraphNode* N : S->NodeGraph->Nodes)
                { if (auto* Call = Cast<UNiagaraNodeFunctionCall>(N); Call && Call->FunctionScript == Source)
                    { Call->Modify(); Call->FunctionScript = Local; Call->FunctionScriptAssetObjectPath = NAME_None; ++Replaced;
                        R.Data->SetNumberField(TEXT("localized_private_scratch_pad_reference_count"), Replaced); Changed = true; } }
                if (Changed) { S->Modify(); S->NodeGraph->Modify(); NotifyGraph(*S->NodeGraph); }
            }
        }
    }
    R.Data->SetNumberField(TEXT("localized_private_scratch_pad_count"), Copies.Num()); R.Data->SetNumberField(TEXT("localized_private_scratch_pad_reference_count"), Replaced);
    R.Data->SetArrayField(TEXT("localized_private_scratch_pad_scripts"), Rows);
    return Fail(R, TEXT("niagara_emitter_localization_iteration_limit"), TEXT("Private scratch localization reached the original 128-iteration bound; no rollback/automatic retry."));
}
bool Observe(FResult& R, FContext& C)
{
    bool SourceOk, TargetOk; auto SourceAfter = SourceObservation(C, SourceOk); auto After = TargetSnapshot(C.Target, TargetOk);
    R.Data->SetObjectField(TEXT("source_after"), SourceAfter); R.Data->SetObjectField(TEXT("after"), After);
    R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.Target)); R.Data->SetNumberField(TEXT("emitter_count_after"), C.Target->GetEmitterHandles().Num());
    if (!SourceOk || !TargetOk) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_emitter_after_unobservable"), TEXT("Actual source/target observation is incomplete; stop without retry.")); }
    const bool Preserved = Text(C.SourceBefore) == Text(SourceAfter); R.Data->SetBoolField(TEXT("source_preserved"), Preserved);
    R.bChanged |= Text(C.Before) != Text(After);
    if (!Preserved) { R.bChanged = true; return Fail(R, TEXT("niagara_emitter_source_changed"), TEXT("Source graph/package changed; any target changes remain and compile/save is suppressed.")); }
    return true;
}
bool VerifyAttached(FResult& R, FContext& C, const FGuid& Id, bool HasEnabled, bool Enabled)
{
    const auto& Current = C.Target->GetEmitterHandles(); const auto& Prior = C.Before->GetArrayField(TEXT("emitters"));
    int32 Matches = 0; const FNiagaraEmitterHandle* New = nullptr; bool PriorPreserved = Current.Num() == Prior.Num() + 1;
    for (int32 I = 0; I < Current.Num(); ++I)
    { if (Current[I].GetId() == Id) { ++Matches; New = &Current[I]; }
        if (I < Prior.Num() && Text(HandleRow(Current[I], I)) != Text(Prior[I]->AsObject().ToSharedRef())) { PriorPreserved = false; } }
    const auto* D = New ? New->GetEmitterData() : nullptr; const auto Instance = New ? New->GetInstance() : FVersionedNiagaraEmitter();
    const bool Identity = Id.IsValid() && Matches == 1 && New && D && Loaded(Instance.Emitter) && Instance.Emitter->GetOuter() == C.Target &&
        Instance.Emitter->GetClass() == UNiagaraEmitter::StaticClass() && Instance.Emitter != C.SourceEmitter;
    const bool Detached = D && !D->VersionedParent.Emitter && !D->VersionedParentAtLastMerge.Emitter;
    const bool State = New && New->GetIsEnabled() == (HasEnabled ? Enabled : true);
    R.Data->SetBoolField(TEXT("prior_handle_identity_state_preserved"), PriorPreserved); R.Data->SetBoolField(TEXT("new_handle_identity_verified"), Identity);
    R.Data->SetBoolField(TEXT("parent_detachment_verified"), Detached); R.Data->SetBoolField(TEXT("enabled_presence_state_verified"), State);
    FSources Owned; TArray<UNiagaraScript*> Private;
    const bool Observable = Owned.AddSystem(C.Target) && ExpandPrivateClosure(C.Target, Owned, Private);
    R.Data->SetBoolField(TEXT("private_script_closure_observable"), Observable); R.Data->SetNumberField(TEXT("external_private_function_script_count_after"), Private.Num());
    if (!Observable) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_emitter_final_closure_unobservable"), TEXT("The resulting private-script closure is unobservable; stop without retry.")); }
    if (!Identity || !Detached || !State || !PriorPreserved || !Private.IsEmpty())
    { return Fail(R, TEXT("niagara_emitter_final_identity_failed"), TEXT("Current attached/parent/enabled/private-closure or prior handle state differs; all changes retained.")); }
    return true;
}
// TW: concrete, synchronous User-store copy. No offset or DI array storage survives an SDK call.
FORCENOINLINE bool CopyAdd(FNiagaraParameterStore& Store,const FNiagaraVariable& V)
{ return Store.AddParameter(V,true,true); }
FORCENOINLINE const FVector* CopyPosition(const FNiagaraParameterStore& Store,FName Name)
{ return Store.GetPositionParameterValue(Name); }
FORCENOINLINE bool CopySetPosition(FNiagaraParameterStore& Store,const FVector& V,FName Name)
{ return Store.SetPositionParameterValue(V,Name,true); }
FORCENOINLINE void CopySanity(FNiagaraParameterStore& Store)
{ Store.SanityCheckData(false); }
struct FUserCopy
{
    FResult& R;FContext& C;bool Stopped=false,Started=false;
    struct FHeld {TStrongObjectPtr<UObject> Object,Outer;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString Name;EClassFlags Flags=CLASS_None;int32 ClassBytes=0,StructBytes=INDEX_NONE;};
    struct FCell {FNiagaraVariable Variable;TArray<uint8> Bytes;FVector Position=FVector::ZeroVector;UObject* Object=nullptr;FString LegacyType,ReportName;TSharedRef<FJsonObject> Row=MakeShared<FJsonObject>();};
    struct FSourceGraph {TStrongObjectPtr<UNiagaraScriptSource> Source;TStrongObjectPtr<UNiagaraGraph> Graph;};
    struct FEmitterRole {UNiagaraSystem* System=nullptr;int32 Index=0;FGuid Id,Version;TStrongObjectPtr<UNiagaraEmitter> Emitter,Parent,LastParent;TStrongObjectPtr<UNiagaraScriptSourceBase> Graph;FString Row;};
    TArray<FEmitterRole> Emitters;int32 SourceHandleCount=0,TargetHandleCount=0;bool AttachmentReturned=false;
    TStrongObjectPtr<UNiagaraScriptSourceBase> SelectedGraph;TStrongObjectPtr<UNiagaraEmitter> SelectedParent,SelectedLastParent;bool SelectedCaptured=false;
    TStrongObjectPtr<UNiagaraEmitter> Added;TStrongObjectPtr<UNiagaraScriptSourceBase> AddedGraph;FGuid AddedId,AddedVersion;
    TArray<FSourceGraph> SourceGraphs;TArray<FHeld> Held;TArray<FCell> SourceUsers;TMap<FString,FCell> SourceBefore,Expected;
    TArray<TSharedPtr<FJsonValue>> Calls,Copied,Skipped;TSharedPtr<FJsonObject> Call;
    explicit FUserCopy(FResult& In,FContext& Context):R(In),C(Context){}
    bool Stop(const TCHAR* Property,const FString& Want,const FString& Got,const FString& ObjectPath=FString())
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Want);J->SetStringField(TEXT("actual"),Got);if(!ObjectPath.IsEmpty())J->SetStringField(TEXT("object"),ObjectPath);R.Data->SetObjectField(TEXT("user_copy_first_failure"),J);}
        Stopped=true;
        if(!Started&&!R.bMutationAttempted)return Fail(R,TEXT("niagara_emitter_user_copy_profile_pending"),TEXT("This valid wider User-copy declaration/value profile is incomplete; no attach/copy dispatched. Actual primary-loader prefix remains separate."));
        return PrimaryStop(R,C,TEXT("niagara_emitter_user_copy_current_failed"),TEXT("User-copy callback/current failed; completed attach/copy prefix retained, no next getter, SDK, save, rollback or replay."));
    }
    // Fixed roles never relax at SetName. Only captured owned relative paths have a one-return plan.
    static FString IdentityOperand(UObject* O)
    {return Loaded(O)?O->GetPathName():FString::Printf(TEXT("unavailable:%p"),static_cast<void*>(O));}
    bool HeldFixed(const FHeld& H)
    {
        if(!Loaded(H.Object.Get()))return Stop(TEXT("held_object_live"),TEXT("live held object"),TEXT("unavailable"),H.Name);
        if(!Loaded(H.Class.Get()))return Stop(TEXT("held_class_live"),TEXT("live held class"),TEXT("unavailable"),H.Name);
        if(!Loaded(H.Outer.Get()))return Stop(TEXT("held_outer_live"),TEXT("live held outer"),TEXT("unavailable"),H.Name);
        if(!Loaded(H.Package.Get()))return Stop(TEXT("held_package_live"),TEXT("live held package"),TEXT("unavailable"),H.Name);
        if(H.Object->GetClass()!=H.Class.Get())return Stop(TEXT("held_class"),H.Class->GetPathName(),IdentityOperand(H.Object->GetClass()),H.Name);
        if(H.Object->GetOuter()!=H.Outer.Get())return Stop(TEXT("held_outer"),H.Outer->GetPathName(),IdentityOperand(H.Object->GetOuter()),H.Name);
        if(H.Object->GetPackage()!=H.Package.Get())return Stop(TEXT("held_package"),H.Package->GetPathName(),IdentityOperand(H.Object->GetPackage()),H.Name);
        if(H.Class->GetClassFlags()!=H.Flags)return Stop(TEXT("held_class_flags"),LexToString(static_cast<uint32>(H.Flags)),LexToString(static_cast<uint32>(H.Class->GetClassFlags())),H.Name);
        if(H.Class->GetPropertiesSize()!=H.ClassBytes)return Stop(TEXT("held_class_bytes"),LexToString(H.ClassBytes),LexToString(H.Class->GetPropertiesSize()),H.Name);
        if(H.StructBytes!=INDEX_NONE&&static_cast<UStruct*>(H.Object.Get())->GetPropertiesSize()!=H.StructBytes)return Stop(TEXT("held_struct_bytes"),LexToString(H.StructBytes),LexToString(static_cast<UStruct*>(H.Object.Get())->GetPropertiesSize()),H.Name);
        return true;
    }
    bool Base(const TMap<int32,FString>* SetNamePaths=nullptr)
    {
        if(Stopped||!PrimaryRolesCurrent(R,C)){Stopped=true;return false;}
        for(int32 HeldIndex=0;HeldIndex<Held.Num();++HeldIndex)
        {
            const auto& H=Held[HeldIndex];if(!HeldFixed(H))return false;
            const FString* PlannedPath=SetNamePaths?SetNamePaths->Find(HeldIndex):nullptr;
            const FString& ExpectedPath=PlannedPath?*PlannedPath:H.Name;
            const FString ActualPath=H.Object->GetPathName();
            if(!ActualPath.Equals(ExpectedPath,ESearchCase::CaseSensitive))return Stop(TEXT("held_path"),ExpectedPath,ActualPath,H.Name);
        }
        for(const auto& G:SourceGraphs)if(!G.Source->NodeGraph.IsResolved()||G.Source->NodeGraph.Get()!=G.Graph.Get())return Stop(TEXT("source_graph_slot"),TEXT("resolved captured source graph"),TEXT("different/unresolved"));
        if(SelectedCaptured){const auto* D=C.SourceEmitter->GetEmitterData(C.SourceVersion);if(!D||!D->GraphSource.IsResolved()||D->GraphSource.Get()!=SelectedGraph.Get()||!D->VersionedParent.Emitter.IsResolved()||!D->VersionedParentAtLastMerge.Emitter.IsResolved()||D->VersionedParent.Emitter.Get()!=SelectedParent.Get()||D->VersionedParentAtLastMerge.Emitter.Get()!=SelectedLastParent.Get())return Stop(TEXT("selected_source_version"),TEXT("same selected emitter/version/graph/nullable parents"),TEXT("different/unresolved"));}
        if(C.SourceSystem&&C.SourceSystem->GetEmitterHandles().Num()!=SourceHandleCount)return Stop(TEXT("source_handle_count"),LexToString(SourceHandleCount),LexToString(C.SourceSystem->GetEmitterHandles().Num()));
        if(C.Target->GetEmitterHandles().Num()!=TargetHandleCount+(AttachmentReturned?1:0))return Stop(TEXT("target_handle_count"),LexToString(TargetHandleCount+(AttachmentReturned?1:0)),LexToString(C.Target->GetEmitterHandles().Num()));
        for(const auto& B:Emitters)
        {
            const auto& Rows=B.System->GetEmitterHandles();if(!Rows.IsValidIndex(B.Index)||Rows[B.Index].GetId()!=B.Id||Rows[B.Index].GetEmitterMode()!=ENiagaraEmitterMode::Standard)return Stop(TEXT("existing_handle"),Guid(B.Id),TEXT("missing/reordered/different mode"));
            const auto I=Rows[B.Index].GetInstance();if(!I.Emitter.IsResolved()||I.Emitter.Get()!=B.Emitter.Get()||I.Version!=B.Version)return Stop(TEXT("existing_instance"),Path(B.Emitter.Get()),TEXT("different/unresolved emitter/version"));
            const auto* D=B.Emitter->GetEmitterData(B.Version);if(!D||!D->GraphSource.IsResolved()||D->GraphSource.Get()!=B.Graph.Get()||!D->VersionedParent.Emitter.IsResolved()||!D->VersionedParentAtLastMerge.Emitter.IsResolved()||D->VersionedParent.Emitter.Get()!=B.Parent.Get()||D->VersionedParentAtLastMerge.Emitter.Get()!=B.LastParent.Get())return Stop(TEXT("existing_version_roles"),TEXT("same graph and nullable parent roles"),TEXT("different/unresolved"));
            if(Text(HandleRow(Rows[B.Index],B.Index))!=B.Row)return Stop(TEXT("existing_handle_state"),B.Row,Text(HandleRow(Rows[B.Index],B.Index)));
        }
        if(Added.Get())
        {
            if(!Loaded(Added.Get())||Added->GetClass()!=UNiagaraEmitter::StaticClass()||Added->GetOuter()!=C.Target)return Stop(TEXT("new_emitter_lifetime"),TEXT("held live owned new emitter"),TEXT("retired/replaced"));
            int32 Matches=0;for(const auto& H:C.Target->GetEmitterHandles())if(H.GetId()==AddedId){const auto I=H.GetInstance();if(!I.Emitter.IsResolved()||I.Emitter.Get()!=Added.Get()||I.Version!=AddedVersion)return Stop(TEXT("new_emitter_binding"),TEXT("same ID/instance/version"),TEXT("different/unresolved"));++Matches;}if(Matches!=1)return Stop(TEXT("new_emitter_membership"),TEXT("one returned handle"),LexToString(Matches));
            const auto* D=Added->GetEmitterData(AddedVersion);if(!D||!D->GraphSource.IsResolved()||D->GraphSource.Get()!=AddedGraph.Get()||!Loaded(AddedGraph.Get()))return Stop(TEXT("new_emitter_version_graph"),TEXT("existing version and same resolved held GraphSource"),TEXT("removed/replaced/unresolved"));
        }
        return true;
    }
    struct FSetNamePlan
    {
        bool Ready=false,Rename=false;int32 HeldCount=0;FName OldObjectName,HandleName,AllocatorBase;
        FString OldPath,OldUnique,Unique,ParentPath;TMap<int32,FString> OwnedSuffixes;
    } SetNamePlan;
    bool PrepareSetName()
    {
        if(!Current()||!Hold(UNiagaraEmitterBase::StaticClass()))return false;
        if(SetNamePlan.Ready||!Added.Get())return Stop(TEXT("set_name_plan"),TEXT("one held added emitter boundary"),TEXT("missing/repeated"));
        const FNiagaraEmitterHandle* SelectedHandle=nullptr;
        TSet<FName> OtherNames;
        for(const auto& CandidateHandle:C.Target->GetEmitterHandles())
        {if(CandidateHandle.GetId()==AddedId)SelectedHandle=&CandidateHandle;else OtherNames.Add(CandidateHandle.GetName());}
        if(!SelectedHandle)return Stop(TEXT("set_name_handle"),Guid(AddedId),TEXT("missing"));
        FSetNamePlan Plan;Plan.HeldCount=Held.Num();Plan.OldPath=Added->GetPathName();Plan.OldObjectName=Added->GetFName();Plan.OldUnique=Added->GetUniqueEmitterName();
        const FName Sanitized(*FNiagaraUtilities::SanitizeNameForObjectsAndPackages(FName(*C.DesiredName).ToString()));
        const bool HandleNoOp=Sanitized.IsEqual(SelectedHandle->GetName(),ENameCase::CaseSensitive,false);
        Plan.HandleName=HandleNoOp?SelectedHandle->GetName():FNiagaraUtilities::GetUniqueName(Sanitized,OtherNames);
        Plan.Unique=HandleNoOp?Plan.OldUnique:Plan.HandleName.ToString();
        const FString ExistingName=Added->IsAsset()?Plan.OldObjectName.GetPlainNameString():Plan.OldObjectName.ToString();
        Plan.Rename=Plan.Unique!=Plan.OldUnique&&ExistingName!=Plan.Unique;
        const FName RequestedBase(*Plan.Unique);
        Plan.AllocatorBase=RequestedBase.IsNone()?UNiagaraEmitterBase::StaticClass()->GetFName():RequestedBase;
        if(Plan.AllocatorBase==NAME_Package)Plan.AllocatorBase=FName(NAME_Untitled);
        if(!Plan.OldPath.EndsWith(Plan.OldObjectName.ToString(),ESearchCase::CaseSensitive))return Stop(TEXT("set_name_root_path"),TEXT("captured root path/name suffix"),Plan.OldPath);
        Plan.ParentPath=Plan.OldPath.LeftChop(Plan.OldObjectName.ToString().Len());
        bool RootHeld=false;
        for(int32 HeldIndex=0;HeldIndex<Held.Num();++HeldIndex)
        {
            const auto& H=Held[HeldIndex];
            if(H.Object.Get()==Added.Get()){RootHeld=true;Plan.OwnedSuffixes.Add(HeldIndex,FString());}
            else if(H.Object->IsIn(Added.Get()))
            {
                if(!H.Name.StartsWith(Plan.OldPath+TEXT("."),ESearchCase::CaseSensitive))return Stop(TEXT("set_name_owned_path"),Plan.OldPath+TEXT(".<owned relative path>"),H.Name,H.Name);
                Plan.OwnedSuffixes.Add(HeldIndex,H.Name.Mid(Plan.OldPath.Len()));
            }
        }
        if(!RootHeld)return Stop(TEXT("set_name_root_hold"),TEXT("added root already held"),TEXT("missing"));
        if(!Current())return false;Plan.Ready=true;SetNamePlan=MoveTemp(Plan);return true;
    }
    bool AfterSetName()
    {
        Returned();
        if(Stopped||!SetNamePlan.Ready||!PrimaryRolesCurrent(R,C)){Stopped=true;return false;}
        const auto& Plan=SetNamePlan;
        if(Held.Num()!=Plan.HeldCount)return Stop(TEXT("set_name_held_count"),LexToString(Plan.HeldCount),LexToString(Held.Num()));
        // Validate every fixed operand and every unaffected full path before reading the returned root.
        for(int32 HeldIndex=0;HeldIndex<Held.Num();++HeldIndex)
        {
            const auto& H=Held[HeldIndex];if(!HeldFixed(H))return false;
            if(!Plan.OwnedSuffixes.Contains(HeldIndex))
            {const FString ActualPath=H.Object->GetPathName();if(!ActualPath.Equals(H.Name,ESearchCase::CaseSensitive))return Stop(TEXT("held_path"),H.Name,ActualPath,H.Name);}
        }
        const FName ActualObjectName=Added->GetFName();const FString ActualUnique=Added->GetUniqueEmitterName();
        if(ActualUnique!=Plan.Unique)return Stop(TEXT("set_name_unique_emitter_name"),Plan.Unique,ActualUnique,Plan.OldPath);
        if(!Plan.Rename)
        {if(!ActualObjectName.IsEqual(Plan.OldObjectName,ENameCase::CaseSensitive,true))return Stop(TEXT("set_name_noop_object_name"),Plan.OldObjectName.ToString(),ActualObjectName.ToString(),Plan.OldPath);}
        else
        {
            // Editor + parent + default None options: NameReuse is disabled. No allocator is called here.
            // NAME_Package uses the SDK's NAME_Untitled special branch; numeric desired suffixes use plain FName base.
            const FName RequestedBase(*Plan.Unique);const FName AllocatorBase=Plan.AllocatorBase;
            if(!FPlatformProperties::HasEditorOnlyData()||ActualObjectName.GetNumber()<=0
                ||!ActualObjectName.GetPlainNameString().Equals(AllocatorBase.GetPlainNameString(),ESearchCase::CaseSensitive)
                ||ActualObjectName==RequestedBase||ActualObjectName==Plan.OldObjectName)
                return Stop(TEXT("set_name_allocated_object_name"),AllocatorBase.GetPlainNameString()+TEXT(" + positive unique FName number, different from old/requested name"),ActualObjectName.ToString(),Plan.OldPath);
        }
        const FString NewRootPath=Plan.ParentPath+ActualObjectName.ToString();
        TMap<int32,FString> PlannedPaths;
        for(const auto& Owned:Plan.OwnedSuffixes)PlannedPaths.Add(Owned.Key,NewRootPath+Owned.Value);
        if(!Base(&PlannedPaths))return false;
        int32 HandleMatches=0;
        for(const auto& CandidateHandle:C.Target->GetEmitterHandles())if(CandidateHandle.GetId()==AddedId)
        {++HandleMatches;if(!CandidateHandle.GetName().IsEqual(Plan.HandleName,ENameCase::CaseSensitive,true))return Stop(TEXT("set_name_handle_name"),Plan.HandleName.ToString(),CandidateHandle.GetName().ToString(),Plan.OldPath);}
        if(HandleMatches!=1)return Stop(TEXT("set_name_handle_matches"),TEXT("1"),LexToString(HandleMatches));
        if(FindObject<UObject>(nullptr,*NewRootPath)!=Added.Get())return Stop(TEXT("set_name_fresh_root"),NewRootPath,TEXT("not exact added root"),Plan.OldPath);
        auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("before_object_path"),Plan.OldPath);Receipt->SetStringField(TEXT("after_object_path"),NewRootPath);
        Receipt->SetStringField(TEXT("expected_handle_name"),Plan.HandleName.ToString());Receipt->SetStringField(TEXT("expected_unique_emitter_name"),Plan.Unique);
        Receipt->SetBoolField(TEXT("uobject_rename_expected"),Plan.Rename);Receipt->SetNumberField(TEXT("owned_path_role_count"),PlannedPaths.Num());Receipt->SetBoolField(TEXT("allocator_prediction_called"),false);
        R.Data->SetObjectField(TEXT("user_copy_set_name_transition"),Receipt);
        // Commit only this verified transition. Subsequent callbacks again require exact full paths.
        for(const auto& Planned:PlannedPaths)Held[Planned.Key].Name=Planned.Value;
        SetNamePlan.Ready=false;return Current();
    }
    bool Hold(UObject* O)
    {
        if(!O)return true;if(!Loaded(O))return Stop(TEXT("object"),TEXT("live nullable reference"),TEXT("unavailable"));
        for(const auto& H:Held)if(H.Object.Get()==O)return Base();
        if(Held.Num()>=2048)return Stop(TEXT("object_count"),TEXT("<=2048"),TEXT("larger"));
        FHeld H;H.Object.Reset(O);UClass* K=O->GetClass();H.Class.Reset(K);UObject* Outer=O->GetOuter();H.Outer.Reset(Outer);UPackage* P=O->GetPackage();H.Package.Reset(P);
        if(!Loaded(K)||!Loaded(Outer)||!Loaded(P)||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_NewerVersionExists|CLASS_Deprecated))return Stop(TEXT("reference_role"),TEXT("existing native class/outer/package"),TEXT("unsupported"));
        H.Name=O->GetPathName();H.Flags=K->GetClassFlags();H.ClassBytes=K->GetPropertiesSize();if(auto* S=Cast<UStruct>(O))H.StructBytes=S->GetPropertiesSize();Held.Add(MoveTemp(H));return Base();
    }
    static bool Scalar(const FNiagaraTypeDefinition& T)
    {
        return T==FNiagaraTypeDefinition::GetFloatDef()||T==FNiagaraTypeDefinition::GetIntDef()||T==FNiagaraTypeDefinition::GetBoolDef()||T==FNiagaraTypeDefinition::GetVec2Def()||T==FNiagaraTypeDefinition::GetVec3Def()||T==FNiagaraTypeDefinition::GetVec4Def()||T==FNiagaraTypeDefinition::GetColorDef();
    }
    static int32 ScalarBytes(const FNiagaraTypeDefinition& T)
    {
        if(T==FNiagaraTypeDefinition::GetFloatDef())return sizeof(float);if(T==FNiagaraTypeDefinition::GetIntDef())return sizeof(int32);if(T==FNiagaraTypeDefinition::GetBoolDef())return sizeof(FNiagaraBool);
        if(T==FNiagaraTypeDefinition::GetVec2Def())return sizeof(FVector2f);if(T==FNiagaraTypeDefinition::GetVec3Def())return sizeof(FVector3f);if(T==FNiagaraTypeDefinition::GetVec4Def())return sizeof(FVector4f);if(T==FNiagaraTypeDefinition::GetColorDef())return sizeof(FLinearColor);return 0;
    }
    bool Payload(UNiagaraDataInterface* DI,TSharedRef<FJsonObject>& J)
    {
        if(!DI)return true;if(!Hold(DI))return false;
        const bool Float=DI->GetClass()==UNiagaraDataInterfaceArrayFloat::StaticClass(),Int=DI->GetClass()==UNiagaraDataInterfaceArrayInt32::StaticClass();
        if(!Float&&!Int)return Stop(TEXT("DI_class"),TEXT("exact native ArrayFloat or ArrayInt32; other valid DI copy remains pending"),DI->GetClass()->GetPathName());
        auto* A=static_cast<UNiagaraDataInterfaceArray*>(DI);TArray<TSharedPtr<FJsonValue>> Values;
        if(Float){const auto& Data=static_cast<UNiagaraDataInterfaceArrayFloat*>(DI)->FloatData;if(Data.Num()>4096)return Stop(TEXT("DI_array"),TEXT("<=4096"),TEXT("larger"));for(float V:Data){if(!FMath::IsFinite(V))return Stop(TEXT("DI_value"),TEXT("finite float"),TEXT("nonfinite"));Values.Add(MakeShared<FJsonValueNumber>(V));}}
        else {const auto& Data=static_cast<UNiagaraDataInterfaceArrayInt32*>(DI)->IntData;if(Data.Num()>4096)return Stop(TEXT("DI_array"),TEXT("<=4096"),TEXT("larger"));for(int32 V:Data)Values.Add(MakeShared<FJsonValueNumber>(V));}
        J->SetArrayField(TEXT("values"),Values);J->SetNumberField(TEXT("max_elements"),A->MaxElements);J->SetNumberField(TEXT("gpu_sync_mode"),static_cast<int32>(A->GpuSyncMode));return Base();
    }
    bool Read(UNiagaraSystem* S,TMap<FString,FCell>& Out)
    {
        if(!Base())return false;Out.Empty();auto& Store=S->GetExposedParameters();TArray<FNiagaraVariable> Variables;Store.GetParameters(Variables);
        if(Store.GetOwner()!=S||Variables.Num()>512||Store.GetParameterDataArray().Num()>262144||Store.GetDataInterfaces().Num()>512||Store.GetUObjects().Num()>512)return Stop(TEXT("store"),TEXT("owned bounded parameter store"),TEXT("other owner/count"));
        for(const auto& V:Variables)
        {
            if(!Base())return false;const auto T=V.GetType();const FString Name=V.GetName().ToString(),Key=Name.ToLower();
            if(!T.IsValid()||T.GetFlags()!=0||T.IsEnum()||!Bounded(Name,512)||Out.Contains(Key))return Stop(TEXT("declaration"),TEXT("unique bounded builtin name/type"),Name);
            UObject* TypeObject=T.IsUObject()||T.IsDataInterface()?static_cast<UObject*>(const_cast<UClass*>(T.GetClass())):static_cast<UObject*>(const_cast<UScriptStruct*>(T.GetScriptStruct()));
            // Native type objects may be CDO-independent; hold their actual role without constructing defaults.
            if(!Hold(TypeObject))return false;FCell Cell;Cell.Variable=FNiagaraVariable(T,V.GetName());Cell.LegacyType=T.GetNameText().ToString();if(!Base())return false;auto& J=Cell.Row;
            J->SetStringField(TEXT("name"),Name);J->SetStringField(TEXT("type"),T.GetName());J->SetStringField(TEXT("type_object"),Path(TypeObject));
            const int32 Offset=Store.IndexOf(V),Size=V.GetSizeInBytes();
            if(T.IsDataInterface())
            {
                if(T.GetClass()!=UNiagaraDataInterfaceArrayFloat::StaticClass()&&T.GetClass()!=UNiagaraDataInterfaceArrayInt32::StaticClass())return Stop(TEXT("DI_type"),TEXT("ArrayFloat/ArrayInt32"),T.GetName());
                if(!Store.GetDataInterfaces().IsValidIndex(Offset))return Stop(TEXT("DI_slot"),TEXT("valid index"),Name);
                UNiagaraDataInterface* D=Store.GetDataInterface(Offset);if(D&&(!Hold(D)||D->GetClass()!=T.GetClass()||D->GetOuter()!=S))return Stop(TEXT("DI_owner"),TEXT("same type/direct owner"),Name);
                Cell.Object=D;auto P=MakeShared<FJsonObject>();if(!Payload(D,P))return false;J->SetObjectField(TEXT("payload"),P);J->SetStringField(TEXT("reference"),Path(D));J->SetField(TEXT("value"),MakeShared<FJsonValueNull>());
            }
            else if(T.IsUObject())
            {
                if(T!=FNiagaraTypeDefinition::GetUObjectDef()&&T!=FNiagaraTypeDefinition::GetUMaterialDef())return Stop(TEXT("object_type"),TEXT("UObject or Material"),T.GetName());
                if(!Store.GetUObjects().IsValidIndex(Offset)||!Store.GetUObjects()[Offset].IsResolved())return Stop(TEXT("object_slot"),TEXT("resolved nullable slot"),Name);
                UObject* O=Store.GetUObject(Offset).Get();if(O&&(!Hold(O)||!O->IsA(T.GetClass())))return Stop(TEXT("object_value"),TEXT("live assignable reference"),Name);
                Cell.Object=O;J->SetStringField(TEXT("reference"),Path(O));J->SetField(TEXT("value"),O?TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(Path(O))):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
            }
            else
            {
                if(!Scalar(T)&&T!=FNiagaraTypeDefinition::GetPositionDef())return Stop(TEXT("serializable_type"),TEXT("builtin scalar/vector/color/Position"),T.GetName());
                if(Size<=0||Size>64||(Scalar(T)&&Size!=ScalarBytes(T))||Offset<0||Offset>Store.GetParameterDataArray().Num()-Size)return Stop(TEXT("data_slot"),TEXT("bounded current native-size data"),Name);
                TArray<TSharedPtr<FJsonValue>> Values;
                if(T==FNiagaraTypeDefinition::GetPositionDef())
                {const FVector* P=CopyPosition(Store,V.GetName());if(!P||P->ContainsNaN())return Stop(TEXT("Position"),TEXT("finite stored Position"),Name);Cell.Position=*P;Values={MakeShared<FJsonValueNumber>(P->X),MakeShared<FJsonValueNumber>(P->Y),MakeShared<FJsonValueNumber>(P->Z)};J->SetArrayField(TEXT("value"),Values);}
                else
                {
                    Cell.Bytes.Append(Store.GetParameterDataArray().GetData()+Offset,Size);J->SetStringField(TEXT("bytes"),BytesToHex(Cell.Bytes.GetData(),Cell.Bytes.Num()));
                    if(T==FNiagaraTypeDefinition::GetIntDef())J->SetNumberField(TEXT("value"),Store.GetParameterValue<int32>(V));
                    else if(T==FNiagaraTypeDefinition::GetBoolDef()){const auto B=Store.GetParameterValue<FNiagaraBool>(V);if(!B.IsValid())return Stop(TEXT("bool"),TEXT("valid bool"),Name);J->SetBoolField(TEXT("value"),B.GetValue());}
                    else {for(int32 I=0;I<Size;I+=sizeof(float)){float X;FMemory::Memcpy(&X,Cell.Bytes.GetData()+I,sizeof(float));if(!FMath::IsFinite(X))return Stop(TEXT("number"),TEXT("finite"),Name);Values.Add(MakeShared<FJsonValueNumber>(X));}if(Values.Num()==1)J->SetField(TEXT("value"),Values[0]);else J->SetArrayField(TEXT("value"),Values);}
                }
            }
            Out.Add(Key,MoveTemp(Cell));
        }
        TArray<FNiagaraVariable> Users;Store.GetUserParameters(Users);if(Users.Num()>512)return Stop(TEXT("redirects"),TEXT("<=512"),TEXT("larger"));
        TSet<FName> Names;for(const auto& V:Users){FNiagaraVariableBase Canonical(V);if(!Store.RedirectUserVariable(Canonical)||!Out.Contains(Canonical.GetName().ToString().ToLower())||Out[Canonical.GetName().ToString().ToLower()].Variable.GetType()!=Canonical.GetType()||Names.Contains(V.GetName()))return Stop(TEXT("redirect"),TEXT("unique actual User declaration"),V.GetName().ToString());Names.Add(V.GetName());}
        return Base();
    }
    bool Same(const TMap<FString,FCell>& Before,const TMap<FString,FCell>& Now,const FString& Except=FString())
    {
        for(const auto& P:Before){if(P.Key==Except)continue;const auto* Q=Now.Find(P.Key);if(!Q||Text(P.Value.Row)!=Text(Q->Row))return Stop(TEXT("unselected_parameter"),P.Key,Q?Text(Q->Row):TEXT("missing"));}
        for(const auto& P:Now)if(P.Key!=Except&&!Before.Contains(P.Key))return Stop(TEXT("unexpected_parameter"),TEXT("original inventory"),P.Key);return true;
    }
    bool Current(const FString& Except=FString(),TMap<FString,FCell>* Actual=nullptr)
    {
        if(!Base())return false;
        // TW does not continue the legacy aggregate observer after its first incomplete graph.
        for(const auto& G:SourceGraphs){bool Complete=false;GraphSnapshot(G.Graph.Get(),Complete);if(!Complete)return Stop(TEXT("source_graph"),TEXT("complete captured graph"),TEXT("incomplete"));}
        bool Complete=false;auto SourceNow=SourceObservation(C,Complete);if(!Complete||Text(SourceNow)!=Text(C.SourceBefore))return Stop(TEXT("source_graphs_handles"),TEXT("captured original emitter/source observation"),TEXT("different/incomplete"));
        TMap<FString,FCell> S,T;if(C.SourceSystem&&(!Read(C.SourceSystem,S)||!Same(SourceBefore,S)))return false;
        if(!Read(C.Target,T)||!Same(Expected,T,Except))return false;if(Actual)*Actual=MoveTemp(T);return true;
    }
    bool Enter(const TCHAR* Name,bool Mutation=true)
    {
        if(!Current())return false;R.Stage=Name;if(Mutation)R.bMutationAttempted=true;Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("api"),Name);Call->SetBoolField(TEXT("entered"),true);Call->SetBoolField(TEXT("returned"),false);Calls.Add(MakeShared<FJsonValueObject>(Call));R.Data->SetArrayField(TEXT("user_copy_sdk_calls"),Calls);return true;
    }
    void Returned(){Call->SetBoolField(TEXT("returned"),true);R.Data->SetArrayField(TEXT("user_copy_sdk_calls"),Calls);}
    bool After(const FString& Except=FString(),TMap<FString,FCell>* Actual=nullptr){Returned();return Current(Except,Actual);}
    void Count(const TCHAR* Name){R.Data->SetNumberField(Name,R.Data->GetNumberField(Name)+1);}
    bool Begin()
    {
        TargetHandleCount=C.Target->GetEmitterHandles().Num();if(C.SourceSystem)SourceHandleCount=C.SourceSystem->GetEmitterHandles().Num();if(!Hold(C.SourceEmitter))return false;
        {const auto* D=C.SourceEmitter->GetEmitterData(C.SourceVersion);if(!D||!D->GraphSource.IsResolved()||!D->VersionedParent.Emitter.IsResolved()||!D->VersionedParentAtLastMerge.Emitter.IsResolved()||!Hold(D->GraphSource.Get())||!Hold(D->VersionedParent.Emitter.Get())||!Hold(D->VersionedParentAtLastMerge.Emitter.Get()))return false;SelectedGraph.Reset(D->GraphSource.Get());SelectedParent.Reset(D->VersionedParent.Emitter.Get());SelectedLastParent.Reset(D->VersionedParentAtLastMerge.Emitter.Get());SelectedCaptured=true;}
        for(UNiagaraSystem* System:{C.Target,C.SourceSystem})
        {
            if(!System)continue;const auto& Rows=System->GetEmitterHandles();
            for(int32 Index=0;Index<Rows.Num();++Index)
            {
                const auto I=Rows[Index].GetInstance();if(!I.Emitter.IsResolved()||!Hold(I.Emitter.Get()))return false;UNiagaraEmitter* E=I.Emitter.Get();if(!E||E->GetClass()!=UNiagaraEmitter::StaticClass()||E->GetOuter()!=System)return Stop(TEXT("source_target_instance"),TEXT("exact direct-owned native emitter"),TEXT("other role"));
                const auto* D=E->GetEmitterData(I.Version);if(!D||!D->GraphSource.IsResolved()||!D->VersionedParent.Emitter.IsResolved()||!D->VersionedParentAtLastMerge.Emitter.IsResolved()||!Hold(D->GraphSource.Get())||!Hold(D->VersionedParent.Emitter.Get())||!Hold(D->VersionedParentAtLastMerge.Emitter.Get()))return false;
                FEmitterRole B;B.System=System;B.Index=Index;B.Id=Rows[Index].GetId();B.Version=I.Version;B.Emitter.Reset(E);B.Graph.Reset(D->GraphSource.Get());B.Parent.Reset(D->VersionedParent.Emitter.Get());B.LastParent.Reset(D->VersionedParentAtLastMerge.Emitter.Get());B.Row=Text(HandleRow(Rows[Index],Index));Emitters.Add(MoveTemp(B));
            }
        }
        for(auto* S:C.OriginalSources.Sources){if(!Hold(S)||!S->NodeGraph.IsResolved()||!Hold(S->NodeGraph.Get()))return false;FSourceGraph G;G.Source.Reset(S);G.Graph.Reset(S->NodeGraph.Get());for(UEdGraphNode* N:G.Graph->Nodes)if(!Hold(N))return false;SourceGraphs.Add(MoveTemp(G));}
        for(auto* S:C.OriginalSources.Scripts)if(!Hold(S))return false;
        if(!Read(C.Target,Expected))return false;if(C.SourceSystem)
        {
            if(!Read(C.SourceSystem,SourceBefore))return false;TArray<FNiagaraVariable> Users;C.SourceSystem->GetExposedParameters().GetUserParameters(Users);
            for(const auto& V:Users){FNiagaraVariableBase Canonical(V);C.SourceSystem->GetExposedParameters().RedirectUserVariable(Canonical);FCell Cell=SourceBefore[Canonical.GetName().ToString().ToLower()];Cell.ReportName=V.GetName().ToString();SourceUsers.Add(MoveTemp(Cell));}
            for(const auto& P:SourceBefore){const auto* T=Expected.Find(P.Key);if(T&&T->Variable.GetType()!=P.Value.Variable.GetType())return Fail(R,TEXT("niagara_emitter_user_copy_type_collision_pending"),TEXT("Same-name different-type User declarations remain incomplete; no attach dispatched."));}
        }
        R.Data->SetNumberField(TEXT("source_user_parameter_count"),SourceUsers.Num());R.Data->SetBoolField(TEXT("copy_source_user_parameters_supported"),true);R.Data->SetBoolField(TEXT("explicit_user_parameter_writes"),C.SourceSystem!=nullptr);Started=true;return Current();
    }
    bool Copy()
    {
        if(!C.SourceSystem)return true;if(!Enter(TEXT("copy_target_Modify")))return false;C.Target->Modify();if(!After())return false;
        for(const auto& Source:SourceUsers)
        {
            const auto V=Source.Variable;const auto T=V.GetType();const FString Key=V.GetName().ToString().ToLower();const bool Exists=Expected.Contains(Key);
            if(!Exists)
            {
                if(!Enter(TEXT("User_AddParameter")))return false;const bool AddedParameter=CopyAdd(C.Target->GetExposedParameters(),V);Call->SetBoolField(TEXT("bool_return"),AddedParameter);if(AddedParameter)Count(TEXT("added_user_parameter_count"));TMap<FString,FCell> Actual;if(!After(Key,&Actual))return false;
                auto* A=Actual.Find(Key);if(!AddedParameter||!A||A->Variable.GetType()!=T)return Stop(TEXT("added_slot"),TEXT("one new selected default declaration"),Key);
                bool Default=true;if(T.IsDataInterface())Default=A->Object&&A->Row->GetObjectField(TEXT("payload"))->GetArrayField(TEXT("values")).IsEmpty()&&A->Row->GetObjectField(TEXT("payload"))->GetIntegerField(TEXT("max_elements"))==0&&A->Row->GetObjectField(TEXT("payload"))->GetIntegerField(TEXT("gpu_sync_mode"))==static_cast<int32>(ENiagaraGpuSyncMode::SyncCpuToGpu);
                else if(T.IsUObject())Default=A->Object==nullptr;else if(T==FNiagaraTypeDefinition::GetPositionDef())Default=A->Position==FVector::ZeroVector;else for(uint8 B:A->Bytes)Default&=B==0;
                if(!Default)return Stop(TEXT("default_slot"),TEXT("SDK initialized default value"),Text(A->Row));Expected=MoveTemp(Actual);
            }
            bool Good=false,SkipDICounted=false;FString Reason;UNiagaraDataInterface* Destination=nullptr;
            if(T.IsDataInterface())
            {
                if(!Source.Object)Reason=TEXT("missing_source_data_interface");
                else
                {
                    Destination=static_cast<UNiagaraDataInterface*>(Expected[Key].Object);
                    if(!Destination)
                    {
                        if(!Enter(TEXT("User_NewDataInterface")))return false;const EObjectFlags Flags=UNiagaraDataInterface::BuildObjectFlagsForOwner(C.Target,RF_Transactional);Destination=NewObject<UNiagaraDataInterface>(C.Target,T.GetClass(),NAME_None,Flags);TStrongObjectPtr<UNiagaraDataInterface> NewHold(Destination);Call->SetBoolField(TEXT("returned_nonnull"),Destination!=nullptr);Returned();if(!Base()||!Hold(Destination)||!Current())return false;
                        if(!Enter(TEXT("User_SetDataInterface")))return false;C.Target->GetExposedParameters().SetDataInterface(Destination,V);Count(TEXT("created_user_data_interface_parameter_count"));TMap<FString,FCell> A;if(!After(Key,&A))return false;if(A[Key].Object!=Destination)return Stop(TEXT("DI_binding"),TEXT("returned new DI"),Key);Expected=MoveTemp(A);
                    }
                    if(!Enter(TEXT("User_DI_Modify")))return false;Destination->Modify();if(!After())return false;
                    if(!Enter(TEXT("User_DI_CopyTo")))return false;Good=static_cast<UNiagaraDataInterface*>(Source.Object)->CopyTo(Destination);Call->SetBoolField(TEXT("bool_return"),Good);if(Good){Count(TEXT("copied_user_data_interface_parameter_count"));Count(TEXT("copied_user_parameter_count"));if(Exists)Count(TEXT("updated_user_parameter_count"));}else{Count(TEXT("skipped_user_data_interface_parameter_count"));SkipDICounted=true;}TMap<FString,FCell> A;if(!After(Key,&A))return false;if(!A.Contains(Key)||A[Key].Object!=Destination)return Stop(TEXT("DI_return_binding"),TEXT("same selected destination DI"),Key);
                    if(Good&&Text(Source.Row->GetObjectField(TEXT("payload")).ToSharedRef())!=Text(A[Key].Row->GetObjectField(TEXT("payload")).ToSharedRef()))return Stop(TEXT("DI_payload"),Text(Source.Row),Text(A[Key].Row));Expected=MoveTemp(A);
                    if(!Good)Reason=TEXT("data_interface_copy_failed");
                }
                if(!Good&&!SkipDICounted)Count(TEXT("skipped_user_data_interface_parameter_count"));
            }
            else if(T.IsUObject()&&Source.Object&&Source.Object->GetPackage()!=C.Target->GetPackage()&&!Source.Object->HasAnyFlags(RF_Public|RF_Standalone))
            {Reason=TEXT("private_uobject_parameter_not_copied");Count(TEXT("skipped_user_private_uobject_parameter_count"));}
            else
            {
                if(!Enter(TEXT("User_SetValue")))return false;
                if(T.IsUObject()){C.Target->GetExposedParameters().SetUObject(Source.Object,V);Good=true;}
                else if(T==FNiagaraTypeDefinition::GetPositionDef())Good=CopySetPosition(C.Target->GetExposedParameters(),Source.Position,V.GetName());
                else Good=C.Target->GetExposedParameters().SetParameterData(Source.Bytes.GetData(),V,true);
                if(T.IsUObject()){Call->SetBoolField(TEXT("void_returned"),true);Call->SetBoolField(TEXT("nominal_original_copied_value"),Good);}else Call->SetBoolField(TEXT("bool_return"),Good);if(Good){Count(TEXT("copied_user_parameter_count"));if(Exists)Count(TEXT("updated_user_parameter_count"));}else{Reason=TEXT("missing_or_unsupported_serializable_data");Count(TEXT("skipped_user_missing_data_parameter_count"));}TMap<FString,FCell> A;if(!After(Key,&A))return false;
                if(Good&&Text(A[Key].Row)!=Text(Source.Row))return Stop(TEXT("copied_value"),Text(Source.Row),Text(A[Key].Row));Expected=MoveTemp(A);
            }
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("parameter_name"),Source.ReportName);Row->SetStringField(TEXT("type"),Source.LegacyType);
            if(Good){Row->SetBoolField(TEXT("added"),!Exists);Row->SetField(TEXT("value"),Expected[Key].Row->Values[TEXT("value")]);if(Destination){Row->SetStringField(TEXT("source_data_interface_path"),Path(Source.Object));Row->SetStringField(TEXT("target_data_interface_path"),Path(Destination));Row->SetStringField(TEXT("data_interface_class"),Path(Destination->GetClass()));}Copied.Add(MakeShared<FJsonValueObject>(Row));}
            else {Row->SetStringField(TEXT("reason"),Reason);if(T.IsUObject()){Row->SetStringField(TEXT("object_path"),Path(Source.Object));Row->SetStringField(TEXT("object_package"),Source.Object?Path(Source.Object->GetPackage()):FString());}Skipped.Add(MakeShared<FJsonValueObject>(Row));}
            R.Data->SetArrayField(TEXT("copied_user_parameters"),Copied);R.Data->SetArrayField(TEXT("skipped_user_parameters"),Skipped);R.Data->SetBoolField(TEXT("user_copy_selected_prefix_verified"),true);if(!Current())return false;
        }
        // Keep the original terminal store validation; it is not a rollback or a second conversion.
        if(!Enter(TEXT("User_SanityCheckData"),false))return false;CopySanity(C.Target->GetExposedParameters());return After();
    }
};
FResult AttachWithUserCopy(const FString& TargetPath,const FString& EmitterPath,const FString& SourcePath,const FString& IndexJson,const FString& SourceName,const FString& NewName,const FString& EnabledJson,bool Allow,bool Save,bool Compile)
{
    FResult R;Initialize(R,TargetPath,Save);R.Data->SetBoolField(TEXT("copy_source_user_parameters"),true);R.Data->SetBoolField(TEXT("request_compile"),Compile);R.Data->SetStringField(TEXT("enabled_json"),EnabledJson);
    TSharedPtr<FJsonValue> Enabled;if(!OptionalScalar(EnabledJson,Enabled)||(Enabled->Type!=EJson::Null&&Enabled->Type!=EJson::Boolean)){Fail(R,TEXT("niagara_emitter_enabled_invalid"),TEXT("enabled must be JSON null or bool."));return R;}
    FContext C;if(!Prepare(R,C,TargetPath,EmitterPath,SourcePath,IndexJson,SourceName,NewName,true,Allow))return R;
    FUserCopy W(R,C);if(!W.Begin())return R;
    // Existing localization mutates arbitrary private-script closures. This new branch does not infer their callback effects.
    if(!C.PrivateClosure.IsEmpty()){Fail(R,TEXT("niagara_emitter_user_copy_private_closure_pending"),TEXT("User copy with external private script localization remains incomplete; no attach dispatched."));return R;}
    const bool HasEnabled=Enabled->Type==EJson::Boolean;R.Data->SetBoolField(TEXT("enabled_override_present"),HasEnabled);const int32 BeforeCount=C.Target->GetEmitterHandles().Num();
    if(!W.Enter(TEXT("attach_target_Modify")))return R;C.Target->Modify();if(!W.After())return R;
    if(!W.Enter(TEXT("AddEmitterToSystem")))return R;const FGuid AddedId=FNiagaraEditorUtilities::AddEmitterToSystem(*C.Target,*C.SourceEmitter,C.SourceVersion,true);R.Data->SetStringField(TEXT("native_returned_emitter_id"),Guid(AddedId));W.AttachmentReturned=true;if(!W.After())return R;
    TStrongObjectPtr<UNiagaraEmitter> AddedEmitter;
    auto Fresh=[&]()->FNiagaraEmitterHandle*
    {
        if(!W.Current())return nullptr;FNiagaraEmitterHandle* Found=nullptr;int32 Matches=0;for(auto& H:C.Target->GetEmitterHandles())if(H.GetId()==AddedId){Found=&H;++Matches;}
        if(!AddedId.IsValid()||Matches!=1||C.Target->GetEmitterHandles().Num()!=BeforeCount+1){W.Stop(TEXT("attached_handle"),TEXT("one returned ID/count+1"),Guid(AddedId));return nullptr;}
        UNiagaraEmitter* E=Found->GetInstance().Emitter;if(!Loaded(E)||E->GetClass()!=UNiagaraEmitter::StaticClass()||E->GetOuter()!=C.Target||!Found->GetEmitterData()||(AddedEmitter.Get()&&AddedEmitter.Get()!=E)){W.Stop(TEXT("attached_instance"),TEXT("same live owned native emitter/version"),TEXT("different"));return nullptr;}
        AddedEmitter.Reset(E);if(!W.Added.Get()){W.Added.Reset(E);W.AddedId=AddedId;W.AddedVersion=Found->GetInstance().Version;const auto* NewData=E->GetEmitterData(W.AddedVersion);if(!NewData||!NewData->GraphSource.IsResolved()){W.Stop(TEXT("new_emitter_graph"),TEXT("resolved actual GraphSource"),TEXT("unavailable"));return nullptr;}W.AddedGraph.Reset(NewData->GraphSource.Get());if(!W.Hold(E)||!W.Hold(W.AddedGraph.Get()))return nullptr;}return Found;
    };
    auto* H=Fresh();if(!H)return R;if(!W.PrepareSetName()||!W.Enter(TEXT("SetEmitterName")))return R;
    // No borrowed handle-array address crosses a current observer.
    H=nullptr;for(auto& SetNameHandle:C.Target->GetEmitterHandles())if(SetNameHandle.GetId()==AddedId)H=&SetNameHandle;
    if(!H){W.Stop(TEXT("set_name_dispatch_handle"),Guid(AddedId),TEXT("missing"));return R;}
    H->SetName(FName(*C.DesiredName),*C.Target);if(!W.AfterSetName())return R;
    if(HasEnabled){H=Fresh();if(!H||!W.Enter(TEXT("SetEmitterEnabled")))return R;H->SetIsEnabled(Enabled->AsBool(),*C.Target,false);if(!W.After())return R;}
    H=Fresh();if(!H)return R;bool HadParent=H->GetEmitterData()->VersionedParent.Emitter!=nullptr||H->GetEmitterData()->VersionedParentAtLastMerge.Emitter!=nullptr;
    if(HadParent){if(!W.Enter(TEXT("attached_Modify")))return R;AddedEmitter->Modify();if(!W.After()||!Fresh())return R;H=Fresh();if(!H||!W.Enter(TEXT("RemoveEmitterParent")))return R;H->GetEmitterData()->RemoveParent();if(!W.After())return R;}
    H=Fresh();if(!H)return R;R.Data->SetBoolField(TEXT("parent_reference_removed"),HadParent);R.Data->SetStringField(TEXT("new_emitter_name"),H->GetName().ToString());R.Data->SetStringField(TEXT("new_emitter_id"),Guid(AddedId));R.Data->SetNumberField(TEXT("new_emitter_index"),C.Target->GetEmitterHandles().Num()-1);R.Data->SetBoolField(TEXT("new_emitter_enabled"),H->GetIsEnabled());R.Data->SetStringField(TEXT("new_emitter_unique_instance_name"),H->GetUniqueInstanceName());
    if(!W.Copy())return R;
    if(!Fresh()||!W.Enter(TEXT("emitter_PostEditChange")))return R;AddedEmitter->PostEditChange();R.Data->SetBoolField(TEXT("emitter_post_edit_invoked"),true);if(!W.After()||!Fresh())return R;
    if(!W.Enter(TEXT("ComputeEmittersExecutionOrder")))return R;C.Target->ComputeEmittersExecutionOrder();R.Data->SetBoolField(TEXT("execution_order_refresh_invoked"),true);if(!W.After()||!Fresh())return R;
    if(!W.Enter(TEXT("SynchronizeOverviewGraphWithSystem")))return R;C.EditorData->SynchronizeOverviewGraphWithSystem(*C.Target);R.Data->SetBoolField(TEXT("overview_sync_invoked"),true);if(!W.After()||!Fresh())return R;
    C.Target->MarkPackageDirty();if(!W.Current()||!Observe(R,C)||!VerifyAttached(R,C,AddedId,HasEnabled,HasEnabled&&Enabled->AsBool()))return R;
    if(Compile){if(!W.Enter(TEXT("request_compile")))return R;R.Data->SetBoolField(TEXT("compile_requested"),true);R.Data->SetBoolField(TEXT("compile_attempted"),true);const bool Returned=C.Target->RequestCompile(false);R.Data->SetBoolField(TEXT("compile_request_returned"),Returned);if(!W.After()||!Fresh())return R;}
    if(Save)
    {
        if(!W.Enter(TEXT("save_compile_state"),false))return R;auto State=CompileState(C.Target);R.Data->SetObjectField(TEXT("save_compile_state"),State);if(!W.After())return R;
        if(!State->GetBoolField(TEXT("ready_to_save"))){R.Stage=TEXT("save_deferred");R.Data->SetBoolField(TEXT("save_deferred"),true);Fail(R,TEXT("niagara_save_deferred_compile_pending"),TEXT("Attached/copied work remains; inspect readiness then explicitly save, do not replay attach."));}
        else {if(!W.Enter(TEXT("save")))return R;R.Data->SetBoolField(TEXT("save_attempted"),true);const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(C.Target,false);R.Data->SetBoolField(TEXT("saved"),Saved);if(!W.After()||!Fresh())return R;if(!Saved||C.Target->GetPackage()->IsDirty())Fail(R,TEXT("niagara_emitter_save_failed"),TEXT("Actual save false/dirty; work retained."));}
    }
    if(!W.Current()||!Observe(R,C)||!VerifyAttached(R,C,AddedId,HasEnabled,HasEnabled&&Enabled->AsBool()))return R;
    R.Data->SetBoolField(TEXT("source_user_parameters_preserved"),true);R.Data->SetBoolField(TEXT("target_unselected_user_parameters_preserved"),true);R.bSuccess=R.bPostconditionPassed=R.bChangeStateKnown&&R.ErrorCode.IsEmpty();if(R.bSuccess)R.Stage=TEXT("complete");return R;
}

}

Function::FResult Inspect(const FString& TargetSystemPath, const FString& SourceEmitterPath,
    const FString& SourceSystemPath, const FString& SourceEmitterIndexJson,
    const FString& SourceEmitterName, const FString& NewEmitterName)
{
    FResult R; Initialize(R, TargetSystemPath); R.Data->SetBoolField(TEXT("read_only"), true); FContext C;
    if (!Prepare(R, C, TargetSystemPath, SourceEmitterPath, SourceSystemPath, SourceEmitterIndexJson, SourceEmitterName, NewEmitterName, false, false)) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(C.Target), KeepSourceSystem(C.SourceSystem); TStrongObjectPtr<UNiagaraEmitter> KeepSource(C.SourceEmitter);
    const bool Ok = Observe(R, C); const bool Preserved = Ok && Text(C.Before) == Text(R.Data->GetObjectField(TEXT("after")).ToSharedRef());
    R.Data->SetBoolField(TEXT("state_preserved"), Preserved); if (!Preserved) { Fail(R, TEXT("niagara_emitter_inspection_changed"), TEXT("Read-only observation did not preserve target state.")); }
    R.bSuccess = R.bPostconditionPassed = R.bChangeStateKnown && Preserved && R.ErrorCode.IsEmpty(); if (R.bSuccess) { R.Stage = TEXT("complete"); } return R;
}
Function::FResult Attach(const FString& TargetSystemPath, const FString& SourceEmitterPath,
    const FString& SourceSystemPath, const FString& SourceEmitterIndexJson,
    const FString& SourceEmitterName, const FString& NewEmitterName, const FString& EnabledJson,
    bool bCopySourceUserParameters, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    FResult R; Initialize(R, TargetSystemPath, bSave); R.Data->SetBoolField(TEXT("copy_source_user_parameters"), bCopySourceUserParameters);
    R.Data->SetBoolField(TEXT("request_compile"), bRequestCompile); R.Data->SetStringField(TEXT("enabled_json"), EnabledJson);
    TSharedPtr<FJsonValue> Enabled;
    if (!OptionalScalar(EnabledJson, Enabled) || (Enabled->Type != EJson::Null && Enabled->Type != EJson::Boolean))
    { Fail(R, TEXT("niagara_emitter_enabled_invalid"), TEXT("enabled must be JSON null (no override) or a boolean.")); return R; }
    if (bCopySourceUserParameters)
    { return AttachWithUserCopy(TargetSystemPath,SourceEmitterPath,SourceSystemPath,SourceEmitterIndexJson,SourceEmitterName,NewEmitterName,EnabledJson,bAllowSourceEdit,bSave,bRequestCompile); }
    const bool HasEnabled = Enabled->Type == EJson::Boolean; R.Data->SetBoolField(TEXT("enabled_override_present"), HasEnabled);
    FContext C; if (!Prepare(R, C, TargetSystemPath, SourceEmitterPath, SourceSystemPath, SourceEmitterIndexJson, SourceEmitterName, NewEmitterName, true, bAllowSourceEdit)) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(C.Target), KeepSourceSystem(C.SourceSystem); TStrongObjectPtr<UNiagaraEmitter> KeepSource(C.SourceEmitter);
    const int32 BeforeCount = C.Target->GetEmitterHandles().Num(); R.Stage = TEXT("attach_emitter"); R.bMutationAttempted = true; C.Target->Modify();
    const FGuid AddedId = FNiagaraEditorUtilities::AddEmitterToSystem(*C.Target, *C.SourceEmitter, C.SourceVersion, true);
    R.Data->SetStringField(TEXT("native_returned_emitter_id"), Guid(AddedId)); int32 AddedIndex = INDEX_NONE, Matches = 0;
    for (int32 I = 0; I < C.Target->GetEmitterHandles().Num(); ++I) { if (C.Target->GetEmitterHandles()[I].GetId() == AddedId) { AddedIndex = I; ++Matches; } }
    if (!AddedId.IsValid() || Matches != 1 || C.Target->GetEmitterHandles().Num() != BeforeCount + 1)
    { Fail(R, TEXT("niagara_emitter_attach_identity_failed"), TEXT("Native attach did not add exactly one uniquely returned handle; observed work retained.")); Observe(R, C); return R; }
    auto& Added = C.Target->GetEmitterHandles()[AddedIndex]; auto Instance = Added.GetInstance(); auto* Data = Added.GetEmitterData();
    if (!Loaded(Instance.Emitter) || Instance.Emitter->GetClass() != UNiagaraEmitter::StaticClass() || Instance.Emitter->GetOuter() != C.Target || !Data)
    { Fail(R, TEXT("niagara_emitter_owned_instance_failed"), TEXT("Returned handle does not own an ordinary copied emitter/version; partial work retained.")); Observe(R, C); return R; }
    R.Stage = TEXT("name_and_enabled"); Added.SetName(FName(*C.DesiredName), *C.Target);
    if (HasEnabled) { Added.SetIsEnabled(Enabled->AsBool(), *C.Target, false); }
    const bool HadParent = Data->VersionedParent.Emitter != nullptr || Data->VersionedParentAtLastMerge.Emitter != nullptr;
    R.Stage = TEXT("detach_parent"); if (HadParent) { Instance.Emitter->Modify(); Data->RemoveParent(); }
    const bool Detached = Data->VersionedParent.Emitter == nullptr && Data->VersionedParentAtLastMerge.Emitter == nullptr;
    R.Data->SetBoolField(TEXT("parent_reference_removed"), HadParent && Detached); R.Data->SetBoolField(TEXT("parent_detachment_verified"), Detached);
    R.Data->SetStringField(TEXT("new_emitter_name"), Added.GetName().ToString()); R.Data->SetStringField(TEXT("new_emitter_id"), Guid(AddedId));
    R.Data->SetNumberField(TEXT("new_emitter_index"), AddedIndex); R.Data->SetBoolField(TEXT("new_emitter_enabled"), Added.GetIsEnabled());
    R.Data->SetStringField(TEXT("new_emitter_unique_instance_name"), Added.GetUniqueInstanceName()); R.Data->SetObjectField(TEXT("new_emitter"), HandleRow(Added, AddedIndex));
    if (!Detached || (HasEnabled && Added.GetIsEnabled() != Enabled->AsBool()) || (!HasEnabled && !Added.GetIsEnabled()))
    { Fail(R, TEXT("niagara_emitter_detach_enabled_failed"), TEXT("Parent references or actual enabled override/default differ; partial work retained.")); }
    if (R.ErrorCode.IsEmpty()) { Localize(R, C); }
    if (R.ErrorCode.IsEmpty() && R.bChangeStateKnown)
    {
        R.Stage = TEXT("post_edit_refresh"); Instance.Emitter->PostEditChange(); R.Data->SetBoolField(TEXT("emitter_post_edit_invoked"), true);
        C.Target->ComputeEmittersExecutionOrder(); R.Data->SetBoolField(TEXT("execution_order_refresh_invoked"), true);
        C.EditorData->SynchronizeOverviewGraphWithSystem(*C.Target); R.Data->SetBoolField(TEXT("overview_sync_invoked"), true);
    }
    C.Target->MarkPackageDirty(); Observe(R, C); VerifyAttached(R, C, AddedId, HasEnabled, HasEnabled && Enabled->AsBool());
    if (R.ErrorCode.IsEmpty() && R.bChangeStateKnown)
    {
        if (bRequestCompile)
        { R.Stage = TEXT("request_compile"); R.Data->SetBoolField(TEXT("compile_requested"), true); R.Data->SetBoolField(TEXT("compile_attempted"), true);
            R.Data->SetBoolField(TEXT("compile_request_returned"), C.Target->RequestCompile(false)); }
        if (bSave)
        {
            auto State = CompileState(C.Target); R.Data->SetObjectField(TEXT("save_compile_state"), State);
            if (!State->GetBoolField(TEXT("ready_to_save")))
            { R.Stage = TEXT("save_deferred"); R.Data->SetBoolField(TEXT("save_deferred"), true); Fail(R, TEXT("niagara_save_deferred_compile_pending"), TEXT("Attached/detached/localized work remains. Inspect readiness, then explicitly save the exact asset; do not replay attachment.")); }
            else
            { R.Stage = TEXT("save"); R.Data->SetBoolField(TEXT("save_attempted"), true); const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(C.Target, false);
                R.Data->SetBoolField(TEXT("saved"), Saved); if (!Saved || C.Target->GetOutermost()->IsDirty()) { Fail(R, TEXT("niagara_emitter_save_failed"), TEXT("Save returned false or package remains dirty; retained work is not retried.")); } }
        }
    }
    Observe(R, C); VerifyAttached(R, C, AddedId, HasEnabled, HasEnabled && Enabled->AsBool());
    R.bSuccess = R.bPostconditionPassed = R.bChangeStateKnown && R.ErrorCode.IsEmpty();
    if (R.bSuccess) { R.Stage = TEXT("complete"); } return R;
}
}
