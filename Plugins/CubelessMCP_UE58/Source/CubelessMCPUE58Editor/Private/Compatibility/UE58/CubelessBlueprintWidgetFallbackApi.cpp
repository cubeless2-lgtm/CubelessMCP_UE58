#include "Compatibility/UE58/CubelessBlueprintWidgetFallbackApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlotInterface.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "WidgetBlueprint.h"
namespace Cubeless::UE58::BlueprintWidgetFallback
{
namespace
{
using Variable::FResult;
constexpr int32 MaxAssets=2048,MaxGraphs=4096,MaxNodes=131072,MaxWidgets=4096,MaxDepth=128,MaxReportBytes=1024*1024;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed); }
bool Text(const FString& S,int32 Limit) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(FResult& R,const TCHAR* Key,int32 N=1) { auto C=R.Data->GetObjectField(TEXT("native_call_counts"));C->SetNumberField(Key,C->GetNumberField(Key)+N); }
void Init(FResult& R)
{
    auto C=MakeShared<FJsonObject>();
    for(const TCHAR* K:{TEXT("registry_module_load_calls"),TEXT("registry_module_load_returns"),TEXT("registry_scan_calls"),TEXT("registry_scan_returns"),TEXT("registry_enumeration_calls"),TEXT("registry_enumeration_returns"),TEXT("source_assets_visited"),TEXT("source_asset_load_calls"),TEXT("source_asset_load_returns"),TEXT("target_asset_load_calls"),TEXT("target_asset_load_returns"),TEXT("target_registry_lookup_calls"),TEXT("target_registry_lookup_returns"),TEXT("summary_calls"),TEXT("graph_preflight_visits"),TEXT("get_all_graphs_calls"),TEXT("get_all_graphs_returns"),TEXT("graphs_visited"),TEXT("nodes_visited"),TEXT("get_all_scs_nodes_calls"),TEXT("scs_nodes_visited"),TEXT("widget_preflight_visits"),TEXT("named_slot_name_calls"),TEXT("named_slot_content_calls"),TEXT("panel_slot_get_calls"),TEXT("panel_child_get_calls"),TEXT("get_all_widgets_calls"),TEXT("get_all_widgets_returns"),TEXT("widgets_visited"),TEXT("path_reparse_checks"),TEXT("report_directory_create_calls"),TEXT("report_directory_create_returns"),TEXT("report_file_write_calls"),TEXT("report_file_write_returns"),TEXT("report_file_read_calls"),TEXT("report_file_read_returns"),TEXT("asset_modify_calls"),TEXT("asset_compile_calls"),TEXT("asset_save_calls"),TEXT("package_dirty_mark_calls")}) { C->SetNumberField(K,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),C);R.Data->SetStringField(TEXT("operation"),TEXT("analyze_blueprint_widget_fallbacks_mcp"));
    R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetArrayField(TEXT("observation_errors"),TArray<TSharedPtr<FJsonValue>>{});
    R.Data->SetBoolField(TEXT("read_only"),false);R.Data->SetBoolField(TEXT("direct_asset_write_attempted"),false);R.Data->SetBoolField(TEXT("total_scan_load_cache_effects_verified"),false);
    R.Data->SetStringField(TEXT("change_scope"),TEXT("Direct report directory/file publication; scan/load/cache and package PostLoad effects are separately qualified, not zero effects."));
    R.Data->SetBoolField(TEXT("runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("cold_reload_verified"),false);R.Data->SetBoolField(TEXT("full_structure_equivalence_verified"),false);
    R.Data->SetBoolField(TEXT("report_write_attempted"),false);R.Data->SetBoolField(TEXT("report_write_returned"),false);R.Data->SetBoolField(TEXT("report_readback_verified"),false);
}
void Incomplete(FResult& R,const FString& Reason)
{
    R.Data->SetBoolField(TEXT("observation_incomplete"),true);const auto& Old=R.Data->GetArrayField(TEXT("observation_errors"));auto Rows=Old;
    if(Rows.Num()<50) { Rows.Add(MakeShared<FJsonValueString>(Reason));R.Data->SetArrayField(TEXT("observation_errors"),Rows); }
}
void Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=false;R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error; }
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Rows) { TArray<TSharedPtr<FJsonValue>> Out;for(const auto& S:Rows) { Out.Add(MakeShared<FJsonValueString>(S)); }return Out; }
FString NormalizeContentRoot(FString Path)
{
    Path.TrimStartAndEndInline();
    while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
    {
        Path.LeftChopInline(1);
    }
    return Path;
}

FString NormalizePackagePathParam(FString Path)
{
    Path.TrimStartAndEndInline();
    Path.TrimQuotesInline();

    FString LongPackageName;
    if (FPackageName::TryConvertFilenameToLongPackageName(Path, LongPackageName))
    {
        return NormalizeContentRoot(LongPackageName);
    }

    if (Path.Contains(TEXT(".")))
    {
        Path = FPackageName::ObjectPathToPackageName(Path);
    }
    return NormalizeContentRoot(Path);
}

