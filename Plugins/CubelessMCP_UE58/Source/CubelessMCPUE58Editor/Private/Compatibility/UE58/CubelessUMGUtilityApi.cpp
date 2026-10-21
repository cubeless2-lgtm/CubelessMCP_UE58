#include "Compatibility/UE58/CubelessUMGUtilityApi.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "ScopedTransaction.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Blueprint/UserWidget.h"
#include "Components/CheckBox.h"
#include "UObject/EnumProperty.h"
namespace Cubeless::UE58::UMGUtility
{
namespace
{
using Variable::FResult;
bool Active=false;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool TextSafe(const FString& S,int32 Limit=65536) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(FResult& R,const TCHAR* K,int32 N=1) { auto J=R.Data->GetObjectField(TEXT("native_call_counts"));J->SetNumberField(K,J->GetNumberField(K)+N); }
void Init(FResult& R,const TCHAR* Operation,bool Writer)
{
    R.bCompileRequested=R.bSaveRequested=Writer;R.Data->SetStringField(TEXT("operation"),Operation);
    auto J=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("transaction_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("state_capture_calls"),TEXT("object_inventory_calls"),TEXT("name_collision_checks"),TEXT("unique_widget_name_calls"),TEXT("unique_widget_name_returns"),TEXT("widget_construct_calls"),TEXT("widget_construct_returns"),TEXT("widget_tree_modify_calls"),TEXT("root_widget_assignments"),TEXT("canvas_child_add_calls"),TEXT("canvas_child_add_returns"),TEXT("slot_position_set_calls"),TEXT("slot_size_set_calls"),TEXT("on_variable_added_calls"),TEXT("checkbox_set_calls"),TEXT("checkbox_property_reads"),TEXT("text_set_calls"),TEXT("slider_property_reads"),TEXT("mark_structural_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("generated_class_reads"),TEXT("runtime_widget_create_calls"),TEXT("runtime_add_to_viewport_calls")}) { J->SetNumberField(K,0); }R.Data->SetObjectField(TEXT("native_call_counts"),J);
    R.Data->SetBoolField(TEXT("total_SDK_side_effects_verified"),false);R.Data->SetBoolField(TEXT("implicit_structural_skeleton_compile_possible"),Writer);R.Data->SetBoolField(TEXT("runtime_widget_or_viewport_execution_verified"),false);R.Data->SetBoolField(TEXT("cold_saved_readback_verified"),false);R.Data->SetBoolField(TEXT("auto_retry_safe"),false);
}
void Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.ErrorCode=Code;R.Error=Error; }
bool Admit(FResult& R,const TArray<FString>& Strings)
{
    if(!GEditor||!IsInGameThread()||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor||Active) { Fail(R,TEXT("umg_editor_state_invalid"),TEXT("Editor game thread outside PIE/SIE and nonreentrant call required"));return false; }
    for(const FString& S:Strings) { if(!TextSafe(S,131072)) { Fail(R,TEXT("umg_input_text_invalid"),TEXT("Bounded strings without interior NUL required"));return false; } }
    return true;
}
struct FActiveScope { FActiveScope() { Active=true; }~FActiveScope() { Active=false; } };
FString Json(const TSharedRef<FJsonObject>& J) { FString S;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&S));return S; }
void Vec(const TSharedRef<FJsonObject>& J,const TCHAR* K,const FVector2D& V) { J->SetArrayField(K,{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y)}); }
bool JsonFinite(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Visits)
{
    if(!V.IsValid()||Depth>128||++Visits>65536) { return false; }
    if(V->Type==EJson::Number) { return FMath::IsFinite(V->AsNumber()); }
    if(V->Type==EJson::Array) { for(const auto& A:V->AsArray()) { if(!JsonFinite(A,Depth+1,Visits)) { return false; } } }
    if(V->Type==EJson::Object) { for(const auto& A:V->AsObject()->Values) { if(!JsonFinite(A.Value,Depth+1,Visits)) { return false; } } }
    return true;
}
bool BoxValue(const FString& Text,TSharedPtr<FJsonValue>& V,FResult& R)
{
    if(!TextSafe(Text,131072)) { Fail(R,TEXT("umg_array_carrier_invalid"),TEXT("Bounded whole JSON carrier required"));return false; }
    TSharedPtr<FJsonObject> Box;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("{\"value\":")+Text+TEXT("}")),Box)||!Box.IsValid()) { Fail(R,TEXT("umg_array_carrier_invalid"),TEXT("Invalid whole JSON carrier"));return false; }
    V=Box->TryGetField(TEXT("value"));int32 Visits=0;if(!JsonFinite(V,0,Visits)) { Fail(R,TEXT("umg_array_carrier_nonfinite"),TEXT("Whole JSON carrier must remain finite and within bounded serializer scope"));return false; }return true;
}
FString NormalizeWidgetAssetPath(FString BlueprintNameOrPath)
{
	BlueprintNameOrPath = FPackageName::ExportTextPathToObjectPath(BlueprintNameOrPath).TrimStartAndEnd();
	BlueprintNameOrPath.TrimQuotesInline();
	BlueprintNameOrPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (BlueprintNameOrPath.EndsWith(TEXT(".uasset"), ESearchCase::IgnoreCase))
	{
		BlueprintNameOrPath.LeftChopInline(7);
	}
	if (BlueprintNameOrPath.Contains(TEXT(".")))
	{
		BlueprintNameOrPath = FPackageName::ObjectPathToPackageName(BlueprintNameOrPath);
	}
	if (BlueprintNameOrPath.StartsWith(TEXT("/Game/")))
	{
		return BlueprintNameOrPath;
	}
	if (BlueprintNameOrPath.StartsWith(TEXT("Game/")))
	{
		return TEXT("/") + BlueprintNameOrPath;
	}
	return TEXT("/Game/Widgets/") + BlueprintNameOrPath;
}
FString WidgetObjectPathFromPackage(const FString& PackageName)
{
	return PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName);
}
bool GamePackage(const FString& Package,FResult& R)
{
    FText Why;
    if(Package.Len()>=NAME_SIZE||WidgetObjectPathFromPackage(Package).Len()>=NAME_SIZE) { Fail(R,TEXT("umg_target_path_name_too_long"),TEXT("Composed package and object path must be shorter than NAME_SIZE before exists/load/create; no allocation or save attempted"));return false; }
    if(!Package.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(Package,false,&Why)) { Fail(R,TEXT("umg_target_path_invalid"),TEXT("Mutable Widget Blueprint must use a valid /Game package path"));return false; }return true;
}
bool ToolingPackage(const UObject* O)
{
    if(!O) { return false; }const FString P=O->GetOutermost()->GetName();
    for(const TCHAR* Bad:{TEXT("/Script/Cubeless"),TEXT("/Script/UnrealMCP"),TEXT("/Script/MCPUnreal"),TEXT("/Script/MCP_Unreal"),TEXT("/Script/ToolsetRegistry"),TEXT("/Script/ModelContextProtocol"),TEXT("/Script/PythonScriptPlugin")}) { if(P.StartsWith(Bad,ESearchCase::IgnoreCase)) { return true; } }return false;
}
bool NameValid(const FString& Name,bool Empty,FResult& R)
{
    if(Name.Len()>=NAME_SIZE||!TextSafe(Name,1024)||(!Empty&&Name.IsEmpty())||(!Name.IsEmpty()&&!FName(*Name).IsValidXName(INVALID_OBJECTNAME_CHARACTERS))) { Fail(R,TEXT("umg_widget_name_invalid"),TEXT("Bounded valid UObject name required; empty child names retain automatic-name semantics"));return false; }return true;
}
bool FreeName(UWidgetBlueprint* BP,UWidgetTree* Tree,const FName Name,FResult& R)
{
    Count(R,TEXT("name_collision_checks"));
    if(!Loaded(Tree)||StaticFindObjectFast(UObject::StaticClass(),Tree,Name)||BP->WidgetVariableNameToGuidMap.Contains(Name)) { Fail(R,TEXT("umg_widget_name_collision"),TEXT("Existing tree object or variable GUID name refused before any allocation/Modify/compile/save; avoids original NewObject replacement/fatal path"));return false; }return true;
}
UWidgetBlueprint* Load(const FString& Name,FString& Package,FResult& R)
{
    Package=NormalizeWidgetAssetPath(Name);if(!GamePackage(Package,R)) { return nullptr; }
    Count(R,TEXT("asset_load_calls"));UWidgetBlueprint* BP=Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(Package));Count(R,TEXT("asset_load_returns"));
    if(!BP) { Count(R,TEXT("asset_load_calls"));BP=Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetObjectPathFromPackage(Package)));Count(R,TEXT("asset_load_returns")); }
    if(!Loaded(BP)||!Loaded(BP->WidgetTree)) { R.bChangeStateKnown=BP==nullptr;Fail(R,TEXT("umg_blueprint_not_found"),FString::Printf(TEXT("Failed to load Widget Blueprint/tree: %s"),*Package));return nullptr; }
    if(BP->GetOutermost()->GetName()!=Package||BP->WidgetTree->GetOuter()!=BP||ToolingPackage(BP->GetClass())) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_widget_tree_owner_invalid"),TEXT("Current package and source WidgetTree ownership do not match"));return nullptr; }
    R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());return BP;
}
void PublishWidget(FResult& R,const TCHAR* Key,UObject* O)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("returned"),O!=nullptr);if(Loaded(O)) { J->SetStringField(TEXT("path"),O->GetPathName());J->SetStringField(TEXT("class"),O->GetClass()->GetPathName());J->SetStringField(TEXT("name"),O->GetName()); }R.Data->SetObjectField(Key,J);
}
UCanvasPanel* Canvas(UWidgetBlueprint* BP,FResult& R)
{
    UWidgetTree* Tree=BP->WidgetTree;UCanvasPanel* Root=Cast<UCanvasPanel>(Tree->RootWidget);
    if(!Root)
    {
        R.bMutationAttempted=true;Count(R,TEXT("widget_tree_modify_calls"));Tree->Modify();Count(R,TEXT("widget_construct_calls"));Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas"));Count(R,TEXT("widget_construct_returns"));PublishWidget(R,TEXT("root_canvas_return"),Root);if(!Loaded(Root)) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_canvas_creation_failed"),TEXT("Canvas construction did not return a loaded source widget; actual prefix retained"));return nullptr; }
        Tree->RootWidget=Root;Count(R,TEXT("root_widget_assignments"));R.bChanged=true;Count(R,TEXT("on_variable_added_calls"));BP->OnVariableAdded(Root->GetFName());
    }
    return Root;
}

