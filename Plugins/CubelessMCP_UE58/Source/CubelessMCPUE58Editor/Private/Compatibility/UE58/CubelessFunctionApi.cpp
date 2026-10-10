// UE56 resolve/function-parameter/validated-call contracts via public UE58 APIs.
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Function
{
namespace
{
constexpr int32 MaxGraphs = 256, MaxNodes = 2048, MaxParameters = 128;
bool Fail(FResult& R, const TCHAR* Code, const FString& Message) { R.ErrorCode = Code; R.Error = Message; return false; }
FString Text(const TSharedRef<FJsonObject>& J) { FString Out; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out)); return Out; }
void Merge(FJsonObject& Out, const FJsonObject& From) { for (const auto& Pair : From.Values) { Out.SetField(FString(Pair.Key.ToView()), Pair.Value); } }
FString GraphType(UBlueprint* BP, UEdGraph* G)
{
    if (BP->FunctionGraphs.Contains(G)) { return TEXT("function"); }
    if (BP->MacroGraphs.Contains(G)) { return TEXT("macro"); }
    if (BP->UbergraphPages.Contains(G) || BP->EventGraphs.Contains(G)) { return TEXT("event"); }
    if (BP->DelegateSignatureGraphs.Contains(G)) { return TEXT("delegate"); } return TEXT("unknown");
}
FString NormalizeType(FString T)
{
    T = T.ToLower().Replace(TEXT(" "), TEXT("")).Replace(TEXT("-"), TEXT("_"));
    if (T == TEXT("eventgraph") || T == TEXT("event_graph") || T == TEXT("ubergraph") || T == TEXT("uber_graph")) { return TEXT("event"); }
    if (T == TEXT("functiongraph") || T == TEXT("function_graph")) { return TEXT("function"); }
    if (T == TEXT("macrograph") || T == TEXT("macro_graph")) { return TEXT("macro"); } return T;
}
TSharedRef<FJsonObject> DescribeGraph(UBlueprint* BP, UEdGraph* G)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("graph_id"), G->GraphGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("graph_name"), G->GetName()); J->SetStringField(TEXT("graph_type"), GraphType(BP, G)); J->SetStringField(TEXT("object_path"), G->GetPathName());
    J->SetStringField(TEXT("schema"), G->GetSchema() ? G->GetSchema()->GetClass()->GetName() : TEXT("")); J->SetNumberField(TEXT("node_count"), G->Nodes.Num()); return J;
}
TSharedRef<FJsonObject> DescribeNode(UEdGraphNode* Node)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits)); J->SetStringField(TEXT("object_path"), Node->GetPathName());
    J->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); J->SetStringField(TEXT("node_name"), Node->GetName());
    J->SetNumberField(TEXT("x"), Node->NodePosX); J->SetNumberField(TEXT("y"), Node->NodePosY);
    TArray<TSharedPtr<FJsonValue>> Pins;
    for (UEdGraphPin* P : Node->Pins)
    {
        if (!P) { continue; } auto Q = MakeShared<FJsonObject>(); Q->SetStringField(TEXT("name"), P->PinName.ToString());
        Q->SetStringField(TEXT("pin_id"), P->PinId.ToString(EGuidFormats::Digits)); Q->SetStringField(TEXT("direction"), P->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        Q->SetObjectField(TEXT("pin_type"), Pin::Describe(P->PinType)); Q->SetBoolField(TEXT("orphaned"), P->bOrphanedPin);
        Q->SetStringField(TEXT("default_value"), P->DefaultValue); Q->SetStringField(TEXT("default_object"), P->DefaultObject ? P->DefaultObject->GetPathName() : TEXT(""));
        Q->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString()); TArray<TSharedPtr<FJsonValue>> Links;
        for (UEdGraphPin* Other : P->LinkedTo)
        {
            if (!Other || !IsValid(Other->GetOwningNodeUnchecked())) { continue; } auto Link = MakeShared<FJsonObject>();
            Link->SetStringField(TEXT("node_id"), Other->GetOwningNode()->NodeGuid.ToString(EGuidFormats::Digits)); Link->SetStringField(TEXT("node_path"), Other->GetOwningNode()->GetPathName());
            Link->SetStringField(TEXT("pin_name"), Other->PinName.ToString()); Links.Add(MakeShared<FJsonValueObject>(Link));
        }
        Q->SetArrayField(TEXT("linked_to"), Links); Pins.Add(MakeShared<FJsonValueObject>(Q));
    }
    J->SetArrayField(TEXT("pins"), Pins);
    if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
    { const UFunction* F = Call->GetTargetFunction(); J->SetStringField(TEXT("function_path"), F ? F->GetPathName() : TEXT("")); }
    return J;
}
bool Capture(UBlueprint* BP, UEdGraph* G, TSharedRef<FJsonObject>& Out, FResult& R)
{
    Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("blueprint"), BP->GetPathName()); Out->SetBoolField(TEXT("dirty"), BP->GetOutermost()->IsDirty());
    Out->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(BP->Status.GetValue()));
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    if (Graphs.Num() > MaxGraphs) { return Fail(R, TEXT("function_graph_budget_exceeded"), TEXT("Blueprint graph observation exceeds 256 graphs.")); }
    TArray<TSharedPtr<FJsonValue>> GraphRows; TSet<FGuid> GraphGuids;
    for (UEdGraph* Candidate : Graphs)
    {
        if (!IsValid(Candidate) || !Candidate->GraphGuid.IsValid() || GraphGuids.Contains(Candidate->GraphGuid))
        { return Fail(R, TEXT("function_graph_identity_invalid"), TEXT("Graph identities are invalid or duplicated; no identities are repaired.")); }
        GraphGuids.Add(Candidate->GraphGuid); GraphRows.Add(MakeShared<FJsonValueObject>(DescribeGraph(BP, Candidate)));
    }
    Out->SetArrayField(TEXT("graphs"), GraphRows);
    if (G)
    {
        if (!Graphs.Contains(G) || G->Nodes.Num() > MaxNodes) { return Fail(R, TEXT("function_graph_state_invalid"), TEXT("Target graph is no longer owned by the Blueprint or exceeds 2048 nodes.")); }
        TArray<TSharedPtr<FJsonValue>> Nodes; TSet<FGuid> NodeGuids;
        for (UEdGraphNode* N : G->Nodes)
        {
            if (!IsValid(N) || !N->NodeGuid.IsValid() || NodeGuids.Contains(N->NodeGuid) || N->Pins.Num() > 512)
            { return Fail(R, TEXT("function_node_identity_invalid"), TEXT("A graph node identity/pin budget is invalid; observation is incomplete.")); }
            NodeGuids.Add(N->NodeGuid); Nodes.Add(MakeShared<FJsonValueObject>(DescribeNode(N)));
        }
        Out->SetObjectField(TEXT("graph"), DescribeGraph(BP, G)); Out->SetArrayField(TEXT("nodes"), Nodes);
    }
    return true;
}
void After(UBlueprint* BP, UEdGraph* G, const TSharedRef<FJsonObject>& Before, FResult& R)
{
    TSharedRef<FJsonObject> Observed = MakeShared<FJsonObject>();
    if (!Capture(BP, G, Observed, R)) { R.bChangeStateKnown = false; R.Stage = TEXT("verification"); R.Data->SetObjectField(TEXT("incomplete_after"), Observed); return; }
    R.Data->SetObjectField(TEXT("after"), Observed); R.Data->SetBoolField(TEXT("dirty_before"), Before->GetBoolField(TEXT("dirty")));
    R.Data->SetBoolField(TEXT("dirty_after"), BP->GetOutermost()->IsDirty()); R.bChanged = Text(Before) != Text(Observed); R.bChangeStateKnown = true;
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("owned graph identities, target graph node/pin/default/link snapshots, Blueprint dirty/status; arbitrary callback state is not fully observed"));
}
UBlueprint* Load(const FString& Path, FResult& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("function_editor_state_invalid"), TEXT("Function tools require the editor game thread outside PIE/SIE.")); return nullptr; }
    if (!Pin::SafeName(Path) || !Path.StartsWith(TEXT("/Game/"))) { Fail(R, TEXT("function_blueprint_path_invalid"), TEXT("Use a bounded exact /Game Blueprint asset/package path.")); return nullptr; }
    const FString AssetPath = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
    if (!Pin::SafeName(AssetPath)) { Fail(R, TEXT("function_blueprint_path_invalid"), TEXT("Completed object path exceeds the safe name bound.")); return nullptr; }
    const bool WasLoaded = FindObject<UBlueprint>(nullptr, *AssetPath) != nullptr; R.Data->SetBoolField(TEXT("loaded_during_operation"), !WasLoaded);
    R.Data->SetBoolField(TEXT("implicit_postload_work_possible"), !WasLoaded); R.Data->SetStringField(TEXT("load_scope"), TEXT("Blueprint dirty/status baseline starts after load; implicit PostLoad effects are unmeasured"));
    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!IsValid(BP)) { R.bChangeStateKnown = WasLoaded; Fail(R, TEXT("function_blueprint_not_found"), TEXT("Blueprint asset load failed; inspect load effects before retry.")); return nullptr; } return BP;
}
bool MutationScope(UBlueprint* BP, FResult& R)
{
    if (BP->BlueprintType != BPTYPE_Normal || !BP->ParentClass || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->Status == BS_BeingCreated)
    { return Fail(R, TEXT("function_blueprint_state_unsupported"), TEXT("Requires a normal idle Blueprint with no queued/suppressed structural change.")); }
    auto Parent = InspectBlueprintParent(BP->ParentClass->GetPathName()); R.Data->SetObjectField(TEXT("parent_validation"), Parent);
    if (!Parent->GetBoolField(TEXT("success"))) { return Fail(R, TEXT("function_parent_unsupported"), TEXT("Blueprint runtime parent checks failed.")); }
    for (UClass* C = BP->ParentClass; C; C = C->GetSuperClass())
    { const FString P = C->GetOutermost()->GetName(); if (C->HasAnyClassFlags(CLASS_Native) && P != TEXT("/Script/Engine") && P != TEXT("/Script/CoreUObject")) { return Fail(R, TEXT("function_parent_unsupported"), TEXT("Unaudited native parent module.")); } }
    TArray<FString> External;
    for (TObjectIterator<UBlueprint> It; It; ++It)
    { if (*It != BP && It->ParentClass && BP->GeneratedClass && It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.AddUnique(It->GetPathName()); } }
    if (BP->GeneratedClass)
    { TArray<UObject*> Instances; GetObjectsOfClass(BP->GeneratedClass, Instances, true, RF_NoFlags); for (UObject* O : Instances) { if (IsValid(O) && O->GetOutermost() != BP->GetOutermost()) { External.AddUnique(O->GetPathName()); } } }
    TArray<TSharedPtr<FJsonValue>> J; External.Sort(); for (const FString& P : External) { J.Add(MakeShared<FJsonValueString>(P)); }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"), J);
    return External.IsEmpty() || Fail(R, TEXT("function_external_instances_unsupported"), TEXT("Loaded external instances/derived Blueprints can be changed by structural regeneration; edit is unsupported in this slice."));
}
UEdGraph* FindGraph(UBlueprint* BP, const FString& Name, const FString& Id, const FString& Type, FResult& R, bool bMissingAllowed = false)
{
    if (!Pin::SafeName(Name, true) || !Pin::SafeName(Id, true) || !Pin::SafeName(Type, true)) { Fail(R, TEXT("function_graph_selector_invalid"), TEXT("Graph selectors must be bounded safe strings.")); return nullptr; }
    FGuid Guid; if (!Id.IsEmpty() && (!FGuid::Parse(Id, Guid) || !Guid.IsValid())) { Fail(R, TEXT("function_graph_selector_invalid"), TEXT("graph_id must be a valid nonzero GUID.")); return nullptr; }
    FString Kind = NormalizeType(Type);
    if (!Kind.IsEmpty() && Kind != TEXT("any") && Kind != TEXT("event") && Kind != TEXT("function") && Kind != TEXT("macro") && Kind != TEXT("delegate"))
    { Fail(R, TEXT("function_graph_type_unsupported"), TEXT("Unknown graph_type selector.")); return nullptr; }
    if (Name.IsEmpty() && Id.IsEmpty() && Kind.IsEmpty()) { Kind = TEXT("event"); }
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); UEdGraph* Found = nullptr;
    if (Graphs.Num() > MaxGraphs) { Fail(R, TEXT("function_graph_budget_exceeded"), TEXT("Graph selection exceeds 256 graphs.")); return nullptr; }
    for (UEdGraph* G : Graphs)
    {
        if (!IsValid(G) || (!Name.IsEmpty() && !G->GetName().Equals(Name, ESearchCase::IgnoreCase)) || (!Id.IsEmpty() && G->GraphGuid != Guid)
            || (!Kind.IsEmpty() && Kind != TEXT("any") && GraphType(BP, G) != Kind)) { continue; }
        if (Found) { Fail(R, TEXT("function_graph_ambiguous"), TEXT("Graph selector matches multiple graphs; supply exact name or GUID.")); return nullptr; } Found = G;
    }
    if (!Found && !bMissingAllowed) { Fail(R, TEXT("function_graph_not_found"), TEXT("Graph not found; no implicit event graph is created.")); }
    if (Found && (!Found->GraphGuid.IsValid() || !Found->GetSchema() || !Found->GetSchema()->IsA<UEdGraphSchema_K2>()))
    { Fail(R, TEXT("function_graph_state_invalid"), TEXT("Target requires a valid GUID and K2 schema.")); return nullptr; } return Found;
}
bool SameExistingNodes(UEdGraph* G, const TSharedRef<FJsonObject>& Before, const UEdGraphNode* AllowedChanged = nullptr)
{
    for (const auto& V : Before->GetArrayField(TEXT("nodes")))
    {
        const auto Old = V->AsObject(); UEdGraphNode* N = FindObject<UEdGraphNode>(nullptr, *Old->GetStringField(TEXT("object_path")));
        if (!IsValid(N) || !G->Nodes.Contains(N) || N->NodeGuid.ToString(EGuidFormats::Digits) != Old->GetStringField(TEXT("node_id"))) { return false; }
        if (N != AllowedChanged && Text(DescribeNode(N)) != Text(Old.ToSharedRef())) { return false; }
    }
    return true;
}
bool CheckNewName(UBlueprint* BP, const FString& Name, FResult& R)
{
    if (!Pin::Identifier(Name)) { return Fail(R, TEXT("function_name_invalid"), TEXT("New function/parameter names require an ASCII identifier of 1..100 characters.")); }
    if (FKismetNameValidator(BP).IsValid(Name) != EValidatorResult::Ok)
    { return Fail(R, TEXT("function_name_collision"), TEXT("Function name is invalid or already in use; automatic renaming is disabled.")); } return true;
}
bool ResolveOwnCompiledFunction(UBlueprint* BP, const FString& ClassPath, const FString& Name,
    UClass*& Owner, UFunction*& Function, UFunction*& Compiled, UEdGraph*& Callee, FResult& R)
{
    UClass* Generated = BP->GeneratedClass; Owner = BP->SkeletonGeneratedClass;
    if (!IsValid(Generated) || !IsValid(Owner) || Generated == Owner || BP->bIsRegeneratingOnLoad
        || Generated->GetPathName() != ClassPath || Generated->ClassGeneratedBy != BP || Owner->ClassGeneratedBy != BP
        || Generated->GetOutermost() != BP->GetOutermost() || Generated->GetAuthoritativeClass() != Generated
        || Owner->GetAuthoritativeClass() != Generated || Generated->IsEditorOnly()
        || Generated->HasAnyClassFlags(CLASS_Native | CLASS_NewerVersionExists))
    { return Fail(R, TEXT("function_self_class_unsupported"), TEXT("Use this Blueprint's exact current generated class path; skeleton, reinstanced, external and inherited class selectors are unsupported.")); }
    const FName FunctionFName(*Name);
    Compiled = Generated->FindFunctionByName(FunctionFName); Function = Owner->FindFunctionByName(FunctionFName);
    if (!IsValid(Compiled) || !IsValid(Function) || Compiled->GetOuterUClass() != Generated || Function->GetOuterUClass() != Owner
        || Compiled->GetName() != Name || Function->GetName() != Name || Compiled->Script.IsEmpty()
        || Compiled->GetSuperFunction() || Function->GetSuperFunction())
    { return Fail(R, TEXT("function_self_compiled_target_required"), TEXT("Requires an already compiled own function and its current skeleton signature; newly uncompiled, inherited and override functions remain unsupported.")); }
    if (BP->FunctionGraphs.Num() > MaxGraphs)
    { return Fail(R, TEXT("function_graph_budget_exceeded"), TEXT("Function selection exceeds 256 graphs.")); }
    for (UEdGraph* Candidate : BP->FunctionGraphs)
    {
        if (IsValid(Candidate) && Candidate->GetName() == Name)
        { if (Callee) { return Fail(R, TEXT("function_self_identity_invalid"), TEXT("Own function graph is ambiguous.")); } Callee = Candidate; }
    }
    FGuid GeneratedGuid, SkeletonGuid;
    if (!Callee || !Callee->GraphGuid.IsValid()
        || !UBlueprint::GetGuidFromClassByFieldName<UFunction>(Generated, FunctionFName, GeneratedGuid)
        || !UBlueprint::GetGuidFromClassByFieldName<UFunction>(Owner, FunctionFName, SkeletonGuid)
        || GeneratedGuid != Callee->GraphGuid || SkeletonGuid != Callee->GraphGuid)
    { return Fail(R, TEXT("function_self_identity_invalid"), TEXT("Compiled/skeleton function must resolve to the same exact owned function graph GUID.")); }
    const EFunctionFlags SemanticFlags = FUNC_Static | FUNC_Public | FUNC_Private | FUNC_Protected | FUNC_BlueprintCallable
        | FUNC_BlueprintPure | FUNC_Const | FUNC_EditorOnly | FUNC_Delegate | FUNC_Native | FUNC_Net;
    if ((Compiled->FunctionFlags & SemanticFlags) != (Function->FunctionFlags & SemanticFlags)
        || Function->HasAnyFunctionFlags(FUNC_Static | FUNC_Native | FUNC_Net))
    { return Fail(R, TEXT("function_self_signature_mismatch"), TEXT("Compiled/current signature flags differ, or the generated function is static, native or networked.")); }
    TArray<FProperty*> CompiledParams, CurrentParams;
    for (TFieldIterator<FProperty> It(Compiled); It && It->HasAnyPropertyFlags(CPF_Parm); ++It) { CompiledParams.Add(*It); }
    for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It) { CurrentParams.Add(*It); }
    if (CompiledParams.Num() != CurrentParams.Num() || CurrentParams.Num() > MaxParameters)
    { return Fail(R, TEXT("function_self_signature_mismatch"), TEXT("Compiled/current parameter counts differ or exceed 128.")); }
    const auto* Schema = GetDefault<UEdGraphSchema_K2>();
    const EPropertyFlags ParameterFlags = CPF_Parm | CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm | CPF_ConstParm;
    for (int32 I = 0; I < CurrentParams.Num(); ++I)
    {
        FEdGraphPinType OldType, CurrentType;
        if (CompiledParams[I]->GetFName() != CurrentParams[I]->GetFName()
            || (CompiledParams[I]->GetPropertyFlags() & ParameterFlags) != (CurrentParams[I]->GetPropertyFlags() & ParameterFlags)
            || !Schema->ConvertPropertyToPinType(CompiledParams[I], OldType) || !Schema->ConvertPropertyToPinType(CurrentParams[I], CurrentType)
            || OldType != CurrentType)
        { return Fail(R, TEXT("function_self_signature_mismatch"), TEXT("Compiled/current parameter order, names, directions or exact pin types differ; compile explicitly before calling.")); }
    }
    R.Data->SetStringField(TEXT("requested_function_class"), ClassPath);
    R.Data->SetStringField(TEXT("compiled_function_path"), Compiled->GetPathName());
    R.Data->SetNumberField(TEXT("compiled_script_bytes"), Compiled->Script.Num());
    R.Data->SetStringField(TEXT("skeleton_function_path"), Function->GetPathName());
    R.Data->SetStringField(TEXT("function_graph_guid"), Callee->GraphGuid.ToString(EGuidFormats::Digits));
    R.Data->SetBoolField(TEXT("compiled_signature_matches_skeleton"), true);
    R.Data->SetStringField(TEXT("self_call_scope"), TEXT("same Blueprint, existing compiled own function, current matching skeleton signature; compiled body freshness and arbitrary existing dependencies are not certified"));
    return true;
}
}

