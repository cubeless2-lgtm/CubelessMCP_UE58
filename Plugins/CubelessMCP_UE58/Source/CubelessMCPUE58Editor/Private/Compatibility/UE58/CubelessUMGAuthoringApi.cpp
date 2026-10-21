#include "Compatibility/UE58/CubelessUMGAuthoringApi.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
namespace Cubeless::UE58::UMGAuthoring
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
    R.bCompileRequested=R.bSaveRequested=true;
    R.Data->SetStringField(TEXT("operation"),Operation);
    auto J=MakeShared<FJsonObject>();
    for(const TCHAR* K:{TEXT("transaction_calls"),TEXT("asset_exists_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("parent_class_load_calls"),TEXT("parent_class_load_returns"),TEXT("parent_class_find_calls"),TEXT("package_create_calls"),TEXT("package_create_returns"),TEXT("blueprint_create_calls"),TEXT("blueprint_create_returns"),TEXT("widget_construct_calls"),TEXT("widget_construct_returns"),TEXT("widget_tree_modify_calls"),TEXT("root_widget_assignments"),TEXT("text_set_calls"),TEXT("text_child_add_calls"),TEXT("text_child_add_returns"),TEXT("canvas_child_add_calls"),TEXT("canvas_child_add_returns"),TEXT("slot_position_set_calls"),TEXT("slot_size_set_calls"),TEXT("is_variable_assignments"),TEXT("on_variable_added_calls"),TEXT("package_dirty_calls"),TEXT("asset_created_calls"),TEXT("mark_structural_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("state_capture_calls"),TEXT("object_inventory_calls"),TEXT("name_collision_checks"),TEXT("unique_name_calls")}) { J->SetNumberField(K,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),J);
    R.Data->SetStringField(TEXT("style_scope"),TEXT("Original font_size/color/background_color inputs are preserved but ignored by the original C++ handlers. No style setter is added."));
    R.Data->SetBoolField(TEXT("runtime_widget_execution_verified"),false);
    R.Data->SetBoolField(TEXT("cold_saved_readback_verified"),false);
    R.Data->SetBoolField(TEXT("total_SDK_side_effects_verified"),false);
    R.Data->SetBoolField(TEXT("implicit_structural_skeleton_compile_possible"),true);
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
struct FGeometry { bool HasPosition=false,HasSize=false;FVector2D Position=FVector2D::ZeroVector,Size=FVector2D::ZeroVector; };
bool ProjectVector(const TSharedPtr<FJsonValue>& V,bool& Present,FVector2D& Out,FResult& R)
{
    Present=false;if(!V.IsValid()||V->Type!=EJson::Array||V->AsArray().Num()<2) { return true; }
    const auto& A=V->AsArray();double X=0,Y=0;
    if(A[0].IsValid()&&A[0]->Type==EJson::Number) { X=A[0]->AsNumber(); }
    if(A[1].IsValid()&&A[1]->Type==EJson::Number) { Y=A[1]->AsNumber(); }
    if(!FMath::IsFinite(X)||!FMath::IsFinite(Y)||FMath::Abs(X)>static_cast<double>(MAX_flt)||FMath::Abs(Y)>static_cast<double>(MAX_flt)) { Fail(R,TEXT("umg_geometry_nonfinite"),TEXT("Original slot float32 projection must remain finite before any widget allocation"));return false; }
    Out=FVector2D(X,Y);Present=true;return true;
}
bool Geometry(const FString& PositionJson,const FString& SizeJson,const FString& ColorJson,const FString& BackgroundJson,bool Button,FGeometry& G,FResult& R)
{
    TSharedPtr<FJsonValue> P,S,C,B;
    if(!BoxValue(PositionJson,P,R)||!BoxValue(SizeJson,S,R)||!BoxValue(ColorJson,C,R)||(Button&&!BoxValue(BackgroundJson,B,R))) { return false; }
    R.Data->SetField(TEXT("requested_position"),P);R.Data->SetField(TEXT("requested_size"),S);R.Data->SetField(TEXT("ignored_requested_color"),C);if(Button) { R.Data->SetField(TEXT("ignored_requested_background_color"),B); }
    return ProjectVector(P,G.HasPosition,G.Position,R)&&ProjectVector(S,G.HasSize,G.Size,R);
}

FString NormalizeContentDirectory(FString PackagePath)
{
	PackagePath = FPackageName::ExportTextPathToObjectPath(PackagePath).TrimStartAndEnd();
	PackagePath.TrimQuotesInline();
	PackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (PackagePath.EndsWith(TEXT(".uasset"), ESearchCase::IgnoreCase))
	{
		PackagePath.LeftChopInline(7);
	}
	if (PackagePath.Contains(TEXT(".")))
	{
		PackagePath = FPackageName::ObjectPathToPackageName(PackagePath);
	}
	while (PackagePath.Len() > 1 && PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath.LeftChopInline(1);
	}
	return PackagePath;
}

bool ValidateContentDirectory(const FString& PackagePath, FString& OutError)
{
	if (PackagePath.IsEmpty())
	{
		OutError = TEXT("'path' cannot be empty");
		return false;
	}
	if (PackagePath != TEXT("/Game") && !PackagePath.StartsWith(TEXT("/Game/")))
	{
		OutError = TEXT("'path' must be under /Game");
		return false;
	}
	if (PackagePath.Contains(TEXT(".")))
	{
		OutError = TEXT("'path' must be a package directory, not an object path");
		return false;
	}
	if (PackagePath != TEXT("/Game"))
	{
		FText FailureReason;
		if (!FPackageName::IsValidLongPackageName(PackagePath, false, &FailureReason))
		{
			OutError = FString::Printf(TEXT("'path' is not a valid long package path: %s"), *FailureReason.ToString());
			return false;
		}
	}
	return true;
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
UClass* Parent(FString Name,FResult& R,UClass*& AssetClass,bool& Utility)
{
    if(Name.Len()>=NAME_SIZE) { Fail(R,TEXT("umg_parent_name_too_long"),TEXT("Parent request must be shorter than NAME_SIZE before any load/find"));return nullptr; }
    Name.TrimStartAndEndInline();Name.TrimQuotesInline();
    if((!Name.StartsWith(TEXT("U"))&&(TEXT("U")+Name).Len()>=NAME_SIZE)||(TEXT("/Script/UMG.")+Name).Len()>=NAME_SIZE||(TEXT("/Script/Blutility.")+Name).Len()>=NAME_SIZE) { Fail(R,TEXT("umg_parent_name_too_long"),TEXT("Actual parent alias lookup paths must be shorter than NAME_SIZE before any load/find"));return nullptr; }
    UClass* EU=FindObject<UClass>(nullptr,TEXT("/Script/Blutility.EditorUtilityWidget"));
    UClass* Found=nullptr;
    if(Name.IsEmpty()||Name.Equals(TEXT("UserWidget"),ESearchCase::IgnoreCase)||Name.Equals(TEXT("UUserWidget"),ESearchCase::IgnoreCase)) { Found=UUserWidget::StaticClass(); }
    else if(Name.Equals(TEXT("EditorUtilityWidget"),ESearchCase::IgnoreCase)||Name.Equals(TEXT("UEditorUtilityWidget"),ESearchCase::IgnoreCase)||Name.Equals(TEXT("/Script/Blutility.EditorUtilityWidget"),ESearchCase::IgnoreCase)) { Found=EU; }
    else
    {
        if(Name.StartsWith(TEXT("/Script/"))||Name.StartsWith(TEXT("/Game/"))||Name.Contains(TEXT("."))) { Count(R,TEXT("parent_class_load_calls"));Found=LoadObject<UClass>(nullptr,*Name);Count(R,TEXT("parent_class_load_returns"));if(!Found) { Count(R,TEXT("parent_class_load_calls"));Found=LoadClass<UUserWidget>(nullptr,*Name);Count(R,TEXT("parent_class_load_returns")); } }
        if(!Found) { Count(R,TEXT("parent_class_find_calls"));Found=FindFirstObject<UClass>(*Name); }
        if(!Found&&!Name.StartsWith(TEXT("U"))) { Count(R,TEXT("parent_class_find_calls"));Found=FindFirstObject<UClass>(*(TEXT("U")+Name)); }
        for(const TCHAR* Prefix:{TEXT("/Script/UMG."),TEXT("/Script/Blutility.")}) { if(!Found) { Count(R,TEXT("parent_class_load_calls"));Found=LoadClass<UUserWidget>(nullptr,*(FString(Prefix)+Name));Count(R,TEXT("parent_class_load_returns")); } }
    }
    if(!Loaded(Found)||!Found->IsChildOf(UUserWidget::StaticClass())||Found->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)) { R.bChangeStateKnown=Found==nullptr;Fail(R,TEXT("umg_parent_unavailable"),TEXT("Loaded usable UUserWidget parent required; EditorUtility alias additionally requires its actual loaded class"));return nullptr; }
    for(UClass* C=Found;C;C=C->GetSuperClass()) { if(ToolingPackage(C)) { Fail(R,TEXT("umg_parent_tooling_dependency"),TEXT("Authored Widget Blueprint parent cannot depend on MCP tooling classes"));return nullptr; } }
    Utility=Loaded(EU)&&Found->IsChildOf(EU);
    AssetClass=Utility?FindObject<UClass>(nullptr,TEXT("/Script/Blutility.EditorUtilityWidgetBlueprint")):UWidgetBlueprint::StaticClass();
    if(!Loaded(AssetClass)||!AssetClass->IsChildOf(UWidgetBlueprint::StaticClass())) { Fail(R,TEXT("umg_asset_class_unavailable"),TEXT("Exact loaded Widget Blueprint asset class required; no forced Blutility module load or Actor substitute"));return nullptr; }
    R.Data->SetBoolField(TEXT("editor_utility_widget"),Utility);R.Data->SetStringField(TEXT("resolved_parent_class"),Found->GetPathName());R.Data->SetStringField(TEXT("resolved_blueprint_asset_class"),AssetClass->GetPathName());return Found;
}
bool NameValid(const FString& Name,bool Empty,FResult& R)
{
    if(Name.Len()>=NAME_SIZE||!TextSafe(Name,1024)||(!Empty&&Name.IsEmpty())||(!Name.IsEmpty()&&!FName(*Name).IsValidXName(INVALID_OBJECTNAME_CHARACTERS))) { Fail(R,TEXT("umg_widget_name_invalid"),TEXT("Bounded valid UObject name required; empty child names retain automatic-name semantics"));return false; }return true;
}
FName PlannedName(UWidgetTree* Tree,UClass* Class,const FString& Name,FResult& R)
{
    if(!Name.IsEmpty()) { return FName(*Name); }Count(R,TEXT("unique_name_calls"));return MakeUniqueObjectName(Tree,Class,NAME_None);
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
TSharedRef<FJsonObject> Capture(UWidgetBlueprint* BP,FResult& R,bool& OK)
{
    Count(R,TEXT("state_capture_calls"));auto J=MakeShared<FJsonObject>();OK=false;
    if(!Loaded(BP)||!Loaded(BP->ParentClass.Get())||!Loaded(BP->WidgetTree)||BP->WidgetTree->GetOuter()!=BP||(BP->WidgetTree->RootWidget&&(!Loaded(BP->WidgetTree->RootWidget)||BP->WidgetTree->RootWidget->GetOuter()!=BP->WidgetTree))) { return J; }
    UWidgetTree* Tree=BP->WidgetTree;J->SetStringField(TEXT("blueprint"),BP->GetPathName());J->SetStringField(TEXT("asset_class"),BP->GetClass()->GetPathName());J->SetStringField(TEXT("parent_class"),BP->ParentClass?BP->ParentClass->GetPathName():FString());J->SetStringField(TEXT("widget_tree"),Tree->GetPathName());J->SetStringField(TEXT("root"),Loaded(Tree->RootWidget)?Tree->RootWidget->GetPathName():FString());J->SetBoolField(TEXT("package_dirty"),BP->GetOutermost()->IsDirty());
    TArray<UObject*> Objects;Count(R,TEXT("object_inventory_calls"));GetObjectsWithOuter(Tree,Objects,EGetObjectsFlags::IncludeNestedObjects);if(Objects.Num()>4096) { return J; }
    auto Rows=MakeShared<FJsonObject>();
    for(UObject* O:Objects)
    {
        if(!Loaded(O)) { return J; }UWidget* W=Cast<UWidget>(O);if(!W) { continue; }if(!W->IsIn(Tree)||ToolingPackage(W->GetClass())||(W->Slot&&!Loaded(W->Slot))) { return J; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("path"),W->GetPathName());Q->SetStringField(TEXT("class"),W->GetClass()->GetPathName());Q->SetStringField(TEXT("name"),W->GetName());Q->SetBoolField(TEXT("is_variable"),W->bIsVariable);Q->SetStringField(TEXT("slot"),Loaded(W->Slot)?W->Slot->GetPathName():FString());
        if(UPanelWidget* Panel=Cast<UPanelWidget>(W)) { const int32 N=Panel->GetChildrenCount();if(N<0||N>4096) { return J; }TArray<TSharedPtr<FJsonValue>> Children;for(int32 I=0;I<N;++I) { UWidget* C=Panel->GetChildAt(I);if(!Loaded(C)||C->GetOuter()!=Tree) { return J; }Children.Add(MakeShared<FJsonValueString>(C->GetPathName())); }Q->SetArrayField(TEXT("children"),Children); }
        if(UTextBlock* T=Cast<UTextBlock>(W)) { Q->SetStringField(TEXT("text"),T->GetText().ToString());Q->SetNumberField(TEXT("font_size"),T->GetFont().Size); }
        if(UCanvasPanelSlot* S=Cast<UCanvasPanelSlot>(W->Slot)) { if(S->Content!=W||!Loaded(S->Parent)) { return J; }const FVector2D P=S->GetPosition(),Z=S->GetSize();if(!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)||!FMath::IsFinite(Z.X)||!FMath::IsFinite(Z.Y)) { return J; }Vec(Q,TEXT("position"),P);Vec(Q,TEXT("size"),Z); }
        Rows->SetObjectField(W->GetPathName(),Q);
    }
    J->SetObjectField(TEXT("widget_rows"),Rows);auto Guids=MakeShared<FJsonObject>();for(const auto& V:BP->WidgetVariableNameToGuidMap) { Guids->SetStringField(V.Key.ToString(),V.Value.ToString()); }J->SetObjectField(TEXT("variable_guids"),Guids);
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);if(Graphs.Num()>4096) { return J; }auto GraphRows=MakeShared<FJsonObject>();
    for(UEdGraph* G:Graphs) { if(!Loaded(G)||G->Nodes.Num()>65536) { return J; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("class"),G->GetClass()->GetPathName());Q->SetStringField(TEXT("guid"),G->GraphGuid.ToString());auto Nodes=MakeShared<FJsonObject>();for(UEdGraphNode* N:G->Nodes) { if(!Loaded(N)) { return J; }auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("class"),N->GetClass()->GetPathName());A->SetNumberField(TEXT("x"),N->NodePosX);A->SetNumberField(TEXT("y"),N->NodePosY);Nodes->SetObjectField(N->NodeGuid.ToString(),A); }Q->SetObjectField(TEXT("nodes"),Nodes);GraphRows->SetObjectField(G->GetPathName(),Q); }
    J->SetObjectField(TEXT("graph_rows"),GraphRows);OK=true;return J;
}
bool Preserve(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,const UWidget* Added,const UCanvasPanel* Root)
{
    for(const TCHAR* K:{TEXT("blueprint"),TEXT("asset_class"),TEXT("parent_class"),TEXT("widget_tree")}) { if(Before->GetStringField(K)!=After->GetStringField(K)) { return false; } }
    for(const TCHAR* K:{TEXT("widget_rows"),TEXT("variable_guids"),TEXT("graph_rows")})
    {
        const auto A=Before->GetObjectField(K),B=After->GetObjectField(K);
        for(const auto& Pair:A->Values)
        {
            const FString Key(Pair.Key.ToView());auto V=B->TryGetField(Key);if(!V.IsValid()) { return false; }
            if(Added&&Root&&FString(K)==TEXT("widget_rows")&&Key==Root->GetPathName())
            {
                if(Pair.Value->Type!=EJson::Object||V->Type!=EJson::Object) { return false; }
                const auto Old=Pair.Value->AsObject(),Current=V->AsObject();const auto& OldChildren=Old->GetArrayField(TEXT("children"));const auto& NewChildren=Current->GetArrayField(TEXT("children"));
                if(NewChildren.Num()!=OldChildren.Num()+1||NewChildren.Last()->AsString()!=Added->GetPathName()) { return false; }
                auto Copy=MakeShared<FJsonObject>(*Current);TArray<TSharedPtr<FJsonValue>> RetainedChildren=NewChildren;RetainedChildren.Pop();Copy->SetArrayField(TEXT("children"),RetainedChildren);V=MakeShared<FJsonValueObject>(Copy);
            }
            FString Left,Right;FJsonSerializer::Serialize(Pair.Value,TEXT(""),TJsonWriterFactory<>::Create(&Left));FJsonSerializer::Serialize(V,TEXT(""),TJsonWriterFactory<>::Create(&Right));if(!Left.Equals(Right,ESearchCase::CaseSensitive)) { return false; }
        }
    }
    return true;
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
void Finish(UWidgetBlueprint* BP,const FString& Package,FResult& R,const TSharedPtr<FJsonObject>& Before,UWidget* Added,UTextBlock* Label,UCanvasPanel* Root,const FGeometry* G,const FString& Text,bool Button)
{
    R.Stage=TEXT("structural_refresh");R.bMutationAttempted=true;Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    R.Stage=TEXT("compile");R.bCompileAttempted=true;Count(R,TEXT("explicit_compile_calls"));FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
    const bool CompileOK=Log.NumErrors==0&&(BP->Status==BS_UpToDate||BP->Status==BS_UpToDateWithWarnings)&&Loaded(BP->GeneratedClass.Get());R.Compiled=CompileOK;
    auto C=MakeShared<FJsonObject>();C->SetNumberField(TEXT("errors"),Log.NumErrors);C->SetNumberField(TEXT("warnings"),Log.NumWarnings);C->SetNumberField(TEXT("blueprint_status"),static_cast<int32>(BP->Status));C->SetBoolField(TEXT("generated_class_present"),BP->GeneratedClass!=nullptr);C->SetBoolField(TEXT("current_editor_compile_verified"),CompileOK);R.Data->SetObjectField(TEXT("compiler_result"),C);
    // Original SaveAndCompileWidgetBlueprint unconditionally saves after CompileBlueprint, even on compiler errors.
    R.Stage=TEXT("save");R.bSaveAttempted=true;Count(R,TEXT("explicit_save_calls"));R.bSaved=UEditorAssetLibrary::SaveAsset(WidgetObjectPathFromPackage(Package),false);R.Data->SetBoolField(TEXT("save_returned"),true);R.Data->SetBoolField(TEXT("force_save_return"),R.bSaved);
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("path"),Package);Legacy->SetStringField(TEXT("asset_path"),WidgetObjectPathFromPackage(Package));if(Added) { Legacy->SetStringField(TEXT("widget_name"),R.Data->GetStringField(TEXT("requested_component_name")));Legacy->SetStringField(TEXT("text"),Text); }else { Legacy->SetStringField(TEXT("name"),BP->GetName());Legacy->SetStringField(TEXT("parent_class"),Loaded(BP->ParentClass.Get())?BP->ParentClass->GetPathName():FString());Legacy->SetStringField(TEXT("blueprint_class"),BP->GetClass()->GetPathName()); }R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    R.Stage=TEXT("after_save_observation");bool Complete=false;const auto After=Capture(BP,R,Complete);R.Data->SetObjectField(TEXT("after_state"),After);R.Data->SetBoolField(TEXT("package_dirty_after_save"),BP->GetOutermost()->IsDirty());
    bool Preserved=Complete&&Loaded(Root)&&(!Before.IsValid()||Preserve(Before.ToSharedRef(),After,Added,Root));bool Selected=Complete&&Loaded(Root)&&BP->WidgetTree->RootWidget==Root&&Root->GetOuter()==BP->WidgetTree;
    if(!Added&&Selected) { Selected=BP->ParentClass->GetPathName()==R.Data->GetStringField(TEXT("resolved_parent_class"))&&BP->GetClass()->GetPathName()==R.Data->GetStringField(TEXT("resolved_blueprint_asset_class")); }
    if(Selected&&R.Data->GetObjectField(TEXT("native_call_counts"))->GetNumberField(TEXT("root_widget_assignments"))>0) { Selected=BP->WidgetVariableNameToGuidMap.Contains(Root->GetFName()); }
    if(Added)
    {
        Selected=Selected&&Loaded(Added)&&Added->GetOuter()==BP->WidgetTree&&Loaded(Added->Slot)&&Added->Slot->Parent==Root&&Added->Slot->Content==Added&&Root->HasChild(Added)&&BP->WidgetVariableNameToGuidMap.Contains(Added->GetFName());
        UTextBlock* TextWidget=Button?Label:Cast<UTextBlock>(Added);Selected=Selected&&Loaded(TextWidget)&&TextWidget->GetText().ToString().Equals(Text,ESearchCase::CaseSensitive);
        if(Button) { UButton* B=Cast<UButton>(Added);Selected=Selected&&Loaded(B)&&Loaded(Label)&&B->GetChildrenCount()==1&&B->GetChildAt(0)==Label&&Loaded(Label->Slot)&&Label->Slot->Parent==B; }
        else { Selected=Selected&&Added->bIsVariable; }
        FObjectPropertyBase* Member=Loaded(BP->GeneratedClass.Get())?FindFProperty<FObjectPropertyBase>(BP->GeneratedClass.Get(),Added->GetFName()):nullptr;
        const bool MemberOK=Member&&Loaded(Member->PropertyClass)&&Added->IsA(Member->PropertyClass);R.Data->SetBoolField(TEXT("generated_widget_variable_verified"),MemberOK);if(Member&&Loaded(Member->PropertyClass)) { R.Data->SetStringField(TEXT("generated_widget_variable_type"),Member->PropertyClass->GetPathName()); }Selected=Selected&&MemberOK;
        UCanvasPanelSlot* Slot=Cast<UCanvasPanelSlot>(Added->Slot);Selected=Selected&&Loaded(Slot);
        if(Selected&&G) { const FVector2D P=Slot->GetPosition(),Z=Slot->GetSize();if(G->HasPosition||!Button) { Selected=Selected&&P==FVector2D(static_cast<float>(G->Position.X),static_cast<float>(G->Position.Y)); }if(G->HasSize) { Selected=Selected&&Z==FVector2D(static_cast<float>(G->Size.X),static_cast<float>(G->Size.Y)); } }
    }
    R.Data->SetBoolField(TEXT("old_selected_source_preserved"),Preserved);R.Data->SetBoolField(TEXT("current_authored_widget_postcondition"),Selected);R.Data->SetStringField(TEXT("verification_scope"),TEXT("Current authored source WidgetTree/old widget rows/variable GUIDs/graph GUID-class-position and selected text/slot/containment. Graph pins, CDO, generated runtime widget behavior, total Slate/cache effects, cold disk equality and cooking are not certified."));
    if(!Complete) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_observation_incomplete"),TEXT("Actual compile/save prefix retained; current source observation did not close"));return; }
    R.bChangeStateKnown=true;R.bChanged|=!Preserved;if(!Selected||!Preserved) { Fail(R,TEXT("umg_source_postcondition_failed"),TEXT("Current authored widget or preserved source postcondition failed after actual compile/save"));return; }
    if(!CompileOK) { Fail(R,TEXT("umg_compile_failed"),TEXT("Original legacy response is retained separately; actual Editor compiler did not complete error-free"));return; }
    if(!R.bSaved) { Fail(R,TEXT("umg_save_failed"),TEXT("Original forced save returned false; no second save/retry"));return; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");
}
FResult Add(bool Button,const FString& WidgetName,const FString& ComponentName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson,const FString& BackgroundJson)
{
    FResult R;Init(R,Button?TEXT("add_button_to_widget"):TEXT("add_text_block_to_widget"));R.Data->SetStringField(TEXT("requested_component_name"),ComponentName.TrimStartAndEnd());R.Data->SetStringField(TEXT("ignored_requested_font_size"),LexToString(FontSize));
    if(!Admit(R,{WidgetName,ComponentName,Text,PositionJson,SizeJson,ColorJson,BackgroundJson})) { return R; }FActiveScope Scope;Count(R,TEXT("transaction_calls"));FScopedTransaction Tx(FText::FromString(Button?TEXT("MCP add_button_to_widget"):TEXT("MCP add_text_block_to_widget")));
    R.Stage=TEXT("load_widget_blueprint");FString Package;UWidgetBlueprint* BP=Load(WidgetName.TrimStartAndEnd(),Package,R);if(!BP) { return R; }TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);
    FGeometry G;if(!Geometry(PositionJson,SizeJson,ColorJson,BackgroundJson,Button,G,R)) { return R; }
    const FString Name=ComponentName.TrimStartAndEnd();if(!NameValid(Name,true,R)||!NameValid(Name+TEXT("_Text"),true,R)) { return R; }const FName Planned=PlannedName(BP->WidgetTree,Button?UButton::StaticClass():UTextBlock::StaticClass(),Name,R);
    if(BP->WidgetTree->RootWidget&&!Loaded(BP->WidgetTree->RootWidget)) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_root_unavailable"),TEXT("Current non-null root must be loaded before allocation"));return R; }
    const bool NeedsRoot=Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)==nullptr;
    if(!FreeName(BP,BP->WidgetTree,Planned,R)||(Button&&!FreeName(BP,BP->WidgetTree,FName(*(Name+TEXT("_Text"))),R))||(NeedsRoot&&!FreeName(BP,BP->WidgetTree,TEXT("RootCanvas"),R))) { return R; }
    if((Button&&Planned==FName(*(Name+TEXT("_Text"))))||(NeedsRoot&&(Planned==TEXT("RootCanvas")||(Button&&FName(*(Name+TEXT("_Text")))==TEXT("RootCanvas"))))) { Fail(R,TEXT("umg_planned_name_collision"),TEXT("Planned widget/root/text child names must differ before allocation"));return R; }
    bool BeforeOK=false;const auto Before=Capture(BP,R,BeforeOK);R.Data->SetObjectField(TEXT("before_state"),Before);if(!BeforeOK) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_prewrite_observation_incomplete"),TEXT("Current source tree inventory must close before allocation"));return R; }
    R.Stage=TEXT("construct_widget");R.bMutationAttempted=true;Count(R,TEXT("widget_construct_calls"));UWidget* Added=Button?static_cast<UWidget*>(BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Planned)):static_cast<UWidget*>(BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Planned));Count(R,TEXT("widget_construct_returns"));PublishWidget(R,TEXT("widget_return"),Added);if(!Loaded(Added)) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_widget_creation_failed"),TEXT("Widget constructor did not return a loaded object; no automatic cleanup/retry"));return R; }R.bChanged=true;TStrongObjectPtr<UWidget> KeepAdded(Added);
    UTextBlock* Label=Cast<UTextBlock>(Added);TStrongObjectPtr<UTextBlock> KeepLabel;
    if(Button)
    {
        Count(R,TEXT("widget_construct_calls"));Label=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),FName(*(Name+TEXT("_Text"))));Count(R,TEXT("widget_construct_returns"));PublishWidget(R,TEXT("button_text_return"),Label);if(Loaded(Label)) { KeepLabel.Reset(Label);Count(R,TEXT("text_set_calls"));Label->SetText(FText::FromString(Text));Count(R,TEXT("text_child_add_calls"));UPanelSlot* TextSlot=CastChecked<UButton>(Added)->AddChild(Label);Count(R,TEXT("text_child_add_returns"));PublishWidget(R,TEXT("button_text_slot_return"),TextSlot); }
    }
    else { Count(R,TEXT("text_set_calls"));Label->SetText(FText::FromString(Text)); }
    R.Stage=TEXT("attach_canvas");UCanvasPanel* Root=Canvas(BP,R);if(!Root) { return R; }TStrongObjectPtr<UCanvasPanel> KeepRoot(Root);Count(R,TEXT("canvas_child_add_calls"));UCanvasPanelSlot* Slot=Root->AddChildToCanvas(Added);Count(R,TEXT("canvas_child_add_returns"));PublishWidget(R,TEXT("canvas_slot_return"),Slot);
    if(Loaded(Slot)) { if(!Button) { Count(R,TEXT("slot_position_set_calls"));Slot->SetPosition(G.Position); }if(G.HasPosition) { Count(R,TEXT("slot_position_set_calls"));Slot->SetPosition(G.Position); }if(G.HasSize) { Count(R,TEXT("slot_size_set_calls"));Slot->SetSize(G.Size); } }
    if(!Button) { Added->bIsVariable=true;Count(R,TEXT("is_variable_assignments")); }Count(R,TEXT("on_variable_added_calls"));BP->OnVariableAdded(Added->GetFName());
    Finish(BP,Package,R,Before,Added,Label,Root,&G,Text,Button);return R;
}
}
FResult Create(const FString& WidgetName,const FString& ParentClass,const FString& Path)
{
    FResult R;Init(R,TEXT("create_umg_widget_blueprint"));if(!Admit(R,{WidgetName,ParentClass,Path})) { return R; }FActiveScope Scope;Count(R,TEXT("transaction_calls"));FScopedTransaction Tx(FText::FromString(TEXT("MCP create_umg_widget_blueprint")));
    const FString Name=WidgetName.TrimStartAndEnd();if(!NameValid(Name,false,R)) { return R; }const FString Directory=NormalizeContentDirectory(Path);FString Error;if(!ValidateContentDirectory(Directory,Error)) { Fail(R,TEXT("umg_content_directory_invalid"),Error);return R; }const FString Package=Directory+TEXT("/")+Name;if(!GamePackage(Package,R)) { return R; }
    R.Stage=TEXT("target_preflight");Count(R,TEXT("asset_exists_calls"));const bool Exists=UEditorAssetLibrary::DoesAssetExist(Package);bool ObjectExists=false;if(!Exists) { Count(R,TEXT("asset_exists_calls"));ObjectExists=UEditorAssetLibrary::DoesAssetExist(WidgetObjectPathFromPackage(Package)); }
    if(Exists||ObjectExists||FindObject<UPackage>(nullptr,*Package)||FindObject<UObject>(nullptr,*WidgetObjectPathFromPackage(Package))) { Fail(R,TEXT("umg_target_exists"),FString::Printf(TEXT("Widget Blueprint already exists: %s"),*Package));return R; }
    R.Stage=TEXT("resolve_parent");UClass* AssetClass=nullptr;bool Utility=false;UClass* ParentClassObject=Parent(ParentClass,R,AssetClass,Utility);if(!ParentClassObject) { return R; }TStrongObjectPtr<UClass> KeepParent(ParentClassObject),KeepAssetClass(AssetClass);
    R.Stage=TEXT("create_package");R.bMutationAttempted=true;Count(R,TEXT("package_create_calls"));UPackage* P=CreatePackage(*Package);Count(R,TEXT("package_create_returns"));PublishWidget(R,TEXT("package_return"),P);if(!Loaded(P)) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_package_creation_failed"),TEXT("Package creation did not return a loaded object"));return R; }R.bChanged=true;TStrongObjectPtr<UPackage> KeepPackage(P);
    R.Stage=TEXT("create_blueprint");Count(R,TEXT("blueprint_create_calls"));UBlueprint* NewBP=FKismetEditorUtilities::CreateBlueprint(ParentClassObject,P,FName(*Name),BPTYPE_Normal,AssetClass,UWidgetBlueprintGeneratedClass::StaticClass(),FName(TEXT("MCPCreateWidgetBlueprint")));Count(R,TEXT("blueprint_create_returns"));PublishWidget(R,TEXT("blueprint_return"),NewBP);UWidgetBlueprint* BP=Cast<UWidgetBlueprint>(NewBP);if(!Loaded(BP)||!Loaded(BP->WidgetTree)||BP->WidgetTree->GetOuter()!=BP) { R.bChangeStateKnown=false;Fail(R,TEXT("umg_blueprint_creation_failed"),TEXT("Widget Blueprint or owned source tree unavailable after creation; actual prefix retained without rollback"));return R; }TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());
    if(!Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)&&!FreeName(BP,BP->WidgetTree,TEXT("RootCanvas"),R)) { return R; }R.Stage=TEXT("create_canvas");UCanvasPanel* Root=Canvas(BP,R);if(!Root) { return R; }TStrongObjectPtr<UCanvasPanel> KeepRoot(Root);
    Count(R,TEXT("package_dirty_calls"));P->MarkPackageDirty();Count(R,TEXT("asset_created_calls"));FAssetRegistryModule::AssetCreated(BP);R.Data->SetBoolField(TEXT("asset_created_returned"),true);
    Finish(BP,Package,R,nullptr,nullptr,nullptr,Root,nullptr,FString(),false);return R;
}
FResult AddText(const FString& WidgetName,const FString& TextBlockName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson)
{
    return Add(false,WidgetName,TextBlockName,Text,PositionJson,SizeJson,FontSize,ColorJson,FString());
}
FResult AddButton(const FString& WidgetName,const FString& ButtonName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson,const FString& BackgroundColorJson)
{
    return Add(true,WidgetName,ButtonName,Text,PositionJson,SizeJson,FontSize,ColorJson,BackgroundColorJson);
}
}
