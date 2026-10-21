// UE 5.8 collection inheritance adaptation. Public engine getters only.
// Included in CubelessMaterialApi.cpp's anonymous namespace after legacy helpers.
constexpr int32 MaxCollectionBaseDepth = 32;
constexpr int32 MaxCollectionReadParameters = 10000;

struct FCollectionReadView
{
    TArray<UMaterialParameterCollection*> Chain;
    TArray<FName> ScalarNames;
    TArray<FName> VectorNames;
    TMap<FName, UMaterialParameterCollection*> DeclaringCollections;
    int32 TotalParameters = 0;
};

bool BuildCollectionReadView(UMaterialParameterCollection* Collection, int32 MaxParameters, FCollectionReadView& Out, FString& Error)
{
    CountRead(TEXT("collection_view_build_calls"));
    if (!Collection) { Error = TEXT("Material Parameter Collection is null."); return false; }
    TSet<UMaterialParameterCollection*> Visited;
    TSet<FName> Names;
    TSet<FGuid> Ids;
    for (UMaterialParameterCollection* Current = Collection; Current; Current = Current->GetBaseParameterCollection())
    {
        CountRead(TEXT("collection_base_chain_visits"));
        if (Visited.Contains(Current)) { Error = TEXT("Collection base chain contains a cycle: ") + Current->GetPathName(); return false; }
        if (Out.Chain.Num() >= MaxCollectionBaseDepth) { Error = TEXT("Collection base chain exceeds depth 32."); return false; }
        Visited.Add(Current);
        Out.Chain.Add(Current);
        const int64 Total = static_cast<int64>(Out.TotalParameters) + Current->ScalarParameters.Num() + Current->VectorParameters.Num();
        if (Total > MaxParameters) { Error = TEXT("Effective collection parameter count exceeds the inspection budget."); return false; }
        Out.TotalParameters = static_cast<int32>(Total);
        auto AddDeclaration = [&](const FCollectionParameterBase& Parameter)
        {
            CountRead(TEXT("collection_parameter_declaration_visits"));
            if (Parameter.ParameterName.IsNone() || !Parameter.Id.IsValid() || Names.Contains(Parameter.ParameterName) || Ids.Contains(Parameter.Id))
            {
                Error = TEXT("Invalid or ambiguous collection parameter name/ID in base chain: ") + Parameter.ParameterName.ToString();
                return false;
            }
            Names.Add(Parameter.ParameterName);
            Ids.Add(Parameter.Id);
            Out.DeclaringCollections.Add(Parameter.ParameterName, Current);
            return true;
        };
        for (const FCollectionScalarParameter& Parameter : Current->ScalarParameters) { if (!AddDeclaration(Parameter)) { return false; } }
        for (const FCollectionVectorParameter& Parameter : Current->VectorParameters) { if (!AddDeclaration(Parameter)) { return false; } }
    }
    // Engine name getters recurse; the chain was bounded and checked first.
    CountRead(TEXT("collection_effective_name_enumerations"), 2);
    Out.ScalarNames = Collection->GetScalarParameterNames();
    Out.VectorNames = Collection->GetVectorParameterNames();
    if (Out.ScalarNames.Num() + Out.VectorNames.Num() != Out.TotalParameters)
    {
        Error = TEXT("Public effective collection name enumeration disagrees with the inspected chain."); return false;
    }
    return true;
}

TArray<TSharedPtr<FJsonValue>> CollectionChainToJson(const FCollectionReadView& View)
{
    TArray<TSharedPtr<FJsonValue>> Chain;
    for (const UMaterialParameterCollection* Collection : View.Chain)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("collection"), Collection->GetPathName());
        Row->SetNumberField(TEXT("local_scalar_count"), Collection->ScalarParameters.Num());
        Row->SetNumberField(TEXT("local_vector_count"), Collection->VectorParameters.Num());
        Chain.Add(MakeShared<FJsonValueObject>(Row));
    }
    return Chain;
}

TSharedPtr<FJsonObject> CollectionReadError(const FString& Error)
{
    auto Result = MaterialApiError(Error);
    Result->SetStringField(TEXT("error_code"), TEXT("collection_inheritance_inspection_failed"));
    Result->SetStringField(TEXT("scope"), TEXT("bounded public collection base-chain inspection"));
    return Result;
}

