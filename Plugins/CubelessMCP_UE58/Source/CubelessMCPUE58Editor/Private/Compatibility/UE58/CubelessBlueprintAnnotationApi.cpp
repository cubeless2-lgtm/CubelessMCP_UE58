// DI original comment geometry, live grouping and entry metadata; Editor only.
#include "Compatibility/UE58/CubelessBlueprintAnnotationApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintAnnotation
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

bool OwnerScope(UBlueprint* BP,UEdGraph* G,FResult& R)
{
    if(!Loaded(BP)||!Loaded(G)||!Loaded(BP->ParentClass)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP
        ||(BP->BlueprintType!=BPTYPE_Normal&&BP->BlueprintType!=BPTYPE_FunctionLibrary&&BP->BlueprintType!=BPTYPE_MacroLibrary)
        ||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad
        ||BP->Status==BS_BeingCreated||!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>=2048)
    { return Fail(R,TEXT("annotation_owner_scope_pending"),TEXT("Idle owned editable K2 graph and ordinary native runtime Blueprint owner required; broader owners remain tracked.")); }
    int32 Depth=0;
    for(UClass* C=BP->ParentClass;C;C=C->GetSuperClass())
    {
        const FString Package=C->GetOutermost()->GetName();
        if(++Depth>64||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->IsEditorOnly()
            ||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)
            ||(!Package.Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)&&!Package.Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)))
        { return Fail(R,TEXT("annotation_parent_scope_pending"),TEXT("Finite native Engine/CoreUObject parent hierarchy required; not restricted to direct Actor. Generated/custom/editor/plugin parents remain valid residuals.")); }
    }
    Count(R,TEXT("external_scope_observation_calls"));TArray<TSharedPtr<FJsonValue>> External;
    for(TObjectIterator<UBlueprint> It;It;++It)
    { if(*It!=BP&&It->ParentClass&&BP->GeneratedClass&&It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.Add(MakeShared<FJsonValueString>(It->GetPathName())); } }
    if(BP->GeneratedClass)
    { TArray<UObject*> Instances;GetObjectsOfClass(BP->GeneratedClass,Instances,true,RF_NoFlags);for(UObject* O:Instances) { if(IsValid(O)&&O->GetOutermost()!=BP->GetOutermost()) { External.Add(MakeShared<FJsonValueString>(O->GetPathName())); } } }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"),External);
    return External.IsEmpty()||Fail(R,TEXT("annotation_external_instances_pending"),TEXT("Loaded external instances/derived Blueprint callback effects are outside this observed scope."));
}

