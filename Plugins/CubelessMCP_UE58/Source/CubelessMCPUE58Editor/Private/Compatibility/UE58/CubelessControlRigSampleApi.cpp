#include "Compatibility/UE58/CubelessControlRigSampleApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "Blueprint/BlueprintExtension.h"
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_ModifyCurve.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigBlueprintFactory.h"
#include "ControlRigBlueprintEditorLibrary.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "Rigs/RigHierarchyController.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "EditorAssetLibrary.h"
#include "Compatibility/UE58/CubelessAnimGraphInspectionApi.h"
#include "RigVMModel/RigVMPin.h"
#include "Units/Execution/RigUnit_Item.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimNode_ControlRig.h"
#include "ControlRig.h"
#include "ControlRigAssetReference.h"
#include "ControlRigRuntimeAsset.h"
#include "Rigs/RigHierarchy.h"
#include "Animation/Skeleton.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_Root.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
namespace Cubeless::UE58::ControlRigSample
{
namespace
{
using Variable::FResult;
constexpr int32 AuthoringGraphBudget=4096,AuthoringNodeBudget=4096,AuthoringPinBudget=32768,AuthoringJsonBudget=1024*1024;
FResult* CurrentResult=nullptr;UBlueprint* CurrentBlueprint=nullptr;TSet<const UEdGraphPin*> KnownPins;int32 PropertyValues=0;UClass* AnimBase=nullptr;UScriptStruct* AnimStruct=nullptr;UScriptStruct* PoseStruct=nullptr;UScriptStruct* ComponentPoseStruct=nullptr;
TArray<TStrongObjectPtr<UObject>> Held;TSet<const UObject*> HeldObjects;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeText(const FString& S,int32 Limit) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(FResult& R,const TCHAR* Key,int32 N=1) { auto J=R.Data->GetObjectField(TEXT("native_call_counts"));double V=0;J->TryGetNumberField(Key,V);J->SetNumberField(Key,V+N); }
void NoteUnsafe(const TCHAR* Error) { if(CurrentResult) { CurrentResult->Data->SetBoolField(TEXT("observation_incomplete"),true);CurrentResult->ErrorCode=TEXT("controlrig_sample_observation_incomplete");CurrentResult->Error=Error; } }
bool Incomplete() { bool V=false;return CurrentResult&&CurrentResult->Data->TryGetBoolField(TEXT("observation_incomplete"),V)&&V; }
bool Hold(const UObject* O)
{ if(!O) { return true; }if(!Loaded(O)||!Loaded(O->GetClass())) { NoteUnsafe(TEXT("Current object/class unavailable"));return false; }if(!HeldObjects.Contains(O)) { if(HeldObjects.Num()>=32768) { NoteUnsafe(TEXT("Strong source-reference bound exceeded"));return false; }HeldObjects.Add(O);Held.Emplace(const_cast<UObject*>(O)); }return true; }
bool PinKnown(const UEdGraphPin* P)
{
    if(!P) { return false; }if(CurrentResult) { Count(*CurrentResult,TEXT("pins_visited")); }
    if(!KnownPins.Contains(P)) { NoteUnsafe(TEXT("Pin is not a current inventoried address; no unsafe pointer dereference"));return false; }
    if(P->WasTrashed()) { NoteUnsafe(TEXT("Trashed primary/linked pin prevents complete current proof"));return false; }
    auto* Node=P->GetOwningNodeUnchecked();if(!Loaded(Node)||!Node->Pins.Contains(const_cast<UEdGraphPin*>(P))) { NoteUnsafe(TEXT("Pin owner/array identity mismatch"));return false; }return true;
}
bool RefreshInventory();
TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Error) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Error);return J; }
template<class T> T* CountedLoad(const FString& Path,FResult& R)
{ if(!SafeText(Path,NAME_SIZE-1)) { NoteUnsafe(TEXT("Composed Blueprint/class path exceeds safe FName load bound"));return nullptr; }Count(R,TEXT("asset_load_calls"));T* O=LoadObject<T>(nullptr,*Path);Count(R,TEXT("asset_load_returns"));if(!Hold(O)) { return nullptr; }return O; }
bool Layout(const FProperty* P,int32 Size) { return P&&Size>=0&&P->GetOffset_ForInternal()>=0&&P->GetSize()>0&&P->GetOffset_ForInternal()<=Size&&P->GetSize()<=Size-P->GetOffset_ForInternal(); }
bool ValueBudget() { if(CurrentResult) { Count(*CurrentResult,TEXT("reflected_property_reads")); }if(++PropertyValues>65536) { NoteUnsafe(TEXT("Reflected settings value budget exceeded"));return false; }return true; }
FString ValueText(FString S) { if(!SafeText(S,65536)) { NoteUnsafe(TEXT("Reflected text exceeds source bound"));S.LeftInline(65536); }return S; }
bool BoundedGraphTree(UBlueprint* BP)
{
    if(!Loaded(BP)) { NoteUnsafe(TEXT("Blueprint unavailable before SDK recursive enumeration"));return false; }
    // Stock GetAllGraphs also invokes extension graph producers. These installed native modules
    // have no GetAllGraphs override; their extension hierarchies inherit Engine's public no-op.
    const auto Extensions=BP->GetExtensions();if(Extensions.Num()>AuthoringGraphBudget) { NoteUnsafe(TEXT("Blueprint extension array exceeds4096"));return false; }
    for(const UBlueprintExtension* Extension:Extensions)
    {
        if(!Extension) { continue; }if(!Loaded(Extension)) { NoteUnsafe(TEXT("Unloaded Blueprint extension before SDK enumeration"));return false; }
        for(UClass* C=Extension->GetClass();C&&C!=UBlueprintExtension::StaticClass();C=C->GetSuperClass())
        {
            if(!Loaded(C)) { NoteUnsafe(TEXT("Extension class unavailable before SDK graph producer"));return false; }
            const FString Package=C->GetOutermost()->GetName();
            if(!C->IsNative()||(Package!=TEXT("/Script/Engine")&&Package!=TEXT("/Script/AnimGraph")&&Package!=TEXT("/Script/UMGEditor")))
            { NoteUnsafe(TEXT("Unaudited extension graph producer outside finite native no-op hierarchy"));return false; }
        }
    }
    int32 Visits=0;TSet<const UEdGraph*> Path;
    TFunction<bool(UEdGraph*,int32)> Visit=[&](UEdGraph* G,int32 Depth)
    {
        if(!Loaded(G)||Depth>32||++Visits>AuthoringGraphBudget||Path.Contains(G))
        { NoteUnsafe(TEXT("Unloaded/cyclic/deep graph tree or more than4096 recursive graph occurrences before SDK traversal"));return false; }
        Path.Add(G);for(UEdGraph* Child:G->SubGraphs) { if(!Visit(Child,Depth+1)) { return false; } }Path.Remove(G);return true;
    };
    auto Roots=[&](const TArray<TObjectPtr<UEdGraph>>& Array)
    { if(Array.Num()>AuthoringGraphBudget) { NoteUnsafe(TEXT("Blueprint root graph array exceeds4096"));return false; }for(UEdGraph* G:Array) { if(G&&!Visit(G,0)) { return false; } }return true; };
    // Preserve SDK root/child order and multiplicity; this read only validates, never deduplicates/replaces traversal.
    if(!Roots(BP->FunctionGraphs)||!Roots(BP->MacroGraphs)||!Roots(BP->UbergraphPages)||!Roots(BP->DelegateSignatureGraphs)) { return false; }
    if(BP->ImplementedInterfaces.Num()>AuthoringGraphBudget) { NoteUnsafe(TEXT("Blueprint interface array exceeds4096"));return false; }
    for(const auto& Interface:BP->ImplementedInterfaces) { if(!Roots(Interface.Graphs)) { return false; } }
    return true;
}

FString NormalizeBlueprintObjectPath(const FString& BlueprintNameOrPath)
{
    FString Path = FPackageName::ExportTextPathToObjectPath(BlueprintNameOrPath).TrimStartAndEnd();
    Path.TrimQuotesInline();

    if (Path.EndsWith(TEXT("_C")))
    {
        Path.LeftChopInline(2);
    }

    if ((Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/"))) && !Path.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(Path);
        Path = FString::Printf(TEXT("%s.%s"), *Path, *AssetName);
    }

    return Path;
}

UBlueprint* LoadBlueprintFromObjectPath(const FString& ObjectPath,FResult& R)
{
    if (ObjectPath.IsEmpty())
    {
        return nullptr;
    }

    const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
    FText InvalidPackageReason;
    if (PackageName.IsEmpty() ||
        ObjectPath.Contains(TEXT("//")) ||
        PackageName.Contains(TEXT("//")) ||
        !FPackageName::IsValidLongPackageName(PackageName, true, &InvalidPackageReason))
    {
        UE_LOG(LogTemp, Warning, TEXT("Skipping invalid blueprint object path '%s': %s"), *ObjectPath, *InvalidPackageReason.ToString());
        return nullptr;
    }

    if (UBlueprint* Blueprint = CountedLoad<UBlueprint>(ObjectPath,R))
    {
        return Blueprint;
    }

    FString ClassPath = ObjectPath;
    if (!ClassPath.EndsWith(TEXT("_C")))
    {
        ClassPath += TEXT("_C");
    }

    if (UClass* BlueprintClass = CountedLoad<UClass>(ClassPath,R))
    {
        return Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy);
    }

    return nullptr;
}

UBlueprint* LoadExistingLevelBlueprintFromWorldPath(const FString& ObjectPath,FResult& R)
{
    if(ObjectPath.IsEmpty()) { return nullptr; }if(!SafeText(ObjectPath,NAME_SIZE-1)) { NoteUnsafe(TEXT("World path exceeds safe FName bound"));return nullptr; }
    UWorld* World=FindObject<UWorld>(nullptr,*ObjectPath);
    if(!World) { World=CountedLoad<UWorld>(ObjectPath,R); }
    if(!Loaded(World)||!Loaded(World->PersistentLevel)) { return nullptr; }
    // Actual SDK argument is bDontCreate. Preserve the original true=no-create behavior.
    if(ULevelScriptBlueprint* B=World->PersistentLevel->GetLevelScriptBlueprint(true)) { return B; }
    return FindObject<ULevelScriptBlueprint>(World->PersistentLevel,*World->GetName());
}

TArray<FString> FindBlueprintAssetPaths(const FString& BlueprintNameOrPath,FResult& R)
{
    TArray<FString> CandidatePaths;
    const FString Query = NormalizeBlueprintObjectPath(BlueprintNameOrPath);

    if (Query.StartsWith(TEXT("/Game/")) || Query.StartsWith(TEXT("/Engine/")))
    {
        if (LoadBlueprintFromObjectPath(Query,R))
        {
            CandidatePaths.Add(Query);
            return CandidatePaths;
        }
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> BlueprintAssets;
    AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets, true);

    if(BlueprintAssets.Num()>4096) { NoteUnsafe(TEXT("Blueprint registry search result exceeds4096; full scan allocation effects unverified"));return CandidatePaths; }
    const FString ShortQuery = FPackageName::GetShortName(Query);
    for (const FAssetData& AssetData : BlueprintAssets)
    {
        const FString AssetName = AssetData.AssetName.ToString();
        const FString PackageName = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();

        if (AssetName.Equals(BlueprintNameOrPath, ESearchCase::IgnoreCase) ||
            AssetName.Equals(ShortQuery, ESearchCase::IgnoreCase) ||
            PackageName.Equals(BlueprintNameOrPath, ESearchCase::IgnoreCase) ||
            ObjectPath.Equals(Query, ESearchCase::IgnoreCase))
        {
            CandidatePaths.Add(ObjectPath);
        }
    }

    return CandidatePaths;
}

UBlueprint* FindBlueprintByName(const FString& BlueprintName,FResult& R)
{
    const FString NormalizedPath = NormalizeBlueprintObjectPath(BlueprintName);
    if (UBlueprint* DirectBlueprint = LoadBlueprintFromObjectPath(NormalizedPath,R))
    {
        return DirectBlueprint;
    }

    if (UBlueprint* LevelBlueprint = LoadExistingLevelBlueprintFromWorldPath(NormalizedPath,R))
    {
        return LevelBlueprint;
    }

    const bool bIsPathLikeInput = BlueprintName.Contains(TEXT("/")) || BlueprintName.Contains(TEXT("."));
    if (!bIsPathLikeInput)
    {
        const FString LegacyObjectPath = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *BlueprintName, *BlueprintName);
        if (UBlueprint* LegacyBlueprint = LoadBlueprintFromObjectPath(LegacyObjectPath,R))
        {
            return LegacyBlueprint;
        }
    }

    const TArray<FString> CandidatePaths = FindBlueprintAssetPaths(BlueprintName,R);
    if (CandidatePaths.Num() == 1)
    {
        return LoadBlueprintFromObjectPath(CandidatePaths[0],R);
    }

    if (CandidatePaths.Num() > 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Ambiguous blueprint name '%s'. Use a full asset path. Candidates:"), *BlueprintName);
        for (const FString& CandidatePath : CandidatePaths)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - %s"), *CandidatePath);
        }
    }

    return nullptr;
}

FString NormalizeGraphType(const FString& GraphType)
{
    if (GraphType.IsEmpty())
    {
        return FString();
    }

    FString Normalized = GraphType.ToLower();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT("_"));

    if (Normalized == TEXT("eventgraph") || Normalized == TEXT("event_graph") || Normalized == TEXT("ubergraph") || Normalized == TEXT("uber_graph"))
    {
        return TEXT("event");
    }
    if (Normalized == TEXT("functiongraph") || Normalized == TEXT("function_graph"))
    {
        return TEXT("function");
    }
    if (Normalized == TEXT("macrograph") || Normalized == TEXT("macro_graph"))
    {
        return TEXT("macro");
    }

    return Normalized;
}

bool BlueprintGraphArrayContains(const TArray<TObjectPtr<UEdGraph>>& GraphArray, const UEdGraph* Graph)
{
    for (const TObjectPtr<UEdGraph>& CandidateGraph : GraphArray)
    {
        if (CandidateGraph.Get() == Graph)
        {
            return true;
        }
    }

    return false;
}

FString GetBlueprintGraphType(const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    if (!Blueprint || !Graph)
    {
        return TEXT("unknown");
    }

    if (BlueprintGraphArrayContains(Blueprint->FunctionGraphs, Graph))
    {
        return TEXT("function");
    }
    if (BlueprintGraphArrayContains(Blueprint->MacroGraphs, Graph))
    {
        return TEXT("macro");
    }
    if (BlueprintGraphArrayContains(Blueprint->UbergraphPages, Graph) || BlueprintGraphArrayContains(Blueprint->EventGraphs, Graph))
    {
        return TEXT("event");
    }
    if (BlueprintGraphArrayContains(Blueprint->DelegateSignatureGraphs, Graph))
    {
        return TEXT("delegate");
    }

    return TEXT("unknown");
}

bool GraphMatchesSelector(const UBlueprint* Blueprint, const UEdGraph* Graph, const FString& GraphId, const FString& GraphName, const FString& GraphType)
{
    if (!Loaded(Graph))
    {
        return false;
    }

    if (!GraphId.IsEmpty())
    {
        FGuid ParsedGraphId;
        if (FGuid::Parse(GraphId, ParsedGraphId))
        {
            if (Graph->GraphGuid != ParsedGraphId)
            {
                return false;
            }
        }
        else if (Graph->GraphGuid.ToString() != GraphId)
        {
            return false;
        }
    }

    if (!GraphName.IsEmpty() && !Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
    {
        return false;
    }

    const FString NormalizedType = NormalizeGraphType(GraphType);
    if (!NormalizedType.IsEmpty() && NormalizedType != TEXT("any") && GetBlueprintGraphType(Blueprint, Graph) != NormalizedType)
    {
        return false;
    }

    return true;
}

TSharedPtr<FJsonObject> GraphToJson(const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
    if (!Graph)
    {
        return GraphObject;
    }

    GraphObject->SetStringField(TEXT("graph_id"), Graph->GraphGuid.ToString());
    GraphObject->SetStringField(TEXT("graph_name"), Graph->GetName());
    GraphObject->SetStringField(TEXT("graph_type"), GetBlueprintGraphType(Blueprint, Graph));
    GraphObject->SetStringField(TEXT("schema"), Graph->GetSchema() ? Graph->GetSchema()->GetClass()->GetName() : FString());
    GraphObject->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
    return GraphObject;
}

FString GetStringParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, const FString& DefaultValue = FString())
{
    if (!Params.IsValid())
    {
        return DefaultValue;
    }

    FString Value;
    return Params->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, bool bDefaultValue = false)
{
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return bDefaultValue;
    }
    return Params->GetBoolField(FieldName);
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
    if (PinType.PinSubCategoryObject.IsValid()&&!Hold(PinType.PinSubCategoryObject.Get())) { return TypeObject; }
    if (PinType.PinSubCategoryObject.IsValid())
    {
        TypeObject->SetStringField(TEXT("subcategory_object"), PinType.PinSubCategoryObject->GetPathName());
    }
    return TypeObject;
}

TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (Pin && !PinKnown(Pin))
    {
        PinObject->SetBoolField(TEXT("observation_incomplete"), true);
        return PinObject;
    }
    if(Pin&&(!Hold(Pin->PinType.PinSubCategoryObject.Get())||!Hold(Pin->DefaultObject))) { return PinObject; }
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
            if (!PinKnown(LinkedPin))
            {
                continue;
            }

            TSharedPtr<FJsonObject> LinkedPinObject = MakeShared<FJsonObject>();
            LinkedPinObject->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNodeUnchecked()->NodeGuid.ToString());
            LinkedPinObject->SetStringField(TEXT("node_name"), LinkedPin->GetOwningNodeUnchecked()->GetName());
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
    if (Node&&!Hold(Node)) { return NodeObject; }
    if(CurrentResult) { Count(*CurrentResult,TEXT("nodes_serialized")); }
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

FString ExportPropertyValueToText(const FProperty* Property, const void* ValuePtr)
{
    if (!Property || !ValuePtr)
    {
        return FString();
    }

    if(const auto* P=CastField<FMapProperty>(Property);P&&FScriptMapHelper(P,ValuePtr).Num()>1024) { NoteUnsafe(TEXT("Map export exceeds1024"));return FString(); }
    if(const auto* P=CastField<FSetProperty>(Property);P&&FScriptSetHelper(P,ValuePtr).Num()>1024) { NoteUnsafe(TEXT("Set export exceeds1024"));return FString(); }
    FString ExportedValue;
    if(CurrentResult) { Count(*CurrentResult,TEXT("property_export_calls")); }Property->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
    if(ExportedValue.Len()>65536) { NoteUnsafe(TEXT("Property export text exceeds finite observer budget"));ExportedValue.LeftInline(65536); }
    return ExportedValue;
}

int64 GetIntegerPropertyValueAsInt64(const FNumericProperty* NumericProperty, const void* ValuePtr)
{
    if (!NumericProperty || !ValuePtr)
    {
        return 0;
    }

    if (CastField<FUInt64Property>(NumericProperty) ||
        CastField<FUInt32Property>(NumericProperty) ||
        CastField<FUInt16Property>(NumericProperty))
    {
        const uint64 Bits=NumericProperty->GetUnsignedIntPropertyValue(ValuePtr);int64 Signed=0;FMemory::Memcpy(&Signed,&Bits,sizeof(Signed));return Signed; // Explicit original signed-bit projection, not exact decimal JSON guarantee.
    }

    return NumericProperty->GetSignedIntPropertyValue(ValuePtr);
}

TSharedPtr<FJsonValue> PropertyValueToJson(const FProperty* Property,const void* ValuePtr,int32 Depth,int32 MaxDepth);

TSharedPtr<FJsonObject> StructValueToJson(const UScriptStruct* Struct, const void* StructValuePtr, int32 Depth, int32 MaxDepth)
{
    TSharedPtr<FJsonObject> StructObject = MakeShared<FJsonObject>();
    if(Struct&&!Hold(Struct)) { return StructObject; }
    if (!Struct || !StructValuePtr)
    {
        return StructObject;
    }

    StructObject->SetStringField(TEXT("_struct_type"), Struct->GetName());
    if (Depth >= MaxDepth)
    {
        return StructObject;
    }

    for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::IncludeSuper); It; ++It)
    {
        const FProperty* Property = *It;
        if (!Property)
        {
            continue;
        }

        if(!Layout(Property,Struct->GetStructureSize())) { NoteUnsafe(TEXT("Reflected property does not fit actual struct container"));continue; }
        const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructValuePtr);
        StructObject->SetField(Property->GetName(), PropertyValueToJson(Property, ValuePtr, Depth + 1, MaxDepth));
    }

    return StructObject;
}

TSharedPtr<FJsonValue> EnumValueToJson(const UEnum* Enum, int64 RawValue)
{
    TSharedPtr<FJsonObject> EnumObject = MakeShared<FJsonObject>();
    EnumObject->SetNumberField(TEXT("value"), static_cast<double>(RawValue));
    if (Enum)
    {
        EnumObject->SetStringField(TEXT("name"), Enum->GetNameStringByValue(RawValue));
        EnumObject->SetStringField(TEXT("enum_type"), Enum->GetName());
    }
    return MakeShared<FJsonValueObject>(EnumObject);
}

TSharedPtr<FJsonValue> PropertyValueToJson(const FProperty* Property, const void* ValuePtr, int32 Depth, int32 MaxDepth)
{
    if(Depth>8) { NoteUnsafe(TEXT("Reflection depth exceeds8"));return MakeShared<FJsonValueNull>(); }
    if(!ValueBudget()) { return MakeShared<FJsonValueNull>(); }
    if (!Property || !ValuePtr)
    {
        return MakeShared<FJsonValueNull>();
    }

    if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BoolProperty->GetPropertyValue(ValuePtr));
    }

    if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        return EnumValueToJson(
            EnumProperty->GetEnum(),
            GetIntegerPropertyValueAsInt64(EnumProperty->GetUnderlyingProperty(), ValuePtr));
    }

    if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
    {
        const uint8 ByteValue = ByteProperty->GetPropertyValue(ValuePtr);
        if (ByteProperty->Enum)
        {
            return EnumValueToJson(ByteProperty->Enum, ByteValue);
        }
        return MakeShared<FJsonValueNumber>(ByteValue);
    }

    if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
    {
        if (NumericProperty->IsFloatingPoint())
        {
            const double Number=NumericProperty->GetFloatingPointPropertyValue(ValuePtr);if(!FMath::IsFinite(Number)) { NoteUnsafe(TEXT("Nonfinite reflected numeric setting cannot be a truthful JSON number"));return MakeShared<FJsonValueNull>(); }return MakeShared<FJsonValueNumber>(Number);
        }
        return MakeShared<FJsonValueNumber>(static_cast<double>(GetIntegerPropertyValueAsInt64(NumericProperty, ValuePtr)));
    }

    if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(ValueText(NameProperty->GetPropertyValue(ValuePtr).ToString()));
    }

    if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(ValueText(StringProperty->GetPropertyValue(ValuePtr)));
    }

    if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
    {
        return MakeShared<FJsonValueString>(ValueText(TextProperty->GetPropertyValue(ValuePtr).ToString()));
    }

    if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
    {
        if (const UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(ValuePtr))
        {
            if(!Hold(ObjectValue)) { return MakeShared<FJsonValueNull>(); }
            return MakeShared<FJsonValueString>(ObjectValue->GetPathName());
        }
        return MakeShared<FJsonValueNull>();
    }

    if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
    {
        return MakeShared<FJsonValueString>(SoftObjectProperty->GetPropertyValue(ValuePtr).ToString());
    }

    if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Property))
    {
        return MakeShared<FJsonValueString>(SoftClassProperty->GetPropertyValue(ValuePtr).ToString());
    }

    if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        if (Depth >= MaxDepth)
        {
            TSharedPtr<FJsonObject> StructSummary = MakeShared<FJsonObject>();
            StructSummary->SetStringField(TEXT("_struct_type"), StructProperty->Struct ? StructProperty->Struct->GetName() : FString());
            StructSummary->SetStringField(TEXT("_export_text"), ExportPropertyValueToText(Property, ValuePtr));
            return MakeShared<FJsonValueObject>(StructSummary);
        }
        return MakeShared<FJsonValueObject>(StructValueToJson(StructProperty->Struct, ValuePtr, Depth, MaxDepth));
    }

    if(Depth>=MaxDepth) { return MakeShared<FJsonValueString>(ExportPropertyValueToText(Property,ValuePtr)); }
    if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
    {
        TArray<TSharedPtr<FJsonValue>> ArrayValues;
        FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
        if(Helper.Num()<0||Helper.Num()>65536) { NoteUnsafe(TEXT("Dynamic settings array exceeds finite observer budget"));return MakeShared<FJsonValueArray>(ArrayValues); }
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            ArrayValues.Add(PropertyValueToJson(ArrayProperty->Inner, Helper.GetRawPtr(Index), Depth + 1, MaxDepth));
        }
        return MakeShared<FJsonValueArray>(ArrayValues);
    }

    return MakeShared<FJsonValueString>(ExportPropertyValueToText(Property, ValuePtr));
}

bool IsAnimNode(const UEdGraphNode* N) { return Loaded(N)&&Loaded(AnimBase)&&N->IsA(AnimBase); }

FStructProperty* ReflectedNodeProperty(UEdGraphNode* Node)
{
    if(CurrentResult) { Count(*CurrentResult,TEXT("node_struct_property_search_calls")); }
    for(TFieldIterator<FProperty> It(Node->GetClass(),EFieldIterationFlags::IncludeSuper);It;++It)
    { if(FStructProperty* P=CastField<FStructProperty>(*It)) { if(Loaded(P->Struct)&&P->Struct->IsChildOf(AnimStruct)) { return P; } } }return nullptr;
}

TSharedPtr<FJsonObject> AnimGraphNodeSettingsToJson(UEdGraphNode* Node,int32 MaxDepth)
{
    auto J=MakeShared<FJsonObject>();if(!IsAnimNode(Node)) { J->SetBoolField(TEXT("has_anim_node_struct"),false);return J; }
    FStructProperty* P=ReflectedNodeProperty(Node);if(!P||!P->Struct) { J->SetBoolField(TEXT("has_anim_node_struct"),false);return J; }
    if(!Layout(P,Node->GetClass()->GetPropertiesSize())) { NoteUnsafe(TEXT("Actual anim struct property exceeds its native UObject layout"));J->SetBoolField(TEXT("has_anim_node_struct"),false);return J; }
    const void* Value=P->ContainerPtrToValuePtr<void>(Node);J->SetBoolField(TEXT("has_anim_node_struct"),true);J->SetStringField(TEXT("anim_node_property"),P->GetName());J->SetStringField(TEXT("anim_node_struct_type"),P->Struct->GetName());J->SetObjectField(TEXT("anim_node_properties"),StructValueToJson(P->Struct,Value,0,MaxDepth));return J;
}

FString GetConnectionResponseName(ECanCreateConnectionResponse Response)
{
    switch (Response)
    {
    case CONNECT_RESPONSE_MAKE:
        return TEXT("make");
    case CONNECT_RESPONSE_DISALLOW:
        return TEXT("disallow");
    case CONNECT_RESPONSE_BREAK_OTHERS_A:
        return TEXT("break_others_a");
    case CONNECT_RESPONSE_BREAK_OTHERS_B:
        return TEXT("break_others_b");
    case CONNECT_RESPONSE_BREAK_OTHERS_AB:
        return TEXT("break_others_ab");
    case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:
        return TEXT("make_with_conversion_node");
    case CONNECT_RESPONSE_MAKE_WITH_PROMOTION:
        return TEXT("make_with_promotion");
    default:
        return TEXT("unknown");
    }
}

void AddGraphField(TSharedPtr<FJsonObject> ResultObj, const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    if (ResultObj.IsValid() && Graph)
    {
        ResultObj->SetObjectField(TEXT("graph"), GraphToJson(Blueprint, Graph));
    }
}

FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

