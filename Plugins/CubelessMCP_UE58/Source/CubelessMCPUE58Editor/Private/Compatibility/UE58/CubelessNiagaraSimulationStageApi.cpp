#include "Compatibility/UE58/CubelessNiagaraSimulationStageApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessNiagaraEmitterEventAuthoringAsyncApi.h"
#include "CubelessNiagaraEmitterEventAuthoringJob.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "AssetCompilingManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "CoreGlobals.h"
#include "Misc/Crc.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceGrid2DCollection.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSystem.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::NiagaraSimulationStage
{
namespace
{
using Function::FResult;
constexpr int32 MaxEmitters = 128, MaxStages = 128, MaxNodes = 1024, MaxPins = 8192, MaxLinkRefs = 32768;
struct FIBStageJob;
struct FContext; struct FSettingsContext; struct FSettingsPatch; struct FExpectedSettings; struct FGraphState;
FIBStageJob* ActiveIB=nullptr;
bool IBCheck(const TCHAR* Stage);
bool IBBind(FResult& R);
bool IBReadAdmission(FResult& R);
bool IBKeep(UObject* Object);
bool IBStageScriptAssignment(UNiagaraSimulationStageBase* Stage,UNiagaraScript* Script);
bool IBStageAttachVersion(UNiagaraSimulationStageBase* Stage,const FGuid& Version);
bool IBScriptSourceAssignment(UNiagaraScript* Script,UNiagaraScriptSourceBase* Source);
UEdGraphNode* IBAllocateNode(UNiagaraGraph* Graph,UClass* Class);
bool IBFinalizeNode(UEdGraphNode* Node);
bool IBCapture(FResult& R,FContext& Context);
bool IBArmAdd(FResult& R,FContext& Context,const FGraphState& Graph,bool Save,bool Compile);
bool IBArmSettings(FResult& R,FSettingsContext& Context,const FSettingsPatch& Patch,const FExpectedSettings& Expected,bool Save,bool Compile);
FString Path(const UObject* O) { return O ? O->GetPathName() : FString(); }
FString Guid(const FGuid& G) { return G.ToString(EGuidFormats::DigitsWithHyphens); }
FString Text(const TSharedRef<FJsonObject>& J)
{ FString Out; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out)); return Out; }
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects |
    RF_BeginDestroyed | RF_FinishDestroyed | RF_ClassDefaultObject | RF_ArchetypeObject); }
