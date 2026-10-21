// Exact standard K2 Sequence creation; never embeds a Cubeless runtime reference.
#include "Compatibility/UE58/CubelessSequenceApi.h"
#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_ExecutionSequence.h"
#include "BlueprintNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::Sequence
{
namespace
{
void Fail(const TSharedRef<FJsonObject>& R, const TCHAR* Code, const FString& Message)
{ R->SetStringField(TEXT("error_code"), Code); R->SetStringField(TEXT("error"), Message); }

FString Kind(UBlueprint* BP, UEdGraph* G)
{
    if (BP->FunctionGraphs.Contains(G)) { return TEXT("function"); }
    if (BP->MacroGraphs.Contains(G)) { return TEXT("macro"); }
    if (BP->UbergraphPages.Contains(G) || BP->EventGraphs.Contains(G)) { return TEXT("event"); }
    return FString();
}
}

static TSharedRef<FJsonObject> AddWork(const FString& GraphPath, int32 X, int32 Y, int32 OutputCount,const TFunction<bool(UEdGraphNode*)>& Guard,const TFunction<bool(UEdGraphNode*)>& FreezePins)
{
    auto R = MakeShared<FJsonObject>(); auto Data = MakeShared<FJsonObject>();
    R->SetObjectField(TEXT("data"), Data); R->SetBoolField(TEXT("success"), false);
    R->SetStringField(TEXT("stage"), TEXT("preflight")); R->SetBoolField(TEXT("mutation_attempted"), false);
    R->SetBoolField(TEXT("change_state_known"), true); R->SetBoolField(TEXT("changed"), false);
    R->SetBoolField(TEXT("postcondition_passed"), false);
    R->SetBoolField(TEXT("compiled"), false); R->SetBoolField(TEXT("saved"), false);
    R->SetBoolField(TEXT("loaded_during_operation"), false);
    Data->SetBoolField(TEXT("implicit_compile_requested"), false);
    Data->SetStringField(TEXT("observation_scope"), TEXT("native loaded graph identities, new exec pins and dirty/status; Python additionally compares existing node/pin/link snapshots; arbitrary callbacks are not completely audited"));
    TStrongObjectPtr<UK2Node_ExecutionSequence> ReturnedGuardNode;
    auto Fence=[&](){if(!Guard||Guard(ReturnedGuardNode.Get()))return true;R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("postcondition_passed"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());Fail(R,TEXT("sequence_current_unknown"),TEXT("Guarded source/current failed; no next SDK or cleanup."));return false;};
    if(!Fence())return R;
    if (OutputCount < 2 || OutputCount > 64 || X < -1000000 || X > 1000000 || Y < -1000000 || Y > 1000000)
    { Fail(R, TEXT("sequence_argument_out_of_range"), TEXT("output_count must be 2..64 and coordinates -1000000..1000000; no clamp is applied.")); return R; }
    // Reuse the existing bounds, editor thread/PIE, loaded /Game K2 ownership,
    // GUID uniqueness and dirty-state read guards without copying their logic.
    const auto Before = Graph::ReadNodeIdentities(GraphPath);
    Data->SetObjectField(TEXT("before"), Before);if(!Fence())return R;
    if (!Before->GetBoolField(TEXT("success")))
    { Fail(R, TEXT("sequence_graph_preflight_failed"), TEXT("Existing graph identity preflight failed; inspect data.before.")); return R; }
    UEdGraph* G = FindObject<UEdGraph>(nullptr, *GraphPath);
    UBlueprint* BP = G ? FBlueprintEditorUtils::FindBlueprintForGraph(G) : nullptr;
    if (!IsValid(BP) || !IsValid(G) || BP->BlueprintType != BPTYPE_Normal || BP->bBeingCompiled || BP->bQueuedForCompilation
        || BP->bSuppressStructurallyModified || BP->Status == BS_BeingCreated || G->Nodes.Num() >= 2048)
    { Fail(R, TEXT("sequence_blueprint_state_unsupported"), TEXT("Requires a normal idle Blueprint and room below 2048 total graph nodes.")); return R; }
    const FString GraphKind = Kind(BP, G);
    const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(G->GetSchema());
    if (GraphKind.IsEmpty() || !Schema || Schema->GetClass() != UEdGraphSchema_K2::StaticClass()
        || !Schema->DoesGraphSupportImpureFunctions(G) || !G->GraphGuid.IsValid())
    { Fail(R, TEXT("sequence_graph_scope_unsupported"), TEXT("Requires an existing directly owned standard K2 event/function/macro graph permitting execution flow.")); return R; }
    const auto Parent = InspectBlueprintParent(BP->ParentClass ? BP->ParentClass->GetPathName() : FString());
    Data->SetObjectField(TEXT("parent_inspection"), Parent);if(!Fence())return R;
    if (!Parent->GetBoolField(TEXT("success")))
    { Fail(R, TEXT("sequence_parent_unsupported"), TEXT("Blueprint parent failed the common runtime-class dependency guard.")); return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP); const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    const int32 OldCount = G->Nodes.Num(); const EBlueprintStatus OldStatus = BP->Status;
    const bool WasDirty = BP->GetOutermost()->IsDirty();
    R->SetBoolField(TEXT("dirty_before"), WasDirty); Data->SetNumberField(TEXT("blueprint_status_before"), static_cast<int32>(OldStatus));
    R->SetStringField(TEXT("stage"), TEXT("add_sequence")); R->SetBoolField(TEXT("mutation_attempted"), true);
    R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
    BP->Modify();if(!Fence())return R;G->Modify();if(!Fence())return R;
    UK2Node_ExecutionSequence* Node=nullptr;
    if(Guard){
        Data->SetBoolField(TEXT("guarded_spawner_create_entered"),true);auto* Spawner=UBlueprintNodeSpawner::Create(UK2Node_ExecutionSequence::StaticClass());TStrongObjectPtr<UBlueprintNodeSpawner> HeldSpawner(Spawner);Data->SetBoolField(TEXT("guarded_spawner_create_returned"),true);if(!Fence()||!IsValid(Spawner))return R;
        Data->SetBoolField(TEXT("guarded_spawner_invoke_entered"),true);UEdGraphNode* Returned=Spawner->Invoke(G,{},FVector2D(X,Y));TStrongObjectPtr<UEdGraphNode> HeldReturned(Returned);Data->SetBoolField(TEXT("guarded_spawner_invoke_returned"),true);Node=Cast<UK2Node_ExecutionSequence>(Returned);if(Node)ReturnedGuardNode.Reset(Node);if(!Fence()||!Node)return R;
    }else{
        // Original False/GraphPath writer stays on its original Creator path.
        FGraphNodeCreator<UK2Node_ExecutionSequence> Creator(*G);
        Node=Creator.CreateNode(false);Node->NodePosX=X;Node->NodePosY=Y;Creator.Finalize();
    }
    bool Applied = true;
    for (int32 Index = 2; Index < OutputCount; ++Index)
    {
        UEdGraphPin* Last = Node->GetThenPinGivenIndex(Index - 1);
        if (!Last) { Applied = false; break; }
        // Both calls are exported public APIs on the MinimalAPI Sequence class.
        Node->InsertPinIntoExecutionNode(Last, EPinInsertPosition::After);if(!Fence())return R;
    }
    Node->ReconstructNode();if(!Fence())return R;if(FreezePins&&!FreezePins(Node))return R;
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);if(!Fence())return R;
    Data->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    Data->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    Data->SetStringField(TEXT("object_path"), Node->GetPathName());
    Data->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName());
    Data->SetStringField(TEXT("node_name"), Node->GetName());
    Data->SetNumberField(TEXT("x"), Node->NodePosX); Data->SetNumberField(TEXT("y"), Node->NodePosY);
    auto GraphInfo = MakeShared<FJsonObject>(); GraphInfo->SetStringField(TEXT("graph_id"), G->GraphGuid.ToString(EGuidFormats::Digits));
    GraphInfo->SetStringField(TEXT("graph_name"), G->GetName()); GraphInfo->SetStringField(TEXT("graph_type"), GraphKind);
    GraphInfo->SetStringField(TEXT("object_path"), G->GetPathName()); GraphInfo->SetStringField(TEXT("schema"), Schema->GetClass()->GetName());
    GraphInfo->SetNumberField(TEXT("node_count"), G->Nodes.Num()); Data->SetObjectField(TEXT("graph"), GraphInfo);
    TArray<TSharedPtr<FJsonValue>> Pins; int32 Inputs = 0, Outputs = 0; TSet<FGuid> PinGuids;
    bool PinsValid = true;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin) { PinsValid = false; continue; }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Pin->PinName.ToString());
        Row->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::Digits));
        Row->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        Row->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
        TArray<TSharedPtr<FJsonValue>> Links;
        for (UEdGraphPin* Other : Pin->LinkedTo)
        {
            if (!Other || !IsValid(Other->GetOwningNodeUnchecked())) { continue; }
            auto Link = MakeShared<FJsonObject>(); Link->SetStringField(TEXT("node_path"), Other->GetOwningNode()->GetPathName());
            Link->SetStringField(TEXT("pin_name"), Other->PinName.ToString()); Links.Add(MakeShared<FJsonValueObject>(Link));
        }
        Row->SetArrayField(TEXT("linked_to"), Links); Pins.Add(MakeShared<FJsonValueObject>(Row));
        PinsValid &= Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && !Pin->bOrphanedPin
            && Pin->LinkedTo.IsEmpty() && Pin->PinId.IsValid() && !PinGuids.Contains(Pin->PinId);
        if(Guard){FEdGraphPinType Expected;Expected.PinCategory=UEdGraphSchema_K2::PC_Exec;PinsValid &= Pin->PinType==Expected&&!Pin->PinType.bIsUObjectWrapper&&!Pin->PinType.bSerializeAsSinglePrecisionFloat&&!Pin->PinType.PinValueType.bTerminalIsUObjectWrapper;}
        PinGuids.Add(Pin->PinId);
        if (Pin->Direction == EGPD_Input) { ++Inputs; PinsValid &= Pin->PinName == UEdGraphSchema_K2::PN_Execute; }
        else { PinsValid &= Pin->PinName.ToString() == FString::Printf(TEXT("then_%d"), Outputs); ++Outputs; }
    }
    Data->SetArrayField(TEXT("pins"), Pins); Data->SetNumberField(TEXT("output_count"), Outputs);
    Data->SetNumberField(TEXT("requested_output_count"), OutputCount);
    const auto After = Graph::ReadNodeIdentities(GraphPath); Data->SetObjectField(TEXT("after"), After);if(!Fence())return R;
    R->SetBoolField(TEXT("dirty_after"), BP->GetOutermost()->IsDirty());
    Data->SetNumberField(TEXT("blueprint_status_after"), static_cast<int32>(BP->Status.GetValue()));
    bool Known = After->GetBoolField(TEXT("success")) && G->GetPathName() == GraphPath
        && FBlueprintEditorUtils::FindBlueprintForGraph(G) == BP;
    R->SetBoolField(TEXT("change_state_known"), Known);
    if (Known) { R->SetBoolField(TEXT("changed"), OldCount != G->Nodes.Num() || WasDirty != BP->GetOutermost()->IsDirty() || OldStatus != BP->Status); }
    bool ExistingPreserved = Known;
    if (Known)
    {
        TMap<FString, FString> CurrentIds;
        for (const auto& Current : After->GetArrayField(TEXT("nodes")))
        { CurrentIds.Add(Current->AsObject()->GetStringField(TEXT("object_path")), Current->AsObject()->GetStringField(TEXT("node_guid"))); }
        for (const auto& Old : Before->GetArrayField(TEXT("nodes")))
        {
            const FString* Current = CurrentIds.Find(Old->AsObject()->GetStringField(TEXT("object_path")));
            ExistingPreserved &= Current && *Current == Old->AsObject()->GetStringField(TEXT("node_guid"));
        }
    }
    const bool Passed = Applied && Known && ExistingPreserved && PinsValid && Inputs == 1 && Outputs == OutputCount
        && Node->GetClass() == UK2Node_ExecutionSequence::StaticClass() && G->Nodes.Contains(Node)
        && Node->NodeGuid.IsValid() && G->Nodes.Num() == OldCount + 1 && Node->NodePosX == X && Node->NodePosY == Y;
    R->SetBoolField(TEXT("postcondition_passed"), Passed); R->SetBoolField(TEXT("success"), Passed);
    R->SetStringField(TEXT("stage"), Passed ? TEXT("complete") : TEXT("verification"));
    if (!Passed) { Fail(R, TEXT("sequence_postcondition_failed"), TEXT("Sequence identity/pins or existing graph identity verification failed; inspect retained partial observations without retry.")); }
    return R;
}
TSharedRef<FJsonObject> Add(const FString& Path,int32 X,int32 Y,int32 Count){return AddWork(Path,X,Y,Count,{},{});}
TSharedRef<FJsonObject> AddGuarded(const FString& Path,int32 X,int32 Y,int32 Count,const TFunction<bool(UEdGraphNode*)>& Guard,const TFunction<bool(UEdGraphNode*)>& FreezePins){return AddWork(Path,X,Y,Count,Guard,FreezePins);}
}