bool JsonSafe(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Items,int64& Chars)
{
    if(!V.IsValid()||Depth>32||++Items>100000) { NoteUnsafe(TEXT("Native JSON structure budget exceeded"));return false; }
    if(V->Type==EJson::Number&&!FMath::IsFinite(V->AsNumber())) { NoteUnsafe(TEXT("Nonfinite current numeric value"));return false; }
    if(V->Type==EJson::String) { const auto Text=V->AsString();Chars+=Text.Len();if(!SafeText(Text,65536)||Chars>262144) { NoteUnsafe(TEXT("Native JSON text bound exceeded"));return false; } }
    else if(V->Type==EJson::Array) { for(const auto& X:V->AsArray()) { if(!JsonSafe(X,Depth+1,Items,Chars)) { return false; } } }
    else if(V->Type==EJson::Object) { for(const auto& X:V->AsObject()->Values) { if(!JsonSafe(X.Value,Depth+1,Items,Chars)) { return false; } } }
    return true;
}
FString Serialize(const TSharedPtr<FJsonObject>& J)
{ int32 Items=0;int64 Chars=0;if(!J.IsValid()||!JsonSafe(MakeShared<FJsonValueObject>(J),0,Items,Chars)) { return FString(); }FString Text;FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));if(FTCHARToUTF8(*Text).Length()>AuthoringJsonBudget) { NoteUnsafe(TEXT("Native selected snapshot/result exceeds1MiB"));return FString(); }return Text; }
bool Pack(const FString& Json,const TCHAR* Key,const TSharedRef<FJsonObject>& Params,bool Omitted=false)
{
    if(Json.IsEmpty()&&Omitted) { return true; }
    if(!SafeText(Json,131072)) { NoteUnsafe(TEXT("Private JSON carrier exceeds bounded syntax scope"));return false; }
    TSharedPtr<FJsonObject> Box;const FString Text=FString(TEXT("{\"value\":"))+Json+TEXT("}");if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Box)||!Box.IsValid()||Box->Values.Num()!=1) { NoteUnsafe(TEXT("Private JSON carrier must contain exactly one original JSON value"));return false; }
    const auto V=Box->TryGetField(TEXT("value"));int32 Items=0;int64 Chars=0;if(!JsonSafe(V,0,Items,Chars)) { return false; }Params->SetField(Key,V);return true;
}
bool Enumerate(UBlueprint* BP,TArray<UEdGraph*>& Graphs)
{
    if(!BoundedGraphTree(BP)) { return false; }Count(*CurrentResult,TEXT("get_all_graphs_calls"));BP->GetAllGraphs(Graphs);if(Graphs.Num()>AuthoringGraphBudget) { NoteUnsafe(TEXT("SDK graph occurrence budget exceeded"));return false; }for(UEdGraph* G:Graphs) { if(!G||!Hold(G)) { NoteUnsafe(TEXT("SDK graph occurrence unavailable"));return false; } }return true;
}
bool RefreshInventory()
{
    Count(*CurrentResult,TEXT("pins_inventory_calls"));KnownPins.Reset();TArray<UEdGraph*> Graphs;if(!Enumerate(CurrentBlueprint,Graphs)) { return false; }
    int32 NodeCount=0,PinCount=0;TSet<const UEdGraphNode*> SeenNodes;TSet<const UEdGraphPin*> SeenPins;
    for(UEdGraph* G:Graphs)
    {
        if(!G||!Hold(G)||!G->Schema.Get()||!Hold(G->Schema.Get())||!Hold(G->GetSchema())||G->Nodes.Num()>AuthoringNodeBudget) { NoteUnsafe(TEXT("Graph/schema/node array unavailable or exceeds4096"));return false; }
        for(UEdGraphNode* Node:G->Nodes)
        {
            if(!Node||SeenNodes.Contains(Node)) { continue; }SeenNodes.Add(Node);if(++NodeCount>AuthoringNodeBudget||!Hold(Node)||Node->GetGraph()!=G||Node->Pins.Num()>4096) { NoteUnsafe(TEXT("Current node identity/4096-node bound invalid"));return false; }
            for(UEdGraphPin* Pin:Node->Pins)
            {
                if(!Pin||SeenPins.Contains(Pin)||++PinCount>AuthoringPinBudget) { NoteUnsafe(TEXT("Primary pin null/duplicate/budget invalid"));return false; }SeenPins.Add(Pin);KnownPins.Add(Pin);
            }
        }
    }
    for(const UEdGraphPin* Pin:KnownPins)
    {
        if(!PinKnown(Pin)||!Hold(Pin->PinType.PinSubCategoryObject.Get())||!Hold(Pin->DefaultObject)||Pin->LinkedTo.Num()>4096||Pin->SubPins.Num()>4096) { return false; }
        if(Pin->ParentPin&&!KnownPins.Contains(Pin->ParentPin)) { NoteUnsafe(TEXT("Unknown parent pin address"));return false; }
        for(const UEdGraphPin* Child:Pin->SubPins) { if(!KnownPins.Contains(Child)) { NoteUnsafe(TEXT("Unknown subpin address"));return false; } }
        for(const UEdGraphPin* Linked:Pin->LinkedTo) { if(!KnownPins.Contains(Linked)) { NoteUnsafe(TEXT("Unknown linked pin address"));return false; } }
    }
    return !Incomplete();
}
UEdGraph* Resolve(UBlueprint* BP,const FString& Name,const FString& Id,const FString& Type,FResult& R)
{
    const FString Kind=NormalizeGraphType(Type);TArray<UEdGraph*> Graphs;if(!Enumerate(BP,Graphs)) { return nullptr; }
    if(!Id.IsEmpty()||!Name.IsEmpty()||!Kind.IsEmpty())
    { for(UEdGraph* G:Graphs) { if(GraphMatchesSelector(BP,G,Id,Name,Type)) { return G; } }R.Error=FString::Printf(TEXT("Blueprint graph not found. graph_name='%s', graph_id='%s', graph_type='%s'"),*Name,*Id,*Type);return nullptr; }
    for(UEdGraph* G:BP->UbergraphPages) { if(!Loaded(G)) { NoteUnsafe(TEXT("Default graph unavailable"));return nullptr; }if(G->GetName().Contains(TEXT("EventGraph"))) { return G; } }
    TArray<TSharedPtr<FJsonValue>> Before;for(UEdGraph* G:Graphs) { if(!Hold(G)) { return nullptr; }Before.Add(MakeShared<FJsonValueObject>(GraphToJson(BP,G))); }R.Data->SetArrayField(TEXT("graphs_before_default_creation"),Before);
    R.Stage=TEXT("create_default_event_graph");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("event_graph_create_calls"));UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());Count(R,TEXT("event_graph_create_returns"));R.Data->SetBoolField(TEXT("default_graph_returned"),G!=nullptr);if(!Hold(G)||!G) { return nullptr; }
    Count(R,TEXT("add_ubergraph_page_calls"));FBlueprintEditorUtils::AddUbergraphPage(BP,G);R.bChanged=true;R.Data->SetBoolField(TEXT("implicit_structural_skeleton_compile_possible"),true);R.Data->SetStringField(TEXT("default_graph_path"),G->GetPathName());R.Data->SetBoolField(TEXT("default_graph_attached"),BP->UbergraphPages.Contains(G));R.bChangeStateKnown=Loaded(G)&&G->GetOuter()==BP&&BP->UbergraphPages.Contains(G);return G;
}
bool RequiredTypes(UEdGraph* G,FResult& R)
{
    Count(R,TEXT("source_class_lookups"),5);AnimBase=FindObject<UClass>(nullptr,TEXT("/Script/AnimGraph.AnimGraphNode_Base"));UClass* Schema=FindObject<UClass>(nullptr,TEXT("/Script/AnimGraph.AnimationGraphSchema"));AnimStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.AnimNode_Base"));PoseStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.PoseLink"));ComponentPoseStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.ComponentSpacePoseLink"));
    if(!Hold(AnimBase)||!Hold(Schema)||!Hold(AnimStruct)||!Hold(PoseStruct)||!Hold(ComponentPoseStruct)||!AnimBase||!Schema||!AnimStruct||!PoseStruct||!ComponentPoseStruct||!G->GetSchema()->IsA(Schema)) { R.Error=TEXT("Already-loaded native AnimGraph/Engine types and compatible schema required");return false; }return true;
}
bool IsAnimationGraph(const UEdGraph* G) { return Loaded(G)&&G->GetSchema()&&G->GetSchema()->GetClass()->GetName().Contains(TEXT("AnimationGraphSchema")); }
UEdGraphPin* FindPin(UEdGraphNode* Node,const FString& Name,EEdGraphPinDirection Direction)
{ if(!Loaded(Node)) { return nullptr; }for(UEdGraphPin* P:Node->Pins) { if(!PinKnown(P)) { return nullptr; }if(P->Direction==Direction&&P->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)) { return P; } }return nullptr; }
bool IsPoseLinkPin(const UEdGraphPin* P) { return PinKnown(P)&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&P->PinType.PinSubCategoryObject.Get()==PoseStruct; }
UEdGraphPin* FindFirstPosePin(UEdGraphNode* Node,EEdGraphPinDirection Direction,const TArray<FString>& Preferred=TArray<FString>())
{
    if(!Loaded(Node)) { return nullptr; }for(const auto& Name:Preferred) { for(UEdGraphPin* P:Node->Pins) { if(PinKnown(P)&&P->Direction==Direction&&!P->ParentPin&&IsPoseLinkPin(P)&&P->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)) { return P; } } }
    for(UEdGraphPin* P:Node->Pins) { if(PinKnown(P)&&P->Direction==Direction&&!P->ParentPin&&IsPoseLinkPin(P)) { return P; } }return nullptr;
}
template<class T> T* FindFirstNodeOfType(UEdGraph* G)
{ for(UEdGraphNode* Node:G->Nodes) { if(!Node) { continue; }if(!Hold(Node)) { return nullptr; }if(auto* Typed=Cast<T>(Node)) { return Typed; } }return nullptr; }
bool FloatProjectionSafe(double Number)
{ if(!FMath::IsFinite(Number)||FMath::Abs(Number)>TNumericLimits<float>::Max()) { if(CurrentResult) { CurrentResult->Error=TEXT("Original float32 projection outside finite safe scope"); }return false; }return FMath::IsFinite(static_cast<float>(Number)); }

bool Member(UEdGraphNode* N,UEdGraph* G) { if(!N||!G||!Hold(N)||!Hold(G)||N->GetGraph()!=G||!G->Nodes.Contains(N)) { NoteUnsafe(TEXT("Selected typed node no longer belongs to current graph"));return false; }return true; }
void WriteIntent(const TCHAR* Counter) { auto& R=*CurrentResult;R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,Counter); }
void FieldWritten(const TCHAR* Field)
{
    auto& R=*CurrentResult;R.bChanged=true;TArray<TSharedPtr<FJsonValue>> Rows;
    if(const TArray<TSharedPtr<FJsonValue>>* Existing=nullptr;R.Data->TryGetArrayField(TEXT("native_changed_fields_prefix"),Existing)) { Rows=*Existing; }
    Rows.Add(MakeShared<FJsonValueString>(Field));R.Data->SetArrayField(TEXT("native_changed_fields_prefix"),Rows);
}
template<class T> T* AddAnimGraphNodeToGraph(UEdGraph* Graph,const FVector2D& Position)
{
    auto& R=*CurrentResult;WriteIntent(TEXT("node_allocation_calls"));UEdGraphNode* Allocated=NewObject<UEdGraphNode>(Graph,T::StaticClass());T* Node=Cast<T>(Allocated);Count(R,TEXT("node_allocation_returns"));
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("class"),T::StaticClass()->GetPathName());Receipt->SetBoolField(TEXT("allocation_returned"),Node!=nullptr);
    TArray<TSharedPtr<FJsonValue>> Rows;if(const TArray<TSharedPtr<FJsonValue>>* Existing=nullptr;R.Data->TryGetArrayField(TEXT("new_node_receipts"),Existing)) { Rows=*Existing; }Rows.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("new_node_receipts"),Rows);
    if(!Node||!Hold(Node)) { return nullptr; }Receipt->SetStringField(TEXT("path"),Node->GetPathName());
    Count(R,TEXT("node_position_write_calls"),2);Node->NodePosX=static_cast<int32>(Position.X);Node->NodePosY=static_cast<int32>(Position.Y);UEdGraphNode* Base=Node;
    Count(R,TEXT("graph_add_node_calls"));Graph->AddNode(Base,true);R.bChanged=true;Receipt->SetBoolField(TEXT("added"),Graph->Nodes.Contains(Base));if(!RefreshInventory()||!Member(Node,Graph)) { return nullptr; }
    Count(R,TEXT("create_node_guid_calls"));Base->CreateNewGuid();Receipt->SetStringField(TEXT("node_id"),Base->NodeGuid.ToString());
    Count(R,TEXT("post_placed_calls"));Base->PostPlacedNewNode();if(!RefreshInventory()||!Member(Node,Graph)) { return nullptr; }
    Count(R,TEXT("allocate_pins_calls"));Base->AllocateDefaultPins();if(!RefreshInventory()||!Member(Node,Graph)) { return nullptr; }
    Count(R,TEXT("reconstruct_calls"));Base->ReconstructNode();if(!RefreshInventory()||!Member(Node,Graph)) { return nullptr; }
    Receipt->SetBoolField(TEXT("lifecycle_completed"),true);return Node;
}
bool AfterDefaultSetter(UEdGraphPin* Pin,const FEdGraphPinReference& Reference) { if(!RefreshInventory()||Reference.Get()!=Pin) { NoteUnsafe(TEXT("Original default pin identity retired across callback"));return false; }return PinKnown(Pin); }
TSharedPtr<FJsonObject> Failure(const FString& Error) { return CreateErrorResponse(Error); }
bool EnsureAnimGraphConnection(UEdGraph* Graph,UEdGraphPin* Source,UEdGraphPin* Target,bool Replace,bool& Changed,FString& Error,TSharedPtr<FJsonObject>* ErrorObject=nullptr)
{
    Changed=false;Error.Reset();auto& R=*CurrentResult;
    if(!Graph||!Hold(Graph)||!PinKnown(Source)||!PinKnown(Target)) { Error=TEXT("Invalid graph or pins for AnimGraph connection");return false; }
    const FEdGraphPinReference SourceReference(Source),TargetReference(Target);
    if(Target->LinkedTo.Contains(Source)) { return true; }
    if(Target->LinkedTo.Num()>0)
    {
        if(!Replace)
        {
            TArray<TSharedPtr<FJsonValue>> Existing;for(UEdGraphPin* P:Target->LinkedTo) { if(!PinKnown(P)) { Error=R.Error;return false; }Existing.Add(MakeShared<FJsonValueObject>(PinToJson(P))); }
            Error=FString::Printf(TEXT("AnimGraph target pose pin already has a different link: %s"),*Target->PinName.ToString());
            if(ErrorObject) { auto J=Failure(Error);J->SetObjectField(TEXT("target_pin"),PinToJson(Target));J->SetArrayField(TEXT("existing_links"),Existing);*ErrorObject=J; }return false;
        }
        WriteIntent(TEXT("break_all_pin_links_calls"));Target->BreakAllPinLinks();Changed=true;R.bChanged=true;
        if(!RefreshInventory()||SourceReference.Get()!=Source||TargetReference.Get()!=Target||!PinKnown(Source)||!PinKnown(Target)) { Error=TEXT("Current pins unavailable after original BreakAllPinLinks; accepted prefix retained");return false; }
    }
    const UEdGraphSchema* Schema=Graph->GetSchema();if(!Schema||!Hold(Schema)) { Error=TEXT("Failed to get AnimationGraph schema");return false; }
    Count(R,TEXT("can_create_connection_calls"));const FPinConnectionResponse Response=Schema->CanCreateConnection(Source,Target);
    if(!RefreshInventory()||SourceReference.Get()!=Source||TargetReference.Get()!=Target||!PinKnown(Source)||!PinKnown(Target)) { Error=TEXT("Current pins unavailable after schema admission");return false; }
    if(!Response.CanSafeConnect()&&(!Replace||Response.Response==CONNECT_RESPONSE_DISALLOW))
    {
        Error=Response.Message.ToString();if(ErrorObject) { auto J=Failure(Error);J->SetStringField(TEXT("schema_response"),GetConnectionResponseName(Response.Response));J->SetStringField(TEXT("schema_message"),Response.Message.ToString());J->SetObjectField(TEXT("source_pin"),PinToJson(Source));J->SetObjectField(TEXT("target_pin"),PinToJson(Target));*ErrorObject=J; }return false;
    }
    WriteIntent(TEXT("try_create_connection_calls"));const bool Accepted=Schema->TryCreateConnection(Source,Target);Count(R,TEXT("try_create_connection_returns"));
    R.Data->SetBoolField(TEXT("last_try_create_connection_return"),Accepted);R.bChanged|=Accepted;
    if(!RefreshInventory()||SourceReference.Get()!=Source||TargetReference.Get()!=Target||!PinKnown(Source)||!PinKnown(Target)) { Error=TEXT("Current pins unavailable after schema connection; original accepted prefix retained");return false; }
    if(!Accepted) { Error=FString::Printf(TEXT("AnimationGraph schema failed to create connection from %s to %s"),*Source->PinName.ToString(),*Target->PinName.ToString());return false; }
    Changed=true;return true;
}

FString NormalizeAssetObjectPathForLoad(const FString& AssetPath)
{
    FString NormalizedPath = FPackageName::ExportTextPathToObjectPath(AssetPath).TrimStartAndEnd();
    NormalizedPath.TrimQuotesInline();

    if ((NormalizedPath.StartsWith(TEXT("/Game/")) || NormalizedPath.StartsWith(TEXT("/Engine/"))) && !NormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(NormalizedPath);
        NormalizedPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    return NormalizedPath;
}



UObject* SelectedTargetAsset=nullptr;UBlueprint* SourceBlueprint=nullptr;UControlRigBlueprint* SourceRigBlueprint=nullptr;
FString CurrentOperation;TSharedPtr<FJsonObject> SourceBefore;TSharedPtr<FJsonObject> TargetBefore;
TSharedPtr<FJsonObject> Capture(UBlueprint* BP);
bool CurrentClassesSafe(UBlueprint* BP);
UBlueprint* FindBlueprint(const FString& Name) { auto* BP=FindBlueprintByName(Name,*CurrentResult);if(BP&&Hold(BP)) { CurrentBlueprint=BP; }return BP; }
UEdGraph* ResolveBlueprintGraphForNodeCommand(UBlueprint* BP,const TSharedPtr<FJsonObject>& P,FString& Error)
{ CurrentBlueprint=BP;if(!Hold(BP)||!Hold(BP->GetOutermost())) { return nullptr; }auto* G=Resolve(BP,GetStringParam(P,TEXT("graph_name")),GetStringParam(P,TEXT("graph_id")),GetStringParam(P,TEXT("graph_type")),*CurrentResult);if(!G||!RequiredTypes(G,*CurrentResult)||!Hold(G->Schema.Get())||!Hold(G->GetSchema())||!RefreshInventory()) { Error=CurrentResult->Error;return nullptr; }return G; }
int32 GetIntParam(const TSharedPtr<FJsonObject>& P,const FString& Key,int32 Default) { int32 Value=Default;P->TryGetNumberField(Key,Value);return Value; }
UClass* RigBlueprintClass(UControlRigBlueprint* BP) { if(!BP||!Hold(BP)||!Hold(BP->GeneratedClass.Get())) { return nullptr; }const auto Ref=static_cast<IControlRigEditorAssetInterface*>(BP)->GetControlRigAssetReference();auto* C=Ref.GetBlueprintClass().Get();return Hold(C)?C:nullptr; }
bool RequiredName(const FString& S) { if(!SafeText(S,NAME_SIZE-1)) { NoteUnsafe(TEXT("Actual name exceeds bounded FName/NUL scope"));return false; }return true; }
bool SamplePackageSafe(const FString& S)
{ const FString Object=S+TEXT(".")+FPackageName::GetShortName(S);if(!RequiredName(S)||!RequiredName(Object)) { return false; }return true; }
bool HierarchySafe(URigHierarchy* H)
{ if(!H||!Hold(H)||H->Num()>4096) { NoteUnsafe(TEXT("Loaded bounded current hierarchy required"));return false; }for(int32 I=0;I<H->Num();++I) { if(!H->Get(I)) { NoteUnsafe(TEXT("Null current hierarchy element"));return false; } }return true; }
bool RefSkeletonSafe(const FReferenceSkeleton& R)
{ const auto& Info=R.GetRefBoneInfo();const auto& Poses=R.GetRefBonePose();if(Info.Num()>4096||Info.Num()!=Poses.Num()) { NoteUnsafe(TEXT("Ref skeleton info/pose count mismatch or bound"));return false; }for(int32 I=0;I<Info.Num();++I) { if(Info[I].ParentIndex<-1||Info[I].ParentIndex>=I||Poses[I].ContainsNaN()||R.FindBoneIndex(Info[I].Name)!=I) { NoteUnsafe(TEXT("Ref skeleton current canonical name/earlier-parent/finite pose required before SDK import"));return false; } }return true; }
bool RefreshTarget(UBlueprint* BP)
{ if(!BP||!Hold(BP)||!Hold(BP->GetOutermost())||!CurrentClassesSafe(BP)) { return false; }SelectedTargetAsset=BP;CurrentBlueprint=BP;return Cast<UControlRigBlueprint>(BP)?HierarchySafe(Cast<UControlRigBlueprint>(BP)->Hierarchy):RefreshInventory(); }
void AdoptSaved(bool Saved) { CurrentResult->bSaved=Saved;CurrentResult->Data->SetBoolField(TEXT("actual_save_return"),Saved); }

bool RigModelsSafe(UControlRigBlueprint* BP);

struct FControlRigSampleCurveSpec
{
    FString Name;
    float Value = 0.0f;
};
struct FControlRigSampleNullSpec
{
    FString Name;
    FString ParentName;
    bool bHasParent = false;
    FTransform Transform = FTransform::Identity;
    bool bHasTransform = false;
    bool bTransformInGlobal = true;
    FString TransformFieldName;
};
struct FControlRigSampleControlSpec
{
    FString Name;
    FString ParentName;
    bool bHasParent = false;
    ERigControlType ControlType = ERigControlType::Float;
    float FloatValue = 0.0f;
    bool BoolValue = false;
    FTransform TransformValue = FTransform::Identity;
    bool bHasTransformValue = false;
    FString ShapeName;
    bool bHasShapeName = false;
    FLinearColor ShapeColor = FLinearColor::Red;
    bool bHasShapeColor = false;
    FTransform ShapeTransform = FTransform::Identity;
    bool bHasShapeTransform = false;
};
struct FControlRigSampleRigVMUnitNodeSpec
{
    FString UnitType = TEXT("begin_execution");
    FString NodeName;
    FVector2D Position = FVector2D::ZeroVector;
};
bool IsFiniteControlRigProbeFloatRange(const double Value);
bool TryGetNumberFromObjectField(const TSharedPtr<FJsonObject>& Object, const FString& LowerName, const FString& UpperName, double& OutValue);
bool JsonValueToVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector);
bool JsonValueToRotator(const TSharedPtr<FJsonValue>& Value, FRotator& OutRotator);
bool JsonValueToTransform(const TSharedPtr<FJsonValue>& Value, FTransform& OutTransform);
bool JsonNumberToLinearColorComponent(const TSharedPtr<FJsonValue>& Value, float& OutComponent);
bool JsonValueToLinearColor(const TSharedPtr<FJsonValue>& Value, FLinearColor& OutColor);
bool TryGetLinearColorObjectComponent(
    const TSharedPtr<FJsonObject>& Object,
    std::initializer_list<const TCHAR*> FieldNames,
    float& OutComponent);
bool HasAnyLinearColorObjectComponent(const TSharedPtr<FJsonObject>& Object, std::initializer_list<const TCHAR*> FieldNames);
TSharedPtr<FJsonObject> LinearColorToJsonObject(const FLinearColor& Color);
TSharedPtr<FJsonObject> TransformToJsonObject(const FTransform& Transform);
TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector);
bool TryGetFirstJsonField(
    const TSharedPtr<FJsonObject>& Object,
    std::initializer_list<const TCHAR*> FieldNames,
    TSharedPtr<FJsonValue>& OutValue);
TSharedPtr<FJsonObject> RigElementKeyToJson(const FRigElementKey& Key);
TArray<TSharedPtr<FJsonValue>> RigElementKeysToJsonValues(const TArray<FRigElementKey>& Keys, int32 MaxKeys, bool& bOutTruncated);
TSharedPtr<FJsonObject> ControlRigSampleCurveSpecToJson(const FControlRigSampleCurveSpec& Spec, int32 Index);
TArray<TSharedPtr<FJsonValue>> ControlRigSampleCurveSpecsToJsonValues(const TArray<FControlRigSampleCurveSpec>& Specs);
bool ParseControlRigSampleCurveSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxDirectCurves,
    TArray<FControlRigSampleCurveSpec>& OutSpecs,
    FString& OutError);
TSharedPtr<FJsonObject> ControlRigSampleNullSpecToJson(const FControlRigSampleNullSpec& Spec, int32 Index);
TArray<TSharedPtr<FJsonValue>> ControlRigSampleNullSpecsToJsonValues(const TArray<FControlRigSampleNullSpec>& Specs);
bool ParseControlRigSampleNullSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxDirectNulls,
    TArray<FControlRigSampleNullSpec>& OutSpecs,
    FString& OutError);
bool ControlRigSampleControlHasShapeCustomization(const FControlRigSampleControlSpec& Spec);
bool ControlRigSampleControlIsTransformControl(ERigControlType ControlType);
FString ControlRigSampleControlTypeToString(ERigControlType ControlType);
TSharedPtr<FJsonObject> ControlRigSampleControlSpecToJson(const FControlRigSampleControlSpec& Spec, int32 Index);
TArray<TSharedPtr<FJsonValue>> ControlRigSampleControlSpecsToJsonValues(const TArray<FControlRigSampleControlSpec>& Specs);
bool ParseControlRigSampleFloatValue(const TSharedPtr<FJsonValue>& Value, float& OutValue, const FString& FieldPath, FString& OutError);
bool ParseControlRigSampleBoolValue(const TSharedPtr<FJsonValue>& Value, bool& OutValue, const FString& FieldPath, FString& OutError);
bool ParseControlRigSampleControlSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxDirectControls,
    TArray<FControlRigSampleControlSpec>& OutSpecs,
    FString& OutError);
bool TryResolveControlRigSampleParentKey(
    URigHierarchy* Hierarchy,
    const TArray<FRigElementKey>& AvailableDirectParentKeys,
    const FString& ParentSpec,
    FRigElementKey& OutParentKey,
    FString& OutError,
    const FString& FieldPath);
bool ParseRigElementType(const FString& RawValue, ERigElementType& OutType);
bool IsControlRigSampleParentType(ERigElementType Type);
bool TryParseControlRigProbeName(
    const FString& RequestedName,
    const FString& FieldPath,
    FName& OutName,
    FString& OutError,
    const bool bAllowNone = false);
void CollectControlRigSampleParentMatches(
    URigHierarchy* Hierarchy,
    const TArray<FRigElementKey>& AvailableDirectParentKeys,
    const FName ParentName,
    const TArray<ERigElementType>& ProbeTypes,
    TArray<FRigElementKey>& OutMatches);
void AddUniqueControlRigSampleParentMatch(TArray<FRigElementKey>& Matches, const FRigElementKey& Key);
int32 GetControlRigSampleRigVMAllowedUnitNodeCount();
FString GetControlRigSampleRigVMAllowedUnitsString();
FString NormalizeControlRigSampleRigVMUnitType(FString UnitType);
UScriptStruct* GetControlRigSampleRigVMUnitScriptStruct(const FString& UnitType);
TSharedPtr<FJsonObject> ControlRigSampleRigVMUnitSpecToJson(const FControlRigSampleRigVMUnitNodeSpec& Spec, int32 Index);
TSharedPtr<FJsonObject> Vector2DToJsonObject(const FVector2D& Vector);
TArray<TSharedPtr<FJsonValue>> ControlRigSampleRigVMUnitSpecsToJsonValues(const TArray<FControlRigSampleRigVMUnitNodeSpec>& Specs);
bool ParseControlRigSampleRigVMUnitNodeSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxRigVMUnitNodes,
    TArray<FControlRigSampleRigVMUnitNodeSpec>& OutSpecs,
    FString& OutError);
bool JsonValueToVector2D(const TSharedPtr<FJsonValue>& Value, FVector2D& OutVector, const FString& FieldPath, FString& OutError);
FString GetControlRigSampleRigVMUnitDefaultNodeName(const FString& UnitType);
TSharedPtr<FJsonObject> ControlRigSampleRigVMUnitNodeToJsonObject(
    const URigVMUnitNode* UnitNode,
    const FControlRigSampleRigVMUnitNodeSpec& Spec,
    int32 Index,
    bool bNodeAdded,
    bool bNodeReused);
TSharedPtr<FJsonObject> RigVMPinToJsonObject(const URigVMPin* Pin, int32 Index);
FString RigVMPinDirectionToString(ERigVMPinDirection Direction);
URigVMUnitNode* FindControlRigSampleRigVMUnitNode(URigVMGraph* Graph, UScriptStruct* ScriptStruct);
FString NormalizePackagePathForPostProcessVariant(const FString& RawPath);
FString SanitizeAssetNameSegmentForAnimNodeProbe(const FString& RawName);
bool IsValidGamePackagePathForPostProcessVariant(const FString& PackagePath, FString& OutError);
bool IsAllowedAnimStateMachineSampleTargetPath(const FString& PackagePath);
bool IsAllowedPostProcessVariantTargetPath(const FString& PackagePath);
bool GetJsonBoolFieldOrDefault(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, bool bDefaultValue);
bool ApplySampleCompileSaveOutcome(
    const TSharedPtr<FJsonObject>& ResultObj,
    const TSharedPtr<FJsonObject>& CompileResult,
    bool bCompileRequested,
    bool bSaveRequested,
    bool bTargetSaved);
void SetSamplePartialTargetMutationStatus(
    const TSharedPtr<FJsonObject>& ResponseObj,
    UObject* TargetAsset,
    const FString& TargetAssetPath,
    bool bPartialTargetModified);
