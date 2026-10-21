#include "Compatibility/UE58/CubelessAnimGraphBoneDemoApi.h"
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
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_Root.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
namespace Cubeless::UE58::AnimGraphBoneDemo
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
void NoteUnsafe(const TCHAR* Error) { if(CurrentResult) { CurrentResult->Data->SetBoolField(TEXT("observation_incomplete"),true);CurrentResult->ErrorCode=TEXT("anim_graph_bone_demo_observation_incomplete");CurrentResult->Error=Error; } }
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
bool NewPoseOwnerSafe(UEdGraph* G)
{
    auto* BP=Cast<UAnimBlueprint>(G->GetOuter());if(BP!=CurrentBlueprint||!Loaded(BP)) { CurrentResult->Error=TEXT("LinkedInputPose PostPlaced requires the selected graph's direct AnimBlueprint outer");return false; }
    for(UEdGraph* Graph:BP->FunctionGraphs) { if(!Loaded(Graph)||!Loaded(Graph->Schema.Get())) { CurrentResult->Error=TEXT("LinkedInputPose SDK naming scan requires loaded function graphs/schema classes");return false; } }
    Count(*CurrentResult,TEXT("asset_editor_readonly_queries"));auto* Subsystem=GEditor?GEditor->GetEditorSubsystem<UAssetEditorSubsystem>():nullptr;
    if(!Subsystem) { CurrentResult->Error=TEXT("Editor AssetEditorSubsystem unavailable for deferred SDK inspector safety");return false; }
    if(IAssetEditorInstance* Editor=Subsystem->FindEditorForAsset(BP,false);Editor&&Editor->GetEditorName()!=FName(TEXT("AnimationBlueprintEditor"))) { CurrentResult->Error=TEXT("Existing asset editor is not AnimationBlueprintEditor; SDK deferred checked cast is unsafe");return false; }
    CurrentResult->Data->SetBoolField(TEXT("SDK_PostPlaced_enqueues_weak_editor_inspector_ticker"),true);return true;
}

bool IsComponentPoseLinkPin(const UEdGraphPin* P) { return PinKnown(P)&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&P->PinType.PinSubCategoryObject.Get()==ComponentPoseStruct; }
UEdGraphPin* FindFirstComponentPosePin(UEdGraphNode* Node,EEdGraphPinDirection Direction,const TArray<FString>& Preferred=TArray<FString>())
{
    if(!Loaded(Node)) { return nullptr; }
    for(const auto& Name:Preferred) { for(UEdGraphPin* P:Node->Pins) { if(PinKnown(P)&&P->Direction==Direction&&!P->ParentPin&&IsComponentPoseLinkPin(P)&&P->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)) { return P; } } }
    for(UEdGraphPin* P:Node->Pins) { if(PinKnown(P)&&P->Direction==Direction&&!P->ParentPin&&IsComponentPoseLinkPin(P)) { return P; } }return nullptr;
}
bool RotationSafe(const TSharedPtr<FJsonObject>& Params)
{
    if(const TArray<TSharedPtr<FJsonValue>>* A=nullptr;Params->TryGetArrayField(TEXT("rotation"),A)&&A->Num()>=3)
    { for(int32 I=0;I<3;++I) { if(!(*A)[I].IsValid()||!FloatProjectionSafe((*A)[I]->AsNumber())) { return false; } } }return true;
}

bool Member(UEdGraphNode* N,UEdGraph* G) { if(!N||!G||!Hold(N)||!Hold(G)||N->GetGraph()!=G||!G->Nodes.Contains(N)) { NoteUnsafe(TEXT("Selected typed node no longer belongs to current graph"));return false; }return true; }
bool SkeletalDefaultsSafe(UAnimGraphNode_ModifyBone* N)
{
    if(!N||!Member(N,N->GetGraph())) { return false; }
    for(const TCHAR* K:{TEXT("Translation"),TEXT("Rotation"),TEXT("Scale")})
    {
        if(auto* P=FindPin(N,K,EGPD_Input))
        {
            if(P->ParentPin||P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Struct||!Loaded(P->PinType.PinSubCategoryObject.Get())) { NoteUnsafe(TEXT("Selected skeletal vector default has unsupported split/type layout before callback"));return false; }
            TArray<FString> Parts;P->DefaultValue.ParseIntoArray(Parts,TEXT(","));
            if(Parts.Num()!=3) { NoteUnsafe(TEXT("Selected skeletal vector default must have three comma fields before SDK checked parser"));return false; }
            for(const FString& V:Parts) { if(!FMath::IsFinite(FCString::Atod(*V))) { NoteUnsafe(TEXT("Selected skeletal default outside finite callback scope"));return false; } }
        }
    }return !Incomplete();
}

