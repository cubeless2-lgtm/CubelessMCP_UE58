// ADM source-only: niagara_tools.py8716 -> set_niagara_module_inputs_batch -> Part1.cpp266
// -> Part5.cpp2920 HandleSetNiagaraModuleInputsBatch. Original batch has no CPU-only admission.
// SDK NiagaraEmitter.cpp1588-1623 GetScripts(false,false) includes GPUComputeScript;
// NiagaraEmitter.h566 private UPROPERTY is read through public reflection, never direct access.
// NiagaraSystem.cpp2130-2164 HasOutstandingCompilationRequests(true) includes GPU shader readiness.
// Shared RI row order, continue-on-error, partial/unknown stop, compile/wait/save and no replay remain.
// Ordinary owned GPU authoring is implemented/usage validation pending. Stateless/external emitters,
// broader input coercions, GPU execution/shader correctness and cold persistence remain unverified.
#include "Compatibility/UE58/CubelessNiagaraModuleApi.h"
#include "Compatibility/UE58/CubelessNiagaraModuleInputsBatchAsyncApi.h"
#include "CubelessNiagaraModuleInputsBatchJob.h"
#include "Operations/CubelessVariableOperations.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformTime.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include <atomic>
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "NiagaraCommon.h"
#include "NiagaraParameterMapHistory.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ScopedTransaction.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraSystem.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::NiagaraModule
{
namespace
{
using Function::FResult;
constexpr int32 MaxNodes = 1024, MaxPins = 8192, MaxLinks = 32768, MaxEmitters = 128;
FString Path(const UObject* Value) { return Value ? Value->GetPathName() : FString(); }
FString Guid(const FGuid& Value) { return Value.ToString(EGuidFormats::DigitsWithHyphens); }
FString Text(const TSharedRef<FJsonObject>& Value)
{ FString Out; FJsonSerializer::Serialize(Value, TJsonWriterFactory<>::Create(&Out)); return Out; }
bool Loaded(const UObject* Value)
{
    return IsValid(Value) && !Value->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects |
        RF_BeginDestroyed | RF_FinishDestroyed | RF_ClassDefaultObject | RF_ArchetypeObject);
}
bool Fail(FResult& R, const TCHAR* Code, const TCHAR* Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
bool Bounded(const FString& Value, int32 Limit = 1024)
{ if (Value.Len() > Limit) { return false; } for (TCHAR C : Value) { if (C < 32 || C == 127) { return false; } } return true; }
FString Token(FString Value)
{ Value = Value.TrimStartAndEnd().ToLower(); Value.ReplaceInline(TEXT("_"), TEXT("")); Value.ReplaceInline(TEXT("-"), TEXT("")); Value.ReplaceInline(TEXT(" "), TEXT("")); return Value; }
bool Usage(const FString& Value, ENiagaraScriptUsage& Out)
{
    const FString N = Token(Value);
    if (N == TEXT("systemspawn") || N == TEXT("systemspawnscript")) { Out = ENiagaraScriptUsage::SystemSpawnScript; }
    else if (N == TEXT("systemupdate") || N == TEXT("systemupdatescript")) { Out = ENiagaraScriptUsage::SystemUpdateScript; }
    else if (N == TEXT("emitterspawn") || N == TEXT("emitterspawnscript")) { Out = ENiagaraScriptUsage::EmitterSpawnScript; }
    else if (N == TEXT("emitterupdate") || N == TEXT("emitterupdatescript")) { Out = ENiagaraScriptUsage::EmitterUpdateScript; }
    else if (N == TEXT("particlespawn") || N == TEXT("particlespawnscript")) { Out = ENiagaraScriptUsage::ParticleSpawnScript; }
    else if (N == TEXT("particlespawninterpolated") || N == TEXT("particlespawnscriptinterpolated")) { Out = ENiagaraScriptUsage::ParticleSpawnScriptInterpolated; }
    else if (N == TEXT("particleupdate") || N == TEXT("particleupdatescript")) { Out = ENiagaraScriptUsage::ParticleUpdateScript; }
    else if (N == TEXT("particleevent") || N == TEXT("particleeventscript")) { Out = ENiagaraScriptUsage::ParticleEventScript; }
    else if (N == TEXT("particlesimulationstage") || N == TEXT("particlesimulationstagescript") || N == TEXT("simulationstage")) { Out = ENiagaraScriptUsage::ParticleSimulationStageScript; }
    else { return false; }
    return true;
}
FString UsageName(ENiagaraScriptUsage Value)
{ return StaticEnum<ENiagaraScriptUsage>()->GetNameStringByValue(static_cast<int64>(Value)); }
bool Index(const FString& Json, int32& Out)
{
    if (Json.Len() > 64) { return false; }
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Values) || Values.Num() != 1 || !Values[0].IsValid()) { return false; }
    if (Values[0]->Type == EJson::Null) { Out = INDEX_NONE; return true; }
    double Number;
    if (Values[0]->Type != EJson::Number || !Values[0]->TryGetNumber(Number) || !FMath::IsFinite(Number) ||
        Number != FMath::TruncToDouble(Number) || Number < static_cast<double>(MIN_int32) || Number > static_cast<double>(MAX_int32)) { return false; }
    Out = static_cast<int32>(Number); return true;
}
bool ObjectPath(const FString& Input, FString& Out)
{
    if (!Bounded(Input)) { return false; } Out = Input.TrimStartAndEnd();
    if (!Out.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(Out))
    { Out += TEXT(".") + FPackageName::GetLongPackageAssetName(Out); }
    return FPackageName::IsValidObjectPath(Out);
}
TSharedRef<FJsonObject> CompileState(UNiagaraSystem* System)
{
    auto J = MakeShared<FJsonObject>();
    const bool Outstanding = System->HasOutstandingCompilationRequests(true), Needs = System->NeedsRequestCompile();
    J->SetBoolField(TEXT("outstanding_including_gpu"), Outstanding); J->SetBoolField(TEXT("needs_request_compile"), Needs);
    J->SetBoolField(TEXT("ready_to_run"), System->IsReadyToRun()); J->SetBoolField(TEXT("ready_to_save"), !Outstanding && !Needs);
    J->SetBoolField(TEXT("compile_success_verified"), false); J->SetBoolField(TEXT("poll_or_flush_invoked"), false); return J;
}
void Initialize(FResult& R, const FString& SystemPath, const FString& ModulePath, bool bSave = false)
{
    R.Data->SetStringField(TEXT("requested_system_path"), SystemPath); R.Data->SetStringField(TEXT("requested_module_script_path"), ModulePath);
    R.Data->SetBoolField(TEXT("asset_load_attempted"), false); R.Data->SetBoolField(TEXT("already_loaded_assets_required"), true); R.Data->SetNumberField(TEXT("asset_load_count"),0);R.Data->SetArrayField(TEXT("asset_load_calls"),{});
    R.Data->SetBoolField(TEXT("full_legacy_niagara_parity"), false); R.Data->SetBoolField(TEXT("compile_requested"), false);
    R.Data->SetBoolField(TEXT("compile_attempted"), false); R.Data->SetField(TEXT("compile_request_returned"), MakeShared<FJsonValueNull>());
    R.Data->SetBoolField(TEXT("save_requested"), bSave); R.Data->SetBoolField(TEXT("save_attempted"), false);
    R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetBoolField(TEXT("save_deferred"), false);
    R.Data->SetBoolField(TEXT("blocking_wait_invoked"), false); R.Data->SetBoolField(TEXT("atomic_rollback_available"), false);
    R.Data->SetBoolField(TEXT("auto_retry_safe"), false); R.Data->SetBoolField(TEXT("explicit_user_parameter_writes"), false);
    R.Data->SetBoolField(TEXT("implicit_graph_notification_callbacks_possible"), true);
    R.Data->SetBoolField(TEXT("recursive_asset_dependency_audit_performed"), false);
    R.Data->SetStringField(TEXT("engine_api"), TEXT("FNiagaraStackGraphUtilities::AddScriptModuleToStack(UNiagaraScript*,UNiagaraNodeOutput&,int32,FString,const FGuid&)"));
    R.Data->SetStringField(TEXT("selector_policy"), TEXT("unique loaded emitter and equivalent usage/id output; ambiguous legacy first-match selectors rejected"));
}
FString PinId(const UEdGraphPin* Pin)
{ return Pin ? Path(Pin->GetOwningNode()) + TEXT(".") + Pin->PinName.ToString() + TEXT("/") + Guid(Pin->PinId) : FString(); }
TSharedRef<FJsonObject> PinMetadata(const UEdGraphPin* Pin)
{
    auto J = MakeShared<FJsonObject>(); const auto& T = Pin->PinType;
    J->SetStringField(TEXT("pin_id"), PinId(Pin)); J->SetStringField(TEXT("pin_guid"), Guid(Pin->PinId));
    J->SetStringField(TEXT("name"), Pin->PinName.ToString()); J->SetNumberField(TEXT("direction"), static_cast<int32>(Pin->Direction));
    J->SetStringField(TEXT("category"), T.PinCategory.ToString()); J->SetStringField(TEXT("subcategory"), T.PinSubCategory.ToString());
    J->SetStringField(TEXT("subcategory_object_path"), Path(T.PinSubCategoryObject.Get()));
    J->SetNumberField(TEXT("container_type"), static_cast<int32>(T.ContainerType)); J->SetBoolField(TEXT("is_reference"), T.bIsReference);
    J->SetBoolField(TEXT("is_const"), T.bIsConst); J->SetBoolField(TEXT("is_weak_pointer"), T.bIsWeakPointer);
    J->SetStringField(TEXT("terminal_category"), T.PinValueType.TerminalCategory.ToString());
    J->SetStringField(TEXT("terminal_subcategory"), T.PinValueType.TerminalSubCategory.ToString());
    J->SetStringField(TEXT("terminal_subcategory_object_path"), Path(T.PinValueType.TerminalSubCategoryObject.Get()));
    J->SetBoolField(TEXT("terminal_is_const"), T.PinValueType.bTerminalIsConst); J->SetBoolField(TEXT("terminal_is_weak_pointer"), T.PinValueType.bTerminalIsWeakPointer);
    J->SetStringField(TEXT("raw_default_value"), Pin->DefaultValue); J->SetStringField(TEXT("autogenerated_default_value"), Pin->AutogeneratedDefaultValue);
    J->SetStringField(TEXT("default_object_path"), Path(Pin->DefaultObject)); J->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
    J->SetBoolField(TEXT("default_text_empty"), Pin->DefaultTextValue.IsEmpty()); J->SetBoolField(TEXT("hidden"), Pin->bHidden);
    J->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin); J->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable);
    J->SetBoolField(TEXT("default_value_is_ignored"), Pin->bDefaultValueIsIgnored); J->SetBoolField(TEXT("default_value_is_readonly"), Pin->bDefaultValueIsReadOnly);
    return J;
}
TSharedRef<FJsonObject> NodeMetadata(const UEdGraphNode* Node)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("node_path"), Path(Node)); J->SetStringField(TEXT("node_name"), Node->GetName());
    J->SetStringField(TEXT("node_class"), Path(Node->GetClass())); J->SetStringField(TEXT("node_guid"), Guid(Node->NodeGuid));
    J->SetNumberField(TEXT("position_x"), Node->NodePosX); J->SetNumberField(TEXT("position_y"), Node->NodePosY);
    if (const auto* Call = Cast<UNiagaraNodeFunctionCall>(Node))
    {
        J->SetStringField(TEXT("function_script_path"), Path(Call->FunctionScript)); J->SetStringField(TEXT("selected_script_version"), Guid(Call->SelectedScriptVersion));
        J->SetStringField(TEXT("function_name"), Call->GetFunctionName()); J->SetStringField(TEXT("function_script_asset_object_path"), Call->FunctionScriptAssetObjectPath.ToString());
        J->SetNumberField(TEXT("node_index"), INDEX_NONE); J->SetStringField(TEXT("function_script"), Path(Call->FunctionScript));
        J->SetStringField(TEXT("script_usage"), Call->FunctionScript ? UsageName(Call->FunctionScript->GetUsage()) : FString());
        J->SetStringField(TEXT("called_usage"), UsageName(Call->GetCalledUsage()));
        const bool Scratch = Call->FunctionScript && Call->FunctionScript->GetOuter() && Call->FunctionScript->GetOuter()->GetClass()->GetName() == TEXT("NiagaraScratchPadContainer");
        J->SetBoolField(TEXT("is_scratch_pad"), Scratch); J->SetStringField(TEXT("source_kind"), Scratch ? TEXT("scratch_pad") : (Call->FunctionScript ? TEXT("script_asset") : TEXT("signature_only")));
        auto Variable = [](const FNiagaraVariableBase& V)
        {
            auto Row = MakeShared<FJsonObject>(); const auto& T = V.GetType(); Row->SetStringField(TEXT("name"), V.GetName().ToString());
            Row->SetStringField(TEXT("type"), T.GetNameText().ToString()); Row->SetBoolField(TEXT("type_is_valid"), T.IsValid());
            Row->SetBoolField(TEXT("type_is_data_interface"), T.IsDataInterface()); Row->SetBoolField(TEXT("type_is_uobject"), T.IsUObject());
            if (UClass* Class = T.GetClass()) { Row->SetStringField(TEXT("type_class"), Class->GetName()); Row->SetStringField(TEXT("type_class_path"), Path(Class)); }
            return MakeShared<FJsonValueObject>(Row);
        };
        TArray<TSharedPtr<FJsonValue>> Inputs, Outputs, Specifiers; FString Search = Call->GetFunctionName() + TEXT(" ") + Path(Call->FunctionScript);
        for (const auto& V : Call->Signature.Inputs) { Inputs.Add(Variable(V)); Search += TEXT(" ") + V.GetName().ToString(); }
        for (const auto& V : Call->Signature.Outputs) { Outputs.Add(Variable(V)); Search += TEXT(" ") + V.GetName().ToString(); }
        J->SetArrayField(TEXT("signature_inputs"), Inputs); J->SetArrayField(TEXT("signature_outputs"), Outputs);
        TArray<FString> SpecifierText; for (const auto& V : Call->FunctionSpecifiers) { SpecifierText.AddUnique(V.Key.ToString() + TEXT("=") + V.Value.ToString()); }
        SpecifierText.Sort(); for (const auto& V : SpecifierText) { Specifiers.Add(MakeShared<FJsonValueString>(V)); } J->SetArrayField(TEXT("function_specifiers"), Specifiers);
        const TPair<const TCHAR*, const TCHAR*> HintTokens[] = {
            {TEXT("color"),TEXT("color_or_tint_control")},{TEXT("colour"),TEXT("color_or_tint_control")},{TEXT("tint"),TEXT("color_or_tint_control")},
            {TEXT("ribbonwidth"),TEXT("ribbon_width_control")},{TEXT("ribbon width"),TEXT("ribbon_width_control")},{TEXT("width"),TEXT("width_control")},
            {TEXT("scale"),TEXT("scale_control")},{TEXT("size"),TEXT("size_control")},{TEXT("lifetime"),TEXT("lifetime_control")},{TEXT("life time"),TEXT("lifetime_control")},
            {TEXT("duration"),TEXT("duration_control")},{TEXT("velocity"),TEXT("velocity_control")},{TEXT("speed"),TEXT("velocity_control")},
            {TEXT("spawn"),TEXT("spawn_control")},{TEXT("materialparam"),TEXT("dynamic_material_parameter_control")},{TEXT("dynamicmaterial"),TEXT("dynamic_material_parameter_control")},
            {TEXT("user."),TEXT("user_parameter_reference")},{TEXT("scratch"),TEXT("scratch_pad_reference")},{TEXT("trail"),TEXT("trail_behavior")},{TEXT("ribbon"),TEXT("ribbon_behavior")}};
        TArray<FString> HintText; for (const auto& Hint : HintTokens) { if (Search.Contains(Hint.Key, ESearchCase::IgnoreCase)) { HintText.AddUnique(Hint.Value); } }
        TArray<TSharedPtr<FJsonValue>> Hints; for (const auto& Hint : HintText) { Hints.Add(MakeShared<FJsonValueString>(Hint)); } J->SetArrayField(TEXT("control_hints"), Hints);
    }
    if (const auto* Output = Cast<UNiagaraNodeOutput>(Node))
    { J->SetStringField(TEXT("usage"), UsageName(Output->GetUsage())); J->SetStringField(TEXT("usage_id"), Guid(Output->GetUsageId())); }
    return J;
}
struct FSnapshot
{
    bool bComplete = false; TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    TMap<FString, FString> Nodes, Pins; TMap<FString, TArray<FString>> Links;
};
FSnapshot Capture(UNiagaraGraph* Graph)
{
    FSnapshot S; S.Json->SetStringField(TEXT("graph_path"), Path(Graph));
    if (!Loaded(Graph) || Graph->Nodes.Num() > MaxNodes) { return S; }
    S.Json->SetStringField(TEXT("graph_change_id"), Guid(Graph->GetChangeID()));
    TSet<const UEdGraphNode*> Nodes; TSet<const UEdGraphPin*> Pins; int32 LinkCount = 0;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Loaded(Node) || Node->GetOuter() != Graph || !Node->GetClass()->HasAllClassFlags(CLASS_Native) || Nodes.Contains(Node) ||
            Node->Pins.Num() > MaxPins || Pins.Num() > MaxPins - Node->Pins.Num()) { return S; }
        Nodes.Add(Node);
        for (const UEdGraphPin* Pin : Node->Pins)
        { if (!Pin || Pin->GetOwningNode() != Node || Pins.Contains(Pin) || !Pin->PinId.IsValid()) { return S; } Pins.Add(Pin); }
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        auto Row = NodeMetadata(Node); const FString N = Path(Node); S.Nodes.Add(N, Text(Row));
        TArray<TSharedPtr<FJsonValue>> PinRows;
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->LinkedTo.Num() > MaxLinks || LinkCount > MaxLinks - Pin->LinkedTo.Num()) { return S; }
            LinkCount += Pin->LinkedTo.Num(); TSet<const UEdGraphPin*> Seen; TArray<FString> Links;
            for (const UEdGraphPin* Linked : Pin->LinkedTo)
            {
                if (!Linked || !Pins.Contains(Linked) || Seen.Contains(Linked) || Linked->Direction == Pin->Direction || !Linked->LinkedTo.Contains(Pin)) { return S; }
                Seen.Add(Linked); Links.Add(PinId(Linked));
            }
            Links.Sort(); const FString P = PinId(Pin); if (S.Pins.Contains(P)) { return S; }
            auto PinRow = PinMetadata(Pin); S.Pins.Add(P, Text(PinRow)); S.Links.Add(P, Links);
            TArray<TSharedPtr<FJsonValue>> LinkRows; for (const auto& Link : Links) { LinkRows.Add(MakeShared<FJsonValueString>(Link)); }
            PinRow->SetArrayField(TEXT("links"), LinkRows); PinRows.Add(MakeShared<FJsonValueObject>(PinRow));
        }
        Row->SetArrayField(TEXT("pins"), PinRows); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    S.Json->SetArrayField(TEXT("nodes"), Rows); S.Json->SetNumberField(TEXT("node_count"), S.Nodes.Num());
    S.Json->SetNumberField(TEXT("pin_count"), S.Pins.Num()); S.Json->SetNumberField(TEXT("physical_link_count"), LinkCount / 2);
    S.Json->SetBoolField(TEXT("observation_complete"), true); S.bComplete = true; return S;
}

// These public pin/type observations mirror only the map-chain read algorithm.
// The installed stack helpers that read module order/groups are not exported.
bool MapPin(UEdGraphNode* Node, EEdGraphPinDirection Direction, UEdGraphPin*& Out)
{
    Out = nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == Direction && !Pin->bOrphanedPin &&
            Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType &&
            Pin->PinType.PinSubCategoryObject.Get() == FNiagaraTypeDefinition::GetParameterMapDef().GetScriptStruct() &&
            UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
        { if (Out) { return false; } Out = Pin; }
    }
    return true;
}
// RO: original Add primary target -> module loading. No load in Inspect/internal observers.
struct FAddAssetRole
{
    TStrongObjectPtr<UObject> Object;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;
    FString Path;bool Ready=false;
};
struct FAddLoadScope
{
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    FAddAssetRole System,Module;
};
bool ModulePrimaryLoadActive=false;
bool AddLoadStop(FResult& R,const TCHAR* Code,const TCHAR* Why)
{R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;R.Data->SetBoolField(TEXT("primary_load_continuation_safe"),false);return Fail(R,Code,Why);}
bool AddLoadContext(const FAddLoadScope& L)
{
    return IsEditorGameThread()&&Loaded(L.Editor.Get())&&GEditor==L.Editor.Get()&&!IsPlaySessionActive()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&Loaded(L.World.Get())&&GEditor->GetEditorWorldContext().World()==L.World.Get()&&L.World->WorldType==EWorldType::Editor&&!L.World->bIsTearingDown;
}
bool AddAssetCurrent(const FAddAssetRole& A)
{
    if(!A.Ready)return true;
    return Loaded(A.Object.Get())&&Loaded(A.Class.Get())&&Loaded(A.Package.Get())&&A.Object->GetClass()==A.Class.Get()&&A.Object->GetOuter()==A.Package.Get()&&A.Object->GetPackage()==A.Package.Get()&&A.Object->GetPathName().Equals(A.Path,ESearchCase::IgnoreCase)&&FindObject<UObject>(nullptr,*A.Path)==A.Object.Get();
}
bool AddLoadsCurrent(FResult& R,const FAddLoadScope& L)
{
    if(!AddLoadContext(L)||!AddAssetCurrent(L.System)||!AddAssetCurrent(L.Module))return AddLoadStop(R,TEXT("niagara_primary_load_current_lost"),TEXT("Held Add Editor/world/primary asset identity changed; actual loader prefix retained, no next SDK or retry."));return true;
}
bool AddAcquire(FResult& R,FAddLoadScope& L,FAddAssetRole& Role,const FString& ObjectPath,bool IsSystem)
{
    if(!AddLoadsCurrent(R,L))return false;Role.Path=ObjectPath;
    UObject* Found=IsSystem?static_cast<UObject*>(FindObject<UNiagaraSystem>(nullptr,*ObjectPath)):static_cast<UObject*>(FindObject<UNiagaraScript>(nullptr,*ObjectPath));Role.Object.Reset(Found);
    if(!Found)
    {
        if(ModulePrimaryLoadActive)return Fail(R,TEXT("niagara_primary_load_reentry"),TEXT("Another primary loader is active; no nested load or replay."));
        auto Row=MakeShared<FJsonObject>();auto Calls=R.Data->GetArrayField(TEXT("asset_load_calls"));Calls.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("asset_load_calls"),Calls);R.Data->SetNumberField(TEXT("asset_load_count"),Calls.Num());R.Data->SetBoolField(TEXT("asset_load_attempted"),true);
        Row->SetStringField(TEXT("role"),IsSystem?TEXT("target_system"):TEXT("module_script"));Row->SetStringField(TEXT("route"),IsSystem?TEXT("LoadObject<UNiagaraSystem>"):TEXT("LoadObject<UNiagaraScript>"));Row->SetStringField(TEXT("requested_object_path"),ObjectPath);Row->SetBoolField(TEXT("attempted"),true);Row->SetBoolField(TEXT("returned"),false);R.Data->SetBoolField(TEXT("loader_full_effect_state_known"),false);R.bChangeStateKnown=false;
        TGuardValue<bool> Loading(ModulePrimaryLoadActive,true);
        UObject* Returned=IsSystem?static_cast<UObject*>(LoadObject<UNiagaraSystem>(nullptr,*ObjectPath)):static_cast<UObject*>(LoadObject<UNiagaraScript>(nullptr,*ObjectPath));Role.Object.Reset(Returned);
        Row->SetBoolField(TEXT("returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),Returned!=nullptr);
        if(!AddLoadsCurrent(R,L))return false;
        if(!Returned){R.bChangeStateKnown=true;return Fail(R,IsSystem?TEXT("niagara_system_load_failed"):TEXT("niagara_module_load_failed"),IsSystem?TEXT("Original target system load returned null; no module load, writer or retry."):TEXT("Original module load returned null; earlier target load prefix retained, no writer or retry."));}
    }
    if(!Loaded(Role.Object.Get()))return AddLoadStop(R,TEXT("niagara_primary_loaded_object_unavailable"),TEXT("Primary asset exists but is incomplete/retired; no automatic reload or getter."));
    Role.Class.Reset(Role.Object->GetClass());if(!Loaded(Role.Class.Get()))return AddLoadStop(R,TEXT("niagara_primary_loaded_class_unavailable"),TEXT("Returned asset class is unavailable before classification."));
    TStrongObjectPtr<UObject> Outer(Role.Object->GetOuter());if(!Loaded(Outer.Get())||!Loaded(Outer->GetClass()))return AddLoadStop(R,TEXT("niagara_primary_loaded_outer_unavailable"),TEXT("Returned asset outer unavailable before package read."));
    Role.Package.Reset(Cast<UPackage>(Outer.Get()));
    if(!Loaded(Role.Package.Get())||Role.Object->GetPackage()!=Role.Package.Get()||!Role.Object->GetPathName().Equals(ObjectPath,ESearchCase::IgnoreCase)||FindObject<UObject>(nullptr,*ObjectPath)!=Role.Object.Get())return AddLoadStop(R,TEXT("niagara_primary_loaded_identity_changed"),TEXT("Primary loader must return the exact requested current top-level asset; no redirect substitution or retry."));
    UClass* Expected=IsSystem?UNiagaraSystem::StaticClass():UNiagaraScript::StaticClass();if(Role.Class.Get()!=Expected){R.bChangeStateKnown=true;return Fail(R,IsSystem?TEXT("niagara_loaded_system_required"):TEXT("niagara_loaded_module_required"),TEXT("Primary returned asset must retain the existing exact native System/Script class admission."));}
    Role.Ready=true;if(!AddLoadsCurrent(R,L))return false;R.bChangeStateKnown=true;return true;
}
bool AddPrimaryAssets(FResult& R,FAddLoadScope& L,const FString& SystemObject,const FString& ModuleObject)
{
    if(!Loaded(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())return Fail(R,TEXT("niagara_primary_load_editor_required"),TEXT("Original Add loader requires idle Editor outside exit/GC/save."));
    L.Editor.Reset(GEditor);L.World.Reset(GEditor->GetEditorWorldContext().World());
    R.Data->SetNumberField(TEXT("asset_load_count"),0);R.Data->SetArrayField(TEXT("asset_load_calls"),{});R.Data->SetBoolField(TEXT("already_loaded_assets_required"),false);R.Data->SetBoolField(TEXT("primary_load_continuation_safe"),false);
    R.Data->SetStringField(TEXT("asset_loading_scope"),TEXT("Original Add primary target then module, loaded-fast0 or one conditional LoadObject each. Source profiles are captured after loader callbacks; global/dependency/PostLoad/cache effects are not proven unchanged. Inspect/internal observers remain find-only; no warm, reload, runtime or retry."));
    if(!AddAcquire(R,L,L.System,SystemObject,true)||!AddAcquire(R,L,L.Module,ModuleObject,false)||!AddLoadsCurrent(R,L))return false;
    R.Data->SetBoolField(TEXT("primary_load_continuation_safe"),true);return true;
}

struct FContext
{
    UNiagaraSystem* System = nullptr; UNiagaraScript* Module = nullptr; UNiagaraGraph* Graph = nullptr;
    UNiagaraScriptSource* ModuleSource = nullptr; UNiagaraGraph* ModuleGraph = nullptr;
    UNiagaraNodeOutput* Output = nullptr; UNiagaraNodeInput* Input = nullptr;
    FAddLoadScope PrimaryLoads;
    TArray<UNiagaraNodeFunctionCall*> Modules; FSnapshot Before, ModuleBefore;
    ENiagaraScriptUsage TargetUsage = ENiagaraScriptUsage::ParticleSpawnScriptInterpolated;
    FGuid UsageId, ModuleVersion; int32 EmitterIndex = INDEX_NONE;
    bool bSystemDirty = false, bModuleDirty = false; FString OwnerKind, OwnerName;
};
bool Stack(FResult& R, FContext& C)
{
    C.Modules.Empty(); C.Input = nullptr; UEdGraphNode* Current = C.Output; TSet<UEdGraphNode*> Visited;
    while (Current && Visited.Num() < MaxNodes)
    {
        if (!Loaded(Current) || Current->GetOuter() != C.Graph || Visited.Contains(Current) || !Cast<UNiagaraNode>(Current))
        { return Fail(R, TEXT("niagara_stack_chain_invalid"), TEXT("Selected map chain is cyclic, foreign or incomplete; no insertion dispatched.")); }
        Visited.Add(Current);
        if (auto* Call = Cast<UNiagaraNodeFunctionCall>(Current)) { C.Modules.Insert(Call, 0); }
        UEdGraphPin* In = nullptr; UEdGraphPin* Out = nullptr;
        if (!MapPin(Current, EGPD_Input, In) || !MapPin(Current, EGPD_Output, Out))
        { return Fail(R, TEXT("niagara_stack_map_pin_ambiguous"), TEXT("Map-chain nodes must have at most one non-orphan map pin per direction.")); }
        if (auto* Input = Cast<UNiagaraNodeInput>(Current))
        {
            if (In || !Out || Input->Input.GetType() != FNiagaraTypeDefinition::GetParameterMapDef())
            { return Fail(R, TEXT("niagara_stack_input_invalid"), TEXT("Selected chain does not end at an exact parameter-map input.")); }
            C.Input = Input; break;
        }
        if (!In || In->LinkedTo.Num() != 1 || !In->LinkedTo[0] || In->LinkedTo[0]->Direction != EGPD_Output)
        { return Fail(R, TEXT("niagara_stack_input_link_invalid"), TEXT("Selected map chain requires one reciprocal upstream map link per node.")); }
        UEdGraphPin* Source = In->LinkedTo[0]; UEdGraphNode* SourceNode = Source->GetOwningNode(); UEdGraphPin* SourceMap = nullptr;
        if (!Loaded(SourceNode) || SourceNode->GetOuter() != C.Graph || !MapPin(SourceNode, EGPD_Output, SourceMap) || SourceMap != Source || !Source->LinkedTo.Contains(In))
        { return Fail(R, TEXT("niagara_stack_upstream_invalid"), TEXT("Upstream pin is not the exact owned reciprocal parameter-map output.")); }
        Current = SourceNode;
    }
    if (!C.Input || C.Modules.Num() > MaxNodes - 2)
    { return Fail(R, TEXT("niagara_stack_input_missing"), TEXT("Bounded selected stack lacks a valid parameter-map input/output pair.")); }
    // The native grouping routine dereferences every start-node map input.
    // Validate all linked starts of each group boundary, including overrides.
    TArray<UEdGraphNode*> Boundaries; Boundaries.Add(C.Input); for (auto* M : C.Modules) { Boundaries.Add(M); }
    for (UEdGraphNode* Boundary : Boundaries)
    {
        UEdGraphPin* Out = nullptr;
        if (!MapPin(Boundary, EGPD_Output, Out) || !Out || Out->LinkedTo.IsEmpty())
        { return Fail(R, TEXT("niagara_stack_boundary_invalid"), TEXT("A selected stack boundary has no parameter-map output/start group.")); }
        for (UEdGraphPin* Target : Out->LinkedTo)
        {
            UEdGraphNode* Node = Target ? Target->GetOwningNode() : nullptr; UEdGraphPin* In = nullptr;
            if (!Loaded(Node) || !Cast<UNiagaraNode>(Node) || Node->GetOuter() != C.Graph ||
                !MapPin(Node, EGPD_Input, In) || !In || In != Target || !In->LinkedTo.Contains(Out))
            { return Fail(R, TEXT("niagara_stack_group_invalid"), TEXT("A native stack start group contains a missing/foreign/non-map node; no unsafe grouping call dispatched.")); }
        }
    }
    return true;
}
TArray<TSharedPtr<FJsonValue>> ModuleRows(const FContext& C)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 I = 0; I < C.Modules.Num(); ++I)
    { auto Row = NodeMetadata(C.Modules[I]); Row->SetNumberField(TEXT("module_index"), I); Rows.Add(MakeShared<FJsonValueObject>(Row)); }
    return Rows;
}

