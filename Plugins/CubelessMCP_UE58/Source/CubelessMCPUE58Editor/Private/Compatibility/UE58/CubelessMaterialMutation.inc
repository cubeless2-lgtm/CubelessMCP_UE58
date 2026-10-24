// UE58 adaptation of legacy HandleAddMaterialNode (line 5092),
// HandleSetMaterialNodeProperty (5298), and SetObjectPropertyValue (801).
// Source/provenance is recorded in native-material-mutation-extraction.json.
// Included only inside CubelessMaterialApi.cpp's anonymous namespace.

bool IsForbiddenReferencePath(const FString& Path)
{
    if (IsToolPackage(Path)) { return true; }
    for (const TCHAR* Prefix : {TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/"), TEXT("/ModelContextProtocol/"),
        TEXT("/ToolsetRegistry/"), TEXT("/PythonScriptPlugin/"), TEXT("/EditorScriptingUtilities/"),
        TEXT("/CubelessControlRigTools/"), TEXT("/Engine/Editor"), TEXT("/Script/UnrealEd"), TEXT("/Script/Blutility")})
    {
        if (Path.StartsWith(Prefix, ESearchCase::IgnoreCase)) { return true; }
    }
    return false;
}

// Exact installed Runtime/Landscape expression leaves only. The two CustomOutput
// leaves have an Engine UMaterialExpressionCustomOutput parent; the previous six
// retain their direct UMaterialExpression parent check. No module-wide admission.
bool IsVerifiedLandscapeExpressionClass(const UClass* Class)
{
    if(!Class||Class->GetOutermost()->GetName()!=TEXT("/Script/Landscape"))return false;
    if(Class==UMaterialExpressionLandscapeGrassOutput::StaticClass()
        ||Class==UMaterialExpressionLandscapePhysicalMaterialOutput::StaticClass())
        return Class->GetSuperClass()==UMaterialExpressionCustomOutput::StaticClass();
    return Class->GetSuperClass()==UMaterialExpression::StaticClass()
        &&(Class==UMaterialExpressionLandscapeLayerBlend::StaticClass()
            ||Class==UMaterialExpressionLandscapeLayerCoords::StaticClass()
            ||Class==UMaterialExpressionLandscapeLayerSample::StaticClass()
            ||Class==UMaterialExpressionLandscapeLayerSwitch::StaticClass()
            ||Class==UMaterialExpressionLandscapeLayerWeight::StaticClass()
            ||Class==UMaterialExpressionLandscapeVisibilityMask::StaticClass());
}

// GrassTypes/Inputs contain these exact native runtime asset references. Their
// assets still undergo the existing loaded-property and saved dependency audit;
// this does not admit generated subclasses, custom objects or any tool module.
bool IsVerifiedLandscapeOutputAssetClass(const UClass* Class)
{
    return Class&&Class->GetSuperClass()==UObject::StaticClass()
        &&((Class==ULandscapeGrassType::StaticClass()&&Class->GetOutermost()->GetName()==TEXT("/Script/Landscape"))
            ||(Class==UPhysicalMaterial::StaticClass()&&Class->GetOutermost()->GetName()==TEXT("/Script/PhysicsCore")));
}

// ADI: the installed public MaterialX blur leaf is declared by the runtime
// InterchangeImport module, not Engine. Admit only its exact resident native
// class and public TextureSampleParameter2D superclass. No module load, new CDO,
// generated subclass, deprecated MaterialX replacement or module-wide allowlist.
// The shared mutation snapshots/property/reference checks remain authoritative.
bool IsVerifiedMaterialXBlurExpressionClass(const UClass* Class)
{
    static const TCHAR* LeafPath=TEXT("/Script/InterchangeImport.MaterialExpressionMaterialXTextureSampleParameterBlur");
    static const TCHAR* ParentPath=TEXT("/Script/Engine.MaterialExpressionTextureSampleParameter2D");
    if(!IsValid(Class)||Class->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)
        ||!Class->GetPathName().Equals(LeafPath,ESearchCase::CaseSensitive)
        ||!Class->GetOutermost()->GetName().Equals(TEXT("/Script/InterchangeImport"),ESearchCase::CaseSensitive)
        ||!Class->HasAnyClassFlags(CLASS_Native)||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)
        ||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy.GetHandle()!=TObjectPtr<UObject>(nullptr).GetHandle()
        ||Class->IsEditorOnly())return false;
    const TStrongObjectPtr<UClass> LeafHold(const_cast<UClass*>(Class));
    auto* Parent=FindObject<UClass>(nullptr,ParentPath);
    if(!IsValid(Parent)||Parent->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)
        ||!Parent->HasAnyClassFlags(CLASS_Native)||Parent->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists))return false;
    const TStrongObjectPtr<UClass> ParentHold(Parent);
    // Read the existing reflected superclass slot without tracked GetSuperClass
    // or class-default construction. These public descriptor/layout checks are
    // scoped to the installed UE5.8 UStruct declaration, not arbitrary classes.
    const FObjectProperty* P=FindFProperty<FObjectProperty>(UStruct::StaticClass(),TEXT("SuperStruct"));
    if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=UStruct::StaticClass()||P->GetOwnerProperty()!=P
        ||P->ArrayDim!=1||P->ElementSize!=sizeof(TObjectPtr<UStruct>)||P->GetSize()!=sizeof(TObjectPtr<UStruct>)
        ||P->GetMinAlignment()!=alignof(TObjectPtr<UStruct>)||P->GetOffset_ForInternal()<0
        ||int64(P->GetOffset_ForInternal())+P->GetSize()>UStruct::StaticClass()->GetPropertiesSize()||P->HasSetterOrGetter()
        ||!P->PropertyClass.IsResolved()||P->PropertyClass.GetHandle()!=TObjectPtr<UClass>(UStruct::StaticClass()).GetHandle())return false;
    const TObjectPtr<UObject> SuperSlot=P->GetObjectPtrPropertyValue(P->ContainerPtrToValuePtr<void>(Class));
    return SuperSlot.IsResolved()&&SuperSlot.GetHandle()==TObjectPtr<UObject>(Parent).GetHandle();
}

