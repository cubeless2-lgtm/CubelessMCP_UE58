#include "Compatibility/UE58/CubelessLoopApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "Compatibility/UE58/CubelessConnectionApi.h"
#include "Kismet/DataTableFunctionLibrary.h"
#include "Engine/DataTable.h"
#include <type_traits>
#include "EdGraph/EdGraph.h"
#include "Components/SkeletalMeshComponent.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/UnrealType.h"
#include "UObject/MetaData.h"
#include "Editor.h"
#include "Engine/World.h"
#include "UObject/GarbageCollection.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformProcess.h"
#include "K2Node_Tunnel.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeArray.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::Loop
{
namespace
{
constexpr const TCHAR* StockBlueprintPath = TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros");
constexpr const TCHAR* StockPackageName = TEXT("/Engine/EditorBlueprintResources/StandardMacros");
constexpr const TCHAR* MacroNames[] = {TEXT("ForLoop"), TEXT("ForEachLoop")};

bool Fail(Function::FResult& R, const TCHAR* Code, const TCHAR* Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }

bool FullyLoaded(const UObject* Object)
{ return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects); }

FString Text(const TSharedRef<FJsonObject>& Object)
{ FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out; }

FString Path(const UObject* Object)
{ return Object ? Object->GetPathName() : FString(); }

TSharedRef<FJsonObject> PackageState(const UPackage* Package)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetBoolField(TEXT("loaded"), IsValid(Package));
    Row->SetStringField(TEXT("package_name"), Package ? Package->GetName() : FString());
    Row->SetStringField(TEXT("package_flags_string"), Package ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    Row->SetBoolField(TEXT("package_dirty"), Package && Package->IsDirty()); return Row;
}

TSharedRef<FJsonObject> DescribeType(const FEdGraphPinType& InputType,const TFunction<bool()>* Fence=nullptr)
{
    const FEdGraphPinType Snapshot=InputType;const FEdGraphPinType& Type=Fence?Snapshot:InputType;auto Row = MakeShared<FJsonObject>(); FString TypeText;
    FEdGraphPinType::StaticStruct()->ExportText(TypeText, &Type, &Type, nullptr, PPF_None, nullptr);
    if(Fence&&!(*Fence)()){Row->SetBoolField(TEXT("observation_incomplete"),true);return Row;}
    Row->SetStringField(TEXT("pin_type_text"), TypeText);
    Row->SetStringField(TEXT("category"), Type.PinCategory.ToString());
    Row->SetStringField(TEXT("subcategory"), Type.PinSubCategory.ToString());
    Row->SetStringField(TEXT("type_object"), Type.PinSubCategoryObject.IsValid() ? Path(Type.PinSubCategoryObject.Get()) : FString());if(Fence&&!(*Fence)()){Row->SetBoolField(TEXT("observation_incomplete"),true);return Row;}
    Row->SetStringField(TEXT("container"), Type.ContainerType == EPinContainerType::Array ? TEXT("array")
        : Type.ContainerType == EPinContainerType::Set ? TEXT("set") : Type.ContainerType == EPinContainerType::Map ? TEXT("map") : TEXT("none"));
    Row->SetBoolField(TEXT("is_reference"), Type.bIsReference);
    Row->SetBoolField(TEXT("is_const"), Type.bIsConst);
    Row->SetBoolField(TEXT("is_weak_pointer"), Type.bIsWeakPointer);
    Row->SetBoolField(TEXT("is_uobject_wrapper"), Type.bIsUObjectWrapper);
    Row->SetBoolField(TEXT("serialize_as_single_precision_float"), Type.bSerializeAsSinglePrecisionFloat);
    Row->SetStringField(TEXT("terminal_category"), Type.PinValueType.TerminalCategory.ToString());
    Row->SetStringField(TEXT("terminal_subcategory"), Type.PinValueType.TerminalSubCategory.ToString());
    Row->SetStringField(TEXT("terminal_type_object"), Type.PinValueType.TerminalSubCategoryObject.IsValid() ? Path(Type.PinValueType.TerminalSubCategoryObject.Get()) : FString());if(Fence&&!(*Fence)()){Row->SetBoolField(TEXT("observation_incomplete"),true);return Row;}
    Row->SetBoolField(TEXT("terminal_is_const"), Type.PinValueType.bTerminalIsConst);
    Row->SetBoolField(TEXT("terminal_is_weak_pointer"), Type.PinValueType.bTerminalIsWeakPointer);
    Row->SetBoolField(TEXT("terminal_is_uobject_wrapper"), Type.PinValueType.bTerminalIsUObjectWrapper);
    return Row;
}

TSharedRef<FJsonObject> DescribePin(const UEdGraphPin* Pin, int32 Ordinal, bool& Complete,const TFunction<bool()>* Fence=nullptr)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("ordinal"), Ordinal);
    Row->SetBoolField(TEXT("pin_present"), Pin != nullptr);
    if (!Pin) { Complete = false; return Row; }
    Row->SetStringField(TEXT("name"), Pin->PinName.ToString());
    Row->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::DigitsWithHyphens));
    Row->SetStringField(TEXT("persistent_guid"), Pin->PersistentGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Row->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("unknown"));
    Row->SetObjectField(TEXT("type"), DescribeType(Pin->PinType,Fence));if(Fence&&!(*Fence)()){Complete=false;return Row;}
    Row->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    Row->SetStringField(TEXT("autogenerated_default_value"), Pin->AutogeneratedDefaultValue);
    if(Fence){UObject* D=Pin->DefaultObject.Get();if(!(*Fence)()||(D&&!FullyLoaded(D))){Complete=false;return Row;}TStrongObjectPtr<UObject> Held(D);Row->SetStringField(TEXT("default_object_path"),Path(D));}
    else Row->SetStringField(TEXT("default_object_path"), Path(Pin->DefaultObject.Get()));
    if(Fence){const FText Copied=Pin->DefaultTextValue;const FString Display=Copied.ToString();if(!(*Fence)()){Complete=false;return Row;}Row->SetStringField(TEXT("default_text_value"),Display);Row->SetBoolField(TEXT("default_text_empty"),Copied.IsEmpty());}
    else{Row->SetStringField(TEXT("default_text_value"), Pin->DefaultTextValue.ToString());Row->SetBoolField(TEXT("default_text_empty"), Pin->DefaultTextValue.IsEmpty());}
    Row->SetBoolField(TEXT("hidden"), Pin->bHidden);
    Row->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable);
    Row->SetBoolField(TEXT("default_read_only"), Pin->bDefaultValueIsReadOnly);
    Row->SetBoolField(TEXT("default_ignored"), Pin->bDefaultValueIsIgnored);
    Row->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin);
    Row->SetBoolField(TEXT("was_trashed"), Pin->bWasTrashed);
    Row->SetStringField(TEXT("parent_pin_name"), Pin->ParentPin ? Pin->ParentPin->PinName.ToString() : FString());
    Row->SetNumberField(TEXT("subpin_count"), Pin->SubPins.Num());
    Row->SetNumberField(TEXT("link_count"), Pin->LinkedTo.Num());
    TArray<TSharedPtr<FJsonValue>> Links;
    if (Pin->LinkedTo.Num() > 256 || Pin->SubPins.Num() > 64) { Complete = false; }
    else for (const UEdGraphPin* Linked : Pin->LinkedTo)
    {
        auto Link = MakeShared<FJsonObject>();
        Link->SetBoolField(TEXT("pin_present"), Linked != nullptr);
        if (Linked)
        {
            Link->SetStringField(TEXT("node_path"), Path(Linked->GetOwningNode()));
            Link->SetStringField(TEXT("pin_name"), Linked->PinName.ToString());
            Link->SetStringField(TEXT("pin_id"), Linked->PinId.ToString(EGuidFormats::DigitsWithHyphens));
        }
        else { Complete = false; }
        Links.Add(MakeShared<FJsonValueObject>(Link));
    }
    Row->SetArrayField(TEXT("links"), Links); return Row;
}

TSharedRef<FJsonObject> DescribeGraph(UEdGraph* Graph, UBlueprint* Blueprint, const UEdGraphSchema_K2* Schema,
    const UClass* TunnelClass, bool& Complete,const TFunction<bool()>* Fence=nullptr)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("graph_path"), Path(Graph));
    Row->SetStringField(TEXT("graph_name"), Graph ? Graph->GetName() : FString());
    const bool Identity = FullyLoaded(Graph) && Graph->GetOuter() == Blueprint && Graph->GetOutermost() == Blueprint->GetOutermost();
    Row->SetBoolField(TEXT("exact_loaded_own_graph"), Identity);
    if (!Identity) { Complete = false; return Row; }
    Row->SetStringField(TEXT("graph_guid"), Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Row->SetBoolField(TEXT("graph_guid_valid"), Graph->GraphGuid.IsValid());
    Row->SetStringField(TEXT("object_flags_string"), LexToString(static_cast<uint32>(Graph->GetFlags())));
    Row->SetStringField(TEXT("schema_class_path"), Path(Graph->Schema.Get()));if(Fence&&!(*Fence)()){Complete=false;return Row;}
    const bool ExactSchema = Graph->Schema.Get() == Schema->GetClass();if(Fence&&!(*Fence)()){Complete=false;return Row;}
    Row->SetBoolField(TEXT("exact_k2_schema"), ExactSchema);
    if (ExactSchema) { Row->SetNumberField(TEXT("schema_graph_type"), static_cast<int32>(Schema->GetGraphType(Graph))); }
    else { Complete = false; }
    Row->SetBoolField(TEXT("editable"), Graph->bEditable);
    Row->SetBoolField(TEXT("allow_deletion"), Graph->bAllowDeletion);
    Row->SetBoolField(TEXT("allow_renaming"), Graph->bAllowRenaming);
    Row->SetNumberField(TEXT("subgraph_count"), Graph->SubGraphs.Num());
    Row->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
    TArray<TSharedPtr<FJsonValue>> Nodes, Ports; int32 TunnelCount = 0;
    if (Graph->Nodes.Num() > 256) { Complete = false; }
    else for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
    {
        const UEdGraphNode* Node = Graph->Nodes[Index];if(Fence&&!(*Fence)()){Complete=false;return Row;} auto NodeRow = MakeShared<FJsonObject>();
        NodeRow->SetNumberField(TEXT("ordinal"), Index); NodeRow->SetStringField(TEXT("object_path"), Path(Node));
        const bool NodeValid = FullyLoaded(Node) && Node->GetOuter() == Graph;
        NodeRow->SetBoolField(TEXT("exact_loaded_own_node"), NodeValid);
        if (!NodeValid) { Complete = false; Nodes.Add(MakeShared<FJsonValueObject>(NodeRow)); continue; }
        NodeRow->SetStringField(TEXT("class_path"), Path(Node->GetClass()));
        NodeRow->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
        NodeRow->SetStringField(TEXT("object_flags_string"), LexToString(static_cast<uint32>(Node->GetFlags())));
        NodeRow->SetNumberField(TEXT("x"), Node->NodePosX); NodeRow->SetNumberField(TEXT("y"), Node->NodePosY);
        const bool ExactTunnel = Node->GetClass() == TunnelClass; NodeRow->SetBoolField(TEXT("exact_tunnel"), ExactTunnel);
        if (ExactTunnel)
        {
            const auto* Tunnel = static_cast<const UK2Node_Tunnel*>(Node); ++TunnelCount;
            NodeRow->SetBoolField(TEXT("can_have_inputs"), Tunnel->bCanHaveInputs);
            NodeRow->SetBoolField(TEXT("can_have_outputs"), Tunnel->bCanHaveOutputs);
            NodeRow->SetStringField(TEXT("input_sink_path"), Path(Tunnel->InputSinkNode.Get()));if(Fence&&!(*Fence)()){Complete=false;return Row;}
            NodeRow->SetStringField(TEXT("output_source_path"), Path(Tunnel->OutputSourceNode.Get()));if(Fence&&!(*Fence)()){Complete=false;return Row;}
            NodeRow->SetNumberField(TEXT("cached_has_latent_functions"), Tunnel->MetaData.HasLatentFunctions);
            NodeRow->SetStringField(TEXT("metadata_category"), Tunnel->MetaData.Category.ToString());
            NodeRow->SetStringField(TEXT("metadata_tooltip"), Tunnel->MetaData.ToolTip.ToString());
            NodeRow->SetStringField(TEXT("metadata_keywords"), Tunnel->MetaData.Keywords.ToString());
            NodeRow->SetStringField(TEXT("metadata_compact_title"), Tunnel->MetaData.CompactNodeTitle.ToString());
        }
        NodeRow->SetNumberField(TEXT("pin_count"), Node->Pins.Num()); TArray<TSharedPtr<FJsonValue>> Pins;
        if (Node->Pins.Num() > 256) { Complete = false; }
        else for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
        {
            const UEdGraphPin* Pin = Node->Pins[PinIndex]; auto PinRow = DescribePin(Pin, PinIndex, Complete,Fence);if(Fence&&!(*Fence)()){Complete=false;return Row;}
            Pins.Add(MakeShared<FJsonValueObject>(PinRow));
            // This is a source-port projection, not allocated instance-pin evidence.
            if (ExactTunnel && Pin && !Pin->ParentPin)
            {
                auto Port = MakeShared<FJsonObject>();
                Port->SetStringField(TEXT("source_node_path"), Node->GetPathName());
                Port->SetStringField(TEXT("source_node_guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
                Port->SetObjectField(TEXT("source_pin"), PinRow);
                Port->SetStringField(TEXT("instance_direction_from_public_complement_rule"), Pin->Direction == EGPD_Input
                    ? TEXT("output") : Pin->Direction == EGPD_Output ? TEXT("input") : TEXT("unknown"));
                Ports.Add(MakeShared<FJsonValueObject>(Port));
            }
        }
        NodeRow->SetArrayField(TEXT("pins"), Pins); Nodes.Add(MakeShared<FJsonValueObject>(NodeRow));
    }
    Row->SetNumberField(TEXT("exact_tunnel_count"), TunnelCount);
    Row->SetArrayField(TEXT("nodes"), Nodes); Row->SetArrayField(TEXT("source_ports"), Ports); return Row;
}

TSharedRef<FJsonObject> Observe(UBlueprint* Blueprint, const UEdGraphSchema_K2* Schema, const UClass* TunnelClass,const TFunction<bool()>* Fence=nullptr)
{
    auto State = MakeShared<FJsonObject>(); bool Complete = FullyLoaded(Blueprint)
        && Blueprint->GetPathName() == StockBlueprintPath && Blueprint->GetOutermost()->GetName() == StockPackageName;
    State->SetBoolField(TEXT("exact_loaded_stock_blueprint"), Complete);
    State->SetStringField(TEXT("blueprint_path"), Path(Blueprint));
    if (!Complete) { State->SetBoolField(TEXT("observations_complete"), false); return State; }
    State->SetStringField(TEXT("class_path"), Path(Blueprint->GetClass()));
    State->SetNumberField(TEXT("blueprint_type"), static_cast<int32>(Blueprint->BlueprintType.GetValue()));
    State->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(Blueprint->Status.GetValue()));
    State->SetStringField(TEXT("object_flags_string"), LexToString(static_cast<uint32>(Blueprint->GetFlags())));
    State->SetStringField(TEXT("native_parent_class_path"), Path(Blueprint->ParentClass.Get()));if(Fence&&!(*Fence)()){State->SetBoolField(TEXT("observations_complete"),false);return State;}
    State->SetStringField(TEXT("parent_class_flags_string"), Blueprint->ParentClass ? LexToString(static_cast<uint32>(Blueprint->ParentClass->ClassFlags)) : FString());if(Fence&&!(*Fence)()){State->SetBoolField(TEXT("observations_complete"),false);return State;}
    State->SetStringField(TEXT("generated_class_path"), Path(Blueprint->GeneratedClass.Get()));if(Fence&&!(*Fence)()){State->SetBoolField(TEXT("observations_complete"),false);return State;}
    State->SetStringField(TEXT("skeleton_generated_class_path"), Path(Blueprint->SkeletonGeneratedClass.Get()));if(Fence&&!(*Fence)()){State->SetBoolField(TEXT("observations_complete"),false);return State;}
    State->SetBoolField(TEXT("has_been_regenerated"), Blueprint->bHasBeenRegenerated);
    State->SetBoolField(TEXT("is_regenerating_on_load"), Blueprint->bIsRegeneratingOnLoad);
    State->SetObjectField(TEXT("package"), PackageState(Blueprint->GetOutermost()));
    State->SetNumberField(TEXT("macro_graph_count"), Blueprint->MacroGraphs.Num());
    State->SetNumberField(TEXT("variable_count"), Blueprint->NewVariables.Num());
    State->SetNumberField(TEXT("interface_count"), Blueprint->ImplementedInterfaces.Num());
    TArray<TSharedPtr<FJsonValue>> Inventory, Macros;
    if (Blueprint->MacroGraphs.Num() > 256) { Complete = false; }
    else for (UEdGraph* Graph : Blueprint->MacroGraphs)
    {
        if(Fence&&!(*Fence)()){State->SetBoolField(TEXT("observations_complete"),false);return State;}auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), Path(Graph));
        Row->SetStringField(TEXT("name"), Graph ? Graph->GetName() : FString());
        Row->SetStringField(TEXT("guid"), Graph ? Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString());
        Row->SetBoolField(TEXT("fully_loaded"), FullyLoaded(Graph)); Inventory.Add(MakeShared<FJsonValueObject>(Row));
    }
    for (const TCHAR* Name : MacroNames)
    {
        int32 Matches = 0; UEdGraph* Match = nullptr;
        if (Blueprint->MacroGraphs.Num() <= 256) for (UEdGraph* Graph : Blueprint->MacroGraphs)
        { if(Fence&&!(*Fence)()){State->SetBoolField(TEXT("observations_complete"),false);return State;}if (Graph && Graph->GetName() == Name) { ++Matches; Match = Graph; } }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("requested_macro_name"), Name);
        Row->SetNumberField(TEXT("exact_name_match_count"), Matches);
        if (Matches == 1) { Row->SetObjectField(TEXT("graph"), DescribeGraph(Match, Blueprint, Schema, TunnelClass, Complete,Fence)); }
        else { Complete = false; }
        if(Fence&&!(*Fence)()){State->SetBoolField(TEXT("observations_complete"),false);return State;}Macros.Add(MakeShared<FJsonValueObject>(Row));
    }
    State->SetArrayField(TEXT("macro_graph_inventory"), Inventory); State->SetArrayField(TEXT("macros"), Macros);
    State->SetBoolField(TEXT("observations_complete"), Complete); return State;
}
}