UEdGraphPin* FindPreferredOutputPosePinForNode(UEdGraphNode* Node);
UEdGraphPin* FindFirstComponentPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const TArray<FString>& PreferredPinNames = TArray<FString>());
bool IsComponentPoseLinkPin(const UEdGraphPin* Pin);
bool IsFiniteControlRigProbeFloatRange(const double Value)
{
    return FMath::IsFinite(Value) &&
        FMath::Abs(Value) <= static_cast<double>(TNumericLimits<float>::Max());
}
bool TryGetNumberFromObjectField(const TSharedPtr<FJsonObject>& Object, const FString& LowerName, const FString& UpperName, double& OutValue)
{
    return Object.IsValid() &&
        (Object->TryGetNumberField(LowerName, OutValue) || Object->TryGetNumberField(UpperName, OutValue)) &&
        IsFiniteControlRigProbeFloatRange(OutValue);
}
bool JsonValueToVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector)
{
    if (!Value.IsValid())
    {
        return false;
    }

    if (Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
        if (Values.Num() < 3)
        {
            return false;
        }
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!Values[0].IsValid() || Values[0]->Type != EJson::Number || !Values[0]->TryGetNumber(X) ||
            !Values[1].IsValid() || Values[1]->Type != EJson::Number || !Values[1]->TryGetNumber(Y) ||
            !Values[2].IsValid() || Values[2]->Type != EJson::Number || !Values[2]->TryGetNumber(Z) ||
            !IsFiniteControlRigProbeFloatRange(X) ||
            !IsFiniteControlRigProbeFloatRange(Y) ||
            !IsFiniteControlRigProbeFloatRange(Z))
        {
            return false;
        }
        OutVector = FVector(X, Y, Z);
        return true;
    }

    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!TryGetNumberFromObjectField(Object, TEXT("x"), TEXT("X"), X) ||
            !TryGetNumberFromObjectField(Object, TEXT("y"), TEXT("Y"), Y) ||
            !TryGetNumberFromObjectField(Object, TEXT("z"), TEXT("Z"), Z))
        {
            return false;
        }
        OutVector = FVector(X, Y, Z);
        return true;
    }

    return false;
}
bool JsonValueToRotator(const TSharedPtr<FJsonValue>& Value, FRotator& OutRotator)
{
    FVector Vector;
    if (JsonValueToVector(Value, Vector))
    {
        OutRotator = FRotator(Vector.X, Vector.Y, Vector.Z);
        return true;
    }
    return false;
}
bool JsonValueToTransform(const TSharedPtr<FJsonValue>& Value, FTransform& OutTransform)
{
    if (!Value.IsValid() || Value->Type != EJson::Object)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Object = Value->AsObject();
    if (!Object.IsValid())
    {
        return false;
    }

    FVector Translation = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    bool bHasAnyField = false;

    const TSharedPtr<FJsonValue> TranslationValue = Object->TryGetField(TEXT("translation")).IsValid()
        ? Object->TryGetField(TEXT("translation"))
        : Object->TryGetField(TEXT("location"));
    if (TranslationValue.IsValid())
    {
        if (!JsonValueToVector(TranslationValue, Translation))
        {
            return false;
        }
        bHasAnyField = true;
    }

    if (const TSharedPtr<FJsonValue> RotationValue = Object->TryGetField(TEXT("rotation")))
    {
        if (!JsonValueToRotator(RotationValue, Rotation))
        {
            return false;
        }
        bHasAnyField = true;
    }

    const TSharedPtr<FJsonValue> ScaleValue = Object->TryGetField(TEXT("scale")).IsValid()
        ? Object->TryGetField(TEXT("scale"))
        : Object->TryGetField(TEXT("scale3d"));
    if (ScaleValue.IsValid())
    {
        if (!JsonValueToVector(ScaleValue, Scale))
        {
            return false;
        }
        bHasAnyField = true;
    }

    if (!bHasAnyField)
    {
        return false;
    }

    OutTransform = FTransform(Rotation, Translation, Scale);
    return true;
}
bool JsonNumberToLinearColorComponent(const TSharedPtr<FJsonValue>& Value, float& OutComponent)
{
    if (!Value.IsValid() || Value->Type != EJson::Number)
    {
        return false;
    }

    const double ComponentValue = Value->AsNumber();
    if (!FMath::IsFinite(ComponentValue) || ComponentValue < 0.0 || ComponentValue > 255.0)
    {
        return false;
    }

    OutComponent = static_cast<float>(ComponentValue > 1.0 ? ComponentValue / 255.0 : ComponentValue);
    return true;
}
bool JsonValueToLinearColor(const TSharedPtr<FJsonValue>& Value, FLinearColor& OutColor)
{
    if (!Value.IsValid())
    {
        return false;
    }

    float R = 0.0f;
    float G = 0.0f;
    float B = 0.0f;
    float A = 1.0f;

    if (Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
        if (Values.Num() < 3 || Values.Num() > 4)
        {
            return false;
        }
        if (!JsonNumberToLinearColorComponent(Values[0], R) ||
            !JsonNumberToLinearColorComponent(Values[1], G) ||
            !JsonNumberToLinearColorComponent(Values[2], B))
        {
            return false;
        }
        if (Values.Num() == 4 && !JsonNumberToLinearColorComponent(Values[3], A))
        {
            return false;
        }

        OutColor = FLinearColor(R, G, B, A);
        return true;
    }

    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!TryGetLinearColorObjectComponent(Object, {TEXT("r"), TEXT("R"), TEXT("red"), TEXT("Red")}, R) ||
            !TryGetLinearColorObjectComponent(Object, {TEXT("g"), TEXT("G"), TEXT("green"), TEXT("Green")}, G) ||
            !TryGetLinearColorObjectComponent(Object, {TEXT("b"), TEXT("B"), TEXT("blue"), TEXT("Blue")}, B))
        {
            return false;
        }

        const std::initializer_list<const TCHAR*> AlphaFields = {TEXT("a"), TEXT("A"), TEXT("alpha"), TEXT("Alpha")};
        float ParsedAlpha = 1.0f;
        if (HasAnyLinearColorObjectComponent(Object, AlphaFields))
        {
            if (!TryGetLinearColorObjectComponent(Object, AlphaFields, ParsedAlpha))
            {
                return false;
            }
            A = ParsedAlpha;
        }

        OutColor = FLinearColor(R, G, B, A);
        return true;
    }

    return false;
}
bool TryGetLinearColorObjectComponent(
    const TSharedPtr<FJsonObject>& Object,
    std::initializer_list<const TCHAR*> FieldNames,
    float& OutComponent)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const TCHAR* FieldName : FieldNames)
    {
        const TSharedPtr<FJsonValue> FieldValue = Object->TryGetField(FieldName);
        if (FieldValue.IsValid())
        {
            return JsonNumberToLinearColorComponent(FieldValue, OutComponent);
        }
    }
    return false;
}
bool HasAnyLinearColorObjectComponent(const TSharedPtr<FJsonObject>& Object, std::initializer_list<const TCHAR*> FieldNames)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const TCHAR* FieldName : FieldNames)
    {
        if (Object->HasField(FieldName))
        {
            return true;
        }
    }
    return false;
}
TSharedPtr<FJsonObject> LinearColorToJsonObject(const FLinearColor& Color)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("r"), Color.R);
    Object->SetNumberField(TEXT("g"), Color.G);
    Object->SetNumberField(TEXT("b"), Color.B);
    Object->SetNumberField(TEXT("a"), Color.A);
    return Object;
}
TSharedPtr<FJsonObject> TransformToJsonObject(const FTransform& Transform)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetArrayField(TEXT("translation"), VectorToJsonArray(Transform.GetLocation()));

    const FQuat Rotation = Transform.GetRotation();
    TArray<TSharedPtr<FJsonValue>> QuatValues;
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.X));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.Y));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.Z));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.W));
    Object->SetArrayField(TEXT("rotation"), QuatValues);

    const FRotator Rotator = Rotation.Rotator();
    TArray<TSharedPtr<FJsonValue>> RotatorValues;
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Pitch));
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Yaw));
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Roll));
    Object->SetArrayField(TEXT("rotation_euler"), RotatorValues);

    Object->SetArrayField(TEXT("scale"), VectorToJsonArray(Transform.GetScale3D()));
    return Object;
}
TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vector.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Z));
    return Values;
}
bool TryGetFirstJsonField(
    const TSharedPtr<FJsonObject>& Object,
    std::initializer_list<const TCHAR*> FieldNames,
    TSharedPtr<FJsonValue>& OutValue)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const TCHAR* FieldName : FieldNames)
    {
        if (Object->HasField(FieldName))
        {
            OutValue = Object->TryGetField(FieldName);
            return OutValue.IsValid();
        }
    }
    return false;
}
TSharedPtr<FJsonObject> RigElementKeyToJson(const FRigElementKey& Key)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    const UEnum* ElementTypeEnum = StaticEnum<ERigElementType>();
    Object->SetStringField(TEXT("name"), Key.Name.ToString());
    Object->SetStringField(
        TEXT("type"),
        ElementTypeEnum ? ElementTypeEnum->GetNameStringByValue(static_cast<int64>(Key.Type)) : FString::Printf(TEXT("%d"), static_cast<int32>(Key.Type)));
    Object->SetStringField(TEXT("key"), Key.ToString());
    return Object;
}
TArray<TSharedPtr<FJsonValue>> RigElementKeysToJsonValues(const TArray<FRigElementKey>& Keys, int32 MaxKeys, bool& bOutTruncated)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    bOutTruncated = false;
    const int32 SafeMaxKeys = FMath::Max(0, MaxKeys);
    const int32 Count = FMath::Min(Keys.Num(), SafeMaxKeys);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Values.Add(MakeShared<FJsonValueObject>(RigElementKeyToJson(Keys[Index])));
    }
    bOutTruncated = Keys.Num() > Count;
    return Values;
}
TSharedPtr<FJsonObject> ControlRigSampleCurveSpecToJson(const FControlRigSampleCurveSpec& Spec, int32 Index)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetStringField(TEXT("name"), Spec.Name);
    Object->SetNumberField(TEXT("value"), Spec.Value);
    return Object;
}
TArray<TSharedPtr<FJsonValue>> ControlRigSampleCurveSpecsToJsonValues(const TArray<FControlRigSampleCurveSpec>& Specs)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        Values.Add(MakeShared<FJsonValueObject>(ControlRigSampleCurveSpecToJson(Specs[Index], Index)));
    }
    return Values;
}
bool ParseControlRigSampleCurveSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxDirectCurves,
    TArray<FControlRigSampleCurveSpec>& OutSpecs,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* CurveValues = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("curves"), CurveValues) || !CurveValues || CurveValues->Num() == 0)
    {
        return true;
    }

    if (CurveValues->Num() > MaxDirectCurves)
    {
        OutError = FString::Printf(TEXT("curves contains %d entries, max_direct_curves=%d"), CurveValues->Num(), MaxDirectCurves);
        return false;
    }

    TSet<FString> SeenNames;
    for (int32 Index = 0; Index < CurveValues->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& CurveValue = (*CurveValues)[Index];
        FControlRigSampleCurveSpec Spec;

        if (!CurveValue.IsValid())
        {
            OutError = FString::Printf(TEXT("curves[%d] is invalid"), Index);
            return false;
        }

        if (CurveValue->Type == EJson::String)
        {
            Spec.Name = CurveValue->AsString();
        }
        else if (CurveValue->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> CurveObject = CurveValue->AsObject();
            if (!CurveObject.IsValid())
            {
                OutError = FString::Printf(TEXT("curves[%d] must be a string or object"), Index);
                return false;
            }
            Spec.Name = GetStringParam(CurveObject, TEXT("name"));
            double InitialValue = 0.0;
            if (CurveObject->TryGetNumberField(TEXT("value"), InitialValue) || CurveObject->TryGetNumberField(TEXT("initial_value"), InitialValue))
            {
                Spec.Value = static_cast<float>(InitialValue);
            }
        }
        else
        {
            OutError = FString::Printf(TEXT("curves[%d] must be a string or object"), Index);
            return false;
        }

        Spec.Name.TrimStartAndEndInline();
        if (Spec.Name.IsEmpty())
        {
            OutError = FString::Printf(TEXT("curves[%d] has no name"), Index);
            return false;
        }
        if (Spec.Name.Contains(TEXT("/")) || Spec.Name.Contains(TEXT("\\")) || Spec.Name.Contains(TEXT(".")) || Spec.Name.Contains(TEXT(":")))
        {
            OutError = FString::Printf(TEXT("curves[%d] name contains an unsupported character: %s"), Index, *Spec.Name);
            return false;
        }
        const FString NameKey = Spec.Name.ToLower();
        if (SeenNames.Contains(NameKey))
        {
            OutError = FString::Printf(TEXT("Duplicate direct curve name: %s"), *Spec.Name);
            return false;
        }
        SeenNames.Add(NameKey);
        OutSpecs.Add(Spec);
    }
    return true;
}
TSharedPtr<FJsonObject> ControlRigSampleNullSpecToJson(const FControlRigSampleNullSpec& Spec, int32 Index)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetStringField(TEXT("name"), Spec.Name);
    if (Spec.bHasParent)
    {
        Object->SetStringField(TEXT("parent_name"), Spec.ParentName);
    }
    if (Spec.bHasTransform)
    {
        Object->SetStringField(TEXT("transform_field"), Spec.TransformFieldName);
        Object->SetStringField(TEXT("transform_space"), Spec.bTransformInGlobal ? TEXT("global") : TEXT("local"));
        Object->SetObjectField(TEXT("transform"), TransformToJsonObject(Spec.Transform));
    }
    return Object;
}
TArray<TSharedPtr<FJsonValue>> ControlRigSampleNullSpecsToJsonValues(const TArray<FControlRigSampleNullSpec>& Specs)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        Values.Add(MakeShared<FJsonValueObject>(ControlRigSampleNullSpecToJson(Specs[Index], Index)));
    }
    return Values;
}
bool ParseControlRigSampleNullSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxDirectNulls,
    TArray<FControlRigSampleNullSpec>& OutSpecs,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* NullValues = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("nulls"), NullValues) || !NullValues || NullValues->Num() == 0)
    {
        return true;
    }

    if (NullValues->Num() > MaxDirectNulls)
    {
        OutError = FString::Printf(TEXT("nulls contains %d entries, max_direct_nulls=%d"), NullValues->Num(), MaxDirectNulls);
        return false;
    }

    const TArray<FString> UnsupportedNullObjectFields = {
        TEXT("parent_key"),
        TEXT("initial_transform")
    };

    TSet<FString> SeenNames;
    for (int32 Index = 0; Index < NullValues->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& NullValue = (*NullValues)[Index];
        FControlRigSampleNullSpec Spec;

        if (!NullValue.IsValid())
        {
            OutError = FString::Printf(TEXT("nulls[%d] is invalid"), Index);
            return false;
        }

        if (NullValue->Type == EJson::String)
        {
            Spec.Name = NullValue->AsString();
        }
        else if (NullValue->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> NullObject = NullValue->AsObject();
            if (!NullObject.IsValid())
            {
                OutError = FString::Printf(TEXT("nulls[%d] must be a string or object"), Index);
                return false;
            }
            for (const FString& FieldName : UnsupportedNullObjectFields)
            {
                if (NullObject->HasField(FieldName))
                {
                    OutError = FString::Printf(
                        TEXT("nulls[%d] parent_key/initial_transform authoring is not supported by ensure_controlrig_sample_asset direct nulls; this pass only supports named parents with current local/global transforms"),
                        Index);
                    return false;
                }
            }
            Spec.Name = GetStringParam(NullObject, TEXT("name"));

            TSharedPtr<FJsonValue> TransformValue;
            int32 TransformFieldCount = 0;
            for (const TCHAR* FieldName : {TEXT("transform"), TEXT("global_transform"), TEXT("local_transform")})
            {
                if (NullObject->HasField(FieldName))
                {
                    ++TransformFieldCount;
                    TransformValue = NullObject->TryGetField(FieldName);
                    Spec.TransformFieldName = FieldName;
                    Spec.bTransformInGlobal = FCString::Stricmp(FieldName, TEXT("local_transform")) != 0;
                }
            }
            if (TransformFieldCount > 1)
            {
                OutError = FString::Printf(TEXT("nulls[%d] must specify only one of transform, global_transform, or local_transform"), Index);
                return false;
            }
            if (TransformFieldCount == 1)
            {
                if (!JsonValueToTransform(TransformValue, Spec.Transform))
                {
                    OutError = FString::Printf(TEXT("nulls[%d].%s must be a transform object with translation/location, rotation, or scale/scale3d"), Index, *Spec.TransformFieldName);
                    return false;
                }
                Spec.bHasTransform = true;
            }

            TSharedPtr<FJsonValue> ParentValue;
            if (TryGetFirstJsonField(NullObject, {TEXT("parent"), TEXT("parent_name")}, ParentValue))
            {
                if (!ParentValue.IsValid() || ParentValue->Type != EJson::String)
                {
                    OutError = FString::Printf(TEXT("nulls[%d].parent must be a string parent name or typed key"), Index);
                    return false;
                }
                Spec.ParentName = ParentValue->AsString();
                Spec.ParentName.TrimStartAndEndInline();
                if (Spec.ParentName.IsEmpty())
                {
                    OutError = FString::Printf(TEXT("nulls[%d].parent cannot be empty"), Index);
                    return false;
                }
                Spec.bHasParent = true;
            }
        }
        else
        {
            OutError = FString::Printf(TEXT("nulls[%d] must be a string or object"), Index);
            return false;
        }

        Spec.Name.TrimStartAndEndInline();
        if (Spec.Name.IsEmpty())
        {
            OutError = FString::Printf(TEXT("nulls[%d] has no name"), Index);
            return false;
        }
        if (Spec.Name.Contains(TEXT("/")) || Spec.Name.Contains(TEXT("\\")) || Spec.Name.Contains(TEXT(".")) || Spec.Name.Contains(TEXT(":")))
        {
            OutError = FString::Printf(TEXT("nulls[%d] name contains an unsupported character: %s"), Index, *Spec.Name);
            return false;
        }
        const FString NameKey = Spec.Name.ToLower();
        if (SeenNames.Contains(NameKey))
        {
            OutError = FString::Printf(TEXT("Duplicate direct null name: %s"), *Spec.Name);
            return false;
        }
        SeenNames.Add(NameKey);
        OutSpecs.Add(Spec);
    }
    return true;
}
bool ControlRigSampleControlHasShapeCustomization(const FControlRigSampleControlSpec& Spec)
{
    return Spec.bHasShapeName || Spec.bHasShapeColor || Spec.bHasShapeTransform;
}
bool ControlRigSampleControlIsTransformControl(ERigControlType ControlType)
{
    return ControlType == ERigControlType::EulerTransform;
}
FString ControlRigSampleControlTypeToString(ERigControlType ControlType)
{
    switch (ControlType)
    {
        case ERigControlType::Bool:
        {
            return TEXT("bool");
        }
        case ERigControlType::Float:
        {
            return TEXT("float");
        }
        case ERigControlType::EulerTransform:
        {
            return TEXT("euler_transform");
        }
        default:
        {
            return TEXT("unsupported");
        }
    }
}
TSharedPtr<FJsonObject> ControlRigSampleControlSpecToJson(const FControlRigSampleControlSpec& Spec, int32 Index)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetStringField(TEXT("name"), Spec.Name);
    Object->SetStringField(TEXT("control_type"), ControlRigSampleControlTypeToString(Spec.ControlType));
    if (Spec.bHasParent)
    {
        Object->SetStringField(TEXT("parent_name"), Spec.ParentName);
    }
    if (Spec.ControlType == ERigControlType::Bool)
    {
        Object->SetBoolField(TEXT("default_value"), Spec.BoolValue);
    }
    else if (ControlRigSampleControlIsTransformControl(Spec.ControlType))
    {
        Object->SetObjectField(TEXT("default_value"), TransformToJsonObject(Spec.TransformValue));
        Object->SetBoolField(TEXT("transform_value_specified"), Spec.bHasTransformValue);
    }
    else
    {
        Object->SetNumberField(TEXT("default_value"), Spec.FloatValue);
    }
    Object->SetBoolField(TEXT("transform_control"), ControlRigSampleControlIsTransformControl(Spec.ControlType));
    Object->SetBoolField(TEXT("shape_customized"), ControlRigSampleControlHasShapeCustomization(Spec));
    if (Spec.bHasShapeName)
    {
        Object->SetStringField(TEXT("shape_name"), Spec.ShapeName);
    }
    if (Spec.bHasShapeColor)
    {
        Object->SetObjectField(TEXT("shape_color"), LinearColorToJsonObject(Spec.ShapeColor));
    }
    if (Spec.bHasShapeTransform)
    {
        Object->SetObjectField(TEXT("shape_transform"), TransformToJsonObject(Spec.ShapeTransform));
    }
    return Object;
}
TArray<TSharedPtr<FJsonValue>> ControlRigSampleControlSpecsToJsonValues(const TArray<FControlRigSampleControlSpec>& Specs)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        Values.Add(MakeShared<FJsonValueObject>(ControlRigSampleControlSpecToJson(Specs[Index], Index)));
    }
    return Values;
}
bool ParseControlRigSampleFloatValue(const TSharedPtr<FJsonValue>& Value, float& OutValue, const FString& FieldPath, FString& OutError)
{
    if (!Value.IsValid())
    {
        return true;
    }

    if (Value->Type == EJson::Number)
    {
        OutValue = static_cast<float>(Value->AsNumber());
        return true;
    }
    if (Value->Type == EJson::Boolean)
    {
        OutValue = Value->AsBool() ? 1.0f : 0.0f;
        return true;
    }
    if (Value->Type == EJson::String)
    {
        double ParsedValue = 0.0;
        if (LexTryParseString(ParsedValue, *Value->AsString()))
        {
            OutValue = static_cast<float>(ParsedValue);
            return true;
        }
    }

    OutError = FString::Printf(TEXT("%s must be a number for float controls"), *FieldPath);
    return false;
}
bool ParseControlRigSampleBoolValue(const TSharedPtr<FJsonValue>& Value, bool& OutValue, const FString& FieldPath, FString& OutError)
{
    if (!Value.IsValid())
    {
        return true;
    }

    if (Value->Type == EJson::Boolean)
    {
        OutValue = Value->AsBool();
        return true;
    }
    if (Value->Type == EJson::Number)
    {
        OutValue = !FMath::IsNearlyZero(Value->AsNumber());
        return true;
    }
    if (Value->Type == EJson::String)
    {
        const FString ValueLower = Value->AsString().TrimStartAndEnd().ToLower();
        if (ValueLower == TEXT("true") || ValueLower == TEXT("1"))
        {
            OutValue = true;
            return true;
        }
        if (ValueLower == TEXT("false") || ValueLower == TEXT("0"))
        {
            OutValue = false;
            return true;
        }
    }

    OutError = FString::Printf(TEXT("%s must be a boolean for bool controls"), *FieldPath);
    return false;
}
bool ParseControlRigSampleControlSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxDirectControls,
    TArray<FControlRigSampleControlSpec>& OutSpecs,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* ControlValues = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("controls"), ControlValues) || !ControlValues || ControlValues->Num() == 0)
    {
        return true;
    }

    if (ControlValues->Num() > MaxDirectControls)
    {
        OutError = FString::Printf(TEXT("controls contains %d entries, max_direct_controls=%d"), ControlValues->Num(), MaxDirectControls);
        return false;
    }

    const TArray<FString> UnsupportedControlObjectFields = {
        TEXT("parent_key"),
        TEXT("transform"),
        TEXT("local_transform"),
        TEXT("global_transform"),
        TEXT("initial_transform"),
        TEXT("offset_transform"),
        TEXT("animation_type"),
        TEXT("proxy"),
        TEXT("driven_controls"),
        TEXT("channels"),
        TEXT("filtered_channels"),
        TEXT("space"),
        TEXT("spaces"),
        TEXT("settings"),
        TEXT("unit_node"),
        TEXT("unit_nodes"),
        TEXT("rigvm")
    };

    TSet<FString> SeenNames;
    for (int32 Index = 0; Index < ControlValues->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& ControlValue = (*ControlValues)[Index];
        FControlRigSampleControlSpec Spec;

        if (!ControlValue.IsValid())
        {
            OutError = FString::Printf(TEXT("controls[%d] is invalid"), Index);
            return false;
        }

        if (ControlValue->Type == EJson::String)
        {
            Spec.Name = ControlValue->AsString();
        }
        else if (ControlValue->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> ControlObject = ControlValue->AsObject();
            if (!ControlObject.IsValid())
            {
                OutError = FString::Printf(TEXT("controls[%d] must be a string or object"), Index);
                return false;
            }
            for (const FString& FieldName : UnsupportedControlObjectFields)
            {
                if (ControlObject->HasField(FieldName))
                {
                    OutError = FString::Printf(
                        TEXT("controls[%d] parent_key/transform/proxy/RigVM authoring is not supported by ensure_controlrig_sample_asset direct controls; this pass only supports named parents, float/bool/euler transform controls, value/default_value transform defaults, and control shape metadata"),
                        Index);
                    return false;
                }
            }

            Spec.Name = GetStringParam(ControlObject, TEXT("name"));
            TSharedPtr<FJsonValue> ParentValue;
            if (TryGetFirstJsonField(ControlObject, {TEXT("parent"), TEXT("parent_name")}, ParentValue))
            {
                if (!ParentValue.IsValid() || ParentValue->Type != EJson::String)
                {
                    OutError = FString::Printf(TEXT("controls[%d].parent must be a string parent name or typed key"), Index);
                    return false;
                }
                Spec.ParentName = ParentValue->AsString();
                Spec.ParentName.TrimStartAndEndInline();
                if (Spec.ParentName.IsEmpty())
                {
                    OutError = FString::Printf(TEXT("controls[%d].parent cannot be empty"), Index);
                    return false;
                }
                Spec.bHasParent = true;
            }

            FString ControlTypeName = GetStringParam(ControlObject, TEXT("control_type"));
            if (ControlTypeName.IsEmpty())
            {
                ControlTypeName = GetStringParam(ControlObject, TEXT("type"), TEXT("float"));
            }
            ControlTypeName.TrimStartAndEndInline();
            ControlTypeName = ControlTypeName.ToLower();
            const FString ControlTypeKey = ControlTypeName.Replace(TEXT("_"), TEXT("")).Replace(TEXT(" "), TEXT(""));
            if (ControlTypeKey.IsEmpty() || ControlTypeKey == TEXT("float") || ControlTypeKey == TEXT("scalar"))
            {
                Spec.ControlType = ERigControlType::Float;
            }
            else if (ControlTypeKey == TEXT("bool") || ControlTypeKey == TEXT("boolean"))
            {
                Spec.ControlType = ERigControlType::Bool;
            }
            else if (ControlTypeKey == TEXT("transform") || ControlTypeKey == TEXT("eulertransform"))
            {
                Spec.ControlType = ERigControlType::EulerTransform;
            }
            else
            {
                OutError = FString::Printf(
                    TEXT("controls[%d] control_type '%s' is not supported; this pass only supports float, bool, and transform/euler_transform"),
                    Index,
                    *ControlTypeName);
                return false;
            }

            TSharedPtr<FJsonValue> InitialValue;
            if (TryGetFirstJsonField(ControlObject, {TEXT("value"), TEXT("default_value"), TEXT("initial_value"), TEXT("default")}, InitialValue))
            {
                const FString FieldPath = FString::Printf(TEXT("controls[%d].value"), Index);
                if (Spec.ControlType == ERigControlType::Bool)
                {
                    if (!ParseControlRigSampleBoolValue(InitialValue, Spec.BoolValue, FieldPath, OutError))
                    {
                        return false;
                    }
                }
                else if (ControlRigSampleControlIsTransformControl(Spec.ControlType))
                {
                    if (!JsonValueToTransform(InitialValue, Spec.TransformValue))
                    {
                        OutError = FString::Printf(TEXT("%s must be a transform object with translation/location, rotation, or scale/scale3d for transform controls"), *FieldPath);
                        return false;
                    }
                    Spec.bHasTransformValue = true;
                }
                else
                {
                    if (!ParseControlRigSampleFloatValue(InitialValue, Spec.FloatValue, FieldPath, OutError))
                    {
                        return false;
                    }
                }
            }

            if (const TSharedPtr<FJsonValue> ShapeNameValue = ControlObject->TryGetField(TEXT("shape_name")))
            {
                if (!ShapeNameValue.IsValid() || ShapeNameValue->Type != EJson::String)
                {
                    OutError = FString::Printf(TEXT("controls[%d].shape_name must be a string"), Index);
                    return false;
                }
                Spec.ShapeName = ShapeNameValue->AsString();
                Spec.ShapeName.TrimStartAndEndInline();
                if (Spec.ShapeName.IsEmpty())
                {
                    OutError = FString::Printf(TEXT("controls[%d].shape_name cannot be empty"), Index);
                    return false;
                }
                if (Spec.ShapeName.Contains(TEXT("/")) || Spec.ShapeName.Contains(TEXT("\\")))
                {
                    OutError = FString::Printf(TEXT("controls[%d].shape_name contains an unsupported path character: %s"), Index, *Spec.ShapeName);
                    return false;
                }
                Spec.bHasShapeName = true;
            }

            if (const TSharedPtr<FJsonValue> ShapeColorValue = ControlObject->TryGetField(TEXT("shape_color")))
            {
                if (!JsonValueToLinearColor(ShapeColorValue, Spec.ShapeColor))
                {
                    OutError = FString::Printf(TEXT("controls[%d].shape_color must be [r,g,b,a?] or {r,g,b,a}; components must be 0..1 or 0..255"), Index);
                    return false;
                }
                Spec.bHasShapeColor = true;
            }

            if (const TSharedPtr<FJsonValue> ShapeTransformValue = ControlObject->TryGetField(TEXT("shape_transform")))
            {
                if (!JsonValueToTransform(ShapeTransformValue, Spec.ShapeTransform))
                {
                    OutError = FString::Printf(TEXT("controls[%d].shape_transform must be a transform object with translation/location, rotation, or scale/scale3d"), Index);
                    return false;
                }
                Spec.bHasShapeTransform = true;
            }
        }
        else
        {
            OutError = FString::Printf(TEXT("controls[%d] must be a string or object"), Index);
            return false;
        }

        Spec.Name.TrimStartAndEndInline();
        if (Spec.Name.IsEmpty())
        {
            OutError = FString::Printf(TEXT("controls[%d] has no name"), Index);
            return false;
        }
        if (Spec.Name.Contains(TEXT("/")) || Spec.Name.Contains(TEXT("\\")) || Spec.Name.Contains(TEXT(".")) || Spec.Name.Contains(TEXT(":")))
        {
            OutError = FString::Printf(TEXT("controls[%d] name contains an unsupported character: %s"), Index, *Spec.Name);
            return false;
        }
        const FString NameKey = Spec.Name.ToLower();
        if (SeenNames.Contains(NameKey))
        {
            OutError = FString::Printf(TEXT("Duplicate direct control name: %s"), *Spec.Name);
            return false;
        }
        SeenNames.Add(NameKey);
        OutSpecs.Add(Spec);
    }
    return true;
}
bool TryResolveControlRigSampleParentKey(
    URigHierarchy* Hierarchy,
    const TArray<FRigElementKey>& AvailableDirectParentKeys,
    const FString& ParentSpec,
    FRigElementKey& OutParentKey,
    FString& OutError,
    const FString& FieldPath)
{
    FString ParentText = ParentSpec;
    ParentText.TrimStartAndEndInline();
    if (ParentText.IsEmpty())
    {
        OutError = FString::Printf(TEXT("%s cannot be empty"), *FieldPath);
        return false;
    }

    FString TypeText;
    FString NameText = ParentText;
    TArray<ERigElementType> ProbeTypes;
    if (ParentText.Split(TEXT(":"), &TypeText, &NameText))
    {
        TypeText.TrimStartAndEndInline();
        NameText.TrimStartAndEndInline();
        ERigElementType ParsedType = ERigElementType::None;
        if (!ParseRigElementType(TypeText, ParsedType) || !IsControlRigSampleParentType(ParsedType))
        {
            OutError = FString::Printf(TEXT("%s parent type '%s' is not supported; use Bone, Null, or Control"), *FieldPath, *TypeText);
            return false;
        }
        if (NameText.IsEmpty())
        {
            OutError = FString::Printf(TEXT("%s has no parent name after type prefix"), *FieldPath);
            return false;
        }
        ProbeTypes.Add(ParsedType);
    }
    else
    {
        ProbeTypes.Add(ERigElementType::Bone);
        ProbeTypes.Add(ERigElementType::Null);
        ProbeTypes.Add(ERigElementType::Control);
    }

    FName ParentName = NAME_None;
    if (!TryParseControlRigProbeName(
            NameText,
            FString::Printf(TEXT("%s parent name"), *FieldPath),
            ParentName,
            OutError))
    {
        return false;
    }

    TArray<FRigElementKey> Matches;
    CollectControlRigSampleParentMatches(Hierarchy, AvailableDirectParentKeys, ParentName, ProbeTypes, Matches);
    if (Matches.Num() == 1)
    {
        OutParentKey = Matches[0];
        return true;
    }
    if (Matches.Num() == 0)
    {
        OutError = FString::Printf(
            TEXT("%s parent '%s' was not found in existing/imported hierarchy or earlier direct Bone/Null/Control keys"),
            *FieldPath,
            *ParentText);
        return false;
    }

    TArray<FString> MatchStrings;
    for (const FRigElementKey& Match : Matches)
    {
        MatchStrings.Add(Match.ToString());
    }
    OutError = FString::Printf(
        TEXT("%s parent '%s' is ambiguous (%s); use a typed parent such as Bone:name, Null:name, or Control:name"),
        *FieldPath,
        *ParentText,
        *FString::Join(MatchStrings, TEXT(", ")));
    return false;
}
bool ParseRigElementType(const FString& RawValue, ERigElementType& OutType)
{
    const FString Value = RawValue.TrimStartAndEnd().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
    if (Value.Equals(TEXT("Bone"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Bone;
        return true;
    }
    if (Value.Equals(TEXT("Null"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Null;
        return true;
    }
    if (Value.Equals(TEXT("Control"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Control;
        return true;
    }
    if (Value.Equals(TEXT("Curve"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Curve;
        return true;
    }
    if (Value.Equals(TEXT("Reference"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Reference;
        return true;
    }
    if (Value.Equals(TEXT("Connector"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Connector;
        return true;
    }
    if (Value.Equals(TEXT("Socket"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Socket;
        return true;
    }
    return false;
}
bool IsControlRigSampleParentType(ERigElementType Type)
{
    return Type == ERigElementType::Bone || Type == ERigElementType::Null || Type == ERigElementType::Control;
}
bool TryParseControlRigProbeName(
    const FString& RequestedName,
    const FString& FieldPath,
    FName& OutName,
    FString& OutError,
    const bool bAllowNone)
{
    OutError.Reset();if(!SafeText(RequestedName,NAME_SIZE-1)) { OutError=TEXT("Name contains NUL or exceeds actual FName bound");return false; }
    const FString NormalizedName = RequestedName.TrimStartAndEnd();
    if (NormalizedName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), *FieldPath);
        return false;
    }
    if (NormalizedName.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(
            TEXT("'%s' is too long for an Unreal name (maximum %d characters)"),
            *FieldPath,
            NAME_SIZE - 1);
        return false;
    }

    OutName = FName(*NormalizedName);
    if (!bAllowNone && OutName.IsNone())
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' name '%s': name resolves to None"),
            *FieldPath,
            *RequestedName);
        return false;
    }
    return true;
}
void CollectControlRigSampleParentMatches(
    URigHierarchy* Hierarchy,
    const TArray<FRigElementKey>& AvailableDirectParentKeys,
    const FName ParentName,
    const TArray<ERigElementType>& ProbeTypes,
    TArray<FRigElementKey>& OutMatches)
{
    for (const ERigElementType Type : ProbeTypes)
    {
        for (const FRigElementKey& DirectParentKey : AvailableDirectParentKeys)
        {
            if (DirectParentKey.Type == Type && DirectParentKey.Name == ParentName)
            {
                AddUniqueControlRigSampleParentMatch(OutMatches, DirectParentKey);
            }
        }

        if (Hierarchy)
        {
            const FRigElementKey Candidate(ParentName, Type);
            if (Hierarchy->Contains(Candidate))
            {
                AddUniqueControlRigSampleParentMatch(OutMatches, Candidate);
            }
        }
    }
}
void AddUniqueControlRigSampleParentMatch(TArray<FRigElementKey>& Matches, const FRigElementKey& Key)
{
    if (Key.IsValid() && IsControlRigSampleParentType(Key.Type) && !Matches.Contains(Key))
    {
        Matches.Add(Key);
    }
}
int32 GetControlRigSampleRigVMAllowedUnitNodeCount()
{
    return 6;
}
FString GetControlRigSampleRigVMAllowedUnitsString()
{
    return TEXT("begin_execution, pre_begin_execution, post_begin_execution, prepare_for_execution, post_prepare_for_execution, inverse_execution");
}
FString NormalizeControlRigSampleRigVMUnitType(FString UnitType)
{
    UnitType.TrimStartAndEndInline();
    FString UnitTypeKey = UnitType.ToLower();
    UnitTypeKey.ReplaceInline(TEXT("_"), TEXT(""));
    UnitTypeKey.ReplaceInline(TEXT("-"), TEXT(""));
    UnitTypeKey.ReplaceInline(TEXT(" "), TEXT(""));
    UnitTypeKey.ReplaceInline(TEXT("."), TEXT(""));

    if (UnitTypeKey == TEXT("begin") ||
        UnitTypeKey == TEXT("beginexecution") ||
        UnitTypeKey == TEXT("rigunitbeginexecution") ||
        UnitTypeKey == TEXT("frigunitbeginexecution") ||
        UnitTypeKey == TEXT("forwardssolve"))
    {
        return TEXT("begin_execution");
    }

    if (UnitTypeKey == TEXT("prebegin") ||
        UnitTypeKey == TEXT("prebeginexecution") ||
        UnitTypeKey == TEXT("rigunitprebeginexecution") ||
        UnitTypeKey == TEXT("frigunitprebeginexecution") ||
        UnitTypeKey == TEXT("preforward") ||
        UnitTypeKey == TEXT("preforwardssolve"))
    {
        return TEXT("pre_begin_execution");
    }

    if (UnitTypeKey == TEXT("postbegin") ||
        UnitTypeKey == TEXT("postbeginexecution") ||
        UnitTypeKey == TEXT("rigunitpostbeginexecution") ||
        UnitTypeKey == TEXT("frigunitpostbeginexecution") ||
        UnitTypeKey == TEXT("postforward") ||
        UnitTypeKey == TEXT("postforwardssolve"))
    {
        return TEXT("post_begin_execution");
    }

    if (UnitTypeKey == TEXT("prepare") ||
        UnitTypeKey == TEXT("prepareforexecution") ||
        UnitTypeKey == TEXT("rigunitprepareforexecution") ||
        UnitTypeKey == TEXT("frigunitprepareforexecution") ||
        UnitTypeKey == TEXT("construction") ||
        UnitTypeKey == TEXT("constructionevent"))
    {
        return TEXT("prepare_for_execution");
    }

    if (UnitTypeKey == TEXT("postprepare") ||
        UnitTypeKey == TEXT("postprepareforexecution") ||
        UnitTypeKey == TEXT("rigunitpostprepareforexecution") ||
        UnitTypeKey == TEXT("frigunitpostprepareforexecution") ||
        UnitTypeKey == TEXT("postconstruction") ||
        UnitTypeKey == TEXT("postconstructionevent"))
    {
        return TEXT("post_prepare_for_execution");
    }

    if (UnitTypeKey == TEXT("inverse") ||
        UnitTypeKey == TEXT("inverseexecution") ||
        UnitTypeKey == TEXT("rigunitinverseexecution") ||
        UnitTypeKey == TEXT("frigunitinverseexecution") ||
        UnitTypeKey == TEXT("backwards") ||
        UnitTypeKey == TEXT("backwardssolve"))
    {
        return TEXT("inverse_execution");
    }

    return FString();
}
UScriptStruct* GetControlRigSampleRigVMUnitScriptStruct(const FString& UnitType)
{
    if (UnitType == TEXT("begin_execution"))
    {
        return FRigUnit_BeginExecution::StaticStruct();
    }
    if (UnitType == TEXT("pre_begin_execution"))
    {
        return FRigUnit_PreBeginExecution::StaticStruct();
    }
    if (UnitType == TEXT("post_begin_execution"))
    {
        return FRigUnit_PostBeginExecution::StaticStruct();
    }
    if (UnitType == TEXT("prepare_for_execution"))
    {
        return FRigUnit_PrepareForExecution::StaticStruct();
    }
    if (UnitType == TEXT("post_prepare_for_execution"))
    {
        return FRigUnit_PostPrepareForExecution::StaticStruct();
    }
    if (UnitType == TEXT("inverse_execution"))
    {
        return FRigUnit_InverseExecution::StaticStruct();
    }
    return nullptr;
}
TSharedPtr<FJsonObject> ControlRigSampleRigVMUnitSpecToJson(const FControlRigSampleRigVMUnitNodeSpec& Spec, int32 Index)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetStringField(TEXT("type"), Spec.UnitType);
    Object->SetStringField(TEXT("node_name"), Spec.NodeName);
    Object->SetObjectField(TEXT("position"), Vector2DToJsonObject(Spec.Position));
    Object->SetBoolField(TEXT("allowlisted"), true);
    Object->SetStringField(TEXT("method_name"), TEXT("Execute"));
    if (UScriptStruct* ScriptStruct = GetControlRigSampleRigVMUnitScriptStruct(Spec.UnitType))
    {
        Object->SetStringField(TEXT("script_struct"), ScriptStruct->GetName());
        Object->SetStringField(TEXT("script_struct_path"), ScriptStruct->GetPathName());
    }
    return Object;
}
TSharedPtr<FJsonObject> Vector2DToJsonObject(const FVector2D& Vector)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Vector.X);
    Object->SetNumberField(TEXT("y"), Vector.Y);
    return Object;
}
TArray<TSharedPtr<FJsonValue>> ControlRigSampleRigVMUnitSpecsToJsonValues(const TArray<FControlRigSampleRigVMUnitNodeSpec>& Specs)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        Values.Add(MakeShared<FJsonValueObject>(ControlRigSampleRigVMUnitSpecToJson(Specs[Index], Index)));
    }
    return Values;
}
bool ParseControlRigSampleRigVMUnitNodeSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxRigVMUnitNodes,
    TArray<FControlRigSampleRigVMUnitNodeSpec>& OutSpecs,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* UnitNodeValues = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("unit_nodes"), UnitNodeValues) || !UnitNodeValues || UnitNodeValues->Num() == 0)
    {
        return true;
    }

    if (MaxRigVMUnitNodes <= 0)
    {
        OutError = TEXT("unit_nodes were supplied but max_unit_nodes is 0");
        return false;
    }
    if (UnitNodeValues->Num() > MaxRigVMUnitNodes)
    {
        OutError = FString::Printf(TEXT("unit_nodes contains %d entries, max_unit_nodes=%d"), UnitNodeValues->Num(), MaxRigVMUnitNodes);
        return false;
    }

    const TArray<FString> AllowedUnitObjectFields = {
        TEXT("type"),
        TEXT("unit_type"),
        TEXT("unit"),
        TEXT("kind"),
        TEXT("name"),
        TEXT("node_name"),
        TEXT("position"),
        TEXT("graph_position"),
        TEXT("x"),
        TEXT("y")
    };
    const TArray<FString> UnsupportedUnitObjectFields = {
        TEXT("script_struct"),
        TEXT("script_struct_path"),
        TEXT("struct_path"),
        TEXT("method"),
        TEXT("method_name"),
        TEXT("pins"),
        TEXT("pin"),
        TEXT("pin_defaults"),
        TEXT("defaults"),
        TEXT("default_values"),
        TEXT("variables"),
        TEXT("variable"),
        TEXT("functions"),
        TEXT("function"),
        TEXT("comments"),
        TEXT("comment"),
        TEXT("reroute"),
        TEXT("reroute_nodes"),
        TEXT("dispatch"),
        TEXT("dispatch_node"),
        TEXT("dispatch_nodes"),
        TEXT("exposed_pins"),
        TEXT("expose_pins"),
        TEXT("graph"),
        TEXT("graph_path"),
        TEXT("replace_graph"),
        TEXT("rigvm"),
        TEXT("links"),
        TEXT("unit_links"),
        TEXT("rigvm_links")
    };

    TSet<FString> SeenTypes;
    TSet<FString> SeenNames;
    for (int32 Index = 0; Index < UnitNodeValues->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& UnitNodeValue = (*UnitNodeValues)[Index];
        FControlRigSampleRigVMUnitNodeSpec Spec;
        FString RawUnitType;

        if (!UnitNodeValue.IsValid())
        {
            OutError = FString::Printf(TEXT("unit_nodes[%d] is invalid"), Index);
            return false;
        }

        if (UnitNodeValue->Type == EJson::String)
        {
            RawUnitType = UnitNodeValue->AsString();
        }
        else if (UnitNodeValue->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> UnitObject = UnitNodeValue->AsObject();
            if (!UnitObject.IsValid())
            {
                OutError = FString::Printf(TEXT("unit_nodes[%d] must be a string or object"), Index);
                return false;
            }

            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : UnitObject->Values)
            {
                if (UnsupportedUnitObjectFields.Contains(Pair.Key))
                {
                    OutError = FString::Printf(
                        TEXT("unit_nodes[%d].%s is not supported; this pass only supports allowlisted event unit node evidence with optional name and position"),
                        Index,
                        *Pair.Key);
                    return false;
                }
                if (!AllowedUnitObjectFields.Contains(Pair.Key))
                {
                    OutError = FString::Printf(
                        TEXT("unit_nodes[%d].%s is not supported; allowed fields are type/unit_type/unit/kind, name/node_name, and position/graph_position/x/y"),
                        Index,
                        *Pair.Key);
                    return false;
                }
            }

            TSharedPtr<FJsonValue> TypeValue;
            if (!TryGetFirstJsonField(UnitObject, {TEXT("type"), TEXT("unit_type"), TEXT("unit"), TEXT("kind")}, TypeValue) || !TypeValue.IsValid())
            {
                OutError = FString::Printf(TEXT("unit_nodes[%d] must specify an allowlisted event unit type"), Index);
                return false;
            }
            if (TypeValue->Type != EJson::String)
            {
                OutError = FString::Printf(TEXT("unit_nodes[%d].type must be a string"), Index);
                return false;
            }
            RawUnitType = TypeValue->AsString();

            Spec.NodeName = GetStringParam(UnitObject, TEXT("name"));
            if (Spec.NodeName.IsEmpty())
            {
                Spec.NodeName = GetStringParam(UnitObject, TEXT("node_name"));
            }

            TSharedPtr<FJsonValue> PositionValue;
            if (TryGetFirstJsonField(UnitObject, {TEXT("position"), TEXT("graph_position")}, PositionValue))
            {
                if (!JsonValueToVector2D(PositionValue, Spec.Position, FString::Printf(TEXT("unit_nodes[%d].position"), Index), OutError))
                {
                    return false;
                }
            }
            else if (UnitObject->HasField(TEXT("x")) || UnitObject->HasField(TEXT("y")))
            {
                double X = 0.0;
                double Y = 0.0;
                if (!UnitObject->TryGetNumberField(TEXT("x"), X) || !UnitObject->TryGetNumberField(TEXT("y"), Y))
                {
                    OutError = FString::Printf(TEXT("unit_nodes[%d].x and unit_nodes[%d].y must both be numeric when either is supplied"), Index, Index);
                    return false;
                }
                Spec.Position = FVector2D(X, Y);
            }
        }
        else
        {
            OutError = FString::Printf(TEXT("unit_nodes[%d] must be a string or object"), Index);
            return false;
        }

        const FString NormalizedType = NormalizeControlRigSampleRigVMUnitType(RawUnitType);
        if (NormalizedType.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("unit_nodes[%d] type '%s' is not supported; this pass only allows %s"),
                Index,
                *RawUnitType,
                *GetControlRigSampleRigVMAllowedUnitsString());
            return false;
        }
        Spec.UnitType = NormalizedType;

        Spec.NodeName.TrimStartAndEndInline();
        if (Spec.NodeName.IsEmpty())
        {
            Spec.NodeName = GetControlRigSampleRigVMUnitDefaultNodeName(Spec.UnitType);
        }
        if (Spec.NodeName.Contains(TEXT("/")) || Spec.NodeName.Contains(TEXT("\\")) || Spec.NodeName.Contains(TEXT(".")) || Spec.NodeName.Contains(TEXT(":")))
        {
            OutError = FString::Printf(TEXT("unit_nodes[%d].name contains an unsupported character: %s"), Index, *Spec.NodeName);
            return false;
        }

        if (SeenTypes.Contains(Spec.UnitType))
        {
            OutError = FString::Printf(TEXT("Duplicate ControlRig sample RigVM unit type: %s"), *Spec.UnitType);
            return false;
        }
        SeenTypes.Add(Spec.UnitType);

        const FString NodeNameKey = Spec.NodeName.ToLower();
        if (SeenNames.Contains(NodeNameKey))
        {
            OutError = FString::Printf(TEXT("Duplicate ControlRig sample RigVM unit node name: %s"), *Spec.NodeName);
            return false;
        }
        SeenNames.Add(NodeNameKey);
        OutSpecs.Add(Spec);
    }

    return true;
}
bool JsonValueToVector2D(const TSharedPtr<FJsonValue>& Value, FVector2D& OutVector, const FString& FieldPath, FString& OutError)
{
    if (!Value.IsValid())
    {
        return true;
    }

    if (Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
        if (Values.Num() != 2 || !Values[0].IsValid() || !Values[1].IsValid() || Values[0]->Type != EJson::Number || Values[1]->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("%s must be [x, y] with numeric values"), *FieldPath);
            return false;
        }
        OutVector = FVector2D(Values[0]->AsNumber(), Values[1]->AsNumber());
        return true;
    }

    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        double X = 0.0;
        double Y = 0.0;
        const bool bHasX = Object.IsValid() && (Object->TryGetNumberField(TEXT("x"), X) || Object->TryGetNumberField(TEXT("X"), X));
        const bool bHasY = Object.IsValid() && (Object->TryGetNumberField(TEXT("y"), Y) || Object->TryGetNumberField(TEXT("Y"), Y));
        if (!bHasX || !bHasY)
        {
            OutError = FString::Printf(TEXT("%s must contain numeric x and y fields"), *FieldPath);
            return false;
        }
        OutVector = FVector2D(X, Y);
        return true;
    }

    OutError = FString::Printf(TEXT("%s must be [x, y] or {x, y}"), *FieldPath);
    return false;
}
FString GetControlRigSampleRigVMUnitDefaultNodeName(const FString& UnitType)
{
    if (UnitType == TEXT("pre_begin_execution"))
    {
        return TEXT("MCP_PreBeginExecution");
    }
    if (UnitType == TEXT("post_begin_execution"))
    {
        return TEXT("MCP_PostBeginExecution");
    }
    if (UnitType == TEXT("prepare_for_execution"))
    {
        return TEXT("MCP_PrepareForExecution");
    }
    if (UnitType == TEXT("post_prepare_for_execution"))
    {
        return TEXT("MCP_PostPrepareForExecution");
    }
    if (UnitType == TEXT("inverse_execution"))
    {
        return TEXT("MCP_InverseExecution");
    }
    return TEXT("MCP_BeginExecution");
}
TSharedPtr<FJsonObject> ControlRigSampleRigVMUnitNodeToJsonObject(
    const URigVMUnitNode* UnitNode,
    const FControlRigSampleRigVMUnitNodeSpec& Spec,
    int32 Index,
    bool bNodeAdded,
    bool bNodeReused)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("requested_index"), Index);
    Object->SetStringField(TEXT("requested_type"), Spec.UnitType);
    Object->SetStringField(TEXT("requested_node_name"), Spec.NodeName);
    Object->SetObjectField(TEXT("requested_position"), Vector2DToJsonObject(Spec.Position));
    Object->SetBoolField(TEXT("node_added"), bNodeAdded);
    Object->SetBoolField(TEXT("node_reused"), bNodeReused);
    if (!UnitNode)
    {
        Object->SetBoolField(TEXT("valid"), false);
        return Object;
    }

    Object->SetBoolField(TEXT("valid"), true);
    Object->SetStringField(TEXT("node_name"), UnitNode->GetName());
    Object->SetStringField(TEXT("node_title"), UnitNode->GetNodeTitle());
    Object->SetStringField(TEXT("method_name"), UnitNode->GetMethodName().ToString());
    Object->SetObjectField(TEXT("position"), Vector2DToJsonObject(UnitNode->GetPosition()));
    Object->SetBoolField(TEXT("is_event"), UnitNode->IsEvent());
    Object->SetStringField(TEXT("event_name"), UnitNode->GetEventName().ToString());
    Object->SetBoolField(TEXT("can_only_exist_once"), UnitNode->CanOnlyExistOnce());
    if (UScriptStruct* ScriptStruct = UnitNode->GetScriptStruct())
    {
        Object->SetStringField(TEXT("script_struct"), ScriptStruct->GetName());
        Object->SetStringField(TEXT("script_struct_path"), ScriptStruct->GetPathName());
    }

    const TArray<URigVMPin*>& Pins = UnitNode->GetPins();
    TArray<TSharedPtr<FJsonValue>> PinValues;
    for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
    {
        PinValues.Add(MakeShared<FJsonValueObject>(RigVMPinToJsonObject(Pins[PinIndex], PinIndex)));
    }
    Object->SetNumberField(TEXT("pin_count"), Pins.Num());
    Object->SetArrayField(TEXT("pins"), PinValues);
    return Object;
}
TSharedPtr<FJsonObject> RigVMPinToJsonObject(const URigVMPin* Pin, int32 Index)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), Index);
    if (!Pin)
    {
        Object->SetBoolField(TEXT("valid"), false);
        return Object;
    }

    Object->SetBoolField(TEXT("valid"), true);
    Object->SetStringField(TEXT("name"), Pin->GetName());
    Object->SetStringField(TEXT("display_name"), Pin->GetDisplayName().ToString());
    Object->SetStringField(TEXT("pin_path"), Pin->GetPinPath());
    Object->SetStringField(TEXT("direction"), RigVMPinDirectionToString(Pin->GetDirection()));
    Object->SetStringField(TEXT("cpp_type"), Pin->GetCPPType());
    Object->SetStringField(TEXT("default_value"), Pin->GetDefaultValue());
    Object->SetBoolField(TEXT("is_execute_context"), Pin->IsExecuteContext());
    return Object;
}
FString RigVMPinDirectionToString(ERigVMPinDirection Direction)
{
    switch (Direction)
    {
        case ERigVMPinDirection::Input:
        {
            return TEXT("input");
        }
        case ERigVMPinDirection::Output:
        {
            return TEXT("output");
        }
        case ERigVMPinDirection::IO:
        {
            return TEXT("io");
        }
        case ERigVMPinDirection::Visible:
        {
            return TEXT("visible");
        }
        case ERigVMPinDirection::Hidden:
        {
            return TEXT("hidden");
        }
        default:
        {
            return TEXT("invalid");
        }
    }
}
URigVMUnitNode* FindControlRigSampleRigVMUnitNode(URigVMGraph* Graph, UScriptStruct* ScriptStruct)
{
    if (!Graph || !ScriptStruct)
    {
        return nullptr;
    }

    for (URigVMNode* Node : Graph->GetNodes())
    {
        URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node);
        if (UnitNode && UnitNode->GetScriptStruct() == ScriptStruct)
        {
            return UnitNode;
        }
    }
    return nullptr;
}
FString NormalizePackagePathForPostProcessVariant(const FString& RawPath)
{
    FString PackagePath = FPackageName::ExportTextPathToObjectPath(RawPath).TrimStartAndEnd();
    PackagePath.TrimQuotesInline();
    PackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));

    int32 DotIndex = INDEX_NONE;
    if (PackagePath.FindChar(TEXT('.'), DotIndex))
    {
        PackagePath.LeftInline(DotIndex);
    }

    while (PackagePath.Contains(TEXT("//")))
    {
        PackagePath.ReplaceInline(TEXT("//"), TEXT("/"));
    }

    while (PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath.LeftChopInline(1);
    }

    return PackagePath;
}
FString SanitizeAssetNameSegmentForAnimNodeProbe(const FString& RawName)
{
    FString Result = RawName;
    for (TCHAR& Character : Result)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
        {
            Character = TEXT('_');
        }
    }
    Result.TrimStartAndEndInline();
    return Result.IsEmpty() ? TEXT("AnimNodeProbe") : Result;
}
bool IsValidGamePackagePathForPostProcessVariant(const FString& PackagePath, FString& OutError)
{
    if (PackagePath.IsEmpty())
    {
        OutError = TEXT("Package path cannot be empty");
        return false;
    }
    if (!PackagePath.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive))
    {
        OutError = FString::Printf(TEXT("Expected a /Game package path, got: %s"), *PackagePath);
        return false;
    }
    if (PackagePath.Contains(TEXT(".")))
    {
        OutError = FString::Printf(TEXT("Expected a package path without object suffix, got: %s"), *PackagePath);
        return false;
    }
    return true;
}
bool IsAllowedAnimStateMachineSampleTargetPath(const FString& PackagePath)
{
    return IsAllowedPostProcessVariantTargetPath(PackagePath)
        || PackagePath.Equals(TEXT("/Game/_MCP_Temp"), ESearchCase::IgnoreCase)
        || PackagePath.StartsWith(TEXT("/Game/_MCP_Temp/"), ESearchCase::IgnoreCase);
}
bool IsAllowedPostProcessVariantTargetPath(const FString& PackagePath)
{
    return PackagePath.Equals(TEXT("/Game/_MCP_Sample"), ESearchCase::IgnoreCase)
        || PackagePath.StartsWith(TEXT("/Game/_MCP_Sample/"), ESearchCase::IgnoreCase);
}
bool GetJsonBoolFieldOrDefault(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, bool bDefaultValue)
{
    bool bValue = bDefaultValue;
    return Object.IsValid() && Object->TryGetBoolField(FieldName, bValue) ? bValue : bDefaultValue;
}
bool ApplySampleCompileSaveOutcome(
    const TSharedPtr<FJsonObject>& ResultObj,
    const TSharedPtr<FJsonObject>& CompileResult,
    bool bCompileRequested,
    bool bSaveRequested,
    bool bTargetSaved)
{
    if (!ResultObj.IsValid())
    {
        return false;
    }

    const bool bCompileSucceeded = !bCompileRequested || GetJsonBoolFieldOrDefault(CompileResult, TEXT("success"), false);
    const bool bSaveSucceeded = !bSaveRequested || bTargetSaved;
    const bool bOverallSuccess = bCompileSucceeded && bSaveSucceeded;

    ResultObj->SetBoolField(TEXT("compile_succeeded"), bCompileSucceeded);
    ResultObj->SetBoolField(TEXT("save_succeeded"), bSaveSucceeded);
    ResultObj->SetBoolField(TEXT("success"), bOverallSuccess);

    if (!bOverallSuccess)
    {
        TArray<TSharedPtr<FJsonValue>> CompletionErrorValues;
        if (!bCompileSucceeded)
        {
            CompletionErrorValues.Add(MakeShared<FJsonValueString>(TEXT("compile_failed")));
        }
        if (!bSaveSucceeded)
        {
            CompletionErrorValues.Add(MakeShared<FJsonValueString>(TEXT("save_failed")));
        }
        ResultObj->SetArrayField(TEXT("completion_errors"), CompletionErrorValues);
    }

    return bOverallSuccess;
}
void SetSamplePartialTargetMutationStatus(
    const TSharedPtr<FJsonObject>& ResponseObj,
    UObject* TargetAsset,
    const FString& TargetAssetPath,
    bool bPartialTargetModified)
{
    if (!ResponseObj.IsValid())
    {
        return;
    }

    ResponseObj->SetBoolField(TEXT("partial_target_modified"), bPartialTargetModified);
    ResponseObj->SetBoolField(TEXT("target_modified"), bPartialTargetModified);
    ResponseObj->SetBoolField(TEXT("target_asset_modified"), bPartialTargetModified);
    ResponseObj->SetStringField(TEXT("target_asset_path"), TargetAssetPath);
    ResponseObj->SetBoolField(TEXT("target_dirty_after"), TargetAsset && TargetAsset->GetOutermost() && TargetAsset->GetOutermost()->IsDirty());
}
UEdGraphPin* FindPreferredOutputPosePinForNode(UEdGraphNode* Node)
{
    if (UEdGraphPin* ComponentPosePin = FindFirstComponentPosePin(Node, EGPD_Output, { TEXT("Pose"), TEXT("ComponentPose") }))
    {
        return ComponentPosePin;
    }
    return FindFirstPosePin(Node, EGPD_Output, { TEXT("Pose"), TEXT("Result") });
}
UEdGraphPin* FindFirstComponentPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const TArray<FString>& PreferredPinNames)
{
    if (!Node)
    {
        return nullptr;
    }

    for (const FString& PreferredName : PreferredPinNames)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (PinKnown(Pin)
                && Pin->Direction == Direction
                && !Pin->ParentPin
                && IsComponentPoseLinkPin(Pin)
                && Pin->PinName.ToString().Equals(PreferredName, ESearchCase::IgnoreCase))
            {
                return Pin;
            }
        }
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (PinKnown(Pin) && Pin->Direction == Direction && !Pin->ParentPin && IsComponentPoseLinkPin(Pin))
        {
            return Pin;
        }
    }

    return nullptr;
}
bool IsComponentPoseLinkPin(const UEdGraphPin* P)
{ return PinKnown(P)&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&P->PinType.PinSubCategoryObject.Get()==ComponentPoseStruct; }
bool TargetSkeletonSafe(UAnimBlueprint* BP)
{
    auto* Skeleton=BP->TargetSkeleton.Get();if(!Skeleton) { return true; }if(!Hold(Skeleton)) { return false; }const auto& Ref=Skeleton->GetReferenceSkeleton();const auto& Bones=Ref.GetRefBoneInfo();const auto& Poses=Ref.GetRefBonePose();if(Bones.Num()>4096||Bones.Num()!=Poses.Num()) { NoteUnsafe(TEXT("Target skeleton arrays exceed finite SDK mapping scope"));return false; }
    for(int32 I=0;I<Bones.Num();++I) { if(Bones[I].ParentIndex<-1||Bones[I].ParentIndex>=I||Poses[I].ContainsNaN()) { NoteUnsafe(TEXT("Target skeleton parent/transform admission failed before SDK template mapping"));return false; } }return true;
}
bool ExternalValueSafe(const FProperty* P,const void* V,int32 Depth,int32& Visits)
{
    if(!P||!V||Depth>8||++Visits>65536||P->ArrayDim!=1||P->ElementSize<1||P->ElementSize>1048576) { NoteUnsafe(TEXT("External descriptor value exceeds finite no-export shape/depth budget"));return false; }
    Count(*CurrentResult,TEXT("external_descriptor_value_reads"));
    if(CastField<FBoolProperty>(P)) { return true; }
    if(const auto* E=CastField<FEnumProperty>(P)) { return E->GetUnderlyingProperty()&&E->GetEnum()&&Hold(E->GetEnum())&&E->GetEnum()->NumEnums()<=4096; }
    if(const auto* B=CastField<FByteProperty>(P)) { return !B->Enum||(Hold(B->Enum)&&B->Enum->NumEnums()<=4096); }
    if(const auto* N=CastField<FNumericProperty>(P)) { return !N->IsFloatingPoint()||FMath::IsFinite(N->GetFloatingPointPropertyValue(V)); }
    if(const auto* S=CastField<FStrProperty>(P)) { return SafeText(S->GetPropertyValue(V),4096); }
    if(const auto* N=CastField<FNameProperty>(P)) { return SafeText(N->GetPropertyValue(V).ToString(),NAME_SIZE-1); }
    if(const auto* T=CastField<FTextProperty>(P)) { return SafeText(T->GetPropertyValue(V).ToString(),4096); }
    if(const auto* S=CastField<FSoftObjectProperty>(P)) { return SafeText(S->GetPropertyValue(V).ToString(),NAME_SIZE-1); }
    if(const auto* O=CastField<FObjectPropertyBase>(P)) { const UObject* Object=O->GetObjectPropertyValue(V);return Hold(Object)&&(!Object||SafeText(Object->GetPathName(),NAME_SIZE-1)); }
    if(const auto* A=CastField<FArrayProperty>(P)) { FScriptArrayHelper H(A,V);if(H.Num()>1024||!A->Inner) { return false; }for(int32 I=0;I<H.Num();++I) { if(!ExternalValueSafe(A->Inner,H.GetRawPtr(I),Depth+1,Visits)) { return false; } }return true; }
    if(const auto* M=CastField<FMapProperty>(P)) { FScriptMapHelper H(M,V);if(H.Num()>1024||H.GetMaxIndex()>4096||!M->KeyProp||!M->ValueProp) { return false; }for(int32 I=0;I<H.GetMaxIndex();++I) { if(H.IsValidIndex(I)&&(!ExternalValueSafe(M->KeyProp,H.GetKeyPtr(I),Depth+1,Visits)||!ExternalValueSafe(M->ValueProp,H.GetValuePtr(I),Depth+1,Visits))) { return false; } }return true; }
    if(const auto* S=CastField<FSetProperty>(P)) { FScriptSetHelper H(S,V);if(H.Num()>1024||H.GetMaxIndex()>4096||!S->ElementProp) { return false; }for(int32 I=0;I<H.GetMaxIndex();++I) { if(H.IsValidIndex(I)&&!ExternalValueSafe(S->ElementProp,H.GetElementPtr(I),Depth+1,Visits)) { return false; } }return true; }
    if(const auto* S=CastField<FStructProperty>(P))
    {
        if(!S->Struct||!Hold(S->Struct)||!Hold(S->Struct->GetOutermost())||S->Struct->GetOutermost()->GetName()!=TEXT("/Script/CoreUObject")) { return false; }
        const FString Name=S->Struct->GetName();if(Name!=TEXT("Vector")&&Name!=TEXT("Vector2D")&&Name!=TEXT("Vector4")&&Name!=TEXT("Rotator")&&Name!=TEXT("Quat")&&Name!=TEXT("Transform")&&Name!=TEXT("LinearColor")&&Name!=TEXT("Color")) { return false; }
        int32 Fields=0;for(TFieldIterator<FProperty> It(S->Struct);It;++It) { const auto* Field=*It;if(++Fields>32||!Layout(Field,S->Struct->GetStructureSize())||!ExternalValueSafe(Field,Field->ContainerPtrToValuePtr<void>(V),Depth+1,Visits)) { return false; } }return true;
    }
    return false; // Custom exporters/delegates remain valid but outside this finite first scope.
}

