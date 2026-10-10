// UE56 specialized Custom-node parser closure; only JSON/identifier parsing.
// Original: MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPMaterialCommands.cpp
// Source SHA256: 8c41e028d0f6b27e0d5c2d85cad37216bab8d2234e61189c812e6a463b0d5a91
// Original comments retained; no license notice was present in this source file.
// UE58 adaptation: JsonObject Values key is FStringType, converted through ToView.
// Additional bound: reject input names at NAME_SIZE before constructing FName.

bool IsHlslIdentifierStart(TCHAR Character)
{
    return Character == TEXT('_') ||
        (Character >= TEXT('A') && Character <= TEXT('Z')) ||
        (Character >= TEXT('a') && Character <= TEXT('z'));
}

bool IsHlslIdentifierChar(TCHAR Character)
{
    return IsHlslIdentifierStart(Character) ||
        (Character >= TEXT('0') && Character <= TEXT('9'));
}

bool IsReservedHlslIdentifier(const FString& Name)
{
    static const TSet<FString> ReservedNames = {
        TEXT("break"), TEXT("case"), TEXT("cbuffer"), TEXT("class"), TEXT("const"),
        TEXT("continue"), TEXT("default"), TEXT("discard"), TEXT("do"), TEXT("else"),
        TEXT("false"), TEXT("for"), TEXT("groupshared"), TEXT("if"), TEXT("in"),
        TEXT("inline"), TEXT("inout"), TEXT("namespace"), TEXT("out"), TEXT("return"),
        TEXT("sampler"), TEXT("samplerstate"), TEXT("static"), TEXT("struct"), TEXT("switch"),
        TEXT("texture"), TEXT("texture2d"), TEXT("true"), TEXT("typedef"), TEXT("uniform"),
        TEXT("void"), TEXT("volatile"), TEXT("while"),
        TEXT("bool"), TEXT("bool1"), TEXT("bool2"), TEXT("bool3"), TEXT("bool4"),
        TEXT("double"), TEXT("float"), TEXT("float1"), TEXT("float2"), TEXT("float3"), TEXT("float4"),
        TEXT("half"), TEXT("half1"), TEXT("half2"), TEXT("half3"), TEXT("half4"),
        TEXT("int"), TEXT("int1"), TEXT("int2"), TEXT("int3"), TEXT("int4"),
        TEXT("uint"), TEXT("uint1"), TEXT("uint2"), TEXT("uint3"), TEXT("uint4")
    };

    return ReservedNames.Contains(Name.ToLower());
}

bool IsValidHlslIdentifier(const FString& Name)
{
    if (Name.IsEmpty() || !IsHlslIdentifierStart(Name[0]))
    {
        return false;
    }

    for (int32 Index = 1; Index < Name.Len(); ++Index)
    {
        if (!IsHlslIdentifierChar(Name[Index]))
        {
            return false;
        }
    }

    return !IsReservedHlslIdentifier(Name);
}

bool ParseCustomOutputType(const FString& OutputTypeText, ECustomMaterialOutputType& OutOutputType, FString& OutError)
{
    FString Normalized = OutputTypeText.TrimStartAndEnd();
    if (Normalized.IsEmpty())
    {
        OutError = TEXT("Custom material output_type cannot be empty");
        return false;
    }

    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized = Normalized.ToUpper();

    if (Normalized.StartsWith(TEXT("CMOT")))
    {
        Normalized.RightChopInline(4);
    }

    if (Normalized == TEXT("1") || Normalized == TEXT("FLOAT") || Normalized == TEXT("FLOAT1"))
    {
        OutOutputType = CMOT_Float1;
        return true;
    }
    if (Normalized == TEXT("2") || Normalized == TEXT("FLOAT2"))
    {
        OutOutputType = CMOT_Float2;
        return true;
    }
    if (Normalized == TEXT("3") || Normalized == TEXT("FLOAT3"))
    {
        OutOutputType = CMOT_Float3;
        return true;
    }
    if (Normalized == TEXT("4") || Normalized == TEXT("FLOAT4"))
    {
        OutOutputType = CMOT_Float4;
        return true;
    }
    if (Normalized == TEXT("MATERIALATTRIBUTES") || Normalized == TEXT("ATTRIBUTES"))
    {
        OutOutputType = CMOT_MaterialAttributes;
        return true;
    }

    OutError = FString::Printf(
        TEXT("Invalid custom material output_type '%s'. Expected CMOT_Float1, CMOT_Float2, CMOT_Float3, CMOT_Float4, or CMOT_MaterialAttributes."),
        *OutputTypeText);
    return false;
}

bool TryGetStringFromJsonObjectLoose(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, FString& OutValue)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const FString& FieldName : FieldNames)
    {
        if (Object->TryGetStringField(FieldName, OutValue))
        {
            return true;
        }

        for (const auto& Pair : Object->Values)
        {
            if (FString(Pair.Key.ToView()).Equals(FieldName, ESearchCase::IgnoreCase) &&
                Pair.Value.IsValid() &&
                Pair.Value->Type == EJson::String)
            {
                OutValue = Pair.Value->AsString();
                return true;
            }
        }
    }

    return false;
}