Function::FResult InspectNativeSupport(bool bAllowStockMacroLoad)
{
    Function::FResult R; const FEngineVersion& Version = FEngineVersion::Current();
    R.Data->SetStringField(TEXT("engine_version"), Version.ToString());
    R.Data->SetStringField(TEXT("engine_directory"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    R.Data->SetStringField(TEXT("project_file"), FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
    R.Data->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
    R.Data->SetStringField(TEXT("scope"), TEXT("exact stock StandardMacros ForLoop/ForEachLoop catalog; source ports are not instance construction or runtime proof"));
    R.Data->SetStringField(TEXT("stock_blueprint_path"), StockBlueprintPath);
    R.Data->SetBoolField(TEXT("stock_macro_load_allowed"), bAllowStockMacroLoad);
    R.Data->SetBoolField(TEXT("asset_load_attempted"), false);
    R.Data->SetBoolField(TEXT("asset_load_returned"), false);
    R.Data->SetBoolField(TEXT("implicit_load_compile_or_postload_changes_uninstrumented"), false);
    R.Data->SetBoolField(TEXT("asset_modification_requested"), false);
    R.Data->SetBoolField(TEXT("node_creation_requested"), false);
    R.Data->SetBoolField(TEXT("default_object_creation_requested"), false);
    R.Data->SetBoolField(TEXT("callback_or_event_invocation_requested"), false);
    R.Data->SetBoolField(TEXT("compile_requested"), false);
    R.Data->SetBoolField(TEXT("save_requested"), false);
    R.Data->SetBoolField(TEXT("latency_cache_population_requested"), false);
    R.Data->SetBoolField(TEXT("latency_behavior_verified"), false);
    R.Data->SetBoolField(TEXT("construction_implemented"), true);
    R.Data->SetBoolField(TEXT("construction_behavior_verified"), false);
    R.Data->SetBoolField(TEXT("private_property_read_or_write_requested"), false);
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Fail(R, TEXT("loop_catalog_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); return R; }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8)
    { Fail(R, TEXT("loop_catalog_version_unsupported"), TEXT("This catalog is scoped to installed UE 5.8.x.")); return R; }
    UClass* SchemaClass = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.EdGraphSchema_K2"));
    UClass* TunnelClass = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.K2Node_Tunnel"));
    UClass* MacroClass = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.K2Node_MacroInstance"));
    const UEdGraphSchema_K2* Schema = FullyLoaded(SchemaClass) ? Cast<UEdGraphSchema_K2>(SchemaClass->GetDefaultObject(false)) : nullptr;
    if (!Schema || Schema->GetClass() != SchemaClass || !FullyLoaded(TunnelClass) || !FullyLoaded(MacroClass)
        || !TunnelClass->HasAnyClassFlags(CLASS_Native) || !MacroClass->HasAnyClassFlags(CLASS_Native))
    { Fail(R, TEXT("loop_catalog_native_types_not_loaded"), TEXT("Requires loaded exact native macro/tunnel classes and the already-created standard K2 schema CDO.")); return R; }
    R.Data->SetStringField(TEXT("schema_class_path"), Path(SchemaClass));
    R.Data->SetStringField(TEXT("schema_cdo_path"), Path(Schema));
    R.Data->SetStringField(TEXT("tunnel_class_path"), Path(TunnelClass));
    R.Data->SetStringField(TEXT("macro_instance_class_path"), Path(MacroClass));
    UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, StockBlueprintPath);
    R.Data->SetBoolField(TEXT("stock_blueprint_loaded_before"), Blueprint != nullptr);
    R.Data->SetObjectField(TEXT("stock_package_before_load"), PackageState(FindPackage(nullptr, StockPackageName)));
    if (!Blueprint && bAllowStockMacroLoad)
    {
        R.Stage = TEXT("stock_macro_load"); R.Data->SetBoolField(TEXT("asset_load_attempted"), true);
        // The read snapshot starts after LoadObject; it cannot account for all
        // implicit post-load effects. Do not turn that limitation into changed=false.
        R.bChangeStateKnown = false;
        R.Data->SetBoolField(TEXT("implicit_load_compile_or_postload_changes_uninstrumented"), true);
        Blueprint = LoadObject<UBlueprint>(nullptr, StockBlueprintPath);
        R.Data->SetBoolField(TEXT("asset_load_returned"), true);
        R.Data->SetStringField(TEXT("asset_load_result_path"), Path(Blueprint));
    }
    R.Data->SetBoolField(TEXT("stock_blueprint_loaded_after"), Blueprint != nullptr);
    R.Data->SetObjectField(TEXT("stock_package_after_load"), PackageState(FindPackage(nullptr, StockPackageName)));
    if (!Blueprint)
    { Fail(R, TEXT("loop_catalog_stock_blueprint_not_loaded"), TEXT("Exact StandardMacros Blueprint is unavailable. Explicit load permission is separate from read-only observations.")); return R; }
    if (!FullyLoaded(Blueprint))
    { Fail(R, TEXT("loop_catalog_stock_blueprint_load_incomplete"), TEXT("Existing stock Blueprint has pending load/post-load flags; no implicit retry or repair was requested.")); return R; }
    R.Stage = TEXT("read_observations");
    const auto Before = Observe(Blueprint, Schema, TunnelClass); const auto After = Observe(Blueprint, Schema, TunnelClass);
    R.Data->SetObjectField(TEXT("before"), Before); R.Data->SetObjectField(TEXT("after"), After);
    R.bChanged = Text(Before) != Text(After);
    R.Data->SetBoolField(TEXT("observed_postload_source_state_preserved"), !R.bChanged);
    R.Data->SetBoolField(TEXT("observed_postload_source_state_changed"), R.bChanged);
    R.Data->SetStringField(TEXT("preservation_scope"), TEXT("two observations after optional stock load: Blueprint identity/status/package and bounded selected graph/node/pin metadata; not load-side-effect instrumentation or unselected macro bodies"));
    if (R.bChanged)
    { R.Stage = TEXT("verification"); Fail(R, TEXT("loop_catalog_source_state_changed"), TEXT("Observed stock source state changed across catalog reads; no modification was requested.")); return R; }
    if (!Before->GetBoolField(TEXT("observations_complete")) || !After->GetBoolField(TEXT("observations_complete")))
    { Fail(R, TEXT("loop_catalog_observation_incomplete"), TEXT("Exact selected macro graphs or bounded loaded source observations are unavailable; inspect partial data.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}

namespace
{
using FResult = Function::FResult;
struct FContext
{
    bool CreateGraphIfMissing=false;TFunction<bool(UEdGraphNode*)> GraphCreationCurrent;TStrongObjectPtr<UEdGraphNode> GraphCreationNode;
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;
    const UEdGraphSchema_K2* Schema = nullptr;
    UBlueprint* MacroBlueprint = nullptr;
    UEdGraph* MacroGraph = nullptr;
    UClass* TunnelClass = nullptr;
    UClass* MacroClass = nullptr;
    int32 X = 0, Y = 0;
    bool bFirst = false, bLast = false, bForEach = false;
    FEdGraphPinType ElementPinType;
    FString ElementType;
    bool bFiniteForEach=false,bSkeletalForEach=false,bAssetDataForEach=false,bPresetLibraryForEach=false;
    TFunction<bool()> FiniteCheckpoint;
    TFunction<bool(const UK2Node_MacroInstance*,bool)> FiniteShape;
    int32 First = 0, Last = 0;
    TSharedRef<FJsonObject> Before = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> MacroBefore = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> CallsBefore = MakeShared<FJsonObject>();
};
bool Error(FResult& R, const TCHAR* Code, const FString& Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
FString Normalize(FString Value)
{ Value.TrimStartAndEndInline(); Value.ToLowerInline(); Value.ReplaceInline(TEXT(" "), TEXT("")); Value.ReplaceInline(TEXT("_"), TEXT("")); Value.ReplaceInline(TEXT("-"), TEXT("")); return Value; }
bool ExactInt32(const TSharedPtr<FJsonValue>& Value, int32& Out)
{
    if (!Value.IsValid() || Value->Type != EJson::Number) { return false; }
    const double Number = Value->AsNumber();
    if (!FMath::IsFinite(Number) || Number < static_cast<double>(MIN_int32) || Number > static_cast<double>(MAX_int32)
        || FMath::TruncToDouble(Number) != Number) { return false; }
    Out = static_cast<int32>(Number); return true;
}
bool Arguments(FContext& C, FResult& R, const FString& LoopType, const FString& FirstJson,
    const FString& LastJson, const FString& PositionJson, bool bCreate,
    const FString& ElementType, const FString& TypeObject, const FString& EnumType)
{
    const FString Kind = Normalize(LoopType);
    C.bForEach = Kind == TEXT("foreach") || Kind == TEXT("foreachloop");
    if (!C.bForEach && !Kind.IsEmpty() && Kind != TEXT("for") && Kind != TEXT("forloop"))
    { return Error(R, TEXT("loop_type_unsupported"), TEXT("Finite ForLoop and typed int/bool ForEach aliases only; While/WithBreak/arbitrary macros remain incomplete.")); }
    R.Data->SetStringField(TEXT("loop_type"), C.bForEach ? TEXT("for_each") : TEXT("for_loop"));
    const bool Skeletal=C.bForEach&&Normalize(ElementType)==TEXT("object")&&TypeObject.Equals(TEXT("/Script/Engine.SkeletalMeshComponent"),ESearchCase::CaseSensitive)&&EnumType.IsEmpty();
    const bool AssetData=C.bForEach&&Normalize(ElementType)==TEXT("struct")&&TypeObject.Equals(TEXT("/Script/CoreUObject.AssetData"),ESearchCase::CaseSensitive)&&EnumType.IsEmpty();
    if ((!Skeletal&&!AssetData&&!TypeObject.IsEmpty()) || !EnumType.IsEmpty() || (!C.bForEach && !ElementType.IsEmpty()))
    { return Error(R, TEXT("loop_type_descriptor_unsupported"), TEXT("ForLoop accepts no element descriptor; ForEach accepts primitive selectors, exact SkeletalMeshComponent object or exact CoreUObject.AssetData struct with no enum selector.")); }
    if (C.bForEach)
    {
        C.ElementType = Normalize(ElementType);
        if (C.ElementType == TEXT("integer")) { C.ElementType = TEXT("int"); }
        if (C.ElementType == TEXT("boolean")) { C.ElementType = TEXT("bool"); }
        if (C.ElementType != TEXT("int") && C.ElementType != TEXT("bool") && C.ElementType != TEXT("name") && !Skeletal && !AssetData)
        { return Error(R, TEXT("loop_element_type_unsupported"), TEXT("Typed ForEach admits int/bool, Name, exact SkeletalMeshComponent object or CoreUObject.AssetData struct only; other object/string/struct/enum cohorts remain incomplete.")); }
        C.bSkeletalForEach=Skeletal;C.bAssetDataForEach=AssetData;C.bFiniteForEach=Skeletal||AssetData||C.ElementType==TEXT("name");
        C.ElementPinType.PinCategory = AssetData?UEdGraphSchema_K2::PC_Struct:Skeletal?UEdGraphSchema_K2::PC_Object:C.ElementType==TEXT("name")?UEdGraphSchema_K2::PC_Name:C.ElementType == TEXT("int") ? UEdGraphSchema_K2::PC_Int : UEdGraphSchema_K2::PC_Boolean;
        R.Data->SetStringField(TEXT("element_type"), C.ElementType);
        R.Data->SetObjectField(TEXT("element_pin_type"), DescribeType(C.ElementPinType));
    }
    C.CreateGraphIfMissing=bCreate;
    TSharedPtr<FJsonValue> Value;
    if (!Function::Pin::ParseJson(FirstJson, Value, R)) { return false; }
    C.bFirst = Value->Type != EJson::Null;
    if (C.bFirst && !ExactInt32(Value, C.First))
    { return Error(R, TEXT("loop_index_invalid"), TEXT("first_index must be JSON null or an exact int32 number; bool/fraction/nonfinite/out-of-range values are rejected before mutation.")); }
    if (!Function::Pin::ParseJson(LastJson, Value, R)) { return false; }
    C.bLast = Value->Type != EJson::Null;
    if (C.bLast && !ExactInt32(Value, C.Last))
    { return Error(R, TEXT("loop_index_invalid"), TEXT("last_index must be JSON null or an exact int32 number; negative and reversed ranges remain legal engine semantics.")); }
    if (C.bForEach && (C.bFirst || C.bLast))
    { return Error(R, TEXT("loop_for_each_indices_unsupported"), TEXT("first_index/last_index do not apply to ForEach and cannot be silently ignored.")); }
    if (!Function::Pin::ParseJson(PositionJson, Value, R)) { return false; }
    if (Value->Type != EJson::Null)
    {
        if (Value->Type != EJson::Array || Value->AsArray().Num() != 2 || !ExactInt32(Value->AsArray()[0], C.X) || !ExactInt32(Value->AsArray()[1], C.Y)
            || C.X < -1000000 || C.X > 1000000 || C.Y < -1000000 || C.Y > 1000000)
        { return Error(R, TEXT("loop_position_invalid"), TEXT("node_position must be JSON null or two exact integer coordinates within +/-1000000.")); }
    }
    return true;
}
FString GraphKind(const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    if (Blueprint->FunctionGraphs.Contains(Graph)) { return TEXT("function"); }
    if (Blueprint->MacroGraphs.Contains(Graph)) { return TEXT("macro"); }
    if (Blueprint->UbergraphPages.Contains(Graph) || Blueprint->EventGraphs.Contains(Graph)) { return TEXT("event"); }
    return TEXT("unknown");
}

TSharedRef<FJsonObject> GraphDescription(const FContext& C)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("graph_name"), C.Graph->GetName());
    Row->SetStringField(TEXT("graph_id"), C.Graph->GraphGuid.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("graph_type"), GraphKind(C.Blueprint, C.Graph));
    Row->SetStringField(TEXT("object_path"), C.Graph->GetPathName()); Row->SetStringField(TEXT("schema"), C.Schema->GetClass()->GetPathName());
    Row->SetNumberField(TEXT("node_count"), C.Graph->Nodes.Num()); return Row;
}

bool ResolveTarget(FContext& C, FResult& R, const FString& Path, const FString& GraphName, const FString& GraphId, const FString& GraphType)
{
    if (!Function::Pin::SafeName(Path) || !Path.StartsWith(TEXT("/Game/")) || Path.Contains(TEXT(":")))
    { return Error(R, TEXT("loop_blueprint_path_invalid"), TEXT("Use an exact bounded Game Blueprint asset or package path.")); }
    const FString AssetPath = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
    if (!Function::Pin::SafeName(AssetPath)) { return Error(R, TEXT("loop_blueprint_path_invalid"), TEXT("Completed asset path exceeds the bound.")); }
    C.Blueprint = FindObject<UBlueprint>(nullptr, *AssetPath);
    if (!FullyLoaded(C.Blueprint)) { return Error(R, TEXT("loop_blueprint_not_loaded"), TEXT("Load the exact Blueprint through the official asset tool first; this operation never loads it implicitly.")); }
    UBlueprint* BP = C.Blueprint;
    C.bPresetLibraryForEach=C.bFiniteForEach&&!C.bSkeletalForEach&&(C.bAssetDataForEach||C.ElementType==TEXT("name"))&&BP->BlueprintType==BPTYPE_FunctionLibrary;
    if(C.bPresetLibraryForEach&&(!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=UBlueprintFunctionLibrary::StaticClass()||!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript!=nullptr||!BP->NewVariables.IsEmpty()||!BP->GeneratedVariables.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()||!BP->CachedUDSDependencies.IsEmpty()))
    {return Error(R,TEXT("loop_preset_loader_context_unsupported"),TEXT("Only exact native member-free PresetLoader function-library Name/AssetData ForEach authoring is admitted."));}
    if (BP->GetPathName() != AssetPath || BP->BlueprintType != (C.bPresetLibraryForEach?BPTYPE_FunctionLibrary:BPTYPE_Normal) || (C.bPresetLibraryForEach?!FullyLoaded(UBlueprintFunctionLibrary::StaticClass()):!FullyLoaded(BP->ParentClass)) || (C.bPresetLibraryForEach?(!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->SkeletonGeneratedClass.GetGCPtr()==nullptr):!BP->SkeletonGeneratedClass)
        || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->bIsRegeneratingOnLoad
        || BP->Status == BS_BeingCreated || BP->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
    { return Error(R, TEXT("loop_blueprint_state_unsupported"), TEXT("Requires an already-loaded normal idle runtime Blueprint with a current skeleton.")); }
    if(!C.bPresetLibraryForEach)
    {
    for (const UClass* Parent = BP->ParentClass; Parent; Parent = Parent->GetSuperClass())
    {
        if (!FullyLoaded(Parent) || !Parent->HasAnyClassFlags(CLASS_Native) || Parent->IsEditorOnly()
            || Parent->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Interface)
            || (Parent->GetOutermost()->GetName() != TEXT("/Script/Engine") && Parent->GetOutermost()->GetName() != TEXT("/Script/CoreUObject")))
        { return Error(R, TEXT("loop_blueprint_parent_unsupported"), TEXT("This cohort requires an exact loaded native Engine/CoreUObject runtime parent chain.")); }
    }
    }
    TArray<FString> External;
    for (TObjectIterator<UBlueprint> It; It; ++It)
    { if (*It != BP && BP->GeneratedClass && It->ParentClass && It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.AddUnique(It->GetPathName()); } }
    if (BP->GeneratedClass)
    {
        TArray<UObject*> Instances; GetObjectsOfClass(BP->GeneratedClass, Instances, true, RF_NoFlags);
        for (UObject* Instance : Instances) { if (IsValid(Instance) && Instance->GetOutermost() != BP->GetOutermost()) { External.AddUnique(Instance->GetPathName()); } }
    }
    External.Sort(); TArray<TSharedPtr<FJsonValue>> Rows; for (const FString& Value : External) { Rows.Add(MakeShared<FJsonValueString>(Value)); }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"), Rows);
    if (!External.IsEmpty()) { return Error(R, TEXT("loop_external_instances_unsupported"), TEXT("Loaded external instances or derived Blueprints are outside this mutation slice.")); }
    if (!Function::Pin::SafeName(GraphName, true) || !Function::Pin::SafeName(GraphId, true) || !Function::Pin::SafeName(GraphType, true))
    { return Error(R, TEXT("loop_graph_selector_invalid"), TEXT("Graph selectors must be bounded safe strings.")); }
    FGuid RequestedGuid; if (!GraphId.IsEmpty() && (!FGuid::Parse(GraphId, RequestedGuid) || !RequestedGuid.IsValid()))
    { return Error(R, TEXT("loop_graph_selector_invalid"), TEXT("graph_id must be a valid nonzero GUID.")); }
    FString Kind = Normalize(GraphType);
    if (Kind == TEXT("eventgraph") || Kind == TEXT("ubergraph")) { Kind = TEXT("event"); }
    else if (Kind == TEXT("functiongraph")) { Kind = TEXT("function"); }
    else if (Kind == TEXT("macrograph")) { Kind = TEXT("macro"); }
    if (!Kind.IsEmpty() && Kind != TEXT("any") && Kind != TEXT("event") && Kind != TEXT("function") && Kind != TEXT("macro"))
    { return Error(R, TEXT("loop_graph_type_unsupported"), TEXT("Initial selectors support existing direct event/function/macro K2 graphs only.")); }
    if (GraphName.IsEmpty() && GraphId.IsEmpty() && Kind.IsEmpty()) { Kind = TEXT("event"); }
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    if (Graphs.Num() > 256) { return Error(R, TEXT("loop_graph_budget_exceeded"), TEXT("Graph selection exceeds 256 owned graphs.")); }
    for (UEdGraph* Graph : Graphs)
    {
        if (!IsValid(Graph) || (!GraphName.IsEmpty() && !Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
            || (!GraphId.IsEmpty() && Graph->GraphGuid != RequestedGuid) || (!Kind.IsEmpty() && Kind != TEXT("any") && GraphKind(BP, Graph) != Kind)) { continue; }
        if (C.Graph) { return Error(R, TEXT("loop_graph_ambiguous"), TEXT("Graph selectors match multiple graphs; provide an exact name or GUID.")); } C.Graph = Graph;
    }
    if(!C.Graph&&C.CreateGraphIfMissing)
    {
        if(!GraphId.IsEmpty()||Kind!=TEXT("function")||GraphName.IsEmpty())return Error(R,TEXT("loop_graph_creation_selector_unsupported"),TEXT("Missing graph creation requires an explicit function name/type and no graph_id; missing event/macro creation remains incomplete."));
        if(C.bFiniteForEach||C.bForEach||C.bPresetLibraryForEach)return Error(R,TEXT("loop_graph_creation_profile_unsupported"),TEXT("New function creation is limited to normal Blueprint stable ForLoop or primitive Array profiles; existing selected graph support is unchanged."));
        UBlueprint* ReadonlySource=FindObject<UBlueprint>(nullptr,StockBlueprintPath);
        if(!FullyLoaded(ReadonlySource)||ReadonlySource->GetOutermost()->IsDirty())return Error(R,TEXT("loop_graph_creation_macro_source_unavailable"),TEXT("Missing function creation requires the existing loaded clean StandardMacros source; no graph was created."));
        if(!Function::CreateFunctionGraphForNode(BP,GraphName,R,C.Graph,C.GraphCreationCurrent,ReadonlySource))return false;
        C.FiniteCheckpoint=[&C](){return C.GraphCreationCurrent(C.GraphCreationNode.Get());};
    }
    else if(C.Graph&&C.CreateGraphIfMissing)
    {
        auto Resolution=MakeShared<FJsonObject>();Resolution->SetBoolField(TEXT("requested_create_if_missing"),true);Resolution->SetBoolField(TEXT("created"),false);Resolution->SetBoolField(TEXT("reused_existing"),true);Resolution->SetStringField(TEXT("graph_path"),C.Graph->GetPathName());Resolution->SetStringField(TEXT("graph_guid"),C.Graph->GraphGuid.ToString(EGuidFormats::Digits));Resolution->SetBoolField(TEXT("implicit_skeleton_compile_possible"),false);R.Data->SetObjectField(TEXT("graph_resolution"),Resolution);
    }
    if (!C.Graph) { return Error(R, TEXT("loop_graph_not_found"), TEXT("No matching existing graph; nothing was created.")); }
    if(C.bPresetLibraryForEach&&(C.Graph->GetFName()!=TEXT("PresetLoader")||!BP->FunctionGraphs.Contains(C.Graph)||C.Graph->GetOuter()!=BP))
    {return Error(R,TEXT("loop_preset_loader_graph_unsupported"),TEXT("This function-library route requires the exact directly owned PresetLoader function graph; no other BFL function/event/macro is admitted."));}
    UClass* SchemaClass = C.Graph->Schema.Get();
    C.Schema = FullyLoaded(SchemaClass) && SchemaClass->GetPathName() == TEXT("/Script/BlueprintGraph.EdGraphSchema_K2")
        ? Cast<UEdGraphSchema_K2>(SchemaClass->GetDefaultObject(false)) : nullptr;
    if (!FullyLoaded(C.Graph) || C.Graph->GetOuter() != BP || !C.Graph->GraphGuid.IsValid() || GraphKind(BP, C.Graph) == TEXT("unknown")
        || !FullyLoaded(C.Schema) || C.Schema->GetClass() != SchemaClass || C.Graph->Nodes.Num() >= 2048)
    { return Error(R, TEXT("loop_graph_state_unsupported"), TEXT("Requires a directly owned standard K2 graph with valid GUID and room below 2048 nodes.")); }
    if (!C.Schema->DoesGraphSupportImpureFunctions(C.Graph))
    { return Error(R, TEXT("loop_graph_operation_rejected"), TEXT("Selected graph does not permit this impure loop operation.")); }
    R.Data->SetObjectField(TEXT("graph"), GraphDescription(C)); R.Data->SetStringField(TEXT("resolved_blueprint_path"), BP->GetPathName());
    R.Data->SetBoolField(TEXT("loaded_during_operation"), false); R.Data->SetBoolField(TEXT("schema_cdo_already_loaded"), true); return true;
}

FString TypeText(const FEdGraphPinType& Type)
{ FString Out; FEdGraphPinType::StaticStruct()->ExportText(Out, &Type, &Type, nullptr, PPF_None, nullptr); return Out; }

TSharedRef<FJsonObject> NodeState(const UEdGraphNode* Node, const FContext* Observed = nullptr)
{
    auto Row = MakeShared<FJsonObject>();
    auto Current=[&](){return !Observed||!Observed->FiniteCheckpoint||Observed->FiniteCheckpoint();};
    if(!Current()){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}
    Row->SetStringField(TEXT("object_path"), Node->GetPathName());
    Row->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    Row->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); Row->SetStringField(TEXT("comment"), Node->NodeComment);
    Row->SetNumberField(TEXT("x"), Node->NodePosX); Row->SetNumberField(TEXT("y"), Node->NodePosY);
    Row->SetNumberField(TEXT("enabled_state"), static_cast<int32>(Node->GetDesiredEnabledState())); Row->SetBoolField(TEXT("ghost"), Node->IsAutomaticallyPlacedGhostNode());
    TArray<TSharedPtr<FJsonValue>> Pins;
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->WasTrashed()) { Row->SetBoolField(TEXT("pin_observation_incomplete"), true); continue; }
        auto P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("name"), Pin->PinName.ToString()); P->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::Digits));
        P->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        const FEdGraphPinType PhysicalType=Pin->PinType;auto Type = Function::Pin::Describe(Observed?PhysicalType:Pin->PinType); if(!Current()){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;} Type->SetBoolField(TEXT("is_const"), Pin->PinType.bIsConst); P->SetObjectField(TEXT("pin_type"), Type);
        P->SetStringField(TEXT("pin_type_text"), TypeText(Observed?PhysicalType:Pin->PinType)); if(!Current()){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;} P->SetStringField(TEXT("default_value"), Pin->DefaultValue);
        P->SetStringField(TEXT("autogenerated_default_value"), Pin->AutogeneratedDefaultValue);
        if(Observed&&Observed->FiniteCheckpoint)
        {UObject* D=Pin->DefaultObject.Get();if(!Current()){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}if(D&&!FullyLoaded(D)){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}TStrongObjectPtr<UObject> HeldDefault(D);P->SetStringField(TEXT("default_object"),Path(D));if(!Current()){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}}
        else P->SetStringField(TEXT("default_object"), Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString());
        if(Observed&&Observed->FiniteCheckpoint){const FText Copy=Pin->DefaultTextValue;const FString Display=Copy.ToString();if(!Current()){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}P->SetStringField(TEXT("default_text"),Display);}else P->SetStringField(TEXT("default_text"),Pin->DefaultTextValue.ToString()); P->SetBoolField(TEXT("hidden"), Pin->bHidden); P->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin);
        P->SetBoolField(TEXT("default_value_is_ignored"), Pin->bDefaultValueIsIgnored); P->SetBoolField(TEXT("default_value_is_read_only"), Pin->bDefaultValueIsReadOnly);
        P->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable); P->SetBoolField(TEXT("advanced_view"), Pin->bAdvancedView);
        P->SetBoolField(TEXT("display_as_mutable_ref"), Pin->bDisplayAsMutableRef);
        if (Pin->ParentPin && Pin->ParentPin->WasTrashed()) { P->SetBoolField(TEXT("parent_pin_observation_incomplete"), true); }
        P->SetStringField(TEXT("parent_pin_id"), Pin->ParentPin && !Pin->ParentPin->WasTrashed() ? Pin->ParentPin->PinId.ToString(EGuidFormats::Digits) : FString());
        TArray<TSharedPtr<FJsonValue>> Children;
        for (const UEdGraphPin* Child : Pin->SubPins)
        {
            if (!Child || Child->WasTrashed()) { P->SetBoolField(TEXT("sub_pin_observation_incomplete"), true); Children.Add(MakeShared<FJsonValueString>(FString())); }
            else { Children.Add(MakeShared<FJsonValueString>(Child->PinId.ToString(EGuidFormats::Digits))); }
        }
        P->SetArrayField(TEXT("sub_pin_ids"), Children); TArray<FString> Edges;
        for (const UEdGraphPin* Other : Pin->LinkedTo)
        {
            if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked())) { P->SetBoolField(TEXT("link_observation_incomplete"), true); continue; }
            Edges.Add(Other->GetOwningNodeUnchecked()->GetPathName() + TEXT(":") + Other->PinName.ToString());
        }
        Edges.Sort();
        TArray<TSharedPtr<FJsonValue>> Links; for (const FString& Edge : Edges) { Links.Add(MakeShared<FJsonValueString>(Edge)); } P->SetArrayField(TEXT("links"), Links);
        Pins.Add(MakeShared<FJsonValueObject>(P));
    }
    Row->SetArrayField(TEXT("pins"), Pins);
    if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
    {
        Row->SetStringField(TEXT("function_name"), Call->FunctionReference.GetMemberName().ToString()); const UClass* Owner = Call->FunctionReference.GetMemberParentClass();if(!Current()){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}
        if(Observed&&Owner&&!FullyLoaded(Owner)){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}TStrongObjectPtr<UClass> HeldOwner(Observed?const_cast<UClass*>(Owner):nullptr);Row->SetStringField(TEXT("function_member_parent_path"), Owner ? Owner->GetPathName() : FString());
        Row->SetStringField(TEXT("function_member_guid"), Call->FunctionReference.GetMemberGuid().ToString(EGuidFormats::Digits)); Row->SetBoolField(TEXT("function_self_context"), Call->FunctionReference.IsSelfContext());
    }
    if (const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node))
    {
        if(Observed){UBlueprint* B=Macro->GetSourceBlueprint();if(!Current()||(B&&!FullyLoaded(B))){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}TStrongObjectPtr<UBlueprint> HeldSource(B);Row->SetStringField(TEXT("macro_blueprint_path"),Path(B));}
        else Row->SetStringField(TEXT("macro_blueprint_path"), Path(Macro->GetSourceBlueprint()));
        Row->SetStringField(TEXT("resolved_wildcard_type_text"), TypeText(Macro->ResolvedWildcardType));if(!Current()){Row->SetBoolField(TEXT("pin_observation_incomplete"),true);return Row;}
    }
    return Row;
}

bool Capture(const FContext& C, const TSharedRef<FJsonObject>& Out)
{
    if (!IsValid(C.Blueprint) || !IsValid(C.Graph) || C.Graph->GetOuter() != C.Blueprint) { return false; }
    Out->SetStringField(TEXT("blueprint_path"), C.Blueprint->GetPathName()); Out->SetStringField(TEXT("parent_class"), C.Blueprint->ParentClass ? C.Blueprint->ParentClass->GetPathName() : FString());
    if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
    Out->SetBoolField(TEXT("dirty"), C.Blueprint->GetOutermost()->IsDirty()); Out->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(C.Blueprint->Status.GetValue()));
    TArray<TSharedPtr<FJsonValue>> Variables;
    const TArray<FBPVariableDescription> FiniteVariables=(C.bFiniteForEach||C.GraphCreationCurrent)?C.Blueprint->NewVariables:TArray<FBPVariableDescription>();
    for (const FBPVariableDescription& Variable : (C.bFiniteForEach||C.GraphCreationCurrent)?FiniteVariables:C.Blueprint->NewVariables)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Variable.VarName.ToString()); Row->SetStringField(TEXT("guid"), Variable.VarGuid.ToString(EGuidFormats::Digits));
        FString Declaration; FBPVariableDescription::StaticStruct()->ExportText(Declaration, &Variable, &Variable, C.Blueprint, PPF_None, nullptr);
        if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;Row->SetStringField(TEXT("declaration_text"), Declaration);
        Row->SetStringField(TEXT("type"), TypeText(Variable.VarType));if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false; Row->SetStringField(TEXT("flags_string"), LexToString(Variable.PropertyFlags)); Row->SetStringField(TEXT("stored_default"), Variable.DefaultValue);
        Row->SetStringField(TEXT("category"), Variable.Category.ToString()); TArray<TSharedPtr<FJsonValue>> Metadata;
        for (const FBPVariableMetaDataEntry& Entry : Variable.MetaDataArray)
        { auto M = MakeShared<FJsonObject>(); M->SetStringField(TEXT("key"), Entry.DataKey.ToString()); M->SetStringField(TEXT("value"), Entry.DataValue); Metadata.Add(MakeShared<FJsonValueObject>(M)); }
        Row->SetArrayField(TEXT("metadata"), Metadata); Variables.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("variables"), Variables); TArray<TSharedPtr<FJsonValue>> Interfaces;
    const TArray<FBPInterfaceDescription> FiniteInterfaces=(C.bFiniteForEach||C.GraphCreationCurrent)?C.Blueprint->ImplementedInterfaces:TArray<FBPInterfaceDescription>();
    for (const FBPInterfaceDescription& Interface : (C.bFiniteForEach||C.GraphCreationCurrent)?FiniteInterfaces:C.Blueprint->ImplementedInterfaces)
    {
        if (!IsValid(Interface.Interface)) { return false; } auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("class_path"), Interface.Interface->GetPathName());if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
        TArray<TSharedPtr<FJsonValue>> Rows; for (const UEdGraph* Graph : Interface.Graphs) { if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;if (!IsValid(Graph)) { return false; } Rows.Add(MakeShared<FJsonValueString>(Graph->GetPathName())); }
        Row->SetArrayField(TEXT("graphs"), Rows); Interfaces.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("interfaces"), Interfaces); TArray<UEdGraph*> Graphs; C.Blueprint->GetAllGraphs(Graphs);if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
    if (Graphs.Num() > 256 || !Graphs.Contains(C.Graph)) { return false; }
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<FGuid> GraphIds; int32 TotalPins = 0, TotalNodes = 0;
    for (const UEdGraph* Graph : Graphs)
    {
        if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
        if (!IsValid(Graph) || !Graph->GraphGuid.IsValid() || GraphIds.Contains(Graph->GraphGuid) || Graph->Nodes.Num() > 2048 || (TotalNodes += Graph->Nodes.Num()) > 8192) { return false; }
        GraphIds.Add(Graph->GraphGuid); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), Graph->GetPathName()); Row->SetStringField(TEXT("guid"), Graph->GraphGuid.ToString(EGuidFormats::Digits));
        Row->SetStringField(TEXT("name"), Graph->GetName()); Row->SetStringField(TEXT("type"), GraphKind(C.Blueprint, Graph)); TArray<TSharedPtr<FJsonValue>> Nodes; TSet<FGuid> NodeIds;
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
            if (!IsValid(Node) || !Node->NodeGuid.IsValid() || NodeIds.Contains(Node->NodeGuid) || Node->Pins.Num() > 512) { return false; } NodeIds.Add(Node->NodeGuid);
            TSet<FGuid> PinIds;
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (++TotalPins > 16384 || !Pin || Pin->WasTrashed() || !Pin->PinId.IsValid() || PinIds.Contains(Pin->PinId)) { return false; } PinIds.Add(Pin->PinId);
                for (const UEdGraphPin* Other : Pin->LinkedTo)
                { if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked()) || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(Pin))) { return false; } }
            }
            Nodes.Add(MakeShared<FJsonValueObject>(NodeState(Node,(C.bFiniteForEach||C.GraphCreationCurrent)?&C:nullptr)));if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
        }
        Row->SetArrayField(TEXT("nodes"), Nodes); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("graphs"), Rows); return true;
}

