// DQ original direct self-member getter. No component existence preflight or full compiler/save.
#include "Compatibility/UE58/CubelessBlueprintComponentReferenceApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/Actor.h"

#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintComponentReference
{
namespace
{
using FResult=Variable::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
TSharedRef<FJsonObject> FunctionJson(const Function::FResult& R)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),R.bSuccess);J->SetBoolField(TEXT("mutation_attempted"),R.bMutationAttempted);
    if(R.bChangeStateKnown) { J->SetBoolField(TEXT("changed"),R.bChanged); }else { J->SetField(TEXT("changed"),MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);J->SetBoolField(TEXT("postcondition_passed"),R.bPostconditionPassed);J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("error_code"),R.ErrorCode);J->SetStringField(TEXT("error"),R.Error);J->SetObjectField(TEXT("data"),R.Data);return J;
}
void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("position_parse_calls"),TEXT("position_parse_returns"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("source_observation_calls"),TEXT("node_class_lookup_calls"),TEXT("node_class_lookup_returns"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("self_member_set_calls"),TEXT("self_member_set_returns"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("legacy_node_metadata_observation_calls"),TEXT("property_resolution_calls"),TEXT("property_resolution_returns"),TEXT("property_pin_type_calls"),TEXT("event_graph_create_calls"),TEXT("event_graph_create_returns"),TEXT("add_ubergraph_page_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_graph_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("destroy_node_calls"),TEXT("component_existence_validation_calls"),TEXT("runtime_component_lookup_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("operation"),TEXT("add_blueprint_get_self_component_reference"));
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Original outer transaction; NewObject native VariableGet, SetSelfMember literal FName, position, AddNode(false,true), GUID, PostPlaced, AllocateDefaultPins, ReconstructNode, MarkBlueprintAsModified. No explicit Modify, transactional flag, component check, added cleanup, full compiler or save. Public graph helpers/MarkModified/getters can regenerate skeleton/dirty/status/cache/member-reference state; direct counts are not SDK-internal totals."));
    R.Data->SetStringField(TEXT("preservation_scope"),TEXT("Selected graph old nodes/pins/defaults/links and ordered graph metadata, exact native VariableGet class/parent source metadata. SCS/CDO/world/other graph node fields and all loader/editor caches are not complete observations."));
    R.Data->SetBoolField(TEXT("component_existence_preflight_performed"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("full_original_pipeline_or_runtime_verified"),false);
}

bool Position(const FString& Json,int32 (&XY)[2],FString& Projected,FResult& R)
{
    TSharedPtr<FJsonValue> V;Function::FResult Parse;Count(R,TEXT("position_parse_calls"));const bool Parsed=Function::Pin::ParseJson(Json,V,Parse);Count(R,TEXT("position_parse_returns"));R.Data->SetObjectField(TEXT("position_parse"),FunctionJson(Parse));if(!Parsed) { return Fail(R,Parse.ErrorCode.IsEmpty()?TEXT("component_position_json_invalid"):*Parse.ErrorCode,Parse.Error); }
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

TSharedRef<FJsonObject> Source(const UStruct* Struct,FResult& R)
{
    Count(R,TEXT("source_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Struct->GetPathName());J->SetStringField(TEXT("class"),Struct->GetClass()->GetPathName());J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(Struct->GetFlags())));J->SetStringField(TEXT("super"),GetPathNameSafe(Struct->GetSuperStruct()));J->SetStringField(TEXT("package"),Struct->GetOutermost()->GetName());J->SetStringField(TEXT("package_flags"),LexToString(static_cast<uint32>(Struct->GetOutermost()->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Struct->GetOutermost()->IsDirty());
    if(const UClass* Class=Cast<UClass>(Struct)) { J->SetStringField(TEXT("class_flags"),LexToString(static_cast<uint32>(Class->GetClassFlags())));J->SetStringField(TEXT("generated_by"),GetPathNameSafe(Class->ClassGeneratedBy)); }
    if(const UScriptStruct* Script=Cast<UScriptStruct>(Struct)) { J->SetStringField(TEXT("struct_flags"),LexToString(static_cast<uint32>(Script->StructFlags))); }
    return J;
}


bool OwnerScope(UBlueprint* BP,UEdGraph* G,FResult& R)
{
    if(!Loaded(BP)||!Loaded(BP->ParentClass)||(G&&(!Loaded(G)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP||!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>=2048))
        ||(BP->BlueprintType!=BPTYPE_Normal&&BP->BlueprintType!=BPTYPE_FunctionLibrary&&BP->BlueprintType!=BPTYPE_MacroLibrary)
        ||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated)
    { return Fail(R,TEXT("component_reference_owner_scope_pending"),TEXT("Idle owned editable K2 graph required; noneditable/delegate/animation/generated-parent/editor owners remain explicit residuals.")); }
    int32 Depth=0;for(UClass* C=BP->ParentClass;C;C=C->GetSuperClass())
    {
        const FString Package=C->GetOutermost()->GetName();
        if(++Depth>64||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->IsEditorOnly()||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)
            ||(Package!=TEXT("/Script/Engine")&&Package!=TEXT("/Script/CoreUObject")&&Package!=TEXT("/Script/UMG")))
        { return Fail(R,TEXT("component_reference_parent_scope_pending"),TEXT("Loaded native Engine/CoreUObject/UMG runtime parent chain required, including Actor/Component/Widget; custom/generated/editor/plugin hierarchies remain tracked.")); }
    }
    return true;
}
UBlueprint* Load(const FString& Requested,FResult& R)
{
    FString Path=Requested;if(!Path.Contains(TEXT("/"))&&!Path.Contains(TEXT("."))) { Path=TEXT("/Game/Blueprints/")+Path; }
    if(!Function::Pin::SafeName(Path)||!Path.StartsWith(TEXT("/Game/"))) { Fail(R,TEXT("component_reference_blueprint_path_invalid"),TEXT("Bounded /Game Blueprint path or original /Game/Blueprints simple name required; global ambiguous/LevelScript resolution remains a residual."));return nullptr; }
    const FString ObjectPath=Path.Contains(TEXT("."))?Path:Path+TEXT(".")+FPackageName::GetShortName(Path);if(!Function::Pin::SafeName(ObjectPath)) { Fail(R,TEXT("component_reference_blueprint_path_invalid"),TEXT("Completed Blueprint object path exceeds input bounds."));return nullptr; }
    const bool Already=FindObject<UBlueprint>(nullptr,*ObjectPath)!=nullptr;R.Data->SetBoolField(TEXT("asset_already_loaded"),Already);R.Data->SetBoolField(TEXT("implicit_postload_work_possible"),!Already);
    Count(R,TEXT("asset_load_calls"));UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*ObjectPath);Count(R,TEXT("asset_load_returns"));R.Data->SetStringField(TEXT("blueprint"),GetPathNameSafe(BP));
    if(!Loaded(BP)) { R.bChangeStateKnown=Already;Fail(R,TEXT("component_reference_blueprint_unavailable"),TEXT("Blueprint not found or has unfinished load/destruction; load effects not certified zero."));return nullptr; }return BP;
}
TSharedRef<FJsonObject> GraphInventory(UBlueprint* BP,bool& Complete)
{
    auto J=MakeShared<FJsonObject>();TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);Complete=Graphs.Num()<=256;TArray<TSharedPtr<FJsonValue>> Rows;TSet<FGuid> Ids;
    for(UEdGraph* G:Graphs)
    {
        if(!Loaded(G)||!G->GraphGuid.IsValid()||Ids.Contains(G->GraphGuid)) { Complete=false;continue; }Ids.Add(G->GraphGuid);
        auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("path"),G->GetPathName());Q->SetStringField(TEXT("guid"),G->GraphGuid.ToString(EGuidFormats::Digits));Q->SetStringField(TEXT("name"),G->GetName());Q->SetStringField(TEXT("schema"),GetPathNameSafe(G->GetSchema()));Q->SetNumberField(TEXT("node_count"),G->Nodes.Num());Q->SetBoolField(TEXT("editable"),G->bEditable);Rows.Add(MakeShared<FJsonValueObject>(Q));
    }
    J->SetArrayField(TEXT("graphs"),Rows);J->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());J->SetNumberField(TEXT("status"),static_cast<int32>(BP->Status.GetValue()));J->SetBoolField(TEXT("complete"),Complete);return J;
}
bool InventoryPreserved(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,int32 Added)
{
    const auto& A=Before->GetArrayField(TEXT("graphs"));const auto& B=After->GetArrayField(TEXT("graphs"));if(B.Num()!=A.Num()+Added) { return false; }
    int32 Previous=-1;for(const auto& Old:A)
    {
        const FString Path=Old->AsObject()->GetStringField(TEXT("path"));int32 Found=INDEX_NONE;
        for(int32 I=0;I<B.Num();++I) { if(B[I]->AsObject()->GetStringField(TEXT("path")).Equals(Path,ESearchCase::CaseSensitive)) { if(Found!=INDEX_NONE) { return false; }Found=I; } }
        if(Found<=Previous||Found==INDEX_NONE||!Equal(Old,B[Found])) { return false; }Previous=Found;
    }return true;
}
bool Observe(UBlueprint* BP,UEdGraph* G,const TCHAR* Key,Function::FResult& Out,FResult& R)
{
    Count(R,TEXT("shared_graph_resolve_calls"));Out=Function::Resolve(BP->GetPathName(),TEXT(""),G->GraphGuid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(Key,FunctionJson(Out));
    if(!Out.bSuccess||!Out.bChangeStateKnown||!Out.bPostconditionPassed) { R.bChangeStateKnown=false;return Fail(R,TEXT("component_reference_observation_incomplete"),TEXT("Current graph snapshot did not close; actual prefix retained without retry or cleanup.")); }return true;
}
UEdGraph* Graph(UBlueprint* BP,const FString& Name,const FString& Id,const FString& Type,bool Create,FResult& R)
{
    FString Kind=Type.ToLower();Kind.TrimStartAndEndInline();Kind.ReplaceInline(TEXT(" "),TEXT(""));Kind.ReplaceInline(TEXT("_"),TEXT(""));Kind.ReplaceInline(TEXT("-"),TEXT(""));
    const bool Default=Id.IsEmpty()&&Name.IsEmpty()&&(Kind.IsEmpty()||(Create&&(Kind==TEXT("event")||Kind==TEXT("eventgraph")||Kind==TEXT("ubergraph"))));
    if(Default)
    {
        for(UEdGraph* G:BP->UbergraphPages) { if(!Loaded(G)) { Fail(R,TEXT("component_reference_default_graph_invalid"),TEXT("Original default graph inventory contains an invalid graph."));return nullptr; }if(G->GetName().Contains(TEXT("EventGraph"))) { R.Data->SetBoolField(TEXT("default_graph_created"),false);return G; } }
        if(!OwnerScope(BP,nullptr,R)) { return nullptr; }bool Complete=false;const auto Before=GraphInventory(BP,Complete);R.Data->SetObjectField(TEXT("before_default_graph_creation"),Before);if(!Complete) { Fail(R,TEXT("component_reference_graph_inventory_incomplete"),TEXT("Default graph creation requires bounded valid existing graph identities."));return nullptr; }
        R.Stage=TEXT("create_default_event_graph");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("event_graph_create_calls"));UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());Count(R,TEXT("event_graph_create_returns"));R.Data->SetBoolField(TEXT("default_graph_returned"),G!=nullptr);R.Data->SetStringField(TEXT("default_graph_path"),GetPathNameSafe(G));
        if(!Loaded(G)) { Fail(R,TEXT("component_reference_default_graph_failed"),TEXT("Public CreateNewGraph did not return a usable graph; original creation prefix retained."));return nullptr; }
        const TStrongObjectPtr<UEdGraph> Keep(G);Count(R,TEXT("add_ubergraph_page_calls"));FBlueprintEditorUtils::AddUbergraphPage(BP,G);R.Data->SetBoolField(TEXT("implicit_structural_skeleton_compile_possible"),true);R.Data->SetBoolField(TEXT("default_graph_attached"),BP->UbergraphPages.Contains(G));const auto After=GraphInventory(BP,Complete);R.Data->SetObjectField(TEXT("after_default_graph_creation"),After);
        const bool Preserved=Complete&&InventoryPreserved(Before,After,1);R.Data->SetBoolField(TEXT("default_graph_old_graph_metadata_preserved"),Preserved);R.bChanged=true;R.bChangeStateKnown=Complete;
        if(!Loaded(G)||G->GetOuter()!=BP||!BP->UbergraphPages.Contains(G)||!Preserved) { Fail(R,TEXT("component_reference_default_graph_postcondition_failed"),TEXT("Default graph creation/attachment or old graph metadata differs; no rollback is claimed."));return nullptr; }R.Data->SetBoolField(TEXT("default_graph_created"),true);return G;
    }
    Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(BP->GetPathName(),Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),FunctionJson(S));R.bMutationAttempted|=S.bMutationAttempted;R.bChanged|=S.bChanged;R.bChangeStateKnown&=S.bChangeStateKnown;
    if(!S.bSuccess||!S.bPostconditionPassed) { Fail(R,S.ErrorCode.IsEmpty()?TEXT("component_reference_graph_unverified"):*S.ErrorCode,S.Error);return nullptr; }
    return FindObject<UEdGraph>(nullptr,*S.Data->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")));
}
TSharedRef<FJsonObject> Reference(UK2Node_Variable* Node)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("member_name"),Node->VariableReference.GetMemberName().ToString());J->SetBoolField(TEXT("self_context"),Node->VariableReference.IsSelfContext());J->SetBoolField(TEXT("local_scope"),Node->VariableReference.IsLocalScope());J->SetStringField(TEXT("member_guid"),Node->VariableReference.GetMemberGuid().ToString(EGuidFormats::Digits));J->SetStringField(TEXT("member_parent"),GetPathNameSafe(Node->VariableReference.GetMemberParentClass()));return J;
}
bool PinShape(UK2Node_Variable* Node,FProperty* Property,FResult& R)
{
    bool Valid=true;TSet<FGuid> Ids;for(const UEdGraphPin* P:Node->Pins) { if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { Valid=false;continue; }Ids.Add(P->PinId); }
    R.Data->SetNumberField(TEXT("current_pin_count"),Node->Pins.Num());R.Data->SetBoolField(TEXT("current_member_resolved"),Property!=nullptr);R.Data->SetStringField(TEXT("current_property_path"),Property?Property->GetPathName():FString());R.Data->SetStringField(TEXT("current_property_owner"),Property?GetPathNameSafe(Property->GetOwnerStruct()):FString());R.Data->SetBoolField(TEXT("current_node_pure"),Node->IsNodePure());
    if(!Property) { R.Data->SetStringField(TEXT("unresolved_scope"),TEXT("Original deliberately skips component verification. Empty/unresolved member creates a node with zero pins; compiler diagnostics and runtime component access are not tested or claimed."));return Valid&&Node->Pins.IsEmpty()&&Node->IsNodePure(); }
    FEdGraphPinType Expected;const auto* Schema=CastChecked<UEdGraphSchema_K2>(Node->GetGraph()->GetSchema());Count(R,TEXT("property_pin_type_calls"));const bool Converted=Schema->ConvertPropertyToPinType(Property,Expected);const auto* Value=Node->FindPin(Node->GetVarName(),EGPD_Output);const auto* Self=Node->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
    R.Data->SetBoolField(TEXT("property_is_object"),CastField<FObjectPropertyBase>(Property)!=nullptr);R.Data->SetStringField(TEXT("property_type_scope"),TEXT("Actual property pin type is read from the public schema; component subclass/existence is not a prerequisite. Non-component self members retain original direct-getter semantics."));
    return Valid&&Converted&&Node->Pins.Num()==2&&Value&&Value->PinType==Expected&&Self&&Self->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Node->IsNodePure();
}

