#include "Compatibility/UE58/CubelessParticleReceiverApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EdGraphSchema_K2.h"
#include "HAL/PlatformProcess.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::ParticleReceiver
{
namespace
{
struct FTarget
{
    const TCHAR* ClassPath;
    const TCHAR* PackageName;
    const TCHAR* FunctionName;
    bool bInterface;
};

constexpr FTarget Targets[] = {
    {TEXT("/Script/Niagara.NiagaraParticleCallbackHandler"), TEXT("/Script/Niagara"), TEXT("ReceiveParticleData"), true},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("/Script/Engine"), TEXT("Array_Length"), false},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("/Script/Engine"), TEXT("Array_Get"), false},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("/Script/Engine"), TEXT("Add_VectorVector"), false},
};

bool Fail(Function::FResult& R, const TCHAR* Code, const TCHAR* Message)
{
    R.ErrorCode = Code; R.Error = Message; return false;
}

FString Text(const TSharedRef<FJsonObject>& Object)
{
    FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out;
}

bool FullyLoaded(const UObject* Object)
{
    return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects);
}

bool RuntimePackage(const UPackage* Package, const FString& ExpectedName)
{
    return IsValid(Package) && Package->GetName() == ExpectedName
        && !Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly);
}

bool ExactNativeClass(const UClass* Class, const FString& Path, const FString& PackageName, bool bInterface)
{
    return FullyLoaded(Class) && Class->GetPathName() == Path && Class->HasAnyClassFlags(CLASS_Native)
        && Class->HasAnyClassFlags(CLASS_Interface) == bInterface
        && !Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) && !Class->IsEditorOnly()
        && RuntimePackage(Class->GetOutermost(), PackageName);
}

TSharedRef<FJsonObject> PackageState(const UPackage* Package)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("package_name"), Package ? Package->GetName() : FString());
    Row->SetStringField(TEXT("package_flags_string"), Package ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    Row->SetBoolField(TEXT("package_dirty"), Package && Package->IsDirty());
    return Row;
}