bool PriorPreserved(const FContext& C, const TSharedRef<FJsonObject>& After, const UEdGraphNode* Created)
{
    if (!IsValid(Created) || Created->GetGraph() != C.Graph) { return false; }
    auto Expected = MakeShared<FJsonObject>(); auto Actual = MakeShared<FJsonObject>();
    for (const auto& Pair : C.Before->Values) { const FString Key(Pair.Key.ToView()); if (Key != TEXT("dirty") && Key != TEXT("blueprint_status")) { Expected->SetField(Key, Pair.Value); } }
    for (const auto& Pair : After->Values) { const FString Key(Pair.Key.ToView()); if (Key != TEXT("dirty") && Key != TEXT("blueprint_status")) { Actual->SetField(Key, Pair.Value); } }
    TArray<TSharedPtr<FJsonValue>> Graphs; int32 Removed = 0;
    for (const auto& Value : After->GetArrayField(TEXT("graphs")))
    {
        auto Graph = MakeShared<FJsonObject>(); const auto Original = Value->AsObject();
        for (const auto& Pair : Original->Values) { Graph->SetField(FString(Pair.Key.ToView()), Pair.Value); }
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (const auto& Node : Original->GetArrayField(TEXT("nodes")))
        {
            if (Node->AsObject()->GetStringField(TEXT("object_path")) == Created->GetPathName()) { ++Removed; continue; } Nodes.Add(Node);
        }
        Graph->SetArrayField(TEXT("nodes"), Nodes); Graphs.Add(MakeShared<FJsonValueObject>(Graph));
    }
    Actual->SetArrayField(TEXT("graphs"), Graphs); return Removed == 1 && Text(Expected) == Text(Actual);
}


bool SameGuid(const FGuid& Guid, const TCHAR* Expected)
{ FGuid Value; return FGuid::Parse(Expected, Value) && Guid == Value; }
FEdGraphPinType PortType(int32 Index)
{ FEdGraphPinType Type; Type.PinCategory = Index == 1 || Index == 2 || Index == 4 ? UEdGraphSchema_K2::PC_Int : UEdGraphSchema_K2::PC_Exec; return Type; }
constexpr const TCHAR* PortNames[] = {TEXT("execute"), TEXT("FirstIndex"), TEXT("LastIndex"), TEXT("LoopBody"), TEXT("Index"), TEXT("Completed")};
constexpr const TCHAR* PortIds[] = {
    TEXT("79F04C0C-4533-1E5B-2C94-FEBCA81B819C"), TEXT("1F658B6F-4542-D1B1-08BA-C9A3E209364A"), TEXT("561C9855-4B54-19D7-CC96-FF95063A7349"),
    TEXT("0498472E-4675-C9EA-5D9B-CB81267CF5C9"), TEXT("01C3E5E8-4DC4-4CD1-4196-5BBAFFAEF72B"), TEXT("13648DBC-495B-0058-33D0-2084EBF57E28")};
constexpr const TCHAR* BodyClasses[] = {
    TEXT("K2Node_Tunnel"), TEXT("K2Node_Tunnel"), TEXT("K2Node_TemporaryVariable"), TEXT("K2Node_AssignmentStatement"),
    TEXT("K2Node_CallFunction"), TEXT("K2Node_CallFunction"), TEXT("K2Node_AssignmentStatement"), TEXT("K2Node_IfThenElse"), TEXT("K2Node_ExecutionSequence")};
constexpr const TCHAR* BodyGuids[] = {
    TEXT("6D23C160-4C08-E068-453F-B7AB5F19574B"), TEXT("7E3F19A4-4CBD-29CD-83B6-959AAD90D985"), TEXT("262405AF-4ADF-4813-4B12-C6BA80F6E421"),
    TEXT("44C388BA-4BEB-BC5D-5C5B-85ACB1633416"), TEXT("CA1A6336-42DF-374B-D474-6DB0F5657615"), TEXT("3B4DB0E6-4942-3B54-3B89-0A9121CCFDF3"),
    TEXT("32A16E68-4623-6BCC-CD73-87BD9E946D0F"), TEXT("4B26A54E-4AD4-0C72-FB2E-0FB6950913DE"), TEXT("503FF1CD-442C-58D9-ECFF-C4A8F1C08CBE")};

constexpr const TCHAR* EachPortNames[] = {TEXT("Exec"), TEXT("Array"), TEXT("LoopBody"), TEXT("Array Element"), TEXT("Array Index"), TEXT("Completed")};
constexpr const TCHAR* EachPortIds[] = {
    TEXT("1806EEE4-447C-1ED5-2276-84BF76798860"), TEXT("7D5BCF1E-471D-6107-96CD-EB8ACBC184E3"), TEXT("B85C5CDA-4E0B-41BF-00A1-B1B1200400F5"),
    TEXT("C5104C63-4879-502F-EB4E-FEB13FF30228"), TEXT("D765AE4A-4ED2-8E83-2579-3CA25B1F4059"), TEXT("C73E4ACE-424C-4768-B14C-D79E8C5DCC61")};
constexpr const TCHAR* EachBodyClasses[] = {
    TEXT("/Script/BlueprintGraph.K2Node_Tunnel"),
    TEXT("/Script/BlueprintGraph.K2Node_Tunnel"),
    TEXT("/Script/UnrealEd.EdGraphNode_Comment"),
    TEXT("/Script/UnrealEd.EdGraphNode_Comment"),
    TEXT("/Script/UnrealEd.EdGraphNode_Comment"),
    TEXT("/Script/UnrealEd.EdGraphNode_Comment"),
    TEXT("/Script/UnrealEd.EdGraphNode_Comment"),
    TEXT("/Script/BlueprintGraph.K2Node_TemporaryVariable"),
    TEXT("/Script/BlueprintGraph.K2Node_AssignmentStatement"),
    TEXT("/Script/BlueprintGraph.K2Node_CallFunction"),
    TEXT("/Script/BlueprintGraph.K2Node_AssignmentStatement"),
    TEXT("/Script/BlueprintGraph.K2Node_IfThenElse"),
    TEXT("/Script/BlueprintGraph.K2Node_ExecutionSequence"),
    TEXT("/Script/BlueprintGraph.K2Node_CallArrayFunction"),
    TEXT("/Script/BlueprintGraph.K2Node_CallFunction"),
    TEXT("/Script/BlueprintGraph.K2Node_TemporaryVariable"),
    TEXT("/Script/UnrealEd.EdGraphNode_Comment"),
    TEXT("/Script/BlueprintGraph.K2Node_AssignmentStatement"),
    TEXT("/Script/BlueprintGraph.K2Node_AssignmentStatement"),
    TEXT("/Script/BlueprintGraph.K2Node_GetArrayItem")};
constexpr const TCHAR* EachBodyGuids[] = {
    TEXT("2879959A-49A1-5F6A-CE31-22BDF3EF4AC1"),
    TEXT("CB24560A-4B58-AE8F-16B7-FC9C044D0440"),
    TEXT("25F964FB-470C-74E2-0741-E287E07C09C7"),
    TEXT("D2C9AD32-4D20-0AF7-50B0-6397E470BCE5"),
    TEXT("C66840D6-4DC2-9CC9-463B-3585024E17E9"),
    TEXT("59B28A7D-4A46-510A-4721-2C8D8DAA1EF9"),
    TEXT("790D2C11-456C-19B1-54C5-359A5338165E"),
    TEXT("BCE39B3B-4B4A-DEEA-7F6A-6383C9DABCFB"),
    TEXT("58AB8E2D-4720-C8E0-B6CE-C689982BAB1B"),
    TEXT("57D3EC07-4AA9-B09D-113A-D8961495E255"),
    TEXT("486CAE9B-42BF-F4E2-0F7F-ADB7AD98B3D5"),
    TEXT("DB3FF8DF-4E31-63B1-A83E-A39DB2891F85"),
    TEXT("78969797-4469-E212-76C5-83B02CE91C89"),
    TEXT("7C83F2BD-4A56-F117-2750-C385217BBF76"),
    TEXT("9BB55D36-4E8D-676A-D921-96B17FB18801"),
    TEXT("342D5D75-4B90-54C7-7D58-A581E30B05B0"),
    TEXT("08B2F1FC-482F-486D-446B-F6A53CE593FD"),
    TEXT("843DA473-4728-83C6-EE0C-87AF6602BDBC"),
    TEXT("77E84C71-4700-1FB8-020D-2AB75A066BA5"),
    TEXT("6BDD0D03-476E-9255-579A-05AD9DDF99D5")};
FEdGraphPinType EachPortType(int32 Index, const FContext* Typed = nullptr)
{
    FEdGraphPinType Type;
    if (Index == 1 || Index == 3)
    { Type = Typed ? Typed->ElementPinType : FEdGraphPinType(); Type.PinCategory = Typed ? Typed->ElementPinType.PinCategory : UEdGraphSchema_K2::PC_Wildcard; }
    else { Type.PinCategory = Index == 4 ? UEdGraphSchema_K2::PC_Int : UEdGraphSchema_K2::PC_Exec; }
    if (Index == 1) { Type.ContainerType = EPinContainerType::Array; } return Type;
}

bool SourceCalls(const FContext& C, const TSharedRef<FJsonObject>& Out)
{
    UClass* Math = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetMathLibrary"));
    if (!FullyLoaded(Math) || !Math->HasAnyClassFlags(CLASS_Native) || Math->GetOutermost()->GetName() != TEXT("/Script/Engine") || Math->GetOutermost()->IsDirty() || Math->IsEditorOnly()) { return false; }
    TArray<TSharedPtr<FJsonValue>> Calls;
    const TArray<int32> Indices = C.bForEach ? TArray<int32>{9, 13, 14} : TArray<int32>{4, 5};
    for (int32 Index : Indices)
    {
        if (!IsValid(C.MacroGraph) || !C.MacroGraph->Nodes.IsValidIndex(Index)) { return false; }
        const auto* Call = Cast<UK2Node_CallFunction>(C.MacroGraph->Nodes[Index]);
        UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;TStrongObjectPtr<UFunction> HeldFunction(C.bFiniteForEach&&FullyLoaded(Function)?Function:nullptr);
        UClass* DeclaringClass = C.bForEach && Index == 13
            ? FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetArrayLibrary")) : Math;
        const UClass* ReferenceOwner=nullptr;if(C.FiniteCheckpoint){ReferenceOwner=Call?Call->FunctionReference.GetMemberParentClass():nullptr;if(!C.FiniteCheckpoint())return false;}
        const uint32 Required = FUNC_Native | FUNC_Public | FUNC_Static | FUNC_BlueprintCallable | FUNC_BlueprintPure;
        if (!FullyLoaded(DeclaringClass) || !DeclaringClass->HasAnyClassFlags(CLASS_Native) || DeclaringClass->IsEditorOnly()
            || DeclaringClass->GetOutermost()->GetName() != TEXT("/Script/Engine") || DeclaringClass->GetOutermost()->IsDirty()
            || !FullyLoaded(Function) || Function->GetOuterUClass() != DeclaringClass || (static_cast<uint32>(Function->FunctionFlags) & Required) != Required
            || Function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate | FUNC_EditorOnly)
            || Function->HasMetaData(TEXT("Latent")) || Call->IsLatentForMacros()
            || (C.FiniteCheckpoint?ReferenceOwner:Call->FunctionReference.GetMemberParentClass()) != DeclaringClass || Call->FunctionReference.IsSelfContext()
            || Call->FunctionReference.GetMemberGuid().IsValid() || Call->FunctionReference.GetMemberName() != Function->GetFName()) { return false; }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("source_node_guid"), Call->NodeGuid.ToString(EGuidFormats::Digits));
        Row->SetStringField(TEXT("function_path"), Function->GetPathName()); Row->SetStringField(TEXT("declaring_class"), DeclaringClass->GetPathName());
        Row->SetStringField(TEXT("function_flags_string"), LexToString(static_cast<uint32>(Function->FunctionFlags)));
        Row->SetBoolField(TEXT("latent_metadata_present"), Function->HasMetaData(TEXT("Latent")));
        Row->SetObjectField(TEXT("package"), PackageState(Function->GetOutermost())); Calls.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("functions"), Calls); return Calls.Num() == (C.bForEach ? 3 : 2);
}

bool MacroSource(FContext& C, FResult& R)
{
    C.TunnelClass = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.K2Node_Tunnel"));
    C.MacroClass = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.K2Node_MacroInstance"));
    C.MacroBlueprint = FindObject<UBlueprint>(nullptr, StockBlueprintPath);
    R.Data->SetBoolField(TEXT("stock_macro_load_attempted"), false);
    if (!FullyLoaded(C.TunnelClass) || !FullyLoaded(C.MacroClass) || !C.TunnelClass->HasAnyClassFlags(CLASS_Native)
        || !C.MacroClass->HasAnyClassFlags(CLASS_Native) || !FullyLoaded(C.MacroBlueprint))
    { return Error(R, TEXT("loop_stock_macro_not_loaded"), TEXT("Exact loaded native macro/tunnel classes and StandardMacros are required; load stock explicitly through the separate inspector permission first.")); }
    UBlueprint* BP = C.MacroBlueprint;
    if (BP->GetPathName() != StockBlueprintPath || BP->GetClass()->GetPathName() != TEXT("/Script/Engine.Blueprint")
        || BP->GetOutermost()->GetName() != StockPackageName || BP->GetOutermost()->IsDirty()
        || static_cast<uint32>(BP->GetOutermost()->GetPackageFlags()) != 262144u || BP->BlueprintType != BPTYPE_MacroLibrary || BP->Status != BS_UpToDate
        || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bIsRegeneratingOnLoad || !BP->bHasBeenRegenerated
        || (C.bFiniteForEach ? (!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr().GetPathName()!=TEXT("/Script/CoreUObject.Object")
            ||!BP->GeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr().GetPathName()!=TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros_C")
            ||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->SkeletonGeneratedClass.GetGCPtr().GetPathName()!=TEXT("/Engine/EditorBlueprintResources/StandardMacros.SKEL_StandardMacros_C"))
            : (!FullyLoaded(BP->ParentClass) || BP->ParentClass->GetPathName() != TEXT("/Script/CoreUObject.Object")
        || static_cast<uint32>(BP->ParentClass->GetClassFlags()) != 810027169u
        || Path(BP->GeneratedClass.Get()) != TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros_C")
        || Path(BP->SkeletonGeneratedClass.Get()) != TEXT("/Engine/EditorBlueprintResources/StandardMacros.SKEL_StandardMacros_C")
        || !FullyLoaded(BP->GeneratedClass) || !FullyLoaded(BP->SkeletonGeneratedClass) || !C.Blueprint->ParentClass->IsChildOf(BP->ParentClass))))
    { return Error(R, TEXT("loop_stock_macro_contract_mismatch"), TEXT("Loaded stock Blueprint runtime parent/status/package identity differs from observed UE 5.8.3 AJ1; no repair or fallback load is attempted.")); }
    if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
    for (const auto& Handle : BP->MacroGraphs)
    {
        UEdGraph* Graph=C.bFiniteForEach?FindObject<UEdGraph>(nullptr,*Handle.GetPathName()):Handle.Get();if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
        if (Graph && Graph->GetName() == (C.bForEach ? TEXT("ForEachLoop") : TEXT("ForLoop")))
        { if (C.MacroGraph) { return Error(R, TEXT("loop_stock_macro_ambiguous"), TEXT("Expected one exact ForLoop graph.")); } C.MacroGraph = Graph; }
    }
    if (!FullyLoaded(C.MacroGraph) || C.MacroGraph == C.Graph || C.MacroGraph->GetOuter() != BP
        || !SameGuid(C.MacroGraph->GraphGuid, C.bForEach ? TEXT("99DBFD55-40A7-9604-1F72-A5A9DA655026") : TEXT("55C904AF-4B45-FE17-61FB-55A8DB9FB801"))
        || (C.bFiniteForEach ? (!C.MacroGraph->Schema.GetGCPtr().IsResolved()||C.MacroGraph->Schema.GetGCPtr()!=C.Schema->GetClass()) : C.MacroGraph->Schema.Get() != C.Schema->GetClass()) || C.Schema->GetGraphType(C.MacroGraph) != GT_Macro
        || C.MacroGraph->Nodes.Num() != (C.bForEach ? 20 : 9) || !C.MacroGraph->SubGraphs.IsEmpty())
    { return Error(R, TEXT("loop_stock_graph_contract_mismatch"), TEXT("Exact ForLoop graph GUID/schema/body shape differs from actual AJ1 catalog.")); }
    for (int32 Index = 0; Index < (C.bForEach ? 20 : 9); ++Index)
    {
        UEdGraphNode* Node = C.MacroGraph->Nodes[Index];if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
        const FString ClassPath = C.bForEach ? FString(EachBodyClasses[Index]) : TEXT("/Script/BlueprintGraph.") + FString(BodyClasses[Index]);
        if (!FullyLoaded(Node) || Node->GetOuter() != C.MacroGraph || Node->GetClass()->GetPathName() != ClassPath
            || !Node->GetClass()->HasAnyClassFlags(CLASS_Native) || !SameGuid(Node->NodeGuid, C.bForEach ? EachBodyGuids[Index] : BodyGuids[Index]))
        { return Error(R, TEXT("loop_stock_body_contract_mismatch"), TEXT("Exact observed stock ForLoop node class/order/GUID differs; arbitrary macros are not admitted.")); }
    }
    for (int32 TunnelIndex = 0; TunnelIndex < 2; ++TunnelIndex)
    {
        const auto* Tunnel = Cast<UK2Node_Tunnel>(C.MacroGraph->Nodes[TunnelIndex]);
        if (!Tunnel || Tunnel->GetClass() != C.TunnelClass || Tunnel->bCanHaveInputs != (TunnelIndex == 1) || Tunnel->bCanHaveOutputs != (TunnelIndex == 0)
            || Tunnel->InputSinkNode || Tunnel->OutputSourceNode || Tunnel->Pins.Num() != (C.bForEach ? (TunnelIndex == 0 ? 2 : 4) : 3)
            || (Tunnel->MetaData.HasLatentFunctions != -1 && Tunnel->MetaData.HasLatentFunctions != 0))
        { return Error(R, TEXT("loop_stock_tunnel_contract_mismatch"), TEXT("Exact observed tunnel direction/ports/nonlatent cache state differs.")); }
        for (int32 Local = 0; Local < (C.bForEach ? (TunnelIndex == 0 ? 2 : 4) : 3); ++Local)
        {
            const int32 Index = (C.bForEach ? TunnelIndex * 2 : TunnelIndex * 3) + Local; const UEdGraphPin* Pin = Tunnel->Pins[Local];
            if (!Pin || Pin->WasTrashed() || Pin->GetOwningNodeUnchecked() != Tunnel || Pin->PinName != FName(C.bForEach ? EachPortNames[Index] : PortNames[Index])
                || !SameGuid(Pin->PinId, C.bForEach ? EachPortIds[Index] : PortIds[Index]) || Pin->Direction != (TunnelIndex == 0 ? EGPD_Output : EGPD_Input)
                || (C.bFiniteForEach ? Pin->PinType != EachPortType(Index) : TypeText(Pin->PinType) != TypeText(C.bForEach ? EachPortType(Index) : PortType(Index))) || Pin->ParentPin || !Pin->SubPins.IsEmpty()
                || Pin->bHidden || Pin->bNotConnectable || Pin->bDefaultValueIsReadOnly || Pin->bDefaultValueIsIgnored || Pin->bOrphanedPin
                || !Pin->DefaultValue.IsEmpty() || !Pin->AutogeneratedDefaultValue.IsEmpty() || Pin->DefaultObject
                || !Pin->DefaultTextValue.IsEmpty() || Pin->LinkedTo.Num() != (C.bForEach && Index == 1 ? 2 : 1))
            { return Error(R, TEXT("loop_stock_port_contract_mismatch"), TEXT("Observed exact ForLoop source pin GUID/type/default/flags differ from actual catalog.")); }
        }
    }
    C.MacroBefore = Observe(BP, C.Schema, C.TunnelClass,C.FiniteCheckpoint?&C.FiniteCheckpoint:nullptr);if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
    const auto Repeated = Observe(BP, C.Schema, C.TunnelClass,C.FiniteCheckpoint?&C.FiniteCheckpoint:nullptr);if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return false;
    if (!C.MacroBefore->GetBoolField(TEXT("observations_complete")) || Text(C.MacroBefore) != Text(Repeated) || !SourceCalls(C, C.CallsBefore))
    { return Error(R, TEXT("loop_stock_before_state_unobservable"), TEXT("Bounded source observations or exact pure nonlatent native math target metadata are unavailable or changed.")); }
    R.Data->SetObjectField(TEXT("macro_source_before"), C.MacroBefore); R.Data->SetObjectField(TEXT("source_native_calls_before"), C.CallsBefore);
    R.Data->SetStringField(TEXT("macro_blueprint_path"), StockBlueprintPath); R.Data->SetStringField(TEXT("macro_graph_path"), C.MacroGraph->GetPathName());
    R.Data->SetStringField(TEXT("macro_graph_guid"), C.MacroGraph->GraphGuid.ToString(EGuidFormats::Digits)); R.Data->SetStringField(TEXT("macro_name"), C.bForEach ? TEXT("ForEachLoop") : TEXT("ForLoop"));
    R.Data->SetStringField(TEXT("native_contract_basis"), TEXT("engine-loop-catalog-AJ1.json; installed exported source macro allocation and pure native math metadata; actual constructor/pin/runtime proof remains pending"));
    return true;
}

bool NodeShape(const FContext& C, const UK2Node_MacroInstance* Node, bool bTyped = true)
{
    if(C.FiniteShape)return C.FiniteShape(Node,bTyped);
    FEdGraphPinType Empty;
    if (!IsValid(Node) || Node->GetClass() != C.MacroClass || Node->GetGraph() != C.Graph || !C.Graph->Nodes.Contains(Node)
        || !Node->NodeGuid.IsValid() || Node->NodePosX != C.X || Node->NodePosY != C.Y || Node->Pins.Num() != 6
        || Node->GetSourceBlueprint() != C.MacroBlueprint || Node->GetMacroGraph() != C.MacroGraph
        || TypeText(Node->ResolvedWildcardType) != TypeText(C.bForEach && bTyped ? C.ElementPinType : Empty)) { return false; }
    TSet<FGuid> PinIds;
    for (int32 Index = 0; Index < 6; ++Index)
    {
        const UEdGraphPin* Pin = Node->Pins[Index];
        if (!Pin || Pin->WasTrashed() || Pin->GetOwningNodeUnchecked() != Node || !Pin->PinId.IsValid() || PinIds.Contains(Pin->PinId)
            || Pin->PinName != FName(C.bForEach ? EachPortNames[Index] : PortNames[Index]) || Pin->Direction != (Index < (C.bForEach ? 2 : 3) ? EGPD_Input : EGPD_Output)
            || TypeText(Pin->PinType) != TypeText(C.bForEach ? EachPortType(Index, bTyped ? &C : nullptr) : PortType(Index)) || Pin->ParentPin || !Pin->SubPins.IsEmpty() || !Pin->LinkedTo.IsEmpty()
            || Pin->bHidden || Pin->bNotConnectable || Pin->bDefaultValueIsReadOnly || Pin->bDefaultValueIsIgnored || Pin->bOrphanedPin
            || Pin->DefaultObject || !Pin->DefaultTextValue.IsEmpty()) { return false; }
        PinIds.Add(Pin->PinId);
    }
    return true;
}