TSharedPtr<FJsonObject> Capture(UBlueprint* BP)
{
    Count(*CurrentResult,TEXT("state_capture_calls"));auto J=MakeShared<FJsonObject>();if(!BP||!Hold(BP)||!Hold(BP->GetOutermost())||!CurrentClassesSafe(BP)) { return J; }
    J->SetStringField(TEXT("blueprint"),BP->GetPathName());J->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());J->SetStringField(TEXT("generated_class"),GetPathNameSafe(BP->GeneratedClass.Get()));J->SetStringField(TEXT("skeleton_class"),GetPathNameSafe(BP->SkeletonGeneratedClass.Get()));
    if(auto* Rig=Cast<UControlRigBlueprint>(BP))
    { if(!HierarchySafe(Rig->Hierarchy)) { return J; }TArray<TSharedPtr<FJsonValue>> Keys;for(int32 I=0;I<Rig->Hierarchy->Num();++I) { Keys.Add(MakeShared<FJsonValueString>(Rig->Hierarchy->Get(I)->GetKey().ToString())); }J->SetArrayField(TEXT("hierarchy_keys"),Keys);auto* Model=Rig->GetDefaultModel();if(Model&&Hold(Model)) { TArray<TSharedPtr<FJsonValue>> Rows;if(Model->GetNodes().Num()>4096) { NoteUnsafe(TEXT("RigVM model node bound exceeded"));return J; }for(URigVMNode* N:Model->GetNodes()) { if(!N||!Hold(N)) { NoteUnsafe(TEXT("RigVM primary node unavailable"));return J; }Rows.Add(MakeShared<FJsonValueString>(N->GetNodePath())); }J->SetArrayField(TEXT("model_nodes"),Rows); }return J; }
    if(!RefreshInventory()) { return J; }TArray<UEdGraph*> Graphs;if(!Enumerate(BP,Graphs)) { return J; }TArray<TSharedPtr<FJsonValue>> Rows;
    for(UEdGraph* G:Graphs) { auto Row=GraphToJson(BP,G);TArray<TSharedPtr<FJsonValue>> Nodes;for(UEdGraphNode* N:G->Nodes) { if(!N) { continue; }if(!Hold(N)) { return J; }Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(N,true))); }Row->SetArrayField(TEXT("nodes"),Nodes);Rows.Add(MakeShared<FJsonValueObject>(Row)); }J->SetArrayField(TEXT("graphs"),Rows);return J;
}
bool CurrentClassesSafe(UBlueprint* BP)
{ if(!BP||!Hold(BP)||!Hold(BP->ParentClass.Get())) { return false; }for(UClass* C:{BP->GeneratedClass.Get(),BP->SkeletonGeneratedClass.Get()}) { if(C&&(!Hold(C)||C->ClassGeneratedBy!=BP||!C->IsChildOf(BP->ParentClass.Get()))) { NoteUnsafe(TEXT("Current generated/skeleton class ownership mismatch"));return false; } }return true; }
void RememberSource(UBlueprint* BP)
{ SourceBlueprint=BP;CurrentBlueprint=BP;if(!CurrentClassesSafe(BP)) { return; }SourceBefore=Capture(BP);CurrentResult->Data->SetStringField(TEXT("source_before_json"),Serialize(SourceBefore)); }
bool ClassForRigSafe(UClass* C)
{
    if(!C||!Hold(C)||!C->IsChildOf(UControlRig::StaticClass())||C->HasAnyClassFlags(CLASS_Abstract)) { NoteUnsafe(TEXT("Loaded concrete ControlRig class required before original constructor"));return false; }
    int32 Parents=0;for(UClass* P=C;P;P=P->GetSuperClass()) { if(++Parents>32||!Hold(P)) { NoteUnsafe(TEXT("Rig class lineage unavailable"));return false; } }
    auto* BP=Cast<UControlRigBlueprint>(C->ClassGeneratedBy);const bool Native=C->IsNative();
    if(!Native&&(!BP||!Hold(BP)||BP->GeneratedClass.Get()!=C||!CurrentClassesSafe(BP))) { NoteUnsafe(TEXT("Rig generated owner/class does not match current Blueprint"));return false; }
    auto* CDO=Cast<UControlRig>(C->GetDefaultObject(false));if(!CDO||!Hold(CDO)||CDO->GetClass()!=C||CDO!=C->ClassDefaultObject) { NoteUnsafe(TEXT("Existing current loaded ControlRig CDO required"));return false; }
    const auto Owner=CDO->GetGeneratedByAsset();if(Owner.GetObject()||Owner.GetInterface()) { NoteUnsafe(TEXT("Asset-owned runtime bag outside current finite class branch"));return false; }
    if(!HierarchySafe(CDO->GetHierarchy())) { return false; }int32 Visits=0,Fields=0;
    for(TFieldIterator<FProperty> It(C);It;++It) { auto* P=*It;if(++Fields>4096||!Layout(P,C->GetPropertiesSize())||!Hold(P->GetOwnerStruct())||(Native&&!P->IsNative())) { NoteUnsafe(TEXT("Rig external descriptor current layout/native lineage unavailable"));return false; }if(P->IsNative()) { continue; }for(int32 I=0;I<P->ArrayDim;++I) { if(!ExternalValueSafe(P,P->ContainerPtrToValuePtr<void>(CDO,I),0,Visits)) { NoteUnsafe(TEXT("Rig external descriptor finite no-export admission failed"));return false; } } }
    if(BP) { if(!RigModelsSafe(BP)) { return false; }auto* Skeleton=BP->SkeletonGeneratedClass.Get();if(!Skeleton||!Hold(Skeleton)||Skeleton->ClassGeneratedBy!=BP||!Skeleton->IsChildOf(UControlRig::StaticClass())) { NoteUnsafe(TEXT("Matching current Rig skeleton class required"));return false; } }
    return true;
}
UObject* LoadAssetObjectForRuntimeProbe(const FString& Path) { return CountedLoad<UObject>(NormalizeAssetObjectPathForLoad(Path),*CurrentResult); }
UObject* LoadObjectForPin(const FString& Path) { return LoadAssetObjectForRuntimeProbe(Path); }
UClass* LoadClassForPin(const FString& Path) { return Cast<UClass>(CountedLoad<UObject>(Path,*CurrentResult)); }
UControlRig* CreateTransientControlRigFromParams(const TSharedPtr<FJsonObject>& P,FString& AssetPath,FString& ClassPath,FString& Error)
{
    AssetPath.Reset();ClassPath.Reset();Error.Reset();FString C=GetStringParam(P,TEXT("control_rig_class"));if(C.IsEmpty()) { C=GetStringParam(P,TEXT("control_rig_class_path")); }FString A=GetStringParam(P,TEXT("control_rig_path"));if(A.IsEmpty()) { A=GetStringParam(P,TEXT("control_rig_asset")); }if(A.IsEmpty()) { A=GetStringParam(P,TEXT("control_rig_asset_path")); }if(A.IsEmpty()) { A=GetStringParam(P,TEXT("control_rig")); }
    UControlRigBlueprint* BP=nullptr;UClass* Class=nullptr;
    if(!C.TrimStartAndEnd().IsEmpty()) { Class=LoadClassForPin(C); }
    else if(!A.TrimStartAndEnd().IsEmpty()) { auto* O=LoadObjectForPin(A);BP=Cast<UControlRigBlueprint>(O);Class=BP?RigBlueprintClass(BP):Cast<UClass>(O);if(!Class&&O&&!BP) { Class=O->GetClass(); }AssetPath=GetPathNameSafe(O);if(BP) { SourceRigBlueprint=BP;CurrentResult->Data->SetBoolField(TEXT("source_rig_dirty_before_transient"),BP->GetOutermost()->IsDirty());CurrentResult->Data->SetStringField(TEXT("source_rig_class_before_transient"),GetPathNameSafe(Class)); } }
    else { Error=TEXT("Missing 'control_rig_path' or 'control_rig_class' parameter");return nullptr; }
    if(!ClassForRigSafe(Class)) { Error=CurrentResult->Error.IsEmpty()?TEXT("ControlRig class not found or invalid"):CurrentResult->Error;return nullptr; }ClassPath=Class->GetPathName();Count(*CurrentResult,TEXT("transient_rig_create_calls"));CurrentResult->Data->SetBoolField(TEXT("transient_template_initialization_effects_qualified"),true);UControlRig* Rig=BP?BP->CreateControlRig():NewObject<UControlRig>(GetTransientPackage(),Class);Count(*CurrentResult,TEXT("transient_rig_create_returns"));if(!Rig||!Hold(Rig)||!HierarchySafe(Rig->GetHierarchy())) { Error=TEXT("Current transient ControlRig creation unavailable");return nullptr; }return Rig;
}
bool Exists(const FString& Path) { if(!SamplePackageSafe(Path)) { return false; }Count(*CurrentResult,TEXT("asset_exists_calls"));return UEditorAssetLibrary::DoesAssetExist(Path); }
UObject* AssetLoad(const FString& Path) { if(!SamplePackageSafe(Path)) { return nullptr; }Count(*CurrentResult,TEXT("editor_asset_load_calls"));auto* O=UEditorAssetLibrary::LoadAsset(Path);Count(*CurrentResult,TEXT("editor_asset_load_returns"));return Hold(O)?O:nullptr; }
bool Delete(const FString& Path)
{ if(Incomplete()||!SamplePackageSafe(Path)) { return false; }WriteIntent(TEXT("asset_delete_calls"));const bool V=UEditorAssetLibrary::DeleteAsset(Path);Count(*CurrentResult,TEXT("asset_delete_returns"));CurrentResult->bChanged|=V;CurrentResult->Data->SetBoolField(TEXT("last_delete_return"),V);if(!V) { CurrentResult->Data->SetBoolField(TEXT("unproven_asset_writer_return"),true); }return V; }
void MakeDir(const FString& Path) { if(Incomplete()||!SamplePackageSafe(Path)) { return; }WriteIntent(TEXT("make_directory_calls"));const bool V=UEditorAssetLibrary::MakeDirectory(Path);CurrentResult->Data->SetBoolField(TEXT("last_make_directory_return"),V); }
UBlueprint* DuplicateAnimBlueprintForNodeProbe(UBlueprint* Source,const FString& Path,bool Overwrite,TArray<TSharedPtr<FJsonValue>>& Errors)
{ if(!Source||!Hold(Source)||!SamplePackageSafe(Path)||Incomplete()) { Errors.Add(MakeShared<FJsonValueString>(TEXT("Source/target admission failed before duplicate")));return nullptr; }if(Exists(Path)) { if(!Overwrite) { Errors.Add(MakeShared<FJsonValueString>(TEXT("Temp asset already exists")));return nullptr; }if(!Delete(Path)) { Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to delete existing temp asset")));return nullptr; } }WriteIntent(TEXT("asset_duplicate_calls"));auto* O=UEditorAssetLibrary::DuplicateAsset(Source->GetOutermost()->GetName(),Path);Count(*CurrentResult,TEXT("asset_duplicate_returns"));auto* BP=Cast<UAnimBlueprint>(O);if(!BP||!RefreshTarget(BP)) { CurrentResult->Data->SetBoolField(TEXT("unproven_asset_writer_return"),true);Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed current duplicated Anim Blueprint")));return nullptr; }CurrentResult->bChanged=true;TargetBefore=Capture(BP);return BP; }
UControlRigBlueprint* Factory(const FString& Path,bool Modular)
{ if(Incomplete()||!SamplePackageSafe(Path)) { return nullptr; }WriteIntent(TEXT("rig_factory_calls"));CurrentResult->Data->SetBoolField(TEXT("factory_implicit_full_compile_possible"),true);auto* BP=UControlRigBlueprintFactory::CreateNewControlRigAsset(Path,Modular);Count(*CurrentResult,TEXT("rig_factory_returns"));if(!BP||!RefreshTarget(BP)) { CurrentResult->Data->SetBoolField(TEXT("unproven_asset_writer_return"),true);return nullptr; }CurrentResult->bChanged=true;TargetBefore=Capture(BP);return BP; }
bool Save(const FString& Path,bool OnlyIfDirty)
{ if(Incomplete()||!SamplePackageSafe(Path)||!SelectedTargetAsset||!Hold(SelectedTargetAsset)) { return false; }const FString ObjectPath=NormalizeAssetObjectPathForLoad(Path);const FString PackagePath=FPackageName::ObjectPathToPackageName(ObjectPath);if(!SelectedTargetAsset->GetOutermost()->GetName().Equals(PackagePath,ESearchCase::IgnoreCase)||!SelectedTargetAsset->GetPathName().Equals(ObjectPath,ESearchCase::IgnoreCase)||FindObject<UObject>(nullptr,*ObjectPath)!=SelectedTargetAsset||!RefreshTarget(Cast<UBlueprint>(SelectedTargetAsset))) { NoteUnsafe(TEXT("Stored save path no longer resolves to exact loaded current target; no save issued"));return false; }CurrentResult->bSaveAttempted=true;Count(*CurrentResult,TEXT("force_save_calls"));CurrentResult->Stage=TEXT("force_save");const bool V=UEditorAssetLibrary::SaveAsset(Path,OnlyIfDirty);Count(*CurrentResult,TEXT("force_save_returns"));AdoptSaved(V);return V; }
TSharedPtr<FJsonObject> AnimNodeProbeCompileBlueprint(UBlueprint* BP)
{ auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("requested"),true);if(Incomplete()||!BP||!Hold(BP)||!CurrentClassesSafe(BP)) { J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),TEXT("Current compile target unavailable"));return J; }if(auto* Rig=Cast<UControlRigBlueprint>(BP);Rig&&!RigModelsSafe(Rig)) { J->SetBoolField(TEXT("success"),false);return J; }auto& R=*CurrentResult;R.bCompileAttempted=true;R.Stage=TEXT("compile");Count(R,TEXT("full_compile_calls"));FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);Count(R,TEXT("full_compile_returns"));R.Compiled=Loaded(BP->GeneratedClass.Get())&&Log.NumErrors==0&&(BP->Status==BS_UpToDate||BP->Status==BS_UpToDateWithWarnings);auto Actual=MakeShared<FJsonObject>();Actual->SetNumberField(TEXT("errors"),Log.NumErrors);Actual->SetNumberField(TEXT("warnings"),Log.NumWarnings);Actual->SetNumberField(TEXT("status"),static_cast<int32>(BP->Status));Actual->SetBoolField(TEXT("healthy"),R.Compiled.GetValue());R.Data->SetObjectField(TEXT("compiler_result"),Actual);J->SetBoolField(TEXT("success"),BP->GeneratedClass!=nullptr);J->SetStringField(TEXT("blueprint"),BP->GetPathName());J->SetStringField(TEXT("generated_class"),GetPathNameSafe(BP->GeneratedClass.Get()));R.Data->SetBoolField(TEXT("legacy_generated_class_compile_predicate"),BP->GeneratedClass!=nullptr);if(!RefreshTarget(BP)) { NoteUnsafe(TEXT("Target unavailable after actual full compile; compiler prefix retained")); }return J; }
USkeletalMesh* Preview(UControlRigBlueprint* BP)
{ if(!BP||!Hold(BP)||Incomplete()) { return nullptr; }const auto PreviewReference=BP->GetPreviewSkeletalMesh();auto* Current=PreviewReference.Get();if(!PreviewReference.IsNull()&&!Current) { NoteUnsafe(TEXT("Non-null unresolved soft preview reference before SDK synchronous getter"));return nullptr; }if(Current&&!Hold(Current)) { return nullptr; }Count(*CurrentResult,TEXT("preview_mesh_getter_calls"));auto* V=UControlRigBlueprintEditorLibrary::GetPreviewMesh(BP);return Hold(V)?V:nullptr; }
void SetPreview(UControlRigBlueprint* BP,USkeletalMesh* Mesh,bool Modify)
{ if(!BP||!Hold(BP)||!Mesh||!Hold(Mesh)||Incomplete()) { return; }WriteIntent(TEXT("set_preview_mesh_calls"));UControlRigBlueprintEditorLibrary::SetPreviewMesh(BP,Mesh,Modify);if(!RefreshTarget(BP)) { return; }CurrentResult->bChanged=true; }
URigHierarchyController* Controller(UControlRigBlueprint* BP)
{ if(!BP||!Hold(BP)||!HierarchySafe(BP->Hierarchy)||Incomplete()) { return nullptr; }Count(*CurrentResult,TEXT("hierarchy_controller_getter_calls"));auto* C=BP->GetHierarchyController();return C&&Hold(C)&&C->GetHierarchy()==BP->Hierarchy?C:nullptr; }
void Propagate(UControlRigBlueprint* BP)
{ if(!BP||!Hold(BP)||!HierarchySafe(BP->Hierarchy)||Incomplete()) { return; }WriteIntent(TEXT("propagate_hierarchy_calls"));CurrentResult->Data->SetBoolField(TEXT("propagation_archetype_initialization_effects_qualified"),true);BP->PropagateHierarchyFromBPToInstances();if(!HierarchySafe(BP->Hierarchy)) { return; }CurrentResult->bChanged=true; }
void RequestInit(UControlRigBlueprint* BP) { if(!BP||!Hold(BP)||!RigModelsSafe(BP)||Incomplete()) { return; }WriteIntent(TEXT("request_rig_init_calls"));UControlRigBlueprintEditorLibrary::RequestControlRigInit(BP);RefreshTarget(BP); }
UControlRig* CreateRig(UControlRigBlueprint* BP)
{ auto* Class=RigBlueprintClass(BP);if(!ClassForRigSafe(Class)||Incomplete()) { return nullptr; }Count(*CurrentResult,TEXT("transient_rig_create_calls"));CurrentResult->Data->SetBoolField(TEXT("transient_template_initialization_effects_qualified"),true);auto* V=BP->CreateControlRig();Count(*CurrentResult,TEXT("transient_rig_create_returns"));return V&&Hold(V)&&HierarchySafe(V->GetHierarchy())?V:nullptr; }
void AutoRecompile(UControlRigBlueprint* BP)
{ if(!BP||!Hold(BP)||!RigModelsSafe(BP)||Incomplete()) { return; }WriteIntent(TEXT("auto_VM_recompile_requests"));CurrentResult->Data->SetBoolField(TEXT("auto_VM_compile_possible"),true);BP->RequestAutoVMRecompilation();RefreshTarget(BP); }
bool ModelSafe(URigVMGraph* G)
{ if(!G||!Hold(G)||G->GetNodes().Num()>4096) { NoteUnsafe(TEXT("Current bounded RigVM model required"));return false; }int32 Pins=0;TSet<const URigVMPin*> Seen;TFunction<bool(URigVMPin*,int32)> Visit=[&](URigVMPin* P,int32 D) { if(!P||D>16||++Pins>32768||Seen.Contains(P)||!Hold(P)) { NoteUnsafe(TEXT("RigVM pin tree unavailable/cyclic/bounded"));return false; }Seen.Add(P);for(URigVMPin* S:P->GetSubPins()) { if(!Visit(S,D+1)) { return false; } }return true; };for(URigVMNode* N:G->GetNodes()) { if(!N||!Hold(N)) { return false; }for(URigVMPin* P:N->GetPins()) { if(!Visit(P,0)) { return false; } } }return true; }
URigVMGraph* Model(UControlRigBlueprint* BP) { if(!BP||!Hold(BP)||Incomplete()) { return nullptr; }auto* G=BP->GetDefaultModel();return ModelSafe(G)?G:nullptr; }
URigVMController* VMController(UControlRigBlueprint* BP,URigVMGraph* G) { if(!BP||!Hold(BP)||!ModelSafe(G)||Incomplete()) { return nullptr; }Count(*CurrentResult,TEXT("VM_controller_getter_calls"));auto* C=BP->GetOrCreateController(G);return C&&Hold(C)?C:nullptr; }
URigVMUnitNode* AddUnit(URigVMController* C,UScriptStruct* S,const FName& Method,const FVector2D& Pos,const FString& Name,bool Undo,bool Python)
{ if(!C||!Hold(C)||!S||!Hold(S)||!RequiredName(Name)||!FloatProjectionSafe(Pos.X)||!FloatProjectionSafe(Pos.Y)||Incomplete()) { return nullptr; }WriteIntent(TEXT("add_unit_node_calls"));auto* N=C->AddUnitNode(S,Method,Pos,Name,Undo,Python);Count(*CurrentResult,TEXT("add_unit_node_returns"));if(N) { CurrentResult->bChanged=true; }return N&&Hold(N)?N:nullptr; }
TSharedPtr<FJsonObject> HandleInspectAnimGraphProtectedTopology(const TSharedPtr<FJsonObject>& P)
{ Count(*CurrentResult,TEXT("nested_topology_read_calls"));const auto R=AnimGraphInspection::Topology(GetStringParam(P,TEXT("blueprint_name")),GetStringParam(P,TEXT("node_id")),TEXT(""),TEXT(""),true,true,true,false,true,4,256,512,GetStringParam(P,TEXT("graph_name")),TEXT(""),TEXT("function"));CurrentResult->Data->SetObjectField(TEXT("nested_topology_native_data"),R.Data);const TSharedPtr<FJsonObject>* Legacy=nullptr;if(!R.bSuccess||!R.Data->TryGetObjectField(TEXT("legacy_result"),Legacy)||!Legacy) { NoteUnsafe(TEXT("Original composite topology readback incomplete"));return CreateErrorResponse(TEXT("Topology readback incomplete")); }return *Legacy; }
UAnimGraphNode_ControlRig* FindControlRigNodeByClassPath(UEdGraph* G,const FString& Path)
{ if(!G||!Hold(G)||!RefreshInventory()) { return nullptr; }for(UEdGraphNode* Node:G->Nodes) { auto* N=Cast<UAnimGraphNode_ControlRig>(Node);if(!N) { continue; }if(!Member(N,G)) { return nullptr; }const auto Ref=N->Node.GetControlRigAssetReference();auto* C=Ref.GetBlueprintClass().Get();auto* A=Ref.GetControlRigAsset().Get();if(!Hold(C)||!Hold(A)) { return nullptr; }Count(*CurrentResult,TEXT("rig_asset_reference_getter_calls"));const FString Current=GetPathNameSafe(C);if(Current.Equals(Path,ESearchCase::IgnoreCase)) { return N; } }return nullptr; }