TSharedRef<FJsonObject> DescribeProperty(const FProperty* Property, const UEdGraphSchema_K2* Schema, int32 Depth = 0)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), Property->GetName());
    Row->SetStringField(TEXT("path"), Property->GetPathName());
    Row->SetStringField(TEXT("field_class"), Property->GetClass()->GetName());
    Row->SetStringField(TEXT("cpp_type"), Property->GetCPPType());
    Row->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
    Row->SetNumberField(TEXT("array_dim"), Property->ArrayDim);
    Row->SetBoolField(TEXT("parm"), Property->HasAnyPropertyFlags(CPF_Parm));
    Row->SetBoolField(TEXT("return_parm"), Property->HasAnyPropertyFlags(CPF_ReturnParm));
    Row->SetBoolField(TEXT("out_parm"), Property->HasAnyPropertyFlags(CPF_OutParm));
    Row->SetBoolField(TEXT("reference_parm"), Property->HasAnyPropertyFlags(CPF_ReferenceParm));
    Row->SetBoolField(TEXT("const_parm"), Property->HasAnyPropertyFlags(CPF_ConstParm));
    Row->SetBoolField(TEXT("blueprint_visible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
    Row->SetBoolField(TEXT("blueprint_read_only"), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
    Row->SetBoolField(TEXT("edit_const"), Property->HasAnyPropertyFlags(CPF_EditConst));
    Row->SetBoolField(TEXT("editor_only"), Property->HasAnyPropertyFlags(CPF_EditorOnly));
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    { Row->SetStringField(TEXT("struct_path"), Struct->Struct ? Struct->Struct->GetPathName() : FString()); }
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    { Row->SetStringField(TEXT("object_class_path"), Object->PropertyClass ? Object->PropertyClass->GetPathName() : FString()); }
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        Row->SetBoolField(TEXT("array_inner_present"), Array->Inner != nullptr);
        if (Array->Inner && Depth == 0) { Row->SetObjectField(TEXT("array_inner"), DescribeProperty(Array->Inner, Schema, Depth + 1)); }
    }
    FEdGraphPinType Type;
    const bool Converted = Schema->ConvertPropertyToPinType(Property, Type);
    Row->SetBoolField(TEXT("schema_conversion_success"), Converted);
    if (Converted)
    {
        FString TypeText;
        FEdGraphPinType::StaticStruct()->ExportText(TypeText, &Type, &Type, nullptr, PPF_None, nullptr);
        Row->SetStringField(TEXT("pin_type_text"), TypeText);
        Row->SetStringField(TEXT("pin_category"), Type.PinCategory.ToString());
        Row->SetStringField(TEXT("pin_subcategory"), Type.PinSubCategory.ToString());
        Row->SetStringField(TEXT("pin_type_object"), Type.PinSubCategoryObject.IsValid() ? Type.PinSubCategoryObject->GetPathName() : FString());
        Row->SetStringField(TEXT("pin_container_type"), Type.ContainerType == EPinContainerType::Array ? TEXT("array")
            : Type.ContainerType == EPinContainerType::Set ? TEXT("set") : Type.ContainerType == EPinContainerType::Map ? TEXT("map") : TEXT("none"));
        Row->SetBoolField(TEXT("pin_is_reference"), Type.bIsReference);
        Row->SetBoolField(TEXT("pin_is_const"), Type.bIsConst);
        Row->SetBoolField(TEXT("pin_is_weak_pointer"), Type.bIsWeakPointer);
    }
    return Row;
}

TSharedRef<FJsonObject> Observe(const UEdGraphSchema_K2* Schema)
{
    auto State = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Functions;
    bool Complete = true;
    for (const FTarget& Target : Targets)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("requested_class_path"), Target.ClassPath);
        Row->SetStringField(TEXT("requested_function_name"), Target.FunctionName);
        UClass* Owner = FindObject<UClass>(nullptr, Target.ClassPath);
        const bool OwnerValid = ExactNativeClass(Owner, Target.ClassPath, Target.PackageName, Target.bInterface);
        Row->SetBoolField(TEXT("exact_loaded_native_owner"), OwnerValid);
        UFunction* Function = OwnerValid ? Owner->FindFunctionByName(FName(Target.FunctionName)) : nullptr;
        const bool FunctionValid = FullyLoaded(Function) && Function->GetOuterUClass() == Owner
            && Function->GetName() == Target.FunctionName && Function->GetOutermost() == Owner->GetOutermost();
        Row->SetBoolField(TEXT("exact_declared_function"), FunctionValid);
        if (!FunctionValid) { Complete = false; Functions.Add(MakeShared<FJsonValueObject>(Row)); continue; }
        Row->SetStringField(TEXT("function_path"), Function->GetPathName());
        Row->SetStringField(TEXT("declaring_class_path"), Function->GetOuterUClass()->GetPathName());
        Row->SetStringField(TEXT("class_flags_string"), LexToString(static_cast<uint32>(Owner->ClassFlags)));
        Row->SetStringField(TEXT("function_flags_string"), LexToString(static_cast<uint32>(Function->FunctionFlags)));
        Row->SetBoolField(TEXT("native"), Function->HasAnyFunctionFlags(FUNC_Native));
        Row->SetBoolField(TEXT("public"), Function->HasAnyFunctionFlags(FUNC_Public));
        Row->SetBoolField(TEXT("blueprint_callable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
        Row->SetBoolField(TEXT("blueprint_pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
        Row->SetBoolField(TEXT("blueprint_event"), Function->HasAnyFunctionFlags(FUNC_BlueprintEvent));
        Row->SetBoolField(TEXT("static"), Function->HasAnyFunctionFlags(FUNC_Static));
        Row->SetBoolField(TEXT("const"), Function->HasAnyFunctionFlags(FUNC_Const));
        Row->SetBoolField(TEXT("editor_only"), Function->HasAnyFunctionFlags(FUNC_EditorOnly));
        Row->SetObjectField(TEXT("package"), PackageState(Owner->GetOutermost()));
        for (const TCHAR* Key : {TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("AutoCreateRefTerm"),
            TEXT("BlueprintInternalUseOnly"), TEXT("CustomThunk"), TEXT("CommutativeAssociativeBinaryOperator"),
            TEXT("Latent"), TEXT("DeprecatedFunction"), TEXT("BlueprintThreadSafe"), TEXT("WorldContext"), TEXT("ScriptName")})
        { Row->SetStringField(Key, Function->GetMetaData(Key)); }
        Row->SetBoolField(TEXT("can_be_placed_as_event"), UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function));
        Row->SetBoolField(TEXT("can_kismet_override"), UEdGraphSchema_K2::CanKismetOverrideFunction(Function));
        TArray<TSharedPtr<FJsonValue>> Parameters;
        int32 Count = 0;
        for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            if (++Count > 128) { Complete = false; break; }
            auto Parameter = DescribeProperty(*It, Schema);
            const bool Input = !It->HasAnyPropertyFlags(CPF_ReturnParm)
                && (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ReferenceParm));
            Parameter->SetNumberField(TEXT("ordinal"), Count - 1);
            Parameter->SetStringField(TEXT("k2_call_pin_direction"), Input ? TEXT("input") : TEXT("output"));
            bool Converted = false; Parameter->TryGetBoolField(TEXT("schema_conversion_success"), Converted);
            Complete &= Converted;
            Parameters.Add(MakeShared<FJsonValueObject>(Parameter));
        }
        Row->SetNumberField(TEXT("parameter_count_observed"), Count);
        Row->SetArrayField(TEXT("parameters"), Parameters);
        Functions.Add(MakeShared<FJsonValueObject>(Row));
    }
    State->SetArrayField(TEXT("functions"), Functions);

    auto StructRow = MakeShared<FJsonObject>();
    StructRow->SetStringField(TEXT("requested_struct_path"), TEXT("/Script/Niagara.BasicParticleData"));
    UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/Niagara.BasicParticleData"));
    const bool StructValid = FullyLoaded(Struct) && Struct->GetPathName() == TEXT("/Script/Niagara.BasicParticleData")
        && (Struct->StructFlags & STRUCT_Native) && !Struct->IsEditorOnly()
        && RuntimePackage(Struct->GetOutermost(), TEXT("/Script/Niagara"));
    StructRow->SetBoolField(TEXT("exact_loaded_native_struct"), StructValid);
    if (StructValid)
    {
        StructRow->SetStringField(TEXT("struct_flags_string"), LexToString(static_cast<uint32>(Struct->StructFlags)));
        StructRow->SetNumberField(TEXT("structure_size"), Struct->GetStructureSize());
        StructRow->SetObjectField(TEXT("package"), PackageState(Struct->GetOutermost()));
        for (const TCHAR* Key : {TEXT("BlueprintType"), TEXT("Blueprintable"), TEXT("NativeBreakFunction"), TEXT("NativeMakeFunction")})
        { StructRow->SetStringField(Key, Struct->GetMetaData(Key)); }
        TArray<TSharedPtr<FJsonValue>> Fields;
        int32 Count = 0;
        for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            if (++Count > 32) { Complete = false; break; }
            auto Field = DescribeProperty(*It, Schema);
            Field->SetNumberField(TEXT("ordinal"), Count - 1);
            Field->SetBoolField(TEXT("exact_declared_owner"), It->GetOwnerStruct() == Struct);
            bool Converted = false; Field->TryGetBoolField(TEXT("schema_conversion_success"), Converted);
            Complete &= Converted && It->GetOwnerStruct() == Struct;
            Fields.Add(MakeShared<FJsonValueObject>(Field));
        }
        StructRow->SetNumberField(TEXT("field_count_observed"), Count);
        StructRow->SetArrayField(TEXT("fields"), Fields);
    }
    else { Complete = false; }
    State->SetObjectField(TEXT("basic_particle_data"), StructRow);

    auto SystemRow = MakeShared<FJsonObject>();
    SystemRow->SetStringField(TEXT("requested_class_path"), TEXT("/Script/Niagara.NiagaraSystem"));
    UClass* System = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraSystem"));
    const bool SystemValid = ExactNativeClass(System, TEXT("/Script/Niagara.NiagaraSystem"), TEXT("/Script/Niagara"), false);
    SystemRow->SetBoolField(TEXT("exact_loaded_native_owner"), SystemValid);
    if (SystemValid)
    {
        SystemRow->SetStringField(TEXT("class_flags_string"), LexToString(static_cast<uint32>(System->ClassFlags)));
        SystemRow->SetObjectField(TEXT("package"), PackageState(System->GetOutermost()));
    }
    else { Complete = false; }
    State->SetObjectField(TEXT("niagara_system"), SystemRow);
    State->SetBoolField(TEXT("observations_complete"), Complete);
    return State;
}
}

