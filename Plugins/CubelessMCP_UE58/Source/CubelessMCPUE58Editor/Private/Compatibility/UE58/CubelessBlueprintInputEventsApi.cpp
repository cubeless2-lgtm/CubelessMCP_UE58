// DH original Axis10/Enhanced7 event authoring through existing public Editor APIs.
#include "Compatibility/UE58/CubelessBlueprintInputEventsApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_InputAxisEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintInputEvents
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }

bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }

bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }

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

TSharedRef<FJsonObject> Source(const UStruct* Struct,FResult& R)
{
    Count(R,TEXT("source_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Struct->GetPathName());J->SetStringField(TEXT("class"),Struct->GetClass()->GetPathName());J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(Struct->GetFlags())));J->SetStringField(TEXT("super"),GetPathNameSafe(Struct->GetSuperStruct()));J->SetStringField(TEXT("package"),Struct->GetOutermost()->GetName());J->SetStringField(TEXT("package_flags"),LexToString(static_cast<uint32>(Struct->GetOutermost()->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Struct->GetOutermost()->IsDirty());
    if(const UClass* Class=Cast<UClass>(Struct)) { J->SetStringField(TEXT("class_flags"),LexToString(static_cast<uint32>(Class->GetClassFlags())));J->SetStringField(TEXT("generated_by"),GetPathNameSafe(Class->ClassGeneratedBy)); }
    if(const UScriptStruct* Script=Cast<UScriptStruct>(Struct)) { J->SetStringField(TEXT("struct_flags"),LexToString(static_cast<uint32>(Script->StructFlags))); }
    return J;
}

bool OwnerScope(UBlueprint* BP,UEdGraph* G,FResult& R)
{
    if(!Loaded(BP)||!Loaded(G)||!Loaded(BP->ParentClass)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP
        ||(BP->BlueprintType!=BPTYPE_Normal&&BP->BlueprintType!=BPTYPE_FunctionLibrary&&BP->BlueprintType!=BPTYPE_MacroLibrary)
        ||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad
        ||BP->Status==BS_BeingCreated||!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>=2048)
    { return Fail(R,TEXT("input_events_owner_scope_pending"),TEXT("Idle owned editable K2 graph and ordinary native runtime Blueprint owner required; broader owners remain tracked.")); }
    int32 Depth=0;
    for(UClass* C=BP->ParentClass;C;C=C->GetSuperClass())
    {
        const FString Package=C->GetOutermost()->GetName();
        if(++Depth>64||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->IsEditorOnly()
            ||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)
            ||(!Package.Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)&&!Package.Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)))
        { return Fail(R,TEXT("input_events_parent_scope_pending"),TEXT("Finite native Engine/CoreUObject parent hierarchy required; not restricted to direct Actor. Generated/custom/editor/plugin parents remain valid residuals.")); }
    }
    Count(R,TEXT("external_scope_observation_calls"));TArray<TSharedPtr<FJsonValue>> External;
    for(TObjectIterator<UBlueprint> It;It;++It)
    { if(*It!=BP&&It->ParentClass&&BP->GeneratedClass&&It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.Add(MakeShared<FJsonValueString>(It->GetPathName())); } }
    if(BP->GeneratedClass)
    { TArray<UObject*> Instances;GetObjectsOfClass(BP->GeneratedClass,Instances,true,RF_NoFlags);for(UObject* O:Instances) { if(IsValid(O)&&O->GetOutermost()!=BP->GetOutermost()) { External.Add(MakeShared<FJsonValueString>(O->GetPathName())); } } }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"),External);
    return External.IsEmpty()||Fail(R,TEXT("input_events_external_instances_pending"),TEXT("Loaded external instances/derived Blueprint callback effects are outside this observed scope."));
}