bool DefaultsMatch(const FContext& C, const UK2Node_MacroInstance* Node)
{
    if (!IsValid(Node)) { return false; }
    for (int32 Index : {1, 2})
    {
        if (Index == 1 ? !C.bFirst : !C.bLast) { continue; }
        const UEdGraphPin* Pin = Node->FindPin(PortNames[Index], EGPD_Input);
        const FString Value = LexToString(Index == 1 ? C.First : C.Last);
        if (!Pin || Pin->WasTrashed() || Pin->bDefaultValueIsIgnored || Pin->bDefaultValueIsReadOnly || Pin->DefaultObject || !Pin->DefaultTextValue.IsEmpty()
            || !C.Schema->DoesDefaultValueMatch(*Pin, Value) || !C.Schema->IsCurrentPinDefaultValid(Pin).IsEmpty()) { return false; }
    }
    return true;
}
// Finite Name/SkeletalMeshComponent and MA exact AssetData ForEach roles; original int/bool/ForLoop constructors use no hooks.
struct FFiniteForEachScope
{
    FContext& C;FResult& R;bool Stopped=false,Attached=false,Bound=false,GuidFixed=false,PinsFixed=false,Typed=false;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;decltype(GUndo) Undo=nullptr;
    TStrongObjectPtr<UK2Node_MacroInstance> Selected;TStrongObjectPtr<UClass> Element;TStrongObjectPtr<UObject> ElementCDO;
    TArray<TStrongObjectPtr<UClass>> Hierarchy;UClass *Parent=nullptr,*Generated=nullptr,*Skeleton=nullptr;FString NodePath;FGuid NodeGuid;EObjectFlags NodeFlags=RF_NoFlags;
    struct FRole{TStrongObjectPtr<UObject> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;uint32 ClassFlags=0,PackageFlags=0;int32 Size=0;bool Frozen=false,WasDirty=false;FGuid GraphGuid;TObjectPtr<UClass> GraphSchema;};TArray<FRole> Roles;TMap<UEdGraph*,int32> GraphCounts;
    struct FPinRole{UEdGraphPin* P=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_MAX;FEdGraphPinType Type;FString Default,Auto;TObjectPtr<UObject> Object;FText Text;UEdGraphPin* ParentPin=nullptr;TArray<UEdGraphPin*> SubPins,Links;bool Hidden=false,Ignored=false,Readonly=false,Orphaned=false,NotConnectable=false;};
    struct FNodeRole{TStrongObjectPtr<UEdGraphNode> Hold;UEdGraph* Graph=nullptr;FGuid Guid;TArray<FPinRole> Pins;FMemberReference Member;bool Call=false,Tunnel=false;TObjectPtr<UK2Node_Tunnel> InputSink,OutputSource;};TArray<FNodeRole> Prior;
    TArray<FPinRole> Pins;int32 OldTargetCount=0;UBlueprint* Stock=nullptr;UEdGraph* StockEach=nullptr;TArray<UEdGraph*> StockGraphs;UClass *StockParent=nullptr,*StockGenerated=nullptr,*StockSkeleton=nullptr;
    TStrongObjectPtr<UK2Node_FunctionEntry> PresetEntry;FMemberReference PresetEntryReference;FGuid PresetEntryGuid;uint32 PresetEntryFlags=0;bool PresetEntryBound=false;TArray<FBPVariableDescription> PresetLocals;
    explicit FFiniteForEachScope(FContext& In,FResult& Result):C(In),R(Result){}
    ~FFiniteForEachScope(){C.FiniteCheckpoint=nullptr;C.FiniteShape=nullptr;}
    static bool Live(const UObject* O){return FullyLoaded(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Safe()const{return IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
    bool Bad(const TCHAR* At)
    {
        if(!Stopped){Stopped=true;R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=false;R.Stage=TEXT("verification");R.Data->SetStringField(TEXT("finite_for_each_failed_boundary"),At);R.Data->SetBoolField(TEXT("finite_for_each_followup_sdk_stopped"),true);R.Data->SetBoolField(TEXT("automatic_retry_safe"),false);Error(R,TEXT("loop_finite_scope_changed"),TEXT("Current exact finite type/source/target/macro/node/pin role is unavailable or changed; actual returned identity/specialization prefix is retained without cleanup, compile/save or automatic retry."));}return false;
    }
    bool Basic(){return !Stopped&&Safe()&&Live(Editor.Get())&&GEditor==Editor.Get()&&Live(World.Get())&&GWorld==World.Get()&&World->WorldType==EWorldType::Editor&&!World->bIsTearingDown&&GUndo==Undo;}
    bool Hold(UObject* O,bool Frozen)
    {
        if(!Live(O))return Bad(TEXT("hold_role"));for(const auto& V:Roles)if(V.Hold.Get()==O)return true;
        FRole V;V.Hold.Reset(O);V.Class=O->GetClass();V.Outer=O->GetOuter();V.Package=O->GetPackage();V.Path=O->GetPathName();V.Flags=O->GetFlags();V.Frozen=Frozen;if(UEdGraph* G=Cast<UEdGraph>(O)){V.GraphGuid=G->GraphGuid;V.GraphSchema=G->Schema.GetGCPtr();if(!V.GraphSchema.IsResolved())return Bad(TEXT("hold_graph_schema"));}
        if(const UClass* K=Cast<UClass>(O)){V.ClassFlags=uint32(K->GetClassFlags());V.Size=K->GetStructureSize();}if(const UPackage* P=Cast<UPackage>(O)){V.PackageFlags=P->GetPackageFlags();V.WasDirty=P->IsDirty();}Roles.Add(MoveTemp(V));return true;
    }
    static FPinRole PinRole(UEdGraphPin* P)
    {FPinRole V;V.P=P;V.Id=P->PinId;V.Name=P->PinName;V.Direction=P->Direction;V.Type=P->PinType;V.Default=P->DefaultValue;V.Auto=P->AutogeneratedDefaultValue;V.Object=P->DefaultObject;V.Text=P->DefaultTextValue;V.ParentPin=P->ParentPin;V.SubPins=P->SubPins;V.Links=P->LinkedTo;V.Hidden=P->bHidden;V.Ignored=P->bDefaultValueIsIgnored;V.Readonly=P->bDefaultValueIsReadOnly;V.Orphaned=P->bOrphanedPin;V.NotConnectable=P->bNotConnectable;return V;}
    static bool PinCurrent(const FPinRole& V,const UEdGraphPin* P,const UEdGraphNode* N)
    {return P==V.P&&P&&!P->WasTrashed()&&P->GetOwningNodeUnchecked()==N&&P->PinId==V.Id&&P->PinName==V.Name&&P->Direction==V.Direction&&P->PinType==V.Type&&P->PinType.bIsUObjectWrapper==V.Type.bIsUObjectWrapper&&P->PinType.bSerializeAsSinglePrecisionFloat==V.Type.bSerializeAsSinglePrecisionFloat&&P->PinType.PinValueType.bTerminalIsUObjectWrapper==V.Type.PinValueType.bTerminalIsUObjectWrapper&&P->DefaultValue.Equals(V.Default,ESearchCase::CaseSensitive)&&P->AutogeneratedDefaultValue.Equals(V.Auto,ESearchCase::CaseSensitive)&&P->DefaultObject.IsResolved()&&P->DefaultObject==V.Object&&P->DefaultTextValue.EqualTo(V.Text)&&P->ParentPin==V.ParentPin&&P->SubPins==V.SubPins&&P->LinkedTo==V.Links&&P->bHidden==V.Hidden&&P->bDefaultValueIsIgnored==V.Ignored&&P->bDefaultValueIsReadOnly==V.Readonly&&P->bOrphanedPin==V.Orphaned&&P->bNotConnectable==V.NotConnectable;}
    bool KeepGraph(UEdGraph* G,bool Frozen)
    {
        if(!Hold(G,Frozen)||G->Nodes.Num()>2048)return Bad(TEXT("graph_inventory"));GraphCounts.Add(G,G->Nodes.Num());
        for(const auto& H:G->Nodes){if(!H.IsResolved())return Bad(TEXT("prior_node_unresolved"));UEdGraphNode* N=FindObject<UEdGraphNode>(nullptr,*H.GetPathName());if(H!=N||!Live(N)||N->GetOuter()!=G||N->Pins.Num()>512||!Hold(N,Frozen))return Bad(TEXT("prior_node_role"));FNodeRole V;V.Hold.Reset(N);V.Graph=G;V.Guid=N->NodeGuid;if(const auto* Call=Cast<UK2Node_CallFunction>(N)){V.Call=true;V.Member=Call->FunctionReference;}if(const auto* Tunnel=Cast<UK2Node_Tunnel>(N)){V.Tunnel=true;V.InputSink=Tunnel->InputSinkNode;V.OutputSource=Tunnel->OutputSourceNode;if(!V.InputSink.IsResolved()||!V.OutputSource.IsResolved())return Bad(TEXT("prior_tunnel_handle"));}
            for(UEdGraphPin* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||!P->DefaultObject.IsResolved())return Bad(TEXT("prior_pin_role"));V.Pins.Add(PinRole(P));}Prior.Add(MoveTemp(V));}return true;
    }

    // MA only: metadata/layout identities, never an FAssetData value buffer or referenced asset.
    struct FAssetFieldRole
    {
        const FProperty* Field=nullptr;const FFieldClass* Class=nullptr;FName Name;uint64 Flags=0;int32 Offset=0,Size=0,ArrayDim=0;
        TObjectPtr<UScriptStruct> Struct;bool HasStruct=false;
    };
    struct FAssetStructRole
    {
        TStrongObjectPtr<UScriptStruct> Hold;TStrongObjectPtr<UPackage> Package;FString Path;
        EObjectFlags Flags=RF_NoFlags;uint32 StructFlags=0,PackageFlags=0;int32 Size=0,Alignment=0;
        TArray<const FProperty*> Inventory;TArray<FAssetFieldRole> Fields;
    };
    TArray<FAssetStructRole> AssetStructs;
    bool AssetDataCurrent()
    {
        if(!C.bAssetDataForEach)return true;
        for(const auto& V:AssetStructs)
        {
            const UScriptStruct* S=V.Hold.Get();const UPackage* P=V.Package.Get();
            if(!Basic()||!Live(S)||!Live(P)||S->GetClass()!=UScriptStruct::StaticClass()||S->GetOuter()!=P||S->GetPackage()!=P
                ||!S->GetPathName().Equals(V.Path,ESearchCase::CaseSensitive)||S->GetFlags()!=V.Flags||uint32(S->StructFlags)!=V.StructFlags
                ||S->GetStructureSize()!=V.Size||S->GetMinAlignment()!=V.Alignment||!P->GetName().Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)
                ||P->GetPackageFlags()!=V.PackageFlags||P->IsDirty())return Bad(TEXT("asset_data_struct_current"));
            auto Super=S->GetSuperStructIterator();++Super;if(Super)return Bad(TEXT("asset_data_struct_super"));
            // Verify pointer inventory before any previously captured FProperty is dereferenced.
            TArray<const FProperty*> Inventory;for(TFieldIterator<FProperty> It(S,EFieldIteratorFlags::ExcludeSuper);It;++It)Inventory.Add(*It);
            if(Inventory!=V.Inventory)return Bad(TEXT("asset_data_field_inventory"));
            for(const auto& F:V.Fields)
            {
                const FProperty* PField=F.Field;
                if(PField->GetOwnerStruct()!=S||PField->GetClass()!=F.Class||PField->GetFName()!=F.Name
                    ||uint64(PField->GetPropertyFlags())!=F.Flags||PField->GetOffset_ForInternal()!=F.Offset||PField->GetSize()!=F.Size
                    ||PField->ArrayDim!=F.ArrayDim||PField->HasSetterOrGetter())return Bad(TEXT("asset_data_field_current"));
                if(F.HasStruct){const auto* SP=static_cast<const FStructProperty*>(PField);if(!SP->Struct.IsResolved()||SP->Struct!=F.Struct)return Bad(TEXT("asset_data_field_struct"));}
            }
        }
        return true;
    }
    bool PrepareAssetData()
    {
        UScriptStruct* Asset=FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.AssetData"));
        if(!Live(Asset))return Bad(TEXT("asset_data_loaded_source"));TStrongObjectPtr<UScriptStruct> HeldAsset(Asset);
        if(!Basic()||Asset!=FAssetData::StaticStruct())return Bad(TEXT("asset_data_native_identity"));
        UScriptStruct* Top=FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.TopLevelAssetPath"));
        if(!Live(Top))return Bad(TEXT("asset_data_child_loaded_source"));TStrongObjectPtr<UScriptStruct> HeldTop(Top);
        if(!Basic()||Top!=FTopLevelAssetPath::StaticStruct())return Bad(TEXT("asset_data_child_native_identity"));
        for(UScriptStruct* S:{Asset,Top})
        {
            const bool IsAsset=S==Asset;const int32 ExpectedSize=IsAsset?sizeof(FAssetData):sizeof(FTopLevelAssetPath);
            const int32 ExpectedAlignment=IsAsset?alignof(FAssetData):alignof(FTopLevelAssetPath);UPackage* P=S->GetPackage();
            if(!Live(P)||S->GetClass()!=UScriptStruct::StaticClass()||S->GetOuter()!=P||!P->GetName().Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)
                ||P->GetPackageFlags()!=16U||P->IsDirty()||S->IsEditorOnly()||!(S->StructFlags&STRUCT_Native)
                ||S->GetStructureSize()!=ExpectedSize||S->GetMinAlignment()!=ExpectedAlignment||!Hold(S,true)||!Hold(P,true))return Bad(TEXT("asset_data_native_layout"));
            auto Super=S->GetSuperStructIterator();++Super;if(Super)return Bad(TEXT("asset_data_native_super"));
            FAssetStructRole V;V.Hold.Reset(S);V.Package.Reset(P);V.Path=S->GetPathName();V.Flags=S->GetFlags();V.StructFlags=uint32(S->StructFlags);
            V.PackageFlags=P->GetPackageFlags();V.Size=S->GetStructureSize();V.Alignment=S->GetMinAlignment();
            for(TFieldIterator<FProperty> It(S,EFieldIteratorFlags::ExcludeSuper);It;++It)V.Inventory.Add(*It);
            const TCHAR* AssetNames[]={TEXT("ObjectPath"),TEXT("PackageName"),TEXT("PackagePath"),TEXT("AssetName"),TEXT("AssetClass"),TEXT("AssetClassPath")};
            const TCHAR* TopNames[]={TEXT("PackageName"),TEXT("AssetName")};
PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const int32 Offsets[]={STRUCT_OFFSET(FAssetData,ObjectPath),STRUCT_OFFSET(FAssetData,PackageName),STRUCT_OFFSET(FAssetData,PackagePath),STRUCT_OFFSET(FAssetData,AssetName),STRUCT_OFFSET(FAssetData,AssetClass),STRUCT_OFFSET(FAssetData,AssetClassPath)};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(V.Inventory.Num()!=(IsAsset?6:2))return Bad(TEXT("asset_data_native_field_count"));
            for(int32 I=0;I<V.Inventory.Num();++I)
            {
                const FProperty* F=V.Inventory[I];const bool StructField=IsAsset&&I==5;const int32 Size=StructField?sizeof(FTopLevelAssetPath):sizeof(FName);
                if(F->GetOwnerStruct()!=S||F->GetFName()!=FName(IsAsset?AssetNames[I]:TopNames[I])||F->ArrayDim!=1
                    ||F->GetClass()!=(StructField?FStructProperty::StaticClass():FNameProperty::StaticClass())||F->HasSetterOrGetter()
                    ||F->GetSize()!=Size||F->GetOffset_ForInternal()<0||F->GetOffset_ForInternal()+Size>ExpectedSize
                    ||(IsAsset&&F->GetOffset_ForInternal()!=Offsets[I]))return Bad(TEXT("asset_data_native_field_layout"));
                FAssetFieldRole Field;Field.Field=F;Field.Class=F->GetClass();Field.Name=F->GetFName();Field.Flags=uint64(F->GetPropertyFlags());
                Field.Offset=F->GetOffset_ForInternal();Field.Size=F->GetSize();Field.ArrayDim=F->ArrayDim;Field.HasStruct=StructField;
                if(StructField){Field.Struct=static_cast<const FStructProperty*>(F)->Struct;if(!Field.Struct.IsResolved()||Field.Struct!=Top)return Bad(TEXT("asset_data_child_binding"));}
                V.Fields.Add(MoveTemp(Field));
            }
            AssetStructs.Add(MoveTemp(V));
        }
        C.ElementPinType.PinSubCategoryObject=Asset;
        R.Data->SetBoolField(TEXT("native_asset_data_for_each"),true);R.Data->SetBoolField(TEXT("specialization_pending"),true);
        R.Data->SetBoolField(TEXT("producer_connection_required"),true);R.Data->SetBoolField(TEXT("producer_connection_verified"),false);
        R.Data->SetBoolField(TEXT("runtime_asset_resolution_attempted"),false);R.Data->SetBoolField(TEXT("runtime_values_sampled"),false);
        return AssetDataCurrent();
    }
    void AssetDataState(const TSharedRef<FJsonObject>& J)const
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& V:AssetStructs)
        {
            auto S=MakeShared<FJsonObject>();S->SetStringField(TEXT("path"),V.Path);S->SetStringField(TEXT("identity"),LexToString(reinterpret_cast<UPTRINT>(V.Hold.Get())));
            S->SetStringField(TEXT("flags_string"),LexToString(uint32(V.Flags)));S->SetStringField(TEXT("struct_flags_string"),LexToString(V.StructFlags));
            S->SetStringField(TEXT("package_flags_string"),LexToString(V.PackageFlags));S->SetNumberField(TEXT("size"),V.Size);S->SetNumberField(TEXT("alignment"),V.Alignment);
            TArray<TSharedPtr<FJsonValue>> Fields;for(const auto& F:V.Fields){auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),F.Name.ToString());P->SetStringField(TEXT("identity"),LexToString(reinterpret_cast<UPTRINT>(F.Field)));P->SetStringField(TEXT("field_class"),F.Class->GetName());P->SetStringField(TEXT("flags_string"),LexToString(F.Flags));P->SetNumberField(TEXT("offset"),F.Offset);P->SetNumberField(TEXT("size"),F.Size);P->SetNumberField(TEXT("array_dim"),F.ArrayDim);P->SetStringField(TEXT("struct_path"),F.HasStruct?TEXT("/Script/CoreUObject.TopLevelAssetPath"):TEXT(""));Fields.Add(MakeShared<FJsonValueObject>(P));}S->SetArrayField(TEXT("fields"),Fields);Rows.Add(MakeShared<FJsonValueObject>(S));
        }
        J->SetArrayField(TEXT("native_structs"),Rows);J->SetBoolField(TEXT("runtime_values_sampled"),false);J->SetBoolField(TEXT("struct_value_buffer_created"),false);
    }
    bool PrepareType()
    {
        if(!Safe()||!Live(GEditor)||!Live(GWorld))return Bad(TEXT("initial_editor_state"));Editor.Reset(GEditor);World.Reset(GWorld);Undo=GUndo;if(!Basic())return Bad(TEXT("initial_context"));
        if(C.bSkeletalForEach)
        {
            UClass* K=FindObject<UClass>(nullptr,TEXT("/Script/Engine.SkeletalMeshComponent"));if(!Live(K)||K!=USkeletalMeshComponent::StaticClass())return Bad(TEXT("exact_native_type"));Element.Reset(K);C.ElementPinType.PinSubCategoryObject=K;
            const TCHAR* Names[]={TEXT("/Script/Engine.SkeletalMeshComponent"),TEXT("/Script/Engine.SkinnedMeshComponent"),TEXT("/Script/Engine.MeshComponent"),TEXT("/Script/Engine.PrimitiveComponent"),TEXT("/Script/Engine.SceneComponent"),TEXT("/Script/Engine.ActorComponent"),TEXT("/Script/CoreUObject.Object")};
            int32 Index=0;for(auto It=K->GetSuperStructIterator();It;++It,++Index){UClass* T=const_cast<UClass*>(Cast<UClass>(*It));if(Index>=UE_ARRAY_COUNT(Names)||!Live(T)||!T->GetPathName().Equals(Names[Index],ESearchCase::CaseSensitive)||!T->ClassGeneratedBy.IsResolved()||T->ClassGeneratedBy!=nullptr||!T->HasAnyClassFlags(CLASS_Native)||T->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||T->IsEditorOnly()||!Live(T->GetPackage())||T->GetPackage()->GetName()!=(Index==6?TEXT("/Script/CoreUObject"):TEXT("/Script/Engine"))||T->GetPackage()->GetPackageFlags()!=16U||T->GetPackage()->IsDirty()||!Hold(T,true)||!Hold(T->GetPackage(),true))return Bad(TEXT("native_type_hierarchy"));Hierarchy.Emplace(T);}if(Index!=UE_ARRAY_COUNT(Names))return Bad(TEXT("native_type_hierarchy_count"));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto D=K->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
            UObject* O=FindObject<UObject>(nullptr,TEXT("/Script/Engine.Default__SkeletalMeshComponent"));if(!D.IsResolved()||D!=O||!Live(O)||O->GetClass()!=K||O->GetOuter()!=K->GetPackage()||!O->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!Hold(O,true))return Bad(TEXT("existing_native_type_cdo"));ElementCDO.Reset(O);
        }
        if(C.bAssetDataForEach&&!PrepareAssetData())return false;
        R.Data->SetBoolField(TEXT("finite_native_for_each"),true);R.Data->SetStringField(TEXT("finite_for_each_kind"),C.bAssetDataForEach?TEXT("asset_data"):C.bSkeletalForEach?TEXT("skeletal_component"):TEXT("name"));R.Data->SetBoolField(TEXT("runtime_iteration_verified"),false);return Check();
    }
    TSharedRef<FJsonObject> TypeState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("element_type"),C.ElementType);J->SetStringField(TEXT("type_path"),C.bAssetDataForEach?TEXT("/Script/CoreUObject.AssetData"):C.bSkeletalForEach?TEXT("/Script/Engine.SkeletalMeshComponent"):TEXT(""));J->SetBoolField(TEXT("cdo_values_sampled"),false);J->SetBoolField(TEXT("cdo_creation_requested"),false);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& H:Hierarchy){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),H->GetPathName());V->SetStringField(TEXT("class_flags_string"),LexToString(uint32(H->GetClassFlags())));V->SetNumberField(TEXT("class_size"),H->GetStructureSize());Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("native_hierarchy"),Rows);if(C.bAssetDataForEach)AssetDataState(J);return J;
    }
    static bool SamePresetLocals(const TArray<FBPVariableDescription>& A,const TArray<FBPVariableDescription>& B)
    {
        if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I){const auto& X=A[I];const auto& Y=B[I];if(X.VarName!=Y.VarName||X.VarGuid!=Y.VarGuid||X.VarType!=Y.VarType||X.VarType.bIsUObjectWrapper!=Y.VarType.bIsUObjectWrapper||X.VarType.bSerializeAsSinglePrecisionFloat!=Y.VarType.bSerializeAsSinglePrecisionFloat||X.VarType.PinValueType.bTerminalIsUObjectWrapper!=Y.VarType.PinValueType.bTerminalIsUObjectWrapper||!X.FriendlyName.Equals(Y.FriendlyName,ESearchCase::CaseSensitive)||!X.Category.IdenticalTo(Y.Category)||X.PropertyFlags!=Y.PropertyFlags||X.RepNotifyFunc!=Y.RepNotifyFunc||X.ReplicationCondition!=Y.ReplicationCondition||!X.DefaultValue.Equals(Y.DefaultValue,ESearchCase::CaseSensitive)||X.MetaDataArray.Num()!=Y.MetaDataArray.Num())return false;for(int32 K=0;K<X.MetaDataArray.Num();++K)if(X.MetaDataArray[K].DataKey!=Y.MetaDataArray[K].DataKey||!X.MetaDataArray[K].DataValue.Equals(Y.MetaDataArray[K].DataValue,ESearchCase::CaseSensitive))return false;}return true;
    }
    bool PresetContext()const
    {
        if(!C.bPresetLibraryForEach)return true;auto* BP=C.Blueprint;auto* G=C.Graph;
        if(!C.bForEach||!C.bFiniteForEach||C.bSkeletalForEach||(!C.bAssetDataForEach&&C.ElementType!=TEXT("name"))||!Live(BP)||!Live(G)||BP->BlueprintType!=BPTYPE_FunctionLibrary||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=UBlueprintFunctionLibrary::StaticClass()||!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript!=nullptr||!BP->NewVariables.IsEmpty()||!BP->GeneratedVariables.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()||!BP->CachedUDSDependencies.IsEmpty()||G->GetFName()!=TEXT("PresetLoader")||G->GetOuter()!=BP||!BP->FunctionGraphs.Contains(G))return false;
        if(!PresetEntryBound)return true;auto* E=PresetEntry.Get();if(!Live(E)||E->GetClass()!=UK2Node_FunctionEntry::StaticClass()||E->GetOuter()!=G||!G->Nodes.Contains(E)||E->NodeGuid!=PresetEntryGuid||!E->FunctionReference.IsSameReference(PresetEntryReference)||E->FunctionReference.IsDeprecated()||!E->CustomGeneratedFunctionName.IsNone()||E->GetExtraFlags()!=PresetEntryFlags||!(PresetEntryFlags&FUNC_Static)||!SamePresetLocals(E->LocalVariables,PresetLocals))return false;
        int32 WorldPins=0;for(UEdGraphPin* P:E->Pins)if(P&&P->PinName==TEXT("__WorldContext")){FEdGraphPinType ExpectedWorld;ExpectedWorld.PinCategory=UEdGraphSchema_K2::PC_Object;ExpectedWorld.PinSubCategoryObject=UObject::StaticClass();if(P->WasTrashed()||P->GetOwningNodeUnchecked()!=E||!P->PinId.IsValid()||P->Direction!=EGPD_Output||P->PinType!=ExpectedWorld||P->PinType.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat||!P->bHidden||P->bDeprecated||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->DefaultObject.IsResolved()||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return false;++WorldPins;}return WorldPins==1;
    }
    bool BindPresetEntry()
    {
        if(!C.bPresetLibraryForEach)return true;if(!PresetContext())return Bad(TEXT("preset_loader_initial_context"));
        const auto Inventory=C.Graph->Nodes;for(const auto& H:Inventory){if(!H.IsResolved())return Bad(TEXT("preset_loader_entry_inventory"));UEdGraphNode* N=H.Get();if(!Check()||C.Graph->Nodes!=Inventory)return Bad(TEXT("preset_loader_entry_inventory_changed"));if(N&&N->GetClass()==UK2Node_FunctionEntry::StaticClass()){if(PresetEntry.IsValid()||!Hold(N,false))return Bad(TEXT("preset_loader_entry_ambiguous"));PresetEntry.Reset(static_cast<UK2Node_FunctionEntry*>(N));}}
        if(!PresetEntry.IsValid())return Bad(TEXT("preset_loader_entry_missing"));PresetEntryReference=PresetEntry->FunctionReference;PresetEntryGuid=PresetEntry->NodeGuid;PresetEntryFlags=PresetEntry->GetExtraFlags();PresetLocals=PresetEntry->LocalVariables;
        FMemberReference OwnReference;OwnReference.SetExternalMember(C.Graph->GetFName(),nullptr,PresetEntryReference.GetMemberGuid());if(!PresetEntryReference.IsSameReference(OwnReference)||PresetEntryReference.IsSelfContext()||PresetEntryReference.IsLocalScope()||PresetEntryReference.IsDeprecated()||!PresetEntryGuid.IsValid())return Bad(TEXT("preset_loader_entry_binding"));PresetEntryBound=true;
        if(!PresetContext())return Bad(TEXT("preset_loader_entry_current"));R.Data->SetStringField(TEXT("function_library_profile"),TEXT("preset_loader"));R.Data->SetStringField(TEXT("target_context"),TEXT("preset_loader_library"));R.Data->SetStringField(TEXT("function_entry_guid"),PresetEntryGuid.ToString(EGuidFormats::Digits));R.Data->SetStringField(TEXT("function_entry_flags_string"),LexToString(PresetEntryFlags));R.Data->SetBoolField(TEXT("automatic_entry_world_context_current_verified"),true);R.Data->SetBoolField(TEXT("authored_world_context_edge_assumed"),false);R.Data->SetBoolField(TEXT("producer_connection_verified"),false);R.Data->SetBoolField(TEXT("producer_specialization_pending"),true);return Check();
    }
    bool BindTarget()
    {
        if(!Basic()||!Live(C.Blueprint)||!Live(C.Graph)||!Live(C.Schema)||!C.Blueprint->ParentClass.GetGCPtr().IsResolved()||!C.Blueprint->GeneratedClass.GetGCPtr().IsResolved()||!C.Blueprint->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("bind_target"));
        const auto ResolveClass=[](const TObjectPtr<UClass>& H)->UClass*{UClass* K=FindObject<UClass>(nullptr,*H.GetPathName());return H.IsResolved()&&H==K?K:nullptr;};Parent=ResolveClass(C.Blueprint->ParentClass.GetGCPtr());Generated=ResolveClass(C.Blueprint->GeneratedClass.GetGCPtr());Skeleton=ResolveClass(C.Blueprint->SkeletonGeneratedClass.GetGCPtr());
        if(!Hold(Parent,true)||!Hold(Parent->GetPackage(),true)||!Hold(const_cast<UEdGraphSchema_K2*>(C.Schema),true))return false;
        for(UObject* O:{static_cast<UObject*>(C.Blueprint),static_cast<UObject*>(C.Graph),static_cast<UObject*>(const_cast<UEdGraphSchema_K2*>(C.Schema)),static_cast<UObject*>(Parent),static_cast<UObject*>(Generated),static_cast<UObject*>(Skeleton)})if(!Hold(O,false))return false;
        if(C.bPresetLibraryForEach&&!PresetContext())return Bad(TEXT("preset_loader_target_context"));
        OldTargetCount=C.Graph->Nodes.Num();TArray<UEdGraph*> All;C.Blueprint->GetAllGraphs(All);if(!Check()||All.Num()>256||!All.Contains(C.Graph))return Bad(TEXT("bind_target_graphs"));for(UEdGraph* G:All)if(!KeepGraph(G,false))return false;if(!BindPresetEntry())return false;return Check();
    }
    bool PrepareStockRoles()
    {
        if(!Check())return false;Stock=FindObject<UBlueprint>(nullptr,StockBlueprintPath);UClass* Tunnel=FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_Tunnel"));UClass* Macro=FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_MacroInstance"));
        if(!Live(Stock)||!Live(Tunnel)||!Live(Macro)||Stock->MacroGraphs.Num()>256||!Stock->ParentClass.GetGCPtr().IsResolved()||!Stock->GeneratedClass.GetGCPtr().IsResolved()||!Stock->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("stock_initial_roles"));
        const auto StockClass=[](const TObjectPtr<UClass>& H)->UClass*{if(!H.IsResolved()||H==nullptr)return nullptr;UClass* K=FindObject<UClass>(nullptr,*H.GetPathName());return H==K?K:nullptr;};
        StockParent=StockClass(Stock->ParentClass.GetGCPtr());StockGenerated=StockClass(Stock->GeneratedClass.GetGCPtr());StockSkeleton=StockClass(Stock->SkeletonGeneratedClass.GetGCPtr());UClass* StockRoot=StockParent;if(!Live(StockRoot)||uint32(StockRoot->GetClassFlags())!=810027169u)return Bad(TEXT("stock_root_class"));
        for(UObject* O:{static_cast<UObject*>(Stock),static_cast<UObject*>(Stock->GetPackage()),static_cast<UObject*>(Tunnel),static_cast<UObject*>(Macro),static_cast<UObject*>(StockClass(Stock->ParentClass.GetGCPtr())),static_cast<UObject*>(StockClass(Stock->GeneratedClass.GetGCPtr())),static_cast<UObject*>(StockClass(Stock->SkeletonGeneratedClass.GetGCPtr()))})if(!Hold(O,true))return false;
        for(const auto& H:Stock->MacroGraphs){if(!H.IsResolved())return Bad(TEXT("stock_graph_unresolved"));UEdGraph* G=FindObject<UEdGraph>(nullptr,*H.GetPathName());if(H!=G||!Live(G)||G->GetOuter()!=Stock||!Hold(G,true))return Bad(TEXT("stock_graph_initial_role"));StockGraphs.Add(G);if(G->GetName()==TEXT("ForEachLoop")){if(StockEach)return Bad(TEXT("stock_each_ambiguous"));StockEach=G;}if(G->GetName()==TEXT("ForEachLoop")||G->GetName()==TEXT("ForLoop"))if(!KeepGraph(G,true))return false;}
        return StockEach&&Check()?true:Bad(TEXT("stock_each_missing"));
    }
    bool BindStock()
    {
        return Check()&&C.MacroBlueprint==Stock&&C.MacroGraph==StockEach&&Live(C.TunnelClass)&&Live(C.MacroClass)?true:Bad(TEXT("bind_stock"));
    }
    bool Check()
    {
        if(!Basic())return Bad(TEXT("context"));
        for(const auto& V:Roles){const UObject* O=V.Hold.Get();if(!Live(O)||O->GetClass()!=V.Class||O->GetOuter()!=V.Outer||O->GetPackage()!=V.Package||!O->GetPathName().Equals(V.Path,ESearchCase::CaseSensitive)||(V.Frozen&&O->GetFlags()!=V.Flags))return Bad(TEXT("held_identity"));if(const UEdGraph* G=Cast<UEdGraph>(O)){auto S=G->Schema;if(G->GraphGuid!=V.GraphGuid||!S.GetGCPtr().IsResolved()||S.GetGCPtr()!=V.GraphSchema)return Bad(TEXT("held_graph_guid_schema"));}if(const UClass* K=Cast<UClass>(O))if(uint32(K->GetClassFlags())!=V.ClassFlags||K->GetStructureSize()!=V.Size)return Bad(TEXT("held_class_layout"));if(V.Frozen)if(const UPackage* P=Cast<UPackage>(O))if(P->GetPackageFlags()!=V.PackageFlags||P->IsDirty()!=V.WasDirty)return Bad(TEXT("held_source_package"));}
        if(C.bSkeletalForEach&&Element.IsValid())
        {
            int32 Index=0;for(auto It=Element->GetSuperStructIterator();It;++It,++Index)if(Index>=Hierarchy.Num()||*It!=Hierarchy[Index].Get())return Bad(TEXT("current_type_hierarchy"));if(Index!=Hierarchy.Num())return Bad(TEXT("current_type_hierarchy_count"));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto D=Element->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!D.IsResolved()||D!=ElementCDO.Get())return Bad(TEXT("current_native_cdo"));
        }
        if(C.bAssetDataForEach&&!AssetDataCurrent())return false;
        if(Parent){if(!Live(C.Blueprint)||!Live(C.Graph)||!C.Blueprint->ParentClass.GetGCPtr().IsResolved()||C.Blueprint->ParentClass.GetGCPtr()!=Parent||!C.Blueprint->GeneratedClass.GetGCPtr().IsResolved()||C.Blueprint->GeneratedClass.GetGCPtr()!=Generated||!C.Blueprint->SkeletonGeneratedClass.GetGCPtr().IsResolved()||C.Blueprint->SkeletonGeneratedClass.GetGCPtr()!=Skeleton||C.Blueprint->bBeingCompiled||C.Blueprint->bQueuedForCompilation||C.Blueprint->bIsRegeneratingOnLoad||C.Blueprint->bSuppressStructurallyModified||C.Graph->GetOuter()!=C.Blueprint||C.Graph->Nodes.Num()!=OldTargetCount+(Attached?1:0))return Bad(TEXT("current_target"));bool Found=false;for(const auto* List:{&C.Blueprint->FunctionGraphs,&C.Blueprint->UbergraphPages,&C.Blueprint->MacroGraphs})for(const auto& H:*List){if(!H.IsResolved())return Bad(TEXT("unresolved_graph_inventory"));Found|=H==C.Graph;}if(!Found)return Bad(TEXT("current_graph_membership"));}
        if(Parent&&C.bPresetLibraryForEach&&!PresetContext())return Bad(TEXT("preset_loader_current_context"));
        if(Stock){if(!Live(Stock)||!Stock->ParentClass.GetGCPtr().IsResolved()||Stock->ParentClass.GetGCPtr()!=StockParent||!Stock->GeneratedClass.GetGCPtr().IsResolved()||Stock->GeneratedClass.GetGCPtr()!=StockGenerated||!Stock->SkeletonGeneratedClass.GetGCPtr().IsResolved()||Stock->SkeletonGeneratedClass.GetGCPtr()!=StockSkeleton||Stock->MacroGraphs.Num()!=StockGraphs.Num())return Bad(TEXT("stock_graph_inventory"));for(int32 I=0;I<StockGraphs.Num();++I)if(!Stock->MacroGraphs[I].IsResolved()||Stock->MacroGraphs[I]!=StockGraphs[I])return Bad(TEXT("stock_graph_role"));}
        for(const auto& Pair:GraphCounts)if(!Live(Pair.Key)||Pair.Key->Nodes.Num()!=Pair.Value+((Pair.Key==C.Graph&&Attached)?1:0))return Bad(TEXT("current_graph_node_count"));
        for(const auto& V:Prior){const UEdGraphNode* N=V.Hold.Get();bool Found=false;for(const auto& H:V.Graph->Nodes){if(!H.IsResolved())return Bad(TEXT("current_prior_node_handle"));Found|=H==N;}if(!Found||!Live(N)||N->GetOuter()!=V.Graph||N->NodeGuid!=V.Guid||N->Pins.Num()!=V.Pins.Num())return Bad(TEXT("current_prior_node"));if(V.Call){const auto* Call=Cast<UK2Node_CallFunction>(N);if(!Call||!Call->FunctionReference.IsSameReference(V.Member)||Call->FunctionReference.IsDeprecated()!=V.Member.IsDeprecated())return Bad(TEXT("current_prior_function_reference"));}if(V.Tunnel){const auto* T=Cast<UK2Node_Tunnel>(N);if(!T||!T->InputSinkNode.IsResolved()||!T->OutputSourceNode.IsResolved()||T->InputSinkNode!=V.InputSink||T->OutputSourceNode!=V.OutputSource)return Bad(TEXT("current_tunnel_reference"));}for(int32 Index=0;Index<V.Pins.Num();++Index)if(!PinCurrent(V.Pins[Index],N->Pins[Index],N))return Bad(TEXT("current_prior_pin"));}
        if(Selected.IsValid()){auto* N=Selected.Get();if(!Live(N)||N->GetClass()!=C.MacroClass||N->GetOuter()!=C.Graph||N->GetPathName()!=NodePath||N->GetFlags()!=NodeFlags||((C.Graph->Nodes.Contains(N))!=Attached)||(GuidFixed&&(N->NodeGuid!=NodeGuid||N->NodePosX!=C.X||N->NodePosY!=C.Y)))return Bad(TEXT("current_created_node"));if(PinsFixed){if(N->Pins.Num()!=Pins.Num())return Bad(TEXT("current_created_pin_count"));for(int32 I=0;I<Pins.Num();++I)if(!PinCurrent(Pins[I],N->Pins[I],N)||N->Pins[I]->ParentPin||!N->Pins[I]->SubPins.IsEmpty()||!N->Pins[I]->LinkedTo.IsEmpty())return Bad(TEXT("current_created_pin"));FEdGraphPinType Empty;if(N->ResolvedWildcardType!=(Typed?C.ElementPinType:Empty))return Bad(TEXT("current_wildcard_type"));}}
        return true;
    }
    bool Adopt(UK2Node_MacroInstance* N)
    {
        R.Data->SetBoolField(TEXT("loop_node_allocation_returned_nonnull"),N!=nullptr);
        if(IsEditorGameThread()&&!IsGarbageCollecting()&&!GExitPurge&&Live(N)&&Live(C.Graph)&&N->GetClass()==C.MacroClass&&N->GetOuter()==C.Graph)
        {Selected.Reset(N);NodePath=N->GetPathName();NodeFlags=N->GetFlags();R.Data->SetStringField(TEXT("loop_allocated_node_path"),NodePath);R.bChanged=true;R.Data->SetBoolField(TEXT("loop_returned_identity_observed"),true);}
        else R.Data->SetBoolField(TEXT("loop_returned_identity_observed"),false);
        return Selected.IsValid()&&Check()?true:Bad(TEXT("allocation_return"));
    }
    bool Reference()
    {if(!Check())return false;UEdGraph* G=Selected->GetMacroGraph();if(!Check())return false;UBlueprint* B=Selected->GetSourceBlueprint();if(!Check())return false;return G==C.MacroGraph&&B==C.MacroBlueprint?true:Bad(TEXT("current_macro_reference"));}
    bool Freeze()
    {
        if(!Check()||!Reference()||Selected->Pins.Num()!=6)return Bad(TEXT("freeze_pins"));TSet<FGuid> Ids;
        for(UEdGraphPin* P:Selected->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Selected.Get()||!P->PinId.IsValid()||Ids.Contains(P->PinId)||!P->DefaultObject.IsResolved())return Bad(TEXT("freeze_physical_pin"));Ids.Add(P->PinId);Pins.Add(PinRole(P));}PinsFixed=true;return Check();
    }
    bool Shape(const UK2Node_MacroInstance* N,bool bTyped)
    {
        if(!Check()||N!=Selected.Get()||!Reference()||!GuidFixed||N->Pins.Num()!=6)return false;FEdGraphPinType Empty;if(N->ResolvedWildcardType!=(bTyped?C.ElementPinType:Empty))return false;
        TSet<FGuid> Ids;for(int32 I=0;I<6;++I){const UEdGraphPin* P=N->Pins[I];const auto Type=EachPortType(I,bTyped?&C:nullptr);if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->PinName!=FName(EachPortNames[I])||P->Direction!=(I<2?EGPD_Input:EGPD_Output)||P->PinType!=Type||P->PinType.bIsUObjectWrapper!=Type.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=Type.bSerializeAsSinglePrecisionFloat||P->PinType.PinValueType.bTerminalIsUObjectWrapper!=Type.PinValueType.bTerminalIsUObjectWrapper||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()||P->bHidden||P->bNotConnectable||P->bDefaultValueIsReadOnly||P->bDefaultValueIsIgnored||P->bOrphanedPin||!P->DefaultObject.IsResolved()||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return false;Ids.Add(P->PinId);}return Check();
    }
};
void Finish(FContext& C, FResult& R, UK2Node_MacroInstance* Node, bool Good)
{
    if (!R.ErrorCode.IsEmpty()) { R.Data->SetStringField(TEXT("failed_mutation_stage"), R.Stage); }
    R.Stage = TEXT("verification"); auto After = MakeShared<FJsonObject>();
    const bool TargetKnown = Capture(C, After);
    if(C.FiniteCheckpoint&&!C.FiniteCheckpoint()){R.Data->SetObjectField(TEXT("after"),After);return;} R.Data->SetObjectField(TEXT("after"), After);
    const auto SourceAfter = Observe(C.MacroBlueprint, C.Schema, C.TunnelClass,C.FiniteCheckpoint?&C.FiniteCheckpoint:nullptr);if(C.FiniteCheckpoint&&!C.FiniteCheckpoint()){R.Data->SetObjectField(TEXT("macro_source_after"),SourceAfter);return;} auto CallsAfter = MakeShared<FJsonObject>();
    const bool SourceKnown = SourceAfter->GetBoolField(TEXT("observations_complete")) && SourceCalls(C, CallsAfter);if(C.FiniteCheckpoint&&!C.FiniteCheckpoint()){R.Data->SetObjectField(TEXT("macro_source_after"),SourceAfter);R.Data->SetObjectField(TEXT("source_native_calls_after"),CallsAfter);return;}
    R.Data->SetObjectField(TEXT("macro_source_after"), SourceAfter); R.Data->SetObjectField(TEXT("source_native_calls_after"), CallsAfter);
    R.bChangeStateKnown = TargetKnown && SourceKnown;
    R.bChanged = (C.GraphCreationCurrent&&R.bChanged) || (TargetKnown && Text(C.Before) != Text(After)) || (SourceKnown && (Text(C.MacroBefore) != Text(SourceAfter) || Text(C.CallsBefore) != Text(CallsAfter)));
    const bool Prior = TargetKnown && PriorPreserved(C, After, Node);
    const bool SourcePreserved = SourceKnown && Text(C.MacroBefore) == Text(SourceAfter) && Text(C.CallsBefore) == Text(CallsAfter);
    bool Declarations = false;
    if (TargetKnown)
    {
        auto Expected = MakeShared<FJsonObject>(); auto Actual = MakeShared<FJsonObject>();
        for (const TCHAR* Key : {TEXT("variables"), TEXT("interfaces")})
        { Expected->SetArrayField(Key, C.Before->GetArrayField(Key)); Actual->SetArrayField(Key, After->GetArrayField(Key)); }
        Declarations = Text(Expected) == Text(Actual);
    }
    R.Data->SetBoolField(TEXT("existing_nodes_preserved"), Prior); R.Data->SetBoolField(TEXT("declarations_preserved"), Declarations);
    R.Data->SetBoolField(TEXT("macro_source_observed_unchanged"), SourcePreserved);
    R.Data->SetBoolField(TEXT("created"), IsValid(Node) && C.Graph->Nodes.Contains(Node));
    if (IsValid(Node))
    {
        auto State = NodeState(Node,(C.bFiniteForEach||C.GraphCreationCurrent)?&C:nullptr);if(C.FiniteCheckpoint&&!C.FiniteCheckpoint()){R.Data->SetObjectField(TEXT("node"),State);return;}const FText Title=Node->GetNodeTitle(ENodeTitleType::FullTitle);if(C.FiniteCheckpoint&&!C.FiniteCheckpoint()){R.Data->SetObjectField(TEXT("node"),State);return;}const FString TitleDisplay=Title.ToString();if(C.FiniteCheckpoint&&!C.FiniteCheckpoint()){R.Data->SetObjectField(TEXT("node"),State);return;}State->SetStringField(TEXT("title"),TitleDisplay);R.Data->SetObjectField(TEXT("node"), State);
        for (const TCHAR* Key : {TEXT("node_id"), TEXT("node_guid"), TEXT("object_path"), TEXT("node_class")}) { R.Data->SetStringField(Key, State->GetStringField(Key)); }
        R.Data->SetArrayField(TEXT("pins"), State->GetArrayField(TEXT("pins"))); R.Data->SetObjectField(TEXT("graph"), GraphDescription(C));
    }
    const bool DefaultsVerified = DefaultsMatch(C, Node); R.Data->SetBoolField(TEXT("requested_defaults_verified"), DefaultsVerified);
    const bool Shape = NodeShape(C, Node); R.Data->SetBoolField(TEXT("typed_node_shape_verified"), Shape);if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return;
    const bool Verified = Good && R.bChangeStateKnown && Prior && Declarations && SourcePreserved && DefaultsVerified && Shape;
    R.Data->SetBoolField(TEXT("node_only_verified"), Verified);
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("bounded owned graphs/full pin/default/link identities, variable/interface declarations and dirty/status; selected stock source graphs and loaded native math metadata/packages. Arbitrary Editor callbacks and disk writes are not fully observed."));
    R.Data->SetStringField(TEXT("partial_failure_policy"), TEXT("retain created node/defaults and report known or unknown observations; no deletion, rollback, compile, save or automatic retry"));
    if (!Verified)
    { if (R.ErrorCode.IsEmpty()) { Error(R, TEXT("loop_node_postcondition_failed"), TEXT("Created ForLoop shape/defaults or prior/source preservation failed; retained partial state must be inspected before any continuation.")); } return; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete");
}
FResult AddFiniteForEach(FContext& C,FResult& R,const FString& BlueprintPath,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    FFiniteForEachScope Scope(C,R);if(!Scope.PrepareType())return R;
    const bool TargetResolved=ResolveTarget(C,R,BlueprintPath,GraphName,GraphId,GraphType);if(!Scope.Check()||!TargetResolved||!Scope.BindTarget())return R;
    if(!Scope.PrepareStockRoles())return R;C.FiniteCheckpoint=[&](){return Scope.Check();};
    const bool StockResolved=MacroSource(C,R);if(!Scope.Check()||!StockResolved||!Scope.BindStock())return R;
    C.FiniteShape=[&](const UK2Node_MacroInstance* N,bool Typed){return Scope.Shape(N,Typed);};
    R.Data->SetObjectField(TEXT("finite_for_each_type_source_before"),Scope.TypeState());R.Data->SetObjectField(TEXT("element_pin_type"),Function::Pin::Describe(C.ElementPinType));if(!Scope.Check())return R;
    if(!Capture(C,C.Before)||!Scope.Check()){if(!Scope.Stopped)Error(R,TEXT("loop_before_state_unobservable"),TEXT("Finite ForEach before observation is incomplete; no writer dispatched."));return R;}R.Data->SetObjectField(TEXT("before"),C.Before);
    R.Stage=TEXT("create_loop_node");R.bMutationAttempted=true;R.Data->SetBoolField(TEXT("finite_for_each_modify_blueprint_attempted"),true);const bool Modified=C.Blueprint->Modify();R.Data->SetBoolField(TEXT("finite_for_each_modify_blueprint_returned"),Modified);if(!Scope.Check())return R;
    R.Data->SetBoolField(TEXT("finite_for_each_modify_graph_attempted"),true);const bool GraphModified=C.Graph->Modify();R.Data->SetBoolField(TEXT("finite_for_each_modify_graph_returned"),GraphModified);if(!Scope.Check())return R;
    R.Data->SetBoolField(TEXT("finite_for_each_new_object_attempted"),true);UK2Node_MacroInstance* N=NewObject<UK2Node_MacroInstance>(C.Graph,C.MacroClass,NAME_None,RF_Transactional);if(!Scope.Adopt(N))return R;
    N->SetMacroGraph(C.MacroGraph);Scope.Bound=true;R.Data->SetBoolField(TEXT("finite_for_each_set_macro_returned"),true);if(!Scope.Reference())return R;
    N->NodePosX=C.X;N->NodePosY=C.Y;C.Graph->AddNode(N,false,false);Scope.Attached=true;R.Data->SetBoolField(TEXT("finite_for_each_add_node_returned"),true);if(!Scope.Reference())return R;
    N->CreateNewGuid();Scope.NodeGuid=N->NodeGuid;Scope.GuidFixed=true;R.Data->SetStringField(TEXT("loop_created_node_guid"),Scope.NodeGuid.ToString(EGuidFormats::Digits));if(!Scope.NodeGuid.IsValid()){Scope.Bad(TEXT("created_guid"));return R;}if(!Scope.Reference())return R;
    N->PostPlacedNewNode();R.Data->SetBoolField(TEXT("finite_for_each_post_placed_returned"),true);if(!Scope.Reference())return R;
    N->AllocateDefaultPins();R.Data->SetBoolField(TEXT("finite_for_each_allocate_pins_returned"),true);if(!Scope.Check()||!Scope.Freeze())return R;
    bool Good=Scope.Shape(N,false);R.Data->SetBoolField(TEXT("allocated_native_shape_verified"),Good);if(!Scope.Check())return R;R.Data->SetObjectField(TEXT("allocated_node_before_defaults"),NodeState(N,&C));if(!Scope.Reference())return R;
    if(Good)
    {
        R.Stage=TEXT("specialize_for_each_ports");for(int32 Index:{1,3})
        {if(!Scope.Check())return R;UEdGraphPin* P=N->Pins[Index];P->PinType.PinCategory=C.ElementPinType.PinCategory;P->PinType.PinSubCategory=C.ElementPinType.PinSubCategory;P->PinType.PinSubCategoryObject=C.ElementPinType.PinSubCategoryObject;Scope.Pins[Index].Type=P->PinType;R.Data->SetNumberField(TEXT("finite_for_each_specialized_port_prefix"),Index==1?1:2);if(!Scope.Check())return R;}
        N->ResolvedWildcardType=C.ElementPinType;Scope.Typed=true;if(!Scope.Check())return R;R.Data->SetObjectField(TEXT("resolved_wildcard_type"),Function::Pin::Describe(N->ResolvedWildcardType));if(!Scope.Check())return R;
        Good=Scope.Shape(N,true);if(!Scope.Check())return R;R.Data->SetBoolField(TEXT("wildcard_reconstruction_persistence_verified"),false);R.Data->SetBoolField(TEXT("finite_for_each_reconstruct_dispatched"),false);
    }
    if(!Good)Error(R,TEXT("loop_for_each_specialization_mismatch"),TEXT("Exact new finite ForEach port shape differs; actual allocated node/type prefix retained without repair."));
    R.Data->SetObjectField(TEXT("pin_defaults_applied"),MakeShared<FJsonObject>());if(!Scope.Check())return R;
    if(!Scope.Reference())return R;R.Data->SetBoolField(TEXT("finite_for_each_mark_modified_attempted"),true);FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint);R.Data->SetBoolField(TEXT("finite_for_each_mark_modified_returned"),true);if(!Scope.Reference())return R;
    R.Data->SetObjectField(TEXT("finite_for_each_type_source_after"),Scope.TypeState());R.Data->SetBoolField(TEXT("finite_for_each_type_source_observed_unchanged"),true);
    R.Data->SetStringField(TEXT("runtime_reference_scope"),C.bAssetDataForEach?TEXT("Exact stock ForEachLoop with native CoreUObject.AssetData array/element ports and held layout/current roles. Typed seed only: actual producer link/callback/compile/save and AssetData values/asset resolution/runtime iteration remain separate and unverified."):TEXT("Exact stock ForEachLoop with finite Name or native SkeletalMeshComponent array/element ports and held current source roles; no component/bone/iteration execution, live values, extra object type, explicit compile/save or old5.6 working ForEach claim."));Finish(C,R,N,Good);return R;
}
}

