// Adapted local-SCS inspection contracts from UE56 Blueprint commands.
// Source provenance and deliberate limits: native-component-read-N.md.
#include "Compatibility/UE58/CubelessComponentApi.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::Component
{
namespace
{
bool Reject(FString& Code, FString& Error, const TCHAR* NewCode, const FString& Message)
{
    Code = NewCode; Error = Message; return false;
}

TArray<TSharedPtr<FJsonValue>> Triple(double X, double Y, double Z)
{
    return { MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y), MakeShared<FJsonValueNumber>(Z) };
}

bool SafeSelector(const FString& Value)
{
    if (Value.Len() >= NAME_SIZE) { return false; }
    for (TCHAR Character : Value) { if (Character < 32 || Character == 127) { return false; } }
    return true;
}

bool IsSimpleExportType(const FProperty* Property)
{
    return Property && (CastField<FBoolProperty>(Property) || CastField<FNumericProperty>(Property)
        || CastField<FEnumProperty>(Property) || CastField<FNameProperty>(Property) || CastField<FStrProperty>(Property));
}

bool HasValidScalarMetadata(const FProperty* Property, FString& Error)
{
    if (!Property) { Error = TEXT("Property metadata is missing."); return false; }
    if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    {
        if (!IsValid(Enum->GetEnum()) || !Enum->GetUnderlyingProperty() || !Enum->GetUnderlyingProperty()->IsInteger())
        { Error = TEXT("Enum metadata or its underlying integer property is missing; moved/deleted enum descriptors cannot be read safely."); return false; }
    }
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        if (!IsValid(Object->PropertyClass)) { Error = TEXT("Object property class metadata is missing."); return false; }
        if (const FClassProperty* Class = CastField<FClassProperty>(Property); Class && !IsValid(Class->MetaClass))
        { Error = TEXT("Class property metaclass metadata is missing."); return false; }
        if (const FSoftClassProperty* Class = CastField<FSoftClassProperty>(Property); Class && !IsValid(Class->MetaClass))
        { Error = TEXT("Soft class property metaclass metadata is missing."); return false; }
    }
    return true;
}