bool IsFiniteCollectionColor(const FLinearColor& Value)
{
    return FMath::IsFinite(Value.R) && FMath::IsFinite(Value.G) && FMath::IsFinite(Value.B) && FMath::IsFinite(Value.A);
}

TSharedPtr<FJsonObject> ReadEffectiveCollectionValues(const TSharedPtr<FJsonObject>& Params, int32 MaxParameters)
{
    CountRead(TEXT("collection_values_read_calls"));
    FString Error;
    TSet<FName> Filter;
    if (!GetOptionalParameterNameFilter(Params, Filter, Error))
    {
        auto Result = MaterialApiError(Error);
        Result->SetStringField(TEXT("error_code"), TEXT("collection_parameter_filter_invalid"));
        return Result;
    }
    FString Path;
    Params->TryGetStringField(TEXT("collection_path"), Path);
    auto* Collection = Cast<UMaterialParameterCollection>(LoadObjectValue(Path));
    FCollectionReadView View;
    if (!BuildCollectionReadView(Collection, MaxParameters, View, Error)) { return CollectionReadError(Error); }
    bool bDefaults = true, bRuntime = true;
    Params->TryGetBoolField(TEXT("include_asset_defaults"), bDefaults);
    Params->TryGetBoolField(TEXT("include_runtime"), bRuntime);
    UWorld* World = bRuntime ? GetEditorOrPIEWorld() : nullptr;
    UMaterialParameterCollectionInstance* Instance = World ? World->GetParameterCollectionInstance(Collection) : nullptr;
    TArray<TSharedPtr<FJsonValue>> Scalars, Vectors;
    auto NewRow = [&](FName Name, const FGuid& Id)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Name.ToString());
        Row->SetStringField(TEXT("id"), Id.ToString(EGuidFormats::DigitsWithHyphens));
        UMaterialParameterCollection* Declared = View.DeclaringCollections.FindRef(Name);
        Row->SetStringField(TEXT("declared_in"), Declared ? Declared->GetPathName() : FString());
        Row->SetBoolField(TEXT("inherited"), Declared != Collection);
        Row->SetStringField(TEXT("asset_default_semantics"), TEXT("effective_default_at_queried_collection_including_base_overrides"));
        Row->SetStringField(TEXT("runtime_value_semantics"), TEXT("public_world_instance_getter_observation; not rendered_uniform_buffer_proof"));
        return Row;
    };
    for (FName Name : View.ScalarNames)
    {
        if (!ShouldIncludeNamedParameter(Filter, Name)) { continue; }
        CountRead(TEXT("collection_typed_parameter_lookups"));
        const FCollectionScalarParameter* Parameter = Collection->GetScalarParameterByName(Name);
        if (!Parameter) { return CollectionReadError(TEXT("Effective scalar ID lookup disagreed: ") + Name.ToString()); }
        CountRead(TEXT("collection_parameter_id_lookups"));
        if (Parameter->Id != Collection->GetParameterId(Name)) { return CollectionReadError(TEXT("Effective scalar ID lookup disagreed: ") + Name.ToString()); }
        auto Row = NewRow(Name, Parameter->Id);
        if (bDefaults)
        {
            bool bFound = false;
            CountRead(TEXT("collection_asset_default_lookups"));
            const float Value = Collection->GetScalarParameterDefaultValue(Name, bFound);
            if (!bFound || !FMath::IsFinite(Value)) { return CollectionReadError(TEXT("Effective scalar default unavailable/nonfinite: ") + Name.ToString()); }
            Row->SetNumberField(TEXT("asset_default"), Value);
        }
        float Value = 0.0f;
        if (Instance) { CountRead(TEXT("collection_runtime_value_lookups")); }
        const bool bResolved = Instance && Instance->GetScalarParameterValue(Name, Value);
        if (bResolved && !FMath::IsFinite(Value)) { return CollectionReadError(TEXT("Runtime scalar value is nonfinite: ") + Name.ToString()); }
        Row->SetBoolField(TEXT("runtime_resolved"), bResolved);
        if (bResolved) { Row->SetNumberField(TEXT("runtime_value"), Value); }
        Scalars.Add(MakeShared<FJsonValueObject>(Row));
    }
    for (FName Name : View.VectorNames)
    {
        if (!ShouldIncludeNamedParameter(Filter, Name)) { continue; }
        CountRead(TEXT("collection_typed_parameter_lookups"));
        const FCollectionVectorParameter* Parameter = Collection->GetVectorParameterByName(Name);
        if (!Parameter) { return CollectionReadError(TEXT("Effective vector ID lookup disagreed: ") + Name.ToString()); }
        CountRead(TEXT("collection_parameter_id_lookups"));
        if (Parameter->Id != Collection->GetParameterId(Name)) { return CollectionReadError(TEXT("Effective vector ID lookup disagreed: ") + Name.ToString()); }
        auto Row = NewRow(Name, Parameter->Id);
        if (bDefaults)
        {
            bool bFound = false;
            CountRead(TEXT("collection_asset_default_lookups"));
            const FLinearColor Value = Collection->GetVectorParameterDefaultValue(Name, bFound);
            if (!bFound || !IsFiniteCollectionColor(Value)) { return CollectionReadError(TEXT("Effective vector default unavailable/nonfinite: ") + Name.ToString()); }
            Row->SetObjectField(TEXT("asset_default"), LinearColorToJson(Value));
        }
        FLinearColor Value = FLinearColor::Transparent;
        if (Instance) { CountRead(TEXT("collection_runtime_value_lookups")); }
        const bool bResolved = Instance && Instance->GetVectorParameterValue(Name, Value);
        if (bResolved && !IsFiniteCollectionColor(Value)) { return CollectionReadError(TEXT("Runtime vector value is nonfinite: ") + Name.ToString()); }
        Row->SetBoolField(TEXT("runtime_resolved"), bResolved);
        if (bResolved) { Row->SetObjectField(TEXT("runtime_value"), LinearColorToJson(Value)); }
        Vectors.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("collection"), Collection->GetPathName());
    Result->SetBoolField(TEXT("include_asset_defaults"), bDefaults);
    Result->SetBoolField(TEXT("include_runtime"), bRuntime);
    Result->SetBoolField(TEXT("has_world"), World != nullptr);
    Result->SetStringField(TEXT("world_type"), GetWorldTypeName(World));
    Result->SetStringField(TEXT("world_path"), World ? World->GetPathName() : FString());
    Result->SetBoolField(TEXT("has_runtime_instance"), Instance != nullptr);
    // UE58's exported world getter may allocate an instance and update render
    // resources before this payload is captured. No pre-getter inventory is read.
    Result->SetBoolField(TEXT("runtime_instance_getter_dispatched"), World != nullptr);
    Result->SetBoolField(TEXT("runtime_instance_creation_possible"), World != nullptr);
    Result->SetStringField(TEXT("runtime_instance_creation_observed"), TEXT("not_measured"));
    Result->SetBoolField(TEXT("runtime_render_resource_side_effects_possible"), World != nullptr);
    Result->SetBoolField(TEXT("explicit_parameter_value_write_attempted"), false);
    Result->SetStringField(TEXT("runtime_observation_scope"), TEXT("value reads after public world getter; pre-getter world instance/resource changes are not observed"));
    Result->SetArrayField(TEXT("scalars"), Scalars);
    Result->SetArrayField(TEXT("vectors"), Vectors);
    Result->SetNumberField(TEXT("scalar_count"), Scalars.Num());
    Result->SetNumberField(TEXT("vector_count"), Vectors.Num());
    Result->SetArrayField(TEXT("base_chain"), CollectionChainToJson(View));
    Result->SetStringField(TEXT("base_chain_order"), TEXT("queried_collection_to_root_base"));
    Result->SetNumberField(TEXT("effective_parameter_count_before_filter"), View.TotalParameters);
    Result->SetStringField(TEXT("collection_semantics_version"), TEXT("ue58_effective_collection_v1"));
    Result->SetBoolField(TEXT("rendered_runtime_verified"), false);
    return Result;
}

