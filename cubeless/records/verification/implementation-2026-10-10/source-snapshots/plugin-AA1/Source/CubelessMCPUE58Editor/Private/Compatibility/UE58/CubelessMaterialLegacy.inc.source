// Adapted from user-authorized UnrealMCP5_6_1 Material command domain logic.
// Original: MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPMaterialCommands.cpp
// Source SHA-256: 8c41e028d0f6b27e0d5c2d85cad37216bab8d2234e61189c812e6a463b0d5a91
// Only the dependency closure of the nine explicitly wrapped operations is present.
// Original comments retained; no license notice was present in this source file.
// This file is included once inside the UE58 compatibility translation unit.

struct FMaterialGraphTarget
{
    UObject* Asset = nullptr;
    UMaterial* Material = nullptr;
    UMaterialFunction* Function = nullptr;

    bool IsValid() const
    {
        return Material != nullptr || Function != nullptr;
    }

    bool IsFunctionGraph() const
    {
        return Function != nullptr;
    }

    FString GetGraphType() const
    {
        return IsFunctionGraph() ? TEXT("function") : TEXT("material");
    }

    FString GetPathName() const
    {
        return Asset ? Asset->GetPathName() : FString();
    }

    TConstArrayView<TObjectPtr<UMaterialExpression>> GetExpressions() const
    {
        if (Material)
        {
            return Material->GetExpressions();
        }
        if (Function)
        {
            return Function->GetExpressions();
        }
        return TConstArrayView<TObjectPtr<UMaterialExpression>>();
    }

