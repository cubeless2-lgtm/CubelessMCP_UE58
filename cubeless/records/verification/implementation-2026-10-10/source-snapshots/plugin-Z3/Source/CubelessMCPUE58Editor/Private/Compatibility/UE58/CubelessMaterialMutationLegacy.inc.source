// Adapted helper closure for add_material_node and set_material_node_property.
// Original: MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPMaterialCommands.cpp
// Source SHA-256: 8c41e028d0f6b27e0d5c2d85cad37216bab8d2234e61189c812e6a463b0d5a91
// Original comments retained; no license notice was present in this source file.
// Mutation handlers/property conversion are separately hardened in CubelessMaterialMutation.inc.

FString JsonValueToImportText(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return FString();
    }

    if (Value->Type == EJson::String)
    {
        return Value->AsString();
    }
    if (Value->Type == EJson::Boolean)
    {
        return Value->AsBool() ? TEXT("True") : TEXT("False");
    }
    if (Value->Type == EJson::Number)
    {
        return FString::SanitizeFloat(Value->AsNumber());
    }

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Value, TEXT(""), Writer);
    return Serialized;
}

bool TryGetObjectNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, double& OutValue)
{
    if (!Object.IsValid())
    {
        return false;
    }

    if (Object->TryGetNumberField(FieldName, OutValue))
    {
        return true;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
    {
        if (Pair.Key.Equals(FieldName, ESearchCase::IgnoreCase) && Pair.Value.IsValid() && Pair.Value->Type == EJson::Number)
        {
            OutValue = Pair.Value->AsNumber();
            return true;
        }
    }

    return false;
}

FString JsonValueToStructImportText(const TSharedPtr<FJsonValue>& Value, const FString& StructName)
{
    TArray<double> Numbers;
    if (Value.IsValid() && Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Array = Value->AsArray();
        for (const TSharedPtr<FJsonValue>& ArrayValue : Array)
        {
            Numbers.Add(ArrayValue.IsValid() ? ArrayValue->AsNumber() : 0.0);
        }
    }
    else if (Value.IsValid() && Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        auto AddField = [&Numbers, &Object](const TCHAR* FieldName, double DefaultValue)
        {
            double Number = DefaultValue;
            TryGetObjectNumber(Object, FieldName, Number);
            Numbers.Add(Number);
        };

        if (StructName == TEXT("LinearColor") || StructName == TEXT("Color"))
        {
            AddField(TEXT("R"), 0.0);
            AddField(TEXT("G"), 0.0);
            AddField(TEXT("B"), 0.0);
            AddField(TEXT("A"), 1.0);
        }
        else
        {
            AddField(TEXT("X"), 0.0);
            AddField(TEXT("Y"), 0.0);
            AddField(TEXT("Z"), 0.0);
            AddField(TEXT("W"), 0.0);
        }
    }

    auto NumberAt = [&Numbers](int32 Index, double DefaultValue)
    {
        return Numbers.IsValidIndex(Index) ? Numbers[Index] : DefaultValue;
    };

    if (StructName == TEXT("LinearColor") || StructName == TEXT("Color"))
    {
        return FString::Printf(TEXT("(R=%s,G=%s,B=%s,A=%s)"),
            *FString::SanitizeFloat(NumberAt(0, 0.0)),
            *FString::SanitizeFloat(NumberAt(1, 0.0)),
            *FString::SanitizeFloat(NumberAt(2, 0.0)),
            *FString::SanitizeFloat(NumberAt(3, 1.0)));
    }
    if (StructName == TEXT("Vector2D"))
    {
        return FString::Printf(TEXT("(X=%s,Y=%s)"),
            *FString::SanitizeFloat(NumberAt(0, 0.0)),
            *FString::SanitizeFloat(NumberAt(1, 0.0)));
    }
    if (StructName == TEXT("Vector4"))
    {
        return FString::Printf(TEXT("(X=%s,Y=%s,Z=%s,W=%s)"),
            *FString::SanitizeFloat(NumberAt(0, 0.0)),
            *FString::SanitizeFloat(NumberAt(1, 0.0)),
            *FString::SanitizeFloat(NumberAt(2, 0.0)),
            *FString::SanitizeFloat(NumberAt(3, 0.0)));
    }

    return FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),
        *FString::SanitizeFloat(NumberAt(0, 0.0)),
        *FString::SanitizeFloat(NumberAt(1, 0.0)),
        *FString::SanitizeFloat(NumberAt(2, 0.0)));
}

UClass* FindMaterialExpressionClass(const FString& ExpressionClassName)
{
    const FString NormalizedPath = NormalizeObjectPathForLoad(ExpressionClassName);
    if (UClass* Class = LoadObject<UClass>(nullptr, *NormalizedPath))
    {
        return Class->IsChildOf(UMaterialExpression::StaticClass()) ? Class : nullptr;
    }

    TArray<FString> CandidateClassNames;
    CandidateClassNames.Add(ExpressionClassName);

    FString StrippedName = ExpressionClassName;
    if (StrippedName.StartsWith(TEXT("U")) && StrippedName.Len() > 1)
    {
        StrippedName.RightChopInline(1);
    }
    CandidateClassNames.Add(StrippedName);

    if (!StrippedName.StartsWith(TEXT("MaterialExpression")))
    {
        CandidateClassNames.Add(FString::Printf(TEXT("MaterialExpression%s"), *StrippedName));
    }

    TArray<FString> CandidatePaths;
    for (const FString& CandidateName : CandidateClassNames)
    {
        CandidatePaths.Add(CandidateName);
        CandidatePaths.Add(FString::Printf(TEXT("/Script/Engine.%s"), *CandidateName));
    }

    for (const FString& CandidatePath : CandidatePaths)
    {
        if (UClass* Class = FindFirstObject<UClass>(*CandidatePath))
        {
            if (Class->IsChildOf(UMaterialExpression::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
            {
                return Class;
            }
        }

        if (UClass* Class = LoadObject<UClass>(nullptr, *CandidatePath))
        {
            if (Class->IsChildOf(UMaterialExpression::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
            {
                return Class;
            }
        }
    }

    return nullptr;
}

bool ShouldDeferImmediateMaterialExpressionUpdate(const UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return true;
    }

    // External-code expressions, such as SkyAtmosphere nodes, can require editor
    // graph/compiler state that is not fully initialized immediately after creation.
    return Expression->IsA<UMaterialExpressionCustom>() ||
        Expression->IsA<UMaterialExpressionExternalCodeBase>();
}

void RefreshExpressionNodeSafe(UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return;
    }

#if WITH_EDITORONLY_DATA
    // UMaterialExpression::RefreshNode dereferences GraphNode unconditionally.
    // GraphNode only exists while the material editor has this graph open, so
    // calling RefreshNode on an expression created headlessly (MCP, scripts)
    // crashes the editor with an access violation. Skip it when there is no
    // graph node; MarkChanged/PostEditChange still propagates the change.
    if (Expression->GraphNode)
    {
        Expression->RefreshNode(true);
    }
#endif
}
