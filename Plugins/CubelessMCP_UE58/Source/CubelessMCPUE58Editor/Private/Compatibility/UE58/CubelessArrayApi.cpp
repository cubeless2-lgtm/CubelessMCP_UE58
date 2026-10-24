#include "Compatibility/UE58/CubelessArrayApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Math/Vector4.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/UserDefinedEnum.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "UObject/MetaData.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformProcess.h"
#include "K2Node_CallArrayFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/EngineVersion.h"
#include "Misc/DefaultValueHelper.h"
#include "Internationalization/Text.h"
#include "Misc/StringOutputDevice.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"

namespace Cubeless::UE58::Array
{
namespace
{
struct FTarget { const TCHAR* ClassPath; const TCHAR* FunctionName; };
constexpr FTarget Targets[] = {
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_Length")},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_Get")},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_Set")},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_Add")},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_AddUnique")},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_Insert")},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_Contains")},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_RemoveItem")},
    {TEXT("/Script/Engine.KismetArrayLibrary"), TEXT("Array_Clear")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BreakRotator")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BreakTransform")},
};

bool Fail(Function::FResult& R, const TCHAR* Code, const TCHAR* Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }

FString Text(const TSharedRef<FJsonObject>& Object)
{ FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out; }

bool FullyLoaded(const UObject* Object)
{ return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects); }

bool ExactNativeClass(const UClass* Class, const FString& Path)
{
    const UPackage* Package = Class ? Class->GetOutermost() : nullptr;
    return FullyLoaded(Class) && Class->GetPathName() == Path && Class->HasAnyClassFlags(CLASS_Native)
        && !Class->HasAnyClassFlags(CLASS_Interface | CLASS_Deprecated | CLASS_NewerVersionExists) && !Class->IsEditorOnly()
        && IsValid(Package) && Package->GetName() == TEXT("/Script/Engine")
        && !Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly);
}

TSharedRef<FJsonObject> PackageState(const UPackage* Package)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("package_name"), Package ? Package->GetName() : FString());
    Row->SetStringField(TEXT("package_flags_string"), Package ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    Row->SetBoolField(TEXT("package_dirty"), Package && Package->IsDirty());
    return Row;
}

struct FContext;
bool ExtendedObservedCurrent(FContext* C);
TSharedRef<FJsonObject> DescribeProperty(const FProperty* Property, const UEdGraphSchema_K2* Schema, int32 Depth = 0,Function::FPresetLoaderNameArrayScope* NameScope=nullptr,FContext* Extended=nullptr)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), Property->GetName());
    Row->SetStringField(TEXT("path"), Property->GetPathName());
    Row->SetStringField(TEXT("field_class"), Property->GetClass()->GetName());
    Row->SetStringField(TEXT("cpp_type"), Property->GetCPPType());if((NameScope&&!NameScope->Check())||!ExtendedObservedCurrent(Extended))return Row;
    Row->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
    Row->SetNumberField(TEXT("array_dim"), Property->ArrayDim);
    Row->SetBoolField(TEXT("parm"), Property->HasAnyPropertyFlags(CPF_Parm));
    Row->SetBoolField(TEXT("return_parm"), Property->HasAnyPropertyFlags(CPF_ReturnParm));
    Row->SetBoolField(TEXT("out_parm"), Property->HasAnyPropertyFlags(CPF_OutParm));
    Row->SetBoolField(TEXT("reference_parm"), Property->HasAnyPropertyFlags(CPF_ReferenceParm));
    Row->SetBoolField(TEXT("const_parm"), Property->HasAnyPropertyFlags(CPF_ConstParm));
    Row->SetBoolField(TEXT("editor_only"), Property->HasAnyPropertyFlags(CPF_EditorOnly));
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    { Row->SetStringField(TEXT("struct_path"), Struct->Struct ? Struct->Struct->GetPathName() : FString()); }
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    { Row->SetStringField(TEXT("object_class_path"), Object->PropertyClass ? Object->PropertyClass->GetPathName() : FString()); }
    if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
    {
        Row->SetBoolField(TEXT("array_inner_present"), ArrayProperty->Inner != nullptr);
        if (ArrayProperty->Inner && Depth == 0)
        { Row->SetObjectField(TEXT("array_inner"), DescribeProperty(ArrayProperty->Inner, Schema, Depth + 1,NameScope,Extended));if((NameScope&&!NameScope->Check())||!ExtendedObservedCurrent(Extended))return Row; }
    }
    FEdGraphPinType Type;
    const bool Converted = Schema->ConvertPropertyToPinType(Property, Type);if((NameScope&&!NameScope->Check())||!ExtendedObservedCurrent(Extended))return Row;
    Row->SetBoolField(TEXT("schema_conversion_success"), Converted);
    if (Converted)
    {
        FString TypeText;
        FEdGraphPinType::StaticStruct()->ExportText(TypeText, &Type, &Type, nullptr, PPF_None, nullptr);if((NameScope&&!NameScope->Check())||!ExtendedObservedCurrent(Extended))return Row;
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
    auto State = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Functions; bool Complete = true;
    for (const FTarget& Target : Targets)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("requested_class_path"), Target.ClassPath);
        Row->SetStringField(TEXT("requested_function_name"), Target.FunctionName);
        UClass* Owner = FindObject<UClass>(nullptr, Target.ClassPath);
        const bool OwnerValid = ExactNativeClass(Owner, Target.ClassPath);
        Row->SetBoolField(TEXT("exact_loaded_native_owner"), OwnerValid);
        UFunction* Function = OwnerValid ? Owner->FindFunctionByName(FName(Target.FunctionName)) : nullptr;
        const bool FunctionValid = FullyLoaded(Function) && Function->GetOuterUClass() == Owner
            && Function->GetName() == Target.FunctionName && Function->GetOutermost() == Owner->GetOutermost();
        Row->SetBoolField(TEXT("exact_declared_function"), FunctionValid);
        if (!FunctionValid) { Complete = false; Functions.Add(MakeShared<FJsonValueObject>(Row)); continue; }
        Row->SetStringField(TEXT("function_path"), Function->GetPathName());
        Row->SetStringField(TEXT("declaring_class_path"), Owner->GetPathName());
        Row->SetStringField(TEXT("class_flags_string"), LexToString(static_cast<uint32>(Owner->ClassFlags)));
        Row->SetStringField(TEXT("function_flags_string"), LexToString(static_cast<uint32>(Function->FunctionFlags)));
        Row->SetBoolField(TEXT("native"), Function->HasAnyFunctionFlags(FUNC_Native));
        Row->SetBoolField(TEXT("public"), Function->HasAnyFunctionFlags(FUNC_Public));
        Row->SetBoolField(TEXT("blueprint_callable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
        Row->SetBoolField(TEXT("blueprint_pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
        Row->SetBoolField(TEXT("static"), Function->HasAnyFunctionFlags(FUNC_Static));
        Row->SetBoolField(TEXT("const"), Function->HasAnyFunctionFlags(FUNC_Const));
        Row->SetBoolField(TEXT("editor_only"), Function->HasAnyFunctionFlags(FUNC_EditorOnly));
        Row->SetNumberField(TEXT("declared_num_parms"), Function->NumParms);
        Row->SetObjectField(TEXT("package"), PackageState(Owner->GetOutermost()));
        const UObject* ExistingCDO = Owner->GetDefaultObject(false);
        Row->SetBoolField(TEXT("default_object_already_loaded"), FullyLoaded(ExistingCDO));
        Row->SetStringField(TEXT("existing_default_object_path"), ExistingCDO ? ExistingCDO->GetPathName() : FString());
        for (const TCHAR* Key : {TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("AutoCreateRefTerm"),
            TEXT("BlueprintInternalUseOnly"), TEXT("CustomThunk"), TEXT("NativeBreakFunc"), TEXT("Latent"),
            TEXT("DeprecatedFunction"), TEXT("BlueprintThreadSafe"), TEXT("WorldContext"), TEXT("ScriptName")})
        { Row->SetStringField(Key, Function->GetMetaData(Key)); }
        TArray<TSharedPtr<FJsonValue>> Parameters; int32 Count = 0;
        for (TFieldIterator<FProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            if (!It->HasAnyPropertyFlags(CPF_Parm)) { continue; }
            if (++Count > 32) { Complete = false; break; }
            auto Parameter = DescribeProperty(*It, Schema);
            Parameter->SetNumberField(TEXT("ordinal"), Count - 1);
            Parameter->SetBoolField(TEXT("exact_declared_owner"), It->GetOwnerStruct() == Function);
            const bool Input = !It->HasAnyPropertyFlags(CPF_ReturnParm)
                && (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ReferenceParm));
            Parameter->SetStringField(TEXT("k2_call_pin_direction"), Input ? TEXT("input") : TEXT("output"));
            const FString DefaultKey = TEXT("CPP_Default_") + It->GetName();
            Parameter->SetBoolField(TEXT("cpp_default_metadata_present"), Function->HasMetaData(FName(*DefaultKey)));
            Parameter->SetStringField(TEXT("cpp_default_metadata_value"), Function->GetMetaData(FName(*DefaultKey)));
            bool Converted = false; Parameter->TryGetBoolField(TEXT("schema_conversion_success"), Converted);
            Complete &= Converted && It->GetOwnerStruct() == Function;
            Parameters.Add(MakeShared<FJsonValueObject>(Parameter));
        }
        Complete &= Count == Function->NumParms;
        Row->SetNumberField(TEXT("parameter_count_observed"), Count);
        Row->SetArrayField(TEXT("parameters"), Parameters); Functions.Add(MakeShared<FJsonValueObject>(Row));
    }
    State->SetArrayField(TEXT("functions"), Functions);
    State->SetNumberField(TEXT("expected_function_count"), UE_ARRAY_COUNT(Targets));
    State->SetBoolField(TEXT("observations_complete"), Complete); return State;
}
}