bool Fail(FResult& R, const TCHAR* Code, const TCHAR* Message)
{ if (R.ErrorCode.IsEmpty()) { R.ErrorCode = Code; R.Error = Message; } return false; }
bool Bounded(const FString& V, int32 Limit = 1024)
{ if (V.Len() > Limit) { return false; } for (TCHAR C : V) { if (C < 32 || C == 127) { return false; } } return true; }
FString Token(FString V)
{ V = V.TrimStartAndEnd().ToLower(); V.ReplaceInline(TEXT("_"), TEXT("")); V.ReplaceInline(TEXT("-"), TEXT("")); V.ReplaceInline(TEXT(" "), TEXT("")); return V; }
bool Int(const TSharedPtr<FJsonValue>& V, int32& Out, int32 Minimum = 0)
{
    double N;
    if (!V.IsValid() || V->Type != EJson::Number || !V->TryGetNumber(N) || !FMath::IsFinite(N) ||
        N != FMath::TruncToDouble(N) || N < Minimum || N > MAX_int32) { return false; }
    Out = static_cast<int32>(N); return true;
}
bool Index(const FString& Json, int32& Out)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    if (Json.Len() > 64 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Rows) ||
        Rows.Num() != 1 || !Rows[0].IsValid()) { return false; }
    if (Rows[0]->Type == EJson::Null) { Out = INDEX_NONE; return true; }
    return Int(Rows[0], Out);
}
bool ObjectPath(const FString& In, FString& Out)
{
    if (!Bounded(In)) { return false; } Out = In.TrimStartAndEnd();
    if (!Out.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(Out))
    { Out += TEXT(".") + FPackageName::GetLongPackageAssetName(Out); }
    return FPackageName::IsValidObjectPath(Out);
}
// These four public UE58 enums have no exported StaticEnum specialization.
// Preserve the exact declared enumerator names without linking generated code.
FString Enum(ENiagaraIterationSource Value)
{
    switch (Value)
    {
    case ENiagaraIterationSource::Particles: return TEXT("Particles");
    case ENiagaraIterationSource::DataInterface: return TEXT("DataInterface");
    case ENiagaraIterationSource::DirectSet: return TEXT("DirectSet");
    default: return FString();
    }
}
FString Enum(ENiagaraGpuDispatchType Value)
{
    switch (Value)
    {
    case ENiagaraGpuDispatchType::OneD: return TEXT("OneD");
    case ENiagaraGpuDispatchType::TwoD: return TEXT("TwoD");
    case ENiagaraGpuDispatchType::ThreeD: return TEXT("ThreeD");
    case ENiagaraGpuDispatchType::Custom: return TEXT("Custom");
    default: return FString();
    }
}
FString Enum(ENiagaraDirectDispatchElementType Value)
{
    switch (Value)
    {
    case ENiagaraDirectDispatchElementType::NumThreads: return TEXT("NumThreads");
    case ENiagaraDirectDispatchElementType::NumThreadsNoClipping: return TEXT("NumThreadsNoClipping");
    case ENiagaraDirectDispatchElementType::NumGroups: return TEXT("NumGroups");
    default: return FString();
    }
}
FString Enum(ENiagaraSimStageExecuteBehavior Value)
{
    switch (Value)
    {
    case ENiagaraSimStageExecuteBehavior::Always: return TEXT("Always");
    case ENiagaraSimStageExecuteBehavior::OnSimulationReset: return TEXT("OnSimulationReset");
    case ENiagaraSimStageExecuteBehavior::NotOnSimulationReset: return TEXT("NotOnSimulationReset");
    default: return FString();
    }
}
template<typename T> FString Enum(T Value) { return StaticEnum<T>()->GetNameStringByValue(static_cast<int64>(Value)); }
TSharedRef<FJsonObject> CompileState(UNiagaraSystem* S)
{
    auto J = MakeShared<FJsonObject>(); const bool Outstanding = S->HasOutstandingCompilationRequests(true), Needs = S->NeedsRequestCompile();
    J->SetBoolField(TEXT("outstanding_including_gpu"), Outstanding); J->SetBoolField(TEXT("needs_request_compile"), Needs);
    J->SetBoolField(TEXT("ready_to_run"), S->IsReadyToRun()); J->SetBoolField(TEXT("ready_to_save"), !Outstanding && !Needs);
    J->SetBoolField(TEXT("compile_success_verified"), false); J->SetBoolField(TEXT("poll_or_flush_invoked"), false); return J;
}
void Initialize(FResult& R, const FString& SystemPath, bool bSave = false)
{
    R.Data->SetStringField(TEXT("requested_system_path"), SystemPath); R.Data->SetBoolField(TEXT("asset_load_attempted"), false);
    R.Data->SetBoolField(TEXT("already_loaded_assets_required"), true); R.Data->SetBoolField(TEXT("compile_requested"), false);
    R.Data->SetBoolField(TEXT("compile_attempted"), false); R.Data->SetField(TEXT("compile_request_returned"), MakeShared<FJsonValueNull>());
    R.Data->SetBoolField(TEXT("save_requested"), bSave); R.Data->SetBoolField(TEXT("save_attempted"), false);
    R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetBoolField(TEXT("save_deferred"), false);
    R.Data->SetBoolField(TEXT("blocking_wait_invoked"), false); R.Data->SetBoolField(TEXT("atomic_rollback_available"), false);
    R.Data->SetBoolField(TEXT("auto_retry_safe"), false); R.Data->SetBoolField(TEXT("implicit_notification_callbacks_possible"), true);
    R.Data->SetBoolField(TEXT("implicit_compile_effect_observed"), false);
    R.Data->SetBoolField(TEXT("explicit_stage_request_recompile_invoked"), false); R.Data->SetBoolField(TEXT("full_legacy_niagara_parity"), false);
    R.Data->SetBoolField(TEXT("legacy_unconditional_compile_preserved"), true);
    R.Data->SetStringField(TEXT("stage_compile_call_count_scope"), TEXT("Owned RequestRecompile entries and SetEnabled's source-proven conditional path; inner RequestCompileForEmitter fanout is not counted as exact System requests or shader completion."));
    for(const TCHAR* K:{TEXT("stage_request_recompile_attempt_count"),TEXT("stage_request_recompile_return_count"),TEXT("set_enabled_call_count"),TEXT("set_enabled_return_count"),TEXT("set_enabled_recompile_path_count"),TEXT("set_enabled_on_changed_path_count")})R.Data->SetNumberField(K,0);
    R.Data->SetBoolField(TEXT("recursive_asset_dependency_audit_performed"), false); R.Data->SetBoolField(TEXT("runtime_export_behavior_verified"), false);
    constexpr const TCHAR* Attempts[] = {TEXT("stage_allocation_attempted"), TEXT("script_allocation_attempted"),
        TEXT("stage_output_graph_creation_attempted"), TEXT("stage_map_connection_attempted"), TEXT("stage_attach_attempted"), TEXT("stage_move_attempted")};
    for (const TCHAR* A : Attempts) { R.Data->SetBoolField(A, false); }
    R.Data->SetNumberField(TEXT("explicit_compile_request_count"), 0); R.Data->SetNumberField(TEXT("exact_save_attempt_count"), 0);
    R.Data->SetStringField(TEXT("engine_api"), TEXT("UNiagaraEmitter::AddSimulationStage/MoveSimulationStageToIndex; public FGraphNodeCreator<UNiagaraNodeOutput/Input> and parameter-map edge"));
}
struct FOptions
{
    ENiagaraGpuDispatchType Dispatch = ENiagaraGpuDispatchType::OneD;
    ENiagaraDirectDispatchElementType Element = ENiagaraDirectDispatchElementType::NumThreads;
    ENiagaraSimStageExecuteBehavior Execute = ENiagaraSimStageExecuteBehavior::Always;
    int32 Counts[3] = {0, 0, 0}, Iterations = 1;
    bool HasCount[3] = {false, false, false}, HasForce = false, Force = false, HasThreads = false, Threads = false;
};
bool Options(FResult& R, const FString& Json, FOptions& O)
{
    TSharedPtr<FJsonObject> J;
    if (Json.Len() > 8192 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), J) || !J.IsValid())
    { return Fail(R, TEXT("niagara_stage_options_invalid"), TEXT("OptionsJson must be a bounded object with only the documented legacy stage settings.")); }
    const TSet<FString> Allowed = {TEXT("iteration_source"), TEXT("direct_dispatch_type"), TEXT("direct_dispatch_element_type"),
        TEXT("execute_behavior"), TEXT("element_count"), TEXT("element_count_x"), TEXT("element_count_y"), TEXT("element_count_z"),
        TEXT("num_iterations"), TEXT("gpu_dispatch_force_linear"), TEXT("override_gpu_dispatch_num_threads"), TEXT("wait_for_completion")};
    for (const auto& P : J->Values) { if (!Allowed.Contains(FString(P.Key))) { return Fail(R, TEXT("niagara_stage_option_unsupported"), TEXT("Unknown stage option; no allocation or graph mutation dispatched.")); } }
    auto String = [&](const TCHAR* K, FString& V)
    { return !J->HasField(K) || (J->TryGetStringField(K, V) && Bounded(V, 64)); };
    FString Iteration = TEXT("DataInterface"), Dispatch = TEXT("OneD"), Element = TEXT("NumThreads"), Execute = TEXT("Always");
    if (!String(TEXT("iteration_source"), Iteration) || !String(TEXT("direct_dispatch_type"), Dispatch) ||
        !String(TEXT("direct_dispatch_element_type"), Element) || !String(TEXT("execute_behavior"), Execute))
    { return Fail(R, TEXT("niagara_stage_enum_invalid"), TEXT("Stage enum settings must be bounded strings.")); }
    const FString I = Token(Iteration), D = Token(Dispatch), E = Token(Element), X = Token(Execute);
    if (I != TEXT("datainterface") && I != TEXT("datainterfaceiteration"))
    { return Fail(R, TEXT("niagara_stage_iteration_unsupported"), TEXT("The legacy add command supports only DataInterface iteration. DirectSet requires a separate settings operation.")); }
    if (D == TEXT("oned") || D == TEXT("1d")) { O.Dispatch = ENiagaraGpuDispatchType::OneD; }
    else if (D == TEXT("twod") || D == TEXT("2d")) { O.Dispatch = ENiagaraGpuDispatchType::TwoD; }
    else if (D == TEXT("threed") || D == TEXT("3d")) { O.Dispatch = ENiagaraGpuDispatchType::ThreeD; }
    else if (D == TEXT("custom")) { O.Dispatch = ENiagaraGpuDispatchType::Custom; }
    else { return Fail(R, TEXT("niagara_stage_enum_invalid"), TEXT("Unsupported direct_dispatch_type.")); }
    if (E == TEXT("numthreads") || E == TEXT("threads")) { O.Element = ENiagaraDirectDispatchElementType::NumThreads; }
    else if (E == TEXT("numthreadsnoclipping") || E == TEXT("threadsnoclipping") || E == TEXT("noclip")) { O.Element = ENiagaraDirectDispatchElementType::NumThreadsNoClipping; }
    else if (E == TEXT("numgroups") || E == TEXT("groups")) { O.Element = ENiagaraDirectDispatchElementType::NumGroups; }
    else { return Fail(R, TEXT("niagara_stage_enum_invalid"), TEXT("Unsupported direct_dispatch_element_type.")); }
    if (X == TEXT("always")) { O.Execute = ENiagaraSimStageExecuteBehavior::Always; }
    else if (X == TEXT("onsimulationreset") || X == TEXT("onreset")) { O.Execute = ENiagaraSimStageExecuteBehavior::OnSimulationReset; }
    else if (X == TEXT("notonsimulationreset") || X == TEXT("notonreset")) { O.Execute = ENiagaraSimStageExecuteBehavior::NotOnSimulationReset; }
    else { return Fail(R, TEXT("niagara_stage_enum_invalid"), TEXT("Unsupported execute_behavior.")); }
    constexpr const TCHAR* CountKeys[] = {TEXT("element_count_x"), TEXT("element_count_y"), TEXT("element_count_z")};
    for (int32 N = 0; N < 3; ++N)
    {
        if (J->HasField(CountKeys[N]))
        { O.HasCount[N] = true; if (!Int(J->TryGetField(CountKeys[N]), O.Counts[N])) { return Fail(R, TEXT("niagara_stage_count_invalid"), TEXT("Explicit element counts require nonnegative exact int32.")); } }
    }
    // Preserve original precedence: an element_count array overrides provided
    // x/y/z values for the first 1..3 elements, retaining other explicit axes.
    if (J->HasField(TEXT("element_count")))
    {
        const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
        if (!J->TryGetArrayField(TEXT("element_count"), A) || !A || A->Num() < 1 || A->Num() > 3)
        { return Fail(R, TEXT("niagara_stage_count_invalid"), TEXT("element_count requires 1..3 nonnegative exact int32 values.")); }
        for (int32 N = 0; N < A->Num(); ++N) { O.HasCount[N] = true; if (!Int((*A)[N], O.Counts[N])) { return Fail(R, TEXT("niagara_stage_count_invalid"), TEXT("element_count contains a fractional/out-of-range/non-number value.")); } }
    }
    if (J->HasField(TEXT("num_iterations")) && !Int(J->TryGetField(TEXT("num_iterations")), O.Iterations, 1))
    { return Fail(R, TEXT("niagara_stage_count_invalid"), TEXT("num_iterations requires an exact int32 >= 1.")); }
    O.HasForce = J->HasField(TEXT("gpu_dispatch_force_linear")); O.HasThreads = J->HasField(TEXT("override_gpu_dispatch_num_threads"));
    if ((O.HasForce && !J->TryGetBoolField(TEXT("gpu_dispatch_force_linear"), O.Force)) ||
        (O.HasThreads && !J->TryGetBoolField(TEXT("override_gpu_dispatch_num_threads"), O.Threads)))
    { return Fail(R, TEXT("niagara_stage_bool_invalid"), TEXT("GPU flags require JSON bool.")); }
    bool Wait = false;
    if (J->HasField(TEXT("wait_for_completion")) && (!J->TryGetBoolField(TEXT("wait_for_completion"), Wait) || (Wait && !ActiveIB)))
    { return Fail(R, TEXT("niagara_stage_blocking_wait_unsupported"), TEXT("Blocking wait is unsupported; inspect readiness then explicitly save without replaying creation.")); }
    return true;
}
FString PinId(const UEdGraphPin* P) { return P ? Path(P->GetOwningNode()) + TEXT(".") + P->PinName.ToString() + TEXT("/") + Guid(P->PinId) : FString(); }
UObject* YHResolved(const TObjectPtr<UObject>& Slot)
{
 if(!Slot.IsResolved()||Slot==nullptr)return nullptr;const FString P=Slot.GetPathName();auto* O=StaticFindObject(nullptr,nullptr,*P);return Loaded(O)&&Slot==O?O:nullptr;
}
TSharedRef<FJsonObject> PinRow(const UEdGraphPin* P,const TFunction<bool()>& Current={})
{
    auto J = MakeShared<FJsonObject>();if(Current&&!Current())return J; const auto& T = P->PinType;
    J->SetStringField(TEXT("pin_id"), PinId(P)); J->SetStringField(TEXT("pin_guid"), Guid(P->PinId)); J->SetStringField(TEXT("name"), P->PinName.ToString());
    J->SetNumberField(TEXT("direction"), static_cast<int32>(P->Direction)); J->SetStringField(TEXT("category"), T.PinCategory.ToString());
    J->SetStringField(TEXT("subcategory"), T.PinSubCategory.ToString()); J->SetStringField(TEXT("subcategory_object_path"), Path(T.PinSubCategoryObject.Get()));
    J->SetNumberField(TEXT("container_type"), static_cast<int32>(T.ContainerType)); J->SetBoolField(TEXT("is_reference"), T.bIsReference);
    J->SetBoolField(TEXT("is_const"), T.bIsConst); J->SetBoolField(TEXT("is_weak_pointer"), T.bIsWeakPointer);
    J->SetStringField(TEXT("terminal_category"), T.PinValueType.TerminalCategory.ToString()); J->SetStringField(TEXT("terminal_subcategory"), T.PinValueType.TerminalSubCategory.ToString());
    J->SetStringField(TEXT("terminal_subcategory_object_path"), Path(T.PinValueType.TerminalSubCategoryObject.Get()));
    J->SetBoolField(TEXT("terminal_is_const"), T.PinValueType.bTerminalIsConst); J->SetBoolField(TEXT("terminal_is_weak_pointer"), T.PinValueType.bTerminalIsWeakPointer);
    J->SetStringField(TEXT("raw_default_value"), P->DefaultValue); J->SetStringField(TEXT("autogenerated_default_value"), P->AutogeneratedDefaultValue);
    J->SetStringField(TEXT("default_object_path"), Current?Path(YHResolved(P->DefaultObject)):Path(P->DefaultObject)); J->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString());if(Current&&!Current())return J;
    J->SetBoolField(TEXT("hidden"), P->bHidden); J->SetBoolField(TEXT("orphaned"), P->bOrphanedPin); J->SetBoolField(TEXT("not_connectable"), P->bNotConnectable);
    J->SetBoolField(TEXT("default_value_is_ignored"), P->bDefaultValueIsIgnored); J->SetBoolField(TEXT("default_value_is_readonly"), P->bDefaultValueIsReadOnly); return J;
}
struct FGraphState
{
    bool Complete = false; TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    TMap<FString, FString> Nodes, Pins; TMap<FString, TArray<FString>> Links;
};
FGraphState GraphState(UNiagaraGraph* G,const TFunction<bool()>& Current={})
{
    FGraphState S;if(Current&&!Current())return S; S.Json->SetStringField(TEXT("graph_path"), Path(G));
    S.Json->SetBoolField(TEXT("observation_complete"), false);
    if (!Loaded(G) || G->GetClass() != UNiagaraGraph::StaticClass() || G->Nodes.Num() > MaxNodes) { return S; }
    TArray<UEdGraphNode*> Ordered; TSet<FGuid> Guids; int32 Links = 0; TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<TObjectPtr<UEdGraphNode>> CopiedNodeSlots;if(Current)CopiedNodeSlots=G->Nodes;const auto& NodeSlots=Current?CopiedNodeSlots:G->Nodes;
    for (const auto& Slot : NodeSlots)
    { UEdGraphNode* N=Current?Cast<UEdGraphNode>(YHResolved(Slot)):Slot.Get();if(Current&&!Current())return S;if (!Loaded(N) || N->GetOuter() != G || !N->NodeGuid.IsValid() || Guids.Contains(N->NodeGuid)) { return S; } Guids.Add(N->NodeGuid); Ordered.Add(N); }
    Ordered.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) { return A.GetPathName() < B.GetPathName(); });
    TSet<const UEdGraphPin*> Pins;
    for (UEdGraphNode* N : Ordered)
    { TArray<UEdGraphPin*> CopiedPins;if(Current)CopiedPins=N->Pins;const auto& NodePins=Current?CopiedPins:N->Pins;for (const UEdGraphPin* P : NodePins) { if (!P || P->GetOwningNode() != N || !P->PinId.IsValid() || Pins.Contains(P)) { return S; } Pins.Add(P); if (Pins.Num() > MaxPins) { return S; } } }
    for (UEdGraphNode* N : Ordered)
    {
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("node_path"), Path(N)); J->SetStringField(TEXT("node_class"), Path(N->GetClass()));
        J->SetStringField(TEXT("node_guid"), Guid(N->NodeGuid)); J->SetNumberField(TEXT("position_x"), N->NodePosX);
        J->SetNumberField(TEXT("position_y"), N->NodePosY); J->SetStringField(TEXT("comment"), N->NodeComment);
        if (auto* O = Cast<UNiagaraNodeOutput>(N))
        { J->SetStringField(TEXT("usage"), Enum(O->GetUsage())); J->SetStringField(TEXT("usage_id"), Guid(O->GetUsageId())); }
        if (auto* I = Cast<UNiagaraNodeInput>(N))
        { J->SetStringField(TEXT("input_name"), I->Input.GetName().ToString()); J->SetStringField(TEXT("input_type"), I->Input.GetType().GetNameText().ToString());if(Current&&!Current())return S; J->SetStringField(TEXT("input_usage"), Enum(I->Usage)); }
        if (auto* F = Cast<UNiagaraNodeFunctionCall>(N))
        { J->SetStringField(TEXT("function_script_path"), Current?Path(YHResolved(F->FunctionScript)):Path(F->FunctionScript)); J->SetStringField(TEXT("selected_script_version"), Guid(F->SelectedScriptVersion)); J->SetStringField(TEXT("function_name"), F->GetFunctionName()); }
        S.Nodes.Add(Path(N), Text(J)); TArray<TSharedPtr<FJsonValue>> PinRows;
        TArray<UEdGraphPin*> CopiedPins;if(Current)CopiedPins=N->Pins;const auto& NodePins=Current?CopiedPins:N->Pins;
        for (const UEdGraphPin* P : NodePins)
        {
            auto PJ = PinRow(P,Current);if(Current&&!Current())return S; const FString Id = PinId(P); if (S.Pins.Contains(Id)) { return S; } S.Pins.Add(Id, Text(PJ));
            TArray<FString>& Peers = S.Links.FindOrAdd(Id); TArray<TSharedPtr<FJsonValue>> Linked;
            for (const UEdGraphPin* Other : P->LinkedTo)
            {
                if (!Pins.Contains(Other) || Other == P || !Other->LinkedTo.Contains(P) || Peers.Contains(PinId(Other)) || ++Links > MaxLinkRefs) { return S; }
                Peers.Add(PinId(Other));
            }
            Peers.Sort(); for (const FString& V : Peers) { Linked.Add(MakeShared<FJsonValueString>(V)); }
            PJ->SetArrayField(TEXT("links"), Linked); PinRows.Add(MakeShared<FJsonValueObject>(PJ));
        }
        J->SetArrayField(TEXT("pins"), PinRows); Rows.Add(MakeShared<FJsonValueObject>(J));
    }
    S.Json->SetArrayField(TEXT("nodes"), Rows); S.Json->SetNumberField(TEXT("node_count"), S.Nodes.Num()); S.Json->SetNumberField(TEXT("pin_count"), S.Pins.Num());
    S.Json->SetNumberField(TEXT("physical_link_count"), Links / 2); S.Json->SetBoolField(TEXT("observation_complete"), true); S.Complete = true; return S;
}
bool SameGraph(const FGraphState& A, const FGraphState& B)
{
    if (!A.Complete || !B.Complete || !A.Nodes.OrderIndependentCompareEqual(B.Nodes) || !A.Pins.OrderIndependentCompareEqual(B.Pins) || A.Links.Num() != B.Links.Num()) { return false; }
    for (const auto& P : A.Links) { const auto* V = B.Links.Find(P.Key); if (!V || *V != P.Value) { return false; } } return true;
}
TSharedRef<FJsonObject> Binding(FNiagaraParameterBindingWithValue& B, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); const auto A = B.GetDefaultValueArray();
    J->SetStringField(TEXT("aliased_parameter_name"), B.AliasedParameter.GetName().ToString()); J->SetStringField(TEXT("resolved_parameter_name"), B.ResolvedParameter.GetName().ToString());
    J->SetStringField(TEXT("default_parameter_name"), B.GetDefaultAliasedParameter().GetName().ToString());
    J->SetStringField(TEXT("default_parameter_type"), B.GetDefaultAliasedParameter().GetType().GetNameText().ToString());
    J->SetStringField(TEXT("binding_text"), B.ToString()); J->SetNumberField(TEXT("default_value_bytes"), A.Num());
    if (A.Num() != sizeof(int32)) { Complete = false; J->SetField(TEXT("default_value"), MakeShared<FJsonValueNull>()); }
    else { J->SetNumberField(TEXT("default_value"), B.GetDefaultValue<int32>()); } return J;
}
TArray<TSharedPtr<FJsonValue>> IntArray(std::initializer_list<int32> Values)
{ TArray<TSharedPtr<FJsonValue>> Rows; for (int32 V : Values) { Rows.Add(MakeShared<FJsonValueNumber>(V)); } return Rows; }
TSharedRef<FJsonObject> Variable(const FNiagaraVariableBase& V)
{
    auto J = MakeShared<FJsonObject>(); const auto& T = V.GetType(); J->SetStringField(TEXT("name"), V.GetName().ToString());
    J->SetStringField(TEXT("type"), T.GetNameText().ToString()); J->SetBoolField(TEXT("type_is_valid"), T.IsValid());
    J->SetBoolField(TEXT("type_is_data_interface"), T.IsDataInterface()); J->SetBoolField(TEXT("type_is_uobject"), T.IsUObject());
    if (auto* C = T.GetClass()) { J->SetStringField(TEXT("type_class"), C->GetName()); J->SetStringField(TEXT("type_class_path"), Path(C)); } return J;
}
TSharedRef<FJsonObject> CompilationRow(const FNiagaraSimulationStageCompilationData& C)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("stage_guid"), Guid(C.StageGuid)); J->SetStringField(TEXT("stage_name"), C.StageName.ToString());
    J->SetStringField(TEXT("enabled_binding"), C.EnabledBinding.ToString()); J->SetArrayField(TEXT("element_count"), IntArray({C.ElementCount.X, C.ElementCount.Y, C.ElementCount.Z}));
    J->SetStringField(TEXT("element_count_x_binding"), C.ElementCountXBinding.ToString()); J->SetStringField(TEXT("element_count_y_binding"), C.ElementCountYBinding.ToString());
    J->SetStringField(TEXT("element_count_z_binding"), C.ElementCountZBinding.ToString()); J->SetNumberField(TEXT("num_iterations"), C.NumIterations);
    J->SetStringField(TEXT("num_iterations_binding"), C.NumIterationsBinding.ToString()); J->SetStringField(TEXT("iteration_source_type"), Enum(C.IterationSourceType));
    J->SetStringField(TEXT("iteration_data_interface"), C.IterationDataInterface.ToString()); J->SetStringField(TEXT("iteration_direct_binding"), C.IterationDirectBinding.ToString());
    J->SetStringField(TEXT("execute_behavior"), Enum(C.ExecuteBehavior)); J->SetBoolField(TEXT("partial_particle_update"), C.PartialParticleUpdate);
    J->SetBoolField(TEXT("particle_iteration_state_enabled"), C.bParticleIterationStateEnabled); J->SetStringField(TEXT("particle_iteration_state_binding"), C.ParticleIterationStateBinding.ToString());
    J->SetArrayField(TEXT("particle_iteration_state_range"), IntArray({C.ParticleIterationStateRange.X, C.ParticleIterationStateRange.Y}));
    J->SetBoolField(TEXT("gpu_dispatch_force_linear"), C.bGpuDispatchForceLinear); J->SetStringField(TEXT("direct_dispatch_type"), Enum(C.DirectDispatchType));
    J->SetStringField(TEXT("direct_dispatch_element_type"), Enum(C.DirectDispatchElementType)); J->SetBoolField(TEXT("override_gpu_dispatch_num_threads"), C.bOverrideGpuDispatchNumThreads);
    J->SetArrayField(TEXT("override_gpu_dispatch_num_threads_value"), IntArray({C.OverrideGpuDispatchNumThreads.X, C.OverrideGpuDispatchNumThreads.Y, C.OverrideGpuDispatchNumThreads.Z}));
    J->SetObjectField(TEXT("override_gpu_dispatch_num_threads_x_binding"), Variable(C.OverrideGpuDispatchNumThreadsXBinding));
    J->SetObjectField(TEXT("override_gpu_dispatch_num_threads_y_binding"), Variable(C.OverrideGpuDispatchNumThreadsYBinding));
    J->SetObjectField(TEXT("override_gpu_dispatch_num_threads_z_binding"), Variable(C.OverrideGpuDispatchNumThreadsZBinding)); return J;
}
TSharedRef<FJsonObject> StageRow(UNiagaraSimulationStageBase* Base, int32 Index, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("stage_index"), Index); J->SetStringField(TEXT("stage_path"), Path(Base));
    if (!Loaded(Base) || Base->GetClass() != UNiagaraSimulationStageGeneric::StaticClass()) { Complete = false; return J; }
    auto* S = CastChecked<UNiagaraSimulationStageGeneric>(Base);
    J->SetStringField(TEXT("stage_class"), Path(S->GetClass())); J->SetStringField(TEXT("stage_name"), S->SimulationStageName.ToString());
    J->SetStringField(TEXT("owner_path"), Path(S->GetOuter())); J->SetBoolField(TEXT("enabled"), S->bEnabled);
    J->SetStringField(TEXT("outer_emitter_version"), Guid(S->OuterEmitterVersion)); J->SetStringField(TEXT("script_path"), Path(S->Script));
    if (!Loaded(S->Script) || S->Script->GetClass() != UNiagaraScript::StaticClass() || S->Script->GetOuter() != S)
    { Complete = false; return J; }
    J->SetStringField(TEXT("script_usage"), Enum(S->Script->GetUsage())); J->SetStringField(TEXT("target_usage_id"), Guid(S->Script->GetUsageId()));
    J->SetStringField(TEXT("script_source_path"), Path(S->Script->GetLatestSource()));
    J->SetStringField(TEXT("iteration_source"), Enum(S->IterationSource)); J->SetStringField(TEXT("execute_behavior"), Enum(S->ExecuteBehavior));
    J->SetStringField(TEXT("direct_dispatch_type"), Enum(S->DirectDispatchType)); J->SetStringField(TEXT("direct_dispatch_element_type"), Enum(S->DirectDispatchElementType));
    if (Enum(S->IterationSource).IsEmpty() || Enum(S->ExecuteBehavior).IsEmpty() ||
        Enum(S->DirectDispatchType).IsEmpty() || Enum(S->DirectDispatchElementType).IsEmpty()) { Complete = false; }
    J->SetBoolField(TEXT("gpu_dispatch_force_linear"), S->bGpuDispatchForceLinear); J->SetBoolField(TEXT("override_gpu_dispatch_num_threads"), S->bOverrideGpuDispatchNumThreads);
    J->SetBoolField(TEXT("disable_partial_particle_update"), S->bDisablePartialParticleUpdate); J->SetBoolField(TEXT("particle_iteration_state_enabled"), S->bParticleIterationStateEnabled);
    J->SetStringField(TEXT("enabled_binding"), S->EnabledBinding.GetName().ToString()); J->SetStringField(TEXT("particle_iteration_state_binding"), S->ParticleIterationStateBinding.GetName().ToString());
    J->SetNumberField(TEXT("particle_iteration_state_min"), S->ParticleIterationStateRange.X); J->SetNumberField(TEXT("particle_iteration_state_max"), S->ParticleIterationStateRange.Y);
    J->SetStringField(TEXT("data_interface_name"), S->DataInterface.BoundVariable.GetName().ToString());
    J->SetStringField(TEXT("data_interface_class_path"), Path(S->DataInterface.BoundVariable.GetType().GetClass()));
    J->SetObjectField(TEXT("num_iterations_binding_observation"), Binding(S->NumIterations, Complete)); J->SetObjectField(TEXT("element_count_x"), Binding(S->ElementCountX, Complete));
    J->SetObjectField(TEXT("element_count_y"), Binding(S->ElementCountY, Complete)); J->SetObjectField(TEXT("element_count_z"), Binding(S->ElementCountZ, Complete));
    J->SetObjectField(TEXT("override_gpu_dispatch_num_threads_x"), Binding(S->OverrideGpuDispatchNumThreadsX, Complete));
    J->SetObjectField(TEXT("override_gpu_dispatch_num_threads_y"), Binding(S->OverrideGpuDispatchNumThreadsY, Complete));
    J->SetObjectField(TEXT("override_gpu_dispatch_num_threads_z"), Binding(S->OverrideGpuDispatchNumThreadsZ, Complete));
    if (S->bParticleIterationStateEnabled && !S->ParticleIterationStateBinding.GetName().ToString().StartsWith(TEXT("Particles."))) { Complete = false; }
    // Preserve the legacy scalar/array settings keys as well as richer raw
    // binding observations. FillCompilationData is metadata, not a compile.
    if (Complete)
    {
        J->SetNumberField(TEXT("num_iterations"), S->NumIterations.GetDefaultValue<int32>());
        J->SetArrayField(TEXT("element_count"), IntArray({S->ElementCountX.GetDefaultValue<int32>(), S->ElementCountY.GetDefaultValue<int32>(), S->ElementCountZ.GetDefaultValue<int32>()}));
        TArray<FNiagaraSimulationStageCompilationData> Data;
        if(!IBCheck(TEXT("before_FillCompilationData"))){Complete=false;return J;}
        const bool Filled = S->FillCompilationData(Data);
        if(!IBCheck(TEXT("after_FillCompilationData"))){Complete=false;return J;}
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto& D : Data) { Rows.Add(MakeShared<FJsonValueObject>(CompilationRow(D))); }
        J->SetBoolField(TEXT("filled_compile_data"), Filled); J->SetNumberField(TEXT("compile_data_count"), Rows.Num()); J->SetArrayField(TEXT("compile_data"), Rows);
        J->SetBoolField(TEXT("compile_data_is_metadata_only"), true);
    }
    return J;
}
struct FYHInherited
{
 struct FObject{TStrongObjectPtr<UObject> Object,Outer;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString Path;};
 struct FNode{TStrongObjectPtr<UEdGraphNode> Node;TArray<UEdGraphPin*> Pins;};
 struct FRole{TStrongObjectPtr<UNiagaraEmitter> Emitter;TStrongObjectPtr<UNiagaraScriptSource> Source;TStrongObjectPtr<UNiagaraGraph> Graph;FGuid Version;FVersionedNiagaraEmitterData* Data=nullptr;FVersionedNiagaraEmitter Parent,LastMerge;TArray<FNode> Nodes;FGraphState Before;bool Ready=false,Dirty=false;};
 TStrongObjectPtr<UNiagaraEmitter> Target;TStrongObjectPtr<UNiagaraScriptSource> LocalSource;TStrongObjectPtr<UNiagaraGraph> LocalGraph;
 FVersionedNiagaraEmitter Parent,LastMerge;TArray<TUniquePtr<FRole>> Roles;TArray<FObject> Objects;bool Lost=false;
};
struct FContext
{
    TSharedPtr<FYHInherited> Inherited;
    UNiagaraSystem* System = nullptr; UNiagaraEmitter* Emitter = nullptr; FVersionedNiagaraEmitterData* Data = nullptr;
    UNiagaraScriptSource* Source = nullptr; UNiagaraGraph* Graph = nullptr; FGuid Version; int32 EmitterIndex = INDEX_NONE; FString EmitterName;
    FGraphState BeforeGraph; TArray<UNiagaraSimulationStageBase*> BeforeStages; TArray<TSharedPtr<FJsonValue>> BeforeStageRows;
    TMap<FString, FGraphState> OtherGraphs; TMap<FString, UNiagaraGraph*> OtherGraphObjects; FString HandlesBefore; bool DirtyBefore = false;
};
bool YHSlotSame(const FVersionedNiagaraEmitter& A,const FVersionedNiagaraEmitter& B)
{return A.Emitter.IsResolved()&&B.Emitter.IsResolved()&&A.Emitter==B.Emitter&&A.Version==B.Version;}
bool YHStop(FResult& R,const FContext& C,const TCHAR* Why)
{if(C.Inherited)C.Inherited->Lost=true;R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("inherited_stage_do_not_retry"),true);if(!R.Data->HasField(TEXT("inherited_stage_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("reason"),Why);J->SetStringField(TEXT("expected"),TEXT("captured local and readonly inherited roles"));J->SetStringField(TEXT("actual"),TEXT("changed or unavailable; no replacement adopted"));R.Data->SetObjectField(TEXT("inherited_stage_first_mismatch"),J);}return Fail(R,TEXT("niagara_stage_inherited_source_changed"),Why);}
bool YHPhysical(FResult& R,const FContext& C)
{
 if(!C.Inherited)return true;auto& P=*C.Inherited;if(P.Lost)return false;
 if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||!Loaded(C.System)||!Loaded(P.Target.Get())||P.Target.Get()!=C.Emitter||P.Target->GetEmitterData(C.Version)!=C.Data||!C.Data||!YHSlotSame(C.Data->VersionedParent,P.Parent)||!YHSlotSame(C.Data->VersionedParentAtLastMerge,P.LastMerge)||!C.Data->GraphSource.IsResolved()||C.Data->GraphSource!=P.LocalSource.Get()||!Loaded(P.LocalSource.Get())||!P.LocalSource->NodeGraph.IsResolved()||P.LocalSource->NodeGraph!=P.LocalGraph.Get()||!Loaded(P.LocalGraph.Get())||P.LocalGraph->GetOuter()!=P.LocalSource.Get())return YHStop(R,C,TEXT("Captured local emitter/version/Source/Graph or Parent/LastMerge role changed; no following stage SDK."));
 for(const auto& O:P.Objects)if(!Loaded(O.Object.Get())||!Loaded(O.Class.Get())||!Loaded(O.Outer.Get())||!Loaded(O.Package.Get())||O.Object->GetClass()!=O.Class.Get()||O.Object->GetOuter()!=O.Outer.Get()||O.Object->GetPackage()!=O.Package.Get()||O.Object->GetPathName()!=O.Path)return YHStop(R,C,TEXT("Held inherited source identity/owner/package changed."));
 for(const auto& E:P.Roles)
 {
  auto* D=E->Emitter->GetEmitterData(E->Version);if(!D||D!=E->Data||!YHSlotSame(D->VersionedParent,E->Parent)||!YHSlotSame(D->VersionedParentAtLastMerge,E->LastMerge)||!D->GraphSource.IsResolved()||D->GraphSource!=E->Source.Get()||!D->GetSimulationStages().IsEmpty()||!E->Source->NodeGraph.IsResolved()||E->Source->NodeGraph!=E->Graph.Get()||E->Graph->Nodes.Num()!=E->Nodes.Num()||(E->Graph->GetPackage()!=C.System->GetPackage()&&E->Graph->GetPackage()->IsDirty()!=E->Dirty))return YHStop(R,C,TEXT("Inherited version/source/empty-stage inventory or external dirty role changed."));
  for(const auto& N:E->Nodes){auto* O=N.Node.Get();if(!Loaded(O)||!E->Graph->Nodes.Contains(O)||O->GetOuter()!=E->Graph.Get()||O->GetGraph()!=E->Graph.Get()||O->Pins.Num()!=N.Pins.Num())return YHStop(R,C,TEXT("Inherited graph owning node/pin storage changed."));TSet<UEdGraphPin*> Now;for(auto* Q:O->Pins)Now.Add(Q);for(auto* Q:N.Pins)if(!Now.Contains(Q))return YHStop(R,C,TEXT("Inherited borrowed pin address retired; no old pin is read."));}
 }
 return true;
}
bool YHCurrent(FResult& R,const FContext& C)
{
 if(!C.Inherited)return true;if(!YHPhysical(R,C))return false;const TFunction<bool()> Fence=[&](){return YHPhysical(R,C);};
 TSet<UNiagaraGraph*> SeenGraphs;for(const auto& E:C.Inherited->Roles)if(E->Ready&&!SeenGraphs.Contains(E->Graph.Get())){SeenGraphs.Add(E->Graph.Get());const auto Now=GraphState(E->Graph.Get(),Fence);if(!YHPhysical(R,C)||!SameGraph(E->Before,Now))return YHStop(R,C,TEXT("Readonly inherited authored graph differs; actual local stage prefix retained without merge/rollback/retry."));}
 return true;
}
bool YHCapture(FResult& R,FContext& C)
{
 if(!C.Data->VersionedParent.Emitter.IsResolved()||!C.Data->VersionedParentAtLastMerge.Emitter.IsResolved())return Fail(R,TEXT("niagara_stage_parent_unresolved"),TEXT("Resolved nullable Parent/LastMerge slots required; no inherited loader."));
 if(C.Data->VersionedParent.Emitter==nullptr&&C.Data->VersionedParentAtLastMerge.Emitter==nullptr)return true;
 if(!ActiveIB)return Fail(R,TEXT("niagara_stage_inherited_continuation_required"),TEXT("Inherited local stage writes require the existing IB continuation; direct/action/synchronous Batch inherited support remains incomplete."));
 C.Inherited=MakeShared<FYHInherited>();auto& P=*C.Inherited;P.Target.Reset(C.Emitter);P.LocalSource.Reset(C.Source);P.LocalGraph.Reset(C.Graph);P.Parent=C.Data->VersionedParent;P.LastMerge=C.Data->VersionedParentAtLastMerge;
 auto Keep=[&](UObject* O)->bool{if(!O)return true;for(const auto& E:P.Objects)if(E.Object.Get()==O)return true;if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetOuter())||!Loaded(O->GetPackage())||P.Objects.Num()>=8192)return false;FYHInherited::FObject E;E.Object.Reset(O);E.Class.Reset(O->GetClass());E.Outer.Reset(O->GetOuter());E.Package.Reset(O->GetPackage());E.Path=O->GetPathName();P.Objects.Add(MoveTemp(E));return true;};
 if(!Keep(C.Emitter)||!Keep(C.Source)||!Keep(C.Graph))return Fail(R,TEXT("niagara_stage_inherited_local_unavailable"),TEXT("Held local source/graph roles required before inherited observers."));
 TFunction<bool(const FVersionedNiagaraEmitter&,int32)> Visit;
 Visit=[&](const FVersionedNiagaraEmitter& Slot,int32 Depth)->bool
 {
  if(!Slot.Emitter.IsResolved())return false;if(Slot.Emitter==nullptr)return true;auto* E=Cast<UNiagaraEmitter>(YHResolved(Slot.Emitter));if(!Loaded(E)||E==C.Emitter||Depth>16||P.Roles.Num()>=32||E->GetClass()!=UNiagaraEmitter::StaticClass())return false;
  for(const auto& Old:P.Roles)if(Old->Emitter.Get()==E&&Old->Version==Slot.Version)return Old->Ready;
  auto V=MakeUnique<FYHInherited::FRole>();auto* Role=V.Get();V->Emitter.Reset(E);V->Version=Slot.Version;if(!Keep(E))return false;
  auto* D=E->GetEmitterData(Slot.Version);if(!D||!D->GraphSource.IsResolved()||!D->VersionedParent.Emitter.IsResolved()||!D->VersionedParentAtLastMerge.Emitter.IsResolved()||!D->GetSimulationStages().IsEmpty())return false;V->Data=D;V->Parent=D->VersionedParent;V->LastMerge=D->VersionedParentAtLastMerge;
  auto* Source=Cast<UNiagaraScriptSource>(YHResolved(D->GraphSource));auto* G=Source&&Source->NodeGraph.IsResolved()?Cast<UNiagaraGraph>(YHResolved(Source->NodeGraph)):nullptr;
  if(!Loaded(Source)||!Loaded(G)||Source==C.Source||G==C.Graph||Source->GetClass()!=UNiagaraScriptSource::StaticClass()||G->GetClass()!=UNiagaraGraph::StaticClass()||G->GetOuter()!=Source||Source->bIsCompilationCopy||Source->bIsReleased||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=UEdGraphSchema_Niagara::StaticClass()||G->Nodes.Num()>MaxNodes||!Keep(Source)||!Keep(G))return false;
  V->Source.Reset(Source);V->Graph.Reset(G);V->Dirty=G->GetPackage()->IsDirty();
  for(const auto& NodeSlot:G->Nodes){if(!NodeSlot.IsResolved())return false;auto* N=Cast<UEdGraphNode>(YHResolved(NodeSlot));if(!Loaded(N)||N->GetOuter()!=G||!Keep(N)||N->Pins.Num()>MaxPins)return false;FYHInherited::FNode NR;NR.Node.Reset(N);NR.Pins=N->Pins;
   if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(!F->FunctionScript.IsResolved()||(F->FunctionScript!=nullptr&&!Keep(YHResolved(F->FunctionScript))))return false;}
   for(auto* Q:N->Pins){if(!Q||Q->GetOwningNodeUnchecked()!=N||!Q->DefaultObject.IsResolved()||(Q->DefaultObject!=nullptr&&!Keep(YHResolved(Q->DefaultObject)))||!Keep(Q->PinType.PinSubCategoryObject.Get())||!Keep(Q->PinType.PinValueType.TerminalSubCategoryObject.Get()))return false;}V->Nodes.Add(MoveTemp(NR));}
  P.Roles.Add(MoveTemp(V));int32 TotalParentNodes=0;for(const auto& Row:P.Roles){TotalParentNodes+=Row->Nodes.Num();if(TotalParentNodes>4096)return false;}if(!YHPhysical(R,C))return false;
  const TFunction<bool()> Fence=[&](){return YHPhysical(R,C);};Role->Before=GraphState(G,Fence);if(!Role->Before.Complete||!YHPhysical(R,C))return false;
  // Store visiting state before descending; LastMerge may legitimately be a local non-versioned snapshot.
  if(!Visit(Role->Parent,Depth+1)||!Visit(Role->LastMerge,Depth+1))return false;Role->Ready=true;return true;
 };
 if(!Visit(P.Parent,0)||!Visit(P.LastMerge,0))return Fail(R,TEXT("niagara_stage_inherited_source_pending"),TEXT("Finite loaded acyclic parent/LastMerge closure with distinct readonly local graphs and no parent stages required; aliases, inherited parent stages, unresolved/custom source variants remain incomplete."));
 R.Data->SetBoolField(TEXT("inherited_local_stage_supported"),true);R.Data->SetNumberField(TEXT("inherited_readonly_version_roles"),P.Roles.Num());R.Data->SetStringField(TEXT("inherited_stage_scope"),TEXT("Existing IB local stage continuation only; loaded nullable Parent/LastMerge closure, distinct source/graph and empty parent stage arrays. Readonly authored graph and identity checks at SDK boundaries, not compiler-cache/pixel/runtime equivalence. LastMerge may share target package; target package dirty lifecycle is allowed. No parent writer, merge, detach, loader or source save. Wider direct/Batch, parent-stage, alias and inheritance variants remain incomplete; usage validation pending."));return YHCurrent(R,C);
}
FString Handles(UNiagaraSystem* S)
{
    auto J = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> A;
    for (const auto& H : S->GetEmitterHandles())
    { auto R = MakeShared<FJsonObject>(); const auto I = H.GetInstance(); R->SetStringField(TEXT("id"), Guid(H.GetId()));
        R->SetStringField(TEXT("name"), H.GetName().ToString()); R->SetBoolField(TEXT("enabled"), H.GetIsEnabled());
        R->SetStringField(TEXT("emitter_path"), Path(I.Emitter)); R->SetStringField(TEXT("version"), Guid(I.Version)); A.Add(MakeShared<FJsonValueObject>(R)); }
    J->SetArrayField(TEXT("emitters"), A); return Text(J);
}
bool Prepare(FResult& R, FContext& C, const FString& SystemPath, const FString& IndexJson,
    const FString& Name, bool bWrite, bool bAllowSourceEdit)
{
    const auto& V = FEngineVersion::Current(); FString Object; int32 RequestedIndex;
    if (!IsEditorGameThread() || IsPlaySessionActive() || V.GetMajor() != 5 || V.GetMinor() != 8)
    { return Fail(R, TEXT("niagara_stage_editor_state_invalid"), TEXT("Requires UE 5.8.x editor game thread outside PIE/SIE.")); }
    if (!ObjectPath(SystemPath, Object) || !Index(IndexJson, RequestedIndex) || !Bounded(Name, 256))
    { return Fail(R, TEXT("niagara_stage_selector_invalid"), TEXT("Use precise system path, null/nonnegative exact int32 emitter index and bounded name.")); }
    C.System = FindObject<UNiagaraSystem>(nullptr, *Object);
    if (!Loaded(C.System) || C.System->GetClass() != UNiagaraSystem::StaticClass() || !C.System->IsAsset() ||
        C.System->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn) || C.System->GetEmitterHandles().Num() > MaxEmitters)
    { return Fail(R, TEXT("niagara_stage_loaded_system_required"), TEXT("An ordinary already-loaded native Niagara system is required; no loading occurs here.")); }
    const FString Package = C.System->GetOutermost()->GetName(); const bool Temporary = Package.StartsWith(TEXT("/Game/CubelessMCPTests/")) || Package.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/"));
    if (bWrite && (!Package.StartsWith(TEXT("/Game/")) || (!Temporary && !bAllowSourceEdit)))
    { return Fail(R, TEXT("niagara_stage_write_scope_forbidden"), TEXT("Writes require generated/test roots or explicit /Game source-edit opt-in.")); }
    int32 Matches = 0; const FString TrimmedName = Name.TrimStartAndEnd();
    for (int32 N = 0; N < C.System->GetEmitterHandles().Num(); ++N)
    {
        const auto& H = C.System->GetEmitterHandles()[N];
        if ((RequestedIndex == INDEX_NONE || RequestedIndex == N) && (TrimmedName.IsEmpty() || TrimmedName.Equals(H.GetName().ToString(), ESearchCase::IgnoreCase)))
        {
            ++Matches; if (H.GetEmitterMode() != ENiagaraEmitterMode::Standard) { return Fail(R, TEXT("niagara_stage_stateless_unsupported"), TEXT("Only standard scripted emitters are supported.")); }
            const auto I = H.GetInstance(); C.Emitter = I.Emitter; C.Version = I.Version; C.Data = H.GetEmitterData(); C.EmitterIndex = N; C.EmitterName = H.GetName().ToString();
        }
    }
    if (Matches != 1 || !Loaded(C.Emitter) || C.Emitter->GetClass() != UNiagaraEmitter::StaticClass() || !C.Data ||
        C.Emitter->GetEmitterData(C.Version) != C.Data || !C.Version.IsValid() || C.Emitter->GetOutermost() != C.System->GetOutermost() || C.Emitter->IsAsset())
    { return Fail(R, TEXT("niagara_stage_unique_owned_emitter_required"), TEXT("Resolve exactly one loaded native emitter instance/version owned by the target system.")); }
    C.Source = Cast<UNiagaraScriptSource>(C.Data->GraphSource); C.Graph = C.Source ? C.Source->NodeGraph.Get() : nullptr;
    if (!Loaded(C.Source) || C.Source->GetClass() != UNiagaraScriptSource::StaticClass() || C.Source->bIsCompilationCopy || C.Source->bIsReleased ||
        !Loaded(C.Graph) || C.Graph->GetClass() != UNiagaraGraph::StaticClass() || C.Source->GetOutermost() != C.System->GetOutermost() ||
        C.Graph->GetOutermost() != C.System->GetOutermost() || C.Graph->GetOuter() != C.Source ||
        C.Graph->Schema != UEdGraphSchema_Niagara::StaticClass() || !UEdGraphSchema_Niagara::StaticClass()->GetDefaultObject(false))
    { return Fail(R, TEXT("niagara_stage_source_graph_unsupported"), TEXT("Requires an already-loaded owned native source graph and existing Niagara schema CDO.")); }
    if (bWrite && (!Loaded(C.Data->UpdateScriptProps.Script) || C.Data->UpdateScriptProps.Script->GetLatestSource() != C.Source))
    { return Fail(R, TEXT("niagara_stage_recompile_update_source_unsupported"), TEXT("Public stage RequestRecompile requires the selected owned emitter update Script to use this exact observed source.")); }
    if (bWrite && C.System->HasOutstandingCompilationRequests(true))
    { return Fail(R, TEXT("niagara_stage_compile_outstanding"), TEXT("Inspect compilation readiness before editing; no waiting or flushing is performed.")); }
    if (bWrite) { for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        { if (Loaded(*It) && (*It)->GetAsset() == C.System && (*It)->GetWorld()) { return Fail(R, TEXT("niagara_stage_live_component_unsupported"), TEXT("Do not modify a system referenced by a live world component.")); } } }
    if(bWrite&&!YHCapture(R,C))return false;
    C.BeforeGraph = GraphState(C.Graph); C.BeforeStages = C.Data->GetSimulationStages(); bool Complete = C.BeforeGraph.Complete;
    if (C.BeforeStages.Num() >= MaxStages) { Complete = false; }
    TSet<FGuid> UsageIds; TSet<FString> StageNames;
    for (int32 N = 0; N < C.BeforeStages.Num(); ++N)
    {
        auto Row = StageRow(C.BeforeStages[N], N, Complete); C.BeforeStageRows.Add(MakeShared<FJsonValueObject>(Row));
        if(!IBCheck(TEXT("after_prepare_stage_row")))return false;
        auto* S = C.BeforeStages[N];
        if (!Loaded(S) || S->GetOuter() != C.Emitter || !Loaded(S->Script) || S->Script->GetLatestSource() != C.Source ||
            S->Script->GetUsage() != ENiagaraScriptUsage::ParticleSimulationStageScript || !S->Script->GetUsageId().IsValid() ||
            UsageIds.Contains(S->Script->GetUsageId()) || StageNames.Contains(S->SimulationStageName.ToString().ToLower())) { Complete = false; }
        if (Loaded(S) && Loaded(S->Script)) { UsageIds.Add(S->Script->GetUsageId()); StageNames.Add(S->SimulationStageName.ToString().ToLower()); }
    }
    // Preserve other native emitter graphs and both system graphs, without
    // following arbitrary function references or claiming a recursive audit.
    auto AddOther = [&](UNiagaraScriptSourceBase* B)
    { auto* S = Cast<UNiagaraScriptSource>(B); if (!Loaded(S) || !Loaded(S->NodeGraph) || S->bIsCompilationCopy || S->bIsReleased) { Complete = false; return; }
        if (S->NodeGraph != C.Graph && !C.OtherGraphs.Contains(Path(S->NodeGraph))) { auto G = GraphState(S->NodeGraph); Complete &= G.Complete; C.OtherGraphs.Add(Path(S->NodeGraph), G); C.OtherGraphObjects.Add(Path(S->NodeGraph), S->NodeGraph); } };
    if (!Loaded(C.System->GetSystemSpawnScript()) || !Loaded(C.System->GetSystemUpdateScript())) { Complete = false; }
    else { AddOther(C.System->GetSystemSpawnScript()->GetLatestSource()); AddOther(C.System->GetSystemUpdateScript()->GetLatestSource()); }
    for (const auto& H : C.System->GetEmitterHandles())
    { const auto* D = H.GetEmitterData(); if (H.GetEmitterMode() != ENiagaraEmitterMode::Standard || !D) { Complete = false; } else { AddOther(D->GraphSource); } }
    if (!Complete) { return Fail(R, TEXT("niagara_stage_observation_incomplete"), TEXT("Bounded graph/stage/other-source identities or binding defaults are incomplete; no mutation dispatched.")); }
    C.HandlesBefore = Handles(C.System); C.DirtyBefore = C.System->GetOutermost()->IsDirty();
    R.Data->SetStringField(TEXT("system_path"), Path(C.System)); R.Data->SetStringField(TEXT("emitter_path"), Path(C.Emitter));
    R.Data->SetNumberField(TEXT("emitter_index"), C.EmitterIndex); R.Data->SetStringField(TEXT("emitter_name"), C.EmitterName);
    R.Data->SetStringField(TEXT("emitter_version"), Guid(C.Version)); R.Data->SetStringField(TEXT("sim_target"), Enum(C.Data->SimTarget));
    R.Data->SetStringField(TEXT("script_source_path"), Path(C.Source)); R.Data->SetStringField(TEXT("graph_path"), Path(C.Graph));
    R.Data->SetStringField(TEXT("graph_change_id_before"), Guid(C.Graph->GetChangeID()));
    R.Data->SetObjectField(TEXT("before"), C.BeforeGraph.Json); R.Data->SetArrayField(TEXT("stages_before"), C.BeforeStageRows);
    R.Data->SetNumberField(TEXT("stage_count_before"), C.BeforeStages.Num()); R.Data->SetBoolField(TEXT("package_dirty_before"), C.DirtyBefore);
    R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.System)); R.Data->SetStringField(TEXT("write_scope"), TEXT("grid_simulation_stage_create")); return IBCapture(R,C);
}
bool MapPin(UEdGraphNode* N, EEdGraphPinDirection D, UEdGraphPin*& Out)
{
    Out = nullptr;
    for (UEdGraphPin* P : N->Pins)
    { if (P && P->Direction == D && !P->bOrphanedPin && UEdGraphSchema_Niagara::PinToTypeDefinition(P) == FNiagaraTypeDefinition::GetParameterMapDef())
        { if (Out) { return false; } Out = P; } }
    return Out != nullptr;
}
bool PreserveOther(FResult& R, const FContext& C)
{
    if(!YHCurrent(R,C))return false;
    bool Complete = true, Preserved = C.HandlesBefore == Handles(C.System); TArray<TSharedPtr<FJsonValue>> Rows;
    for (const auto& P : C.OtherGraphs)
    { const auto* Ptr = C.OtherGraphObjects.Find(P.Key); auto Current = GraphState(Ptr ? *Ptr : nullptr); Complete &= Current.Complete; Preserved &= SameGraph(P.Value, Current);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("graph_path"), P.Key); Row->SetBoolField(TEXT("preserved"), SameGraph(P.Value, Current));
        Row->SetObjectField(TEXT("before"), P.Value.Json); Row->SetObjectField(TEXT("after"), Current.Json); Rows.Add(MakeShared<FJsonValueObject>(Row)); }
    R.Data->SetArrayField(TEXT("other_graph_observations"), Rows); R.Data->SetBoolField(TEXT("other_graphs_preserved"), Preserved);
    R.Data->SetBoolField(TEXT("system_handles_preserved"), C.HandlesBefore == Handles(C.System));
    if (!Complete) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_stage_other_source_unobservable"), TEXT("Other native source observation is incomplete; retain changes and do not retry.")); }
    if (!Preserved) { R.bChanged = true; return Fail(R, TEXT("niagara_stage_other_source_changed"), TEXT("Unselected source graph or handle identities changed; retain actual state.")); } return true;
}
bool StageRows(FResult& R, const FContext& C, TArray<TSharedPtr<FJsonValue>>& Rows)
{
    bool Complete = true; int32 N = 0;
    if (C.Data->GetSimulationStages().Num() > MaxStages) { Complete = false; }
    else { const auto Snapshot=C.Data->GetSimulationStages(); for (auto* S : Snapshot) { Rows.Add(MakeShared<FJsonValueObject>(StageRow(S, N++, Complete))); if(!IBCheck(TEXT("after_stage_row")))return false; } }
    R.Data->SetArrayField(TEXT("stages_after"), Rows); R.Data->SetNumberField(TEXT("stage_count_after"), C.Data->GetSimulationStages().Num());
    if (!Complete) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_stage_after_unobservable"), TEXT("Stage/script/default observation is incomplete; no compile/save or retry is safe.")); } return true;
}
bool VerifyPriorStages(FResult& R, const FContext& C, UNiagaraSimulationStageGeneric* Added, int32 ExpectedIndex,
    const TArray<TSharedPtr<FJsonValue>>& Rows)
{
    auto Expected = C.BeforeStages; if (Added) { Expected.Insert(Added, ExpectedIndex); }
    bool Preserved = Expected == C.Data->GetSimulationStages();
    for (int32 N = 0; N < C.BeforeStages.Num(); ++N)
    {
        const int32 CurrentIndex = C.Data->GetSimulationStages().IndexOfByKey(C.BeforeStages[N]);
        if (!Rows.IsValidIndex(CurrentIndex)) { Preserved = false; continue; }
        auto Current = MakeShared<FJsonObject>(); Current->Values = Rows[CurrentIndex]->AsObject()->Values; Current->SetNumberField(TEXT("stage_index"), N);
        Preserved &= Text(Current) == Text(C.BeforeStageRows[N]->AsObject().ToSharedRef());
    }
    R.Data->SetBoolField(TEXT("existing_stage_metadata_preserved"), Preserved); R.Data->SetBoolField(TEXT("stage_order_verified"), Expected == C.Data->GetSimulationStages());
    if (!Preserved) { return Fail(R, TEXT("niagara_stage_order_or_prior_metadata_changed"), TEXT("Existing stages or expected one-stage order differ; observed changes retained.")); } return true;
}
bool VerifyGraphAddition(FResult& R, const FContext& C, const FGraphState& After, UNiagaraNodeOutput* O, UNiagaraNodeInput* I)
{
    bool Preserved = After.Complete && After.Nodes.Num() == C.BeforeGraph.Nodes.Num() + 2;
    for (const auto& P : C.BeforeGraph.Nodes) { const auto* V = After.Nodes.Find(P.Key); Preserved &= V && *V == P.Value; }
    for (const auto& P : C.BeforeGraph.Pins) { const auto* V = After.Pins.Find(P.Key); Preserved &= V && *V == P.Value; }
    auto ExpectedLinks = C.BeforeGraph.Links; UEdGraphPin* In = nullptr; UEdGraphPin* Out = nullptr;
    const bool Shape = Loaded(O) && Loaded(I) && O->GetOuter() == C.Graph && I->GetOuter() == C.Graph && O->GetClass() == UNiagaraNodeOutput::StaticClass() &&
        I->GetClass() == UNiagaraNodeInput::StaticClass() && C.Graph->Nodes.Contains(O) && C.Graph->Nodes.Contains(I) &&
        MapPin(O, EGPD_Input, In) && MapPin(I, EGPD_Output, Out) && O->Pins.Num() == 1 && I->Pins.Num() == 1 &&
        I->Input.GetName() == FName(TEXT("InputMap")) && I->Input.GetType() == FNiagaraTypeDefinition::GetParameterMapDef() && I->Usage == ENiagaraInputNodeUsage::Parameter &&
        O->Outputs.Num() == 1 && O->Outputs[0].GetName() == FName(TEXT("Out")) && O->Outputs[0].GetType() == FNiagaraTypeDefinition::GetParameterMapDef();
    if (Shape) { ExpectedLinks.FindOrAdd(PinId(In)).Add(PinId(Out)); ExpectedLinks.FindOrAdd(PinId(Out)).Add(PinId(In)); }
    bool ExactLinks = Shape && ExpectedLinks.Num() == After.Links.Num();
    for (const auto& P : ExpectedLinks) { const auto* V = After.Links.Find(P.Key); ExactLinks &= V && *V == P.Value; }
    R.Data->SetBoolField(TEXT("prior_node_pin_metadata_preserved"), Preserved); R.Data->SetBoolField(TEXT("native_stage_graph_shape_verified"), Shape);
    R.Data->SetBoolField(TEXT("exact_stage_graph_edge_union_verified"), ExactLinks);
    if (!After.Complete) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_stage_graph_after_unobservable"), TEXT("Stage graph was modified but cannot be observed completely; stop without retry.")); }
    return Preserved && Shape && ExactLinks ? true : Fail(R, TEXT("niagara_stage_graph_postcondition_failed"), TEXT("Stage graph is not exactly two new native map nodes plus one edge with prior nodes/pins/links preserved."));
}
void Finish(FResult& R, FContext& C, const FGraphState& AfterAddition, bool bSave, bool bRequestCompile)
{
    if(!IBCheck(TEXT("stage_finish_admission")))return;
    if(IBArmAdd(R,C,AfterAddition,bSave,bRequestCompile))return;
    if (R.ErrorCode.IsEmpty() && R.bChangeStateKnown)
    {
        if (bRequestCompile)
        { R.Stage = TEXT("request_compile"); R.Data->SetBoolField(TEXT("compile_requested"), true); R.Data->SetBoolField(TEXT("compile_attempted"), true);
            R.Data->SetNumberField(TEXT("explicit_compile_request_count"), 1);
            R.Data->SetBoolField(TEXT("compile_request_returned"), C.System->RequestCompile(false)); }
        if (bSave)
        {
            auto State = CompileState(C.System); R.Data->SetObjectField(TEXT("save_compile_state"), State);
            if (!State->GetBoolField(TEXT("ready_to_save")))
            { R.Stage = TEXT("save_deferred"); R.Data->SetBoolField(TEXT("save_deferred"), true); Fail(R, TEXT("niagara_save_deferred_compile_pending"), TEXT("Stage remains. Outstanding/required compilation would block PreSave; inspect readiness and explicitly save the exact system. Do not replay creation.")); }
            else
            { R.Stage = TEXT("save"); R.Data->SetBoolField(TEXT("save_attempted"), true); R.Data->SetNumberField(TEXT("exact_save_attempt_count"), 1); const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(C.System, false);
                R.Data->SetBoolField(TEXT("saved"), Saved);
                if (!Saved) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_stage_save_failed"), TEXT("Save returned false; disk outcome is uncertain and the created stage remains.")); }
                else if (C.System->GetOutermost()->IsDirty()) { Fail(R, TEXT("niagara_stage_save_postcondition_failed"), TEXT("Save returned true but the target package remains dirty.")); } }
        }
    }
    auto After = GraphState(C.Graph); R.Data->SetObjectField(TEXT("after"), After.Json); R.Data->SetBoolField(TEXT("package_dirty_after"), C.System->GetOutermost()->IsDirty());
    R.Data->SetStringField(TEXT("graph_change_id_after"), Guid(C.Graph->GetChangeID()));
    R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.System));
    TArray<TSharedPtr<FJsonValue>> Rows; StageRows(R, C, Rows);
    const TArray<TSharedPtr<FJsonValue>>* BeforeRows = nullptr;
    if (R.Data->TryGetArrayField(TEXT("stages_after_addition"), BeforeRows) && BeforeRows)
    {
        bool Same = BeforeRows->Num() == Rows.Num();
        if (Same) { for (int32 N = 0; N < Rows.Num(); ++N) { Same &= Text((*BeforeRows)[N]->AsObject().ToSharedRef()) == Text(Rows[N]->AsObject().ToSharedRef()); } }
        R.Data->SetBoolField(TEXT("stage_settings_preserved_across_compile_save"), Same);
        if (!Same) { Fail(R, TEXT("niagara_stage_compile_save_settings_changed"), TEXT("Stage/script/binding metadata changed across compile/save; observed changes retained.")); }
    }
    if (!After.Complete) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_stage_graph_after_unobservable"), TEXT("Final graph observation incomplete; no automatic retry.")); }
    else
    { R.bChanged |= !SameGraph(C.BeforeGraph, After) || C.BeforeStages != C.Data->GetSimulationStages() || C.DirtyBefore != C.System->GetOutermost()->IsDirty();
        const bool Preserved = SameGraph(AfterAddition, After); R.Data->SetBoolField(TEXT("graph_preserved_across_compile_save"), Preserved);
        if (!Preserved) { Fail(R, TEXT("niagara_stage_compile_save_graph_changed"), TEXT("Graph identity/topology/literals changed across compile/save; actual graph retained.")); } }
    const bool Other = PreserveOther(R, C); R.Data->SetBoolField(TEXT("compile_and_save_suppressed_unknown_outcome"),
        !R.bChangeStateKnown && !R.Data->GetBoolField(TEXT("compile_attempted")) && !R.Data->GetBoolField(TEXT("save_attempted")));
    R.bSuccess = R.bChangeStateKnown && Other && R.ErrorCode.IsEmpty(); R.bPostconditionPassed = R.bSuccess; if (R.bSuccess) { R.Stage = TEXT("complete"); }
}
struct FSettingsPatch
{
    bool HasEnabled = false, Enabled = false, HasIteration = false, HasDI = false,
        HasDispatch = false, HasElement = false, HasExecute = false, HasIterations = false,
        HasBinding = false, HasForce = false, Force = false, HasThreads = false, Threads = false;
    ENiagaraIterationSource Iteration = ENiagaraIterationSource::DataInterface;
    ENiagaraGpuDispatchType Dispatch = ENiagaraGpuDispatchType::OneD;
    ENiagaraDirectDispatchElementType Element = ENiagaraDirectDispatchElementType::NumThreads;
    ENiagaraSimStageExecuteBehavior Execute = ENiagaraSimStageExecuteBehavior::Always;
    bool HasCount[3] = {false, false, false}; int32 Counts[3] = {0, 0, 0}, Iterations = 1;
    FString DIName, BindingName;
};
bool SettingsOptions(FResult& R, const FString& Json, FSettingsPatch& O)
{
    TSharedPtr<FJsonObject> J;
    if (Json.Len() > 8192 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), J) || !J.IsValid())
    { return Fail(R, TEXT("niagara_stage_options_invalid"), TEXT("Settings require a bounded presence-only JSON object.")); }
    const TSet<FString> Allowed = {TEXT("enabled"), TEXT("iteration_source"), TEXT("data_interface_name"),
        TEXT("direct_dispatch_type"), TEXT("direct_dispatch_element_type"), TEXT("execute_behavior"), TEXT("element_count"),
        TEXT("element_count_x"), TEXT("element_count_y"), TEXT("element_count_z"), TEXT("num_iterations"),
        TEXT("num_iterations_binding"), TEXT("gpu_dispatch_force_linear"), TEXT("override_gpu_dispatch_num_threads")};
    for (const auto& P : J->Values) { if (!Allowed.Contains(FString(P.Key))) { return Fail(R, TEXT("niagara_stage_option_unsupported"), TEXT("Unknown existing-stage setting; no mutation dispatched.")); } }
    if (J->Values.IsEmpty()) { return Fail(R, TEXT("niagara_stage_settings_required"), TEXT("At least one setting field is required; selectors alone do not modify a stage.")); }
    auto String = [&](const TCHAR* K, bool& Has, FString& Value, int32 Limit)
    { Has = J->HasField(K); return !Has || (J->TryGetStringField(K, Value) && Bounded(Value, Limit) && !(Value = Value.TrimStartAndEnd()).IsEmpty()); };
    FString I, D, E, X;
    if (!String(TEXT("iteration_source"), O.HasIteration, I, 64) || !String(TEXT("direct_dispatch_type"), O.HasDispatch, D, 64) ||
        !String(TEXT("direct_dispatch_element_type"), O.HasElement, E, 64) || !String(TEXT("execute_behavior"), O.HasExecute, X, 64) ||
        !String(TEXT("data_interface_name"), O.HasDI, O.DIName, 256) || !String(TEXT("num_iterations_binding"), O.HasBinding, O.BindingName, 256))
    { return Fail(R, TEXT("niagara_stage_setting_text_invalid"), TEXT("Present setting strings must be bounded and nonempty; omit absent options.")); }
    I = Token(I); D = Token(D); E = Token(E); X = Token(X);
    if (O.HasIteration)
    {
        if (I == TEXT("particles")) { O.Iteration = ENiagaraIterationSource::Particles; }
        else if (I == TEXT("datainterface") || I == TEXT("datainterfaceiteration")) { O.Iteration = ENiagaraIterationSource::DataInterface; }
        else if (I == TEXT("directset") || I == TEXT("direct")) { O.Iteration = ENiagaraIterationSource::DirectSet; }
        else { return Fail(R, TEXT("niagara_stage_enum_invalid"), TEXT("Unsupported iteration_source.")); }
    }
    if (O.HasDispatch)
    {
        if (D == TEXT("oned") || D == TEXT("1d")) { O.Dispatch = ENiagaraGpuDispatchType::OneD; }
        else if (D == TEXT("twod") || D == TEXT("2d")) { O.Dispatch = ENiagaraGpuDispatchType::TwoD; }
        else if (D == TEXT("threed") || D == TEXT("3d")) { O.Dispatch = ENiagaraGpuDispatchType::ThreeD; }
        else if (D == TEXT("custom")) { O.Dispatch = ENiagaraGpuDispatchType::Custom; }
        else { return Fail(R, TEXT("niagara_stage_enum_invalid"), TEXT("Unsupported direct_dispatch_type.")); }
    }
    if (O.HasElement)
    {
        if (E == TEXT("numthreads") || E == TEXT("threads")) { O.Element = ENiagaraDirectDispatchElementType::NumThreads; }
        else if (E == TEXT("numthreadsnoclipping") || E == TEXT("threadsnoclipping") || E == TEXT("noclip")) { O.Element = ENiagaraDirectDispatchElementType::NumThreadsNoClipping; }
        else if (E == TEXT("numgroups") || E == TEXT("groups")) { O.Element = ENiagaraDirectDispatchElementType::NumGroups; }
        else { return Fail(R, TEXT("niagara_stage_enum_invalid"), TEXT("Unsupported direct_dispatch_element_type.")); }
    }
    if (O.HasExecute)
    {
        if (X == TEXT("always")) { O.Execute = ENiagaraSimStageExecuteBehavior::Always; }
        else if (X == TEXT("onsimulationreset") || X == TEXT("onreset")) { O.Execute = ENiagaraSimStageExecuteBehavior::OnSimulationReset; }
        else if (X == TEXT("notonsimulationreset") || X == TEXT("notonreset")) { O.Execute = ENiagaraSimStageExecuteBehavior::NotOnSimulationReset; }
        else { return Fail(R, TEXT("niagara_stage_enum_invalid"), TEXT("Unsupported execute_behavior.")); }
    }
    constexpr const TCHAR* Keys[] = {TEXT("element_count_x"), TEXT("element_count_y"), TEXT("element_count_z")};
    for (int32 N = 0; N < 3; ++N) { O.HasCount[N] = J->HasField(Keys[N]); if (O.HasCount[N] && !Int(J->TryGetField(Keys[N]), O.Counts[N]))
        { return Fail(R, TEXT("niagara_stage_count_invalid"), TEXT("Element counts require nonnegative exact int32.")); } }
    if (J->HasField(TEXT("element_count")))
    { const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
        if (!J->TryGetArrayField(TEXT("element_count"), A) || !A || A->Num() < 1 || A->Num() > 3)
        { return Fail(R, TEXT("niagara_stage_count_invalid"), TEXT("element_count requires 1..3 exact int32 values.")); }
        for (int32 N = 0; N < A->Num(); ++N) { O.HasCount[N] = true; if (!Int((*A)[N], O.Counts[N]))
            { return Fail(R, TEXT("niagara_stage_count_invalid"), TEXT("Invalid element_count item; array entries override explicit axes.")); } }
    }
    O.HasIterations = J->HasField(TEXT("num_iterations"));
    if (O.HasIterations && !Int(J->TryGetField(TEXT("num_iterations")), O.Iterations, 1))
    { return Fail(R, TEXT("niagara_stage_count_invalid"), TEXT("num_iterations requires exact int32 >= 1.")); }
    O.HasEnabled = J->HasField(TEXT("enabled")); O.HasForce = J->HasField(TEXT("gpu_dispatch_force_linear")); O.HasThreads = J->HasField(TEXT("override_gpu_dispatch_num_threads"));
    if ((O.HasEnabled && !J->TryGetBoolField(TEXT("enabled"), O.Enabled)) || (O.HasForce && !J->TryGetBoolField(TEXT("gpu_dispatch_force_linear"), O.Force)) ||
        (O.HasThreads && !J->TryGetBoolField(TEXT("override_gpu_dispatch_num_threads"), O.Threads)))
    { return Fail(R, TEXT("niagara_stage_bool_invalid"), TEXT("Present enabled/GPU flags require exact JSON bool.")); }
    auto Qualified = [](const FString& V) { return V.Contains(TEXT(".")) && !V.StartsWith(TEXT(".")) && !V.EndsWith(TEXT(".")) &&
        !V.Contains(TEXT("/")) && !V.Contains(TEXT("\\")) && !V.Contains(TEXT(":")) && !V.Contains(TEXT("'")) && !V.Contains(TEXT("\"")); };
    if ((O.HasDI && !Qualified(O.DIName)) || (O.HasBinding && (!Qualified(O.BindingName) || !O.BindingName.StartsWith(TEXT("User.")) ||
        O.BindingName.Len() <= 5 || O.BindingName.Contains(TEXT("CubelessMCP"), ESearchCase::IgnoreCase) || O.BindingName.Contains(TEXT("UnrealMCP"), ESearchCase::IgnoreCase))))
    { return Fail(R, TEXT("niagara_stage_binding_name_invalid"), TEXT("Use a qualified Grid DI name or existing User.* int binding without MCP references.")); }
    return true;
}
FORCENOINLINE const FVector* StorePosition(const FNiagaraParameterStore& Store, const FName& Name)
{ return Store.GetPositionParameterValue(Name); }
TSharedRef<FJsonObject> StoreState(UNiagaraSystem* System, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); const auto& Store = System->GetExposedParameters(); TArray<FNiagaraVariable> Variables;
    Store.GetParameters(Variables); Variables.Sort([](const FNiagaraVariable& A, const FNiagaraVariable& B) { return A.GetName().ToString() < B.GetName().ToString(); });
    if (Store.GetOwner() != System || Variables.Num() > 512 || Store.GetParameterDataArray().Num() > 65536 || Store.GetUObjects().Num() > 512 || Store.GetDataInterfaces().Num() > 512)
    { Complete = false; J->SetBoolField(TEXT("observation_complete"), false); return J; }
    TArray<TSharedPtr<FJsonValue>> Rows, RedirectRows;
    for (const auto& V : Variables)
    {
        const auto& T = V.GetType(); auto Row = Variable(V); const int32 Offset = Store.IndexOf(V), Size = V.GetSizeInBytes();
        Row->SetNumberField(TEXT("offset"), Offset); Row->SetNumberField(TEXT("size_bytes"), Size);
        Row->SetStringField(TEXT("type_flags_string"), LexToString(T.GetFlags())); Row->SetStringField(TEXT("type_object_path"), T.GetClass() ? Path(T.GetClass()) : Path(T.GetScriptStruct()));
        if (!T.IsValid() || Offset < 0) { Complete = false; break; }
        if (T.IsDataInterface())
        { if (!Store.GetDataInterfaces().IsValidIndex(Offset)) { Complete = false; break; }
            UObject* O = Store.GetDataInterfaces()[Offset]; Row->SetStringField(TEXT("reference_path"), Path(O)); Row->SetStringField(TEXT("reference_class"), O ? Path(O->GetClass()) : FString()); }
        else if (T.IsUObject())
        { if (!Store.GetUObjects().IsValidIndex(Offset)) { Complete = false; break; }
            UObject* O = Store.GetUObjects()[Offset].Get(); Row->SetStringField(TEXT("reference_path"), Path(O)); Row->SetStringField(TEXT("reference_class"), O ? Path(O->GetClass()) : FString()); }
        else
        { if (Size < 0 || Offset > Store.GetParameterDataArray().Num() - Size) { Complete = false; break; }
            FString Bytes; for (int32 N = 0; N < Size; ++N) { Bytes += FString::Printf(TEXT("%02x"), Store.GetParameterDataArray()[Offset + N]); }
            Row->SetStringField(TEXT("raw_bytes_hex"), Bytes);
            if (T == FNiagaraTypeDefinition::GetPositionDef())
            { const FVector* P = StorePosition(Store, V.GetName()); if (!P) { Complete = false; break; }
                Row->SetStringField(TEXT("original_double_position"), FString::Printf(TEXT("%.17g,%.17g,%.17g"), P->X, P->Y, P->Z)); }
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TArray<FNiagaraVariable> Redirects; Store.GetUserParameters(Redirects);
    Redirects.Sort([](const FNiagaraVariable& A, const FNiagaraVariable& B) { return A.GetName().ToString() < B.GetName().ToString(); });
    for (const auto& V : Redirects)
    { FNiagaraVariableBase Canonical(V); if (!Store.RedirectUserVariable(Canonical) || Store.IndexOf(Canonical) == INDEX_NONE) { Complete = false; break; }
        auto Row = Variable(V); Row->SetObjectField(TEXT("canonical"), Variable(Canonical)); RedirectRows.Add(MakeShared<FJsonValueObject>(Row)); }
    J->SetStringField(TEXT("owner_path"), Path(Store.GetOwner())); J->SetNumberField(TEXT("layout_version"), Store.GetLayoutVersion());
    J->SetArrayField(TEXT("parameters"), Rows); J->SetArrayField(TEXT("user_redirects"), RedirectRows); J->SetBoolField(TEXT("observation_complete"), Complete);
    J->SetBoolField(TEXT("data_interface_payload_observed"), false); return J;
}
struct FSettingsContext
{
    FContext Target; UNiagaraSimulationStageGeneric* Stage = nullptr; int32 StageIndex = INDEX_NONE;
    TSharedPtr<FJsonObject> BeforeSettings, UsersBefore;
};
void InitializeSettings(FResult& R, const FString& PathValue, bool Save = false)
{
    Initialize(R, PathValue, Save); R.Data->SetStringField(TEXT("write_scope"), TEXT("simulation_stage_settings"));
    R.Data->SetStringField(TEXT("engine_api"), TEXT("Public native Generic stage fields/bindings; original SetEnabled and unconditional RequestRecompile scheduling, optional explicit System compile; no shader-completion inference"));
    R.Data->SetBoolField(TEXT("settings_write_attempted"), false); R.Data->SetBoolField(TEXT("script_invalidation_attempted"), false);
    R.Data->SetBoolField(TEXT("source_notification_attempted"), false); R.Data->SetBoolField(TEXT("enabled_notification_attempted"), false);
    R.Data->SetBoolField(TEXT("legacy_unconditional_compile_preserved"), true);
    R.Data->SetStringField(TEXT("compile_scheduling_difference"), TEXT("Original SetEnabled conditional RequestRecompile/OnChanged and final unconditional RequestRecompile are restored. Caller compile_requested is separate from these actual attempts; Engine internal fanout/completion remains unmeasured."));
}
bool PrepareSettings(FResult& R, FSettingsContext& S, const FString& SystemPath, const FString& EmitterIndexJson,
    const FString& EmitterName, const FString& StageIndexJson, const FString& StageName, bool Write, bool SourceEdit)
{
    FString Object; int32 EI, SI; const auto& V = FEngineVersion::Current();
    if (!IsEditorGameThread() || IsPlaySessionActive() || V.GetMajor() != 5 || V.GetMinor() != 8)
    { return Fail(R, TEXT("niagara_stage_editor_state_invalid"), TEXT("Requires UE5.8.x editor game thread outside PIE/SIE.")); }
    const FString EN = EmitterName.TrimStartAndEnd(), SN = StageName.TrimStartAndEnd();
    if (!ObjectPath(SystemPath, Object) || !Index(EmitterIndexJson, EI) || !Index(StageIndexJson, SI) || !Bounded(EN, 256) || !Bounded(SN, 256) || (SI == INDEX_NONE && SN.IsEmpty()))
    { return Fail(R, TEXT("niagara_stage_selector_invalid"), TEXT("Require stage index/name; joint optional selectors use exact nonnegative int32 and bounded names.")); }
    auto* System = FindObject<UNiagaraSystem>(nullptr, *Object);
    if (!Loaded(System) || System->GetClass() != UNiagaraSystem::StaticClass() || !System->IsAsset() || System->GetEmitterHandles().Num() > MaxEmitters)
    { return Fail(R, TEXT("niagara_stage_loaded_system_required"), TEXT("Exact already-loaded native system required; no loading occurs.")); }
    int32 Matches = 0, MatchedEmitter = INDEX_NONE;
    for (int32 EIndex = 0; EIndex < System->GetEmitterHandles().Num(); ++EIndex)
    {
        const auto& H = System->GetEmitterHandles()[EIndex];
        if ((EI != INDEX_NONE && EI != EIndex) || (!EN.IsEmpty() && !EN.Equals(H.GetName().ToString(), ESearchCase::IgnoreCase))) { continue; }
        const auto* D = H.GetEmitterData(); if (!D) { continue; }
        if (D->GetSimulationStages().Num() > MaxStages) { return Fail(R, TEXT("niagara_stage_observation_incomplete"), TEXT("Stage inventory exceeds the finite bound.")); }
        for (int32 N = 0; N < D->GetSimulationStages().Num(); ++N)
        { auto* Stage = D->GetSimulationStages()[N]; if (!Loaded(Stage)) { return Fail(R, TEXT("niagara_stage_observation_incomplete"), TEXT("Stage inventory contains an unobservable object.")); }
            if ((SI == INDEX_NONE || SI == N) && (SN.IsEmpty() || SN.Equals(Stage->SimulationStageName.ToString(), ESearchCase::IgnoreCase)))
            { ++Matches; MatchedEmitter = EIndex; S.StageIndex = N; S.Stage = Cast<UNiagaraSimulationStageGeneric>(Stage); }
        }
    }
    R.Data->SetNumberField(TEXT("joint_stage_match_count"), Matches);
    if (Matches != 1) { return Fail(R, TEXT("niagara_stage_unique_match_required"), TEXT("Combined emitter/stage selectors must match exactly one stage across the system.")); }
    if (!S.Stage || S.Stage->GetClass() != UNiagaraSimulationStageGeneric::StaticClass())
    { return Fail(R, TEXT("niagara_stage_generic_required"), TEXT("Only the exact selected native Generic stage is supported.")); }
    if (!Prepare(R, S.Target, SystemPath, LexToString(MatchedEmitter), FString(), Write, SourceEdit)) { return false; }
    auto& C = S.Target;
    if (!C.BeforeStages.IsValidIndex(S.StageIndex) || C.BeforeStages[S.StageIndex] != S.Stage || S.Stage->OuterEmitterVersion != C.Version)
    { return Fail(R, TEXT("niagara_stage_selected_identity_invalid"), TEXT("Selected stage index/owner/version changed during observation.")); }
    // Before changing settings, prove the exact owned update source that the
    // restored public RequestRecompile path will read.
    if (!Loaded(C.Data->UpdateScriptProps.Script) || C.Data->UpdateScriptProps.Script->GetLatestSource() != C.Source)
    { return Fail(R, TEXT("niagara_stage_update_source_unsupported"), TEXT("Owning emitter update Script must use the observed owned source.")); }
    bool Complete = true; S.UsersBefore = StoreState(C.System, Complete); S.BeforeSettings = C.BeforeStageRows[S.StageIndex]->AsObject();
    R.Data->SetObjectField(TEXT("graph_before"), C.BeforeGraph.Json); R.Data->SetObjectField(TEXT("graph_after"), C.BeforeGraph.Json);
    R.Data->SetObjectField(TEXT("before"), S.BeforeSettings); R.Data->SetObjectField(TEXT("after"), S.BeforeSettings);
    R.Data->SetObjectField(TEXT("user_store_before"), S.UsersBefore); R.Data->SetObjectField(TEXT("user_store_after"), S.UsersBefore);
    R.Data->SetNumberField(TEXT("stage_index"), S.StageIndex); R.Data->SetStringField(TEXT("stage_name"), S.Stage->SimulationStageName.ToString());
    R.Data->SetStringField(TEXT("stage_path"), Path(S.Stage)); R.Data->SetStringField(TEXT("target_usage_id"), Guid(S.Stage->Script->GetUsageId()));
    R.Data->SetStringField(TEXT("target_usage"), Enum(S.Stage->Script->GetUsage())); R.Data->SetStringField(TEXT("write_scope"), TEXT("simulation_stage_settings"));
    if (!Complete) { return Fail(R, TEXT("niagara_stage_user_store_unobservable"), TEXT("Native User identities/default bytes/reference identities are incomplete; no write dispatched.")); }
    return true;
}
bool ObserveSettings(FResult& R, FSettingsContext& S, const FSettingsPatch* Patch, TSharedPtr<FJsonObject>& Selected)
{
    auto& C = S.Target; auto Graph = GraphState(C.Graph); R.Data->SetObjectField(TEXT("graph_after"), Graph.Json);
    R.Data->SetStringField(TEXT("graph_change_id_after"), Guid(C.Graph->GetChangeID())); R.Data->SetBoolField(TEXT("package_dirty_after"), C.System->GetOutermost()->IsDirty());
    TArray<TSharedPtr<FJsonValue>> Rows; const bool RowsKnown = StageRows(R, C, Rows); if(!IBCheck(TEXT("after_settings_stage_rows")))return false; bool Complete = Graph.Complete && RowsKnown;
    bool Unselected = C.BeforeStages == C.Data->GetSimulationStages();
    if (Rows.IsValidIndex(S.StageIndex)) { Selected = Rows[S.StageIndex]->AsObject(); R.Data->SetObjectField(TEXT("after"), Selected); }
    else { Complete = false; }
    for (int32 N = 0; N < C.BeforeStageRows.Num(); ++N)
    { if (N != S.StageIndex && (!Rows.IsValidIndex(N) || Text(C.BeforeStageRows[N]->AsObject().ToSharedRef()) != Text(Rows[N]->AsObject().ToSharedRef()))) { Unselected = false; } }
    bool UsersKnown = true; auto Users = StoreState(C.System, UsersKnown); Complete &= UsersKnown;
    R.Data->SetObjectField(TEXT("user_store_after"), Users); const bool UsersSame = Text(S.UsersBefore.ToSharedRef()) == Text(Users);
    R.bChangeStateKnown = Complete;
    const bool GraphSame = SameGraph(C.BeforeGraph, Graph); const bool OtherSame = PreserveOther(R, C);
    bool StableSelected = Selected.IsValid();
    if (StableSelected)
    {
        auto A = MakeShared<FJsonObject>(); A->Values = S.BeforeSettings->Values; auto B = MakeShared<FJsonObject>(); B->Values = Selected->Values;
        auto Remove = [&](const TCHAR* K, bool Allow) { if (Allow) { A->RemoveField(K); B->RemoveField(K); } };
        if (Patch)
        { Remove(TEXT("enabled"), Patch->HasEnabled); Remove(TEXT("iteration_source"), Patch->HasIteration);
            Remove(TEXT("data_interface_name"), Patch->HasDI); Remove(TEXT("data_interface_class_path"), Patch->HasDI);
            Remove(TEXT("direct_dispatch_type"), Patch->HasDispatch); Remove(TEXT("direct_dispatch_element_type"), Patch->HasElement);
            Remove(TEXT("execute_behavior"), Patch->HasExecute); Remove(TEXT("gpu_dispatch_force_linear"), Patch->HasForce);
            Remove(TEXT("override_gpu_dispatch_num_threads"), Patch->HasThreads); Remove(TEXT("num_iterations"), Patch->HasIterations);
            Remove(TEXT("num_iterations_binding_observation"), Patch->HasIterations || Patch->HasBinding);
            constexpr const TCHAR* Keys[] = {TEXT("element_count_x"), TEXT("element_count_y"), TEXT("element_count_z")};
            for (int32 N = 0; N < 3; ++N) { Remove(Keys[N], Patch->HasCount[N]); }
            Remove(TEXT("element_count"), Patch->HasCount[0] || Patch->HasCount[1] || Patch->HasCount[2]); Remove(TEXT("compile_data"), true);
        }
        StableSelected = Text(A) == Text(B);
    }
    R.Data->SetBoolField(TEXT("graph_preserved"), GraphSame); R.Data->SetBoolField(TEXT("unselected_stages_preserved"), Unselected);
    R.Data->SetBoolField(TEXT("selected_unrequested_fields_preserved"), StableSelected); R.Data->SetBoolField(TEXT("user_store_preserved"), UsersSame);
    if (!Complete || !R.bChangeStateKnown) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_stage_settings_after_unobservable"), TEXT("Incomplete post-read; changes retained and compile/save suppressed.")); }
    R.bChanged |= !UsersSame || !GraphSame || !Unselected || (Selected && Text(S.BeforeSettings.ToSharedRef()) != Text(Selected.ToSharedRef())) ||
        C.DirtyBefore != C.System->GetOutermost()->IsDirty() || R.Data->GetStringField(TEXT("graph_change_id_before")) != Guid(C.Graph->GetChangeID());
    if (!GraphSame || !Unselected || !StableSelected || !UsersSame || !OtherSame)
    { return Fail(R, TEXT("niagara_stage_settings_preservation_failed"), TEXT("Graph, stage identities/order, unrequested fields, User defaults or other sources changed; no rollback/retry.")); }
    return true;
}
struct FExpectedSettings
{
    bool Enabled, Force, Threads; ENiagaraIterationSource Iteration; ENiagaraGpuDispatchType Dispatch;
    ENiagaraDirectDispatchElementType Element; ENiagaraSimStageExecuteBehavior Execute;
    FNiagaraVariableDataInterfaceBinding DI; FNiagaraParameterBindingWithValue Counts[3], Iterations;
    explicit FExpectedSettings(UNiagaraSimulationStageGeneric* S)
        : Enabled(S->bEnabled), Force(S->bGpuDispatchForceLinear), Threads(S->bOverrideGpuDispatchNumThreads),
          Iteration(S->IterationSource), Dispatch(S->DirectDispatchType), Element(S->DirectDispatchElementType), Execute(S->ExecuteBehavior),
          DI(S->DataInterface), Counts{S->ElementCountX, S->ElementCountY, S->ElementCountZ}, Iterations(S->NumIterations) {}
};
bool ExpectedSettings(FResult& R, FSettingsContext& S, const FSettingsPatch& O, FExpectedSettings& E)
{
    if (O.HasEnabled) { E.Enabled = O.Enabled; } if (O.HasIteration) { E.Iteration = O.Iteration; }
    if (O.HasDispatch) { E.Dispatch = O.Dispatch; } if (O.HasElement) { E.Element = O.Element; }
    if (O.HasExecute) { E.Execute = O.Execute; } if (O.HasForce) { E.Force = O.Force; } if (O.HasThreads) { E.Threads = O.Threads; }
    if (O.HasDI)
    { auto* Class = UNiagaraDataInterfaceGrid2DCollection::StaticClass();
        if (!Class || Path(Class) != TEXT("/Script/Niagara.NiagaraDataInterfaceGrid2DCollection") || !Class->HasAnyClassFlags(CLASS_Native) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        { return Fail(R, TEXT("niagara_stage_grid_class_unsupported"), TEXT("Exact native Grid2DCollection binding class unavailable; no write dispatched.")); }
        E.DI = FNiagaraVariableDataInterfaceBinding(FNiagaraVariable(FNiagaraTypeDefinition(Class), FName(*O.DIName)));
    }
    for (int32 N = 0; N < 3; ++N) { if (O.HasCount[N]) { E.Counts[N].SetDefaultParameter(FNiagaraTypeDefinition::GetIntDef(), O.Counts[N]); } }
    if (O.HasIterations) { E.Iterations.SetDefaultParameter(FNiagaraTypeDefinition::GetIntDef(), O.Iterations); }
    if (O.HasBinding)
    {
        auto& BindingValue = S.Stage->NumIterations;
        const bool UserAllowed = BindingValue.AllowUserParameters(), IntAllowed = BindingValue.GetAllowedTypeDefinitions().Contains(FNiagaraTypeDefinition::GetIntDef());
        auto Proof = MakeShared<FJsonObject>(); Proof->SetBoolField(TEXT("user_parameters_allowed"), UserAllowed); Proof->SetBoolField(TEXT("exact_int_type_allowed"), IntAllowed);
        Proof->SetStringField(TEXT("requested_name"), O.BindingName); R.Data->SetObjectField(TEXT("num_iterations_binding_validation"), Proof);
        if (!UserAllowed || !IntAllowed) { return Fail(R, TEXT("niagara_stage_user_int_binding_forbidden"), TEXT("The selected NumIterations binding must permit User parameters and the exact native int type.")); }
        TArray<FNiagaraVariable> Users; const auto& Store = S.Target.System->GetExposedParameters(); Store.GetUserParameters(Users);
        int32 Matches = 0; FNiagaraVariableBase Matched; FString Short = O.BindingName; Short.RemoveFromStart(TEXT("User."));
        for (const auto& Candidate : Users)
        { FString Name = Candidate.GetName().ToString(), CandidateShort = Name; CandidateShort.RemoveFromStart(TEXT("User."));
            if (Name.Equals(O.BindingName, ESearchCase::IgnoreCase) || CandidateShort.Equals(Short, ESearchCase::IgnoreCase))
            { ++Matches; Matched = Candidate; }
        }
        if (Matches != 1) { return Fail(R, TEXT("niagara_stage_user_int_binding_missing"), TEXT("Exactly one existing User parameter must match the iteration binding; no parameter is declared.")); }
        if (Matched.GetType() != FNiagaraTypeDefinition::GetIntDef()) { return Fail(R, TEXT("niagara_stage_user_int_binding_type_invalid"), TEXT("Existing iteration binding parameter must be exactly native int32.")); }
        FNiagaraVariableBase Canonical(Matched);
        if (!Store.RedirectUserVariable(Canonical) || Store.IndexOf(Canonical) == INDEX_NONE)
        { return Fail(R, TEXT("niagara_stage_user_int_binding_unresolved"), TEXT("Existing User redirect must resolve to native store identity.")); }
        Proof->SetObjectField(TEXT("existing_parameter"), Variable(Canonical)); Proof->SetBoolField(TEXT("existing_int_identity_verified"), true);
        // Setting a constant resets the alias; the requested qualified runtime
        // name is applied afterwards, exactly as in the original handler.
        E.Iterations.AliasedParameter = FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), FName(*O.BindingName));
        E.Iterations.ResolvedParameter = E.Iterations.AliasedParameter;
    }
    if (E.Iteration == ENiagaraIterationSource::DirectSet)
    {
        int32 Dimensions = E.Dispatch == ENiagaraGpuDispatchType::OneD ? 1 : E.Dispatch == ENiagaraGpuDispatchType::TwoD ? 2 : E.Dispatch == ENiagaraGpuDispatchType::ThreeD ? 3 : 0;
        for (int32 N = 0; N < Dimensions; ++N) { if (E.Counts[N].GetDefaultValue<int32>() <= 0)
            { return Fail(R, TEXT("niagara_stage_direct_dispatch_count_invalid"), TEXT("Every active DirectSet OneD/TwoD/ThreeD dimension requires a positive effective count.")); } }
    }
    return true;
}
bool SettingsMatch(FSettingsContext& S, FExpectedSettings& E)
{
    auto* Stage = S.Stage; bool Complete = Loaded(Stage) && Stage->GetClass() == UNiagaraSimulationStageGeneric::StaticClass();
    if (!Complete) { return false; }
    bool Same = Stage->bEnabled == E.Enabled && Stage->IterationSource == E.Iteration && Stage->DirectDispatchType == E.Dispatch &&
        Stage->DirectDispatchElementType == E.Element && Stage->ExecuteBehavior == E.Execute && Stage->bGpuDispatchForceLinear == E.Force &&
        Stage->bOverrideGpuDispatchNumThreads == E.Threads && Stage->DataInterface.BoundVariable == E.DI.BoundVariable;
    FNiagaraParameterBindingWithValue* Actual[] = {&Stage->ElementCountX, &Stage->ElementCountY, &Stage->ElementCountZ, &Stage->NumIterations};
    FNiagaraParameterBindingWithValue* Expected[] = {&E.Counts[0], &E.Counts[1], &E.Counts[2], &E.Iterations};
    for (int32 N = 0; N < 4; ++N) { auto A = Binding(*Actual[N], Complete), B = Binding(*Expected[N], Complete); Same &= Text(A) == Text(B); }
    return Complete && Same;
}
void FinishSettings(FResult& R, FSettingsContext& S, const FSettingsPatch& O, FExpectedSettings& E, bool Save, bool Compile)
{
    if(!IBCheck(TEXT("settings_finish_admission")))return;
    auto& C = S.Target; TSharedPtr<FJsonObject> Written;
    const bool Observed = ObserveSettings(R, S, &O, Written), Desired = Observed && SettingsMatch(S, E);
    R.Data->SetBoolField(TEXT("requested_settings_verified"), Desired);
    if (Observed && !Desired) { Fail(R, TEXT("niagara_stage_settings_postcondition_failed"), TEXT("Observed requested scalar/binding/default values differ; retain actual work and do not retry.")); }
    if (R.ErrorCode.IsEmpty() && R.bChangeStateKnown && Desired)
    {
        if(IBArmSettings(R,S,O,E,Save,Compile))return;
        if (Compile)
        { R.Stage = TEXT("request_compile"); R.Data->SetBoolField(TEXT("compile_requested"), true); R.Data->SetBoolField(TEXT("compile_attempted"), true);
            R.Data->SetNumberField(TEXT("explicit_compile_request_count"), 1); R.Data->SetBoolField(TEXT("compile_request_returned"), C.System->RequestCompile(false)); }
        if (Save)
        { auto State = CompileState(C.System); R.Data->SetObjectField(TEXT("save_compile_state"), State);
            if (!State->GetBoolField(TEXT("ready_to_save")))
            { R.Stage = TEXT("save_deferred"); R.Data->SetBoolField(TEXT("save_deferred"), true);
                Fail(R, TEXT("niagara_save_deferred_compile_pending"), TEXT("Changed settings remain. Observe readiness and explicitly save the exact asset; do not replay the setter.")); }
            else
            { R.Stage = TEXT("save"); R.Data->SetBoolField(TEXT("save_attempted"), true); R.Data->SetNumberField(TEXT("exact_save_attempt_count"), 1);
                const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(C.System, false); R.Data->SetBoolField(TEXT("saved"), Saved);
                if (!Saved) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_stage_settings_save_failed"), TEXT("Save returned false; RAM settings remain and disk outcome is uncertain.")); }
                else if (C.System->GetOutermost()->IsDirty()) { Fail(R, TEXT("niagara_stage_settings_save_postcondition_failed"), TEXT("Save returned true but target package remains dirty.")); }
            }
        }
    }
    // A later successful observation must not clear a previously uncertain
    // disk-save outcome. Keep it independent from current RAM state.
    const bool UnknownBeforeRead = !R.bChangeStateKnown; TSharedPtr<FJsonObject> Final;
    const bool FinalObserved = ObserveSettings(R, S, &O, Final), FinalDesired = FinalObserved && SettingsMatch(S, E);
    if (UnknownBeforeRead) { R.bChangeStateKnown = false; }
    const bool Stable = Written.IsValid() && Final.IsValid() && Text(Written.ToSharedRef()) == Text(Final.ToSharedRef());
    R.Data->SetBoolField(TEXT("settings_preserved_across_compile_save"), Stable);
    R.Data->SetBoolField(TEXT("requested_settings_verified"), FinalDesired); R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.System));
    if (FinalObserved && (!Stable || !FinalDesired)) { Fail(R, TEXT("niagara_stage_settings_compile_save_changed"), TEXT("Requested settings or identities changed across compile/save; actual changes retained.")); }
    R.Data->SetBoolField(TEXT("compile_and_save_suppressed_unknown_outcome"), !R.bChangeStateKnown &&
        !R.Data->GetBoolField(TEXT("compile_attempted")) && !R.Data->GetBoolField(TEXT("save_attempted")));
    R.bSuccess = R.bChangeStateKnown && FinalObserved && FinalDesired && Stable && R.ErrorCode.IsEmpty(); R.bPostconditionPassed = R.bSuccess;
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
}
}