bool IsVerifiedRuntimeClass(const UClass* Class)
{
    if (!Class || !IsSupportedExistingClass(Class)) { return false; }
    const bool LandscapeLeaf=IsVerifiedLandscapeExpressionClass(Class)||IsVerifiedLandscapeOutputAssetClass(Class);
    const bool MaterialXBlurLeaf=IsVerifiedMaterialXBlurExpressionClass(Class);
    for (const UClass* Current = Class; Current; Current = Current->GetSuperClass())
    {
        const FString Package = Current->GetOutermost()->GetName();
        const bool AuditedLeaf=Current==Class
            &&((LandscapeLeaf&&(Package==TEXT("/Script/Landscape")||Package==TEXT("/Script/PhysicsCore")))
                ||(MaterialXBlurLeaf&&Package.Equals(TEXT("/Script/InterchangeImport"),ESearchCase::CaseSensitive)));
        if (!Current->HasAnyClassFlags(CLASS_Native) ||
            (Package != TEXT("/Script/Engine") && Package != TEXT("/Script/CoreUObject")&&!AuditedLeaf)) { return false; }
    }
    return true;
}

int32 GReferenceAuditDepth = 0;
int32 GReferenceAuditValues = 0;
TSet<const UObject*> GReferenceAuditVisited;

bool ValidateReference(UObject* Object, UObject* Target, FString& Error)
{
    if (!Object) { return true; }
    if (GReferenceAuditDepth == 0) { GReferenceAuditVisited.Reset(); GReferenceAuditValues = 0; }
    if (GReferenceAuditDepth > 16 || ++GReferenceAuditValues > 8192) { Error = TEXT("Loaded reference audit exceeded its bounded budget."); return false; }
    TGuardValue<int32> DepthGuard(GReferenceAuditDepth, GReferenceAuditDepth + 1);
    const UClass* ReferencedClass = Cast<UClass>(Object);
    if (ReferencedClass)
    {
        if (IsVerifiedRuntimeClass(ReferencedClass)) { return true; }
        Error = TEXT("Referenced class is not an audited runtime Engine/CoreUObject, exact Landscape expression/GrassType/PhysicalMaterial asset or exact public MaterialXTextureSampleParameterBlur leaf."); return false;
    }
    UPackage* Package = Object->GetOutermost();
    if (!Package || IsForbiddenReferencePath(Object->GetPathName()) ||
        Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly) ||
        Object->IsEditorOnly() || Object->HasAnyFlags(RF_Transient) || !IsVerifiedRuntimeClass(Object->GetClass()))
    {
        Error = TEXT("Tool/editor/transient or unaudited reference is not allowed: ") + Object->GetPathName(); return false;
    }
    if (Object->IsA<UMaterialExpression>())
    {
        if (Object->GetOuter() == Target) { return true; }
        Error = TEXT("Expression references must belong to the same material/function."); return false;
    }
    if (!Object->IsAsset()) { Error = TEXT("References must be assets or same-graph expressions: ") + Object->GetPathName(); return false; }
    if (GReferenceAuditVisited.Contains(Object)) { return true; }
    GReferenceAuditVisited.Add(Object);
    const FString Root = Package->GetName();
    if (!Root.StartsWith(TEXT("/Game/")) && !Root.StartsWith(TEXT("/Engine/")))
    {
        Error = TEXT("New asset references currently require /Game/ or /Engine/; other plugin mounts need a runtime audit: ") + Root; return false;
    }
    // Include the root and saved hard/soft transitive package dependencies.
    // Unsaved property references are also checked when importing each value.
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FName> Pending{FName(*Root)};
    TSet<FName> Visited;
    while (!Pending.IsEmpty())
    {
        const FName Name = Pending.Pop(EAllowShrinking::No);
        if (Visited.Contains(Name)) { continue; }
        Visited.Add(Name);
        if (Visited.Num() > 4096) { Error = TEXT("Reference dependency audit exceeded 4096 packages."); return false; }
        const FString Path = Name.ToString();
        if (IsForbiddenReferencePath(Path) ||
            (Path.StartsWith(TEXT("/Script/")) && Path != TEXT("/Script/Engine") && Path != TEXT("/Script/CoreUObject")
                && Path != TEXT("/Script/Landscape") && Path != TEXT("/Script/PhysicsCore")
                && Path != TEXT("/Script/InterchangeImport")))
        {
            Error = TEXT("Reference dependency is editor/tool or an unaudited script module: ") + Path; return false;
        }
        if (!Path.StartsWith(TEXT("/Script/")) && !Path.StartsWith(TEXT("/Game/")) && !Path.StartsWith(TEXT("/Engine/")))
        {
            Error = TEXT("Reference dependency uses an unaudited plugin mount: ") + Path; return false;
        }
        TArray<FName> Dependencies;
        Registry.GetDependencies(Name, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
        Pending.Append(Dependencies);
    }
    // Inspect loaded persistent references too; saved registry data alone would
    // miss a newly edited, unsaved function's forbidden expression/reference.
    TArray<UObject*> InspectObjects{Object};
    if (UMaterialFunction* Function = Cast<UMaterialFunction>(Object))
    {
        for (const auto& Expression : Function->GetExpressions())
        {
            if (Expression)
            {
                if (!IsVerifiedRuntimeClass(Expression->GetClass())) { Error = TEXT("Referenced function contains an unaudited expression class."); return false; }
                InspectObjects.Add(Expression.Get());
            }
        }
    }
    for (UObject* Inspect : InspectObjects)
    {
        for (FPropertyValueIterator It(FProperty::StaticClass(), Inspect->GetClass(), Inspect); It; ++It)
        {
            if (++GReferenceAuditValues > 8192) { Error = TEXT("Loaded reference audit exceeded 8192 property values."); return false; }
            if (It.Key()->HasAnyPropertyFlags(CPF_Transient | CPF_EditorOnly | CPF_Deprecated)) { It.SkipRecursiveProperty(); continue; }
            UObject* Reference = nullptr;
            if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(It.Key()))
            {
                const FSoftObjectPtr Value = Soft->GetPropertyValue(It.Value());
                if (!Value.IsNull())
                {
                    Reference = Value.LoadSynchronous();
                    if (!Reference) { Error = TEXT("Loaded asset has an unresolved persistent soft reference: ") + Value.ToString(); return false; }
                }
            }
            else if (const FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(It.Key())) { Reference = Property->GetObjectPropertyValue(It.Value()); }
            else if (const FStructProperty* Struct = CastField<FStructProperty>(It.Key()); Struct &&
                (Struct->Struct->GetName() == TEXT("SoftObjectPath") || Struct->Struct->GetName() == TEXT("SoftClassPath")))
            {
                const FSoftObjectPath& Path = *static_cast<const FSoftObjectPath*>(It.Value());
                if (!Path.IsNull())
                {
                    Reference = Path.TryLoad();
                    if (!Reference) { Error = TEXT("Loaded asset has an unresolved persistent soft object path: ") + Path.ToString(); return false; }
                }
            }
            if (Reference && Reference != Object && Reference != Inspect && !ValidateReference(Reference, Object, Error)) { return false; }
        }
    }
    return true;
}

