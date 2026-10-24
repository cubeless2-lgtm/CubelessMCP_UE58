// DK original dispatcher lifecycle and custom-event signatures; Editor only.
#include "Compatibility/UE58/CubelessBlueprintDispatcherLifecycleApi.h"
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
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Settings/EditorStyleSettings.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
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
#include "GameFramework/Actor.h"
#include "Internationalization/Text.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "Misc/PackageName.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintDispatcherLifecycle
{
namespace
{
using FResult=Function::FResult;
namespace Pin=Function::Pin;
constexpr int32 MaxGraphs=256,MaxNodes=2048;
// WQ is request-local to the three existing dispatcher lifecycle operations.
const TFunction<bool(UBlueprint*,UEdGraph*)>* GeneratedParentAdmission=nullptr;
const TFunction<bool()>* GeneratedParentFence=nullptr;
const TFunction<bool(const TCHAR*)>* GeneratedParentBorrowStop=nullptr;
bool GeneratedParentCurrent(){return !GeneratedParentFence||(*GeneratedParentFence)();}
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
        if(Loaded(C)&&C->GetClass()==UBlueprintGeneratedClass::StaticClass()&&GeneratedParentAdmission)
        {if(!(*GeneratedParentAdmission)(BP,G))return false;break;}
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
    if(!GeneratedParentCurrent())return MakeShared<FJsonObject>();
    // Hold only this observation's physical borrow. Compilation/placement is
    // outside this read, so legitimate subsequent pin regeneration is allowed.
    const bool Guarded=GeneratedParentFence!=nullptr;
    const TStrongObjectPtr<UEdGraphNode> ObserverNode(Guarded&&Loaded(Node)?Node:nullptr);
    const TStrongObjectPtr<UEdGraph> ObserverGraph(Guarded&&Loaded(Node)?Node->GetGraph():nullptr);
    UObject* ObserverOuter=Guarded&&Loaded(Node)?Node->GetOuter():nullptr;
    const bool ObserverMember=Guarded&&ObserverGraph.IsValid()&&ObserverGraph->Nodes.Contains(Node);
    const TArray<UEdGraphPin*> ObserverPins=Guarded&&Loaded(Node)?Node->Pins:TArray<UEdGraphPin*>();
    auto BorrowCurrent=[&]()->bool
    {
        if(!GeneratedParentCurrent())return false;if(!Guarded)return true;
        bool Valid=Loaded(ObserverNode.Get())&&ObserverNode.Get()==Node&&Loaded(ObserverGraph.Get())&&Node->GetOuter()==ObserverOuter&&Node->GetGraph()==ObserverGraph.Get()&&ObserverGraph->Nodes.Contains(Node)==ObserverMember&&Node->Pins==ObserverPins;
        if(Valid)for(auto* P:ObserverPins)if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node){Valid=false;break;}
        return Valid||(GeneratedParentBorrowStop&&(*GeneratedParentBorrowStop)(TEXT("node_observer_borrow")));
    };
    if(!BorrowCurrent())return MakeShared<FJsonObject>();
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits)); J->SetStringField(TEXT("object_path"), Node->GetPathName());
    J->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); J->SetStringField(TEXT("node_name"), Node->GetName());
    J->SetNumberField(TEXT("x"), Node->NodePosX); J->SetNumberField(TEXT("y"), Node->NodePosY);
    TArray<TSharedPtr<FJsonValue>> Pins;
    const auto& PinRows=Guarded?ObserverPins:Node->Pins;for (UEdGraphPin* P : PinRows)
    {
        if (!P) { continue; } auto Q = MakeShared<FJsonObject>(); Q->SetStringField(TEXT("name"), P->PinName.ToString());
        Q->SetStringField(TEXT("pin_id"), P->PinId.ToString(EGuidFormats::Digits)); Q->SetStringField(TEXT("direction"), P->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        const FEdGraphPinType ObservedType=P->PinType;const auto Type=Pin::Describe(ObservedType);if(!BorrowCurrent())return J;
        Q->SetObjectField(TEXT("pin_type"),Type); Q->SetBoolField(TEXT("orphaned"), P->bOrphanedPin);
        Q->SetStringField(TEXT("default_value"), P->DefaultValue); Q->SetStringField(TEXT("default_object"), P->DefaultObject ? P->DefaultObject->GetPathName() : TEXT(""));
        const FText ObservedText=P->DefaultTextValue;const FString DefaultText=ObservedText.ToString();if(!BorrowCurrent())return J;
        Q->SetStringField(TEXT("default_text"),DefaultText); TArray<TSharedPtr<FJsonValue>> Links;
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
    { const UFunction* F = Call->GetTargetFunction();if(!BorrowCurrent())return J; J->SetStringField(TEXT("function_path"), F ? F->GetPathName() : TEXT("")); }
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
    if(!GeneratedParentCurrent())return false;
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
        const auto HeldNodes=G->Nodes;for (const auto& Slot:HeldNodes)
        {
            UEdGraphNode* N=Slot.Get();const TStrongObjectPtr<UEdGraphNode> Held(Loaded(N)?N:nullptr);if(!GeneratedParentCurrent())return false;
            if (!IsValid(N) || !N->NodeGuid.IsValid() || NodeGuids.Contains(N->NodeGuid) || N->Pins.Num() > 512)
            { return Fail(R, TEXT("function_node_identity_invalid"), TEXT("A graph node identity/pin budget is invalid; observation is incomplete.")); }
            NodeGuids.Add(N->NodeGuid); Nodes.Add(MakeShared<FJsonValueObject>(DescribeNode(N)));if(!GeneratedParentCurrent())return false;
        }
        Out->SetObjectField(TEXT("graph"), DescribeGraph(BP, G)); Out->SetArrayField(TEXT("nodes"), Nodes);
    }
    return true;
}

bool DispatcherName(const FString& Input,FString& Out,FResult& R)
{
    Out=Input.TrimStartAndEnd();bool Bounded=!Out.IsEmpty()&&Out.Len()<NAME_SIZE;for(int32 I=0;I<Out.Len();++I) { Bounded&=Out[I]!=0; }
    return (Bounded&&!FName(*Out).IsNone())||Fail(R,TEXT("dispatcher_name_invalid"),TEXT("Original trimmed dispatcher FName must be nonempty/nonNone and fit NAME_SIZE without interior NUL."));
}

TSharedRef<FJsonObject> Variables(UBlueprint* BP,FResult& R)
{
    Count(R,TEXT("variable_observation_calls"));auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;bool Complete=BP->NewVariables.Num()<=256;
    const auto Copied=GeneratedParentFence?BP->NewVariables:TArray<FBPVariableDescription>();const auto& Descriptors=GeneratedParentFence?Copied:BP->NewVariables;
    if(Complete) { for(const auto& V:Descriptors) { FString Text;FBPVariableDescription::StaticStruct()->ExportText(Text,&V,nullptr,BP,PPF_None,nullptr);if(!GeneratedParentCurrent()){J->SetBoolField(TEXT("complete"),false);return J;}if(Text.Len()>32768) { Complete=false;break; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),V.VarName.ToString());Q->SetStringField(TEXT("guid"),V.VarGuid.ToString());Q->SetStringField(TEXT("reflected_descriptor"),Text);Rows.Add(MakeShared<FJsonValueObject>(Q)); } }J->SetArrayField(TEXT("ordered_descriptors"),Rows);J->SetBoolField(TEXT("complete"),Complete);return J;
}

bool BlueprintState(UBlueprint* BP,TSharedRef<FJsonObject>& Out,FResult& R)
{
    Count(R,TEXT("blueprint_capture_calls"));Out=MakeShared<FJsonObject>();const auto V=Variables(BP,R);if(!GeneratedParentCurrent())return false;Out->SetObjectField(TEXT("variables"),V);if(!V->GetBoolField(TEXT("complete"))) { return Fail(R,TEXT("dispatcher_variable_observation_incomplete"),TEXT("Descriptor snapshot exceeds finite256/32768 bounds.")); }
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);if(!GeneratedParentCurrent())return false;if(Graphs.Num()>64) { return Fail(R,TEXT("dispatcher_graph_budget"),TEXT("Full selected owned graph preservation scope is64 graphs.")); }
    TArray<TSharedPtr<FJsonValue>> Rows;int32 Nodes=0;for(UEdGraph* G:Graphs)
    { if(!Loaded(G)||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||(Nodes+=G->Nodes.Num())>4096) { return Fail(R,TEXT("dispatcher_graph_observation_incomplete"),TEXT("Loaded exact K2 graphs within total4096-node scope required.")); }auto Q=MakeShared<FJsonObject>();Count(R,TEXT("graph_capture_calls"));if(!Capture(BP,G,Q,R)) { Out->SetArrayField(TEXT("graph_states"),Rows);return false; }Rows.Add(MakeShared<FJsonValueObject>(Q)); }Out->SetArrayField(TEXT("graph_states"),Rows);Out->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());Out->SetNumberField(TEXT("blueprint_status"),BP->Status.GetValue());return true;
}

TSharedRef<FJsonObject> StableGraphs(const TSharedRef<FJsonObject>& State,const FString& Excluded)
{
    auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& V:State->GetArrayField(TEXT("graph_states")))
    { const auto O=V->AsObject();if(O->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")).Equals(Excluded,ESearchCase::CaseSensitive)) { continue; }auto Q=MakeShared<FJsonObject>();Q->SetObjectField(TEXT("graph"),O->GetObjectField(TEXT("graph")));Q->SetArrayField(TEXT("nodes"),O->GetArrayField(TEXT("nodes")));Rows.Add(MakeShared<FJsonValueObject>(Q)); }J->SetArrayField(TEXT("graphs"),Rows);return J;
}

bool OldVariables(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,int32 Delta)
{
    const auto& A=Before->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("ordered_descriptors"));const auto& B=After->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("ordered_descriptors"));if(B.Num()!=A.Num()+Delta) { return false; }for(int32 I=0;I<A.Num();++I) { if(!Equal(A[I],B[I])) { return false; } }return true;
}