Function::FResult InspectNativeSupport()
{
    Function::FResult R; const FEngineVersion& Version = FEngineVersion::Current();
    R.Data->SetStringField(TEXT("engine_version"), Version.ToString());
    R.Data->SetStringField(TEXT("engine_directory"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    R.Data->SetStringField(TEXT("project_file"), FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
    R.Data->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
    R.Data->SetStringField(TEXT("scope"), TEXT("exact already-loaded native nine-array family plus BreakRotator and BreakTransform; reflected flags/schema/default metadata only, not node construction or runtime proof"));
    R.Data->SetBoolField(TEXT("asset_selectors_accepted"), false);
    R.Data->SetBoolField(TEXT("object_or_cdo_creation_requested"), false);
    R.Data->SetBoolField(TEXT("native_function_invocation_requested"), false);
    R.Data->SetBoolField(TEXT("asset_or_module_load_requested"), false);
    R.Data->SetBoolField(TEXT("node_creation_requested"), false);
    R.Data->SetBoolField(TEXT("private_property_read_or_write_requested"), false);
    R.Data->SetBoolField(TEXT("construction_implemented"), true);
    R.Data->SetBoolField(TEXT("construction_behavior_verified"), false);
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Fail(R, TEXT("array_signature_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); return R; }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8 || Version.GetPatch() != 3)
    { Fail(R, TEXT("array_signature_version_unsupported"), TEXT("This evidence adapter is scoped to installed UE 5.8.3.")); return R; }
    UClass* SchemaClass = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.EdGraphSchema_K2"));
    const UEdGraphSchema_K2* Schema = FullyLoaded(SchemaClass)
        ? Cast<UEdGraphSchema_K2>(SchemaClass->GetDefaultObject(false)) : nullptr;
    if (!Schema || Schema->GetClass() != SchemaClass)
    { Fail(R, TEXT("array_signature_schema_not_loaded"), TEXT("Requires the already-created exact standard K2 schema CDO; this read never creates it.")); return R; }
    const auto Before = Observe(Schema); const auto After = Observe(Schema);
    R.Data->SetObjectField(TEXT("before"), Before); R.Data->SetObjectField(TEXT("after"), After);
    R.bChanged = Text(Before) != Text(After);
    R.Data->SetBoolField(TEXT("observed_native_state_preserved"), !R.bChanged);
    if (R.bChanged)
    { R.Stage = TEXT("verification"); Fail(R, TEXT("array_signature_read_state_changed"), TEXT("Finite native observations changed during the read; no modification was requested.")); return R; }
    if (!Before->GetBoolField(TEXT("observations_complete")) || !After->GetBoolField(TEXT("observations_complete")))
    { Fail(R, TEXT("array_signature_observation_incomplete"), TEXT("At least one exact loaded native signature or bounded schema conversion was unavailable; inspect partial observations.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}

namespace
{
using Function::FResult;
bool Error(FResult& R, const TCHAR* Code, const FString& Message);
struct FParameter { const TCHAR* Name; const TCHAR* Class; uint64 Flags; const TCHAR* Category; };
constexpr FParameter LengthParams[] = {
    {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589122ULL, TEXT("wildcard")},
    {TEXT("ReturnValue"), TEXT("IntProperty"), 6755469234276224ULL, TEXT("int")}};
constexpr FParameter GetParams[] = {
    {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589122ULL, TEXT("wildcard")},
    {TEXT("Index"), TEXT("IntProperty"), 6755469234274944ULL, TEXT("int")},
    {TEXT("Item"), TEXT("IntProperty"), 6755469234275200ULL, TEXT("wildcard")}};
constexpr FParameter SetParams[] = {
    {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589120ULL, TEXT("wildcard")},
    {TEXT("Index"), TEXT("IntProperty"), 6755469234274944ULL, TEXT("int")},
    {TEXT("Item"), TEXT("IntProperty"), 6755469368492930ULL, TEXT("wildcard")},
    {TEXT("bSizeToFit"), TEXT("BoolProperty"), 6755469234274944ULL, TEXT("bool")}};
constexpr FParameter AddParams[] = {
    {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589120ULL, TEXT("wildcard")},
    {TEXT("NewItem"), TEXT("IntProperty"), 6755469368492930ULL, TEXT("wildcard")},
    {TEXT("ReturnValue"), TEXT("IntProperty"), 6755469234276224ULL, TEXT("int")}};
constexpr FParameter InsertParams[] = {
    {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589120ULL, TEXT("wildcard")},
    {TEXT("NewItem"), TEXT("IntProperty"), 6755469368492930ULL, TEXT("wildcard")},
    {TEXT("Index"), TEXT("IntProperty"), 6755469234274944ULL, TEXT("int")}};
constexpr FParameter ContainsParams[] = {
    {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589122ULL, TEXT("wildcard")},
    {TEXT("ItemToFind"), TEXT("IntProperty"), 6755469368492930ULL, TEXT("wildcard")},
    {TEXT("ReturnValue"), TEXT("BoolProperty"), 6755469234276224ULL, TEXT("bool")}};
constexpr FParameter RemoveParams[] = {
    {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589120ULL, TEXT("wildcard")},
    {TEXT("Item"), TEXT("IntProperty"), 6755469368492930ULL, TEXT("wildcard")},
    {TEXT("ReturnValue"), TEXT("BoolProperty"), 6755469234276224ULL, TEXT("bool")}};
constexpr FParameter ClearParams[] = {{TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589120ULL, TEXT("wildcard")}};
struct FOperation
{
    const TCHAR* Name; const TCHAR* FunctionName; uint32 Flags; const TCHAR* Dependent; const TCHAR* RefTerm;
    const TCHAR* Internal; const FParameter* Parameters; int32 Count;
};
constexpr FOperation Operations[] = {
    {TEXT("length"), TEXT("Array_Length"), 339878915U, TEXT(""), TEXT(""), TEXT(""), LengthParams, UE_ARRAY_COUNT(LengthParams)},
    {TEXT("get"), TEXT("Array_Get"), 339878915U, TEXT("Item"), TEXT(""), TEXT("true"), GetParams, UE_ARRAY_COUNT(GetParams)},
    {TEXT("set"), TEXT("Array_Set"), 71443459U, TEXT("Item"), TEXT("Item"), TEXT(""), SetParams, UE_ARRAY_COUNT(SetParams)},
    {TEXT("add"), TEXT("Array_Add"), 71443459U, TEXT("NewItem"), TEXT("NewItem"), TEXT(""), AddParams, UE_ARRAY_COUNT(AddParams)},
    {TEXT("add_unique"), TEXT("Array_AddUnique"), 71443459U, TEXT("NewItem"), TEXT("NewItem"), TEXT(""), AddParams, UE_ARRAY_COUNT(AddParams)},
    {TEXT("insert"), TEXT("Array_Insert"), 71443459U, TEXT("NewItem"), TEXT("NewItem"), TEXT(""), InsertParams, UE_ARRAY_COUNT(InsertParams)},
    {TEXT("contains"), TEXT("Array_Contains"), 339878915U, TEXT("ItemToFind"), TEXT("ItemToFind"), TEXT(""), ContainsParams, UE_ARRAY_COUNT(ContainsParams)},
    {TEXT("remove"), TEXT("Array_RemoveItem"), 71443459U, TEXT("Item"), TEXT("Item"), TEXT(""), RemoveParams, UE_ARRAY_COUNT(RemoveParams)},
    {TEXT("clear"), TEXT("Array_Clear"), 71443459U, TEXT(""), TEXT(""), TEXT(""), ClearParams, UE_ARRAY_COUNT(ClearParams)},
};
struct FDefault { FName PinName; FString RequestedName; FString Value; };


// IK: concrete read-only UserDefinedEnum scope. Primitive/Vector routes keep their existing path.
bool EnumContext()
{ return IsEditorGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage(); }
bool EnumLoaded(const UObject* O)
{ return FullyLoaded(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed); }
bool EnumEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!EnumEqual(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& Pair:X->Values) { const auto* V=Y->Values.Find(Pair.Key);if(!V||!EnumEqual(Pair.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}
struct FEnumArrayScope
{
    Function::FResult* Reply=nullptr;bool Active=false,Stopped=false,Attached=false,GuidFixed=false,PinsFixed=false,LoadEntered=false,FunctionAssigned=false;
    TStrongObjectPtr<UEnum> Source;TStrongObjectPtr<UPackage> SourcePackage;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    UObject* SourceOuter=nullptr;UClass* SourceClass=nullptr;FString SourcePath;EObjectFlags SourceFlags=RF_NoFlags;bool SourceDirty=false;uint32 SourcePackageFlags=0;EObjectFlags SourcePackageObjectFlags=RF_NoFlags;
    TSharedPtr<FJsonObject> Before;
    TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UEdGraphNode> Node;
    TStrongObjectPtr<UPackage> TargetPackage;TStrongObjectPtr<UClass> Parent,Generated,Skeleton;TStrongObjectPtr<const UEdGraphSchema_K2> Schema;
    UObject *BlueprintOuter=nullptr,*GraphOuter=nullptr;UClass *BlueprintClass=nullptr,*GraphClass=nullptr,*NodeClass=nullptr;
    FString BlueprintPath,GraphPath,NodePath;FGuid GraphGuid,NodeGuid;
    struct FPinRole { UEdGraphPin* Address=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_MAX;FEdGraphPinType Type;uint32 Flags=0;UObject* DefaultObject=nullptr; };
    TArray<FPinRole> Roles;
    TStrongObjectPtr<UClass> FunctionOwner;TStrongObjectPtr<UFunction> NativeFunction;TStrongObjectPtr<UObject> FunctionCDO;
    bool Bad(const TCHAR* Code,const TCHAR* Why)
    {
        if(Stopped) { return false; }Stopped=true;auto& R=*Reply;R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=!LoadEntered&&!R.bMutationAttempted;
        if(!R.ErrorCode.IsEmpty()&&R.ErrorCode!=Code) { R.Data->SetStringField(TEXT("enum_preceding_error_code"),R.ErrorCode); }
        R.ErrorCode=Code;R.Error=Why;R.Data->SetBoolField(TEXT("enum_source_observed_unchanged"),false);
        R.Data->SetBoolField(TEXT("automatic_retry_safe"),false);return false;
    }
    bool Context() const
    { return EnumContext()&&GEditor==Editor.Get()&&!IsPlaySessionActive()&&World.IsValid()&&GEditor->GetEditorWorldContext().World()==World.Get()&&!World->bIsTearingDown; }
    bool Identity()
    {
        if(Stopped) { return false; }if(!Active) { return true; }
        if(!Context()) { return Bad(TEXT("array_enum_context_changed"),TEXT("Captured Editor/world or GT/PIE/GC/exit/save admission changed; no next SDK or cleanup.")); }
        auto* E=Source.Get();
        if(!EnumLoaded(E)||!EnumLoaded(SourcePackage.Get())||!EnumLoaded(SourceOuter)||!EnumLoaded(SourceClass)
            ||E->GetClass()!=SourceClass||E->GetOuter()!=SourceOuter||E->GetPackage()!=SourcePackage.Get()||E->GetPathName()!=SourcePath||E->GetFlags()!=SourceFlags||SourcePackage->IsDirty()!=SourceDirty||uint32(SourcePackage->GetPackageFlags())!=SourcePackageFlags||SourcePackage->GetFlags()!=SourcePackageObjectFlags)
        { return Bad(TEXT("array_enum_identity_changed"),TEXT("Read-only enum source identity/package/flags/dirty changed; no replacement source is adopted.")); }
        return true;
    }
    static uint32 PinFlags(const UEdGraphPin* P)
    { return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bOrphanedPin)<<4)|(uint32(P->bAdvancedView)<<5); }
    bool Target()
    {
        if(!Blueprint.IsValid()) { return true; }auto* B=Blueprint.Get();auto* G=Graph.Get();
        if(!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(TargetPackage.Get())||!EnumLoaded(BlueprintOuter)||!EnumLoaded(GraphOuter)
            ||B->GetClass()!=BlueprintClass||B->GetOuter()!=BlueprintOuter||B->GetPackage()!=TargetPackage.Get()||B->GetPathName()!=BlueprintPath
            ||B->ParentClass.Get()!=Parent.Get()||B->GeneratedClass.Get()!=Generated.Get()||B->SkeletonGeneratedClass.Get()!=Skeleton.Get()||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
            ||G->GetClass()!=GraphClass||G->GetOuter()!=GraphOuter||G->GetPackage()!=TargetPackage.Get()||G->GetPathName()!=GraphPath||G->GraphGuid!=GraphGuid
            ||TargetPackage->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_CompiledIn|PKG_PlayInEditor)||!G->bEditable||G->GetSchema()!=Schema.Get()||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=B)
        { return Bad(TEXT("array_enum_target_changed"),TEXT("Selected Blueprint/owned graph/schema role changed; no stale target SDK or cleanup.")); }
        if(FunctionOwner.IsValid()&&(!EnumLoaded(FunctionOwner.Get())||!EnumLoaded(NativeFunction.Get())||!EnumLoaded(FunctionCDO.Get())||NativeFunction->GetOuterUClass()!=FunctionOwner.Get()||FunctionOwner->GetDefaultObject(false)!=FunctionCDO.Get()))
        { return Bad(TEXT("array_enum_native_function_changed"),TEXT("Captured native function/library/CDO role changed; no next SDK.")); }
        if(Node.IsValid())
        {
            auto* N=Node.Get();int32 Count=0;for(const auto& It:G->Nodes) { if(It.Get()==N) { ++Count; } }
            if(!EnumLoaded(N)||N->GetClass()!=NodeClass||N->GetOuter()!=G||N->GetPackage()!=TargetPackage.Get()||N->GetPathName()!=NodePath||Count!=(Attached?1:0)||(GuidFixed&&N->NodeGuid!=NodeGuid))
            { return Bad(TEXT("array_enum_node_changed"),TEXT("Created native node identity/membership changed; retained partial state is not repaired.")); }
            if(FunctionAssigned)
            {
                const auto* Call=Cast<UK2Node_CallFunction>(N);
                if(!Call||Call->FunctionReference.GetMemberParentClass()!=FunctionOwner.Get()||Call->FunctionReference.GetMemberName()!=NativeFunction->GetFName()||Call->FunctionReference.IsSelfContext()||Call->FunctionReference.GetMemberGuid().IsValid())
                { return Bad(TEXT("array_enum_function_binding_changed"),TEXT("Created call node no longer has the selected exact native function binding.")); }
            }
            if(PinsFixed)
            {
                if(N->Pins.Num()!=Roles.Num()) { return Bad(TEXT("array_enum_pin_changed"),TEXT("Current physical pin count differs from the allocation receipt.")); }
                for(int32 I=0;I<Roles.Num();++I)
                {
                    const auto& Role=Roles[I];UEdGraphPin* P=N->Pins[I];
                    if(P!=Role.Address||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=Role.Id||P->PinName!=Role.Name||P->Direction!=Role.Direction
                        ||P->PinType!=Role.Type||P->PinType.bIsUObjectWrapper!=Role.Type.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=Role.Type.bSerializeAsSinglePrecisionFloat||P->PinType.PinValueType.bTerminalIsUObjectWrapper!=Role.Type.PinValueType.bTerminalIsUObjectWrapper||PinFlags(P)!=Role.Flags||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()||P->DefaultObject.Get()!=Role.DefaultObject||(Role.DefaultObject&&!EnumLoaded(Role.DefaultObject))||!P->DefaultTextValue.IsEmpty())
                    { return Bad(TEXT("array_enum_pin_changed"),TEXT("Original physical pin ID/owner/type/flags or unlinked default role changed across a callback.")); }
                }
            }
        }
        return true;
    }
    bool Snapshot(TSharedRef<FJsonObject>& J)
    {
        if(!Identity()) { return false; }auto* E=Source.Get();
        J->SetStringField(TEXT("path"),SourcePath);J->SetStringField(TEXT("class"),SourceClass->GetPathName());J->SetBoolField(TEXT("dirty"),SourceDirty);J->SetNumberField(TEXT("flags"),uint32(SourceFlags));J->SetNumberField(TEXT("package_flags"),SourcePackageFlags);J->SetNumberField(TEXT("package_object_flags"),uint32(SourcePackageObjectFlags));
        if(E->NumEnums()<2||E->NumEnums()>257) { return Bad(TEXT("array_enum_entries_invalid"),TEXT("Bounded byte UserDefinedEnum entries required.")); }
        TArray<TSharedPtr<FJsonValue>> Entries;
        for(int32 I=0;I<E->NumEnums();++I)
        {
            auto Row=MakeShared<FJsonObject>();const FString Name=E->GetNameStringByIndex(I);const int64 Value=E->GetValueByIndex(I);const FString Display=E->GetDisplayNameTextByIndex(I).ToString();
            if(!Identity()) { return false; }
            if(Name.Len()>512||Display.Len()>4096||(I<E->NumEnums()-1&&(Value<0||Value>255))) { return Bad(TEXT("array_enum_entries_invalid"),TEXT("Enum byte entries/text exceed the finite source scope.")); }
            Row->SetStringField(TEXT("name"),Name);Row->SetStringField(TEXT("full_name"),E->GetNameByIndex(I).ToString());Row->SetStringField(TEXT("value"),LexToString(Value));Row->SetStringField(TEXT("display"),Display);
            Row->SetBoolField(TEXT("hidden"),E->HasMetaData(TEXT("Hidden"),I));Row->SetBoolField(TEXT("spacer"),E->HasMetaData(TEXT("Spacer"),I));if(!Identity()) { return false; }Entries.Add(MakeShared<FJsonValueObject>(Row));
        }
        J->SetArrayField(TEXT("entries"),Entries);J->SetNumberField(TEXT("cpp_form"),int32(E->GetCppForm()));J->SetNumberField(TEXT("underlying"),int32(E->GetUnderlyingType()));
        for(const FName Field:{FName(TEXT("UniqueNameIndex")),FName(TEXT("EnumDescription")),FName(TEXT("DisplayNameMap"))})
        {
            auto* P=FindFProperty<FProperty>(E->GetClass(),Field);FString Value;
            if(!P||P->ArrayDim!=1||!P->ExportText_Direct(Value,P->ContainerPtrToValuePtr<void>(E),P->ContainerPtrToValuePtr<void>(E),E,PPF_None)||!Identity()) { return Bad(TEXT("array_enum_stored_fields_incomplete"),TEXT("Complete stored UserDefinedEnum fields unavailable.")); }
            if(Value.Len()>262144) { return Bad(TEXT("array_enum_source_bound"),TEXT("Stored enum source field exceeds finite observation bound.")); }J->SetStringField(Field.ToString(),Value);
        }
        const TMap<FName,FString>* Meta=FMetaData::GetMapForObject(E);if(!Identity()) { return false; }auto M=MakeShared<FJsonObject>();
        if(Meta)
        {
            if(Meta->Num()>2048) { return Bad(TEXT("array_enum_source_bound"),TEXT("Enum source metadata exceeds finite observation bound.")); }
            for(const auto& Pair:*Meta) { if(Pair.Key.ToString().Len()>512||Pair.Value.Len()>16384) { return Bad(TEXT("array_enum_source_bound"),TEXT("Enum metadata text exceeds bound.")); }M->SetStringField(Pair.Key.ToString(),Pair.Value); }
        }
        J->SetObjectField(TEXT("metadata"),M);FString File;
        if(!FPackageName::TryConvertLongPackageNameToFilename(SourcePackage->GetName(),File,FPackageName::GetAssetPackageExtension())) { return Bad(TEXT("array_enum_disk_incomplete"),TEXT("Enum source package filename unavailable.")); }
        const int64 Size=IFileManager::Get().FileSize(*File);J->SetBoolField(TEXT("disk_exists"),Size>=0);J->SetStringField(TEXT("disk_path"),File);
        if(Size>=0)
        {
            if(Size>16*1024*1024) { return Bad(TEXT("array_enum_disk_bound"),TEXT("Enum source disk package exceeds16MiB bound.")); }
            TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*File)||Bytes.Num()!=Size) { return Bad(TEXT("array_enum_disk_incomplete"),TEXT("Enum source disk read incomplete.")); }
            uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);J->SetNumberField(TEXT("disk_bytes"),Size);J->SetStringField(TEXT("disk_sha1"),BytesToHex(Digest,FSHA1::DigestSize));
        }
        if(!Identity()||Text(J).Len()>1048576) { return Bad(TEXT("array_enum_source_bound"),TEXT("Complete enum source snapshot exceeds1Mi UTF16 bound or context changed.")); }return true;
    }
    bool Check()
    {
        if(Stopped) { return false; }if(!Active) { return true; }if(!Identity()||!Target()) { return false; }
        auto Now=MakeShared<FJsonObject>();if(!Snapshot(Now)) { return false; }
        if(!EnumEqual(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(Now))) { return Bad(TEXT("array_enum_source_changed"),TEXT("Read-only enum entries/stored fields/metadata/disk changed; no next SDK, rollback or retry.")); }
        Reply->Data->SetBoolField(TEXT("enum_source_observed_unchanged"),true);return Target();
    }
    bool Load(const FString& Input,Function::FResult& R)
    {
        Reply=&R;Active=true;
        if(!EnumContext()||IsPlaySessionActive()) { return Bad(TEXT("array_enum_editor_required"),TEXT("Safe Editor GT context required before enum asset load.")); }
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!World.IsValid()||World->bIsTearingDown) { return Bad(TEXT("array_enum_editor_required"),TEXT("Current Editor world required.")); }
        FString P=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();P.TrimQuotesInline();P.ReplaceInline(TEXT("\\"),TEXT("/"));
        if(!Function::Pin::SafeName(Input)||(!P.StartsWith(TEXT("/Game/"))&&!P.StartsWith(TEXT("/Engine/")))) { return Bad(TEXT("array_enum_descriptor_pending"),TEXT("This enum extension consumes exact Game/Engine UserDefinedEnum assets; native/custom-module/slow-find variants remain pending.")); }
        const FString Low=P.ToLower();for(const TCHAR* Token:{TEXT("cubelessmcp"),TEXT("cubelesscontrolrigtools"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal")}) { if(Low.Contains(Token)) { return Bad(TEXT("array_enum_dependency_forbidden"),TEXT("Enum asset dependency points to authoring bridge content.")); } }
        if(!P.Contains(TEXT("."))) { P+=TEXT(".")+FPackageName::GetShortName(P); }
        if(P.Len()>=NAME_SIZE||!FPackageName::IsValidObjectPath(P)) { return Bad(TEXT("array_enum_path_invalid"),TEXT("Raw and expanded exact enum asset object path must fit NAME_SIZE.")); }
        LoadEntered=true;R.Data->SetNumberField(TEXT("enum_load_calls"),1);UEnum* E=LoadObject<UEnum>(nullptr,*P);R.Data->SetNumberField(TEXT("enum_load_returns"),1);R.Data->SetBoolField(TEXT("enum_load_global_effects_unverified"),true);
        if(!Context()) { return Bad(TEXT("array_enum_context_changed"),TEXT("Enum load returned after context loss; return UObject was not observed/adopted.")); }
        if(!EnumLoaded(E)||E->GetClass()!=UUserDefinedEnum::StaticClass()||E->GetPathName()!=P||E->GetOuter()!=E->GetPackage()||!E->HasAnyFlags(RF_Public)
            ||E->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||E->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly))
        { return Bad(TEXT("array_enum_source_unsupported"),TEXT("Exact runtime public UserDefinedEnum asset required; redirected/custom/transient source is not substituted.")); }
        Source.Reset(E);SourcePackage.Reset(E->GetPackage());SourceOuter=E->GetOuter();SourceClass=E->GetClass();SourcePath=P;SourceFlags=E->GetFlags();SourceDirty=SourcePackage->IsDirty();SourcePackageFlags=uint32(SourcePackage->GetPackageFlags());SourcePackageObjectFlags=SourcePackage->GetFlags();
        auto J=MakeShared<FJsonObject>();if(!Snapshot(J)) { return false; }Before=J;R.Data->SetStringField(TEXT("enum_asset_path"),P);R.Data->SetObjectField(TEXT("enum_source_before"),J);
        R.Data->SetBoolField(TEXT("enum_source_read_only"),true);R.Data->SetBoolField(TEXT("enum_source_initial_dirty"),SourceDirty);return Check();
    }
    bool SetTarget(UBlueprint* B,UEdGraph* G,const UEdGraphSchema_K2* S)
    {
        if(!Check()||!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(S)||B->GetPackage()==SourcePackage.Get()) { return Bad(TEXT("array_enum_target_unsupported"),TEXT("A current directly owned Blueprint graph in a distinct source package is required.")); }
        Blueprint.Reset(B);Graph.Reset(G);Schema.Reset(S);TargetPackage.Reset(B->GetPackage());Parent.Reset(B->ParentClass.Get());Generated.Reset(B->GeneratedClass.Get());Skeleton.Reset(B->SkeletonGeneratedClass.Get());BlueprintOuter=B->GetOuter();GraphOuter=G->GetOuter();BlueprintClass=B->GetClass();GraphClass=G->GetClass();BlueprintPath=B->GetPathName();GraphPath=G->GetPathName();GraphGuid=G->GraphGuid;return Check();
    }
    bool SetNode(UEdGraphNode* N,UClass* ExpectedClass)
    {
        // NewObject has returned, but the node is not yet owned by Graph.Nodes.
        // No source/metadata/display SDK runs before the exact return is strongly held.
        if(!Identity()) { return false; }
        if(!EnumLoaded(N)||N->GetClass()!=ExpectedClass||N->GetOuter()!=Graph.Get()||N->GetPackage()!=TargetPackage.Get())
        { return Bad(TEXT("array_enum_node_invalid"),TEXT("NewObject return does not have the expected exact native class/owned graph role.")); }
        Node.Reset(N);NodeClass=ExpectedClass;NodePath=N->GetPathName();return Check();
    }
    bool FreezePins()
    {
        if(!Check()||!Node.IsValid()||Node->Pins.Num()>512) { return Bad(TEXT("array_enum_pin_invalid"),TEXT("New allocation has no bounded physical pin array.")); }
        TSet<FGuid> Ids;for(UEdGraphPin* P:Node->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node.Get()||!P->PinId.IsValid()||Ids.Contains(P->PinId)) { return Bad(TEXT("array_enum_pin_invalid"),TEXT("Allocated physical pin ID/owner is invalid.")); }
            Ids.Add(P->PinId);Roles.Add({P,P->PinId,P->PinName,P->Direction,P->PinType,PinFlags(P),P->DefaultObject.Get()});
        }
        PinsFixed=true;return Check();
    }
    bool ExpectType(UEdGraphPin* P,const FEdGraphPinType& Type)
    {
        if(!Check()) { return false; }for(auto& Role:Roles) { if(Role.Address==P) { Role.Type=Type;return true; } }return Bad(TEXT("array_enum_pin_invalid"),TEXT("Writer pin is not an original allocated physical pin."));
    }
    bool Value(const TSharedPtr<FJsonValue>& V,FString& Out)
    {
        if(!Check()) { return false; }int32 Index=INDEX_NONE;UEnum* E=Source.Get();
        if(V.IsValid()&&V->Type==EJson::String)
        {
            FString Name=V->AsString().TrimStartAndEnd();Name.TrimQuotesInline();if(Name.IsEmpty()||Name.Len()>512) { return Bad(TEXT("array_enum_value_invalid"),TEXT("Enum value must be a bounded nonempty entry name or integral numeric value.")); }
            Index=E->GetIndexByNameString(Name);if(Index==INDEX_NONE&&!Name.Contains(TEXT("::"))) { Index=E->GetIndexByNameString(E->GetName()+TEXT("::")+Name); }
            if(Index==INDEX_NONE) { const FString Full=E->GenerateFullEnumName(*Name);if(!Check()) { return false; }Index=E->GetIndexByNameString(Full); }
        }
        else if(V.IsValid()&&V->Type==EJson::Number)
        {
            const double D=V->AsNumber();if(!FMath::IsFinite(D)||D<0||D>255||FMath::TruncToDouble(D)!=D) { return Bad(TEXT("array_enum_value_invalid"),TEXT("Enum numeric default must be an exact byte-valued entry; no truncation.")); }Index=E->GetIndexByValue(int64(D));
        }
        if(!Check()) { return false; }
        if(Index==INDEX_NONE||Index>=E->NumEnums()-1||E->HasMetaData(TEXT("Hidden"),Index)||E->HasMetaData(TEXT("Spacer"),Index)) { return Bad(TEXT("array_enum_value_invalid"),TEXT("Enum entry is missing, hidden, spacer, MAX or is not a string/integer default.")); }
        Out=E->GetNameStringByIndex(Index);return Check();
    }
};
// IN: callback admission only for the newly accepted Vector literal/operation routes.
struct FVectorArrayScope
{
    FResult* Reply=nullptr;bool Stopped=false,Attached=false,GuidFixed=false,PinsFixed=false;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UEdGraphNode> Node;
    TStrongObjectPtr<const UEdGraphSchema_K2> Schema;TStrongObjectPtr<UScriptStruct> Vector;TStrongObjectPtr<UClass> Parent,Generated,Skeleton,NodeClass;
    UObject *BlueprintOuter=nullptr,*VectorOuter=nullptr,*GeneratedOuter=nullptr,*SkeletonOuter=nullptr;UClass *BlueprintClass=nullptr,*VectorClass=nullptr,*GeneratedClass=nullptr,*SkeletonClass=nullptr;
    FString BlueprintPath,GraphPath,VectorPath,NodePath,GeneratedPath,SkeletonPath;FGuid GraphGuid,NodeGuid;EObjectFlags VectorFlags=RF_NoFlags,GeneratedFlags=RF_NoFlags,SkeletonFlags=RF_NoFlags;
    bool VectorDirty=false,FunctionAssigned=false;TArray<FEnumArrayScope::FPinRole> Roles;
    TStrongObjectPtr<UClass> FunctionOwner;TStrongObjectPtr<UFunction> Function;TStrongObjectPtr<UObject> FunctionDefault;
    bool Bad(const TCHAR* Code,const TCHAR* Why) { if(!Stopped) { if(!Reply->ErrorCode.IsEmpty()&&Reply->ErrorCode!=Code) { Reply->Data->SetStringField(TEXT("vector_preceding_error_code"),Reply->ErrorCode);Reply->Data->SetStringField(TEXT("vector_preceding_error"),Reply->Error); }Stopped=true;Reply->bSuccess=Reply->bPostconditionPassed=false;Reply->bChangeStateKnown=false;Reply->ErrorCode=Code;Reply->Error=Why;Reply->Data->SetBoolField(TEXT("vector_callback_admission_failed"),true); }return false; }
    bool Context()
    {
        if(Stopped) { return false; }
        if(!EnumContext()||GEditor!=Editor.Get()||IsPlaySessionActive()||!World.IsValid()||GEditor->GetEditorWorldContext().World()!=World.Get()||World->bIsTearingDown) { return Bad(TEXT("array_vector_context_changed"),TEXT("Captured Editor/world admission changed; no next Vector SDK or cleanup.")); }return true;
    }
    bool Check()
    {
        if(!Context()) { return false; }
        UBlueprint* B=Blueprint.Get();UEdGraph* G=Graph.Get();
        if(!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(Vector.Get())||B->GetClass()!=BlueprintClass||B->GetOuter()!=BlueprintOuter||B->GetPathName()!=BlueprintPath||B->ParentClass.Get()!=Parent.Get()
            ||B->GeneratedClass.Get()!=Generated.Get()||B->SkeletonGeneratedClass.Get()!=Skeleton.Get()||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
            ||!EnumLoaded(Generated.Get())||!EnumLoaded(Skeleton.Get())||Generated->GetClass()!=GeneratedClass||Skeleton->GetClass()!=SkeletonClass||Generated->GetOuter()!=GeneratedOuter||Skeleton->GetOuter()!=SkeletonOuter||Generated->GetPathName()!=GeneratedPath||Skeleton->GetPathName()!=SkeletonPath||Generated->GetFlags()!=GeneratedFlags||Skeleton->GetFlags()!=SkeletonFlags
            ||G->GetOuter()!=B||G->GetPathName()!=GraphPath||G->GraphGuid!=GraphGuid||G->GetPackage()!=B->GetPackage()||!G->bEditable||G->GetSchema()!=Schema.Get()
            ||Vector->GetClass()!=VectorClass||Vector->GetOuter()!=VectorOuter||Vector->GetPathName()!=VectorPath||Vector->GetFlags()!=VectorFlags||Vector->GetPackage()->IsDirty()!=VectorDirty||Vector->GetStructureSize()!=sizeof(FVector))
        { return Bad(TEXT("array_vector_target_changed"),TEXT("Selected Blueprint/graph/current classes or exact native Vector identity changed; no next SDK.")); }
        if(Node.IsValid())
        {
            UEdGraphNode* N=Node.Get();int32 Count=0;for(const auto& Current:G->Nodes) { Count+=Current.Get()==N?1:0; }
            if(!EnumLoaded(N)||N->GetClass()!=NodeClass.Get()||N->GetOuter()!=G||N->GetPackage()!=B->GetPackage()||N->GetPathName()!=NodePath||Count!=(Attached?1:0)||(GuidFixed&&N->NodeGuid!=NodeGuid)) { return Bad(TEXT("array_vector_node_changed"),TEXT("Fresh Vector node no longer has its exact current identity/ownership.")); }
            if(FunctionAssigned)
            {
                auto* Call=Cast<UK2Node_CallFunction>(N);if(!Call||!EnumLoaded(FunctionOwner.Get())||!EnumLoaded(Function.Get())||Function->GetOuterUClass()!=FunctionOwner.Get()||FunctionOwner->GetDefaultObject(false)!=FunctionDefault.Get()
                    ||Call->FunctionReference.GetMemberParentClass()!=FunctionOwner.Get()||Call->FunctionReference.GetMemberName()!=Function->GetFName()||Call->FunctionReference.GetMemberGuid().IsValid()||Call->FunctionReference.IsSelfContext()) { return Bad(TEXT("array_vector_function_changed"),TEXT("Fresh Vector array node no longer targets the exact captured native array function.")); }
            }
            if(PinsFixed)
            {
                if(N->Pins.Num()!=Roles.Num()) { return Bad(TEXT("array_vector_pins_changed"),TEXT("Fresh Vector physical pin array changed.")); }
                for(int32 I=0;I<Roles.Num();++I) { const auto& Role=Roles[I];const UEdGraphPin* P=N->Pins[I];if(P!=Role.Address||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=Role.Id||P->PinName!=Role.Name||P->Direction!=Role.Direction||P->PinType!=Role.Type||FEnumArrayScope::PinFlags(P)!=Role.Flags||P->DefaultObject!=Role.DefaultObject||!P->DefaultTextValue.IsEmpty()||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return Bad(TEXT("array_vector_pins_changed"),TEXT("Captured Vector physical pin ID/type/flags/links changed during callback.")); } }
            }
        }
        return true;
    }
    bool Init(UBlueprint* B,UEdGraph* G,const UEdGraphSchema_K2* S,UScriptStruct* V,FResult& R)
    {
        Reply=&R;if(!EnumContext()||IsPlaySessionActive()) { return Bad(TEXT("array_vector_editor_required"),TEXT("Safe Editor GT context required before new Vector node route.")); }Editor=GEditor;World=GEditor->GetEditorWorldContext().World();
        if(!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(V)||!EnumLoaded(B->GeneratedClass.Get())||!EnumLoaded(B->SkeletonGeneratedClass.Get())) { return Bad(TEXT("array_vector_target_changed"),TEXT("Current loaded Blueprint/graph/native Vector required.")); }
        Blueprint.Reset(B);Graph.Reset(G);Schema.Reset(S);Vector.Reset(V);Parent.Reset(B->ParentClass.Get());Generated.Reset(B->GeneratedClass.Get());Skeleton.Reset(B->SkeletonGeneratedClass.Get());
        BlueprintClass=B->GetClass();BlueprintOuter=B->GetOuter();BlueprintPath=B->GetPathName();GraphPath=G->GetPathName();GraphGuid=G->GraphGuid;
        VectorClass=V->GetClass();VectorOuter=V->GetOuter();VectorPath=V->GetPathName();VectorFlags=V->GetFlags();VectorDirty=V->GetPackage()->IsDirty();
        GeneratedClass=Generated->GetClass();SkeletonClass=Skeleton->GetClass();GeneratedOuter=Generated->GetOuter();SkeletonOuter=Skeleton->GetOuter();GeneratedPath=Generated->GetPathName();SkeletonPath=Skeleton->GetPathName();GeneratedFlags=Generated->GetFlags();SkeletonFlags=Skeleton->GetFlags();return Check();
    }
    bool SetNode(UEdGraphNode* N,UClass* Class)
    {
        if(!Context()) { return false; }if(!EnumLoaded(Blueprint.Get())||!EnumLoaded(Graph.Get())||!EnumLoaded(N)||N->GetClass()!=Class||N->GetOuter()!=Graph.Get()||N->GetPackage()!=Blueprint->GetPackage()) { return Bad(TEXT("array_vector_node_changed"),TEXT("Public Vector allocation return cannot be adopted as the exact fresh node.")); }
        Node.Reset(N);NodeClass.Reset(Class);NodePath=N->GetPathName();return Check();
    }
    bool FreezePins()
    {
        if(!Check()||!Node.IsValid()||Node->Pins.Num()>512) { return false; }Roles.Reset();TSet<FGuid> Ids;
        for(UEdGraphPin* P:Node->Pins) { if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node.Get()||!P->PinId.IsValid()||Ids.Contains(P->PinId)) { return Bad(TEXT("array_vector_pins_changed"),TEXT("Fresh Vector physical pins have invalid IDs/ownership.")); }Ids.Add(P->PinId);FEnumArrayScope::FPinRole Role;Role.Address=P;Role.Id=P->PinId;Role.Name=P->PinName;Role.Direction=P->Direction;Role.Type=P->PinType;Role.Flags=FEnumArrayScope::PinFlags(P);Role.DefaultObject=P->DefaultObject;Roles.Add(MoveTemp(Role)); }PinsFixed=true;return Check();
    }
    bool ExpectType(UEdGraphPin* P,const FEdGraphPinType& Type) { if(!Check()) { return false; }for(auto& Role:Roles) { if(Role.Address==P) { Role.Type=Type;return true; } }return Bad(TEXT("array_vector_pins_changed"),TEXT("Type write does not select a captured fresh Vector pin.")); }
};

// KU finite object-array authoring; LB shares this guarded writer only for exact Core Vector4 clear/add. No global type-policy expansion.
struct FObjectArrayScope
{
    struct FRole
    {
        TStrongObjectPtr<UObject> Hold; UClass* Class=nullptr; UObject* Outer=nullptr; UPackage* Package=nullptr;
        FString Path; EObjectFlags Flags=RF_NoFlags; uint32 ClassFlags=0; int32 Size=0; bool Native=false;bool IsClass=false,ParityNative=false;uint32 PackageFlags=0;bool PackageDirty=false;
    };
    struct FPin
    {
        UEdGraphPin* Address=nullptr; FGuid Id; FName Name; EEdGraphPinDirection Direction=EGPD_MAX;
        FEdGraphPinType Type; uint32 Flags=0; UObject* Object=nullptr; FString Default,Auto; FText Text;
    };
    FResult* Reply=nullptr; bool OperationParityMode=false;bool Stopped=false,Attached=false,GuidFixed=false,PinsFixed=false,FunctionAssigned=false;
    decltype(GUndo) Undo=nullptr; TStrongObjectPtr<UEditorEngine> Editor; TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UClass> Element; TStrongObjectPtr<UObject> ElementDefault;
    TStrongObjectPtr<UScriptStruct> Vector4;TStrongObjectPtr<UPackage> Vector4Package;
    struct FValueField{const FProperty* P;FName Name;EPropertyFlags Flags;int32 Offset,Size;};TArray<FValueField> Vector4Fields;
    uint32 Vector4Flags=0;int32 Vector4Align=0;TObjectPtr<UObject> LibraryDefaultHandle;TObjectPtr<UClass> GraphSchemaHandle;
    struct FOldNode{TStrongObjectPtr<UEdGraphNode> Hold;FString Path;FGuid Guid;TArray<UEdGraphPin*> Pins;TArray<FGuid> Ids;};TArray<FOldNode> Vector4OldNodes;
    bool Vector4Mode=false;bool IsVector4()const{return Vector4Mode;}
    // LN: only the exact new SkeletalMeshComponent length route uses these resolved source/role reads.
    bool SkeletalLengthMode=false;TArray<TStrongObjectPtr<UClass>> SkeletalHierarchy;
    bool SkeletalDedupMode=false; // LP: exact clear/contains/add_unique only; the LN class/current observer is shared.
    bool NativeLengthMode=false,NativeLengthReady=false;FName NativeLengthCategory;
    struct FLengthRole{TStrongObjectPtr<UClass> Hold,Within;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UObject> Default;UClass* Class=nullptr;UObject* Outer=nullptr;UClass* Super=nullptr;FString Path,PackagePath,DefaultPath;EObjectFlags Flags=RF_NoFlags,PackageObjectFlags=RF_NoFlags;uint32 ClassFlags=0,PackageFlags=0;int32 Size=0,Alignment=0;bool Dirty=false;};
    TArray<FLengthRole> NativeLengthRoles;
    // ON: finite native Rotator/Transform type sources for Array_Length only.
    // Length never samples a struct value, CDO, default-instance buffer or runtime array.
    bool NativeStructLengthMode=false,NativeStructReady=false,NativeStructOperationMode=false;
    struct FStructLengthField{const FProperty* Address=nullptr;FFieldClass* FieldClass=nullptr;FName FieldName;uint64 Flags=0;int32 Offset=0,Size=0,ArrayDim=0;UScriptStruct* Binding=nullptr;FString BindingPath;};
    struct FStructLengthRole{TStrongObjectPtr<UScriptStruct> Type;TStrongObjectPtr<UPackage> Package;UClass* ObjectClass=nullptr;UObject* Outer=nullptr;FString Path,PackagePath;EObjectFlags ObjectFlags=RF_NoFlags,PackageObjectFlags=RF_NoFlags;uint32 StructFlags=0,PackageFlags=0;int32 Size=0,Alignment=0;bool Dirty=false;TArray<FStructLengthField> Fields;};
    TArray<FStructLengthRole> StructLengthRoles;
    bool StructLengthCurrent()
    {
        if(!NativeStructReady)return true;
        for(const auto& TypeRole:StructLengthRoles)
        {
            const auto* NativeType=TypeRole.Type.Get();const auto* NativePackage=TypeRole.Package.Get();
            if(!EnumLoaded(NativeType)||!EnumLoaded(NativePackage)||NativeType->GetClass()!=TypeRole.ObjectClass||NativeType->GetOuter()!=TypeRole.Outer||NativeType->GetPathName()!=TypeRole.Path||NativeType->GetFlags()!=TypeRole.ObjectFlags||NativeType->GetPackage()!=NativePackage||uint32(NativeType->StructFlags)!=TypeRole.StructFlags||NativeType->GetStructureSize()!=TypeRole.Size||NativeType->GetMinAlignment()!=TypeRole.Alignment||NativePackage->GetFlags()!=TypeRole.PackageObjectFlags||NativePackage->GetPackageFlags()!=TypeRole.PackageFlags||NativePackage->IsDirty()!=TypeRole.Dirty)return Bad(TEXT("native_struct_length_source_identity"));
            int32 FieldIndex=0;
            for(TFieldIterator<FProperty> It(NativeType,EFieldIterationFlags::None);It;++It,++FieldIndex)
            {
                if(!TypeRole.Fields.IsValidIndex(FieldIndex)||*It!=TypeRole.Fields[FieldIndex].Address)return Bad(TEXT("native_struct_length_field_membership"));
                const auto& FieldRole=TypeRole.Fields[FieldIndex];const auto* Physical=FieldRole.Address;
                if(Physical->GetOwnerVariant()!=NativeType||Physical->GetClass()!=FieldRole.FieldClass||Physical->GetFName()!=FieldRole.FieldName||uint64(Physical->GetPropertyFlags())!=FieldRole.Flags||Physical->ArrayDim!=FieldRole.ArrayDim||Physical->GetOffset_ForInternal()!=FieldRole.Offset||Physical->GetSize()!=FieldRole.Size)return Bad(TEXT("native_struct_length_field_layout"));
                if(FieldRole.Binding){const auto* StructField=static_cast<const FStructProperty*>(Physical);if(!StructField->Struct.IsResolved()||StructField->Struct!=FieldRole.Binding)return Bad(TEXT("native_struct_length_field_binding"));}
            }
            if(FieldIndex!=TypeRole.Fields.Num())return Bad(TEXT("native_struct_length_field_count"));
        }
        return !StructLengthRoles.IsEmpty()?true:Bad(TEXT("native_struct_length_source_inventory"));
    }
    TSharedRef<FJsonObject> StructLengthState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("struct_path"),StructLengthRoles[0].Path);J->SetBoolField(TEXT("metadata_observed"),false);J->SetBoolField(TEXT("values_sampled"),false);J->SetBoolField(TEXT("cdo_creation_requested"),false);
        TArray<TSharedPtr<FJsonValue>> Types;
        for(const auto& TypeRole:StructLengthRoles){auto T=MakeShared<FJsonObject>();T->SetStringField(TEXT("path"),TypeRole.Path);T->SetStringField(TEXT("package_path"),TypeRole.PackagePath);T->SetStringField(TEXT("package_flags_string"),LexToString(TypeRole.PackageFlags));T->SetBoolField(TEXT("package_dirty"),TypeRole.Dirty);T->SetStringField(TEXT("struct_flags_string"),LexToString(TypeRole.StructFlags));T->SetNumberField(TEXT("struct_size"),TypeRole.Size);T->SetNumberField(TEXT("struct_alignment"),TypeRole.Alignment);TArray<TSharedPtr<FJsonValue>> Fields;for(const auto& FieldRole:TypeRole.Fields){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),FieldRole.FieldName.ToString());V->SetStringField(TEXT("class"),FieldRole.FieldClass->GetName());V->SetStringField(TEXT("owner_path"),TypeRole.Path);V->SetStringField(TEXT("flags"),LexToString(FieldRole.Flags));V->SetNumberField(TEXT("offset"),FieldRole.Offset);V->SetNumberField(TEXT("size"),FieldRole.Size);V->SetNumberField(TEXT("array_dim"),FieldRole.ArrayDim);V->SetStringField(TEXT("struct_path"),FieldRole.BindingPath);Fields.Add(MakeShared<FJsonValueObject>(V));}T->SetArrayField(TEXT("native_fields"),Fields);Types.Add(MakeShared<FJsonValueObject>(T));}J->SetArrayField(TEXT("structs"),Types);return J;
    }
    bool PrepareStructLength(bool TransformType,FResult& R,bool OperationMode=false)
    {
        Reply=&R;NativeStructLengthMode=true;NativeStructOperationMode=OperationMode;
        if(!EnumContext()||GIsImportingT3D||IsPlaySessionActive()||!EnumLoaded(GEditor))return Bad(TEXT("native_struct_length_editor_state"));
        Editor.Reset(GEditor);World.Reset(GWorld);Undo=GUndo;if(!Context())return Bad(TEXT("native_struct_length_context"));
        const TCHAR* Paths[]={TransformType?TEXT("/Script/CoreUObject.Transform"):TEXT("/Script/CoreUObject.Rotator"),TEXT("/Script/CoreUObject.Quat"),TEXT("/Script/CoreUObject.Vector")};
        for(int32 TypeIndex=0;TypeIndex<(TransformType?3:1);++TypeIndex)
        {
            UScriptStruct* NativeType=FindObject<UScriptStruct>(nullptr,Paths[TypeIndex]);TStrongObjectPtr<UScriptStruct> ReturnedType(NativeType);if(!Check())return false;
            if(!EnumLoaded(NativeType))return Bad(TEXT("native_struct_length_loaded_type"));
            UScriptStruct* ExpectedType=TypeIndex==0?(TransformType?TBaseStructure<FTransform>::Get():TBaseStructure<FRotator>::Get()):TypeIndex==1?TBaseStructure<FQuat>::Get():TBaseStructure<FVector>::Get();TStrongObjectPtr<UScriptStruct> ExpectedHold(ExpectedType);if(!Check())return false;
            if(NativeType!=ExpectedType||NativeType->GetPathName()!=Paths[TypeIndex]||NativeType->IsEditorOnly())return Bad(TEXT("native_struct_length_exact_type"));
            FStructLengthRole TypeRole;TypeRole.Type.Reset(NativeType);TypeRole.Package.Reset(NativeType->GetPackage());
            if(!EnumLoaded(TypeRole.Package.Get())||TypeRole.Package->GetName()!=TEXT("/Script/CoreUObject")||TypeRole.Package->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||NativeType->GetStructureSize()<=0||NativeType->GetMinAlignment()<=0)return Bad(TEXT("native_struct_length_native_package"));
            TypeRole.Path=NativeType->GetPathName();TypeRole.PackagePath=TypeRole.Package->GetName();TypeRole.ObjectClass=NativeType->GetClass();TypeRole.Outer=NativeType->GetOuter();TypeRole.ObjectFlags=NativeType->GetFlags();TypeRole.PackageObjectFlags=TypeRole.Package->GetFlags();TypeRole.PackageFlags=TypeRole.Package->GetPackageFlags();TypeRole.Dirty=TypeRole.Package->IsDirty();TypeRole.StructFlags=uint32(NativeType->StructFlags);TypeRole.Size=NativeType->GetStructureSize();TypeRole.Alignment=NativeType->GetMinAlignment();
            if(!Hold(NativeType)||!Hold(TypeRole.Package.Get()))return false;StructLengthRoles.Add(MoveTemp(TypeRole));
        }
        for(int32 TypeIndex=0;TypeIndex<StructLengthRoles.Num();++TypeIndex)
        {
            auto& TypeRole=StructLengthRoles[TypeIndex];TSet<FName> Seen;
            for(TFieldIterator<FProperty> It(TypeRole.Type.Get(),EFieldIterationFlags::None);It;++It)
            {
                const auto* Physical=*It;const FName FieldName=Physical->GetFName();UScriptStruct* BoundType=nullptr;
                if(Physical->GetOwnerVariant()!=TypeRole.Type.Get()||Seen.Contains(FieldName)||Physical->ArrayDim!=1||Physical->GetOffset_ForInternal()<0||Physical->GetSize()<=0||int64(Physical->GetOffset_ForInternal())+Physical->GetSize()>TypeRole.Size)return Bad(TEXT("native_struct_length_direct_field"));
                if(TransformType&&TypeIndex==0)
                {
                    if(FieldName==TEXT("Rotation"))BoundType=StructLengthRoles[1].Type.Get();else if(FieldName==TEXT("Translation")||FieldName==TEXT("Scale3D"))BoundType=StructLengthRoles[2].Type.Get();else return Bad(TEXT("native_struct_length_transform_field_name"));
                    if(Physical->GetClass()!=FStructProperty::StaticClass())return Bad(TEXT("native_struct_length_transform_field_class"));const auto* StructField=static_cast<const FStructProperty*>(Physical);if(!StructField->Struct.IsResolved()||StructField->Struct!=BoundType)return Bad(TEXT("native_struct_length_transform_binding"));
                }
                else
                {
                    const bool Named=(!TransformType?(FieldName==TEXT("Pitch")||FieldName==TEXT("Yaw")||FieldName==TEXT("Roll")):(FieldName==TEXT("X")||FieldName==TEXT("Y")||FieldName==TEXT("Z")||(TypeIndex==1&&FieldName==TEXT("W"))));
                    if(!Named||Physical->GetClass()!=FDoubleProperty::StaticClass())return Bad(TEXT("native_struct_length_numeric_field"));
                }
                Seen.Add(FieldName);FStructLengthField FieldRole;FieldRole.Address=Physical;FieldRole.FieldClass=Physical->GetClass();FieldRole.FieldName=FieldName;FieldRole.Flags=uint64(Physical->GetPropertyFlags());FieldRole.Offset=Physical->GetOffset_ForInternal();FieldRole.Size=Physical->GetSize();FieldRole.ArrayDim=Physical->ArrayDim;FieldRole.Binding=BoundType;FieldRole.BindingPath=BoundType?(TypeIndex==0&&FieldName==TEXT("Rotation")?StructLengthRoles[1].Path:StructLengthRoles[2].Path):FString();TypeRole.Fields.Add(MoveTemp(FieldRole));
            }
            if(Seen.Num()!=((TransformType&&TypeIndex==1)?4:3))return Bad(TEXT("native_struct_length_native_field_inventory"));
        }
        NativeStructReady=true;if(!Check())return false;R.Data->SetObjectField(NativeStructOperationMode?TEXT("native_value_struct_array_operation_source_before"):TEXT("native_value_struct_array_length_source_before"),StructLengthState());return true;
    }
    // OQ: loaded saved-clean scalar UDS Length. No instance/default-value buffer is read.
    bool UdsLengthMode=false,UdsOperationMode=false,UdsReady=false,UdsMetadataReady=false;TFunction<bool()> UdsPhysicalCurrent;
    TStrongObjectPtr<UUserDefinedStruct> UdsType;TStrongObjectPtr<UUserDefinedStructEditorData> UdsEditor;
    TStrongObjectPtr<UPackage> UdsPackage;FGuid UdsGuid;uint32 UdsFlags=0,UdsPackageFlags=0;int32 UdsSize=0,UdsAlignment=0;
    FString UdsPath,UdsEditorPath,UdsPackagePath,UdsEditorTooltip,UdsFilename,UdsSHA1;int64 UdsFileBytes=-1;
    TMap<FName,FString> UdsMetadata,UdsEditorMetadata,UdsEditorStoredMetadata;
    struct FUdsLengthField{const FProperty* Address=nullptr;FFieldClass* FieldClass=nullptr;FName InternalName,Category,SubCategory;FGuid Guid;FString FriendlyName,AuthoredName,Tooltip,SubtypePath;FEdGraphTerminalType Terminal;uint32 DescriptorBits=0;uint64 Flags=0;int32 Offset=0,Size=0;TMap<FName,FString> Metadata,DescriptorMetadata;};
    TArray<FUdsLengthField> UdsFields;
    static bool UdsMapSame(const TMap<FName,FString>& A,const TMap<FName,FString>& B)
    {if(A.Num()!=B.Num())return false;for(const auto& Pair:A){const FString* Actual=B.Find(Pair.Key);if(!Actual||!Actual->Equals(Pair.Value,ESearchCase::CaseSensitive))return false;}return true;}
    static uint32 UdsDescriptionBits(const FStructVariableDescription& D)
    {return (D.bInvalidMember?1U:0U)|(D.bDontEditOnInstance?2U:0U)|(D.bEnableSaveGame?4U:0U)|(D.bEnableMultiLineText?8U:0U)|(D.bEnable3dWidget?16U:0U);}
    bool UdsPlainCurrent()
    {
        if(!UdsReady)return true;
        auto* UserType=UdsType.Get();auto* Data=UdsEditor.Get();
        if(!EnumLoaded(UserType)||!EnumLoaded(Data)||!EnumLoaded(UdsPackage.Get())||UserType->GetClass()!=UUserDefinedStruct::StaticClass()||UserType->GetOuter()!=UdsPackage.Get()||!UserType->GetPathName().Equals(UdsPath,ESearchCase::CaseSensitive)||UserType->GetPackage()!=UdsPackage.Get()||UserType->Status!=UDSS_UpToDate||UserType->Guid!=UdsGuid||!UserType->PrimaryStruct.IsExplicitlyNull()||!UserType->EditorData.IsResolved()||UserType->EditorData!=Data||uint32(UserType->StructFlags)!=UdsFlags||UserType->GetStructureSize()!=UdsSize||UserType->GetMinAlignment()!=UdsAlignment||UserType->GetSuperStruct()!=nullptr||UdsPackage->GetPackageFlags()!=UdsPackageFlags||UdsPackage->IsDirty()||Data->GetClass()!=UUserDefinedStructEditorData::StaticClass()||Data->GetOuter()!=UserType||!Data->GetPathName().Equals(UdsEditorPath,ESearchCase::CaseSensitive))return Bad(TEXT("scalar_uds_length_source_role"));
        if(Data->VariablesDescriptions.Num()!=UdsFields.Num()||!Data->ToolTip.Equals(UdsEditorTooltip,ESearchCase::CaseSensitive)||!UdsMapSame(Data->MetaData,UdsEditorStoredMetadata))return Bad(TEXT("scalar_uds_length_editor_inventory"));
        int32 FieldIndex=0;
        for(TFieldIterator<FProperty> It(UserType,EFieldIterationFlags::None);It;++It,++FieldIndex)
        {
            if(!UdsFields.IsValidIndex(FieldIndex)||*It!=UdsFields[FieldIndex].Address)return Bad(TEXT("scalar_uds_length_field_membership"));
            const auto& FieldRole=UdsFields[FieldIndex];const auto* Physical=FieldRole.Address;
            if(Physical->GetOwnerVariant()!=UserType||Physical->GetClass()!=FieldRole.FieldClass||Physical->GetFName()!=FieldRole.InternalName||Physical->ArrayDim!=1||Physical->GetOffset_ForInternal()!=FieldRole.Offset||Physical->GetSize()!=FieldRole.Size||uint64(Physical->GetPropertyFlags())!=FieldRole.Flags)return Bad(TEXT("scalar_uds_length_field_identity"));
            const auto* Meta=Physical->GetMetaDataMap();if((Meta?Meta->Num():0)!=FieldRole.Metadata.Num()||(Meta&&!UdsMapSame(*Meta,FieldRole.Metadata)))return Bad(TEXT("scalar_uds_length_field_metadata"));
            const auto* Description=Data->VariablesDescriptions.FindByPredicate([&](const FStructVariableDescription& D){return D.VarName==FieldRole.InternalName;});
            if(!Description||Description->VarGuid!=FieldRole.Guid||!Description->FriendlyName.Equals(FieldRole.FriendlyName,ESearchCase::CaseSensitive)||Description->Category!=FieldRole.Category||Description->SubCategory!=FieldRole.SubCategory||Description->ContainerType!=EPinContainerType::None||!Description->SubCategoryObject.ToSoftObjectPath().ToString().Equals(FieldRole.SubtypePath,ESearchCase::CaseSensitive)||Description->PinValueType!=FieldRole.Terminal||Description->PinValueType.bTerminalIsUObjectWrapper!=FieldRole.Terminal.bTerminalIsUObjectWrapper||UdsDescriptionBits(*Description)!=FieldRole.DescriptorBits||!Description->ToolTip.Equals(FieldRole.Tooltip,ESearchCase::CaseSensitive)||!UdsMapSame(Description->MetaData,FieldRole.DescriptorMetadata))return Bad(TEXT("scalar_uds_length_authored_definition"));
        }
        return FieldIndex==UdsFields.Num()?true:Bad(TEXT("scalar_uds_length_field_count"));
    }
    bool UdsQueryCurrent(){return CheckPlain()&&(!UdsPhysicalCurrent||UdsPhysicalCurrent());}
    bool UdsMetadataQuery(bool Capture)
    {
        for(int32 Index=0;Index<2;++Index)
        {
            UObject* Source=Index==0?static_cast<UObject*>(UdsType.Get()):static_cast<UObject*>(UdsEditor.Get());
            const auto* Returned=FMetaData::GetMapForObject(Source);TMap<FName,FString> Copy;if(Returned){if(Returned->Num()>256)return Bad(TEXT("scalar_uds_length_metadata_bound"));Copy=*Returned;}
            if(!UdsQueryCurrent())return false;
            auto& Expected=Index==0?UdsMetadata:UdsEditorMetadata;if(Capture)Expected=MoveTemp(Copy);else if(!UdsMapSame(Copy,Expected))return Bad(TEXT("scalar_uds_length_object_metadata"));
        }
        return true;
    }
    bool UdsAuthoredQuery(bool Capture)
    {
        if(!Check())return false;
        const auto* Returned=FStructureEditorUtils::GetVarDescPtr(static_cast<const UUserDefinedStruct*>(UdsType.Get()));if(!Check())return false;
        if(Returned!=&UdsEditor->VariablesDescriptions)return Bad(TEXT("scalar_uds_length_descriptor_return"));
        for(auto& FieldRole:UdsFields)
        {
            const FGuid ActualGuid=FStructureEditorUtils::GetGuidForProperty(FieldRole.Address);if(!Check())return false;
            if(ActualGuid!=FieldRole.Guid)return Bad(TEXT("scalar_uds_length_authored_guid"));
            const FString Authored=FieldRole.Address->GetAuthoredName();if(!Check())return false;
            if(Authored.IsEmpty()||(!Capture&&!Authored.Equals(FieldRole.AuthoredName,ESearchCase::CaseSensitive)))return Bad(TEXT("scalar_uds_length_authored_name"));if(Capture)FieldRole.AuthoredName=Authored;
        }
        return Check();
    }
    bool UdsFileQuery(bool Capture)
    {
        FString Filename;const bool Exists=FPackageName::DoesPackageExist(UdsPackagePath,&Filename);if(!Check())return false;
        const int64 Bytes=Exists?IFileManager::Get().FileSize(*Filename):-1;
        if(!Exists||Bytes<0||Bytes>268435456)return Bad(TEXT("scalar_uds_length_saved_source_file"));
        TArray<uint8> Storage;const bool Read=FFileHelper::LoadFileToArray(Storage,*Filename);if(!Check())return false;
        if(!Read||Storage.Num()!=Bytes)return Bad(TEXT("scalar_uds_length_source_file_read"));FSHAHash Hash;FSHA1::HashBuffer(Storage.GetData(),Storage.Num(),Hash.Hash);const FString Digest=Hash.ToString();
        if(Capture){UdsFilename=Filename;UdsFileBytes=Bytes;UdsSHA1=Digest;}else if(!Filename.Equals(UdsFilename,ESearchCase::CaseSensitive)||Bytes!=UdsFileBytes||!Digest.Equals(UdsSHA1,ESearchCase::CaseSensitive))return Bad(TEXT("scalar_uds_length_source_file_changed"));return true;
    }
    TSharedRef<FJsonObject> UdsState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("struct_path"),UdsPath);J->SetStringField(TEXT("struct_guid"),UdsGuid.ToString(EGuidFormats::Digits));J->SetNumberField(TEXT("status"),int32(UdsType->Status));J->SetNumberField(TEXT("struct_size"),UdsSize);J->SetNumberField(TEXT("struct_alignment"),UdsAlignment);J->SetStringField(TEXT("struct_flags_string"),LexToString(UdsFlags));J->SetStringField(TEXT("package_path"),UdsPackagePath);J->SetStringField(TEXT("package_flags_string"),LexToString(UdsPackageFlags));J->SetBoolField(TEXT("package_dirty"),false);J->SetStringField(TEXT("editor_data_path"),UdsEditorPath);
        for(const TCHAR* Flag:{TEXT("values_sampled"),TEXT("default_buffer_sampled"),TEXT("cdo_creation_requested"),TEXT("runtime_invocation_verified")})J->SetBoolField(Flag,false);
        auto Meta=[](const TMap<FName,FString>& M){auto O=MakeShared<FJsonObject>();for(const auto& Pair:M)O->SetStringField(Pair.Key.ToString(),Pair.Value);return O;};J->SetObjectField(TEXT("root_metadata"),Meta(UdsMetadata));J->SetObjectField(TEXT("editor_data_metadata"),Meta(UdsEditorMetadata));
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& FieldRole:UdsFields){auto F=MakeShared<FJsonObject>();F->SetStringField(TEXT("internal_name"),FieldRole.InternalName.ToString());F->SetStringField(TEXT("authored_name"),FieldRole.AuthoredName);F->SetStringField(TEXT("field_guid"),FieldRole.Guid.ToString(EGuidFormats::Digits));F->SetStringField(TEXT("class"),FieldRole.FieldClass->GetName());F->SetStringField(TEXT("owner_path"),UdsPath);F->SetStringField(TEXT("flags"),LexToString(FieldRole.Flags));F->SetNumberField(TEXT("offset"),FieldRole.Offset);F->SetNumberField(TEXT("size"),FieldRole.Size);F->SetNumberField(TEXT("array_dim"),1);F->SetStringField(TEXT("category"),FieldRole.Category.ToString());F->SetStringField(TEXT("subcategory"),FieldRole.SubCategory.ToString());F->SetObjectField(TEXT("metadata"),Meta(FieldRole.Metadata));Rows.Add(MakeShared<FJsonValueObject>(F));}J->SetArrayField(TEXT("fields"),Rows);
        auto File=MakeShared<FJsonObject>();File->SetStringField(TEXT("filename"),UdsFilename);File->SetBoolField(TEXT("exists"),true);File->SetNumberField(TEXT("bytes"),UdsFileBytes);File->SetStringField(TEXT("sha1"),UdsSHA1);J->SetObjectField(TEXT("file"),File);return J;
    }
    bool PrepareUdsLength(const FString& Requested,FResult& R)
    {
        Reply=&R;UdsLengthMode=true;
        if(!EnumContext()||GIsImportingT3D||IsPlaySessionActive()||!EnumLoaded(GEditor))return Bad(TEXT("scalar_uds_length_editor_state"));Editor.Reset(GEditor);World.Reset(GWorld);Undo=GUndo;if(!Context())return Bad(TEXT("scalar_uds_length_context"));
        if(!Function::Pin::SafeName(Requested)||!Requested.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidObjectPath(Requested)||Requested.Contains(TEXT(":"))||Requested.Contains(TEXT("'")))return Error(R,TEXT("array_scalar_uds_path_pending"),TEXT("This slice requires an exact full Game object path; loader, aliases, other ScriptStructs and nested/property families remain incomplete."));
        UScriptStruct* Raw=FindObject<UScriptStruct>(nullptr,*Requested);TStrongObjectPtr<UScriptStruct> Returned(Raw);if(!CheckPlain())return false;
        if(!EnumLoaded(Raw)||Raw->GetClass()!=UUserDefinedStruct::StaticClass())return Error(R,TEXT("array_scalar_uds_not_loaded"),TEXT("An already-loaded exact UserDefinedStruct is required; use an explicit asset preload or existing MakeArray resolver first. No loader is dispatched here."));
        auto* UserType=static_cast<UUserDefinedStruct*>(Raw);UdsType.Reset(UserType);UdsPackage.Reset(UserType->GetPackage());
        if(!EnumLoaded(UdsPackage.Get())||UserType->GetPathName()!=Requested||UserType->GetOuter()!=UdsPackage.Get()||UserType->Status!=UDSS_UpToDate||!UserType->Guid.IsValid()||!UserType->PrimaryStruct.IsExplicitlyNull()||!UserType->EditorData.IsResolved()||UserType->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||UserType->StructFlags&STRUCT_Native||UdsPackage->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor)||UdsPackage->IsDirty()||UserType->GetSuperStruct()!=nullptr)return Error(R,TEXT("array_scalar_uds_source_pending"),TEXT("Requires saved-clean compiled primary runtime UDS without inherited fields; no source creation, repair, compilation or saving."));
        UObject* EditorRaw=UserType->EditorData.Get();TStrongObjectPtr<UObject> ReturnedEditor(EditorRaw);if(!EnumLoaded(EditorRaw)||EditorRaw->GetClass()!=UUserDefinedStructEditorData::StaticClass()||EditorRaw->GetOuter()!=UserType)return Bad(TEXT("scalar_uds_length_editor_data"));UdsEditor.Reset(static_cast<UUserDefinedStructEditorData*>(EditorRaw));
        UdsPath=UserType->GetPathName();UdsEditorPath=EditorRaw->GetPathName();UdsPackagePath=UdsPackage->GetName();UdsGuid=UserType->Guid;UdsFlags=uint32(UserType->StructFlags);UdsPackageFlags=UdsPackage->GetPackageFlags();UdsSize=UserType->GetStructureSize();UdsAlignment=UserType->GetMinAlignment();UdsEditorTooltip=UdsEditor->ToolTip;UdsEditorStoredMetadata=UdsEditor->MetaData;
        if(UdsSize<1||UdsSize>65536||UdsAlignment<=0||UdsEditorStoredMetadata.Num()>256)return Error(R,TEXT("array_scalar_uds_layout_bound"),TEXT("Compiled scalar UDS layout must fit 1..65536 bytes and bounded metadata."));
        for(UObject* O:{static_cast<UObject*>(UserType),EditorRaw,static_cast<UObject*>(UdsPackage.Get()),static_cast<UObject*>(UserType->GetClass()),static_cast<UObject*>(EditorRaw->GetClass())})if(!Hold(O))return false;
        TSet<FName> SeenNames;TSet<FGuid> SeenGuids;
        for(TFieldIterator<FProperty> It(UserType,EFieldIterationFlags::None);It;++It)
        {
            const auto* Physical=*It;auto* Kind=Physical->GetClass();FName Category,SubCategory;
            if(Kind==FStrProperty::StaticClass())Category=UEdGraphSchema_K2::PC_String;else if(Kind==FIntProperty::StaticClass())Category=UEdGraphSchema_K2::PC_Int;else if(Kind==FInt64Property::StaticClass())Category=UEdGraphSchema_K2::PC_Int64;else if(Kind==FFloatProperty::StaticClass()){Category=UEdGraphSchema_K2::PC_Real;SubCategory=UEdGraphSchema_K2::PC_Float;}else if(Kind==FDoubleProperty::StaticClass()){Category=UEdGraphSchema_K2::PC_Real;SubCategory=UEdGraphSchema_K2::PC_Double;}else if(Kind==FBoolProperty::StaticClass())Category=UEdGraphSchema_K2::PC_Boolean;else if(Kind==FNameProperty::StaticClass())Category=UEdGraphSchema_K2::PC_Name;else return Error(R,TEXT("array_scalar_uds_field_pending"),TEXT("Only own FString/int32/int64/float/double/bool/FName scalar fields are admitted; Text, enum, nested/object/container/accessor families remain incomplete."));
            if(UdsFields.Num()>=32||Physical->GetOwnerVariant()!=UserType||Physical->ArrayDim!=1||Physical->HasSetterOrGetter()||!Physical->HasAnyPropertyFlags(CPF_Edit)||Physical->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference)||Physical->GetOffset_ForInternal()<0||Physical->GetSize()<=0||int64(Physical->GetOffset_ForInternal())+Physical->GetSize()>UdsSize)return Error(R,TEXT("array_scalar_uds_field_pending"),TEXT("Requires 1..32 own editable scalar fields within the current compiled layout; no accessor/instanced/transient fields."));
            const auto* Description=UdsEditor->VariablesDescriptions.FindByPredicate([&](const FStructVariableDescription& D){return D.VarName==Physical->GetFName();});
            if(!Description||!Description->VarGuid.IsValid()||SeenNames.Contains(Description->VarName)||SeenGuids.Contains(Description->VarGuid)||Description->bInvalidMember||Description->ContainerType!=EPinContainerType::None||!Description->SubCategoryObject.IsNull()||Description->Category!=Category||Description->SubCategory!=SubCategory||Description->MetaData.Num()>256)return Error(R,TEXT("array_scalar_uds_descriptor_pending"),TEXT("Current stock EditorData must map every direct scalar field to a unique authored GUID and matching scalar descriptor."));
            FUdsLengthField FieldRole;FieldRole.Address=Physical;FieldRole.FieldClass=Kind;FieldRole.InternalName=Physical->GetFName();FieldRole.Category=Category;FieldRole.SubCategory=SubCategory;FieldRole.Guid=Description->VarGuid;FieldRole.FriendlyName=Description->FriendlyName;FieldRole.Tooltip=Description->ToolTip;FieldRole.SubtypePath=Description->SubCategoryObject.ToSoftObjectPath().ToString();FieldRole.Terminal=Description->PinValueType;FieldRole.DescriptorBits=UdsDescriptionBits(*Description);FieldRole.DescriptorMetadata=Description->MetaData;FieldRole.Flags=uint64(Physical->GetPropertyFlags());FieldRole.Offset=Physical->GetOffset_ForInternal();FieldRole.Size=Physical->GetSize();if(const auto* M=Physical->GetMetaDataMap()){if(M->Num()>256)return Error(R,TEXT("array_scalar_uds_metadata_bound"),TEXT("Field metadata exceeds bounded observation."));FieldRole.Metadata=*M;}SeenNames.Add(FieldRole.InternalName);SeenGuids.Add(FieldRole.Guid);UdsFields.Add(MoveTemp(FieldRole));
        }
        if(UdsFields.IsEmpty()||UdsFields.Num()!=UdsEditor->VariablesDescriptions.Num())return Error(R,TEXT("array_scalar_uds_descriptor_pending"),TEXT("The entire current compiled scalar field and authored descriptor inventories must match."));
        UdsReady=true;return CheckPlain();
    }
    bool BindUdsSource()
    {
        if(!UdsLengthMode||!UdsPhysicalCurrent||!UdsQueryCurrent()||!UdsMetadataQuery(true))return false;UdsMetadataReady=true;
        if(!UdsAuthoredQuery(true)||!UdsFileQuery(true)||!Check())return false;Reply->Data->SetObjectField(UdsOperationMode?TEXT("native_scalar_uds_array_operation_source_before"):TEXT("native_scalar_uds_array_length_source_before"),UdsState());return true;
    }
    bool ResolvedRoles()const{return IsVector4()||SkeletalLengthMode||NativeLengthMode||NativeStructLengthMode||UdsLengthMode;}
    bool NativeLengthCurrent()
    {
        if(!NativeLengthReady)return true;
        int32 Index=0;
        for(auto It=Element->GetSuperStructIterator();It;++It,++Index)
        {
            if(!NativeLengthRoles.IsValidIndex(Index)||*It!=NativeLengthRoles[Index].Hold.Get())return Bad(TEXT("descriptor_length_hierarchy"));
            const auto& Role=NativeLengthRoles[Index];auto* K=Role.Hold.Get();
            if(!EnumLoaded(K)||!EnumLoaded(Role.Package.Get())||K->GetClass()!=Role.Class||K->GetOuter()!=Role.Outer||K->GetPathName()!=Role.Path||K->GetPackage()!=Role.Package.Get()||K->GetFlags()!=Role.Flags||uint32(K->GetClassFlags())!=Role.ClassFlags||K->GetStructureSize()!=Role.Size||K->GetMinAlignment()!=Role.Alignment||Role.Package->GetFlags()!=Role.PackageObjectFlags||Role.Package->GetPackageFlags()!=Role.PackageFlags||Role.Package->IsDirty()!=Role.Dirty||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy!=nullptr||!K->ClassWithin.IsResolved()||K->ClassWithin!=Role.Within.Get())return Bad(TEXT("descriptor_length_source_identity"));
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto Handle=K->ClassDefaultObject;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!Handle.IsResolved()||Handle!=Role.Default.Get())return Bad(TEXT("descriptor_length_optional_cdo_slot"));
        }
        return Index==NativeLengthRoles.Num()&&Index>0&&NativeLengthRoles[0].Hold.Get()==Element.Get()&&NativeLengthRoles[0].Default.Get()==ElementDefault.Get()?true:Bad(TEXT("descriptor_length_source_inventory"));
    }
    TSharedRef<FJsonObject> NativeLengthState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("class_path"),Element->GetPathName());J->SetStringField(TEXT("element_category"),NativeLengthCategory.ToString());J->SetBoolField(TEXT("existing_cdo_resolved"),true);J->SetBoolField(TEXT("existing_cdo_is_null"),ElementDefault.Get()==nullptr);J->SetStringField(TEXT("existing_cdo_path"),NativeLengthRoles[0].DefaultPath);J->SetBoolField(TEXT("cdo_values_sampled"),false);J->SetBoolField(TEXT("cdo_creation_requested"),false);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& Role:NativeLengthRoles){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),Role.Path);V->SetStringField(TEXT("class_flags_string"),LexToString(Role.ClassFlags));V->SetNumberField(TEXT("class_size"),Role.Size);V->SetNumberField(TEXT("class_alignment"),Role.Alignment);V->SetStringField(TEXT("package_path"),Role.PackagePath);V->SetStringField(TEXT("package_flags_string"),LexToString(Role.PackageFlags));V->SetBoolField(TEXT("package_dirty"),Role.Dirty);V->SetStringField(TEXT("super_path"),Role.Super?Role.Super->GetPathName():FString());V->SetStringField(TEXT("within_path"),Role.Within.IsValid()?Role.Within->GetPathName():FString());V->SetStringField(TEXT("existing_cdo_path"),Role.DefaultPath);V->SetBoolField(TEXT("existing_cdo_is_null"),Role.Default.Get()==nullptr);V->SetObjectField(TEXT("metadata"),MakeShared<FJsonObject>());V->SetBoolField(TEXT("metadata_observed"),false);Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("hierarchy"),Rows);return J;
    }
    bool PrepareNativeLength(const FString& RequestedPath,const FName& Category,FResult& R)
    {
        Reply=&R;NativeLengthMode=true;NativeLengthCategory=Category;
        if(!EnumContext()||GIsImportingT3D||IsPlaySessionActive()||!EnumLoaded(GEditor))return Bad(TEXT("descriptor_length_editor_state"));
        Editor.Reset(GEditor);World.Reset(GWorld);Undo=GUndo;if(!Context())return Bad(TEXT("descriptor_length_context"));
        UClass* RootClass=FindObject<UClass>(nullptr,*RequestedPath);TStrongObjectPtr<UClass> Returned(RootClass);if(!Context())return Bad(TEXT("descriptor_length_find_return_context"));
        if(!EnumLoaded(RootClass))return Bad(TEXT("descriptor_length_loaded_class"));
        UClass* Expected=RequestedPath==TEXT("/Script/CoreUObject.Object")?UObject::StaticClass():RequestedPath==TEXT("/Script/Engine.Actor")?AActor::StaticClass():RequestedPath==TEXT("/Script/Engine.ActorComponent")?UActorComponent::StaticClass():RequestedPath==TEXT("/Script/Engine.ChildActorComponent")?UChildActorComponent::StaticClass():RequestedPath==TEXT("/Script/Engine.SkeletalMeshComponent")?USkeletalMeshComponent::StaticClass():nullptr;
        if(!Expected||RootClass!=Expected||RootClass->GetPathName()!=RequestedPath)return Bad(TEXT("descriptor_length_exact_class"));
        Element.Reset(RootClass);
        for(auto It=RootClass->GetSuperStructIterator();It;++It)
        {
            auto* K=Cast<UClass>(*It);if(!EnumLoaded(K)||NativeLengthRoles.Num()>=32)return Bad(TEXT("descriptor_length_native_hierarchy"));
            FLengthRole Role;Role.Hold.Reset(K);Role.Package.Reset(K->GetPackage());
            if(!EnumLoaded(Role.Package.Get())||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy!=nullptr||!K->ClassWithin.IsResolved()||K->IsEditorOnly()||(Role.Package->GetName()!=TEXT("/Script/CoreUObject")&&Role.Package->GetName()!=TEXT("/Script/Engine")))return Bad(TEXT("descriptor_length_native_source"));
            Role.Class=K->GetClass();Role.Outer=K->GetOuter();Role.Path=K->GetPathName();Role.PackagePath=Role.Package->GetName();Role.Flags=K->GetFlags();Role.ClassFlags=uint32(K->GetClassFlags());Role.Size=K->GetStructureSize();Role.Alignment=K->GetMinAlignment();Role.PackageFlags=Role.Package->GetPackageFlags();Role.PackageObjectFlags=Role.Package->GetFlags();Role.Dirty=Role.Package->IsDirty();Role.Within.Reset(K->ClassWithin.Get());auto Next=It;++Next;Role.Super=Next?Cast<UClass>(*Next):nullptr;
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto Handle=K->ClassDefaultObject;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!Handle.IsResolved())return Bad(TEXT("descriptor_length_optional_cdo_unresolved"));Role.Default.Reset(Handle.Get());
            if(Role.Default.IsValid()){if(!EnumLoaded(Role.Default.Get())||!Hold(Role.Default.Get()))return Bad(TEXT("descriptor_length_optional_cdo_unavailable"));Role.DefaultPath=Role.Default->GetPathName();}
            if(!Hold(K)||(Role.Within.IsValid()&&!Hold(Role.Within.Get())))return false;
            NativeLengthRoles.Add(MoveTemp(Role));
        }
        if(NativeLengthRoles.IsEmpty())return Bad(TEXT("descriptor_length_empty_hierarchy"));ElementDefault.Reset(NativeLengthRoles[0].Default.Get());NativeLengthReady=true;
        if(!Check())return false;R.Data->SetObjectField(TEXT("native_descriptor_array_length_source_before"),NativeLengthState());return true;
    }
    bool SkeletalSourceCurrent()
    {
        if(!SkeletalLengthMode||!EnumLoaded(Element.Get())||Element.Get()!=USkeletalMeshComponent::StaticClass()||SkeletalHierarchy.Num()!=7)return Bad(TEXT("skeletal_native_element"));
        int32 Index=0;for(auto It=Element->GetSuperStructIterator();It;++It,++Index)
        {
            const auto* C=Cast<UClass>(*It);if(Index>=SkeletalHierarchy.Num()||C!=SkeletalHierarchy[Index].Get()||!EnumLoaded(C)||!EnumLoaded(C->GetPackage())
                ||C->GetPackage()->GetPackageFlags()!=16U||C->GetPackage()->IsDirty())return Bad(TEXT("skeletal_native_hierarchy"));
        }
        if(Index!=SkeletalHierarchy.Num()||!EnumLoaded(ElementDefault.Get()))return Bad(TEXT("skeletal_native_cdo"));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto H=Element->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        return H.IsResolved()&&H==ElementDefault.Get()?true:Bad(TEXT("skeletal_native_cdo_handle"));
    }
    TSharedRef<FJsonObject> SkeletalState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);
        J->SetStringField(TEXT("class_path"),TEXT("/Script/Engine.SkeletalMeshComponent"));J->SetStringField(TEXT("existing_cdo_path"),ElementDefault->GetPathName());J->SetBoolField(TEXT("cdo_values_sampled"),false);J->SetBoolField(TEXT("cdo_creation_requested"),false);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& C:SkeletalHierarchy){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),C->GetPathName());V->SetStringField(TEXT("class_flags_string"),LexToString(uint32(C->GetClassFlags())));V->SetNumberField(TEXT("class_size"),C->GetStructureSize());V->SetStringField(TEXT("package_path"),C->GetPackage()->GetName());V->SetStringField(TEXT("package_flags_string"),LexToString(uint32(C->GetPackage()->GetPackageFlags())));V->SetBoolField(TEXT("package_dirty"),C->GetPackage()->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("hierarchy"),Rows);return J;
    }
    bool Vector4Current()
    {
        if(!EnumLoaded(Vector4.Get())||!EnumLoaded(Vector4Package.Get())||Vector4->GetPackage()!=Vector4Package.Get()
            ||Vector4->GetPathName()!=TEXT("/Script/CoreUObject.Vector4")||Vector4->GetStructureSize()!=sizeof(FVector4)||Vector4->GetMinAlignment()!=Vector4Align||Vector4Align!=alignof(FVector4)||uint32(Vector4->StructFlags)!=Vector4Flags
            ||Vector4Package->GetName()!=TEXT("/Script/CoreUObject")||Vector4Package->GetPackageFlags()!=16U||Vector4Package->IsDirty())return Bad(TEXT("vector4_native_source"));
        int32 Count=0;for(TFieldIterator<FProperty> It(Vector4.Get(),EFieldIterationFlags::None);It;++It)++Count;
        if(Count!=4||Vector4Fields.Num()!=4)return Bad(TEXT("vector4_native_field_count"));
        for(const auto& F:Vector4Fields)if(Vector4->FindPropertyByName(F.Name)!=F.P||F.P->GetClass()!=FDoubleProperty::StaticClass()||F.P->GetOwnerStruct()!=Vector4.Get()||F.P->ArrayDim!=1
            ||F.P->GetPropertyFlags()!=F.Flags||F.P->GetOffset_ForInternal()!=F.Offset||F.P->GetSize()!=F.Size)return Bad(TEXT("vector4_native_field_layout"));
        return true;
    }
    TSharedRef<FJsonObject> Vector4State()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("struct_path"),TEXT("/Script/CoreUObject.Vector4"));J->SetStringField(TEXT("package_path"),TEXT("/Script/CoreUObject"));J->SetStringField(TEXT("package_flags_string"),TEXT("16"));J->SetBoolField(TEXT("package_dirty"),false);J->SetStringField(TEXT("struct_flags_string"),LexToString(Vector4Flags));J->SetNumberField(TEXT("struct_size"),sizeof(FVector4));J->SetNumberField(TEXT("struct_alignment"),Vector4Align);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& F:Vector4Fields){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),F.Name.ToString());V->SetStringField(TEXT("class"),TEXT("DoubleProperty"));V->SetStringField(TEXT("flags"),LexToString(uint64(F.Flags)));V->SetNumberField(TEXT("offset"),F.Offset);V->SetNumberField(TEXT("size"),F.Size);Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("native_fields"),Rows);return J;
    }

    TStrongObjectPtr<UBlueprint> Blueprint; TStrongObjectPtr<UEdGraph> Graph; TStrongObjectPtr<UK2Node_CallArrayFunction> Node;
    TStrongObjectPtr<const UEdGraphSchema_K2> Schema; TStrongObjectPtr<UClass> FunctionOwner;
    TStrongObjectPtr<UFunction> Function; TStrongObjectPtr<UObject> FunctionDefault;
    UClass *Parent=nullptr,*Generated=nullptr,*Skeleton=nullptr; FGuid GraphGuid,NodeGuid; FString NodePath;
    FMemberReference Member; int32 X=0,Y=0; EObjectFlags NodeFlags=RF_NoFlags; TArray<FRole> Holds; TArray<FPin> Pins;
    bool Bad(const TCHAR* Boundary)
    {
        if(!Stopped)
        {
            if(OperationParityMode||NativeLengthMode||NativeStructLengthMode||UdsLengthMode){Reply->Data->SetStringField(TEXT("object_array_failed_stage"),Reply->Stage);Reply->Data->SetBoolField(TEXT("object_array_failure_is_compound_guard"),true);Reply->Data->SetBoolField(TEXT("object_array_expected_actual_observed"),false);}
            Stopped=true; Reply->bSuccess=Reply->bPostconditionPassed=false;Reply->bChangeStateKnown=false;Reply->Stage=TEXT("verification");
            Reply->Data->SetStringField(TEXT("object_array_failed_boundary"),Boundary);Reply->Data->SetBoolField(TEXT("object_array_followup_sdk_stopped"),true);
            if(!Reply->ErrorCode.IsEmpty()) { Reply->Data->SetStringField(TEXT("object_array_preceding_error_code"),Reply->ErrorCode); }
            Reply->ErrorCode=IsVector4()?TEXT("array_vector4_scope_changed"):TEXT("array_object_scope_changed");Reply->Error=TEXT("Current exact object-array source/target/node/pin role changed or is unobservable; actual allocation/default prefix is retained. No cleanup, rollback, compile/save or retry.");
        }
        return false;
    }
    bool Context()
    {
        if(ResolvedRoles())return !Stopped&&EnumContext()&&!GIsImportingT3D&&!IsPlaySessionActive()&&EnumLoaded(Editor.Get())&&GEditor==Editor.Get()
            &&EnumLoaded(World.Get())&&GWorld==World.Get()&&World->WorldType==EWorldType::Editor&&!World->bIsTearingDown&&GUndo==Undo;
        return !Stopped&&EnumContext()&&!GIsImportingT3D&&!IsPlaySessionActive()&&EnumLoaded(Editor.Get())&&GEditor==Editor.Get()
            &&EnumLoaded(World.Get())&&GEditor->GetEditorWorldContext().World()==World.Get()&&GWorld==World.Get()
            &&World->WorldType==EWorldType::Editor&&!World->bIsTearingDown&&GUndo==Undo;
    }
    bool Hold(UObject* O,bool Native=false)
    {
        if(!EnumLoaded(O))return Bad(TEXT("required_object_unavailable"));
        for(const auto& Role:Holds)if(Role.Hold.Get()==O)return true;
        FRole Role;Role.Hold.Reset(O);Role.Class=O->GetClass();Role.Outer=O->GetOuter();Role.Package=O->GetPackage();Role.Path=O->GetPathName();Role.Flags=O->GetFlags();Role.Native=Native;
        if(const UClass* C=Cast<UClass>(O)){Role.IsClass=true;Role.ClassFlags=uint32(C->GetClassFlags());Role.Size=C->GetStructureSize();}
        if(OperationParityMode){const auto* NativeClass=Cast<UClass>(O);Role.ParityNative=Native||(NativeClass&&NativeClass->HasAnyClassFlags(CLASS_Native)&&(Role.Package->GetName()==TEXT("/Script/Engine")||Role.Package->GetName()==TEXT("/Script/CoreUObject")));if(Role.ParityNative){Role.PackageFlags=Role.Package->GetPackageFlags();Role.PackageDirty=Role.Package->IsDirty();}}
        Holds.Add(MoveTemp(Role));return true;
    }
    bool Check()
    {
        if(!CheckPlain())return false;
        if(UdsLengthMode&&UdsPhysicalCurrent&&!UdsPhysicalCurrent())return false;
        return !UdsLengthMode||!UdsMetadataReady||UdsMetadataQuery(false);
    }
    bool CheckPlain()
    {
        if(!Context())return Bad(TEXT("context_or_transaction"));
        for(const auto& Role:Holds)
        {
            const UObject* O=Role.Hold.Get();
            if(!EnumLoaded(O)||O->GetClass()!=Role.Class||O->GetOuter()!=Role.Outer||O->GetPackage()!=Role.Package
                ||!O->GetPathName().Equals(Role.Path,ESearchCase::CaseSensitive)||O->GetFlags()!=Role.Flags)return Bad(TEXT("held_identity"));
            if(OperationParityMode&&Role.ParityNative&&(!EnumLoaded(Role.Package)||Role.Package->GetPackageFlags()!=Role.PackageFlags||Role.Package->IsDirty()!=Role.PackageDirty))return Bad(TEXT("parity_source_package"));
            if(Role.Native&&(!EnumLoaded(Role.Package)||Role.Package->GetName()!=TEXT("/Script/Engine")||Role.Package->GetPackageFlags()!=16U||Role.Package->IsDirty()))return Bad(TEXT("native_package"));
            if(const UClass* C=Cast<UClass>(O))if(uint32(C->GetClassFlags())!=Role.ClassFlags||C->GetStructureSize()!=Role.Size)return Bad(TEXT("native_or_target_class_layout"));
        }
        if(UdsLengthMode){if(!UdsPlainCurrent())return false;}else if(NativeStructLengthMode){if(!StructLengthCurrent())return false;}else if(NativeLengthMode){if(!NativeLengthCurrent())return false;}else if(IsVector4()){if(!Vector4Current())return false;}else if(SkeletalLengthMode){if(!SkeletalSourceCurrent())return false;}else if(!Element.IsValid()||Element->GetDefaultObject(false)!=ElementDefault.Get())return Bad(TEXT("native_class_cdo"));
        if(Blueprint.IsValid())
        {
            UBlueprint* B=Blueprint.Get();UEdGraph* G=Graph.Get();
            if(ResolvedRoles()&&(!B->ParentClass.GetGCPtr().IsResolved()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()
                ||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=GraphSchemaHandle||!GraphSchemaHandle.IsResolved()||GraphSchemaHandle!=Schema->GetClass()))return Bad(TEXT("vector4_current_handles"));
            if(!EnumLoaded(B)||!EnumLoaded(G)||B->ParentClass.GetGCPtr()!=Parent||B->GeneratedClass.GetGCPtr()!=Generated||B->SkeletonGeneratedClass.GetGCPtr()!=Skeleton
                ||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
                ||G->GetOuter()!=B||G->GetPackage()!=B->GetPackage()||G->GraphGuid!=GraphGuid||(!ResolvedRoles()&&G->GetSchema()!=Schema.Get())||!G->bEditable)return Bad(TEXT("current_target_roles"));
            if(ResolvedRoles())
            {
                bool Found=false;for(const auto* List:{&B->FunctionGraphs,&B->UbergraphPages,&B->EventGraphs})for(const auto& H:*List){if(!H.IsResolved())return Bad(TEXT("vector4_graph_unresolved"));Found|=H==G;}
                if(!Found)return Bad(TEXT("current_graph_membership"));
            }
            else {TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);if(!Graphs.Contains(G))return Bad(TEXT("current_graph_membership"));}
        }
        if(FunctionOwner.IsValid())
        {
            if(ResolvedRoles())
            {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
                const auto H=FunctionOwner->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
                if(!EnumLoaded(Function.Get())||Function->GetOuterUClass()!=FunctionOwner.Get()||!H.IsResolved()||H!=LibraryDefaultHandle||H!=FunctionDefault.Get())return Bad(TEXT("vector4_native_array_function"));
            }
            else if(!EnumLoaded(Function.Get())||Function->GetOuterUClass()!=FunctionOwner.Get()||FunctionOwner->GetDefaultObject(false)!=FunctionDefault.Get())return Bad(TEXT("native_array_function"));
        }
        if(IsVector4()&&Blueprint.IsValid())
        {
            if(Graph->Nodes.Num()!=Vector4OldNodes.Num()+(Attached?1:0))return Bad(TEXT("vector4_prior_inventory"));
            for(int32 I=0;I<Vector4OldNodes.Num();++I){const auto& O=Vector4OldNodes[I];const UEdGraphNode* N=O.Hold.Get();
                if(!Graph->Nodes[I].IsResolved()||Graph->Nodes[I]!=N||!EnumLoaded(N)||N->GetPathName()!=O.Path||N->GetOuter()!=Graph.Get()||N->NodeGuid!=O.Guid||N->Pins.Num()!=O.Pins.Num())return Bad(TEXT("vector4_prior_node_role"));
                for(int32 P=0;P<O.Pins.Num();++P)if(N->Pins[P]!=O.Pins[P]||!O.Pins[P]||O.Pins[P]->WasTrashed()||O.Pins[P]->GetOwningNodeUnchecked()!=N||O.Pins[P]->PinId!=O.Ids[P])return Bad(TEXT("vector4_prior_pin_role"));
            }
        }
        if(Node.IsValid())
        {
            auto* N=Node.Get();int32 Count=0;for(const auto& It:Graph->Nodes){if(ResolvedRoles()){if(!It.IsResolved())return Bad(TEXT("vector4_node_unresolved"));Count+=It==N?1:0;}else Count+=It.Get()==N?1:0;}
            if(!EnumLoaded(N)||N->GetClass()!=UK2Node_CallArrayFunction::StaticClass()||N->GetOuter()!=Graph.Get()||N->GetPathName()!=NodePath||N->GetFlags()!=NodeFlags||Count!=(Attached?1:0)
                ||(GuidFixed&&(N->NodeGuid!=NodeGuid||N->NodePosX!=X||N->NodePosY!=Y)))return Bad(TEXT("current_node_role"));
            if(FunctionAssigned&&(!N->FunctionReference.IsSameReference(Member)||N->FunctionReference.IsDeprecated()))return Bad(TEXT("current_function_binding"));
            if(PinsFixed)
            {
                if(N->Pins.Num()!=Pins.Num())return Bad(TEXT("current_pin_count"));
                for(int32 I=0;I<Pins.Num();++I)
                {
                    const FPin& Old=Pins[I];const UEdGraphPin* P=N->Pins[I];
                    if(ResolvedRoles()&&P&&!P->DefaultObject.IsResolved())return Bad(TEXT("vector4_pin_default_unresolved"));
                    if(P!=Old.Address||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=Old.Id||P->PinName!=Old.Name||P->Direction!=Old.Direction
                        ||P->PinType!=Old.Type||P->PinType.bIsUObjectWrapper!=Old.Type.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=Old.Type.bSerializeAsSinglePrecisionFloat
                        ||P->PinType.PinValueType.bTerminalIsUObjectWrapper!=Old.Type.PinValueType.bTerminalIsUObjectWrapper||FEnumArrayScope::PinFlags(P)!=Old.Flags
                        ||P->DefaultObject!=Old.Object||!P->DefaultValue.Equals(Old.Default,ESearchCase::CaseSensitive)||!P->AutogeneratedDefaultValue.Equals(Old.Auto,ESearchCase::CaseSensitive)
                        ||!P->DefaultTextValue.EqualTo(Old.Text)||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty())return Bad(TEXT("current_physical_pin_inventory"));
                }
            }
        }
        return true;
    }
    // Copied held roles only; no CDO/private/runtime values are sampled.
    TSharedRef<FJsonObject> ParityState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("class_path"),Element->GetPathName());J->SetStringField(TEXT("existing_cdo_path"),ElementDefault->GetPathName());J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetBoolField(TEXT("cdo_values_sampled"),false);J->SetBoolField(TEXT("cdo_creation_requested"),false);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& H:Holds)if(H.ParityNative){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),H.Path);V->SetStringField(TEXT("object_flags_string"),LexToString(uint32(H.Flags)));V->SetBoolField(TEXT("class_layout_observed"),H.IsClass);V->SetStringField(TEXT("class_flags_string"),LexToString(H.ClassFlags));V->SetNumberField(TEXT("class_size"),H.Size);V->SetStringField(TEXT("package_path"),H.Package->GetName());V->SetStringField(TEXT("package_flags_string"),LexToString(H.PackageFlags));V->SetBoolField(TEXT("package_dirty"),H.PackageDirty);Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("held_native_roles"),Rows);return J;
    }
    bool Prepare(const FString& Path,FResult& R)
    {
        Reply=&R;
        if(!EnumContext()||GIsImportingT3D||IsPlaySessionActive()||!EnumLoaded(GEditor))return Bad(TEXT("initial_editor_state"));
        Editor.Reset(GEditor);World.Reset(GEditor->GetEditorWorldContext().World());Undo=GUndo;
        if(!Context())return Bad(TEXT("initial_context"));
        UClass* C=FindObject<UClass>(nullptr,*Path);
        if(!EnumLoaded(C)||(C!=UActorComponent::StaticClass()&&C!=UChildActorComponent::StaticClass())||!C->GetPathName().Equals(Path,ESearchCase::CaseSensitive))return Bad(TEXT("exact_native_element_class"));
        Element.Reset(C);ElementDefault.Reset(C->GetDefaultObject(false));
        for(UClass* Current=C;Current;Current=Current->GetSuperClass())
        {
            if(!EnumLoaded(Current)||!Current->HasAnyClassFlags(CLASS_Native)||Current->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)
                ||Current->ClassGeneratedBy||Current->IsEditorOnly()||Current->GetPackage()->IsDirty()
                ||(Current->GetPackage()->GetName()!=TEXT("/Script/Engine")&&Current->GetPackage()->GetName()!=TEXT("/Script/CoreUObject")))return Bad(TEXT("native_element_hierarchy"));
            if(!Hold(Current,Current->GetPackage()->GetName()==TEXT("/Script/Engine")))return false;
        }
        if(!EnumLoaded(ElementDefault.Get())||ElementDefault->GetClass()!=C||ElementDefault->GetPackage()!=C->GetPackage()
            ||!ElementDefault->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!Hold(ElementDefault.Get(),true))return Bad(TEXT("existing_native_element_cdo"));
        R.Data->SetStringField(TEXT("object_array_native_class"),Path);R.Data->SetStringField(TEXT("object_array_existing_cdo"),ElementDefault->GetPathName());
        R.Data->SetBoolField(TEXT("object_array_source_read_only"),true);R.Data->SetBoolField(TEXT("object_array_cdo_created"),false);return Check();
    }
    bool PrepareSkeletalLength(FResult& R)
    {
        Reply=&R;SkeletalLengthMode=true;if(!EnumContext()||GIsImportingT3D||IsPlaySessionActive()||!EnumLoaded(GEditor))return Bad(TEXT("skeletal_initial_editor_state"));
        Editor.Reset(GEditor);World.Reset(GWorld);Undo=GUndo;if(!Context())return Bad(TEXT("skeletal_initial_context"));
        UClass* C=FindObject<UClass>(nullptr,TEXT("/Script/Engine.SkeletalMeshComponent"));if(!EnumLoaded(C)||C!=USkeletalMeshComponent::StaticClass())return Bad(TEXT("skeletal_exact_loaded_class"));Element.Reset(C);
        const TCHAR* Names[]={TEXT("/Script/Engine.SkeletalMeshComponent"),TEXT("/Script/Engine.SkinnedMeshComponent"),TEXT("/Script/Engine.MeshComponent"),TEXT("/Script/Engine.PrimitiveComponent"),TEXT("/Script/Engine.SceneComponent"),TEXT("/Script/Engine.ActorComponent"),TEXT("/Script/CoreUObject.Object")};
        int32 Index=0;for(auto It=C->GetSuperStructIterator();It;++It,++Index)
        {
            UClass* K=const_cast<UClass*>(Cast<UClass>(*It));if(Index>=UE_ARRAY_COUNT(Names)||!EnumLoaded(K)||!K->GetPathName().Equals(Names[Index],ESearchCase::CaseSensitive)||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy!=nullptr
                ||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||K->IsEditorOnly()||!EnumLoaded(K->GetPackage())
                ||K->GetPackage()->GetName()!=(Index==6?TEXT("/Script/CoreUObject"):TEXT("/Script/Engine"))||K->GetPackage()->GetPackageFlags()!=16U||K->GetPackage()->IsDirty())return Bad(TEXT("skeletal_exact_hierarchy"));
            SkeletalHierarchy.Emplace(K);if(!Hold(K,Index!=6)||!Hold(K->GetPackage(),Index!=6))return false;
        }
        if(Index!=UE_ARRAY_COUNT(Names))return Bad(TEXT("skeletal_hierarchy_count"));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto H=C->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        UObject* D=FindObject<UObject>(nullptr,TEXT("/Script/Engine.Default__SkeletalMeshComponent"));if(!H.IsResolved()||H!=D||!EnumLoaded(D)||D->GetClass()!=C||D->GetOuter()!=C->GetPackage()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Bad(TEXT("skeletal_existing_cdo"));ElementDefault.Reset(D);
        if(!Hold(D,true)||!Check())return false;R.Data->SetStringField(TEXT("object_array_native_class"),Names[0]);R.Data->SetStringField(TEXT("object_array_existing_cdo"),D->GetPathName());R.Data->SetBoolField(TEXT("object_array_source_read_only"),true);R.Data->SetBoolField(TEXT("object_array_cdo_created"),false);R.Data->SetObjectField(TEXT("native_skeletal_length_source_before"),SkeletalState());return true;
    }
    // LP: reuse the unchanged LN native class/hierarchy/CDO observer, not a runtime Array_Length invocation.
    bool PrepareSkeletalDedup(FResult& R)
    {
        if(!PrepareSkeletalLength(R))return false;SkeletalDedupMode=true;
        const auto Before=R.Data->GetObjectField(TEXT("native_skeletal_length_source_before"));
        R.Data->SetObjectField(TEXT("native_skeletal_dedup_source_before"),Before);R.Data->RemoveField(TEXT("native_skeletal_length_source_before"));
        R.Data->SetBoolField(TEXT("native_skeletal_array_dedup"),true);R.Data->SetBoolField(TEXT("skeletal_array_literal_defaults_supported"),false);return true;
    }
    bool PrepareVector4(FResult& R)
    {
        Reply=&R;Vector4Mode=true;if(!EnumContext()||GIsImportingT3D||IsPlaySessionActive()||!EnumLoaded(GEditor))return Bad(TEXT("vector4_initial_editor_state"));
        Editor.Reset(GEditor);World.Reset(GWorld);Undo=GUndo;if(!Context())return Bad(TEXT("vector4_initial_context"));
        UScriptStruct* V=FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.Vector4"));
        if(!EnumLoaded(V)||V!=TBaseStructure<FVector4>::Get()||V->GetStructureSize()!=sizeof(FVector4)||V->IsEditorOnly())return Bad(TEXT("vector4_exact_loaded_type"));
        Vector4.Reset(V);Vector4Package.Reset(V->GetPackage());Vector4Flags=uint32(V->StructFlags);Vector4Align=V->GetMinAlignment();
        const TCHAR* Names[]={TEXT("X"),TEXT("Y"),TEXT("Z"),TEXT("W")};const int32 Offsets[]={STRUCT_OFFSET(FVector4,X),STRUCT_OFFSET(FVector4,Y),STRUCT_OFFSET(FVector4,Z),STRUCT_OFFSET(FVector4,W)};
        for(int32 I=0;I<4;++I){const FProperty* P=V->FindPropertyByName(Names[I]);if(!P||P->GetClass()!=FDoubleProperty::StaticClass()||P->GetOwnerStruct()!=V||P->ArrayDim!=1||P->GetOffset_ForInternal()!=Offsets[I]||P->GetSize()!=sizeof(double))return Bad(TEXT("vector4_native_layout"));Vector4Fields.Add({P,P->GetFName(),P->GetPropertyFlags(),Offsets[I],P->GetSize()});}
        if(!Hold(V)||!Hold(Vector4Package.Get())||!Check())return false;R.Data->SetObjectField(TEXT("native_vector4_source_before"),Vector4State());R.Data->SetBoolField(TEXT("vector4_source_read_only"),true);return true;
    }
    bool BindVector4Target(UBlueprint* B,UEdGraph* G,const UEdGraphSchema_K2* S,int32 InX,int32 InY)
    {
        if(!IsVector4()||!Check()||!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(S))return Bad(TEXT("vector4_bind_target"));
        Blueprint.Reset(B);Graph.Reset(G);Schema.Reset(S);
        const auto CurrentClass=[](const TObjectPtr<UClass>& H)->UClass*{if(!H.IsResolved()||H==nullptr)return nullptr;UClass* C=FindObject<UClass>(nullptr,*H.GetPathName());return H==C?C:nullptr;};
        Parent=CurrentClass(B->ParentClass.GetGCPtr());Generated=CurrentClass(B->GeneratedClass.GetGCPtr());Skeleton=CurrentClass(B->SkeletonGeneratedClass.GetGCPtr());GraphGuid=G->GraphGuid;X=InX;Y=InY;
        GraphSchemaHandle=G->Schema.GetGCPtr();if(!GraphSchemaHandle.IsResolved()||GraphSchemaHandle!=S->GetClass()||G->Nodes.Num()>2048)return Bad(TEXT("vector4_bind_schema"));
        for(const auto& H:G->Nodes){if(!H.IsResolved())return Bad(TEXT("vector4_prior_unresolved_node"));UEdGraphNode* N=FindObject<UEdGraphNode>(nullptr,*H.GetPathName());
            if(!EnumLoaded(N)||H!=N||N->GetOuter()!=G||N->Pins.Num()>512)return Bad(TEXT("vector4_prior_node_unavailable"));FOldNode O;O.Hold.Reset(N);O.Path=N->GetPathName();O.Guid=N->NodeGuid;O.Pins=N->Pins;
            for(UEdGraphPin* P:O.Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid())return Bad(TEXT("vector4_prior_pin_unavailable"));O.Ids.Add(P->PinId);}Vector4OldNodes.Add(MoveTemp(O));}
        if(!Hold(const_cast<UEdGraphSchema_K2*>(S)))return false;
        for(UObject* O:{static_cast<UObject*>(B),static_cast<UObject*>(G),static_cast<UObject*>(Parent),static_cast<UObject*>(Generated),static_cast<UObject*>(Skeleton)})if(!Hold(O))return false;
        return Check();
    }
    // Newly admitted object pairs capture the target classes before any native
    // reflection/metadata query. Actual Library/Function/CDO are adopted later.
    bool BindParityTarget(UBlueprint* B,UEdGraph* G,const UEdGraphSchema_K2* S,int32 InX,int32 InY)
    {
        if((!OperationParityMode&&!UdsLengthMode)||!Check()||!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(S))return Bad(TEXT("parity_initial_target"));
        if(UdsLengthMode&&UdsPackage.Get()==B->GetPackage())return Error(*Reply,TEXT("array_scalar_uds_same_target_pending"),TEXT("Separate source and target packages are required in this read-only UDS slice."));
        const auto P=B->ParentClass.GetGCPtr(),GC=B->GeneratedClass.GetGCPtr(),SC=B->SkeletonGeneratedClass.GetGCPtr();const auto SH=G->Schema.GetGCPtr();
        if(!P.IsResolved()||!GC.IsResolved()||!SC.IsResolved()||!SH.IsResolved()||SH!=S->GetClass())return Bad(TEXT("parity_initial_target_handles"));
        Blueprint.Reset(B);Graph.Reset(G);Schema.Reset(S);Parent=P;Generated=GC;Skeleton=SC;GraphGuid=G->GraphGuid;X=InX;Y=InY;GraphSchemaHandle=SH;
        for(UObject* O:{static_cast<UObject*>(B),static_cast<UObject*>(G),static_cast<UObject*>(Parent),static_cast<UObject*>(Generated),static_cast<UObject*>(Skeleton),static_cast<UObject*>(const_cast<UEdGraphSchema_K2*>(S))})if(!Hold(O))return false;
        return Check();
    }
    bool Bind(UBlueprint* B,UEdGraph* G,const UEdGraphSchema_K2* S,UClass* Library,UFunction* F,UObject* Default,int32 InX,int32 InY)
    {
        if(IsVector4())
        {
            if(!Check()||B!=Blueprint.Get()||G!=Graph.Get()||S!=Schema.Get()||InX!=X||InY!=Y)return Bad(TEXT("vector4_bound_target_changed"));
            FunctionOwner.Reset(Library);Function.Reset(F);FunctionDefault.Reset(Default);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
            LibraryDefaultHandle=Library->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!LibraryDefaultHandle.IsResolved()||LibraryDefaultHandle!=Default)return Bad(TEXT("vector4_bind_library_cdo"));
            for(UObject* O:{static_cast<UObject*>(Library),static_cast<UObject*>(F),Default})if(!Hold(O,true))return false;
            return Check();
        }
        if(!Check()||!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(S))return Bad(TEXT("bind_target"));
        if(UdsLengthMode&&UdsPackage.Get()==B->GetPackage())return Error(*Reply,TEXT("array_scalar_uds_same_target_pending"),TEXT("UDS source and target Blueprint must be separate packages in this read-only source slice."));
        if((SkeletalLengthMode||NativeLengthMode||NativeStructLengthMode||UdsLengthMode)&&(!B->ParentClass.GetGCPtr().IsResolved()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!G->Schema.GetGCPtr().IsResolved()))return Bad(TEXT("skeletal_bind_unresolved_role"));
        Blueprint.Reset(B);Graph.Reset(G);Schema.Reset(S);
        if(SkeletalLengthMode||NativeLengthMode||NativeStructLengthMode||UdsLengthMode){const auto ResolveClass=[](const TObjectPtr<UClass>& H)->UClass*{UClass* K=FindObject<UClass>(nullptr,*H.GetPathName());return H.IsResolved()&&H==K?K:nullptr;};Parent=ResolveClass(B->ParentClass.GetGCPtr());Generated=ResolveClass(B->GeneratedClass.GetGCPtr());Skeleton=ResolveClass(B->SkeletonGeneratedClass.GetGCPtr());}
        else{Parent=B->ParentClass.GetGCPtr();Generated=B->GeneratedClass.GetGCPtr();Skeleton=B->SkeletonGeneratedClass.GetGCPtr();}GraphGuid=G->GraphGuid;X=InX;Y=InY;
        FunctionOwner.Reset(Library);Function.Reset(F);FunctionDefault.Reset(Default);
        if(SkeletalLengthMode||NativeLengthMode||NativeStructLengthMode||UdsLengthMode)
        {
            GraphSchemaHandle=G->Schema.GetGCPtr();
PRAGMA_DISABLE_DEPRECATION_WARNINGS
            LibraryDefaultHandle=Library->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!LibraryDefaultHandle.IsResolved()||LibraryDefaultHandle!=Default)return Bad(TEXT("skeletal_library_cdo"));
        }
        if(!Hold(const_cast<UEdGraphSchema_K2*>(S)))return false;
        for(UObject* O:{static_cast<UObject*>(B),static_cast<UObject*>(G),static_cast<UObject*>(Parent),static_cast<UObject*>(Generated),static_cast<UObject*>(Skeleton),static_cast<UObject*>(Library),static_cast<UObject*>(F),Default}){if(!EnumLoaded(O))return Bad(TEXT("bind_required_role"));if(!Hold(O,O->GetPackage()->GetName()==TEXT("/Script/Engine")))return false;}
        return Check();
    }
    bool Adopt(UK2Node_CallArrayFunction* N)
    {
        Reply->Data->SetBoolField(TEXT("object_array_node_allocation_returned_nonnull"),N!=nullptr);
        if(ResolvedRoles()&&(!EnumContext()||GIsImportingT3D))return Bad(TEXT("vector4_allocation_context_unobservable"));
        // Preserve a safe actual returned prefix before full source/target validation.
        if(EnumLoaded(N)&&EnumLoaded(Graph.Get())&&N->GetClass()==UK2Node_CallArrayFunction::StaticClass()&&N->GetOuter()==Graph.Get())
        {Node.Reset(N);NodePath=N->GetPathName();NodeFlags=N->GetFlags();Reply->Data->SetStringField(TEXT("object_array_allocated_node_path"),NodePath);Reply->bChanged=true;}
        if(!Node.IsValid()||!Check())return Bad(TEXT("allocation_return_role"));return true;
    }
    bool FreezePins()
    {
        if(!Check()||!Node.IsValid()||Node->Pins.Num()>512)return Bad(TEXT("freeze_current_pins"));Pins.Reset();TSet<FGuid> Ids;
        for(UEdGraphPin* P:Node->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node.Get()||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty())return Bad(TEXT("freeze_physical_pin_role"));
            Ids.Add(P->PinId);FPin Old;Old.Address=P;Old.Id=P->PinId;Old.Name=P->PinName;Old.Direction=P->Direction;Old.Type=P->PinType;Old.Flags=FEnumArrayScope::PinFlags(P);
            if(ResolvedRoles()&&!P->DefaultObject.IsResolved())return Bad(TEXT("vector4_pin_default_unresolved"));
            if(P->DefaultObject!=(P->PinName==UEdGraphSchema_K2::PN_Self?FunctionDefault.Get():nullptr))return Bad(TEXT("freeze_default_object_role"));
            Old.Object=P->PinName==UEdGraphSchema_K2::PN_Self?FunctionDefault.Get():nullptr;Old.Default=P->DefaultValue;Old.Auto=P->AutogeneratedDefaultValue;Old.Text=P->DefaultTextValue;Pins.Add(MoveTemp(Old));
        }
        PinsFixed=true;return Check();
    }
    // MX: reflected parameter roles include Get.Item output and const-reference item inputs.
    // CallFunction initializes each wildcard pin through K2; wildcard and Vector4 auto-defaults are empty.
    bool ExpectVector4Specialization(UEdGraphPin* P,const FEdGraphPinType& Type,const FParameter& Parameter)
    {
        if(!IsVector4()||!Check())return false;
        for(auto& Old:Pins)if(Old.Address==P)
        {
            const bool IsInput=(Parameter.Flags&CPF_ReturnParm)==0&&((Parameter.Flags&CPF_OutParm)==0||(Parameter.Flags&CPF_ReferenceParm)!=0);
            const EEdGraphPinDirection Direction=IsInput?EGPD_Input:EGPD_Output;
            FEdGraphPinType SourceType;SourceType.PinCategory=UEdGraphSchema_K2::PC_Wildcard;
            SourceType.ContainerType=FString(Parameter.Class)==TEXT("ArrayProperty")?EPinContainerType::Array:EPinContainerType::None;
            SourceType.bIsReference=(Parameter.Flags&CPF_ReferenceParm)!=0;SourceType.bIsConst=(Parameter.Flags&CPF_ConstParm)!=0;
            FEdGraphPinType Expected=SourceType;Expected.PinCategory=UEdGraphSchema_K2::PC_Struct;Expected.PinSubCategoryObject=Vector4.Get();
            const bool SourceMatches=FString(Parameter.Category)==TEXT("wildcard")&&Old.Name==FName(Parameter.Name)&&Old.Direction==Direction
                &&Old.Type==SourceType&&Old.Type.bIsUObjectWrapper==SourceType.bIsUObjectWrapper
                &&Old.Type.bSerializeAsSinglePrecisionFloat==SourceType.bSerializeAsSinglePrecisionFloat
                &&Old.Type.PinValueType.bTerminalIsUObjectWrapper==SourceType.PinValueType.bTerminalIsUObjectWrapper
                &&Old.Auto.IsEmpty()&&Old.Default.IsEmpty()&&!Old.Object&&Old.Text.IsEmpty();
            const bool TypeMatches=SourceMatches&&Type==Expected&&Type.bIsUObjectWrapper==Expected.bIsUObjectWrapper&&Type.bSerializeAsSinglePrecisionFloat==Expected.bSerializeAsSinglePrecisionFloat
                &&Type.PinValueType.bTerminalIsUObjectWrapper==Expected.PinValueType.bTerminalIsUObjectWrapper;
            if(!SourceMatches||!TypeMatches)
            {
                // Check already proved this frozen FPin current. JSON-only first mismatch:
                // do not follow UObject pointers, export types or dispatch another SDK after failure.
                if(!Reply->Data->HasField(TEXT("vector4_specialization_first_mismatch")))
                {
                    auto Failure=MakeShared<FJsonObject>();Failure->SetStringField(TEXT("pin_name"),Old.Name.ToString());
                    Failure->SetStringField(TEXT("expected_category"),UEdGraphSchema_K2::PC_Wildcard.ToString());Failure->SetStringField(TEXT("actual_category"),Old.Type.PinCategory.ToString());
                    Failure->SetStringField(TEXT("expected_default"),TEXT(""));Failure->SetStringField(TEXT("actual_default"),Old.Default);
                    Failure->SetStringField(TEXT("expected_auto"),TEXT(""));Failure->SetStringField(TEXT("actual_auto"),Old.Auto);
                    Failure->SetNumberField(TEXT("expected_direction"),int32(Direction));Failure->SetNumberField(TEXT("actual_direction"),int32(Old.Direction));
                    Failure->SetNumberField(TEXT("expected_container"),int32(SourceType.ContainerType));Failure->SetNumberField(TEXT("actual_container"),int32(Old.Type.ContainerType));
                    Failure->SetBoolField(TEXT("expected_reference"),SourceType.bIsReference);Failure->SetBoolField(TEXT("actual_reference"),Old.Type.bIsReference);
                    Failure->SetBoolField(TEXT("expected_const"),SourceType.bIsConst);Failure->SetBoolField(TEXT("actual_const"),Old.Type.bIsConst);
                    Failure->SetBoolField(TEXT("actual_default_object_null"),Old.Object==nullptr);Failure->SetBoolField(TEXT("actual_default_text_empty"),Old.Text.IsEmpty());
                    Failure->SetBoolField(TEXT("frozen_source_matches"),SourceMatches);Failure->SetBoolField(TEXT("planned_type_comparison_attempted"),SourceMatches);Failure->SetBoolField(TEXT("planned_type_matches"),TypeMatches);
                    Failure->SetStringField(TEXT("observation_scope"),TEXT("Frozen plain FPin fields after successful current Check; no post-failure UObject/type export or follow-up SDK"));
                    Reply->Data->SetObjectField(TEXT("vector4_specialization_first_mismatch"),Failure);
                }
                return Bad(TEXT("vector4_specialization_default_source"));
            }
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("pin_name"),Old.Name.ToString());J->SetStringField(TEXT("default_before"),Old.Default);J->SetStringField(TEXT("auto_before"),Old.Auto);
            J->SetNumberField(TEXT("direction"),int32(Direction));J->SetNumberField(TEXT("container"),int32(SourceType.ContainerType));
            J->SetBoolField(TEXT("is_reference"),SourceType.bIsReference);J->SetBoolField(TEXT("is_const"),SourceType.bIsConst);
            J->SetStringField(TEXT("expected_default_after"),TEXT(""));J->SetStringField(TEXT("expected_auto_after"),TEXT(""));
            Reply->Data->SetObjectField(FString(TEXT("vector4_specialization_"))+Old.Name.ToString(),J);
            // Expectations only. The public schema call performs the real writes.
            Old.Type=Type;Old.Default.Empty();Old.Auto.Empty();return true;
        }
        return Bad(TEXT("vector4_specialization_pin_not_frozen"));
    }
    bool ExpectStructOperationSpecialization(UEdGraphPin* Physical,const FEdGraphPinType& Planned,const FParameter& Parameter)
    {
        if(!NativeStructOperationMode||!Check())return false;
        for(auto& Frozen:Pins)if(Frozen.Address==Physical)
        {
            const bool In=(Parameter.Flags&CPF_ReturnParm)==0&&((Parameter.Flags&CPF_OutParm)==0||(Parameter.Flags&CPF_ReferenceParm)!=0);
            FEdGraphPinType SourceType;SourceType.PinCategory=UEdGraphSchema_K2::PC_Wildcard;SourceType.ContainerType=FString(Parameter.Class)==TEXT("ArrayProperty")?EPinContainerType::Array:EPinContainerType::None;SourceType.bIsReference=(Parameter.Flags&CPF_ReferenceParm)!=0;SourceType.bIsConst=(Parameter.Flags&CPF_ConstParm)!=0;
            FEdGraphPinType Expected=SourceType;Expected.PinCategory=UEdGraphSchema_K2::PC_Struct;Expected.PinSubCategoryObject=StructLengthRoles[0].Type.Get();
            if(FString(Parameter.Category)!=TEXT("wildcard")||Frozen.Name!=FName(Parameter.Name)||Frozen.Direction!=(In?EGPD_Input:EGPD_Output)||Frozen.Type!=SourceType||Frozen.Type.bIsUObjectWrapper!=SourceType.bIsUObjectWrapper||Frozen.Type.bSerializeAsSinglePrecisionFloat!=SourceType.bSerializeAsSinglePrecisionFloat||Frozen.Type.PinValueType.bTerminalIsUObjectWrapper!=SourceType.PinValueType.bTerminalIsUObjectWrapper||!Frozen.Auto.IsEmpty()||!Frozen.Default.IsEmpty()||Frozen.Object||!Frozen.Text.IsEmpty()||Planned!=Expected||Planned.bIsUObjectWrapper!=Expected.bIsUObjectWrapper||Planned.bSerializeAsSinglePrecisionFloat!=Expected.bSerializeAsSinglePrecisionFloat||Planned.PinValueType.bTerminalIsUObjectWrapper!=Expected.PinValueType.bTerminalIsUObjectWrapper)return Bad(TEXT("native_struct_operation_specialization_source"));
            // SchemaK2 5215-5256: only a non-container Rotator receives this stock auto/raw.
            const FString PlannedDefault=SourceType.IsContainer()||StructLengthRoles[0].Path!=TEXT("/Script/CoreUObject.Rotator")?FString():FString(TEXT("0, 0, 0"));
            auto Observation=MakeShared<FJsonObject>();Observation->SetStringField(TEXT("pin_name"),Frozen.Name.ToString());Observation->SetStringField(TEXT("pin_id"),Frozen.Id.ToString(EGuidFormats::Digits));Observation->SetStringField(TEXT("raw_before"),Frozen.Default);Observation->SetStringField(TEXT("auto_before"),Frozen.Auto);Observation->SetStringField(TEXT("planned_raw_after"),PlannedDefault);Observation->SetStringField(TEXT("planned_auto_after"),PlannedDefault);Reply->Data->SetObjectField(FString(TEXT("value_struct_array_specialization_"))+Frozen.Name.ToString(),Observation);
            Frozen.Type=Planned;Frozen.Default=PlannedDefault;Frozen.Auto=PlannedDefault;return true;
        }
        return Bad(TEXT("native_struct_operation_specialization_pin"));
    }
    bool ExpectType(UEdGraphPin* P,const FEdGraphPinType& Type)
    {if(!Check())return false;for(auto& Old:Pins)if(Old.Address==P){Old.Type=Type;return true;}return Bad(TEXT("planned_pin_type"));}
    bool ExpectDescriptorDefault(UEdGraphPin* Physical,const FString& Raw,UObject* Object)
    {if(!NativeLengthMode||!Check())return false;for(auto& Frozen:Pins)if(Frozen.Address==Physical){if(!Frozen.Text.IsEmpty()||Frozen.Object)return Bad(TEXT("descriptor_operation_default_source"));Frozen.Default=Raw;Frozen.Object=Object;return true;}return Bad(TEXT("descriptor_operation_default_pin"));}
    bool ExpectDefault(UEdGraphPin* P,const FString& Value)
    {if(!Check())return false;for(auto& Old:Pins)if(Old.Address==P){Old.Default=Value;return true;}return Bad(TEXT("planned_pin_default"));}
};
struct FContext
{
    bool CreateGraphIfMissing=false;TFunction<bool(UEdGraphNode*)> GraphCreationCurrent;
    // WT: concrete default-EventGraph bootstrap; no new selector/graph kind.
    bool DefaultEventPending=false,DefaultEventAttached=false,DefaultEventPinRefresh=false;
    TStrongObjectPtr<UObject> DefaultEventEditor;TStrongObjectPtr<UWorld> DefaultEventWorld;
    TStrongObjectPtr<UPackage> DefaultEventPackage;TStrongObjectPtr<UClass> DefaultEventParent;
    TStrongObjectPtr<UObject> DefaultEventSchema;
    TSharedPtr<Function::FPresetLoaderNameArrayScope> NameScope;
    TSharedPtr<FEnumArrayScope> EnumScope;
    TSharedPtr<FVectorArrayScope> VectorScope;
    TSharedPtr<FObjectArrayScope> ObjectScope;
    bool bVector4=false;bool ObjectParity=false;bool DescriptorLength=false,DescriptorOperation=false;bool StructLength=false,StructOperation=false,UdsLength=false,UdsOperation=false;FString StructLengthKind;
    UBlueprint* Blueprint = nullptr; UEdGraph* Graph = nullptr; const UEdGraphSchema_K2* Schema = nullptr;
    UClass* Library = nullptr; UFunction* Function = nullptr; UObject* LibraryCDO = nullptr;
    const FOperation* Operation = nullptr; FName ElementCategory; FName ElementSubCategory; UObject* ElementObject = nullptr; int32 X = 0, Y = 0;
    TArray<FDefault> Defaults;
    struct FDescriptorDefaultSource{FResult Result;TSharedPtr<FObjectArrayScope> Scope;};
    TMap<FName,TSharedPtr<FDescriptorDefaultSource>> DescriptorDefaultSources;
    TMap<FName,UObject*> DescriptorDefaultObjects,DescriptorAppliedObjects;
    struct FExtendedPin { UEdGraphPin* Address;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type; };
    struct FExtendedNode { TStrongObjectPtr<UEdGraphNode> Hold;UEdGraph* Graph;int32 Index;FGuid Guid;TArray<FExtendedPin> Pins; };
    struct FExtendedGraph {TStrongObjectPtr<UEdGraph> Hold;int32 NodeCount;
        // Used only by WT's creation observer; ordinary Array graph observation is unchanged.
        UObject* BootstrapOuter=nullptr;UPackage* BootstrapPackage=nullptr;UClass* BootstrapClass=nullptr;
        TObjectPtr<UClass> BootstrapSchema;FString BootstrapPath;FGuid BootstrapGuid;
    };TArray<FExtendedGraph> ExtendedGraphs;
    struct FExtendedField {FProperty* Address;const FFieldClass* Class;FFieldVariant Owner;FName Name;uint64 Flags;FProperty* Inner;const FFieldClass* InnerClass;FFieldVariant InnerOwner;uint64 InnerFlags;};
    TArray<FExtendedField> ExtendedFields;FResult* ExtendedResult=nullptr;
    TArray<FExtendedNode> ExtendedOld;TArray<FExtendedPin> ExtendedPins;
    TStrongObjectPtr<UBlueprint> ExtendedBP;TStrongObjectPtr<UEdGraph> ExtendedGraph;
    TStrongObjectPtr<UClass> ExtendedLibrary;TStrongObjectPtr<UFunction> ExtendedFunction;TStrongObjectPtr<UObject> ExtendedCDO;
    TStrongObjectPtr<UK2Node_CallArrayFunction> ExtendedCreated;bool ExtendedBound=false,ExtendedPinsFixed=false,ExtendedAttached=false;
    TMap<FName,FString> ExtendedSchemaDefaults;
    TMap<FName,FVector4> Vector4LiteralValues; // MZ: plain preflight values only; no UObject/CDO scratch.
    TSharedRef<FJsonObject> Before = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> NativeBefore = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> VectorBefore = MakeShared<FJsonObject>();
};

