#include "Compatibility/UE58/CubelessAnimGraphSecondaryMotionApi.h"
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
#include "AnimGraphNode_AnimDynamics.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "BoneControllers/AnimNode_AnimDynamics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_Root.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
namespace Cubeless::UE58::AnimGraphSecondaryMotion
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
void NoteUnsafe(const TCHAR* Error) { if(CurrentResult) { CurrentResult->Data->SetBoolField(TEXT("observation_incomplete"),true);CurrentResult->ErrorCode=TEXT("anim_graph_secondary_motion_observation_incomplete");CurrentResult->Error=Error; } }
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


bool IsComponentPoseLinkPin(const UEdGraphPin* P) { return PinKnown(P)&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&P->PinType.PinSubCategoryObject.Get()==ComponentPoseStruct; }
UEdGraphPin* FindFirstComponentPosePin(UEdGraphNode* Node,EEdGraphPinDirection Direction,const TArray<FString>& Preferred=TArray<FString>())
{
    if(!Loaded(Node)) { return nullptr; }
    for(const auto& Name:Preferred) { for(UEdGraphPin* P:Node->Pins) { if(PinKnown(P)&&P->Direction==Direction&&!P->ParentPin&&IsComponentPoseLinkPin(P)&&P->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)) { return P; } } }
    for(UEdGraphPin* P:Node->Pins) { if(PinKnown(P)&&P->Direction==Direction&&!P->ParentPin&&IsComponentPoseLinkPin(P)) { return P; } }return nullptr;
}
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

constexpr int32 SecondaryBoneBudget=4096,SecondaryBodyBudget=4096;
UEdGraph* SelectedGraph=nullptr;FGuid SelectedGraphGuid;TSharedPtr<FJsonObject> BeforeSource;FString BeforeSourceText;TSet<FString> AllowedNodes;

TSharedPtr<FJsonObject> Capture(UBlueprint* BP);
bool CurrentClassesSafe(UBlueprint* BP);
void AddAllowed(UEdGraphNode* Node,TSet<FString>& Allowed);

FAnimNode_AnimDynamics* GetMutableDynamics(UAnimGraphNode_AnimDynamics* Node)
{
    if(!Node||!Member(Node,Node->GetGraph())) { return nullptr; }auto& R=*CurrentResult;
    Count(R,TEXT("get_fnode_property_calls"));FStructProperty* P=Node->GetFNodeProperty();
    if(!P||!Loaded(P->Struct)||!Hold(P->GetOwnerStruct())||P->Struct!=FAnimNode_AnimDynamics::StaticStruct()||P->GetFName()!=FName(TEXT("Node"))||P->ArrayDim!=1||!Layout(P,Node->GetClass()->GetPropertiesSize())) { NoteUnsafe(TEXT("Exact current AnimDynamics public runtime property/layout required"));return nullptr; }
    Count(R,TEXT("get_fnode_type_calls"));if(Node->GetFNodeType()!=P->Struct) { NoteUnsafe(TEXT("AnimDynamics public property/type mismatch"));return nullptr; }
    Count(R,TEXT("get_fnode_calls"));auto* Runtime=static_cast<FAnimNode_AnimDynamics*>(Node->GetFNode());
    if(!Runtime||Runtime!=P->ContainerPtrToValuePtr<FAnimNode_AnimDynamics>(Node)||Runtime->PhysicsBodyDefinitions.Num()>SecondaryBodyBudget||Runtime->PlanarLimits.Num()>SecondaryBodyBudget||Runtime->SphericalLimits.Num()>SecondaryBodyBudget) { NoteUnsafe(TEXT("Current AnimDynamics pointer/array bound invalid"));return nullptr; }
    if(!FMath::IsFinite(Runtime->Alpha)||Runtime->GravityOverride.ContainsNaN()||!FMath::IsFinite(Runtime->LinearDampingOverride)||!FMath::IsFinite(Runtime->AngularDampingOverride)) { NoteUnsafe(TEXT("Current AnimDynamics finite fields required"));return nullptr; }
    for(const auto& Body:Runtime->PhysicsBodyDefinitions)
    { if(Body.BoxExtents.ContainsNaN()||Body.ConstraintSetup.LinearAxesMin.ContainsNaN()||Body.ConstraintSetup.LinearAxesMax.ContainsNaN()||!FMath::IsFinite(Body.ConstraintSetup.ConeAngle)) { NoteUnsafe(TEXT("Current body definition nonfinite"));return nullptr; } }
    R.Data->SetBoolField(TEXT("runtime_public_accessor_identity_verified"),true);return Runtime;
}
TSharedPtr<FJsonObject> TypedDynamicsState(UAnimGraphNode_AnimDynamics* Node)
{
    auto J=MakeShared<FJsonObject>();auto* V=GetMutableDynamics(Node);if(!V) { return J; }TArray<TSharedPtr<FJsonValue>> Bodies;
    for(const auto& B:V->PhysicsBodyDefinitions)
    { auto X=MakeShared<FJsonObject>();X->SetStringField(TEXT("bone"),B.BoundBone.BoneName.ToString());X->SetBoolField(TEXT("linear_fully_locked"),B.ConstraintSetup.bLinearFullyLocked);Bodies.Add(MakeShared<FJsonValueObject>(X)); }
    J->SetArrayField(TEXT("typed_body_constraints"),Bodies);J->SetNumberField(TEXT("body_count"),V->PhysicsBodyDefinitions.Num());return J;
}
bool SkeletonChainSafe(const FReferenceSkeleton& Ref,const FName Root,const FName End)
{
    Count(*CurrentResult,TEXT("skeleton_chain_preflight_calls"));const auto& Infos=Ref.GetRefBoneInfo();
    if(Ref.GetNum()<=0||Ref.GetNum()>SecondaryBoneBudget||Infos.Num()!=Ref.GetNum()) { NoteUnsafe(TEXT("Target skeleton requires finite bounded final bone array"));return false; }
    TSet<int32> Seen;TArray<TSharedPtr<FJsonValue>> Chain;int32 Index=Ref.FindBoneIndex(End),RootIndex=Ref.FindBoneIndex(Root);int32 Steps=0;
    if(Index==INDEX_NONE||RootIndex==INDEX_NONE) { return true; } // Original missing-bone error remains authoritative.
    if(!Infos.IsValidIndex(Index)||!Infos.IsValidIndex(RootIndex)||Infos[Index].Name!=End||Infos[RootIndex].Name!=Root||Ref.FindBoneIndex(Infos[Index].Name)!=Index||Ref.FindBoneIndex(Infos[RootIndex].Name)!=RootIndex) { NoteUnsafe(TEXT("Requested skeleton names and final name-index mapping disagree before SDK chain traversal"));return false; }
    while(Index!=INDEX_NONE)
    { if(!Infos.IsValidIndex(Index)||Seen.Contains(Index)||++Steps>SecondaryBoneBudget) { NoteUnsafe(TEXT("Skeleton ancestry unavailable/cyclic/bounded traversal exceeded"));return false; }Seen.Add(Index);if(Ref.FindBoneIndex(Infos[Index].Name)!=Index) { NoteUnsafe(TEXT("Current ancestor name-index mapping disagrees with SDK name walk"));return false; }Chain.Add(MakeShared<FJsonValueString>(Infos[Index].Name.ToString()));if(Index==RootIndex) { CurrentResult->Data->SetNumberField(TEXT("requested_chain_bone_count"),Steps);CurrentResult->Data->SetArrayField(TEXT("requested_chain_bones"),Chain);return true; }const int32 Parent=Infos[Index].ParentIndex;if(Parent!=INDEX_NONE&&(!Infos.IsValidIndex(Parent)||Parent>=Index||Ref.FindBoneIndex(Infos[Parent].Name)!=Parent)) { NoteUnsafe(TEXT("Skeleton parent index violates loaded ordered final skeleton"));return false; }Index=Parent; }
    return true; // Original non-descendant refusal, never an extra repair.
}
bool KawaiiLayoutSafe(UClass* Class)
{
    if(!Class||!Hold(Class)||!Class->IsNative()||!Class->IsChildOf(UAnimGraphNode_Base::StaticClass())) { NoteUnsafe(TEXT("Loaded native Kawaii editor class required"));return false; }
    const auto* P=FindFProperty<FStructProperty>(Class,TEXT("Node"));
    if(!P||P->ArrayDim!=1||!Hold(P->Struct)||!Layout(P,Class->GetPropertiesSize())) { NoteUnsafe(TEXT("Kawaii reflected Node current layout unavailable"));return false; }
    TSet<const UStruct*> Path;int32 Counted=0;
    TFunction<bool(const UStruct*,int32)> Visit=[&](const UStruct* Struct,int32 Depth)
    { if(!Struct||!Hold(Struct)||Depth>16||Path.Contains(Struct)||++Counted>4096) { NoteUnsafe(TEXT("Kawaii current reflected struct traversal invalid"));return false; }Path.Add(Struct);
      for(TFieldIterator<FProperty> It(Struct);It;++It) { const FProperty* F=*It;if(!Layout(F,Struct->GetPropertiesSize())||F->ArrayDim<1||F->ArrayDim>4096) { NoteUnsafe(TEXT("Kawaii field current bounded layout invalid"));return false; }if(const auto* S=CastField<FStructProperty>(F)) { if(!Visit(S->Struct,Depth+1)) { return false; } }if(const auto* A=CastField<FArrayProperty>(F)) { if(!A->Inner) { return false; }if(const auto* S=CastField<FStructProperty>(A->Inner)) { if(!Visit(S->Struct,Depth+1)) { return false; } } } }
      Path.Remove(Struct);return true; };
    return Visit(P->Struct,0);
}
void AllowChain(UEdGraph* G,UClass* KawaiiClass=nullptr)
{
    AddAllowed(FindFirstNodeOfType<UAnimGraphNode_Root>(G),AllowedNodes);AddAllowed(FindFirstNodeOfType<UAnimGraphNode_LocalRefPose>(G),AllowedNodes);
    AddAllowed(FindFirstNodeOfType<UAnimGraphNode_LocalToComponentSpace>(G),AllowedNodes);AddAllowed(FindFirstNodeOfType<UAnimGraphNode_ComponentToLocalSpace>(G),AllowedNodes);
    AddAllowed(FindFirstNodeOfType<UAnimGraphNode_AnimDynamics>(G),AllowedNodes);
    if(KawaiiClass) { for(UEdGraphNode* Node:G->Nodes) { if(Node&&Hold(Node)&&Node->GetClass()==KawaiiClass) { AddAllowed(Node,AllowedNodes);break; } } }
}
bool PrepareBeforeWriters(UBlueprint* BP,UEdGraph* G,UAnimGraphNode_Root* Root,UClass* KawaiiClass)
{
    if(!RequiredTypes(G,*CurrentResult)||!RefreshInventory()||!CurrentClassesSafe(BP)||!Member(Root,G)) { return false; }
    if(auto* Existing=FindFirstNodeOfType<UAnimGraphNode_AnimDynamics>(G))
    { auto* V=GetMutableDynamics(Existing);if(!V) { return false; }const auto& Chain=CurrentResult->Data->GetArrayField(TEXT("requested_chain_bones"));TSet<FName> Names,Present;for(const auto& X:Chain) { Names.Add(FName(*X->AsString())); }int32 Predicted=0;for(const auto& Body:V->PhysicsBodyDefinitions) { if(Names.Contains(Body.BoundBone.BoneName)) { ++Predicted;Present.Add(Body.BoundBone.BoneName); } }Predicted+=Names.Num()-Present.Num();CurrentResult->Data->SetNumberField(TEXT("predicted_physics_body_count"),Predicted);if(Predicted>SecondaryBodyBudget) { NoteUnsafe(TEXT("SDK retained body definitions plus missing chain allocation exceed finite4096 scope"));return false; } }
    BeforeSource=Capture(BP);BeforeSourceText=Serialize(BeforeSource);CurrentResult->Data->SetStringField(TEXT("selected_state_before_json"),BeforeSourceText);SelectedGraph=G;SelectedGraphGuid=G->GraphGuid;CurrentResult->Data->SetStringField(TEXT("selected_root_guid"),Root->NodeGuid.ToString());AllowChain(G,KawaiiClass);
    return !BeforeSourceText.IsEmpty()&&!Incomplete();
}
void ModifyGraph(UEdGraph* G) { WriteIntent(TEXT("graph_modify_calls"));G->Modify(); }
void ModifyNode(UEdGraphNode* N) { WriteIntent(TEXT("node_modify_calls"));N->Modify(); }
bool CompileCurrent(UBlueprint* BP)
{
    if(!Hold(BP)||!CurrentClassesSafe(BP)||!RefreshInventory()) { return false; }
    auto& R=*CurrentResult;R.Stage=TEXT("compile_blueprint");R.bCompileAttempted=true;WriteIntent(TEXT("explicit_compile_calls"));FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
    auto C=MakeShared<FJsonObject>();C->SetNumberField(TEXT("errors"),Log.NumErrors);C->SetNumberField(TEXT("warnings"),Log.NumWarnings);C->SetNumberField(TEXT("status"),BP->Status);R.Data->SetObjectField(TEXT("compiler_result"),C);
    R.Compiled=Log.NumErrors==0&&(BP->Status==BS_UpToDate||BP->Status==BS_UpToDateWithWarnings);Count(R,TEXT("explicit_compile_returns"));
    if(!Hold(BP)||!CurrentClassesSafe(BP)||!RefreshInventory()) { return false; }return true;
}
bool SaveCurrent(UBlueprint* BP)
{
    auto& R=*CurrentResult;if(!Hold(BP)||!Hold(BP->GetOutermost())||!CurrentClassesSafe(BP)||!RefreshInventory()) { return false; }
    R.Stage=TEXT("forced_save_blueprint");R.bSaveAttempted=true;WriteIntent(TEXT("explicit_save_calls"));const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(BP,false);R.bSaved=Saved;Count(R,TEXT("explicit_save_returns"));R.Data->SetBoolField(TEXT("actual_forced_save_return"),Saved);
    if(!Hold(BP)||!CurrentClassesSafe(BP)||!RefreshInventory()) { return false; }return Saved;
}
UEdGraph* ResolveBlueprintGraphForNodeCommand(UBlueprint* BP,const TSharedPtr<FJsonObject>& P,FString& Error)
{
    CurrentBlueprint=BP;if(!Hold(BP)||!Hold(BP->GetOutermost())) { return nullptr; }auto* G=Resolve(BP,GetStringParam(P,TEXT("graph_name")),GetStringParam(P,TEXT("graph_id")),GetStringParam(P,TEXT("graph_type")),*CurrentResult);if(!G) { Error=CurrentResult->Error;return nullptr; }
    if(!G->Schema.Get()||!Hold(G->Schema.Get())||!Hold(G->GetSchema())||!RefreshInventory()) { Error=CurrentResult->Error;return nullptr; }return G;
}
UBlueprint* FindBlueprint(const FString& Name)
{
    CurrentResult->Stage=TEXT("load_blueprint");auto* BP=FindBlueprintByName(Name,*CurrentResult);if(BP&&Hold(BP)) { CurrentBlueprint=BP;CurrentResult->Data->SetStringField(TEXT("blueprint"),BP->GetPathName()); }return BP;
}
UEdGraph* FindBlueprintGraph(UBlueprint* BP,const FString& Id,const FString& Name,const FString& Type)
{ auto* G=Resolve(BP,Name,Id,Type,*CurrentResult);if(!G||!RefreshInventory()) { return nullptr; }return G; }
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
            if(!Node) { continue; }if(!Hold(Node)||!Node->NodeGuid.IsValid()||Guids.Contains(Node->NodeGuid)) { NoteUnsafe(TEXT("Node GUID identity missing/duplicate"));return J; }Guids.Add(Node->NodeGuid);auto Entry=NodeToJson(Node,true);Entry->SetStringField(TEXT("path"),Node->GetPathName());if(auto* D=Cast<UAnimGraphNode_AnimDynamics>(Node)) { Entry->SetObjectField(TEXT("typed_dynamics"),TypedDynamicsState(D)); }if(IsAnimNode(Node)) { Entry->SetObjectField(TEXT("settings"),AnimGraphNodeSettingsToJson(Node,4)); }Nodes.Add(MakeShared<FJsonValueObject>(Entry));
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
            else if(!Allowed.Contains(Id)&&(!EqualJson(Row->TryGetField(TEXT("x")),Current->TryGetField(TEXT("x")))||!EqualJson(Row->TryGetField(TEXT("y")),Current->TryGetField(TEXT("y"))))) { return false; }
            if(!Allowed.Contains(Id)&&!EqualJson(V,*Found)) { return false; }
        }
    }return !Incomplete();
}
constexpr const TCHAR* KawaiiEditorModuleName = TEXT("KawaiiPhysicsEd");
constexpr const TCHAR* KawaiiNodeClassPath = TEXT("/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics");