bool Graph(const FString& Path,const FString& Name,const FString& Id,const FString& Type,bool Create,UBlueprint*& BP,UEdGraph*& G,TSharedPtr<FJsonObject>& Before,FResult& R)
{
    R.Stage=TEXT("resolve_graph");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(Path,Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));
    R.bMutationAttempted=S.bMutationAttempted;R.bChanged=S.bChanged;R.bChangeStateKnown=S.bChangeStateKnown;
    if(!S.bSuccess||!S.bPostconditionPassed) { return Fail(R,S.ErrorCode.IsEmpty()?TEXT("input_events_graph_unverified"):*S.ErrorCode,S.Error); }
    BP=FindObject<UBlueprint>(nullptr,*S.Data->GetStringField(TEXT("blueprint")));const auto J=S.Data->GetObjectField(TEXT("graph"));G=FindObject<UEdGraph>(nullptr,*J->GetStringField(TEXT("object_path")));
    Before=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetStringField(TEXT("blueprint"),S.Data->GetStringField(TEXT("blueprint")));
    return OwnerScope(BP,G,R);
}

bool After(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const FGuid& Guid,const TSharedPtr<FJsonObject>& Before,FResult& R)
{
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(BP->GetPathName(),TEXT(""),Guid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(S));
    if(!S.bSuccess||!S.bChangeStateKnown||!S.bPostconditionPassed)
    { R.bChangeStateKnown=false;return Fail(R,TEXT("input_events_after_incomplete"),TEXT("Current graph observation did not close; created node/notification prefix retained with no retry or new cleanup.")); }
    const auto A=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),A);R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true;
    const auto* Row=A->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(Row) { R.Data->SetObjectField(TEXT("node"),(*Row)->AsObject());for(const auto& P:(*Row)->AsObject()->Values) { R.Data->SetField(P.Key,P.Value); }LegacyNodeFields(Node,R); }
    const bool Old=ExistingNodesPreserved(Before,A,1),Rows=GraphRowsPreserved(Before,A,G->GetPathName(),1);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);R.Data->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());Legacy->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    return (Old&&Rows&&G->GraphGuid==Guid&&Row)||Fail(R,TEXT("input_events_graph_postcondition_failed"),TEXT("Selected node or old node/pin/graph preservation differs; actual current state retained, no rollback claimed."));
}

