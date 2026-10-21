// CP: independent owner for one original empty source-collection default.
#include "Registration/CubelessMaterialReferenceDefaults.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr const TCHAR* MaterialClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools");
constexpr const TCHAR* ConverterName = TEXT("CubelessMCP_UE58.MaterialReferenceDefaults.v1");
constexpr const TCHAR* PreparedEmpty = TEXT("\"\"");
constexpr const TCHAR* MethodName = TEXT("replace_material_collection_references");
constexpr const TCHAR* PropertyName = TEXT("source_collection_path");
constexpr const TCHAR* DefaultKey = TEXT("CPP_Default_source_collection_path");

const FStrProperty* MatchMaterialReferenceSource(const FProperty* Property)
{
    const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
    const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
    if (!Property || !Owner || Owner->GetPathName() != MaterialClass
        || Function->GetName() != MethodName || Property->GetName() != PropertyName
        || Property->ArrayDim != 1 || !Property->HasAllPropertyFlags(CPF_Parm)
        || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
        || !Function->HasMetaData(DefaultKey) || !Function->GetMetaData(PropertyName).IsEmpty())
    { return nullptr; }
    const FString Text = Function->GetMetaData(DefaultKey);
    return (Text.IsEmpty() || Text == PreparedEmpty) ? CastField<FStrProperty>(Property) : nullptr;
}

TSharedPtr<FJsonValue> MaterialReferenceDefault(const FProperty* Property, const FString& Text)
{
    const auto String = MatchMaterialReferenceSource(Property);
    if (!String || (!Text.IsEmpty() && Text != PreparedEmpty)) { return nullptr; }
    FDefaultConstructedPropertyElement Scratch(String);
    const TCHAR* End = String->ImportText_Direct(PreparedEmpty, Scratch.GetObjAddress(), nullptr, PPF_Delimited);
    if (!End || *End != TCHAR('\0') || !String->GetPropertyValue(Scratch.GetObjAddress()).IsEmpty()) { return nullptr; }
    return MakeShared<FJsonValueString>(FString());
}

// DA: one original empty node selector on the same owned converter.
const FStrProperty* MatchExpansionNodeId(const FProperty* Property)
{
    const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
    const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
    if (!Property || !Owner || Owner->GetPathName() != MaterialClass
        || Function->GetName() != TEXT("expand_material_function_calls") || Property->GetName() != TEXT("node_id")
        || Property->ArrayDim != 1 || !Property->HasAllPropertyFlags(CPF_Parm)
        || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
        || !Function->HasMetaData(TEXT("CPP_Default_node_id")) || !Function->GetMetaData(TEXT("node_id")).IsEmpty())
    { return nullptr; }
    const FString Text = Function->GetMetaData(TEXT("CPP_Default_node_id"));
    return (Text.IsEmpty() || Text == PreparedEmpty) ? CastField<FStrProperty>(Property) : nullptr;
}

TSharedPtr<FJsonValue> ExpansionNodeIdDefault(const FProperty* Property, const FString& Text)
{
    const auto String = MatchExpansionNodeId(Property);
    if (!String || (!Text.IsEmpty() && Text != PreparedEmpty)) { return nullptr; }
    FDefaultConstructedPropertyElement Scratch(String);
    const TCHAR* End = String->ImportText_Direct(PreparedEmpty, Scratch.GetObjAddress(), nullptr, PPF_Delimited);
    if (!End || *End != TCHAR('\0') || !String->GetPropertyValue(Scratch.GetObjAddress()).IsEmpty()) { return nullptr; }
    return MakeShared<FJsonValueString>(FString());
}