TSharedPtr<FJsonObject> Failure(const FString& Error) { return CreateErrorResponse(Error); }
void WriteIntent(const TCHAR* Counter) { auto& R=*CurrentResult;R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,Counter); }
void FieldWritten(const TCHAR* Field)
{
    auto& R=*CurrentResult;R.bChanged=true;TArray<TSharedPtr<FJsonValue>> Rows;
    if(const TArray<TSharedPtr<FJsonValue>>* Existing=nullptr;R.Data->TryGetArrayField(TEXT("native_changed_fields_prefix"),Existing)) { Rows=*Existing; }
    Rows.Add(MakeShared<FJsonValueString>(Field));R.Data->SetArrayField(TEXT("native_changed_fields_prefix"),Rows);
}
template<class T> T* AddAnimGraphNodeToGraph(UEdGraph* Graph,const FVector2D& Position)
{
    if(T::StaticClass()==UAnimGraphNode_LinkedInputPose::StaticClass()&&!NewPoseOwnerSafe(Graph)) { return nullptr; }
    auto& R=*CurrentResult;WriteIntent(TEXT("node_allocation_calls"));T* Node=NewObject<T>(Graph);Count(R,TEXT("node_allocation_returns"));
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
TSharedPtr<FJsonObject> RunDemo(const TSharedPtr<FJsonObject>& Params,UBlueprint* Blueprint,UEdGraph* TargetGraph)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return Failure(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString BoneName;
    if (!Params->TryGetStringField(TEXT("bone_name"), BoneName))
    {
        BoneName = TEXT("head");
    }
    BoneName.TrimStartAndEndInline();
    if (BoneName.IsEmpty())
    {
        return Failure(TEXT("'bone_name' cannot be empty"));
    }
    const FName TargetBoneName(*BoneName);

    FVector RotationVector(0.0f, 0.0f, 6.0f);
    if (Params->HasField(TEXT("rotation")))
    {
        RotationVector = GetVectorFromJson(Params, TEXT("rotation"));
    }
    const FRotator AdditiveRotation(RotationVector.X, RotationVector.Y, RotationVector.Z);

    bool bReplaceExisting = false;
    if (Params->HasField(TEXT("replace_existing")))
    {
        bReplaceExisting = Params->GetBoolField(TEXT("replace_existing"));
    }

    UAnimGraphNode_Root* RootNode = FindFirstNodeOfType<UAnimGraphNode_Root>(TargetGraph);
    if (!RootNode)
    {
        return Failure(FString::Printf(TEXT("AnimGraph root node not found in graph: %s"), *TargetGraph->GetName()));
    }

    bool bCreatedInputNode = false;
    UAnimGraphNode_LinkedInputPose* InputPoseNode = FindFirstNodeOfType<UAnimGraphNode_LinkedInputPose>(TargetGraph);
    if (!InputPoseNode)
    {
        InputPoseNode = AddAnimGraphNodeToGraph<UAnimGraphNode_LinkedInputPose>(TargetGraph, FVector2D(-720.0f, 0.0f));
        if (!InputPoseNode)
        {
            return Failure(TEXT("Failed to create LinkedInputPose node"));
        }
        bCreatedInputNode = true;
    }

    bool bCreatedLocalToComponentNode = false;
    UAnimGraphNode_LocalToComponentSpace* LocalToComponentNode = FindFirstNodeOfType<UAnimGraphNode_LocalToComponentSpace>(TargetGraph);
    if (!LocalToComponentNode)
    {
        LocalToComponentNode = AddAnimGraphNodeToGraph<UAnimGraphNode_LocalToComponentSpace>(TargetGraph, FVector2D(-480.0f, 0.0f));
        if (!LocalToComponentNode)
        {
            return Failure(TEXT("Failed to create LocalToComponentSpace node"));
        }
        bCreatedLocalToComponentNode = true;
    }

    bool bCreatedModifyBoneNode = false;
    UAnimGraphNode_ModifyBone* ModifyBoneNode = nullptr;
    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if(!Node) { continue; }if(!Hold(Node)) { return Failure(CurrentResult->Error); }
        UAnimGraphNode_ModifyBone* Candidate = Cast<UAnimGraphNode_ModifyBone>(Node);
        if (Candidate && Candidate->Node.BoneToModify.BoneName == TargetBoneName)
        {
            ModifyBoneNode = Candidate;
            break;
        }
    }
    if (!ModifyBoneNode)
    {
        ModifyBoneNode = FindFirstNodeOfType<UAnimGraphNode_ModifyBone>(TargetGraph);
    }
    if (!ModifyBoneNode)
    {
        ModifyBoneNode = AddAnimGraphNodeToGraph<UAnimGraphNode_ModifyBone>(TargetGraph, FVector2D(-240.0f, 0.0f));
        if (!ModifyBoneNode)
        {
            return Failure(TEXT("Failed to create ModifyBone node"));
        }
        bCreatedModifyBoneNode = true;
    }

    bool bCreatedComponentToLocalNode = false;
    UAnimGraphNode_ComponentToLocalSpace* ComponentToLocalNode = FindFirstNodeOfType<UAnimGraphNode_ComponentToLocalSpace>(TargetGraph);
    if (!ComponentToLocalNode)
    {
        ComponentToLocalNode = AddAnimGraphNodeToGraph<UAnimGraphNode_ComponentToLocalSpace>(TargetGraph, FVector2D(0.0f, 0.0f));
        if (!ComponentToLocalNode)
        {
            return Failure(TEXT("Failed to create ComponentToLocalSpace node"));
        }
        bCreatedComponentToLocalNode = true;
    }
    if(!RefreshInventory()||!Member(RootNode,TargetGraph)||!Member(InputPoseNode,TargetGraph)||!Member(LocalToComponentNode,TargetGraph)||!Member(ModifyBoneNode,TargetGraph)||!Member(ComponentToLocalNode,TargetGraph)) { return Failure(CurrentResult->Error); }
    CurrentResult->Data->SetStringField(TEXT("selected_root_guid"),RootNode->NodeGuid.ToString());
    const bool RootMoved=RootNode->NodePosX!=240||RootNode->NodePosY!=0;WriteIntent(TEXT("root_position_write_calls"));Count(*CurrentResult,TEXT("root_position_write_calls"));
    RootNode->NodePosX = 240;RootNode->NodePosY = 0;CurrentResult->bChanged|=RootMoved;CurrentResult->Data->SetBoolField(TEXT("root_position_observed_changed"),RootMoved);

    bool bSettingsChanged = false;
    if (ModifyBoneNode->Node.BoneToModify.BoneName != TargetBoneName)
    {
        WriteIntent(TEXT("bone_field_write_calls"));ModifyBoneNode->Node.BoneToModify.BoneName = TargetBoneName;FieldWritten(TEXT("bone_name"));
        bSettingsChanged = true;
    }
    if (!ModifyBoneNode->Node.Rotation.Equals(AdditiveRotation, KINDA_SMALL_NUMBER))
    {
        WriteIntent(TEXT("bone_field_write_calls"));ModifyBoneNode->Node.Rotation = AdditiveRotation;FieldWritten(TEXT("rotation"));
        bSettingsChanged = true;
    }
    if(!SkeletalDefaultsSafe(ModifyBoneNode)) { return Failure(CurrentResult->Error); }
    if (UEdGraphPin* RotationPin = FindPin(ModifyBoneNode, TEXT("Rotation"), EGPD_Input))
    {
        const FEdGraphPinReference RotationReference(RotationPin);
        const FString RotationDefault = FString::Printf(
            TEXT("%f,%f,%f"),
            AdditiveRotation.Pitch,
            AdditiveRotation.Yaw,
            AdditiveRotation.Roll);
        if (!RotationPin->DefaultValue.Equals(RotationDefault, ESearchCase::CaseSensitive))
        {
            if (TargetGraph->GetSchema())
            {
                WriteIntent(TEXT("try_set_default_calls"));TargetGraph->GetSchema()->TrySetDefaultValue(*RotationPin, RotationDefault);
                if(!AfterDefaultSetter(RotationPin,RotationReference)||!Member(ModifyBoneNode,TargetGraph)||!SkeletalDefaultsSafe(ModifyBoneNode)) { return Failure(CurrentResult->Error); }
            }
            WriteIntent(TEXT("pin_default_changed_calls"));ModifyBoneNode->PinDefaultValueChanged(RotationPin);
            if(!AfterDefaultSetter(RotationPin,RotationReference)||!Member(ModifyBoneNode,TargetGraph)||!SkeletalDefaultsSafe(ModifyBoneNode)) { return Failure(CurrentResult->Error); }FieldWritten(TEXT("rotation_default"));
            bSettingsChanged = true;
        }
    }
    if (ModifyBoneNode->Node.TranslationMode != BMM_Ignore)
    {
        WriteIntent(TEXT("bone_field_write_calls"));ModifyBoneNode->Node.TranslationMode = BMM_Ignore;FieldWritten(TEXT("translation_mode"));
        bSettingsChanged = true;
    }
    if (ModifyBoneNode->Node.RotationMode != BMM_Additive)
    {
        WriteIntent(TEXT("bone_field_write_calls"));ModifyBoneNode->Node.RotationMode = BMM_Additive;FieldWritten(TEXT("rotation_mode"));
        bSettingsChanged = true;
    }
    if (ModifyBoneNode->Node.ScaleMode != BMM_Ignore)
    {
        WriteIntent(TEXT("bone_field_write_calls"));ModifyBoneNode->Node.ScaleMode = BMM_Ignore;FieldWritten(TEXT("scale_mode"));
        bSettingsChanged = true;
    }
    if (ModifyBoneNode->Node.TranslationSpace != BCS_BoneSpace)
    {
        WriteIntent(TEXT("bone_field_write_calls"));ModifyBoneNode->Node.TranslationSpace = BCS_BoneSpace;FieldWritten(TEXT("translation_space"));
        bSettingsChanged = true;
    }
    if (ModifyBoneNode->Node.RotationSpace != BCS_BoneSpace)
    {
        WriteIntent(TEXT("bone_field_write_calls"));ModifyBoneNode->Node.RotationSpace = BCS_BoneSpace;FieldWritten(TEXT("rotation_space"));
        bSettingsChanged = true;
    }
    if (ModifyBoneNode->Node.ScaleSpace != BCS_BoneSpace)
    {
        WriteIntent(TEXT("bone_field_write_calls"));ModifyBoneNode->Node.ScaleSpace = BCS_BoneSpace;FieldWritten(TEXT("scale_space"));
        bSettingsChanged = true;
    }

    UEdGraphPin* InputPoseOutput = FindFirstPosePin(InputPoseNode, EGPD_Output);
    UEdGraphPin* LocalToComponentInput = FindFirstPosePin(LocalToComponentNode, EGPD_Input, { TEXT("Pose") });
    UEdGraphPin* LocalToComponentOutput = FindFirstComponentPosePin(LocalToComponentNode, EGPD_Output);
    UEdGraphPin* ModifyBoneInput = FindFirstComponentPosePin(ModifyBoneNode, EGPD_Input, { TEXT("ComponentPose") });
    UEdGraphPin* ModifyBoneOutput = FindFirstComponentPosePin(ModifyBoneNode, EGPD_Output);
    UEdGraphPin* ComponentToLocalInput = FindFirstComponentPosePin(ComponentToLocalNode, EGPD_Input, { TEXT("ComponentPose") });
    UEdGraphPin* ComponentToLocalOutput = FindFirstPosePin(ComponentToLocalNode, EGPD_Output);
    UEdGraphPin* RootInput = FindFirstPosePin(RootNode, EGPD_Input, { TEXT("Result") });

    if (!InputPoseOutput || !LocalToComponentInput || !LocalToComponentOutput || !ModifyBoneInput || !ModifyBoneOutput || !ComponentToLocalInput || !ComponentToLocalOutput || !RootInput)
    {
        return Failure(TEXT("Failed to resolve one or more required pose pins for ModifyBone demo chain"));
    }

    UEdGraphPin* OriginalPins[8]={InputPoseOutput,LocalToComponentInput,LocalToComponentOutput,ModifyBoneInput,ModifyBoneOutput,ComponentToLocalInput,ComponentToLocalOutput,RootInput};
    const FEdGraphPinReference OriginalPinReferences[8]={FEdGraphPinReference(InputPoseOutput),FEdGraphPinReference(LocalToComponentInput),FEdGraphPinReference(LocalToComponentOutput),FEdGraphPinReference(ModifyBoneInput),FEdGraphPinReference(ModifyBoneOutput),FEdGraphPinReference(ComponentToLocalInput),FEdGraphPinReference(ComponentToLocalOutput),FEdGraphPinReference(RootInput)};
    auto OriginalPinsCurrent=[&]() { for(int32 I=0;I<8;++I) { if(OriginalPinReferences[I].Get()!=OriginalPins[I]||!PinKnown(OriginalPins[I])) { NoteUnsafe(TEXT("Required original pose pin identity retired across callback; no unsafe continuation"));return false; } }return true; };
    bool bLinkChanged = false;
    bool bChangedThisLink = false;
    FString LinkError;
    TSharedPtr<FJsonObject> LinkErrorObj;
    if(!SkeletalDefaultsSafe(ModifyBoneNode)||!OriginalPinsCurrent()) { return Failure(CurrentResult->Error); }
    if (!EnsureAnimGraphConnection(TargetGraph, InputPoseOutput, LocalToComponentInput, bReplaceExisting, bChangedThisLink, LinkError, &LinkErrorObj))
    {
        if (LinkErrorObj.IsValid())
        {
            AddGraphField(LinkErrorObj, Blueprint, TargetGraph);
            return LinkErrorObj;
        }
        return Failure(LinkError);
    }
    bLinkChanged |= bChangedThisLink;

    if(!SkeletalDefaultsSafe(ModifyBoneNode)||!OriginalPinsCurrent()) { return Failure(CurrentResult->Error); }
    if (!EnsureAnimGraphConnection(TargetGraph, LocalToComponentOutput, ModifyBoneInput, bReplaceExisting, bChangedThisLink, LinkError, &LinkErrorObj))
    {
        if (LinkErrorObj.IsValid())
        {
            AddGraphField(LinkErrorObj, Blueprint, TargetGraph);
            return LinkErrorObj;
        }
        return Failure(LinkError);
    }
    bLinkChanged |= bChangedThisLink;

    if(!SkeletalDefaultsSafe(ModifyBoneNode)||!OriginalPinsCurrent()) { return Failure(CurrentResult->Error); }
    if (!EnsureAnimGraphConnection(TargetGraph, ModifyBoneOutput, ComponentToLocalInput, bReplaceExisting, bChangedThisLink, LinkError, &LinkErrorObj))
    {
        if (LinkErrorObj.IsValid())
        {
            AddGraphField(LinkErrorObj, Blueprint, TargetGraph);
            return LinkErrorObj;
        }
        return Failure(LinkError);
    }
    bLinkChanged |= bChangedThisLink;

    if(!SkeletalDefaultsSafe(ModifyBoneNode)||!OriginalPinsCurrent()) { return Failure(CurrentResult->Error); }
    if (!EnsureAnimGraphConnection(TargetGraph, ComponentToLocalOutput, RootInput, bReplaceExisting, bChangedThisLink, LinkError, &LinkErrorObj))
    {
        if (LinkErrorObj.IsValid())
        {
            AddGraphField(LinkErrorObj, Blueprint, TargetGraph);
            return LinkErrorObj;
        }
        return Failure(LinkError);
    }
    bLinkChanged |= bChangedThisLink;

    const bool bGraphChanged = bCreatedInputNode || bCreatedLocalToComponentNode || bCreatedModifyBoneNode || bCreatedComponentToLocalNode || bSettingsChanged || bLinkChanged;
    if (bGraphChanged)
    {
        WriteIntent(TEXT("mark_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        if(!RefreshInventory()||!Hold(TargetGraph)||!Member(RootNode,TargetGraph)||!Member(ModifyBoneNode,TargetGraph)) { return Failure(CurrentResult->Error); }
        WriteIntent(TEXT("notify_graph_changed_calls"));TargetGraph->NotifyGraphChanged();
        if(!RefreshInventory()) { return Failure(CurrentResult->Error); }
    }

    if(!RefreshInventory()||!Member(RootNode,TargetGraph)||!Member(InputPoseNode,TargetGraph)||!Member(LocalToComponentNode,TargetGraph)||!Member(ModifyBoneNode,TargetGraph)||!Member(ComponentToLocalNode,TargetGraph)) { return Failure(CurrentResult->Error); }
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("connected"), true);
    ResultObj->SetBoolField(TEXT("graph_changed"), bGraphChanged);
    ResultObj->SetBoolField(TEXT("created_input_node"), bCreatedInputNode);
    ResultObj->SetBoolField(TEXT("created_local_to_component_node"), bCreatedLocalToComponentNode);
    ResultObj->SetBoolField(TEXT("created_modify_bone_node"), bCreatedModifyBoneNode);
    ResultObj->SetBoolField(TEXT("created_component_to_local_node"), bCreatedComponentToLocalNode);
    ResultObj->SetBoolField(TEXT("settings_changed"), bSettingsChanged);
    ResultObj->SetBoolField(TEXT("links_changed"), bLinkChanged);
    ResultObj->SetStringField(TEXT("bone_name"), BoneName);
    ResultObj->SetStringField(TEXT("rotation_mode"), TEXT("BMM_Additive"));
    ResultObj->SetStringField(TEXT("rotation_space"), TEXT("BCS_BoneSpace"));
    ResultObj->SetStringField(TEXT("rotation"), AdditiveRotation.ToString());
    ResultObj->SetObjectField(TEXT("input_pose_node"), NodeToJson(InputPoseNode, true));
    ResultObj->SetObjectField(TEXT("local_to_component_node"), NodeToJson(LocalToComponentNode, true));
    ResultObj->SetObjectField(TEXT("modify_bone_node"), NodeToJson(ModifyBoneNode, true));
    ResultObj->SetObjectField(TEXT("component_to_local_node"), NodeToJson(ComponentToLocalNode, true));
    ResultObj->SetObjectField(TEXT("root_node"), NodeToJson(RootNode, true));
    AddGraphField(ResultObj, Blueprint, TargetGraph);
    return ResultObj;
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
void ChainAllowed(UEdGraph* Graph,const FName& Bone,TSet<FString>& Allowed)
{
    AddAllowed(FindFirstNodeOfType<UAnimGraphNode_Root>(Graph),Allowed);AddAllowed(FindFirstNodeOfType<UAnimGraphNode_LinkedInputPose>(Graph),Allowed);
    AddAllowed(FindFirstNodeOfType<UAnimGraphNode_LocalToComponentSpace>(Graph),Allowed);AddAllowed(FindFirstNodeOfType<UAnimGraphNode_ComponentToLocalSpace>(Graph),Allowed);
    UAnimGraphNode_ModifyBone* Selected=nullptr;for(UEdGraphNode* N:Graph->Nodes) { if(!N) { continue; }if(!Hold(N)) { return; }if(auto* M=Cast<UAnimGraphNode_ModifyBone>(N);M&&M->Node.BoneToModify.BoneName==Bone) { Selected=M;break; } }
    if(!Selected) { Selected=FindFirstNodeOfType<UAnimGraphNode_ModifyBone>(Graph); }AddAllowed(Selected,Allowed);
}
bool Preserve(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,UEdGraph* Graph,const TSet<FString>& Allowed)
{
    if(!EqualJson(Before->TryGetField(TEXT("member_variables")),After->TryGetField(TEXT("member_variables")))) { return false; }
    for(const TCHAR* K:{TEXT("blueprint"),TEXT("parent_class"),TEXT("generated_class"),TEXT("skeleton_class")}) { if(!EqualJson(Before->TryGetField(K),After->TryGetField(K))) { return false; } }
    const auto& A=Before->GetArrayField(TEXT("graphs"));const auto& B=After->GetArrayField(TEXT("graphs"));if(A.Num()!=B.Num()) { return false; }
    FString RootGuid;CurrentResult->Data->TryGetStringField(TEXT("selected_root_guid"),RootGuid);TSet<FString> Added;
    if(const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;CurrentResult->Data->TryGetArrayField(TEXT("new_node_receipts"),Rows)) { for(const auto& V:*Rows) { FString Id;if(V->AsObject()->TryGetStringField(TEXT("node_id"),Id)) { Added.Add(Id); } } }
    for(int32 I=0;I<A.Num();++I)
    {
        const auto Old=A[I]->AsObject();const auto Now=B[I]->AsObject();if(Old->GetStringField(TEXT("path"))!=Graph->GetPathName()) { if(!EqualJson(A[I],B[I])) { return false; }continue; }
        for(const TCHAR* K:{TEXT("path"),TEXT("graph_id"),TEXT("graph_name"),TEXT("graph_type"),TEXT("schema"),TEXT("editable")}) { if(!EqualJson(Old->TryGetField(K),Now->TryGetField(K))) { return false; } }
        const auto& OldNodes=Old->GetArrayField(TEXT("nodes"));const auto& NewNodes=Now->GetArrayField(TEXT("nodes"));
        for(const auto& V:NewNodes) { const auto Id=V->AsObject()->GetStringField(TEXT("node_id"));if(!OldNodes.ContainsByPredicate([&](const TSharedPtr<FJsonValue>& X){return X->AsObject()->GetStringField(TEXT("node_id"))==Id;})&&!Added.Contains(Id)) { return false; } }
        for(const auto& V:OldNodes)
        {
            const auto Row=V->AsObject();const auto Id=Row->GetStringField(TEXT("node_id"));const auto* Found=NewNodes.FindByPredicate([&](const TSharedPtr<FJsonValue>& X){return X->AsObject()->GetStringField(TEXT("node_id"))==Id;});if(!Found) { return false; }const auto Current=(*Found)->AsObject();
            for(const TCHAR* K:{TEXT("node_id"),TEXT("name"),TEXT("class"),TEXT("path"),TEXT("node_comment"),TEXT("comment_bubble_visible"),TEXT("comment_bubble_pinned")}) { if(!EqualJson(Row->TryGetField(K),Current->TryGetField(K))) { return false; } }
            if(Id==RootGuid) { if(Current->GetNumberField(TEXT("x"))!=240||Current->GetNumberField(TEXT("y"))!=0) { return false; } }
            else if(!EqualJson(Row->TryGetField(TEXT("x")),Current->TryGetField(TEXT("x")))||!EqualJson(Row->TryGetField(TEXT("y")),Current->TryGetField(TEXT("y")))) { return false; }
            if(!Allowed.Contains(Id)&&!EqualJson(V,*Found)) { return false; }
        }
    }return !Incomplete();
}
bool VerifyChain(const TSharedPtr<FJsonObject>& Params,UEdGraph* Graph,const TSharedPtr<FJsonObject>& Legacy)
{
    UEdGraphNode* Nodes[5]={};const TCHAR* Keys[5]={TEXT("input_pose_node"),TEXT("local_to_component_node"),TEXT("modify_bone_node"),TEXT("component_to_local_node"),TEXT("root_node")};
    for(int32 I=0;I<5;++I)
    { const auto Row=Legacy->GetObjectField(Keys[I]);const FString Id=Row->GetStringField(TEXT("node_id"));for(UEdGraphNode* N:Graph->Nodes) { if(N&&Hold(N)&&N->NodeGuid.ToString()==Id) { Nodes[I]=N;break; } }if(!Nodes[I]) { return false; } }
    auto* Input=Cast<UAnimGraphNode_LinkedInputPose>(Nodes[0]);auto* L=Cast<UAnimGraphNode_LocalToComponentSpace>(Nodes[1]);auto* M=Cast<UAnimGraphNode_ModifyBone>(Nodes[2]);auto* C=Cast<UAnimGraphNode_ComponentToLocalSpace>(Nodes[3]);auto* Root=Cast<UAnimGraphNode_Root>(Nodes[4]);if(!Input||!L||!M||!C||!Root) { return false; }
    FVector V(0,0,6);if(Params->HasField(TEXT("rotation"))) { V=GetVectorFromJson(Params,TEXT("rotation")); }const FRotator Rotation(V.X,V.Y,V.Z);const FName Bone(*GetStringParam(Params,TEXT("bone_name"),TEXT("head")).TrimStartAndEnd());
    const bool Fields=M->Node.BoneToModify.BoneName==Bone&&M->Node.Rotation.Equals(Rotation,KINDA_SMALL_NUMBER)&&M->Node.TranslationMode==BMM_Ignore&&M->Node.RotationMode==BMM_Additive&&M->Node.ScaleMode==BMM_Ignore&&M->Node.TranslationSpace==BCS_BoneSpace&&M->Node.RotationSpace==BCS_BoneSpace&&M->Node.ScaleSpace==BCS_BoneSpace&&Root->NodePosX==240&&Root->NodePosY==0;
    bool Default=true;if(auto* P=FindPin(M,TEXT("Rotation"),EGPD_Input)) { Default=P->DefaultValue.Equals(FString::Printf(TEXT("%f,%f,%f"),Rotation.Pitch,Rotation.Yaw,Rotation.Roll),ESearchCase::CaseSensitive)&&Graph->GetSchema()->IsCurrentPinDefaultValid(P).IsEmpty(); }
    UEdGraphPin* A[4]={FindFirstPosePin(Input,EGPD_Output),FindFirstComponentPosePin(L,EGPD_Output),FindFirstComponentPosePin(M,EGPD_Output),FindFirstPosePin(C,EGPD_Output)};
    UEdGraphPin* B[4]={FindFirstPosePin(L,EGPD_Input,{TEXT("Pose")}),FindFirstComponentPosePin(M,EGPD_Input,{TEXT("ComponentPose")}),FindFirstComponentPosePin(C,EGPD_Input,{TEXT("ComponentPose")}),FindFirstPosePin(Root,EGPD_Input,{TEXT("Result")})};
    bool Links=true;TArray<TSharedPtr<FJsonValue>> Edges;for(int32 I=0;I<4;++I) { const bool Ok=PinKnown(A[I])&&PinKnown(B[I])&&A[I]->LinkedTo.Contains(B[I])&&B[I]->LinkedTo.Contains(A[I]);Links&=Ok;auto E=MakeShared<FJsonObject>();E->SetNumberField(TEXT("index"),I);E->SetBoolField(TEXT("reciprocal_current_link"),Ok);if(PinKnown(A[I])) { E->SetObjectField(TEXT("source_pin"),PinToJson(A[I])); }if(PinKnown(B[I])) { E->SetObjectField(TEXT("target_pin"),PinToJson(B[I])); }Edges.Add(MakeShared<FJsonValueObject>(E)); }
    CurrentResult->Data->SetArrayField(TEXT("current_chain_edges"),Edges);CurrentResult->Data->SetBoolField(TEXT("current_bone_settings_verified"),Fields);CurrentResult->Data->SetBoolField(TEXT("current_rotation_default_verified"),Default);CurrentResult->Data->SetBoolField(TEXT("current_chain_topology_verified"),Links);return Fields&&Default&&Links&&!Incomplete();
}
void Init(FResult& R)
{
    R.Data->SetStringField(TEXT("operation"),TEXT("ensure_anim_graph_modify_bone_demo"));R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("automatic_retry"),false);R.Data->SetBoolField(TEXT("explicit_compile_save_requested"),false);R.Data->SetBoolField(TEXT("total_SDK_load_cache_side_effects_unverified"),true);R.Data->SetBoolField(TEXT("internal_binding_map_contents_not_fully_observed"),true);auto C=MakeShared<FJsonObject>();
    for(const TCHAR* K:{TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("get_all_graphs_calls"),TEXT("pins_inventory_calls"),TEXT("pins_visited"),TEXT("nodes_serialized"),TEXT("source_class_lookups"),TEXT("node_struct_property_search_calls"),TEXT("reflected_property_reads"),TEXT("property_export_calls"),TEXT("state_capture_calls"),TEXT("outer_transaction_calls"),TEXT("event_graph_create_calls"),TEXT("event_graph_create_returns"),TEXT("add_ubergraph_page_calls"),TEXT("bone_field_write_calls"),TEXT("try_set_default_calls"),TEXT("pin_default_changed_calls"),TEXT("mark_modified_calls"),TEXT("reconstruct_calls"),TEXT("notify_graph_changed_calls"),TEXT("asset_editor_readonly_queries"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("node_position_write_calls"),TEXT("root_position_write_calls"),TEXT("graph_add_node_calls"),TEXT("create_node_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("break_all_pin_links_calls"),TEXT("can_create_connection_calls"),TEXT("try_create_connection_calls"),TEXT("try_create_connection_returns"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("runtime_pose_evaluation_calls")}) { C->SetNumberField(K,0); }R.Data->SetObjectField(TEXT("native_call_counts"),C);
}
FResult Finish(FResult& R)
{
    if(Serialize(R.Data).IsEmpty()) { R.Data->RemoveField(TEXT("selected_state_before_json"));R.Data->RemoveField(TEXT("selected_state_after_json"));R.Data->RemoveField(TEXT("legacy_result"));R.Data->SetBoolField(TEXT("oversized_observations_omitted"),true);R.bSuccess=false;R.bPostconditionPassed=false;R.bChangeStateKnown=false;if(R.ErrorCode.IsEmpty()) { R.ErrorCode=TEXT("anim_graph_bone_demo_output_incomplete"); }if(R.Error.IsEmpty()) { R.Error=TEXT("Current observations exceed result bound; actual action prefix retained"); } }return R;
}
FResult Execute(const TSharedRef<FJsonObject>& Params)
{
    FResult R;Init(R);if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { R.ErrorCode=TEXT("anim_graph_bone_demo_editor_invalid");R.Error=TEXT("Idle nonreentrant Editor game thread required");return Finish(R); }
    struct FScope { FScope(FResult& V){CurrentResult=&V;Held.Reset();HeldObjects.Reset();KnownPins.Reset();PropertyValues=0;}~FScope(){CurrentResult=nullptr;CurrentBlueprint=nullptr;Held.Reset();HeldObjects.Reset();KnownPins.Reset();AnimBase=nullptr;AnimStruct=PoseStruct=ComponentPoseStruct=nullptr;} } Scope(R);
    for(const TCHAR* K:{TEXT("blueprint_name"),TEXT("bone_name"),TEXT("graph_name"),TEXT("graph_id"),TEXT("graph_type")}) { if(!SafeText(GetStringParam(Params,K),NAME_SIZE-1)) { R.ErrorCode=TEXT("anim_graph_bone_demo_input_invalid");R.Error=TEXT("Bounded literal strings without interior NUL required before FName/load");return Finish(R); } }
    const FString Bone=GetStringParam(Params,TEXT("bone_name"),TEXT("head")).TrimStartAndEnd();if(Bone.IsEmpty()) { R.ErrorCode=TEXT("anim_graph_bone_demo_input_invalid");R.Error=TEXT("'bone_name' cannot be empty");R.Data->SetObjectField(TEXT("legacy_result"),Failure(R.Error));return Finish(R); }
    if(!RotationSafe(Params)) { R.ErrorCode=TEXT("anim_graph_bone_demo_rotation_invalid");return Finish(R); }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Transaction(FText::FromString(TEXT("MCP ensure_anim_graph_modify_bone_demo")));
    R.Stage=TEXT("load_blueprint");const FString Name=GetStringParam(Params,TEXT("blueprint_name"));UBlueprint* BP=FindBlueprintByName(Name,R);if(!BP||!Hold(BP)) { R.ErrorCode=TEXT("anim_graph_bone_demo_blueprint_not_found");if(R.Error.IsEmpty()){R.Error=TEXT("Blueprint not found: ")+Name;}if(Incomplete()){R.bChangeStateKnown=false;}R.Data->SetObjectField(TEXT("legacy_result"),Failure(R.Error));return Finish(R); }CurrentBlueprint=BP;R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());
    if(!GetBoolParam(Params,TEXT("allow_non_sample"),false)&&!BP->GetPathName().StartsWith(TEXT("/Game/_MCP_Sample/"))) { R.ErrorCode=TEXT("anim_graph_bone_demo_sample_guard");R.Error=FString::Printf(TEXT("Refusing to modify non-sample Anim Blueprint '%s'. Pass allow_non_sample=true only for intentional non-sample edits."),*BP->GetPathName());R.Data->SetObjectField(TEXT("legacy_result"),Failure(R.Error));return Finish(R); }
    R.Stage=TEXT("resolve_graph");UEdGraph* G=Resolve(BP,GetStringParam(Params,TEXT("graph_name")),GetStringParam(Params,TEXT("graph_id")),GetStringParam(Params,TEXT("graph_type")),R);
    if(!G||!Hold(G)) { if(R.ErrorCode.IsEmpty()){R.ErrorCode=TEXT("anim_graph_bone_demo_graph_not_found");}if(R.Error.IsEmpty()){R.Error=TEXT("Selected graph not found");}if(Incomplete()){R.bChangeStateKnown=false;}R.Data->SetObjectField(TEXT("legacy_result"),Failure(R.Error));return Finish(R); }
    if(!G->Schema.Get()||!Hold(G->Schema.Get())||!Hold(G->GetSchema())) { NoteUnsafe(TEXT("Selected graph schema unavailable"));R.bChangeStateKnown=false;return Finish(R); }R.Data->SetObjectField(TEXT("resolved_graph"),GraphToJson(BP,G));
    if(!IsAnimationGraph(G)) { R.ErrorCode=TEXT("anim_graph_bone_demo_graph_type_invalid");R.Error=FString::Printf(TEXT("Target graph is not an AnimationGraph. graph='%s' schema='%s'"),*G->GetName(),*G->GetSchema()->GetClass()->GetName());R.Data->SetObjectField(TEXT("legacy_result"),Failure(R.Error));return Finish(R); }
    if(!Cast<UAnimBlueprint>(BP)||!Hold(BP->GetOutermost())) { NoteUnsafe(TEXT("Loaded AnimBlueprint/package required for typed AnimGraph SDK lifecycle/title scope"));R.bChangeStateKnown=false;return Finish(R); }
    if(!RequiredTypes(G,R)||!RefreshInventory()) { R.ErrorCode=TEXT("anim_graph_bone_demo_source_incomplete");R.bChangeStateKnown=false;return Finish(R); }
    R.Stage=TEXT("before_authoring");const auto Before=Capture(BP);const auto BeforeText=Serialize(Before);R.Data->SetStringField(TEXT("selected_state_before_json"),BeforeText);if(BeforeText.IsEmpty()||Incomplete()){R.bChangeStateKnown=false;return Finish(R);}TSet<FString> Allowed;ChainAllowed(G,FName(*Bone),Allowed);
    R.Stage=TEXT("authoring");const auto Legacy=RunDemo(Params,BP,G);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("legacy_result_returned"),true);
    R.Stage=TEXT("after_authoring");if(!Hold(BP)||!Hold(G)) { R.bChangeStateKnown=false;return Finish(R); }const auto After=Capture(BP);const auto AfterText=Serialize(After);R.Data->SetStringField(TEXT("selected_state_after_json"),AfterText);const bool Difference=!BeforeText.IsEmpty()&&!AfterText.IsEmpty()&&!BeforeText.Equals(AfterText,ESearchCase::CaseSensitive);R.bChanged|=Difference;R.Data->SetBoolField(TEXT("observed_selected_state_changed"),Difference);ChainAllowed(G,FName(*Bone),Allowed);
    const bool Preserved=!Incomplete()&&Preserve(Before,After,G,Allowed);R.Data->SetBoolField(TEXT("unaffected_selected_source_preserved"),Preserved);
    if(Incomplete()||BeforeText.IsEmpty()||AfterText.IsEmpty()) { R.bChangeStateKnown=false;R.ErrorCode=TEXT("anim_graph_bone_demo_observation_incomplete");if(R.Error.IsEmpty()){R.Error=TEXT("Current poststate incomplete; accepted prefix retained without cleanup/retry");}return Finish(R); }R.bChangeStateKnown=true;
    bool LegacySuccess=true;Legacy->TryGetBoolField(TEXT("success"),LegacySuccess);if(!LegacySuccess){R.ErrorCode=TEXT("anim_graph_bone_demo_original_failure");Legacy->TryGetStringField(TEXT("error"),R.Error);R.Data->SetBoolField(TEXT("late_failure_no_automatic_cleanup"),true);return Finish(R);}
    R.bPostconditionPassed=Preserved&&VerifyChain(Params,G,Legacy);if(!R.bPostconditionPassed||Incomplete()){R.ErrorCode=TEXT("anim_graph_bone_demo_postcondition_failed");R.Error=TEXT("Current original settings/default/reciprocal4edge chain or unaffected source proof failed");if(Incomplete()){R.bChangeStateKnown=false;}return Finish(R);}R.bSuccess=true;R.Stage=TEXT("complete");return Finish(R);
}
}
Variable::FResult EnsureModifyBoneDemo(const FString& BlueprintName,const FString& BoneName,const FString& RotationJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample)
{
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("blueprint_name"),BlueprintName);P->SetStringField(TEXT("bone_name"),BoneName);P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_id"),GraphId);P->SetStringField(TEXT("graph_type"),GraphType);P->SetBoolField(TEXT("replace_existing"),bReplaceExisting);P->SetBoolField(TEXT("allow_non_sample"),bAllowNonSample);
    FResult Invalid;Init(Invalid);if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult){Invalid.ErrorCode=TEXT("anim_graph_bone_demo_editor_invalid");Invalid.Error=TEXT("Idle nonreentrant Editor game thread required before private carrier parse");return Invalid;}CurrentResult=&Invalid;const bool Packed=Pack(RotationJson,TEXT("rotation"),P,true);CurrentResult=nullptr;if(!Packed){Invalid.ErrorCode=TEXT("anim_graph_bone_demo_json_invalid");return Invalid;}return Execute(P);
}
}
