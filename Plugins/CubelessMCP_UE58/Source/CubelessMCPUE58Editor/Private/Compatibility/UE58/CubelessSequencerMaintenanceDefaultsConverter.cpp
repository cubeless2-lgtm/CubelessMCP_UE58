#include "Compatibility/UE58/CubelessSequencerMaintenanceDefaultsConverter.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr const TCHAR* ClassPath = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/sequencer_toolset_PY.CubelessSequencerTools");
    constexpr const TCHAR* PreparedEmpty = TEXT("\"\"");

    const FStrProperty* MatchParameter(const FProperty* Property)
    {
        const FStrProperty* String = CastField<FStrProperty>(Property);
        const UFunction* Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const UClass* Owner = Function ? Function->GetOwnerClass() : nullptr;
        if (!String || String->ArrayDim != 1 || !String->HasAllPropertyFlags(CPF_Parm)
            || String->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || !Owner || Owner->GetPathName() != ClassPath)
        { return nullptr; }
        const bool ExactField = (Function->GetName() == TEXT("remove_level_sequence_binding") && String->GetName() == TEXT("binding_id"))
            || (Function->GetName() == TEXT("remove_level_sequence_binding") && String->GetName() == TEXT("actor_label"));
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

    class FSequencerMaintenanceDefaultsConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
    {
    public:
        virtual FString GetName() const override { return TEXT("CubelessMCP_UE58.SequencerMaintenanceEmptyStringDefaults.v1"); }

        virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override
        { return MatchParameter(Property) != nullptr; }

        virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
        {
            TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
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
            if (TSharedPtr<FJsonValue> Default = ParseEmptyDefault(Property, DefaultString)) { return Default; }
            UKismetSystemLibrary::RaiseScriptError(TEXT("The exact Sequencer Maintenance empty-string default metadata is invalid; registration must fail."));
            return MakeShared<FJsonValueNull>();
        }

        virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
        {
            const FStrProperty* String = MatchParameter(Property);
            if (!String || !Value) { return nullptr; }
            return MakeShared<FJsonValueString>(String->GetPropertyValue(Value));
        }

        virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& JsonValue, TNotNull<FProperty*> Property,
            void* OutValue, UObject* Outer) override
        {
            const FStrProperty* String = MatchParameter(Property);
            if (!String || !OutValue || !JsonValue.IsValid() || JsonValue->Type != EJson::String)
            {
                UKismetSystemLibrary::RaiseScriptError(TEXT("Sequencer Maintenance binding_id/actor_label requires a JSON string; null and coercions are unsupported."));
                return false;
            }
            String->SetPropertyValue(OutValue, JsonValue->AsString());
            return true;
        }
    };
}

TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Cubeless::UE58::MakeSequencerMaintenanceDefaultsConverter()
{ return MakeShared<FSequencerMaintenanceDefaultsConverter>(); }

bool Cubeless::UE58::PrepareSequencerMaintenanceDefaults(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    if (!IsInGameThread() || !ToolsetClass || ToolsetClass->GetPathName() != ClassPath)
    { Error = TEXT("Expected the exact merged mounted CubelessSequencerTools generated class on the game thread."); return false; }
    TArray<UFunction*> Functions;
    TArray<FProperty*> Properties;
    TArray<FName> Keys;
    TArray<TSharedPtr<FJsonValue>> Before;
    for (const TPair<FName, FName>& Names : {
        TPair<FName, FName>(TEXT("remove_level_sequence_binding"), TEXT("binding_id")),
        TPair<FName, FName>(TEXT("remove_level_sequence_binding"), TEXT("actor_label")) })
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
        { Error = TEXT("Expected both exact scalar FString inputs with declared empty or prepared-empty CPP_Default metadata and no overriding default."); return false; }
        Functions.Add(Function); Properties.Add(Property); Keys.Add(Key);
    }
    // Every target/default has passed before the first public metadata write.
    TArray<TSharedPtr<FJsonValue>> Receipts;
    int32 AttemptCount = 0;
    int32 ChangedCount = 0;
    for (int32 Index = 0; Index < Functions.Num(); ++Index)
    {
        const FString BeforeValue = Functions[Index]->GetMetaData(Keys[Index]);
        const bool Attempted = BeforeValue.IsEmpty();
        if (Attempted)
        {
            ++AttemptCount;
            Report->SetBoolField(TEXT("mutation_attempted"), true);
            Report->SetNumberField(TEXT("metadata_write_attempt_count"), AttemptCount);
            Functions[Index]->SetMetaData(Keys[Index], PreparedEmpty);
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
        const TSharedPtr<FJsonValue> Default = ParseEmptyDefault(Properties[Index], AfterValue);
        Row->SetBoolField(TEXT("decoded_empty_string"), Default.IsValid() && Default->Type == EJson::String && Default->AsString().IsEmpty());
        if (Default.IsValid()) { Row->SetField(TEXT("public_default"), Default); }
        Receipts.Add(MakeShared<FJsonValueObject>(Row));
        Report->SetArrayField(TEXT("metadata_changes"), Receipts);
        Report->SetNumberField(TEXT("metadata_changed_count"), ChangedCount);
        Report->SetBoolField(TEXT("changed"), ChangedCount != 0);
        if (AfterValue != PreparedEmpty || !Default.IsValid() || !Default->AsString().IsEmpty())
        { Error = TEXT("Exact metadata preparation postcondition failed; the returned metadata prefix is retained and must not be retried automatically."); return false; }
    }
    Report->SetStringField(TEXT("public_default"), FString());
    auto Defaults = MakeShared<FJsonObject>();
    Defaults->SetStringField(TEXT("remove_level_sequence_binding.binding_id"), FString());
    Defaults->SetStringField(TEXT("remove_level_sequence_binding.actor_label"), FString());
    Report->SetObjectField(TEXT("defaults"), Defaults);
    Report->SetBoolField(TEXT("default_verified"), true);
    return true;
}