bool IsAllowedSecondaryMotionTarget(const FString& BlueprintPath)
{
    return BlueprintPath.StartsWith(TEXT("/Game/_MCP_Sample/"), ESearchCase::IgnoreCase)
        || BlueprintPath.StartsWith(TEXT("/Game/_MCP_Temp/"), ESearchCase::IgnoreCase);
}

bool TryParseSecondaryMotionBoneName(
    const FString& RequestedName,
    const TCHAR* FieldName,
    FName& OutBoneName,
    FString& OutError)
{
    if (RequestedName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), FieldName);
        return false;
    }
    if (RequestedName.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(
            TEXT("'%s' is too long for an Unreal name (maximum %d characters)"),
            FieldName,
            NAME_SIZE - 1);
        return false;
    }

    OutBoneName = FName(*RequestedName);
    if (OutBoneName.IsNone())
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' name '%s': name resolves to None"),
            FieldName,
            *RequestedName);
        return false;
    }
    return true;
}

bool TryGetFiniteSecondaryMotionNumber(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    const double DefaultValue,
    double& OutValue,
    FString& OutError)
{
    OutValue = DefaultValue;
    if (Params.IsValid() && Params->HasField(FieldName) && !Params->TryGetNumberField(FieldName, OutValue))
    {
        OutError = FString::Printf(TEXT("'%s' must be a number"), FieldName);
        return false;
    }
    if (!FMath::IsFinite(OutValue) || FMath::Abs(OutValue) > static_cast<double>(TNumericLimits<float>::Max()))
    {
        OutError = FString::Printf(TEXT("'%s' must be a finite float-range number"), FieldName);
        return false;
    }
    return true;
}

bool IsFiniteSecondaryMotionVector(const FVector& Value)
{
    return !Value.ContainsNaN()
        && FMath::Abs(Value.X) <= static_cast<double>(TNumericLimits<float>::Max())
        && FMath::Abs(Value.Y) <= static_cast<double>(TNumericLimits<float>::Max())
        && FMath::Abs(Value.Z) <= static_cast<double>(TNumericLimits<float>::Max());
}

bool TryGetFiniteSecondaryMotionVector(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    const FVector& DefaultValue,
    FVector& OutValue,
    FString& OutError)
{
    OutValue = DefaultValue;
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Params->TryGetArrayField(FieldName, Values) || !Values || Values->Num() != 3)
    {
        OutError = FString::Printf(TEXT("'%s' must be a three-number array"), FieldName);
        return false;
    }

    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!(*Values)[0].IsValid() || !(*Values)[0]->TryGetNumber(X)
        || !(*Values)[1].IsValid() || !(*Values)[1]->TryGetNumber(Y)
        || !(*Values)[2].IsValid() || !(*Values)[2]->TryGetNumber(Z))
    {
        OutError = FString::Printf(TEXT("'%s' must contain only numbers"), FieldName);
        return false;
    }

    OutValue = FVector(X, Y, Z);
    if (!IsFiniteSecondaryMotionVector(OutValue))
    {
        OutError = FString::Printf(TEXT("'%s' must contain finite float-range numbers"), FieldName);
        return false;
    }
    return true;
}

int32 CountGraphNodesByExactClass(const UEdGraph* Graph, const UClass* NodeClass)
{
    int32 Count = 0;
    if (!Graph || !NodeClass)
    {
        return Count;
    }
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node && Node->GetClass() == NodeClass)
        {
            ++Count;
        }
    }
    return Count;
}

UAnimGraphNode_Base* FindSecondaryMotionNodeByGuid(UEdGraph* Graph, const FGuid& NodeGuid)
{
    if (!Graph || !NodeGuid.IsValid())
    {
        return nullptr;
    }
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node && Node->NodeGuid == NodeGuid)
        {
            return Cast<UAnimGraphNode_Base>(Node);
        }
    }
    return nullptr;
}

bool AreGraphPinsLinked(const UEdGraphPin* A,const UEdGraphPin* B) { return PinKnown(A)&&PinKnown(B)&&A->LinkedTo.Contains(const_cast<UEdGraphPin*>(B))&&B->LinkedTo.Contains(const_cast<UEdGraphPin*>(A)); }

bool VerifyKawaiiNodeReadback(
    UAnimGraphNode_Base* NodeObject,
    const FName RootBone,
    const FVector& Gravity,
    const float Alpha,
    const float Damping,
    const float Stiffness,
    const float LimitAngle)
{
    if(!NodeObject||!Member(NodeObject,NodeObject->GetGraph())||!KawaiiLayoutSafe(NodeObject->GetClass())) { return false; }
    FStructProperty* NodeProperty = NodeObject
        ? FindFProperty<FStructProperty>(NodeObject->GetClass(), TEXT("Node"))
        : nullptr;
    if (!NodeProperty)
    {
        return false;
    }

    void* NodeData = NodeProperty->ContainerPtrToValuePtr<void>(NodeObject);
    UScriptStruct* NodeStruct = NodeProperty->Struct;
    FStructProperty* RootBoneProperty = FindFProperty<FStructProperty>(NodeStruct, TEXT("RootBone"));
    FNameProperty* RootBoneNameProperty = RootBoneProperty
        ? FindFProperty<FNameProperty>(RootBoneProperty->Struct, TEXT("BoneName"))
        : nullptr;
    FStructProperty* GravityProperty = FindFProperty<FStructProperty>(NodeStruct, TEXT("Gravity"));
    FFloatProperty* AlphaProperty = FindFProperty<FFloatProperty>(NodeStruct, TEXT("Alpha"));
    FStructProperty* PhysicsSettingsProperty = FindFProperty<FStructProperty>(NodeStruct, TEXT("PhysicsSettings"));
    if (!RootBoneProperty || !RootBoneNameProperty || !GravityProperty || !AlphaProperty || !PhysicsSettingsProperty)
    {
        return false;
    }

    void* RootBoneData = RootBoneProperty->ContainerPtrToValuePtr<void>(NodeData);
    void* PhysicsSettingsData = PhysicsSettingsProperty->ContainerPtrToValuePtr<void>(NodeData);
    FFloatProperty* DampingProperty = FindFProperty<FFloatProperty>(PhysicsSettingsProperty->Struct, TEXT("Damping"));
    FFloatProperty* StiffnessProperty = FindFProperty<FFloatProperty>(PhysicsSettingsProperty->Struct, TEXT("Stiffness"));
    FFloatProperty* WorldDampingLocationProperty = FindFProperty<FFloatProperty>(PhysicsSettingsProperty->Struct, TEXT("WorldDampingLocation"));
    FFloatProperty* WorldDampingRotationProperty = FindFProperty<FFloatProperty>(PhysicsSettingsProperty->Struct, TEXT("WorldDampingRotation"));
    FFloatProperty* RadiusProperty = FindFProperty<FFloatProperty>(PhysicsSettingsProperty->Struct, TEXT("Radius"));
    FFloatProperty* LimitAngleProperty = FindFProperty<FFloatProperty>(PhysicsSettingsProperty->Struct, TEXT("LimitAngle"));
    FFloatProperty* DummyBoneLengthProperty = FindFProperty<FFloatProperty>(NodeStruct, TEXT("DummyBoneLength"));
    const FVector* ReadGravity = GravityProperty->ContainerPtrToValuePtr<FVector>(NodeData);
    auto ReadBoolEquals = [NodeStruct, NodeData](const FName PropertyName, const bool Expected)
    {
        const FBoolProperty* Property = FindFProperty<FBoolProperty>(NodeStruct, PropertyName);
        return Property && Property->GetPropertyValue_InContainer(NodeData) == Expected;
    };
    auto ReadIntEquals = [NodeStruct, NodeData](const FName PropertyName, const int32 Expected)
    {
        const FIntProperty* Property = FindFProperty<FIntProperty>(NodeStruct, PropertyName);
        return Property && Property->GetPropertyValue_InContainer(NodeData) == Expected;
    };
    auto ReadVectorEquals = [NodeStruct, NodeData](const FName PropertyName, const FVector& Expected)
    {
        const FStructProperty* Property = FindFProperty<FStructProperty>(NodeStruct, PropertyName);
        const FVector* Value = Property && Property->Struct == TBaseStructure<FVector>::Get()
            ? Property->ContainerPtrToValuePtr<FVector>(NodeData)
            : nullptr;
        return Value && Value->Equals(Expected, 0.01);
    };
    auto ReadEnumEquals = [NodeStruct, NodeData](const FName PropertyName, const FString& Expected)
    {
        if (const FEnumProperty* Property = FindFProperty<FEnumProperty>(NodeStruct, PropertyName))
        {
            const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(NodeData);
            const int64 Value = Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
            return Property->GetEnum()
                && Property->GetEnum()->GetNameStringByValue(Value).Equals(Expected, ESearchCase::IgnoreCase);
        }
        if (const FByteProperty* Property = FindFProperty<FByteProperty>(NodeStruct, PropertyName))
        {
            return Property->Enum
                && Property->Enum->GetNameStringByValue(Property->GetPropertyValue_InContainer(NodeData)).Equals(
                    Expected,
                    ESearchCase::IgnoreCase);
        }
        return false;
    };
    auto ReadArrayEmpty = [NodeStruct, NodeData](const FName PropertyName)
    {
        const FArrayProperty* Property = FindFProperty<FArrayProperty>(NodeStruct, PropertyName);
        if (!Property)
        {
            return false;
        }
        FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(NodeData));
        return Helper.Num() == 0;
    };
    auto ReadObjectNull = [NodeStruct, NodeData](const FName PropertyName)
    {
        const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(NodeStruct, PropertyName);
        return Property && Property->GetObjectPropertyValue_InContainer(NodeData) == nullptr;
    };

    return DampingProperty && StiffnessProperty && WorldDampingLocationProperty
        && WorldDampingRotationProperty && RadiusProperty && LimitAngleProperty
        && DummyBoneLengthProperty && ReadGravity
        && RootBoneNameProperty->GetPropertyValue_InContainer(RootBoneData) == RootBone
        && ReadGravity->Equals(Gravity, 0.01)
        && FMath::IsNearlyZero(DummyBoneLengthProperty->GetPropertyValue_InContainer(NodeData), 0.001f)
        && FMath::IsNearlyEqual(AlphaProperty->GetPropertyValue_InContainer(NodeData), Alpha, 0.001f)
        && FMath::IsNearlyEqual(DampingProperty->GetPropertyValue_InContainer(PhysicsSettingsData), Damping, 0.001f)
        && FMath::IsNearlyEqual(StiffnessProperty->GetPropertyValue_InContainer(PhysicsSettingsData), Stiffness, 0.001f)
        && FMath::IsNearlyEqual(WorldDampingLocationProperty->GetPropertyValue_InContainer(PhysicsSettingsData), 0.8f, 0.001f)
        && FMath::IsNearlyEqual(WorldDampingRotationProperty->GetPropertyValue_InContainer(PhysicsSettingsData), 0.8f, 0.001f)
        && FMath::IsNearlyZero(RadiusProperty->GetPropertyValue_InContainer(PhysicsSettingsData), 0.001f)
        && FMath::IsNearlyEqual(LimitAngleProperty->GetPropertyValue_InContainer(PhysicsSettingsData), LimitAngle, 0.001f)
        && ReadEnumEquals(TEXT("BoneForwardAxis"), TEXT("X_Positive"))
        && ReadEnumEquals(TEXT("SimulationSpace"), TEXT("WorldSpace"))
        && ReadEnumEquals(TEXT("PlanarConstraint"), TEXT("None"))
        && ReadBoolEquals(TEXT("bUseLegacyGravity"), false)
        && ReadBoolEquals(TEXT("bUseDefaultGravityZProjectSetting"), false)
        && ReadBoolEquals(TEXT("bUseWorldSpaceGravity"), true)
        && ReadBoolEquals(TEXT("bEnableWind"), false)
        && ReadBoolEquals(TEXT("OverrideTargetFramerate"), true)
        && ReadBoolEquals(TEXT("bNeedWarmUp"), false)
        && ReadIntEquals(TEXT("TargetFramerate"), 60)
        && ReadIntEquals(TEXT("LODThreshold"), INDEX_NONE)
        && ReadVectorEquals(TEXT("SimpleExternalForce"), FVector::ZeroVector)
        && ReadArrayEmpty(TEXT("ExcludeBones"))
        && ReadArrayEmpty(TEXT("AdditionalRootBones"))
        && ReadArrayEmpty(TEXT("SphericalLimits"))
        && ReadArrayEmpty(TEXT("CapsuleLimits"))
        && ReadArrayEmpty(TEXT("BoxLimits"))
        && ReadArrayEmpty(TEXT("PlanarLimits"))
        && ReadArrayEmpty(TEXT("BoneConstraints"))
        && ReadObjectNull(TEXT("LimitsDataAsset"))
        && ReadObjectNull(TEXT("PhysicsAssetForLimits"))
        && ReadObjectNull(TEXT("BoneConstraintsDataAsset"));
}