// CY: three exact inputs use the existing owned converter; old CP matching stays unchanged.
enum class ESyncInput { Unhandled, Source, Target, Names };
ESyncInput MatchSyncInput(const FProperty* Property)
{
    const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
    const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
    if (!Property || !Owner || Owner->GetPathName() != MaterialClass
        || Function->GetName() != TEXT("set_material_parameter_collection_sync")
        || Property->ArrayDim != 1 || !Property->HasAllPropertyFlags(CPF_Parm)
        || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)) { return ESyncInput::Unhandled; }
    const FString Name = Property->GetName();
    const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), *Name));
    if (!Function->HasMetaData(Key) || !Function->GetMetaData(*Name).IsEmpty()) { return ESyncInput::Unhandled; }
    const FString Text = Function->GetMetaData(Key);
    if ((Name == TEXT("source_collection_path") || Name == TEXT("target_collection_path"))
        && CastField<FStrProperty>(Property) && (Text.IsEmpty() || Text == PreparedEmpty))
    { return Name == TEXT("source_collection_path") ? ESyncInput::Source : ESyncInput::Target; }
    const auto Array = CastField<FArrayProperty>(Property);
    if (Name == TEXT("parameter_names") && Array && CastField<FStrProperty>(Array->Inner)
        && (Text.IsEmpty() || Text == TEXT("()"))) { return ESyncInput::Names; }
    return ESyncInput::Unhandled;
}

TSharedPtr<FJsonValue> SyncDefault(const FProperty* Property, const FString& Text)
{
    const ESyncInput Kind = MatchSyncInput(Property);
    if (Kind == ESyncInput::Names)
    {
        if (!Text.IsEmpty() && Text != TEXT("()")) { return nullptr; }
        const auto Array = CastFieldChecked<FArrayProperty>(Property);
        FDefaultConstructedPropertyElement Scratch(Array);
        const TCHAR* End = Array->ImportText_Direct(TEXT("()"), Scratch.GetObjAddress(), nullptr, PPF_None);
        if (!End || *End != TCHAR('\0') || FScriptArrayHelper(Array, Scratch.GetObjAddress()).Num() != 0) { return nullptr; }
        return MakeShared<FJsonValueNull>();
    }
    if (Kind != ESyncInput::Source && Kind != ESyncInput::Target) { return nullptr; }
    if (!Text.IsEmpty() && Text != PreparedEmpty) { return nullptr; }
    const auto String = CastFieldChecked<FStrProperty>(Property);
    FDefaultConstructedPropertyElement Scratch(String);
    const TCHAR* End = String->ImportText_Direct(PreparedEmpty, Scratch.GetObjAddress(), nullptr, PPF_Delimited);
    if (!End || *End != TCHAR('\0') || !String->GetPropertyValue(Scratch.GetObjAddress()).IsEmpty()) { return nullptr; }
    return MakeShared<FJsonValueString>(FString());
}

TSharedPtr<FJsonObject> SyncType(ESyncInput Kind)
{
    auto Json = MakeShared<FJsonObject>();
    if (Kind == ESyncInput::Source || Kind == ESyncInput::Target) { Json->SetStringField(TEXT("type"), TEXT("string")); }
    if (Kind == ESyncInput::Names)
    {
        Json->SetArrayField(TEXT("type"), {MakeShared<FJsonValueString>(TEXT("array")), MakeShared<FJsonValueString>(TEXT("null"))});
        auto Items = MakeShared<FJsonObject>(); Items->SetStringField(TEXT("type"), TEXT("string"));
        Json->SetObjectField(TEXT("items"), Items);
    }
    return Json;
}

bool SyncTypeMatches(const TSharedPtr<FJsonObject>& Json, ESyncInput Kind)
{
    if (!Json.IsValid()) { return false; }
    if (Kind == ESyncInput::Source || Kind == ESyncInput::Target)
    { FString Type; return Json->Values.Num() == 1 && Json->TryGetStringField(TEXT("type"), Type) && Type == TEXT("string"); }
    if (Kind != ESyncInput::Names || Json->Values.Num() != 2) { return false; }
    const TArray<TSharedPtr<FJsonValue>>* Types = nullptr; const TSharedPtr<FJsonObject>* Items = nullptr;
    FString ItemType;
    return Json->TryGetArrayField(TEXT("type"), Types) && Types && Types->Num() == 2
        && (*Types)[0].IsValid() && (*Types)[0]->Type == EJson::String && (*Types)[0]->AsString() == TEXT("array")
        && (*Types)[1].IsValid() && (*Types)[1]->Type == EJson::String && (*Types)[1]->AsString() == TEXT("null")
        && Json->TryGetObjectField(TEXT("items"), Items) && Items && (*Items).IsValid() && (*Items)->Values.Num() == 1
        && (*Items)->TryGetStringField(TEXT("type"), ItemType) && ItemType == TEXT("string");
}

