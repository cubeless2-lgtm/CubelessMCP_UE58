#include "Compatibility/UE58/CubelessUMGTextBindingApi.h"
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
#include "EdGraphSchema_K2.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
namespace Cubeless::UE58::UMGTextBinding
{
namespace
{
using Variable::FResult;
bool Active=false;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool TextSafe(const FString& S,int32 Limit=65536) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(FResult& R,const TCHAR* K,int32 N=1) { auto J=R.Data->GetObjectField(TEXT("native_call_counts"));J->SetNumberField(K,J->GetNumberField(K)+N); }
void Init(FResult& R)
{
    R.bCompileRequested=R.bSaveRequested=true;R.Data->SetStringField(TEXT("operation"),TEXT("set_text_block_binding"));
    auto J=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("transaction_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("state_capture_calls"),TEXT("object_inventory_calls"),TEXT("slider_property_reads"),TEXT("checkbox_property_reads"),TEXT("policy_reads"),TEXT("source_field_lookups"),TEXT("class_variable_inventory_calls"),TEXT("add_member_variable_calls"),TEXT("add_member_variable_returns"),TEXT("explicit_skeleton_refresh_calls"),TEXT("blueprint_modify_calls"),TEXT("editor_binding_write_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("editor_binding_validate_calls"),TEXT("runtime_binding_reads"),TEXT("text_delegate_reads"),TEXT("runtime_widget_create_calls"),TEXT("runtime_text_getter_invoke_calls"),TEXT("persistent_getter_graph_create_calls")}) { J->SetNumberField(K,0); }R.Data->SetObjectField(TEXT("native_call_counts"),J);
    R.Data->SetBoolField(TEXT("total_SDK_side_effects_verified"),false);R.Data->SetBoolField(TEXT("runtime_text_display_verified"),false);R.Data->SetBoolField(TEXT("cold_saved_readback_verified"),false);R.Data->SetBoolField(TEXT("no_fake_getter_graph"),true);R.Data->SetBoolField(TEXT("auto_retry_safe"),false);
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
UWidgetBlueprint* Load(const FString& Name,FString& Package,FResult& R)
{
    Package=NormalizeWidgetAssetPath(Name);if(!GamePackage(Package,R)) { return nullptr; }
    Count(R,TEXT("asset_load_calls"));UWidgetBlueprint* BP=Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(Package));Count(R,TEXT("asset_load_returns"));
    if(!BP) { Count(R,TEXT("asset_load_calls"));BP=Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetObjectPathFromPackage(Package)));Count(R,TEXT("asset_load_returns")); }
    if(!Loaded(BP)||!Loaded(BP->WidgetTree)) { R.bChangeStateKnown=BP==nullptr;Fail(R,TEXT("umg_blueprint_not_found"),FString::Printf(TEXT("Failed to load Widget Blueprint/tree: %s"),*Package));return nullptr; }
    if(BP->GetOutermost()->GetName()!=Package||BP->WidgetTree->GetOuter()!=BP||ToolingPackage(BP->GetClass())) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_widget_tree_owner_invalid"),TEXT("Current package and source WidgetTree ownership do not match"));return nullptr; }
    R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());return BP;
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
bool BindingAndVariableState(UWidgetBlueprint* BP,const TSharedRef<FJsonObject>& J,FResult& R);
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
    J->SetObjectField(TEXT("graph_rows"),GraphRows);if(!BindingAndVariableState(BP,J,R)) { return J; }OK=true;return J;
}
bool ExactValue(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()) { return false; }FString X,Y;FJsonSerializer::Serialize(A,TEXT(""),TJsonWriterFactory<>::Create(&X));FJsonSerializer::Serialize(B,TEXT(""),TJsonWriterFactory<>::Create(&Y));return X.Equals(Y,ESearchCase::CaseSensitive);
}
bool Compile(UWidgetBlueprint* BP,FResult& R,const TCHAR* Phase)
{
    R.Stage=Phase;R.bMutationAttempted=R.bCompileAttempted=true;Count(R,TEXT("explicit_compile_calls"));FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);const bool Good=Log.NumErrors==0&&(BP->Status==BS_UpToDate||BP->Status==BS_UpToDateWithWarnings)&&Loaded(BP->GeneratedClass.Get());R.Compiled=Good;
    TArray<TSharedPtr<FJsonValue>> Rows;if(R.Data->HasField(TEXT("compiler_results"))) { Rows=R.Data->GetArrayField(TEXT("compiler_results")); }auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("phase"),Phase);C->SetNumberField(TEXT("errors"),Log.NumErrors);C->SetNumberField(TEXT("warnings"),Log.NumWarnings);C->SetNumberField(TEXT("blueprint_status"),static_cast<int32>(BP->Status));C->SetBoolField(TEXT("current_editor_compile_verified"),Good);Rows.Add(MakeShared<FJsonValueObject>(C));R.Data->SetArrayField(TEXT("compiler_results"),Rows);return Good;
}
void Save(UWidgetBlueprint* BP,const FString& Package,FResult& R)
{
    Compile(BP,R,TEXT("full_compile"));R.Stage=TEXT("save");R.bSaveAttempted=true;Count(R,TEXT("explicit_save_calls"));R.bSaved=UEditorAssetLibrary::SaveAsset(WidgetObjectPathFromPackage(Package),false);R.Data->SetBoolField(TEXT("save_returned"),true);R.Data->SetBoolField(TEXT("force_save_return"),R.bSaved);
}
TSharedRef<FJsonObject> End(UWidgetBlueprint* BP,FResult& R,const TSharedRef<FJsonObject>& Before,bool& OK)
{
    auto After=Capture(BP,R,OK);R.Data->SetObjectField(TEXT("after_state"),After);R.Data->SetBoolField(TEXT("package_dirty_after"),BP->GetOutermost()->IsDirty());if(!OK) { R.Data->SetStringField(TEXT("preceding_error_code"),R.ErrorCode);R.Data->SetStringField(TEXT("preceding_error"),R.Error);R.bChangeStateKnown=false;R.Stage=TEXT("observation_incomplete");Fail(R,TEXT("umg_text_binding_observation_incomplete"),TEXT("Actual accepted/native compiler/save prefix retained; current source inventory incomplete")); }
    else { R.bChangeStateKnown=true;R.bChanged|=!Json(Before).Equals(Json(After),ESearchCase::CaseSensitive); }return After;
}