bool TryGetRequiredStringParamForMCP(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    FString& OutValue,
    FString& OutError)
{
    FString Value;
    if (!Params.IsValid() || !Params->TryGetStringField(FieldName, Value))
    {
        OutError = FString::Printf(TEXT("Missing required parameter: %s"), FieldName);
        return false;
    }

    Value.TrimStartAndEndInline();
    if (Value.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Missing required parameter: %s"), FieldName);
        return false;
    }

    OutValue = Value;
    return true;
}

bool TryGetRequiredContentRootParamForMCP(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    FString& OutRoot,
    FString& OutError)
{
    FString RawValue;
    if (!TryGetRequiredStringParamForMCP(Params, FieldName, RawValue, OutError))
    {
        return false;
    }

    OutRoot = NormalizeContentRoot(RawValue);
    if (!FPackageName::IsValidLongPackageName(OutRoot) || !OutRoot.StartsWith(TEXT("/Game/")))
    {
        OutError = FString::Printf(TEXT("Invalid %s: %s"), FieldName, *OutRoot);
        return false;
    }

    return true;
}

bool ValidateContentRootPairForMCP(const FString& SourceRoot, const FString& TargetRoot, FString& OutError)
{
    if (TargetRoot.Equals(SourceRoot) || TargetRoot.StartsWith(SourceRoot + TEXT("/")))
    {
        OutError = TEXT("target_root must not be the source root or a child of source_root");
        return false;
    }
    return true;
}

bool IsBlueprintWidgetFallbackClassName(const FString& ClassName)
{
    return ClassName == TEXT("Blueprint") ||
        ClassName == TEXT("WidgetBlueprint") ||
        ClassName == TEXT("EditorUtilityBlueprint") ||
        ClassName == TEXT("EditorUtilityWidgetBlueprint");
}

TSharedPtr<FJsonObject> StringIntMapToJsonObject(const TMap<FString, int32>& Map)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    TArray<FString> Keys;
    Map.GetKeys(Keys);
    Keys.Sort();
    for (const FString& Key : Keys)
    {
        Object->SetNumberField(Key, Map.FindRef(Key));
    }
    return Object;
}

FString BuildTargetAssetPath(const FAssetData& SourceAsset, const FString& SourceRoot, const FString& TargetRoot, const FString& Suffix)
{
    const FString SourceDirectory = SourceAsset.PackagePath.ToString();
    FString RelativeDirectory;
    if (SourceDirectory.Equals(SourceRoot))
    {
        RelativeDirectory = FString();
    }
    else if (SourceDirectory.StartsWith(SourceRoot + TEXT("/")))
    {
        RelativeDirectory = SourceDirectory.RightChop(SourceRoot.Len());
    }
    else
    {
        RelativeDirectory = TEXT("/") + FPackageName::GetShortName(SourceDirectory);
    }

    FString TargetName = SourceAsset.AssetName.ToString();
    if (!TargetName.EndsWith(Suffix))
    {
        TargetName += Suffix;
    }

    return TargetRoot + RelativeDirectory + TEXT("/") + TargetName;
}

FString MakeSafeReportFilenameStemForMCP(FString Stem)
{
    Stem.TrimStartAndEndInline();
    if (Stem.IsEmpty())
    {
        Stem = TEXT("content");
    }

    for (int32 CharacterIndex = 0; CharacterIndex < Stem.Len(); ++CharacterIndex)
    {
        TCHAR& Character = Stem[CharacterIndex];
        if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-') && Character != TEXT('.'))
        {
            Character = TEXT('_');
        }
    }

    return Stem;
}

FString BuildMCPReportFilename(const FString& PackagePathOrRoot, const FString& OperationName)
{
    FString Stem = FPackageName::GetShortName(PackagePathOrRoot);
    if (Stem.IsEmpty())
    {
        Stem = FPackageName::GetShortName(NormalizePackagePathParam(PackagePathOrRoot));
    }
    Stem = MakeSafeReportFilenameStemForMCP(Stem);
    return Stem + TEXT("_") + MakeSafeReportFilenameStemForMCP(OperationName) + TEXT("_report.json");
}

int32 GetJsonIntField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    return Object.IsValid() && Object->HasTypedField<EJson::Number>(FieldName)
        ? static_cast<int32>(Object->GetNumberField(FieldName))
        : 0;
}

bool CompareBlueprintStructureSummaries(
    const TSharedPtr<FJsonObject>& SourceSummary,
    const TSharedPtr<FJsonObject>& TargetSummary,
    TArray<FString>& OutMismatchFields)
{
    static const TArray<FString> CountFields = {
        TEXT("graph_count"),
        TEXT("non_empty_graph_count"),
        TEXT("total_node_count"),
        TEXT("new_variable_count"),
        TEXT("generated_variable_count"),
        TEXT("scs_node_count"),
        TEXT("widget_count")
    };

    for (const FString& FieldName : CountFields)
    {
        if (GetJsonIntField(SourceSummary, FieldName) != GetJsonIntField(TargetSummary, FieldName))
        {
            OutMismatchFields.Add(FieldName);
        }
    }

    return OutMismatchFields.IsEmpty();
}