bool SyncDefaultMatches(const TSharedPtr<FJsonValue>& Value, ESyncInput Kind)
{
    return Value.IsValid() && (Kind == ESyncInput::Names ? Value->Type == EJson::Null
        : (Kind == ESyncInput::Source || Kind == ESyncInput::Target) && Value->Type == EJson::String && Value->AsString().IsEmpty());
}

bool ImportSyncNames(const TSharedPtr<FJsonValue>& Value, FProperty* Property, void* Out)
{
    if (!Out || MatchSyncInput(Property) != ESyncInput::Names || !Value.IsValid()
        || (Value->Type != EJson::Null && Value->Type != EJson::Array)) { return false; }
    TArray<TSharedPtr<FJsonValue>> Items;
    if (Value->Type == EJson::Array) { Items = Value->AsArray(); }
    for (const auto& Item : Items) { if (!Item.IsValid() || Item->Type != EJson::String) { return false; } }
    const auto Array = CastFieldChecked<FArrayProperty>(Property);
    FDefaultConstructedPropertyElement Scratch(Array); FScriptArrayHelper Helper(Array, Scratch.GetObjAddress());
    Helper.AddValues(Items.Num());
    for (int32 I = 0; I < Items.Num(); ++I) { CastFieldChecked<FStrProperty>(Array->Inner)->SetPropertyValue(Helper.GetRawPtr(I), Items[I]->AsString()); }
    Array->CopyCompleteValue(Out, Scratch.GetObjAddress());
    return true;
}

class FMaterialReferenceDefaultsConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
{
public:
    virtual FString GetName() const override { return ConverterName; }
    virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override
    { return MatchMaterialReferenceSource(Property) != nullptr || MatchExpansionNodeId(Property) != nullptr || MatchSyncInput(Property) != ESyncInput::Unhandled; }
    virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
    {
        const ESyncInput Kind = MatchSyncInput(Property);
        if (Kind != ESyncInput::Unhandled) { return SyncType(Kind); }
        auto Json = MakeShared<FJsonObject>();
        if (MatchMaterialReferenceSource(Property) || MatchExpansionNodeId(Property)) { Json->SetStringField(TEXT("type"), TEXT("string")); }
        return Json;
    }
    virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> Property, const FString& Text) override
    {
        auto Default = MatchSyncInput(Property) != ESyncInput::Unhandled ? SyncDefault(Property, Text)
            : MatchExpansionNodeId(Property) ? ExpansionNodeIdDefault(Property, Text) : MaterialReferenceDefault(Property, Text);
        if (Default.IsValid()) { return Default; }
        UKismetSystemLibrary::RaiseScriptError(TEXT("Exact Material source-collection empty default invalid."));
        return MakeShared<FJsonValueNull>();
    }
    virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
    { return Value && (MatchMaterialReferenceSource(Property) || MatchExpansionNodeId(Property) || MatchSyncInput(Property) != ESyncInput::Unhandled) ? FJsonObjectConverter::UPropertyToJsonValue(Property, Value) : nullptr; }
    virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& Value, TNotNull<FProperty*> Property, void* Out, UObject* Outer) override
    {
        const ESyncInput Kind = MatchSyncInput(Property);
        if (Kind == ESyncInput::Names) { return ImportSyncNames(Value, Property, Out); }
        const auto String = (Kind == ESyncInput::Source || Kind == ESyncInput::Target) ? CastField<FStrProperty>(Property)
            : MatchExpansionNodeId(Property) ? MatchExpansionNodeId(Property) : MatchMaterialReferenceSource(Property);
        if (!Out || !String || !Value.IsValid() || Value->Type != EJson::String) { return false; }
        String->SetPropertyValue(Out, Value->AsString());
        return true;
    }
};

TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeMaterialReferenceDefaultsConverter()
{ return MakeShared<FMaterialReferenceDefaultsConverter>(); }
}