bool IsBoneDescendantOrSame(const FReferenceSkeleton& Ref,const FName Root,const FName End)
{ if(!SkeletonChainSafe(Ref,Root,End)) { return false; }const int32 RootIndex=Ref.FindBoneIndex(Root);int32 Index=Ref.FindBoneIndex(End);while(Index!=INDEX_NONE) { if(Index==RootIndex) { return true; }Index=Ref.GetParentIndex(Index); }return false; }

UAnimGraphNode_Base* FindAnimGraphNodeByExactClass(UEdGraph* Graph, const UClass* NodeClass)
{
    if (!Graph || !NodeClass)
    {
        return nullptr;
    }
    for (UEdGraphNode* GraphNode : Graph->Nodes)
    {
        if (GraphNode && GraphNode->GetClass() == NodeClass)
        {
            return Cast<UAnimGraphNode_Base>(GraphNode);
        }
    }
    return nullptr;
}

UAnimGraphNode_Base* AddDynamicAnimGraphNode(UEdGraph* G,UClass* Class,const FVector2D& Position)
{
    if(!G||!Hold(G)||!KawaiiLayoutSafe(Class)) { return nullptr; }ModifyGraph(G);WriteIntent(TEXT("node_allocation_calls"));auto* N=NewObject<UAnimGraphNode_Base>(G,Class,NAME_None,RF_Transactional);Count(*CurrentResult,TEXT("node_allocation_returns"));if(!N||!Hold(N)) { return nullptr; }
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("class"),Class->GetPathName());Receipt->SetStringField(TEXT("path"),N->GetPathName());TArray<TSharedPtr<FJsonValue>> Rows;if(const TArray<TSharedPtr<FJsonValue>>* Old=nullptr;CurrentResult->Data->TryGetArrayField(TEXT("new_node_receipts"),Old)) { Rows=*Old; }Rows.Add(MakeShared<FJsonValueObject>(Receipt));CurrentResult->Data->SetArrayField(TEXT("new_node_receipts"),Rows);
    Count(*CurrentResult,TEXT("node_position_write_calls"),2);N->NodePosX=static_cast<int32>(Position.X);N->NodePosY=static_cast<int32>(Position.Y);Count(*CurrentResult,TEXT("graph_add_node_calls"));G->AddNode(N,true);CurrentResult->bChanged=true;if(!RefreshInventory()||!Member(N,G)) { return nullptr; }
    Count(*CurrentResult,TEXT("create_node_guid_calls"));N->CreateNewGuid();Receipt->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());Count(*CurrentResult,TEXT("post_placed_calls"));N->PostPlacedNewNode();if(!RefreshInventory()||!Member(N,G)) { return nullptr; }Count(*CurrentResult,TEXT("allocate_pins_calls"));N->AllocateDefaultPins();if(!RefreshInventory()||!Member(N,G)) { return nullptr; }Count(*CurrentResult,TEXT("reconstruct_calls"));N->ReconstructNode();if(!RefreshInventory()||!Member(N,G)) { return nullptr; }Receipt->SetBoolField(TEXT("lifecycle_completed"),true);return N;
}

bool SetReflectedFloat(UStruct* Struct, void* Container, const FName PropertyName, const float Value)
{
    if (FFloatProperty* Property = FindFProperty<FFloatProperty>(Struct, PropertyName))
    {
        Property->SetPropertyValue_InContainer(Container, Value);
        return true;
    }
    return false;
}

bool SetReflectedInt(UStruct* Struct, void* Container, const FName PropertyName, const int32 Value)
{
    if (FIntProperty* Property = FindFProperty<FIntProperty>(Struct, PropertyName))
    {
        Property->SetPropertyValue_InContainer(Container, Value);
        return true;
    }
    return false;
}

bool SetReflectedBool(UStruct* Struct, void* Container, const FName PropertyName, const bool Value)
{
    if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Struct, PropertyName))
    {
        Property->SetPropertyValue_InContainer(Container, Value);
        return true;
    }
    return false;
}

bool SetReflectedVector(UStruct* Struct, void* Container, const FName PropertyName, const FVector& Value)
{
    FStructProperty* Property = FindFProperty<FStructProperty>(Struct, PropertyName);
    if (!Property || Property->Struct != TBaseStructure<FVector>::Get())
    {
        return false;
    }
    *Property->ContainerPtrToValuePtr<FVector>(Container) = Value;
    return true;
}

bool SetReflectedEnum(UStruct* Struct, void* Container, const FName PropertyName, const FString& ValueName)
{
    if (FEnumProperty* Property = FindFProperty<FEnumProperty>(Struct, PropertyName))
    {
        UEnum* Enum = Property->GetEnum();
        const int64 EnumValue = Enum ? Enum->GetValueByNameString(ValueName) : INDEX_NONE;
        if (EnumValue == INDEX_NONE)
        {
            return false;
        }
        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
        Property->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
        return true;
    }
    if (FByteProperty* Property = FindFProperty<FByteProperty>(Struct, PropertyName))
    {
        const int64 EnumValue = Property->Enum ? Property->Enum->GetValueByNameString(ValueName) : INDEX_NONE;
        if (EnumValue == INDEX_NONE)
        {
            return false;
        }
        Property->SetPropertyValue_InContainer(Container, static_cast<uint8>(EnumValue));
        return true;
    }
    return false;
}

bool SetReflectedBoneReference(UStruct* Struct, void* Container, const FName PropertyName, const FName BoneName)
{
    FStructProperty* BoneReferenceProperty = FindFProperty<FStructProperty>(Struct, PropertyName);
    if (!BoneReferenceProperty)
    {
        return false;
    }
    void* BoneReferenceData = BoneReferenceProperty->ContainerPtrToValuePtr<void>(Container);
    FNameProperty* BoneNameProperty = FindFProperty<FNameProperty>(BoneReferenceProperty->Struct, TEXT("BoneName"));
    if (!BoneNameProperty)
    {
        return false;
    }
    BoneNameProperty->SetPropertyValue_InContainer(BoneReferenceData, BoneName);
    return true;
}

bool ClearReflectedArray(UStruct* Struct, void* Container, const FName PropertyName)
{
    FArrayProperty* Property = FindFProperty<FArrayProperty>(Struct, PropertyName);
    if (!Property)
    {
        return false;
    }
    FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Container));
    Helper.EmptyValues();
    return true;
}

bool ClearReflectedObject(UStruct* Struct, void* Container, const FName PropertyName)
{
    FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Struct, PropertyName);
    if (!Property)
    {
        return false;
    }
    Property->SetObjectPropertyValue_InContainer(Container, nullptr);
    return true;
}

bool ValidateKawaiiNodeSchema(UClass* NodeClass, FString& OutError)
{
    if(!KawaiiLayoutSafe(NodeClass)) { OutError=CurrentResult->Error;return false; }
    FStructProperty* NodeProperty = NodeClass ? FindFProperty<FStructProperty>(NodeClass, TEXT("Node")) : nullptr;
    if (!NodeProperty)
    {
        OutError = TEXT("Kawaii editor node has no reflected Node struct property");
        return false;
    }

    UScriptStruct* NodeStruct = NodeProperty->Struct;
    auto HasFloat = [](const UStruct* Struct, const FName PropertyName)
    {
        return FindFProperty<FFloatProperty>(Struct, PropertyName) != nullptr;
    };
    auto HasBool = [](const UStruct* Struct, const FName PropertyName)
    {
        return FindFProperty<FBoolProperty>(Struct, PropertyName) != nullptr;
    };
    auto HasInt = [](const UStruct* Struct, const FName PropertyName)
    {
        return FindFProperty<FIntProperty>(Struct, PropertyName) != nullptr;
    };
    auto HasVector = [](const UStruct* Struct, const FName PropertyName)
    {
        const FStructProperty* Property = FindFProperty<FStructProperty>(Struct, PropertyName);
        return Property && Property->Struct == TBaseStructure<FVector>::Get();
    };
    auto HasEnum = [](const UStruct* Struct, const FName PropertyName)
    {
        return FindFProperty<FEnumProperty>(Struct, PropertyName) != nullptr
            || FindFProperty<FByteProperty>(Struct, PropertyName) != nullptr;
    };
    auto HasArray = [](const UStruct* Struct, const FName PropertyName)
    {
        return FindFProperty<FArrayProperty>(Struct, PropertyName) != nullptr;
    };
    auto HasObject = [](const UStruct* Struct, const FName PropertyName)
    {
        return FindFProperty<FObjectPropertyBase>(Struct, PropertyName) != nullptr;
    };

    FStructProperty* RootBoneProperty = FindFProperty<FStructProperty>(NodeStruct, TEXT("RootBone"));
    FStructProperty* PhysicsSettingsProperty = FindFProperty<FStructProperty>(NodeStruct, TEXT("PhysicsSettings"));
    if (!RootBoneProperty || !FindFProperty<FNameProperty>(RootBoneProperty->Struct, TEXT("BoneName")))
    {
        OutError = TEXT("Kawaii Node.RootBone.BoneName reflection contract is unavailable");
        return false;
    }
    if (!PhysicsSettingsProperty
        || !HasFloat(PhysicsSettingsProperty->Struct, TEXT("Damping"))
        || !HasFloat(PhysicsSettingsProperty->Struct, TEXT("Stiffness"))
        || !HasFloat(PhysicsSettingsProperty->Struct, TEXT("WorldDampingLocation"))
        || !HasFloat(PhysicsSettingsProperty->Struct, TEXT("WorldDampingRotation"))
        || !HasFloat(PhysicsSettingsProperty->Struct, TEXT("Radius"))
        || !HasFloat(PhysicsSettingsProperty->Struct, TEXT("LimitAngle")))
    {
        OutError = TEXT("Kawaii Node.PhysicsSettings reflection contract is unavailable");
        return false;
    }
    if (!HasFloat(NodeStruct, TEXT("DummyBoneLength"))
        || !HasFloat(NodeStruct, TEXT("Alpha"))
        || !HasVector(NodeStruct, TEXT("Gravity"))
        || !HasVector(NodeStruct, TEXT("SimpleExternalForce"))
        || !HasEnum(NodeStruct, TEXT("BoneForwardAxis"))
        || !HasEnum(NodeStruct, TEXT("SimulationSpace"))
        || !HasEnum(NodeStruct, TEXT("PlanarConstraint"))
        || !HasBool(NodeStruct, TEXT("bUseLegacyGravity"))
        || !HasBool(NodeStruct, TEXT("bUseDefaultGravityZProjectSetting"))
        || !HasBool(NodeStruct, TEXT("bUseWorldSpaceGravity"))
        || !HasBool(NodeStruct, TEXT("bEnableWind"))
        || !HasBool(NodeStruct, TEXT("OverrideTargetFramerate"))
        || !HasBool(NodeStruct, TEXT("bNeedWarmUp"))
        || !HasInt(NodeStruct, TEXT("TargetFramerate"))
        || !HasInt(NodeStruct, TEXT("LODThreshold")))
    {
        OutError = TEXT("Kawaii scalar, vector, enum, or simulation flag reflection contract is unavailable");
        return false;
    }
    if (!HasArray(NodeStruct, TEXT("ExcludeBones"))
        || !HasArray(NodeStruct, TEXT("AdditionalRootBones"))
        || !HasArray(NodeStruct, TEXT("SphericalLimits"))
        || !HasArray(NodeStruct, TEXT("CapsuleLimits"))
        || !HasArray(NodeStruct, TEXT("BoxLimits"))
        || !HasArray(NodeStruct, TEXT("PlanarLimits"))
        || !HasArray(NodeStruct, TEXT("BoneConstraints"))
        || !HasObject(NodeStruct, TEXT("LimitsDataAsset"))
        || !HasObject(NodeStruct, TEXT("PhysicsAssetForLimits"))
        || !HasObject(NodeStruct, TEXT("BoneConstraintsDataAsset")))
    {
        OutError = TEXT("Kawaii collision or data-asset reflection contract is unavailable");
        return false;
    }
    return true;
}