bool PackagePath(const FString& S)
{
    if(!Text(S,NAME_SIZE-1)||!S.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(S)) { return false; }
    const FString ObjectPath=S+TEXT(".")+FPackageName::GetShortName(S);return Text(ObjectPath,NAME_SIZE-1);
}
FString ObjectPathOrEmpty(UObject* O,FResult& R,bool& Complete)
{
    if(!O) { return FString(); }if(!Loaded(O)) { Complete=false;Incomplete(R,TEXT("Non-null Blueprint class reference is not currently loaded/usable"));return FString(); }return O->GetPathName();
}
bool GraphSafe(UEdGraph* G,TSet<UEdGraph*>& Active,int32 Depth,int32& Visits,FResult& R)
{
    Count(R,TEXT("graph_preflight_visits"));if(!Loaded(G)||Depth>MaxDepth||++Visits>MaxGraphs||Active.Contains(G)||G->SubGraphs.Num()>MaxGraphs) { return false; }
    Active.Add(G);for(UEdGraph* Child:G->SubGraphs) { if(!Child||!GraphSafe(Child,Active,Depth+1,Visits,R)) { Active.Remove(G);return false; } }Active.Remove(G);return true;
}
bool BlueprintGraphsSafe(UBlueprint* B,FResult& R)
{
    TSet<UEdGraph*> Active;int32 Visits=0;bool OK=true;
    auto Check=[&](const auto& Rows) { if(Rows.Num()>MaxGraphs) { OK=false;return; }for(UEdGraph* G:Rows) { if(G&&!GraphSafe(G,Active,0,Visits,R)) { OK=false;return; } } };
    Check(B->FunctionGraphs);if(!OK) { return false; }Check(B->MacroGraphs);if(!OK) { return false; }Check(B->UbergraphPages);if(!OK) { return false; }Check(B->DelegateSignatureGraphs);if(!OK||B->ImplementedInterfaces.Num()>MaxGraphs) { return false; }
    for(const auto& I:B->ImplementedInterfaces) { Check(I.Graphs);if(!OK) { return false; } }return true;
}
bool WidgetSafe(UWidget* W,TSet<UWidget*>& Active,int32 Depth,int32& Visits,FResult& R)
{
    Count(R,TEXT("widget_preflight_visits"));if(!Loaded(W)||!Loaded(W->GetClass())||Depth>MaxDepth||++Visits>MaxWidgets||Active.Contains(W)) { return false; }Active.Add(W);
    if(INamedSlotInterface* Host=Cast<INamedSlotInterface>(W))
    {
        TArray<FName> Names;Count(R,TEXT("named_slot_name_calls"));Host->GetSlotNames(Names);if(Names.Num()>MaxWidgets) { Active.Remove(W);return false; }
        for(FName Name:Names) { Count(R,TEXT("named_slot_content_calls"));UWidget* Child=Host->GetContentForSlot(Name);if(Child&&!WidgetSafe(Child,Active,Depth+1,Visits,R)) { Active.Remove(W);return false; } }
    }
    if(UPanelWidget* P=Cast<UPanelWidget>(W))
    {
        Count(R,TEXT("panel_slot_get_calls"));const auto& Slots=P->GetSlots();if(Slots.Num()>MaxWidgets) { Active.Remove(W);return false; }
        for(UPanelSlot* Slot:Slots) { if(!Loaded(Slot)||Slot->Parent!=P||(Slot->Content&&!Loaded(Slot->Content))) { Active.Remove(W);return false; } }
        for(int32 I=0;I<Slots.Num();++I) { Count(R,TEXT("panel_child_get_calls"));UWidget* Child=P->GetChildAt(I);if(Child&&!WidgetSafe(Child,Active,Depth+1,Visits,R)) { Active.Remove(W);return false; } }
    }
    Active.Remove(W);return true;
}
bool WidgetTreeSafe(UWidgetTree* Tree,FResult& R)
{
    if(!Loaded(Tree)) { return false; }TSet<UWidget*> Active;int32 Visits=0;if(Tree->RootWidget&&!WidgetSafe(Tree->RootWidget,Active,0,Visits,R)) { return false; }
    TArray<FName> Names;Count(R,TEXT("named_slot_name_calls"));Tree->GetSlotNames(Names);if(Names.Num()>MaxWidgets) { return false; }
    for(FName Name:Names) { Count(R,TEXT("named_slot_content_calls"));UWidget* Content=Tree->GetContentForSlot(Name);if(Content&&!WidgetSafe(Content,Active,0,Visits,R)) { return false; } }return true;
}
bool MapCount(TMap<FString,int32>& Map,const FString& Name,int32& Budget,FResult& R)
{
    if(!Map.Contains(Name)) { const int32 Bytes=FTCHARToUTF8(*Name).Length()+32;if(Budget>512*1024-Bytes) { Incomplete(R,TEXT("Summary string budget reached before adding another class key"));return false; }Budget+=Bytes; }Map.FindOrAdd(Name)++;return true;
}
TSharedRef<FJsonObject> Summary(UBlueprint* B,FResult& R)
{
    Count(R,TEXT("summary_calls"));auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("loaded"),B!=nullptr);J->SetBoolField(TEXT("summary_complete"),false);
    if(!B) { return J; }if(!Loaded(B)||!Loaded(B->GetClass())) { Incomplete(R,TEXT("Blueprint summary source is non-null but unavailable"));return J; }
    J->SetStringField(TEXT("path"),B->GetPathName());J->SetStringField(TEXT("class"),B->GetClass()->GetName());J->SetBoolField(TEXT("package_dirty_at_summary"),B->GetOutermost()->IsDirty());
    bool Complete=true;int32 StringBudget=0;
    J->SetStringField(TEXT("parent_class"),ObjectPathOrEmpty(B->ParentClass.Get(),R,Complete));J->SetStringField(TEXT("generated_class"),ObjectPathOrEmpty(B->GeneratedClass.Get(),R,Complete));J->SetStringField(TEXT("skeleton_generated_class"),ObjectPathOrEmpty(B->SkeletonGeneratedClass.Get(),R,Complete));
    J->SetNumberField(TEXT("new_variable_count"),B->NewVariables.Num());J->SetNumberField(TEXT("generated_variable_count"),B->GeneratedVariables.Num());
    TArray<UEdGraph*> Graphs;TArray<TSharedPtr<FJsonValue>> GraphValues;TMap<FString,int32> NodeClasses;int32 Nodes=0,NonEmpty=0;
    if(!BlueprintGraphsSafe(B,R)) { Complete=false;Incomplete(R,TEXT("Graph recursion/loaded pointer/depth/count preflight incomplete; GetAllGraphs not called")); }
    else
    {
        Count(R,TEXT("get_all_graphs_calls"));B->GetAllGraphs(Graphs);Count(R,TEXT("get_all_graphs_returns"));J->SetNumberField(TEXT("graph_count"),Graphs.Num());
        for(UEdGraph* G:Graphs)
        {
            Count(R,TEXT("graphs_visited"));if(!Loaded(G)||!Loaded(G->GetClass())||G->Nodes.Num()>MaxNodes||Nodes>MaxNodes-G->Nodes.Num()) { Complete=false;Incomplete(R,TEXT("Graph/node summary bound or readiness incomplete"));break; }
            const int32 RowBudget=FTCHARToUTF8(*G->GetName()).Length()+FTCHARToUTF8(*G->GetClass()->GetName()).Length()+128;if(StringBudget>512*1024-RowBudget) { Complete=false;Incomplete(R,TEXT("Graph row string budget reached; actual prior rows retained"));break; }StringBudget+=RowBudget;
            auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),G->GetName());Q->SetStringField(TEXT("class"),G->GetClass()->GetName());Q->SetNumberField(TEXT("node_count"),G->Nodes.Num());GraphValues.Add(MakeShared<FJsonValueObject>(Q));Nodes+=G->Nodes.Num();NonEmpty+=G->Nodes.Num()>0;
            for(UEdGraphNode* N:G->Nodes) { Count(R,TEXT("nodes_visited"));if(!N) { continue; }if(!Loaded(N)||!Loaded(N->GetClass())) { Complete=false;Incomplete(R,TEXT("Non-null graph node/class unavailable; prefix retained"));continue; }Complete=MapCount(NodeClasses,N->GetClass()->GetName(),StringBudget,R)&&Complete; }
        }
    }
    J->SetNumberField(TEXT("non_empty_graph_count"),NonEmpty);J->SetNumberField(TEXT("total_node_count"),Nodes);J->SetObjectField(TEXT("node_class_counts"),StringIntMapToJsonObject(NodeClasses));J->SetArrayField(TEXT("graphs"),GraphValues);
    int32 SCSCount=0;TMap<FString,int32> SCSClasses;
    if(B->SimpleConstructionScript)
    {
        USimpleConstructionScript* SCS=B->SimpleConstructionScript;if(!Loaded(SCS)) { Complete=false;Incomplete(R,TEXT("Non-null SCS unavailable")); }
        else
        {
            Count(R,TEXT("get_all_scs_nodes_calls"));const auto& Rows=SCS->GetAllNodes();SCSCount=Rows.Num();
            if(Rows.Num()>MaxNodes) { Complete=false;Incomplete(R,TEXT("SCS summary count exceeds bound")); }
            else { for(USCS_Node* N:Rows) { Count(R,TEXT("scs_nodes_visited"));if(!N) { continue; }if(!Loaded(N)||(N->ComponentClass&&!Loaded(N->ComponentClass))) { Complete=false;Incomplete(R,TEXT("SCS node/class unavailable"));continue; }if(N->ComponentClass) { Complete=MapCount(SCSClasses,N->ComponentClass->GetName(),StringBudget,R)&&Complete; } } }
        }
    }
    J->SetNumberField(TEXT("scs_node_count"),SCSCount);J->SetObjectField(TEXT("scs_component_class_counts"),StringIntMapToJsonObject(SCSClasses));
    int32 WidgetCount=0;FString RootClass;TMap<FString,int32> WidgetClasses;
    if(UWidgetBlueprint* WB=Cast<UWidgetBlueprint>(B))
    {
        if(WB->WidgetTree)
        {
            UWidgetTree* Tree=WB->WidgetTree;if(!WidgetTreeSafe(Tree,R)) { Complete=false;Incomplete(R,TEXT("Widget tree traversal/cycle/readiness/count preflight incomplete; GetAllWidgets not called")); }
            else
            {
                TArray<UWidget*> Widgets;Count(R,TEXT("get_all_widgets_calls"));Tree->GetAllWidgets(Widgets);Count(R,TEXT("get_all_widgets_returns"));WidgetCount=Widgets.Num();
                if(Tree->RootWidget&&Loaded(Tree->RootWidget->GetClass())) { RootClass=Tree->RootWidget->GetClass()->GetName(); }
                for(UWidget* W:Widgets) { Count(R,TEXT("widgets_visited"));if(!Loaded(W)||!Loaded(W->GetClass())) { Complete=false;Incomplete(R,TEXT("Widget/class unavailable after public enumeration"));continue; }Complete=MapCount(WidgetClasses,W->GetClass()->GetName(),StringBudget,R)&&Complete; }
            }
        }
    }
    J->SetNumberField(TEXT("widget_count"),WidgetCount);J->SetStringField(TEXT("root_widget_class"),RootClass);J->SetObjectField(TEXT("widget_class_counts"),StringIntMapToJsonObject(WidgetClasses));
    const bool HasWork=Nodes>0||SCSCount>0||WidgetCount>0||B->NewVariables.Num()>0;J->SetBoolField(TEXT("data_only_like"),!HasWork);J->SetStringField(TEXT("fresh_recreation_risk"),HasWork?TEXT("high_serialized_graph_or_widget_state"):TEXT("low_data_only_like"));J->SetBoolField(TEXT("summary_complete"),Complete);return J;
}
bool NoReparse(const FString& Path,FResult& R)
{
    FString Probe=Path;for(int32 I=0;I<128;++I)
    {
        Count(R,TEXT("path_reparse_checks"));const auto Kind=FPlatformFileManager::Get().GetPlatformFile().IsSymlink(*Probe);if(Kind!=ESymlinkResult::NonSymlink) { return false; }
        const FString Parent=FPaths::GetPath(Probe);if(Parent.IsEmpty()||Parent.Equals(Probe)) { return true; }Probe=Parent;
    }return false;
}
bool ReportPath(FResult& R,FString& Directory,FString& Path)
{
    const FString ProjectFile=FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Project);
    R.Data->SetStringField(TEXT("project_file"),ProjectFile);R.Data->SetStringField(TEXT("project_directory"),Project);
    if(!Text(ProjectFile,4096)||!Text(Project,4096)||!ProjectFile.EndsWith(TEXT(".uproject"))||!IFileManager::Get().FileExists(*ProjectFile)||!FPaths::GetPath(ProjectFile).Equals(Project)||!NoReparse(Project,R)) { Fail(R,TEXT("fallback_report_project_anchor_invalid"),TEXT("Current .uproject and non-reparse project directory must match before scans/publication"));return false; }
    Directory=FPaths::Combine(Project,TEXT("cubeless/records/verification/blueprint-widget-fallbacks"));Path=FPaths::Combine(Directory,FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT("_report.json"));
    R.Data->SetStringField(TEXT("report_path"),Path);R.Data->SetStringField(TEXT("report_directory"),Directory);
    if(!NoReparse(Directory,R)||IFileManager::Get().FileExists(*Directory)||IFileManager::Get().FileExists(*Path)) { Fail(R,TEXT("fallback_report_destination_invalid"),TEXT("Operation-specific report destination cannot be a reparse/file/collision"));return false; }return true;
}
bool Publish(FResult& R,const FString& Directory,const FString& Path,const TSharedRef<FJsonObject>& Report)
{
    R.Stage=TEXT("prepare_diagnostic_report");FString Encoded;const bool Serialized=FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Encoded));
    const FTCHARToUTF8 UTF8(*Encoded);R.Data->SetNumberField(TEXT("report_utf8_bytes_without_bom"),UTF8.Length());if(!Serialized||UTF8.Length()>MaxReportBytes-3) { Fail(R,TEXT("fallback_report_serialization_bound"),TEXT("Complete diagnostic serialization must fit the 1 MiB report bound"));return false; }
    if(!NoReparse(Directory,R)||IFileManager::Get().FileExists(*Path)) { Fail(R,TEXT("fallback_report_prewrite_destination_changed"),TEXT("Reparse/collision preflight changed before report publication; no write/retry"));return false; }
    const bool Before=IFileManager::Get().DirectoryExists(*Directory);R.Stage=TEXT("create_report_directory");R.bMutationAttempted=true;R.bChangeStateKnown=false;
    Count(R,TEXT("report_directory_create_calls"));const bool Made=IFileManager::Get().MakeDirectory(*Directory,true);Count(R,TEXT("report_directory_create_returns"));const bool Exists=IFileManager::Get().DirectoryExists(*Directory);
    R.Data->SetBoolField(TEXT("report_directory_existed_before"),Before);R.Data->SetBoolField(TEXT("report_directory_create_returned"),Made);R.Data->SetBoolField(TEXT("report_directory_exists_after"),Exists);R.bChanged=!Before&&Exists;
    if(!Made||!Exists||!NoReparse(Directory,R)) { Fail(R,TEXT("fallback_report_directory_failed"),TEXT("Directory creation returned failure or destination changed; actual prefix retained"));return false; }
    R.Stage=TEXT("write_diagnostic_report");R.Data->SetBoolField(TEXT("report_write_attempted"),true);Count(R,TEXT("report_file_write_calls"));const bool Wrote=FFileHelper::SaveStringToFile(Encoded,*Path,FFileHelper::EEncodingOptions::ForceUTF8,&IFileManager::Get(),FILEWRITE_NoReplaceExisting);Count(R,TEXT("report_file_write_returns"));
    R.Data->SetBoolField(TEXT("report_write_returned"),Wrote);const bool FileExists=IFileManager::Get().FileExists(*Path);R.Data->SetBoolField(TEXT("report_file_exists_after"),FileExists);R.Data->SetNumberField(TEXT("report_file_bytes_after"),IFileManager::Get().FileSize(*Path));R.bChanged=R.bChanged||FileExists;
    if(!Wrote) { Fail(R,TEXT("fallback_report_write_failed"),TEXT("Exclusive UTF8 report write returned false; possible partial file retained, no replacement/cleanup/retry"));return false; }
    R.Stage=TEXT("readback_diagnostic_report");FString Readback;Count(R,TEXT("report_file_read_calls"));const bool Read=FFileHelper::LoadFileToString(Readback,*Path);Count(R,TEXT("report_file_read_returns"));const bool Verified=Read&&Readback.Equals(Encoded,ESearchCase::CaseSensitive);R.Data->SetBoolField(TEXT("report_read_returned"),Read);R.Data->SetBoolField(TEXT("report_readback_verified"),Verified);
    if(!Verified) { Fail(R,TEXT("fallback_report_readback_failed"),TEXT("Written diagnostic readback is unavailable/different; real publication prefix retained, no retry"));return false; }
    R.bChanged=true;R.bChangeStateKnown=true;return true;
}
}
Variable::FResult Analyze(const FString& SourceRootInput,const FString& TargetRootInput,const FString& Suffix)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_source_root"),SourceRootInput);R.Data->SetStringField(TEXT("requested_target_root"),TargetRootInput);R.Data->SetStringField(TEXT("suffix"),Suffix);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("fallback_editor_idle_required"),TEXT("Editor game-thread/idle scope required"));return R; }
    if(!Text(SourceRootInput,NAME_SIZE-1)||!Text(TargetRootInput,NAME_SIZE-1)||!Text(Suffix,NAME_SIZE-1)) { Fail(R,TEXT("fallback_input_text_bound"),TEXT("Bounded input strings without interior NUL required before FName/load"));return R; }
    auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("source_root"),SourceRootInput);Params->SetStringField(TEXT("target_root"),TargetRootInput);FString SourceRoot,TargetRoot,Error;
    if(!TryGetRequiredContentRootParamForMCP(Params,TEXT("source_root"),SourceRoot,Error)||!TryGetRequiredContentRootParamForMCP(Params,TEXT("target_root"),TargetRoot,Error)||!ValidateContentRootPairForMCP(SourceRoot,TargetRoot,Error)) { Fail(R,TEXT("fallback_root_invalid"),Error);return R; }
    R.Data->SetStringField(TEXT("source_root"),SourceRoot);R.Data->SetStringField(TEXT("target_root"),TargetRoot);const FString LegacyName=BuildMCPReportFilename(TargetRoot,TEXT("blueprint_widget_diagnostic"));R.Data->SetStringField(TEXT("legacy_report_basename"),LegacyName);R.Data->SetStringField(TEXT("legacy_suggested_report_path"),FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("MCP"),LegacyName));
    FString Directory,Path;if(!ReportPath(R,Directory,Path)) { return R; }
    R.Stage=TEXT("load_asset_registry");Count(R,TEXT("registry_module_load_calls"));FAssetRegistryModule* Module=FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));Count(R,TEXT("registry_module_load_returns"));if(!Module) { Fail(R,TEXT("fallback_registry_unavailable"),TEXT("AssetRegistry public module load returned null"));return R; }IAssetRegistry& Registry=Module->Get();
    R.Stage=TEXT("scan_roots");Count(R,TEXT("registry_scan_calls"));Registry.ScanPathsSynchronous({SourceRoot,TargetRoot},true);Count(R,TEXT("registry_scan_returns"));R.Data->SetBoolField(TEXT("forced_scan_returned"),true);
    R.Stage=TEXT("enumerate_sources");TArray<FAssetData> Assets;Count(R,TEXT("registry_enumeration_calls"));const bool Enumerated=Registry.GetAssetsByPath(FName(*SourceRoot),Assets,true);Count(R,TEXT("registry_enumeration_returns"));R.Data->SetBoolField(TEXT("registry_enumeration_returned"),Enumerated);R.Data->SetNumberField(TEXT("source_registry_asset_count"),Assets.Num());// The original ignores this boolean; a valid empty root remains a complete empty diagnostic.
    if(Assets.Num()>MaxAssets) { Fail(R,TEXT("fallback_asset_count_bound"),TEXT("Source registry rows exceed 2048; scan prefix retained without partial loads/report"));return R; }
    Assets.Sort([](const FAssetData& A,const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
    int32 SourceCount=0,Paired=0,Missing=0,Mismatches=0,DataOnly=0,HighRisk=0,TotalGraphs=0,TotalNodes=0,TotalWidgets=0;int64 TotalSCS=0;TMap<FString,int32> ClassCounts,RiskClasses;TArray<FString> MissingSamples,MismatchSamples;TArray<TSharedPtr<FJsonValue>> Rows;int32 PairBytes=0;
    R.Data->SetArrayField(TEXT("asset_prefix"),Rows);R.Stage=TEXT("analyze_pairs");
    for(const FAssetData& A:Assets)
    {
        Count(R,TEXT("source_assets_visited"));const FString ClassName=A.AssetClassPath.GetAssetName().ToString();if(!IsBlueprintWidgetFallbackClassName(ClassName)) { continue; }
        ++SourceCount;ClassCounts.FindOrAdd(ClassName)++;const FString SourcePath=A.PackageName.ToString(),TargetPath=BuildTargetAssetPath(A,SourceRoot,TargetRoot,Suffix);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("source_path"),SourcePath);Row->SetStringField(TEXT("target_path"),TargetPath);Row->SetStringField(TEXT("class"),ClassName);Rows.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("asset_prefix"),Rows);
        if(!PackagePath(SourcePath)||!PackagePath(TargetPath)||!Text(A.GetObjectPathString(),NAME_SIZE-1)) { Incomplete(R,TEXT("Pair path/object path exceeds safe FName/package admission; no pair load"));Row->SetBoolField(TEXT("pair_path_admitted"),false);continue; }Row->SetBoolField(TEXT("pair_path_admitted"),true);
        Count(R,TEXT("source_asset_load_calls"));UObject* SourceObject=A.GetAsset();Count(R,TEXT("source_asset_load_returns"));TStrongObjectPtr<UObject> HoldSource(SourceObject);Row->SetBoolField(TEXT("source_load_returned_nonnull"),SourceObject!=nullptr);
        Count(R,TEXT("target_asset_load_calls"));UObject* TargetObject=UEditorAssetLibrary::LoadAsset(TargetPath);Count(R,TEXT("target_asset_load_returns"));TStrongObjectPtr<UObject> HoldTarget(TargetObject);Row->SetBoolField(TEXT("target_exists"),TargetObject!=nullptr);
        UBlueprint* SourceBP=Cast<UBlueprint>(SourceObject);UBlueprint* TargetBP=Cast<UBlueprint>(TargetObject);if(!Loaded(SourceBP)) { Incomplete(R,TEXT("Source selected Blueprint did not load as a usable Blueprint")); }
        const FString TargetObjectPath=TargetPath+TEXT(".")+FPackageName::GetShortName(TargetPath);Count(R,TEXT("target_registry_lookup_calls"));const FAssetData TargetData=Registry.GetAssetByObjectPath(FSoftObjectPath(TargetObjectPath));Count(R,TEXT("target_registry_lookup_returns"));Row->SetBoolField(TEXT("target_registry_asset_exists"),TargetData.IsValid());
        Row->SetBoolField(TEXT("target_loaded_wrong_class"),Loaded(TargetObject)&&TargetBP==nullptr);if(TargetData.IsValid()&&!Loaded(TargetObject)) { Incomplete(R,TEXT("Registered target pair object did not load as a usable object")); }if(TargetObject&&!Loaded(TargetObject)) { Incomplete(R,TEXT("Non-null target object is unavailable")); }
        auto SourceSummary=Summary(SourceBP,R),TargetSummary=Summary(TargetBP,R);Row->SetObjectField(TEXT("source"),SourceSummary);Row->SetObjectField(TEXT("target"),TargetSummary);
        TotalGraphs+=GetJsonIntField(SourceSummary,TEXT("graph_count"));TotalNodes+=GetJsonIntField(SourceSummary,TEXT("total_node_count"));TotalSCS+=GetJsonIntField(SourceSummary,TEXT("scs_node_count"));TotalWidgets+=GetJsonIntField(SourceSummary,TEXT("widget_count"));bool Only=false;SourceSummary->TryGetBoolField(TEXT("data_only_like"),Only);
        if(Only) { ++DataOnly;Row->SetStringField(TEXT("fresh_recreation_recommendation"),TEXT("candidate_after_explicit_single_asset_test")); }else { ++HighRisk;RiskClasses.FindOrAdd(ClassName)++;Row->SetStringField(TEXT("fresh_recreation_recommendation"),TEXT("keep_fallback_duplicate_until_graph_rebuild_support_exists")); }
        if(!TargetBP) { ++Missing;if(MissingSamples.Num()<50) { MissingSamples.Add(TargetPath); } }
        else { ++Paired;TArray<FString> Fields;const bool Match=CompareBlueprintStructureSummaries(SourceSummary,TargetSummary,Fields);Row->SetBoolField(TEXT("structure_match"),Match);Row->SetArrayField(TEXT("structure_mismatch_fields"),Strings(Fields));if(!Match) { ++Mismatches;if(MismatchSamples.Num()<50) { MismatchSamples.Add(FString::Printf(TEXT("%s -> %s (%s)"),*SourcePath,*TargetPath,*FString::Join(Fields,TEXT(",")))); } } }
        FString PairJson;FJsonSerializer::Serialize(Row,TJsonWriterFactory<>::Create(&PairJson));PairBytes+=FTCHARToUTF8(*PairJson).Length();if(PairBytes>6*1024*1024) { Incomplete(R,TEXT("Paired diagnostic detail budget reached; no later pair loads, actual prefix retained"));break; }
    }
    auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("operation"),TEXT("analyze_blueprint_widget_fallbacks_mcp"));Report->SetStringField(TEXT("source_root"),SourceRoot);Report->SetStringField(TEXT("target_root"),TargetRoot);Report->SetStringField(TEXT("suffix"),Suffix);
    Report->SetNumberField(TEXT("source_asset_count"),SourceCount);Report->SetNumberField(TEXT("paired_asset_count"),Paired);Report->SetNumberField(TEXT("missing_target_asset_count"),Missing);Report->SetNumberField(TEXT("mismatch_pair_count"),Mismatches);Report->SetNumberField(TEXT("data_only_like_count"),DataOnly);Report->SetNumberField(TEXT("high_risk_count"),HighRisk);Report->SetNumberField(TEXT("total_source_graph_count"),TotalGraphs);Report->SetNumberField(TEXT("total_source_node_count"),TotalNodes);Report->SetNumberField(TEXT("total_source_scs_node_count"),TotalSCS);Report->SetNumberField(TEXT("total_source_widget_count"),TotalWidgets);
    Report->SetObjectField(TEXT("class_counts"),StringIntMapToJsonObject(ClassCounts));Report->SetObjectField(TEXT("high_risk_class_counts"),StringIntMapToJsonObject(RiskClasses));Report->SetArrayField(TEXT("missing_target_samples"),Strings(MissingSamples));Report->SetArrayField(TEXT("mismatch_samples"),Strings(MismatchSamples));Report->SetArrayField(TEXT("assets"),Rows);Report->SetBoolField(TEXT("verification_pass"),Missing==0&&Mismatches==0);
    Report->SetStringField(TEXT("fresh_recreation_conclusion"),HighRisk>0?TEXT("Blueprint/Widget fresh recreation should stay disabled for high-risk serialized graph/widget assets until graph, SCS, and WidgetTree rebuild support is implemented."):TEXT("Only data-only-like Blueprint assets were found; fresh recreation can be tested per asset."));Report->SetBoolField(TEXT("observation_incomplete"),R.Data->GetBoolField(TEXT("observation_incomplete")));Report->SetArrayField(TEXT("observation_errors"),R.Data->GetArrayField(TEXT("observation_errors")));Report->SetStringField(TEXT("report_path"),Path);
    R.Data->RemoveField(TEXT("asset_prefix"));R.Data->SetObjectField(TEXT("diagnostic_report"),Report);auto Legacy=MakeShared<FJsonObject>();for(const auto& P:Report->Values) { const FString Key(P.Key.ToView());if(Key!=TEXT("assets")) { Legacy->SetField(Key,P.Value); } }Legacy->SetBoolField(TEXT("success"),true);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    if(!Publish(R,Directory,Path,Report)) { return R; }
    if(R.Data->GetBoolField(TEXT("observation_incomplete"))) { Fail(R,TEXT("fallback_observation_incomplete"),TEXT("Published diagnostic retains incomplete source/target/summary prefix; legacy seven-count predicate is not complete current proof"));return R; }
    R.Stage=TEXT("complete");R.bSuccess=true;R.bPostconditionPassed=true;return R;
}
}