bool Prepare(FResult& R, FContext& C, const FString& SystemPath, const FString& ModulePath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& EmitterIndexJson,
    const FString& EmitterName, bool bWrite, bool bAllowSourceEdit, bool bPrimaryLoad = false)
{
    const FEngineVersion& V = FEngineVersion::Current();
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("niagara_editor_state_invalid"), TEXT("Requires Editor game thread outside PIE/SIE.")); }
    if (V.GetMajor() != 5 || V.GetMinor() != 8 || V.GetPatch() != 3) { return Fail(R, TEXT("niagara_version_unsupported"), TEXT("Only UE 5.8.3 is implemented.")); }
    int32 RequestedEmitterIndex; FString SystemObject, ModuleObject;
    if (!Usage(TargetUsage, C.TargetUsage)) { return Fail(R, TEXT("niagara_usage_unsupported"), TEXT("Use one of the nine legacy system/emitter/particle stack usages.")); }
    if (!Bounded(TargetUsageId) || (!TargetUsageId.IsEmpty() && !FGuid::Parse(TargetUsageId, C.UsageId)))
    { return Fail(R, TEXT("niagara_usage_id_invalid"), TEXT("target_usage_id must be empty or a valid GUID.")); }
    if (!Index(EmitterIndexJson, RequestedEmitterIndex) || RequestedEmitterIndex < INDEX_NONE || !Bounded(EmitterName, 256))
    { return Fail(R, TEXT("niagara_emitter_selector_invalid"), TEXT("Emitter index must be JSON null, -1 or a nonnegative int32; name must be bounded.")); }
    if (!ObjectPath(SystemPath, SystemObject) || !ObjectPath(ModulePath, ModuleObject))
    { return Fail(R, TEXT("niagara_asset_path_invalid"), TEXT("Use precise system/module asset package or object paths.")); }
    if(bPrimaryLoad&&(!ModuleObject.StartsWith(TEXT("/Niagara/"))&&!ModuleObject.StartsWith(TEXT("/Engine/"))&&!ModuleObject.StartsWith(TEXT("/Game/"))))return Fail(R,TEXT("niagara_loaded_module_required"),TEXT("Existing external native Module mount admission is unchanged; no loader dispatched for an excluded mount."));
    if(bPrimaryLoad)
    {if(!AddPrimaryAssets(R,C.PrimaryLoads,SystemObject,ModuleObject))return false;C.System=static_cast<UNiagaraSystem*>(C.PrimaryLoads.System.Object.Get());C.Module=static_cast<UNiagaraScript*>(C.PrimaryLoads.Module.Object.Get());}
    else
    {C.System = FindObject<UNiagaraSystem>(nullptr, *SystemObject); C.Module = FindObject<UNiagaraScript>(nullptr, *ModuleObject);}
    if (!Loaded(C.System) || C.System->GetClass() != UNiagaraSystem::StaticClass() || !C.System->IsAsset() ||
        C.System->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn))
    { return Fail(R, TEXT("niagara_loaded_system_required"), TEXT("Load the exact native NiagaraSystem with official tools before this find-only operation.")); }
    const FString Package = C.System->GetOutermost()->GetName();
    const bool Temporary = Package.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/")) || Package.StartsWith(TEXT("/Game/CubelessMCPTests/"));
    if (bWrite && (!Package.StartsWith(TEXT("/Game/")) || (!Temporary && !bAllowSourceEdit)))
    { return Fail(R, TEXT("niagara_write_scope_forbidden"), TEXT("Writes require the generated/test roots or explicit source edit on /Game.")); }
    if (!Loaded(C.Module) || C.Module->GetClass() != UNiagaraScript::StaticClass() || !C.Module->IsAsset() || C.Module->GetUsage() != ENiagaraScriptUsage::Module ||
        C.Module->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor | PKG_CompiledIn) ||
        (!ModuleObject.StartsWith(TEXT("/Niagara/")) && !ModuleObject.StartsWith(TEXT("/Engine/")) && !ModuleObject.StartsWith(TEXT("/Game/"))))
    { return Fail(R, TEXT("niagara_loaded_module_required"), TEXT("Load an exact native external Niagara Module asset from /Niagara, /Engine or /Game; inline scratch scripts and Cubeless mounts are excluded.")); }
    if (C.System->GetEmitterHandles().Num() > MaxEmitters)
    { return Fail(R, TEXT("niagara_emitter_bound_exceeded"), TEXT("Finite selector observation is bounded to 128 emitter handles.")); }
    if (bWrite)
    {
        TArray<TSharedPtr<FJsonValue>> Instances;
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        { auto* Component = *It; if (IsValid(Component) && !Component->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && Component->GetAsset() == C.System && Component->GetWorld())
            { Instances.Add(MakeShared<FJsonValueString>(Path(Component))); if (Instances.Num() > 32) { break; } } }
        R.Data->SetArrayField(TEXT("loaded_world_component_references"), Instances);
        if (!Instances.IsEmpty()) { return Fail(R, TEXT("niagara_system_has_world_instances"), TEXT("Stack authoring refuses systems with live world components; native notifications/compile may update them.")); }
        if (C.System->HasOutstandingCompilationRequests(true))
        { return Fail(R, TEXT("niagara_compile_outstanding"), TEXT("Wait via read-only status inspection before editing a system with outstanding compilation; no wait/poll is executed here.")); }
    }
    C.ModuleVersion = C.Module->IsVersioningEnabled() ? C.Module->GetExposedVersion().VersionGuid : FGuid();
    const FVersionedNiagaraScriptData* ModuleData = C.Module->GetScriptData(C.ModuleVersion);
    if (!ModuleData || !UNiagaraScript::IsSupportedUsageContextForBitmask(ModuleData->ModuleUsageBitmask, C.TargetUsage))
    { return Fail(R, TEXT("niagara_module_usage_incompatible"), TEXT("The exact module version selected by the stock helper does not advertise the selected usage.")); }
    C.ModuleSource = Cast<UNiagaraScriptSource>(C.Module->GetSource(C.ModuleVersion)); C.ModuleGraph = C.ModuleSource ? C.ModuleSource->NodeGraph.Get() : nullptr;
    if (!Loaded(C.ModuleSource) || !Loaded(C.ModuleGraph) || C.ModuleGraph->GetClass() != UNiagaraGraph::StaticClass() ||
        C.ModuleGraph->GetOutermost() != C.Module->GetOutermost() || !C.ModuleSource->IsIn(C.Module) || C.ModuleSource->bIsCompilationCopy || C.ModuleSource->bIsReleased)
    { return Fail(R, TEXT("niagara_module_source_invalid"), TEXT("The already-loaded exact selected module version must own an ordinary native editor graph.")); }
    UNiagaraScriptSource* TargetSource = nullptr;
    if (C.TargetUsage == ENiagaraScriptUsage::SystemSpawnScript || C.TargetUsage == ENiagaraScriptUsage::SystemUpdateScript)
    {
        UNiagaraScript* Script = C.TargetUsage == ENiagaraScriptUsage::SystemSpawnScript ? C.System->GetSystemSpawnScript() : C.System->GetSystemUpdateScript();
        if (Loaded(Script)) { TargetSource = Cast<UNiagaraScriptSource>(Script->GetLatestSource()); }
        C.OwnerKind = TEXT("system"); C.OwnerName = C.System->GetName();
    }
    else
    {
        int32 Matches = 0;
        for (int32 I = 0; I < C.System->GetEmitterHandles().Num(); ++I)
        {
            const auto& H = C.System->GetEmitterHandles()[I]; const FString Name = H.GetName().ToString();
            if ((RequestedEmitterIndex == INDEX_NONE || RequestedEmitterIndex == I) && (EmitterName.IsEmpty() || EmitterName.Equals(Name, ESearchCase::IgnoreCase)))
            {
                ++Matches; C.EmitterIndex = I; C.OwnerName = Name;
                if (const auto* Data = H.GetEmitterData()) { TargetSource = Cast<UNiagaraScriptSource>(Data->GraphSource); }
            }
        }
        if (Matches != 1) { return Fail(R, TEXT("niagara_emitter_selector_not_unique"), TEXT("The combined index/name selector must resolve exactly one existing emitter; no first-match fallback or emitter creation.")); }
        C.OwnerKind = TEXT("emitter");
    }
    C.Graph = TargetSource ? TargetSource->NodeGraph.Get() : nullptr;
    if (!Loaded(TargetSource) || !Loaded(C.Graph) || C.Graph->GetClass() != UNiagaraGraph::StaticClass() ||
        !TargetSource->IsIn(C.System) || C.Graph->GetOutermost() != C.System->GetOutermost() ||
        TargetSource->bIsCompilationCopy || TargetSource->bIsReleased || C.Graph == C.ModuleGraph)
    { return Fail(R, TEXT("niagara_stack_graph_invalid"), TEXT("The selected existing native stack graph must be fully loaded and owned within this exact system.")); }
    if (C.Graph->Nodes.Num() > MaxNodes || C.ModuleGraph->Nodes.Num() > MaxNodes)
    { return Fail(R, TEXT("niagara_graph_bound_exceeded"), TEXT("Selected target and source graph observations are each bounded to 1024 nodes.")); }
    UClass* SchemaClass = FindObject<UClass>(nullptr, TEXT("/Script/NiagaraEditor.EdGraphSchema_Niagara"));
    if (!IsValid(SchemaClass) || SchemaClass != UEdGraphSchema_Niagara::StaticClass() || !IsValid(SchemaClass->GetDefaultObject(false)) ||
        C.Graph->Schema.Get() != SchemaClass || C.ModuleGraph->Schema.Get() != SchemaClass)
    { return Fail(R, TEXT("niagara_schema_not_loaded"), TEXT("Exact Niagara schema/default must already be loaded; no default object creation or schema fallback.")); }
    int32 Outputs = 0;
    for (UEdGraphNode* Node : C.Graph->Nodes)
    { if (auto* Output = Cast<UNiagaraNodeOutput>(Node); Output && UNiagaraScript::IsEquivalentUsage(Output->GetUsage(), C.TargetUsage) && Output->GetUsageId() == C.UsageId)
        { C.Output = Output; ++Outputs; } }
    if (Outputs != 1 || C.Graph->FindEquivalentOutputNode(C.TargetUsage, C.UsageId) != C.Output)
    { return Fail(R, TEXT("niagara_output_selector_not_unique"), TEXT("Exactly one existing output must match equivalent usage and exact usage GUID; no output creation.")); }
    C.Before = Capture(C.Graph); C.ModuleBefore = Capture(C.ModuleGraph);
    if (!C.Before.bComplete || !C.ModuleBefore.bComplete || !Stack(R, C))
    { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("niagara_graph_observation_incomplete"), TEXT("Bounded native graph/pin/link observation is incomplete; no insertion dispatched.")); } return false; }
    C.bSystemDirty = C.System->GetOutermost()->IsDirty(); C.bModuleDirty = C.Module->GetOutermost()->IsDirty();
    R.Data->SetStringField(TEXT("system_path"), Path(C.System)); R.Data->SetStringField(TEXT("target_system_path"), Path(C.System));
    R.Data->SetStringField(TEXT("module_script_path"), Path(C.Module)); R.Data->SetStringField(TEXT("module_script_name"), C.Module->GetName());
    R.Data->SetStringField(TEXT("module_script_version"), Guid(C.ModuleVersion)); R.Data->SetNumberField(TEXT("module_usage_bitmask"), ModuleData->ModuleUsageBitmask);
    R.Data->SetStringField(TEXT("module_source_path"), Path(C.ModuleSource)); R.Data->SetStringField(TEXT("module_graph_path"), Path(C.ModuleGraph));
    R.Data->SetStringField(TEXT("target_usage"), UsageName(C.TargetUsage)); R.Data->SetStringField(TEXT("target_usage_id"), Guid(C.UsageId));
    R.Data->SetStringField(TEXT("requested_target_usage"), TargetUsage); R.Data->SetStringField(TEXT("target_emitter_index_json"), EmitterIndexJson);
    R.Data->SetStringField(TEXT("requested_target_emitter_name"), EmitterName); R.Data->SetNumberField(TEXT("target_emitter_index"), C.EmitterIndex);
    R.Data->SetStringField(TEXT("stack_owner_kind"), C.OwnerKind); R.Data->SetStringField(TEXT("stack_owner_name"), C.OwnerName);
    R.Data->SetStringField(TEXT("graph_path"), Path(C.Graph)); R.Data->SetStringField(TEXT("output_node_path"), Path(C.Output));
    R.Data->SetStringField(TEXT("output_node_guid"), Guid(C.Output->NodeGuid)); R.Data->SetStringField(TEXT("input_node_path"), Path(C.Input));
    R.Data->SetObjectField(TEXT("before"), C.Before.Json); R.Data->SetObjectField(TEXT("module_source_before"), C.ModuleBefore.Json);
    R.Data->SetArrayField(TEXT("ordered_modules"), ModuleRows(C)); R.Data->SetNumberField(TEXT("module_count"), C.Modules.Num());
    R.Data->SetNumberField(TEXT("graph_node_count_before"), C.Before.Nodes.Num()); R.Data->SetBoolField(TEXT("package_dirty_before"), C.bSystemDirty);
    R.Data->SetBoolField(TEXT("module_package_dirty_before"), C.bModuleDirty); R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.System));
    R.Data->SetBoolField(TEXT("selected_stack_preflight_verified"), true); R.Data->SetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    R.Data->SetStringField(TEXT("write_scope"), Temporary ? TEXT("target_temp_stock_module_stack_insert") : TEXT("source_stock_module_stack_insert")); return true;
}

bool SameGraph(const FSnapshot& A, const FSnapshot& B)
{
    if (!A.bComplete || !B.bComplete || !A.Nodes.OrderIndependentCompareEqual(B.Nodes) || !A.Pins.OrderIndependentCompareEqual(B.Pins) || A.Links.Num() != B.Links.Num()) { return false; }
    for (const auto& Pair : A.Links) { const auto* Found = B.Links.Find(Pair.Key); if (!Found || Pair.Value != *Found) { return false; } }
    return true;
}
bool SourcePreserved(FResult& R, const FContext& C)
{
    const FSnapshot After = Capture(C.ModuleGraph); R.Data->SetObjectField(TEXT("module_source_after"), After.Json);
    const bool Preserved = SameGraph(C.ModuleBefore, After) && Text(C.ModuleBefore.Json) == Text(After.Json) &&
        C.bModuleDirty == C.Module->GetOutermost()->IsDirty() && C.ModuleSource->NodeGraph == C.ModuleGraph;
    R.Data->SetBoolField(TEXT("module_source_preserved"), Preserved); R.Data->SetBoolField(TEXT("module_package_dirty_after"), C.Module->GetOutermost()->IsDirty());
    if (!After.bComplete)
    { R.bChangeStateKnown = false; if (R.ErrorCode.IsEmpty()) { R.Stage = TEXT("observe_module_source"); Fail(R, TEXT("niagara_module_source_unobservable"), TEXT("Module source observation is incomplete; any target insertion remains and no retry is safe.")); } return false; }
    if (!Preserved)
    { R.bChanged = true; if (R.ErrorCode.IsEmpty()) { R.Stage = TEXT("observe_module_source"); Fail(R, TEXT("niagara_module_source_changed"), TEXT("Exact external module graph/source package changed across observation; any target insertion is retained.")); } return false; }
    return true;
}
void AddEdge(TMap<FString, TArray<FString>>& Links, const FString& A, const FString& B)
{ Links.FindOrAdd(A).AddUnique(B); Links.FindOrAdd(B).AddUnique(A); }
void RemoveEdge(TMap<FString, TArray<FString>>& Links, const FString& A, const FString& B)
{ if (auto* Row = Links.Find(A)) { Row->Remove(B); } if (auto* Row = Links.Find(B)) { Row->Remove(A); } }
bool VerifyInsertion(FResult& R, FContext& C, UNiagaraNodeFunctionCall* NewModule, const FSnapshot& After,
    int32 InsertIndex, const FString& PreviousId, const TArray<FString>& NextIds, const TArray<UNiagaraNodeFunctionCall*>& BeforeModules)
{
    if (!After.bComplete) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_insert_state_unobservable"), TEXT("Native insertion was dispatched but actual bounded graph state is incomplete; stop without retry.")); }
    const bool PriorNodes = [&]() { for (const auto& Pair : C.Before.Nodes) { const auto* V = After.Nodes.Find(Pair.Key); if (!V || *V != Pair.Value) { return false; } } return true; }();
    const bool PriorPins = [&]() { for (const auto& Pair : C.Before.Pins) { const auto* V = After.Pins.Find(Pair.Key); if (!V || *V != Pair.Value) { return false; } } return true; }();
    R.Data->SetBoolField(TEXT("prior_node_metadata_preserved"), PriorNodes); R.Data->SetBoolField(TEXT("prior_pin_metadata_preserved"), PriorPins);
    const bool Identity = Loaded(NewModule) && NewModule->GetClass() == UNiagaraNodeFunctionCall::StaticClass() && NewModule->GetOuter() == C.Graph &&
        C.Graph->Nodes.Contains(NewModule) && NewModule->FunctionScript == C.Module && NewModule->SelectedScriptVersion == C.ModuleVersion &&
        NewModule->NodeGuid.IsValid() && !C.Before.Nodes.Contains(Path(NewModule)) && After.Nodes.Num() == C.Before.Nodes.Num() + 1 && !NewModule->GetFunctionName().IsEmpty();
    R.Data->SetBoolField(TEXT("new_module_identity_verified"), Identity);
    if (!PriorNodes || !PriorPins || !Identity) { return Fail(R, TEXT("niagara_insert_node_postcondition_failed"), TEXT("New module identity/version/node count or existing node/pin metadata differs; observed partial graph retained.")); }
    for (UEdGraphNode* Existing : C.Graph->Nodes)
    { if (Existing != NewModule && Existing->NodeGuid == NewModule->NodeGuid) { return Fail(R, TEXT("niagara_insert_guid_duplicate"), TEXT("Inserted module GUID duplicates an existing node; observed graph retained.")); } }
    UEdGraphPin* NewIn = nullptr; UEdGraphPin* NewOut = nullptr;
    if (!MapPin(NewModule, EGPD_Input, NewIn) || !MapPin(NewModule, EGPD_Output, NewOut) || !NewIn || !NewOut)
    { return Fail(R, TEXT("niagara_insert_map_pins_invalid"), TEXT("Inserted module lacks unique actual parameter-map input/output pins; graph retained.")); }
    auto ExpectedLinks = C.Before.Links; const FString InputId = PinId(NewIn), OutputId = PinId(NewOut);
    for (const FString& Next : NextIds) { RemoveEdge(ExpectedLinks, PreviousId, Next); AddEdge(ExpectedLinks, OutputId, Next); }
    AddEdge(ExpectedLinks, PreviousId, InputId);
    for (const UEdGraphPin* Pin : NewModule->Pins) { ExpectedLinks.FindOrAdd(PinId(Pin)); }
    for (auto& Pair : ExpectedLinks) { Pair.Value.Sort(); }
    bool ExactLinks = ExpectedLinks.Num() == After.Links.Num();
    for (const auto& Pair : ExpectedLinks) { const auto* V = After.Links.Find(Pair.Key); if (!V || *V != Pair.Value) { ExactLinks = false; break; } }
    R.Data->SetBoolField(TEXT("exact_native_splice_verified"), ExactLinks);
    if (!ExactLinks) { return Fail(R, TEXT("niagara_insert_splice_postcondition_failed"), TEXT("Actual link union differs from the one native boundary splice; all observed changes retained.")); }
    if (!Stack(R, C)) { return false; }
    auto ExpectedModules = BeforeModules; ExpectedModules.Insert(NewModule, InsertIndex);
    const bool Order = ExpectedModules == C.Modules;
    R.Data->SetBoolField(TEXT("module_order_verified"), Order); R.Data->SetArrayField(TEXT("ordered_modules"), ModuleRows(C));
    R.Data->SetNumberField(TEXT("module_count"), C.Modules.Num());
    if (!Order) { return Fail(R, TEXT("niagara_insert_order_postcondition_failed"), TEXT("Observed module order differs from the clamped legacy absolute insertion index.")); }
    return true;
}
void Finish(FResult& R, FContext& C, const FSnapshot& AfterInsertion, bool bSave, bool bRequestCompile)
{
    if (R.ErrorCode.IsEmpty() && R.bChangeStateKnown)
    {
        if (bRequestCompile)
        {
            R.Stage = TEXT("request_compile"); R.Data->SetBoolField(TEXT("compile_requested"), true); R.Data->SetBoolField(TEXT("compile_attempted"), true);
            const bool Returned = C.System->RequestCompile(false); R.Data->SetBoolField(TEXT("compile_request_returned"), Returned);
        }
        if (bSave)
        {
            auto State = CompileState(C.System); R.Data->SetObjectField(TEXT("save_compile_state"), State);
            if (!State->GetBoolField(TEXT("ready_to_save")))
            {
                R.Stage = TEXT("save_deferred"); R.Data->SetBoolField(TEXT("save_deferred"), true);
                Fail(R, TEXT("niagara_save_deferred_compile_pending"), TEXT("Inserted module remains. Outstanding/required compilation would block PreSave; inspect readiness and explicitly save the exact asset when ready. Do not replay insertion."));
            }
            else
            {
                R.Stage = TEXT("save"); R.Data->SetBoolField(TEXT("save_attempted"), true);
                const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(C.System, false); R.Data->SetBoolField(TEXT("saved"), Saved);
                if (!Saved) { Fail(R, TEXT("niagara_save_failed"), TEXT("Save returned false; insertion remains, no rollback or retry performed.")); }
                else if (C.System->GetOutermost()->IsDirty()) { Fail(R, TEXT("niagara_save_postcondition_failed"), TEXT("Save returned true but the exact system package remains dirty.")); }
            }
        }
    }
    const FSnapshot After = Capture(C.Graph); R.Data->SetObjectField(TEXT("after"), After.Json);
    R.Data->SetNumberField(TEXT("graph_node_count_after"), C.Graph->Nodes.Num()); R.Data->SetBoolField(TEXT("package_dirty_after"), C.System->GetOutermost()->IsDirty());
    R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.System));
    if (!After.bComplete)
    { R.bChangeStateKnown = false; R.Stage = TEXT("observe_after"); Fail(R, TEXT("niagara_after_state_unobservable"), TEXT("Final graph observation is incomplete; mutations retained and no automatic retry.")); }
    else
    {
        R.bChanged = R.bChanged || !SameGraph(C.Before, After) || C.bSystemDirty != C.System->GetOutermost()->IsDirty();
        const bool Preserved = SameGraph(AfterInsertion, After); R.Data->SetBoolField(TEXT("graph_preserved_across_compile_save"), Preserved);
        if (!Preserved && R.ErrorCode.IsEmpty()) { R.Stage = TEXT("observe_after"); Fail(R, TEXT("niagara_compile_save_graph_changed"), TEXT("Graph metadata/links changed across the compile/save boundary; actual graph retained.")); }
    }
    const bool SourceOk = SourcePreserved(R, C);
    R.Data->SetBoolField(TEXT("compile_and_save_suppressed_unknown_outcome"), !R.bChangeStateKnown);
    R.bPostconditionPassed = R.bChangeStateKnown && SourceOk && R.ErrorCode.IsEmpty(); R.bSuccess = R.bPostconditionPassed;
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
}
}

Function::FResult Inspect(const FString& SystemPath, const FString& ModuleScriptPath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson, const FString& TargetEmitterName)
{
    FResult R; Initialize(R, SystemPath, ModuleScriptPath); R.Data->SetBoolField(TEXT("read_only"), true); FContext C;
    if (!Prepare(R, C, SystemPath, ModuleScriptPath, TargetUsage, TargetUsageId, TargetEmitterIndexJson, TargetEmitterName, false, false)) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(C.System); TStrongObjectPtr<UNiagaraScript> KeepModule(C.Module);
    const FSnapshot After = Capture(C.Graph); R.Data->SetObjectField(TEXT("after"), After.Json);
    const bool Preserved = SameGraph(C.Before, After) && Text(C.Before.Json) == Text(After.Json) && C.bSystemDirty == C.System->GetOutermost()->IsDirty();
    R.Data->SetBoolField(TEXT("state_preserved"), Preserved); R.Data->SetBoolField(TEXT("package_dirty_after"), C.System->GetOutermost()->IsDirty());
    R.Data->SetNumberField(TEXT("graph_node_count_after"), C.Graph->Nodes.Num()); R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.System));
    if (!After.bComplete) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_target_after_unobservable"), TEXT("Read-only target observation is incomplete.")); }
    else if (!Preserved) { R.bChanged = true; Fail(R, TEXT("niagara_target_not_preserved"), TEXT("Loaded target graph/package changed during read-only observation.")); }
    const bool SourceOk = SourcePreserved(R, C); R.bSuccess = R.bChangeStateKnown && Preserved && SourceOk && R.ErrorCode.IsEmpty();
    R.bPostconditionPassed = R.bSuccess; if (R.bSuccess) { R.Stage = TEXT("complete"); } return R;
}