bool ValidateJsonReferenceText(const TSharedPtr<FJsonValue>& Value, FString& Error, int32 Depth = 0)
{
    if (!Value.IsValid() || Depth > 16) { Error = TEXT("Invalid or excessively nested property JSON."); return false; }
    if (Value->Type == EJson::String)
    {
        const FString Text = Value->AsString();
        for (const TCHAR* Token : {TEXT("/Script/CubelessMCP"), TEXT("/Script/UnrealMCP"), TEXT("/Script/ModelContextProtocol"),
            TEXT("/Script/ToolsetRegistry"), TEXT("/Script/PythonScriptPlugin"), TEXT("/CubelessMCP_UE58/"),
            TEXT("/UnrealMCP/"), TEXT("/Plugin/"), TEXT("/Engine/PythonTypes"), TEXT("/Script/UnrealEd"),
            TEXT("/Script/Blutility"), TEXT("/Script/EditorScriptingUtilities"), TEXT("/ModelContextProtocol/"), TEXT("/ToolsetRegistry/")})
        {
            if (Text.Contains(Token, ESearchCase::IgnoreCase)) { Error = TEXT("Property text contains a tool/plugin dependency reference."); return false; }
        }
    }
    else if (Value->Type == EJson::Number && !FMath::IsFinite(Value->AsNumber())) { Error = TEXT("JSON numbers must be finite."); return false; }
    else if (Value->Type == EJson::Array)
    {
        for (const auto& Child : Value->AsArray()) { if (!ValidateJsonReferenceText(Child, Error, Depth + 1)) { return false; } }
    }
    else if (Value->Type == EJson::Object)
    {
        for (const auto& Child : Value->AsObject()->Values) { if (!ValidateJsonReferenceText(Child.Value, Error, Depth + 1)) { return false; } }
    }
    return true;
}

bool ValidatePropertyReferences(const FProperty* Property, const void* Address, UObject* Target, FString& Error, int32& Budget, int32 Depth = 0)
{
    if (--Budget < 0 || Depth > 16) { Error = TEXT("Property reference inspection exceeded its bounded budget."); return false; }
    if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
    {
        const FSoftObjectPtr Value = Soft->GetPropertyValue(Address);
        if (Value.IsNull()) { return true; }
        UObject* Object = Value.LoadSynchronous();
        if (!Object) { Error = TEXT("Soft reference could not be resolved for dependency validation: ") + Value.ToString(); return false; }
        return ValidateReference(Object, Target, Error);
    }
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        return ValidateReference(Object->GetObjectPropertyValue(Address), Target, Error);
    }
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        if (Struct->Struct->GetName() == TEXT("SoftObjectPath") || Struct->Struct->GetName() == TEXT("SoftClassPath"))
        {
            const FSoftObjectPath& Path = *static_cast<const FSoftObjectPath*>(Address);
            if (Path.IsNull()) { return true; }
            UObject* Reference = Path.TryLoad();
            if (!Reference) { Error = TEXT("Soft object path could not be resolved for validation: ") + Path.ToString(); return false; }
            return ValidateReference(Reference, Target, Error);
        }
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
        {
            for (int32 Index = 0; Index < It->ArrayDim; ++Index)
            {
                if (!ValidatePropertyReferences(*It, It->ContainerPtrToValuePtr<void>(Address, Index), Target, Error, Budget, Depth + 1)) { return false; }
            }
        }
    }
    else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Helper(Array, Address);
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            if (!ValidatePropertyReferences(Array->Inner, Helper.GetRawPtr(Index), Target, Error, Budget, Depth + 1)) { return false; }
        }
    }
    else if (const FSetProperty* Set = CastField<FSetProperty>(Property))
    {
        FScriptSetHelper Helper(Set, Address);
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
        {
            if (Helper.IsValidIndex(Index) && !ValidatePropertyReferences(Set->ElementProp, Helper.GetElementPtr(Index), Target, Error, Budget, Depth + 1)) { return false; }
        }
    }
    else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        FScriptMapHelper Helper(Map, Address);
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
        {
            if (Helper.IsValidIndex(Index) &&
                (!ValidatePropertyReferences(Map->KeyProp, Helper.GetKeyPtr(Index), Target, Error, Budget, Depth + 1) ||
                 !ValidatePropertyReferences(Map->ValueProp, Helper.GetValuePtr(Index), Target, Error, Budget, Depth + 1))) { return false; }
        }
    }
    return true;
}

struct FPreparedMaterialProperty
{
    FProperty* Property = nullptr;
    FDefaultConstructedPropertyElement Storage;
    FString ExpectedExport;
};

bool RequiresBoundedMaterialImport(const FProperty* Property, int32 Depth = 0)
{
    // All structs are included: even a struct with no FName value members may
    // parse authored field-name tokens before it reaches those values. Unknown
    // importer types are conservatively bounded as well. We do not execute a
    // speculative import to discover whether an arbitrary token is a name.
    if (!Property || Depth > 16) { return true; }
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        return RequiresBoundedMaterialImport(Array->Inner, Depth + 1);
    }
    if (const FSetProperty* Set = CastField<FSetProperty>(Property))
    {
        return RequiresBoundedMaterialImport(Set->ElementProp, Depth + 1);
    }
    if (const FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        return RequiresBoundedMaterialImport(Map->KeyProp, Depth + 1) || RequiresBoundedMaterialImport(Map->ValueProp, Depth + 1);
    }
    // Plain strings/bools/numbers in containers do not construct FName from
    // their imported values. Enum, struct, name, object and unknown types do.
    if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
    {
        return Numeric->GetIntPropertyEnum() != nullptr;
    }
    return !(CastField<FStrProperty>(Property) || CastField<FBoolProperty>(Property));
}