bool Forbidden(const FString& Text)
{
    const FString S=Text.TrimStartAndEnd().Replace(TEXT("\\"),TEXT("/")).ToLower();
    for(const TCHAR* Token:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor")}) { if(S.Contains(Token)) { return true; } }return false;
}
void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("external_scope_observation_calls"),TEXT("source_observation_calls"),TEXT("node_class_lookup_calls"),TEXT("node_class_lookup_returns"),TEXT("module_readiness_observation_calls"),TEXT("action_class_lookup_calls"),TEXT("action_class_lookup_returns"),TEXT("action_load_calls"),TEXT("action_load_returns"),TEXT("action_observation_calls"),TEXT("action_property_export_calls"),TEXT("action_layout_observation_calls"),TEXT("enum_observation_calls"),TEXT("settings_observation_calls"),TEXT("settings_class_lookup_calls"),TEXT("settings_cdo_lookup_calls"),TEXT("settings_property_read_calls"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("axis_initialize_calls"),TEXT("axis_initialize_returns"),TEXT("axis_flag_assignment_calls"),TEXT("input_action_assignment_calls"),TEXT("input_action_assignment_returns"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("legacy_node_metadata_observation_calls"),TEXT("axis_field_observation_calls"),TEXT("input_action_storage_observation_calls"),TEXT("pin_observation_calls"),TEXT("graph_compatibility_observation_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("destroy_node_calls"),TEXT("module_load_calls"),TEXT("mapping_membership_lookup_calls"),TEXT("pin_default_match_calls"),TEXT("pin_default_validity_calls"),TEXT("node_position_pair_assignment_calls"),TEXT("input_runtime_calls"),TEXT("explicit_cdo_construction_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Original direct event constructors: outer transaction; Axis Initialize/explicit3 flags or Enhanced public InputAction member; Add/GUID/PostPlaced/Allocate/Reconstruct/Mark. No explicit Modify/Compile/Save/Destroy or UI spawner duplicate suppression. SDK callbacks/load/CDO/cache/skeleton/child effects are not direct counter totals."));
    R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("runtime_or_full_original_verified"),false);R.Data->SetBoolField(TEXT("mapping_membership_checked"),false);
}
UClass* ExactClass(const TCHAR* Path,UClass* Super,FResult& R,bool Action=false)
{
    Count(R,Action?TEXT("action_class_lookup_calls"):TEXT("node_class_lookup_calls"));UClass* C=FindObject<UClass>(nullptr,Path);Count(R,Action?TEXT("action_class_lookup_returns"):TEXT("node_class_lookup_returns"));
    if(!Loaded(C)||!C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||(Super&&C->GetSuperClass()!=Super))
    { Fail(R,TEXT("input_events_class_readiness_pending"),FString(TEXT("Exact loaded native class required, no module/class fallback: "))+Path);return nullptr; }
    const FString Package=C->GetOutermost()->GetName();if(!Package.StartsWith(TEXT("/Script/"))) { Fail(R,TEXT("input_events_class_package_unverified"),TEXT("Native script class package required."));return nullptr; }
    Count(R,TEXT("module_readiness_observation_calls"));if(!FModuleManager::Get().IsModuleLoaded(FName(*Package.Mid(8)))) { Fail(R,TEXT("input_events_module_readiness_pending"),TEXT("Class owning module must already be loaded; no plugin/module enable or load."));return nullptr; }return C;
}
bool EventGraph(UBlueprint* BP,UEdGraph* G,FResult& R)
{
    const auto* S=CastChecked<UEdGraphSchema_K2>(G->GetSchema());
    return (S->GetGraphType(G)==GT_Ubergraph&&BP->SupportsInputEvents()&&!UEdGraphSchema_K2::IsConstructionScript(G))||Fail(R,TEXT("input_events_graph_scope_pending"),TEXT("Original event node requires an input-supporting Ubergraph outside construction for this valid slice. Original incompatible created-then-compiler branch remains tracked; requested graph creation prefix retained."));
}
void Place(UEdGraph* G,UEdGraphNode* Node,const int32 (&XY)[2],FResult& R,bool PositionAssigned=false)
{
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());if(!PositionAssigned) { Count(R,TEXT("node_position_pair_assignment_calls"));Node->NodePosX=XY[0];Node->NodePosY=XY[1]; }Count(R,TEXT("graph_add_node_calls"));G->AddNode(Node,true);Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();R.Data->SetStringField(TEXT("created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));
    Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();Count(R,TEXT("reconstruct_calls"));Node->ReconstructNode();
}
bool PinType(const UEdGraphPin* P,FName Category,FName Sub=NAME_None,const UObject* Object=nullptr)
{
    return P&&P->Direction==EGPD_Output&&P->PinType.PinCategory==Category&&P->PinType.PinSubCategory==Sub&&P->PinType.PinSubCategoryObject.Get()==Object&&!P->PinType.IsContainer()&&!P->PinType.bIsReference&&!P->PinType.bIsConst&&!P->PinType.bIsWeakPointer;
}
void Pins(UEdGraphNode* Node,FResult& R)
{
    Count(R,TEXT("pin_observation_calls"));TArray<TSharedPtr<FJsonValue>> Rows;
    for(const UEdGraphPin* P:Node->Pins)
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),P->PinName.ToString());J->SetStringField(TEXT("id"),P->PinId.ToString(EGuidFormats::Digits));J->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(P->PinType));J->SetBoolField(TEXT("advanced_view"),P->bAdvancedView);J->SetBoolField(TEXT("hidden"),P->bHidden);J->SetBoolField(TEXT("not_connectable"),P->bNotConnectable);J->SetBoolField(TEXT("default_ignored"),P->bDefaultValueIsIgnored);J->SetStringField(TEXT("default_value"),P->DefaultValue);J->SetStringField(TEXT("autogenerated_default"),P->AutogeneratedDefaultValue);J->SetStringField(TEXT("default_object"),GetPathNameSafe(P->DefaultObject));J->SetStringField(TEXT("friendly_name"),P->PinFriendlyName.ToString());Rows.Add(MakeShared<FJsonValueObject>(J));
    }
    R.Data->SetArrayField(TEXT("current_event_pins"),Rows);R.Data->SetNumberField(TEXT("current_advanced_pin_display"),static_cast<int32>(Node->AdvancedPinDisplay));
}
TSharedRef<FJsonObject> ActionFields(UObject* A,FResult& R,bool& Complete)
{
    Count(R,TEXT("action_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),A->GetPathName());J->SetStringField(TEXT("class"),A->GetClass()->GetPathName());J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(A->GetFlags())));J->SetBoolField(TEXT("package_dirty"),A->GetOutermost()->IsDirty());TArray<TSharedPtr<FJsonValue>> Rows;int32 Num=0;Complete=true;
    for(TFieldIterator<FProperty> It(A->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)
    {
        const FProperty* P=*It;if(P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_SkipSerialization)) { continue; }
        if(++Num>128) { Complete=false;break; }FString V;Count(R,TEXT("action_property_export_calls"));const bool Exported=P->ExportText_InContainer(0,V,A,A,A,PPF_None);if(!Exported||V.Len()>65536) { Complete=false;break; }
        auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),P->GetName());Q->SetStringField(TEXT("type"),P->GetCPPType());Q->SetStringField(TEXT("value"),V);Rows.Add(MakeShared<FJsonValueObject>(Q));
    }
    J->SetArrayField(TEXT("fields"),Rows);J->SetBoolField(TEXT("complete"),Complete);return J;
}
TSharedRef<FJsonObject> Settings(FResult& R)
{
    Count(R,TEXT("settings_observation_calls"));auto J=MakeShared<FJsonObject>();
    for(const auto& Pair:{TPair<const TCHAR*,const TCHAR*>(TEXT("/Script/BlueprintGraph.BlueprintEditorSettings"),TEXT("bEnableInputTriggerSupportWarnings")),TPair<const TCHAR*,const TCHAR*>(TEXT("/Script/InputEditor.EnhancedInputEditorSettings"),TEXT("VisibleEventPinsByDefault"))})
    {
        Count(R,TEXT("settings_class_lookup_calls"));UClass* C=FindObject<UClass>(nullptr,Pair.Key);Count(R,TEXT("settings_cdo_lookup_calls"));UObject* O=Loaded(C)?C->GetDefaultObject(false):nullptr;auto Q=MakeShared<FJsonObject>();Q->SetBoolField(TEXT("already_constructed"),Loaded(O));Q->SetStringField(TEXT("class"),GetPathNameSafe(C));Q->SetStringField(TEXT("cdo"),GetPathNameSafe(O));
        if(Loaded(O)) { if(const FProperty* P=FindFProperty<FProperty>(C,Pair.Value)) { FString V;Count(R,TEXT("settings_property_read_calls"));P->ExportText_InContainer(0,V,O,O,O,PPF_None);Q->SetStringField(TEXT("value"),V); } }
        J->SetObjectField(Pair.Value,Q);
    }
    return J;
}
}