bool ReferenceGuard(const FString& S)
{
    FString N=S.TrimStartAndEnd();N.TrimQuotesInline();N.ReplaceInline(TEXT("\\"),TEXT("/"));N=N.ToLower();for(const TCHAR* Token:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor")}) { if(N.Contains(Token)) { return false; } }return true;
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

void Init(FResult& R);

bool LivePins(UEdGraphNode* N,bool AllowLinks)
{
    TSet<FGuid> Ids;
    for(const UEdGraphPin* P:N->Pins)
    {
        if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||(!AllowLinks&&!P->LinkedTo.IsEmpty())) { return false; }
        Ids.Add(P->PinId);for(const UEdGraphPin* Other:P->LinkedTo) { if(!Other||Other->WasTrashed()||!Loaded(Other->GetOwningNodeUnchecked())||!Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(P))) { return false; } }
    }
    return true;
}
TSharedRef<FJsonObject> EventDetails(UK2Node_CustomEvent* E,FResult& R)
{
    Count(R,TEXT("event_detail_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("custom_event_name"),E->CustomFunctionName.ToString());J->SetBoolField(TEXT("call_in_editor"),E->bCallInEditor);J->SetBoolField(TEXT("override_function"),E->bOverrideFunction);J->SetBoolField(TEXT("editable"),E->bIsEditable);J->SetBoolField(TEXT("deprecated"),E->bIsDeprecated);J->SetStringField(TEXT("deprecation_message"),E->DeprecationMessage);J->SetStringField(TEXT("function_flags_uint32"),LexToString(E->FunctionFlags));J->SetNumberField(TEXT("net_flags"),E->GetNetFlags());
    TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& U:E->UserDefinedPins) { if(!U.IsValid()) { Rows.Add(MakeShared<FJsonValueNull>());continue; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),U->PinName.ToString());Q->SetNumberField(TEXT("direction"),U->DesiredPinDirection);Q->SetObjectField(TEXT("pin_type"),Pin::Describe(U->PinType));Q->SetBoolField(TEXT("is_const"),U->PinType.bIsConst);Q->SetStringField(TEXT("default_value"),U->PinDefaultValue);Rows.Add(MakeShared<FJsonValueObject>(Q)); }J->SetArrayField(TEXT("user_defined_pins"),Rows);return J;
}
bool SameExceptCallFlag(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B)
{
    auto X=MakeShared<FJsonObject>(*A),Y=MakeShared<FJsonObject>(*B);X->RemoveField(TEXT("call_in_editor"));Y->RemoveField(TEXT("call_in_editor"));return Equal(MakeShared<FJsonValueObject>(X),MakeShared<FJsonValueObject>(Y));
}
bool Grid(uint32& Value,FResult& R)
{
    Count(R,TEXT("grid_settings_observation_calls"));const auto* Settings=GetDefault<UEditorStyleSettings>();Value=Settings?Settings->GridSnapSize:0;R.Data->SetNumberField(TEXT("grid_snap_size"),Value);
    return Value!=0||Fail(R,TEXT("dispatcher_grid_zero_pending"),TEXT("SDK CreateFromFunction SnapToGrid divides by uint32 GridSnapSize; zero is refused without altering editor settings."));
}
int32 Snapped(int32 Value,uint32 GridSize)
{
    const uint32 U=GridSize*(static_cast<uint32>(Value)/GridSize);
    return U<=static_cast<uint32>(MAX_int32)?static_cast<int32>(U):static_cast<int32>(static_cast<int64>(U)-4294967296LL);
}
bool IncludedSignature(UFunction* F,TArray<FName>& Names,TArray<FEdGraphPinType>& Types,TArray<TStrongObjectPtr<UObject>>& KeepTypes,FResult& R)
{
    if(!Loaded(F)) { return Fail(R,TEXT("dispatcher_signature_unavailable"),TEXT("Loaded signature required before callback/factory.")); }
    const auto* Schema=GetDefault<UEdGraphSchema_K2>();TSet<FName> Seen;
    // The exported factory stops at the first non-Parm, includes inherited fields,
    // and excludes output-only parameters. Preserve that actual SDK loop.
    for(TFieldIterator<FProperty> It(F);It&&It->HasAnyPropertyFlags(CPF_Parm);++It)
    {
        if(It->HasAnyPropertyFlags(CPF_OutParm)&&!It->HasAnyPropertyFlags(CPF_ReferenceParm)) { continue; }
        FEdGraphPinType T;Count(R,TEXT("included_parameter_type_observation_calls"));const bool Converted=Schema->ConvertPropertyToPinType(*It,T);
        if(Names.Num()>=128||!Converted||It->GetFName()==UEdGraphSchema_K2::PN_Then||It->GetFName()==UK2Node_Event::DelegateOutputName||Seen.Contains(It->GetFName())||T.PinCategory==UEdGraphSchema_K2::PC_Exec||T.PinCategory==UEdGraphSchema_K2::PC_Wildcard)
        { return Fail(R,TEXT("dispatcher_signature_copy_scope_pending"),TEXT("Finite schema-convertible unique signature inputs required. Reserved/duplicate/exec/wildcard and larger valid signatures remain explicit residuals.")); }
        if(T.PinSubCategoryObject.IsValid()) { if(!Loaded(T.PinSubCategoryObject.Get())||!ReferenceGuard(T.PinSubCategoryObject->GetPathName())||T.PinSubCategoryObject->GetOutermost()->GetName().Equals(TEXT("/Script/CubelessMCPUE58Editor"),ESearchCase::CaseSensitive)||T.PinSubCategoryObject->IsEditorOnly()) { return Fail(R,TEXT("dispatcher_signature_type_scope_pending"),TEXT("Loaded runtime signature type without MCP dependency required.")); }KeepTypes.Emplace(T.PinSubCategoryObject.Get()); }
        Seen.Add(It->GetFName());Names.Add(It->GetFName());Types.Add(T);
    }
    TArray<TSharedPtr<FJsonValue>> Rows;for(int32 I=0;I<Names.Num();++I) { auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),Names[I].ToString());J->SetObjectField(TEXT("pin_type"),Pin::Describe(Types[I]));J->SetBoolField(TEXT("is_const"),Types[I].bIsConst);Rows.Add(MakeShared<FJsonValueObject>(J)); }R.Data->SetArrayField(TEXT("included_signature_parameters"),Rows);return true;
}
bool TypedEvent(UK2Node_CustomEvent* E,const TArray<FName>& Names,const TArray<FEdGraphPinType>& Types,bool Linked,FResult& R)
{
    if(!Loaded(E)) { return false; }UEdGraphNode* Base=E;auto* Then=Base->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output);auto* Delegate=Base->FindPin(UK2Node_Event::DelegateOutputName,EGPD_Output);
    bool Good=LivePins(Base,Linked)&&Then&&Delegate&&Then->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Delegate->PinType.PinCategory==UEdGraphSchema_K2::PC_Delegate&&Then->LinkedTo.IsEmpty()&&Base->Pins.Num()==2+Names.Num()&&E->UserDefinedPins.Num()==Names.Num()&&!E->bOverrideFunction&&E->GetNetFlags()==0&&E->FunctionFlags==(FUNC_BlueprintCallable|FUNC_BlueprintEvent|FUNC_Public);
    for(int32 I=0;I<Names.Num();++I)
    { auto* P=Base->FindPin(Names[I],EGPD_Output);const auto U=E->UserDefinedPins.IsValidIndex(I)?E->UserDefinedPins[I]:TSharedPtr<FUserPinInfo>();Good&=P&&P->LinkedTo.IsEmpty()&&PinsMatchType(P->PinType,Types[I])&&U.IsValid()&&U->PinName==Names[I]&&U->DesiredPinDirection==EGPD_Output&&PinsMatchType(U->PinType,Types[I]); }
    R.Data->SetBoolField(TEXT("event_signature_pins_verified"),Good);return Good;
}
bool Observe(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Selected,const TSharedRef<FJsonObject>& Before,UEdGraphNode* Reused,FResult& R)
{
    R.Stage=TEXT("after_observation");TSharedRef<FJsonObject> AfterState=MakeShared<FJsonObject>();const bool Complete=BlueprintState(BP,AfterState,R);R.Data->SetObjectField(Complete?TEXT("after_blueprint_state"):TEXT("incomplete_after_blueprint_state"),AfterState);
    if(!Complete) { R.bChangeStateKnown=false;return false; }
    const auto& A=Before->GetArrayField(TEXT("graph_states"));const auto& B=AfterState->GetArrayField(TEXT("graph_states"));bool Old=A.Num()==B.Num(),Rows=Old;TArray<TSharedPtr<FJsonValue>> Added;int32 SelectedDelta=0;bool SelectedPresent=false;for(const auto& V:A) { const auto X=V->AsObject();if(X->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")).Equals(G->GetPathName(),ESearchCase::CaseSensitive)) { SelectedDelta=G->Nodes.Num()-X->GetArrayField(TEXT("nodes")).Num();SelectedPresent=true;break; } }Rows&=SelectedPresent;
    if(Old) { for(int32 I=0;I<A.Num();++I)
    {
        const auto X=A[I]->AsObject(),Y=B[I]->AsObject();const FString Path=X->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path"));const bool Target=Path.Equals(G->GetPathName(),ESearchCase::CaseSensitive);const auto& NX=X->GetArrayField(TEXT("nodes"));const auto& NY=Y->GetArrayField(TEXT("nodes"));const int32 Delta=NY.Num()-NX.Num();
        Rows&=GraphRowsPreserved(X,Y,G->GetPathName(),SelectedDelta);
        auto GX=MakeShared<FJsonObject>(*X->GetObjectField(TEXT("graph")));if(Target) { GX->SetNumberField(TEXT("node_count"),GX->GetNumberField(TEXT("node_count"))+Delta); }Rows&=Equal(MakeShared<FJsonValueObject>(GX),MakeShared<FJsonValueObject>(Y->GetObjectField(TEXT("graph"))));
        if(Target) { R.Data->SetObjectField(TEXT("after"),Y);R.Data->SetObjectField(TEXT("graph"),DescribeGraph(BP,G)); }
        if(NY.Num()<NX.Num()||(!Target&&NY.Num()!=NX.Num())) { Old=false;continue; }
        for(int32 K=0;K<NX.Num();++K) { const auto N=NX[K]->AsObject();if(Reused&&N->GetStringField(TEXT("object_path")).Equals(Reused->GetPathName(),ESearchCase::CaseSensitive)) { Old&=SameExceptCallFlag(N.ToSharedRef(),NY[K]->AsObject().ToSharedRef()); }else { Old&=Equal(NX[K],NY[K]); } }
        for(int32 K=NX.Num();K<NY.Num();++K) { Added.Add(NY[K]); }
    } }
    R.Data->SetArrayField(TEXT("new_nodes"),Added);R.Data->SetNumberField(TEXT("observed_new_node_count"),Added.Num());R.Data->SetBoolField(TEXT("old_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);const bool Vars=OldVariables(Before,AfterState,0);R.Data->SetBoolField(TEXT("old_variable_descriptors_preserved"),Vars);
    R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(AfterState));R.bChangeStateKnown=true;
    if(Loaded(Selected)) { const auto Node=DescribeNode(Selected);R.Data->SetObjectField(TEXT("node"),Node);for(const auto& P:Node->Values) { R.Data->SetField(P.Key,P.Value); }LegacyNodeFields(Selected,R); }
    return (Old&&Rows&&Vars&&Loaded(G)&&Loaded(Selected)&&G->Nodes.Contains(Selected))||Fail(R,TEXT("dispatcher_lifecycle_preservation_failed"),TEXT("Current owned nodes/pins/defaults/links/descriptors differ; actual prefix retained without rollback or retry."));
}
void Legacy(UEdGraphNode* N,UBlueprint* BP,UEdGraph* G,const TCHAR* Kind,const FString& Name,UFunction* F,FResult& R)
{
    auto J=NodeToJson(N,true);if(Kind) { J->SetStringField(TEXT("dispatcher_name"),Name);J->SetStringField(TEXT("node_kind"),Kind); }else { J->SetStringField(TEXT("custom_event_name"),Name); }
    J->SetStringField(TEXT("signature_function"),GetPathNameSafe(F));auto GraphJson=MakeShared<FJsonObject>(*DescribeGraph(BP,G));GraphJson->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());GraphJson->RemoveField(TEXT("object_path"));J->SetObjectField(TEXT("graph"),GraphJson);R.Data->SetObjectField(TEXT("legacy_result"),J);
}
bool DelegatePins(UK2Node_BaseMCDelegate* N,FMulticastDelegateProperty* P,bool Clear,bool Linked,FResult& R)
{
    UEdGraphNode* Base=N;auto* Exec=Base->FindPin(UEdGraphSchema_K2::PN_Execute,EGPD_Input);auto* Then=Base->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output);auto* Target=Base->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);bool Good=LivePins(Base,Linked)&&P&&Loaded(P->GetOwnerClass())&&Exec&&Then&&Target&&Exec->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Then->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Target->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Target->PinType.PinSubCategoryObject.Get()==P->GetOwnerClass()->GetAuthoritativeClass()&&Exec->LinkedTo.IsEmpty()&&Then->LinkedTo.IsEmpty()&&Target->LinkedTo.IsEmpty();
    auto* D=N->GetDelegatePin();if(Clear) { Good&=Base->Pins.Num()==3&&!D; }else { FSimpleMemberReference Ref;if(P&&Loaded(P->SignatureFunction)) { FMemberReference::FillSimpleMemberReference<UFunction>(P->SignatureFunction,Ref); }Good&=Base->Pins.Num()==4&&D&&D->Direction==EGPD_Input&&D->PinType.PinCategory==UEdGraphSchema_K2::PC_Delegate&&D->PinType.bIsConst&&D->PinType.bIsReference&&D->PinType.PinSubCategoryMemberReference==Ref; }
    R.Data->SetBoolField(TEXT("delegate_specialized_pins_verified"),Good);return Good;
}
struct FWidgetLifecycleWork
{
    FResult& R;TStrongObjectPtr<UWidgetBlueprint> BP;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UWidgetTree> Tree;TStrongObjectPtr<UCanvasPanel> Root;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<UClass> Parent,Gen,Skel;TStrongObjectPtr<UObject> GenCDO,SkelCDO;
    TStrongObjectPtr<UBlueprintEditorSettings> Policy;TArray<TStrongObjectPtr<UObject>> Holds;
    FString Path,GenPath,SkelPath,TreePath,RootPath;UClass* GenMeta=nullptr;UClass* SkelMeta=nullptr;FName Dispatcher;
    bool Stopped=false,Rebinding=false,ClassesReady=false,Writer=false,MemberAdded=false,GraphAdded=false;UEdGraph* NewGraph=nullptr;UEdGraphNode* NewNode=nullptr;
    FName ExplicitCustomName;bool ExplicitCustomReady=false,ExplicitCustomEditor=false;
    bool PlacementReady=false,GuidReady=false;int32 RequestedX=0,RequestedY=0;FGuid NewId;int32 PendingDelta=1;TStrongObjectPtr<UK2Node_CustomEvent> ImplicitEvent;TMap<FGuid,FSimpleMemberReference> CompilerEventReferences;bool NodeMember=false,SelectedPlanReady=false,MutableCategory=false;FBPVariableDescription SelectedPlan;FMemberReference NewReference;bool NewReferenceReady=false;UClass* NewNodeClass=nullptr;TArray<TObjectPtr<UEdGraph>> Uber,Functions,Macros,Delegates;TMap<FName,FGuid> WidgetGuids;TArray<FBPVariableDescription> Variables;
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
    explicit FWidgetLifecycleWork(FResult& In):R(In){}
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
                auto* N=H.Get();if(NodeMember&&(N==NewNode||N==ImplicitEvent.Get())&&!IncludeSelected)continue;FNode V;
                if(auto* D=Cast<UK2Node_BaseMCDelegate>(N)){V.Reference=D->DelegateReference;V.ReferenceKind=1;}
                else if(auto* Event=Cast<UK2Node_CustomEvent>(N)){V.Reference=Event->EventReference;V.ReferenceKind=4;V.CustomName=Event->CustomFunctionName;V.CustomFlags=Event->FunctionFlags;V.CustomOverride=Event->bOverrideFunction;V.CustomDeprecated=Event->bIsDeprecated;V.CustomEditor=Event->bCallInEditor;V.CustomEditable=Event->bIsEditable;V.CustomDeprecation=Event->DeprecationMessage;for(const auto& Info:Event->UserDefinedPins){if(!Info)return false;V.UserPins.Add({Info->PinName,Info->PinType,Info->DesiredPinDirection,Info->PinDefaultValue});}}
                else if(auto* E=Cast<UK2Node_Event>(N)){V.Reference=E->EventReference;V.ReferenceKind=2;}
                else if(auto* EntryNode=Cast<UK2Node_FunctionEntry>(N)){V.Reference=EntryNode->FunctionReference;V.ReferenceKind=3;V.EntryFlags=EntryNode->GetExtraFlags();V.EntryEditable=EntryNode->bIsEditable;
                    for(const auto& Info:EntryNode->UserDefinedPins){if(!Info.IsValid())return false;V.UserPins.Add({Info->PinName,Info->PinType,Info->DesiredPinDirection,Info->PinDefaultValue});}}
                else if(N->GetClass()->GetPathName()==TEXT("/Script/BlueprintGraph.K2Node_Self")){V.ReferenceKind=5;}
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
            if(Graph->Nodes.Num()!=G.Nodes.Num()+((Graph==NewGraph&&NodeMember)?PendingDelta:0))return false;
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
            if(Graph==NewGraph&&NodeMember){const int32 At=G.Nodes.Num();if(!Graph->Nodes.IsValidIndex(At)||!Graph->Nodes[At].IsResolved()||Graph->Nodes[At].Get()!=NewNode||!Loaded(NewNode)||NewNode->GetOuter()!=Graph)return false;if(PendingDelta==2&&(!ImplicitEvent.IsValid()||!Graph->Nodes.IsValidIndex(At+1)||!Graph->Nodes[At+1].IsResolved()||Graph->Nodes[At+1].Get()!=ImplicitEvent.Get()||!Loaded(ImplicitEvent.Get())||ImplicitEvent->GetClass()!=UK2Node_CustomEvent::StaticClass()||ImplicitEvent->GetOuter()!=Graph))return false;}
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
        if(NewNode&&((PlacementReady&&(NewNode->NodePosX!=RequestedX||NewNode->NodePosY!=RequestedY))||(GuidReady&&NewNode->NodeGuid!=NewId)))return Bad(Stage,TEXT("selected_lifecycle_position_or_guid"),FString::Printf(TEXT("%d,%d/%s"),RequestedX,RequestedY,*NewId.ToString()),FString::Printf(TEXT("%d,%d/%s"),NewNode->NodePosX,NewNode->NodePosY,*NewNode->NodeGuid.ToString()));
        if(NewNode&&ExplicitCustomReady){auto* Event=static_cast<UK2Node_CustomEvent*>(NewNode);if(Event->CustomFunctionName!=ExplicitCustomName||Event->bCallInEditor!=ExplicitCustomEditor||Event->GetNetFlags()!=0)return Bad(Stage,TEXT("explicit_custom_event_plan"));}
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
        TArray<TSharedPtr<FJsonValue>> Pins;for(const auto& P:Role->Pins){auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),P.Name.ToString());Q->SetStringField(TEXT("pin_id"),P.Id.ToString(EGuidFormats::Digits));Q->SetStringField(TEXT("direction"),P.Direction==EGPD_Input?TEXT("input"):TEXT("output"));auto Type=Pin::Describe(P.Type);if(!Current(Stage))return J;Q->SetObjectField(TEXT("pin_type"),Type);Q->SetStringField(TEXT("category"),P.Type.PinCategory.ToString());Q->SetStringField(TEXT("subcategory"),P.Type.PinSubCategory.ToString());Q->SetBoolField(TEXT("is_array"),P.Type.IsArray());Q->SetStringField(TEXT("default_value"),P.Raw);Q->SetStringField(TEXT("default_text"),P.Display);Q->SetStringField(TEXT("default_object"),P.Default?P.Default->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> Links;for(const FGuid Id:P.Links){const FPin* Peer=nullptr;const FNode* Owner=nullptr;for(const auto& GRow:Graphs)for(const auto& NRow:GRow.Nodes)for(const auto& PRow:NRow.Pins)if(PRow.Id==Id){Peer=&PRow;Owner=&NRow;}if(!Peer||!Owner){Bad(Stage,TEXT("published_link_endpoint"));return J;}auto Link=MakeShared<FJsonObject>();Link->SetStringField(TEXT("node_id"),Owner->Id.ToString());Link->SetStringField(TEXT("node_path"),Owner->Path);Link->SetStringField(TEXT("pin_name"),Peer->Name.ToString());Links.Add(MakeShared<FJsonValueObject>(Link));}Q->SetArrayField(TEXT("linked_to"),Links);Pins.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("pins"),Pins);return J;
    }
    bool FreezeNew(const TCHAR* Stage)
    {
        if(!Current(Stage))return false;
        TArray<FGraph> All;if(!CaptureGraphs(All,true))return Bad(Stage,TEXT("final_selected_inventory"));
        // Old protected graphs were already checked; only selected new roles are admitted here.
        if(NodeMember){for(auto& G:All)if(G.Hold.Get()==NewGraph){const auto* Old=Graphs.FindByPredicate([&](const FGraph& Row){return Row.Hold.Get()==NewGraph;});if(!Old||G.Nodes.Num()!=Old->Nodes.Num()+PendingDelta||G.Nodes[Old->Nodes.Num()].Hold.Get()!=NewNode||(PendingDelta==2&&G.Nodes.Last().Hold.Get()!=ImplicitEvent.Get()))return Bad(Stage,TEXT("final_node_membership"));}NodeMember=false;}
        CopyDelegateSources(Graphs,All);Graphs=MoveTemp(All);if(!Current(Stage)||!Displays(Graphs,Stage))return false;return true;
    }
};