Function::FResult Add(const FString& SystemPath, const FString& ModuleScriptPath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
    const FString& TargetEmitterName, const FString& TargetIndexJson, const FString& SuggestedName,
    bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    FResult R; Initialize(R, SystemPath, ModuleScriptPath, bSave); R.Data->SetBoolField(TEXT("already_loaded_assets_required"),false);
    R.Data->SetBoolField(TEXT("request_compile"), bRequestCompile); R.Data->SetBoolField(TEXT("skip_if_duplicate"), bSkipIfDuplicate);
    R.Data->SetBoolField(TEXT("skipped_duplicate"), false); R.Data->SetStringField(TEXT("target_index_json"), TargetIndexJson);
    R.Data->SetStringField(TEXT("suggested_name"), SuggestedName); int32 RequestedIndex;
    if (!Index(TargetIndexJson, RequestedIndex) || !Bounded(SuggestedName, 256))
    { Fail(R, TEXT("niagara_target_index_or_name_invalid"), TEXT("target_index requires JSON null or exact int32; suggested_name must be bounded and contain no control characters.")); return R; }
    R.Data->SetNumberField(TEXT("target_index"), RequestedIndex); FContext C;
    if (!Prepare(R, C, SystemPath, ModuleScriptPath, TargetUsage, TargetUsageId, TargetEmitterIndexJson, TargetEmitterName, true, bAllowSourceEdit, true)) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(C.System); TStrongObjectPtr<UNiagaraScript> KeepModule(C.Module);
    if (bSkipIfDuplicate)
    {
        for (auto* Existing : C.Modules)
        {
            if (Existing->FunctionScript == C.Module)
            {
                R.Data->SetBoolField(TEXT("skipped_duplicate"), true); R.Data->SetStringField(TEXT("skip_reason"), TEXT("stock_module_already_exists_in_target_usage"));
                R.Data->SetStringField(TEXT("existing_module_node_guid"), Guid(Existing->NodeGuid)); R.Data->SetStringField(TEXT("existing_module_node_name"), Existing->GetName());
                R.Data->SetStringField(TEXT("existing_module_function_name"), Existing->GetFunctionName()); R.Data->SetStringField(TEXT("existing_module_node_path"), Path(Existing));
                R.Data->SetObjectField(TEXT("module"), NodeMetadata(Existing)); R.Data->SetObjectField(TEXT("after"), C.Before.Json);
                R.Data->SetNumberField(TEXT("graph_node_count_after"), C.Graph->Nodes.Num()); R.Data->SetBoolField(TEXT("package_dirty_after"), C.System->GetOutermost()->IsDirty());
                const FSnapshot Current = Capture(C.Graph); const bool Preserved = SameGraph(C.Before, Current) && C.bSystemDirty == C.System->GetOutermost()->IsDirty();
                R.Data->SetBoolField(TEXT("state_preserved"), Preserved); const bool SourceOk = SourcePreserved(R, C);
                if (!Current.bComplete) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_duplicate_state_unobservable"), TEXT("Duplicate skip dispatched no mutation, but current graph observation is incomplete.")); }
                else if (!Preserved) { R.bChanged = true; Fail(R, TEXT("niagara_duplicate_state_changed"), TEXT("Graph/package changed during duplicate preflight; no insertion/compile/save dispatched.")); }
                R.bSuccess = R.bChangeStateKnown && Preserved && SourceOk && R.ErrorCode.IsEmpty(); R.bPostconditionPassed = R.bSuccess;
                if (R.bSuccess) { R.Stage = TEXT("complete"); } return R;
            }
        }
    }
    const int32 InsertIndex = RequestedIndex == INDEX_NONE ? C.Modules.Num() : FMath::Clamp(RequestedIndex, 0, C.Modules.Num());
    R.Data->SetNumberField(TEXT("resolved_target_index"), InsertIndex);
    // Match the native clamp before dispatch, avoiding TargetIndex+1 overflow
    // for MAX_int32 while retaining its append-at-end observable behavior.
    UEdGraphNode* Previous = InsertIndex == 0 ? static_cast<UEdGraphNode*>(C.Input) : static_cast<UEdGraphNode*>(C.Modules[InsertIndex - 1]);
    UEdGraphPin* PreviousOut = nullptr;
    if (!MapPin(Previous, EGPD_Output, PreviousOut) || !PreviousOut || PreviousOut->LinkedTo.IsEmpty())
    { Fail(R, TEXT("niagara_splice_boundary_invalid"), TEXT("Native insertion boundary is not observable before mutation.")); return R; }
    const FString PreviousId = PinId(PreviousOut); TArray<FString> NextIds;
    for (auto* Next : PreviousOut->LinkedTo) { NextIds.Add(PinId(Next)); }
    NextIds.Sort(); R.Data->SetStringField(TEXT("splice_previous_output_pin_id"), PreviousId);
    TArray<TSharedPtr<FJsonValue>> NextRows; for (const FString& Id : NextIds) { NextRows.Add(MakeShared<FJsonValueString>(Id)); }
    R.Data->SetArrayField(TEXT("splice_target_pin_ids"), NextRows); const auto BeforeModules = C.Modules;
    R.Stage = TEXT("insert_stock_module"); R.bMutationAttempted = true; C.System->Modify(); C.Graph->Modify(); C.Output->Modify();
    UNiagaraNodeFunctionCall* NewModule = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
        C.Module, *C.Output, InsertIndex, SuggestedName, C.ModuleVersion);
    R.Data->SetBoolField(TEXT("native_module_returned"), NewModule != nullptr);
    if (Loaded(NewModule))
    {
        R.Data->SetStringField(TEXT("new_module_node_guid"), Guid(NewModule->NodeGuid)); R.Data->SetStringField(TEXT("new_module_node_name"), NewModule->GetName());
        R.Data->SetStringField(TEXT("new_module_function_name"), NewModule->GetFunctionName()); R.Data->SetStringField(TEXT("new_module_node_path"), Path(NewModule));
        R.Data->SetObjectField(TEXT("module"), NodeMetadata(NewModule));
    }
    C.System->MarkPackageDirty(); const FSnapshot AfterInsertion = Capture(C.Graph); R.Data->SetObjectField(TEXT("after_insertion"), AfterInsertion.Json);
    VerifyInsertion(R, C, NewModule, AfterInsertion, InsertIndex, PreviousId, NextIds, BeforeModules);
    SourcePreserved(R, C); Finish(R, C, AfterInsertion, bSave, bRequestCompile); return R;
}
}

