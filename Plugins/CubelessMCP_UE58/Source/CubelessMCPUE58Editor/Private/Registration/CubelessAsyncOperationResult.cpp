// CU: native public async result completion; no shader/domain/journal logic.
#include "Registration/CubelessAsyncOperationResult.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/CoreMisc.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr int32 FieldCount = 11;
constexpr int32 MaxValueBytes = 16 * 1024 * 1024;
const TCHAR* const FieldNames[FieldCount] = {
    TEXT("success"), TEXT("operation_id"), TEXT("stage"), TEXT("error_code"), TEXT("error"),
    TEXT("changes_json"), TEXT("compile_json"), TEXT("save_json"), TEXT("validation_json"),
    TEXT("retry_json"), TEXT("data_json")
};

int32 FieldIndex(const FString& Name)
{
    for (int32 I = 0; I < FieldCount; ++I)
    { if (Name.Equals(FieldNames[I], ESearchCase::CaseSensitive)) { return I; } }
    return INDEX_NONE;
}

// These protected static helpers are explicitly TOOLSETREGISTRY_API public
// extension points. This abstract access type is never instantiated or registered.
// UToolCallAsyncResult's public static GetValueJsonSchema/MatchesProperty have
// no API export; stock Registry calls them internally, this module does not.
class FAsyncValueJsonAccess : public UE::ToolsetRegistry::FToolsetJsonConverter
{
public:
    static TSharedPtr<FJsonObject> Schema(const FProperty* Property)
    { return ToolsetPropertyToJsonSchema(Property); }
    static TSharedPtr<FJsonValue> Data(FProperty* Property, const void* Value)
    { return ToolsetPropertyToJsonData(Property, Value); }
};

bool FieldsMatch(const TSharedPtr<FJsonObject>& Fields, bool Schema)
{
    if (!Fields.IsValid() || Fields->Values.Num() != FieldCount) { return false; }
    bool Seen[FieldCount] = {};
    for (const auto& Pair : Fields->Values)
    {
        const int32 Index = FieldIndex(FString(Pair.Key.ToView()));
        if (Index == INDEX_NONE || Seen[Index] || !Pair.Value.IsValid()) { return false; }
        Seen[Index] = true;
        if (Schema)
        {
            if (Pair.Value->Type != EJson::Object) { return false; }
            const auto Row = Pair.Value->AsObject(); FString Type;
            if (!Row.IsValid() || !Row->TryGetStringField(TEXT("type"), Type)
                || !Type.Equals(Index == 0 ? TEXT("boolean") : TEXT("string"), ESearchCase::CaseSensitive)) { return false; }
        }
        else if (Pair.Value->Type != (Index == 0 ? EJson::Boolean : EJson::String)) { return false; }
    }
    return true;
}

bool SchemaMatches(const TSharedPtr<FJsonObject>& Schema)
{
    FString Type;
    if (!Schema.IsValid() || !Schema->TryGetStringField(TEXT("type"), Type)
        || !Type.Equals(TEXT("object"), ESearchCase::CaseSensitive)
        || !Schema->HasTypedField<EJson::Object>(TEXT("properties"))
        || !FieldsMatch(Schema->GetObjectField(TEXT("properties")), true)
        || !Schema->HasTypedField<EJson::Array>(TEXT("required"))) { return false; }
    const auto& Required = Schema->GetArrayField(TEXT("required"));
    if (Required.Num() != FieldCount) { return false; }
    bool Seen[FieldCount] = {};
    for (const auto& Value : Required)
    {
        if (!Value.IsValid() || Value->Type != EJson::String) { return false; }
        const int32 Index = FieldIndex(Value->AsString());
        if (Index == INDEX_NONE || Seen[Index]) { return false; }
        Seen[Index] = true;
    }
    return true;
}
}

bool UCubelessAsyncOperationResult::Reject(const FString& Reason)
{
    CompletionDiagnostic = Reason;
    // The real instance is already game-thread/live/one-shot checked by caller.
    // A validation error resolves the official future instead of leaving it hung.
    SetError(Reason);
    return false;
}