// Long aggregate imports use the same public token reader as the delimited FName
// importer. This is an admission check, not a replacement ImportText grammar.
bool MaterialAggregateDescriptor(const FProperty* Property,FString& Error,int32& Budget,int32 Depth=0)
{
    if(!Property||--Budget<0||Depth>16){Error=TEXT("Aggregate descriptor exceeds depth16/8192 fields; long raw import remains unsupported.");return false;}
    if(const FArrayProperty* Array=CastField<FArrayProperty>(Property))return MaterialAggregateDescriptor(Array->Inner,Error,Budget,Depth+1);
    if(const FSetProperty* Set=CastField<FSetProperty>(Property))return MaterialAggregateDescriptor(Set->ElementProp,Error,Budget,Depth+1);
    if(const FMapProperty* Map=CastField<FMapProperty>(Property))return MaterialAggregateDescriptor(Map->KeyProp,Error,Budget,Depth+1)&&MaterialAggregateDescriptor(Map->ValueProp,Error,Budget,Depth+1);
    if(const FStructProperty* Struct=CastField<FStructProperty>(Property))
    {
        if(!Struct->Struct||!Struct->Struct->GetOutermost()->GetName().StartsWith(TEXT("/Script/"))
            ||(Struct->Struct->StructFlags&STRUCT_ImportTextItemNative))
        {Error=TEXT("Long raw aggregate imports require native reflected structs without custom ImportTextItem overrides.");return false;}
        for(TFieldIterator<FProperty> It(Struct->Struct);It;++It)
        {
            if(!MaterialAggregateDescriptor(*It,Error,Budget,Depth+1))return false;
        }
        return true;
    }
    if(CastField<FNumericProperty>(Property)||CastField<FEnumProperty>(Property)||CastField<FBoolProperty>(Property)
        ||CastField<FNameProperty>(Property)||CastField<FStrProperty>(Property)||CastField<FObjectPropertyBase>(Property))return true;
    Error=TEXT("Long raw aggregate contains an unaudited leaf importer (including text, delegates or interfaces); existing short imports are unchanged.");return false;
}

// ImportSingleProperty scans an unquoted field through punctuation/whitespace
// until =([.<, not through ReadToken. Bound every possible struct-field start
// after an initializer/separator conservatively before invoking that importer.
bool BoundedMaterialAggregateFieldSpan(const TCHAR* Start,FString& Error)
{
    while(*Start==TEXT(' ')||*Start==TEXT('\t'))++Start;
    if(*Start==TEXT('"'))
    {
        FString Token;const TCHAR* End=FPropertyHelpers::ReadToken(Start,Token,true);
        if(!End||Token.Len()>=NAME_SIZE){Error=TEXT("Aggregate quoted field name is invalid or exceeds NAME_SIZE.");return false;}
        return true;
    }
    const TCHAR* End=Start;
    while(*End&&*End!=TEXT('=')&&*End!=TEXT('(')&&*End!=TEXT('[')&&*End!=TEXT('.')&&*End!=TEXT('<'))
    {
        if(End-Start>=NAME_SIZE)
        {Error=TEXT("Potential aggregate field-name span is unresolved beyond NAME_SIZE; long scalar-array/whitespace variants remain unsupported.");return false;}
        ++End;
    }
    if(*End)
    {
        const TCHAR* Trimmed=End;
        while(Trimmed>Start&&(Trimmed[-1]==TEXT(' ')||Trimmed[-1]==TEXT('\t')))--Trimmed;
        if(Trimmed-Start>=NAME_SIZE)
        {Error=TEXT("Potential aggregate field-name span exceeds NAME_SIZE before ImportSingleProperty; some nested initializer variants remain unsupported.");return false;}
    }
    return true;
}

bool BoundedMaterialAggregateImport(const FProperty* Property,const FString& Import,FString& Error)
{
    if(!(CastField<FArrayProperty>(Property)||CastField<FSetProperty>(Property)||CastField<FMapProperty>(Property)||CastField<FStructProperty>(Property)))
    {Error=TEXT("Long raw import is supported only for reflected aggregate properties.");return false;}
    if(Import.Len()>65536){Error=TEXT("Aggregate raw import exceeds65536 UTF-16 code units.");return false;}
    for(int32 I=0;I<Import.Len();++I)if(Import[I]==0){Error=TEXT("Aggregate raw import contains embedded NUL.");return false;}
    int32 DescriptorBudget=8192;
    if(!MaterialAggregateDescriptor(Property,Error,DescriptorBudget))return false;
    const TCHAR* Begin=*Import;const TCHAR* Cursor=Begin;int32 Depth=0,Tokens=0,Entries=0;
    while(*Cursor)
    {
        if(FChar::IsWhitespace(*Cursor)){++Cursor;continue;}
        if(++Tokens>8192){Error=TEXT("Aggregate raw import exceeds8192 tokens.");return false;}
        // DottedNames=true is also used by FNameProperty::ImportText_Internal.
        // A quoted token is unescaped before its actual FName length is checked.
        FString Token;const TCHAR* Next=FPropertyHelpers::ReadToken(Cursor,Token,true);
        if(!Next){Error=TEXT("Aggregate raw import has an unterminated quoted token.");return false;}
        if(Token.Len()>=NAME_SIZE){Error=TEXT("Aggregate raw import token must be shorter than NAME_SIZE before ImportText/FName construction.");return false;}
        if(Next==Cursor)
        {
            if(*Cursor==TEXT('('))
            {
                // ImportSingleProperty also accepts indexed assignment/Add/Remove
                // syntax and may ExpandForIndex. The long-input profile admits
                // aggregate initializers only, never those imperative forms.
                const TCHAR* Prior=Cursor;
                while(Prior>Begin&&FChar::IsWhitespace(Prior[-1]))--Prior;
                if(Prior>Begin&&Prior[-1]!=TEXT('=')&&Prior[-1]!=TEXT(',')&&Prior[-1]!=TEXT('('))
                {Error=TEXT("Long aggregate raw import does not support indexed assignment or property operations; use an aggregate initializer.");return false;}
                if(++Depth>16){Error=TEXT("Aggregate raw import exceeds16 nested containers.");return false;}
                if(!BoundedMaterialAggregateFieldSpan(Cursor+1,Error))return false;
            }
            else if(*Cursor==TEXT(')'))
            {
                if(--Depth<0){Error=TEXT("Aggregate raw import has an unmatched closing delimiter.");return false;}
            }
            else if(*Cursor==TEXT('[')||*Cursor==TEXT(']'))
            {Error=TEXT("Long aggregate raw import does not support bracket/index syntax.");return false;}
            else if(*Cursor==TEXT(','))
            {
                if(++Entries>8192){Error=TEXT("Aggregate raw import exceeds8192 separators.");return false;}
                if(!BoundedMaterialAggregateFieldSpan(Cursor+1,Error))return false;
            }
            // The public reader returns the same address for a single punctuation
            // token. Advance once; only Unreal's importer decides its grammar.
            ++Cursor;
        }
        else Cursor=Next;
    }
    if(Depth!=0){Error=TEXT("Aggregate raw import has an unmatched opening delimiter.");return false;}
    return true;
}