TSharedRef<FJsonObject> WidgetAuthoredSnapshot(FWidgetLifecycleWork& W,const TCHAR* Stage)
{
    auto State=MakeShared<FJsonObject>();if(!W.Current(Stage))return State;State->SetStringField(TEXT("blueprint"),W.Path);State->SetBoolField(TEXT("dirty"),W.Package->IsDirty());State->SetNumberField(TEXT("blueprint_status"),W.BP->Status.GetValue());
    TArray<TSharedPtr<FJsonValue>> States;for(const auto& G:W.Graphs){auto Row=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("graph"),DescribeGraph(W.BP.Get(),G.Hold.Get()));if(!W.Current(Stage))return State;TArray<TSharedPtr<FJsonValue>> Nodes;for(const auto& N:G.Nodes){auto Node=W.NodeJson(N.Hold.Get(),Stage);if(W.Stopped)return State;Nodes.Add(MakeShared<FJsonValueObject>(Node));}Row->SetArrayField(TEXT("nodes"),Nodes);States.Add(MakeShared<FJsonValueObject>(Row));}State->SetArrayField(TEXT("graph_states"),States);return State;
}
bool WidgetLifecycleActive=false;
bool WidgetLifecyclePath(const FString& Path)
{
    if(!Path.StartsWith(TEXT("/Game/"))||!Function::Pin::SafeName(Path)||!ReferenceGuard(Path))return false;
    const FString ObjectPath=Path.Contains(TEXT("."))?Path:Path+TEXT(".")+FPackageName::GetShortName(Path);return FindObject<UWidgetBlueprint>(nullptr,*ObjectPath)!=nullptr;
}

FResult WidgetLifecycle(int32 Kind,const FString& Path,const FString& Requested,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphKind,bool Create)
{
    FResult R;Init(R);if(WidgetLifecycleActive){Fail(R,TEXT("widget_dispatcher_lifecycle_busy"),TEXT("Request-long Widget lifecycle writer active; no reentry."));return R;}TGuardValue<bool> Active(WidgetLifecycleActive,true);
    const bool Assign=Kind==0,Clear=Kind==2;const TCHAR* KindName=Assign?TEXT("assign"):Clear?TEXT("clear"):TEXT("unbind");R.Data->SetStringField(TEXT("node_kind"),KindName);R.Data->SetBoolField(TEXT("widget_dispatcher_lifecycle_contract"),true);R.Data->SetBoolField(TEXT("runtime_delegate_operation_executed"),false);
    FString Dispatcher;if(!DispatcherName(Requested,Dispatcher,R))return R;int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R))return R;
    FWidgetLifecycleWork W(R);if(!W.Begin(Path,FName(*Dispatcher)))return R;UEdGraph* Graph=nullptr;if(!W.SelectGraph(Name,Id,GraphKind,Create,Graph)||!W.AcquireSource(TEXT("lifecycle_signature_bind")))return R;W.NewGraph=Graph;
    TArray<FName> Names;TArray<FEdGraphPinType> Types;if(!W.SignatureTypes(Names,Types,TEXT("lifecycle_signature_types")))return R;
    const FString ClassPath=Assign?TEXT("/Script/BlueprintGraph.K2Node_AssignDelegate"):Clear?TEXT("/Script/BlueprintGraph.K2Node_ClearDelegate"):TEXT("/Script/BlueprintGraph.K2Node_RemoveDelegate");UClass* Class=FindObject<UClass>(nullptr,*ClassPath);TStrongObjectPtr<UClass> ClassHold(Class);if(!W.Current(TEXT("lifecycle_class_lookup_return")))return R;
    const bool Super=Loaded(Class)&&(Assign?Loaded(Class->GetSuperClass())&&Class->GetSuperClass()->GetPathName()==TEXT("/Script/BlueprintGraph.K2Node_AddDelegate")&&Class->GetSuperClass()->GetSuperClass()==UK2Node_BaseMCDelegate::StaticClass():Class->GetSuperClass()==UK2Node_BaseMCDelegate::StaticClass());
    if(!Super||!Class->HasAnyClassFlags(CLASS_Native)||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy.Get()!=nullptr||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)){Fail(R,TEXT("dispatcher_node_class_unavailable"),TEXT("Exact already-loaded native lifecycle class required; no private derived symbol or loader."));return R;}
    const EClassFlags ClassFlags=Class->GetClassFlags();const EObjectFlags ClassObjectFlags=Class->GetFlags();UClass* ClassSuper=Class->GetSuperClass();TStrongObjectPtr<UClass> SuperHold(ClassSuper);
    auto Current=[&](const TCHAR* Stage){if(!W.Current(Stage))return false;if(!Loaded(Class)||Class->GetClassFlags()!=ClassFlags||Class->GetFlags()!=ClassObjectFlags||Class->GetSuperClass()!=ClassSuper||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy.Get()!=nullptr||!Class->GetPathName().Equals(ClassPath,ESearchCase::CaseSensitive))return W.Bad(Stage,TEXT("native_lifecycle_class_role"));return true;};
    uint32 GridSize=0;int32 EventXY[2]={0,0};if(Assign){if(!Grid(GridSize,R)||!Current(TEXT("grid_settings_return")))return R;const int64 X=int64(XY[0])-150,Y=int64(XY[1])+150;if(X<MIN_int32||X>MAX_int32||Y<MIN_int32||Y>MAX_int32){Fail(R,TEXT("dispatcher_assign_offset_overflow"),TEXT("Original implicit event offset exceeds safe int32 range."));return R;}EventXY[0]=Snapped(int32(X),GridSize);EventXY[1]=Snapped(int32(Y),GridSize);}
    const uint64 ExpectedPropertyFlags=W.PropertyFlags;const EFunctionFlags ExpectedSignatureFlags=W.SignatureFlags;
    auto SignatureSnapshot=[&](const TCHAR* Stage){auto J=MakeShared<FJsonObject>();if(!Current(Stage))return J;J->SetBoolField(TEXT("loaded"),true);J->SetStringField(TEXT("path"),W.SignaturePath);J->SetStringField(TEXT("flags_uint32"),LexToString(uint32(W.SignatureFlags)));TArray<TSharedPtr<FJsonValue>> Rows;for(int32 I=0;I<W.Fields.Num();++I){const auto& F=W.Fields[I];auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),F.Name.ToString());Row->SetStringField(TEXT("field_class"),F.Class->GetName());Row->SetStringField(TEXT("flags_uint64"),LexToString(F.Flags));Row->SetBoolField(TEXT("schema_converted"),true);const int32 At=Names.IndexOfByKey(F.Name);if(At==INDEX_NONE){W.Bad(Stage,TEXT("signature_projection_name"));return J;}auto T=Pin::Describe(Types[At]);T->SetBoolField(TEXT("is_const"),Types[At].bIsConst);Row->SetObjectField(TEXT("pin_type"),T);Rows.Add(MakeShared<FJsonValueObject>(Row));}J->SetArrayField(TEXT("parameters"),Rows);J->SetBoolField(TEXT("complete"),true);return J;};const auto SignatureBefore=SignatureSnapshot(TEXT("signature_before_observer"));if(W.Stopped)return R;R.Data->SetObjectField(TEXT("signature_before"),SignatureBefore);
    auto Snapshot=[&](const TCHAR* Stage)->TSharedRef<FJsonObject>
    {
        auto State=MakeShared<FJsonObject>();if(!Current(Stage))return State;State->SetStringField(TEXT("blueprint"),W.Path);State->SetBoolField(TEXT("dirty"),W.Package->IsDirty());State->SetNumberField(TEXT("blueprint_status"),W.BP->Status.GetValue());
        auto Variables=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Descriptors;const auto Copies=W.BP->NewVariables;for(const auto& Copy:Copies){FString Text;FBPVariableDescription::StaticStruct()->ExportText(Text,&Copy,nullptr,W.BP.Get(),PPF_None,nullptr);if(!Current(Stage))return State;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Copy.VarName.ToString());Row->SetStringField(TEXT("guid"),Copy.VarGuid.ToString());Row->SetStringField(TEXT("reflected_descriptor"),Text);Descriptors.Add(MakeShared<FJsonValueObject>(Row));}Variables->SetArrayField(TEXT("ordered_descriptors"),Descriptors);Variables->SetBoolField(TEXT("complete"),true);State->SetObjectField(TEXT("variables"),Variables);
        TArray<TSharedPtr<FJsonValue>> GraphRows,States;for(const auto& G:W.Graphs){auto Row=DescribeGraph(W.BP.Get(),G.Hold.Get());if(!Current(Stage))return State;GraphRows.Add(MakeShared<FJsonValueObject>(Row));}
        for(const auto& G:W.Graphs){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("blueprint"),W.Path);Row->SetBoolField(TEXT("dirty"),W.Package->IsDirty());Row->SetNumberField(TEXT("blueprint_status"),W.BP->Status.GetValue());Row->SetArrayField(TEXT("graphs"),GraphRows);Row->SetObjectField(TEXT("graph"),DescribeGraph(W.BP.Get(),G.Hold.Get()));if(!Current(Stage))return State;TArray<TSharedPtr<FJsonValue>> Nodes;for(const auto& N:G.Nodes){auto Node=W.NodeJson(N.Hold.Get(),Stage);if(W.Stopped)return State;Nodes.Add(MakeShared<FJsonValueObject>(Node));}Row->SetArrayField(TEXT("nodes"),Nodes);States.Add(MakeShared<FJsonValueObject>(Row));}State->SetArrayField(TEXT("graph_states"),States);return State;
    };
    const auto Before=Snapshot(TEXT("lifecycle_before_observation"));if(W.Stopped)return R;R.Data->SetObjectField(TEXT("before_blueprint_state"),Before);
    auto Run=[&]()->FResult
    {
        Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","WidgetDispatcherLifecycle","Add Widget Dispatcher Lifecycle Node"));if(!Current(TEXT("transaction_begin")))return R;
        R.Stage=TEXT("allocate_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;W.Writer=true;Count(R,TEXT("node_allocation_calls"));UK2Node_BaseMCDelegate* Node=NewObject<UK2Node_BaseMCDelegate>(Graph,Class);TStrongObjectPtr<UK2Node_BaseMCDelegate> Hold(Node);Count(R,TEXT("node_allocation_returns"));W.NewNode=Node;W.NewNodeClass=Class;if(Node)W.Holds.Emplace(Node);if(!Current(TEXT("allocation_return")))return R;if(!Node){Fail(R,TEXT("dispatcher_node_allocation_failed"),TEXT("Allocation returned null; actual prefix retained."));return R;}R.Data->SetStringField(TEXT("allocated_node_path"),Node->GetPathName());
        W.NewReference.SetFromField<FProperty>(W.Property,true,W.PropertyOwner.Get());if(!Current(TEXT("planned_reference_return")))return R;Count(R,TEXT("set_from_property_calls"));Node->SetFromProperty(W.Property,true,W.PropertyOwner.Get());W.NewReferenceReady=true;Count(R,TEXT("set_from_property_returns"));if(!Current(TEXT("set_from_property_return")))return R;
        Count(R,TEXT("compatibility_calls"));const bool Compatible=Node->IsCompatibleWithGraph(Graph);R.Data->SetBoolField(TEXT("graph_compatible"),Compatible);if(!Current(TEXT("compatibility_return")))return R;if(!Compatible){Fail(R,TEXT("dispatcher_graph_incompatible"),TEXT("Original late refusal follows allocation/reference assignment; no added DestroyNode cleanup."));return R;}
        Node->NodePosX=XY[0];Node->NodePosY=XY[1];W.RequestedX=XY[0];W.RequestedY=XY[1];W.PlacementReady=true;Count(R,TEXT("graph_add_node_calls"));Graph->AddNode(Node,true);W.NodeMember=true;if(!Current(TEXT("add_node_return")))return R;Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();W.NewId=Node->NodeGuid;W.GuidReady=true;if(!W.NewId.IsValid()){W.Bad(TEXT("guid_return"),TEXT("created_node_guid"));return R;}if(!Current(TEXT("guid_return")))return R;R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());R.Data->SetStringField(TEXT("created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));
        if(Assign)
        {
            Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();if(!Current(TEXT("assign_allocate_pins_return")))return R;auto* Input=Node->GetDelegatePin();if(!Current(TEXT("assign_delegate_pin_return")))return R;if(!Input||!Node->Pins.Contains(Input)||Input->WasTrashed()||Input->Direction!=EGPD_Input||Input->PinType.PinCategory!=UEdGraphSchema_K2::PC_Delegate||!Input->LinkedTo.IsEmpty()){Fail(R,TEXT("dispatcher_assign_delegate_pin_unavailable"),TEXT("Required allocated delegate input absent; no unsafe PostPlaced or cleanup."));return R;}
            // This opaque SDK call owns CustomEvent creation, signature copy, snap and link.
            R.Stage=TEXT("post_placed_with_implicit_event");Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();Count(R,TEXT("post_placed_returns"));
            // Hold new returned inventory before any callback observer; never reuse old pin addresses.
            if(!W.Context()||!Loaded(W.BP.Get())||!Loaded(Graph)||Graph->GetOuter()!=W.BP.Get()){W.Bad(TEXT("post_placed_return"),TEXT("returned_graph_role"));return R;}
            const auto* Prior=W.Graphs.FindByPredicate([&](const FWidgetLifecycleWork::FGraph& Row){return Row.Hold.Get()==Graph;});if(!Prior){W.Bad(TEXT("post_placed_return"),TEXT("prior_graph_role"));return R;}
            const int32 At=Prior->Nodes.Num();if(Graph->Nodes.Num()==At+2&&Graph->Nodes[At+1].IsResolved()){UEdGraphNode* Returned=Graph->Nodes[At+1].Get();TStrongObjectPtr<UEdGraphNode> ReturnedHold(Returned);if(Loaded(Returned)&&Returned->GetClass()==UK2Node_CustomEvent::StaticClass()&&Returned->GetOuter()==Graph){W.ImplicitEvent.Reset(static_cast<UK2Node_CustomEvent*>(Returned));W.Holds.Emplace(Returned);W.PendingDelta=2;}}
            if(!Current(TEXT("post_placed_compound_return")))return R;R.Data->SetNumberField(TEXT("implicit_custom_events_observed"),W.ImplicitEvent.IsValid()?1:0);R.Data->SetNumberField(TEXT("observed_new_node_count"),W.PendingDelta);
            if(!W.ImplicitEvent.IsValid()){if(!W.FreezeNew(TEXT("incomplete_assign_inventory")))return R;const auto Partial=Snapshot(TEXT("incomplete_assign_observation"));if(W.Stopped)return R;R.Data->SetObjectField(TEXT("after_blueprint_state"),Partial);R.bChanged=R.bChangeStateKnown=true;Fail(R,TEXT("dispatcher_assign_event_incomplete"),TEXT("PostPlaced returned without its expected CustomEvent; current accepted node prefix retained without rollback/retry."));return R;}
        }
        else
        {
            Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();Count(R,TEXT("post_placed_returns"));if(!Current(TEXT("post_placed_return")))return R;Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();if(!Current(TEXT("allocate_pins_return")))return R;Count(R,TEXT("reconstruct_calls"));Node->ReconstructNode();if(!Current(TEXT("reconstruct_return")))return R;
        }
        if(!W.FreezeNew(TEXT("returned_lifecycle_inventory")))return R;
        auto Verify=[&](const TCHAR* Stage)
        {
            if(!Current(Stage))return false;UClass* Authoritative=W.PropertyOwner->GetAuthoritativeClass();TStrongObjectPtr<UClass> KeepAuthoritative(Authoritative);if(!Current(Stage))return false;
            auto* Execute=Node->FindPin(UEdGraphSchema_K2::PN_Execute,EGPD_Input);auto* Then=Node->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output);auto* Self=Node->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);bool Good=Node->NodePosX==XY[0]&&Node->NodePosY==XY[1]&&Node->NodeGuid==W.NewId&&Execute&&Then&&Self&&Execute->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Then->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Self->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Self->PinType.PinSubCategoryObject.Get()==Authoritative&&Execute->LinkedTo.IsEmpty()&&Then->LinkedTo.IsEmpty()&&Self->LinkedTo.IsEmpty();
            auto* Delegate=Node->GetDelegatePin();if(!Current(Stage))return false;if(Clear)Good&=Node->Pins.Num()==3&&!Delegate;else{FSimpleMemberReference Ref;FMemberReference::FillSimpleMemberReference<UFunction>(W.Signature.Get(),Ref);if(!Current(Stage))return false;Good&=Node->Pins.Num()==4&&Delegate&&Delegate->Direction==EGPD_Input&&Delegate->PinType.PinCategory==UEdGraphSchema_K2::PC_Delegate&&Delegate->PinType.bIsConst&&Delegate->PinType.bIsReference&&Delegate->PinType.PinSubCategoryMemberReference==Ref;}
            if(Assign){auto* Event=W.ImplicitEvent.Get();auto* Out=Event->FindPin(UK2Node_Event::DelegateOutputName,EGPD_Output);auto* EventThen=Event->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output);Good&=Delegate&&Out&&EventThen&&Delegate->LinkedTo.Num()==1&&Out->LinkedTo.Num()==1&&Delegate->LinkedTo[0]==Out&&Out->LinkedTo[0]==Delegate&&EventThen->LinkedTo.IsEmpty()&&Event->NodePosX==EventXY[0]&&Event->NodePosY==EventXY[1]&&Event->Pins.Num()==Names.Num()+2&&Event->UserDefinedPins.Num()==Names.Num()&&!Event->bOverrideFunction&&!Event->bCallInEditor&&!Event->bIsDeprecated&&Event->GetNetFlags()==0&&Event->FunctionFlags==(FUNC_BlueprintCallable|FUNC_BlueprintEvent|FUNC_Public);if(!Current(Stage))return false;for(int32 I=0;I<Names.Num();++I){const auto* Pin=Event->FindPin(Names[I],EGPD_Output);const auto Info=Event->UserDefinedPins.IsValidIndex(I)?Event->UserDefinedPins[I]:TSharedPtr<FUserPinInfo>();Good&=Pin&&Info.IsValid()&&Pin->LinkedTo.IsEmpty()&&PinsMatchType(Pin->PinType,Types[I])&&Info->PinName==Names[I]&&Info->DesiredPinDirection==EGPD_Output&&PinsMatchType(Info->PinType,Types[I]);}}
            else if(!Clear)Good&=Delegate&&Delegate->LinkedTo.IsEmpty();
            const FProperty* Resolved=Node->GetProperty();if(!Current(Stage))return false;UFunction* Signature=Node->GetDelegateSignature();TStrongObjectPtr<UFunction> ReturnedSignature(Signature);if(!Current(Stage))return false;Good&=Resolved==W.Property&&Signature==W.Signature.Get()&&Node->DelegateReference.IsSameReference(W.NewReference);if(!Good)return W.Bad(Stage,TEXT("lifecycle_typed_self_signature_or_pair"));return true;
        };
        if(!Verify(TEXT("allocated_compound_postcondition")))return R;
        if(Assign)
        {
            // The only structural seam: freeze authored pair first, then reacquire SDK roles.
            W.Rebinding=true;W.ClassesReady=false;W.Candidates.Reset();W.Property=nullptr;R.Stage=TEXT("mark_structural");Structural(W.BP.Get(),R);if(!W.Refresh(TEXT("structural_return")))return R;if(!W.AcquireSource(TEXT("structural_signature_reacquire")))return R;
            TArray<FName> AfterNames;TArray<FEdGraphPinType> AfterTypes;if(!W.SignatureTypes(AfterNames,AfterTypes,TEXT("structural_signature_types")))return R;bool Same=AfterNames.Num()==Names.Num()&&W.PropertyFlags==ExpectedPropertyFlags&&W.SignatureFlags==ExpectedSignatureFlags;for(int32 I=0;Same&&I<Names.Num();++I){const int32 At=AfterNames.IndexOfByKey(Names[I]);Same=At!=INDEX_NONE&&PinsMatchType(Types[I],AfterTypes[At]);}if(!Same){W.Bad(TEXT("structural_return"),TEXT("dispatcher_signature_semantics"));return R;}if(!Verify(TEXT("structural_pair_postcondition")))return R;
        }
        else{R.Stage=TEXT("mark_modified");Mark(W.BP.Get(),R);if(!Current(TEXT("mark_modified_return")))return R;}
        const auto SignatureAfter=SignatureSnapshot(TEXT("signature_after_observer"));if(W.Stopped)return R;R.Data->SetObjectField(TEXT("signature_after"),SignatureAfter);R.Data->SetBoolField(TEXT("native_class_signature_preserved"),true);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"),Assign);R.Data->SetBoolField(TEXT("assign_event_link_verified"),Assign);R.Data->SetNumberField(TEXT("observed_new_node_count"),Assign?2:1);R.Data->SetNumberField(TEXT("expected_event_x"),EventXY[0]);R.Data->SetNumberField(TEXT("expected_event_y"),EventXY[1]);
        const FText Title=Node->GetNodeTitle(ENodeTitleType::FullTitle);if(!Current(TEXT("node_title_return")))return R;const FString Display=Title.ToString();if(!Current(TEXT("node_title_text_return")))return R;
        auto Row=W.NodeJson(Node,TEXT("node_description"));if(W.Stopped)return R;Row->SetStringField(TEXT("name"),Node->GetName());Row->SetStringField(TEXT("class"),Node->GetClass()->GetName());Row->SetStringField(TEXT("title"),Display);Row->SetStringField(TEXT("node_comment"),Node->NodeComment);Row->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);Row->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);R.Data->SetObjectField(TEXT("node"),Row);R.Data->SetObjectField(TEXT("graph"),DescribeGraph(W.BP.Get(),Graph));R.Data->SetStringField(TEXT("dispatcher_name"),Dispatcher);R.Data->SetStringField(TEXT("signature_function"),W.Signature->GetPathName());R.Data->SetBoolField(TEXT("self_context"),true);R.Data->SetBoolField(TEXT("delegate_specialized_pins_verified"),true);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),true);R.Data->SetBoolField(TEXT("graph_rows_preserved"),true);R.Data->SetBoolField(TEXT("old_variable_descriptors_preserved"),true);
        TArray<TSharedPtr<FJsonValue>> NewNodes;NewNodes.Add(MakeShared<FJsonValueObject>(Row));if(Assign){auto Event=W.NodeJson(W.ImplicitEvent.Get(),TEXT("event_description"));if(W.Stopped)return R;Event->SetStringField(TEXT("custom_event_name"),W.ImplicitEvent->CustomFunctionName.ToString());R.Data->SetObjectField(TEXT("implicit_custom_event"),Event);NewNodes.Add(MakeShared<FJsonValueObject>(Event));}R.Data->SetArrayField(TEXT("new_nodes"),NewNodes);
        const auto After=Snapshot(TEXT("lifecycle_after_observation"));if(W.Stopped)return R;R.Data->SetObjectField(TEXT("after_blueprint_state"),After);R.Data->SetBoolField(TEXT("dirty_before"),Before->GetBoolField(TEXT("dirty")));R.Data->SetBoolField(TEXT("dirty_after"),After->GetBoolField(TEXT("dirty")));R.Data->SetBoolField(TEXT("old_nodes_preserved"),true);
        auto Legacy=MakeShared<FJsonObject>();Legacy->Values=Row->Values;Legacy->SetStringField(TEXT("dispatcher_name"),Dispatcher);Legacy->SetStringField(TEXT("node_kind"),KindName);Legacy->SetStringField(TEXT("signature_function"),W.Signature->GetPathName());auto LegacyGraph=DescribeGraph(W.BP.Get(),Graph);LegacyGraph->SetStringField(TEXT("graph_id"),Graph->GraphGuid.ToString());LegacyGraph->RemoveField(TEXT("object_path"));Legacy->SetObjectField(TEXT("graph"),LegacyGraph);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);if(!Current(TEXT("lifecycle_publication")))return R;
        R.bChanged=R.bChangeStateKnown=R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
    };
    Run();if(!W.Stopped&&R.bChangeStateKnown)Current(TEXT("transaction_publication"));return R;
}

