#include "Compatibility/UE58/CubelessAnimGraphInspectionApi.h"
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
namespace Cubeless::UE58::AnimGraphInspection
{
namespace
{
using Variable::FResult;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeText(const FString& S,int32 Limit) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
FResult* CurrentResult=nullptr;TSet<const UEdGraphPin*> KnownPins;int32 PropertyValues=0;UClass* AnimBase=nullptr;UScriptStruct* AnimStruct=nullptr;UScriptStruct* PoseStruct=nullptr;UScriptStruct* ComponentPoseStruct=nullptr;
void Count(FResult& R,const TCHAR* Key,int32 N=1) { auto J=R.Data->GetObjectField(TEXT("native_call_counts"));J->SetNumberField(Key,J->GetNumberField(Key)+N); }
void NoteUnsafe(const TCHAR* Error) { if(CurrentResult) { CurrentResult->Data->SetBoolField(TEXT("observation_incomplete"),true);CurrentResult->ErrorCode=TEXT("anim_graph_observation_incomplete");CurrentResult->Error=Error; } }
bool PinKnown(const UEdGraphPin* P)
{
    if(!P) { return false; }if(CurrentResult) { Count(*CurrentResult,TEXT("pins_visited")); }
    if(!KnownPins.Contains(P)) { NoteUnsafe(TEXT("Linked pin not in current Blueprint inventory; no unsafe pointer dereference or complete-drop claim"));return false; }
    if(P->WasTrashed()) { NoteUnsafe(TEXT("Trashed pin retained only as incomplete scope"));return false; }UEdGraphNode* N=P->GetOwningNodeUnchecked();if(!Loaded(N)||!N->Pins.Contains(const_cast<UEdGraphPin*>(P))) { NoteUnsafe(TEXT("Pin owner does not match live owning array"));return false; }return true;
}
void Init(FResult& R,const TCHAR* Operation)
{
    auto J=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("get_all_graphs_calls"),TEXT("graphs_visited"),TEXT("nodes_visited"),TEXT("nodes_serialized"),TEXT("link_rows_serialized"),TEXT("pins_inventory_calls"),TEXT("pins_visited"),TEXT("source_class_lookups"),TEXT("node_struct_property_search_calls"),TEXT("reflected_property_reads"),TEXT("property_export_calls"),TEXT("state_capture_calls"),TEXT("event_graph_create_calls"),TEXT("event_graph_create_returns"),TEXT("add_ubergraph_page_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("runtime_pose_evaluation_calls")}) { J->SetNumberField(K,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),J);R.Data->SetStringField(TEXT("operation"),Operation);R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("read_only_direct_API"),true);R.Data->SetBoolField(TEXT("runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("total_SDK_cache_or_load_side_effects_verified"),false);R.Data->SetStringField(TEXT("numeric_representation_scope"),TEXT("Original integer/enum JSON double projection and Windows unsigned signed-bit projection retained; arbitrary int64 exact decimal precision is not claimed."));
}
TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Error) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Error);return J; }
template<class T> T* CountedLoad(const FString& Path,FResult& R) { Count(R,TEXT("asset_load_calls"));T* O=LoadObject<T>(nullptr,*Path);Count(R,TEXT("asset_load_returns"));return O; }
bool Layout(const FProperty* P,int32 Size) { return P&&Size>=0&&P->GetOffset_ForInternal()>=0&&P->GetSize()>0&&P->GetOffset_ForInternal()<=Size&&P->GetSize()<=Size-P->GetOffset_ForInternal(); }
bool ValueBudget() { if(CurrentResult) { Count(*CurrentResult,TEXT("reflected_property_reads")); }if(++PropertyValues>65536) { NoteUnsafe(TEXT("Reflected settings value budget exceeded; partial output retained"));return false; }return true; }
FString ValueText(FString S) { if(S.Len()>65536) { NoteUnsafe(TEXT("Reflected string value exceeds finite text budget"));S.LeftInline(65536); }return S; }

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
    if(ObjectPath.IsEmpty()) { return nullptr; }
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
    if (PinType.PinSubCategoryObject.IsValid())
    {
        TypeObject->SetStringField(TEXT("subcategory_object"), PinType.PinSubCategoryObject->GetPathName());
    }
    return TypeObject;
}

bool MatchesNodeFilter(UEdGraphNode* Node, const FString& NodeType, const FString& TitleContains)
{
    if (!Node)
    {
        return false;
    }

    if (!NodeType.IsEmpty())
    {
        const FString ClassName = Node->GetClass()->GetName();
        const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
        if (!ClassName.Contains(NodeType) && !NodeTitle.Contains(NodeType))
        {
            return false;
        }
    }

    if (!TitleContains.IsEmpty() && !Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(TitleContains))
    {
        return false;
    }

    return true;
}

TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (Pin && !PinKnown(Pin))
    {
        PinObject->SetBoolField(TEXT("observation_incomplete"), true);
        return PinObject;
    }
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