bool ReadValue(const FProperty* Property, const void* Address, int32 MaxChars, TSharedPtr<FJsonValue>& Out, FString& Encoding, FString& Error)
{
    if (!HasValidScalarMetadata(Property, Error)) { return false; }
    Encoding = TEXT("legacy_typed_json");
    if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property)) { Out = MakeShared<FJsonValueBoolean>(Bool->GetPropertyValue(Address)); }
    else if (const FNumericProperty* Number = CastField<FNumericProperty>(Property))
    {
        // UE56 tests FNumericProperty before FByteProperty, so byte enums are
        // numeric in the observed legacy serializer; preserve that ordering.
        if (const FUInt64Property* Unsigned = CastField<FUInt64Property>(Property); Unsigned && Unsigned->GetPropertyValue(Address) > 9007199254740991ULL)
        { Error = TEXT("Integer cannot be represented exactly as JSON double; exact large-integer adapter is not implemented."); return false; }
        const double Value = Number->IsInteger() ? static_cast<double>(Number->GetSignedIntPropertyValue(Address)) : Number->GetFloatingPointPropertyValue(Address);
        if (!FMath::IsFinite(Value) || (Number->IsInteger() && FMath::Abs(Value) > 9007199254740991.0))
        { Error = TEXT("Nonfinite or inexact large integer property values are unsupported."); return false; }
        Out = MakeShared<FJsonValueNumber>(Value);
    }
    else if (const FStrProperty* String = CastField<FStrProperty>(Property)) { Out = MakeShared<FJsonValueString>(String->GetPropertyValue(Address)); }
    else if (const FNameProperty* Name = CastField<FNameProperty>(Property)) { Out = MakeShared<FJsonValueString>(Name->GetPropertyValue(Address).ToString()); }
    else if (const FTextProperty* Text = CastField<FTextProperty>(Property)) { Out = MakeShared<FJsonValueString>(Text->GetPropertyValue(Address).ToString()); }
    else if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    {
        const int64 Value = Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Address);
        Out = MakeShared<FJsonValueString>(Enum->GetEnum()->GetNameStringByValue(Value));
    }
    else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        if (Struct->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector& Value = *static_cast<const FVector*>(Address);
            if (Value.ContainsNaN()) { Error = TEXT("Vector property is nonfinite."); return false; }
            Out = MakeShared<FJsonValueArray>(Triple(Value.X, Value.Y, Value.Z));
        }
        else if (Struct->Struct == TBaseStructure<FRotator>::Get())
        {
            const FRotator& Value = *static_cast<const FRotator*>(Address);
            if (Value.ContainsNaN()) { Error = TEXT("Rotator property is nonfinite."); return false; }
            Out = MakeShared<FJsonValueArray>(Triple(Value.Pitch, Value.Yaw, Value.Roll));
        }
        else if (Struct->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor& Value = *static_cast<const FLinearColor*>(Address);
            if (!FMath::IsFinite(Value.R) || !FMath::IsFinite(Value.G) || !FMath::IsFinite(Value.B) || !FMath::IsFinite(Value.A))
            { Error = TEXT("Linear color property is nonfinite."); return false; }
            Out = MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Value.R), MakeShared<FJsonValueNumber>(Value.G), MakeShared<FJsonValueNumber>(Value.B), MakeShared<FJsonValueNumber>(Value.A)});
        }
        else { Error = TEXT("This struct's arbitrary native export is not audited; only FVector/FRotator/FLinearColor are supported in this slice."); return false; }
    }
    else if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        const UObject* Value = Object->GetObjectPropertyValue(Address);
        Out = MakeShared<FJsonValueString>(IsValid(Value) ? Value->GetPathName() : FString());
    }
    else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        // Validate inner metadata even for an empty array: GetCPPType and the
        // legacy exporter can otherwise assert on a moved/deleted enum class.
        if (!HasValidScalarMetadata(Array->Inner, Error)) { return false; }
        FScriptArrayHelper Helper(Array, Address);
        if (!IsSimpleExportType(Array->Inner) || Helper.Num() > 256) { Error = TEXT("Only arrays of up to 256 plain scalar/name/string/enum elements have a bounded legacy text export in this slice."); return false; }
        int64 ApproximateChars = 2;
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            TSharedPtr<FJsonValue> Item; FString ItemEncoding;
            if (!ReadValue(Array->Inner, Helper.GetRawPtr(Index), MaxChars, Item, ItemEncoding, Error)) { return false; }
            // Conservative bound for quoted/escaped string export. It rejects
            // some valid long arrays rather than running an unbounded exporter.
            ApproximateChars += Item->Type == EJson::String ? static_cast<int64>(Item->AsString().Len()) * 6 + 3 : 64;
            if (ApproximateChars > MaxChars) { Error = TEXT("Array text may exceed the bounded export budget."); return false; }
        }
        FString Exported;
        Property->ExportTextItem_Direct(Exported, Address, nullptr, nullptr, PPF_None);
        Out = MakeShared<FJsonValueString>(Exported);
        Encoding = TEXT("legacy_ue_export_text_bounded_scalar_array");
    }
    else { Error = TEXT("This property type requires an unaudited generic exporter and remains unsupported."); return false; }
    if (Out->Type == EJson::String && Out->AsString().Len() > MaxChars) { Error = TEXT("Property text exceeds max_value_chars."); return false; }
    return true;
}

struct FNodeVisit { USCS_Node* Node = nullptr; FString ParentName; };
}

bool ReadComponentValue(const FProperty* Property, const void* Address, int32 MaxChars,
    TSharedPtr<FJsonValue>& Out, FString& Encoding, FString& Error)
{
    return ReadValue(Property, Address, MaxChars, Out, Encoding, Error);
}