bool TryGetStringFieldAny(const TSharedPtr<FJsonObject>& Params, const TArray<FString>& FieldNames, FString& OutValue)
{
	if (!Params.IsValid())
	{
		return false;
	}

	for (const FString& FieldName : FieldNames)
	{
		if (Params->TryGetStringField(FieldName, OutValue))
		{
			OutValue.TrimStartAndEndInline();
			return true;
		}
	}
	return false;
}

bool SliderValues(USlider* S,const TSharedRef<FJsonObject>& J,FResult& R)
{
    if(!Loaded(S)) { return false; }
    for(const TCHAR* K:{TEXT("MinValue"),TEXT("MaxValue"),TEXT("Value"),TEXT("StepSize")})
    {
        Count(R,TEXT("slider_property_reads"));const FFloatProperty* P=FindFProperty<FFloatProperty>(S->GetClass(),K);if(!P||P->GetOffset_ForInternal()<0||P->GetOffset_ForInternal()>S->GetClass()->GetStructureSize()-static_cast<int32>(sizeof(float))) { return false; }const float V=P->GetPropertyValue_InContainer(S);if(!FMath::IsFinite(V)) { return false; }J->SetNumberField(K,V);
    }
    J->SetStringField(TEXT("orientation"),S->GetOrientation()==Orient_Horizontal?TEXT("horizontal"):TEXT("vertical"));J->SetBoolField(TEXT("indent_handle"),S->HasIndentHandle());J->SetBoolField(TEXT("locked"),S->IsLocked());J->SetBoolField(TEXT("mouse_uses_step"),S->MouseUsesStep);J->SetBoolField(TEXT("requires_controller_lock"),S->RequiresControllerLock);J->SetBoolField(TEXT("focusable"),S->IsFocusable);return true;
}
bool CheckboxValue(UCheckBox* C,int64& Value,FResult& R)
{
    Count(R,TEXT("checkbox_property_reads"));FProperty* P=FindFProperty<FProperty>(C->GetClass(),TEXT("CheckedState"));if(!P||P->GetOffset_ForInternal()<0||P->GetOffset_ForInternal()>C->GetClass()->GetStructureSize()-P->GetSize()) { return false; }
    if(FEnumProperty* E=CastField<FEnumProperty>(P)) { const FNumericProperty* N=E->GetUnderlyingProperty();if(!Loaded(E->GetEnum())||!N) { return false; }Value=N->GetSignedIntPropertyValue(P->ContainerPtrToValuePtr<void>(C)); }
    else if(FByteProperty* B=CastField<FByteProperty>(P)) { if(!Loaded(B->Enum)) { return false; }Value=B->GetUnsignedIntPropertyValue_InContainer(C); }
    else { return false; }
    return Value==static_cast<int64>(ECheckBoxState::Unchecked)||Value==static_cast<int64>(ECheckBoxState::Checked)||Value==static_cast<int64>(ECheckBoxState::Undetermined);
}
TSharedRef<FJsonObject> Capture(UWidgetBlueprint* BP,FResult& R,bool& OK)
{
    Count(R,TEXT("state_capture_calls"));auto J=MakeShared<FJsonObject>();OK=false;if(!Loaded(BP)||!Loaded(BP->ParentClass.Get())||!Loaded(BP->WidgetTree)||BP->WidgetTree->GetOuter()!=BP) { return J; }UWidgetTree* Tree=BP->WidgetTree;
    if(Tree->RootWidget&&(!Loaded(Tree->RootWidget)||Tree->RootWidget->GetOuter()!=Tree)) { return J; }J->SetStringField(TEXT("blueprint"),BP->GetPathName());J->SetStringField(TEXT("asset_class"),BP->GetClass()->GetPathName());J->SetStringField(TEXT("parent_class"),BP->ParentClass->GetPathName());J->SetStringField(TEXT("widget_tree"),Tree->GetPathName());J->SetStringField(TEXT("root"),Tree->RootWidget?Tree->RootWidget->GetPathName():FString());
    TArray<UObject*> Objects;Count(R,TEXT("object_inventory_calls"));GetObjectsWithOuter(Tree,Objects,EGetObjectsFlags::IncludeNestedObjects);if(Objects.Num()>4096) { return J; }auto Widgets=MakeShared<FJsonObject>();
    for(UObject* O:Objects)
    {
        if(!Loaded(O)) { return J; }UWidget* W=Cast<UWidget>(O);if(!W) { continue; }if(!W->IsIn(Tree)||ToolingPackage(W->GetClass())||(W->Slot&&!Loaded(W->Slot))) { return J; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("path"),W->GetPathName());Q->SetStringField(TEXT("class"),W->GetClass()->GetPathName());Q->SetBoolField(TEXT("is_variable"),W->bIsVariable);Q->SetStringField(TEXT("slot"),W->Slot?W->Slot->GetPathName():FString());
        if(UPanelWidget* Panel=Cast<UPanelWidget>(W)) { if(Panel->GetChildrenCount()>4096) { return J; }TArray<TSharedPtr<FJsonValue>> Children;for(int32 I=0;I<Panel->GetChildrenCount();++I) { UWidget* C=Panel->GetChildAt(I);if(!Loaded(C)||C->GetOuter()!=Tree) { return J; }Children.Add(MakeShared<FJsonValueString>(C->GetPathName())); }Q->SetArrayField(TEXT("children"),Children); }
        if(UTextBlock* T=Cast<UTextBlock>(W)) { Q->SetStringField(TEXT("text"),T->GetText().ToString()); }
        if(UCanvasPanelSlot* S=Cast<UCanvasPanelSlot>(W->Slot)) { if(S->Content!=W||!Loaded(S->Parent)) { return J; }const auto P=S->GetPosition(),Z=S->GetSize();if(!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)||!FMath::IsFinite(Z.X)||!FMath::IsFinite(Z.Y)) { return J; }Vec(Q,TEXT("position"),P);Vec(Q,TEXT("size"),Z); }
        if(USlider* S=Cast<USlider>(W)) { auto Values=MakeShared<FJsonObject>();if(!SliderValues(S,Values,R)) { return J; }Q->SetObjectField(TEXT("slider"),Values); }if(UCheckBox* C=Cast<UCheckBox>(W)) { int64 State=0;if(!CheckboxValue(C,State,R)) { return J; }Q->SetNumberField(TEXT("checked_state"),static_cast<double>(State)); }Widgets->SetObjectField(W->GetPathName(),Q);
    }
    J->SetObjectField(TEXT("widget_rows"),Widgets);auto Guids=MakeShared<FJsonObject>();for(const auto& V:BP->WidgetVariableNameToGuidMap) { Guids->SetStringField(V.Key.ToString(),V.Value.ToString()); }J->SetObjectField(TEXT("variable_guids"),Guids);
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);if(Graphs.Num()>4096) { return J; }TSet<const UEdGraphPin*> Pins;int32 NodeCount=0,PinCount=0;
    for(UEdGraph* G:Graphs) { if(!Loaded(G)||G->Nodes.Num()>65536) { return J; }for(UEdGraphNode* N:G->Nodes) { if(!Loaded(N)||++NodeCount>65536||N->Pins.Num()>65536) { return J; }for(UEdGraphPin* P:N->Pins) { if(!P||++PinCount>262144||Pins.Contains(P)) { return J; }Pins.Add(P); } } }
    auto GraphRows=MakeShared<FJsonObject>();
    for(UEdGraph* G:Graphs)
    {
        auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("class"),G->GetClass()->GetPathName());Q->SetStringField(TEXT("guid"),G->GraphGuid.ToString());auto Nodes=MakeShared<FJsonObject>();
        for(UEdGraphNode* N:G->Nodes)
        {
            if(Nodes->HasField(N->NodeGuid.ToString())) { return J; }auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("class"),N->GetClass()->GetPathName());A->SetNumberField(TEXT("x"),N->NodePosX);A->SetNumberField(TEXT("y"),N->NodePosY);TArray<TSharedPtr<FJsonValue>> PinRows;
            for(UEdGraphPin* P:N->Pins)
            {
                if(P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->LinkedTo.Num()>262144) { return J; }auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),P->PinName.ToString());V->SetStringField(TEXT("id"),P->PinId.ToString());V->SetNumberField(TEXT("direction"),P->Direction);V->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());V->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());V->SetStringField(TEXT("type_object"),P->PinType.PinSubCategoryObject.IsValid()?P->PinType.PinSubCategoryObject->GetPathName():FString());V->SetNumberField(TEXT("container"),static_cast<int32>(P->PinType.ContainerType));V->SetBoolField(TEXT("const"),P->PinType.bIsConst);V->SetBoolField(TEXT("reference"),P->PinType.bIsReference);V->SetStringField(TEXT("default_value"),P->DefaultValue);V->SetStringField(TEXT("default_text"),P->DefaultTextValue.ToString());V->SetStringField(TEXT("autogenerated_default"),P->AutogeneratedDefaultValue);V->SetBoolField(TEXT("default_ignored"),P->bDefaultValueIsIgnored);if(P->DefaultObject&&!Loaded(P->DefaultObject)) { return J; }V->SetStringField(TEXT("default_object"),P->DefaultObject?P->DefaultObject->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> Links;
                for(UEdGraphPin* L:P->LinkedTo) { if(!Pins.Contains(L)||L->WasTrashed()) { return J; }UEdGraphNode* Owner=L->GetOwningNodeUnchecked();if(!Loaded(Owner)||!Owner->Pins.Contains(L)) { return J; }auto Link=MakeShared<FJsonObject>();Link->SetStringField(TEXT("node"),Owner->NodeGuid.ToString());Link->SetStringField(TEXT("pin"),L->PinName.ToString());Link->SetStringField(TEXT("pin_id"),L->PinId.ToString());Links.Add(MakeShared<FJsonValueObject>(Link)); }V->SetArrayField(TEXT("links"),Links);PinRows.Add(MakeShared<FJsonValueObject>(V));
            }
            A->SetArrayField(TEXT("pins"),PinRows);Nodes->SetObjectField(N->NodeGuid.ToString(),A);
        }
        Q->SetObjectField(TEXT("nodes"),Nodes);GraphRows->SetObjectField(G->GetPathName(),Q);
    }
    J->SetObjectField(TEXT("graph_rows"),GraphRows);OK=true;return J;
}
bool ExactValue(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()) { return false; }FString X,Y;FJsonSerializer::Serialize(A,TEXT(""),TJsonWriterFactory<>::Create(&X));FJsonSerializer::Serialize(B,TEXT(""),TJsonWriterFactory<>::Create(&Y));return X.Equals(Y,ESearchCase::CaseSensitive);
}
bool Preserve(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,UCanvasPanel* Root,const TArray<FString>& Appended,const FString& AddedGraph,const FString& AddedNode,bool ReplacedRoot=false)
{
    for(const TCHAR* K:{TEXT("blueprint"),TEXT("asset_class"),TEXT("parent_class"),TEXT("widget_tree")}) { if(Before->GetStringField(K)!=After->GetStringField(K)) { return false; } }
    if(Before->GetStringField(TEXT("root"))!=After->GetStringField(TEXT("root"))&&!((Before->GetStringField(TEXT("root")).IsEmpty()||ReplacedRoot)&&Root&&!Appended.IsEmpty()&&After->GetStringField(TEXT("root"))==Root->GetPathName())) { return false; }
    for(const TCHAR* K:{TEXT("widget_rows"),TEXT("variable_guids"),TEXT("graph_rows")})
    {
        const auto A=Before->GetObjectField(K),B=After->GetObjectField(K);
        for(const auto& Pair:A->Values)
        {
            const FString Key(Pair.Key.ToView());auto V=B->TryGetField(Key);if(!V.IsValid()) { return false; }
            if(Root&&!Appended.IsEmpty()&&FString(K)==TEXT("widget_rows")&&Key==Root->GetPathName()) { const auto Old=Pair.Value->AsObject(),Current=V->AsObject();const auto& OC=Old->GetArrayField(TEXT("children"));const auto& NC=Current->GetArrayField(TEXT("children"));if(NC.Num()!=OC.Num()+Appended.Num()) { return false; }for(int32 I=0;I<Appended.Num();++I) { if(NC[OC.Num()+I]->AsString()!=Appended[I]) { return false; } }auto Copy=MakeShared<FJsonObject>(*Current);TArray<TSharedPtr<FJsonValue>> Retained=NC;Retained.SetNum(OC.Num());Copy->SetArrayField(TEXT("children"),Retained);V=MakeShared<FJsonValueObject>(Copy); }
            if(!AddedNode.IsEmpty()&&FString(K)==TEXT("graph_rows")&&Key==AddedGraph) { auto Copy=MakeShared<FJsonObject>(*V->AsObject());auto Nodes=MakeShared<FJsonObject>(*Copy->GetObjectField(TEXT("nodes")));Nodes->RemoveField(AddedNode);Copy->SetObjectField(TEXT("nodes"),Nodes);V=MakeShared<FJsonValueObject>(Copy); }
            if(!ExactValue(Pair.Value,V)) { return false; }
        }
    }
    return true;
}
bool Compile(UWidgetBlueprint* BP,FResult& R,const TCHAR* Phase)
{
    R.Stage=Phase;R.bMutationAttempted=R.bCompileAttempted=true;Count(R,TEXT("explicit_compile_calls"));FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);const bool Good=Log.NumErrors==0&&(BP->Status==BS_UpToDate||BP->Status==BS_UpToDateWithWarnings)&&Loaded(BP->GeneratedClass.Get());R.Compiled=Good;
    TArray<TSharedPtr<FJsonValue>> Rows;if(R.Data->HasField(TEXT("compiler_results"))) { Rows=R.Data->GetArrayField(TEXT("compiler_results")); }auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("phase"),Phase);C->SetNumberField(TEXT("errors"),Log.NumErrors);C->SetNumberField(TEXT("warnings"),Log.NumWarnings);C->SetNumberField(TEXT("blueprint_status"),static_cast<int32>(BP->Status));C->SetBoolField(TEXT("current_editor_compile_verified"),Good);Rows.Add(MakeShared<FJsonValueObject>(C));R.Data->SetArrayField(TEXT("compiler_results"),Rows);return Good;
}
void Save(UWidgetBlueprint* BP,const FString& Package,FResult& R)
{
    R.Stage=TEXT("structural_refresh");R.bMutationAttempted=true;Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP,R,TEXT("final_compile"));R.Stage=TEXT("save");R.bSaveAttempted=true;Count(R,TEXT("explicit_save_calls"));R.bSaved=UEditorAssetLibrary::SaveAsset(WidgetObjectPathFromPackage(Package),false);R.Data->SetBoolField(TEXT("save_returned"),true);R.Data->SetBoolField(TEXT("force_save_return"),R.bSaved);
}
TSharedRef<FJsonObject> End(UWidgetBlueprint* BP,FResult& R,const TSharedRef<FJsonObject>& Before,bool& OK)
{
    auto After=Capture(BP,R,OK);R.Data->SetObjectField(TEXT("after_state"),After);R.Data->SetBoolField(TEXT("package_dirty_after"),BP->GetOutermost()->IsDirty());if(!OK) { R.Data->SetStringField(TEXT("preceding_error_code"),R.ErrorCode);R.Data->SetStringField(TEXT("preceding_error"),R.Error);R.bChangeStateKnown=false;R.Stage=TEXT("observation_incomplete");Fail(R,TEXT("umg_utility_observation_incomplete"),TEXT("Actual accepted/native compiler/save prefix retained; current source inventory incomplete")); }
    else { R.bChangeStateKnown=true;R.bChanged|=!Json(Before).Equals(Json(After),ESearchCase::CaseSensitive); }return After;
}

