#include "Compatibility/UE58/CubelessAnimGraphControlRigApi.h"
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
namespace Cubeless::UE58::AnimGraphControlRig
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
void NoteUnsafe(const TCHAR* Error) { if(CurrentResult) { CurrentResult->Data->SetBoolField(TEXT("observation_incomplete"),true);CurrentResult->ErrorCode=TEXT("anim_graph_controlrig_observation_incomplete");CurrentResult->Error=Error; } }
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

bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, bool bDefaultValue)
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

FString CurrentOperation;UEdGraph* SelectedGraph=nullptr;UAnimGraphNode_ControlRig* SelectedRig=nullptr;UAnimGraphNode_ModifyCurve* SelectedCurve=nullptr;FEdGraphPinReference SelectedUpstream;TMap<FName,UObject*> ExpectedDefaultObjects;TSharedPtr<FJsonObject> BeforeSource;FString BeforeText;TSet<FString> AllowedNodes;
bool ParseModifyCurveApplyMode(const FString& RawValue, EModifyCurveApplyMode& OutApplyMode)
{
    const FString Value = RawValue.TrimStartAndEnd().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
    if (Value.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
    {
        OutApplyMode = EModifyCurveApplyMode::Add;
        return true;
    }
    if (Value.Equals(TEXT("Scale"), ESearchCase::IgnoreCase))
    {
        OutApplyMode = EModifyCurveApplyMode::Scale;
        return true;
    }
    if (Value.Equals(TEXT("Blend"), ESearchCase::IgnoreCase))
    {
        OutApplyMode = EModifyCurveApplyMode::Blend;
        return true;
    }
    if (Value.Equals(TEXT("WeightedMovingAverage"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("WMA"), ESearchCase::IgnoreCase))
    {
        OutApplyMode = EModifyCurveApplyMode::WeightedMovingAverage;
        return true;
    }
    if (Value.Equals(TEXT("RemapCurve"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Remap"), ESearchCase::IgnoreCase))
    {
        OutApplyMode = EModifyCurveApplyMode::RemapCurve;
        return true;
    }
    return false;
}
FString GetModifyCurveApplyModeName(EModifyCurveApplyMode ApplyMode)
{
    switch (ApplyMode)
    {
    case EModifyCurveApplyMode::Add:
        return TEXT("Add");
    case EModifyCurveApplyMode::Scale:
        return TEXT("Scale");
    case EModifyCurveApplyMode::Blend:
        return TEXT("Blend");
    case EModifyCurveApplyMode::WeightedMovingAverage:
        return TEXT("WeightedMovingAverage");
    case EModifyCurveApplyMode::RemapCurve:
        return TEXT("RemapCurve");
    default:
        return TEXT("Unknown");
    }
}
void UpsertModifyCurveEntry(TArray<TPair<FName, float>>& Entries, const FName& CurveName, float CurveValue)
{
    if (CurveName.IsNone())
    {
        return;
    }

    for (TPair<FName, float>& Entry : Entries)
    {
        if (Entry.Key == CurveName)
        {
            Entry.Value = CurveValue;
            return;
        }
    }

    Entries.Emplace(CurveName, CurveValue);
}
TArray<TPair<FName, float>> BuildDefaultModifyCurveEntries()
{
    TArray<TPair<FName, float>> Entries;
    Entries.Emplace(FName(TEXT("IK_blend_interact")), 1.0f);
    Entries.Emplace(FName(TEXT("IKBlend_l")), 1.0f);
    return Entries;
}
void SortModifyCurveEntries(TArray<TPair<FName, float>>& Entries)
{
    Entries.Sort([](const TPair<FName, float>& A, const TPair<FName, float>& B)
    {
        return A.Key.LexicalLess(B.Key);
    });
}
TArray<TPair<FName, float>> ParseModifyCurveEntries(const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    OutError.Reset();
    if (!Params.IsValid())
    {
        return BuildDefaultModifyCurveEntries();
    }

    TArray<TPair<FName, float>> Entries;
    const TSharedPtr<FJsonObject>* CurveObject = nullptr;
    if (Params->TryGetObjectField(TEXT("curve_values"), CurveObject) || Params->TryGetObjectField(TEXT("curves"), CurveObject))
    {
        if((*CurveObject)->Values.Num()>1024) { OutError=TEXT("Curve object exceeds1024 entries");return {}; }
        for (const auto& Pair : (*CurveObject)->Values)
        {
            const FString Key(Pair.Key.Len(),*Pair.Key);
            if(!SafeText(Key,NAME_SIZE-1)) { OutError=TEXT("Curve key outside safe FName/NUL bound");return {}; }
            if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Number)
            {
                OutError = FString::Printf(TEXT("Curve '%s' value must be numeric"), *Key);
                return TArray<TPair<FName, float>>();
            }
            if(!FloatProjectionSafe(Pair.Value->AsNumber())) { OutError=TEXT("Curve value outside finite float32 scope");return {}; }
            UpsertModifyCurveEntry(Entries, FName(*Key), static_cast<float>(Pair.Value->AsNumber()));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* CurveArray = nullptr;
    if (Params->TryGetArrayField(TEXT("curve_values"), CurveArray) || Params->TryGetArrayField(TEXT("curves"), CurveArray))
    {
        if(CurveArray->Num()>1024) { OutError=TEXT("Curve array exceeds1024 entries");return {}; }
        for (const TSharedPtr<FJsonValue>& Value : *CurveArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                OutError = TEXT("Curve array entries must be objects with 'name' and 'value'");
                return TArray<TPair<FName, float>>();
            }

            const TSharedPtr<FJsonObject> EntryObject = Value->AsObject();
            FString NameText;
            double CurveValue = 0.0;
            if (!EntryObject->TryGetStringField(TEXT("name"), NameText) || NameText.TrimStartAndEnd().IsEmpty())
            {
                OutError = TEXT("Curve array entry is missing non-empty 'name'");
                return TArray<TPair<FName, float>>();
            }
            if (!EntryObject->TryGetNumberField(TEXT("value"), CurveValue))
            {
                OutError = FString::Printf(TEXT("Curve array entry '%s' is missing numeric 'value'"), *NameText);
                return TArray<TPair<FName, float>>();
            }

            const FString Name=NameText.TrimStartAndEnd();if(!SafeText(Name,NAME_SIZE-1)||!FloatProjectionSafe(CurveValue)) { OutError=TEXT("Curve name/value outside finite source scope");return {}; }
            UpsertModifyCurveEntry(Entries, FName(*Name), static_cast<float>(CurveValue));
        }
    }

    if (Entries.Num() == 0)
    {
        Entries = BuildDefaultModifyCurveEntries();
    }

    SortModifyCurveEntries(Entries);
    return Entries;
}
TSharedPtr<FJsonObject> ModifyCurveEntriesToJsonObject(const TArray<TPair<FName, float>>& Entries)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    for (const TPair<FName, float>& Entry : Entries)
    {
        Object->SetNumberField(Entry.Key.ToString(), Entry.Value);
    }
    return Object;
}
bool ModifyCurveEntriesMatchNode(const FAnimNode_ModifyCurve* ModifyCurveNode, const TArray<TPair<FName, float>>& Entries)
{
    if (!ModifyCurveNode)
    {
        return false;
    }

    if (ModifyCurveNode->CurveNames.Num() != Entries.Num() ||
        ModifyCurveNode->CurveValues.Num() != Entries.Num() ||
        ModifyCurveNode->CurveMap.Num() != Entries.Num())
    {
        return false;
    }

    for (int32 Index = 0; Index < Entries.Num(); ++Index)
    {
        if (ModifyCurveNode->CurveNames[Index] != Entries[Index].Key ||
            !FMath::IsNearlyEqual(ModifyCurveNode->CurveValues[Index], Entries[Index].Value, KINDA_SMALL_NUMBER))
        {
            return false;
        }

        const float* MapValue = ModifyCurveNode->CurveMap.Find(Entries[Index].Key);
        if (!MapValue || !FMath::IsNearlyEqual(*MapValue, Entries[Index].Value, KINDA_SMALL_NUMBER))
        {
            return false;
        }
    }

    return true;
}

FAnimNode_ModifyCurve* GetMutableModifyCurveAnimNode(UAnimGraphNode_ModifyCurve* Node)
{
    auto& R=*CurrentResult;if(!Node||!Member(Node,Node->GetGraph())) { return nullptr; }
    Count(R,TEXT("get_fnode_property_calls"));FStructProperty* P=Node->GetFNodeProperty();
    if(!P||!Loaded(P->Struct)||!Hold(P->GetOwnerStruct())||P->Struct!=FAnimNode_ModifyCurve::StaticStruct()||P->GetFName()!=FName(TEXT("Node"))||P->ArrayDim!=1||!Layout(P,Node->GetClass()->GetPropertiesSize())) { NoteUnsafe(TEXT("Exact current ModifyCurve runtime property/layout required"));return nullptr; }
    Count(R,TEXT("get_fnode_type_calls"));if(Node->GetFNodeType()!=P->Struct) { NoteUnsafe(TEXT("Runtime property/type accessor identity mismatch"));return nullptr; }
    Count(R,TEXT("get_fnode_calls"));FAnimNode_Base* Base=Node->GetFNode();auto* Runtime=static_cast<FAnimNode_ModifyCurve*>(Base);
    if(!Runtime||Runtime!=P->ContainerPtrToValuePtr<FAnimNode_ModifyCurve>(Node)||Runtime->CurveNames.Num()>1024||Runtime->CurveValues.Num()>1024||Runtime->CurveMap.Num()>1024) { NoteUnsafe(TEXT("Current runtime accessor/parallelcurve array health bound failed"));return nullptr; }
    R.Data->SetBoolField(TEXT("current_curve_arrays_aligned"),Runtime->CurveNames.Num()==Runtime->CurveValues.Num());
    for(float V:Runtime->CurveValues) { if(!FMath::IsFinite(V)) { NoteUnsafe(TEXT("Nonfinite existing curve value"));return nullptr; } }
    for(const auto& Entry:Runtime->CurveMap) { if(!FMath::IsFinite(Entry.Value)) { NoteUnsafe(TEXT("Nonfinite existing curve map value"));return nullptr; } }
    R.Data->SetStringField(TEXT("runtime_node_type"),P->Struct->GetPathName());R.Data->SetNumberField(TEXT("runtime_node_property_offset"),P->GetOffset_ForInternal());R.Data->SetBoolField(TEXT("runtime_public_accessor_identity_verified"),true);return Runtime;
}


TSharedPtr<FJsonObject> Capture(UBlueprint* BP);
bool CurrentClassesSafe(UBlueprint* BP);
void AddAllowed(UEdGraphNode* N,TSet<FString>& Allowed);
FAnimNode_ControlRig* GetMutableControlRigAnimNode(UAnimGraphNode_ControlRig* N)
{
    if(!N||!Member(N,N->GetGraph())) { return nullptr; }auto& R=*CurrentResult;Count(R,TEXT("get_fnode_property_calls"));auto* P=N->GetFNodeProperty();
    if(!P||!Hold(P->Struct)||P->Struct!=FAnimNode_ControlRig::StaticStruct()||P->ArrayDim!=1||P->GetFName()!=FName(TEXT("Node"))||!Layout(P,N->GetClass()->GetPropertiesSize())) { NoteUnsafe(TEXT("Exact public current ControlRig Node property/type/layout required"));return nullptr; }
    Count(R,TEXT("get_fnode_type_calls"));if(N->GetFNodeType()!=P->Struct) { NoteUnsafe(TEXT("ControlRig public runtime type disagrees with reflected property"));return nullptr; }
    Count(R,TEXT("get_fnode_calls"));auto* V=static_cast<FAnimNode_ControlRig*>(N->GetFNode());if(!V||V!=P->ContainerPtrToValuePtr<FAnimNode_ControlRig>(N)) { NoteUnsafe(TEXT("ControlRig public current runtime pointer mismatch"));return nullptr; }return V;
}
bool IsComponentPoseLinkPin(const UEdGraphPin* P)
{ return PinKnown(P)&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&P->PinType.PinSubCategoryObject.Get()==ComponentPoseStruct; }
bool CustomPinsSafe(UAnimGraphNode_ControlRig* N)
{
    if(!N||!Member(N,N->GetGraph())) { return false; }auto* P=FindFProperty<FArrayProperty>(N->GetClass(),TEXT("CustomPinProperties"));auto* Inner=P?CastField<FStructProperty>(P->Inner):nullptr;
    if(!P||P->ArrayDim!=1||!Layout(P,N->GetClass()->GetPropertiesSize())||!Inner||!Inner->Struct||!Hold(Inner->Struct)) { NoteUnsafe(TEXT("Current public reflected CustomPinProperties array shape unavailable"));return false; }
    auto* Name=FindFProperty<FNameProperty>(Inner->Struct,TEXT("PropertyName"));auto* Visible=FindFProperty<FBoolProperty>(Inner->Struct,TEXT("bShowPin"));
    if(!Layout(Name,Inner->Struct->GetStructureSize())||!Layout(Visible,Inner->Struct->GetStructureSize())||Name->ArrayDim!=1||Visible->ArrayDim!=1) { NoteUnsafe(TEXT("Current optional pin name/bShowPin exact layout unavailable"));return false; }
    FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(N));if(A.Num()>1024) { NoteUnsafe(TEXT("Custom optional pin array exceeds1024"));return false; }return true;
}
// No exporter is called here. Stock scalar/stock math structs and bounded containers
// are admitted before the SDK exports every external variable, including unrequested ones.
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
bool TargetSkeletonSafe(UAnimBlueprint* BP)
{
    auto* Skeleton=BP->TargetSkeleton.Get();if(!Skeleton) { return true; }if(!Hold(Skeleton)) { return false; }const auto& Ref=Skeleton->GetReferenceSkeleton();const auto& Bones=Ref.GetRefBoneInfo();const auto& Poses=Ref.GetRefBonePose();if(Bones.Num()>4096||Bones.Num()!=Poses.Num()) { NoteUnsafe(TEXT("Target skeleton arrays exceed finite SDK mapping scope"));return false; }
    for(int32 I=0;I<Bones.Num();++I) { if(Bones[I].ParentIndex<-1||Bones[I].ParentIndex>=I||Poses[I].ContainsNaN()) { NoteUnsafe(TEXT("Target skeleton parent/transform admission failed before SDK template mapping"));return false; } }return true;
}
bool RigSourceSafe(UAnimGraphNode_ControlRig* N)
{
    auto* V=GetMutableControlRigAnimNode(N);auto* Target=Cast<UAnimBlueprint>(CurrentBlueprint);if(!V||!CustomPinsSafe(N)||!Target||!TargetSkeletonSafe(Target)) { return false; }
    Count(*CurrentResult,TEXT("rig_asset_reference_getter_calls"));const auto Ref=V->GetControlRigAssetReference();UClass* Class=Ref.GetBlueprintClass().Get();auto* Asset=Ref.GetControlRigAsset().Get();
    if(!Hold(Class)||!Hold(Asset)) { return false; }
    CurrentResult->Data->SetStringField(TEXT("current_control_rig_class"),Class?Class->GetPathName():TEXT(""));CurrentResult->Data->SetBoolField(TEXT("asset_only_rig_reference"),Asset!=nullptr);
    if(!Class&&!Asset) { CurrentResult->Data->SetBoolField(TEXT("empty_reference_original_safe_branch"),true);return true; }
    if(Asset) { NoteUnsafe(TEXT("Runtime-asset-only rig bag/construction scope requires separate finite source admission; valid followup retained"));return false; }
    if(!Class||!Hold(Class)||!Class->IsChildOf(UControlRig::StaticClass())||Class->HasAnyClassFlags(CLASS_Abstract)||!Cast<UBlueprintGeneratedClass>(Class)) { NoteUnsafe(TEXT("Loaded ordinary Blueprint-generated ControlRig class/lineage required before SDK unchecked generated-class cast"));return false; }
    auto* RigBlueprint=Cast<UBlueprint>(Class->ClassGeneratedBy);if(!RigBlueprint||!Hold(RigBlueprint)||RigBlueprint->GeneratedClass.Get()!=Class||(RigBlueprint->Status!=BS_UpToDate&&RigBlueprint->Status!=BS_UpToDateWithWarnings)||RigBlueprint->NewVariables.Num()>1024) { NoteUnsafe(TEXT("Current compiled rig Blueprint owner/variable descriptors required before SDK fallback"));return false; }
    auto* SkeletonClass=RigBlueprint->SkeletonGeneratedClass.Get();if(!SkeletonClass||!Hold(SkeletonClass)||SkeletonClass->ClassGeneratedBy!=RigBlueprint||!SkeletonClass->IsChildOf(UControlRig::StaticClass())) { NoteUnsafe(TEXT("Loaded matching rig skeleton class required before SDK target-class pin enumeration"));return false; }
    for(UClass* Owner:{Class,SkeletonClass}) { int32 Parents=0;for(UClass* C=Owner;C;C=C->GetSuperClass()) { if(++Parents>32||!Hold(C)) { NoteUnsafe(TEXT("Loaded bounded generated/skeleton rig class lineage required"));return false; } } }
    int32 SkeletonFields=0;for(TFieldIterator<FProperty> It(SkeletonClass);It;++It) { const auto* P=*It;if(++SkeletonFields>4096||!Layout(P,SkeletonClass->GetPropertiesSize())||P->ArrayDim<1||P->ArrayDim>1024||!Hold(P->GetOwnerStruct())) { NoteUnsafe(TEXT("Rig skeleton fields exceed current loaded bounded layout"));return false; } }
    Count(*CurrentResult,TEXT("existing_rig_CDO_queries"));auto* CDO=Cast<UControlRig>(Class->GetDefaultObject(false));if(!CDO||!Hold(CDO)||CDO->GetClass()!=Class||CDO!=Class->ClassDefaultObject) { NoteUnsafe(TEXT("Existing loaded rig CDO required without validator-created CDO"));return false; }
    const auto GeneratedBy=CDO->GetGeneratedByAsset();if(GeneratedBy.GetObject()||GeneratedBy.GetInterface()) { NoteUnsafe(TEXT("Nonempty runtime-asset GeneratedBy would enter mutable bag/unchecked property path before descriptor validation"));return false; }
    auto* Hierarchy=CDO->GetHierarchy();if(!Hierarchy||!Hold(Hierarchy)||Hierarchy->Num()>4096) { NoteUnsafe(TEXT("Existing loaded bounded CDO hierarchy required before SDK control iteration"));return false; }for(int32 I=0;I<Hierarchy->Num();++I) { if(!Hierarchy->Get(I)) { NoteUnsafe(TEXT("Null existing CDO hierarchy element before SDK control mapping"));return false; } }
    int32 Properties=0;for(TFieldIterator<FProperty> It(Class);It;++It) { const auto* P=*It;if(++Properties>4096||!Layout(P,Class->GetPropertiesSize())||P->ArrayDim<1||P->ArrayDim>1024||!Hold(P->GetOwnerStruct())) { NoteUnsafe(TEXT("Rig CDO fields outside finite current loaded layout"));return false; } }
    Count(*CurrentResult,TEXT("rig_modular_state_queries"));if(Ref.IsModularRig()||Ref.IsRigModule()) { NoteUnsafe(TEXT("Modular/module rig preload/construction is retained valid scope but not finite initial Blueprint-rig admission"));return false; }
    Count(*CurrentResult,TEXT("rig_external_variable_queries"));const auto Variables=Ref.GetExternalVariables();if(Variables.Num()>1024) { NoteUnsafe(TEXT("External variable descriptors exceed1024"));return false; }
    TArray<TSharedPtr<FJsonValue>> Receipts;int32 ExternalVisits=0;
    for(const auto& Variable:Variables)
    { const auto* P=Variable.GetProperty();if(!P||!Hold(P->GetOwnerStruct())||!Layout(P,Class->GetPropertiesSize())||P->ArrayDim<1||P->ArrayDim>1024||!Class->FindPropertyByName(P->GetFName())||Class->FindPropertyByName(P->GetFName())!=P||!Variable.GetMemory()||Variable.GetMemory()!=P->ContainerPtrToValuePtr<uint8>(CDO)) { NoteUnsafe(TEXT("Every SDK external descriptor requires nonnull current property/layout/canonical CDO memory before unchecked CreateCustomPins loop"));return false; }
      if(P->ArrayDim!=1||!ExternalValueSafe(P,Variable.GetMemory(),0,ExternalVisits)) { NoteUnsafe(TEXT("Every external variable requires bounded no-export values before SDK default ExportText; custom value scopes remain incomplete"));return false; }
      auto X=MakeShared<FJsonObject>();X->SetStringField(TEXT("name"),P->GetName());X->SetStringField(TEXT("owner"),P->GetOwnerStruct()->GetPathName());X->SetNumberField(TEXT("array_dim"),P->ArrayDim);X->SetBoolField(TEXT("property_and_memory_verified"),true);X->SetBoolField(TEXT("value_no_export_bound_verified"),true);Receipts.Add(MakeShared<FJsonValueObject>(X)); }
    CurrentResult->Data->SetArrayField(TEXT("current_rig_external_descriptor_receipts"),Receipts);CurrentResult->Data->SetBoolField(TEXT("SDK_template_rig_create_initialize_evaluate_possible"),true);CurrentResult->Data->SetBoolField(TEXT("SDK_private_template_execution_or_hierarchy_bound_proven"),false);return true;
}
FString GetControlRigClassPathFromNode(UAnimGraphNode_ControlRig* N)
{ auto* V=GetMutableControlRigAnimNode(N);if(!V) { return FString(); }Count(*CurrentResult,TEXT("rig_asset_reference_getter_calls"));const auto Ref=V->GetControlRigAssetReference();auto* C=Ref.GetBlueprintClass().Get();if(C&&!Hold(C)) { return FString(); }return C?C->GetPathName():FString(); }
bool ReconstructRig(UAnimGraphNode_ControlRig* N)
{ if(!RigSourceSafe(N)) { return false; }WriteIntent(TEXT("reconstruct_calls"));static_cast<UEdGraphNode*>(N)->ReconstructNode();return RefreshInventory()&&Member(N,SelectedGraph)&&CustomPinsSafe(N)&&RigSourceSafe(N); }
bool ReconstructCurve(UAnimGraphNode_ModifyCurve* N)
{ if(!GetMutableModifyCurveAnimNode(N)) { return false; }WriteIntent(TEXT("reconstruct_calls"));static_cast<UEdGraphNode*>(N)->ReconstructNode();return RefreshInventory()&&Member(N,SelectedGraph)&&GetMutableModifyCurveAnimNode(N); }
void ModifyNode(UEdGraphNode* N) { WriteIntent(TEXT("node_modify_calls"));N->Modify(); }
bool ReadySelected(UAnimGraphNode_ControlRig* N,UEdGraph* G)
{ if(!RequiredTypes(G,*CurrentResult)||!RefreshInventory()||!CurrentClassesSafe(CurrentBlueprint)||!RigSourceSafe(N)) { return false; }SelectedGraph=G;SelectedRig=N;AddAllowed(N,AllowedNodes);BeforeSource=Capture(CurrentBlueprint);BeforeText=Serialize(BeforeSource);CurrentResult->Data->SetStringField(TEXT("selected_state_before_json"),BeforeText);return !BeforeText.IsEmpty()&&!Incomplete(); }
UBlueprint* FindBlueprint(const FString& Name)
{ auto* BP=FindBlueprintByName(Name,*CurrentResult);if(BP&&Hold(BP)) { CurrentBlueprint=BP;CurrentResult->Data->SetStringField(TEXT("blueprint"),BP->GetPathName()); }return BP; }
UEdGraph* ResolveBlueprintGraphForNodeCommand(UBlueprint* BP,const TSharedPtr<FJsonObject>& P,FString& Error)
{ CurrentBlueprint=BP;if(!Hold(BP)||!Hold(BP->GetOutermost())) { return nullptr; }auto* G=Resolve(BP,GetStringParam(P,TEXT("graph_name")),GetStringParam(P,TEXT("graph_id")),GetStringParam(P,TEXT("graph_type")),*CurrentResult);if(!G||!Hold(G->Schema.Get())||!Hold(G->GetSchema())||!RefreshInventory()) { Error=CurrentResult->Error;return nullptr; }return G; }
bool IsMCPDependencyReference(const FString& Value)
{ FString Candidate=FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();Candidate.TrimQuotesInline();if(Candidate.IsEmpty()) { return false; }return Candidate.Contains(TEXT("/Script/UnrealMCP"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("/UnrealMCP"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("UnrealMCP"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("MCPUnreal"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("mcp_unreal"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("/Script/CubelessControlRigTools"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("CubelessControlRigTools"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("CubelessControlRigToolsEditor"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("/Script/CubelessMCPUE58Editor"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("/Script/ToolsetRegistry"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("/Script/ModelContextProtocol"),ESearchCase::IgnoreCase); }
FString GetMCPDependencyReferenceError(const FString& Field,const FString& Value) { const FString Label=Field.IsEmpty()?TEXT("value"):Field;return FString::Printf(TEXT("Refusing to persist MCP-only reference in '%s': %s. UnrealMCP is an editor authoring bridge; finished assets must not depend on it."),*Label,*Value); }
UObject* LoadObjectForPin(const FString& Raw)
{ const FString P=NormalizeAssetObjectPathForLoad(Raw);if(P.IsEmpty()||IsMCPDependencyReference(P)) { return nullptr; }return CountedLoad<UObject>(P,*CurrentResult); }
UClass* LoadClassForPin(const FString& Raw)
{ if(!SafeText(Raw,NAME_SIZE-1)||IsMCPDependencyReference(Raw)) { NoteUnsafe(TEXT("Default class path outside safe source scope"));return nullptr; }const FString P=NormalizeAssetObjectPathForLoad(Raw);if(auto* C=CountedLoad<UClass>(P,*CurrentResult)) { return C; }if(auto* C=FindFirstObject<UClass>(*Raw)) { return Hold(C)?C:nullptr; }return CountedLoad<UClass>(FString::Printf(TEXT("/Script/Engine.%s"),*Raw),*CurrentResult); }

FString GetPinDefaultStringForType(const TSharedPtr<FJsonValue>& V,const FEdGraphPinType& T);
FString GetPinDefaultString(const TSharedPtr<FJsonValue>& Value, const UEdGraphPin* Pin)
{
    if (!Value.IsValid())
    {
        return FString();
    }

    if (Value->Type == EJson::Boolean)
    {
        return Value->AsBool() ? TEXT("true") : TEXT("false");
    }

    if (Value->Type == EJson::Number)
    {
        if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
        {
            return FString::FromInt(FMath::RoundToInt(Value->AsNumber()));
        }
        return FString::SanitizeFloat(Value->AsNumber());
    }

    if (Value->Type == EJson::String)
    {
        return Value->AsString();
    }

    if (Value->Type == EJson::Array || Value->Type == EJson::Object)
    {
        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(Value, TEXT(""), Writer);
        return Serialized;
    }

    return FString();
}
const UEnum* GetEnumForPinType(const FEdGraphPinType& PinType)
{
    if (PinType.PinCategory != UEdGraphSchema_K2::PC_Byte || PinType.IsArray())
    {
        return nullptr;
    }

    return Cast<UEnum>(PinType.PinSubCategoryObject.Get());
}
bool IsEnumIndexHiddenOrSpacer(const UEnum* Enum, int32 Index)
{
    return !Enum ||
        !Enum->IsValidEnumValue(Enum->GetValueByIndex(Index)) ||
        Enum->HasMetaData(TEXT("Hidden"), Index) ||
        Enum->HasMetaData(TEXT("Spacer"), Index);
}
int32 FindFirstVisibleEnumIndex(const UEnum* Enum)
{
    if (!Enum)
    {
        return INDEX_NONE;
    }

    for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
    {
        if (!IsEnumIndexHiddenOrSpacer(Enum, Index))
        {
            return Index;
        }
    }

    return INDEX_NONE;
}
bool ResolveEnumValueDefaultString(const TSharedPtr<FJsonValue>& Value, const UEnum* Enum, FString& OutDefaultValue, int32& OutIndex, FString& OutError)
{
    OutDefaultValue.Reset();
    OutIndex = INDEX_NONE;
    OutError.Reset();

    if(Enum&&(!Hold(Enum)||Enum->NumEnums()>4096)) { OutError=TEXT("Loaded bounded enum required");return false; }
    if (!Enum)
    {
        OutError = TEXT("Invalid enum type");
        return false;
    }

    if (!Value.IsValid())
    {
        OutIndex = FindFirstVisibleEnumIndex(Enum);
        if (OutIndex == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("Enum has no visible values: %s"), *Enum->GetPathName());
            return false;
        }
    }
    else if (Value->Type == EJson::String)
    {
        FString RequestedValue = Value->AsString();
        RequestedValue.TrimStartAndEndInline();
        RequestedValue.TrimQuotesInline();
        if (RequestedValue.IsEmpty())
        {
            OutError = TEXT("Enum value cannot be empty");
            return false;
        }

        OutIndex = Enum->GetIndexByNameString(RequestedValue);
        if (OutIndex == INDEX_NONE && !RequestedValue.Contains(TEXT("::")))
        {
            OutIndex = Enum->GetIndexByNameString(FString::Printf(TEXT("%s::%s"), *Enum->GetName(), *RequestedValue));
        }
        if (OutIndex == INDEX_NONE)
        {
            OutIndex = Enum->GetIndexByNameString(Enum->GenerateFullEnumName(*RequestedValue));
        }
    }
    else if (Value->Type == EJson::Number)
    {
        const double NumberValue = Value->AsNumber();
        if(!FMath::IsFinite(NumberValue)||NumberValue<-9223372036854775808.0||NumberValue>=9223372036854775808.0) { OutError=TEXT("Enum int64 projection outside safe scope");return false; }
        const int64 IntegerValue = static_cast<int64>(NumberValue);
        if (!FMath::IsNearlyEqual(NumberValue, static_cast<double>(IntegerValue)))
        {
            OutError = TEXT("Enum numeric default must be an integer value");
            return false;
        }

        OutIndex = Enum->GetIndexByValue(IntegerValue);
    }
    else
    {
        OutError = TEXT("Enum default must be a string enum entry or integer enum value");
        return false;
    }

    if (OutIndex == INDEX_NONE)
    {
        OutError = FString::Printf(TEXT("Enum value not found for %s"), *Enum->GetPathName());
        return false;
    }
    if (IsEnumIndexHiddenOrSpacer(Enum, OutIndex))
    {
        OutError = FString::Printf(TEXT("Enum value is hidden or spacer: %s"), *Enum->GetNameStringByIndex(OutIndex));
        return false;
    }

    OutDefaultValue = Enum->GetNameStringByIndex(OutIndex);
    return true;
}
bool GetPinDefaultStringForTypeChecked(const TSharedPtr<FJsonValue>& Value, const FEdGraphPinType& PinType, FString& OutDefaultValue, FString& OutError)
{
    OutDefaultValue.Reset();
    OutError.Reset();
    if(!Hold(PinType.PinSubCategoryObject.Get())) { OutError=CurrentResult->Error;return false; }
    if(Value.IsValid()&&Value->Type==EJson::Number&&(PinType.PinCategory==UEdGraphSchema_K2::PC_Int||PinType.PinCategory==UEdGraphSchema_K2::PC_Int64||PinType.PinCategory==UEdGraphSchema_K2::PC_Byte)&&(Value->AsNumber()<-9223372036854775808.0||Value->AsNumber()>=9223372036854775808.0)) { OutError=TEXT("Original integer default projection outside safe int64 scope");return false; }

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte && Cast<UEnum>(PinType.PinSubCategoryObject.Get()) && PinType.IsArray())
    {
        OutError = TEXT("Default values for enum arrays are not supported");
        return false;
    }
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean && Value.IsValid() && Value->Type != EJson::Boolean)
    {
        OutError = TEXT("Boolean default values must be JSON booleans");
        return false;
    }
    if (Value.IsValid() &&
        Value->Type == EJson::String &&
        (PinType.PinCategory == UEdGraphSchema_K2::PC_Class || PinType.PinCategory == UEdGraphSchema_K2::PC_Object) &&
        IsMCPDependencyReference(Value->AsString()))
    {
        OutError = GetMCPDependencyReferenceError(TEXT("default_value"), Value->AsString());
        return false;
    }

    if (const UEnum* Enum = GetEnumForPinType(PinType))
    {
        int32 EnumIndex = INDEX_NONE;
        return ResolveEnumValueDefaultString(Value, Enum, OutDefaultValue, EnumIndex, OutError);
    }

    OutDefaultValue = GetPinDefaultStringForType(Value, PinType);
    return true;
}
FString GetPinDefaultStringForType(const TSharedPtr<FJsonValue>& Value, const FEdGraphPinType& PinType)
{
    if (!Value.IsValid())
    {
        return FString();
    }

    const bool bIsVector = PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get();
    const bool bIsRotator = PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == TBaseStructure<FRotator>::Get();
    const bool bIsTransform = PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == TBaseStructure<FTransform>::Get();

    if ((bIsVector || bIsRotator) && Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
        const double X = Values.IsValidIndex(0) ? Values[0]->AsNumber() : 0.0;
        const double Y = Values.IsValidIndex(1) ? Values[1]->AsNumber() : 0.0;
        const double Z = Values.IsValidIndex(2) ? Values[2]->AsNumber() : 0.0;
        if (bIsRotator)
        {
            return FString::Printf(TEXT("(Pitch=%s,Yaw=%s,Roll=%s)"), *FString::SanitizeFloat(X), *FString::SanitizeFloat(Y), *FString::SanitizeFloat(Z));
        }
        return FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"), *FString::SanitizeFloat(X), *FString::SanitizeFloat(Y), *FString::SanitizeFloat(Z));
    }

    if ((bIsVector || bIsRotator || bIsTransform) && Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> ObjectValue = Value->AsObject();
        if (ObjectValue.IsValid())
        {
            if (bIsTransform)
            {
                const FEdGraphPinType VectorType(UEdGraphSchema_K2::PC_Struct, NAME_None, TBaseStructure<FVector>::Get(), EPinContainerType::None, false, FEdGraphTerminalType());
                const FEdGraphPinType RotatorType(UEdGraphSchema_K2::PC_Struct, NAME_None, TBaseStructure<FRotator>::Get(), EPinContainerType::None, false, FEdGraphTerminalType());

                const TSharedPtr<FJsonValue> LocationValue = ObjectValue->TryGetField(TEXT("location")).IsValid()
                    ? ObjectValue->TryGetField(TEXT("location"))
                    : ObjectValue->TryGetField(TEXT("translation"));
                const TSharedPtr<FJsonValue> RotationValue = ObjectValue->TryGetField(TEXT("rotation"));
                const TSharedPtr<FJsonValue> ScaleValue = ObjectValue->TryGetField(TEXT("scale")).IsValid()
                    ? ObjectValue->TryGetField(TEXT("scale"))
                    : ObjectValue->TryGetField(TEXT("scale3d"));

                const FString Location = LocationValue.IsValid()
                    ? GetPinDefaultStringForType(LocationValue, VectorType)
                    : TEXT("(X=0,Y=0,Z=0)");
                const FString Rotation = RotationValue.IsValid()
                    ? GetPinDefaultStringForType(RotationValue, RotatorType)
                    : TEXT("(Pitch=0,Yaw=0,Roll=0)");
                const FString Scale = ScaleValue.IsValid()
                    ? GetPinDefaultStringForType(ScaleValue, VectorType)
                    : TEXT("(X=1,Y=1,Z=1)");
                // UEdGraphSchema_K2 validates transform defaults through
                // FTransform::InitFromString in UE 5.6. Emit the compact
                // Translation|Rotator|Scale form expected by that parser.
                FVector ParsedLocation = FVector::ZeroVector;
                FRotator ParsedRotation = FRotator::ZeroRotator;
                FVector ParsedScale = FVector::OneVector;
                ParsedLocation.InitFromString(Location);
                ParsedRotation.InitFromString(Rotation);
                ParsedScale.InitFromString(Scale);
                return FTransform(ParsedRotation, ParsedLocation, ParsedScale).ToString();
            }

            double X = 0.0;
            double Y = 0.0;
            double Z = 0.0;
            ObjectValue->TryGetNumberField(bIsRotator ? TEXT("pitch") : TEXT("x"), X);
            ObjectValue->TryGetNumberField(bIsRotator ? TEXT("yaw") : TEXT("y"), Y);
            ObjectValue->TryGetNumberField(bIsRotator ? TEXT("roll") : TEXT("z"), Z);
            if (bIsRotator)
            {
                return FString::Printf(TEXT("(Pitch=%s,Yaw=%s,Roll=%s)"), *FString::SanitizeFloat(X), *FString::SanitizeFloat(Y), *FString::SanitizeFloat(Z));
            }
            return FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"), *FString::SanitizeFloat(X), *FString::SanitizeFloat(Y), *FString::SanitizeFloat(Z));
        }
    }

    if (Value->Type == EJson::String)
    {
        return Value->AsString();
    }
    if (Value->Type == EJson::Boolean)
    {
        return Value->AsBool() ? TEXT("true") : TEXT("false");
    }
    if (Value->Type == EJson::Number)
    {
        if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int ||
            PinType.PinCategory == UEdGraphSchema_K2::PC_Int64 ||
            PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
        {
            return FString::Printf(TEXT("%lld"), static_cast<int64>(Value->AsNumber()));
        }
        return FString::SanitizeFloat(Value->AsNumber());
    }

    return GetPinDefaultString(Value, nullptr);
}
bool ApplyPinDefaultValueChecked(UEdGraphPin* Pin, const TSharedPtr<FJsonValue>& Value, const UEdGraphSchema_K2* K2Schema, FString& OutAppliedValue, FString& OutError)
{
    OutAppliedValue.Reset();
    OutError.Reset();

    if (!Pin || !Value.IsValid() || !K2Schema)
    {
        OutError = TEXT("Invalid pin default request");
        return false;
    }

    if(!PinKnown(Pin)||!Hold(K2Schema)||!Hold(Pin->DefaultObject)) { OutError=CurrentResult->Error;return false; }const FEdGraphPinReference Ref(Pin);
    const FString PreviousDefaultValue = Pin->DefaultValue;
    UObject* PreviousDefaultObject = Pin->DefaultObject;
    const FText PreviousDefaultTextValue = Pin->DefaultTextValue;
    FString RequestedDefaultValue;
    UObject* RequestedDefaultObject = nullptr;
    bool bRequestedDefaultValue = false;
    bool bRequestedDefaultObject = false;

    if (Value->Type == EJson::String && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
    {
        if (IsMCPDependencyReference(Value->AsString()))
        {
            OutError = GetMCPDependencyReferenceError(Pin->PinName.ToString(), Value->AsString());
            return false;
        }

        UClass* Class = LoadClassForPin(Value->AsString());
        if (!Class)
        {
            OutError = FString::Printf(TEXT("Class not found: %s"), *Value->AsString());
            return false;
        }
        if(!AfterDefaultSetter(Pin,Ref)||!Hold(K2Schema)||SelectedGraph->GetSchema()!=K2Schema) { OutError=CurrentResult->Error;return false; }
        ExpectedDefaultObjects.Add(Pin->GetFName(),Class);
        RequestedDefaultObject = Class;
        bRequestedDefaultObject = true;
        WriteIntent(TEXT("checked_default_setter_calls"));K2Schema->TrySetDefaultObject(*Pin, Class);
        if(!AfterDefaultSetter(Pin,Ref)) { OutError=CurrentResult->Error;return false; }
        OutAppliedValue = Class->GetPathName();
    }
    else if (Value->Type == EJson::String && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
    {
        if (IsMCPDependencyReference(Value->AsString()))
        {
            OutError = GetMCPDependencyReferenceError(Pin->PinName.ToString(), Value->AsString());
            return false;
        }

        UObject* Object = LoadObjectForPin(Value->AsString());
        if (!Object)
        {
            OutError = FString::Printf(TEXT("Object not found: %s"), *Value->AsString());
            return false;
        }
        if(!AfterDefaultSetter(Pin,Ref)||!Hold(K2Schema)||SelectedGraph->GetSchema()!=K2Schema) { OutError=CurrentResult->Error;return false; }
        ExpectedDefaultObjects.Add(Pin->GetFName(),Object);
        RequestedDefaultObject = Object;
        bRequestedDefaultObject = true;
        WriteIntent(TEXT("checked_default_setter_calls"));K2Schema->TrySetDefaultObject(*Pin, Object);
        if(!AfterDefaultSetter(Pin,Ref)) { OutError=CurrentResult->Error;return false; }
        OutAppliedValue = Object->GetPathName();
    }
    else
    {
        FString DefaultResolveError;
        if (!GetPinDefaultStringForTypeChecked(Value, Pin->PinType, RequestedDefaultValue, DefaultResolveError))
        {
            OutError = DefaultResolveError;
            return false;
        }
        bRequestedDefaultValue = true;
        WriteIntent(TEXT("checked_default_setter_calls"));K2Schema->TrySetDefaultValue(*Pin, RequestedDefaultValue);
        if(!AfterDefaultSetter(Pin,Ref)) { OutError=CurrentResult->Error;return false; }
        OutAppliedValue = RequestedDefaultValue;
    }

    Count(*CurrentResult,TEXT("default_validity_queries"));const FString DefaultError = K2Schema->IsCurrentPinDefaultValid(Pin);
    const bool bDefaultValueApplied = !bRequestedDefaultValue || K2Schema->DoesDefaultValueMatch(*Pin, RequestedDefaultValue);
    const bool bDefaultObjectApplied = !bRequestedDefaultObject || Pin->DefaultObject == RequestedDefaultObject;
    if (!DefaultError.IsEmpty() || !bDefaultValueApplied || !bDefaultObjectApplied)
    {
        if(!AfterDefaultSetter(Pin,Ref)||!Hold(PreviousDefaultObject)) { OutError=CurrentResult->Error;return false; }Count(*CurrentResult,TEXT("original_default_restore_field_writes"),3);
        Pin->DefaultValue = PreviousDefaultValue;
        Pin->DefaultObject = PreviousDefaultObject;
        Pin->DefaultTextValue = PreviousDefaultTextValue;
        OutError = DefaultError.IsEmpty()
            ? FString::Printf(TEXT("Requested default was not accepted. requested='%s', actual='%s'"), *RequestedDefaultValue, *Pin->GetDefaultAsString())
            : DefaultError;
        return false;
    }

    return true;
}
TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vector.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Z));
    return Values;
}
TSharedPtr<FJsonValue> VectorToJsonValue(const FVector& Vector)
{
    return MakeShared<FJsonValueArray>(VectorToJsonArray(Vector));
}
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
bool TryParseVectorDefaultString(const FString& DefaultString, FVector& OutVector)
{
    const FString Trimmed = DefaultString.TrimStartAndEnd().TrimChar(TEXT('(')).TrimChar(TEXT(')')).TrimStartAndEnd();

    FVector VectorValue = FVector::ZeroVector;
    if (VectorValue.InitFromString(Trimmed))
    {
        OutVector = VectorValue;
        return true;
    }

    TArray<FString> Parts;
    Trimmed.ParseIntoArray(Parts, TEXT(","), true);
    if (Parts.Num() != 3)
    {
        return false;
    }

    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!LexTryParseString(X, *Parts[0].TrimStartAndEnd()) ||
        !LexTryParseString(Y, *Parts[1].TrimStartAndEnd()) ||
        !LexTryParseString(Z, *Parts[2].TrimStartAndEnd()))
    {
        return false;
    }

    OutVector = FVector(X, Y, Z);
    return true;
}
bool TryParseBoolDefaultString(const FString& DefaultString, bool& bOutValue)
{
    const FString Trimmed = DefaultString.TrimStartAndEnd();
    if (Trimmed.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Trimmed == TEXT("1"))
    {
        bOutValue = true;
        return true;
    }
    if (Trimmed.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Trimmed == TEXT("0"))
    {
        bOutValue = false;
        return true;
    }
    return false;
}
bool PinDefaultSemanticallyMatchesJsonValue(
    UEdGraphPin* Pin,
    const TSharedPtr<FJsonValue>& Value,
    const UEdGraphSchema_K2* K2Schema,
    FString& OutRequestedDefaultValue)
{
    OutRequestedDefaultValue.Reset();if(Pin&&!PinKnown(Pin)) { return false; }
    if (!Pin || !Value.IsValid())
    {
        return false;
    }

    if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
        Pin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
    {
        FVector RequestedVector = FVector::ZeroVector;
        FVector CurrentVector = FVector::ZeroVector;
        if (JsonValueToVector(Value, RequestedVector) &&
            TryParseVectorDefaultString(Pin->GetDefaultAsString(), CurrentVector))
        {
            OutRequestedDefaultValue = FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),
                *FString::SanitizeFloat(RequestedVector.X),
                *FString::SanitizeFloat(RequestedVector.Y),
                *FString::SanitizeFloat(RequestedVector.Z));
            return CurrentVector.Equals(RequestedVector, KINDA_SMALL_NUMBER);
        }
    }

    if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean && Value->Type == EJson::Boolean)
    {
        bool bCurrentValue = false;
        if (TryParseBoolDefaultString(Pin->GetDefaultAsString(), bCurrentValue))
        {
            OutRequestedDefaultValue = Value->AsBool() ? TEXT("true") : TEXT("false");
            return bCurrentValue == Value->AsBool();
        }
    }

    if (K2Schema)
    {
        FString DefaultResolveError;
        if (GetPinDefaultStringForTypeChecked(Value, Pin->PinType, OutRequestedDefaultValue, DefaultResolveError))
        {
            return K2Schema->DoesDefaultValueMatch(*Pin, OutRequestedDefaultValue);
        }
    }

    return false;
}
struct FControlRigInputDefaultRequest
{
    FName PropertyName;
    TSharedPtr<FJsonValue> Value;
};

void UpsertControlRigInputDefault(TArray<FControlRigInputDefaultRequest>& Requests, const FName& PropertyName, const TSharedPtr<FJsonValue>& Value)
{
    if (PropertyName.IsNone() || !Value.IsValid())
    {
        return;
    }

    for (FControlRigInputDefaultRequest& Request : Requests)
    {
        if (Request.PropertyName == PropertyName)
        {
            Request.Value = Value;
            return;
        }
    }

    FControlRigInputDefaultRequest Request;
    Request.PropertyName = PropertyName;
    Request.Value = Value;
    Requests.Add(Request);
}

TArray<FControlRigInputDefaultRequest> BuildDefaultControlRigInputDefaults()
{
    TArray<FControlRigInputDefaultRequest> Requests;
    UpsertControlRigInputDefault(Requests, FName(TEXT("ShouldDoIKTrace")), MakeShared<FJsonValueBoolean>(true));
    UpsertControlRigInputDefault(Requests, FName(TEXT("InteractionWorldLocation")), VectorToJsonValue(FVector(80.0, -40.0, 80.0)));
    return Requests;
}

void SortControlRigInputDefaults(TArray<FControlRigInputDefaultRequest>& Requests)
{
    Requests.Sort([](const FControlRigInputDefaultRequest& A, const FControlRigInputDefaultRequest& B)
    {
        return A.PropertyName.LexicalLess(B.PropertyName);
    });
}

void UpsertControlRigInputName(TArray<FName>& Names, const FName& InputName)
{
    if (InputName.IsNone())
    {
        return;
    }

    for (const FName& ExistingName : Names)
    {
        if (ExistingName == InputName)
        {
            return;
        }
    }

    Names.Add(InputName);
}

TArray<FControlRigInputDefaultRequest> ParseControlRigInputDefaults(const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    OutError.Reset();
    if (!Params.IsValid())
    {
        return BuildDefaultControlRigInputDefaults();
    }

    TArray<FControlRigInputDefaultRequest> Requests;
    const TSharedPtr<FJsonObject>* DefaultsObject = nullptr;
    if (Params->TryGetObjectField(TEXT("input_defaults"), DefaultsObject) || Params->TryGetObjectField(TEXT("defaults"), DefaultsObject))
    {
        if((*DefaultsObject)->Values.Num()>1024) { OutError=TEXT("Default map exceeds1024");return {}; }
        for (const auto& Pair : (*DefaultsObject)->Values)
        {
            const FString Key(Pair.Key.Len(),*Pair.Key);if(!SafeText(Key,NAME_SIZE-1)) { OutError=TEXT("Default property name outside safe FName/NUL scope");return {}; }
            if (!Pair.Value.IsValid())
            {
                OutError = FString::Printf(TEXT("Input default '%s' has an invalid value"), *Key);
                return TArray<FControlRigInputDefaultRequest>();
            }
            UpsertControlRigInputDefault(Requests, FName(*Key), Pair.Value);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* DefaultsArray = nullptr;
    if (Params->TryGetArrayField(TEXT("input_defaults"), DefaultsArray) || Params->TryGetArrayField(TEXT("defaults"), DefaultsArray))
    {
        if(DefaultsArray->Num()>1024) { OutError=TEXT("Default array exceeds1024");return {}; }
        for (const TSharedPtr<FJsonValue>& Value : *DefaultsArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                OutError = TEXT("Input default array entries must be objects with 'name' and 'value'");
                return TArray<FControlRigInputDefaultRequest>();
            }

            const TSharedPtr<FJsonObject> EntryObject = Value->AsObject();
            FString NameText;
            if (!EntryObject->TryGetStringField(TEXT("name"), NameText) || NameText.TrimStartAndEnd().IsEmpty())
            {
                OutError = TEXT("Input default array entry is missing non-empty 'name'");
                return TArray<FControlRigInputDefaultRequest>();
            }

            TSharedPtr<FJsonValue> EntryValue = EntryObject->TryGetField(TEXT("value"));
            if (!EntryValue.IsValid())
            {
                OutError = FString::Printf(TEXT("Input default '%s' is missing 'value'"), *NameText);
                return TArray<FControlRigInputDefaultRequest>();
            }

            if(!SafeText(NameText.TrimStartAndEnd(),NAME_SIZE-1)) { OutError=TEXT("Default property name outside safe FName/NUL scope");return {}; }
            UpsertControlRigInputDefault(Requests, FName(*NameText.TrimStartAndEnd()), EntryValue);
        }
    }

    const bool bUseBuiltInDefaults =
        !Params->HasField(TEXT("input_defaults")) &&
        !Params->HasField(TEXT("defaults")) &&
        !Params->HasField(TEXT("expose_input_pins"));
    if (Requests.Num() == 0 && bUseBuiltInDefaults)
    {
        Requests = BuildDefaultControlRigInputDefaults();
    }

    SortControlRigInputDefaults(Requests);
    return Requests;
}

TArray<FName> ParseControlRigExposeInputPins(const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    OutError.Reset();
    TArray<FName> Names;
    if (!Params.IsValid())
    {
        return Names;
    }

    const TArray<TSharedPtr<FJsonValue>>* PinValues = nullptr;
    if (!Params->TryGetArrayField(TEXT("expose_input_pins"), PinValues) || !PinValues)
    {
        return Names;
    }

    constexpr int32 MaxExposeInputPins = 32;
    if (PinValues->Num() > MaxExposeInputPins)
    {
        OutError = FString::Printf(TEXT("expose_input_pins is limited to %d entries"), MaxExposeInputPins);
        return TArray<FName>();
    }

    for (const TSharedPtr<FJsonValue>& Value : *PinValues)
    {
        if (!Value.IsValid() || Value->Type != EJson::String)
        {
            OutError = TEXT("expose_input_pins entries must be non-empty strings");
            return TArray<FName>();
        }

        const FString NameText = Value->AsString().TrimStartAndEnd();
        if (NameText.IsEmpty())
        {
            OutError = TEXT("expose_input_pins entries must be non-empty strings");
            return TArray<FName>();
        }

        if(!SafeText(NameText,NAME_SIZE-1)) { OutError=TEXT("Expose name outside safe FName/NUL scope");return {}; }
        UpsertControlRigInputName(Names, FName(*NameText));
    }

    return Names;
}

TSharedPtr<FJsonObject> ControlRigInputDefaultsToJsonObject(const TArray<FControlRigInputDefaultRequest>& Requests)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    for (const FControlRigInputDefaultRequest& Request : Requests)
    {
        Object->SetField(Request.PropertyName.ToString(), Request.Value);
    }
    return Object;
}





TArray<FName> GetControlRigCustomPinNamesByReflection(UAnimGraphNode_ControlRig* ControlRigNode)
{
    TArray<FName> Names;
    if (!ControlRigNode)
    {
        return Names;
    }

    if(!CustomPinsSafe(ControlRigNode)) { return Names; }
    FArrayProperty* CustomPinPropertiesProperty = FindFProperty<FArrayProperty>(ControlRigNode->GetClass(), TEXT("CustomPinProperties"));
    FStructProperty* OptionalPinStructProperty = CustomPinPropertiesProperty ? CastField<FStructProperty>(CustomPinPropertiesProperty->Inner) : nullptr;
    if (!CustomPinPropertiesProperty || !OptionalPinStructProperty)
    {
        return Names;
    }

    FNameProperty* PropertyNameProperty = FindFProperty<FNameProperty>(OptionalPinStructProperty->Struct, TEXT("PropertyName"));
    if (!PropertyNameProperty)
    {
        return Names;
    }

    FScriptArrayHelper ArrayHelper(CustomPinPropertiesProperty, CustomPinPropertiesProperty->ContainerPtrToValuePtr<void>(ControlRigNode));
    for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
    {
        const void* EntryPtr = ArrayHelper.GetRawPtr(Index);
        const FName PropertyName = PropertyNameProperty->GetPropertyValue_InContainer(EntryPtr);
        if (!PropertyName.IsNone())
        {
            Names.Add(PropertyName);
        }
    }

    Names.Sort([](const FName& A, const FName& B)
    {
        return A.LexicalLess(B);
    });
    return Names;
}

TArray<TSharedPtr<FJsonValue>> NamesToJsonArray(const TArray<FName>& Names)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FName& Name : Names)
    {
        Values.Add(MakeShared<FJsonValueString>(Name.ToString()));
    }
    return Values;
}

bool SetControlRigCustomPinVisibleByReflection(UAnimGraphNode_ControlRig* ControlRigNode, const FName& PropertyName, bool bVisible, bool& bOutFound, bool& bOutChanged, FString& OutError)
{
    bOutFound = false;
    bOutChanged = false;
    OutError.Reset();

    if (!ControlRigNode)
    {
        OutError = TEXT("ControlRig node is null");
        return false;
    }

    if(!CustomPinsSafe(ControlRigNode)) { OutError=CurrentResult->Error;return false; }
    FArrayProperty* CustomPinPropertiesProperty = FindFProperty<FArrayProperty>(ControlRigNode->GetClass(), TEXT("CustomPinProperties"));
    FStructProperty* OptionalPinStructProperty = CustomPinPropertiesProperty ? CastField<FStructProperty>(CustomPinPropertiesProperty->Inner) : nullptr;
    if (!CustomPinPropertiesProperty || !OptionalPinStructProperty)
    {
        OutError = TEXT("Failed to resolve ControlRig CustomPinProperties");
        return false;
    }

    FNameProperty* PropertyNameProperty = FindFProperty<FNameProperty>(OptionalPinStructProperty->Struct, TEXT("PropertyName"));
    FBoolProperty* ShowPinProperty = FindFProperty<FBoolProperty>(OptionalPinStructProperty->Struct, TEXT("bShowPin"));
    if (!PropertyNameProperty || !ShowPinProperty)
    {
        OutError = TEXT("Failed to resolve ControlRig optional pin fields");
        return false;
    }

    FScriptArrayHelper ArrayHelper(CustomPinPropertiesProperty, CustomPinPropertiesProperty->ContainerPtrToValuePtr<void>(ControlRigNode));
    for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
    {
        void* EntryPtr = ArrayHelper.GetRawPtr(Index);
        const FName EntryPropertyName = PropertyNameProperty->GetPropertyValue_InContainer(EntryPtr);
        if (EntryPropertyName == PropertyName)
        {
            bOutFound = true;
            const bool bCurrentVisible = ShowPinProperty->GetPropertyValue_InContainer(EntryPtr);
            if (bCurrentVisible != bVisible)
            {
                WriteIntent(TEXT("optional_pin_visibility_writes"));ShowPinProperty->SetPropertyValue_InContainer(EntryPtr, bVisible);
                bOutChanged = true;
            }
            return true;
        }
    }

    return true;
}

UEdGraphPin* FindControlRigInputPin(UAnimGraphNode_ControlRig* ControlRigNode, const FName& PropertyName)
{
    if (!ControlRigNode)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : ControlRigNode->Pins)
    {
        if(Pin&&!PinKnown(Pin)) { return nullptr; }
        if (!Pin || Pin->Direction != EGPD_Input || Pin->ParentPin || IsPoseLinkPin(Pin)||IsComponentPoseLinkPin(Pin))
        {
            continue;
        }

        if (Pin->GetFName() == PropertyName || Pin->PinFriendlyName.ToString().Equals(PropertyName.ToString(), ESearchCase::CaseSensitive))
        {
            return Pin;
        }
    }

    return nullptr;
}







UAnimGraphNode_ControlRig* ResolveControlRigNodeForInputDefaults(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params, int32& OutCandidateCount, FString& OutError)
{
    OutCandidateCount = 0;
    OutError.Reset();
    if (!Graph)
    {
        OutError = TEXT("Graph is null");
        return nullptr;
    }

    FString NodeId;
    FString NodeName;
    FString TitleContains;
    FString ControlRigClassFilter;
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("node_id"), NodeId);
        Params->TryGetStringField(TEXT("node_name"), NodeName);
        Params->TryGetStringField(TEXT("title_contains"), TitleContains);
        Params->TryGetStringField(TEXT("control_rig_class"), ControlRigClassFilter);
    }

    TArray<UAnimGraphNode_ControlRig*> Candidates;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if(Node&&!Hold(Node)) { return nullptr; }
        UAnimGraphNode_ControlRig* ControlRigNode = Cast<UAnimGraphNode_ControlRig>(Node);
        if (!ControlRigNode)
        {
            continue;
        }

        ++OutCandidateCount;

        if (!NodeId.IsEmpty() &&
            !ControlRigNode->NodeGuid.ToString().Equals(NodeId, ESearchCase::IgnoreCase) &&
            !ControlRigNode->GetName().Equals(NodeId, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (!NodeName.IsEmpty() && !ControlRigNode->GetName().Equals(NodeName, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (!TitleContains.IsEmpty() && !static_cast<UEdGraphNode*>(ControlRigNode)->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(TitleContains))
        {
            continue;
        }
        if (!ControlRigClassFilter.IsEmpty())
        {
            const FString ClassPath = GetControlRigClassPathFromNode(ControlRigNode);
            if (!ClassPath.Equals(ControlRigClassFilter, ESearchCase::IgnoreCase) &&
                !ClassPath.Contains(ControlRigClassFilter))
            {
                continue;
            }
        }

        Candidates.Add(ControlRigNode);
    }

    if (Candidates.Num() == 0)
    {
        OutError = OutCandidateCount == 0
            ? FString::Printf(TEXT("No ControlRig nodes found in graph '%s'"), *Graph->GetName())
            : FString::Printf(TEXT("No ControlRig node matched the supplied filters in graph '%s'"), *Graph->GetName());
        return nullptr;
    }

    return Candidates[0];
}

UAnimGraphNode_ModifyCurve* FindModifyCurveFeedingControlRig(UAnimGraphNode_ControlRig* ControlRigNode)
{
    if (!ControlRigNode)
    {
        return nullptr;
    }

    UEdGraphPin* ControlRigSourceInput = FindFirstPosePin(ControlRigNode, EGPD_Input, { TEXT("Source") });
    if (!ControlRigSourceInput)
    {
        return nullptr;
    }

    for (UEdGraphPin* LinkedPin : ControlRigSourceInput->LinkedTo)
    {
        if(LinkedPin&&!PinKnown(LinkedPin)) { return nullptr; }
        if (LinkedPin && LinkedPin->GetOwningNode())
        {
            if (UAnimGraphNode_ModifyCurve* ModifyCurveNode = Cast<UAnimGraphNode_ModifyCurve>(LinkedPin->GetOwningNode()))
            {
                return ModifyCurveNode;
            }
        }
    }

    return nullptr;
}
TSharedPtr<FJsonObject> Capture(UBlueprint* BP)
{
    Count(*CurrentResult,TEXT("state_capture_calls"));auto J=MakeShared<FJsonObject>();if(!BP||!Hold(BP)||!Hold(BP->ParentClass.Get())||!Hold(BP->GeneratedClass.Get())||!Hold(BP->SkeletonGeneratedClass.Get())||!RefreshInventory()) { return J; }
    PropertyValues=0;J->SetStringField(TEXT("blueprint"),BP->GetPathName());J->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());J->SetStringField(TEXT("parent_class"),GetPathNameSafe(BP->ParentClass.Get()));J->SetStringField(TEXT("generated_class"),GetPathNameSafe(BP->GeneratedClass.Get()));J->SetStringField(TEXT("skeleton_class"),GetPathNameSafe(BP->SkeletonGeneratedClass.Get()));
    TArray<UEdGraph*> Graphs;if(!Enumerate(BP,Graphs)) { return J; }TArray<TSharedPtr<FJsonValue>> GraphRows;
    for(UEdGraph* G:Graphs)
    {
        auto Row=GraphToJson(BP,G);Row->SetStringField(TEXT("path"),G->GetPathName());Row->SetBoolField(TEXT("editable"),G->bEditable);TArray<TSharedPtr<FJsonValue>> Nodes;TSet<FGuid> Guids;
        for(UEdGraphNode* Node:G->Nodes)
        {
            if(!Node) { continue; }if(!Hold(Node)||!Node->NodeGuid.IsValid()||Guids.Contains(Node->NodeGuid)) { NoteUnsafe(TEXT("Node GUID identity missing/duplicate"));return J; }Guids.Add(Node->NodeGuid);auto Entry=NodeToJson(Node,true);Entry->SetStringField(TEXT("path"),Node->GetPathName());if(IsAnimNode(Node)) { Entry->SetObjectField(TEXT("settings"),AnimGraphNodeSettingsToJson(Node,4)); }Nodes.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Row->SetArrayField(TEXT("nodes"),Nodes);GraphRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("graphs"),GraphRows);TArray<TSharedPtr<FJsonValue>> Variables;if(BP->NewVariables.Num()>1024) { NoteUnsafe(TEXT("Member descriptor count exceeds1024"));return J; }
    for(const FBPVariableDescription& V:BP->NewVariables)
    {
        auto X=MakeShared<FJsonObject>();X->SetStringField(TEXT("name"),V.VarName.ToString());X->SetStringField(TEXT("guid"),V.VarGuid.ToString());X->SetObjectField(TEXT("type"),PinTypeToJson(V.VarType));X->SetStringField(TEXT("default"),V.DefaultValue);X->SetStringField(TEXT("friendly"),V.FriendlyName);X->SetStringField(TEXT("category"),V.Category.ToString());X->SetStringField(TEXT("flags"),LexToString(V.PropertyFlags));X->SetStringField(TEXT("rep_notify"),V.RepNotifyFunc.ToString());X->SetNumberField(TEXT("rep_condition"),V.ReplicationCondition.GetValue());TArray<TSharedPtr<FJsonValue>> Metadata;if(V.MetaDataArray.Num()>1024) { NoteUnsafe(TEXT("Member metadata count exceeds1024"));return J; }for(const auto& M:V.MetaDataArray) { auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("key"),M.DataKey.ToString());Meta->SetStringField(TEXT("value"),M.DataValue);Metadata.Add(MakeShared<FJsonValueObject>(Meta)); }X->SetArrayField(TEXT("metadata"),Metadata);Variables.Add(MakeShared<FJsonValueObject>(X));
    }
    J->SetArrayField(TEXT("member_variables"),Variables);return J;
}
bool EqualJson(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{ auto Left=MakeShared<FJsonObject>();auto Right=MakeShared<FJsonObject>();Left->SetField(TEXT("value"),A);Right->SetField(TEXT("value"),B);return Serialize(Left).Equals(Serialize(Right),ESearchCase::CaseSensitive)&&!Incomplete(); }
void AddAllowed(UEdGraphNode* Node,TSet<FString>& Allowed)
{ if(!Loaded(Node)) { return; }Allowed.Add(Node->NodeGuid.ToString());for(UEdGraphPin* P:Node->Pins) { if(!PinKnown(P)) { continue; }for(UEdGraphPin* L:P->LinkedTo) { if(PinKnown(L)) { Allowed.Add(L->GetOwningNodeUnchecked()->NodeGuid.ToString()); } } } }
bool CurrentClassesSafe(UBlueprint* BP)
{
    if(!BP||!Hold(BP)||!Hold(BP->ParentClass.Get())) { return false; }
    for(UClass* Class:{BP->GeneratedClass.Get(),BP->SkeletonGeneratedClass.Get()})
    { if(Class&&(!Hold(Class)||Class->ClassGeneratedBy!=BP||!Class->IsChildOf(BP->ParentClass.Get()))) { NoteUnsafe(TEXT("Current generated/skeleton class owner or parent lineage mismatch"));return false; } }
    return true;
}
bool Preserve(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,UEdGraph* Graph,const TSet<FString>& Allowed)
{
    if(!EqualJson(Before->TryGetField(TEXT("member_variables")),After->TryGetField(TEXT("member_variables")))) { return false; }
    for(const TCHAR* K:{TEXT("blueprint"),TEXT("parent_class")}) { if(!EqualJson(Before->TryGetField(K),After->TryGetField(K))) { return false; } }
    CurrentResult->Data->SetBoolField(TEXT("generated_class_path_preserved"),EqualJson(Before->TryGetField(TEXT("generated_class")),After->TryGetField(TEXT("generated_class"))));
    CurrentResult->Data->SetBoolField(TEXT("skeleton_class_path_preserved"),EqualJson(Before->TryGetField(TEXT("skeleton_class")),After->TryGetField(TEXT("skeleton_class"))));
    if(!CurrentClassesSafe(CurrentBlueprint)) { return false; }
    const auto& A=Before->GetArrayField(TEXT("graphs"));const auto& B=After->GetArrayField(TEXT("graphs"));if(A.Num()!=B.Num()) { return false; }
    FString Root;CurrentResult->Data->TryGetStringField(TEXT("selected_root_guid"),Root);TSet<FString> Added;
    if(const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;CurrentResult->Data->TryGetArrayField(TEXT("new_node_receipts"),Rows))
    { for(const auto& V:*Rows) { FString Id;if(V->AsObject()->TryGetStringField(TEXT("node_id"),Id)) { Added.Add(Id); } } }
    for(int32 I=0;I<A.Num();++I)
    {
        const auto Old=A[I]->AsObject();const auto Now=B[I]->AsObject();
        if(Old->GetStringField(TEXT("path"))!=Graph->GetPathName()) { if(!EqualJson(A[I],B[I])) { return false; }continue; }
        for(const TCHAR* K:{TEXT("path"),TEXT("graph_id"),TEXT("graph_name"),TEXT("graph_type"),TEXT("schema"),TEXT("editable")}) { if(!EqualJson(Old->TryGetField(K),Now->TryGetField(K))) { return false; } }
        const auto& O=Old->GetArrayField(TEXT("nodes"));const auto& N=Now->GetArrayField(TEXT("nodes"));
        for(const auto& V:N)
        { const auto Id=V->AsObject()->GetStringField(TEXT("node_id"));if(!O.ContainsByPredicate([&](const TSharedPtr<FJsonValue>& X){return X->AsObject()->GetStringField(TEXT("node_id"))==Id;})&&!Added.Contains(Id)) { return false; } }
        for(const auto& V:O)
        {
            const auto Row=V->AsObject();const auto Id=Row->GetStringField(TEXT("node_id"));const auto* Found=N.FindByPredicate([&](const TSharedPtr<FJsonValue>& X){return X->AsObject()->GetStringField(TEXT("node_id"))==Id;});if(!Found) { return false; }const auto Current=(*Found)->AsObject();
            for(const TCHAR* K:{TEXT("node_id"),TEXT("name"),TEXT("class"),TEXT("path"),TEXT("node_comment"),TEXT("comment_bubble_visible"),TEXT("comment_bubble_pinned")}) { if(!EqualJson(Row->TryGetField(K),Current->TryGetField(K))) { return false; } }
            if(Id==Root) { if(Current->GetNumberField(TEXT("x"))!=0||Current->GetNumberField(TEXT("y"))!=0) { return false; } }
            else if(!EqualJson(Row->TryGetField(TEXT("x")),Current->TryGetField(TEXT("x")))||!EqualJson(Row->TryGetField(TEXT("y")),Current->TryGetField(TEXT("y")))) { return false; }

            if(!Allowed.Contains(Id)&&!EqualJson(V,*Found)) { return false; }
            if(Allowed.Contains(Id)&&Id!=SelectedRig->NodeGuid.ToString()&&(!SelectedCurve||Id!=SelectedCurve->NodeGuid.ToString()))
            {
                if(Row->Values.Num()!=Current->Values.Num()) { return false; }for(const auto& Field:Row->Values) { const FString Key(Field.Key.Len(),*Field.Key);if(Key!=TEXT("pins")&&!EqualJson(Field.Value,Current->TryGetField(Key))) { return false; } }
                const auto& OldPins=Row->GetArrayField(TEXT("pins"));const auto& NewPins=Current->GetArrayField(TEXT("pins"));if(OldPins.Num()!=NewPins.Num()) { return false; }
                for(int32 PinIndex=0;PinIndex<OldPins.Num();++PinIndex) { const auto OldPin=OldPins[PinIndex]->AsObject();const auto NewPin=NewPins[PinIndex]->AsObject();if(OldPin->Values.Num()!=NewPin->Values.Num()) { return false; }for(const auto& Field:OldPin->Values) { const FString Key(Field.Key.Len(),*Field.Key);if(Key!=TEXT("linked_to")&&!EqualJson(Field.Value,NewPin->TryGetField(Key))) { return false; } } }
            }
        }
    }return !Incomplete();
}
TSharedPtr<FJsonObject> RunSet(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString DefaultsError;
    const TArray<FControlRigInputDefaultRequest> DefaultRequests = ParseControlRigInputDefaults(Params, DefaultsError);
    if (!DefaultsError.IsEmpty())
    {
        return CreateErrorResponse(DefaultsError);
    }

    FString ExposeError;
    const TArray<FName> ExplicitExposeInputNames = ParseControlRigExposeInputPins(Params, ExposeError);
    if (!ExposeError.IsEmpty())
    {
        return CreateErrorResponse(ExposeError);
    }

    if (DefaultRequests.Num() == 0 && ExplicitExposeInputNames.Num() == 0)
    {
        return CreateErrorResponse(TEXT("At least one ControlRig input default or expose_input_pins entry is required"));
    }

    bool bDisconnectExistingLinks = false;
    Params->TryGetBoolField(TEXT("disconnect_existing_links"), bDisconnectExistingLinks);

    UBlueprint* Blueprint = FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    bool bAllowNonSample = false;
    Params->TryGetBoolField(TEXT("allow_non_sample"), bAllowNonSample);
    if (!bAllowNonSample && !Blueprint->GetPathName().StartsWith(TEXT("/Game/_MCP_Sample/")))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Refusing to modify non-sample Anim Blueprint '%s'. Pass allow_non_sample=true only for intentional non-sample edits."),
            *Blueprint->GetPathName()));
    }

    FString GraphError;
    UEdGraph* TargetGraph = ResolveBlueprintGraphForNodeCommand(Blueprint, Params, GraphError);
    if (!TargetGraph)
    {
        return CreateErrorResponse(GraphError);
    }

    if (!IsAnimationGraph(TargetGraph))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Target graph is not an AnimationGraph. graph='%s' schema='%s'"),
            *TargetGraph->GetName(),
            TargetGraph->GetSchema() ? *TargetGraph->GetSchema()->GetClass()->GetName() : TEXT("<none>")));
    }

    const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema());
    if (!K2Schema)
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Target graph schema is not K2-compatible: %s"),
            TargetGraph->GetSchema() ? *TargetGraph->GetSchema()->GetClass()->GetName() : TEXT("<none>")));
    }

    int32 ControlRigCandidateCount = 0;
    FString ResolveError;
    UAnimGraphNode_ControlRig* ControlRigNode = ResolveControlRigNodeForInputDefaults(TargetGraph, Params, ControlRigCandidateCount, ResolveError);
    if (!ControlRigNode)
    {
        return CreateErrorResponse(ResolveError);
    }

    if(!ReadySelected(ControlRigNode,TargetGraph)) { return Failure(CurrentResult->Error); }
    ModifyNode(ControlRigNode);
    if(!ReconstructRig(ControlRigNode)) { return Failure(CurrentResult->Error); }

    TArray<FName> AvailableInputNames = GetControlRigCustomPinNamesByReflection(ControlRigNode);
    bool bVisibilityChanged = false;
    TArray<TSharedPtr<FJsonValue>> PerInputResults;
    TArray<TSharedPtr<FJsonValue>> ExposedInputResults;
    TArray<FString> ChangedFields;

    TArray<FName> InputNamesToExpose;
    for (const FControlRigInputDefaultRequest& Request : DefaultRequests)
    {
        UpsertControlRigInputName(InputNamesToExpose, Request.PropertyName);
    }
    for (const FName& InputName : ExplicitExposeInputNames)
    {
        UpsertControlRigInputName(InputNamesToExpose, InputName);
    }

    for (const FName& InputName : InputNamesToExpose)
    {
        bool bFound = false;
        bool bChangedThisPinVisibility = false;
        FString VisibilityError;
        if (!SetControlRigCustomPinVisibleByReflection(ControlRigNode, InputName, true, bFound, bChangedThisPinVisibility, VisibilityError))
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(VisibilityError);
            const bool bPartialTargetModified = bVisibilityChanged || bChangedThisPinVisibility;
            ErrorObj->SetBoolField(TEXT("visibility_changed"), bPartialTargetModified);
            ErrorObj->SetBoolField(TEXT("graph_changed"), bPartialTargetModified);
            ErrorObj->SetBoolField(TEXT("target_modified"), bPartialTargetModified);
            ErrorObj->SetBoolField(TEXT("partial_target_modified"), bPartialTargetModified);
            return ErrorObj;
        }

        if (!bFound)
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("ControlRig input '%s' is not available on node '%s'"),
                *InputName.ToString(),
                *ControlRigNode->GetName()));
            ErrorObj->SetArrayField(TEXT("available_input_names"), NamesToJsonArray(AvailableInputNames));
            ErrorObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
            ErrorObj->SetBoolField(TEXT("visibility_changed"), bVisibilityChanged);
            ErrorObj->SetBoolField(TEXT("graph_changed"), bVisibilityChanged);
            ErrorObj->SetBoolField(TEXT("target_modified"), bVisibilityChanged);
            ErrorObj->SetBoolField(TEXT("partial_target_modified"), bVisibilityChanged);
            AddGraphField(ErrorObj, Blueprint, TargetGraph);
            return ErrorObj;
        }

        if (bChangedThisPinVisibility)
        {
            bVisibilityChanged = true;
            ChangedFields.Add(FString::Printf(TEXT("ExposePin:%s"), *InputName.ToString()));
        }

        TSharedPtr<FJsonObject> ExposedInputResult = MakeShared<FJsonObject>();
        ExposedInputResult->SetStringField(TEXT("name"), InputName.ToString());
        ExposedInputResult->SetBoolField(TEXT("pin_found"), true);
        ExposedInputResult->SetBoolField(TEXT("visibility_changed"), bChangedThisPinVisibility);
        ExposedInputResult->SetBoolField(TEXT("already_visible"), !bChangedThisPinVisibility);
        ExposedInputResults.Add(MakeShared<FJsonValueObject>(ExposedInputResult));
    }

    if (bVisibilityChanged)
    {
        if(!ReconstructRig(ControlRigNode)) { return Failure(CurrentResult->Error); }
        AvailableInputNames = GetControlRigCustomPinNamesByReflection(ControlRigNode);
    }

    bool bDefaultChanged = false;
    bool bLinksChanged = false;
    for (const FControlRigInputDefaultRequest& Request : DefaultRequests)
    {
        UEdGraphPin* InputPin = FindControlRigInputPin(ControlRigNode, Request.PropertyName);
        if (!InputPin)
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("ControlRig input pin '%s' was not found after exposing it"),
                *Request.PropertyName.ToString()));
            ErrorObj->SetArrayField(TEXT("available_input_names"), NamesToJsonArray(AvailableInputNames));
            ErrorObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
            const bool bPartialTargetModified = bVisibilityChanged || bDefaultChanged || bLinksChanged;
            ErrorObj->SetBoolField(TEXT("visibility_changed"), bVisibilityChanged);
            ErrorObj->SetBoolField(TEXT("defaults_changed"), bDefaultChanged);
            ErrorObj->SetBoolField(TEXT("links_changed"), bLinksChanged);
            ErrorObj->SetBoolField(TEXT("graph_changed"), bPartialTargetModified);
            ErrorObj->SetBoolField(TEXT("target_modified"), bPartialTargetModified);
            ErrorObj->SetBoolField(TEXT("partial_target_modified"), bPartialTargetModified);
            AddGraphField(ErrorObj, Blueprint, TargetGraph);
            return ErrorObj;
        }

        const int32 LinkedPinCountBefore = InputPin->LinkedTo.Num();
        if (bDisconnectExistingLinks && LinkedPinCountBefore > 0)
        {
            const FEdGraphPinReference BreakRef(InputPin);WriteIntent(TEXT("break_pin_links_calls"));K2Schema->BreakPinLinks(*InputPin, true);
            if(!AfterDefaultSetter(InputPin,BreakRef)) { return Failure(CurrentResult->Error); }
            Count(*CurrentResult,TEXT("pin_connection_changed_calls"));ControlRigNode->PinConnectionListChanged(InputPin);
            if(!AfterDefaultSetter(InputPin,BreakRef)) { return Failure(CurrentResult->Error); }
            bLinksChanged = true;
            ChangedFields.Add(FString::Printf(TEXT("BreakLinks:%s"), *Request.PropertyName.ToString()));
        }

        const FString PreviousDefaultValue = InputPin->GetDefaultAsString();
        FString AppliedValue;
        FString DefaultError;
        FString RequestedDefaultValue;
        if(Request.Value.IsValid()&&Request.Value->Type==EJson::String&&(InputPin->PinType.PinCategory==UEdGraphSchema_K2::PC_Class||InputPin->PinType.PinCategory==UEdGraphSchema_K2::PC_Object)) { if(!Hold(InputPin->DefaultObject)) { return Failure(CurrentResult->Error); }ExpectedDefaultObjects.Add(InputPin->GetFName(),InputPin->DefaultObject.Get()); }
        const bool bDefaultAlreadyMatches = PinDefaultSemanticallyMatchesJsonValue(InputPin, Request.Value, K2Schema, RequestedDefaultValue);
        if (bDefaultAlreadyMatches)
        {
            AppliedValue = RequestedDefaultValue.IsEmpty() ? PreviousDefaultValue : RequestedDefaultValue;
        }
        else if (!ApplyPinDefaultValueChecked(InputPin, Request.Value, K2Schema, AppliedValue, DefaultError))
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("Failed to set ControlRig input '%s' default: %s"),
                *Request.PropertyName.ToString(),
                *DefaultError));
            ErrorObj->SetStringField(TEXT("pin_name"), Request.PropertyName.ToString());
            ErrorObj->SetStringField(TEXT("previous_default_value"), PreviousDefaultValue);
            ErrorObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
            const bool bPartialTargetModified = bVisibilityChanged || bDefaultChanged || bLinksChanged;
            ErrorObj->SetBoolField(TEXT("visibility_changed"), bVisibilityChanged);
            ErrorObj->SetBoolField(TEXT("defaults_changed"), bDefaultChanged);
            ErrorObj->SetBoolField(TEXT("links_changed"), bLinksChanged);
            ErrorObj->SetBoolField(TEXT("graph_changed"), bPartialTargetModified);
            ErrorObj->SetBoolField(TEXT("target_modified"), bPartialTargetModified);
            ErrorObj->SetBoolField(TEXT("partial_target_modified"), bPartialTargetModified);
            AddGraphField(ErrorObj, Blueprint, TargetGraph);
            return ErrorObj;
        }

        if (!bDefaultAlreadyMatches)
        {
            const FEdGraphPinReference DefaultRef(InputPin);Count(*CurrentResult,TEXT("pin_default_changed_calls"));ControlRigNode->PinDefaultValueChanged(InputPin);
            if(!AfterDefaultSetter(InputPin,DefaultRef)) { return Failure(CurrentResult->Error); }
        }
        const FString CurrentDefaultValue = InputPin->GetDefaultAsString();
        const bool bChangedThisDefault = !bDefaultAlreadyMatches && !PreviousDefaultValue.Equals(CurrentDefaultValue, ESearchCase::CaseSensitive);
        if (bChangedThisDefault)
        {
            bDefaultChanged = true;
            ChangedFields.Add(FString::Printf(TEXT("Default:%s"), *Request.PropertyName.ToString()));
        }

        TSharedPtr<FJsonObject> InputResult = MakeShared<FJsonObject>();
        InputResult->SetStringField(TEXT("name"), Request.PropertyName.ToString());
        InputResult->SetField(TEXT("requested_value"), Request.Value);
        InputResult->SetBoolField(TEXT("pin_found"), true);
        InputResult->SetBoolField(TEXT("default_changed"), bChangedThisDefault);
        InputResult->SetBoolField(TEXT("default_already_matched"), bDefaultAlreadyMatches);
        InputResult->SetBoolField(TEXT("pin_linked"), InputPin->LinkedTo.Num() > 0);
        InputResult->SetNumberField(TEXT("linked_pin_count_before"), LinkedPinCountBefore);
        InputResult->SetNumberField(TEXT("linked_pin_count"), InputPin->LinkedTo.Num());
        InputResult->SetBoolField(TEXT("links_disconnected"), bDisconnectExistingLinks && LinkedPinCountBefore > 0);
        InputResult->SetStringField(TEXT("previous_default_value"), PreviousDefaultValue);
        InputResult->SetStringField(TEXT("applied_default_value"), AppliedValue);
        InputResult->SetStringField(TEXT("current_default_value"), CurrentDefaultValue);
        InputResult->SetStringField(TEXT("pin_type_category"), InputPin->PinType.PinCategory.ToString());
        if (InputPin->PinType.PinSubCategoryObject.IsValid())
        {
            InputResult->SetStringField(TEXT("pin_subcategory_object"), InputPin->PinType.PinSubCategoryObject->GetPathName());
        }
        PerInputResults.Add(MakeShared<FJsonValueObject>(InputResult));
    }

    const bool bGraphChanged = bVisibilityChanged || bDefaultChanged || bLinksChanged;
    if (bGraphChanged)
    {
        if (bVisibilityChanged)
        {
            WriteIntent(TEXT("mark_structurally_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);CurrentResult->Data->SetBoolField(TEXT("implicit_skeleton_regeneration_possible"),true);
            if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)) { return Failure(CurrentResult->Error); }
        }
        else
        {
            WriteIntent(TEXT("mark_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)) { return Failure(CurrentResult->Error); }
        }
        Count(*CurrentResult,TEXT("notify_graph_changed_calls"));TargetGraph->NotifyGraphChanged();
        if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)) { return Failure(CurrentResult->Error); }
    }

    TArray<TSharedPtr<FJsonValue>> ChangedFieldValues;
    for (const FString& FieldName : ChangedFields)
    {
        ChangedFieldValues.Add(MakeShared<FJsonValueString>(FieldName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("connected"), true);
    ResultObj->SetBoolField(TEXT("graph_changed"), bGraphChanged);
    ResultObj->SetBoolField(TEXT("visibility_changed"), bVisibilityChanged);
    ResultObj->SetBoolField(TEXT("defaults_changed"), bDefaultChanged);
    ResultObj->SetBoolField(TEXT("links_changed"), bLinksChanged);
    ResultObj->SetBoolField(TEXT("disconnect_existing_links"), bDisconnectExistingLinks);
    ResultObj->SetNumberField(TEXT("controlrig_candidate_count"), ControlRigCandidateCount);
    ResultObj->SetStringField(TEXT("control_rig_class"), GetControlRigClassPathFromNode(ControlRigNode));
    ResultObj->SetObjectField(TEXT("requested_input_defaults"), ControlRigInputDefaultsToJsonObject(DefaultRequests));
    ResultObj->SetArrayField(TEXT("requested_expose_input_pins"), NamesToJsonArray(ExplicitExposeInputNames));
    ResultObj->SetArrayField(TEXT("exposed_input_names"), NamesToJsonArray(InputNamesToExpose));
    ResultObj->SetArrayField(TEXT("exposed_input_results"), ExposedInputResults);
    ResultObj->SetArrayField(TEXT("input_results"), PerInputResults);
    ResultObj->SetArrayField(TEXT("available_input_names"), NamesToJsonArray(AvailableInputNames));
    ResultObj->SetArrayField(TEXT("changed_fields"), ChangedFieldValues);
    ResultObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
    ResultObj->SetObjectField(TEXT("settings"), AnimGraphNodeSettingsToJson(ControlRigNode, 4));
    AddGraphField(ResultObj, Blueprint, TargetGraph);
    return ResultObj;
}
TSharedPtr<FJsonObject> RunForced(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString CurveError;
    const TArray<TPair<FName, float>> CurveEntries = ParseModifyCurveEntries(Params, CurveError);
    if (!CurveError.IsEmpty())
    {
        return CreateErrorResponse(CurveError);
    }
    if (CurveEntries.Num() == 0)
    {
        return CreateErrorResponse(TEXT("At least one curve entry is required"));
    }

    FString DefaultsError;
    const TArray<FControlRigInputDefaultRequest> DefaultRequests = ParseControlRigInputDefaults(Params, DefaultsError);
    if (!DefaultsError.IsEmpty())
    {
        return CreateErrorResponse(DefaultsError);
    }
    if (DefaultRequests.Num() == 0)
    {
        return CreateErrorResponse(TEXT("At least one ControlRig input default is required"));
    }

    float Alpha = 1.0f;
    if (Params->HasField(TEXT("alpha")))
    {
        Alpha = FMath::Clamp(static_cast<float>(Params->GetNumberField(TEXT("alpha"))), 0.0f, 1.0f);
    }

    FString ApplyModeText;
    if (!Params->TryGetStringField(TEXT("apply_mode"), ApplyModeText))
    {
        ApplyModeText = TEXT("Add");
    }
    EModifyCurveApplyMode ApplyMode = EModifyCurveApplyMode::Add;
    if (!ParseModifyCurveApplyMode(ApplyModeText, ApplyMode))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Invalid 'apply_mode': '%s'. Use Add, Scale, Blend, WeightedMovingAverage, or RemapCurve."),
            *ApplyModeText));
    }

    bool bReplaceExisting = true;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

    bool bDisconnectExistingLinks = true;
    Params->TryGetBoolField(TEXT("disconnect_existing_links"), bDisconnectExistingLinks);

    UBlueprint* Blueprint = FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    bool bAllowNonSample = false;
    Params->TryGetBoolField(TEXT("allow_non_sample"), bAllowNonSample);
    if (!bAllowNonSample && !Blueprint->GetPathName().StartsWith(TEXT("/Game/_MCP_Sample/")))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Refusing to modify non-sample Anim Blueprint '%s'. Pass allow_non_sample=true only for intentional non-sample edits."),
            *Blueprint->GetPathName()));
    }

    FString GraphError;
    UEdGraph* TargetGraph = ResolveBlueprintGraphForNodeCommand(Blueprint, Params, GraphError);
    if (!TargetGraph)
    {
        return CreateErrorResponse(GraphError);
    }

    if (!IsAnimationGraph(TargetGraph))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Target graph is not an AnimationGraph. graph='%s' schema='%s'"),
            *TargetGraph->GetName(),
            TargetGraph->GetSchema() ? *TargetGraph->GetSchema()->GetClass()->GetName() : TEXT("<none>")));
    }

    const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema());
    if (!K2Schema)
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Target graph schema is not K2-compatible: %s"),
            TargetGraph->GetSchema() ? *TargetGraph->GetSchema()->GetClass()->GetName() : TEXT("<none>")));
    }

    int32 ControlRigCandidateCount = 0;
    FString ResolveError;
    UAnimGraphNode_ControlRig* ControlRigNode = ResolveControlRigNodeForInputDefaults(TargetGraph, Params, ControlRigCandidateCount, ResolveError);
    if (!ControlRigNode)
    {
        return CreateErrorResponse(ResolveError);
    }

    if(!ReadySelected(ControlRigNode,TargetGraph)) { return Failure(CurrentResult->Error); }
    ModifyNode(ControlRigNode);
    if(!ReconstructRig(ControlRigNode)) { return Failure(CurrentResult->Error); }

    UEdGraphPin* ControlRigSourceInput = FindFirstPosePin(ControlRigNode, EGPD_Input, { TEXT("Source") });
    if (!ControlRigSourceInput)
    {
        return CreateErrorResponse(TEXT("ControlRig Source pose input pin not found"));
    }

    if(ControlRigNode->NodePosX<TNumericLimits<int32>::Min()+260) { NoteUnsafe(TEXT("Derived ModifyCurve X outside int32 scope"));return Failure(CurrentResult->Error); }
    bool bCreatedModifyCurveNode = false;
    bool bCurveInsertedBeforeControlRig = false;
    UAnimGraphNode_ModifyCurve* ModifyCurveNode = FindModifyCurveFeedingControlRig(ControlRigNode);
    if (!ModifyCurveNode)
    {
        ModifyCurveNode = AddAnimGraphNodeToGraph<UAnimGraphNode_ModifyCurve>(
            TargetGraph,
            FVector2D(static_cast<double>(ControlRigNode->NodePosX) - 260.0, ControlRigNode->NodePosY));
        if (!ModifyCurveNode)
        {
            return CreateErrorResponse(TEXT("Failed to create ModifyCurve node for ControlRig forced driver"));
        }
        bCreatedModifyCurveNode = true;
    }
    else
    {
        bCurveInsertedBeforeControlRig = true;
    }

    SelectedCurve=ModifyCurveNode;AddAllowed(ModifyCurveNode,AllowedNodes);
    FAnimNode_ModifyCurve* ModifyCurveAnimNode = GetMutableModifyCurveAnimNode(ModifyCurveNode);
    if (!ModifyCurveAnimNode)
    {
        return CreateErrorResponse(TEXT("Failed to resolve internal FAnimNode_ModifyCurve settings"));
    }

    bool bSettingsChanged = false;
    bool bCurvePinsChanged = false;
    TArray<FString> ChangedFields;

    if (!ModifyCurveEntriesMatchNode(ModifyCurveAnimNode, CurveEntries))
    {
        ModifyNode(ModifyCurveNode);WriteIntent(TEXT("modify_curve_array_writes"));
        ModifyCurveAnimNode->CurveNames.Reset();
        ModifyCurveAnimNode->CurveValues.Reset();
        ModifyCurveAnimNode->CurveMap.Reset();
        for (const TPair<FName, float>& Entry : CurveEntries)
        {
            ModifyCurveAnimNode->CurveNames.Add(Entry.Key);
            ModifyCurveAnimNode->CurveValues.Add(Entry.Value);
            ModifyCurveAnimNode->CurveMap.Add(Entry.Key, Entry.Value);
        }
        if(!ReconstructCurve(ModifyCurveNode)) { return Failure(CurrentResult->Error); }
        ModifyCurveAnimNode=GetMutableModifyCurveAnimNode(ModifyCurveNode);if(!ModifyCurveAnimNode) { return Failure(CurrentResult->Error); }
        bSettingsChanged = true;
        bCurvePinsChanged = true;
        ChangedFields.Add(TEXT("Curves"));
    }

    if (!FMath::IsNearlyEqual(ModifyCurveAnimNode->Alpha, Alpha, KINDA_SMALL_NUMBER))
    {
        WriteIntent(TEXT("modify_curve_scalar_writes"));ModifyCurveAnimNode->Alpha = Alpha;
        bSettingsChanged = true;
        ChangedFields.Add(TEXT("Alpha"));
    }
    if (UEdGraphPin* AlphaPin = FindPin(ModifyCurveNode, TEXT("Alpha"), EGPD_Input))
    {
        const FString AlphaDefault = FString::SanitizeFloat(Alpha);
        if (!AlphaPin->DefaultValue.Equals(AlphaDefault, ESearchCase::CaseSensitive))
        {
            const FEdGraphPinReference SetterRef(AlphaPin);WriteIntent(TEXT("try_set_default_calls"));TargetGraph->GetSchema()->TrySetDefaultValue(*AlphaPin, AlphaDefault);
            if(!AfterDefaultSetter(AlphaPin,SetterRef)) { return Failure(CurrentResult->Error); }
            Count(*CurrentResult,TEXT("pin_default_changed_calls"));ModifyCurveNode->PinDefaultValueChanged(AlphaPin);
            if(!AfterDefaultSetter(AlphaPin,SetterRef)) { return Failure(CurrentResult->Error); }
            ModifyCurveAnimNode=GetMutableModifyCurveAnimNode(ModifyCurveNode);if(!ModifyCurveAnimNode) { return Failure(CurrentResult->Error); }
            bSettingsChanged = true;
            ChangedFields.Add(TEXT("AlphaPin"));
        }
    }

    if (ModifyCurveAnimNode->ApplyMode != ApplyMode)
    {
        WriteIntent(TEXT("modify_curve_scalar_writes"));ModifyCurveAnimNode->ApplyMode = ApplyMode;
        bSettingsChanged = true;
        ChangedFields.Add(TEXT("ApplyMode"));
    }

    for (const TPair<FName, float>& Entry : CurveEntries)
    {
        const FString CurveName = Entry.Key.ToString();
        UEdGraphPin* CurvePin = nullptr;
        for (UEdGraphPin* Pin : ModifyCurveNode->Pins)
        {
            if(Pin&&!PinKnown(Pin)) { return Failure(CurrentResult->Error); }
            if (Pin && Pin->Direction == EGPD_Input && Pin->PinFriendlyName.ToString().Equals(CurveName, ESearchCase::CaseSensitive))
            {
                CurvePin = Pin;
                break;
            }
        }

        if (CurvePin)
        {
            const FString CurveDefault = FString::SanitizeFloat(Entry.Value);
            if (!CurvePin->DefaultValue.Equals(CurveDefault, ESearchCase::CaseSensitive))
            {
                const FEdGraphPinReference SetterRef(CurvePin);WriteIntent(TEXT("try_set_default_calls"));TargetGraph->GetSchema()->TrySetDefaultValue(*CurvePin, CurveDefault);
            if(!AfterDefaultSetter(CurvePin,SetterRef)) { return Failure(CurrentResult->Error); }
                Count(*CurrentResult,TEXT("pin_default_changed_calls"));ModifyCurveNode->PinDefaultValueChanged(CurvePin);
            if(!AfterDefaultSetter(CurvePin,SetterRef)) { return Failure(CurrentResult->Error); }
            ModifyCurveAnimNode=GetMutableModifyCurveAnimNode(ModifyCurveNode);if(!ModifyCurveAnimNode) { return Failure(CurrentResult->Error); }
                bSettingsChanged = true;
                ChangedFields.Add(FString::Printf(TEXT("CurvePin:%s"), *CurveName));
            }
        }
    }

    UEdGraphPin* ModifyCurveInput = FindFirstPosePin(ModifyCurveNode, EGPD_Input, { TEXT("SourcePose") });
    UEdGraphPin* ModifyCurveOutput = FindFirstPosePin(ModifyCurveNode, EGPD_Output);
    ControlRigSourceInput = FindFirstPosePin(ControlRigNode, EGPD_Input, { TEXT("Source") });
    if (!ModifyCurveInput || !ModifyCurveOutput || !ControlRigSourceInput)
    {
        return CreateErrorResponse(TEXT("Failed to resolve required ControlRig forced-driver pose pins"));
    }

    UEdGraphPin* UpstreamPoseOutput = nullptr;
    if (ControlRigSourceInput->LinkedTo.Contains(ModifyCurveOutput))
    {
        bCurveInsertedBeforeControlRig = true;
        UpstreamPoseOutput = ModifyCurveInput->LinkedTo.Num() > 0 ? ModifyCurveInput->LinkedTo[0] : nullptr;
    }
    else
    {
        UpstreamPoseOutput = ControlRigSourceInput->LinkedTo.Num() > 0 ? ControlRigSourceInput->LinkedTo[0] : nullptr;
    }

    if(UpstreamPoseOutput&&!PinKnown(UpstreamPoseOutput)) { return Failure(CurrentResult->Error); }
    if (!UpstreamPoseOutput)
    {
        return CreateErrorResponse(TEXT("ControlRig Source pose input has no upstream pose to route through ModifyCurve"));
    }

    SelectedUpstream=FEdGraphPinReference(UpstreamPoseOutput);AddAllowed(UpstreamPoseOutput->GetOwningNodeUnchecked(),AllowedNodes);
    const FEdGraphPinReference UpstreamRef(UpstreamPoseOutput),CurveInRef(ModifyCurveInput),CurveOutRef(ModifyCurveOutput),RigSourceRef(ControlRigSourceInput);
    auto PoseCurrent=[&](){return RefreshInventory()&&UpstreamRef.Get()==UpstreamPoseOutput&&CurveInRef.Get()==ModifyCurveInput&&CurveOutRef.Get()==ModifyCurveOutput&&RigSourceRef.Get()==ControlRigSourceInput&&PinKnown(UpstreamPoseOutput)&&PinKnown(ModifyCurveInput)&&PinKnown(ModifyCurveOutput)&&PinKnown(ControlRigSourceInput);};
    bool bPoseLinksChanged = false;
    bool bChangedThisLink = false;
    FString LinkError;
    TSharedPtr<FJsonObject> LinkErrorObj;
    if (!EnsureAnimGraphConnection(TargetGraph, UpstreamPoseOutput, ModifyCurveInput, bReplaceExisting, bChangedThisLink, LinkError, &LinkErrorObj))
    {
        if (LinkErrorObj.IsValid())
        {
            AddGraphField(LinkErrorObj, Blueprint, TargetGraph);
            return LinkErrorObj;
        }
        return CreateErrorResponse(LinkError);
    }
    if(!PoseCurrent()) { NoteUnsafe(TEXT("Current four pose pins retired across original link callback"));return Failure(CurrentResult->Error); }
    bPoseLinksChanged |= bChangedThisLink;

    if (!EnsureAnimGraphConnection(TargetGraph, ModifyCurveOutput, ControlRigSourceInput, bReplaceExisting, bChangedThisLink, LinkError, &LinkErrorObj))
    {
        if (LinkErrorObj.IsValid())
        {
            AddGraphField(LinkErrorObj, Blueprint, TargetGraph);
            return LinkErrorObj;
        }
        return CreateErrorResponse(LinkError);
    }
    if(!PoseCurrent()) { NoteUnsafe(TEXT("Current four pose pins retired across original link callback"));return Failure(CurrentResult->Error); }
    bPoseLinksChanged |= bChangedThisLink;
    bCurveInsertedBeforeControlRig = true;

    TArray<FName> AvailableInputNames = GetControlRigCustomPinNamesByReflection(ControlRigNode);
    bool bVisibilityChanged = false;
    bool bDefaultChanged = false;
    bool bInputLinksChanged = false;
    TArray<TSharedPtr<FJsonValue>> PerInputResults;

    for (const FControlRigInputDefaultRequest& Request : DefaultRequests)
    {
        bool bFound = false;
        bool bChangedThisPinVisibility = false;
        FString VisibilityError;
        if (!SetControlRigCustomPinVisibleByReflection(ControlRigNode, Request.PropertyName, true, bFound, bChangedThisPinVisibility, VisibilityError))
        {
            return CreateErrorResponse(VisibilityError);
        }

        if (!bFound)
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("ControlRig input '%s' is not available on node '%s'"),
                *Request.PropertyName.ToString(),
                *ControlRigNode->GetName()));
            ErrorObj->SetArrayField(TEXT("available_input_names"), NamesToJsonArray(AvailableInputNames));
            ErrorObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
            AddGraphField(ErrorObj, Blueprint, TargetGraph);
            return ErrorObj;
        }

        if (bChangedThisPinVisibility)
        {
            bVisibilityChanged = true;
            ChangedFields.Add(FString::Printf(TEXT("ExposePin:%s"), *Request.PropertyName.ToString()));
        }
    }

    if (bVisibilityChanged)
    {
        if(!ReconstructRig(ControlRigNode)) { return Failure(CurrentResult->Error); }
        AvailableInputNames = GetControlRigCustomPinNamesByReflection(ControlRigNode);
    }

    for (const FControlRigInputDefaultRequest& Request : DefaultRequests)
    {
        UEdGraphPin* InputPin = FindControlRigInputPin(ControlRigNode, Request.PropertyName);
        if (!InputPin)
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("ControlRig input pin '%s' was not found after exposing it"),
                *Request.PropertyName.ToString()));
            ErrorObj->SetArrayField(TEXT("available_input_names"), NamesToJsonArray(AvailableInputNames));
            ErrorObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
            AddGraphField(ErrorObj, Blueprint, TargetGraph);
            return ErrorObj;
        }

        const int32 LinkedPinCountBefore = InputPin->LinkedTo.Num();
        if (bDisconnectExistingLinks && LinkedPinCountBefore > 0)
        {
            const FEdGraphPinReference BreakRef(InputPin);WriteIntent(TEXT("break_pin_links_calls"));K2Schema->BreakPinLinks(*InputPin, true);
            if(!AfterDefaultSetter(InputPin,BreakRef)) { return Failure(CurrentResult->Error); }
            Count(*CurrentResult,TEXT("pin_connection_changed_calls"));ControlRigNode->PinConnectionListChanged(InputPin);
            if(!AfterDefaultSetter(InputPin,BreakRef)) { return Failure(CurrentResult->Error); }
            bInputLinksChanged = true;
            ChangedFields.Add(FString::Printf(TEXT("BreakLinks:%s"), *Request.PropertyName.ToString()));
        }

        const FString PreviousDefaultValue = InputPin->GetDefaultAsString();
        FString AppliedValue;
        FString DefaultError;
        FString RequestedDefaultValue;
        if(Request.Value.IsValid()&&Request.Value->Type==EJson::String&&(InputPin->PinType.PinCategory==UEdGraphSchema_K2::PC_Class||InputPin->PinType.PinCategory==UEdGraphSchema_K2::PC_Object)) { if(!Hold(InputPin->DefaultObject)) { return Failure(CurrentResult->Error); }ExpectedDefaultObjects.Add(InputPin->GetFName(),InputPin->DefaultObject.Get()); }
        const bool bDefaultAlreadyMatches = PinDefaultSemanticallyMatchesJsonValue(InputPin, Request.Value, K2Schema, RequestedDefaultValue);
        if (bDefaultAlreadyMatches)
        {
            AppliedValue = RequestedDefaultValue.IsEmpty() ? PreviousDefaultValue : RequestedDefaultValue;
        }
        else if (!ApplyPinDefaultValueChecked(InputPin, Request.Value, K2Schema, AppliedValue, DefaultError))
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(
                TEXT("Failed to set ControlRig input '%s' default: %s"),
                *Request.PropertyName.ToString(),
                *DefaultError));
            ErrorObj->SetStringField(TEXT("pin_name"), Request.PropertyName.ToString());
            ErrorObj->SetStringField(TEXT("previous_default_value"), PreviousDefaultValue);
            ErrorObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
            AddGraphField(ErrorObj, Blueprint, TargetGraph);
            return ErrorObj;
        }

        if (!bDefaultAlreadyMatches)
        {
            const FEdGraphPinReference DefaultRef(InputPin);Count(*CurrentResult,TEXT("pin_default_changed_calls"));ControlRigNode->PinDefaultValueChanged(InputPin);
            if(!AfterDefaultSetter(InputPin,DefaultRef)) { return Failure(CurrentResult->Error); }
        }
        const FString CurrentDefaultValue = InputPin->GetDefaultAsString();
        const bool bChangedThisDefault = !bDefaultAlreadyMatches && !PreviousDefaultValue.Equals(CurrentDefaultValue, ESearchCase::CaseSensitive);
        if (bChangedThisDefault)
        {
            bDefaultChanged = true;
            ChangedFields.Add(FString::Printf(TEXT("Default:%s"), *Request.PropertyName.ToString()));
        }

        TSharedPtr<FJsonObject> InputResult = MakeShared<FJsonObject>();
        InputResult->SetStringField(TEXT("name"), Request.PropertyName.ToString());
        InputResult->SetField(TEXT("requested_value"), Request.Value);
        InputResult->SetBoolField(TEXT("pin_found"), true);
        InputResult->SetBoolField(TEXT("default_changed"), bChangedThisDefault);
        InputResult->SetBoolField(TEXT("default_already_matched"), bDefaultAlreadyMatches);
        InputResult->SetBoolField(TEXT("pin_linked"), InputPin->LinkedTo.Num() > 0);
        InputResult->SetNumberField(TEXT("linked_pin_count_before"), LinkedPinCountBefore);
        InputResult->SetNumberField(TEXT("linked_pin_count"), InputPin->LinkedTo.Num());
        InputResult->SetBoolField(TEXT("links_disconnected"), bDisconnectExistingLinks && LinkedPinCountBefore > 0);
        InputResult->SetStringField(TEXT("previous_default_value"), PreviousDefaultValue);
        InputResult->SetStringField(TEXT("applied_default_value"), AppliedValue);
        InputResult->SetStringField(TEXT("current_default_value"), CurrentDefaultValue);
        InputResult->SetStringField(TEXT("pin_type_category"), InputPin->PinType.PinCategory.ToString());
        if (InputPin->PinType.PinSubCategoryObject.IsValid())
        {
            InputResult->SetStringField(TEXT("pin_subcategory_object"), InputPin->PinType.PinSubCategoryObject->GetPathName());
        }
        PerInputResults.Add(MakeShared<FJsonValueObject>(InputResult));
    }

    const bool bGraphChanged = bCreatedModifyCurveNode || bSettingsChanged || bPoseLinksChanged || bVisibilityChanged || bDefaultChanged || bInputLinksChanged;
    if (bGraphChanged)
    {
        if (bCurvePinsChanged || bVisibilityChanged)
        {
            WriteIntent(TEXT("mark_structurally_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);CurrentResult->Data->SetBoolField(TEXT("implicit_skeleton_regeneration_possible"),true);
            if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)) { return Failure(CurrentResult->Error); }
        }
        else
        {
            WriteIntent(TEXT("mark_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)) { return Failure(CurrentResult->Error); }
        }
        Count(*CurrentResult,TEXT("notify_graph_changed_calls"));TargetGraph->NotifyGraphChanged();
        if(!RefreshInventory()||!Member(ControlRigNode,TargetGraph)) { return Failure(CurrentResult->Error); }
    }

    TArray<TSharedPtr<FJsonValue>> ChangedFieldValues;
    for (const FString& FieldName : ChangedFields)
    {
        ChangedFieldValues.Add(MakeShared<FJsonValueString>(FieldName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("connected"), true);
    ResultObj->SetBoolField(TEXT("graph_changed"), bGraphChanged);
    ResultObj->SetBoolField(TEXT("created_modify_curve_node"), bCreatedModifyCurveNode);
    ResultObj->SetBoolField(TEXT("curve_inserted_before_controlrig"), bCurveInsertedBeforeControlRig);
    ResultObj->SetBoolField(TEXT("curve_settings_changed"), bSettingsChanged);
    ResultObj->SetBoolField(TEXT("pose_links_changed"), bPoseLinksChanged);
    ResultObj->SetBoolField(TEXT("visibility_changed"), bVisibilityChanged);
    ResultObj->SetBoolField(TEXT("defaults_changed"), bDefaultChanged);
    ResultObj->SetBoolField(TEXT("input_links_changed"), bInputLinksChanged);
    ResultObj->SetBoolField(TEXT("disconnect_existing_links"), bDisconnectExistingLinks);
    ResultObj->SetBoolField(TEXT("replace_existing"), bReplaceExisting);
    ResultObj->SetNumberField(TEXT("alpha"), Alpha);
    ResultObj->SetStringField(TEXT("apply_mode"), GetModifyCurveApplyModeName(ApplyMode));
    ResultObj->SetStringField(TEXT("control_rig_class"), GetControlRigClassPathFromNode(ControlRigNode));
    ResultObj->SetObjectField(TEXT("curve_values"), ModifyCurveEntriesToJsonObject(CurveEntries));
    ResultObj->SetObjectField(TEXT("requested_input_defaults"), ControlRigInputDefaultsToJsonObject(DefaultRequests));
    ResultObj->SetArrayField(TEXT("input_results"), PerInputResults);
    ResultObj->SetArrayField(TEXT("available_input_names"), NamesToJsonArray(AvailableInputNames));
    ResultObj->SetArrayField(TEXT("changed_fields"), ChangedFieldValues);
    if(!PinKnown(UpstreamPoseOutput)||UpstreamRef.Get()!=UpstreamPoseOutput) { NoteUnsafe(TEXT("Returned upstream pin retired across later Rig callback"));return Failure(CurrentResult->Error); }
    ResultObj->SetObjectField(TEXT("upstream_pose_pin"), PinToJson(UpstreamPoseOutput));
    ResultObj->SetObjectField(TEXT("modify_curve_node"), NodeToJson(ModifyCurveNode, true));
    ResultObj->SetObjectField(TEXT("control_rig_node"), NodeToJson(ControlRigNode, true));
    ResultObj->SetObjectField(TEXT("modify_curve_settings"), AnimGraphNodeSettingsToJson(ModifyCurveNode, 4));
    ResultObj->SetObjectField(TEXT("control_rig_settings"), AnimGraphNodeSettingsToJson(ControlRigNode, 4));
    AddGraphField(ResultObj, Blueprint, TargetGraph);
    return ResultObj;
}

bool CurrentOptionalVisible(UAnimGraphNode_ControlRig* N,const FName& Name)
{
    if(!CustomPinsSafe(N)) { return false; }auto* A=FindFProperty<FArrayProperty>(N->GetClass(),TEXT("CustomPinProperties"));auto* S=CastField<FStructProperty>(A->Inner);auto* Key=FindFProperty<FNameProperty>(S->Struct,TEXT("PropertyName"));auto* Show=FindFProperty<FBoolProperty>(S->Struct,TEXT("bShowPin"));FScriptArrayHelper H(A,A->ContainerPtrToValuePtr<void>(N));
    for(int32 I=0;I<H.Num();++I) { if(Key->GetPropertyValue_InContainer(H.GetRawPtr(I))==Name) { return Show->GetPropertyValue_InContainer(H.GetRawPtr(I)); } }return false;
}
bool CurrentDefaultMatches(UEdGraphPin* Pin,const TSharedPtr<FJsonValue>& V,const UEdGraphSchema_K2* Schema)
{
    if(!PinKnown(Pin)||!V.IsValid()||!Hold(Schema)||!Hold(Pin->DefaultObject)) { return false; }
    Count(*CurrentResult,TEXT("default_validity_queries"));if(!Schema->IsCurrentPinDefaultValid(Pin).IsEmpty()) { return false; }
    if(V->Type==EJson::String&&(Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Class||Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Object))
    { auto* O=Pin->DefaultObject.Get();const auto* Expected=ExpectedDefaultObjects.Find(Pin->GetFName());return Expected&&Hold(*Expected)&&*Expected==O; }
    FString Text;return PinDefaultSemanticallyMatchesJsonValue(Pin,V,Schema,Text);
}
bool CurrentDesiredState(const TSharedRef<FJsonObject>& P,bool Forced)
{
    Count(*CurrentResult,TEXT("final_desired_state_checks"));if(!RefreshInventory()||!SelectedGraph||!SelectedRig||!Member(SelectedRig,SelectedGraph)||!RigSourceSafe(SelectedRig)) { return false; }const auto* Schema=Cast<UEdGraphSchema_K2>(SelectedGraph->GetSchema());if(!Schema||!Hold(Schema)) { return false; }
    FString Error;const auto Defaults=ParseControlRigInputDefaults(P,Error);if(!Error.IsEmpty()) { return false; }const auto Expose=Forced?TArray<FName>():ParseControlRigExposeInputPins(P,Error);if(!Error.IsEmpty()) { return false; }bool Disconnect=false;P->TryGetBoolField(TEXT("disconnect_existing_links"),Disconnect);TArray<TSharedPtr<FJsonValue>> Receipts;
    for(const auto& Request:Defaults)
    { auto* Pin=FindControlRigInputPin(SelectedRig,Request.PropertyName);if(!CurrentOptionalVisible(SelectedRig,Request.PropertyName)||!Pin||!CurrentDefaultMatches(Pin,Request.Value,Schema)||(Disconnect&&Pin->LinkedTo.Num()!=0)) { return false; }auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Request.PropertyName.ToString());Row->SetObjectField(TEXT("current_pin"),PinToJson(Pin));Row->SetBoolField(TEXT("current_default_and_visibility_verified"),true);Receipts.Add(MakeShared<FJsonValueObject>(Row)); }
    for(const FName& Name:Expose) { if(!CurrentOptionalVisible(SelectedRig,Name)||!FindControlRigInputPin(SelectedRig,Name)) { return false; } }
    CurrentResult->Data->SetArrayField(TEXT("current_input_postcondition_receipts"),Receipts);CurrentResult->Data->SetBoolField(TEXT("current_input_defaults_and_visibility_verified"),true);
    if(!Forced) { return !Incomplete(); }if(!SelectedCurve||!Member(SelectedCurve,SelectedGraph)) { return false; }auto* Runtime=GetMutableModifyCurveAnimNode(SelectedCurve);const auto Curves=ParseModifyCurveEntries(P,Error);EModifyCurveApplyMode Mode;const float Alpha=FMath::Clamp(static_cast<float>(P->GetNumberField(TEXT("alpha"))),0.0f,1.0f);
    if(!Runtime||!Error.IsEmpty()||!ParseModifyCurveApplyMode(GetStringParam(P,TEXT("apply_mode")),Mode)||!ModifyCurveEntriesMatchNode(Runtime,Curves)||!FMath::IsNearlyEqual(Runtime->Alpha,Alpha,KINDA_SMALL_NUMBER)||Runtime->ApplyMode!=Mode) { return false; }
    if(auto* Pin=FindPin(SelectedCurve,TEXT("Alpha"),EGPD_Input)) { if(!CurrentDefaultMatches(Pin,MakeShared<FJsonValueNumber>(Alpha),Schema)) { return false; } }
    for(const auto& Curve:Curves) { for(UEdGraphPin* Pin:SelectedCurve->Pins) { if(!PinKnown(Pin)) { return false; }if(Pin->Direction==EGPD_Input&&Pin->PinFriendlyName.ToString().Equals(Curve.Key.ToString(),ESearchCase::CaseSensitive)) { if(!CurrentDefaultMatches(Pin,MakeShared<FJsonValueNumber>(Curve.Value),Schema)) { return false; }break; } } }
    auto* Upstream=SelectedUpstream.Get();auto* In=FindFirstPosePin(SelectedCurve,EGPD_Input,{TEXT("SourcePose")});auto* Out=FindFirstPosePin(SelectedCurve,EGPD_Output);auto* RigIn=FindFirstPosePin(SelectedRig,EGPD_Input,{TEXT("Source")});
    if(!PinKnown(Upstream)||!PinKnown(In)||!PinKnown(Out)||!PinKnown(RigIn)||!Upstream->LinkedTo.Contains(In)||!In->LinkedTo.Contains(Upstream)||!Out->LinkedTo.Contains(RigIn)||!RigIn->LinkedTo.Contains(Out)) { return false; }
    CurrentResult->Data->SetBoolField(TEXT("current_curve_settings_and_two_reciprocal_pose_edges_verified"),true);return !Incomplete();
}


void Init(FResult& R,const FString& Operation)
{
    R.Data->SetStringField(TEXT("operation"),Operation);R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("automatic_retry"),false);R.Data->SetBoolField(TEXT("total_SDK_cache_or_template_evaluation_effects_unverified"),true);R.Data->SetBoolField(TEXT("runtime_ControlRig_execution_verified"),false);auto C=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("add_ubergraph_page_calls"),TEXT("allocate_pins_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("break_all_pin_links_calls"),TEXT("break_pin_links_calls"),TEXT("can_create_connection_calls"),TEXT("checked_default_setter_calls"),TEXT("create_node_guid_calls"),TEXT("default_validity_queries"),TEXT("event_graph_create_calls"),TEXT("event_graph_create_returns"),TEXT("existing_rig_CDO_queries"),TEXT("external_descriptor_value_reads"),TEXT("final_desired_state_checks"),TEXT("get_all_graphs_calls"),TEXT("get_fnode_calls"),TEXT("get_fnode_property_calls"),TEXT("get_fnode_type_calls"),TEXT("graph_add_node_calls"),TEXT("mark_modified_calls"),TEXT("mark_structurally_modified_calls"),TEXT("modify_curve_array_writes"),TEXT("modify_curve_scalar_writes"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("node_modify_calls"),TEXT("node_position_write_calls"),TEXT("node_struct_property_search_calls"),TEXT("nodes_serialized"),TEXT("notify_graph_changed_calls"),TEXT("optional_pin_visibility_writes"),TEXT("original_default_restore_field_writes"),TEXT("outer_transaction_calls"),TEXT("pin_connection_changed_calls"),TEXT("pin_default_changed_calls"),TEXT("pins_inventory_calls"),TEXT("pins_visited"),TEXT("post_placed_calls"),TEXT("property_export_calls"),TEXT("reconstruct_calls"),TEXT("reflected_property_reads"),TEXT("rig_asset_reference_getter_calls"),TEXT("rig_external_variable_queries"),TEXT("rig_modular_state_queries"),TEXT("source_class_lookups"),TEXT("state_capture_calls"),TEXT("try_create_connection_calls"),TEXT("try_create_connection_returns"),TEXT("try_set_default_calls")}) { C->SetNumberField(K,0); };R.Data->SetObjectField(TEXT("native_call_counts"),C);
}
FResult Finish(FResult& R)
{ if(Serialize(R.Data).IsEmpty()) { R.Data->RemoveField(TEXT("selected_state_before_json"));R.Data->RemoveField(TEXT("selected_state_after_json"));R.Data->RemoveField(TEXT("legacy_result"));R.Data->SetBoolField(TEXT("oversized_observations_omitted"),true);R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;if(R.Error.IsEmpty()) { R.Error=TEXT("Current observations exceed bounded output; accepted prefix retained"); }R.ErrorCode=TEXT("controlrig_output_incomplete"); }return R; }
FResult Execute(const TSharedRef<FJsonObject>& P,const FString& Operation,bool Forced)
{
    FResult R;Init(R,Operation);if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { R.ErrorCode=TEXT("controlrig_editor_invalid");R.Error=TEXT("Idle nonreentrant Editor game thread required");return Finish(R); }
    struct FScope { FScope(FResult& V,const FString& O) { CurrentResult=&V;CurrentOperation=O;Held.Reset();HeldObjects.Reset();KnownPins.Reset();PropertyValues=0;SelectedGraph=nullptr;SelectedRig=nullptr;SelectedCurve=nullptr;SelectedUpstream=FEdGraphPinReference();ExpectedDefaultObjects.Reset();BeforeSource.Reset();BeforeText.Reset();AllowedNodes.Reset(); }~FScope(){CurrentResult=nullptr;CurrentBlueprint=nullptr;SelectedGraph=nullptr;SelectedRig=nullptr;SelectedCurve=nullptr;SelectedUpstream=FEdGraphPinReference();ExpectedDefaultObjects.Reset();BeforeSource.Reset();Held.Reset();HeldObjects.Reset();KnownPins.Reset();AnimBase=nullptr;AnimStruct=PoseStruct=ComponentPoseStruct=nullptr;} } Scope(R,Operation);
    for(const TCHAR* K:{TEXT("blueprint_name"),TEXT("graph_name"),TEXT("graph_id"),TEXT("graph_type"),TEXT("node_id"),TEXT("node_name"),TEXT("title_contains"),TEXT("control_rig_class"),TEXT("apply_mode")}) { if(!SafeText(GetStringParam(P,K),NAME_SIZE-1)) { R.ErrorCode=TEXT("controlrig_input_invalid");R.Error=TEXT("Bounded original literal inputs without interior NUL required");return Finish(R); } }
    if(Forced&&!FloatProjectionSafe(P->GetNumberField(TEXT("alpha")))) { R.ErrorCode=TEXT("controlrig_alpha_invalid");return Finish(R); }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(FText::FromString(Operation));const auto Legacy=Forced?RunForced(P):RunSet(P);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("legacy_result_returned"),true);
    if(CurrentBlueprint&&Hold(CurrentBlueprint)&&Hold(CurrentBlueprint->GetOutermost()))
    { const auto After=Capture(CurrentBlueprint);const auto Text=Serialize(After);R.Data->SetStringField(TEXT("selected_state_after_json"),Text);const bool Changed=!BeforeText.IsEmpty()&&!Text.IsEmpty()&&!BeforeText.Equals(Text,ESearchCase::CaseSensitive);R.bChanged|=Changed;R.Data->SetBoolField(TEXT("observed_selected_state_changed"),Changed);if(!Incomplete()&&!Text.IsEmpty()) { R.bChangeStateKnown=true; }if(SelectedGraph&&!BeforeText.IsEmpty()&&!Text.IsEmpty()&&!Incomplete()) { R.Data->SetBoolField(TEXT("unaffected_selected_source_preserved"),Preserve(BeforeSource,After,SelectedGraph,AllowedNodes)); } }
    if(Incomplete()) { R.bChangeStateKnown=false;return Finish(R); }bool Success=true;Legacy->TryGetBoolField(TEXT("success"),Success);if(!Success) { R.ErrorCode=TEXT("controlrig_original_failure");Legacy->TryGetStringField(TEXT("error"),R.Error);R.Data->SetBoolField(TEXT("late_failure_no_automatic_cleanup"),R.bMutationAttempted);return Finish(R); }
    bool Preserved=false;R.Data->TryGetBoolField(TEXT("unaffected_selected_source_preserved"),Preserved);R.bPostconditionPassed=Preserved&&CurrentClassesSafe(CurrentBlueprint)&&CurrentDesiredState(P,Forced);if(!R.bPostconditionPassed) { if(Incomplete()) { R.bChangeStateKnown=false; }R.ErrorCode=TEXT("controlrig_current_source_postcondition_failed");R.Error=TEXT("Current source/default/visibility/curve/edge proof failed; original accepted prefix retained");return Finish(R); }R.bSuccess=true;R.Stage=TEXT("complete");return Finish(R);
}
}
Variable::FResult SetInputDefaults(const FString& BlueprintName,const FString& InputDefaultsJson,const FString& ExposeInputPinsJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeName,const FString& TitleContains,const FString& ControlRigClass,bool bDisconnectExistingLinks,bool bAllowNonSample)
{
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("blueprint_name"),BlueprintName);P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_id"),GraphId);P->SetStringField(TEXT("graph_type"),GraphType);P->SetStringField(TEXT("node_id"),NodeId);P->SetStringField(TEXT("node_name"),NodeName);P->SetStringField(TEXT("title_contains"),TitleContains);P->SetStringField(TEXT("control_rig_class"),ControlRigClass);P->SetBoolField(TEXT("disconnect_existing_links"),bDisconnectExistingLinks);P->SetBoolField(TEXT("allow_non_sample"),bAllowNonSample);FResult Invalid;Init(Invalid,TEXT("set_anim_graph_controlrig_input_defaults"));if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { Invalid.ErrorCode=TEXT("controlrig_editor_invalid");Invalid.Error=TEXT("Idle Editor game thread required before carriers");return Invalid; }CurrentResult=&Invalid;const bool Good=Pack(InputDefaultsJson,TEXT("input_defaults"),P,true)&&Pack(ExposeInputPinsJson,TEXT("expose_input_pins"),P,true);CurrentResult=nullptr;if(!Good) { Invalid.ErrorCode=TEXT("controlrig_json_invalid");return Invalid; }return Execute(P,TEXT("set_anim_graph_controlrig_input_defaults"),false);
}
Variable::FResult EnsureForcedDriver(const FString& BlueprintName,const FString& CurveValuesJson,const FString& InputDefaultsJson,double Alpha,const FString& ApplyMode,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeName,const FString& TitleContains,const FString& ControlRigClass,bool bReplaceExisting,bool bDisconnectExistingLinks,bool bAllowNonSample)
{
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("blueprint_name"),BlueprintName);P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_id"),GraphId);P->SetStringField(TEXT("graph_type"),GraphType);P->SetStringField(TEXT("node_id"),NodeId);P->SetStringField(TEXT("node_name"),NodeName);P->SetStringField(TEXT("title_contains"),TitleContains);P->SetStringField(TEXT("control_rig_class"),ControlRigClass);P->SetStringField(TEXT("apply_mode"),ApplyMode);P->SetNumberField(TEXT("alpha"),Alpha);P->SetBoolField(TEXT("replace_existing"),bReplaceExisting);P->SetBoolField(TEXT("disconnect_existing_links"),bDisconnectExistingLinks);P->SetBoolField(TEXT("allow_non_sample"),bAllowNonSample);FResult Invalid;Init(Invalid,TEXT("ensure_controlrig_forced_driver_animbp"));if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { Invalid.ErrorCode=TEXT("controlrig_editor_invalid");Invalid.Error=TEXT("Idle Editor game thread required before carriers");return Invalid; }CurrentResult=&Invalid;const bool Good=Pack(CurveValuesJson,TEXT("curve_values"),P,true)&&Pack(InputDefaultsJson,TEXT("input_defaults"),P,true);CurrentResult=nullptr;if(!Good) { Invalid.ErrorCode=TEXT("controlrig_json_invalid");return Invalid; }return Execute(P,TEXT("ensure_controlrig_forced_driver_animbp"),true);
}
}