TSharedPtr<FJsonObject> ReadEffectiveCollectionNodes(const TSharedPtr<FJsonObject>& Params)
{
    CountRead(TEXT("collection_nodes_read_calls"));
    FString Path, GraphType, Error;
    Params->TryGetStringField(TEXT("material_path"), Path);
    Params->TryGetStringField(TEXT("graph_type"), GraphType);
    FMaterialGraphTarget Target;
    if (!ResolveMaterialGraph(Path, GraphType, Target, nullptr, Error)) { return MaterialApiError(Error); }
    TMap<UMaterialParameterCollection*, FCollectionReadView> Views;
    TArray<TSharedPtr<FJsonValue>> Nodes;
    int32 Missing = 0, Mismatched = 0;
    for (const auto& Expression : Target.GetExpressions())
    {
        CountRead(TEXT("collection_node_expression_visits"));
        auto* Node = Cast<UMaterialExpressionCollectionParameter>(Expression.Get());
        if (!Node) { continue; }
        UMaterialParameterCollection* Collection = Node->Collection;
        FCollectionReadView* View = Collection ? Views.Find(Collection) : nullptr;
        if (Collection && !View)
        {
            FCollectionReadView NewView;
            if (!BuildCollectionReadView(Collection, MaxCollectionReadParameters, NewView, Error)) { return CollectionReadError(Error); }
            View = &Views.Add(Collection, MoveTemp(NewView));
        }
        if (Collection) { CountRead(TEXT("collection_typed_parameter_lookups"), 2); }
        const FCollectionScalarParameter* Scalar = Collection ? Collection->GetScalarParameterByName(Node->ParameterName) : nullptr;
        const FCollectionVectorParameter* Vector = Collection ? Collection->GetVectorParameterByName(Node->ParameterName) : nullptr;
        const bool bFound = Scalar || Vector;
        const FGuid Id = Scalar ? Scalar->Id : Vector ? Vector->Id : FGuid();
        const bool bMatches = bFound && Id == Node->ParameterId;
        Missing += !bFound;
        Mismatched += bFound && !bMatches;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("node_id"), GetExpressionId(Node));
        Row->SetStringField(TEXT("node_name"), Node->GetName());
        Row->SetStringField(TEXT("object_path"), Node->GetPathName());
        Row->SetStringField(TEXT("parameter_name"), Node->ParameterName.ToString());
        Row->SetStringField(TEXT("parameter_id"), Node->ParameterId.ToString(EGuidFormats::DigitsWithHyphens));
        Row->SetNumberField(TEXT("x"), Node->MaterialExpressionEditorX);
        Row->SetNumberField(TEXT("y"), Node->MaterialExpressionEditorY);
        Row->SetStringField(TEXT("collection"), Collection ? Collection->GetPathName() : FString());
        Row->SetBoolField(TEXT("collection_parameter_found"), bFound);
        Row->SetBoolField(TEXT("parameter_id_matches_collection"), bMatches);
        Row->SetStringField(TEXT("collection_parameter_id"), bFound ? Id.ToString(EGuidFormats::DigitsWithHyphens) : FString());
        Row->SetStringField(TEXT("collection_parameter_type"), Scalar ? TEXT("scalar") : Vector ? TEXT("vector") : TEXT(""));
        UMaterialParameterCollection* Declared = View ? View->DeclaringCollections.FindRef(Node->ParameterName) : nullptr;
        Row->SetStringField(TEXT("declared_in"), Declared ? Declared->GetPathName() : FString());
        Row->SetBoolField(TEXT("inherited"), Declared && Declared != Collection);
        Row->SetStringField(TEXT("parameter_lookup_scope"), TEXT("effective_name_and_ID_across_public_base_chain"));
        if (View) { Row->SetArrayField(TEXT("base_chain"), CollectionChainToJson(*View)); }
        Nodes.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("material"), Target.GetPathName());
    Result->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    Result->SetNumberField(TEXT("collection_parameter_node_count"), Nodes.Num());
    Result->SetNumberField(TEXT("mismatched_id_count"), Mismatched);
    Result->SetNumberField(TEXT("missing_collection_parameter_count"), Missing);
    Result->SetArrayField(TEXT("nodes"), Nodes);
    Result->SetStringField(TEXT("collection_semantics_version"), TEXT("ue58_effective_collection_v1"));
    Result->SetStringField(TEXT("node_scope"), TEXT("direct MaterialExpressionCollectionParameter nodes; CollectionTransform and nested functions not validated"));
    return Result;
}

