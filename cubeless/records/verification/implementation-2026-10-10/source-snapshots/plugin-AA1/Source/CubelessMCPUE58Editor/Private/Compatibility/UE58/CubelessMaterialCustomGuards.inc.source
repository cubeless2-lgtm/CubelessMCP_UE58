// Shared by the specialized Custom builder and generic Add/Set property paths.
// A typed converted value is inspected before any persistent property assignment.
bool ContainsCustomIncludeDirective(FString Text)
{
    Text.ReplaceInline(TEXT("\\\r\n"), TEXT(""));
    Text.ReplaceInline(TEXT("\\\n"), TEXT(""));
    // Conservative: a complete HLSL preprocessor is not implemented. Rejecting
    // markers even in comments/strings also blocks #/**/include and alternate # tokens.
    return Text.Contains(TEXT("#")) || Text.Contains(TEXT("%:")) || Text.Contains(TEXT("?" "?="));
}

bool ValidateCustomShaderProperty(UObject* Object, const FProperty* Property, const void* Address, FString& Error)
{
    if (!Object->IsA<UMaterialExpressionCustom>()) { return true; }
    const FName Name = Property->GetFName();
    bool bUnsupported = false;
    if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Code))
    {
        bUnsupported = ContainsCustomIncludeDirective(*static_cast<const FString*>(Address));
    }
    else if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, IncludeFilePaths))
    {
        bUnsupported = !static_cast<const TArray<FString>*>(Address)->IsEmpty();
    }
    else if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, AdditionalDefines))
    {
        for (const FCustomDefine& Define : *static_cast<const TArray<FCustomDefine>*>(Address))
        {
            bUnsupported |= ContainsCustomIncludeDirective(Define.DefineName) || ContainsCustomIncludeDirective(Define.DefineValue);
        }
    }
    else if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Inputs))
    {
        for (const FCustomInput& Input : *static_cast<const TArray<FCustomInput>*>(Address))
        {
            bUnsupported |= ContainsCustomIncludeDirective(Input.InputName.ToString());
        }
    }
    else if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, AdditionalOutputs))
    {
        for (const FCustomOutput& Output : *static_cast<const TArray<FCustomOutput>*>(Address))
        {
            bUnsupported |= ContainsCustomIncludeDirective(Output.OutputName.ToString());
        }
    }
    if (bUnsupported)
    {
        Error = TEXT("Custom shader include/preprocessor dependency support is incomplete. IncludeFilePaths must be empty and Code/define/input/output text cannot contain directive markers; this restriction also applies to generic Add/Set.");
        return false;
    }
    return true;
}