    bool HasUnconnectedCustomInputs() const
    {
        for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : GetExpressions())
        {
            if (const UMaterialExpressionCustom* CustomExpression = Cast<UMaterialExpressionCustom>(ExpressionPtr.Get()))
            {
                for (const FCustomInput& CustomInput : CustomExpression->Inputs)
                {
                    if (!CustomInput.Input.Expression)
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void MarkChanged(bool bPostEditChange = true) const
    {
        if (!Asset)
        {
            return;
        }

        Asset->Modify();
        Asset->MarkPackageDirty();
        if (bPostEditChange && !HasUnconnectedCustomInputs())
        {
            Asset->PostEditChange();
        }
    }
};

FString NormalizeObjectPathForLoad(const FString& ObjectPath)
{
    FString NormalizedPath = FPackageName::ExportTextPathToObjectPath(ObjectPath).TrimStartAndEnd();
    NormalizedPath.TrimQuotesInline();

    if ((NormalizedPath.StartsWith(TEXT("/Game/")) || NormalizedPath.StartsWith(TEXT("/Engine/"))) && !NormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(NormalizedPath);
        NormalizedPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    return NormalizedPath;
}

FString NormalizeMaterialGraphType(const FString& GraphType)
{
    if (GraphType.Equals(TEXT("material_function"), ESearchCase::IgnoreCase) ||
        GraphType.Equals(TEXT("function"), ESearchCase::IgnoreCase))
    {
        return TEXT("function");
    }
    if (GraphType.Equals(TEXT("material"), ESearchCase::IgnoreCase))
    {
        return TEXT("material");
    }
    return TEXT("auto");
}

bool LoadMaterialGraphByObjectPath(const FString& GraphPath, const FString& GraphType, FMaterialGraphTarget& OutTarget, FString& OutError)
{
    const FString Query = NormalizeObjectPathForLoad(GraphPath);
    const FString NormalizedGraphType = NormalizeMaterialGraphType(GraphType);

    if (NormalizedGraphType != TEXT("function"))
    {
        if (UMaterial* Material = LoadObject<UMaterial>(nullptr, *Query))
        {
            OutTarget.Asset = Material;
            OutTarget.Material = Material;
            return true;
        }
    }

    if (NormalizedGraphType != TEXT("material"))
    {
        if (UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, *Query))
        {
            OutTarget.Asset = Function;
            OutTarget.Function = Function;
            return true;
        }
    }

    if (UMaterialInstance* MaterialInstance = LoadObject<UMaterialInstance>(nullptr, *Query))
    {
        OutError = FString::Printf(
            TEXT("Material Instance has no expression graph: %s. Use parameter tools instead."),
            *MaterialInstance->GetPathName());
    }

    return false;
}

void AppendAssetCandidatesForClass(UClass* AssetClass, const FString& GraphPathOrName, TArray<FString>& CandidatePaths)
{
    if (!AssetClass)
    {
        return;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssetsByClass(AssetClass->GetClassPathName(), Assets, true);

    const FString Query = NormalizeObjectPathForLoad(GraphPathOrName);
    const FString ShortQuery = FPackageName::GetShortName(Query);

    for (const FAssetData& AssetData : Assets)
    {
        const FString AssetName = AssetData.AssetName.ToString();
        const FString PackageName = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();

        if (AssetName.Equals(GraphPathOrName, ESearchCase::IgnoreCase) ||
            AssetName.Equals(ShortQuery, ESearchCase::IgnoreCase) ||
            PackageName.Equals(GraphPathOrName, ESearchCase::IgnoreCase) ||
            ObjectPath.Equals(Query, ESearchCase::IgnoreCase))
        {
            CandidatePaths.AddUnique(ObjectPath);
        }
    }
}

TArray<FString> FindMaterialGraphAssetPaths(const FString& GraphPathOrName, const FString& GraphType)
{
    TArray<FString> CandidatePaths;
    const FString NormalizedGraphType = NormalizeMaterialGraphType(GraphType);

    FMaterialGraphTarget DirectTarget;
    FString DirectError;
    if (LoadMaterialGraphByObjectPath(GraphPathOrName, NormalizedGraphType, DirectTarget, DirectError))
    {
        CandidatePaths.Add(DirectTarget.GetPathName());
        return CandidatePaths;
    }

    if (NormalizedGraphType != TEXT("function"))
    {
        AppendAssetCandidatesForClass(UMaterial::StaticClass(), GraphPathOrName, CandidatePaths);
    }
    if (NormalizedGraphType != TEXT("material"))
    {
        AppendAssetCandidatesForClass(UMaterialFunction::StaticClass(), GraphPathOrName, CandidatePaths);
    }

    return CandidatePaths;
}

bool ResolveMaterialGraph(const FString& GraphPath, const FString& GraphType, FMaterialGraphTarget& OutTarget, TArray<FString>* OutCandidates, FString& OutError)
{
    if (LoadMaterialGraphByObjectPath(GraphPath, GraphType, OutTarget, OutError))
    {
        return true;
    }

    const TArray<FString> CandidatePaths = FindMaterialGraphAssetPaths(GraphPath, GraphType);
    if (OutCandidates)
    {
        *OutCandidates = CandidatePaths;
    }

    if (CandidatePaths.Num() == 1)
    {
        OutError.Reset();
        return LoadMaterialGraphByObjectPath(CandidatePaths[0], GraphType, OutTarget, OutError);
    }

    if (OutError.IsEmpty())
    {
        if (CandidatePaths.Num() > 1)
        {
            OutError = FString::Printf(TEXT("Ambiguous material graph '%s'. Use a full object path. Candidates: %s"),
                *GraphPath,
                *FString::Join(CandidatePaths, TEXT(", ")));
        }
        else
        {
            OutError = FString::Printf(TEXT("Material or Material Function not found: %s"), *GraphPath);
        }
    }

    return false;
}

FProperty* FindPropertyByNameLoose(UObject* Object, const FString& PropertyName)
{
    // UE58 FNAME_Add checks fatally at NAME_SIZE. This helper is also used by
    // readback before mutation validation, so reject before implicit FName creation.
    if (!Object || PropertyName.Len() >= NAME_SIZE)
    {
        return nullptr;
    }

    if (FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName))
    {
        return Property;
    }

    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (Property && Property->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
        {
            return Property;
        }
    }

    return nullptr;
}

UObject* LoadObjectValue(const FString& ObjectPath)
{
    return LoadObject<UObject>(nullptr, *NormalizeObjectPathForLoad(ObjectPath));
}

FString GetExpressionKey(const UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return FString();
    }

    return FString::Printf(TEXT("path:%s"), *Expression->GetPathName());
}

FString GetExpressionLegacyId(const UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return FString();
    }

    if (Expression->MaterialExpressionGuid.IsValid())
    {
        return Expression->MaterialExpressionGuid.ToString();
    }

    return Expression->GetName();
}

FString GetExpressionId(const UMaterialExpression* Expression)
{
    return GetExpressionKey(Expression);
}

FString GetExpressionGuidString(const UMaterialExpression* Expression)
{
    return Expression ? Expression->MaterialExpressionGuid.ToString() : FString();
}

int32 GetExpressionGuidDuplicateCount(
    const UMaterialExpression* Expression,
    TConstArrayView<TObjectPtr<UMaterialExpression>> AllExpressions)
{
    if (!Expression || !Expression->MaterialExpressionGuid.IsValid())
    {
        return 0;
    }

    int32 Count = 0;
    for (const TObjectPtr<UMaterialExpression>& OtherExpressionPtr : AllExpressions)
    {
        const UMaterialExpression* OtherExpression = OtherExpressionPtr.Get();
        if (OtherExpression && OtherExpression->MaterialExpressionGuid == Expression->MaterialExpressionGuid)
        {
            ++Count;
        }
    }
    return Count;
}

TArray<TSharedPtr<FJsonValue>> GetExpressionGuidCollisionMembers(
    const UMaterialExpression* Expression,
    TConstArrayView<TObjectPtr<UMaterialExpression>> AllExpressions)
{
    TArray<TSharedPtr<FJsonValue>> Members;
    if (!Expression || !Expression->MaterialExpressionGuid.IsValid())
    {
        return Members;
    }

    for (const TObjectPtr<UMaterialExpression>& OtherExpressionPtr : AllExpressions)
    {
        const UMaterialExpression* OtherExpression = OtherExpressionPtr.Get();
        if (!OtherExpression || OtherExpression->MaterialExpressionGuid != Expression->MaterialExpressionGuid)
        {
            continue;
        }

        TSharedPtr<FJsonObject> Member = MakeShared<FJsonObject>();
        Member->SetStringField(TEXT("node_key"), GetExpressionKey(OtherExpression));
        Member->SetStringField(TEXT("name"), OtherExpression->GetName());
        Member->SetStringField(TEXT("class"), OtherExpression->GetClass()->GetName());
        Members.Add(MakeShared<FJsonValueObject>(Member));
    }

    return Members;
}

void AddExpressionReferenceFields(TSharedPtr<FJsonObject> Object, const UMaterialExpression* Expression, const FString& Prefix)
{
    if (!Object.IsValid() || !Expression)
    {
        return;
    }

    Object->SetStringField(Prefix + TEXT("node_id"), GetExpressionId(Expression));
    Object->SetStringField(Prefix + TEXT("node_key"), GetExpressionKey(Expression));
    Object->SetStringField(Prefix + TEXT("legacy_node_id"), GetExpressionLegacyId(Expression));
    Object->SetStringField(Prefix + TEXT("guid"), GetExpressionGuidString(Expression));
    Object->SetStringField(Prefix + TEXT("node_name"), Expression->GetName());
    Object->SetStringField(Prefix + TEXT("object_path"), Expression->GetPathName());
}

FString GetFallbackOutputName(const FExpressionOutput& Output)
{
    if (!Output.OutputName.IsNone())
    {
        return Output.OutputName.ToString();
    }
    if (Output.MaskR && !Output.MaskG && !Output.MaskB && !Output.MaskA)
    {
        return TEXT("R");
    }
    if (!Output.MaskR && Output.MaskG && !Output.MaskB && !Output.MaskA)
    {
        return TEXT("G");
    }
    if (!Output.MaskR && !Output.MaskG && Output.MaskB && !Output.MaskA)
    {
        return TEXT("B");
    }
    if (!Output.MaskR && !Output.MaskG && !Output.MaskB && Output.MaskA)
    {
        return TEXT("A");
    }
    return FString();
}

TArray<FString> GetMaterialExpressionInputNamesSafe(UMaterialExpression* Expression)
{
    TArray<FString> InputNames;
    if (!Expression)
    {
        return InputNames;
    }

    InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(Expression);
    if (InputNames.Num() == 0)
    {
        for (FExpressionInputIterator It{ Expression }; It; ++It)
        {
            InputNames.Add(Expression->GetInputName(It.Index).ToString());
        }
    }

    return InputNames;
}

TArray<FString> GetMaterialExpressionOutputNames(UMaterialExpression* Expression)
{
    TArray<FString> OutputNames;
    if (!Expression)
    {
        return OutputNames;
    }

    for (const FExpressionOutput& Output : Expression->Outputs)
    {
        OutputNames.Add(GetFallbackOutputName(Output));
    }

    return OutputNames;
}

int32 FindOutputIndexByName(UMaterialExpression* Expression, const FString& OutputName)
{
    if (!Expression || Expression->Outputs.Num() == 0)
    {
        return INDEX_NONE;
    }
    if (OutputName.IsEmpty())
    {
        return 0;
    }

    for (int32 Index = 0; Index < Expression->Outputs.Num(); ++Index)
    {
        const FString CandidateName = GetFallbackOutputName(Expression->Outputs[Index]);
        if (CandidateName.Equals(OutputName, ESearchCase::IgnoreCase))
        {
            return Index;
        }
    }

    return INDEX_NONE;
}

FString ResolveOutputConnectName(UMaterialExpression* Expression, const FString& RequestedOutputName)
{
    if (!Expression || RequestedOutputName.IsEmpty())
    {
        return FString();
    }

    for (const FString& CandidateName : GetMaterialExpressionOutputNames(Expression))
    {
        if (CandidateName.Equals(RequestedOutputName, ESearchCase::IgnoreCase))
        {
            return CandidateName;
        }
    }

    return RequestedOutputName;
}

bool HasInputName(UMaterialExpression* Expression, const FString& InputName)
{
    if (!Expression)
    {
        return false;
    }
    if (InputName.IsEmpty())
    {
        return Expression->GetInput(0) != nullptr;
    }

    const TArray<FString> InputNames = GetMaterialExpressionInputNamesSafe(Expression);
    for (const FString& CandidateName : InputNames)
    {
        if (CandidateName.Equals(InputName, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }

    return false;
}

FString ResolveInputConnectName(UMaterialExpression* Expression, const FString& RequestedInputName)
{
    if (!Expression || RequestedInputName.IsEmpty())
    {
        return FString();
    }

    for (const FString& CandidateName : GetMaterialExpressionInputNamesSafe(Expression))
    {
        if (CandidateName.Equals(RequestedInputName, ESearchCase::IgnoreCase))
        {
            return CandidateName;
        }
    }

    return RequestedInputName;
}

bool FindExpressionById(const FMaterialGraphTarget& Target, const FString& NodeId, UMaterialExpression*& OutExpression, FString& OutError)
{
    TArray<UMaterialExpression*> Matches;
    FString Query = NodeId.TrimStartAndEnd();
    FString Prefix;
    FString Value = Query;
    if (Query.Split(TEXT(":"), &Prefix, &Value))
    {
        Prefix = Prefix.ToLower();
    }
    else
    {
        Prefix.Reset();
    }

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Target.GetExpressions())
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (!Expression)
        {
            continue;
        }

        bool bMatches = false;
        if (Prefix == TEXT("path"))
        {
            bMatches = Expression->GetPathName().Equals(Value, ESearchCase::IgnoreCase);
        }
        else if (Prefix == TEXT("name"))
        {
            bMatches = Expression->GetName().Equals(Value, ESearchCase::IgnoreCase);
        }
        else if (Prefix == TEXT("guid"))
        {
            bMatches = GetExpressionGuidString(Expression).Equals(Value, ESearchCase::IgnoreCase);
        }
        else
        {
            bMatches =
                GetExpressionId(Expression).Equals(Query, ESearchCase::IgnoreCase) ||
                GetExpressionKey(Expression).Equals(Query, ESearchCase::IgnoreCase) ||
                GetExpressionLegacyId(Expression).Equals(Query, ESearchCase::IgnoreCase) ||
                GetExpressionGuidString(Expression).Equals(Query, ESearchCase::IgnoreCase) ||
                Expression->GetName().Equals(Query, ESearchCase::IgnoreCase) ||
                Expression->GetPathName().Equals(Query, ESearchCase::IgnoreCase);
        }

        if (bMatches)
        {
            Matches.Add(Expression);
        }
    }

    if (Matches.Num() == 1)
    {
        OutExpression = Matches[0];
        return true;
    }
    if (Matches.Num() > 1)
    {
        TArray<FString> MatchIds;
        for (UMaterialExpression* Match : Matches)
        {
            MatchIds.Add(FString::Printf(TEXT("%s guid=%s (%s)"), *GetExpressionKey(Match), *GetExpressionGuidString(Match), *Match->GetClass()->GetName()));
        }
        OutError = FString::Printf(TEXT("Ambiguous material node id '%s'. Matches: %s"), *NodeId, *FString::Join(MatchIds, TEXT(", ")));
        return false;
    }

    OutError = FString::Printf(TEXT("Material node not found: %s"), *NodeId);
    return false;
}

TSharedPtr<FJsonObject> ExpressionInputToJson(UMaterialExpression* Expression, int32 InputIndex, const FString& InputName)
{
    TSharedPtr<FJsonObject> InputObject = MakeShared<FJsonObject>();
    InputObject->SetStringField(TEXT("name"), InputName);
    InputObject->SetNumberField(TEXT("index"), InputIndex);

    if (!Expression)
    {
        return InputObject;
    }

    InputObject->SetStringField(TEXT("raw_name"), Expression->GetInputName(InputIndex).ToString());
    InputObject->SetBoolField(TEXT("required"), Expression->IsInputConnectionRequired(InputIndex));
    InputObject->SetNumberField(TEXT("value_type"), static_cast<int32>(Expression->GetInputValueType(InputIndex)));

    FExpressionInput* Input = Expression->GetInput(InputIndex);
    if (Input && Input->Expression)
    {
        InputObject->SetBoolField(TEXT("connected"), true);
        AddExpressionReferenceFields(InputObject, Input->Expression, TEXT("connected_"));
        InputObject->SetStringField(TEXT("connected_output"), Input->Expression->Outputs.IsValidIndex(Input->OutputIndex)
            ? GetFallbackOutputName(Input->Expression->Outputs[Input->OutputIndex])
            : FString());
        InputObject->SetNumberField(TEXT("connected_output_index"), Input->OutputIndex);
    }
    else
    {
        InputObject->SetBoolField(TEXT("connected"), false);
    }

    return InputObject;
}

TSharedPtr<FJsonObject> ExpressionOutputToJson(
    UMaterialExpression* Expression,
    int32 OutputIndex,
    const FExpressionOutput& Output,
    TConstArrayView<TObjectPtr<UMaterialExpression>> AllExpressions)
{
    TSharedPtr<FJsonObject> OutputObject = MakeShared<FJsonObject>();
    const FString OutputName = GetFallbackOutputName(Output);
    OutputObject->SetStringField(TEXT("name"), OutputName);
    OutputObject->SetStringField(TEXT("connect_name"), OutputName);
    OutputObject->SetNumberField(TEXT("index"), OutputIndex);
    OutputObject->SetNumberField(TEXT("value_type"), Expression ? static_cast<int32>(Expression->GetOutputValueType(OutputIndex)) : 0);
    OutputObject->SetBoolField(TEXT("mask"), Output.Mask != 0);
    OutputObject->SetBoolField(TEXT("mask_r"), Output.MaskR != 0);
    OutputObject->SetBoolField(TEXT("mask_g"), Output.MaskG != 0);
    OutputObject->SetBoolField(TEXT("mask_b"), Output.MaskB != 0);
    OutputObject->SetBoolField(TEXT("mask_a"), Output.MaskA != 0);

    TArray<TSharedPtr<FJsonValue>> ConnectedTo;
    for (const TObjectPtr<UMaterialExpression>& OtherExpressionPtr : AllExpressions)
    {
        UMaterialExpression* OtherExpression = OtherExpressionPtr.Get();
        if (!OtherExpression)
        {
            continue;
        }

        const TArray<FString> InputNames = GetMaterialExpressionInputNamesSafe(OtherExpression);
        for (int32 InputIndex = 0; InputIndex < InputNames.Num(); ++InputIndex)
        {
            FExpressionInput* Input = OtherExpression->GetInput(InputIndex);
            if (Input && Input->Expression == Expression && Input->OutputIndex == OutputIndex)
            {
                TSharedPtr<FJsonObject> ConnectionObject = MakeShared<FJsonObject>();
                AddExpressionReferenceFields(ConnectionObject, OtherExpression, TEXT(""));
                ConnectionObject->SetStringField(TEXT("input"), InputNames[InputIndex]);
                ConnectionObject->SetNumberField(TEXT("input_index"), InputIndex);
                ConnectedTo.Add(MakeShared<FJsonValueObject>(ConnectionObject));
            }
        }
    }
    OutputObject->SetArrayField(TEXT("connected_to"), ConnectedTo);

    return OutputObject;
}

FString ObjectPathOrEmpty(const UObject* Object)
{
    return Object ? Object->GetPathName() : FString();
}

FString EnumNameOrValue(UEnum* Enum, int64 Value)
{
    if (!Enum)
    {
        return FString::FromInt(static_cast<int32>(Value));
    }

    FString Name = Enum->GetNameStringByValue(Value);
    return Name.IsEmpty() ? FString::FromInt(static_cast<int32>(Value)) : Name;
}

TSharedPtr<FJsonObject> FunctionCallToJson(UMaterialExpressionMaterialFunctionCall* FunctionCall)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!FunctionCall)
    {
        return Object;
    }

    UMaterialFunctionInterface* Function = FunctionCall->MaterialFunction;
    Object->SetBoolField(TEXT("has_function"), Function != nullptr);
    Object->SetStringField(TEXT("function_path"), ObjectPathOrEmpty(Function));
    Object->SetStringField(TEXT("function_name"), Function ? Function->GetName() : FString());
    Object->SetStringField(TEXT("function_class"), Function ? Function->GetClass()->GetName() : FString());

    UEnum* InputTypeEnum = StaticEnum<EFunctionInputType>();
    TArray<TSharedPtr<FJsonValue>> Inputs;
    for (int32 Index = 0; Index < FunctionCall->FunctionInputs.Num(); ++Index)
    {
        const FFunctionExpressionInput& FunctionInput = FunctionCall->FunctionInputs[Index];
        TSharedPtr<FJsonObject> InputObject = MakeShared<FJsonObject>();
        InputObject->SetNumberField(TEXT("index"), Index);
        InputObject->SetStringField(TEXT("id"), FunctionInput.ExpressionInputId.ToString());
        if (UMaterialExpressionFunctionInput* ExpressionInput = FunctionInput.ExpressionInput)
        {
            InputObject->SetStringField(TEXT("name"), ExpressionInput->InputName.ToString());
            InputObject->SetStringField(TEXT("description"), ExpressionInput->Description);
            InputObject->SetStringField(TEXT("type"), EnumNameOrValue(InputTypeEnum, static_cast<int64>(ExpressionInput->InputType.GetValue())));
            InputObject->SetNumberField(TEXT("sort_priority"), ExpressionInput->SortPriority);
            InputObject->SetBoolField(TEXT("use_preview_as_default"), ExpressionInput->bUsePreviewValueAsDefault != 0);
        }
        InputObject->SetBoolField(TEXT("connected"), FunctionInput.Input.Expression != nullptr);
        if (FunctionInput.Input.Expression)
        {
            AddExpressionReferenceFields(InputObject, FunctionInput.Input.Expression, TEXT("connected_"));
            InputObject->SetNumberField(TEXT("connected_output_index"), FunctionInput.Input.OutputIndex);
        }
        Inputs.Add(MakeShared<FJsonValueObject>(InputObject));
    }
    Object->SetArrayField(TEXT("inputs"), Inputs);

    TArray<TSharedPtr<FJsonValue>> Outputs;
    for (int32 Index = 0; Index < FunctionCall->FunctionOutputs.Num(); ++Index)
    {
        const FFunctionExpressionOutput& FunctionOutput = FunctionCall->FunctionOutputs[Index];
        TSharedPtr<FJsonObject> OutputObject = MakeShared<FJsonObject>();
        OutputObject->SetNumberField(TEXT("index"), Index);
        OutputObject->SetStringField(TEXT("id"), FunctionOutput.ExpressionOutputId.ToString());
        OutputObject->SetStringField(TEXT("connect_name"), GetFallbackOutputName(FunctionOutput.Output));
        if (UMaterialExpressionFunctionOutput* ExpressionOutput = FunctionOutput.ExpressionOutput)
        {
            OutputObject->SetStringField(TEXT("name"), ExpressionOutput->OutputName.ToString());
            OutputObject->SetStringField(TEXT("description"), ExpressionOutput->Description);
            OutputObject->SetNumberField(TEXT("sort_priority"), ExpressionOutput->SortPriority);
        }
        Outputs.Add(MakeShared<FJsonValueObject>(OutputObject));
    }
    Object->SetArrayField(TEXT("outputs"), Outputs);

    return Object;
}