// CH: original ordered RapidIteration batch, separate from graph override inputs.
namespace Cubeless::UE58::NiagaraModule
{
namespace
{
using FBatchResult = Variable::FResult;
void BatchCount(FBatchResult& R, const TCHAR* Key)
{ double N = 0; R.Data->TryGetNumberField(Key, N); R.Data->SetNumberField(Key, N + 1); }
bool BatchFail(FBatchResult& R, const TCHAR* Code, const TCHAR* Message)
{ bool Owned=false;R.Data->TryGetBoolField(TEXT("GU_owned_authoring_child"),Owned);if(Owned&&!R.Error.IsEmpty())return false;R.ErrorCode = Code; R.Error = Message; return false; }
bool BatchEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (!A || !B || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object)
    {
        const auto X = A->AsObject(), Y = B->AsObject(); if (!X || !Y || X->Values.Num() != Y->Values.Num()) { return false; }
        for (const auto& P : X->Values) { if (!BatchEqual(P.Value, Y->TryGetField(P.Key.ToView()))) { return false; } } return true;
    }
    if (A->Type == EJson::Array)
    { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!BatchEqual(X[I], Y[I])) { return false; } } return true; }
    if (A->Type == EJson::String) { return A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive); }
    return FJsonValue::CompareEqual(*A, *B);
}
bool BatchEqual(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B)
{ return BatchEqual(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
TSharedRef<FJsonObject> BatchCopy(const TSharedRef<FJsonObject>& Old)
{ auto J = MakeShared<FJsonObject>(); for (const auto& P : Old->Values) { J->SetField(FString(P.Key.ToView()), P.Value); } return J; }
FString BatchParameterKey(const TSharedRef<FJsonObject>& Parameters, FName Name)
{ for (const auto& P : Parameters->Values) { const FString Key(P.Key.ToView()); if (FName(*Key) == Name) { return Key; } } return Name.ToString(); }
bool BatchForbidden(const FString& S)
{
    for (const TCHAR* T : { TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/"), TEXT("/Script/CubelessMCP"),
        TEXT("/Script/UnrealMCP"), TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry") })
    { if (S.Contains(T, ESearchCase::IgnoreCase)) { return true; } } return false;
}
bool BatchJsonBound(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Count)
{
    if (!V || Depth > 32 || ++Count > 16384) { return false; }
    if (V->Type == EJson::Array) { for (const auto& E : V->AsArray()) { if (!BatchJsonBound(E,Depth+1,Count)) { return false; } } }
    else if (V->Type == EJson::Object) { for (const auto& P : V->AsObject()->Values) { if (P.Key.Len() > 1024 || !BatchJsonBound(P.Value,Depth+1,Count)) { return false; } } }
    else if (V->Type == EJson::String) { return V->AsString().Len() <= 8192; }
    else if (V->Type == EJson::Number) { return FMath::IsFinite(V->AsNumber()); }
    return true;
}
FString BatchBytes(const uint8* Data, int32 Size)
{ FString S; for (int32 I = 0; I < Size; ++I) { S += FString::Printf(TEXT("%02x"), Data[I]); } return S; }
TSharedRef<FJsonObject> BatchStore(const FNiagaraParameterStore& Store, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); auto Rows = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("parameters"), Rows);
    J->SetBoolField(TEXT("complete"), false); J->SetStringField(TEXT("owner"), Path(Store.GetOwner()));
    TArray<FNiagaraVariable> Vars; Store.GetParameters(Vars); TSet<FName> Names; Complete = Vars.Num() <= 2048 && Store.GetParameterDataArray().Num() <= 1048576;
    if (!Complete || Store.GetDataInterfaces().Num() || Store.GetUObjects().Num()) { Complete = false; return J; }
    for (const auto& V : Vars)
    {
        auto Row = MakeShared<FJsonObject>(); Rows->SetObjectField(V.GetName().ToString(), Row);
        const auto& T = V.GetType(); const int32 Size = V.GetSizeInBytes(), Offset = Store.IndexOf(V);
        Row->SetStringField(TEXT("name"), V.GetName().ToString()); Row->SetStringField(TEXT("type"), T.GetNameText().ToString());
        Row->SetStringField(TEXT("struct"), Path(T.GetScriptStruct())); Row->SetStringField(TEXT("enum"), Path(T.GetEnum()));
        Row->SetNumberField(TEXT("size"), Size);
        if (!T.IsValid() || T.IsDataInterface() || T.IsUObject() || Names.Contains(V.GetName()) || Size <= 0 || Size > 4096 ||
            Offset < 0 || Offset > Store.GetParameterDataArray().Num() - Size) { Complete = false; return J; }
        Names.Add(V.GetName()); Row->SetStringField(TEXT("bytes_hex"), BatchBytes(Store.GetParameterDataArray().GetData() + Offset, Size));
    }
    J->SetBoolField(TEXT("complete"), true); return J;
}

// HX: authoring identities only; compiler cached-DI exports are not captured.
bool BatchAsyncRunning=false, BatchAsyncLease=false;
std::atomic<bool> BatchAsyncStopping{false};
bool BatchSafe()
{ return IsInGameThread() && GEditor && !IsEngineExitRequested() && !GExitPurge && !IsGarbageCollecting() && !UE::IsSavingPackage(); }
bool BatchLive(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool BatchReadableChain(const UObject* O)
{
    TSet<const UObject*> Seen;
    for(int32 I=0;O&&I<64;++I)
    { if(!BatchLive(O)||Seen.Contains(O))return false; Seen.Add(O); O=O->GetOuter(); }
    return O==nullptr;
}
struct FBatchIdentity
{
    TStrongObjectPtr<UObject> Hold{nullptr};
    TWeakObjectPtr<UObject> Object,Outer; TWeakObjectPtr<UClass> Class;
    TWeakObjectPtr<UPackage> Package,EffectivePackage; FString ObjectPath;
    explicit FBatchIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),
        Package(O->GetOutermost()),EffectivePackage(O->GetPackage()),ObjectPath(O->GetPathName()) {}
    bool Exact() const
    {
        UObject* O=Object.Get();
        return BatchReadableChain(O)&&BatchLive(Class.Get())&&BatchReadableChain(Package.Get())&&BatchReadableChain(EffectivePackage.Get())&&
            O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==EffectivePackage.Get()&&
            O->GetPathName().Equals(ObjectPath,ESearchCase::CaseSensitive);
    }
};
struct FBatchGraphIdentity
{
    TStrongObjectPtr<UNiagaraGraph> Graph{nullptr};
    TArray<UEdGraphNode*> Nodes;
    TMap<UEdGraphNode*,TArray<UEdGraphPin*>> Pins;
    TMap<UEdGraphPin*,TArray<UEdGraphPin*>> Links;
    TMap<UEdGraphPin*,FString> DefaultValues,DefaultTexts;
    TMap<UEdGraphPin*,FName> PinNames;TMap<UEdGraphPin*,FGuid> PinIds;
    TMap<UEdGraphPin*,FEdGraphPinType> PinTypes;
    TMap<UEdGraphPin*,UObject*> DefaultObjects;
    explicit FBatchGraphIdentity(UNiagaraGraph* G):Graph(G)
    {
        for(UEdGraphNode* N:G->Nodes)
        {
            Nodes.Add(N); Pins.Add(N,N->Pins);
            for(UEdGraphPin* P:N->Pins)
            {
                Links.Add(P,P->LinkedTo);
                PinIds.Add(P,P->PinId);PinNames.Add(P,P->PinName);PinTypes.Add(P,P->PinType);DefaultValues.Add(P,P->DefaultValue);DefaultTexts.Add(P,P->DefaultTextValue.ToString());
                DefaultObjects.Add(P,P->DefaultObject.Get());
            }
        }
    }
    bool Exact() const
    {
        if(!BatchLive(Graph.Get())||Graph->Nodes.Num()!=Nodes.Num())return false;
        for(int32 I=0;I<Nodes.Num();++I)
        {
            UEdGraphNode* N=Nodes[I];
            if(Graph->Nodes[I]!=N||!BatchLive(N))return false;
            const TArray<UEdGraphPin*>* Expected=Pins.Find(N);
            if(!Expected||N->Pins!=*Expected)return false;
        }
        // Every raw pin is compared against live physical membership before any dereference.
        for(UEdGraphNode* N:Nodes)for(UEdGraphPin* P:N->Pins)
        {
            if(!P||P->GetOwningNodeUnchecked()!=N||P->LinkedTo!=Links.FindChecked(P)||P->DefaultObject.Get()!=DefaultObjects.FindChecked(P)||
                PinIds.FindChecked(P)!=P->PinId||PinNames.FindChecked(P)!=P->PinName||!(PinTypes.FindChecked(P)==P->PinType)||DefaultValues.FindChecked(P)!=P->DefaultValue||DefaultTexts.FindChecked(P)!=P->DefaultTextValue.ToString())return false;
        }
        return true;
    }
};
struct FBatchScriptIdentity
{
    UNiagaraScript* Script=nullptr; UNiagaraScriptSource* Source=nullptr; FGuid Version;
};

struct FBatchSourceIdentity { UNiagaraScriptSource* Source=nullptr; UNiagaraGraph* Graph=nullptr; };
struct FBatchCallIdentity { UNiagaraNodeFunctionCall* Call=nullptr; UNiagaraScript* Script=nullptr; UNiagaraScriptSource* Source=nullptr; FGuid Version; };
struct FBatchHandleIdentity { UNiagaraEmitter* Emitter=nullptr; FGuid Id,Version; FName Name; bool Enabled=false; const FVersionedNiagaraEmitterData* Data=nullptr; UNiagaraScriptSource* Source=nullptr; ENiagaraSimTarget SimTarget=ENiagaraSimTarget::CPUSim; bool Interpolated=false; UNiagaraScript* Spawn=nullptr; };

struct FBatchAssignmentIdentity
{
    UNiagaraNodeAssignment* Node=nullptr;UNiagaraGraph* HostGraph=nullptr;FGuid Guid,Version,Exposed;
    UNiagaraScript* Script=nullptr;const FVersionedNiagaraScriptData* Data=nullptr;UNiagaraScriptSource* Source=nullptr;UNiagaraGraph* Graph=nullptr;
    FString NodePath;TArray<FNiagaraVariable> Targets;TArray<FString> Defaults;TArray<UObject*> TypeObjects;
};
// ADM: existing shared batch authoring also admits ordinary owned GPU emitters.
// Private GPUComputeScript is observed through public reflected slot access only;
// no private access, script setter, shader wait/flush, CDO or renderer resource is added.
struct FBatchGpuRole
{
 int32 Index=INDEX_NONE;FGuid Id,Version;const FVersionedNiagaraEmitterData* Data=nullptr;
 TObjectPtr<UNiagaraScriptSourceBase> SourceSlot;TObjectPtr<UObject> ScriptSlot;TObjectPtr<UNiagaraGraph> GraphSlot;
 FObjectProperty* Field=nullptr;uint64 Flags=0;int32 Offset=0;
 TArray<TUniquePtr<FBatchIdentity>> Objects;
};
struct FBatchScratchCatalog
{
    UObject* Owner=nullptr;
    int32 EmitterIndex=INDEX_NONE;
    UNiagaraEmitter* Emitter=nullptr;
    FGuid EmitterId,EmitterVersion;
    const FVersionedNiagaraEmitterData* Data=nullptr;
    bool Parent=false;
    TArray<TObjectPtr<UNiagaraScript>> Members;
    TArray<TUniquePtr<FBatchIdentity>> Objects;
};
struct FBatchContext
{

    bool Deferred=false, Captured=false, CancelRequested=false, GuardFailed=false, RIWriter=false;
    const TFunction<bool(UNiagaraSystem*,const TCHAR*)>* OwnedAuthoringGuard=nullptr; // borrowed only for one synchronous GU call
    FString CancelReason;
    TWeakObjectPtr<UEditorEngine> Editor;
    TWeakObjectPtr<UWorld> World;
    TArray<TUniquePtr<FBatchIdentity>> Identities;
    TArray<TUniquePtr<FBatchGraphIdentity>> PhysicalGraphs;
    TArray<FBatchScriptIdentity> ScriptIdentities;

    TArray<FBatchSourceIdentity> SourceIdentities;
    TArray<FBatchCallIdentity> CallIdentities;
    TArray<FBatchAssignmentIdentity> AssignmentIdentities;
    TArray<FBatchHandleIdentity> HandleIdentities;
    TArray<FBatchGpuRole> GpuRoles;
    TArray<FBatchScratchCatalog> ScratchCatalogs;
    TMap<UNiagaraScript*,ENiagaraScriptUsage> ScratchUsages;
    TSet<UNiagaraGraph*> ScratchSourceGraphs;
    UNiagaraScript* SystemSpawn=nullptr;UNiagaraScript* SystemUpdate=nullptr;

    UNiagaraSystem* System = nullptr;
    TArray<UNiagaraScript*> Scripts;
    TArray<UNiagaraGraph*> Graphs, SourceGraphs;
    TArray<TStrongObjectPtr<UObject>> Keep;
};

bool BatchPublicationSafe(const FBatchContext& C)
{
    if(!BatchSafe()||C.Editor.Get()!=GEditor)return false;
    UWorld* W=GEditor->GetEditorWorldContext().World();
    return BatchLive(W)&&!W->bIsTearingDown&&C.World.Get()==W&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext();
}
bool BatchScratchCatalogsCurrent(FBatchContext& C,FBatchResult& R,const TCHAR* Stage)
{
    if(C.ScratchCatalogs.IsEmpty())return true;
    auto Stop=[&](const TCHAR* Field){C.GuardFailed=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;R.Stage=Stage;
        if(!R.Data->HasField(TEXT("scratch_catalog_first_mismatch")))R.Data->SetStringField(TEXT("scratch_catalog_first_mismatch"),Field);
        return BatchFail(R,TEXT("niagara_batch_scratch_catalog_changed"),TEXT("Captured local Scratch catalog/usage changed; actual prefix retained, no further SDK or replay."));};
    if(!BatchPublicationSafe(C)||!BatchLive(C.System))return Stop(TEXT("Editor/world/system"));
    for(const auto& E:C.ScratchCatalogs)
    {
        for(const auto& I:E.Objects)if(!I->Exact())return Stop(TEXT("catalog owner/member identity"));
        const TArray<TObjectPtr<UNiagaraScript>>* Current=nullptr;
        if(E.EmitterIndex==INDEX_NONE){if(E.Owner!=C.System)return Stop(TEXT("system catalog owner"));Current=&C.System->ScratchPadScripts;}
        else
        {
            const auto& Handles=C.System->GetEmitterHandles();if(!Handles.IsValidIndex(E.EmitterIndex))return Stop(TEXT("emitter index"));
            const auto& H=Handles[E.EmitterIndex];
            if(H.GetId()!=E.EmitterId||H.GetInstance().Emitter!=E.Emitter||H.GetInstance().Version!=E.EmitterVersion)return Stop(TEXT("emitter handle/version/instance"));
            const auto* D=H.GetEmitterData();if(!D||D!=E.Data)return Stop(TEXT("current emitter data"));
            auto* Container=(E.Parent?D->ParentScratchPads:D->ScratchPads).Get();
            if(Container!=E.Owner)return Stop(TEXT("current scratch container"));Current=&Container->Scripts;
        }
        if(*Current!=E.Members)return Stop(TEXT("ordered catalog members"));
    }
    for(const auto& E:C.ScratchUsages)if(!BatchLive(E.Key)||E.Key->GetUsage()!=E.Value)return Stop(TEXT("selected scratch script usage"));
    return true;
}

bool BatchPrepareScratchSource(FBatchContext& C,FBatchResult& R,UNiagaraNodeFunctionCall* Call)
{
    bool SourceGetterEntered=false;
    auto Bad=[&](const TCHAR* Field){R.Data->SetStringField(TEXT("scratch_source_admission_failure"),Field);
        if(SourceGetterEntered){C.GuardFailed=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;R.Stage=TEXT("capture_local_scratch_source");}
        return BatchFail(R,TEXT("niagara_batch_scratch_source_unobservable"),TEXT("Exact registered local Scratch catalog/Script/version/Source/Graph required; no source construction or replacement."));};
    auto* Script=Call->FunctionScript.Get();if(!Loaded(Script)||Script->GetClass()!=UNiagaraScript::StaticClass()||!Script->IsIn(C.System))return Bad(TEXT("local script"));
    FBatchScratchCatalog E;const TArray<TObjectPtr<UNiagaraScript>>* Members=nullptr;
    if(Script->GetOuter()==C.System&&C.System->ScratchPadScripts.Contains(Script)){E.Owner=C.System;Members=&C.System->ScratchPadScripts;}
    else
    {
        const auto& Handles=C.System->GetEmitterHandles();
        for(int32 I=0;I<Handles.Num()&&!Members;++I)
        {
            const auto& H=Handles[I];const auto* D=H.GetEmitterData();if(!D)continue;
            for(bool Parent:{false,true})
            {
                auto* Container=(Parent?D->ParentScratchPads:D->ScratchPads).Get();
                if(!Container||Script->GetOuter()!=Container)continue;
                if(!Loaded(Container)||!BatchReadableChain(Container)||Container->GetClass()!=UNiagaraScratchPadContainer::StaticClass()||!Container->IsIn(C.System))return Bad(TEXT("local container"));
                if(!Container->Scripts.Contains(Script))continue;
                E.Owner=Container;E.EmitterIndex=I;E.Emitter=H.GetInstance().Emitter;E.EmitterId=H.GetId();E.EmitterVersion=H.GetInstance().Version;E.Data=D;E.Parent=Parent;Members=&Container->Scripts;break;
            }
        }
    }
    if(!Members||Members->Num()>128)return Bad(TEXT("registered bounded catalog"));
    if(!C.Editor.IsValid()){C.Editor=GEditor;C.World=GEditor->GetEditorWorldContext().World();C.Keep.Emplace(GEditor);if(C.World.IsValid())C.Keep.Emplace(C.World.Get());}
    if(!BatchPublicationSafe(C))return Bad(TEXT("Editor/world"));
    if(!C.ScratchCatalogs.ContainsByPredicate([&](const FBatchScratchCatalog& Existing){return Existing.Owner==E.Owner;}))
    {
        if(C.ScratchCatalogs.Num()>=33)return Bad(TEXT("catalog count bound"));
        auto Hold=[&](UObject* O){if(!BatchLive(O)||!BatchReadableChain(O))return false;E.Objects.Add(MakeUnique<FBatchIdentity>(O));return true;};
        if(!Hold(C.System)||!Hold(E.Owner)||(E.Emitter&&!Hold(E.Emitter)))return Bad(TEXT("catalog identity"));
        E.Members=*Members;for(const auto& Member:E.Members)if(Member&&!Hold(Member.Get()))return Bad(TEXT("catalog member"));
        C.ScratchCatalogs.Add(MoveTemp(E));
    }
    const auto Usage=Script->GetUsage();const FGuid Version=Call->SelectedScriptVersion;
    auto* HostGraph=Call->GetNiagaraGraph();if(!Loaded(HostGraph)||!C.Graphs.Contains(HostGraph)||Call->GetOuter()!=HostGraph||!HostGraph->Nodes.Contains(Call))return Bad(TEXT("direct call host"));
    C.Keep.Emplace(Call);C.Keep.Emplace(Script);C.Keep.Emplace(HostGraph);
    FBatchIdentity CallIdentity(Call),HostIdentity(HostGraph);
    if(const auto* ExpectedUsage=C.ScratchUsages.Find(Script);ExpectedUsage&&*ExpectedUsage!=Usage)return Bad(TEXT("changed scratch usage during preparation"));
    C.ScratchUsages.FindOrAdd(Script)=Usage;
    if(!BatchScratchCatalogsCurrent(C,R,TEXT("before_local_scratch_source")))return false;
    auto* Data=Script->GetScriptData(Version);if(!Data)return Bad(TEXT("selected script version data"));
    SourceGetterEntered=true;R.Data->SetBoolField(TEXT("local_scratch_source_getter_entered"),true);
    auto* Source=Cast<UNiagaraScriptSource>(Script->GetSource(Version));C.Keep.Emplace(Source);
    if(!BatchScratchCatalogsCurrent(C,R,TEXT("after_local_scratch_source")))return false;
    if(!CallIdentity.Exact()||!HostIdentity.Exact()||!HostGraph->Nodes.Contains(Call)||!Call->FunctionScript.IsResolved()||Call->FunctionScript.Get()!=Script||Call->SelectedScriptVersion!=Version||Script->GetScriptData(Version)!=Data)return Bad(TEXT("current call/script version"));
    if(!Loaded(Source)||Source->GetClass()!=UNiagaraScriptSource::StaticClass()||Source->GetOuter()!=Script||Source->bIsCompilationCopy||Source->bIsReleased||!Source->NodeGraph.IsResolved())return Bad(TEXT("local source"));
    auto* Graph=Source->NodeGraph.Get();C.Keep.Emplace(Graph);
    if(!Loaded(Graph)||Graph->GetClass()!=UNiagaraGraph::StaticClass()||Graph->GetOuter()!=Source||Graph->GetPackage()!=C.System->GetPackage()||Graph->GetOutermost()!=C.System->GetOutermost()||!Graph->Schema.GetGCPtr().IsResolved()||Graph->Schema.GetGCPtr().Get()!=UEdGraphSchema_Niagara::StaticClass()||C.SourceGraphs.Num()>=128)return Bad(TEXT("local authoring graph"));
    C.SourceGraphs.AddUnique(Graph);C.ScratchSourceGraphs.Add(Graph);
    R.Data->SetNumberField(TEXT("local_scratch_source_graph_count"),C.ScratchSourceGraphs.Num());
    return true;
}
// SDK NiagaraEmitter.cpp1588-1623 includes GPUComputeScript in GetScripts(false,false).
// The authored SimTarget and source/script routing remain current across the existing
// whole-call callback boundaries; GPU caches/executable/shader state are not frozen.
bool BatchGpuCurrent(FBatchContext& C,FBatchResult& R,const TCHAR* Stage)
{
 if(C.GpuRoles.IsEmpty())return true;
 auto Stop=[&](const TCHAR* Field){C.GuardFailed=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;R.Stage=Stage;if(!R.Data->HasField(TEXT("gpu_batch_first_mismatch")))R.Data->SetStringField(TEXT("gpu_batch_first_mismatch"),Field);return BatchFail(R,TEXT("niagara_batch_gpu_source_changed"),TEXT("Captured owned GPU emitter/version/data/source/script routing changed; no next SDK, replay or source replacement."));};
 if(!BatchPublicationSafe(C)||C.CancelRequested||BatchAsyncStopping.load()||!BatchLive(C.System))return Stop(TEXT("Editor/world/system"));
 const auto& Handles=C.System->GetEmitterHandles();
 for(const auto& E:C.GpuRoles)
 {
  for(const auto& I:E.Objects)if(!I->Exact())return Stop(TEXT("held source identity"));
  if(!Handles.IsValidIndex(E.Index))return Stop(TEXT("emitter index"));const auto& H=Handles[E.Index];
  const auto Instance=H.GetInstance();auto* Emitter=static_cast<UNiagaraEmitter*>(E.Objects[1]->Hold.Get());
  if(H.GetEmitterMode()!=ENiagaraEmitterMode::Standard||H.GetId()!=E.Id||Instance.Version!=E.Version||!Instance.Emitter.IsResolved()||Instance.Emitter.GetHandle()!=TObjectPtr<UNiagaraEmitter>(Emitter).GetHandle())return Stop(TEXT("emitter mode/id/version/instance"));
  // SDK NiagaraEmitter.cpp2531-2569 is a native VersionData lookup; avoid the tracked handle getter.
  const FVersionedNiagaraEmitterData* D=Emitter->GetEmitterData(E.Version);if(D!=E.Data||!D||D->SimTarget!=ENiagaraSimTarget::GPUComputeSim||D->GraphSource.GetHandle()!=E.SourceSlot.GetHandle())return Stop(TEXT("current data/SimTarget/GraphSource"));
  if(FindFProperty<FObjectProperty>(FVersionedNiagaraEmitterData::StaticStruct(),TEXT("GPUComputeScript"))!=E.Field||E.Field->GetOwnerStruct()!=FVersionedNiagaraEmitterData::StaticStruct()||E.Field->ArrayDim!=1||E.Field->GetSize()!=sizeof(TObjectPtr<UObject>)||E.Field->HasSetterOrGetter()||E.Field->GetPropertyFlags()!=E.Flags||E.Field->GetOffset_ForInternal()!=E.Offset||!E.Field->PropertyClass.IsResolved()||E.Field->PropertyClass.GetHandle()!=TObjectPtr<UClass>(UNiagaraScript::StaticClass()).GetHandle())return Stop(TEXT("GPUComputeScript descriptor"));
  const auto* Slot=E.Field->GetObjectPtrPropertyValuePtr(E.Field->ContainerPtrToValuePtr<void>(D));if(!Slot->IsResolved()||Slot->GetHandle()!=E.ScriptSlot.GetHandle())return Stop(TEXT("GPUComputeScript slot"));
  if(static_cast<UNiagaraScript*>(E.Objects[4]->Hold.Get())->GetUsage()!=ENiagaraScriptUsage::ParticleGPUComputeScript)return Stop(TEXT("GPUComputeScript Usage"));
  auto* Source=static_cast<UNiagaraScriptSource*>(E.Objects[2]->Hold.Get());if(Source->bIsCompilationCopy||Source->bIsReleased||!Source->NodeGraph.IsResolved()||Source->NodeGraph.GetHandle()!=E.GraphSlot.GetHandle())return Stop(TEXT("owned graph source role"));
 }
 return true;
}
bool BatchCaptureGpuRole(FBatchContext& C,FBatchResult& R,int32 Index,const FNiagaraEmitterHandle& H,const FVersionedNiagaraEmitterData* D)
{
 if(!D||D->SimTarget!=ENiagaraSimTarget::GPUComputeSim)return true;
 auto Bad=[&](const TCHAR* Field){R.Data->SetStringField(TEXT("gpu_batch_admission_failure"),Field);return BatchFail(R,TEXT("niagara_batch_gpu_source_unavailable"),TEXT("Owned resolved GPU emitter/script/native source roles required before GetScripts; no hidden load or source construction."));};
 if(!C.Editor.IsValid()){C.Editor=GEditor;C.World=GEditor->GetEditorWorldContext().World();C.Keep.Emplace(GEditor);if(C.World.IsValid())C.Keep.Emplace(C.World.Get());}
 if(!BatchPublicationSafe(C)||!BatchGpuCurrent(C,R,TEXT("before_gpu_source_capture")))return false;
 FBatchGpuRole E;E.Index=Index;E.Id=H.GetId();E.Version=H.GetInstance().Version;E.Data=D;
 auto Own=[&](UObject* O){if(!BatchLive(O)||!BatchReadableChain(O))return false;E.Objects.Add(MakeUnique<FBatchIdentity>(O));return true;};
 if(!Own(C.System))return Bad(TEXT("system"));
 auto Resident=[&](const auto& Slot)->UObject* {if(!Slot.IsResolved()||Slot.GetPathName().IsEmpty())return nullptr;UObject* O=StaticFindObject(nullptr,nullptr,*Slot.GetPathName(),false);TStrongObjectPtr<UObject> Hold(BatchLive(O)?O:nullptr);return Hold.IsValid()&&Slot.GetHandle()==TObjectPtr<UObject>(Hold.Get()).GetHandle()&&Own(Hold.Get())?Hold.Get():nullptr;};
 const auto Instance=H.GetInstance();if(!Resident(Instance.Emitter))return Bad(TEXT("emitter"));
 E.SourceSlot=D->GraphSource;auto* Source=Cast<UNiagaraScriptSource>(Resident(E.SourceSlot));if(!Source||Source->GetClass()!=UNiagaraScriptSource::StaticClass()||!Source->IsIn(C.System)||Source->bIsCompilationCopy||Source->bIsReleased)return Bad(TEXT("GraphSource"));
 E.GraphSlot=Source->NodeGraph;auto* Graph=Cast<UNiagaraGraph>(Resident(E.GraphSlot));if(!Graph||Graph->GetClass()!=UNiagaraGraph::StaticClass()||Graph->GetOuter()!=Source)return Bad(TEXT("NodeGraph"));
 E.Field=FindFProperty<FObjectProperty>(FVersionedNiagaraEmitterData::StaticStruct(),TEXT("GPUComputeScript"));
 if(!E.Field||E.Field->GetOwnerStruct()!=FVersionedNiagaraEmitterData::StaticStruct()||E.Field->ArrayDim!=1||E.Field->GetSize()!=sizeof(TObjectPtr<UObject>)||E.Field->HasSetterOrGetter()||!E.Field->PropertyClass.IsResolved()||E.Field->PropertyClass.GetHandle()!=TObjectPtr<UClass>(UNiagaraScript::StaticClass()).GetHandle())return Bad(TEXT("GPUComputeScript descriptor"));
 E.Flags=E.Field->GetPropertyFlags();E.Offset=E.Field->GetOffset_ForInternal();E.ScriptSlot=*E.Field->GetObjectPtrPropertyValuePtr(E.Field->ContainerPtrToValuePtr<void>(D));
 auto* Script=Cast<UNiagaraScript>(Resident(E.ScriptSlot));if(!Script||Script->GetClass()!=UNiagaraScript::StaticClass()||!Script->IsIn(C.System)||Script->GetUsage()!=ENiagaraScriptUsage::ParticleGPUComputeScript)return Bad(TEXT("GPUComputeScript"));
 C.GpuRoles.Add(MoveTemp(E));R.Data->SetNumberField(TEXT("gpu_batch_emitter_count"),C.GpuRoles.Num());R.Data->SetBoolField(TEXT("gpu_runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("GPU_resources_or_shader_caches_frozen"),false);
 return BatchGpuCurrent(C,R,TEXT("captured_gpu_source_roles"));
}
// SZ: separate ordered authored Assignment state from its generated function cache.
bool BatchAssignmentsCurrent(FBatchContext& C,FBatchResult& R,const TCHAR* Stage)
{
    if(C.AssignmentIdentities.IsEmpty())return true;
    auto Bad=[&](const FBatchAssignmentIdentity& E,const TCHAR* Field,const FString& Actual)
    {
        C.GuardFailed=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("assignment_authored_targets_defaults_current"),false);R.bSuccess=R.bPostconditionPassed=false;R.Stage=Stage;
        if(!R.Data->HasField(TEXT("assignment_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node"),E.NodePath);J->SetStringField(TEXT("field"),Field);J->SetStringField(TEXT("expected"),TEXT("captured authored targets/defaults and current generated role"));J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("assignment_first_mismatch"),J);}
        return BatchFail(R,TEXT("niagara_batch_assignment_source_changed"),TEXT("Assignment authored state or generated source role changed; no automatic regeneration, source replacement or retry."));
    };
    auto Pure=[&](const FBatchAssignmentIdentity& E)->bool
    {
        if(!BatchPublicationSafe(C)||BatchAsyncStopping.load()||C.CancelRequested)return Bad(E,TEXT("context"),TEXT("Editor/world/exit/cancel changed"));
        if(!BatchLive(C.System)||!BatchReadableChain(C.System)||!BatchLive(E.HostGraph)||!BatchReadableChain(E.HostGraph)||!C.Graphs.Contains(E.HostGraph)||!E.HostGraph->IsIn(C.System)||!BatchLive(E.Node)||!BatchReadableChain(E.Node)||E.Node->GetClass()!=UNiagaraNodeAssignment::StaticClass()||E.Node->GetOuter()!=E.HostGraph||!E.HostGraph->Nodes.Contains(E.Node)||E.Node->NodeGuid!=E.Guid)return Bad(E,TEXT("assignment_host"),TEXT("native node/host membership unavailable"));
        if(!E.Node->FunctionScript.IsResolved()||E.Node->FunctionScript.Get()!=E.Script||E.Node->SelectedScriptVersion!=E.Version||!BatchLive(E.Script)||!BatchReadableChain(E.Script)||E.Script->GetClass()!=UNiagaraScript::StaticClass()||E.Script->GetOuter()!=E.Node||E.Script->GetUsage()!=ENiagaraScriptUsage::Module||E.Script->GetExposedVersion().VersionGuid!=E.Exposed||E.Script->GetScriptData(E.Version)!=E.Data)return Bad(E,TEXT("generated_script"),TEXT("actual Script/version/data/outer changed"));
        if(!BatchLive(E.Source)||!BatchReadableChain(E.Source)||E.Source->GetClass()!=UNiagaraScriptSource::StaticClass()||E.Source->GetOuter()!=E.Script||E.Source->bIsCompilationCopy||E.Source->bIsReleased||!E.Source->NodeGraph.IsResolved()||E.Source->NodeGraph.Get()!=E.Graph||!BatchLive(E.Graph)||!BatchReadableChain(E.Graph)||E.Graph->GetClass()!=UNiagaraGraph::StaticClass()||E.Graph->GetOuter()!=E.Source||E.Graph->GetPackage()!=C.System->GetPackage()||E.Graph->GetOutermost()!=C.System->GetOutermost()||!E.Graph->Schema.GetGCPtr().IsResolved()||E.Graph->Schema.GetGCPtr().Get()!=UEdGraphSchema_Niagara::StaticClass())return Bad(E,TEXT("generated_source"),TEXT("actual native Source/Graph outer/slot changed"));
        for(UObject* O:E.TypeObjects)if(!BatchLive(O)||!BatchReadableChain(O))return Bad(E,TEXT("target_type_source"),TEXT("captured target type unavailable"));
        const auto& Targets=E.Node->GetAssignmentTargets();const auto& Defaults=E.Node->GetAssignmentDefaults();
        if(Targets.Num()!=E.Targets.Num()||Defaults!=E.Defaults)return Bad(E,TEXT("ordered_targets_defaults"),FString::Printf(TEXT("targets=%d defaults=%d"),Targets.Num(),Defaults.Num()));
        for(int32 I=0;I<Targets.Num();++I){const auto& A=Targets[I];const auto& B=E.Targets[I];const int32 Size=A.GetAllocatedSizeInBytes();if(A.GetName()!=B.GetName()||A.GetType()!=B.GetType()||Size!=B.GetAllocatedSizeInBytes()||(Size>0&&FMemory::Memcmp(A.GetData(),B.GetData(),Size)!=0))return Bad(E,TEXT("ordered_target_value"),FString::Printf(TEXT("index=%d"),I));}
        return true;
    };
    // Close authored state before the public source getter, hold its return
    // immediately, then revalidate every captured role before further access.
    for(const auto& E:C.AssignmentIdentities)if(!Pure(E))return false;
    for(const auto& E:C.AssignmentIdentities)
    {
        UNiagaraScriptSourceBase* Returned=E.Script->GetSource(E.Version);TStrongObjectPtr<UNiagaraScriptSourceBase> Immediate(Returned);
        for(const auto& Current:C.AssignmentIdentities)if(!Pure(Current))return false;
        if(Returned!=E.Source)return Bad(E,TEXT("selected_source"),TEXT("public getter returned a different source"));
    }
    R.Data->SetBoolField(TEXT("assignment_authored_targets_defaults_current"),true);return true;
}

bool BatchAdmit(FBatchContext& C,FBatchResult& R,const TCHAR* Stage)
{
    if(C.GuardFailed||!BatchGpuCurrent(C,R,Stage)||!BatchScratchCatalogsCurrent(C,R,Stage))return false;
    if(C.OwnedAuthoringGuard&&C.System)
    {
        if(!(*C.OwnedAuthoringGuard)(C.System,Stage))
        {
            C.GuardFailed=true;R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;R.Stage=Stage;
            if(!R.Data->HasField(TEXT("first_owned_authoring_guard_failure")))R.Data->SetStringField(TEXT("first_owned_authoring_guard_failure"),Stage);
            return BatchFail(R,TEXT("niagara_batch_owned_authoring_guard_failed"),TEXT("GU owner/target/current or VM/GPU quiescence failed; actual ordered prefix retained, no later SDK."));
        }
        BatchCount(R,TEXT("owned_authoring_guard_calls"));
    }
    if(!BatchAssignmentsCurrent(C,R,Stage))return false;
    if(!C.Deferred&&!C.OwnedAuthoringGuard&&!C.RIWriter&&C.ScratchCatalogs.IsEmpty())return BatchGpuCurrent(C,R,Stage);
    if(!BatchPublicationSafe(C)||C.CancelRequested||BatchAsyncStopping.load())
    {
        C.GuardFailed=true;R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;
        R.Stage=Stage;return BatchFail(R,TEXT("niagara_batch_async_context_stopped"),TEXT("Exact Editor/world/GC/exit/owned cancellation admission failed; actual prefix retained, no SDK replay."));
    }
    if(C.GuardFailed)return false;
    if(!C.Captured)return true;
    for(const auto& I:C.Identities)if(!I->Exact())
    {C.GuardFailed=true;R.bChangeStateKnown=false;R.Stage=Stage;return BatchFail(R,TEXT("niagara_batch_async_identity_changed"),TEXT("Captured authoring class/outer/effective package/path changed; no further SDK."));}
    for(const auto& S:C.SourceIdentities)
        if(S.Source->bIsCompilationCopy||S.Source->bIsReleased||S.Source->NodeGraph!=S.Graph)
        {C.GuardFailed=true;R.bChangeStateKnown=false;R.Stage=Stage;return BatchFail(R,TEXT("niagara_batch_async_source_graph_changed"),TEXT("Captured source points at a different graph/released compilation role; old held graph is not current authoring."));}
    for(const auto& G:C.PhysicalGraphs)if(!G->Exact())
    {C.GuardFailed=true;R.bChangeStateKnown=false;R.Stage=Stage;return BatchFail(R,TEXT("niagara_batch_async_physical_graph_changed"),TEXT("Current physical node/pin/link/default membership changed; no raw pin is reused."));}
    for(const auto& S:C.ScriptIdentities)
    {
        if(S.Script->GetExposedVersion().VersionGuid!=S.Version||S.Script->GetSource(S.Version)!=S.Source)
        {C.GuardFailed=true;R.bChangeStateKnown=false;R.Stage=Stage;return BatchFail(R,TEXT("niagara_batch_async_script_source_changed"),TEXT("Captured authoring Script source/version changed; no subsequent SDK."));}
    }

    for(const auto& Call:C.CallIdentities)
        if(Call.Call->FunctionScript.Get()!=Call.Script||Call.Call->SelectedScriptVersion!=Call.Version||Call.Script->GetSource(Call.Version)!=Call.Source)
        {C.GuardFailed=true;R.bChangeStateKnown=false;R.Stage=Stage;return BatchFail(R,TEXT("niagara_batch_async_module_source_changed"),TEXT("Captured module selected version/source changed; no subsequent SDK."));}
    if(C.System->GetSystemSpawnScript()!=C.SystemSpawn||C.System->GetSystemUpdateScript()!=C.SystemUpdate)
        return C.GuardFailed=true,R.bChangeStateKnown=false,BatchFail(R,TEXT("niagara_batch_async_system_script_changed"),TEXT("Current System spawn/update routing differs from its captured owned Scripts."));
    const auto& Handles=C.System->GetEmitterHandles();
    if(Handles.Num()!=C.HandleIdentities.Num())return C.GuardFailed=true,R.bChangeStateKnown=false,BatchFail(R,TEXT("niagara_batch_async_emitter_changed"),TEXT("Captured emitter membership changed."));
    for(int32 I=0;I<Handles.Num();++I)
    {
        const auto& H=Handles[I];const auto& E=C.HandleIdentities[I];
        if(H.GetInstance().Emitter!=E.Emitter||H.GetInstance().Version!=E.Version||H.GetId()!=E.Id||H.GetName()!=E.Name||H.GetIsEnabled()!=E.Enabled)
            return C.GuardFailed=true,R.bChangeStateKnown=false,BatchFail(R,TEXT("niagara_batch_async_emitter_changed"),TEXT("Captured emitter identity/version/order changed."));
        const FVersionedNiagaraEmitterData* D=H.GetEmitterData();
        if(!D||D!=E.Data)
            return C.GuardFailed=true,R.bChangeStateKnown=false,BatchFail(R,TEXT("niagara_batch_async_emitter_data_changed"),TEXT("Current Emitter version-data pointer changed; old data is not dereferenced."));
        if(D->SimTarget!=E.SimTarget||D->UsesInterpolatedSpawning()!=E.Interpolated||D->SpawnScriptProps.Script!=E.Spawn)
            return C.GuardFailed=true,R.bChangeStateKnown=false,BatchFail(R,TEXT("niagara_batch_emitter_routing_changed"),TEXT("Captured emitter SimTarget/interpolation/spawn dependency routing changed across an SDK boundary."));
        if(D->GraphSource.Get()!=E.Source)
            return C.GuardFailed=true,R.bChangeStateKnown=false,BatchFail(R,TEXT("niagara_batch_async_emitter_source_changed"),TEXT("Current Emitter GraphSource differs from its captured authoring source."));
    }
    return BatchGpuCurrent(C,R,Stage);
}
bool BatchCaptureIdentities(FBatchContext& C,FBatchResult& R)
{
    if(!C.Deferred&&!C.OwnedAuthoringGuard&&!C.RIWriter&&C.ScratchCatalogs.IsEmpty())return true;
    FString Role=TEXT("initial_admission");
    UNiagaraScript* CaptureScript=nullptr;UNiagaraScriptSource* CaptureSource=nullptr;
    UNiagaraGraph* HostGraph=nullptr;UEdGraphNode* CaptureNode=nullptr;
    int32 GraphIndex=INDEX_NONE,NodeIndex=INDEX_NONE,PinIndex=INDEX_NONE,NestedSourceRoles=0;
    auto CaptureFail=[&](const TCHAR* Reason,UObject* Object=nullptr)->bool
    {
        if(R.Data->HasField(TEXT("first_identity_capture_failure")))return false;
        auto J=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("first_identity_capture_failure"),J);
        J->SetStringField(TEXT("reason"),Reason);J->SetStringField(TEXT("role"),Role.Left(128));
        J->SetStringField(TEXT("stage"),R.Stage.Left(256));J->SetStringField(TEXT("preserved_error_code"),R.ErrorCode.Left(256));
        J->SetNumberField(TEXT("graph_index"),GraphIndex);J->SetNumberField(TEXT("node_index"),NodeIndex);J->SetNumberField(TEXT("pin_index"),PinIndex);
        J->SetNumberField(TEXT("captured_object_count"),C.Identities.Num());J->SetNumberField(TEXT("captured_source_role_count"),C.SourceIdentities.Num());
        J->SetNumberField(TEXT("captured_physical_graph_count"),C.PhysicalGraphs.Num());
        J->SetNumberField(TEXT("owned_graph_count"),C.Graphs.Num());J->SetNumberField(TEXT("direct_module_source_graph_count"),C.SourceGraphs.Num());
        // Failure diagnostics never read a rejected UObject or unsafe Editor context.
        const bool Readable=(!C.OwnedAuthoringGuard||!C.GuardFailed)&&BatchPublicationSafe(C);
        auto PlainPath=[&](UObject* O)->FString
        {return Readable&&BatchReadableChain(O)?O->GetPathName().Left(1024):FString(TEXT("unavailable"));};
        J->SetBoolField(TEXT("current_object_observation_available"),Readable&&BatchReadableChain(Object));
        J->SetStringField(TEXT("object_path"),PlainPath(Object));
        J->SetStringField(TEXT("object_class_path"),Readable&&BatchReadableChain(Object)?PlainPath(Object->GetClass()):FString(TEXT("unavailable")));
        J->SetStringField(TEXT("script_path"),PlainPath(CaptureScript));J->SetStringField(TEXT("source_path"),PlainPath(CaptureSource));
        J->SetStringField(TEXT("host_graph_path"),PlainPath(HostGraph));J->SetStringField(TEXT("node_path"),PlainPath(CaptureNode));
        UNiagaraGraph* SourceGraph=Readable&&BatchReadableChain(CaptureSource)?CaptureSource->NodeGraph.Get():nullptr;
        J->SetStringField(TEXT("source_graph_path"),PlainPath(SourceGraph));
        J->SetBoolField(TEXT("source_graph_in_owned_set"),SourceGraph&&C.Graphs.Contains(SourceGraph));
        J->SetBoolField(TEXT("source_graph_in_direct_module_set"),SourceGraph&&C.SourceGraphs.Contains(SourceGraph));
        return false;
    };
    R.Data->SetStringField(TEXT("identity_capture_scope"),TEXT("Physical owned/direct module sources include exact native Assignment-owned generated source graphs with separately preserved ordered authored targets/defaults; nested FunctionCall Script/version/Source/NodeGraph are exact strong routing identities only, without nested raw pin capture or recursive graph-purity proof."));
    if(!BatchAdmit(C,R,TEXT("capture_batch_identities")))return CaptureFail(TEXT("initial_admission_failed"));
    TSet<UObject*> Seen;
    auto Own=[&](UObject* O)->bool
    {
        if(!O)return true;
        if(!BatchReadableChain(O))return CaptureFail(TEXT("object_chain_unreadable"),O);
        if(!Seen.Contains(O)){Seen.Add(O);C.Identities.Add(MakeUnique<FBatchIdentity>(O));}
        return true;
    };
    auto SourceRole=[&](UNiagaraScriptSource* Source,bool RequireObservedGraph)->bool
    {
        CaptureSource=Source;
        if(!Source)return CaptureFail(TEXT("source_missing"));
        if(!Own(Source))return false;
        if(Source->bIsCompilationCopy)return CaptureFail(TEXT("source_is_compilation_copy"),Source);
        if(Source->bIsReleased)return CaptureFail(TEXT("source_is_released"),Source);
        UNiagaraGraph* SourceGraph=Source->NodeGraph.Get();
        if(!SourceGraph)return CaptureFail(TEXT("source_graph_missing"),Source);
        if(!Own(SourceGraph))return false;
        const bool Observed=C.Graphs.Contains(SourceGraph)||C.SourceGraphs.Contains(SourceGraph);
        if(RequireObservedGraph&&!Observed)return CaptureFail(TEXT("source_graph_outside_prepared_first_level_scope"),SourceGraph);
        for(const auto& Existing:C.SourceIdentities)if(Existing.Source==Source)
            return Existing.Graph==SourceGraph?true:CaptureFail(TEXT("source_graph_role_conflict"),SourceGraph);
        C.SourceIdentities.Add({Source,SourceGraph});
        if(!Observed)++NestedSourceRoles;
        return true;
    };
    Role=TEXT("system");if(!Own(C.System))return false;
    C.SystemSpawn=C.System->GetSystemSpawnScript();C.SystemUpdate=C.System->GetSystemUpdateScript();
    for(const auto& H:C.System->GetEmitterHandles())
    {
        Role=TEXT("emitter_source");CaptureScript=nullptr;CaptureSource=nullptr;
        if(!Own(H.GetInstance().Emitter))return false;
        const FVersionedNiagaraEmitterData* D=H.GetEmitterData();
        if(!D)return CaptureFail(TEXT("emitter_data_missing"),H.GetInstance().Emitter);
        auto* Source=Cast<UNiagaraScriptSource>(D->GraphSource);
        if(!SourceRole(Source,true))return false;
        C.HandleIdentities.Add({H.GetInstance().Emitter,H.GetId(),H.GetInstance().Version,H.GetName(),H.GetIsEnabled(),D,Source,D->SimTarget,D->UsesInterpolatedSpawning(),D->SpawnScriptProps.Script});
    }
    for(UNiagaraScript* S:C.Scripts)
    {
        Role=TEXT("owned_script_source");CaptureScript=S;CaptureSource=nullptr;
        if(!S)return CaptureFail(TEXT("owned_script_missing"));
        if(!Own(S))return false;
        const FGuid V=S->GetExposedVersion().VersionGuid;
        auto* Source=Cast<UNiagaraScriptSource>(S->GetSource(V));
        if(!SourceRole(Source,true))return false;
        C.ScriptIdentities.Add({S,Source,V});
    }
    TArray<UNiagaraGraph*> Graphs=C.Graphs;for(auto* G:C.SourceGraphs)Graphs.AddUnique(G);
    for(UNiagaraGraph* G:Graphs)
    {
        ++GraphIndex;NodeIndex=INDEX_NONE;PinIndex=INDEX_NONE;HostGraph=G;CaptureNode=nullptr;CaptureScript=nullptr;CaptureSource=nullptr;
        Role=TEXT("physical_graph");
        if(!G)return CaptureFail(TEXT("physical_graph_missing"));
        if(!Own(G))return false;
        if(G->Nodes.Num()>1024)return CaptureFail(TEXT("physical_graph_node_bound"),G);
        int32 PinCount=0;
        for(UEdGraphNode* N:G->Nodes)
        {
            ++NodeIndex;PinIndex=INDEX_NONE;CaptureNode=N;CaptureScript=nullptr;CaptureSource=nullptr;Role=TEXT("physical_node");
            if(!N)return CaptureFail(TEXT("physical_node_missing"));
            if(!Own(N))return false;
            if(N->Pins.Num()>8192)return CaptureFail(TEXT("physical_node_pin_bound"),N);
            if(auto* Call=Cast<UNiagaraNodeFunctionCall>(N))
            {
                if(Call->FunctionScript)
                {
                    Role=C.Graphs.Contains(G)?TEXT("direct_module_source"):TEXT("nested_call_source_identity");
                    UNiagaraScript* S=Call->FunctionScript.Get();CaptureScript=S;
                    if(!Own(S))return false;
                    auto* Source=Cast<UNiagaraScriptSource>(S->GetSource(Call->SelectedScriptVersion));
                    // CH prepares only direct module-source graphs. Nested callees retain
                    // exact strong Script/version/Source/Graph roles, not an invented
                    // requirement that their graphs were first-level observed as well.
                    if(!SourceRole(Source,C.Graphs.Contains(G)))return false;
                    C.CallIdentities.Add({Call,S,Source,Call->SelectedScriptVersion});
                }
            }
            Role=TEXT("physical_pin");
            for(UEdGraphPin* P:N->Pins)
            {
                ++PinIndex;
                if(!P)return CaptureFail(TEXT("physical_pin_missing"),N);
                if(++PinCount>8192)return CaptureFail(TEXT("physical_graph_pin_bound"),N);
                if(P->GetOwningNodeUnchecked()!=N)return CaptureFail(TEXT("physical_pin_owner_changed"),N);
                if(!Own(P->DefaultObject.Get())||!Own(P->PinType.PinSubCategoryObject.Get())||!Own(P->PinType.PinValueType.TerminalSubCategoryObject.Get()))return false;
            }
        }
        C.PhysicalGraphs.Add(MakeUnique<FBatchGraphIdentity>(G));
    }
    R.Data->SetNumberField(TEXT("assignment_generated_source_count"),C.AssignmentIdentities.Num());
    R.Data->SetBoolField(TEXT("assignment_authored_targets_defaults_current"),true);
    R.Data->SetBoolField(TEXT("assignment_cache_regeneration_invoked"),false);
    R.Data->SetNumberField(TEXT("nested_source_identity_role_count"),NestedSourceRoles);
    C.Captured=true;Role=TEXT("final_capture_admission");CaptureScript=nullptr;CaptureSource=nullptr;HostGraph=nullptr;CaptureNode=nullptr;
    return BatchAdmit(C,R,TEXT("captured_batch_identities"))?true:CaptureFail(TEXT("final_admission_failed"));
}

struct FBatchObservation
{
    bool Complete = false, Dirty = false;
    TSharedRef<FJsonObject> Stores = MakeShared<FJsonObject>(), Graphs = MakeShared<FJsonObject>(), Sources = MakeShared<FJsonObject>(),
        Other = MakeShared<FJsonObject>(), Disk = MakeShared<FJsonObject>(), Handles = MakeShared<FJsonObject>(), Exposed = MakeShared<FJsonObject>();
};

struct FBatchDeferred
{
    FBatchResult R; FBatchContext Context; FBatchObservation Before,After;
    TSharedRef<FJsonObject> Expected=MakeShared<FJsonObject>();
    int32 Applied=0,Failed=0; bool Ready=false,Save=true,Compile=true,Wait=false;
    double Timeout=10.0,Poll=0.1;
};

bool BatchDisk(UNiagaraSystem* S, const TSharedRef<FJsonObject>& J)
{
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(S->GetOutermost()->GetName(), Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    J->SetStringField(TEXT("path"), Filename); const int64 Size = IFileManager::Get().FileSize(*Filename); J->SetBoolField(TEXT("exists"), Size >= 0);
    J->SetNumberField(TEXT("bytes"), Size < 0 ? 0 : Size); if (Size < 0) { J->SetStringField(TEXT("sha1"), TEXT("")); return true; }
    if (Size > 67108864) { return false; } TArray<uint8> Data; if (!FFileHelper::LoadFileToArray(Data, *Filename) || Data.Num() != Size) { return false; }
    FSHAHash Hash; FSHA1::HashBuffer(Data.GetData(), Data.Num(), Hash.Hash); J->SetStringField(TEXT("sha1"), Hash.ToString()); return true;
}

// HZ: returned Named diagnostics only. Full FBatchObservation and preservation inputs stay unchanged.
struct FBatchDiagnosticDigest
{
    FSHA1 Hash;
    uint64 Bytes=0,Values=0;
    bool Complete=true;
    static constexpr uint64 MaxBytes=2*1024*1024,MaxValues=65536;
    bool Add(const void* Data,uint64 Size)
    {
        if(!Complete||Size>MaxBytes-Bytes){Complete=false;return false;}
        Hash.Update(static_cast<const uint8*>(Data),static_cast<uint32>(Size));Bytes+=Size;return true;
    }
    bool String(const FString& Value)
    {
        const int32 Length=Value.Len();return Add(&Length,sizeof(Length))&&Add(*Value,static_cast<uint64>(Length)*sizeof(TCHAR));
    }
    bool Visit(const TSharedPtr<FJsonValue>& Item,int32 Depth=0)
    {
        if(!Complete||!Item.IsValid()||++Values>MaxValues||Depth>32){Complete=false;return false;}
        const uint8 Kind=static_cast<uint8>(Item->Type);if(!Add(&Kind,sizeof(Kind)))return false;
        switch(Item->Type)
        {
        case EJson::Null:return true;
        case EJson::Boolean:{const uint8 B=Item->AsBool()?1:0;return Add(&B,sizeof(B));}
        case EJson::Number:{const double N=Item->AsNumber();return Add(&N,sizeof(N));}
        case EJson::String:return String(Item->AsString());
        case EJson::Array:
        {
            const auto& Items=Item->AsArray();const int32 Count=Items.Num();if(!Add(&Count,sizeof(Count)))return false;
            for(const auto& Child:Items)if(!Visit(Child,Depth+1))return false;return true;
        }
        case EJson::Object:
        {
            const auto Object=Item->AsObject();if(!Object.IsValid()){Complete=false;return false;}
            const int32 Count=Object->Values.Num();if(!Add(&Count,sizeof(Count)))return false;
            if(static_cast<uint64>(Count)>MaxValues-Values){Complete=false;return false;}
            TArray<TPair<FString,TSharedPtr<FJsonValue>>> Ordered;Ordered.Reserve(Count);
            for(const auto& Pair:Object->Values)Ordered.Emplace(FString(Pair.Key.ToView()),Pair.Value);
            Ordered.Sort([](const auto& A,const auto& B){return A.Key.Compare(B.Key,ESearchCase::CaseSensitive)<0;});
            for(const auto& Pair:Ordered)if(!String(Pair.Key)||!Visit(Pair.Value,Depth+1))return false;return true;
        }
        default:Complete=false;return false;
        }
    }
};
TSharedRef<FJsonObject> BatchDiagnosticSummary(const TSharedRef<FJsonObject>& Source)
{
    auto Summary=MakeShared<FJsonObject>();FBatchDiagnosticDigest Digest;
    Digest.Visit(MakeShared<FJsonValueObject>(Source));Digest.Hash.Final();uint8 Raw[FSHA1::DigestSize];Digest.Hash.GetHash(Raw);
    Summary->SetNumberField(TEXT("top_level_field_count"),Source->Values.Num());
    Summary->SetBoolField(TEXT("full_diagnostic_payload_omitted"),true);
    Summary->SetBoolField(TEXT("digest_complete"),Digest.Complete);
    Summary->SetBoolField(TEXT("digest_truncated"),!Digest.Complete);
    Summary->SetNumberField(TEXT("digest_visited_values"),Digest.Values);
    Summary->SetNumberField(TEXT("digest_hashed_bytes"),Digest.Bytes);
    Summary->SetStringField(Digest.Complete?TEXT("digest_sha1"):TEXT("prefix_digest_sha1"),BytesToHex(Raw,FSHA1::DigestSize));
    Summary->SetStringField(TEXT("digest_format"),TEXT("sorted typed JSON tokens/native little-endian TCHAR and numeric bytes v1; diagnostic only, not the preservation comparator"));
    return Summary;
}
struct FBatchDiagnosticScope
{
    bool Deferred;const FBatchObservation& Observation;FBatchResult& Result;FString Phase;
    FBatchDiagnosticScope(bool InDeferred,const FBatchObservation& InObservation,FBatchResult& InResult,const TCHAR* InPhase)
        :Deferred(InDeferred),Observation(InObservation),Result(InResult),Phase(InPhase){}
    ~FBatchDiagnosticScope()
    {
        if(!Deferred)return;
        // Plain retained JSON only, including on an unsafe/partial capture return. No UObject getters or new SDK calls.
        auto Summary=MakeShared<FJsonObject>();Summary->SetBoolField(TEXT("complete"),Observation.Complete);
        Summary->SetBoolField(TEXT("internal_full_observation_complete"),Observation.Complete);
        Summary->SetBoolField(TEXT("returned_diagnostic_is_summary"),true);Summary->SetBoolField(TEXT("full_diagnostic_payload_omitted"),true);
        Summary->SetBoolField(TEXT("package_dirty"),Observation.Dirty);
        Summary->SetObjectField(TEXT("rapid_iteration_stores"),BatchDiagnosticSummary(Observation.Stores));
        Summary->SetObjectField(TEXT("owned_graphs"),BatchDiagnosticSummary(Observation.Graphs));
        Summary->SetObjectField(TEXT("module_sources"),BatchDiagnosticSummary(Observation.Sources));
        Summary->SetObjectField(TEXT("exposed_parameters"),BatchDiagnosticSummary(Observation.Exposed));
        Summary->SetObjectField(TEXT("emitter_handles"),BatchDiagnosticSummary(Observation.Handles));
        Summary->SetObjectField(TEXT("other_package_dirty"),BatchDiagnosticSummary(Observation.Other));
        Summary->SetObjectField(TEXT("disk"),Observation.Disk);
        Result.Data->SetObjectField(Phase,Summary);
        Result.Data->SetBoolField(TEXT("Named_batch_capture_diagnostics_summarized"),true);
        Result.Data->SetBoolField(TEXT("internal_full_batch_preservation_observations_unchanged"),true);
        Result.Data->SetStringField(TEXT("Named_batch_capture_diagnostic_scope"),TEXT("Counts and bounded diagnostic digests replace returned fullgraph/store/source copies only. Full Before/After/Expected, graph/pin/RI/source/other-package preservation comparisons and original ordered results remain in the same core. Digest truncation never changes internal capture completeness."));
    }
};

bool BatchCapture(FBatchContext& C, FBatchObservation& O, FBatchResult& R, const TCHAR* Phase)
{
    if(!BatchAdmit(C,R,Phase))return false;
    O = FBatchObservation{}; BatchCount(R, TEXT("observation_capture_calls")); auto J = MakeShared<FJsonObject>(); if(!C.Deferred)R.Data->SetObjectField(Phase, J); J->SetBoolField(TEXT("complete"), false);
    FBatchDiagnosticScope Diagnostic(C.Deferred,O,R,Phase);
    J->SetObjectField(TEXT("rapid_iteration_stores"), O.Stores); J->SetObjectField(TEXT("owned_graphs"), O.Graphs); J->SetObjectField(TEXT("module_sources"), O.Sources);
    J->SetObjectField(TEXT("exposed_parameters"), O.Exposed); J->SetObjectField(TEXT("emitter_handles"), O.Handles); J->SetObjectField(TEXT("other_package_dirty"), O.Other); J->SetObjectField(TEXT("disk"), O.Disk);
    O.Dirty = C.System->GetOutermost()->IsDirty(); J->SetStringField(TEXT("system_path"), Path(C.System)); J->SetStringField(TEXT("system_class"), Path(C.System->GetClass())); J->SetBoolField(TEXT("package_dirty"), O.Dirty);
    for (auto* S : C.Scripts) { bool OK; auto Row = BatchStore(S->RapidIterationParameters, OK); O.Stores->SetObjectField(Path(S), Row); if (!OK) { return false; } }
    auto GraphPrefix = [](const FSnapshot& S) { auto Nodes = MakeShared<FJsonObject>(), Pins = MakeShared<FJsonObject>(); for (const auto& P : S.Nodes) { Nodes->SetStringField(P.Key,P.Value); } for (const auto& P : S.Pins) { Pins->SetStringField(P.Key,P.Value); } S.Json->SetObjectField(TEXT("captured_node_prefix"),Nodes); S.Json->SetObjectField(TEXT("captured_pin_prefix"),Pins); return S.Json; };
    for (auto* G : C.Graphs) { const auto S = Capture(G); O.Graphs->SetObjectField(Path(G), GraphPrefix(S)); if (!S.bComplete) { return false; } }
    for (auto* G : C.SourceGraphs) { const auto S = Capture(G); auto Row = MakeShared<FJsonObject>(); O.Sources->SetObjectField(Path(G), Row); Row->SetObjectField(TEXT("graph"), GraphPrefix(S)); bool AssignmentCache=false;for(const auto& A:C.AssignmentIdentities)AssignmentCache|=A.Graph==G; const bool LocalScratch=C.ScratchSourceGraphs.Contains(G); if(AssignmentCache||LocalScratch){if(AssignmentCache)Row->SetBoolField(TEXT("assignment_generated_cache"),true);if(LocalScratch)Row->SetBoolField(TEXT("registered_local_scratch_source"),true);Row->SetStringField(TEXT("package_dirty_role"),TEXT("shared target package; actual dirty value is the root observation"));}else Row->SetBoolField(TEXT("package_dirty"), G->GetOutermost()->IsDirty()); if (!S.bComplete) { return false; } }
    bool ExposedOK; const auto Exposed = BatchStore(C.System->GetExposedParameters(), ExposedOK); O.Exposed->Values = Exposed->Values; if (!ExposedOK) { return false; }
    int32 N = 0; for (const auto& H : C.System->GetEmitterHandles())
    { auto Row = MakeShared<FJsonObject>(); O.Handles->SetObjectField(LexToString(N++), Row); Row->SetStringField(TEXT("name"), H.GetName().ToString()); Row->SetStringField(TEXT("id"), Guid(H.GetId())); Row->SetStringField(TEXT("emitter"), Path(H.GetInstance().Emitter)); Row->SetStringField(TEXT("version"), Guid(H.GetInstance().Version)); Row->SetBoolField(TEXT("enabled"), H.GetIsEnabled());if(!C.GpuRoles.IsEmpty()){const auto* Data=H.GetEmitterData();if(!Data)return false;Row->SetNumberField(TEXT("SimTarget"),static_cast<int32>(Data->SimTarget));} }
    int32 Packages = 0; for (TObjectIterator<UPackage> It; It; ++It)
    { if (!IsValid(*It) || *It == C.System->GetOutermost() || (*It)->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)) { continue; } if (++Packages > 8192) { return false; } O.Other->SetBoolField((*It)->GetName(), (*It)->IsDirty()); }
    if (!BatchDisk(C.System, O.Disk) || !BatchAdmit(C,R,Phase)) { return false; } O.Complete = true; J->SetBoolField(TEXT("complete"), true); return true;
}
bool BatchPrepare(FBatchContext& C, FBatchResult& R, const FString& SystemPath, bool Allow, bool ReadPending = false)
{
    const FEngineVersion& V = FEngineVersion::Current(); FString Object;
    if (!IsEditorGameThread() || IsPlaySessionActive() || V.GetMajor() != 5 || V.GetMinor() != 8 || V.GetPatch() != 3)
    { return BatchFail(R, TEXT("niagara_batch_editor_state_invalid"), TEXT("Idle UE5.8.3 Editor game thread is required.")); }
    if (!ObjectPath(SystemPath, Object) || BatchForbidden(Object)) { return BatchFail(R, TEXT("niagara_batch_path_invalid"), TEXT("Bounded native runtime system package/object path is required.")); }
    C.System = FindObject<UNiagaraSystem>(nullptr, *Object);
    if(C.OwnedAuthoringGuard){if(C.System)C.Keep.Emplace(C.System);if(!BatchAdmit(C,R,TEXT("owned_batch_target_resolved")))return false;}
    if (!Loaded(C.System) || C.System->GetClass() != UNiagaraSystem::StaticClass() || !C.System->IsAsset() || !C.System->GetOutermost()->IsFullyLoaded() ||
        C.System->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn) || C.System->GetEmitterHandles().Num() > 16)
    { return BatchFail(R, TEXT("niagara_batch_loaded_system_required"), TEXT("Already-loaded fully loaded exact NiagaraSystem with at most16 ordinary CPU/GPU emitters is required; no hidden warm.")); }
    const FString Package = C.System->GetOutermost()->GetName(); const FString Temp = TEXT("/Game/_MCP_Temp/NiagaraGenerated");
    if (!Package.StartsWith(TEXT("/Game/")) || (!Allow && Package != Temp && !Package.StartsWith(Temp + TEXT("/"))))
    { return BatchFail(R, TEXT("niagara_batch_source_edit_refused"), TEXT("Original temp root or explicit allow_source_edit on /Game is required.")); }
    if (!ReadPending && C.System->HasOutstandingCompilationRequests(true))
    {
        if(!C.OwnedAuthoringGuard)return BatchFail(R, TEXT("niagara_batch_compile_outstanding"), TEXT("Pre-existing compilation is unsupported; no wait, flush or mutation."));
        if(!BatchAdmit(C,R,TEXT("owned_batch_pending_compile_admission")))return false;
        R.Data->SetBoolField(TEXT("GU_owned_pending_authoring_admitted"),true);
        R.Data->SetBoolField(TEXT("pending_request_is_not_compile_completion"),true);
    }
    if(C.OwnedAuthoringGuard&&!BatchAdmit(C,R,TEXT("after_owned_batch_outstanding_query")))return false;
    for (TObjectIterator<UNiagaraComponent> It; It; ++It)
    { if (IsValid(*It) && !(*It)->IsTemplate() && (*It)->GetAsset() == C.System && (*It)->GetWorld()) { return BatchFail(R, TEXT("niagara_batch_world_instance_unsupported"), TEXT("Live world system instances are outside finite authoring scope.")); } }
    auto AddScript = [&](UNiagaraScript* S) { if (!Loaded(S) || S->GetClass() != UNiagaraScript::StaticClass() || !S->IsIn(C.System)) { return false; } C.Scripts.AddUnique(S); return C.Scripts.Num() <= 128; };
    if (!AddScript(C.System->GetSystemSpawnScript()) || !AddScript(C.System->GetSystemUpdateScript())) { return BatchFail(R, TEXT("niagara_batch_script_scope_unsupported"), TEXT("Owned runtime scripts must already be observable.")); }
    int32 EmitterIndex=0;
    for (const auto& H : C.System->GetEmitterHandles())
    {
        auto* D = H.GetEmitterData(); if (!D || !Loaded(H.GetInstance().Emitter) || !H.GetInstance().Emitter->IsIn(C.System) || (D->SimTarget != ENiagaraSimTarget::CPUSim&&D->SimTarget!=ENiagaraSimTarget::GPUComputeSim))
        { return BatchFail(R, TEXT("niagara_batch_emitter_scope_unsupported"), TEXT("Only current owned ordinary CPU/GPU emitter instances are implemented; stateless/external/invalid targets remain valid residuals.")); }
        if(!BatchCaptureGpuRole(C,R,EmitterIndex++,H,D))return false;
        TArray<UNiagaraScript*> Scripts; D->GetScripts(Scripts, false, false); if(!BatchGpuCurrent(C,R,TEXT("after_owned_emitter_GetScripts")))return false; for (auto* S : Scripts) { if (!AddScript(S)) { return BatchFail(R, TEXT("niagara_batch_script_scope_unsupported"), TEXT("Emitter script owner/count incomplete.")); } }
        auto* Source = Cast<UNiagaraScriptSource>(D->GraphSource); if (!Loaded(Source) || !Source->IsIn(C.System) || !Loaded(Source->NodeGraph) || Source->bIsCompilationCopy || Source->bIsReleased)
        { return BatchFail(R, TEXT("niagara_batch_graph_scope_unsupported"), TEXT("Ordinary existing owned emitter graph source required.")); } C.Graphs.AddUnique(Source->NodeGraph);
    }
    for (auto* S : C.Scripts)
    { if (auto* Source = Cast<UNiagaraScriptSource>(S->GetLatestSource()); Loaded(Source) && Loaded(Source->NodeGraph) && Source->IsIn(C.System)) { C.Graphs.AddUnique(Source->NodeGraph); } }
    if (C.Graphs.Num() > 32) { return BatchFail(R, TEXT("niagara_batch_graph_scope_unsupported"), TEXT("Owned graph count exceeds32.")); }
    UClass* Schema = FindObject<UClass>(nullptr, TEXT("/Script/NiagaraEditor.EdGraphSchema_Niagara"));
    if (!IsValid(Schema) || Schema != UEdGraphSchema_Niagara::StaticClass() || !IsValid(Schema->GetDefaultObject(false))) { return BatchFail(R, TEXT("niagara_batch_schema_unloaded"), TEXT("Already existing native Niagara schema CDO is required.")); }
    for (auto* G : C.Graphs) for (UEdGraphNode* Node : G->Nodes)
    {
        auto* Call = Cast<UNiagaraNodeFunctionCall>(Node);
        if(Call&&Call->GetClass()==UNiagaraNodeAssignment::StaticClass())
        {
            // Native generated Script ownership differs from authored private scratch.
            if(!BatchLive(G)||!BatchReadableChain(G)||!G->IsIn(C.System)||!BatchLive(Call)||!BatchReadableChain(Call)||Call->GetOuter()!=G||!G->Nodes.Contains(Call)||!Call->FunctionScript.IsResolved())return BatchFail(R,TEXT("niagara_batch_assignment_host_unobservable"),TEXT("Exact live Assignment/owned host/resolved Script slot required before capture."));
            C.Keep.Emplace(C.System);C.Keep.Emplace(G);C.Keep.Emplace(Call);auto* Script=Call->FunctionScript.Get();C.Keep.Emplace(Script);
            if(!BatchLive(Script)||!BatchReadableChain(Script)||Script->GetClass()!=UNiagaraScript::StaticClass()||Script->GetOuter()!=Call||Script->GetUsage()!=ENiagaraScriptUsage::Module)return BatchFail(R,TEXT("niagara_batch_assignment_script_unobservable"),TEXT("Actual generated native Module Script must be owned by its Assignment node."));
            if(!C.Editor.IsValid()){C.Editor=GEditor;C.World=GEditor->GetEditorWorldContext().World();C.Keep.Emplace(GEditor);if(C.World.IsValid())C.Keep.Emplace(C.World.Get());}
            if(!BatchPublicationSafe(C))return BatchFail(R,TEXT("niagara_batch_assignment_context_unavailable"),TEXT("Captured Editor/world required before generated source observation."));
            FBatchAssignmentIdentity E;E.Node=static_cast<UNiagaraNodeAssignment*>(Call);E.HostGraph=G;E.Guid=Call->NodeGuid;E.NodePath=Path(Call);E.Script=Script;E.Version=Call->SelectedScriptVersion;E.Exposed=Script->GetExposedVersion().VersionGuid;E.Data=Script->GetScriptData(E.Version);
            E.Targets=E.Node->GetAssignmentTargets();E.Defaults=E.Node->GetAssignmentDefaults();
            if(!E.Data||E.Targets.Num()>2048||E.Targets.Num()!=E.Defaults.Num())return BatchFail(R,TEXT("niagara_batch_assignment_authored_unobservable"),TEXT("Bounded ordered actual Assignment targets/defaults and selected version data required."));
            for(const auto& Target:E.Targets){const auto& Type=Target.GetType();if(!Type.IsValid()||Target.GetAllocatedSizeInBytes()>4096)return BatchFail(R,TEXT("niagara_batch_assignment_target_unobservable"),TEXT("Valid native target type and bounded copied payload required."));for(UObject* TypeObject:{static_cast<UObject*>(Type.GetStruct()),static_cast<UObject*>(Type.GetEnum())})if(TypeObject){TStrongObjectPtr<UObject> Immediate(TypeObject);if(!BatchLive(TypeObject)||!BatchReadableChain(TypeObject))return BatchFail(R,TEXT("niagara_batch_assignment_type_unavailable"),TEXT("Captured Assignment type source is not live."));E.TypeObjects.AddUnique(TypeObject);C.Keep.Emplace(TypeObject);}}
            if(!BatchAdmit(C,R,TEXT("before_assignment_generated_source")))return false;
            auto ReturnedSourceFail=[&](const TCHAR* Code,const TCHAR* Message){C.GuardFailed=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;R.Stage=TEXT("capture_assignment_generated_source");return BatchFail(R,Code,Message);};
            UNiagaraScriptSourceBase* Returned=Script->GetSource(E.Version);TStrongObjectPtr<UNiagaraScriptSourceBase> Immediate(Returned);C.Keep.Emplace(Returned);
            if(!BatchAdmit(C,R,TEXT("after_assignment_generated_source")))return false;
            if(!BatchPublicationSafe(C))return ReturnedSourceFail(TEXT("niagara_batch_assignment_context_changed"),TEXT("Editor/world changed during source getter; no further object observer."));
            if(!BatchLive(Call)||!BatchReadableChain(Call)||!Call->FunctionScript.IsResolved()||Call->FunctionScript.Get()!=Script||Call->SelectedScriptVersion!=E.Version||!BatchLive(Script)||!BatchReadableChain(Script)||Script->GetOuter()!=Call||Script->GetScriptData(E.Version)!=E.Data)return ReturnedSourceFail(TEXT("niagara_batch_assignment_source_changed_during_capture"),TEXT("Assignment/Script changed during its source getter; no replacement baseline."));
            if(!BatchLive(Returned)||!BatchReadableChain(Returned))return ReturnedSourceFail(TEXT("niagara_batch_assignment_source_return_unavailable"),TEXT("Public source getter returned null/incomplete source; no replacement/readback."));
            auto* Source=Cast<UNiagaraScriptSource>(Returned);if(!BatchLive(Source)||!BatchReadableChain(Source)||Source->GetClass()!=UNiagaraScriptSource::StaticClass()||Source->GetOuter()!=Script||Source->bIsCompilationCopy||Source->bIsReleased||!Source->NodeGraph.IsResolved())return ReturnedSourceFail(TEXT("niagara_batch_assignment_source_unobservable"),TEXT("Actual generated native Source must belong to the Assignment Script."));
            auto* GeneratedGraph=Source->NodeGraph.Get();C.Keep.Emplace(GeneratedGraph);if(!BatchLive(GeneratedGraph)||!BatchReadableChain(GeneratedGraph)||GeneratedGraph->GetClass()!=UNiagaraGraph::StaticClass()||GeneratedGraph->GetOuter()!=Source||GeneratedGraph->GetPackage()!=C.System->GetPackage()||GeneratedGraph->GetOutermost()!=C.System->GetOutermost()||!GeneratedGraph->Schema.GetGCPtr().IsResolved()||GeneratedGraph->Schema.GetGCPtr().Get()!=UEdGraphSchema_Niagara::StaticClass()||C.SourceGraphs.Num()>=128)return ReturnedSourceFail(TEXT("niagara_batch_assignment_graph_unobservable"),TEXT("Actual generated native graph must belong to its Source within existing first-level bounds."));
            E.Source=Source;E.Graph=GeneratedGraph;C.SourceGraphs.AddUnique(GeneratedGraph);C.AssignmentIdentities.Add(MoveTemp(E));
            if(!BatchAssignmentsCurrent(C,R,TEXT("assignment_generated_source_captured")))return false;
            R.Data->SetNumberField(TEXT("assignment_generated_source_count"),C.AssignmentIdentities.Num());
            R.Data->SetBoolField(TEXT("assignment_cache_regeneration_invoked"),false);
            continue;
        }
        if (!Call || !Call->FunctionScript) { continue; }
        if(Call->FunctionScript->IsIn(C.System)){if(!BatchPrepareScratchSource(C,R,Call))return false;continue;}
        auto* Script = Call->FunctionScript.Get(); auto* Source = Cast<UNiagaraScriptSource>(Script->GetSource(Call->SelectedScriptVersion));
        if (!Loaded(Script) || BatchForbidden(Path(Script)) || !Loaded(Source) || !Loaded(Source->NodeGraph) || Source->bIsCompilationCopy || Source->bIsReleased || C.SourceGraphs.Num() >= 128)
        { return BatchFail(R, TEXT("niagara_batch_module_source_unobservable"), TEXT("Bounded already-loaded external native module graph is required.")); }
        C.SourceGraphs.AddUnique(Source->NodeGraph); C.Keep.Emplace(Script); C.Keep.Emplace(Source);
    }
    C.Keep.Emplace(C.System); for (auto* S : C.Scripts) { C.Keep.Emplace(S); } for (auto* G : C.Graphs) { C.Keep.Emplace(G); } for (auto* G : C.SourceGraphs) { C.Keep.Emplace(G); } return true;
}
TSharedRef<FJsonObject> BatchWaitPoint(UNiagaraSystem* S,double Timeout,double Poll,FBatchResult& R)
{
    BatchCount(R,TEXT("compile_wait_observation_calls")); auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("wait_for_completion"),false); J->SetNumberField(TEXT("timeout_seconds"),Timeout); J->SetNumberField(TEXT("poll_interval_seconds"),Poll);
    J->SetBoolField(TEXT("wait_timed_out"),false); J->SetNumberField(TEXT("wait_iterations"),0); J->SetNumberField(TEXT("wait_elapsed_seconds"),0); J->SetBoolField(TEXT("outstanding_compilation_requests_after"),S->HasOutstandingCompilationRequests(true)); J->SetBoolField(TEXT("active_compilations_after"),S->HasActiveCompilations()); return J;
}
TSharedRef<FJsonObject> BatchCompileState(FBatchContext& C, TOptional<bool>& Compiled,FBatchResult& R)
{
    BatchCount(R,TEXT("compile_readiness_observation_calls"));
    auto J = MakeShared<FJsonObject>(); auto Rows = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("script_statuses"), Rows);
    const bool Outstanding = C.System->HasOutstandingCompilationRequests(true), Active = C.System->HasActiveCompilations(), Needs = C.System->NeedsRequestCompile(); bool Good = true, Error = false;
    J->SetBoolField(TEXT("outstanding_compilation_requests_after"), Outstanding); J->SetBoolField(TEXT("active_compilations_after"), Active); J->SetBoolField(TEXT("needs_request_compile"), Needs);
    auto Inline = MakeShared<FJsonObject>(), Compileable = MakeShared<FJsonObject>(), Usages = MakeShared<FJsonObject>();
    J->SetObjectField(TEXT("inlined_emitter_script_statuses"), Inline); J->SetObjectField(TEXT("compileable_script_statuses"), Compileable);
    J->SetObjectField(TEXT("script_usages"), Usages); int32 Considered = 0;
    for (auto* S : C.Scripts)
    {
        BatchCount(R,TEXT("script_compile_status_query_count")); const auto Status = S->GetLastCompileStatus();
        const auto U = S->GetUsage(); const FString Key = Path(S); Rows->SetNumberField(Key, static_cast<int32>(Status));
        Usages->SetStringField(Key, UsageName(U));
        // Public usage classifier follows the installed official collector:
        // these bodies compile inside the system scripts, not independently.
        if (U == ENiagaraScriptUsage::EmitterSpawnScript || U == ENiagaraScriptUsage::EmitterUpdateScript)
        { Inline->SetNumberField(Key, static_cast<int32>(Status)); continue; }
        ++Considered; Compileable->SetNumberField(Key, static_cast<int32>(Status));
        Error |= Status == ENiagaraScriptCompileStatus::NCS_Error;
        Good &= Status == ENiagaraScriptCompileStatus::NCS_UpToDate || Status == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings;
    }
    J->SetNumberField(TEXT("compileable_script_count"), Considered); J->SetNumberField(TEXT("inlined_emitter_script_count"), Inline->Values.Num());
    J->SetStringField(TEXT("classification_scope"), TEXT("All owned scripts remain in raw status and RI observations; only EmitterSpawn/EmitterUpdate are excluded from standalone compile status, following installed public Niagara GetSystemCompileState source. Uses public GetUsage/GetLastCompileStatus; private EnumerateSystemScripts is never called. No compile-event/runtime proof or implicit completion advancement."));
    Compiled.Reset(); if (!Outstanding && !Active && !Needs && Considered > 0 && (Good || Error)) { Compiled = Good && !Error; }
    J->SetBoolField(TEXT("ready_to_save"), !Outstanding && !Active && !Needs); J->SetBoolField(TEXT("explicit_poll_or_wait_invoked"), false);
    J->SetField(TEXT("compiled"), Compiled.IsSet() ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueBoolean>(Compiled.GetValue())) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>())); return J;
}
// Installed NiagaraCompilationTasks.cpp:115-159 FindDependentScripts and
// NiagaraStackFunctionInputBinder.cpp:258-275 SetData: editor RI writes include
// the exact dependent script; RequestCompile(false) alone need not synchronize it.
bool BatchRIDependentScript(FBatchContext& C,UNiagaraScript* Source,int32 EmitterIndex,UNiagaraScript*& Target)
{
    Target=nullptr;const auto& Handles=C.System->GetEmitterHandles();
    if(!Handles.IsValidIndex(EmitterIndex)||!Handles[EmitterIndex].GetIsEnabled())return true;
    const auto& H=Handles[EmitterIndex];auto* D=H.GetEmitterData();
    if(!D||!Source->IsIn(H.GetInstance().Emitter))return false;
    const auto Usage=Source->GetUsage();
    if(UNiagaraScript::IsEquivalentUsage(Usage,ENiagaraScriptUsage::EmitterSpawnScript))Target=C.SystemSpawn;
    else if(UNiagaraScript::IsEquivalentUsage(Usage,ENiagaraScriptUsage::EmitterUpdateScript))Target=C.SystemUpdate;
    else if(D->SimTarget==ENiagaraSimTarget::GPUComputeSim&&
        (UNiagaraScript::IsEquivalentUsage(Usage,ENiagaraScriptUsage::ParticleSpawnScript)||UNiagaraScript::IsEquivalentUsage(Usage,ENiagaraScriptUsage::ParticleUpdateScript)))
    {
        // Public GetScripts result already admitted with exact GPU slot protection.
        for(auto* S:C.Scripts)if(S->IsIn(H.GetInstance().Emitter)&&S->GetUsage()==ENiagaraScriptUsage::ParticleGPUComputeScript)
        {if(Target)return false;Target=S;}
        if(!Target)return false;
    }
    else if(D->SimTarget==ENiagaraSimTarget::CPUSim&&D->UsesInterpolatedSpawning()&&UNiagaraScript::IsEquivalentUsage(Usage,ENiagaraScriptUsage::ParticleUpdateScript))Target=D->SpawnScriptProps.Script;
    return !Target||(Target!=Source&&C.Scripts.Contains(Target));
}
bool BatchPreserved(const FBatchObservation& Before, const FBatchObservation& After, const TSharedRef<FJsonObject>& ExpectedStores, FBatchResult& R, bool Saved)
{
    const auto& Expected=ExpectedStores;
    auto EqualValue=[](const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B){return (!A&&!B)||(A&&B&&BatchEqual(A,B));};
    const bool Stores=BatchEqual(Expected,After.Stores), Graphs=BatchEqual(Before.Graphs,After.Graphs), Sources=BatchEqual(Before.Sources,After.Sources),
        Handles=BatchEqual(Before.Handles,After.Handles), Exposed=BatchEqual(Before.Exposed,After.Exposed), Other=BatchEqual(Before.Other,After.Other);
    if(!Stores&&!R.Data->HasField(TEXT("first_rapid_iteration_mismatch")))
    {
        TArray<FString> Names;for(const auto& P:Expected->Values)Names.Add(FString(P.Key.ToView()));for(const auto& P:After.Stores->Values)Names.AddUnique(FString(P.Key.ToView()));Names.Sort();
        for(const auto& Name:Names)
        {
            auto E=Expected->TryGetField(Name),A=After.Stores->TryGetField(Name);if(EqualValue(E,A))continue;
            auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("stage"),R.Stage);M->SetStringField(TEXT("script"),Name);
            M->SetBoolField(TEXT("expected_store_present"),E.IsValid());M->SetBoolField(TEXT("actual_store_present"),A.IsValid());
            if(E&&A&&E->Type==EJson::Object&&A->Type==EJson::Object)
            {
                auto EP=E->AsObject()->GetObjectField(TEXT("parameters")),AP=A->AsObject()->GetObjectField(TEXT("parameters"));
                TArray<FString> Keys;for(const auto& P:EP->Values)Keys.Add(FString(P.Key.ToView()));for(const auto& P:AP->Values)Keys.AddUnique(FString(P.Key.ToView()));Keys.Sort();
                for(const auto& Key:Keys)if(!EqualValue(EP->TryGetField(Key),AP->TryGetField(Key)))
                {
                    M->SetStringField(TEXT("parameter"),Key);
                    auto Describe=[](const TSharedPtr<FJsonValue>& V){return !V?FString(TEXT("<absent>")):V->Type==EJson::Object?Text(V->AsObject().ToSharedRef()).Left(2048):FString(TEXT("<not_object>"));};
                    M->SetStringField(TEXT("expected_json_prefix"),Describe(EP->TryGetField(Key)));M->SetStringField(TEXT("actual_json_prefix"),Describe(AP->TryGetField(Key)));break;
                }
            }
            R.Data->SetObjectField(TEXT("first_rapid_iteration_mismatch"),M);break;
        }
    }
    const bool Disk = Saved ? After.Disk->GetBoolField(TEXT("exists")) && After.Disk->GetNumberField(TEXT("bytes")) > 0 && Before.Disk->GetStringField(TEXT("path")) == After.Disk->GetStringField(TEXT("path")) : BatchEqual(Before.Disk, After.Disk);
    R.Data->SetBoolField(TEXT("observations_complete"), Before.Complete && After.Complete); R.Data->SetBoolField(TEXT("rapid_iteration_values_verified"), Stores);
    R.Data->SetBoolField(TEXT("owned_graphs_preserved"), Graphs); R.Data->SetBoolField(TEXT("module_sources_preserved"), Sources); R.Data->SetBoolField(TEXT("emitter_handles_preserved"), Handles);
    R.Data->SetBoolField(TEXT("exposed_parameters_preserved"), Exposed); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(Saved ? TEXT("saved_disk_verified") : TEXT("disk_preserved_before_save"), Disk);
    return Before.Complete && After.Complete && Stores && Graphs && Sources && Handles && Exposed && Other && Disk;
}
bool BatchChanged(const FBatchObservation& A,const FBatchObservation& B)
{ return A.Dirty != B.Dirty || !BatchEqual(A.Stores,B.Stores) || !BatchEqual(A.Graphs,B.Graphs) || !BatchEqual(A.Sources,B.Sources) || !BatchEqual(A.Handles,B.Handles) || !BatchEqual(A.Exposed,B.Exposed) || !BatchEqual(A.Other,B.Other) || !BatchEqual(A.Disk,B.Disk); }
void BatchClearProof(FBatchResult& R)
{ for (const TCHAR* K : {TEXT("observations_complete"), TEXT("rapid_iteration_values_verified"), TEXT("owned_graphs_preserved"), TEXT("module_sources_preserved"), TEXT("emitter_handles_preserved"), TEXT("exposed_parameters_preserved"), TEXT("other_package_dirty_preserved"), TEXT("disk_preserved_before_save"), TEXT("saved_disk_verified")}) { R.Data->SetBoolField(K, false); } }
bool BatchScalar(const TSharedPtr<FJsonValue>& V, float& F)
{ double N; if (!V || V->Type != EJson::Number || !V->TryGetNumber(N) || !FMath::IsFinite(N) || N < -MAX_flt || N > MAX_flt) { return false; } F = static_cast<float>(N); return FMath::IsFinite(F); }
TSharedPtr<FJsonValue> BatchReadValue(const FNiagaraVariable& V)
{
    const auto& T = V.GetType(); if (!V.IsDataAllocated()) { return MakeShared<FJsonValueNull>(); }
    if (T == FNiagaraTypeDefinition::GetFloatDef() && T.GetSize() == sizeof(float)) { const float X = V.GetValue<float>(); if (!FMath::IsFinite(X)) { return nullptr; } return MakeShared<FJsonValueNumber>(X); }
    if (T == FNiagaraTypeDefinition::GetIntDef() && T.GetSize() == sizeof(int32)) { return MakeShared<FJsonValueNumber>(V.GetValue<int32>()); }
    if (T == FNiagaraTypeDefinition::GetBoolDef() && T.GetSize() == sizeof(FNiagaraBool)) { return MakeShared<FJsonValueBoolean>(V.GetValue<FNiagaraBool>().GetValue()); }
    if (T.IsEnum() && T.GetEnum() && T.GetSize() == sizeof(FNiagaraInt32))
    { const int32 I = V.GetValue<FNiagaraInt32>().Value, Index = T.GetEnum()->GetIndexByValue(I); if (Index != INDEX_NONE) { return MakeShared<FJsonValueString>(T.GetEnum()->GetNameStringByIndex(Index)); } return MakeShared<FJsonValueNumber>(I); }
    TArray<TSharedPtr<FJsonValue>> A; auto Numbers = [&](std::initializer_list<float> Nums) -> TSharedPtr<FJsonValue> { for (float X : Nums) { if (!FMath::IsFinite(X)) { return nullptr; } A.Add(MakeShared<FJsonValueNumber>(X)); } return MakeShared<FJsonValueArray>(A); };
    if (T == FNiagaraTypeDefinition::GetVec2Def() && T.GetSize() == sizeof(FVector2f)) { const auto X = V.GetValue<FVector2f>(); return Numbers({X.X,X.Y}); }
    if (T == FNiagaraTypeDefinition::GetVec3Def() && T.GetSize() == sizeof(FVector3f)) { const auto X = V.GetValue<FVector3f>(); return Numbers({X.X,X.Y,X.Z}); }
    if (T == FNiagaraTypeDefinition::GetVec4Def() && T.GetSize() == sizeof(FVector4f)) { const auto X = V.GetValue<FVector4f>(); return Numbers({X.X,X.Y,X.Z,X.W}); }
    if (T == FNiagaraTypeDefinition::GetColorDef() && T.GetSize() == sizeof(FLinearColor)) { const auto X = V.GetValue<FLinearColor>(); return Numbers({X.R,X.G,X.B,X.A}); }
    return nullptr;
}
TSharedPtr<FJsonValue> BatchValue(FNiagaraVariable& V, const TSharedPtr<FJsonValue>& Input, FString& Error)
{
    const auto& T = V.GetType(); float F; double N; bool B;
    if (T == FNiagaraTypeDefinition::GetFloatDef() && T.GetSize() == sizeof(float)) { if (!BatchScalar(Input, F)) { Error = TEXT("Expected finite float value"); return nullptr; } V.SetValue<float>(F); return MakeShared<FJsonValueNumber>(F); }
    if (T == FNiagaraTypeDefinition::GetIntDef() && T.GetSize() == sizeof(int32))
    { if (!Input || Input->Type != EJson::Number || !Input->TryGetNumber(N) || !FMath::IsFinite(N) || N < MIN_int32 || N > MAX_int32) { Error = TEXT("Expected finite int32-range numeric value"); return nullptr; } const int32 I = static_cast<int32>(N); V.SetValue<int32>(I); return MakeShared<FJsonValueNumber>(I); }
    if (T == FNiagaraTypeDefinition::GetBoolDef() && T.GetSize() == sizeof(FNiagaraBool)) { if (!Input || Input->Type != EJson::Boolean || !Input->TryGetBool(B)) { Error = TEXT("Expected Boolean value"); return nullptr; } V.SetValue<FNiagaraBool>(FNiagaraBool(B)); return MakeShared<FJsonValueBoolean>(B); }
    if (T.IsEnum() && Loaded(T.GetEnum()) && T.GetSize() == sizeof(FNiagaraInt32) && !BatchForbidden(Path(T.GetEnum())))
    {
        const auto* E = T.GetEnum(); int32 Found = INDEX_NONE; if (E->NumEnums() > 4096) { Error = TEXT("Enum count exceeds4096"); return nullptr; }
        if (Input && Input->Type == EJson::Number && Input->TryGetNumber(N) && FMath::IsFinite(N) && N == FMath::TruncToDouble(N) && N >= MIN_int32 && N <= MAX_int32) { Found = E->GetIndexByValue(static_cast<int32>(N)); }
        else if (Input && Input->Type == EJson::String)
        { const FString S = Input->AsString().TrimStartAndEnd(); int32 Matches = 0; if (!Bounded(S, 256) || S.IsEmpty()) { Error = TEXT("Invalid enum name"); return nullptr; } for (int32 I = 0; I < E->NumEnums(); ++I) { if (E->GetNameStringByIndex(I).Equals(S, ESearchCase::IgnoreCase) || E->GetNameByIndex(I).ToString().Equals(S, ESearchCase::IgnoreCase) || E->GetAuthoredNameStringByIndex(I).Equals(S, ESearchCase::IgnoreCase) || E->GetDisplayNameTextByIndex(I).ToString().Equals(S, ESearchCase::IgnoreCase)) { Found = I; ++Matches; } } if (Matches != 1) { Found = INDEX_NONE; } }
        if (Found == INDEX_NONE || E->HasMetaData(TEXT("Hidden"), Found) || E->GetNameStringByIndex(Found).EndsWith(TEXT("_MAX"), ESearchCase::IgnoreCase) || E->GetNameStringByIndex(Found).Equals(TEXT("MAX"), ESearchCase::IgnoreCase) || E->GetValueByIndex(Found) < MIN_int32 || E->GetValueByIndex(Found) > MAX_int32) { Error = TEXT("Invalid/hidden/terminal enum value"); return nullptr; }
        FNiagaraInt32 I; I.Value = static_cast<int32>(E->GetValueByIndex(Found)); V.SetValue<FNiagaraInt32>(I); return MakeShared<FJsonValueNumber>(I.Value);
    }
    const TArray<TSharedPtr<FJsonValue>>* A = nullptr; TArray<float> Values;
    if (!Input || !Input->TryGetArray(A) || A->Num() > 16) { Error = TEXT("Expected bounded numeric vector/color array"); return nullptr; }
    for (const auto& E : *A) { if (!BatchScalar(E, F)) { Error = TEXT("Vector/color element must be finite float"); return nullptr; } Values.Add(F); }
    TArray<TSharedPtr<FJsonValue>> Actual; auto Numbers = [&](std::initializer_list<float> Nums) { for (float X : Nums) { Actual.Add(MakeShared<FJsonValueNumber>(X)); } return MakeShared<FJsonValueArray>(Actual); };
    if (T == FNiagaraTypeDefinition::GetVec2Def() && T.GetSize() == sizeof(FVector2f) && Values.Num() >= 2) { V.SetValue<FVector2f>({Values[0],Values[1]}); return Numbers({Values[0],Values[1]}); }
    if (T == FNiagaraTypeDefinition::GetVec3Def() && T.GetSize() == sizeof(FVector3f) && Values.Num() >= 3) { V.SetValue<FVector3f>({Values[0],Values[1],Values[2]}); return Numbers({Values[0],Values[1],Values[2]}); }
    if (T == FNiagaraTypeDefinition::GetVec4Def() && T.GetSize() == sizeof(FVector4f) && Values.Num() >= 4) { V.SetValue<FVector4f>(FVector4f(Values[0],Values[1],Values[2],Values[3])); return Numbers({Values[0],Values[1],Values[2],Values[3]}); }
    if (T == FNiagaraTypeDefinition::GetColorDef() && T.GetSize() == sizeof(FLinearColor) && Values.Num() >= 3) { const float Alpha = Values.Num() >= 4 ? Values[3] : 1.0f; V.SetValue<FLinearColor>({Values[0],Values[1],Values[2],Alpha}); return Numbers({Values[0],Values[1],Values[2],Alpha}); }
    Error = TEXT("Unsupported scalar RapidIteration type/shape; position LWC, arbitrary structs, object and DataInterface remain valid residuals"); return nullptr;
}
bool BatchSelectorIndex(const TSharedRef<FJsonObject>& Edit, const TCHAR* Key, int32& Out)
{ Out = INDEX_NONE; double N; if (!Edit->TryGetNumberField(Key, N)) { return true; } if (!FMath::IsFinite(N) || N < -1 || N > MAX_int32) { return false; } Out = static_cast<int32>(N); return true; }
bool BatchSelect(FBatchContext& C, const TSharedRef<FJsonObject>& Edit, UNiagaraNodeFunctionCall*& Module, UNiagaraScript*& Script, FNiagaraVariable& Input,
    FString& EmitterName, int32& EmitterIndex, int32& ModuleIndex, FString& Error, FBatchResult& R)
{
    FString EN, MN, MG, InputName; Edit->TryGetStringField(TEXT("emitter_name"), EN); Edit->TryGetStringField(TEXT("module_name"), MN); Edit->TryGetStringField(TEXT("module_node_guid"), MG);
    int32 EI, MI; if (!Edit->TryGetStringField(TEXT("input_name"), InputName) || !Bounded(InputName,256) || !Bounded(EN,256) || !Bounded(MN,256) || !Bounded(MG,128) || !BatchSelectorIndex(Edit,TEXT("emitter_index"),EI) || !BatchSelectorIndex(Edit,TEXT("module_index"),MI)) { Error = TEXT("Missing/bounded input_name or invalid emitter/module selector"); return false; }
    int32 Matches = 0, I = 0; FVersionedNiagaraEmitter Owner; FVersionedNiagaraEmitterData* Data = nullptr;
    for (const auto& H : C.System->GetEmitterHandles())
    {
        const FString Name = H.GetName().ToString(); if ((EI == INDEX_NONE || EI == I) && (EN.IsEmpty() || EN.Equals(Name,ESearchCase::IgnoreCase)))
        {
            auto* D = H.GetEmitterData(); auto* Source = D ? Cast<UNiagaraScriptSource>(D->GraphSource) : nullptr; TArray<UNiagaraNodeFunctionCall*> Calls;
            if (Source && Source->NodeGraph) { for (UEdGraphNode* N : Source->NodeGraph->Nodes) { if (auto* Call = Cast<UNiagaraNodeFunctionCall>(N)) { Calls.Add(Call); } } }
            Calls.Sort([](const UNiagaraNodeFunctionCall& A,const UNiagaraNodeFunctionCall& B) { return A.NodePosY == B.NodePosY ? A.NodePosX < B.NodePosX : A.NodePosY < B.NodePosY; });
            for (int32 M = 0; M < Calls.Num(); ++M)
            { auto* Call = Calls[M]; FString NameOfModule = Call->GetFunctionName(); if (NameOfModule.IsEmpty()) { NameOfModule = Call->Signature.GetNameString(); } if ((MI == INDEX_NONE || MI == M) && (MN.IsEmpty() || MN.Equals(NameOfModule,ESearchCase::IgnoreCase)) && (MG.IsEmpty() || MG.Equals(Guid(Call->NodeGuid),ESearchCase::IgnoreCase))) { ++Matches; Module = Call; Data = D; Owner = H.GetInstance(); EmitterName = Name; EmitterIndex = I; ModuleIndex = M; } }
        } ++I;
    }
    if (Matches != 1 || !Loaded(Module) || !Data) { Error = Matches ? TEXT("Ambiguous module selector; original first-match variants remain unsupported") : TEXT("No matching Niagara module was found"); return false; }
    const ENiagaraScriptUsage U = FNiagaraStackGraphUtilities::GetOutputNodeUsage(*Module); TArray<UNiagaraScript*> Scripts; Data->GetScripts(Scripts,false,false); Matches = 0;
    for (auto* S : Scripts) { if (S && S->IsEquivalentUsage(U)) { Script = S; ++Matches; } } if (Matches != 1 || !C.Scripts.Contains(Script)) { Error = TEXT("Owning script usage must be unique/current"); return false; }
    if(!BatchAdmit(C,R,TEXT("before_stack_input_query"))){Error=R.Error;return false;}
    FCompileConstantResolver Resolver(Owner,U,Module->DebugState); TArray<FNiagaraVariable> Inputs; TSet<FNiagaraVariable> Hidden; BatchCount(R,TEXT("stack_input_query_calls"));
    FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Module,Inputs,Hidden,Resolver,FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,true);
    if(!BatchAdmit(C,R,TEXT("after_stack_input_query"))){Error=R.Error;return false;}
    if (Inputs.Num() > 2048) { Error = TEXT("Module input count exceeds2048"); return false; } Matches = 0;
    for (const auto& V : Inputs) { FString Short = V.GetName().ToString(); Short.RemoveFromStart(TEXT("Module.")); if (V.GetName().ToString().Equals(InputName,ESearchCase::IgnoreCase) || Short.Equals(InputName,ESearchCase::IgnoreCase)) { Input = V; ++Matches; } }
    if (Matches != 1) { Error = TEXT("No unique matching module input was found"); return false; } return true;
}
}
static Variable::FResult RunSetInputsBatch(const FString& SystemPath, const FString& EditsJson, const FString& Operation, bool Overwrite,
    bool Continue, bool Allow, bool Save, bool Compile, bool Wait, double Timeout, double Poll, FBatchDeferred* Deferred,
    const TFunction<bool(UNiagaraSystem*,const TCHAR*)>* OwnedAuthoringGuard=nullptr)
{
    FBatchResult LocalR; FBatchResult& R=Deferred?Deferred->R:LocalR; R.bCompileRequested = Compile; R.bSaveRequested = Save;
    R.Data->SetStringField(TEXT("system_path"),SystemPath); R.Data->SetStringField(TEXT("write_scope"),TEXT("batch_rapid_iteration_module_inputs")); R.Data->SetStringField(TEXT("default_operation"),Operation);
    R.Data->SetBoolField(TEXT("continue_on_error"),Continue); R.Data->SetBoolField(TEXT("overwrite_existing"),Overwrite); R.Data->SetBoolField(TEXT("allow_source_edit"),Allow);
    R.Data->SetBoolField(TEXT("full_legacy_parity"),false); R.Data->SetBoolField(TEXT("explicit_global_async_pump_invoked"),false); R.Data->SetBoolField(TEXT("compile_requested"),false); R.Data->SetBoolField(TEXT("saved"),false);
    R.Data->SetBoolField(TEXT("save_deferred"),false); R.Data->SetArrayField(TEXT("save_calls"),{}); R.Data->SetArrayField(TEXT("asset_load_calls"),{}); R.Data->SetArrayField(TEXT("results"),{});
    R.Data->SetStringField(TEXT("observation_scope"),TEXT("Point observations of bounded RapidIteration/exposed stores, graph topology/pins/module source projections, handles, exact target disk and loaded other-package dirty flags. No exhaustive UProperty/cache/world/runtime/cook proof."));
    R.Data->SetBoolField(TEXT("implicit_engine_work_measured"),false); R.Data->SetBoolField(TEXT("original_blocking_wait_implemented"),false);
    for (const TCHAR* K : {TEXT("requested_count"),TEXT("processed_count"),TEXT("applied_count"),TEXT("failed_count"),TEXT("outer_transaction_calls"),TEXT("asset_load_count"),TEXT("observation_capture_calls"),TEXT("explicit_modify_calls"),TEXT("system_modify_calls"),TEXT("script_modify_calls"),TEXT("parameter_set_attempt_count"),TEXT("parameter_set_return_count"),TEXT("parameter_set_returned_true_count"),TEXT("mark_package_dirty_calls"),TEXT("explicit_compile_requests"),TEXT("save_attempt_count"),TEXT("stack_input_query_calls"),TEXT("explicit_poll_calls"),TEXT("explicit_wait_iterations"),TEXT("compile_wait_observation_calls"),TEXT("compile_readiness_observation_calls"),TEXT("script_compile_status_query_count")}) { R.Data->SetNumberField(K,0); }
    BatchClearProof(R); TArray<TSharedPtr<FJsonValue>> Edits;
    if (!Bounded(Operation,128) || EditsJson.Len() > 1048576 || !FMath::IsFinite(Timeout) || !FMath::IsFinite(Poll) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(EditsJson),Edits) || Edits.Num() < 1 || Edits.Num() > 128)
    { BatchFail(R,TEXT("niagara_batch_input_invalid"),TEXT("Nonempty bounded JSON edit array1..128 and finite caller wait values required.")); return R; }
    R.Data->SetNumberField(TEXT("requested_count"),Edits.Num()); R.Data->SetNumberField(TEXT("requested_timeout_seconds"),Timeout); R.Data->SetNumberField(TEXT("requested_poll_interval_seconds"),Poll); R.Data->SetBoolField(TEXT("wait_for_completion_requested"),Wait);
    Timeout = FMath::Clamp(Timeout,0.0,60.0); Poll = FMath::Clamp(Poll,0.02,1.0); R.Data->SetNumberField(TEXT("timeout_seconds"),Timeout); R.Data->SetNumberField(TEXT("poll_interval_seconds"),Poll);
    int32 JsonCount = 0; for (const auto& Edit : Edits) { if (!BatchJsonBound(Edit,0,JsonCount)) { BatchFail(R,TEXT("niagara_batch_input_bound_exceeded"),TEXT("Edit JSON depth32/value16384/string8192/key1024 finite bounds exceeded before asset work.")); return R; } }
    if (Wait && !Deferred) { BatchFail(R,TEXT("niagara_batch_wait_unsupported"),TEXT("Original blocking Poll/global AssetCompilingManager/sleep wait remains incomplete; no writer dispatched for wait_for_completion=True.")); return R; }
    FBatchContext LocalC; FBatchContext& C=Deferred?Deferred->Context:LocalC;
    if(OwnedAuthoringGuard)
    {
        if(Deferred||Save||Compile||Wait||Continue||!Allow){BatchFail(R,TEXT("niagara_batch_owned_authoring_options_invalid"),TEXT("Only a synchronous GU child with no compile/save/wait/continue and explicit target permission may borrow the owner guard."));return R;}
        if(!BatchSafe()){R.bChangeStateKnown=false;BatchFail(R,TEXT("niagara_batch_owned_authoring_context_unavailable"),TEXT("No live Editor context for a borrowed GU authoring call."));return R;}
        C.OwnedAuthoringGuard=OwnedAuthoringGuard;C.Editor=GEditor;C.World=GEditor->GetEditorWorldContext().World();if(C.World.IsValid())C.Keep.Emplace(C.World.Get());
        R.Data->SetBoolField(TEXT("GU_owned_authoring_child"),true);
    }
    if(!BatchAdmit(C,R,TEXT("before_batch_prepare"))||!BatchPrepare(C,R,SystemPath,Allow,Deferred&&Wait))return R;
    C.RIWriter=true;
    if(!C.Editor.IsValid())
    {
        if(!BatchSafe()||!BatchLive(GEditor)){BatchFail(R,TEXT("niagara_batch_writer_context_unavailable"),TEXT("Safe Editor context required before capturing synchronous RI writer identities."));return R;}
        C.Editor=GEditor;C.World=GEditor->GetEditorWorldContext().World();C.Keep.Emplace(GEditor);if(C.World.IsValid())C.Keep.Emplace(C.World.Get());
    }
    if(!BatchCaptureIdentities(C,R)){R.bChangeStateKnown=false;BatchFail(R,TEXT("niagara_batch_identity_capture_failed"),TEXT("Exact authoring identity/physical pin capture failed before row writers."));return R;}
    FBatchObservation LocalBefore,LocalAfter; FBatchObservation& Before=Deferred?Deferred->Before:LocalBefore; FBatchObservation& After=Deferred?Deferred->After:LocalAfter;
    if (!BatchCapture(C,Before,R,TEXT("before_observation"))) { R.bChangeStateKnown = false; BatchFail(R,TEXT("niagara_batch_observation_incomplete"),TEXT("Complete bounded baseline required before writers.")); return R; }
    if (BatchForbidden(Text(Before.Graphs)) || BatchForbidden(Text(Before.Sources)) || BatchForbidden(Text(Before.Stores)) || BatchForbidden(Text(Before.Exposed)))
    { BatchFail(R,TEXT("niagara_batch_reference_unsupported"),TEXT("Selected asset graph/store/source references contain a bridge dependency; global dirty map is retained separately.")); return R; }
    auto Expected = BatchCopy(Before.Stores); TArray<TSharedPtr<FJsonValue>> Results,Loads; int32 Applied = 0,Failed = 0; bool GuardFailed = false;
    TUniquePtr<FScopedTransaction> Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP","NiagaraModuleInputsBatch","MCP set_niagara_module_inputs_batch")); BatchCount(R,TEXT("outer_transaction_calls"));
    auto Load = [&](const TCHAR* Phase)
    { if(!BatchAdmit(C,R,Phase))return false;auto Call = MakeShared<FJsonObject>(); Loads.Add(MakeShared<FJsonValueObject>(Call)); R.Data->SetArrayField(TEXT("asset_load_calls"),Loads); Call->SetStringField(TEXT("phase"),Phase); Call->SetStringField(TEXT("asset_path"),Path(C.System)); Call->SetBoolField(TEXT("attempted"),true); Call->SetBoolField(TEXT("returned"),false); BatchCount(R,TEXT("asset_load_count")); const FString LoadPath=C.System->GetPathName();auto* S = LoadObject<UNiagaraSystem>(nullptr,*LoadPath); Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("returned_nonnull"),S!=nullptr); if(!BatchAdmit(C,R,Phase))return false;Call->SetStringField(TEXT("returned_path"),S==C.System?Path(S):TEXT("unavailable")); return S == C.System; };
    for (int32 I = 0; I < Edits.Num(); ++I)
    {
        if(!BatchAdmit(C,R,TEXT("before_ordered_row"))){GuardFailed=true;break;}
        auto Row = MakeShared<FJsonObject>(); Results.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("results"),Results); R.Data->SetNumberField(TEXT("processed_count"),Results.Num());
        Row->SetNumberField(TEXT("edit_index"),I); Row->SetBoolField(TEXT("success"),false); Row->SetBoolField(TEXT("setter_attempted"),false); Row->SetBoolField(TEXT("setter_returned"),false);
        FString Op = Operation,Error; const auto Edit = Edits[I] && Edits[I]->Type == EJson::Object ? Edits[I]->AsObject() : nullptr;
        if (!Edit) { Error = TEXT("Batch edit entry must be an object"); }
        else
        {
            Edit->TryGetStringField(TEXT("operation"),Op); Row->SetStringField(TEXT("operation"),Op); Row->SetObjectField(TEXT("request"),Edit.ToSharedRef());
            const bool Create = Op.Equals(TEXT("create_override"),ESearchCase::IgnoreCase) || Op.Equals(TEXT("create"),ESearchCase::IgnoreCase) || Op.Equals(TEXT("upsert"),ESearchCase::IgnoreCase);
            const bool Set = Op.Equals(TEXT("set_existing"),ESearchCase::IgnoreCase) || Op.Equals(TEXT("set"),ESearchCase::IgnoreCase); bool Replace = Overwrite; Edit->TryGetBoolField(TEXT("overwrite_existing"),Replace);
            if (!Bounded(Op,128) || (!Create && !Set)) { Error = FString::Printf(TEXT("Unsupported batch operation: %s"),*Op); }
            else if (!Edit->HasField(TEXT("value")) || !Edit->HasField(TEXT("input_name"))) { Error = TEXT("Missing input_name or value"); }
            else if (!Load(TEXT("child"))) { Error = TEXT("Child system load returned a different current object"); GuardFailed = true; R.bChangeStateKnown = false; }
            else
            {
                UNiagaraNodeFunctionCall* Module = nullptr; UNiagaraScript* Script = nullptr; UNiagaraScript* Dependent=nullptr; FNiagaraVariable Input; FString EmitterName; int32 EI = INDEX_NONE,MI = INDEX_NONE;
                if (BatchSelect(C,Edit.ToSharedRef(),Module,Script,Input,EmitterName,EI,MI,Error,R))
                {
                    const FNiagaraParameterHandle Handle(Input.GetName()); const auto ModuleHandle = Handle.IsModuleHandle() ? Handle : FNiagaraParameterHandle::CreateModuleParameterHandle(Input.GetName());
                    const auto Alias = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle,Module); FNiagaraVariable Value = FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(FNiagaraVariable(Input.GetType(),Alias.GetParameterHandleString()),*EmitterName,Script->GetUsage());
                    const uint8* Previous = Value.IsValid() ? Script->RapidIterationParameters.GetParameterData(Value) : nullptr;
                    Row->SetStringField(TEXT("system_path"),Path(C.System)); Row->SetStringField(TEXT("emitter_name"),EmitterName); Row->SetNumberField(TEXT("emitter_index"),EI); Row->SetNumberField(TEXT("module_index"),MI);
                    Row->SetStringField(TEXT("module_name"),Module->GetFunctionName()); Row->SetStringField(TEXT("module_node_guid"),Guid(Module->NodeGuid)); Row->SetStringField(TEXT("input_name"),Input.GetName().ToString()); Row->SetStringField(TEXT("input_type"),Input.GetType().GetNameText().ToString()); Row->SetStringField(TEXT("script_path"),Path(Script)); Row->SetStringField(TEXT("rapid_iteration_name"),Value.GetName().ToString());
                    Row->SetBoolField(TEXT("existed_before"),Previous != nullptr); Row->SetBoolField(TEXT("would_create"),Previous == nullptr); Row->SetField(TEXT("previous_bytes_hex"),Previous ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(BatchBytes(Previous,Value.GetSizeInBytes()))) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
                    FNiagaraVariable PreviousVariable = Value; if (Previous) { PreviousVariable.SetData(Previous); } const auto PreviousJson = Previous ? BatchReadValue(PreviousVariable) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()); Row->SetField(TEXT("previous_value"),PreviousJson ? PreviousJson : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>())); Row->SetBoolField(TEXT("previous_value_observed"),PreviousJson.IsValid());
                    if (!Value.IsValid()) { Error = TEXT("Failed to build RapidIteration parameter"); }
                    else if (Set && !Previous) { Error = TEXT("Existing RapidIteration value not found; set_existing cannot create"); }
                    else if (Create && Previous && !Replace) { Error = TEXT("RapidIteration override already exists; overwrite_existing is required"); }
                    else if (Previous && !PreviousJson) { Error = TEXT("Existing value is outside finite scalar/enum/vector/color observation; raw bytes retained without writer"); }
                    else if (!BatchRIDependentScript(C,Script,EI,Dependent)) { Error=TEXT("Exact current RI dependency routing unavailable; no row writer"); }
                    else if (auto Actual = BatchValue(Value,Edit->TryGetField(TEXT("value")),Error))
                    {
                        Row->SetField(TEXT("new_value"),BatchReadValue(Value)); Row->SetStringField(TEXT("new_bytes_hex"),BatchBytes(Value.GetData(),Value.GetSizeInBytes())); Row->SetBoolField(TEXT("setter_attempted"),true);
                        auto Parameter = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("rapid_iteration_parameter"),Parameter); Parameter->SetStringField(TEXT("name"),Value.GetName().ToString()); Parameter->SetStringField(TEXT("type"),Value.GetType().GetNameText().ToString()); Parameter->SetBoolField(TEXT("type_is_valid"),Value.GetType().IsValid()); Parameter->SetBoolField(TEXT("type_is_data_interface"),Value.GetType().IsDataInterface()); Parameter->SetBoolField(TEXT("type_is_uobject"),Value.GetType().IsUObject()); Parameter->SetBoolField(TEXT("has_data"),Value.IsDataAllocated()); Parameter->SetNumberField(TEXT("data_size_bytes"),Value.GetAllocatedSizeInBytes()); Parameter->SetField(TEXT("value"),BatchReadValue(Value));
                        Row->SetBoolField(TEXT("semantic_value_changed"),!Previous || Row->GetStringField(TEXT("previous_bytes_hex")) != Row->GetStringField(TEXT("new_bytes_hex")));
                        R.Stage = TEXT("set_rapid_iteration_parameter"); R.bMutationAttempted = true; R.bChangeStateKnown = false; BatchClearProof(R);
                        if(!BatchAdmit(C,R,TEXT("before_system_modify"))){GuardFailed=true;break;}
                        BatchCount(R,TEXT("explicit_modify_calls")); BatchCount(R,TEXT("system_modify_calls")); C.System->Modify();
                        if(!BatchAdmit(C,R,TEXT("after_system_modify"))){GuardFailed=true;break;}
                        BatchCount(R,TEXT("explicit_modify_calls")); BatchCount(R,TEXT("script_modify_calls")); Script->Modify();
                        if(!BatchAdmit(C,R,TEXT("after_script_modify"))){GuardFailed=true;break;}
                        BatchCount(R,TEXT("parameter_set_attempt_count")); const bool SetReturned = Script->RapidIterationParameters.SetParameterData(Value.GetData(),Value,Create);
                        BatchCount(R,TEXT("parameter_set_return_count")); Row->SetBoolField(TEXT("setter_returned"),true); Row->SetBoolField(TEXT("set_parameter_returned"),SetReturned); if (SetReturned) { BatchCount(R,TEXT("parameter_set_returned_true_count")); }
                        if(!BatchAdmit(C,R,TEXT("after_parameter_set"))){GuardFailed=true;break;}
                        bool DependentOK=true;
                        if(SetReturned&&Dependent)
                        {
                            auto Call=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("dependent_parameter_write"),Call);
                            Call->SetStringField(TEXT("script_path"),Path(Dependent));Call->SetStringField(TEXT("source_script_path"),Path(Script));
                            Call->SetStringField(TEXT("parameter"),Value.GetName().ToString());Call->SetStringField(TEXT("bytes_hex"),BatchBytes(Value.GetData(),Value.GetSizeInBytes()));
                            const uint8* Old=Dependent->RapidIterationParameters.GetParameterData(Value);Call->SetBoolField(TEXT("existed_before"),Old!=nullptr);
                            Call->SetField(TEXT("previous_bytes_hex"),Old?TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(BatchBytes(Old,Value.GetSizeInBytes()))):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
                            Call->SetBoolField(TEXT("modify_attempted"),true);Call->SetBoolField(TEXT("setter_attempted"),false);Call->SetBoolField(TEXT("setter_returned"),false);
                            BatchCount(R,TEXT("explicit_modify_calls"));BatchCount(R,TEXT("script_modify_calls"));Dependent->Modify();
                            if(!BatchAdmit(C,R,TEXT("after_dependent_script_modify"))){GuardFailed=true;break;}
                            Call->SetBoolField(TEXT("setter_attempted"),true);BatchCount(R,TEXT("parameter_set_attempt_count"));BatchCount(R,TEXT("dependent_parameter_set_attempt_count"));
                            DependentOK=Dependent->RapidIterationParameters.SetParameterData(Value.GetData(),Value,true);
                            BatchCount(R,TEXT("parameter_set_return_count"));if(DependentOK)BatchCount(R,TEXT("parameter_set_returned_true_count"));
                            Call->SetBoolField(TEXT("setter_returned"),true);Call->SetBoolField(TEXT("set_parameter_returned"),DependentOK);
                            if(!BatchAdmit(C,R,TEXT("after_dependent_parameter_set"))){GuardFailed=true;break;}
                        }
                        BatchCount(R,TEXT("mark_package_dirty_calls")); C.System->MarkPackageDirty(); Row->SetBoolField(TEXT("mark_package_dirty_returned"),true);
                        if(!BatchAdmit(C,R,TEXT("after_mark_package_dirty"))){GuardFailed=true;break;}
                        bool StoreOK; const auto ObservedStore = BatchStore(Script->RapidIterationParameters,StoreOK); Row->SetObjectField(TEXT("returned_store"),ObservedStore);
                        auto DesiredStore = BatchCopy(Expected->GetObjectField(Path(Script)).ToSharedRef()); auto Parameters = BatchCopy(DesiredStore->GetObjectField(TEXT("parameters")).ToSharedRef()); auto DesiredValue = MakeShared<FJsonObject>();
                        const FString StoredKey = BatchParameterKey(Parameters,Value.GetName()); DesiredValue->SetStringField(TEXT("name"),StoredKey); DesiredValue->SetStringField(TEXT("type"),Value.GetType().GetNameText().ToString()); DesiredValue->SetStringField(TEXT("struct"),Path(Value.GetType().GetScriptStruct())); DesiredValue->SetStringField(TEXT("enum"),Path(Value.GetType().GetEnum())); DesiredValue->SetNumberField(TEXT("size"),Value.GetSizeInBytes()); DesiredValue->SetStringField(TEXT("bytes_hex"),BatchBytes(Value.GetData(),Value.GetSizeInBytes()));
                        Parameters->SetObjectField(StoredKey,DesiredValue); DesiredStore->SetObjectField(TEXT("parameters"),Parameters); Expected->SetObjectField(Path(Script),DesiredStore);

                        if(SetReturned&&Dependent)
                        {
                            auto Store=BatchCopy(Expected->GetObjectField(Path(Dependent)).ToSharedRef());auto Params=BatchCopy(Store->GetObjectField(TEXT("parameters")).ToSharedRef());
                            const FString Key=BatchParameterKey(Params,Value.GetName());auto V=BatchCopy(DesiredValue);V->SetStringField(TEXT("name"),Key);
                            Params->SetObjectField(Key,V);Store->SetObjectField(TEXT("parameters"),Params);Expected->SetObjectField(Path(Dependent),Store);
                            Row->SetObjectField(TEXT("expected_dependent_store"),Store);
                        }
                        Row->SetObjectField(TEXT("expected_store"),DesiredStore); R.Stage = TEXT("after_edit");
                        const FString Phase = FString::Printf(TEXT("after_edit_%d"),I); const bool Complete = BatchCapture(C,After,R,*Phase);
                        R.bChangeStateKnown = Complete; R.bChanged = Complete && BatchChanged(Before,After);
                        if (!Complete || !StoreOK) { Error = TEXT("Writer returned; bounded current observation incomplete"); GuardFailed = true; R.bChangeStateKnown = false; }
                        else if (!SetReturned || !DependentOK || GuardFailed || !BatchPreserved(Before,After,Expected,R,false)) { Error = TEXT("Known returned writer did not preserve exact scoped parameter/graph/source/disk state"); GuardFailed = true; }
                        else { Row->SetBoolField(TEXT("success"),true); if(Create) { Row->SetBoolField(TEXT("created"),Previous == nullptr); Row->SetBoolField(TEXT("overwrote_existing"),Previous != nullptr); } Row->SetBoolField(TEXT("compile_requested"),false); Row->SetBoolField(TEXT("saved"),false); Row->SetObjectField(TEXT("compile_wait"),BatchWaitPoint(C.System,10.0,0.1,R)); Row->SetStringField(TEXT("write_scope"),Create ? TEXT("new_or_explicitly_overwritten_rapid_iteration_parameter") : TEXT("selected_rapid_iteration_and_exact_engine_dependency_parameters")); ++Applied; }
                    }
                }
            }
        }
        if (!Error.IsEmpty()) { ++Failed; Row->SetStringField(TEXT("error"),Error); } R.Data->SetNumberField(TEXT("applied_count"),Applied); R.Data->SetNumberField(TEXT("failed_count"),Failed);
        if (GuardFailed || (!Error.IsEmpty() && !Continue)) { break; }
    }
    if(Deferred){Transaction.Reset();R.Data->SetBoolField(TEXT("row_transaction_closed_before_yield"),true);if(!BatchAdmit(C,R,TEXT("after_row_transaction"))){GuardFailed=true;}}
    GuardFailed|=C.GuardFailed;
    // Callback guard exits can break before the ordinary row footer. Count the
    // actual completed rows, while retaining every source/dependent writer prefix.
    Applied=Failed=0;
    for(const auto& V:Results)
    {
        auto Row=V->AsObject();if(Row->GetBoolField(TEXT("success")))++Applied;
        else {++Failed;if(GuardFailed&&!Row->HasField(TEXT("error")))Row->SetStringField(TEXT("error"),TEXT("Row interrupted at protected SDK boundary: ")+R.Stage);}
    }
    R.Data->SetNumberField(TEXT("applied_count"),Applied);R.Data->SetNumberField(TEXT("failed_count"),Failed);
    R.Data->SetBoolField(TEXT("partial_success"),Applied > 0 && Failed > 0); R.Data->SetBoolField(TEXT("all_processed_edits_succeeded"),Failed == 0&&!GuardFailed); R.Data->SetObjectField(TEXT("expected_final_rapid_iteration_stores"),Expected);
    if (GuardFailed) { BatchFail(R,TEXT("niagara_batch_preservation_failed"),TEXT("Actual ordered prefix retained; later compile/save suppressed by finite preservation failure. No retry/rollback.")); return R; }
    R.Stage = TEXT("after_edits"); if (!BatchCapture(C,After,R,TEXT("after_edits"))) { R.bChangeStateKnown = false; BatchFail(R,TEXT("niagara_batch_observation_incomplete"),TEXT("Current final edit prefix incomplete; no compile/save.")); return R; }
    R.bChangeStateKnown = true; R.bChanged = BatchChanged(Before,After);
    if (!BatchPreserved(Before,After,Expected,R,false)) { BatchFail(R,TEXT("niagara_batch_preservation_failed"),TEXT("Complete final edit preservation failed; no compile/save.")); return R; }
    if(Deferred)
    {
        Deferred->Expected=Expected;Deferred->Applied=Applied;Deferred->Failed=Failed;Deferred->Save=Save;Deferred->Compile=Compile;Deferred->Wait=Wait;
        Deferred->Timeout=Timeout;Deferred->Poll=Poll;Deferred->Ready=true;
        R.Stage=TEXT("ordered_rows_complete");return R;
    }
    auto WaitReceipt = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("compile_wait"),WaitReceipt); WaitReceipt->SetBoolField(TEXT("wait_for_completion"),false); WaitReceipt->SetNumberField(TEXT("timeout_seconds"),Timeout); WaitReceipt->SetNumberField(TEXT("poll_interval_seconds"),Poll); WaitReceipt->SetBoolField(TEXT("wait_timed_out"),false); WaitReceipt->SetNumberField(TEXT("wait_iterations"),0); WaitReceipt->SetNumberField(TEXT("wait_elapsed_seconds"),0);
    if (Applied > 0)
    {
        if (!Load(TEXT("post_process"))) { R.bChangeStateKnown = false; BatchFail(R,TEXT("niagara_batch_reload_failed"),TEXT("Ordered edit prefix retained; final reload returned a different object.")); return R; }
        if (Compile) { BatchClearProof(R); R.Stage = TEXT("request_compile"); R.bCompileAttempted = true; R.bChangeStateKnown = false; BatchCount(R,TEXT("explicit_compile_requests")); const bool Launched = C.System->RequestCompile(false); R.Data->SetBoolField(TEXT("compile_request_returned"),Launched); R.Data->SetBoolField(TEXT("compile_requested"),true); }
        TOptional<bool> Status; const auto State = BatchCompileState(C,Status,R); State->SetStringField(TEXT("phase"),TEXT("after_compile_request")); R.Data->SetObjectField(TEXT("compile_state"),State); if (Compile) { R.Compiled = Status; }
        WaitReceipt->SetBoolField(TEXT("outstanding_compilation_requests_after"),State->GetBoolField(TEXT("outstanding_compilation_requests_after"))); WaitReceipt->SetBoolField(TEXT("active_compilations_after"),State->GetBoolField(TEXT("active_compilations_after")));
        R.Stage = TEXT("after_compile_request"); if (!BatchCapture(C,After,R,TEXT("after_compile_request"))) { R.bChangeStateKnown = false; BatchFail(R,TEXT("niagara_batch_observation_incomplete"),TEXT("Compile/load returned; current prefix incomplete.")); return R; }
        R.bChangeStateKnown = true; R.bChanged = BatchChanged(Before,After); if (!BatchPreserved(Before,After,Expected,R,false)) { BatchFail(R,TEXT("niagara_batch_compile_preservation_failed"),TEXT("Compile-request changed a scoped state outside exact requested typed RI dependency propagation. Save suppressed.")); return R; }
        if ((Compile || Save) && !State->GetBoolField(TEXT("ready_to_save"))) { R.bChangeStateKnown = false; R.Data->SetBoolField(TEXT("save_deferred"),Save); BatchFail(R,TEXT("niagara_batch_compile_pending"),TEXT("Actual asynchronous request remains pending; no poll/pump/save/retry. Original blocking wait and save-while-pending are incomplete.")); return R; }
        if (Compile && !Status.IsSet()) { R.bChangeStateKnown = false; R.Data->SetBoolField(TEXT("save_deferred"),Save); BatchFail(R,TEXT("niagara_batch_compile_status_unobservable"),TEXT("Returned request has no actual terminal script status; nullable compiled is not healthy and save is suppressed.")); return R; }
        if (Save)
        {
            BatchClearProof(R); R.Compiled.Reset(); R.Stage = TEXT("save"); R.bSaveAttempted = true; R.bChangeStateKnown = false; BatchCount(R,TEXT("save_attempt_count")); auto Call = MakeShared<FJsonObject>(); R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Call)});
            Call->SetStringField(TEXT("asset_path"),Path(C.System)); Call->SetStringField(TEXT("route"),TEXT("UEditorAssetLibrary::SaveLoadedAsset")); Call->SetBoolField(TEXT("only_if_dirty"),false); Call->SetBoolField(TEXT("attempted"),true); Call->SetBoolField(TEXT("returned"),false); Call->SetField(TEXT("saved"),MakeShared<FJsonValueNull>());
            R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(C.System,false); Call->SetBoolField(TEXT("returned"),true); Call->SetBoolField(TEXT("saved"),R.bSaved); R.Data->SetBoolField(TEXT("saved"),R.bSaved);
            R.Stage = TEXT("after_save"); if (!BatchCapture(C,After,R,TEXT("after_save"))) { BatchFail(R,TEXT("niagara_batch_observation_incomplete"),TEXT("Real save prefix retained; current observation incomplete.")); return R; }
            R.bChangeStateKnown = true; R.bChanged = BatchChanged(Before,After); const auto Final = BatchCompileState(C,Status,R); Final->SetStringField(TEXT("phase"),TEXT("after_save")); R.Data->SetObjectField(TEXT("compile_state_after_save"),Final); if (Compile) { R.Compiled = Status; }
            if (!R.bSaved || !BatchPreserved(Before,After,Expected,R,true) || C.System->GetOutermost()->IsDirty() || !Final->GetBoolField(TEXT("ready_to_save"))) { R.bChangeStateKnown = Final->GetBoolField(TEXT("ready_to_save")); BatchFail(R,TEXT("niagara_batch_save_failed"),TEXT("Returned save/final disk and status prefix failed; no second save or writer replay.")); return R; }
            if (Compile && !Status.IsSet()) { R.bChangeStateKnown = false; BatchFail(R,TEXT("niagara_batch_postsave_compile_status_unobservable"),TEXT("Actual save prefix is retained; requested final compiler status is unobservable. No second save or compile retry.")); return R; }
        }
    }
    R.bPostconditionPassed = Failed == 0; if (Compile && Applied > 0) { R.bPostconditionPassed &= R.Compiled.IsSet() && R.Compiled.GetValue(); }
    if (!R.bPostconditionPassed) { BatchFail(R,TEXT("niagara_batch_partial_failure"),TEXT("Ordered edit failures or actual compiler status remain false; any successful edits and final save prefix are retained.")); return R; }
    R.bSuccess = true; R.Stage = TEXT("complete"); return R;
}