TSharedRef<FJsonObject> Source(const UStruct* Struct,FResult& R)
{
    Count(R,TEXT("source_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Struct->GetPathName());J->SetStringField(TEXT("class"),Struct->GetClass()->GetPathName());J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(Struct->GetFlags())));J->SetStringField(TEXT("super"),GetPathNameSafe(Struct->GetSuperStruct()));J->SetStringField(TEXT("package"),Struct->GetOutermost()->GetName());J->SetStringField(TEXT("package_flags"),LexToString(static_cast<uint32>(Struct->GetOutermost()->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Struct->GetOutermost()->IsDirty());
    if(const UClass* Class=Cast<UClass>(Struct)) { J->SetStringField(TEXT("class_flags"),LexToString(static_cast<uint32>(Class->GetClassFlags())));J->SetStringField(TEXT("generated_by"),GetPathNameSafe(Class->ClassGeneratedBy)); }
    if(const UScriptStruct* Script=Cast<UScriptStruct>(Struct)) { J->SetStringField(TEXT("struct_flags"),LexToString(static_cast<uint32>(Script->StructFlags))); }
    return J;
}

void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("external_scope_observation_calls"),TEXT("source_observation_calls"),TEXT("annotation_capture_calls"),TEXT("node_annotation_observation_calls"),TEXT("group_observation_calls"),TEXT("function_metadata_observation_calls"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("set_transactional_flag_calls"),TEXT("graph_modify_calls"),TEXT("node_modify_calls"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("initial_style_write_calls"),TEXT("late_style_write_calls"),TEXT("rename_node_calls"),TEXT("group_lookup_calls"),TEXT("group_add_calls"),TEXT("group_add_returns"),TEXT("node_comment_write_calls"),TEXT("function_tooltip_write_calls"),TEXT("function_metadata_set_calls"),TEXT("function_metadata_set_returns"),TEXT("graph_notify_calls"),TEXT("mark_structural_calls"),TEXT("mark_structural_returns"),TEXT("mark_modified_calls"),TEXT("mark_modified_returns"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("destroy_node_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("counter_scope"),TEXT("Direct calls only. MarkStructurallyModified synchronously requests RegenerateSkeletonOnly and changes visualization/dependency caches, compiled skeleton/CDO/children and callbacks. Explicit compile/save0 is not total compilation0. Graph.Modify/AddNode/Notify and metadata calls can have SDK transitive effects."));
    R.Data->SetStringField(TEXT("preservation_scope"),TEXT("Ordered graph identities and selected graph node/pin/default/link rows, all selected-graph base annotations/comment styling/live grouped objects and public entry metadata except requested fields; native comment/entry class and parent source. Full Blueprint variables/CDO/SCS/world/editor caches, generated UFunction tooltip and cold saved grouping are not claimed."));
    R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("atomic_rollback_claimed"),false);R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);R.Data->SetBoolField(TEXT("cold_grouping_verified"),false);
}
bool ParseOptional(const FString& Json,TSharedPtr<FJsonValue>& Out,FResult& R)
{
    if(Json.IsEmpty()) { Out.Reset();return true; }
    return Function::Pin::ParseJson(Json,Out,R);
}
bool Vector(const FString& Json,double DefaultX,double DefaultY,bool Round,int32 (&XY)[2],const TCHAR* Label,FResult& R)
{
    TSharedPtr<FJsonValue> V;if(!ParseOptional(Json,V,R)) { return false; }double Values[2]={DefaultX,DefaultY};
    if(V.IsValid())
    {
        Values[0]=Values[1]=0;
        if(V->Type==EJson::Array&&V->AsArray().Num()>=2)
        { for(int32 I=0;I<2;++I) { V->AsArray()[I]->TryGetNumber(Values[I]); } }
        else if(V->Type==EJson::String)
        {
            FString S=V->AsString();S.TrimStartAndEndInline();S.RemoveFromStart(TEXT("["));S.RemoveFromEnd(TEXT("]"));S.RemoveFromStart(TEXT("("));S.RemoveFromEnd(TEXT(")"));TArray<FString> Parts;S.ParseIntoArray(Parts,TEXT(","),true);
            if(Parts.Num()>=2) { Values[0]=FCString::Atof(*Parts[0].TrimStartAndEnd());Values[1]=FCString::Atof(*Parts[1].TrimStartAndEnd()); }
        }
    }
    TArray<TSharedPtr<FJsonValue>> Floats,Ints;
    for(int32 I=0;I<2;++I)
    {
        if(!FMath::IsFinite(Values[I])||FMath::Abs(Values[I])>FLT_MAX) { return Fail(R,TEXT("annotation_geometry_unrepresentable"),TEXT("Finite float32 geometry required; original nonfinite/out-of-range conversions remain incomplete.")); }
        const float F=static_cast<float>(Values[I]);const double D=F;
        if(!FMath::IsFinite(F)||D<static_cast<double>(MIN_int32)||D>static_cast<double>(MAX_int32)-(Round?0.5:0.0)) { return Fail(R,TEXT("annotation_geometry_unrepresentable"),TEXT("Original float32 geometry must fit safe int32 assignment/rounding before graph Modify.")); }
        XY[I]=Round?FMath::Max(64,FMath::RoundToInt(F)):static_cast<int32>(F);Floats.Add(MakeShared<FJsonValueNumber>(D));Ints.Add(MakeShared<FJsonValueNumber>(XY[I]));
    }
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("omitted"),!V.IsValid());if(V.IsValid()) { J->SetField(TEXT("requested"),V); }J->SetArrayField(TEXT("float32"),Floats);J->SetArrayField(TEXT("stored_int32"),Ints);R.Data->SetObjectField(Label,J);return true;
}
bool Color(const FString& Json,FLinearColor& Out,FResult& R)
{
    Out=FLinearColor(0.15f,0.18f,0.22f,1.f);TSharedPtr<FJsonValue> V;if(!ParseOptional(Json,V,R)) { return false; }
    if(V.IsValid()&&V->Type==EJson::Array&&V->AsArray().Num()>=3)
    {
        double D[4]={0,0,0,Out.A};for(int32 I=0;I<FMath::Min(4,V->AsArray().Num());++I) { V->AsArray()[I]->TryGetNumber(D[I]); }
        for(double N:D) { if(!FMath::IsFinite(N)||FMath::Abs(N)>FLT_MAX) { return Fail(R,TEXT("annotation_color_unrepresentable"),TEXT("Finite float32 color channels required; channels are not clamped.")); } }
        Out=FLinearColor(static_cast<float>(D[0]),static_cast<float>(D[1]),static_cast<float>(D[2]),static_cast<float>(D[3]));
    }
    R.Data->SetBoolField(TEXT("color_omitted"),!V.IsValid());if(V.IsValid()) { R.Data->SetField(TEXT("requested_color"),V); }return true;
}
TSharedRef<FJsonObject> NodeFields(UEdGraphNode* N,FResult& R)
{
    Count(R,TEXT("node_annotation_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),N->GetPathName());J->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());J->SetStringField(TEXT("class"),N->GetClass()->GetPathName());J->SetStringField(TEXT("node_comment"),N->NodeComment);J->SetBoolField(TEXT("comment_bubble_visible"),N->bCommentBubbleVisible);J->SetBoolField(TEXT("comment_bubble_make_visible"),N->bCommentBubbleMakeVisible);J->SetBoolField(TEXT("comment_bubble_pinned"),N->bCommentBubblePinned);
    if(const auto* C=Cast<UEdGraphNode_Comment>(N))
    {
        J->SetNumberField(TEXT("width"),C->NodeWidth);J->SetNumberField(TEXT("height"),C->NodeHeight);J->SetNumberField(TEXT("font_size"),C->FontSize);J->SetStringField(TEXT("move_mode"),C->MoveMode==ECommentBoxMode::NoGroupMovement?TEXT("no_group"):TEXT("group"));J->SetBoolField(TEXT("comment_bubble_visible_in_details"),C->bCommentBubbleVisible_InDetailsPanel);J->SetBoolField(TEXT("color_comment_bubble"),C->bColorCommentBubble);J->SetStringField(TEXT("node_details"),C->NodeDetails.ToString());J->SetNumberField(TEXT("comment_depth"),C->CommentDepth);
        TArray<TSharedPtr<FJsonValue>> RGBA;for(float F:{C->CommentColor.R,C->CommentColor.G,C->CommentColor.B,C->CommentColor.A}) { RGBA.Add(MakeShared<FJsonValueNumber>(F)); }J->SetArrayField(TEXT("comment_color"),RGBA);
        Count(R,TEXT("group_observation_calls"));TArray<TSharedPtr<FJsonValue>> Ids,Objects;for(const UObject* O:C->GetNodesUnderComment()) { Objects.Add(MakeShared<FJsonValueString>(GetPathNameSafe(O)));if(const auto* G=Cast<UEdGraphNode>(O)) { Ids.Add(MakeShared<FJsonValueString>(G->NodeGuid.ToString())); } }J->SetArrayField(TEXT("nodes_under_comment"),Ids);J->SetArrayField(TEXT("grouped_object_paths"),Objects);
    }
    if(const auto* E=Cast<UK2Node_FunctionEntry>(N))
    {
        Count(R,TEXT("function_metadata_observation_calls"));auto M=MakeShared<FJsonObject>();const auto& D=E->MetaData;M->SetStringField(TEXT("tooltip"),D.ToolTip.ToString());M->SetStringField(TEXT("category"),D.Category.ToString());M->SetStringField(TEXT("keywords"),D.Keywords.ToString());M->SetStringField(TEXT("compact_title"),D.CompactNodeTitle.ToString());M->SetStringField(TEXT("deprecation_message"),D.DeprecationMessage);M->SetBoolField(TEXT("deprecated"),D.bIsDeprecated);M->SetBoolField(TEXT("call_in_editor"),D.bCallInEditor);M->SetBoolField(TEXT("thread_safe"),D.bThreadSafe);M->SetBoolField(TEXT("unsafe_actor_construction"),D.bIsUnsafeDuringActorConstruction);
        TArray<TSharedPtr<FJsonValue>> RGBA;for(float F:{D.InstanceTitleColor.R,D.InstanceTitleColor.G,D.InstanceTitleColor.B,D.InstanceTitleColor.A}) { RGBA.Add(MakeShared<FJsonValueNumber>(F)); }M->SetArrayField(TEXT("title_color"),RGBA);auto Map=MakeShared<FJsonObject>();for(const auto& P:D.GetMetaDataMap()) { Map->SetStringField(P.Key.ToString(),P.Value); }M->SetObjectField(TEXT("map"),Map);J->SetObjectField(TEXT("function_metadata"),M);
        // Transient compiler cache is retained separately, not compared as persisted user metadata.
        auto Cache=MakeShared<FJsonObject>();Cache->SetStringField(TEXT("entry_path"),E->GetPathName());Cache->SetNumberField(TEXT("has_latent_functions"),D.HasLatentFunctions);TArray<TSharedPtr<FJsonValue>> CachePrefix;const TArray<TSharedPtr<FJsonValue>>* Existing=nullptr;if(R.Data->TryGetArrayField(TEXT("entry_transient_cache_observations"),Existing)) { CachePrefix=*Existing; }CachePrefix.Add(MakeShared<FJsonValueObject>(Cache));R.Data->SetArrayField(TEXT("entry_transient_cache_observations"),CachePrefix);
    }
    return J;
}
bool Annotations(UEdGraph* G,TArray<TSharedPtr<FJsonValue>>& Rows,FResult& R)
{
    Count(R,TEXT("annotation_capture_calls"));Rows.Reset();for(UEdGraphNode* N:G->Nodes)
    { if(!Loaded(N)) { return Fail(R,TEXT("annotation_observation_incomplete"),TEXT("A selected graph node is not fully loaded; no annotation reader repairs or loaders.")); }
      if(const auto* C=Cast<UEdGraphNode_Comment>(N)) { if(C->GetNodesUnderComment().Num()>4096) { return Fail(R,TEXT("annotation_group_budget"),TEXT("Live grouped object list exceeds4096.")); }for(const auto* O:C->GetNodesUnderComment()) { if(!Loaded(O)) { return Fail(R,TEXT("annotation_group_observation_incomplete"),TEXT("Grouped object is invalid or requires load; actual prefix retained.")); } } }
      Rows.Add(MakeShared<FJsonValueObject>(NodeFields(N,R))); }return true;
}
bool Begin(const FString& Path,const FString& Name,const FString& Id,const FString& Type,bool Create,UBlueprint*& BP,UEdGraph*& G,TSharedPtr<FJsonObject>& Before,TArray<TSharedPtr<FJsonValue>>& Old,FResult& R)
{
    R.Stage=TEXT("resolve_graph");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(Path,Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));R.bMutationAttempted=S.bMutationAttempted;R.bChanged=S.bChanged;R.bChangeStateKnown=S.bChangeStateKnown;
    if(!S.bSuccess||!S.bPostconditionPassed) { return Fail(R,S.ErrorCode.IsEmpty()?TEXT("annotation_graph_unverified"):*S.ErrorCode,S.Error); }
    BP=FindObject<UBlueprint>(nullptr,*S.Data->GetStringField(TEXT("blueprint")));G=FindObject<UEdGraph>(nullptr,*S.Data->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")));Before=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetStringField(TEXT("blueprint"),S.Data->GetStringField(TEXT("blueprint")));
    if(!OwnerScope(BP,G,R)||!Annotations(G,Old,R)) { return false; }R.Data->SetArrayField(TEXT("annotations_before"),Old);return true;
}
UEdGraphNode* FindNode(UEdGraph* G,const FString& Id)
{
    // Exact original GUID string rendering comparison (FString equality is case-insensitive).
    for(UEdGraphNode* N:G->Nodes) { if(N&&N->NodeGuid.ToString()==Id) { return N; } }return nullptr;
}
void Modified(UBlueprint* BP,UEdGraph* G,bool Structural,FResult& R)
{
    R.Stage=TEXT("notify_mark");Count(R,TEXT("graph_notify_calls"));G->NotifyGraphChanged();
    if(Structural) { Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Count(R,TEXT("mark_structural_returns"));R.Data->SetStringField(TEXT("implicit_skeleton_scope"),TEXT("One direct structural request; this admitted idle nonsuppressed owner reaches public synchronous RegenerateSkeletonOnly. No explicit full compile or save.")); }
    else { Count(R,TEXT("mark_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Count(R,TEXT("mark_modified_returns")); }
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

FString CommentMoveModeToString(const ECommentBoxMode::Type MoveMode)
{
    return MoveMode == ECommentBoxMode::NoGroupMovement ? TEXT("no_group") : TEXT("group");
}

void AddCommentNodeFields(TSharedPtr<FJsonObject>& NodeObject, const UEdGraphNode_Comment* CommentNode)
{
    if (!NodeObject.IsValid() || !CommentNode)
    {
        return;
    }

    NodeObject->SetNumberField(TEXT("width"), CommentNode->NodeWidth);
    NodeObject->SetNumberField(TEXT("height"), CommentNode->NodeHeight);
    NodeObject->SetNumberField(TEXT("font_size"), CommentNode->FontSize);
    NodeObject->SetStringField(TEXT("move_mode"), CommentMoveModeToString(CommentNode->MoveMode.GetValue()));
    NodeObject->SetBoolField(TEXT("comment_bubble_visible_in_details"), CommentNode->bCommentBubbleVisible_InDetailsPanel);
    NodeObject->SetBoolField(TEXT("color_comment_bubble"), CommentNode->bColorCommentBubble);
    NodeObject->SetStringField(TEXT("node_details"), CommentNode->NodeDetails.ToString());

    TArray<TSharedPtr<FJsonValue>> ColorValues;
    ColorValues.Add(MakeShared<FJsonValueNumber>(CommentNode->CommentColor.R));
    ColorValues.Add(MakeShared<FJsonValueNumber>(CommentNode->CommentColor.G));
    ColorValues.Add(MakeShared<FJsonValueNumber>(CommentNode->CommentColor.B));
    ColorValues.Add(MakeShared<FJsonValueNumber>(CommentNode->CommentColor.A));
    NodeObject->SetArrayField(TEXT("comment_color"), ColorValues);

    TArray<TSharedPtr<FJsonValue>> NodesUnderCommentValues;
    for (const UObject* Object : CommentNode->GetNodesUnderComment())
    {
        const UEdGraphNode* Node = Cast<UEdGraphNode>(Object);
        if (!Node)
        {
            continue;
        }
        NodesUnderCommentValues.Add(MakeShared<FJsonValueString>(Node->NodeGuid.ToString()));
    }
    NodeObject->SetArrayField(TEXT("nodes_under_comment"), NodesUnderCommentValues);
}

bool Close(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const FGuid& Guid,const TSharedPtr<FJsonObject>& Before,const TArray<TSharedPtr<FJsonValue>>& Old,int32 Delta,const TSharedRef<FJsonObject>& Expected,FResult& R)
{
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(BP->GetPathName(),TEXT(""),Guid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(S));
    if(!S.bSuccess||!S.bPostconditionPassed||!S.bChangeStateKnown) { R.bChangeStateKnown=false;return Fail(R,TEXT("annotation_after_incomplete"),TEXT("Current graph observation incomplete; accepted writes/implicit refresh remain without retry or rollback.")); }
    const auto After=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),After);R.Data->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After));
    TArray<TSharedPtr<FJsonValue>> Rows;if(!Annotations(G,Rows,R)) { R.Data->SetArrayField(TEXT("incomplete_annotations_after"),Rows);R.bChangeStateKnown=false;return false; }R.Data->SetArrayField(TEXT("annotations_after"),Rows);
    const bool Topology=ExistingNodesPreserved(Before,After,Delta)&&GraphRowsPreserved(Before,After,G->GetPathName(),Delta)&&G->GraphGuid==Guid;
    bool Metadata=Rows.Num()==Old.Num()+Delta;int32 SelectedCount=0;
    for(int32 I=0;I<Old.Num()&&I<Rows.Num();++I)
    { const auto O=Old[I]->AsObject();if(O->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive)) { ++SelectedCount;Metadata&=Equal(MakeShared<FJsonValueObject>(Expected),Rows[I]); }else { Metadata&=Equal(Old[I],Rows[I]); } }
    if(Delta==1) { Metadata&=Rows.Num()>0&&Equal(MakeShared<FJsonValueObject>(Expected),Rows.Last()); }else { Metadata&=SelectedCount==1; }
    R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueArray>(Old),MakeShared<FJsonValueArray>(Rows));R.bChangeStateKnown=true;R.Data->SetBoolField(TEXT("existing_nodes_pins_links_preserved"),Topology);R.Data->SetBoolField(TEXT("annotation_metadata_verified"),Metadata);
    const auto* NodeRow=After->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(NodeRow) { R.Data->SetObjectField(TEXT("node"),(*NodeRow)->AsObject());for(const auto& P:(*NodeRow)->AsObject()->Values) { R.Data->SetField(P.Key,P.Value); } }
    const auto Current=NodeFields(Node,R);R.Data->SetObjectField(TEXT("current_annotations"),Current);for(const auto& P:Current->Values) { R.Data->SetField(P.Key,P.Value); }
    auto Legacy=NodeToJson(Node,Delta==0);if(const auto* C=Cast<UEdGraphNode_Comment>(Node)) { AddCommentNodeFields(Legacy,C); }
    auto LegacyGraph=MakeShared<FJsonObject>(*S.Data->GetObjectField(TEXT("graph")));LegacyGraph->RemoveField(TEXT("object_path"));LegacyGraph->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());Legacy->SetObjectField(TEXT("graph"),LegacyGraph);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);

    return (Topology&&Metadata&&NodeRow&&Loaded(Node)&&G->Nodes.Contains(Node)&&BP->GetOutermost()->IsDirty())||Fail(R,TEXT("annotation_postcondition_failed"),TEXT("Current requested annotation/topology/source scope differs; actual writes retained and no rollback claimed."));
}
}