TSharedPtr<FJsonObject> DirectCollectionMetadata(const FMaterialGraphTarget& Target, int32 MaxParameters, FString& Error)
{
    CountRead(TEXT("direct_collection_metadata_calls"));
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("status"), TEXT("base_chains_inspected_effective_values_not_evaluated"));
    Result->SetStringField(TEXT("scope"), TEXT("direct CollectionParameter and CollectionTransform references; material instances use base material graph"));
    Result->SetBoolField(TEXT("effective_values_verified"), false);
    Result->SetBoolField(TEXT("parameter_ids_verified"), false);
    Result->SetBoolField(TEXT("collection_transform_semantics_verified"), false);
    Result->SetBoolField(TEXT("nested_function_collections_verified"), false);
    Result->SetBoolField(TEXT("rendered_runtime_verified"), false);
    TSet<UMaterialParameterCollection*> SeenRoots, SeenChain;
    TArray<TSharedPtr<FJsonValue>> Collections;
    int64 Total = 0;
    for (const auto& Expression : Target.GetExpressions())
    {
        CountRead(TEXT("collection_metadata_expression_visits"));
        UMaterialParameterCollection* Collection = nullptr;
        if (auto* ParameterNode = Cast<UMaterialExpressionCollectionParameter>(Expression.Get())) { Collection = ParameterNode->Collection; }
        else if (auto* TransformNode = Cast<UMaterialExpressionCollectionTransform>(Expression.Get())) { Collection = TransformNode->Collection; }
        if (!Collection || SeenRoots.Contains(Collection)) { continue; }
        SeenRoots.Add(Collection);
        FCollectionReadView View;
        if (!BuildCollectionReadView(Collection, MaxParameters, View, Error)) { return CollectionReadError(Error); }
        for (UMaterialParameterCollection* Current : View.Chain)
        {
            if (SeenChain.Contains(Current)) { continue; }
            SeenChain.Add(Current);
            Total += Current->ScalarParameters.Num() + Current->VectorParameters.Num();
            if (Total > MaxParameters) { Error = TEXT("Unique direct graph collection parameters exceed the inspection budget."); return CollectionReadError(Error); }
        }
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("collection"), Collection->GetPathName());
        Row->SetStringField(TEXT("base_collection"), View.Chain.Num() > 1 ? View.Chain[1]->GetPathName() : FString());
        Row->SetBoolField(TEXT("has_inherited_parameters"), View.Chain.Num() > 1);
        Row->SetArrayField(TEXT("base_chain"), CollectionChainToJson(View));
        Row->SetNumberField(TEXT("effective_parameter_count"), View.TotalParameters);
        Collections.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("collections"), Collections);
    Result->SetNumberField(TEXT("unique_chain_parameter_count"), Total);
    Result->SetNumberField(TEXT("max_base_depth"), MaxCollectionBaseDepth);
    return Result;
}