bool Error(FResult& R, const TCHAR* Code, const FString& Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }

FString Normalize(FString Value)
{ return Value.TrimStartAndEnd().ToLower().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")); }

const FOperation* OperationFor(const FString& Requested)
{
    FString Name = Normalize(Requested);
    if (Name == TEXT("num") || Name == TEXT("count") || Name == TEXT("size")) { Name = TEXT("length"); }
    else if (Name == TEXT("setarrayelem")) { Name = TEXT("set"); }
    else if (Name == TEXT("unique")) { Name = TEXT("addunique"); }
    else if (Name == TEXT("insertitem")) { Name = TEXT("insert"); }
    else if (Name == TEXT("containsitem")) { Name = TEXT("contains"); }
    else if (Name == TEXT("removeitem")) { Name = TEXT("remove"); }
    else if (Name == TEXT("empty") || Name == TEXT("reset")) { Name = TEXT("clear"); }
    for (const FOperation& Operation : Operations)
    { if (Normalize(Operation.Name) == Name) { return &Operation; } }
    return nullptr;
}

bool Input(const FParameter& Parameter)
{ return !(Parameter.Flags & CPF_ReturnParm) && (!(Parameter.Flags & CPF_OutParm) || (Parameter.Flags & CPF_ReferenceParm)); }

FEdGraphPinType ParameterType(const FParameter& Parameter, const FName& ElementCategory = NAME_None, UObject* ElementObject = nullptr, const FName& ElementSubCategory = NAME_None)
{
    FEdGraphPinType Type; Type.PinCategory = FName(Parameter.Category);
    Type.ContainerType = FString(Parameter.Class) == TEXT("ArrayProperty") ? EPinContainerType::Array : EPinContainerType::None;
    Type.bIsReference = (Parameter.Flags & CPF_ReferenceParm) != 0; Type.bIsConst = (Parameter.Flags & CPF_ConstParm) != 0;
    if (!ElementCategory.IsNone() && Type.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
    { Type.PinCategory = ElementCategory; Type.PinSubCategory = ElementSubCategory; Type.PinSubCategoryObject = ElementObject; }
    return Type;
}

TSharedRef<FJsonObject> VectorState(const UScriptStruct* Vector)
{
    auto Row = MakeShared<FJsonObject>();
    const bool Readable = FullyLoaded(Vector) && IsValid(Vector->GetOutermost());
    const bool Exact = Readable && Vector == TBaseStructure<FVector>::Get()
        && Vector->GetPathName() == TEXT("/Script/CoreUObject.Vector") && Vector->GetStructureSize() == sizeof(FVector)
        && !Vector->IsEditorOnly() && Vector->GetOutermost()->GetName() == TEXT("/Script/CoreUObject")
        && !Vector->GetOutermost()->IsDirty()
        && !Vector->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly);
    Row->SetBoolField(TEXT("observations_complete"), Readable);
    Row->SetBoolField(TEXT("native_vector_identity_verified"), Exact);
    Row->SetStringField(TEXT("struct_path"), Vector ? Vector->GetPathName() : FString());
    if (FullyLoaded(Vector))
    {
        Row->SetNumberField(TEXT("struct_size"), Vector->GetStructureSize());
        Row->SetObjectField(TEXT("package"), PackageState(Vector->GetOutermost()));
    }
    return Row;
}

bool SameType(const FEdGraphPinType& A, const FEdGraphPinType& B)
{
    return A == B && A.bIsUObjectWrapper == B.bIsUObjectWrapper
        && A.bSerializeAsSinglePrecisionFloat == B.bSerializeAsSinglePrecisionFloat
        && A.PinValueType.bTerminalIsUObjectWrapper == B.PinValueType.bTerminalIsUObjectWrapper;
}

bool ExactInt32(const TSharedPtr<FJsonValue>& Value, int32& Out)
{
    if (!Value || Value->Type != EJson::Number) { return false; } const double Number = Value->AsNumber();
    if (!FMath::IsFinite(Number) || Number < MIN_int32 || Number > MAX_int32 || FMath::TruncToDouble(Number) != Number) { return false; }
    Out = static_cast<int32>(Number); return true;
}

// IN: original Vector literal conversion; no object/struct type catalog expansion.
bool VectorLiteral(const TSharedPtr<FJsonValue>& Input,FString& Out,FResult& R)
{
    if(!Input) { return Error(R,TEXT("array_vector_literal_invalid"),TEXT("A Vector value needs a finite array, lowercase axis object, or schema-valid literal string.")); }
    if(Input->Type==EJson::String)
    {
        const FString Literal=Input->AsString();FVector Parsed;bool Safe=!Literal.IsEmpty()&&Literal.Len()<=65536;for(TCHAR Unit:Literal) { Safe&=Unit!=0; }
        if(Safe&&FDefaultValueHelper::ParseVector(Literal,Parsed)&&!Parsed.ContainsNaN()) { Out=Literal;return true; }
        return Error(R,TEXT("array_vector_literal_invalid"),TEXT("Vector literal string is not finite and schema-valid."));
    }
    double Axes[3]={0,0,0};
    if(Input->Type==EJson::Array)
    {
        const auto& Values=Input->AsArray();
        // Original GetPinDefaultStringForType reads the first three values and defaults missing axes to zero.
        for(int32 I=0;I<FMath::Min(Values.Num(),3);++I) { if(!Values[I]||Values[I]->Type!=EJson::Number||!FMath::IsFinite(Values[I]->AsNumber())) { return Error(R,TEXT("array_vector_literal_invalid"),TEXT("Provided Vector axes must be finite JSON numbers; legacy malformed-number coercions remain pending.")); }Axes[I]=Values[I]->AsNumber(); }
    }
    else if(Input->Type==EJson::Object)
    {
        const auto Object=Input->AsObject();const TCHAR* Names[3]={TEXT("x"),TEXT("y"),TEXT("z")};if(!Object) { return false; }
        for(int32 I=0;I<3;++I) { const auto Value=Object->TryGetField(Names[I]);if(!Value) { continue; }if(Value->Type!=EJson::Number||!FMath::IsFinite(Value->AsNumber())) { return Error(R,TEXT("array_vector_literal_invalid"),TEXT("Provided lowercase Vector axes must be finite JSON numbers; missing axes default to zero.")); }Axes[I]=Value->AsNumber(); }
    }
    else { return Error(R,TEXT("array_vector_literal_invalid"),TEXT("Vector values do not coerce null/bool/scalar into a struct literal.")); }
    Out=FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),*FString::SanitizeFloat(Axes[0]),*FString::SanitizeFloat(Axes[1]),*FString::SanitizeFloat(Axes[2]));return true;
}

