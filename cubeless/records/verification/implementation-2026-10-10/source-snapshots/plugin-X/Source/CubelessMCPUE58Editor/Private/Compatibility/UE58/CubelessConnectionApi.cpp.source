// Guarded, non-atomic replacement of one standard K2 exec edge.
// No restore/retry, compile/save, asset load, or Cubeless runtime reference.
#include "Compatibility/UE58/CubelessConnectionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessGraphApi.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::Connection
{
const TCHAR* SchemaResponseName(ECanCreateConnectionResponse Response)
{
    switch (Response)
    {
    case CONNECT_RESPONSE_MAKE: return TEXT("make");
    case CONNECT_RESPONSE_DISALLOW: return TEXT("disallow");
    case CONNECT_RESPONSE_BREAK_OTHERS_A: return TEXT("break_others_a");
    case CONNECT_RESPONSE_BREAK_OTHERS_B: return TEXT("break_others_b");
    case CONNECT_RESPONSE_BREAK_OTHERS_AB: return TEXT("break_others_ab");
    case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE: return TEXT("make_with_conversion_node");
    case CONNECT_RESPONSE_MAKE_WITH_PROMOTION: return TEXT("make_with_promotion");
    default: return TEXT("unknown");
    }
}

namespace
{
void Fail(const TSharedRef<FJsonObject>& R, const TCHAR* Code, const TCHAR* Message)
{ R->SetStringField(TEXT("error_code"), Code); R->SetStringField(TEXT("error"), Message); }

bool SafeSelector(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() >= NAME_SIZE) { return false; }
    for (const TCHAR Ch : Value) { if (Ch < 32 || Ch == 127) { return false; } }
    return true;
}

bool Idle(UBlueprint* BP)
{
    return IsValid(BP) && BP->BlueprintType == BPTYPE_Normal && BP->ParentClass && !BP->bBeingCompiled
        && !BP->bQueuedForCompilation && !BP->bSuppressStructurallyModified && !BP->bIsRegeneratingOnLoad
        && BP->Status != BS_BeingCreated;
}

bool OwnGraph(UBlueprint* BP, UEdGraph* G)
{
    return IsValid(G) && G->GraphGuid.IsValid() && FBlueprintEditorUtils::FindBlueprintForGraph(G) == BP
        && (BP->FunctionGraphs.Contains(G) || BP->MacroGraphs.Contains(G)
            || BP->UbergraphPages.Contains(G) || BP->EventGraphs.Contains(G));
}

// A name that identifies multiple pins is not an exact endpoint. Do not choose
// the first match or repair invalid identities as part of a connection request.
UEdGraphPin* FindExactPin(UEdGraph* G, const FString& NodePath, const FString& PinName,
    EEdGraphPinDirection Direction, UK2Node*& Node)
{
    Node = FindObject<UK2Node>(nullptr, *NodePath);
    if (!IsValid(Node) || Node->GetGraph() != G || !G->Nodes.Contains(Node) || !Node->NodeGuid.IsValid()) { return nullptr; }
    UEdGraphPin* Found = nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == Direction && Pin->PinName.ToString() == PinName)
        {
            if (Found || Pin->IsPendingKill() || Pin->bOrphanedPin || !Pin->PinId.IsValid()) { return nullptr; }
            Found = Pin;
        }
    }
    return Found;
}

TSharedRef<FJsonObject> Identity(const UEdGraphPin* Pin)
{
    auto Row = MakeShared<FJsonObject>(); const UEdGraphNode* Node = Pin->GetOwningNodeUnchecked();
    Row->SetStringField(TEXT("node_path"), Node->GetPathName());
    Row->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    Row->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
    Row->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::Digits));
    Row->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("input"));
    return Row;
}