bool RigModelsSafe(UControlRigBlueprint* BP)
{
    if(!BP||!Hold(BP)||!Hold(BP->GetClass())||BP->GetClass()!=UControlRigBlueprint::StaticClass()) { NoteUnsafe(TEXT("Exact loaded legacy Rig Blueprint owner required before VM compilation/initialization"));return false; }const FRigVMClient* Client=static_cast<const URigVMBlueprint*>(BP)->GetRigVMClient();if(!Client) { NoteUnsafe(TEXT("Current RigVM client absent before transient constructor"));return false; }
    // This public nonrecursive inventory precedes the compiler's recursive model traversal.
    if(Client->Num()>64) { NoteUnsafe(TEXT("Root model count exceeds64 before SDK inventory allocation"));return false; }const auto Models=Client->GetAllModels(false,false);if(Models.Num()>64) { NoteUnsafe(TEXT("Root RigVM model inventory exceeds finite64 bound"));return false; }
    const auto* Library=Client->GetFunctionLibrary();if(Library&&(!Hold(Library)||Library->GetNodes().Num()!=0)) { NoteUnsafe(TEXT("Nonempty function library remains valid but outside this finite sample-builder VM basis"));return false; }
    for(URigVMGraph* G:Models)
    { if(!ModelSafe(G)) { return false; }for(URigVMNode* N:G->GetNodes()) { auto* Unit=Cast<URigVMUnitNode>(N);if(!Unit||Unit->GetClass()!=URigVMUnitNode::StaticClass()) { NoteUnsafe(TEXT("Unaudited RigVM node/contained-graph work outside finite allowlisted-six event basis"));return false; }auto* S=Unit->GetScriptStruct();if(!S||!Hold(S)) { return false; }bool Allowed=false;for(const TCHAR* Type:{TEXT("begin_execution"),TEXT("pre_begin_execution"),TEXT("post_begin_execution"),TEXT("prepare_for_execution"),TEXT("post_prepare_for_execution"),TEXT("inverse_execution")}) { Allowed|=S==GetControlRigSampleRigVMUnitScriptStruct(Type); }if(!Allowed) { NoteUnsafe(TEXT("Existing VM unit outside source-audited event basis; no implicit compile invoked"));return false; } } }
    return true;
}


template<class... A> TArray<FRigElementKey> CheckedImportBones(URigHierarchyController* C,A&&... Args)
{ if(!C||!Hold(C)||!HierarchySafe(C->GetHierarchy())||Incomplete()) { return {}; }WriteIntent(TEXT("import_bones_calls"));auto V=C->ImportBones(Forward<A>(Args)...);Count(*CurrentResult,TEXT("import_bones_returns"));if(!HierarchySafe(C->GetHierarchy())) { return {}; }CurrentResult->bChanged|=V.Num()>0;return V; }
template<class... A> TArray<FRigElementKey> CheckedImportCurves(URigHierarchyController* C,A&&... Args)
{ if(!C||!Hold(C)||!HierarchySafe(C->GetHierarchy())||Incomplete()) { return {}; }WriteIntent(TEXT("import_curves_calls"));auto V=C->ImportCurves(Forward<A>(Args)...);Count(*CurrentResult,TEXT("import_curves_returns"));if(!HierarchySafe(C->GetHierarchy())) { return {}; }CurrentResult->bChanged|=V.Num()>0;return V; }
template<class... A> TArray<FRigElementKey> CheckedImportCurvesFromSkeletalMesh(URigHierarchyController* C,A&&... Args)
{ if(!C||!Hold(C)||!HierarchySafe(C->GetHierarchy())||Incomplete()) { return {}; }WriteIntent(TEXT("import_mesh_curves_calls"));auto V=C->ImportCurvesFromSkeletalMesh(Forward<A>(Args)...);Count(*CurrentResult,TEXT("import_mesh_curves_returns"));if(!HierarchySafe(C->GetHierarchy())) { return {}; }CurrentResult->bChanged|=V.Num()>0;return V; }
template<class... A> FRigElementKey CheckedAddCurve(URigHierarchyController* C,A&&... Args)
{ if(!C||!Hold(C)||!HierarchySafe(C->GetHierarchy())||Incomplete()) { return {}; }WriteIntent(TEXT("add_curve_calls"));const auto V=C->AddCurve(Forward<A>(Args)...);Count(*CurrentResult,TEXT("add_curve_returns"));CurrentResult->bChanged|=V.IsValid();if(!HierarchySafe(C->GetHierarchy())||(V.IsValid()&&!C->GetHierarchy()->Find(V))) { NoteUnsafe(TEXT("Current curve key missing after SDK callback")); }return V; }
template<class... A> FRigElementKey CheckedAddNull(URigHierarchyController* C,A&&... Args)
{ if(!C||!Hold(C)||!HierarchySafe(C->GetHierarchy())||Incomplete()) { return {}; }WriteIntent(TEXT("add_null_calls"));const auto V=C->AddNull(Forward<A>(Args)...);Count(*CurrentResult,TEXT("add_null_returns"));CurrentResult->bChanged|=V.IsValid();return V; }
template<class... A> FRigElementKey CheckedAddControl(URigHierarchyController* C,A&&... Args)
{ if(!C||!Hold(C)||!HierarchySafe(C->GetHierarchy())||Incomplete()) { return {}; }WriteIntent(TEXT("add_control_calls"));const auto V=C->AddControl(Forward<A>(Args)...);Count(*CurrentResult,TEXT("add_control_returns"));CurrentResult->bChanged|=V.IsValid();return V; }