Function::FResult AddCommentBox(const FString& Path,const FString& Text,const FString& PositionJson,const FString& SizeJson,const FString& ColorJson,int32 FontSize,const FString& MoveMode,bool Bubble,bool ColorBubble,const FString& Details,const FString& NodeIdsJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_comment_text"),Text);R.Data->SetStringField(TEXT("requested_node_details"),Details);R.Data->SetStringField(TEXT("requested_move_mode"),MoveMode);R.Data->SetNumberField(TEXT("requested_font_size"),FontSize);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("annotation_editor_invalid"),TEXT("Idle Editor game thread required; original PIE/SIE refusal preserved."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddCommentBox","Add Blueprint Comment Box"));
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;TArray<TSharedPtr<FJsonValue>> Old;if(!Begin(Path,Name,Id,Type,Create,BP,G,Before,Old,R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;
    // Original graph resolution precedes array/style normalization, retaining any create prefix.
    int32 XY[2],WH[2];FLinearColor Tint;if(!Vector(PositionJson,0,0,false,XY,TEXT("position_projection"),R)||!Vector(SizeJson,640,320,true,WH,TEXT("size_projection"),R)||!Color(ColorJson,Tint,R)) { return R; }
    FontSize=FMath::Clamp(FontSize,1,1000);const auto Mode=MoveMode.IsEmpty()||MoveMode.Equals(TEXT("group"),ESearchCase::IgnoreCase)||MoveMode.Equals(TEXT("group_movement"),ESearchCase::IgnoreCase)?ECommentBoxMode::GroupMovement:ECommentBoxMode::NoGroupMovement;
    TSharedPtr<FJsonValue> IdValues;if(!ParseOptional(NodeIdsJson,IdValues,R)) { return R; }if(IdValues.IsValid()&&IdValues->Type==EJson::Array&&IdValues->AsArray().Num()>4096) { Fail(R,TEXT("annotation_group_budget"),TEXT("Requested group list exceeds4096; original larger lists remain incomplete."));return R; }
    UClass* Class=UEdGraphNode_Comment::StaticClass();if(!Loaded(Class)) { Fail(R,TEXT("annotation_class_unavailable"),TEXT("Native comment class must already be fully loaded."));return R; }
    const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R),ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);
    R.Stage=TEXT("graph_modify");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("graph_modify_calls"));G->Modify();
    R.Stage=TEXT("allocate_comment");Count(R,TEXT("node_allocation_calls"));auto* C=NewObject<UEdGraphNode_Comment>(G);Count(R,TEXT("node_allocation_returns"));R.Data->SetBoolField(TEXT("allocation_returned_null"),C==nullptr);const TStrongObjectPtr<UEdGraphNode_Comment> KeepNode(C);
    if(!C) { Fail(R,TEXT("annotation_allocation_failed"),TEXT("Failed to create Blueprint comment node; Graph.Modify prefix retained."));return R; }R.Data->SetStringField(TEXT("created_node_path"),C->GetPathName());
    Count(R,TEXT("set_transactional_flag_calls"));C->SetFlags(RF_Transactional);Count(R,TEXT("initial_style_write_calls"));C->NodePosX=XY[0];C->NodePosY=XY[1];C->NodeWidth=WH[0];C->NodeHeight=WH[1];C->NodeComment=Text;C->CommentColor=Tint;C->FontSize=FontSize;C->MoveMode=Mode;C->bCommentBubbleVisible_InDetailsPanel=Bubble;C->bColorCommentBubble=ColorBubble;C->NodeDetails=FText::FromString(Details);
    R.Data->SetObjectField(TEXT("initial_annotations"),NodeFields(C,R));
    UEdGraphNode* Base=C;Count(R,TEXT("graph_add_node_calls"));G->AddNode(Base,true,false);Count(R,TEXT("new_guid_calls"));Base->CreateNewGuid();R.Data->SetStringField(TEXT("created_node_guid"),Base->NodeGuid.ToString());Count(R,TEXT("post_placed_calls"));Base->PostPlacedNewNode();R.Data->SetObjectField(TEXT("post_placed_annotations"),NodeFields(C,R));Count(R,TEXT("allocate_pins_calls"));Base->AllocateDefaultPins();Count(R,TEXT("reconstruct_calls"));Base->ReconstructNode();
    Count(R,TEXT("late_style_write_calls"));C->NodeWidth=WH[0];C->NodeHeight=WH[1];Count(R,TEXT("rename_node_calls"));Base->OnRenameNode(Text);C->CommentColor=Tint;C->FontSize=FontSize;C->MoveMode=Mode;C->bCommentBubbleVisible_InDetailsPanel=Bubble;C->bCommentBubbleVisible=Bubble;C->bCommentBubblePinned=Bubble;C->bColorCommentBubble=ColorBubble;C->NodeDetails=FText::FromString(Details);
    R.Stage=TEXT("group_nodes");TArray<TSharedPtr<FJsonValue>> Grouped,GroupPaths,Lookups;int32 ExpectedDepth=-1;
    if(IdValues.IsValid()&&IdValues->Type==EJson::Array)
    {
        for(const auto& V:IdValues->AsArray())
        {
            FString NodeId;if(!V.IsValid()||!V->TryGetString(NodeId)||NodeId.IsEmpty()) { continue; }Count(R,TEXT("group_lookup_calls"));auto* N=FindNode(G,NodeId);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_id"),NodeId);J->SetStringField(TEXT("resolved_path"),GetPathNameSafe(N));J->SetBoolField(TEXT("self_skipped"),N==C);Lookups.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("group_lookup_prefix"),Lookups);
            if(N&&N!=C)
            {
                if(const auto* Child=Cast<UEdGraphNode_Comment>(N)) { if(Child->CommentDepth==MIN_int32) { R.Data->SetObjectField(TEXT("partial_current_annotations"),NodeFields(C,R));Fail(R,TEXT("annotation_group_depth_unrepresentable"),TEXT("Original child depth-1 would overflowint32; created node/group prefix remains with no destroy/replay."));return R; }ExpectedDepth=FMath::Min(ExpectedDepth,Child->CommentDepth-1); }
                Count(R,TEXT("group_add_calls"));C->AddNodeUnderComment(N);Count(R,TEXT("group_add_returns"));Grouped.Add(MakeShared<FJsonValueString>(N->NodeGuid.ToString()));GroupPaths.Add(MakeShared<FJsonValueString>(N->GetPathName()));R.Data->SetArrayField(TEXT("group_add_prefix"),GroupPaths);
            }
        }
    }
    auto Expected=NodeFields(C,R);R.Data->SetObjectField(TEXT("configured_annotations"),Expected);
    bool Requested=C->NodeComment.Equals(Text,ESearchCase::CaseSensitive)&&C->NodePosX==XY[0]&&C->NodePosY==XY[1]&&C->NodeWidth==WH[0]&&C->NodeHeight==WH[1]&&C->CommentColor==Tint&&C->FontSize==FontSize&&C->MoveMode==Mode&&C->bCommentBubbleVisible_InDetailsPanel==Bubble&&C->bCommentBubbleVisible==Bubble&&C->bCommentBubblePinned==Bubble&&C->bColorCommentBubble==ColorBubble&&C->NodeDetails.ToString().Equals(Details,ESearchCase::CaseSensitive)&&C->CommentDepth==ExpectedDepth&&C->Pins.IsEmpty()&&C->HasAnyFlags(RF_Transactional)&&Equal(MakeShared<FJsonValueArray>(Grouped),Expected->TryGetField(TEXT("nodes_under_comment")))&&Equal(MakeShared<FJsonValueArray>(GroupPaths),Expected->TryGetField(TEXT("grouped_object_paths")));
    R.Data->SetBoolField(TEXT("configured_requested_fields_verified"),Requested);Modified(BP,G,true,R);const bool Observed=Close(BP,G,C,Guid,Before,Old,1,Expected,R);
    const auto ClassAfter=Source(Class,R),ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);const bool Sources=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter));R.Data->SetBoolField(TEXT("native_source_preserved"),Sources);R.bChanged|=!Sources;
    if(!Observed) { return R; }if(!Requested||!Sources) { Fail(R,TEXT("annotation_requested_or_source_failed"),TEXT("Requested style/live grouping or native source differs; actual node and structural refresh prefix retained."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
Function::FResult SetNodeComment(const FString& Path,const FString& Id,const FString& Text,bool Visible,bool Pinned,const FString& Name,const FString& GraphId,const FString& Type)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_node_id"),Id);R.Data->SetStringField(TEXT("requested_node_comment"),Text);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("annotation_editor_invalid"),TEXT("Idle Editor game thread required; original PIE/SIE refusal preserved."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","SetNodeComment","Set Blueprint Node Comment"));UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;TArray<TSharedPtr<FJsonValue>> Old;if(!Begin(Path,Name,GraphId,Type,false,BP,G,Before,Old,R)) { return R; }
    auto* N=FindNode(G,Id);if(!N) { Fail(R,TEXT("annotation_node_not_found"),FString::Printf(TEXT("Node not found: %s"),*Id));return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const TStrongObjectPtr<UEdGraphNode> KeepNode(N);const FGuid Guid=G->GraphGuid;auto Expected=MakeShared<FJsonObject>(*NodeFields(N,R));Expected->SetStringField(TEXT("node_comment"),Text);Expected->SetBoolField(TEXT("comment_bubble_visible"),Visible);Expected->SetBoolField(TEXT("comment_bubble_make_visible"),Visible);Expected->SetBoolField(TEXT("comment_bubble_pinned"),Pinned);
    const auto ClassBefore=Source(N->GetClass(),R),ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);
    R.Stage=TEXT("set_node_comment");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("graph_modify_calls"));G->Modify();Count(R,TEXT("node_modify_calls"));N->Modify();Count(R,TEXT("node_comment_write_calls"));N->NodeComment=Text;N->bCommentBubbleVisible=Visible;N->bCommentBubbleMakeVisible=Visible;N->bCommentBubblePinned=Pinned;R.Data->SetObjectField(TEXT("configured_annotations"),NodeFields(N,R));Modified(BP,G,false,R);const bool Observed=Close(BP,G,N,Guid,Before,Old,0,Expected,R);
    const auto ClassAfter=Source(N->GetClass(),R),ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);const bool Sources=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter));R.Data->SetBoolField(TEXT("native_source_preserved"),Sources);R.bChanged|=!Sources;
    if(!Observed) { return R; }if(!Sources) { Fail(R,TEXT("annotation_native_source_failed"),TEXT("Native node/parent source differs; requested writes remain without rollback."));return R; }R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