bool ConfigureKawaiiNode(
    UAnimGraphNode_Base* NodeObject,
    const FName RootBone,
    const FVector& Gravity,
    const float Alpha,
    const float Damping,
    const float Stiffness,
    const float LimitAngle,
    FString& OutError)
{
    if(!NodeObject||!Member(NodeObject,NodeObject->GetGraph())||!KawaiiLayoutSafe(NodeObject->GetClass())) { OutError=CurrentResult->Error;return false; }
    FStructProperty* NodeProperty = NodeObject ? FindFProperty<FStructProperty>(NodeObject->GetClass(), TEXT("Node")) : nullptr;
    if (!NodeProperty)
    {
        OutError = TEXT("Failed to resolve Kawaii Node struct during mutation");
        return false;
    }

    void* NodeData = NodeProperty->ContainerPtrToValuePtr<void>(NodeObject);
    UScriptStruct* NodeStruct = NodeProperty->Struct;
    FStructProperty* PhysicsSettingsProperty = FindFProperty<FStructProperty>(NodeStruct, TEXT("PhysicsSettings"));
    void* PhysicsSettingsData = PhysicsSettingsProperty
        ? PhysicsSettingsProperty->ContainerPtrToValuePtr<void>(NodeData)
        : nullptr;

    WriteIntent(TEXT("kawaii_configuration_calls"));
    bool bApplied = PhysicsSettingsData != nullptr;
    bApplied &= SetReflectedBoneReference(NodeStruct, NodeData, TEXT("RootBone"), RootBone);
    bApplied &= SetReflectedFloat(NodeStruct, NodeData, TEXT("DummyBoneLength"), 0.0f);
    bApplied &= SetReflectedEnum(NodeStruct, NodeData, TEXT("BoneForwardAxis"), TEXT("X_Positive"));
    bApplied &= SetReflectedEnum(NodeStruct, NodeData, TEXT("SimulationSpace"), TEXT("WorldSpace"));
    bApplied &= SetReflectedVector(NodeStruct, NodeData, TEXT("Gravity"), Gravity);
    bApplied &= SetReflectedBool(NodeStruct, NodeData, TEXT("bUseLegacyGravity"), false);
    bApplied &= SetReflectedBool(NodeStruct, NodeData, TEXT("bUseDefaultGravityZProjectSetting"), false);
    bApplied &= SetReflectedBool(NodeStruct, NodeData, TEXT("bUseWorldSpaceGravity"), true);
    bApplied &= SetReflectedBool(NodeStruct, NodeData, TEXT("bEnableWind"), false);
    bApplied &= SetReflectedVector(NodeStruct, NodeData, TEXT("SimpleExternalForce"), FVector::ZeroVector);
    bApplied &= SetReflectedEnum(NodeStruct, NodeData, TEXT("PlanarConstraint"), TEXT("None"));
    bApplied &= SetReflectedBool(NodeStruct, NodeData, TEXT("OverrideTargetFramerate"), true);
    bApplied &= SetReflectedInt(NodeStruct, NodeData, TEXT("TargetFramerate"), 60);
    bApplied &= SetReflectedBool(NodeStruct, NodeData, TEXT("bNeedWarmUp"), false);
    bApplied &= SetReflectedFloat(NodeStruct, NodeData, TEXT("Alpha"), Alpha);
    bApplied &= SetReflectedInt(NodeStruct, NodeData, TEXT("LODThreshold"), INDEX_NONE);

    bApplied &= SetReflectedFloat(PhysicsSettingsProperty->Struct, PhysicsSettingsData, TEXT("Damping"), Damping);
    bApplied &= SetReflectedFloat(PhysicsSettingsProperty->Struct, PhysicsSettingsData, TEXT("Stiffness"), Stiffness);
    bApplied &= SetReflectedFloat(PhysicsSettingsProperty->Struct, PhysicsSettingsData, TEXT("WorldDampingLocation"), 0.8f);
    bApplied &= SetReflectedFloat(PhysicsSettingsProperty->Struct, PhysicsSettingsData, TEXT("WorldDampingRotation"), 0.8f);
    bApplied &= SetReflectedFloat(PhysicsSettingsProperty->Struct, PhysicsSettingsData, TEXT("Radius"), 0.0f);
    bApplied &= SetReflectedFloat(PhysicsSettingsProperty->Struct, PhysicsSettingsData, TEXT("LimitAngle"), LimitAngle);

    // Collision and data-asset inputs remain empty by contract for this
    // collision-free secondary-motion setup.
    bApplied &= ClearReflectedArray(NodeStruct, NodeData, TEXT("ExcludeBones"));
    bApplied &= ClearReflectedArray(NodeStruct, NodeData, TEXT("AdditionalRootBones"));
    bApplied &= ClearReflectedArray(NodeStruct, NodeData, TEXT("SphericalLimits"));
    bApplied &= ClearReflectedArray(NodeStruct, NodeData, TEXT("CapsuleLimits"));
    bApplied &= ClearReflectedArray(NodeStruct, NodeData, TEXT("BoxLimits"));
    bApplied &= ClearReflectedArray(NodeStruct, NodeData, TEXT("PlanarLimits"));
    bApplied &= ClearReflectedArray(NodeStruct, NodeData, TEXT("BoneConstraints"));
    bApplied &= ClearReflectedObject(NodeStruct, NodeData, TEXT("LimitsDataAsset"));
    bApplied &= ClearReflectedObject(NodeStruct, NodeData, TEXT("PhysicsAssetForLimits"));
    bApplied &= ClearReflectedObject(NodeStruct, NodeData, TEXT("BoneConstraintsDataAsset"));

    if (!bApplied)
    {
        OutError = TEXT("One or more required Kawaii reflected properties could not be applied");
    }
    return bApplied;
}
void AddMutationResultContract(
    const TSharedPtr<FJsonObject>& Result,
    bool bDryRun,
    bool bMutationPerformed,
    bool bChanged,
    const TSharedPtr<FJsonObject>& Before,
    const TSharedPtr<FJsonObject>& After,
    const TSharedPtr<FJsonObject>& Delta,
    bool bVerificationPassed,
    const FString& VerificationMethod,
    const TArray<FString>& VerificationChecks,
    bool bStrictSuccess = true)
{
    if (!Result.IsValid())
    {
        return;
    }

    bool bLegacySuccess = true;
    if (Result->HasTypedField<EJson::Boolean>(TEXT("success")))
    {
        bLegacySuccess = Result->GetBoolField(TEXT("success"));
    }

    Result->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1"));
    Result->SetStringField(TEXT("mode"), bDryRun ? TEXT("dry_run") : TEXT("apply"));
    Result->SetBoolField(TEXT("dry_run"), bDryRun);
    Result->SetBoolField(TEXT("DRY_RUN"), bDryRun);
    Result->SetBoolField(TEXT("changed"), bChanged);
    Result->SetBoolField(TEXT("mutation_performed"), bMutationPerformed);

    TSharedPtr<FJsonObject> Mutation = MakeShared<FJsonObject>();
    Mutation->SetBoolField(TEXT("attempted"), !bDryRun);
    Mutation->SetBoolField(TEXT("performed"), bMutationPerformed);
    Mutation->SetStringField(
        TEXT("state"),
        bMutationPerformed ? TEXT("performed") : TEXT("not_performed"));
    Result->SetObjectField(TEXT("mutation"), Mutation);

    if (Before.IsValid())
    {
        Result->SetObjectField(TEXT("before"), Before);
    }
    if (After.IsValid())
    {
        Result->SetObjectField(TEXT("after"), After);
    }

    TSharedPtr<FJsonObject> NormalizedDelta = Delta.IsValid()
        ? Delta
        : MakeShared<FJsonObject>();
    static const TCHAR* CanonicalCountFields[] =
    {
        TEXT("affected_count"),
        TEXT("created_count"),
        TEXT("updated_count"),
        TEXT("removed_count"),
        TEXT("skipped_count"),
        TEXT("failed_count")
    };
    for (const TCHAR* FieldName : CanonicalCountFields)
    {
        if (!NormalizedDelta->HasField(FieldName))
        {
            NormalizedDelta->SetField(FieldName, MakeShared<FJsonValueNull>());
        }
    }
    Result->SetObjectField(TEXT("delta"), NormalizedDelta);

    TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(
        TEXT("status"),
        bVerificationPassed ? TEXT("passed") : TEXT("failed"));
    Verification->SetStringField(TEXT("method"), VerificationMethod);
    Verification->SetBoolField(TEXT("before_observed"), Before.IsValid());
    Verification->SetBoolField(TEXT("after_observed"), After.IsValid());
    Verification->SetBoolField(TEXT("strict"), bStrictSuccess);
    TArray<TSharedPtr<FJsonValue>> CheckValues;
    CheckValues.Reserve(VerificationChecks.Num());
    for (const FString& Check : VerificationChecks)
    {
        CheckValues.Add(MakeShared<FJsonValueString>(Check));
    }
    Verification->SetArrayField(TEXT("checks"), CheckValues);
    Result->SetObjectField(TEXT("verification"), Verification);

    const bool bVerifiedSuccess = bLegacySuccess && bVerificationPassed;
    Result->SetBoolField(TEXT("verified_success"), bVerifiedSuccess);
    FString SuccessSemantics = TEXT("legacy_execution");
    if (bDryRun)
    {
        SuccessSemantics = bVerifiedSuccess ? TEXT("verified_dry_run") : TEXT("failed_dry_run");
    }
    else if (bVerifiedSuccess)
    {
        SuccessSemantics = TEXT("verified_postcondition");
    }
    else if (bStrictSuccess && bLegacySuccess)
    {
        SuccessSemantics = TEXT("postcondition_failed");
    }
    else if (!bLegacySuccess)
    {
        SuccessSemantics = TEXT("failed_execution");
    }
    Result->SetStringField(TEXT("success_semantics"), SuccessSemantics);

    if (!Result->HasField(TEXT("success")))
    {
        Result->SetBoolField(TEXT("success"), bLegacySuccess);
    }
    if (bStrictSuccess && bLegacySuccess && !bVerificationPassed)
    {
        Result->SetBoolField(TEXT("legacy_success"), true);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error_code"), TEXT("mutation_postcondition_failed"));
        if (!Result->HasField(TEXT("error")))
        {
            Result->SetStringField(
                TEXT("error"),
                TEXT("Mutation postcondition failed independent readback"));
        }
    }
}
FString GetBlueprintNodeStatusName(EBlueprintStatus Status)
{
    switch (Status)
    {
    case BS_Unknown:
        return TEXT("unknown");
    case BS_Dirty:
        return TEXT("dirty");
    case BS_Error:
        return TEXT("error");
    case BS_UpToDate:
        return TEXT("up_to_date");
    case BS_BeingCreated:
        return TEXT("being_created");
    case BS_UpToDateWithWarnings:
        return TEXT("up_to_date_with_warnings");
    default:
        return TEXT("unknown");
    }
}
TSharedPtr<FJsonObject> RunDemo(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) || BlueprintName.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString DynamicsType = GetStringParam(Params, TEXT("dynamics_type"), TEXT("anim_dynamics"));
    DynamicsType.TrimStartAndEndInline();
    DynamicsType.ToLowerInline();
    const bool bUseAnimDynamics = DynamicsType == TEXT("anim_dynamics") || DynamicsType == TEXT("animdynamics");
    const bool bUseKawaii = DynamicsType == TEXT("kawaii_physics") || DynamicsType == TEXT("kawaii");
    if (!bUseAnimDynamics && !bUseKawaii)
    {
        return CreateErrorResponse(TEXT("dynamics_type must be anim_dynamics or kawaii_physics"));
    }

    FString RootBoneText = GetStringParam(Params, TEXT("root_bone"));
    FString ChainEndBoneText = GetStringParam(Params, TEXT("chain_end_bone"));
    RootBoneText.TrimStartAndEndInline();
    ChainEndBoneText.TrimStartAndEndInline();
    if (RootBoneText.IsEmpty() || ChainEndBoneText.IsEmpty())
    {
        return CreateErrorResponse(TEXT("root_bone and chain_end_bone are required"));
    }
    FName RootBone;
    FName ChainEndBone;
    FString InputError;
    if (!TryParseSecondaryMotionBoneName(RootBoneText, TEXT("root_bone"), RootBone, InputError)
        || !TryParseSecondaryMotionBoneName(ChainEndBoneText, TEXT("chain_end_bone"), ChainEndBone, InputError))
    {
        return CreateErrorResponse(InputError);
    }

    double AlphaValue = 1.0;
    double AngularLimitValue = 60.0;
    double AnimLinearDampingValue = 0.75;
    double AnimAngularDampingValue = 0.75;
    double KawaiiDampingValue = 0.08;
    double KawaiiStiffnessValue = 0.03;
    if (!TryGetFiniteSecondaryMotionNumber(Params, TEXT("alpha"), 1.0, AlphaValue, InputError)
        || !TryGetFiniteSecondaryMotionNumber(Params, TEXT("angular_limit_degrees"), 60.0, AngularLimitValue, InputError)
        || !TryGetFiniteSecondaryMotionNumber(Params, TEXT("linear_damping"), 0.75, AnimLinearDampingValue, InputError)
        || !TryGetFiniteSecondaryMotionNumber(Params, TEXT("angular_damping"), 0.75, AnimAngularDampingValue, InputError)
        || !TryGetFiniteSecondaryMotionNumber(Params, TEXT("kawaii_damping"), 0.08, KawaiiDampingValue, InputError)
        || !TryGetFiniteSecondaryMotionNumber(Params, TEXT("kawaii_stiffness"), 0.03, KawaiiStiffnessValue, InputError))
    {
        return CreateErrorResponse(InputError);
    }

    const float Alpha = FMath::Clamp(static_cast<float>(AlphaValue), 0.0f, 1.0f);
    const float AngularLimit = FMath::Clamp(static_cast<float>(AngularLimitValue), 0.0f, 90.0f);
    const float AnimLinearDamping = FMath::Max(static_cast<float>(AnimLinearDampingValue), 0.7f);
    const float AnimAngularDamping = FMath::Max(static_cast<float>(AnimAngularDampingValue), 0.7f);
    const float KawaiiDamping = FMath::Max(static_cast<float>(KawaiiDampingValue), 0.0f);
    const float KawaiiStiffness = FMath::Max(static_cast<float>(KawaiiStiffnessValue), 0.0f);

    FVector Gravity(0.0f, 0.0f, -980.0f);
    FVector BodyExtents(5.0f, 5.0f, 30.0f);
    if (!TryGetFiniteSecondaryMotionVector(Params, TEXT("gravity"), Gravity, Gravity, InputError)
        || !TryGetFiniteSecondaryMotionVector(Params, TEXT("body_extents"), BodyExtents, BodyExtents, InputError))
    {
        return CreateErrorResponse(InputError);
    }
    BodyExtents.X = FMath::Max(1.0, BodyExtents.X);
    BodyExtents.Y = FMath::Max(1.0, BodyExtents.Y);
    BodyExtents.Z = FMath::Max(1.0, BodyExtents.Z);

    const bool bReplaceExisting = GetBoolParam(Params, TEXT("replace_existing"), true);
    const bool bAllowNonSample = GetBoolParam(Params, TEXT("allow_non_sample"), false);
    const bool bCompileRequested = GetBoolParam(Params, TEXT("compile"), true);
    const bool bSaveRequested = GetBoolParam(Params, TEXT("save"), true);
    if (bSaveRequested && !bCompileRequested)
    {
        return CreateErrorResponse(
            TEXT("save=true requires compile=true for verified secondary-motion authoring"));
    }

    UBlueprint* Blueprint = FindBlueprint(BlueprintName);
    UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint);
    if (!AnimBlueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Animation Blueprint not found: %s"), *BlueprintName));
    }
    if (!bAllowNonSample && !IsAllowedSecondaryMotionTarget(AnimBlueprint->GetPathName()))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Refusing to modify non-sample Anim Blueprint '%s'. Pass allow_non_sample=true for an intentional project-content edit."),
            *AnimBlueprint->GetPathName()));
    }
    if (!AnimBlueprint->TargetSkeleton)
    {
        return CreateErrorResponse(TEXT("Animation Blueprint has no TargetSkeleton"));
    }

    if(!Hold(AnimBlueprint->TargetSkeleton.Get())) { return Failure(CurrentResult->Error); }
    const FReferenceSkeleton& ReferenceSkeleton = AnimBlueprint->TargetSkeleton->GetReferenceSkeleton();
    if(!SkeletonChainSafe(ReferenceSkeleton,RootBone,ChainEndBone)) { return Failure(CurrentResult->Error); }
    if (ReferenceSkeleton.FindBoneIndex(RootBone) == INDEX_NONE || ReferenceSkeleton.FindBoneIndex(ChainEndBone) == INDEX_NONE)
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Requested chain bones were not found. root='%s' end='%s'"),
            *RootBoneText,
            *ChainEndBoneText));
    }
    if (!IsBoneDescendantOrSame(ReferenceSkeleton, RootBone, ChainEndBone))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("chain_end_bone '%s' is not a descendant of root_bone '%s'"),
            *ChainEndBoneText,
            *RootBoneText));
    }

    UClass* KawaiiNodeClass = nullptr;
    if (bUseKawaii)
    {
        Count(*CurrentResult,TEXT("optional_module_load_calls"));
        if (!FModuleManager::Get().LoadModulePtr<IModuleInterface>(KawaiiEditorModuleName))
        {
            TSharedPtr<FJsonObject> Error = CreateErrorResponse(TEXT("KawaiiPhysicsEd module is unavailable; enable/install the KawaiiPhysics plugin"));
            Error->SetBoolField(TEXT("plugin_available"), false);
            return Error;
        }
        Count(*CurrentResult,TEXT("optional_class_load_calls"));
        KawaiiNodeClass = StaticLoadClass(UAnimGraphNode_Base::StaticClass(), nullptr, KawaiiNodeClassPath);
        FString SchemaError;
        if (!KawaiiNodeClass || !ValidateKawaiiNodeSchema(KawaiiNodeClass, SchemaError))
        {
            TSharedPtr<FJsonObject> Error = CreateErrorResponse(
                SchemaError.IsEmpty() ? TEXT("Kawaii AnimGraph editor node class is unavailable") : SchemaError);
            Error->SetBoolField(TEXT("plugin_available"), KawaiiNodeClass != nullptr);
            return Error;
        }
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
            TEXT("Target graph is not an AnimationGraph: %s"),
            *TargetGraph->GetName()));
    }

    UAnimGraphNode_Root* RootNode = FindFirstNodeOfType<UAnimGraphNode_Root>(TargetGraph);
    if (!RootNode)
    {
        return CreateErrorResponse(TEXT("AnimGraph root node was not found"));
    }

    const int32 NodeCountBefore = TargetGraph->Nodes.Num();
    const FGuid TargetGraphGuid = TargetGraph->GraphGuid;
    TSharedPtr<FJsonObject> Before = MakeShared<FJsonObject>();
    Before->SetStringField(TEXT("blueprint"), AnimBlueprint->GetPathName());
    Before->SetStringField(TEXT("graph"), TargetGraph->GetName());
    Before->SetStringField(TEXT("graph_id"), TargetGraph->GraphGuid.ToString());
    Before->SetNumberField(TEXT("node_count"), NodeCountBefore);
    Before->SetNumberField(TEXT("reference_pose_node_count"), CountGraphNodesByExactClass(TargetGraph, UAnimGraphNode_LocalRefPose::StaticClass()));
    Before->SetNumberField(TEXT("local_to_component_node_count"), CountGraphNodesByExactClass(TargetGraph, UAnimGraphNode_LocalToComponentSpace::StaticClass()));
    Before->SetNumberField(TEXT("dynamics_node_count"), CountGraphNodesByExactClass(
        TargetGraph,
        bUseAnimDynamics ? UAnimGraphNode_AnimDynamics::StaticClass() : KawaiiNodeClass));
    Before->SetNumberField(TEXT("component_to_local_node_count"), CountGraphNodesByExactClass(TargetGraph, UAnimGraphNode_ComponentToLocalSpace::StaticClass()));
    Before->SetBoolField(TEXT("package_dirty"), AnimBlueprint->GetOutermost() && AnimBlueprint->GetOutermost()->IsDirty());

    auto CreatePartialMutationError = [&](const FString& Message) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Error = CreateErrorResponse(Message);
        Error->SetStringField(TEXT("command"), TEXT("ensure_anim_graph_secondary_motion_chain"));
        Error->SetBoolField(TEXT("partial_target_modified"), true);
        Error->SetBoolField(TEXT("do_not_retry"), true);
        Error->SetBoolField(TEXT("verification_pass"), false);

        TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();
        After->SetStringField(TEXT("blueprint"), AnimBlueprint->GetPathName());
        After->SetStringField(TEXT("graph"), TargetGraph->GetName());
        After->SetStringField(TEXT("graph_id"), TargetGraphGuid.ToString());
        After->SetNumberField(TEXT("node_count"), TargetGraph->Nodes.Num());
        After->SetBoolField(TEXT("package_dirty"), AnimBlueprint->GetOutermost() && AnimBlueprint->GetOutermost()->IsDirty());

        const int32 NodeCountChange = TargetGraph->Nodes.Num() - NodeCountBefore;
        TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
        Delta->SetNumberField(TEXT("affected_count"), FMath::Max(1, FMath::Abs(NodeCountChange)));
        Delta->SetNumberField(TEXT("created_count"), FMath::Max(0, NodeCountChange));
        Delta->SetNumberField(TEXT("updated_count"), 1);
        Delta->SetNumberField(TEXT("removed_count"), FMath::Max(0, -NodeCountChange));
        Delta->SetNumberField(TEXT("skipped_count"), 0);
        Delta->SetNumberField(TEXT("failed_count"), 1);
        Delta->SetNumberField(TEXT("node_count_change"), NodeCountChange);

        AddGraphField(Error, Blueprint, TargetGraph);
        AddMutationResultContract(
            Error,
            false,
            true,
            true,
            Before,
            After,
            Delta,
            false,
            TEXT("partial_graph_state_after_failed_secondary_motion_authoring"),
            {
                TEXT("command_stopped_after_partial_graph_edit"),
                TEXT("caller_must_inspect_or_undo_before_retry")
            });
        return Error;
    };

    if(!PrepareBeforeWriters(Blueprint,TargetGraph,RootNode,KawaiiNodeClass)) { return Failure(CurrentResult->Error); }
    ModifyGraph(TargetGraph);
    ModifyNode(RootNode);

    bool bCreatedRefPose = false;
    UAnimGraphNode_LocalRefPose* RefPoseNode = FindFirstNodeOfType<UAnimGraphNode_LocalRefPose>(TargetGraph);
    if (!RefPoseNode)
    {
        RefPoseNode = AddAnimGraphNodeToGraph<UAnimGraphNode_LocalRefPose>(TargetGraph, FVector2D(-960.0f, 0.0f));
        bCreatedRefPose = RefPoseNode != nullptr;
    }
    if(Incomplete()) { return CreatePartialMutationError(CurrentResult->Error); }
    bool bCreatedLocalToComponent = false;
    UAnimGraphNode_LocalToComponentSpace* LocalToComponentNode = FindFirstNodeOfType<UAnimGraphNode_LocalToComponentSpace>(TargetGraph);
    if (!LocalToComponentNode)
    {
        LocalToComponentNode = AddAnimGraphNodeToGraph<UAnimGraphNode_LocalToComponentSpace>(TargetGraph, FVector2D(-720.0f, 0.0f));
        bCreatedLocalToComponent = LocalToComponentNode != nullptr;
    }
    if(Incomplete()) { return CreatePartialMutationError(CurrentResult->Error); }
    bool bCreatedComponentToLocal = false;
    UAnimGraphNode_ComponentToLocalSpace* ComponentToLocalNode = FindFirstNodeOfType<UAnimGraphNode_ComponentToLocalSpace>(TargetGraph);
    if (!ComponentToLocalNode)
    {
        ComponentToLocalNode = AddAnimGraphNodeToGraph<UAnimGraphNode_ComponentToLocalSpace>(TargetGraph, FVector2D(-240.0f, 0.0f));
        bCreatedComponentToLocal = ComponentToLocalNode != nullptr;
    }
    if (!RefPoseNode || !LocalToComponentNode || !ComponentToLocalNode)
    {
        return CreatePartialMutationError(TEXT("Failed to create one or more common secondary-motion pose nodes"));
    }

    bool bCreatedDynamicsNode = false;
    UAnimGraphNode_Base* DynamicsNode = nullptr;
    if (bUseAnimDynamics)
    {
        UAnimGraphNode_AnimDynamics* AnimDynamicsNode = FindFirstNodeOfType<UAnimGraphNode_AnimDynamics>(TargetGraph);
        if (!AnimDynamicsNode)
        {
            AnimDynamicsNode = AddAnimGraphNodeToGraph<UAnimGraphNode_AnimDynamics>(TargetGraph, FVector2D(-480.0f, 0.0f));
            bCreatedDynamicsNode = AnimDynamicsNode != nullptr;
        }
        if (!AnimDynamicsNode)
        {
            return CreatePartialMutationError(TEXT("Failed to create AnimDynamics node"));
        }

        ModifyNode(AnimDynamicsNode);
        auto* Runtime=GetMutableDynamics(AnimDynamicsNode);if(!Runtime) { return CreatePartialMutationError(CurrentResult->Error); }
        WriteIntent(TEXT("dynamics_configuration_calls"));
        Runtime->BoundBone.BoneName = RootBone;
        Runtime->ChainEnd.BoneName = ChainEndBone;
        Runtime->bChain = true;
        Runtime->SimulationSpace = AnimPhysSimSpaceType::World;
        Runtime->bUseGravityOverride = true;
        Runtime->bGravityOverrideInSimSpace = false;
        Runtime->GravityOverride = Gravity;
        Runtime->GravityScale = 1.0f;
        Runtime->bUsePlanarLimit = false;
        Runtime->bUseSphericalLimits = false;
        Runtime->bEnableWind = false;
        Runtime->bDoUpdate = true;
        Runtime->bDoEval = true;
        Runtime->bOverrideLinearDamping = true;
        Runtime->LinearDampingOverride = AnimLinearDamping;
        Runtime->bOverrideAngularDamping = true;
        Runtime->AngularDampingOverride = AnimAngularDamping;
        Runtime->NumSolverIterationsPreUpdate = 8;
        Runtime->NumSolverIterationsPostUpdate = 2;
        Runtime->Alpha = Alpha;
        Runtime->LODThreshold = INDEX_NONE;
        Count(*CurrentResult,TEXT("update_chain_physics_body_calls"));Runtime->UpdateChainPhysicsBodyDefinitions(ReferenceSkeleton);if(!GetMutableDynamics(AnimDynamicsNode)) { return CreatePartialMutationError(CurrentResult->Error); }
        for (FAnimPhysBodyDefinition& BodyDefinition : Runtime->PhysicsBodyDefinitions)
        {
            BodyDefinition.BoxExtents = BodyExtents;
            BodyDefinition.CollisionType = AnimPhysCollisionType::CoM;
            BodyDefinition.ConstraintSetup.LinearXLimitType = AnimPhysLinearConstraintType::Limited;
            BodyDefinition.ConstraintSetup.LinearYLimitType = AnimPhysLinearConstraintType::Limited;
            BodyDefinition.ConstraintSetup.LinearZLimitType = AnimPhysLinearConstraintType::Limited;
            BodyDefinition.ConstraintSetup.LinearAxesMin = FVector::ZeroVector;
            BodyDefinition.ConstraintSetup.LinearAxesMax = FVector::ZeroVector;
            BodyDefinition.ConstraintSetup.bLinearFullyLocked = true;
            BodyDefinition.ConstraintSetup.AngularConstraintType = AnimPhysAngularConstraintType::Cone;
            BodyDefinition.ConstraintSetup.ConeAngle = AngularLimit;
        }
        DynamicsNode = AnimDynamicsNode;
    }
    else
    {
        DynamicsNode = FindAnimGraphNodeByExactClass(TargetGraph, KawaiiNodeClass);
        if (!DynamicsNode)
        {
            DynamicsNode = AddDynamicAnimGraphNode(TargetGraph, KawaiiNodeClass, FVector2D(-480.0f, 0.0f));
            bCreatedDynamicsNode = DynamicsNode != nullptr;
        }
        if (!DynamicsNode)
        {
            return CreatePartialMutationError(TEXT("Failed to create KawaiiPhysics AnimGraph node"));
        }
        ModifyNode(DynamicsNode);
        FString KawaiiError;
        if (!ConfigureKawaiiNode(DynamicsNode, RootBone, Gravity, Alpha, KawaiiDamping, KawaiiStiffness, AngularLimit, KawaiiError))
        {
            return CreatePartialMutationError(KawaiiError);
        }
    }

    if(Incomplete()||!Member(DynamicsNode,TargetGraph)||!Member(RootNode,TargetGraph)) { return CreatePartialMutationError(CurrentResult->Error); }
    Count(*CurrentResult,TEXT("node_position_write_calls"),2);
    DynamicsNode->NodePosX = -480.0f;
    DynamicsNode->NodePosY = 0.0f;
    Count(*CurrentResult,TEXT("reconstruct_calls"));DynamicsNode->ReconstructNode();
    if(!RefreshInventory()||!Member(DynamicsNode,TargetGraph)||!Member(RootNode,TargetGraph)) { return CreatePartialMutationError(CurrentResult->Error); }
    Count(*CurrentResult,TEXT("root_position_write_calls"),2);
    RootNode->NodePosX = 0.0f;
    RootNode->NodePosY = 0.0f;

    if (UEdGraphPin* AlphaPin = FindPin(DynamicsNode, TEXT("Alpha"), EGPD_Input))
    {
        if (TargetGraph->GetSchema())
        {
            const FEdGraphPinReference AlphaSetterRef(AlphaPin);WriteIntent(TEXT("try_set_default_calls"));TargetGraph->GetSchema()->TrySetDefaultValue(*AlphaPin, FString::SanitizeFloat(Alpha));
            if(!AfterDefaultSetter(AlphaPin,AlphaSetterRef)) { return CreatePartialMutationError(CurrentResult->Error); }
        }
        const FEdGraphPinReference AlphaCallbackRef(AlphaPin);Count(*CurrentResult,TEXT("pin_default_changed_calls"));DynamicsNode->PinDefaultValueChanged(AlphaPin);
        if(!AfterDefaultSetter(AlphaPin,AlphaCallbackRef)) { return CreatePartialMutationError(CurrentResult->Error); }
    }

    UEdGraphPin* RefPoseOutput = FindFirstPosePin(RefPoseNode, EGPD_Output);
    UEdGraphPin* LocalToComponentInput = FindFirstPosePin(LocalToComponentNode, EGPD_Input, { TEXT("Pose") });
    UEdGraphPin* LocalToComponentOutput = FindFirstComponentPosePin(LocalToComponentNode, EGPD_Output);
    UEdGraphPin* DynamicsInput = FindFirstComponentPosePin(DynamicsNode, EGPD_Input, { TEXT("ComponentPose") });
    UEdGraphPin* DynamicsOutput = FindFirstComponentPosePin(DynamicsNode, EGPD_Output);
    UEdGraphPin* ComponentToLocalInput = FindFirstComponentPosePin(ComponentToLocalNode, EGPD_Input, { TEXT("ComponentPose") });
    UEdGraphPin* ComponentToLocalOutput = FindFirstPosePin(ComponentToLocalNode, EGPD_Output);
    UEdGraphPin* RootInput = FindFirstPosePin(RootNode, EGPD_Input, { TEXT("Result") });
    if (!RefPoseOutput || !LocalToComponentInput || !LocalToComponentOutput || !DynamicsInput || !DynamicsOutput
        || !ComponentToLocalInput || !ComponentToLocalOutput || !RootInput)
    {
        return CreatePartialMutationError(TEXT("Failed to resolve one or more secondary-motion pose pins"));
    }

    struct FSecondaryMotionConnection
    {
        UEdGraphPin* OutputPin = nullptr;
        UEdGraphPin* InputPin = nullptr;
    };
    const TArray<FSecondaryMotionConnection> DesiredConnections = {
        { RefPoseOutput, LocalToComponentInput },
        { LocalToComponentOutput, DynamicsInput },
        { DynamicsOutput, ComponentToLocalInput },
        { ComponentToLocalOutput, RootInput }
    };
    TArray<FEdGraphPinReference> PoseRefs;for(const auto& Pair:DesiredConnections) { PoseRefs.Emplace(Pair.OutputPin);PoseRefs.Emplace(Pair.InputPin); }
    auto AllPosePinsCurrent=[&]() -> bool { if(!RefreshInventory()) { return false; }int32 I=0;for(const auto& Pair:DesiredConnections) { if(PoseRefs[I++].Get()!=Pair.OutputPin||PoseRefs[I++].Get()!=Pair.InputPin||!PinKnown(Pair.OutputPin)||!PinKnown(Pair.InputPin)) { NoteUnsafe(TEXT("Complete eight-pin identity retired across schema callback"));return false; } }return true; };
    const UEdGraphSchema* GraphSchema = TargetGraph->GetSchema();
    if (!GraphSchema)
    {
        return CreatePartialMutationError(TEXT("Failed to get AnimationGraph schema before connection preflight"));
    }
    for (const FSecondaryMotionConnection& Connection : DesiredConnections)
    {
        if(!PinKnown(Connection.OutputPin)||!PinKnown(Connection.InputPin)||!Hold(GraphSchema)) { return CreatePartialMutationError(CurrentResult->Error); }
        if (AreGraphPinsLinked(Connection.OutputPin, Connection.InputPin))
        {
            continue;
        }
        if (Connection.InputPin->LinkedTo.Num() > 0 && !bReplaceExisting)
        {
            return CreatePartialMutationError(FString::Printf(
                TEXT("AnimGraph target pose pin already has a different link: %s"),
                *Connection.InputPin->PinName.ToString()));
        }
        const FEdGraphPinReference SourceRef(Connection.OutputPin),TargetRef(Connection.InputPin);Count(*CurrentResult,TEXT("can_create_connection_calls"));
        const FPinConnectionResponse ConnectionResponse = GraphSchema->CanCreateConnection(
            Connection.OutputPin,
            Connection.InputPin);
        if(!RefreshInventory()||SourceRef.Get()!=Connection.OutputPin||TargetRef.Get()!=Connection.InputPin||!PinKnown(Connection.OutputPin)||!PinKnown(Connection.InputPin)) { return CreatePartialMutationError(CurrentResult->Error); }
        if(!AllPosePinsCurrent()) { return CreatePartialMutationError(CurrentResult->Error); }
        if (!ConnectionResponse.CanSafeConnect()
            && (!bReplaceExisting || ConnectionResponse.Response == CONNECT_RESPONSE_DISALLOW))
        {
            return CreatePartialMutationError(FString::Printf(
                TEXT("AnimationGraph connection preflight failed for %s -> %s: %s"),
                *Connection.OutputPin->PinName.ToString(),
                *Connection.InputPin->PinName.ToString(),
                *ConnectionResponse.Message.ToString()));
        }
    }

    bool bLinksChanged = false;
    bool bChangedThisLink = false;
    FString LinkError;
    TSharedPtr<FJsonObject> LinkErrorObject;
    auto EnsureLink = [&](UEdGraphPin* OutputPin, UEdGraphPin* InputPin) -> bool
    {
        if (!EnsureAnimGraphConnection(TargetGraph, OutputPin, InputPin, bReplaceExisting, bChangedThisLink, LinkError, &LinkErrorObject))
        {
            return false;
        }
        if(!AllPosePinsCurrent()) { LinkError=CurrentResult->Error;return false; }
        bLinksChanged |= bChangedThisLink;
        return true;
    };

    if (!EnsureLink(RefPoseOutput, LocalToComponentInput)
        || !EnsureLink(LocalToComponentOutput, DynamicsInput)
        || !EnsureLink(DynamicsOutput, ComponentToLocalInput)
        || !EnsureLink(ComponentToLocalOutput, RootInput))
    {
        if (LinkErrorObject.IsValid())
        {
            FString StructuredLinkError;
            if (!LinkErrorObject->TryGetStringField(TEXT("error"), StructuredLinkError))
            {
                StructuredLinkError = LinkError;
            }
            return CreatePartialMutationError(StructuredLinkError);
        }
        return CreatePartialMutationError(LinkError);
    }

    WriteIntent(TEXT("mark_structurally_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);CurrentResult->Data->SetBoolField(TEXT("implicit_skeleton_regeneration_possible"),true);
    if(!RefreshInventory()||!Member(RefPoseNode,TargetGraph)||!Member(LocalToComponentNode,TargetGraph)||!Member(DynamicsNode,TargetGraph)||!Member(ComponentToLocalNode,TargetGraph)||!Member(RootNode,TargetGraph)) { return CreatePartialMutationError(CurrentResult->Error); }
    Count(*CurrentResult,TEXT("notify_graph_changed_calls"));TargetGraph->NotifyGraphChanged();
    if(!RefreshInventory()||!Member(RefPoseNode,TargetGraph)||!Member(LocalToComponentNode,TargetGraph)||!Member(DynamicsNode,TargetGraph)||!Member(ComponentToLocalNode,TargetGraph)||!Member(RootNode,TargetGraph)) { return CreatePartialMutationError(CurrentResult->Error); }

    const FGuid RefPoseNodeGuid = RefPoseNode->NodeGuid;
    const FGuid LocalToComponentNodeGuid = LocalToComponentNode->NodeGuid;
    const FGuid DynamicsNodeGuid = DynamicsNode->NodeGuid;
    const FGuid ComponentToLocalNodeGuid = ComponentToLocalNode->NodeGuid;
    const FGuid RootNodeGuid = RootNode->NodeGuid;

    bool bCompiled = false;
    bool bCompileSucceeded = true;
    if (bCompileRequested)
    {
        if(!CompileCurrent(Blueprint)) { return CreatePartialMutationError(CurrentResult->Error); }
        bCompiled = true;
        bCompileSucceeded = Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;
    }

    UEdGraph* ReadbackGraph = FindBlueprintGraph(Blueprint, TargetGraphGuid.ToString(), FString(), FString());
    UAnimGraphNode_LocalRefPose* ReadbackRefPoseNode = Cast<UAnimGraphNode_LocalRefPose>(FindSecondaryMotionNodeByGuid(ReadbackGraph, RefPoseNodeGuid));
    UAnimGraphNode_LocalToComponentSpace* ReadbackLocalToComponentNode = Cast<UAnimGraphNode_LocalToComponentSpace>(FindSecondaryMotionNodeByGuid(ReadbackGraph, LocalToComponentNodeGuid));
    UAnimGraphNode_Base* ReadbackDynamicsNode = FindSecondaryMotionNodeByGuid(ReadbackGraph, DynamicsNodeGuid);
    UAnimGraphNode_ComponentToLocalSpace* ReadbackComponentToLocalNode = Cast<UAnimGraphNode_ComponentToLocalSpace>(FindSecondaryMotionNodeByGuid(ReadbackGraph, ComponentToLocalNodeGuid));
    UAnimGraphNode_Root* ReadbackRootNode = Cast<UAnimGraphNode_Root>(FindSecondaryMotionNodeByGuid(ReadbackGraph, RootNodeGuid));

    UEdGraphPin* ReadbackRefPoseOutput = FindFirstPosePin(ReadbackRefPoseNode, EGPD_Output);
    UEdGraphPin* ReadbackLocalToComponentInput = FindFirstPosePin(ReadbackLocalToComponentNode, EGPD_Input, { TEXT("Pose") });
    UEdGraphPin* ReadbackLocalToComponentOutput = FindFirstComponentPosePin(ReadbackLocalToComponentNode, EGPD_Output);
    UEdGraphPin* ReadbackDynamicsInput = FindFirstComponentPosePin(ReadbackDynamicsNode, EGPD_Input, { TEXT("ComponentPose") });
    UEdGraphPin* ReadbackDynamicsOutput = FindFirstComponentPosePin(ReadbackDynamicsNode, EGPD_Output);
    UEdGraphPin* ReadbackComponentToLocalInput = FindFirstComponentPosePin(ReadbackComponentToLocalNode, EGPD_Input, { TEXT("ComponentPose") });
    UEdGraphPin* ReadbackComponentToLocalOutput = FindFirstPosePin(ReadbackComponentToLocalNode, EGPD_Output);
    UEdGraphPin* ReadbackRootInput = FindFirstPosePin(ReadbackRootNode, EGPD_Input, { TEXT("Result") });
    const bool bTopologyReadbackPassed =
        AreGraphPinsLinked(ReadbackRefPoseOutput, ReadbackLocalToComponentInput)
        && AreGraphPinsLinked(ReadbackLocalToComponentOutput, ReadbackDynamicsInput)
        && AreGraphPinsLinked(ReadbackDynamicsOutput, ReadbackComponentToLocalInput)
        && AreGraphPinsLinked(ReadbackComponentToLocalOutput, ReadbackRootInput);

    bool bSettingsReadbackPassed = false;
    if (bUseAnimDynamics)
    {
        auto* ReadbackAnimDynamics = Cast<UAnimGraphNode_AnimDynamics>(ReadbackDynamicsNode);
        auto* ReadbackRuntime=ReadbackAnimDynamics?GetMutableDynamics(ReadbackAnimDynamics):nullptr;
        bSettingsReadbackPassed = ReadbackRuntime
            && ReadbackRuntime->BoundBone.BoneName == RootBone
            && ReadbackRuntime->ChainEnd.BoneName == ChainEndBone
            && ReadbackRuntime->bChain
            && ReadbackRuntime->SimulationSpace == AnimPhysSimSpaceType::World
            && ReadbackRuntime->bUseGravityOverride
            && ReadbackRuntime->GravityOverride.Equals(Gravity, 0.01)
            && ReadbackRuntime->bOverrideLinearDamping
            && FMath::IsNearlyEqual(ReadbackRuntime->LinearDampingOverride, AnimLinearDamping, 0.001f)
            && ReadbackRuntime->bOverrideAngularDamping
            && FMath::IsNearlyEqual(ReadbackRuntime->AngularDampingOverride, AnimAngularDamping, 0.001f)
            && FMath::IsNearlyEqual(ReadbackRuntime->Alpha, Alpha, 0.001f)
            && ReadbackRuntime->PhysicsBodyDefinitions.Num() > 0;
        if (bSettingsReadbackPassed)
        {
            for (const FAnimPhysBodyDefinition& BodyDefinition : ReadbackRuntime->PhysicsBodyDefinitions)
            {
                bSettingsReadbackPassed = bSettingsReadbackPassed
                    && BodyDefinition.BoxExtents.Equals(BodyExtents, 0.01)
                    && BodyDefinition.ConstraintSetup.LinearXLimitType == AnimPhysLinearConstraintType::Limited
                    && BodyDefinition.ConstraintSetup.LinearYLimitType == AnimPhysLinearConstraintType::Limited
                    && BodyDefinition.ConstraintSetup.LinearZLimitType == AnimPhysLinearConstraintType::Limited
                    && BodyDefinition.ConstraintSetup.bLinearFullyLocked
                    && BodyDefinition.ConstraintSetup.AngularConstraintType == AnimPhysAngularConstraintType::Cone
                    && FMath::IsNearlyEqual(BodyDefinition.ConstraintSetup.ConeAngle, AngularLimit, 0.001f);
            }
        }
    }
    else
    {
        bSettingsReadbackPassed = ReadbackDynamicsNode
            && ReadbackDynamicsNode->GetClass() == KawaiiNodeClass
            && VerifyKawaiiNodeReadback(
                ReadbackDynamicsNode,
                RootBone,
                Gravity,
                Alpha,
                KawaiiDamping,
                KawaiiStiffness,
                AngularLimit);
    }

    const bool bSemanticReadbackPassed = bCompileSucceeded
        && ReadbackGraph
        && ReadbackRefPoseNode
        && ReadbackLocalToComponentNode
        && ReadbackDynamicsNode
        && ReadbackComponentToLocalNode
        && ReadbackRootNode
        && bTopologyReadbackPassed
        && bSettingsReadbackPassed;

    auto PostSaveCurrentProof=[&]() -> bool
    {
    ReadbackGraph = FindBlueprintGraph(Blueprint, TargetGraphGuid.ToString(), FString(), FString());
    ReadbackRefPoseNode = Cast<UAnimGraphNode_LocalRefPose>(FindSecondaryMotionNodeByGuid(ReadbackGraph, RefPoseNodeGuid));
    ReadbackLocalToComponentNode = Cast<UAnimGraphNode_LocalToComponentSpace>(FindSecondaryMotionNodeByGuid(ReadbackGraph, LocalToComponentNodeGuid));
    ReadbackDynamicsNode = FindSecondaryMotionNodeByGuid(ReadbackGraph, DynamicsNodeGuid);
    ReadbackComponentToLocalNode = Cast<UAnimGraphNode_ComponentToLocalSpace>(FindSecondaryMotionNodeByGuid(ReadbackGraph, ComponentToLocalNodeGuid));
    ReadbackRootNode = Cast<UAnimGraphNode_Root>(FindSecondaryMotionNodeByGuid(ReadbackGraph, RootNodeGuid));

    UEdGraphPin* PostSave_ReadbackRefPoseOutput = FindFirstPosePin(ReadbackRefPoseNode, EGPD_Output);
    UEdGraphPin* PostSave_ReadbackLocalToComponentInput = FindFirstPosePin(ReadbackLocalToComponentNode, EGPD_Input, { TEXT("Pose") });
    UEdGraphPin* PostSave_ReadbackLocalToComponentOutput = FindFirstComponentPosePin(ReadbackLocalToComponentNode, EGPD_Output);
    UEdGraphPin* PostSave_ReadbackDynamicsInput = FindFirstComponentPosePin(ReadbackDynamicsNode, EGPD_Input, { TEXT("ComponentPose") });
    UEdGraphPin* PostSave_ReadbackDynamicsOutput = FindFirstComponentPosePin(ReadbackDynamicsNode, EGPD_Output);
    UEdGraphPin* PostSave_ReadbackComponentToLocalInput = FindFirstComponentPosePin(ReadbackComponentToLocalNode, EGPD_Input, { TEXT("ComponentPose") });
    UEdGraphPin* PostSave_ReadbackComponentToLocalOutput = FindFirstPosePin(ReadbackComponentToLocalNode, EGPD_Output);
    UEdGraphPin* PostSave_ReadbackRootInput = FindFirstPosePin(ReadbackRootNode, EGPD_Input, { TEXT("Result") });
    const bool PostSave_bTopologyReadbackPassed =
        AreGraphPinsLinked(PostSave_ReadbackRefPoseOutput, PostSave_ReadbackLocalToComponentInput)
        && AreGraphPinsLinked(PostSave_ReadbackLocalToComponentOutput, PostSave_ReadbackDynamicsInput)
        && AreGraphPinsLinked(PostSave_ReadbackDynamicsOutput, PostSave_ReadbackComponentToLocalInput)
        && AreGraphPinsLinked(PostSave_ReadbackComponentToLocalOutput, PostSave_ReadbackRootInput);

    bool PostSave_bSettingsReadbackPassed = false;
    if (bUseAnimDynamics)
    {
        auto* PostSave_ReadbackAnimDynamics = Cast<UAnimGraphNode_AnimDynamics>(ReadbackDynamicsNode);
        auto* PostSave_ReadbackRuntime=PostSave_ReadbackAnimDynamics?GetMutableDynamics(PostSave_ReadbackAnimDynamics):nullptr;
        PostSave_bSettingsReadbackPassed = PostSave_ReadbackRuntime
            && PostSave_ReadbackRuntime->BoundBone.BoneName == RootBone
            && PostSave_ReadbackRuntime->ChainEnd.BoneName == ChainEndBone
            && PostSave_ReadbackRuntime->bChain
            && PostSave_ReadbackRuntime->SimulationSpace == AnimPhysSimSpaceType::World
            && PostSave_ReadbackRuntime->bUseGravityOverride
            && PostSave_ReadbackRuntime->GravityOverride.Equals(Gravity, 0.01)
            && PostSave_ReadbackRuntime->bOverrideLinearDamping
            && FMath::IsNearlyEqual(PostSave_ReadbackRuntime->LinearDampingOverride, AnimLinearDamping, 0.001f)
            && PostSave_ReadbackRuntime->bOverrideAngularDamping
            && FMath::IsNearlyEqual(PostSave_ReadbackRuntime->AngularDampingOverride, AnimAngularDamping, 0.001f)
            && FMath::IsNearlyEqual(PostSave_ReadbackRuntime->Alpha, Alpha, 0.001f)
            && PostSave_ReadbackRuntime->PhysicsBodyDefinitions.Num() > 0;
        if (PostSave_bSettingsReadbackPassed)
        {
            for (const FAnimPhysBodyDefinition& BodyDefinition : PostSave_ReadbackRuntime->PhysicsBodyDefinitions)
            {
                PostSave_bSettingsReadbackPassed = PostSave_bSettingsReadbackPassed
                    && BodyDefinition.BoxExtents.Equals(BodyExtents, 0.01)
                    && BodyDefinition.ConstraintSetup.LinearXLimitType == AnimPhysLinearConstraintType::Limited
                    && BodyDefinition.ConstraintSetup.LinearYLimitType == AnimPhysLinearConstraintType::Limited
                    && BodyDefinition.ConstraintSetup.LinearZLimitType == AnimPhysLinearConstraintType::Limited
                    && BodyDefinition.ConstraintSetup.bLinearFullyLocked
                    && BodyDefinition.ConstraintSetup.AngularConstraintType == AnimPhysAngularConstraintType::Cone
                    && FMath::IsNearlyEqual(BodyDefinition.ConstraintSetup.ConeAngle, AngularLimit, 0.001f);
            }
        }
    }
    else
    {
        PostSave_bSettingsReadbackPassed = ReadbackDynamicsNode
            && ReadbackDynamicsNode->GetClass() == KawaiiNodeClass
            && VerifyKawaiiNodeReadback(
                ReadbackDynamicsNode,
                RootBone,
                Gravity,
                Alpha,
                KawaiiDamping,
                KawaiiStiffness,
                AngularLimit);
    }

    const bool PostSave_bSemanticReadbackPassed = bCompileSucceeded
        && ReadbackGraph
        && ReadbackRefPoseNode
        && ReadbackLocalToComponentNode
        && ReadbackDynamicsNode
        && ReadbackComponentToLocalNode
        && ReadbackRootNode
        && PostSave_bTopologyReadbackPassed
        && PostSave_bSettingsReadbackPassed;

    return PostSave_bSemanticReadbackPassed&&!Incomplete()&&Member(ReadbackRefPoseNode,ReadbackGraph)&&Member(ReadbackLocalToComponentNode,ReadbackGraph)&&Member(ReadbackDynamicsNode,ReadbackGraph)&&Member(ReadbackComponentToLocalNode,ReadbackGraph)&&Member(ReadbackRootNode,ReadbackGraph);
    };
    bool bSaved = false;
    if (bSaveRequested && bSemanticReadbackPassed && (!CurrentResult->bCompileAttempted || (CurrentResult->Compiled.IsSet() && CurrentResult->Compiled.GetValue())))
    {
        bSaved = SaveCurrent(Blueprint);
        const bool CurrentProof=PostSaveCurrentProof();CurrentResult->Data->SetBoolField(TEXT("post_save_current_semantics_verified"),CurrentProof);
        if(Incomplete()||!CurrentProof) { return CreatePartialMutationError(CurrentResult->Error.IsEmpty()?TEXT("Current topology/settings changed across forced save; actual save return retained"):CurrentResult->Error); }
    }
    const bool bSaveSucceeded = !bSaveRequested || bSaved;
    const bool bVerificationPassed = bSemanticReadbackPassed && bSaveSucceeded;
    const bool bSuccess = bVerificationPassed;
    UEdGraph* ReportingGraph = ReadbackGraph ? ReadbackGraph : TargetGraph;
    if(!Hold(ReportingGraph)||!Member(ReadbackRefPoseNode?ReadbackRefPoseNode:RefPoseNode,ReportingGraph)||!Member(ReadbackLocalToComponentNode?ReadbackLocalToComponentNode:LocalToComponentNode,ReportingGraph)||!Member(ReadbackDynamicsNode?ReadbackDynamicsNode:DynamicsNode,ReportingGraph)||!Member(ReadbackComponentToLocalNode?ReadbackComponentToLocalNode:ComponentToLocalNode,ReportingGraph)||!Member(ReadbackRootNode?ReadbackRootNode:RootNode,ReportingGraph)) { return CreatePartialMutationError(CurrentResult->Error); }

    TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();
    After->SetStringField(TEXT("blueprint"), AnimBlueprint->GetPathName());
    After->SetStringField(TEXT("graph"), ReportingGraph->GetName());
    After->SetStringField(TEXT("graph_id"), TargetGraphGuid.ToString());
    After->SetNumberField(TEXT("node_count"), ReportingGraph->Nodes.Num());
    After->SetStringField(TEXT("reference_pose_node_id"), RefPoseNodeGuid.ToString());
    After->SetStringField(TEXT("local_to_component_node_id"), LocalToComponentNodeGuid.ToString());
    After->SetStringField(TEXT("dynamics_node_id"), DynamicsNodeGuid.ToString());
    After->SetStringField(TEXT("component_to_local_node_id"), ComponentToLocalNodeGuid.ToString());
    After->SetStringField(TEXT("root_node_id"), RootNodeGuid.ToString());
    After->SetBoolField(TEXT("topology_readback_passed"), bTopologyReadbackPassed);
    After->SetBoolField(TEXT("settings_readback_passed"), bSettingsReadbackPassed);
    After->SetBoolField(TEXT("compile_succeeded"), bCompileSucceeded);
    After->SetBoolField(TEXT("save_succeeded"), bSaveSucceeded);
    After->SetBoolField(TEXT("package_dirty"), AnimBlueprint->GetOutermost() && AnimBlueprint->GetOutermost()->IsDirty());

    const int32 CreatedNodeCount = static_cast<int32>(bCreatedRefPose)
        + static_cast<int32>(bCreatedLocalToComponent)
        + static_cast<int32>(bCreatedDynamicsNode)
        + static_cast<int32>(bCreatedComponentToLocal);
    TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
    Delta->SetNumberField(TEXT("affected_count"), 5);
    Delta->SetNumberField(TEXT("created_count"), CreatedNodeCount);
    Delta->SetNumberField(TEXT("updated_count"), 5 - CreatedNodeCount);
    Delta->SetNumberField(TEXT("removed_count"), 0);
    Delta->SetNumberField(TEXT("skipped_count"), 0);
    Delta->SetNumberField(TEXT("failed_count"), bVerificationPassed ? 0 : 1);
    Delta->SetNumberField(TEXT("node_count_change"), ReportingGraph->Nodes.Num() - NodeCountBefore);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("command"), TEXT("ensure_anim_graph_secondary_motion_chain"));
    Result->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
    Result->SetStringField(TEXT("dynamics_type"), bUseAnimDynamics ? TEXT("anim_dynamics") : TEXT("kawaii_physics"));
    Result->SetStringField(TEXT("root_bone"), RootBone.ToString());
    Result->SetStringField(TEXT("chain_end_bone"), ChainEndBone.ToString());
    Result->SetStringField(TEXT("node_class"), (ReadbackDynamicsNode ? ReadbackDynamicsNode : DynamicsNode)->GetClass()->GetPathName());
    Result->SetBoolField(TEXT("plugin_available"), !bUseKawaii || KawaiiNodeClass != nullptr);
    Result->SetBoolField(TEXT("created_reference_pose_node"), bCreatedRefPose);
    Result->SetBoolField(TEXT("created_local_to_component_node"), bCreatedLocalToComponent);
    Result->SetBoolField(TEXT("created_dynamics_node"), bCreatedDynamicsNode);
    Result->SetBoolField(TEXT("created_component_to_local_node"), bCreatedComponentToLocal);
    Result->SetBoolField(TEXT("links_changed"), bLinksChanged);
    Result->SetNumberField(TEXT("alpha"), Alpha);
    Result->SetNumberField(TEXT("angular_limit_degrees"), AngularLimit);
    Result->SetStringField(TEXT("gravity"), Gravity.ToString());
    Result->SetBoolField(TEXT("compiled"), bCompiled);
    Result->SetBoolField(TEXT("compile_succeeded"), bCompileSucceeded);
    Result->SetStringField(TEXT("compile_status"), GetBlueprintNodeStatusName(Blueprint->Status));
    Result->SetBoolField(TEXT("save_requested"), bSaveRequested);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("verification_pass"), bVerificationPassed);
    Result->SetStringField(TEXT("verification_level"), TEXT("independent_readback"));
    Result->SetBoolField(TEXT("partial_target_modified"), !bSuccess);
    Result->SetBoolField(TEXT("do_not_retry"), !bSuccess);
    Result->SetBoolField(TEXT("serialized_unrealmcp_dependency"), false);
    Result->SetObjectField(TEXT("reference_pose_node"), NodeToJson(ReadbackRefPoseNode ? ReadbackRefPoseNode : RefPoseNode, true));
    Result->SetObjectField(TEXT("local_to_component_node"), NodeToJson(ReadbackLocalToComponentNode ? ReadbackLocalToComponentNode : LocalToComponentNode, true));
    Result->SetObjectField(TEXT("dynamics_node"), NodeToJson(ReadbackDynamicsNode ? ReadbackDynamicsNode : DynamicsNode, true));
    Result->SetObjectField(TEXT("component_to_local_node"), NodeToJson(ReadbackComponentToLocalNode ? ReadbackComponentToLocalNode : ComponentToLocalNode, true));
    Result->SetObjectField(TEXT("root_node"), NodeToJson(ReadbackRootNode ? ReadbackRootNode : RootNode, true));
    Result->SetObjectField(TEXT("settings"), AnimGraphNodeSettingsToJson(ReadbackDynamicsNode ? ReadbackDynamicsNode : DynamicsNode, 5));
    AddGraphField(Result, Blueprint, ReportingGraph);
    if (!bSuccess)
    {
        Result->SetStringField(
            TEXT("error"),
            !bCompileSucceeded
                ? TEXT("Animation Blueprint compile failed")
                : !bSemanticReadbackPassed
                    ? TEXT("Secondary-motion graph failed independent topology or settings readback")
                    : TEXT("Animation Blueprint failed to save after verified readback"));
    }
    AddMutationResultContract(
        Result,
        false,
        true,
        true,
        Before,
        After,
        Delta,
        bVerificationPassed,
        TEXT("fresh_anim_graph_guid_settings_link_readback_and_save_gate"),
        {
            TEXT("node_guids_resolve_after_compile"),
            TEXT("secondary_motion_pose_links_match_request"),
            TEXT("dynamics_settings_match_request"),
            TEXT("compile_and_save_requests_completed")
        });
    return Result;
}