// KC loaded generated-Actor exception; existing native-owner path remains below.
struct FGeneratedReferenceScope
{
    FResult& R; TStrongObjectPtr<UEditorEngine> Editor; TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UBlueprint> BP,Declaring; TStrongObjectPtr<UEdGraph> G;
    TStrongObjectPtr<USCS_Node> Member; TStrongObjectPtr<UK2Node_Variable> Node;
    TArray<TStrongObjectPtr<UObject>> Holds; TArray<UObject*> SourceRoles;
    TArray<UEdGraphNode*> OldNodes; TArray<TArray<UEdGraphPin*>> OldPins;
    FString SourcesBefore,OldBefore,NewPins,NodePath; FGuid GraphGuid,NodeGuid;
    FMemberReference NewReference;bool NewReferenceDeprecated=false;
    UClass *Parent=nullptr,*Generated=nullptr,*Skeleton=nullptr,*NodeClass=nullptr;
    FName Requested;int32 X=0,Y=0;bool Added=false,GuidFixed=false,PinsFixed=false;
    explicit FGeneratedReferenceScope(FResult& In):R(In){}
    bool Stop(const TCHAR* Boundary)
    {R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=false;R.Stage=TEXT("verification");R.Data->SetStringField(TEXT("generated_failed_boundary"),Boundary);R.Data->SetBoolField(TEXT("generated_followup_sdk_stopped"),true);return Fail(R,TEXT("component_reference_generated_state_unavailable"),TEXT("Generated parent/target/member/pin identity or selected state changed across an SDK boundary; actual prefix retained without further SDK, cleanup, compile/save or retry."));}
    bool Context()const
    {if(!IsEditorGameThread()||IsGarbageCollecting()||IsEngineExitRequested()||GIsRequestingExit||UE::IsSavingPackage()||!Editor.IsValid()||GEditor!=Editor.Get()||!Loaded(GEditor)||!World.IsValid()||!Loaded(World.Get()))return false;return !IsPlaySessionActive()&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get();}
    bool Hold(UObject* O)
    {if(!Loaded(O))return false;for(const auto& H:Holds)if(H.Get()==O)return true;Holds.Emplace(O);return true;}
    static FString Identity(const UObject* O)
    {return O?O->GetPathName()+TEXT("|")+O->GetClass()->GetPathName()+TEXT("|")+GetPathNameSafe(O->GetOuter())+TEXT("|")+LexToString(static_cast<uint32>(O->GetFlags())):FString();}
    static bool Pins(UEdGraphNode* N,FString& Out)
    {
        if(!Loaded(N)||N->Pins.Num()>512)return false;auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;int64 TextSize=0;
        for(auto* P:N->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->ParentPin||!P->SubPins.IsEmpty())return false;
            auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("id"),P->PinId.ToString(EGuidFormats::Digits));Q->SetStringField(TEXT("name"),P->PinName.ToString());Q->SetNumberField(TEXT("direction"),P->Direction);
            FString Type;FEdGraphPinType::StaticStruct()->ExportText(Type,&P->PinType,nullptr,nullptr,PPF_None,nullptr);Q->SetStringField(TEXT("type"),Type);
            const FString DefaultText=P->DefaultTextValue.ToString();TextSize+=Type.Len()+P->DefaultValue.Len()+P->AutogeneratedDefaultValue.Len()+DefaultText.Len();if(P->DefaultValue.Len()>65536||P->AutogeneratedDefaultValue.Len()>65536||DefaultText.Len()>65536||TextSize>4194304)return false;Q->SetStringField(TEXT("default"),P->DefaultValue);Q->SetStringField(TEXT("auto"),P->AutogeneratedDefaultValue);Q->SetStringField(TEXT("text"),DefaultText);Q->SetStringField(TEXT("object"),GetPathNameSafe(P->DefaultObject.Get()));
            Q->SetBoolField(TEXT("hidden"),P->bHidden);Q->SetBoolField(TEXT("ignored"),P->bDefaultValueIsIgnored);Q->SetBoolField(TEXT("readonly"),P->bDefaultValueIsReadOnly);Q->SetBoolField(TEXT("not_connectable"),P->bNotConnectable);Q->SetBoolField(TEXT("orphan"),P->bOrphanedPin);
            TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:P->LinkedTo)Links.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%p"),L)));Q->SetArrayField(TEXT("link_addresses_only"),Links);Rows.Add(MakeShared<FJsonValueObject>(Q));
        }
        J->SetArrayField(TEXT("pins"),Rows);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return true;
    }
    bool OldState(FString& Out)
    {
        if(!Loaded(BP.Get())||!Loaded(G.Get())||G->Nodes.Num()!=OldNodes.Num()+(Added?1:0))return false;
        auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;int64 TextSize=0;
        for(int32 I=0;I<OldNodes.Num();++I)
        {
            auto* N=OldNodes[I];if(!Loaded(N)||G->Nodes[I].Get()!=N||N->GetOuter()!=G.Get()||N->Pins!=OldPins[I])return false;FString P;if(!Pins(N,P))return false;TextSize+=P.Len();if(TextSize>4194304)return false;
            auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("identity"),Identity(N));Q->SetStringField(TEXT("guid"),N->NodeGuid.ToString(EGuidFormats::Digits));Q->SetNumberField(TEXT("x"),N->NodePosX);Q->SetNumberField(TEXT("y"),N->NodePosY);Q->SetStringField(TEXT("comment"),N->NodeComment);Q->SetStringField(TEXT("pins"),P);if(auto* V=Cast<UK2Node_Variable>(N))Q->SetObjectField(TEXT("reference"),Reference(V));Rows.Add(MakeShared<FJsonValueObject>(Q));
        }
        J->SetArrayField(TEXT("nodes"),Rows);TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);if(Graphs.Num()>256||!Graphs.Contains(G.Get()))return false;TArray<TSharedPtr<FJsonValue>> GR;
        for(auto* Q:Graphs){if(!Loaded(Q))return false;auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("identity"),Identity(Q));V->SetStringField(TEXT("guid"),Q->GraphGuid.ToString(EGuidFormats::Digits));V->SetNumberField(TEXT("node_count"),Q->Nodes.Num()-(Q==G.Get()&&Added?1:0));GR.Add(MakeShared<FJsonValueObject>(V));}
        J->SetArrayField(TEXT("graphs"),GR);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return true;
    }
    bool RequestedProperty(const FProperty* P,UClass* DeclaringRole,UClass* ComponentClass)const
    {const auto* O=CastField<FObjectPropertyBase>(P);return P&&O&&P->GetFName()==Requested&&P->ArrayDim==1&&O->PropertyClass.Get()==ComponentClass&&P->GetOwnerClass()==DeclaringRole;}
    bool Sources(FString& Out,TArray<UObject*>& Roles,bool Capture)
    {
        if(!Context()||!Loaded(BP.Get())||BP->ParentClass.Get()!=Parent||BP->GeneratedClass.Get()!=Generated||BP->SkeletonGeneratedClass.Get()!=Skeleton)return false;
        TArray<UBlueprint*> Bs={BP.Get()};int32 Depth=0;
        for(UClass* C=Parent;C;C=C->GetSuperClass())
        {
            if(++Depth>64||!Loaded(C)||C->IsEditorOnly()||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists))return false;Roles.Add(C);if(Capture&&!Hold(C))return false;
            if(C->HasAnyClassFlags(CLASS_Native)){const FString P=C->GetPackage()->GetName();if(C->ClassGeneratedBy||(P!=TEXT("/Script/Engine")&&P!=TEXT("/Script/CoreUObject")))return false;}
            else{auto* B=Cast<UBlueprint>(C->ClassGeneratedBy);if(!Loaded(B)||!Cast<UBlueprintGeneratedClass>(C)||B->GeneratedClass.Get()!=C||B->GetPackage()->IsDirty()||B->BlueprintType!=BPTYPE_Normal||B->bBeingCompiled||B->bQueuedForCompilation||B->bSuppressStructurallyModified||B->bIsRegeneratingOnLoad||(B->Status!=BS_UpToDate&&B->Status!=BS_UpToDateWithWarnings))return false;Bs.Add(B);}
        }
        auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;USCS_Node* Found=nullptr;UBlueprint* FoundBP=nullptr;int32 Matches=0;int64 TextSize=0;
        for(auto* B:Bs)
        {
            if(!Loaded(B)||!Loaded(B->GeneratedClass.Get())||!Loaded(B->SkeletonGeneratedClass.Get())||!Loaded(B->SimpleConstructionScript.Get()))return false;
            auto* GC=Cast<UBlueprintGeneratedClass>(B->GeneratedClass.Get());if(!GC)return false;auto* H=GC->GetInheritableComponentHandler(false);if(H&&(!Loaded(H)||!H->IsEmpty()))return false;
            for(UObject* O:{static_cast<UObject*>(B),static_cast<UObject*>(B->GeneratedClass.Get()),static_cast<UObject*>(B->SkeletonGeneratedClass.Get()),static_cast<UObject*>(B->SimpleConstructionScript.Get())}){Roles.Add(O);if(Capture&&!Hold(O))return false;}
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("blueprint"),B->GetPathName());Row->SetStringField(TEXT("parent"),GetPathNameSafe(B->ParentClass.Get()));Row->SetStringField(TEXT("generated"),Identity(B->GeneratedClass.Get()));Row->SetStringField(TEXT("skeleton"),Identity(B->SkeletonGeneratedClass.Get()));Row->SetStringField(TEXT("scs"),Identity(B->SimpleConstructionScript.Get()));
            if(B!=BP.Get()){Row->SetBoolField(TEXT("dirty"),B->GetPackage()->IsDirty());Row->SetNumberField(TEXT("status"),B->Status.GetValue());}
            const auto& All=B->SimpleConstructionScript->GetAllNodes();if(All.Num()>256)return false;TSet<USCS_Node*> Reachable;TArray<USCS_Node*> Pending=B->SimpleConstructionScript->GetRootNodes();
            while(!Pending.IsEmpty()){auto* N=Pending.Pop();if(!Loaded(N)||Reachable.Contains(N)||!All.Contains(N)||N->GetOuter()!=B->SimpleConstructionScript.Get())return false;Reachable.Add(N);Pending.Append(N->GetChildNodes());}
            if(Reachable.Num()!=All.Num())return false;TArray<TSharedPtr<FJsonValue>> Nodes;
            for(auto* N:All)
            {
                auto* Class=N->ComponentClass.Get();auto* T=N->ComponentTemplate.Get();
                if(!Loaded(Class)||!Class->HasAnyClassFlags(CLASS_Native)||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Class->ClassGeneratedBy||Class->GetPackage()->GetName()!=TEXT("/Script/Engine")||Class->IsEditorOnly()||!Class->IsChildOf(UActorComponent::StaticClass())||!Loaded(T)||T->GetClass()!=Class||T->GetOuter()!=B->GeneratedClass.Get())return false;
                for(UObject* O:{static_cast<UObject*>(N),static_cast<UObject*>(Class),static_cast<UObject*>(T)}){Roles.Add(O);if(Capture&&!Hold(O))return false;}
                auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("node"),Identity(N));Q->SetStringField(TEXT("name"),N->GetVariableName().ToString());Q->SetStringField(TEXT("guid"),N->VariableGuid.ToString(EGuidFormats::Digits));Q->SetStringField(TEXT("class"),Identity(Class));Q->SetStringField(TEXT("template"),Identity(T));
                TArray<TSharedPtr<FJsonValue>> Children;for(auto* Child:N->GetChildNodes())Children.Add(MakeShared<FJsonValueString>(Child->GetPathName()));Q->SetArrayField(TEXT("children"),Children);
                TArray<TSharedPtr<FJsonValue>> Fields;int32 CountFields=0;for(TFieldIterator<FProperty> It(Class);It;++It)
                {if(It->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient))continue;if(++CountFields>1024)return false;FString V;It->ExportText_InContainer(0,V,T,nullptr,T,PPF_None);TextSize+=V.Len();if(V.Len()>65536||TextSize>4194304)return false;auto F=MakeShared<FJsonObject>();F->SetStringField(TEXT("name"),It->GetName());F->SetStringField(TEXT("value"),V);Fields.Add(MakeShared<FJsonValueObject>(F));}
                Q->SetArrayField(TEXT("serialized_nontransient_fields"),Fields);Nodes.Add(MakeShared<FJsonValueObject>(Q));if(N->GetVariableName()==Requested){++Matches;Found=N;FoundBP=B;}
            }
            Row->SetArrayField(TEXT("nodes"),Nodes);Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        if(Matches!=1||!Found||!Found->VariableGuid.IsValid())return false;if(Capture){Member.Reset(Found);Declaring.Reset(FoundBP);}else if(Member.Get()!=Found||Declaring.Get()!=FoundBP)return false;
        FProperty* P=BP->GeneratedClass->FindPropertyByName(Requested);FProperty* SP=BP->SkeletonGeneratedClass->FindPropertyByName(Requested);
        if(!RequestedProperty(P,FoundBP->GeneratedClass.Get(),Found->ComponentClass.Get())||!RequestedProperty(SP,FoundBP->SkeletonGeneratedClass.Get(),Found->ComponentClass.Get()))return false;
        J->SetStringField(TEXT("property"),P->GetName());J->SetStringField(TEXT("owner"),P->GetOwnerClass()->GetPathName());J->SetStringField(TEXT("flags"),LexToString(static_cast<uint64>(P->GetPropertyFlags())));J->SetStringField(TEXT("skeleton_property"),SP->GetName());J->SetStringField(TEXT("skeleton_owner"),SP->GetOwnerClass()->GetPathName());J->SetStringField(TEXT("skeleton_flags"),LexToString(static_cast<uint64>(SP->GetPropertyFlags())));J->SetArrayField(TEXT("sources"),Rows);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return Context();
    }
    bool Check(const TCHAR* Boundary)
    {
        Count(R,TEXT("generated_reference_current_checks"));
        if(!Context()||!Loaded(G.Get())||G->GraphGuid!=GraphGuid||G->GetOuter()!=BP.Get()||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad)return Stop(Boundary);
        FString S,Old;TArray<UObject*> Roles;if(!Sources(S,Roles,false)||Roles!=SourceRoles||S!=SourcesBefore||!OldState(Old)||Old!=OldBefore)return Stop(Boundary);
        if(Node.IsValid())
        {if(!Loaded(Node.Get())||Node->GetClass()!=NodeClass||Node->GetOuter()!=G.Get()||Node->GetPathName()!=NodePath||(Added&&(!G->Nodes.Contains(Node.Get())||G->Nodes.Last().Get()!=Node.Get()))||(!Added&&G->Nodes.Contains(Node.Get()))||(GuidFixed&&(Node->NodeGuid!=NodeGuid||Node->NodePosX!=X||Node->NodePosY!=Y)))return Stop(Boundary);if(PinsFixed){FString P;if(!Pins(Node.Get(),P)||P!=NewPins||!Node->VariableReference.IsSameReference(NewReference)||Node->VariableReference.IsDeprecated()!=NewReferenceDeprecated||Node->VariableReference.GetMemberName()!=Requested||!Node->VariableReference.IsSelfContext()||Node->VariableReference.IsLocalScope())return Stop(Boundary);}}
        return Context()||Stop(Boundary);
    }
};