bool ParseMutationJson(const FString& Json, TSharedPtr<FJsonValue>& Value, FString& Error)
{
    if (Json.IsEmpty() || Json.Len() > 65536) { Error = TEXT("JSON must contain one value and be at most 65536 characters."); return false; }
    // UE58's serializer requires an object/array root even for its FJsonValue
    // overload. An enclosing array admits a primitive without changing its
    // type. The reader rejects trailing input; exactly one element is required.
    TArray<TSharedPtr<FJsonValue>> Values;
    const auto Reader = TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]"));
    if (!FJsonSerializer::Deserialize(Reader, Values) || Values.Num() != 1 || !Values[0].IsValid())
    {
        Error = TEXT("Invalid JSON: provide exactly one value; JSON strings must be quoted and trailing input is not allowed."); return false;
    }
    Value = Values[0];
    return true;
}

bool PrepareMaterialProperty(UObject* Object, UObject* Target, const FString& Name, const TSharedPtr<FJsonValue>& Value, FPreparedMaterialProperty& Out, FString& Error)
{
    if (Name.Len() >= NAME_SIZE)
    {
        Error = TEXT("Property name must be shorter than NAME_SIZE (1024 UTF-16 code units); rejected before FName lookup."); return false;
    }
    FProperty* Property = FindPropertyByNameLoose(Object, Name);
    if (!Property) { Error = TEXT("Property not found: ") + Name; return false; }
    if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_Deprecated | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_ExportObject) || Property->ArrayDim != 1)
    {
        Error = TEXT("Property is not an editable, persistent scalar member: ") + Property->GetName(); return false;
    }
    Out.Property = Property;
    Out.Storage = FDefaultConstructedPropertyElement(Property);
    void* Address = Out.Storage.GetObjAddress();
    Property->CopyCompleteValue(Address, Property->ContainerPtrToValuePtr<void>(Object));
    if (!Value.IsValid()) { Error = TEXT("Missing JSON value."); return false; }
    if (!ValidateJsonReferenceText(Value, Error)) { return false; }
    // JT: this exact two-FString native array has no authored FName tokens.
    // Build scratch directly; all other raw importer NAME_SIZE guards stay below.
    const bool TypedCustomDefines=Value->Type==EJson::Array&&Object->GetClass()==UMaterialExpressionCustom::StaticClass()
        &&Property==FindFProperty<FArrayProperty>(UMaterialExpressionCustom::StaticClass(),GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom,AdditionalDefines));
    if(TypedCustomDefines)
    {
        const FArrayProperty* Array=CastFieldChecked<FArrayProperty>(Property);const FStructProperty* Inner=CastField<FStructProperty>(Array->Inner);
        const UScriptStruct* Struct=Inner?Inner->Struct.Get():nullptr;
        const FStrProperty* DefineName=Struct?FindFProperty<FStrProperty>(Struct,TEXT("DefineName")):nullptr;
        const FStrProperty* DefineValue=Struct?FindFProperty<FStrProperty>(Struct,TEXT("DefineValue")):nullptr;
        int32 Fields=0;bool ExactFields=true;
        if(Struct){for(TFieldIterator<FProperty> It(Struct,EFieldIteratorFlags::IncludeSuper);It;++It){++Fields;ExactFields&=*It==DefineName||*It==DefineValue;}}
        if(!Struct||Struct->GetClass()!=UScriptStruct::StaticClass()||Struct->GetPathName()!=TEXT("/Script/Engine.CustomDefine")||!(Struct->StructFlags&STRUCT_Native)||Struct->GetSuperStruct()
            ||Struct->GetStructureSize()!=sizeof(FCustomDefine)||Struct->GetMinAlignment()!=alignof(FCustomDefine)||Inner->ArrayDim!=1||Inner->GetElementSize()!=sizeof(FCustomDefine)
            ||Fields!=2||!ExactFields||!DefineName||!DefineValue||DefineName->GetOwnerStruct()!=Struct||DefineValue->GetOwnerStruct()!=Struct||DefineName->ArrayDim!=1||DefineValue->ArrayDim!=1
            ||DefineName->GetOffset_ForInternal()!=STRUCT_OFFSET(FCustomDefine,DefineName)||DefineValue->GetOffset_ForInternal()!=STRUCT_OFFSET(FCustomDefine,DefineValue)
            ||DefineName->GetElementSize()!=sizeof(FString)||DefineValue->GetElementSize()!=sizeof(FString)
            ||Property->ContainerPtrToValuePtr<void>(Object)!=&static_cast<UMaterialExpressionCustom*>(Object)->AdditionalDefines)
        {Error=TEXT("Exact native Custom.AdditionalDefines two-FString array layout required for typed conversion.");return false;}
        const auto& Rows=Value->AsArray();if(Rows.Num()>64){Error=TEXT("Typed AdditionalDefines accepts at most 64 canonical objects.");return false;}
        TArray<FCustomDefine> Defines;Defines.Reserve(Rows.Num());
        for(int32 Index=0;Index<Rows.Num();++Index)
        {
            if(!Rows[Index].IsValid()||Rows[Index]->Type!=EJson::Object){Error=TEXT("Typed AdditionalDefines entries must be canonical objects with exactly DefineName and DefineValue strings.");return false;}
            const auto Row=Rows[Index]->AsObject();TSharedPtr<FJsonValue> NameValue,TextValue;
            if(Row.IsValid()){NameValue=Row->TryGetField(TEXT("DefineName"));TextValue=Row->TryGetField(TEXT("DefineValue"));}
            if(!Row.IsValid()||Row->Values.Num()!=2||!NameValue.IsValid()||!TextValue.IsValid()||NameValue->Type!=EJson::String||TextValue->Type!=EJson::String)
            {Error=TEXT("Typed AdditionalDefines requires exactly case-sensitive DefineName and DefineValue string fields; aliases, missing/unknown fields and other value types are not admitted.");return false;}
            FCustomDefine& Define=Defines.AddDefaulted_GetRef();Define.DefineName=NameValue->AsString();Define.DefineValue=TextValue->AsString();
        }
        *static_cast<TArray<FCustomDefine>*>(Address)=MoveTemp(Defines);
        // Fall through to the same shader/directive/reference/export audits.
    }
    else if (Value->Type == EJson::Array && Object->GetClass() == UMaterialExpressionCustom::StaticClass()
        && Property == FindFProperty<FArrayProperty>(UMaterialExpressionCustom::StaticClass(),
            GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, IncludeFilePaths)))
    {
        // JSON array spelling is not UE ImportText array spelling. Build only this
        // exact native FString array in scratch; raw string imports keep the old path.
        const FArrayProperty* Array = CastFieldChecked<FArrayProperty>(Property);
        const FProperty* Inner = Array->Inner;
        if (Array->GetClass() != FArrayProperty::StaticClass() || !Inner || Inner->GetClass() != FStrProperty::StaticClass()
            || Array->GetOwnerStruct() != UMaterialExpressionCustom::StaticClass()
            || EnumHasAnyFlags(Array->ArrayFlags, EArrayPropertyFlags::UsesMemoryImageAllocator)
            || Array->GetElementSize() != sizeof(TArray<FString>) || Array->GetMinAlignment() != alignof(TArray<FString>)
            || Inner->GetOwnerProperty() != Array || Inner->ArrayDim != 1 || Inner->GetOffset_ForInternal() != 0
            || Inner->GetElementSize() != sizeof(FString) || Inner->GetMinAlignment() != alignof(FString)
            || Property->ContainerPtrToValuePtr<void>(Object) != &static_cast<UMaterialExpressionCustom*>(Object)->IncludeFilePaths)
        { Error = TEXT("Exact native Custom.IncludeFilePaths FString array layout required for typed conversion."); return false; }
        const auto& Rows = Value->AsArray();
        TArray<FString> IncludePaths;
        IncludePaths.Reserve(Rows.Num());
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            if (!Rows[Index].IsValid() || Rows[Index]->Type != EJson::String)
            {
                Error = FString::Printf(TEXT("Typed IncludeFilePaths entry at index %d must be a JSON string."), Index);
                return false;
            }
            // Preserve actual order, duplicates and string payload; shared shader
            // admission below decides whether each unchanged virtual path is supported.
            IncludePaths.Add(Rows[Index]->AsString());
        }
        *static_cast<TArray<FString>*>(Address) = MoveTemp(IncludePaths);
    }
    else if (FBoolProperty* Bool = CastField<FBoolProperty>(Property))
    {
        if (Value->Type != EJson::Boolean) { Error = TEXT("Boolean property requires JSON true/false."); return false; }
        Bool->SetPropertyValue(Address, Value->AsBool());
    }
    else if (FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    {
        const int64 Number = Value->Type == EJson::String ? Enum->GetEnum()->GetValueByNameString(Value->AsString()) :
            Value->Type == EJson::Number && FMath::IsFinite(Value->AsNumber()) && Value->AsNumber() == FMath::FloorToDouble(Value->AsNumber()) && FMath::Abs(Value->AsNumber()) < 9007199254740992.0 ? static_cast<int64>(Value->AsNumber()) : INDEX_NONE;
        if (!Enum->GetEnum()->IsValidEnumValue(Number)) { Error = TEXT("Invalid enum value: ") + Name; return false; }
        Enum->GetUnderlyingProperty()->SetIntPropertyValue(Address, Number);
    }
    else if (FByteProperty* Byte = CastField<FByteProperty>(Property); Byte && Byte->GetIntPropertyEnum())
    {
        UEnum* ByteEnum = Byte->GetIntPropertyEnum();
        const int64 Number = Value->Type == EJson::String ? ByteEnum->GetValueByNameString(Value->AsString()) :
            Value->Type == EJson::Number && FMath::IsFinite(Value->AsNumber()) && Value->AsNumber() >= 0 && Value->AsNumber() <= 255 && Value->AsNumber() == FMath::FloorToDouble(Value->AsNumber()) ? static_cast<int64>(Value->AsNumber()) : INDEX_NONE;
        if (!ByteEnum->IsValidEnumValue(Number)) { Error = TEXT("Invalid byte enum value: ") + Name; return false; }
        Byte->SetPropertyValue(Address, static_cast<uint8>(Number));
    }
    else if (FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
    {
        if (Value->Type != EJson::Number || !FMath::IsFinite(Value->AsNumber())) { Error = TEXT("Numeric property requires a finite number: ") + Name; return false; }
        const double Number = Value->AsNumber();
        if (Numeric->IsInteger())
        {
            if (FMath::Abs(Number) >= 9007199254740992.0 || Number != FMath::FloorToDouble(Number)) { Error = TEXT("Integer property requires an exactly representable JSON integer."); return false; }
            const int64 Integer = static_cast<int64>(Number);
            if (!Numeric->CanHoldValue(Integer)) { Error = TEXT("Integer is outside property range: ") + Name; return false; }
            Numeric->SetIntPropertyValue(Address, Integer);
        }
        else
        {
            if (!Numeric->CanHoldValue(Number)) { Error = TEXT("Number is outside property range: ") + Name; return false; }
            Numeric->SetFloatingPointPropertyValue(Address, Number);
        }
    }
    else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
    {
        if (Value->Type != EJson::String) { Error = TEXT("Name property requires a string."); return false; }
        if (Value->AsString().Len() >= NAME_SIZE)
        {
            Error = TEXT("FName value must be shorter than NAME_SIZE (1024 UTF-16 code units); rejected before FName construction."); return false;
        }
        NameProperty->SetPropertyValue(Address, FName(*Value->AsString()));
    }
    else if (FStrProperty* String = CastField<FStrProperty>(Property))
    {
        if (Value->Type != EJson::String) { Error = TEXT("String property requires a string."); return false; }
        String->SetPropertyValue(Address, Value->AsString());
    }
    else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
    {
        if (Value->Type != EJson::String) { Error = TEXT("Text property requires a string."); return false; }
        TextProperty->SetPropertyValue(Address, FText::FromString(Value->AsString()));
    }
    else if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
    {
        if (Value->Type != EJson::String && Value->Type != EJson::Null) { Error = TEXT("Object property requires an asset/class path string or null."); return false; }
        UObject* Reference = nullptr;
        if (Value->Type == EJson::String && !Value->AsString().IsEmpty())
        {
            Reference = LoadObjectValue(Value->AsString());
            if (!Reference) { Error = TEXT("Referenced object not found: ") + Value->AsString(); return false; }
        }
        if (Reference && !Reference->IsA(ObjectProperty->PropertyClass)) { Error = TEXT("Referenced object has the wrong class for ") + Name; return false; }
        if (FClassProperty* ClassProperty = CastField<FClassProperty>(Property); ClassProperty && Reference && !CastChecked<UClass>(Reference)->IsChildOf(ClassProperty->MetaClass))
        {
            Error = TEXT("Referenced class does not satisfy property MetaClass."); return false;
        }
        if (FSoftClassProperty* ClassProperty = CastField<FSoftClassProperty>(Property); ClassProperty && Reference && !CastChecked<UClass>(Reference)->IsChildOf(ClassProperty->MetaClass))
        {
            Error = TEXT("Referenced soft class does not satisfy property MetaClass."); return false;
        }
        if (!ValidateReference(Reference, Target, Error)) { return false; }
        ObjectProperty->SetObjectPropertyValue(Address, Reference);
    }
    else
    {
        FString Import = JsonValueToImportText(Value);
        if (FStructProperty* Struct = CastField<FStructProperty>(Property))
        {
            const FString Type = Struct->Struct->GetName();
            if (Type == TEXT("Guid"))
            {
                FGuid Guid;
                if (Value->Type != EJson::String || !FGuid::Parse(Value->AsString(), Guid)) { Error = TEXT("Guid requires a valid GUID string."); return false; }
                *static_cast<FGuid*>(Address) = Guid;
                Import.Reset();
            }
            else if ((Value->Type == EJson::Array || Value->Type == EJson::Object) &&
                (Type == TEXT("LinearColor") || Type == TEXT("Color") || Type == TEXT("Vector") || Type == TEXT("Vector2D") || Type == TEXT("Vector4")))
            {
                if (Value->Type == EJson::Array)
                {
                    for (const auto& Component : Value->AsArray())
                    {
                        if (!Component.IsValid() || Component->Type != EJson::Number || !FMath::IsFinite(Component->AsNumber())) { Error = TEXT("Vector/color array components must be finite numbers."); return false; }
                    }
                }
                else
                {
                    for (const auto& Component : Value->AsObject()->Values)
                    {
                        if (!Component.Value.IsValid() || Component.Value->Type != EJson::Number || !FMath::IsFinite(Component.Value->AsNumber())) { Error = TEXT("Vector/color object components must be finite numbers."); return false; }
                    }
                }
                Import = JsonValueToStructImportText(Value, Type);
            }
        }
        if (!Import.IsEmpty())
        {
            if (RequiresBoundedMaterialImport(Property) && Import.Len() >= NAME_SIZE)
            {
                if(!BoundedMaterialAggregateImport(Property,Import,Error))return false;
            }
            const TCHAR* End = Property->ImportText_Direct(*Import, Address, Object, PPF_None);
            if (!End || !FString(End).TrimStartAndEnd().IsEmpty()) { Error = TEXT("Property import failed or left trailing input: ") + Name; return false; }
        }
        else if (!(CastField<FStructProperty>(Property) && CastField<FStructProperty>(Property)->Struct->GetName() == TEXT("Guid")))
        {
            Error = TEXT("Unsupported empty property import: ") + Name; return false;
        }
    }
    if (!ValidateCustomShaderProperty(Object, Property, Address, Error)) { return false; }
    int32 Budget = 8192;
    if (!ValidatePropertyReferences(Property, Address, Target, Error, Budget)) { return false; }
    Property->ExportTextItem_Direct(Out.ExpectedExport, Address, nullptr, Object, PPF_None);
    return true;
}

bool ReadMutationProperties(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& Properties, FString& Error)
{
    FString Json;
    Params->TryGetStringField(TEXT("properties_json"), Json);
    TSharedPtr<FJsonValue> Value;
    if (!ParseMutationJson(Json.IsEmpty() ? TEXT("{}") : Json, Value, Error)) { return false; }
    if (Value->Type != EJson::Object) { Error = TEXT("properties_json must encode a JSON object."); return false; }
    Properties = Value->AsObject();
    if (Properties->Values.Num() > 64) { Error = TEXT("At most 64 properties may be assigned per added node."); return false; }
    return true;
}

bool ApplyPreparedProperty(UMaterialExpression* Expression, const FPreparedMaterialProperty& Prepared, FString& Error)
{
    Expression->Modify(false);
    if (UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);
        Call && Prepared.Property->GetFName() == TEXT("MaterialFunction"))
    {
        FObjectPropertyBase* ObjectProperty = CastFieldChecked<FObjectPropertyBase>(Prepared.Property);
        UMaterialFunctionInterface* Function = Cast<UMaterialFunctionInterface>(ObjectProperty->GetObjectPropertyValue(Prepared.Storage.GetObjAddress()));
        if (!Call->SetMaterialFunction(Function)) { Error = TEXT("SetMaterialFunction rejected the value; inspect the actual graph before retry."); return false; }
        Call->UpdateFromFunctionResource(true);
    }
    else { Prepared.Property->CopyCompleteValue(Prepared.Property->ContainerPtrToValuePtr<void>(Expression), Prepared.Storage.GetObjAddress()); }
    if (!Prepared.Property->Identical(Prepared.Property->ContainerPtrToValuePtr<void>(Expression), Prepared.Storage.GetObjAddress(), PPF_None))
    {
        Error = TEXT("Assigned property did not read back as the converted requested value."); return false;
    }
    return true;
}