TSharedPtr<FJsonObject> RunNode(const TSharedPtr<FJsonObject>& Params)
{
    FString SourceAnimBPPath = GetStringParam(Params, TEXT("source_anim_bp_path"));
    if (SourceAnimBPPath.IsEmpty())
    {
        SourceAnimBPPath = GetStringParam(Params, TEXT("source_blueprint_name"));
    }
    if (SourceAnimBPPath.IsEmpty())
    {
        SourceAnimBPPath = GetStringParam(Params, TEXT("blueprint_name"));
    }
    if (SourceAnimBPPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing 'source_anim_bp_path' parameter"));
    }

    UAnimBlueprint* SourceAnimBlueprint = Cast<UAnimBlueprint>(FindBlueprint(SourceAnimBPPath));
    if (!SourceAnimBlueprint)
    {
        SourceAnimBlueprint = Cast<UAnimBlueprint>(LoadAssetObjectForRuntimeProbe(SourceAnimBPPath));
    }
    if (!SourceAnimBlueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Source Anim Blueprint not found: %s"), *SourceAnimBPPath));
    }

    if(!Hold(SourceAnimBlueprint)||!TargetSkeletonSafe(SourceAnimBlueprint)) { return CreateErrorResponse(CurrentResult->Error); }RememberSource(SourceAnimBlueprint);
    FString ControlRigAssetPath;
    FString ControlRigClassPath;
    FString ControlRigResolveError;
    UControlRig* TransientControlRig = CreateTransientControlRigFromParams(Params, ControlRigAssetPath, ControlRigClassPath, ControlRigResolveError);
    if (!TransientControlRig || !TransientControlRig->GetClass()->IsChildOf(UControlRig::StaticClass()))
    {
        return CreateErrorResponse(ControlRigResolveError.IsEmpty() ? TEXT("Failed to resolve ControlRig class") : ControlRigResolveError);
    }
    UClass* ControlRigClass = TransientControlRig->GetClass();
    ControlRigClassPath = ControlRigClass->GetPathName();

    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), false) || GetBoolParam(Params, TEXT("validate_only"), false);
    const bool bCompile = GetBoolParam(Params, TEXT("compile"), true);
    const bool bSave = GetBoolParam(Params, TEXT("save_asset"), GetBoolParam(Params, TEXT("save"), false));
    const bool bAllowExistingTargetReuse = GetBoolParam(Params, TEXT("allow_existing_target_reuse"), true);
    const bool bOverwriteExisting = GetBoolParam(Params, TEXT("overwrite_existing"), false);
    const bool bReplaceExistingLinks = GetBoolParam(Params, TEXT("replace_existing_links"), true);

    FString VariantName = SanitizeAssetNameSegmentForAnimNodeProbe(GetStringParam(Params, TEXT("variant_name"), TEXT("ControlRigNodeSample")));
    const FString TargetRoot = NormalizePackagePathForPostProcessVariant(GetStringParam(Params, TEXT("target_root"), TEXT("/Game/_MCP_Sample/AnimStudy")));
    FString TargetAnimBPPath = NormalizePackagePathForPostProcessVariant(GetStringParam(Params, TEXT("target_anim_bp_path")));
    if (TargetAnimBPPath.IsEmpty())
    {
        TargetAnimBPPath = NormalizePackagePathForPostProcessVariant(GetStringParam(Params, TEXT("target_blueprint_path")));
    }
    if (TargetAnimBPPath.IsEmpty())
    {
        TargetAnimBPPath = FString::Printf(
            TEXT("%s/%s_%s"),
            *TargetRoot,
            *SanitizeAssetNameSegmentForAnimNodeProbe(SourceAnimBlueprint->GetName()),
            *VariantName);
    }

    if(!SamplePackageSafe(TargetRoot)||!SamplePackageSafe(TargetAnimBPPath)) { return CreateErrorResponse(CurrentResult->Error); }
    FString PathError;
    if (!IsValidGamePackagePathForPostProcessVariant(TargetRoot, PathError))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Invalid target_root. %s"), *PathError));
    }
    if (!IsValidGamePackagePathForPostProcessVariant(TargetAnimBPPath, PathError))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Invalid target_anim_bp_path. %s"), *PathError));
    }
    if (!IsAllowedAnimStateMachineSampleTargetPath(TargetAnimBPPath))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Refusing to create or edit ControlRig node sample outside /Game/_MCP_Sample or /Game/_MCP_Temp: %s"),
            *TargetAnimBPPath));
    }
    if (SourceAnimBlueprint->GetOutermost()->GetName().Equals(TargetAnimBPPath, ESearchCase::IgnoreCase))
    {
        return CreateErrorResponse(TEXT("Refusing to use the source AnimBP as the target sample AnimBP"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("command"), TEXT("ensure_anim_graph_controlrig_node_sample"));
    ResultObj->SetBoolField(TEXT("sample_only"), true);
    ResultObj->SetBoolField(TEXT("dry_run"), bDryRun);
    ResultObj->SetBoolField(TEXT("source_modified"), false);
    ResultObj->SetBoolField(TEXT("source_asset_modified"), false);
    ResultObj->SetStringField(TEXT("source_anim_bp_path"), SourceAnimBlueprint->GetPathName());
    ResultObj->SetStringField(TEXT("target_anim_bp_path"), TargetAnimBPPath);
    ResultObj->SetStringField(TEXT("target_root"), TargetRoot);
    ResultObj->SetStringField(TEXT("variant_name"), VariantName);
    ResultObj->SetStringField(TEXT("control_rig_asset_path"), ControlRigAssetPath);
    ResultObj->SetStringField(TEXT("control_rig_class_path"), ControlRigClassPath);
    ResultObj->SetBoolField(TEXT("compile_requested"), bCompile);
    ResultObj->SetBoolField(TEXT("save_requested"), bSave);
    ResultObj->SetBoolField(TEXT("replace_existing_links"), bReplaceExistingLinks);
    ResultObj->SetStringField(TEXT("insert_mode"), TEXT("input_pose_to_controlrig_to_root"));

    if (Params->HasField(TEXT("input_defaults")) || Params->HasField(TEXT("expose_input_pins")))
    {
        TArray<TSharedPtr<FJsonValue>> WarningValues;
        WarningValues.Add(MakeShared<FJsonValueString>(TEXT("input_defaults and expose_input_pins are not applied by this v1 command; run set_anim_graph_controlrig_input_defaults on the target sample after creation.")));
        ResultObj->SetArrayField(TEXT("warnings"), WarningValues);
    }

    if (bDryRun)
    {
        ResultObj->SetBoolField(TEXT("target_modified"), false);
        ResultObj->SetBoolField(TEXT("target_asset_saved"), false);
        ResultObj->SetStringField(TEXT("runtime_note"), TEXT("Dry run only: no AnimBP was duplicated, edited, compiled, or saved."));
        return ResultObj;
    }

    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    UAnimBlueprint* TargetAnimBlueprint = nullptr;
    bool bTargetCreated = false;
    bool bTargetReused = false;
    const bool bTargetExists = Exists(TargetAnimBPPath);
    if (bTargetExists && bAllowExistingTargetReuse && !bOverwriteExisting)
    {
        TargetAnimBlueprint = Cast<UAnimBlueprint>(AssetLoad(TargetAnimBPPath));
        if (!TargetAnimBlueprint)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Existing target is not an AnimBlueprint: %s"), *TargetAnimBPPath));
        }
        bTargetReused = true;
    }
    else
    {
        TargetAnimBlueprint = Cast<UAnimBlueprint>(DuplicateAnimBlueprintForNodeProbe(SourceAnimBlueprint, TargetAnimBPPath, bOverwriteExisting, ErrorValues));
        bTargetCreated = TargetAnimBlueprint != nullptr;
    }

    if (!TargetAnimBlueprint || ErrorValues.Num() > 0)
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(TEXT("Failed to prepare target sample AnimBP"));
        ErrorObj->SetArrayField(TEXT("errors"), ErrorValues);
        ErrorObj->SetStringField(TEXT("source_anim_bp_path"), SourceAnimBlueprint->GetPathName());
        ErrorObj->SetStringField(TEXT("target_anim_bp_path"), TargetAnimBPPath);
        ErrorObj->SetBoolField(TEXT("source_asset_modified"), false);
        return ErrorObj;
    }

    if(!RefreshTarget(TargetAnimBlueprint)) { return CreateErrorResponse(CurrentResult->Error); }TargetBefore=Capture(TargetAnimBlueprint);
    auto AddAnimGraphControlRigPartialTargetFields = [&](const TSharedPtr<FJsonObject>& ErrorObj, bool bGraphChanged)
    {
        const bool bPartialTargetModified = bTargetCreated || bGraphChanged;
        ErrorObj->SetBoolField(TEXT("target_created"), bTargetCreated);
        ErrorObj->SetBoolField(TEXT("target_reused"), bTargetReused);
        ErrorObj->SetStringField(TEXT("target_anim_bp_path"), TargetAnimBPPath);
        SetSamplePartialTargetMutationStatus(ErrorObj, TargetAnimBlueprint, TargetAnimBPPath, bPartialTargetModified);
    };

    auto MakeAnimGraphControlRigPartialError = [&](const FString& ErrorText, bool bGraphChanged) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(ErrorText);
        AddAnimGraphControlRigPartialTargetFields(ErrorObj, bGraphChanged);
        return ErrorObj;
    };

    TSharedPtr<FJsonObject> GraphParams = MakeShared<FJsonObject>();
    GraphParams->SetStringField(TEXT("graph_name"), GetStringParam(Params, TEXT("graph_name"), TEXT("AnimGraph")));
    GraphParams->SetStringField(TEXT("graph_type"), GetStringParam(Params, TEXT("graph_type"), TEXT("function")));
    FString GraphError;
    UEdGraph* TargetGraph = ResolveBlueprintGraphForNodeCommand(TargetAnimBlueprint, GraphParams, GraphError);
    if (!TargetGraph)
    {
        return MakeAnimGraphControlRigPartialError(GraphError, false);
    }
    if (!IsAnimationGraph(TargetGraph))
    {
        return MakeAnimGraphControlRigPartialError(FString::Printf(TEXT("Target graph is not an AnimationGraph: %s"), *TargetGraph->GetName()), false);
    }

    bool bGraphChanged = false;
    bool bCreatedInputPoseNode = false;
    UAnimGraphNode_LinkedInputPose* InputPoseNode = FindFirstNodeOfType<UAnimGraphNode_LinkedInputPose>(TargetGraph);
    if (!InputPoseNode)
    {
        InputPoseNode = AddAnimGraphNodeToGraph<UAnimGraphNode_LinkedInputPose>(TargetGraph, FVector2D(-480.0f, 0.0f));
        if (!InputPoseNode)
        {
            return MakeAnimGraphControlRigPartialError(TEXT("Failed to create LinkedInputPose node"), bGraphChanged);
        }
        bCreatedInputPoseNode = true;
        bGraphChanged = true;
    }

    bool bCreatedControlRigNode = false;
    UAnimGraphNode_ControlRig* ControlRigNode = FindControlRigNodeByClassPath(TargetGraph, ControlRigClassPath);
    if (!ControlRigNode)
    {
        ControlRigNode = AddAnimGraphNodeToGraph<UAnimGraphNode_ControlRig>(TargetGraph, FVector2D(-240.0f, 0.0f));
        if (!ControlRigNode)
        {
            return MakeAnimGraphControlRigPartialError(TEXT("Failed to create ControlRig AnimGraph node"), bGraphChanged);
        }
        if(!Member(ControlRigNode,TargetGraph)||!ClassForRigSafe(ControlRigClass)||!TargetSkeletonSafe(TargetAnimBlueprint)) { return MakeAnimGraphControlRigPartialError(CurrentResult->Error,true); }WriteIntent(TEXT("node_modify_calls"));
        ControlRigNode->Modify();
        WriteIntent(TEXT("set_rig_class_calls"));ControlRigNode->Node.SetControlRigClass(ControlRigClass);CurrentResult->bChanged=true;
        WriteIntent(TEXT("reconstruct_calls"));ControlRigNode->ReconstructNode();if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)) { return MakeAnimGraphControlRigPartialError(CurrentResult->Error,true); }
        bCreatedControlRigNode = true;
        bGraphChanged = true;
    }

    UAnimGraphNode_Root* RootNode = FindFirstNodeOfType<UAnimGraphNode_Root>(TargetGraph);
    if (!RootNode)
    {
        return MakeAnimGraphControlRigPartialError(FString::Printf(TEXT("AnimGraph root node not found in graph: %s"), *TargetGraph->GetName()), bGraphChanged);
    }

    UEdGraphPin* InputPoseOutputPin = FindFirstPosePin(InputPoseNode, EGPD_Output);
    UEdGraphPin* ControlRigInputPin = FindFirstPosePin(ControlRigNode, EGPD_Input, { TEXT("Source") });
    UEdGraphPin* ControlRigOutputPin = FindPreferredOutputPosePinForNode(ControlRigNode);
    UEdGraphPin* RootInputPin = FindFirstPosePin(RootNode, EGPD_Input, { TEXT("Result") });

    TArray<TSharedPtr<FJsonValue>> LinkValues;
    auto AddLinkResult = [&LinkValues](const FString& Name, bool bSuccess, bool bChanged, const FString& Error)
    {
        TSharedPtr<FJsonObject> LinkObject = MakeShared<FJsonObject>();
        LinkObject->SetStringField(TEXT("name"), Name);
        LinkObject->SetBoolField(TEXT("success"), bSuccess);
        LinkObject->SetBoolField(TEXT("changed"), bChanged);
        LinkObject->SetStringField(TEXT("error"), Error);
        LinkValues.Add(MakeShared<FJsonValueObject>(LinkObject));
    };

    FString LinkError;
    bool bChangedThisLink = false;
    bool bInputToRigConnected = EnsureAnimGraphConnection(TargetGraph, InputPoseOutputPin, ControlRigInputPin, bReplaceExistingLinks, bChangedThisLink, LinkError);
    AddLinkResult(TEXT("input_pose_to_controlrig"), bInputToRigConnected, bChangedThisLink, LinkError);
    bGraphChanged = bGraphChanged || bChangedThisLink;
    if (!bInputToRigConnected)
    {
        ErrorValues.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Failed to connect input pose to ControlRig: %s"), *LinkError)));
    }

    LinkError.Reset();
    bChangedThisLink = false;
    if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)||!Member(RootNode,TargetGraph)) { return MakeAnimGraphControlRigPartialError(CurrentResult->Error,bGraphChanged); }ControlRigOutputPin=FindPreferredOutputPosePinForNode(ControlRigNode);RootInputPin=FindFirstPosePin(RootNode,EGPD_Input,{TEXT("Result")});
    bool bRigToRootConnected = EnsureAnimGraphConnection(TargetGraph, ControlRigOutputPin, RootInputPin, bReplaceExistingLinks, bChangedThisLink, LinkError);
    AddLinkResult(TEXT("controlrig_to_root"), bRigToRootConnected, bChangedThisLink, LinkError);
    bGraphChanged = bGraphChanged || bChangedThisLink;
    if (!bRigToRootConnected)
    {
        ErrorValues.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Failed to connect ControlRig to root: %s"), *LinkError)));
    }

    if (ErrorValues.Num() > 0)
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(TEXT("Failed to author ControlRig node sample"));
        ErrorObj->SetArrayField(TEXT("errors"), ErrorValues);
        ErrorObj->SetArrayField(TEXT("created_links"), LinkValues);
        ErrorObj->SetBoolField(TEXT("target_modified"), bGraphChanged);
        ErrorObj->SetStringField(TEXT("target_anim_bp_path"), TargetAnimBPPath);
        AddAnimGraphControlRigPartialTargetFields(ErrorObj, bGraphChanged);
        return ErrorObj;
    }

    if (bGraphChanged)
    {
        WriteIntent(TEXT("mark_structurally_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetAnimBlueprint);if(!RefreshInventory()) { return MakeAnimGraphControlRigPartialError(CurrentResult->Error,true); }
        WriteIntent(TEXT("notify_graph_changed_calls"));TargetGraph->NotifyGraphChanged();if(!RefreshInventory()) { return MakeAnimGraphControlRigPartialError(CurrentResult->Error,true); }
    }

    TSharedPtr<FJsonObject> CompileResult = MakeShared<FJsonObject>();
    CompileResult->SetBoolField(TEXT("requested"), bCompile);
    if (bCompile)
    {
        CompileResult = AnimNodeProbeCompileBlueprint(TargetAnimBlueprint);
    }

    bool bTargetSaved = false;
    if (bSave)
    {
        bTargetSaved = Save(TargetAnimBPPath, false);
    }

    if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)||!Member(InputPoseNode,TargetGraph)||!Member(RootNode,TargetGraph)) { return MakeAnimGraphControlRigPartialError(CurrentResult->Error,bGraphChanged); }
    const auto Ref=ControlRigNode->Node.GetControlRigAssetReference();CurrentResult->Data->SetBoolField(TEXT("current_rig_class_matches"),Ref.GetBlueprintClass().Get()==ControlRigClass);CurrentResult->Data->SetBoolField(TEXT("current_reciprocal_pose_edges_verified"),FindFirstPosePin(InputPoseNode,EGPD_Output)&&FindFirstPosePin(ControlRigNode,EGPD_Input,{TEXT("Source")})&&FindFirstPosePin(InputPoseNode,EGPD_Output)->LinkedTo.Contains(FindFirstPosePin(ControlRigNode,EGPD_Input,{TEXT("Source")}))&&FindFirstPosePin(ControlRigNode,EGPD_Input,{TEXT("Source")})->LinkedTo.Contains(FindFirstPosePin(InputPoseNode,EGPD_Output))&&FindPreferredOutputPosePinForNode(ControlRigNode)&&FindFirstPosePin(RootNode,EGPD_Input,{TEXT("Result")})&&FindPreferredOutputPosePinForNode(ControlRigNode)->LinkedTo.Contains(FindFirstPosePin(RootNode,EGPD_Input,{TEXT("Result")}))&&FindFirstPosePin(RootNode,EGPD_Input,{TEXT("Result")})->LinkedTo.Contains(FindPreferredOutputPosePinForNode(ControlRigNode)));
    TSharedPtr<FJsonObject> TopologyParams = MakeShared<FJsonObject>();
    TopologyParams->SetStringField(TEXT("blueprint_name"), TargetAnimBPPath);
    TopologyParams->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
    TopologyParams->SetStringField(TEXT("graph_type"), TEXT("function"));
    TopologyParams->SetStringField(TEXT("node_id"), ControlRigNode->NodeGuid.ToString());
    TopologyParams->SetBoolField(TEXT("include_settings"), true);
    TopologyParams->SetBoolField(TEXT("include_pins"), true);
    TopologyParams->SetBoolField(TEXT("include_links"), true);

    ResultObj->SetBoolField(TEXT("target_created"), bTargetCreated);
    ResultObj->SetBoolField(TEXT("target_reused"), bTargetReused);
    ResultObj->SetBoolField(TEXT("created_input_pose_node"), bCreatedInputPoseNode);
    ResultObj->SetBoolField(TEXT("created_controlrig_node"), bCreatedControlRigNode);
    ResultObj->SetBoolField(TEXT("target_modified"), bGraphChanged);
    ResultObj->SetBoolField(TEXT("target_asset_saved"), bTargetSaved);
    ResultObj->SetStringField(TEXT("created_node_id"), ControlRigNode->NodeGuid.ToString());
    ResultObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
    ResultObj->SetObjectField(TEXT("input_pose_node"), NodeToJson(InputPoseNode, true));
    ResultObj->SetObjectField(TEXT("root_node"), NodeToJson(RootNode, true));
    ResultObj->SetArrayField(TEXT("created_links"), LinkValues);
    ResultObj->SetObjectField(TEXT("compile_result"), CompileResult);
    ResultObj->SetObjectField(TEXT("readback"), HandleInspectAnimGraphProtectedTopology(TopologyParams));
    AddGraphField(ResultObj, TargetAnimBlueprint, TargetGraph);
    ApplySampleCompileSaveOutcome(ResultObj, CompileResult, bCompile, bSave, bTargetSaved);
    return ResultObj;
}
TSharedPtr<FJsonObject> RunAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString TargetControlRigPath = NormalizePackagePathForPostProcessVariant(GetStringParam(Params, TEXT("target_control_rig_path")));
    if (TargetControlRigPath.IsEmpty())
    {
        TargetControlRigPath = NormalizePackagePathForPostProcessVariant(GetStringParam(Params, TEXT("target_controlrig_path")));
    }
    if (TargetControlRigPath.IsEmpty())
    {
        TargetControlRigPath = NormalizePackagePathForPostProcessVariant(GetStringParam(Params, TEXT("target_asset_path")));
    }
    if (TargetControlRigPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing 'target_control_rig_path' parameter"));
    }

    if(!SamplePackageSafe(TargetControlRigPath)) { return CreateErrorResponse(CurrentResult->Error); }
    FString PathError;
    if (!IsValidGamePackagePathForPostProcessVariant(TargetControlRigPath, PathError))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Invalid target_control_rig_path. %s"), *PathError));
    }
    if (!IsAllowedAnimStateMachineSampleTargetPath(TargetControlRigPath))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Refusing to create ControlRig sample outside /Game/_MCP_Sample or /Game/_MCP_Temp: %s"),
            *TargetControlRigPath));
    }

    TArray<FString> UnsupportedArrayFields;
    UnsupportedArrayFields.Add(TEXT("unit_links"));
    UnsupportedArrayFields.Add(TEXT("rigvm_links"));
    UnsupportedArrayFields.Add(TEXT("links"));
    UnsupportedArrayFields.Add(TEXT("variables"));
    UnsupportedArrayFields.Add(TEXT("functions"));
    UnsupportedArrayFields.Add(TEXT("comments"));
    UnsupportedArrayFields.Add(TEXT("reroute_nodes"));
    UnsupportedArrayFields.Add(TEXT("dispatch_nodes"));
    UnsupportedArrayFields.Add(TEXT("exposed_pins"));
    for (const FString& FieldName : UnsupportedArrayFields)
    {
        const TArray<TSharedPtr<FJsonValue>>* ArrayField = nullptr;
        if (Params->TryGetArrayField(FieldName, ArrayField) && ArrayField && ArrayField->Num() > 0)
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("unsupported_feature: direct RigVM '%s' authoring is not supported by ensure_controlrig_sample_asset"),
                *FieldName));
            ErrorObj->SetStringField(TEXT("unsupported_feature"), FieldName);
            ErrorObj->SetBoolField(TEXT("source_modified"), false);
            return ErrorObj;
        }
    }
    const TArray<FString> UnsupportedObjectFields = {
        TEXT("rigvm"),
        TEXT("script_struct"),
        TEXT("script_struct_path"),
        TEXT("script_structs"),
        TEXT("graph"),
        TEXT("graph_replacement"),
        TEXT("replace_graph")
    };
    for (const FString& FieldName : UnsupportedObjectFields)
    {
        if (Params->HasField(FieldName))
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("unsupported_feature: direct RigVM '%s' authoring is not supported by ensure_controlrig_sample_asset"),
                *FieldName));
            ErrorObj->SetStringField(TEXT("unsupported_feature"), FieldName);
            ErrorObj->SetBoolField(TEXT("source_modified"), false);
            return ErrorObj;
        }
    }

    FString SetupMode = GetStringParam(Params, TEXT("setup_mode"));
    SetupMode.TrimStartAndEndInline();
    if (SetupMode.IsEmpty())
    {
        SetupMode = TEXT("empty_with_preview");
    }
    const FString SetupModeLower = SetupMode.ToLower();
    const bool bImportPreviewHierarchy = SetupModeLower == TEXT("import_preview_hierarchy");
    if (SetupModeLower != TEXT("empty") && SetupModeLower != TEXT("empty_with_preview") && !bImportPreviewHierarchy)
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
            TEXT("unsupported_feature: setup_mode '%s' is not supported by ensure_controlrig_sample_asset"),
            *SetupMode));
        ErrorObj->SetStringField(TEXT("unsupported_feature"), TEXT("setup_mode"));
        ErrorObj->SetStringField(TEXT("setup_mode"), SetupMode);
        ErrorObj->SetBoolField(TEXT("source_modified"), false);
        return ErrorObj;
    }

    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), false) || GetBoolParam(Params, TEXT("validate_only"), false);
    const bool bOverwriteExisting = GetBoolParam(Params, TEXT("overwrite_existing"), false);
    const bool bAllowExistingTargetReuse = GetBoolParam(Params, TEXT("allow_existing_target_reuse"), true);
    const bool bCompile = GetBoolParam(Params, TEXT("compile"), true);
    const bool bSave = GetBoolParam(Params, TEXT("save_asset"), GetBoolParam(Params, TEXT("save"), false));
    const bool bModularRig = GetBoolParam(Params, TEXT("modular_rig"), false);
    const int32 MaxDirectCurves = FMath::Clamp(GetIntParam(Params, TEXT("max_direct_curves"), 32), 0, 128);
    const int32 MaxDirectNulls = FMath::Clamp(GetIntParam(Params, TEXT("max_direct_nulls"), 32), 0, 128);
    const int32 MaxDirectControls = FMath::Clamp(GetIntParam(Params, TEXT("max_direct_controls"), 32), 0, 128);
    const int32 MaxRigVMUnitNodes = FMath::Clamp(GetIntParam(Params, TEXT("max_unit_nodes"), GetControlRigSampleRigVMAllowedUnitNodeCount()), 0, GetControlRigSampleRigVMAllowedUnitNodeCount());

    TArray<FControlRigSampleCurveSpec> DirectCurveSpecs;
    FString DirectCurveParseError;
    if (!ParseControlRigSampleCurveSpecs(Params, MaxDirectCurves, DirectCurveSpecs, DirectCurveParseError))
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(DirectCurveParseError);
        ErrorObj->SetStringField(TEXT("unsupported_feature"), TEXT("curves"));
        ErrorObj->SetBoolField(TEXT("source_modified"), false);
        return ErrorObj;
    }
    for(const auto& Spec:DirectCurveSpecs) { if(!RequiredName(Spec.Name)||!FloatProjectionSafe(Spec.Value)) { return CreateErrorResponse(CurrentResult->Error); } }
    const bool bDirectCurveAuthoring = DirectCurveSpecs.Num() > 0;

    TArray<FControlRigSampleNullSpec> DirectNullSpecs;
    FString DirectNullParseError;
    if (!ParseControlRigSampleNullSpecs(Params, MaxDirectNulls, DirectNullSpecs, DirectNullParseError))
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(DirectNullParseError);
        ErrorObj->SetStringField(TEXT("unsupported_feature"), TEXT("nulls"));
        ErrorObj->SetBoolField(TEXT("source_modified"), false);
        return ErrorObj;
    }
    for(const auto& Spec:DirectNullSpecs) { if(!RequiredName(Spec.Name)||!RequiredName(Spec.ParentName)||Spec.Transform.ContainsNaN()) { return CreateErrorResponse(CurrentResult->Error); } }
    const bool bDirectNullAuthoring = DirectNullSpecs.Num() > 0;
    bool bDirectNullParentingRequested = false;
    bool bDirectNullTransformRequested = false;
    for (const FControlRigSampleNullSpec& Spec : DirectNullSpecs)
    {
        bDirectNullParentingRequested = bDirectNullParentingRequested || Spec.bHasParent;
        bDirectNullTransformRequested = bDirectNullTransformRequested || Spec.bHasTransform;
    }

    TArray<FControlRigSampleControlSpec> DirectControlSpecs;
    FString DirectControlParseError;
    if (!ParseControlRigSampleControlSpecs(Params, MaxDirectControls, DirectControlSpecs, DirectControlParseError))
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(DirectControlParseError);
        ErrorObj->SetStringField(TEXT("unsupported_feature"), TEXT("controls"));
        ErrorObj->SetBoolField(TEXT("source_modified"), false);
        return ErrorObj;
    }
    for(const auto& Spec:DirectControlSpecs) { if(!RequiredName(Spec.Name)||!RequiredName(Spec.ParentName)||!RequiredName(Spec.ShapeName)||!FloatProjectionSafe(Spec.FloatValue)||Spec.TransformValue.ContainsNaN()||Spec.ShapeTransform.ContainsNaN()) { return CreateErrorResponse(CurrentResult->Error); }if(ControlRigSampleControlIsTransformControl(Spec.ControlType)&&FRigControlValue::Make<FEulerTransform>(FEulerTransform(Spec.TransformValue)).GetAsTransform(Spec.ControlType,ERigControlAxis::X).ContainsNaN()) { NoteUnsafe(TEXT("Requested transform control overflows actual float storage"));return CreateErrorResponse(CurrentResult->Error); } }
    const bool bDirectControlAuthoring = DirectControlSpecs.Num() > 0;
    bool bDirectControlParentingRequested = false;
    bool bDirectControlShapeCustomizationRequested = false;
    bool bDirectTransformControlRequested = false;
    for (const FControlRigSampleControlSpec& Spec : DirectControlSpecs)
    {
        bDirectControlParentingRequested = bDirectControlParentingRequested || Spec.bHasParent;
        bDirectControlShapeCustomizationRequested = bDirectControlShapeCustomizationRequested || ControlRigSampleControlHasShapeCustomization(Spec);
        bDirectTransformControlRequested = bDirectTransformControlRequested || ControlRigSampleControlIsTransformControl(Spec.ControlType);
    }

    TArray<FControlRigSampleRigVMUnitNodeSpec> RigVMUnitNodeSpecs;
    FString RigVMUnitNodeParseError;
    if (!ParseControlRigSampleRigVMUnitNodeSpecs(Params, MaxRigVMUnitNodes, RigVMUnitNodeSpecs, RigVMUnitNodeParseError))
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(RigVMUnitNodeParseError);
        ErrorObj->SetStringField(TEXT("unsupported_feature"), TEXT("unit_nodes"));
        ErrorObj->SetBoolField(TEXT("source_modified"), false);
        return ErrorObj;
    }
    for(const auto& Spec:RigVMUnitNodeSpecs) { if(!RequiredName(Spec.NodeName)||!FloatProjectionSafe(Spec.Position.X)||!FloatProjectionSafe(Spec.Position.Y)) { return CreateErrorResponse(CurrentResult->Error); } }
    const bool bRigVMUnitNodeAuthoring = RigVMUnitNodeSpecs.Num() > 0;

    const FString SkeletonPath = GetStringParam(Params, TEXT("skeleton_path"));
    const FString SkeletalMeshPath = GetStringParam(Params, TEXT("skeletal_mesh_path"));
    FString PreviewSkeletalMeshPath = GetStringParam(Params, TEXT("preview_skeletal_mesh_path"));
    if (PreviewSkeletalMeshPath.IsEmpty())
    {
        PreviewSkeletalMeshPath = SkeletalMeshPath;
    }

    USkeleton* SourceSkeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        SourceSkeleton = Cast<USkeleton>(LoadAssetObjectForRuntimeProbe(SkeletonPath));
        if (!SourceSkeleton)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));
        }
    }

    USkeletalMesh* SourceSkeletalMesh = nullptr;
    if (!SkeletalMeshPath.IsEmpty())
    {
        SourceSkeletalMesh = Cast<USkeletalMesh>(LoadAssetObjectForRuntimeProbe(SkeletalMeshPath));
        if (!SourceSkeletalMesh)
        {
            return CreateErrorResponse(FString::Printf(TEXT("SkeletalMesh not found: %s"), *SkeletalMeshPath));
        }
    }

    USkeletalMesh* PreviewSkeletalMesh = nullptr;
    if (!PreviewSkeletalMeshPath.IsEmpty())
    {
        PreviewSkeletalMesh = Cast<USkeletalMesh>(LoadAssetObjectForRuntimeProbe(PreviewSkeletalMeshPath));
        if (!PreviewSkeletalMesh)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Preview SkeletalMesh not found: %s"), *PreviewSkeletalMeshPath));
        }
    }

    if (SourceSkeleton && SourceSkeletalMesh && SourceSkeletalMesh->GetSkeleton() != SourceSkeleton)
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("SkeletalMesh skeleton does not match explicit Skeleton. mesh_skeleton='%s' skeleton='%s'"),
            SourceSkeletalMesh->GetSkeleton() ? *SourceSkeletalMesh->GetSkeleton()->GetPathName() : TEXT("<none>"),
            *SourceSkeleton->GetPathName()));
    }
    if (SourceSkeleton && PreviewSkeletalMesh && PreviewSkeletalMesh->GetSkeleton() != SourceSkeleton)
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Preview SkeletalMesh skeleton does not match explicit Skeleton. preview_skeleton='%s' skeleton='%s'"),
            PreviewSkeletalMesh->GetSkeleton() ? *PreviewSkeletalMesh->GetSkeleton()->GetPathName() : TEXT("<none>"),
            *SourceSkeleton->GetPathName()));
    }

    USkeleton* ResolvedImportSkeleton = SourceSkeleton;
    if (!ResolvedImportSkeleton && SourceSkeletalMesh)
    {
        ResolvedImportSkeleton = SourceSkeletalMesh->GetSkeleton();
    }
    if (bImportPreviewHierarchy)
    {
        if (!SourceSkeletalMesh && !ResolvedImportSkeleton)
        {
            return CreateErrorResponse(TEXT("setup_mode=import_preview_hierarchy requires skeletal_mesh_path or skeleton_path"));
        }
        if (SourceSkeletalMesh && !ResolvedImportSkeleton)
        {
            return CreateErrorResponse(FString::Printf(
                TEXT("SkeletalMesh has no Skeleton; cannot import ControlRig preview hierarchy: %s"),
                *SourceSkeletalMesh->GetPathName()));
        }
        if (ResolvedImportSkeleton && PreviewSkeletalMesh && PreviewSkeletalMesh->GetSkeleton() != ResolvedImportSkeleton)
        {
            return CreateErrorResponse(FString::Printf(
                TEXT("Preview SkeletalMesh skeleton does not match import skeleton. preview_skeleton='%s' import_skeleton='%s'"),
                PreviewSkeletalMesh->GetSkeleton() ? *PreviewSkeletalMesh->GetSkeleton()->GetPathName() : TEXT("<none>"),
                *ResolvedImportSkeleton->GetPathName()));
        }
    }

    for(USkeletalMesh* Mesh:{SourceSkeletalMesh,PreviewSkeletalMesh}) { if(Mesh&&(!Hold(Mesh)||Mesh->IsCompiling()||!Hold(Mesh->GetSkeleton())||!RefSkeletonSafe(Mesh->GetRefSkeleton()))) { return CreateErrorResponse(CurrentResult->Error.IsEmpty()?TEXT("SkeletalMesh currently compiling or unavailable"):CurrentResult->Error); } }if(ResolvedImportSkeleton&&(!Hold(ResolvedImportSkeleton)||!RefSkeletonSafe(ResolvedImportSkeleton->GetReferenceSkeleton()))) { return CreateErrorResponse(CurrentResult->Error); }
    const FString TargetDirectory = FPackageName::GetLongPackagePath(TargetControlRigPath);
    const FString TargetAssetName = FPackageName::GetShortName(TargetControlRigPath);
    const bool bTargetExists = Exists(TargetControlRigPath);

    UPackage* SkeletonPackage = SourceSkeleton ? SourceSkeleton->GetOutermost() : nullptr;
    UPackage* ResolvedImportSkeletonPackage = ResolvedImportSkeleton ? ResolvedImportSkeleton->GetOutermost() : nullptr;
    UPackage* MeshPackage = SourceSkeletalMesh ? SourceSkeletalMesh->GetOutermost() : nullptr;
    UPackage* PreviewMeshPackage = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetOutermost() : nullptr;
    const bool bSkeletonDirtyBefore = SkeletonPackage && SkeletonPackage->IsDirty();
    const bool bResolvedImportSkeletonDirtyBefore = ResolvedImportSkeletonPackage && ResolvedImportSkeletonPackage->IsDirty();
    const bool bMeshDirtyBefore = MeshPackage && MeshPackage->IsDirty();
    const bool bPreviewMeshDirtyBefore = PreviewMeshPackage && PreviewMeshPackage->IsDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("command"), TEXT("ensure_controlrig_sample_asset"));
    ResultObj->SetBoolField(TEXT("sample_only"), true);
    ResultObj->SetBoolField(TEXT("dry_run"), bDryRun);
    ResultObj->SetBoolField(TEXT("source_modified"), false);
    ResultObj->SetBoolField(TEXT("source_assets_modified"), false);
    ResultObj->SetStringField(TEXT("target_control_rig_path"), TargetControlRigPath);
    ResultObj->SetStringField(TEXT("target_directory"), TargetDirectory);
    ResultObj->SetStringField(TEXT("target_asset_name"), TargetAssetName);
    ResultObj->SetStringField(TEXT("setup_mode"), SetupModeLower);
    ResultObj->SetBoolField(TEXT("target_exists"), bTargetExists);
    ResultObj->SetBoolField(TEXT("allow_existing_target_reuse"), bAllowExistingTargetReuse);
    ResultObj->SetBoolField(TEXT("overwrite_existing"), bOverwriteExisting);
    ResultObj->SetBoolField(TEXT("modular_rig"), bModularRig);
    ResultObj->SetStringField(TEXT("skeleton_path"), SourceSkeleton ? SourceSkeleton->GetPathName() : FString());
    ResultObj->SetStringField(TEXT("skeletal_mesh_path"), SourceSkeletalMesh ? SourceSkeletalMesh->GetPathName() : FString());
    ResultObj->SetStringField(TEXT("preview_skeletal_mesh_path"), PreviewSkeletalMesh ? PreviewSkeletalMesh->GetPathName() : FString());
    ResultObj->SetStringField(TEXT("resolved_import_skeleton_path"), ResolvedImportSkeleton ? ResolvedImportSkeleton->GetPathName() : FString());
    ResultObj->SetBoolField(TEXT("compile_requested"), bCompile);
    ResultObj->SetBoolField(TEXT("save_requested"), bSave);
    ResultObj->SetBoolField(TEXT("skeleton_dirty_before"), bSkeletonDirtyBefore);
    ResultObj->SetBoolField(TEXT("resolved_import_skeleton_dirty_before"), bResolvedImportSkeletonDirtyBefore);
    ResultObj->SetBoolField(TEXT("skeletal_mesh_dirty_before"), bMeshDirtyBefore);
    ResultObj->SetBoolField(TEXT("preview_skeletal_mesh_dirty_before"), bPreviewMeshDirtyBefore);

    TSharedPtr<FJsonObject> HierarchyImportResult = MakeShared<FJsonObject>();
    const FString HierarchyImportSourceType = SourceSkeletalMesh ? FString(TEXT("skeletal_mesh")) : (ResolvedImportSkeleton ? FString(TEXT("skeleton")) : FString());
    HierarchyImportResult->SetBoolField(TEXT("requested"), bImportPreviewHierarchy);
    HierarchyImportResult->SetBoolField(TEXT("performed"), false);
    HierarchyImportResult->SetStringField(TEXT("source_type"), HierarchyImportSourceType);
    HierarchyImportResult->SetStringField(TEXT("source_skeletal_mesh_path"), SourceSkeletalMesh ? SourceSkeletalMesh->GetPathName() : FString());
    HierarchyImportResult->SetStringField(TEXT("source_skeleton_path"), ResolvedImportSkeleton ? ResolvedImportSkeleton->GetPathName() : FString());
    HierarchyImportResult->SetNumberField(TEXT("bone_count"), 0);
    HierarchyImportResult->SetNumberField(TEXT("curve_count"), 0);
    HierarchyImportResult->SetBoolField(TEXT("keys_truncated"), false);
    ResultObj->SetObjectField(TEXT("hierarchy_import"), HierarchyImportResult);

    TSharedPtr<FJsonObject> DirectCurvesResult = MakeShared<FJsonObject>();
    DirectCurvesResult->SetBoolField(TEXT("requested"), bDirectCurveAuthoring);
    DirectCurvesResult->SetBoolField(TEXT("performed"), false);
    DirectCurvesResult->SetNumberField(TEXT("requested_count"), DirectCurveSpecs.Num());
    DirectCurvesResult->SetNumberField(TEXT("max_direct_curves"), MaxDirectCurves);
    DirectCurvesResult->SetNumberField(TEXT("curve_count"), 0);
    DirectCurvesResult->SetArrayField(TEXT("requested_curves"), ControlRigSampleCurveSpecsToJsonValues(DirectCurveSpecs));
    DirectCurvesResult->SetBoolField(TEXT("keys_truncated"), false);
    ResultObj->SetObjectField(TEXT("direct_curves"), DirectCurvesResult);

    TSharedPtr<FJsonObject> DirectNullsResult = MakeShared<FJsonObject>();
    DirectNullsResult->SetBoolField(TEXT("requested"), bDirectNullAuthoring);
    DirectNullsResult->SetBoolField(TEXT("performed"), false);
    DirectNullsResult->SetNumberField(TEXT("requested_count"), DirectNullSpecs.Num());
    DirectNullsResult->SetNumberField(TEXT("max_direct_nulls"), MaxDirectNulls);
    DirectNullsResult->SetNumberField(TEXT("null_count"), 0);
    DirectNullsResult->SetArrayField(TEXT("requested_nulls"), ControlRigSampleNullSpecsToJsonValues(DirectNullSpecs));
    DirectNullsResult->SetBoolField(TEXT("root_level_identity_only"), !bDirectNullParentingRequested && !bDirectNullTransformRequested);
    DirectNullsResult->SetBoolField(TEXT("identity_transform_only"), !bDirectNullTransformRequested);
    DirectNullsResult->SetBoolField(TEXT("parenting_supported"), true);
    DirectNullsResult->SetBoolField(TEXT("transform_supported"), true);
    DirectNullsResult->SetBoolField(TEXT("keys_truncated"), false);
    ResultObj->SetObjectField(TEXT("direct_nulls"), DirectNullsResult);

    TSharedPtr<FJsonObject> DirectControlsResult = MakeShared<FJsonObject>();
    DirectControlsResult->SetBoolField(TEXT("requested"), bDirectControlAuthoring);
    DirectControlsResult->SetBoolField(TEXT("performed"), false);
    DirectControlsResult->SetNumberField(TEXT("requested_count"), DirectControlSpecs.Num());
    DirectControlsResult->SetNumberField(TEXT("max_direct_controls"), MaxDirectControls);
    DirectControlsResult->SetNumberField(TEXT("control_count"), 0);
    DirectControlsResult->SetArrayField(TEXT("requested_controls"), ControlRigSampleControlSpecsToJsonValues(DirectControlSpecs));
    DirectControlsResult->SetBoolField(TEXT("root_level_only"), !bDirectControlParentingRequested);
    DirectControlsResult->SetBoolField(TEXT("scalar_only"), !bDirectTransformControlRequested);
    DirectControlsResult->SetBoolField(TEXT("parenting_supported"), true);
    DirectControlsResult->SetBoolField(TEXT("transform_controls_requested"), bDirectTransformControlRequested);
    DirectControlsResult->SetBoolField(TEXT("transform_controls_supported"), true);
    DirectControlsResult->SetStringField(TEXT("transform_control_scope"), TEXT("direct sample controls with control_type/type transform or euler_transform; transform defaults are accepted only through value/default_value/initial_value/default"));
    DirectControlsResult->SetBoolField(TEXT("shape_customization_requested"), bDirectControlShapeCustomizationRequested);
    DirectControlsResult->SetBoolField(TEXT("shape_customization_supported"), true);
    DirectControlsResult->SetStringField(TEXT("shape_customization_scope"), TEXT("shape_name, shape_color, and shape_transform on direct sample controls; visible shapes are limited by UE control type support"));
    DirectControlsResult->SetBoolField(TEXT("keys_truncated"), false);
    ResultObj->SetObjectField(TEXT("direct_controls"), DirectControlsResult);

    TSharedPtr<FJsonObject> RigVMResult = MakeShared<FJsonObject>();
    RigVMResult->SetBoolField(TEXT("requested"), bRigVMUnitNodeAuthoring);
    RigVMResult->SetBoolField(TEXT("performed"), false);
    RigVMResult->SetNumberField(TEXT("requested_count"), RigVMUnitNodeSpecs.Num());
    RigVMResult->SetNumberField(TEXT("max_unit_nodes"), MaxRigVMUnitNodes);
    RigVMResult->SetNumberField(TEXT("unit_node_count"), 0);
    RigVMResult->SetNumberField(TEXT("links_requested"), 0);
    RigVMResult->SetNumberField(TEXT("links_added"), 0);
    RigVMResult->SetBoolField(TEXT("unit_links_supported"), false);
    RigVMResult->SetBoolField(TEXT("arbitrary_script_structs_supported"), false);
    RigVMResult->SetBoolField(TEXT("graph_replacement_supported"), false);
    RigVMResult->SetStringField(TEXT("scope"), TEXT("sample/temp ControlRig default RigVM model only; allowlisted single-instance ControlRig event unit node evidence"));
    RigVMResult->SetStringField(TEXT("allowed_units"), GetControlRigSampleRigVMAllowedUnitsString());
    RigVMResult->SetStringField(TEXT("ue56_end_execution_note"), TEXT("UE 5.6 public ControlRig headers do not expose FRigUnit_EndExecution; this pass does not synthesize an unavailable unit."));
    RigVMResult->SetArrayField(TEXT("requested_unit_nodes"), ControlRigSampleRigVMUnitSpecsToJsonValues(RigVMUnitNodeSpecs));
    ResultObj->SetObjectField(TEXT("rigvm_summary"), RigVMResult);

    if (bDryRun)
    {
        ResultObj->SetBoolField(TEXT("created"), false);
        ResultObj->SetBoolField(TEXT("target_reused"), false);
        ResultObj->SetBoolField(TEXT("target_modified"), false);
        ResultObj->SetBoolField(TEXT("target_asset_saved"), false);
        ResultObj->SetStringField(TEXT("runtime_note"), TEXT("Dry run only: no ControlRig asset was created, edited, compiled, or saved."));
        return ResultObj;
    }

    UControlRigBlueprint* TargetControlRigBlueprint = nullptr;
    bool bCreated = false;
    bool bReused = false;
    bool bOverwroteExisting = false;
    bool bTargetModified = false;

    auto AddControlRigPartialTargetFields = [&](const TSharedPtr<FJsonObject>& ErrorObj)
    {
        const bool bPartialTargetModified = bTargetModified || bCreated;
        ErrorObj->SetBoolField(TEXT("created"), bCreated);
        ErrorObj->SetBoolField(TEXT("target_reused"), bReused);
        ErrorObj->SetBoolField(TEXT("overwrote_existing"), bOverwroteExisting);
        ErrorObj->SetStringField(TEXT("target_control_rig_path"), TargetControlRigPath);
        SetSamplePartialTargetMutationStatus(ErrorObj, TargetControlRigBlueprint, TargetControlRigPath, bPartialTargetModified);
    };

    auto MakeControlRigPartialError = [&](const FString& ErrorText) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(ErrorText);
        AddControlRigPartialTargetFields(ErrorObj);
        return ErrorObj;
    };

    if ((bImportPreviewHierarchy || bDirectCurveAuthoring || bDirectNullAuthoring || bDirectControlAuthoring || bRigVMUnitNodeAuthoring) && bTargetExists && !bOverwriteExisting)
    {
        TArray<FString> FeatureParts;
        if (bImportPreviewHierarchy)
        {
            FeatureParts.Add(TEXT("setup_mode=import_preview_hierarchy"));
        }
        if (bDirectCurveAuthoring)
        {
            FeatureParts.Add(TEXT("direct curves"));
        }
        if (bDirectNullAuthoring)
        {
            FeatureParts.Add(TEXT("direct nulls"));
        }
        if (bDirectControlAuthoring)
        {
            FeatureParts.Add(TEXT("direct controls"));
        }
        if (bRigVMUnitNodeAuthoring)
        {
            FeatureParts.Add(TEXT("RigVM unit nodes"));
        }
        const FString FeatureName = FeatureParts.Num() > 0 ? FString::Join(FeatureParts, TEXT(" with ")) : FString(TEXT("requested ControlRig setup"));
        TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
            TEXT("%s requires overwrite_existing=true when the target already exists: %s"),
            *FeatureName,
            *TargetControlRigPath));
        ErrorObj->SetBoolField(TEXT("target_exists"), true);
        ErrorObj->SetBoolField(TEXT("requires_overwrite_existing"), true);
        ErrorObj->SetStringField(TEXT("feature_requires_overwrite"), FeatureName);
        ErrorObj->SetStringField(TEXT("target_control_rig_path"), TargetControlRigPath);
        ErrorObj->SetBoolField(TEXT("source_modified"), false);
        return ErrorObj;
    }

    for(UObject* Source:{static_cast<UObject*>(SourceSkeleton),static_cast<UObject*>(ResolvedImportSkeleton),static_cast<UObject*>(SourceSkeletalMesh),static_cast<UObject*>(PreviewSkeletalMesh)}) { if(Source&&(!Hold(Source)||!Hold(Source->GetOutermost())||Source->GetOutermost()->GetName().Equals(TargetControlRigPath,ESearchCase::IgnoreCase))) { return CreateErrorResponse(TEXT("Target/source package identity collision refused before target deletion or creation")); } }
    if (bTargetExists)
    {
        if (bOverwriteExisting)
        {
            if (!Delete(TargetControlRigPath))
            {
                return CreateErrorResponse(FString::Printf(TEXT("Failed to delete existing ControlRig sample: %s"), *TargetControlRigPath));
            }
            bOverwroteExisting = true;
        }
        else if (bAllowExistingTargetReuse)
        {
            TargetControlRigBlueprint = Cast<UControlRigBlueprint>(AssetLoad(TargetControlRigPath));
            if (!TargetControlRigBlueprint)
            {
                return CreateErrorResponse(FString::Printf(TEXT("Existing target is not a ControlRig Blueprint: %s"), *TargetControlRigPath));
            }
            bReused = true;
        }
        else
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(TEXT("Target ControlRig sample already exists: %s"), *TargetControlRigPath));
            ErrorObj->SetBoolField(TEXT("target_exists"), true);
            ErrorObj->SetStringField(TEXT("target_control_rig_path"), TargetControlRigPath);
            ErrorObj->SetBoolField(TEXT("source_modified"), false);
            return ErrorObj;
        }
    }

    if (!TargetControlRigBlueprint)
    {
        if (!TargetDirectory.IsEmpty())
        {
            MakeDir(TargetDirectory);
        }
        TargetControlRigBlueprint = Factory(TargetControlRigPath, bModularRig);
        if (!TargetControlRigBlueprint)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Failed to create ControlRig sample: %s"), *TargetControlRigPath));
        }
        bCreated = true;
        bTargetModified = true;
    }

    if(!RefreshTarget(TargetControlRigBlueprint)) { return CreateErrorResponse(CurrentResult->Error); }TargetBefore=Capture(TargetControlRigBlueprint);
    if (SetupModeLower == TEXT("empty_with_preview") && PreviewSkeletalMesh)
    {
        USkeletalMesh* ExistingPreviewMesh = Preview(TargetControlRigBlueprint);
        if (ExistingPreviewMesh != PreviewSkeletalMesh)
        {
            SetPreview(TargetControlRigBlueprint, PreviewSkeletalMesh, true);
            bTargetModified = true;
        }
    }

    if (bImportPreviewHierarchy)
    {
        const FReferenceSkeleton* ImportReferenceSkeleton = nullptr;
        if (SourceSkeletalMesh)
        {
            ImportReferenceSkeleton = &SourceSkeletalMesh->GetRefSkeleton();
        }
        else if (ResolvedImportSkeleton)
        {
            ImportReferenceSkeleton = &ResolvedImportSkeleton->GetReferenceSkeleton();
        }
        if (!ImportReferenceSkeleton)
        {
            return MakeControlRigPartialError(TEXT("Failed to resolve a reference skeleton for ControlRig hierarchy import"));
        }

        URigHierarchyController* HierarchyController = Controller(TargetControlRigBlueprint);
        if (!HierarchyController)
        {
            return MakeControlRigPartialError(FString::Printf(
                TEXT("ControlRig Blueprint has no hierarchy controller: %s"),
                *TargetControlRigPath));
        }

        if(SourceSkeletalMesh) { if(!Hold(SourceSkeletalMesh)||SourceSkeletalMesh->IsCompiling()||!Hold(SourceSkeletalMesh->GetSkeleton())) { return MakeControlRigPartialError(TEXT("Current import mesh became unavailable/compiling after controller callback")); }ImportReferenceSkeleton=&SourceSkeletalMesh->GetRefSkeleton(); }else if(ResolvedImportSkeleton) { if(!Hold(ResolvedImportSkeleton)) { return MakeControlRigPartialError(TEXT("Current import skeleton unavailable")); }ImportReferenceSkeleton=&ResolvedImportSkeleton->GetReferenceSkeleton(); }if(!ImportReferenceSkeleton||!RefSkeletonSafe(*ImportReferenceSkeleton)) { return MakeControlRigPartialError(CurrentResult->Error); }
        const TArray<FRigElementKey> ImportedBoneKeys = CheckedImportBones(HierarchyController,*ImportReferenceSkeleton, NAME_None, false, false, false, false);
        TArray<FRigElementKey> ImportedCurveKeys;
        if (SourceSkeletalMesh)
        {
            ImportedCurveKeys = CheckedImportCurvesFromSkeletalMesh(HierarchyController,SourceSkeletalMesh, NAME_None, false, false);
            SetPreview(TargetControlRigBlueprint, SourceSkeletalMesh, true);
        }
        else if (ResolvedImportSkeleton)
        {
            ImportedCurveKeys = CheckedImportCurves(HierarchyController,ResolvedImportSkeleton, NAME_None, false, false);
            if (PreviewSkeletalMesh)
            {
                SetPreview(TargetControlRigBlueprint, PreviewSkeletalMesh, true);
            }
        }

        Propagate(TargetControlRigBlueprint);
        bTargetModified = true;

        bool bBoneKeysTruncated = false;
        bool bCurveKeysTruncated = false;
        HierarchyImportResult->SetBoolField(TEXT("performed"), true);
        HierarchyImportResult->SetBoolField(TEXT("source_import_metadata_written"), false);
        HierarchyImportResult->SetStringField(TEXT("source_import_metadata_note"), TEXT("UE 5.6 SourceHierarchyImport and SourceCurveImport are private; command uses public hierarchy import and preview mesh APIs only."));
        HierarchyImportResult->SetNumberField(TEXT("bone_count"), ImportedBoneKeys.Num());
        HierarchyImportResult->SetNumberField(TEXT("curve_count"), ImportedCurveKeys.Num());
        HierarchyImportResult->SetArrayField(TEXT("bone_keys"), RigElementKeysToJsonValues(ImportedBoneKeys, 64, bBoneKeysTruncated));
        HierarchyImportResult->SetArrayField(TEXT("curve_keys"), RigElementKeysToJsonValues(ImportedCurveKeys, 64, bCurveKeysTruncated));
        HierarchyImportResult->SetBoolField(TEXT("keys_truncated"), bBoneKeysTruncated || bCurveKeysTruncated);
    }

    if (bDirectCurveAuthoring)
    {
        URigHierarchyController* HierarchyController = Controller(TargetControlRigBlueprint);
        if (!HierarchyController)
        {
            return MakeControlRigPartialError(FString::Printf(
                TEXT("ControlRig Blueprint has no hierarchy controller: %s"),
                *TargetControlRigPath));
        }

        TArray<FRigElementKey> AddedCurveKeys;
        TArray<TSharedPtr<FJsonValue>> AddedCurveValues;
        for (int32 Index = 0; Index < DirectCurveSpecs.Num(); ++Index)
        {
            const FControlRigSampleCurveSpec& Spec = DirectCurveSpecs[Index];
            const FRigElementKey CurveKey = CheckedAddCurve(HierarchyController,FName(*Spec.Name), Spec.Value, false, false);
            if (!CurveKey.IsValid())
            {
                TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                    TEXT("Failed to add ControlRig sample curve: %s"),
                    *Spec.Name));
                ErrorObj->SetObjectField(TEXT("direct_curves"), DirectCurvesResult);
                ErrorObj->SetBoolField(TEXT("source_modified"), false);
                AddControlRigPartialTargetFields(ErrorObj);
                return ErrorObj;
            }

            AddedCurveKeys.Add(CurveKey);
            bTargetModified = true;
            TSharedPtr<FJsonObject> CurveObject = RigElementKeyToJson(CurveKey);
            CurveObject->SetNumberField(TEXT("requested_index"), Index);
            CurveObject->SetStringField(TEXT("requested_name"), Spec.Name);
            CurveObject->SetNumberField(TEXT("requested_value"), Spec.Value);
            AddedCurveValues.Add(MakeShared<FJsonValueObject>(CurveObject));
        }

        Propagate(TargetControlRigBlueprint);
        bTargetModified = true;

        bool bAddedCurveKeysTruncated = false;
        DirectCurvesResult->SetBoolField(TEXT("performed"), true);
        DirectCurvesResult->SetNumberField(TEXT("curve_count"), AddedCurveKeys.Num());
        DirectCurvesResult->SetArrayField(TEXT("curve_keys"), RigElementKeysToJsonValues(AddedCurveKeys, 64, bAddedCurveKeysTruncated));
        DirectCurvesResult->SetArrayField(TEXT("curves"), AddedCurveValues);
        DirectCurvesResult->SetBoolField(TEXT("keys_truncated"), bAddedCurveKeysTruncated);
    }

    TArray<FRigElementKey> AddedControlRigSampleParentKeys;

    if (bDirectNullAuthoring)
    {
        URigHierarchyController* HierarchyController = Controller(TargetControlRigBlueprint);
        if (!HierarchyController)
        {
            return MakeControlRigPartialError(FString::Printf(
                TEXT("ControlRig Blueprint has no hierarchy controller: %s"),
                *TargetControlRigPath));
        }
        URigHierarchy* Hierarchy = HierarchyController->GetHierarchy();
        if (!Hierarchy)
        {
            return MakeControlRigPartialError(FString::Printf(
                TEXT("ControlRig Blueprint has no hierarchy for direct null parent resolution: %s"),
                *TargetControlRigPath));
        }

        TArray<FRigElementKey> ResolvedNullParentKeys;
        ResolvedNullParentKeys.SetNum(DirectNullSpecs.Num());
        TArray<FRigElementKey> NullParentProbeKeys = AddedControlRigSampleParentKeys;
        for (int32 Index = 0; Index < DirectNullSpecs.Num(); ++Index)
        {
            const FControlRigSampleNullSpec& Spec = DirectNullSpecs[Index];
            if (Spec.bHasParent)
            {
                FString ParentResolveError;
                if (!TryResolveControlRigSampleParentKey(
                    Hierarchy,
                    NullParentProbeKeys,
                    Spec.ParentName,
                    ResolvedNullParentKeys[Index],
                    ParentResolveError,
                    FString::Printf(TEXT("nulls[%d].parent"), Index)))
                {
                    TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(ParentResolveError);
                    ErrorObj->SetObjectField(TEXT("direct_nulls"), DirectNullsResult);
                    ErrorObj->SetBoolField(TEXT("source_modified"), false);
                    AddControlRigPartialTargetFields(ErrorObj);
                    return ErrorObj;
                }
            }
            NullParentProbeKeys.Add(FRigElementKey(FName(*Spec.Name), ERigElementType::Null));
        }

        TArray<FRigElementKey> AddedNullKeys;
        TArray<TSharedPtr<FJsonValue>> AddedNullValues;
        for (int32 Index = 0; Index < DirectNullSpecs.Num(); ++Index)
        {
            const FControlRigSampleNullSpec& Spec = DirectNullSpecs[Index];
            const FRigElementKey ParentKey = Spec.bHasParent ? ResolvedNullParentKeys[Index] : FRigElementKey();
            const FRigElementKey NullKey = CheckedAddNull(HierarchyController,
                FName(*Spec.Name),
                ParentKey,
                Spec.Transform,
                Spec.bTransformInGlobal,
                false,
                false);
            if (!NullKey.IsValid())
            {
                TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                    TEXT("Failed to add ControlRig sample null: %s"),
                    *Spec.Name));
                ErrorObj->SetObjectField(TEXT("direct_nulls"), DirectNullsResult);
                ErrorObj->SetBoolField(TEXT("source_modified"), false);
                AddControlRigPartialTargetFields(ErrorObj);
                return ErrorObj;
            }

            if(!HierarchySafe(Hierarchy)||HierarchyController->GetHierarchy()!=Hierarchy||!Hierarchy->Find(NullKey)) { return MakeControlRigPartialError(TEXT("Current returned null key unavailable after SDK callback")); }
            AddedNullKeys.Add(NullKey);
            AddedControlRigSampleParentKeys.Add(NullKey);
            bTargetModified = true;
            TSharedPtr<FJsonObject> NullObject = RigElementKeyToJson(NullKey);
            NullObject->SetNumberField(TEXT("requested_index"), Index);
            NullObject->SetStringField(TEXT("requested_name"), Spec.Name);
            NullObject->SetBoolField(TEXT("root_level_identity"), !Spec.bHasParent && !Spec.bHasTransform);
            NullObject->SetBoolField(TEXT("identity_transform"), !Spec.bHasTransform);
            NullObject->SetStringField(TEXT("transform_space"), Spec.bTransformInGlobal ? TEXT("global") : TEXT("local"));
            if (Spec.bHasTransform)
            {
                NullObject->SetStringField(TEXT("transform_field"), Spec.TransformFieldName);
                NullObject->SetObjectField(TEXT("requested_transform"), TransformToJsonObject(Spec.Transform));
            }
            NullObject->SetObjectField(TEXT("local_transform"), TransformToJsonObject(Hierarchy->GetLocalTransform(NullKey)));
            NullObject->SetObjectField(TEXT("global_transform"), TransformToJsonObject(Hierarchy->GetGlobalTransform(NullKey)));
            if (Spec.bHasParent)
            {
                NullObject->SetStringField(TEXT("parent_name"), Spec.ParentName);
                NullObject->SetObjectField(TEXT("parent_key"), RigElementKeyToJson(ParentKey));
            }
            AddedNullValues.Add(MakeShared<FJsonValueObject>(NullObject));
        }

        Propagate(TargetControlRigBlueprint);
        bTargetModified = true;

        bool bAddedNullKeysTruncated = false;
        DirectNullsResult->SetBoolField(TEXT("performed"), true);
        DirectNullsResult->SetNumberField(TEXT("null_count"), AddedNullKeys.Num());
        DirectNullsResult->SetArrayField(TEXT("null_keys"), RigElementKeysToJsonValues(AddedNullKeys, 64, bAddedNullKeysTruncated));
        DirectNullsResult->SetArrayField(TEXT("nulls"), AddedNullValues);
        DirectNullsResult->SetBoolField(TEXT("keys_truncated"), bAddedNullKeysTruncated);
    }

    if (bDirectControlAuthoring)
    {
        URigHierarchyController* HierarchyController = Controller(TargetControlRigBlueprint);
        if (!HierarchyController)
        {
            return MakeControlRigPartialError(FString::Printf(
                TEXT("ControlRig Blueprint has no hierarchy controller: %s"),
                *TargetControlRigPath));
        }
        URigHierarchy* Hierarchy = HierarchyController->GetHierarchy();
        if (!Hierarchy)
        {
            return MakeControlRigPartialError(FString::Printf(
                TEXT("ControlRig Blueprint has no hierarchy for direct control parent resolution: %s"),
                *TargetControlRigPath));
        }

        TArray<FRigElementKey> ResolvedControlParentKeys;
        ResolvedControlParentKeys.SetNum(DirectControlSpecs.Num());
        TArray<FRigElementKey> ControlParentProbeKeys = AddedControlRigSampleParentKeys;
        for (int32 Index = 0; Index < DirectControlSpecs.Num(); ++Index)
        {
            const FControlRigSampleControlSpec& Spec = DirectControlSpecs[Index];
            if (Spec.bHasParent)
            {
                FString ParentResolveError;
                if (!TryResolveControlRigSampleParentKey(
                    Hierarchy,
                    ControlParentProbeKeys,
                    Spec.ParentName,
                    ResolvedControlParentKeys[Index],
                    ParentResolveError,
                    FString::Printf(TEXT("controls[%d].parent"), Index)))
                {
                    TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(ParentResolveError);
                    ErrorObj->SetObjectField(TEXT("direct_controls"), DirectControlsResult);
                    ErrorObj->SetBoolField(TEXT("source_modified"), false);
                    AddControlRigPartialTargetFields(ErrorObj);
                    return ErrorObj;
                }
            }
            ControlParentProbeKeys.Add(FRigElementKey(FName(*Spec.Name), ERigElementType::Control));
        }

        TArray<FRigElementKey> AddedControlKeys;
        TArray<TSharedPtr<FJsonValue>> AddedControlValues;
        for (int32 Index = 0; Index < DirectControlSpecs.Num(); ++Index)
        {
            const FControlRigSampleControlSpec& Spec = DirectControlSpecs[Index];

            FRigControlSettings Settings;
            Settings.AnimationType = ERigControlAnimationType::AnimationControl;
            Settings.ControlType = Spec.ControlType;
            Settings.DisplayName = FName(*Spec.Name);
            Settings.bIsCurve = false;
            Settings.bIsTransientControl = false;
            Settings.bShapeVisible = Spec.ControlType != ERigControlType::Bool;
            if (Spec.bHasShapeName)
            {
                Settings.ShapeName = FName(*Spec.ShapeName);
            }
            if (Spec.bHasShapeColor)
            {
                Settings.ShapeColor = Spec.ShapeColor;
            }
            Settings.SetupLimitArrayForType();

            FRigControlValue ControlValue;
            if (Spec.ControlType == ERigControlType::Bool)
            {
                ControlValue = FRigControlValue::Make<bool>(Spec.BoolValue);
            }
            else if (ControlRigSampleControlIsTransformControl(Spec.ControlType))
            {
                ControlValue = FRigControlValue::Make<FEulerTransform>(FEulerTransform(Spec.TransformValue));
            }
            else
            {
                ControlValue = FRigControlValue::Make<float>(Spec.FloatValue);
            }
            const FRigElementKey ParentKey = Spec.bHasParent ? ResolvedControlParentKeys[Index] : FRigElementKey();
            const FTransform ShapeTransform = Spec.bHasShapeTransform ? Spec.ShapeTransform : FTransform::Identity;
            const FRigElementKey ControlKey = CheckedAddControl(HierarchyController,
                FName(*Spec.Name),
                ParentKey,
                Settings,
                ControlValue,
                FTransform::Identity,
                ShapeTransform,
                false,
                false);
            if (!ControlKey.IsValid())
            {
                TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                    TEXT("Failed to add ControlRig sample control: %s"),
                    *Spec.Name));
                ErrorObj->SetObjectField(TEXT("direct_controls"), DirectControlsResult);
                ErrorObj->SetBoolField(TEXT("source_modified"), false);
                AddControlRigPartialTargetFields(ErrorObj);
                return ErrorObj;
            }

            if(!HierarchySafe(Hierarchy)||HierarchyController->GetHierarchy()!=Hierarchy||!Hierarchy->Find(ControlKey)) { return MakeControlRigPartialError(TEXT("Current returned control key unavailable after SDK callback")); }
            AddedControlKeys.Add(ControlKey);
            AddedControlRigSampleParentKeys.Add(ControlKey);
            bTargetModified = true;
            TSharedPtr<FJsonObject> ControlObject = RigElementKeyToJson(ControlKey);
            ControlObject->SetNumberField(TEXT("requested_index"), Index);
            ControlObject->SetStringField(TEXT("requested_name"), Spec.Name);
            ControlObject->SetStringField(TEXT("control_type"), ControlRigSampleControlTypeToString(Spec.ControlType));
            ControlObject->SetBoolField(TEXT("root_level"), !Spec.bHasParent);
            ControlObject->SetBoolField(TEXT("transform_control"), ControlRigSampleControlIsTransformControl(Spec.ControlType));
            ControlObject->SetBoolField(TEXT("shape_customized"), ControlRigSampleControlHasShapeCustomization(Spec));
            ControlObject->SetBoolField(TEXT("shape_visible_supported_by_type"), Spec.ControlType != ERigControlType::Bool);
            if (ControlRigSampleControlIsTransformControl(Spec.ControlType))
            {
                ControlObject->SetObjectField(TEXT("requested_transform_value"), TransformToJsonObject(Spec.TransformValue));
                ControlObject->SetBoolField(TEXT("transform_value_specified"), Spec.bHasTransformValue);
            }
            if (Spec.bHasShapeName)
            {
                ControlObject->SetStringField(TEXT("requested_shape_name"), Spec.ShapeName);
            }
            if (Spec.bHasShapeColor)
            {
                ControlObject->SetObjectField(TEXT("requested_shape_color"), LinearColorToJsonObject(Spec.ShapeColor));
            }
            if (Spec.bHasShapeTransform)
            {
                ControlObject->SetObjectField(TEXT("requested_shape_transform"), TransformToJsonObject(Spec.ShapeTransform));
            }
            if (const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(ControlKey))
            {
                ControlObject->SetStringField(TEXT("shape_name"), ControlElement->Settings.ShapeName.ToString());
                ControlObject->SetObjectField(TEXT("shape_color"), LinearColorToJsonObject(ControlElement->Settings.ShapeColor));
                ControlObject->SetBoolField(TEXT("shape_visible"), ControlElement->Settings.IsVisible());
                ControlObject->SetObjectField(TEXT("shape_transform"), TransformToJsonObject(Hierarchy->GetLocalControlShapeTransform(ControlKey)));
                if (ControlRigSampleControlIsTransformControl(Spec.ControlType))
                {
                    const FRigControlValue CurrentValue = Hierarchy->GetControlValue(ControlKey, ERigControlValueType::Current);
                    const FRigControlValue InitialValue = Hierarchy->GetControlValue(ControlKey, ERigControlValueType::Initial);
                    ControlObject->SetObjectField(
                        TEXT("readback_current_value"),
                        TransformToJsonObject(CurrentValue.GetAsTransform(ControlElement->Settings.ControlType, ControlElement->Settings.PrimaryAxis)));
                    ControlObject->SetObjectField(
                        TEXT("readback_initial_value"),
                        TransformToJsonObject(InitialValue.GetAsTransform(ControlElement->Settings.ControlType, ControlElement->Settings.PrimaryAxis)));
                    ControlObject->SetObjectField(TEXT("local_transform"), TransformToJsonObject(Hierarchy->GetLocalTransform(ControlKey)));
                    ControlObject->SetObjectField(TEXT("global_transform"), TransformToJsonObject(Hierarchy->GetGlobalTransform(ControlKey)));
                }
            }
            if (Spec.bHasParent)
            {
                ControlObject->SetStringField(TEXT("parent_name"), Spec.ParentName);
                ControlObject->SetObjectField(TEXT("parent_key"), RigElementKeyToJson(ParentKey));
            }
            if (Spec.ControlType == ERigControlType::Bool)
            {
                ControlObject->SetBoolField(TEXT("default_value"), Spec.BoolValue);
            }
            else if (ControlRigSampleControlIsTransformControl(Spec.ControlType))
            {
                ControlObject->SetObjectField(TEXT("default_value"), TransformToJsonObject(Spec.TransformValue));
            }
            else
            {
                ControlObject->SetNumberField(TEXT("default_value"), Spec.FloatValue);
            }
            AddedControlValues.Add(MakeShared<FJsonValueObject>(ControlObject));
        }

        Propagate(TargetControlRigBlueprint);
        bTargetModified = true;

        bool bAddedControlKeysTruncated = false;
        DirectControlsResult->SetBoolField(TEXT("performed"), true);
        DirectControlsResult->SetNumberField(TEXT("control_count"), AddedControlKeys.Num());
        DirectControlsResult->SetArrayField(TEXT("control_keys"), RigElementKeysToJsonValues(AddedControlKeys, 64, bAddedControlKeysTruncated));
        DirectControlsResult->SetArrayField(TEXT("controls"), AddedControlValues);
        DirectControlsResult->SetBoolField(TEXT("keys_truncated"), bAddedControlKeysTruncated);
    }

    if (bRigVMUnitNodeAuthoring)
    {
        URigVMGraph* RigVMGraph = Model(TargetControlRigBlueprint);
        if (!RigVMGraph)
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("ControlRig Blueprint has no default RigVM model: %s"),
                *TargetControlRigPath));
            ErrorObj->SetObjectField(TEXT("rigvm_summary"), RigVMResult);
            ErrorObj->SetBoolField(TEXT("source_modified"), false);
            AddControlRigPartialTargetFields(ErrorObj);
            return ErrorObj;
        }

        URigVMController* RigVMController = VMController(TargetControlRigBlueprint,RigVMGraph);
        if (!RigVMController)
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("ControlRig Blueprint has no RigVM controller for default model: %s"),
                *TargetControlRigPath));
            ErrorObj->SetObjectField(TEXT("rigvm_summary"), RigVMResult);
            ErrorObj->SetBoolField(TEXT("source_modified"), false);
            AddControlRigPartialTargetFields(ErrorObj);
            return ErrorObj;
        }

        TArray<TSharedPtr<FJsonValue>> AddedUnitNodeValues;
        int32 AddedUnitNodeCount = 0;
        int32 ReusedUnitNodeCount = 0;
        bool bAddedAnyRigVMNode = false;
        for (int32 Index = 0; Index < RigVMUnitNodeSpecs.Num(); ++Index)
        {
            const FControlRigSampleRigVMUnitNodeSpec& Spec = RigVMUnitNodeSpecs[Index];
            UScriptStruct* ScriptStruct = GetControlRigSampleRigVMUnitScriptStruct(Spec.UnitType);
            if (!ScriptStruct)
            {
                TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                    TEXT("No script struct mapped for ControlRig sample RigVM unit type: %s"),
                    *Spec.UnitType));
                ErrorObj->SetObjectField(TEXT("rigvm_summary"), RigVMResult);
                ErrorObj->SetBoolField(TEXT("source_modified"), false);
                AddControlRigPartialTargetFields(ErrorObj);
                return ErrorObj;
            }

            bool bNodeAdded = false;
            bool bNodeReused = false;
            URigVMUnitNode* UnitNode = FindControlRigSampleRigVMUnitNode(RigVMGraph, ScriptStruct);
            if (UnitNode)
            {
                bNodeReused = true;
                ++ReusedUnitNodeCount;
            }
            else
            {
                UnitNode = AddUnit(RigVMController,ScriptStruct, FName(TEXT("Execute")), Spec.Position, Spec.NodeName, false, false);
                if (!UnitNode)
                {
                    TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                        TEXT("Failed to add allowlisted ControlRig sample RigVM unit node: %s"),
                        *Spec.UnitType));
                    ErrorObj->SetObjectField(TEXT("rigvm_summary"), RigVMResult);
                    ErrorObj->SetBoolField(TEXT("source_modified"), false);
                    AddControlRigPartialTargetFields(ErrorObj);
                    return ErrorObj;
                }
                bNodeAdded = true;
                bAddedAnyRigVMNode = true;
                bTargetModified = true;
                ++AddedUnitNodeCount;
            }

            if(!ModelSafe(RigVMGraph)||!Hold(UnitNode)||!RigVMGraph->GetNodes().Contains(UnitNode)) { return MakeControlRigPartialError(TEXT("Current unit identity unavailable after controller callback")); }
            AddedUnitNodeValues.Add(MakeShared<FJsonValueObject>(ControlRigSampleRigVMUnitNodeToJsonObject(UnitNode, Spec, Index, bNodeAdded, bNodeReused)));
        }

        if (bAddedAnyRigVMNode)
        {
            AutoRecompile(TargetControlRigBlueprint);
            bTargetModified = true;
        }

        RigVMResult->SetBoolField(TEXT("performed"), true);
        RigVMResult->SetStringField(TEXT("graph_name"), RigVMGraph->GetGraphName());
        RigVMResult->SetStringField(TEXT("graph_node_path"), RigVMGraph->GetNodePath());
        RigVMResult->SetNumberField(TEXT("unit_node_count"), RigVMUnitNodeSpecs.Num());
        RigVMResult->SetNumberField(TEXT("nodes_added"), AddedUnitNodeCount);
        RigVMResult->SetNumberField(TEXT("nodes_reused"), ReusedUnitNodeCount);
        RigVMResult->SetArrayField(TEXT("unit_nodes"), AddedUnitNodeValues);
    }

    TSharedPtr<FJsonObject> CompileResult = MakeShared<FJsonObject>();
    CompileResult->SetBoolField(TEXT("requested"), bCompile);
    if (bCompile)
    {
        RequestInit(TargetControlRigBlueprint);
        CompileResult = AnimNodeProbeCompileBlueprint(TargetControlRigBlueprint);
    }

    bool bCanCreateTransientControlRig = false;
    if (UClass* GeneratedControlRigClass = RigBlueprintClass(TargetControlRigBlueprint))
    {
        UControlRig* TransientControlRig = CreateRig(TargetControlRigBlueprint);
        bCanCreateTransientControlRig = TransientControlRig != nullptr && TransientControlRig->GetClass()->IsChildOf(UControlRig::StaticClass());
    }

    const FString ActualTargetPath = TargetControlRigBlueprint->GetOutermost()->GetName();
    bool bTargetSaved = false;
    if (bSave)
    {
        bTargetSaved = Save(ActualTargetPath, false);
    }

    USkeletalMesh* PreviewMeshReadback = Preview(TargetControlRigBlueprint);
    UClass* ControlRigClass = RigBlueprintClass(TargetControlRigBlueprint);

    ResultObj->SetBoolField(TEXT("created"), bCreated);
    ResultObj->SetBoolField(TEXT("target_reused"), bReused);
    ResultObj->SetBoolField(TEXT("overwrote_existing"), bOverwroteExisting);
    ResultObj->SetBoolField(TEXT("target_modified"), bTargetModified);
    ResultObj->SetBoolField(TEXT("target_asset_saved"), bTargetSaved);
    ResultObj->SetStringField(TEXT("created_asset_path"), TargetControlRigBlueprint->GetPathName());
    ResultObj->SetStringField(TEXT("actual_target_control_rig_path"), ActualTargetPath);
    ResultObj->SetStringField(TEXT("control_rig_class_path"), ControlRigClass ? ControlRigClass->GetPathName() : FString());
    ResultObj->SetStringField(TEXT("preview_skeletal_mesh_readback"), PreviewMeshReadback ? PreviewMeshReadback->GetPathName() : FString());
    ResultObj->SetBoolField(TEXT("can_create_transient_control_rig"), bCanCreateTransientControlRig);
    ResultObj->SetObjectField(TEXT("compile_result"), CompileResult);
    ResultObj->SetBoolField(TEXT("target_dirty_after"), TargetControlRigBlueprint->GetOutermost()->IsDirty());
    ResultObj->SetBoolField(TEXT("skeleton_dirty_after"), SkeletonPackage && SkeletonPackage->IsDirty());
    ResultObj->SetBoolField(TEXT("resolved_import_skeleton_dirty_after"), ResolvedImportSkeletonPackage && ResolvedImportSkeletonPackage->IsDirty());
    ResultObj->SetBoolField(TEXT("skeletal_mesh_dirty_after"), MeshPackage && MeshPackage->IsDirty());
    ResultObj->SetBoolField(TEXT("preview_skeletal_mesh_dirty_after"), PreviewMeshPackage && PreviewMeshPackage->IsDirty());
    ApplySampleCompileSaveOutcome(ResultObj, CompileResult, bCompile, bSave, bTargetSaved);
    return ResultObj;
}