bool ReadLocalComponents(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName,
    bool bReadProperty, int32 MaxComponents, int32 MaxValueChars, FReadResult& Out, FString& Code, FString& Error)
{
    if (!IsInGameThread() || !GEditor) { return Reject(Code, Error, TEXT("editor_game_thread_required"), TEXT("Component inspection requires the editor game thread.")); }
    if (MaxComponents < 1 || MaxComponents > 10000 || MaxValueChars < 64 || MaxValueChars > 65536)
    { return Reject(Code, Error, TEXT("component_budget_invalid"), TEXT("max_components must be 1..10000 and max_value_chars 64..65536.")); }
    if (!SafeSelector(BlueprintPath) || !SafeSelector(ComponentName) || !SafeSelector(PropertyName) || (bReadProperty && (ComponentName.IsEmpty() || PropertyName.IsEmpty())))
    { return Reject(Code, Error, TEXT("component_selector_invalid"), TEXT("Selectors must be shorter than NAME_SIZE with no control characters; property read requires component and property names.")); }
    FString ObjectPath = BlueprintPath;
    if (!ObjectPath.StartsWith(TEXT("/")) || ObjectPath.Contains(TEXT(":")) || ObjectPath.Contains(TEXT("'")))
    { return Reject(Code, Error, TEXT("blueprint_path_invalid"), TEXT("Provide an exact Blueprint package/object path, not a short name, export-text reference or subobject path.")); }
    if (!ObjectPath.Contains(TEXT("."))) { ObjectPath += TEXT(".") + FPackageName::GetLongPackageAssetName(ObjectPath); }
    if (ObjectPath.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(ObjectPath))
    { return Reject(Code, Error, TEXT("blueprint_path_invalid"), TEXT("Blueprint object path is invalid or too long.")); }
    UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *ObjectPath);
    Out.bLoadedDuringRead = Blueprint == nullptr;
    if (!Blueprint) { Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath); }
    if (!Blueprint) { return Reject(Code, Error, TEXT("blueprint_not_found"), TEXT("No UBlueprint asset exists at the exact path.")); }
    Out.BlueprintPath = Blueprint->GetPathName();
    Out.bDirtyObserved = true;
    Out.bDirtyBefore = Blueprint->GetOutermost()->IsDirty();
    Out.bDirtyAfter = Out.bDirtyBefore;
    Out.StatusBefore = static_cast<int32>(Blueprint->Status);
    Out.StatusAfter = Out.StatusBefore;
    const TWeakObjectPtr<UBlueprint> ObservedBlueprint = Blueprint;
    ON_SCOPE_EXIT
    {
        if (ObservedBlueprint.IsValid())
        {
            Out.bDirtyAfter = ObservedBlueprint->GetOutermost()->IsDirty();
            Out.StatusAfter = static_cast<int32>(ObservedBlueprint->Status);
            Out.bDirtyAfterObserved = true;
        }
    };
    const EBlueprintStatus StatusBefore = Blueprint->Status;
    auto& Data = Out.Data;
    Data->SetStringField(TEXT("blueprint_name"), BlueprintPath);
    Data->SetStringField(TEXT("blueprint_path"), Out.BlueprintPath);
    Data->SetStringField(TEXT("enumeration_scope"), TEXT("reachable nodes of this Blueprint's local SCS only"));
    Data->SetBoolField(TEXT("inherited_effective_defaults_verified"), false);
    Data->SetBoolField(TEXT("native_cdo_components_included"), false);
    Data->SetBoolField(TEXT("orphan_allnodes_entries_inspected"), false);
    Data->SetBoolField(TEXT("has_simple_construction_script"), Blueprint->SimpleConstructionScript != nullptr);
    TArray<TSharedPtr<FJsonValue>> Components;
    USCS_Node* Match = nullptr;
    if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
    {
        TArray<FNodeVisit> Pending;
        if (SCS->GetRootNodes().Num() > MaxComponents) { return Reject(Code, Error, TEXT("component_budget_exceeded"), TEXT("Local root node count exceeds the inspection budget.")); }
        for (USCS_Node* Root : SCS->GetRootNodes()) { Pending.Add({Root, FString()}); }
        TSet<USCS_Node*> Visited;
        TSet<FName> Names;
        while (!Pending.IsEmpty())
        {
            const FNodeVisit Current = Pending.Pop(EAllowShrinking::No);
            USCS_Node* Node = Current.Node;
            if (!IsValid(Node)) { return Reject(Code, Error, TEXT("component_tree_invalid"), TEXT("Local SCS contains an invalid node.")); }
            if (Node->GetOuter() != SCS || Node->GetVariableName().IsNone()) { return Reject(Code, Error, TEXT("component_tree_invalid"), TEXT("Node ownership/name is inconsistent with this Blueprint's local SCS.")); }
            if (Visited.Contains(Node) || Names.Contains(Node->GetVariableName())) { return Reject(Code, Error, TEXT("component_tree_ambiguous"), TEXT("Local SCS contains a repeated node/cycle or duplicate variable name.")); }
            if (Visited.Num() >= MaxComponents) { return Reject(Code, Error, TEXT("component_budget_exceeded"), TEXT("Local SCS node count exceeds the inspection budget.")); }
            Visited.Add(Node); Names.Add(Node->GetVariableName()); ++Out.NodeVisits;
            const FString Name = Node->GetVariableName().ToString();
            Out.LocalNodes.Add(Name, Node);
            if (Name == ComponentName) { Match = Node; }
            for (USCS_Node* Child : Node->GetChildNodes())
            {
                if (Pending.Num() + Visited.Num() >= MaxComponents) { return Reject(Code, Error, TEXT("component_budget_exceeded"), TEXT("Local SCS pending traversal exceeds the inspection budget.")); }
                Pending.Add({Child, Name});
            }
            if (!ComponentName.IsEmpty() && Name != ComponentName) { continue; }
            UActorComponent* Template = Node->ComponentTemplate;
            if (!IsValid(Template)) { return Reject(Code, Error, TEXT("component_tree_invalid"), TEXT("Local SCS node has no valid component template.")); }
            if (!IsValid(Node->ComponentClass) || !Template->IsA(Node->ComponentClass))
            { return Reject(Code, Error, TEXT("component_tree_invalid"), TEXT("Selected node class metadata does not match its component template.")); }
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("component_name"), Name);
            Row->SetStringField(TEXT("variable_name"), Name);
            Row->SetStringField(TEXT("node_guid"), Node->VariableGuid.ToString());
            Row->SetStringField(TEXT("component_class"), Node->ComponentClass ? Node->ComponentClass->GetName() : FString());
            Row->SetStringField(TEXT("component_class_path"), Node->ComponentClass ? Node->ComponentClass->GetPathName() : FString());
            Row->SetStringField(TEXT("template_name"), Template ? Template->GetName() : FString());
            Row->SetStringField(TEXT("template_path"), Template ? Template->GetPathName() : FString());
            Row->SetStringField(TEXT("parent_component_name"), Current.ParentName);
            if (const USceneComponent* Scene = Cast<USceneComponent>(Template))
            {
                const FVector Location = Scene->GetRelativeLocation(), Scale = Scene->GetRelativeScale3D();
                const FRotator Rotation = Scene->GetRelativeRotation();
                if (Location.ContainsNaN() || Scale.ContainsNaN() || Rotation.ContainsNaN()) { return Reject(Code, Error, TEXT("component_observation_nonfinite"), TEXT("Component relative transform is nonfinite.")); }
                auto Transform = MakeShared<FJsonObject>();
                Transform->SetArrayField(TEXT("location"), Triple(Location.X, Location.Y, Location.Z));
                Transform->SetArrayField(TEXT("rotation"), Triple(Rotation.Pitch, Rotation.Yaw, Rotation.Roll));
                Transform->SetArrayField(TEXT("scale"), Triple(Scale.X, Scale.Y, Scale.Z));
                Row->SetObjectField(TEXT("relative_transform"), Transform);
            }
            if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Template))
            { Row->SetStringField(TEXT("static_mesh"), Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetPathName() : FString()); }
            Components.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    Components.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
    { return A->AsObject()->GetStringField(TEXT("variable_name")) < B->AsObject()->GetStringField(TEXT("variable_name")); });
    Data->SetArrayField(TEXT("components"), Components);
    Data->SetNumberField(TEXT("component_count"), Components.Num());
    Data->SetNumberField(TEXT("local_reachable_node_count"), Out.NodeVisits);
    if (bReadProperty)
    {
        if (!Match || !Match->ComponentTemplate) { return Reject(Code, Error, TEXT("component_not_found"), TEXT("No local reachable SCS component with that exact variable name and valid template was found.")); }
        UActorComponent* Template = Match->ComponentTemplate;
        FProperty* Property = nullptr;
        for (TFieldIterator<FProperty> It(Template->GetClass()); It; ++It)
        {
            if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase)) { Property = *It; break; }
        }
        if (!Property || Property->ArrayDim != 1) { return Reject(Code, Error, TEXT("component_property_not_found"), TEXT("Property was not found or is a fixed-size native array, which is unsupported.")); }
        TSharedPtr<FJsonValue> Value; FString Encoding;
        if (!ReadValue(Property, Property->ContainerPtrToValuePtr<void>(Template), MaxValueChars, Value, Encoding, Error))
        { Code = TEXT("component_property_type_unsupported"); return false; }
        Data->SetStringField(TEXT("component_name"), ComponentName);
        Data->SetStringField(TEXT("component_class"), Template->GetClass()->GetName());
        Data->SetStringField(TEXT("property_name"), PropertyName);
        Data->SetStringField(TEXT("property_type"), Property->GetCPPType());
        Data->SetStringField(TEXT("property_encoding"), Encoding);
        Data->SetField(TEXT("property_value"), Value);
    }
    Out.bDirtyAfter = Blueprint->GetOutermost()->IsDirty();
    Data->SetBoolField(TEXT("blueprint_status_unchanged"), StatusBefore == Blueprint->Status);
    if (Out.bDirtyAfter != Out.bDirtyBefore || StatusBefore != Blueprint->Status)
    { return Reject(Code, Error, TEXT("component_read_state_changed"), TEXT("Blueprint dirty/status observation changed during the read; inspect the asset before further work.")); }
    Data->SetBoolField(TEXT("success"), true);
    return true;
}
}