bool UCubelessAsyncOperationResult::CompleteValue()
{
    // Never read reflection/value or write diagnostics on a worker thread.
    if (!IsInGameThread()) { return false; }
    if (!IsValid(this) || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
        || IsEngineExitRequested() || GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists)) { return false; }
    if (bIsComplete) { CompletionDiagnostic = TEXT("async_result_already_complete"); return false; }
    if (!Error.IsEmpty()) { return Reject(TEXT("async_result_has_existing_error")); }

    // SDK FindPropertyByName("Value") uses FName identity, so Python 'value'
    // or 'Value' is accepted. No dynamic Python class name/package dependency.
    auto* Value = CastField<FStructProperty>(GetClass()->FindPropertyByName(TEXT("Value")));
    if (!Value || Value->ArrayDim != 1 || !Value->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)
        || !IsValid(Value->Struct) || Value->Struct->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_BeginDestroyed | RF_FinishDestroyed))
    { return Reject(TEXT("async_result_value_requires_visible_real_standard11_struct")); }

    bool Seen[FieldCount] = {}; int32 Count = 0;
    for (TFieldIterator<FProperty> It(Value->Struct); It; ++It)
    {
        FProperty* Field = *It; const int32 Index = FieldIndex(Field->GetName());
        if (Index == INDEX_NONE || Seen[Index] || Field->ArrayDim != 1
            || !Field->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)
            || (Index == 0 ? CastField<FBoolProperty>(Field) == nullptr : CastField<FStrProperty>(Field) == nullptr))
        { return Reject(TEXT("async_result_value_field_names_or_types_do_not_match_standard11")); }
        Seen[Index] = true; ++Count;
    }
    if (Count != FieldCount) { return Reject(TEXT("async_result_value_requires_exactly11_fields")); }

    // Validate the actual exported SDK conversion path before completion.
    // No manual schema, converter registration, metadata write or static helper
    // link call bypasses the stock async schema/data path.
    const auto Schema = FAsyncValueJsonAccess::Schema(Value);
    if (!SchemaMatches(Schema)) { return Reject(TEXT("async_result_value_sdk_schema_incompatible")); }
    const void* Address = Value->ContainerPtrToValuePtr<void>(this);
    const auto JsonValue = FAsyncValueJsonAccess::Data(Value, Address);
    if (!JsonValue.IsValid() || JsonValue->Type != EJson::Object || !FieldsMatch(JsonValue->AsObject(), false))
    { return Reject(TEXT("async_result_value_sdk_json_incompatible")); }

    int32 Characters = 0;
    for (int32 I = 1; I < FieldCount; ++I)
    {
        const int32 Length = JsonValue->AsObject()->GetStringField(FieldNames[I]).Len();
        if (Length > MaxValueBytes - Characters) { return Reject(TEXT("async_result_value_exceeds16MiB_bound")); }
        Characters += Length;
    }
    FString Encoded;
    if (!FJsonSerializer::Serialize(JsonValue->AsObject().ToSharedRef(), TJsonWriterFactory<>::Create(&Encoded))
        || Encoded.Len() > MaxValueBytes) { return Reject(TEXT("async_result_value_serialization_incomplete_or_oversized")); }
    const FTCHARToUTF8 Utf8(*Encoded, Encoded.Len());
    if (Utf8.Length() > MaxValueBytes) { return Reject(TEXT("async_result_value_exceeds16MiB_UTF8_bound")); }
    const FUTF8ToTCHAR RoundTrip(reinterpret_cast<const UTF8CHAR*>(Utf8.Get()), Utf8.Length());
    if (!FString(RoundTrip.Length(), RoundTrip.Get()).Equals(Encoded, ESearchCase::CaseSensitive))
    { return Reject(TEXT("async_result_value_invalid_Unicode")); }

    CompletionDiagnostic = TEXT("standard11_value_validated_transport_completion_only");
    // Python already populated the reflected Value. The protected public SDK
    // no-value overload marks complete and broadcasts; never assign bIsComplete.
    return MaybeBroadcastSuccessfulCompletion();
}