bool Cubeless::Registration::VerifyMaterialReferenceToolset(UClass* Class, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    auto Defaults = MakeShared<FJsonObject>();
    Report->SetObjectField(TEXT("defaults"), Defaults);
    auto Row = MakeShared<FJsonObject>();
    Report->SetArrayField(TEXT("property_observations"), {MakeShared<FJsonValueObject>(Row)});
    Row->SetStringField(TEXT("property"), PropertyName);
    Report->SetBoolField(TEXT("actual_call_packing_verified"), false);
    if (!IsInGameThread() || !Class || Class->GetPathName() != MaterialClass
        || !Class->IsChildOf(UToolsetDefinition::StaticClass()))
    { Error = TEXT("Exact canonical MaterialTools class on the game thread required."); return false; }
    if (UToolsetRegistry::IsToolsetClassRegistered(Class))
    { Error = TEXT("Only the unregistered canonical MaterialTools class can be prepared; no metadata retry."); return false; }

    UFunction* Function = Class->FindFunctionByName(MethodName);
    UFunction* TextureFunction = Class->FindFunctionByName(TEXT("replace_material_texture_references"));
    if (!Function || !TextureFunction || Function->GetOwnerClass() != Class || TextureFunction->GetOwnerClass() != Class)
    { Error = TEXT("Original owned Material reference methods2 required."); return false; }
    const auto Property = FindFProperty<FProperty>(Function, PropertyName);
    const FName Key(DefaultKey);
    const FString Prior = Function->GetMetaData(Key);
    Row->SetBoolField(TEXT("metadata_present_before"), Function->HasMetaData(Key));
    Row->SetStringField(TEXT("metadata_before"), Prior);
    if (!MatchMaterialReferenceSource(Property))
    { Error = TEXT("Exact original scalar string-empty declaration required before metadata write."); return false; }

    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Verify Material source-collection default"));
    if (!Subsystem.HasValue()) { Error = TEXT("Registry unavailable; no metadata retry."); return false; }
    auto ActiveConverter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
    const bool Active = ActiveConverter.IsValid() && ActiveConverter->GetName() == ConverterName;
    Row->SetBoolField(TEXT("exact_converter_active"), Active);
    if (!Active) { Error = TEXT("Exact Material source-collection converter unavailable."); return false; }
    auto Type = ActiveConverter->PropertyToJsonSchema(Property);
    auto Default = MaterialReferenceDefault(Property, Prior);
    FString Kind;
    if (!Type.IsValid() || Type->Values.Num() != 1 || !Type->TryGetStringField(TEXT("type"), Kind)
        || Kind != TEXT("string") || !Default.IsValid() || Default->Type != EJson::String || !Default->AsString().IsEmpty())
    { Error = TEXT("Material reference type/default proof failed before metadata write."); return false; }
    Row->SetObjectField(TEXT("type_schema"), Type);
    Row->SetField(TEXT("public_default"), Default);

    if (Prior.IsEmpty())
    {
        Report->SetBoolField(TEXT("metadata_mutation_attempted"), true);
        Report->SetNumberField(TEXT("metadata_write_attempt_count"), 1);
        Function->SetMetaData(Key, PreparedEmpty);
        const bool Changed = Prior != Function->GetMetaData(Key);
        Report->SetBoolField(TEXT("metadata_changed"), Changed);
        Report->SetNumberField(TEXT("metadata_changed_count"), Changed ? 1 : 0);
    }
    Row->SetStringField(TEXT("metadata_after"), Function->GetMetaData(Key));
    ActiveConverter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
    if (!ActiveConverter.IsValid() || ActiveConverter->GetName() != ConverterName || Function->GetMetaData(Key) != PreparedEmpty)
    { Error = TEXT("Material reference metadata/converter readback failed; actual RAM prefix retained."); return false; }
    Default = ActiveConverter->PropertyToDefault(Property, Function->GetMetaData(Key));
    if (!Default.IsValid() || Default->Type != EJson::String || !Default->AsString().IsEmpty())
    { Error = TEXT("Prepared Material source-collection default differs."); return false; }

    Defaults->SetField(TEXT("replace_material_collection_references.source_collection_path"), Default);
    Report->SetBoolField(TEXT("default_verified"), true);
    Report->SetBoolField(TEXT("exact_converter_active"), true);
    Report->SetBoolField(TEXT("type_schema_verified"), true);
    Report->SetNumberField(TEXT("verified_property_count"), 1);
    Row->SetBoolField(TEXT("type_schema_verified"), true);
    Row->SetBoolField(TEXT("default_verified"), true);
    return true;
}