// The parent classes are readonly sources. Leaf compiler classes/CDOs can be
// regenerated by Assign; they are reacquired from the same held Blueprint.
struct FGeneratedDispatcherParents
{
    struct FFieldRole{FProperty* P=nullptr;FFieldClass* Kind=nullptr;FName Name;uint64 Flags=0;int32 Offset=0,Size=0,Dim=0;FString Value;UFunction* DelegateSignature=nullptr;};
    struct FClassRole
    {
        TStrongObjectPtr<UBlueprintGeneratedClass> Class;TStrongObjectPtr<UObject> Default;
        UClass* Super=nullptr;FString Path;int32 Size=0;TArray<FFieldRole> Fields;
    };
    struct FParent
    {
        TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UClass> Parent;
        FClassRole Generated,Skeleton;FString Path;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();
        TArray<TStrongObjectPtr<UEdGraph>> Graphs;TArray<TArray<TObjectPtr<UEdGraphNode>>> Nodes;
        TArray<TArray<TArray<UEdGraphPin*>>> Pins;
    };
    FResult& R;TStrongObjectPtr<UBlueprint> Target;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UClass> LeafGenerated,LeafSkeleton,LeafSkeletonParent;
    TStrongObjectPtr<UPackage> TargetPackage;FString TargetPath,GraphPath;FGuid GraphId;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<UClass> Parent;
    TArray<TUniquePtr<FParent>> Parents;TArray<TStrongObjectPtr<UObject>> Holds;
    TStrongObjectPtr<UEdGraphNode> Added;bool Active=false,Stopped=false;int32 AllowedDelta=0;
    TArray<TObjectPtr<UEdGraphNode>> OldNodes;
    explicit FGeneratedDispatcherParents(FResult& In):R(In){}
    bool Stop(const TCHAR* Why,const FString& Object=FString())
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Why);R.Data->SetObjectField(TEXT("generated_dispatcher_first_mismatch"),J);Fail(R,TEXT("dispatcher_generated_parent_scope_changed"),TEXT("Generated parent/source or local writer role changed; actual prefix retained with no further lifecycle SDK, cleanup, compile, save or retry."));}
        Stopped=true;R.bSuccess=R.bPostconditionPassed=R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("generated_dispatcher_followup_sdk_stopped"),true);return false;
    }
    bool ClassCurrent(const FClassRole& C,UBlueprint* B)
    {
        auto* K=C.Class.Get();if(!Loaded(K)||K->GetClass()!=UBlueprintGeneratedClass::StaticClass()||K->GetSuperClass()!=C.Super||K->GetPathName()!=C.Path||K->GetPropertiesSize()!=C.Size||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=B||!K->ClassDefaultObject.IsResolved()||K->ClassDefaultObject.Get()!=C.Default.Get())return Stop(TEXT("parent_class_cdo_role"),C.Path);
        if(C.Default.Get()&&(!Loaded(C.Default.Get())||C.Default->GetClass()!=K||!C.Default->HasAnyFlags(RF_ClassDefaultObject)))return Stop(TEXT("parent_cdo_role"),C.Path);
        int32 I=0;for(TFieldIterator<FProperty> It(K,EFieldIterationFlags::None);It;++It,++I){if(!C.Fields.IsValidIndex(I))return Stop(TEXT("parent_field_inventory"),C.Path);const auto& F=C.Fields[I];auto* P=*It;if(P!=F.P||P->GetOwnerStruct()!=K||P->GetClass()!=F.Kind||P->GetFName()!=F.Name||uint64(P->GetPropertyFlags())!=F.Flags||P->GetOffset_ForInternal()!=F.Offset||P->GetSize()!=F.Size||P->ArrayDim!=F.Dim||P->HasSetterOrGetter())return Stop(TEXT("parent_field_role"),C.Path);if(auto* D=CastField<FMulticastDelegateProperty>(P))if(!D->SignatureFunction.IsResolved()||D->SignatureFunction.Get()!=F.DelegateSignature||(F.DelegateSignature&&!Loaded(F.DelegateSignature)))return Stop(TEXT("parent_delegate_signature_role"),C.Path);}return I==C.Fields.Num()||Stop(TEXT("parent_field_count"),C.Path);
    }
    bool LeafClass(UClass* K)
    {return Loaded(K)&&K->GetClass()==UBlueprintGeneratedClass::StaticClass()&&K->ClassGeneratedBy.IsResolved()&&K->ClassGeneratedBy.Get()==Target.Get()&&Loaded(K->GetSuperClass())&&K->ClassDefaultObject.IsResolved()&&(!K->ClassDefaultObject.Get()||(Loaded(K->ClassDefaultObject.Get())&&K->ClassDefaultObject->GetClass()==K));}
    bool Current()
    {
        if(!Active)return !Stopped;if(Stopped)return false;
        if(!IsEditorGameThread()||IsPlaySessionActive()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Loaded(World.Get())||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get())return Stop(TEXT("editor_world"));
        if(!Loaded(Target.Get())||!Loaded(Parent.Get())||!Loaded(TargetPackage.Get())||Target->GetPackage()!=TargetPackage.Get()||Target->GetPathName()!=TargetPath||!Target->ParentClass.GetGCPtr().IsResolved()||Target->ParentClass.Get()!=Parent.Get()||Target->bBeingCompiled||Target->bQueuedForCompilation||Target->bIsRegeneratingOnLoad||!Target->GeneratedClass.GetGCPtr().IsResolved()||!Target->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!LeafClass(Target->GeneratedClass.Get())||!LeafClass(Target->SkeletonGeneratedClass.Get())||Target->GeneratedClass->GetSuperClass()!=Parent.Get()||Target->SkeletonGeneratedClass->GetSuperClass()!=LeafSkeletonParent.Get())return Stop(TEXT("leaf_class_roles"));
        LeafGenerated.Reset(Target->GeneratedClass.Get());LeafSkeleton.Reset(Target->SkeletonGeneratedClass.Get());
        if(!Loaded(Graph.Get())||Graph->GetPathName()!=GraphPath||Graph->GraphGuid!=GraphId||FBlueprintEditorUtils::FindBlueprintForGraph(Graph.Get())!=Target.Get()||Graph->Nodes.Num()<OldNodes.Num()||Graph->Nodes.Num()>OldNodes.Num()+AllowedDelta)return Stop(TEXT("local_graph_membership"));
        for(int32 I=0;I<OldNodes.Num();++I)if(!Loaded(OldNodes[I].Get())||Graph->Nodes[I]!=OldNodes[I])return Stop(TEXT("local_old_node_membership"));
        if(Added.Get()&&(!Loaded(Added.Get())||Added->GetOuter()!=Graph.Get()||Added->GetGraph()!=Graph.Get()||(AllowedDelta>0&&!Graph->Nodes.Contains(Added.Get()))))return Stop(TEXT("allocated_node_owner"));
        for(const auto& Ptr:Parents)
        {
            const auto& P=*Ptr;auto* B=P.BP.Get();if(!Loaded(B)||B->GetPathName()!=P.Path||B->GetPackage()!=P.Package.Get()||!Loaded(P.Package.Get())||P.Package->IsDirty()||B->Status!=BS_UpToDate||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.Get()!=P.Parent.Get()||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.Get()!=P.Generated.Class.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->SkeletonGeneratedClass.Get()!=P.Skeleton.Class.Get()||!ClassCurrent(P.Generated,B)||!ClassCurrent(P.Skeleton,B))return Stop(TEXT("readonly_parent_role"),P.Path);
            for(int32 I=0;I<P.Graphs.Num();++I){auto* G=P.Graphs[I].Get();if(!Loaded(G)||G->Nodes!=P.Nodes[I])return Stop(TEXT("parent_graph_nodes"),P.Path);for(int32 N=0;N<P.Nodes[I].Num();++N){auto* Node=P.Nodes[I][N].Get();if(!Loaded(Node)||Node->GetOuter()!=G||Node->Pins!=P.Pins[I][N])return Stop(TEXT("parent_pin_inventory"),P.Path);for(auto* Pin:P.Pins[I][N])if(!Pin||Pin->WasTrashed()||Pin->GetOwningNodeUnchecked()!=Node)return Stop(TEXT("parent_pin_lifetime"),P.Path);}}
        }return true;
    }
    bool BindClass(FClassRole& Out,UBlueprintGeneratedClass* K,UBlueprint* B)
    {
        if(!Loaded(K)||K->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=B||!K->ClassDefaultObject.IsResolved())return Stop(TEXT("parent_class_binding"));Out.Class.Reset(K);Out.Default.Reset(K->ClassDefaultObject.Get());Out.Super=K->GetSuperClass();Out.Path=K->GetPathName();Out.Size=K->GetPropertiesSize();Holds.Emplace(Out.Super);
        for(TFieldIterator<FProperty> It(K,EFieldIterationFlags::None);It;++It){auto* P=*It;if(Out.Fields.Num()>=256||P->HasSetterOrGetter()||P->GetOwnerStruct()!=K||P->ArrayDim<1||P->GetOffset_ForInternal()<0||int64(P->GetOffset_ForInternal())+P->GetSize()>Out.Size)return Stop(TEXT("parent_field_scope"));FFieldRole F;F.P=P;F.Kind=P->GetClass();F.Name=P->GetFName();F.Flags=uint64(P->GetPropertyFlags());F.Offset=P->GetOffset_ForInternal();F.Size=P->GetSize();F.Dim=P->ArrayDim;if(auto* D=CastField<FMulticastDelegateProperty>(P)){if(!D->SignatureFunction.IsResolved())return Stop(TEXT("parent_delegate_signature_unresolved"));F.DelegateSignature=D->SignatureFunction.Get();if(F.DelegateSignature){if(!Loaded(F.DelegateSignature))return Stop(TEXT("parent_delegate_signature_unavailable"));Holds.Emplace(F.DelegateSignature);}}Out.Fields.Add(MoveTemp(F));}
        return ClassCurrent(Out,B);
    }
    bool Defaults(FClassRole& C,UBlueprint* B,bool CaptureValues)
    {
        if(!ClassCurrent(C,B))return false;if(!C.Default.Get())return true;
        for(auto& F:C.Fields){if(F.P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))continue;FString Value;const bool Exported=F.P->ExportText_Direct(Value,F.P->ContainerPtrToValuePtr<void>(C.Default.Get()),nullptr,C.Default.Get(),PPF_None);if(!Current())return false;if(!Exported||!ClassCurrent(C,B)||Value.Len()>65536)return Stop(TEXT("parent_default_observation"),C.Path);if(CaptureValues)F.Value=MoveTemp(Value);else if(!Value.Equals(F.Value,ESearchCase::CaseSensitive))return Stop(TEXT("readonly_parent_default"),C.Path+TEXT(".")+F.Name.ToString());}return true;
    }
    bool Bind(UBlueprint* B,UEdGraph* G)
    {
        if(Active)return Target.Get()==B&&Graph.Get()==G&&Current();
        if(!Loaded(B)||!Loaded(G)||B->BlueprintType!=BPTYPE_Normal||!B->ParentClass.GetGCPtr().IsResolved()||!Loaded(B->ParentClass.Get())||!B->ParentClass->IsChildOf(AActor::StaticClass())||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!Loaded(B->SkeletonGeneratedClass.Get()))return Stop(TEXT("generated_actor_profile"));
        Target.Reset(B);Graph.Reset(G);TargetPackage.Reset(B->GetPackage());TargetPath=B->GetPathName();GraphPath=G->GetPathName();GraphId=G->GraphGuid;Parent.Reset(B->ParentClass.Get());LeafSkeletonParent.Reset(B->SkeletonGeneratedClass->GetSuperClass());Editor.Reset(GEditor);World.Reset(GEditor->GetEditorWorldContext().World());OldNodes=G->Nodes;for(const auto& N:OldNodes)Holds.Emplace(N.Get());Active=true;
        TSet<UClass*> Seen;UClass* At=Parent.Get();
        while(Loaded(At)&&At->GetClass()==UBlueprintGeneratedClass::StaticClass())
        {
            if(Parents.Num()>=16||Seen.Contains(At)||!At->ClassGeneratedBy.IsResolved())return Stop(TEXT("parent_chain_bound"));Seen.Add(At);auto* Source=Cast<UBlueprint>(At->ClassGeneratedBy.Get());
            if(!Loaded(Source)||Source==B||Source->GetClass()!=UBlueprint::StaticClass()||Source->BlueprintType!=BPTYPE_Normal||!Source->GetPathName().StartsWith(TEXT("/Game/"))||Source->GetPackage()==B->GetPackage()||Source->GetPackage()->IsDirty()||Source->Status!=BS_UpToDate||!Source->ParentClass.GetGCPtr().IsResolved()||!Source->GeneratedClass.GetGCPtr().IsResolved()||Source->GeneratedClass.Get()!=At||!Source->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Stop(TEXT("saved_loaded_parent_required"));
            auto P=MakeUnique<FParent>();P->BP.Reset(Source);P->Package.Reset(Source->GetPackage());P->Parent.Reset(Source->ParentClass.Get());P->Path=Source->GetPathName();
            if(!BindClass(P->Generated,static_cast<UBlueprintGeneratedClass*>(At),Source)||!BindClass(P->Skeleton,Cast<UBlueprintGeneratedClass>(Source->SkeletonGeneratedClass.Get()),Source))return false;
            TArray<UEdGraph*> Graphs;Source->GetAllGraphs(Graphs);if(Graphs.Num()>64)return Stop(TEXT("parent_graph_bound"));for(auto* PG:Graphs){if(!Loaded(PG)||PG->Nodes.Num()>MaxNodes)return Stop(TEXT("parent_graph_role"));P->Graphs.Emplace(PG);P->Nodes.Add(PG->Nodes);TArray<TArray<UEdGraphPin*>> Pins;for(const auto& Slot:PG->Nodes){auto* N=Slot.Get();if(!Loaded(N)||N->GetOuter()!=PG)return Stop(TEXT("parent_node_role"));Holds.Emplace(N);Pins.Add(N->Pins);}P->Pins.Add(MoveTemp(Pins));}
            auto* Captured=P.Get();Parents.Add(MoveTemp(P));if(!Current()||!Defaults(Captured->Generated,Source,true)||!Defaults(Captured->Skeleton,Source,true)||!BlueprintState(Source,Captured->Before,R)||!Current())return false;At=Source->ParentClass.Get();
        }
        for(int32 Depth=0;At;At=At->GetSuperClass()){if(++Depth>64||!Loaded(At)||!At->HasAnyClassFlags(CLASS_Native)||At->IsEditorOnly()||(At->GetPackage()->GetName()!=TEXT("/Script/Engine")&&At->GetPackage()->GetName()!=TEXT("/Script/CoreUObject")))return Stop(TEXT("native_parent_root"));Holds.Emplace(At);}
        Active=true;R.Data->SetBoolField(TEXT("generated_parent_dispatcher_lifecycle_supported"),true);R.Data->SetNumberField(TEXT("readonly_generated_parents"),Parents.Num());R.Data->SetBoolField(TEXT("generated_parent_additional_outer_transaction"),false);R.Data->SetStringField(TEXT("generated_parent_preservation_scope"),TEXT("Loaded saved Actor ancestors, their actual generated/skeleton superclass and nullable CDO roles, own reflected default text and authored graph/variable states. Leaf CDO defaults are independent. Assign implicit leaf compilation may replace generated caches. No parent writer, new transaction, load, explicit compile/save, deep referenced-object purity, standalone Undo or full inheritance parity is claimed."));return Current();
    }
    bool Final()
    {
        if(!Current())return false;for(auto& Ptr:Parents){auto& P=*Ptr;auto Now=MakeShared<FJsonObject>();if(!Defaults(P.Generated,P.BP.Get(),false)||!Defaults(P.Skeleton,P.BP.Get(),false)||!BlueprintState(P.BP.Get(),Now,R)||!Current())return false;if(!Equal(MakeShared<FJsonValueObject>(P.Before),MakeShared<FJsonValueObject>(Now)))return Stop(TEXT("readonly_parent_authored_state"),P.Path);}R.Data->SetBoolField(TEXT("generated_parent_sources_preserved"),true);return true;
    }
};