Function::FResult AddNode(const FString& BlueprintPath, const FString& LoopType,
    const FString& FirstIndexJson, const FString& LastIndexJson, const FString& NodePositionJson,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing,
    const FString& ElementType, const FString& TypeObject, const FString& EnumType)
{
    FResult R; FContext C; const FEngineVersion& Version = FEngineVersion::Current();
    R.Data->SetBoolField(TEXT("node_only_contract"), !bCreateGraphIfMissing); R.Data->SetBoolField(TEXT("implicit_data_wiring"), false);
    R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), false); R.Data->SetBoolField(TEXT("full_legacy_loop_parity"), false);
    R.Data->SetBoolField(TEXT("explicit_compile_requested"), false); R.Data->SetBoolField(TEXT("explicit_save_requested"), false);
    R.Data->SetStringField(TEXT("requested_element_type"), ElementType); R.Data->SetStringField(TEXT("requested_type_object"), TypeObject); R.Data->SetStringField(TEXT("requested_enum_type"), EnumType);
    R.Data->SetStringField(TEXT("requested_loop_type"), LoopType); R.Data->SetStringField(TEXT("loop_type"), TEXT("for_loop"));
    R.Data->SetStringField(TEXT("requested_graph_name"), GraphName); R.Data->SetStringField(TEXT("requested_graph_id"), GraphId); R.Data->SetStringField(TEXT("requested_graph_type"), GraphType);
    R.Data->SetBoolField(TEXT("create_graph_if_missing"), bCreateGraphIfMissing);
    R.Data->SetStringField(TEXT("omitted_index_policy"), TEXT("JSON null leaves engine-allocated native default unchanged; no inferred zero literal"));
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Error(R, TEXT("loop_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); return R; }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8) { Error(R, TEXT("loop_version_unsupported"), TEXT("Only installed UE 5.8.x is implemented.")); return R; }
    if (!Arguments(C, R, LoopType, FirstIndexJson, LastIndexJson, NodePositionJson, bCreateGraphIfMissing, ElementType, TypeObject, EnumType))return R;
    if(C.bFiniteForEach)return AddFiniteForEach(C,R,BlueprintPath,GraphName,GraphId,GraphType);
    if (!ResolveTarget(C, R, BlueprintPath, GraphName, GraphId, GraphType) || !MacroSource(C, R)) { return R; }
    if (!Capture(C, C.Before)) { Error(R, TEXT("loop_before_state_unobservable"), TEXT("Bounded existing graph/declaration/pin observation was incomplete; no mutation dispatched.")); return R; }
    R.Data->SetObjectField(TEXT("before"), C.Before);
    TStrongObjectPtr<UBlueprint> KeepTarget(C.Blueprint); TStrongObjectPtr<UBlueprint> KeepMacro(C.MacroBlueprint);
    R.Stage = TEXT("create_loop_node"); R.bMutationAttempted = true; C.Blueprint->Modify();if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return R; C.Graph->Modify();if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return R;
    UK2Node_MacroInstance* Node=nullptr;
    if(C.GraphCreationCurrent)
    {
        R.Data->SetBoolField(TEXT("graph_creation_loop_node_allocation_entered"),true);Node=NewObject<UK2Node_MacroInstance>(C.Graph,NAME_None,RF_Transactional);C.GraphCreationNode.Reset(Node);
        R.Data->SetBoolField(TEXT("graph_creation_loop_node_returned_nonnull"),Node!=nullptr);if(!Node||!C.GraphCreationCurrent(Node))return R;
        C.Graph->AddNode(Node,false,false);if(!C.GraphCreationCurrent(Node))return R;
        Node->SetMacroGraph(C.MacroGraph);if(!C.GraphCreationCurrent(Node))return R;
        Node->NodePosX=C.X;Node->NodePosY=C.Y;Node->CreateNewGuid();if(!C.GraphCreationCurrent(Node))return R;
        Node->PostPlacedNewNode();if(!C.GraphCreationCurrent(Node))return R;
        if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!C.GraphCreationCurrent(Node))return R;}
    }
    else
    {
        FGraphNodeCreator<UK2Node_MacroInstance> Creator(*C.Graph);Node=Creator.CreateNode(false);
        Node->SetMacroGraph(C.MacroGraph);Node->NodePosX=C.X;Node->NodePosY=C.Y;Creator.Finalize();
    }
    bool Good = NodeShape(C, Node, false); R.Data->SetBoolField(TEXT("allocated_native_shape_verified"), Good);
    R.Data->SetObjectField(TEXT("allocated_node_before_defaults"), NodeState(Node,C.GraphCreationCurrent?&C:nullptr));if(C.FiniteCheckpoint&&!C.FiniteCheckpoint())return R;
    if (!Good) { Error(R, TEXT("loop_allocated_node_shape_mismatch"), TEXT("Engine-allocated exact ForLoop instance pins differed from observed source port mapping; retained node is not repaired.")); }
    auto Applied = MakeShared<FJsonObject>();
    if (Good && C.bForEach)
    {
        R.Stage = TEXT("specialize_for_each_ports");
        for (int32 Index : {1, 3})
        {
            UEdGraphPin* Pin = Node->Pins[Index];
            // Change only scalar wildcard identity; native flags/container and
            // default/pin/link identities are preserved from allocation.
            Pin->PinType.PinCategory = C.ElementPinType.PinCategory;
            Pin->PinType.PinSubCategory = C.ElementPinType.PinSubCategory;
            Pin->PinType.PinSubCategoryObject = C.ElementPinType.PinSubCategoryObject;
        }
        Node->ResolvedWildcardType = C.ElementPinType;
        R.Data->SetObjectField(TEXT("resolved_wildcard_type"), DescribeType(Node->ResolvedWildcardType));
        R.Data->SetBoolField(TEXT("wildcard_reconstruction_persistence_verified"), false);
        Good = NodeShape(C, Node);
        if (!Good) { Error(R, TEXT("loop_for_each_specialization_mismatch"), TEXT("Only scalar int/bool specialization requested; retained native port/wildcard state differs.")); }
    }
    if (Good && !C.bForEach)
    {
        R.Stage = TEXT("apply_loop_defaults");
        for (int32 Index : {1, 2})
        {
            if (Index == 1 ? !C.bFirst : !C.bLast) { continue; }
            UEdGraphPin* Pin = Node->FindPin(PortNames[Index], EGPD_Input); const FString Value = LexToString(Index == 1 ? C.First : C.Last);
            if(C.GraphCreationCurrent)
            {
                if(!Pin||Pin->bDefaultValueIsIgnored||Pin->bDefaultValueIsReadOnly){Good=false;Error(R,TEXT("loop_engine_default_rejected"),TEXT("Allocated loop input is unavailable or readonly."));break;}
                const FString Validation=C.Schema->IsPinDefaultValid(Pin,Value,nullptr,FText::GetEmpty());if(!C.GraphCreationCurrent(Node))return R;
                if(!Validation.IsEmpty()){Good=false;Error(R,TEXT("loop_engine_default_rejected"),Validation);break;}
                R.Data->SetStringField(TEXT("graph_creation_default_entered"),PortNames[Index]);C.Schema->TrySetDefaultValue(*Pin,Value,false);R.Data->SetStringField(TEXT("graph_creation_default_returned"),PortNames[Index]);if(!C.GraphCreationCurrent(Node))return R;
                const bool Match=C.Schema->DoesDefaultValueMatch(*Pin,Value);if(!C.GraphCreationCurrent(Node))return R;
                const FString CurrentValidation=C.Schema->IsCurrentPinDefaultValid(Pin);if(!C.GraphCreationCurrent(Node))return R;
                if(!Match||!CurrentValidation.IsEmpty()||Pin->DefaultObject||!Pin->DefaultTextValue.IsEmpty()){Good=false;Error(R,TEXT("loop_default_readback_mismatch"),TEXT("Actual allocated loop default differs from requested value."));break;}
                Applied->SetStringField(PortNames[Index],Pin->DefaultValue);continue;
            }
            if (!Pin || Pin->bDefaultValueIsIgnored || Pin->bDefaultValueIsReadOnly || !C.Schema->IsPinDefaultValid(Pin, Value, nullptr, FText::GetEmpty()).IsEmpty())
            { Good = false; Error(R, TEXT("loop_engine_default_rejected"), TEXT("Engine rejected the prevalidated int32 default on the newly created loop pin: ") + FString(PortNames[Index])); break; }
            C.Schema->TrySetDefaultValue(*Pin, Value, false);
            if (!C.Schema->DoesDefaultValueMatch(*Pin, Value) || !C.Schema->IsCurrentPinDefaultValid(Pin).IsEmpty() || Pin->DefaultObject || !Pin->DefaultTextValue.IsEmpty())
            { Good = false; Error(R, TEXT("loop_default_readback_mismatch"), TEXT("Engine default readback differs on the retained new node: ") + FString(PortNames[Index])); break; }
            Applied->SetStringField(PortNames[Index], Pin->DefaultValue);
        }
    }
    R.Data->SetObjectField(TEXT("pin_defaults_applied"), Applied);
    if (IsValid(Node) && C.Graph->Nodes.Contains(Node)) { FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint); }
    R.Data->SetStringField(TEXT("runtime_reference_scope"), TEXT("exact stock Engine StandardMacros selected loop expands native Engine math/array/control-flow; no Cubeless runtime function/class/type reference introduced; actual compile/cook/runtime remain separate evidence"));
    Finish(C, R, Node, Good); return R;
}

}