Variable::FResult SetInputsBatch(const FString& SystemPath,const FString& EditsJson,const FString& Operation,bool Overwrite,
    bool Continue,bool Allow,bool Save,bool Compile,bool Wait,double Timeout,double Poll,
    const TFunction<bool(UNiagaraSystem*,const TCHAR*)>* OwnedAuthoringGuard)
{
    if(BatchAsyncRunning||BatchAsyncLease)
    {FBatchResult R;R.bCompileRequested=Compile;R.bSaveRequested=Save;BatchFail(R,TEXT("niagara_batch_async_lease_active"),TEXT("An owned batch continuation holds this core; no synchronous row/retry entered."));return R;}
    return RunSetInputsBatch(SystemPath,EditsJson,Operation,Overwrite,Continue,Allow,Save,Compile,Wait,Timeout,Poll,nullptr,OwnedAuthoringGuard);
}
}

// Current RI observation only; never use an edit row to inspect a pending writer.
namespace Cubeless::UE58::NiagaraModule
{
namespace
{
bool BatchDecodedValues(FBatchContext& C, const TSharedRef<FJsonObject>& Out, FBatchResult& R)
{
    for (auto* Script : C.Scripts)
    {
        auto Rows = MakeShared<FJsonObject>(); Out->SetObjectField(Path(Script), Rows);
        TArray<FNiagaraVariable> Variables; Script->RapidIterationParameters.GetParameters(Variables);
        if (Variables.Num() > 2048) { return false; }
        for (const auto& Variable : Variables)
        {
            auto Row = MakeShared<FJsonObject>(); Rows->SetObjectField(Variable.GetName().ToString(), Row);
            Row->SetStringField(TEXT("type"), Variable.GetType().GetNameText().ToString());
            Row->SetBoolField(TEXT("decoded"), false); Row->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
            const auto& Store = Script->RapidIterationParameters;
            const int32 Offset = Store.IndexOf(Variable), Size = Variable.GetSizeInBytes();
            if (Offset < 0 || Size <= 0 || Size > 4096 || Offset > Store.GetParameterDataArray().Num() - Size) { return false; }
            FNiagaraVariable Copy = Variable;
            // Public SetData affects only the stack-local value copy, never the store.
            Copy.SetData(Store.GetParameterDataArray().GetData() + Offset);
            BatchCount(R, TEXT("decoded_value_projection_calls"));
            const auto Value = BatchReadValue(Copy);
            if (Value.IsValid()) { Row->SetField(TEXT("value"), Value); Row->SetBoolField(TEXT("decoded"), true); }
            // Unsupported non-scalar types retain exact bytes in the separate
            // complete store snapshot; null here is not a stored-value claim.
        }
    }
    return true;
}
}

Variable::FResult InspectRapidIteration(const FString& SystemPath)
{
    FBatchResult R;
    R.Data->SetStringField(TEXT("system_path"), SystemPath);
    R.Data->SetBoolField(TEXT("read_only"), true);
    R.Data->SetBoolField(TEXT("explicit_poll_or_wait_invoked"), false);
    R.Data->SetBoolField(TEXT("implicit_engine_work_measured"), false);
    R.Data->SetStringField(TEXT("observation_scope"), TEXT("Two point observations of the same already-loaded finite CPU system's all owned RapidIteration stores/graphs/module sources/handles/exposed data, exact target disk and other loaded package dirty flags. Inline emitter scripts remain in these observations and raw statuses; only standalone compilation roles enter the compile classifier. No explicit asset load, Modify, transaction, parameter write, RequestCompile, poll/pump/wait or save. Observed compile state is descriptive, not runtime/cold/persistence proof."));
    for (const TCHAR* Key : { TEXT("observation_capture_calls"), TEXT("compile_readiness_observation_calls"),
        TEXT("script_compile_status_query_count"), TEXT("decoded_value_projection_calls"), TEXT("asset_load_count"),
        TEXT("outer_transaction_calls"), TEXT("explicit_modify_calls"), TEXT("parameter_set_attempt_count"),
        TEXT("parameter_set_return_count"), TEXT("mark_package_dirty_calls"), TEXT("explicit_compile_requests"),
        TEXT("explicit_poll_calls"), TEXT("explicit_wait_iterations"), TEXT("save_attempt_count") })
    { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("asset_load_calls"), {}); R.Data->SetArrayField(TEXT("save_calls"), {});
    BatchClearProof(R);
    FBatchContext C;
    // Read supports existing in-flight state without waiting or advancing it.
    // /Game path/source guard and all other finite authoring scope remain exact.
    if (!BatchPrepare(C, R, SystemPath, true, true)) { return R; }
    FBatchObservation Before, After;
    R.Stage = TEXT("before_observation");
    if (!BatchCapture(C, Before, R, TEXT("before_observation")))
    { R.bChangeStateKnown = false; BatchFail(R, TEXT("niagara_rapid_iteration_observation_incomplete"), TEXT("Initial bounded read prefix incomplete; no writer/load/compile/save dispatched.")); return R; }
    auto Decoded = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("decoded_rapid_iteration_values"), Decoded);
    R.Data->SetBoolField(TEXT("decoded_value_projection_complete"), false);
    if (!BatchDecodedValues(C, Decoded, R))
    { R.bChangeStateKnown = false; BatchFail(R, TEXT("niagara_rapid_iteration_projection_incomplete"), TEXT("Current typed RI read prefix incomplete; raw initial stores retained, no writer dispatched.")); return R; }
    R.Data->SetBoolField(TEXT("decoded_value_projection_complete"), true);
    TOptional<bool> Status;
    const auto Compile = BatchCompileState(C, Status, R); Compile->SetStringField(TEXT("phase"), TEXT("read_only_current"));
    R.Data->SetObjectField(TEXT("compile_state"), Compile); R.Compiled = Status;
    R.Stage = TEXT("after_observation");
    if (!BatchCapture(C, After, R, TEXT("after_observation")))
    { R.bChangeStateKnown = false; BatchFail(R, TEXT("niagara_rapid_iteration_observation_incomplete"), TEXT("Read and compile-status prefix retained; final observation incomplete, no mutation or save.")); return R; }
    R.bChangeStateKnown = true; R.bChanged = BatchChanged(Before, After);
    const bool Maps = BatchPreserved(Before, After, Before.Stores, R, false);
    const bool Dirty = Before.Dirty == After.Dirty;
    R.Data->SetBoolField(TEXT("package_dirty_preserved"), Dirty);
    R.Data->SetBoolField(TEXT("read_state_preserved"), Maps && Dirty);
    if (!Maps || !Dirty)
    { BatchFail(R, TEXT("niagara_rapid_iteration_state_changed"), TEXT("Complete point observations differed during read; actual drift retained, no explicit writer dispatched.")); return R; }
    // A complete pending/error status read can succeed. compiled is nullable;
    // no pending state is relabeled complete and no result triggers a save.
    R.bPostconditionPassed = true; R.bSuccess = true; R.Stage = TEXT("complete"); return R;
}
}