bool CapturePin(UEdGraph* G, UEdGraphPin* Pin, const TSharedRef<FJsonObject>& Row)
{
    if (!Pin || Pin->IsPendingKill() || !IsValid(Pin->GetOwningNodeUnchecked())) { return false; }
    Row->Values = Identity(Pin)->Values;
    auto Type = MakeShared<FJsonObject>();
    Type->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
    Type->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
    Type->SetStringField(TEXT("type_object"), Pin->PinType.PinSubCategoryObject.IsValid() ? Pin->PinType.PinSubCategoryObject->GetPathName() : FString());
    Type->SetNumberField(TEXT("container_type"), static_cast<int32>(Pin->PinType.ContainerType));
    Type->SetBoolField(TEXT("is_reference"), Pin->PinType.bIsReference);
    Type->SetBoolField(TEXT("is_const"), Pin->PinType.bIsConst);
    Row->SetObjectField(TEXT("pin_type"), Type); Row->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin);
    Row->SetNumberField(TEXT("link_count"), Pin->LinkedTo.Num());
    bool Valid = Pin->PinId.IsValid() && !Pin->bOrphanedPin && Pin->LinkedTo.Num() <= 2048;
    TArray<TSharedPtr<FJsonValue>> Links; TSet<UEdGraphPin*> Seen;
    for (int32 Index = 0; Index < FMath::Min(Pin->LinkedTo.Num(), 2048); ++Index)
    {
        UEdGraphPin* Other = Pin->LinkedTo[Index];
        UEdGraphNode* OtherNode = Other ? Other->GetOwningNodeUnchecked() : nullptr;
        if (!Other || Other->IsPendingKill() || !IsValid(OtherNode) || OtherNode->GetGraph() != G
            || !G->Nodes.Contains(OtherNode) || !OtherNode->Pins.Contains(Other))
        { Valid = false; Links.Add(MakeShared<FJsonValueNull>()); continue; }
        auto Link = Identity(Other); const bool Reciprocal = Other->LinkedTo.Contains(Pin);
        Link->SetBoolField(TEXT("reciprocal"), Reciprocal); Links.Add(MakeShared<FJsonValueObject>(Link));
        Valid &= !Seen.Contains(Other) && Reciprocal && Other->PinId.IsValid() && OtherNode->NodeGuid.IsValid();
        Seen.Add(Other);
    }
    Row->SetArrayField(TEXT("linked_to"), Links); Row->SetBoolField(TEXT("observation_complete"), Valid);
    return Valid;
}

struct FEndpoints
{
    UK2Node* SourceNode = nullptr; UK2Node* TargetNode = nullptr; UK2Node* OldNode = nullptr;
    UEdGraphPin* Source = nullptr; UEdGraphPin* Target = nullptr; UEdGraphPin* Old = nullptr;
};

bool Resolve(UEdGraph* G, const FString& SourcePath, const FString& SourceName,
    const FString& TargetPath, const FString& TargetName, const FString& OldPath, const FString& OldName,
    FEndpoints& E)
{
    E.Source = FindExactPin(G, SourcePath, SourceName, EGPD_Output, E.SourceNode);
    E.Target = FindExactPin(G, TargetPath, TargetName, EGPD_Input, E.TargetNode);
    E.Old = FindExactPin(G, OldPath, OldName, EGPD_Input, E.OldNode);
    return E.Source && E.Target && E.Old;
}

bool Capture(UEdGraph* G, UBlueprint* BP, const FEndpoints& E, const TSharedRef<FJsonObject>& Out)
{
    auto Source = MakeShared<FJsonObject>(), Target = MakeShared<FJsonObject>(), Old = MakeShared<FJsonObject>();
    Out->SetObjectField(TEXT("source"), Source); Out->SetObjectField(TEXT("target"), Target); Out->SetObjectField(TEXT("old_target"), Old);
    bool Valid = CapturePin(G, E.Source, Source);
    Valid &= CapturePin(G, E.Target, Target); Valid &= CapturePin(G, E.Old, Old);
    Out->SetBoolField(TEXT("dirty"), BP->GetOutermost()->IsDirty());
    Out->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(BP->Status.GetValue()));
    Out->SetStringField(TEXT("graph_guid"), G->GraphGuid.ToString(EGuidFormats::Digits));
    Out->SetStringField(TEXT("schema_class"), G->GetSchema() ? G->GetSchema()->GetClass()->GetPathName() : FString());
    Out->SetNumberField(TEXT("graph_node_count"), G->Nodes.Num()); Out->SetBoolField(TEXT("observation_complete"), Valid);
    return Valid;
}

