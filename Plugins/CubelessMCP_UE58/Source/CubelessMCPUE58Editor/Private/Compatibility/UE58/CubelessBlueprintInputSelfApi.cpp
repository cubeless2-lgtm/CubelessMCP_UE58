// DF legacy mapped input event and current-owner Self node authoring only.
#include "Compatibility/UE58/CubelessBlueprintInputSelfApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "InputCoreTypes.h"
#include "K2Node.h"
#include "K2Node_InputAction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "BlueprintEditorSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Internationalization/Text.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "Misc/PackageName.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintInputSelf
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("external_scope_observation_calls"),TEXT("node_class_lookup_calls"),TEXT("node_class_lookup_returns"),TEXT("source_observation_calls"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("input_field_observation_calls"),TEXT("action_name_set_calls"),TEXT("action_name_set_returns"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("legacy_node_metadata_observation_calls"),TEXT("graph_compatibility_observation_calls"),TEXT("static_function_observation_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_graph_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("destroy_node_calls"),TEXT("input_mapping_registration_calls"),TEXT("input_settings_read_calls"),TEXT("runtime_input_evaluation_calls"),TEXT("runtime_self_lookup_calls"),TEXT("explicit_cdo_construction_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Original NewObject, action-name assignment where requested, AddNode(true), GUID, PostPlaced, AllocateDefaultPins and MarkBlueprintAsModified. No extra Modify/transactional flag/Reconstruct/Destroy, input-map change, runtime self/input evaluation or explicit compile/save. Shared resolver can create a requested function graph; graph notifications and MarkModified have implicit dirty/status/cache/child effects. Counts are direct calls, not SDK-internal totals."));
    R.Data->SetStringField(TEXT("preservation_scope"),TEXT("Selected graph node/pin/default/link identities and ordered other graph rows; exact native node-class/runtime parent/key-struct identity, flags and package-dirty fields. Full Blueprint variable/CDO/SCS/world/editor-cache/loader/module effects are not observed or claimed zero."));
    R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);
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

void LegacyNodeFields(UEdGraphNode* Node,FResult& R)
{
    Count(R,TEXT("legacy_node_metadata_observation_calls"));R.Data->SetStringField(TEXT("name"),Node->GetName());R.Data->SetStringField(TEXT("class"),Node->GetClass()->GetName());
    R.Data->SetStringField(TEXT("title"),Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());R.Data->SetStringField(TEXT("node_comment"),Node->NodeComment);
    R.Data->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);
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

bool FreshPins(UEdGraphNode* Node)
{
    TSet<FGuid> Ids;
    for(const auto* P:Node->Pins)
    { if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return false; }Ids.Add(P->PinId); }
    return true;
}
bool OwnerScope(UBlueprint* BP,UEdGraph* G,FResult& R)
{
    if(!Loaded(BP)||!Loaded(G)||!Loaded(BP->ParentClass)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP
        ||(BP->BlueprintType!=BPTYPE_Normal&&BP->BlueprintType!=BPTYPE_FunctionLibrary&&BP->BlueprintType!=BPTYPE_MacroLibrary)
        ||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad
        ||BP->Status==BS_BeingCreated||!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>=2048)
    { return Fail(R,TEXT("input_self_owner_scope_pending"),TEXT("Idle owned editable K2 graph and ordinary native runtime Blueprint owner required; broader owners remain tracked.")); }
    int32 Depth=0;
    for(UClass* C=BP->ParentClass;C;C=C->GetSuperClass())
    {
        const FString Package=C->GetOutermost()->GetName();
        if(++Depth>64||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->IsEditorOnly()
            ||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)
            ||(!Package.Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)&&!Package.Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)))
        { return Fail(R,TEXT("input_self_parent_scope_pending"),TEXT("Finite native Engine/CoreUObject parent hierarchy required; not restricted to direct Actor. Generated/custom/editor/plugin parents remain valid residuals.")); }
    }
    Count(R,TEXT("external_scope_observation_calls"));TArray<TSharedPtr<FJsonValue>> External;
    for(TObjectIterator<UBlueprint> It;It;++It)
    { if(*It!=BP&&It->ParentClass&&BP->GeneratedClass&&It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.Add(MakeShared<FJsonValueString>(It->GetPathName())); } }
    if(BP->GeneratedClass)
    { TArray<UObject*> Instances;GetObjectsOfClass(BP->GeneratedClass,Instances,true,RF_NoFlags);for(UObject* O:Instances) { if(IsValid(O)&&O->GetOutermost()!=BP->GetOutermost()) { External.Add(MakeShared<FJsonValueString>(O->GetPathName())); } } }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"),External);
    return External.IsEmpty()||Fail(R,TEXT("input_self_external_instances_pending"),TEXT("Loaded external instances/derived Blueprint callback effects are outside this observed scope."));
}
bool Graph(const FString& Path,const FString& Name,const FString& Id,const FString& Type,bool Create,UBlueprint*& BP,UEdGraph*& G,TSharedPtr<FJsonObject>& Before,FResult& R)
{
    R.Stage=TEXT("resolve_graph");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(Path,Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));
    R.bMutationAttempted=S.bMutationAttempted;R.bChanged=S.bChanged;R.bChangeStateKnown=S.bChangeStateKnown;
    if(!S.bSuccess||!S.bPostconditionPassed) { return Fail(R,S.ErrorCode.IsEmpty()?TEXT("input_self_graph_unverified"):*S.ErrorCode,S.Error); }
    BP=FindObject<UBlueprint>(nullptr,*S.Data->GetStringField(TEXT("blueprint")));const auto J=S.Data->GetObjectField(TEXT("graph"));G=FindObject<UEdGraph>(nullptr,*J->GetStringField(TEXT("object_path")));
    Before=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetStringField(TEXT("blueprint"),S.Data->GetStringField(TEXT("blueprint")));
    return OwnerScope(BP,G,R);
}
TSharedRef<FJsonObject> Source(const UStruct* Struct,FResult& R)
{
    Count(R,TEXT("source_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Struct->GetPathName());J->SetStringField(TEXT("class"),Struct->GetClass()->GetPathName());J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(Struct->GetFlags())));J->SetStringField(TEXT("super"),GetPathNameSafe(Struct->GetSuperStruct()));J->SetStringField(TEXT("package"),Struct->GetOutermost()->GetName());J->SetStringField(TEXT("package_flags"),LexToString(static_cast<uint32>(Struct->GetOutermost()->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Struct->GetOutermost()->IsDirty());
    if(const UClass* Class=Cast<UClass>(Struct)) { J->SetStringField(TEXT("class_flags"),LexToString(static_cast<uint32>(Class->GetClassFlags())));J->SetStringField(TEXT("generated_by"),GetPathNameSafe(Class->ClassGeneratedBy)); }
    if(const UScriptStruct* Script=Cast<UScriptStruct>(Struct)) { J->SetStringField(TEXT("struct_flags"),LexToString(static_cast<uint32>(Script->StructFlags))); }
    return J;
}
UClass* NodeClass(bool Input,FResult& R)
{
    const FString Path=Input?TEXT("/Script/BlueprintGraph.K2Node_InputAction"):TEXT("/Script/BlueprintGraph.K2Node_Self");
    Count(R,TEXT("node_class_lookup_calls"));UClass* Class=FindObject<UClass>(nullptr,*Path);Count(R,TEXT("node_class_lookup_returns"));
    R.Data->SetStringField(TEXT("requested_node_class"),Path);R.Data->SetStringField(TEXT("actual_node_class"),GetPathNameSafe(Class));
    if(!Loaded(Class)||!Class->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||Class->GetSuperClass()!=UK2Node::StaticClass()
        ||!Class->HasAnyClassFlags(CLASS_Native)||Class->ClassGeneratedBy||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)
        ||!Class->GetOutermost()->GetName().Equals(TEXT("/Script/BlueprintGraph"),ESearchCase::CaseSensitive))
    { Fail(R,TEXT("input_self_node_class_unavailable"),TEXT("Exact already-loaded native BlueprintGraph node class required; no class/module load, alternate node or unexported derived import."));return nullptr; }
    return Class;
}
TSharedRef<FJsonObject> InputFields(const UK2Node_InputAction* Node,FResult& R)
{
    Count(R,TEXT("input_field_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("action_name"),Node->InputActionName.ToString());J->SetBoolField(TEXT("consume_input"),Node->bConsumeInput);J->SetBoolField(TEXT("execute_when_paused"),Node->bExecuteWhenPaused);J->SetBoolField(TEXT("override_parent_binding"),Node->bOverrideParentBinding);return J;
}
bool OutputPin(const UEdGraphPin* P,const FName& Category,const FName& SubCategory,const UObject* Object)
{
    return P&&P->Direction==EGPD_Output&&P->PinType.PinCategory==Category&&P->PinType.PinSubCategory==SubCategory&&P->PinType.PinSubCategoryObject.Get()==Object
        &&!P->PinType.IsContainer()&&!P->PinType.bIsReference&&!P->PinType.bIsConst&&!P->PinType.bIsWeakPointer&&P->DefaultValue.IsEmpty()&&!P->DefaultObject&&P->DefaultTextValue.IsEmpty();
}
bool After(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const FGuid& Guid,const TSharedPtr<FJsonObject>& Before,FResult& R)
{
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(BP->GetPathName(),TEXT(""),Guid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(S));
    if(!S.bSuccess||!S.bChangeStateKnown||!S.bPostconditionPassed)
    { R.bChangeStateKnown=false;return Fail(R,TEXT("input_self_after_incomplete"),TEXT("Current graph observation did not close; created node/notification prefix retained with no retry or new cleanup.")); }
    const auto A=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),A);R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true;
    const auto* Row=A->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(Row) { R.Data->SetObjectField(TEXT("node"),(*Row)->AsObject());for(const auto& P:(*Row)->AsObject()->Values) { R.Data->SetField(P.Key,P.Value); }LegacyNodeFields(Node,R); }
    const bool Old=ExistingNodesPreserved(Before,A,1),Rows=GraphRowsPreserved(Before,A,G->GetPathName(),1);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);R.Data->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());Legacy->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    return (Old&&Rows&&G->GraphGuid==Guid&&Row)||Fail(R,TEXT("input_self_graph_postcondition_failed"),TEXT("Selected node or old node/pin/graph preservation differs; actual current state retained, no rollback claimed."));
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