// MZ: finite full-field text is the original generic ImportText path, not JSON
// Vector4 conversion. Partial/empty forms remain outside this feature slice.
bool Vector4Literal(FObjectArrayScope& Scope,const FString& Literal,FVector4& Parsed,FResult& R)
{
    if(Literal.IsEmpty()||Literal.Len()>65536||Literal[0]!=TEXT('(')||Literal[Literal.Len()-1]!=TEXT(')'))
        return Error(R,TEXT("array_vector4_literal_unsupported"),TEXT("Vector4 supports only bounded complete (X=...,Y=...,Z=...,W=...) strings here; partial/empty/other valid struct forms remain outside this slice."));
    for(TCHAR Unit:Literal)if(Unit==0)return Error(R,TEXT("array_vector4_literal_invalid"),TEXT("A Vector4 literal cannot contain a NUL."));
    TArray<FString> Parts;Literal.Mid(1,Literal.Len()-2).ParseIntoArray(Parts,TEXT(","),false);
    if(Parts.Num()!=4)return Error(R,TEXT("array_vector4_literal_unsupported"),TEXT("All four X/Y/Z/W fields must be explicit in this finite slice; field omission is not claimed invalid in generic UE struct text."));
    const auto Decimal=[](const FString& Value)
    {
        int32 At=0;const int32 Count=Value.Len();if(At<Count&&(Value[At]==TEXT('+')||Value[At]==TEXT('-')))++At;
        bool Digits=false;while(At<Count&&Value[At]>=TEXT('0')&&Value[At]<=TEXT('9')){Digits=true;++At;}
        if(At<Count&&Value[At]==TEXT('.')){++At;while(At<Count&&Value[At]>=TEXT('0')&&Value[At]<=TEXT('9')){Digits=true;++At;}}
        if(!Digits)return false;
        if(At<Count&&(Value[At]==TEXT('e')||Value[At]==TEXT('E'))){++At;if(At<Count&&(Value[At]==TEXT('+')||Value[At]==TEXT('-')))++At;const int32 First=At;while(At<Count&&Value[At]>=TEXT('0')&&Value[At]<=TEXT('9'))++At;if(At==First)return false;}
        return At==Count;
    };
    double Expected[4]={0,0,0,0};uint32 Seen=0;const TCHAR* Axes[]={TEXT("X"),TEXT("Y"),TEXT("Z"),TEXT("W")};
    for(const FString& Part:Parts)
    {
        FString Axis,Number;if(!Part.Split(TEXT("="),&Axis,&Number))return Error(R,TEXT("array_vector4_literal_invalid"),TEXT("Each Vector4 field needs one numeric value."));
        Axis.TrimStartAndEndInline();Number.TrimStartAndEndInline();int32 Index=INDEX_NONE;
        for(int32 I=0;I<4;++I)if(Axis.Equals(Axes[I],ESearchCase::IgnoreCase))Index=I;
        if(Index==INDEX_NONE||(Seen&(1U<<Index)))return Error(R,TEXT("array_vector4_literal_unsupported"),TEXT("This finite slice requires each X/Y/Z/W field exactly once, with no extra fields."));
        if(!Decimal(Number))return Error(R,TEXT("array_vector4_literal_unsupported"),TEXT("Only complete decimal numeric fields are admitted in this slice; other UE text forms are not classified by this parser."));
        if(!FDefaultValueHelper::ParseDouble(Number,Expected[Index])||!FMath::IsFinite(Expected[Index]))
            return Error(R,TEXT("array_vector4_literal_invalid"),TEXT("Vector4 fields must be finite complete decimal real numbers."));
        Seen|=1U<<Index;
    }
    if(Seen!=15U)return Error(R,TEXT("array_vector4_literal_unsupported"),TEXT("All four Vector4 fields must be explicit."));
    if(!Scope.Check())return false;
    FVector4 Scratch(ForceInit);FStringOutputDevice ImportErrors;
    double Attempts=0;R.Data->TryGetNumberField(TEXT("vector4_literal_import_attempted"),Attempts);R.Data->SetNumberField(TEXT("vector4_literal_import_attempted"),Attempts+1);
    const TCHAR* End=Scope.Vector4->ImportText(*Literal,&Scratch,nullptr,PPF_None,&ImportErrors,TEXT("Vector4"),false);
    double Returned=0;R.Data->TryGetNumberField(TEXT("vector4_literal_import_returned"),Returned);R.Data->SetNumberField(TEXT("vector4_literal_import_returned"),Returned+1);
    // The source has four held current FDoubleProperty fields and no reference
    // members. ImportText targets this stack value, with no Blueprint/CDO value
    // supplied. Close Current before using the result or admitting another SDK;
    // this is not a claim that engine queries are globally side-effect free.
    if(!Scope.Check())return false;
    bool Equal=End==*Literal+Literal.Len()&&ImportErrors.IsEmpty();
    for(int32 I=0;I<4;++I)Equal=Equal&&FMath::IsFinite(Scratch[I])&&Scratch[I]==Expected[I];
    if(!Equal)return Error(R,TEXT("array_vector4_literal_invalid"),TEXT("Public Vector4 ImportText did not fully consume the string without diagnostics or preserve all four expected finite values."));
    Parsed=Scratch;return true;
}

// JP: original Shared.h GetPinDefaultStringForType/GetPinDefaultString for
// name/string values only. Literal text is not an identifier; keep empty/space/
// Unicode/control characters admitted by the existing bounded JSON parser.
bool TextLiteral(const TSharedPtr<FJsonValue>& Value,bool Name,FString& Out,FResult& R)
{
    Out.Reset();
    if(!Value){return Error(R,TEXT("array_text_literal_invalid"),TEXT("A parsed bounded JSON value is required."));}
    if(Value->Type==EJson::String){Out=Value->AsString();}
    else if(Value->Type==EJson::Boolean){Out=Value->AsBool()?TEXT("true"):TEXT("false");}
    else if(Value->Type==EJson::Number){Out=FString::SanitizeFloat(Value->AsNumber());}
    else if(Value->Type==EJson::Array||Value->Type==EJson::Object)
    {if(!FJsonSerializer::Serialize(Value,TEXT(""),TJsonWriterFactory<>::Create(&Out))){return Error(R,TEXT("array_text_literal_invalid"),TEXT("Original JSON fallback serialization failed before allocation."));}}
    else if(Value->Type!=EJson::Null){return Error(R,TEXT("array_text_literal_invalid"),TEXT("Unsupported JSON notation for original text conversion."));}
    // SDK FNameProperty imports by FName(Buffer); this bound prevents its fatal
    // NAME_SIZE path. It does not impose SafeName/Identifier on literal values.
    if(Out.Len()>65536||(Name&&Out.Len()>=NAME_SIZE))
    {return Error(R,TEXT("array_text_literal_bound"),TEXT("Converted string exceeds65536 UTF16 units or name reaches the native NAME_SIZE limit."));}
    return true;
}

// NY: original Shared.h608-627 conversion for the additional finite numeric types.
// JSON number is already a double; exact int64 text must be supplied as a string.
bool ExtendedNumberLiteral(const TSharedPtr<FJsonValue>& V,const FEdGraphPinType& Type,FString& Out,FResult& R)
{
    if(V&&V->Type==EJson::Number)
    {
        const double Number=V->AsNumber();
        if(!FMath::IsFinite(Number))return Error(R,TEXT("array_numeric_literal_invalid"),TEXT("Numeric default must be finite; no engine mutation dispatched."));
        if(Type.PinCategory==UEdGraphSchema_K2::PC_Int64)
        {
            if(Number<-9223372036854775808.0||Number>=9223372036854775808.0)return Error(R,TEXT("array_int64_numeric_range"),TEXT("JSON double cannot be converted safely to int64; use an exact int64 string within SDK bounds."));
            Out=FString::Printf(TEXT("%lld"),static_cast<int64>(Number));return true;
        }
    }
    return TextLiteral(V,false,Out,R);
}

bool ExtendedPrimitive(const FContext& C)
{return C.ElementCategory==UEdGraphSchema_K2::PC_Int64||C.ElementCategory==UEdGraphSchema_K2::PC_Real||C.ElementCategory==UEdGraphSchema_K2::PC_Text;}
// OB opts only newly admitted finite native object operations into the repaired
// NY return/current/copy observer. Existing type/default admission is unchanged.
bool TrackedArrayObservation(const FContext& C){return bool(C.GraphCreationCurrent)||ExtendedPrimitive(C)||C.ObjectParity||C.DescriptorLength||C.StructLength||C.UdsLength;}

