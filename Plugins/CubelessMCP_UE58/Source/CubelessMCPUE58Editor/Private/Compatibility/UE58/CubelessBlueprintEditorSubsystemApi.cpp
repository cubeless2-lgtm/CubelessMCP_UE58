// DG EditorUtility-only EditorSubsystem getter authoring; no runtime Actor owner.
#include "Compatibility/UE58/CubelessBlueprintEditorSubsystemApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "Engine/World.h"
#include "K2Node_GetSubsystem.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Subsystems/Subsystem.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintEditorSubsystem
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }

bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }

bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("module_readiness_observation_calls"),TEXT("class_lookup_calls"),TEXT("class_lookup_returns"),TEXT("class_load_calls"),TEXT("class_load_returns"),TEXT("class_source_observation_calls"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("utility_owner_observation_calls"),TEXT("utility_owner_observation_returns"),TEXT("external_scope_observation_calls"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("initialize_subsystem_class_calls"),TEXT("initialize_subsystem_class_returns"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("legacy_node_metadata_observation_calls"),TEXT("subsystem_class_storage_observation_calls"),TEXT("graph_compatibility_observation_calls"),TEXT("variable_type_observation_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("destroy_node_calls"),TEXT("module_load_calls"),TEXT("runtime_editor_subsystem_get_calls"),TEXT("explicit_cdo_construction_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Original Editor-only Add/GUID/Initialize/PostPlaced/Allocate/Reconstruct/Mark order; no added Modify/transactional flag/cleanup, explicit compile/save, module enable/load, subsystem instance getter or Utility Run. Shared resolver/load and SDK callbacks can affect dirty/status/cache/skeleton/CDO/children; direct counts are not all transitive effects."));
    R.Data->SetStringField(TEXT("owner_scope"),TEXT("Existing exact EditorUtilityBlueprint/native EditorUtilityObject or EditorUtilityWidgetBlueprint/native EditorUtilityWidget with an empty owned authored WidgetTree, public UnrealEd classifier true and Blutility already loaded. Widget preview/external instances are not created and must be absent. Intentional Editor references belong only to this Editor-only owner, never a runtime Actor Blueprint. Populated WidgetTree, generated/custom Utility parents, runtime Actor and broader original variants remain incomplete."));
    R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);R.Data->SetBoolField(TEXT("graph_creation_parity_verified"),false);
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

bool Forbidden(const FString& Text)
{
    const FString S=Text.TrimStartAndEnd().Replace(TEXT("\\"),TEXT("/")).ToLower();
    for(const TCHAR* Token:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor")}) { if(S.Contains(Token)) { return true; } }return false;
}

TSharedRef<FJsonObject> ClassSource(UClass* Class,FResult& R)
{
    Count(R,TEXT("class_source_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Class->GetPathName());J->SetStringField(TEXT("object_class"),Class->GetClass()->GetPathName());J->SetStringField(TEXT("flags"),LexToString(static_cast<uint32>(Class->GetClassFlags())));J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(Class->GetFlags())));J->SetBoolField(TEXT("package_dirty"),Class->GetOutermost()->IsDirty());
    TArray<TSharedPtr<FJsonValue>> Hierarchy;int32 Depth=0;
    for(UClass* C=Class;C&&Depth<64;C=C->GetSuperClass(),++Depth)
    { auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),C->GetPathName());Row->SetStringField(TEXT("flags"),LexToString(static_cast<uint32>(C->GetClassFlags())));for(const TCHAR* Key:{TEXT("BlueprintType"),TEXT("NotBlueprintType"),TEXT("DontUseGenericSpawnObject")}) { Row->SetBoolField(Key,C->GetBoolMetaData(Key)); }Hierarchy.Add(MakeShared<FJsonValueObject>(Row)); }
    J->SetArrayField(TEXT("hierarchy"),Hierarchy);return J;
}

// ADE: one Editor-only owner extension through the original subsystem writer.
// Empty authored tree is a finite admitted profile, not a claim about arbitrary UI.
struct FUtilityWidgetOwner
{
    FResult& R;bool Active=false,Ready=false,Stopped=false;
    TStrongObjectPtr<UWidgetBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UWidgetTree> Tree;
    TStrongObjectPtr<UClass> AssetClass,Parent;TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    TObjectPtr<UWidgetTree> TreeSlot=nullptr;FString BlueprintPath,GraphPath,TreePath;FGuid GraphGuid;EObjectFlags TreeFlags=RF_NoFlags;
    FObjectProperty* Preview=nullptr;TObjectPtr<UClass> PreviewClassSlot=nullptr;TArray<FProperty*> ClassFields;uint64 PreviewFlags=0;int32 PreviewOffset=0,PreviewSize=0;
    explicit FUtilityWidgetOwner(FResult& In):R(In){}
    bool Stop(const TCHAR* Property)
    {
        if(!Stopped){Stopped=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),BlueprintPath);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),TEXT("captured Editor-only owner and existing empty authored tree/preview slot"));J->SetStringField(TEXT("actual"),TEXT("changed or unavailable"));R.Data->SetObjectField(TEXT("utility_widget_first_mismatch"),J);Fail(R,TEXT("editor_subsystem_widget_owner_changed"),TEXT("Editor Utility Widget owner/tree/current role changed; retained prefix, no next SDK, compile/save, cleanup or retry."));}
        R.bChangeStateKnown=false;return false;
    }
    bool Current()
    {
        if(!Active)return true;if(Stopped||!Ready)return false;
        auto* B=Blueprint.Get();auto* G=Graph.Get();
        if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage()
            ||GEditor!=Editor.Get()||!Loaded(Editor.Get())||!Loaded(World.Get())||Editor->GetEditorWorldContext().World()!=World.Get()||World->bIsTearingDown
            ||!Loaded(AssetClass.Get())||!Loaded(Parent.Get())||!Loaded(B)||!Loaded(Package.Get())||B->GetPackage()!=Package.Get()||B->GetOuter()!=Package.Get()||B->GetClass()!=AssetClass.Get()
            ||!B->GetPathName().Equals(BlueprintPath,ESearchCase::CaseSensitive)||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().GetHandle()!=ParentSlot.GetHandle()
            ||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified)return Stop(TEXT("owner_context"));
        if(!Loaded(G)||G->GetOuter()!=B||G->GetPackage()!=Package.Get()||!G->GetPathName().Equals(GraphPath,ESearchCase::CaseSensitive)||G->GraphGuid!=GraphGuid
            ||(!B->UbergraphPages.Contains(G)&&!B->FunctionGraphs.Contains(G)&&!B->MacroGraphs.Contains(G)&&!B->DelegateSignatureGraphs.Contains(G)))return Stop(TEXT("graph_owner_membership"));
        TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(AssetClass.Get(),EFieldIterationFlags::IncludeDeprecated);It;++It)Now.Add(*It);
        if(Now!=ClassFields)return Stop(TEXT("preview_descriptor_inventory"));
        if(!Preview||Preview->GetClass()!=FObjectProperty::StaticClass()||Preview->ArrayDim!=1||Preview->GetOwnerStruct()!=AssetClass.Get()
            ||Preview->GetOffset_ForInternal()!=PreviewOffset||Preview->GetSize()!=PreviewSize||uint64(Preview->GetPropertyFlags())!=PreviewFlags||Preview->HasSetterOrGetter()
            ||!Preview->PropertyClass.IsResolved()||Preview->PropertyClass.GetHandle()!=PreviewClassSlot.GetHandle())return Stop(TEXT("preview_descriptor"));
        const TObjectPtr<UObject> PreviewValue=Preview->GetObjectPtrPropertyValue_InContainer(B);if(!PreviewValue.IsResolved()||!IsObjectHandleNull(PreviewValue.GetHandle()))return Stop(TEXT("existing_preview_must_remain_absent"));
        if(!B->WidgetTree.IsResolved()||B->WidgetTree.GetHandle()!=TreeSlot.GetHandle()||!Loaded(Tree.Get())||Tree->GetClass()!=UWidgetTree::StaticClass()
            ||Tree->GetOuter()!=B||Tree->GetPackage()!=Package.Get()||Tree->GetFlags()!=TreeFlags||!Tree->GetPathName().Equals(TreePath,ESearchCase::CaseSensitive)
            ||!Tree->RootWidget.IsResolved()||!IsObjectHandleNull(Tree->RootWidget.GetHandle())||!Tree->NamedSlotBindings.IsEmpty())return Stop(TEXT("authored_empty_widget_tree"));
        TArray<UObject*> Exports;GetObjectsWithOuter(Tree.Get(),Exports,EGetObjectsFlags::IncludeNestedObjects);if(!Exports.IsEmpty())return Stop(TEXT("authored_tree_owned_exports"));
        return true;
    }
    TObjectPtr<UClass> ParentSlot=nullptr;
    bool Bind(UBlueprint* B,UEdGraph* G,UClass* NativeAsset,UClass* NativeParent)
    {
        Active=NativeAsset&&NativeAsset->GetPathName().Equals(TEXT("/Script/Blutility.EditorUtilityWidgetBlueprint"),ESearchCase::CaseSensitive);if(!Active)return true;
        R.Data->SetBoolField(TEXT("EditorUtilityWidget_owner"),true);R.Data->SetStringField(TEXT("utility_widget_scope"),TEXT("Existing direct native Editor Utility Widget owner with empty authored WidgetTree/root/export set and absent existing preview. Tree identity, owner, object flags, root absence, preview descriptor/slot and graph membership are guarded at original SDK callback boundaries. No widget preview/CDO creation, Run, source compile/save or runtime owner support. Populated trees and generated/custom parents remain valid incomplete branches; compiler-generated classes and the rebuilt AllWidgets serialization cache are not invariants."));
        auto* W=Cast<UWidgetBlueprint>(B);if(!Loaded(W)||!Loaded(G)||W->GetOuter()!=W->GetPackage()||!W->WidgetTree.IsResolved())return Fail(R,TEXT("editor_subsystem_widget_source_pending"),TEXT("Loaded exact Utility Widget with resolved existing authored tree required; no tree creation or preview."));
        Blueprint.Reset(W);Graph.Reset(G);Package.Reset(W->GetPackage());AssetClass.Reset(NativeAsset);Parent.Reset(NativeParent);Editor.Reset(GEditor);World.Reset(GEditor?GEditor->GetEditorWorldContext().World():nullptr);
        BlueprintPath=W->GetPathName();GraphPath=G->GetPathName();GraphGuid=G->GraphGuid;ParentSlot=W->ParentClass.GetGCPtr();TreeSlot=W->WidgetTree;
        // This logical owner read is not part of the original node contract.
        // Resolve no handle and issue no tracked Get before the existing tree has
        // a strong hold: public path metadata + exact resident lookup suffice.
        const FString ExistingTreePath=TreeSlot.GetPathName();UObject* ExistingTree=ExistingTreePath.IsEmpty()?nullptr:StaticFindObject(UWidgetTree::StaticClass(),nullptr,*ExistingTreePath);
        if(!Loaded(ExistingTree)||ExistingTree->GetClass()!=UWidgetTree::StaticClass())return Fail(R,TEXT("editor_subsystem_widget_tree_pending"),TEXT("Exact resident authored WidgetTree required; no tracked resolve/load or tree creation."));
        Tree.Reset(static_cast<UWidgetTree*>(ExistingTree));const TObjectPtr<UWidgetTree> HeldTreeSlot(Tree.Get());
        if(!Loaded(W)||HeldTreeSlot.GetHandle()!=TreeSlot.GetHandle()||W->WidgetTree.GetHandle()!=TreeSlot.GetHandle()||!Loaded(Tree.Get())||Tree->GetClass()!=UWidgetTree::StaticClass()||Tree->GetOuter()!=W||Tree->GetPackage()!=Package.Get()||!Tree->RootWidget.IsResolved()||!IsObjectHandleNull(Tree->RootWidget.GetHandle())||!Tree->NamedSlotBindings.IsEmpty())return Fail(R,TEXT("editor_subsystem_widget_tree_pending"),TEXT("Existing owned WidgetTree without root, named-slot bindings or owned exports required; populated/wider UI authoring remains incomplete, no subsystem node writer."));
        TArray<UObject*> Exports;GetObjectsWithOuter(Tree.Get(),Exports,EGetObjectsFlags::IncludeNestedObjects);if(!Exports.IsEmpty())return Fail(R,TEXT("editor_subsystem_widget_tree_exports_pending"),TEXT("Owned WidgetTree exports require the later populated-tree observer; no subsystem node writer."));
        TreePath=Tree->GetPathName();TreeFlags=Tree->GetFlags();for(TFieldIterator<FProperty> It(NativeAsset,EFieldIterationFlags::IncludeDeprecated);It;++It)ClassFields.Add(*It);
        Preview=FindFProperty<FObjectProperty>(NativeAsset,TEXT("CreatedUMGWidget"));
        if(!Preview||Preview->GetClass()!=FObjectProperty::StaticClass()||Preview->GetOwnerStruct()!=NativeAsset||Preview->ArrayDim!=1||Preview->HasSetterOrGetter())return Fail(R,TEXT("editor_subsystem_widget_preview_descriptor_pending"),TEXT("Exact native reflected existing preview slot required; no getter, preview or fallback creation."));
        PreviewOffset=Preview->GetOffset_ForInternal();PreviewSize=Preview->GetSize();PreviewFlags=uint64(Preview->GetPropertyFlags());PreviewClassSlot=Preview->PropertyClass;
        if(PreviewOffset<0||PreviewSize!=sizeof(TObjectPtr<UObject>)||int64(PreviewOffset)+PreviewSize>NativeAsset->GetPropertiesSize()||!PreviewClassSlot.IsResolved()||PreviewClassSlot.GetHandle()!=ParentSlot.GetHandle())return Fail(R,TEXT("editor_subsystem_widget_preview_layout_pending"),TEXT("Existing preview storage must be an exact scalar resolved native UtilityWidget slot; no accessors or fallback value sampling."));
        Ready=true;return Current();
    }
};
bool AddAndAllocate(UEdGraph* G,UEdGraphNode* Node,const int32 (&XY)[2],UK2Node_GetSubsystem* Subsystem,UClass* Class,FResult& R,TFunction<bool(const TCHAR*)> Fence={})
{
    if(Fence&&!Fence(TEXT("before_graph_add")))return false;
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());Node->NodePosX=XY[0];Node->NodePosY=XY[1];Count(R,TEXT("graph_add_node_calls"));G->AddNode(Node,true);if(Fence&&!Fence(TEXT("graph_add_return")))return false;
    Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();R.Data->SetStringField(TEXT("created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));if(Fence&&!Fence(TEXT("guid_return")))return false;
    if(Subsystem) { Count(R,TEXT("initialize_subsystem_class_calls"));Subsystem->Initialize(Class);Count(R,TEXT("initialize_subsystem_class_returns"));if(Fence&&!Fence(TEXT("initialize_return")))return false; }
    Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();if(Fence&&!Fence(TEXT("post_placed_return")))return false;
    Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();if(Fence&&!Fence(TEXT("allocate_pins_return")))return false;
    if(Subsystem) { Count(R,TEXT("reconstruct_calls"));Node->ReconstructNode();if(Fence&&!Fence(TEXT("reconstruct_return")))return false; }return true;
}
UClass* ExactLoadedClass(const TCHAR* Path,FResult& R)
{
    Count(R,TEXT("class_lookup_calls"));UClass* C=FindObject<UClass>(nullptr,Path);Count(R,TEXT("class_lookup_returns"));
    if(!Loaded(C)||!C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists))
    { Fail(R,TEXT("editor_subsystem_class_readiness_pending"),FString(TEXT("Exact already-loaded native class required: "))+Path);return nullptr; }
    return C;
}
UClass* ResolveSubsystem(const FString& Input,UClass* EditorBase,FResult& R)
{
    if(!Function::Pin::SafeName(Input)||Forbidden(Input)) { Fail(R,TEXT("editor_subsystem_class_invalid_or_dependency"),TEXT("Original bounded subsystem class without an MCP-only reference required."));return nullptr; }
    FString Path=Input;Path.TrimStartAndEndInline();Path.TrimQuotesInline();UClass* Found=nullptr;
    Count(R,TEXT("class_lookup_calls"));
    if(Path.StartsWith(TEXT("/Script/"))) { Found=FindObject<UClass>(nullptr,*Path); }
    else if(!Path.Contains(TEXT("/"))&&!Path.Contains(TEXT(".")))
    {
        // Loaded-only unambiguous short-name adaptation; no unaudited module load.
        const FName Name(*Path);int32 Matches=0;
        for(TObjectIterator<UClass> It;It;++It) { if(It->GetFName()==Name) { Found=*It;++Matches; } }
        if(Matches!=1) { Found=nullptr; }
    }
    Count(R,TEXT("class_lookup_returns"));
    if(!Loaded(Found)||!Found->HasAnyClassFlags(CLASS_Native)||Found->ClassGeneratedBy||Found->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Found==EditorBase||!Found->IsChildOf(EditorBase))
    { Fail(R,TEXT("editor_subsystem_type_or_load_pending"),TEXT("Already-loaded nonabstract native UEditorSubsystem subclass required; generated/custom/abstract and cold-loader variants remain valid residuals."));return nullptr; }
    const FString Package=Found->GetOutermost()->GetName();
    if(!Package.StartsWith(TEXT("/Script/"))||Forbidden(Found->GetPathName())) { Fail(R,TEXT("editor_subsystem_class_scope_pending"),TEXT("Native Editor subsystem source package/reference is outside the original guarded scope."));return nullptr; }
    const FString Module=Package.Mid(8);Count(R,TEXT("module_readiness_observation_calls"));const bool Ready=FModuleManager::Get().IsModuleLoaded(FName(*Module));R.Data->SetStringField(TEXT("subsystem_module"),Module);R.Data->SetBoolField(TEXT("subsystem_module_loaded"),Ready);
    if(!Ready) { Fail(R,TEXT("editor_subsystem_module_pending"),TEXT("Owning module must already be loaded; no subsystem initialization/module load is forced."));return nullptr; }
    Count(R,TEXT("variable_type_observation_calls"));const bool Allowable=UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Found,true);R.Data->SetBoolField(TEXT("subsystem_variable_type_allowable"),Allowable);
    if(!Allowable) { Fail(R,TEXT("editor_subsystem_variable_type_pending"),TEXT("Public SDK Editor subsystem menu variable-type admission failed."));return nullptr; }
    R.Data->SetStringField(TEXT("resolved_subsystem_class"),Found->GetPathName());const TStrongObjectPtr<UClass> KeepFound(Found);
    // Preserve one original LoadClassForPin load on the exact already-loaded identity.
    Count(R,TEXT("class_load_calls"));UClass* Class=LoadObject<UClass>(nullptr,*Found->GetPathName());Count(R,TEXT("class_load_returns"));
    if(Class!=Found||!Loaded(Class)) { R.bChangeStateKnown=false;Fail(R,TEXT("editor_subsystem_class_load_unverified"),TEXT("Original class load returned a different/unready identity; actual load prefix retained with no retry."));return nullptr; }
    return Class;
}
bool UtilityOwner(UBlueprint* BP,UEdGraph* G,UClass* AssetClass,UClass* ParentClass,FResult& R)
{
    Count(R,TEXT("utility_owner_observation_calls"));const bool Utility=Loaded(BP)&&FBlueprintEditorUtils::IsEditorUtilityBlueprint(BP);Count(R,TEXT("utility_owner_observation_returns"));R.Data->SetBoolField(TEXT("public_EditorUtility_classifier"),Utility);
    if(!Utility||BP->GetClass()!=AssetClass||BP->ParentClass!=ParentClass||!Loaded(G)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP||BP->BlueprintType!=BPTYPE_Normal
        ||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated
        ||!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>=2048)
    { return Fail(R,TEXT("editor_subsystem_utility_owner_pending"),TEXT("Existing idle exact EditorUtilityBlueprint/native EditorUtilityObject or EditorUtilityWidgetBlueprint/native EditorUtilityWidget owner and editable K2 graph required. Runtime Actor and broader Utility owners are not substituted.")); }
    Count(R,TEXT("external_scope_observation_calls"));TArray<TSharedPtr<FJsonValue>> External;
    for(TObjectIterator<UBlueprint> It;It;++It) { if(*It!=BP&&It->ParentClass&&BP->GeneratedClass&&It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.Add(MakeShared<FJsonValueString>(It->GetPathName())); } }
    if(BP->GeneratedClass)
    { TArray<UObject*> Instances;GetObjectsOfClass(BP->GeneratedClass,Instances,true,RF_NoFlags);for(UObject* O:Instances) { if(IsValid(O)&&O->GetOutermost()!=BP->GetOutermost()) { External.Add(MakeShared<FJsonValueString>(O->GetPathName())); } } }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"),External);
    return External.IsEmpty()||Fail(R,TEXT("editor_subsystem_external_instances_pending"),TEXT("External Utility instances/derived Blueprint callback effects are outside selected preservation."));
}
bool Graph(const FString& Path,const FString& Name,const FString& Id,const FString& Type,bool Create,UClass*& AssetClass,UClass*& ParentClass,UBlueprint*& BP,UEdGraph*& G,TSharedPtr<FJsonObject>& Before,FResult& R)
{
    R.Stage=TEXT("resolve_graph");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(Path,Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));R.bMutationAttempted=S.bMutationAttempted;R.bChanged=S.bChanged;R.bChangeStateKnown=R.bChangeStateKnown&&S.bChangeStateKnown;
    if(!S.bSuccess||!S.bPostconditionPassed) { return Fail(R,S.ErrorCode.IsEmpty()?TEXT("editor_subsystem_graph_unverified"):*S.ErrorCode,S.Error); }
    BP=FindObject<UBlueprint>(nullptr,*S.Data->GetStringField(TEXT("blueprint")));const auto J=S.Data->GetObjectField(TEXT("graph"));G=FindObject<UEdGraph>(nullptr,*J->GetStringField(TEXT("object_path")));Before=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetStringField(TEXT("blueprint"),S.Data->GetStringField(TEXT("blueprint")));
    if(Loaded(BP)&&BP->GetClass()->GetPathName().Equals(TEXT("/Script/Blutility.EditorUtilityWidgetBlueprint"),ESearchCase::CaseSensitive))
    {
        AssetClass=ExactLoadedClass(TEXT("/Script/Blutility.EditorUtilityWidgetBlueprint"),R);ParentClass=ExactLoadedClass(TEXT("/Script/Blutility.EditorUtilityWidget"),R);
        if(!AssetClass||!ParentClass)return false;
    }
    if(!UtilityOwner(BP,G,AssetClass,ParentClass,R)) { return false; }
    if(FBlueprintEditorUtils::FindUserConstructionScript(BP)==G) { return Fail(R,TEXT("editor_subsystem_construction_graph_pending"),TEXT("Public subsystem node compatibility excludes construction graph; original incompatible allocations remain tracked.")); }
    return true;
}
bool After(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const FGuid& Guid,const TSharedPtr<FJsonObject>& Before,FResult& R)
{
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(BP->GetPathName(),TEXT(""),Guid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(S));
    if(!S.bSuccess||!S.bChangeStateKnown||!S.bPostconditionPassed) { R.bChangeStateKnown=false;return Fail(R,TEXT("editor_subsystem_after_incomplete"),TEXT("Current graph observation did not close; original allocation/lifecycle prefix retained, no retry or cleanup.")); }
    const auto A=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),A);R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true;
    const auto* Row=A->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(Row) { R.Data->SetObjectField(TEXT("node"),(*Row)->AsObject());for(const auto& P:(*Row)->AsObject()->Values) { R.Data->SetField(P.Key,P.Value); }LegacyNodeFields(Node,R); }
    const bool Old=ExistingNodesPreserved(Before,A,1),Rows=GraphRowsPreserved(Before,A,G->GetPathName(),1);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);R.Data->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));
    return (Old&&Rows&&G->GraphGuid==Guid&&Row)||Fail(R,TEXT("editor_subsystem_graph_postcondition_failed"),TEXT("Current selected/old graph-node-pin preservation differs; actual state retained, no rollback claim."));
}
}
Function::FResult AddEditorSubsystem(const FString& Path,const FString& SubsystemClass,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_subsystem_class"),SubsystemClass);R.Data->SetBoolField(TEXT("requested_create_graph_if_missing"),Create);R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_GetEditorSubsystem"));
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("editor_subsystem_editor_invalid"),TEXT("Editor game thread outside PIE/SIE required, preserving original play-session refusal."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddEditorSubsystemNode","Add Blueprint Editor Subsystem"));Count(R,TEXT("module_readiness_observation_calls"));const bool Blutility=FModuleManager::Get().IsModuleLoaded(TEXT("Blutility"));R.Data->SetBoolField(TEXT("Blutility_module_loaded"),Blutility);
    if(!Blutility) { Fail(R,TEXT("editor_subsystem_Blutility_unloaded"),TEXT("Public EditorUtility classifier requires already-loaded Blutility; no enable/load or Runtime Actor fallback."));return R; }
    UClass* EditorBase=ExactLoadedClass(TEXT("/Script/EditorSubsystem.EditorSubsystem"),R);if(!EditorBase||!EditorBase->IsChildOf(USubsystem::StaticClass())) { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("editor_subsystem_base_layout_unverified"),TEXT("Loaded EditorSubsystem base must derive from exported Engine USubsystem.")); }return R; }const TStrongObjectPtr<UClass> KeepEditorBase(EditorBase);
    UClass* Class=ResolveSubsystem(SubsystemClass,EditorBase,R);if(!Class) { return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto SourceBefore=ClassSource(Class,R);R.Data->SetObjectField(TEXT("class_source_before"),SourceBefore);
    int32 XY[2]={0,0};FString PositionText;if(!Position(PositionJson,XY,PositionText,R)) { return R; }
    UClass* AssetClass=ExactLoadedClass(TEXT("/Script/Blutility.EditorUtilityBlueprint"),R);UClass* ParentClass=ExactLoadedClass(TEXT("/Script/Blutility.EditorUtilityObject"),R);if(!AssetClass||!ParentClass) { return R; }if(!AssetClass->IsChildOf(UBlueprint::StaticClass())) { Fail(R,TEXT("editor_subsystem_utility_class_layout_unverified"),TEXT("Exact loaded EditorUtilityBlueprint class must derive from exported Engine UBlueprint."));return R; }const TStrongObjectPtr<UClass> KeepAssetClass(AssetClass),KeepParentClass(ParentClass);
    UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,Create,AssetClass,ParentClass,BP,G,Before,R)) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const TStrongObjectPtr<UClass> KeepActualAssetClass(AssetClass),KeepActualParentClass(ParentClass);const FGuid Guid=G->GraphGuid;
    FUtilityWidgetOwner WidgetOwner(R);if(!WidgetOwner.Bind(BP,G,AssetClass,ParentClass))return R;
    UClass* NodeClass=ExactLoadedClass(TEXT("/Script/BlueprintGraph.K2Node_GetEditorSubsystem"),R);if(!NodeClass||NodeClass->GetSuperClass()!=UK2Node_GetSubsystem::StaticClass()||NodeClass->HasAnyClassFlags(CLASS_Abstract)) { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("editor_subsystem_node_class_unavailable"),TEXT("Exact native EditorSubsystem node subclass of exported GetSubsystem base required.")); }return R; }const TStrongObjectPtr<UClass> KeepNodeClass(NodeClass);
    const auto NodeSource=ClassSource(NodeClass,R),OwnerSource=ClassSource(AssetClass,R),ParentSource=ClassSource(ParentClass,R),BaseSource=ClassSource(EditorBase,R);R.Data->SetObjectField(TEXT("node_class_source_before"),NodeSource);R.Data->SetObjectField(TEXT("owner_class_source_before"),OwnerSource);R.Data->SetObjectField(TEXT("parent_source_before"),ParentSource);R.Data->SetObjectField(TEXT("editor_base_source_before"),BaseSource);
    if(!WidgetOwner.Current())return R;
    R.Stage=TEXT("create_editor_subsystem_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_GetSubsystem>(G,NodeClass);Count(R,TEXT("node_allocation_returns"));const TStrongObjectPtr<UK2Node_GetSubsystem> KeepNode(Node);
    if(!Node) { Fail(R,TEXT("editor_subsystem_allocation_failed"),TEXT("Failed to create Get Editor Subsystem node; actual attempted prefix retained."));return R; }
    UEdGraphNode* Base=Node;
    const FString AllocatedNodePath=Node->GetPathName();FGuid AllocatedGuid;
    auto Fence=[&](const TCHAR* Phase)->bool
    {
        if(!WidgetOwner.Active)return true;if(!WidgetOwner.Current())return false;
        const bool Attached=FCString::Strcmp(Phase,TEXT("before_graph_add"))!=0;
        if(!Loaded(Node)||Node->GetClass()!=NodeClass||Node->GetOuter()!=G||Node->GetPackage()!=BP->GetPackage()||!Node->GetPathName().Equals(AllocatedNodePath,ESearchCase::CaseSensitive)||(Attached&&!G->Nodes.Contains(Node)))return WidgetOwner.Stop(Phase);
        if(FCString::Strcmp(Phase,TEXT("guid_return"))==0)AllocatedGuid=Node->NodeGuid;
        if(AllocatedGuid.IsValid()&&Node->NodeGuid!=AllocatedGuid)return WidgetOwner.Stop(TEXT("created_node_guid"));return true;
    };
    if(!Fence(TEXT("before_graph_add"))||!AddAndAllocate(G,Base,XY,Node,Class,R,Fence))return R;
    Mark(BP,R);if(!Fence(TEXT("mark_modified_return")))return R;const bool Observed=After(BP,G,Base,Guid,Before,R);if(!Fence(TEXT("after_observation_return")))return R;
    const auto SourceAfter=ClassSource(Class,R),NodeAfter=ClassSource(NodeClass,R),OwnerAfter=ClassSource(AssetClass,R),ParentAfter=ClassSource(ParentClass,R),BaseAfter=ClassSource(EditorBase,R);R.Data->SetObjectField(TEXT("class_source_after"),SourceAfter);R.Data->SetObjectField(TEXT("node_class_source_after"),NodeAfter);R.Data->SetObjectField(TEXT("owner_class_source_after"),OwnerAfter);R.Data->SetObjectField(TEXT("parent_source_after"),ParentAfter);R.Data->SetObjectField(TEXT("editor_base_source_after"),BaseAfter);
    if(!WidgetOwner.Current())return R;
    const bool Same=Equal(MakeShared<FJsonValueObject>(SourceBefore),MakeShared<FJsonValueObject>(SourceAfter))&&Equal(MakeShared<FJsonValueObject>(NodeSource),MakeShared<FJsonValueObject>(NodeAfter))&&Equal(MakeShared<FJsonValueObject>(OwnerSource),MakeShared<FJsonValueObject>(OwnerAfter))&&Equal(MakeShared<FJsonValueObject>(ParentSource),MakeShared<FJsonValueObject>(ParentAfter))&&Equal(MakeShared<FJsonValueObject>(BaseSource),MakeShared<FJsonValueObject>(BaseAfter));R.Data->SetBoolField(TEXT("native_class_sources_preserved"),Same);R.bChanged=R.bChanged||!Same;if(!Observed) { return R; }
    Count(R,TEXT("subsystem_class_storage_observation_calls"));const auto* P=FindFProperty<FClassProperty>(UK2Node_GetSubsystem::StaticClass(),TEXT("CustomClass"));const bool Layout=P&&P->GetOwnerStruct()==UK2Node_GetSubsystem::StaticClass()&&P->ArrayDim==1&&P->MetaClass==USubsystem::StaticClass();UObject* Stored=Layout?P->GetObjectPropertyValue_InContainer(Node):nullptr;R.Data->SetBoolField(TEXT("subsystem_class_storage_layout_verified"),Layout);R.Data->SetStringField(TEXT("subsystem_class_storage_actual"),GetPathNameSafe(Stored));
    const auto* Result=Node->GetResultPin();const bool Pins=FreshPins(Base)&&Base->Pins.Num()==1&&Node->GetClassPin()==nullptr&&Node->GetWorldContextPin()==nullptr&&Result&&Result->Direction==EGPD_Output&&Result->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Result->PinType.PinSubCategory.IsNone()&&Result->PinType.PinSubCategoryObject.Get()==Class&&!Result->PinType.IsContainer()&&!Result->PinType.bIsReference&&!Result->PinType.bIsConst&&!Result->PinType.bIsWeakPointer&&Result->DefaultValue.IsEmpty()&&!Result->DefaultObject&&Result->DefaultTextValue.IsEmpty()&&Node->IsNodePure();R.Data->SetBoolField(TEXT("editor_subsystem_pin_shape_verified"),Pins);R.Data->SetStringField(TEXT("subsystem_class"),Class->GetPathName());
    Count(R,TEXT("graph_compatibility_observation_calls"));const bool Compatible=Base->IsCompatibleWithGraph(G);R.Data->SetBoolField(TEXT("current_graph_compatible"),Compatible);const bool Utility=UtilityOwner(BP,G,AssetClass,ParentClass,R);R.Data->SetBoolField(TEXT("Editor_only_owner_reverified"),Utility);
    if(!Fence(TEXT("final_readback_return")))return R;
    if(!Loaded(Node)||Node->GetClass()!=NodeClass||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!BP->GetOutermost()->IsDirty()||Stored!=Class||!Layout||!Pins||!Same||!Compatible||!Utility)
    { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("editor_subsystem_postcondition_failed"),TEXT("Current Editor-only owner/class/default pins/position/source proof failed; original created node retained with no cleanup or replay.")); }return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