FResult AddGeneratedLoaded(UBlueprint* BP,const FString& ComponentName,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool CreateGraph)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_component_name"),ComponentName);R.Data->SetBoolField(TEXT("requested_create_graph_if_missing"),CreateGraph);
    for(const TCHAR* K:{TEXT("generated_reference_current_checks"),TEXT("generated_parent_saved_inspection_calls")})R.Data->SetNumberField(K,0);
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("KC bounded generated-owner writer retains native NewObject/SetSelfMember/AddNode/GUID/Post/Pins/Reconstruct/Mark sequence, with native SCS declaration preflight and selected source/target checks across SDK and outer transaction end. No explicit Modify, compile/save, runtime getter, cleanup or retry."));
    R.Data->SetStringField(TEXT("preservation_scope"),TEXT("Selected target graph/pins/defaults, loaded generated Actor parent roles and native SCS nontransient serialized template fields; no transient/CDO/world/global/disk purity claim."));
    R.Data->SetStringField(TEXT("generated_reference_scope"),TEXT("KC already-loaded Actor child, saved-clean generated parents, unambiguous own/inherited native Engine SCS component and existing graph; nonempty override records, ambiguous/unresolved/generated component members remain pending. Native-parent permissive route unchanged."));
    if(!IsEditorGameThread()||IsGarbageCollecting()||IsEngineExitRequested()||GIsRequestingExit||UE::IsSavingPackage()||!Loaded(GEditor)||IsPlaySessionActive()){Fail(R,TEXT("component_reference_editor_invalid"),TEXT("Safe idle Editor game thread required."));return R;}
    if(CreateGraph||ComponentName.IsEmpty()||!Function::Pin::SafeName(ComponentName)||!Loaded(BP)||!Loaded(BP->ParentClass.Get())||!BP->ParentClass->ClassGeneratedBy||!BP->ParentClass->IsChildOf(AActor::StaticClass())||BP->BlueprintType!=BPTYPE_Normal||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad){Fail(R,TEXT("component_reference_generated_scope_pending"),TEXT("New exception requires loaded idle Actor child, native SCS literal member and existing graph; no new graph/inference."));return R;}
    FGeneratedReferenceScope C(R);C.Editor.Reset(GEditor);C.World.Reset(GEditor->GetEditorWorldContext().World());C.BP.Reset(BP);C.Parent=BP->ParentClass.Get();C.Generated=BP->GeneratedClass.Get();C.Skeleton=BP->SkeletonGeneratedClass.Get();C.Requested=FName(*ComponentName);
    if(!C.Context()){C.Stop(TEXT("before_preflight"));return R;}int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R))return R;C.X=XY[0];C.Y=XY[1];
    UEdGraph* G=Graph(BP,Name,Id,Type,false,R);if(!C.Context()){C.Stop(TEXT("after_graph_resolution"));return R;}if(!G)return R;
    if(!Loaded(G)||G->GetOuter()!=BP||!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>=2048){Fail(R,TEXT("component_reference_owner_scope_pending"),TEXT("Current owned bounded editable K2 graph required."));return R;}
    C.G.Reset(G);if(!C.Hold(const_cast<UEdGraphSchema*>(G->GetSchema()))){C.Stop(TEXT("graph_schema"));return R;}C.GraphGuid=G->GraphGuid;for(const auto& N:G->Nodes){if(!C.Hold(N.Get())){C.Stop(TEXT("old_node"));return R;}C.OldNodes.Add(N.Get());C.OldPins.Add(N->Pins);}
    R.Data->SetBoolField(TEXT("component_existence_preflight_performed"),true);Count(R,TEXT("component_existence_validation_calls"));const bool Declared=C.Sources(C.SourcesBefore,C.SourceRoles,true);R.Data->SetBoolField(TEXT("component_existence_validation_passed"),Declared);
    if(!Declared||!C.OldState(C.OldBefore)){Fail(R,TEXT("component_reference_generated_native_scs_pending"),TEXT("New generated-owner exception requires one reachable own/inherited native Engine SCS object property with exact current compiled/skeleton declaring roles, preserved native templates and no override records; no node writer."));return R;}
    R.Data->SetStringField(TEXT("component_declaration_scope"),C.Declaring.Get()==BP?TEXT("own_scs_in_generated_parent_actor"):TEXT("inherited_native_scs"));R.Data->SetStringField(TEXT("declaring_blueprint"),C.Declaring->GetPathName());R.Data->SetStringField(TEXT("declaring_scs_node"),C.Member->GetPathName());
    Count(R,TEXT("generated_parent_saved_inspection_calls"));const auto ParentInspection=GeneratedComponent::InspectSavedActorParent(C.Parent);R.Data->SetObjectField(TEXT("generated_parent_before"),ParentInspection);if(!C.Check(TEXT("after_saved_parent_inspection")))return R;
    if(!ParentInspection->GetBoolField(TEXT("success"))){Fail(R,TEXT("component_reference_generated_parent_pending"),TEXT("Saved clean generated Actor parent failed existing runtime/source/dependency guard; no node writer."));return R;}
    const auto* PriorUndo=GUndo;TSharedRef<FJsonObject> Core=MakeShared<FJsonObject>();
    {
        Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddSelfComponentReference","Add Blueprint Self Component Reference"));
        R=[&]() -> FResult
        {
            if(!C.Check(TEXT("after_transaction_begin")))return R;
const FGuid GraphGuid=G->GraphGuid;
    Function::FResult BeforeResult;if(!Observe(BP,G,TEXT("graph_before_observation"),BeforeResult,R)||!C.Check(TEXT("after_before_observation"))) { return R; }const auto Before=BeforeResult.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);
    Count(R,TEXT("node_class_lookup_calls"));UClass* Class=FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_VariableGet"));Count(R,TEXT("node_class_lookup_returns"));
    if(!Loaded(Class)||Class->GetSuperClass()!=UK2Node_Variable::StaticClass()||Class->ClassGeneratedBy||!Class->HasAnyClassFlags(CLASS_Native)||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Class->GetPathName()!=TEXT("/Script/BlueprintGraph.K2Node_VariableGet")) { Fail(R,TEXT("component_reference_node_class_unavailable"),TEXT("Exact already-loaded native VariableGet class required; no alternate node/module load."));return R; }
    C.NodeClass=Class;if(!C.Hold(Class)){C.Stop(TEXT("native_node_class"));return R;}const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R),ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);if(!C.Check(TEXT("after_native_class_sources")))return R;
    R.Stage=TEXT("create_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_Variable>(G,Class);Count(R,TEXT("node_allocation_returns"));R.Data->SetBoolField(TEXT("node_allocation_returned"),Node!=nullptr);const TStrongObjectPtr<UK2Node_Variable> KeepNode(Node);if(!Node) { Fail(R,TEXT("component_reference_allocation_failed"),TEXT("Failed to create get component node"));return R; }
    if(!Loaded(Node)){C.Stop(TEXT("allocated_node_unavailable"));return R;}C.Node.Reset(Node);C.NodePath=Node->GetPathName();if(!C.Check(TEXT("after_node_allocation")))return R;R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());const FName RequestedName(*ComponentName);Count(R,TEXT("self_member_set_calls"));Node->VariableReference.SetSelfMember(RequestedName);Count(R,TEXT("self_member_set_returns"));if(!C.Check(TEXT("after_self_member_set")))return R;R.Data->SetObjectField(TEXT("member_reference_after_set"),Reference(Node));Node->NodePosX=XY[0];Node->NodePosY=XY[1];
    UEdGraphNode* Base=Node;Count(R,TEXT("graph_add_node_calls"));G->AddNode(Base);C.Added=true;if(!C.Check(TEXT("after_graph_add")))return R;R.Data->SetBoolField(TEXT("graph_add_user_action"),false);Count(R,TEXT("new_guid_calls"));Base->CreateNewGuid();C.NodeGuid=Base->NodeGuid;C.GuidFixed=true;if(!C.NodeGuid.IsValid()||!C.Check(TEXT("after_new_guid")))return R;R.Data->SetStringField(TEXT("created_node_guid"),Base->NodeGuid.ToString(EGuidFormats::Digits));Count(R,TEXT("post_placed_calls"));Base->PostPlacedNewNode();if(!C.Check(TEXT("after_post_placed")))return R;Count(R,TEXT("allocate_pins_calls"));Base->AllocateDefaultPins();if(!C.Check(TEXT("after_allocate_pins")))return R;Count(R,TEXT("reconstruct_calls"));Base->ReconstructNode();if(!C.Check(TEXT("after_reconstruct")))return R;if(!FGeneratedReferenceScope::Pins(Node,C.NewPins)){C.Stop(TEXT("freeze_pins"));return R;}C.NewReference=Node->VariableReference;C.NewReferenceDeprecated=Node->VariableReference.IsDeprecated();auto FrozenReference=Reference(Node);FrozenReference->SetStringField(TEXT("member_scope"),Node->VariableReference.GetMemberScopeName());FrozenReference->SetBoolField(TEXT("deprecated"),C.NewReferenceDeprecated);R.Data->SetObjectField(TEXT("generated_reference_after_reconstruct"),FrozenReference);C.PinsFixed=true;if(!C.Check(TEXT("after_freeze_pins")))return R;
    R.Stage=TEXT("mark_modified");Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Count(R,TEXT("mark_blueprint_modified_returns"));if(!C.Check(TEXT("after_mark_modified")))return R;
    Count(R,TEXT("property_resolution_calls"));FProperty* Property=Node->GetPropertyForVariable();Count(R,TEXT("property_resolution_returns"));if(!C.Check(TEXT("after_property_resolution")))return R;FProperty* Compiled=BP->GeneratedClass->FindPropertyByName(C.Requested);FProperty* Skel=BP->SkeletonGeneratedClass->FindPropertyByName(C.Requested);if(!((Property==Compiled&&C.RequestedProperty(Compiled,C.Declaring->GeneratedClass.Get(),C.Member->ComponentClass.Get()))||(Property==Skel&&C.RequestedProperty(Skel,C.Declaring->SkeletonGeneratedClass.Get(),C.Member->ComponentClass.Get())))){C.Stop(TEXT("current_property_membership"));return R;}R.Data->SetObjectField(TEXT("member_reference_current"),Reference(Node));const bool Pins=PinShape(Node,Property,R);if(!C.Check(TEXT("after_pin_shape")))return R;if(!Pins){C.Stop(TEXT("current_pin_shape_invalid"));return R;}R.Data->SetBoolField(TEXT("current_pin_shape_verified"),Pins);
    Function::FResult AfterResult;const bool Observed=Observe(BP,G,TEXT("graph_after_observation"),AfterResult,R);if(!Observed||!C.Check(TEXT("after_graph_observation")))return R;const auto ClassAfter=Source(Class,R),ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);const bool Sources=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter));R.Data->SetBoolField(TEXT("native_source_preserved"),Sources);R.bChanged|=!Sources;
    if(!Observed) { return R; }const auto After=AfterResult.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),After);R.bChanged|=!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After));R.bChangeStateKnown=true;
    const auto* Row=After->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});if(Row) { R.Data->SetObjectField(TEXT("node"),(*Row)->AsObject()); }Count(R,TEXT("legacy_node_metadata_observation_calls"));const FText GeneratedTitle=Node->GetNodeTitle(ENodeTitleType::FullTitle);if(!C.Check(TEXT("after_legacy_node_title")))return R;R.Data->SetStringField(TEXT("name"),Node->GetName());R.Data->SetStringField(TEXT("class"),Node->GetClass()->GetName());R.Data->SetStringField(TEXT("title"),GeneratedTitle.ToString());R.Data->SetStringField(TEXT("node_comment"),Node->NodeComment);R.Data->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);R.Data->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());R.Data->SetStringField(TEXT("node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));R.Data->SetObjectField(TEXT("graph"),AfterResult.Data->GetObjectField(TEXT("graph")));auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());Legacy->SetObjectField(TEXT("graph"),AfterResult.Data->GetObjectField(TEXT("graph")));R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    const bool Old=ExistingNodesPreserved(Before,After,1),Rows=GraphRowsPreserved(Before,After,G->GetPathName(),1);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);const bool Ref=Node->VariableReference.IsSelfContext()&&!Node->VariableReference.IsLocalScope()&&Node->VariableReference.GetMemberName()==RequestedName;R.Data->SetBoolField(TEXT("current_requested_self_member_verified"),Ref);
    if(!Loaded(Node)||Node->GetClass()!=Class||Node->GetOuter()!=G||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||G->GraphGuid!=GraphGuid||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!Pins||!Ref||!Sources||!Old||!Rows||!Row||!BP->GetOutermost()->IsDirty()) { Fail(R,TEXT("component_reference_postcondition_failed"),TEXT("Current node/reference/pins/position/old graph or native source proof differs; actual node and partial state retained without rollback."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
        }();
        Core->SetBoolField(TEXT("success"),R.bSuccess);Core->SetBoolField(TEXT("mutation_attempted"),R.bMutationAttempted);Core->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);if(R.bChangeStateKnown)Core->SetBoolField(TEXT("changed"),R.bChanged);else Core->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());Core->SetBoolField(TEXT("postcondition_passed"),R.bPostconditionPassed);Core->SetStringField(TEXT("stage"),R.Stage);Core->SetStringField(TEXT("error_code"),R.ErrorCode);Core->SetStringField(TEXT("error"),R.Error);Core->SetBoolField(TEXT("compile_requested"),R.bCompileRequested);Core->SetBoolField(TEXT("compile_attempted"),R.bCompileAttempted);if(R.Compiled.IsSet())Core->SetBoolField(TEXT("compiled"),R.Compiled.GetValue());else Core->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());Core->SetBoolField(TEXT("save_requested"),R.bSaveRequested);Core->SetBoolField(TEXT("save_attempted"),R.bSaveAttempted);Core->SetBoolField(TEXT("saved"),R.bSaved);Core->SetObjectField(TEXT("data"),R.Data);
    }
    auto Combined=MakeShared<FJsonObject>();for(const auto& P:R.Data->Values)Combined->SetField(FString(P.Key.ToView()),P.Value);R.Data=Combined;R.Data->SetObjectField(TEXT("writer_receipt_at_return"),Core);R.Data->SetBoolField(TEXT("outer_transaction_scope_exited"),true);R.Data->SetBoolField(TEXT("generated_endpoint_observation_complete"),false);
    if(!R.bSuccess||!R.bChangeStateKnown||!R.bPostconditionPassed){R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;R.Data->SetBoolField(TEXT("failed_writer_engine_followup_skipped"),true);return R;}
    if(!C.Context()||GUndo!=PriorUndo){C.Stop(TEXT("after_transaction_context"));return R;}if(!C.Check(TEXT("after_transaction_end")))return R;
    Count(R,TEXT("generated_parent_saved_inspection_calls"));const auto ParentAfter=GeneratedComponent::InspectSavedActorParent(C.Parent);R.Data->SetObjectField(TEXT("generated_parent_after"),ParentAfter);if(!C.Check(TEXT("after_final_saved_parent_inspection")))return R;if(!ParentAfter->GetBoolField(TEXT("success"))){C.Stop(TEXT("final_parent_proof"));return R;}
    Function::FResult Endpoint;if(!Observe(BP,G,TEXT("generated_endpoint_graph_observation"),Endpoint,R)||!C.Check(TEXT("after_endpoint_graph_observation")))return R;
    if(!Equal(MakeShared<FJsonValueObject>(R.Data->GetObjectField(TEXT("after"))),MakeShared<FJsonValueObject>(Endpoint.Data->GetObjectField(TEXT("after"))))){C.Stop(TEXT("endpoint_differs_from_writer"));return R;}
    R.Data->SetBoolField(TEXT("generated_endpoint_observation_complete"),true);R.Data->SetBoolField(TEXT("generated_parent_target_scope_preserved"),true);R.Data->SetStringField(TEXT("generated_observation_limits"),TEXT("Loaded source roles/native SCS nontransient template fields/selected old graph and new pin/reference state only; transient template/CDO/world/global callback/disk effects unobserved. No atomic rollback, runtime getter execution or full original generated-owner parity."));return R;
}
}
Variable::FResult Add(const FString& Path,const FString& ComponentName,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool CreateGraph)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_component_name"),ComponentName);R.Data->SetBoolField(TEXT("requested_create_graph_if_missing"),CreateGraph);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("component_reference_editor_invalid"),TEXT("Idle Editor game thread required; no play-world authoring."));return R; }
    // Loaded-role routing does not load an asset/module or change native-owner writer behavior.
    FString RoutedPath=Path;if(!RoutedPath.Contains(TEXT("/"))&&!RoutedPath.Contains(TEXT(".")))RoutedPath=TEXT("/Game/Blueprints/")+RoutedPath;
    if(Function::Pin::SafeName(RoutedPath)&&RoutedPath.StartsWith(TEXT("/Game/")))
    {
        const FString ObjectPath=RoutedPath.Contains(TEXT("."))?RoutedPath:RoutedPath+TEXT(".")+FPackageName::GetShortName(RoutedPath);
        UBlueprint* LoadedTarget=Function::Pin::SafeName(ObjectPath)?FindObject<UBlueprint>(nullptr,*ObjectPath):nullptr;
        if(Loaded(LoadedTarget)&&Loaded(LoadedTarget->ParentClass.Get())&&LoadedTarget->ParentClass->ClassGeneratedBy)
            return AddGeneratedLoaded(LoadedTarget,ComponentName,PositionJson,Name,Id,Type,CreateGraph);
    }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddSelfComponentReference","Add Blueprint Self Component Reference"));
    if(ComponentName.Len()>=NAME_SIZE) { Fail(R,TEXT("component_reference_name_unrepresentable"),TEXT("Literal member FName exceeds NAME_SIZE; empty/unknown names otherwise remain admitted."));return R; }for(int32 I=0;I<ComponentName.Len();++I) { if(ComponentName[I]==TCHAR(0)) { Fail(R,TEXT("component_reference_name_unrepresentable"),TEXT("Literal member FName contains interior NUL."));return R; } }
    int32 XY[2]={0,0};FString PositionText;if(!Position(PositionJson,XY,PositionText,R)) { return R; }UBlueprint* BP=Load(Path,R);if(!BP) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);UEdGraph* G=Graph(BP,Name,Id,Type,CreateGraph,R);if(!G||!OwnerScope(BP,G,R)) { return R; }const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid GraphGuid=G->GraphGuid;
    Function::FResult BeforeResult;if(!Observe(BP,G,TEXT("graph_before_observation"),BeforeResult,R)) { return R; }const auto Before=BeforeResult.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);
    Count(R,TEXT("node_class_lookup_calls"));UClass* Class=FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_VariableGet"));Count(R,TEXT("node_class_lookup_returns"));
    if(!Loaded(Class)||Class->GetSuperClass()!=UK2Node_Variable::StaticClass()||Class->ClassGeneratedBy||!Class->HasAnyClassFlags(CLASS_Native)||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Class->GetPathName()!=TEXT("/Script/BlueprintGraph.K2Node_VariableGet")) { Fail(R,TEXT("component_reference_node_class_unavailable"),TEXT("Exact already-loaded native VariableGet class required; no alternate node/module load."));return R; }
    const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R),ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);
    R.Stage=TEXT("create_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_Variable>(G,Class);Count(R,TEXT("node_allocation_returns"));R.Data->SetBoolField(TEXT("node_allocation_returned"),Node!=nullptr);const TStrongObjectPtr<UK2Node_Variable> KeepNode(Node);if(!Node) { Fail(R,TEXT("component_reference_allocation_failed"),TEXT("Failed to create get component node"));return R; }
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());const FName RequestedName(*ComponentName);Count(R,TEXT("self_member_set_calls"));Node->VariableReference.SetSelfMember(RequestedName);Count(R,TEXT("self_member_set_returns"));R.Data->SetObjectField(TEXT("member_reference_after_set"),Reference(Node));Node->NodePosX=XY[0];Node->NodePosY=XY[1];
    UEdGraphNode* Base=Node;Count(R,TEXT("graph_add_node_calls"));G->AddNode(Base);R.Data->SetBoolField(TEXT("graph_add_user_action"),false);Count(R,TEXT("new_guid_calls"));Base->CreateNewGuid();R.Data->SetStringField(TEXT("created_node_guid"),Base->NodeGuid.ToString(EGuidFormats::Digits));Count(R,TEXT("post_placed_calls"));Base->PostPlacedNewNode();Count(R,TEXT("allocate_pins_calls"));Base->AllocateDefaultPins();Count(R,TEXT("reconstruct_calls"));Base->ReconstructNode();
    R.Stage=TEXT("mark_modified");Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Count(R,TEXT("mark_blueprint_modified_returns"));
    Count(R,TEXT("property_resolution_calls"));FProperty* Property=Node->GetPropertyForVariable();Count(R,TEXT("property_resolution_returns"));R.Data->SetObjectField(TEXT("member_reference_current"),Reference(Node));const bool Pins=PinShape(Node,Property,R);R.Data->SetBoolField(TEXT("current_pin_shape_verified"),Pins);
    Function::FResult AfterResult;const bool Observed=Observe(BP,G,TEXT("graph_after_observation"),AfterResult,R);const auto ClassAfter=Source(Class,R),ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);const bool Sources=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter));R.Data->SetBoolField(TEXT("native_source_preserved"),Sources);R.bChanged|=!Sources;
    if(!Observed) { return R; }const auto After=AfterResult.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),After);R.bChanged|=!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After));R.bChangeStateKnown=true;
    const auto* Row=After->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});if(Row) { R.Data->SetObjectField(TEXT("node"),(*Row)->AsObject()); }LegacyNodeFields(Node,R);R.Data->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());R.Data->SetStringField(TEXT("node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));R.Data->SetObjectField(TEXT("graph"),AfterResult.Data->GetObjectField(TEXT("graph")));auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());Legacy->SetObjectField(TEXT("graph"),AfterResult.Data->GetObjectField(TEXT("graph")));R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    const bool Old=ExistingNodesPreserved(Before,After,1),Rows=GraphRowsPreserved(Before,After,G->GetPathName(),1);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);const bool Ref=Node->VariableReference.IsSelfContext()&&!Node->VariableReference.IsLocalScope()&&Node->VariableReference.GetMemberName()==RequestedName;R.Data->SetBoolField(TEXT("current_requested_self_member_verified"),Ref);
    if(!Loaded(Node)||Node->GetClass()!=Class||Node->GetOuter()!=G||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||G->GraphGuid!=GraphGuid||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!Pins||!Ref||!Sources||!Old||!Rows||!Row||!BP->GetOutermost()->IsDirty()) { Fail(R,TEXT("component_reference_postcondition_failed"),TEXT("Current node/reference/pins/position/old graph or native source proof differs; actual node and partial state retained without rollback."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