struct FCheckboxGeometry
{
    bool HasPosition=false,HasSize=false;
    FVector2D Position=FVector2D::ZeroVector,Size=FVector2D::ZeroVector,LabelPosition=FVector2D::ZeroVector;
};
bool Pair(const TSharedPtr<FJsonObject>& Params,const TCHAR* Key,bool& Present,double& X,double& Y,FResult& R)
{
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;Present=Params->TryGetArrayField(Key,A)&&A->Num()>=2;if(!Present) { return true; }
    if(!(*A)[0].IsValid()||!(*A)[1].IsValid()) { Fail(R,TEXT("umg_checkbox_geometry_invalid"),TEXT("Geometry has an unavailable JSON element"));return false; }
    // Mirror original AsNumber's zero fallback without invoking its error logger.
    const bool XConverted=(*A)[0]->TryGetNumber(X),YConverted=(*A)[1]->TryGetNumber(Y);if(!XConverted) { X=0; }if(!YConverted) { Y=0; }
    auto Q=MakeShared<FJsonObject>();Q->SetBoolField(TEXT("applied"),true);Q->SetBoolField(TEXT("x_numeric_conversion"),XConverted);Q->SetBoolField(TEXT("y_numeric_conversion"),YConverted);Q->SetNumberField(TEXT("ignored_extra_elements"),A->Num()-2);R.Data->SetObjectField(FString(Key)+TEXT("_projection"),Q);
    if(!FMath::IsFinite(X)||!FMath::IsFinite(Y)||FMath::Abs(X)>static_cast<double>(MAX_flt)||FMath::Abs(Y)>static_cast<double>(MAX_flt)) { Fail(R,TEXT("umg_checkbox_float32_overflow"),TEXT("Geometry must remain finite after actual slot float32 projection"));return false; }return true;
}
bool Geometry(const TSharedPtr<FJsonObject>& Params,bool HasLabel,FCheckboxGeometry& G,FResult& R)
{
    double X=0,Y=0;if(!Pair(Params,TEXT("position"),G.HasPosition,X,Y,R)) { return false; }
    if(G.HasPosition)
    {
        G.Position=FVector2D(static_cast<float>(X),static_cast<float>(Y));if(HasLabel) { const double LX=X+34.0;if(!FMath::IsFinite(LX)||FMath::Abs(LX)>static_cast<double>(MAX_flt)) { Fail(R,TEXT("umg_checkbox_label_geometry_overflow"),TEXT("Sibling label offset must remain finite in slot float32"));return false; }G.LabelPosition=FVector2D(static_cast<float>(LX),static_cast<float>(Y)); }
    }
    if(!Pair(Params,TEXT("size"),G.HasSize,X,Y,R)) { return false; }if(G.HasSize) { G.Size=FVector2D(static_cast<float>(X),static_cast<float>(Y)); }return true;
}
bool GeneratedVariable(UWidgetBlueprint* BP,UWidget* Widget)
{
    if(!Loaded(Widget)||!BP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())||!BP->WidgetVariableNameToGuidMap.FindChecked(Widget->GetFName()).IsValid()||!Loaded(BP->GeneratedClass.Get())) { return false; }
    const FObjectPropertyBase* Property=FindFProperty<FObjectPropertyBase>(BP->GeneratedClass.Get(),Widget->GetFName());return !Widget->bIsVariable||(Property&&Loaded(Property->PropertyClass)&&Widget->IsA(Property->PropertyClass));
}
FResult RunCheckbox(const FString& ParamsJson)
{
    FResult R;Init(R,TEXT("add_checkbox_to_widget"),true);R.Data->SetBoolField(TEXT("original_Python_wrapper_exists"),false);R.Data->SetBoolField(TEXT("native_command_object_adapter"),true);
    if(!Admit(R,{ParamsJson})) { return R; }FActiveScope Scope;Count(R,TEXT("transaction_calls"));FScopedTransaction Tx(FText::FromString(TEXT("MCP add_checkbox_to_widget")));
    TSharedPtr<FJsonValue> Value;if(!BoxValue(ParamsJson,Value,R)) { return R; }if(Value->Type!=EJson::Object) { Fail(R,TEXT("umg_checkbox_params_invalid"),TEXT("Only this original named command's parameter object is admitted"));return R; }const auto Params=Value->AsObject();R.Data->SetObjectField(TEXT("native_command_arguments"),Params);
    FString Target;if(!TryGetStringFieldAny(Params,{TEXT("blueprint_name"),TEXT("widget_blueprint"),TEXT("widget_path"),TEXT("widget_name"),TEXT("name")},Target)) { Fail(R,TEXT("umg_checkbox_target_missing"),TEXT("Missing 'blueprint_name' parameter"));return R; }if(!TextSafe(Target,131072)) { Fail(R,TEXT("umg_checkbox_target_invalid"),TEXT("Bounded target without interior NUL required"));return R; }
    R.Stage=TEXT("load_widget_blueprint");FString Package;UWidgetBlueprint* BP=Load(Target,Package,R);if(!BP) { return R; }TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);
    FString Name;if(!TryGetStringFieldAny(Params,{TEXT("checkbox_name"),TEXT("component_name"),TEXT("widget_component_name")},Name)) { Fail(R,TEXT("umg_checkbox_name_missing"),TEXT("Missing checkbox_name parameter"));return R; }bool Checked=false;Params->TryGetBoolField(TEXT("checked"),Checked);FString Text;Params->TryGetStringField(TEXT("text"),Text);const bool HasLabel=!Text.IsEmpty();R.Data->SetStringField(TEXT("resolved_checkbox_name"),Name);R.Data->SetBoolField(TEXT("requested_checked"),Checked);R.Data->SetBoolField(TEXT("sibling_label_requested"),HasLabel);
    if(!NameValid(Name,true,R)||(HasLabel&&!NameValid(Name+TEXT("_Label"),false,R))||!TextSafe(Text,131072)) { if(R.Error.IsEmpty()) { Fail(R,TEXT("umg_checkbox_text_invalid"),TEXT("Bounded label text without interior NUL required")); }return R; }
    FCheckboxGeometry G;if(!Geometry(Params,HasLabel,G,R)) { return R; }UWidgetTree* Tree=BP->WidgetTree;FName Planned(*Name);if(Planned.IsNone()) { Count(R,TEXT("unique_widget_name_calls"));Planned=MakeUniqueObjectName(Tree,UCheckBox::StaticClass());Count(R,TEXT("unique_widget_name_returns")); }R.Data->SetStringField(TEXT("planned_checkbox_object_name"),Planned.ToString());UWidget* PreviousRoot=Tree->RootWidget;if(PreviousRoot&&!Loaded(PreviousRoot)) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_checkbox_root_unavailable"),TEXT("Non-null root must be loaded before allocation"));return R; }const bool NeedsRoot=Cast<UCanvasPanel>(PreviousRoot)==nullptr;const bool ReplacesRoot=NeedsRoot&&PreviousRoot!=nullptr;
    if(!FreeName(BP,Tree,Planned,R)||(HasLabel&&!FreeName(BP,Tree,FName(*(Name+TEXT("_Label"))),R))||(NeedsRoot&&!FreeName(BP,Tree,TEXT("RootCanvas"),R))) { return R; }
    if(NeedsRoot&&(Planned==TEXT("RootCanvas")||(HasLabel&&FName(*(Name+TEXT("_Label")))==TEXT("RootCanvas")))) { Fail(R,TEXT("umg_checkbox_planned_name_collision"),TEXT("Checkbox/label/RootCanvas names must differ before allocation"));return R; }
    bool BeforeOK=false;const auto Before=Capture(BP,R,BeforeOK);R.Data->SetObjectField(TEXT("before_state"),Before);R.Data->SetBoolField(TEXT("package_dirty_before"),BP->GetOutermost()->IsDirty());R.Data->SetBoolField(TEXT("original_non_canvas_root_replacement"),ReplacesRoot);R.Data->SetStringField(TEXT("prior_root_path"),PreviousRoot?PreviousRoot->GetPathName():FString());TStrongObjectPtr<UWidget> KeepPreviousRoot(PreviousRoot);
    if(!BeforeOK) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_checkbox_prewrite_observation_incomplete"),TEXT("Current bounded source inventory must close before allocation"));return R; }
    R.Stage=TEXT("construct_checkbox");R.bMutationAttempted=true;Count(R,TEXT("widget_construct_calls"));UCheckBox* Checkbox=Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),Planned);Count(R,TEXT("widget_construct_returns"));PublishWidget(R,TEXT("checkbox_return"),Checkbox);if(!Loaded(Checkbox)) { Fail(R,TEXT("umg_checkbox_creation_failed"),TEXT("Failed to create checkbox; no automatic retry/cleanup"));bool OK=false;End(BP,R,Before,OK);R.bChangeStateKnown=false;return R; }R.bChanged=true;TStrongObjectPtr<UCheckBox> KeepCheckbox(Checkbox);Count(R,TEXT("checkbox_set_calls"));Checkbox->SetIsChecked(Checked);
    R.Stage=TEXT("attach_checkbox");UCanvasPanel* Root=Canvas(BP,R);if(!Loaded(Root)) { bool OK=false;End(BP,R,Before,OK);R.bChangeStateKnown=false;return R; }TStrongObjectPtr<UCanvasPanel> KeepRoot(Root);PublishWidget(R,TEXT("current_root_return"),Root);R.Data->SetBoolField(TEXT("root_changed"),Tree->RootWidget!=PreviousRoot);
    Count(R,TEXT("canvas_child_add_calls"));UCanvasPanelSlot* CheckboxSlot=Root->AddChildToCanvas(Checkbox);Count(R,TEXT("canvas_child_add_returns"));PublishWidget(R,TEXT("checkbox_slot_return"),CheckboxSlot);TStrongObjectPtr<UCanvasPanelSlot> KeepCheckboxSlot(CheckboxSlot);FVector2D InitialPosition=FVector2D::ZeroVector,InitialSize=FVector2D::ZeroVector;TArray<FString> Appended;
    if(Loaded(CheckboxSlot)) { InitialPosition=CheckboxSlot->GetPosition();InitialSize=CheckboxSlot->GetSize();Vec(R.Data,TEXT("checkbox_slot_initial_position"),InitialPosition);Vec(R.Data,TEXT("checkbox_slot_initial_size"),InitialSize);if(G.HasPosition) { Count(R,TEXT("slot_position_set_calls"));CheckboxSlot->SetPosition(G.Position); }if(G.HasSize) { Count(R,TEXT("slot_size_set_calls"));CheckboxSlot->SetSize(G.Size); }if(CheckboxSlot->Content==Checkbox&&CheckboxSlot->Parent==Root) { Appended.Add(Checkbox->GetPathName()); } }
    Count(R,TEXT("on_variable_added_calls"));BP->OnVariableAdded(Checkbox->GetFName());
    UTextBlock* Label=nullptr;TStrongObjectPtr<UTextBlock> KeepLabel;UCanvasPanelSlot* LabelSlot=nullptr;TStrongObjectPtr<UCanvasPanelSlot> KeepLabelSlot;
    if(HasLabel)
    {
        R.Stage=TEXT("construct_sibling_label");Count(R,TEXT("widget_construct_calls"));Label=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),FName(*(Name+TEXT("_Label"))));Count(R,TEXT("widget_construct_returns"));PublishWidget(R,TEXT("sibling_label_return"),Label);
        if(Loaded(Label)) { KeepLabel.Reset(Label);Count(R,TEXT("text_set_calls"));Label->SetText(FText::FromString(Text));Count(R,TEXT("canvas_child_add_calls"));LabelSlot=Root->AddChildToCanvas(Label);Count(R,TEXT("canvas_child_add_returns"));PublishWidget(R,TEXT("label_slot_return"),LabelSlot);if(Loaded(LabelSlot)) { KeepLabelSlot.Reset(LabelSlot);Count(R,TEXT("slot_position_set_calls"));LabelSlot->SetPosition(G.LabelPosition);Count(R,TEXT("slot_size_set_calls"));LabelSlot->SetSize(FVector2D(220,32));if(LabelSlot->Content==Label&&LabelSlot->Parent==Root) { Appended.Add(Label->GetPathName()); } }Count(R,TEXT("on_variable_added_calls"));BP->OnVariableAdded(Label->GetFName()); }
    }
    Save(BP,Package,R);auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("path"),Package);Legacy->SetStringField(TEXT("asset_path"),WidgetObjectPathFromPackage(Package));Legacy->SetStringField(TEXT("widget_name"),Name);Legacy->SetBoolField(TEXT("checked"),Checked);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    R.Stage=TEXT("after_save_observation");bool OK=false;const auto After=End(BP,R,Before,OK);if(!OK) { return R; }const bool Preserved=Preserve(Before,After,Root,Appended,FString(),FString(),ReplacesRoot);int64 CurrentState=0;
    bool Selected=Loaded(Checkbox)&&Checkbox->GetOuter()==Tree&&Tree->RootWidget==Root&&Loaded(CheckboxSlot)&&Checkbox->Slot==CheckboxSlot&&CheckboxSlot->Content==Checkbox&&CheckboxSlot->Parent==Root&&CheckboxSlot->GetPosition()==(G.HasPosition?G.Position:InitialPosition)&&CheckboxSlot->GetSize()==(G.HasSize?G.Size:InitialSize)&&CheckboxValue(Checkbox,CurrentState,R)&&CurrentState==static_cast<int64>(Checked?ECheckBoxState::Checked:ECheckBoxState::Unchecked)&&GeneratedVariable(BP,Checkbox);
    if(HasLabel) { Selected=Selected&&Loaded(Label)&&Label->GetOuter()==Tree&&Loaded(LabelSlot)&&Label->Slot==LabelSlot&&LabelSlot->Content==Label&&LabelSlot->Parent==Root&&Label->GetText().ToString().Equals(Text,ESearchCase::CaseSensitive)&&LabelSlot->GetPosition()==G.LabelPosition&&LabelSlot->GetSize()==FVector2D(220,32)&&!Label->bIsVariable&&GeneratedVariable(BP,Label); }
    R.Data->SetBoolField(TEXT("old_selected_source_preserved"),Preserved);R.Data->SetBoolField(TEXT("current_checkbox_postcondition"),Selected);R.Data->SetBoolField(TEXT("prior_non_canvas_root_still_in_source_tree"),!ReplacesRoot||(Loaded(PreviousRoot)&&PreviousRoot->GetOuter()==Tree));R.Data->SetStringField(TEXT("current_root_path"),Tree->RootWidget?Tree->RootWidget->GetPathName():FString());
    if(!Preserved||!Selected) { Fail(R,TEXT("umg_checkbox_postcondition_failed"),TEXT("Actual compile/save prefix retained; checkbox/sibling/current source predicate failed"));return R; }if(!R.Compiled.IsSet()||!*R.Compiled) { Fail(R,TEXT("umg_checkbox_compile_failed"),TEXT("Original legacy success retained separately; actual full compiler failed before forced save"));return R; }if(!R.bSaved) { Fail(R,TEXT("umg_checkbox_save_failed"),TEXT("Original forced save returned false; no second save/retry"));return R; }R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