TSharedPtr<FJsonObject> RerouteToJson(
    UMaterialExpressionRerouteBase* Reroute,
    TConstArrayView<TObjectPtr<UMaterialExpression>> AllExpressions)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!Reroute)
    {
        return Object;
    }

    int32 OutputIndex = INDEX_NONE;
    UMaterialExpression* TracedExpression = Reroute->TraceInputsToRealExpression(OutputIndex);
    Object->SetBoolField(TEXT("trace_resolved"), TracedExpression != nullptr);
    Object->SetNumberField(TEXT("trace_output_index"), OutputIndex);
    if (TracedExpression)
    {
        AddExpressionReferenceFields(Object, TracedExpression, TEXT("trace_source_"));
        Object->SetStringField(TEXT("trace_output"), TracedExpression->Outputs.IsValidIndex(OutputIndex)
            ? GetFallbackOutputName(TracedExpression->Outputs[OutputIndex])
            : FString());
    }

    if (UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Reroute))
    {
        Object->SetStringField(TEXT("kind"), TEXT("named_declaration"));
        Object->SetStringField(TEXT("name"), Declaration->Name.ToString());
        Object->SetStringField(TEXT("variable_guid"), Declaration->VariableGuid.ToString());
        Object->SetStringField(TEXT("node_color"), Declaration->NodeColor.ToString());

        TArray<TSharedPtr<FJsonValue>> Usages;
        for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : AllExpressions)
        {
            UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(ExpressionPtr.Get());
            if (!Usage)
            {
                continue;
            }
            if (Usage->Declaration == Declaration || Usage->DeclarationGuid == Declaration->VariableGuid)
            {
                TSharedPtr<FJsonObject> UsageObject = MakeShared<FJsonObject>();
                AddExpressionReferenceFields(UsageObject, Usage, TEXT(""));
                UsageObject->SetStringField(TEXT("declaration_guid"), Usage->DeclarationGuid.ToString());
                UsageObject->SetBoolField(TEXT("declaration_valid"), Usage->Declaration != nullptr);
                Usages.Add(MakeShared<FJsonValueObject>(UsageObject));
            }
        }
        Object->SetArrayField(TEXT("usages"), Usages);
        Object->SetNumberField(TEXT("usage_count"), Usages.Num());
    }
    else if (UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Reroute))
    {
        Object->SetStringField(TEXT("kind"), TEXT("named_usage"));
        Object->SetStringField(TEXT("declaration_guid"), Usage->DeclarationGuid.ToString());
        Object->SetBoolField(TEXT("declaration_valid"), Usage->Declaration != nullptr);
        if (Usage->Declaration)
        {
            AddExpressionReferenceFields(Object, Usage->Declaration, TEXT("declaration_"));
            Object->SetStringField(TEXT("declaration_name"), Usage->Declaration->Name.ToString());
            Object->SetStringField(TEXT("variable_guid"), Usage->Declaration->VariableGuid.ToString());
        }
    }
    else
    {
        Object->SetStringField(TEXT("kind"), TEXT("reroute"));
    }

    return Object;
}

FString ExportStringProperty(UObject* Object, const TCHAR* PropertyName)
{
    if (!Object)
    {
        return FString();
    }

    FProperty* Property = FindPropertyByNameLoose(Object, PropertyName);
    if (!Property)
    {
        return FString();
    }

    FString Exported;
    Property->ExportTextItem_Direct(Exported, Property->ContainerPtrToValuePtr<void>(Object), nullptr, Object, PPF_None);
    Exported.TrimQuotesInline();
    return Exported;
}

TSharedPtr<FJsonObject> CustomHlslToJson(UMaterialExpression* Expression)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!Expression)
    {
        return Object;
    }

    const FString Code = ExportStringProperty(Expression, TEXT("Code"));
    Object->SetBoolField(TEXT("has_code"), !Code.IsEmpty());
    Object->SetNumberField(TEXT("code_length"), Code.Len());
    Object->SetStringField(TEXT("code_preview"), Code.Left(512));
    Object->SetStringField(TEXT("output_type"), ExportStringProperty(Expression, TEXT("OutputType")));
    Object->SetStringField(TEXT("include_file_paths"), ExportStringProperty(Expression, TEXT("IncludeFilePaths")));
    Object->SetStringField(TEXT("additional_defines"), ExportStringProperty(Expression, TEXT("AdditionalDefines")));
    return Object;
}

TSharedPtr<FJsonObject> StaticSwitchToJson(UMaterialExpression* Expression)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!Expression)
    {
        return Object;
    }

    if (UMaterialExpressionParameter* Parameter = Cast<UMaterialExpressionParameter>(Expression))
    {
        Object->SetStringField(TEXT("parameter_name"), Parameter->ParameterName.ToString());
        Object->SetStringField(TEXT("parameter_guid"), Parameter->ExpressionGUID.ToString());
        Object->SetStringField(TEXT("group"), Parameter->Group.ToString());
        Object->SetNumberField(TEXT("sort_priority"), Parameter->SortPriority);
    }
    if (UMaterialExpressionStaticBoolParameter* StaticBool = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
    {
        Object->SetBoolField(TEXT("default_value"), StaticBool->DefaultValue != 0);
        Object->SetBoolField(TEXT("dynamic_branch"), StaticBool->DynamicBranch != 0);
    }
    return Object;
}

TSharedPtr<FJsonObject> ExpressionToJson(
    UMaterialExpression* Expression,
    bool bIncludePins,
    TConstArrayView<TObjectPtr<UMaterialExpression>> AllExpressions)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!Expression)
    {
        return NodeObject;
    }

    const int32 GuidDuplicateCount = GetExpressionGuidDuplicateCount(Expression, AllExpressions);
    NodeObject->SetStringField(TEXT("node_id"), GetExpressionId(Expression));
    NodeObject->SetStringField(TEXT("node_key"), GetExpressionKey(Expression));
    NodeObject->SetStringField(TEXT("legacy_node_id"), GetExpressionLegacyId(Expression));
    NodeObject->SetStringField(TEXT("guid"), GetExpressionGuidString(Expression));
    NodeObject->SetBoolField(TEXT("guid_valid"), Expression->MaterialExpressionGuid.IsValid());
    NodeObject->SetBoolField(TEXT("guid_duplicate"), GuidDuplicateCount > 1);
    NodeObject->SetNumberField(TEXT("guid_duplicate_count"), GuidDuplicateCount);
    if (GuidDuplicateCount > 1)
    {
        NodeObject->SetArrayField(TEXT("guid_collision_members"), GetExpressionGuidCollisionMembers(Expression, AllExpressions));
    }
    NodeObject->SetStringField(TEXT("name"), Expression->GetName());
    NodeObject->SetStringField(TEXT("object_path"), Expression->GetPathName());
    NodeObject->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
    NodeObject->SetStringField(TEXT("class_path"), Expression->GetClass()->GetPathName());
    NodeObject->SetStringField(TEXT("desc"), Expression->Desc);
    NodeObject->SetNumberField(TEXT("x"), Expression->MaterialExpressionEditorX);
    NodeObject->SetNumberField(TEXT("y"), Expression->MaterialExpressionEditorY);

    if (bIncludePins)
    {
        TArray<TSharedPtr<FJsonValue>> InputArray;
        const TArray<FString> InputNames = GetMaterialExpressionInputNamesSafe(Expression);
        for (int32 InputIndex = 0; InputIndex < InputNames.Num(); ++InputIndex)
        {
            InputArray.Add(MakeShared<FJsonValueObject>(ExpressionInputToJson(Expression, InputIndex, InputNames[InputIndex])));
        }
        NodeObject->SetArrayField(TEXT("inputs"), InputArray);

        TArray<TSharedPtr<FJsonValue>> OutputArray;
        for (int32 OutputIndex = 0; OutputIndex < Expression->Outputs.Num(); ++OutputIndex)
        {
            OutputArray.Add(MakeShared<FJsonValueObject>(ExpressionOutputToJson(Expression, OutputIndex, Expression->Outputs[OutputIndex], AllExpressions)));
        }
        NodeObject->SetArrayField(TEXT("outputs"), OutputArray);
    }

    if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
    {
        NodeObject->SetObjectField(TEXT("function_call"), FunctionCallToJson(FunctionCall));
    }
    if (UMaterialExpressionRerouteBase* Reroute = Cast<UMaterialExpressionRerouteBase>(Expression))
    {
        NodeObject->SetObjectField(TEXT("reroute"), RerouteToJson(Reroute, AllExpressions));
    }
    if (Expression->GetClass()->GetName().Contains(TEXT("MaterialExpressionCustom")))
    {
        NodeObject->SetObjectField(TEXT("custom_hlsl"), CustomHlslToJson(Expression));
    }
    if (Cast<UMaterialExpressionStaticBoolParameter>(Expression) ||
        Expression->GetClass()->GetName().Contains(TEXT("StaticSwitch")))
    {
        NodeObject->SetObjectField(TEXT("static_switch"), StaticSwitchToJson(Expression));
    }

    return NodeObject;
}

TSharedPtr<FJsonObject> MaterialPropertyConnectionToJson(UMaterial* Material, EMaterialProperty Property)
{
    TSharedPtr<FJsonObject> PropertyObject = MakeShared<FJsonObject>();
    UEnum* MaterialPropertyEnum = StaticEnum<EMaterialProperty>();
    const FString PropertyName = MaterialPropertyEnum ? MaterialPropertyEnum->GetNameStringByValue(static_cast<int64>(Property)) : FString::FromInt(static_cast<int32>(Property));
    PropertyObject->SetStringField(TEXT("property"), PropertyName);
    PropertyObject->SetNumberField(TEXT("property_value"), static_cast<int32>(Property));
    if (MaterialPropertyEnum)
    {
        PropertyObject->SetStringField(TEXT("display_name"), MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(Property)).ToString());
    }

    FExpressionInput* Input = Material ? Material->GetExpressionInputForProperty(Property) : nullptr;
    if (Input && Input->Expression)
    {
        PropertyObject->SetBoolField(TEXT("connected"), true);
        AddExpressionReferenceFields(PropertyObject, Input->Expression, TEXT(""));
        PropertyObject->SetStringField(TEXT("output"), Input->Expression->Outputs.IsValidIndex(Input->OutputIndex)
            ? GetFallbackOutputName(Input->Expression->Outputs[Input->OutputIndex])
            : FString());
        PropertyObject->SetNumberField(TEXT("output_index"), Input->OutputIndex);
    }
    else
    {
        PropertyObject->SetBoolField(TEXT("connected"), false);
    }

    return PropertyObject;
}