FResult Resolve(const FString& Path, const FString& Name, const FString& Id, const FString& Type, bool bCreate)
{
    FResult R; R.Data->SetBoolField(TEXT("resolved"), false); R.Data->SetBoolField(TEXT("created"), false);
    UBlueprint* BP = Load(Path, R); if (!BP) { return R; } R.Data->SetStringField(TEXT("blueprint"), BP->GetPathName());
    UEdGraph* G = FindGraph(BP, Name, Id, Type, R, bCreate); if (!R.ErrorCode.IsEmpty()) { return R; }
    TSharedRef<FJsonObject> Before = MakeShared<FJsonObject>(); if (!Capture(BP, G, Before, R)) { return R; } R.Data->SetObjectField(TEXT("before"), Before);
    if (G)
    {
        R.Data->SetBoolField(TEXT("resolved"), true); R.Data->SetObjectField(TEXT("graph"), DescribeGraph(BP, G)); After(BP, G, Before, R);
        R.bSuccess = R.bPostconditionPassed = R.bChangeStateKnown && !R.bChanged; R.Stage = R.bSuccess ? TEXT("complete") : TEXT("verification"); return R;
    }
    if (!bCreate) { Fail(R, TEXT("function_graph_not_found"), TEXT("Graph not found.")); return R; }
    if (!Id.IsEmpty() || NormalizeType(Type) != TEXT("function")) { Fail(R, TEXT("function_graph_creation_unsupported"), TEXT("Create requires graph_type=function, an exact name, and no graph_id.")); return R; }
    if (Before->GetArrayField(TEXT("graphs")).Num() >= MaxGraphs || !CheckNewName(BP, Name, R) || !MutationScope(BP, R))
    { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("function_graph_budget_exceeded"), TEXT("Creating a graph would exceed the graph budget.")); } return R; }
    R.Stage = TEXT("create_graph"); R.bMutationAttempted = true; BP->Modify();
    G = FBlueprintEditorUtils::CreateNewGraph(BP, FName(*Name), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    if (!G) { After(BP, nullptr, Before, R); Fail(R, TEXT("function_graph_creation_failed"), TEXT("CreateNewGraph returned null; inspect observed changes before retry.")); return R; }
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(BP, G, true, nullptr); R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"), true);
    After(BP, G, Before, R); TArray<UK2Node_FunctionEntry*> Entries; G->GetNodesOfClass(Entries);
    R.Data->SetBoolField(TEXT("created"), BP->FunctionGraphs.Contains(G)); R.Data->SetBoolField(TEXT("resolved"), BP->FunctionGraphs.Contains(G)); R.Data->SetObjectField(TEXT("graph"), DescribeGraph(BP, G));
    if (Entries.Num() == 1) { R.Data->SetObjectField(TEXT("entry_node"), DescribeNode(Entries[0])); }
    bool Valid = R.bChangeStateKnown && G->GetName() == Name && BP->FunctionGraphs.Contains(G) && Entries.Num() == 1 && G->Nodes.Num() == 1;
    const auto& OldGraphs = Before->GetArrayField(TEXT("graphs")); TArray<UEdGraph*> All; BP->GetAllGraphs(All); Valid &= All.Num() == OldGraphs.Num() + 1;
    for (const auto& V : OldGraphs)
    { const auto J = V->AsObject(); UEdGraph* Old = FindObject<UEdGraph>(nullptr, *J->GetStringField(TEXT("object_path"))); Valid &= Old && All.Contains(Old) && Text(DescribeGraph(BP, Old)) == Text(J.ToSharedRef()); }
    if (!Valid) { R.Stage = TEXT("verification"); Fail(R, TEXT("function_graph_postcondition_failed"), TEXT("Created graph/entry identity or preservation check failed; no rollback is claimed.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}

FResult AddParameter(const FString& Path, const FString& Name, const FString& Type, const FString& Direction,
    const FString& DefaultJson, const FString& TypeObject, bool bArray, const FString& GraphName, const FString& GraphId,
    const FString& RequestedGraphType, bool bCreateGraph)
{
    FResult R;
    if (bCreateGraph || bArray || !TypeObject.IsEmpty() || (!Direction.IsEmpty() && !Direction.Equals(TEXT("input"), ESearchCase::IgnoreCase)))
    { Fail(R, TEXT("function_option_unsupported"), TEXT("This slice adds scalar input parameters to an existing function graph. Resolve/create it separately; output/array/type_object remain pending.")); return R; }
    if (!Pin::Identifier(Name)) { Fail(R, TEXT("function_parameter_name_invalid"), TEXT("Parameter name requires an ASCII identifier of 1..100 characters.")); return R; }
    FEdGraphPinType TypeInfo; if (!Pin::PrimitiveType(Type, TypeInfo, R)) { return R; } FString Default;
    if (!DefaultJson.IsEmpty()) { TSharedPtr<FJsonValue> V; if (!Pin::ParseJson(DefaultJson, V, R) || !Pin::DefaultText(TypeInfo, V, Default, R)) { return R; } }
    UBlueprint* BP = Load(Path, R); if (!BP) { return R; }
    UEdGraph* G = FindGraph(BP, GraphName, GraphId, RequestedGraphType, R); if (!G) { return R; }
    if (GraphType(BP, G) != TEXT("function")) { Fail(R, TEXT("function_graph_type_unsupported"), TEXT("Parameters require a locally owned function graph.")); return R; }
    TSharedRef<FJsonObject> Before = MakeShared<FJsonObject>(); if (!Capture(BP, G, Before, R) || !MutationScope(BP, R)) { return R; } R.Data->SetObjectField(TEXT("before"), Before);
    TArray<UK2Node_FunctionEntry*> Entries; G->GetNodesOfClass(Entries);
    if (Entries.Num() != 1 || Entries[0]->UserDefinedPins.Num() >= MaxParameters) { Fail(R, TEXT("function_entry_unsupported"), TEXT("Requires exactly one entry node and fewer than 128 user parameters.")); return R; }
    UK2Node_EditablePinBase* Entry = Entries[0]; const FName PinName(*Name);
    if (Entry->FindPin(PinName) || Entry->UserDefinedPinExists(PinName)) { Fail(R, TEXT("function_parameter_collision"), TEXT("The entry already has a pin with this name; no automatic rename is performed.")); return R; }
    FText Reason; if (!Entry->CanCreateUserDefinedPin(TypeInfo, EGPD_Output, Reason)) { Fail(R, TEXT("function_parameter_rejected"), Reason.ToString()); return R; }
    const FGuid EntryId = Entry->NodeGuid; R.Stage = TEXT("add_parameter"); R.bMutationAttempted = true; BP->Modify(); G->Modify(); Entry->Modify();
    UEdGraphPin* NewPin = Entry->CreateUserDefinedPin(PinName, TypeInfo, EGPD_Output, false);
    bool Applied = NewPin != nullptr;
    if (Applied && !DefaultJson.IsEmpty())
    {
        TSharedPtr<FUserPinInfo> Info;
        for (const auto& P : Entry->UserDefinedPins) { if (P && P->PinName == PinName && P->DesiredPinDirection == EGPD_Output) { Info = P; break; } }
        Applied = Info && Entry->ModifyUserDefinedPinDefaultValue(Info, Default);
    }
    // Even a partial pin creation must dirty/rebuild its real owner. Failure is
    // observable; there is no automatic rollback or retry.
    Entry->ReconstructNode(); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"), true); After(BP, G, Before, R);
    UEdGraphPin* Actual = Entry->FindPin(PinName, EGPD_Output); const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    bool Verified = Applied && R.bChangeStateKnown && Entry->NodeGuid == EntryId && Actual && Actual->PinType == TypeInfo && G->Nodes.Num() == Before->GetArrayField(TEXT("nodes")).Num()
        && SameExistingNodes(G, Before, Entry) && (DefaultJson.IsEmpty() || Schema->DoesDefaultValueMatch(*Actual, Default));
    // UE58 TransferPersistentDataFromOldPin preserves PinId. Require the old
    // pin identities/defaults/links to survive; the new pin is the only addition.
    TSharedPtr<FJsonObject> OldEntryObject;
    for (const auto& V : Before->GetArrayField(TEXT("nodes"))) { if (V->AsObject()->GetStringField(TEXT("node_id")) == EntryId.ToString(EGuidFormats::Digits)) { OldEntryObject = V->AsObject(); break; } }
    if (!OldEntryObject) { Verified = false; }
    else
    {
        auto ActualEntry = DescribeNode(Entry); Verified &= ActualEntry->GetArrayField(TEXT("pins")).Num() == OldEntryObject->GetArrayField(TEXT("pins")).Num() + 1;
        for (const auto& V : OldEntryObject->GetArrayField(TEXT("pins")))
        {
            const auto OldPin = V->AsObject(); bool Found = false;
            for (const auto& W : ActualEntry->GetArrayField(TEXT("pins")))
            { auto Current = W->AsObject(); if (Current->GetStringField(TEXT("name")) == OldPin->GetStringField(TEXT("name")) && Current->GetStringField(TEXT("direction")) == OldPin->GetStringField(TEXT("direction")))
              { Found = true; Verified &= Text(Current.ToSharedRef()) == Text(OldPin.ToSharedRef()); break; } }
            Verified &= Found;
        }
    }
    Merge(*R.Data, *DescribeNode(Entry)); R.Data->SetStringField(TEXT("blueprint"), BP->GetPathName()); R.Data->SetObjectField(TEXT("graph"), DescribeGraph(BP, G)); R.Data->SetStringField(TEXT("parameter_name"), Name);
    R.Data->SetStringField(TEXT("direction"), TEXT("input")); R.Data->SetStringField(TEXT("default_value"), Actual ? Actual->GetDefaultAsString() : TEXT(""));
    R.Data->SetStringField(TEXT("requested_default_value"), Default); R.Data->SetBoolField(TEXT("default_requested"), !DefaultJson.IsEmpty()); R.Data->SetObjectField(TEXT("pin_type"), Pin::Describe(TypeInfo));
    if (!Verified) { R.Stage = TEXT("verification"); Fail(R, TEXT("function_parameter_postcondition_failed"), TEXT("Parameter/default or existing pin preservation check failed; actual partial changes are retained in before/after.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}

FResult AddCall(const FString& Path, const FString& ClassPath, const FString& FunctionName,
    const FString& DefaultsJson, const FString& PositionJson, const FString& GraphName, const FString& GraphId,
    const FString& RequestedGraphType, bool bCreateGraph, bool bLatent, bool bEditorOnly, bool bWildcard, bool bDeprecated, bool bInternal)
{
    FResult R;
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Fail(R, TEXT("function_editor_state_invalid"), TEXT("Function tools require the editor game thread outside PIE/SIE.")); return R; }
    if (bEditorOnly) { Fail(R, TEXT("function_editor_only_forbidden"), TEXT("Editor-only function references in game Blueprints are forbidden even when requested.")); return R; }
    if (bCreateGraph || bLatent || bWildcard || bDeprecated || bInternal)
    { Fail(R, TEXT("function_option_unsupported"), TEXT("Create the graph separately; latent/wildcard/deprecated/internal allowances remain unsupported.")); return R; }
    const bool bGeneratedClassRequested = ClassPath.StartsWith(TEXT("/Game/"));
    if (!Pin::SafeName(ClassPath) || (!ClassPath.StartsWith(TEXT("/Script/Engine.")) && !bGeneratedClassRequested) || !Pin::Identifier(FunctionName))
    { Fail(R, TEXT("function_reference_unsupported"), TEXT("Use an exact Engine native class, this Blueprint's own generated class, or an audited saved non-scene ActorComponent generated class.")); return R; }
    TSharedPtr<FJsonValue> DefaultsValue, PositionValue;
    if (!Pin::ParseJson(DefaultsJson, DefaultsValue, R) || !Pin::ParseJson(PositionJson, PositionValue, R)) { return R; }
    if (DefaultsValue->Type != EJson::Object || DefaultsValue->AsObject()->Values.Num() > 64)
    { Fail(R, TEXT("function_defaults_invalid"), TEXT("param_defaults_json must be an object of at most 64 named scalar defaults.")); return R; }
    if (PositionValue->Type != EJson::Array || PositionValue->AsArray().Num() != 2)
    { Fail(R, TEXT("function_position_invalid"), TEXT("node_position_json must contain exactly two int32 coordinates.")); return R; }
    int32 Position[2];
    for (int32 I = 0; I != 2; ++I)
    {
        const auto& V = PositionValue->AsArray()[I]; const double N = V->Type == EJson::Number ? V->AsNumber() : MAX_dbl;
        if (V->Type != EJson::Number || N < MIN_int32 || N > MAX_int32 || FMath::TruncToDouble(N) != N)
        { Fail(R, TEXT("function_position_invalid"), TEXT("Node coordinates must be exact int32 JSON numbers, excluding bool.")); return R; }
        Position[I] = static_cast<int32>(N);
    }
    UBlueprint* BP = nullptr; UBlueprint* ExternalBP = nullptr; UClass* Owner = nullptr;
    UFunction* Function = nullptr; UFunction* CompiledFunction = nullptr; UFunction* ExpectedResolvedFunction = nullptr; UEdGraph* Callee = nullptr; UEdGraph* G = nullptr;
    bool bOwnClassRequested = false, bExternalComponentRequested = false;
    TStrongObjectPtr<UBlueprint> KeepTarget, KeepExternal;
    TSharedRef<FJsonObject> ExternalBefore = MakeShared<FJsonObject>();
    if (bGeneratedClassRequested)
    {
        BP = Load(Path, R); if (!BP || !MutationScope(BP, R)) { return R; }
        KeepTarget.Reset(BP);
        bOwnClassRequested = BP->GeneratedClass && BP->GeneratedClass->GetPathName() == ClassPath;
        if (bOwnClassRequested)
        {
            if (!ResolveOwnCompiledFunction(BP, ClassPath, FunctionName, Owner, Function, CompiledFunction, Callee, R)) { return R; }
        }
        else
        {
            const FString Package = FPackageName::ObjectPathToPackageName(ClassPath);
            if (!FPackageName::IsValidLongPackageName(Package) || ClassPath != Package + TEXT(".") + FPackageName::GetShortName(Package) + TEXT("_C"))
            { Fail(R, TEXT("function_external_component_path_invalid"), TEXT("External generated calls require an exact /Game/Package.Package_C path; skeleton/reinstanced/short aliases are not loaded.")); return R; }
            G = FindGraph(BP, GraphName, GraphId, RequestedGraphType, R); if (!G) { return R; }
            TSharedRef<FJsonObject> LoadBefore = MakeShared<FJsonObject>();
            if (!Capture(BP, G, LoadBefore, R)) { return R; }
            R.Data->SetObjectField(TEXT("before_external_class_load"), LoadBefore);
            const bool ClassWasLoaded = FindObject<UClass>(nullptr, *ClassPath) != nullptr;
            R.Data->SetBoolField(TEXT("external_class_loaded_during_operation"), !ClassWasLoaded);
            R.Data->SetBoolField(TEXT("implicit_source_postload_work_possible"), !ClassWasLoaded);
            R.Data->SetBoolField(TEXT("external_load_side_effects_fully_observed"), false);
            UBlueprint* LoadedSource = FindObject<UBlueprint>(nullptr, *(Package + TEXT(".") + FPackageName::GetShortName(Package)));
            const bool SourceWasObserved = IsValid(LoadedSource);
            const bool SourceDirty = SourceWasObserved && LoadedSource->GetOutermost()->IsDirty();
            const EBlueprintStatus SourceStatus = SourceWasObserved ? LoadedSource->Status.GetValue() : BS_Unknown;
            auto SourceLoad = MakeShared<FJsonObject>();
            SourceLoad->SetBoolField(TEXT("observed_before_load"), SourceWasObserved);
            if (SourceWasObserved) { SourceLoad->SetBoolField(TEXT("dirty_before"), SourceDirty); SourceLoad->SetNumberField(TEXT("status_before"), SourceStatus); }
            R.Stage = TEXT("load_external_component_class");
            Owner = LoadObject<UClass>(nullptr, *ClassPath);
            const auto Inspection = GeneratedComponent::InspectSavedClass(Owner);
            R.Data->SetObjectField(TEXT("external_component_validation"), Inspection);
            After(BP, G, LoadBefore, R);
            R.Data->SetBoolField(TEXT("target_preserved_during_external_load"), R.bChangeStateKnown && !R.bChanged);
            if (!R.bChangeStateKnown || R.bChanged)
            { Fail(R, TEXT("function_external_load_changed_target"), TEXT("External class resolution changed or obscured the target Blueprint snapshot; no call node was dispatched.")); return R; }
            LoadedSource = IsValid(Owner) ? Cast<UBlueprint>(Owner->ClassGeneratedBy) : LoadedSource;
            const bool SourceObserved = IsValid(LoadedSource);
            SourceLoad->SetBoolField(TEXT("observed_after_load"), SourceObserved);
            if (SourceObserved) { SourceLoad->SetBoolField(TEXT("dirty_after"), LoadedSource->GetOutermost()->IsDirty()); SourceLoad->SetNumberField(TEXT("status_after"), LoadedSource->Status.GetValue()); }
            R.Data->SetObjectField(TEXT("external_source_load_observation"), SourceLoad);
            if (!SourceObserved) { R.bChangeStateKnown = false; }
            if (SourceWasObserved && (!SourceObserved || LoadedSource->GetOutermost()->IsDirty() != SourceDirty || LoadedSource->Status != SourceStatus))
            { R.bChanged = true; Fail(R, TEXT("function_external_load_changed_source"), TEXT("Source Blueprint dirty/status changed during class resolution; no call node was dispatched.")); return R; }
            if (!IsValid(Owner) || Owner->GetPathName() != ClassPath || !Inspection->GetBoolField(TEXT("success")))
            { Fail(R, TEXT("function_external_component_unsupported"), TEXT("External class failed saved/clean/compiled runtime ActorComponent inspection; inspect external_component_validation and load observations.")); return R; }
            ExternalBP = Cast<UBlueprint>(Owner->ClassGeneratedBy);
            if (!ExternalBP || ExternalBP == BP || (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(Owner)))
            { Fail(R, TEXT("function_external_self_context_unsupported"), TEXT("External calls must target a distinct component class, not an inherited/self context.")); return R; }
            KeepExternal.Reset(ExternalBP);
            UClass* SkeletonOwner = nullptr; UFunction* SkeletonFunction = nullptr;
            if (!ResolveOwnCompiledFunction(ExternalBP, ClassPath, FunctionName, SkeletonOwner, SkeletonFunction, CompiledFunction, Callee, R)) { return R; }
            // Serialize the authoritative external class, never SKEL/REINST.
            Function = CompiledFunction; Owner = ExternalBP->GeneratedClass; bExternalComponentRequested = true;
            // Editor node resolution uses the host skeleton as SelfScope and
            // therefore resolves external generated functions to their current
            // skeleton. The serialized member parent remains authoritative.
            ExpectedResolvedFunction = SkeletonFunction;
            R.Data->RemoveField(TEXT("self_call_scope"));
            R.Data->SetStringField(TEXT("external_call_scope"), TEXT("saved clean non-scene ActorComponent, own compiled function with matching skeleton signature/GUID; explicit external object input, no inherited target or arbitrary class"));
            if (!Capture(ExternalBP, Callee, ExternalBefore, R)) { return R; }
            R.Data->SetObjectField(TEXT("external_function_before"), ExternalBefore);
            R.Stage = TEXT("preflight");
        }
    }
    else
    {
        Owner = LoadObject<UClass>(nullptr, *ClassPath);
        if (!IsValid(Owner) || Owner->GetPathName() != ClassPath || Owner->GetOutermost()->GetName() != TEXT("/Script/Engine")
            || !Owner->HasAnyClassFlags(CLASS_Native) || Owner->IsEditorOnly())
        { Fail(R, TEXT("function_reference_unsupported"), TEXT("Function owner is not the exact supported runtime Engine native class.")); return R; }
        Function = Owner->FindFunctionByName(FName(*FunctionName));
        if (!IsValid(Function) || Function->GetOuterUClass() != Owner || Function->GetName() != FunctionName)
        { Fail(R, TEXT("function_not_found"), TEXT("Exact function not found on the requested declaring class; inherited/renamed resolution is not implicit.")); return R; }
    }
    const bool AuditedEngineInstance = !bGeneratedClassRequested && Owner == UActorComponent::StaticClass()
        && Function->GetFName() == TEXT("GetOwner") && !Function->HasAnyFunctionFlags(FUNC_Static);
    const bool AuditedObjectSignature = AuditedEngineInstance
        || (Owner == UGameplayStatics::StaticClass() && Function->GetFName() == TEXT("GetPlayerController"))
        || (Owner == UKismetSystemLibrary::StaticClass() && Function->GetFName() == TEXT("IsValid"));
    if (Function->HasAnyFunctionFlags(FUNC_EditorOnly))
    { Fail(R, TEXT("function_editor_only_forbidden"), TEXT("The target UFunction is editor-only.")); return R; }
    if (!Function->HasAllFunctionFlags(FUNC_Public | FUNC_BlueprintCallable) || (!bGeneratedClassRequested && !AuditedEngineInstance && !Function->HasAnyFunctionFlags(FUNC_Static))
        || Function->HasAnyFunctionFlags(FUNC_Private | FUNC_Protected | FUNC_Delegate)
        || Function->HasMetaData(TEXT("Latent")) || Function->HasMetaData(TEXT("DeprecatedFunction"))
        || Function->HasMetaData(TEXT("CustomThunk")) || Function->GetBoolMetaData(TEXT("BlueprintInternalUseOnly"))
        || Owner->GetBoolMetaDataHierarchical(TEXT("BlueprintInternalUseOnlyHierarchical"))
        || UEdGraphSchema_K2::HasWildcardParams(Function))
    { Fail(R, TEXT("function_callable_unsupported"), TEXT("Requires a public callable runtime function (Engine static, audited ActorComponent.GetOwner, own generated instance or audited external component), without latent, wildcard, custom-thunk, deprecated or internal behavior.")); return R; }
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    TMap<FName, FEdGraphPinType> InputTypes, OutputTypes;
    int32 ParameterCount = 0;
    for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
    {
        FEdGraphPinType T;
        if (++ParameterCount > MaxParameters || !Schema->ConvertPropertyToPinType(*It, T)
            || (!Pin::IsPrimitive(T) && !(AuditedObjectSignature && RuntimeObject::IsSupportedPin(T)))
            || It->HasAnyPropertyFlags(CPF_ReferenceParm))
        { Fail(R, TEXT("function_signature_pending"), TEXT("Requires at most 128 scalar primitive parameters; audited native object pins are additionally supported for GetPlayerController/IsValid/ActorComponent.GetOwner. Other object/struct/container/reference signatures remain pending.")); return R; }
        if (It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm)) { OutputTypes.Add(It->GetFName(), T); }
        else { InputTypes.Add(It->GetFName(), T); }
    }
    TMap<FName, FString> Defaults;
    for (const auto& Pair : DefaultsValue->AsObject()->Values)
    {
        const FString Key(Pair.Key.ToView()); const FName Name(*Key); const FEdGraphPinType* T = InputTypes.Find(Name);
        if (Defaults.Contains(Name)) { Fail(R, TEXT("function_defaults_invalid"), TEXT("Case-insensitive duplicate default pin name.")); return R; }
        if (!T) { Fail(R, TEXT("function_default_pin_not_found"), TEXT("A requested default does not name an input parameter of this function.")); return R; }
        FString Value; if (!Pin::DefaultText(*T, Pair.Value, Value, R)) { return R; } Defaults.Add(Name, MoveTemp(Value));
    }
    if (!BP) { BP = Load(Path, R); } if (!BP) { return R; }
    if (!G) { G = FindGraph(BP, GraphName, GraphId, RequestedGraphType, R); } if (!G) { return R; }
    const FString Kind = GraphType(BP, G);
    if (Kind != TEXT("function") && Kind != TEXT("event"))
    { Fail(R, TEXT("function_graph_type_unsupported"), TEXT("Call creation currently supports owned function/event graphs.")); return R; }
    const FString WorldContextName = Function->GetMetaData(TEXT("WorldContext"));
    if (!WorldContextName.IsEmpty() && (!FBlueprintEditorUtils::ImplementsGetWorld(BP) || Schema->IsStaticFunctionGraph(G)
        || UEdGraphSchema_K2::IsConstructionScript(G) || Defaults.Contains(FName(*WorldContextName))))
    { Fail(R, TEXT("function_world_context_unsupported"), TEXT("World-context calls require an instance graph with intrinsic GetWorld outside construction scripts; a literal override of the compiler-provided world context is not supported.")); return R; }
    TSharedRef<FJsonObject> Before = MakeShared<FJsonObject>();
    if (!Capture(BP, G, Before, R) || (!bGeneratedClassRequested && !MutationScope(BP, R))) { return R; }
    R.Data->SetObjectField(TEXT("before"), Before);
    TSharedRef<FJsonObject> CalleeBefore = MakeShared<FJsonObject>();
    if (bOwnClassRequested)
    {
        if (Callee == G) { Fail(R, TEXT("function_self_recursion_unsupported"), TEXT("Direct recursive self-call authoring is outside this slice.")); return R; }
        if (!Capture(BP, Callee, CalleeBefore, R)) { return R; }
        R.Data->SetObjectField(TEXT("callee_before"), CalleeBefore);
    }
    if (G->Nodes.Num() >= MaxNodes) { Fail(R, TEXT("function_node_budget_exceeded"), TEXT("The new call would exceed 2048 nodes.")); return R; }
    FText Reason;
    if (!Schema->CanFunctionBeUsedInGraph(Owner, Function, G, UEdGraphSchema_K2::FT_Pure | UEdGraphSchema_K2::FT_Imperative | UEdGraphSchema_K2::FT_Const, false, &Reason))
    { Fail(R, TEXT("function_graph_call_rejected"), Reason.ToString()); return R; }
    R.Stage = TEXT("add_call"); R.bMutationAttempted = true; BP->Modify(); G->Modify();
    // Spawner::Create can return a globally cached promotable operator even for
    // an explicit class. This public exact creator preserves the requested UFunction.
    FGraphNodeCreator<UK2Node_CallFunction> Creator(*G);
    UK2Node_CallFunction* Call = Creator.CreateNode(false);
    Call->SetFromFunction(Function); Call->NodePosX = Position[0]; Call->NodePosY = Position[1]; Creator.Finalize();
    bool Applied = true;
    for (const auto& Pair : Defaults)
    {
        UEdGraphPin* P = Call->FindPin(Pair.Key, EGPD_Input);
        if (!P) { Applied = false; continue; }
        Schema->TrySetDefaultValue(*P, Pair.Value, false);
        Applied &= Schema->DoesDefaultValueMatch(*P, Pair.Value) && Schema->IsCurrentPinDefaultValid(P).IsEmpty();
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    After(BP, G, Before, R);
    if (!ExpectedResolvedFunction) { ExpectedResolvedFunction = Function; }
    UFunction* ResolvedFunction = Call->GetTargetFunction();
    bool Verified = Applied && R.bChangeStateKnown && Call->GetClass() == UK2Node_CallFunction::StaticClass()
        && ResolvedFunction == ExpectedResolvedFunction && G->Nodes.Contains(Call) && Call->NodeGuid.IsValid()
        && G->Nodes.Num() == Before->GetArrayField(TEXT("nodes")).Num() + 1 && SameExistingNodes(G, Before)
        && Call->NodePosX == Position[0] && Call->NodePosY == Position[1];
    if (bOwnClassRequested)
    {
        const UEdGraphPin* Self = Call->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
        TSharedRef<FJsonObject> CalleeAfter = MakeShared<FJsonObject>(); const bool CalleeObserved = Capture(BP, Callee, CalleeAfter, R);
        R.Data->SetObjectField(TEXT("callee_after"), CalleeAfter);
        if (!CalleeObserved) { R.bChangeStateKnown = false; }
        Verified &= CalleeObserved && SameExistingNodes(Callee, CalleeBefore)
            && Callee->Nodes.Num() == CalleeBefore->GetArrayField(TEXT("nodes")).Num()
            && Call->FunctionReference.IsSelfContext() && Call->FunctionReference.GetMemberParentClass() == nullptr
            && Call->FunctionReference.GetMemberGuid() == Callee->GraphGuid
            && Call->FunctionReference.GetMemberName() == Function->GetFName()
            && Self && Self->PinType.PinCategory == UEdGraphSchema_K2::PC_Object && Self->PinType.PinSubCategory == UEdGraphSchema_K2::PSC_Self
            && Self->PinType.PinSubCategoryObject == nullptr && Self->DefaultObject == nullptr && Self->LinkedTo.IsEmpty();
        R.Data->SetBoolField(TEXT("self_context"), Call->FunctionReference.IsSelfContext());
        R.Data->SetStringField(TEXT("runtime_reference_scope"), TEXT("self member GUID/name and self pin only; no Cubeless or external generated class reference introduced"));
    }
    if (bExternalComponentRequested)
    {
        const UEdGraphPin* Self = Call->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
        TSharedRef<FJsonObject> ExternalAfter = MakeShared<FJsonObject>();
        const bool SourceObserved = Capture(ExternalBP, Callee, ExternalAfter, R);
        const bool SourcePreserved = SourceObserved && Text(ExternalBefore) == Text(ExternalAfter);
        R.Data->SetObjectField(TEXT("external_function_after"), ExternalAfter);
        R.Data->SetBoolField(TEXT("external_source_observed_unchanged"), SourcePreserved);
        if (!SourceObserved) { R.bChangeStateKnown = false; }
        else if (!SourcePreserved) { R.bChanged = true; }
        Verified &= SourcePreserved && ResolvedFunction == ExpectedResolvedFunction
            && !Call->FunctionReference.IsSelfContext() && Call->FunctionReference.GetMemberParentClass() == Owner
            && Call->FunctionReference.GetMemberGuid() == Callee->GraphGuid
            && Call->FunctionReference.GetMemberName() == Function->GetFName()
            && Self && Self->PinType.PinCategory == UEdGraphSchema_K2::PC_Object && Self->PinType.PinSubCategory.IsNone()
            && Self->PinType.PinSubCategoryObject.Get() == Owner && Self->DefaultObject == nullptr && Self->LinkedTo.IsEmpty();
        R.Data->SetBoolField(TEXT("self_context"), Call->FunctionReference.IsSelfContext());
        R.Data->SetStringField(TEXT("target_component_class"), Owner->GetPathName());
        R.Data->SetStringField(TEXT("serialized_member_parent_class"), Call->FunctionReference.GetMemberParentClass() ? Call->FunctionReference.GetMemberParentClass()->GetPathName() : FString());
        R.Data->SetStringField(TEXT("runtime_reference_scope"), TEXT("audited external runtime ActorComponent generated class and own function GUID/name; no Cubeless class/function reference"));
    }
    if (AuditedEngineInstance)
    {
        const UEdGraphPin* Self = Call->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
        Verified &= Self && Self->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
            && Self->PinType.PinSubCategory.IsNone() && Self->PinType.PinSubCategoryObject.Get() == Owner
            && Self->DefaultObject == nullptr && Self->LinkedTo.IsEmpty();
        R.Data->SetStringField(TEXT("runtime_reference_scope"), TEXT("audited native ActorComponent.GetOwner instance target; no editor or Cubeless class/function reference"));
    }
    if (!WorldContextName.IsEmpty())
    {
        const UEdGraphPin* World = Call->FindPin(FName(*WorldContextName), EGPD_Input);
        Verified &= World && RuntimeObject::IsSupportedPin(World->PinType) && World->LinkedTo.IsEmpty() && World->DefaultObject == nullptr;
        R.Data->SetStringField(TEXT("world_context_pin"), WorldContextName);
        R.Data->SetBoolField(TEXT("world_context_hidden"), World && World->bHidden);
        R.Data->SetStringField(TEXT("world_context_resolution"), TEXT("standard K2 compiler provides the owning instance as world context; controller availability is a runtime result"));
    }
    for (const auto& Pair : InputTypes)
    { const UEdGraphPin* P = Call->FindPin(Pair.Key, EGPD_Input); Verified &= P && P->PinType == Pair.Value; }
    for (const auto& Pair : OutputTypes)
    { const UEdGraphPin* P = Call->FindPin(Pair.Key, EGPD_Output); Verified &= P && P->PinType == Pair.Value; }
    auto AppliedDefaults = MakeShared<FJsonObject>();
    for (const auto& Pair : Defaults)
    { const UEdGraphPin* P = Call->FindPin(Pair.Key, EGPD_Input); if (P) { AppliedDefaults->SetStringField(P->PinName.ToString(), P->GetDefaultAsString()); } }
    Merge(*R.Data, *DescribeNode(Call)); R.Data->SetObjectField(TEXT("graph"), DescribeGraph(BP, G));
    R.Data->SetStringField(TEXT("blueprint"), BP->GetPathName()); R.Data->SetStringField(TEXT("function_name"), Function->GetName());
    R.Data->SetStringField(TEXT("function_class"), bOwnClassRequested ? ClassPath : Owner->GetPathName()); R.Data->SetStringField(TEXT("function_path"), ResolvedFunction ? ResolvedFunction->GetPathName() : FString());
    R.Data->SetBoolField(TEXT("is_static"), Function->HasAnyFunctionFlags(FUNC_Static)); R.Data->SetBoolField(TEXT("is_pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
    R.Data->SetBoolField(TEXT("is_latent"), Function->HasMetaData(TEXT("Latent"))); R.Data->SetObjectField(TEXT("param_defaults_applied"), AppliedDefaults);
    R.Data->SetStringField(TEXT("node_creation_api"), TEXT("FGraphNodeCreator<UK2Node_CallFunction>/SetFromFunction/Finalize"));
    R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"), false);
    if (!Verified) { R.Stage = TEXT("verification"); Fail(R, TEXT("function_call_postcondition_failed"), TEXT("Call identity/signature/defaults or existing graph preservation check failed; inspect actual partial changes before retry.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}