namespace Cubeless::UE58::Loop
{
namespace
{
// ML read-only role: no allocation, wildcard inference, reconstruction or link writer.
static_assert(std::is_same_v<decltype(&UDataTableFunctionLibrary::GetDataTableRowNames),
    void (*)(const UDataTable*,TArray<FName>&)>);
struct FNameForEachConnection
{
    FContext C; FResult Legacy; FFiniteForEachScope S;
    TSharedRef<FJsonObject> R=MakeShared<FJsonObject>();
    UEdGraphNode* Producer=nullptr; UK2Node_MacroInstance* Macro=nullptr;
    bool MakeProducer=false,MakeReady=false;UClass* MakeClass=nullptr;UStruct* MakeSuper=nullptr;int32 MakeInputs=0,MakeClassAlignment=0;FString MakePath;TMap<FName,FString> MakeMetadata;
    UEdGraphPin *Output=nullptr,*Input=nullptr;
    UFunction* NativeFunction=nullptr; const FArrayProperty* ArrayField=nullptr; const FProperty* InnerField=nullptr; UClass* Owner=nullptr; UClass* TableClass=nullptr;
    TArray<const FProperty*> Inventory;
    struct FFieldRole
    {
        const FProperty* Field=nullptr; const FFieldClass* Kind=nullptr; FFieldVariant DirectOwner;
        FName Name; uint64 Flags=0; int32 Offset=0,Size=0,Dim=0;
        TObjectPtr<UClass> ObjectClass; bool Object=false;
        TMap<FName,FString> Metadata;
    };
    TArray<FFieldRole> Fields;
    struct FExtraPin
    {
        UEdGraphNode* Node=nullptr; UEdGraphPin* Pin=nullptr; FGuid Persistent;
        uint32 Flags=0; FText Friendly; FString Tooltip,Deprecated;
        UEdGraphPin* PassThrough=nullptr;
    };
    TArray<FExtraPin> ExtraPins;
    struct FCDORole{TStrongObjectPtr<UClass> Class; TObjectPtr<UObject> Handle;};
    TArray<FCDORole> CDOs;
    TArray<TObjectPtr<UEdGraph>> Functions,Events,Macros;
    TArray<FBPVariableDescription> Variables,GeneratedVariables;
    EBlueprintType BlueprintType=BPTYPE_Normal; EBlueprintStatus Status=BS_Unknown;
    FEdGraphPinType ResolvedType; bool Reconstruct=false,TargetReady=false,MacroReady=false,FunctionReady=false,Stopped=false;
    uint32 FunctionFlags=0; int32 FunctionSize=0,FunctionAlignment=0; uint8 ParameterCount=0;uint16 ParameterSize=0,ReturnOffset=0;
    TMap<FName,FString> FunctionMetadata;
    bool Typed=false,Linked=false,Inapplicable=false;
    explicit FNameForEachConnection():S(C,Legacy)
    {
        C.bForEach=C.bFiniteForEach=true; C.ElementType=TEXT("name"); C.ElementPinType.PinCategory=UEdGraphSchema_K2::PC_Name;
        R->SetBoolField(TEXT("success"),false); R->SetStringField(TEXT("stage"),TEXT("preflight"));
        R->SetStringField(TEXT("connection_scope"),TEXT("native_name_foreach_connection"));
        for(const TCHAR* K:{TEXT("mutation_attempted"),TEXT("saved"),TEXT("compiled"),TEXT("loaded_during_read")})R->SetBoolField(K,false);
    }
    bool Bad(const TCHAR* At,const FString& Expected=FString(),const FString& Actual=FString())
    {
        if(!Stopped)
        {
            const bool WasComplete=R->GetBoolField(TEXT("success"));Stopped=true;R->SetBoolField(TEXT("success"),false);
            if(WasComplete)R->SetStringField(TEXT("stage"),TEXT("verification"));
            R->SetStringField(TEXT("error_code"),TEXT("graph_name_foreach_scope_changed"));
            R->SetStringField(TEXT("error"),TEXT("Exact loaded Name ForEach connection roles are unavailable or changed; no writer or follow-up SDK read is dispatched."));
            auto F=MakeShared<FJsonObject>(); F->SetStringField(TEXT("first_failed_condition"),At);
            F->SetStringField(TEXT("expected"),Expected); F->SetStringField(TEXT("actual"),Actual);
            if(MakeProducer){F->SetStringField(TEXT("object"),MakePath.IsEmpty()?TEXT("/Script/BlueprintGraph.K2Node_MakeArray"):MakePath);if(Expected.IsEmpty()&&Actual.IsEmpty()){F->SetStringField(TEXT("expected"),TEXT("combined condition true"));F->SetStringField(TEXT("actual"),TEXT("combined condition false"));}}
            R->SetObjectField(TEXT("name_foreach_preflight_failure"),F);
            R->SetBoolField(TEXT("followup_sdk_stopped"),true);
            if(!Legacy.ErrorCode.IsEmpty())R->SetObjectField(TEXT("stock_scope_failure"),Legacy.Data);
        }
        return false;
    }
    static bool SameType(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {
        return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper
            &&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat
            &&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;
    }
    // PostReconstructNode may copy a connected array consumer's whole type.
    // Admit only these two exact Name[] output types; scalar inputs stay values.
    bool CheckMakeArrayOutputType(const FEdGraphPinType& Actual,const TCHAR* At)
    {
        FEdGraphPinType Value;Value.PinCategory=UEdGraphSchema_K2::PC_Name;Value.ContainerType=EPinContainerType::Array;
        FEdGraphPinType ConstRef=Value;ConstRef.bIsReference=true;ConstRef.bIsConst=true;
        if(SameType(Actual,Value)||SameType(Actual,ConstRef))return true;
        const FEdGraphPinType Observed=Actual;
        const FString ExpectedValue=TypeText(Value);if(!Current())return false;
        const FString ExpectedConstRef=TypeText(ConstRef);if(!Current())return false;
        const FString ActualText=TypeText(Observed);if(!Current())return false;
        return Bad(At,ExpectedValue+TEXT(" OR ")+ExpectedConstRef,ActualText);
    }
    static uint32 Flags(const UEdGraphPin* P)
    {
        return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)
            |(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)
            |(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)
            |(uint32(P->ShouldSavePinIfOrphaned())<<9);
    }
    static bool SameMetadata(const TMap<FName,FString>& A,const TMap<FName,FString>* B)
    {
        if(A.Num()!=(B?B->Num():0))return false;
        if(B)for(const auto& V:A){const FString* Found=B->Find(V.Key);if(!Found||!Found->Equals(V.Value,ESearchCase::CaseSensitive))return false;}
        return true;
    }
    // Pointer inventory is checked by S.Check before any captured physical pin or field is consumed.
    bool State()
    {
        if(Stopped)return false;
        if(!S.Check())return Bad(TEXT("held_source_target_current"));
        if(TargetReady)
        {
            UBlueprint* BP=C.Blueprint;
            if(BP->BlueprintType!=BlueprintType||BP->Status!=Status||BP->FunctionGraphs!=Functions
                ||BP->UbergraphPages!=Events||BP->MacroGraphs!=Macros
                ||!S.SamePresetLocals(BP->NewVariables,Variables)||!S.SamePresetLocals(BP->GeneratedVariables,GeneratedVariables))
                return Bad(TEXT("target_declarations_current"));
            for(const auto& D:CDOs)
            {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
                const auto H=D.Class->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
                if(!H.IsResolved()||H!=D.Handle)return Bad(TEXT("held_cdo_handle_current"));
            }
        }
        for(const auto& V:ExtraPins)
        {
            // S.Check already proves current membership, exact physical pointer, identity/type/default/link values.
            const UEdGraphPin* P=V.Pin;
            if(Flags(P)!=V.Flags||P->PersistentGuid!=V.Persistent||!P->PinFriendlyName.IdenticalTo(V.Friendly)
                ||!P->PinToolTip.Equals(V.Tooltip,ESearchCase::CaseSensitive)
                ||!P->DeprecationMessage.Equals(V.Deprecated,ESearchCase::CaseSensitive)
                ||P->ReferencePassThroughConnection!=V.PassThrough)return Bad(TEXT("whole_pin_metadata_current"));
        }
        if(MacroReady&&(!SameType(Macro->ResolvedWildcardType,ResolvedType)||Macro->bReconstructNode!=Reconstruct))
            return Bad(TEXT("macro_resolution_current"));
        if(MakeReady)
        {
            auto It=MakeClass->GetSuperStructIterator();++It;
            if(Producer->GetClass()!=MakeClass||static_cast<UK2Node_MakeArray*>(Producer)->NumInputs!=MakeInputs)
                return Bad(TEXT("make_array_count_current"),LexToString(MakeInputs),LexToString(static_cast<UK2Node_MakeArray*>(Producer)->NumInputs));
            if((It?*It:nullptr)!=MakeSuper||MakeClass->GetMinAlignment()!=MakeClassAlignment
                ||!MakeClass->ClassGeneratedBy.IsResolved()||MakeClass->ClassGeneratedBy!=nullptr
                ||!SameMetadata(MakeMetadata,FMetaData::GetMapForObject(MakeClass)))return Bad(TEXT("make_array_native_current"));
        }
        if(FunctionReady)
        {
            if(uint32(NativeFunction->FunctionFlags)!=FunctionFlags||NativeFunction->NumParms!=ParameterCount||NativeFunction->ParmsSize!=ParameterSize||NativeFunction->ReturnValueOffset!=ReturnOffset
                ||NativeFunction->GetStructureSize()!=FunctionSize||NativeFunction->GetMinAlignment()!=FunctionAlignment
                ||!SameMetadata(FunctionMetadata,FMetaData::GetMapForObject(NativeFunction)))return Bad(TEXT("native_function_current"));
            TArray<const FProperty*> Now; for(TFieldIterator<FProperty> It(NativeFunction,EFieldIteratorFlags::ExcludeSuper);It;++It)Now.Add(*It);
            if(Now!=Inventory)return Bad(TEXT("native_function_field_inventory"));
            const FArrayProperty* A=nullptr;for(const FProperty* P:Now)if(P->GetFName()==TEXT("OutRowNames"))A=CastField<FArrayProperty>(P);
            if(A!=ArrayField||!A||A->Inner!=InnerField)return Bad(TEXT("native_array_inner_slot_current"));
            for(const auto& V:Fields)
            {
                const FProperty* P=V.Field;
                if(P->GetClass()!=V.Kind||P->GetOwnerVariant()!=V.DirectOwner||P->GetOwnerStruct()!=NativeFunction
                    ||P->GetFName()!=V.Name||uint64(P->GetPropertyFlags())!=V.Flags||P->GetOffset_ForInternal()!=V.Offset
                    ||P->GetSize()!=V.Size||P->ArrayDim!=V.Dim||P->HasSetterOrGetter()
                    ||!SameMetadata(V.Metadata,P->GetMetaDataMap()))return Bad(TEXT("native_property_current"),V.Name.ToString());
                if(V.Object){const auto* O=static_cast<const FObjectProperty*>(P);if(!O->PropertyClass.IsResolved()||O->PropertyClass!=V.ObjectClass)return Bad(TEXT("native_table_property_class_current"));}
                if(P->GetOwner<FProperty>()&&(!A||A->Inner!=P))return Bad(TEXT("native_array_inner_slot_current"));
            }
        }
        return true;
    }
    bool Current()
    {
        if(!State())return false;
        if(MacroReady)
        {
            UEdGraph* G=Macro->GetMacroGraph(); if(!State())return false;
            if(G!=C.MacroGraph)return Bad(TEXT("current_macro_graph_reference"));
            UBlueprint* BP=Macro->GetSourceBlueprint(); if(!State())return false;
            if(BP!=C.MacroBlueprint)return Bad(TEXT("current_macro_blueprint_reference"));
        }
        return true;
    }
    UClass* LoadedClass(const TObjectPtr<UClass>& H)
    {
        if(!H.IsResolved()||H==nullptr)return nullptr;
        const FString ClassPath=H.GetPathName();
        UClass* K=FindObject<UClass>(nullptr,*ClassPath);TStrongObjectPtr<UClass> Returned;
        if(S.Safe()&&S.Live(K))Returned.Reset(K);
        if(!Current()||!Returned.IsValid()||H!=K)return nullptr;
        if(!S.Hold(K,true)||!Current()||H!=K)return nullptr;return K;
    }
    bool HoldCDO(UClass* K)
    {
        if(!S.Hold(K,true)||!S.Hold(K->GetPackage(),true))return false;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto H=K->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        if(!H.IsResolved())return Bad(TEXT("initial_cdo_unresolved"));
        if(H!=nullptr)
        {
            const FString CDOPath=H.GetPathName();UObject* D=FindObject<UObject>(nullptr,*CDOPath);
            TStrongObjectPtr<UObject> Returned;if(S.Safe()&&S.Live(D))Returned.Reset(D);
            if(!Current()||!Returned.IsValid())return Bad(TEXT("initial_cdo_lookup_current"));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto CurrentHandle=K->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!CurrentHandle.IsResolved()||CurrentHandle!=H||H!=D||D->GetClass()!=K||D->GetOuter()!=K->GetPackage()
                ||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!S.Hold(D,true))return Bad(TEXT("initial_cdo_role"));
        }
        FCDORole V;V.Class.Reset(K);V.Handle=H;CDOs.Add(MoveTemp(V));return true;
    }
    bool Bind(const FString& GraphPath,const FString& SourcePath,const FString& TargetPath)
    {
        if(!S.PrepareType())return Bad(TEXT("initial_context"));
        C.Graph=FindObject<UEdGraph>(nullptr,*GraphPath);TStrongObjectPtr<UEdGraph> ReturnedGraph;
        if(S.Safe()&&S.Live(C.Graph))ReturnedGraph.Reset(C.Graph);
        if(!Current()||!ReturnedGraph.IsValid()||!C.Graph->GetPathName().Equals(GraphPath,ESearchCase::CaseSensitive)||!C.Graph->GraphGuid.IsValid()||!S.Hold(C.Graph,true))return Bad(TEXT("loaded_graph"));
        C.Blueprint=Cast<UBlueprint>(C.Graph->GetOuter());TStrongObjectPtr<UBlueprint> ReturnedBlueprint;
        if(S.Safe()&&S.Live(C.Blueprint))ReturnedBlueprint.Reset(C.Blueprint);
        if(!Current()||!ReturnedBlueprint.IsValid()||!C.Blueprint->GetPathName().StartsWith(TEXT("/Game/"),ESearchCase::CaseSensitive)
            ||C.Blueprint->bBeingCompiled||C.Blueprint->bQueuedForCompilation||C.Blueprint->bIsRegeneratingOnLoad
            ||C.Blueprint->bSuppressStructurallyModified||C.Blueprint->Status==BS_BeingCreated
            ||C.Blueprint->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly))return Bad(TEXT("loaded_blueprint"));
        for(UObject* O:{static_cast<UObject*>(C.Blueprint),static_cast<UObject*>(C.Blueprint->GetPackage()),static_cast<UObject*>(C.Graph)})if(!S.Hold(O,true))return Bad(TEXT("hold_target"));
        const auto SchemaHandle=C.Graph->Schema.GetGCPtr(),ParentHandle=C.Blueprint->ParentClass.GetGCPtr(),GeneratedHandle=C.Blueprint->GeneratedClass.GetGCPtr(),SkeletonHandle=C.Blueprint->SkeletonGeneratedClass.GetGCPtr();
        auto BootstrapCurrent=[&](){if(!Current())return false;return C.Graph->Schema.GetGCPtr().IsResolved()&&C.Graph->Schema.GetGCPtr()==SchemaHandle&&C.Blueprint->ParentClass.GetGCPtr().IsResolved()&&C.Blueprint->ParentClass.GetGCPtr()==ParentHandle&&C.Blueprint->GeneratedClass.GetGCPtr().IsResolved()&&C.Blueprint->GeneratedClass.GetGCPtr()==GeneratedHandle&&C.Blueprint->SkeletonGeneratedClass.GetGCPtr().IsResolved()&&C.Blueprint->SkeletonGeneratedClass.GetGCPtr()==SkeletonHandle?true:Bad(TEXT("initial_class_wrappers_current"));};
        UClass* SchemaClass=LoadedClass(SchemaHandle);if(!SchemaClass||!BootstrapCurrent())return Bad(TEXT("loaded_schema_class"));
        UClass* Parent=LoadedClass(ParentHandle);if(!Parent||!BootstrapCurrent())return Bad(TEXT("loaded_parent_class"));
        UClass* Generated=LoadedClass(GeneratedHandle);if(!Generated||!BootstrapCurrent())return Bad(TEXT("loaded_generated_class"));
        UClass* Skeleton=LoadedClass(SkeletonHandle);if(!Skeleton||!BootstrapCurrent())return Bad(TEXT("loaded_skeleton_class"));
        if(!SchemaClass->GetPathName().Equals(TEXT("/Script/BlueprintGraph.EdGraphSchema_K2"),ESearchCase::CaseSensitive))return Bad(TEXT("loaded_target_classes"));
        for(UClass* K:{SchemaClass,Parent,Generated,Skeleton})if(!HoldCDO(K)||!BootstrapCurrent())return false;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto SchemaCDO=SchemaClass->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        if(!SchemaCDO.IsResolved()||SchemaCDO==nullptr)return Bad(TEXT("existing_schema_cdo"));
        const FString SchemaCDOPath=SchemaCDO.GetPathName();UObject* SchemaObject=FindObject<UObject>(nullptr,*SchemaCDOPath);
        TStrongObjectPtr<UObject> ReturnedSchemaCDO;if(S.Safe()&&S.Live(SchemaObject))ReturnedSchemaCDO.Reset(SchemaObject);
        if(!BootstrapCurrent()||!ReturnedSchemaCDO.IsValid()||SchemaCDO!=SchemaObject||SchemaObject->GetClass()!=SchemaClass)return Bad(TEXT("existing_schema_cdo"));
        C.Schema=Cast<UEdGraphSchema_K2>(SchemaObject);if(!BootstrapCurrent()||!C.Schema)return Bad(TEXT("exact_schema_cdo"));
        BlueprintType=C.Blueprint->BlueprintType;Status=C.Blueprint->Status;Functions=C.Blueprint->FunctionGraphs;Events=C.Blueprint->UbergraphPages;Macros=C.Blueprint->MacroGraphs;
        Variables=C.Blueprint->NewVariables;GeneratedVariables=C.Blueprint->GeneratedVariables;
        C.bPresetLibraryForEach=BlueprintType==BPTYPE_FunctionLibrary;
        if(!C.bPresetLibraryForEach&&BlueprintType!=BPTYPE_Normal)return Bad(TEXT("supported_target_type"));
        if(!C.bPresetLibraryForEach)
        {
            for(auto It=Parent->GetSuperStructIterator();It;++It)
            {
                UClass* K=const_cast<UClass*>(Cast<UClass>(*It));
                if(!S.Live(K)||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)
                    ||K->IsEditorOnly()||!(K->GetPackage()->GetName().Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)
                    ||K->GetPackage()->GetName().Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive))
                    ||!S.Hold(K,true)||!S.Hold(K->GetPackage(),true))return Bad(TEXT("native_parent_chain"));
            }
        }
        // Freeze the selected graph before the first public GetAllGraphs/reference boundary.
        if(!S.KeepGraph(C.Graph,false))return Bad(TEXT("initial_graph_inventory"));
        TargetReady=true; S.Parent=Parent;S.Generated=Generated;S.Skeleton=Skeleton;S.OldTargetCount=C.Graph->Nodes.Num();
        FreezeExtraPins();
        if(!S.BindPresetEntry()||!Current())return Bad(TEXT("target_entry_binding"));
        TArray<UEdGraph*> All;C.Blueprint->GetAllGraphs(All);if(!Current())return false;
        if(All.Num()>256||!All.Contains(C.Graph))return Bad(TEXT("target_graph_inventory"));
        for(UEdGraph* G:All)if(!S.KeepGraph(G,false))return Bad(TEXT("target_graph_roles"));
        FreezeExtraPins();
        C.FiniteCheckpoint=[this](){return Current();};
        UEdGraphNode* Source=FindObject<UEdGraphNode>(nullptr,*SourcePath);UEdGraphNode* Target=FindObject<UEdGraphNode>(nullptr,*TargetPath);
        if(!Current())return false;
        if(!S.Live(Source)||!S.Live(Target)||(Source->GetClass()!=UK2Node_CallFunction::StaticClass()&&Source->GetClass()!=UK2Node_MakeArray::StaticClass())||Target->GetClass()!=UK2Node_MacroInstance::StaticClass()
            ||Source->GetOuter()!=C.Graph||Target->GetOuter()!=C.Graph||!C.Graph->Nodes.Contains(Source)||!C.Graph->Nodes.Contains(Target)
            ||!Source->GetPathName().Equals(SourcePath,ESearchCase::CaseSensitive)||!Target->GetPathName().Equals(TargetPath,ESearchCase::CaseSensitive))return Bad(TEXT("source_target_node_role"));
        Producer=Source;MakeProducer=Source->GetClass()==UK2Node_MakeArray::StaticClass();if(MakeProducer)MakePath=SourcePath;Macro=static_cast<UK2Node_MacroInstance*>(Target);
        if(!Producer->NodeGuid.IsValid()||!Macro->NodeGuid.IsValid()||Macro->bReconstructNode)return Bad(TEXT("stable_macro_node"));
        if(!NativeProducer())return false;
        // Only this fully fenced dispatch result may resume the existing generic Macro route.
        UEdGraph* Referenced=Macro->GetMacroGraph();if(!Current())return false;
        if(!S.Live(Referenced)||!S.Hold(Referenced,true))return Bad(TEXT("dispatch_loaded_macro_graph"));
        UBlueprint* SourceBP=Cast<UBlueprint>(Referenced->GetOuter());
        if(!S.Live(SourceBP)||!S.Hold(SourceBP,true))return Bad(TEXT("dispatch_loaded_macro_blueprint"));
        UBlueprint* ReferencedBP=Macro->GetSourceBlueprint();if(!Current())return false;
        if(ReferencedBP!=SourceBP)return Bad(TEXT("dispatch_macro_owner"));
        if(Referenced->GetFName()!=TEXT("ForEachLoop")||!SourceBP->GetPathName().Equals(StockBlueprintPath,ESearchCase::CaseSensitive))
        {
            Inapplicable=true;R->SetBoolField(TEXT("success"),true);R->SetBoolField(TEXT("applicable"),false);
            R->SetBoolField(TEXT("current_verified"),true);R->SetBoolField(TEXT("observations_complete"),true);
            R->SetStringField(TEXT("stage"),TEXT("complete"));return true;
        }
        if(!S.PrepareStockRoles())return Bad(TEXT("stock_roles"));
        FreezeExtraPins();
        if(!MacroSource(C,Legacy)||!Current()||!S.BindStock()||Referenced!=C.MacroGraph||SourceBP!=C.MacroBlueprint)return Bad(TEXT("stock_contract"));
        ResolvedType=Macro->ResolvedWildcardType;Reconstruct=Macro->bReconstructNode;
        FEdGraphPinType Empty,NameArray=C.ElementPinType;NameArray.ContainerType=EPinContainerType::Array;
        if(!SameType(ResolvedType,Empty)&&!SameType(ResolvedType,C.ElementPinType)&&!SameType(ResolvedType,NameArray))return Bad(TEXT("bounded_current_resolved_name_type"));
        MacroReady=true;
        return Current();
    }
    void FreezeExtraPins()
    {
        for(const auto& N:S.Prior)for(const auto& V:N.Pins)
        {
            if(ExtraPins.ContainsByPredicate([&](const FExtraPin& E){return E.Pin==V.P;}))continue;
            FExtraPin E;E.Node=N.Hold.Get();E.Pin=V.P;E.Persistent=V.P->PersistentGuid;E.Flags=Flags(V.P);
            E.Friendly=V.P->PinFriendlyName;E.Tooltip=V.P->PinToolTip;E.Deprecated=V.P->DeprecationMessage;E.PassThrough=V.P->ReferencePassThroughConnection;ExtraPins.Add(MoveTemp(E));
        }
    }
    // MR: this producer is already a concrete Name MakeArray. It grants no
    // wildcard producer inference and introduces no connection writer.
    bool MakeArraySource()
    {
        if(!Current())return false;
        UClass* K=UK2Node_MakeArray::StaticClass();TStrongObjectPtr<UClass> Returned;
        if(S.Safe()&&S.Live(K))Returned.Reset(K);
        if(!Current()||!Returned.IsValid()||Producer->GetClass()!=K||!S.Hold(K,true)||!S.Hold(K->GetPackage(),true)||!Current())return Bad(TEXT("make_array_native_class"),TEXT("exact held native K2Node_MakeArray"),TEXT("combined class role unavailable"));
        if(!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy!=nullptr
            ||!K->GetPathName().Equals(TEXT("/Script/BlueprintGraph.K2Node_MakeArray"),ESearchCase::CaseSensitive)
            ||!K->GetPackage()->GetName().Equals(TEXT("/Script/BlueprintGraph"),ESearchCase::CaseSensitive)
            ||!K->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||K->GetPackage()->IsDirty())return Bad(TEXT("make_array_native_source"),TEXT("current native compiled-in BlueprintGraph class"),TEXT("combined source role mismatch"));
        auto It=K->GetSuperStructIterator();++It;UStruct* Parent=It?const_cast<UStruct*>(*It):nullptr;
        if(Parent!=UK2Node_MakeContainer::StaticClass()||!S.Hold(Parent,true)||!Current())return Bad(TEXT("make_array_native_parent"),TEXT("K2Node_MakeContainer"),TEXT("current parent differs"));
        MakeClass=K;MakeSuper=Parent;MakeClassAlignment=K->GetMinAlignment();if(const auto* M=FMetaData::GetMapForObject(K))MakeMetadata=*M;
        MakeInputs=static_cast<UK2Node_MakeArray*>(Producer)->NumInputs;
        if(MakeInputs<0||MakeInputs>128||Producer->Pins.Num()!=MakeInputs+1)return Bad(TEXT("make_array_input_inventory"),TEXT("NumInputs 0..128 and NumInputs+1 pins"),LexToString(MakeInputs)+TEXT(" / ")+LexToString(Producer->Pins.Num()));
        MakeReady=true;if(!Current())return false;
        FEdGraphPinType Name;Name.PinCategory=UEdGraphSchema_K2::PC_Name;
        UEdGraphPin* Array=Producer->FindPin(TEXT("Array"),EGPD_Output);TSet<const UEdGraphPin*> Found;
        if(!Array)return Bad(TEXT("make_array_output_type"),TEXT("Name[] output Array"),TEXT("missing"));
        if(!CheckMakeArrayOutputType(Array->PinType,TEXT("make_array_output_type")))return false;Found.Add(Array);
        for(int32 Index=0;Index<MakeInputs;++Index)
        {
            const FName ExpectedName(*FString::Printf(TEXT("[%d]"),Index));UEdGraphPin* P=Producer->FindPin(ExpectedName,EGPD_Input);
            if(!P||Found.Contains(P)||!SameType(P->PinType,Name))return Bad(TEXT("make_array_scalar_input"),ExpectedName.ToString()+TEXT(": nonref nonconst scalar Name"),TEXT("missing/duplicate/whole type differs"));Found.Add(P);
            const FString Error=C.Schema->IsCurrentPinDefaultValid(P);if(!Current())return false;
            if(!Error.IsEmpty())return Bad(TEXT("make_array_current_name_default"),TEXT("schema-valid current Name default"),Error);
        }
        for(auto* P:Producer->Pins)
        {
            if(!Found.Contains(P)||P->GetOwningNodeUnchecked()!=Producer||P->ParentPin||!P->SubPins.IsEmpty()
                ||P->bHidden||P->bNotConnectable||P->bDefaultValueIsReadOnly||P->bDefaultValueIsIgnored||P->bOrphanedPin
                ||!P->DefaultObject.IsResolved()||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return Bad(TEXT("make_array_whole_pin_role"),TEXT("current unsplit Name value ports with null object/empty text"),TEXT("combined physical role differs"));
        }
        auto* Actual=static_cast<UK2Node_MakeArray*>(Producer)->GetOutputPin();if(!Current())return false;
        return Actual==Array?true:Bad(TEXT("make_array_public_output"),TEXT("Array physical output"),TEXT("public output accessor differs"));
    }
    TSharedRef<FJsonObject> MakeClassSource()
    {
        auto J=MakeShared<FJsonObject>();if(!Current()||!MakeReady)return J;
        J->SetStringField(TEXT("class_path"),MakeClass->GetPathName());J->SetStringField(TEXT("identity"),LexToString(reinterpret_cast<UPTRINT>(MakeClass)));
        J->SetStringField(TEXT("class_flags_string"),LexToString(uint32(MakeClass->GetClassFlags())));J->SetStringField(TEXT("package_path"),MakeClass->GetPackage()->GetName());J->SetStringField(TEXT("package_flags_string"),LexToString(MakeClass->GetPackage()->GetPackageFlags()));J->SetBoolField(TEXT("package_dirty"),MakeClass->GetPackage()->IsDirty());
        J->SetNumberField(TEXT("class_size"),MakeClass->GetStructureSize());J->SetNumberField(TEXT("class_alignment"),MakeClassAlignment);J->SetStringField(TEXT("super_class_path"),MakeSuper->GetPathName());J->SetStringField(TEXT("super_class_identity"),LexToString(reinterpret_cast<UPTRINT>(MakeSuper)));
        auto Meta=MakeShared<FJsonObject>();for(const auto& E:MakeMetadata)Meta->SetStringField(E.Key.ToString(),E.Value);J->SetObjectField(TEXT("metadata"),Meta);J->SetBoolField(TEXT("native"),true);J->SetBoolField(TEXT("class_generated_by_null"),true);J->SetBoolField(TEXT("current_verified"),Current());return J;
    }

    bool NativeProducer()
    {
        if(MakeProducer)return MakeArraySource();
        Owner=FindObject<UClass>(nullptr,TEXT("/Script/Engine.DataTableFunctionLibrary"));TStrongObjectPtr<UClass> ReturnedOwner;
        if(S.Safe()&&S.Live(Owner))ReturnedOwner.Reset(Owner);
        if(!Current()||!ReturnedOwner.IsValid()||!S.Hold(Owner,true)||!Current())return Bad(TEXT("native_owner_lookup"));
        NativeFunction=FindObject<UFunction>(nullptr,TEXT("/Script/Engine.DataTableFunctionLibrary:GetDataTableRowNames"));TStrongObjectPtr<UFunction> ReturnedFunction;
        if(S.Safe()&&S.Live(NativeFunction))ReturnedFunction.Reset(NativeFunction);
        if(!Current()||!ReturnedFunction.IsValid()||!S.Hold(NativeFunction,true)||!Current())return Bad(TEXT("native_function_lookup"));
        TableClass=FindObject<UClass>(nullptr,TEXT("/Script/Engine.DataTable"));TStrongObjectPtr<UClass> ReturnedTable;
        if(S.Safe()&&S.Live(TableClass))ReturnedTable.Reset(TableClass);
        if(!Current()||!ReturnedTable.IsValid()||!S.Hold(TableClass,true)||!Current())return Bad(TEXT("native_table_class_lookup"));
        if(!S.Live(Owner)||!S.Live(NativeFunction)||!S.Live(TableClass)||NativeFunction->GetOuter()!=Owner||NativeFunction->GetOwnerClass()!=Owner
            ||!Owner->ClassGeneratedBy.IsResolved()||Owner->ClassGeneratedBy!=nullptr||!Owner->HasAnyClassFlags(CLASS_Native)
            ||!Owner->GetPackage()->GetName().Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)
            ||!(Owner->GetPackage()->GetPackageFlags()&PKG_CompiledIn)||Owner->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)
            ||Owner->GetPackage()->IsDirty()||!NativeFunction->HasAllFunctionFlags(FUNC_Native|FUNC_Static|FUNC_BlueprintCallable)
            ||NativeFunction->HasAnyFunctionFlags(FUNC_EditorOnly|FUNC_Delegate)||NativeFunction->NumParms!=2)return Bad(TEXT("exact_native_row_names_function"));
        if(!HoldCDO(Owner)||!HoldCDO(TableClass)||!S.Hold(NativeFunction,true))return Bad(TEXT("hold_native_function"));
        FMemberReference Expected;Expected.SetExternalMember(TEXT("GetDataTableRowNames"),Owner);
        if(!static_cast<UK2Node_CallFunction*>(Producer)->FunctionReference.IsSameReference(Expected)||static_cast<UK2Node_CallFunction*>(Producer)->FunctionReference.IsDeprecated())return Bad(TEXT("exact_native_function_reference"));
        for(TFieldIterator<FProperty> It(NativeFunction,EFieldIteratorFlags::ExcludeSuper);It;++It)Inventory.Add(*It);
        const FObjectProperty* Table=nullptr;const FArrayProperty* Array=nullptr;
        if(Inventory.Num()!=2)return Bad(TEXT("native_function_inventory"),TEXT("2"),LexToString(Inventory.Num()));
        for(const FProperty* P:Inventory)
        {
            if(P->GetFName()==TEXT("Table"))Table=CastField<FObjectProperty>(P);
            if(P->GetFName()==TEXT("OutRowNames"))Array=CastField<FArrayProperty>(P);
        }
        if(!Table||!Array||Table->GetClass()!=FObjectProperty::StaticClass()||Array->GetClass()!=FArrayProperty::StaticClass()
            ||!Table->PropertyClass.IsResolved()||Table->PropertyClass!=TableClass
            ||Table->GetOwner<UObject>()!=NativeFunction||Array->GetOwner<UObject>()!=NativeFunction
            ||!Table->HasAnyPropertyFlags(CPF_Parm)||Table->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm)
            ||!Array->HasAllPropertyFlags(CPF_Parm|CPF_OutParm)||Array->HasAnyPropertyFlags(CPF_ReturnParm)
            ||Table->GetSize()!=sizeof(UDataTable*)||Array->GetSize()!=sizeof(FScriptArray)||Array->Inner==nullptr)return Bad(TEXT("native_parameter_roles"));
        const FProperty* Inner=Array->Inner;ArrayField=Array;InnerField=Inner;
        if(!Inner||Inner->GetClass()!=FNameProperty::StaticClass()||Inner->GetOwner<FProperty>()!=Array||Inner->GetOwnerStruct()!=NativeFunction
            ||Inner->GetSize()!=sizeof(FName)||Inner->ArrayDim!=1)return Bad(TEXT("native_array_inner_owner_layout"));
        for(const FProperty* P:{static_cast<const FProperty*>(Table),static_cast<const FProperty*>(Array),Inner})
        {
            if(P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetSize()<=0||P->GetOffset_ForInternal()<0)return Bad(TEXT("native_property_layout"));
            FFieldRole V;V.Field=P;V.Kind=P->GetClass();V.DirectOwner=P->GetOwnerVariant();V.Name=P->GetFName();V.Flags=uint64(P->GetPropertyFlags());
            V.Offset=P->GetOffset_ForInternal();V.Size=P->GetSize();V.Dim=P->ArrayDim;
            if(const auto* M=P->GetMetaDataMap())V.Metadata=*M;
            if(P==Table){V.Object=true;V.ObjectClass=Table->PropertyClass;}
            Fields.Add(MoveTemp(V));
        }
        FunctionFlags=uint32(NativeFunction->FunctionFlags);FunctionSize=NativeFunction->GetStructureSize();FunctionAlignment=NativeFunction->GetMinAlignment();ParameterCount=NativeFunction->NumParms;ParameterSize=NativeFunction->ParmsSize;ReturnOffset=NativeFunction->ReturnValueOffset;
        if(const auto* M=FMetaData::GetMapForObject(NativeFunction))FunctionMetadata=*M;FunctionReady=true;
        if(!Current())return false;
        UFunction* Actual=static_cast<UK2Node_CallFunction*>(Producer)->GetTargetFunction();if(!Current())return false;
        return Actual==NativeFunction?true:Bad(TEXT("resolved_native_function"));
    }
    bool PeerLinks()
    {
        if(!Current())return false;
        TMap<const UEdGraphPin*,const UEdGraphNode*> Members;
        for(const auto& N:S.Prior)if(N.Graph==C.Graph)for(const auto& P:N.Pins)Members.Add(P.P,N.Hold.Get());
        for(const auto& N:S.Prior)if(N.Graph==C.Graph)for(const auto& V:N.Pins)
        {
            TSet<const UEdGraphPin*> Unique;
            for(const UEdGraphPin* P:V.Links)
            {
                const UEdGraphNode* const* OwnerNode=Members.Find(P);
                if(!OwnerNode||Unique.Contains(P))return Bad(TEXT("linked_pin_membership"));
                Unique.Add(P);
                if(P->WasTrashed()||P->GetOwningNodeUnchecked()!=*OwnerNode||!P->LinkedTo.Contains(V.P))return Bad(TEXT("linked_pin_reciprocity"));
            }
        }
        return Current();
    }
    bool Shape()
    {
        if(!Current()||!PeerLinks())return false;
        Output=Producer->FindPin(MakeProducer?TEXT("Array"):TEXT("OutRowNames"),EGPD_Output);Input=Macro->FindPin(TEXT("Array"),EGPD_Input);
        if(!Current())return false;
        FEdGraphPinType Names;Names.PinCategory=UEdGraphSchema_K2::PC_Name;Names.ContainerType=EPinContainerType::Array;
        if(Output&&Input&&MakeProducer&&!CheckMakeArrayOutputType(Output->PinType,TEXT("exact_name_array_producer_pin")))return false;
        if(!Output||!Input||(!MakeProducer&&!SameType(Output->PinType,Names))||Output->ParentPin||!Output->SubPins.IsEmpty()
            ||Output->bHidden||Output->bOrphanedPin||Output->bNotConnectable||!Output->DefaultObject.IsResolved()
            ||Output->DefaultObject!=nullptr||!Output->DefaultTextValue.IsEmpty())return Bad(TEXT("exact_name_array_producer_pin"));
        Typed=Input->PinType.PinCategory==UEdGraphSchema_K2::PC_Name;
        if(Macro->Pins.Num()!=6)return Bad(TEXT("whole_six_macro_ports"),TEXT("6"),LexToString(Macro->Pins.Num()));
        TSet<UEdGraphPin*> Found;
        for(int32 I=0;I<6;++I)
        {
            UEdGraphPin* P=Macro->FindPin(EachPortNames[I],I<2?EGPD_Input:EGPD_Output);
            const auto Expected=EachPortType(I,Typed?&C:nullptr);
            if(!P||Found.Contains(P)||!SameType(P->PinType,Expected)||P->ParentPin||!P->SubPins.IsEmpty()
                ||P->bHidden||P->bNotConnectable||P->bDefaultValueIsReadOnly||P->bDefaultValueIsIgnored||P->bOrphanedPin
                ||!P->DefaultValue.IsEmpty()||!P->AutogeneratedDefaultValue.IsEmpty()||!P->DefaultObject.IsResolved()
                ||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return Bad(TEXT("macro_port_shape_default"),EachPortNames[I]);
            Found.Add(P);
            if(!Typed&&(I==1||I==3)&&!P->LinkedTo.IsEmpty())return Bad(TEXT("cold_wildcard_requires_unlinked_ports"));
        }
        Linked=Output->LinkedTo.Contains(Input)&&Input->LinkedTo.Contains(Output);
        if(Input->LinkedTo.Num()>(Linked?1:0)||(!Typed&&Linked))return Bad(TEXT("exact_first_array_input_link"));
        return Current();
    }
    TSharedRef<FJsonObject> TypeJson(const FEdGraphPinType& T)
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("category"),T.PinCategory.ToString());J->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());
        J->SetStringField(TEXT("type_object"),TEXT(""));J->SetNumberField(TEXT("container_type"),int32(T.ContainerType));
        J->SetBoolField(TEXT("is_reference"),T.bIsReference);J->SetBoolField(TEXT("is_const"),T.bIsConst);J->SetBoolField(TEXT("is_weak_pointer"),T.bIsWeakPointer);
        J->SetBoolField(TEXT("is_uobject_wrapper"),T.bIsUObjectWrapper);J->SetBoolField(TEXT("serialize_single_precision"),T.bSerializeAsSinglePrecisionFloat);
        auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("category"),T.PinValueType.TerminalCategory.ToString());V->SetStringField(TEXT("subcategory"),T.PinValueType.TerminalSubCategory.ToString());
        V->SetStringField(TEXT("type_object"),TEXT(""));V->SetBoolField(TEXT("is_const"),T.PinValueType.bTerminalIsConst);V->SetBoolField(TEXT("is_weak_pointer"),T.PinValueType.bTerminalIsWeakPointer);
        V->SetBoolField(TEXT("is_uobject_wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);J->SetObjectField(TEXT("terminal"),V);return J;
    }
    TSharedRef<FJsonObject> PinJson(UEdGraphPin* P,bool Projected,bool Reconstructed=true)
    {
        auto J=MakeShared<FJsonObject>();if(!Current())return J;
        FEdGraphPinType T=P->PinType;
        if(Projected&&(P==Input||P->PinName==TEXT("Array Element")))
        {T.PinCategory=UEdGraphSchema_K2::PC_Name;T.PinSubCategory=NAME_None;T.PinSubCategoryObject.Reset();}
        FString Export;FEdGraphPinType::StaticStruct()->ExportText(Export,&T,&T,nullptr,PPF_None,nullptr);if(!Current())return J;
        J->SetStringField(TEXT("name"),P->PinName.ToString());J->SetStringField(TEXT("direction"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"));
        if(!Projected)J->SetStringField(TEXT("pin_id"),P->PinId.ToString(EGuidFormats::Digits));
        const FString Display=UEdGraphSchema_K2::TypeToText(T).ToString();if(!Current())return J;
        J->SetStringField(TEXT("pin_type_display"),Display);
        J->SetStringField(TEXT("projected_category"),T.IsArray()?TEXT("array"):T.PinCategory==UEdGraphSchema_K2::PC_Name?TEXT("string"):T.PinCategory==UEdGraphSchema_K2::PC_Exec?TEXT("exec"):TEXT("integer"));
        J->SetNumberField(TEXT("flags"),Projected&&!Typed&&Reconstructed?(Flags(P)|512U):Flags(P));J->SetObjectField(TEXT("pin_type"),TypeJson(T));J->SetStringField(TEXT("pin_type_text"),Export);
        J->SetStringField(TEXT("raw_default_value"),P->DefaultValue);J->SetStringField(TEXT("autogenerated_default_value"),P->AutogeneratedDefaultValue);
        J->SetStringField(TEXT("default_object_path"),TEXT(""));J->SetBoolField(TEXT("default_text_empty"),P->DefaultTextValue.IsEmpty());
        J->SetStringField(TEXT("pin_friendly_name"),Projected&&!Typed&&Reconstructed?FString():P->PinFriendlyName.ToString());J->SetStringField(TEXT("pin_tooltip"),Projected&&!Typed&&Reconstructed?FString():P->PinToolTip);
        J->SetStringField(TEXT("persistent_guid"),Projected&&!Typed&&Reconstructed?FGuid().ToString(EGuidFormats::Digits):P->PersistentGuid.ToString(EGuidFormats::Digits));
        J->SetStringField(TEXT("deprecation_message"),Projected&&!Typed&&Reconstructed?FString():P->DeprecationMessage);
        // Link identities are copied only from the fenced current membership inventory.
        TArray<TSharedPtr<FJsonValue>> Links;TSet<const UEdGraphPin*> RenderedPeers;
        for(const auto& N:S.Prior)if(N.Graph==C.Graph)for(const auto& V:N.Pins)if(P->LinkedTo.Contains(V.P)&&!RenderedPeers.Contains(V.P))
        {RenderedPeers.Add(V.P);auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("node_path"),N.Hold->GetPathName());L->SetStringField(TEXT("pin_name"),V.Name.ToString());L->SetStringField(TEXT("pin_id"),V.Id.ToString(EGuidFormats::Digits));Links.Add(MakeShared<FJsonValueObject>(L));}
        if(!Projected)J->SetArrayField(TEXT("links"),Links);return J;
    }
    TSharedRef<FJsonObject> Run(const FString& GraphPath,const FString& SourcePath,const FString& SourcePin,const FString& TargetPath,const FString& TargetPin)
    {
        for(const auto* Value:{&GraphPath,&SourcePath,&SourcePin,&TargetPath,&TargetPin})if(!Function::Pin::SafeName(*Value))
        {Bad(TEXT("bounded_exact_selectors"));return R;}
        if((!SourcePin.Equals(TEXT("OutRowNames"),ESearchCase::CaseSensitive)&&!SourcePin.Equals(TEXT("Array"),ESearchCase::CaseSensitive))||!TargetPin.Equals(TEXT("Array"),ESearchCase::CaseSensitive)){Bad(TEXT("exact_supported_pin_pair"));return R;}
        const FEngineVersion& V=FEngineVersion::Current();
        if(V.GetMajor()!=5||V.GetMinor()!=8){Bad(TEXT("installed_version"));return R;}
        if(!Bind(GraphPath,SourcePath,TargetPath))return R;
        if(!SourcePin.Equals(MakeProducer?TEXT("Array"):TEXT("OutRowNames"),ESearchCase::CaseSensitive)){Bad(TEXT("exact_producer_pin_role"));return R;}
        if(Inapplicable)return R;
        if(!Shape())return R;
        R->SetBoolField(TEXT("dirty_before"),C.Blueprint->GetPackage()->IsDirty());
        const auto Response=C.Schema->CanCreateConnection(Output,Input);if(!Current())return R;
        R->SetStringField(TEXT("schema_response_name"),Connection::SchemaResponseName(Response.Response));
        R->SetStringField(TEXT("schema_response_message"),Response.Message.ToString());
        if(!Current())return R;
        R->SetBoolField(TEXT("direct_connection_allowed"),Response.Response==CONNECT_RESPONSE_MAKE);
        R->SetBoolField(TEXT("already_linked"),Output->LinkedTo.Contains(Input)&&Input->LinkedTo.Contains(Output));
        R->SetBoolField(TEXT("type_exactly_equal"),SameType(Output->PinType,Input->PinType));
        R->SetBoolField(TEXT("exact_standard_k2_schema"),C.Schema->GetClass()==UEdGraphSchema_K2::StaticClass());
        R->SetStringField(TEXT("schema_message"),R->GetStringField(TEXT("schema_response_message")));
        if(!Current())return R;
        if(!Linked&&Response.Response!=CONNECT_RESPONSE_MAKE){Bad(TEXT("direct_make_schema_response"),TEXT("make"),Connection::SchemaResponseName(Response.Response));return R;}
        auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("eligible"),true);O->SetBoolField(TEXT("native_source_current_verified"),true);
        O->SetBoolField(TEXT("whole_macro_shape_verified"),true);O->SetStringField(TEXT("role"),TEXT("name_array_input"));
        O->SetStringField(TEXT("source_node_path"),SourcePath);O->SetStringField(TEXT("source_node_guid"),Producer->NodeGuid.ToString(EGuidFormats::Digits));
        O->SetStringField(TEXT("target_node_path"),TargetPath);O->SetStringField(TEXT("target_node_guid"),Macro->NodeGuid.ToString(EGuidFormats::Digits));
        O->SetStringField(TEXT("macro_node_path"),TargetPath);O->SetStringField(TEXT("macro_node_guid"),Macro->NodeGuid.ToString(EGuidFormats::Digits));
        O->SetStringField(TEXT("macro_node_class"),Macro->GetClass()->GetPathName());O->SetStringField(TEXT("macro_graph_path"),C.MacroGraph->GetPathName());
        O->SetBoolField(TEXT("first_connection"),!Linked);O->SetBoolField(TEXT("actual_link_verified"),Linked);
        O->SetBoolField(TEXT("specialization_pending"),!Typed);O->SetBoolField(TEXT("allowed_reconstruction"),!Typed&&!Linked);
        // ResolvedWildcardType is current-only evidence, never a constructor/callback cross-plan invariant.
        O->SetObjectField(TEXT("resolved_wildcard_type"),DescribeType(ResolvedType,&C.FiniteCheckpoint));if(!Current())return R;
        O->SetBoolField(TEXT("resolved_wildcard_type_cross_plan_equality_required"),false);
        auto Native=MakeShared<FJsonObject>();
        if(MakeProducer){Native->SetStringField(TEXT("producer_kind"),TEXT("name_make_array"));Native->SetObjectField(TEXT("native_class_source"),MakeClassSource());if(!Current())return R;}
        else
        {
        Native->SetStringField(TEXT("function_path"),NativeFunction->GetPathName());Native->SetStringField(TEXT("function_flags_string"),LexToString(FunctionFlags));
        Native->SetNumberField(TEXT("num_parameters"),ParameterCount);Native->SetNumberField(TEXT("parameter_size"),ParameterSize);Native->SetNumberField(TEXT("return_value_offset"),ReturnOffset);Native->SetNumberField(TEXT("size"),FunctionSize);Native->SetNumberField(TEXT("alignment"),FunctionAlignment);
        TArray<TSharedPtr<FJsonValue>> FieldRows;for(const auto& F:Fields){auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),F.Name.ToString());P->SetStringField(TEXT("kind"),F.Kind->GetName());
            P->SetStringField(TEXT("identity"),LexToString(reinterpret_cast<UPTRINT>(F.Field)));P->SetStringField(TEXT("flags_string"),LexToString(F.Flags));P->SetNumberField(TEXT("offset"),F.Offset);P->SetNumberField(TEXT("size"),F.Size);P->SetNumberField(TEXT("array_dim"),F.Dim);
            P->SetStringField(TEXT("direct_owner_role"),F.Field->GetOwner<FProperty>()?TEXT("array"):TEXT("function"));FieldRows.Add(MakeShared<FJsonValueObject>(P));}
        Native->SetArrayField(TEXT("properties"),FieldRows);
        }
        Native->SetObjectField(TEXT("stock_macro_source"),C.MacroBefore);Native->SetObjectField(TEXT("stock_native_calls"),C.CallsBefore);
        O->SetObjectField(TEXT("native_source"),Native);
        auto Context=MakeShared<FJsonObject>();Context->SetStringField(TEXT("blueprint_path"),C.Blueprint->GetPathName());Context->SetStringField(TEXT("graph_path"),GraphPath);
        Context->SetStringField(TEXT("graph_guid"),C.Graph->GraphGuid.ToString(EGuidFormats::Digits));Context->SetStringField(TEXT("parent_class_path"),S.Parent->GetPathName());
        Context->SetStringField(TEXT("generated_class_path"),S.Generated->GetPathName());Context->SetStringField(TEXT("skeleton_class_path"),S.Skeleton->GetPathName());
        TArray<TSharedPtr<FJsonValue>> ClassRows;
        for(const auto& D:CDOs)
        {
            auto K=MakeShared<FJsonObject>();K->SetStringField(TEXT("class_path"),D.Class->GetPathName());
            K->SetStringField(TEXT("class_identity"),LexToString(reinterpret_cast<UPTRINT>(D.Class.Get())));
            UObject* HeldCDO=nullptr;for(const auto& H:S.Roles)if(D.Handle==H.Hold.Get())HeldCDO=H.Hold.Get();
            K->SetStringField(TEXT("existing_cdo_identity"),LexToString(reinterpret_cast<UPTRINT>(HeldCDO)));
            K->SetStringField(TEXT("existing_cdo_path"),HeldCDO?HeldCDO->GetPathName():FString());ClassRows.Add(MakeShared<FJsonValueObject>(K));
        }
        Context->SetArrayField(TEXT("held_class_roles"),ClassRows);
        if(C.bPresetLibraryForEach){Context->SetStringField(TEXT("function_library_profile"),TEXT("preset_loader"));Context->SetStringField(TEXT("entry_guid"),S.PresetEntryGuid.ToString(EGuidFormats::Digits));Context->SetStringField(TEXT("entry_flags_string"),LexToString(S.PresetEntryFlags));}
        O->SetObjectField(TEXT("target_context"),Context);
        if(MakeProducer)
        {
            auto Proof=MakeShared<FJsonObject>();Proof->SetStringField(TEXT("node_path"),MakePath);Proof->SetStringField(TEXT("node_guid"),Producer->NodeGuid.ToString(EGuidFormats::Digits));Proof->SetStringField(TEXT("node_class"),Producer->GetClass()->GetPathName());Proof->SetNumberField(TEXT("num_inputs"),MakeInputs);
            TArray<TSharedPtr<FJsonValue>> WholePins;for(auto* P:Producer->Pins){WholePins.Add(MakeShared<FJsonValueObject>(PinJson(P,false)));if(!Current())return R;}Proof->SetArrayField(TEXT("pins"),WholePins);Proof->SetObjectField(TEXT("native_class_source"),MakeClassSource());if(!Current())return R;Proof->SetBoolField(TEXT("current_verified"),true);
            O->SetStringField(TEXT("producer_kind"),TEXT("name_make_array"));O->SetObjectField(TEXT("make_array_producer"),Proof);
        }
        O->SetObjectField(TEXT("source_pin"),PinJson(Output,false));if(!Current())return R;
        O->SetObjectField(TEXT("target_pin"),PinJson(Input,false));if(!Current())return R;
        TArray<TSharedPtr<FJsonValue>> Pins,Expected,ExpectedNoReconstruction;
        for(int32 I=0;I<6;++I)
        {
            UEdGraphPin* P=Macro->FindPin(EachPortNames[I],I<2?EGPD_Input:EGPD_Output);
            Pins.Add(MakeShared<FJsonValueObject>(PinJson(P,false)));if(!Current())return R;
            Expected.Add(MakeShared<FJsonValueObject>(PinJson(P,true)));if(!Current())return R;
            ExpectedNoReconstruction.Add(MakeShared<FJsonValueObject>(PinJson(P,true,false)));if(!Current())return R;
        }
        O->SetArrayField(TEXT("macro_pins"),Pins);O->SetArrayField(TEXT("expected_after_macro_pins"),Expected);
        O->SetArrayField(TEXT("expected_after_macro_pins_without_reconstruction"),ExpectedNoReconstruction);
        R->SetObjectField(TEXT("name_foreach_observation"),O);R->SetObjectField(TEXT("source_pin_type"),TypeJson(Output->PinType));R->SetObjectField(TEXT("target_pin_type"),TypeJson(Input->PinType));
        auto Defaults=PinJson(Input,false);if(!Current())return R;
        Defaults->SetBoolField(TEXT("is_orphaned"),Input->bOrphanedPin);
        Defaults->SetBoolField(TEXT("default_value_is_ignored"),Input->bDefaultValueIsIgnored);
        Defaults->SetBoolField(TEXT("default_value_is_read_only"),Input->bDefaultValueIsReadOnly);
        R->SetObjectField(TEXT("target_pin_default_state"),Defaults);if(!Current())return R;
        R->SetStringField(TEXT("graph_path"),GraphPath);R->SetStringField(TEXT("blueprint_path"),C.Blueprint->GetPathName());
        R->SetBoolField(TEXT("dirty_after"),C.Blueprint->GetPackage()->IsDirty());R->SetBoolField(TEXT("observations_complete"),true);
        R->SetBoolField(TEXT("applicable"),true);R->SetBoolField(TEXT("current_verified"),true);
        R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return R;
    }
};
}
TSharedRef<FJsonObject> ReadNameForEachConnection(const FString& GraphPath,const FString& SourceNodePath,
    const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName)
{
    FNameForEachConnection Read;return Read.Run(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);
}
}


