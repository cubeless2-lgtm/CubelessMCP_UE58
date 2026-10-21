#include "Compatibility/UE58/CubelessUMGInteractionApi.h"
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
#include "K2Node_ComponentBoundEvent.h"
#include "EdGraphSchema_K2.h"
#include "Settings/EditorStyleSettings.h"
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
#include "Engine/ComponentDelegateBinding.h"
namespace Cubeless::UE58::UMGInteraction
{
namespace
{
using Variable::FResult;
bool Active=false;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool TextSafe(const FString& S,int32 Limit=65536) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(FResult& R,const TCHAR* K,int32 N=1) { auto J=R.Data->GetObjectField(TEXT("native_call_counts"));J->SetNumberField(K,J->GetNumberField(K)+N); }
void Init(FResult& R,const TCHAR* Operation)
{
    R.bCompileRequested=R.bSaveRequested=true;R.Data->SetStringField(TEXT("operation"),Operation);
    auto J=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("transaction_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("state_capture_calls"),TEXT("object_inventory_calls"),TEXT("name_collision_checks"),TEXT("widget_construct_calls"),TEXT("widget_construct_returns"),TEXT("widget_tree_modify_calls"),TEXT("blueprint_modify_calls"),TEXT("canvas_modify_calls"),TEXT("root_widget_assignments"),TEXT("canvas_child_add_calls"),TEXT("canvas_child_add_returns"),TEXT("slot_position_set_calls"),TEXT("slot_size_set_calls"),TEXT("on_variable_added_calls"),TEXT("slider_setter_calls"),TEXT("slider_public_flag_assignments"),TEXT("slider_property_reads"),TEXT("mark_structural_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("find_event_graph_calls"),TEXT("find_widget_calls"),TEXT("widget_property_lookups"),TEXT("delegate_property_lookups"),TEXT("find_bound_event_calls"),TEXT("find_bound_event_returns"),TEXT("create_bound_event_calls"),TEXT("create_bound_event_returns"),TEXT("get_last_edited_graph_calls"),TEXT("good_place_queries"),TEXT("editor_grid_reads"),TEXT("signature_pin_type_checks"),TEXT("generated_binding_reads"),TEXT("explicit_UI_jump_calls"),TEXT("runtime_delegate_bind_calls")}) { J->SetNumberField(K,0); }R.Data->SetObjectField(TEXT("native_call_counts"),J);
    R.Data->SetBoolField(TEXT("total_SDK_side_effects_verified"),false);R.Data->SetBoolField(TEXT("implicit_structural_skeleton_compile_possible"),true);R.Data->SetBoolField(TEXT("runtime_widget_or_delegate_execution_verified"),false);R.Data->SetBoolField(TEXT("cold_saved_readback_verified"),false);R.Data->SetBoolField(TEXT("auto_retry_safe"),false);
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

bool TryParseVector2Field(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FVector2D& DefaultValue,
	bool bRequirePositive,
	FVector2D& OutValue,
	FString& OutError)
{
	OutValue = DefaultValue;
	if (!Object->HasField(FieldName))
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(FieldName, Values) || Values->Num() != 2 ||
		!(*Values)[0].IsValid() || !(*Values)[1].IsValid() ||
		(*Values)[0]->Type != EJson::Number || (*Values)[1]->Type != EJson::Number)
	{
		OutError = FString::Printf(TEXT("'%s' must be an array of two numbers"), FieldName);
		return false;
	}

	const double X = (*Values)[0]->AsNumber();
	const double Y = (*Values)[1]->AsNumber();
	if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || (bRequirePositive && (X <= 0.0 || Y <= 0.0)))
	{
		OutError = FString::Printf(
			TEXT("'%s' values must be finite%s"),
			FieldName,
			bRequirePositive ? TEXT(" and greater than zero") : TEXT(""));
		return false;
	}

	if (FMath::Abs(X)>static_cast<double>(MAX_flt)||FMath::Abs(Y)>static_cast<double>(MAX_flt)||(bRequirePositive&&(static_cast<float>(X)<=0.0f||static_cast<float>(Y)<=0.0f))) { OutError=TEXT("Vector must remain finite and preserve positive size after float32 projection");return false; }
	OutValue = FVector2D(static_cast<float>(X), static_cast<float>(Y));
	return true;
}