bool Cubeless::Registration::VerifyMaterialFunctionExpansionToolset(UClass* Class, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    auto Defaults = MakeShared<FJsonObject>();
    Report->SetObjectField(TEXT("defaults"), Defaults);
    auto Row = MakeShared<FJsonObject>();
    Report->SetArrayField(TEXT("property_observations"), {MakeShared<FJsonValueObject>(Row)});
    Row->SetStringField(TEXT("property"), TEXT("node_id"));
    Row->SetBoolField(TEXT("metadata_write_attempted"), false);
    Row->SetBoolField(TEXT("metadata_write_returned"), false);
    Report->SetBoolField(TEXT("actual_call_packing_verified"), false);
    if (!IsInGameThread() || !Class || Class->GetPathName() != MaterialClass
        || !Class->IsChildOf(UToolsetDefinition::StaticClass()))
    { Error = TEXT("Exact canonical MaterialTools class on the game thread required."); return false; }
    if (UToolsetRegistry::IsToolsetClassRegistered(Class))
    { Error = TEXT("Only the unregistered canonical MaterialTools class can be prepared; no metadata retry."); return false; }

    UFunction* Function = Class->FindFunctionByName(TEXT("expand_material_function_calls"));
    if (!Function || Function->GetOwnerClass() != Class)
    { Error = TEXT("Original owned Material expansion method required."); return false; }
    const auto Property = FindFProperty<FProperty>(Function, TEXT("node_id"));
    const FName Key(TEXT("CPP_Default_node_id"));
    const FString Prior = Function->GetMetaData(Key);
    Row->SetBoolField(TEXT("metadata_present_before"), Function->HasMetaData(Key));
    Row->SetStringField(TEXT("metadata_before"), Prior);
    if (!MatchExpansionNodeId(Property))
    { Error = TEXT("Exact original scalar string-empty declaration required before metadata write."); return false; }

    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Verify Material expansion node selector default"));
    if (!Subsystem.HasValue()) { Error = TEXT("Registry unavailable; no metadata retry."); return false; }
    auto ActiveConverter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
    const bool Active = ActiveConverter.IsValid() && ActiveConverter->GetName() == ConverterName;
    Row->SetBoolField(TEXT("exact_converter_active"), Active);
    if (!Active) { Error = TEXT("Exact Material expansion node selector converter unavailable."); return false; }
    auto Type = ActiveConverter->PropertyToJsonSchema(Property);
    auto Default = ExpansionNodeIdDefault(Property, Prior);
    FString Kind;
    if (!Type.IsValid() || Type->Values.Num() != 1 || !Type->TryGetStringField(TEXT("type"), Kind)
        || Kind != TEXT("string") || !Default.IsValid() || Default->Type != EJson::String || !Default->AsString().IsEmpty())
    { Error = TEXT("Material reference type/default proof failed before metadata write."); return false; }
    Row->SetObjectField(TEXT("type_schema"), Type);
    Row->SetField(TEXT("public_default"), Default);

    if (Prior.IsEmpty())
    {
        Report->SetBoolField(TEXT("metadata_mutation_attempted"), true);
        Report->SetNumberField(TEXT("metadata_write_attempt_count"), 1);
        Row->SetBoolField(TEXT("metadata_write_attempted"), true);
        Function->SetMetaData(Key, PreparedEmpty);
        Row->SetBoolField(TEXT("metadata_write_returned"), true);
        const bool Changed = Prior != Function->GetMetaData(Key);
        Report->SetBoolField(TEXT("metadata_changed"), Changed);
        Report->SetNumberField(TEXT("metadata_changed_count"), Changed ? 1 : 0);
    }
    Row->SetStringField(TEXT("metadata_after"), Function->GetMetaData(Key));
    const auto ExpectedConverter = ActiveConverter;
    ActiveConverter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
    if (!ActiveConverter.IsValid() || ActiveConverter != ExpectedConverter || ActiveConverter->GetName() != ConverterName || Function->GetMetaData(Key) != PreparedEmpty)
    { Error = TEXT("Material reference metadata/converter readback failed; actual RAM prefix retained."); return false; }
    Default = ActiveConverter->PropertyToDefault(Property, Function->GetMetaData(Key));
    if (!Default.IsValid() || Default->Type != EJson::String || !Default->AsString().IsEmpty())
    { Error = TEXT("Prepared Material expansion node selector default differs."); return false; }

    Defaults->SetField(TEXT("expand_material_function_calls.node_id"), Default);
    Report->SetBoolField(TEXT("default_verified"), true);
    Report->SetBoolField(TEXT("exact_converter_active"), true);
    Report->SetBoolField(TEXT("type_schema_verified"), true);
    Report->SetNumberField(TEXT("verified_property_count"), 1);
    Report->SetNumberField(TEXT("verified_parameter_count"), 1);
    Report->SetNumberField(TEXT("property_count"), 1);
    Report->SetBoolField(TEXT("nullable_schema_verified"), true);
    Report->SetStringField(TEXT("converter_name"), ConverterName);
    Row->SetBoolField(TEXT("type_schema_verified"), true);
    Row->SetBoolField(TEXT("default_verified"), true);
    return true;
}

