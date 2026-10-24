#include "Compatibility/UE58/CubelessJsonValueLibrary.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

FString UCubelessJsonValueLibrary::TryJsonStringBool(const FString& Value)
{
    const FJsonValueString JsonValue(Value);
    bool Converted = false;
    const bool ReadSuccess = JsonValue.TryGetBool(Converted);
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("read_success"), ReadSuccess);
    Result->SetBoolField(TEXT("value"), Converted);
    FString Encoded;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded));
    return Encoded;
}