bool TryParseFiniteNumberField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	double DefaultValue,
	double& OutValue,
	FString& OutError)
{
	OutValue = DefaultValue;
	if (!Object->HasField(FieldName))
	{
		return true;
	}
	if (!Object->TryGetNumberField(FieldName, OutValue) || !FMath::IsFinite(OutValue))
	{
		OutError = FString::Printf(TEXT("'%s' must be a finite number"), FieldName);
		return false;
	}
	return true;
}

bool TryParseBoolField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	bool DefaultValue,
	bool& OutValue,
	FString& OutError)
{
	OutValue = DefaultValue;
	if (!Object->HasField(FieldName))
	{
		return true;
	}
	if (!Object->TryGetBoolField(FieldName, OutValue))
	{
		OutError = FString::Printf(TEXT("'%s' must be a boolean"), FieldName);
		return false;
	}
	return true;
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
        if(USlider* S=Cast<USlider>(W)) { auto Values=MakeShared<FJsonObject>();if(!SliderValues(S,Values,R)) { return J; }Q->SetObjectField(TEXT("slider"),Values); }Widgets->SetObjectField(W->GetPathName(),Q);
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
bool Preserve(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,UCanvasPanel* Root,const TArray<FString>& Appended,const FString& AddedGraph,const FString& AddedNode)
{
    for(const TCHAR* K:{TEXT("blueprint"),TEXT("asset_class"),TEXT("parent_class"),TEXT("widget_tree")}) { if(Before->GetStringField(K)!=After->GetStringField(K)) { return false; } }
    if(Before->GetStringField(TEXT("root"))!=After->GetStringField(TEXT("root"))&&!(Before->GetStringField(TEXT("root")).IsEmpty()&&Root&&!Appended.IsEmpty()&&After->GetStringField(TEXT("root"))==Root->GetPathName())) { return false; }
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
    auto After=Capture(BP,R,OK);R.Data->SetObjectField(TEXT("after_state"),After);R.Data->SetBoolField(TEXT("package_dirty_after"),BP->GetOutermost()->IsDirty());if(!OK) { R.Data->SetStringField(TEXT("preceding_error_code"),R.ErrorCode);R.Data->SetStringField(TEXT("preceding_error"),R.Error);R.bChangeStateKnown=false;R.Stage=TEXT("observation_incomplete");Fail(R,TEXT("umg_interaction_observation_incomplete"),TEXT("Actual accepted/native compiler/save prefix retained; current source inventory incomplete")); }
    else { R.bChangeStateKnown=true;R.bChanged|=!Json(Before).Equals(Json(After),ESearchCase::CaseSensitive); }return After;
}

struct FSpec { FString Name;FVector2D Position=FVector2D::ZeroVector,Size=FVector2D(240,32);float Min=0,Max=1,Value=0,Step=.01f;EOrientation Orientation=Orient_Horizontal;bool Indent=true,Locked=false,Mouse=false,Controller=false,Focusable=true; };
bool Specs(const TSharedPtr<FJsonValue>& Value,UWidgetBlueprint* BP,TArray<FSpec>& Out,FResult& R)
{
    if(!Value.IsValid()||Value->Type!=EJson::Array||Value->AsArray().IsEmpty()||Value->AsArray().Num()>100) { Fail(R,TEXT("umg_slider_batch_invalid"),TEXT("'sliders' must be a non-empty array with at most100 entries"));return false; }TSet<FName> Names;
    for(const auto& Item:Value->AsArray())
    {
        if(!Item.IsValid()||Item->Type!=EJson::Object) { Fail(R,TEXT("umg_slider_spec_invalid"),TEXT("Each slider entry must be an object"));return false; }const auto J=Item->AsObject();FSpec S;FString Why;
        if(!TryGetStringFieldAny(J,{TEXT("slider_name"),TEXT("name")},S.Name)||!NameValid(S.Name,false,R)) { if(R.Error.IsEmpty()) { Fail(R,TEXT("umg_slider_name_missing"),TEXT("Slider name is required")); }return false; }const FName Name(*S.Name);if(Name.IsNone()||Names.Contains(Name)||!FreeName(BP,BP->WidgetTree,Name,R)) { if(R.Error.IsEmpty()) { Fail(R,TEXT("umg_slider_name_duplicate"),TEXT("Slider names must be unique in batch and current tree/GUID map")); }return false; }Names.Add(Name);
        if(!TryParseVector2Field(J,TEXT("position"),FVector2D::ZeroVector,false,S.Position,Why)||!TryParseVector2Field(J,TEXT("size"),FVector2D(240,32),true,S.Size,Why)) { Fail(R,TEXT("umg_slider_geometry_invalid"),Why);return false; }
        double Min=0,Max=1,V=0,Step=.01;if(!TryParseFiniteNumberField(J,TEXT("min_value"),0,Min,Why)||!TryParseFiniteNumberField(J,TEXT("max_value"),1,Max,Why)) { Fail(R,TEXT("umg_slider_range_invalid"),Why);return false; }if(Max<=Min) { Fail(R,TEXT("umg_slider_range_invalid"),TEXT("max_value must exceed min_value"));return false; }
        if(!TryParseFiniteNumberField(J,TEXT("value"),FMath::Clamp(0.0,Min,Max),V,Why)||!TryParseFiniteNumberField(J,TEXT("step_size"),.01,Step,Why)) { Fail(R,TEXT("umg_slider_value_invalid"),Why);return false; }if(V<Min||V>Max||Step<0||Step>1) { Fail(R,TEXT("umg_slider_value_invalid"),TEXT("Value must lie in range and step_size must be0..1"));return false; }
        if(FMath::Abs(Min)>static_cast<double>(MAX_flt)||FMath::Abs(Max)>static_cast<double>(MAX_flt)||FMath::Abs(V)>static_cast<double>(MAX_flt)) { Fail(R,TEXT("umg_slider_float32_overflow"),TEXT("Slider values must remain finite after original float32 projection"));return false; }S.Min=static_cast<float>(Min);S.Max=static_cast<float>(Max);S.Value=static_cast<float>(V);S.Step=static_cast<float>(Step);if(S.Max<=S.Min||S.Value<S.Min||S.Value>S.Max) { Fail(R,TEXT("umg_slider_float32_range_collapse"),TEXT("Original scalar range must remain ordered after float32 projection"));return false; }
        FString Orientation=TEXT("horizontal");if(J->HasField(TEXT("orientation"))&&!J->TryGetStringField(TEXT("orientation"),Orientation)) { Fail(R,TEXT("umg_slider_orientation_invalid"),TEXT("Orientation must be a string"));return false; }Orientation.TrimStartAndEndInline();if(Orientation.Equals(TEXT("horizontal"),ESearchCase::IgnoreCase)) { S.Orientation=Orient_Horizontal; }else if(Orientation.Equals(TEXT("vertical"),ESearchCase::IgnoreCase)) { S.Orientation=Orient_Vertical; }else { Fail(R,TEXT("umg_slider_orientation_invalid"),TEXT("Orientation must be horizontal or vertical"));return false; }
        if(!TryParseBoolField(J,TEXT("indent_handle"),true,S.Indent,Why)||!TryParseBoolField(J,TEXT("locked"),false,S.Locked,Why)||!TryParseBoolField(J,TEXT("mouse_uses_step"),false,S.Mouse,Why)||!TryParseBoolField(J,TEXT("requires_controller_lock"),false,S.Controller,Why)||!TryParseBoolField(J,TEXT("focusable"),true,S.Focusable,Why)) { Fail(R,TEXT("umg_slider_boolean_invalid"),Why);return false; }Out.Add(S);
    }
    if(!BP->WidgetTree->RootWidget)
    {
        if(Names.Contains(TEXT("RootCanvas"))||!FreeName(BP,BP->WidgetTree,TEXT("RootCanvas"),R)) { if(R.Error.IsEmpty()) { Fail(R,TEXT("umg_slider_root_name_collision"),TEXT("Planned RootCanvas and slider names must not collide")); }return false; }
    }
    return true;
}
}
FResult AddSliders(const FString& WidgetName,const FString& SlidersJson)
{
    FResult R;Init(R,TEXT("add_sliders_to_widget"));if(!Admit(R,{WidgetName,SlidersJson})) { return R; }FActiveScope Scope;Count(R,TEXT("transaction_calls"));FScopedTransaction Tx(FText::FromString(TEXT("MCP add_sliders_to_widget")));
    R.Stage=TEXT("load_widget_blueprint");FString Package;UWidgetBlueprint* BP=Load(WidgetName.TrimStartAndEnd(),Package,R);if(!BP) { return R; }TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);
    if(BP->WidgetTree->RootWidget&&(!Loaded(BP->WidgetTree->RootWidget)||!Cast<UCanvasPanel>(BP->WidgetTree->RootWidget))) { Fail(R,TEXT("umg_slider_root_invalid"),TEXT("Widget Blueprint root must be a CanvasPanel before adding sliders"));return R; }
    TSharedPtr<FJsonValue> Value;TArray<FSpec> Plans;if(!BoxValue(SlidersJson,Value,R)) { return R; }R.Data->SetField(TEXT("requested_sliders"),Value);R.Stage=TEXT("all_row_preflight");if(!Specs(Value,BP,Plans,R)) { return R; }
    bool BeforeOK=false;const auto Before=Capture(BP,R,BeforeOK);R.Data->SetObjectField(TEXT("before_state"),Before);R.Data->SetBoolField(TEXT("package_dirty_before"),BP->GetOutermost()->IsDirty());if(!BeforeOK) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_slider_prewrite_incomplete"),TEXT("All current source rows must close before any allocation/Modify"));return R; }
    R.Stage=TEXT("prepare_canvas");R.bMutationAttempted=true;Count(R,TEXT("blueprint_modify_calls"));BP->Modify();Count(R,TEXT("widget_tree_modify_calls"));BP->WidgetTree->Modify();UCanvasPanel* Root=Cast<UCanvasPanel>(BP->WidgetTree->RootWidget);if(!Root) { Root=Canvas(BP,R); }if(!Loaded(Root)) { if(R.Error.IsEmpty()) { Fail(R,TEXT("umg_slider_canvas_failed"),TEXT("Canvas creation failed; no cleanup/retry")); }return R; }TStrongObjectPtr<UCanvasPanel> KeepRoot(Root);Count(R,TEXT("canvas_modify_calls"));Root->Modify();
    TArray<TStrongObjectPtr<USlider>> Kept;TArray<FString> Paths;TArray<TSharedPtr<FJsonValue>> Names,Receipts;
    for(const FSpec& S:Plans)
    {
        R.Stage=TEXT("construct_slider");Count(R,TEXT("widget_construct_calls"));USlider* Slider=BP->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),FName(*S.Name));Count(R,TEXT("widget_construct_returns"));PublishWidget(R,TEXT("last_slider_return"),Slider);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_name"),S.Name);Row->SetBoolField(TEXT("constructor_returned"),Slider!=nullptr);Receipts.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("slider_receipts"),Receipts);if(!Loaded(Slider)) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_slider_creation_failed"),TEXT("Slider constructor failed; accepted partial prefix retained"));return R; }Kept.Emplace(Slider);R.bChanged=true;Row->SetStringField(TEXT("path"),Slider->GetPathName());
        Count(R,TEXT("slider_setter_calls"));Slider->SetMinValue(S.Min);Count(R,TEXT("slider_setter_calls"));Slider->SetMaxValue(S.Max);Count(R,TEXT("slider_setter_calls"));Slider->SetValue(S.Value);Count(R,TEXT("slider_setter_calls"));Slider->SetStepSize(S.Step);Count(R,TEXT("slider_setter_calls"));Slider->SetOrientation(S.Orientation);Count(R,TEXT("slider_setter_calls"));Slider->SetIndentHandle(S.Indent);Count(R,TEXT("slider_setter_calls"));Slider->SetLocked(S.Locked);Slider->MouseUsesStep=S.Mouse;Count(R,TEXT("slider_public_flag_assignments"));Slider->RequiresControllerLock=S.Controller;Count(R,TEXT("slider_public_flag_assignments"));Slider->IsFocusable=S.Focusable;Count(R,TEXT("slider_public_flag_assignments"));Row->SetBoolField(TEXT("settings_returned"),true);
        Count(R,TEXT("canvas_child_add_calls"));UCanvasPanelSlot* Slot=Root->AddChildToCanvas(Slider);Count(R,TEXT("canvas_child_add_returns"));PublishWidget(R,TEXT("last_canvas_slot_return"),Slot);Row->SetBoolField(TEXT("slot_returned"),Slot!=nullptr);if(!Loaded(Slot)) { Fail(R,TEXT("umg_slider_slot_failed"),TEXT("Slider canvas slot failed; no rollback or compile/save after this original early failure"));bool OK=false;End(BP,R,Before,OK);return R; }
        Count(R,TEXT("slot_position_set_calls"));Slot->SetPosition(S.Position);Count(R,TEXT("slot_size_set_calls"));Slot->SetSize(S.Size);Count(R,TEXT("on_variable_added_calls"));BP->OnVariableAdded(Slider->GetFName());Row->SetBoolField(TEXT("slot_variable_returned"),true);Paths.Add(Slider->GetPathName());Names.Add(MakeShared<FJsonValueString>(S.Name));
    }
    Save(BP,Package,R);auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("path"),Package);Legacy->SetStringField(TEXT("asset_path"),WidgetObjectPathFromPackage(Package));Legacy->SetNumberField(TEXT("sliders_added"),Plans.Num());Legacy->SetArrayField(TEXT("slider_names"),Names);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    R.Stage=TEXT("after_save_observation");bool OK=false;const auto After=End(BP,R,Before,OK);if(!OK) { return R; }const bool Preserved=Preserve(Before,After,Root,Paths,FString(),FString());bool Selected=Loaded(Root)&&BP->WidgetTree->RootWidget==Root;
    for(int32 I=0;I<Kept.Num();++I)
    {
        USlider* S=Kept[I].Get();const FSpec& P=Plans[I];auto V=MakeShared<FJsonObject>();UCanvasPanelSlot* Slot=Loaded(S)?Cast<UCanvasPanelSlot>(S->Slot):nullptr;bool Good=Loaded(S)&&Loaded(Slot)&&S->GetOuter()==BP->WidgetTree&&Slot->Parent==Root&&Slot->Content==S&&Root->HasChild(S)&&S->bIsVariable&&BP->WidgetVariableNameToGuidMap.Contains(S->GetFName())&&SliderValues(S,V,R);
        if(Good) { Good=V->GetNumberField(TEXT("MinValue"))==P.Min&&V->GetNumberField(TEXT("MaxValue"))==P.Max&&V->GetNumberField(TEXT("Value"))==P.Value&&V->GetNumberField(TEXT("StepSize"))==P.Step&&S->GetOrientation()==P.Orientation&&S->HasIndentHandle()==P.Indent&&S->IsLocked()==P.Locked&&S->MouseUsesStep==P.Mouse&&S->RequiresControllerLock==P.Controller&&S->IsFocusable==P.Focusable&&Slot->GetPosition()==P.Position&&Slot->GetSize()==P.Size; }
        const FObjectPropertyBase* Property=Loaded(BP->GeneratedClass.Get())?FindFProperty<FObjectPropertyBase>(BP->GeneratedClass.Get(),FName(*P.Name)):nullptr;Good=Good&&Property&&Loaded(Property->PropertyClass)&&S->IsA(Property->PropertyClass);Receipts[I]->AsObject()->SetBoolField(TEXT("current_postcondition"),Good);Selected=Selected&&Good;
    }
    R.Data->SetBoolField(TEXT("old_selected_source_preserved"),Preserved);R.Data->SetBoolField(TEXT("current_slider_postcondition"),Selected);if(!Preserved||!Selected) { Fail(R,TEXT("umg_slider_postcondition_failed"),TEXT("Actual compiler/save returned; current slider/preserved-source postcondition failed"));return R; }if(!R.Compiled.IsSet()||!*R.Compiled) { Fail(R,TEXT("umg_slider_compile_failed"),TEXT("Actual final Editor compile failed; forced save result and legacy response retained"));return R; }if(!R.bSaved) { Fail(R,TEXT("umg_slider_save_failed"),TEXT("Forced save returned false; no second save"));return R; }R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}