Function::FResult Inspect(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName)
{
    FResult R; Initialize(R, SystemPath); if(!IBReadAdmission(R))return R; R.Data->SetBoolField(TEXT("read_only"), true); FContext C;
    if (!Prepare(R, C, SystemPath, EmitterIndexJson, EmitterName, false, false)) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(C.System); auto After = GraphState(C.Graph); R.Data->SetObjectField(TEXT("after"), After.Json);
    TArray<TSharedPtr<FJsonValue>> Rows; StageRows(R, C, Rows); const bool Other = PreserveOther(R, C);
    const bool Preserved = SameGraph(C.BeforeGraph, After) && VerifyPriorStages(R, C, nullptr, 0, Rows) && C.DirtyBefore == C.System->GetOutermost()->IsDirty();
    R.Data->SetBoolField(TEXT("state_preserved"), Preserved); R.Data->SetBoolField(TEXT("package_dirty_after"), C.System->GetOutermost()->IsDirty());
    if (!After.Complete) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_stage_graph_after_unobservable"), TEXT("Read-only graph observation incomplete.")); }
    else if (!Preserved) { R.bChanged = true; Fail(R, TEXT("niagara_stage_read_not_preserved"), TEXT("Native stage/graph/package state changed during read-only observation.")); }
    R.bSuccess = R.bChangeStateKnown && Other && Preserved && R.ErrorCode.IsEmpty(); R.bPostconditionPassed = R.bSuccess;
    if (R.bSuccess) { R.Stage = TEXT("complete"); } return R;
}
Function::FResult Add(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageName, const FString& DataInterfaceName, const FString& InsertIndexJson,
    bool bSkipIfDuplicate, bool bEnabled, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    FResult R; Initialize(R, SystemPath, bSave); if(!IBBind(R))return R; const FString Name = StageName.TrimStartAndEnd(), DIName = DataInterfaceName.TrimStartAndEnd(); FOptions O; int32 Insert;
    R.Data->SetBoolField(TEXT("request_compile"), bRequestCompile); R.Data->SetBoolField(TEXT("compile_requested"), bRequestCompile); R.Data->SetBoolField(TEXT("skip_if_duplicate"), bSkipIfDuplicate);
    R.Data->SetBoolField(TEXT("skipped_duplicate"), false); R.Data->SetStringField(TEXT("requested_options_json"), OptionsJson);
    if (Name.IsEmpty() || !Bounded(Name, 256) || DIName.IsEmpty() || !DIName.Contains(TEXT(".")) || !Bounded(DIName, 256) || !Index(InsertIndexJson, Insert))
    { Fail(R, TEXT("niagara_stage_name_or_index_invalid"), TEXT("Nonempty bounded stage/qualified DI name and null/nonnegative exact int32 insert index are required.")); return R; }
    if (!Options(R, OptionsJson, O)) { return R; }
    FContext C; if (!Prepare(R, C, SystemPath, EmitterIndexJson, EmitterName, true, bAllowSourceEdit)) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(C.System); R.Data->SetStringField(TEXT("data_interface_name"), DIName);
    for (int32 N = 0; N < C.BeforeStages.Num(); ++N)
    {
        auto* Existing = Cast<UNiagaraSimulationStageGeneric>(C.BeforeStages[N]);
        if (Existing && Existing->SimulationStageName.ToString().Equals(Name, ESearchCase::IgnoreCase))
        {
            if (!bSkipIfDuplicate) { Fail(R, TEXT("niagara_stage_duplicate"), TEXT("Simulation stage already exists; no graph reset or compile/save dispatched.")); return R; }
            R.Data->SetBoolField(TEXT("skipped_duplicate"), true); R.Data->SetStringField(TEXT("stage_name"), Existing->SimulationStageName.ToString()); R.Data->SetNumberField(TEXT("stage_index"), N);
            R.Data->SetStringField(TEXT("target_usage"), Enum(ENiagaraScriptUsage::ParticleSimulationStageScript)); R.Data->SetStringField(TEXT("target_usage_id"), Guid(Existing->Script->GetUsageId()));
            R.Data->SetObjectField(TEXT("stage_settings"), C.BeforeStageRows[N]->AsObject().ToSharedRef());
            R.Data->SetBoolField(TEXT("duplicate_settings_reconciled"), false); R.Data->SetBoolField(TEXT("duplicate_preserves_existing_settings"), true);
            auto After = GraphState(C.Graph); R.Data->SetObjectField(TEXT("after"), After.Json); TArray<TSharedPtr<FJsonValue>> Rows;
            StageRows(R, C, Rows); const bool Other = PreserveOther(R, C);
            const bool Preserved = SameGraph(C.BeforeGraph, After) && VerifyPriorStages(R, C, nullptr, 0, Rows) && C.DirtyBefore == C.System->GetOutermost()->IsDirty();
            R.Data->SetBoolField(TEXT("state_preserved"), Preserved); R.Data->SetBoolField(TEXT("package_dirty_after"), C.System->GetOutermost()->IsDirty());
            if (!After.Complete) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_stage_graph_after_unobservable"), TEXT("Duplicate observation incomplete; creation was not dispatched.")); }
            else if (!Preserved) { R.bChanged = true; Fail(R, TEXT("niagara_stage_duplicate_state_changed"), TEXT("Stage/graph/package state changed during duplicate skip.")); }
            R.bSuccess = R.bChangeStateKnown && Other && Preserved && R.ErrorCode.IsEmpty(); R.bPostconditionPassed = R.bSuccess; if (R.bSuccess) { R.Stage = TEXT("complete"); } return R;
        }
    }
    auto* GridClass = UNiagaraDataInterfaceGrid2DCollection::StaticClass();
    if (!GridClass || Path(GridClass) != TEXT("/Script/Niagara.NiagaraDataInterfaceGrid2DCollection") || !GridClass->HasAnyClassFlags(CLASS_Native) ||
        GridClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    { Fail(R, TEXT("niagara_stage_grid_class_unsupported"), TEXT("The exact native Grid2DCollection DI class must be available before allocation.")); return R; }
    const FGuid UsageId = FGuid::NewGuid();
    if (!UsageId.IsValid()) { Fail(R, TEXT("niagara_stage_usage_id_invalid"), TEXT("No valid fresh usage GUID; no stage allocation dispatched.")); return R; }
    for (UEdGraphNode* N : C.Graph->Nodes) { if (auto* Existing = Cast<UNiagaraNodeOutput>(N))
        { if (Existing->GetUsage() == ENiagaraScriptUsage::ParticleSimulationStageScript && Existing->GetUsageId() == UsageId)
            { Fail(R, TEXT("niagara_stage_usage_id_duplicate"), TEXT("Fresh usage GUID collision; no creation retry performed.")); return R; } } }
    const int32 NewIndex = Insert == INDEX_NONE ? C.BeforeStages.Num() : FMath::Clamp(Insert, 0, C.BeforeStages.Num());
    R.Data->SetNumberField(TEXT("requested_insert_index"), Insert); R.Data->SetNumberField(TEXT("resolved_insert_index"), NewIndex);
    R.Data->SetBoolField(TEXT("insert_index_clamped"), Insert != INDEX_NONE && Insert != NewIndex);
    R.Stage = TEXT("allocate_stage_script"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
    if(!IBCheck(TEXT("before_System_Modify")))return R; C.System->Modify(); if(!IBCheck(TEXT("after_System_Modify")))return R;
    C.Emitter->Modify(); if(!IBCheck(TEXT("after_Emitter_Modify")))return R;
    C.Graph->Modify(); if(!IBCheck(TEXT("after_Graph_Modify")))return R;
    R.Data->SetBoolField(TEXT("stage_allocation_attempted"), true);
    auto* Stage = NewObject<UNiagaraSimulationStageGeneric>(C.Emitter, NAME_None, RF_Transactional);
    if(ActiveIB)R.bChanged|=Stage!=nullptr; if(!IBCheck(TEXT("after_stage_allocation")))return R;
    if (!Loaded(Stage)) { Fail(R, TEXT("niagara_stage_allocation_failed"), TEXT("Stage allocation dispatched but no valid stage returned; do not retry.")); Finish(R, C, GraphState(C.Graph), false, false); return R; }
    TStrongObjectPtr<UNiagaraSimulationStageGeneric> KeepStage(Stage); if(!IBKeep(Stage))return R;
    R.Data->SetStringField(TEXT("allocated_stage_path"), Path(Stage)); R.bChanged = true;
    R.Data->SetBoolField(TEXT("script_allocation_attempted"), true);
    auto* CreatedScript = NewObject<UNiagaraScript>(Stage, MakeUniqueObjectName(Stage, UNiagaraScript::StaticClass(), FName(TEXT("SimulationStage"))), RF_Transactional);
    if(!IBCheck(TEXT("after_script_allocation")))return R;
    if(!IBStageScriptAssignment(Stage,CreatedScript))return R; Stage->Script=CreatedScript;
    if(Stage->Script&&!IBKeep(Stage->Script))return R;
    if (!Loaded(Stage->Script)) { Fail(R, TEXT("niagara_stage_script_allocation_failed"), TEXT("Stage remains allocated but its native script could not be observed; do not retry.")); Finish(R, C, GraphState(C.Graph), false, false); return R; }
    // Disabled initialization avoids the original SetEnabled(false) call before
    // Script/version attachment. The unconditional request is restored below.
    Stage->bEnabled = bEnabled; Stage->SimulationStageName = FName(*Name); Stage->IterationSource = ENiagaraIterationSource::DataInterface;
    Stage->ExecuteBehavior = O.Execute; Stage->DirectDispatchType = O.Dispatch; Stage->DirectDispatchElementType = O.Element;
    if (O.HasForce) { Stage->bGpuDispatchForceLinear = O.Force; } if (O.HasThreads) { Stage->bOverrideGpuDispatchNumThreads = O.Threads; }
    FNiagaraParameterBindingWithValue* Counts[] = {&Stage->ElementCountX, &Stage->ElementCountY, &Stage->ElementCountZ};
    for (int32 N = 0; N < 3; ++N) { if (O.HasCount[N]) { Counts[N]->SetDefaultParameter(FNiagaraTypeDefinition::GetIntDef(), O.Counts[N]); } }
    Stage->NumIterations.SetDefaultParameter(FNiagaraTypeDefinition::GetIntDef(), O.Iterations);
    Stage->DataInterface = FNiagaraVariableDataInterfaceBinding(FNiagaraVariable(FNiagaraTypeDefinition(GridClass), FName(*DIName)));
    Stage->Script->SetUsage(ENiagaraScriptUsage::ParticleSimulationStageScript); if(!IBCheck(TEXT("after_script_usage")))return R;
    Stage->Script->SetUsageId(UsageId); if(!IBCheck(TEXT("after_script_usage_id")))return R;
    if(!IBScriptSourceAssignment(Stage->Script,C.Source))return R;
    Stage->Script->SetLatestSource(C.Source); if(!IBCheck(TEXT("after_script_source")))return R;
    R.Stage = TEXT("create_stage_output_graph"); R.Data->SetBoolField(TEXT("stage_output_graph_creation_attempted"), true);
    UNiagaraNodeOutput* Output=nullptr; UNiagaraNodeInput* Input=nullptr;
    if(ActiveIB)
    {
        // FGraphNodeCreator requires Finalize even on unsafe early exit. Use the
        // same public placement steps explicitly so cancellation can stop safely.
        Output=Cast<UNiagaraNodeOutput>(IBAllocateNode(C.Graph,UNiagaraNodeOutput::StaticClass())); if(!Output)return R;
        Output->SetUsage(ENiagaraScriptUsage::ParticleSimulationStageScript); if(!IBCheck(TEXT("after_output_usage")))return R; Output->SetUsageId(UsageId);
        Output->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(),TEXT("Out"))); if(!IBFinalizeNode(Output))return R;
        Input=Cast<UNiagaraNodeInput>(IBAllocateNode(C.Graph,UNiagaraNodeInput::StaticClass())); if(!Input)return R;
        Input->Input=FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(),TEXT("InputMap")); Input->Usage=ENiagaraInputNodeUsage::Parameter;
        if(!IBFinalizeNode(Input))return R;
    }
    else
    {
        FGraphNodeCreator<UNiagaraNodeOutput> OutputCreator(*C.Graph); Output=OutputCreator.CreateNode(false);
        Output->SetUsage(ENiagaraScriptUsage::ParticleSimulationStageScript); Output->SetUsageId(UsageId);
        Output->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(),TEXT("Out"))); OutputCreator.Finalize();
        FGraphNodeCreator<UNiagaraNodeInput> InputCreator(*C.Graph); Input=InputCreator.CreateNode(false);
        Input->Input=FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(),TEXT("InputMap")); Input->Usage=ENiagaraInputNodeUsage::Parameter; InputCreator.Finalize();
    }
    R.Data->SetStringField(TEXT("new_output_node_path"), Path(Output)); R.Data->SetStringField(TEXT("new_output_node_guid"), Guid(Output->NodeGuid));
    R.Data->SetStringField(TEXT("new_input_node_path"), Path(Input)); R.Data->SetStringField(TEXT("new_input_node_guid"), Guid(Input->NodeGuid));
    UEdGraphPin* OutputPin = nullptr; UEdGraphPin* InputPin = nullptr;
    if (!MapPin(Output, EGPD_Input, OutputPin) || !MapPin(Input, EGPD_Output, InputPin) || !OutputPin->LinkedTo.IsEmpty() || !InputPin->LinkedTo.IsEmpty())
    { Fail(R, TEXT("niagara_stage_new_map_pin_invalid"), TEXT("New stage/output/input objects remain, but unique unlinked parameter-map pins were not observed.")); Finish(R, C, GraphState(C.Graph), false, false); return R; }
    R.Data->SetBoolField(TEXT("stage_map_connection_attempted"), true); OutputPin->MakeLinkTo(InputPin); if(!IBCheck(TEXT("after_stage_map_connection")))return R;
    R.Stage = TEXT("attach_stage"); R.Data->SetBoolField(TEXT("stage_attach_attempted"), true); if(!IBStageAttachVersion(Stage,C.Version))return R; C.Emitter->AddSimulationStage(Stage, C.Version); if(!IBCheck(TEXT("after_AddSimulationStage")))return R;
    if (NewIndex != C.BeforeStages.Num()) { R.Data->SetBoolField(TEXT("stage_move_attempted"), true); C.Emitter->MoveSimulationStageToIndex(Stage, NewIndex, C.Version); if(!IBCheck(TEXT("after_MoveSimulationStage")))return R; }
    R.Data->SetBoolField(TEXT("add_initial_disabled_SetEnabled_exact_order_preserved"), false);
    R.Data->SetStringField(TEXT("add_enabled_initialization_scope"), TEXT("Original enabled=true SetEnabled is a no-op. Disabled initialization remains a plain field assignment: the original pre-Script SetEnabled(false) would dereference null Script in UE5.8. Post-attachment unconditional RequestRecompile is restored; early setter scheduling/OnChanged parity is not claimed."));
    if(!IBCheck(TEXT("before_add_RequestRecompile")))return R;
    R.Data->SetBoolField(TEXT("compile_requested"),bRequestCompile);R.Data->SetBoolField(TEXT("compile_attempted"),true);
    R.Data->SetBoolField(TEXT("explicit_stage_request_recompile_invoked"),true);
    R.Data->SetNumberField(TEXT("stage_request_recompile_attempt_count"),R.Data->GetNumberField(TEXT("stage_request_recompile_attempt_count"))+1);
    Stage->RequestRecompile();
    R.Data->SetNumberField(TEXT("stage_request_recompile_return_count"),R.Data->GetNumberField(TEXT("stage_request_recompile_return_count"))+1);
    if(!IBCheck(TEXT("after_add_RequestRecompile")))return R;
    C.System->MarkPackageDirty(); if(!IBCheck(TEXT("after_MarkPackageDirty")))return R;
    R.Data->SetBoolField(TEXT("explicit_source_mark_not_synchronized_invoked"), false); R.Data->SetBoolField(TEXT("explicit_graph_notification_invoked"), false);
    auto After = GraphState(C.Graph); R.Data->SetObjectField(TEXT("after_addition"), After.Json); TArray<TSharedPtr<FJsonValue>> Rows;
    R.bChangeStateKnown = After.Complete; StageRows(R, C, Rows); if(!IBCheck(TEXT("after_added_stage_rows")))return R; R.Data->SetArrayField(TEXT("stages_after_addition"), Rows);
    VerifyPriorStages(R, C, Stage, NewIndex, Rows); VerifyGraphAddition(R, C, After, Output, Input);
    bool SettingsComplete = true; auto Settings = StageRow(Stage, NewIndex, SettingsComplete); if(!IBCheck(TEXT("after_added_stage_settings")))return R;
    const bool Identity = SettingsComplete && Stage->GetOuter() == C.Emitter && Stage->OuterEmitterVersion == C.Version && Stage->Script->GetLatestSource() == C.Source &&
        Stage->Script->GetUsageId() == UsageId && Output->GetUsageId() == UsageId && Output->GetUsage() == Stage->Script->GetUsage() &&
        Stage->SimulationStageName == FName(*Name) && Stage->bEnabled == bEnabled && Stage->IterationSource == ENiagaraIterationSource::DataInterface &&
        Stage->ExecuteBehavior == O.Execute && Stage->DirectDispatchType == O.Dispatch && Stage->DirectDispatchElementType == O.Element &&
        Stage->DataInterface.BoundVariable.GetName() == FName(*DIName) && Stage->DataInterface.BoundVariable.GetType().GetClass() == GridClass;
    bool Defaults = SettingsComplete && Stage->NumIterations.GetDefaultValueArray().Num() == sizeof(int32) && Stage->NumIterations.GetDefaultValue<int32>() == O.Iterations;
    for (int32 N = 0; N < 3; ++N) { Defaults &= Counts[N]->GetDefaultValueArray().Num() == sizeof(int32);
        if (Counts[N]->GetDefaultValueArray().Num() == sizeof(int32)) { Defaults &= Counts[N]->GetDefaultValue<int32>() == (O.HasCount[N] ? O.Counts[N] : 0); } }
    Defaults &= (!O.HasForce || Stage->bGpuDispatchForceLinear == O.Force) && (!O.HasThreads || Stage->bOverrideGpuDispatchNumThreads == O.Threads);
    R.Data->SetStringField(TEXT("stage_name"), Stage->SimulationStageName.ToString()); R.Data->SetNumberField(TEXT("stage_index"), NewIndex);
    R.Data->SetStringField(TEXT("target_usage"), Enum(ENiagaraScriptUsage::ParticleSimulationStageScript)); R.Data->SetStringField(TEXT("target_usage_id"), Guid(UsageId));
    R.Data->SetObjectField(TEXT("stage_settings"), Settings); R.Data->SetBoolField(TEXT("stage_identity_verified"), Identity); R.Data->SetBoolField(TEXT("requested_stage_defaults_verified"), Defaults);
    if (!SettingsComplete) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_stage_settings_unobservable"), TEXT("New stage settings could not be observed completely; compile/save suppressed.")); }
    else if (!Identity || !Defaults) { Fail(R, TEXT("niagara_stage_settings_postcondition_failed"), TEXT("Observed owner/version/script usage/binding/defaults differ; stage remains and no retry performed.")); }
    PreserveOther(R, C); Finish(R, C, After, bSave, bRequestCompile); return R;
}
Function::FResult InspectSettings(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageIndexJson, const FString& StageName)
{
    FResult R; InitializeSettings(R, SystemPath); if(!IBReadAdmission(R))return R; R.Data->SetBoolField(TEXT("read_only"), true); FSettingsContext S;
    if (!PrepareSettings(R, S, SystemPath, EmitterIndexJson, EmitterName, StageIndexJson, StageName, false, false)) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(S.Target.System); TSharedPtr<FJsonObject> Selected;
    const bool Observed = ObserveSettings(R, S, nullptr, Selected);
    const bool Preserved = Observed && S.Target.DirtyBefore == S.Target.System->GetOutermost()->IsDirty() &&
        R.Data->GetStringField(TEXT("graph_change_id_before")) == Guid(S.Target.Graph->GetChangeID());
    R.Data->SetBoolField(TEXT("state_preserved"), Preserved); R.Data->SetObjectField(TEXT("compile_state"), CompileState(S.Target.System));
    if (Observed && !Preserved) { Fail(R, TEXT("niagara_stage_settings_inspection_changed"), TEXT("Read-only settings observation changed graph/package state.")); }
    R.bSuccess = Preserved && R.bChangeStateKnown && R.ErrorCode.IsEmpty(); R.bPostconditionPassed = R.bSuccess;
    if (R.bSuccess) { R.Stage = TEXT("complete"); } return R;
}
Function::FResult SetSettings(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageIndexJson, const FString& StageName, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    FResult R; InitializeSettings(R, SystemPath, bSave); if(!IBBind(R))return R; R.Data->SetStringField(TEXT("requested_options_json"), OptionsJson);
    R.Data->SetBoolField(TEXT("requested_explicit_compile"), bRequestCompile); R.Data->SetBoolField(TEXT("compile_requested"), bRequestCompile); FSettingsPatch O;
    if (!SettingsOptions(R, OptionsJson, O)) { return R; } FSettingsContext S;
    if (!PrepareSettings(R, S, SystemPath, EmitterIndexJson, EmitterName, StageIndexJson, StageName, true, bAllowSourceEdit)) { return R; }
    TStrongObjectPtr<UNiagaraSystem> KeepSystem(S.Target.System); TStrongObjectPtr<UNiagaraSimulationStageGeneric> KeepStage(S.Stage);
    FExpectedSettings E(S.Stage); if (!ExpectedSettings(R, S, O, E)) { return R; }
    auto& C = S.Target; auto* Stage = S.Stage;
    R.Data->SetBoolField(TEXT("compile_requested"),bRequestCompile);
    R.Stage = TEXT("write_stage_settings"); R.bMutationAttempted = true; R.bChangeStateKnown = false; R.Data->SetBoolField(TEXT("settings_write_attempted"), true);
    if(!IBCheck(TEXT("before_settings_System_Modify")))return R; C.System->Modify(); if(!IBCheck(TEXT("after_settings_System_Modify")))return R;
    Stage->Modify(); if(!IBCheck(TEXT("after_settings_Stage_Modify")))return R;
    Stage->Script->Modify(); if(!IBCheck(TEXT("after_settings_Script_Modify")))return R;
    C.Source->Modify(); if(!IBCheck(TEXT("after_settings_Source_Modify")))return R;
    C.Graph->Modify(); if(!IBCheck(TEXT("after_settings_Graph_Modify")))return R;
    // Fully validated plain binding copies retain omitted fields and guarantee
    // constants are applied before an explicitly requested qualified alias.
    if (O.HasEnabled)
    {
        if(!IBCheck(TEXT("before_settings_SetEnabled")))return R;
        const bool RecompilePath=Stage->bEnabled!=E.Enabled;
        R.Data->SetNumberField(TEXT("set_enabled_call_count"),R.Data->GetNumberField(TEXT("set_enabled_call_count"))+1);
        if(RecompilePath)
        {
            R.Data->SetBoolField(TEXT("compile_attempted"),true);
            R.Data->SetNumberField(TEXT("set_enabled_recompile_path_count"),R.Data->GetNumberField(TEXT("set_enabled_recompile_path_count"))+1);
            R.Data->SetBoolField(TEXT("enabled_notification_attempted"),true);
        }
        Stage->SetEnabled(E.Enabled);
        R.Data->SetNumberField(TEXT("set_enabled_return_count"),R.Data->GetNumberField(TEXT("set_enabled_return_count"))+1);
        if(RecompilePath)
        {
            R.Data->SetBoolField(TEXT("enabled_notification_returned"),true);
            R.Data->SetNumberField(TEXT("set_enabled_on_changed_path_count"),R.Data->GetNumberField(TEXT("set_enabled_on_changed_path_count"))+1);
        }
        if(!IBCheck(TEXT("after_settings_SetEnabled")))return R;
    }
    if (O.HasIteration) { Stage->IterationSource = E.Iteration; }
    if (O.HasDI) { Stage->DataInterface = E.DI; } if (O.HasDispatch) { Stage->DirectDispatchType = E.Dispatch; }
    if (O.HasElement) { Stage->DirectDispatchElementType = E.Element; } if (O.HasExecute) { Stage->ExecuteBehavior = E.Execute; }
    if (O.HasForce) { Stage->bGpuDispatchForceLinear = E.Force; } if (O.HasThreads) { Stage->bOverrideGpuDispatchNumThreads = E.Threads; }
    FNiagaraParameterBindingWithValue* Counts[] = {&Stage->ElementCountX, &Stage->ElementCountY, &Stage->ElementCountZ};
    for (int32 N = 0; N < 3; ++N) { if (O.HasCount[N]) { *Counts[N] = E.Counts[N]; } }
    if (O.HasIterations || O.HasBinding) { Stage->NumIterations = E.Iterations; }
    R.Data->SetBoolField(TEXT("settings_write_returned"), true); R.Stage = TEXT("notify_stage_settings");
    if(!IBCheck(TEXT("before_settings_RequestRecompile")))return R;
    R.Data->SetBoolField(TEXT("compile_requested"),bRequestCompile);R.Data->SetBoolField(TEXT("compile_attempted"),true);
    R.Data->SetBoolField(TEXT("explicit_stage_request_recompile_invoked"),true);
    R.Data->SetBoolField(TEXT("source_notification_attempted"),true);
    const bool DisabledRecompilePath=!Stage->bEnabled;
    if(DisabledRecompilePath)R.Data->SetBoolField(TEXT("script_invalidation_attempted"),true);
    R.Data->SetNumberField(TEXT("stage_request_recompile_attempt_count"),R.Data->GetNumberField(TEXT("stage_request_recompile_attempt_count"))+1);
    Stage->RequestRecompile();
    R.Data->SetNumberField(TEXT("stage_request_recompile_return_count"),R.Data->GetNumberField(TEXT("stage_request_recompile_return_count"))+1);
    R.Data->SetBoolField(TEXT("source_notification_returned"),true);
    if(DisabledRecompilePath)R.Data->SetBoolField(TEXT("script_invalidation_returned"),true);
    if(!IBCheck(TEXT("after_settings_RequestRecompile")))return R;
    C.System->MarkPackageDirty(); if(!IBCheck(TEXT("after_settings_MarkPackageDirty")))return R; FinishSettings(R, S, O, E, bSave, bRequestCompile); return R;
}