Function::FResult AddAxis(const FString& Path,const FString& AxisName,const FString& PositionJson,bool Consume,bool Paused,bool Override,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("node_family"),TEXT("UK2Node_InputAxisEvent"));R.Data->SetStringField(TEXT("requested_axis_name"),AxisName);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("input_events_editor_invalid"),TEXT("Idle Editor game thread outside PIE/SIE required; no runtime input control."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddAxisEvent","Add Blueprint Input Axis Event"));
    bool Bounded=AxisName.Len()<NAME_SIZE;for(int32 I=0;I<AxisName.Len();++I) { Bounded&=AxisName[I]!=0; }
    if(!Bounded) { Fail(R,TEXT("input_axis_name_unrepresentable"),TEXT("Literal FName must fit NAME_SIZE with no interior NUL. Empty/unmapped names remain admitted; no input settings membership gate."));return R; }
    R.Data->SetStringField(TEXT("axis_warning_scope"),TEXT("Source ValidateNodeDuringCompilation warns if mapping absent; constructor is retained for unknown/empty names. Connected warning/runtime/dispatch not verified; API does not compile or change mappings."));
    int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,Create,BP,G,Before,R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;if(!EventGraph(BP,G,R)) { return R; }
    UClass* C=ExactClass(TEXT("/Script/BlueprintGraph.K2Node_InputAxisEvent"),UK2Node_Event::StaticClass(),R);if(!C) { return R; }const TStrongObjectPtr<UClass> KeepClass(C);const auto ClassBefore=Source(C,R),ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);
    R.Stage=TEXT("create_input_axis_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_InputAxisEvent>(G,C);Count(R,TEXT("node_allocation_returns"));const TStrongObjectPtr<UK2Node_InputAxisEvent> KeepNode(Node);
    if(!Node) { Fail(R,TEXT("input_axis_allocation_failed"),TEXT("Input Axis allocation returned null; actual prefix retained without retry."));return R; }
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());const FName Axis(*AxisName);const FString ExpectedFunction=FString::Printf(TEXT("InpAxisEvt_%s_%s"),*Axis.ToString(),*Node->GetName());
    if(ExpectedFunction.Len()>=NAME_SIZE) { Fail(R,TEXT("input_axis_generated_name_unrepresentable"),TEXT("Original Initialize generated FName exceeds safe NAME_SIZE; newly allocated object prefix retained, no undefined name creation or cleanup claim."));return R; }
    Count(R,TEXT("axis_initialize_calls"));Node->Initialize(Axis);Count(R,TEXT("axis_initialize_returns"));
    Count(R,TEXT("node_position_pair_assignment_calls"));Node->NodePosX=XY[0];Node->NodePosY=XY[1];Count(R,TEXT("axis_flag_assignment_calls"));Node->bConsumeInput=Consume;Count(R,TEXT("axis_flag_assignment_calls"));Node->bExecuteWhenPaused=Paused;Count(R,TEXT("axis_flag_assignment_calls"));Node->bOverrideParentBinding=Override;
    R.Data->SetBoolField(TEXT("supplied_consume_input"),Consume);R.Data->SetBoolField(TEXT("supplied_execute_when_paused"),Paused);R.Data->SetBoolField(TEXT("supplied_override_parent_binding"),Override);R.Data->SetBoolField(TEXT("public_override_false_differs_native_ctor_true"),!Override);
    Place(G,Node,XY,R,true);Mark(BP,R);const bool Observed=After(BP,G,Node,Guid,Before,R);
    const auto ClassAfter=Source(C,R),ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);const bool Same=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter));R.Data->SetBoolField(TEXT("native_sources_preserved"),Same);R.bChanged=R.bChanged||!Same;if(!Observed) { return R; }
    Count(R,TEXT("axis_field_observation_calls"));R.Data->SetStringField(TEXT("axis_name"),Node->InputAxisName.ToString());R.Data->SetStringField(TEXT("custom_function_name"),Node->CustomFunctionName.ToString());R.Data->SetStringField(TEXT("event_signature_name"),Node->EventReference.GetMemberName().ToString());R.Data->SetBoolField(TEXT("consume_input"),Node->bConsumeInput);R.Data->SetBoolField(TEXT("execute_when_paused"),Node->bExecuteWhenPaused);R.Data->SetBoolField(TEXT("override_parent_binding"),Node->bOverrideParentBinding);R.Data->SetBoolField(TEXT("internal_event"),Node->bInternalEvent);Pins(Node,R);
    const auto* AxisPin=Node->FindPin(TEXT("AxisValue"));const auto* Exec=Node->FindPin(UEdGraphSchema_K2::PN_Then);int32 Delegates=0;for(const auto* Pin:Node->Pins) { Delegates+=PinType(Pin,UEdGraphSchema_K2::PC_Delegate)?1:0; }
    const bool Shape=FreshPins(Node)&&Node->Pins.Num()==3&&PinType(AxisPin,UEdGraphSchema_K2::PC_Real,UEdGraphSchema_K2::PC_Float)&&PinType(Exec,UEdGraphSchema_K2::PC_Exec)&&Delegates==1&&!Node->IsNodePure();R.Data->SetBoolField(TEXT("axis_pin_shape_verified"),Shape);
    Count(R,TEXT("graph_compatibility_observation_calls"));const bool Compatible=Node->IsCompatibleWithGraph(G);R.Data->SetBoolField(TEXT("current_graph_compatible"),Compatible);
    if(!Loaded(Node)||Node->GetClass()!=C||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!BP->GetOutermost()->IsDirty()||Node->InputAxisName!=Axis||Node->CustomFunctionName!=FName(*ExpectedFunction)||Node->EventReference.GetMemberName()!=FName(TEXT("InputAxisHandlerDynamicSignature__DelegateSignature"))||Node->bConsumeInput!=Consume||Node->bExecuteWhenPaused!=Paused||Node->bOverrideParentBinding!=Override||!Node->bInternalEvent||!Shape||!Same||!Compatible||!OwnerScope(BP,G,R))
    { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("input_axis_postcondition_failed"),TEXT("Current axis identity/Initialize flags/pins/owner/source proof failed; original created node retained without cleanup/rollback/replay.")); }return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