bool BindingRow(const FDelegateEditorBinding& B,UWidgetBlueprint* BP,TSharedRef<FJsonObject>& Out,TSharedRef<FJsonObject>& Structs)
{
    if(!TextSafe(B.ObjectName,NAME_SIZE-1)||B.PropertyName.ToString().Len()+8>=NAME_SIZE||B.SourcePath.Segments.Num()>64) { return false; }
    Out=MakeShared<FJsonObject>();Structs=MakeShared<FJsonObject>();Out->SetStringField(TEXT("object"),B.ObjectName);Out->SetStringField(TEXT("property"),B.PropertyName.ToString());Out->SetStringField(TEXT("function"),B.FunctionName.ToString());Out->SetStringField(TEXT("source_property"),B.SourceProperty.ToString());Out->SetStringField(TEXT("member_guid"),B.MemberGuid.ToString());Out->SetNumberField(TEXT("kind"),static_cast<int32>(B.Kind));TArray<TSharedPtr<FJsonValue>> Segments,Owners;
    for(int32 I=0;I<B.SourcePath.Segments.Num();++I)
    {
        const FEditorPropertyPathSegment& S=B.SourcePath.Segments[I];UStruct* Owner=S.GetStruct();if(Owner&&!Loaded(Owner)) { return false; }auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),S.GetMemberName().ToString());Row->SetStringField(TEXT("guid"),S.GetMemberGuid().ToString());FString StructName=Owner?Owner->GetPathName():FString();Owners.Add(MakeShared<FJsonValueString>(StructName));
        if(I==0) { if(UClass* C=Cast<UClass>(Owner)) { if(C->ClassGeneratedBy==BP||(Loaded(BP->ParentClass.Get())&&BP->ParentClass->IsChildOf(C))) { StructName=TEXT("<this_widget_or_inherited_root_class>"); } } }
        Row->SetStringField(TEXT("owner_semantics"),StructName);Segments.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("segments"),Segments);Structs->SetArrayField(TEXT("actual_segment_owner_paths"),Owners);return true;
}
bool BindingAndVariableState(UWidgetBlueprint* BP,const TSharedRef<FJsonObject>& J,FResult& R)
{
    if(BP->Bindings.Num()>4096||BP->NewVariables.Num()>4096) { return false; }TArray<TSharedPtr<FJsonValue>> Bindings,ActualOwners;
    for(const FDelegateEditorBinding& B:BP->Bindings) { auto Row=MakeShared<FJsonObject>(),Owners=MakeShared<FJsonObject>();if(!BindingRow(B,BP,Row,Owners)) { return false; }Bindings.Add(MakeShared<FJsonValueObject>(Row));ActualOwners.Add(MakeShared<FJsonValueObject>(Owners)); }
    J->SetArrayField(TEXT("binding_rows"),Bindings);J->SetArrayField(TEXT("binding_owner_observations"),ActualOwners);auto Vars=MakeShared<FJsonObject>();
    for(const FBPVariableDescription& V:BP->NewVariables)
    {
        const FString Name=V.VarName.ToString();if(Vars->HasField(Name)||V.MetaDataArray.Num()>1024) { return false; }auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("guid"),V.VarGuid.ToString());A->SetStringField(TEXT("category"),V.Category.ToString());A->SetStringField(TEXT("friendly_name"),V.FriendlyName);A->SetStringField(TEXT("property_flags"),LexToString(V.PropertyFlags));A->SetStringField(TEXT("rep_notify"),V.RepNotifyFunc.ToString());A->SetNumberField(TEXT("replication_condition"),static_cast<int32>(V.ReplicationCondition));A->SetStringField(TEXT("default"),V.DefaultValue);A->SetStringField(TEXT("pin_category"),V.VarType.PinCategory.ToString());A->SetStringField(TEXT("pin_subcategory"),V.VarType.PinSubCategory.ToString());A->SetStringField(TEXT("pin_object"),V.VarType.PinSubCategoryObject.IsValid()?V.VarType.PinSubCategoryObject->GetPathName():FString());A->SetNumberField(TEXT("container"),static_cast<int32>(V.VarType.ContainerType));A->SetBoolField(TEXT("const"),V.VarType.bIsConst);A->SetBoolField(TEXT("reference"),V.VarType.bIsReference);A->SetBoolField(TEXT("weak"),V.VarType.bIsWeakPointer);TArray<TSharedPtr<FJsonValue>> Metadata;for(const FBPVariableMetaDataEntry& M:V.MetaDataArray) { auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("key"),M.DataKey.ToString());Q->SetStringField(TEXT("value"),M.DataValue);Metadata.Add(MakeShared<FJsonValueObject>(Q)); }A->SetArrayField(TEXT("metadata"),Metadata);Vars->SetObjectField(Name,A);
    }
    J->SetObjectField(TEXT("source_variable_rows"),Vars);return true;
}
bool Preserve(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,const FString& Target,FName Source,bool SourceAdded)
{
    for(const TCHAR* K:{TEXT("blueprint"),TEXT("asset_class"),TEXT("parent_class"),TEXT("widget_tree"),TEXT("root")}) { if(!Before->GetStringField(K).Equals(After->GetStringField(K),ESearchCase::CaseSensitive)) { return false; } }
    for(const TCHAR* K:{TEXT("widget_rows"),TEXT("variable_guids"),TEXT("graph_rows"),TEXT("source_variable_rows")})
    {
        const auto Old=Before->GetObjectField(K),Current=After->GetObjectField(K);for(const auto& Pair:Old->Values) { if(!ExactValue(Pair.Value,Current->TryGetField(FString(Pair.Key.ToView())))) { return false; } }
        if(FString(K)!=TEXT("source_variable_rows")&&Old->Values.Num()!=Current->Values.Num()) { return false; }
    }
    const auto OV=Before->GetObjectField(TEXT("source_variable_rows")),NV=After->GetObjectField(TEXT("source_variable_rows"));if(NV->Values.Num()!=OV->Values.Num()+(SourceAdded?1:0)||(SourceAdded&&!NV->HasField(Source.ToString()))) { return false; }
    TArray<TSharedPtr<FJsonValue>> OldBindings,NewBindings;for(const auto& V:Before->GetArrayField(TEXT("binding_rows"))) { const auto B=V->AsObject();if(!(FName(*B->GetStringField(TEXT("object")))==FName(*Target)&&FName(*B->GetStringField(TEXT("property")))==TEXT("Text"))) { OldBindings.Add(V); } }for(const auto& V:After->GetArrayField(TEXT("binding_rows"))) { const auto B=V->AsObject();if(!(FName(*B->GetStringField(TEXT("object")))==FName(*Target)&&FName(*B->GetStringField(TEXT("property")))==TEXT("Text"))) { NewBindings.Add(V); } }
    return ExactValue(MakeShared<FJsonValueArray>(OldBindings),MakeShared<FJsonValueArray>(NewBindings));
}
FProperty* SourceProperty(UWidgetBlueprint* BP,FName Name,FResult& R)
{
    for(UClass* C:{BP->SkeletonGeneratedClass.Get(),BP->GeneratedClass.Get(),BP->ParentClass.Get()}) { if(!C) { continue; }if(!Loaded(C)) { return nullptr; }Count(R,TEXT("source_field_lookups"));if(FProperty* P=FindFProperty<FProperty>(C,Name)) { return P; } }return nullptr;
}
bool TextType(const FEdGraphPinType& T)
{
    return T.PinCategory==UEdGraphSchema_K2::PC_Text&&T.PinSubCategory.IsNone()&&!T.PinSubCategoryObject.IsValid()&&T.ContainerType==EPinContainerType::None&&!T.bIsReference&&!T.bIsConst;
}
void Partial(UWidgetBlueprint* BP,FResult& R,const TSharedRef<FJsonObject>& Before)
{
    bool OK=false;End(BP,R,Before,OK);
}
FResult Run(const FString& WidgetName,const FString& TextBlockName,const FString& BindingProperty,const FString& BindingType)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_binding_type"),BindingType);R.Data->SetBoolField(TEXT("binding_type_ignored"),true);R.Data->SetStringField(TEXT("effective_target_property"),TEXT("Text"));R.Data->SetStringField(TEXT("functional_repair_delta"),TEXT("Map original public four fields to actual Text PropertyPath binding; create/reuse FText source member and Bindings/runtime row; no broken Get<property> graph"));
    if(!Admit(R,{WidgetName,TextBlockName,BindingProperty,BindingType})) { return R; }FActiveScope Scope;Count(R,TEXT("transaction_calls"));FScopedTransaction Tx(FText::FromString(TEXT("MCP set_text_block_binding")));
    R.Stage=TEXT("load_widget_blueprint");FString Package;UWidgetBlueprint* BP=Load(WidgetName,Package,R);if(!BP) { return R; }TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);if(BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->Status==BS_BeingCreated) { Fail(R,TEXT("umg_text_binding_compilation_state_invalid"),TEXT("Source needs an idle, initialized Blueprint with public structural regeneration available"));return R; }
    if(!NameValid(TextBlockName,false,R)||!NameValid(BindingProperty,false,R)) { return R; }const FName TargetName(*TextBlockName),SourceName(*BindingProperty);if(TargetName.IsNone()||SourceName.IsNone()) { Fail(R,TEXT("umg_text_binding_name_invalid"),TEXT("A named TextBlock and source FText property are required"));return R; }
    UTextBlock* Target=Cast<UTextBlock>(BP->WidgetTree->FindWidget(TargetName));if(!Loaded(Target)||Target->GetOuter()!=BP->WidgetTree) { Fail(R,TEXT("umg_text_binding_target_missing"),TEXT("Failed to find current owned TextBlock before any source-member write"));return R; }TStrongObjectPtr<UTextBlock> KeepTarget(Target);const FString ActualTarget=Target->GetName();Count(R,TEXT("text_delegate_reads"));FDelegateProperty* Destination=FindFProperty<FDelegateProperty>(Target->GetClass(),TEXT("TextDelegate"));if(!Destination||!Loaded(Destination->SignatureFunction)||Destination->SignatureFunction->NumParms!=1||!CastField<FTextProperty>(Destination->SignatureFunction->GetReturnProperty())) { Fail(R,TEXT("umg_text_binding_destination_unavailable"),TEXT("Actual TextDelegate must return one FText value"));return R; }TStrongObjectPtr<UFunction> KeepDestination(Destination->SignatureFunction);
    Count(R,TEXT("policy_reads"));const bool Allowed=BP->ArePropertyBindingsAllowed();R.Data->SetBoolField(TEXT("property_binding_policy_allows"),Allowed);if(!Allowed) { Fail(R,TEXT("umg_text_binding_policy_refused"),TEXT("Current public Widget Blueprint policy disallows property bindings; no settings override"));return R; }
    for(UClass* C:{BP->SkeletonGeneratedClass.Get(),BP->GeneratedClass.Get(),BP->ParentClass.Get()}) { if(C&&!Loaded(C)) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_text_binding_class_unavailable"),TEXT("Current source classes must be loaded before field preflight"));return R; } }
    bool Declared=false;for(const FBPVariableDescription& V:BP->NewVariables) { if(V.VarName==SourceName) { if(Declared||!TextType(V.VarType)) { Fail(R,TEXT("umg_text_binding_source_type_mismatch"),TEXT("Existing source declaration must be a unique scalar FText"));return R; }Declared=true; } }
    FProperty* Existing=SourceProperty(BP,SourceName,R);if(Existing&&(!CastField<FTextProperty>(Existing)||Existing->ArrayDim!=1)) { Fail(R,TEXT("umg_text_binding_source_type_mismatch"),TEXT("Existing source property must be scalar FText; no replacement/coercion"));return R; }
    if(!Existing&&!Declared)
    {
        Count(R,TEXT("class_variable_inventory_calls"));TSet<FName> Vars;FBlueprintEditorUtils::GetClassVariableList(BP,Vars,true);bool Collision=Vars.Contains(SourceName)||BP->WidgetVariableNameToGuidMap.Contains(SourceName)||BP->WidgetTree->FindWidget(SourceName)!=nullptr;
        for(UClass* C:{BP->SkeletonGeneratedClass.Get(),BP->GeneratedClass.Get(),BP->ParentClass.Get()}) { if(C&&C->FindFunctionByName(SourceName)) { Collision=true; } }TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(UEdGraph* G:Graphs) { if(Loaded(G)&&G->GetFName()==SourceName) { Collision=true; } }
        if(Collision) { Fail(R,TEXT("umg_text_binding_source_name_collision"),TEXT("Existing member/widget/function/graph name refuses source creation before mutation"));return R; }
    }
    int32 TargetIndex=INDEX_NONE,Matches=0;for(int32 I=0;I<BP->Bindings.Num();++I) { const FDelegateEditorBinding& B=BP->Bindings[I];if(!TextSafe(B.ObjectName,NAME_SIZE-1)||B.PropertyName.ToString().Len()+8>=NAME_SIZE) { Fail(R,TEXT("umg_text_binding_existing_name_overflow"),TEXT("Every existing binding name must be safe for public compiler FName conversion"));return R; }if(FName(*B.ObjectName)==Target->GetFName()&&B.PropertyName==TEXT("Text")) { TargetIndex=I;++Matches; } }
    if(Matches>1) { Fail(R,TEXT("umg_text_binding_duplicate_target_rows"),TEXT("Ambiguous duplicate target Text rows refused before mutation; no silent removal"));return R; }
    bool BeforeOK=false;const auto Before=Capture(BP,R,BeforeOK);R.Data->SetObjectField(TEXT("before_state"),Before);R.Data->SetBoolField(TEXT("package_dirty_before"),BP->GetOutermost()->IsDirty());if(!BeforeOK) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_text_binding_prewrite_observation_incomplete"),TEXT("Current bounded tree/graphs/bindings/variables must close before write"));return R; }
    R.Data->SetBoolField(TEXT("source_property_reused"),Existing!=nullptr);R.Data->SetBoolField(TEXT("source_property_declared_before"),Declared);R.Data->SetBoolField(TEXT("source_property_added"),false);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_from_member_add_possible"),false);
    if(!Existing)
    {
        R.Stage=TEXT("source_skeleton_refresh");R.bMutationAttempted=true;
        if(Declared) { Count(R,TEXT("explicit_skeleton_refresh_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);R.Data->SetBoolField(TEXT("explicit_skeleton_refresh_returned"),true); }
        else { FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Text;Count(R,TEXT("add_member_variable_calls"));const bool Added=FBlueprintEditorUtils::AddMemberVariable(BP,SourceName,Type);Count(R,TEXT("add_member_variable_returns"));R.Data->SetBoolField(TEXT("add_member_variable_return"),Added);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_from_member_add_possible"),true);R.Data->SetBoolField(TEXT("child_blueprint_variable_validation_possible"),true);if(!Added) { Fail(R,TEXT("umg_text_binding_source_creation_failed"),TEXT("Public AddMemberVariable returned false; actual helper prefix retained without replay"));Partial(BP,R,Before);return R; }R.bChanged=true;R.Data->SetBoolField(TEXT("source_property_added"),true); }
        Existing=SourceProperty(BP,SourceName,R);if(!Existing||!CastField<FTextProperty>(Existing)||Existing->ArrayDim!=1) { Fail(R,TEXT("umg_text_binding_source_refresh_incomplete"),TEXT("Source FText field unavailable after actual public skeleton work; no extra compile/retry"));Partial(BP,R,Before);R.bChangeStateKnown=false;return R; }
    }
    // Re-resolve after any structural callbacks; do not retain FProperty across regeneration.
    Target=Cast<UTextBlock>(BP->WidgetTree->FindWidget(TargetName));if(!Loaded(Target)||Target->GetOuter()!=BP->WidgetTree) { Fail(R,TEXT("umg_text_binding_target_refresh_incomplete"),TEXT("Actual source member prefix retained; target unavailable after structural callbacks"));Partial(BP,R,Before);R.bChangeStateKnown=false;return R; }KeepTarget.Reset(Target);Count(R,TEXT("text_delegate_reads"));Destination=FindFProperty<FDelegateProperty>(Target->GetClass(),TEXT("TextDelegate"));if(!Destination||!Loaded(Destination->SignatureFunction)) { Fail(R,TEXT("umg_text_binding_destination_refresh_incomplete"),TEXT("Actual destination unavailable after source refresh"));Partial(BP,R,Before);return R; }
    FDelegateEditorBinding Desired;Desired.ObjectName=Target->GetName();Desired.PropertyName=TEXT("Text");Desired.SourceProperty=Existing->GetFName();Desired.SourcePath=FEditorPropertyPath(TArray<FFieldVariant>{FFieldVariant(Existing)});Desired.Kind=EBindingKind::Property;FText Why;if(Desired.SourcePath.IsEmpty()||!Desired.SourcePath.Validate(Destination,Why)) { Fail(R,TEXT("umg_text_binding_path_invalid"),Why.IsEmpty()?TEXT("Public nonempty FText property path validation failed"):Why.ToString());Partial(BP,R,Before);return R; }
    auto Wanted=MakeShared<FJsonObject>(),WantedOwners=MakeShared<FJsonObject>();if(!BindingRow(Desired,BP,Wanted,WantedOwners)) { Fail(R,TEXT("umg_text_binding_path_capture_incomplete"),TEXT("Desired path must be bounded/current"));Partial(BP,R,Before);return R; }
    // Re-find the target row after skeleton callbacks rather than reusing an old index.
    TargetIndex=INDEX_NONE;Matches=0;for(int32 I=0;I<BP->Bindings.Num();++I) { if(FName(*BP->Bindings[I].ObjectName)==Target->GetFName()&&BP->Bindings[I].PropertyName==TEXT("Text")) { TargetIndex=I;++Matches; } }if(Matches>1) { Fail(R,TEXT("umg_text_binding_duplicate_after_refresh"),TEXT("Actual source refresh returned duplicate target bindings; no overwrite/replay"));Partial(BP,R,Before);return R; }
    bool Matching=false;if(TargetIndex!=INDEX_NONE) { auto Row=MakeShared<FJsonObject>(),Owners=MakeShared<FJsonObject>();Matching=BindingRow(BP->Bindings[TargetIndex],BP,Row,Owners)&&ExactValue(MakeShared<FJsonValueObject>(Row),MakeShared<FJsonValueObject>(Wanted)); }R.Data->SetBoolField(TEXT("editor_binding_matching_before"),Matching);R.Data->SetObjectField(TEXT("desired_binding"),Wanted);
    if(!Matching) { R.Stage=TEXT("write_editor_binding");R.bMutationAttempted=true;Count(R,TEXT("blueprint_modify_calls"));BP->Modify();Count(R,TEXT("editor_binding_write_calls"));if(TargetIndex==INDEX_NONE) { BP->Bindings.Add(Desired); }else { BP->Bindings[TargetIndex]=Desired; }R.bChanged=true;R.Data->SetBoolField(TEXT("editor_binding_write_returned"),true);Count(R,TEXT("explicit_skeleton_refresh_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);R.Data->SetBoolField(TEXT("editor_binding_skeleton_refresh_returned"),true); }
    Save(BP,Package,R);auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("binding_name"),BindingProperty);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetStringField(TEXT("binding_name"),BindingProperty);R.Data->SetStringField(TEXT("actual_source_property"),SourceName.ToString());R.Data->SetStringField(TEXT("target_text_block"),ActualTarget);
    R.Stage=TEXT("after_save_observation");bool OK=false;const auto After=End(BP,R,Before,OK);if(!OK) { return R; }const bool Preserved=Preserve(Before,After,ActualTarget,SourceName,R.Data->GetBoolField(TEXT("source_property_added")));Target=Cast<UTextBlock>(BP->WidgetTree->FindWidget(TargetName));Existing=SourceProperty(BP,SourceName,R);UWidgetBlueprintGeneratedClass* Class=Cast<UWidgetBlueprintGeneratedClass>(BP->GeneratedClass.Get());TStrongObjectPtr<UWidgetBlueprintGeneratedClass> KeepClass(Class);bool Selected=Loaded(Target)&&Target->GetOuter()==BP->WidgetTree&&Loaded(Class)&&Class->ClassGeneratedBy==BP&&Existing&&CastField<FTextProperty>(Existing)&&Existing->ArrayDim==1;
    Count(R,TEXT("source_field_lookups"));const FTextProperty* GeneratedText=Loaded(Class)?FindFProperty<FTextProperty>(Class,SourceName):nullptr;Selected=Selected&&GeneratedText&&GeneratedText->ArrayDim==1;int32 CurrentCount=0;const FDelegateEditorBinding* Current=nullptr;for(const FDelegateEditorBinding& B:BP->Bindings) { if(FName(*B.ObjectName)==TargetName&&B.PropertyName==TEXT("Text")) { Current=&B;++CurrentCount; } }Selected=Selected&&CurrentCount==1&&Current&&Current->Kind==EBindingKind::Property&&Current->SourceProperty==SourceName&&Current->FunctionName.IsNone()&&!Current->MemberGuid.IsValid()&&Current->SourcePath.Segments.Num()==1&&Current->SourcePath.Segments[0].GetMemberName()==SourceName;
    if(Selected) { Count(R,TEXT("editor_binding_validate_calls"));FCompilerResultsLog Log;Selected=Current->IsBindingValid(Class,BP,Log)&&Log.NumErrors==0;R.Data->SetNumberField(TEXT("binding_validation_errors"),Log.NumErrors);R.Data->SetNumberField(TEXT("binding_validation_warnings"),Log.NumWarnings);const FObjectPropertyBase* WidgetProperty=FindFProperty<FObjectPropertyBase>(Class,TargetName);Selected=Selected&&WidgetProperty&&Loaded(WidgetProperty->PropertyClass)&&Target->IsA(WidgetProperty->PropertyClass); }
    int32 RuntimeCount=0;if(Selected&&Class->Bindings.Num()<=4096) { for(const FDelegateRuntimeBinding& B:Class->Bindings) { Count(R,TEXT("runtime_binding_reads"));if(!TextSafe(B.ObjectName,NAME_SIZE-1)) { Selected=false;continue; }if(FName(*B.ObjectName)==TargetName&&B.PropertyName==TEXT("Text")) { ++RuntimeCount;const bool Match=B.Kind==EBindingKind::Property&&B.FunctionName.IsNone()&&B.SourcePath.GetNumSegments()==1&&B.SourcePath.GetSegment(0).GetName()==SourceName;if(!Match) { Selected=false; } } } }else { Selected=false; }
    Selected=Selected&&RuntimeCount==1;R.Data->SetBoolField(TEXT("old_selected_source_preserved"),Preserved);R.Data->SetBoolField(TEXT("current_editor_and_runtime_binding_verified"),Selected);R.Data->SetNumberField(TEXT("selected_runtime_binding_count"),RuntimeCount);R.Data->SetBoolField(TEXT("SDK_binding_owner_rebase_qualified"),true);
    if(!Preserved||!Selected) { Fail(R,TEXT("umg_text_binding_postcondition_failed"),TEXT("Actual compiler/save prefix retained; selected source/Path/editor/runtime row or other fields did not close"));return R; }if(!R.Compiled.IsSet()||!*R.Compiled) { Fail(R,TEXT("umg_text_binding_compile_failed"),TEXT("Actual full compiler failed; original forced save and legacy response retained"));return R; }if(!R.bSaved) { Fail(R,TEXT("umg_text_binding_save_failed"),TEXT("Forced save returned false; no second save/retry"));return R; }R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
Variable::FResult SetTextBinding(const FString& WidgetName,const FString& TextBlockName,const FString& BindingProperty,const FString& BindingType)
{
    return Run(WidgetName,TextBlockName,BindingProperty,BindingType);
}
}