bool ReferenceGuard(const FString& S)
{
    FString N=S.TrimStartAndEnd();N.TrimQuotesInline();N.ReplaceInline(TEXT("\\"),TEXT("/"));N=N.ToLower();for(const TCHAR* Token:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor")}) { if(N.Contains(Token)) { return false; } }return true;
}
namespace Pin=Function::Pin;
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
    struct FCompiledTree{UClass* Owner=nullptr;TStrongObjectPtr<UWidgetTree> Tree;TStrongObjectPtr<UWidget> Root;FString Path,RootPath;};TArray<FCompiledTree> CompiledTrees;
    FObjectProperty* CompiledTreeField=nullptr;int32 CompiledTreeOffset=0;EClassFlags ParentFlags=CLASS_None;TStrongObjectPtr<UObject> ParentCDO;
    explicit FWidgetLifecycleWork(FResult& In):R(In){}
    bool Bad(const TCHAR* Stage,const TCHAR* Field,const FString& Expected=TEXT("captured role"),const FString& Actual=TEXT("changed/unavailable"))
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Path);J->SetStringField(TEXT("property"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("widget_dispatcher_first_mismatch"),J);R.Stage=Stage;Fail(R,TEXT("widget_dispatcher_current_changed"),TEXT("Current Widget dispatcher roles or authored values are unsafe; actual prefix retained, no further SDK/cleanup/retry."));}
        Stopped=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;return false;
    }
    bool Context(){return !Stopped&&IsInGameThread()&&Loaded(Editor.Get())&&GEditor==Editor.Get()&&!IsPlaySessionActive()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GExitPurge&&!IsEngineExitRequested()&&Loaded(World.Get())&&World->WorldType==EWorldType::Editor&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get();}
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
                if(Row.Root.IsValid()){if(!Loaded(Row.Root.Get())||Row.Root->GetClass()!=UCanvasPanel::StaticClass()||Row.Root->GetOuter()!=Row.Tree.Get())return false;Row.RootPath=Row.Root->GetPathName();}
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
        if(!Loaded(Tree.Get())||!B->WidgetTree.IsResolved()||B->WidgetTree.Get()!=Tree.Get()||Tree->GetOuter()!=B||!Tree->GetPathName().Equals(TreePath,ESearchCase::CaseSensitive)||!Tree->RootWidget.IsResolved()||Tree->RootWidget.Get()!=Root.Get()||!Loaded(Root.Get())||Root->GetClass()!=UCanvasPanel::StaticClass()||Root->GetOuter()!=Tree.Get()||!Root->GetPathName().Equals(RootPath,ESearchCase::CaseSensitive)||Root->GetChildrenCount()!=0||!Root->Slot.IsResolved()||Root->Slot.Get()!=nullptr||Root->bIsVariable!=IsVariable||Root->GetVisibility()!=Visibility||Root->GetIsEnabled()!=Enabled||Root->GetRenderOpacity()!=Opacity||!(Root->GetRenderTransform()==Transform)||Root->GetRenderTransformPivot()!=Pivot||Root->GetClipping()!=Clipping)return Bad(Stage,TEXT("source_canvas_tree_captured_values"));
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
        UClass* OldGen=Gen.Get();UClass* OldSkel=Skel.Get();Gen.Reset(NewGen.Get());Skel.Reset(NewSkel.Get());GenCDO.Reset(NewGCDO.Get());SkelCDO.Reset(NewSCDO.Get());TArray<FCompiledTree> NewTrees;if(!CaptureCompiledTrees(NewTrees))return Bad(Stage,TEXT("reacquire_compiled_widget_tree"));CompiledTrees=MoveTemp(NewTrees);ClassesReady=true;
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
        BP.Reset(FindObject<UWidgetBlueprint>(nullptr,*ObjectPath));Editor.Reset(GEditor);if(!Loaded(BP.Get())||BP->GetClass()!=UWidgetBlueprint::StaticClass()||!Loaded(Editor.Get())||!IsInGameThread()||IsPlaySessionActive())return Fail(R,TEXT("widget_dispatcher_loaded_target_required"),TEXT("Idle Editor and loaded exact Widget Blueprint required; no new loader."));
        World.Reset(GEditor->GetEditorWorldContext().World());Package.Reset(BP->GetPackage());Path=BP->GetPathName();
        if(!Context()||!Loaded(Package.Get())||BP->GetOuter()!=Package.Get()||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr().Get()!=UUserWidget::StaticClass()||BP->BlueprintType!=BPTYPE_Normal||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!BP->WidgetTree.IsResolved()||!BP->Bindings.IsEmpty()||!BP->Animations.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()||!BP->FunctionGraphs.IsEmpty()||!BP->MacroGraphs.IsEmpty()||BP->DelegateSignatureGraphs.Num()>1||BP->NewVariables.Num()>2||BP->UbergraphPages.Num()>1)
            return Fail(R,TEXT("widget_dispatcher_profile_pending"),TEXT("Loaded direct UserWidget, existing flat EventGraph, empty Canvas source tree, no functions/macros/interfaces/animations/bindings, and at most one own dispatcher plus optional factory Canvas descriptor are admitted. Wider Widgets remain incomplete."));
        Parent.Reset(UUserWidget::StaticClass());ParentFlags=Parent->GetClassFlags();if(!Parent->ClassDefaultObject.IsResolved())return Fail(R,TEXT("widget_dispatcher_parent_pending"),TEXT("Already-resolved nullable native parent CDO required."));ParentCDO.Reset(Parent->ClassDefaultObject.Get());Tree.Reset(BP->WidgetTree.Get());if(!Loaded(Tree.Get())||Tree->GetOuter()!=BP.Get()||!Tree->RootWidget.IsResolved())return Fail(R,TEXT("widget_dispatcher_tree_pending"),TEXT("Resolved owned WidgetTree required."));
        Root.Reset(Cast<UCanvasPanel>(Tree->RootWidget.Get()));if(!Loaded(Root.Get())||Root->GetClass()!=UCanvasPanel::StaticClass()||Root->GetOuter()!=Tree.Get()||Root->GetChildrenCount()!=0||!Root->Slot.IsResolved()||Root->Slot.Get()!=nullptr)return Fail(R,TEXT("widget_dispatcher_canvas_pending"),TEXT("First profile has one exact native empty Canvas root; other trees remain incomplete."));
        Gen.Reset(BP->GeneratedClass.GetGCPtr().Get());Skel.Reset(BP->SkeletonGeneratedClass.GetGCPtr().Get());if(!Loaded(Gen.Get())||!Loaded(Skel.Get())||Gen->GetClass()!=UWidgetBlueprintGeneratedClass::StaticClass()||!Skel->GetClass()->IsChildOf(UBlueprintGeneratedClass::StaticClass()))return Fail(R,TEXT("widget_dispatcher_generated_roles_pending"),TEXT("Actual Widget generated class and observed Blueprint skeleton subtype required."));
        GenMeta=Gen->GetClass();SkelMeta=Skel->GetClass();Holds.Emplace(GenMeta);Holds.Emplace(SkelMeta);GenPath=Gen->GetPathName();SkelPath=Skel->GetPathName();
        if(!ClassRole(Gen.Get(),GenMeta,GenPath)||!ClassRole(Skel.Get(),SkelMeta,SkelPath))return Fail(R,TEXT("widget_dispatcher_class_role_pending"),TEXT("Current class/source/outer roles required."));GenCDO.Reset(Gen->ClassDefaultObject.Get());SkelCDO.Reset(Skel->ClassDefaultObject.Get());if(!CDO(Gen.Get(),GenCDO.Get())||!CDO(Skel.Get(),SkelCDO.Get()))return Fail(R,TEXT("widget_dispatcher_cdo_role_pending"),TEXT("Existing nullable CDO roles required; none are constructed."));
        CompiledTreeField=FindFProperty<FObjectProperty>(UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("WidgetTree"));if(!CompiledTreeField)return Fail(R,TEXT("widget_dispatcher_compiled_tree_pending"),TEXT("Public reflected native compiled WidgetTree field required; no private member access."));CompiledTreeOffset=CompiledTreeField->GetOffset_ForInternal();
        if(!CaptureCompiledTrees(CompiledTrees))return Fail(R,TEXT("widget_dispatcher_compiled_tree_pending"),TEXT("Actual nullable compiled WidgetTree/root roles required."));
        TreePath=Tree->GetPathName();RootPath=Root->GetPathName();IsVariable=Root->bIsVariable;Visibility=Root->GetVisibility();Enabled=Root->GetIsEnabled();Opacity=Root->GetRenderOpacity();Transform=Root->GetRenderTransform();Pivot=Root->GetRenderTransformPivot();Clipping=Root->GetClipping();
        for(const auto* Category:{&BP->UbergraphPages,&BP->FunctionGraphs,&BP->MacroGraphs,&BP->DelegateSignatureGraphs})for(const auto& H:*Category)if(!H.IsResolved()||!Loaded(H.Get()))return Fail(R,TEXT("widget_dispatcher_graph_slot_pending"),TEXT("Authored graph slots must already be resolved."));
        Uber=BP->UbergraphPages;Functions=BP->FunctionGraphs;Macros=BP->MacroGraphs;Delegates=BP->DelegateSignatureGraphs;Variables=BP->NewVariables;WidgetGuids=BP->WidgetVariableNameToGuidMap;
        if(!UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.IsResolved())return Fail(R,TEXT("widget_dispatcher_save_policy_pending"),TEXT("Already-resolved Editor save policy required."));Policy.Reset(Cast<UBlueprintEditorSettings>(UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.Get()));
        if(!Policy.IsValid()||Policy->SaveOnCompile!=SoC_Never)return Fail(R,TEXT("widget_dispatcher_save_policy_pending"),TEXT("Existing SaveOnCompile must be Never; settings are not changed."));
        if(!Current(TEXT("widget_dispatcher_bind"))||!CaptureGraphs(Graphs)||!Current(TEXT("widget_dispatcher_capture"))||!ObserveDelegateSources(Graphs,TEXT("create_delegate_initial_sources"))||!Displays(Graphs,TEXT("widget_dispatcher_initial_text")))return Stopped?false:Bad(TEXT("bind"),TEXT("initial_graph_capture"));
        R.Data->SetBoolField(TEXT("widget_dispatcher_contract"),true);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"),true);R.Data->SetStringField(TEXT("implicit_compile_scope"),TEXT("Original member/category/metadata/structural helpers may regenerate skeleton/class/CDO; actual explicit full compile and save remain zero."));R.Data->SetStringField(TEXT("blueprint"),Path);R.Data->SetBoolField(TEXT("explicit_compile_requested"),false);R.Data->SetBoolField(TEXT("explicit_save_requested"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
        R.Data->SetStringField(TEXT("widget_preservation_scope"),TEXT("Captured source Canvas role/child0/variable/visibility/enabled/render transform-opacity-pivot/clipping, factory Widget GUID map, pre-existing authored variables/graph node/pin type/default/link values. Generated/skeleton/nullable CDO roles reacquired only after actual structural helpers. Compiled WidgetTree/preview/animation/runtime and unobserved Widget fields are not full-state proof."));return true;
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
bool WidgetLifecyclePath(const FString& Path)
{
    if(!Path.StartsWith(TEXT("/Game/"))||!Function::Pin::SafeName(Path)||!ReferenceGuard(Path))return false;
    const FString ObjectPath=Path.Contains(TEXT("."))?Path:Path+TEXT(".")+FPackageName::GetShortName(Path);return FindObject<UWidgetBlueprint>(nullptr,*ObjectPath)!=nullptr;
}

