// DJ original dispatcher declaration and call/bind nodes; Editor only.
#include "Compatibility/UE58/CubelessBlueprintDispatcherApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include <cfloat>
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/UnrealType.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SparseDelegate.h"
#include "BlueprintEditorSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Internationalization/Text.h"
namespace Cubeless::UE58::BlueprintDispatcher
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }

bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }

bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }

TSharedRef<FJsonObject> ResultJson(const FResult& R)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),R.bSuccess);J->SetBoolField(TEXT("mutation_attempted"),R.bMutationAttempted);
    if(R.bChangeStateKnown) { J->SetBoolField(TEXT("changed"),R.bChanged); }else { J->SetField(TEXT("changed"),MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);J->SetBoolField(TEXT("postcondition_passed"),R.bPostconditionPassed);J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("error_code"),R.ErrorCode);J->SetStringField(TEXT("error"),R.Error);J->SetObjectField(TEXT("data"),R.Data);return J;
}

bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!Equal(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& P:X->Values) { const auto* V=Y->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}

bool Position(const FString& Json,int32 (&XY)[2],FString& Projected,FResult& R)
{
    TSharedPtr<FJsonValue> V;if(!Function::Pin::ParseJson(Json,V,R)) { return false; }
    double Values[2]={0,0};
    if(V->Type==EJson::Array&&V->AsArray().Num()>=2)
    { for(int32 I=0;I<2;++I) { V->AsArray()[I]->TryGetNumber(Values[I]); } }
    else if(V->Type==EJson::String)
    {
        FString S=V->AsString();S.TrimStartAndEndInline();S.RemoveFromStart(TEXT("["));S.RemoveFromEnd(TEXT("]"));S.RemoveFromStart(TEXT("("));S.RemoveFromEnd(TEXT(")"));
        TArray<FString> Parts;S.ParseIntoArray(Parts,TEXT(","),true);if(Parts.Num()>=2) { Values[0]=FCString::Atof(*Parts[0].TrimStartAndEnd());Values[1]=FCString::Atof(*Parts[1].TrimStartAndEnd()); }
    }
    TArray<TSharedPtr<FJsonValue>> Original,FloatRows,IntRows;
    for(int32 I=0;I<2;++I)
    {
        if(!FMath::IsFinite(Values[I])||FMath::Abs(Values[I])>FLT_MAX) { return Fail(R,TEXT("value_position_invalid"),TEXT("Finite float32 position required before graph writer.")); }
        const float F=static_cast<float>(Values[I]);const double D=static_cast<double>(F);
        if(!FMath::IsFinite(F)||D<static_cast<double>(MIN_int32)||D>static_cast<double>(MAX_int32)) { return Fail(R,TEXT("value_position_invalid"),TEXT("Original float32 position exceeds safe int32 assignment budget.")); }
        XY[I]=static_cast<int32>(F);Original.Add(MakeShared<FJsonValueNumber>(Values[I]));FloatRows.Add(MakeShared<FJsonValueNumber>(D));IntRows.Add(MakeShared<FJsonValueNumber>(XY[I]));
    }
    R.Data->SetField(TEXT("requested_position"),V);R.Data->SetArrayField(TEXT("position_numeric_prefix"),Original);R.Data->SetArrayField(TEXT("position_float32"),FloatRows);R.Data->SetArrayField(TEXT("position_int32"),IntRows);
    Projected=FString::Printf(TEXT("[%d,%d]"),XY[0],XY[1]);return true;
}

bool ExistingNodesPreserved(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,int32 Delta)
{
    const auto& A=Before->GetArrayField(TEXT("nodes"));const auto& B=After->GetArrayField(TEXT("nodes"));
    if(B.Num()!=A.Num()+Delta) { return false; }for(int32 I=0;I<A.Num();++I) { if(!Equal(A[I],B[I])) { return false; } }return true;
}

bool GraphRowsPreserved(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,const FString& Selected,int32 Delta)
{
    const auto& A=Before->GetArrayField(TEXT("graphs"));const auto& B=After->GetArrayField(TEXT("graphs"));if(A.Num()!=B.Num()) { return false; }int32 SelectedCount=0;
    for(int32 I=0;I<A.Num();++I)
    { const auto Expected=MakeShared<FJsonObject>(*A[I]->AsObject());if(Expected->GetStringField(TEXT("object_path")).Equals(Selected,ESearchCase::CaseSensitive))
      { ++SelectedCount;Expected->SetNumberField(TEXT("node_count"),Expected->GetNumberField(TEXT("node_count"))+Delta); }
      if(!Equal(MakeShared<FJsonValueObject>(Expected),B[I])) { return false; } }return SelectedCount==1;
}

bool FreshPins(UEdGraphNode* Node)
{
    TSet<FGuid> Ids;
    for(const auto* P:Node->Pins)
    { if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return false; }Ids.Add(P->PinId); }
    return true;
}

TSharedRef<FJsonObject> Source(const UStruct* Struct,FResult& R)
{
    Count(R,TEXT("source_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Struct->GetPathName());J->SetStringField(TEXT("class"),Struct->GetClass()->GetPathName());J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(Struct->GetFlags())));J->SetStringField(TEXT("super"),GetPathNameSafe(Struct->GetSuperStruct()));J->SetStringField(TEXT("package"),Struct->GetOutermost()->GetName());J->SetStringField(TEXT("package_flags"),LexToString(static_cast<uint32>(Struct->GetOutermost()->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Struct->GetOutermost()->IsDirty());
    if(const UClass* Class=Cast<UClass>(Struct)) { J->SetStringField(TEXT("class_flags"),LexToString(static_cast<uint32>(Class->GetClassFlags())));J->SetStringField(TEXT("generated_by"),GetPathNameSafe(Class->ClassGeneratedBy)); }
    if(const UScriptStruct* Script=Cast<UScriptStruct>(Struct)) { J->SetStringField(TEXT("struct_flags"),LexToString(static_cast<uint32>(Script->StructFlags))); }
    return J;
}

void Place(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const int32 (&XY)[2],bool Reconstruct,FResult& R)
{
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());Node->NodePosX=XY[0];Node->NodePosY=XY[1];
    Count(R,TEXT("graph_add_node_calls"));G->AddNode(Node,true);Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();
    if(Reconstruct) { Count(R,TEXT("reconstruct_calls"));Node->ReconstructNode(); }R.Data->SetStringField(TEXT("created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));
}

void Mark(UBlueprint* BP,FResult& R)
{
    R.Stage=TEXT("mark_modified");Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Count(R,TEXT("mark_blueprint_modified_returns"));
}

void LegacyNodeFields(UEdGraphNode* Node,FResult& R)
{
    Count(R,TEXT("legacy_node_metadata_observation_calls"));R.Data->SetStringField(TEXT("name"),Node->GetName());R.Data->SetStringField(TEXT("class"),Node->GetClass()->GetName());
    R.Data->SetStringField(TEXT("title"),Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());R.Data->SetStringField(TEXT("node_comment"),Node->NodeComment);
    R.Data->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);
}

bool After(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const FGuid& Guid,const TSharedPtr<FJsonObject>& Before,FResult& R)
{
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(BP->GetPathName(),TEXT(""),Guid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(S));
    if(!S.bSuccess||!S.bChangeStateKnown||!S.bPostconditionPassed)
    { R.bChangeStateKnown=false;return Fail(R,TEXT("dispatcher_after_incomplete"),TEXT("Current graph observation did not close; created node/notification prefix retained with no retry or new cleanup.")); }
    const auto A=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),A);R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true;
    const auto* Row=A->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(Row) { R.Data->SetObjectField(TEXT("node"),(*Row)->AsObject());for(const auto& P:(*Row)->AsObject()->Values) { R.Data->SetField(P.Key,P.Value); }LegacyNodeFields(Node,R); }
    const bool Old=ExistingNodesPreserved(Before,A,1),Rows=GraphRowsPreserved(Before,A,G->GetPathName(),1);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);R.Data->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());Legacy->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    return (Old&&Rows&&G->GraphGuid==Guid&&Row)||Fail(R,TEXT("dispatcher_graph_postcondition_failed"),TEXT("Selected node or old node/pin/graph preservation differs; actual current state retained, no rollback claimed."));
}
bool OwnerScope(UBlueprint* BP,UEdGraph* G,FResult& R)
{
    if(!Loaded(BP)||(G&&!Loaded(G))||!Loaded(BP->ParentClass)||(G&&FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP)
        ||(BP->BlueprintType!=BPTYPE_Normal&&BP->BlueprintType!=BPTYPE_FunctionLibrary&&BP->BlueprintType!=BPTYPE_MacroLibrary)
        ||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad
        ||BP->Status==BS_BeingCreated||(G&&(!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>=2048)))
    { return Fail(R,TEXT("dispatcher_owner_scope_pending"),TEXT("Idle owned editable K2 graph and ordinary native runtime Blueprint owner required; broader owners remain tracked.")); }
    int32 Depth=0;
    for(UClass* C=BP->ParentClass;C;C=C->GetSuperClass())
    {
        const FString Package=C->GetOutermost()->GetName();
        if(++Depth>64||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->IsEditorOnly()
            ||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)
            ||(!Package.Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)&&!Package.Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)))
        { return Fail(R,TEXT("dispatcher_parent_scope_pending"),TEXT("Finite native Engine/CoreUObject parent hierarchy required; not restricted to direct Actor. Generated/custom/editor/plugin parents remain valid residuals.")); }
    }
    Count(R,TEXT("external_scope_observation_calls"));TArray<TSharedPtr<FJsonValue>> External;
    for(TObjectIterator<UBlueprint> It;It;++It)
    { if(*It!=BP&&It->ParentClass&&BP->GeneratedClass&&It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.Add(MakeShared<FJsonValueString>(It->GetPathName())); } }
    if(BP->GeneratedClass)
    { TArray<UObject*> Instances;GetObjectsOfClass(BP->GeneratedClass,Instances,true,RF_NoFlags);for(UObject* O:Instances) { if(IsValid(O)&&O->GetOutermost()!=BP->GetOutermost()) { External.Add(MakeShared<FJsonValueString>(O->GetPathName())); } } }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"),External);
    return External.IsEmpty()||Fail(R,TEXT("dispatcher_external_instances_pending"),TEXT("Loaded external instances/derived Blueprint callback effects are outside this observed scope."));
}

namespace Pin=Function::Pin;
constexpr int32 MaxGraphs=256,MaxNodes=2048;
FString GraphType(UBlueprint* BP, UEdGraph* G)
{
    if (BP->FunctionGraphs.Contains(G)) { return TEXT("function"); }
    if (BP->MacroGraphs.Contains(G)) { return TEXT("macro"); }
    if (BP->UbergraphPages.Contains(G) || BP->EventGraphs.Contains(G)) { return TEXT("event"); }
    if (BP->DelegateSignatureGraphs.Contains(G)) { return TEXT("delegate"); } return TEXT("unknown");
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
    if (const UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node))
    {
        J->SetStringField(TEXT("custom_event_name"), Event->CustomFunctionName.ToString());
        J->SetBoolField(TEXT("call_in_editor"), Event->bCallInEditor); J->SetBoolField(TEXT("override_function"), Event->bOverrideFunction);
        J->SetBoolField(TEXT("deprecated"), Event->bIsDeprecated); J->SetNumberField(TEXT("function_flags"), Event->FunctionFlags);
        J->SetNumberField(TEXT("user_pin_count"), Event->UserDefinedPins.Num());
    }
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

void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("blueprint_load_calls"),TEXT("blueprint_load_returns"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("external_scope_observation_calls"),TEXT("source_observation_calls"),TEXT("legacy_node_metadata_observation_calls"),TEXT("blueprint_capture_calls"),TEXT("graph_capture_calls"),TEXT("variable_observation_calls"),TEXT("type_object_lookup_calls"),TEXT("type_object_lookup_returns"),TEXT("member_add_calls"),TEXT("member_add_returns"),TEXT("signature_graph_create_calls"),TEXT("signature_graph_create_returns"),TEXT("default_nodes_calls"),TEXT("function_terminators_calls"),TEXT("function_flags_calls"),TEXT("entry_editable_calls"),TEXT("signature_graph_add_calls"),TEXT("user_pin_type_parse_calls"),TEXT("user_pin_can_create_calls"),TEXT("user_pin_create_calls"),TEXT("user_pin_create_returns"),TEXT("entry_reconstruct_calls"),TEXT("category_write_calls"),TEXT("category_helper_calls"),TEXT("tooltip_descriptor_write_calls"),TEXT("tooltip_helper_calls"),TEXT("friendly_name_write_calls"),TEXT("mark_structural_calls"),TEXT("mark_structural_returns"),TEXT("remove_graph_calls"),TEXT("remove_graph_returns"),TEXT("remove_member_variable_calls"),TEXT("remove_member_variable_returns"),TEXT("property_lookup_calls"),TEXT("property_lookup_returns"),TEXT("property_observation_calls"),TEXT("signature_observation_calls"),TEXT("node_class_lookup_calls"),TEXT("node_class_lookup_returns"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("set_from_property_calls"),TEXT("set_from_property_returns"),TEXT("compatibility_calls"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("explicit_modify_calls"),TEXT("explicit_graph_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("destroy_node_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetBoolField(TEXT("cleanup_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);
    R.Data->SetStringField(TEXT("counter_scope"),TEXT("Direct calls/returns only. AddMemberVariable, metadata/category and default cleanup helpers modify/mark/regenerate skeleton/cache/CDO/children and callbacks internally. Outertransaction1 is not SDKtotaltransaction1; explicit fullcompile/save0 is not totalcompilation0."));
    R.Data->SetStringField(TEXT("preservation_scope"),TEXT("Owned graph rows/node/pin/default/link snapshots; declaration additionally observes ordered FBPVariableDescription reflected text and new signature/variable metadata; node operations observe current selected property/signature/specialized pins. Arbitrary Blueprint CDO/SCS/world/editorcache/childcallback/runtime state is not fully observed. Declaration cleanup is best effort, never atomic restoration."));
}
bool DispatcherName(const FString& Input,FString& Out,FResult& R)
{
    Out=Input.TrimStartAndEnd();bool Bounded=!Out.IsEmpty()&&Out.Len()<NAME_SIZE;for(int32 I=0;I<Out.Len();++I) { Bounded&=Out[I]!=0; }
    return (Bounded&&!FName(*Out).IsNone())||Fail(R,TEXT("dispatcher_name_invalid"),TEXT("Original trimmed dispatcher FName must be nonempty/nonNone and fit NAME_SIZE without interior NUL."));
}
UBlueprint* LoadBlueprint(const FString& Path,FResult& R)
{
    if(!Function::Pin::SafeName(Path)||!Path.StartsWith(TEXT("/Game/"))) { Fail(R,TEXT("dispatcher_blueprint_path_invalid"),TEXT("Bounded exact /Game Blueprint asset/package path required; original simple-name/plugin variants remain tracked."));return nullptr; }
    const FString ObjectPath=Path.Contains(TEXT("."))?Path:Path+TEXT(".")+FPackageName::GetShortName(Path);if(!Function::Pin::SafeName(ObjectPath)) { Fail(R,TEXT("dispatcher_blueprint_path_invalid"),TEXT("Completed object path exceeds finite bound."));return nullptr; }
    const bool WasLoaded=FindObject<UBlueprint>(nullptr,*ObjectPath)!=nullptr;R.Data->SetBoolField(TEXT("loaded_during_operation"),!WasLoaded);R.Data->SetStringField(TEXT("load_scope"),TEXT("PostLoad effects precede snapshot and remain unmeasured; no forced loader/module finish."));Count(R,TEXT("blueprint_load_calls"));auto* BP=LoadObject<UBlueprint>(nullptr,*ObjectPath);Count(R,TEXT("blueprint_load_returns"));
    if(!Loaded(BP)) { R.bChangeStateKnown=WasLoaded;Fail(R,TEXT("dispatcher_blueprint_unavailable"),TEXT("Blueprint load is unavailable/incomplete; load receipt retained."));return nullptr; }R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());return BP;
}
TSharedRef<FJsonObject> Variables(UBlueprint* BP,FResult& R)
{
    Count(R,TEXT("variable_observation_calls"));auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;bool Complete=BP->NewVariables.Num()<=256;
    if(Complete) { for(const auto& V:BP->NewVariables) { FString Text;FBPVariableDescription::StaticStruct()->ExportText(Text,&V,nullptr,BP,PPF_None,nullptr);if(Text.Len()>32768) { Complete=false;break; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),V.VarName.ToString());Q->SetStringField(TEXT("guid"),V.VarGuid.ToString());Q->SetStringField(TEXT("reflected_descriptor"),Text);Rows.Add(MakeShared<FJsonValueObject>(Q)); } }J->SetArrayField(TEXT("ordered_descriptors"),Rows);J->SetBoolField(TEXT("complete"),Complete);return J;
}
bool BlueprintState(UBlueprint* BP,TSharedRef<FJsonObject>& Out,FResult& R)
{
    Count(R,TEXT("blueprint_capture_calls"));Out=MakeShared<FJsonObject>();const auto V=Variables(BP,R);Out->SetObjectField(TEXT("variables"),V);if(!V->GetBoolField(TEXT("complete"))) { return Fail(R,TEXT("dispatcher_variable_observation_incomplete"),TEXT("Descriptor snapshot exceeds finite256/32768 bounds.")); }
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);if(Graphs.Num()>64) { return Fail(R,TEXT("dispatcher_graph_budget"),TEXT("Full selected owned graph preservation scope is64 graphs.")); }
    TArray<TSharedPtr<FJsonValue>> Rows;int32 Nodes=0;for(UEdGraph* G:Graphs)
    { if(!Loaded(G)||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||(Nodes+=G->Nodes.Num())>4096) { return Fail(R,TEXT("dispatcher_graph_observation_incomplete"),TEXT("Loaded exact K2 graphs within total4096-node scope required.")); }auto Q=MakeShared<FJsonObject>();Count(R,TEXT("graph_capture_calls"));if(!Capture(BP,G,Q,R)) { Out->SetArrayField(TEXT("graph_states"),Rows);return false; }Rows.Add(MakeShared<FJsonValueObject>(Q)); }Out->SetArrayField(TEXT("graph_states"),Rows);Out->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());Out->SetNumberField(TEXT("blueprint_status"),BP->Status.GetValue());return true;
}
TSharedRef<FJsonObject> StableGraphs(const TSharedRef<FJsonObject>& State,const FString& Excluded)
{
    auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& V:State->GetArrayField(TEXT("graph_states")))
    { const auto O=V->AsObject();if(O->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")).Equals(Excluded,ESearchCase::CaseSensitive)) { continue; }auto Q=MakeShared<FJsonObject>();Q->SetObjectField(TEXT("graph"),O->GetObjectField(TEXT("graph")));Q->SetArrayField(TEXT("nodes"),O->GetArrayField(TEXT("nodes")));Rows.Add(MakeShared<FJsonValueObject>(Q)); }J->SetArrayField(TEXT("graphs"),Rows);return J;
}
FBPVariableDescription* Variable(UBlueprint* BP,FName Name)
{ for(auto& V:BP->NewVariables) { if(V.VarName==Name) { return &V; } }return nullptr; }
bool OldVariables(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,int32 Delta)
{
    const auto& A=Before->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("ordered_descriptors"));const auto& B=After->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("ordered_descriptors"));if(B.Num()!=A.Num()+Delta) { return false; }for(int32 I=0;I<A.Num();++I) { if(!Equal(A[I],B[I])) { return false; } }return true;
}
bool ReferenceGuard(const FString& S)
{
    FString N=S.TrimStartAndEnd();N.TrimQuotesInline();N.ReplaceInline(TEXT("\\"),TEXT("/"));N=N.ToLower();for(const TCHAR* Token:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor")}) { if(N.Contains(Token)) { return false; } }return true;
}
bool InputType(const FString& Requested,const TSharedPtr<FJsonObject>& J,FEdGraphPinType& T,FResult& R)
{
    Count(R,TEXT("user_pin_type_parse_calls"));FString N=Requested.ToLower().TrimStartAndEnd();N.ReplaceInline(TEXT(" "),TEXT(""));N.ReplaceInline(TEXT("_"),TEXT(""));N.ReplaceInline(TEXT("-"),TEXT(""));T=FEdGraphPinType();
    if(N==TEXT("bool")||N==TEXT("boolean")) { T.PinCategory=UEdGraphSchema_K2::PC_Boolean; }
    else if(N==TEXT("int")||N==TEXT("integer")) { T.PinCategory=UEdGraphSchema_K2::PC_Int; }
    else if(N==TEXT("int64")||N==TEXT("integer64")) { T.PinCategory=UEdGraphSchema_K2::PC_Int64; }
    else if(N==TEXT("float")||N==TEXT("single")||N==TEXT("double")||N==TEXT("real")) { T.PinCategory=UEdGraphSchema_K2::PC_Real;T.PinSubCategory=N==TEXT("float")||N==TEXT("single")?UEdGraphSchema_K2::PC_Float:UEdGraphSchema_K2::PC_Double; }
    else if(N==TEXT("string")) { T.PinCategory=UEdGraphSchema_K2::PC_String; }else if(N==TEXT("name")) { T.PinCategory=UEdGraphSchema_K2::PC_Name; }else if(N==TEXT("text")) { T.PinCategory=UEdGraphSchema_K2::PC_Text; }
    else if(N==TEXT("vector")||N==TEXT("rotator")||N==TEXT("transform")) { T.PinCategory=UEdGraphSchema_K2::PC_Struct;T.PinSubCategoryObject=N==TEXT("vector")?TBaseStructure<FVector>::Get():N==TEXT("rotator")?TBaseStructure<FRotator>::Get():TBaseStructure<FTransform>::Get(); }
    else if(N==TEXT("object")||N==TEXT("class")||N==TEXT("struct")||N==TEXT("scriptstruct")||N==TEXT("enum")||N==TEXT("byteenum"))
    {
        FString Path;J->TryGetStringField(TEXT("type_object"),Path);if(N==TEXT("enum")||N==TEXT("byteenum")) { if(Path.IsEmpty()) { J->TryGetStringField(TEXT("enum_type"),Path); }if(Path.IsEmpty()) { J->TryGetStringField(TEXT("enum_path"),Path); } }
        if(Path.IsEmpty()&&(N==TEXT("object")||N==TEXT("class"))) { Path=TEXT("/Script/CoreUObject.Object"); }
        if(!ReferenceGuard(Path)) { return Fail(R,TEXT("dispatcher_type_dependency_reference"),TEXT("Original type_object/enum reference guard refuses MCP-only asset/class dependencies.")); }
        bool PathBounded=!Path.IsEmpty()&&Path.Len()<=1024;for(int32 K=0;K<Path.Len();++K) { PathBounded&=Path[K]!=0; }if(!PathBounded) { return Fail(R,TEXT("dispatcher_type_object_unavailable"),TEXT("Required bounded type_object missing.")); }
        Count(R,TEXT("type_object_lookup_calls"));UObject* O=nullptr;
        if(N==TEXT("object")||N==TEXT("class")) { O=FindObject<UClass>(nullptr,*Path);if(!O&&!Path.StartsWith(TEXT("/"))) { O=FindFirstObject<UClass>(*Path);if(!O) { O=FindObject<UClass>(nullptr,*(TEXT("/Script/Engine.")+Path)); } } }
        else if(N==TEXT("enum")||N==TEXT("byteenum")) { O=FindObject<UEnum>(nullptr,*Path); }else { O=FindObject<UScriptStruct>(nullptr,*Path); }Count(R,TEXT("type_object_lookup_returns"));
        if(!Loaded(O)) { return Fail(R,TEXT("dispatcher_type_object_loading_pending"),TEXT("This slice admits already-loaded exact struct/enum/class; original loading/search aliases remain valid pending variants. Cleanup preserves actual constructed declaration prefix.")); }
        T.PinCategory=N==TEXT("class")?UEdGraphSchema_K2::PC_Class:N==TEXT("object")?UEdGraphSchema_K2::PC_Object:(N==TEXT("enum")||N==TEXT("byteenum"))?UEdGraphSchema_K2::PC_Byte:UEdGraphSchema_K2::PC_Struct;T.PinSubCategoryObject=O;
    }
    else { return Fail(R,TEXT("dispatcher_input_type_invalid"),TEXT("Unsupported original type descriptor.")); }
    bool Array=false;J->TryGetBoolField(TEXT("is_array"),Array);if(Array) { T.ContainerType=EPinContainerType::Array; }return true;
}
FMulticastDelegateProperty* Property(UBlueprint* BP,FName Name,FResult& R)
{
    Count(R,TEXT("property_lookup_calls"));FMulticastDelegateProperty* P=nullptr;for(UClass* C:{BP->SkeletonGeneratedClass.Get(),BP->GeneratedClass.Get(),BP->ParentClass.Get()}) { if(C&&(P=FindFProperty<FMulticastDelegateProperty>(C,Name))) { break; } }Count(R,TEXT("property_lookup_returns"));return P;
}
TSharedRef<FJsonObject> PropertyJson(const FMulticastDelegateProperty* P,FResult& R)
{
    Count(R,TEXT("property_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("present"),P!=nullptr);if(P) { J->SetStringField(TEXT("name"),P->GetName());J->SetStringField(TEXT("field_class"),P->GetClass()->GetName());J->SetStringField(TEXT("owner_class"),GetPathNameSafe(P->GetOwnerClass()));J->SetStringField(TEXT("flags_uint64"),LexToString(static_cast<uint64>(P->GetPropertyFlags())));J->SetStringField(TEXT("signature_function"),GetPathNameSafe(P->SignatureFunction));J->SetBoolField(TEXT("blueprint_callable"),P->HasAllPropertyFlags(CPF_BlueprintCallable));J->SetBoolField(TEXT("blueprint_assignable"),P->HasAllPropertyFlags(CPF_BlueprintAssignable)); }return J;
}
bool Graph(const FString& Path,const FString& Name,const FString& Id,const FString& Type,bool Create,UBlueprint*& BP,UEdGraph*& G,TSharedPtr<FJsonObject>& Before,FResult& R)
{
    Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(Path,Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));R.bMutationAttempted=S.bMutationAttempted;R.bChanged=S.bChanged;R.bChangeStateKnown=S.bChangeStateKnown;if(!S.bSuccess||!S.bPostconditionPassed) { return Fail(R,S.ErrorCode.IsEmpty()?TEXT("dispatcher_graph_unverified"):*S.ErrorCode,S.Error); }
    BP=FindObject<UBlueprint>(nullptr,*S.Data->GetStringField(TEXT("blueprint")));G=FindObject<UEdGraph>(nullptr,*S.Data->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")));Before=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetStringField(TEXT("blueprint"),S.Data->GetStringField(TEXT("blueprint")));return OwnerScope(BP,G,R);
}

FString GetPinDirectionName(const UEdGraphPin* Pin)
{
    return Pin && Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("input");
}

TSharedPtr<FJsonObject> PinTypeToJson(const FEdGraphPinType& PinType)
{
    TSharedPtr<FJsonObject> TypeObject = MakeShared<FJsonObject>();
    TypeObject->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
    TypeObject->SetStringField(TEXT("subcategory"), PinType.PinSubCategory.ToString());
    TypeObject->SetBoolField(TEXT("is_array"), PinType.IsArray());
    if (PinType.PinSubCategoryObject.IsValid())
    {
        TypeObject->SetStringField(TEXT("subcategory_object"), PinType.PinSubCategoryObject->GetPathName());
    }
    return TypeObject;
}

TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    PinObject->SetStringField(TEXT("name"), Pin ? Pin->PinName.ToString() : FString());
    PinObject->SetStringField(TEXT("direction"), GetPinDirectionName(Pin));

    if (Pin)
    {
        PinObject->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
        PinObject->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
        PinObject->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());
        PinObject->SetObjectField(TEXT("pin_type"), PinTypeToJson(Pin->PinType));
        PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);

        if (Pin->PinType.PinSubCategoryObject.IsValid())
        {
            PinObject->SetStringField(TEXT("subcategory_object"), Pin->PinType.PinSubCategoryObject->GetPathName());
        }
        if (Pin->DefaultObject)
        {
            PinObject->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
        }

        TArray<TSharedPtr<FJsonValue>> LinkedPins;
        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            if (!LinkedPin || !LinkedPin->GetOwningNode())
            {
                continue;
            }

            TSharedPtr<FJsonObject> LinkedPinObject = MakeShared<FJsonObject>();
            LinkedPinObject->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
            LinkedPinObject->SetStringField(TEXT("node_name"), LinkedPin->GetOwningNode()->GetName());
            LinkedPinObject->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
            LinkedPins.Add(MakeShared<FJsonValueObject>(LinkedPinObject));
        }
        PinObject->SetArrayField(TEXT("linked_to"), LinkedPins);
    }

    return PinObject;
}

TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!Node)
    {
        return NodeObject;
    }

    NodeObject->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
    NodeObject->SetStringField(TEXT("name"), Node->GetName());
    NodeObject->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    NodeObject->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
    NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);
    NodeObject->SetStringField(TEXT("node_comment"), Node->NodeComment);
    NodeObject->SetBoolField(TEXT("comment_bubble_visible"), Node->bCommentBubbleVisible);
    NodeObject->SetBoolField(TEXT("comment_bubble_pinned"), Node->bCommentBubblePinned);

    if (bIncludePins)
    {
        TArray<TSharedPtr<FJsonValue>> Pins;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            Pins.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
        }
        NodeObject->SetArrayField(TEXT("pins"), Pins);
    }

    return NodeObject;
}

bool PinsMatchType(const FEdGraphPinType& A,const FEdGraphPinType& B)
{ return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper; }
TSharedRef<FJsonObject> Signature(UFunction* F,FResult& R)
{
    Count(R,TEXT("signature_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("loaded"),Loaded(F));J->SetStringField(TEXT("path"),GetPathNameSafe(F));bool Complete=Loaded(F);TArray<TSharedPtr<FJsonValue>> Rows;
    if(Complete) { J->SetStringField(TEXT("flags_uint32"),LexToString(static_cast<uint32>(F->FunctionFlags)));for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It;++It)
    { if(!It->HasAnyPropertyFlags(CPF_Parm)) { continue; }if(Rows.Num()>=256) { Complete=false;break; }FEdGraphPinType T;const bool Converted=GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(*It,T);auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),It->GetName());Q->SetStringField(TEXT("field_class"),It->GetClass()->GetName());Q->SetStringField(TEXT("flags_uint64"),LexToString(static_cast<uint64>(It->GetPropertyFlags())));Q->SetBoolField(TEXT("schema_converted"),Converted);if(Converted) { auto D=Function::Pin::Describe(T);D->SetBoolField(TEXT("is_const"),T.bIsConst);Q->SetObjectField(TEXT("pin_type"),D); }Rows.Add(MakeShared<FJsonValueObject>(Q));Complete&=Converted; } }
    J->SetArrayField(TEXT("parameters"),Rows);J->SetBoolField(TEXT("complete"),Complete);return J;
}
void Structural(UBlueprint* BP,FResult& R)
{ Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Count(R,TEXT("mark_structural_returns")); }
// QK concrete Widget dispatcher scope. Existing Actor declaration/node bodies remain separate.
struct FWidgetDispatcherWork
{
    FResult& R;TStrongObjectPtr<UWidgetBlueprint> BP;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UWidgetTree> Tree;TStrongObjectPtr<UCanvasPanel> Root;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<UClass> Parent,Gen,Skel;TStrongObjectPtr<UObject> GenCDO,SkelCDO;
    TStrongObjectPtr<UBlueprintEditorSettings> Policy;TArray<TStrongObjectPtr<UObject>> Holds;
    FString Path,GenPath,SkelPath,TreePath,RootPath;UClass* GenMeta=nullptr;UClass* SkelMeta=nullptr;FName Dispatcher;
    bool Stopped=false,Rebinding=false,ClassesReady=false,Writer=false,MemberAdded=false,GraphAdded=false;UEdGraph* NewGraph=nullptr;UEdGraphNode* NewNode=nullptr;
    bool NodeMember=false,SelectedPlanReady=false,MutableCategory=false;FBPVariableDescription SelectedPlan;FMemberReference NewReference;bool NewReferenceReady=false;UClass* NewNodeClass=nullptr;TArray<TObjectPtr<UEdGraph>> Uber,Functions,Macros,Delegates;TMap<FGuid,FSimpleMemberReference> CompilerEventReferences;TMap<FName,FGuid> WidgetGuids;TArray<FBPVariableDescription> Variables;
    struct FPin{UEdGraphPin* Address;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;FString Raw,Auto,Display;FText Text;uint32 TextFlags;UObject* Default;UObject* TypeObject;TArray<FGuid> Links;};
    struct FUserPin{FName Name;FEdGraphPinType Type;EEdGraphPinDirection Direction;FString Default;};
    struct FDelegateField{FProperty* P=nullptr;FName Name;FFieldClass* Class=nullptr;uint64 Flags=0;int32 Offset=0,Size=0,Dim=0;};
    struct FNode{FName DelegateName;FGuid DelegateGuid;FStructProperty* DelegateGuidField=nullptr;int32 DelegateGuidOffset=0;uint64 DelegateGuidFlags=0;TStrongObjectPtr<UScriptStruct> DelegateGuidStruct;bool DelegateObserved=false;TStrongObjectPtr<UClass> DelegateScope,DelegateSignatureOwner;TStrongObjectPtr<UFunction> DelegateSignature;FName DelegateSignatureName;EFunctionFlags DelegateSignatureFlags=FUNC_None;TArray<FDelegateField> DelegateFields;FName CustomName;uint32 CustomFlags=0;bool CustomOverride=false,CustomDeprecated=false,CustomEditor=false,CustomEditable=false;FString CustomDeprecation;TArray<FUserPin> UserPins;int32 EntryFlags=0;bool EntryEditable=false;FMemberReference Reference;int32 ReferenceKind=0;TStrongObjectPtr<UEdGraphNode> Hold;UClass* Class;FGuid Id;FString Path,Comment;int32 X,Y;TArray<FPin> Pins;};
    struct FGraph{TStrongObjectPtr<UEdGraph> Hold;FGuid Id;FString Path;bool Editable;TArray<FNode> Nodes;};TArray<FGraph> Graphs,Candidates;
    TSharedPtr<FJsonObject> SelectedDescriptor;FFieldClass* PropertyClass=nullptr;uint64 PropertyFlags=0;int32 PropertyOffset=0,PropertySize=0,PropertyDim=0;FMulticastDelegateProperty* Property=nullptr;TStrongObjectPtr<UClass> PropertyOwner;TStrongObjectPtr<UFunction> Signature;
    struct FField{FProperty* P;FFieldClass* Class;FName Name;uint64 Flags;int32 Offset,Size,Dim;};TArray<FField> Fields;UObject* SignatureOuter=nullptr;UClass* SignatureMeta=nullptr;FString SignaturePath;EFunctionFlags SignatureFlags=FUNC_None;
    ESlateVisibility Visibility;bool Enabled=false,IsVariable=false;float Opacity=1;FWidgetTransform Transform;FVector2D Pivot;EWidgetClipping Clipping;
    // UU: the authored tree is physical authority; compiler duplicates are logical caches.
    struct FTreeObject
    {
        TStrongObjectPtr<UObject> Object,Outer;TStrongObjectPtr<UClass> Class;
        FString Relative,Path,Values,RuntimeValues;
    };
    struct FTreeState{TArray<FTreeObject> Objects;FString Shape,Runtime;};
    FTreeState AuthoredTree;
    struct FTreeField{FProperty* Property;FFieldClass* Class;UStruct* Owner;FName Name;uint64 Flags;int32 Offset,Size,Dim;bool Accessor;UStruct* Type;uint32 BoolLayout;FProperty* Inner;FProperty* Value;FProperty* RootProperty;};
    TArray<FTreeField> TreeFields;
    bool TreeReady=false;
    static void TreeToken(FString& Out,const FString& Value){Out+=FString::FromInt(Value.Len())+TEXT(":")+Value;}
    bool TreeNative(UClass* C)
    {
        return Loaded(C)&&C->HasAnyClassFlags(CLASS_Native)&&C->ClassGeneratedBy.IsResolved()&&!C->ClassGeneratedBy.Get()
            &&Loaded(C->GetOuter())&&C->GetOuter()->GetPathName()==TEXT("/Script/UMG")&&!C->HasAnyClassFlags(CLASS_NewerVersionExists|CLASS_Deprecated);
    }
    bool TreeField(FProperty* P)
    {
        if(!P)return false;auto* K=P->GetClass();
        const bool Plain=K==FBoolProperty::StaticClass()||K==FByteProperty::StaticClass()||K==FInt8Property::StaticClass()||K==FInt16Property::StaticClass()||K==FUInt16Property::StaticClass()||K==FIntProperty::StaticClass()||K==FUInt32Property::StaticClass()||K==FInt64Property::StaticClass()||K==FUInt64Property::StaticClass()||K==FFloatProperty::StaticClass()||K==FDoubleProperty::StaticClass()||K==FNameProperty::StaticClass()||K==FStrProperty::StaticClass()||K==FTextProperty::StaticClass()||K==FObjectProperty::StaticClass()||K==FSoftObjectProperty::StaticClass()||K==FStructProperty::StaticClass()||K==FEnumProperty::StaticClass()||K==FArrayProperty::StaticClass()||K==FSetProperty::StaticClass()||K==FMapProperty::StaticClass()||K==FDelegateProperty::StaticClass()||K==FMulticastInlineDelegateProperty::StaticClass()||K==FMulticastSparseDelegateProperty::StaticClass();
        if(!Plain||P->ArrayDim<1||P->ArrayDim>256||P->GetSize()<1)return false;
        UStruct* Type=nullptr;uint32 BoolLayout=0;
        if(K==FStructProperty::StaticClass()){auto* SP=static_cast<FStructProperty*>(P);if(!SP->Struct.IsResolved()||!Loaded(SP->Struct.Get()))return false;Type=SP->Struct.Get();}
        if(K==FObjectProperty::StaticClass()){auto* OP=static_cast<FObjectProperty*>(P);if(!OP->PropertyClass.IsResolved()||!Loaded(OP->PropertyClass.Get()))return false;Type=OP->PropertyClass.Get();}
        if(K==FBoolProperty::StaticClass()){const auto* B=static_cast<FBoolProperty*>(P);BoolLayout=uint32(B->GetBoolFieldSize())|(uint32(B->GetByteOffset())<<8)|(uint32(B->GetByteMask())<<16)|(uint32(B->GetFieldMask())<<24);}
        FProperty* Inner=nullptr;FProperty* Value=nullptr;
        if(K==FArrayProperty::StaticClass())Inner=static_cast<FArrayProperty*>(P)->Inner;
        else if(K==FSetProperty::StaticClass())Inner=static_cast<FSetProperty*>(P)->ElementProp;
        else if(K==FMapProperty::StaticClass()){Inner=static_cast<FMapProperty*>(P)->KeyProp;Value=static_cast<FMapProperty*>(P)->ValueProp;}
        else if(K==FEnumProperty::StaticClass())Inner=static_cast<FEnumProperty*>(P)->GetUnderlyingProperty();
        if(const auto* E=TreeFields.FindByPredicate([&](const FTreeField& V){return V.Property==P;}))return E->Class==K&&E->Owner==P->GetOwnerStruct()&&E->Name==P->GetFName()&&E->Flags==uint64(P->GetPropertyFlags())&&E->Offset==P->GetOffset_ForInternal()&&E->Size==P->GetSize()&&E->Dim==P->ArrayDim&&E->Accessor==P->HasSetterOrGetter()&&E->Type==Type&&E->BoolLayout==BoolLayout&&E->Inner==Inner&&E->Value==Value&&E->RootProperty==P->GetOwnerProperty();
        if(TreeReady||TreeFields.Num()>=4096)return false;
        auto* Owner=P->GetOwnerStruct();if(!Loaded(Owner))return false;Holds.Emplace(Owner);if(Type)Holds.Emplace(Type);
        TreeFields.Add({P,K,Owner,P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim,P->HasSetterOrGetter(),Type,BoolLayout,Inner,Value,P->GetOwnerProperty()});return true;
    }
    bool TreeReference(UObject* Value,UWidgetTree* InTree,FString& Out)
    {
        if(!Value){TreeToken(Out,TEXT("null"));return true;}if(!Loaded(Value))return false;
        if(Value==InTree||Value->IsIn(InTree)){TreeToken(Out,TEXT("tree:")+Value->GetPathName(InTree));return true;}
        const FString Ref=Value->GetPathName();if(!ReferenceGuard(Ref))return false;
        if(!TreeReady)Holds.Emplace(Value);else if(!Holds.ContainsByPredicate([&](const auto& H){return H.Get()==Value;}))return false;
        TreeToken(Out,TEXT("external:")+Ref);TreeToken(Out,Value->GetClass()->GetPathName());return true;
    }
    bool CompleteNumericPOD(UScriptStruct* S,int32 Depth)
    {
        if(Depth>16||!Loaded(S)||S->GetClass()!=UScriptStruct::StaticClass()||!S->GetOutermost()->GetName().StartsWith(TEXT("/Script/"))||!ReferenceGuard(S->GetPathName()))return false;
        const uint32 Required=uint32(STRUCT_IsPlainOldData)|uint32(STRUCT_NoDestructor);
        const int32 Size=S->GetStructureSize();if((uint32(S->StructFlags)&Required)!=Required||Size<=0||Size>65536)return false;
        TArray<uint8> Covered;Covered.Init(0,Size);int32 Count=0;
        for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::IncludeSuper|EFieldIterationFlags::IncludeDeprecated);It;++It)
        {
            FProperty* Q=*It;if(++Count>256||!TreeField(Q)||Q->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_SkipSerialization|CPF_EditorOnly))return false;
            const int32 Offset=Q->GetOffset_ForInternal(),Span=Q->GetSize();if(Offset<0||Span<=0||int64(Offset)+Span>Size||int64(Q->ElementSize)*Q->ArrayDim!=Span)return false;
            auto* K=Q->GetClass();
            const bool Numeric=K==FByteProperty::StaticClass()||K==FInt8Property::StaticClass()||K==FInt16Property::StaticClass()||K==FUInt16Property::StaticClass()||K==FIntProperty::StaticClass()||K==FUInt32Property::StaticClass()||K==FInt64Property::StaticClass()||K==FUInt64Property::StaticClass()||K==FFloatProperty::StaticClass()||K==FDoubleProperty::StaticClass();
            if(!Numeric)
            {
                if(K!=FStructProperty::StaticClass())return false;auto* Nested=static_cast<FStructProperty*>(Q);if(!Nested->Struct.IsResolved()||!Loaded(Nested->Struct.Get())||Nested->Struct->GetStructureSize()!=Q->ElementSize||!CompleteNumericPOD(Nested->Struct.Get(),Depth+1))return false;
            }
            for(int32 I=Offset;I<Offset+Span;++I){if(Covered[I])return false;Covered[I]=1;}
        }
        if(Count==0||Covered.Contains(0))return false;
        return true; // No padding, unreflected payload or object/container/bool storage is admitted.
    }
    bool TreeScalar(FProperty* P,const void* Data,UWidgetTree* InTree,FString& Out,int32 Depth,bool Runtime)
    {
        if(Depth>16||!TreeField(P)||!Data)return false;auto* K=P->GetClass();
        if(K==FBoolProperty::StaticClass()){TreeToken(Out,static_cast<FBoolProperty*>(P)->GetPropertyValue(Data)?TEXT("1"):TEXT("0"));return true;}
        if(K==FNameProperty::StaticClass()){TreeToken(Out,static_cast<const FName*>(Data)->ToString());return true;}
        if(K==FStrProperty::StaticClass()){const auto& S=*static_cast<const FString*>(Data);if(S.Len()>65536)return false;TreeToken(Out,S);return true;}
        if(K==FTextProperty::StaticClass())
        {
            const FText& T=*static_cast<const FText*>(Data);const FString* S=FTextInspector::GetSourceString(T);
            const auto NS=FTextInspector::GetNamespace(T),Key=FTextInspector::GetKey(T);
            TreeToken(Out,LexToString(FTextInspector::GetFlags(T)));TreeToken(Out,S?*S:TEXT(""));TreeToken(Out,NS.IsSet()?TEXT("set:")+NS.GetValue():TEXT("unset"));TreeToken(Out,Key.IsSet()?TEXT("set:")+Key.GetValue():TEXT("unset"));
            // No display evaluation, namespace repair or native exporter is invoked.
            if(!Runtime)TreeToken(Out,FString::Printf(TEXT("%p"),FTextInspector::GetSharedDataId(T)));
            return S!=nullptr||FTextInspector::GetSharedDataId(T)==FTextInspector::GetSharedDataId(FText::GetEmpty());
        }
        if(K==FObjectProperty::StaticClass())
        {
            const auto* V=static_cast<const TObjectPtr<UObject>*>(Data);if(!V->IsResolved())return false;return TreeReference(V->Get(),InTree,Out);
        }
        if(K==FSoftObjectProperty::StaticClass()){const auto& V=*static_cast<const FSoftObjectPtr*>(Data);const FString Ref=V.ToSoftObjectPath().ToString();if(!Ref.IsEmpty()&&!ReferenceGuard(Ref))return false;TreeToken(Out,Ref);return true;}
        if(K==FStructProperty::StaticClass())
        {
            auto* SP=static_cast<FStructProperty*>(P);if(!SP->Struct.IsResolved())return false;auto* S=SP->Struct.Get();
            if(!Loaded(S)||S->GetClass()!=UScriptStruct::StaticClass()||!S->GetOutermost()->GetName().StartsWith(TEXT("/Script/"))||!ReferenceGuard(S->GetPathName())||S->GetStructureSize()!=P->ElementSize)return false;
            // Private native serialization/export state has no general reflected proof.
            if((S->StructFlags&STRUCT_ExportTextItemNative)||((S->StructFlags&STRUCT_SerializeNative)&&S!=FFontOutlineSettings::StaticStruct()))
            {
                if(!CompleteNumericPOD(S,Depth))
                {
                    if(!R.Data->HasField(TEXT("widget_tree_pending_native_struct"))){R.Data->SetStringField(TEXT("widget_tree_pending_native_struct"),S->GetPathName());R.Data->SetNumberField(TEXT("widget_tree_pending_native_struct_flags"),uint32(S->StructFlags));R.Data->SetNumberField(TEXT("widget_tree_pending_native_struct_size"),S->GetStructureSize());}
                    return false;
                }
                if(!TreeReady)R.Data->SetBoolField(TEXT("widget_tree_complete_numeric_pod_observed"),true);
            }
            if(!TreeReady)Holds.Emplace(S);TreeToken(Out,S->GetPathName());int32 Count=0;
            for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::IncludeSuper|EFieldIterationFlags::IncludeDeprecated);It;++It)
            {
                auto* Q=*It;if(Q->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_SkipSerialization)||(Runtime&&Q->HasAnyPropertyFlags(CPF_EditorOnly)))continue;
                if(++Count>256||Q->GetOffset_ForInternal()<0||Q->GetOffset_ForInternal()+Q->GetSize()>S->GetStructureSize()||!TreeValue(Q,Q->ContainerPtrToValuePtr<void>(Data),InTree,Out,Depth+1,Runtime))return false;
            }return Count>0;
        }
        if(K==FArrayProperty::StaticClass())
        {
            auto* A=static_cast<FArrayProperty*>(P);if(!TreeField(A->Inner))return false;FScriptArrayHelper H(A,Data);if(H.Num()>256)return false;TreeToken(Out,FString::FromInt(H.Num()));for(int32 I=0;I<H.Num();++I)if(!TreeValue(A->Inner,H.GetRawPtr(I),InTree,Out,Depth+1,Runtime))return false;return true;
        }
        if(K==FSetProperty::StaticClass())
        {
            auto* A=static_cast<FSetProperty*>(P);if(!TreeField(A->ElementProp))return false;FScriptSetHelper H(A,Data);if(H.Num()>256)return false;TArray<FString> Values;for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){FString V;if(!TreeValue(A->ElementProp,H.GetElementPtr(I),InTree,V,Depth+1,Runtime))return false;Values.Add(MoveTemp(V));}Values.Sort();for(const auto& V:Values)TreeToken(Out,V);return true;
        }
        if(K==FMapProperty::StaticClass())
        {
            auto* A=static_cast<FMapProperty*>(P);if(!TreeField(A->KeyProp)||!TreeField(A->ValueProp))return false;FScriptMapHelper H(A,Data);if(H.Num()>256)return false;TArray<FString> Values;for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){FString V;if(!TreeValue(A->KeyProp,H.GetKeyPtr(I),InTree,V,Depth+1,Runtime)||!TreeValue(A->ValueProp,H.GetValuePtr(I),InTree,V,Depth+1,Runtime))return false;Values.Add(MoveTemp(V));}Values.Sort();for(const auto& V:Values)TreeToken(Out,V);return true;
        }
        if(K==FDelegateProperty::StaticClass()){if(!static_cast<const FScriptDelegate*>(Data)->GetFunctionName().IsNone())return false;TreeToken(Out,TEXT("unbound"));return true;}
        if(K==FMulticastSparseDelegateProperty::StaticClass()){if(static_cast<const FSparseDelegate*>(Data)->IsBound())return false;TreeToken(Out,TEXT("unbound"));return true;}
        if(K==FMulticastInlineDelegateProperty::StaticClass())
        {
            const auto* D=static_cast<FMulticastDelegateProperty*>(P)->GetMulticastDelegate(Data);if(D&&D->IsBound())return false;TreeToken(Out,TEXT("unbound"));return true;
        }
        if(K==FEnumProperty::StaticClass())
        {
            auto* Inner=static_cast<FEnumProperty*>(P)->GetUnderlyingProperty();if(!TreeField(Inner)||Inner->GetSize()!=P->ElementSize)return false;
        }
        TreeToken(Out,BytesToHex(static_cast<const uint8*>(Data),P->ElementSize));return true;
    }
    bool TreeValue(FProperty* P,const void* Data,UWidgetTree* InTree,FString& Out,int32 Depth,bool Runtime)
    {
        if(!TreeField(P))return false;TreeToken(Out,P->GetName());TreeToken(Out,P->GetClass()->GetName());
        for(int32 I=0;I<P->ArrayDim;++I)if(!TreeScalar(P,static_cast<const uint8*>(Data)+I*P->ElementSize,InTree,Out,Depth,Runtime)){if(!R.Data->HasField(TEXT("widget_tree_pending_property"))){R.Data->SetStringField(TEXT("widget_tree_pending_property"),P->GetPathName());R.Data->SetStringField(TEXT("widget_tree_pending_field_class"),P->GetClass()->GetName());}return false;}return Out.Len()<=1048576;
    }
    bool ReadWidgetTree(UWidgetTree* InTree,FTreeState& Out)
    {
        if(!Loaded(InTree)||InTree->GetClass()!=UWidgetTree::StaticClass()||!InTree->RootWidget.IsResolved()||!InTree->NamedSlotBindings.IsEmpty())return false;
        auto* TreeRoot=InTree->RootWidget.Get();if(!Loaded(TreeRoot)||TreeRoot->GetClass()!=UCanvasPanel::StaticClass())return false;
        TArray<UObject*> Objects;GetObjectsWithOuter(InTree,Objects,true);if(Objects.Num()>256)return false;
        for(auto* O:Objects)
        {
            if(!Loaded(O)||!TreeNative(O->GetClass())||(!O->IsA<UWidget>()&&!O->IsA<UPanelSlot>())||O->IsA<UUserWidget>())return false;
            FTreeObject Row;Row.Object.Reset(O);Row.Class.Reset(O->GetClass());Row.Outer.Reset(O->GetOuter());Row.Path=O->GetPathName();Row.Relative=O->GetPathName(InTree);Out.Objects.Add(MoveTemp(Row));
        }
        if(!Objects.Contains(TreeRoot))return false;Out.Objects.Sort([](const auto& A,const auto& B){return A.Relative<B.Relative;});
        TSet<UWidget*> Seen;TArray<UWidget*> Pending;Pending.Add(TreeRoot);
        for(int32 I=0;I<Pending.Num();++I)
        {
            auto* W=Pending[I];if(!Objects.Contains(W)||Seen.Contains(W)||W->GetOuter()!=InTree||!W->Slot.IsResolved())return false;Seen.Add(W);
            if(W==TreeRoot){if(W->Slot.Get())return false;}
            else{auto* Slot=W->Slot.Get();if(!Objects.Contains(Slot)||!Slot->Parent.IsResolved()||!Slot->Content.IsResolved()||Slot->Content.Get()!=W||!Loaded(Slot->Parent.Get())||!Objects.Contains(Slot->Parent.Get()))return false;}
            TreeToken(Out.Shape,W->GetPathName(InTree));
            if(auto* Panel=Cast<UPanelWidget>(W))
            {
                auto* SlotsField=FindFProperty<FArrayProperty>(UPanelWidget::StaticClass(),TEXT("Slots"));
                if(!SlotsField||SlotsField->GetOwnerClass()!=UPanelWidget::StaticClass()||!TreeField(SlotsField)||SlotsField->Inner->GetClass()!=FObjectProperty::StaticClass()||!TreeField(SlotsField->Inner))return false;
                FScriptArrayHelper Slots(SlotsField,SlotsField->ContainerPtrToValuePtr<void>(Panel));if(Slots.Num()>128)return false;
                for(int32 K=0;K<Slots.Num();++K){const auto& H=*reinterpret_cast<const TObjectPtr<UPanelSlot>*>(Slots.GetRawPtr(K));if(!H.IsResolved()||!Objects.Contains(H.Get()))return false;auto* Slot=H.Get();if(!Slot->Parent.IsResolved()||Slot->Parent.Get()!=Panel||!Slot->Content.IsResolved()||!Objects.Contains(Slot->Content.Get())||!Slot->Content->Slot.IsResolved()||Slot->Content->Slot.Get()!=Slot)return false;Pending.Add(Slot->Content.Get());TreeToken(Out.Shape,Slot->GetPathName(InTree));}
            }
        }
        int32 Widgets=0,Slots=0;for(auto* O:Objects){Widgets+=O->IsA<UWidget>()?1:0;Slots+=O->IsA<UPanelSlot>()?1:0;}if(Widgets!=Seen.Num()||Slots!=Widgets-1)return false;
        for(auto& Row:Out.Objects)
        {
            auto* O=Row.Object.Get();TreeToken(Out.Runtime,Row.Relative);TreeToken(Out.Runtime,Row.Class->GetPathName());
            for(TFieldIterator<FProperty> It(Row.Class.Get(),EFieldIterationFlags::IncludeSuper|EFieldIterationFlags::IncludeDeprecated);It;++It)
            {
                auto* Q=*It;if(Q->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_SkipSerialization))continue;
                if(Q->GetOffset_ForInternal()<0||Q->GetOffset_ForInternal()+Q->GetSize()>Row.Class->GetPropertiesSize())return false;
                if(!TreeValue(Q,Q->ContainerPtrToValuePtr<void>(O),InTree,Row.Values,0,false))return false;
                if(!Q->HasAnyPropertyFlags(CPF_EditorOnly)&&!TreeValue(Q,Q->ContainerPtrToValuePtr<void>(O),InTree,Row.RuntimeValues,0,true))return false;
            }TreeToken(Out.Runtime,Row.RuntimeValues);
        }TreeToken(Out.Runtime,Out.Shape);return true;
    }
    bool WidgetTreeCurrent()
    {
        if(!TreeReady)return true;FTreeState Actual;if(!ReadWidgetTree(Tree.Get(),Actual)||Actual.Shape!=AuthoredTree.Shape||Actual.Objects.Num()!=AuthoredTree.Objects.Num())return false;
        for(int32 I=0;I<Actual.Objects.Num();++I){const auto& A=Actual.Objects[I];const auto& E=AuthoredTree.Objects[I];if(A.Object.Get()!=E.Object.Get()||A.Class.Get()!=E.Class.Get()||A.Outer.Get()!=E.Outer.Get()||A.Path!=E.Path||A.Values!=E.Values)return false;}return true;
    }

    struct FCompiledTree{UClass* Owner=nullptr;TStrongObjectPtr<UWidgetTree> Tree;TStrongObjectPtr<UWidget> Root;FString Path,RootPath;FTreeState State;};TArray<FCompiledTree> CompiledTrees;
    FObjectProperty* CompiledTreeField=nullptr;int32 CompiledTreeOffset=0;EClassFlags ParentFlags=CLASS_None;TStrongObjectPtr<UObject> ParentCDO;
    explicit FWidgetDispatcherWork(FResult& In):R(In){}
    bool Bad(const TCHAR* Stage,const TCHAR* Field,const FString& Expected=TEXT("captured role"),const FString& Actual=TEXT("changed/unavailable"))
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Path);J->SetStringField(TEXT("property"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("widget_dispatcher_first_mismatch"),J);R.Stage=Stage;Fail(R,TEXT("widget_dispatcher_current_changed"),TEXT("Current Widget dispatcher roles or authored values are unsafe; actual prefix retained, no further SDK/cleanup/retry."));}
        Stopped=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;return false;
    }
    bool Context(){return !Stopped&&IsInGameThread()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GExitPurge&&!IsEngineExitRequested()&&Loaded(Editor.Get())&&GEditor==Editor.Get()&&Loaded(World.Get())&&World->WorldType==EWorldType::Editor&&!World->bIsTearingDown&&Editor->PlayWorld.IsResolved()&&!Editor->PlayWorld.Get()&&!IsPlaySessionActive()&&Editor->GetEditorWorldContext().World()==World.Get();}
    bool ClassRole(UClass* C,UClass* Meta,const FString& ExpectedPath)
    {return Loaded(C)&&C->GetClass()==Meta&&C->GetOuter()==Package.Get()&&C->GetSuperClass()==Parent.Get()&&C->ClassGeneratedBy.IsResolved()&&C->ClassGeneratedBy.Get()==BP.Get()&&C->GetPathName().Equals(ExpectedPath,ESearchCase::CaseSensitive)&&!C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)&&C->ClassDefaultObject.IsResolved();}
    bool CDO(UClass* C,UObject* O){return !O||(Loaded(O)&&O->GetClass()==C&&O->GetOuter()==Package.Get()&&O->GetPackage()==Package.Get()&&O->HasAnyFlags(RF_ClassDefaultObject)&&O->GetFName()==FName(*(TEXT("Default__")+C->GetName())));}
    static bool SameType(const FEdGraphPinType& A,const FEdGraphPinType& B){return A==B;}
    bool OldVariables()
    {
        if(BP->NewVariables.Num()!=Variables.Num()+(MemberAdded?1:0))return false;
        for(int32 I=0;I<Variables.Num();++I)
        {
            const auto& A=Variables[I];const auto& B=BP->NewVariables[I];
            if(A.VarName!=B.VarName||A.VarGuid!=B.VarGuid||!SameType(A.VarType,B.VarType)||A.PropertyFlags!=B.PropertyFlags||!A.FriendlyName.Equals(B.FriendlyName,ESearchCase::CaseSensitive)||!A.DefaultValue.Equals(B.DefaultValue,ESearchCase::CaseSensitive)||A.RepNotifyFunc!=B.RepNotifyFunc||A.ReplicationCondition!=B.ReplicationCondition||A.MetaDataArray.Num()!=B.MetaDataArray.Num()||FTextInspector::GetFlags(A.Category)!=FTextInspector::GetFlags(B.Category)||FTextInspector::GetSharedDataId(A.Category)!=FTextInspector::GetSharedDataId(B.Category))return false;
            for(int32 K=0;K<A.MetaDataArray.Num();++K)if(A.MetaDataArray[K].DataKey!=B.MetaDataArray[K].DataKey||!A.MetaDataArray[K].DataValue.Equals(B.MetaDataArray[K].DataValue,ESearchCase::CaseSensitive))return false;
        }
        if(MemberAdded)
        {
            const auto& D=BP->NewVariables.Last();if(D.VarName!=Dispatcher||!D.VarGuid.IsValid()||D.VarType.PinCategory!=UEdGraphSchema_K2::PC_MCDelegate)return false;
            if(SelectedPlanReady)
            {
                const auto& E=SelectedPlan;if(D.VarGuid!=E.VarGuid||!SameType(D.VarType,E.VarType)||D.PropertyFlags!=E.PropertyFlags||!D.FriendlyName.Equals(E.FriendlyName,ESearchCase::CaseSensitive)||!D.DefaultValue.Equals(E.DefaultValue,ESearchCase::CaseSensitive)||D.MetaDataArray.Num()!=E.MetaDataArray.Num())return false;
                for(int32 I=0;I<E.MetaDataArray.Num();++I)if(E.MetaDataArray[I].DataKey!=D.MetaDataArray[I].DataKey||!E.MetaDataArray[I].DataValue.Equals(D.MetaDataArray[I].DataValue,ESearchCase::CaseSensitive))return false;
                if(!MutableCategory&&(FTextInspector::GetFlags(E.Category)!=FTextInspector::GetFlags(D.Category)||FTextInspector::GetSharedDataId(E.Category)!=FTextInspector::GetSharedDataId(D.Category)))return false;
            }
        }
        return true;
    }
    bool PinRows(UEdGraphNode* N,TArray<FPin>& Out)
    {
        if(N->Pins.Num()>128)return false;TSet<FGuid> Ids;
        for(UEdGraphPin* P:N->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||!P->DefaultObject.IsResolved()||P->ParentPin||!P->SubPins.IsEmpty()||!P->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved()||P->PinType.PinSubCategoryObject.IsStale()||P->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return false;
            Ids.Add(P->PinId);if(P->PinType.PinSubCategoryMemberReference.MemberParent.Get())Holds.Emplace(P->PinType.PinSubCategoryMemberReference.MemberParent.Get());if(P->DefaultObject.Get())Holds.Emplace(P->DefaultObject.Get());if(P->PinType.PinSubCategoryObject.Get())Holds.Emplace(P->PinType.PinSubCategoryObject.Get());
            FPin V{P,P->PinId,P->PinName,P->Direction,P->PinType,P->DefaultValue,P->AutogeneratedDefaultValue,FString(),P->DefaultTextValue,FTextInspector::GetFlags(P->DefaultTextValue),P->DefaultObject.Get(),P->PinType.PinSubCategoryObject.Get(),{}};Out.Add(MoveTemp(V));
        }
        return true;
    }
    static bool StockSelf(UEdGraphNode* N)
    {
        if(N->Pins.Num()!=1)return false;const auto* P=N->Pins[0];return P&&!P->WasTrashed()&&P->GetOwningNodeUnchecked()==N&&P->PinName==UEdGraphSchema_K2::PN_Self&&P->Direction==EGPD_Output&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&P->PinType.PinSubCategory==UEdGraphSchema_K2::PSC_Self&&!P->PinType.PinSubCategoryObject.IsStale()&&P->PinType.PinSubCategoryObject.Get()==nullptr&&!P->PinType.IsContainer()&&!P->PinType.bIsReference&&!P->PinType.bIsConst&&!P->PinType.bIsWeakPointer&&P->DefaultValue.IsEmpty()&&P->DefaultObject.IsResolved()&&P->DefaultObject.Get()==nullptr;
    }
    // QN3: passive support for the existing public CreateDelegate authoring node.
    // Scope/signature getters can resolve member references: use them only outside
    // pure Current, after the complete pin inventory has been registered.
    bool DelegateSelection(const FNode& Row,FNode* Capture)
    {
        auto* N=static_cast<UK2Node_CreateDelegate*>(Row.Hold.Get());if(!Loaded(N)||N->GetClass()!=UK2Node_CreateDelegate::StaticClass()||N->Pins.Num()!=2)return false;
        auto* NativeGuid=FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.Guid"));if(!Loaded(NativeGuid)||NativeGuid->GetClass()!=UScriptStruct::StaticClass()||NativeGuid->GetStructureSize()!=sizeof(FGuid)||NativeGuid->GetMinAlignment()!=alignof(FGuid))return false;
        auto* Field=FindFProperty<FStructProperty>(UK2Node_CreateDelegate::StaticClass(),TEXT("SelectedFunctionGuid"));if(!Field||Field->GetOwnerStruct()!=UK2Node_CreateDelegate::StaticClass()||Field->GetOwnerProperty()!=Field||Field->GetClass()!=FStructProperty::StaticClass()||Field->ArrayDim!=1||Field->GetSize()!=sizeof(FGuid)||Field->HasSetterOrGetter()||!Field->Struct.IsResolved()||Field->Struct.Get()!=NativeGuid)return false;
        const FName Name=N->GetFunctionName();const FGuid Id=*Field->ContainerPtrToValuePtr<FGuid>(N);
        if(Capture){Capture->DelegateName=Name;Capture->DelegateGuid=Id;Capture->DelegateGuidField=Field;Capture->DelegateGuidOffset=Field->GetOffset_ForInternal();Capture->DelegateGuidFlags=uint64(Field->GetPropertyFlags());Capture->DelegateGuidStruct.Reset(NativeGuid);}
        else if(Name!=Row.DelegateName||Id!=Row.DelegateGuid||Field!=Row.DelegateGuidField||Field->GetOffset_ForInternal()!=Row.DelegateGuidOffset||uint64(Field->GetPropertyFlags())!=Row.DelegateGuidFlags||NativeGuid!=Row.DelegateGuidStruct.Get())return false;
        UEdGraphPin* Object=nullptr;UEdGraphPin* Delegate=nullptr;
        for(auto* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N)return false;if(P->PinName==UEdGraphSchema_K2::PN_Self&&P->Direction==EGPD_Input&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Object)Object=P;else if(P->PinName==FName(TEXT("OutputDelegate"))&&P->Direction==EGPD_Output&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Delegate)Delegate=P;else return false;}
        // The SDK Scope getter checks this cardinality. Reciprocal peer membership
        // is checked by the shared two-pass inventory before either getter runs.
        return Object&&Delegate&&Object->LinkedTo.Num()<=1;
    }
    bool DelegateSourcesCurrent(const FNode& N)
    {
        if(!N.DelegateObserved)return true;
        UClass* Owner=N.DelegateScope.Get();UFunction* DelegateFunction=N.DelegateSignature.Get();UClass* SignatureOwner=N.DelegateSignatureOwner.Get();
        if(!Loaded(Owner)||(Owner!=Gen.Get()&&Owner!=Skel.Get())||!Loaded(SignatureOwner)||(SignatureOwner!=Gen.Get()&&SignatureOwner!=Skel.Get())||!Loaded(DelegateFunction)||DelegateFunction->GetOuter()!=SignatureOwner||DelegateFunction->GetClass()!=UFunction::StaticClass()||DelegateFunction->GetFName()!=N.DelegateSignatureName||DelegateFunction->FunctionFlags!=N.DelegateSignatureFlags)return false;
        bool Found=false;for(TFieldIterator<UFunction> It(SignatureOwner,EFieldIterationFlags::None);It;++It)if(*It==DelegateFunction){Found=true;break;}if(!Found)return false;
        TArray<FProperty*> SignatureProperties;for(TFieldIterator<FProperty> It(DelegateFunction,EFieldIterationFlags::None);It;++It){if(SignatureProperties.Num()>=16)return false;SignatureProperties.Add(*It);}if(SignatureProperties.Num()!=N.DelegateFields.Num())return false;
        for(int32 I=0;I<SignatureProperties.Num();++I){const auto& E=N.DelegateFields[I];if(SignatureProperties[I]!=E.P)return false;auto* P=SignatureProperties[I];if(P->GetOwnerStruct()!=DelegateFunction||P->GetOwnerProperty()!=P||P->GetFName()!=E.Name||P->GetClass()!=E.Class||uint64(P->GetPropertyFlags())!=E.Flags||P->GetOffset_ForInternal()!=E.Offset||P->GetSize()!=E.Size||P->ArrayDim!=E.Dim||P->HasSetterOrGetter())return false;}
        return true;
    }
    bool ObserveDelegateSources(TArray<FGraph>& Rows,const TCHAR* Stage)
    {
        for(auto& G:Rows)for(auto& N:G.Nodes)if(N.ReferenceKind==6)
        {
            if(!Current(Stage))return false;auto* Node=static_cast<UK2Node_CreateDelegate*>(N.Hold.Get());UClass* Scope=Node->GetScopeClass();TStrongObjectPtr<UClass> ScopeHold(Scope);if(!Current(Stage))return false;
            if(!Loaded(Scope)||(Scope!=Gen.Get()&&Scope!=Skel.Get()))return Bad(Stage,TEXT("create_delegate_own_scope"),TEXT("current own generated/skeleton class"),Scope?TEXT("returned unavailable/non-own class"):TEXT("null"));
            UFunction* DelegateFunction=Node->GetDelegateSignature();TStrongObjectPtr<UFunction> SignatureHold(DelegateFunction);if(!Current(Stage))return false;
            if(!Loaded(DelegateFunction)||DelegateFunction->GetClass()!=UFunction::StaticClass())return Bad(Stage,TEXT("create_delegate_signature"),TEXT("current own delegate signature"),DelegateFunction?TEXT("returned unavailable/non-function role"):TEXT("null"));
            auto* Owner=Cast<UClass>(DelegateFunction->GetOuter());TStrongObjectPtr<UClass> OwnerHold(Owner);if(!Loaded(Owner)||(Owner!=Gen.Get()&&Owner!=Skel.Get()))return Bad(Stage,TEXT("create_delegate_signature_owner"));
            bool Found=false;for(TFieldIterator<UFunction> It(Owner,EFieldIterationFlags::None);It;++It)if(*It==DelegateFunction){Found=true;break;}if(!Found)return Bad(Stage,TEXT("create_delegate_signature_membership"));
            N.DelegateScope.Reset(Scope);N.DelegateSignature.Reset(DelegateFunction);N.DelegateSignatureOwner.Reset(Owner);N.DelegateSignatureName=DelegateFunction->GetFName();N.DelegateSignatureFlags=DelegateFunction->FunctionFlags;N.DelegateFields.Reset();
            for(TFieldIterator<FProperty> It(DelegateFunction,EFieldIterationFlags::None);It;++It){auto* P=*It;if(N.DelegateFields.Num()>=16||P->GetOwnerStruct()!=DelegateFunction||P->GetOwnerProperty()!=P||P->ArrayDim!=1||P->HasSetterOrGetter())return Bad(Stage,TEXT("create_delegate_signature_fields"));N.DelegateFields.Add({P,P->GetFName(),P->GetClass(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim});}
            N.DelegateObserved=true;if(!Current(Stage)||!DelegateSourcesCurrent(N))return Bad(Stage,TEXT("create_delegate_sources_current"));
        }return true;
    }
    void CopyDelegateSources(const TArray<FGraph>& Before,TArray<FGraph>& After)
    {
        // FreezeNew is not a compiler boundary: retain the prior source authority.
        for(auto& G:After)for(auto& N:G.Nodes)if(N.ReferenceKind==6)for(const auto& OldGraph:Before)for(const auto& E:OldGraph.Nodes)if(E.Id==N.Id){N.DelegateScope.Reset(E.DelegateScope.Get());N.DelegateSignature.Reset(E.DelegateSignature.Get());N.DelegateSignatureOwner.Reset(E.DelegateSignatureOwner.Get());N.DelegateSignatureName=E.DelegateSignatureName;N.DelegateSignatureFlags=E.DelegateSignatureFlags;N.DelegateFields=E.DelegateFields;N.DelegateObserved=E.DelegateObserved;}
    }

    bool CaptureGraphs(TArray<FGraph>& Out,bool IncludeSelected=false)
    {
        TArray<UEdGraph*> All;BP->GetAllGraphs(All);if(All.Num()>16)return false;
        TSet<UEdGraphPin*> Inventory;for(auto* G:All){if(!Loaded(G)||G->Nodes.Num()>64)return false;for(const auto& H:G->Nodes){if(!H.IsResolved()||!Loaded(H.Get()))return false;for(auto* P:H->Pins){if(!P||Inventory.Contains(P))return false;Inventory.Add(P);}}}
        for(auto* G:All)
        {
            if(G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=BP.Get()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||!G->SubGraphs.IsEmpty())return false;
            FGraph Row;Row.Hold.Reset(G);Row.Id=G->GraphGuid;Row.Path=G->GetPathName();Row.Editable=G->bEditable;
            for(const auto& H:G->Nodes)
            {
                auto* N=H.Get();if(NodeMember&&N==NewNode&&!IncludeSelected)continue;FNode V;
                if(auto* D=Cast<UK2Node_BaseMCDelegate>(N)){V.Reference=D->DelegateReference;V.ReferenceKind=1;}
                else if(auto* Event=Cast<UK2Node_CustomEvent>(N)){V.Reference=Event->EventReference;V.ReferenceKind=4;V.CustomName=Event->CustomFunctionName;V.CustomFlags=Event->FunctionFlags;V.CustomOverride=Event->bOverrideFunction;V.CustomDeprecated=Event->bIsDeprecated;V.CustomEditor=Event->bCallInEditor;V.CustomEditable=Event->bIsEditable;V.CustomDeprecation=Event->DeprecationMessage;for(const auto& Info:Event->UserDefinedPins){if(!Info)return false;V.UserPins.Add({Info->PinName,Info->PinType,Info->DesiredPinDirection,Info->PinDefaultValue});}}
                else if(auto* E=Cast<UK2Node_Event>(N)){V.Reference=E->EventReference;V.ReferenceKind=2;}
                else if(auto* EntryNode=Cast<UK2Node_FunctionEntry>(N)){V.Reference=EntryNode->FunctionReference;V.ReferenceKind=3;V.EntryFlags=EntryNode->GetExtraFlags();V.EntryEditable=EntryNode->bIsEditable;
                    for(const auto& Info:EntryNode->UserDefinedPins){if(!Info.IsValid())return false;V.UserPins.Add({Info->PinName,Info->PinType,Info->DesiredPinDirection,Info->PinDefaultValue});}}
                else if(N->GetClass()->GetPathName()==TEXT("/Script/BlueprintGraph.K2Node_Self")){V.ReferenceKind=5;if(!StockSelf(N))return false;}
                else if(N->GetClass()==UK2Node_CreateDelegate::StaticClass()){V.ReferenceKind=6;}
                else return false;V.Hold.Reset(N);V.Class=N->GetClass();V.Id=N->NodeGuid;V.Path=N->GetPathName();V.Comment=N->NodeComment;V.X=N->NodePosX;V.Y=N->NodePosY;if(!PinRows(N,V.Pins))return false;if(V.ReferenceKind==6&&!DelegateSelection(V,&V))return false;
                for(auto& P:V.Pins)for(auto* L:P.Address->LinkedTo){if(!Inventory.Contains(L)||!L->LinkedTo.Contains(P.Address))return false;P.Links.Add(L->PinId);}
                Row.Nodes.Add(MoveTemp(V));
            }Out.Add(MoveTemp(Row));
        }return true;
    }
    bool Physical(const TArray<FGraph>& Rows)
    {
        for(const auto& G:Rows)
        {
            auto* Graph=G.Hold.Get();if(!Loaded(Graph)||Graph->GetOuter()!=BP.Get()||Graph->GetClass()!=UEdGraph::StaticClass()||!Graph->Schema.GetGCPtr().IsResolved()||Graph->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||Graph->GraphGuid!=G.Id||!Graph->GetPathName().Equals(G.Path,ESearchCase::CaseSensitive)||Graph->bEditable!=G.Editable||!Graph->SubGraphs.IsEmpty())return false;
            if(Graph->Nodes.Num()!=G.Nodes.Num()+((Graph==NewGraph&&NodeMember)?1:0))return false;
            TSet<UEdGraphPin*> Inventory;for(const auto& N:G.Nodes)for(const auto& P:N.Pins)Inventory.Add(P.Address);
            for(int32 I=0;I<G.Nodes.Num();++I)
            {
                const auto& E=G.Nodes[I];auto* N=E.Hold.Get();if(!Graph->Nodes[I].IsResolved()||Graph->Nodes[I].Get()!=N||!Loaded(N)||N->GetClass()!=E.Class||N->GetOuter()!=Graph||N->NodeGuid!=E.Id||!N->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||!N->NodeComment.Equals(E.Comment,ESearchCase::CaseSensitive)||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num())return false;
                if(E.ReferenceKind==6&&(!DelegateSelection(E,nullptr)||!DelegateSourcesCurrent(E)))return false;
                if(E.ReferenceKind==1&&!static_cast<UK2Node_BaseMCDelegate*>(N)->DelegateReference.IsSameReference(E.Reference))return false;
                if(E.ReferenceKind==2&&!static_cast<UK2Node_Event*>(N)->EventReference.IsSameReference(E.Reference))return false;
                if(E.ReferenceKind==4){auto* Event=static_cast<UK2Node_CustomEvent*>(N);if(!Event->EventReference.IsSameReference(E.Reference)||Event->CustomFunctionName!=E.CustomName||Event->FunctionFlags!=E.CustomFlags||Event->bOverrideFunction!=E.CustomOverride||Event->bIsDeprecated!=E.CustomDeprecated||Event->bCallInEditor!=E.CustomEditor||Event->bIsEditable!=E.CustomEditable||!Event->DeprecationMessage.Equals(E.CustomDeprecation,ESearchCase::CaseSensitive)||Event->UserDefinedPins.Num()!=E.UserPins.Num())return false;for(int32 K=0;K<E.UserPins.Num();++K){const auto& Info=Event->UserDefinedPins[K];const auto& U=E.UserPins[K];if(!Info||Info->PinName!=U.Name||!SameType(Info->PinType,U.Type)||Info->DesiredPinDirection!=U.Direction||!Info->PinDefaultValue.Equals(U.Default,ESearchCase::CaseSensitive))return false;}}
                if(E.ReferenceKind==3)
                {
                    auto* Entry=static_cast<UK2Node_FunctionEntry*>(N);if(!Entry->FunctionReference.IsSameReference(E.Reference)||Entry->GetExtraFlags()!=E.EntryFlags||Entry->bIsEditable!=E.EntryEditable||Entry->UserDefinedPins.Num()!=E.UserPins.Num())return false;
                    for(int32 K=0;K<E.UserPins.Num();++K){const auto& Info=Entry->UserDefinedPins[K];const auto& U=E.UserPins[K];if(!Info.IsValid()||Info->PinName!=U.Name||!SameType(Info->PinType,U.Type)||Info->DesiredPinDirection!=U.Direction||!Info->PinDefaultValue.Equals(U.Default,ESearchCase::CaseSensitive))return false;}
                }
                for(const auto& P:E.Pins)
                {
                    if(!N->Pins.Contains(P.Address))return false;auto* A=P.Address;
                    if(!A||A->WasTrashed()||A->GetOwningNodeUnchecked()!=N||A->PinId!=P.Id||A->PinName!=P.Name||A->Direction!=P.Direction||!A->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved()||A->PinType.PinSubCategoryObject.IsStale()||A->PinType.PinValueType.TerminalSubCategoryObject.IsStale()||!SameType(A->PinType,P.Type)||!A->DefaultValue.Equals(P.Raw,ESearchCase::CaseSensitive)||!A->AutogeneratedDefaultValue.Equals(P.Auto,ESearchCase::CaseSensitive)||!A->DefaultObject.IsResolved()||A->DefaultObject.Get()!=P.Default||FTextInspector::GetFlags(A->DefaultTextValue)!=P.TextFlags||FTextInspector::GetSharedDataId(A->DefaultTextValue)!=FTextInspector::GetSharedDataId(P.Text)||A->ParentPin||!A->SubPins.IsEmpty()||A->LinkedTo.Num()!=P.Links.Num())return false;
                    for(int32 L=0;L<A->LinkedTo.Num();++L)if(!Inventory.Contains(A->LinkedTo[L])||A->LinkedTo[L]->PinId!=P.Links[L]||!A->LinkedTo[L]->LinkedTo.Contains(A))return false;
                }
            }
            if(Graph==NewGraph&&NodeMember&&(!Graph->Nodes.Last().IsResolved()||Graph->Nodes.Last().Get()!=NewNode||!Loaded(NewNode)||NewNode->GetOuter()!=Graph))return false;
        }return true;
    }
    bool TreeFieldCurrent()
    {
        if(!CompiledTreeField||FindFProperty<FObjectProperty>(UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("WidgetTree"))!=CompiledTreeField)return false;
        return CompiledTreeField->GetOwnerClass()==UWidgetBlueprintGeneratedClass::StaticClass()&&CompiledTreeField->GetOffset_ForInternal()==CompiledTreeOffset&&CompiledTreeField->ArrayDim==1&&CompiledTreeField->GetSize()==sizeof(TObjectPtr<UWidgetTree>)&&CompiledTreeField->PropertyClass.IsResolved()&&CompiledTreeField->PropertyClass.Get()==UWidgetTree::StaticClass();
    }
    bool CaptureCompiledTrees(TArray<FCompiledTree>& Out)
    {
        if(!TreeFieldCurrent())return false;
        for(UClass* C:{Gen.Get(),Skel.Get()})
        {
            if(!C->GetClass()->IsChildOf(UWidgetBlueprintGeneratedClass::StaticClass()))continue;
            const auto* Slot=CompiledTreeField->ContainerPtrToValuePtr<TObjectPtr<UWidgetTree>>(C);if(!Slot->IsResolved())return false;FCompiledTree Row;Row.Owner=C;Row.Tree.Reset(Slot->Get());
            if(Row.Tree.IsValid())
            {
                if(!Loaded(Row.Tree.Get())||Row.Tree->GetClass()!=UWidgetTree::StaticClass()||Row.Tree->GetOuter()!=C||!Row.Tree->RootWidget.IsResolved())return false;
                Row.Path=Row.Tree->GetPathName();Row.Root.Reset(Row.Tree->RootWidget.Get());
                if(Row.Root.IsValid()){if(!Loaded(Row.Root.Get())||Row.Root->GetClass()!=UCanvasPanel::StaticClass()||Row.Root->GetOuter()!=Row.Tree.Get())return false;Row.RootPath=Row.Root->GetPathName();if(!ReadWidgetTree(Row.Tree.Get(),Row.State))return false;}
            }Out.Add(MoveTemp(Row));
        }return true;
    }
    bool CompiledTreesCurrent()
    {
        if(!TreeFieldCurrent())return false;
        for(const auto& Row:CompiledTrees)
        {
            if(Row.Owner!=Gen.Get()&&Row.Owner!=Skel.Get())return false;
            const auto* Slot=CompiledTreeField->ContainerPtrToValuePtr<TObjectPtr<UWidgetTree>>(Row.Owner);if(!Slot->IsResolved()||Slot->Get()!=Row.Tree.Get())return false;
            if(Row.Tree.IsValid()&&(!Loaded(Row.Tree.Get())||Row.Tree->GetClass()!=UWidgetTree::StaticClass()||Row.Tree->GetOuter()!=Row.Owner||!Row.Tree->GetPathName().Equals(Row.Path,ESearchCase::CaseSensitive)||!Row.Tree->RootWidget.IsResolved()||Row.Tree->RootWidget.Get()!=Row.Root.Get()))return false;
            if(Row.Root.IsValid()&&(!Loaded(Row.Root.Get())||Row.Root->GetClass()!=UCanvasPanel::StaticClass()||Row.Root->GetOuter()!=Row.Tree.Get()||!Row.Root->GetPathName().Equals(Row.RootPath,ESearchCase::CaseSensitive)))return false;
            if(Row.Root.IsValid()){FTreeState Actual;if(!ReadWidgetTree(Row.Tree.Get(),Actual)||Actual.Shape!=Row.State.Shape||Actual.Objects.Num()!=Row.State.Objects.Num())return false;for(int32 I=0;I<Actual.Objects.Num();++I){const auto& A=Actual.Objects[I];const auto& E=Row.State.Objects[I];if(A.Object.Get()!=E.Object.Get()||A.Class.Get()!=E.Class.Get()||A.Outer.Get()!=E.Outer.Get()||A.Path!=E.Path||A.Values!=E.Values)return false;}}
        }return true;
    }
    bool Current(const TCHAR* Stage)
    {
        if(!Context())return Bad(Stage,TEXT("editor_context"));auto* B=BP.Get();
        if(!Loaded(B)||B->GetClass()!=UWidgetBlueprint::StaticClass()||B->GetOuter()!=Package.Get()||!B->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!Loaded(Package.Get())||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=Parent.Get()||B->BlueprintType!=BPTYPE_Normal||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified)return Bad(Stage,TEXT("widget_blueprint_role"));
        if(!Loaded(Parent.Get())||Parent.Get()!=UUserWidget::StaticClass()||Parent->GetClassFlags()!=ParentFlags||!Parent->ClassDefaultObject.IsResolved()||Parent->ClassDefaultObject.Get()!=ParentCDO.Get()||(ParentCDO.IsValid()&&(!Loaded(ParentCDO.Get())||ParentCDO->GetClass()!=Parent.Get())))return Bad(Stage,TEXT("native_user_widget_parent"));
        if(B->UbergraphPages!=Uber||B->FunctionGraphs!=Functions||B->MacroGraphs!=Macros||B->DelegateSignatureGraphs.Num()!=Delegates.Num()+(GraphAdded?1:0)||!OldVariables()||!B->Bindings.IsEmpty()||!B->Animations.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->WidgetVariableNameToGuidMap.OrderIndependentCompareEqual(WidgetGuids))return Bad(Stage,TEXT("authored_graph_variable_binding_inventory"));
        for(int32 I=0;I<Delegates.Num();++I)if(!B->DelegateSignatureGraphs[I].IsResolved()||B->DelegateSignatureGraphs[I].Get()!=Delegates[I].Get())return Bad(Stage,TEXT("delegate_signature_graph"));
        if(GraphAdded&&(!B->DelegateSignatureGraphs.Last().IsResolved()||B->DelegateSignatureGraphs.Last().Get()!=NewGraph))return Bad(Stage,TEXT("new_signature_graph_membership"));
        if(!Loaded(Tree.Get())||!B->WidgetTree.IsResolved()||B->WidgetTree.Get()!=Tree.Get()||Tree->GetOuter()!=B||!Tree->GetPathName().Equals(TreePath,ESearchCase::CaseSensitive)||!Tree->RootWidget.IsResolved()||Tree->RootWidget.Get()!=Root.Get()||!Loaded(Root.Get())||Root->GetClass()!=UCanvasPanel::StaticClass()||Root->GetOuter()!=Tree.Get()||!Root->GetPathName().Equals(RootPath,ESearchCase::CaseSensitive)||!WidgetTreeCurrent()||!Root->Slot.IsResolved()||Root->Slot.Get()!=nullptr||Root->bIsVariable!=IsVariable||Root->GetVisibility()!=Visibility||Root->GetIsEnabled()!=Enabled||Root->GetRenderOpacity()!=Opacity||!(Root->GetRenderTransform()==Transform)||Root->GetRenderTransformPivot()!=Pivot||Root->GetClipping()!=Clipping)return Bad(Stage,TEXT("source_canvas_tree_captured_values"));
        if(NewGraph&&(!Loaded(NewGraph)||NewGraph->GetClass()!=UEdGraph::StaticClass()||NewGraph->GetOuter()!=B||!NewGraph->Schema.GetGCPtr().IsResolved()||NewGraph->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()))return Bad(Stage,TEXT("selected_graph_role"));
        if(NewNode&&(!Loaded(NewNode)||NewNode->GetClass()!=NewNodeClass||NewNode->GetOuter()!=NewGraph||!Loaded(NewNodeClass)||((NodeMember||NewNodeClass==UK2Node_FunctionEntry::StaticClass())&&!NewGraph->Nodes.Contains(NewNode))))return Bad(Stage,TEXT("selected_new_node_role"));
        if(NewNode&&NewReferenceReady&&!static_cast<UK2Node_BaseMCDelegate*>(NewNode)->DelegateReference.IsSameReference(NewReference))return Bad(Stage,TEXT("new_delegate_reference"));
        if(!SourceCurrent(Stage))return false;
        if(!Rebinding||ClassesReady)
        {
            if(!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=Gen.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->SkeletonGeneratedClass.GetGCPtr().Get()!=Skel.Get()||!ClassRole(Gen.Get(),GenMeta,GenPath)||!ClassRole(Skel.Get(),SkelMeta,SkelPath)||Gen->ClassDefaultObject.Get()!=GenCDO.Get()||Skel->ClassDefaultObject.Get()!=SkelCDO.Get()||!CDO(Gen.Get(),GenCDO.Get())||!CDO(Skel.Get(),SkelCDO.Get())||!CompiledTreesCurrent())return Bad(Stage,TEXT("generated_skeleton_nullable_cdo"));
        }
        if(!Rebinding){if(!Physical(Graphs))return Bad(Stage,TEXT("authored_graph_pin_values"));}else if(!Candidates.IsEmpty()&&!Physical(Candidates))return Bad(Stage,TEXT("reacquired_graph_pin_values"));
        if(Policy.IsValid()&&(!Loaded(Policy.Get())||Policy->SaveOnCompile!=SoC_Never||!UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.IsResolved()||UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.Get()!=Policy.Get()))return Bad(Stage,TEXT("save_on_compile_policy"));return true;
    }
    bool SourceCurrent(const TCHAR* Stage)
    {
        if(!Property||Rebinding)return true;
        if(!Loaded(PropertyOwner.Get())||!Loaded(Signature.Get())||Signature->GetOuter()!=SignatureOuter||Signature->GetClass()!=SignatureMeta||!Signature->GetPathName().Equals(SignaturePath,ESearchCase::CaseSensitive)||Signature->FunctionFlags!=SignatureFlags)return Bad(Stage,TEXT("dispatcher_signature_role"));
        bool Found=false;for(TFieldIterator<FProperty> It(PropertyOwner.Get(),EFieldIterationFlags::None);It;++It)if(*It==Property){Found=true;break;}
        if(!Found||Property->GetClass()!=PropertyClass||uint64(Property->GetPropertyFlags())!=PropertyFlags||Property->GetOffset_ForInternal()!=PropertyOffset||Property->GetSize()!=PropertySize||Property->ArrayDim!=PropertyDim||Property->GetOwnerClass()!=PropertyOwner.Get()||Property->GetFName()!=Dispatcher||!Property->SignatureFunction.IsResolved()||Property->SignatureFunction.Get()!=Signature.Get()||!Property->HasAllPropertyFlags(CPF_BlueprintAssignable|CPF_BlueprintCallable))return Bad(Stage,TEXT("own_dispatcher_property_binding"));
        TArray<FProperty*> Actual;for(TFieldIterator<FProperty> It(Signature.Get(),EFieldIterationFlags::None);It;++It){if(Actual.Num()>=16)return Bad(Stage,TEXT("signature_field_budget"));Actual.Add(*It);}
        if(Actual.Num()!=Fields.Num())return Bad(Stage,TEXT("signature_field_count"));
        for(int32 I=0;I<Actual.Num();++I){const auto& E=Fields[I];if(Actual[I]!=E.P)return Bad(Stage,TEXT("signature_field_membership"));auto* P=E.P;if(P->GetOwnerStruct()!=Signature.Get()||P->GetOwnerProperty()!=P||P->GetClass()!=E.Class||P->GetFName()!=E.Name||uint64(P->GetPropertyFlags())!=E.Flags||P->GetOffset_ForInternal()!=E.Offset||P->GetSize()!=E.Size||P->ArrayDim!=E.Dim||P->HasSetterOrGetter())return Bad(Stage,TEXT("signature_field_layout"));}
        return true;
    }
    bool AcquireSource(const TCHAR* Stage)
    {
        if(!Current(Stage))return false;FMulticastDelegateProperty* P=FindFProperty<FMulticastDelegateProperty>(Skel.Get(),Dispatcher);UClass* Owner=Skel.Get();if(!P){P=FindFProperty<FMulticastDelegateProperty>(Gen.Get(),Dispatcher);Owner=Gen.Get();}
        if(!P||P->GetOwnerClass()!=Owner||!P->SignatureFunction.IsResolved()||!Loaded(P->SignatureFunction.Get())||!P->HasAllPropertyFlags(CPF_BlueprintAssignable|CPF_BlueprintCallable))return Fail(R,TEXT("widget_dispatcher_property_unavailable"),TEXT("Compile/create own assignable/callable dispatcher before adding nodes; actual owned property/signature required."));
        Property=P;PropertyClass=P->GetClass();PropertyFlags=uint64(P->GetPropertyFlags());PropertyOffset=P->GetOffset_ForInternal();PropertySize=P->GetSize();PropertyDim=P->ArrayDim;PropertyOwner.Reset(Owner);Signature.Reset(P->SignatureFunction.Get());SignatureFlags=Signature->FunctionFlags;SignatureOuter=Signature->GetOuter();SignatureMeta=Signature->GetClass();SignaturePath=Signature->GetPathName();Holds.Emplace(SignatureOuter);Holds.Emplace(SignatureMeta);Fields.Reset();
        for(TFieldIterator<FProperty> It(Signature.Get(),EFieldIterationFlags::None);It;++It)
        {
            auto* Field=*It;const bool Scalar=Field->GetClass()==FIntProperty::StaticClass()||Field->GetClass()==FInt64Property::StaticClass()||Field->GetClass()==FFloatProperty::StaticClass()||Field->GetClass()==FDoubleProperty::StaticClass()||Field->GetClass()==FBoolProperty::StaticClass()||Field->GetClass()==FNameProperty::StaticClass()||Field->GetClass()==FStrProperty::StaticClass();
            if(Fields.Num()>=16||!Scalar||Field->GetOwnerStruct()!=Signature.Get()||Field->GetOwnerProperty()!=Field||Field->HasSetterOrGetter()||Field->ArrayDim!=1||!Field->HasAnyPropertyFlags(CPF_Parm)||Field->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm|CPF_ReferenceParm))return Fail(R,TEXT("widget_dispatcher_signature_pending"),TEXT("First Widget slice has at most16 direct primitive by-value inputs; other signature shapes remain incomplete."));
            Fields.Add({Field,Field->GetClass(),Field->GetFName(),uint64(Field->GetPropertyFlags()),Field->GetOffset_ForInternal(),Field->GetSize(),Field->ArrayDim});
        }return Current(Stage);
    }
    bool SignatureTypes(TArray<FName>& Names,TArray<FEdGraphPinType>& Types,const TCHAR* Stage)
    {
        if(!Current(Stage))return false;const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();if(!Current(Stage))return false;
        for(const auto& E:Fields){FEdGraphPinType Type;const bool Converted=Schema->ConvertPropertyToPinType(E.P,Type);if(!Current(Stage))return false;if(!Converted)return Bad(Stage,TEXT("signature_type_conversion"));Names.Add(E.Name);Types.Add(Type);}return true;
    }
    bool Displays(TArray<FGraph>& Rows,const TCHAR* Stage)
    {
        for(auto& G:Rows)for(auto& N:G.Nodes)for(auto& P:N.Pins){const FText Copy=P.Text;const FString Display=Copy.ToString();if(!Current(Stage))return false;P.Display=Display;}return true;
    }
    bool Logical(const TArray<FGraph>& Before,const TArray<FGraph>& After,UClass* OldGen,UClass* OldSkel,const TCHAR* Stage)
    {
        if(Before.Num()!=After.Num())return Bad(Stage,TEXT("graph_count"),FString::FromInt(Before.Num()),FString::FromInt(After.Num()));
        for(const auto& A:Before)
        {
            const auto* B=After.FindByPredicate([&](const FGraph& G){return G.Id==A.Id;});if(!B||B->Hold.Get()!=A.Hold.Get()||!A.Path.Equals(B->Path,ESearchCase::CaseSensitive)||B->Editable!=A.Editable||B->Nodes.Num()!=A.Nodes.Num())return Bad(Stage,TEXT("authored_graph_semantics"));
            for(const auto& N:A.Nodes)
            {
                const auto* M=B->Nodes.FindByPredicate([&](const FNode& X){return X.Id==N.Id;});if(!M||M->Hold.Get()!=N.Hold.Get()||M->Class!=N.Class||!M->Path.Equals(N.Path,ESearchCase::CaseSensitive)||!M->Comment.Equals(N.Comment,ESearchCase::CaseSensitive)||M->X!=N.X||M->Y!=N.Y||M->Pins.Num()!=N.Pins.Num()||M->ReferenceKind!=N.ReferenceKind||!M->Reference.IsSameReference(N.Reference)||M->UserPins.Num()!=N.UserPins.Num()||M->EntryFlags!=N.EntryFlags||M->EntryEditable!=N.EntryEditable||M->CustomName!=N.CustomName||M->CustomFlags!=N.CustomFlags||M->CustomOverride!=N.CustomOverride||M->CustomDeprecated!=N.CustomDeprecated||M->CustomEditor!=N.CustomEditor||M->CustomEditable!=N.CustomEditable||!M->CustomDeprecation.Equals(N.CustomDeprecation,ESearchCase::CaseSensitive))return Bad(Stage,TEXT("authored_node_semantics"));
                if(N.ReferenceKind==6){if(M->DelegateName!=N.DelegateName||M->DelegateGuid!=N.DelegateGuid||M->DelegateSignatureName!=N.DelegateSignatureName||M->DelegateSignatureFlags!=N.DelegateSignatureFlags||M->DelegateFields.Num()!=N.DelegateFields.Num())return Bad(Stage,TEXT("create_delegate_selection_signature"),N.DelegateName.ToString(),M->DelegateName.ToString());for(int32 K=0;K<N.DelegateFields.Num();++K){const auto& E=N.DelegateFields[K];const auto& V=M->DelegateFields[K];if(E.Name!=V.Name||E.Class!=V.Class||E.Flags!=V.Flags||E.Offset!=V.Offset||E.Size!=V.Size||E.Dim!=V.Dim)return Bad(Stage,TEXT("create_delegate_signature_layout"),E.Name.ToString(),V.Name.ToString());}}
                for(int32 K=0;K<N.UserPins.Num();++K){const auto& AUser=N.UserPins[K];const auto& BUser=M->UserPins[K];if(AUser.Name!=BUser.Name||!SameType(AUser.Type,BUser.Type)||AUser.Direction!=BUser.Direction||!AUser.Default.Equals(BUser.Default,ESearchCase::CaseSensitive))return Bad(Stage,TEXT("signature_user_pin_semantics"));}
                for(const auto& ACell:N.Pins)
                {
                    const auto* BCell=M->Pins.FindByPredicate([&](const FPin& X){return X.Id==ACell.Id;});if(!BCell)return Bad(Stage,TEXT("authored_pin_guid"),ACell.Id.ToString(),TEXT("missing"));
                    auto ExpectedType=ACell.Type;auto* Binding=ACell.TypeObject;if(Binding==OldGen)ExpectedType.PinSubCategoryObject=Gen.Get();else if(Binding==OldSkel)ExpectedType.PinSubCategoryObject=Skel.Get();
                    auto RefParent=ExpectedType.PinSubCategoryMemberReference.MemberParent;if(!RefParent.IsResolved())return Bad(Stage,TEXT("delegate_member_parent_unresolved"));if(RefParent.Get()==OldGen)ExpectedType.PinSubCategoryMemberReference.MemberParent=Gen.Get();else if(RefParent.Get()==OldSkel)ExpectedType.PinSubCategoryMemberReference.MemberParent=Skel.Get();
                    bool TypeMatches=SameType(ExpectedType,BCell->Type);if(N.ReferenceKind==4&&ACell.Name==UK2Node_Event::DelegateOutputName){if(const auto* Projection=CompilerEventReferences.Find(N.Id)){auto Published=ExpectedType;Published.PinSubCategoryMemberReference=*Projection;TypeMatches|=SameType(Published,BCell->Type);}}
                    if(ACell.Name!=BCell->Name||ACell.Direction!=BCell->Direction||!TypeMatches||!ACell.Raw.Equals(BCell->Raw,ESearchCase::CaseSensitive)||!ACell.Auto.Equals(BCell->Auto,ESearchCase::CaseSensitive)||ACell.Default!=BCell->Default||ACell.TextFlags!=BCell->TextFlags||!ACell.Display.Equals(BCell->Display,ESearchCase::CaseSensitive)||ACell.Links!=BCell->Links)
                        return Bad(Stage,TEXT("authored_pin_type_value_links"),ACell.Name.ToString()+TEXT(":")+ACell.Raw,BCell->Name.ToString()+TEXT(":")+BCell->Raw);
                }
            }
        }return true;
    }
    bool Refresh(const TCHAR* Stage)
    {
        if(!Rebinding||!Current(Stage))return false;
        auto G=BP->GeneratedClass,S=BP->SkeletonGeneratedClass;if(!G.GetGCPtr().IsResolved()||!S.GetGCPtr().IsResolved())return Bad(Stage,TEXT("reacquire_class_slots"));
        TStrongObjectPtr<UClass> NewGen(G.GetGCPtr().Get()),NewSkel(S.GetGCPtr().Get());
        if(!ClassRole(NewGen.Get(),GenMeta,GenPath)||!ClassRole(NewSkel.Get(),SkelMeta,SkelPath))return Bad(Stage,TEXT("reacquire_class_roles"));
        TStrongObjectPtr<UObject> NewGCDO(NewGen->ClassDefaultObject.Get()),NewSCDO(NewSkel->ClassDefaultObject.Get());if(!CDO(NewGen.Get(),NewGCDO.Get())||!CDO(NewSkel.Get(),NewSCDO.Get()))return Bad(Stage,TEXT("reacquire_nullable_cdo"));
        UClass* OldGen=Gen.Get();UClass* OldSkel=Skel.Get();Gen.Reset(NewGen.Get());Skel.Reset(NewSkel.Get());GenCDO.Reset(NewGCDO.Get());SkelCDO.Reset(NewSCDO.Get());TArray<FCompiledTree> NewTrees;if(!CaptureCompiledTrees(NewTrees))return Bad(Stage,TEXT("reacquire_compiled_widget_tree"));if(NewTrees.Num()!=CompiledTrees.Num())return Bad(Stage,TEXT("compiled_tree_owner_inventory"));for(int32 I=0;I<NewTrees.Num();++I){const auto& N=NewTrees[I];const auto& E=CompiledTrees[I];if(N.Root.IsValid()&&N.State.Runtime!=AuthoredTree.Runtime&&(!E.Root.IsValid()||N.State.Runtime!=E.State.Runtime))return Bad(Stage,TEXT("compiled_tree_authored_projection"));if(!N.Root.IsValid()&&E.Root.IsValid())return Bad(Stage,TEXT("compiled_tree_root_lost"));}CompiledTrees=MoveTemp(NewTrees);ClassesReady=true;
        CompilerEventReferences.Reset();for(const auto& Row:Graphs)for(const auto& Node:Row.Nodes)if(Node.ReferenceKind==4){UFunction* Function=Skel->FindFunctionByName(Node.CustomName,EIncludeSuperFlag::ExcludeSuper);TStrongObjectPtr<UFunction> Returned(Function);if(!Current(Stage))return false;if(!Function)continue;if(!Loaded(Function)||Function->GetOuter()!=Skel.Get()||!Function->HasAllFunctionFlags(FUNC_BlueprintCallable|FUNC_BlueprintEvent)||Function->HasAnyFunctionFlags(FUNC_Net|FUNC_Static|FUNC_Native))return Bad(Stage,TEXT("custom_event_compiler_role"));FGuid ActualGuid;const bool Found=UBlueprint::GetGuidFromClassByFieldName<UFunction>(Skel.Get(),Node.CustomName,ActualGuid);if(!Current(Stage))return false;if(!Found||ActualGuid!=Node.Id)return Bad(Stage,TEXT("custom_event_compiler_guid"));FSimpleMemberReference Ref;FMemberReference::FillSimpleMemberReference<UFunction>(Function,Ref);if(!Current(Stage))return false;Holds.Emplace(Function);CompilerEventReferences.Add(Node.Id,Ref);}
        Candidates.Reset();if(!CaptureGraphs(Candidates))return Bad(Stage,TEXT("reacquire_current_inventory"));
        // The new selected graph is still being authored, not a baseline for prior graphs.
        if(GraphAdded&&!Graphs.ContainsByPredicate([&](const FGraph& Row){return Row.Hold.Get()==NewGraph;}))Candidates.RemoveAll([&](const FGraph& Row){return Row.Hold.Get()==NewGraph;});
        if(!Current(Stage)||!ObserveDelegateSources(Candidates,Stage)||!Displays(Candidates,Stage)||!Logical(Graphs,Candidates,OldGen,OldSkel,Stage))return false;
        Graphs=MoveTemp(Candidates);Rebinding=false;return Current(Stage);
    }
    bool Begin(const FString& Requested,FName Name)
    {
        Dispatcher=Name;const FString ObjectPath=Requested.Contains(TEXT("."))?Requested:Requested+TEXT(".")+FPackageName::GetShortName(Requested);
        if(!Function::Pin::SafeName(ObjectPath)||!ReferenceGuard(ObjectPath)||!Requested.StartsWith(TEXT("/Game/")))return Fail(R,TEXT("widget_dispatcher_path_pending"),TEXT("Already-loaded exact Game Widget Blueprint path required."));
        BP.Reset(FindObject<UWidgetBlueprint>(nullptr,*ObjectPath));Editor.Reset(GEditor);if(!IsInGameThread()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()||!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Editor->PlayWorld.IsResolved()||Editor->PlayWorld.Get()||IsPlaySessionActive()||!Loaded(BP.Get())||BP->GetClass()!=UWidgetBlueprint::StaticClass())return Fail(R,TEXT("widget_dispatcher_loaded_target_required"),TEXT("Idle Editor and loaded exact Widget Blueprint required; no new loader."));
        World.Reset(GEditor->GetEditorWorldContext().World());Package.Reset(BP->GetPackage());Path=BP->GetPathName();
        if(!Context()||!Loaded(Package.Get())||BP->GetOuter()!=Package.Get()||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr().Get()!=UUserWidget::StaticClass()||BP->BlueprintType!=BPTYPE_Normal||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!BP->WidgetTree.IsResolved()||!BP->Bindings.IsEmpty()||!BP->Animations.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()||!BP->FunctionGraphs.IsEmpty()||!BP->MacroGraphs.IsEmpty()||BP->DelegateSignatureGraphs.Num()>1||BP->NewVariables.Num()>2||BP->UbergraphPages.Num()>1)
            return Fail(R,TEXT("widget_dispatcher_profile_pending"),TEXT("Loaded direct UserWidget, existing flat EventGraph, bounded native Canvas-root WidgetTree, no functions/macros/interfaces/animations/bindings, and at most one own dispatcher plus optional factory descriptor are admitted. Wider Widgets remain incomplete."));
        Parent.Reset(UUserWidget::StaticClass());ParentFlags=Parent->GetClassFlags();if(!Parent->ClassDefaultObject.IsResolved())return Fail(R,TEXT("widget_dispatcher_parent_pending"),TEXT("Already-resolved nullable native parent CDO required."));ParentCDO.Reset(Parent->ClassDefaultObject.Get());Tree.Reset(BP->WidgetTree.Get());if(!Loaded(Tree.Get())||Tree->GetOuter()!=BP.Get()||!Tree->RootWidget.IsResolved())return Fail(R,TEXT("widget_dispatcher_tree_pending"),TEXT("Resolved owned WidgetTree required."));
        Root.Reset(Cast<UCanvasPanel>(Tree->RootWidget.Get()));if(!Loaded(Root.Get())||Root->GetClass()!=UCanvasPanel::StaticClass()||Root->GetOuter()!=Tree.Get()||!Root->Slot.IsResolved()||Root->Slot.Get()!=nullptr)return Fail(R,TEXT("widget_dispatcher_canvas_pending"),TEXT("The bounded native UMG profile requires one owned Canvas root with no parent slot."));
        Gen.Reset(BP->GeneratedClass.GetGCPtr().Get());Skel.Reset(BP->SkeletonGeneratedClass.GetGCPtr().Get());if(!Loaded(Gen.Get())||!Loaded(Skel.Get())||Gen->GetClass()!=UWidgetBlueprintGeneratedClass::StaticClass()||!Skel->GetClass()->IsChildOf(UBlueprintGeneratedClass::StaticClass()))return Fail(R,TEXT("widget_dispatcher_generated_roles_pending"),TEXT("Actual Widget generated class and observed Blueprint skeleton subtype required."));
        GenMeta=Gen->GetClass();SkelMeta=Skel->GetClass();Holds.Emplace(GenMeta);Holds.Emplace(SkelMeta);GenPath=Gen->GetPathName();SkelPath=Skel->GetPathName();
        if(!ClassRole(Gen.Get(),GenMeta,GenPath)||!ClassRole(Skel.Get(),SkelMeta,SkelPath))return Fail(R,TEXT("widget_dispatcher_class_role_pending"),TEXT("Current class/source/outer roles required."));GenCDO.Reset(Gen->ClassDefaultObject.Get());SkelCDO.Reset(Skel->ClassDefaultObject.Get());if(!CDO(Gen.Get(),GenCDO.Get())||!CDO(Skel.Get(),SkelCDO.Get()))return Fail(R,TEXT("widget_dispatcher_cdo_role_pending"),TEXT("Existing nullable CDO roles required; none are constructed."));
        CompiledTreeField=FindFProperty<FObjectProperty>(UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("WidgetTree"));if(!CompiledTreeField)return Fail(R,TEXT("widget_dispatcher_compiled_tree_pending"),TEXT("Public reflected native compiled WidgetTree field required; no private member access."));CompiledTreeOffset=CompiledTreeField->GetOffset_ForInternal();
        if(!ReadWidgetTree(Tree.Get(),AuthoredTree))return Fail(R,TEXT("widget_dispatcher_authored_tree_pending"),TEXT("Bounded native UMG widget/slot ownership, closed ordered tree and reflected authored values required; custom/nested/opaque/bound trees remain incomplete."));
        if(!CaptureCompiledTrees(CompiledTrees))return Fail(R,TEXT("widget_dispatcher_compiled_tree_pending"),TEXT("Actual nullable compiled WidgetTree/root roles required."));
        TreeReady=true;
        TreePath=Tree->GetPathName();RootPath=Root->GetPathName();IsVariable=Root->bIsVariable;Visibility=Root->GetVisibility();Enabled=Root->GetIsEnabled();Opacity=Root->GetRenderOpacity();Transform=Root->GetRenderTransform();Pivot=Root->GetRenderTransformPivot();Clipping=Root->GetClipping();
        for(const auto* Category:{&BP->UbergraphPages,&BP->FunctionGraphs,&BP->MacroGraphs,&BP->DelegateSignatureGraphs})for(const auto& H:*Category)if(!H.IsResolved()||!Loaded(H.Get()))return Fail(R,TEXT("widget_dispatcher_graph_slot_pending"),TEXT("Authored graph slots must already be resolved."));
        Uber=BP->UbergraphPages;Functions=BP->FunctionGraphs;Macros=BP->MacroGraphs;Delegates=BP->DelegateSignatureGraphs;Variables=BP->NewVariables;WidgetGuids=BP->WidgetVariableNameToGuidMap;
        if(!UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.IsResolved())return Fail(R,TEXT("widget_dispatcher_save_policy_pending"),TEXT("Already-resolved Editor save policy required."));Policy.Reset(Cast<UBlueprintEditorSettings>(UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.Get()));
        if(!Policy.IsValid()||Policy->SaveOnCompile!=SoC_Never)return Fail(R,TEXT("widget_dispatcher_save_policy_pending"),TEXT("Existing SaveOnCompile must be Never; settings are not changed."));
        if(!Current(TEXT("widget_dispatcher_bind"))||!CaptureGraphs(Graphs)||!Current(TEXT("widget_dispatcher_capture"))||!ObserveDelegateSources(Graphs,TEXT("create_delegate_initial_sources"))||!Displays(Graphs,TEXT("widget_dispatcher_initial_text")))return Stopped?false:Bad(TEXT("bind"),TEXT("initial_graph_capture"));
        R.Data->SetBoolField(TEXT("widget_dispatcher_contract"),true);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"),true);R.Data->SetStringField(TEXT("implicit_compile_scope"),TEXT("Original member/category/metadata/structural helpers may regenerate skeleton/class/CDO; actual explicit full compile and save remain zero."));R.Data->SetStringField(TEXT("blueprint"),Path);R.Data->SetBoolField(TEXT("explicit_compile_requested"),false);R.Data->SetBoolField(TEXT("explicit_save_requested"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
        R.Data->SetStringField(TEXT("widget_preservation_scope"),TEXT("Captured physical native UMG source widgets/slots, ordered reciprocal tree, reflected authored values and valid Widget GUID map; prior variables/graph pin/default/link values remain protected. Structural callbacks reacquire generated classes/nullable CDOs and compiled trees against copied prior-cache or protected source projection. Native opaque/custom/nested/bound widgets, private Slate/render caches and complete runtime state remain incomplete."));return true;
    }
    bool SelectGraph(const FString& Name,const FString& Id,const FString& Kind,bool Create,UEdGraph*& Out)
    {
        R.Data->SetBoolField(TEXT("requested_create_graph_if_missing"),Create);R.Data->SetBoolField(TEXT("graph_created"),false);
        FGuid Guid;if(!Id.IsEmpty()&&!FGuid::Parse(Id,Guid))return Fail(R,TEXT("function_graph_id_invalid"),TEXT("Graph ID must be a valid GUID."));
        for(const auto& H:Uber){if(!H.IsResolved())return Bad(TEXT("resolve"),TEXT("event_graph_slot"));auto* G=H.Get();if((!Id.IsEmpty()?G->GraphGuid==Guid:(Name.IsEmpty()||G->GetName().Equals(Name,ESearchCase::CaseSensitive)))&&(Kind.IsEmpty()||Kind.Equals(TEXT("event"),ESearchCase::IgnoreCase))){Out=G;break;}}
        if(!Out)return Fail(R,TEXT("widget_dispatcher_missing_graph_pending"),TEXT("Only an existing owned EventGraph is admitted. True reuses it; actual missing graph creation remains incomplete."));return Current(TEXT("widget_dispatcher_graph_selected"));
    }
    TSharedRef<FJsonObject> NodeJson(UEdGraphNode* Node,const TCHAR* Stage)
    {
        auto J=MakeShared<FJsonObject>();if(!Current(Stage))return J;
        const FNode* Role=nullptr;for(const auto& G:Graphs)for(const auto& N:G.Nodes)if(N.Hold.Get()==Node)Role=&N;
        if(!Role){Bad(Stage,TEXT("published_node_role"));return J;}
        J->SetStringField(TEXT("node_id"),Role->Id.ToString());J->SetStringField(TEXT("node_guid"),Role->Id.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("object_path"),Role->Path);J->SetStringField(TEXT("node_name"),FPackageName::ObjectPathToObjectName(Role->Path));J->SetStringField(TEXT("node_class"),Role->Class->GetPathName());J->SetNumberField(TEXT("x"),Role->X);J->SetNumberField(TEXT("y"),Role->Y);
        if(Role->ReferenceKind==6){J->SetStringField(TEXT("function_name"),Role->DelegateName.ToString());J->SetStringField(TEXT("selected_function_guid"),Role->DelegateGuid.ToString());J->SetBoolField(TEXT("create_delegate_captured_role"),true);}
        TArray<TSharedPtr<FJsonValue>> Pins;for(const auto& P:Role->Pins){auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),P.Name.ToString());Q->SetStringField(TEXT("pin_id"),P.Id.ToString(EGuidFormats::Digits));Q->SetStringField(TEXT("direction"),P.Direction==EGPD_Input?TEXT("input"):TEXT("output"));auto Type=Pin::Describe(P.Type);if(!Current(Stage))return J;Q->SetObjectField(TEXT("pin_type"),Type);Q->SetStringField(TEXT("category"),P.Type.PinCategory.ToString());Q->SetStringField(TEXT("subcategory"),P.Type.PinSubCategory.ToString());Q->SetBoolField(TEXT("is_array"),P.Type.IsArray());Q->SetStringField(TEXT("default_value"),P.Raw);Q->SetStringField(TEXT("default_text"),P.Display);Q->SetStringField(TEXT("default_object"),P.Default?P.Default->GetPathName():FString());Q->SetArrayField(TEXT("linked_to"),{});Pins.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("pins"),Pins);return J;
    }
    bool FreezeNew(const TCHAR* Stage)
    {
        if(!Current(Stage))return false;
        TArray<FGraph> All;if(!CaptureGraphs(All,true))return Bad(Stage,TEXT("final_selected_inventory"));
        // Old protected graphs were already checked; only selected new roles are admitted here.
        if(NodeMember){for(auto& G:All)if(G.Hold.Get()==NewGraph){const auto* Old=Graphs.FindByPredicate([&](const FGraph& Row){return Row.Hold.Get()==NewGraph;});if(!Old||G.Nodes.Num()!=Old->Nodes.Num()+1||G.Nodes.Last().Hold.Get()!=NewNode)return Bad(Stage,TEXT("final_node_membership"));}NodeMember=false;}
        CopyDelegateSources(Graphs,All);Graphs=MoveTemp(All);if(!Current(Stage)||!Displays(Graphs,Stage))return false;return true;
    }
};