bool CurrentRigElements(const TSharedPtr<FJsonObject>& Legacy,UControlRigBlueprint* BP)
{
    if(!BP||!Hold(BP)||!HierarchySafe(BP->Hierarchy)) { return false; }URigHierarchy* H=BP->Hierarchy.Get();bool All=true;TArray<TSharedPtr<FJsonValue>> Rows;
    for(const TCHAR* Group:{TEXT("direct_curves"),TEXT("direct_nulls"),TEXT("direct_controls")})
    {
        const TSharedPtr<FJsonObject>* Object=nullptr;if(!Legacy->TryGetObjectField(Group,Object)||!Object) { continue; }const TCHAR* Field=FCString::Strcmp(Group,TEXT("direct_curves"))==0?TEXT("curves"):FCString::Strcmp(Group,TEXT("direct_nulls"))==0?TEXT("nulls"):TEXT("controls");const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(!(*Object)->TryGetArrayField(Field,Values)||!Values) { continue; }
        for(const auto& V:*Values)
        {
            const auto Original=V->AsObject();const FString Name=Original->GetStringField(TEXT("name"));if(!RequiredName(Name)) { return false; }const auto Type=FCString::Strcmp(Group,TEXT("direct_curves"))==0?ERigElementType::Curve:FCString::Strcmp(Group,TEXT("direct_nulls"))==0?ERigElementType::Null:ERigElementType::Control;const FRigElementKey Key(FName(*Name),Type);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("key"),Key.ToString());const bool ExistsNow=H->Find(Key)!=nullptr;Row->SetBoolField(TEXT("current_key_present"),ExistsNow);bool Matches=ExistsNow;
            if(ExistsNow&&Type==ERigElementType::Curve) { const float Value=H->GetCurveValue(Key);Row->SetNumberField(TEXT("current_value"),Value);Matches=FMath::IsFinite(Value)&&FMath::IsNearlyEqual(Value,static_cast<float>(Original->GetNumberField(TEXT("requested_value")))); }
            if(ExistsNow&&Type==ERigElementType::Null) { const auto Local=H->GetLocalTransform(Key),Global=H->GetGlobalTransform(Key);Row->SetObjectField(TEXT("current_local"),TransformToJsonObject(Local));Row->SetObjectField(TEXT("current_global"),TransformToJsonObject(Global));Matches=!Local.ContainsNaN()&&!Global.ContainsNaN(); }
            if(ExistsNow&&Type==ERigElementType::Control)
            { const auto* E=H->Find<FRigControlElement>(Key);Matches=E!=nullptr;if(E) { const auto ControlType=E->Settings.ControlType;Row->SetStringField(TEXT("current_control_type"),ControlRigSampleControlTypeToString(ControlType));Matches&=ControlRigSampleControlTypeToString(ControlType)==Original->GetStringField(TEXT("control_type"));const auto Value=H->GetControlValue(Key);if(ControlType==ERigControlType::Bool) { const bool B=Value.Get<bool>();Row->SetBoolField(TEXT("current_value"),B);Matches&=B==Original->GetBoolField(TEXT("default_value")); }else if(ControlType==ERigControlType::Float) { const float F=Value.Get<float>();Row->SetNumberField(TEXT("current_value"),F);Matches&=FMath::IsFinite(F)&&FMath::IsNearlyEqual(F,static_cast<float>(Original->GetNumberField(TEXT("default_value")))); }else { const auto T=Value.GetAsTransform(ControlType,E->Settings.PrimaryAxis);Row->SetObjectField(TEXT("current_value"),TransformToJsonObject(T));Matches&=!T.ContainsNaN(); } } }
            Row->SetBoolField(TEXT("current_typed_value_verified"),Matches);Rows.Add(MakeShared<FJsonValueObject>(Row));All&=Matches;
        }
    }
    CurrentResult->Data->SetArrayField(TEXT("current_authored_element_receipts"),Rows);CurrentResult->Data->SetBoolField(TEXT("current_authored_elements_verified"),All);return All&&!Incomplete();
}