// NY finite callback fence; complete values/defaults/links remain covered by
// the original Capture/Finish preservation checks. This check invokes no SDK.
bool ExtendedStop(FContext& C,FResult& R,const TCHAR* Property,const FString& Expected,const FString& Actual)
{
    if(!R.Data->HasField(TEXT("extended_primitive_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("extended_primitive_first_mismatch"),J);}
    R.Data->SetBoolField(TEXT("extended_primitive_followup_sdk_stopped"),true);R.bChangeStateKnown=false;if(C.ObjectParity||C.DescriptorOperation){R.Data->SetBoolField(TEXT("object_array_extended_followup_sdk_stopped"),true);R.Data->SetBoolField(TEXT("object_array_expected_actual_observed"),false);R.Data->SetStringField(TEXT("object_array_current_diagnostic_scope"),TEXT("Compound current guard and copied operands; no independent per-property post-failure SDK readback."));}return Error(R,C.ObjectParity?TEXT("array_object_operation_current_changed"):TEXT("array_extended_primitive_current_changed"),TEXT("Current target/source/pin identity changed at an SDK return; partial prefix retained without rollback, retry or followup SDK."));
}
bool ExtendedCurrentPlain(FContext& C,FResult& R,const UK2Node_CallArrayFunction* Node)
{
    if(C.GraphCreationCurrent&&!C.GraphCreationCurrent(const_cast<UK2Node_CallArrayFunction*>(Node)))return false;
    if(!TrackedArrayObservation(C)||!C.ExtendedBound)return true;
    if(R.Data->HasField(TEXT("extended_primitive_followup_sdk_stopped")))return false;
    auto Bad=[&](const TCHAR* K,const FString& E,const FString& A){return ExtendedStop(C,R,K,E,A);};
    if(!EnumContext()||!EnumLoaded(C.ExtendedBP.Get())||C.Blueprint!=C.ExtendedBP.Get()||C.Blueprint->bBeingCompiled||C.Blueprint->bQueuedForCompilation||C.Blueprint->bIsRegeneratingOnLoad)return Bad(TEXT("target_current"),TEXT("held idle Blueprint"),TEXT("different, retired or busy"));
    if(!C.DefaultEventPending&&(!EnumLoaded(C.ExtendedGraph.Get())||C.Graph!=C.ExtendedGraph.Get()||C.Graph->GetOuter()!=C.Blueprint||!C.Graph->Schema.GetGCPtr().IsResolved()||C.Graph->Schema.GetGCPtr()!=C.Schema->GetClass()))return Bad(TEXT("target_current"),TEXT("held direct graph/schema"),TEXT("different or retired"));
    // Bootstrap checks only source roles whose SDK return has already been adopted.
    if(C.ExtendedLibrary.IsValid()&&(!EnumLoaded(C.ExtendedLibrary.Get())||C.Library!=C.ExtendedLibrary.Get()))return Bad(TEXT("source_library_current"),TEXT("held native ArrayLibrary"),TEXT("different or retired"));
    if(C.ExtendedFunction.IsValid()&&(!EnumLoaded(C.ExtendedFunction.Get())||C.Function!=C.ExtendedFunction.Get()||C.Function->GetOuter()!=C.Library))return Bad(TEXT("source_function_current"),TEXT("held selected native function"),TEXT("different or retired"));
    if(C.ExtendedCDO.IsValid()&&(!EnumLoaded(C.ExtendedCDO.Get())||C.LibraryCDO!=C.ExtendedCDO.Get()||C.LibraryCDO->GetClass()!=C.Library))return Bad(TEXT("source_cdo_current"),TEXT("held existing native CDO"),TEXT("different or retired"));
    int32 FieldIndex=0;for(TFieldIterator<FProperty> It(C.ExtendedFunction.Get(),EFieldIterationFlags::None);It;++It){if(!C.ExtendedFields.IsValidIndex(FieldIndex)||*It!=C.ExtendedFields[FieldIndex].Address)return Bad(TEXT("native_field_inventory"),TEXT("captured direct function fields"),TEXT("different field routing"));const auto& F=C.ExtendedFields[FieldIndex++];if(F.Address->GetClass()!=F.Class||F.Address->GetOwnerVariant()!=F.Owner||F.Address->GetFName()!=F.Name||uint64(F.Address->GetPropertyFlags())!=F.Flags)return Bad(TEXT("native_field_identity"),F.Name.ToString(),F.Address->GetFName().ToString());if(F.Class==FArrayProperty::StaticClass()){const auto* A=static_cast<const FArrayProperty*>(F.Address);if(A->Inner!=F.Inner||!F.Inner)return Bad(TEXT("native_array_inner_routing"),FString::Printf(TEXT("%p"),F.Inner),FString::Printf(TEXT("%p"),A->Inner));if(F.Inner->GetClass()!=F.InnerClass||F.Inner->GetOwnerVariant()!=F.InnerOwner||uint64(F.Inner->GetPropertyFlags())!=F.InnerFlags)return Bad(TEXT("native_array_inner_identity"),TEXT("captured actual Inner class/owner/flags"),TEXT("different Inner role"));}}if(FieldIndex!=C.ExtendedFields.Num())return Bad(TEXT("native_field_count"),LexToString(C.ExtendedFields.Num()),LexToString(FieldIndex));
    for(const auto& G:C.ExtendedGraphs){if(!EnumLoaded(G.Hold.Get()))return Bad(TEXT("observed_graph_current"),TEXT("held complete-capture graph"),TEXT("retired"));const int32 ExpectedCount=G.NodeCount+(G.Hold.Get()==C.Graph&&C.ExtendedAttached?1:0);if(G.Hold->Nodes.Num()!=ExpectedCount)return Bad(TEXT("graph_node_count"),LexToString(ExpectedCount),LexToString(G.Hold->Nodes.Num()));}
    auto PinsCurrent=[&](const UEdGraphNode* N,const TArray<FContext::FExtendedPin>& Roles)
    {
        if(N->Pins.Num()!=Roles.Num())return Bad(TEXT("pin_count"),LexToString(Roles.Num()),LexToString(N->Pins.Num()));
        for(int32 I=0;I<Roles.Num();++I){const auto& Role=Roles[I];const UEdGraphPin* P=N->Pins[I];if(P!=Role.Address||!P||P->WasTrashed())return Bad(TEXT("pin_address"),FString::Printf(TEXT("%p"),Role.Address),FString::Printf(TEXT("%p"),P));if(P->GetOwningNodeUnchecked()!=N||P->PinId!=Role.Id||P->PinName!=Role.Name||P->Direction!=Role.Direction||!SameType(P->PinType,Role.Type))return Bad(TEXT("pin_role"),Role.Id.ToString(EGuidFormats::Digits),P->PinId.ToString(EGuidFormats::Digits));}return true;
    };
    for(const auto& Old:C.ExtendedOld){const auto* N=Old.Graph->Nodes[Old.Index].Get();if(N!=Old.Hold.Get()||!EnumLoaded(Old.Hold.Get())||N->NodeGuid!=Old.Guid||N->GetOuter()!=Old.Graph)return Bad(TEXT("prior_node_identity"),FString::Printf(TEXT("%p"),Old.Hold.Get()),FString::Printf(TEXT("%p"),N));if(!C.DefaultEventPinRefresh&&!PinsCurrent(N,Old.Pins))return false;}
    if(C.ExtendedCreated.IsValid()){const auto* N=C.ExtendedCreated.Get();if(Node!=N||!EnumLoaded(N)||N->GetOuter()!=C.Graph||(C.ExtendedAttached&&C.Graph->Nodes.Last()!=N))return Bad(TEXT("created_node_identity"),FString::Printf(TEXT("%p"),N),FString::Printf(TEXT("%p"),Node));if(C.ExtendedPinsFixed&&!PinsCurrent(N,C.ExtendedPins))return false;}
    return true;
}
bool ExtendedCurrent(FContext& C,FResult& R,const UK2Node_CallArrayFunction* Node)
{
    if(C.GraphCreationCurrent&&!C.GraphCreationCurrent(const_cast<UK2Node_CallArrayFunction*>(Node)))return false;
    if(!TrackedArrayObservation(C)||!C.ExtendedBound)return true;
    if((C.ObjectParity||C.DescriptorLength||C.StructLength||C.UdsLength)&&C.ObjectScope.IsValid()&&!C.ObjectScope->Check())return false;
    if(C.DescriptorOperation)for(const auto& Pair:C.DescriptorDefaultSources)if(!Pair.Value->Scope->Check())return ExtendedStop(C,R,TEXT("descriptor_default_source"),TEXT("captured exact native value class/hierarchy/optional CDO"),TEXT("source current guard failed"));
    return ExtendedCurrentPlain(C,R,Node);
}
bool ExtendedObservedCurrent(FContext* C)
{return (!C||!C->GraphCreationCurrent||C->GraphCreationCurrent(C->ExtendedCreated.Get()))&&(!C||!C->ExtendedBound||ExtendedCurrent(*C,*C->ExtendedResult,C->ExtendedCreated.Get()));}
bool ExtendedBind(FContext& C,FResult& R)
{
    if(!TrackedArrayObservation(C))return true;
    if(C.ExtendedBound)return ExtendedCurrent(C,R,nullptr);
    C.ExtendedBP.Reset(C.Blueprint);C.ExtendedGraph.Reset(C.Graph);
    C.ExtendedResult=&R;
    TArray<UEdGraph*> Graphs;C.Blueprint->GetAllGraphs(Graphs);if(Graphs.Num()>256)return ExtendedStop(C,R,TEXT("observed_graph_capture"),TEXT("existing bounded complete-capture graphs"),TEXT("over bound"));
    for(auto* Graph:Graphs){if(!EnumLoaded(Graph))return ExtendedStop(C,R,TEXT("observed_graph_capture"),TEXT("loaded complete-capture graph"),TEXT("retired/null"));FContext::FExtendedGraph G;G.Hold.Reset(Graph);G.NodeCount=Graph->Nodes.Num();C.ExtendedGraphs.Add(MoveTemp(G));for(int32 Index=0;Index<Graph->Nodes.Num();++Index){const auto& Node=Graph->Nodes[Index];if(!EnumLoaded(Node.Get()))return ExtendedStop(C,R,TEXT("prior_node_capture"),TEXT("loaded current node"),TEXT("retired/null"));FContext::FExtendedNode Old;Old.Hold.Reset(Node.Get());Old.Graph=Graph;Old.Index=Index;Old.Guid=Node->NodeGuid;for(auto* Pin:Node->Pins){if(!Pin||Pin->WasTrashed()||Pin->GetOwningNodeUnchecked()!=Node.Get())return ExtendedStop(C,R,TEXT("prior_pin_capture"),TEXT("node-owned current pin"),TEXT("retired/different owner"));Old.Pins.Add({Pin,Pin->PinId,Pin->PinName,Pin->Direction,Pin->PinType});}C.ExtendedOld.Add(MoveTemp(Old));}}
    C.ExtendedBound=true;return ExtendedCurrent(C,R,nullptr);
}
bool ExtendedAdoptNative(FContext& C,FResult& R)
{
    if(!TrackedArrayObservation(C))return true;
    TStrongObjectPtr<UClass> UdsReturnedLibrary(C.UdsLength?C.Library:nullptr);TStrongObjectPtr<UFunction> UdsReturnedFunction(C.UdsLength?C.Function:nullptr);TStrongObjectPtr<UObject> UdsReturnedCDO(C.UdsLength?C.LibraryCDO:nullptr);
    if(!ExtendedObservedCurrent(&C))return false;
    if(C.Library&&!C.ExtendedLibrary.IsValid()){if(!EnumLoaded(C.Library))return ExtendedStop(C,R,TEXT("source_library_return"),TEXT("loaded actual ArrayLibrary return"),TEXT("retired/null"));C.ExtendedLibrary.Reset(C.Library);}
    if(C.Function&&!C.ExtendedFunction.IsValid())
    {
        if(!EnumLoaded(C.Function)||C.Function->GetOuter()!=C.Library)return ExtendedStop(C,R,TEXT("source_function_return"),TEXT("loaded actual selected function under held library"),TEXT("retired/different owner"));
        C.ExtendedFunction.Reset(C.Function);
        for(TFieldIterator<FProperty> It(C.Function,EFieldIterationFlags::None);It;++It){const auto* A=CastField<FArrayProperty>(*It);FProperty* Inner=A?A->Inner:nullptr;C.ExtendedFields.Add({*It,It->GetClass(),It->GetOwnerVariant(),It->GetFName(),uint64(It->GetPropertyFlags()),Inner,Inner?Inner->GetClass():nullptr,Inner?Inner->GetOwnerVariant():FFieldVariant(),Inner?uint64(Inner->GetPropertyFlags()):0});}
    }
    if(C.LibraryCDO&&!C.ExtendedCDO.IsValid()){if(!EnumLoaded(C.LibraryCDO)||C.LibraryCDO->GetClass()!=C.Library)return ExtendedStop(C,R,TEXT("source_cdo_return"),TEXT("loaded actual existing ArrayLibrary CDO"),TEXT("retired/different class"));C.ExtendedCDO.Reset(C.LibraryCDO);}
    return ExtendedObservedCurrent(&C);
}

bool ExtendedFreeze(FContext& C,FResult& R,UK2Node_CallArrayFunction* Node)
{
    if(!ExtendedCurrent(C,R,Node))return false;
    if(!Node||Node->Pins.Num()>32)return ExtendedStop(C,R,TEXT("new_pin_capture"),TEXT("bounded actual stock pins"),TEXT("null/over bound"));
    for(auto* Pin:Node->Pins){if(!Pin||Pin->WasTrashed()||Pin->GetOwningNodeUnchecked()!=Node||!Pin->PinId.IsValid())return ExtendedStop(C,R,TEXT("new_pin_capture"),TEXT("stock node-owned current pin/GUID"),TEXT("retired/different/invalid"));C.ExtendedPins.Add({Pin,Pin->PinId,Pin->PinName,Pin->Direction,Pin->PinType});}
    C.ExtendedPinsFixed=true;return ExtendedCurrent(C,R,Node);
}

bool Arguments(FContext& C, FResult& R, const FString& RequestedOperation, const FString& ElementType,
    const FString& DefaultsJson, const FString& TypeObject, const FString& EnumType, const FString& PositionJson, bool bCreate)
{
    if (!Function::Pin::SafeName(RequestedOperation) || !(C.Operation = OperationFor(RequestedOperation)))
    { return Error(R, TEXT("array_operation_unsupported"), TEXT("Supported operations: length, get, set, add, add_unique, insert, contains, remove, clear; remove means RemoveItem.")); }
    if (!Function::Pin::SafeName(ElementType))
    { return Error(R, TEXT("array_element_type_unsupported"), TEXT("Element type must be a bounded primitive8, enum/byteenum or supported native object/struct descriptor.")); }
    const FString Element = Normalize(ElementType);
    if (Element == TEXT("int") || Element == TEXT("integer")) { C.ElementCategory = UEdGraphSchema_K2::PC_Int; }
    else if (Element == TEXT("int64") || Element == TEXT("integer64")) { C.ElementCategory = UEdGraphSchema_K2::PC_Int64; }
    else if (Element == TEXT("float") || Element == TEXT("single")) { C.ElementCategory = UEdGraphSchema_K2::PC_Real; C.ElementSubCategory = UEdGraphSchema_K2::PC_Float; }
    else if (Element == TEXT("double") || Element == TEXT("real")) { C.ElementCategory = UEdGraphSchema_K2::PC_Real; C.ElementSubCategory = UEdGraphSchema_K2::PC_Double; }
    else if (Element == TEXT("text")) { C.ElementCategory = UEdGraphSchema_K2::PC_Text; }
    else if (Element == TEXT("bool") || Element == TEXT("boolean")) { C.ElementCategory = UEdGraphSchema_K2::PC_Boolean; }
    else if (Element == TEXT("vector")) { C.ElementCategory = UEdGraphSchema_K2::PC_Struct; }
    else if(Element==TEXT("name")){C.ElementCategory=UEdGraphSchema_K2::PC_Name;}
    else if(Element==TEXT("string")){C.ElementCategory=UEdGraphSchema_K2::PC_String;}
    else if((Element==TEXT("object")||Element==TEXT("class"))&&(Element==TEXT("class")||TypeObject.IsEmpty()||TypeObject==TEXT("/Script/CoreUObject.Object")||TypeObject==TEXT("/Script/Engine.Actor")))
    {
        if(!EnumType.IsEmpty())return Error(R,TEXT("array_descriptor_length_selector_unsupported"),TEXT("Finite native Object/Class Length requires empty enum_type."));
        const FString Constraint=TypeObject.IsEmpty()?TEXT("/Script/CoreUObject.Object"):TypeObject;
        C.DescriptorLength=true;C.DescriptorOperation=FString(C.Operation->Name)!=TEXT("length");C.ElementCategory=Element==TEXT("class")?UEdGraphSchema_K2::PC_Class:UEdGraphSchema_K2::PC_Object;
        C.ObjectScope=MakeShared<FObjectArrayScope>();if(!C.ObjectScope->PrepareNativeLength(Constraint,C.ElementCategory,R))return false;C.ElementObject=C.ObjectScope->Element.Get();
        if(C.DescriptorOperation)
        {
            R.Data->SetObjectField(TEXT("native_descriptor_array_operation_source_before"),R.Data->GetObjectField(TEXT("native_descriptor_array_length_source_before")));R.Data->RemoveField(TEXT("native_descriptor_array_length_source_before"));
            R.Data->SetBoolField(TEXT("native_descriptor_array_operation_parity"),true);R.Data->SetStringField(TEXT("descriptor_array_operation"),C.Operation->Name);R.Data->SetStringField(TEXT("descriptor_array_operation_category"),Element);R.Data->SetStringField(TEXT("descriptor_array_operation_class_path"),C.ObjectScope->Element->GetPathName());R.Data->SetBoolField(TEXT("descriptor_array_operation_runtime_invocation_verified"),false);
        }
        else{R.Data->SetBoolField(TEXT("native_descriptor_array_length"),true);R.Data->SetStringField(TEXT("descriptor_array_length_category"),Element);R.Data->SetStringField(TEXT("descriptor_array_length_class_path"),Constraint);R.Data->SetBoolField(TEXT("descriptor_array_length_runtime_invocation_verified"),false);}
    }
    else if(Element==TEXT("rotator")||Element==TEXT("transform"))
    {
        // Original implicit native descriptors ignore type_object and enum_type.
        C.StructLength=true;C.StructOperation=FString(C.Operation->Name)!=TEXT("length");C.StructLengthKind=Element;C.ElementCategory=UEdGraphSchema_K2::PC_Struct;
        C.ObjectScope=MakeShared<FObjectArrayScope>();if(!C.ObjectScope->PrepareStructLength(Element==TEXT("transform"),R,C.StructOperation))return false;C.ElementObject=C.ObjectScope->StructLengthRoles[0].Type.Get();
        if(C.StructOperation){R.Data->SetBoolField(TEXT("native_value_struct_array_operation_parity"),true);R.Data->SetStringField(TEXT("value_struct_array_operation"),C.Operation->Name);R.Data->SetStringField(TEXT("value_struct_array_kind"),Element);R.Data->SetStringField(TEXT("value_struct_array_type_path"),C.ObjectScope->StructLengthRoles[0].Path);R.Data->SetBoolField(TEXT("value_struct_array_runtime_invocation_verified"),false);}
        else{R.Data->SetBoolField(TEXT("native_value_struct_array_length"),true);R.Data->SetStringField(TEXT("value_struct_array_length_kind"),Element);R.Data->SetStringField(TEXT("value_struct_array_length_type_path"),C.ObjectScope->StructLengthRoles[0].Path);R.Data->SetBoolField(TEXT("value_struct_array_length_runtime_invocation_verified"),false);}
    }
    else if(Element==TEXT("scriptstruct")||(Element==TEXT("struct")&&TypeObject!=TEXT("/Script/CoreUObject.Vector4")))
    {
        // Original struct/scriptstruct descriptors read type_object and ignore enum_type.
        C.UdsLength=true;C.UdsOperation=FString(C.Operation->Name)!=TEXT("length");C.ElementCategory=UEdGraphSchema_K2::PC_Struct;C.ObjectScope=MakeShared<FObjectArrayScope>();C.ObjectScope->UdsOperationMode=C.UdsOperation;if(!C.ObjectScope->PrepareUdsLength(TypeObject,R))return false;C.ElementObject=C.ObjectScope->UdsType.Get();
        if(C.UdsOperation){R.Data->SetBoolField(TEXT("native_scalar_uds_array_operation_parity"),true);R.Data->SetStringField(TEXT("scalar_uds_array_operation"),C.Operation->Name);R.Data->SetStringField(TEXT("scalar_uds_array_operation_path"),C.ObjectScope->UdsPath);R.Data->SetBoolField(TEXT("scalar_uds_array_operation_runtime_invocation_verified"),false);R.Data->SetBoolField(TEXT("scalar_uds_array_literal_compile_validation_verified"),false);}
        else{R.Data->SetBoolField(TEXT("native_scalar_uds_array_length"),true);R.Data->SetStringField(TEXT("scalar_uds_array_length_path"),C.ObjectScope->UdsPath);R.Data->SetBoolField(TEXT("scalar_uds_array_length_runtime_invocation_verified"),false);}
    }
    else if(Element==TEXT("struct"))
    {
        if(TypeObject!=TEXT("/Script/CoreUObject.Vector4")||!EnumType.IsEmpty())return Error(R,TEXT("array_vector4_type_unsupported"),TEXT("Use struct, exact type_object=/Script/CoreUObject.Vector4 and empty enum_type; no f/d aliases or arbitrary structure."));
        C.ObjectScope=MakeShared<FObjectArrayScope>();if(!C.ObjectScope->PrepareVector4(R))return false;C.bVector4=true;
        C.ElementCategory=UEdGraphSchema_K2::PC_Struct;C.ElementObject=C.ObjectScope->Vector4.Get();
    }
    else if(Element==TEXT("object"))
    {
        const FString SkeletalOperation(C.Operation->Name);
        const bool SkeletalDedupRequest=TypeObject.Equals(TEXT("/Script/Engine.SkeletalMeshComponent"),ESearchCase::CaseSensitive)
            &&(SkeletalOperation==TEXT("clear")||SkeletalOperation==TEXT("contains")||SkeletalOperation==TEXT("add_unique"));
        if(SkeletalDedupRequest)
        {
            if(!EnumType.IsEmpty())return Error(R,TEXT("array_object_type_unsupported"),TEXT("Exact SkeletalMeshComponent clear/contains/add_unique requires empty enum_type; generated/alias/other class descriptors remain unsupported."));
            C.ObjectScope=MakeShared<FObjectArrayScope>();if(!C.ObjectScope->PrepareSkeletalDedup(R))return false;
            C.ElementCategory=UEdGraphSchema_K2::PC_Object;C.ElementObject=C.ObjectScope->Element.Get();
            R.Data->SetStringField(TEXT("native_skeletal_array_operation"),SkeletalOperation);
            R.Data->SetStringField(TEXT("skeletal_array_item_input_name"),SkeletalOperation==TEXT("contains")?TEXT("ItemToFind"):SkeletalOperation==TEXT("add_unique")?TEXT("NewItem"):TEXT(""));
            R.Data->SetBoolField(TEXT("skeletal_array_target_requires_connection"),true);R.Data->SetBoolField(TEXT("skeletal_array_item_requires_connection"),SkeletalOperation!=TEXT("clear"));
        }
        else
        {
        const FString Op(C.Operation->Name);
        const bool SkeletalLength=TypeObject.Equals(TEXT("/Script/Engine.SkeletalMeshComponent"),ESearchCase::CaseSensitive);
        // The resolver/table already admits exactly the original nine operations.
        // Only the previously missing finite pairs enable the observer below.
        C.ObjectParity=SkeletalLength?(Op==TEXT("add")||Op==TEXT("set")||Op==TEXT("insert")||Op==TEXT("remove")):(Op==TEXT("set")||Op==TEXT("add_unique")||Op==TEXT("insert")||Op==TEXT("contains")||Op==TEXT("remove"));
        if(!EnumType.IsEmpty()||(!SkeletalLength&&!TypeObject.Equals(TEXT("/Script/Engine.ActorComponent"),ESearchCase::CaseSensitive)&&!TypeObject.Equals(TEXT("/Script/Engine.ChildActorComponent"),ESearchCase::CaseSensitive)))
            return Error(R,TEXT("array_object_type_unsupported"),TEXT("Object arrays require exact native ActorComponent/ChildActorComponent, or SkeletalMeshComponent for length/get here, and empty enum_type; no generated/alias/arbitrary descriptor."));
        C.ObjectScope=MakeShared<FObjectArrayScope>();C.ObjectScope->OperationParityMode=C.ObjectParity;if(SkeletalLength?!C.ObjectScope->PrepareSkeletalLength(R):!C.ObjectScope->Prepare(TypeObject,R))return false;
        if(C.ObjectParity){R.Data->SetBoolField(TEXT("native_object_array_operation_parity"),true);R.Data->SetBoolField(TEXT("object_array_runtime_invocation_verified"),false);R.Data->SetStringField(TEXT("object_array_operation"),Op);R.Data->SetBoolField(TEXT("object_array_literal_defaults_supported"),false);if(SkeletalLength)R.Data->RemoveField(TEXT("native_skeletal_length_source_before"));}
        if(SkeletalLength&&Op==TEXT("get"))
        {
            // LU: only admission changes. Reuse LN source/current and the existing Array_Get copy writer.
            const auto Before=R.Data->GetObjectField(TEXT("native_skeletal_length_source_before"));
            R.Data->SetObjectField(TEXT("native_skeletal_get_source_before"),Before);R.Data->RemoveField(TEXT("native_skeletal_length_source_before"));
            R.Data->SetBoolField(TEXT("native_skeletal_array_get"),true);R.Data->SetBoolField(TEXT("skeletal_array_get_returns_copy"),true);R.Data->SetBoolField(TEXT("skeletal_array_get_runtime_verified"),false);
        }
        C.ElementCategory=UEdGraphSchema_K2::PC_Object;C.ElementObject=C.ObjectScope->Element.Get();
            }
    }
    else if(Element==TEXT("enum")||Element==TEXT("byteenum"))
    {
        C.EnumScope=MakeShared<FEnumArrayScope>();
        if(!C.EnumScope->Load(TypeObject.TrimStartAndEnd().IsEmpty()?EnumType:TypeObject,R)) { return false; }
        C.ElementCategory=UEdGraphSchema_K2::PC_Byte;C.ElementObject=C.EnumScope->Source.Get();
    }
    else { return Error(R, TEXT("array_element_type_unsupported"), TEXT("Node-only primitive8/enum/Vector use the existing nine-operation table; other element types remain pending.")); }
    if (!C.EnumScope.IsValid() && !C.ObjectScope.IsValid() && (!TypeObject.IsEmpty() || !EnumType.IsEmpty()))
    { return Error(R, TEXT("array_type_selector_unsupported"), TEXT("type_object and enum_type must remain empty for primitive8 and exact builtin Vector arrays.")); }
    C.CreateGraphIfMissing=bCreate;
    TSharedPtr<FJsonValue> Defaults, Position;
    if (!Function::Pin::ParseJson(DefaultsJson, Defaults, R,ExtendedPrimitive(C)||C.StructOperation||C.DescriptorOperation||C.UdsOperation) || !Function::Pin::ParseJson(PositionJson, Position, R)) { return false; }
    if (Defaults->Type == EJson::Null) { Defaults = MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()); }
    if (Defaults->Type != EJson::Object || Defaults->AsObject()->Values.Num() > 64)
    { return Error(R, TEXT("array_defaults_invalid"), TEXT("param_defaults_json must be an object of at most 64 distinct input pin names.")); }
    TSet<FName> Names;
    for (const auto& Pair : Defaults->AsObject()->Values)
    {
        const FString Key(Pair.Key.ToView());
        if (!Function::Pin::SafeName(Key)) { return Error(R, TEXT("array_default_pin_unsupported"), TEXT("Input default pin names must be bounded safe strings.")); }
        const FName Name(*Key);
        if (Name == FName(TEXT("TargetArray")))
        { return Error(R, TEXT("array_target_literal_unsupported"), TEXT("TargetArray must be connected later; array literal defaults are unsupported.")); }
        if (Names.Contains(Name)) { return Error(R, TEXT("array_defaults_invalid"), TEXT("Default pin names must be unique case-insensitively.")); } Names.Add(Name);
        const FParameter* Parameter = nullptr;
        for (int32 Index = 0; Index < C.Operation->Count; ++Index)
        { const FParameter& Candidate = C.Operation->Parameters[Index]; if (Name == FName(Candidate.Name) && Input(Candidate)) { Parameter = &Candidate; break; } }
        if (!Parameter) { return Error(R, TEXT("array_default_pin_unsupported"), TEXT("Unknown or output-only default pin for this exact operation: ") + Key); }
        const FEdGraphPinType Type = ParameterType(*Parameter, C.ElementCategory, C.ElementObject, C.ElementSubCategory); FString Value;
        if(C.DescriptorOperation&&(Type.PinCategory==UEdGraphSchema_K2::PC_Object||Type.PinCategory==UEdGraphSchema_K2::PC_Class))
        {
            if(Pair.Value->Type==EJson::String)
            {
                Value=Pair.Value->AsString();
                if(Value!=TEXT("/Script/CoreUObject.Object")&&Value!=TEXT("/Script/Engine.Actor")&&Value!=TEXT("/Script/Engine.ActorComponent")&&Value!=TEXT("/Script/Engine.ChildActorComponent")&&Value!=TEXT("/Script/Engine.SkeletalMeshComponent"))return Error(R,TEXT("array_descriptor_literal_resolution_pending"),TEXT("String Object/Class literals require an exact loaded native value class path; arbitrary instances, None-string resolution, aliases and loaders remain incomplete. Nonstring Any retains original default-string conversion."));
                auto Source=MakeShared<FContext::FDescriptorDefaultSource>();Source->Scope=MakeShared<FObjectArrayScope>();
                if(!Source->Scope->PrepareNativeLength(Value,UEdGraphSchema_K2::PC_Class,Source->Result)){R.Data->SetObjectField(TEXT("descriptor_array_default_preflight_failure"),Source->Result.Data);R.bChangeStateKnown=false;return Error(R,TEXT("array_descriptor_literal_source_unavailable"),Source->Result.Error);}
                if(!C.ObjectScope->Check())return false;
                UClass* ValueClass=Source->Scope->Element.Get();const auto* ConstraintClass=C.ObjectScope->Element.Get();
                const bool Compatible=Type.PinCategory==UEdGraphSchema_K2::PC_Class?ValueClass->IsChildOf(ConstraintClass):ValueClass->GetClass()->IsChildOf(ConstraintClass);
                if(!Source->Scope->Check()){R.bChangeStateKnown=false;return Error(R,TEXT("array_descriptor_literal_source_unavailable"),Source->Result.Error);}if(!C.ObjectScope->Check())return false;
                if(!Compatible)return Error(R,TEXT("array_descriptor_literal_type_mismatch"),TEXT("Loaded native value does not satisfy the class MetaClass/object PropertyClass constraint; target lookup and node allocation were not attempted."));
                C.DescriptorDefaultObjects.Add(Name,ValueClass);C.DescriptorDefaultSources.Add(FName(Parameter->Name),Source);
            }
            else if(!TextLiteral(Pair.Value,false,Value,R))return false;
            C.Defaults.Add({Name,Key,Value});continue;
        }
        if(C.ObjectScope.IsValid()&&C.ObjectScope->SkeletalDedupMode&&Type.PinCategory==UEdGraphSchema_K2::PC_Object)
        {return Error(R,TEXT("array_object_literal_unsupported"),TEXT("Exact SkeletalMeshComponent array input ")+Key+TEXT(" requires a typed wire; explicit literal/null is rejected before target lookup/allocation. No object value is loaded."));}
        if(C.ObjectScope.IsValid()&&Type.PinCategory==UEdGraphSchema_K2::PC_Object)
        {return Error(R,TEXT("array_object_literal_unsupported"),TEXT("Object NewItem requires a typed wire; explicit literal/null is rejected before target lookup/allocation. No object value is loaded."));}
        if(Type.PinCategory==UEdGraphSchema_K2::PC_Name||Type.PinCategory==UEdGraphSchema_K2::PC_String||Type.PinCategory==UEdGraphSchema_K2::PC_Text)
        {if(!TextLiteral(Pair.Value,Type.PinCategory==UEdGraphSchema_K2::PC_Name,Value,R)){return false;}}
        else if(Type.PinCategory==UEdGraphSchema_K2::PC_Int64||Type.PinCategory==UEdGraphSchema_K2::PC_Real)
        {if(!ExtendedNumberLiteral(Pair.Value,Type,Value,R))return false;}
        else if(Type.PinCategory==UEdGraphSchema_K2::PC_Struct)
        {
            // UE56 generic struct descriptors use string/number/bool/JSON fallback,
            // not field-name JSON conversion or per-element runtime initialization.
            if(C.UdsOperation){if(!TextLiteral(Pair.Value,false,Value,R))return false;}
            else if(C.StructOperation)
            {
                // UE56 applies the same descriptor Any conversion to item/default strings
                // independently of ContainerType. Reuse OK's corrected math conversion.
                FEdGraphPinType ConversionType=Type;ConversionType.ContainerType=EPinContainerType::Array;ConversionType.bIsReference=false;ConversionType.bIsConst=false;
                if(!C.ObjectScope->Check())return false;
                if(!Function::Pin::PrimitiveArrayDefaultText(ConversionType,Pair.Value,Value,R))return false;
                if(!C.ObjectScope->Check())return false;
                auto Conversion=MakeShared<FJsonObject>();Conversion->SetStringField(TEXT("requested_pin"),Key);Conversion->SetStringField(TEXT("converted_literal"),Value);Conversion->SetBoolField(TEXT("conversion_complete"),true);Conversion->SetBoolField(TEXT("runtime_initialization_verified"),false);R.Data->SetObjectField(FString(TEXT("value_struct_array_conversion_"))+Key,Conversion);
            }
            else if(C.bVector4)
            {
                if(!Pair.Value||Pair.Value->Type!=EJson::String)return Error(R,TEXT("array_vector4_literal_unsupported"),TEXT("Vector4 item literals support complete finite XYZW strings only; JSON arrays/objects/null are not original Vector4 conversions."));
                Value=Pair.Value->AsString();FVector4 Parsed;
                if(!Vector4Literal(*C.ObjectScope,Value,Parsed,R))return false;
                C.Vector4LiteralValues.Add(Name,Parsed);
                auto Proof=MakeShared<FJsonObject>();Proof->SetStringField(TEXT("raw_default"),Value);Proof->SetBoolField(TEXT("import_fully_consumed"),true);Proof->SetBoolField(TEXT("native_source_current_verified"),true);
                for(int32 I=0;I<4;++I)Proof->SetNumberField(I==0?TEXT("X"):I==1?TEXT("Y"):I==2?TEXT("Z"):TEXT("W"),Parsed[I]);
                R.Data->SetObjectField(FString(TEXT("vector4_literal_preflight_"))+Key,Proof);
            }
            else if(!VectorLiteral(Pair.Value,Value,R))return false;
        }
        else if(Type.PinCategory==UEdGraphSchema_K2::PC_Byte&&C.EnumScope.IsValid())
        { if(!C.EnumScope->Value(Pair.Value,Value)) { return false; } }
        else
        if (Type.PinCategory == UEdGraphSchema_K2::PC_Boolean)
        {
            if (!Pair.Value || Pair.Value->Type != EJson::Boolean) { return Error(R, TEXT("array_default_type_invalid"), Key + TEXT(" must be a JSON boolean.")); }
            Value = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
        }
        else
        {
            int32 Number = 0; if (!ExactInt32(Pair.Value, Number)) { return Error(R, TEXT("array_default_type_invalid"), Key + TEXT(" must be an exact JSON int32 integer; fractional truncation is forbidden.")); }
            Value = FString::FromInt(Number);
        }
        C.Defaults.Add({Name, Key, Value});
    }
    if (Position->Type == EJson::Null)
    { C.X = C.Y = 0; }
    else if (Position->Type != EJson::Array || Position->AsArray().Num() != 2
        || !ExactInt32(Position->AsArray()[0], C.X) || !ExactInt32(Position->AsArray()[1], C.Y)
        || C.X < -1000000 || C.X > 1000000 || C.Y < -1000000 || C.Y > 1000000)
    { return Error(R, TEXT("array_position_invalid"), TEXT("node_position_json must contain two integers from -1000000 to 1000000.")); }
    if (C.ElementCategory == UEdGraphSchema_K2::PC_Struct && !C.bVector4 && !C.StructLength && !C.UdsLength)
    {
        C.ElementObject = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Vector"));
        C.VectorBefore = VectorState(Cast<UScriptStruct>(C.ElementObject));
        R.Data->SetObjectField(TEXT("native_vector_source_before"), C.VectorBefore);
        if (!C.VectorBefore->GetBoolField(TEXT("observations_complete")) || !C.VectorBefore->GetBoolField(TEXT("native_vector_identity_verified")))
        { return Error(R, TEXT("array_vector_native_type_unavailable"), TEXT("Exact already-loaded, clean runtime CoreUObject.Vector identity and FVector size are required before Blueprint lookup; no loads requested.")); }
    }
    if(C.DescriptorOperation){auto Sources=MakeShared<FJsonObject>();for(const auto& Pair:C.DescriptorDefaultSources){if(!Pair.Value->Scope->Check()){R.bChangeStateKnown=false;return Error(R,TEXT("array_descriptor_literal_source_unavailable"),Pair.Value->Result.Error);}Sources->SetObjectField(Pair.Key.ToString(),Pair.Value->Scope->NativeLengthState());}R.Data->SetObjectField(TEXT("descriptor_array_default_sources_before"),Sources);}
    return true;
}

FString GraphKind(const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    if (Blueprint->FunctionGraphs.Contains(Graph)) { return TEXT("function"); }
    if (Blueprint->MacroGraphs.Contains(Graph)) { return TEXT("macro"); }
    if (Blueprint->UbergraphPages.Contains(Graph) || Blueprint->EventGraphs.Contains(Graph)) { return TEXT("event"); }
    return TEXT("unknown");
}

TSharedRef<FJsonObject> GraphDescription(const FContext& C)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("graph_name"), C.Graph->GetName());
    Row->SetStringField(TEXT("graph_id"), C.Graph->GraphGuid.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("graph_type"), GraphKind(C.Blueprint, C.Graph));
    Row->SetStringField(TEXT("object_path"), C.Graph->GetPathName()); Row->SetStringField(TEXT("schema"), C.Schema->GetClass()->GetPathName());
    Row->SetNumberField(TEXT("node_count"), C.Graph->Nodes.Num()); return Row;
}