bool ParseCustomInputNames(const TSharedPtr<FJsonObject>& Params, TArray<FName>& OutInputNames, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* InputArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("inputs"), InputArray))
    {
        return true;
    }

    TSet<FString> SeenNames;
    for (int32 Index = 0; Index < InputArray->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& InputValue = (*InputArray)[Index];
        FString InputName;
        if (InputValue.IsValid() && InputValue->Type == EJson::String)
        {
            InputName = InputValue->AsString();
        }
        else if (InputValue.IsValid() && InputValue->Type == EJson::Object)
        {
            TryGetStringFromJsonObjectLoose(InputValue->AsObject(), { TEXT("name"), TEXT("input_name") }, InputName);
        }
        else
        {
            OutError = FString::Printf(TEXT("Custom input at index %d must be a string or object with a name field"), Index);
            return false;
        }

        InputName.TrimStartAndEndInline();
        if (InputName.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Custom input at index %d has an empty name"), Index);
            return false;
        }
        if (InputName.Len() >= NAME_SIZE)
        {
            OutError = TEXT("Custom input name exceeds the engine FName length limit");
            return false;
        }
        if (!IsValidHlslIdentifier(InputName))
        {
            OutError = FString::Printf(TEXT("Custom input name '%s' is not a valid HLSL identifier"), *InputName);
            return false;
        }

        const FString LowerName = InputName.ToLower();
        if (SeenNames.Contains(LowerName))
        {
            OutError = FString::Printf(TEXT("Duplicate custom input name: %s"), *InputName);
            return false;
        }

        SeenNames.Add(LowerName);
        OutInputNames.Add(FName(*InputName));
    }

    return true;
}

bool ParseCustomDefines(const TSharedPtr<FJsonObject>& Params, TArray<FCustomDefine>& OutDefines, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* DefineArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("additional_defines"), DefineArray))
    {
        return true;
    }

    TSet<FString> SeenNames;
    for (int32 Index = 0; Index < DefineArray->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& DefineValue = (*DefineArray)[Index];
        if (!DefineValue.IsValid() || DefineValue->Type != EJson::Object)
        {
            OutError = FString::Printf(TEXT("additional_defines entry at index %d must be an object"), Index);
            return false;
        }

        const TSharedPtr<FJsonObject> DefineObject = DefineValue->AsObject();
        FString DefineName;
        if (!TryGetStringFromJsonObjectLoose(DefineObject, { TEXT("name"), TEXT("define_name") }, DefineName))
        {
            OutError = FString::Printf(TEXT("additional_defines entry at index %d is missing a string name"), Index);
            return false;
        }
        DefineName.TrimStartAndEndInline();
        if (!IsValidHlslIdentifier(DefineName))
        {
            OutError = FString::Printf(TEXT("Custom define name '%s' is not a valid HLSL identifier"), *DefineName);
            return false;
        }

        const FString LowerName = DefineName.ToLower();
        if (SeenNames.Contains(LowerName))
        {
            OutError = FString::Printf(TEXT("Duplicate custom define name: %s"), *DefineName);
            return false;
        }

        FString DefineText = TEXT("1");
        TSharedPtr<FJsonValue> RawDefineText = DefineObject->TryGetField(TEXT("value"));
        if (!RawDefineText.IsValid())
        {
            RawDefineText = DefineObject->TryGetField(TEXT("define_value"));
        }
        if (RawDefineText.IsValid())
        {
            if (RawDefineText->Type != EJson::String && RawDefineText->Type != EJson::Number && RawDefineText->Type != EJson::Boolean)
            {
                OutError = FString::Printf(TEXT("Custom define '%s' value must be a string, number, or boolean"), *DefineName);
                return false;
            }
            DefineText = JsonValueToImportText(RawDefineText);
        }

        FCustomDefine Define;
        Define.DefineName = DefineName;
        Define.DefineValue = DefineText;
        OutDefines.Add(Define);
        SeenNames.Add(LowerName);
    }

    return true;
}

bool ParseIncludeFilePaths(const TSharedPtr<FJsonObject>& Params, TArray<FString>& OutIncludeFilePaths, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* IncludeArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("include_file_paths"), IncludeArray))
    {
        return true;
    }

    TSet<FString> SeenPaths;
    for (int32 Index = 0; Index < IncludeArray->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& IncludeValue = (*IncludeArray)[Index];
        if (!IncludeValue.IsValid() || IncludeValue->Type != EJson::String)
        {
            OutError = FString::Printf(TEXT("include_file_paths entry at index %d must be a string"), Index);
            return false;
        }

        FString IncludePath = IncludeValue->AsString().TrimStartAndEnd();
        if (IncludePath.IsEmpty())
        {
            OutError = FString::Printf(TEXT("include_file_paths entry at index %d is empty"), Index);
            return false;
        }
        if (IncludePath.Contains(TEXT("\n")) || IncludePath.Contains(TEXT("\r")))
        {
            OutError = FString::Printf(TEXT("include_file_paths entry at index %d contains a newline"), Index);
            return false;
        }

        const FString LowerPath = IncludePath.ToLower();
        if (SeenPaths.Contains(LowerPath))
        {
            continue;
        }

        SeenPaths.Add(LowerPath);
        OutIncludeFilePaths.Add(IncludePath);
    }

    return true;
}