bool WidgetDispatcherActive=false;
bool WidgetPath(const FString& Path)
{
    if(!Path.StartsWith(TEXT("/Game/"))||!Function::Pin::SafeName(Path)||!ReferenceGuard(Path))return false;
    const FString ObjectPath=Path.Contains(TEXT("."))?Path:Path+TEXT(".")+FPackageName::GetShortName(Path);return FindObject<UWidgetBlueprint>(nullptr,*ObjectPath)!=nullptr;
}
FResult WidgetDeclare(const FString& Path,const FString& Requested,const FString& InputsJson,const FString& Category,const FString& Tooltip,const FString& Friendly)
{
    FResult R;Init(R);if(WidgetDispatcherActive){Fail(R,TEXT("widget_dispatcher_busy"),TEXT("Request-long Widget dispatcher writer is active; no reentry."));return R;}TGuardValue<bool> Active(WidgetDispatcherActive,true);
    FString Name;if(!DispatcherName(Requested,Name,R))return R;FWidgetDispatcherWork W(R);if(!W.Begin(Path,FName(*Name)))return R;
    const FName Member(*Name);for(const auto& V:W.BP->NewVariables)if(V.VarName==Member){Fail(R,TEXT("dispatcher_already_exists"),TEXT("Event dispatcher/member already exists."));return R;}
    for(const auto& G:W.Graphs)if(G.Hold->GetName().Equals(Name,ESearchCase::IgnoreCase)){Fail(R,TEXT("dispatcher_graph_conflict"),TEXT("Existing graph name conflicts with dispatcher."));return R;}
    if(FindObject<UObject>(W.BP.Get(),*Name)){Fail(R,TEXT("dispatcher_outer_object_conflict"),TEXT("Owned name collision refused before CreateNewGraph rename effects."));return R;}
    FText NameError;const bool ValidName=Member.IsValidObjectName(NameError);if(!W.Current(TEXT("name_validation")))return R;if(!ValidName){const FString Error=NameError.ToString();if(!W.Current(TEXT("name_error_text")))return R;Fail(R,TEXT("dispatcher_graph_name_unrepresentable"),Error);return R;}
    auto Run=[&]()->FResult
    {
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","WidgetDispatcherDeclare","Add Widget Event Dispatcher"));if(!W.Current(TEXT("transaction_begin")))return R;
    FEdGraphPinType DelegateType;DelegateType.PinCategory=UEdGraphSchema_K2::PC_MCDelegate;const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();if(!W.Current(TEXT("schema_return")))return R;
    R.Stage=TEXT("add_member_variable");R.bMutationAttempted=true;R.bChangeStateKnown=false;W.Writer=true;W.Rebinding=true;W.ClassesReady=false;
    Count(R,TEXT("member_add_calls"));const bool Added=FBlueprintEditorUtils::AddMemberVariable(W.BP.Get(),Member,DelegateType);Count(R,TEXT("member_add_returns"));R.Data->SetBoolField(TEXT("member_add_returned"),Added);W.MemberAdded=Added;
    if(Added){if(!W.Current(TEXT("new_member_initial_roles")))return R;W.SelectedPlan=W.BP->NewVariables.Last();W.SelectedPlanReady=true;}
    if(!W.Refresh(TEXT("member_add_return")))return R;if(!Added){Fail(R,TEXT("dispatcher_member_add_failed"),TEXT("AddMemberVariable returned false; actual prefix retained."));return R;}
    R.bChanged=true;R.Data->SetStringField(TEXT("dispatcher_name"),Name);
    R.Stage=TEXT("create_signature_graph");Count(R,TEXT("signature_graph_create_calls"));UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(W.BP.Get(),Member,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());TStrongObjectPtr<UEdGraph> Returned(Graph);Count(R,TEXT("signature_graph_create_returns"));W.NewGraph=Graph;if(Graph)W.Holds.Emplace(Graph);if(!W.Current(TEXT("create_signature_return")))return R;
    // Original cleanup runs only while identity/current remains known. Unknown never enters cleanup.
    auto Cleanup=[&](const TCHAR* Code,const FString& Error)->FResult
    {
        if(!W.Current(TEXT("known_cleanup_begin")))return R;R.Data->SetBoolField(TEXT("cleanup_attempted"),true);
        if(Graph)
        {
            W.Graphs.RemoveAll([&](const FWidgetDispatcherWork::FGraph& Row){return Row.Hold.Get()==Graph;});W.NewNode=nullptr;W.NewGraph=nullptr;W.GraphAdded=false;W.Rebinding=true;W.ClassesReady=false;Count(R,TEXT("remove_graph_calls"));FBlueprintEditorUtils::RemoveGraph(W.BP.Get(),Graph);Count(R,TEXT("remove_graph_returns"));
            if(!W.Refresh(TEXT("cleanup_graph_return")))return R;Graph=nullptr;
        }
        W.SelectedPlanReady=false;W.MemberAdded=false;W.Rebinding=true;W.ClassesReady=false;Count(R,TEXT("remove_member_variable_calls"));FBlueprintEditorUtils::RemoveMemberVariable(W.BP.Get(),Member);Count(R,TEXT("remove_member_variable_returns"));if(!W.Refresh(TEXT("cleanup_member_return")))return R;
        R.bChanged=true;R.bChangeStateKnown=false;Fail(R,Code,Error);return R;
    };
    if(!Graph)return Cleanup(TEXT("dispatcher_signature_graph_failed"),TEXT("CreateNewGraph returned null; original cleanup prefix retained."));Graph->bEditable=false;
    Count(R,TEXT("default_nodes_calls"));Schema->CreateDefaultNodesForGraph(*Graph);if(!W.Current(TEXT("default_nodes_return")))return R;
    Count(R,TEXT("function_terminators_calls"));Schema->CreateFunctionGraphTerminators(*Graph,static_cast<UClass*>(nullptr));if(!W.Current(TEXT("terminators_return")))return R;
    Count(R,TEXT("function_flags_calls"));Schema->AddExtraFunctionFlags(Graph,FUNC_BlueprintCallable|FUNC_BlueprintEvent|FUNC_Public);if(!W.Current(TEXT("function_flags_return")))return R;
    Count(R,TEXT("entry_editable_calls"));Schema->MarkFunctionEntryAsEditable(Graph,true);if(!W.Current(TEXT("entry_editable_return")))return R;
    Count(R,TEXT("signature_graph_add_calls"));W.BP->DelegateSignatureGraphs.Add(Graph);W.GraphAdded=true;if(!W.Current(TEXT("signature_graph_added")))return R;
    UK2Node_FunctionEntry* Entry=Cast<UK2Node_FunctionEntry>(FBlueprintEditorUtils::GetEntryNode(Graph));TStrongObjectPtr<UK2Node_FunctionEntry> KeepEntry(Entry);if(!W.Current(TEXT("entry_return")))return R;if(!Entry)return Cleanup(TEXT("dispatcher_entry_missing"),TEXT("Signature entry node not found."));W.NewNode=Entry;W.NewNodeClass=UK2Node_FunctionEntry::StaticClass();W.Holds.Emplace(Entry);if(!W.Current(TEXT("entry_role_fixed")))return R;
    TSharedPtr<FJsonValue> Inputs;if(!InputsJson.IsEmpty()&&!Function::Pin::ParseJson(InputsJson,Inputs,R))return Cleanup(TEXT("dispatcher_inputs_json_invalid"),R.Error);
    TArray<TSharedPtr<FJsonValue>> Values;if(Inputs.IsValid()&&Inputs->Type==EJson::Array)Values=Inputs->AsArray();if(Values.Num()>16)return Cleanup(TEXT("dispatcher_input_budget"),TEXT("Widget primitive signature bound16; broader signatures remain incomplete."));
    TArray<FName> Names;TArray<FEdGraphPinType> Types;TArray<TSharedPtr<FJsonValue>> Created;
    for(const auto& Value:Values)
    {
        if(!Value.IsValid()||Value->Type!=EJson::Object)return Cleanup(TEXT("dispatcher_input_object_invalid"),TEXT("Event dispatcher input must be an object."));const auto J=Value->AsObject();FString Param,RequestedType;
        if(!J->TryGetStringField(TEXT("parameter_name"),Param)&&!J->TryGetStringField(TEXT("name"),Param))return Cleanup(TEXT("dispatcher_input_name_missing"),TEXT("Missing input name."));Param.TrimStartAndEndInline();for(TCHAR Character:Param)if(Character==0)return Cleanup(TEXT("dispatcher_input_name_invalid"),TEXT("Interior NUL parameter name refused."));
        if(Param.IsEmpty()||Param.Len()>=NAME_SIZE||FName(*Param).IsNone()||Names.Contains(FName(*Param)))return Cleanup(TEXT("dispatcher_input_name_invalid"),TEXT("Unique nonempty bounded input FName required."));
        if(!J->TryGetStringField(TEXT("parameter_type"),RequestedType)&&!J->TryGetStringField(TEXT("type"),RequestedType))return Cleanup(TEXT("dispatcher_input_type_missing"),TEXT("Missing input type."));
        FEdGraphPinType Type;const bool ParsedType=InputType(RequestedType,J,Type,R);if(!W.Current(TEXT("input_type_return")))return R;if(!ParsedType)return Cleanup(*R.ErrorCode,R.Error);if(Type.ContainerType!=EPinContainerType::None||Type.bIsReference||Type.PinSubCategoryObject.IsValid()||(Type.PinCategory!=UEdGraphSchema_K2::PC_Int&&Type.PinCategory!=UEdGraphSchema_K2::PC_Int64&&Type.PinCategory!=UEdGraphSchema_K2::PC_Boolean&&Type.PinCategory!=UEdGraphSchema_K2::PC_Real&&Type.PinCategory!=UEdGraphSchema_K2::PC_Name&&Type.PinCategory!=UEdGraphSchema_K2::PC_String))return Cleanup(TEXT("widget_dispatcher_signature_pending"),TEXT("Only primitive scalar by-value Widget inputs are admitted; broader native path remains unchanged."));
        FText Error;Count(R,TEXT("user_pin_can_create_calls"));const bool Can=Entry->CanCreateUserDefinedPin(Type,EGPD_Output,Error);if(!W.Current(TEXT("can_create_pin_return")))return R;if(!Can){const FString Message=Error.ToString();if(!W.Current(TEXT("pin_error_string")))return R;return Cleanup(TEXT("dispatcher_input_pin_refused"),Message);}
        Count(R,TEXT("user_pin_create_calls"));UEdGraphPin* Pin=Entry->CreateUserDefinedPin(FName(*Param),Type,EGPD_Output,false);Count(R,TEXT("user_pin_create_returns"));if(!W.Current(TEXT("create_pin_return")))return R;if(!Pin)return Cleanup(TEXT("dispatcher_input_pin_failed"),TEXT("User pin creation returned null."));
        Names.Add(FName(*Param));Types.Add(Type);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("parameter_name"),Param);Row->SetStringField(TEXT("parameter_type"),RequestedType);Row->SetObjectField(TEXT("pin_type"),PinTypeToJson(Type));Created.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("created_input_prefix"),Created);
    }
    if(!Created.IsEmpty()){Count(R,TEXT("entry_reconstruct_calls"));Entry->ReconstructNode();if(!W.Current(TEXT("entry_reconstruct_return")))return R;}
    if(!W.FreezeNew(TEXT("signature_allocated_roles")))return R;
    if(!Category.IsEmpty())
    {
        auto* V=Variable(W.BP.Get(),Member);if(!V)return W.Bad(TEXT("category"),TEXT("selected_descriptor")),R;const FText CategoryText=FText::FromString(Category);V->Category=CategoryText;W.SelectedPlan.Category=CategoryText;W.MutableCategory=true;Count(R,TEXT("category_write_calls"));
        W.Rebinding=true;W.ClassesReady=false;Count(R,TEXT("category_helper_calls"));FBlueprintEditorUtils::SetBlueprintVariableCategory(W.BP.Get(),Member,nullptr,CategoryText,true);if(!W.Refresh(TEXT("category_helper_return")))return R;
        auto* Updated=Variable(W.BP.Get(),Member);if(!Updated)return W.Bad(TEXT("category_return"),TEXT("selected_descriptor")),R;W.SelectedPlan.Category=Updated->Category;W.MutableCategory=false;
        const FText Copy=W.SelectedPlan.Category;const FString Display=Copy.ToString();if(!W.Current(TEXT("category_return_text")))return R;if(!Display.Equals(Category,ESearchCase::CaseSensitive))return W.Bad(TEXT("category_return"),TEXT("selected_category"),Category,Display),R;
    }
    if(!Tooltip.IsEmpty())
    {
        auto* V=Variable(W.BP.Get(),Member);if(!V)return W.Bad(TEXT("tooltip"),TEXT("selected_descriptor")),R;V->SetMetaData(TEXT("Tooltip"),Tooltip);W.SelectedPlan.SetMetaData(TEXT("Tooltip"),Tooltip);Count(R,TEXT("tooltip_descriptor_write_calls"));
        W.Rebinding=true;W.ClassesReady=false;Count(R,TEXT("tooltip_helper_calls"));FBlueprintEditorUtils::SetBlueprintVariableMetaData(W.BP.Get(),Member,nullptr,TEXT("Tooltip"),Tooltip);if(!W.Refresh(TEXT("tooltip_helper_return")))return R;
    }
    if(!Friendly.IsEmpty()){auto* V=Variable(W.BP.Get(),Member);if(!V)return W.Bad(TEXT("friendly"),TEXT("selected_descriptor")),R;V->FriendlyName=Friendly;W.SelectedPlan.FriendlyName=Friendly;Count(R,TEXT("friendly_name_write_calls"));}
    W.Rebinding=true;W.ClassesReady=false;R.Stage=TEXT("mark_structural");Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(W.BP.Get());Count(R,TEXT("mark_structural_returns"));if(!W.Refresh(TEXT("mark_structural_return"))||!W.AcquireSource(TEXT("signature_source")))return R;
    TArray<FName> ActualNames;TArray<FEdGraphPinType> ActualTypes;if(!W.SignatureTypes(ActualNames,ActualTypes,TEXT("signature_projection")))return R;
    bool Verified=Names==ActualNames&&Types.Num()==ActualTypes.Num()&&FreshPins(Entry)&&Graph->Nodes.Num()==1&&Entry->bIsEditable&&!Graph->bEditable;
    for(int32 I=0;I<Types.Num()&&I<ActualTypes.Num();++I){const auto* Pin=Entry->FindPin(Names[I],EGPD_Output);Verified&=Pin&&PinsMatchType(Pin->PinType,Types[I])&&Types[I].PinCategory==ActualTypes[I].PinCategory&&Types[I].PinSubCategory==ActualTypes[I].PinSubCategory;}
    auto* V=Variable(W.BP.Get(),Member);if(!V)Verified=false;else
    {
        const FBPVariableDescription ActualDescriptor=*V;const FText Copy=ActualDescriptor.Category;const FString ActualCategory=Copy.ToString();if(!W.Current(TEXT("selected_category_text")))return R;
        Verified&=(Category.IsEmpty()||ActualCategory.Equals(Category,ESearchCase::CaseSensitive))&&(Tooltip.IsEmpty()||(ActualDescriptor.HasMetaData(TEXT("Tooltip"))&&ActualDescriptor.GetMetaData(TEXT("Tooltip")).Equals(Tooltip,ESearchCase::CaseSensitive)))&&(Friendly.IsEmpty()||ActualDescriptor.FriendlyName.Equals(Friendly,ESearchCase::CaseSensitive));
    }
    if(!Verified){Fail(R,TEXT("dispatcher_declaration_postcondition_failed"),TEXT("Actual Widget signature/property/metadata projection differs; actual creation prefix retained, no cleanup after original success boundary."));return R;}
    if(!W.FreezeNew(TEXT("declaration_final_roles")))return R;
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("dispatcher_name"),Name);Legacy->SetObjectField(TEXT("pin_type"),PinTypeToJson(DelegateType));auto LegacyGraph=DescribeGraph(W.BP.Get(),Graph);LegacyGraph->SetStringField(TEXT("graph_id"),Graph->GraphGuid.ToString());LegacyGraph->RemoveField(TEXT("object_path"));Legacy->SetObjectField(TEXT("signature_graph"),LegacyGraph);Legacy->SetArrayField(TEXT("inputs"),Created);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetArrayField(TEXT("inputs"),Created);R.Data->SetStringField(TEXT("signature_function"),W.Signature->GetPathName());R.Data->SetBoolField(TEXT("signature_inputs_verified"),true);R.Data->SetBoolField(TEXT("dispatcher_metadata_verified"),true);R.Data->SetBoolField(TEXT("current_signature_names_value_types_verified"),true);R.Data->SetBoolField(TEXT("old_variables_graphs_preserved"),true);
    if(!W.Current(TEXT("declaration_publication")))return R;R.bChanged=R.bChangeStateKnown=R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
    };
    Run();if(!W.Stopped&&R.bChangeStateKnown)W.Current(TEXT("transaction_publication"));return R;
}
FResult WidgetNode(bool Bind,const FString& Path,const FString& Requested,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Kind,bool Create)
{
    FResult R;Init(R);if(WidgetDispatcherActive){Fail(R,TEXT("widget_dispatcher_busy"),TEXT("Request-long Widget dispatcher writer is active; no reentry."));return R;}TGuardValue<bool> Active(WidgetDispatcherActive,true);
    FString Dispatcher;if(!DispatcherName(Requested,Dispatcher,R))return R;int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R))return R;
    FWidgetDispatcherWork W(R);if(!W.Begin(Path,FName(*Dispatcher)))return R;UEdGraph* Graph=nullptr;if(!W.SelectGraph(Name,Id,Kind,Create,Graph)||!W.AcquireSource(TEXT("node_signature_bind")))return R;W.NewGraph=Graph;
    TArray<FName> Names;TArray<FEdGraphPinType> Types;if(!W.SignatureTypes(Names,Types,TEXT("node_signature_types")))return R;
    const TCHAR* ClassPath=Bind?TEXT("/Script/BlueprintGraph.K2Node_AddDelegate"):TEXT("/Script/BlueprintGraph.K2Node_CallDelegate");UClass* Class=FindObject<UClass>(nullptr,ClassPath);TStrongObjectPtr<UClass> ClassHold(Class);
    if(!W.Current(TEXT("node_class_lookup_return")))return R;if(!Loaded(Class)||Class->GetSuperClass()!=UK2Node_BaseMCDelegate::StaticClass()||!Class->HasAnyClassFlags(CLASS_Native)||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)){Fail(R,TEXT("dispatcher_node_class_unavailable"),TEXT("Exact already-loaded native Call/AddDelegate node class required."));return R;}
    auto Run=[&]()->FResult
    {
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","WidgetDispatcherNode","Add Widget Dispatcher Call or Bind"));if(!W.Current(TEXT("node_transaction_begin")))return R;
    R.Stage=TEXT("allocate_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;W.Writer=true;Count(R,TEXT("node_allocation_calls"));UK2Node_BaseMCDelegate* Node=NewObject<UK2Node_BaseMCDelegate>(Graph,Class);TStrongObjectPtr<UK2Node_BaseMCDelegate> Hold(Node);Count(R,TEXT("node_allocation_returns"));W.NewNode=Node;W.NewNodeClass=Class;if(Node)W.Holds.Emplace(Node);if(!W.Current(TEXT("node_allocation_return")))return R;if(!Node){Fail(R,TEXT("dispatcher_node_allocation_failed"),TEXT("NewObject returned null; actual allocation prefix retained."));return R;}
    R.Data->SetStringField(TEXT("allocated_node_path"),Node->GetPathName());R.Data->SetBoolField(TEXT("self_context"),true);
    Count(R,TEXT("set_from_property_calls"));W.NewReference.SetFromField<FProperty>(W.Property,true,W.PropertyOwner.Get());if(!W.Current(TEXT("planned_reference")))return R;Node->SetFromProperty(W.Property,true,W.PropertyOwner.Get());W.NewReferenceReady=true;Count(R,TEXT("set_from_property_returns"));if(!W.Current(TEXT("set_from_property_return")))return R;
    Count(R,TEXT("compatibility_calls"));const bool Compatible=Node->IsCompatibleWithGraph(Graph);R.Data->SetBoolField(TEXT("graph_compatible"),Compatible);if(!W.Current(TEXT("compatibility_return")))return R;if(!Compatible){Fail(R,TEXT("dispatcher_graph_incompatible"),TEXT("Original late compatibility refusal after allocation/reference assignment; no added cleanup."));return R;}
    Node->NodePosX=XY[0];Node->NodePosY=XY[1];Count(R,TEXT("graph_add_node_calls"));Graph->AddNode(Node,true);W.NodeMember=true;if(!W.Current(TEXT("graph_add_node_return")))return R;
    Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();if(!W.Current(TEXT("guid_return")))return R;R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());R.Data->SetStringField(TEXT("created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));
    Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();if(!W.Current(TEXT("post_placed_return")))return R;
    Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();if(!W.Current(TEXT("allocate_pins_return")))return R;
    Count(R,TEXT("reconstruct_calls"));Node->ReconstructNode();if(!W.Current(TEXT("reconstruct_return")))return R;
    // Fix the returned new physical pins before callback-capable notification/observation.
    if(!W.FreezeNew(TEXT("returned_node_roles")))return R;
    UClass* Authoritative=W.PropertyOwner->GetAuthoritativeClass();TStrongObjectPtr<UClass> KeepAuthoritative(Authoritative);if(!W.Current(TEXT("authoritative_class_return")))return R;
    auto* Execute=Node->FindPin(UEdGraphSchema_K2::PN_Execute,EGPD_Input);auto* Then=Node->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output);auto* Target=Node->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
    bool Good=FreshPins(Node)&&Execute&&Then&&Target&&Execute->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Then->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Target->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Target->PinType.PinSubCategoryObject.Get()==Authoritative;
    if(Bind)
    {
        auto* Delegate=Node->GetDelegatePin();FSimpleMemberReference Ref;FMemberReference::FillSimpleMemberReference<UFunction>(W.Signature.Get(),Ref);if(!W.Current(TEXT("delegate_member_reference_return")))return R;
        Good&=Node->Pins.Num()==4&&Delegate&&Delegate->Direction==EGPD_Input&&Delegate->PinType.PinCategory==UEdGraphSchema_K2::PC_Delegate&&Delegate->PinType.bIsConst&&Delegate->PinType.bIsReference&&Delegate->PinType.PinSubCategoryMemberReference==Ref;
    }
    else{Good&=Node->Pins.Num()==3+Names.Num();for(int32 I=0;I<Names.Num();++I){const auto* Pin=Node->FindPin(Names[I],EGPD_Input);Good&=Pin&&PinsMatchType(Pin->PinType,Types[I]);}}
    FMemberReference Expected;Expected.SetFromField<FProperty>(W.Property,true,W.PropertyOwner.Get());Good&=Node->DelegateReference.IsSameReference(Expected);
    R.Data->SetNumberField(TEXT("node_property_resolve_calls"),1);const FProperty* Resolved=Node->GetProperty();if(!W.Current(TEXT("node_property_resolve_return")))return R;
    R.Data->SetBoolField(TEXT("node_property_resolved_to_selected"),Resolved==W.Property);Good&=Resolved==W.Property;
    R.Data->SetNumberField(TEXT("node_signature_resolve_calls"),1);UFunction* ResolvedSignature=Node->GetDelegateSignature();TStrongObjectPtr<UFunction> HoldResolvedSignature(ResolvedSignature);if(!W.Current(TEXT("node_signature_resolve_return")))return R;
    R.Data->SetBoolField(TEXT("node_signature_resolved_to_selected"),ResolvedSignature==W.Signature.Get());Good&=ResolvedSignature==W.Signature.Get();
    if(!Good){Fail(R,TEXT("dispatcher_node_postcondition_failed"),TEXT("Actual property/self binding or specialized typed pins differ; accepted prefix retained, no automatic cleanup."));return R;}
    if(!W.FreezeNew(TEXT("new_node_roles_frozen")))return R;
    R.Stage=TEXT("mark_modified");Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(W.BP.Get());Count(R,TEXT("mark_blueprint_modified_returns"));if(!W.Current(TEXT("mark_modified_return")))return R;
    const FText Title=Node->GetNodeTitle(ENodeTitleType::FullTitle);if(!W.Current(TEXT("node_title_return")))return R;const FString TitleString=Title.ToString();if(!W.Current(TEXT("node_title_text_return")))return R;
    auto Row=W.NodeJson(Node,TEXT("node_description"));if(W.Stopped)return R;Row->SetStringField(TEXT("title"),TitleString);Row->SetStringField(TEXT("name"),Node->GetName());Row->SetStringField(TEXT("class"),Node->GetClass()->GetName());Row->SetStringField(TEXT("node_comment"),Node->NodeComment);auto Position=MakeShared<FJsonObject>();Position->SetNumberField(TEXT("x"),XY[0]);Position->SetNumberField(TEXT("y"),XY[1]);Row->SetObjectField(TEXT("position"),Position);if(!W.Current(TEXT("node_description_return")))return R;R.Data->SetObjectField(TEXT("node"),Row);R.Data->SetStringField(TEXT("signature_function"),W.Signature->GetPathName());R.Data->SetBoolField(TEXT("specialized_pins_verified"),true);R.Data->SetBoolField(TEXT("native_class_signature_preserved"),true);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),true);R.Data->SetBoolField(TEXT("graph_rows_preserved"),true);R.Data->SetStringField(TEXT("dispatcher_name"),Dispatcher);
    auto Legacy=MakeShared<FJsonObject>();Legacy->Values=Row->Values;Legacy->SetStringField(TEXT("dispatcher_name"),Dispatcher);Legacy->SetStringField(TEXT("signature_function"),W.Signature->GetPathName());auto LegacyGraph=DescribeGraph(W.BP.Get(),Graph);LegacyGraph->SetStringField(TEXT("graph_id"),Graph->GraphGuid.ToString());LegacyGraph->RemoveField(TEXT("object_path"));Legacy->SetObjectField(TEXT("graph"),LegacyGraph);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetObjectField(TEXT("graph"),DescribeGraph(W.BP.Get(),Graph));if(!W.Current(TEXT("node_publication")))return R;
    R.bChanged=R.bChangeStateKnown=R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
    };
    Run();if(!W.Stopped&&R.bChangeStateKnown)W.Current(TEXT("transaction_publication"));return R;
}