bool WidgetSelfActive=false;
FResult WidgetSelf(const FString& Path,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Kind,bool Create)
{
    FResult R;Init(R);if(WidgetSelfActive){Fail(R,TEXT("widget_self_busy"),TEXT("Request-long Widget Self authoring is active."));return R;}TGuardValue<bool> Active(WidgetSelfActive,true);
    R.Data->SetBoolField(TEXT("widget_explicit_self_reference_contract"),true);int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R))return R;
    FWidgetLifecycleWork W(R);if(!W.Begin(Path,NAME_None))return R;UEdGraph* Graph=nullptr;if(!W.SelectGraph(Name,Id,Kind,Create,Graph))return R;W.NewGraph=Graph;
    UClass* Class=NodeClass(false,R);TStrongObjectPtr<UClass> KeepClass(Class);if(!W.Current(TEXT("widget_self_class_return")))return R;if(!Class)return R;const EClassFlags ClassFlags=Class->GetClassFlags();
    auto Current=[&](const TCHAR* Stage){if(!W.Current(Stage))return false;if(!Loaded(Class)||Class->GetSuperClass()!=UK2Node::StaticClass()||Class->GetClassFlags()!=ClassFlags||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy.Get()!=nullptr||Class->GetPathName()!=TEXT("/Script/BlueprintGraph.K2Node_Self"))return W.Bad(Stage,TEXT("native_self_class_role"));return true;};
    const auto Before=WidgetAuthoredSnapshot(W,TEXT("widget_self_before"));if(W.Stopped)return R;R.Data->SetObjectField(TEXT("before_blueprint_state"),Before);
    auto Run=[&]()
    {
        Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","WidgetSelfReference","Add Widget Self Reference"));if(!Current(TEXT("widget_self_transaction")))return;
        R.bMutationAttempted=true;R.bChangeStateKnown=false;W.Writer=true;R.Stage=TEXT("allocate_widget_self");Count(R,TEXT("node_allocation_calls"));UK2Node* Node=NewObject<UK2Node>(Graph,Class);TStrongObjectPtr<UK2Node> Returned(Node);Count(R,TEXT("node_allocation_returns"));W.NewNode=Node;W.NewNodeClass=Class;if(Node)W.Holds.Emplace(Node);if(!Current(TEXT("widget_self_allocate_return")))return;if(!Node){Fail(R,TEXT("input_self_node_allocation_failed"),TEXT("Allocation returned null; actual prefix retained."));return;}
        Node->NodePosX=XY[0];Node->NodePosY=XY[1];W.RequestedX=XY[0];W.RequestedY=XY[1];W.PlacementReady=true;Count(R,TEXT("graph_add_node_calls"));Graph->AddNode(Node,true);W.NodeMember=true;if(!Current(TEXT("widget_self_add_return")))return;
        Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();W.NewId=Node->NodeGuid;W.GuidReady=true;if(!W.NewId.IsValid()){W.Bad(TEXT("widget_self_guid_return"),TEXT("created_guid"));return;}if(!Current(TEXT("widget_self_guid_return")))return;Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();if(!Current(TEXT("widget_self_post_return")))return;Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();if(!Current(TEXT("widget_self_pins_return"))||!W.FreezeNew(TEXT("widget_self_new_inventory")))return;
        Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(W.BP.Get());Count(R,TEXT("mark_blueprint_modified_returns"));if(!Current(TEXT("widget_self_modified_return")))return;
        const bool Pure=Node->IsNodePure();if(!Current(TEXT("widget_self_pure_return")))return;const bool Compatible=Node->IsCompatibleWithGraph(Graph);if(!Current(TEXT("widget_self_compatibility_return")))return;
        const FWidgetLifecycleWork::FNode* Role=nullptr;for(const auto& G:W.Graphs)for(const auto& N:G.Nodes)if(N.Hold.Get()==Node)Role=&N;const auto* Output=Role&&Role->Pins.Num()==1?&Role->Pins[0]:nullptr;
        const bool Shape=Pure&&Compatible&&Output&&Output->Name==UEdGraphSchema_K2::PN_Self&&Output->Direction==EGPD_Output&&Output->Type.PinCategory==UEdGraphSchema_K2::PC_Object&&Output->Type.PinSubCategory==UEdGraphSchema_K2::PSC_Self&&Output->TypeObject==nullptr&&!Output->Type.IsContainer()&&!Output->Type.bIsReference&&!Output->Type.bIsConst&&!Output->Type.bIsWeakPointer&&Output->Raw.IsEmpty()&&Output->Default==nullptr&&Output->Display.IsEmpty()&&Output->Links.IsEmpty();R.Data->SetBoolField(TEXT("specialized_pin_shape_verified"),Shape);if(!Shape){W.Bad(TEXT("widget_self_final_shape"),TEXT("stock_self_output"));return;}
        auto Row=W.NodeJson(Node,TEXT("widget_self_publication"));if(W.Stopped)return;auto GraphRow=DescribeGraph(W.BP.Get(),Graph);if(!Current(TEXT("widget_self_graph_publication")))return;GraphRow->SetStringField(TEXT("graph_id"),Graph->GraphGuid.ToString());Row->SetObjectField(TEXT("graph"),GraphRow);R.Data->SetObjectField(TEXT("node"),Row);R.Data->SetObjectField(TEXT("graph"),GraphRow);R.Data->SetObjectField(TEXT("legacy_result"),Row);R.Data->SetStringField(TEXT("self_type_scope"),TEXT("Stock PC_Object/PSC_Self with null subtype represents the own Widget instance; no runtime object or forced generated class literal."));
        const auto After=WidgetAuthoredSnapshot(W,TEXT("widget_self_after"));if(W.Stopped)return;R.Data->SetObjectField(TEXT("after_blueprint_state"),After);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),true);R.Data->SetNumberField(TEXT("observed_new_node_count"),1);R.bChanged=R.bChangeStateKnown=R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");
    };
    Run();if(!W.Stopped&&R.bMutationAttempted&&!R.bChangeStateKnown&&Current(TEXT("widget_self_failed_prefix"))){R.bChanged=R.bChangeStateKnown=true;}if(!W.Stopped&&R.bChangeStateKnown)Current(TEXT("widget_self_transaction_publication"));return R;
}