bool IsAnimationGraph(const UEdGraph* G) { return G&&G->GetSchema()&&G->GetSchema()->GetClass()->GetName().Contains(TEXT("AnimationGraphSchema")); }
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
FString GetAnimGraphTopologyNodeKind(UEdGraphNode* Node)
{
    if(!Node) { return TEXT("unknown"); }
    const TCHAR* Paths[]={TEXT("/Script/AnimGraph.AnimGraphNode_Root"),TEXT("/Script/AnimGraph.AnimGraphNode_StateMachineBase"),TEXT("/Script/ControlRigDeveloper.AnimGraphNode_ControlRig"),TEXT("/Script/AnimGraph.AnimGraphNode_RigidBody"),TEXT("/Script/AnimGraph.AnimGraphNode_Trail"),TEXT("/Script/AnimGraph.AnimGraphNode_ModifyBone"),TEXT("/Script/AnimGraph.AnimGraphNode_ModifyCurve"),TEXT("/Script/AnimGraph.AnimGraphNode_LinkedInputPose"),TEXT("/Script/AnimGraph.AnimGraphNode_LocalToComponentSpace"),TEXT("/Script/AnimGraph.AnimGraphNode_ComponentToLocalSpace")};
    const TCHAR* Kinds[]={TEXT("root"),TEXT("state_machine"),TEXT("controlrig"),TEXT("rigidbody"),TEXT("trail"),TEXT("modify_bone"),TEXT("modify_curve"),TEXT("linked_input_pose"),TEXT("local_to_component_space"),TEXT("component_to_local_space")};
    for(int32 I=0;I<UE_ARRAY_COUNT(Paths);++I) { if(CurrentResult) { Count(*CurrentResult,TEXT("source_class_lookups")); }UClass* C=FindObject<UClass>(nullptr,Paths[I]);if(Loaded(C)&&C->GetPathName().Equals(Paths[I],ESearchCase::CaseSensitive)&&Node->IsA(C)) { return Kinds[I]; } }return IsAnimNode(Node)?TEXT("anim_graph_node"):TEXT("graph_node");
}
bool IsPoseLinkPin(const UEdGraphPin* P) { return PinKnown(P)&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&P->PinType.PinSubCategoryObject.Get()==PoseStruct; }
bool IsComponentPoseLinkPin(const UEdGraphPin* P) { return PinKnown(P)&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&P->PinType.PinSubCategoryObject.Get()==ComponentPoseStruct; }
UEdGraphPin* FindFirstPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const TArray<FString>& PreferredPinNames = TArray<FString>())
{
    if (!Node)
    {
        return nullptr;
    }

    for (const FString& PreferredName : PreferredPinNames)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin
                && Pin->Direction == Direction
                && !Pin->ParentPin
                && IsPoseLinkPin(Pin)
                && Pin->PinName.ToString().Equals(PreferredName, ESearchCase::IgnoreCase))
            {
                return Pin;
            }
        }
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == Direction && !Pin->ParentPin && IsPoseLinkPin(Pin))
        {
            return Pin;
        }
    }

    return nullptr;
}
UEdGraphPin* FindFirstComponentPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const TArray<FString>& PreferredPinNames = TArray<FString>())
{
    if (!Node)
    {
        return nullptr;
    }

    for (const FString& PreferredName : PreferredPinNames)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin
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
        if (Pin && Pin->Direction == Direction && !Pin->ParentPin && IsComponentPoseLinkPin(Pin))
        {
            return Pin;
        }
    }

    return nullptr;
}
UEdGraphPin* FindPreferredInputPosePinForNode(UEdGraphNode* Node)
{
    if (UEdGraphPin* ComponentPosePin = FindFirstComponentPosePin(Node, EGPD_Input, { TEXT("ComponentPose") }))
    {
        return ComponentPosePin;
    }
    return FindFirstPosePin(Node, EGPD_Input, { TEXT("Pose"), TEXT("Result") });
}
UEdGraphPin* FindPreferredOutputPosePinForNode(UEdGraphNode* Node)
{
    if (UEdGraphPin* ComponentPosePin = FindFirstComponentPosePin(Node, EGPD_Output, { TEXT("Pose"), TEXT("ComponentPose") }))
    {
        return ComponentPosePin;
    }
    return FindFirstPosePin(Node, EGPD_Output, { TEXT("Pose"), TEXT("Result") });
}
TSharedPtr<FJsonObject> AnimGraphTopologyPosePinsToJson(UEdGraphNode* Node)
{
    TArray<TSharedPtr<FJsonValue>> InputPosePins;
    TArray<TSharedPtr<FJsonValue>> OutputPosePins;
    TArray<TSharedPtr<FJsonValue>> InputComponentPosePins;
    TArray<TSharedPtr<FJsonValue>> OutputComponentPosePins;
    int32 LinkedPosePinCount = 0;

    auto AddPosePin = [&LinkedPosePinCount](UEdGraphPin* Pin, TArray<TSharedPtr<FJsonValue>>& Values)
    {
        Values.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
        if (Pin && Pin->LinkedTo.Num() > 0)
        {
            ++LinkedPosePinCount;
        }
    };

    if (Node)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->ParentPin)
            {
                continue;
            }

            if (IsPoseLinkPin(Pin))
            {
                AddPosePin(Pin, Pin->Direction == EGPD_Output ? OutputPosePins : InputPosePins);
            }
            else if (IsComponentPoseLinkPin(Pin))
            {
                AddPosePin(Pin, Pin->Direction == EGPD_Output ? OutputComponentPosePins : InputComponentPosePins);
            }
        }
    }

    TSharedPtr<FJsonObject> PosePinsObject = MakeShared<FJsonObject>();
    PosePinsObject->SetArrayField(TEXT("input_pose_pins"), InputPosePins);
    PosePinsObject->SetArrayField(TEXT("output_pose_pins"), OutputPosePins);
    PosePinsObject->SetArrayField(TEXT("input_component_pose_pins"), InputComponentPosePins);
    PosePinsObject->SetArrayField(TEXT("output_component_pose_pins"), OutputComponentPosePins);
    PosePinsObject->SetNumberField(TEXT("input_pose_pin_count"), InputPosePins.Num());
    PosePinsObject->SetNumberField(TEXT("output_pose_pin_count"), OutputPosePins.Num());
    PosePinsObject->SetNumberField(TEXT("input_component_pose_pin_count"), InputComponentPosePins.Num());
    PosePinsObject->SetNumberField(TEXT("output_component_pose_pin_count"), OutputComponentPosePins.Num());
    PosePinsObject->SetNumberField(TEXT("linked_pose_pin_count"), LinkedPosePinCount);
    PosePinsObject->SetBoolField(TEXT("has_pose_pins"),
        InputPosePins.Num() > 0 ||
        OutputPosePins.Num() > 0 ||
        InputComponentPosePins.Num() > 0 ||
        OutputComponentPosePins.Num() > 0);
    PosePinsObject->SetBoolField(TEXT("has_linked_pose_pins"), LinkedPosePinCount > 0);

    if (UEdGraphPin* PreferredInput = FindPreferredInputPosePinForNode(Node))
    {
        PosePinsObject->SetObjectField(TEXT("preferred_input_pose_pin"), PinToJson(PreferredInput));
    }
    if (UEdGraphPin* PreferredOutput = FindPreferredOutputPosePinForNode(Node))
    {
        PosePinsObject->SetObjectField(TEXT("preferred_output_pose_pin"), PinToJson(PreferredOutput));
    }
    return PosePinsObject;
}
TSharedPtr<FJsonObject> AnimGraphTopologyNodeToJson(
    UEdGraphNode* Node,
    bool bIncludePins,
    bool bIncludePosePins,
    bool bIncludeSettings,
    int32 MaxDepth)
{
    TSharedPtr<FJsonObject> NodeObject = NodeToJson(Node, bIncludePins);
    UEdGraphNode* AnimGraphNode = IsAnimNode(Node)?Node:nullptr;
    NodeObject->SetStringField(TEXT("node_kind"), GetAnimGraphTopologyNodeKind(Node));
    NodeObject->SetBoolField(TEXT("is_anim_graph_node"), AnimGraphNode != nullptr);

    if (bIncludePosePins)
    {
        NodeObject->SetObjectField(TEXT("pose_pins"), AnimGraphTopologyPosePinsToJson(Node));
    }

    if (bIncludeSettings)
    {
        if (AnimGraphNode)
        {
            NodeObject->SetObjectField(TEXT("settings"), AnimGraphNodeSettingsToJson(AnimGraphNode, MaxDepth));
        }
        else
        {
            TSharedPtr<FJsonObject> SettingsObject = MakeShared<FJsonObject>();
            SettingsObject->SetBoolField(TEXT("has_anim_node_struct"), false);
            NodeObject->SetObjectField(TEXT("settings"), SettingsObject);
        }
    }

    return NodeObject;
}
TSharedPtr<FJsonObject> BlueprintLinkToJson(const UBlueprint* Blueprint, UEdGraphPin* Pin, UEdGraphPin* LinkedPin)
{
    Count(*CurrentResult,TEXT("link_rows_serialized"));
    TSharedPtr<FJsonObject> LinkObject = MakeShared<FJsonObject>();
    if (!PinKnown(Pin) || !PinKnown(LinkedPin))
    {
        return LinkObject;
    }

    UEdGraphPin* SourcePin = Pin->Direction == EGPD_Output ? Pin : LinkedPin;
    UEdGraphPin* TargetPin = Pin->Direction == EGPD_Output ? LinkedPin : Pin;
    UEdGraphNode* SourceNode = SourcePin->GetOwningNodeUnchecked();
    UEdGraphNode* TargetNode = TargetPin->GetOwningNodeUnchecked();

    LinkObject->SetStringField(TEXT("source_node_id"), SourceNode->NodeGuid.ToString());
    LinkObject->SetStringField(TEXT("source_node_name"), SourceNode->GetName());
    LinkObject->SetStringField(TEXT("source_node_title"), SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    LinkObject->SetStringField(TEXT("source_pin"), SourcePin->PinName.ToString());
    LinkObject->SetStringField(TEXT("target_node_id"), TargetNode->NodeGuid.ToString());
    LinkObject->SetStringField(TEXT("target_node_name"), TargetNode->GetName());
    LinkObject->SetStringField(TEXT("target_node_title"), TargetNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    LinkObject->SetStringField(TEXT("target_pin"), TargetPin->PinName.ToString());
    LinkObject->SetStringField(TEXT("pin_category"), SourcePin->PinType.PinCategory.ToString());
    LinkObject->SetStringField(TEXT("link_kind"), SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec ? TEXT("exec") : TEXT("data"));

    if (UEdGraph* SourceGraph = SourceNode->GetGraph())
    {
        LinkObject->SetObjectField(TEXT("source_graph"), GraphToJson(Blueprint, SourceGraph));
    }
    if (UEdGraph* TargetGraph = TargetNode->GetGraph())
    {
        LinkObject->SetObjectField(TEXT("target_graph"), GraphToJson(Blueprint, TargetGraph));
    }

    return LinkObject;
}
TSharedPtr<FJsonObject> AnimGraphTopologyLinkToJson(const UBlueprint* Blueprint, UEdGraphPin* Pin, UEdGraphPin* LinkedPin)
{
    TSharedPtr<FJsonObject> LinkObject = BlueprintLinkToJson(Blueprint, Pin, LinkedPin);
    if (!PinKnown(Pin) || !PinKnown(LinkedPin))
    {
        return LinkObject;
    }

    UEdGraphPin* SourcePin = Pin->Direction == EGPD_Output ? Pin : LinkedPin;
    UEdGraphPin* TargetPin = Pin->Direction == EGPD_Output ? LinkedPin : Pin;
    const bool bPoseLink = IsPoseLinkPin(SourcePin) || IsPoseLinkPin(TargetPin);
    const bool bComponentPoseLink = IsComponentPoseLinkPin(SourcePin) || IsComponentPoseLinkPin(TargetPin);
    FString LinkKind = TEXT("data");
    if (bComponentPoseLink)
    {
        LinkKind = TEXT("component_pose");
    }
    else if (bPoseLink)
    {
        LinkKind = TEXT("pose");
    }
    else if (SourcePin && SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
    {
        LinkKind = TEXT("exec");
    }

    LinkObject->SetStringField(TEXT("link_kind"), LinkKind);
    LinkObject->SetBoolField(TEXT("is_pose_link"), bPoseLink);
    LinkObject->SetBoolField(TEXT("is_component_pose_link"), bComponentPoseLink);
    if (SourcePin)
    {
        LinkObject->SetObjectField(TEXT("source_pin_type"), PinTypeToJson(SourcePin->PinType));
    }
    if (TargetPin)
    {
        LinkObject->SetObjectField(TEXT("target_pin_type"), PinTypeToJson(TargetPin->PinType));
    }
    return LinkObject;
}

bool Inventory(const TArray<UEdGraph*>& Graphs,FResult& R)
{
    Count(R,TEXT("pins_inventory_calls"));KnownPins.Reset();int32 Nodes=0;for(UEdGraph* G:Graphs)
    {
        if(!Loaded(G)) { NoteUnsafe(TEXT("Invalid graph in current Blueprint inventory"));continue; }
        for(UEdGraphNode* N:G->Nodes)
        {
            if(++Nodes>65536) { NoteUnsafe(TEXT("Native node inventory budget exceeded"));return false; }if(!Loaded(N)||N->GetGraph()!=G) { NoteUnsafe(TEXT("Node is not a usable member of its current graph"));continue; }
            for(UEdGraphPin* P:N->Pins) { if(!P) { NoteUnsafe(TEXT("Null current pin inventory entry"));continue; }if(KnownPins.Num()>=262144) { NoteUnsafe(TEXT("Native pin inventory budget exceeded"));return false; }KnownPins.Add(P); }
        }
    }return true;
}
UEdGraph* Resolve(UBlueprint* BP,const FString& Name,const FString& Id,const FString& Type,FResult& R)
{
    const FString Kind=NormalizeGraphType(Type);if(!Id.IsEmpty()||!Name.IsEmpty()||!Kind.IsEmpty())
    { TArray<UEdGraph*> Graphs;Count(R,TEXT("get_all_graphs_calls"));BP->GetAllGraphs(Graphs);if(Graphs.Num()>4096) { NoteUnsafe(TEXT("Graph selection budget exceeded"));return nullptr; }for(UEdGraph* G:Graphs) { if(GraphMatchesSelector(BP,G,Id,Name,Type)) { return G; } }R.Error=FString::Printf(TEXT("Blueprint graph not found. graph_name='%s', graph_id='%s', graph_type='%s'"),*Name,*Id,*Type);return nullptr; }
    // Actual legacy common resolver creates a default EventGraph before the AnimationGraph type refusal.
    for(UEdGraph* G:BP->UbergraphPages) { if(!Loaded(G)) { NoteUnsafe(TEXT("Invalid default graph entry"));return nullptr; }if(G->GetName().Contains(TEXT("EventGraph"))) { return G; } }
    TArray<UEdGraph*> Before;BP->GetAllGraphs(Before);if(Before.Num()>4096) { R.Error=TEXT("Default graph creation inventory exceeds finite budget");return nullptr; }TArray<TSharedPtr<FJsonValue>> Old;for(UEdGraph* G:Before) { if(Loaded(G)) { Old.Add(MakeShared<FJsonValueObject>(GraphToJson(BP,G))); }else { NoteUnsafe(TEXT("Invalid graph before legacy default creation"));return nullptr; } }R.Data->SetArrayField(TEXT("graphs_before_default_creation"),Old);
    R.Stage=TEXT("create_default_event_graph");R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("read_only_direct_API"),false);Count(R,TEXT("event_graph_create_calls"));UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());Count(R,TEXT("event_graph_create_returns"));R.Data->SetBoolField(TEXT("default_graph_returned"),G!=nullptr);R.Data->SetStringField(TEXT("default_graph_path"),GetPathNameSafe(G));if(!Loaded(G)) { R.Error=TEXT("Failed to get event graph");return nullptr; }
    const TStrongObjectPtr<UEdGraph> Keep(G);Count(R,TEXT("add_ubergraph_page_calls"));FBlueprintEditorUtils::AddUbergraphPage(BP,G);R.Data->SetBoolField(TEXT("implicit_structural_skeleton_compile_possible"),true);R.Data->SetBoolField(TEXT("default_graph_attached"),BP->UbergraphPages.Contains(G));R.Data->SetBoolField(TEXT("package_dirty_after_default_creation"),BP->GetOutermost()->IsDirty());R.bChanged=true;R.bChangeStateKnown=Loaded(G)&&G->GetOuter()==BP&&BP->UbergraphPages.Contains(G);TArray<UEdGraph*> After;BP->GetAllGraphs(After);TArray<TSharedPtr<FJsonValue>> Rows;for(UEdGraph* A:After) { if(Loaded(A)) { Rows.Add(MakeShared<FJsonValueObject>(GraphToJson(BP,A))); }else { R.bChangeStateKnown=false; } }R.Data->SetArrayField(TEXT("graphs_after_default_creation"),Rows);if(!R.bChangeStateKnown) { R.Error=TEXT("Default graph creation prefix is incomplete; no rollback or retry");return nullptr; }return G;
}
bool RequiredTypes(UEdGraph* G,FResult& R)
{
    Count(R,TEXT("source_class_lookups"),5);AnimBase=FindObject<UClass>(nullptr,TEXT("/Script/AnimGraph.AnimGraphNode_Base"));UClass* Schema=FindObject<UClass>(nullptr,TEXT("/Script/AnimGraph.AnimationGraphSchema"));AnimStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.AnimNode_Base"));PoseStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.PoseLink"));ComponentPoseStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.ComponentSpacePoseLink"));
    if(!Loaded(AnimBase)||!Loaded(Schema)||!Loaded(AnimStruct)||!Loaded(PoseStruct)||!Loaded(ComponentPoseStruct)||!G->GetSchema()->IsA(Schema)) { R.Error=TEXT("Exact already-loaded native AnimGraph/Engine reflection types required; no module force-load or substituted schema");return false; }
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("anim_graph_base"),AnimBase->GetPathName());J->SetStringField(TEXT("schema"),Schema->GetPathName());J->SetStringField(TEXT("anim_node_base"),AnimStruct->GetPathName());J->SetStringField(TEXT("pose_link"),PoseStruct->GetPathName());J->SetStringField(TEXT("component_pose_link"),ComponentPoseStruct->GetPathName());R.Data->SetObjectField(TEXT("loaded_public_reflection_types"),J);return true;
}
FString Capture(UBlueprint* BP,UEdGraph* G,const TSharedPtr<FJsonObject>& Params,bool Topology,bool Settings,int32 Depth,FResult& R)
{
    Count(R,TEXT("state_capture_calls"));auto J=MakeShared<FJsonObject>();J->SetObjectField(TEXT("graph"),GraphToJson(BP,G));J->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());TArray<TSharedPtr<FJsonValue>> Nodes;int32 Included=0;for(UEdGraphNode* N:G->Nodes) { if(!Loaded(N)) { NoteUnsafe(TEXT("Invalid selected node during current state capture"));continue; }auto Row=NodeToJson(N,true);const FString Id=GetStringParam(Params,TEXT("node_id"));const bool MatchId=Id.IsEmpty()||(Topology?N->NodeGuid.ToString().Equals(Id,ESearchCase::IgnoreCase):N->NodeGuid.ToString()==Id);const bool Match=MatchId&&MatchesNodeFilter(N,GetStringParam(Params,TEXT("node_type")),GetStringParam(Params,TEXT("title_contains")));const int32 Limit=Topology?static_cast<int32>(Params->GetNumberField(TEXT("max_nodes"))):-1;if(Settings&&Match&&(Limit<0||Included<Limit)) { ++Included;Row->SetObjectField(TEXT("settings"),AnimGraphNodeSettingsToJson(N,Depth)); }Nodes.Add(MakeShared<FJsonValueObject>(Row)); }J->SetArrayField(TEXT("nodes"),Nodes);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return Text;
}