bool MaterialPropertyFromString(const FString& PropertyName, EMaterialProperty& OutProperty, FString& OutError)
{
    UEnum* MaterialPropertyEnum = StaticEnum<EMaterialProperty>();
    if (MaterialPropertyEnum)
    {
        int64 EnumValue = MaterialPropertyEnum->GetValueByNameString(PropertyName);
        if (EnumValue == INDEX_NONE && !PropertyName.StartsWith(TEXT("MP_")))
        {
            EnumValue = MaterialPropertyEnum->GetValueByNameString(FString::Printf(TEXT("MP_%s"), *PropertyName));
        }
        if (EnumValue != INDEX_NONE && EnumValue < MP_MAX)
        {
            OutProperty = static_cast<EMaterialProperty>(EnumValue);
            return true;
        }
    }

    FString Normalized = PropertyName.ToLower();
    Normalized.ReplaceInline(TEXT("mp_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));

    static const TMap<FString, EMaterialProperty> CommonNames =
    {
        { TEXT("emissive"), MP_EmissiveColor },
        { TEXT("emissivecolor"), MP_EmissiveColor },
        { TEXT("opacity"), MP_Opacity },
        { TEXT("opacitymask"), MP_OpacityMask },
        { TEXT("basecolor"), MP_BaseColor },
        { TEXT("diffuse"), MP_BaseColor },
        { TEXT("albedo"), MP_BaseColor },
        { TEXT("metallic"), MP_Metallic },
        { TEXT("specular"), MP_Specular },
        { TEXT("roughness"), MP_Roughness },
        { TEXT("anisotropy"), MP_Anisotropy },
        { TEXT("normal"), MP_Normal },
        { TEXT("tangent"), MP_Tangent },
        { TEXT("worldpositionoffset"), MP_WorldPositionOffset },
        { TEXT("wpo"), MP_WorldPositionOffset },
        { TEXT("subsurface"), MP_SubsurfaceColor },
        { TEXT("subsurfacecolor"), MP_SubsurfaceColor },
        { TEXT("clearcoat"), MP_CustomData0 },
        { TEXT("clearcoatamount"), MP_CustomData0 },
        { TEXT("clearcoatroughness"), MP_CustomData1 },
        { TEXT("customdata0"), MP_CustomData0 },
        { TEXT("customdata1"), MP_CustomData1 },
        { TEXT("ambientocclusion"), MP_AmbientOcclusion },
        { TEXT("ao"), MP_AmbientOcclusion },
        { TEXT("refraction"), MP_Refraction },
        { TEXT("pixeldepthoffset"), MP_PixelDepthOffset },
        { TEXT("pdo"), MP_PixelDepthOffset },
        { TEXT("frontmaterial"), MP_FrontMaterial },
        { TEXT("surfacethickness"), MP_SurfaceThickness },
        { TEXT("displacement"), MP_Displacement },
        { TEXT("materialattributes"), MP_MaterialAttributes },
    };

    if (const EMaterialProperty* FoundProperty = CommonNames.Find(Normalized))
    {
        OutProperty = *FoundProperty;
        return true;
    }

    OutError = FString::Printf(TEXT("Unknown material property '%s'. Use names like BaseColor, Roughness, Normal, or MP_BaseColor."), *PropertyName);
    return false;
}

TSharedPtr<FJsonObject> LinearColorToJson(const FLinearColor& Color)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("r"), Color.R);
    Object->SetNumberField(TEXT("g"), Color.G);
    Object->SetNumberField(TEXT("b"), Color.B);
    Object->SetNumberField(TEXT("a"), Color.A);
    return Object;
}

TSharedPtr<FJsonObject> Vector4dToJson(const FVector4d& Value)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Value.X);
    Object->SetNumberField(TEXT("y"), Value.Y);
    Object->SetNumberField(TEXT("z"), Value.Z);
    Object->SetNumberField(TEXT("w"), Value.W);
    return Object;
}

TSharedPtr<FJsonObject> ParameterInfoToJson(const FMaterialParameterInfo& Info)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    UEnum* AssociationEnum = StaticEnum<EMaterialParameterAssociation>();
    Object->SetStringField(TEXT("name"), Info.Name.ToString());
    Object->SetStringField(TEXT("association"), EnumNameOrValue(AssociationEnum, static_cast<int64>(Info.Association.GetValue())));
    Object->SetNumberField(TEXT("index"), Info.Index);
    Object->SetStringField(TEXT("key"), Info.ToString());
    return Object;
}

TArray<TSharedPtr<FJsonValue>> ShadingModelsToJson(FMaterialShadingModelField ShadingModels)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    UEnum* ShadingModelEnum = StaticEnum<EMaterialShadingModel>();
    for (int32 Index = 0; Index < MSM_NUM; ++Index)
    {
        const EMaterialShadingModel ShadingModel = static_cast<EMaterialShadingModel>(Index);
        if (!ShadingModels.HasShadingModel(ShadingModel))
        {
            continue;
        }

        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("name"), EnumNameOrValue(ShadingModelEnum, Index));
        Object->SetNumberField(TEXT("value"), Index);
        Values.Add(MakeShared<FJsonValueObject>(Object));
    }
    return Values;
}

TSharedPtr<FJsonObject> MaterialSettingsToJson(UMaterialInterface* MaterialInterface, const UMaterial* BaseMaterial)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!MaterialInterface && !BaseMaterial)
    {
        return Object;
    }

    UEnum* DomainEnum = StaticEnum<EMaterialDomain>();
    UEnum* BlendEnum = StaticEnum<EBlendMode>();
    const UMaterial* EffectiveBase = BaseMaterial ? BaseMaterial : (MaterialInterface ? MaterialInterface->GetMaterial() : nullptr);
    const FMaterialShadingModelField ShadingModels = MaterialInterface ? MaterialInterface->GetShadingModels() : (EffectiveBase ? EffectiveBase->GetShadingModels() : FMaterialShadingModelField());
    const EBlendMode BlendMode = MaterialInterface ? MaterialInterface->GetBlendMode() : (EffectiveBase ? EffectiveBase->BlendMode.GetValue() : BLEND_Opaque);
    const EMaterialDomain Domain = EffectiveBase ? EffectiveBase->MaterialDomain.GetValue() : MD_Surface;

    Object->SetStringField(TEXT("domain"), EnumNameOrValue(DomainEnum, static_cast<int64>(Domain)));
    Object->SetStringField(TEXT("domain_display"), MaterialDomainString(Domain));
    Object->SetStringField(TEXT("blend_mode"), EnumNameOrValue(BlendEnum, static_cast<int64>(BlendMode)));
    Object->SetNumberField(TEXT("blend_mode_value"), static_cast<int32>(BlendMode));
    Object->SetArrayField(TEXT("shading_models"), ShadingModelsToJson(ShadingModels));
    Object->SetNumberField(TEXT("shading_model_field"), ShadingModels.GetShadingModelField());
    Object->SetBoolField(TEXT("two_sided"), MaterialInterface ? MaterialInterface->IsTwoSided() : (EffectiveBase ? EffectiveBase->TwoSided != 0 : false));
    Object->SetBoolField(TEXT("use_material_attributes"), EffectiveBase ? EffectiveBase->bUseMaterialAttributes != 0 : false);
    Object->SetBoolField(TEXT("thin_surface"), EffectiveBase ? EffectiveBase->bIsThinSurface != 0 : false);
    Object->SetBoolField(TEXT("substrate_detected"), ShadingModels.HasShadingModel(MSM_Strata));
    Object->SetStringField(TEXT("base_material"), ObjectPathOrEmpty(EffectiveBase));
    return Object;
}

bool HasShadingModel(FMaterialShadingModelField ShadingModels, EMaterialShadingModel ShadingModel)
{
    return ShadingModels.HasShadingModel(ShadingModel);
}

void ResolveMaterialPropertySemantic(
    EMaterialProperty Property,
    EMaterialDomain Domain,
    EBlendMode BlendMode,
    FMaterialShadingModelField ShadingModels,
    FString& OutSemanticName,
    FString& OutSemanticRole,
    FString& OutShaderHint)
{
    UEnum* MaterialPropertyEnum = StaticEnum<EMaterialProperty>();
    OutSemanticName = MaterialPropertyEnum ? MaterialPropertyEnum->GetNameStringByValue(static_cast<int64>(Property)) : FString::FromInt(static_cast<int32>(Property));
    OutSemanticRole = TEXT("standard_material_property");
    OutShaderHint.Reset();

    if (Domain == MD_LightFunction)
    {
        if (Property == MP_EmissiveColor)
        {
            OutSemanticName = TEXT("LightFunctionIntensity");
            OutSemanticRole = TEXT("light_function_output");
        }
        return;
    }
    if (Domain == MD_PostProcess)
    {
        if (Property == MP_EmissiveColor)
        {
            OutSemanticName = TEXT("PostProcessColor");
            OutSemanticRole = TEXT("post_process_output");
        }
        else if (Property == MP_Opacity)
        {
            OutSemanticName = TEXT("PostProcessOpacity");
            OutSemanticRole = TEXT("post_process_blend_weight");
        }
        return;
    }
    if (Domain == MD_Volume)
    {
        if (Property == MP_SubsurfaceColor)
        {
            OutSemanticName = TEXT("VolumeScatteringOrExtinction");
            OutSemanticRole = TEXT("volume_material_input");
            OutShaderHint = TEXT("Volume domain uses a reduced property set; do not interpret this as surface subsurface shading.");
        }
        return;
    }
    if (Domain == MD_UI)
    {
        if (Property == MP_EmissiveColor)
        {
            OutSemanticName = TEXT("UIFinalColor");
            OutSemanticRole = TEXT("ui_color_output");
        }
        return;
    }
    if (Domain == MD_DeferredDecal)
    {
        OutSemanticRole = TEXT("deferred_decal_property");
        if (Property == MP_Opacity)
        {
            OutSemanticName = TEXT("DecalOpacity");
        }
        return;
    }

    if (Property == MP_CustomData0)
    {
        if (HasShadingModel(ShadingModels, MSM_ClearCoat))
        {
            OutSemanticName = TEXT("ClearCoat");
            OutSemanticRole = TEXT("clear_coat_lobe_weight");
            OutShaderHint = TEXT("ClearCoatCommon.ush / ShadingModels.ush read this through GBuffer.CustomData.x.");
        }
        else if (HasShadingModel(ShadingModels, MSM_Hair))
        {
            OutSemanticName = TEXT("HairBacklit");
            OutSemanticRole = TEXT("hair_custom_data");
        }
        else if (HasShadingModel(ShadingModels, MSM_Cloth))
        {
            OutSemanticName = TEXT("ClothAmount");
            OutSemanticRole = TEXT("cloth_custom_data");
        }
        else if (HasShadingModel(ShadingModels, MSM_Eye))
        {
            OutSemanticName = TEXT("IrisMask");
            OutSemanticRole = TEXT("eye_custom_data");
        }
        else if (HasShadingModel(ShadingModels, MSM_SubsurfaceProfile))
        {
            OutSemanticName = TEXT("SubsurfaceProfileOpacity");
            OutSemanticRole = TEXT("subsurface_profile_custom_data");
        }
        else
        {
            OutSemanticRole = TEXT("shading_model_custom_data");
        }
    }
    else if (Property == MP_CustomData1)
    {
        if (HasShadingModel(ShadingModels, MSM_ClearCoat))
        {
            OutSemanticName = TEXT("ClearCoatRoughness");
            OutSemanticRole = TEXT("clear_coat_lobe_roughness");
            OutShaderHint = TEXT("ClearCoatCommon.ush / ShadingModels.ush read this through GBuffer.CustomData.y.");
        }
        else if (HasShadingModel(ShadingModels, MSM_Eye))
        {
            OutSemanticName = TEXT("IrisDistance");
            OutSemanticRole = TEXT("eye_custom_data");
        }
        else
        {
            OutSemanticRole = TEXT("shading_model_custom_data");
        }
    }
    else if (Property == MP_SubsurfaceColor)
    {
        if (HasShadingModel(ShadingModels, MSM_TwoSidedFoliage))
        {
            OutSemanticName = TEXT("TwoSidedFoliageTransmissionColor");
            OutSemanticRole = TEXT("foliage_transmission");
        }
        else if (HasShadingModel(ShadingModels, MSM_Cloth))
        {
            OutSemanticName = TEXT("ClothFuzzColor");
            OutSemanticRole = TEXT("cloth_subsurface_fuzz");
        }
        else if (HasShadingModel(ShadingModels, MSM_Subsurface) || HasShadingModel(ShadingModels, MSM_PreintegratedSkin))
        {
            OutSemanticName = TEXT("SubsurfaceColor");
            OutSemanticRole = TEXT("subsurface_scattering_color");
        }
    }
    else if (Property == MP_Opacity)
    {
        if (HasShadingModel(ShadingModels, MSM_SingleLayerWater))
        {
            OutSemanticName = TEXT("SingleLayerWaterOpacity");
            OutSemanticRole = TEXT("water_volume_absorption_hint");
        }
        else if (BlendMode != BLEND_Opaque && BlendMode != BLEND_Masked)
        {
            OutSemanticName = TEXT("TranslucencyOpacity");
            OutSemanticRole = TEXT("translucent_blend_alpha");
        }
    }
    else if (Property == MP_MaterialAttributes)
    {
        OutSemanticName = TEXT("MaterialAttributesProxy");
        OutSemanticRole = TEXT("packed_material_attributes");
        OutShaderHint = TEXT("Root property is a packed proxy; expand Make/BreakMaterialAttributes or MaterialFunctionCall to inspect individual channels.");
    }
    else if (Property == MP_FrontMaterial)
    {
        OutSemanticName = TEXT("SubstrateFrontMaterial");
        OutSemanticRole = TEXT("substrate_material_tree");
    }
}