FResult Create(bool Input,const FString& Path,const FString& ActionName,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool CreateGraph)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("node_family"),Input?TEXT("UK2Node_InputAction"):TEXT("UK2Node_Self"));R.Data->SetStringField(TEXT("requested_action_name"),Input?ActionName:TEXT(""));
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("input_self_editor_invalid"),TEXT("Idle Editor game thread required; live PIE/SIE authoring remains outside this source scope."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddInputSelfNode","Add Blueprint Input Action or Self Reference"));
    if(Input)
    {
        bool Bounded=ActionName.Len()<NAME_SIZE;for(int32 I=0;I<ActionName.Len();++I) { Bounded&=ActionName[I]!=0; }
        if(!Bounded) { Fail(R,TEXT("input_action_name_unrepresentable"),TEXT("Original literal FName must fit NAME_SIZE and contain no interior NUL. Empty/unknown/control/unmapped names otherwise remain admitted."));return R; }
        R.Data->SetBoolField(TEXT("action_mapping_availability_checked"),false);R.Data->SetStringField(TEXT("action_warning_scope"),TEXT("Original unknown/empty action remains creatable; installed ValidateNodeDuringCompilation emits a Warning if unmapped. No mapping registration, EnhancedInput replacement or compiler run occurs here."));
    }
    int32 XY[2]={0,0};FString PositionText;if(!Position(PositionJson,XY,PositionText,R)) { return R; }
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,CreateGraph,BP,G,Before,R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;const auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());
    if(Input&&(Schema->GetGraphType(G)!=GT_Ubergraph||!BP->SupportsInputEvents()||UEdGraphSchema_K2::IsConstructionScript(G)))
    { Fail(R,TEXT("input_action_graph_scope_pending"),TEXT("Public InputAction graph compatibility requires an input-supporting Ubergraph outside construction script. Original incompatible graph allocations remain a tracked authoring/compiler difference; any requested graph-creation prefix is retained."));return R; }
    auto* Class=NodeClass(Input,R);if(!Class) { return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R),ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);
    UScriptStruct* KeyStruct=Input?FKey::StaticStruct():nullptr;const TStrongObjectPtr<UScriptStruct> KeepKey(KeyStruct);TSharedPtr<FJsonObject> KeyBefore;
    if(Input) { KeyBefore=Source(KeyStruct,R);R.Data->SetObjectField(TEXT("key_struct_source_before"),KeyBefore); }
    // Factory dispatches the native subclass constructor, while calls below use exported/base virtual interfaces.
    R.Stage=TEXT("create_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node>(G,Class);Count(R,TEXT("node_allocation_returns"));const TStrongObjectPtr<UK2Node> KeepNode(Node);
    if(!Node) { Fail(R,TEXT("input_self_allocation_failed"),Input?TEXT("Failed to create input action node"):TEXT("Failed to create self node"));return R; }
    TSharedPtr<FJsonObject> Defaults;FName RequestedName;
    if(Input)
    {
        auto* Action=static_cast<UK2Node_InputAction*>(Node);Defaults=InputFields(Action,R);R.Data->SetObjectField(TEXT("allocated_input_fields"),Defaults);RequestedName=FName(*ActionName);
        Count(R,TEXT("action_name_set_calls"));Action->InputActionName=RequestedName;Count(R,TEXT("action_name_set_returns"));R.Data->SetStringField(TEXT("stored_action_name_after_set"),Action->InputActionName.ToString());
    }
    UEdGraphNode* Base=Node;Place(BP,G,Base,XY,false,R);Mark(BP,R);const bool Observed=After(BP,G,Base,Guid,Before,R);
    const auto ClassAfter=Source(Class,R),ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);
    bool SourcePreserved=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter));
    if(Input) { const auto KeyAfter=Source(KeyStruct,R);R.Data->SetObjectField(TEXT("key_struct_source_after"),KeyAfter);SourcePreserved&=Equal(MakeShared<FJsonValueObject>(KeyBefore),MakeShared<FJsonValueObject>(KeyAfter)); }
    R.Data->SetBoolField(TEXT("native_source_preserved"),SourcePreserved);R.bChanged=R.bChanged||!SourcePreserved;
    if(!Observed) { return R; }
    Count(R,TEXT("graph_compatibility_observation_calls"));const bool Compatible=Base->IsCompatibleWithGraph(G);R.Data->SetBoolField(TEXT("current_graph_compatible"),Compatible);
    bool Pins=FreshPins(Base);
    if(Input)
    {
        const auto* Action=static_cast<const UK2Node_InputAction*>(Node);const auto Fields=InputFields(Action,R);R.Data->SetObjectField(TEXT("current_input_fields"),Fields);const auto* Pressed=Base->FindPin(TEXT("Pressed"),EGPD_Output);const auto* Released=Base->FindPin(TEXT("Released"),EGPD_Output);const auto* Key=Base->FindPin(TEXT("Key"),EGPD_Output);
        const bool Flags=Fields->GetBoolField(TEXT("consume_input"))==Defaults->GetBoolField(TEXT("consume_input"))&&Fields->GetBoolField(TEXT("execute_when_paused"))==Defaults->GetBoolField(TEXT("execute_when_paused"))&&Fields->GetBoolField(TEXT("override_parent_binding"))==Defaults->GetBoolField(TEXT("override_parent_binding"));
        Pins&=Base->Pins.Num()==3&&OutputPin(Pressed,UEdGraphSchema_K2::PC_Exec,NAME_None,nullptr)&&OutputPin(Released,UEdGraphSchema_K2::PC_Exec,NAME_None,nullptr)&&OutputPin(Key,UEdGraphSchema_K2::PC_Struct,NAME_None,KeyStruct)&&!Node->IsNodePure()&&Flags&&Action->InputActionName==RequestedName;
        R.Data->SetStringField(TEXT("action_name"),Action->InputActionName.ToString());R.Data->SetBoolField(TEXT("action_FName_match"),Action->InputActionName==RequestedName);R.Data->SetBoolField(TEXT("allocated_input_defaults_preserved"),Flags);R.Data->SetStringField(TEXT("stock_constructor_default_prediction"),TEXT("ConsumeInput=True; OverrideParentBinding=True; zero-initialized ExecuteWhenPaused=False. Actual allocated defaults are observed and preserved, not overwritten or assumed from an unobserved archetype."));
    }
    else
    {
        const auto* Self=Base->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Output);Pins&=Base->Pins.Num()==1&&OutputPin(Self,UEdGraphSchema_K2::PC_Object,UEdGraphSchema_K2::PSC_Self,nullptr)&&Node->IsNodePure();
        Count(R,TEXT("static_function_observation_calls"));const bool Static=Schema->IsStaticFunctionGraph(G);R.Data->SetBoolField(TEXT("static_function_graph"),Static);R.Data->SetStringField(TEXT("self_type_scope"),TEXT("PC_Object/PSC_Self with null subcategory object represents this Blueprint instance at compile/runtime, not a generic UObject constant or captured runtime owner. Static function Self is retained as original authoring with compiler Warning; no compiler invoked."));
    }
    R.Data->SetBoolField(TEXT("specialized_pin_shape_verified"),Pins);
    if(!Loaded(Node)||Node->GetClass()!=Class||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!BP->GetOutermost()->IsDirty()||!Pins||!Compatible||!SourcePreserved)
    { Fail(R,TEXT("input_self_node_postcondition_failed"),TEXT("Current specialized class/GUID/position/pins/defaults/graph/source proof failed; original created node and actual prefix retained with no added rollback."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
Function::FResult AddInputAction(const FString& Path,const FString& ActionName,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool CreateGraph)
{ return Create(true,Path,ActionName,PositionJson,Name,Id,Type,CreateGraph); }
Function::FResult AddSelfReference(const FString& Path,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool CreateGraph)
{ if(WidgetLifecyclePath(Path))return WidgetSelf(Path,PositionJson,Name,Id,Type,CreateGraph);return Create(false,Path,TEXT(""),PositionJson,Name,Id,Type,CreateGraph); }
}