int32 GetClampedIntParam(const TSharedPtr<FJsonObject>& P,const FString& Key,int32 Default,int32 Min,int32 Max) { double V=Default;P->TryGetNumberField(Key,V);return static_cast<int32>(FMath::Clamp(V,static_cast<double>(Min),static_cast<double>(Max))); }
TSharedPtr<FJsonObject> RunSettings(const TSharedPtr<FJsonObject>& Params,UBlueprint* Blueprint,UEdGraph* TargetGraph,FResult& R)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeId;
    Params->TryGetStringField(TEXT("node_id"), NodeId);

    FString NodeType;
    Params->TryGetStringField(TEXT("node_type"), NodeType);

    FString TitleContains;
    Params->TryGetStringField(TEXT("title_contains"), TitleContains);

    bool bIncludePins = true;
    if (Params->HasField(TEXT("include_pins")))
    {
        bIncludePins = Params->GetBoolField(TEXT("include_pins"));
    }

    int32 MaxDepth = 4;
    if (Params->HasField(TEXT("max_depth")))
    {
        MaxDepth = FMath::Clamp(static_cast<int32>(Params->GetIntegerField(TEXT("max_depth"))), 1, 8);
    }

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        Count(R,TEXT("nodes_visited"));if (!Loaded(Node))
        {
            continue;
        }
        if (!NodeId.IsEmpty() && Node->NodeGuid.ToString() != NodeId)
        {
            continue;
        }
        if (!MatchesNodeFilter(Node, NodeType, TitleContains))
        {
            continue;
        }

        Count(R,TEXT("nodes_serialized"));TSharedPtr<FJsonObject> NodeObject = NodeToJson(Node, bIncludePins);
        if (UEdGraphNode* AnimGraphNode = IsAnimNode(Node)?Node:nullptr)
        {
            NodeObject->SetObjectField(TEXT("settings"), AnimGraphNodeSettingsToJson(AnimGraphNode, MaxDepth));
        }
        else
        {
            TSharedPtr<FJsonObject> SettingsObject = MakeShared<FJsonObject>();
            SettingsObject->SetBoolField(TEXT("has_anim_node_struct"), false);
            NodeObject->SetObjectField(TEXT("settings"), SettingsObject);
        }
        Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
    ResultObj->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
    ResultObj->SetNumberField(TEXT("max_depth"), MaxDepth);
    ResultObj->SetObjectField(TEXT("graph"),GraphToJson(Blueprint,TargetGraph));
    ResultObj->SetArrayField(TEXT("nodes"), Nodes);
    return ResultObj;
}