TArray<TSharedPtr<FJsonValue>> SemanticPropertiesToJson(UMaterial* Material, UMaterialInterface* DerivedMaterial)
{
    TArray<TSharedPtr<FJsonValue>> Properties;
    if (!Material)
    {
        return Properties;
    }

    UEnum* MaterialPropertyEnum = StaticEnum<EMaterialProperty>();
    const UMaterialInterface* ActiveMaterial = DerivedMaterial ? DerivedMaterial : Material;
    const EMaterialDomain Domain = Material->MaterialDomain.GetValue();
    const EBlendMode BlendMode = ActiveMaterial ? ActiveMaterial->GetBlendMode() : Material->BlendMode.GetValue();
    const FMaterialShadingModelField ShadingModels = ActiveMaterial ? ActiveMaterial->GetShadingModels() : Material->GetShadingModels();

    for (int32 PropertyIndex = 0; PropertyIndex < static_cast<int32>(MP_MAX); ++PropertyIndex)
    {
        const EMaterialProperty Property = static_cast<EMaterialProperty>(PropertyIndex);
        FExpressionInput* Input = Material->GetExpressionInputForProperty(Property);
        const bool bActive = ActiveMaterial ? Material->IsPropertyActiveInDerived(Property, ActiveMaterial) : Material->IsPropertyActiveInEditor(Property);
        const bool bConnected = Input && Input->Expression;
        const bool bInteresting =
            Property == MP_CustomData0 ||
            Property == MP_CustomData1 ||
            Property == MP_SubsurfaceColor ||
            Property == MP_Opacity ||
            Property == MP_MaterialAttributes ||
            Property == MP_FrontMaterial;

        if (!Input && !bActive && !bInteresting)
        {
            continue;
        }

        FString SemanticName;
        FString SemanticRole;
        FString ShaderHint;
        ResolveMaterialPropertySemantic(Property, Domain, BlendMode, ShadingModels, SemanticName, SemanticRole, ShaderHint);

        TSharedPtr<FJsonObject> Object = MaterialPropertyConnectionToJson(Material, Property);
        Object->SetBoolField(TEXT("active"), bActive);
        Object->SetBoolField(TEXT("connected"), bConnected);
        Object->SetStringField(TEXT("semantic_name"), SemanticName);
        Object->SetStringField(TEXT("semantic_role"), SemanticRole);
        Object->SetStringField(TEXT("shader_hint"), ShaderHint);
        Object->SetStringField(TEXT("property_enum"), MaterialPropertyEnum ? MaterialPropertyEnum->GetNameStringByValue(static_cast<int64>(Property)) : FString::FromInt(PropertyIndex));
        Properties.Add(MakeShared<FJsonValueObject>(Object));
    }

    return Properties;
}

TSharedPtr<FJsonObject> CostHintsToJson(TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    TSet<FString> UniqueTextures;
    int32 TextureSampleCount = 0;
    int32 TextureObjectCount = 0;
    int32 UniqueSamplerEstimate = 0;
    int32 SharedSamplerCount = 0;
    int32 FunctionCallCount = 0;
    int32 StaticSwitchCount = 0;
    int32 RerouteCount = 0;
    int32 CustomHlslCount = 0;

    TArray<TSharedPtr<FJsonValue>> TextureNodes;
    UEnum* SamplerSourceEnum = StaticEnum<ESamplerSourceMode>();
    UEnum* SamplerTypeEnum = StaticEnum<EMaterialSamplerType>();

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Expressions)
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (!Expression)
        {
            continue;
        }

        if (Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
        {
            ++FunctionCallCount;
        }
        if (Cast<UMaterialExpressionStaticBoolParameter>(Expression) || Expression->GetClass()->GetName().Contains(TEXT("StaticSwitch")))
        {
            ++StaticSwitchCount;
        }
        if (Cast<UMaterialExpressionRerouteBase>(Expression))
        {
            ++RerouteCount;
        }
        if (Expression->GetClass()->GetName().Contains(TEXT("MaterialExpressionCustom")))
        {
            ++CustomHlslCount;
        }

        if (!Expression->CanReferenceTexture())
        {
            continue;
        }

        const bool bIsTextureSample = Cast<UMaterialExpressionTextureSample>(Expression) != nullptr;
        TextureSampleCount += bIsTextureSample ? 1 : 0;
        TextureObjectCount += bIsTextureSample ? 0 : 1;

        TSharedPtr<FJsonObject> TextureObject = MakeShared<FJsonObject>();
        AddExpressionReferenceFields(TextureObject, Expression, TEXT(""));
        TextureObject->SetBoolField(TEXT("is_texture_sample"), bIsTextureSample);

        TArray<TSharedPtr<FJsonValue>> References;
        for (UObject* ReferencedObject : Expression->GetReferencedTextures())
        {
            if (!ReferencedObject)
            {
                continue;
            }
            UniqueTextures.Add(ReferencedObject->GetPathName());
            References.Add(MakeShared<FJsonValueString>(ReferencedObject->GetPathName()));
        }
        TextureObject->SetArrayField(TEXT("textures"), References);

        if (UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression))
        {
            const ESamplerSourceMode SamplerSource = TextureSample->SamplerSource.GetValue();
            TextureObject->SetStringField(TEXT("sampler_source"), EnumNameOrValue(SamplerSourceEnum, static_cast<int64>(SamplerSource)));
            TextureObject->SetStringField(TEXT("sampler_type"), EnumNameOrValue(SamplerTypeEnum, static_cast<int64>(TextureSample->SamplerType.GetValue())));
            if (SamplerSource == SSM_FromTextureAsset)
            {
                ++UniqueSamplerEstimate;
            }
            else
            {
                ++SharedSamplerCount;
            }
        }
        TextureNodes.Add(MakeShared<FJsonValueObject>(TextureObject));
    }

    Object->SetNumberField(TEXT("texture_sample_count"), TextureSampleCount);
    Object->SetNumberField(TEXT("texture_object_or_parameter_count"), TextureObjectCount);
    Object->SetNumberField(TEXT("unique_texture_count"), UniqueTextures.Num());
    Object->SetNumberField(TEXT("unique_sampler_estimate"), UniqueSamplerEstimate);
    Object->SetNumberField(TEXT("shared_sampler_count"), SharedSamplerCount);
    Object->SetNumberField(TEXT("material_function_call_count"), FunctionCallCount);
    Object->SetNumberField(TEXT("static_switch_count"), StaticSwitchCount);
    Object->SetNumberField(TEXT("reroute_count"), RerouteCount);
    Object->SetNumberField(TEXT("custom_hlsl_count"), CustomHlslCount);
    Object->SetStringField(TEXT("cost_model"), TEXT("graph_hint_only"));
    Object->SetArrayField(TEXT("texture_nodes"), TextureNodes);
    return Object;
}

TSharedPtr<FJsonObject> UsageHintsToJson(UObject* Asset, int32 MaxReferencers)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!Asset)
    {
        return Object;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FName> ReferencerPackages;
    AssetRegistryModule.Get().GetReferencers(FName(*Asset->GetOutermost()->GetName()), ReferencerPackages);

    Object->SetNumberField(TEXT("referencer_package_count"), ReferencerPackages.Num());
    Object->SetStringField(TEXT("source"), TEXT("AssetRegistry package referencers"));

    TArray<TSharedPtr<FJsonValue>> Referencers;
    const int32 Limit = MaxReferencers > 0 ? FMath::Min(MaxReferencers, ReferencerPackages.Num()) : ReferencerPackages.Num();
    for (int32 Index = 0; Index < Limit; ++Index)
    {
        TSharedPtr<FJsonObject> ReferencerObject = MakeShared<FJsonObject>();
        ReferencerObject->SetStringField(TEXT("package"), ReferencerPackages[Index].ToString());

        TArray<FAssetData> AssetsInPackage;
        AssetRegistryModule.Get().GetAssetsByPackageName(ReferencerPackages[Index], AssetsInPackage);
        TArray<TSharedPtr<FJsonValue>> Assets;
        for (const FAssetData& AssetData : AssetsInPackage)
        {
            TSharedPtr<FJsonObject> AssetObject = MakeShared<FJsonObject>();
            AssetObject->SetStringField(TEXT("object_path"), AssetData.GetObjectPathString());
            AssetObject->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());
            AssetObject->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
            Assets.Add(MakeShared<FJsonValueObject>(AssetObject));
        }
        ReferencerObject->SetArrayField(TEXT("assets"), Assets);
        Referencers.Add(MakeShared<FJsonValueObject>(ReferencerObject));
    }
    Object->SetArrayField(TEXT("referencers"), Referencers);
    Object->SetBoolField(TEXT("truncated"), Limit < ReferencerPackages.Num());
    return Object;
}