// IB: concrete Stage add/settings continuation. Existing result/core/readback are shared.
namespace
{
using FIBJob=UCubelessNiagaraEmitterEventAuthoringJob;
namespace IBAsync=NiagaraEmitterEventAuthoringAsync;
bool IBGlobals(){return IsInGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
bool IBLease=false,IBStarting=false;
TSharedPtr<FIBStageJob> IBPending;
TArray<TSharedPtr<FIBStageJob>> IBUnsafe;
struct FIBIdentity
{
 TStrongObjectPtr<UObject> Hold{nullptr}; TWeakObjectPtr<UObject> Object,Outer; TWeakObjectPtr<UPackage> Package,Effective; TWeakObjectPtr<UClass> Class; FString ObjectPath;
 explicit FIBIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Package(O->GetOutermost()),Effective(O->GetPackage()),Class(O->GetClass()),ObjectPath(O->GetPathName()){}
};
struct FIBStageRole{TWeakObjectPtr<UNiagaraSimulationStageBase> Stage;TWeakObjectPtr<UNiagaraScript> Script;FGuid Version;bool NewStage=false,ScriptAssigned=false,VersionAssigned=false;};
struct FIBScriptRole{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;bool InitializingVersion=false;};
struct FIBSourceRole{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;};
struct FIBHandleRole{FGuid Id,Version;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;};
struct FIBNodeRole{TWeakObjectPtr<UEdGraphNode> Node;TArray<UEdGraphPin*> Pins;};
struct FIBPhysicalGraph{TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FIBNodeRole> Nodes;FGraphState Expected;};
enum class EIBPhase:uint8{Core,Compile,Wait,Save,Readback,Final};
struct FIBStageJob:TSharedFromThis<FIBStageJob>
{
 FResult R;FResult* ActiveResult=&R;TSharedPtr<FJsonObject> Params;bool Settings=false,Armed=false,Resuming=false,Frozen=false,AllowCoreEntry=false;
 bool Executing=false,Terminal=false,Quarantined=false,Cancelled=false,PersistentAttempted=false,JobAllocated=false;
 bool Save=false,Compile=false,Wait=false,TimedOut=false,Idle=true;double Timeout=10.,Poll=.1,Started=0.,Next=0.;int32 Iterations=0;
 EIBPhase Phase=EIBPhase::Core;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<FIBJob> Job;FIBJob* Key=nullptr;TStrongObjectPtr<FIBJob> JobHold{nullptr};FString JobPath,LastJson,CancelReason;FGuid Id=FGuid::NewGuid();int64 Revision=0;uint64 LastFrame=MAX_uint64;FTSTicker::FDelegateHandle Ticker;
 FContext Context;FGraphState AfterAddition;FSettingsContext SettingsContext;FSettingsPatch Patch;TUniquePtr<FExpectedSettings> ExpectedSettings;
 TArray<TUniquePtr<FIBIdentity>> Objects;TArray<FIBStageRole> StageRoles;TArray<FIBScriptRole> Scripts;TArray<FIBSourceRole> Sources;TArray<FIBHandleRole> Handles;TArray<FIBPhysicalGraph> Graphs;
 TWeakObjectPtr<UNiagaraScript> Spawn,Update,RecompileUpdate;TArray<TWeakObjectPtr<UNiagaraSimulationStageBase>> Stages;TArray<TSharedPtr<FJsonValue>> ExpectedStageRows;TSharedPtr<FJsonObject> WaitResult;TSharedRef<FJsonObject> NativeCounts=MakeShared<FJsonObject>();
 explicit FIBStageJob(bool IsSettings):Settings(IsSettings){Editor=GEditor;World=GEditor->GetEditorWorldContext().World();}
 const TCHAR* Operation()const{return Settings?TEXT("set_niagara_simulation_stage_settings"):TEXT("add_niagara_grid_simulation_stage");}
 FResult& Out(){return *ActiveResult;}
 bool Stop(const TCHAR* Code,const TCHAR* Message){Out().bSuccess=Out().bPostconditionPassed=false;Out().bChangeStateKnown=false;return Fail(Out(),Code,Message);}
 bool PublicationSafe()const
 {
  if(!IBGlobals()||Editor.Get()!=GEditor)return false;
  auto* W=GEditor->GetEditorWorldContext().World();
  return Loaded(W)&&!W->bIsTearingDown&&World.Get()==W&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext();
 }
 bool JobExact()const{return PublicationSafe()&&Job.Get()==Key&&Loaded(Key)&&Key->GetClass()==FIBJob::StaticClass()&&Key->GetOuter()==GetTransientPackage()&&Key->GetOutermost()==GetTransientPackage()&&Key->GetPackage()==GetTransientPackage()&&Key->HasAnyFlags(RF_Transient)&&Key->GetPathName()==JobPath;}
 bool LiveObject(const UObject* O)const{return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
 bool Keep(UObject* O)
 {
  if(!O)return true;if(!PublicationSafe()||!LiveObject(O))return Stop(TEXT("niagara_stage_async_object_unavailable"),TEXT("Expected live object at a safe captured Editor/world boundary."));
  TArray<UObject*> Chain;UObject* Owner=O;
  for(int32 Depth=0;Owner&&Depth<64;++Depth){if(!LiveObject(Owner))return Stop(TEXT("niagara_stage_async_owner_unavailable"),TEXT("Captured owner chain is not live."));Chain.Add(Owner);Owner=Owner->GetOuter();}
  if(Owner)return Stop(TEXT("niagara_stage_async_owner_bound"),TEXT("Owner chain exceeds the explicit 64-level bound."));
  for(int32 N=Chain.Num()-1;N>=0;--N){bool Exists=false;for(const auto& I:Objects)if(I->Object.Get()==Chain[N]){Exists=true;break;}if(Exists)continue;if(Objects.Num()>=262144)return Stop(TEXT("niagara_stage_async_identity_bound"),TEXT("Bounded authoring identity inventory exceeded."));Objects.Add(MakeUnique<FIBIdentity>(Chain[N]));}return true;
 }
 bool Check(const TCHAR* Label)
 {
  Out().Stage=Label;
  if(Cancelled||IBAsync::IsStopping())return Stop(TEXT("niagara_stage_async_cancelled"),TEXT("Owned continuation stopped; no new target SDK, save, observation or retry."));
  if(!PublicationSafe())return Stop(TEXT("niagara_stage_async_context_changed"),TEXT("Captured Editor/world changed or GT/PIE/GC/exit/save boundary is unsafe."));
  if(!Out().ErrorCode.IsEmpty())return false;
  for(const auto& I:Objects)
  {
   auto* O=I->Object.Get();auto* P=I->Package.Get();auto* E=I->Effective.Get();auto* Outer=I->Outer.Get();
   if(!LiveObject(O)||!LiveObject(P)||!LiveObject(E)||(Outer&&!LiveObject(Outer))||O->GetClass()!=I->Class.Get()||O->GetOuter()!=Outer||O->GetOutermost()!=P||O->GetPackage()!=E||O->GetPathName()!=I->ObjectPath)
    return Stop(TEXT("niagara_stage_async_identity_changed"),TEXT("Captured authoring class/outer/package/path changed; no replacement or replay."));
  }
  if(Context.System)
  {
   if(Context.System->GetSystemSpawnScript()!=Spawn.Get()||Context.System->GetSystemUpdateScript()!=Update.Get()||Context.System->GetEmitterHandles().Num()!=Handles.Num())return Stop(TEXT("niagara_stage_async_system_routing_changed"),TEXT("Captured system script or emitter routing changed."));
   for(int32 N=0;N<Handles.Num();++N){const auto& H=Context.System->GetEmitterHandles()[N];const auto& E=Handles[N];if(H.GetId()!=E.Id||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.Version||H.GetEmitterData()!=E.Data)return Stop(TEXT("niagara_stage_async_emitter_routing_changed"),TEXT("Captured emitter/version/data role changed."));if(E.Data&&E.Data->GraphSource!=E.Source.Get())return Stop(TEXT("niagara_stage_async_emitter_source_changed"),TEXT("Captured emitter GraphSource role changed."));}
  }
  if(!YHCurrent(Out(),Context))return false;
  if(Context.Data&&Context.Data->UpdateScriptProps.Script.Get()!=RecompileUpdate.Get())return Stop(TEXT("niagara_stage_async_update_script_changed"),TEXT("Selected emitter update Script role changed before public stage scheduling."));
  for(const auto& E:StageRoles){auto* S=E.Stage.Get();if(!LiveObject(S)||S->Script!=E.Script.Get()||S->OuterEmitterVersion!=E.Version)return Stop(TEXT("niagara_stage_async_stage_script_changed"),TEXT("Captured Stage Script/OuterEmitterVersion role changed before further Script SDK."));}
  for(auto& S:Scripts)
  {
   auto* O=S.Script.Get();if(!LiveObject(O)||O->GetLatestSource()!=S.Source.Get())return Stop(TEXT("niagara_stage_async_script_source_changed"),TEXT("Captured Script source role changed."));
   const auto* Data=static_cast<const UNiagaraScript*>(O)->GetLatestScriptData();
   if(S.InitializingVersion)
   {
    // SetSource calls CheckVersionDataAvailable: a fresh zero-version Script
    // receives exactly the exported SDK's stable initial version GUID.
    if(!Data||O->IsVersioningEnabled()||!Data->Version.VersionGuid.IsValid()||Data->Version.VersionGuid!=FNiagaraAssetVersion::CreateStableVersionGuid(O))return Stop(TEXT("niagara_stage_async_initial_version_invalid"),TEXT("New owned Script source initialization did not produce the exact SDK initial version."));
    S.Version=Data->Version.VersionGuid;S.InitializingVersion=false;
   }
   else if((Data?Data->Version.VersionGuid:FGuid())!=S.Version)return Stop(TEXT("niagara_stage_async_script_source_changed"),TEXT("Captured Script version role changed."));
  }
  for(const auto& S:Sources){auto* O=S.Source.Get();if(!LiveObject(O)||O->bIsCompilationCopy||O->bIsReleased||O->NodeGraph!=S.Graph.Get())return Stop(TEXT("niagara_stage_async_source_graph_changed"),TEXT("Captured source graph role changed or released."));}
  if(Frozen)
  {
   if(Context.Data->GetSimulationStages().Num()!=Stages.Num())return Stop(TEXT("niagara_stage_async_stage_membership_changed"),TEXT("Captured stage membership changed."));
   for(int32 N=0;N<Stages.Num();++N)if(Context.Data->GetSimulationStages()[N]!=Stages[N].Get())return Stop(TEXT("niagara_stage_async_stage_membership_changed"),TEXT("Captured stage order changed."));
   for(const auto& G:Graphs){auto* Graph=G.Graph.Get();if(!LiveObject(Graph)||Graph->Nodes.Num()!=G.Nodes.Num())return Stop(TEXT("niagara_stage_async_graph_membership_changed"),TEXT("Captured graph node membership changed."));for(int32 N=0;N<G.Nodes.Num();++N){auto* Node=G.Nodes[N].Node.Get();if(Graph->Nodes[N]!=Node||!LiveObject(Node)||Node->Pins!=G.Nodes[N].Pins)return Stop(TEXT("niagara_stage_async_physical_pins_changed"),TEXT("Captured physical node/pin array changed; stale pins are not dereferenced."));}}
  }
  return true;
 }
 bool CaptureStageRole(UNiagaraSimulationStageBase* S,bool NewStage)
 {
  if(!S||!LiveObject(S))return Stop(TEXT("niagara_stage_async_stage_unavailable"),TEXT("Expected a live nonnull Stage in the current captured membership."));
  if(!Keep(S))return false;for(const auto& E:StageRoles)if(E.Stage.Get()==S)return true;
  FIBStageRole Role;Role.Stage=S;Role.Script=S->Script;Role.Version=S->OuterEmitterVersion;Role.NewStage=NewStage;StageRoles.Add(Role);return true;
 }
 bool Script(UNiagaraScript* S)
 {
  if(!S)return true;if(!Keep(S))return false;for(const auto& E:Scripts)if(E.Script.Get()==S)return true;
  auto* B=S->GetLatestSource();if(B&&!Keep(B))return false;const auto* Data=static_cast<const UNiagaraScript*>(S)->GetLatestScriptData();Scripts.Add({S,B,Data?Data->Version.VersionGuid:FGuid(),false});return Source(Cast<UNiagaraScriptSource>(B));
 }
 bool Source(UNiagaraScriptSource* S)
 {
  if(!S)return true;if(!Keep(S)||!Keep(S->NodeGraph))return false;for(const auto& E:Sources)if(E.Source.Get()==S)return true;Sources.Add({S,S->NodeGraph});
  auto* G=S->NodeGraph.Get();if(!G||G->Nodes.Num()>MaxNodes)return Stop(TEXT("niagara_stage_async_graph_bound"),TEXT("Expected bounded live authoring graph."));
  for(const auto& NodePtr:G->Nodes){auto* N=NodePtr.Get();if(!N||!LiveObject(N))return Stop(TEXT("niagara_stage_async_node_unavailable"),TEXT("Expected a live nonnull graph node before reading pins."));if(!Keep(N)||N->Pins.Num()>MaxPins)return false;for(auto* P:N->Pins){if(!P||!Keep(P->DefaultObject))return false;if(auto* O=P->PinType.PinSubCategoryObject.Get()){if(!Keep(O))return false;}if(auto* O=P->PinType.PinValueType.TerminalSubCategoryObject.Get()){if(!Keep(O))return false;}}}
  return true;
 }
 bool Capture(FContext& C)
 {
  if(!Check(TEXT("capture_stage_roles")))return false;Context=C;
  if(!Keep(C.System)||!Keep(C.System->GetOutermost())||!Keep(C.Emitter)||!Source(C.Source))return false;
  Spawn=C.System->GetSystemSpawnScript();Update=C.System->GetSystemUpdateScript();RecompileUpdate=C.Data->UpdateScriptProps.Script;if(!Script(Spawn.Get())||!Script(Update.Get())||!Script(RecompileUpdate.Get()))return false;
  Handles.Reset();for(const auto& H:C.System->GetEmitterHandles()){auto* D=H.GetEmitterData();Handles.Add({H.GetId(),H.GetInstance().Version,H.GetInstance().Emitter,D,D?D->GraphSource.Get():nullptr});if(!Keep(H.GetInstance().Emitter)||!Source(D?Cast<UNiagaraScriptSource>(D->GraphSource):nullptr))return false;}
  for(auto* S:C.Data->GetSimulationStages())if(!CaptureStageRole(S,false)||!Script(S->Script))return false;
  return Check(TEXT("stage_roles_captured"));
 }
 bool Freeze()
 {
  if(!Check(TEXT("freeze_stage_roles")))return false;
  Stages.Reset();for(auto* S:Context.Data->GetSimulationStages()){if(!CaptureStageRole(S,false)||!Script(S->Script))return false;Stages.Add(S);}
  Graphs.Reset();for(const auto& S:Sources){auto* G=S.Graph.Get();FIBPhysicalGraph P;P.Graph=G;P.Expected=GraphState(G);if(!P.Expected.Complete)return Stop(TEXT("niagara_stage_async_graph_unobservable"),TEXT("Post-writer authoring graph is incomplete."));for(const auto& NodePtr:G->Nodes){auto* N=NodePtr.Get();if(!N||!LiveObject(N))return Stop(TEXT("niagara_stage_async_node_unavailable"),TEXT("Expected a live nonnull graph node before freezing physical pins."));if(!Keep(N))return false;FIBNodeRole E;E.Node=N;E.Pins=N->Pins;P.Nodes.Add(MoveTemp(E));for(auto* Pin:N->Pins)if(!Keep(Pin->DefaultObject))return false;}Graphs.Add(MoveTemp(P));}
  Frozen=true;return Check(TEXT("stage_roles_frozen"));
 }
 bool ObserveExpected()
 {
  if(!Check(TEXT("stage_phase_admission")))return false;
  for(const auto& G:Graphs){auto Now=GraphState(G.Graph.Get());if(!SameGraph(G.Expected,Now))return Stop(TEXT("niagara_stage_async_graph_changed"),TEXT("Authoring graph/pin/default/link state changed across compile/wait/save."));}
  TArray<TSharedPtr<FJsonValue>> Rows;if(!StageRows(Out(),Context,Rows)||!Check(TEXT("after_stage_phase_rows")))return false;
  if(Rows.Num()!=ExpectedStageRows.Num())return Stop(TEXT("niagara_stage_async_settings_changed"),TEXT("Stage row count changed."));
  for(int32 N=0;N<Rows.Num();++N)if(Text(Rows[N]->AsObject().ToSharedRef())!=Text(ExpectedStageRows[N]->AsObject().ToSharedRef()))return Stop(TEXT("niagara_stage_async_settings_changed"),TEXT("Stage settings/script/bindings changed across compile/wait/save."));
  return true;
 }
 bool SDK(const TCHAR* Label){if(!ObserveExpected())return false;Out().Stage=Label;Out().bChangeStateKnown=false;Out().Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);if(!Check(Label))return false;double Count=0;NativeCounts->TryGetNumberField(Label,Count);NativeCounts->SetNumberField(Label,Count+1);return true;}
 FString Encode(bool IsFinal)
 {
  R.bMutationAttempted|=JobAllocated;R.Data->SetStringField(TEXT("operation"),Operation());R.Data->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::Digits));R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);R.Data->SetBoolField(TEXT("native_job_terminal"),IsFinal);R.Data->SetBoolField(TEXT("operation_complete"),IsFinal);
  R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),PersistentAttempted);R.Data->SetBoolField(TEXT("transient_creation_only"),JobAllocated&&!PersistentAttempted);R.Data->SetBoolField(TEXT("transient_creation_attempted"),JobAllocated);R.Data->SetNumberField(TEXT("owned_job_allocation_count"),JobAllocated?1:0);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("all_SDK_calls_nonblocking_proven"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);
  R.Data->SetObjectField(TEXT("native_call_counts"),NativeCounts);R.Data->SetStringField(TEXT("native_call_count_scope"),TEXT("Owned Job allocation and new suffix SDK calls only; existing synchronous core and implicit Engine callbacks are not comprehensively counted."));
  auto Base=Cubeless::Operations::EncodeFunctionResult(R,Operation());TSharedPtr<FJsonObject> J;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Base),J)||!J)return Base;
  for(const TCHAR* K:{TEXT("compile_requested"),TEXT("compile_attempted"),TEXT("save_requested"),TEXT("save_attempted"),TEXT("saved"),TEXT("save_deferred")}){bool V=false;if(R.Data->TryGetBoolField(K,V))J->SetBoolField(K,V);}J->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());FString OutText;FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&OutText));return OutText;
 }
 void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
 void Quarantine(const TCHAR* Why)
 {
  if(Terminal||Quarantined)return;Unhook();Quarantined=true;ActiveResult=&R;Stop(TEXT("niagara_stage_async_unsafe"),Why);R.Data->SetBoolField(TEXT("native_writers_stopped"),true);R.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);LastJson=Encode(false);IBAsync::Publish(Key,LastJson,false,false);IBUnsafe.Add(AsShared());
 }
 void Finalize()
 {
  if(Terminal||Quarantined)return;if(!JobExact()){Quarantine(TEXT("Unsafe final publication; exact prefix/lease/holds retained without new target SDK."));return;}
  ActiveResult=&R;LastJson=Encode(true);Terminal=true;Unhook();IBAsync::Publish(Key,LastJson,true,true);Objects.Reset();JobHold.Reset();IBLease=false;if(IBPending.Get()==this)IBPending.Reset();
 }
 void Pending()
 {
  R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=false;LastJson=Encode(false);IBAsync::Publish(Key,LastJson,false,JobExact());
 }
 void Cancel(const FString& Why)
 {
  if(Terminal||Quarantined)return;Cancelled=true;CancelReason=Why.Left(8192);if(Executing)return;Unhook();ActiveResult=&R;Stop(TEXT("niagara_stage_async_cancelled"),TEXT("Owned operation cancelled without replay or rollback; actual prefix retained."));if(JobExact())Finalize();else Quarantine(TEXT("Cancellation cannot safely publish final; no new target observation."));
 }
 bool CompileStatus(bool& Outstanding,bool& Active)
 {
  if(!SDK(TEXT("HasOutstandingCompilationRequests")))return false;Outstanding=Context.System->HasOutstandingCompilationRequests(true);if(!Check(TEXT("after_HasOutstandingCompilationRequests")))return false;
  if(!SDK(TEXT("HasActiveCompilations")))return false;Active=Context.System->HasActiveCompilations();return Check(TEXT("after_HasActiveCompilations"));
 }
 bool Step()
 {
  if(Terminal||Quarantined)return false;if(!IBGlobals()){Quarantine(TEXT("Unsafe GT/GC/exit/save boundary stopped native writers."));return false;}
  if(Executing||LastFrame==GFrameCounter)return true;LastFrame=GFrameCounter;
  if(Cancelled||IBAsync::IsStopping()){Cancel(TEXT("Module shutdown/cancellation; no replay."));return false;}
  if(!JobExact()){Quarantine(TEXT("Captured Job/Editor/world changed."));return false;}if(Phase==EIBPhase::Wait&&FPlatformTime::Seconds()<Next)return true;
  TGuardValue<bool> InCall(Executing,true);TGuardValue<FIBStageJob*> Scope(ActiveIB,this);ActiveResult=&R;
  if(Phase==EIBPhase::Core)
  {
   auto S=[&](const TCHAR* K){return Params->GetStringField(K);};auto B=[&](const TCHAR* K){return Params->GetBoolField(K);};
   AllowCoreEntry=true;
   if(Settings)R=SetSettings(S(TEXT("system_path")),S(TEXT("emitter_index_json")),S(TEXT("emitter_name")),S(TEXT("stage_index_json")),S(TEXT("stage_name")),S(TEXT("options_json")),B(TEXT("allow_source_edit")),B(TEXT("save")),B(TEXT("request_compile")));
   else R=Add(S(TEXT("system_path")),S(TEXT("emitter_index_json")),S(TEXT("emitter_name")),S(TEXT("stage_name")),S(TEXT("data_interface_name")),S(TEXT("insert_index_json")),B(TEXT("skip_if_duplicate")),B(TEXT("enabled")),S(TEXT("options_json")),B(TEXT("allow_source_edit")),B(TEXT("save")),B(TEXT("request_compile")));
   AllowCoreEntry=false;ActiveResult=&R;PersistentAttempted|=R.bMutationAttempted;
   if(!PublicationSafe()){Quarantine(TEXT("Core SDK returned across unsafe lifetime; no further UObject observation."));return false;}
   if(!R.ErrorCode.IsEmpty()||!Armed){Phase=EIBPhase::Final;Finalize();return false;}
  }
  else
  {
   if(!ObserveExpected()){Finalize();return false;}
   if(Phase==EIBPhase::Compile)
   {
    if(Compile){if(!SDK(TEXT("RequestCompile"))){Finalize();return false;}R.Data->SetBoolField(TEXT("compile_requested"),true);R.Data->SetBoolField(TEXT("compile_attempted"),true);R.Data->SetNumberField(TEXT("explicit_compile_request_count"),1);const bool Accepted=Context.System->RequestCompile(false);R.Data->SetBoolField(TEXT("compile_request_returned"),Accepted);if(!Check(TEXT("after_RequestCompile"))){Finalize();return false;}}
    Started=Next=FPlatformTime::Seconds();Phase=Settings?EIBPhase::Save:EIBPhase::Wait;
   }
   else if(Phase==EIBPhase::Wait)
   {
    bool Outstanding=false,Active=false;if(!CompileStatus(Outstanding,Active)){Finalize();return false;}
    if(Wait&&(Outstanding||Active))
    {if(!SDK(TEXT("PollForCompilationComplete"))){Finalize();return false;}Context.System->PollForCompilationComplete();if(!Check(TEXT("after_PollForCompilationComplete"))){Finalize();return false;}if(!SDK(TEXT("ProcessAsyncTasks"))){Finalize();return false;}FAssetCompilingManager::Get().ProcessAsyncTasks(true);if(!Check(TEXT("after_ProcessAsyncTasks"))||!CompileStatus(Outstanding,Active)){Finalize();return false;}}
    Idle=!Outstanding&&!Active;TimedOut=Wait&&!Idle&&FPlatformTime::Seconds()>=Started+Timeout;
    WaitResult->SetBoolField(TEXT("wait_timed_out"),TimedOut);WaitResult->SetNumberField(TEXT("wait_iterations"),Iterations);WaitResult->SetNumberField(TEXT("wait_elapsed_seconds"),Wait?FPlatformTime::Seconds()-Started:0.);WaitResult->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);WaitResult->SetBoolField(TEXT("active_compilations_after"),Active);
    if(!Wait||Idle||TimedOut)Phase=EIBPhase::Save;else{++Iterations;Next=FPlatformTime::Seconds()+Poll;}
   }
   else if(Phase==EIBPhase::Save)
   {
    if(Save){if(!SDK(TEXT("SaveLoadedAsset"))){Finalize();return false;}R.Data->SetBoolField(TEXT("save_attempted"),true);R.Data->SetNumberField(TEXT("exact_save_attempt_count"),1);const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(Context.System,false);bool Earlier=false;R.Data->TryGetBoolField(TEXT("saved"),Earlier);R.Data->SetBoolField(TEXT("saved"),Earlier||Saved);if(!Check(TEXT("after_SaveLoadedAsset"))){Finalize();return false;}if(!Saved){Stop(TEXT("niagara_stage_save_failed"),TEXT("Original direct save returned false; RAM work/disk uncertainty retained."));Finalize();return false;}if(Context.System->GetOutermost()->IsDirty()){Stop(TEXT("niagara_stage_save_postcondition_failed"),TEXT("Save returned true but exact target remains dirty; no flag clear or retry."));Finalize();return false;}}
    Phase=EIBPhase::Readback;
   }
   else if(Phase==EIBPhase::Readback)
   {
    if(!ObserveExpected()){Finalize();return false;}R.bChangeStateKnown=true;Resuming=true;
    if(Settings)FinishSettings(R,SettingsContext,Patch,*ExpectedSettings,false,false);else Finish(R,Context,AfterAddition,false,false);
    Resuming=false;
    const TSharedPtr<FJsonObject>* Status=nullptr;double PollCount=0,PumpCount=0;NativeCounts->TryGetNumberField(TEXT("PollForCompilationComplete"),PollCount);NativeCounts->TryGetNumberField(TEXT("ProcessAsyncTasks"),PumpCount);
    if(R.Data->TryGetObjectField(TEXT("compile_state"),Status)&&Status&&Status->IsValid())(*Status)->SetBoolField(TEXT("poll_or_flush_invoked"),PollCount>0||PumpCount>0);
    R.Data->SetBoolField(TEXT("finite_GT_ticker_wait_implemented"),true);R.Data->SetBoolField(TEXT("successful_compilation_proven"),false);Phase=EIBPhase::Final;
   }
  }
  if(!PublicationSafe()){Quarantine(TEXT("SDK/phase returned across unsafe lifetime; actual prefix retained."));return false;}
  if(Cancelled||!R.ErrorCode.IsEmpty()){R.bSuccess=R.bPostconditionPassed=false;Finalize();return false;}
  if(Phase==EIBPhase::Final){Finalize();return false;}Pending();return true;
 }
};
bool IBBind(FResult& R)
{
 if(ActiveIB&&ActiveIB->AllowCoreEntry){ActiveIB->AllowCoreEntry=false;ActiveIB->ActiveResult=&R;return true;}
 if(IBLease||ActiveIB)return Fail(R,TEXT("niagara_stage_async_core_busy"),TEXT("The concrete Stage controller owns this core; no nested/direct writer or replay."));
 return true;
}
bool IBReadAdmission(FResult& R)
{if(IBLease||ActiveIB)return Fail(R,TEXT("niagara_stage_async_core_busy"),TEXT("The concrete Stage controller owns this core; inspection is not reentered across its SDK callbacks."));return true;}
bool IBCheck(const TCHAR* Label){return !ActiveIB||ActiveIB->Check(Label);}
bool IBKeep(UObject* O)
{if(!ActiveIB)return true;if(!ActiveIB->Keep(O))return false;if(auto* S=Cast<UNiagaraSimulationStageBase>(O))return ActiveIB->CaptureStageRole(S,true);return true;}
bool IBStageScriptAssignment(UNiagaraSimulationStageBase* S,UNiagaraScript* Script)
{
 if(!ActiveIB)return true;auto& W=*ActiveIB;if(!W.Check(TEXT("before_owned_stage_script_assignment")))return false;
 auto* E=W.StageRoles.FindByPredicate([&](const FIBStageRole& X){return X.Stage.Get()==S;});
 if(!E||!E->NewStage||E->ScriptAssigned||E->Script.IsValid()||S->Script||!Script||!W.LiveObject(Script)||Script->GetOuter()!=S||Script->GetClass()!=UNiagaraScript::StaticClass())return W.Stop(TEXT("niagara_stage_async_script_plan_invalid"),TEXT("Only one exact new Stage null-to-owned Script assignment is allowed."));
 if(!W.Script(Script))return false;E->Script=Script;E->ScriptAssigned=true;return true;
}
bool IBStageAttachVersion(UNiagaraSimulationStageBase* S,const FGuid& Version)
{
 if(!ActiveIB)return true;auto& W=*ActiveIB;if(!W.Check(TEXT("before_owned_stage_attach")))return false;
 auto* E=W.StageRoles.FindByPredicate([&](const FIBStageRole& X){return X.Stage.Get()==S;});
 if(!E||!E->NewStage||E->VersionAssigned||Version!=W.Context.Version)return W.Stop(TEXT("niagara_stage_async_version_plan_invalid"),TEXT("Only the captured new Stage attachment to the selected emitter version is planned."));
 E->Version=Version;E->VersionAssigned=true;return true;
}
bool IBScriptSourceAssignment(UNiagaraScript* S,UNiagaraScriptSourceBase* Source)
{
 if(!ActiveIB)return true;auto& W=*ActiveIB;if(!W.Check(TEXT("before_owned_script_source_assignment")))return false;
 const auto* Stage=W.StageRoles.FindByPredicate([&](const FIBStageRole& X){return X.Script.Get()==S&&X.NewStage&&X.ScriptAssigned;});
 auto* Role=W.Scripts.FindByPredicate([&](const FIBScriptRole& X){return X.Script.Get()==S;});
 if(!Stage||!Role||Role->Source.IsValid()||Source!=W.Context.Source)return W.Stop(TEXT("niagara_stage_async_source_plan_invalid"),TEXT("Only the new owned Script null-to-selected Source assignment is planned."));
 Role->InitializingVersion=static_cast<const UNiagaraScript*>(S)->GetLatestScriptData()==nullptr;Role->Source=Source;return true;
}
bool IBCapture(FResult&,FContext& C){return !ActiveIB||ActiveIB->Capture(C);}
FORCENOINLINE void IBAddGraphNode(UEdGraph& G,UEdGraphNode& N){G.AddNode(&N,false,false);}
UEdGraphNode* IBAllocateNode(UNiagaraGraph* G,UClass* Class)
{
 if(!IBCheck(TEXT("before_NewObject_Node")))return nullptr;auto* N=NewObject<UEdGraphNode>(G,Class,NAME_None,RF_Transactional);if(!IBCheck(TEXT("after_NewObject_Node")))return nullptr;if(!N){if(ActiveIB)ActiveIB->Stop(TEXT("niagara_stage_async_node_allocation_failed"),TEXT("Graph node allocation returned null; no placement or replay."));return nullptr;}if(!IBKeep(N))return nullptr;
 if(G->HasAnyFlags(RF_Transient))N->SetFlags(RF_Transient);
 IBAddGraphNode(*G,*N);if(!IBCheck(TEXT("after_Graph_AddNode")))return nullptr;N->CreateNewGuid();return N;
}
bool IBFinalizeNode(UEdGraphNode* N)
{
 if(!IBCheck(TEXT("before_PostPlacedNewNode")))return false;N->PostPlacedNewNode();if(!IBCheck(TEXT("after_PostPlacedNewNode")))return false;
 if(N->Pins.IsEmpty()){N->AllocateDefaultPins();if(!IBCheck(TEXT("after_AllocateDefaultPins")))return false;}return true;
}
bool IBArmAdd(FResult& R,FContext& C,const FGraphState& G,bool Save,bool Compile)
{
 if(!ActiveIB||ActiveIB->Resuming||!R.ErrorCode.IsEmpty()||!R.bChangeStateKnown)return false;
 auto& W=*ActiveIB;W.Context=C;W.AfterAddition=G;W.Save=Save;W.Compile=Compile;W.Wait=false;TSharedPtr<FJsonObject> O;
 FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(W.Params->GetStringField(TEXT("options_json"))),O);if(O)O->TryGetBoolField(TEXT("wait_for_completion"),W.Wait);W.Wait&=Compile;
 W.Params->TryGetNumberField(TEXT("timeout_seconds"),W.Timeout);W.Params->TryGetNumberField(TEXT("poll_interval_seconds"),W.Poll);W.Timeout=FMath::Clamp(W.Timeout,0.,60.);W.Poll=FMath::Clamp(W.Poll,.02,1.);
 W.WaitResult=MakeShared<FJsonObject>();W.WaitResult->SetBoolField(TEXT("wait_for_completion"),W.Wait);W.WaitResult->SetNumberField(TEXT("timeout_seconds"),W.Timeout);W.WaitResult->SetNumberField(TEXT("poll_interval_seconds"),W.Poll);W.WaitResult->SetBoolField(TEXT("wait_timed_out"),false);W.WaitResult->SetNumberField(TEXT("wait_iterations"),0);W.WaitResult->SetNumberField(TEXT("wait_elapsed_seconds"),0.);R.Data->SetObjectField(TEXT("compile_wait"),W.WaitResult);
 StageRows(R,C,W.ExpectedStageRows);if(!IBCheck(TEXT("after_arm_stage_rows"))||!W.Freeze())return true;W.Armed=true;W.Phase=EIBPhase::Compile;return true;
}
bool IBArmSettings(FResult& R,FSettingsContext& S,const FSettingsPatch& O,const FExpectedSettings& E,bool Save,bool Compile)
{
 if(!ActiveIB||ActiveIB->Resuming)return false;auto& W=*ActiveIB;W.Context=S.Target;W.SettingsContext=S;W.Patch=O;W.ExpectedSettings=MakeUnique<FExpectedSettings>(E);W.Save=Save;W.Compile=Compile;
 StageRows(R,S.Target,W.ExpectedStageRows);if(!IBCheck(TEXT("after_arm_settings_rows"))||!W.Freeze())return true;W.Armed=true;W.Phase=EIBPhase::Compile;return true;
}