FResult Lifecycle(int32 Kind,const FString& Path,const FString& Requested,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphKind,bool Create)
{
    FResult R;Init(R);const bool Assign=Kind==0,Clear=Kind==2;const TCHAR* KindName=Assign?TEXT("assign"):Clear?TEXT("clear"):TEXT("unbind");R.Data->SetStringField(TEXT("node_kind"),KindName);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("dispatcher_editor_invalid"),TEXT("Idle Editor game thread required."));return R; }
    if(GeneratedParentAdmission){Fail(R,TEXT("dispatcher_generated_reentry"),TEXT("Generated dispatcher lifecycle already active; no nested writer."));return R;}
    FString LoadedPath=Path;if(LoadedPath.StartsWith(TEXT("/Game/"))&&!LoadedPath.Contains(TEXT(".")))LoadedPath+=TEXT(".")+FPackageName::GetShortName(LoadedPath);
    auto* LoadedBP=FindObject<UBlueprint>(nullptr,*LoadedPath);
    const bool GeneratedRequest=Loaded(LoadedBP)&&LoadedBP->ParentClass.GetGCPtr().IsResolved()&&Loaded(LoadedBP->ParentClass.Get())&&LoadedBP->ParentClass->GetClass()==UBlueprintGeneratedClass::StaticClass();
    FGeneratedDispatcherParents Parents(R);TFunction<bool(UBlueprint*,UEdGraph*)> Admission=[&](UBlueprint* B,UEdGraph* G){return GeneratedRequest&&B==LoadedBP&&Parents.Bind(B,G);};TFunction<bool()> CurrentScope=[&](){return Parents.Current();};TFunction<bool(const TCHAR*)> BorrowStop=[&](const TCHAR* Why){return Parents.Stop(Why);};
    TGuardValue<const TFunction<bool(UBlueprint*,UEdGraph*)>*> AdmissionLease(GeneratedParentAdmission,GeneratedRequest?&Admission:nullptr);
    TGuardValue<const TFunction<bool()>*> SourceLease(GeneratedParentFence,GeneratedRequest?&CurrentScope:nullptr);
    TGuardValue<const TFunction<bool(const TCHAR*)>*> BorrowLease(GeneratedParentBorrowStop,GeneratedRequest?&BorrowStop:nullptr);
    // The original native helper has no outer transaction. Do not introduce a
    // destructor End into this new scope; Batch retains its separate owner.
    TUniquePtr<FScopedTransaction> Tx;if(!GeneratedRequest){Count(R,TEXT("outer_transaction_calls"));Tx=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP","DispatcherLifecycle","Add Blueprint Dispatcher Lifecycle Node"));}
    FString Name;if(!DispatcherName(Requested,Name,R)) { return R; }R.Data->SetStringField(TEXT("dispatcher_name"),Name);int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> BeforeGraph;if(!Graph(Path,GraphName,GraphId,GraphKind,GeneratedRequest?false:Create,BP,G,BeforeGraph,R)) { return R; }if(!Parents.Current())return R;const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid GraphGuid=G->GraphGuid;auto* P=Property(BP,FName(*Name),R);R.Data->SetObjectField(TEXT("property_before"),PropertyJson(P,R));
    if(!P||!P->HasAllPropertyFlags(CPF_BlueprintAssignable)||!Loaded(P->SignatureFunction)||!Loaded(P->GetOwnerClass())) { Fail(R,TEXT("dispatcher_property_unavailable"),TEXT("Original lifecycle requires BlueprintAssignable property and current loaded signature/owner after graph resolution."));return R; }
    const TStrongObjectPtr<UClass> KeepOwner(P->GetOwnerClass());const TStrongObjectPtr<UFunction> KeepSignature(P->SignatureFunction);const auto SigBefore=Signature(P->SignatureFunction,R);R.Data->SetObjectField(TEXT("signature_before"),SigBefore);TArray<FName> ParamNames;TArray<FEdGraphPinType> ParamTypes;TArray<TStrongObjectPtr<UObject>> KeepTypes;
    uint32 GridSize=0;int32 EventXY[2]={0,0};if(Assign)
    {
        if(!Grid(GridSize,R)||!IncludedSignature(P->SignatureFunction,ParamNames,ParamTypes,KeepTypes,R)) { return R; }
        const int64 X=static_cast<int64>(XY[0])-150,Y=static_cast<int64>(XY[1])+150;if(X<MIN_int32||X>MAX_int32||Y<MIN_int32||Y>MAX_int32) { Fail(R,TEXT("dispatcher_assign_offset_overflow"),TEXT("SDK implicit event offset exceeds safe int32 conversion; no writer invoked."));return R; }EventXY[0]=Snapped(static_cast<int32>(X),GridSize);EventXY[1]=Snapped(static_cast<int32>(Y),GridSize);
    }
    const FString ClassPath=Assign?TEXT("/Script/BlueprintGraph.K2Node_AssignDelegate"):Clear?TEXT("/Script/BlueprintGraph.K2Node_ClearDelegate"):TEXT("/Script/BlueprintGraph.K2Node_RemoveDelegate");Count(R,TEXT("node_class_lookup_calls"));auto* Class=FindObject<UClass>(nullptr,*ClassPath);Count(R,TEXT("node_class_lookup_returns"));
    const bool Super=Class&&(Assign?Class->GetSuperClass()&&Class->GetSuperClass()->GetPathName().Equals(TEXT("/Script/BlueprintGraph.K2Node_AddDelegate"),ESearchCase::CaseSensitive)&&Class->GetSuperClass()->GetSuperClass()==UK2Node_BaseMCDelegate::StaticClass():Class->GetSuperClass()==UK2Node_BaseMCDelegate::StaticClass());
    if(!Loaded(Class)||!Super||!Class->HasAnyClassFlags(CLASS_Native)||Class->ClassGeneratedBy||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)) { Fail(R,TEXT("dispatcher_node_class_unavailable"),TEXT("Exact loaded native delegate derived class required; no unexported derived imports or forced module load."));return R; }
    const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!BlueprintState(BP,Before,R)) { return R; }R.Data->SetObjectField(TEXT("before_blueprint_state"),Before);
    if(!Parents.Current())return R;R.Stage=TEXT("allocate_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* N=NewObject<UK2Node_BaseMCDelegate>(G,Class);Count(R,TEXT("node_allocation_returns"));if(GeneratedRequest)R.Data->SetBoolField(TEXT("generated_node_allocation_returned_nonnull"),N!=nullptr);if(!Parents.Current())return R;if(GeneratedRequest&&(!Loaded(N)||N->GetOuter()!=G))return Parents.Stop(TEXT("returned_node_lifetime_owner")),R;const TStrongObjectPtr<UK2Node_BaseMCDelegate> KeepNode(N);Parents.Added.Reset(N);R.Data->SetStringField(TEXT("allocated_node_path"),GetPathNameSafe(N));if(!Parents.Current())return R;if(!N) { Fail(R,TEXT("dispatcher_node_allocation_failed"),TEXT("NewObject returnedNull; allocation prefix retained."));return R; }
    if(GeneratedRequest){P=Property(BP,FName(*Name),R);if(!P||!Loaded(P->GetOwnerClass())||!Loaded(P->SignatureFunction))return Parents.Stop(TEXT("current_delegate_before_binding")),R;}
    const bool Self=!P->GetOwnerClass()||(BP->SkeletonGeneratedClass&&BP->SkeletonGeneratedClass->IsChildOf(P->GetOwnerClass()));R.Data->SetBoolField(TEXT("self_context"),Self);Count(R,TEXT("set_from_property_calls"));N->SetFromProperty(P,Self,P->GetOwnerClass());Count(R,TEXT("set_from_property_returns"));if(!Parents.Current())return R;UEdGraphNode* Base=N;Count(R,TEXT("compatibility_calls"));const bool Compatible=Base->IsCompatibleWithGraph(G);R.Data->SetBoolField(TEXT("graph_compatible"),Compatible);if(!Parents.Current())return R;
    if(!Compatible) { R.Data->SetObjectField(TEXT("allocated_node_prefix"),DescribeNode(Base));Fail(R,TEXT("dispatcher_graph_incompatible"),TEXT("Original refusal follows node allocation and reference assignment. No added Destroy/atomic cleanup."));return R; }
    Base->NodePosX=XY[0];Base->NodePosY=XY[1];R.Stage=TEXT("place_node");Count(R,TEXT("graph_add_node_calls"));Parents.AllowedDelta=1;G->AddNode(Base,true);if(!Parents.Current())return R;Count(R,TEXT("new_guid_calls"));Base->CreateNewGuid();if(!Parents.Current())return R;R.Data->SetStringField(TEXT("created_node_path"),Base->GetPathName());R.Data->SetStringField(TEXT("created_node_guid"),Base->NodeGuid.ToString(EGuidFormats::Digits));
    if(Assign)
    {
        Count(R,TEXT("allocate_pins_calls"));Base->AllocateDefaultPins();if(!Parents.Current())return R;R.Data->SetObjectField(TEXT("after_allocate_node"),DescribeNode(Base));if(!Parents.Current())return R;
        if(!N->GetDelegatePin()||N->GetDelegatePin()->Direction!=EGPD_Input||N->GetDelegatePin()->PinType.PinCategory!=UEdGraphSchema_K2::PC_Delegate||!N->GetDelegatePin()->LinkedTo.IsEmpty()) { Observe(BP,G,Base,Before,nullptr,R);Fail(R,TEXT("dispatcher_assign_delegate_pin_unavailable"),TEXT("SDK Assign dereferences allocated delegate pin; unavailable pin preserves accepted node prefix without PostPlaced or cleanup."));return R; }
        R.Stage=TEXT("post_placed_with_implicit_event");Count(R,TEXT("post_placed_calls"));Parents.AllowedDelta=Assign?2:1;Base->PostPlacedNewNode();Count(R,TEXT("post_placed_returns"));if(!Parents.Current())return R;R.Data->SetObjectField(TEXT("after_post_placed_node"),DescribeNode(Base));if(!Parents.Current())return R;TSet<FString> OldPaths;for(const auto& V:BeforeGraph->GetArrayField(TEXT("nodes"))) { OldPaths.Add(V->AsObject()->GetStringField(TEXT("object_path"))); }TArray<TSharedPtr<FJsonValue>> Prefix;for(UEdGraphNode* Candidate:G->Nodes) { if(Loaded(Candidate)&&!OldPaths.Contains(Candidate->GetPathName())) { Prefix.Add(MakeShared<FJsonValueObject>(DescribeNode(Candidate)));if(!Parents.Current())return R; } }R.Data->SetArrayField(TEXT("post_placed_new_node_prefix"),Prefix);
    }
    else { Count(R,TEXT("post_placed_calls"));Parents.AllowedDelta=Assign?2:1;Base->PostPlacedNewNode();Count(R,TEXT("post_placed_returns"));if(!Parents.Current())return R;Count(R,TEXT("allocate_pins_calls"));Base->AllocateDefaultPins();if(!Parents.Current())return R;Count(R,TEXT("reconstruct_calls"));Base->ReconstructNode();if(!Parents.Current())return R; }
    if(Assign) { R.Stage=TEXT("mark_structural");Structural(BP,R); }else { Mark(BP,R); }if(!Parents.Current())return R;
    const bool Observed=Observe(BP,G,Base,Before,nullptr,R);if(!Parents.Current())return R;auto* Current=Property(BP,FName(*Name),R);R.Data->SetObjectField(TEXT("property_after"),PropertyJson(Current,R));const auto SigAfter=Signature(Current?Current->SignatureFunction.Get():nullptr,R);if(!Parents.Current())return R;R.Data->SetObjectField(TEXT("signature_after"),SigAfter);const auto ClassAfter=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);const bool SourceSame=Equal(MakeShared<FJsonValueObject>(SigBefore),MakeShared<FJsonValueObject>(SigAfter))&&Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter));R.Data->SetBoolField(TEXT("native_class_signature_preserved"),SourceSame);
    bool Pins=DelegatePins(N,Current,Clear,Assign,R),Compound=!Assign;UK2Node_CustomEvent* E=nullptr;
    if(Assign)
    {
        TArray<UK2Node_CustomEvent*> AddedEvents;TSet<FString> OldPaths;for(const auto& V:BeforeGraph->GetArrayField(TEXT("nodes"))) { OldPaths.Add(V->AsObject()->GetStringField(TEXT("object_path"))); }
        for(UEdGraphNode* Candidate:G->Nodes) { if(Candidate&&!OldPaths.Contains(Candidate->GetPathName())) { if(auto* Event=Cast<UK2Node_CustomEvent>(Candidate)) { AddedEvents.Add(Event); } } }
        R.Data->SetNumberField(TEXT("implicit_custom_events_observed"),AddedEvents.Num());TArray<TSharedPtr<FJsonValue>> Events;for(auto* Event:AddedEvents) { const TStrongObjectPtr<UK2Node_CustomEvent> HoldEvent(Event);auto J=DescribeNode(Event);if(!Parents.Current())return R;J->SetObjectField(TEXT("event_details"),EventDetails(Event,R));if(!Parents.Current())return R;Events.Add(MakeShared<FJsonValueObject>(J)); }R.Data->SetArrayField(TEXT("implicit_custom_event_prefix"),Events);
        if(AddedEvents.Num()==1) { E=AddedEvents[0];UEdGraphNode* EventBase=E;auto* In=N->GetDelegatePin();auto* Out=EventBase->FindPin(UK2Node_Event::DelegateOutputName,EGPD_Output);Compound=TypedEvent(E,ParamNames,ParamTypes,true,R)&&In&&Out&&In->LinkedTo.Num()==1&&Out->LinkedTo.Num()==1&&In->LinkedTo[0]==Out&&Out->LinkedTo[0]==In&&E->NodePosX==EventXY[0]&&E->NodePosY==EventXY[1]&&E->HasAnyFlags(RF_Transactional); }
        R.Data->SetBoolField(TEXT("assign_event_link_verified"),Compound);R.Data->SetNumberField(TEXT("expected_event_x"),EventXY[0]);R.Data->SetNumberField(TEXT("expected_event_y"),EventXY[1]);
    }
    Legacy(Base,BP,G,KindName,Name,Current?Current->SignatureFunction.Get():nullptr,R);if(!Parents.Current()||(GeneratedRequest&&!Parents.Final()))return R;
    if(!Observed) { return R; }if(!Current||N->GetProperty()!=Current||N->GetDelegateSignature()!=Current->SignatureFunction||!Pins||!Compound||!SourceSame||Base->GetClass()!=Class||Base->NodePosX!=XY[0]||Base->NodePosY!=XY[1]||G->GraphGuid!=GraphGuid||R.Data->GetNumberField(TEXT("observed_new_node_count"))!=(Assign?2:1)) { Fail(R,TEXT("dispatcher_lifecycle_postcondition_failed"),TEXT("Actual selected delegate/event/signature/link/position prefix differs; no automatic Destroy/rollback/compile/save."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}

FMulticastDelegateProperty* NativeProperty(UClass* Class,FName Name,FResult& R)
{
    Count(R,TEXT("property_lookup_calls"));FMulticastDelegateProperty* Found=nullptr;for(TFieldIterator<FMulticastDelegateProperty> It(Class,EFieldIteratorFlags::IncludeSuper);It;++It) { if(It->GetFName()==Name) { Found=*It;break; } }Count(R,TEXT("property_lookup_returns"));return Found;
}
bool Selector(const FString& Value,FResult& R)
{
    bool Safe=Value.Len()<=1024;for(int32 I=0;I<Value.Len();++I) { Safe&=Value[I]!=0; }return Safe||Fail(R,TEXT("custom_event_selector_invalid"),TEXT("Signature selector exceeds bounded1024/interior-NUL admission; original larger variants remain tracked."));
}
FResult WidgetCustom(const FString& Path,const FString& Requested,const FString& PositionJson,const FString& RawDispatcher,const FString& RawClass,const FString& RawDelegate,const FString& NativeDelegate,bool CallInEditor,bool HasCall,const FString& GraphName,const FString& GraphId,const FString& GraphKind,bool Create)
{
    FResult R;Init(R);if(WidgetLifecycleActive){Fail(R,TEXT("widget_dispatcher_lifecycle_busy"),TEXT("Request-long Widget authoring is active."));return R;}TGuardValue<bool> Active(WidgetLifecycleActive,true);
    R.Data->SetBoolField(TEXT("widget_explicit_custom_event_contract"),true);FString Name;if(!DispatcherName(Requested,Name,R))return R;int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R))return R;
    const FString Dispatcher=RawDispatcher.TrimStartAndEnd(),ClassName=RawClass.TrimStartAndEnd(),Delegate=(RawDelegate.IsEmpty()?NativeDelegate:RawDelegate).TrimStartAndEnd();
    if(!Selector(Dispatcher,R)||!Selector(ClassName,R)||!Selector(Delegate,R))return R;if(Dispatcher.Len()>=NAME_SIZE||Delegate.Len()>=NAME_SIZE){Fail(R,TEXT("custom_event_source_name_overflow"),TEXT("Source names must fit FName."));return R;}
    const bool RequestedSignature=!Dispatcher.IsEmpty()||!ClassName.IsEmpty()||!Delegate.IsEmpty();R.Data->SetStringField(TEXT("custom_event_name"),Name);R.Data->SetStringField(TEXT("signature_source_dispatcher_name"),Dispatcher);R.Data->SetStringField(TEXT("signature_source_class"),ClassName);R.Data->SetStringField(TEXT("signature_source_delegate_name"),Delegate);R.Data->SetBoolField(TEXT("requested_signature_source"),RequestedSignature);R.Data->SetBoolField(TEXT("call_in_editor_supplied"),HasCall);R.Data->SetBoolField(TEXT("requested_call_in_editor"),CallInEditor);
    FWidgetLifecycleWork W(R);if(!W.Begin(Path,FName(*Dispatcher)))return R;UEdGraph* Graph=nullptr;if(!W.SelectGraph(GraphName,GraphId,GraphKind,Create,Graph))return R;W.NewGraph=Graph;
    TArray<UK2Node_CustomEvent*> Matches;for(const auto& G:W.Graphs)for(const auto& N:G.Nodes)if(N.ReferenceKind==4&&N.CustomName==FName(*Name))Matches.Add(static_cast<UK2Node_CustomEvent*>(N.Hold.Get()));R.Data->SetNumberField(TEXT("existing_custom_event_matches"),Matches.Num());
    if(Matches.Num()>1){Fail(R,TEXT("custom_event_ambiguous_pending"),TEXT("Ambiguous existing event is not selected."));return R;}
    UK2Node_CustomEvent* Event=Matches.Num()==1?Matches[0]:nullptr;const bool Reuse=Event!=nullptr;
    if(Reuse&&RequestedSignature){Fail(R,TEXT("custom_event_signature_already_exists"),TEXT("Original existing event refuses every supplied signature source."));return R;}
    if(Reuse){Graph=Event->GetGraph();W.NewGraph=Graph;if(Event->GetClass()!=UK2Node_CustomEvent::StaticClass()||Event->GetNetFlags()!=0){Fail(R,TEXT("custom_event_reuse_scope_pending"),TEXT("Own non-networked stock CustomEvent required."));return R;}}
    if(!Reuse&&Dispatcher.IsEmpty()&&RequestedSignature){Fail(R,TEXT("widget_custom_event_external_signature_pending"),TEXT("First Widget scope supports own dispatcher or no signature. Native/external delegate source remains incomplete; no loader or writer invoked."));return R;}
    TArray<FName> Names;TArray<FEdGraphPinType> Types;uint64 ExpectedProperty=0;EFunctionFlags ExpectedSignature=FUNC_None;
    if(!Reuse&&!Dispatcher.IsEmpty()){if(!W.AcquireSource(TEXT("explicit_event_signature_bind"))||!W.SignatureTypes(Names,Types,TEXT("explicit_event_signature_types")))return R;ExpectedProperty=W.PropertyFlags;ExpectedSignature=W.SignatureFlags;}
    uint32 GridSize=0;int32 ExpectedXY[2]={XY[0],XY[1]};if(W.Signature.IsValid()){if(!Grid(GridSize,R)||!W.Current(TEXT("explicit_event_grid_return")))return R;ExpectedXY[0]=Snapped(XY[0],GridSize);ExpectedXY[1]=Snapped(XY[1],GridSize);}
    const auto Before=WidgetAuthoredSnapshot(W,TEXT("explicit_event_before"));if(W.Stopped)return R;R.Data->SetObjectField(TEXT("before_blueprint_state"),Before);
    const FGuid ReusedId=Reuse?Event->NodeGuid:FGuid();const int32 ReusedX=Reuse?Event->NodePosX:0,ReusedY=Reuse?Event->NodePosY:0;const bool PreviousEditor=Reuse?Event->bCallInEditor:false;
    auto Verify=[&](const TCHAR* Stage)
    {
        if(!W.Current(Stage))return false;
        if(!Loaded(Event)||Event->GetClass()!=UK2Node_CustomEvent::StaticClass()||Event->GetOuter()!=Graph||Event->CustomFunctionName!=FName(*Name)||Event->GetNetFlags()!=0||Event->bCallInEditor!=(HasCall?CallInEditor:PreviousEditor))return W.Bad(Stage,TEXT("explicit_event_name_class_editor"));
        if(Reuse){if(Event->NodeGuid!=ReusedId||Event->NodePosX!=ReusedX||Event->NodePosY!=ReusedY)return W.Bad(Stage,TEXT("reused_event_guid_position"));return true;}
        if(Event->NodeGuid!=W.NewId||Event->NodePosX!=ExpectedXY[0]||Event->NodePosY!=ExpectedXY[1])return W.Bad(Stage,TEXT("explicit_event_guid_position"));
        const bool Good=TypedEvent(Event,Names,Types,false,R);if(!W.Current(Stage))return false;if(!Good)return W.Bad(Stage,TEXT("explicit_event_typed_signature"));return true;
    };
    auto Run=[&]()
    {
        Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","WidgetExplicitEvent","Add Widget Explicit Custom Event"));if(!W.Current(TEXT("explicit_event_transaction")))return;
        if(!Reuse)
        {
            R.bMutationAttempted=true;R.bChangeStateKnown=false;W.Writer=true;R.Stage=W.Signature.IsValid()?TEXT("create_from_signature"):TEXT("allocate_custom_event");
            if(W.Signature.IsValid())
            {
                Count(R,TEXT("create_from_function_calls"));Event=UK2Node_CustomEvent::CreateFromFunction(FVector2D(XY[0],XY[1]),Graph,Name,W.Signature.Get(),false);TStrongObjectPtr<UK2Node_CustomEvent> Returned(Event);Count(R,TEXT("create_from_function_returns"));W.NewNode=Event;W.NewNodeClass=UK2Node_CustomEvent::StaticClass();W.ExplicitCustomName=FName(*Name);W.ExplicitCustomReady=Event!=nullptr&&W.Signature.IsValid();if(Event)W.Holds.Emplace(Event);W.NodeMember=Event!=nullptr;
                if(!W.Current(TEXT("explicit_event_factory_return")))return;if(!Event){Fail(R,TEXT("custom_event_create_failed"),TEXT("Factory returned null; actual prefix retained without cleanup/retry."));return;}
                W.NewId=Event->NodeGuid;W.GuidReady=true;W.RequestedX=ExpectedXY[0];W.RequestedY=ExpectedXY[1];W.PlacementReady=true;
            }
            else
            {
                Count(R,TEXT("node_allocation_calls"));Event=NewObject<UK2Node_CustomEvent>(Graph);TStrongObjectPtr<UK2Node_CustomEvent> Returned(Event);Count(R,TEXT("node_allocation_returns"));W.NewNode=Event;W.NewNodeClass=UK2Node_CustomEvent::StaticClass();W.ExplicitCustomName=FName(*Name);W.ExplicitCustomReady=Event!=nullptr&&W.Signature.IsValid();if(Event)W.Holds.Emplace(Event);if(!W.Current(TEXT("explicit_event_allocate_return")))return;if(!Event){Fail(R,TEXT("custom_event_create_failed"),TEXT("Allocation returned null; prefix retained."));return;}
                Event->CustomFunctionName=FName(*Name);W.ExplicitCustomReady=true;Event->SetFlags(RF_Transactional);Event->NodePosX=XY[0];Event->NodePosY=XY[1];W.RequestedX=XY[0];W.RequestedY=XY[1];W.PlacementReady=true;
                Count(R,TEXT("explicit_graph_modify_calls"));Graph->Modify();if(!W.Current(TEXT("explicit_event_graph_modify_return")))return;Count(R,TEXT("graph_add_node_calls"));Graph->AddNode(Event,true,false);W.NodeMember=true;if(!W.Current(TEXT("explicit_event_add_return")))return;
                Count(R,TEXT("new_guid_calls"));Event->CreateNewGuid();W.NewId=Event->NodeGuid;W.GuidReady=true;if(!W.Current(TEXT("explicit_event_guid_return")))return;Count(R,TEXT("post_placed_calls"));Event->PostPlacedNewNode();Count(R,TEXT("post_placed_returns"));if(!W.Current(TEXT("explicit_event_post_return")))return;Count(R,TEXT("allocate_pins_calls"));Event->AllocateDefaultPins();if(!W.Current(TEXT("explicit_event_pins_return")))return;
            }
            if(!W.NewId.IsValid()||Event->CustomFunctionName!=FName(*Name)||Event->NodePosX!=ExpectedXY[0]||Event->NodePosY!=ExpectedXY[1]){W.Bad(TEXT("explicit_event_creation_return"),TEXT("factory_name_guid_position"));return;}
            if(HasCall&&CallInEditor){Event->bCallInEditor=true;W.ExplicitCustomEditor=true;Count(R,TEXT("call_in_editor_write_calls"));}
            if(!W.FreezeNew(TEXT("explicit_event_allocated_inventory"))||!Verify(TEXT("explicit_event_allocated_shape")))return;
        }
        else if(HasCall)
        {
            R.bMutationAttempted=true;R.bChangeStateKnown=false;W.Writer=true;R.Stage=TEXT("update_reused_call_in_editor");Count(R,TEXT("explicit_modify_calls"));Event->Modify();if(!W.Current(TEXT("reused_event_modify_return")))return;
            Event->bCallInEditor=CallInEditor;for(auto& G:W.Graphs)for(auto& N:G.Nodes)if(N.Hold.Get()==Event)N.CustomEditor=CallInEditor;Count(R,TEXT("call_in_editor_write_calls"));if(!W.Current(TEXT("reused_event_editor_write")))return;
        }
        if(!Reuse||HasCall)
        {
            W.Property=nullptr;W.Rebinding=true;W.ClassesReady=false;W.Candidates.Reset();Count(R,TEXT("reconstruct_calls"));Event->ReconstructNode();if(!W.Refresh(TEXT("explicit_event_reconstruct_return")))return;
            W.Property=nullptr;W.Rebinding=true;W.ClassesReady=false;W.Candidates.Reset();R.Stage=TEXT("mark_structural");Structural(W.BP.Get(),R);if(!W.Refresh(TEXT("explicit_event_structural_return")))return;
            if(!Reuse&&!Dispatcher.IsEmpty())
            {
                if(!W.AcquireSource(TEXT("explicit_event_source_reacquire")))return;TArray<FName> NewNames;TArray<FEdGraphPinType> NewTypes;if(!W.SignatureTypes(NewNames,NewTypes,TEXT("explicit_event_source_types")))return;
                bool Same=NewNames==Names&&NewTypes.Num()==Types.Num()&&W.PropertyFlags==ExpectedProperty&&W.SignatureFlags==ExpectedSignature;for(int32 I=0;Same&&I<Types.Num();++I)Same=PinsMatchType(Types[I],NewTypes[I]);if(!Same){W.Bad(TEXT("explicit_event_structural_return"),TEXT("own_dispatcher_signature"));return;}
            }
        }
        if(!Verify(TEXT("explicit_event_final_shape")))return;const auto After=WidgetAuthoredSnapshot(W,TEXT("explicit_event_after"));if(W.Stopped)return;R.Data->SetObjectField(TEXT("after_blueprint_state"),After);R.Data->SetBoolField(TEXT("reused_existing"),Reuse);R.Data->SetBoolField(TEXT("call_in_editor"),Event->bCallInEditor);R.Data->SetNumberField(TEXT("observed_new_node_count"),Reuse?0:1);
        auto Row=W.NodeJson(Event,TEXT("explicit_event_publication"));if(W.Stopped)return;Row->SetStringField(TEXT("custom_event_name"),Name);Row->SetBoolField(TEXT("reused_existing"),Reuse);Row->SetBoolField(TEXT("call_in_editor"),Event->bCallInEditor);auto GraphRow=DescribeGraph(W.BP.Get(),Graph);if(!W.Current(TEXT("explicit_event_graph_publication")))return;GraphRow->SetStringField(TEXT("graph_id"),Graph->GraphGuid.ToString());Row->SetObjectField(TEXT("graph"),GraphRow);if(W.Signature.IsValid())Row->SetStringField(TEXT("signature_function"),W.SignaturePath);R.Data->SetObjectField(TEXT("node"),Row);R.Data->SetObjectField(TEXT("graph"),GraphRow);R.Data->SetObjectField(TEXT("legacy_result"),Row);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),true);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"),!Reuse||HasCall);
        R.bChangeStateKnown=true;R.bChanged=!Reuse||(HasCall&&PreviousEditor!=CallInEditor)||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After));R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");
    };
    Run();if(!W.Stopped&&R.bMutationAttempted&&!R.bChangeStateKnown){if(W.Current(TEXT("explicit_event_failed_prefix"))){R.bChanged=true;R.bChangeStateKnown=true;}}if(!W.Stopped&&R.bChangeStateKnown)W.Current(TEXT("explicit_event_transaction_publication"));return R;
}