void Init(FResult& R,const FString& Op,bool Compile,bool SaveAsset)
{ R.Data->SetStringField(TEXT("operation"),Op);R.bCompileRequested=Compile;R.bSaveRequested=SaveAsset;R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("automatic_retry"),false);R.Data->SetBoolField(TEXT("runtime_ControlRig_execution_verified"),false);R.Data->SetBoolField(TEXT("implicit_factory_VM_template_compile_initialization_qualified"),true);auto C=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("VM_controller_getter_calls"),TEXT("add_control_calls"),TEXT("add_control_returns"),TEXT("add_curve_calls"),TEXT("add_curve_returns"),TEXT("add_null_calls"),TEXT("add_null_returns"),TEXT("add_ubergraph_page_calls"),TEXT("add_unit_node_calls"),TEXT("add_unit_node_returns"),TEXT("allocate_pins_calls"),TEXT("asset_delete_calls"),TEXT("asset_delete_returns"),TEXT("asset_duplicate_calls"),TEXT("asset_duplicate_returns"),TEXT("asset_exists_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("auto_VM_recompile_requests"),TEXT("break_all_pin_links_calls"),TEXT("can_create_connection_calls"),TEXT("create_node_guid_calls"),TEXT("editor_asset_load_calls"),TEXT("editor_asset_load_returns"),TEXT("event_graph_create_calls"),TEXT("event_graph_create_returns"),TEXT("external_descriptor_value_reads"),TEXT("force_save_calls"),TEXT("force_save_returns"),TEXT("full_compile_calls"),TEXT("full_compile_returns"),TEXT("get_all_graphs_calls"),TEXT("graph_add_node_calls"),TEXT("hierarchy_controller_getter_calls"),TEXT("import_bones_calls"),TEXT("import_bones_returns"),TEXT("import_curves_calls"),TEXT("import_curves_returns"),TEXT("import_mesh_curves_calls"),TEXT("import_mesh_curves_returns"),TEXT("make_directory_calls"),TEXT("mark_structurally_modified_calls"),TEXT("nested_topology_read_calls"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("node_modify_calls"),TEXT("node_position_write_calls"),TEXT("node_struct_property_search_calls"),TEXT("nodes_serialized"),TEXT("notify_graph_changed_calls"),TEXT("outer_transaction_calls"),TEXT("pins_inventory_calls"),TEXT("pins_visited"),TEXT("post_placed_calls"),TEXT("preview_mesh_getter_calls"),TEXT("propagate_hierarchy_calls"),TEXT("property_export_calls"),TEXT("reconstruct_calls"),TEXT("reflected_property_reads"),TEXT("request_rig_init_calls"),TEXT("rig_asset_reference_getter_calls"),TEXT("rig_factory_calls"),TEXT("rig_factory_returns"),TEXT("set_preview_mesh_calls"),TEXT("set_rig_class_calls"),TEXT("source_class_lookups"),TEXT("state_capture_calls"),TEXT("transient_rig_create_calls"),TEXT("transient_rig_create_returns"),TEXT("try_create_connection_calls"),TEXT("try_create_connection_returns")}) { C->SetNumberField(K,0); } R.Data->SetObjectField(TEXT("native_call_counts"),C); }
FResult Finish(FResult& R)
{ bool ObservationIncomplete=false;R.Data->TryGetBoolField(TEXT("observation_incomplete"),ObservationIncomplete);if(ObservationIncomplete) { R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;if(R.ErrorCode.IsEmpty()) { R.ErrorCode=TEXT("controlrig_sample_observation_incomplete"); } }if(Serialize(R.Data).IsEmpty()) { R.Data->RemoveField(TEXT("legacy_result"));R.Data->RemoveField(TEXT("source_before_json"));R.Data->RemoveField(TEXT("source_after_json"));R.Data->RemoveField(TEXT("current_target_json"));R.Data->SetBoolField(TEXT("oversized_observations_omitted"),true);R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;R.ErrorCode=TEXT("controlrig_sample_output_incomplete");R.Error=TEXT("Current observations exceed bounded output; accepted prefix retained"); }return R; }
FResult Execute(const TSharedRef<FJsonObject>& P,const FString& Op,bool Asset)
{
    FResult R;Init(R,Op,GetBoolParam(P,TEXT("compile"),true),GetBoolParam(P,TEXT("save_asset"),false));if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { R.ErrorCode=TEXT("controlrig_sample_editor_invalid");R.Error=TEXT("Idle nonreentrant Editor game thread required");return Finish(R); }
    struct Scope { Scope(FResult& V,const FString& O) { CurrentResult=&V;CurrentOperation=O;CurrentBlueprint=nullptr;SourceBlueprint=nullptr;SourceRigBlueprint=nullptr;SelectedTargetAsset=nullptr;SourceBefore.Reset();TargetBefore.Reset();Held.Reset();HeldObjects.Reset();KnownPins.Reset();PropertyValues=0; }~Scope() { CurrentResult=nullptr;CurrentBlueprint=nullptr;SourceBlueprint=nullptr;SourceRigBlueprint=nullptr;SelectedTargetAsset=nullptr;SourceBefore.Reset();TargetBefore.Reset();Held.Reset();HeldObjects.Reset();KnownPins.Reset();AnimBase=nullptr;AnimStruct=PoseStruct=ComponentPoseStruct=nullptr; } } Lifetime(R,Op);
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(FText::FromString(Op));R.Stage=TEXT("original_composite");auto Legacy=Asset?RunAsset(P):RunNode(P);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("legacy_result_returned"),true);
    if(SelectedTargetAsset&&Hold(SelectedTargetAsset)) { auto* BP=Cast<UBlueprint>(SelectedTargetAsset);CurrentBlueprint=BP;const auto Current=Capture(BP);const FString Text=Serialize(Current);R.Data->SetStringField(TEXT("current_target_json"),Text);if(TargetBefore) { const auto Prior=Serialize(TargetBefore);R.bChanged|=!Prior.IsEmpty()&&!Text.IsEmpty()&&!Prior.Equals(Text,ESearchCase::CaseSensitive); }R.Data->SetStringField(TEXT("current_target_path"),SelectedTargetAsset->GetPathName());R.Data->SetBoolField(TEXT("current_target_dirty"),SelectedTargetAsset->GetOutermost()->IsDirty()); }
    bool SourcePreserved=true;if(SourceBlueprint&&Hold(SourceBlueprint)) { CurrentBlueprint=SourceBlueprint;const auto Current=Capture(SourceBlueprint);const auto Before=Serialize(SourceBefore),After=Serialize(Current);R.Data->SetStringField(TEXT("source_after_json"),After);SourcePreserved=!Before.IsEmpty()&&!After.IsEmpty()&&Before.Equals(After,ESearchCase::CaseSensitive);R.Data->SetBoolField(TEXT("actual_source_state_preserved"),SourcePreserved);if(!SourcePreserved) { R.bChanged=true; } }
    bool UnprovenWriter=false;R.Data->TryGetBoolField(TEXT("unproven_asset_writer_return"),UnprovenWriter);R.bChangeStateKnown=!Incomplete()&&!UnprovenWriter;if(Incomplete()) { R.ErrorCode=TEXT("controlrig_sample_observation_incomplete");return Finish(R); }
    bool LegacySuccess=false;Legacy->TryGetBoolField(TEXT("success"),LegacySuccess);if(!LegacySuccess) { R.ErrorCode=TEXT("controlrig_sample_original_failure");Legacy->TryGetStringField(TEXT("error"),R.Error);R.Data->SetBoolField(TEXT("late_failure_no_automatic_cleanup"),R.bMutationAttempted);return Finish(R); }
    if(SourceRigBlueprint&&Hold(SourceRigBlueprint)) { R.Data->SetBoolField(TEXT("source_rig_dirty_after_transient"),SourceRigBlueprint->GetOutermost()->IsDirty());R.Data->SetStringField(TEXT("source_rig_class_after_transient"),GetPathNameSafe(RigBlueprintClass(SourceRigBlueprint))); }
    const bool Dry=GetBoolParam(P,TEXT("dry_run"),false)||GetBoolParam(P,TEXT("validate_only"),false);bool CurrentProof=Dry||(SelectedTargetAsset&&Hold(SelectedTargetAsset));if(Asset&&!Dry) { CurrentProof&=CurrentRigElements(Legacy,Cast<UControlRigBlueprint>(SelectedTargetAsset)); }if(!Asset&&!Dry) { bool ClassMatch=false,Edges=false;R.Data->TryGetBoolField(TEXT("current_rig_class_matches"),ClassMatch);R.Data->TryGetBoolField(TEXT("current_reciprocal_pose_edges_verified"),Edges);CurrentProof&=ClassMatch&&Edges; }
    R.bPostconditionPassed=R.bChangeStateKnown&&SourcePreserved&&CurrentProof&&(!R.bCompileAttempted||(R.Compiled.IsSet()&&R.Compiled.GetValue()))&&(!R.bSaveAttempted||R.bSaved);if(!R.bPostconditionPassed) { R.ErrorCode=TEXT("controlrig_sample_current_postcondition_failed");R.Error=TEXT("Current source/target/compile/save proof failed; original prefix retained");return Finish(R); }R.bSuccess=true;R.Stage=TEXT("complete");return Finish(R);
}
}
Variable::FResult EnsureAnimGraphNodeSample(const FString& SourceAnimBPPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bReplaceExistingLinks,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly)
{ auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("source_anim_bp_path"),SourceAnimBPPath);if(!ControlRigAssetPath.IsEmpty()) { P->SetStringField(TEXT("control_rig_asset_path"),ControlRigAssetPath); }if(!ControlRigClassPath.IsEmpty()) { P->SetStringField(TEXT("control_rig_class_path"),ControlRigClassPath); }if(!TargetAnimBPPath.IsEmpty()) { P->SetStringField(TEXT("target_anim_bp_path"),TargetAnimBPPath); }P->SetStringField(TEXT("target_root"),TargetRoot);P->SetStringField(TEXT("variant_name"),VariantName);P->SetBoolField(TEXT("allow_existing_target_reuse"),bAllowExistingTargetReuse);P->SetBoolField(TEXT("overwrite_existing"),bOverwriteExisting);P->SetBoolField(TEXT("replace_existing_links"),bReplaceExistingLinks);P->SetBoolField(TEXT("compile"),bCompile);P->SetBoolField(TEXT("save_asset"),bSaveAsset);P->SetBoolField(TEXT("dry_run"),bDryRun);P->SetBoolField(TEXT("validate_only"),bValidateOnly);return Execute(P,TEXT("ensure_anim_graph_controlrig_node_sample"),false); }
Variable::FResult EnsureSampleAsset(const FString& TargetControlRigPath,const FString& SkeletalMeshPath,const FString& SkeletonPath,const FString& PreviewSkeletalMeshPath,const FString& SetupMode,const FString& CurvesJson,const FString& NullsJson,const FString& ControlsJson,const FString& UnitNodesJson,int64 MaxDirectCurves,int64 MaxDirectNulls,int64 MaxDirectControls,int64 MaxUnitNodes,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bModularRig,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly)
{ auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("target_control_rig_path"),TargetControlRigPath);P->SetStringField(TEXT("skeletal_mesh_path"),SkeletalMeshPath);P->SetStringField(TEXT("skeleton_path"),SkeletonPath);P->SetStringField(TEXT("preview_skeletal_mesh_path"),PreviewSkeletalMeshPath);P->SetStringField(TEXT("setup_mode"),SetupMode);P->SetNumberField(TEXT("max_direct_curves"),FMath::Clamp<int64>(MaxDirectCurves,0,128));P->SetNumberField(TEXT("max_direct_nulls"),FMath::Clamp<int64>(MaxDirectNulls,0,128));P->SetNumberField(TEXT("max_direct_controls"),FMath::Clamp<int64>(MaxDirectControls,0,128));P->SetNumberField(TEXT("max_unit_nodes"),FMath::Clamp<int64>(MaxUnitNodes,0,6));P->SetBoolField(TEXT("allow_existing_target_reuse"),bAllowExistingTargetReuse);P->SetBoolField(TEXT("overwrite_existing"),bOverwriteExisting);P->SetBoolField(TEXT("modular_rig"),bModularRig);P->SetBoolField(TEXT("compile"),bCompile);P->SetBoolField(TEXT("save_asset"),bSaveAsset);P->SetBoolField(TEXT("dry_run"),bDryRun);P->SetBoolField(TEXT("validate_only"),bValidateOnly);FResult Bad;Init(Bad,TEXT("ensure_controlrig_sample_asset"),bCompile,bSaveAsset);if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { Bad.ErrorCode=TEXT("controlrig_sample_editor_invalid");Bad.Error=TEXT("Idle Editor game thread required before carriers");return Bad; }CurrentResult=&Bad;const bool Good=Pack(CurvesJson,TEXT("curves"),P,true)&&Pack(NullsJson,TEXT("nulls"),P,true)&&Pack(ControlsJson,TEXT("controls"),P,true)&&Pack(UnitNodesJson,TEXT("unit_nodes"),P,true);CurrentResult=nullptr;if(!Good) { Bad.ErrorCode=TEXT("controlrig_sample_json_invalid");return Bad; }return Execute(P,TEXT("ensure_controlrig_sample_asset"),true); }
}