bool RawDepth(const FString& S)
{
    int32 Depth=0;bool String=false,Escape=false;for(TCHAR C:S){if(C==0){return false;}if(String){if(Escape){Escape=false;}else if(C=='\\'){Escape=true;}else if(C=='"'){String=false;}continue;}if(C=='"'){String=true;}else if(C=='{'||C=='['){if(++Depth>64){return false;}}else if(C=='}'||C==']'){if(--Depth<0){return false;}}}return !String&&Depth==0;
}

bool FiniteValues(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Slots)
{
    if(!V.IsValid()||Depth>64||++Slots>262144){return false;}if(V->Type==EJson::Number){double X=0;return V->TryGetNumber(X)&&FMath::IsFinite(X);}if(V->Type==EJson::String){for(TCHAR C:V->AsString()){if(C==0){return false;}}return true;}
    if(V->Type==EJson::Array){for(const auto& I:V->AsArray()){if(!FiniteValues(I,Depth+1,Slots)){return false;}}}
    else if(V->Type==EJson::Object){for(const auto& P:V->AsObject()->Values){if(!FiniteValues(P.Value,Depth+1,Slots)){return false;}}}return true;
}

struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>{static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}};
bool ParseParams(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>1024*1024||!RawDepth(Text)){return false;}
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));
    TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>262144){return false;}
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        {const FString Key(Reader->GetIdentifier());for(TCHAR C:Key){if(C==0)return false;}if(Keys.Last().Contains(Key)){return false;}Keys.Last().Add(Key);}
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        {Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>65){return false;}}
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        {if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()||Values[0]->Type!=EJson::Object){return false;}
    int32 Visits=0;if(!FiniteValues(Values[0],0,Visits)){return false;}Out=Values[0]->AsObject();return Out.IsValid();
}