TSharedPtr<FJsonObject> RunTopology(const TSharedPtr<FJsonObject>& Params,UBlueprint* Blueprint,UEdGraph* TargetGraph,FResult& R)
{
    if (!Params.IsValid())
    {
        return CreateErrorResponse(TEXT("Missing params"));
    }

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    const FString NodeId = GetStringParam(Params, TEXT("node_id"));
    const FString NodeType = GetStringParam(Params, TEXT("node_type"));
    const FString TitleContains = GetStringParam(Params, TEXT("title_contains"));
    const bool bIncludePins = GetBoolParam(Params, TEXT("include_pins"), true);
    const bool bIncludePosePins = GetBoolParam(Params, TEXT("include_pose_pins"), true);
    const bool bIncludeLinks = GetBoolParam(Params, TEXT("include_links"), true);
    const bool bIncludeNonPoseLinks = GetBoolParam(Params, TEXT("include_non_pose_links"), false);
    const bool bIncludeSettings = GetBoolParam(Params, TEXT("include_settings"), false);
    const int32 MaxDepth = GetClampedIntParam(Params, TEXT("max_depth"), 3, 1, 8);
    const int32 MaxNodes = GetClampedIntParam(Params, TEXT("max_nodes"), 512, -1, 10000);
    const int32 MaxLinks = GetClampedIntParam(Params, TEXT("max_links"), 2048, -1, 50000);

    TArray<TSharedPtr<FJsonValue>> NodeValues;
    TArray<TSharedPtr<FJsonValue>> LinkValues;
    TSet<FString> SeenLinkKeys;
    int32 MatchingNodeCount = 0;
    int32 IncludedNodeCount = 0;
    int32 MatchingLinkCount = 0;
    bool bNodesTruncated = false;
    bool bLinksTruncated = false;

    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        Count(R,TEXT("nodes_visited"));if (!Loaded(Node))
        {
            continue;
        }
        if (!NodeId.IsEmpty() && !Node->NodeGuid.ToString().Equals(NodeId, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (!MatchesNodeFilter(Node, NodeType, TitleContains))
        {
            continue;
        }

        ++MatchingNodeCount;
        if (MaxNodes >= 0 && IncludedNodeCount >= MaxNodes)
        {
            bNodesTruncated = true;
            continue;
        }

        Count(R,TEXT("nodes_serialized"));NodeValues.Add(MakeShared<FJsonValueObject>(AnimGraphTopologyNodeToJson(
            Node,
            bIncludePins,
            bIncludePosePins,
            bIncludeSettings,
            MaxDepth)));
        ++IncludedNodeCount;

        if (!bIncludeLinks)
        {
            continue;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!PinKnown(Pin))
            {
                continue;
            }
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (!PinKnown(LinkedPin))
                {
                    continue;
                }

                UEdGraphPin* SourcePin = Pin->Direction == EGPD_Output ? Pin : LinkedPin;
                UEdGraphPin* TargetPin = Pin->Direction == EGPD_Output ? LinkedPin : Pin;
                if (!SourcePin || !TargetPin || !SourcePin->GetOwningNodeUnchecked() || !TargetPin->GetOwningNodeUnchecked())
                {
                    continue;
                }

                const bool bPoseLink = IsPoseLinkPin(SourcePin) || IsPoseLinkPin(TargetPin);
                const bool bComponentPoseLink = IsComponentPoseLinkPin(SourcePin) || IsComponentPoseLinkPin(TargetPin);
                if (!bIncludeNonPoseLinks && !bPoseLink && !bComponentPoseLink)
                {
                    continue;
                }

                const FString LinkKey = FString::Printf(
                    TEXT("%s:%s->%s:%s"),
                    *SourcePin->GetOwningNodeUnchecked()->NodeGuid.ToString(),
                    *SourcePin->PinName.ToString(),
                    *TargetPin->GetOwningNodeUnchecked()->NodeGuid.ToString(),
                    *TargetPin->PinName.ToString());
                if (SeenLinkKeys.Contains(LinkKey))
                {
                    continue;
                }
                SeenLinkKeys.Add(LinkKey);
                ++MatchingLinkCount;

                if (MaxLinks >= 0 && LinkValues.Num() >= MaxLinks)
                {
                    bLinksTruncated = true;
                    continue;
                }

                LinkValues.Add(MakeShared<FJsonValueObject>(AnimGraphTopologyLinkToJson(Blueprint, Pin, LinkedPin)));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetBoolField(TEXT("asset_modified"), false);
    ResultObj->SetBoolField(TEXT("saves_assets"), false);
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
    ResultObj->SetStringField(TEXT("node_id"), NodeId);
    ResultObj->SetStringField(TEXT("node_type"), NodeType);
    ResultObj->SetStringField(TEXT("title_contains"), TitleContains);
    ResultObj->SetBoolField(TEXT("include_pins"), bIncludePins);
    ResultObj->SetBoolField(TEXT("include_pose_pins"), bIncludePosePins);
    ResultObj->SetBoolField(TEXT("include_links"), bIncludeLinks);
    ResultObj->SetBoolField(TEXT("include_non_pose_links"), bIncludeNonPoseLinks);
    ResultObj->SetBoolField(TEXT("include_settings"), bIncludeSettings);
    ResultObj->SetNumberField(TEXT("max_depth"), MaxDepth);
    ResultObj->SetNumberField(TEXT("matching_node_count"), MatchingNodeCount);
    ResultObj->SetNumberField(TEXT("included_node_count"), IncludedNodeCount);
    ResultObj->SetNumberField(TEXT("matching_link_count"), MatchingLinkCount);
    ResultObj->SetNumberField(TEXT("included_link_count"), LinkValues.Num());
    ResultObj->SetBoolField(TEXT("nodes_truncated"), bNodesTruncated);
    ResultObj->SetBoolField(TEXT("links_truncated"), bLinksTruncated);
    ResultObj->SetStringField(TEXT("scope_note"), TEXT("Static AnimGraph editor topology only. This reads protected graph nodes, pins, and pose links; it does not prove runtime execution or sample pose values."));
    ResultObj->SetObjectField(TEXT("graph"),GraphToJson(Blueprint,TargetGraph));
    ResultObj->SetArrayField(TEXT("nodes"), NodeValues);
    ResultObj->SetArrayField(TEXT("links"), LinkValues);
    return ResultObj;
}


FResult Execute(bool Topology,const TSharedPtr<FJsonObject>& Params)
{
    FResult R;Init(R,Topology?TEXT("inspect_anim_graph_protected_topology"):TEXT("inspect_anim_graph_node_settings"));
    if(!IsEditorGameThread()||IsPlaySessionActive()||CurrentResult) { R.ErrorCode=TEXT("anim_graph_editor_state_invalid");R.Error=TEXT("Editor game thread outside PIE/SIE and nonreentrant call required; no runtime pose access");return R; }
    struct FScope { FScope(FResult& R) { CurrentResult=&R;PropertyValues=0; }~FScope() { CurrentResult=nullptr;KnownPins.Reset();AnimBase=nullptr;AnimStruct=PoseStruct=ComponentPoseStruct=nullptr; } } Scope(R);
    const FString Name=GetStringParam(Params,TEXT("blueprint_name"));for(const TCHAR* Key:{TEXT("blueprint_name"),TEXT("node_id"),TEXT("node_type"),TEXT("title_contains"),TEXT("graph_name"),TEXT("graph_id"),TEXT("graph_type")}) { if(!SafeText(GetStringParam(Params,Key),65536)) { R.ErrorCode=TEXT("anim_graph_input_text_invalid");R.Error=TEXT("Bounded strings without interior NUL required; no selector/type normalization by adapter");return R; } }
    R.Stage=TEXT("load_blueprint");UBlueprint* BP=FindBlueprintByName(Name,R);if(!Loaded(BP)) { R.bChangeStateKnown=BP==nullptr;R.ErrorCode=TEXT("anim_graph_blueprint_not_found");R.Error=FString::Printf(TEXT("Blueprint not found: %s"),*Name);R.Data->SetObjectField(TEXT("legacy_result"),CreateErrorResponse(R.Error));return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());R.Data->SetBoolField(TEXT("package_dirty_after_load"),BP->GetOutermost()->IsDirty());
    R.Stage=TEXT("resolve_graph");UEdGraph* G=Resolve(BP,GetStringParam(Params,TEXT("graph_name")),GetStringParam(Params,TEXT("graph_id")),GetStringParam(Params,TEXT("graph_type")),R);if(!G) { R.ErrorCode=TEXT("anim_graph_resolution_failed");if(R.Error.IsEmpty()) { R.Error=TEXT("Failed to get target graph"); }R.Data->SetObjectField(TEXT("legacy_result"),CreateErrorResponse(R.Error));return R; }const TStrongObjectPtr<UEdGraph> KeepGraph(G);R.Data->SetObjectField(TEXT("resolved_graph"),GraphToJson(BP,G));
    if(!IsAnimationGraph(G)) { R.ErrorCode=TEXT("anim_graph_type_invalid");R.Error=FString::Printf(TEXT("Target graph is not an AnimationGraph. graph='%s' schema='%s'"),*G->GetName(),G->GetSchema()?*G->GetSchema()->GetClass()->GetName():TEXT("<none>"));R.Data->SetObjectField(TEXT("legacy_result"),CreateErrorResponse(R.Error));return R; }
    if(!RequiredTypes(G,R)) { R.ErrorCode=TEXT("anim_graph_source_types_unavailable");R.Data->SetObjectField(TEXT("legacy_result"),CreateErrorResponse(R.Error));return R; }const TStrongObjectPtr<UClass> KeepBase(AnimBase);const TStrongObjectPtr<UScriptStruct> KeepAnim(AnimStruct),KeepPose(PoseStruct),KeepComponentPose(ComponentPoseStruct);
    TArray<UEdGraph*> Graphs;Count(R,TEXT("get_all_graphs_calls"));BP->GetAllGraphs(Graphs);if(Graphs.Num()>4096||!Graphs.Contains(G)) { NoteUnsafe(TEXT("Selected graph identity/inventory budget does not close"));R.bChangeStateKnown=false;return R; }TArray<TStrongObjectPtr<UEdGraph>> KeepGraphs;TArray<TStrongObjectPtr<UEdGraphNode>> KeepNodes;for(UEdGraph* A:Graphs) { if(Loaded(A)) { KeepGraphs.Emplace(A);for(UEdGraphNode* N:A->Nodes) { if(Loaded(N)) { KeepNodes.Emplace(N); } } } }
    if(!Inventory(Graphs,R)) { R.bChangeStateKnown=false;return R; }const int32 Depth=GetClampedIntParam(Params,TEXT("max_depth"),Topology?3:4,1,8);const bool Settings=!Topology||GetBoolParam(Params,TEXT("include_settings"),false);R.Stage=TEXT("before_observation");const FString Before=Capture(BP,G,Params,Topology,Settings,Depth,R);const bool Dirty=BP->GetOutermost()->IsDirty();R.Data->SetStringField(TEXT("selected_state_before_json"),Before);R.Data->SetBoolField(TEXT("package_dirty_before"),Dirty);
    R.Stage=TEXT("observation");Count(R,TEXT("graphs_visited"));const auto Legacy=Topology?RunTopology(Params,BP,G,R):RunSettings(Params,BP,G,R);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("legacy_result_returned"),true);
    R.Stage=TEXT("after_observation");const FString After=Capture(BP,G,Params,Topology,Settings,Depth,R);R.Data->SetStringField(TEXT("selected_state_after_json"),After);R.Data->SetBoolField(TEXT("package_dirty_after"),BP->GetOutermost()->IsDirty());const bool Same=Before.Equals(After,ESearchCase::CaseSensitive);R.Data->SetBoolField(TEXT("selected_state_preserved"),Same);R.bChanged|=!Same;
    if(R.Data->GetBoolField(TEXT("observation_incomplete"))) { R.bChangeStateKnown=false;R.Stage=TEXT("observation_incomplete");return R; }if(!Same) { R.bChangeStateKnown=true;R.ErrorCode=TEXT("anim_graph_selected_state_changed");R.Error=TEXT("Getter/export selected authored state changed; full current prefix retained without retry");return R; }
    R.bChangeStateKnown=true;R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
Variable::FResult Settings(const FString& BlueprintName,const FString& NodeId,const FString& NodeType,const FString& TitleContains,bool bIncludePins,int64 MaxDepth,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("blueprint_name"),BlueprintName);P->SetStringField(TEXT("node_id"),NodeId);P->SetStringField(TEXT("node_type"),NodeType);P->SetStringField(TEXT("title_contains"),TitleContains);P->SetBoolField(TEXT("include_pins"),bIncludePins);P->SetNumberField(TEXT("max_depth"),static_cast<int32>(FMath::Clamp<int64>(MaxDepth,1,8)));P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_id"),GraphId);P->SetStringField(TEXT("graph_type"),GraphType);return Execute(false,P);
}
Variable::FResult Topology(const FString& BlueprintName,const FString& NodeId,const FString& NodeType,const FString& TitleContains,bool bIncludePins,bool bIncludePosePins,bool bIncludeLinks,bool bIncludeNonPoseLinks,bool bIncludeSettings,int64 MaxDepth,int64 MaxNodes,int64 MaxLinks,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("blueprint_name"),BlueprintName);P->SetStringField(TEXT("node_id"),NodeId);P->SetStringField(TEXT("node_type"),NodeType);P->SetStringField(TEXT("title_contains"),TitleContains);P->SetBoolField(TEXT("include_pins"),bIncludePins);P->SetBoolField(TEXT("include_pose_pins"),bIncludePosePins);P->SetBoolField(TEXT("include_links"),bIncludeLinks);P->SetBoolField(TEXT("include_non_pose_links"),bIncludeNonPoseLinks);P->SetBoolField(TEXT("include_settings"),bIncludeSettings);P->SetNumberField(TEXT("max_depth"),static_cast<int32>(FMath::Clamp<int64>(MaxDepth,1,8)));P->SetNumberField(TEXT("max_nodes"),static_cast<int32>(FMath::Clamp<int64>(MaxNodes,-1,10000)));P->SetNumberField(TEXT("max_links"),static_cast<int32>(FMath::Clamp<int64>(MaxLinks,-1,50000)));P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_id"),GraphId);P->SetStringField(TEXT("graph_type"),GraphType);return Execute(true,P);
}
}