bool Cubeless::Registration::VerifyMaterialCollectionSyncToolset(UClass* Class, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    auto Defaults = MakeShared<FJsonObject>(); Report->SetObjectField(TEXT("defaults"), Defaults);
    TArray<TSharedPtr<FJsonValue>> Rows; Report->SetArrayField(TEXT("property_observations"), Rows);
    Report->SetBoolField(TEXT("actual_call_packing_verified"), false);
    if (!IsInGameThread() || !Class || Class->GetPathName() != MaterialClass || !Class->IsChildOf(UToolsetDefinition::StaticClass()))
    { Error = TEXT("Exact canonical MaterialTools on the game thread required."); return false; }
    if (UToolsetRegistry::IsToolsetClassRegistered(Class))
    { Error = TEXT("Only the unregistered canonical MaterialTools class can be prepared; no metadata retry."); return false; }
    UFunction* Function = Class->FindFunctionByName(TEXT("set_material_parameter_collection_sync"));
    if (!Function || Function->GetOwnerClass() != Class) { Error = TEXT("Exact owned sync method required."); return false; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Verify original Material collection sync defaults"));
    if (!Subsystem.HasValue()) { Error = TEXT("Registry unavailable."); return false; }
    TArray<FProperty*> Properties; TArray<FName> Keys; TArray<ESyncInput> Kinds;
    TArray<TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter>> Converters;
    for (const TCHAR* Name : {TEXT("source_collection_path"), TEXT("target_collection_path"), TEXT("parameter_names")})
    {
        auto Row = MakeShared<FJsonObject>(); Rows.Add(MakeShared<FJsonValueObject>(Row)); Report->SetArrayField(TEXT("property_observations"), Rows);
        Row->SetStringField(TEXT("property"), Name);
        FProperty* Property = FindFProperty<FProperty>(Function, Name); const ESyncInput Kind = MatchSyncInput(Property);
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), Name)); const FString Prior = Function->GetMetaData(Key);
        Row->SetBoolField(TEXT("metadata_present_before"), Function->HasMetaData(Key)); Row->SetStringField(TEXT("metadata_before"), Prior);
        if (Kind == ESyncInput::Unhandled) { Error = TEXT("Exact original default declarations required; no metadata write."); return false; }
        auto Converter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
        const bool Active = Converter.IsValid() && Converter->GetName() == ConverterName; Row->SetBoolField(TEXT("exact_converter_active_before"), Active);
        const auto Type = Active ? Converter->PropertyToJsonSchema(Property) : TSharedPtr<FJsonObject>();
        const auto Default = SyncDefault(Property, Prior);
        if (!Active || !SyncTypeMatches(Type, Kind) || !SyncDefaultMatches(Default, Kind))
        { Error = TEXT("Actual sync converter/default/schema proof failed before writes."); return false; }
        Row->SetObjectField(TEXT("type_schema_before"), Type); Row->SetField(TEXT("public_default_before"), Default);
        Properties.Add(Property); Keys.Add(Key); Kinds.Add(Kind); Converters.Add(Converter);
    }
    int32 Attempts = 0, Changed = 0, Verified = 0;
    for (int32 I = 0; I < Properties.Num(); ++I)
    {
        const auto Row = Rows[I]->AsObject(); const FString Prior = Function->GetMetaData(Keys[I]);
        const TCHAR* Prepared = Kinds[I] == ESyncInput::Names ? TEXT("()") : PreparedEmpty;
        if (Prior.IsEmpty())
        {
            Report->SetBoolField(TEXT("metadata_mutation_attempted"), true);
            Report->SetNumberField(TEXT("metadata_write_attempt_count"), ++Attempts); Row->SetBoolField(TEXT("metadata_write_attempted"), true);
            Function->SetMetaData(Keys[I], Prepared); Row->SetBoolField(TEXT("metadata_write_returned"), true);
        }
        const FString After = Function->GetMetaData(Keys[I]); Row->SetStringField(TEXT("metadata_after"), After);
        if (After != Prior) { ++Changed; } Report->SetNumberField(TEXT("metadata_changed_count"), Changed);
        Report->SetBoolField(TEXT("metadata_changed"), Changed != 0);
        auto Active = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Properties[I]);
        const bool Exact = Active.IsValid() && Active == Converters[I] && Active->GetName() == ConverterName;
        const auto Type = Exact ? Active->PropertyToJsonSchema(Properties[I]) : TSharedPtr<FJsonObject>();
        const auto Default = Exact ? Active->PropertyToDefault(Properties[I], After) : TSharedPtr<FJsonValue>();
        const bool TypeOK = SyncTypeMatches(Type, Kinds[I]), DefaultOK = SyncDefaultMatches(Default, Kinds[I]);
        Row->SetBoolField(TEXT("exact_converter_active"), Exact); Row->SetBoolField(TEXT("type_schema_verified"), TypeOK); Row->SetBoolField(TEXT("default_verified"), DefaultOK);
        if (Type.IsValid()) { Row->SetObjectField(TEXT("type_schema"), Type); }
        if (Default.IsValid()) { Row->SetField(TEXT("public_default"), Default); Defaults->SetField(FString::Printf(TEXT("set_material_parameter_collection_sync.%s"), *Properties[I]->GetName()), Default); }
        if (After != Prepared || !Exact || !TypeOK || !DefaultOK)
        { Error = TEXT("Ordered sync preparation postcondition failed; actual RAM metadata prefix retained, no retry."); return false; }
        Report->SetNumberField(TEXT("verified_property_count"), ++Verified);
    }
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("exact_converter_active"), true);
    Report->SetBoolField(TEXT("type_schema_verified"), true); Report->SetBoolField(TEXT("nullable_schema_verified"), true);
    Report->SetNumberField(TEXT("property_count"), 3); Report->SetNumberField(TEXT("verified_parameter_count"), Verified);
    Report->SetStringField(TEXT("converter_name"), ConverterName);
    return true;
}

bool Cubeless::Registration::FMaterialReferenceDefaultsOwner::Register()
{
    if (!IsInGameThread()) { return false; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Material reference default converter registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Converter.IsValid())
    { return RegistrySubsystem.IsValid() && RegistrySubsystem.Get() == Subsystem.GetValue().Get(); }
    auto Candidate = MakeMaterialReferenceDefaultsConverter();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue();
    Converter = Candidate;
    return true;
}

void Cubeless::Registration::FMaterialReferenceDefaultsOwner::Unregister()
{
    // Tear down only the owned converter on its original subsystem, without discovery or loading.
    if (UObjectInitialized())
    {
        if (auto Subsystem = RegistrySubsystem.Get())
        {
            if (Converter.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(Converter); }
        }
    }
    Converter.Reset();
    RegistrySubsystem.Reset();
}
