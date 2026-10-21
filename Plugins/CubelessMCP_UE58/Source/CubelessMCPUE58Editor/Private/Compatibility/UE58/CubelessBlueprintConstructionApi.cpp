// DE specialized LocalPlayer-subsystem getter and GenericCreateObject authoring only.
#include "Compatibility/UE58/CubelessBlueprintConstructionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"
#include "K2Node_GetSubsystem.h"
#include "K2Node_GenericCreateObject.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "UObject/SavePackage.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintConstruction
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeToken(const FString& S) { if(S.Len()>512) { return false; }for(int32 I=0;I<S.Len();++I) { if(!S[I]) { return false; } }return true; }
void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("class_load_calls"),TEXT("class_load_returns"),TEXT("class_source_observation_calls"),TEXT("class_property_observation_calls"),TEXT("node_class_lookup_calls"),TEXT("node_class_lookup_returns"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("external_scope_observation_calls"),TEXT("node_allocation_calls"),TEXT("graph_modify_calls"),TEXT("set_transactional_flag_calls"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("initialize_subsystem_class_calls"),TEXT("initialize_subsystem_class_returns"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("class_pin_observation_calls"),TEXT("subsystem_class_storage_observation_calls"),TEXT("try_set_default_object_calls"),TEXT("try_set_default_object_returns"),TEXT("default_validation_calls"),TEXT("default_object_match_calls"),TEXT("default_restore_calls"),TEXT("default_restore_returns"),TEXT("pin_default_changed_calls"),TEXT("pin_default_changed_returns"),TEXT("destroy_node_calls"),TEXT("destroy_node_returns"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("legacy_node_metadata_observation_calls"),TEXT("exposed_default_projection_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("target_object_construction_calls"),TEXT("runtime_subsystem_get_calls"),TEXT("explicit_cdo_construction_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Original specialized Engine editor node creation, not target UObject construction or subsystem evaluation. Shared resolver may create a requested graph/skeleton; default setters/reconstruction/MarkModified have implicit callback/cache/status/dirty/child effects. Direct counts do not include SDK internals; no explicit fullcompile/save, replay, atomic rollback or global-zero claim."));
    R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
    R.Data->SetStringField(TEXT("class_source_scope"),TEXT("Class identity/flags/native hierarchy selected metadata and package dirty after requested class load. Arbitrary class CDO values, loader/module/editor cache/child effects are not a full preservation proof. GetDefaultObject(false) observers and SDK exposed-default reader do not explicitly construct the target class CDO; editor node allocation can have its own archetype/CDO work."));
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
bool ActorScope(UBlueprint* BP,UEdGraph* G,const TSharedPtr<FJsonObject>& Graph,FResult& R)
{
    if(!Loaded(BP)||!Loaded(G)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP||BP->ParentClass!=AActor::StaticClass()
        ||BP->BlueprintType!=BPTYPE_Normal||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified
        ||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated||!G->bEditable||!G->GetSchema()
        ||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()
        ||(Graph->GetStringField(TEXT("graph_type"))!=TEXT("function")&&Graph->GetStringField(TEXT("graph_type"))!=TEXT("event"))||G->Nodes.Num()>=2048)
    { return Fail(R,TEXT("value_blueprint_scope_pending"),TEXT("Finite idle direct native Actor Blueprint with an owned editable function/event K2 graph required; wider original owners remain residual.")); }
    Count(R,TEXT("external_scope_observation_calls"));TArray<TSharedPtr<FJsonValue>> External;
    for(TObjectIterator<UBlueprint> It;It;++It)
    { if(*It!=BP&&It->ParentClass&&BP->GeneratedClass&&It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.Add(MakeShared<FJsonValueString>(It->GetPathName())); } }
    if(BP->GeneratedClass)
    { TArray<UObject*> Instances;GetObjectsOfClass(BP->GeneratedClass,Instances,true,RF_NoFlags);for(UObject* O:Instances) { if(IsValid(O)&&O->GetOutermost()!=BP->GetOutermost()) { External.Add(MakeShared<FJsonValueString>(O->GetPathName())); } } }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"),External);
    return External.IsEmpty()||Fail(R,TEXT("value_external_instances_pending"),TEXT("Loaded external instances/derived Blueprints can receive MarkModified effects; finite scope refused."));
}
bool Graph(const FString& Path,const FString& Name,const FString& Id,const FString& Type,bool Create,UBlueprint*& BP,UEdGraph*& G,TSharedPtr<FJsonObject>& Before,FResult& R)
{
    R.Stage=TEXT("resolve_graph");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(Path,Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));
    R.bMutationAttempted=S.bMutationAttempted;R.bChanged=S.bChanged;R.bChangeStateKnown=S.bChangeStateKnown;
    if(!S.bSuccess||!S.bPostconditionPassed) { return Fail(R,S.ErrorCode.IsEmpty()?TEXT("branching_graph_unverified"):*S.ErrorCode,S.Error); }
    BP=FindObject<UBlueprint>(nullptr,*S.Data->GetStringField(TEXT("blueprint")));const auto J=S.Data->GetObjectField(TEXT("graph"));G=FindObject<UEdGraph>(nullptr,*J->GetStringField(TEXT("object_path")));
    if(!ActorScope(BP,G,J,R)) { return false; }Before=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());return true;
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
bool After(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const FGuid& Guid,const TSharedPtr<FJsonObject>& Before,int32 Delta,FResult& R)
{
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(BP->GetPathName(),TEXT(""),Guid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(S));
    if(!S.bSuccess||!S.bChangeStateKnown||!S.bPostconditionPassed) { return Fail(R,TEXT("branching_after_incomplete"),TEXT("Current graph observation did not close; actual mutation/cleanup prefix retained, no retry.")); }
    const auto A=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),A);R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true;
    const auto* Row=A->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(Row) { R.Data->SetObjectField(TEXT("node"),(*Row)->AsObject());for(const auto& P:(*Row)->AsObject()->Values) { R.Data->SetField(P.Key,P.Value); }LegacyNodeFields(Node,R); }
    const bool Old=ExistingNodesPreserved(Before,A,Delta),Rows=GraphRowsPreserved(Before,A,G->GetPathName(),Delta);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);R.Data->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));
    return (Old&&Rows&&G->GraphGuid==Guid&&(Delta==0?!Row:Row!=nullptr))||Fail(R,TEXT("branching_graph_postcondition_failed"),TEXT("Selected node/old graph preservation differs; actual current state retained, no rollback claimed."));
}
bool FreshPins(UEdGraphNode* Node)
{
    TSet<FGuid> Ids;
    for(const auto* P:Node->Pins)
    { if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return false; }Ids.Add(P->PinId); }
    return true;
}
bool Forbidden(const FString& Text)
{
    const FString S=Text.TrimStartAndEnd().Replace(TEXT("\\"),TEXT("/")).ToLower();
    for(const TCHAR* Token:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor")}) { if(S.Contains(Token)) { return true; } }return false;
}
UClass* ResolveClass(const FString& Input,bool PlayerSubsystem,FResult& R)
{
    if(!Function::Pin::SafeName(Input)||Forbidden(Input)) { Fail(R,TEXT("construction_class_invalid_or_dependency"),TEXT("Bounded original class without an MCP-only runtime dependency required."));return nullptr; }
    FString Path=Input;Path.TrimStartAndEndInline();Path.TrimQuotesInline();if(!Path.Contains(TEXT("/"))&&!Path.Contains(TEXT("."))) { Path=TEXT("/Script/Engine.")+Path; }
    if(!Path.StartsWith(TEXT("/Script/Engine."))&&!Path.StartsWith(TEXT("/Script/CoreUObject."))&&!(PlayerSubsystem&&Path.StartsWith(TEXT("/Script/EnhancedInput."))))
    { Fail(R,TEXT("construction_class_scope_pending"),TEXT("Finite native runtime Engine/CoreUObject class (plus existing EnhancedInput LocalPlayer subsystem) selected; original generated/asset/custom-module/slow-find variants remain valid residuals."));return nullptr; }
    Count(R,TEXT("class_load_calls"));auto* Class=LoadObject<UClass>(nullptr,*Path);Count(R,TEXT("class_load_returns"));
    if(!Loaded(Class)||!Class->HasAnyClassFlags(CLASS_Native)||Class->ClassGeneratedBy||Class->IsEditorOnly()||Class->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists))
    { Fail(R,TEXT("construction_class_not_found_or_scope"),TEXT("Loaded exact native runtime class required; class load prefix retained."));return nullptr; }
    const FString Package=Class->GetOutermost()->GetName();
    if(!Package.Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)&&!Package.Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)&&!(PlayerSubsystem&&Package.Equals(TEXT("/Script/EnhancedInput"),ESearchCase::CaseSensitive)))
    { Fail(R,TEXT("construction_class_scope_pending"),TEXT("Redirected class is outside selected runtime modules."));return nullptr; }
    if(PlayerSubsystem&&!Class->IsChildOf(ULocalPlayerSubsystem::StaticClass()))
    { Fail(R,TEXT("construction_not_local_player_subsystem"),TEXT("Subsystem class is not a LocalPlayerSubsystem: ")+Class->GetPathName());return nullptr; }
    return Class;
}
TSharedRef<FJsonObject> ClassSource(UClass* Class,FResult& R)
{
    Count(R,TEXT("class_source_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Class->GetPathName());J->SetStringField(TEXT("object_class"),Class->GetClass()->GetPathName());J->SetStringField(TEXT("flags"),LexToString(static_cast<uint32>(Class->GetClassFlags())));J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(Class->GetFlags())));J->SetBoolField(TEXT("package_dirty"),Class->GetOutermost()->IsDirty());
    TArray<TSharedPtr<FJsonValue>> Hierarchy;int32 Depth=0;
    for(UClass* C=Class;C&&Depth<64;C=C->GetSuperClass(),++Depth)
    { auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),C->GetPathName());Row->SetStringField(TEXT("flags"),LexToString(static_cast<uint32>(C->GetClassFlags())));for(const TCHAR* Key:{TEXT("BlueprintType"),TEXT("NotBlueprintType"),TEXT("DontUseGenericSpawnObject")}) { Row->SetBoolField(Key,C->GetBoolMetaData(Key)); }Hierarchy.Add(MakeShared<FJsonValueObject>(Row)); }
    J->SetArrayField(TEXT("hierarchy"),Hierarchy);return J;
}
bool ConstructClass(UClass* Class,FResult& R)
{
    // Public fields/metadata adaptation of compiler eligibility evidence only.
    // Private FK2Node_GenericCreateObject_Utils is never called.
    if(Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Class->IsChildOf(AActor::StaticClass())||Class->IsChildOf(UActorComponent::StaticClass()))
    { return Fail(R,TEXT("construction_object_class_pending"),TEXT("Finite nonabstract runtime BlueprintType UObject class, outside Actor/Component hierarchy, required; wider original node-success/compiler-failure variants remain tracked.")); }
    const UObject* CDO=Class->GetDefaultObject(false);if(CDO&&!Loaded(CDO)) { return Fail(R,TEXT("construction_object_cdo_pending"),TEXT("Existing class CDO is not ready for the original exposed-default reader; no forced finish or CDO construction.")); }
    bool Allowed=false;int32 Depth=0;
    for(UClass* C=Class;C&&Depth<64;C=C->GetSuperClass(),++Depth)
    { if(C->GetBoolMetaData(TEXT("NotBlueprintType"))) { break; }if(C->GetBoolMetaData(TEXT("BlueprintType"))) { Allowed=!C->GetBoolMetaData(TEXT("DontUseGenericSpawnObject"));break; } }
    return Allowed||Fail(R,TEXT("construction_object_class_pending"),TEXT("Current native class hierarchy does not satisfy the bounded public metadata construction admission."));
}
struct FExposed { TMap<FName,FProperty*> Properties;TMap<FName,FEdGraphPinType> Types; };
bool Exposed(UClass* Class,const UEdGraphSchema_K2* Schema,FExposed& Out,FResult& R)
{
    TArray<TSharedPtr<FJsonValue>> Rows;R.Data->SetArrayField(TEXT("exposed_property_layout"),Rows);int32 CountProperties=0;
    for(TFieldIterator<FProperty> It(Class,EFieldIteratorFlags::IncludeSuper);It;++It)
    {
        if(++CountProperties>2048) { return Fail(R,TEXT("construction_property_scope_pending"),TEXT("Native class property budget exceeded before node writer.")); }Count(R,TEXT("class_property_observation_calls"));FProperty* P=*It;
        const FName Name=P->GetFName();const bool Reserved=Name==UEdGraphSchema_K2::PN_Execute||Name==UEdGraphSchema_K2::PN_Then||Name==UEdGraphSchema_K2::PN_ReturnValue||Name==TEXT("Class")||Name==TEXT("Outer");
        if(!UEdGraphSchema_K2::IsPropertyExposedOnSpawn(P)||P->HasAnyPropertyFlags(CPF_Parm|CPF_DisableEditOnInstance)||!P->HasAllPropertyFlags(CPF_BlueprintVisible)||CastField<FMulticastDelegateProperty>(P)||Reserved||!FBlueprintEditorUtils::PropertyStillExists(P)) { continue; }
        FEdGraphPinType Type;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),P->GetName());Row->SetStringField(TEXT("owner"),P->GetOwnerStruct()->GetPathName());Row->SetStringField(TEXT("property_flags"),LexToString(static_cast<uint64>(P->GetPropertyFlags())));Rows.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("exposed_property_layout"),Rows);
        if(P->ArrayDim!=1||P->HasAnyPropertyFlags(CPF_EditorOnly)||!Schema->ConvertPropertyToPinType(P,Type)||Type.IsContainer()||Type.bIsReference||Type.bIsWeakPointer||(!Function::Pin::IsPrimitive(Type)&&!Function::Pin::IsScalarValueStruct(Type))||Out.Properties.Num()>=32)
        { return Fail(R,TEXT("construction_property_scope_pending"),TEXT("Selected exposed properties require bounded native scalar/value-struct layout; wider exposed object/container/editor layouts remain residual.")); }
        Row->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(Type));Out.Properties.Add(Name,P);Out.Types.Add(Name,Type);
    }
    R.Data->SetNumberField(TEXT("exposed_property_count"),Out.Properties.Num());return true;
}
void AddAndAllocate(UEdGraph* G,UEdGraphNode* Node,const int32 (&XY)[2],UK2Node_GetSubsystem* Subsystem,UClass* Class,FResult& R)
{
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());Node->NodePosX=XY[0];Node->NodePosY=XY[1];Count(R,TEXT("graph_add_node_calls"));G->AddNode(Node,true);Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();R.Data->SetStringField(TEXT("created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));
    if(Subsystem) { Count(R,TEXT("initialize_subsystem_class_calls"));Subsystem->Initialize(Class);Count(R,TEXT("initialize_subsystem_class_returns")); }
    Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();
    if(Subsystem) { Count(R,TEXT("reconstruct_calls"));Node->ReconstructNode(); }
}
TSharedRef<FJsonObject> DefaultRow(const UEdGraphPin* Pin)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("value"),Pin->DefaultValue);J->SetStringField(TEXT("object"),GetPathNameSafe(Pin->DefaultObject));J->SetStringField(TEXT("text"),Pin->DefaultTextValue.ToString());return J;
}
bool ApplyClassDefault(UK2Node_ConstructObjectFromClass* Node,UClass* Original,const UEdGraphSchema_K2* Schema,FResult& R)
{
    Count(R,TEXT("class_pin_observation_calls"));auto* Pin=Node->GetClassPin();R.Data->SetBoolField(TEXT("class_pin_found"),Pin!=nullptr);
    if(!Pin) { return Fail(R,TEXT("construction_class_pin_missing"),TEXT("Construct Object node has no class pin.")); }
    const FString PreviousValue=Pin->DefaultValue;UObject* PreviousObject=Pin->DefaultObject;const TStrongObjectPtr<UObject> KeepPrevious(PreviousObject);const FText PreviousText=Pin->DefaultTextValue;R.Data->SetObjectField(TEXT("class_default_before"),DefaultRow(Pin));
    // Preserve original checked helper's second class resolution before setter.
    auto* Requested=ResolveClass(Original->GetPathName(),false,R);if(!Requested) { return false; }
    if(Requested!=Original) { return Fail(R,TEXT("construction_class_reload_mismatch"),TEXT("Second original class resolver returned a different identity before setter.")); }
    const TStrongObjectPtr<UClass> KeepRequested(Requested);Count(R,TEXT("try_set_default_object_calls"));Schema->TrySetDefaultObject(*Pin,Requested);Count(R,TEXT("try_set_default_object_returns"));R.Data->SetStringField(TEXT("class_default_applied"),Requested->GetPathName());
    if(!Node->Pins.Contains(Pin)||Pin->WasTrashed()) { return Fail(R,TEXT("construction_class_pin_lifetime_incomplete"),TEXT("Class callback unexpectedly replaced the original Class pin; setter returned prefix retained without unsafe restore.")); }
    R.Data->SetObjectField(TEXT("class_default_after_setter"),DefaultRow(Pin));Count(R,TEXT("default_validation_calls"));const FString Error=Schema->IsCurrentPinDefaultValid(Pin);Count(R,TEXT("default_object_match_calls"));const bool Match=Pin->DefaultObject==Requested;R.Data->SetStringField(TEXT("class_default_validation_error"),Error);R.Data->SetBoolField(TEXT("class_default_object_match"),Match);
    if(!Error.IsEmpty()||!Match)
    {
        Count(R,TEXT("default_restore_calls"));Pin->DefaultValue=PreviousValue;Pin->DefaultObject=PreviousObject;Pin->DefaultTextValue=PreviousText;Count(R,TEXT("default_restore_returns"));R.Data->SetObjectField(TEXT("class_default_after_restore"),DefaultRow(Pin));
        return Fail(R,TEXT("construction_class_default_invalid"),Error.IsEmpty()?TEXT("Requested object class default was not accepted."):Error);
    }
    Count(R,TEXT("pin_default_changed_calls"));static_cast<UEdGraphNode*>(Node)->PinDefaultValueChanged(Pin);Count(R,TEXT("pin_default_changed_returns"));Count(R,TEXT("reconstruct_calls"));static_cast<UEdGraphNode*>(Node)->ReconstructNode();return true;
}
bool ExposedPins(UK2Node_ConstructObjectFromClass* Node,UClass* Class,const FExposed& Layout,const UEdGraphSchema_K2* Schema,FResult& R)
{
    TArray<TSharedPtr<FJsonValue>> Rows;R.Data->SetArrayField(TEXT("exposed_default_observations"),Rows);const auto* CDO=Class->GetDefaultObject(false);R.Data->SetBoolField(TEXT("source_class_cdo_exists_current"),CDO!=nullptr);if(CDO&&!Loaded(CDO)) { return Fail(R,TEXT("construction_object_cdo_pending"),TEXT("Existing CDO readiness changed after writer; exposed defaults remain unverified, no forced completion.")); }
    for(const auto& Pair:Layout.Properties)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Pair.Key.ToString());Rows.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("exposed_default_observations"),Rows);const auto* Pin=Node->FindPin(Pair.Key,EGPD_Input);
        if(!Pin||Pin->PinType!=Layout.Types.FindChecked(Pair.Key)||Pin->bHidden||Pin->bNotConnectable||!Schema->IsCurrentPinDefaultValid(Pin).IsEmpty()) { return Fail(R,TEXT("construction_exposed_pin_unverified"),TEXT("Current exposed-property pin/type/default validity differs.")); }
        Row->SetStringField(TEXT("actual_default"),Pin->GetDefaultAsString());Row->SetStringField(TEXT("autogenerated_default"),Pin->AutogeneratedDefaultValue);
        if(CDO&&Schema->PinDefaultValueIsEditable(*Pin))
        {
            FString Expected;Count(R,TEXT("exposed_default_projection_calls"));const bool Projected=FBlueprintEditorUtils::PropertyValueToString(Pair.Value,reinterpret_cast<const uint8*>(CDO),Expected,Node);Row->SetBoolField(TEXT("projection_returned"),Projected);Row->SetStringField(TEXT("source_default"),Expected);
            const bool Match=Projected&&Schema->DoesDefaultValueMatch(*Pin,Expected);Row->SetBoolField(TEXT("source_default_match"),Match);if(!Match) { return Fail(R,TEXT("construction_exposed_default_unverified"),TEXT("Current exposed default differs from the existing class CDO public projection.")); }
        }
    }
    return true;
}
// PZ: read-only generated UObject source and concrete construct-node lifetime.
// The native/subsystem code below remains the original DE route.
struct FGeneratedConstruct
{
    FResult& R;bool Stopped=false,Member=false,Placed=false,Retired=false,Replacing=false,PinsFixed=false,ClassAssigned=false;
    TStrongObjectPtr<UBlueprint> Source,Target;TStrongObjectPtr<UPackage> SourcePackage,TargetPackage;
    TStrongObjectPtr<UClass> Generated,Skeleton,Parent,TargetGenerated,TargetSkeleton;
    TStrongObjectPtr<UObject> CDO;TStrongObjectPtr<UEdGraphSchema_K2> Schema;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UK2Node_GenericCreateObject> Node;
    TArray<TStrongObjectPtr<UObject>> Holds;FString SourcePath,TargetPath,GeneratedPath,SkeletonPath,CDOPath,Filename,FileHash,NodePath;
    UObject* CDOOuter=nullptr;EObjectFlags CDOFlags=RF_NoFlags,GeneratedFlags=RF_NoFlags,SkeletonFlags=RF_NoFlags,SourceFlags=RF_NoFlags;EClassFlags GeneratedClassFlags=CLASS_None,SkeletonClassFlags=CLASS_None;FGuid NodeGuid;int32 X=0,Y=0;
    TArray<FBPVariableDescription> Variables,TargetVariables;TArray<UEdGraph*> SourceGraphs,TargetGraphs;
    TArray<TObjectPtr<UEdGraph>> SourceFunctions,SourceMacros,SourceEvents,SourceDelegates,TargetFunctions,TargetMacros,TargetEvents,TargetDelegates;
    struct FField{FProperty* Address;UClass* Owner;FFieldClass* Class;FName Name;uint64 Flags;int32 Dim,Size,Offset;FEdGraphPinType Type;bool Exposed=false;bool CompilerFrame=false;UScriptStruct* Struct=nullptr;};
    TArray<FField> Fields,SkeletonFields;
    TStrongObjectPtr<UScriptStruct> FrameStruct;TStrongObjectPtr<UObject> FrameStructOuter;UClass* FrameStructClass=nullptr;
    EObjectFlags FrameStructObjectFlags=RF_NoFlags;EStructFlags FrameStructFlags=STRUCT_NoFlags;int32 FrameStructSize=0,FrameStructAlignment=0;
    TSharedPtr<FJsonObject> FieldDiagnostic;
    struct FCompilerRole{bool Bound=false;TStrongObjectPtr<UFunction> Function;FStructProperty* PublicFrame=nullptr;UClass* FunctionClass=nullptr;EObjectFlags Flags=RF_NoFlags;FString Path;};
    FCompilerRole GeneratedCompiler,SkeletonCompiler;
    struct FPin{UEdGraphPin* Address;FGuid Guid;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;FString Raw,Auto;FText Text,Friendly;uint32 TextFlags,FriendlyFlags,Flags;UObject* Default;TArray<UEdGraphPin*> Links;};
    struct FNode{TStrongObjectPtr<UEdGraphNode> Hold;UEdGraph* Graph;UClass* Class;FGuid Guid;FString Path,Comment;int32 X,Y;TArray<FPin> Pins;};
    struct FGraph{TStrongObjectPtr<UEdGraph> Hold;UBlueprint* BP;UClass* Class;UClass* Schema;FGuid Guid;FString Path;TArray<UEdGraphNode*> Nodes;};
    TArray<FNode> OldNodes;TArray<FGraph> Graphs;TArray<FPin> NodePins,Candidates;
    TSharedPtr<FJsonObject> SourceBefore;UEdGraph* Selected=nullptr;FGuid GraphGuid;
    explicit FGeneratedConstruct(FResult& In):R(In),Editor(GEditor),World(GWorld)
    {
        FieldDiagnostic=MakeShared<FJsonObject>();FieldDiagnostic->SetBoolField(TEXT("source_bind_completed"),false);
        FieldDiagnostic->SetBoolField(TEXT("generated_observed"),false);FieldDiagnostic->SetBoolField(TEXT("skeleton_observed"),false);
        FieldDiagnostic->SetField(TEXT("generated_physical_count_observed"),MakeShared<FJsonValueNull>());FieldDiagnostic->SetField(TEXT("skeleton_physical_count_observed"),MakeShared<FJsonValueNull>());
        FieldDiagnostic->SetField(TEXT("authored_variable_count"),MakeShared<FJsonValueNull>());
        FieldDiagnostic->SetArrayField(TEXT("generated_fields"),TArray<TSharedPtr<FJsonValue>>());FieldDiagnostic->SetArrayField(TEXT("skeleton_fields"),TArray<TSharedPtr<FJsonValue>>());
        for(const TCHAR* Role:{TEXT("generated"),TEXT("skeleton")})
        {
            FieldDiagnostic->SetBoolField(FString(Role)+TEXT("_ubergraph_function_observed"),false);
            FieldDiagnostic->SetField(FString(Role)+TEXT("_ubergraph_function_resolved"),MakeShared<FJsonValueNull>());
            FieldDiagnostic->SetField(FString(Role)+TEXT("_ubergraph_function_path"),MakeShared<FJsonValueNull>());
        }
        R.Data->SetObjectField(TEXT("generated_source_field_inventory"),FieldDiagnostic);
    }
    bool Bad(const TCHAR* Why)
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("reason"),Why);J->SetBoolField(TEXT("individual_operands_observed"),false);R.Data->SetObjectField(TEXT("generated_construct_first_current_failure"),J);}
        Stopped=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("generated_construct_followup_sdk_stopped"),true);
        if(R.ErrorCode.IsEmpty())Fail(R,TEXT("construction_generated_current_changed"),Why);return false;
    }
    bool Context()const{return IsEditorGameThread()&&!IsPlaySessionActive()&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage()&&GEditor==Editor.Get()&&GWorld==World.Get()&&Loaded(Editor.Get());}
    static bool SameType(const FEdGraphPinType& A,const FEdGraphPinType& B){return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    static uint32 PinFlags(const UEdGraphPin* P){return uint32(P->bHidden)|(uint32(P->bOrphanedPin)<<1)|(uint32(P->bNotConnectable)<<2)|(uint32(P->bDefaultValueIsReadOnly)<<3)|(uint32(P->bDefaultValueIsIgnored)<<4)|(uint32(P->bAdvancedView)<<5);}
    bool CapturePins(UEdGraphNode* N,TArray<FPin>& Out)
    {
        if(!Loaded(N)||N->Pins.Num()>512)return Bad(TEXT("pin_inventory"));TSet<FGuid> Ids;
        for(UEdGraphPin* P:N->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||!P->DefaultObject.IsResolved())return Bad(TEXT("pin_role"));Ids.Add(P->PinId);
            UObject* D=P->DefaultObject.Get();if(D)Holds.Emplace(D);
            Out.Add({P,P->PinId,P->PinName,P->Direction,P->PinType,P->DefaultValue,P->AutogeneratedDefaultValue,P->DefaultTextValue,P->PinFriendlyName,FTextInspector::GetFlags(P->DefaultTextValue),FTextInspector::GetFlags(P->PinFriendlyName),PinFlags(P),D,P->LinkedTo});
        }return true;
    }
    bool PinsCurrent(UEdGraphNode* N,const TArray<FPin>& Pins)
    {
        if(N->Pins.Num()!=Pins.Num())return Bad(TEXT("pin_count"));
        for(int32 I=0;I<Pins.Num();++I){const auto& E=Pins[I];const auto* P=N->Pins[I];if(P!=E.Address||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=E.Guid||P->PinName!=E.Name||P->Direction!=E.Direction||!SameType(P->PinType,E.Type)||!P->DefaultValue.Equals(E.Raw,ESearchCase::CaseSensitive)||!P->AutogeneratedDefaultValue.Equals(E.Auto,ESearchCase::CaseSensitive)||!P->DefaultObject.IsResolved()||P->DefaultObject.Get()!=E.Default||P->LinkedTo!=E.Links||PinFlags(P)!=E.Flags||FTextInspector::GetFlags(P->DefaultTextValue)!=E.TextFlags||FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(E.Text)||FTextInspector::GetFlags(P->PinFriendlyName)!=E.FriendlyFlags||FTextInspector::GetSharedDataId(P->PinFriendlyName)!=FTextInspector::GetSharedDataId(E.Friendly))return Bad(TEXT("pin_current"));}return true;
    }
    bool BindGraphs(UBlueprint* BP,TArray<UEdGraph*>& Out)
    {
        BP->GetAllGraphs(Out);if(Out.Num()>256)return Bad(TEXT("graph_budget"));
        for(UEdGraph* G:Out)
        {
            if(!Loaded(G)||G->GetOuter()!=BP||!G->GraphGuid.IsValid()||G->Nodes.Num()>2048||!G->Schema.GetGCPtr().IsResolved())return Bad(TEXT("graph_inventory"));
            FGraph E;E.Hold.Reset(G);E.BP=BP;E.Class=G->GetClass();E.Schema=G->Schema.Get();E.Guid=G->GraphGuid;E.Path=G->GetPathName();if(E.Schema)Holds.Emplace(E.Schema);
            for(const auto& H:G->Nodes){if(!H.IsResolved())return Bad(TEXT("node_handle"));auto* N=H.Get();if(!Loaded(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid())return Bad(TEXT("node_inventory"));FNode P;P.Hold.Reset(N);P.Graph=G;P.Class=N->GetClass();P.Guid=N->NodeGuid;P.Path=N->GetPathName();P.Comment=N->NodeComment;P.X=N->NodePosX;P.Y=N->NodePosY;if(!CapturePins(N,P.Pins))return false;E.Nodes.Add(N);OldNodes.Add(MoveTemp(P));}Graphs.Add(MoveTemp(E));
        }return true;
    }
    int32 UserFieldCount(const TArray<FField>& List)const
    {int32 Count=0;for(const auto& E:List)if(!E.CompilerFrame)++Count;return Count;}
    bool FieldMismatch(const TCHAR* Role,int32 Index,const TCHAR* Operand,const FString& Expected,const FString& Actual,bool Unknown=false)
    {
        if(!R.Data->HasField(TEXT("generated_source_first_field_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("role"),Role);J->SetNumberField(TEXT("index"),Index);
            J->SetStringField(TEXT("operand"),Operand);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);
            J->SetBoolField(TEXT("operands_observed"),true);R.Data->SetObjectField(TEXT("generated_source_first_field_mismatch"),J);
        }
        if(Unknown)return Bad(TEXT("source_field_current_mismatch"));
        return Fail(R,TEXT("construction_generated_source_fields_pending"),TEXT("Observed direct source field inventory/layout does not match authored scalars plus the exact SDK compiler-frame role; inspect copied operands."));
    }
    bool FrameStructCurrent()const
    {
        return Loaded(FrameStruct.Get())&&FrameStruct->GetClass()==FrameStructClass&&FrameStruct->GetOuter()==FrameStructOuter.Get()
            &&FrameStruct->GetPathName().Equals(TEXT("/Script/Engine.PointerToUberGraphFrame"),ESearchCase::CaseSensitive)
            &&FrameStruct->GetFlags()==FrameStructObjectFlags&&FrameStruct->StructFlags==FrameStructFlags
            &&FrameStruct->GetStructureSize()==FrameStructSize&&FrameStruct->GetMinAlignment()==FrameStructAlignment;
    }
    bool CompilerRoleCurrent(UClass* C)
    {
        if((C!=Generated.Get()&&C!=Skeleton.Get())||!Loaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass())return Bad(TEXT("compiler_role_class_current"));
        const auto& E=C==Generated.Get()?GeneratedCompiler:SkeletonCompiler;
        const auto* BGC=static_cast<UBlueprintGeneratedClass*>(C);
        if(!E.Bound||!BGC->UberGraphFunction.IsResolved()||BGC->UberGraphFunction.Get()!=E.Function.Get()||BGC->UberGraphFramePointerProperty!=E.PublicFrame)return Bad(TEXT("compiler_function_or_public_frame_slot_current"));
        UFunction* F=E.Function.Get();
        if(F&&(!Loaded(F)||F->GetClass()!=E.FunctionClass||F->GetOuter()!=C||F->GetFlags()!=E.Flags||!F->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)))return Bad(TEXT("compiler_function_role_current"));
        return true;
    }
    bool FieldsCurrent(UClass* C,const TArray<FField>& Expected)
    {
        const TCHAR* Role=C==Generated.Get()?TEXT("generated"):TEXT("skeleton");
        TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It){if(Now.Num()>=33)return FieldMismatch(Role,Now.Num(),TEXT("physical_count_upper_bound"),TEXT("33"),TEXT("at least 34"),true);Now.Add(*It);}
        if(Now.Num()!=Expected.Num())return FieldMismatch(Role,INDEX_NONE,TEXT("physical_count"),LexToString(Expected.Num()),LexToString(Now.Num()),true);
        if(!CompilerRoleCurrent(C))return false;
        const auto& Compiler=C==Generated.Get()?GeneratedCompiler:SkeletonCompiler;
        const FProperty* ActualFrame=static_cast<UBlueprintGeneratedClass*>(C)->UberGraphFramePointerProperty;
        for(int32 I=0;I<Expected.Num();++I)
        {
            const auto& E=Expected[I];if(Now[I]!=E.Address)return FieldMismatch(Role,I,TEXT("physical_address"),LexToString(reinterpret_cast<UPTRINT>(E.Address)),LexToString(reinterpret_cast<UPTRINT>(Now[I])),true);
            auto* P=E.Address;
            if(P->GetOwnerStruct()!=E.Owner||P->GetOwnerProperty()!=P||P->GetClass()!=E.Class||P->GetFName()!=E.Name||uint64(P->GetPropertyFlags())!=E.Flags||P->ArrayDim!=E.Dim||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->HasSetterOrGetter())return Bad(TEXT("source_field_layout"));
            if(E.Struct){auto* S=CastField<FStructProperty>(P);if(!S||!S->Struct.IsResolved()||S->Struct.Get()!=E.Struct)return FieldMismatch(Role,I,TEXT("struct_binding"),LexToString(reinterpret_cast<UPTRINT>(E.Struct)),S&&S->Struct.IsResolved()?LexToString(reinterpret_cast<UPTRINT>(S->Struct.Get())):TEXT("unresolved_or_other_field"),true);}
            if(E.CompilerFrame&&(!FrameStructCurrent()||E.Struct!=FrameStruct.Get()||!(ActualFrame==P||(!Compiler.Function.IsValid()&&!ActualFrame))))return Bad(TEXT("compiler_frame_role_current"));
        }
        return true;
    }
    bool VariablesCurrent(const TArray<FBPVariableDescription>& A,const TArray<FBPVariableDescription>& B)
    {
        if(A.Num()!=B.Num())return Bad(TEXT("variable_inventory"));for(int32 I=0;I<B.Num();++I){const auto& ActualVariable=A[I];const auto& E=B[I];if(ActualVariable.VarGuid!=E.VarGuid||ActualVariable.VarName!=E.VarName||!SameType(ActualVariable.VarType,E.VarType)||ActualVariable.PropertyFlags!=E.PropertyFlags||!ActualVariable.DefaultValue.Equals(E.DefaultValue,ESearchCase::CaseSensitive)||!ActualVariable.FriendlyName.Equals(E.FriendlyName,ESearchCase::CaseSensitive)||ActualVariable.RepNotifyFunc!=E.RepNotifyFunc||ActualVariable.ReplicationCondition!=E.ReplicationCondition||FTextInspector::GetSharedDataId(ActualVariable.Category)!=FTextInspector::GetSharedDataId(E.Category)||FTextInspector::GetFlags(ActualVariable.Category)!=FTextInspector::GetFlags(E.Category)||ActualVariable.MetaDataArray.Num()!=E.MetaDataArray.Num())return Bad(TEXT("variable_declaration"));for(int32 J=0;J<E.MetaDataArray.Num();++J)if(ActualVariable.MetaDataArray[J].DataKey!=E.MetaDataArray[J].DataKey||!ActualVariable.MetaDataArray[J].DataValue.Equals(E.MetaDataArray[J].DataValue,ESearchCase::CaseSensitive))return Bad(TEXT("variable_metadata"));}return true;
    }
    bool Plain()
    {
        if(Stopped)return false;if(!Context())return Bad(TEXT("editor_context"));
        if(Source.IsValid())
        {
            auto* B=Source.Get();if(!Loaded(B)||B->GetFlags()!=SourceFlags||B->GetClass()!=UBlueprint::StaticClass()||B->GetOuter()!=SourcePackage.Get()||B->GetPackage()!=SourcePackage.Get()||SourcePackage->IsDirty()||B->Status!=BS_UpToDate||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->BlueprintType!=BPTYPE_Normal||!B->GetPathName().Equals(SourcePath,ESearchCase::CaseSensitive)||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=Parent.Get()||B->SimpleConstructionScript||!B->ImplementedInterfaces.IsEmpty()||!VariablesCurrent(B->NewVariables,Variables))return Bad(TEXT("source_blueprint_current"));
            if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=Generated.Get()||B->SkeletonGeneratedClass.GetGCPtr().Get()!=Skeleton.Get())return Bad(TEXT("source_class_slots"));
            for(UClass* C:{Generated.Get(),Skeleton.Get()})if(!Loaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=B||C->GetOuter()!=SourcePackage.Get()||C->GetSuperClass()!=Parent.Get())return Bad(TEXT("source_class_current"));
            if(Generated->GetFlags()!=GeneratedFlags||Skeleton->GetFlags()!=SkeletonFlags||Generated->GetClassFlags()!=GeneratedClassFlags||Skeleton->GetClassFlags()!=SkeletonClassFlags||!Generated->GetPathName().Equals(GeneratedPath,ESearchCase::CaseSensitive)||!Skeleton->GetPathName().Equals(SkeletonPath,ESearchCase::CaseSensitive)||!Generated->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject.Get()!=CDO.Get())return Bad(TEXT("source_class_path_or_cdo"));
            if(CDO.IsValid()&&(!Loaded(CDO.Get())||CDO->GetClass()!=Generated.Get()||CDO->GetOuter()!=CDOOuter||CDO->GetFlags()!=CDOFlags||!CDO->GetPathName().Equals(CDOPath,ESearchCase::CaseSensitive)))return Bad(TEXT("source_cdo_current"));
            if(B->FunctionGraphs!=SourceFunctions||B->MacroGraphs!=SourceMacros||B->UbergraphPages!=SourceEvents||B->DelegateSignatureGraphs!=SourceDelegates||!FieldsCurrent(Generated.Get(),Fields)||!FieldsCurrent(Skeleton.Get(),SkeletonFields))return Bad(TEXT("source_roles"));TArray<UEdGraph*> All;B->GetAllGraphs(All);if(All!=SourceGraphs)return Bad(TEXT("source_graph_inventory"));
        }
        if(Target.IsValid())
        {
            auto* B=Target.Get();if(!Loaded(B)||B->GetPackage()!=TargetPackage.Get()||!B->GetPathName().Equals(TargetPath,ESearchCase::CaseSensitive)||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=AActor::StaticClass()||B->bBeingCompiled||B->bQueuedForCompilation||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=TargetGenerated.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->SkeletonGeneratedClass.GetGCPtr().Get()!=TargetSkeleton.Get()||B->FunctionGraphs!=TargetFunctions||B->MacroGraphs!=TargetMacros||B->UbergraphPages!=TargetEvents||B->DelegateSignatureGraphs!=TargetDelegates||!VariablesCurrent(B->NewVariables,TargetVariables))return Bad(TEXT("target_current"));TArray<UEdGraph*> All;B->GetAllGraphs(All);if(All!=TargetGraphs)return Bad(TEXT("target_graph_inventory"));
        }
        for(const auto& E:Graphs){auto* G=E.Hold.Get();if(!Loaded(G)||G->GetOuter()!=E.BP||G->GetClass()!=E.Class||G->GraphGuid!=E.Guid||!G->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr().Get()!=E.Schema||G->Nodes.Num()!=E.Nodes.Num()+((Member&&!Retired&&G==Selected)?1:0))return Bad(TEXT("old_graph_current"));for(int32 I=0;I<E.Nodes.Num();++I)if(!G->Nodes[I].IsResolved()||G->Nodes[I].Get()!=E.Nodes[I])return Bad(TEXT("old_node_order"));if(Member&&!Retired&&G==Selected&&(!G->Nodes.Last().IsResolved()||G->Nodes.Last().Get()!=Node.Get()))return Bad(TEXT("new_node_membership"));}
        for(const auto& E:OldNodes){auto* N=E.Hold.Get();if(!Loaded(N)||N->GetOuter()!=E.Graph||N->GetClass()!=E.Class||N->NodeGuid!=E.Guid||N->NodePosX!=E.X||N->NodePosY!=E.Y||!N->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||!N->NodeComment.Equals(E.Comment,ESearchCase::CaseSensitive)||!PinsCurrent(N,E.Pins))return Bad(TEXT("old_node_current"));}
        if(Retired){if(Selected&&Selected->Nodes.Contains(Node.Get()))return Bad(TEXT("retired_node_membership"));return true;}
        if(Node.IsValid()){if(!Loaded(Node.Get())||Node->GetOuter()!=Selected||Node->GetClass()!=UK2Node_GenericCreateObject::StaticClass()||!Node->GetPathName().Equals(NodePath,ESearchCase::CaseSensitive)||(Placed&&(Node->NodeGuid!=NodeGuid||Node->NodePosX!=X||Node->NodePosY!=Y)))return Bad(TEXT("construct_node_current"));if(PinsFixed&&!Replacing&&!PinsCurrent(Node.Get(),NodePins))return false;if(Replacing&&!Candidates.IsEmpty()&&!PinsCurrent(Node.Get(),Candidates))return false;}
        return true;
    }
    bool ReadFileHash(FString& Out)
    {
        if(!Plain())return false;TArray<uint8> Bytes;const int64 Size=IFileManager::Get().FileSize(*Filename);if(!Plain())return false;if(Size<0||Size>64*1024*1024)return Bad(TEXT("source_saved_file_unavailable"));
        const bool Read=FFileHelper::LoadFileToArray(Bytes,*Filename);if(!Plain()||!Read||Bytes.Num()!=Size)return Bad(TEXT("source_file_read"));FSHAHash Hash;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash.Hash);Out=Hash.ToString();return Plain();
    }
    bool BindFields(UClass* C,TArray<FField>& Out,const TCHAR* Role)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        FieldDiagnostic->SetBoolField(FString(Role)+TEXT("_observed"),true);
        for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It)
        {
            FProperty* P=*It;auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),Rows.Num());
            J->SetStringField(TEXT("name"),P->GetName());J->SetStringField(TEXT("field_class"),P->GetClass()->GetName());
            J->SetStringField(TEXT("owner"),GetPathNameSafe(P->GetOwnerStruct()));J->SetStringField(TEXT("flags"),LexToString(uint64(P->GetPropertyFlags())));
            J->SetNumberField(TEXT("array_dim"),P->ArrayDim);J->SetNumberField(TEXT("size"),P->GetSize());J->SetNumberField(TEXT("offset"),P->GetOffset_ForInternal());
            UScriptStruct* Struct=nullptr;bool Resolved=true;
            if(auto* S=CastField<FStructProperty>(P))
            {
                Resolved=S->Struct.IsResolved();if(Resolved){Struct=S->Struct.Get();if(Struct)Holds.Emplace(Struct);}
                J->SetBoolField(TEXT("struct_resolved"),Resolved);J->SetStringField(TEXT("struct_path"),Resolved&&Loaded(Struct)?Struct->GetPathName():TEXT("unresolved_or_null_unread"));
            }
            J->SetStringField(TEXT("role"),TEXT("unclassified"));Rows.Add(MakeShared<FJsonValueObject>(J));
            FieldDiagnostic->SetArrayField(FString(Role)+TEXT("_fields"),Rows);FieldDiagnostic->SetNumberField(FString(Role)+TEXT("_physical_count_observed"),Rows.Num());
            if(Out.Num()>=33){FieldDiagnostic->SetBoolField(FString(Role)+TEXT("_inventory_complete"),false);return FieldMismatch(Role,Rows.Num()-1,TEXT("physical_count_upper_bound"),TEXT("33"),TEXT("at least 34"));}
            if(!Context())return Bad(TEXT("source_field_bind_context"));
            if(P->GetOwnerStruct()!=C||P->GetOwnerProperty()!=P||P->ArrayDim!=1||P->HasSetterOrGetter())return FieldMismatch(Role,Rows.Num()-1,TEXT("direct_scalar_field_layout"),C->GetPathName()+TEXT("/dim1/no_accessor"),P->GetPathName());
            if(!Resolved|| (CastField<FStructProperty>(P)&&!Loaded(Struct)))return FieldMismatch(Role,Rows.Num()-1,TEXT("struct_binding"),TEXT("already resolved loaded struct"),TEXT("unresolved_or_null"));
            Out.Add({P,C,P->GetClass(),P->GetFName(),uint64(P->GetPropertyFlags()),P->ArrayDim,P->GetSize(),P->GetOffset_ForInternal(),FEdGraphPinType(),false,false,Struct});
        }
        FieldDiagnostic->SetBoolField(FString(Role)+TEXT("_inventory_complete"),true);return true;
    }
    bool ClassifyCompilerFields(UClass* C,TArray<FField>& List,const TCHAR* Role)
    {
        if(!Loaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=Source.Get()||C->GetOuter()!=SourcePackage.Get()||C->GetSuperClass()!=Parent.Get())
            return FieldMismatch(Role,INDEX_NONE,TEXT("class_role"),TEXT("exact owned BlueprintGeneratedClass/direct UObject parent"),Loaded(C)?C->GetClass()->GetPathName():TEXT("unavailable"));
        auto GeneratedSlot=Source->GeneratedClass;auto SkeletonSlot=Source->SkeletonGeneratedClass;
        if(!GeneratedSlot.GetGCPtr().IsResolved()||!SkeletonSlot.GetGCPtr().IsResolved()||GeneratedSlot.GetGCPtr().Get()!=Generated.Get()||SkeletonSlot.GetGCPtr().Get()!=Skeleton.Get())
            return Bad(TEXT("source_class_slots_before_frame_classification"));
        auto* BGC=static_cast<UBlueprintGeneratedClass*>(C);
        FieldDiagnostic->SetBoolField(FString(Role)+TEXT("_ubergraph_function_resolved"),BGC->UberGraphFunction.IsResolved());
        if(!BGC->UberGraphFunction.IsResolved())return FieldMismatch(Role,INDEX_NONE,TEXT("ubergraph_function_slot"),TEXT("resolved nullable slot"),TEXT("unresolved"));
        UFunction* UberFunction=BGC->UberGraphFunction.Get();const TStrongObjectPtr<UFunction> ImmediateFunction(UberFunction);
        if(!Context())return Bad(TEXT("compiler_function_bind_context"));
        if(UberFunction&&(!Loaded(UberFunction)||UberFunction->GetOuter()!=C))return FieldMismatch(Role,INDEX_NONE,TEXT("ubergraph_function_owner"),TEXT("loaded function owned by observed class"),TEXT("unavailable_or_other_owner"));
        auto& Compiler=C==Generated.Get()?GeneratedCompiler:SkeletonCompiler;
        Compiler.Function.Reset(UberFunction);Compiler.PublicFrame=BGC->UberGraphFramePointerProperty;
        if(UberFunction){Compiler.FunctionClass=UberFunction->GetClass();Holds.Emplace(Compiler.FunctionClass);Compiler.Flags=UberFunction->GetFlags();Compiler.Path=UberFunction->GetPathName();}
        Compiler.Bound=true;if(!CompilerRoleCurrent(C))return false;
        FStructProperty* Pointer=Compiler.PublicFrame;
        FieldDiagnostic->SetBoolField(FString(Role)+TEXT("_ubergraph_function_observed"),true);
        FieldDiagnostic->SetBoolField(FString(Role)+TEXT("_ubergraph_function_exists"),UberFunction!=nullptr);
        if(UberFunction)FieldDiagnostic->SetStringField(FString(Role)+TEXT("_ubergraph_function_path"),Compiler.Path);
        FieldDiagnostic->SetStringField(FString(Role)+TEXT("_ubergraph_function_address"),LexToString(reinterpret_cast<UPTRINT>(UberFunction)));
        FieldDiagnostic->SetStringField(FString(Role)+TEXT("_public_frame_property_address"),LexToString(reinterpret_cast<UPTRINT>(Pointer)));
        int32 Frames=0;
        for(int32 I=0;I<List.Num();++I)
        {
            auto& E=List[I];auto Row=FieldDiagnostic->GetArrayField(FString(Role)+TEXT("_fields"))[I]->AsObject();
            if(E.Name!=UBlueprintGeneratedClass::GetUberGraphFrameName()&&E.Address!=Pointer){Row->SetStringField(TEXT("role"),TEXT("authored_candidate"));continue;}
            const uint64 Required=uint64(CPF_Transient|CPF_DuplicateTransient);
            const bool Declared=Variables.ContainsByPredicate([&](const FBPVariableDescription& V){return V.VarName==E.Name;});
            if(E.Name!=UBlueprintGeneratedClass::GetUberGraphFrameName()||!(E.Address==Pointer||(!UberFunction&&!Pointer))||!CastField<FStructProperty>(E.Address)||!E.Struct||Declared||(E.Flags&Required)!=Required||(E.Flags&uint64(CPF_Parm|CPF_ExposeOnSpawn|CPF_BlueprintVisible))||++Frames>1)
                return FieldMismatch(Role,I,TEXT("compiler_frame_role"),TEXT("undeclared exact own SDK frame struct/layout/flags and bound pointer or null function+null pointer"),E.Name.ToString()+TEXT("/")+E.Class->GetName()+TEXT("/public_pointer=")+LexToString(E.Address==Pointer)+TEXT("/function_null=")+LexToString(UberFunction==nullptr));
            UScriptStruct* Native=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.PointerToUberGraphFrame"));const TStrongObjectPtr<UScriptStruct> Immediate(Native);
            if(!Context()||!CompilerRoleCurrent(C))return Bad(TEXT("compiler_frame_bind_context"));
            if(!Loaded(Native)||Native!=E.Struct||Native->GetClass()!=UScriptStruct::StaticClass()||!Native->GetOutermost()->GetName().Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)||(Native->StructFlags&STRUCT_Native)==0||Native->GetStructureSize()!=sizeof(FPointerToUberGraphFrame)||Native->GetMinAlignment()!=alignof(FPointerToUberGraphFrame))
                return FieldMismatch(Role,I,TEXT("compiler_frame_struct"),TEXT("/Script/Engine.PointerToUberGraphFrame native exact layout"),GetPathNameSafe(E.Struct));
            if(!FrameStruct.IsValid())
            {
                FrameStruct.Reset(Native);FrameStructOuter.Reset(Native->GetOuter());FrameStructClass=Native->GetClass();Holds.Emplace(FrameStructClass);
                FrameStructObjectFlags=Native->GetFlags();FrameStructFlags=Native->StructFlags;FrameStructSize=Native->GetStructureSize();FrameStructAlignment=Native->GetMinAlignment();
            }
            if(FrameStruct.Get()!=Native||!FrameStructCurrent())return Bad(TEXT("compiler_frame_native_struct_changed"));
            E.CompilerFrame=true;Row->SetStringField(TEXT("role"),TEXT("compiler_ubergraph_frame"));Row->SetBoolField(TEXT("public_frame_property_match"),E.Address==Pointer);
            Row->SetBoolField(TEXT("public_frame_pointer_deferred_without_function"),!UberFunction&&!Pointer);
            Row->SetBoolField(TEXT("cdo_value_exported"),false);
        }
        if(Pointer&&!List.ContainsByPredicate([&](const FField& E){return E.CompilerFrame&&E.Address==Pointer;}))
            return FieldMismatch(Role,INDEX_NONE,TEXT("public_frame_property_membership"),TEXT("null or exact classified direct field"),LexToString(reinterpret_cast<UPTRINT>(Pointer)));
        FieldDiagnostic->SetNumberField(FString(Role)+TEXT("_compiler_frame_count"),Frames);
        FieldDiagnostic->SetNumberField(FString(Role)+TEXT("_user_field_count"),UserFieldCount(List));
        if(UserFieldCount(List)!=Variables.Num())return FieldMismatch(Role,INDEX_NONE,TEXT("user_field_count"),LexToString(Variables.Num()),LexToString(UserFieldCount(List)));
        return true;
    }
    bool BindSource(const FString& Input)
    {
        FieldDiagnostic->SetStringField(TEXT("requested_source"),Input);
        FString Path=Input;Path.TrimStartAndEndInline();Path.TrimQuotesInline();
        if(!Function::Pin::SafeName(Path)||Forbidden(Path)||!Path.StartsWith(TEXT("/Game/"))||!Path.EndsWith(TEXT("_C"))||!Context())return Fail(R,TEXT("construction_generated_source_path_unsupported"),TEXT("Exact loaded Game generated class object path required; aliases/source loading remain incomplete."));
        UClass* C=FindObject<UClass>(nullptr,*Path);TStrongObjectPtr<UClass> Returned(C);
        if(!Context()||!Loaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!C->ClassGeneratedBy.IsResolved())return Fail(R,TEXT("construction_generated_source_unavailable"),TEXT("An already loaded resolved generated class is required; no loader or compile fallback."));
        UObject* Object=C->ClassGeneratedBy.Get();TStrongObjectPtr<UObject> HeldObject(Object);if(!Context()||!Loaded(Object)||Object->GetClass()!=UBlueprint::StaticClass())return Fail(R,TEXT("construction_generated_source_unavailable"),TEXT("Exact owning Blueprint source required."));
        auto* B=static_cast<UBlueprint*>(Object);if(B->GetOuter()!=B->GetPackage()||B->GetPackage()->IsDirty()||B->Status!=BS_UpToDate||B->BlueprintType!=BPTYPE_Normal||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->SimpleConstructionScript||!B->ImplementedInterfaces.IsEmpty()||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=UObject::StaticClass()||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=C||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->NewVariables.Num()>32)return Fail(R,TEXT("construction_generated_source_scope_pending"),TEXT("First generated slice requires saved-clean normal direct UObject Blueprint, no SCS/interfaces and at most32 own primitive scalar members; broader source hierarchies remain incomplete."));
        UClass* S=B->SkeletonGeneratedClass.GetGCPtr().Get();TStrongObjectPtr<UClass> HeldSkeleton(S);if(!Context()||!Loaded(S)||!C->ClassDefaultObject.IsResolved())return Fail(R,TEXT("construction_generated_source_unavailable"),TEXT("Current skeleton and resolved existing nullable CDO slot required."));
        UObject* Default=C->ClassDefaultObject.Get();TStrongObjectPtr<UObject> HeldDefault(Default);if(!Context()||(Default&&(!Loaded(Default)||Default->GetClass()!=C||Default->GetPackage()!=B->GetPackage())))return Fail(R,TEXT("construction_generated_source_cdo_unavailable"),TEXT("The returned existing CDO must be live and owned by the source; no construction fallback."));
        Source.Reset(B);SourcePackage.Reset(B->GetPackage());SourcePath=B->GetPathName();Parent.Reset(UObject::StaticClass());Generated.Reset(C);Skeleton.Reset(S);GeneratedPath=C->GetPathName();SkeletonPath=S->GetPathName();CDO.Reset(Default);if(Default){CDOPath=Default->GetPathName();CDOFlags=Default->GetFlags();CDOOuter=Default->GetOuter();Holds.Emplace(CDOOuter);}
        SourceFlags=B->GetFlags();GeneratedFlags=C->GetFlags();SkeletonFlags=S->GetFlags();GeneratedClassFlags=C->GetClassFlags();SkeletonClassFlags=S->GetClassFlags();Variables=B->NewVariables;SourceFunctions=B->FunctionGraphs;SourceMacros=B->MacroGraphs;SourceEvents=B->UbergraphPages;SourceDelegates=B->DelegateSignatureGraphs;
        FieldDiagnostic->SetNumberField(TEXT("authored_variable_count"),Variables.Num());
        if(!BindFields(C,Fields,TEXT("generated"))||!BindFields(S,SkeletonFields,TEXT("skeleton")))return false;
        if(!ClassifyCompilerFields(C,Fields,TEXT("generated"))||!ClassifyCompilerFields(S,SkeletonFields,TEXT("skeleton"))||!BindGraphs(B,SourceGraphs)||!Plain())return false;
        const auto* ActualSchema=GetDefault<UEdGraphSchema_K2>();Schema.Reset(const_cast<UEdGraphSchema_K2*>(ActualSchema));if(!Plain())return false;
        for(auto* List:{&Fields,&SkeletonFields})for(auto& E:*List)
        {
            if(E.CompilerFrame)continue;
            const bool Converted=Schema->ConvertPropertyToPinType(E.Address,E.Type);if(!Plain())return false;if(!Converted||!Function::Pin::IsPrimitive(E.Type))return FieldMismatch(List==&Fields?TEXT("generated"):TEXT("skeleton"),List->IndexOfByPredicate([&](const FField& V){return V.Address==E.Address;}),TEXT("user_field_type"),TEXT("primitive scalar"),E.Name.ToString()+TEXT("/")+E.Class->GetName());
            const int32 Index=Variables.IndexOfByPredicate([&](const FBPVariableDescription& V){return V.VarName==E.Name&&V.VarGuid.IsValid();});if(Index==INDEX_NONE||!SameType(Variables[Index].VarType,E.Type))return FieldMismatch(List==&Fields?TEXT("generated"):TEXT("skeleton"),Index,TEXT("authored_name_guid_type"),TEXT("matching NewVariables name/valid GUID/fulltype"),E.Name.ToString()+TEXT("/")+E.Class->GetName());
            const bool Exposed=UEdGraphSchema_K2::IsPropertyExposedOnSpawn(E.Address);if(!Plain())return false;E.Exposed=Exposed&&!(E.Flags&(CPF_Parm|CPF_DisableEditOnInstance))&&(E.Flags&CPF_BlueprintVisible);if(E.Exposed&&!CDO.IsValid())return Fail(R,TEXT("construction_generated_source_cdo_required"),TEXT("Nonempty exposed defaults require the existing CDO; no CDO construction fallback."));
        }
        Filename=FPackageName::LongPackageNameToFilename(SourcePackage->GetName(),FPackageName::GetAssetPackageExtension());if(!ReadFileHash(FileHash))return false;
        TSharedRef<FJsonObject> Initial=MakeShared<FJsonObject>();if(!Snapshot(Initial))return false;SourceBefore=Initial;R.Data->SetObjectField(TEXT("class_source_before"),Initial);R.Data->SetBoolField(TEXT("source_class_cdo_exists_before"),CDO.IsValid());FieldDiagnostic->SetBoolField(TEXT("source_bind_completed"),true);return true;
    }
    bool Snapshot(TSharedRef<FJsonObject>& Out)
    {
        if(!Plain())return false;Out=MakeShared<FJsonObject>();Out->SetStringField(TEXT("path"),GeneratedPath);Out->SetStringField(TEXT("blueprint_path"),SourcePath);Out->SetStringField(TEXT("skeleton_path"),SkeletonPath);Out->SetStringField(TEXT("cdo_path"),CDOPath);Out->SetBoolField(TEXT("package_dirty"),SourcePackage->IsDirty());Out->SetNumberField(TEXT("generated_class_flags"),uint32(Generated->GetClassFlags()));Out->SetNumberField(TEXT("skeleton_class_flags"),uint32(Skeleton->GetClassFlags()));Out->SetNumberField(TEXT("generated_object_flags"),uint32(Generated->GetFlags()));Out->SetNumberField(TEXT("skeleton_object_flags"),uint32(Skeleton->GetFlags()));
        auto Metadata=MakeShared<FJsonObject>();for(UClass* C:{Generated.Get(),Skeleton.Get(),Parent.Get()}){auto J=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("BlueprintType"),TEXT("NotBlueprintType"),TEXT("DontUseGenericSpawnObject")}){const bool Value=C->GetBoolMetaData(Key);if(!Plain())return false;J->SetBoolField(Key,Value);}Metadata->SetObjectField(C->GetPathName(),J);}Out->SetObjectField(TEXT("hierarchy_metadata"),Metadata);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& E:Fields)
        {
            if(E.CompilerFrame)continue;
            if(!Plain())return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),E.Name.ToString());J->SetStringField(TEXT("property_path"),E.Address->GetPathName());J->SetStringField(TEXT("property_flags"),LexToString(E.Flags));const bool ExposedNow=UEdGraphSchema_K2::IsPropertyExposedOnSpawn(E.Address);if(!Plain())return false;const bool EffectiveExpose=ExposedNow&&!(E.Flags&(CPF_Parm|CPF_DisableEditOnInstance))&&(E.Flags&CPF_BlueprintVisible);if(EffectiveExpose!=E.Exposed)return Bad(TEXT("source_expose_metadata_changed"));J->SetBoolField(TEXT("exposed"),EffectiveExpose);J->SetObjectField(TEXT("type"),Function::Pin::Describe(E.Type));
            if(CDO.IsValid()){FString Value;const void* Address=E.Address->ContainerPtrToValuePtr<void>(CDO.Get());const bool Exported=E.Address->ExportText_Direct(Value,Address,Address,CDO.Get(),PPF_None);if(!Plain()||!Exported)return Bad(TEXT("source_value_export"));if(Value.Len()>65536)return Bad(TEXT("source_value_budget"));J->SetStringField(TEXT("value_text"),Value);}else J->SetField(TEXT("value_text"),MakeShared<FJsonValueNull>());Rows.Add(MakeShared<FJsonValueObject>(J));
        }Out->SetArrayField(TEXT("own_fields"),Rows);Out->SetNumberField(TEXT("generated_physical_field_count"),Fields.Num());Out->SetNumberField(TEXT("skeleton_physical_field_count"),SkeletonFields.Num());Out->SetNumberField(TEXT("generated_compiler_frame_count"),Fields.Num()-UserFieldCount(Fields));Out->SetNumberField(TEXT("skeleton_compiler_frame_count"),SkeletonFields.Num()-UserFieldCount(SkeletonFields));Out->SetBoolField(TEXT("compiler_frame_cdo_value_exported"),false);Out->SetBoolField(TEXT("runtime_object_constructed"),false);Out->SetBoolField(TEXT("default_buffer_imported"),false);return Plain();
    }
    bool Check()
    {
        if(!Plain())return false;TSharedRef<FJsonObject> Now=MakeShared<FJsonObject>();if(!Snapshot(Now))return false;
        const bool Preserved=Equal(MakeShared<FJsonValueObject>(SourceBefore),MakeShared<FJsonValueObject>(Now));R.Data->SetObjectField(TEXT("class_source_after"),Now);R.Data->SetBoolField(TEXT("class_source_preserved"),Preserved);if(!Preserved)return Bad(TEXT("source_observed_values_changed"));FString Hash;if(!ReadFileHash(Hash)||!Hash.Equals(FileHash,ESearchCase::CaseSensitive))return Bad(TEXT("source_disk_changed"));return Plain();
    }
    bool BindTarget(UBlueprint* BP,UEdGraph* G)
    {
        if(!Check()||!Loaded(BP)||BP==Source.Get()||BP->GetPackage()==SourcePackage.Get()||!Loaded(G)||G->GetOuter()!=BP||BP->GetClass()!=UBlueprint::StaticClass()||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("target_bind"));
        Target.Reset(BP);TargetPackage.Reset(BP->GetPackage());TargetPath=BP->GetPathName();TargetGenerated.Reset(BP->GeneratedClass.GetGCPtr().Get());TargetSkeleton.Reset(BP->SkeletonGeneratedClass.GetGCPtr().Get());TargetVariables=BP->NewVariables;TargetFunctions=BP->FunctionGraphs;TargetMacros=BP->MacroGraphs;TargetEvents=BP->UbergraphPages;TargetDelegates=BP->DelegateSignatureGraphs;Selected=G;GraphGuid=G->GraphGuid;
        return BindGraphs(BP,TargetGraphs)&&Check();
    }
    bool ReacquirePins(bool Assigned,bool First=false)
    {
        // First register returned physical inventory, then observe only copied text.
        if(!Plain())return false;Candidates.Empty();if(!CapturePins(Node.Get(),Candidates))return false;if(!Plain())return false;
        const auto Prior=NodePins;const int32 Exposed=Fields.FilterByPredicate([](const FField& F){return F.Exposed;}).Num();
        if(Candidates.Num()!=5+(Assigned?Exposed:0))return Bad(TEXT("construct_returned_pin_shape"));
        for(const auto& Old:Prior)
        {
            const auto* Next=Candidates.FindByPredicate([&](const FPin& P){return P.Name==Old.Name&&P.Direction==Old.Direction;});
            if(!Next)return Bad(TEXT("construct_reacquire_missing_pin"));FEdGraphPinType ExpectedType=Old.Type;if(Assigned&&Old.Name==UEdGraphSchema_K2::PN_ReturnValue)ExpectedType.PinSubCategoryObject=Generated.Get();
            if(!SameType(Next->Type,ExpectedType)||Next->Flags!=Old.Flags||!Next->Links.IsEmpty()||!Old.Links.IsEmpty()||!Next->Raw.Equals(Old.Raw,ESearchCase::CaseSensitive)||!Next->Auto.Equals(Old.Auto,ESearchCase::CaseSensitive)||Next->Default!=(Assigned&&Old.Name==TEXT("Class")?Generated.Get():Old.Default)||Next->TextFlags!=Old.TextFlags||Next->FriendlyFlags!=Old.FriendlyFlags)return Bad(TEXT("construct_reacquire_value"));
            const FText A=Old.Text,B=Next->Text,FA=Old.Friendly,FB=Next->Friendly;const FString AV=A.ToString();if(!Plain())return false;const FString BV=B.ToString();if(!Plain())return false;const FString AF=FA.ToString();if(!Plain())return false;const FString BF=FB.ToString();if(!Plain())return false;if(!AV.Equals(BV,ESearchCase::CaseSensitive)||!AF.Equals(BF,ESearchCase::CaseSensitive))return Bad(TEXT("construct_reacquire_text"));
            if(Next->Guid!=Old.Guid)return Bad(TEXT("construct_reacquire_guid"));
        }
        for(const auto& E:Fields)if(Assigned&&E.Exposed)
        {
            const auto* P=Candidates.FindByPredicate([&](const FPin& V){return V.Name==E.Name&&V.Direction==EGPD_Input;});if(!P||!SameType(P->Type,E.Type)||!P->Links.IsEmpty()||(P->Flags&7)!=0)return Bad(TEXT("construct_exposed_projection"));
        }
        NodePins=MoveTemp(Candidates);Candidates.Empty();PinsFixed=true;Replacing=false;ClassAssigned=Assigned;return Check();
    }

};