FResult Declare(const FString& Path,const FString& Requested,const FString& InputsJson,const FString& Category,const FString& Tooltip,const FString& Friendly)
{
    FResult R;Init(R);FString Name;if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("dispatcher_editor_invalid"),TEXT("Idle Editor game thread required."));return R; }Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","DeclareDispatcher","Add Blueprint Event Dispatcher"));if(!DispatcherName(Requested,Name,R)) { return R; }
    auto* BP=LoadBlueprint(Path,R);if(!BP||!OwnerScope(BP,nullptr,R)) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);const FName FNameValue(*Name);R.Data->SetStringField(TEXT("dispatcher_name"),Name);
    FText NameError;if(!FNameValue.IsValidObjectName(NameError)) { Fail(R,TEXT("dispatcher_graph_name_unrepresentable"),NameError.ToString());return R; }
    if(Variable(BP,FNameValue)) { Fail(R,TEXT("dispatcher_already_exists"),TEXT("Event dispatcher/member variable already exists."));return R; }TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(UEdGraph* G:Graphs) { if(G&&G->GetName().Equals(Name,ESearchCase::IgnoreCase)) { Fail(R,TEXT("dispatcher_graph_conflict"),TEXT("Blueprint graph already exists with dispatcher name."));return R; } }
    if(FindObject<UObject>(BP,*Name)) { Fail(R,TEXT("dispatcher_outer_object_conflict"),TEXT("An existing owned object name would enter CreateNewGraph rename/collision effects; finite scope refuses before member writer."));return R; }
    TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!BlueprintState(BP,Before,R)) { return R; }R.Data->SetObjectField(TEXT("before"),Before);const auto ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);
    FEdGraphPinType DelegateType;DelegateType.PinCategory=UEdGraphSchema_K2::PC_MCDelegate;const auto* Schema=GetDefault<UEdGraphSchema_K2>();R.Stage=TEXT("add_member_variable");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("member_add_calls"));const bool Added=FBlueprintEditorUtils::AddMemberVariable(BP,FNameValue,DelegateType);Count(R,TEXT("member_add_returns"));R.Data->SetBoolField(TEXT("member_add_returned"),Added);R.Data->SetObjectField(TEXT("after_member_add_variables"),Variables(BP,R));
    if(!Added) { TSharedRef<FJsonObject> A=MakeShared<FJsonObject>();if(BlueprintState(BP,A,R)) { R.Data->SetObjectField(TEXT("after"),A);R.bChanged=!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true; }Fail(R,TEXT("dispatcher_member_add_failed"),TEXT("Original AddMemberVariable returnedFalse; returned state retained."));return R; }
    R.Stage=TEXT("create_signature_graph");Count(R,TEXT("signature_graph_create_calls"));UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,FNameValue,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());Count(R,TEXT("signature_graph_create_returns"));const TStrongObjectPtr<UEdGraph> KeepGraph(G);R.Data->SetStringField(TEXT("created_signature_graph_path"),GetPathNameSafe(G));
    auto Cleanup=[&](FString Code,FString Error)->FResult
    {
        R.Stage=TEXT("declaration_cleanup");R.Data->SetBoolField(TEXT("cleanup_attempted"),true);if(G) { Count(R,TEXT("remove_graph_calls"));FBlueprintEditorUtils::RemoveGraph(BP,G);Count(R,TEXT("remove_graph_returns"));R.Data->SetStringField(TEXT("removed_graph_current_path"),GetPathNameSafe(G));R.Data->SetBoolField(TEXT("removed_graph_transient"),G->HasAnyFlags(RF_Transient)); }
        Count(R,TEXT("remove_member_variable_calls"));FBlueprintEditorUtils::RemoveMemberVariable(BP,FNameValue);Count(R,TEXT("remove_member_variable_returns"));R.Data->SetBoolField(TEXT("cleanup_variable_absent"),Variable(BP,FNameValue)==nullptr);R.Data->SetBoolField(TEXT("cleanup_graph_detached"),!G||!BP->DelegateSignatureGraphs.Contains(G));TSharedRef<FJsonObject> A=MakeShared<FJsonObject>();if(BlueprintState(BP,A,R)) { R.Data->SetObjectField(TEXT("cleanup_current"),A);R.Data->SetBoolField(TEXT("cleanup_selected_descriptors_graphs_restored"),OldVariables(Before,A,0)&&Equal(MakeShared<FJsonValueObject>(StableGraphs(Before,TEXT(""))),MakeShared<FJsonValueObject>(StableGraphs(A,TEXT(""))))); }else { R.Data->SetObjectField(TEXT("cleanup_current_incomplete"),A); }
        R.bChanged=true;R.bChangeStateKnown=false;Fail(R,*Code,Error);return R;
    };
    if(!G) { return Cleanup(TEXT("dispatcher_signature_graph_failed"),TEXT("Failed to create signature graph; original removal helper prefix retained.")); }
    G->bEditable=false;Count(R,TEXT("default_nodes_calls"));Schema->CreateDefaultNodesForGraph(*G);Count(R,TEXT("function_terminators_calls"));Schema->CreateFunctionGraphTerminators(*G,static_cast<UClass*>(nullptr));Count(R,TEXT("function_flags_calls"));Schema->AddExtraFunctionFlags(G,FUNC_BlueprintCallable|FUNC_BlueprintEvent|FUNC_Public);Count(R,TEXT("entry_editable_calls"));Schema->MarkFunctionEntryAsEditable(G,true);Count(R,TEXT("signature_graph_add_calls"));BP->DelegateSignatureGraphs.Add(G);R.Data->SetObjectField(TEXT("created_signature_graph"),DescribeGraph(BP,G));
    auto* E=Cast<UK2Node_FunctionEntry>(FBlueprintEditorUtils::GetEntryNode(G));if(!E) { return Cleanup(TEXT("dispatcher_entry_missing"),TEXT("Signature entry node not found.")); }const TStrongObjectPtr<UK2Node_FunctionEntry> KeepEntry(E);UEdGraphNode* Base=E;UK2Node_EditablePinBase* Editable=E;
    TSharedPtr<FJsonValue> Inputs;if(!InputsJson.IsEmpty()&&!Function::Pin::ParseJson(InputsJson,Inputs,R)) { const FString Error=R.Error;return Cleanup(TEXT("dispatcher_inputs_json_invalid"),Error); }TArray<TSharedPtr<FJsonValue>> Values;if(Inputs.IsValid()&&Inputs->Type==EJson::Array) { Values=Inputs->AsArray(); }if(Values.Num()>128) { return Cleanup(TEXT("dispatcher_input_budget"),TEXT("Original larger signatures remain incomplete outside128 inputs.")); }
    TArray<TSharedPtr<FJsonValue>> Created;TArray<FName> InputNames;TArray<FEdGraphPinType> Types;TArray<TStrongObjectPtr<UObject>> KeepTypes;
    for(int32 I=0;I<Values.Num();++I)
    {
        if(!Values[I].IsValid()||Values[I]->Type!=EJson::Object) { return Cleanup(TEXT("dispatcher_input_object_invalid"),TEXT("Event dispatcher input must be an object.")); }auto J=Values[I]->AsObject();FString Param,Type;if(!J->TryGetStringField(TEXT("parameter_name"),Param)&&!J->TryGetStringField(TEXT("name"),Param)) { return Cleanup(TEXT("dispatcher_input_name_missing"),TEXT("Missing input name.")); }Param.TrimStartAndEndInline();
        if(Param.IsEmpty()||Param.Len()>=NAME_SIZE) { return Cleanup(TEXT("dispatcher_input_name_invalid"),TEXT("Nonempty bounded input FName required.")); }for(int32 K=0;K<Param.Len();++K) { if(Param[K]==0) { return Cleanup(TEXT("dispatcher_input_name_invalid"),TEXT("Interior NUL input FName unsupported.")); } }
        if(!J->TryGetStringField(TEXT("parameter_type"),Type)&&!J->TryGetStringField(TEXT("type"),Type)) { return Cleanup(TEXT("dispatcher_input_type_missing"),TEXT("Missing input type.")); }
        FEdGraphPinType PinType;if(!InputType(Type,J,PinType,R)) { const FString Error=R.Error;return Cleanup(*R.ErrorCode,Error); }const FName PinName(*Param);if(PinName.IsNone()) { return Cleanup(TEXT("dispatcher_input_name_invalid"),TEXT("Input FName None is invalid.")); }
        if(Base->FindPin(PinName,EGPD_Output)) { return Cleanup(TEXT("dispatcher_input_duplicate"),TEXT("Event dispatcher input already exists.")); }FText Error;Count(R,TEXT("user_pin_can_create_calls"));if(!Editable->CanCreateUserDefinedPin(PinType,EGPD_Output,Error)) { return Cleanup(TEXT("dispatcher_input_pin_refused"),Error.ToString()); }
        if(PinType.PinSubCategoryObject.IsValid()) { KeepTypes.Emplace(PinType.PinSubCategoryObject.Get()); }Count(R,TEXT("user_pin_create_calls"));auto* P=Editable->CreateUserDefinedPin(PinName,PinType,EGPD_Output,false);Count(R,TEXT("user_pin_create_returns"));if(!P) { return Cleanup(TEXT("dispatcher_input_pin_failed"),TEXT("Failed to create user signature pin.")); }
        auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("parameter_name"),Param);Q->SetStringField(TEXT("parameter_type"),Type);Q->SetObjectField(TEXT("pin_type"),PinTypeToJson(PinType));Created.Add(MakeShared<FJsonValueObject>(Q));InputNames.Add(PinName);Types.Add(PinType);R.Data->SetArrayField(TEXT("created_input_prefix"),Created);
    }
    if(!Created.IsEmpty()) { Count(R,TEXT("entry_reconstruct_calls"));Base->ReconstructNode(); }
    if(!Category.IsEmpty()) { if(auto* V=Variable(BP,FNameValue)) { Count(R,TEXT("category_write_calls"));V->Category=FText::FromString(Category);Count(R,TEXT("category_helper_calls"));FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,FNameValue,nullptr,FText::FromString(Category),true); } }
    if(!Tooltip.IsEmpty()) { if(auto* V=Variable(BP,FNameValue)) { Count(R,TEXT("tooltip_descriptor_write_calls"));V->SetMetaData(TEXT("Tooltip"),Tooltip);Count(R,TEXT("tooltip_helper_calls"));FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,FNameValue,nullptr,TEXT("Tooltip"),Tooltip); } }
    // Re-resolve descriptor after metadata helper's structural regeneration; do not retain its old address.
    if(!Friendly.IsEmpty()) { if(auto* V=Variable(BP,FNameValue)) { Count(R,TEXT("friendly_name_write_calls"));V->FriendlyName=Friendly; } }
    R.Stage=TEXT("mark_structural");Structural(BP,R);TSharedRef<FJsonObject> A=MakeShared<FJsonObject>();if(!BlueprintState(BP,A,R)) { R.Data->SetObjectField(TEXT("incomplete_after"),A);R.bChangeStateKnown=false;return R; }R.Data->SetObjectField(TEXT("after"),A);R.bChanged=!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true;
    bool Pins=FreshPins(Base);for(int32 I=0;I<InputNames.Num();++I) { auto* P=Base->FindPin(InputNames[I],EGPD_Output);Pins&=P&&PinsMatchType(P->PinType,Types[I]); }R.Data->SetObjectField(TEXT("signature_entry"),DescribeNode(E));R.Data->SetObjectField(TEXT("signature_graph"),DescribeGraph(BP,G));R.Data->SetArrayField(TEXT("inputs"),Created);R.Data->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(DelegateType));
    auto* V=Variable(BP,FNameValue);const auto* DP=Property(BP,FNameValue,R);R.Data->SetObjectField(TEXT("current_property"),PropertyJson(DP,R));R.Data->SetObjectField(TEXT("current_signature"),Signature(DP?DP->SignatureFunction.Get():nullptr,R));const auto ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);
    const bool Preserved=OldVariables(Before,A,1)&&Equal(MakeShared<FJsonValueObject>(StableGraphs(Before,TEXT(""))),MakeShared<FJsonValueObject>(StableGraphs(A,G->GetPathName())));R.Data->SetBoolField(TEXT("old_variables_graphs_preserved"),Preserved);
    const bool Metadata=V&&V->VarGuid.IsValid()&&V->VarType.PinCategory==UEdGraphSchema_K2::PC_MCDelegate&&V->PropertyFlags&CPF_BlueprintAssignable&&V->PropertyFlags&CPF_BlueprintCallable&&(Category.IsEmpty()||V->Category.ToString().Equals(Category,ESearchCase::CaseSensitive))&&(Tooltip.IsEmpty()||(V->HasMetaData(TEXT("Tooltip"))&&V->GetMetaData(TEXT("Tooltip")).Equals(Tooltip,ESearchCase::CaseSensitive)))&&(Friendly.IsEmpty()||V->FriendlyName.Equals(Friendly,ESearchCase::CaseSensitive));
    bool CurrentTypedSignature=DP&&Loaded(DP->SignatureFunction);int32 SignatureIndex=0;if(CurrentTypedSignature) { for(TFieldIterator<FProperty> It(DP->SignatureFunction.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It) { if(!It->HasAnyPropertyFlags(CPF_Parm)) { continue; }FEdGraphPinType PT;const bool Converted=Schema->ConvertPropertyToPinType(*It,PT);CurrentTypedSignature&=SignatureIndex<InputNames.Num()&&It->GetFName()==InputNames[SignatureIndex]&&Converted&&PT.PinCategory==Types[SignatureIndex].PinCategory&&PT.PinSubCategory==Types[SignatureIndex].PinSubCategory&&PT.PinSubCategoryObject==Types[SignatureIndex].PinSubCategoryObject&&PT.ContainerType==Types[SignatureIndex].ContainerType;++SignatureIndex; } }CurrentTypedSignature&=SignatureIndex==InputNames.Num();R.Data->SetBoolField(TEXT("current_signature_names_value_types_verified"),CurrentTypedSignature);
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("dispatcher_name"),Name);Legacy->SetObjectField(TEXT("pin_type"),PinTypeToJson(DelegateType));auto LG=MakeShared<FJsonObject>(*DescribeGraph(BP,G));LG->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());LG->RemoveField(TEXT("object_path"));Legacy->SetObjectField(TEXT("signature_graph"),LG);Legacy->SetArrayField(TEXT("inputs"),Created);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("signature_inputs_verified"),Pins);R.Data->SetBoolField(TEXT("dispatcher_metadata_verified"),Metadata);
    if(!Preserved||!Metadata||!Pins||!CurrentTypedSignature||!Loaded(G)||G->bEditable||!E->bIsEditable||!BP->DelegateSignatureGraphs.Contains(G)||!G->GraphGuid.IsValid()||!BP->GetOutermost()->IsDirty()||!DP||!Loaded(DP->SignatureFunction)||!R.Data->GetObjectField(TEXT("current_signature"))->GetBoolField(TEXT("complete"))||!Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter))) { Fail(R,TEXT("dispatcher_declaration_postcondition_failed"),TEXT("Current declaration/signature/member/metadata/oldstate proof failed; accepted prefix retained, no added rollback after original success boundary."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
FResult Node(bool Bind,const FString& Path,const FString& Requested,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("dispatcher_editor_invalid"),TEXT("Idle Editor game thread required."));return R; }Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","DispatcherNode","Add Blueprint Dispatcher Call or Bind"));FString Dispatcher;if(!DispatcherName(Requested,Dispatcher,R)) { return R; }R.Data->SetStringField(TEXT("dispatcher_name"),Dispatcher);int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,Create,BP,G,Before,R)) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;const FName DName(*Dispatcher);auto* P=Property(BP,DName,R);R.Data->SetObjectField(TEXT("property_before"),PropertyJson(P,R));
    if(!P) { Fail(R,TEXT("dispatcher_property_missing"),TEXT("Compile or recreate dispatcher before adding node; graph resolution prefix retained."));return R; }if(!P->HasAllPropertyFlags(Bind?CPF_BlueprintAssignable:CPF_BlueprintCallable)) { Fail(R,TEXT("dispatcher_property_flag_invalid"),Bind?TEXT("Dispatcher is not BlueprintAssignable"):TEXT("Dispatcher is not BlueprintCallable"));return R; }if(!Loaded(P->SignatureFunction)||!Loaded(P->GetOwnerClass())) { Fail(R,TEXT("dispatcher_signature_unavailable"),TEXT("Current loaded signature and actual property owner required."));return R; }
    const TStrongObjectPtr<UClass> KeepOwner(P->GetOwnerClass());const TStrongObjectPtr<UFunction> KeepSignature(P->SignatureFunction);const auto SigBefore=Signature(P->SignatureFunction,R);R.Data->SetObjectField(TEXT("signature_before"),SigBefore);if(!SigBefore->GetBoolField(TEXT("complete"))) { Fail(R,TEXT("dispatcher_signature_observation_incomplete"),TEXT("Current signature type rows unavailable/budgeted."));return R; }
    const FString ClassPath=Bind?TEXT("/Script/BlueprintGraph.K2Node_AddDelegate"):TEXT("/Script/BlueprintGraph.K2Node_CallDelegate");Count(R,TEXT("node_class_lookup_calls"));auto* Class=FindObject<UClass>(nullptr,*ClassPath);Count(R,TEXT("node_class_lookup_returns"));if(!Loaded(Class)||Class->GetSuperClass()!=UK2Node_BaseMCDelegate::StaticClass()||!Class->HasAnyClassFlags(CLASS_Native)||Class->ClassGeneratedBy||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)) { Fail(R,TEXT("dispatcher_node_class_unavailable"),TEXT("Exact loaded native derived delegate class required; no forced module/class load or unexported derived import."));return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);
    R.Stage=TEXT("allocate_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* N=NewObject<UK2Node_BaseMCDelegate>(G,Class);Count(R,TEXT("node_allocation_returns"));const TStrongObjectPtr<UK2Node_BaseMCDelegate> KeepNode(N);R.Data->SetStringField(TEXT("allocated_node_path"),GetPathNameSafe(N));if(!N) { Fail(R,TEXT("dispatcher_node_allocation_failed"),TEXT("Failed to create dispatcher node."));return R; }
    const bool Self=!P->GetOwnerClass()||(BP->SkeletonGeneratedClass&&BP->SkeletonGeneratedClass->IsChildOf(P->GetOwnerClass()));R.Data->SetBoolField(TEXT("self_context"),Self);Count(R,TEXT("set_from_property_calls"));N->SetFromProperty(P,Self,P->GetOwnerClass());Count(R,TEXT("set_from_property_returns"));UEdGraphNode* Base=N;Count(R,TEXT("compatibility_calls"));const bool Compatible=Base->IsCompatibleWithGraph(G);R.Data->SetBoolField(TEXT("graph_compatible"),Compatible);if(!Compatible) { R.Data->SetObjectField(TEXT("allocated_node_prefix"),DescribeNode(Base));Fail(R,TEXT("dispatcher_graph_incompatible"),TEXT("Original late compatibility refusal follows allocation/reference assignment; no added Destroy/rollback."));return R; }
    Place(BP,G,Base,XY,true,R);Mark(BP,R);const bool Observed=After(BP,G,Base,Guid,Before,R);auto* Current=Property(BP,DName,R);R.Data->SetObjectField(TEXT("property_after"),PropertyJson(Current,R));const auto SigAfter=Signature(Current?Current->SignatureFunction.Get():nullptr,R);R.Data->SetObjectField(TEXT("signature_after"),SigAfter);const auto ClassAfter=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);
    bool Pins=FreshPins(Base);auto* Exec=Base->FindPin(UEdGraphSchema_K2::PN_Execute,EGPD_Input);auto* Then=Base->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output);auto* Target=Base->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);Pins&=Exec&&Then&&Target&&Exec->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Then->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Target->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Current&&Target->PinType.PinSubCategoryObject.Get()==Current->GetOwnerClass()->GetAuthoritativeClass();
    if(Bind) { auto* D=N->GetDelegatePin();FSimpleMemberReference ExpectedMember;if(Current&&Current->SignatureFunction) { FMemberReference::FillSimpleMemberReference<UFunction>(Current->SignatureFunction,ExpectedMember); }Pins&=Base->Pins.Num()==4&&D&&D->Direction==EGPD_Input&&D->PinType.PinCategory==UEdGraphSchema_K2::PC_Delegate&&D->PinType.bIsReference&&D->PinType.bIsConst&&D->PinType.PinSubCategoryMemberReference==ExpectedMember;R.Data->SetStringField(TEXT("delegate_member_name"),D?D->PinType.PinSubCategoryMemberReference.MemberName.ToString():FString()); }
    else if(Current&&Loaded(Current->SignatureFunction)) { int32 CountParams=0;for(TFieldIterator<FProperty> It(Current->SignatureFunction,EFieldIteratorFlags::ExcludeSuper);It;++It) { if(It->HasAnyPropertyFlags(CPF_Parm)&&(!It->HasAnyPropertyFlags(CPF_OutParm)||It->HasAnyPropertyFlags(CPF_ReferenceParm))) { ++CountParams;FEdGraphPinType PT;const bool Converted=GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(*It,PT);auto* Input=Base->FindPin(It->GetFName(),EGPD_Input);Pins&=Converted&&Input&&PinsMatchType(Input->PinType,PT); } }Pins&=Base->Pins.Num()==3+CountParams; }
    R.Data->SetBoolField(TEXT("specialized_pins_verified"),Pins);R.Data->SetStringField(TEXT("signature_function"),GetPathNameSafe(Current?Current->SignatureFunction.Get():nullptr));R.Data->SetBoolField(TEXT("native_class_signature_preserved"),Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(SigBefore),MakeShared<FJsonValueObject>(SigAfter)));
    auto Legacy=NodeToJson(Base,true);Legacy->SetStringField(TEXT("dispatcher_name"),Dispatcher);Legacy->SetStringField(TEXT("signature_function"),GetPathNameSafe(Current?Current->SignatureFunction.Get():nullptr));if(R.Data->HasTypedField<EJson::Object>(TEXT("graph"))) { auto LG=MakeShared<FJsonObject>(*R.Data->GetObjectField(TEXT("graph")));LG->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());LG->RemoveField(TEXT("object_path"));Legacy->SetObjectField(TEXT("graph"),LG); }R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    if(!Observed) { return R; }if(!Current||N->GetProperty()!=Current||N->GetDelegateSignature()!=Current->SignatureFunction||!Pins||!R.Data->GetBoolField(TEXT("native_class_signature_preserved"))||Base->NodePosX!=XY[0]||Base->NodePosY!=XY[1]||Base->GetClass()!=Class) { Fail(R,TEXT("dispatcher_node_postcondition_failed"),TEXT("Current property/signature/specialized pins/class/position proof failed; accepted node prefix retained."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
Function::FResult AddDispatcher(const FString& P,const FString& N,const FString& I,const FString& C,const FString& T,const FString& F) { if(WidgetDispatcherActive){FResult R;Init(R);Fail(R,TEXT("widget_dispatcher_busy"),TEXT("Widget dispatcher callback reentry refused."));return R;}if(WidgetPath(P))return WidgetDeclare(P,N,I,C,T,F);return Declare(P,N,I,C,T,F); }
Function::FResult AddCall(const FString& P,const FString& D,const FString& X,const FString& N,const FString& I,const FString& T,bool C) { if(WidgetDispatcherActive){FResult R;Init(R);Fail(R,TEXT("widget_dispatcher_busy"),TEXT("Widget dispatcher callback reentry refused."));return R;}if(WidgetPath(P))return WidgetNode(false,P,D,X,N,I,T,C);return Node(false,P,D,X,N,I,T,C); }
Function::FResult AddBind(const FString& P,const FString& D,const FString& X,const FString& N,const FString& I,const FString& T,bool C) { if(WidgetDispatcherActive){FResult R;Init(R);Fail(R,TEXT("widget_dispatcher_busy"),TEXT("Widget dispatcher callback reentry refused."));return R;}if(WidgetPath(P))return WidgetNode(true,P,D,X,N,I,T,C);return Node(true,P,D,X,N,I,T,C); }
}