bool BootstrapDefaultEventGraph(FContext& C,FResult& R);

bool ResolveTarget(FContext& C, FResult& R, const FString& Path, const FString& GraphName, const FString& GraphId, const FString& GraphType)
{
    if (!Function::Pin::SafeName(Path) || !Path.StartsWith(TEXT("/Game/")) || Path.Contains(TEXT(":")))
    { return Error(R, TEXT("array_blueprint_path_invalid"), TEXT("Use an exact bounded Game Blueprint asset or package path.")); }
    const FString AssetPath = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
    if (!Function::Pin::SafeName(AssetPath)) { return Error(R, TEXT("array_blueprint_path_invalid"), TEXT("Completed asset path exceeds the bound.")); }
    C.Blueprint = FindObject<UBlueprint>(nullptr, *AssetPath);
    if (!FullyLoaded(C.Blueprint)) { return Error(R, TEXT("array_blueprint_not_loaded"), TEXT("Load the exact Blueprint through the official asset tool first; this operation never loads it implicitly.")); }
    UBlueprint* BP = C.Blueprint;
    if(C.Blueprint->BlueprintType==BPTYPE_FunctionLibrary)
    {
        if(!(C.ElementCategory==UEdGraphSchema_K2::PC_Name&&FString(C.Operation->Name)==TEXT("length"))) {return Error(R,TEXT("array_blueprint_state_unsupported"),TEXT("Only Name MakeArray and Name Array_Length are supported in the exact member-free PresetLoader BFL."));}
        C.NameScope=Function::BeginPresetLoaderNameArray(R,C.Blueprint,GraphName,GraphId,GraphType,true,C.Graph,C.Schema);
        if(!C.NameScope.IsValid())return false;
        R.Data->SetObjectField(TEXT("graph"),GraphDescription(C));R.Data->SetStringField(TEXT("resolved_blueprint_path"),C.Blueprint->GetPathName());return C.NameScope->Check();
    }
    if (BP->GetPathName() != AssetPath || BP->BlueprintType != BPTYPE_Normal || !FullyLoaded(BP->ParentClass) || !BP->SkeletonGeneratedClass
        || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->bIsRegeneratingOnLoad
        || BP->Status == BS_BeingCreated || BP->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
    { return Error(R, TEXT("array_blueprint_state_unsupported"), TEXT("Requires an already-loaded normal idle runtime Blueprint with a current skeleton.")); }
    for (const UClass* Parent = BP->ParentClass; Parent; Parent = Parent->GetSuperClass())
    {
        if (!FullyLoaded(Parent) || !Parent->HasAnyClassFlags(CLASS_Native) || Parent->IsEditorOnly()
            || Parent->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Interface)
            || (Parent->GetOutermost()->GetName() != TEXT("/Script/Engine") && Parent->GetOutermost()->GetName() != TEXT("/Script/CoreUObject")))
        { return Error(R, TEXT("array_blueprint_parent_unsupported"), TEXT("This cohort requires an exact loaded native Engine/CoreUObject runtime parent chain.")); }
    }
    TArray<FString> External;
    for (TObjectIterator<UBlueprint> It; It; ++It)
    { if (*It != BP && BP->GeneratedClass && It->ParentClass && It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.AddUnique(It->GetPathName()); } }
    if (BP->GeneratedClass)
    {
        TArray<UObject*> Instances; GetObjectsOfClass(BP->GeneratedClass, Instances, true, RF_NoFlags);
        for (UObject* Instance : Instances) { if (IsValid(Instance) && Instance->GetOutermost() != BP->GetOutermost()) { External.AddUnique(Instance->GetPathName()); } }
    }
    External.Sort(); TArray<TSharedPtr<FJsonValue>> Rows; for (const FString& Value : External) { Rows.Add(MakeShared<FJsonValueString>(Value)); }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"), Rows);
    if (!External.IsEmpty()) { return Error(R, TEXT("array_external_instances_unsupported"), TEXT("Loaded external instances or derived Blueprints are outside this mutation slice.")); }
    if (!Function::Pin::SafeName(GraphName, true) || !Function::Pin::SafeName(GraphId, true) || !Function::Pin::SafeName(GraphType, true))
    { return Error(R, TEXT("array_graph_selector_invalid"), TEXT("Graph selectors must be bounded safe strings.")); }
    FGuid RequestedGuid; if (!GraphId.IsEmpty() && (!FGuid::Parse(GraphId, RequestedGuid) || !RequestedGuid.IsValid()))
    { return Error(R, TEXT("array_graph_selector_invalid"), TEXT("graph_id must be a valid nonzero GUID.")); }
    FString Kind = Normalize(GraphType);
    if (Kind == TEXT("eventgraph") || Kind == TEXT("ubergraph")) { Kind = TEXT("event"); }
    else if (Kind == TEXT("functiongraph")) { Kind = TEXT("function"); }
    else if (Kind == TEXT("macrograph")) { Kind = TEXT("macro"); }
    if (!Kind.IsEmpty() && Kind != TEXT("any") && Kind != TEXT("event") && Kind != TEXT("function") && Kind != TEXT("macro"))
    { return Error(R, TEXT("array_graph_type_unsupported"), TEXT("Initial selectors support existing direct event/function/macro K2 graphs only.")); }
    const bool DefaultSelector=GraphName.IsEmpty()&&GraphId.IsEmpty()&&Kind.IsEmpty();
    const bool ExplicitDefaultEvent=GraphName.IsEmpty()&&GraphId.IsEmpty()&&Kind==TEXT("event")&&C.CreateGraphIfMissing;
    if(DefaultSelector)
    {
        // UE56 FindOrCreateEventGraph searches only UbergraphPages, first Contains match.
        for(const auto& Slot:BP->UbergraphPages){if(!Slot.IsResolved())return Error(R,TEXT("array_event_graph_unresolved"),TEXT("Default EventGraph selection will not load an unresolved page."));UEdGraph* G=Slot.Get();if(FullyLoaded(G)&&G->GetName().Contains(TEXT("EventGraph"))){C.Graph=G;break;}}
        if(!C.Graph&&!BootstrapDefaultEventGraph(C,R))return false;
        Kind=TEXT("event");
    }
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    if(C.GraphCreationCurrent&&!ExtendedCurrent(C,R,nullptr))return false;
    if (Graphs.Num() > 256) { return Error(R, TEXT("array_graph_budget_exceeded"), TEXT("Graph selection exceeds 256 owned graphs.")); }
    for (UEdGraph* Graph : Graphs)
    {
        if(DefaultSelector)break;
        if (!IsValid(Graph) || (!GraphName.IsEmpty() && !Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
            || (!GraphId.IsEmpty() && Graph->GraphGuid != RequestedGuid) || (!Kind.IsEmpty() && Kind != TEXT("any") && GraphKind(BP, Graph) != Kind)) { continue; }
        if (C.Graph) { return Error(R, TEXT("array_graph_ambiguous"), TEXT("Graph selectors match multiple graphs; provide an exact name or GUID.")); } C.Graph = Graph;
        if(ExplicitDefaultEvent)break; // UE56 FindBlueprintGraph returns the first existing match.
    }
    if(!C.Graph&&ExplicitDefaultEvent&&!BootstrapDefaultEventGraph(C,R))return false;
    if(!C.Graph&&C.CreateGraphIfMissing)
    {
        if(!GraphId.IsEmpty()||Kind!=TEXT("function")||GraphName.IsEmpty())return Error(R,TEXT("array_graph_creation_selector_unsupported"),TEXT("Creation supports the original default EventGraph conditions or an explicit function name/type without graph_id; named missing events/macros were not created by UE56."));
        if(C.ElementObject!=nullptr||C.EnumScope.IsValid()||C.ObjectScope.IsValid()||C.NameScope.IsValid())return Error(R,TEXT("array_graph_creation_profile_unsupported"),TEXT("New function creation is limited to normal Blueprint stable ForLoop or primitive Array profiles; existing selected graph support is unchanged."));
        UBlueprint* ReadonlySource=nullptr;

        if(!Function::CreateFunctionGraphForNode(BP,GraphName,R,C.Graph,C.GraphCreationCurrent,ReadonlySource))return false;

    }
    else if(C.Graph&&(C.CreateGraphIfMissing||DefaultSelector)&&!C.DefaultEventAttached)
    {
        auto Resolution=MakeShared<FJsonObject>();Resolution->SetBoolField(TEXT("requested_create_if_missing"),C.CreateGraphIfMissing);Resolution->SetBoolField(TEXT("created"),false);Resolution->SetBoolField(TEXT("reused_existing"),true);Resolution->SetStringField(TEXT("graph_path"),C.Graph->GetPathName());Resolution->SetStringField(TEXT("graph_guid"),C.Graph->GraphGuid.ToString(EGuidFormats::Digits));Resolution->SetBoolField(TEXT("implicit_skeleton_compile_possible"),false);R.Data->SetObjectField(TEXT("graph_resolution"),Resolution);
    }
    if (!C.Graph) { return Error(R, TEXT("array_graph_not_found"), TEXT("No matching existing graph; nothing was created.")); }
    UClass* SchemaClass = C.Graph->Schema.Get();
    C.Schema = FullyLoaded(SchemaClass) && SchemaClass->GetPathName() == TEXT("/Script/BlueprintGraph.EdGraphSchema_K2")
        ? Cast<UEdGraphSchema_K2>(SchemaClass->GetDefaultObject(false)) : nullptr;
    if(C.GraphCreationCurrent&&!ExtendedCurrent(C,R,nullptr))return false;
    if (!FullyLoaded(C.Graph) || C.Graph->GetOuter() != BP || !C.Graph->GraphGuid.IsValid() || GraphKind(BP, C.Graph) == TEXT("unknown")
        || !FullyLoaded(C.Schema) || C.Schema->GetClass() != SchemaClass || C.Graph->Nodes.Num() >= 2048)
    { return Error(R, TEXT("array_graph_state_unsupported"), TEXT("Requires a directly owned standard K2 graph with valid GUID and room below 2048 nodes.")); }
    if (!(C.Operation->Flags & FUNC_BlueprintPure))
    {
        const bool SupportsImpure=C.Schema->DoesGraphSupportImpureFunctions(C.Graph);
        if(C.GraphCreationCurrent&&!ExtendedCurrent(C,R,nullptr))return false;
        if(!SupportsImpure)return Error(R,TEXT("array_graph_operation_rejected"),TEXT("Selected graph does not permit this impure array operation."));
    }
    R.Data->SetObjectField(TEXT("graph"), GraphDescription(C)); R.Data->SetStringField(TEXT("resolved_blueprint_path"), BP->GetPathName());
    R.Data->SetBoolField(TEXT("loaded_during_operation"), false); R.Data->SetBoolField(TEXT("schema_cdo_already_loaded"), true); return true;
}

TSharedRef<FJsonObject> NativeState(FContext& C)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("function_path"), C.Function->GetPathName());
    Row->SetStringField(TEXT("class_flags_string"), LexToString(static_cast<uint32>(C.Library->ClassFlags)));
    Row->SetStringField(TEXT("function_flags_string"), LexToString(static_cast<uint32>(C.Function->FunctionFlags)));
    Row->SetNumberField(TEXT("declared_num_parms"), C.Function->NumParms); Row->SetObjectField(TEXT("package"), PackageState(C.Library->GetOutermost()));
    Row->SetStringField(TEXT("existing_default_object_path"), C.LibraryCDO->GetPathName());
    for (const TCHAR* Key : {TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("AutoCreateRefTerm"), TEXT("BlueprintInternalUseOnly"), TEXT("CustomThunk"),
        TEXT("Latent"), TEXT("DeprecatedFunction"), TEXT("WorldContext"), TEXT("ScriptName"), TEXT("BlueprintThreadSafe"), TEXT("NativeBreakFunc")})
    { const FString Value=C.Function->GetMetaData(Key);if(!ExtendedObservedCurrent(&C))return Row;Row->SetStringField(Key, Value); }
    TArray<TSharedPtr<FJsonValue>> Parameters; int32 Count = 0; bool Complete = true;
    for (TFieldIterator<FProperty> It(C.Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm)) { continue; }
        if (++Count > 32) { Complete = false; break; }
        auto Parameter = DescribeProperty(*It, C.Schema,0,C.NameScope.Get(),TrackedArrayObservation(C)&&C.ExtendedBound?&C:nullptr);if((C.NameScope.IsValid()&&!C.NameScope->Check())||!ExtendedObservedCurrent(&C))return Row; const FString DefaultKey = TEXT("CPP_Default_") + It->GetName();
        Parameter->SetBoolField(TEXT("exact_declared_owner"), It->GetOwnerStruct() == C.Function);
        Complete &= It->GetOwnerStruct() == C.Function && Parameter->GetBoolField(TEXT("schema_conversion_success"));
        Parameter->SetBoolField(TEXT("cpp_default_metadata_present"), C.Function->HasMetaData(FName(*DefaultKey)));if(!ExtendedObservedCurrent(&C))return Row;
        Parameter->SetStringField(TEXT("cpp_default_metadata_value"), C.Function->GetMetaData(FName(*DefaultKey)));if(!ExtendedObservedCurrent(&C))return Row;
        Parameters.Add(MakeShared<FJsonValueObject>(Parameter));
    }
    Row->SetArrayField(TEXT("parameters"), Parameters);
    Row->SetBoolField(TEXT("observations_complete"), Complete && Count == C.Function->NumParms); return Row;
}

bool Native(FContext& C, FResult& R)
{
    if(C.NameScope.IsValid()){if(!C.NameScope->NativeLength(C.Library,C.Function,C.LibraryCDO))return false;}
    else
    {
    C.Library = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetArrayLibrary"));if(!ExtendedAdoptNative(C,R))return false;
    C.Function = ExactNativeClass(C.Library, TEXT("/Script/Engine.KismetArrayLibrary")) ? C.Library->FindFunctionByName(C.Operation->FunctionName) : nullptr;if(!ExtendedAdoptNative(C,R))return false;
    C.LibraryCDO = C.Library ? C.Library->GetDefaultObject(false) : nullptr;if(!ExtendedAdoptNative(C,R))return false;
    }
    if (!FullyLoaded(C.Function) || !FullyLoaded(C.LibraryCDO) || C.LibraryCDO->GetClass() != C.Library
        || C.LibraryCDO->GetPathName() != TEXT("/Script/Engine.Default__KismetArrayLibrary") || C.Function->GetOuterUClass() != C.Library)
    { return Error(R, TEXT("array_native_support_unavailable"), TEXT("Requires the exact loaded native array function and already-created ArrayLibrary CDO; no loads or CDO creation requested.")); }
    C.NativeBefore = NativeState(C);if((C.NameScope.IsValid()&&!C.NameScope->Check())||!ExtendedObservedCurrent(&C))return false; R.Data->SetObjectField(TEXT("native_signature_before"), C.NativeBefore);
    bool Match = C.NativeBefore->GetBoolField(TEXT("observations_complete")) && C.Library->ClassFlags == 810025120U && C.Function->FunctionFlags == C.Operation->Flags && C.Function->NumParms == C.Operation->Count
        && (TrackedArrayObservation(C)?C.NativeBefore->GetStringField(TEXT("ArrayParm")):C.Function->GetMetaData(TEXT("ArrayParm"))) == TEXT("TargetArray") && (TrackedArrayObservation(C)?C.NativeBefore->GetStringField(TEXT("ArrayTypeDependentParams")):C.Function->GetMetaData(TEXT("ArrayTypeDependentParams"))) == C.Operation->Dependent
        && (TrackedArrayObservation(C)?C.NativeBefore->GetStringField(TEXT("AutoCreateRefTerm")):C.Function->GetMetaData(TEXT("AutoCreateRefTerm"))) == C.Operation->RefTerm && (TrackedArrayObservation(C)?C.NativeBefore->GetStringField(TEXT("BlueprintInternalUseOnly")):C.Function->GetMetaData(TEXT("BlueprintInternalUseOnly"))) == C.Operation->Internal
        && (TrackedArrayObservation(C)?C.NativeBefore->GetStringField(TEXT("CustomThunk")):C.Function->GetMetaData(TEXT("CustomThunk"))) == TEXT("true");
    for (const TCHAR* Key : {TEXT("Latent"), TEXT("DeprecatedFunction"), TEXT("WorldContext"), TEXT("ScriptName"), TEXT("BlueprintThreadSafe"), TEXT("NativeBreakFunc")})
    { Match &= (TrackedArrayObservation(C)?C.NativeBefore->GetStringField(Key):C.Function->GetMetaData(Key)).IsEmpty(); }
    int32 Index = 0;
    for (TFieldIterator<FProperty> It(C.Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm)) { continue; }
        if (Index >= C.Operation->Count) { Match = false; break; }
        const FParameter& Expected = C.Operation->Parameters[Index++]; FEdGraphPinType Type;
        Match &= It->GetName() == Expected.Name && It->GetClass()->GetName() == Expected.Class && It->GetOwnerStruct() == C.Function && It->ArrayDim == 1
            && static_cast<uint64>(It->GetPropertyFlags()) == Expected.Flags && C.Schema->ConvertPropertyToPinType(*It, Type) && SameType(Type, ParameterType(Expected));
        if((C.NameScope.IsValid()&&!C.NameScope->Check())||!ExtendedObservedCurrent(&C))return false;
        const FString DefaultKey = TEXT("CPP_Default_") + It->GetName(); Match &= !C.Function->HasMetaData(FName(*DefaultKey));if(!ExtendedObservedCurrent(&C))return false;
        if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(*It))
        {
            FEdGraphPinType InnerType; FEdGraphPinType ExpectedInner; ExpectedInner.PinCategory = UEdGraphSchema_K2::PC_Int;
            Match &= ArrayProperty->Inner && ArrayProperty->Inner->ArrayDim == 1 && CastField<FIntProperty>(ArrayProperty->Inner)
                && static_cast<uint64>(ArrayProperty->Inner->GetPropertyFlags()) == 2251869606904320ULL
                && C.Schema->ConvertPropertyToPinType(ArrayProperty->Inner, InnerType) && SameType(InnerType, ExpectedInner);
            if((C.NameScope.IsValid()&&!C.NameScope->Check())||!ExtendedObservedCurrent(&C))return false;
        }
    }
    Match &= Index == C.Operation->Count;
    R.Data->SetStringField(TEXT("native_contract_basis"), TEXT("UE5.8.3-58210709 engine-array-native-support-AG2 exact nine-array reflection"));
    R.Data->SetBoolField(TEXT("native_array_contract_verified"), Match);
    return Match || Error(R, TEXT("array_native_contract_mismatch"), TEXT("Exact selected native flags, ordered parameters, schema types or metadata differ from AG2; no node was created."));
}

FString TypeText(const FEdGraphPinType& Type)
{ FString Out; FEdGraphPinType::StaticStruct()->ExportText(Out, &Type, &Type, nullptr, PPF_None, nullptr); return Out; }

TSharedRef<FJsonObject> NodeState(const UEdGraphNode* Node,FEnumArrayScope* Scope=nullptr,FVectorArrayScope* VectorScope=nullptr,FObjectArrayScope* ObjectScope=nullptr,Function::FPresetLoaderNameArrayScope* NameScope=nullptr,FContext* Extended=nullptr)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("object_path"), Node->GetPathName());
    Row->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    Row->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); Row->SetStringField(TEXT("comment"), Node->NodeComment);
    Row->SetNumberField(TEXT("x"), Node->NodePosX); Row->SetNumberField(TEXT("y"), Node->NodePosY);
    const auto Enabled=Node->GetDesiredEnabledState();if((NameScope&&!NameScope->Check())||(Scope&&!Scope->Check())||(VectorScope&&!VectorScope->Check())||(ObjectScope&&!ObjectScope->Check())||(Extended&&!ExtendedCurrent(*Extended,*Extended->ExtendedResult,Extended->ExtendedCreated.Get()))) { return Row; }
    const bool Ghost=Node->IsAutomaticallyPlacedGhostNode();if((NameScope&&!NameScope->Check())||(Scope&&!Scope->Check())||(VectorScope&&!VectorScope->Check())||(ObjectScope&&!ObjectScope->Check())||(Extended&&!ExtendedCurrent(*Extended,*Extended->ExtendedResult,Extended->ExtendedCreated.Get()))) { return Row; }
    Row->SetNumberField(TEXT("enabled_state"),static_cast<int32>(Enabled));Row->SetBoolField(TEXT("ghost"),Ghost);
    TArray<TSharedPtr<FJsonValue>> Pins;
    const auto PinCopies=Extended?Node->Pins:TArray<UEdGraphPin*>();const auto& PinRows=Extended?PinCopies:Node->Pins;
    for (const UEdGraphPin* Pin : PinRows)
    {
        if (!Pin || Pin->WasTrashed()) { Row->SetBoolField(TEXT("pin_observation_incomplete"), true); continue; }
        if(ObjectScope&&ObjectScope->IsVector4()&&!Pin->DefaultObject.IsResolved()){ObjectScope->Bad(TEXT("vector4_observed_default_unresolved"));return Row;}
        auto P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("name"), Pin->PinName.ToString()); P->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::Digits));
        P->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        const FEdGraphPinType CopiedPinType=Pin->PinType;auto Type = Function::Pin::Describe(Extended?CopiedPinType:Pin->PinType);if(!ExtendedObservedCurrent(Extended))return Row;if(NameScope&&!NameScope->Check())return Row;if(ObjectScope&&ObjectScope->IsVector4()&&!ObjectScope->Check())return Row; Type->SetBoolField(TEXT("is_const"), Pin->PinType.bIsConst); P->SetObjectField(TEXT("pin_type"), Type);
        P->SetStringField(TEXT("pin_type_text"), TypeText(Extended?CopiedPinType:Pin->PinType));if(!ExtendedObservedCurrent(Extended))return Row;if(NameScope&&!NameScope->Check())return Row;if(ObjectScope&&ObjectScope->IsVector4()&&!ObjectScope->Check())return Row; P->SetStringField(TEXT("default_value"), Pin->DefaultValue);
        P->SetStringField(TEXT("autogenerated_default_value"), Pin->AutogeneratedDefaultValue); P->SetStringField(TEXT("default_object"), ObjectScope && Node == ObjectScope->Node.Get() ? (Pin->PinName == UEdGraphSchema_K2::PN_Self ? FString(TEXT("/Script/Engine.Default__KismetArrayLibrary")) : Extended&&Extended->DescriptorOperation ? (Pin->DefaultObject==nullptr?FString():Pin->DefaultObject.GetPathName()) : FString()) : ObjectScope&&ObjectScope->IsVector4() ? (Pin->DefaultObject==nullptr?FString():Pin->DefaultObject.GetPathName()) : NameScope?(Pin->DefaultObject==nullptr?FString():Pin->DefaultObject.GetPathName()):Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString());
        if(ObjectScope&&ObjectScope->IsVector4()&&!ObjectScope->Check())return Row;
        if(NameScope&&!NameScope->Check())return Row;const FText ObservedText=Pin->DefaultTextValue;const FString ObservedDisplay=ObservedText.ToString();if(Extended&&!ExtendedCurrent(*Extended,*Extended->ExtendedResult,Extended->ExtendedCreated.Get()))return Row;P->SetStringField(TEXT("default_text"), ObservedDisplay); P->SetBoolField(TEXT("hidden"), Pin->bHidden); P->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin);
        P->SetBoolField(TEXT("default_value_is_ignored"), Pin->bDefaultValueIsIgnored); P->SetBoolField(TEXT("default_value_is_read_only"), Pin->bDefaultValueIsReadOnly);
        P->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable); P->SetBoolField(TEXT("advanced_view"), Pin->bAdvancedView);
        P->SetBoolField(TEXT("display_as_mutable_ref"), Pin->bDisplayAsMutableRef);
        if (Pin->ParentPin && Pin->ParentPin->WasTrashed()) { P->SetBoolField(TEXT("parent_pin_observation_incomplete"), true); }
        P->SetStringField(TEXT("parent_pin_id"), Pin->ParentPin && !Pin->ParentPin->WasTrashed() ? Pin->ParentPin->PinId.ToString(EGuidFormats::Digits) : FString());
        TArray<TSharedPtr<FJsonValue>> Children;
        for (const UEdGraphPin* Child : Pin->SubPins)
        {
            if (!Child || Child->WasTrashed()) { P->SetBoolField(TEXT("sub_pin_observation_incomplete"), true); Children.Add(MakeShared<FJsonValueString>(FString())); }
            else { Children.Add(MakeShared<FJsonValueString>(Child->PinId.ToString(EGuidFormats::Digits))); }
        }
        P->SetArrayField(TEXT("sub_pin_ids"), Children); TArray<FString> Edges;
        for (const UEdGraphPin* Other : Pin->LinkedTo)
        {
            if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked())) { P->SetBoolField(TEXT("link_observation_incomplete"), true); continue; }
            Edges.Add(Other->GetOwningNodeUnchecked()->GetPathName() + TEXT(":") + Other->PinName.ToString());
        }
        Edges.Sort();
        TArray<TSharedPtr<FJsonValue>> Links; for (const FString& Edge : Edges) { Links.Add(MakeShared<FJsonValueString>(Edge)); } P->SetArrayField(TEXT("links"), Links);
        Pins.Add(MakeShared<FJsonValueObject>(P));
    }
    Row->SetArrayField(TEXT("pins"), Pins);
    if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
    {
        Row->SetStringField(TEXT("function_name"), Call->FunctionReference.GetMemberName().ToString()); const UClass* Owner = ObjectScope && Node == ObjectScope->Node.Get() ? ObjectScope->FunctionOwner.Get() : Call->FunctionReference.GetMemberParentClass();if(ObjectScope&&ObjectScope->IsVector4()&&!ObjectScope->Check())return Row;TStrongObjectPtr<const UClass> ReturnedOwner;
        if(Extended){if(!ExtendedObservedCurrent(Extended))return Row;if(Owner){if(!EnumLoaded(Owner)){ExtendedStop(*Extended,*Extended->ExtendedResult,TEXT("member_parent_return"),TEXT("loaded resolved parent class"),TEXT("retired return"));return Row;}ReturnedOwner.Reset(Owner);}if(!ExtendedObservedCurrent(Extended))return Row;}
        if(NameScope&&!NameScope->Check())return Row;Row->SetStringField(TEXT("function_member_parent_path"), Owner ? Owner->GetPathName() : FString());
        Row->SetStringField(TEXT("function_member_guid"), Call->FunctionReference.GetMemberGuid().ToString(EGuidFormats::Digits)); Row->SetBoolField(TEXT("function_self_context"), Call->FunctionReference.IsSelfContext());
    }
    return Row;
}

bool Capture(FContext& C, const TSharedRef<FJsonObject>& Out)
{
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||(C.ExtendedBound&&!ExtendedCurrent(C,*C.ExtendedResult,C.ExtendedCreated.Get()))) { return false; }
    if (!IsValid(C.Blueprint) || (!C.DefaultEventPending&&(!IsValid(C.Graph)||C.Graph->GetOuter()!=C.Blueprint))) { return false; }
    Out->SetStringField(TEXT("blueprint_path"), C.Blueprint->GetPathName()); Out->SetStringField(TEXT("parent_class"), (C.bVector4&&(C.ObjectScope->Parent != nullptr))?C.ObjectScope->Parent->GetPathName():C.Blueprint->ParentClass ? C.Blueprint->ParentClass->GetPathName() : FString());
    if(C.NameScope.IsValid()&&!C.NameScope->Check())return false;Out->SetBoolField(TEXT("dirty"), C.Blueprint->GetOutermost()->IsDirty()); Out->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(C.Blueprint->Status.GetValue()));
    TArray<TSharedPtr<FJsonValue>> Variables;
    const auto VariableCopies=(C.bVector4||TrackedArrayObservation(C))?C.Blueprint->NewVariables:TArray<FBPVariableDescription>();
    const auto& VariableRows=(C.bVector4||TrackedArrayObservation(C))?VariableCopies:C.Blueprint->NewVariables;
    for (const FBPVariableDescription& Variable : VariableRows)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Variable.VarName.ToString()); Row->SetStringField(TEXT("guid"), Variable.VarGuid.ToString(EGuidFormats::Digits));
        FString Declaration; FBPVariableDescription::StaticStruct()->ExportText(Declaration, &Variable, &Variable, C.Blueprint, PPF_None, nullptr);
        if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||(C.ExtendedBound&&!ExtendedCurrent(C,*C.ExtendedResult,C.ExtendedCreated.Get()))) { return false; }
        Row->SetStringField(TEXT("declaration_text"), Declaration);
        Row->SetStringField(TEXT("type"), TypeText(Variable.VarType));if(!ExtendedObservedCurrent(&C))return false; Row->SetStringField(TEXT("flags_string"), LexToString(Variable.PropertyFlags)); Row->SetStringField(TEXT("stored_default"), Variable.DefaultValue);
        Row->SetStringField(TEXT("category"), Variable.Category.ToString());if(!ExtendedObservedCurrent(&C))return false; TArray<TSharedPtr<FJsonValue>> Metadata;
        for (const FBPVariableMetaDataEntry& Entry : Variable.MetaDataArray)
        { auto M = MakeShared<FJsonObject>(); M->SetStringField(TEXT("key"), Entry.DataKey.ToString()); M->SetStringField(TEXT("value"), Entry.DataValue); Metadata.Add(MakeShared<FJsonValueObject>(M)); }
        Row->SetArrayField(TEXT("metadata"), Metadata); Variables.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("variables"), Variables); TArray<TSharedPtr<FJsonValue>> Interfaces;
    for (const FBPInterfaceDescription& Interface : C.Blueprint->ImplementedInterfaces)
    {
        if (!IsValid(Interface.Interface)) { return false; } auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("class_path"), Interface.Interface->GetPathName());
        TArray<TSharedPtr<FJsonValue>> Rows; for (const UEdGraph* Graph : Interface.Graphs) { if (!IsValid(Graph)) { return false; } Rows.Add(MakeShared<FJsonValueString>(Graph->GetPathName())); }
        Row->SetArrayField(TEXT("graphs"), Rows); Interfaces.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("interfaces"), Interfaces); TArray<UEdGraph*> Graphs; C.Blueprint->GetAllGraphs(Graphs);if(C.NameScope.IsValid()&&!C.NameScope->Check())return false;
    if (Graphs.Num() > 256 || (!C.DefaultEventPending&&!Graphs.Contains(C.Graph))) { return false; }
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<FGuid> GraphIds; int32 TotalPins = 0, TotalNodes = 0;
    for (const UEdGraph* Graph : Graphs)
    {
        if (!IsValid(Graph) || !Graph->GraphGuid.IsValid() || GraphIds.Contains(Graph->GraphGuid) || Graph->Nodes.Num() > 2048 || (TotalNodes += Graph->Nodes.Num()) > 8192) { return false; }
        GraphIds.Add(Graph->GraphGuid); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), Graph->GetPathName()); Row->SetStringField(TEXT("guid"), Graph->GraphGuid.ToString(EGuidFormats::Digits));
        Row->SetStringField(TEXT("name"), Graph->GetName()); Row->SetStringField(TEXT("type"), GraphKind(C.Blueprint, Graph)); TArray<TSharedPtr<FJsonValue>> Nodes; TSet<FGuid> NodeIds;
        const auto NodeCopies=TrackedArrayObservation(C)?Graph->Nodes:TArray<TObjectPtr<UEdGraphNode>>();const auto& NodeRows=TrackedArrayObservation(C)?NodeCopies:Graph->Nodes;
        for (const UEdGraphNode* Node : NodeRows)
        {
            if (!IsValid(Node) || !Node->NodeGuid.IsValid() || NodeIds.Contains(Node->NodeGuid) || Node->Pins.Num() > 512) { return false; } NodeIds.Add(Node->NodeGuid);
            TSet<FGuid> PinIds;
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (++TotalPins > 16384 || !Pin || Pin->WasTrashed() || !Pin->PinId.IsValid() || PinIds.Contains(Pin->PinId)) { return false; } PinIds.Add(Pin->PinId);
                for (const UEdGraphPin* Other : Pin->LinkedTo)
                { if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked()) || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(Pin))) { return false; } }
            }
            auto Observed=NodeState(Node,C.EnumScope.Get(),C.VectorScope.Get(),C.ObjectScope.Get(),C.NameScope.Get(),TrackedArrayObservation(C)&&C.ExtendedBound?&C:nullptr);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||(C.ExtendedBound&&!ExtendedCurrent(C,*C.ExtendedResult,C.ExtendedCreated.Get()))) { return false; }
            Nodes.Add(MakeShared<FJsonValueObject>(Observed));
        }
        Row->SetArrayField(TEXT("nodes"), Nodes); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("graphs"), Rows); return true;
}

// WT uses Capture's authored values, replacing only physical pin IDs/order with
// logical split-pin roles across AddUbergraphPage's skeleton-only compile.
// Pin defaults/types/links and actual split relationships remain compared.
bool DefaultEventState(FContext& C,FResult& R,const TSharedRef<FJsonObject>& Out,bool RemoveCreated)
{
    if(!Capture(C,Out)||!ExtendedCurrent(C,R,nullptr))return false;
    Out->RemoveField(TEXT("dirty"));Out->RemoveField(TEXT("blueprint_status"));
    TArray<TSharedPtr<FJsonValue>> GraphRows;int32 Removed=0;
    for(const auto& GraphValue:Out->GetArrayField(TEXT("graphs")))
    {
        auto Graph=GraphValue->AsObject();
        if(RemoveCreated&&Graph->GetStringField(TEXT("path"))==C.Graph->GetPathName()){++Removed;continue;}
        for(const auto& NodeValue:Graph->GetArrayField(TEXT("nodes")))
        {
            auto Node=NodeValue->AsObject();const auto Pins=Node->GetArrayField(TEXT("pins"));
            TMap<FString,FString> Keys;TMap<FString,TSharedPtr<FJsonObject>> ByKey;
            for(int32 Pass=0;Pass<=Pins.Num()&&Keys.Num()<Pins.Num();++Pass)
            {
                const int32 Prior=Keys.Num();
                for(const auto& V:Pins)
                {
                    auto P=V->AsObject();const FString Id=P->GetStringField(TEXT("pin_id"));if(Keys.Contains(Id))continue;
                    const FString Parent=P->GetStringField(TEXT("parent_pin_id"));if(!Parent.IsEmpty()&&!Keys.Contains(Parent))continue;
                    const FString Name=P->GetStringField(TEXT("name"));
                    const FString Key=(Parent.IsEmpty()?FString():Keys[Parent]+TEXT("/"))+P->GetStringField(TEXT("direction"))+TEXT(":")+LexToString(Name.Len())+TEXT(":")+Name;
                    if(ByKey.Contains(Key))return ExtendedStop(C,R,TEXT("default_event_pin_semantic_key"),TEXT("unambiguous logical split-pin roles"),Key);
                    Keys.Add(Id,Key);ByKey.Add(Key,P);
                }
                if(Keys.Num()==Prior)break;
            }
            if(Keys.Num()!=Pins.Num())return ExtendedStop(C,R,TEXT("default_event_pin_parentage"),TEXT("complete acyclic local split-pin roles"),TEXT("unresolved parent/duplicate ID"));
            TArray<FString> Ordered;ByKey.GetKeys(Ordered);Ordered.Sort();TArray<TSharedPtr<FJsonValue>> SemanticPins;
            for(const FString& Key:Ordered)
            {
                auto P=ByKey[Key];const FString Parent=P->GetStringField(TEXT("parent_pin_id"));
                TArray<FString> Children;for(const auto& Child:P->GetArrayField(TEXT("sub_pin_ids"))){const FString* K=Keys.Find(Child->AsString());if(!K)return ExtendedStop(C,R,TEXT("default_event_pin_children"),TEXT("node-owned logical child role"),Child->AsString());Children.Add(*K);}Children.Sort();
                TArray<TSharedPtr<FJsonValue>> Rows;for(const FString& Child:Children)Rows.Add(MakeShared<FJsonValueString>(Child));
                P->RemoveField(TEXT("pin_id"));P->SetStringField(TEXT("parent_pin_id"),Parent.IsEmpty()?FString():Keys[Parent]);P->SetArrayField(TEXT("sub_pin_ids"),Rows);
                SemanticPins.Add(MakeShared<FJsonValueObject>(P));
            }
            Node->SetArrayField(TEXT("pins"),SemanticPins);
        }
        GraphRows.Add(GraphValue);
    }
    Out->SetArrayField(TEXT("graphs"),GraphRows);
    if(RemoveCreated&&Removed!=1)return ExtendedStop(C,R,TEXT("default_event_graph_prefix"),TEXT("exactly one attached new EventGraph"),LexToString(Removed));
    return true;
}

bool DefaultEventDifference(FContext& C,FResult& R,const TSharedPtr<FJsonValue>& Expected,const TSharedPtr<FJsonValue>& Actual,const FString& Path)
{
    if(EnumEqual(Expected,Actual))return true;
    if(Expected.IsValid()&&Actual.IsValid()&&Expected->Type==Actual->Type)
    {
        if(Expected->Type==EJson::Object){const auto E=Expected->AsObject(),A=Actual->AsObject();for(const auto& Pair:E->Values){const auto* V=A->Values.Find(Pair.Key);const FString P=Path+TEXT(".")+FString(Pair.Key.ToView());if(!V)return ExtendedStop(C,R,*P,TEXT("present authored field"),TEXT("missing"));if(!DefaultEventDifference(C,R,Pair.Value,*V,P))return false;}if(E->Values.Num()!=A->Values.Num())return ExtendedStop(C,R,*Path,TEXT("same authored field inventory"),TEXT("additional field"));}
        else if(Expected->Type==EJson::Array){const auto& E=Expected->AsArray();const auto& A=Actual->AsArray();if(E.Num()!=A.Num())return ExtendedStop(C,R,*Path,LexToString(E.Num()),LexToString(A.Num()));for(int32 I=0;I<E.Num();++I)if(!DefaultEventDifference(C,R,E[I],A[I],Path+FString::Printf(TEXT("[%d]"),I)))return false;}
    }
    auto E=MakeShared<FJsonObject>(),A=MakeShared<FJsonObject>();E->SetField(TEXT("value"),Expected);A->SetField(TEXT("value"),Actual);return ExtendedStop(C,R,*Path,Text(E),Text(A));
}