namespace Cubeless::UE58::Loop
{
TSharedRef<FJsonObject> ConnectNameForEachPins(const FString& GraphPath,const FString& SourceNodePath,
    const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName)
{
    Function::FResult Result;FNameForEachConnection Read;
    TSharedPtr<FJsonValue> EngineReturn=MakeShared<FJsonValueNull>();
    for(const TCHAR* K:{TEXT("schema_connection_attempts"),TEXT("schema_connection_returns"),TEXT("node_notification_attempts"),TEXT("node_notification_returns"),TEXT("actual_forward_pair_count"),TEXT("actual_reverse_pair_count"),TEXT("verified_macro_pin_count"),TEXT("macro_pin_pointer_replacements")})Result.Data->SetNumberField(K,0);
    Result.Data->SetBoolField(TEXT("native_name_foreach_connection"),true);
    Result.Data->SetBoolField(TEXT("expected_macro_semantics_verified"),false);
    Result.Data->SetField(TEXT("actual_link_verified"),MakeShared<FJsonValueNull>());
    Result.Data->SetNumberField(TEXT("expected_macro_pin_count"),6);
    Result.Data->SetStringField(TEXT("partial_failure_policy"),TEXT("Retain actual schema/notification return and bounded current evidence; stop on first mismatch. No cleanup, rollback, compile, save or automatic replay."));
    auto Finish=[&]()
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("operation"),TEXT("connect_blueprint_graph_pins"));
        J->SetBoolField(TEXT("success"),Result.bSuccess);J->SetStringField(TEXT("stage"),Result.Stage);
        J->SetStringField(TEXT("error_code"),Result.ErrorCode);J->SetStringField(TEXT("error"),Result.Error);
        J->SetBoolField(TEXT("mutation_attempted"),Result.bMutationAttempted);J->SetBoolField(TEXT("change_state_known"),Result.bChangeStateKnown);
        if(Result.bChangeStateKnown)J->SetBoolField(TEXT("changed"),Result.bChanged);else J->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());
        J->SetBoolField(TEXT("postcondition_passed"),Result.bPostconditionPassed);J->SetBoolField(TEXT("compiled"),false);J->SetBoolField(TEXT("saved"),false);
        J->SetField(TEXT("engine_return"),EngineReturn);J->SetObjectField(TEXT("data"),Result.Data);J->SetBoolField(TEXT("auto_retry_safe"),false);return J;
    };
    auto Bad=[&](const TCHAR* At,const FString& Expected=FString(),const FString& Actual=FString())
    {
        if(Result.ErrorCode.IsEmpty())
        {
            Result.ErrorCode=TEXT("name_foreach_connection_scope_changed");
            Result.Error=TEXT("Exact Name ForEach connection current/semantics verification failed; actual dispatched prefix is retained without follow-up SDK or replay.");
            auto F=MakeShared<FJsonObject>();F->SetStringField(TEXT("first_failed_condition"),At);F->SetStringField(TEXT("expected"),Expected);F->SetStringField(TEXT("actual"),Actual);
            Result.Data->SetObjectField(TEXT("first_failure"),F);Result.Data->SetBoolField(TEXT("followup_sdk_stopped"),true);
            Result.Data->SetObjectField(TEXT("native_current_failure"),Read.R);
        }
        Result.bSuccess=Result.bPostconditionPassed=false;
        if(Result.bMutationAttempted)Result.bChangeStateKnown=false;
        return false;
    };
    const auto BeforeRead=Read.Run(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);
    auto Before=MakeShared<FJsonObject>();Before->Values=BeforeRead->Values;
    Result.Data->SetObjectField(TEXT("connection_plan_before"),Before);
    if(!Before->GetBoolField(TEXT("success"))||!Before->GetBoolField(TEXT("applicable")))
    {Bad(TEXT("exact_finite_current_plan"));return Finish();}
    if(!Read.Current()){Bad(TEXT("before_dispatch_current"));return Finish();}
    const auto Observation=Before->GetObjectField(TEXT("name_foreach_observation"));
    if(Read.Linked)
    {
        Result.Data->SetBoolField(TEXT("actual_link_verified"),true);Result.Data->SetBoolField(TEXT("expected_macro_semantics_verified"),true);
        Result.Data->SetNumberField(TEXT("actual_forward_pair_count"),1);Result.Data->SetNumberField(TEXT("actual_reverse_pair_count"),1);Result.Data->SetNumberField(TEXT("verified_macro_pin_count"),6);
        Result.bSuccess=Result.bPostconditionPassed=true;Result.Stage=TEXT("complete");return Finish();
    }
    if(Before->GetStringField(TEXT("schema_response_name"))!=TEXT("make"))
    {Bad(TEXT("direct_make_required"),TEXT("make"),Before->GetStringField(TEXT("schema_response_name")));return Finish();}
    // All old addresses below are opaque keys after either SDK dispatch.
    const auto OriginalNodes=Read.S.Prior;const auto OriginalExtras=Read.ExtraPins;
    const auto* Selected=OriginalNodes.FindByPredicate([&](const auto& N){return N.Hold.Get()==Read.Macro;});
    if(!Selected||Selected->Pins.Num()!=6){Bad(TEXT("selected_six_before_snapshot"));return Finish();}
    const auto OriginalMacro=*Selected;const bool Cold=!Read.Typed;
    const FEdGraphPinType OriginalResolved=Read.ResolvedType;
    UEdGraphPin* const OldOutput=Read.Output;UEdGraphPin* const OldInput=Read.Input;
    UEdGraphNode* const NotificationTarget=Read.Macro;
    UPackage* const TargetPackage=Read.C.Blueprint->GetPackage();
    const bool DirtyBefore=TargetPackage->IsDirty();
    Result.Data->SetBoolField(TEXT("dirty_before"),DirtyBefore);Result.Data->SetNumberField(TEXT("expected_edge_additions"),1);
    // Only held UObject/node/reference/source metadata are inspected here. Raw old
    // pin arrays are temporarily removed so State cannot dereference retired pins.
    auto Held=[&](bool MayHaveReconstructed)
    {
        auto SavedNodes=MoveTemp(Read.S.Prior);auto SavedExtras=MoveTemp(Read.ExtraPins);
        const bool MacroWasReady=Read.MacroReady;Read.MacroReady=false;
        const bool Safe=Read.State();Read.MacroReady=MacroWasReady;
        Read.S.Prior=MoveTemp(SavedNodes);Read.ExtraPins=MoveTemp(SavedExtras);
        if(!Safe)return Bad(TEXT("held_roles_after_dispatch"));
        for(const auto& N:OriginalNodes)
        {
            UEdGraphNode* Node=N.Hold.Get();bool Member=false;
            for(const auto& H:N.Graph->Nodes){if(!H.IsResolved())return Bad(TEXT("current_node_handle"));Member|=H==Node;}
            if(!Member||!Read.S.Live(Node)||Node->GetOuter()!=N.Graph||Node->NodeGuid!=N.Guid||Node->Pins.Num()!=N.Pins.Num())return Bad(TEXT("held_node_identity_after_dispatch"));
            if(N.Call){auto* Call=Cast<UK2Node_CallFunction>(Node);if(!Call||!Call->FunctionReference.IsSameReference(N.Member)||Call->FunctionReference.IsDeprecated()!=N.Member.IsDeprecated())return Bad(TEXT("held_function_reference_after_dispatch"));}
            if(N.Tunnel){auto* Tunnel=Cast<UK2Node_Tunnel>(Node);if(!Tunnel||!Tunnel->InputSinkNode.IsResolved()||!Tunnel->OutputSourceNode.IsResolved()||Tunnel->InputSinkNode!=N.InputSink||Tunnel->OutputSourceNode!=N.OutputSource)return Bad(TEXT("held_tunnel_reference_after_dispatch"));}
            if(!MayHaveReconstructed||Node!=NotificationTarget)
                for(int32 I=0;I<N.Pins.Num();++I)if(Node->Pins[I]!=N.Pins[I].P)return Bad(TEXT("unexpected_physical_pin_inventory_after_dispatch"));
        }
        return true;
    };
    auto SetExpectedDirty=[&]()
    {
        // Schema_K2::TryCreateConnection and aggregate Macro notification call
        // MarkBlueprintAsModified: only the target package dirty and BP status
        // transitions are planned; package flags and every source role stay exact.
        Read.Status=BS_Dirty;
        for(auto& V:Read.S.Roles)if(V.Hold.Get()==TargetPackage)V.WasDirty=true;
    };
    Result.Stage=TEXT("schema_connection");Result.bMutationAttempted=true;Result.bChangeStateKnown=false;
    Result.Data->SetNumberField(TEXT("schema_connection_attempts"),1);
    const bool Returned=Read.C.Schema->TryCreateConnection(OldOutput,OldInput);
    EngineReturn=MakeShared<FJsonValueBoolean>(Returned);Result.Data->SetBoolField(TEXT("schema_engine_return"),Returned);
    Result.Data->SetNumberField(TEXT("schema_connection_returns"),1);if(Returned)Result.bChanged=true;
    if(Returned)SetExpectedDirty();
    if(!Held(false))return Finish();
    bool ReconstructionPending=false;
    if(Returned)
    {
        FEdGraphPinType Names=Read.C.ElementPinType;Names.ContainerType=EPinContainerType::Array;
        // Public metadata is copied only after held-role validation. These are
        // exactly the two installed Notify branches; no protected mode accessor.
        const FEdGraphPinType ObservedResolved=Read.Macro->ResolvedWildcardType;
        const bool ObservedPending=Read.Macro->bReconstructNode;
        if(Cold&&ObservedPending&&Read.SameType(ObservedResolved,Names))
        {Read.ResolvedType=Names;Read.Reconstruct=true;ReconstructionPending=true;}
        else if(!ObservedPending&&Read.SameType(ObservedResolved,OriginalResolved))
        {Read.ResolvedType=OriginalResolved;Read.Reconstruct=false;}
        else{Bad(TEXT("source_derived_macro_notification_intermediate"));return Finish();}
        for(auto& N:Read.S.Prior)for(auto& P:N.Pins)
        {
            if(P.P==OldOutput)P.Links.Add(OldInput);else if(P.P==OldInput)P.Links.Add(OldOutput);
            if(N.Hold.Get()==Read.Macro&&(P.Name==TEXT("Array")||P.Name==TEXT("Array Element")))
            {P.Type.PinCategory=UEdGraphSchema_K2::PC_Name;P.Type.PinSubCategory=NAME_None;P.Type.PinSubCategoryObject.Reset();}
        }
    }
    if(!Read.Current()){Bad(TEXT("schema_intermediate_full_current"));return Finish();}
    Result.Data->SetBoolField(TEXT("schema_intermediate_current_verified"),true);
    Result.Data->SetBoolField(TEXT("observed_intermediate_reconstruction_pending"),ReconstructionPending);
    Result.Data->SetBoolField(TEXT("actual_link_verified"),Returned);
    if(!Returned)
    {
        Result.bChangeStateKnown=true;Result.bChanged=false;Result.ErrorCode=TEXT("name_foreach_connection_rejected");
        Result.Error=TEXT("Schema returned false and the complete original current state was preserved; no notification or retry followed.");return Finish();
    }
    Result.Data->SetNumberField(TEXT("actual_forward_pair_count"),1);Result.Data->SetNumberField(TEXT("actual_reverse_pair_count"),1);
    Result.Stage=TEXT("macro_node_notification");Result.Data->SetNumberField(TEXT("node_notification_attempts"),1);
    // Exported public base virtual dispatch; no direct MinimalAPI override call.
    NotificationTarget->NodeConnectionListChanged();
    Result.Data->SetNumberField(TEXT("node_notification_returns"),1);
    if(!Held(ReconstructionPending))return Finish();
    const TArray<UEdGraphPin*> CurrentPins=NotificationTarget->Pins;
    if(CurrentPins.Num()!=6){Bad(TEXT("notified_macro_current_six_count"),TEXT("6"),LexToString(CurrentPins.Num()));return Finish();}
    TMap<UEdGraphPin*,UEdGraphPin*> Mapping;TMap<UEdGraphPin*,FGuid> NewIds;TSet<UEdGraphPin*> SeenPins;TSet<FGuid> SeenIds;int32 Replacements=0;
    for(const auto& Old:OriginalMacro.Pins)
    {
        UEdGraphPin* Found=nullptr;
        for(UEdGraphPin* P:CurrentPins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=NotificationTarget)continue;
            if(P->PinName==Old.Name&&P->Direction==Old.Direction){if(Found){Bad(TEXT("notified_macro_duplicate_semantic_role"),Old.Name.ToString());return Finish();}Found=P;}
        }
        if(!Found||!Found->PinId.IsValid()||SeenPins.Contains(Found)||SeenIds.Contains(Found->PinId)
            ||(!ReconstructionPending&&(Found!=Old.P||Found->PinId!=Old.Id)))
        {Bad(TEXT("notified_macro_semantic_identity"),Old.Name.ToString());return Finish();}
        SeenPins.Add(Found);SeenIds.Add(Found->PinId);Mapping.Add(Old.P,Found);NewIds.Add(Old.P,Found->PinId);
        Replacements+=Found!=Old.P?1:0;
    }
    Result.Data->SetNumberField(TEXT("macro_pin_pointer_replacements"),Replacements);
    // Rebuild only source-derived expected state. No arbitrary after snapshot is adopted.
    Read.S.Prior=OriginalNodes;Read.ExtraPins=OriginalExtras;
    for(auto& N:Read.S.Prior)
    {
        for(auto& P:N.Pins)
        {
            UEdGraphPin* const Old=P.P;
            if(Old==OldOutput)P.Links.Add(OldInput);else if(Old==OldInput)P.Links.Add(OldOutput);
            for(auto*& L:P.Links)if(const auto* M=Mapping.Find(L))L=*M;
            if(N.Hold.Get()==Read.Macro)
            {
                if(P.Name==TEXT("Array")||P.Name==TEXT("Array Element"))
                {P.Type.PinCategory=UEdGraphSchema_K2::PC_Name;P.Type.PinSubCategory=NAME_None;P.Type.PinSubCategoryObject.Reset();}
                P.P=Mapping.FindChecked(Old);P.Id=NewIds.FindChecked(Old);
            }
        }
        if(N.Hold.Get()==Read.Macro)
        {
            TArray<FFiniteForEachScope::FPinRole> Ordered;
            for(UEdGraphPin* P:CurrentPins)
            {
                const auto* E=N.Pins.FindByPredicate([&](const auto& V){return V.P==P;});
                if(!E){Bad(TEXT("notified_macro_expected_role_mapping"));return Finish();}Ordered.Add(*E);
            }
            N.Pins=MoveTemp(Ordered);
        }
    }
    for(auto& E:Read.ExtraPins)if(E.Node==Read.Macro)
    {
        E.Pin=Mapping.FindChecked(E.Pin);
        if(ReconstructionPending)
        {E.Flags|=(1U<<9);E.Persistent.Invalidate();E.Friendly=FText::GetEmpty();E.Tooltip.Reset();E.Deprecated.Reset();E.PassThrough=nullptr;}
    }
    Read.Input=Mapping.FindChecked(OldInput);Read.Reconstruct=false;
    if(!Read.Current()){Bad(TEXT("notified_macro_full_expected_current"));return Finish();}
    if(!Read.Shape()||!Read.Typed||!Read.Linked){Bad(TEXT("notified_macro_full_typed_linked_shape"));return Finish();}
    FNameForEachConnection AfterRead;
    const auto After=AfterRead.Run(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);
    Result.Data->SetObjectField(TEXT("connection_plan_after"),After);
    if(!After->GetBoolField(TEXT("success"))||!After->GetBoolField(TEXT("applicable")))
    {Bad(TEXT("notified_macro_final_current_plan"));return Finish();}
    if(!Read.Current()){Bad(TEXT("final_plan_outer_current"));return Finish();}
    const auto AO=After->GetObjectField(TEXT("name_foreach_observation"));
    if(!AO->GetBoolField(TEXT("actual_link_verified"))||AO->GetBoolField(TEXT("specialization_pending")))
    {Bad(TEXT("notified_macro_final_actual_link"));return Finish();}
    for(const TCHAR* K:{TEXT("native_source"),TEXT("target_context")})
        if(!FJsonValue::CompareEqual(FJsonValueObject(Observation->GetObjectField(K).ToSharedRef()),FJsonValueObject(AO->GetObjectField(K).ToSharedRef())))
        {Bad(TEXT("notified_macro_source_context_preserved"),K);return Finish();}
    const TCHAR* Projection=ReconstructionPending?TEXT("expected_after_macro_pins"):TEXT("expected_after_macro_pins_without_reconstruction");
    int32 Verified=0;
    for(const auto& V:Observation->GetArrayField(Projection))
    {
        const auto E=V->AsObject();const auto* A=AO->GetArrayField(TEXT("macro_pins")).FindByPredicate([&](const auto& X)
        {return X->AsObject()->GetStringField(TEXT("name")).Equals(E->GetStringField(TEXT("name")),ESearchCase::CaseSensitive)&&X->AsObject()->GetStringField(TEXT("direction"))==E->GetStringField(TEXT("direction"));});
        if(!A){Bad(TEXT("notified_macro_expected_full_pin"));return Finish();}
        auto Copied=MakeShared<FJsonObject>();Copied->Values=(*A)->AsObject()->Values;Copied->RemoveField(TEXT("pin_id"));Copied->RemoveField(TEXT("links"));
        if(!FJsonValue::CompareEqual(FJsonValueObject(E.ToSharedRef()),FJsonValueObject(Copied)))
        {Bad(TEXT("notified_macro_expected_full_pin"),E->GetStringField(TEXT("name")));return Finish();}
        ++Verified;
    }
    if(Verified!=6||!Read.Current()){Bad(TEXT("final_expected_six_current"));return Finish();}
    Result.Data->SetNumberField(TEXT("verified_macro_pin_count"),Verified);
    Result.Data->SetBoolField(TEXT("actual_link_verified"),true);Result.Data->SetBoolField(TEXT("expected_macro_semantics_verified"),true);
    Result.Data->SetBoolField(TEXT("dirty_after"),Read.C.Blueprint->GetPackage()->IsDirty());
    Result.bSuccess=Result.bPostconditionPassed=Result.bChangeStateKnown=Result.bChanged=true;Result.Stage=TEXT("complete");return Finish();
}
}
