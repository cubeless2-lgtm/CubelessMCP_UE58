// Specialized legacy Custom-node inputs mapped to the shared audited Add core.
// Included after the generic mutation helpers and the UE56 parser closure.

bool ReadCustomArray(const TSharedPtr<FJsonObject>& Input, const TCHAR* InputName, const TCHAR* ParsedName,
    int32 Limit, const TSharedPtr<FJsonObject>& Parsed, FString& Error)
{
    FString Text;
    TSharedPtr<FJsonValue> Value;
    if (!Input->TryGetStringField(InputName, Text) || !ParseMutationJson(Text, Value, Error)) { return false; }
    if (Value->Type != EJson::Array || Value->AsArray().Num() > Limit)
    {
        Error = FString::Printf(TEXT("%s must encode a JSON array with at most %d entries."), InputName, Limit);
        return false;
    }
    Parsed->SetArrayField(ParsedName, Value->AsArray());
    return true;
}

bool ExportCustomArrayDefault(const TCHAR* Name, const void* Value, FString& OutText, FString& Error)
{
    const FArrayProperty* Property = FindFProperty<FArrayProperty>(UMaterialExpressionCustom::StaticClass(), Name);
    if (!Property) { Error = FString(TEXT("Expected public Custom array property is unavailable: ")) + Name; return false; }
    // Export actual typed arrays rather than constructing UE import text from user strings.
    Property->ExportTextItem_Direct(OutText, Value, nullptr, nullptr, PPF_None);
    if (OutText.IsEmpty()) { OutText = TEXT("()"); }
    return true;
}

bool PrepareCustomNodeRequestImpl(const TSharedPtr<FJsonObject>& Input, TSharedPtr<FJsonObject>& OutParams,
    FString& ErrorCode, FString& Error)
{
    ErrorCode = TEXT("custom_material_input_invalid");
    FString Code, Description, OutputTypeText;
    if (!Input->TryGetStringField(TEXT("code"), Code) || Code.TrimStartAndEnd().IsEmpty() || Code.Len() > 65536)
    {
        Error = TEXT("Custom code must be nonempty and at most 65536 UTF-16 code units."); return false;
    }
    if (!Input->TryGetStringField(TEXT("description"), Description) || Description.Len() > 4096)
    {
        Error = TEXT("Custom description must be a string of at most 4096 UTF-16 code units."); return false;
    }
    if (!Input->TryGetStringField(TEXT("output_type"), OutputTypeText)) { Error = TEXT("Custom output_type is required by the native adapter."); return false; }
    auto Parsed = MakeShared<FJsonObject>();
    if (!ReadCustomArray(Input, TEXT("inputs_json"), TEXT("inputs"), 64, Parsed, Error)
        || !ReadCustomArray(Input, TEXT("additional_defines_json"), TEXT("additional_defines"), 64, Parsed, Error)
        || !ReadCustomArray(Input, TEXT("include_file_paths_json"), TEXT("include_file_paths"), 32, Parsed, Error)) { return false; }
    ECustomMaterialOutputType OutputType = CMOT_Float3;
    TArray<FName> InputNames;
    TArray<FCustomDefine> Defines;
    TArray<FString> Includes;
    if (!ParseCustomOutputType(OutputTypeText, OutputType, Error)
        || !ParseCustomInputNames(Parsed, InputNames, Error)
        || !ParseCustomDefines(Parsed, Defines, Error)
        || !ParseIncludeFilePaths(Parsed, Includes, Error)) { return false; }
    bool bHasInclude = ContainsCustomIncludeDirective(Code, true, &Error);
    for (const FCustomDefine& Define : Defines)
    { bHasInclude |= ContainsCustomIncludeDirective(Define.DefineValue, true, &Error, false); }
    for (const FString& IncludePath : Includes)
    { if (!ValidateCustomShaderFilePath(IncludePath, Error)) { bHasInclude = true; break; } }
    if (bHasInclude)
    {
        ErrorCode = TEXT("custom_shader_includes_unsupported");
        if (Error.IsEmpty())
        { Error = TEXT("Custom shader dependencies outside the checked literal leaf-file scope remain incomplete; no expression was created for this refused request."); }
        return false;
    }

    TArray<FCustomInput> Inputs;
    for (FName Name : InputNames) { FCustomInput& Item = Inputs.AddDefaulted_GetRef(); Item.InputName = Name; }
    FString InputsText, IncludesText;
    if (!ExportCustomArrayDefault(TEXT("Inputs"), &Inputs, InputsText, Error)
        || !ExportCustomArrayDefault(TEXT("IncludeFilePaths"), &Includes, IncludesText, Error)) { return false; }
    auto Properties = MakeShared<FJsonObject>();
    Properties->SetStringField(TEXT("Code"), Code);
    Properties->SetStringField(TEXT("OutputType"), StaticEnum<ECustomMaterialOutputType>()->GetNameStringByValue(OutputType));
    Properties->SetStringField(TEXT("Description"), Description);
    Properties->SetStringField(TEXT("Desc"), Description);
    Properties->SetStringField(TEXT("Inputs"), InputsText);
    // Preserve legacy parser aliases/default/value text, then use the exact
    // shared typed scratch path rather than export-to-raw-import roundtripping.
    TArray<TSharedPtr<FJsonValue>> DefineRows;
    for(const FCustomDefine& Define:Defines)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("DefineName"),Define.DefineName);Row->SetStringField(TEXT("DefineValue"),Define.DefineValue);
        DefineRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Properties->SetArrayField(TEXT("AdditionalDefines"),DefineRows);
    Properties->SetStringField(TEXT("IncludeFilePaths"), IncludesText);
    const FString PropertiesText = ToJson(Properties);
    if (PropertiesText.Len() > 65536)
    {
        Error = TEXT("Combined encoded Custom properties exceed the shared Add core's 65536 UTF-16 limit."); return false;
    }
    OutParams = MakeShared<FJsonObject>();
    OutParams->Values = Input->Values;
    OutParams->SetStringField(TEXT("expression_class"), TEXT("/Script/Engine.MaterialExpressionCustom"));
    OutParams->SetStringField(TEXT("selected_asset"), FString());
    OutParams->SetStringField(TEXT("properties_json"), PropertiesText);
    OutParams->SetBoolField(TEXT("cubeless_custom_builder"), true);
    return true;
}