TSharedPtr<FJsonObject> HandleCubelessSetMaterialNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    FMaterialGraphTarget Target;
    FString Error;
    if (!ResolveMaterialGraph(Params->GetStringField(TEXT("material_path")), Params->GetStringField(TEXT("graph_type")), Target, nullptr, Error)) { return MaterialApiError(Error); }
    UMaterialExpression* Expression = nullptr;
    if (!FindExpressionById(Target, Params->GetStringField(TEXT("node_id")), Expression, Error)) { return MaterialApiError(Error); }
    TSharedPtr<FJsonValue> Value;
    FPreparedMaterialProperty Prepared;
    if (!ParseMutationJson(Params->GetStringField(TEXT("value_json")), Value, Error) ||
        !PrepareMaterialProperty(Expression, Target.Asset, Params->GetStringField(TEXT("property_name")), Value, Prepared, Error)) { return MaterialApiError(Error); }
    const bool bApplied = ApplyPreparedProperty(Expression, Prepared, Error);
    if (bApplied)
    {
        if (!Target.HasUnconnectedCustomInputs() && !ShouldDeferImmediateMaterialExpressionUpdate(Expression)) { RefreshExpressionNodeSafe(Expression); }
        Target.MarkChanged(!ShouldDeferImmediateMaterialExpressionUpdate(Expression));
    }
    auto Result = ExpressionToJson(Expression, true, Target.GetExpressions());
    Result->SetStringField(TEXT("updated_property"), Prepared.Property->GetName());
    Result->SetStringField(TEXT("expected_property_export"), Prepared.ExpectedExport);
    FString ActualExport;
    Prepared.Property->ExportTextItem_Direct(ActualExport, Prepared.Property->ContainerPtrToValuePtr<void>(Expression), nullptr, Expression, PPF_None);
    Result->SetStringField(TEXT("actual_property_export"), ActualExport);
    Result->SetBoolField(TEXT("property_write_attempted"), true);
    Result->SetBoolField(TEXT("success"), bApplied);
    if (!bApplied) { Result->SetStringField(TEXT("error"), Error); }
    return Result;
}

