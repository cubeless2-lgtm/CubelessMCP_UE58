#include "CubelessSolverTextSemanticsLibrary.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Char.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 MaxInputUnits = 1 << 20;
constexpr int32 MaxScalarRows = 65538; // Request value slots plus null/empty sentinels.
constexpr int32 MaxOutputUnits = 16 << 20;

FString Encode(const TSharedRef<FJsonObject>& Object)
{
    FString Encoded;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
    if (!FJsonSerializer::Serialize(Object, Writer))
    {
        return TEXT("{\"success\":false,\"operation\":\"evaluate_solver_text_semantics\",\"error\":\"text receipt serialization failed\",\"asset_query\":false,\"asset_mutation\":false}");
    }
    return Encoded;
}

bool StableIdentifier(const FString& Value)
{
    if (Value.IsEmpty() || !(FChar::IsAlpha(Value[0]) || Value[0] == TEXT('_')))
    {
        return false;
    }
    for (int32 Index = 1; Index < Value.Len(); ++Index)
    {
        if (!(FChar::IsAlnum(Value[Index]) || Value[Index] == TEXT('_')))
        {
            return false;
        }
    }
    return true;
}

FString NormalizePackageText(const FString& Value)
{
    FString PackagePath = FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();
    PackagePath.TrimQuotesInline();
    PackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));
    int32 DotIndex = INDEX_NONE;
    if (PackagePath.FindChar(TEXT('.'), DotIndex))
    {
        PackagePath.LeftInline(DotIndex);
    }
    while (PackagePath.Contains(TEXT("//")))
    {
        PackagePath.ReplaceInline(TEXT("//"), TEXT("/"));
    }
    while (PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath.LeftChopInline(1);
    }
    return PackagePath;
}
}

FString UCubelessSolverTextSemanticsLibrary::EvaluateSolverTextSemantics(const FString& ScalarsJson)
{
    const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("success"), false);
    Report->SetStringField(TEXT("operation"), TEXT("evaluate_solver_text_semantics"));
    Report->SetStringField(TEXT("backend"), TEXT("UE58_public_FJsonValue_FString_FChar_text_only"));
    Report->SetBoolField(TEXT("asset_query"), false);
    Report->SetBoolField(TEXT("asset_mutation"), false);
    Report->SetBoolField(TEXT("compile_attempted"), false);
    Report->SetBoolField(TEXT("save_attempted"), false);
    Report->SetBoolField(TEXT("solver_policy_executed"), false);
    Report->SetBoolField(TEXT("journal_created"), false);
    auto Failure = [&Report](const FString& Error) -> FString
    {
        Report->SetBoolField(TEXT("success"), false);
        Report->SetStringField(TEXT("error"), Error);
        return Encode(Report);
    };
    if (!IsInGameThread())
    {
        return Failure(TEXT("text semantics require the official game-thread dispatch"));
    }
    if (ScalarsJson.Len() > MaxInputUnits)
    {
        return Failure(TEXT("scalar batch exceeds 1Mi UTF16 units or contains NUL"));
    }
    for (const TCHAR Character : ScalarsJson)
    {
        if (Character == 0)
        {
            return Failure(TEXT("scalar batch contains NUL"));
        }
    }
    // Reject nested containers before building any SDK DOM. A malformed string
    // remains the SDK parser's error, but cannot hide a valid nested container.
    bool bQuoted = false;
    bool bEscaped = false;
    bool bOuterArrayOpened = false;
    int32 ScalarSeparators = 0;
    for (const TCHAR Character : ScalarsJson)
    {
        if (bQuoted)
        {
            if (bEscaped)
            {
                bEscaped = false;
            }
            else if (Character == TEXT('\\'))
            {
                bEscaped = true;
            }
            else if (Character == TEXT('"'))
            {
                bQuoted = false;
            }
        }
        else if (Character == TEXT('"'))
        {
            bQuoted = true;
        }
        else if (Character == TEXT('{') || (Character == TEXT('[') && bOuterArrayOpened))
        {
            return Failure(TEXT("scalar batch must be flat before JSON deserialization"));
        }
        else if (Character == TEXT('['))
        {
            bOuterArrayOpened = true;
        }
        else if (Character == TEXT(',') && ++ScalarSeparators >= MaxScalarRows)
        {
            return Failure(TEXT("scalar batch exceeds the pre-parse row boundary"));
        }
    }
    TArray<TSharedPtr<FJsonValue>> Scalars;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ScalarsJson);
    if (!FJsonSerializer::Deserialize(Reader, Scalars, FJsonSerializer::EFlags::None) || Scalars.Num() > MaxScalarRows)
    {
        return Failure(TEXT("scalar batch must be a JSON array of at most 65538 scalars"));
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    Rows.Reserve(Scalars.Num());
    int64 TextOutputUnits = 0;
    for (const TSharedPtr<FJsonValue>& Scalar : Scalars)
    {
        if (!Scalar.IsValid() || Scalar->Type == EJson::Array || Scalar->Type == EJson::Object)
        {
            return Failure(TEXT("scalar batch must not contain nested arrays or objects"));
        }
        if (Scalar->Type == EJson::Number && !FMath::IsFinite(Scalar->AsNumber()))
        {
            return Failure(TEXT("scalar batch must contain finite double-compatible JSON numbers"));
        }
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        FString Text;
        const bool bHasString = Scalar->TryGetString(Text);
        for (const TCHAR Character : Text)
        {
            if (Character == 0)
            {
                return Failure(TEXT("scalar text contains NUL"));
            }
        }
        Text.TrimStartAndEndInline();
        const FString Lower = Text.ToLower();
        const FString Package = NormalizePackageText(Text);
        const FString PackageLower = Package.ToLower();
        TextOutputUnits += Text.Len() + Lower.Len() + Package.Len() + PackageLower.Len() + 192;
        if (TextOutputUnits > MaxOutputUnits)
        {
            return Failure(TEXT("scalar text receipt exceeds the bounded 16Mi UTF16-unit projection"));
        }
        Row->SetBoolField(TEXT("has_string"), bHasString);
        Row->SetStringField(TEXT("trimmed"), Text);
        Row->SetStringField(TEXT("lower"), Lower);
        Row->SetStringField(TEXT("package"), Package);
        Row->SetStringField(TEXT("package_lower"), PackageLower);
        Row->SetBoolField(TEXT("stable_identifier"), StableIdentifier(Text));
        bool bBoolean = false;
        if (Scalar->TryGetBool(bBoolean))
        {
            Row->SetBoolField(TEXT("boolean"), bBoolean);
        }
        else
        {
            Row->SetField(TEXT("boolean"), MakeShared<FJsonValueNull>());
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Report->SetArrayField(TEXT("rows"), Rows);
    Report->SetNumberField(TEXT("scalar_count"), Scalars.Num());
    Report->SetStringField(TEXT("error"), TEXT(""));
    Report->SetBoolField(TEXT("success"), true);
    const FString Encoded = Encode(Report);
    if (Encoded.Len() > MaxOutputUnits)
    {
        Report->RemoveField(TEXT("rows"));
        return Failure(TEXT("encoded scalar text receipt exceeds 16Mi UTF16 units"));
    }
    return Encoded;
}