bool BootstrapDefaultEventGraph(FContext& C,FResult& R)
{
    UBlueprint* BP=C.Blueprint;C.DefaultEventPending=true;
    C.DefaultEventEditor.Reset(GEditor);C.DefaultEventWorld.Reset(GEditor->GetEditorWorldContext().World());
    C.DefaultEventPackage.Reset(BP->GetPackage());
    if(!BP->ParentClass.GetGCPtr().IsResolved())return Error(R,TEXT("array_default_event_parent_unresolved"),TEXT("Default EventGraph bootstrap will not resolve a parent handle."));
    C.DefaultEventParent.Reset(BP->ParentClass.GetGCPtr().Get());C.ExtendedBP.Reset(BP);
    UClass* SchemaClass=UEdGraphSchema_K2::StaticClass();
    C.Schema=FullyLoaded(SchemaClass)?Cast<UEdGraphSchema_K2>(SchemaClass->GetDefaultObject(false)):nullptr;
    if(!FullyLoaded(C.Schema))return Error(R,TEXT("array_default_event_schema_unavailable"),TEXT("Default EventGraph creation requires the existing native K2 schema CDO."));
    C.DefaultEventSchema.Reset(const_cast<UEdGraphSchema_K2*>(C.Schema));
    const FString Path=BP->GetPathName();UObject* const Owner=BP->GetOuter();UClass* const Class=BP->GetClass();
    const auto UbergraphPages=BP->UbergraphPages;const auto FunctionGraphs=BP->FunctionGraphs;const auto MacroGraphs=BP->MacroGraphs;
    C.GraphCreationCurrent=[&C,&R,Path,Owner,Class,UbergraphPages,FunctionGraphs,MacroGraphs](UEdGraphNode*)
    {
        if(R.Data->HasField(TEXT("extended_primitive_followup_sdk_stopped")))return false;
        auto Bad=[&](const TCHAR* P,const FString& E,const FString& A){return ExtendedStop(C,R,P,E,A);};
        UBlueprint* B=C.ExtendedBP.Get();
        if(!EnumContext()||GEditor!=C.DefaultEventEditor.Get()||!EnumLoaded(C.DefaultEventWorld.Get())||GEditor->GetEditorWorldContext().World()!=C.DefaultEventWorld.Get()||C.DefaultEventWorld->bIsTearingDown)return Bad(TEXT("default_event_context"),TEXT("admitted Editor/world"),TEXT("lost or changed"));
        if(!EnumLoaded(B)||B!=C.Blueprint||B->GetClass()!=Class||B->GetOuter()!=Owner||B->GetPackage()!=C.DefaultEventPackage.Get()||B->GetPathName()!=Path||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()!=C.DefaultEventParent.Get()||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified)return Bad(TEXT("default_event_blueprint"),Path,TEXT("identity/parent/idle role changed"));
        if(!EnumLoaded(C.DefaultEventSchema.Get())||C.Schema!=C.DefaultEventSchema.Get()||C.Schema->GetClass()!=UEdGraphSchema_K2::StaticClass())return Bad(TEXT("default_event_schema"),TEXT("held native K2 schema CDO"),TEXT("different or retired"));
        auto SamePages=[&](const auto& Actual,const auto& Expected,bool WithNew)
        {
            if(Actual.Num()!=Expected.Num()+(WithNew?1:0))return false;
            for(int32 I=0;I<Expected.Num();++I)if(!Actual[I].IsResolved()||!Expected[I].IsResolved()||Actual[I]!=Expected[I])return false;
            return !WithNew||(Actual.Last().IsResolved()&&Actual.Last()==C.Graph);
        };
        if(!SamePages(B->UbergraphPages,UbergraphPages,C.DefaultEventAttached)||!SamePages(B->FunctionGraphs,FunctionGraphs,false)||!SamePages(B->MacroGraphs,MacroGraphs,false))return Bad(TEXT("default_event_graph_collections"),TEXT("original graph collections plus only the attached new page"),TEXT("different graph collection"));
        for(const auto& G:C.ExtendedGraphs)if(G.BootstrapClass){UEdGraph* Actual=G.Hold.Get();if(!EnumLoaded(Actual)||Actual->GetClass()!=G.BootstrapClass||Actual->GetOuter()!=G.BootstrapOuter||Actual->GetPackage()!=G.BootstrapPackage||Actual->GetPathName()!=G.BootstrapPath||Actual->GraphGuid!=G.BootstrapGuid||!Actual->Schema.GetGCPtr().IsResolved()||Actual->Schema.GetGCPtr()!=G.BootstrapSchema)return Bad(TEXT("default_event_prior_graph"),G.BootstrapPath,TEXT("class/owner/package/path/GUID/schema role changed"));}
        if(C.ExtendedGraph.IsValid()){UEdGraph* G=C.ExtendedGraph.Get();if(!EnumLoaded(G)||G!=C.Graph||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=B||G->GetPackage()!=C.DefaultEventPackage.Get()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=C.Schema->GetClass()||!G->GraphGuid.IsValid()||G->GetFName()!=FName(TEXT("EventGraph")))return Bad(TEXT("default_event_created_graph"),TEXT("held direct standard EventGraph/K2 page"),TEXT("different or retired"));}
        return true;
    };
    if(!ExtendedBind(C,R))return false;
    for(auto& G:C.ExtendedGraphs){UEdGraph* P=G.Hold.Get();if(!P->Schema.GetGCPtr().IsResolved())return ExtendedStop(C,R,TEXT("default_event_prior_schema"),TEXT("already resolved graph schema"),TEXT("unresolved"));G.BootstrapOuter=P->GetOuter();G.BootstrapPackage=P->GetPackage();G.BootstrapClass=P->GetClass();G.BootstrapSchema=P->Schema.GetGCPtr();G.BootstrapPath=P->GetPathName();G.BootstrapGuid=P->GraphGuid;}
    auto Before=MakeShared<FJsonObject>();if(!DefaultEventState(C,R,Before,false)){if(R.ErrorCode.IsEmpty())Error(R,TEXT("array_default_event_before_unobservable"),TEXT("Existing authored state could not be observed before default EventGraph creation."));return false;}
    R.Data->SetObjectField(TEXT("default_event_graph_before"),Before);
    auto Collision=[&](){return FindObject<UObject>(BP,TEXT("EventGraph"))!=nullptr;};
    if(Collision())return Error(R,TEXT("array_default_event_name_collision_unsupported"),TEXT("UE56 CreateNewGraph may rename a colliding object; that collateral rename remains incomplete and was not dispatched."));
    auto Resolution=MakeShared<FJsonObject>();Resolution->SetBoolField(TEXT("requested_create_if_missing"),C.CreateGraphIfMissing);Resolution->SetBoolField(TEXT("default_selector_bootstrap"),true);Resolution->SetBoolField(TEXT("graph_object_created"),false);Resolution->SetBoolField(TEXT("created"),false);Resolution->SetBoolField(TEXT("reused_existing"),false);Resolution->SetBoolField(TEXT("implicit_skeleton_compile_possible"),true);Resolution->SetBoolField(TEXT("explicit_compile_requested"),false);R.Data->SetObjectField(TEXT("graph_resolution"),Resolution);
    R.Stage=TEXT("default_event_blueprint_modify");R.bMutationAttempted=true;R.bChangeStateKnown=false;BP->Modify();Resolution->SetBoolField(TEXT("blueprint_modify_returned"),true);if(!ExtendedCurrent(C,R,nullptr))return false;Resolution->SetBoolField(TEXT("package_dirty_after_modify"),BP->GetPackage()->IsDirty());
    if(Collision())return Error(R,TEXT("array_default_event_name_collision_unsupported"),TEXT("A name collision appeared after Modify; CreateNewGraph and collateral rename were not dispatched."));
    R.Stage=TEXT("default_event_graph_create");R.bChangeStateKnown=false;
    UEdGraph* Returned=FBlueprintEditorUtils::CreateNewGraph(BP,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    // Context/source roles precede every inspection/hold of the returned UObject.
    if(!ExtendedCurrent(C,R,nullptr))return false;
    if(!EnumLoaded(Returned)||Returned->GetClass()!=UEdGraph::StaticClass()||Returned->GetOuter()!=BP||Returned->GetPackage()!=C.DefaultEventPackage.Get()||!Returned->Schema.GetGCPtr().IsResolved()||Returned->Schema.GetGCPtr()!=C.Schema->GetClass()||!Returned->GraphGuid.IsValid()||Returned->GetFName()!=FName(TEXT("EventGraph"))||!Returned->Nodes.IsEmpty())return ExtendedStop(C,R,TEXT("default_event_graph_return"),TEXT("new direct empty standard K2 EventGraph"),TEXT("null, unexpected owner/class/schema/name/nodes"));
    C.ExtendedGraph.Reset(Returned);C.Graph=Returned;R.bChanged=true;Resolution->SetBoolField(TEXT("graph_object_created"),true);Resolution->SetStringField(TEXT("graph_path"),Returned->GetPathName());Resolution->SetStringField(TEXT("graph_guid"),Returned->GraphGuid.ToString(EGuidFormats::Digits));R.Data->SetBoolField(TEXT("node_only_contract"),false);
    FContext::FExtendedGraph NewGraph;NewGraph.Hold.Reset(Returned);NewGraph.NodeCount=0;NewGraph.BootstrapOuter=BP;NewGraph.BootstrapPackage=Returned->GetPackage();NewGraph.BootstrapClass=Returned->GetClass();NewGraph.BootstrapSchema=Returned->Schema.GetGCPtr();NewGraph.BootstrapPath=Returned->GetPathName();NewGraph.BootstrapGuid=Returned->GraphGuid;C.ExtendedGraphs.Add(MoveTemp(NewGraph));
    if(!ExtendedCurrent(C,R,nullptr))return false;
    R.Stage=TEXT("default_event_graph_attach");C.DefaultEventAttached=true;C.DefaultEventPending=false;C.DefaultEventPinRefresh=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"),true);Resolution->SetBoolField(TEXT("add_ubergraph_page_dispatched"),true);
    FBlueprintEditorUtils::AddUbergraphPage(BP,Returned);
    Resolution->SetBoolField(TEXT("implicit_skeleton_compile_returned"),true);
    if(!ExtendedCurrent(C,R,nullptr))return false;
    Resolution->SetBoolField(TEXT("created"),true);
    // Skeleton reconstruction may replace physical pins. Adopt only live node-owned
    // pins, then compare the original logical authored state before any next writer.
    for(auto& Old:C.ExtendedOld){UEdGraphNode* N=Old.Hold.Get();if(N->Pins.Num()>512)return ExtendedStop(C,R,TEXT("default_event_reconstructed_pins"),TEXT("bounded live pin inventory"),TEXT("over bound"));Old.Pins.Reset();for(auto* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N)return ExtendedStop(C,R,TEXT("default_event_reconstructed_pin_owner"),TEXT("live node-owned physical pin"),TEXT("retired/different owner"));Old.Pins.Add({P,P->PinId,P->PinName,P->Direction,P->PinType});}}
    C.DefaultEventPinRefresh=false;if(!ExtendedCurrent(C,R,nullptr))return false;
    auto After=MakeShared<FJsonObject>();if(!DefaultEventState(C,R,After,true)){if(R.ErrorCode.IsEmpty())Error(R,TEXT("array_default_event_after_unobservable"),TEXT("Default EventGraph prefix is retained; post-skeleton authored state could not be fully observed."));return false;}
    R.Data->SetObjectField(TEXT("default_event_graph_after_without_created_page"),After);
    if(!DefaultEventDifference(C,R,MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After),TEXT("default_event_authored")))return false;
    Resolution->SetBoolField(TEXT("created"),true);Resolution->SetBoolField(TEXT("prior_authored_state_preserved"),true);Resolution->SetBoolField(TEXT("implicit_skeleton_compile_returned"),true);R.bChangeStateKnown=true;
    return true;
}

// New WT profile borrows the allocation return only after the parent observer
// and actual owner/class are current; existing unrelated allocation paths remain.
bool AdoptDefaultEventArrayNode(FContext& C,FResult& R,UK2Node_CallArrayFunction* Node)
{
    if(!ExtendedCurrent(C,R,nullptr))return false;
    if(!EnumLoaded(Node)||Node->GetClass()!=UK2Node_CallArrayFunction::StaticClass()||Node->GetOuter()!=C.Graph||Node->GetPackage()!=C.Blueprint->GetPackage())return ExtendedStop(C,R,TEXT("default_event_array_node_return"),TEXT("live exact Array node owned by selected new page"),TEXT("null/retired/different class or owner"));
    C.ExtendedCreated.Reset(Node);return ExtendedCurrent(C,R,Node);
}

bool PriorPreserved(const FContext& C, const TSharedRef<FJsonObject>& After, const UEdGraphNode* Created)
{
    if (!IsValid(Created) || Created->GetGraph() != C.Graph) { return false; }
    auto Expected = MakeShared<FJsonObject>(); auto Actual = MakeShared<FJsonObject>();
    for (const auto& Pair : C.Before->Values) { const FString Key(Pair.Key.ToView()); if (Key != TEXT("dirty") && Key != TEXT("blueprint_status")) { Expected->SetField(Key, Pair.Value); } }
    for (const auto& Pair : After->Values) { const FString Key(Pair.Key.ToView()); if (Key != TEXT("dirty") && Key != TEXT("blueprint_status")) { Actual->SetField(Key, Pair.Value); } }
    TArray<TSharedPtr<FJsonValue>> Graphs; int32 Removed = 0;
    for (const auto& Value : After->GetArrayField(TEXT("graphs")))
    {
        auto Graph = MakeShared<FJsonObject>(); const auto Original = Value->AsObject();
        for (const auto& Pair : Original->Values) { Graph->SetField(FString(Pair.Key.ToView()), Pair.Value); }
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (const auto& Node : Original->GetArrayField(TEXT("nodes")))
        {
            if (Node->AsObject()->GetStringField(TEXT("object_path")) == Created->GetPathName()) { ++Removed; continue; } Nodes.Add(Node);
        }
        Graph->SetArrayField(TEXT("nodes"), Nodes); Graphs.Add(MakeShared<FJsonValueObject>(Graph));
    }
    Actual->SetArrayField(TEXT("graphs"), Graphs); return Removed == 1 && Text(Expected) == Text(Actual);
}

bool NodeShape(const FContext& C, const UK2Node_CallArrayFunction* Node, bool bTyped)
{
    const bool Pure = (C.Operation->Flags & FUNC_BlueprintPure) != 0;
    if (!IsValid(Node) || Node->GetClass() != UK2Node_CallArrayFunction::StaticClass() || Node->GetGraph() != C.Graph || !C.Graph->Nodes.Contains(Node)
        || !Node->NodeGuid.IsValid() || Node->IsNodePure() != Pure || Node->Pins.Num() != C.Operation->Count + 1 + (Pure ? 0 : 2)
        || (C.NameScope.IsValid()?false:C.ObjectScope.IsValid() ? !Node->FunctionReference.IsSameReference(C.ObjectScope->Member) || Node->FunctionReference.IsDeprecated() : Node->FunctionReference.GetMemberParentClass() != C.Library) || Node->FunctionReference.GetMemberName() != FName(C.Operation->FunctionName)
        || Node->FunctionReference.GetMemberGuid().IsValid() || Node->FunctionReference.IsSelfContext() || Node->NodePosX != C.X || Node->NodePosY != C.Y) { return false; }
    const UEdGraphPin* Self = Node->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input); FEdGraphPinType SelfType; SelfType.PinCategory = UEdGraphSchema_K2::PC_Object; SelfType.PinSubCategoryObject = C.Library;
    if (!Self || !SameType(Self->PinType, SelfType) || !Self->bHidden || Self->DefaultObject != C.LibraryCDO || !Self->LinkedTo.IsEmpty()) { return false; }
    for (int32 Index = 0; Index < C.Operation->Count; ++Index)
    {
        const FParameter& Parameter = C.Operation->Parameters[Index]; const EEdGraphPinDirection Direction = Input(Parameter) ? EGPD_Input : EGPD_Output;
        const UEdGraphPin* Pin = Node->FindPin(Parameter.Name, Direction); const FEdGraphPinType Type = ParameterType(Parameter, bTyped ? C.ElementCategory : NAME_None, bTyped ? C.ElementObject : nullptr, bTyped ? C.ElementSubCategory : NAME_None);
        const bool IgnoreDefault = ((Parameter.Flags & (CPF_ConstParm | CPF_ReferenceParm)) == (CPF_ConstParm | CPF_ReferenceParm)
            && (FString(C.Operation->RefTerm).IsEmpty() || Type.IsContainer())) || (Parameter.Flags & CPF_RequiredParm) != 0;
        if (!Pin || Pin->WasTrashed() || !SameType(Pin->PinType, Type) || Pin->bHidden || Pin->bNotConnectable || Pin->bDefaultValueIsReadOnly
            || Pin->bDefaultValueIsIgnored != IgnoreDefault || Pin->bAdvancedView != ((Parameter.Flags & CPF_AdvancedDisplay) != 0)
            || (C.DescriptorOperation ? (!Pin->DefaultObject.IsResolved()||Pin->DefaultObject!=C.DescriptorAppliedObjects.FindRef(FName(Parameter.Name))) : Pin->DefaultObject!=nullptr) || (Type.PinCategory==UEdGraphSchema_K2::PC_Text&&!Type.IsContainer()? !Pin->DefaultValue.IsEmpty() : !Pin->DefaultTextValue.IsEmpty())) { return false; }
    }
    if (!Pure)
    {
        FEdGraphPinType ExecType; ExecType.PinCategory = UEdGraphSchema_K2::PC_Exec;
        const UEdGraphPin* Execute = Node->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input); const UEdGraphPin* Then = Node->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
        if (!Execute || !Then || !SameType(Execute->PinType, ExecType) || !SameType(Then->PinType, ExecType)) { return false; }
    }
    for (const UEdGraphPin* Pin : Node->Pins)
    { if (!Pin || Pin->WasTrashed() || Pin->GetOwningNodeUnchecked() != Node || Pin->bOrphanedPin || !Pin->LinkedTo.IsEmpty() || Pin->ParentPin || !Pin->SubPins.IsEmpty()) { return false; } }
    return true;
}

// OS: new descriptor defaults keep actual physical ref/const and source roles intact.
bool DescriptorDefaultVerified(FContext& C,FResult& R,UK2Node_CallArrayFunction* Node,const FDefault& Request)
{
    if(!ExtendedCurrent(C,R,Node))return false;const UEdGraphPin* Physical=Node->FindPin(Request.PinName,EGPD_Input);
    UObject* ExpectedObject=C.DescriptorDefaultObjects.FindRef(Request.PinName);
    if(!Physical||Physical->WasTrashed()||Physical->bDefaultValueIsIgnored||Physical->bDefaultValueIsReadOnly||!Physical->DefaultObject.IsResolved()||Physical->DefaultObject!=ExpectedObject||!Physical->DefaultTextValue.IsEmpty())return Error(R,TEXT("array_descriptor_default_readback_mismatch"),TEXT("Descriptor default object/text role differs; retained node and actual prefix are not rolled back."));
    const bool Match=ExpectedObject?Physical->DefaultValue.IsEmpty():C.Schema->DoesDefaultValueMatch(*Physical,Request.Value);
    if(!ExtendedCurrent(C,R,Node))return false;Physical=Node->FindPin(Request.PinName,EGPD_Input);
    const FString Validation=C.Schema->IsCurrentPinDefaultValid(Physical);if(!ExtendedCurrent(C,R,Node))return false;
    return Match&&Validation.IsEmpty()?true:Error(R,TEXT("array_descriptor_default_readback_mismatch"),TEXT("SDK descriptor default match/current validation failed; no rollback or retry."));
}
bool ApplyDescriptorDefault(FContext& C,FResult& R,UK2Node_CallArrayFunction* Node,const FDefault& Request,const TSharedRef<FJsonObject>& Applied,const TSharedRef<FJsonObject>& Observations,TArray<TSharedPtr<FJsonValue>>& Prefixes)
{
    if(!ExtendedCurrent(C,R,Node))return false;UEdGraphPin* Physical=Node->FindPin(Request.PinName,EGPD_Input);
    auto Prefix=MakeShared<FJsonObject>();Prefix->SetStringField(TEXT("requested_pin"),Request.RequestedName);Prefix->SetStringField(TEXT("requested_literal"),Request.Value);for(const TCHAR* Flag:{TEXT("validation_attempted"),TEXT("validation_returned"),TEXT("setter_attempted"),TEXT("setter_returned"),TEXT("explicit_notify_attempted"),TEXT("explicit_notify_returned"),TEXT("verified")})Prefix->SetBoolField(Flag,false);Prefixes.Add(MakeShared<FJsonValueObject>(Prefix));R.Data->SetArrayField(TEXT("descriptor_array_default_prefix"),Prefixes);
    if(!Physical||Physical->bDefaultValueIsIgnored||Physical->bDefaultValueIsReadOnly)return Error(R,TEXT("array_engine_default_rejected"),TEXT("Selected descriptor input is absent or ignores defaults."));
    UObject* ExpectedObject=C.DescriptorDefaultObjects.FindRef(Request.PinName);const bool ObjectSetter=ExpectedObject!=nullptr;const FString ExpectedRaw=ObjectSetter?FString():Request.Value;
    Prefix->SetStringField(TEXT("pin_id"),Physical->PinId.ToString(EGuidFormats::Digits));Prefix->SetStringField(TEXT("setter_kind"),ObjectSetter?TEXT("TrySetDefaultObject"):TEXT("TrySetDefaultValue"));Prefix->SetBoolField(TEXT("validation_attempted"),true);
    const FString Validation=C.Schema->IsPinDefaultValid(Physical,ExpectedRaw,ExpectedObject,FText());Prefix->SetBoolField(TEXT("validation_returned"),true);
    if(!ExtendedCurrent(C,R,Node))return false;Physical=Node->FindPin(Request.PinName,EGPD_Input);
    if(!Validation.IsEmpty())return Error(R,TEXT("array_engine_default_rejected"),TEXT("SDK rejected the descriptor input default before its setter; node and earlier defaults remain."));
    if(!C.ObjectScope->ExpectDescriptorDefault(Physical,ExpectedRaw,ExpectedObject))return false;
    Prefix->SetBoolField(TEXT("setter_attempted"),true);
    if(ObjectSetter)C.Schema->TrySetDefaultObject(*Physical,ExpectedObject,false);else C.Schema->TrySetDefaultValue(*Physical,Request.Value,false);
    Prefix->SetBoolField(TEXT("setter_returned"),true);if(!ExtendedCurrent(C,R,Node))return false;Physical=Node->FindPin(Request.PinName,EGPD_Input);
    if(ObjectSetter)C.DescriptorAppliedObjects.Add(Request.PinName,ExpectedObject);
    const FString RawAfter=Physical->DefaultValue;const FString ObjectAfter=ExpectedObject?ExpectedObject->GetPathName():FString();const FString ActualApplied=ObjectSetter?ObjectAfter:RawAfter;
    Applied->SetStringField(Request.RequestedName,ActualApplied);Prefix->SetStringField(TEXT("actual_applied_after_set"),ActualApplied);Prefix->SetStringField(TEXT("actual_raw_after_set"),RawAfter);Prefix->SetStringField(TEXT("actual_object_after_set"),ObjectAfter);
    Prefix->SetBoolField(TEXT("explicit_notify_attempted"),true);Node->PinDefaultValueChanged(Physical);Prefix->SetBoolField(TEXT("explicit_notify_returned"),true);if(!ExtendedCurrent(C,R,Node))return false;
    if(!DescriptorDefaultVerified(C,R,Node,Request))return false;Physical=Node->FindPin(Request.PinName,EGPD_Input);
    const FString Raw=Physical->DefaultValue;const FText TextCopy=Physical->DefaultTextValue;const FGuid PinId=Physical->PinId;const auto TypeCopy=Physical->PinType;const FString Auto=Physical->AutogeneratedDefaultValue;
    const FString Display=TextCopy.ToString();if(!ExtendedCurrent(C,R,Node))return false;Physical=Node->FindPin(Request.PinName,EGPD_Input);const FString AsString=Physical->GetDefaultAsString();if(!ExtendedCurrent(C,R,Node))return false;
    auto Observation=MakeShared<FJsonObject>();Observation->SetStringField(TEXT("requested_literal"),Request.Value);Observation->SetStringField(TEXT("schema_literal"),ExpectedRaw);Observation->SetStringField(TEXT("raw_default"),Raw);Observation->SetStringField(TEXT("default_object"),ObjectAfter);Observation->SetStringField(TEXT("default_text"),Display);Observation->SetStringField(TEXT("default_as_string"),AsString);Observation->SetStringField(TEXT("autogenerated_default_value"),Auto);Observation->SetStringField(TEXT("applied_kind"),ObjectSetter?TEXT("object_path"):TEXT("raw_default"));Observation->SetStringField(TEXT("applied_value"),ActualApplied);Observation->SetStringField(TEXT("pin_id"),PinId.ToString(EGuidFormats::Digits));Observation->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(TypeCopy));Observation->SetBoolField(TEXT("schema_match"),true);Observation->SetBoolField(TEXT("current_default_valid"),true);Observations->SetObjectField(Request.RequestedName,Observation);Prefix->SetBoolField(TEXT("verified"),true);return true;
}
void Finish(FContext& C, FResult& R, UK2Node_CallArrayFunction* Node, bool Good)
{
    if (!R.ErrorCode.IsEmpty()) { R.Data->SetStringField(TEXT("failed_mutation_stage"), R.Stage); }
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; }
    R.Stage = TEXT("verification"); auto After = MakeShared<FJsonObject>(); const bool Known = Capture(C, After);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; }
    R.Data->SetObjectField(TEXT("after"), After); R.bChangeStateKnown = Known; R.bChanged = (C.GraphCreationCurrent&&R.bChanged) || Known && Text(C.Before) != Text(After);
    const bool NativeReadable = ExactNativeClass(C.Library, TEXT("/Script/Engine.KismetArrayLibrary")) && FullyLoaded(C.Function)
        && FullyLoaded(C.LibraryCDO) && C.Function->GetOuterUClass() == C.Library;
    bool NativePreserved = false;
    if (NativeReadable)
    { const auto Observed = NativeState(C);if(C.NameScope.IsValid()&&!C.NameScope->Check())return;if((C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; } R.Data->SetObjectField(TEXT("native_signature_after"), Observed); NativePreserved = Text(Observed) == Text(C.NativeBefore); if (!NativePreserved) { R.bChanged = true; } }
    else { R.bChangeStateKnown = false; }
    bool VectorPreserved = true;
    if (C.ElementObject && !C.EnumScope.IsValid() && !C.ObjectScope.IsValid())
    {
        const auto Observed = VectorState(Cast<UScriptStruct>(C.ElementObject));if((C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; }
        R.Data->SetObjectField(TEXT("native_vector_source_after"), Observed);
        const bool Readable = Observed->GetBoolField(TEXT("observations_complete"));
        VectorPreserved = Readable && Observed->GetBoolField(TEXT("native_vector_identity_verified")) && Text(Observed) == Text(C.VectorBefore);
        if (!Readable) { R.bChangeStateKnown = false; }
        else if (!VectorPreserved) { R.bChanged = true; }
        R.Data->SetBoolField(TEXT("native_vector_source_observed_unchanged"), VectorPreserved);
    }
    const bool Prior = Known && PriorPreserved(C, After, Node); R.Data->SetBoolField(TEXT("existing_nodes_preserved"), Prior);
    bool Declarations = false;
    if (Known)
    {
        auto Expected = MakeShared<FJsonObject>(); auto Actual = MakeShared<FJsonObject>();
        for (const TCHAR* Key : {TEXT("variables"), TEXT("interfaces")})
        { Expected->SetArrayField(Key, C.Before->GetArrayField(Key)); Actual->SetArrayField(Key, After->GetArrayField(Key)); }
        Declarations = Text(Expected) == Text(Actual);
    }
    R.Data->SetBoolField(TEXT("declarations_preserved"), Declarations);
    R.Data->SetBoolField(TEXT("native_source_observed_unchanged"), NativePreserved);
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("all bounded owned graph/node/full pin/default/link identities, variable/interface declarations, dirty/status and selected native function/package; arbitrary Editor callback state and disk writes are not fully observed"));
    R.Data->SetBoolField(TEXT("created"), IsValid(Node) && C.Graph->Nodes.Contains(Node));
    if (IsValid(Node))
    {
        auto State=NodeState(Node,C.EnumScope.Get(),C.VectorScope.Get(),C.ObjectScope.Get(),C.NameScope.Get(),TrackedArrayObservation(C)&&C.ExtendedBound?&C:nullptr);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; }
        const FString Title=Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; }
        State->SetStringField(TEXT("title"),Title);R.Data->SetObjectField(TEXT("node"),State);
        for (const TCHAR* Key : {TEXT("node_id"), TEXT("node_guid"), TEXT("object_path"), TEXT("node_class")}) { R.Data->SetStringField(Key, State->GetStringField(Key)); }
        R.Data->SetObjectField(TEXT("graph"), GraphDescription(C));
    }
    bool DefaultsVerified = IsValid(Node);
    if (DefaultsVerified && (!(C.DescriptorOperation||C.UdsOperation)||Good))
    {
        for (const FDefault& Default : C.Defaults)
        {
            if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; }
            if(C.DescriptorOperation){if(!DescriptorDefaultVerified(C,R,Node,Default)){if(!R.bChangeStateKnown)return;DefaultsVerified=false;break;}continue;}
            const UEdGraphPin* Pin=Node->FindPin(Default.PinName,EGPD_Input);
            if(!Pin||Pin->WasTrashed()||Pin->bDefaultValueIsIgnored||Pin->bDefaultValueIsReadOnly||Pin->DefaultObject||(Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Text&&!Pin->PinType.IsContainer()?!Pin->DefaultValue.IsEmpty():!Pin->DefaultTextValue.IsEmpty())) { DefaultsVerified=false;break; }
            const bool Match=C.Schema->DoesDefaultValueMatch(*Pin,C.ExtendedSchemaDefaults.Contains(Default.PinName)?C.ExtendedSchemaDefaults.FindChecked(Default.PinName):Default.Value);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; }Pin=Node->FindPin(Default.PinName,EGPD_Input);
            const FString Validation=C.Schema->IsCurrentPinDefaultValid(Pin);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return; }
            DefaultsVerified &= Match&&Validation.IsEmpty();
            if(C.bVector4&&C.Vector4LiteralValues.Contains(Default.PinName))
            {
                if(!DefaultsVerified)break;
                if(!C.ObjectScope->Check())return;
                Pin=Node->FindPin(Default.PinName,EGPD_Input);if(!Pin){C.ObjectScope->Bad(TEXT("vector4_literal_readback_pin_missing"));return;}
                const FString Actual=Pin->DefaultValue;const FVector4 ExpectedValue=C.Vector4LiteralValues.FindChecked(Default.PinName);FVector4 ParsedValue;
                if(!Vector4Literal(*C.ObjectScope,Actual,ParsedValue,R))return;
                if(ParsedValue!=ExpectedValue){Error(R,TEXT("array_vector4_literal_readback_mismatch"),TEXT("Actual current item pin literal differs from its prewriter imported XYZW value."));return;}
                auto Proof=MakeShared<FJsonObject>();Proof->SetStringField(TEXT("raw_default"),Actual);Proof->SetBoolField(TEXT("import_fully_consumed"),true);Proof->SetBoolField(TEXT("values_match_preflight"),true);
                for(int32 I=0;I<4;++I)Proof->SetNumberField(I==0?TEXT("X"):I==1?TEXT("Y"):I==2?TEXT("Z"):TEXT("W"),ParsedValue[I]);
                R.Data->SetObjectField(FString(TEXT("vector4_literal_readback_"))+Default.RequestedName,Proof);
            }
        }
    }
    if((C.DescriptorOperation||C.UdsOperation)&&!Good)DefaultsVerified=false;
    R.Data->SetBoolField(TEXT("requested_defaults_verified"), DefaultsVerified);
    bool ObjectParityPreserved=true;
    if(C.ObjectParity){if(!C.ObjectScope->Check()||!ExtendedCurrent(C,R,Node))return;const auto SourceAfter=C.ObjectScope->ParityState();R.Data->SetObjectField(TEXT("object_array_operation_source_after"),SourceAfter);ObjectParityPreserved=Text(SourceAfter)==Text(R.Data->GetObjectField(TEXT("object_array_operation_source_before")).ToSharedRef());R.Data->SetBoolField(TEXT("object_array_operation_source_observed_unchanged"),ObjectParityPreserved);}
    bool Verified = Good && Known && R.bChangeStateKnown && Prior && Declarations && NativePreserved && VectorPreserved && DefaultsVerified && ObjectParityPreserved && NodeShape(C, Node, true);
    if (C.ElementObject && !C.EnumScope.IsValid() && !C.ObjectScope.IsValid()) { R.Data->SetBoolField(TEXT("native_vector_pin_shape_verified"), Verified); }
    if(C.bVector4)
    {
        if(!C.ObjectScope->Check())return;R.Data->SetObjectField(TEXT("native_vector4_source_after"),C.ObjectScope->Vector4State());
        R.Data->SetBoolField(TEXT("native_vector4_source_observed_unchanged"),true);R.Data->SetBoolField(TEXT("native_vector4_pin_shape_verified"),Verified);
    }
    if(C.StructLength)
    {
        if(!C.ObjectScope->Check())return;
        const auto AfterSource=C.ObjectScope->StructLengthState();R.Data->SetObjectField(C.StructOperation?TEXT("native_value_struct_array_operation_source_after"):TEXT("native_value_struct_array_length_source_after"),AfterSource);
        const bool SourceSame=Text(R.Data->GetObjectField(C.StructOperation?TEXT("native_value_struct_array_operation_source_before"):TEXT("native_value_struct_array_length_source_before")).ToSharedRef())==Text(AfterSource);R.Data->SetBoolField(C.StructOperation?TEXT("value_struct_array_operation_source_preserved"):TEXT("value_struct_array_length_source_preserved"),SourceSame);Verified&=SourceSame;
        if(C.StructOperation)R.Data->SetBoolField(TEXT("value_struct_array_operation_parity_verified"),Verified);
    }
    if(C.UdsLength)
    {
        if(!C.ObjectScope->Check()||!C.ObjectScope->UdsAuthoredQuery(false)||!C.ObjectScope->UdsFileQuery(false))return;
        const auto AfterSource=C.ObjectScope->UdsState();R.Data->SetObjectField(C.UdsOperation?TEXT("native_scalar_uds_array_operation_source_after"):TEXT("native_scalar_uds_array_length_source_after"),AfterSource);
        const bool SourceSame=Text(R.Data->GetObjectField(C.UdsOperation?TEXT("native_scalar_uds_array_operation_source_before"):TEXT("native_scalar_uds_array_length_source_before")).ToSharedRef())==Text(AfterSource);R.Data->SetBoolField(C.UdsOperation?TEXT("scalar_uds_array_operation_source_preserved"):TEXT("scalar_uds_array_length_source_preserved"),SourceSame);Verified&=SourceSame;R.Data->SetBoolField(C.UdsOperation?TEXT("scalar_uds_array_operation_pin_shape_verified"):TEXT("scalar_uds_array_length_pin_shape_verified"),Verified);if(C.UdsOperation)R.Data->SetBoolField(TEXT("scalar_uds_array_operation_parity_verified"),Verified);
    }
    if(C.DescriptorLength)
    {
        if(!C.ObjectScope->Check())return;
        const auto AfterSource=C.ObjectScope->NativeLengthState();R.Data->SetObjectField(C.DescriptorOperation?TEXT("native_descriptor_array_operation_source_after"):TEXT("native_descriptor_array_length_source_after"),AfterSource);
        const bool SourceSame=Text(R.Data->GetObjectField(C.DescriptorOperation?TEXT("native_descriptor_array_operation_source_before"):TEXT("native_descriptor_array_length_source_before")).ToSharedRef())==Text(AfterSource);
        R.Data->SetBoolField(C.DescriptorOperation?TEXT("descriptor_array_operation_source_preserved"):TEXT("descriptor_array_length_source_preserved"),SourceSame);Verified&=SourceSame;
        if(C.DescriptorOperation){auto Sources=MakeShared<FJsonObject>();for(const auto& Pair:C.DescriptorDefaultSources){if(!ExtendedCurrent(C,R,Node))return;Sources->SetObjectField(Pair.Key.ToString(),Pair.Value->Scope->NativeLengthState());}R.Data->SetObjectField(TEXT("descriptor_array_default_sources_after"),Sources);const bool DefaultsSame=Text(Sources)==Text(R.Data->GetObjectField(TEXT("descriptor_array_default_sources_before")).ToSharedRef());R.Data->SetBoolField(TEXT("descriptor_array_default_sources_preserved"),DefaultsSame);Verified&=DefaultsSame;R.Data->SetBoolField(TEXT("descriptor_array_operation_parity_verified"),Verified);}
    }
    if(C.ObjectParity)R.Data->SetBoolField(TEXT("object_array_operation_parity_verified"),Verified);
    R.Data->SetBoolField(TEXT("typed_node_shape_verified"), Verified); R.Data->SetBoolField(TEXT("node_only_verified"), Verified);
    if (!Verified)
    { if (R.ErrorCode.IsEmpty()) { Error(R, TEXT("array_node_postcondition_failed"), TEXT("Created node shape/defaults or bounded prior-state preservation failed; retained partial state is reported without rollback/retry.")); } return; }
    if(C.NameScope.IsValid()){auto Context=C.NameScope->Context();if(!Context.IsValid()||!C.NameScope->Check())return;R.Data->SetObjectField(TEXT("bfl_name_array_context_after"),Context);R.Data->SetBoolField(TEXT("name_array_target_current_verified"),true);}
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete");
}
}