Function::FResult SetFunctionDescription(const FString& Path,const FString& Description,const FString& Name,const FString& Id,const FString& Type)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_description"),Description);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("annotation_editor_invalid"),TEXT("Idle Editor game thread required; original PIE/SIE refusal preserved."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","SetFunctionDescription","Set Blueprint Function Description"));UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;TArray<TSharedPtr<FJsonValue>> Old;if(!Begin(Path,Name,Id,Type,false,BP,G,Before,Old,R)) { return R; }
    UK2Node_FunctionEntry* E=nullptr;for(UEdGraphNode* N:G->Nodes) { if((E=Cast<UK2Node_FunctionEntry>(N))) { break; } }if(!E) { Fail(R,TEXT("annotation_entry_not_found"),FString::Printf(TEXT("Function entry node not found in graph: %s"),*G->GetName()));return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const TStrongObjectPtr<UK2Node_FunctionEntry> KeepNode(E);const FGuid Guid=G->GraphGuid;
    auto Expected=MakeShared<FJsonObject>(*NodeFields(E,R));auto Meta=MakeShared<FJsonObject>(*Expected->GetObjectField(TEXT("function_metadata")));auto Map=MakeShared<FJsonObject>(*Meta->GetObjectField(TEXT("map")));Meta->SetStringField(TEXT("tooltip"),Description);FString TooltipKey=FBlueprintMetadata::MD_Tooltip.ToString();for(const auto& P:E->MetaData.GetMetaDataMap()) { if(P.Key==FBlueprintMetadata::MD_Tooltip) { TooltipKey=P.Key.ToString();break; } }Map->SetStringField(TooltipKey,Description);Meta->SetObjectField(TEXT("map"),Map);Expected->SetObjectField(TEXT("function_metadata"),Meta);
    const auto ClassBefore=Source(E->GetClass(),R),ParentBefore=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_before"),ClassBefore);R.Data->SetObjectField(TEXT("parent_source_before"),ParentBefore);
    R.Stage=TEXT("set_function_description");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("graph_modify_calls"));G->Modify();Count(R,TEXT("node_modify_calls"));E->Modify();Count(R,TEXT("function_tooltip_write_calls"));E->MetaData.ToolTip=FText::FromString(Description);Count(R,TEXT("function_metadata_set_calls"));E->MetaData.SetMetaData(FBlueprintMetadata::MD_Tooltip,FString(Description));Count(R,TEXT("function_metadata_set_returns"));R.Data->SetObjectField(TEXT("configured_annotations"),NodeFields(E,R));Modified(BP,G,false,R);const bool Observed=Close(BP,G,E,Guid,Before,Old,0,Expected,R);
    R.Data->SetStringField(TEXT("description"),E->MetaData.ToolTip.ToString());R.Data->SetStringField(TEXT("function_name"),G->GetName());R.Data->SetBoolField(TEXT("immediate_generated_function_tooltip_verified"),false);
    if(R.Data->HasTypedField<EJson::Object>(TEXT("legacy_result"))) { const auto L=R.Data->GetObjectField(TEXT("legacy_result"));L->SetStringField(TEXT("description"),E->MetaData.ToolTip.ToString());L->SetStringField(TEXT("function_name"),G->GetName()); }
    const auto ClassAfter=Source(E->GetClass(),R),ParentAfter=Source(BP->ParentClass,R);R.Data->SetObjectField(TEXT("node_class_source_after"),ClassAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);const bool Sources=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter))&&Equal(MakeShared<FJsonValueObject>(ParentBefore),MakeShared<FJsonValueObject>(ParentAfter));R.Data->SetBoolField(TEXT("native_source_preserved"),Sources);R.bChanged|=!Sources;
    if(!Observed) { return R; }if(!Sources) { Fail(R,TEXT("annotation_native_source_failed"),TEXT("Native node/parent source differs; accepted metadata writes retained."));return R; }R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