bool IBParams(FIBStageJob& W,const FString& Raw)
{
 if(!ParseParams(Raw,W.Params))return W.Stop(TEXT("niagara_stage_async_params_invalid"),TEXT("Finite bounded exact normalized ParamsJson object is required."));
 const TSet<FString> Strings=W.Settings?TSet<FString>{TEXT("system_path"),TEXT("emitter_index_json"),TEXT("emitter_name"),TEXT("stage_index_json"),TEXT("stage_name"),TEXT("options_json")}:TSet<FString>{TEXT("system_path"),TEXT("emitter_index_json"),TEXT("emitter_name"),TEXT("stage_name"),TEXT("data_interface_name"),TEXT("insert_index_json"),TEXT("options_json")};
 const TSet<FString> Bools=W.Settings?TSet<FString>{TEXT("allow_source_edit"),TEXT("save"),TEXT("request_compile")}:TSet<FString>{TEXT("skip_if_duplicate"),TEXT("enabled"),TEXT("allow_source_edit"),TEXT("save"),TEXT("request_compile")};
 for(const auto& K:Strings){FString V;if(!W.Params->TryGetStringField(K,V))return W.Stop(TEXT("niagara_stage_async_string_required"),TEXT("Normalized selector/options keys must be JSON strings."));}
 {FString Expanded;const FString RawPath=W.Params->GetStringField(TEXT("system_path"));if(!ObjectPath(RawPath,Expanded)||RawPath.Len()>=NAME_SIZE||Expanded.Len()>=NAME_SIZE)return W.Stop(TEXT("niagara_stage_async_path_invalid"),TEXT("Raw and expanded object paths must fit NAME_SIZE before any target lookup."));}
 for(const auto& K:Bools){bool V=false;if(!W.Params->TryGetBoolField(K,V))return W.Stop(TEXT("niagara_stage_async_bool_required"),TEXT("Normalized intent keys must be JSON booleans."));}
 for(const auto& P:W.Params->Values){const FString Key(P.Key.ToView());if(!Strings.Contains(Key)&&!Bools.Contains(Key)&&(W.Settings||(Key!=TEXT("timeout_seconds")&&Key!=TEXT("poll_interval_seconds"))))return W.Stop(TEXT("niagara_stage_async_unknown_field"),TEXT("Only the exact existing normalized Stage keys are accepted."));}
 if(!W.Settings){double T=0,P=0;if(!W.Params->TryGetNumberField(TEXT("timeout_seconds"),T)||!W.Params->TryGetNumberField(TEXT("poll_interval_seconds"),P)||!FMath::IsFinite(T)||!FMath::IsFinite(P))return W.Stop(TEXT("niagara_stage_async_timing_invalid"),TEXT("Original finite timeout/poll values are required for add only."));}
 return true;
}
}
bool HasSimulationStageContinuationLease(){return IBLease||IBStarting;}
UCubelessNiagaraEmitterEventAuthoringJob* StartStageContinuation(const FString& Raw,bool Settings)
{
 if(!IBGlobals()||IBAsync::IsStopping()||IBStarting||IBLease)return nullptr;
 auto W=MakeShared<FIBStageJob>(Settings);if(!W->PublicationSafe())return nullptr;
 TGuardValue<bool> StartingScope(IBStarting,true);IBLease=true;IBPending=W;
 const bool Parsed=IBParams(*W,Raw);W->R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);W->R.Data->SetBoolField(TEXT("owned_job_allocation_attempted"),true);
 FIBJob* Created=nullptr;W->NativeCounts->SetNumberField(TEXT("NewObject_StageJob"),1);{TGuardValue<bool> InCall(W->Executing,true);Created=NewObject<FIBJob>(GetTransientPackage(),NAME_None,RF_Transient);W->Key=Created;W->JobAllocated=true;}
 if(!W->PublicationSafe()){W->Quarantine(TEXT("Job allocation returned across unsafe Editor lifetime; no target dispatch or UObject publication."));return Created;}
 if(!Loaded(Created)||Created->GetClass()!=FIBJob::StaticClass()){W->Quarantine(TEXT("Exact owned Job allocation could not be verified."));return Created;}
 W->Job=Created;W->JobHold=TStrongObjectPtr<FIBJob>(Created);W->JobPath=Created->GetPathName();IBAsync::RegisterJob(Created,Settings?IBAsync::EJobKind::SimulationStageSettings:IBAsync::EJobKind::SimulationStageAdd,W->Id);
 if(!Parsed||W->Cancelled||IBAsync::IsStopping())
 {if(W->Cancelled||IBAsync::IsStopping())W->Stop(TEXT("niagara_stage_async_cancelled"),TEXT("Cancellation occurred during owned Job allocation; target dispatch was not started."));W->R.bChangeStateKnown=true;W->Finalize();return Created;}
 W->Pending();W->Step();if(!W->Terminal&&!W->Quarantined){TWeakPtr<FIBStageJob> Weak=W;W->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float){auto S=Weak.Pin();return S.IsValid()&&S->Step();}));}return Created;
}
UCubelessNiagaraEmitterEventAuthoringJob* StartSimulationStageAddContinuation(const FString& Raw){return StartStageContinuation(Raw,false);}
UCubelessNiagaraEmitterEventAuthoringJob* StartSimulationStageSettingsContinuation(const FString& Raw){return StartStageContinuation(Raw,true);}
bool CancelSimulationStageContinuation(UCubelessNiagaraEmitterEventAuthoringJob* Job,const FString& Reason)
{if(!IsInGameThread()||!IBPending.IsValid()||IBPending->Key!=Job)return false;auto W=IBPending;W->Cancel(Reason);return true;}
void ShutdownSimulationStageContinuation(const FString& Reason)
{if(!IsInGameThread())return;if(auto W=IBPending){W->Unhook();W->Cancel(Reason);}}

}