bool ExactOldEdge(const FEndpoints& E)
{
    return E.Source != E.Target && E.Source != E.Old && E.Target != E.Old
        && E.Source->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
        && E.Source->PinType.ContainerType == EPinContainerType::None && E.Source->PinType.PinSubCategory.IsNone()
        && !E.Source->PinType.PinSubCategoryObject.IsValid() && !E.Source->PinType.bIsReference && !E.Source->PinType.bIsConst
        && E.Source->PinType == E.Target->PinType && E.Source->PinType == E.Old->PinType
        && E.Source->LinkedTo.Num() == 1 && E.Source->LinkedTo[0] == E.Old
        && E.Old->LinkedTo.Num() == 1 && E.Old->LinkedTo[0] == E.Source && E.Target->LinkedTo.IsEmpty();
}
}

TSharedRef<FJsonObject> ReplaceExecLink(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName,
    const FString& ExpectedOldTargetNodePath, const FString& ExpectedOldTargetPinName,
    int32 ExpectedSourceLinkCount, const FString& ExpectedSchemaResponse)
{
    auto R = MakeShared<FJsonObject>(); auto Data = MakeShared<FJsonObject>(); R->SetObjectField(TEXT("data"), Data);
    R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("stage"), TEXT("preflight"));
    R->SetBoolField(TEXT("mutation_attempted"), false); R->SetBoolField(TEXT("change_state_known"), true);
    R->SetBoolField(TEXT("changed"), false); R->SetField(TEXT("engine_return"), MakeShared<FJsonValueNull>());
    R->SetBoolField(TEXT("postcondition_passed"), false); R->SetBoolField(TEXT("compiled"), false); R->SetBoolField(TEXT("saved"), false);
    R->SetBoolField(TEXT("loaded_during_operation"), false);
    Data->SetBoolField(TEXT("explicit_compile_requested"), false); Data->SetBoolField(TEXT("explicit_save_requested"), false);
    Data->SetBoolField(TEXT("replacement_guard_requested"), true); Data->SetBoolField(TEXT("replacement_guard_verified"), false);
    Data->SetBoolField(TEXT("expected_existing_link_removed"), false); Data->SetNumberField(TEXT("replaced_source_link_count"), 0);
    Data->SetNumberField(TEXT("try_create_connection_count"), 0);
    Data->SetNumberField(TEXT("expected_existing_source_link_count"), ExpectedSourceLinkCount);
    Data->SetStringField(TEXT("expected_existing_source_link_target_node_path"), ExpectedOldTargetNodePath);
    Data->SetStringField(TEXT("expected_existing_source_link_target_pin"), ExpectedOldTargetPinName);
    Data->SetStringField(TEXT("expected_schema_response"), ExpectedSchemaResponse);
    Data->SetStringField(TEXT("observation_scope"), TEXT("three loaded exec endpoints plus graph identity/count and Blueprint dirty/status; Python independently verifies the complete graph delta; callbacks may have other unobserved effects"));
    Data->SetStringField(TEXT("atomicity"), TEXT("not atomic; no rollback or automatic retry"));
    if (ExpectedSourceLinkCount != 1 || ExpectedSchemaResponse != TEXT("break_others_a")
        || !SafeSelector(GraphPath) || !SafeSelector(SourceNodePath) || !SafeSelector(SourcePinName)
        || !SafeSelector(TargetNodePath) || !SafeSelector(TargetPinName)
        || !SafeSelector(ExpectedOldTargetNodePath) || !SafeSelector(ExpectedOldTargetPinName))
    { Fail(R, TEXT("connection_replacement_guard_invalid"), TEXT("Supply exact bounded endpoints, expected source count 1, and expected_schema_response='break_others_a'.")); return R; }
    const auto Plan = Graph::ReadConnectionPlan(GraphPath, SourceNodePath, SourcePinName, TargetNodePath, TargetPinName);
    Data->SetObjectField(TEXT("initial_connection_plan"), Plan);
    if (!Plan->GetBoolField(TEXT("success")))
    { Fail(R, TEXT("connection_replacement_guard_mismatch"), TEXT("Loaded graph connection preflight failed; inspect initial_connection_plan.")); return R; }
    UEdGraph* G = FindObject<UEdGraph>(nullptr, *GraphPath);
    UBlueprint* BP = G ? FBlueprintEditorUtils::FindBlueprintForGraph(G) : nullptr;
    const UEdGraphSchema_K2* Schema = G ? Cast<UEdGraphSchema_K2>(G->GetSchema()) : nullptr;
    if (!Idle(BP) || !OwnGraph(BP, G) || !Schema || Schema->GetClass() != UEdGraphSchema_K2::StaticClass()
        || !Schema->DoesGraphSupportImpureFunctions(G))
    { Fail(R, TEXT("connection_graph_scope_unsupported"), TEXT("Requires an idle normal Blueprint and directly owned standard K2 event/function/macro graph supporting execution flow.")); return R; }
    const auto Parent = InspectBlueprintParent(BP->ParentClass->GetPathName()); Data->SetObjectField(TEXT("parent_inspection"), Parent);
    if (!Parent->GetBoolField(TEXT("success")))
    { Fail(R, TEXT("connection_parent_unsupported"), TEXT("Blueprint parent failed the existing runtime-class dependency guard.")); return R; }
    for (UClass* Class = BP->ParentClass; Class; Class = Class->GetSuperClass())
    {
        const FString PackageName = Class->GetOutermost()->GetName();
        if (Class->HasAnyClassFlags(CLASS_Native) && PackageName != TEXT("/Script/Engine") && PackageName != TEXT("/Script/CoreUObject"))
        { Fail(R, TEXT("connection_parent_unsupported"), TEXT("Native parent module is outside the audited Engine/CoreUObject scope.")); return R; }
    }
    FEndpoints Before;
    if (!Resolve(G, SourceNodePath, SourcePinName, TargetNodePath, TargetPinName, ExpectedOldTargetNodePath, ExpectedOldTargetPinName, Before))
    { Fail(R, TEXT("connection_replacement_guard_mismatch"), TEXT("Three unique loaded pins with valid identities were not found in the requested graph.")); return R; }
    auto BeforeJson = MakeShared<FJsonObject>(); Data->SetObjectField(TEXT("before"), BeforeJson);
    if (!Capture(G, BP, Before, BeforeJson) || !ExactOldEdge(Before))
    { Fail(R, TEXT("connection_replacement_guard_mismatch"), TEXT("Requires exactly one reciprocal source-to-old exec link, a distinct empty new input, and equal exec types; stale guards are rejected.")); return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP); const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    const TStrongObjectPtr<UK2Node> KeepSource(Before.SourceNode), KeepTarget(Before.TargetNode), KeepOld(Before.OldNode);
    const FGuid GraphGuid = G->GraphGuid;
    const FGuid SourceGuid = Before.SourceNode->NodeGuid, TargetGuid = Before.TargetNode->NodeGuid, OldGuid = Before.OldNode->NodeGuid;
    const FGuid SourcePinId = Before.Source->PinId, TargetPinId = Before.Target->PinId, OldPinId = Before.Old->PinId;
    const FEdGraphPinType OriginalType = Before.Source->PinType;
    const bool WasDirty = BP->GetOutermost()->IsDirty(); const EBlueprintStatus OldStatus = BP->Status;
    const int32 OldNodeCount = G->Nodes.Num();
    R->SetBoolField(TEXT("dirty_before"), WasDirty);
    // Nothing between this exact final guard and dispatch changes asset state.
    // The public API re-evaluates the schema, breaks A's old edge, makes the new
    // one and notifies affected nodes. Its internal callbacks are not atomic.
    const FPinConnectionResponse ImmediatePlan = Schema->CanCreateConnection(Before.Source, Before.Target);
    Data->SetStringField(TEXT("schema_response_name"), SchemaResponseName(ImmediatePlan.Response));
    Data->SetNumberField(TEXT("schema_response_code"), static_cast<int32>(ImmediatePlan.Response));
    if (!IsEditorGameThread() || IsPlaySessionActive() || !Idle(BP) || !ExactOldEdge(Before)
        || ImmediatePlan.Response != CONNECT_RESPONSE_BREAK_OTHERS_A)
    { Fail(R, TEXT("connection_replacement_guard_mismatch"), TEXT("Immediate source/old/new endpoint or schema guard no longer matches; connection API was not dispatched.")); return R; }
    Data->SetBoolField(TEXT("replacement_guard_verified"), true);
    R->SetStringField(TEXT("stage"), TEXT("replace_exec_link")); R->SetBoolField(TEXT("mutation_attempted"), true);
    R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
    Data->SetNumberField(TEXT("try_create_connection_count"), 1);
    const bool EngineReturn = Schema->TryCreateConnection(Before.Source, Before.Target);
    R->SetBoolField(TEXT("engine_return"), EngineReturn); R->SetStringField(TEXT("stage"), TEXT("readback"));
    // Never dereference the original pins after callbacks: they may have been
    // reconstructed. Resolve current pins again and retain partial observations.
    FEndpoints After; auto AfterJson = MakeShared<FJsonObject>(); Data->SetObjectField(TEXT("after"), AfterJson);
    bool Known = IsValid(BP) && IsValid(G) && OwnGraph(BP, G) && G->GetPathName() == GraphPath;
    if (Known)
    {
        Resolve(G, SourceNodePath, SourcePinName, TargetNodePath, TargetPinName, ExpectedOldTargetNodePath, ExpectedOldTargetPinName, After);
        Known = Capture(G, BP, After, AfterJson);
        R->SetBoolField(TEXT("dirty_after"), BP->GetOutermost()->IsDirty());
    }
    R->SetBoolField(TEXT("change_state_known"), Known);
    if (!Known)
    { Fail(R, TEXT("connection_replacement_readback_incomplete"), TEXT("The connection API returned but endpoint readback is incomplete; actual changes are unknown. Inspect retained observations without retry.")); return R; }
    const bool SameIdentities = G->GraphGuid == GraphGuid && After.SourceNode == Before.SourceNode
        && After.TargetNode == Before.TargetNode && After.OldNode == Before.OldNode
        && After.SourceNode->NodeGuid == SourceGuid && After.TargetNode->NodeGuid == TargetGuid && After.OldNode->NodeGuid == OldGuid
        && After.Source->PinId == SourcePinId && After.Target->PinId == TargetPinId && After.Old->PinId == OldPinId;
    const bool SameTypes = After.Source->PinType == OriginalType && After.Target->PinType == OriginalType && After.Old->PinType == OriginalType;
    const bool OldRemoved = !After.Source->LinkedTo.Contains(After.Old) && !After.Old->LinkedTo.Contains(After.Source);
    const bool NewExact = After.Source->LinkedTo.Num() == 1 && After.Source->LinkedTo[0] == After.Target
        && After.Target->LinkedTo.Num() == 1 && After.Target->LinkedTo[0] == After.Source && After.Old->LinkedTo.IsEmpty();
    const bool OldUnchanged = ExactOldEdge(After);
    R->SetBoolField(TEXT("changed"), !SameIdentities || !SameTypes || !OldUnchanged || OldNodeCount != G->Nodes.Num()
        || WasDirty != BP->GetOutermost()->IsDirty() || OldStatus != BP->Status);
    Data->SetBoolField(TEXT("expected_existing_link_removed"), OldRemoved);
    Data->SetNumberField(TEXT("replaced_source_link_count"), SameIdentities && OldRemoved ? 1 : 0);
    Data->SetBoolField(TEXT("endpoint_identities_preserved"), SameIdentities); Data->SetBoolField(TEXT("endpoint_types_preserved"), SameTypes);
    Data->SetBoolField(TEXT("new_link_reciprocal_and_exclusive"), NewExact);
    const bool Passed = EngineReturn && SameIdentities && SameTypes && OldRemoved && NewExact
        && G->Nodes.Num() == OldNodeCount && G->GetSchema() == Schema;
    R->SetBoolField(TEXT("postcondition_passed"), Passed); R->SetBoolField(TEXT("success"), Passed);
    if (Passed) { R->SetStringField(TEXT("stage"), TEXT("complete")); }
    else { Fail(R, EngineReturn ? TEXT("connection_replacement_postcondition_failed") : TEXT("connection_replacement_api_failed"),
        TEXT("Connection API or endpoint postconditions failed; the before/after observations describe retained changes. No rollback or retry was performed.")); }
    return R;
}
}