TSharedPtr<FJsonObject> MaterialInstanceToJson(UMaterialInstance* Instance)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!Instance)
    {
        return Object;
    }

    Object->SetStringField(TEXT("parent"), ObjectPathOrEmpty(Instance->Parent));

    TArray<TSharedPtr<FJsonValue>> ParentChain;
    UMaterialInterface* Current = Instance;
    TSet<UMaterialInterface*> Visited;
    while (UMaterialInstance* CurrentInstance = Cast<UMaterialInstance>(Current))
    {
        if (Visited.Contains(CurrentInstance))
        {
            TSharedPtr<FJsonObject> LoopObject = MakeShared<FJsonObject>();
            LoopObject->SetStringField(TEXT("asset"), CurrentInstance->GetPathName());
            LoopObject->SetBoolField(TEXT("cycle_detected"), true);
            ParentChain.Add(MakeShared<FJsonValueObject>(LoopObject));
            break;
        }
        Visited.Add(CurrentInstance);

        TSharedPtr<FJsonObject> LinkObject = MakeShared<FJsonObject>();
        LinkObject->SetStringField(TEXT("asset"), CurrentInstance->GetPathName());
        LinkObject->SetStringField(TEXT("class"), CurrentInstance->GetClass()->GetName());
        LinkObject->SetStringField(TEXT("parent"), ObjectPathOrEmpty(CurrentInstance->Parent));
        ParentChain.Add(MakeShared<FJsonValueObject>(LinkObject));
        Current = CurrentInstance->Parent;
    }
    if (Current)
    {
        TSharedPtr<FJsonObject> BaseObject = MakeShared<FJsonObject>();
        BaseObject->SetStringField(TEXT("asset"), Current->GetPathName());
        BaseObject->SetStringField(TEXT("class"), Current->GetClass()->GetName());
        BaseObject->SetBoolField(TEXT("base"), true);
        ParentChain.Add(MakeShared<FJsonValueObject>(BaseObject));
    }
    Object->SetArrayField(TEXT("parent_chain"), ParentChain);

    TArray<TSharedPtr<FJsonValue>> Scalars;
    for (const FScalarParameterValue& Parameter : Instance->ScalarParameterValues)
    {
        TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
        ParameterObject->SetObjectField(TEXT("parameter"), ParameterInfoToJson(Parameter.ParameterInfo));
        ParameterObject->SetNumberField(TEXT("value"), Parameter.ParameterValue);
        ParameterObject->SetStringField(TEXT("expression_guid"), Parameter.ExpressionGUID.ToString());
        Scalars.Add(MakeShared<FJsonValueObject>(ParameterObject));
    }
    Object->SetArrayField(TEXT("scalar_overrides"), Scalars);

    TArray<TSharedPtr<FJsonValue>> Vectors;
    for (const FVectorParameterValue& Parameter : Instance->VectorParameterValues)
    {
        TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
        ParameterObject->SetObjectField(TEXT("parameter"), ParameterInfoToJson(Parameter.ParameterInfo));
        ParameterObject->SetObjectField(TEXT("value"), LinearColorToJson(Parameter.ParameterValue));
        ParameterObject->SetStringField(TEXT("expression_guid"), Parameter.ExpressionGUID.ToString());
        Vectors.Add(MakeShared<FJsonValueObject>(ParameterObject));
    }
    Object->SetArrayField(TEXT("vector_overrides"), Vectors);

    TArray<TSharedPtr<FJsonValue>> DoubleVectors;
    for (const FDoubleVectorParameterValue& Parameter : Instance->DoubleVectorParameterValues)
    {
        TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
        ParameterObject->SetObjectField(TEXT("parameter"), ParameterInfoToJson(Parameter.ParameterInfo));
        ParameterObject->SetObjectField(TEXT("value"), Vector4dToJson(Parameter.ParameterValue));
        ParameterObject->SetStringField(TEXT("expression_guid"), Parameter.ExpressionGUID.ToString());
        DoubleVectors.Add(MakeShared<FJsonValueObject>(ParameterObject));
    }
    Object->SetArrayField(TEXT("double_vector_overrides"), DoubleVectors);

    TArray<TSharedPtr<FJsonValue>> Textures;
    for (const FTextureParameterValue& Parameter : Instance->TextureParameterValues)
    {
        TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
        ParameterObject->SetObjectField(TEXT("parameter"), ParameterInfoToJson(Parameter.ParameterInfo));
        ParameterObject->SetStringField(TEXT("value"), ObjectPathOrEmpty(Parameter.ParameterValue));
        ParameterObject->SetStringField(TEXT("expression_guid"), Parameter.ExpressionGUID.ToString());
        Textures.Add(MakeShared<FJsonValueObject>(ParameterObject));
    }
    Object->SetArrayField(TEXT("texture_overrides"), Textures);

    TArray<TSharedPtr<FJsonValue>> StaticSwitches;
    TArray<FMaterialParameterInfo> StaticSwitchInfos;
    TArray<FGuid> StaticSwitchIds;
    Instance->GetAllStaticSwitchParameterInfo(StaticSwitchInfos, StaticSwitchIds);
    for (int32 Index = 0; Index < StaticSwitchInfos.Num(); ++Index)
    {
        bool bValue = false;
        FGuid ExpressionGuid;
        const bool bHasValue = Instance->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(StaticSwitchInfos[Index]), bValue, ExpressionGuid, false);
        bool bOverrideValue = false;
        FGuid OverrideGuid;
        const bool bOverridden = Instance->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(StaticSwitchInfos[Index]), bOverrideValue, OverrideGuid, true);

        TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
        ParameterObject->SetObjectField(TEXT("parameter"), ParameterInfoToJson(StaticSwitchInfos[Index]));
        ParameterObject->SetStringField(TEXT("declared_guid"), StaticSwitchIds.IsValidIndex(Index) ? StaticSwitchIds[Index].ToString() : FString());
        ParameterObject->SetBoolField(TEXT("resolved"), bHasValue);
        ParameterObject->SetBoolField(TEXT("value"), bValue);
        ParameterObject->SetBoolField(TEXT("overridden"), bOverridden);
        if (bOverridden)
        {
            ParameterObject->SetBoolField(TEXT("override_value"), bOverrideValue);
            ParameterObject->SetStringField(TEXT("override_expression_guid"), OverrideGuid.ToString());
        }
        else
        {
            ParameterObject->SetStringField(TEXT("expression_guid"), ExpressionGuid.ToString());
        }
        StaticSwitches.Add(MakeShared<FJsonValueObject>(ParameterObject));
    }
    Object->SetArrayField(TEXT("static_switches"), StaticSwitches);

    Object->SetNumberField(TEXT("scalar_override_count"), Scalars.Num());
    Object->SetNumberField(TEXT("vector_override_count"), Vectors.Num());
    Object->SetNumberField(TEXT("texture_override_count"), Textures.Num());
    Object->SetNumberField(TEXT("static_switch_count"), StaticSwitches.Num());
    return Object;
}

TSharedPtr<FJsonObject> GraphSummaryToJson(const FMaterialGraphTarget& Target, bool bIncludeNodes)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    Object->SetNumberField(TEXT("node_count"), Target.GetExpressions().Num());
    Object->SetObjectField(TEXT("cost_hints"), CostHintsToJson(Target.GetExpressions()));

    TArray<TSharedPtr<FJsonValue>> FunctionCalls;
    TArray<TSharedPtr<FJsonValue>> Reroutes;
    TArray<TSharedPtr<FJsonValue>> CustomOutputs;
    TArray<TSharedPtr<FJsonValue>> CustomHlslNodes;
    TArray<TSharedPtr<FJsonValue>> StaticSwitches;
    TArray<TSharedPtr<FJsonValue>> Nodes;

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Target.GetExpressions())
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (!Expression)
        {
            continue;
        }

        if (bIncludeNodes)
        {
            Nodes.Add(MakeShared<FJsonValueObject>(ExpressionToJson(Expression, true, Target.GetExpressions())));
        }
        if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
        {
            TSharedPtr<FJsonObject> ObjectValue = MakeShared<FJsonObject>();
            AddExpressionReferenceFields(ObjectValue, Expression, TEXT(""));
            ObjectValue->SetObjectField(TEXT("function_call"), FunctionCallToJson(FunctionCall));
            FunctionCalls.Add(MakeShared<FJsonValueObject>(ObjectValue));
        }
        if (UMaterialExpressionRerouteBase* Reroute = Cast<UMaterialExpressionRerouteBase>(Expression))
        {
            TSharedPtr<FJsonObject> ObjectValue = MakeShared<FJsonObject>();
            AddExpressionReferenceFields(ObjectValue, Expression, TEXT(""));
            ObjectValue->SetObjectField(TEXT("reroute"), RerouteToJson(Reroute, Target.GetExpressions()));
            Reroutes.Add(MakeShared<FJsonValueObject>(ObjectValue));
        }
        if (Expression->GetClass()->GetName().Contains(TEXT("CustomOutput")))
        {
            TSharedPtr<FJsonObject> ObjectValue = MakeShared<FJsonObject>();
            AddExpressionReferenceFields(ObjectValue, Expression, TEXT(""));
            ObjectValue->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
            CustomOutputs.Add(MakeShared<FJsonValueObject>(ObjectValue));
        }
        if (Expression->GetClass()->GetName().Contains(TEXT("MaterialExpressionCustom")))
        {
            TSharedPtr<FJsonObject> ObjectValue = MakeShared<FJsonObject>();
            AddExpressionReferenceFields(ObjectValue, Expression, TEXT(""));
            ObjectValue->SetObjectField(TEXT("custom_hlsl"), CustomHlslToJson(Expression));
            CustomHlslNodes.Add(MakeShared<FJsonValueObject>(ObjectValue));
        }
        if (Cast<UMaterialExpressionStaticBoolParameter>(Expression) || Expression->GetClass()->GetName().Contains(TEXT("StaticSwitch")))
        {
            TSharedPtr<FJsonObject> ObjectValue = MakeShared<FJsonObject>();
            AddExpressionReferenceFields(ObjectValue, Expression, TEXT(""));
            ObjectValue->SetObjectField(TEXT("static_switch"), StaticSwitchToJson(Expression));
            StaticSwitches.Add(MakeShared<FJsonValueObject>(ObjectValue));
        }
    }

    Object->SetArrayField(TEXT("function_calls"), FunctionCalls);
    Object->SetArrayField(TEXT("reroutes"), Reroutes);
    Object->SetArrayField(TEXT("custom_outputs"), CustomOutputs);
    Object->SetArrayField(TEXT("custom_hlsl_nodes"), CustomHlslNodes);
    Object->SetArrayField(TEXT("static_switches"), StaticSwitches);
    if (bIncludeNodes)
    {
        Object->SetArrayField(TEXT("nodes"), Nodes);
    }
    return Object;
}

UWorld* GetEditorOrPIEWorld()
{
    if (!GEditor)
    {
        return nullptr;
    }

    for (const FWorldContext& WorldContext : GEditor->GetWorldContexts())
    {
        if (WorldContext.World() && WorldContext.WorldType == EWorldType::PIE)
        {
            return WorldContext.World();
        }
    }

    return GEditor->GetEditorWorldContext().World();
}

FString GetWorldTypeName(const UWorld* World)
{
    if (!World)
    {
        return TEXT("");
    }

    switch (World->WorldType)
    {
    case EWorldType::Editor:
        return TEXT("Editor");
    case EWorldType::PIE:
        return TEXT("PIE");
    case EWorldType::Game:
        return TEXT("Game");
    case EWorldType::GamePreview:
        return TEXT("GamePreview");
    case EWorldType::EditorPreview:
        return TEXT("EditorPreview");
    case EWorldType::Inactive:
        return TEXT("Inactive");
    default:
        return TEXT("Unknown");
    }
}

bool ShouldIncludeNamedParameter(const TSet<FName>& ParameterFilter, const FName& ParameterName)
{
    return ParameterFilter.Num() == 0 || ParameterFilter.Contains(ParameterName);
}

bool GetOptionalParameterNameFilter(const TSharedPtr<FJsonObject>& Params, TSet<FName>& ParameterFilter, FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* ParameterNames = nullptr;
    if (!Params->TryGetArrayField(TEXT("parameter_names"), ParameterNames))
    {
        return true;
    }

    // Validate the entire request before constructing names. Never discard an
    // invalid filter item: an empty filter means all parameters to this API.
    for (const TSharedPtr<FJsonValue>& Value : *ParameterNames)
    {
        if (!Value.IsValid() || Value->Type != EJson::String)
        {
            Error = TEXT("Collection parameter filter entries must be strings."); return false;
        }
        if (Value->AsString().TrimStartAndEnd().Len() >= NAME_SIZE)
        {
            Error = TEXT("Collection parameter filter names must be shorter than NAME_SIZE (1024 UTF-16 code units); rejected before FName construction."); return false;
        }
    }
    for (const TSharedPtr<FJsonValue>& Value : *ParameterNames)
    {
        const FString Name = Value->AsString().TrimStartAndEnd();
        if (!Name.IsEmpty())
        {
            ParameterFilter.Add(FName(*Name));
        }
    }

    return true;
}