FResult BindEvent(const FString& WidgetName,const FString& WidgetComponentName,const FString& EventName,const FString& FunctionName)
{
    FResult R;Init(R,TEXT("bind_widget_event"));R.Data->SetStringField(TEXT("ignored_function_name"),FunctionName);R.Data->SetBoolField(TEXT("original_function_name_ignored"),true);R.Data->SetBoolField(TEXT("SDK_UI_jump_suppressed"),true);if(!Admit(R,{WidgetName,WidgetComponentName,EventName,FunctionName})) { return R; }FActiveScope Scope;Count(R,TEXT("transaction_calls"));FScopedTransaction Tx(FText::FromString(TEXT("MCP bind_widget_event")));
    const FString Component=WidgetComponentName.TrimStartAndEnd();if(Component.Len()>=NAME_SIZE||EventName.Len()>=NAME_SIZE) { Fail(R,TEXT("umg_binding_name_too_long"),TEXT("Reference names must fit FName before initial compiler or lookup"));return R; }
    R.Stage=TEXT("load_widget_blueprint");FString Package;UWidgetBlueprint* BP=Load(WidgetName.TrimStartAndEnd(),Package,R);if(!BP) { return R; }TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);
    Count(R,TEXT("find_event_graph_calls"));UEdGraph* EventGraph=FBlueprintEditorUtils::FindEventGraph(BP);if(!Loaded(EventGraph)) { Fail(R,TEXT("umg_binding_event_graph_missing"),TEXT("Failed to find or create event graph (original FindEventGraph does not create)"));return R; }TStrongObjectPtr<UEdGraph> KeepEventGraph(EventGraph);
    Count(R,TEXT("find_widget_calls"));UWidget* Widget=BP->WidgetTree->FindWidget(FName(*Component));if(!Loaded(Widget)) { Fail(R,TEXT("umg_binding_widget_missing"),TEXT("Failed to find widget: ")+Component);return R; }TStrongObjectPtr<UWidget> KeepWidget(Widget);
    bool BeforeOK=false;const auto Before=Capture(BP,R,BeforeOK);R.Data->SetObjectField(TEXT("before_state"),Before);R.Data->SetBoolField(TEXT("package_dirty_before"),BP->GetOutermost()->IsDirty());if(!BeforeOK) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_binding_prewrite_incomplete"),TEXT("Current source inventory must close before initial compile"));return R; }
    const bool InitialOK=Compile(BP,R,TEXT("initial_compile"));FObjectProperty* Member=nullptr;
    if(Loaded(BP->SkeletonGeneratedClass.Get())) { Count(R,TEXT("widget_property_lookups"));Member=FindFProperty<FObjectProperty>(BP->SkeletonGeneratedClass.Get(),FName(*Component)); }if(!Member&&Loaded(BP->GeneratedClass.Get())) { Count(R,TEXT("widget_property_lookups"));Member=FindFProperty<FObjectProperty>(BP->GeneratedClass.Get(),FName(*Component)); }
    if(!Member||!Loaded(Member->PropertyClass)||!Widget->IsA(Member->PropertyClass)) { Fail(R,TEXT("umg_binding_widget_property_missing"),TEXT("Widget variable property not found/compatible after actual initial compile: ")+Component);bool OK=false;End(BP,R,Before,OK);return R; }
    Count(R,TEXT("delegate_property_lookups"));FMulticastDelegateProperty* Delegate=FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(),FName(*EventName));if(!Delegate||!Loaded(Delegate->SignatureFunction)) { Fail(R,TEXT("umg_binding_delegate_missing"),TEXT("Delegate event not found on widget class: ")+EventName);bool OK=false;End(BP,R,Before,OK);return R; }TStrongObjectPtr<UFunction> KeepSignature(Delegate->SignatureFunction);TStrongObjectPtr<UClass> KeepWidgetClass(Widget->GetClass());
    const FName MemberName=Member->GetFName(),SignatureName=KeepSignature->GetFName();
    Count(R,TEXT("find_bound_event_calls"));const UK2Node_ComponentBoundEvent* Existing=FKismetEditorUtilities::FindBoundEventForComponent(BP,FName(*EventName),MemberName);Count(R,TEXT("find_bound_event_returns"));const bool Reused=Loaded(Existing);R.Data->SetBoolField(TEXT("existing_bound_event_reused"),Reused);
    if(!Existing)
    {
        Count(R,TEXT("get_last_edited_graph_calls"));UEdGraph* Target=BP->GetLastEditedUberGraph();if(!Loaded(Target)||Target->GetTypedOuter<UBlueprint>()!=BP) { Fail(R,TEXT("umg_binding_target_graph_invalid"),TEXT("SDK last-edited Ubergraph must be currently owned"));bool OK=false;End(BP,R,Before,OK);return R; }
        const UClass* NodeClass=UK2Node_ComponentBoundEvent::StaticClass();const FString MaxNodeName=FName(NodeClass->GetFName(),MAX_int32).ToString();const FString Generated=FString::Printf(TEXT("BndEvt__%s_%s_%s_%s"),*BP->GetName(),*Member->GetName(),*MaxNodeName,*Delegate->SignatureFunction->GetName());if(Generated.Len()>=NAME_SIZE) { Fail(R,TEXT("umg_binding_generated_name_too_long"),TEXT("Actual SDK generated bound-event function name must fit NAME_SIZE before node allocation"));bool OK=false;End(BP,R,Before,OK);return R; }
        Count(R,TEXT("good_place_queries"));const FVector2D Position(Target->GetGoodPlaceForNewNode());Count(R,TEXT("editor_grid_reads"));const uint32 Grid=GetDefault<UEditorStyleSettings>()->GridSnapSize;if(!Grid||!FMath::IsFinite(Position.X)||!FMath::IsFinite(Position.Y)||Position.X<static_cast<double>(MIN_int32)||Position.X>static_cast<double>(MAX_int32)||Position.Y<static_cast<double>(MIN_int32)||Position.Y>static_cast<double>(MAX_int32)) { Fail(R,TEXT("umg_binding_placement_invalid"),TEXT("SDK automatic placement must be finite/int32 and grid nonzero; original signed/uint32 snapping retained"));bool OK=false;End(BP,R,Before,OK);return R; }
        R.Stage=TEXT("create_bound_event");R.bMutationAttempted=true;Count(R,TEXT("create_bound_event_calls"));FKismetEditorUtilities::CreateNewBoundEventForClass(Widget->GetClass(),FName(*EventName),BP,Member,false);Count(R,TEXT("create_bound_event_returns"));R.Data->SetBoolField(TEXT("create_bound_event_returned"),true);Count(R,TEXT("find_bound_event_calls"));Existing=FKismetEditorUtilities::FindBoundEventForComponent(BP,FName(*EventName),MemberName);Count(R,TEXT("find_bound_event_returns"));
    }
    if(!Loaded(Existing)) { Fail(R,TEXT("umg_binding_event_creation_failed"),TEXT("Failed to create/reacquire event node; helper prefix retained without retry/cleanup"));bool OK=false;End(BP,R,Before,OK);return R; }TStrongObjectPtr<const UK2Node_ComponentBoundEvent> KeepNode(Existing);if(!Loaded(Existing->GetGraph())||Existing->GetGraph()->GetTypedOuter<UBlueprint>()!=BP) { Fail(R,TEXT("umg_binding_returned_graph_invalid"),TEXT("Actual returned bound event graph must remain owned; no second creation"));bool OK=false;End(BP,R,Before,OK);return R; }
    const FGuid Guid=Existing->NodeGuid;const FName BoundName=Existing->CustomFunctionName;const FString GraphPath=Existing->GetGraph()->GetPathName();R.Data->SetStringField(TEXT("bound_event_guid"),Guid.ToString());R.Data->SetStringField(TEXT("bound_event_graph"),GraphPath);R.Data->SetStringField(TEXT("bound_function"),Existing->CustomFunctionName.ToString());
    Save(BP,Package,R);auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("path"),Package);Legacy->SetStringField(TEXT("asset_path"),WidgetObjectPathFromPackage(Package));Legacy->SetStringField(TEXT("widget_name"),Component);Legacy->SetStringField(TEXT("event_name"),EventName);Legacy->SetStringField(TEXT("node_guid"),Guid.ToString());R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    R.Stage=TEXT("after_save_observation");bool OK=false;const auto After=End(BP,R,Before,OK);if(!OK) { return R; }const bool Preserved=Preserve(Before,After,nullptr,{},Reused?FString():GraphPath,Reused?FString():Guid.ToString());Count(R,TEXT("find_bound_event_calls"));const UK2Node_ComponentBoundEvent* Current=FKismetEditorUtilities::FindBoundEventForComponent(BP,FName(*EventName),FName(*Component));Count(R,TEXT("find_bound_event_returns"));bool Selected=Loaded(Current)&&Loaded(Current->GetGraph())&&Current->GetGraph()->GetTypedOuter<UBlueprint>()==BP&&Current->GetGraph()->GetPathName().Equals(GraphPath,ESearchCase::CaseSensitive)&&Current->NodeGuid==Guid&&Current->ComponentPropertyName==FName(*Component)&&Current->DelegatePropertyName==FName(*EventName)&&Loaded(Current->DelegateOwnerClass)&&Current->CustomFunctionName==BoundName;
    if(Selected)
    {
        UFunction* CurrentSignature=Current->FindEventSignatureFunction();TStrongObjectPtr<UFunction> KeepCurrentSignature(CurrentSignature);const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();if(!Loaded(CurrentSignature)||CurrentSignature->GetFName()!=SignatureName) { Selected=false; }
        if(Selected) { for(TFieldIterator<FProperty> It(CurrentSignature);It;++It) { FProperty* P=*It;if(!P->HasAnyPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReturnParm)||(P->HasAnyPropertyFlags(CPF_OutParm)&&!P->HasAnyPropertyFlags(CPF_ReferenceParm))) { continue; }Count(R,TEXT("signature_pin_type_checks"));FEdGraphPinType Type;UEdGraphPin* Pin=Current->FindPin(P->GetFName(),EGPD_Output);if(!Pin||Pin->WasTrashed()||Pin->GetOwningNodeUnchecked()!=Current||!Schema->ConvertPropertyToPinType(P,Type)||Pin->PinType!=Type) { Selected=false;break; } } }
    }
    bool GeneratedBinding=false;if(Selected&&Loaded(BP->GeneratedClass.Get())) { UBlueprintGeneratedClass* Class=Cast<UBlueprintGeneratedClass>(BP->GeneratedClass.Get());if(Class) { for(UDynamicBlueprintBinding* D:Class->DynamicBindingObjects) { Count(R,TEXT("generated_binding_reads"));if(!Loaded(D)) { Selected=false;break; }if(UComponentDelegateBinding* B=Cast<UComponentDelegateBinding>(D)) { for(const auto& V:B->ComponentDelegateBindings) { if(V.ComponentPropertyName==FName(*Component)&&V.DelegatePropertyName==FName(*EventName)&&V.FunctionNameToBind==Current->CustomFunctionName) { GeneratedBinding=true; } } } } } }
    R.Data->SetBoolField(TEXT("old_selected_source_preserved"),Preserved);R.Data->SetBoolField(TEXT("current_bound_event_postcondition"),Selected);R.Data->SetBoolField(TEXT("generated_component_binding_verified"),GeneratedBinding);if(!Preserved||!Selected||!GeneratedBinding) { Fail(R,TEXT("umg_binding_postcondition_failed"),TEXT("Actual compiler/save returned; bound-event/signature/generated binding or preserved source proof failed"));return R; }if(!InitialOK||!R.Compiled.IsSet()||!*R.Compiled) { Fail(R,TEXT("umg_binding_compile_failed"),TEXT("Actual initial/final compiler predicate failed; real forced-save and legacy response retained"));return R; }if(!R.bSaved) { Fail(R,TEXT("umg_binding_save_failed"),TEXT("Forced save returned false; no replay"));return R; }R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