FResult RunViewport(const FString& WidgetName,int64 ZOrder)
{
    FResult R;Init(R,TEXT("add_widget_to_viewport"),false);R.Data->SetBoolField(TEXT("read_only"),true);R.Data->SetStringField(TEXT("read_only_scope"),TEXT("Direct authored Widget Blueprint/class readiness after loader; no total loader/transaction/global side-effect proof"));R.Data->SetBoolField(TEXT("widget_instance_created"),false);R.Data->SetBoolField(TEXT("widget_added_to_viewport"),false);R.Data->SetStringField(TEXT("requested_z_order"),LexToString(ZOrder));
    if(!Admit(R,{WidgetName})) { return R; }FActiveScope Scope;Count(R,TEXT("transaction_calls"));FScopedTransaction Tx(FText::FromString(TEXT("MCP add_widget_to_viewport")));
    R.Stage=TEXT("load_widget_blueprint");const FString Package=NormalizeWidgetAssetPath(WidgetName);if(!GamePackage(Package,R)) { return R; }Count(R,TEXT("asset_load_calls"));UWidgetBlueprint* BP=Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(Package));Count(R,TEXT("asset_load_returns"));if(!BP) { Count(R,TEXT("asset_load_calls"));BP=Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetObjectPathFromPackage(Package)));Count(R,TEXT("asset_load_returns")); }
    if(!Loaded(BP)) { R.bChangeStateKnown=BP==nullptr;Fail(R,TEXT("umg_viewport_blueprint_not_found"),FString::Printf(TEXT("Failed to load Widget Blueprint: %s"),*Package));return R; }TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);if(BP->GetOutermost()->GetName()!=Package||ToolingPackage(BP->GetClass())) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_viewport_asset_owner_invalid"),TEXT("Current loaded Widget Blueprint package/class identity must match"));return R; }
    // Original handler loads first and reads the optional int32 Z-order second.
    if(ZOrder<MIN_int32||ZOrder>MAX_int32) { Fail(R,TEXT("umg_viewport_z_order_out_of_range"),TEXT("Original response int32 must be representable; no unsafe narrowing"));return R; }const int32 Z=static_cast<int32>(ZOrder);R.Stage=TEXT("read_generated_class");Count(R,TEXT("generated_class_reads"));UClass* Class=BP->GeneratedClass.Get();if(!Class) { Fail(R,TEXT("umg_viewport_generated_class_missing"),TEXT("Failed to get widget class"));return R; }if(!Loaded(Class)||!Class->IsChildOf(UUserWidget::StaticClass())||Class->ClassGeneratedBy!=BP) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_viewport_generated_class_unavailable"),TEXT("Non-null generated widget class must be loaded and owned; no implicit compiler"));return R; }TStrongObjectPtr<UClass> KeepClass(Class);
    const bool Dirty=BP->GetOutermost()->IsDirty();const FString ClassPath=Class->GetPathName();const FString Note=TEXT("Widget class ready. Use CreateWidget and AddToViewport nodes in Blueprint to display in game.");auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("blueprint_name"),Package);Legacy->SetStringField(TEXT("class_path"),ClassPath);Legacy->SetNumberField(TEXT("z_order"),Z);Legacy->SetStringField(TEXT("note"),Note);Legacy->SetStringField(TEXT("path"),Package);Legacy->SetStringField(TEXT("asset_path"),WidgetObjectPathFromPackage(Package));R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    R.Data->SetStringField(TEXT("blueprint_name"),Package);R.Data->SetStringField(TEXT("class_path"),ClassPath);R.Data->SetStringField(TEXT("asset_path"),WidgetObjectPathFromPackage(Package));R.Data->SetNumberField(TEXT("z_order"),Z);R.Data->SetStringField(TEXT("note"),Note);R.Data->SetBoolField(TEXT("z_order_used_for_runtime_UI"),false);R.Data->SetBoolField(TEXT("package_dirty_after_load"),Dirty);Count(R,TEXT("generated_class_reads"));const bool Current=Loaded(BP)&&Loaded(Class)&&BP->GeneratedClass.Get()==Class&&Class->ClassGeneratedBy==BP&&Class->GetPathName().Equals(ClassPath,ESearchCase::CaseSensitive)&&BP->GetOutermost()->IsDirty()==Dirty;R.Data->SetBoolField(TEXT("current_class_readiness_verified"),Current);
    if(!Current) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_viewport_readiness_incomplete"),TEXT("Returned class-readiness prefix retained; current selected identity did not close"));return R; }R.bSuccess=R.bPostconditionPassed=true;R.bChangeStateKnown=true;R.bChanged=R.bMutationAttempted=false;R.Stage=TEXT("complete");return R;
}
}
Variable::FResult AddCheckbox(const FString& ParamsJson)
{
    return RunCheckbox(ParamsJson);
}
Variable::FResult ViewportReadiness(const FString& WidgetName,int64 ZOrder)
{
    return RunViewport(WidgetName,ZOrder);
}
}