TSharedPtr<FJsonObject> AddDirectCollectionMetadata(TSharedPtr<FJsonObject> Data, const FString& Path)
{
    FMaterialGraphTarget Target;
    UObject* Asset = Path.IsEmpty() ? nullptr : FindObject<UObject>(nullptr, *Path);
    if (auto* Instance = Cast<UMaterialInstance>(Asset)) { Target.Material = Instance->GetMaterial(); Target.Asset = Target.Material; }
    else if (auto* Material = Cast<UMaterial>(Asset)) { Target.Material = Material; Target.Asset = Material; }
    else if (auto* Function = Cast<UMaterialFunction>(Asset)) { Target.Function = Function; Target.Asset = Function; }
    if (Target.IsValid())
    {
        FString Error;
        auto Metadata = DirectCollectionMetadata(Target, MaxCollectionReadParameters, Error);
        if (!Error.IsEmpty()) { return Metadata; }
        Data->SetObjectField(TEXT("ue58_collection_semantics"), Metadata);
    }
    return Data;
}

TSharedPtr<FJsonObject> ReadNodesWithCollectionMetadata(const TSharedPtr<FJsonObject>& Params)
{
    auto Data = HandleListMaterialNodes(Params);
    FString Path;
    Data->TryGetStringField(TEXT("material"), Path);
    return AddDirectCollectionMetadata(Data, Path);
}