FResult Custom(const FString& Path,const FString& Requested,const FString& PositionJson,const FString& RawDispatcher,const FString& RawClass,const FString& RawDelegate,const FString& NativeDelegate,bool CallInEditor,bool HasCall,const FString& GraphName,const FString& GraphId,const FString& GraphKind,bool Create)
{
    FResult R;Init(R);if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("custom_event_editor_invalid"),TEXT("Original CustomEvent requires idle Editor game thread; PIE/SIE refused."));return R; }Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","CustomEventSignature","Add Blueprint Custom Event"));FString Name;if(!DispatcherName(Requested,Name,R)) { return R; }R.Data->SetStringField(TEXT("custom_event_name"),Name);int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> BeforeGraph;if(!Graph(Path,GraphName,GraphId,GraphKind,Create,BP,G,BeforeGraph,R)) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepRequestedGraph(G);if(GraphType(BP,G)!=TEXT("event")) { Fail(R,TEXT("custom_event_graph_invalid"),TEXT("Original custom events require an event graph after graph resolution."));return R; }
    // Public native_delegate_name fallback is falsey-empty BEFORE trim. A truthy
    // whitespace source_delegate must not activate a second alias fallback.
    const FString Dispatcher=RawDispatcher.TrimStartAndEnd(),ClassName=RawClass.TrimStartAndEnd(),Delegate=(RawDelegate.IsEmpty()?NativeDelegate:RawDelegate).TrimStartAndEnd();
    if(!Selector(Dispatcher,R)||!Selector(ClassName,R)||!Selector(Delegate,R)) { return R; }if(Dispatcher.Len()>=NAME_SIZE||Delegate.Len()>=NAME_SIZE) { Fail(R,TEXT("custom_event_source_name_overflow"),TEXT("Source dispatcher/delegate must fit FName before lookup."));return R; }const bool RequestedSignature=!Dispatcher.IsEmpty()||!ClassName.IsEmpty()||!Delegate.IsEmpty();R.Data->SetStringField(TEXT("signature_source_dispatcher_name"),Dispatcher);R.Data->SetStringField(TEXT("signature_source_class"),ClassName);R.Data->SetStringField(TEXT("signature_source_delegate_name"),Delegate);R.Data->SetBoolField(TEXT("call_in_editor_supplied"),HasCall);R.Data->SetBoolField(TEXT("requested_call_in_editor"),CallInEditor);R.Data->SetBoolField(TEXT("requested_signature_source"),RequestedSignature);
    TArray<UK2Node_CustomEvent*> Matches;TArray<UEdGraph*> AllGraphs;BP->GetAllGraphs(AllGraphs);int32 TotalNodes=0;if(AllGraphs.Num()>64) { Fail(R,TEXT("custom_event_inventory_budget"),TEXT("Finite64 graph inventory required before event lookup."));return R; }for(UEdGraph* Candidate:AllGraphs) { if(!Loaded(Candidate)||(TotalNodes+=Candidate->Nodes.Num())>4096) { Fail(R,TEXT("custom_event_inventory_incomplete"),TEXT("Current loaded4096-node inventory required before event lookup."));return R; }for(UEdGraphNode* N:Candidate->Nodes) { if(auto* E=Cast<UK2Node_CustomEvent>(N)) { if(E->CustomFunctionName==FName(*Name)) { Matches.Add(E); } } } }
    R.Data->SetNumberField(TEXT("existing_custom_event_matches"),Matches.Num());if(Matches.Num()>1) { Fail(R,TEXT("custom_event_ambiguous_pending"),TEXT("Original first-match ambiguous event reuse remains residual; finite path refuses before event writer."));return R; }
    if(Matches.Num()==1)
    {
        auto* E=Matches[0];if(RequestedSignature) { R.Data->SetObjectField(TEXT("existing_event_prefix"),DescribeNode(E));Fail(R,TEXT("custom_event_signature_already_exists"),TEXT("Original existing event refuses every supplied signature source. Delete it or choose a new name."));return R; }
        G=E->GetGraph();if(!OwnerScope(BP,G,R)||GraphType(BP,G)!=TEXT("event")||E->GetClass()!=UK2Node_CustomEvent::StaticClass()||E->GetNetFlags()!=0) { Fail(R,TEXT("custom_event_reuse_scope_pending"),TEXT("Single loaded own event in finite native-parent event graph required; Net/derived/other owners remain valid residuals."));return R; }
        const TStrongObjectPtr<UEdGraph> KeepGraph(G);const TStrongObjectPtr<UK2Node_CustomEvent> KeepEvent(E);const FGuid Guid=E->NodeGuid,GraphGuid=G->GraphGuid;const int32 OldX=E->NodePosX,OldY=E->NodePosY;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!BlueprintState(BP,Before,R)) { return R; }R.Data->SetObjectField(TEXT("before_blueprint_state"),Before);const auto DetailsBefore=EventDetails(E,R);R.Data->SetObjectField(TEXT("event_details_before"),DetailsBefore);R.Data->SetObjectField(TEXT("requested_graph_before"),BeforeGraph);
        for(const auto& V:Before->GetArrayField(TEXT("graph_states"))) { const auto O=V->AsObject();if(O->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")).Equals(G->GetPathName(),ESearchCase::CaseSensitive)) { R.Data->SetObjectField(TEXT("before"),O);break; } }
        if(HasCall)
        {
            R.Stage=TEXT("update_reused_call_in_editor");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("explicit_modify_calls"));E->Modify();Count(R,TEXT("call_in_editor_write_calls"));E->bCallInEditor=CallInEditor;UEdGraphNode* Base=E;Count(R,TEXT("reconstruct_calls"));Base->ReconstructNode();R.Data->SetObjectField(TEXT("reused_event_after_reconstruct"),DescribeNode(E));R.Stage=TEXT("mark_structural");Structural(BP,R);
        }
        const bool Observed=Observe(BP,G,E,Before,HasCall?E:nullptr,R);const auto DetailsAfter=EventDetails(E,R);R.Data->SetObjectField(TEXT("event_details_after"),DetailsAfter);R.Data->SetBoolField(TEXT("reused_existing"),true);R.Data->SetBoolField(TEXT("call_in_editor"),E->bCallInEditor);Legacy(E,BP,G,nullptr,Name,nullptr,R);auto J=R.Data->GetObjectField(TEXT("legacy_result"));J->RemoveField(TEXT("signature_function"));J->SetBoolField(TEXT("reused_existing"),true);J->SetBoolField(TEXT("call_in_editor"),E->bCallInEditor);const bool SameDetails=SameExceptCallFlag(DetailsBefore,DetailsAfter);R.Data->SetBoolField(TEXT("reused_signature_defaults_preserved"),SameDetails);
        if(!Observed) { return R; }if(!SameDetails||E->NodeGuid!=Guid||G->GraphGuid!=GraphGuid||E->NodePosX!=OldX||E->NodePosY!=OldY||!LivePins(E,true)||(HasCall&&E->bCallInEditor!=CallInEditor)||(!HasCall&&!Equal(MakeShared<FJsonValueObject>(DetailsBefore),MakeShared<FJsonValueObject>(DetailsAfter)))||R.Data->GetNumberField(TEXT("observed_new_node_count"))!=0) { Fail(R,TEXT("custom_event_reuse_postcondition_failed"),TEXT("Actual reused typed event/flag/pins/defaults/links differ; no event recreation, cleanup or retry."));return R; }
        R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
    }
    UClass* SourceClass=nullptr;FMulticastDelegateProperty* P=nullptr;UFunction* Sig=nullptr;FName SourceName;const bool BlueprintSource=!Dispatcher.IsEmpty();
    if(BlueprintSource)
    {
        SourceName=FName(*Dispatcher);P=Property(BP,SourceName,R);R.Data->SetObjectField(TEXT("property_before"),PropertyJson(P,R));if(!P||!Loaded(P->SignatureFunction)||!Loaded(P->GetOwnerClass())) { Fail(R,TEXT("custom_event_dispatcher_signature_unavailable"),TEXT("Original dispatcher signature missing/unavailable after graph resolution."));return R; }SourceClass=P->GetOwnerClass();Sig=P->SignatureFunction;
    }
    else if(RequestedSignature)
    {
        if(ClassName.IsEmpty()||Delegate.IsEmpty()) { Fail(R,TEXT("custom_event_native_source_missing"),TEXT("Both signature_source_class and signature_source_delegate_name required."));return R; }if(!ReferenceGuard(ClassName)) { Fail(R,TEXT("custom_event_source_dependency_reference"),TEXT("Original native source class dependency guard refuses MCP-only references."));return R; }
        Count(R,TEXT("source_class_lookup_calls"));SourceClass=FindObject<UClass>(nullptr,*ClassName);Count(R,TEXT("source_class_lookup_returns"));R.Data->SetBoolField(TEXT("source_class_was_loaded"),SourceClass!=nullptr);
        if(!SourceClass) { Count(R,TEXT("source_class_load_calls"));SourceClass=LoadObject<UClass>(nullptr,*ClassName);Count(R,TEXT("source_class_load_returns"));R.Data->SetStringField(TEXT("source_class_load_returned_path"),GetPathNameSafe(SourceClass));R.Data->SetStringField(TEXT("source_class_load_scope"),TEXT("Original class loader retained; PostLoad/module/cache effects precede current source snapshot and are not fully observed.")); }
        if(!Loaded(SourceClass)||!SourceClass->HasAnyClassFlags(CLASS_Native)||SourceClass->ClassGeneratedBy||SourceClass->IsEditorOnly()||SourceClass->GetOutermost()->GetName().Equals(TEXT("/Script/CubelessMCPUE58Editor"),ESearchCase::CaseSensitive)||SourceClass->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)) { if(R.Data->GetNumberField(TEXT("source_class_load_calls"))>0) { R.bChangeStateKnown=false; }Fail(R,TEXT("custom_event_native_class_scope_pending"),TEXT("Loaded native runtime delegate source class required; generated/editor/custom source variants remain residuals."));return R; }
        SourceName=FName(*Delegate);P=NativeProperty(SourceClass,SourceName,R);R.Data->SetObjectField(TEXT("property_before"),PropertyJson(P,R));if(!P||!P->HasAllPropertyFlags(CPF_BlueprintAssignable)||!Loaded(P->SignatureFunction)||!Loaded(P->GetOwnerClass())) { Fail(R,TEXT("custom_event_native_delegate_unavailable"),TEXT("Original native delegate requires BlueprintAssignable and signature; class-load prefix retained."));return R; }Sig=P->SignatureFunction;
    }
    const TStrongObjectPtr<UClass> KeepSourceClass(SourceClass),KeepSignatureOwner(Sig?Sig->GetOwnerClass():nullptr);const TStrongObjectPtr<UFunction> KeepSignature(Sig);TArray<FName> ParamNames;TArray<FEdGraphPinType> ParamTypes;TArray<TStrongObjectPtr<UObject>> KeepTypes;TSharedRef<FJsonObject> SigBefore=MakeShared<FJsonObject>(),SourceBefore=MakeShared<FJsonObject>();uint32 GridSize=0;int32 ExpectedXY[2]={XY[0],XY[1]};
    if(Sig)
    {
        SigBefore=Signature(Sig,R);R.Data->SetObjectField(TEXT("signature_before"),SigBefore);SourceBefore=Source(SourceClass,R);R.Data->SetObjectField(TEXT("signature_owner_source_before"),SourceBefore);if(!Grid(GridSize,R)||!IncludedSignature(Sig,ParamNames,ParamTypes,KeepTypes,R)) { return R; }ExpectedXY[0]=Snapped(XY[0],GridSize);ExpectedXY[1]=Snapped(XY[1],GridSize);
    }
    Count(R,TEXT("node_class_lookup_calls"));auto* Class=UK2Node_CustomEvent::StaticClass();Count(R,TEXT("node_class_lookup_returns"));if(!Loaded(Class)||!Class->GetPathName().Equals(TEXT("/Script/BlueprintGraph.K2Node_CustomEvent"),ESearchCase::CaseSensitive)) { Fail(R,TEXT("custom_event_class_unavailable"),TEXT("Exact loaded Engine CustomEvent class required."));return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!BlueprintState(BP,Before,R)) { return R; }R.Data->SetObjectField(TEXT("before_blueprint_state"),Before);const FGuid GraphGuid=G->GraphGuid;const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    R.Stage=Sig?TEXT("create_from_signature"):TEXT("allocate_custom_event");R.bMutationAttempted=true;R.bChangeStateKnown=false;UK2Node_CustomEvent* E=nullptr;
    if(Sig)
    {
        Count(R,TEXT("create_from_function_calls"));E=UK2Node_CustomEvent::CreateFromFunction(FVector2D(XY[0],XY[1]),G,Name,Sig,false);Count(R,TEXT("create_from_function_returns"));R.Data->SetStringField(TEXT("create_from_function_returned_path"),GetPathNameSafe(E));R.Data->SetBoolField(TEXT("create_from_function_returned_null"),E==nullptr);R.Data->SetStringField(TEXT("factory_counter_scope"),TEXT("One public factory call. SDK internally allocates/Modify/Add/GUID/Post/Allocate/copies included user pins/Snap; direct counters do not fabricate those nested calls."));
    }
    else
    {
        Count(R,TEXT("node_allocation_calls"));E=NewObject<UK2Node_CustomEvent>(G);Count(R,TEXT("node_allocation_returns"));R.Data->SetStringField(TEXT("allocated_node_path"),GetPathNameSafe(E));if(E)
        {
            E->CustomFunctionName=FName(*Name);Count(R,TEXT("transactional_flag_write_calls"));E->SetFlags(RF_Transactional);E->NodePosX=XY[0];E->NodePosY=XY[1];Count(R,TEXT("explicit_graph_modify_calls"));G->Modify();Count(R,TEXT("graph_add_node_calls"));G->AddNode(E,true,false);UEdGraphNode* Base=E;Count(R,TEXT("new_guid_calls"));Base->CreateNewGuid();Count(R,TEXT("post_placed_calls"));Base->PostPlacedNewNode();Count(R,TEXT("post_placed_returns"));Count(R,TEXT("allocate_pins_calls"));Base->AllocateDefaultPins();
        }
    }
    const TStrongObjectPtr<UK2Node_CustomEvent> KeepEvent(E);if(!E) { Observe(BP,G,nullptr,Before,nullptr,R);Fail(R,TEXT("custom_event_create_failed"),TEXT("Original event creation returnedNull; actual allocation/factory prefix retained without cleanup."));return R; }R.Data->SetStringField(TEXT("created_node_path"),E->GetPathName());R.Data->SetStringField(TEXT("created_node_guid"),E->NodeGuid.ToString(EGuidFormats::Digits));R.Data->SetObjectField(TEXT("created_event_before_reconstruct"),DescribeNode(E));R.Data->SetObjectField(TEXT("created_event_details_before_reconstruct"),EventDetails(E,R));
    if(HasCall&&CallInEditor) { Count(R,TEXT("call_in_editor_write_calls"));E->bCallInEditor=true; }UEdGraphNode* Base=E;Count(R,TEXT("reconstruct_calls"));Base->ReconstructNode();R.Data->SetObjectField(TEXT("created_event_after_reconstruct"),DescribeNode(E));R.Stage=TEXT("mark_structural");Structural(BP,R);const bool Observed=Observe(BP,G,Base,Before,nullptr,R);R.Data->SetObjectField(TEXT("event_details_after"),EventDetails(E,R));R.Data->SetBoolField(TEXT("reused_existing"),false);R.Data->SetBoolField(TEXT("call_in_editor"),E->bCallInEditor);
    UFunction* CurrentSig=nullptr;UClass* CurrentSource=SourceClass;bool SourceSame=true;
    if(Sig)
    {
        auto* Current=BlueprintSource?Property(BP,SourceName,R):NativeProperty(SourceClass,SourceName,R);R.Data->SetObjectField(TEXT("property_after"),PropertyJson(Current,R));CurrentSig=Current?Current->SignatureFunction.Get():nullptr;if(BlueprintSource) { CurrentSource=Current?Current->GetOwnerClass():nullptr; }const auto SigAfter=Signature(CurrentSig,R);R.Data->SetObjectField(TEXT("signature_after"),SigAfter);SourceSame=Equal(MakeShared<FJsonValueObject>(SigBefore),MakeShared<FJsonValueObject>(SigAfter));
        // Skeleton regeneration can replace the owning class. Do not compare its
        // address or dereference the old property; native source is read-only.
        if(!BlueprintSource&&CurrentSource) { const auto SourceAfter=Source(CurrentSource,R);R.Data->SetObjectField(TEXT("signature_owner_source_after"),SourceAfter);SourceSame&=Equal(MakeShared<FJsonValueObject>(SourceBefore),MakeShared<FJsonValueObject>(SourceAfter)); }
    }
    const auto ClassAfter=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);SourceSame&=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter));R.Data->SetBoolField(TEXT("signature_native_class_preserved"),SourceSame);const bool Typed=TypedEvent(E,ParamNames,ParamTypes,false,R);R.Data->SetNumberField(TEXT("expected_event_x"),ExpectedXY[0]);R.Data->SetNumberField(TEXT("expected_event_y"),ExpectedXY[1]);Legacy(Base,BP,G,nullptr,Name,CurrentSig,R);auto J=R.Data->GetObjectField(TEXT("legacy_result"));J->SetStringField(TEXT("signature_source_dispatcher_name"),Dispatcher);J->SetStringField(TEXT("signature_source_class"),ClassName);J->SetStringField(TEXT("signature_source_delegate_name"),Delegate);
    if(!Observed) { return R; }if(!Typed||!SourceSame||E->GetClass()!=Class||E->CustomFunctionName!=FName(*Name)||E->NodePosX!=ExpectedXY[0]||E->NodePosY!=ExpectedXY[1]||E->bCallInEditor!=(HasCall&&CallInEditor)||!E->HasAnyFlags(RF_Transactional)||G->GraphGuid!=GraphGuid||R.Data->GetNumberField(TEXT("observed_new_node_count"))!=1||(Sig&&!Loaded(CurrentSig))) { Fail(R,TEXT("custom_event_signature_postcondition_failed"),TEXT("Current included signature/user pins/flag/snapping/old state proof failed. Factory/new node prefix retained without cleanup, compiler, save or retry."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}

void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("allocate_pins_calls"),TEXT("blueprint_capture_calls"),TEXT("call_in_editor_write_calls"),TEXT("compatibility_calls"),TEXT("create_from_function_calls"),TEXT("create_from_function_returns"),TEXT("event_detail_observation_calls"),TEXT("explicit_graph_modify_calls"),TEXT("explicit_modify_calls"),TEXT("external_scope_observation_calls"),TEXT("graph_add_node_calls"),TEXT("graph_capture_calls"),TEXT("grid_settings_observation_calls"),TEXT("included_parameter_type_observation_calls"),TEXT("legacy_node_metadata_observation_calls"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("mark_structural_calls"),TEXT("mark_structural_returns"),TEXT("new_guid_calls"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("node_class_lookup_calls"),TEXT("node_class_lookup_returns"),TEXT("outer_transaction_calls"),TEXT("post_placed_calls"),TEXT("post_placed_returns"),TEXT("property_lookup_calls"),TEXT("property_lookup_returns"),TEXT("property_observation_calls"),TEXT("reconstruct_calls"),TEXT("set_from_property_calls"),TEXT("set_from_property_returns"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("signature_observation_calls"),TEXT("source_class_load_calls"),TEXT("source_class_load_returns"),TEXT("source_class_lookup_calls"),TEXT("source_class_lookup_returns"),TEXT("source_observation_calls"),TEXT("transactional_flag_write_calls"),TEXT("variable_observation_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("destroy_node_calls"),TEXT("cleanup_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetBoolField(TEXT("cleanup_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);
    R.Data->SetStringField(TEXT("counter_scope"),TEXT("Direct API calls and observed returns only. Assign PostPlaced internally creates/copies/snaps/connects another event; CreateFromFunction internally allocates/Modify/Add/GUID/Post/Allocate/copies/Snap. Structural notifications can synchronously regenerate skeleton/CDO/caches/children. Explicit fullcompile/save0 is not total compilation/effects0."));
    R.Data->SetStringField(TEXT("preservation_scope"),TEXT("Ordered owned K2 graph rows/nodes/pin identities/value types/defaults/links and member descriptor text, current selected delegate/signature/native classes. No atomic rollback; no automatic cleanup. Arbitrary CDO/SCS/world/cache/children/live runtime effects and full compiled/generated-event metadata are not exhaustively observed."));
}
}
Function::FResult AddAssign(const FString& P,const FString& D,const FString& X,const FString& N,const FString& I,const FString& T,bool C) { if(WidgetLifecycleActive){FResult R;Init(R);Fail(R,TEXT("widget_dispatcher_lifecycle_busy"),TEXT("Widget lifecycle request active; no reentry."));return R;}if(WidgetLifecyclePath(P))return WidgetLifecycle(0,P,D,X,N,I,T,C);return Lifecycle(0,P,D,X,N,I,T,C); }
Function::FResult AddUnbind(const FString& P,const FString& D,const FString& X,const FString& N,const FString& I,const FString& T,bool C) { if(WidgetLifecycleActive){FResult R;Init(R);Fail(R,TEXT("widget_dispatcher_lifecycle_busy"),TEXT("Widget lifecycle request active; no reentry."));return R;}if(WidgetLifecyclePath(P))return WidgetLifecycle(1,P,D,X,N,I,T,C);return Lifecycle(1,P,D,X,N,I,T,C); }
Function::FResult AddClear(const FString& P,const FString& D,const FString& X,const FString& N,const FString& I,const FString& T,bool C) { if(WidgetLifecycleActive){FResult R;Init(R);Fail(R,TEXT("widget_dispatcher_lifecycle_busy"),TEXT("Widget lifecycle request active; no reentry."));return R;}if(WidgetLifecyclePath(P))return WidgetLifecycle(2,P,D,X,N,I,T,C);return Lifecycle(2,P,D,X,N,I,T,C); }
Function::FResult AddCustomEventSignature(const FString& P,const FString& E,const FString& X,const FString& D,const FString& S,const FString& V,const FString& A,bool Call,bool Has,const FString& N,const FString& I,const FString& T,bool C) { if(WidgetLifecyclePath(P))return WidgetCustom(P,E,X,D,S,V,A,Call,Has,N,I,T,C);return Custom(P,E,X,D,S,V,A,Call,Has,N,I,T,C); }
}