Function::FResult AddNode(const FString& BlueprintPath, const FString& RequestedOperation, const FString& ElementType,
    const FString& ParamDefaultsJson, const FString& TypeObject, const FString& EnumType, const FString& NodePositionJson,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing)
{
    FResult R; FContext C; const FEngineVersion& Version = FEngineVersion::Current();
    R.Data->SetBoolField(TEXT("node_only_contract"), !bCreateGraphIfMissing); R.Data->SetBoolField(TEXT("implicit_data_wiring"), false);
    R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), false); R.Data->SetBoolField(TEXT("full_legacy_array_parity"), false);
    R.Data->SetStringField(TEXT("requested_operation"), RequestedOperation); R.Data->SetStringField(TEXT("requested_element_type"), ElementType);
    R.Data->SetStringField(TEXT("operation"), Normalize(RequestedOperation));
    R.Data->SetStringField(TEXT("requested_graph_name"), GraphName); R.Data->SetStringField(TEXT("requested_graph_id"), GraphId); R.Data->SetStringField(TEXT("requested_graph_type"), GraphType);
    R.Data->SetBoolField(TEXT("create_graph_if_missing"), bCreateGraphIfMissing);
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Error(R, TEXT("array_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); return R; }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8 || Version.GetPatch() != 3) { Error(R, TEXT("array_version_unsupported"), TEXT("Only installed UE 5.8.3 is implemented.")); return R; }
    if (!Arguments(C, R, RequestedOperation, ElementType, ParamDefaultsJson, TypeObject, EnumType, NodePositionJson, bCreateGraphIfMissing)) { return R; }
    R.Data->SetStringField(TEXT("canonical_operation"), C.Operation->Name); R.Data->SetStringField(TEXT("element_type"), C.bVector4?TEXT("struct"):C.UdsLength?TEXT("struct"):C.StructLength?C.StructLengthKind:C.DescriptorLength?(C.ElementCategory==UEdGraphSchema_K2::PC_Class?TEXT("class"):TEXT("object")):C.ObjectScope.IsValid()?TEXT("object"):C.EnumScope.IsValid()?TEXT("enum"):C.ElementObject ? TEXT("vector") : C.ElementCategory == UEdGraphSchema_K2::PC_Int ? TEXT("int") : C.ElementCategory == UEdGraphSchema_K2::PC_Int64 ? TEXT("int64") : C.ElementCategory == UEdGraphSchema_K2::PC_Real ? (C.ElementSubCategory==UEdGraphSchema_K2::PC_Float?TEXT("float"):TEXT("double")) : C.ElementCategory==UEdGraphSchema_K2::PC_Text?TEXT("text"): C.ElementCategory == UEdGraphSchema_K2::PC_Name ? TEXT("name") : C.ElementCategory == UEdGraphSchema_K2::PC_String ? TEXT("string") : TEXT("bool"));
    FEdGraphPinType Element; Element.PinCategory = C.ElementCategory; Element.PinSubCategory = C.ElementSubCategory; Element.PinSubCategoryObject = C.ElementObject; R.Data->SetObjectField(TEXT("element_pin_type"), Function::Pin::Describe(Element));
    if (C.ElementObject && !C.EnumScope.IsValid() && !C.ObjectScope.IsValid())
    {
        R.Data->SetBoolField(TEXT("native_vector_array"), true);
        R.Data->SetBoolField(TEXT("vector_target_array_requires_connection"), true);
        R.Data->SetBoolField(TEXT("vector_new_item_requires_connection"), false);
        R.Data->SetBoolField(TEXT("vector_literal_defaults_supported"), true);
        R.Data->SetBoolField(TEXT("vector_index_requires_connection"), false);
        R.Data->SetBoolField(TEXT("vector_get_is_value_copy"), FString(C.Operation->Name) == TEXT("get"));
        R.Data->SetBoolField(TEXT("vector_runtime_invocation_verified"), false);
    }
    if(C.bVector4)
    {
        R.Data->SetBoolField(TEXT("native_vector4_array"),true);R.Data->SetBoolField(TEXT("vector4_target_array_requires_connection"),true);R.Data->SetBoolField(TEXT("vector4_new_item_requires_connection"),false);
        R.Data->SetBoolField(TEXT("vector4_literal_defaults_supported"),true);R.Data->SetStringField(TEXT("vector4_literal_scope"),TEXT("Six existing item inputs: complete finite XYZW strings, native scratch ImportText/current and actual pin value readback; no array/null/JSON conversion or general struct support."));R.Data->SetBoolField(TEXT("vector4_runtime_invocation_verified"),false);
    }
    if(ExtendedPrimitive(C)){R.Data->SetBoolField(TEXT("native_extended_primitive_array"),true);R.Data->SetBoolField(TEXT("extended_primitive_runtime_invocation_verified"),false);R.Data->SetStringField(TEXT("int64_numeric_precision_scope"),TEXT("Original JSON number is double then safely truncated within int64 range; exact full int64 digits require a string. No upstream precision recovery is claimed."));}
    R.Data->SetStringField(TEXT("function_name"), C.Operation->FunctionName); R.Data->SetStringField(TEXT("function_class"), TEXT("/Script/Engine.KismetArrayLibrary"));
    if (!ResolveTarget(C, R, BlueprintPath, GraphName, GraphId, GraphType)) { return R; }
    if(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())return R;
    if(C.bVector4&&!C.ObjectScope->BindVector4Target(C.Blueprint,C.Graph,C.Schema,C.X,C.Y))return R;
    if((C.ObjectParity||C.UdsLength)&&!C.ObjectScope->BindParityTarget(C.Blueprint,C.Graph,C.Schema,C.X,C.Y))return R;
    if(!ExtendedBind(C,R))return R;
    TSharedPtr<FJsonObject> UdsTargetBefore;
    if(C.UdsLength)
    {
        C.ObjectScope->UdsPhysicalCurrent=[&C,&R](){return ExtendedCurrentPlain(C,R,C.ExtendedCreated.Get());};
        UdsTargetBefore=MakeShared<FJsonObject>();if(!Capture(C,UdsTargetBefore.ToSharedRef())||!ExtendedCurrent(C,R,nullptr))return R;
        if(!C.ObjectScope->BindUdsSource())return R;
    }
    if(!Native(C,R))return R;
    if(C.ObjectScope.IsValid()&&!C.ObjectScope->Bind(C.Blueprint,C.Graph,C.Schema,C.Library,C.Function,C.LibraryCDO,C.X,C.Y))return R;
    if(C.UdsLength)
    {
        auto AfterSourceQueries=MakeShared<FJsonObject>();if(!Capture(C,AfterSourceQueries)||!ExtendedCurrent(C,R,nullptr))return R;
        if(Text(UdsTargetBefore.ToSharedRef())!=Text(AfterSourceQueries)){C.ObjectScope->Bad(TEXT("scalar_uds_length_target_changed_during_source_queries"));return R;}
        R.Data->SetBoolField(C.UdsOperation?TEXT("scalar_uds_array_operation_target_source_queries_preserved"):TEXT("scalar_uds_length_target_source_queries_preserved"),true);
    }
    if(C.ObjectParity){if(!C.ObjectScope->Check())return R;R.Data->SetObjectField(TEXT("object_array_operation_source_before"),C.ObjectScope->ParityState());
        for(int32 I=0;I<C.Operation->Count;++I){const auto& P=C.Operation->Parameters[I];if(Input(P)&&FString(P.Category)==TEXT("wildcard")&&FString(P.Class)!=TEXT("ArrayProperty"))R.Data->SetStringField(TEXT("object_array_item_input_name"),P.Name);}}
    if(C.EnumScope.IsValid())
    {
        if(!C.EnumScope->SetTarget(C.Blueprint,C.Graph,C.Schema)) { return R; }
        C.EnumScope->FunctionOwner.Reset(C.Library);C.EnumScope->NativeFunction.Reset(C.Function);C.EnumScope->FunctionCDO.Reset(C.LibraryCDO);if(!C.EnumScope->Check()) { return R; }
    }
    const FString CanonicalVectorOperation(C.Operation->Name);
    if(C.ElementCategory==UEdGraphSchema_K2::PC_Struct&&!C.bVector4&&!C.StructLength&&!C.UdsLength&&(!C.Defaults.IsEmpty()||(CanonicalVectorOperation!=TEXT("clear")&&CanonicalVectorOperation!=TEXT("add")&&CanonicalVectorOperation!=TEXT("get"))))
    {
        C.VectorScope=MakeShared<FVectorArrayScope>();if(!C.VectorScope->Init(C.Blueprint,C.Graph,C.Schema,Cast<UScriptStruct>(C.ElementObject),R)) { return R; }
        C.VectorScope->FunctionOwner.Reset(C.Library);C.VectorScope->Function.Reset(C.Function);C.VectorScope->FunctionDefault.Reset(C.LibraryCDO);
    }
    if (!Capture(C, C.Before)) { if((C.NameScope.IsValid())||(C.EnumScope.IsValid()&&C.EnumScope->Stopped)||(C.VectorScope.IsValid()&&C.VectorScope->Stopped)||(C.ObjectScope.IsValid()&&C.ObjectScope->Stopped)) { return R; }Error(R, TEXT("array_before_state_unobservable"), TEXT("Bounded existing graph/declaration/pin observation was incomplete; no mutation dispatched.")); return R; }
    R.Data->SetObjectField(TEXT("before"), C.Before); R.Data->SetStringField(TEXT("function_path"), C.Function->GetPathName());
    TStrongObjectPtr<UBlueprint> KeepAlive(C.Blueprint);
    UK2Node_CallArrayFunction* Node=nullptr;
    R.Stage=TEXT("create_array_node");R.bMutationAttempted=true;
    C.Blueprint->Modify();if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }
    C.Graph->Modify();if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }
    if(C.NameScope.IsValid())
    {
        R.Data->SetNumberField(TEXT("node_allocation_calls"),1);
        Node=NewObject<UK2Node_CallArrayFunction>(C.Graph,NAME_None,RF_Transactional);
        R.Data->SetNumberField(TEXT("node_allocation_returns"),1);
        if(!C.NameScope->Adopt(Node)||!C.NameScope->Place(C.X,C.Y,-1))return R;
        if(!C.NameScope->Attach())return R;
        R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);C.Graph->AddNode(Node,false,false);
        R.Data->SetNumberField(TEXT("graph_add_node_returns"),1);R.bChanged=true;if(!C.NameScope->Check())return R;
        Node->SetFromFunction(C.Function);if(!C.NameScope->Check())return R;
        Node->CreateNewGuid();if(!C.NameScope->FixGuid())return R;
        Node->PostPlacedNewNode();if(!C.NameScope->Check())return R;
        if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!C.NameScope->Check())return R;}
        if(!C.NameScope->Freeze())return R;
    }
    else if(C.ObjectScope.IsValid())
    {
        R.Data->SetNumberField(TEXT("object_array_node_allocation_calls"),1);
        Node=NewObject<UK2Node_CallArrayFunction>(C.Graph,NAME_None,RF_Transactional);
        if(C.DefaultEventAttached){if(!AdoptDefaultEventArrayNode(C,R,Node))return R;}
        else if(C.ObjectParity||C.DescriptorLength||C.StructLength||C.UdsLength){C.ExtendedCreated.Reset(Node);if(!ExtendedCurrent(C,R,Node))return R;}
        if(!C.ObjectScope->Adopt(Node)||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node)))return R;
        if(C.Graph->HasAnyFlags(RF_Transient)){Node->SetFlags(RF_Transient);C.ObjectScope->NodeFlags=Node->GetFlags();}
        if(C.DefaultEventAttached||C.ObjectParity||C.DescriptorLength||C.StructLength||C.UdsLength)C.ExtendedAttached=true;
        C.ObjectScope->Attached=true;C.Graph->AddNode(Node,false,false);if(!C.ObjectScope->Check()||!ExtendedCurrent(C,R,Node))return R;
        Node->SetFromFunction(C.Function);if(!C.ObjectScope->Check()||!ExtendedCurrent(C,R,Node))return R;
        C.ObjectScope->Member.SetExternalMember(C.Function->GetFName(),C.Library);C.ObjectScope->FunctionAssigned=true;if(!C.ObjectScope->Check()||!ExtendedCurrent(C,R,Node))return R;
        Node->NodePosX=C.X;Node->NodePosY=C.Y;Node->CreateNewGuid();if(!C.ObjectScope->Check()||!ExtendedCurrent(C,R,Node))return R;
        C.ObjectScope->NodeGuid=Node->NodeGuid;C.ObjectScope->GuidFixed=true;
        R.Data->SetStringField(TEXT("object_array_created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));
        if(!Node->NodeGuid.IsValid()||!C.ObjectScope->Check())return R;
        Node->PostPlacedNewNode();if(!C.ObjectScope->Check()||!ExtendedCurrent(C,R,Node))return R;
        if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!C.ObjectScope->Check()||!ExtendedCurrent(C,R,Node))return R;}
        if(!C.ObjectScope->FreezePins())return R;
    }
    else if(C.EnumScope.IsValid())
    {
        UClass* AllocationClass=UK2Node_CallArrayFunction::StaticClass();if(!C.EnumScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }
        R.Data->SetNumberField(TEXT("node_allocation_calls"),1);
        Node=NewObject<UK2Node_CallArrayFunction>(C.Graph,NAME_None,RF_Transactional);
        R.Data->SetNumberField(TEXT("node_allocation_returns"),1);R.Data->SetBoolField(TEXT("node_allocation_returned_nonnull"),Node!=nullptr);
        if(C.DefaultEventAttached&&!AdoptDefaultEventArrayNode(C,R,Node))return R;
        if(!C.EnumScope->SetNode(Node,AllocationClass)||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }
        if(C.Graph->HasAnyFlags(RF_Transient)) { Node->SetFlags(RF_Transient); }
        C.EnumScope->Attached=true;if(C.DefaultEventAttached)C.ExtendedAttached=true;R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);
        C.Graph->AddNode(Node,false,false);if(!C.EnumScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }
        C.EnumScope->FunctionAssigned=true;Node->SetFromFunction(C.Function);if(!C.EnumScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }Node->NodePosX=C.X;Node->NodePosY=C.Y;
        Node->CreateNewGuid();if(!C.EnumScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }
        C.EnumScope->NodeGuid=Node->NodeGuid;C.EnumScope->GuidFixed=true;
        Node->PostPlacedNewNode();if(!C.EnumScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }
        if(Node->Pins.IsEmpty()) { Node->AllocateDefaultPins();if(!C.EnumScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; } }
        if(!C.EnumScope->FreezePins()) { return R; }
    }
    else if(C.VectorScope.IsValid())
    {
        UClass* AllocationClass=UK2Node_CallArrayFunction::StaticClass();if(!C.VectorScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }
        R.Data->SetNumberField(TEXT("node_allocation_calls"),1);Node=NewObject<UK2Node_CallArrayFunction>(C.Graph,NAME_None,RF_Transactional);
        R.Data->SetNumberField(TEXT("node_allocation_returns"),1);R.Data->SetBoolField(TEXT("node_allocation_returned_nonnull"),Node!=nullptr);
        if(C.DefaultEventAttached&&!AdoptDefaultEventArrayNode(C,R,Node))return R;
        if(!C.VectorScope->SetNode(Node,AllocationClass)||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }if(C.Graph->HasAnyFlags(RF_Transient)) { Node->SetFlags(RF_Transient); }
        C.VectorScope->Attached=true;if(C.DefaultEventAttached)C.ExtendedAttached=true;R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);C.Graph->AddNode(Node,false,false);if(!C.VectorScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }
        C.VectorScope->FunctionAssigned=true;Node->SetFromFunction(C.Function);if(!C.VectorScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }Node->NodePosX=C.X;Node->NodePosY=C.Y;
        Node->CreateNewGuid();if(!C.VectorScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }C.VectorScope->NodeGuid=Node->NodeGuid;C.VectorScope->GuidFixed=true;Node->PostPlacedNewNode();if(!C.VectorScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; }
        if(Node->Pins.IsEmpty()) { Node->AllocateDefaultPins();if(!C.VectorScope->Check()||(C.DefaultEventAttached&&!ExtendedCurrent(C,R,Node))) { return R; } }if(!C.VectorScope->FreezePins()) { return R; }
    }
    else if(ExtendedPrimitive(C)||C.GraphCreationCurrent)
    {
        R.Data->SetNumberField(TEXT("node_allocation_calls"),1);Node=NewObject<UK2Node_CallArrayFunction>(C.Graph,NAME_None,RF_Transactional);R.Data->SetNumberField(TEXT("node_allocation_returns"),1);
        if(C.DefaultEventAttached){if(!AdoptDefaultEventArrayNode(C,R,Node))return R;}else C.ExtendedCreated.Reset(Node);
        if(!Node||!ExtendedCurrent(C,R,Node))return R;
        if(C.Graph->HasAnyFlags(RF_Transient))Node->SetFlags(RF_Transient);
        C.ExtendedAttached=true;R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);C.Graph->AddNode(Node,false,false);R.Data->SetNumberField(TEXT("graph_add_node_returns"),1);R.bChanged=true;if(!ExtendedCurrent(C,R,Node))return R;
        Node->SetFromFunction(C.Function);if(!ExtendedCurrent(C,R,Node))return R;Node->NodePosX=C.X;Node->NodePosY=C.Y;Node->CreateNewGuid();if(!ExtendedCurrent(C,R,Node))return R;
        Node->PostPlacedNewNode();if(!ExtendedCurrent(C,R,Node))return R;
        if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!ExtendedCurrent(C,R,Node))return R;}
    }
    else
    {
        FGraphNodeCreator<UK2Node_CallArrayFunction> Creator(*C.Graph);Node=Creator.CreateNode(false);
        Node->SetFromFunction(C.Function);Node->NodePosX=C.X;Node->NodePosY=C.Y;Creator.Finalize();
    }
    if(TrackedArrayObservation(C)){C.ExtendedCreated.Reset(Node);if(!ExtendedFreeze(C,R,Node))return R;}
    bool Good = NodeShape(C, Node, false);if(C.bVector4&&!C.ObjectScope->Check())return R;
    R.Data->SetBoolField(TEXT("allocated_native_shape_verified"), Good);
    if (!Good) { Error(R, TEXT("array_allocated_node_shape_mismatch"), TEXT("Engine-allocated exact call-array pins differed before primitive specialization; retained node is not repaired.")); }
    if (Good)
    {
        R.Stage = TEXT("specialize_array_node");
        for (int32 Index = 0; Index < C.Operation->Count; ++Index)
        {
            const FParameter& Parameter = C.Operation->Parameters[Index];
            if (FString(Parameter.Category) != TEXT("wildcard")) { continue; }
            UEdGraphPin* Pin = Node->FindPin(Parameter.Name, Input(Parameter) ? EGPD_Input : EGPD_Output);
            if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }
            FEdGraphPinType Expected=Pin->PinType;Expected.PinCategory=C.ElementCategory;Expected.PinSubCategory=C.ElementSubCategory;Expected.PinSubCategoryObject=C.ElementObject;
            if(C.EnumScope.IsValid()&&!C.EnumScope->ExpectType(Pin,Expected)) { return R; }
            if(C.VectorScope.IsValid()&&!C.VectorScope->ExpectType(Pin,Expected)) { return R; }
            if(C.NameScope.IsValid()&&!C.NameScope->ExpectType(Pin,Expected))return R;
            if(C.StructOperation){if(!C.ObjectScope->ExpectStructOperationSpecialization(Pin,Expected,Parameter))return R;}
            else if(C.bVector4){if(!C.ObjectScope->ExpectVector4Specialization(Pin,Expected,Parameter))return R;}
            else if(C.ObjectScope.IsValid()&&!C.ObjectScope->ExpectType(Pin,Expected)) { return R; }
            if(TrackedArrayObservation(C)){for(auto& Role:C.ExtendedPins)if(Role.Address==Pin)Role.Type=Expected;}
            // Retain engine-allocated ref/const/container and other flags.
            // Only the original type-specialization fields are assigned.
            Pin->PinType.PinCategory = C.ElementCategory; Pin->PinType.PinSubCategory = C.ElementSubCategory; Pin->PinType.PinSubCategoryObject = C.ElementObject;
            if(C.bVector4){double Count=0;R.Data->TryGetNumberField(TEXT("vector4_auto_default_sdk_attempted"),Count);R.Data->SetNumberField(TEXT("vector4_auto_default_sdk_attempted"),Count+1);}
            if(C.NameScope.IsValid()&&!C.NameScope->ExpectDefault(Pin,FString(),FString()))return R;
            C.Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
            if(C.bVector4){double Count=0;R.Data->TryGetNumberField(TEXT("vector4_auto_default_sdk_returned"),Count);R.Data->SetNumberField(TEXT("vector4_auto_default_sdk_returned"),Count+1);}
            if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }
        }
        Good = NodeShape(C, Node, true);
        if (!Good) { Error(R, TEXT("array_typed_node_shape_mismatch"), TEXT("Primitive specialization changed the exact expected pin flags/shape; retained node is not repaired.")); }
    }
    if((C.StructOperation||C.DescriptorOperation||C.UdsOperation)&&Good){auto TypedBefore=NodeState(Node,nullptr,nullptr,C.ObjectScope.Get(),nullptr,&C);if(!C.ObjectScope->Check()||!ExtendedCurrent(C,R,Node))return R;R.Data->SetObjectField(TEXT("typed_node_before_defaults"),TypedBefore);}
    auto Applied = MakeShared<FJsonObject>();auto DefaultObservations=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> StructDefaultPrefix;
    if(C.StructOperation||C.UdsOperation){R.Data->SetObjectField(TEXT("param_defaults_applied"),Applied);R.Data->SetObjectField(TEXT("param_default_observations"),DefaultObservations);R.Data->SetArrayField(C.UdsOperation?TEXT("scalar_uds_array_default_prefix"):TEXT("value_struct_array_default_prefix"),StructDefaultPrefix);}
    TArray<TSharedPtr<FJsonValue>> DescriptorPrefix;
    if(C.DescriptorOperation){R.Data->SetObjectField(TEXT("param_defaults_applied"),Applied);R.Data->SetObjectField(TEXT("param_default_observations"),DefaultObservations);R.Data->SetArrayField(TEXT("descriptor_array_default_prefix"),DescriptorPrefix);}
    if(C.ObjectParity)R.Data->SetObjectField(TEXT("param_defaults_applied"),Applied);if(ExtendedPrimitive(C)){R.Data->SetObjectField(TEXT("param_defaults_applied"),Applied);R.Data->SetObjectField(TEXT("param_default_observations"),DefaultObservations);}
    if (Good)
    {
        R.Stage = TEXT("apply_array_defaults");
        for (const FDefault& Default : C.Defaults)
        {
            if(C.DescriptorOperation){if(!ApplyDescriptorDefault(C,R,Node,Default,Applied,DefaultObservations,DescriptorPrefix)){if(!R.bChangeStateKnown)return R;Good=false;break;}continue;}
            TSharedPtr<FJsonObject> Prefix;
            if(C.StructOperation||C.UdsOperation){Prefix=MakeShared<FJsonObject>();Prefix->SetStringField(TEXT("requested_pin"),Default.RequestedName);Prefix->SetStringField(TEXT("requested_literal"),Default.Value);for(const TCHAR* Flag:{TEXT("validation_attempted"),TEXT("validation_returned"),TEXT("setter_attempted"),TEXT("setter_returned"),TEXT("explicit_notify_attempted"),TEXT("explicit_notify_returned"),TEXT("verified")})Prefix->SetBoolField(Flag,false);StructDefaultPrefix.Add(MakeShared<FJsonValueObject>(Prefix.ToSharedRef()));R.Data->SetArrayField(C.UdsOperation?TEXT("scalar_uds_array_default_prefix"):TEXT("value_struct_array_default_prefix"),StructDefaultPrefix);}
            UEdGraphPin* Pin = Node->FindPin(Default.PinName, EGPD_Input);
            if(!Pin||Pin->bDefaultValueIsIgnored) { Good=false;Error(R,TEXT("array_engine_default_rejected"),TEXT("New input default role is absent or ignored."));break; }
            FString SchemaLiteral=Default.Value;FText ValidationText;
            if(C.ElementCategory==UEdGraphSchema_K2::PC_Text&&Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Text&&!Pin->PinType.IsContainer())
            {
                FString ExpectedRaw;TObjectPtr<UObject> ExpectedObject=nullptr;FText ExpectedText;
                C.Schema->GetPinDefaultValuesFromString(Pin->PinType,Node,Default.Value,ExpectedRaw,ExpectedObject,ExpectedText,false);
                if(!ExtendedCurrent(C,R,Node))return R;
                if(ExpectedObject||!ExpectedRaw.IsEmpty()){Good=false;Error(R,TEXT("array_text_conversion_invalid"),TEXT("Text SDK conversion returned a non-Text default role."));break;}
                ValidationText=ExpectedText;SchemaLiteral.Reset();FTextStringHelper::WriteToBuffer(SchemaLiteral,ExpectedText);
                if(!ExtendedCurrent(C,R,Node))return R;
            }
            if(C.StructOperation&&C.StructLengthKind==TEXT("rotator")&&Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct)
            {
                FRotator Parsed;
                if(FDefaultValueHelper::ParseRotator(SchemaLiteral,Parsed))
                {
                    if(Parsed.ContainsNaN()){Good=false;Error(R,TEXT("array_rotator_default_nonfinite"),TEXT("Rotator item literal must parse to finite pitch/yaw/roll."));break;}
                    SchemaLiteral=FString::Printf(TEXT("%s,%s,%s"),*FString::SanitizeFloat(Parsed.Pitch),*FString::SanitizeFloat(Parsed.Yaw),*FString::SanitizeFloat(Parsed.Roll));
                }
            }
            if(Prefix.IsValid())Prefix->SetStringField(TEXT("schema_literal"),SchemaLiteral);
            if(ExtendedPrimitive(C)||C.StructOperation||C.UdsOperation)C.ExtendedSchemaDefaults.Add(Default.PinName,SchemaLiteral);
            if(Prefix.IsValid()){Prefix->SetStringField(TEXT("pin_id"),Pin->PinId.ToString(EGuidFormats::Digits));Prefix->SetBoolField(TEXT("validation_attempted"),true);}
            const FString Validation=C.Schema->IsPinDefaultValid(Pin,Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Text?FString():SchemaLiteral,nullptr,ValidationText);
            if(Prefix.IsValid())Prefix->SetBoolField(TEXT("validation_returned"),true);
            if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }Pin=Node->FindPin(Default.PinName,EGPD_Input);
            if (!Validation.IsEmpty())
            { Good = false; Error(R, TEXT("array_engine_default_rejected"), TEXT("Engine rejected the prevalidated primitive default on the newly created node: ") + Default.RequestedName); break; }
            if(C.ObjectScope.IsValid()&&!C.ObjectScope->ExpectDefault(Pin,(C.StructOperation||C.UdsOperation)?SchemaLiteral:Default.Value))return R;
            if(Prefix.IsValid())Prefix->SetBoolField(TEXT("setter_attempted"),true);
            C.Schema->TrySetDefaultValue(*Pin,SchemaLiteral,false);if(Prefix.IsValid())Prefix->SetBoolField(TEXT("setter_returned"),true);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }Pin=Node->FindPin(Default.PinName,EGPD_Input);
            if(C.StructOperation||C.UdsOperation){Applied->SetStringField(Default.RequestedName,Pin->DefaultValue);Prefix->SetStringField(TEXT("actual_raw_after_set"),Pin->DefaultValue);}
            if(C.EnumScope.IsValid()||C.VectorScope.IsValid()||C.ObjectScope.IsValid()) { if(Prefix.IsValid())Prefix->SetBoolField(TEXT("explicit_notify_attempted"),true);Node->PinDefaultValueChanged(Pin);if(Prefix.IsValid())Prefix->SetBoolField(TEXT("explicit_notify_returned"),true);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }Pin=Node->FindPin(Default.PinName,EGPD_Input); }
            const bool Match=C.Schema->DoesDefaultValueMatch(*Pin,C.ExtendedSchemaDefaults.Contains(Default.PinName)?C.ExtendedSchemaDefaults.FindChecked(Default.PinName):Default.Value);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }Pin=Node->FindPin(Default.PinName,EGPD_Input);
            const FString CurrentError=C.Schema->IsCurrentPinDefaultValid(Pin);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }Pin=Node->FindPin(Default.PinName,EGPD_Input);
            if (!Match||!CurrentError.IsEmpty()||Pin->DefaultObject||(Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Text?!Pin->DefaultValue.IsEmpty():!Pin->DefaultTextValue.IsEmpty()))
            { Good = false; Error(R, TEXT("array_default_readback_mismatch"), TEXT("Engine default readback differs on the retained new node: ") + Default.RequestedName); break; }
            Applied->SetStringField(Default.RequestedName, Pin->DefaultValue);
            if(ExtendedPrimitive(C)||C.StructOperation||C.UdsOperation)
            {
                const FString Raw=Pin->DefaultValue;const FText ActualText=Pin->DefaultTextValue;const FGuid ActualId=Pin->PinId;const FEdGraphPinType ActualType=Pin->PinType;
                const FString Display=ActualText.ToString();if(!ExtendedCurrent(C,R,Node))return R;
                Pin=Node->FindPin(Default.PinName,EGPD_Input);const FString DefaultAsString=Pin->GetDefaultAsString();if(!ExtendedCurrent(C,R,Node))return R;
                if(C.StructOperation||C.UdsOperation){Prefix->SetBoolField(TEXT("verified"),true);Prefix->SetStringField(TEXT("actual_raw_verified"),Raw);}
                auto Observation=MakeShared<FJsonObject>();if(C.StructOperation||C.UdsOperation){Observation->SetStringField(TEXT("autogenerated_default_value"),Pin->AutogeneratedDefaultValue);Observation->SetStringField(TEXT("default_object"),FString());}Observation->SetStringField(TEXT("requested_literal"),Default.Value);Observation->SetStringField(TEXT("schema_literal"),SchemaLiteral);Observation->SetStringField(TEXT("raw_default"),Raw);Observation->SetStringField(TEXT("default_text"),Display);Observation->SetStringField(TEXT("default_as_string"),DefaultAsString);Observation->SetBoolField(TEXT("schema_match"),Match);Observation->SetBoolField(TEXT("current_default_valid"),CurrentError.IsEmpty());Observation->SetStringField(TEXT("pin_id"),ActualId.ToString(EGuidFormats::Digits));Observation->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(ActualType));if(!ExtendedCurrent(C,R,Node))return R;DefaultObservations->SetObjectField(Default.RequestedName,Observation);
            }
        }
    }
    R.Data->SetObjectField(TEXT("param_defaults_applied"), Applied);if(ExtendedPrimitive(C)||C.StructOperation||C.DescriptorOperation||C.UdsOperation)R.Data->SetObjectField(TEXT("param_default_observations"),DefaultObservations);
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }
    if(IsValid(Node)&&C.Graph->Nodes.Contains(Node)) { FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint); }
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||(C.ObjectScope.IsValid()&&!C.ObjectScope->Check())||!ExtendedCurrent(C,R,Node)) { return R; }
    if(C.ObjectScope.IsValid()&&!C.bVector4&&!C.StructLength&&!C.UdsLength)
    {
        R.Data->SetBoolField(TEXT("native_object_array"),true);R.Data->SetBoolField(TEXT("object_array_target_requires_connection"),true);
        R.Data->SetBoolField(TEXT("object_array_item_requires_connection"),!C.DescriptorLength);R.Data->SetBoolField(TEXT("object_array_runtime_invocation_verified"),false);
        if(C.ObjectScope->SkeletalDedupMode)
        {
            R.Data->SetObjectField(TEXT("native_skeletal_dedup_source_after"),C.ObjectScope->SkeletalState());R.Data->SetBoolField(TEXT("native_skeletal_dedup_source_observed_unchanged"),true);
            R.Data->SetBoolField(TEXT("skeletal_array_runtime_deduplication_verified"),false);
        }
        else if(C.ObjectScope->SkeletalLengthMode&&!C.ObjectParity)
        {
            if(FString(C.Operation->Name)==TEXT("get"))
            {
                R.Data->SetObjectField(TEXT("native_skeletal_get_source_after"),C.ObjectScope->SkeletalState());R.Data->SetBoolField(TEXT("native_skeletal_get_source_observed_unchanged"),true);
            }
            else{R.Data->SetBoolField(TEXT("native_skeletal_array_length"),true);R.Data->SetObjectField(TEXT("native_skeletal_length_source_after"),C.ObjectScope->SkeletalState());R.Data->SetBoolField(TEXT("native_skeletal_length_source_observed_unchanged"),true);}
        }
        R.Data->SetStringField(TEXT("object_array_source_scope"),C.DescriptorOperation?TEXT("Exact native Object2/Class5 constraint and loaded-native value classes; optional resolved shallow CDOs, original nonstring Any/default-object paths; no instance loader or runtime execution"):C.DescriptorLength?TEXT("Exact loaded native Object/Actor/component Object or Class Length constraint; actual hierarchy/layout/flags and optional resolved shallow CDO handle, without CDO creation/existence requirement, element metadata or deep member/value sampling"):C.ObjectParity?TEXT("Existing exact native ActorComponent/CAC/SkeletalMeshComponent hierarchy, existing CDO and nine-operation metadata; current whole-pin/source prefix, no object literals or runtime values/invocation"):C.ObjectScope->SkeletalDedupMode?TEXT("Exact loaded native SkeletalMeshComponent hierarchy/package/layout and existing resolved CDO metadata for clear/contains/add_unique graph authoring; no source/runtime array values, dedup execution or other class/op admission"):C.ObjectScope->SkeletalLengthMode?(FString(C.Operation->Name)==TEXT("get")?TEXT("Exact loaded native SkeletalMeshComponent hierarchy/package/layout and existing resolved CDO identity for Array_Get copy graph authoring only; no source/runtime array values or other operation admission"):TEXT("Exact loaded native SkeletalMeshComponent hierarchy/package/layout and existing resolved CDO identity for length only; no source values, runtime arrays or other object operations")):TEXT("Exact loaded native ActorComponent/CAC class hierarchy layout/identity, clean Engine package and existing CDO identity; full CDO/private/global callback state is unobserved. No object instances are loaded or evaluated."));
    }
    R.Data->SetStringField(TEXT("runtime_reference_scope"), C.UdsOperation?TEXT("Loaded saved-clean 1..32 own scalar-field UDS eight-operation graph authoring; original generic Any-to-literal storage with actual SDK/default/current readback. Schema defers generic struct ImportText to a later compiler stage: source/field-name/default-buffer values, compile acceptance and runtime bounds/equality/mutation are not evaluated here. No loader or other property profile expansion"):C.UdsLength?TEXT("Loaded saved-clean scalar UDS Array_Length authoring and typed producer wire only; 1..32 own FString/int32/int64/float/double/bool/FName fields with current authored GUID/EditorData/source metadata and bind-final file SHA. No source values/default buffer/CDO/runtime array count; other property families, loaders, aliases, custom ScriptStructs and additional operations remain incomplete"):C.StructOperation?TEXT("Exact native Rotator/Transform nine-array authoring, stock same-type wires and original descriptor Any-to-item literal conversion with actual schema/default readback; no runtime bounds/equality/AddUnique/Remove execution, array initialization, loaders or generic UDS"):C.StructLength?TEXT("Exact native Rotator/Transform Array_Length authoring and typed producer wire only; direct native struct fields and Transform Quat/Vector bindings preserved without values/CDO/default buffers or runtime count evaluation; generic UDS, loaders and additional operations remain incomplete"):C.DescriptorOperation?TEXT("Exact native Object2/Class5 eight-operation graph authoring with compatible loaded-native class/default-object and original nonstring Any; no loader, object instance values or runtime array execution"):C.DescriptorLength?TEXT("Exact native Object/Actor/component Object or Class Array_Length authoring and typed producer wire only; optional resolved element CDO handle observed without creation/value sampling; generic structs/classes, loaders and runtime count evaluation remain incomplete"):C.ObjectParity?TEXT("Existing runtime Engine.KismetArrayLibrary nine-array authoring and exact ActorComponent/CAC/SkeletalMeshComponent array/item wires; no runtime bounds, comparison or mutation proof"):C.ObjectScope.IsValid()&&C.ObjectScope->SkeletalDedupMode?TEXT("Exact Engine.KismetArrayLibrary Array_Clear/Array_Contains/Array_AddUnique and SkeletalMeshComponent array/item wires only; no runtime mutation/iteration/dedup or arbitrary object values"):C.ObjectScope.IsValid()&&C.ObjectScope->SkeletalLengthMode?(FString(C.Operation->Name)==TEXT("get")?TEXT("Exact native Engine.KismetArrayLibrary Array_Get value-copy with SkeletalMeshComponent array wire and existing int32 Index default; no runtime bounds/values, reference-get, mutation or other operation admission"):TEXT("Exact native Engine.KismetArrayLibrary Array_Length with SkeletalMeshComponent array wire only; no runtime iteration/values or object get/clear/add")):C.bVector4?TEXT("Exact native Engine.KismetArrayLibrary and CoreUObject.Vector4 pins; nine-operation authoring (Get value-copy, RemoveItem), typed wires and finite complete XYZW item-string literals only; no runtime bounds, equality, array mutation or Cubeless runtime references"):C.ObjectScope.IsValid()?TEXT("Exact runtime Engine.KismetArrayLibrary and native ActorComponent/ChildActorComponent object-array pins; node-only length/get value-copy/clear/add, explicit data wires; no runtime array/component mutation") : C.EnumScope.IsValid()?TEXT("native Engine.KismetArrayLibrary and exact read-only runtime UserDefinedEnum byte pins; source entries/stored fields/metadata/disk preserved; compile/cold/cook proof separate"):C.ElementObject
        ? TEXT("exact native Engine.KismetArrayLibrary and CoreUObject.Vector pins; no Cubeless runtime function/class/type reference introduced")
        : TEXT("exact native Engine.KismetArrayLibrary and primitive8 array pins only; no Cubeless runtime function/class/type reference introduced"));
    Finish(C, R, Node, Good); return R;
}
}
