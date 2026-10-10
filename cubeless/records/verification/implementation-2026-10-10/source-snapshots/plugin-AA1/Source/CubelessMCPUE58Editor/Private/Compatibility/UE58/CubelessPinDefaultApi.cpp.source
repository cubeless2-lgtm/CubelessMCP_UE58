// UE56 set_blueprint_pin_default via public schema APIs and exact readback.
#include "Compatibility/UE58/CubelessPinDefaultApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::PinDefault
{
namespace
{
namespace Pin = Function::Pin;
bool Fail(Function::FResult& R, const TCHAR* Code, const TCHAR* Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
FString Text(const TSharedRef<FJsonObject>& J)
{ FString Out; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out)); return Out; }
TSharedRef<FJsonObject> ObservePin(const UEdGraphPin* P)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("name"), P->PinName.ToString());
    J->SetStringField(TEXT("pin_id"), P->PinId.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("direction"), P->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    J->SetObjectField(TEXT("pin_type"), Pin::Describe(P->PinType));
    FString TypeText; FEdGraphPinType::StaticStruct()->ExportText(TypeText, &P->PinType, &P->PinType, nullptr, PPF_None, nullptr);
    J->SetStringField(TEXT("pin_type_text"), TypeText); J->SetStringField(TEXT("default_value"), P->DefaultValue);
    J->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString());
    J->SetStringField(TEXT("default_object"), P->DefaultObject ? P->DefaultObject->GetPathName() : FString());
    J->SetBoolField(TEXT("hidden"), P->bHidden); J->SetBoolField(TEXT("default_ignored"), P->bDefaultValueIsIgnored);
    TArray<TSharedPtr<FJsonValue>> Links;
    for (const UEdGraphPin* Other : P->LinkedTo)
    {
        auto Link = MakeShared<FJsonObject>(); Link->SetStringField(TEXT("node_path"), Other->GetOwningNode()->GetPathName());
        Link->SetStringField(TEXT("pin_id"), Other->PinId.ToString(EGuidFormats::Digits)); Link->SetStringField(TEXT("pin_name"), Other->PinName.ToString());
        Links.Add(MakeShared<FJsonValueObject>(Link));
    }
    J->SetArrayField(TEXT("linked_to"), Links); return J;
}
bool CaptureNode(const UK2Node* Node, const TSharedRef<FJsonObject>& J)
{
    if (!IsValid(Node) || !Node->NodeGuid.IsValid() || Node->Pins.Num() > 4096) { return false; }
    J->SetStringField(TEXT("object_path"), Node->GetPathName()); J->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); J->SetNumberField(TEXT("x"), Node->NodePosX); J->SetNumberField(TEXT("y"), Node->NodePosY);
    TArray<TSharedPtr<FJsonValue>> Pins; TSet<FGuid> Seen;
    for (const UEdGraphPin* P : Node->Pins)
    {
        if (!P || P->WasTrashed() || !P->PinId.IsValid() || Seen.Contains(P->PinId)) { return false; } Seen.Add(P->PinId);
        for (const UEdGraphPin* Other : P->LinkedTo)
        { if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked()) || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(P))) { return false; } }
        Pins.Add(MakeShared<FJsonValueObject>(ObservePin(P)));
    }
    J->SetArrayField(TEXT("pins"), Pins); return true;
}
}
Function::FResult Set(const FString& GraphPath, const FString& NodePath, const FString& PinName,
    const FString& ValueJson, const FString& Direction)
{
    Function::FResult R;
    if (!Pin::SafeName(GraphPath) || !Pin::SafeName(NodePath) || !Pin::SafeName(PinName)
        || (!Direction.IsEmpty() && !Direction.Equals(TEXT("input"), ESearchCase::IgnoreCase)))
    { Fail(R, TEXT("pin_default_selector_unsupported"), TEXT("Use exact bounded graph/node/pin selectors and input direction; output defaults remain pending.")); return R; }
    const auto Identities = Graph::ReadNodeIdentities(GraphPath); R.Data->SetObjectField(TEXT("graph_identity_before"), Identities);
    if (!Identities->GetBoolField(TEXT("success"))) { Fail(R, TEXT("pin_default_graph_invalid"), TEXT("Loaded graph failed the common editor identity guard.")); return R; }
    UEdGraph* G = FindObject<UEdGraph>(nullptr, *GraphPath); UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(G);
    UK2Node* Node = FindObject<UK2Node>(nullptr, *NodePath);
    if (!IsValid(BP) || !IsValid(Node) || Node->GetPathName() != NodePath || Node->GetGraph() != G || !G->Nodes.Contains(Node)
        || G->GetSchema()->GetClass() != UEdGraphSchema_K2::StaticClass() || BP->BlueprintType != BPTYPE_Normal
        || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bIsRegeneratingOnLoad || BP->bSuppressStructurallyModified)
    { Fail(R, TEXT("pin_default_scope_unsupported"), TEXT("Requires an exact loaded node on an idle normal Blueprint's standard K2 graph.")); return R; }
    if (Node->GetClass() != UK2Node_VariableSet::StaticClass() && Node->GetClass() != UK2Node_CallFunction::StaticClass()
        && Node->GetClass() != UK2Node_IfThenElse::StaticClass())
    { Fail(R, TEXT("pin_default_node_pending"), TEXT("Current default changes support exact standard VariableSet, CallFunction and Branch nodes; other node callbacks remain pending.")); return R; }
    const auto Parent = InspectBlueprintParent(BP->ParentClass ? BP->ParentClass->GetPathName() : FString());
    R.Data->SetObjectField(TEXT("parent_inspection"), Parent);
    if (!Parent->GetBoolField(TEXT("success"))) { Fail(R, TEXT("pin_default_parent_unsupported"), TEXT("Blueprint parent failed the runtime-class guard.")); return R; }
    UEdGraphPin* Selected = nullptr;
    for (UEdGraphPin* P : Node->Pins)
    {
        if (P && P->PinName.ToString() == PinName && (Direction.IsEmpty() || P->Direction == EGPD_Input))
        { if (Selected) { Fail(R, TEXT("pin_default_ambiguous"), TEXT("Pin name is ambiguous; explicitly select input direction.")); return R; } Selected = P; }
    }
    if (!Selected || Selected->Direction != EGPD_Input || Selected->WasTrashed() || Selected->bOrphanedPin
        || Selected->bHidden || Selected->bDefaultValueIsIgnored || Selected->PinName == UEdGraphSchema_K2::PN_Self || !Selected->LinkedTo.IsEmpty())
    { Fail(R, TEXT("pin_default_input_unsupported"), TEXT("Requires one visible non-orphaned unlinked input whose default is used; self/hidden/ignored/connected inputs are not modified.")); return R; }
    TSharedPtr<FJsonValue> Requested; FString RequestedText;
    if (!Pin::ParseJson(ValueJson, Requested, R) || !Pin::DefaultText(Selected->PinType, Requested, RequestedText, R)) { return R; }
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    FString CanonicalText; TObjectPtr<UObject> CanonicalObject = nullptr; FText CanonicalLocalized;
    Schema->GetPinDefaultValuesFromString(Selected->PinType, Node, RequestedText, CanonicalText, CanonicalObject, CanonicalLocalized, false);
    if (CanonicalObject || !Schema->IsPinDefaultValid(Selected, CanonicalText, CanonicalObject, CanonicalLocalized).IsEmpty())
    { Fail(R, TEXT("pin_default_value_rejected"), TEXT("The public schema rejected this typed default before modification; concrete object literals are not loaded.")); return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP); const TStrongObjectPtr<UK2Node> KeepNode(Node);
    auto Before = MakeShared<FJsonObject>(); if (!CaptureNode(Node, Before))
    { Fail(R, TEXT("pin_default_observation_incomplete"), TEXT("Node pins could not be captured within the 4096-pin bound.")); return R; }
    const bool DirtyBefore = BP->GetOutermost()->IsDirty(); const FGuid SelectedId = Selected->PinId;
    const bool LegacyFloatBefore = Selected->PinType.bSerializeAsSinglePrecisionFloat;
    FEdGraphPinType ExpectedType = Selected->PinType;
    R.Data->SetObjectField(TEXT("before"), Before); R.Data->SetObjectField(TEXT("previous"), ObservePin(Selected));
    R.Data->SetField(TEXT("requested_value"), Requested); R.Data->SetStringField(TEXT("requested_default_value"), RequestedText);
    R.Data->SetBoolField(TEXT("dirty_before"), DirtyBefore); R.Data->SetStringField(TEXT("object_path"), NodePath);
    R.Data->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    auto GraphInfo = MakeShared<FJsonObject>(); GraphInfo->SetStringField(TEXT("path"), GraphPath); GraphInfo->SetStringField(TEXT("name"), G->GetName());
    GraphInfo->SetStringField(TEXT("guid"), G->GraphGuid.ToString(EGuidFormats::Digits)); R.Data->SetObjectField(TEXT("graph"), GraphInfo);
    const bool WasMatching = Schema->DoesDefaultValueMatch(*Selected, RequestedText) && Schema->IsCurrentPinDefaultValid(Selected).IsEmpty();
    if (!WasMatching)
    {
        R.Stage = TEXT("set_pin_default"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
        BP->Modify(); G->Modify(); Node->Modify();
        // This API already emits PinDefaultValueChanged exactly once. Do not
        // repeat the legacy explicit callback or attempt an incomplete rollback.
        Schema->TrySetDefaultValue(*Selected, RequestedText, true);
        if (ExpectedType.PinCategory == UEdGraphSchema_K2::PC_Real) { ExpectedType.bSerializeAsSinglePrecisionFloat = false; }
    }
    auto After = MakeShared<FJsonObject>(); const auto AfterIds = Graph::ReadNodeIdentities(GraphPath);
    R.Data->SetObjectField(TEXT("after"), After); R.Data->SetObjectField(TEXT("graph_identity_after"), AfterIds);
    const bool Known = CaptureNode(Node, After) && AfterIds->GetBoolField(TEXT("success"));
    R.bChangeStateKnown = Known; R.bChanged = Known && (Text(Before) != Text(After) || BP->GetOutermost()->IsDirty() != DirtyBefore);
    R.Data->SetBoolField(TEXT("dirty_after"), BP->GetOutermost()->IsDirty());
    UEdGraphPin* Actual = nullptr; for (UEdGraphPin* P : Node->Pins) { if (P && P->PinId == SelectedId) { Actual = P; break; } }
    auto BeforeIdentityRows = MakeShared<FJsonObject>(); auto AfterIdentityRows = MakeShared<FJsonObject>();
    BeforeIdentityRows->SetArrayField(TEXT("nodes"), Identities->GetArrayField(TEXT("nodes")));
    if (Known) { AfterIdentityRows->SetArrayField(TEXT("nodes"), AfterIds->GetArrayField(TEXT("nodes"))); }
    const bool IdentityPreserved = Known && Node->GetGraph() == G && G->Nodes.Contains(Node)
        && Node->GetPathName() == NodePath && Before->GetStringField(TEXT("node_id")) == Node->NodeGuid.ToString(EGuidFormats::Digits)
        && Before->GetIntegerField(TEXT("x")) == Node->NodePosX && Before->GetIntegerField(TEXT("y")) == Node->NodePosY
        && Text(BeforeIdentityRows) == Text(AfterIdentityRows)
        && Identities->GetIntegerField(TEXT("total_node_count")) == AfterIds->GetIntegerField(TEXT("total_node_count"));
    R.bChanged |= Known && !IdentityPreserved;
    const bool Applied = IdentityPreserved && Actual && Actual->PinName.ToString() == PinName && Actual->Direction == EGPD_Input
        && Actual->PinType == ExpectedType && Actual->LinkedTo.IsEmpty()
        && Schema->DoesDefaultValueMatch(*Actual, RequestedText) && Schema->IsCurrentPinDefaultValid(Actual).IsEmpty();
    if (Actual && Known) { R.Data->SetObjectField(TEXT("pin"), ObservePin(Actual)); }
    bool OtherPinsPreserved = Known && Before->GetArrayField(TEXT("pins")).Num() == After->GetArrayField(TEXT("pins")).Num();
    if (OtherPinsPreserved)
    {
        for (const auto& Value : Before->GetArrayField(TEXT("pins")))
        {
            const auto Old = Value->AsObject(); if (Old->GetStringField(TEXT("pin_id")) == SelectedId.ToString(EGuidFormats::Digits)) { continue; }
            const UEdGraphPin* Current = nullptr;
            for (const UEdGraphPin* P : Node->Pins) { if (P && P->PinId.ToString(EGuidFormats::Digits) == Old->GetStringField(TEXT("pin_id"))) { Current = P; break; } }
            OtherPinsPreserved &= Current && Text(ObservePin(Current)) == Text(Old.ToSharedRef());
        }
    }
    R.Data->SetBoolField(TEXT("other_node_pins_preserved"), OtherPinsPreserved);
    R.Data->SetBoolField(TEXT("graph_identities_preserved"), IdentityPreserved);
    R.Data->SetBoolField(TEXT("legacy_float_serialization_flag_cleared"), Known && Actual && LegacyFloatBefore
        && Actual->PinType.PinCategory == UEdGraphSchema_K2::PC_Real && !Actual->PinType.bSerializeAsSinglePrecisionFloat);
    R.Data->SetBoolField(TEXT("requested_value_applied"), Applied); R.Data->SetBoolField(TEXT("already_matching"), WasMatching);
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("selected node pins and target dirty state; Python independently verifies complete graph delta"));
    if (!Applied || !OtherPinsPreserved)
    { R.Stage = TEXT("verification"); Fail(R, TEXT("pin_default_postcondition_failed"), TEXT("Requested default/readback or other node pins differ; actual partial observations retained, no rollback or retry performed.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}