// HX: same ordered RI batch core; only its post-row tail crosses GT ticks.
namespace Cubeless::UE58::NiagaraModuleInputsBatchAsync
{
using namespace Cubeless::UE58::NiagaraModule;
namespace
{
enum class EPhase:uint8 { Rows,Reload,Compile,Wait,Save,Final,Terminal,Quarantined };
TSharedPtr<FJobState> ActiveBatch;
TMap<const UCubelessNiagaraModuleInputsBatchJob*,FString> BatchSnapshotCache;
TMap<const UCubelessNiagaraModuleInputsBatchJob*,TWeakObjectPtr<UCubelessNiagaraModuleInputsBatchJob>> BatchCacheOwners;
TArray<TSharedPtr<FJobState>>& UnsafeBatchRecords(){static auto* V=new TArray<TSharedPtr<FJobState>>;return *V;}
bool BatchOuterJson(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>2*1024*1024+16384)return false;
    bool String=false,Escape=false;int32 Depth=0;
    for(TCHAR Ch:Text)
    {
        if(Ch==0)return false;
        if(String){if(Escape)Escape=false;else if(Ch=='\\')Escape=true;else if(Ch=='"')String=false;}
        else if(Ch=='"')String=true;
        else if(Ch=='{'||Ch=='['){if(++Depth>32)return false;}
        else if(Ch=='}'||Ch==']'){if(--Depth<0)return false;}
    }
    if(Depth||String)return false;
    auto Reader=TJsonReaderFactory<>::Create(Text);EJsonNotation Token;int32 Tokens=0;
    TSet<FString> Keys;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Tokens>128)return false;
        if(Token!=EJsonNotation::ObjectStart&&Token!=EJsonNotation::ObjectEnd)
        {const FString Key=Reader->GetIdentifier();if(Keys.Contains(Key))return false;Keys.Add(Key);}
    }
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out)||!Out.IsValid()||Out->Values.Num()!=11)return false;
    for(const auto& P:Out->Values)
        if(P.Value->Type==EJson::Number&&!FMath::IsFinite(P.Value->AsNumber()))return false;
    return true;
}
}
struct FJobState:public TSharedFromThis<FJobState>
{
    FBatchDeferred Core;
    TStrongObjectPtr<UCubelessNiagaraModuleInputsBatchJob> Job{nullptr};
    TUniquePtr<FBatchIdentity> JobIdentity;
    UCubelessNiagaraModuleInputsBatchJob* RawJob=nullptr;
    TSharedPtr<FJsonObject> Params;
    FString Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),LastSnapshot;
    FString SystemPath,EditsJson,Operation=TEXT("set_existing");
    bool Overwrite=false,Continue=false,Allow=false,Save=true,Compile=true,Wait=false;
    double Timeout=10.0,Poll=0.1,WaitStart=0.0,NextDue=0.0;
    int32 WaitIterations=0;int64 Revision=0;uint64 LastFrame=MAX_uint64;
    bool Executing=false,Dispatched=false,FinalEncoded=false,WaitStarted=false;
    EPhase Phase=EPhase::Rows;
    FTSTicker::FDelegateHandle Ticker;FDelegateHandle PreExit;
    TSharedRef<FJsonObject> WaitReceipt=MakeShared<FJsonObject>();
    FJobState(){Core.Context.Deferred=true;}
    bool PublicationSafe()const
    {
        return BatchPublicationSafe(Core.Context)&&(!JobIdentity||JobIdentity->Exact());
    }
    void Unhook()
    {
        if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}
        if(PreExit.IsValid()){FCoreDelegates::OnEnginePreExit.Remove(PreExit);PreExit.Reset();}
    }
    void Metadata(bool Terminal)
    {
        auto& R=Core.R;auto& D=R.Data;
        double Modifies=0;D->TryGetNumberField(TEXT("explicit_modify_calls"),Modifies);
        const bool Persistent=Modifies>0||R.bCompileAttempted||R.bSaveAttempted;
        D->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),Persistent);
        D->SetBoolField(TEXT("transient_creation_only"),!Persistent);
        D->SetBoolField(TEXT("SDK_global_purity_unverified"),true);
        D->SetStringField(TEXT("native_job_id"),Id);
        D->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
        D->SetBoolField(TEXT("native_job_terminal"),Terminal);D->SetBoolField(TEXT("operation_complete"),Terminal);
        D->SetBoolField(TEXT("async_core_dispatched"),Dispatched);
        D->SetBoolField(TEXT("automatic_retry_permitted"),false);
        D->SetBoolField(TEXT("do_not_retry"),true);
        D->SetBoolField(TEXT("saved"),R.bSaved);
        D->SetBoolField(TEXT("original_blocking_wait_implemented"),false);
        D->SetBoolField(TEXT("finite_GT_wait_implemented"),true);
        D->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);
        D->SetBoolField(TEXT("timeout_does_not_imply_compile_success"),true);
        D->SetStringField(TEXT("continuation_scope"),TEXT("Existing ordered RI rows under one closed transaction, then same-system reload, optional RequestCompile, independent bounded finite-GT wait and optional forced SaveLoadedAsset(false). Row failures do not suppress this tail when applied_count>0. No global pump/Sleep/replay; SDK calls can block internally."));
    }
    void Cache(const FString& Encoded,bool Terminal,bool ObjectSafe)
    {
        LastSnapshot=Encoded;if(RawJob)BatchSnapshotCache.Add(RawJob,Encoded);
        if(ObjectSafe&&Job.IsValid()){Job->SnapshotJson=Encoded;Job->bIsTerminal=Terminal;}
    }
    void Quarantine(const FString& Reason)
    {
        if(Phase==EPhase::Quarantined)return;
        Unhook();Core.Context.CancelRequested=true;Core.Context.CancelReason=Reason;
        Core.R.bSuccess=false;Core.R.bPostconditionPassed=false;Core.R.bChangeStateKnown=false;
        if(Core.R.Error.IsEmpty())BatchFail(Core.R,TEXT("niagara_batch_async_context_quarantined"),*Reason);
        Core.R.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);
        Core.R.Data->SetBoolField(TEXT("native_writers_stopped"),true);
        Phase=EPhase::Quarantined;Metadata(false);
        Cache(Operations::EncodeVariableResult(Core.R,TEXT("set_niagara_module_inputs_batch")),false,false);
        UnsafeBatchRecords().AddUnique(AsShared());
        // Unsafe context keeps the plain snapshot, strong holds and shared native lease.
    }
    void Pending()
    {
        if(!PublicationSafe()){Quarantine(TEXT("Batch pending publication lost exact Editor/world or Job identity."));return;}
        Core.R.bSuccess=false;Core.R.bPostconditionPassed=false;Metadata(false);
        Cache(Operations::EncodeVariableResult(Core.R,TEXT("set_niagara_module_inputs_batch")),false,true);
    }
    bool Admit(const TCHAR* Stage){return BatchAdmit(Core.Context,Core.R,Stage);}
    bool Observe(const TCHAR* Stage,bool Saved)
    {
        if(!Admit(Stage))return false;
        const bool Complete=BatchCapture(Core.Context,Core.After,Core.R,Stage);
        Core.R.bChangeStateKnown=Complete;
        if(Complete)Core.R.bChanged|=BatchChanged(Core.Before,Core.After);
        if(!Complete||!BatchPreserved(Core.Before,Core.After,Core.Expected,Core.R,Saved))
        {
            Core.R.bChangeStateKnown=false;
            if(Core.R.Error.IsEmpty())BatchFail(Core.R,TEXT("niagara_batch_async_preservation_failed"),TEXT("Actual row/compile/save prefix retained; bounded store/authoring graph/source/exposed/disk preservation failed. No replay."));
            return false;
        }
        return true;
    }
    void Finalize()
    {
        if(FinalEncoded||Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return;
        if(!PublicationSafe()){Quarantine(TEXT("Batch terminal publication lost its captured Editor/world or Job."));return;}
        if(Core.R.Error.IsEmpty()&&Core.Context.Captured&&!Admit(TEXT("batch_final_admission"))){}
        if(Core.Ready&&!Core.Context.GuardFailed&&!Core.Context.CancelRequested&&!BatchAsyncStopping.load())
            Observe(TEXT("batch_final_scoped_observation"),Core.R.bSaved);
        if(!Core.R.Error.IsEmpty()){Core.R.bSuccess=false;Core.R.bPostconditionPassed=false;}
        Metadata(true);
        const FString Encoded=Operations::EncodeVariableResult(Core.R,TEXT("set_niagara_module_inputs_batch"));
        if(!PublicationSafe()){Quarantine(TEXT("Batch final encoding lost safe publication context."));return;}
        FinalEncoded=true;Cache(Encoded,true,true);Phase=EPhase::Terminal;Unhook();
    }
    void FinishRows()
    {
        Core.R.bPostconditionPassed=Core.Failed==0;
        if(!Core.R.bPostconditionPassed)
            BatchFail(Core.R,TEXT("niagara_batch_partial_failure"),TEXT("Ordered edit failures retained with any successful edits and actual compile/wait/final save prefix."));
        else {Core.R.bSuccess=true;Core.R.Stage=TEXT("complete");}
        Phase=EPhase::Final;
    }
    bool Reload()
    {
        if(!Admit(TEXT("before_batch_final_reload")))return false;
        auto& R=Core.R;auto& C=Core.Context;
        const TArray<TSharedPtr<FJsonValue>>* Old=nullptr;TArray<TSharedPtr<FJsonValue>> Loads;
        if(R.Data->TryGetArrayField(TEXT("asset_load_calls"),Old)&&Old)Loads=*Old;
        auto Call=MakeShared<FJsonObject>();Loads.Add(MakeShared<FJsonValueObject>(Call));R.Data->SetArrayField(TEXT("asset_load_calls"),Loads);
        const FString PathToLoad=C.System->GetPathName();
        Call->SetStringField(TEXT("phase"),TEXT("post_process"));Call->SetStringField(TEXT("asset_path"),PathToLoad);
        Call->SetBoolField(TEXT("attempted"),true);Call->SetBoolField(TEXT("returned"),false);BatchCount(R,TEXT("asset_load_count"));
        UNiagaraSystem* Returned=LoadObject<UNiagaraSystem>(nullptr,*PathToLoad);
        Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("returned_nonnull"),Returned!=nullptr);
        if(!Admit(TEXT("after_batch_final_reload")))return false;
        if(Returned!=C.System){R.bChangeStateKnown=false;return BatchFail(R,TEXT("niagara_batch_reload_failed"),TEXT("Final reload returned a different object; no compile/save replay."));}
        Call->SetStringField(TEXT("returned_path"),PathToLoad);return Observe(TEXT("after_batch_final_reload"),false);
    }
    void ReadCompilePoint(const TCHAR* Stage)
    {
        TOptional<bool> Status;auto State=BatchCompileState(Core.Context,Status,Core.R);State->SetStringField(TEXT("phase"),Stage);
        State->SetBoolField(TEXT("explicit_poll_or_wait_invoked"),Wait);
        Core.R.Data->SetObjectField(TEXT("compile_state"),State);
        if(Compile)Core.R.Compiled=Status;
        WaitReceipt->SetBoolField(TEXT("outstanding_compilation_requests_after"),State->GetBoolField(TEXT("outstanding_compilation_requests_after")));
        WaitReceipt->SetBoolField(TEXT("active_compilations_after"),State->GetBoolField(TEXT("active_compilations_after")));
    }
    bool WaitTick()
    {
        auto& R=Core.R;auto& C=Core.Context;
        if(!WaitStarted)
        {
            WaitStarted=true;WaitStart=FPlatformTime::Seconds();NextDue=WaitStart;
            R.Data->SetObjectField(TEXT("compile_wait"),WaitReceipt);
            WaitReceipt->SetBoolField(TEXT("wait_for_completion"),Wait);
            WaitReceipt->SetNumberField(TEXT("timeout_seconds"),Core.Timeout);WaitReceipt->SetNumberField(TEXT("poll_interval_seconds"),Core.Poll);
            WaitReceipt->SetBoolField(TEXT("wait_timed_out"),false);WaitReceipt->SetNumberField(TEXT("wait_iterations"),0);WaitReceipt->SetNumberField(TEXT("wait_elapsed_seconds"),0);
        }
        const double Now=FPlatformTime::Seconds();if(Wait&&Now<NextDue)return false;
        if(!Admit(TEXT("before_batch_wait_status")))return true;
        bool Busy=C.System->HasOutstandingCompilationRequests(true)||C.System->HasActiveCompilations();
        if(Wait&&Busy)
        {
            // Original busy -> Poll -> guarded re-observation -> idle -> deadline, including timeout zero.
            R.Stage=TEXT("wait_compile");BatchCount(R,TEXT("explicit_poll_calls"));R.bChangeStateKnown=false;
            C.System->PollForCompilationComplete();
            R.Data->SetBoolField(TEXT("last_poll_returned"),true);
            if(!Admit(TEXT("after_batch_wait_poll")))return true;
            if(!Observe(TEXT("after_batch_wait_poll"),false))return true;
            Busy=C.System->HasOutstandingCompilationRequests(true)||C.System->HasActiveCompilations();
        }
        if(!Admit(TEXT("batch_wait_readback")))return true;
        ReadCompilePoint(TEXT("after_compile_wait"));
        const double Elapsed=FPlatformTime::Seconds()-WaitStart;
        WaitReceipt->SetNumberField(TEXT("wait_elapsed_seconds"),Elapsed);
        if(!Wait||!Busy)return true;
        if(Elapsed>=Core.Timeout){WaitReceipt->SetBoolField(TEXT("wait_timed_out"),true);return true;}
        ++WaitIterations;BatchCount(R,TEXT("explicit_wait_iterations"));
        WaitReceipt->SetNumberField(TEXT("wait_iterations"),WaitIterations);
        NextDue=FPlatformTime::Seconds()+Core.Poll;return false;
    }
    bool SaveOnce()
    {
        auto& R=Core.R;auto& C=Core.Context;
        if(!Admit(TEXT("before_batch_forced_save")))return false;
        BatchClearProof(R);R.Stage=TEXT("save");R.bSaveAttempted=true;R.bChangeStateKnown=false;R.Compiled.Reset();
        BatchCount(R,TEXT("save_attempt_count"));auto Call=MakeShared<FJsonObject>();
        R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Call)});
        Call->SetStringField(TEXT("asset_path"),C.System->GetPathName());Call->SetStringField(TEXT("route"),TEXT("UEditorAssetLibrary::SaveLoadedAsset"));
        Call->SetBoolField(TEXT("only_if_dirty"),false);Call->SetBoolField(TEXT("attempted"),true);Call->SetBoolField(TEXT("returned"),false);Call->SetField(TEXT("saved"),MakeShared<FJsonValueNull>());
        const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(C.System,false);
        R.bSaved|=Saved;Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("saved"),Saved);R.Data->SetBoolField(TEXT("saved"),R.bSaved);
        // Returned SavedTrue is authoritative before any post-SDK admission/getter.
        if(!Admit(TEXT("after_batch_forced_save")))return false;
        if(!Observe(TEXT("after_save"),Saved))return false;
        ReadCompilePoint(TEXT("after_save"));
        if(!Saved||C.System->GetOutermost()->IsDirty())
            return BatchFail(R,TEXT("niagara_batch_save_failed"),TEXT("Actual forced save returned false or target remains dirty; no second save or replay."));
        return true;
    }
    void Execute()
    {
        if(Core.Context.CancelRequested||BatchAsyncStopping.load())
        {
            Core.Context.CancelRequested=true;Core.R.bChangeStateKnown=false;
            BatchFail(Core.R,TEXT("niagara_batch_async_cancelled"),Core.Context.CancelReason.IsEmpty()?TEXT("Owned batch job stopped; no recovery/replay."):*Core.Context.CancelReason);
        }
        if(!Core.R.Error.IsEmpty()){Phase=EPhase::Final;Finalize();return;}
        if(!Admit(TEXT("batch_async_phase_admission"))){Phase=EPhase::Final;Finalize();return;}
        if(Core.Ready&&Phase!=EPhase::Rows&&Phase!=EPhase::Final&&!Observe(TEXT("before_batch_tail_phase"),Core.R.bSaved))
        {Phase=EPhase::Final;Finalize();return;}
        if(Phase==EPhase::Rows)
        {
            if(Dispatched){BatchFail(Core.R,TEXT("niagara_batch_async_replay_forbidden"),TEXT("Ordered rows already dispatched."));Phase=EPhase::Final;Finalize();return;}
            Dispatched=true;
            RunSetInputsBatch(SystemPath,EditsJson,Operation,Overwrite,Continue,Allow,Save,Compile,Wait,Timeout,Poll,&Core);
            if(Core.Ready)
            {
                Core.R.Data->SetObjectField(TEXT("compile_wait"),WaitReceipt);
                WaitReceipt->SetBoolField(TEXT("wait_for_completion"),false);
                WaitReceipt->SetNumberField(TEXT("timeout_seconds"),Core.Timeout);WaitReceipt->SetNumberField(TEXT("poll_interval_seconds"),Core.Poll);
                WaitReceipt->SetBoolField(TEXT("wait_timed_out"),false);WaitReceipt->SetNumberField(TEXT("wait_iterations"),0);WaitReceipt->SetNumberField(TEXT("wait_elapsed_seconds"),0);
            }
            if(!Core.Ready||!Core.R.Error.IsEmpty())Phase=EPhase::Final;
            else if(Core.Applied<=0)FinishRows();
            else Phase=EPhase::Reload;
        }
        else if(Phase==EPhase::Reload){if(Reload())Phase=EPhase::Compile;else Phase=EPhase::Final;}
        else if(Phase==EPhase::Compile)
        {
            if(Compile)
            {
                BatchClearProof(Core.R);Core.R.Stage=TEXT("request_compile");Core.R.bCompileAttempted=true;Core.R.bChangeStateKnown=false;Core.R.Compiled.Reset();
                BatchCount(Core.R,TEXT("explicit_compile_requests"));
                const bool Launched=Core.Context.System->RequestCompile(false);
                Core.R.Data->SetBoolField(TEXT("compile_request_returned"),Launched);Core.R.Data->SetBoolField(TEXT("compile_requested"),true);
                if(!Admit(TEXT("after_batch_compile_request"))||!Observe(TEXT("after_compile_request"),false))Phase=EPhase::Final;
                else Phase=EPhase::Wait;
            }
            else Phase=EPhase::Wait;
        }
        else if(Phase==EPhase::Wait){if(WaitTick()){if(!Core.R.Error.IsEmpty())Phase=EPhase::Final;else Phase=EPhase::Save;}}
        else if(Phase==EPhase::Save)
        {
            if(Save&&!SaveOnce())Phase=EPhase::Final;
            else if(!Observe(TEXT("after_batch_tail"),Core.R.bSaved))Phase=EPhase::Final;
            else FinishRows();
        }
        else if(Phase==EPhase::Final){Finalize();return;}
        if(!Core.R.Error.IsEmpty()){Phase=EPhase::Final;Finalize();return;}
        if(Phase==EPhase::Final){Finalize();return;}
        Pending();
    }
    void ReleaseSafe()
    {
        if(Executing||Phase!=EPhase::Terminal||!PublicationSafe())return;
        Core.Context.PhysicalGraphs.Reset();Core.Context.Identities.Reset();Core.Context.Keep.Reset();JobIdentity.Reset();Job.Reset();
        if(ActiveBatch.Get()==this){ActiveBatch.Reset();BatchAsyncLease=false;}
    }
    bool Tick(float)
    {
        TSharedRef<FJobState> Hold=AsShared();
        if(Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return false;
        if(Executing||BatchAsyncRunning||LastFrame==GFrameCounter)return true;
        LastFrame=GFrameCounter;
        if(!PublicationSafe()){Quarantine(TEXT("Batch ticker lost captured Editor/world/GC/exit context."));return false;}
        {TGuardValue<bool> Busy(Executing,true);TGuardValue<bool> Lease(BatchAsyncRunning,true);Execute();}
        ReleaseSafe();return Phase!=EPhase::Terminal&&Phase!=EPhase::Quarantined;
    }
    void Hook()
    {
        const TWeakPtr<FJobState> Weak=AsShared();
        Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){auto S=Weak.Pin();return S.IsValid()&&S->Tick(Delta);}),0.0f);
        PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([Weak]()
        {
            BatchAsyncStopping.store(true);if(!IsInGameThread())return;auto S=Weak.Pin();if(!S)return;
            S->Core.Context.CancelRequested=true;S->Core.Context.CancelReason=TEXT("Engine pre-exit stopped batch job.");S->Unhook();
            if(!S->Executing)S->Quarantine(S->Core.Context.CancelReason);
        });
    }
    bool Parse(const FString& Text)
    {
        if(!BatchOuterJson(Text,Params))return false;
        return Params->TryGetStringField(TEXT("system_path"),SystemPath)&&Params->TryGetStringField(TEXT("edits_json"),EditsJson)&&
            Params->TryGetStringField(TEXT("operation"),Operation)&&Params->TryGetBoolField(TEXT("overwrite_existing"),Overwrite)&&
            Params->TryGetBoolField(TEXT("continue_on_error"),Continue)&&Params->TryGetBoolField(TEXT("allow_source_edit"),Allow)&&
            Params->TryGetBoolField(TEXT("save"),Save)&&Params->TryGetBoolField(TEXT("request_compile"),Compile)&&
            Params->TryGetBoolField(TEXT("wait_for_completion"),Wait)&&Params->TryGetNumberField(TEXT("timeout_seconds"),Timeout)&&
            Params->TryGetNumberField(TEXT("poll_interval_seconds"),Poll)&&FMath::IsFinite(Timeout)&&FMath::IsFinite(Poll);
    }
};
namespace
{
void PruneBatchCache()
{
    for(auto It=BatchCacheOwners.CreateIterator();It;++It)
        if(!It.Value().IsValid()){BatchSnapshotCache.Remove(It.Key());It.RemoveCurrent();}
}
}
UCubelessNiagaraModuleInputsBatchJob* StartBatch(const FString& ParamsJson)
{
    if(!BatchSafe()||BatchAsyncStopping.load()||BatchAsyncRunning||BatchAsyncLease||!BatchLive(GEditor))return nullptr;
    UWorld* World=GEditor->GetEditorWorldContext().World();
    if(!BatchLive(World)||World->bIsTearingDown||GEditor->PlayWorld||GEditor->GetPIEWorldContext())return nullptr;
    PruneBatchCache();if(BatchSnapshotCache.Num()>=256)return nullptr;
    auto State=MakeShared<FJobState>();State->Core.Context.Editor=GEditor;State->Core.Context.World=World;
    ActiveBatch=State;BatchAsyncLease=true;
    UCubelessNiagaraModuleInputsBatchJob* Created=nullptr;
    {
        TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> Lease(BatchAsyncRunning,true);
        State->Core.R.bMutationAttempted=true;BatchCount(State->Core.R,TEXT("NewObject_ModuleInputsBatchJob"));
        Created=NewObject<UCubelessNiagaraModuleInputsBatchJob>(GetTransientPackage());
        State->RawJob=Created;State->Core.R.Data->SetBoolField(TEXT("job_allocation_returned"),true);State->Core.R.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
        if(BatchAsyncStopping.load()||State->Core.Context.CancelRequested||!State->PublicationSafe())
        {State->Quarantine(TEXT("Batch Job allocation interrupted; row dispatch was not entered."));return nullptr;}
        if(!Created||!BatchReadableChain(Created))
        {BatchFail(State->Core.R,TEXT("niagara_batch_async_allocation_failed"),TEXT("Transient batch Job allocation returned null/unreadable; no target SDK."));ActiveBatch.Reset();BatchAsyncLease=false;return nullptr;}
        State->Job.Reset(Created);State->JobIdentity=MakeUnique<FBatchIdentity>(Created);BatchCacheOwners.Add(Created,Created);
        if(!State->Parse(ParamsJson))
        {BatchFail(State->Core.R,TEXT("niagara_batch_async_params_invalid"),TEXT("Normalized Native11 object within2Mi+16KiB and exact edits_json string carrier required."));State->Phase=EPhase::Final;State->Finalize();}
        else {State->Core.R.bCompileRequested=State->Compile;State->Core.R.bSaveRequested=State->Save;State->Pending();}
    }
    if(State->Phase==EPhase::Terminal){State->ReleaseSafe();return Created;}
    if(State->Phase!=EPhase::Quarantined)State->Hook();
    return Created;
}
FString SnapshotJson(const UCubelessNiagaraModuleInputsBatchJob* Job)
{
    if(!IsInGameThread()||!Job)return FString();
    if(const FString* Snapshot=BatchSnapshotCache.Find(Job))return *Snapshot;
    return FString();
}
bool Cancel(UCubelessNiagaraModuleInputsBatchJob* Job,const FString& Reason)
{
    if(!IsInGameThread()||!Job||!ActiveBatch.IsValid()||ActiveBatch->RawJob!=Job)return false;
    auto State=ActiveBatch;if(State->Phase==EPhase::Terminal||State->Phase==EPhase::Quarantined)return false;
    State->Core.Context.CancelRequested=true;State->Core.Context.CancelReason=Reason.Left(1024);State->Unhook();
    if(State->Executing)return true;
    if(!State->PublicationSafe()){State->Quarantine(TEXT("Batch cancellation cannot publish into an unsafe Editor/world; retained snapshot only."));return true;}
    {TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> Lease(BatchAsyncRunning,true);
     State->Core.R.bChangeStateKnown=false;BatchFail(State->Core.R,TEXT("niagara_batch_async_cancelled"),*State->Core.Context.CancelReason);State->Phase=EPhase::Final;State->Finalize();}
    State->ReleaseSafe();return true;
}
void Shutdown(const FString& Reason)
{
    BatchAsyncStopping.store(true);if(!IsInGameThread())return;
    auto State=ActiveBatch;if(!State.IsValid())return;
    State->Core.Context.CancelRequested=true;State->Core.Context.CancelReason=Reason.Left(1024);State->Unhook();
    if(State->Executing)return;
    if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe batch state without UObject publication."));return;}
    Cancel(State->RawJob,State->Core.Context.CancelReason);
}
}