TSharedPtr<FJsonObject> HandleResolveMaterialGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return MaterialApiError(TEXT("Missing 'material_path' parameter"));
    }

    FString GraphType;
    Params->TryGetStringField(TEXT("graph_type"), GraphType);

    TArray<FString> CandidatePaths;
    FMaterialGraphTarget Target;
    FString ErrorMessage;
    const bool bResolved = ResolveMaterialGraph(MaterialPath, GraphType, Target, &CandidatePaths, ErrorMessage);

    TArray<TSharedPtr<FJsonValue>> CandidateArray;
    for (const FString& CandidatePath : CandidatePaths)
    {
        CandidateArray.Add(MakeShared<FJsonValueString>(CandidatePath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("resolved"), bResolved);
    ResultObj->SetArrayField(TEXT("candidates"), CandidateArray);
    if (bResolved)
    {
        ResultObj->SetStringField(TEXT("asset_path"), Target.GetPathName());
        ResultObj->SetStringField(TEXT("name"), Target.Asset ? Target.Asset->GetName() : FString());
        ResultObj->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    }
    else
    {
        ResultObj->SetStringField(TEXT("error"), ErrorMessage);
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> HandleListMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return MaterialApiError(TEXT("Missing 'material_path' parameter"));
    }

    FString GraphType;
    Params->TryGetStringField(TEXT("graph_type"), GraphType);

    FString NodeType;
    Params->TryGetStringField(TEXT("node_type"), NodeType);

    FString DescContains;
    Params->TryGetStringField(TEXT("desc_contains"), DescContains);

    bool bIncludePins = true;
    if (Params->HasField(TEXT("include_pins")))
    {
        bIncludePins = Params->GetBoolField(TEXT("include_pins"));
    }

    FMaterialGraphTarget Target;
    FString ErrorMessage;
    if (!ResolveMaterialGraph(MaterialPath, GraphType, Target, nullptr, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Target.GetExpressions())
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (!Expression)
        {
            continue;
        }

        if (!NodeType.IsEmpty() &&
            !Expression->GetClass()->GetName().Contains(NodeType) &&
            !Expression->GetName().Contains(NodeType))
        {
            continue;
        }
        if (!DescContains.IsEmpty() && !Expression->Desc.Contains(DescContains))
        {
            continue;
        }

        Nodes.Add(MakeShared<FJsonValueObject>(ExpressionToJson(Expression, bIncludePins, Target.GetExpressions())));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material"), Target.GetPathName());
    ResultObj->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    ResultObj->SetNumberField(TEXT("node_count"), Nodes.Num());
    ResultObj->SetArrayField(TEXT("nodes"), Nodes);
    ResultObj->SetObjectField(TEXT("cost_hints"), CostHintsToJson(Target.GetExpressions()));

    if (Target.Material)
    {
        ResultObj->SetObjectField(TEXT("material_settings"), MaterialSettingsToJson(Target.Material, Target.Material));
        ResultObj->SetArrayField(TEXT("semantic_properties"), SemanticPropertiesToJson(Target.Material, Target.Material));

        TArray<TSharedPtr<FJsonValue>> PropertyConnections;
        for (int32 PropertyIndex = 0; PropertyIndex < static_cast<int32>(MP_MAX); ++PropertyIndex)
        {
            const EMaterialProperty Property = static_cast<EMaterialProperty>(PropertyIndex);
            if (FExpressionInput* Input = Target.Material->GetExpressionInputForProperty(Property))
            {
                (void)Input;
                PropertyConnections.Add(MakeShared<FJsonValueObject>(MaterialPropertyConnectionToJson(Target.Material, Property)));
            }
        }
        ResultObj->SetArrayField(TEXT("material_properties"), PropertyConnections);
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> HandleAnalyzeMaterialGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return MaterialApiError(TEXT("Missing 'material_path' parameter"));
    }

    FString GraphType;
    Params->TryGetStringField(TEXT("graph_type"), GraphType);

    bool bIncludeNodes = false;
    if (Params->HasField(TEXT("include_nodes")))
    {
        bIncludeNodes = Params->GetBoolField(TEXT("include_nodes"));
    }

    bool bIncludeUsage = true;
    if (Params->HasField(TEXT("include_usage")))
    {
        bIncludeUsage = Params->GetBoolField(TEXT("include_usage"));
    }

    int32 MaxReferencers = 25;
    if (Params->HasField(TEXT("max_referencers")))
    {
        MaxReferencers = static_cast<int32>(Params->GetNumberField(TEXT("max_referencers")));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("analysis_version"), TEXT("material_mcp_analysis_v1"));

    const FString Query = NormalizeObjectPathForLoad(MaterialPath);
    UObject* LoadedObject = LoadObject<UObject>(nullptr, *Query);
    if (!LoadedObject)
    {
        TArray<FString> CandidatePaths = FindMaterialGraphAssetPaths(MaterialPath, GraphType);
        if (NormalizeMaterialGraphType(GraphType) != TEXT("function"))
        {
            AppendAssetCandidatesForClass(UMaterialInstance::StaticClass(), MaterialPath, CandidatePaths);
        }
        if (CandidatePaths.Num() == 1)
        {
            LoadedObject = LoadObject<UObject>(nullptr, *NormalizeObjectPathForLoad(CandidatePaths[0]));
        }
        else if (CandidatePaths.Num() > 1)
        {
            TArray<TSharedPtr<FJsonValue>> CandidateArray;
            for (const FString& CandidatePath : CandidatePaths)
            {
                CandidateArray.Add(MakeShared<FJsonValueString>(CandidatePath));
            }
            TSharedPtr<FJsonObject> ErrorObj = MaterialApiError(
                FString::Printf(TEXT("Ambiguous material analysis target '%s'. Use a full object path."), *MaterialPath));
            ErrorObj->SetArrayField(TEXT("candidates"), CandidateArray);
            return ErrorObj;
        }
    }

    if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(LoadedObject))
    {
        ResultObj->SetStringField(TEXT("asset_path"), MaterialInstance->GetPathName());
        ResultObj->SetStringField(TEXT("asset_class"), MaterialInstance->GetClass()->GetName());
        ResultObj->SetStringField(TEXT("asset_kind"), TEXT("material_instance"));
        UMaterial* BaseMaterial = MaterialInstance->GetMaterial();
        ResultObj->SetObjectField(TEXT("material_settings"), MaterialSettingsToJson(MaterialInstance, BaseMaterial));
        ResultObj->SetObjectField(TEXT("material_instance"), MaterialInstanceToJson(MaterialInstance));
        if (BaseMaterial)
        {
            ResultObj->SetStringField(TEXT("base_material"), BaseMaterial->GetPathName());
            ResultObj->SetArrayField(TEXT("semantic_properties"), SemanticPropertiesToJson(BaseMaterial, MaterialInstance));
            FMaterialGraphTarget BaseTarget;
            BaseTarget.Asset = BaseMaterial;
            BaseTarget.Material = BaseMaterial;
            ResultObj->SetObjectField(TEXT("base_graph"), GraphSummaryToJson(BaseTarget, bIncludeNodes));
        }
        if (bIncludeUsage)
        {
            ResultObj->SetObjectField(TEXT("usage_hints"), UsageHintsToJson(MaterialInstance, MaxReferencers));
        }
        return ResultObj;
    }

    FMaterialGraphTarget Target;
    FString ErrorMessage;
    if (!ResolveMaterialGraph(MaterialPath, GraphType, Target, nullptr, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    ResultObj->SetStringField(TEXT("asset_path"), Target.GetPathName());
    ResultObj->SetStringField(TEXT("asset_class"), Target.Asset ? Target.Asset->GetClass()->GetName() : FString());
    ResultObj->SetStringField(TEXT("asset_kind"), Target.IsFunctionGraph() ? TEXT("material_function") : TEXT("material"));
    ResultObj->SetObjectField(TEXT("graph"), GraphSummaryToJson(Target, bIncludeNodes));

    if (Target.Material)
    {
        ResultObj->SetObjectField(TEXT("material_settings"), MaterialSettingsToJson(Target.Material, Target.Material));
        ResultObj->SetArrayField(TEXT("semantic_properties"), SemanticPropertiesToJson(Target.Material, Target.Material));
    }
    else if (Target.Function)
    {
        ResultObj->SetStringField(TEXT("function_path"), Target.Function->GetPathName());
        ResultObj->SetStringField(TEXT("function_name"), Target.Function->GetName());
        ResultObj->SetStringField(TEXT("semantic_note"), TEXT("Material Function has no root shading model or domain; inspect callers for final property semantics."));
    }

    if (bIncludeUsage)
    {
        ResultObj->SetObjectField(TEXT("usage_hints"), UsageHintsToJson(Target.Asset, MaxReferencers));
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> HandleListMaterialCollectionParameterNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return MaterialApiError(TEXT("Missing 'material_path' parameter"));
    }

    FString GraphType;
    Params->TryGetStringField(TEXT("graph_type"), GraphType);

    FMaterialGraphTarget Target;
    FString ErrorMessage;
    if (!ResolveMaterialGraph(MaterialPath, GraphType, Target, nullptr, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    TArray<TSharedPtr<FJsonValue>> Nodes;
    int32 MismatchedIdCount = 0;
    int32 MissingCollectionParameterCount = 0;

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Target.GetExpressions())
    {
        UMaterialExpressionCollectionParameter* CollectionParameter = Cast<UMaterialExpressionCollectionParameter>(ExpressionPtr.Get());
        if (!CollectionParameter)
        {
            continue;
        }

        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("node_id"), GetExpressionId(CollectionParameter));
        NodeObj->SetStringField(TEXT("node_name"), CollectionParameter->GetName());
        NodeObj->SetStringField(TEXT("object_path"), CollectionParameter->GetPathName());
        NodeObj->SetStringField(TEXT("parameter_name"), CollectionParameter->ParameterName.ToString());
        NodeObj->SetStringField(TEXT("parameter_id"), CollectionParameter->ParameterId.ToString(EGuidFormats::DigitsWithHyphens));
        NodeObj->SetNumberField(TEXT("x"), CollectionParameter->MaterialExpressionEditorX);
        NodeObj->SetNumberField(TEXT("y"), CollectionParameter->MaterialExpressionEditorY);

        UMaterialParameterCollection* Collection = CollectionParameter->Collection;
        NodeObj->SetStringField(TEXT("collection"), Collection ? Collection->GetPathName() : FString());

        bool bFoundName = false;
        bool bIdMatches = false;
        FString CollectionParameterId;
        FString CollectionParameterType;
        const FName ParameterName = CollectionParameter->ParameterName;
        if (Collection)
        {
            for (const FCollectionScalarParameter& ScalarParameter : Collection->ScalarParameters)
            {
                if (ScalarParameter.ParameterName == ParameterName)
                {
                    bFoundName = true;
                    bIdMatches = ScalarParameter.Id == CollectionParameter->ParameterId;
                    CollectionParameterId = ScalarParameter.Id.ToString(EGuidFormats::DigitsWithHyphens);
                    CollectionParameterType = TEXT("scalar");
                    break;
                }
            }

            if (!bFoundName)
            {
                for (const FCollectionVectorParameter& VectorParameter : Collection->VectorParameters)
                {
                    if (VectorParameter.ParameterName == ParameterName)
                    {
                        bFoundName = true;
                        bIdMatches = VectorParameter.Id == CollectionParameter->ParameterId;
                        CollectionParameterId = VectorParameter.Id.ToString(EGuidFormats::DigitsWithHyphens);
                        CollectionParameterType = TEXT("vector");
                        break;
                    }
                }
            }
        }

        if (!bFoundName)
        {
            ++MissingCollectionParameterCount;
        }
        else if (!bIdMatches)
        {
            ++MismatchedIdCount;
        }

        NodeObj->SetBoolField(TEXT("collection_parameter_found"), bFoundName);
        NodeObj->SetBoolField(TEXT("parameter_id_matches_collection"), bIdMatches);
        NodeObj->SetStringField(TEXT("collection_parameter_id"), CollectionParameterId);
        NodeObj->SetStringField(TEXT("collection_parameter_type"), CollectionParameterType);
        Nodes.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material"), Target.GetPathName());
    ResultObj->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    ResultObj->SetNumberField(TEXT("collection_parameter_node_count"), Nodes.Num());
    ResultObj->SetNumberField(TEXT("mismatched_id_count"), MismatchedIdCount);
    ResultObj->SetNumberField(TEXT("missing_collection_parameter_count"), MissingCollectionParameterCount);
    ResultObj->SetArrayField(TEXT("nodes"), Nodes);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    FString FromNodeId;
    FString ToNodeId;
    FString ToInput;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return MaterialApiError(TEXT("Missing 'material_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("from_node_id"), FromNodeId))
    {
        return MaterialApiError(TEXT("Missing 'from_node_id' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("to_node_id"), ToNodeId))
    {
        return MaterialApiError(TEXT("Missing 'to_node_id' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("to_input"), ToInput))
    {
        return MaterialApiError(TEXT("Missing 'to_input' parameter"));
    }

    FString FromOutput;
    Params->TryGetStringField(TEXT("from_output"), FromOutput);

    FString GraphType;
    Params->TryGetStringField(TEXT("graph_type"), GraphType);

    FMaterialGraphTarget Target;
    FString ErrorMessage;
    if (!ResolveMaterialGraph(MaterialPath, GraphType, Target, nullptr, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    UMaterialExpression* FromExpression = nullptr;
    UMaterialExpression* ToExpression = nullptr;
    if (!FindExpressionById(Target, FromNodeId, FromExpression, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }
    if (!FindExpressionById(Target, ToNodeId, ToExpression, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    if (FindOutputIndexByName(FromExpression, FromOutput) == INDEX_NONE)
    {
        return MaterialApiError(FString::Printf(TEXT("Source output not found: '%s'. Available outputs: %s"),
            *FromOutput,
            *FString::Join(GetMaterialExpressionOutputNames(FromExpression), TEXT(", "))));
    }
    if (!HasInputName(ToExpression, ToInput))
    {
        return MaterialApiError(FString::Printf(TEXT("Target input not found: '%s'. Available inputs: %s"),
            *ToInput,
            *FString::Join(GetMaterialExpressionInputNamesSafe(ToExpression), TEXT(", "))));
    }

    const FString CanonicalFromOutput = ResolveOutputConnectName(FromExpression, FromOutput);
    const FString CanonicalToInput = ResolveInputConnectName(ToExpression, ToInput);
    const bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(FromExpression, CanonicalFromOutput, ToExpression, CanonicalToInput);
    if (!bConnected)
    {
        return MaterialApiError(FString::Printf(TEXT("Failed to connect material nodes: %s.%s -> %s.%s"),
            *FromNodeId, *CanonicalFromOutput, *ToNodeId, *CanonicalToInput));
    }

    const bool bTouchesCustomExpression = Cast<UMaterialExpressionCustom>(FromExpression) != nullptr ||
        Cast<UMaterialExpressionCustom>(ToExpression) != nullptr;
    Target.MarkChanged(!bTouchesCustomExpression);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material"), Target.GetPathName());
    ResultObj->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    ResultObj->SetStringField(TEXT("from_node_id"), GetExpressionId(FromExpression));
    ResultObj->SetStringField(TEXT("from_output"), CanonicalFromOutput);
    ResultObj->SetStringField(TEXT("to_node_id"), GetExpressionId(ToExpression));
    ResultObj->SetStringField(TEXT("to_input"), CanonicalToInput);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleConnectMaterialProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    FString FromNodeId;
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return MaterialApiError(TEXT("Missing 'material_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("from_node_id"), FromNodeId))
    {
        return MaterialApiError(TEXT("Missing 'from_node_id' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("property"), PropertyName))
    {
        return MaterialApiError(TEXT("Missing 'property' parameter"));
    }

    FString FromOutput;
    Params->TryGetStringField(TEXT("from_output"), FromOutput);

    FMaterialGraphTarget Target;
    FString ErrorMessage;
    if (!ResolveMaterialGraph(MaterialPath, TEXT("material"), Target, nullptr, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }
    if (!Target.Material)
    {
        return MaterialApiError(TEXT("connect_material_property only supports Material graphs, not Material Functions"));
    }

    UMaterialExpression* FromExpression = nullptr;
    if (!FindExpressionById(Target, FromNodeId, FromExpression, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    EMaterialProperty MaterialProperty = MP_MAX;
    if (!MaterialPropertyFromString(PropertyName, MaterialProperty, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    if (!Target.Material->GetExpressionInputForProperty(MaterialProperty))
    {
        return MaterialApiError(FString::Printf(TEXT("Material property cannot be connected in this material: %s"), *PropertyName));
    }
    if (FindOutputIndexByName(FromExpression, FromOutput) == INDEX_NONE)
    {
        return MaterialApiError(FString::Printf(TEXT("Source output not found: '%s'. Available outputs: %s"),
            *FromOutput,
            *FString::Join(GetMaterialExpressionOutputNames(FromExpression), TEXT(", "))));
    }

    const FString CanonicalFromOutput = ResolveOutputConnectName(FromExpression, FromOutput);
    const bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(FromExpression, CanonicalFromOutput, MaterialProperty);
    if (!bConnected)
    {
        return MaterialApiError(FString::Printf(TEXT("Failed to connect material property: %s.%s -> %s"),
            *FromNodeId, *CanonicalFromOutput, *PropertyName));
    }

    Target.MarkChanged(Cast<UMaterialExpressionCustom>(FromExpression) == nullptr);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material"), Target.GetPathName());
    ResultObj->SetStringField(TEXT("from_node_id"), GetExpressionId(FromExpression));
    ResultObj->SetStringField(TEXT("from_output"), CanonicalFromOutput);
    ResultObj->SetObjectField(TEXT("property_connection"), MaterialPropertyConnectionToJson(Target.Material, MaterialProperty));
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleDeleteMaterialNode(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    FString NodeId;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return MaterialApiError(TEXT("Missing 'material_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return MaterialApiError(TEXT("Missing 'node_id' parameter"));
    }

    FString GraphType;
    Params->TryGetStringField(TEXT("graph_type"), GraphType);

    FMaterialGraphTarget Target;
    FString ErrorMessage;
    if (!ResolveMaterialGraph(MaterialPath, GraphType, Target, nullptr, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    UMaterialExpression* Expression = nullptr;
    if (!FindExpressionById(Target, NodeId, Expression, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    const FString DeletedNodeId = GetExpressionId(Expression);
    if (Target.Material)
    {
        UMaterialEditingLibrary::DeleteMaterialExpression(Target.Material, Expression);
    }
    else if (Target.Function)
    {
        UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(Target.Function, Expression);
    }

    Target.MarkChanged();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material"), Target.GetPathName());
    ResultObj->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    ResultObj->SetStringField(TEXT("deleted_node_id"), DeletedNodeId);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleLayoutMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return MaterialApiError(TEXT("Missing 'material_path' parameter"));
    }

    FString GraphType;
    Params->TryGetStringField(TEXT("graph_type"), GraphType);

    FMaterialGraphTarget Target;
    FString ErrorMessage;
    if (!ResolveMaterialGraph(MaterialPath, GraphType, Target, nullptr, ErrorMessage))
    {
        return MaterialApiError(ErrorMessage);
    }

    if (Target.Material)
    {
        UMaterialEditingLibrary::LayoutMaterialExpressions(Target.Material);
    }
    else if (Target.Function)
    {
        UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(Target.Function);
    }

    Target.MarkChanged();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material"), Target.GetPathName());
    ResultObj->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    ResultObj->SetBoolField(TEXT("laid_out"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleGetMaterialParameterCollectionValues(const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionPath;
    if (!Params->TryGetStringField(TEXT("collection_path"), CollectionPath))
    {
        return MaterialApiError(TEXT("Missing 'collection_path' parameter"));
    }

    bool bIncludeAssetDefaults = true;
    if (Params->HasField(TEXT("include_asset_defaults")))
    {
        bIncludeAssetDefaults = Params->GetBoolField(TEXT("include_asset_defaults"));
    }

    bool bIncludeRuntime = true;
    if (Params->HasField(TEXT("include_runtime")))
    {
        bIncludeRuntime = Params->GetBoolField(TEXT("include_runtime"));
    }

    UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(LoadObjectValue(CollectionPath));
    if (!Collection)
    {
        return MaterialApiError(FString::Printf(TEXT("Material Parameter Collection not found: %s"), *CollectionPath));
    }

    UWorld* World = bIncludeRuntime ? GetEditorOrPIEWorld() : nullptr;
    UMaterialParameterCollectionInstance* Instance = (World && bIncludeRuntime) ? World->GetParameterCollectionInstance(Collection) : nullptr;
    TSet<FName> ParameterFilter;
    FString FilterError;
    if (!GetOptionalParameterNameFilter(Params, ParameterFilter, FilterError)) { return MaterialApiError(FilterError); }

    TArray<TSharedPtr<FJsonValue>> ScalarValues;
    for (const FCollectionScalarParameter& Parameter : Collection->ScalarParameters)
    {
        if (!ShouldIncludeNamedParameter(ParameterFilter, Parameter.ParameterName))
        {
            continue;
        }

        TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
        ParameterObject->SetStringField(TEXT("name"), Parameter.ParameterName.ToString());
        ParameterObject->SetStringField(TEXT("id"), Parameter.Id.ToString(EGuidFormats::DigitsWithHyphens));

        if (bIncludeAssetDefaults)
        {
            ParameterObject->SetNumberField(TEXT("asset_default"), Parameter.DefaultValue);
        }

        bool bRuntimeResolved = false;
        float RuntimeValue = 0.0f;
        if (Instance)
        {
            bRuntimeResolved = Instance->GetScalarParameterValue(Parameter.ParameterName, RuntimeValue);
        }
        ParameterObject->SetBoolField(TEXT("runtime_resolved"), bRuntimeResolved);
        if (bRuntimeResolved)
        {
            ParameterObject->SetNumberField(TEXT("runtime_value"), RuntimeValue);
        }

        ScalarValues.Add(MakeShared<FJsonValueObject>(ParameterObject));
    }

    TArray<TSharedPtr<FJsonValue>> VectorValues;
    for (const FCollectionVectorParameter& Parameter : Collection->VectorParameters)
    {
        if (!ShouldIncludeNamedParameter(ParameterFilter, Parameter.ParameterName))
        {
            continue;
        }

        TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
        ParameterObject->SetStringField(TEXT("name"), Parameter.ParameterName.ToString());
        ParameterObject->SetStringField(TEXT("id"), Parameter.Id.ToString(EGuidFormats::DigitsWithHyphens));

        if (bIncludeAssetDefaults)
        {
            ParameterObject->SetObjectField(TEXT("asset_default"), LinearColorToJson(Parameter.DefaultValue));
        }

        bool bRuntimeResolved = false;
        FLinearColor RuntimeValue = FLinearColor::Transparent;
        if (Instance)
        {
            bRuntimeResolved = Instance->GetVectorParameterValue(Parameter.ParameterName, RuntimeValue);
        }
        ParameterObject->SetBoolField(TEXT("runtime_resolved"), bRuntimeResolved);
        if (bRuntimeResolved)
        {
            ParameterObject->SetObjectField(TEXT("runtime_value"), LinearColorToJson(RuntimeValue));
        }

        VectorValues.Add(MakeShared<FJsonValueObject>(ParameterObject));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("collection"), Collection->GetPathName());
    ResultObj->SetBoolField(TEXT("include_asset_defaults"), bIncludeAssetDefaults);
    ResultObj->SetBoolField(TEXT("include_runtime"), bIncludeRuntime);
    ResultObj->SetBoolField(TEXT("has_world"), World != nullptr);
    ResultObj->SetStringField(TEXT("world_type"), GetWorldTypeName(World));
    ResultObj->SetBoolField(TEXT("has_runtime_instance"), Instance != nullptr);
    ResultObj->SetArrayField(TEXT("scalars"), ScalarValues);
    ResultObj->SetArrayField(TEXT("vectors"), VectorValues);
    ResultObj->SetNumberField(TEXT("scalar_count"), ScalarValues.Num());
    ResultObj->SetNumberField(TEXT("vector_count"), VectorValues.Num());
    return ResultObj;
}