Function::FResult AddEnhanced(const FString& Path,const FString& Input,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("node_family"),TEXT("UK2Node_EnhancedInputAction"));R.Data->SetStringField(TEXT("requested_input_action"),Input);R.Data->SetBoolField(TEXT("UI_spawner_duplicate_suppression_used"),false);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("input_events_editor_invalid"),TEXT("Idle Editor game thread outside PIE/SIE required; no runtime input control."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddEnhancedInputEvent","Add Blueprint Enhanced Input Action"));
    if(!Function::Pin::SafeName(Input,true)||Forbidden(Input)) { Fail(R,TEXT("enhanced_input_path_invalid_or_dependency"),TEXT("Bounded original action path and dependency guard required; semantic asset loading remains after graph resolution."));return R; }
    int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,Create,BP,G,Before,R)) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;if(!EventGraph(BP,G,R)) { return R; }
    UClass* ActionClass=ExactClass(TEXT("/Script/EnhancedInput.InputAction"),nullptr,R,true);UClass* C=ExactClass(TEXT("/Script/InputBlueprintNodes.K2Node_EnhancedInputAction"),UK2Node::StaticClass(),R);if(!ActionClass||!C) { return R; }const TStrongObjectPtr<UClass> KeepActionClass(ActionClass),KeepClass(C);
    FString Normal=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();Normal.TrimQuotesInline();if((Normal.StartsWith(TEXT("/Game/"))||Normal.StartsWith(TEXT("/Engine/")))&&!Normal.Contains(TEXT("."))) { const FString AssetName=FPackageName::GetShortName(Normal);Normal=FString::Printf(TEXT("%s.%s"),*Normal,*AssetName); }
    R.Data->SetStringField(TEXT("normalized_input_action"),Normal);const bool WasLoaded=FindObject<UObject>(nullptr,*Normal)!=nullptr;R.Data->SetBoolField(TEXT("action_was_loaded_before_call"),WasLoaded);R.Stage=TEXT("load_input_action");UObject* A=nullptr;if(!Normal.IsEmpty()) { Count(R,TEXT("action_load_calls"));A=LoadObject<UObject>(nullptr,*Normal);Count(R,TEXT("action_load_returns")); }const TStrongObjectPtr<UObject> KeepAction(A);R.Data->SetStringField(TEXT("returned_input_action"),GetPathNameSafe(A));
    if(!Loaded(A)||A->GetClass()!=ActionClass||!A->IsAsset()||!A->GetPathName().Equals(Normal,ESearchCase::CaseSensitive))
    { if(A&&!Loaded(A)) { R.bChangeStateKnown=false; }Fail(R,TEXT("enhanced_input_action_not_found_or_scope_pending"),TEXT("Original graph-before-load prefix retained. Exact ready native InputAction asset required; missing/generated/custom/redirect/async variants remain tracked, no load replay."));return R; }
    const auto ClassBefore=Source(C,R),ActionClassBefore=Source(ActionClass,R),ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);R.Data->SetObjectField(TEXT("action_class_source_before"),ActionClassBefore);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);
    bool FullBefore=false;const auto ActionBefore=ActionFields(A,R,FullBefore);R.Data->SetObjectField(TEXT("action_before"),ActionBefore);const auto SettingsBefore=Settings(R);R.Data->SetObjectField(TEXT("settings_before"),SettingsBefore);
    Count(R,TEXT("action_layout_observation_calls"));const auto* Property=FindFProperty<FObjectProperty>(C,TEXT("InputAction"));const auto* Value=FindFProperty<FEnumProperty>(ActionClass,TEXT("ValueType"));const bool Layout=Property&&Property->GetOwnerStruct()==C&&Property->ArrayDim==1&&Property->PropertyClass==ActionClass&&Value&&Value->GetOwnerStruct()==ActionClass&&Value->ArrayDim==1&&Value->GetUnderlyingProperty()&&Value->GetUnderlyingProperty()->IsA<FByteProperty>()&&Loaded(Value->GetEnum())&&Value->GetEnum()->GetPathName().Equals(TEXT("/Script/EnhancedInput.EInputActionValueType"),ESearchCase::CaseSensitive);
    R.Data->SetBoolField(TEXT("public_input_action_property_layout_verified"),Layout);if(!Layout||!FullBefore) { Fail(R,TEXT("enhanced_input_action_layout_or_snapshot_pending"),TEXT("Actual public InputAction/ValueType reflection layout and bounded native asset fields required before unchecked SDK type map/allocation."));return R; }
    const uint64 V=Value->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Value->ContainerPtrToValuePtr<void>(A));R.Data->SetNumberField(TEXT("value_type"),static_cast<double>(V));if(V>3) { Fail(R,TEXT("enhanced_input_value_type_invalid"),TEXT("Installed public InputAction value type map admits exactly Boolean/Axis1D/Axis2D/Axis3D; invalid enum refused before node allocation."));return R; }
    R.Data->SetStringField(TEXT("value_type_name"),Value->GetEnum()->GetNameStringByValue(static_cast<int64>(V)));R.Data->SetBoolField(TEXT("cold_action_load_global_side_effects_verified"),false);
    Count(R,TEXT("enum_observation_calls"));UEnum* Events=FindObject<UEnum>(nullptr,TEXT("/Script/EnhancedInput.ETriggerEvent"));if(!Loaded(Events)||Events->NumEnums()>16) { Fail(R,TEXT("enhanced_input_trigger_enum_pending"),TEXT("Exact loaded finite native trigger enum required; no module/enum force load."));return R; }const TStrongObjectPtr<UEnum> KeepEvents(Events);TArray<FName> ExecNames;TArray<TSharedPtr<FJsonValue>> EnumRows;
    for(int32 I=0;I<Events->NumEnums()-1;++I) { if(!Events->HasMetaData(TEXT("Hidden"),I)) { const FName Pin(*Events->GetNameStringByIndex(I));if(ExecNames.Contains(Pin)) { Fail(R,TEXT("enhanced_input_trigger_enum_invalid"),TEXT("Duplicate visible trigger enum pin name."));return R; }ExecNames.Add(Pin);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),Pin.ToString());J->SetNumberField(TEXT("value"),static_cast<double>(Events->GetValueByIndex(I)));EnumRows.Add(MakeShared<FJsonValueObject>(J)); } }
    R.Data->SetArrayField(TEXT("trigger_event_rows"),EnumRows);if(ExecNames.Num()!=5) { Fail(R,TEXT("enhanced_input_trigger_enum_shape_pending"),TEXT("Pinned UE5.8 trigger enum requires five visible event rows; broader future layout not guessed."));return R; }
    UScriptStruct* ValueStruct=V==2?TBaseStructure<FVector2D>::Get():V==3?TBaseStructure<FVector>::Get():nullptr;const TStrongObjectPtr<UScriptStruct> KeepValueStruct(ValueStruct);TSharedPtr<FJsonObject> StructBefore;if(ValueStruct) { StructBefore=Source(ValueStruct,R);R.Data->SetObjectField(TEXT("value_struct_source_before"),StructBefore); }
    R.Stage=TEXT("create_enhanced_input_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node>(G,C);Count(R,TEXT("node_allocation_returns"));const TStrongObjectPtr<UK2Node> KeepNode(Node);if(!Node) { Fail(R,TEXT("enhanced_input_allocation_failed"),TEXT("Enhanced node allocation returned null; actual prefix retained."));return R; }
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());Count(R,TEXT("input_action_assignment_calls"));Property->SetObjectPropertyValue_InContainer(Node,A);Count(R,TEXT("input_action_assignment_returns"));Place(G,Node,XY,R);Mark(BP,R);const bool Observed=After(BP,G,Node,Guid,Before,R);
    bool FullAfter=false;const auto ActionAfter=ActionFields(A,R,FullAfter);R.Data->SetObjectField(TEXT("action_after"),ActionAfter);R.Data->SetObjectField(TEXT("settings_after"),Settings(R));const auto ClassAfter=Source(C,R),ActionClassAfter=Source(ActionClass,R),ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);R.Data->SetObjectField(TEXT("action_class_source_after"),ActionClassAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);
    bool Same=FullAfter&&Equal(MakeShared<FJsonValueObject>(ActionBefore),MakeShared<FJsonValueObject>(ActionAfter))&&Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(ActionClassBefore),MakeShared<FJsonValueObject>(ActionClassAfter))&&Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter));if(ValueStruct) { const auto StructAfter=Source(ValueStruct,R);R.Data->SetObjectField(TEXT("value_struct_source_after"),StructAfter);Same&=Equal(MakeShared<FJsonValueObject>(StructBefore),MakeShared<FJsonValueObject>(StructAfter)); }R.Data->SetBoolField(TEXT("action_and_native_sources_preserved"),Same);R.bChanged=R.bChanged||!Same;if(!Observed) { return R; }
    Count(R,TEXT("input_action_storage_observation_calls"));UObject* Stored=Property->GetObjectPropertyValue_InContainer(Node);R.Data->SetStringField(TEXT("stored_input_action"),GetPathNameSafe(Stored));Pins(Node,R);
    bool Shape=FreshPins(Node)&&Node->Pins.Num()==ExecNames.Num()+4&&!Node->IsNodePure()&&Node->AdvancedPinDisplay==ENodeAdvancedPins::Hidden;for(const FName& Pin:ExecNames) { Shape&=PinType(Node->FindPin(Pin),UEdGraphSchema_K2::PC_Exec); }
    auto* ValuePin=Node->FindPin(TEXT("ActionValue"));const auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());bool DefaultValid=false;if(ValuePin) { Count(R,TEXT("pin_default_match_calls"));const bool Match=Schema->DoesDefaultValueMatch(*ValuePin,ValuePin->AutogeneratedDefaultValue);R.Data->SetBoolField(TEXT("action_value_default_match"),Match);Count(R,TEXT("pin_default_validity_calls"));const FString Error=Schema->IsCurrentPinDefaultValid(ValuePin);R.Data->SetStringField(TEXT("action_value_default_validation_error"),Error);DefaultValid=Match&&Error.IsEmpty(); }Shape&=PinType(ValuePin,V==0?UEdGraphSchema_K2::PC_Boolean:V==1?UEdGraphSchema_K2::PC_Real:UEdGraphSchema_K2::PC_Struct,V==1?UEdGraphSchema_K2::PC_Double:NAME_None,ValueStruct)&&DefaultValid;
    auto* Elapsed=Node->FindPin(TEXT("ElapsedSeconds"));auto* Triggered=Node->FindPin(TEXT("TriggeredSeconds"));auto* AssetPin=Node->FindPin(TEXT("InputAction"));Shape&=PinType(Elapsed,UEdGraphSchema_K2::PC_Real,UEdGraphSchema_K2::PC_Double)&&Elapsed&&Elapsed->bAdvancedView&&PinType(Triggered,UEdGraphSchema_K2::PC_Real,UEdGraphSchema_K2::PC_Double)&&Triggered&&Triggered->bAdvancedView&&PinType(AssetPin,UEdGraphSchema_K2::PC_Object,NAME_None,ActionClass)&&AssetPin&&AssetPin->DefaultObject==A&&AssetPin->DefaultValue.Equals(A->GetName(),ESearchCase::CaseSensitive)&&AssetPin->bAdvancedView;R.Data->SetBoolField(TEXT("enhanced_input_pin_shape_verified"),Shape);R.Data->SetStringField(TEXT("pin_visibility_scope"),TEXT("Current exec advanced/hidden/friendly/default fields retained; settings and supported-trigger behavior are SDK-owned. No assumption every event is visible/supported; no setting/trigger/modifier mutation."));
    Count(R,TEXT("graph_compatibility_observation_calls"));const bool Compatible=Node->IsCompatibleWithGraph(G);R.Data->SetBoolField(TEXT("current_graph_compatible"),Compatible);
    if(!Loaded(Node)||Node->GetClass()!=C||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!BP->GetOutermost()->IsDirty()||Stored!=A||!Shape||!Same||!Compatible||!OwnerScope(BP,G,R))
    { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("enhanced_input_postcondition_failed"),TEXT("Current typed4 event/pin/default/action/graph/source proof failed; direct original duplicate/new node prefix retained without automatic cleanup or replay.")); }return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