Function::FResult InspectNativeSignatures()
{
    Function::FResult R;
    const FEngineVersion& Version = FEngineVersion::Current();
    R.Data->SetStringField(TEXT("engine_version"), Version.ToString());
    R.Data->SetStringField(TEXT("engine_directory"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    R.Data->SetStringField(TEXT("project_file"), FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
    R.Data->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
    R.Data->SetStringField(TEXT("scope"), TEXT("exact already-loaded native particle callback, BasicParticleData/NiagaraSystem, Array_Length, Array_Get and Add_VectorVector; signature/metadata/schema evidence only, not operation compatibility or runtime proof"));
    R.Data->SetBoolField(TEXT("asset_selectors_accepted"), false);
    R.Data->SetBoolField(TEXT("object_or_cdo_creation_requested"), false);
    R.Data->SetBoolField(TEXT("native_function_invocation_requested"), false);
    R.Data->SetBoolField(TEXT("asset_or_module_load_requested"), false);
    R.Data->SetBoolField(TEXT("node_creation_requested"), false);
    R.Data->SetBoolField(TEXT("private_property_read_or_write_requested"), false);
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Fail(R, TEXT("particle_signature_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); return R; }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8)
    { Fail(R, TEXT("particle_signature_version_unsupported"), TEXT("This evidence adapter is scoped to installed UE 5.8.x.")); return R; }
    UClass* SchemaClass = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.EdGraphSchema_K2"));
    const UEdGraphSchema_K2* Schema = FullyLoaded(SchemaClass)
        ? Cast<UEdGraphSchema_K2>(SchemaClass->GetDefaultObject(false)) : nullptr;
    if (!Schema || Schema->GetClass() != SchemaClass)
    { Fail(R, TEXT("particle_signature_schema_not_loaded"), TEXT("Requires an already-loaded exact standard K2 schema CDO; this read never creates it.")); return R; }
    const auto Before = Observe(Schema);
    const auto After = Observe(Schema);
    R.Data->SetObjectField(TEXT("before"), Before); R.Data->SetObjectField(TEXT("after"), After);
    R.bChanged = Text(Before) != Text(After);
    R.Data->SetBoolField(TEXT("observed_native_state_preserved"), !R.bChanged);
    if (R.bChanged)
    { R.Stage = TEXT("verification"); Fail(R, TEXT("particle_signature_read_state_changed"), TEXT("Finite native signature/field/package observations changed during the read; no modification was requested.")); return R; }
    if (!Before->GetBoolField(TEXT("observations_complete")) || !After->GetBoolField(TEXT("observations_complete")))
    { Fail(R, TEXT("particle_signature_observation_incomplete"), TEXT("At least one exact loaded native function/struct/class or bounded schema conversion was unavailable; inspect partial observations.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}