void Init(FResult& R)
{
    R.Data->SetStringField(TEXT("operation"),TEXT("ensure_anim_graph_secondary_motion_chain"));R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("automatic_retry"),false);R.Data->SetBoolField(TEXT("runtime_secondary_motion_verified"),false);R.Data->SetBoolField(TEXT("total_SDK_load_cache_side_effects_unverified"),true);
    auto C=MakeShared<FJsonObject>();
    for(const TCHAR* K:{TEXT("add_ubergraph_page_calls"),TEXT("allocate_pins_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("break_all_pin_links_calls"),TEXT("can_create_connection_calls"),TEXT("create_node_guid_calls"),TEXT("dynamics_configuration_calls"),TEXT("event_graph_create_calls"),TEXT("event_graph_create_returns"),TEXT("explicit_compile_calls"),TEXT("explicit_compile_returns"),TEXT("explicit_save_calls"),TEXT("explicit_save_returns"),TEXT("get_all_graphs_calls"),TEXT("get_fnode_calls"),TEXT("get_fnode_property_calls"),TEXT("get_fnode_type_calls"),TEXT("graph_add_node_calls"),TEXT("graph_modify_calls"),TEXT("kawaii_configuration_calls"),TEXT("mark_structurally_modified_calls"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("node_modify_calls"),TEXT("node_position_write_calls"),TEXT("node_struct_property_search_calls"),TEXT("nodes_serialized"),TEXT("notify_graph_changed_calls"),TEXT("optional_class_load_calls"),TEXT("optional_module_load_calls"),TEXT("outer_transaction_calls"),TEXT("pin_default_changed_calls"),TEXT("pins_inventory_calls"),TEXT("pins_visited"),TEXT("post_placed_calls"),TEXT("property_export_calls"),TEXT("reconstruct_calls"),TEXT("reflected_property_reads"),TEXT("root_position_write_calls"),TEXT("skeleton_chain_preflight_calls"),TEXT("source_class_lookups"),TEXT("state_capture_calls"),TEXT("try_create_connection_calls"),TEXT("try_create_connection_returns"),TEXT("try_set_default_calls"),TEXT("update_chain_physics_body_calls")}) { C->SetNumberField(K,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),C);
}
FResult Finish(FResult& R)
{
    if(Serialize(R.Data).IsEmpty()) { R.Data->RemoveField(TEXT("selected_state_before_json"));R.Data->RemoveField(TEXT("selected_state_after_json"));R.Data->RemoveField(TEXT("legacy_result"));R.Data->SetBoolField(TEXT("oversized_observations_omitted"),true);R.bSuccess=false;R.bPostconditionPassed=false;R.bChangeStateKnown=false;if(R.ErrorCode.IsEmpty()) { R.ErrorCode=TEXT("secondary_motion_output_incomplete"); }if(R.Error.IsEmpty()) { R.Error=TEXT("Observation bound exceeded; actual compiler/save/writer prefix retained"); } }return R;
}
FResult Execute(const TSharedRef<FJsonObject>& P)
{
    FResult R;Init(R);R.bCompileRequested=GetBoolParam(P,TEXT("compile"),true);R.bSaveRequested=GetBoolParam(P,TEXT("save"),true);
    if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { R.ErrorCode=TEXT("secondary_motion_editor_invalid");R.Error=TEXT("Idle nonreentrant Editor game thread required");return Finish(R); }
    struct FScope { FScope(FResult& V) { CurrentResult=&V;Held.Reset();HeldObjects.Reset();KnownPins.Reset();PropertyValues=0;SelectedGraph=nullptr;SelectedGraphGuid.Invalidate();BeforeSource.Reset();BeforeSourceText.Reset();AllowedNodes.Reset(); }~FScope() { CurrentResult=nullptr;CurrentBlueprint=nullptr;SelectedGraph=nullptr;Held.Reset();HeldObjects.Reset();KnownPins.Reset();BeforeSource.Reset();AnimBase=nullptr;AnimStruct=PoseStruct=ComponentPoseStruct=nullptr; } } Scope(R);
    for(const TCHAR* K:{TEXT("blueprint_name"),TEXT("dynamics_type"),TEXT("root_bone"),TEXT("chain_end_bone"),TEXT("graph_name"),TEXT("graph_id"),TEXT("graph_type")}) { if(!SafeText(GetStringParam(P,K),NAME_SIZE-1)) { R.ErrorCode=TEXT("secondary_motion_input_invalid");R.Error=TEXT("Bounded original strings without interior NUL required");return Finish(R); } }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(FText::FromString(TEXT("MCP ensure_anim_graph_secondary_motion_chain")));
    const auto Legacy=RunDemo(P);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("legacy_result_returned"),true);
    if(CurrentBlueprint&&Hold(CurrentBlueprint)&&Hold(CurrentBlueprint->GetOutermost()))
    {
        const auto After=Capture(CurrentBlueprint);const auto Text=Serialize(After);R.Data->SetStringField(TEXT("selected_state_after_json"),Text);
        const bool Changed=!BeforeSourceText.IsEmpty()&&!Text.IsEmpty()&&!BeforeSourceText.Equals(Text,ESearchCase::CaseSensitive);R.bChanged|=Changed;R.Data->SetBoolField(TEXT("observed_selected_state_changed"),Changed);
        if(!BeforeSourceText.IsEmpty()&&!Text.IsEmpty()&&!Incomplete())
        { if(auto* G=Resolve(CurrentBlueprint,FString(),SelectedGraphGuid.ToString(),FString(),R)) { SelectedGraph=G;AllowChain(G);R.Data->SetBoolField(TEXT("unaffected_selected_source_preserved"),Preserve(BeforeSource,After,G,AllowedNodes)); }else { NoteUnsafe(TEXT("Selected graph GUID unavailable after composite")); } }
        if(!Incomplete()&&!Text.IsEmpty()) { R.bChangeStateKnown=true; }
    }
    if(Incomplete()) { R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;return Finish(R); }
    bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);bool Verified=false;Legacy->TryGetBoolField(TEXT("verification_pass"),Verified);bool Preserved=false;R.Data->TryGetBoolField(TEXT("unaffected_selected_source_preserved"),Preserved);
    if(!Success) { R.ErrorCode=TEXT("secondary_motion_original_failure");Legacy->TryGetStringField(TEXT("error"),R.Error);R.Data->SetBoolField(TEXT("late_failure_no_automatic_cleanup"),R.bMutationAttempted);return Finish(R); }
    R.bPostconditionPassed=Verified&&Preserved&&(!R.bCompileAttempted||(R.Compiled.IsSet()&&R.Compiled.GetValue()))&&(!R.bSaveRequested||R.bSaved);
    if(!R.bPostconditionPassed) { R.ErrorCode=TEXT("secondary_motion_postcondition_failed");R.Error=TEXT("Current topology/settings/compiler/save or unrelated source proof failed; actual prefix retained");return Finish(R); }
    R.bSuccess=true;R.Stage=TEXT("complete");return Finish(R);
}
}
Variable::FResult EnsureSecondaryMotionChain(const FString& BlueprintName,const FString& DynamicsType,const FString& RootBone,const FString& ChainEndBone,double Alpha,const FString& GravityJson,double AngularLimitDegrees,const FString& BodyExtentsJson,double LinearDamping,double AngularDamping,double KawaiiDamping,double KawaiiStiffness,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample,bool bCompile,bool bSave)
{
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("blueprint_name"),BlueprintName);P->SetStringField(TEXT("dynamics_type"),DynamicsType);P->SetStringField(TEXT("root_bone"),RootBone);P->SetStringField(TEXT("chain_end_bone"),ChainEndBone);P->SetNumberField(TEXT("alpha"),Alpha);P->SetNumberField(TEXT("angular_limit_degrees"),AngularLimitDegrees);P->SetNumberField(TEXT("linear_damping"),LinearDamping);P->SetNumberField(TEXT("angular_damping"),AngularDamping);P->SetNumberField(TEXT("kawaii_damping"),KawaiiDamping);P->SetNumberField(TEXT("kawaii_stiffness"),KawaiiStiffness);P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_id"),GraphId);P->SetStringField(TEXT("graph_type"),GraphType);P->SetBoolField(TEXT("replace_existing"),bReplaceExisting);P->SetBoolField(TEXT("allow_non_sample"),bAllowNonSample);P->SetBoolField(TEXT("compile"),bCompile);P->SetBoolField(TEXT("save"),bSave);
    FResult Invalid;Init(Invalid);Invalid.bCompileRequested=bCompile;Invalid.bSaveRequested=bSave;
    if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { Invalid.ErrorCode=TEXT("secondary_motion_editor_invalid");Invalid.Error=TEXT("Idle nonreentrant Editor game thread required before private parse");return Invalid; }
    CurrentResult=&Invalid;const bool Packed=Pack(GravityJson,TEXT("gravity"),P,true)&&Pack(BodyExtentsJson,TEXT("body_extents"),P,true);CurrentResult=nullptr;if(!Packed) { Invalid.ErrorCode=TEXT("secondary_motion_json_invalid");return Invalid; }return Execute(P);
}
}