TSharedPtr<FJsonObject> HandleCubelessAddMaterialNode(const TSharedPtr<FJsonObject>& Params)
{
    FMaterialGraphTarget Target;
    FString Error;
    if (!ResolveMaterialGraph(Params->GetStringField(TEXT("material_path")), Params->GetStringField(TEXT("graph_type")), Target, nullptr, Error)) { return MaterialApiError(Error); }
    UClass* Class = FindMaterialExpressionClass(Params->GetStringField(TEXT("expression_class")));
    TSharedPtr<FJsonObject> Properties;
    if (!Class || !ReadMutationProperties(Params, Properties, Error)) { return MaterialApiError(Error); }
    FString SelectedPath;
    Params->TryGetStringField(TEXT("selected_asset"), SelectedPath);
    UObject* Selected = SelectedPath.IsEmpty() ? nullptr : LoadObjectValue(SelectedPath);
    UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpressionEx(Target.Material, Target.Function, Class, Selected,
        static_cast<int32>(Params->GetNumberField(TEXT("node_x"))), static_cast<int32>(Params->GetNumberField(TEXT("node_y"))));
    if (!Expression) { return MaterialApiError(TEXT("CreateMaterialExpressionEx returned no node; inspect after snapshot.")); }
    const FString CreatedKey = GetExpressionKey(Expression);
    TArray<TSharedPtr<FJsonValue>> Applied;
    TArray<FString> Names;
    for (const auto& Pair : Properties->Values) { Names.Add(FString(Pair.Key.ToView())); }
    Names.Sort();
    for (const FString& Name : Names)
    {
        FPreparedMaterialProperty Prepared;
        if (!PrepareMaterialProperty(Expression, Target.Asset, Name, Properties->TryGetField(Name), Prepared, Error) || !ApplyPreparedProperty(Expression, Prepared, Error)) { break; }
        Applied.Add(MakeShared<FJsonValueString>(Prepared.Property->GetName()));
    }
    if (!Error.IsEmpty())
    {
        if (Target.Material) { UMaterialEditingLibrary::DeleteMaterialExpression(Target.Material, Expression); }
        else { UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(Target.Function, Expression); }
        bool bAbsent = true;
        for (const auto& Node : Target.GetExpressions()) { if (Node == Expression) { bAbsent = false; } }
        auto Result = MaterialApiError(Error);
        Result->SetStringField(TEXT("created_node_key"), CreatedKey);
        Result->SetBoolField(TEXT("transient_graph_changes_occurred"), true);
        Result->SetArrayField(TEXT("properties_applied_before_failure"), Applied);
        Result->SetBoolField(TEXT("cleanup_attempted"), true);
        Result->SetBoolField(TEXT("cleanup_node_absent"), bAbsent);
        Result->SetStringField(TEXT("cleanup_scope"), TEXT("new expression removed from graph collection only; not atomic package rollback"));
        return Result;
    }
    bool bCustomBuilder = false;
    Params->TryGetBoolField(TEXT("cubeless_custom_builder"), bCustomBuilder);
    bool bCustomGraphReconstructed = false;
    if (bCustomBuilder)
    {
        // Same public operations used by Custom::PostEditChangeProperty, without
        // its Super call that can submit a material recompile. All fields were
        // assigned and checked before this one output/pin topology refresh.
        UMaterialExpressionCustom* Custom = CastChecked<UMaterialExpressionCustom>(Expression);
        Custom->RebuildOutputs();
        if (Custom->GraphNode)
        {
            Custom->GraphNode->ReconstructNode();
            bCustomGraphReconstructed = true;
        }
    }
    const bool bDefer = ShouldDeferImmediateMaterialExpressionUpdate(Expression);
    if (!bDefer && !Target.HasUnconnectedCustomInputs()) { RefreshExpressionNodeSafe(Expression); }
    Target.MarkChanged(!bDefer);
    auto Result = ExpressionToJson(Expression, true, Target.GetExpressions());
    Result->SetStringField(TEXT("material"), Target.GetPathName());
    Result->SetStringField(TEXT("graph_type"), Target.GetGraphType());
    Result->SetArrayField(TEXT("properties_applied"), Applied);
    Result->SetBoolField(TEXT("cleanup_attempted"), false);
    if (bCustomBuilder)
    {
        UMaterialExpressionCustom* Custom = CastChecked<UMaterialExpressionCustom>(Expression);
        Result->SetBoolField(TEXT("created_custom_node"), true);
        Result->SetNumberField(TEXT("input_count"), Custom->Inputs.Num());
        Result->SetStringField(TEXT("requested_output_type"), Params->GetStringField(TEXT("output_type")));
        Result->SetBoolField(TEXT("custom_outputs_rebuilt"), true);
        Result->SetBoolField(TEXT("custom_graph_node_reconstructed"), bCustomGraphReconstructed);
        Result->SetStringField(TEXT("custom_shader_include_scope"), TEXT("bounded literal shader include DAG in Code/IncludeFilePaths via RenderCore mapping and nonfatal load; child-relative literals and exact file-side pragma once supported without changing authored text; computed/generated/platform/future files, guarded cycles, alternate/import/embed and other pragmas remain valid incomplete; authoring only, shader compilation/rendering and complete cache effects not verified"));
        Result->SetBoolField(TEXT("custom_shader_code_compiled"), false);
    }
    return Result;
}
