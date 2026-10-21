#include "Compatibility/UE58/CubelessBlueprintMakeStructDefaultsConverter.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr const TCHAR* ClassPath = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/graph_toolset_PY.CubelessBlueprintGraphTools");
    constexpr const TCHAR* PreparedEmpty = TEXT("\"\"");

    const FArrayProperty* MatchPosition(const FProperty* Property)
    {
        const auto Array = CastField<FArrayProperty>(Property);
        const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
        const auto Inner = Array ? CastField<FIntProperty>(Array->Inner) : nullptr;
        const auto Real = Array ? CastField<FNumericProperty>(Array->Inner) : nullptr;
        const bool Branch = Function && Function->GetName() == TEXT("add_blueprint_branch_node");
        if (!Array || Array->ArrayDim != 1 || !Array->HasAllPropertyFlags(CPF_Parm)
            || Array->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || (Branch ? (!Real || !Real->IsFloatingPoint() || Real->ArrayDim != 1) : (!Inner || Inner->ArrayDim != 1)) || !Owner || Owner->GetPathName() != ClassPath
            || (!Branch && Function->GetName() != TEXT("add_blueprint_make_struct_node")) || Array->GetName() != TEXT("node_position")
            || !Function->HasMetaData(TEXT("CPP_Default_node_position")) || !Function->GetMetaData(TEXT("node_position")).IsEmpty()) { return nullptr; }
        return Array;
    }
    bool PositionJson(const TSharedPtr<FJsonValue>& Value, bool Integral = true)
    {
        if (!Value.IsValid()) { return false; } if (Value->Type == EJson::Null) { return true; }
        if (Value->Type != EJson::Array || (Value->AsArray().Num() != 0 && Value->AsArray().Num() != 2)) { return false; }
        for (const auto& Item : Value->AsArray())
        {
            if (!Item.IsValid() || Item->Type != EJson::Number) { return false; }
            const double N = Item->AsNumber();
            if (!FMath::IsFinite(N) || N < MIN_int32 || N > MAX_int32 || (Integral && N != FMath::TruncToDouble(N))) { return false; }
        } return true;
    }
    TSharedPtr<FJsonValue> PositionDefault(const FProperty* Property, const FString& Text)
    {
        const auto Array = MatchPosition(Property); if (!Array) { return nullptr; }
        FDefaultConstructedPropertyElement Scratch(Array);
        if (!Text.IsEmpty())
        { const TCHAR* End = Array->ImportText_Direct(*Text, Scratch.GetObjAddress(), nullptr, PPF_None); if (!End || *End != TCHAR('\0')) { return nullptr; } }
        FScriptArrayHelper Helper(Array, Scratch.GetObjAddress());
        if (Helper.Num() != 0) { return nullptr; } return MakeShared<FJsonValueNull>();
    }
    TSharedPtr<FJsonValue> PositionExport(const FArrayProperty* Array, const void* Value)
    {
        if (!Array || !Value) { return nullptr; } FScriptArrayHelper Helper(Array, Value);
        if (Helper.Num() == 0) { return MakeShared<FJsonValueNull>(); } if (Helper.Num() != 2) { return nullptr; }
        TArray<TSharedPtr<FJsonValue>> Items;
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        { const auto Numeric=CastFieldChecked<FNumericProperty>(Array->Inner);Items.Add(MakeShared<FJsonValueNumber>(Numeric->IsFloatingPoint()?Numeric->GetFloatingPointPropertyValue(Helper.GetRawPtr(Index)):double(Numeric->GetSignedIntPropertyValue(Helper.GetRawPtr(Index))))); }
        TSharedPtr<FJsonValue> Result = MakeShared<FJsonValueArray>(Items);return PositionJson(Result,!CastFieldChecked<FNumericProperty>(Array->Inner)->IsFloatingPoint())?Result:nullptr;
    }

    const FStrProperty* MatchParameter(const FProperty* Property)
    {
        const FStrProperty* String = CastField<FStrProperty>(Property);
        const UFunction* Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const UClass* Owner = Function ? Function->GetOwnerClass() : nullptr;
        if (!String || String->ArrayDim != 1 || !String->HasAllPropertyFlags(CPF_Parm)
            || String->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || !Owner || Owner->GetPathName() != ClassPath)
        { return nullptr; }
        const bool Branch = Function->GetName() == TEXT("add_blueprint_branch_node");
        const FString FunctionName = Function->GetName();
        const FString FieldName = String->GetName();
        const bool CleanupSelector = (FieldName == TEXT("graph_name") || FieldName == TEXT("graph_id") || FieldName == TEXT("graph_type"));
        const bool ListField = FunctionName == TEXT("list_blueprint_nodes") && (CleanupSelector || FieldName == TEXT("node_type") || FieldName == TEXT("title_contains"));
        const bool DeleteField = FunctionName == TEXT("delete_blueprint_node") && (CleanupSelector || FieldName == TEXT("expected_node_name") || FieldName == TEXT("expected_node_class") || FieldName == TEXT("expected_title_contains"));
        const bool ExactField = ListField || DeleteField || (Branch && (String->GetName()==TEXT("graph_name")||String->GetName()==TEXT("graph_id")||String->GetName()==TEXT("graph_type"))) || (Function->GetName() == TEXT("add_blueprint_make_struct_node") && String->GetName() == TEXT("graph_name"))
            || (Function->GetName() == TEXT("add_blueprint_make_struct_node") && String->GetName() == TEXT("graph_id"))
            || (Function->GetName() == TEXT("add_blueprint_make_struct_node") && String->GetName() == TEXT("graph_type"));
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), *String->GetName()));
        if (!ExactField || !Function->HasMetaData(Key)
            || !Function->GetMetaData(*String->GetName()).IsEmpty())
        { return nullptr; }
        const FString& Default = Function->GetMetaData(Key);
        return Default.IsEmpty() || Default == PreparedEmpty ? String : nullptr;
    }

    TSharedPtr<FJsonValue> ParseEmptyDefault(const FProperty* Property, const FString& Text)
    {
        const FStrProperty* String = MatchParameter(Property);
        if (!String || (!Text.IsEmpty() && Text != PreparedEmpty)) { return nullptr; }
        FString Scratch;
        if (!Text.IsEmpty())
        {
            // PPF_None imports quotes literally. Delimited import decodes the exact
            // prepared spelling into the original empty FString value.
            const TCHAR* End = String->ImportText_Direct(*Text, &Scratch, nullptr, PPF_Delimited);
            if (!End || *End != TCHAR('\0')) { return nullptr; }
        }
        if (!Scratch.IsEmpty()) { return nullptr; }
        return MakeShared<FJsonValueString>(Scratch);
    }

    class FBlueprintMakeStructDefaultsConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
    {
    public:
        virtual FString GetName() const override { return TEXT("CubelessMCP_UE58.BlueprintMakeStructEmptyStringDefaults.v1"); }

        virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override
        { return MatchParameter(Property) != nullptr || MatchPosition(Property) != nullptr; }

        virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
        {
            TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
            if (MatchPosition(Property))
            {
                Schema->SetStringField(TEXT("type"), TEXT("array")); Schema->SetBoolField(TEXT("nullable"), true);
                auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("type"), TEXT("number")); Schema->SetObjectField(TEXT("items"), Item);
                // Stock FJsonObject::Duplicate drops FJsonValueNull. Prepared nonempty
                // CPP_Default metadata reaches PropertyToDefault directly, preserving public null.
                return Schema;
            }
            if (!MatchParameter(Property)) { return Schema; }
            Schema->SetStringField(TEXT("type"), TEXT("string"));
            const UFunction* Function = CastChecked<UFunction>(Property->GetOwnerUObject());
            const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), *Property->GetName()));
            if (TSharedPtr<FJsonValue> Default = ParseEmptyDefault(Property, Function->GetMetaData(Key)))
            { Schema->SetField(TEXT("default"), Default); }
            return Schema;
        }

        virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> Property, const FString& DefaultString) override
        {
            if (const auto Default = PositionDefault(Property, DefaultString)) { return Default; }
            if (TSharedPtr<FJsonValue> Default = ParseEmptyDefault(Property, DefaultString)) { return Default; }
            UKismetSystemLibrary::RaiseScriptError(TEXT("The exact Blueprint MakeStruct empty-string default metadata is invalid; registration must fail."));
            return MakeShared<FJsonValueNull>();
        }

        virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
        {
            if (const auto Array = MatchPosition(Property)) { return PositionExport(Array, Value); }
            const FStrProperty* String = MatchParameter(Property);
            if (!String || !Value) { return nullptr; }
            return MakeShared<FJsonValueString>(String->GetPropertyValue(Value));
        }

        virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& JsonValue, TNotNull<FProperty*> Property,
            void* OutValue, UObject* Outer) override
        {
            if (const auto Array = MatchPosition(Property))
            {
                if (!OutValue || !PositionJson(JsonValue,!CastFieldChecked<FNumericProperty>(Array->Inner)->IsFloatingPoint()))
                { UKismetSystemLibrary::RaiseScriptError(TEXT("MakeStruct position requires null, empty array (legacy origin), or two finite integral int32 numbers; no string/fractional coercion.")); return false; }
                FDefaultConstructedPropertyElement Scratch(Array);
                if (JsonValue->Type == EJson::Array)
                {
                    const auto& Items = JsonValue->AsArray(); FScriptArrayHelper Helper(Array, Scratch.GetObjAddress()); Helper.AddValues(Items.Num());
                    for (int32 Index = 0; Index < Items.Num(); ++Index)
                    { const auto Numeric=CastFieldChecked<FNumericProperty>(Array->Inner);if(Numeric->IsFloatingPoint())Numeric->SetFloatingPointPropertyValue(Helper.GetRawPtr(Index),Items[Index]->AsNumber());else CastFieldChecked<FIntProperty>(Array->Inner)->SetPropertyValue(Helper.GetRawPtr(Index),static_cast<int32>(Items[Index]->AsNumber())); }
                }
                Array->CopyCompleteValue(OutValue, Scratch.GetObjAddress()); return true;
            }
            const FStrProperty* String = MatchParameter(Property);
            if (!String || !OutValue || !JsonValue.IsValid() || JsonValue->Type != EJson::String)
            {
                UKismetSystemLibrary::RaiseScriptError(TEXT("Blueprint MakeStruct graph_name/graph_id/graph_type requires a JSON string; null and coercions are unsupported."));
                return false;
            }
            String->SetPropertyValue(OutValue, JsonValue->AsString());
            return true;
        }
    };
}

TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Cubeless::UE58::MakeBlueprintMakeStructDefaultsConverter()
{ return MakeShared<FBlueprintMakeStructDefaultsConverter>(); }

bool Cubeless::UE58::PrepareBlueprintMakeStructDefaults(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    if (!IsInGameThread() || !ToolsetClass || ToolsetClass->GetPathName() != ClassPath)
    { Error = TEXT("Expected the exact merged mounted CubelessBlueprintGraphTools generated class on the game thread."); return false; }
    UFunction* PositionFunction = ToolsetClass->FindFunctionByName(TEXT("add_blueprint_make_struct_node"));
    FProperty* Position = PositionFunction ? FindFProperty<FProperty>(PositionFunction, TEXT("node_position")) : nullptr;
    const auto DefaultPosition = PositionFunction ? PositionDefault(Position, PositionFunction->GetMetaData(TEXT("CPP_Default_node_position"))) : nullptr;
    Report->SetBoolField(TEXT("node_position_default_verified"), DefaultPosition.IsValid() && DefaultPosition->Type == EJson::Null);
    if (DefaultPosition.IsValid()) { Report->SetField(TEXT("node_position_default"), DefaultPosition); }
    if (!MatchPosition(Position) || !DefaultPosition.IsValid())
    { Error = TEXT("Exact internal Array<int32> empty default with public null/origin semantics required before metadata writes."); return false; }
    const FName PositionKey(TEXT("CPP_Default_node_position"));
    const FString PositionText = PositionFunction->GetMetaData(PositionKey);
    if (!PositionText.IsEmpty() && PositionText != TEXT("()"))
    { Error = TEXT("Expected the exact empty or prepared () position default before metadata writes."); return false; }
    TArray<UFunction*> Functions;
    TArray<FProperty*> Properties;
    TArray<FName> Keys;
    TArray<TSharedPtr<FJsonValue>> Before;
    auto PositionRow = MakeShared<FJsonObject>();
    PositionRow->SetStringField(TEXT("function"), PositionFunction->GetName());
    PositionRow->SetStringField(TEXT("property"), Position->GetName());
    PositionRow->SetStringField(TEXT("property_kind"), TEXT("Array<int32> public-null default"));
    PositionRow->SetBoolField(TEXT("metadata_present"), PositionFunction->HasMetaData(PositionKey));
    PositionRow->SetStringField(TEXT("metadata"), PositionText);
    Before.Add(MakeShared<FJsonValueObject>(PositionRow)); Report->SetArrayField(TEXT("metadata_before"), Before);
    Functions.Add(PositionFunction); Properties.Add(Position); Keys.Add(PositionKey);
    UFunction* BranchFunction=ToolsetClass->FindFunctionByName(TEXT("add_blueprint_branch_node"));FProperty* BranchPosition=BranchFunction?FindFProperty<FProperty>(BranchFunction,TEXT("node_position")):nullptr;
    const FString BranchPositionText=BranchFunction?BranchFunction->GetMetaData(PositionKey):FString();const auto BranchDefault=BranchFunction?PositionDefault(BranchPosition,BranchPositionText):nullptr;
    if(!MatchPosition(BranchPosition)||!BranchDefault||BranchDefault->Type!=EJson::Null||(!BranchPositionText.IsEmpty()&&BranchPositionText!=TEXT("()"))){Error=TEXT("Exact Branch float-array origin/default-null projection required before metadata writes.");return false;}
    Functions.Add(BranchFunction);Properties.Add(BranchPosition);Keys.Add(PositionKey);
    for (const TPair<FName, FName>& Names : {
        TPair<FName, FName>(TEXT("add_blueprint_make_struct_node"), TEXT("graph_name")),
        TPair<FName, FName>(TEXT("add_blueprint_make_struct_node"), TEXT("graph_id")),
        TPair<FName, FName>(TEXT("add_blueprint_make_struct_node"), TEXT("graph_type")),
        TPair<FName, FName>(TEXT("add_blueprint_branch_node"), TEXT("graph_name")),
        TPair<FName, FName>(TEXT("add_blueprint_branch_node"), TEXT("graph_id")),
        TPair<FName, FName>(TEXT("add_blueprint_branch_node"), TEXT("graph_type")),
        TPair<FName, FName>(TEXT("list_blueprint_nodes"), TEXT("node_type")),
        TPair<FName, FName>(TEXT("list_blueprint_nodes"), TEXT("title_contains")),
        TPair<FName, FName>(TEXT("list_blueprint_nodes"), TEXT("graph_name")),
        TPair<FName, FName>(TEXT("list_blueprint_nodes"), TEXT("graph_id")),
        TPair<FName, FName>(TEXT("list_blueprint_nodes"), TEXT("graph_type")),
        TPair<FName, FName>(TEXT("delete_blueprint_node"), TEXT("graph_name")),
        TPair<FName, FName>(TEXT("delete_blueprint_node"), TEXT("graph_id")),
        TPair<FName, FName>(TEXT("delete_blueprint_node"), TEXT("graph_type")),
        TPair<FName, FName>(TEXT("delete_blueprint_node"), TEXT("expected_node_name")),
        TPair<FName, FName>(TEXT("delete_blueprint_node"), TEXT("expected_node_class")),
        TPair<FName, FName>(TEXT("delete_blueprint_node"), TEXT("expected_title_contains")) })
    {
        UFunction* Function = ToolsetClass->FindFunctionByName(Names.Key);
        FProperty* Property = Function ? FindFProperty<FProperty>(Function, Names.Value) : nullptr;
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), *Names.Value.ToString()));
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("function"), Names.Key.ToString());
        Row->SetStringField(TEXT("property"), Names.Value.ToString());
        Row->SetBoolField(TEXT("metadata_present"), Function && Function->HasMetaData(Key));
        Row->SetStringField(TEXT("metadata"), Function ? Function->GetMetaData(Key) : FString());
        Before.Add(MakeShared<FJsonValueObject>(Row));
        Report->SetArrayField(TEXT("metadata_before"), Before);
        if (!MatchParameter(Property) || !ParseEmptyDefault(Property, Function->GetMetaData(Key)))
        { Error = TEXT("Expected only exact Make/Branch/List/Delete scalar FString targets with declared empty or prepared-empty CPP_Default metadata and no overriding default."); return false; }
        Functions.Add(Function); Properties.Add(Property); Keys.Add(Key);
    }
    // All nineteen exact targets/defaults pass before the first public metadata write.
    TArray<TSharedPtr<FJsonValue>> Receipts;
    int32 AttemptCount = 0;
    int32 ChangedCount = 0;
    for (int32 Index = 0; Index < Functions.Num(); ++Index)
    {
        const bool IsPosition = MatchPosition(Properties[Index]) != nullptr;
        const FString Prepared = IsPosition ? TEXT("()") : PreparedEmpty;
        const FString BeforeValue = Functions[Index]->GetMetaData(Keys[Index]);
        const bool Attempted = BeforeValue.IsEmpty();
        if (Attempted)
        {
            ++AttemptCount;
            Report->SetBoolField(TEXT("mutation_attempted"), true);
            Report->SetNumberField(TEXT("metadata_write_attempt_count"), AttemptCount);
            Functions[Index]->SetMetaData(Keys[Index], *Prepared);
        }
        const FString AfterValue = Functions[Index]->GetMetaData(Keys[Index]);
        const bool Changed = BeforeValue != AfterValue;
        ChangedCount += Changed ? 1 : 0;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("function"), Functions[Index]->GetName());
        Row->SetStringField(TEXT("property"), Properties[Index]->GetName());
        Row->SetStringField(TEXT("metadata_before"), BeforeValue);
        Row->SetStringField(TEXT("metadata_after"), AfterValue);
        Row->SetBoolField(TEXT("attempted"), Attempted); Row->SetBoolField(TEXT("changed"), Changed);
        const TSharedPtr<FJsonValue> Default = IsPosition ? PositionDefault(Properties[Index], AfterValue) : ParseEmptyDefault(Properties[Index], AfterValue);
        const bool Decoded = Default.IsValid() && (IsPosition ? Default->Type == EJson::Null : Default->Type == EJson::String && Default->AsString().IsEmpty());
        Row->SetBoolField(TEXT("decoded_public_null"), IsPosition && Decoded);
        Row->SetBoolField(TEXT("decoded_empty_string"), !IsPosition && Decoded);
        if (Default.IsValid()) { Row->SetField(TEXT("public_default"), Default); }
        Receipts.Add(MakeShared<FJsonValueObject>(Row));
        Report->SetArrayField(TEXT("metadata_changes"), Receipts);
        Report->SetNumberField(TEXT("metadata_changed_count"), ChangedCount);
        Report->SetBoolField(TEXT("changed"), ChangedCount != 0);
        if (IsPosition)
        {
            Report->SetBoolField(TEXT("node_position_default_verified"), Decoded);
            if (Default.IsValid()) { Report->SetField(TEXT("node_position_default"), Default); }
        }
        if (AfterValue != Prepared || !Decoded)
        { Error = TEXT("Exact metadata preparation postcondition failed; the returned metadata prefix is retained and must not be retried automatically."); return false; }
    }
    Report->SetStringField(TEXT("public_default"), FString());
    auto Defaults = MakeShared<FJsonObject>();
    Defaults->SetStringField(TEXT("add_blueprint_make_struct_node.graph_name"), FString());
    Defaults->SetStringField(TEXT("add_blueprint_make_struct_node.graph_id"), FString());
    Defaults->SetStringField(TEXT("add_blueprint_make_struct_node.graph_type"), FString());
    Report->SetObjectField(TEXT("defaults"), Defaults);
    Report->SetBoolField(TEXT("default_verified"), true);
    return true;
}