FResult AddGeneratedConstruct(const FString& Path,const FString& ObjectClass,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetBoolField(TEXT("native_generated_construct_object"),true);R.Data->SetStringField(TEXT("requested_object_class"),ObjectClass);R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_GenericCreateObject"));
    R.Data->SetStringField(TEXT("generated_source_scope"),TEXT("Loaded saved-clean normal direct UObject Blueprint; own0..32 primitive scalars, current generated/skeleton and resolved nullable CDO (existing CDO required for exposed values); exact existing native node writer, no source loader/compile/default-buffer import/runtime construction. Broader parents and values remain incomplete."));
    FGeneratedConstruct W(R);if(!W.BindSource(ObjectClass))return R;
    bool Allowed=false;for(UClass* C:{W.Generated.Get(),W.Parent.Get()}){const bool Not=C->GetBoolMetaData(TEXT("NotBlueprintType"));if(!W.Check())return R;const bool BlueprintType=C->GetBoolMetaData(TEXT("BlueprintType"));if(!W.Check())return R;const bool Dont=C->GetBoolMetaData(TEXT("DontUseGenericSpawnObject"));if(!W.Check())return R;if(Not)break;if(BlueprintType){Allowed=!Dont;break;}}
    if(!Allowed||W.Generated->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)){Fail(R,TEXT("construction_generated_class_scope_pending"),TEXT("Current generated class does not satisfy this finite public metadata construction admission; the private compiler utility is not invoked."));return R;}
    int32 XY[2];FString PositionText;if(!Position(PositionJson,XY,PositionText,R))return R;W.X=XY[0];W.Y=XY[1];
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddGeneratedConstructObject","Add Blueprint Construct Object"));
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;
    const bool Resolved=Graph(Path,Name,Id,Type,Create,BP,G,Before,R);if(!R.bChangeStateKnown)return R;if(!W.Check()||!Resolved)return R;
    if(!W.BindTarget(BP,G))return R;
    R.Stage=TEXT("create_construct_object_node");R.bMutationAttempted=true;Count(R,TEXT("node_allocation_calls"));
    UK2Node_GenericCreateObject* Returned=NewObject<UK2Node_GenericCreateObject>(G);W.Node.Reset(Returned);
    if(!W.Context()||!Loaded(Returned)||Returned->GetClass()!=UK2Node_GenericCreateObject::StaticClass()||Returned->GetOuter()!=G){W.Bad(TEXT("construct_node_return"));return R;}
    W.NodePath=Returned->GetPathName();R.Data->SetStringField(TEXT("created_node_path"),W.NodePath);if(!W.Check())return R;
    W.Node->NodePosX=XY[0];W.Node->NodePosY=XY[1];Count(R,TEXT("graph_add_node_calls"));G->AddNode(W.Node.Get(),true);W.Member=true;if(!W.Check())return R;R.bChanged=true;
    Count(R,TEXT("new_guid_calls"));W.Node->CreateNewGuid();W.NodeGuid=W.Node->NodeGuid;W.Placed=true;R.Data->SetStringField(TEXT("created_node_guid"),W.NodeGuid.ToString(EGuidFormats::Digits));if(!W.NodeGuid.IsValid()||!W.Check()){W.Bad(TEXT("construct_guid"));return R;}
    Count(R,TEXT("post_placed_calls"));W.Node->PostPlacedNewNode();if(!W.Check())return R;Count(R,TEXT("allocate_pins_calls"));W.Node->AllocateDefaultPins();W.Replacing=true;if(!W.ReacquirePins(false,true))return R;
    UEdGraphPin* ClassPin=W.Node->GetClassPin();if(!W.Check()||!ClassPin||!W.Node->Pins.Contains(ClassPin)){W.Bad(TEXT("construct_class_pin_missing"));return R;}
    const FString PreviousRaw=ClassPin->DefaultValue,PreviousAuto=ClassPin->AutogeneratedDefaultValue;const FText PreviousText=ClassPin->DefaultTextValue;UObject* PreviousObject=ClassPin->DefaultObject.Get();TStrongObjectPtr<UObject> HeldPrevious(PreviousObject);
    auto DefaultState=[&](const UEdGraphPin* P){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("value"),P->DefaultValue);J->SetStringField(TEXT("object"),GetPathNameSafe(P->DefaultObject.Get()));return J;};R.Data->SetObjectField(TEXT("class_default_before"),DefaultState(ClassPin));
    // Original helper resolves the requested class again before setting the pin.
    UClass* ResolvedClass=FindObject<UClass>(nullptr,*W.GeneratedPath);TStrongObjectPtr<UClass> HeldResolved(ResolvedClass);if(!W.Check()||ResolvedClass!=W.Generated.Get()){W.Bad(TEXT("construct_second_class_lookup"));return R;}
    R.Stage=TEXT("apply_class_default");W.Replacing=true;Count(R,TEXT("try_set_default_object_calls"));W.Schema->TrySetDefaultObject(*ClassPin,ResolvedClass);Count(R,TEXT("try_set_default_object_returns"));
    // Class setter may retire the exposed physical pins; no old pin fields follow it.
    if(!W.Plain())return R;ClassPin=W.Node->GetClassPin();if(!W.Plain()||!ClassPin||!W.Node->Pins.Contains(ClassPin)||!ClassPin->DefaultObject.IsResolved()){W.Bad(TEXT("construct_class_return_unavailable"));return R;}
    const bool Accepted=ClassPin->DefaultObject.Get()==W.Generated.Get();R.Data->SetBoolField(TEXT("class_default_accepted"),Accepted);
    if(!W.ReacquirePins(Accepted,true))return R;ClassPin=W.Node->GetClassPin();R.Data->SetObjectField(TEXT("class_default_after_setter"),DefaultState(ClassPin));
    Count(R,TEXT("default_validation_calls"));const FString Error=W.Schema->IsCurrentPinDefaultValid(ClassPin);if(!W.Check())return R;
    if(!Accepted||!Error.IsEmpty())
    {
        const FString OriginalCode=TEXT("construction_class_default_invalid"),OriginalError=Error.IsEmpty()?TEXT("Requested generated class default was not accepted."):Error;
        R.Data->SetStringField(TEXT("original_class_default_error_code"),OriginalCode);R.Data->SetStringField(TEXT("original_class_default_error"),OriginalError);
        // Known-current local restoration mirrors the original checked-default helper.
        ClassPin=W.Node->GetClassPin();W.Replacing=true;Count(R,TEXT("default_restore_calls"));ClassPin->DefaultValue=PreviousRaw;ClassPin->AutogeneratedDefaultValue=PreviousAuto;ClassPin->DefaultObject=PreviousObject;ClassPin->DefaultTextValue=PreviousText;Count(R,TEXT("default_restore_returns"));
        if(!W.Plain())return R;W.Replacing=false;W.Retired=true;Count(R,TEXT("destroy_node_calls"));W.Node->DestroyNode();Count(R,TEXT("destroy_node_returns"));if(!W.Check())return R;
        R.Data->SetBoolField(TEXT("created_node_retired"),true);R.Data->SetBoolField(TEXT("retired_node_graph_postcondition"),true);R.bChanged=true;Fail(R,*OriginalCode,OriginalError);return R;
    }
    R.Data->SetStringField(TEXT("class_default_applied"),W.GeneratedPath);R.Data->SetStringField(TEXT("object_class"),W.GeneratedPath);
    W.Replacing=true;Count(R,TEXT("pin_default_changed_calls"));static_cast<UEdGraphNode*>(W.Node.Get())->PinDefaultValueChanged(ClassPin);Count(R,TEXT("pin_default_changed_returns"));if(!W.ReacquirePins(true))return R;
    W.Replacing=true;Count(R,TEXT("reconstruct_calls"));static_cast<UEdGraphNode*>(W.Node.Get())->ReconstructNode();if(!W.ReacquirePins(true))return R;
    Mark(BP,R);if(!W.Check())return R;
    // Expected exposed text comes from the original, still-current CDO. The
    // projection observer receives the same node context as the SDK writer.
    TArray<TSharedPtr<FJsonValue>> ExposedRows;
    for(const auto& E:W.Fields)if(E.Exposed)
    {
        auto* P=W.Node->FindPin(E.Name,EGPD_Input);if(!P||!W.Check()){W.Bad(TEXT("construct_exposed_final_missing"));return R;}FString Expected;Count(R,TEXT("exposed_default_projection_calls"));const bool Projected=FBlueprintEditorUtils::PropertyValueToString(E.Address,reinterpret_cast<const uint8*>(W.CDO.Get()),Expected,W.Node.Get());if(!W.Check()||!Projected){W.Bad(TEXT("construct_exposed_source_projection"));return R;}
        P=W.Node->FindPin(E.Name,EGPD_Input);const bool Match=W.Schema->DoesDefaultValueMatch(*P,Expected);if(!W.Check())return R;P=W.Node->FindPin(E.Name,EGPD_Input);const FString Invalid=W.Schema->IsCurrentPinDefaultValid(P);if(!W.Check())return R;
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),E.Name.ToString());J->SetStringField(TEXT("source_default"),Expected);J->SetBoolField(TEXT("source_default_match"),Match);J->SetStringField(TEXT("actual_default"),P->DefaultValue);J->SetStringField(TEXT("autogenerated_default"),P->AutogeneratedDefaultValue);ExposedRows.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("exposed_default_observations"),ExposedRows);
        if(!Match||!Invalid.IsEmpty()){Fail(R,TEXT("construction_exposed_default_unverified"),TEXT("Original source default and actual exposed pin differ; accepted node remains, no retry."));return R;}
    }
    UClass* SpawnedClass=W.Node->GetClassToSpawn();TStrongObjectPtr<UClass> HeldSpawned(SpawnedClass);if(!W.Check()||SpawnedClass!=W.Generated.Get()){W.Bad(TEXT("construct_class_to_spawn"));return R;}
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("node_id"),W.NodeGuid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("name"),W.Node->GetName());Row->SetStringField(TEXT("object_path"),W.NodePath);Row->SetStringField(TEXT("class"),W.Node->GetClass()->GetName());Row->SetStringField(TEXT("node_comment"),W.Node->NodeComment);
    const FText Title=W.Node->GetNodeTitle(ENodeTitleType::FullTitle);if(!W.Check())return R;const FString Display=Title.ToString();if(!W.Check())return R;Row->SetStringField(TEXT("title"),Display);
    TArray<TSharedPtr<FJsonValue>> PinRows;for(const auto& E:W.NodePins){if(!W.Check())return R;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("pin_id"),E.Guid.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("name"),E.Name.ToString());J->SetStringField(TEXT("direction"),E.Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetObjectField(TEXT("type"),Function::Pin::Describe(E.Type));if(!W.Check())return R;J->SetStringField(TEXT("default_value"),E.Raw);J->SetStringField(TEXT("autogenerated_default_value"),E.Auto);PinRows.Add(MakeShared<FJsonValueObject>(J));}Row->SetArrayField(TEXT("pins"),PinRows);
    R.Data->SetObjectField(TEXT("node"),Row);for(const auto& Pair:Row->Values)R.Data->SetField(Pair.Key,Pair.Value);auto GraphRow=MakeShared<FJsonObject>(*Before->GetObjectField(TEXT("graph")));GraphRow->SetNumberField(TEXT("node_count"),G->Nodes.Num());R.Data->SetObjectField(TEXT("graph"),GraphRow);
    auto AfterState=MakeShared<FJsonObject>(*Before);auto Nodes=Before->GetArrayField(TEXT("nodes"));Nodes.Add(MakeShared<FJsonValueObject>(Row));AfterState->SetArrayField(TEXT("nodes"),Nodes);AfterState->SetObjectField(TEXT("graph"),GraphRow);AfterState->SetBoolField(TEXT("dirty"),BP->GetPackage()->IsDirty());TArray<TSharedPtr<FJsonValue>> GraphRows;for(const auto& V:Before->GetArrayField(TEXT("graphs"))){auto J=MakeShared<FJsonObject>(*V->AsObject());if(J->GetStringField(TEXT("object_path")).Equals(G->GetPathName(),ESearchCase::CaseSensitive))J->SetNumberField(TEXT("node_count"),G->Nodes.Num());GraphRows.Add(MakeShared<FJsonValueObject>(J));}AfterState->SetArrayField(TEXT("graphs"),GraphRows);R.Data->SetObjectField(TEXT("after"),AfterState);
    if(!W.Check())return R;R.Data->SetBoolField(TEXT("existing_nodes_preserved"),true);R.Data->SetBoolField(TEXT("graph_rows_preserved"),true);R.Data->SetBoolField(TEXT("construct_object_pin_shape_verified"),true);R.Data->SetBoolField(TEXT("exposed_property_defaults_verified"),true);R.Data->SetBoolField(TEXT("source_class_cdo_exists_after"),W.CDO.IsValid());R.Data->SetBoolField(TEXT("generated_source_disk_preserved"),true);R.Data->SetBoolField(TEXT("runtime_object_constructed"),false);R.bChangeStateKnown=true;R.bChanged=true;R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}

}
Function::FResult AddSubsystemFromPC(const FString& Path,const FString& SubsystemClass,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_subsystem_class"),SubsystemClass);R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_GetSubsystemFromPC"));
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("construction_editor_invalid"),TEXT("Idle Editor game thread required; original PIE/SIE refusal retained."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddPCSubsystem","Add Blueprint LocalPlayer Subsystem Getter"));auto* Class=ResolveClass(SubsystemClass,true,R);if(!Class) { return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto Source=ClassSource(Class,R);R.Data->SetObjectField(TEXT("class_source_before"),Source);
    int32 XY[2]={0,0};FString PositionText;if(!Position(PositionJson,XY,PositionText,R)) { return R; }UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,Create,BP,G,Before,R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;
    if(G==FBlueprintEditorUtils::FindUserConstructionScript(BP)) { Fail(R,TEXT("construction_subsystem_graph_pending"),TEXT("Public subsystem-node compatibility excludes UserConstructionScript; original unguarded compiler-failure variant tracked."));return R; }
    Count(R,TEXT("node_class_lookup_calls"));auto* NodeClass=FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_GetSubsystemFromPC"));Count(R,TEXT("node_class_lookup_returns"));R.Data->SetStringField(TEXT("resolved_node_class"),GetPathNameSafe(NodeClass));
    if(!Loaded(NodeClass)||NodeClass->GetSuperClass()!=UK2Node_GetSubsystem::StaticClass()||!NodeClass->HasAnyClassFlags(CLASS_Native)||NodeClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||NodeClass->ClassGeneratedBy||NodeClass->GetOutermost()->GetName()!=TEXT("/Script/BlueprintGraph"))
    { Fail(R,TEXT("construction_node_class_unavailable"),TEXT("Exact already-loaded native subsystem node subclass required; no derived DLL symbol or fallback."));return R; }const TStrongObjectPtr<UClass> KeepNodeClass(NodeClass);
    R.Stage=TEXT("create_player_subsystem_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_GetSubsystem>(G,NodeClass);const TStrongObjectPtr<UK2Node_GetSubsystem> KeepNode(Node);
    if(!Node) { Fail(R,TEXT("construction_allocation_failed"),TEXT("Subsystem node allocation returned null; attempted prefix retained."));return R; }
    Count(R,TEXT("graph_modify_calls"));Count(R,TEXT("explicit_modify_calls"));G->Modify();Count(R,TEXT("set_transactional_flag_calls"));Node->SetFlags(RF_Transactional);UEdGraphNode* Base=Node;AddAndAllocate(G,Base,XY,Node,Class,R);Mark(BP,R);
    const bool Observed=After(BP,G,Base,Guid,Before,1,R);const auto SA=ClassSource(Class,R);R.Data->SetObjectField(TEXT("class_source_after"),SA);const bool Same=Equal(MakeShared<FJsonValueObject>(Source),MakeShared<FJsonValueObject>(SA));R.Data->SetBoolField(TEXT("class_source_preserved"),Same);R.bChanged=R.bChanged||!Same;if(!Observed) { return R; }
    Count(R,TEXT("subsystem_class_storage_observation_calls"));const auto* Property=FindFProperty<FClassProperty>(UK2Node_GetSubsystem::StaticClass(),TEXT("CustomClass"));const bool Layout=Property&&Property->GetOwnerStruct()==UK2Node_GetSubsystem::StaticClass()&&Property->ArrayDim==1&&Property->MetaClass==USubsystem::StaticClass();
    UObject* Stored=Layout?Property->GetObjectPropertyValue_InContainer(Node):nullptr;R.Data->SetBoolField(TEXT("subsystem_class_storage_layout_verified"),Layout);R.Data->SetStringField(TEXT("subsystem_class_storage_actual"),GetPathNameSafe(Stored));
    const auto* Controller=Base->FindPin(TEXT("PlayerController"),EGPD_Input);const auto* Result=Base->FindPin(UEdGraphSchema_K2::PN_ReturnValue,EGPD_Output);const bool Pins=FreshPins(Base)&&Base->Pins.Num()==2&&Node->GetClassPin()==nullptr&&Controller&&Result&&Controller->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Controller->PinType.PinSubCategoryObject.Get()==APlayerController::StaticClass()&&!Controller->PinType.IsContainer()&&!Controller->PinType.bIsReference&&Result->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Result->PinType.PinSubCategoryObject.Get()==Class&&!Result->PinType.IsContainer()&&!Result->PinType.bIsReference;
    R.Data->SetBoolField(TEXT("subsystem_pin_shape_verified"),Pins);R.Data->SetStringField(TEXT("subsystem_class"),Class->GetPathName());
    if(!Loaded(Node)||Node->GetClass()!=NodeClass||Stored!=Class||!Layout||!Pins||!Same||!Node->IsNodePure()||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||!Node->HasAnyFlags(RF_Transactional)||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1])
    { Fail(R,TEXT("construction_subsystem_postcondition_failed"),TEXT("Current exact subsystem class/pin/transactional/position/source proof failed; original created node retained, no rollback."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
Function::FResult AddConstructObject(const FString& Path,const FString& ObjectClass,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FString GeneratedSelector=ObjectClass;GeneratedSelector.TrimStartAndEndInline();GeneratedSelector.TrimQuotesInline();if(GeneratedSelector.StartsWith(TEXT("/Game/")))return AddGeneratedConstruct(Path,ObjectClass,PositionJson,Name,Id,Type,Create);
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_object_class"),ObjectClass);R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_GenericCreateObject"));
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("construction_editor_invalid"),TEXT("Idle Editor game thread required; original PIE/SIE refusal retained."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddConstructObject","Add Blueprint Construct Object"));auto* Class=ResolveClass(ObjectClass,false,R);if(!Class||!ConstructClass(Class,R)) { return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto Source=ClassSource(Class,R);R.Data->SetObjectField(TEXT("class_source_before"),Source);R.Data->SetBoolField(TEXT("source_class_cdo_exists_before"),Class->GetDefaultObject(false)!=nullptr);
    int32 XY[2]={0,0};FString PositionText;if(!Position(PositionJson,XY,PositionText,R)) { return R; }UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,Create,BP,G,Before,R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;const auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());FExposed Layout;if(!Exposed(Class,Schema,Layout,R)) { return R; }
    R.Stage=TEXT("create_construct_object_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_GenericCreateObject>(G);const TStrongObjectPtr<UK2Node_GenericCreateObject> KeepNode(Node);
    if(!Node) { Fail(R,TEXT("construction_allocation_failed"),TEXT("Construct Object node allocation returned null; attempted prefix retained."));return R; }UEdGraphNode* Base=Node;UK2Node_ConstructObjectFromClass* Construct=Node;AddAndAllocate(G,Base,XY,nullptr,Class,R);
    R.Stage=TEXT("apply_class_default");const bool Accepted=ApplyClassDefault(Construct,Class,Schema,R);R.Data->SetBoolField(TEXT("class_default_accepted"),Accepted);const FString OriginalCode=R.ErrorCode,OriginalError=R.Error;
    if(!Accepted)
    { R.Stage=TEXT("retire_invalid_construct_object");R.Data->SetStringField(TEXT("original_class_default_error_code"),OriginalCode);R.Data->SetStringField(TEXT("original_class_default_error"),OriginalError);Count(R,TEXT("destroy_node_calls"));Base->DestroyNode();Count(R,TEXT("destroy_node_returns"));R.Data->SetBoolField(TEXT("created_node_retired"),!G->Nodes.Contains(Node)); }else { Mark(BP,R); }
    const bool Observed=After(BP,G,Base,Guid,Before,Accepted?1:0,R);const auto SA=ClassSource(Class,R);R.Data->SetObjectField(TEXT("class_source_after"),SA);const bool Same=Equal(MakeShared<FJsonValueObject>(Source),MakeShared<FJsonValueObject>(SA));R.Data->SetBoolField(TEXT("class_source_preserved"),Same);R.bChanged=R.bChanged||!Same;R.Data->SetBoolField(TEXT("source_class_cdo_exists_after"),Class->GetDefaultObject(false)!=nullptr);if(!Observed) { return R; }
    if(!Accepted)
    { const bool Retired=!G->Nodes.Contains(Node)&&Same;R.Data->SetBoolField(TEXT("retired_node_graph_postcondition"),Retired);if(!Retired) { R.bChangeStateKnown=false;Fail(R,TEXT("construction_cleanup_incomplete"),TEXT("Original new-node retirement/source proof did not close; default restoration is not atomic rollback.")); }else { Fail(R,*OriginalCode,OriginalError); }return R; }
    Count(R,TEXT("class_pin_observation_calls"));const auto* CP=Construct->GetClassPin();const auto* Outer=Construct->GetOuterPin();const auto* Result=Base->FindPin(UEdGraphSchema_K2::PN_ReturnValue,EGPD_Output);const auto* Exec=Base->FindPin(UEdGraphSchema_K2::PN_Execute,EGPD_Input);const auto* Then=Base->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output);
    const bool Pins=FreshPins(Base)&&Base->Pins.Num()==5+Layout.Properties.Num()&&CP&&CP->PinType.PinCategory==UEdGraphSchema_K2::PC_Class&&CP->PinType.PinSubCategoryObject.Get()==UObject::StaticClass()&&CP->DefaultObject==Class&&Schema->IsCurrentPinDefaultValid(CP).IsEmpty()&&Construct->GetClassToSpawn()==Class&&Outer&&Outer->Direction==EGPD_Input&&Outer->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Outer->PinType.PinSubCategoryObject.Get()==UObject::StaticClass()&&Result&&Result->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Result->PinType.PinSubCategoryObject.Get()==Class&&Exec&&Then&&Exec->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Then->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec;
    R.Data->SetBoolField(TEXT("construct_object_pin_shape_verified"),Pins);R.Data->SetStringField(TEXT("object_class"),Class->GetPathName());const bool Defaults=ExposedPins(Construct,Class,Layout,Schema,R);R.Data->SetBoolField(TEXT("exposed_property_defaults_verified"),Defaults);
    if(!Loaded(Node)||Node->GetClass()!=UK2Node_GenericCreateObject::StaticClass()||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!Pins||!Defaults||!Same)
    { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("construction_object_postcondition_failed"),TEXT("Current construct node/class/default/outer/exposed/position/source proof failed; accepted node retained, no new rollback.")); }return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
