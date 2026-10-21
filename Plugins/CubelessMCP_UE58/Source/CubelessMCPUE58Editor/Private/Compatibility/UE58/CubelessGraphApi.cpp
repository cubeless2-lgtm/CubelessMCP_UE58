#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Compatibility/UE58/CubelessLoopApi.h"
#include "Compatibility/UE58/CubelessSequenceApi.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_EditablePinBase.h"
#include "Internationalization/Text.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Composite.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Timeline.h"
#include "Engine/TimelineTemplate.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/DataTableFunctionLibrary.h"
#include "Compatibility/UE58/CubelessDataTableApi.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "AssetRegistry/AssetData.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_Select.h"
#include "K2Node_MakeArray.h"
#include "K2Node_CallArrayFunction.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessConnectionApi.h"
#include "Compatibility/UE58/CubelessComponentApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/DataTable.h"
#include "Engine/TextureRenderTarget2D.h"
#include "NiagaraSystem.h"
#include "Math/Color.h"
#include "Components/ActorComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "K2Node.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "Kismet/BlueprintMapLibrary.h"
#include "K2Node_CallParentFunction.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/MetaData.h"
#include "Engine/UserDefinedEnum.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Math/RandomStream.h"
#include "Math/Vector4.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Graph
{
namespace
{
TSharedRef<FJsonObject> Result()
{
    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false);
    R->SetBoolField(TEXT("mutation_attempted"), false);
    R->SetBoolField(TEXT("saved"), false);
    R->SetBoolField(TEXT("compiled"), false);
    R->SetBoolField(TEXT("loaded_during_read"), false);
    R->SetStringField(TEXT("stage"), TEXT("preflight"));
    return R;
}
bool Fail(const TSharedRef<FJsonObject>& R, const TCHAR* Code, const TCHAR* Message)
{
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    return false;
}
bool SafeSelector(const FString& Text)
{
    if (Text.IsEmpty() || Text.Len() >= NAME_SIZE) { return false; }
    for (const TCHAR C : Text) { if (C < 32 || C == 127) { return false; } }
    return true;
}
TSharedRef<FJsonObject> PinTypeObservation(const FEdGraphPinType& Type)
{
    auto Value = MakeShared<FJsonObject>();
    Value->SetStringField(TEXT("category"), Type.PinCategory.ToString());
    Value->SetStringField(TEXT("subcategory"), Type.PinSubCategory.ToString());
    Value->SetStringField(TEXT("type_object"), Type.PinSubCategoryObject.IsValid() ? Type.PinSubCategoryObject->GetPathName() : FString());
    Value->SetNumberField(TEXT("container_type"), static_cast<int32>(Type.ContainerType));
    Value->SetBoolField(TEXT("is_reference"), Type.bIsReference);
    Value->SetBoolField(TEXT("is_const"), Type.bIsConst);
    if(Type.IsMap())
    {
        Value->SetBoolField(TEXT("is_weak_pointer"),Type.bIsWeakPointer);Value->SetBoolField(TEXT("is_uobject_wrapper"),Type.bIsUObjectWrapper);Value->SetBoolField(TEXT("serialize_single_precision"),Type.bSerializeAsSinglePrecisionFloat);
        auto Terminal=MakeShared<FJsonObject>();Terminal->SetStringField(TEXT("category"),Type.PinValueType.TerminalCategory.ToString());Terminal->SetStringField(TEXT("subcategory"),Type.PinValueType.TerminalSubCategory.ToString());Terminal->SetStringField(TEXT("type_object"),Type.PinValueType.TerminalSubCategoryObject.IsValid()?Type.PinValueType.TerminalSubCategoryObject->GetPathName():FString());Terminal->SetBoolField(TEXT("is_const"),Type.PinValueType.bTerminalIsConst);Terminal->SetBoolField(TEXT("is_weak_pointer"),Type.PinValueType.bTerminalIsWeakPointer);Terminal->SetBoolField(TEXT("is_uobject_wrapper"),Type.PinValueType.bTerminalIsUObjectWrapper);Value->SetObjectField(TEXT("terminal"),Terminal);
    }
    return Value;
}
TSharedRef<FJsonObject> RandomStreamSourceObservation()
{
    auto State = MakeShared<FJsonObject>();
    const UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.RandomStream"));
    const UPackage* Package = IsValid(Struct) ? Struct->GetOutermost() : nullptr;
    const bool Complete = IsValid(Struct) && Package;
    State->SetBoolField(TEXT("observations_complete"), Complete);
    State->SetStringField(TEXT("struct_path"), Complete ? Struct->GetPathName() : FString());
    State->SetNumberField(TEXT("struct_size"), Complete ? Struct->GetStructureSize() : 0);
    State->SetStringField(TEXT("package_path"), Complete ? Package->GetName() : FString());
    State->SetBoolField(TEXT("package_dirty"), Complete && Package->IsDirty());
    State->SetStringField(TEXT("package_flags_string"), Complete ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    State->SetBoolField(TEXT("native_identity_verified"), Complete && Struct == TBaseStructure<FRandomStream>::Get()
        && Struct->GetStructureSize() == sizeof(FRandomStream) && !Struct->IsEditorOnly()
        && !Struct->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && Package->GetName() == TEXT("/Script/CoreUObject") && !Package->IsDirty()
        && !Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly));
    return State;
}
bool SameRandomStreamSource(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After)
{
    return Before->GetBoolField(TEXT("observations_complete")) && After->GetBoolField(TEXT("observations_complete"))
        && Before->GetStringField(TEXT("struct_path")) == After->GetStringField(TEXT("struct_path"))
        && Before->GetNumberField(TEXT("struct_size")) == After->GetNumberField(TEXT("struct_size"))
        && Before->GetStringField(TEXT("package_path")) == After->GetStringField(TEXT("package_path"))
        && Before->GetBoolField(TEXT("package_dirty")) == After->GetBoolField(TEXT("package_dirty"))
        && Before->GetStringField(TEXT("package_flags_string")) == After->GetStringField(TEXT("package_flags_string"))
        && Before->GetBoolField(TEXT("native_identity_verified")) == After->GetBoolField(TEXT("native_identity_verified"));
}
TSharedRef<FJsonObject> NativeClassConstraintObservation(const FEdGraphPinType& Type)
{
    auto State = MakeShared<FJsonObject>();
    const UClass* Class = Cast<UClass>(Type.PinSubCategoryObject.Get());
    const UPackage* Package = IsValid(Class) ? Class->GetOutermost() : nullptr;
    const UObject* CDO = IsValid(Class) ? Class->GetDefaultObject(false) : nullptr;
    const bool Complete = IsValid(Class) && Package;
    State->SetBoolField(TEXT("observations_complete"), Complete);
    State->SetStringField(TEXT("class_path"), Complete ? Class->GetPathName() : FString());
    State->SetStringField(TEXT("super_class_path"), Complete && Class->GetSuperClass() ? Class->GetSuperClass()->GetPathName() : FString());
    State->SetStringField(TEXT("class_flags_string"), Complete ? LexToString(static_cast<uint32>(Class->GetClassFlags())) : FString());
    State->SetStringField(TEXT("class_generated_by_path"), Complete && Class->ClassGeneratedBy ? Class->ClassGeneratedBy->GetPathName() : FString());
    State->SetStringField(TEXT("package_path"), Complete ? Package->GetName() : FString());
    State->SetStringField(TEXT("package_flags_string"), Complete ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    State->SetBoolField(TEXT("package_dirty"), Complete && Package->IsDirty());
    State->SetBoolField(TEXT("existing_cdo_available"), IsValid(CDO));
    State->SetStringField(TEXT("existing_cdo_path"), IsValid(CDO) ? CDO->GetPathName() : FString());
    State->SetStringField(TEXT("existing_cdo_flags_string"), IsValid(CDO) ? LexToString(static_cast<uint32>(CDO->GetFlags())) : FString());
    State->SetBoolField(TEXT("cdo_creation_requested"), false);
    State->SetBoolField(TEXT("native_identity_verified"), Complete && RuntimeObject::IsSupportedClassPin(Type)
        && ((Class == AActor::StaticClass() && Class->GetPathName() == TEXT("/Script/Engine.Actor"))
            || (Class == UActorComponent::StaticClass() && Class->GetPathName() == TEXT("/Script/Engine.ActorComponent")))
        && Class->ClassGeneratedBy == nullptr && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && !Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && IsValid(CDO) && CDO->GetClass() == Class
        && !CDO->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects));
    return State;
}
bool MaterialCastCDOMatches(const UClass* Class, const UObject* Expected)
{
    if (!IsValid(Class)) { return false; }
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const bool Matches = Class->ClassDefaultObject.IsResolved() && Class->ClassDefaultObject == Expected;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return Matches;
}
const UObject* MaterialCastExistingCDO(const UClass* Class)
{
    if (!IsValid(Class) || !IsValid(Class->GetPackage())) { return nullptr; }
    const UObject* Found = StaticFindObject(nullptr, Class->GetPackage(), *(TEXT("Default__") + Class->GetName()), false);
    return MaterialCastCDOMatches(Class, Found) ? Found : nullptr;
}
// LI: one exact native DataTable cast observer; no generic RuntimeObject or asset loader admission.
TSharedRef<FJsonObject> DataTableCastSourceObservation(const UClass* Class)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),false);J->SetBoolField(TEXT("native_identity_verified"),false);
    auto Ready=[](){return IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&IsValid(GEditor)&&!GEditor->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects);};
    if(!Ready()||!IsValid(Class)||Class!=UDataTable::StaticClass())return J;TStrongObjectPtr<UClass> Held(const_cast<UClass*>(Class));UPackage* P=Class->GetPackage();if(!IsValid(P))return J;TStrongObjectPtr<UPackage> HeldPackage(P);
    const EObjectFlags Flags=Class->GetFlags();const uint32 ClassFlags=uint32(Class->GetClassFlags()),PackageFlags=P->GetPackageFlags();const int32 Size=Class->GetStructureSize();
    const UObject* CDO=MaterialCastExistingCDO(Class);if(!IsValid(CDO))return J;TStrongObjectPtr<UObject> HeldCDO(const_cast<UObject*>(CDO));const EObjectFlags CDOFlags=CDO->GetFlags();
    auto Current=[&](){if(!Ready()||Class!=Held.Get()||!IsValid(Class)||Class->GetClass()!=UClass::StaticClass()||P!=HeldPackage.Get()||!IsValid(P)||Class->GetOuter()!=P||Class->GetPackage()!=P||Class->GetFlags()!=Flags||uint32(Class->GetClassFlags())!=ClassFlags||Class->GetStructureSize()!=Size||P->GetPackageFlags()!=PackageFlags||P->IsDirty()||Class->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects)||CDO!=HeldCDO.Get()||!IsValid(CDO)||!MaterialCastCDOMatches(Class,CDO)||CDO->GetClass()!=Class||CDO->GetOuter()!=P||CDO->GetFlags()!=CDOFlags||CDO->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects))return false;auto It=Class->GetSuperStructIterator();++It;return It&&*It==UObject::StaticClass();};
    if(!Current())return J;const bool Exact=Class->GetPathName()==TEXT("/Script/Engine.DataTable")&&FindObject<UClass>(nullptr,TEXT("/Script/Engine.DataTable"))==Class&&Class->ClassGeneratedBy.IsResolved()&&Class->ClassGeneratedBy==nullptr&&Class->HasAnyClassFlags(CLASS_Native)&&!Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)&&!Class->IsEditorOnly()&&P->GetName()==TEXT("/Script/Engine")&&PackageFlags==16U&&CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)&&CDO->GetPathName()==TEXT("/Script/Engine.Default__DataTable");
    if(!Current())return J;J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),Exact);J->SetStringField(TEXT("class_path"),TEXT("/Script/Engine.DataTable"));J->SetStringField(TEXT("super_class_path"),TEXT("/Script/CoreUObject.Object"));J->SetStringField(TEXT("class_flags_string"),LexToString(ClassFlags));J->SetStringField(TEXT("object_flags_string"),LexToString(uint32(Flags)));J->SetNumberField(TEXT("class_size"),Size);J->SetStringField(TEXT("class_generated_by_path"),TEXT(""));J->SetStringField(TEXT("package_path"),TEXT("/Script/Engine"));J->SetStringField(TEXT("package_flags_string"),LexToString(PackageFlags));J->SetBoolField(TEXT("package_dirty"),false);J->SetBoolField(TEXT("existing_cdo_available"),true);J->SetStringField(TEXT("existing_cdo_path"),TEXT("/Script/Engine.Default__DataTable"));J->SetStringField(TEXT("existing_cdo_flags_string"),LexToString(uint32(CDOFlags)));J->SetBoolField(TEXT("cdo_creation_requested"),false);return J;
}
TSharedRef<FJsonObject> DynamicCastTargetSourceObservation(const UClass* Class)
{
    if(IsValid(Class)&&Class==UDataTable::StaticClass())return DataTableCastSourceObservation(Class);
    auto State = MakeShared<FJsonObject>();
    const bool Mid = IsValid(Class) && Class == UMaterialInstanceDynamic::StaticClass();
    const UPackage* Package = IsValid(Class) ? Class->GetOutermost() : nullptr;
    const UObject* CDO = IsValid(Class) ? (Mid ? MaterialCastExistingCDO(Class) : Class->GetDefaultObject(false)) : nullptr;
    const UClass* Super = nullptr;
    if (IsValid(Class))
    {
        if (Mid) { auto It = Class->GetSuperStructIterator(); ++It; Super = It ? Cast<UClass>(*It) : nullptr; }
        else { Super = Class->GetSuperClass(); }
    }
    const bool Complete = IsValid(Class) && Package;
    State->SetBoolField(TEXT("observations_complete"), Complete);
    State->SetStringField(TEXT("class_path"), Complete ? Class->GetPathName() : FString());
    State->SetStringField(TEXT("super_class_path"), Complete && Super ? Super->GetPathName() : FString());
    State->SetStringField(TEXT("class_flags_string"), Complete ? LexToString(static_cast<uint32>(Class->GetClassFlags())) : FString());
    State->SetStringField(TEXT("class_generated_by_path"), Complete && Class->ClassGeneratedBy ? Class->ClassGeneratedBy->GetPathName() : FString());
    State->SetStringField(TEXT("package_path"), Complete ? Package->GetName() : FString());
    State->SetStringField(TEXT("package_flags_string"), Complete ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    State->SetBoolField(TEXT("package_dirty"), Complete && Package->IsDirty());
    State->SetBoolField(TEXT("existing_cdo_available"), IsValid(CDO));
    State->SetStringField(TEXT("existing_cdo_path"), IsValid(CDO) ? CDO->GetPathName() : FString());
    State->SetStringField(TEXT("existing_cdo_flags_string"), IsValid(CDO) ? LexToString(static_cast<uint32>(CDO->GetFlags())) : FString());
    State->SetBoolField(TEXT("cdo_creation_requested"), false);
    const bool Exact = Complete
        && ((Class->GetPathName() == TEXT("/Script/Engine.ChildActorComponent") && Class == UChildActorComponent::StaticClass()
                && Class->GetSuperClass() == USceneComponent::StaticClass())
            || (Class->GetPathName() == TEXT("/Script/Engine.SceneComponent") && Class == USceneComponent::StaticClass()
                && Class->GetSuperClass() == UActorComponent::StaticClass())
            || (Class->GetPathName() == TEXT("/Script/Engine.MaterialInstanceDynamic") && Class == UMaterialInstanceDynamic::StaticClass()
                && Super == UMaterialInstance::StaticClass()));
    // MID is admitted only for this finite cast observer; no RuntimeObject widening.
    State->SetBoolField(TEXT("native_identity_verified"), Exact && (RuntimeObject::IsSupportedClass(Class)
            || (Class == UMaterialInstanceDynamic::StaticClass() && Class->GetPathName() == TEXT("/Script/Engine.MaterialInstanceDynamic")))
        && FindObject<UClass>(nullptr, *Class->GetPathName()) == Class && Class->ClassGeneratedBy == nullptr
        && !Class->HasAnyClassFlags(CLASS_Abstract) && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && !Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && IsValid(CDO) && CDO->GetClass() == Class
        && !CDO->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects));
    return State;
}
// Finite source-only descriptor proof for GetMaterial -> MID DynamicCast.
// Absence of an existing abstract MI CDO is preserved; no CDO is created.
TSharedRef<FJsonObject> MaterialCastSourceObservation(const FEdGraphPinType& Type)
{
    auto State = MakeShared<FJsonObject>();
    const UClass* Class = Cast<UClass>(Type.PinSubCategoryObject.Get());
    if (!IsValid(Class) || Class != UMaterialInterface::StaticClass()
        || Class->GetPathName() != TEXT("/Script/Engine.MaterialInterface"))
    { State->SetBoolField(TEXT("observations_complete"), false); State->SetBoolField(TEXT("native_identity_verified"), false); return State; }
    const UPackage* Package = Class->GetOutermost();
    const UObject* CDO = MaterialCastExistingCDO(Class);
    auto ParentIt = Class->GetSuperStructIterator(); ++ParentIt;
    const UClass* Super = ParentIt ? Cast<UClass>(*ParentIt) : nullptr;
    const bool Complete = IsValid(Class) && Package && MaterialCastCDOMatches(Class, CDO);
    State->SetBoolField(TEXT("observations_complete"), Complete);
    State->SetStringField(TEXT("class_path"), Complete ? Class->GetPathName() : FString());
    State->SetStringField(TEXT("super_class_path"), Complete && Super ? Super->GetPathName() : FString());
    State->SetStringField(TEXT("class_flags_string"), Complete ? LexToString(static_cast<uint32>(Class->GetClassFlags())) : FString());
    State->SetStringField(TEXT("class_generated_by_path"), Complete && Class->ClassGeneratedBy ? Class->ClassGeneratedBy->GetPathName() : FString());
    State->SetStringField(TEXT("package_path"), Complete ? Package->GetName() : FString());
    State->SetStringField(TEXT("package_flags_string"), Complete ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    State->SetBoolField(TEXT("package_dirty"), Complete && Package->IsDirty());
    State->SetBoolField(TEXT("existing_cdo_available"), IsValid(CDO));
    State->SetStringField(TEXT("existing_cdo_path"), IsValid(CDO) ? CDO->GetPathName() : FString());
    State->SetStringField(TEXT("existing_cdo_flags_string"), IsValid(CDO) ? LexToString(static_cast<uint32>(CDO->GetFlags())) : FString());
    State->SetBoolField(TEXT("cdo_creation_requested"), false);
    State->SetBoolField(TEXT("native_identity_verified"), Complete && Class == UMaterialInterface::StaticClass()
        && Class->GetPathName() == TEXT("/Script/Engine.MaterialInterface") && Super == UObject::StaticClass()
        && FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialInterface")) == Class && Class->ClassGeneratedBy == nullptr
        && Class->HasAnyClassFlags(CLASS_Native) && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && !Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly)
        && !Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && (!CDO || (IsValid(CDO) && CDO->GetClass() == Class
            && !CDO->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects))));
    return State;
}
TSharedRef<FJsonObject> NativeSplineSourceObservation(const UClass* Class)
{
    auto State = MakeShared<FJsonObject>();
    const UPackage* Package = IsValid(Class) ? Class->GetOutermost() : nullptr;
    const bool Complete = IsValid(Class) && Package;
    const UObject* CDO = Complete ? Class->GetDefaultObject(false) : nullptr;
    State->SetBoolField(TEXT("observations_complete"), Complete);
    State->SetStringField(TEXT("class_path"), Complete ? Class->GetPathName() : FString());
    State->SetStringField(TEXT("super_class_path"), Complete && Class->GetSuperClass() ? Class->GetSuperClass()->GetPathName() : FString());
    State->SetStringField(TEXT("class_flags_string"), Complete ? LexToString(static_cast<uint32>(Class->GetClassFlags())) : FString());
    State->SetStringField(TEXT("class_generated_by_path"), Complete && Class->ClassGeneratedBy ? Class->ClassGeneratedBy->GetPathName() : FString());
    State->SetStringField(TEXT("package_path"), Complete ? Package->GetName() : FString());
    State->SetStringField(TEXT("package_flags_string"), Complete ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    State->SetBoolField(TEXT("package_dirty"), Complete && Package->IsDirty());
    State->SetBoolField(TEXT("existing_cdo_available"), IsValid(CDO));
    State->SetStringField(TEXT("existing_cdo_path"), IsValid(CDO) ? CDO->GetPathName() : FString());
    State->SetStringField(TEXT("existing_cdo_flags_string"), IsValid(CDO) ? LexToString(static_cast<uint32>(CDO->GetFlags())) : FString());
    State->SetBoolField(TEXT("cdo_creation_requested"), false);
    const bool Verified = Complete && Class == USplineComponent::StaticClass()
        && Class->GetPathName() == TEXT("/Script/Engine.SplineComponent") && Class->GetSuperClass() == UPrimitiveComponent::StaticClass()
        && RuntimeObject::IsSupportedClass(Class) && !Class->HasAnyClassFlags(CLASS_Abstract) && !Class->ClassGeneratedBy
        && !Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && IsValid(CDO) && CDO->GetClass() == Class && !CDO->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects);
    State->SetBoolField(TEXT("native_identity_verified"), Verified);
    State->SetBoolField(TEXT("success"), Verified);
    return State;
}
TSharedRef<FJsonObject> NativeIsmSourceObservation(const UClass* Class)
{
    auto State = MakeShared<FJsonObject>();
    const UPackage* Package = IsValid(Class) ? Class->GetOutermost() : nullptr;
    const bool Complete = IsValid(Class) && Package;
    const UObject* CDO = Complete ? Class->GetDefaultObject(false) : nullptr;
    State->SetBoolField(TEXT("observations_complete"), Complete);
    State->SetStringField(TEXT("class_path"), Complete ? Class->GetPathName() : FString());
    State->SetStringField(TEXT("super_class_path"), Complete && Class->GetSuperClass() ? Class->GetSuperClass()->GetPathName() : FString());
    State->SetStringField(TEXT("class_flags_string"), Complete ? LexToString(static_cast<uint32>(Class->GetClassFlags())) : FString());
    State->SetStringField(TEXT("class_generated_by_path"), Complete && Class->ClassGeneratedBy ? Class->ClassGeneratedBy->GetPathName() : FString());
    State->SetStringField(TEXT("package_path"), Complete ? Package->GetName() : FString());
    State->SetStringField(TEXT("package_flags_string"), Complete ? LexToString(static_cast<uint32>(Package->GetPackageFlags())) : FString());
    State->SetBoolField(TEXT("package_dirty"), Complete && Package->IsDirty());
    State->SetBoolField(TEXT("existing_cdo_available"), IsValid(CDO));
    State->SetStringField(TEXT("existing_cdo_path"), IsValid(CDO) ? CDO->GetPathName() : FString());
    State->SetStringField(TEXT("existing_cdo_flags_string"), IsValid(CDO) ? LexToString(static_cast<uint32>(CDO->GetFlags())) : FString());
    State->SetBoolField(TEXT("cdo_creation_requested"), false);
    const bool Verified = Complete && Class == UInstancedStaticMeshComponent::StaticClass()
        && Class->GetPathName() == TEXT("/Script/Engine.InstancedStaticMeshComponent") && Class->GetSuperClass() == UStaticMeshComponent::StaticClass()
        && RuntimeObject::IsSupportedClass(Class) && !Class->HasAnyClassFlags(CLASS_Abstract) && !Class->ClassGeneratedBy
        && !Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && IsValid(CDO) && CDO->GetClass() == Class && !CDO->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects);
    State->SetBoolField(TEXT("native_identity_verified"), Verified);
    State->SetBoolField(TEXT("success"), Verified);
    return State;
}
UEdGraph* ResolveLoadedGraph(const FString& Path, const TSharedRef<FJsonObject>& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Fail(R, TEXT("graph_editor_state_invalid"), TEXT("Requires the editor game thread outside PIE/SIE.")); return nullptr; }
    if (!SafeSelector(Path) || !Path.StartsWith(TEXT("/Game/")))
    { Fail(R, TEXT("graph_path_invalid"), TEXT("Use the exact loaded /Game graph object path.")); return nullptr; }
    UEdGraph* Graph = FindObject<UEdGraph>(nullptr, *Path);
    UBlueprint* Blueprint = Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : nullptr;
    if (!Graph || !Blueprint || !Graph->GetSchema() || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>())
    { Fail(R, TEXT("graph_not_found"), TEXT("A loaded Blueprint K2 graph was not found; this helper never loads assets.")); return nullptr; }
    if (Graph->Nodes.Num() > 2048)
    { Fail(R, TEXT("graph_too_large"), TEXT("Native graph observation is limited to 2048 total nodes, including comments.")); return nullptr; }
    R->SetStringField(TEXT("graph_path"), Graph->GetPathName());
    R->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
    R->SetBoolField(TEXT("dirty_before"), Blueprint->GetOutermost()->IsDirty());
    R->SetNumberField(TEXT("total_node_count"), Graph->Nodes.Num());
    return Graph;
}
bool Complete(const TSharedRef<FJsonObject>& R, UEdGraph* Graph)
{
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint) { return Fail(R, TEXT("graph_owner_changed"), TEXT("Graph owner is unavailable after observation.")); }
    const bool DirtyAfter = Blueprint->GetOutermost()->IsDirty();
    R->SetBoolField(TEXT("dirty_after"), DirtyAfter);
    if (DirtyAfter != R->GetBoolField(TEXT("dirty_before")))
    { return Fail(R, TEXT("graph_read_changed_dirty_state"), TEXT("Graph observation changed package dirty state; inspect the asset.")); }
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("stage"), TEXT("complete")); return true;
}

// UE5.8 exposes this shallow wrapper publicly (deprecated for future releases); no tracked CDO getter or creation is used here.
TObjectPtr<UObject> MaterialValueCDOHandle(const UClass* Class)
{
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const TObjectPtr<UObject> Handle=Class->ClassDefaultObject;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return Handle;
}
TSharedRef<FJsonObject> ReadOwnNativeMaterialValuePlan(UEdGraph* Graph,const FString& VariableName,bool bSetter,const TSharedRef<FJsonObject>& R,bool NiagaraSystem=false,bool TextureRenderTarget2D=false)
{
    const auto Loaded=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    R->SetStringField(TEXT("member_scope"),TEXT("own_native_material_value"));R->SetBoolField(TEXT("native_resource_is_niagara_system"),NiagaraSystem);R->SetBoolField(TEXT("native_resource_is_texture_render_target_2d"),TextureRenderTarget2D);
    UBlueprint* BP=FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if(!Loaded(BP)||!Loaded(Graph)||!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())
    {Fail(R,TEXT("graph_material_value_context_invalid"),TEXT("Requires an idle loaded Blueprint graph on the Editor game thread."));return R;}
    TStrongObjectPtr<UBlueprint> KeepBP(BP);TStrongObjectPtr<UEdGraph> KeepGraph(Graph);TStrongObjectPtr<UEditorEngine> KeepEditor(GEditor);
    const auto ClassRole=[](const TObjectPtr<UClass>& Handle)->UClass*
    {if(!Handle.IsResolved()||Handle==nullptr)return nullptr;UClass* C=FindObject<UClass>(nullptr,*Handle.GetPathName());return Handle==C?C:nullptr;};
    UClass* Skeleton=((NiagaraSystem||TextureRenderTarget2D)?ClassRole(BP->SkeletonGeneratedClass.GetGCPtr()):ClassRole(BP->SkeletonGeneratedClass));UClass* Generated=((NiagaraSystem||TextureRenderTarget2D)?ClassRole(BP->GeneratedClass.GetGCPtr()):ClassRole(BP->GeneratedClass));UClass* Parent=((NiagaraSystem||TextureRenderTarget2D)?ClassRole(BP->ParentClass.GetGCPtr()):ClassRole(BP->ParentClass));
    TStrongObjectPtr<UClass> KeepSkeleton(Skeleton),KeepGenerated(Generated),KeepParent(Parent);
    const FName Name(*VariableName);const int32 Index=FBlueprintEditorUtils::FindNewVariableIndex(BP,Name);
    if(Index==INDEX_NONE||!Loaded(Skeleton)||!Loaded(Generated)||!Loaded(Parent))
    {Fail(R,TEXT("graph_material_value_own_member_required"),TEXT("One own declared member and current generated/skeleton/parent roles are required."));return R;}
    const FBPVariableDescription Description=BP->NewVariables[Index];const FEdGraphPinType Expected=Description.VarType;
    UObject* Native=Expected.PinSubCategoryObject.Get();UClass* NativeClass=Cast<UClass>(Native);UScriptStruct* Color=Cast<UScriptStruct>(Native);
    const bool Object=Expected.PinCategory==UEdGraphSchema_K2::PC_Object&&NativeClass
        &&(NativeClass==UMaterialInterface::StaticClass()||NativeClass==UMaterialInstanceDynamic::StaticClass()||NativeClass==UTexture::StaticClass()||(NiagaraSystem&&NativeClass==UNiagaraSystem::StaticClass())||(TextureRenderTarget2D&&NativeClass==UTextureRenderTarget2D::StaticClass()));
    const bool LinearColor=Expected.PinCategory==UEdGraphSchema_K2::PC_Struct&&Color==TBaseStructure<FLinearColor>::Get();
    if(Description.VarName.ToString()!=VariableName||!Description.VarGuid.IsValid()||(!Object&&!LinearColor)||(NiagaraSystem&&NativeClass!=UNiagaraSystem::StaticClass())||(TextureRenderTarget2D&&NativeClass!=UTextureRenderTarget2D::StaticClass())||!Loaded(Native)
        ||Expected.IsContainer()||!Expected.PinSubCategory.IsNone()||Expected.bIsReference||Expected.bIsConst||Expected.bIsWeakPointer
        ||Expected.bSerializeAsSinglePrecisionFloat||Expected.PinValueType!=FEdGraphTerminalType()||Expected.PinValueType.bTerminalIsUObjectWrapper)
    {Fail(R,TEXT("graph_material_value_type_unsupported"),TEXT("Only own scalar exact native MaterialInterface/MID/Texture/NiagaraSystem/TextureRenderTarget2D and LinearColor members are supported, without external/inherited/soft/weak/container types."));return R;}
    TStrongObjectPtr<UObject> KeepNative(Native);UPackage* Package=Native->GetPackage();TStrongObjectPtr<UPackage> KeepPackage(Package);
    const FString TypePath=Native->GetPathName(),BPPath=BP->GetPathName(),GraphPath=Graph->GetPathName();const FGuid GraphGuid=Graph->GraphGuid;
    const auto Undo=GUndo;const EObjectFlags BPFlags=BP->GetFlags(),GraphFlags=Graph->GetFlags(),SkeletonFlags=Skeleton->GetFlags(),NativeFlags=Native->GetFlags();
    const uint32 NativeTypeFlags=Object?uint32(NativeClass->GetClassFlags()):uint32(Color->StructFlags);
    const int32 NativeSize=Object?NativeClass->GetStructureSize():Color->GetStructureSize(),SkeletonSize=Skeleton->GetStructureSize();
    const EBlueprintStatus Status=BP->Status.GetValue();const bool Dirty=BP->GetPackage()->IsDirty();
    const FProperty* Property=Skeleton->FindPropertyByName(Name);const auto Obj=CastField<FObjectProperty>(Property);const auto Struct=CastField<FStructProperty>(Property);
    const auto Basic=[&]()
    {
        return IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()
            &&GUndo==Undo&&GEditor==KeepEditor.Get()&&Loaded(KeepEditor.Get())&&Loaded(BP)&&Loaded(Graph)&&Loaded(Skeleton)&&Loaded(Generated)&&Loaded(Parent)&&Loaded(Native)&&Loaded(Package)
            &&BP->GetPathName()==BPPath&&Graph->GetPathName()==GraphPath&&Graph->GraphGuid==GraphGuid&&Graph->GetOuter()==BP
            &&((NiagaraSystem||TextureRenderTarget2D)?(BP->ParentClass.GetGCPtr().IsResolved()&&BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()&&BP->GeneratedClass.GetGCPtr().IsResolved()
                &&BP->ParentClass.GetGCPtr()==Parent&&BP->SkeletonGeneratedClass.GetGCPtr()==Skeleton&&BP->GeneratedClass.GetGCPtr()==Generated):(BP->ParentClass==Parent&&BP->SkeletonGeneratedClass==Skeleton&&BP->GeneratedClass==Generated))&&BP->BlueprintType==BPTYPE_Normal
            &&!BP->bBeingCompiled&&!BP->bQueuedForCompilation&&!BP->bIsRegeneratingOnLoad&&!BP->bSuppressStructurallyModified
            &&BP->GetFlags()==BPFlags&&Graph->GetFlags()==GraphFlags&&Skeleton->GetFlags()==SkeletonFlags&&Skeleton->GetStructureSize()==SkeletonSize&&Skeleton->ClassGeneratedBy==BP
            &&BP->Status.GetValue()==Status&&BP->GetPackage()->IsDirty()==Dirty&&Native->GetPathName()==TypePath&&Native->GetFlags()==NativeFlags&&Native->GetPackage()==Package
            &&Package->GetName()==(Object?(NiagaraSystem?TEXT("/Script/Niagara"):TEXT("/Script/Engine")):TEXT("/Script/CoreUObject"))&&Package->GetPackageFlags()==16U&&!Package->IsDirty()
            &&(Object?(NativeClass->ClassGeneratedBy==nullptr&&NativeClass->HasAnyClassFlags(CLASS_Native)&&!NativeClass->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)
                &&uint32(NativeClass->GetClassFlags())==NativeTypeFlags&&NativeClass->GetStructureSize()==NativeSize):
                (Color==TBaseStructure<FLinearColor>::Get()&&uint32(Color->StructFlags)==NativeTypeFlags&&Color->GetStructureSize()==NativeSize&&NativeSize==sizeof(FLinearColor)));
    };
    if(!Basic()||!Property||Property->GetOwnerClass()!=Skeleton||Property->ArrayDim!=1||!Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
        ||(bSetter&&Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly))||Property->HasAnyPropertyFlags(CPF_EditorOnly|CPF_Parm|CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_ExportObject|CPF_PersistentInstance)
        ||(Object?(!Obj||Obj->GetClass()!=FObjectProperty::StaticClass()||!Obj->PropertyClass.IsResolved()||Obj->PropertyClass!=NativeClass
            ||Expected.bIsUObjectWrapper!=Obj->HasAnyPropertyFlags(CPF_UObjectWrapper)):
            (!Struct||!Struct->Struct.IsResolved()||Struct->Struct!=Color||Expected.bIsUObjectWrapper)))
    {Fail(R,TEXT("graph_material_value_property_unsupported"),TEXT("The current own visible scalar strong object/LinearColor skeleton property, native constraint, wrapper and access must match exactly."));return R;}
    const auto PropertyFlags=Property->GetPropertyFlags();const int32 Offset=Property->GetOffset_ForInternal(),Size=Property->GetSize();
    const auto Current=[&]()
    {
        bool Good=Basic();const int32 I=Good?FBlueprintEditorUtils::FindNewVariableIndex(BP,Name):INDEX_NONE;Good=Good&&I!=INDEX_NONE;
        if(Good)
        {
            const auto& D=BP->NewVariables[I];Good=D.VarName==Description.VarName&&D.VarGuid==Description.VarGuid&&D.DefaultValue==Description.DefaultValue
                &&D.VarType==Expected&&D.VarType.bIsUObjectWrapper==Expected.bIsUObjectWrapper&&D.VarType.bSerializeAsSinglePrecisionFloat==Expected.bSerializeAsSinglePrecisionFloat
                &&D.VarType.PinValueType.bTerminalIsUObjectWrapper==Expected.PinValueType.bTerminalIsUObjectWrapper&&Skeleton->FindPropertyByName(Name)==Property
                &&Property->GetOwnerClass()==Skeleton&&Property->ArrayDim==1&&Property->GetPropertyFlags()==PropertyFlags&&Property->GetOffset_ForInternal()==Offset&&Property->GetSize()==Size
                &&(Object?(Obj->PropertyClass.IsResolved()&&Obj->PropertyClass==NativeClass):(Struct->Struct.IsResolved()&&Struct->Struct==Color));
        }
        if(!Good){R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("observation_incomplete"),true);Fail(R,TEXT("graph_material_value_scope_changed"),TEXT("Current selected declaration/property/native type/parent/graph roles changed; no stale value access, retry or repair."));}
        return Good;
    };
    if(!Current())return R;
    const auto* Schema=Cast<UEdGraphSchema_K2>(Graph->GetSchema());if(!Current())return R;
    if(!Schema||Schema->GetClass()!=UEdGraphSchema_K2::StaticClass()){Fail(R,TEXT("graph_material_value_schema_unsupported"),TEXT("Exact standard K2 schema required."));return R;}
    FEdGraphPinType Actual;const bool Converted=Schema->ConvertPropertyToPinType(Property,Actual);if(!Current())return R;
    if(!Converted||Actual!=Expected||Actual.bIsUObjectWrapper!=Expected.bIsUObjectWrapper||Actual.bSerializeAsSinglePrecisionFloat!=Expected.bSerializeAsSinglePrecisionFloat
        ||Actual.PinValueType.bTerminalIsUObjectWrapper!=Expected.PinValueType.bTerminalIsUObjectWrapper)
    {Fail(R,TEXT("graph_member_skeleton_mismatch"),TEXT("Full native material/color declaration and skeleton pin types differ."));return R;}
    const auto Readable=FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP,Property);if(!Current())return R;
    if(Readable!=FBlueprintEditorUtils::EPropertyReadableState::Readable){Fail(R,TEXT("graph_member_access_rejected"),TEXT("The selected own member is not Blueprint-readable."));return R;}
    const UObject* CDO=nullptr;
    if(Object)
    {
        const auto Handle=MaterialValueCDOHandle(NativeClass);
        if(!Handle.IsResolved()){Fail(R,TEXT("graph_material_value_native_source_unavailable"),TEXT("Existing native CDO handle must be resolved; no hidden reference load."));return R;}
        if(Handle!=nullptr)CDO=FindObject<UObject>(nullptr,*Handle.GetPathName());
        if(!Current()||MaterialValueCDOHandle(NativeClass)!=Handle||(Handle!=nullptr&&!CDO)||Handle!=CDO)
        {Fail(R,TEXT("graph_material_value_native_source_changed"),TEXT("Existing optional CDO lookup must match the unchanged shallow native handle."));return R;}
    }
    if(CDO&&(!Loaded(CDO)||CDO->GetClass()!=NativeClass||!CDO->HasAnyFlags(RF_ClassDefaultObject)))
    {Fail(R,TEXT("graph_material_value_native_source_unavailable"),TEXT("Existing native CDO role is incomplete; no creation or warming is requested."));return R;}
    TStrongObjectPtr<const UObject> KeepCDO(CDO);const FString CDOPath=CDO?CDO->GetPathName():FString();const EObjectFlags CDOFlags=CDO?CDO->GetFlags():RF_NoFlags;
    const auto Source=[&]()
    {
        auto J=MakeShared<FJsonObject>();if(!Current())return J;
        if(Object)
        {
            const auto Now=MaterialValueCDOHandle(NativeClass);if(!Current())return J;
            if(!Now.IsResolved()||Now!=CDO||(CDO&&(!Loaded(CDO)||CDO->GetPathName()!=CDOPath||CDO->GetFlags()!=CDOFlags||CDO->GetClass()!=NativeClass)))
            {Fail(R,TEXT("graph_material_value_native_source_changed"),TEXT("Captured native class/existing CDO metadata changed; no value sampling or next SDK."));return J;}
            J->SetStringField(TEXT("class_path"),TypePath);J->SetBoolField(TEXT("existing_cdo_available"),CDO!=nullptr);J->SetStringField(TEXT("existing_cdo_path"),CDOPath);
            J->SetStringField(TEXT("existing_cdo_flags_string"),LexToString(uint32(CDOFlags)));J->SetBoolField(TEXT("cdo_creation_requested"),false);J->SetBoolField(TEXT("cdo_values_sampled"),false);
        }
        else
        {
            J->SetStringField(TEXT("struct_path"),TypePath);TArray<TSharedPtr<FJsonValue>> Fields;
            const TCHAR* Names[]={TEXT("R"),TEXT("G"),TEXT("B"),TEXT("A")};const int32 Offsets[]={STRUCT_OFFSET(FLinearColor,R),STRUCT_OFFSET(FLinearColor,G),STRUCT_OFFSET(FLinearColor,B),STRUCT_OFFSET(FLinearColor,A)};
            int32 Count=0;for(TFieldIterator<FProperty> It(Color,EFieldIterationFlags::None);It;++It){++Count;}
            if(Count!=4){Fail(R,TEXT("graph_material_value_native_source_changed"),TEXT("Native LinearColor must retain exactly four own float fields."));return J;}
            for(int32 I=0;I<4;++I)
            {
                const FProperty* F=Color->FindPropertyByName(Names[I]);
                if(!F||F->GetClass()!=FFloatProperty::StaticClass()||F->GetOwnerStruct()!=Color||F->ArrayDim!=1||F->GetOffset_ForInternal()!=Offsets[I]||F->GetSize()!=sizeof(float))
                {Fail(R,TEXT("graph_material_value_native_source_changed"),TEXT("Native LinearColor field owner/layout differs."));return J;}
                auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),Names[I]);V->SetStringField(TEXT("owner"),TypePath);V->SetStringField(TEXT("class"),F->GetClass()->GetName());
                V->SetNumberField(TEXT("offset"),F->GetOffset_ForInternal());V->SetNumberField(TEXT("size"),F->GetSize());V->SetStringField(TEXT("flags"),LexToString(uint64(F->GetPropertyFlags())));Fields.Add(MakeShared<FJsonValueObject>(V));
            }
            J->SetArrayField(TEXT("native_fields"),Fields);
        }
        if(!Current())return J;J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);
        J->SetStringField(TEXT("type_path"),TypePath);J->SetStringField(TEXT("type_kind"),Object?TEXT("object"):TEXT("linear_color"));J->SetStringField(TEXT("package_path"),Package->GetName());
        J->SetStringField(TEXT("package_flags_string"),LexToString(uint32(Package->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Package->IsDirty());
        J->SetStringField(TEXT("type_flags_string"),LexToString(NativeTypeFlags));J->SetNumberField(TEXT("type_structure_size"),NativeSize);return J;
    };
    const auto Before=Source();if(!Current()||!Before->HasField(TEXT("observations_complete")))return R;R->SetObjectField(TEXT("native_value_source_before"),Before);
    const auto ParentProof=InspectBlueprintParent(Parent->GetPathName());if(!Current())return R;R->SetObjectField(TEXT("parent_inspection"),ParentProof);
    if(!ParentProof->GetBoolField(TEXT("success"))){Fail(R,TEXT("graph_member_parent_unsupported"),TEXT("Target parent failed the existing runtime parent guard; Actor and ActorComponent use the same existing rules."));return R;}
    FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&Actual,&Actual,nullptr,PPF_None,nullptr);if(!Current())return R;
    // Only the selected own member's current get/set roles are observed. No runtime/default-object value is read.
    const auto Inventory=Graph->Nodes;TArray<TStrongObjectPtr<UEdGraphNode>> HeldNodes;TArray<TSharedPtr<FJsonValue>> MemberNodes;
    struct FSelectedRole {UK2Node_Variable* Node;FString Path;FGuid Guid;EObjectFlags Flags;FMemberReference Reference;TArray<UEdGraphPin*> Pins;UEdGraphPin* Value;FGuid ValueId;FEdGraphPinType Type;uint32 ValueFlags;bool Get;};
    TArray<FSelectedRole> SelectedRoles;FMemberReference SelfReference;SelfReference.SetSelfMember(Name,Description.VarGuid);
    const auto InventoryCurrent=[&]()
    {
        if(!Current()||Graph->Nodes.Num()!=Inventory.Num())return false;
        for(int32 I=0;I<Inventory.Num();++I)if(Graph->Nodes[I]!=Inventory[I])return false;
        return true;
    };
    if(Inventory.Num()>2048){Fail(R,TEXT("graph_material_value_node_budget"),TEXT("The selected member observation uses the existing bounded graph inventory."));return R;}
    for(const auto& Handle:Inventory)
    {
        if(!InventoryCurrent()||!Handle.IsResolved()){Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("Current graph inventory must remain resolved; no hidden node load."));return R;}
        const FString NodePath=Handle.GetPathName();
        UEdGraphNode* N=FindObject<UEdGraphNode>(nullptr,*NodePath);
        if(!InventoryCurrent()||!Loaded(N)||Handle!=N){Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("Current graph node identity differs."));return R;}
        HeldNodes.Emplace(N);
        const bool Get=N->GetClass()==UK2Node_VariableGet::StaticClass(),Set=N->GetClass()==UK2Node_VariableSet::StaticClass();
        if(!Get&&!Set)continue;
        UK2Node_Variable* Variable=CastChecked<UK2Node_Variable>(N);const FMemberReference Reference=Variable->VariableReference;
        if(Reference.GetMemberName()!=Name||Reference.GetMemberGuid()!=Description.VarGuid)continue;
        const FGuid NodeGuid=N->NodeGuid;const EObjectFlags NodeFlags=N->GetFlags();const auto Pins=N->Pins;
        const auto NodeCurrent=[&]()
        {
            if(!InventoryCurrent()||!Loaded(N)||N->GetPathName()!=NodePath||N->GetOuter()!=Graph||N->GetGraph()!=Graph||N->NodeGuid!=NodeGuid||N->GetFlags()!=NodeFlags
                ||N->GetClass()!=(Get?UK2Node_VariableGet::StaticClass():UK2Node_VariableSet::StaticClass())||N->Pins.Num()!=Pins.Num()
                ||!Variable->VariableReference.IsSameReference(Reference)||Variable->VariableReference.IsDeprecated()!=Reference.IsDeprecated())return false;
            for(int32 I=0;I<Pins.Num();++I)if(N->Pins[I]!=Pins[I])return false;
            return true;
        };
        if(!Reference.IsSameReference(SelfReference)||Reference.IsDeprecated()||!NodeGuid.IsValid()||!NodeCurrent())
        {Fail(R,TEXT("graph_material_value_node_reference_invalid"),TEXT("Selected own member nodes must retain their exact nondeprecated self GUID/name and current graph role."));return R;}
        FProperty* Resolved=Variable->GetPropertyForVariableFromSkeleton();
        if(!NodeCurrent()){Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("Selected node changed during property resolution; no stale pin access."));return R;}
        if(Resolved!=Property){Fail(R,TEXT("graph_material_value_node_property_mismatch"),TEXT("Selected node must resolve to the exact current own skeleton property."));return R;}
        UEdGraphPin* ValuePin=nullptr;
        for(UEdGraphPin* P:Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N){Fail(R,TEXT("graph_material_value_node_pin_invalid"),TEXT("Current physical pin inventory is incomplete."));return R;}
            if(P->PinName==Name&&P->Direction==(Get?EGPD_Output:EGPD_Input)){if(ValuePin){Fail(R,TEXT("graph_material_value_node_pin_invalid"),TEXT("Selected physical value pin is ambiguous."));return R;}ValuePin=P;}
        }
        const auto Whole=[](const UEdGraphPin* P,UEdGraphNode* Owner)
        {return P&&!P->WasTrashed()&&P->GetOwningNodeUnchecked()==Owner&&P->PinId.IsValid()&&!P->bHidden&&!P->bOrphanedPin&&!P->bNotConnectable&&!P->bDeprecated&&!P->ParentPin&&P->SubPins.IsEmpty();};
        const auto PhysicalFlags=[](const UEdGraphPin* P)->uint32
        {return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)|(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)|(uint32(P->ShouldSavePinIfOrphaned())<<9);};
        if(!Whole(ValuePin,N)||ValuePin->PinType!=Expected||ValuePin->PinType.bIsUObjectWrapper!=Expected.bIsUObjectWrapper
            ||ValuePin->PinType.bSerializeAsSinglePrecisionFloat!=Expected.bSerializeAsSinglePrecisionFloat||ValuePin->PinType.PinValueType.bTerminalIsUObjectWrapper!=Expected.PinValueType.bTerminalIsUObjectWrapper)
        {Fail(R,TEXT("graph_material_value_node_pin_invalid"),TEXT("Selected physical value pin must match the full own descriptor type."));return R;}
        const FGuid ValuePinId=ValuePin->PinId;const FEdGraphPinType ValueType=ValuePin->PinType;const uint32 ValueFlags=PhysicalFlags(ValuePin);
        SelectedRoles.Add({Variable,NodePath,NodeGuid,NodeFlags,Reference,Pins,ValuePin,ValuePinId,ValueType,ValueFlags,Get});
        FString ValueTypeText;FEdGraphPinType::StaticStruct()->ExportText(ValueTypeText,&ValueType,&ValueType,nullptr,PPF_None,nullptr);
        if(!NodeCurrent()){Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("Selected node/pin changed during type export; no stale pin access."));return R;}
        if(!Whole(ValuePin,N)||ValuePin->PinId!=ValuePinId||ValuePin->PinName!=Name||ValuePin->Direction!=(Get?EGPD_Output:EGPD_Input)||PhysicalFlags(ValuePin)!=ValueFlags||ValuePin->PinType!=ValueType||ValuePin->PinType.bIsUObjectWrapper!=ValueType.bIsUObjectWrapper
            ||ValuePin->PinType.bSerializeAsSinglePrecisionFloat!=ValueType.bSerializeAsSinglePrecisionFloat||ValuePin->PinType.PinValueType.bTerminalIsUObjectWrapper!=ValueType.PinValueType.bTerminalIsUObjectWrapper)
        {Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("Selected value pin role changed during type export."));return R;}
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),NodePath);J->SetStringField(TEXT("node_guid"),NodeGuid.ToString(EGuidFormats::Digits));
        J->SetStringField(TEXT("node_class_path"),N->GetClass()->GetPathName());J->SetStringField(TEXT("outer_path"),GraphPath);J->SetStringField(TEXT("graph_path"),GraphPath);J->SetBoolField(TEXT("graph_member"),true);
        J->SetStringField(TEXT("member_name"),Name.ToString());J->SetStringField(TEXT("member_guid"),Description.VarGuid.ToString(EGuidFormats::Digits));J->SetBoolField(TEXT("member_self_context"),true);
        J->SetBoolField(TEXT("member_local_scope"),false);J->SetBoolField(TEXT("member_deprecated"),false);J->SetStringField(TEXT("property_path"),Property->GetPathName());
        J->SetStringField(TEXT("value_pin_name"),Name.ToString());J->SetStringField(TEXT("value_pin_direction"),Get?TEXT("output"):TEXT("input"));J->SetStringField(TEXT("value_pin_type_text"),ValueTypeText);
        J->SetStringField(TEXT("value_pin_id"),ValuePinId.ToString(EGuidFormats::Digits));J->SetBoolField(TEXT("value_pin_whole_current_verified"),true);J->SetBoolField(TEXT("value_pin_trashed"),false);J->SetBoolField(TEXT("value_pin_hidden"),false);J->SetBoolField(TEXT("value_pin_orphaned"),false);J->SetBoolField(TEXT("value_pin_split"),false);J->SetStringField(TEXT("value_pin_owner_path"),NodePath);J->SetNumberField(TEXT("value_pin_flags"),ValueFlags);MemberNodes.Add(MakeShared<FJsonValueObject>(J));
    }
    if(!InventoryCurrent()){Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("Selected member observation ended with a changed graph inventory."));return R;}
    // Recheck earlier selected rows after later property/type-export callbacks. No default object or runtime value is accessed.
    for(const auto& Role:SelectedRoles)
    {
        UK2Node_Variable* N=Role.Node;
        if(!InventoryCurrent()||!Loaded(N)||N->GetPathName()!=Role.Path||N->GetOuter()!=Graph||N->GetGraph()!=Graph||N->NodeGuid!=Role.Guid||N->GetFlags()!=Role.Flags
            ||N->GetClass()!=(Role.Get?UK2Node_VariableGet::StaticClass():UK2Node_VariableSet::StaticClass())||!N->VariableReference.IsSameReference(Role.Reference)||N->VariableReference.IsDeprecated()!=Role.Reference.IsDeprecated()||N->Pins.Num()!=Role.Pins.Num())
        {Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("A selected earlier member node changed before the final observation."));return R;}
        for(int32 I=0;I<Role.Pins.Num();++I)if(N->Pins[I]!=Role.Pins[I]){Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("A selected physical inventory changed before the final observation."));return R;}
        const UEdGraphPin* P=Role.Value;
        if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||P->PinId!=Role.ValueId||P->PinName!=Name||P->Direction!=(Role.Get?EGPD_Output:EGPD_Input)
            ||P->bHidden||P->bOrphanedPin||P->bNotConnectable||P->bDeprecated||P->ParentPin||!P->SubPins.IsEmpty()||P->PinType!=Role.Type
            ||P->PinType.bIsUObjectWrapper!=Role.Type.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=Role.Type.bSerializeAsSinglePrecisionFloat||P->PinType.PinValueType.bTerminalIsUObjectWrapper!=Role.Type.PinValueType.bTerminalIsUObjectWrapper
            ||(uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)|(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)|(uint32(P->ShouldSavePinIfOrphaned())<<9))!=Role.ValueFlags)
        {Fail(R,TEXT("graph_material_value_node_scope_changed"),TEXT("A selected whole value-pin identity/type/flags changed before the final observation."));return R;}
    }
    R->SetArrayField(TEXT("native_value_member_nodes"),MemberNodes);
    const auto After=Source();if(!Current()||!After->HasField(TEXT("observations_complete")))return R;
    const bool Same=FJsonValue::CompareEqual(FJsonValueObject(Before),FJsonValueObject(After));R->SetObjectField(TEXT("native_value_source_after"),After);R->SetBoolField(TEXT("native_value_source_observed_unchanged"),Same);
    if(!Same){Fail(R,TEXT("graph_material_value_native_source_changed"),TEXT("Native source metadata differed during the read-only plan."));return R;}
    R->SetStringField(TEXT("variable_name"),VariableName);R->SetStringField(TEXT("variable_guid"),Description.VarGuid.ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("setter"),bSetter);
    R->SetStringField(TEXT("native_type_path"),TypePath);R->SetStringField(TEXT("value_kind"),Object?TEXT("object"):TEXT("linear_color"));
    R->SetStringField(TEXT("property_path"),Property->GetPathName());R->SetStringField(TEXT("property_owner_class_path"),Skeleton->GetPathName());R->SetStringField(TEXT("skeleton_class_path"),Skeleton->GetPathName());
    R->SetStringField(TEXT("property_class"),Property->GetClass()->GetName());R->SetNumberField(TEXT("property_array_dim"),Property->ArrayDim);R->SetStringField(TEXT("property_flags_string"),LexToString(uint64(PropertyFlags)));
    R->SetStringField(TEXT("expected_pin_type_text"),TypeText);R->SetObjectField(TEXT("pin_type"),PinTypeObservation(Actual));R->SetStringField(TEXT("pin_category"),Actual.PinCategory.ToString());R->SetStringField(TEXT("pin_subcategory"),Actual.PinSubCategory.ToString());
    R->SetBoolField(TEXT("pin_is_uobject_wrapper"),Actual.bIsUObjectWrapper);R->SetBoolField(TEXT("pin_serialize_single_precision"),Actual.bSerializeAsSinglePrecisionFloat);R->SetBoolField(TEXT("pin_terminal_is_uobject_wrapper"),Actual.PinValueType.bTerminalIsUObjectWrapper);
    R->SetStringField(TEXT("stored_default_before"),Description.DefaultValue);R->SetStringField(TEXT("stored_default_after"),Description.DefaultValue);R->SetBoolField(TEXT("stored_default_observed_unchanged"),true);
    R->SetBoolField(TEXT("native_value_type_identity_verified"),true);R->SetBoolField(TEXT("native_value_property_identity_verified"),true);
    R->SetBoolField(TEXT("runtime_value_read_attempted"),false);R->SetBoolField(TEXT("default_write_attempted"),false);R->SetBoolField(TEXT("runtime_evaluation_verified"),false);
    R->SetStringField(TEXT("scope"),TEXT("Exact own native scalar material/texture/NiagaraSystem/TextureRenderTarget2D/LinearColor metadata plan; current property/GUID/type/default and source metadata only. No runtime values/default edits, arbitrary resources, class loads, CDO creation, global callback purity, compile/save or pipeline execution."));
    if(!Current())return R;Complete(R,Graph);if(!Current())R->SetBoolField(TEXT("success"),false);return R;
}


// LB: exact value-array member authoring. No array/default object values are read.
TSharedRef<FJsonObject> ReadNativeVectorArrayPlan(UEdGraph* Graph,const FString& VariableName,bool bSetter,UClass* External,const TSharedRef<FJsonObject>& R,bool MapStringVector=false,bool MapStringDouble=false)
{
    MapStringVector|=MapStringDouble;const bool ExternalMember=External!=nullptr;R->SetStringField(TEXT("member_scope"),MapStringDouble?TEXT("own_native_string_double_map"):MapStringVector?TEXT("own_native_string_vector_map"):ExternalMember?TEXT("external_native_vector_array"):TEXT("own_native_vector4_array"));
    if(MapStringVector){R->SetBoolField(MapStringDouble?TEXT("native_string_double_map_member"):TEXT("native_string_vector_map_member"),true);if(ExternalMember){Fail(R,TEXT("graph_map_external_member_unsupported"),TEXT("String/Vector Maps require an exact own declared member; external/inherited routes remain unsupported."));return R;}}
    const auto Loaded=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    const auto Globals=[&](){return IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();};
    if(!Globals()||!Loaded(Graph)){Fail(R,TEXT("graph_vector_array_context_invalid"),TEXT("Exact loaded K2 graph on the idle Editor game thread required before value-array planning."));return R;}
    UBlueprint* BP=FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if(!Loaded(BP)||!SafeSelector(VariableName)||(ExternalMember&&bSetter)){Fail(R,TEXT("graph_vector_array_scope_unsupported"),TEXT("Requires an own declared Vector4 array, or a readable external own Vector/Vector4 array getter; external setters are not admitted."));return R;}
    TStrongObjectPtr<UBlueprint> KeepBP(BP);TStrongObjectPtr<UEdGraph> KeepGraph(Graph);TStrongObjectPtr<UEditorEngine> KeepEditor(GEditor);
    const auto ClassRole=[](const TObjectPtr<UClass>& H)->UClass*{if(!H.IsResolved()||H==nullptr)return nullptr;UClass* C=FindObject<UClass>(nullptr,*H.GetPathName());return H==C?C:nullptr;};
    UClass* TargetSkeleton=ClassRole(BP->SkeletonGeneratedClass.GetGCPtr()),*TargetGenerated=ClassRole(BP->GeneratedClass.GetGCPtr()),*Parent=ClassRole(BP->ParentClass.GetGCPtr());
    TStrongObjectPtr<UClass> KeepTS(TargetSkeleton),KeepTG(TargetGenerated),KeepParent(Parent),KeepExternal(External);
    UBlueprint* Source=BP;
    if(ExternalMember)
    {
        if(!Loaded(External)||!External->ClassGeneratedBy.IsResolved()){Fail(R,TEXT("graph_vector_array_external_source_invalid"),TEXT("External generated source must already be resolved and loaded."));return R;}
        const auto H=External->ClassGeneratedBy;Source=FindObject<UBlueprint>(nullptr,*H.GetPathName());
        if(!Globals()||!Loaded(Source)||H!=Source||Source==BP){Fail(R,TEXT("graph_vector_array_external_source_invalid"),TEXT("External source must be a distinct exact loaded Blueprint; no hidden source loads."));return R;}
    }
    TStrongObjectPtr<UBlueprint> KeepSource(Source);
    UClass* Skeleton=ClassRole(Source->SkeletonGeneratedClass.GetGCPtr()),*Generated=ClassRole(Source->GeneratedClass.GetGCPtr()),*SourceParent=ClassRole(Source->ParentClass.GetGCPtr());
    TStrongObjectPtr<UClass> KeepSkeleton(Skeleton),KeepGenerated(Generated),KeepSourceParent(SourceParent);
    if(!Loaded(TargetSkeleton)||!Loaded(TargetGenerated)||!Loaded(Parent)||!Loaded(Skeleton)||!Loaded(Generated)||!Loaded(SourceParent)
        ||(ExternalMember&&Generated!=External)){Fail(R,TEXT("graph_vector_array_current_class_required"),TEXT("Current target/source skeleton, generated and parent roles are required."));return R;}
    const FName Name(*VariableName);const int32 Index=FBlueprintEditorUtils::FindNewVariableIndex(Source,Name);
    if(Index==INDEX_NONE){Fail(R,TEXT("graph_member_not_declared"),TEXT("The selected value-array must be declared directly by the exact source Blueprint."));return R;}
    const FBPVariableDescription Description=Source->NewVariables[Index];const FEdGraphPinType Expected=Description.VarType;
    UScriptStruct* Native=MapStringDouble?nullptr:Cast<UScriptStruct>(MapStringVector?Expected.PinValueType.TerminalSubCategoryObject.Get():Expected.PinSubCategoryObject.Get());
    const bool Four=!MapStringVector&&Native==TBaseStructure<FVector4>::Get(),Three=!MapStringDouble&&(ExternalMember||MapStringVector)&&Native==TBaseStructure<FVector>::Get();
    if(Description.VarName.ToString()!=VariableName||!Description.VarGuid.IsValid()||(!MapStringDouble&&((!Four&&!Three)||!Loaded(Native)))
        ||(MapStringVector?(Expected.PinCategory!=UEdGraphSchema_K2::PC_String||Expected.ContainerType!=EPinContainerType::Map||Expected.PinSubCategoryObject.IsValid()
            ||(MapStringDouble?(Expected.PinValueType.TerminalCategory!=UEdGraphSchema_K2::PC_Real||Expected.PinValueType.TerminalSubCategory!=UEdGraphSchema_K2::PC_Double||Expected.PinValueType.TerminalSubCategoryObject!=nullptr):(Expected.PinValueType.TerminalCategory!=UEdGraphSchema_K2::PC_Struct||!Expected.PinValueType.TerminalSubCategory.IsNone()))||Expected.PinValueType.bTerminalIsConst||Expected.PinValueType.bTerminalIsWeakPointer
            ||!(Description.PropertyFlags&CPF_Transient)||!Description.DefaultValue.IsEmpty()):(Expected.PinCategory!=UEdGraphSchema_K2::PC_Struct||Expected.ContainerType!=EPinContainerType::Array||Expected.PinValueType!=FEdGraphTerminalType()))
        ||!Expected.PinSubCategory.IsNone()||Expected.bIsReference||Expected.bIsConst||Expected.bIsWeakPointer||Expected.bIsUObjectWrapper||Expected.bSerializeAsSinglePrecisionFloat||Expected.PinValueType.bTerminalIsUObjectWrapper)
    {Fail(R,TEXT("graph_vector_array_type_unsupported"),TEXT("Only exact own CoreUObject.Vector4[] and external getter CoreUObject.Vector[]/Vector4[] descriptor types are supported; no aliases/nested/ref/soft/weak types."));return R;}
    TStrongObjectPtr<UScriptStruct> KeepNative(Native);UPackage* NativePackage=MapStringDouble?FindPackage(nullptr,TEXT("/Script/CoreUObject")):Native->GetPackage();TStrongObjectPtr<UPackage> KeepNativePackage(NativePackage);if(MapStringDouble&&!Loaded(NativePackage)){Fail(R,TEXT("graph_map_scalar_source_unavailable"),TEXT("Loaded CoreUObject package is required for the exact double Map descriptor; no loader or value access is dispatched."));return R;}
    const FString TypePath=MapStringDouble?TEXT(""):Four?TEXT("/Script/CoreUObject.Vector4"):TEXT("/Script/CoreUObject.Vector"),BPPath=BP->GetPathName(),GraphPath=Graph->GetPathName(),SourcePath=Source->GetPathName();
    const int32 ExpectedSize=MapStringDouble?sizeof(double):Four?sizeof(FVector4):sizeof(FVector),ExpectedAlign=MapStringDouble?alignof(double):Four?alignof(FVector4):alignof(FVector),Axes=MapStringDouble?0:Four?4:3;const FGuid GraphGuid=Graph->GraphGuid;const auto Undo=GUndo;
    struct FRole{TStrongObjectPtr<UObject> Hold;UClass* Class;UObject* Outer;UPackage* Package;FString Path;EObjectFlags Flags;uint32 TypeFlags;int32 Size;};TArray<FRole> Roles;
    for(UObject* O:{static_cast<UObject*>(BP),static_cast<UObject*>(Graph),static_cast<UObject*>(Source),static_cast<UObject*>(TargetSkeleton),static_cast<UObject*>(TargetGenerated),static_cast<UObject*>(Parent),static_cast<UObject*>(Skeleton),static_cast<UObject*>(Generated),static_cast<UObject*>(SourceParent),static_cast<UObject*>(Native),static_cast<UObject*>(NativePackage)})
    {
        if(MapStringDouble&&!O)continue;
        if(!Loaded(O)){Fail(R,TEXT("graph_vector_array_role_unavailable"),TEXT("A required current object is unavailable."));return R;}
        FRole Role;Role.Hold.Reset(O);Role.Class=O->GetClass();Role.Outer=O->GetOuter();Role.Package=O->GetPackage();Role.Path=O->GetPathName();Role.Flags=O->GetFlags();Role.TypeFlags=0;Role.Size=0;
        if(const UClass* C=Cast<UClass>(O)){Role.TypeFlags=uint32(C->GetClassFlags());Role.Size=C->GetStructureSize();}
        Roles.Add(MoveTemp(Role));
    }
    const auto TargetDescriptions=BP->NewVariables,SourceDescriptions=Source->NewVariables;const auto SourceGeneratedDescriptions=Source->GeneratedVariables;
    const auto TargetStatus=BP->Status.GetValue(),SourceStatus=Source->Status.GetValue();UPackage* TargetPackage=BP->GetPackage(),*SourcePackage=Source->GetPackage();
    TStrongObjectPtr<UPackage> KeepTP(TargetPackage),KeepSP(SourcePackage);const bool TargetDirty=TargetPackage->IsDirty(),SourceDirty=SourcePackage->IsDirty();
    // Nested Map connection plans have no ResolveLoadedGraph receipt; preserve an existing baseline.
    if(MapStringVector&&!R->HasField(TEXT("dirty_before")))R->SetBoolField(TEXT("dirty_before"),TargetDirty);
    const uint32 StructFlags=MapStringDouble?0U:uint32(Native->StructFlags);const int32 StructAlign=MapStringDouble?alignof(double):Native->GetMinAlignment();
    const auto SameType=[](const FEdGraphPinType& A,const FEdGraphPinType& B){return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;};
    const auto SameDescriptions=[&](const TArray<FBPVariableDescription>& A,const TArray<FBPVariableDescription>& B)
    {
        if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I){const auto& X=A[I];const auto& Y=B[I];
            if(X.VarName!=Y.VarName||X.VarGuid!=Y.VarGuid||!SameType(X.VarType,Y.VarType)||X.FriendlyName!=Y.FriendlyName||!X.Category.IdenticalTo(Y.Category)||X.PropertyFlags!=Y.PropertyFlags
                ||X.RepNotifyFunc!=Y.RepNotifyFunc||X.ReplicationCondition!=Y.ReplicationCondition||X.DefaultValue!=Y.DefaultValue||X.MetaDataArray.Num()!=Y.MetaDataArray.Num())return false;
            for(int32 M=0;M<X.MetaDataArray.Num();++M)if(X.MetaDataArray[M].DataKey!=Y.MetaDataArray[M].DataKey||X.MetaDataArray[M].DataValue!=Y.MetaDataArray[M].DataValue)return false;
        }return true;
    };
    const auto Basic=[&]()
    {
        if(!Globals()||GUndo!=Undo||GEditor!=KeepEditor.Get()||!Loaded(KeepEditor.Get())||!Loaded(TargetPackage)||!Loaded(SourcePackage))return false;
        for(const auto& Role:Roles){const UObject* O=Role.Hold.Get();if(!Loaded(O)||O->GetClass()!=Role.Class||O->GetOuter()!=Role.Outer||O->GetPackage()!=Role.Package||O->GetPathName()!=Role.Path||O->GetFlags()!=Role.Flags)return false;
            if(const UClass* C=Cast<UClass>(O))if(uint32(C->GetClassFlags())!=Role.TypeFlags||C->GetStructureSize()!=Role.Size)return false;}
        return BP->GetPathName()==BPPath&&Graph->GetPathName()==GraphPath&&Graph->GetOuter()==BP&&Graph->GraphGuid==GraphGuid
            &&(BP->ParentClass.GetGCPtr().IsResolved()&&BP->ParentClass.GetGCPtr()==Parent)&&(BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()&&BP->SkeletonGeneratedClass.GetGCPtr()==TargetSkeleton)&&(BP->GeneratedClass.GetGCPtr().IsResolved()&&BP->GeneratedClass.GetGCPtr()==TargetGenerated)
            &&(Source->ParentClass.GetGCPtr().IsResolved()&&Source->ParentClass.GetGCPtr()==SourceParent)&&(Source->SkeletonGeneratedClass.GetGCPtr().IsResolved()&&Source->SkeletonGeneratedClass.GetGCPtr()==Skeleton)&&(Source->GeneratedClass.GetGCPtr().IsResolved()&&Source->GeneratedClass.GetGCPtr()==Generated)
            &&Skeleton->ClassGeneratedBy.IsResolved()&&Generated->ClassGeneratedBy.IsResolved()&&Skeleton->ClassGeneratedBy==Source&&Generated->ClassGeneratedBy==Source&&BP->BlueprintType==BPTYPE_Normal&&Source->BlueprintType==BPTYPE_Normal
            &&!BP->bBeingCompiled&&!BP->bQueuedForCompilation&&!BP->bIsRegeneratingOnLoad&&!BP->bSuppressStructurallyModified
            &&!Source->bBeingCompiled&&!Source->bQueuedForCompilation&&!Source->bIsRegeneratingOnLoad&&!Source->bSuppressStructurallyModified
            &&BP->GetPackage()==TargetPackage&&Source->GetPackage()==SourcePackage&&TargetPackage->IsDirty()==TargetDirty&&SourcePackage->IsDirty()==SourceDirty
            &&BP->Status.GetValue()==TargetStatus&&Source->Status.GetValue()==SourceStatus
            &&SameDescriptions(BP->NewVariables,TargetDescriptions)&&SameDescriptions(Source->NewVariables,SourceDescriptions)&&SameDescriptions(Source->GeneratedVariables,SourceGeneratedDescriptions)
            &&(MapStringDouble?Native==nullptr:(Native->GetPathName()==TypePath&&Native->GetPackage()==NativePackage&&Native->GetStructureSize()==ExpectedSize&&Native->GetMinAlignment()==StructAlign&&StructAlign==ExpectedAlign&&uint32(Native->StructFlags)==StructFlags))
            &&NativePackage->GetName()==TEXT("/Script/CoreUObject")&&NativePackage->GetPackageFlags()==16U&&!NativePackage->IsDirty();
    };
    if(!Basic()){Fail(R,TEXT("graph_vector_array_scope_changed"),TEXT("Current source/target/native metadata roles changed before property binding."));return R;}
    struct FField{const FProperty* P;FName Name;EPropertyFlags Flags;int32 Offset,Size;};TArray<FField> Fields;const TCHAR* AxisNames[]={TEXT("X"),TEXT("Y"),TEXT("Z"),TEXT("W")};
    const int32 AxisOffsets4[]={STRUCT_OFFSET(FVector4,X),STRUCT_OFFSET(FVector4,Y),STRUCT_OFFSET(FVector4,Z),STRUCT_OFFSET(FVector4,W)};
    const int32 AxisOffsets3[]={STRUCT_OFFSET(FVector,X),STRUCT_OFFSET(FVector,Y),STRUCT_OFFSET(FVector,Z)};
    int32 FieldCount=0;if(!MapStringDouble)for(TFieldIterator<FProperty> It(Native,EFieldIterationFlags::None);It;++It)++FieldCount;
    if(FieldCount!=Axes){Fail(R,TEXT("graph_vector_array_native_layout_unsupported"),TEXT("The exact native value struct must have its expected own double axis fields."));return R;}
    for(int32 I=0;I<Axes;++I){const FProperty* P=Native->FindPropertyByName(AxisNames[I]);const int32 Offset=Four?AxisOffsets4[I]:AxisOffsets3[I];
        if(!P||P->GetClass()!=FDoubleProperty::StaticClass()||P->GetOwnerStruct()!=Native||P->ArrayDim!=1||P->GetOffset_ForInternal()!=Offset||P->GetSize()!=sizeof(double))
        {Fail(R,TEXT("graph_vector_array_native_layout_unsupported"),TEXT("Native axis field class/owner/layout differs; no array values were accessed."));return R;}
        Fields.Add({P,P->GetFName(),P->GetPropertyFlags(),Offset,P->GetSize()});}
    const FMapProperty* MapProperty=nullptr;const FStrProperty* MapKey=nullptr;const FProperty* MapValue=nullptr;
    EPropertyFlags MapFlags=CPF_None,MapKeyFlags=CPF_None,MapValueFlags=CPF_None;int32 MapOffset=0,MapSize=0,MapKeyOffset=0,MapKeySize=0,MapValueOffset=0,MapValueSize=0;
    if(MapStringVector)
    {
        MapProperty=CastField<FMapProperty>(Skeleton->FindPropertyByName(Name));MapKey=MapProperty?CastField<FStrProperty>(MapProperty->KeyProp):nullptr;MapValue=MapProperty?MapProperty->ValueProp:nullptr;
        const EPropertyFlags Forbidden=CPF_Parm|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_ExportObject|CPF_PersistentInstance|CPF_UObjectWrapper;
        if(!MapProperty||!MapKey||!MapValue||MapProperty->GetClass()!=FMapProperty::StaticClass()||MapKey->GetClass()!=FStrProperty::StaticClass()||MapValue->GetClass()!=(MapStringDouble?FDoubleProperty::StaticClass():FStructProperty::StaticClass())
            ||MapProperty->GetOwnerClass()!=Skeleton||MapProperty->ArrayDim!=1||MapKey->ArrayDim!=1||MapValue->ArrayDim!=1||MapKey->GetOwnerProperty()!=MapProperty||MapValue->GetOwnerProperty()!=MapProperty
            ||(!MapStringDouble&&(!static_cast<const FStructProperty*>(MapValue)->Struct.IsResolved()||static_cast<const FStructProperty*>(MapValue)->Struct!=Native))||MapKey->GetSize()!=sizeof(FString)||MapValue->GetSize()!=(MapStringDouble?sizeof(double):sizeof(FVector))||!MapKey->HasAnyPropertyFlags(CPF_HasGetValueTypeHash)
            ||!MapProperty->HasAllPropertyFlags(CPF_BlueprintVisible|CPF_Transient)||(bSetter&&MapProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
            ||MapProperty->HasAnyPropertyFlags(Forbidden)||MapKey->HasAnyPropertyFlags(Forbidden)||MapValue->HasAnyPropertyFlags(Forbidden)
            ||MapProperty->GetOffset_ForInternal()<0||MapProperty->GetSize()<=0||int64(MapProperty->GetOffset_ForInternal())+MapProperty->GetSize()>Skeleton->GetPropertiesSize())
        {Fail(R,TEXT("graph_map_property_unsupported"),TEXT("Exact current own transient visible String-key/Vector-value Map property, child owner/layout/hash/access and full wrappers must match; no Map value is accessed."));return R;}
        MapFlags=MapProperty->GetPropertyFlags();MapKeyFlags=MapKey->GetPropertyFlags();MapValueFlags=MapValue->GetPropertyFlags();MapOffset=MapProperty->GetOffset_ForInternal();MapSize=MapProperty->GetSize();MapKeyOffset=MapKey->GetOffset_ForInternal();MapKeySize=MapKey->GetSize();MapValueOffset=MapValue->GetOffset_ForInternal();MapValueSize=MapValue->GetSize();
    }
    struct FPropertyRole{const FArrayProperty* Array;const FStructProperty* Inner;UClass* Owner;EPropertyFlags Flags,InnerFlags;int32 Offset,Size,InnerOffset,InnerSize;};TArray<FPropertyRole> Properties;
    const auto BindProperty=[&](UClass* Owner)->bool
    {
        const FArrayProperty* A=CastField<FArrayProperty>(Owner->FindPropertyByName(Name));const FStructProperty* I=A?CastField<FStructProperty>(A->Inner):nullptr;
        if(!A||!I||A->GetClass()!=FArrayProperty::StaticClass()||I->GetClass()!=FStructProperty::StaticClass()||A->GetOwnerClass()!=Owner||I->GetOwnerProperty()!=A||A->ArrayDim!=1||I->ArrayDim!=1
            ||!I->Struct.IsResolved()||I->Struct!=Native||!A->HasAnyPropertyFlags(CPF_BlueprintVisible)||(bSetter&&A->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
            ||A->HasAnyPropertyFlags(CPF_Parm|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_ExportObject|CPF_PersistentInstance)
            ||I->HasAnyPropertyFlags(CPF_Parm|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_ExportObject|CPF_PersistentInstance|CPF_UObjectWrapper)
            ||I->GetSize()!=ExpectedSize)return false;
        Properties.Add({A,I,Owner,A->GetPropertyFlags(),I->GetPropertyFlags(),A->GetOffset_ForInternal(),A->GetSize(),I->GetOffset_ForInternal(),I->GetSize()});return true;
    };
    if(!MapStringVector&&(!BindProperty(Skeleton)||(ExternalMember&&!BindProperty(Generated)))){Fail(R,TEXT("graph_vector_array_property_unsupported"),TEXT("Exact own current visible Array/Struct inner properties, native layout, access and wrappers must match in each required declaring role."));return R;}
    const FProperty* Property=MapStringVector?static_cast<const FProperty*>(MapProperty):Properties[0].Array;const FProperty* Compiled=ExternalMember?Properties[1].Array:nullptr;
    const auto Current=[&]()
    {
        bool Good=Basic();if(Good)for(const auto& F:Fields)if(Native->FindPropertyByName(F.Name)!=F.P||F.P->GetClass()!=FDoubleProperty::StaticClass()||F.P->GetOwnerStruct()!=Native||F.P->ArrayDim!=1||F.P->GetPropertyFlags()!=F.Flags||F.P->GetOffset_ForInternal()!=F.Offset||F.P->GetSize()!=F.Size){Good=false;break;}
        if(Good)for(const auto& P:Properties)if(P.Owner->FindPropertyByName(Name)!=P.Array||P.Array->GetOwnerClass()!=P.Owner||P.Array->GetClass()!=FArrayProperty::StaticClass()||P.Array->ArrayDim!=1||P.Array->Inner!=P.Inner
            ||P.Array->GetPropertyFlags()!=P.Flags||P.Array->GetOffset_ForInternal()!=P.Offset||P.Array->GetSize()!=P.Size||P.Inner->GetClass()!=FStructProperty::StaticClass()||P.Inner->GetOwnerProperty()!=P.Array||P.Inner->ArrayDim!=1
            ||P.Inner->GetPropertyFlags()!=P.InnerFlags||P.Inner->GetOffset_ForInternal()!=P.InnerOffset||P.Inner->GetSize()!=P.InnerSize||!P.Inner->Struct.IsResolved()||P.Inner->Struct!=Native){Good=false;break;}
        if(Good&&MapStringVector)Good=Skeleton->FindPropertyByName(Name)==MapProperty&&MapProperty->GetClass()==FMapProperty::StaticClass()&&MapProperty->GetOwnerClass()==Skeleton&&MapProperty->ArrayDim==1
            &&MapProperty->KeyProp==MapKey&&MapProperty->ValueProp==MapValue&&MapProperty->GetPropertyFlags()==MapFlags&&MapProperty->GetOffset_ForInternal()==MapOffset&&MapProperty->GetSize()==MapSize
            &&MapKey->GetClass()==FStrProperty::StaticClass()&&MapKey->GetOwnerProperty()==MapProperty&&MapKey->ArrayDim==1&&MapKey->GetPropertyFlags()==MapKeyFlags&&MapKey->GetOffset_ForInternal()==MapKeyOffset&&MapKey->GetSize()==MapKeySize
            &&MapValue->GetClass()==(MapStringDouble?FDoubleProperty::StaticClass():FStructProperty::StaticClass())&&MapValue->GetOwnerProperty()==MapProperty&&MapValue->ArrayDim==1&&MapValue->GetPropertyFlags()==MapValueFlags&&MapValue->GetOffset_ForInternal()==MapValueOffset&&MapValue->GetSize()==MapValueSize&&(MapStringDouble||static_cast<const FStructProperty*>(MapValue)->Struct.IsResolved()&&static_cast<const FStructProperty*>(MapValue)->Struct==Native);
        if(!Good){R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("observation_incomplete"),true);Fail(R,TEXT("graph_vector_array_scope_changed"),TEXT("Current selected own declaration/property/native layout or source/target role changed; no followup SDK, cleanup or retry."));}return Good;
    };
    if(!Current())return R;const auto* Schema=Cast<UEdGraphSchema_K2>(Graph->GetSchema());if(!Current())return R;
    if(!Schema||Schema->GetClass()!=UEdGraphSchema_K2::StaticClass()){Fail(R,TEXT("graph_vector_array_schema_unsupported"),TEXT("The exact standard K2 schema is required."));return R;}
    FEdGraphPinType Actual;
    if(MapStringVector)
    {
        const bool Converted=Schema->ConvertPropertyToPinType(MapProperty,Actual);if(!Current())return R;if(!Converted||!SameType(Actual,Expected)){Fail(R,TEXT("graph_map_skeleton_mismatch"),TEXT("Full String/Vector Map schema conversion differs from the own declared terminal/ref/const/wrapper type."));return R;}
        const auto Readable=FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP,MapProperty);if(!Current())return R;if(FBlueprintEditorUtils::IsPropertyPrivate(MapProperty)||Readable!=FBlueprintEditorUtils::EPropertyReadableState::Readable){Fail(R,TEXT("graph_member_access_rejected"),TEXT("Selected own Map is not currently Blueprint-readable."));return R;}
    }
    for(const auto& P:Properties){FEdGraphPinType Converted;const bool Ok=Schema->ConvertPropertyToPinType(P.Array,Converted);if(!Current())return R;
        if(!Ok||!SameType(Converted,Expected)){Fail(R,TEXT("graph_member_skeleton_mismatch"),TEXT("Full current Array/Struct property schema conversion differs from its declared exact type."));return R;}Actual=Converted;
        const auto Readable=FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP,P.Array);if(!Current())return R;
        if(FBlueprintEditorUtils::IsPropertyPrivate(P.Array)||Readable!=FBlueprintEditorUtils::EPropertyReadableState::Readable){Fail(R,TEXT("graph_member_access_rejected"),TEXT("Selected value array must be publicly Blueprint-readable in the current target context."));return R;}}
    const auto SourceState=[&](){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("blueprint_path"),SourcePath);J->SetStringField(TEXT("generated_class"),Generated->GetPathName());J->SetStringField(TEXT("skeleton_class"),Skeleton->GetPathName());J->SetBoolField(TEXT("dirty"),SourceDirty);J->SetNumberField(TEXT("status"),SourceStatus);J->SetNumberField(TEXT("declared_variable_count"),SourceDescriptions.Num());return J;};
    if(ExternalMember)
    {
        R->SetObjectField(TEXT("external_source_before"),SourceState());const auto Inspection=GeneratedComponent::InspectSavedClass(External);if(!Current())return R;
        R->SetObjectField(TEXT("external_source_validation"),Inspection);if(!Inspection->GetBoolField(TEXT("success"))){Fail(R,TEXT("graph_external_member_class_unsupported"),TEXT("External value-array source must pass the existing saved-clean compiled non-scene ActorComponent guard."));return R;}
        const bool Inherited=TargetSkeleton->IsChildOf(External);if(!Current())return R;if(Inherited){Fail(R,TEXT("graph_external_member_scope_unsupported"),TEXT("Inherited and self-context arrays use no external admission in this slice."));return R;}
    }
    const auto ParentProof=InspectBlueprintParent(Parent->GetPathName());if(!Current())return R;R->SetObjectField(TEXT("parent_inspection"),ParentProof);
    if(!ParentProof->GetBoolField(TEXT("success"))){Fail(R,TEXT("graph_member_parent_unsupported"),TEXT("Target parent failed the existing supported runtime parent guard."));return R;}
    const auto NativeState=[&](){auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("struct_path"),TypePath);J->SetStringField(TEXT("type_path"),TypePath);J->SetStringField(TEXT("type_kind"),MapStringDouble?TEXT("string_double_map"):MapStringVector?TEXT("string_vector_map"):Four?TEXT("vector4_array"):TEXT("vector_array"));J->SetStringField(TEXT("package_path"),TEXT("/Script/CoreUObject"));J->SetStringField(TEXT("package_flags_string"),TEXT("16"));J->SetBoolField(TEXT("package_dirty"),false);J->SetStringField(TEXT("type_flags_string"),LexToString(StructFlags));J->SetNumberField(TEXT("type_structure_size"),ExpectedSize);J->SetNumberField(TEXT("type_alignment"),StructAlign);TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& F:Fields){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),F.Name.ToString());V->SetStringField(TEXT("owner"),TypePath);V->SetStringField(TEXT("class"),TEXT("DoubleProperty"));V->SetStringField(TEXT("flags"),LexToString(uint64(F.Flags)));V->SetNumberField(TEXT("offset"),F.Offset);V->SetNumberField(TEXT("size"),F.Size);Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("native_fields"),Rows);if(MapStringDouble){J->SetStringField(TEXT("scalar_property_class"),TEXT("DoubleProperty"));J->SetStringField(TEXT("scalar_category"),TEXT("real"));J->SetStringField(TEXT("scalar_subcategory"),TEXT("double"));}return J;};
    const auto Before=NativeState();R->SetObjectField(TEXT("native_value_source_before"),Before);FString TypeText,SelfText;
    FEdGraphPinType::StaticStruct()->ExportText(TypeText,&Actual,&Actual,nullptr,PPF_None,nullptr);if(!Current())return R;
    if(ExternalMember){FEdGraphPinType Self;Self.PinCategory=UEdGraphSchema_K2::PC_Object;Self.PinSubCategoryObject=External;FEdGraphPinType::StaticStruct()->ExportText(SelfText,&Self,&Self,nullptr,PPF_None,nullptr);if(!Current())return R;}
    // Reuse the existing KZ selected returned-node proof, now for exact value-array members only.
    const auto Inventory=Graph->Nodes;TArray<TStrongObjectPtr<UEdGraphNode>> HeldNodes;TArray<TSharedPtr<FJsonValue>> MemberNodes;
    struct FSelectedRole {UK2Node_Variable* Node;FString Path;FGuid Guid;EObjectFlags Flags;FMemberReference Reference;TArray<UEdGraphPin*> Pins;UEdGraphPin* Value;FGuid ValueId;FEdGraphPinType Type;uint32 ValueFlags;bool Get;UEdGraphPin* Self;FGuid SelfId;FEdGraphPinType SelfType;uint32 SelfFlags;};
    TArray<FSelectedRole> SelectedRoles;FMemberReference SelfReference;if(ExternalMember)SelfReference.SetExternalMember(Name,External,Description.VarGuid);else SelfReference.SetSelfMember(Name,Description.VarGuid);
    const auto InventoryCurrent=[&]()
    {
        if(!Current()||Graph->Nodes.Num()!=Inventory.Num())return false;
        for(int32 I=0;I<Inventory.Num();++I)if(!Graph->Nodes[I].IsResolved()||!Inventory[I].IsResolved()||Graph->Nodes[I]!=Inventory[I])return false;
        return true;
    };
    if(Inventory.Num()>2048){Fail(R,TEXT("graph_vector_array_node_budget"),TEXT("The selected member observation uses the existing bounded graph inventory."));return R;}
    for(const auto& Handle:Inventory)
    {
        if(!InventoryCurrent()||!Handle.IsResolved()){Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("Current graph inventory must remain resolved; no hidden node load."));return R;}
        const FString NodePath=Handle.GetPathName();
        UEdGraphNode* N=FindObject<UEdGraphNode>(nullptr,*NodePath);
        if(!InventoryCurrent()||!Loaded(N)||Handle!=N){Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("Current graph node identity differs."));return R;}
        HeldNodes.Emplace(N);
        const bool Get=N->GetClass()==UK2Node_VariableGet::StaticClass(),Set=N->GetClass()==UK2Node_VariableSet::StaticClass();
        if(!Get&&!Set)continue;
        UK2Node_Variable* Variable=CastChecked<UK2Node_Variable>(N);const FMemberReference Reference=Variable->VariableReference;
        if(Reference.GetMemberName()!=Name||Reference.GetMemberGuid()!=Description.VarGuid)continue;
        const FGuid NodeGuid=N->NodeGuid;const EObjectFlags NodeFlags=N->GetFlags();const auto Pins=N->Pins;
        const auto NodeCurrent=[&]()
        {
            if(!InventoryCurrent()||!Loaded(N)||N->GetPathName()!=NodePath||N->GetOuter()!=Graph||N->GetGraph()!=Graph||N->NodeGuid!=NodeGuid||N->GetFlags()!=NodeFlags
                ||N->GetClass()!=(Get?UK2Node_VariableGet::StaticClass():UK2Node_VariableSet::StaticClass())||N->Pins.Num()!=Pins.Num()
                ||!Variable->VariableReference.IsSameReference(Reference)||Variable->VariableReference.IsDeprecated()!=Reference.IsDeprecated())return false;
            for(int32 I=0;I<Pins.Num();++I)if(N->Pins[I]!=Pins[I])return false;
            return true;
        };
        if(!Reference.IsSameReference(SelfReference)||Reference.IsDeprecated()||!NodeGuid.IsValid()||!NodeCurrent())
        {Fail(R,TEXT("graph_vector_array_node_reference_invalid"),TEXT("Selected own member nodes must retain their exact nondeprecated self GUID/name and current graph role."));return R;}
        FProperty* Resolved=Variable->GetPropertyForVariableFromSkeleton();
        if(!NodeCurrent()){Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("Selected node changed during property resolution; no stale pin access."));return R;}
        if(Resolved!=Property&&(!ExternalMember||Resolved!=Compiled)){Fail(R,TEXT("graph_vector_array_node_property_mismatch"),TEXT("Selected node must resolve to the exact current declaring current skeleton/compiled property."));return R;}
        UEdGraphPin* ValuePin=nullptr;
        for(UEdGraphPin* P:Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N){Fail(R,TEXT("graph_vector_array_node_pin_invalid"),TEXT("Current physical pin inventory is incomplete."));return R;}
            if(P->PinName==Name&&P->Direction==(Get?EGPD_Output:EGPD_Input)){if(ValuePin){Fail(R,TEXT("graph_vector_array_node_pin_invalid"),TEXT("Selected physical value pin is ambiguous."));return R;}ValuePin=P;}
        }
        const auto Whole=[](const UEdGraphPin* P,UEdGraphNode* Owner)
        {return P&&!P->WasTrashed()&&P->GetOwningNodeUnchecked()==Owner&&P->PinId.IsValid()&&!P->bHidden&&!P->bOrphanedPin&&!P->bNotConnectable&&!P->bDeprecated&&!P->ParentPin&&P->SubPins.IsEmpty();};
        const auto PhysicalFlags=[](const UEdGraphPin* P)->uint32
        {return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)|(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)|(uint32(P->ShouldSavePinIfOrphaned())<<9);};
        if(!Whole(ValuePin,N)||ValuePin->PinType!=Expected||ValuePin->PinType.bIsUObjectWrapper!=Expected.bIsUObjectWrapper
            ||ValuePin->PinType.bSerializeAsSinglePrecisionFloat!=Expected.bSerializeAsSinglePrecisionFloat||ValuePin->PinType.PinValueType.bTerminalIsUObjectWrapper!=Expected.PinValueType.bTerminalIsUObjectWrapper)
        {Fail(R,TEXT("graph_vector_array_node_pin_invalid"),TEXT("Selected physical value pin must match the full own descriptor type."));return R;}
        const FGuid ValuePinId=ValuePin->PinId;const FEdGraphPinType ValueType=ValuePin->PinType;const uint32 ValueFlags=PhysicalFlags(ValuePin);
        UEdGraphPin* SelfPin=nullptr;FGuid SelfId;FEdGraphPinType SelfType;uint32 SelfFlags=0;FString SelfTypeText;
        if(ExternalMember)
        {
            for(UEdGraphPin* P:Pins)if(P->PinName==UEdGraphSchema_K2::PN_Self&&P->Direction==EGPD_Input){if(SelfPin){Fail(R,TEXT("graph_vector_array_node_pin_invalid"),TEXT("External member self pin is ambiguous."));return R;}SelfPin=P;}
            FEdGraphPinType ExpectedSelf;ExpectedSelf.PinCategory=UEdGraphSchema_K2::PC_Object;ExpectedSelf.PinSubCategoryObject=External;
            if(!Whole(SelfPin,N)||!SameType(SelfPin->PinType,ExpectedSelf)){Fail(R,TEXT("graph_vector_array_node_pin_invalid"),TEXT("External getter requires its exact whole generated component self input."));return R;}
            SelfId=SelfPin->PinId;SelfType=SelfPin->PinType;SelfFlags=PhysicalFlags(SelfPin);
            FEdGraphPinType::StaticStruct()->ExportText(SelfTypeText,&SelfType,&SelfType,nullptr,PPF_None,nullptr);
            if(!NodeCurrent()||!Whole(SelfPin,N)||SelfPin->PinId!=SelfId||!SameType(SelfPin->PinType,SelfType)||PhysicalFlags(SelfPin)!=SelfFlags){Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("External whole self pin changed during type export."));return R;}
        }
        SelectedRoles.Add({Variable,NodePath,NodeGuid,NodeFlags,Reference,Pins,ValuePin,ValuePinId,ValueType,ValueFlags,Get,SelfPin,SelfId,SelfType,SelfFlags});
        FString ValueTypeText;FEdGraphPinType::StaticStruct()->ExportText(ValueTypeText,&ValueType,&ValueType,nullptr,PPF_None,nullptr);
        if(!NodeCurrent()){Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("Selected node/pin changed during type export; no stale pin access."));return R;}
        if(!Whole(ValuePin,N)||ValuePin->PinId!=ValuePinId||ValuePin->PinName!=Name||ValuePin->Direction!=(Get?EGPD_Output:EGPD_Input)||PhysicalFlags(ValuePin)!=ValueFlags||ValuePin->PinType!=ValueType||ValuePin->PinType.bIsUObjectWrapper!=ValueType.bIsUObjectWrapper
            ||ValuePin->PinType.bSerializeAsSinglePrecisionFloat!=ValueType.bSerializeAsSinglePrecisionFloat||ValuePin->PinType.PinValueType.bTerminalIsUObjectWrapper!=ValueType.PinValueType.bTerminalIsUObjectWrapper)
        {Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("Selected value pin role changed during type export."));return R;}
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),NodePath);J->SetStringField(TEXT("node_guid"),NodeGuid.ToString(EGuidFormats::Digits));
        J->SetStringField(TEXT("node_class_path"),N->GetClass()->GetPathName());J->SetStringField(TEXT("outer_path"),GraphPath);J->SetStringField(TEXT("graph_path"),GraphPath);J->SetBoolField(TEXT("graph_member"),true);
        J->SetStringField(TEXT("member_name"),Name.ToString());J->SetStringField(TEXT("member_guid"),Description.VarGuid.ToString(EGuidFormats::Digits));J->SetBoolField(TEXT("member_self_context"),!ExternalMember);
        J->SetBoolField(TEXT("member_local_scope"),false);J->SetBoolField(TEXT("member_deprecated"),false);J->SetStringField(TEXT("property_path"),Resolved->GetPathName());J->SetStringField(TEXT("property_declaring_role"),Resolved==Property?TEXT("skeleton"):TEXT("generated"));
        J->SetStringField(TEXT("value_pin_name"),Name.ToString());J->SetStringField(TEXT("value_pin_direction"),Get?TEXT("output"):TEXT("input"));J->SetStringField(TEXT("value_pin_type_text"),ValueTypeText);
        J->SetStringField(TEXT("value_pin_id"),ValuePinId.ToString(EGuidFormats::Digits));J->SetBoolField(TEXT("value_pin_whole_current_verified"),true);J->SetBoolField(TEXT("value_pin_trashed"),false);J->SetBoolField(TEXT("value_pin_hidden"),false);J->SetBoolField(TEXT("value_pin_orphaned"),false);J->SetBoolField(TEXT("value_pin_split"),false);J->SetStringField(TEXT("value_pin_owner_path"),NodePath);J->SetNumberField(TEXT("value_pin_flags"),ValueFlags);
        if(ExternalMember){J->SetStringField(TEXT("self_pin_name"),UEdGraphSchema_K2::PN_Self.ToString());J->SetStringField(TEXT("self_pin_direction"),TEXT("input"));J->SetStringField(TEXT("self_pin_id"),SelfId.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("self_pin_type_text"),SelfTypeText);J->SetStringField(TEXT("self_pin_owner_path"),NodePath);J->SetBoolField(TEXT("self_pin_whole_current_verified"),true);J->SetNumberField(TEXT("self_pin_flags"),SelfFlags);}
        MemberNodes.Add(MakeShared<FJsonValueObject>(J));
    }
    if(!InventoryCurrent()){Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("Selected member observation ended with a changed graph inventory."));return R;}
    // Recheck earlier selected rows after later property/type-export callbacks. No default object or runtime value is accessed.
    const auto MemberNodesCurrent=[&]()->bool
    {
    for(const auto& Role:SelectedRoles)
    {
        UK2Node_Variable* N=Role.Node;
        if(!InventoryCurrent()||!Loaded(N)||N->GetPathName()!=Role.Path||N->GetOuter()!=Graph||N->GetGraph()!=Graph||N->NodeGuid!=Role.Guid||N->GetFlags()!=Role.Flags
            ||N->GetClass()!=(Role.Get?UK2Node_VariableGet::StaticClass():UK2Node_VariableSet::StaticClass())||!N->VariableReference.IsSameReference(Role.Reference)||N->VariableReference.IsDeprecated()!=Role.Reference.IsDeprecated()||N->Pins.Num()!=Role.Pins.Num())
        {Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("A selected earlier member node changed before the final observation."));return false;}
        for(int32 I=0;I<Role.Pins.Num();++I)if(N->Pins[I]!=Role.Pins[I]){Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("A selected physical inventory changed before the final observation."));return false;}
        const UEdGraphPin* P=Role.Value;
        if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||P->PinId!=Role.ValueId||P->PinName!=Name||P->Direction!=(Role.Get?EGPD_Output:EGPD_Input)
            ||P->bHidden||P->bOrphanedPin||P->bNotConnectable||P->bDeprecated||P->ParentPin||!P->SubPins.IsEmpty()||P->PinType!=Role.Type
            ||P->PinType.bIsUObjectWrapper!=Role.Type.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=Role.Type.bSerializeAsSinglePrecisionFloat||P->PinType.PinValueType.bTerminalIsUObjectWrapper!=Role.Type.PinValueType.bTerminalIsUObjectWrapper
            ||(uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)|(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)|(uint32(P->ShouldSavePinIfOrphaned())<<9))!=Role.ValueFlags)
        {Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("A selected whole value-pin identity/type/flags changed before the final observation."));return false;}
        if(ExternalMember)
        {
            const UEdGraphPin* S=Role.Self;
            if(!S||S->WasTrashed()||S->GetOwningNodeUnchecked()!=N||S->PinId!=Role.SelfId||S->PinName!=UEdGraphSchema_K2::PN_Self||S->Direction!=EGPD_Input
                ||S->bHidden||S->bOrphanedPin||S->bNotConnectable||S->bDeprecated||S->ParentPin||!S->SubPins.IsEmpty()||!SameType(S->PinType,Role.SelfType)
                ||(uint32(S->bHidden)|(uint32(S->bNotConnectable)<<1)|(uint32(S->bDefaultValueIsReadOnly)<<2)|(uint32(S->bDefaultValueIsIgnored)<<3)|(uint32(S->bAdvancedView)<<4)|(uint32(S->bDeprecated)<<5)|(uint32(S->bDisplayAsMutableRef)<<6)|(uint32(S->bAllowFriendlyName)<<7)|(uint32(S->bOrphanedPin)<<8)|(uint32(S->ShouldSavePinIfOrphaned())<<9))!=Role.SelfFlags)
            {Fail(R,TEXT("graph_vector_array_node_scope_changed"),TEXT("An earlier external whole self pin changed before the final observation."));return false;}
        }
    }
    return true;
    };
    if(!MemberNodesCurrent())return R;
    R->SetArrayField(TEXT("native_value_member_nodes"),MemberNodes);
    if(!Current())return R;const auto After=NativeState();R->SetObjectField(TEXT("native_value_source_after"),After);R->SetBoolField(TEXT("native_value_source_observed_unchanged"),true);
    R->SetStringField(TEXT("variable_name"),VariableName);R->SetStringField(TEXT("variable_guid"),Description.VarGuid.ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("setter"),bSetter);R->SetStringField(TEXT("native_type_path"),TypePath);R->SetStringField(TEXT("value_kind"),MapStringDouble?TEXT("string_double_map"):MapStringVector?TEXT("string_vector_map"):Four?TEXT("vector4_array"):TEXT("vector_array"));
    R->SetStringField(TEXT("property_path"),Property->GetPathName());R->SetStringField(TEXT("property_owner_class_path"),Skeleton->GetPathName());R->SetStringField(TEXT("skeleton_class_path"),Skeleton->GetPathName());R->SetStringField(TEXT("property_class"),MapStringVector?TEXT("MapProperty"):TEXT("ArrayProperty"));R->SetNumberField(TEXT("property_array_dim"),1);R->SetStringField(TEXT("property_flags_string"),LexToString(uint64(MapStringVector?MapFlags:Properties[0].Flags)));R->SetStringField(TEXT("inner_property_path"),MapStringVector?MapValue->GetPathName():Properties[0].Inner->GetPathName());R->SetStringField(TEXT("inner_property_flags_string"),LexToString(uint64(MapStringVector?MapValueFlags:Properties[0].InnerFlags)));
    if(MapStringDouble)R->SetStringField(TEXT("inner_property_class"),TEXT("DoubleProperty"));
    if(MapStringVector){R->SetStringField(TEXT("key_property_path"),MapKey->GetPathName());R->SetStringField(TEXT("key_property_flags_string"),LexToString(uint64(MapKeyFlags)));R->SetBoolField(TEXT("map_property_identity_verified"),true);R->SetBoolField(TEXT("map_setter_input_requires_connection"),bSetter);}
    R->SetStringField(TEXT("expected_pin_type_text"),TypeText);R->SetObjectField(TEXT("pin_type"),PinTypeObservation(Actual));R->SetStringField(TEXT("pin_category"),MapStringVector?TEXT("string"):TEXT("struct"));R->SetStringField(TEXT("pin_subcategory"),TEXT("None"));R->SetBoolField(TEXT("pin_is_uobject_wrapper"),Actual.bIsUObjectWrapper);R->SetBoolField(TEXT("pin_serialize_single_precision"),Actual.bSerializeAsSinglePrecisionFloat);R->SetBoolField(TEXT("pin_terminal_is_uobject_wrapper"),Actual.PinValueType.bTerminalIsUObjectWrapper);
    R->SetStringField(TEXT("stored_default_before"),Description.DefaultValue);R->SetStringField(TEXT("stored_default_after"),Description.DefaultValue);R->SetBoolField(TEXT("stored_default_observed_unchanged"),true);R->SetBoolField(TEXT("native_value_type_identity_verified"),true);R->SetBoolField(TEXT("native_value_property_identity_verified"),true);R->SetBoolField(TEXT("runtime_value_read_attempted"),false);R->SetBoolField(TEXT("default_write_attempted"),false);R->SetBoolField(TEXT("runtime_evaluation_verified"),false);
    if(ExternalMember){R->SetStringField(TEXT("target_class_path"),External->GetPathName());R->SetStringField(TEXT("declaring_class_path"),External->GetPathName());R->SetStringField(TEXT("declaring_blueprint_path"),SourcePath);R->SetStringField(TEXT("compiled_property_path"),Compiled->GetPathName());R->SetStringField(TEXT("expected_self_pin_type_text"),SelfText);R->SetObjectField(TEXT("external_source_after"),SourceState());R->SetBoolField(TEXT("external_source_observed_unchanged"),true);}
    R->SetStringField(TEXT("scope"),MapStringDouble?TEXT("Exact own transient empty-default String to scalar double Map get/set authoring; current GUID/FMapProperty/FStrProperty/FDoubleProperty/full terminal and returned whole pins; no float conversion, values/default buffer, local/output Map, runtime assignment or compile/save."):MapStringVector?TEXT("Exact own transient empty-default String to native Vector Map get/set authoring; current own GUID/property/key/value/layout/terminal and returned-node physical roles only; no Map value/default buffer, inherited/external Map, assignment, compile/save or full tracker execution."):TEXT("Exact own Vector4[] get/set or distinct saved-clean non-scene Blueprint component own Vector[]/Vector4[] getter metadata only. Current GUID/native layout/full wrapper pin/source roles are observed; runtime array/default values, class loads, CDO creation, arbitrary structs/containers, external setters, compile/save and callback purity are not provided."));
    if(!Current()||!MemberNodesCurrent())return R;Complete(R,Graph);if(!Current()||!MemberNodesCurrent())R->SetBoolField(TEXT("success"),false);return R;
}

TSharedRef<FJsonObject> ReadOwnNativeObjectArrayPlan(UEdGraph* Graph,const FString& VariableName,bool bSetter,
    const TSharedRef<FJsonObject>& R,bool bSkeletal=false)
{
    // KU/LP: node-authoring metadata only. Never broaden RuntimeObject or access runtime array values.
    R->SetStringField(TEXT("member_scope"),TEXT("own_native_object_array"));
    if(bSkeletal)
    {
        R->SetBoolField(TEXT("native_skeletal_object_array_getter"),true);R->SetBoolField(TEXT("skeletal_array_setter_supported"),false);
        if(bSetter){Fail(R,TEXT("graph_skeletal_array_setter_unsupported"),TEXT("This exact SkeletalMeshComponent array extension admits only the own getter requested by the recipe; no setter was dispatched."));return R;}
    }
    const auto Loaded=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()
        ||IsGarbageCollecting()||UE::IsSavingPackage()||!Loaded(Graph))
    {Fail(R,TEXT("graph_object_array_context_invalid"),TEXT("Requires a loaded existing K2 graph on the idle editor game thread before object-array observation."));return R;}
    UBlueprint* BP=FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if(!Loaded(BP)){Fail(R,TEXT("graph_object_array_scope_unsupported"),TEXT("The graph must belong to one loaded Blueprint."));return R;}
    TStrongObjectPtr<UBlueprint> KeepBP(BP);TStrongObjectPtr<UEdGraph> KeepGraph(Graph);
    const auto ClassRole=[](const TObjectPtr<UClass>& Handle)->UClass*
    {if(!Handle.IsResolved()||Handle==nullptr)return nullptr;UClass* C=FindObject<UClass>(nullptr,*Handle.GetPathName());return Handle==C?C:nullptr;};
    UClass* Skeleton=bSkeletal?ClassRole(BP->SkeletonGeneratedClass.GetGCPtr()):BP->SkeletonGeneratedClass.Get();UClass* Generated=bSkeletal?ClassRole(BP->GeneratedClass.GetGCPtr()):BP->GeneratedClass.Get();UClass* Parent=bSkeletal?ClassRole(BP->ParentClass.GetGCPtr()):BP->ParentClass.Get();
    TStrongObjectPtr<UClass> KeepSkeleton(Skeleton),KeepGenerated(Generated),KeepParent(Parent);
    const auto Undo=GUndo;auto* Editor=GEditor;const FString BPPath=BP->GetPathName(),GraphPath=Graph->GetPathName();const FGuid GraphGuid=Graph->GraphGuid;
    const auto Context=[&]()
    {
        return IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()
            &&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&GUndo==Undo&&GEditor==Editor
            &&Loaded(BP)&&Loaded(Graph)&&Loaded(Skeleton)&&Loaded(Generated)&&Loaded(Parent)
            &&BP->GetPathName()==BPPath&&Graph->GetPathName()==GraphPath&&Graph->GetOuter()==BP&&Graph->GraphGuid==GraphGuid
            &&(bSkeletal?(BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()&&BP->GeneratedClass.GetGCPtr().IsResolved()&&BP->ParentClass.GetGCPtr().IsResolved()
                &&BP->SkeletonGeneratedClass.GetGCPtr()==Skeleton&&BP->GeneratedClass.GetGCPtr()==Generated&&BP->ParentClass.GetGCPtr()==Parent&&Loaded(Editor))
                :(BP->SkeletonGeneratedClass==Skeleton&&BP->GeneratedClass==Generated&&BP->ParentClass==Parent))
            &&BP->BlueprintType==BPTYPE_Normal&&!BP->bBeingCompiled&&!BP->bQueuedForCompilation&&!BP->bIsRegeneratingOnLoad&&!BP->bSuppressStructurallyModified;
    };
    if(!Context()){Fail(R,TEXT("graph_object_array_scope_changed"),TEXT("Target graph/current class/transaction roles are unavailable before declaration binding."));return R;}
    TStrongObjectPtr<UEditorEngine> KeepEditor(Editor);
    const FName Name(*VariableName);const int32 Index=FBlueprintEditorUtils::FindNewVariableIndex(BP,Name);
    if(Index==INDEX_NONE){Fail(R,TEXT("graph_member_not_declared"),TEXT("Object-array getters and setters require one own declared variable."));return R;}
    const FBPVariableDescription Description=BP->NewVariables[Index];
    const FEdGraphPinType Expected=Description.VarType;
    UClass* Class=Cast<UClass>(Expected.PinSubCategoryObject.Get());
    if(Description.VarName.ToString()!=VariableName||!Description.VarGuid.IsValid()||Expected.PinCategory!=UEdGraphSchema_K2::PC_Object
        ||Expected.ContainerType!=EPinContainerType::Array||!Expected.PinSubCategory.IsNone()||Expected.bIsReference||Expected.bIsConst
        ||Expected.bIsWeakPointer||Expected.bSerializeAsSinglePrecisionFloat||Expected.PinValueType!=FEdGraphTerminalType()||Expected.PinValueType.bTerminalIsUObjectWrapper
        ||!Loaded(Class)||(bSkeletal?Class!=USkeletalMeshComponent::StaticClass():(Class!=UActorComponent::StaticClass()&&Class!=UChildActorComponent::StaticClass())))
    {Fail(R,TEXT("graph_object_array_type_unsupported"),TEXT("Only the selected exact own native strong object-array constraint is admitted; no class/soft/weak/nested/external arrays."));return R;}
    if(bSkeletal&&(!(Description.PropertyFlags&CPF_Transient)||!Description.DefaultValue.IsEmpty()))
    {Fail(R,TEXT("graph_skeletal_array_declaration_unsupported"),TEXT("The exact SkeletalMeshComponent[] getter requires an own transient declaration with no stored nonempty default; no runtime values are read."));return R;}
    TStrongObjectPtr<UClass> KeepClass(Class);UPackage* NativePackage=Class->GetPackage();TStrongObjectPtr<UPackage> KeepNativePackage(NativePackage);
    const FString ClassPath=Class->GetPathName();const EObjectFlags NativeFlags=Class->GetFlags(),BPFlags=BP->GetFlags(),GraphFlags=Graph->GetFlags();
    const auto ClassFlags=Class->GetClassFlags();const int32 ClassSize=Class->GetStructureSize();
    // LP keeps the exact native hierarchy without tracked SuperStruct reads.
    struct FSkeletalRole{TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString Path;EObjectFlags Flags;uint32 ClassFlags;int32 Size;};TArray<FSkeletalRole> SkeletalRoles;
    if(bSkeletal)
    {
        const TCHAR* Paths[]={TEXT("/Script/Engine.SkeletalMeshComponent"),TEXT("/Script/Engine.SkinnedMeshComponent"),TEXT("/Script/Engine.MeshComponent"),TEXT("/Script/Engine.PrimitiveComponent"),TEXT("/Script/Engine.SceneComponent"),TEXT("/Script/Engine.ActorComponent"),TEXT("/Script/CoreUObject.Object")};
        int32 I=0;for(auto It=Class->GetSuperStructIterator();It;++It,++I)
        {
            UClass* C=const_cast<UClass*>(Cast<UClass>(*It));UPackage* P=Loaded(C)?C->GetPackage():nullptr;
            if(I>=UE_ARRAY_COUNT(Paths)||!Loaded(C)||!Loaded(P)||C->GetOuter()!=P||C->GetPathName()!=Paths[I]||!C->HasAnyClassFlags(CLASS_Native)
                ||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=nullptr||C->IsEditorOnly()
                ||P->GetName()!=(I==6?TEXT("/Script/CoreUObject"):TEXT("/Script/Engine"))||P->GetPackageFlags()!=16U||P->IsDirty())
            {Fail(R,TEXT("graph_skeletal_array_source_unsupported"),TEXT("The exact already-loaded native SkeletalMeshComponent hierarchy is required without source load or CDO creation."));return R;}
            FSkeletalRole V;V.Class.Reset(C);V.Package.Reset(P);V.Path=Paths[I];V.Flags=C->GetFlags();V.ClassFlags=uint32(C->GetClassFlags());V.Size=C->GetStructureSize();SkeletalRoles.Add(MoveTemp(V));
        }
        if(I!=UE_ARRAY_COUNT(Paths)){Fail(R,TEXT("graph_skeletal_array_source_unsupported"),TEXT("The finite native SkeletalMeshComponent hierarchy differs."));return R;}
    }
    const auto SkeletalSourceCurrent=[&]()
    {
        if(!bSkeletal)return true;int32 I=0;for(auto It=Class->GetSuperStructIterator();It;++It,++I)
        {
            if(!SkeletalRoles.IsValidIndex(I)||*It!=SkeletalRoles[I].Class.Get())return false;
            const auto& V=SkeletalRoles[I];const UClass* C=V.Class.Get();const UPackage* P=V.Package.Get();
            if(!Loaded(C)||!Loaded(P)||C->GetOuter()!=P||C->GetPackage()!=P||C->GetPathName()!=V.Path||C->GetFlags()!=V.Flags||uint32(C->GetClassFlags())!=V.ClassFlags||C->GetStructureSize()!=V.Size
                ||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=nullptr||P->GetName()!=(I==6?TEXT("/Script/CoreUObject"):TEXT("/Script/Engine"))||P->GetPackageFlags()!=16U||P->IsDirty())return false;
        }return I==7&&SkeletalRoles.Num()==7;
    };
    const TArray<FBPVariableDescription> SkeletalDescriptions=bSkeletal?BP->NewVariables:TArray<FBPVariableDescription>(),SkeletalGeneratedDescriptions=bSkeletal?BP->GeneratedVariables:TArray<FBPVariableDescription>();
    TStrongObjectPtr<const UObject> KeepSkeletalCDO;FString SkeletalCDOPath;EObjectFlags SkeletalCDOFlags=RF_NoFlags;
    const auto SkeletalDescriptionsSame=[](const TArray<FBPVariableDescription>& A,const TArray<FBPVariableDescription>& B)
    {
        if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I){const auto& X=A[I];const auto& Y=B[I];
            if(X.VarName!=Y.VarName||X.VarGuid!=Y.VarGuid||X.VarType!=Y.VarType||X.VarType.bIsUObjectWrapper!=Y.VarType.bIsUObjectWrapper||X.VarType.bSerializeAsSinglePrecisionFloat!=Y.VarType.bSerializeAsSinglePrecisionFloat
                ||X.VarType.PinValueType.bTerminalIsUObjectWrapper!=Y.VarType.PinValueType.bTerminalIsUObjectWrapper||X.FriendlyName!=Y.FriendlyName||!X.Category.IdenticalTo(Y.Category)
                ||X.PropertyFlags!=Y.PropertyFlags||X.RepNotifyFunc!=Y.RepNotifyFunc||X.ReplicationCondition!=Y.ReplicationCondition||X.DefaultValue!=Y.DefaultValue||X.MetaDataArray.Num()!=Y.MetaDataArray.Num())return false;
            for(int32 M=0;M<X.MetaDataArray.Num();++M)if(X.MetaDataArray[M].DataKey!=Y.MetaDataArray[M].DataKey||X.MetaDataArray[M].DataValue!=Y.MetaDataArray[M].DataValue)return false;
        }return true;
    };
    const EObjectFlags SkeletonFlags=Skeleton->GetFlags();const auto SkeletonClassFlags=Skeleton->GetClassFlags();const int32 SkeletonSize=Skeleton->GetStructureSize();
    const EBlueprintStatus Status=BP->Status.GetValue();UPackage* TargetPackage=BP->GetPackage();TStrongObjectPtr<UPackage> KeepTargetPackage(TargetPackage);const bool Dirty=TargetPackage->IsDirty();
    const FArrayProperty* Array=CastField<FArrayProperty>(Skeleton->FindPropertyByName(Name));
    const FObjectProperty* Inner=Array?CastField<FObjectProperty>(Array->Inner):nullptr;
    // Keep the original short-circuit order. A failed condition leaves later metadata unobserved.
    auto Checks=MakeShared<FJsonObject>();R->SetObjectField(TEXT("object_array_property_preflight"),Checks);
    auto Evaluated=MakeShared<FJsonObject>();Checks->SetObjectField(TEXT("evaluated_conditions"),Evaluated);
    Checks->SetStringField(TEXT("first_failed_condition"),TEXT(""));Checks->SetBoolField(TEXT("later_conditions_unobserved_after_failure"),true);
    Checks->SetStringField(TEXT("expected_property_owner_role"),TEXT("current_skeleton_class"));Checks->SetStringField(TEXT("expected_object_class_path"),ClassPath);
    const auto Require=[&](const TCHAR* Key,bool Passed)
    {Evaluated->SetBoolField(Key,Passed);if(!Passed)Checks->SetStringField(TEXT("first_failed_condition"),Key);return Passed;};
    const auto RequireInt=[&](const TCHAR* Key,int64 Actual,int64 ExpectedValue)
    {Checks->SetNumberField(FString(Key)+TEXT("_actual"),Actual);Checks->SetNumberField(FString(Key)+TEXT("_expected"),ExpectedValue);return Require(Key,Actual==ExpectedValue);};
    const auto Wrapper=[&]()
    {const bool Actual=Inner->HasAnyPropertyFlags(CPF_UObjectWrapper);Checks->SetBoolField(TEXT("actual_inner_wrapper"),Actual);
        Checks->SetBoolField(TEXT("expected_wrapper"),Expected.bIsUObjectWrapper);return Require(TEXT("wrapper_matches_inner"),Expected.bIsUObjectWrapper==Actual);};
    const auto ArrayFlag=[&](const TCHAR* Key,EPropertyFlags Mask,bool AnyExpected)
    {const auto Flags=Array->GetPropertyFlags();Checks->SetStringField(TEXT("actual_array_flags_string"),LexToString(uint64(Flags)));
        Checks->SetStringField(TEXT("last_array_flag_mask_string"),LexToString(uint64(Mask)));return Require(Key,((Flags&Mask)!=0)==AnyExpected);};
    bool bCompilerArrayContainsAdmitted=false;
    const auto ArrayCompilerFlags=[&]()
    {
        // KismetCompilerMisc: a DefaultToInstanced strong component inner causes
        // its FArrayProperty to carry ContainsInstancedReference. Admit only that
        // observed container bit in this exact own AC/CAC metadata route; never write it.
        const EPropertyFlags Mask=CPF_EditorOnly|CPF_Parm|CPF_ContainsInstancedReference|CPF_InstancedReference|CPF_ExportObject|CPF_PersistentInstance;
        const auto Flags=Array->GetPropertyFlags();const uint64 Rejected=uint64(Flags)&uint64(Mask);
        const bool DefaultInstanced=(uint32(ClassFlags)&uint32(CLASS_DefaultToInstanced))!=0;
        const bool CompilerContainsOnly=Rejected==uint64(CPF_ContainsInstancedReference)&&DefaultInstanced;
        bCompilerArrayContainsAdmitted=CompilerContainsOnly;
        Checks->SetStringField(TEXT("actual_array_flags_string"),LexToString(uint64(Flags)));
        Checks->SetStringField(TEXT("last_array_flag_mask_string"),LexToString(uint64(Mask)));
        Checks->SetStringField(TEXT("array_rejected_flag_bits_string"),LexToString(Rejected));
        Checks->SetStringField(TEXT("constraint_class_flags_string"),LexToString(uint32(ClassFlags)));
        Checks->SetBoolField(TEXT("constraint_default_to_instanced"),DefaultInstanced);
        Checks->SetBoolField(TEXT("array_compiler_contains_instanced_reference_admitted"),CompilerContainsOnly);
        return Require(TEXT("array_no_instanced_editor_or_parameter_flags"),Rejected==0||CompilerContainsOnly);
    };
    const auto InnerCompilerFlags=[&]()
    {
        // The observed exact native component inner carries the compiler's
        // InstancedReference bit. Require its already-approved container pair;
        // this observes metadata and neither instantiates nor accesses a value.
        const EPropertyFlags Mask=CPF_EditorOnly|CPF_Parm|CPF_ContainsInstancedReference|CPF_InstancedReference|CPF_ExportObject|CPF_PersistentInstance;
        const auto Flags=Inner->GetPropertyFlags();const uint64 Rejected=uint64(Flags)&uint64(Mask);
        const bool DefaultInstanced=(uint32(ClassFlags)&uint32(CLASS_DefaultToInstanced))!=0;
        const bool CompilerInstancedOnly=Rejected==uint64(CPF_InstancedReference)&&DefaultInstanced&&bCompilerArrayContainsAdmitted;
        Checks->SetStringField(TEXT("actual_inner_flags_string"),LexToString(uint64(Flags)));
        Checks->SetStringField(TEXT("last_inner_flag_mask_string"),LexToString(uint64(Mask)));
        Checks->SetStringField(TEXT("inner_rejected_flag_bits_string"),LexToString(Rejected));
        Checks->SetBoolField(TEXT("inner_compiler_instanced_reference_admitted"),CompilerInstancedOnly);
        return Require(TEXT("inner_no_instanced_editor_or_parameter_flags"),Rejected==0||CompilerInstancedOnly);
    };
    if(!(Require(TEXT("context_current"),Context())&&Require(TEXT("native_package_loaded"),Loaded(NativePackage))
        &&Require(TEXT("native_package_name_exact"),NativePackage->GetName()==TEXT("/Script/Engine"))
        &&Require(TEXT("native_package_clean"),!NativePackage->IsDirty())&&RequireInt(TEXT("native_package_flags16"),NativePackage->GetPackageFlags(),16)
        &&Require(TEXT("constraint_native"),Class->HasAnyClassFlags(CLASS_Native))&&Require(TEXT("constraint_not_generated"),(!bSkeletal||Class->ClassGeneratedBy.IsResolved())&&Class->ClassGeneratedBy==nullptr)
        &&Require(TEXT("constraint_runtime_flags"),!Class->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface))
        &&Require(TEXT("skeleton_array_found"),Array!=nullptr)&&Require(TEXT("strong_object_inner_found"),Inner!=nullptr)
        &&Require(TEXT("inner_exact_FObjectProperty"),Inner->GetClass()==FObjectProperty::StaticClass())
        &&Require(TEXT("array_owner_is_current_skeleton"),Array->GetOwnerClass()==Skeleton)
        &&RequireInt(TEXT("array_dim1"),Array->ArrayDim,1)&&RequireInt(TEXT("inner_dim1"),Inner->ArrayDim,1)
        &&Require(TEXT("inner_owner_is_array"),Inner->GetOwnerProperty()==Array)
        &&Require(TEXT("inner_class_resolved"),Inner->PropertyClass.IsResolved())&&Require(TEXT("inner_class_is_constraint"),Inner->PropertyClass==Class)
        &&ArrayFlag(TEXT("array_BlueprintVisible"),CPF_BlueprintVisible,true)
        &&Require(TEXT("setter_not_readonly"),!bSetter||!Array->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
        &&ArrayCompilerFlags()
        &&InnerCompilerFlags()
        &&Wrapper()&&Require(TEXT("skeletal_transient_property"),!bSkeletal||Array->HasAllPropertyFlags(CPF_Transient))))
    {Fail(R,TEXT("graph_object_array_property_unsupported"),TEXT("Current own skeleton Array/strong native Object inner property, access, wrapper and runtime source roles must match exactly."));return R;}
    Checks->SetBoolField(TEXT("later_conditions_unobserved_after_failure"),false);
    const auto ArrayFlags=Array->GetPropertyFlags(),InnerFlags=Inner->GetPropertyFlags();
    const int32 ArrayOffset=Array->GetOffset_ForInternal(),ArraySize=Array->GetSize(),InnerOffset=Inner->GetOffset_ForInternal(),InnerSize=Inner->GetSize();
    const auto Current=[&]()
    {
        bool Good=Context()&&Loaded(Editor)&&Loaded(TargetPackage)&&BP->GetPackage()==TargetPackage&&TargetPackage->IsDirty()==Dirty&&BP->Status.GetValue()==Status
            &&BP->GetFlags()==BPFlags&&Graph->GetFlags()==GraphFlags&&Loaded(Class)&&Loaded(NativePackage)
            &&Skeleton->GetFlags()==SkeletonFlags&&Skeleton->GetClassFlags()==SkeletonClassFlags&&Skeleton->GetStructureSize()==SkeletonSize&&(!bSkeletal||Skeleton->ClassGeneratedBy.IsResolved())&&Skeleton->ClassGeneratedBy==BP
            &&Class->GetFlags()==NativeFlags&&Class->GetClassFlags()==ClassFlags&&Class->GetStructureSize()==ClassSize
            &&Class->GetPathName()==ClassPath&&Class->GetPackage()==NativePackage&&(!bSkeletal||Class->ClassGeneratedBy.IsResolved())&&Class->ClassGeneratedBy==nullptr
            &&NativePackage->GetName()==TEXT("/Script/Engine")&&NativePackage->GetPackageFlags()==16U&&!NativePackage->IsDirty();
        if(Good&&bSkeletal)
        {
            Good=SkeletalSourceCurrent()&&SkeletalDescriptionsSame(BP->NewVariables,SkeletalDescriptions)&&SkeletalDescriptionsSame(BP->GeneratedVariables,SkeletalGeneratedDescriptions);
            if(Good&&KeepSkeletalCDO.Get()){const auto Handle=MaterialValueCDOHandle(Class);const UObject* HeldCDO=KeepSkeletalCDO.Get();Good=Handle.IsResolved()&&Handle==HeldCDO&&Loaded(HeldCDO)&&HeldCDO->GetClass()==Class&&HeldCDO->GetOuter()==NativePackage&&HeldCDO->GetPathName()==SkeletalCDOPath&&HeldCDO->GetFlags()==SkeletalCDOFlags&&HeldCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject);}
        }
        const int32 CurrentIndex=Good?FBlueprintEditorUtils::FindNewVariableIndex(BP,Name):INDEX_NONE;
        Good=Good&&CurrentIndex!=INDEX_NONE;
        if(Good)
        {
            const auto& D=BP->NewVariables[CurrentIndex];
            Good=D.VarName==Description.VarName&&D.VarGuid==Description.VarGuid&&D.VarType==Expected&&D.DefaultValue==Description.DefaultValue
                &&D.VarType.bIsUObjectWrapper==Expected.bIsUObjectWrapper&&D.VarType.bSerializeAsSinglePrecisionFloat==Expected.bSerializeAsSinglePrecisionFloat
                &&D.VarType.PinValueType.bTerminalIsUObjectWrapper==Expected.PinValueType.bTerminalIsUObjectWrapper
                &&Skeleton->FindPropertyByName(Name)==Array&&Array->GetOwnerClass()==Skeleton&&Array->ArrayDim==1
                &&Array->Inner==Inner&&Array->GetPropertyFlags()==ArrayFlags&&Inner->GetClass()==FObjectProperty::StaticClass()
                &&Inner->GetOwnerProperty()==Array&&Inner->ArrayDim==1&&Inner->GetPropertyFlags()==InnerFlags&&Inner->PropertyClass.IsResolved()&&Inner->PropertyClass==Class
                &&Array->GetOffset_ForInternal()==ArrayOffset&&Array->GetSize()==ArraySize&&Inner->GetOffset_ForInternal()==InnerOffset&&Inner->GetSize()==InnerSize;
        }
        if(!Good){R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("observation_incomplete"),true);
            Fail(R,TEXT("graph_object_array_scope_changed"),TEXT("Current own declaration/skeleton/native source roles changed; no array value access, node retry or repair was dispatched."));}
        return Good;
    };
    if(!Current())return R;
    const auto* Schema=Cast<UEdGraphSchema_K2>(Graph->GetSchema());if(!Current())return R;
    if(!Schema||Schema->GetClass()!=UEdGraphSchema_K2::StaticClass())
    {Fail(R,TEXT("graph_object_array_schema_unsupported"),TEXT("Only the exact standard K2 schema is supported."));return R;}
    FEdGraphPinType Actual;const bool Converted=Schema->ConvertPropertyToPinType(Array,Actual);if(!Current())return R;
    if(!Converted||Actual!=Expected||Actual.bIsUObjectWrapper!=Expected.bIsUObjectWrapper
        ||Actual.bSerializeAsSinglePrecisionFloat!=Expected.bSerializeAsSinglePrecisionFloat
        ||Actual.PinValueType.bTerminalIsUObjectWrapper!=Expected.PinValueType.bTerminalIsUObjectWrapper){Fail(R,TEXT("graph_member_skeleton_mismatch"),TEXT("Exact own object-array declaration and public skeleton schema conversion differ."));return R;}
    const auto Readable=FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP,Array);if(!Current())return R;
    if(Readable!=FBlueprintEditorUtils::EPropertyReadableState::Readable)
    {Fail(R,TEXT("graph_member_access_rejected"),TEXT("The exact own object-array member is not Blueprint-readable."));return R;}
    const UObject* CDO=nullptr;
    if(bSkeletal)
    {
        const auto Handle=MaterialValueCDOHandle(Class);
        if(!Handle.IsResolved()){Fail(R,TEXT("graph_object_array_native_source_unavailable"),TEXT("The existing native CDO handle must be resolved; no reference load or creation."));return R;}
        CDO=FindObject<UObject>(nullptr,TEXT("/Script/Engine.Default__SkeletalMeshComponent"));
        if(Handle!=CDO||!Loaded(CDO)){Fail(R,TEXT("graph_object_array_native_source_unavailable"),TEXT("The exact native SkeletalMeshComponent CDO must already exist."));return R;}
        KeepSkeletalCDO.Reset(CDO);SkeletalCDOPath=CDO->GetPathName();SkeletalCDOFlags=CDO->GetFlags();if(!Current())return R;
        if(MaterialValueCDOHandle(Class)!=Handle){Fail(R,TEXT("graph_object_array_native_source_changed"),TEXT("The native CDO handle changed during the bounded existing-object lookup."));return R;}
    }
    else {CDO=Class->GetDefaultObject(false);if(!Current())return R;}
    if(!Loaded(CDO)||CDO->GetClass()!=Class||!CDO->HasAnyFlags(RF_ClassDefaultObject)
        ||(bSkeletal&&(CDO->GetOuter()!=NativePackage||!CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))))
    {Fail(R,TEXT("graph_object_array_native_source_unavailable"),TEXT("Native constraint requires its already-existing loaded CDO; no creation was requested."));return R;}
    TStrongObjectPtr<const UObject> KeepCDO(CDO);const FString CDOPath=CDO->GetPathName();const EObjectFlags CDOFlags=CDO->GetFlags();
    const auto Source=[&]()
    {
        auto J=MakeShared<FJsonObject>();
        const UObject* CurrentCDO=nullptr;
        if(bSkeletal){const auto Handle=MaterialValueCDOHandle(Class);if(!Current())return J;if(!Handle.IsResolved()||Handle!=CDO){Fail(R,TEXT("graph_object_array_native_source_changed"),TEXT("The shallow native SkeletalMeshComponent CDO handle changed; no tracked read or CDO creation."));return J;}CurrentCDO=CDO;}
        else {CurrentCDO=Class->GetDefaultObject(false);if(!Current())return J;}
        if(CurrentCDO!=CDO||!Loaded(CDO)||CDO->GetPathName()!=CDOPath||CDO->GetFlags()!=CDOFlags||CDO->GetClass()!=Class||(bSkeletal&&(CDO->GetOuter()!=NativePackage||!CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))))
        {Fail(R,TEXT("graph_object_array_native_source_changed"),TEXT("Native constraint existing CDO identity/flags changed; no default value was read or created."));return J;}
        J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);
        J->SetStringField(TEXT("class_path"),ClassPath);J->SetStringField(TEXT("class_flags_string"),LexToString(uint32(ClassFlags)));
        J->SetStringField(TEXT("class_generated_by_path"),TEXT(""));J->SetStringField(TEXT("package_path"),TEXT("/Script/Engine"));
        J->SetNumberField(TEXT("class_structure_size"),ClassSize);J->SetStringField(TEXT("package_flags_string"),TEXT("16"));J->SetBoolField(TEXT("package_dirty"),false);
        J->SetBoolField(TEXT("existing_cdo_available"),true);J->SetStringField(TEXT("existing_cdo_path"),CDOPath);J->SetStringField(TEXT("existing_cdo_flags_string"),LexToString(uint32(CDOFlags)));
        J->SetBoolField(TEXT("cdo_creation_requested"),false);J->SetBoolField(TEXT("cdo_values_sampled"),false);
        if(bSkeletal){TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& V:SkeletalRoles){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),V.Path);Row->SetStringField(TEXT("class_flags_string"),LexToString(V.ClassFlags));Row->SetNumberField(TEXT("class_size"),V.Size);Row->SetStringField(TEXT("package_path"),V.Package->GetName());Row->SetStringField(TEXT("package_flags_string"),TEXT("16"));Row->SetBoolField(TEXT("package_dirty"),false);Rows.Add(MakeShared<FJsonValueObject>(Row));}J->SetArrayField(TEXT("hierarchy"),Rows);}return J;
    };
    const auto Before=Source();if(!Current()||!Before->HasField(TEXT("observations_complete")))return R;
    R->SetObjectField(TEXT("native_object_array_source_before"),Before);
    const auto ParentProof=InspectBlueprintParent(Parent->GetPathName());if(!Current())return R;R->SetObjectField(TEXT("parent_inspection"),ParentProof);
    if(!ParentProof->GetBoolField(TEXT("success"))){Fail(R,TEXT("graph_member_parent_unsupported"),TEXT("The target parent failed the existing runtime parent guard."));return R;}
    FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&Actual,&Actual,nullptr,PPF_None,nullptr);if(!Current())return R;
    const auto After=Source();if(!Current()||!After->HasField(TEXT("observations_complete")))return R;
    const bool Preserved=FJsonValue::CompareEqual(FJsonValueObject(Before),FJsonValueObject(After));
    R->SetObjectField(TEXT("native_object_array_source_after"),After);R->SetBoolField(TEXT("native_object_array_source_observed_unchanged"),Preserved);
    if(!Preserved){Fail(R,TEXT("graph_object_array_native_source_changed"),TEXT("Native source metadata changed during own array planning; no node dispatch or retry."));return R;}
    R->SetStringField(TEXT("member_scope"),TEXT("own_native_object_array"));R->SetStringField(TEXT("variable_name"),VariableName);
    R->SetStringField(TEXT("variable_guid"),Description.VarGuid.ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("setter"),bSetter);
    R->SetStringField(TEXT("property_path"),Array->GetPathName());R->SetStringField(TEXT("property_owner_class_path"),Skeleton->GetPathName());
    R->SetStringField(TEXT("skeleton_class_path"),Skeleton->GetPathName());R->SetStringField(TEXT("object_constraint_path"),ClassPath);
    R->SetStringField(TEXT("property_flags_string"),LexToString(uint64(ArrayFlags)));R->SetStringField(TEXT("inner_property_flags_string"),LexToString(uint64(InnerFlags)));
    R->SetNumberField(TEXT("property_array_dim"),Array->ArrayDim);R->SetStringField(TEXT("inner_property_path"),Inner->GetPathName());
    R->SetBoolField(TEXT("object_array_property_identity_verified"),true);R->SetBoolField(TEXT("object_array_wrapper_matches"),true);
    R->SetObjectField(TEXT("pin_type"),PinTypeObservation(Actual));R->SetStringField(TEXT("expected_pin_type_text"),TypeText);
    R->SetStringField(TEXT("pin_category"),TEXT("object"));R->SetStringField(TEXT("pin_subcategory"),TEXT("None"));
    R->SetStringField(TEXT("stored_default_before"),Description.DefaultValue);R->SetStringField(TEXT("stored_default_after"),Description.DefaultValue);
    R->SetBoolField(TEXT("stored_default_observed_unchanged"),true);R->SetBoolField(TEXT("array_value_read_attempted"),false);
    R->SetBoolField(TEXT("array_default_write_attempted"),false);R->SetBoolField(TEXT("object_array_runtime_evaluation_verified"),false);
    R->SetStringField(TEXT("scope"),bSkeletal?TEXT("Own transient exact SkeletalMeshComponent[] getter node-authoring only; no scalar/setter/nonempty default, native class or CDO creation, runtime array read, compile/save or pipeline execution."):TEXT("Own declared ActorComponent[]/ChildActorComponent[] node-authoring only; exact native strong object constraint/skeleton/GUID/current source metadata and stored declaration default. No runtime array values, private fields, inherited/external arrays, load, CDO creation, compile/save or callback purity proof."));
    if(!Current())return R;Complete(R,Graph);if(!Current())R->SetBoolField(TEXT("success"),false);return R;
}

TSharedRef<FJsonObject> ReadNativeChildActorMemberPlan(UEdGraph* G,const FString& VariableName,bool bSetter,
    const TSharedRef<FJsonObject>& R)
{
    // KT: one observed native Blueprint-readonly surface; never read the private runtime value.
    if(bSetter||VariableName!=TEXT("ChildActor"))
    {Fail(R,TEXT("graph_native_member_selector_unsupported"),TEXT("Only exact ChildActorComponent.ChildActor getter is admitted here; setters/other native members remain unsupported."));return R;}
    const auto Loaded=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    UBlueprint* BP=FBlueprintEditorUtils::FindBlueprintForGraph(G);
    UClass* Class=FindObject<UClass>(nullptr,TEXT("/Script/Engine.ChildActorComponent"));UClass* Actor=AActor::StaticClass();
    if(!Loaded(BP)||!Loaded(G)||!Loaded(Class)||Class!=UChildActorComponent::StaticClass()||!Loaded(Actor)
        ||!RuntimeObject::IsSupportedClass(Class)||!RuntimeObject::IsSupportedClass(Actor))
    {Fail(R,TEXT("graph_native_member_scope_unsupported"),TEXT("Requires an exact already-loaded native CAC class, Actor result class and loaded target Blueprint graph."));return R;}
    TStrongObjectPtr<UBlueprint> KeepBP(BP);TStrongObjectPtr<UEdGraph> KeepGraph(G);TStrongObjectPtr<UClass> KeepClass(Class),KeepActor(Actor);
    UClass* Parent=BP->ParentClass.Get();UClass* Generated=BP->GeneratedClass.Get();UClass* Skeleton=BP->SkeletonGeneratedClass.Get();
    TStrongObjectPtr<UClass> KeepParent(Parent),KeepGenerated(Generated),KeepSkeleton(Skeleton);
    const EObjectFlags BPFlags=BP->GetFlags(),GraphFlags=G->GetFlags(),ClassFlags=Class->GetFlags(),ActorFlags=Actor->GetFlags();
    const FString BPPath=BP->GetPathName(),GraphPath=G->GetPathName();const FGuid GraphGuid=G->GraphGuid;
    const auto Undo=GUndo;const uint32 ActorClassFlags=uint32(Actor->GetClassFlags());const int32 ClassSize=Class->GetStructureSize(),ActorSize=Actor->GetStructureSize();
    const FProperty* Property=Class->FindPropertyByName(TEXT("ChildActor"));const auto Object=CastField<FObjectProperty>(Property);
    const auto Current=[&]()
    {
        const bool Good=IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge
            &&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&GUndo==Undo
            &&Loaded(BP)&&Loaded(G)&&Loaded(Class)&&Loaded(Actor)&&Loaded(Parent)&&Loaded(Generated)&&Loaded(Skeleton)
            &&BP->GetPathName()==BPPath&&G->GetPathName()==GraphPath&&G->GetOuter()==BP&&G->GraphGuid==GraphGuid
            &&BP->GetFlags()==BPFlags&&G->GetFlags()==GraphFlags&&BP->ParentClass.GetGCPtr()==Parent
            &&BP->GeneratedClass.GetGCPtr()==Generated&&BP->SkeletonGeneratedClass.GetGCPtr()==Skeleton
            &&BP->BlueprintType==BPTYPE_Normal&&!BP->bBeingCompiled&&!BP->bQueuedForCompilation
            &&!BP->bIsRegeneratingOnLoad&&!BP->bSuppressStructurallyModified
            &&Class->GetFlags()==ClassFlags&&Actor->GetFlags()==ActorFlags&&Class->GetStructureSize()==ClassSize
            &&Actor->GetStructureSize()==ActorSize&&uint32(Actor->GetClassFlags())==ActorClassFlags&&uint32(Class->GetClassFlags())==820510884U
            &&Class->ClassGeneratedBy==nullptr&&Class->GetPathName()==TEXT("/Script/Engine.ChildActorComponent")
            &&Actor->GetPathName()==TEXT("/Script/Engine.Actor")&&Actor->HasAnyClassFlags(CLASS_Native)
            &&!Actor->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)&&Actor->ClassGeneratedBy==nullptr
            &&Class->GetPackage()->GetName()==TEXT("/Script/Engine")&&Class->GetPackage()->GetPackageFlags()==16U
            &&!Class->GetPackage()->IsDirty()&&Actor->GetPackage()==Class->GetPackage()
            &&Property&&Object&&Class->FindPropertyByName(TEXT("ChildActor"))==Property&&Property->GetOwnerClass()==Class
            &&Property->ArrayDim==1&&Object->PropertyClass==Actor&&uint64(Property->GetPropertyFlags())==93520134028657204ULL;
        if(!Good){R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("observation_incomplete"),true);
            Fail(R,TEXT("graph_native_child_actor_scope_changed"),TEXT("Current native source/property/target/transaction roles changed; no runtime value read, getter retry or repair was dispatched."));}
        return Good;
    };
    if(!Current())return R;
    const auto* Schema=Cast<UEdGraphSchema_K2>(G->GetSchema());
    if(!Current())return R;
    if(!Schema||Schema->GetClass()!=UEdGraphSchema_K2::StaticClass())
    {Fail(R,TEXT("graph_native_member_scope_unsupported"),TEXT("Native CAC getter requires the exact standard K2 schema."));return R;}
    const bool SelfContext=Skeleton->IsChildOf(Class);if(!Current())return R;
    if(SelfContext){Fail(R,TEXT("graph_native_member_scope_unsupported"),TEXT("This finite native getter requires explicit CAC self; inherited component self context remains unsupported."));return R;}
    const UClass* Super=Class->GetSuperClass();if(!Current())return R;
    if(Super!=USceneComponent::StaticClass()){Fail(R,TEXT("graph_native_member_scope_unsupported"),TEXT("Exact CAC native SceneComponent parent is required."));return R;}
    FEdGraphPinType Type;const bool Converted=Schema->ConvertPropertyToPinType(Property,Type);if(!Current())return R;
    FEdGraphPinType Expected;Expected.PinCategory=UEdGraphSchema_K2::PC_Object;Expected.PinSubCategoryObject=Actor;Expected.bIsUObjectWrapper=true;
    const FString PropertyCppType=Property->GetCPPType();if(!Current())return R;
    const bool Exact=Converted&&Type==Expected&&Type.bIsUObjectWrapper&&!Type.bIsWeakPointer&&!Type.bIsReference&&!Type.bIsConst
        &&!Type.IsContainer()&&!Type.bSerializeAsSinglePrecisionFloat&&!Type.PinValueType.bTerminalIsUObjectWrapper
        &&Property->GetPathName()==TEXT("/Script/Engine.ChildActorComponent:ChildActor")
        &&PropertyCppType==TEXT("TObjectPtr<AActor>")&&Property->GetOffset_ForInternal()>=0
        &&int64(Property->GetOffset_ForInternal())+Property->GetSize()<=ClassSize;
    if(!Current())return R;R->SetBoolField(TEXT("native_property_identity_verified"),Exact);
    if(!Exact){Fail(R,TEXT("graph_native_member_property_mismatch"),TEXT("ChildActor must retain its exact own readonly TObjectPtr Actor property, wrapper-aware scalar output type and observed flags."));return R;}
    const bool Access=Property->HasAllPropertyFlags(CPF_BlueprintVisible|CPF_BlueprintReadOnly|CPF_NativeAccessSpecifierPrivate)
        &&!Property->HasAnyPropertyFlags(CPF_EditorOnly|CPF_NativeAccessSpecifierProtected|CPF_NativeAccessSpecifierPublic)
        &&Property->HasMetaData(TEXT("AllowPrivateAccess"))&&Property->GetMetaData(TEXT("AllowPrivateAccess"))==TEXT("true")
        &&!Property->GetBoolMetaData(TEXT("Private"))&&!Property->HasMetaData(TEXT("BlueprintGetter"))
        &&FBlueprintEditorUtils::IsPropertyPrivate(Property)
        &&FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP,Property)==FBlueprintEditorUtils::EPropertyReadableState::Readable;
    if(!Current())return R;
    if(!Access){Fail(R,TEXT("graph_native_member_access_rejected"),TEXT("Only the exact native private BlueprintReadOnly ChildActor surface with AllowPrivateAccess=true is admitted; no general private access or accessor invocation."));return R;}
    FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&Type,&Type,nullptr,PPF_None,nullptr);if(!Current())return R;
    FEdGraphPinType Self;Self.PinCategory=UEdGraphSchema_K2::PC_Object;Self.PinSubCategoryObject=Class;
    FString SelfText;FEdGraphPinType::StaticStruct()->ExportText(SelfText,&Self,&Self,nullptr,PPF_None,nullptr);if(!Current())return R;
    const auto SourceState=[&]()
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("class_path"),Class->GetPathName());
        J->SetStringField(TEXT("class_flags_string"),LexToString(uint32(Class->GetClassFlags())));
        J->SetStringField(TEXT("package_name"),Class->GetPackage()->GetName());J->SetBoolField(TEXT("package_dirty"),Class->GetPackage()->IsDirty());
        J->SetStringField(TEXT("property_path"),Property->GetPathName());J->SetStringField(TEXT("property_class"),Property->GetClass()->GetName());
        J->SetStringField(TEXT("property_flags_string"),LexToString(uint64(Property->GetPropertyFlags())));
        J->SetNumberField(TEXT("array_dim"),Property->ArrayDim);J->SetNumberField(TEXT("offset"),Property->GetOffset_ForInternal());
        J->SetNumberField(TEXT("size"),Property->GetSize());J->SetStringField(TEXT("cpp_type"),PropertyCppType);
        J->SetStringField(TEXT("object_class_path"),Actor->GetPathName());J->SetStringField(TEXT("allow_private_access"),Property->GetMetaData(TEXT("AllowPrivateAccess")));
        J->SetBoolField(TEXT("blueprint_read_only"),Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));J->SetBoolField(TEXT("native_private"),true);
        J->SetStringField(TEXT("pin_type_text"),TypeText);J->SetBoolField(TEXT("pin_is_uobject_wrapper"),Type.bIsUObjectWrapper);
        return J;
    };
    const auto Before=SourceState();if(!Current())return R;R->SetObjectField(TEXT("native_source_before"),Before);
    const auto ParentProof=InspectBlueprintParent(Parent->GetPathName());if(!Current())return R;R->SetObjectField(TEXT("parent_inspection"),ParentProof);
    if(!ParentProof->GetBoolField(TEXT("success"))){Fail(R,TEXT("graph_member_parent_unsupported"),TEXT("Target parent failed the existing runtime-class inspection."));return R;}
    const auto After=SourceState();if(!Current())return R;
    const bool Preserved=FJsonValue::CompareEqual(FJsonValueObject(Before),FJsonValueObject(After));
    R->SetObjectField(TEXT("native_source_after"),After);R->SetBoolField(TEXT("native_source_observed_unchanged"),Preserved);
    if(!Preserved){Fail(R,TEXT("graph_native_member_source_changed"),TEXT("The exact native CAC readonly property source changed during planning; no getter was dispatched."));return R;}
    R->SetStringField(TEXT("native_property_expected_flags_string"),TEXT("93520134028657204"));
    R->SetStringField(TEXT("native_property_flags_string"),LexToString(uint64(Property->GetPropertyFlags())));
    R->SetStringField(TEXT("member_scope"),TEXT("external_native_child_actor_readonly"));R->SetStringField(TEXT("variable_name"),TEXT("ChildActor"));
    R->SetStringField(TEXT("variable_guid"),FGuid().ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("setter"),false);
    R->SetStringField(TEXT("target_class_path"),Class->GetPathName());R->SetStringField(TEXT("declaring_class_path"),Class->GetPathName());
    R->SetStringField(TEXT("declaring_blueprint_path"),TEXT(""));R->SetStringField(TEXT("property_path"),Property->GetPathName());
    R->SetStringField(TEXT("compiled_property_path"),Property->GetPathName());R->SetStringField(TEXT("pin_category"),TEXT("object"));
    R->SetStringField(TEXT("pin_subcategory"),TEXT("None"));R->SetStringField(TEXT("expected_pin_type_text"),TypeText);
    R->SetStringField(TEXT("expected_self_pin_type_text"),SelfText);R->SetBoolField(TEXT("native_child_actor_private_readonly_verified"),true);
    R->SetBoolField(TEXT("native_child_actor_runtime_evaluation_verified"),false);R->SetBoolField(TEXT("native_cdo_value_sampled"),false);
    R->SetStringField(TEXT("scope"),TEXT("Exact loaded native CAC.ChildActor readonly Blueprint property metadata; zero native GUID, explicit CAC self and wrapper-aware Actor value pin. Public C++ GetChildActor is not a UFUNCTION and is not invoked. No private value/CDO sampling, arbitrary private field, setter, class load, spawn, compile/save or global callback purity proof."));
    if(!Current())return R;Complete(R,G);if(!Current())R->SetBoolField(TEXT("success"),false);return R;
}

// LV: four public native byte-enum setter declarations, never texture value writes.
TSharedRef<FJsonObject> ReadNativeTextureMemberPlan(UEdGraph* G,const FString& VariableName,bool bSetter,const FString& TargetClassPath,const TSharedRef<FJsonObject>& R)
{
    R->SetStringField(TEXT("member_scope"),TEXT("external_native_texture_enum_setter"));
    const bool Address=TargetClassPath==TEXT("/Script/Engine.TextureRenderTarget2D")&&(VariableName==TEXT("AddressX")||VariableName==TEXT("AddressY"));
    const bool Filter=TargetClassPath==TEXT("/Script/Engine.Texture")&&VariableName==TEXT("Filter");
    const bool Group=TargetClassPath==TEXT("/Script/Engine.Texture")&&VariableName==TEXT("LODGroup");
    auto Stop=[&](const TCHAR* Message){Fail(R,TEXT("graph_native_texture_member_scope_changed"),Message);return R;};
    if(!bSetter||(!Address&&!Filter&&!Group)){Fail(R,TEXT("graph_native_member_selector_unsupported"),TEXT("Only exact RT2D.AddressX/AddressY and Texture.Filter/LODGroup setters are admitted; getters, inherited selector aliases and other native fields remain unsupported."));return R;}
    auto Loaded=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    UBlueprint* BP=FBlueprintEditorUtils::FindBlueprintForGraph(G);if(!Loaded(BP)||!Loaded(G)||G->GetOuter()!=BP)return Stop(TEXT("An own loaded Blueprint graph is required."));
    TStrongObjectPtr<UBlueprint> HeldBP(BP);TStrongObjectPtr<UEdGraph> HeldGraph(G);TStrongObjectPtr<UEditorEngine> Editor(GEditor);TStrongObjectPtr<UWorld> EditorWorld(GWorld);auto* Undo=GUndo;
    const auto HasNativeBase=[](const UObject* O,const UClass* Base){for(auto It=O->GetClass()->GetSuperStructIterator();It;++It)if(*It==Base)return true;return false;};
    struct FRole{UObject* O;UClass* Class;UObject* Outer;UPackage* Package;FString Path;EObjectFlags Flags;uint32 PackageFlags,ClassFlags;int32 Size;bool Dirty,IsClass;};
    TArray<FRole> Roles;TArray<TStrongObjectPtr<UObject>> Held;
    auto Hold=[&](UObject* O){if(!Loaded(O)||!Loaded(O->GetPackage()))return false;for(const auto& A:Roles)if(A.O==O)return true;Held.Emplace(O);const bool IsClass=HasNativeBase(O,UClass::StaticClass());UClass* K=IsClass?static_cast<UClass*>(O):nullptr;Roles.Add({O,O->GetClass(),O->GetOuter(),O->GetPackage(),O->GetPathName(),O->GetFlags(),uint32(O->GetPackage()->GetPackageFlags()),K?uint32(K->GetClassFlags()):0,K?K->GetStructureSize():0,O->GetPackage()->IsDirty(),IsClass});return true;};
    auto ResolveClass=[&](const TObjectPtr<UClass>& H)->UClass*{if(!H.IsResolved()||H==nullptr)return nullptr;UClass* K=FindObject<UClass>(nullptr,*H.GetPathName());return H==K&&Hold(K)?K:nullptr;};
    if(!Hold(BP)||!Hold(G)||!Hold(BP->GetPackage())||!Loaded(Editor.Get()))return Stop(TEXT("Target identities are not loaded."));
    UClass* Parent=ResolveClass(BP->ParentClass.GetGCPtr());UClass* Generated=ResolveClass(BP->GeneratedClass.GetGCPtr());UClass* Skeleton=ResolveClass(BP->SkeletonGeneratedClass.GetGCPtr());
    UClass* Class=FindObject<UClass>(nullptr,*TargetClassPath);UClass* ExpectedClass=Address?UTextureRenderTarget2D::StaticClass():UTexture::StaticClass();
    if(!Parent||!Generated||!Skeleton||Class!=ExpectedClass||!Hold(Class)||!Class->GetPathName().Equals(TargetClassPath,ESearchCase::CaseSensitive)||!Class->GetPackage()->GetName().Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)||Class->GetPackage()->IsDirty()||uint32(Class->GetPackage()->GetPackageFlags())!=16||!Class->HasAnyClassFlags(CLASS_Native)||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy!=nullptr)return Stop(TEXT("Exact native source and current target classes are required."));
    TArray<UClass*> Supers;for(auto It=Class->GetSuperStructIterator();It;++It){auto* K=static_cast<UClass*>(*It);if(Supers.Num()>=16||!Hold(K)||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy!=nullptr||K->GetPackage()->IsDirty())return Stop(TEXT("Native texture hierarchy is not current."));Supers.Add(K);}
    const auto SourceCDOHandle=MaterialValueCDOHandle(Class);UObject* SourceCDO=nullptr;
    if(!SourceCDOHandle.IsResolved())return Stop(TEXT("Native source CDO handle must already be resolved."));
    if(SourceCDOHandle!=nullptr){SourceCDO=FindObject<UObject>(nullptr,*SourceCDOHandle.GetPathName());if(SourceCDOHandle!=SourceCDO||!Hold(SourceCDO)||SourceCDO->GetClass()!=Class||SourceCDO->GetOuter()!=Class->GetPackage()||!SourceCDO->HasAnyFlags(RF_ClassDefaultObject)||!SourceCDO->GetPathName().Equals(Address?TEXT("/Script/Engine.Default__TextureRenderTarget2D"):TEXT("/Script/Engine.Default__Texture"),ESearchCase::CaseSensitive))return Stop(TEXT("Existing native source CDO identity is not exact."));}
    UClass* SchemaClass=UEdGraphSchema_K2::StaticClass();const auto SchemaHandle=G->Schema.GetGCPtr();const auto SchemaCDO=MaterialValueCDOHandle(SchemaClass);
    if(!SchemaHandle.IsResolved()||SchemaHandle!=SchemaClass||!SchemaCDO.IsResolved()||SchemaCDO==nullptr||!Hold(SchemaClass))return Stop(TEXT("Exact resolved native K2 schema is required."));
    UObject* SchemaObject=FindObject<UObject>(nullptr,*SchemaCDO.GetPathName());if(SchemaCDO!=SchemaObject||!Hold(SchemaObject)||SchemaObject->GetClass()!=SchemaClass||!SchemaObject->HasAnyFlags(RF_ClassDefaultObject)||!SchemaObject->GetPathName().Equals(TEXT("/Script/BlueprintGraph.Default__EdGraphSchema_K2"),ESearchCase::CaseSensitive))return Stop(TEXT("Existing K2 schema CDO is unavailable."));const auto* Schema=static_cast<const UEdGraphSchema_K2*>(SchemaObject);
    auto Field=[&]()->FByteProperty*{for(TFieldIterator<FProperty> It(Class,EFieldIterationFlags::None);It;++It)if(It->GetFName()==FName(*VariableName))return CastField<FByteProperty>(*It);return nullptr;};
    FByteProperty* Property=Field();const FString EnumName=Address?TEXT("TextureAddress"):Filter?TEXT("TextureFilter"):TEXT("TextureGroup");const FString EnumPath=TEXT("/Script/Engine.")+EnumName;
    if(!Property||Property->GetClass()!=FByteProperty::StaticClass()||Property->GetOwnerClass()!=Class||Property->ArrayDim!=1||Property->HasSetterOrGetter()||Property->GetSize()!=1||Property->GetOffset_ForInternal()<0||Property->GetOffset_ForInternal()+1>Class->GetStructureSize()||!Property->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible|CPF_AssetRegistrySearchable|CPF_NativeAccessSpecifierPublic)||Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly|CPF_EditorOnly|CPF_Parm|CPF_NativeAccessSpecifierPrivate|CPF_NativeAccessSpecifierProtected)||Property->HasAnyPropertyFlags(CPF_AdvancedDisplay)!=Filter)return Stop(TEXT("Exact own public Blueprint-writable native TEnumAsByte descriptor is required; accessor-backed/private/readonly fields are not admitted."));
    const auto EnumHandle=Property->Enum;if(!EnumHandle.IsResolved()||EnumHandle==nullptr)return Stop(TEXT("Native byte enum handle is not resolved."));
    UEnum* Enum=FindObject<UEnum>(nullptr,*EnumPath);if(!Hold(Enum)||EnumHandle!=Enum||Enum->GetClass()!=UEnum::StaticClass()||!Enum->GetPathName().Equals(EnumPath,ESearchCase::CaseSensitive)||Enum->GetPackage()!=Class->GetPackage()||Enum->GetCppForm()!=UEnum::ECppForm::Regular||Enum->NumEnums()<2||Enum->NumEnums()>256)return Stop(TEXT("Exact already-loaded regular native texture enum required; UserDefinedEnum and loaders are excluded."));
    const uint64 PropertyFlags=uint64(Property->GetPropertyFlags());const int32 Offset=Property->GetOffset_ForInternal();const bool FlagEnum=Enum->HasAnyEnumFlags(EEnumFlags::Flags),NewerEnum=Enum->HasAnyEnumFlags(EEnumFlags::NewerVersionExists);const FString CppType=Enum->CppType;const auto Underlying=Enum->GetUnderlyingType();const auto CppForm=Enum->GetCppForm();const int32 EnumCount=Enum->NumEnums();
    struct FNode{TStrongObjectPtr<UEdGraphNode> N;FGuid Guid;TArray<UEdGraphPin*> Pins;bool Variable=false,Deprecated=false;FMemberReference Reference;};TArray<FNode> Nodes;TArray<TObjectPtr<UEdGraphNode>> Inventory=G->Nodes;TSet<UEdGraphPin*> AllPins;
    for(const auto& H:Inventory){if(!H.IsResolved()||H==nullptr)return Stop(TEXT("Current node handles must be resolved."));UEdGraphNode* N=FindObject<UEdGraphNode>(nullptr,*H.GetPathName());if(H!=N||!Hold(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid()||N->Pins.Num()>256||Nodes.Num()>=2048)return Stop(TEXT("Bounded current node inventory required."));FNode Row;Row.N.Reset(N);Row.Guid=N->NodeGuid;Row.Pins=N->Pins;if(HasNativeBase(N,UK2Node_Variable::StaticClass())){const auto* V=static_cast<UK2Node_Variable*>(N);Row.Variable=true;Row.Reference=V->VariableReference;Row.Deprecated=V->VariableReference.IsDeprecated();}for(auto* P:Row.Pins)if(!P||AllPins.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid())return Stop(TEXT("Current unique owned physical pins required."));else AllPins.Add(P);Nodes.Add(MoveTemp(Row));}
    const auto PinFlags=[](const UEdGraphPin* P){return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)|(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)|(uint32(P->ShouldSavePinIfOrphaned())<<9);};
    const auto SameType=[](const FEdGraphPinType& A,const FEdGraphPinType& B){return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;};
    struct FPin{UEdGraphPin* P;UEdGraphNode* Owner;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;uint32 Flags;FString Default,Auto;FText Text;TObjectPtr<UObject> Object;TArray<UEdGraphPin*> Links,Children;UEdGraphPin* Parent;};TArray<FPin> Pins;
    for(const auto& N:Nodes)for(UEdGraphPin* P:N.Pins){if(!P->DefaultObject.IsResolved())return Stop(TEXT("Pin default handles must be resolved."));for(auto* L:P->LinkedTo)if(!AllPins.Contains(L)||!L->LinkedTo.Contains(P))return Stop(TEXT("Links must be current reciprocal in-inventory peers."));Pins.Add({P,N.N.Get(),P->PinId,P->PinName,P->Direction,P->PinType,PinFlags(P),P->DefaultValue,P->AutogeneratedDefaultValue,P->DefaultTextValue,P->DefaultObject,P->LinkedTo,P->SubPins,P->ParentPin});}
    const auto HostStatus=BP->Status;const auto GraphGuid=G->GraphGuid;
    auto Current=[&]()
    {
        if(!IsEditorGameThread()||GIsImportingT3D||IsPlaySessionActive()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Loaded(EditorWorld.Get())||GWorld!=EditorWorld.Get()||EditorWorld->WorldType!=EWorldType::Editor||EditorWorld->bIsTearingDown||GUndo!=Undo)return false;
        for(const auto& A:Roles)if(!Loaded(A.O)||A.O->GetClass()!=A.Class||A.O->GetOuter()!=A.Outer||A.O->GetPackage()!=A.Package||!A.O->GetPathName().Equals(A.Path,ESearchCase::CaseSensitive)||A.O->GetFlags()!=A.Flags||uint32(A.Package->GetPackageFlags())!=A.PackageFlags||A.Package->IsDirty()!=A.Dirty||(A.IsClass&&(uint32(static_cast<UClass*>(A.O)->GetClassFlags())!=A.ClassFlags||static_cast<UClass*>(A.O)->GetStructureSize()!=A.Size)))return false;
        if(BP->BlueprintType!=BPTYPE_Normal||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified||BP->Status!=HostStatus||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=Parent||!BP->GeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr()!=Generated||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->SkeletonGeneratedClass.GetGCPtr()!=Skeleton||G->GetOuter()!=BP||G->GraphGuid!=GraphGuid||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=SchemaClass)return false;
        const auto CDO=MaterialValueCDOHandle(Class),SCDO=MaterialValueCDOHandle(SchemaClass);if(!CDO.IsResolved()||CDO!=SourceCDOHandle||CDO!=SourceCDO||!SCDO.IsResolved()||SCDO!=SchemaObject)return false;
        int32 I=0;for(auto It=Class->GetSuperStructIterator();It;++It,++I)if(I>=Supers.Num()||*It!=Supers[I])return false;if(I!=Supers.Num())return false;
        // Reacquire the own field before dereferencing its captured address. Enum is a shallow resolved wrapper comparison.
        if(Field()!=Property||Property->GetClass()!=FByteProperty::StaticClass()||Property->GetOwnerClass()!=Class||uint64(Property->GetPropertyFlags())!=PropertyFlags||Property->GetOffset_ForInternal()!=Offset||Property->GetSize()!=1||Property->ArrayDim!=1||Property->HasSetterOrGetter()||!Property->Enum.IsResolved()||Property->Enum!=Enum||Enum->NumEnums()!=EnumCount||Enum->HasAnyEnumFlags(EEnumFlags::Flags)!=FlagEnum||Enum->HasAnyEnumFlags(EEnumFlags::NewerVersionExists)!=NewerEnum||!Enum->CppType.Equals(CppType,ESearchCase::CaseSensitive)||Enum->GetUnderlyingType()!=Underlying||Enum->GetCppForm()!=CppForm)return false;
        if(G->Nodes.Num()!=Inventory.Num())return false;for(int32 J=0;J<Inventory.Num();++J){if(!G->Nodes[J].IsResolved()||G->Nodes[J]!=Nodes[J].N.Get()||Nodes[J].N->NodeGuid!=Nodes[J].Guid||Nodes[J].N->Pins!=Nodes[J].Pins)return false;if(Nodes[J].Variable){const auto* V=static_cast<UK2Node_Variable*>(Nodes[J].N.Get());if(!V->VariableReference.IsSameReference(Nodes[J].Reference)||V->VariableReference.IsDeprecated()!=Nodes[J].Deprecated)return false;}}
        for(const auto& P:Pins)if(!P.P||P.P->WasTrashed()||P.P->GetOwningNodeUnchecked()!=P.Owner||P.P->PinId!=P.Id||P.P->PinName!=P.Name||P.P->Direction!=P.Direction||!SameType(P.P->PinType,P.Type)||PinFlags(P.P)!=P.Flags||!P.P->DefaultValue.Equals(P.Default,ESearchCase::CaseSensitive)||!P.P->AutogeneratedDefaultValue.Equals(P.Auto,ESearchCase::CaseSensitive)||!P.P->DefaultTextValue.IdenticalTo(P.Text)||!P.P->DefaultObject.IsResolved()||P.P->DefaultObject!=P.Object||P.P->LinkedTo!=P.Links||P.P->SubPins!=P.Children||P.P->ParentPin!=P.Parent)return false;
        return true;
    };
    if(!Current()||FlagEnum||NewerEnum||!CppType.Equals(EnumName,ESearchCase::CaseSensitive)||Underlying!=UEnum::EUnderlyingType::int32)return Stop(TEXT("Initial current native enum/target roles are incomplete."));
    auto EnumState=[&](TArray<TSharedPtr<FJsonValue>>& Entries)->bool
    {
        for(int32 I=0;I<EnumCount;++I){auto V=MakeShared<FJsonObject>();const FString Name=Enum->GetNameStringByIndex(I);const int64 Value=Enum->GetValueByIndex(I);const bool Hidden=Enum->HasMetaData(TEXT("Hidden"),I);if(!Current())return false;V->SetStringField(TEXT("name"),Name);V->SetStringField(TEXT("value_string"),LexToString(Value));V->SetBoolField(TEXT("hidden"),Hidden);V->SetBoolField(TEXT("sentinel"),Name.EndsWith(TEXT("_MAX"),ESearchCase::CaseSensitive));Entries.Add(MakeShared<FJsonValueObject>(V));}return true;
    };
    FEdGraphPinType Type;const bool Converted=Schema->ConvertPropertyToPinType(Property,Type);if(!Current())return Stop(TEXT("Current source changed after property-to-pin conversion."));
    FEdGraphPinType Expected;Expected.PinCategory=UEdGraphSchema_K2::PC_Byte;Expected.PinSubCategoryObject=Enum;
    if(!Converted||!SameType(Type,Expected))return Stop(TEXT("Exact scalar byte/native enum pin type required."));
    const bool Accessible=FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP,Property)==FBlueprintEditorUtils::EPropertyReadableState::Readable&&FBlueprintEditorUtils::IsPropertyWritableInBlueprint(BP,Property)==FBlueprintEditorUtils::EPropertyWritableState::Writable;if(!Current())return Stop(TEXT("Current source changed during public access inspection."));if(!Accessible)return Stop(TEXT("Public writable property access denied."));
    FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&Type,nullptr,nullptr,PPF_None,nullptr);if(!Current())return Stop(TEXT("Current source changed after value type export."));
    FEdGraphPinType SelfType;SelfType.PinCategory=UEdGraphSchema_K2::PC_Object;SelfType.PinSubCategoryObject=Class;FString SelfText;FEdGraphPinType::StaticStruct()->ExportText(SelfText,&SelfType,nullptr,nullptr,PPF_None,nullptr);if(!Current())return Stop(TEXT("Current source changed after self type export."));
    const auto ParentInspection=InspectBlueprintParent(Parent->GetPathName());if(!Current())return Stop(TEXT("Current target changed during parent inspection."));if(!ParentInspection->GetBoolField(TEXT("success")))return Stop(TEXT("Target parent failed existing runtime-class inspection."));
    auto Source=[&](TSharedRef<FJsonObject> J)->bool{TArray<TSharedPtr<FJsonValue>> Entries;if(!EnumState(Entries))return false;J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("class_path"),TargetClassPath);J->SetStringField(TEXT("package_path"),Class->GetPackage()->GetName());J->SetBoolField(TEXT("package_dirty"),Class->GetPackage()->IsDirty());J->SetStringField(TEXT("class_flags_string"),LexToString(uint32(Class->GetClassFlags())));J->SetNumberField(TEXT("class_size"),Class->GetStructureSize());J->SetStringField(TEXT("property_path"),Property->GetPathName());J->SetStringField(TEXT("property_class"),TEXT("ByteProperty"));J->SetStringField(TEXT("property_flags_string"),LexToString(PropertyFlags));J->SetNumberField(TEXT("property_offset"),Offset);J->SetNumberField(TEXT("property_size"),1);J->SetNumberField(TEXT("array_dim"),1);J->SetStringField(TEXT("enum_path"),EnumPath);J->SetBoolField(TEXT("enum_flags"),FlagEnum);J->SetBoolField(TEXT("enum_newer_version"),NewerEnum);J->SetStringField(TEXT("enum_cpp_type"),CppType);J->SetArrayField(TEXT("enum_entries"),Entries);J->SetBoolField(TEXT("existing_cdo_available"),SourceCDO!=nullptr);J->SetStringField(TEXT("existing_cdo_path"),SourceCDO?SourceCDO->GetPathName():FString());J->SetBoolField(TEXT("cdo_creation_requested"),false);J->SetBoolField(TEXT("cdo_values_sampled"),false);return Current();};
    auto Before=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>();if(!Source(Before))return Stop(TEXT("Source observation incomplete."));
    FMemberReference ExpectedReference;ExpectedReference.SetExternalMember(FName(*VariableName),Class);TArray<TSharedPtr<FJsonValue>> MemberNodes;
    for(const auto& N:Nodes)
    {
        if(N.N->GetClass()!=UK2Node_VariableSet::StaticClass())continue;auto* V=static_cast<UK2Node_VariableSet*>(N.N.Get());if(!V->VariableReference.IsSameReference(ExpectedReference)||V->VariableReference.IsDeprecated())continue;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("object_path"),V->GetPathName());Row->SetStringField(TEXT("node_guid"),N.Guid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("node_class_path"),TEXT("/Script/BlueprintGraph.K2Node_VariableSet"));Row->SetStringField(TEXT("graph_path"),G->GetPathName());Row->SetStringField(TEXT("outer_path"),V->GetOuter()->GetPathName());Row->SetStringField(TEXT("member_name"),VariableName);Row->SetStringField(TEXT("member_guid"),FGuid().ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("member_parent_class"),TargetClassPath);Row->SetBoolField(TEXT("member_self_context"),false);Row->SetStringField(TEXT("property_path"),Property->GetPathName());Row->SetNumberField(TEXT("x"),V->NodePosX);Row->SetNumberField(TEXT("y"),V->NodePosY);TArray<TSharedPtr<FJsonValue>> Physical;int32 Values=0,Selfs=0;
        for(const auto& P:Pins)if(N.Pins.Contains(P.P))
        {
            if(P.P->ParentPin||!P.P->SubPins.IsEmpty()||P.P->bOrphanedPin||P.P->bDeprecated||P.P->bNotConnectable)return Stop(TEXT("Native setter whole pins are required."));
            if(P.Name==FName(*VariableName)&&P.Direction==EGPD_Input){if(!SameType(P.Type,Type)||P.P->bHidden||P.P->bDefaultValueIsReadOnly||P.P->bDefaultValueIsIgnored||P.Object!=nullptr)return Stop(TEXT("Exact visible writable enum input required."));++Values;}
            if(P.Name==UEdGraphSchema_K2::PN_Self&&P.Direction==EGPD_Input){if(!SameType(P.Type,SelfType)||P.P->bHidden||P.Object!=nullptr||!P.Default.IsEmpty())return Stop(TEXT("Exact explicit native texture self input required."));++Selfs;}
            FString PinText;FEdGraphPinType::StaticStruct()->ExportText(PinText,&P.Type,nullptr,nullptr,PPF_None,nullptr);if(!Current())return Stop(TEXT("Current setter pin changed during export."));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("id"),P.Id.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("name"),P.Name.ToString());J->SetStringField(TEXT("direction"),P.Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetStringField(TEXT("type_text"),PinText);J->SetObjectField(TEXT("type"),PinTypeObservation(P.Type));const FString DefaultText=P.Text.ToString();if(!Current())return Stop(TEXT("Current setter changed during text observation."));J->SetStringField(TEXT("default_text"),DefaultText);J->SetStringField(TEXT("default_object"),TEXT(""));J->SetNumberField(TEXT("flags"),P.Flags);J->SetStringField(TEXT("default"),P.Default);J->SetStringField(TEXT("auto"),P.Auto);TArray<TSharedPtr<FJsonValue>> Links;for(auto* Link:P.Links){auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("node_path"),Link->GetOwningNodeUnchecked()->GetPathName());L->SetStringField(TEXT("pin_id"),Link->PinId.ToString(EGuidFormats::Digits));L->SetStringField(TEXT("pin_name"),Link->PinName.ToString());Links.Add(MakeShared<FJsonValueObject>(L));}J->SetArrayField(TEXT("links"),Links);Physical.Add(MakeShared<FJsonValueObject>(J));
        }
        if(Values!=1||Selfs!=1)return Stop(TEXT("Unique native setter value and explicit self pins required."));Row->SetArrayField(TEXT("pins"),Physical);Row->SetBoolField(TEXT("whole_pins_current_verified"),true);MemberNodes.Add(MakeShared<FJsonValueObject>(Row));
    }
    if(!Source(After))return Stop(TEXT("Source re-observation incomplete."));FString A,B;FJsonSerializer::Serialize(Before,TJsonWriterFactory<>::Create(&A));FJsonSerializer::Serialize(After,TJsonWriterFactory<>::Create(&B));if(!A.Equals(B,ESearchCase::CaseSensitive)||!Current())return Stop(TEXT("Native texture enum source changed during this read."));
    R->SetStringField(TEXT("variable_name"),VariableName);R->SetStringField(TEXT("variable_guid"),FGuid().ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("setter"),true);R->SetStringField(TEXT("target_class_path"),TargetClassPath);R->SetStringField(TEXT("declaring_class_path"),TargetClassPath);R->SetStringField(TEXT("declaring_blueprint_path"),TEXT(""));R->SetStringField(TEXT("property_path"),Property->GetPathName());R->SetStringField(TEXT("compiled_property_path"),Property->GetPathName());R->SetStringField(TEXT("pin_category"),TEXT("byte"));R->SetStringField(TEXT("pin_subcategory"),TEXT("None"));R->SetObjectField(TEXT("pin_type"),PinTypeObservation(Type));R->SetStringField(TEXT("expected_pin_type_text"),TypeText);R->SetStringField(TEXT("expected_self_pin_type_text"),SelfText);R->SetObjectField(TEXT("native_source_before"),Before);R->SetObjectField(TEXT("native_source_after"),After);R->SetBoolField(TEXT("native_source_observed_unchanged"),true);R->SetBoolField(TEXT("native_property_identity_verified"),true);R->SetBoolField(TEXT("native_texture_runtime_value_written"),false);R->SetBoolField(TEXT("native_cdo_value_sampled"),false);R->SetStringField(TEXT("enum_path"),EnumPath);R->SetArrayField(TEXT("native_texture_member_nodes"),MemberNodes);R->SetStringField(TEXT("scope"),TEXT("Exact native public byte-enum member setter graph authoring only. No resource property values are read/written; no native getter, inherited selector alias, load, compile/save or runtime/GPU execution."));Complete(R,G);if(!Current()){R->SetBoolField(TEXT("success"),false);return Stop(TEXT("Current native setter roles changed at the final observation boundary."));}return R;
}
// LV NativeTextureMember_End
TSharedRef<FJsonObject> ReadNativePawnMemberPlan(UEdGraph* G, const FString& VariableName, bool bSetter,
    const TSharedRef<FJsonObject>& R)
{
    // AC1-r2 observed this exact native field. No general native access or
    // inherited/self member allowance, and no native setters, are introduced.
    if (bSetter || VariableName != TEXT("BaseEyeHeight"))
    { Fail(R, TEXT("graph_native_member_selector_unsupported"), TEXT("The native member branch admits only Pawn.BaseEyeHeight getter; setters and all other native properties remain unsupported.")); return R; }
    UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(G);
    UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/Engine.Pawn"));
    if (!IsValid(Class) || Class != APawn::StaticClass() || !RuntimeObject::IsSupportedClass(Class)
        || Class->GetPathName() != TEXT("/Script/Engine.Pawn") || Class->GetOutermost()->GetName() != TEXT("/Script/Engine")
        || !BP->SkeletonGeneratedClass || BP->SkeletonGeneratedClass->IsChildOf(Class)
        || BP->BlueprintType != BPTYPE_Normal || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bIsRegeneratingOnLoad
        || BP->bSuppressStructurallyModified || G->GetSchema()->GetClass() != UEdGraphSchema_K2::StaticClass())
    { Fail(R, TEXT("graph_native_member_scope_unsupported"), TEXT("Requires the exact already-loaded native runtime Pawn class and an idle standard K2 target outside Pawn self context.")); return R; }
    const FProperty* Property = Class->FindPropertyByName(TEXT("BaseEyeHeight"));
    FEdGraphPinType Type;
    const bool Converted = Property && GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, Type);
    FString TypeText;
    if (Converted) { FEdGraphPinType::StaticStruct()->ExportText(TypeText, &Type, &Type, nullptr, PPF_None, nullptr); }
    auto SourceState = [&]()
    {
        auto State = MakeShared<FJsonObject>();
        FEdGraphPinType ObservedType; FString ObservedTypeText;
        if (Property && GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, ObservedType))
        { FEdGraphPinType::StaticStruct()->ExportText(ObservedTypeText, &ObservedType, &ObservedType, nullptr, PPF_None, nullptr); }
        State->SetStringField(TEXT("class_path"), Class->GetPathName());
        State->SetStringField(TEXT("class_flags_string"), LexToString(static_cast<uint32>(Class->ClassFlags)));
        State->SetStringField(TEXT("package_name"), Class->GetOutermost()->GetName());
        State->SetBoolField(TEXT("package_dirty"), Class->GetOutermost()->IsDirty());
        State->SetStringField(TEXT("property_path"), Property ? Property->GetPathName() : FString());
        State->SetStringField(TEXT("property_class"), Property ? Property->GetClass()->GetName() : FString());
        State->SetStringField(TEXT("property_flags_string"), Property ? LexToString(static_cast<uint64>(Property->GetPropertyFlags())) : FString());
        State->SetNumberField(TEXT("array_dim"), Property ? Property->ArrayDim : 0);
        State->SetStringField(TEXT("pin_type_text"), ObservedTypeText);
        return State;
    };
    const auto Before = SourceState();
    R->SetObjectField(TEXT("native_source_before"), Before);
    R->SetStringField(TEXT("native_property_expected_flags_string"), TEXT("6755469234274821"));
    R->SetStringField(TEXT("native_property_flags_string"), Property ? LexToString(static_cast<uint64>(Property->GetPropertyFlags())) : FString());
    FEdGraphPinType ExpectedType;
    ExpectedType.PinCategory = UEdGraphSchema_K2::PC_Real; ExpectedType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    const bool ExactProperty = Property && CastField<FFloatProperty>(Property) && Property->GetOwnerClass() == Class
        && Property->GetName() == TEXT("BaseEyeHeight") && Property->ArrayDim == 1
        && static_cast<uint64>(Property->GetPropertyFlags()) == 6755469234274821ULL
        && Converted && Type == ExpectedType && !Type.bIsUObjectWrapper && !Type.bSerializeAsSinglePrecisionFloat
        && !Type.PinValueType.bTerminalIsUObjectWrapper;
    R->SetBoolField(TEXT("native_property_identity_verified"), ExactProperty);
    if (!ExactProperty)
    { R->SetObjectField(TEXT("native_source_after"), SourceState()); Fail(R, TEXT("graph_native_member_property_mismatch"), TEXT("Pawn.BaseEyeHeight must retain the exact observed own FFloatProperty/flags/ArrayDim1 and scalar real/float schema type; no node was dispatched.")); return R; }
    if (!Property->HasAllPropertyFlags(CPF_BlueprintVisible | CPF_NativeAccessSpecifierPublic)
        || Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected)
        || FBlueprintEditorUtils::IsPropertyPrivate(Property)
        || FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP, Property) != FBlueprintEditorUtils::EPropertyReadableState::Readable)
    { Fail(R, TEXT("graph_native_member_access_rejected"), TEXT("The exact native field is not public runtime Blueprint-readable; no getter was spawned.")); return R; }
    const auto Parent = InspectBlueprintParent(BP->ParentClass ? BP->ParentClass->GetPathName() : FString());
    if (!Parent->GetBoolField(TEXT("success")))
    { R->SetObjectField(TEXT("parent_inspection"), Parent); Fail(R, TEXT("graph_member_parent_unsupported"), TEXT("Target parent failed the existing runtime-class inspection.")); return R; }
    const auto After = SourceState();
    bool Unchanged = Before->GetBoolField(TEXT("package_dirty")) == After->GetBoolField(TEXT("package_dirty"))
        && !After->GetBoolField(TEXT("package_dirty")) && FindObject<UClass>(nullptr, TEXT("/Script/Engine.Pawn")) == Class
        && Class->FindPropertyByName(TEXT("BaseEyeHeight")) == Property
        && static_cast<uint64>(Property->GetPropertyFlags()) == 6755469234274821ULL
        && Before->GetNumberField(TEXT("array_dim")) == After->GetNumberField(TEXT("array_dim"));
    for (const TCHAR* Key : {TEXT("class_path"), TEXT("class_flags_string"), TEXT("package_name"), TEXT("property_path"),
        TEXT("property_class"), TEXT("property_flags_string"), TEXT("pin_type_text")})
    { Unchanged &= Before->GetStringField(Key) == After->GetStringField(Key); }
    R->SetObjectField(TEXT("native_source_after"), After); R->SetBoolField(TEXT("native_source_observed_unchanged"), Unchanged);
    if (!Unchanged)
    { Fail(R, TEXT("graph_native_member_source_changed"), TEXT("Already-loaded native field/package observation did not remain clean and identical; no getter was dispatched.")); return R; }
    FEdGraphPinType SelfType; SelfType.PinCategory = UEdGraphSchema_K2::PC_Object; SelfType.PinSubCategoryObject = Class;
    FString SelfText; FEdGraphPinType::StaticStruct()->ExportText(SelfText, &SelfType, &SelfType, nullptr, PPF_None, nullptr);
    R->SetStringField(TEXT("member_scope"), TEXT("external_native_pawn_eye_height"));
    R->SetStringField(TEXT("variable_name"), TEXT("BaseEyeHeight")); R->SetStringField(TEXT("variable_guid"), FGuid().ToString(EGuidFormats::Digits));
    R->SetBoolField(TEXT("setter"), false); R->SetStringField(TEXT("target_class_path"), Class->GetPathName());
    R->SetStringField(TEXT("declaring_class_path"), Class->GetPathName()); R->SetStringField(TEXT("declaring_blueprint_path"), FString());
    R->SetStringField(TEXT("property_path"), Property->GetPathName()); R->SetStringField(TEXT("compiled_property_path"), Property->GetPathName());
    R->SetStringField(TEXT("pin_category"), Type.PinCategory.ToString()); R->SetStringField(TEXT("pin_subcategory"), Type.PinSubCategory.ToString());
    R->SetStringField(TEXT("expected_pin_type_text"), TypeText); R->SetStringField(TEXT("expected_self_pin_type_text"), SelfText);
    R->SetStringField(TEXT("scope"), TEXT("exact already-loaded runtime Pawn.BaseEyeHeight getter with zero native member GUID and explicit Pawn input; no arbitrary native getter/setter, Blueprint load, CDO creation, invocation, compile or save in this read-only plan"));
    Complete(R, G); return R;
}
}

TSharedRef<FJsonObject> ReadNodeIdentities(const FString& GraphPath)
{
    auto R = Result(); UEdGraph* Graph = ResolveLoadedGraph(GraphPath, R); if (!Graph) { return R; }
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<FGuid> Seen;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!IsValid(Node) || !Node->IsA<UK2Node>()) { continue; }
        if (!Node->NodeGuid.IsValid() || Seen.Contains(Node->NodeGuid))
        { Fail(R, TEXT("graph_node_guid_invalid"), TEXT("A K2 node GUID is invalid or duplicated; no GUID was repaired.")); return R; }
        Seen.Add(Node->NodeGuid);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("object_path"), Node->GetPathName());
        Row->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
        if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
        {
            Row->SetStringField(TEXT("event_name"), Event->EventReference.GetMemberName().ToString());
            const UClass* Parent = Event->EventReference.GetMemberParentClass();
            Row->SetStringField(TEXT("event_member_parent_path"), Parent ? Parent->GetPathName() : FString());
        }
        if (Node->GetClass() == UK2Node_CustomEvent::StaticClass())
        {
            // Read the loaded node's public fields directly. Function identity
            // below uses only the serialized reference and FindObject: no
            // reference resolution, loading, or node/function repair occurs.
            const UK2Node_CustomEvent* Custom = CastChecked<UK2Node_CustomEvent>(Node);
            Row->SetStringField(TEXT("custom_function_name"), Custom->CustomFunctionName.ToString());
            Row->SetStringField(TEXT("custom_event_function_flags_string"), FString::Printf(TEXT("%u"), Custom->FunctionFlags));
            Row->SetBoolField(TEXT("call_in_editor"), Custom->bCallInEditor);
            auto Signature = MakeShared<FJsonObject>();
            Signature->SetBoolField(TEXT("valid"), false);
            Signature->SetStringField(TEXT("node_class_path"), Custom->GetClass()->GetPathName());
            for (const TCHAR* Field : {TEXT("member_name"), TEXT("member_parent_path"), TEXT("member_parent_class_path"),
                TEXT("function_path"), TEXT("function_owner_path"), TEXT("function_class_path")})
            { Signature->SetStringField(Field, FString()); }
            TArray<const UEdGraphPin*> DelegatePins;
            for (const UEdGraphPin* Pin : Custom->Pins)
            { if (Pin && Pin->PinName == FName(TEXT("OutputDelegate"))) { DelegatePins.Add(Pin); } }
            if (DelegatePins.Num() != 1)
            { Signature->SetStringField(TEXT("error_code"), TEXT("custom_event_delegate_count_invalid")); }
            else if (DelegatePins[0]->Direction != EGPD_Output || DelegatePins[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Delegate
                || DelegatePins[0]->PinType.IsContainer())
            { Signature->SetStringField(TEXT("error_code"), TEXT("custom_event_delegate_type_invalid")); }
            else
            {
                const FSimpleMemberReference& Reference = DelegatePins[0]->PinType.PinSubCategoryMemberReference;
                const UObject* Parent = Reference.MemberParent.Get();
                const FString ParentPath = Parent ? Parent->GetPathName() : FString();
                Signature->SetStringField(TEXT("member_name"), Reference.MemberName.ToString());
                Signature->SetStringField(TEXT("member_parent_path"), ParentPath);
                Signature->SetStringField(TEXT("member_parent_class_path"), Parent ? Parent->GetClass()->GetPathName() : FString());
                const UFunction* Function = Parent && !Reference.MemberName.IsNone()
                    ? FindObject<UFunction>(nullptr, *(ParentPath + TEXT(":") + Reference.MemberName.ToString())) : nullptr;
                Signature->SetStringField(TEXT("function_path"), Function ? Function->GetPathName() : FString());
                Signature->SetStringField(TEXT("function_owner_path"), Function && Function->GetOuter() ? Function->GetOuter()->GetPathName() : FString());
                Signature->SetStringField(TEXT("function_class_path"), Function ? Function->GetClass()->GetPathName() : FString());
                const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
                const UClass* ParentClass = Cast<UClass>(Parent);
                if (!Function)
                { Signature->SetStringField(TEXT("error_code"), TEXT("custom_event_function_not_loaded")); }
                else if (!Blueprint || !ParentClass || (ParentClass != Blueprint->GeneratedClass && ParentClass != Blueprint->SkeletonGeneratedClass)
                    || ParentClass->ClassGeneratedBy != Blueprint || Function->GetOuter() != Parent
                    || Function->GetFName() != Reference.MemberName || Reference.MemberName != Custom->CustomFunctionName
                    || !Reference.MemberGuid.IsValid() || Reference.MemberGuid != Custom->NodeGuid)
                { Signature->SetStringField(TEXT("error_code"), TEXT("custom_event_own_signature_invalid")); }
                else
                { Signature->SetBoolField(TEXT("valid"), true); }
            }
            Row->SetObjectField(TEXT("custom_event_signature"), Signature);
        }
        if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
        {
            // Duplicate readback proof is limited to these exact parent-event
            // calls. It observes actual current parent roles, never SKEL names
            // derived from an asset name, and does not resolve/repair references.
            const bool ParentEvent = Node->GetClass() == UK2Node_CallParentFunction::StaticClass()
                && (Call->GetFunctionName() == FName(TEXT("ReceiveBeginPlay")) || Call->GetFunctionName() == FName(TEXT("ReceiveTick")));
            UBlueprint* OwnerBP = ParentEvent ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : nullptr;
            UClass* DirectParent = IsValid(OwnerBP) ? OwnerBP->ParentClass.Get() : nullptr;
            UBlueprint* ParentBP = IsValid(DirectParent) ? Cast<UBlueprint>(DirectParent->ClassGeneratedBy) : nullptr;
            UClass* ParentGenerated = IsValid(ParentBP) ? ParentBP->GeneratedClass.Get() : nullptr;
            UClass* ParentSkeleton = IsValid(ParentBP) ? ParentBP->SkeletonGeneratedClass.Get() : nullptr;
            UClass* ReferenceParent = ParentEvent ? Call->FunctionReference.GetMemberParentClass() : nullptr;
            TArray<TStrongObjectPtr<UObject>> ProofHolds;
            if (ParentEvent)
            {
                for (UObject* Object : {static_cast<UObject*>(Node), static_cast<UObject*>(Graph), static_cast<UObject*>(OwnerBP),
                    static_cast<UObject*>(DirectParent), static_cast<UObject*>(ParentBP), static_cast<UObject*>(ParentGenerated),
                    static_cast<UObject*>(ParentSkeleton), static_cast<UObject*>(ReferenceParent)})
                { if (IsValid(Object)) { ProofHolds.Emplace(Object); } }
            }
            const FString NodePath = ParentEvent ? Node->GetPathName() : FString();
            const FGuid NodeGuid = Node->NodeGuid;
            const FMemberReference ReferenceBefore = Call->FunctionReference;
            auto ReadParentProof = [&]()
            {
                auto Proof = MakeShared<FJsonObject>(); Proof->SetBoolField(TEXT("valid"), false);
                auto Readable = [](const UObject* Object)
                {
                    int32 Depth = 0;
                    for (const UObject* Current = Object; Current; Current = Current->GetOuter())
                    { if (++Depth > 64 || !IsValid(Current) || Current->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)) { return false; } }
                    return Object != nullptr;
                };
                if (!ParentEvent || !IsEditorGameThread() || IsPlaySessionActive() || !Readable(Graph)
                    || !Graph->Nodes.Contains(Node) || !Readable(Node) || Node->GetOuter() != Graph
                    || Node->GetClass() != UK2Node_CallParentFunction::StaticClass() || Node->NodeGuid != NodeGuid || Node->GetPathName() != NodePath
                    || !Readable(OwnerBP) || FBlueprintEditorUtils::FindBlueprintForGraph(Graph) != OwnerBP
                    || OwnerBP->ParentClass.Get() != DirectParent || !Readable(DirectParent) || !Readable(ParentBP)
                    || DirectParent->ClassGeneratedBy != ParentBP || ParentBP->GeneratedClass.Get() != ParentGenerated
                    || ParentBP->SkeletonGeneratedClass.Get() != ParentSkeleton || DirectParent != ParentGenerated
                    || !Readable(ParentGenerated) || !Readable(ParentSkeleton) || ParentGenerated == ParentSkeleton
                    || ParentGenerated->ClassGeneratedBy != ParentBP || ParentSkeleton->ClassGeneratedBy != ParentBP
                    || ParentGenerated->GetOuter() != ParentBP->GetOutermost() || ParentSkeleton->GetOuter() != ParentBP->GetOutermost()
                    || ParentGenerated->GetOutermost() != ParentBP->GetOutermost() || ParentSkeleton->GetOutermost() != ParentBP->GetOutermost()
                    || Call->FunctionReference.GetMemberParentClass() != ReferenceParent || (ReferenceParent && !Readable(ReferenceParent))
                    || !Call->FunctionReference.IsSameReference(ReferenceBefore) || Call->FunctionReference.IsDeprecated() != ReferenceBefore.IsDeprecated())
                { return Proof; }
                auto Reference = MakeShared<FJsonObject>();
                Reference->SetStringField(TEXT("member_name"), ReferenceBefore.GetMemberName().ToString());
                Reference->SetStringField(TEXT("member_scope"), ReferenceBefore.GetMemberScopeName());
                Reference->SetStringField(TEXT("member_guid"), ReferenceBefore.GetMemberGuid().ToString(EGuidFormats::Digits));
                Reference->SetStringField(TEXT("member_parent_class"), ReferenceParent ? ReferenceParent->GetPathName() : FString());
                Reference->SetBoolField(TEXT("is_self_context"), ReferenceBefore.IsSelfContext());
                Reference->SetBoolField(TEXT("is_local_scope"), ReferenceBefore.IsLocalScope());
                Reference->SetBoolField(TEXT("is_deprecated"), ReferenceBefore.IsDeprecated());
                Proof->SetObjectField(TEXT("function_reference"), Reference);
                auto Identity = [](const UObject* Object)
                {
                    auto State = MakeShared<FJsonObject>();
                    State->SetStringField(TEXT("path"), Object->GetPathName());
                    State->SetStringField(TEXT("class_path"), Object->GetClass()->GetPathName());
                    State->SetStringField(TEXT("outer_path"), Object->GetOuter() ? Object->GetOuter()->GetPathName() : FString());
                    State->SetStringField(TEXT("package_path"), Object->GetOutermost()->GetPathName());
                    State->SetStringField(TEXT("flags_string"), LexToString(static_cast<uint32>(Object->GetFlags())));
                    return State;
                };
                // ConformCallsToParentFunctions may replace Actor/zero with
                // the direct parent's authoritative owner/declaration GUID.
                // Observe the actual own Event declaration and both current
                // UFunctions; names alone or an inferred GUID are insufficient.
                auto Declaration = MakeShared<FJsonObject>(); Declaration->SetBoolField(TEXT("valid"), false);
                TArray<UEdGraph*> ParentGraphs; ParentBP->GetAllGraphs(ParentGraphs);
                UK2Node_Event* DeclaredEvent = nullptr; UEdGraph* DeclarationGraph = nullptr;
                int32 Matches = 0, ParentNodes = 0; bool CompleteDeclaration = ParentGraphs.Num() <= 64;
                const FName MemberName = ReferenceBefore.GetMemberName();
                for (UEdGraph* ParentGraph : ParentGraphs)
                {
                    if (!CompleteDeclaration) { break; }
                    if (!Readable(ParentGraph) || !ParentGraph->IsIn(ParentBP) || ParentGraph->Nodes.Num() > 2048
                        || (ParentNodes += ParentGraph->Nodes.Num()) > 4096)
                    { CompleteDeclaration = false; break; }
                    for (UEdGraphNode* ParentNode : ParentGraph->Nodes)
                    {
                        if (!Readable(ParentNode) || ParentNode->GetOuter() != ParentGraph)
                        { CompleteDeclaration = false; break; }
                        if (ParentNode->GetClass() == UK2Node_Event::StaticClass())
                        {
                            auto EventNode = CastChecked<UK2Node_Event>(ParentNode);
                            if (EventNode->EventReference.GetMemberName() == MemberName)
                            { ++Matches; DeclaredEvent = EventNode; DeclarationGraph = ParentGraph; }
                        }
                    }
                }
                UFunction* NativeFunction = FindObject<UFunction>(nullptr, *(FString(TEXT("/Script/Engine.Actor:")) + MemberName.ToString()));
                UFunction* GeneratedFunction = ParentGenerated->FindFunctionByName(MemberName);
                UFunction* SkeletonFunction = ParentSkeleton->FindFunctionByName(MemberName);
                UFunction* GeneratedOwnFunction = ParentGenerated->FindFunctionByName(MemberName, EIncludeSuperFlag::ExcludeSuper);
                const bool GeneratedOverride = Readable(GeneratedFunction) && GeneratedOwnFunction == GeneratedFunction
                    && GeneratedFunction->GetOwnerClass() == ParentGenerated;
                // Full compilation excludes disabled source events while the
                // skeleton pass still declares them. Accept only the exact
                // inherited Actor pointer plus the same own disabled ghost
                // declaration and current skeleton implementation below.
                const bool DisabledInheritedNative = CompleteDeclaration && Matches == 1
                    && Readable(DeclaredEvent) && Readable(DeclarationGraph)
                    && !DeclarationGraph->HasAnyFlags(RF_Transient)
                    && DeclaredEvent->bOverrideFunction
                    && DeclaredEvent->GetDesiredEnabledState() == ENodeEnabledState::Disabled
                    && DeclaredEvent->IsAutomaticallyPlacedGhostNode()
                    && Readable(NativeFunction) && NativeFunction->GetOwnerClass() == AActor::StaticClass()
                    && GeneratedFunction == NativeFunction && GeneratedOwnFunction == nullptr;
                FGuid GeneratedGuid, SkeletonGuid;
                const bool GeneratedGuidFound = UBlueprint::GetGuidFromClassByFieldName<UFunction>(ParentGenerated, MemberName, GeneratedGuid);
                const bool SkeletonGuidFound = UBlueprint::GetGuidFromClassByFieldName<UFunction>(ParentSkeleton, MemberName, SkeletonGuid);
                // Diagnostic-only: retain the exact original short-circuit order.
                // No extra UObject getter, signature query, load or admission.
                auto DeclarationChecks = MakeShared<FJsonObject>();
                Declaration->SetObjectField(TEXT("evaluated_conditions"), DeclarationChecks);
                Declaration->SetStringField(TEXT("first_failed_condition"), FString());
                Declaration->SetBoolField(TEXT("later_conditions_are_unobserved_after_first_failure"), true);
                Declaration->SetNumberField(TEXT("own_event_match_count"), Matches);
                Declaration->SetBoolField(TEXT("generated_function_returned_nonnull"), GeneratedFunction != nullptr);
                Declaration->SetBoolField(TEXT("generated_own_function_returned_nonnull"), GeneratedOwnFunction != nullptr);
                Declaration->SetBoolField(TEXT("generated_override_role"), GeneratedOverride);
                Declaration->SetBoolField(TEXT("disabled_inherited_native_role"), DisabledInheritedNative);
                Declaration->SetBoolField(TEXT("skeleton_function_returned_nonnull"), SkeletonFunction != nullptr);
                Declaration->SetBoolField(TEXT("generated_function_equals_native_return"), NativeFunction && GeneratedFunction == NativeFunction);
                Declaration->SetBoolField(TEXT("skeleton_function_equals_native_return"), NativeFunction && SkeletonFunction == NativeFunction);
                Declaration->SetBoolField(TEXT("generated_guid_lookup_return"), GeneratedGuidFound);
                Declaration->SetBoolField(TEXT("skeleton_guid_lookup_return"), SkeletonGuidFound);
                Declaration->SetStringField(TEXT("generated_guid_lookup_value"), GeneratedGuid.ToString(EGuidFormats::Digits));
                Declaration->SetStringField(TEXT("skeleton_guid_lookup_value"), SkeletonGuid.ToString(EGuidFormats::Digits));
                auto RequireDeclaration = [&](const TCHAR* Key, bool Passed)
                {
                    DeclarationChecks->SetBoolField(Key, Passed);
                    if (!Passed) { Declaration->SetStringField(TEXT("first_failed_condition"), Key); }
                    return Passed;
                };
                if (RequireDeclaration(TEXT("parent_graph_observation_complete"), CompleteDeclaration)
                    && RequireDeclaration(TEXT("own_event_count_one"), Matches == 1)
                    && RequireDeclaration(TEXT("event_loaded"), Readable(DeclaredEvent))
                    && RequireDeclaration(TEXT("event_graph_loaded"), Readable(DeclarationGraph))
                    && RequireDeclaration(TEXT("event_graph_guid_valid"), DeclarationGraph->GraphGuid.IsValid())
                    && RequireDeclaration(TEXT("event_node_guid_valid"), DeclaredEvent->NodeGuid.IsValid())
                    && RequireDeclaration(TEXT("event_native_actor_parent"), DeclaredEvent->EventReference.GetMemberParentClass() == AActor::StaticClass())
                    && RequireDeclaration(TEXT("event_not_self_context"), !DeclaredEvent->EventReference.IsSelfContext())
                    && RequireDeclaration(TEXT("event_not_local_scope"), !DeclaredEvent->EventReference.IsLocalScope())
                    && RequireDeclaration(TEXT("event_not_deprecated"), !DeclaredEvent->EventReference.IsDeprecated())
                    && RequireDeclaration(TEXT("event_native_guid_zero"), !DeclaredEvent->EventReference.GetMemberGuid().IsValid())
                    && RequireDeclaration(TEXT("native_function_loaded"), Readable(NativeFunction))
                    && RequireDeclaration(TEXT("native_function_actor_owner"), NativeFunction->GetOwnerClass() == AActor::StaticClass())
                    && RequireDeclaration(TEXT("generated_function_loaded"), Readable(GeneratedFunction))
                    && RequireDeclaration(TEXT("generated_function_exact_supported_role"), GeneratedOverride || DisabledInheritedNative)
                    && RequireDeclaration(TEXT("skeleton_function_loaded"), Readable(SkeletonFunction))
                    && RequireDeclaration(TEXT("skeleton_function_current_parent_owner"), SkeletonFunction->GetOwnerClass() == ParentSkeleton)
                    && RequireDeclaration(TEXT("generated_function_member_name"), GeneratedFunction->GetFName() == MemberName)
                    && RequireDeclaration(TEXT("skeleton_function_member_name"), SkeletonFunction->GetFName() == MemberName)
                    && RequireDeclaration(TEXT("generated_function_exact_super_role"), GeneratedOverride ? GeneratedFunction->GetSuperFunction() == NativeFunction : DisabledInheritedNative && GeneratedFunction->GetSuperFunction() == nullptr)
                    && RequireDeclaration(TEXT("skeleton_function_native_super"), SkeletonFunction->GetSuperFunction() == NativeFunction)
                    && RequireDeclaration(TEXT("generated_native_signature_compatible"), GeneratedFunction->IsSignatureCompatibleWith(NativeFunction))
                    && RequireDeclaration(TEXT("skeleton_native_signature_compatible"), SkeletonFunction->IsSignatureCompatibleWith(NativeFunction))
                    && RequireDeclaration(TEXT("generated_lookup_found"), GeneratedGuidFound)
                    && RequireDeclaration(TEXT("skeleton_lookup_found"), SkeletonGuidFound)
                    && RequireDeclaration(TEXT("generated_lookup_equals_event_guid"), GeneratedGuid == DeclaredEvent->NodeGuid)
                    && RequireDeclaration(TEXT("skeleton_lookup_equals_generated_guid"), SkeletonGuid == GeneratedGuid))
                {
                    for (UObject* Object : {static_cast<UObject*>(DeclaredEvent), static_cast<UObject*>(DeclarationGraph),
                        static_cast<UObject*>(NativeFunction), static_cast<UObject*>(GeneratedFunction), static_cast<UObject*>(SkeletonFunction)})
                    { ProofHolds.Emplace(Object); }
                    Declaration->SetStringField(TEXT("member_name"), MemberName.ToString());
                    Declaration->SetStringField(TEXT("declaration_kind"), DisabledInheritedNative
                        ? TEXT("direct_parent_disabled_native_actor_event_declaration") : TEXT("direct_parent_native_actor_event_override"));
                    Declaration->SetStringField(TEXT("generated_function_kind"), DisabledInheritedNative
                        ? TEXT("inherited_native_actor_for_disabled_event") : TEXT("current_generated_override"));
                    Declaration->SetBoolField(TEXT("event_disabled"), DeclaredEvent->GetDesiredEnabledState() == ENodeEnabledState::Disabled);
                    Declaration->SetBoolField(TEXT("event_automatically_placed_ghost"), DeclaredEvent->IsAutomaticallyPlacedGhostNode());
                    Declaration->SetBoolField(TEXT("event_is_override"), DeclaredEvent->bOverrideFunction);
                    Declaration->SetBoolField(TEXT("declaration_graph_nontransient"), !DeclarationGraph->HasAnyFlags(RF_Transient));
                    Declaration->SetObjectField(TEXT("declaration_node"), Identity(DeclaredEvent));
                    Declaration->SetObjectField(TEXT("declaration_graph"), Identity(DeclarationGraph));
                    Declaration->SetStringField(TEXT("declaration_graph_guid"), DeclarationGraph->GraphGuid.ToString(EGuidFormats::Digits));
                    Declaration->SetStringField(TEXT("declaration_guid"), DeclaredEvent->NodeGuid.ToString(EGuidFormats::Digits));
                    Declaration->SetStringField(TEXT("generated_lookup_guid"), GeneratedGuid.ToString(EGuidFormats::Digits));
                    Declaration->SetStringField(TEXT("skeleton_lookup_guid"), SkeletonGuid.ToString(EGuidFormats::Digits));
                    auto EventReference = MakeShared<FJsonObject>(); const FMemberReference& ER = DeclaredEvent->EventReference;
                    EventReference->SetStringField(TEXT("member_name"), ER.GetMemberName().ToString());
                    EventReference->SetStringField(TEXT("member_scope"), ER.GetMemberScopeName());
                    EventReference->SetStringField(TEXT("member_guid"), ER.GetMemberGuid().ToString(EGuidFormats::Digits));
                    EventReference->SetStringField(TEXT("member_parent_class"), ER.GetMemberParentClass()->GetPathName());
                    EventReference->SetBoolField(TEXT("is_self_context"), ER.IsSelfContext());
                    EventReference->SetBoolField(TEXT("is_local_scope"), ER.IsLocalScope());
                    EventReference->SetBoolField(TEXT("is_deprecated"), ER.IsDeprecated());
                    Declaration->SetObjectField(TEXT("event_reference"), EventReference);
                    Declaration->SetObjectField(TEXT("native_function"), Identity(NativeFunction));
                    Declaration->SetObjectField(TEXT("generated_function"), Identity(GeneratedFunction));
                    Declaration->SetObjectField(TEXT("skeleton_function"), Identity(SkeletonFunction));
                    Declaration->SetStringField(TEXT("native_owner_class"), NativeFunction->GetOwnerClass()->GetPathName());
                    Declaration->SetStringField(TEXT("generated_owner_class"), GeneratedFunction->GetOwnerClass()->GetPathName());
                    Declaration->SetStringField(TEXT("skeleton_owner_class"), SkeletonFunction->GetOwnerClass()->GetPathName());
                    Declaration->SetStringField(TEXT("generated_super_function"), GeneratedFunction->GetSuperFunction() ? GeneratedFunction->GetSuperFunction()->GetPathName() : FString());
                    Declaration->SetStringField(TEXT("skeleton_super_function"), SkeletonFunction->GetSuperFunction()->GetPathName());
                    Declaration->SetBoolField(TEXT("native_signature_compatible"), true);
                    Declaration->SetBoolField(TEXT("valid"), true);
                }
                Proof->SetObjectField(TEXT("parent_function_declaration"), Declaration);
                Proof->SetObjectField(TEXT("direct_parent_blueprint"), Identity(ParentBP));
                Proof->SetObjectField(TEXT("current_generated_class"), Identity(ParentGenerated));
                Proof->SetObjectField(TEXT("current_skeleton_class"), Identity(ParentSkeleton));
                Proof->SetBoolField(TEXT("parent_package_dirty"), ParentBP->GetOutermost()->IsDirty());
                Proof->SetBoolField(TEXT("valid"), true); return Proof;
            };
            const TSharedRef<FJsonObject> ParentBefore = ReadParentProof();
            const UFunction* Function = Call->GetTargetFunction();
            if (ParentEvent)
            {
                if (!IsEditorGameThread() || IsPlaySessionActive() || !IsValid(Graph) || !Graph->Nodes.Contains(Node)
                    || !IsValid(Node) || Node->GetOuter() != Graph || Node->GetClass() != UK2Node_CallParentFunction::StaticClass()
                    || Node->NodeGuid != NodeGuid || Node->GetPathName() != NodePath || !IsValid(OwnerBP)
                    || FBlueprintEditorUtils::FindBlueprintForGraph(Graph) != OwnerBP || OwnerBP->ParentClass.Get() != DirectParent)
                { Fail(R, TEXT("graph_parent_call_identity_changed"), TEXT("Parent-call node/owner membership changed during target-function observation; no stale-node metadata was read.")); return R; }
                const TSharedRef<FJsonObject> ParentAfter = ReadParentProof();
                if (!FJsonValue::CompareEqual(FJsonValueObject(ParentBefore), FJsonValueObject(ParentAfter)))
                { Fail(R, TEXT("graph_parent_call_proof_changed"), TEXT("The exact parent-call reference/current parent roles changed during target-function observation; no repair or load was requested.")); return R; }
                Row->SetStringField(TEXT("node_class_path"), TEXT("/Script/BlueprintGraph.K2Node_CallParentFunction"));
                Row->SetObjectField(TEXT("parent_call_proof"), ParentAfter);
            }
            Row->SetStringField(TEXT("function_name"), Call->GetFunctionName().ToString());
            Row->SetStringField(TEXT("function_path"), Function ? Function->GetPathName() : FString());
            Row->SetStringField(TEXT("function_owner_path"), Function && Function->GetOwnerClass() ? Function->GetOwnerClass()->GetPathName() : FString());
        }
        if (const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node))
        {
            Row->SetStringField(TEXT("variable_name"), Variable->GetVarName().ToString());
            Row->SetBoolField(TEXT("self_context"), Variable->VariableReference.IsSelfContext());
            const UClass* MemberParent = Variable->VariableReference.GetMemberParentClass();
            Row->SetStringField(TEXT("variable_member_parent_path"), MemberParent ? MemberParent->GetPathName() : FString());
            Row->SetStringField(TEXT("variable_member_guid"), Variable->VariableReference.GetMemberGuid().ToString(EGuidFormats::Digits));
            const FProperty* Property = Variable->GetPropertyForVariableFromSkeleton();
            // Native Pawn has no generated Blueprint skeleton. Resolve only
            // the exact already-loaded field admitted by the native getter
            // plan; ordinary/generated variables keep the skeleton-only read.
            if (!Property && Node->GetClass() == UK2Node_VariableGet::StaticClass()
                && MemberParent && MemberParent == FindObject<UClass>(nullptr, TEXT("/Script/Engine.Pawn"))
                && MemberParent == APawn::StaticClass() && MemberParent->ClassGeneratedBy == nullptr
                && MemberParent->GetOutermost()->GetName() == TEXT("/Script/Engine")
                && !Variable->VariableReference.IsSelfContext() && !Variable->VariableReference.IsLocalScope()
                && !Variable->VariableReference.GetMemberGuid().IsValid()
                && Variable->VariableReference.GetMemberName() == FName(TEXT("BaseEyeHeight"))
                && Variable->GetVarName() == FName(TEXT("BaseEyeHeight")))
            {
                const FProperty* Native = MemberParent->FindPropertyByName(TEXT("BaseEyeHeight"));
                if (Native && CastField<FFloatProperty>(Native) && Native->GetOwnerClass() == MemberParent
                    && Native->GetPathName() == TEXT("/Script/Engine.Pawn:BaseEyeHeight") && Native->ArrayDim == 1
                    && static_cast<uint64>(Native->GetPropertyFlags()) == 6755469234274821ULL)
                {
                    const FProperty* Resolved = Variable->GetPropertyForVariable();
                    if (Resolved != Native || Variable->VariableReference.GetMemberParentClass() != MemberParent
                        || Variable->VariableReference.IsSelfContext() || Variable->VariableReference.IsLocalScope()
                        || Variable->VariableReference.GetMemberGuid().IsValid()
                        || Variable->GetVarName() != FName(TEXT("BaseEyeHeight")))
                    { Fail(R, TEXT("graph_native_member_resolution_changed"), TEXT("The exact native Pawn getter property/reference did not retain its preobserved identity during public resolution; no retry or repair was requested.")); return R; }
                    Property = Resolved;
                }
            }
            // KT: exact native readonly CAC field; no private value access or general native fallback.
            if(Node->GetClass()==UK2Node_VariableGet::StaticClass()&&MemberParent==UChildActorComponent::StaticClass()
                &&Variable->VariableReference.GetMemberName()==TEXT("ChildActor")&&!Variable->VariableReference.IsSelfContext())
            {
                TStrongObjectPtr<const UK2Node_Variable> KeepVariable(Variable);TStrongObjectPtr<const UClass> KeepOwner(MemberParent);
                const FGuid Identity=Node->NodeGuid;const FString IdentityPath=Node->GetPathName();
                const auto Native=CastField<FObjectProperty>(MemberParent->FindPropertyByName(TEXT("ChildActor")));
                if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()
                    ||IsGarbageCollecting()||UE::IsSavingPackage()||!Native||Native->GetOwnerClass()!=MemberParent
                    ||Native->ArrayDim!=1||Native->PropertyClass!=AActor::StaticClass()
                    ||uint64(Native->GetPropertyFlags())!=93520134028657204ULL
                    ||Native->GetMetaData(TEXT("AllowPrivateAccess"))!=TEXT("true")
                    ||Variable->VariableReference.IsLocalScope()||Variable->VariableReference.IsDeprecated()
                    ||Variable->VariableReference.GetMemberGuid().IsValid())
                {Fail(R,TEXT("graph_native_member_resolution_changed"),TEXT("Exact native CAC readonly source/reference preflight is unavailable; no private value or retry was requested."));return R;}
                const FMemberReference ReferenceBefore=Variable->VariableReference;
                const FProperty* Resolved=Variable->GetPropertyForVariable();
                if(!IsValid(Node)||Node->GetClass()!=UK2Node_VariableGet::StaticClass()||Node->GetOuter()!=Graph
                    ||!Graph->Nodes.Contains(Node)||Node->NodeGuid!=Identity||Node->GetPathName()!=IdentityPath
                    ||Resolved!=Native||!Variable->VariableReference.IsSameReference(ReferenceBefore)
                    ||Variable->VariableReference.IsSelfContext()||Variable->VariableReference.IsLocalScope()
                    ||Variable->VariableReference.IsDeprecated()!=ReferenceBefore.IsDeprecated()||Variable->VariableReference.GetMemberGuid().IsValid()
                    ||Variable->GetVarName()!=TEXT("ChildActor")||MemberParent->FindPropertyByName(TEXT("ChildActor"))!=Native
                    ||uint64(Native->GetPropertyFlags())!=93520134028657204ULL||Native->PropertyClass!=AActor::StaticClass())
                {Fail(R,TEXT("graph_native_member_resolution_changed"),TEXT("Native CAC getter identity or readonly property changed during public resolution; no retry or repair was requested."));return R;}
                Property=Resolved;
            }
            Row->SetStringField(TEXT("variable_property_path"), Property ? Property->GetPathName() : FString());
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("nodes"), Rows); R->SetStringField(TEXT("scope"), TEXT("loaded K2 nodes; public NodeGuid values, no mutation or GUID repair"));
    Complete(R, Graph); return R;
}

namespace
{
// MC: current own function-local identity, not a receipt-driven class member fallback.
struct FLocalDoubleMap
{
    TSharedRef<FJsonObject> R;UBlueprint* BP=nullptr;UEdGraph* G=nullptr;UK2Node_FunctionEntry* Entry=nullptr;
    UClass* Generated=nullptr;UClass* Skeleton=nullptr;UPackage* Package=nullptr;UPackage* Core=nullptr;
    UEdGraphSchema_K2* Schema=nullptr;UEditorEngine* Editor=nullptr;UWorld* World=nullptr;decltype(GUndo) Undo=nullptr;
    FGuid GraphGuid,EntryGuid;FName Name;FBPVariableDescription D;FMemberReference EntryReference;
    bool PresetLibrary=false,PlanDirtyBefore=false,ObservePackageDirtyTransaction=false;uint32 EntryFlags=0;EObjectFlags BPFlags=RF_NoFlags,GraphFlags=RF_NoFlags;EBlueprintStatus Status=BS_Unknown;
    TArray<TStrongObjectPtr<UObject>> Held;TArray<FBPVariableDescription> Locals,Members;
    struct FObjectRole{UObject* P;UClass* Class;UObject* Outer;FString Path;EObjectFlags Flags;};TArray<FObjectRole> Objects;
    struct FFunctionRole{UFunction* F;UClass* Owner;uint32 Flags;TArray<FProperty*> Fields;};TArray<FFunctionRole> Functions;
    struct FFieldRole{FProperty* P;FFieldClass* Class;FFieldVariant Owner;FName Name;uint64 Flags;int32 Offset,Size,Dim;};TArray<FFieldRole> Fields;
    const FMapProperty* Map=nullptr;const FStrProperty* Key=nullptr;const FDoubleProperty* Value=nullptr;UFunction* LocalFunction=nullptr;
    struct FNodeRole{UEdGraphNode* N;FGuid Guid;TArray<UEdGraphPin*> Pins;FMemberReference Reference;bool Variable=false;};TArray<FNodeRole> Nodes;
    struct FPinRole{UEdGraphPin* P;UEdGraphNode* N;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;uint32 Flags;FString Default,Auto;TObjectPtr<UObject> Object;FText Text;TArray<UEdGraphPin*> Links;};TArray<FPinRole> Pins;
    TArray<TObjectPtr<UEdGraphNode>> NodeInventory;TArray<TObjectPtr<UEdGraph>> FunctionGraphs;
    uint32 GeneratedBits=0,SkeletonBits=0;int32 GeneratedSize=0,SkeletonSize=0;TObjectPtr<UObject> GeneratedCDO,SkeletonCDO;FGuid AddedGuid;
    UK2Node_Variable* Added=nullptr;bool SetterMode=false,PrimitiveMode=false,NativeValueStructMode=false,NativeValueVector=false;const FProperty* PrimitiveField=nullptr;FMemberReference AddedReference;bool AddedBound=false,AddedInGraph=false,AddedPosition=false;int32 AddedX=0,AddedY=0;
    UScriptStruct* NativeValueSource=nullptr;FString NativeValuePath;EStructFlags NativeValueFlags=STRUCT_NoFlags;int32 NativeValueSize=0,NativeValueAlignment=0;UStruct* NativeValueSuper=nullptr;
    struct FNativeValueField{FProperty* P;FFieldClass* Class;FFieldVariant Owner;FName Name;uint64 Flags;int32 Offset,Size,Dim,Alignment;TMap<FName,FString> Metadata;};TArray<FNativeValueField> NativeValueFields;TMap<FName,FString> NativeValueMetadata;bool NativeValueMetadataCaptured=false;
    // OR finite Transform -> Quat/Vector source roles. No values or default buffers.
    struct FTransformSourceField{FProperty* Address=nullptr;FFieldClass* Kind=nullptr;FName FieldName;uint64 PropertyFlags=0;int32 Offset=0,Size=0,Dim=0,Alignment=0;UScriptStruct* Binding=nullptr;FString BindingPath;TMap<FName,FString> Metadata;};
    struct FTransformSourcePart{TStrongObjectPtr<UScriptStruct> Type;FString Path;EObjectFlags ObjectFlags=RF_NoFlags;EStructFlags StructFlags=STRUCT_NoFlags;int32 Size=0,Alignment=0;UStruct* Super=nullptr;TArray<FTransformSourceField> Fields;TMap<FName,FString> Metadata;};
    TArray<FTransformSourcePart> TransformSourceParts;bool TransformSourceReady=false,TransformSourceMetadataReady=false;
    static bool TransformSourceMapSame(const TMap<FName,FString>& A,const TMap<FName,FString>& B)
    {if(A.Num()!=B.Num())return false;for(const auto& Pair:A){const FString* V=B.Find(Pair.Key);if(!V||!V->Equals(Pair.Value,ESearchCase::CaseSensitive))return false;}return true;}
    bool TransformSourceCurrent()
    {
        if(!TransformSourceReady)return true;
        if(!Globals()||TransformSourceParts.Num()!=3)return Stop(TEXT("Current finite Transform source context/inventory differs."));
        for(const auto& Part:TransformSourceParts)
        {
            const auto* SourceType=Part.Type.Get();
            if(!Live(SourceType)||SourceType->GetClass()!=UScriptStruct::StaticClass()||SourceType->GetOuter()!=Core||SourceType->GetPackage()!=Core||!SourceType->GetPathName().Equals(Part.Path,ESearchCase::CaseSensitive)||SourceType->GetFlags()!=Part.ObjectFlags||SourceType->StructFlags!=Part.StructFlags||SourceType->GetStructureSize()!=Part.Size||SourceType->GetMinAlignment()!=Part.Alignment||SourceType->GetSuperStruct()!=Part.Super)return Stop(TEXT("Current native Transform/Quat/Vector source identity/layout differs."));
            int32 FieldIndex=0;for(TFieldIterator<FProperty> It(SourceType,EFieldIterationFlags::None);It;++It,++FieldIndex)
            {
                if(!Part.Fields.IsValidIndex(FieldIndex)||*It!=Part.Fields[FieldIndex].Address)return Stop(TEXT("Current native Transform source field address inventory differs; no retained field read."));
                const auto& Field=Part.Fields[FieldIndex];const auto* Physical=Field.Address;
                if(Physical->GetClass()!=Field.Kind||Physical->GetOwnerVariant()!=FFieldVariant(SourceType)||Physical->GetFName()!=Field.FieldName||uint64(Physical->GetPropertyFlags())!=Field.PropertyFlags||Physical->ArrayDim!=Field.Dim||Physical->GetOffset_ForInternal()!=Field.Offset||Physical->GetSize()!=Field.Size||Physical->GetMinAlignment()!=Field.Alignment||Physical->HasSetterOrGetter())return Stop(TEXT("Current Transform source direct field owner/layout/flags differ."));
                const auto* FieldMetadataMap=Physical->GetMetaDataMap();if((FieldMetadataMap?FieldMetadataMap->Num():0)!=Field.Metadata.Num()||(FieldMetadataMap&&!TransformSourceMapSame(*FieldMetadataMap,Field.Metadata)))return Stop(TEXT("Current Transform source field metadata differs."));
                if(Field.Binding){const auto* StructField=static_cast<const FStructProperty*>(Physical);if(!StructField->Struct.IsResolved()||StructField->Struct!=Field.Binding)return Stop(TEXT("Current Transform source FStructProperty binding differs."));}
            }
            if(FieldIndex!=Part.Fields.Num())return Stop(TEXT("Current Transform source field count differs."));
        }
        return true;
    }
    bool CaptureTransformSource()
    {
        const TCHAR* Paths[]={TEXT("/Script/CoreUObject.Transform"),TEXT("/Script/CoreUObject.Quat"),TEXT("/Script/CoreUObject.Vector")};
        for(int32 Index=0;Index<3;++Index)
        {
            UScriptStruct* Returned=FindObject<UScriptStruct>(nullptr,Paths[Index]);TStrongObjectPtr<UScriptStruct> HoldReturned(Returned);if(!Globals())return Stop(TEXT("Transform source lookup changed Editor context."));
            UScriptStruct* Expected=Index==0?TBaseStructure<FTransform>::Get():Index==1?TBaseStructure<FQuat>::Get():TBaseStructure<FVector>::Get();TStrongObjectPtr<UScriptStruct> HoldExpected(Expected);if(!Globals())return Stop(TEXT("Native Transform type lookup changed context."));
            if(!Live(Returned)||Returned!=Expected||Returned->GetClass()!=UScriptStruct::StaticClass()||Returned->GetOuter()!=Core||Returned->GetPackage()!=Core||!Returned->GetPathName().Equals(Paths[Index],ESearchCase::CaseSensitive)||!(Returned->StructFlags&STRUCT_Native)||Returned->IsEditorOnly()||Returned->GetStructureSize()<=0||Returned->GetMinAlignment()<=0)return Stop(TEXT("Exact loaded native Transform/Quat/Vector types required; no loader or source values."));
            FTransformSourcePart Part;Part.Type.Reset(Returned);Part.Path=Paths[Index];Part.ObjectFlags=Returned->GetFlags();Part.StructFlags=Returned->StructFlags;Part.Size=Returned->GetStructureSize();Part.Alignment=Returned->GetMinAlignment();Part.Super=Returned->GetSuperStruct();TransformSourceParts.Add(MoveTemp(Part));
        }
        for(int32 Index=0;Index<3;++Index)
        {
            auto& Part=TransformSourceParts[Index];TSet<FName> Seen;
            for(TFieldIterator<FProperty> It(Part.Type.Get(),EFieldIterationFlags::None);It;++It)
            {
                FProperty* Physical=*It;const FName FieldName=Physical->GetFName();UScriptStruct* Binding=nullptr;FString BindingPath;
                if(Physical->GetOwnerVariant()!=FFieldVariant(Part.Type.Get())||Seen.Contains(FieldName)||Physical->ArrayDim!=1||Physical->HasSetterOrGetter()||Physical->GetOffset_ForInternal()<0||Physical->GetSize()<=0||int64(Physical->GetOffset_ForInternal())+Physical->GetSize()>Part.Size||Physical->GetMinAlignment()<=0)return Stop(TEXT("Native Transform source direct field layout is unobservable."));
                if(Index==0)
                {
                    if(FieldName==TEXT("Rotation")){Binding=TransformSourceParts[1].Type.Get();BindingPath=TransformSourceParts[1].Path;}else if(FieldName==TEXT("Translation")||FieldName==TEXT("Scale3D")){Binding=TransformSourceParts[2].Type.Get();BindingPath=TransformSourceParts[2].Path;}else return Stop(TEXT("Transform must expose its entire Rotation/Translation/Scale3D reflected inventory."));
                    if(Physical->GetClass()!=FStructProperty::StaticClass())return Stop(TEXT("Transform direct fields must be exact FStructProperty."));const auto* StructField=static_cast<const FStructProperty*>(Physical);if(!StructField->Struct.IsResolved()||StructField->Struct!=Binding)return Stop(TEXT("Transform direct Quat/Vector binding is unresolved or differs."));
                }
                else if(Physical->GetClass()!=FDoubleProperty::StaticClass()||!(FieldName==TEXT("X")||FieldName==TEXT("Y")||FieldName==TEXT("Z")||(Index==1&&FieldName==TEXT("W"))))return Stop(TEXT("Native Transform nested Quat/Vector fields must match complete Double inventory."));
                FTransformSourceField Field;Field.Address=Physical;Field.Kind=Physical->GetClass();Field.FieldName=FieldName;Field.PropertyFlags=uint64(Physical->GetPropertyFlags());Field.Offset=Physical->GetOffset_ForInternal();Field.Size=Physical->GetSize();Field.Dim=Physical->ArrayDim;Field.Alignment=Physical->GetMinAlignment();Field.Binding=Binding;Field.BindingPath=BindingPath;if(const auto* FieldMetadataMap=Physical->GetMetaDataMap()){if(FieldMetadataMap->Num()>256)return Stop(TEXT("Native Transform field metadata bound exceeded."));Field.Metadata=*FieldMetadataMap;}Seen.Add(FieldName);Part.Fields.Add(MoveTemp(Field));
            }
            if(Seen.Num()!=(Index==1?4:3))return Stop(TEXT("Native Transform source property inventory is incomplete."));
        }
        TransformSourceReady=true;return TransformSourceCurrent();
    }
    bool TransformSourceMetadata(bool Capture)
    {
        for(auto& Part:TransformSourceParts)
        {
            const auto* Returned=FMetaData::GetMapForObject(Part.Type.Get());TMap<FName,FString> Copy;if(Returned){if(Returned->Num()>2048)return Stop(TEXT("Transform source metadata bound exceeded."));Copy=*Returned;}
            if(!CurrentPlain())return false;if(Capture)Part.Metadata=MoveTemp(Copy);else if(!TransformSourceMapSame(Copy,Part.Metadata))return Stop(TEXT("Native Transform/Quat/Vector source metadata changed."));
        }
        if(Capture)TransformSourceMetadataReady=true;return true;
    }
    TSharedRef<FJsonObject> TransformSourcePartJson(int32 Index)const
    {
        const auto& Part=TransformSourceParts[Index];auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("struct_path"),Part.Path);J->SetStringField(TEXT("type_path"),Part.Path);J->SetStringField(TEXT("type_flags_string"),LexToString(uint32(Part.StructFlags)));J->SetNumberField(TEXT("type_structure_size"),Part.Size);J->SetNumberField(TEXT("type_alignment"),Part.Alignment);auto Metadata=MakeShared<FJsonObject>();for(const auto& Pair:Part.Metadata)Metadata->SetStringField(Pair.Key.ToString(),Pair.Value);J->SetObjectField(TEXT("metadata"),Metadata);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& Field:Part.Fields){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),Field.FieldName.ToString());V->SetStringField(TEXT("class"),Field.Kind->GetName());V->SetStringField(TEXT("owner"),Part.Path);V->SetStringField(TEXT("path"),Field.Address->GetPathName());V->SetNumberField(TEXT("offset"),Field.Offset);V->SetNumberField(TEXT("size"),Field.Size);V->SetNumberField(TEXT("element_size"),Field.Size);V->SetNumberField(TEXT("array_dim"),Field.Dim);V->SetNumberField(TEXT("alignment"),Field.Alignment);V->SetStringField(TEXT("flags"),LexToString(Field.PropertyFlags));V->SetStringField(TEXT("struct_path"),Field.BindingPath);Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("properties"),Rows);return J;
    }
    bool NativeValueTransform=false,ScalarUdsMode=false,ScalarUdsWritePlan=false;TSharedPtr<Variable::FLocalScalarUdsSource> ScalarUds;
    bool ReferencedMode=false,ReferencedEnum=false,ReferencedClass=false;UObject* ReferencedSource=nullptr;UPackage* ReferencedPackage=nullptr;FString ReferencedPath;uint32 ReferencedPackageBits=0;bool ReferencedPackageDirty=false;
    TArray<FName> ReferencedEnumNames;TArray<int64> ReferencedEnumValues;TArray<FProperty*> ReferencedEnumFieldInventory;TArray<FFieldRole> ReferencedEnumFields;
    struct FReferencedClass{UClass* C=nullptr;UClass* Super=nullptr;UClass* Within=nullptr;TObjectPtr<UObject> CDO;uint32 Flags=0;int32 Size=0,Alignment=0;UPackage* Package=nullptr;uint32 PackageBits=0;bool Dirty=false;};TArray<FReferencedClass> ReferencedClasses;TSharedPtr<FJsonObject> ReferencedBefore;
    bool PrimitiveArrayMode=false;FProperty* PrimitiveArrayInner=nullptr;int32 PrimitiveArrayAlignment=0,PrimitiveArrayInnerAlignment=0;
    explicit FLocalDoubleMap(const TSharedRef<FJsonObject>& In,bool InSetter=false,bool InPrimitive=false,bool InNativeValue=false,bool InReferenced=false,bool InPrimitiveArray=false):R(In),SetterMode(InSetter),PrimitiveMode(InPrimitive||InNativeValue||InReferenced||InPrimitiveArray),NativeValueStructMode(InNativeValue),ReferencedMode(InReferenced),PrimitiveArrayMode(InPrimitiveArray){}
    static bool Live(const UObject* P){return IsValid(P)&&!P->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    static bool Type(const FEdGraphPinType& T){FEdGraphPinType E;E.PinCategory=UEdGraphSchema_K2::PC_String;E.ContainerType=EPinContainerType::Map;E.PinValueType.TerminalCategory=UEdGraphSchema_K2::PC_Real;E.PinValueType.TerminalSubCategory=UEdGraphSchema_K2::PC_Double;return T==E&&T.bIsUObjectWrapper==E.bIsUObjectWrapper&&T.bSerializeAsSinglePrecisionFloat==E.bSerializeAsSinglePrecisionFloat&&T.PinValueType.bTerminalIsUObjectWrapper==E.PinValueType.bTerminalIsUObjectWrapper;}
    static bool SameDescription(const FBPVariableDescription& A,const FBPVariableDescription& B){if(A.VarName!=B.VarName||A.VarGuid!=B.VarGuid||A.VarType!=B.VarType||A.VarType.bIsUObjectWrapper!=B.VarType.bIsUObjectWrapper||A.VarType.bSerializeAsSinglePrecisionFloat!=B.VarType.bSerializeAsSinglePrecisionFloat||A.VarType.PinValueType.bTerminalIsUObjectWrapper!=B.VarType.PinValueType.bTerminalIsUObjectWrapper||!A.FriendlyName.Equals(B.FriendlyName,ESearchCase::CaseSensitive)||!A.Category.IdenticalTo(B.Category)||A.PropertyFlags!=B.PropertyFlags||A.RepNotifyFunc!=B.RepNotifyFunc||A.ReplicationCondition!=B.ReplicationCondition||!A.DefaultValue.Equals(B.DefaultValue,ESearchCase::CaseSensitive)||A.MetaDataArray.Num()!=B.MetaDataArray.Num())return false;for(int32 I=0;I<A.MetaDataArray.Num();++I)if(A.MetaDataArray[I].DataKey!=B.MetaDataArray[I].DataKey||!A.MetaDataArray[I].DataValue.Equals(B.MetaDataArray[I].DataValue,ESearchCase::CaseSensitive))return false;return true;}
    static bool SameDescriptions(const TArray<FBPVariableDescription>& A,const TArray<FBPVariableDescription>& B){if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(!SameDescription(A[I],B[I]))return false;return true;}
    static uint32 Flags(const UEdGraphPin* P){return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)|(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)|(uint32(P->ShouldSavePinIfOrphaned())<<9);}
    bool Stop(const TCHAR* Why){R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("observation_incomplete"),true);return Fail(R,TEXT("graph_local_map_scope_changed"),Why);}
    bool Globals()const{return IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&GEditor==Editor&&Live(Editor)&&static_cast<UWorld*>(GWorld)==World&&Live(World)&&!World->bIsTearingDown;}
    bool Hold(UObject* O){if(!Live(O))return false;if(Objects.ContainsByPredicate([&](const FObjectRole& X){return X.P==O;}))return true;Held.Emplace(O);Objects.Add({O,O->GetClass(),O->GetOuter(),O->GetPathName(),O->GetFlags()});return true;}
    static FFieldClass* PrimitiveTypeClass(const FEdGraphPinType& T)
    {
        FEdGraphPinType E;E.PinCategory=T.PinCategory;FFieldClass* C=nullptr;
        if(T.PinCategory==UEdGraphSchema_K2::PC_Boolean)C=FBoolProperty::StaticClass();
        else if(T.PinCategory==UEdGraphSchema_K2::PC_Int)C=FIntProperty::StaticClass();
        else if(T.PinCategory==UEdGraphSchema_K2::PC_Int64)C=FInt64Property::StaticClass();
        else if(T.PinCategory==UEdGraphSchema_K2::PC_String)C=FStrProperty::StaticClass();
        else if(T.PinCategory==UEdGraphSchema_K2::PC_Name)C=FNameProperty::StaticClass();
        else if(T.PinCategory==UEdGraphSchema_K2::PC_Text)C=FTextProperty::StaticClass();
        else if(T.PinCategory==UEdGraphSchema_K2::PC_Real&&(T.PinSubCategory==UEdGraphSchema_K2::PC_Float||T.PinSubCategory==UEdGraphSchema_K2::PC_Double))
        {E.PinSubCategory=T.PinSubCategory;C=T.PinSubCategory==UEdGraphSchema_K2::PC_Float?FFloatProperty::StaticClass():FDoubleProperty::StaticClass();}
        return C&&T==E&&T.bIsUObjectWrapper==E.bIsUObjectWrapper&&T.bSerializeAsSinglePrecisionFloat==E.bSerializeAsSinglePrecisionFloat&&T.PinValueType.bTerminalIsUObjectWrapper==E.PinValueType.bTerminalIsUObjectWrapper?C:nullptr;
    }
    static bool PrimitiveTypeSame(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    static TSharedRef<FJsonObject> PrimitivePinTypeState(const FEdGraphPinType& T)
    {
        auto J=PinTypeObservation(T);J->SetBoolField(TEXT("is_weak_pointer"),T.bIsWeakPointer);J->SetBoolField(TEXT("is_uobject_wrapper"),T.bIsUObjectWrapper);J->SetBoolField(TEXT("serialize_single_precision"),T.bSerializeAsSinglePrecisionFloat);
        auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("category"),T.PinValueType.TerminalCategory.ToString());V->SetStringField(TEXT("subcategory"),T.PinValueType.TerminalSubCategory.ToString());V->SetStringField(TEXT("type_object"),T.PinValueType.TerminalSubCategoryObject.IsValid()?T.PinValueType.TerminalSubCategoryObject->GetPathName():FString());V->SetBoolField(TEXT("is_const"),T.PinValueType.bTerminalIsConst);V->SetBoolField(TEXT("is_weak_pointer"),T.PinValueType.bTerminalIsWeakPointer);V->SetBoolField(TEXT("is_uobject_wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);J->SetObjectField(TEXT("terminal"),V);return J;
    }
    static bool NativeValueType(const FEdGraphPinType& T)
    {
        if(T.PinCategory!=UEdGraphSchema_K2::PC_Struct)return false;
        UObject* O=T.PinSubCategoryObject.Get();
        if(O!=TBaseStructure<FVector>::Get()&&O!=TBaseStructure<FRotator>::Get()&&O!=TBaseStructure<FTransform>::Get())return false;
        FEdGraphPinType E;E.PinCategory=UEdGraphSchema_K2::PC_Struct;E.PinSubCategoryObject=O;return PrimitiveTypeSame(T,E);
    }
    static bool ScalarUdsType(const FEdGraphPinType& T)
    {if(T.PinCategory!=UEdGraphSchema_K2::PC_Struct||T.ContainerType!=EPinContainerType::None)return false;UObject* O=T.PinSubCategoryObject.Get();if(!Live(O)||O->GetClass()!=UUserDefinedStruct::StaticClass())return false;FEdGraphPinType E;E.PinCategory=UEdGraphSchema_K2::PC_Struct;E.PinSubCategoryObject=O;return PrimitiveTypeSame(T,E);}
    static bool TransformArrayElementType(const FEdGraphPinType& T)
    {FEdGraphPinType E;E.PinCategory=UEdGraphSchema_K2::PC_Struct;E.PinSubCategoryObject=TBaseStructure<FTransform>::Get();return T.PinCategory==UEdGraphSchema_K2::PC_Struct&&T.PinSubCategoryObject.Get()==E.PinSubCategoryObject.Get()&&PrimitiveTypeSame(T,E);}
    bool NativeValueMismatch(const TCHAR* Condition,const FString& Expected,const FString& Actual)
    {
        if(!R->HasField(TEXT("local_native_value_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R->HasField(TEXT("stage"))?R->GetStringField(TEXT("stage")):TEXT("source_preflight"));J->SetStringField(TEXT("condition"),Condition);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("actual_observed_in_guard"),true);R->SetObjectField(TEXT("local_native_value_first_mismatch"),J);}
        return Stop(TEXT("Current native Vector/Rotator source/local reference differs; first operands retained, no later SDK or retry."));
    }
    bool InitNativeValueSource()
    {
        if(ScalarUdsMode){NativeValueSource=static_cast<UScriptStruct*>(D.VarType.PinSubCategoryObject.Get());if(!NativeValueSource)return Stop(TEXT("Loaded exact UDS local source required."));NativeValuePath=NativeValueSource->GetPathName();ScalarUds=MakeShared<Variable::FLocalScalarUdsSource>();return ScalarUds->Load(NativeValuePath,[this]{return CurrentPlain();},[this](const TCHAR*,const TCHAR* Why){if(ScalarUds->OutcomeUnknown()){R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("mutation_state_unknown"),true);}return Stop(Why);});}

        FEdGraphPinType ElementType=D.VarType;if(PrimitiveArrayMode)ElementType.ContainerType=EPinContainerType::None;const FProperty* StructField=PrimitiveArrayMode?PrimitiveArrayInner:PrimitiveField;
        if(NativeValueTransform)
        {
            if(!TransformArrayElementType(ElementType)||!StructField||StructField->GetClass()!=FStructProperty::StaticClass())return Stop(TEXT("Exact Transform scalar FStructProperty or enrolled Array.Inner descriptor required."));
            const auto* SelectedStruct=static_cast<const FStructProperty*>(StructField);if(!SelectedStruct->Struct.IsResolved()||SelectedStruct->Struct!=ElementType.PinSubCategoryObject.Get())return Stop(TEXT("Current selected Transform Struct binding required."));
            if(!CaptureTransformSource())return false;NativeValueSource=TransformSourceParts[0].Type.Get();NativeValuePath=TransformSourceParts[0].Path;NativeValueFlags=NativeValueSource->StructFlags;NativeValueSize=NativeValueSource->GetStructureSize();NativeValueAlignment=NativeValueSource->GetMinAlignment();NativeValueSuper=NativeValueSource->GetSuperStruct();return true;
        }
        if(!NativeValueType(ElementType)||!StructField||StructField->GetClass()!=FStructProperty::StaticClass())return NativeValueMismatch(TEXT("local_struct_descriptor"),TEXT("exact Vector/Rotator FStructProperty or enrolled Array.Inner"),TEXT("different admitted local descriptor/class"));
        const auto* SP=static_cast<const FStructProperty*>(StructField);
        const auto Handle=SP->Struct;
        if(!Handle.IsResolved()||Handle==nullptr)return NativeValueMismatch(TEXT("local_struct_handle"),TEXT("resolved nonnull native Struct handle"),TEXT("unresolved/null"));
        UScriptStruct* S=Handle.Get();
        if(!Globals()||!Live(S)||S->GetClass()!=UScriptStruct::StaticClass()||S!=D.VarType.PinSubCategoryObject.Get()||!Hold(S))return NativeValueMismatch(TEXT("local_struct_source"),TEXT("held live exact descriptor source"),TEXT("source adoption failed"));
        NativeValueSource=S;NativeValueVector=S==TBaseStructure<FVector>::Get();NativeValuePath=NativeValueVector?TEXT("/Script/CoreUObject.Vector"):TEXT("/Script/CoreUObject.Rotator");
        const int32 Size=NativeValueVector?sizeof(FVector):sizeof(FRotator),Align=NativeValueVector?alignof(FVector):alignof(FRotator);
        if(S->GetOuter()!=Core||S->GetPackage()!=Core||!S->GetPathName().Equals(NativeValuePath,ESearchCase::CaseSensitive)||!(S->StructFlags&STRUCT_Native)||S->IsEditorOnly()||S->GetStructureSize()!=Size||S->GetMinAlignment()!=Align)
            return NativeValueMismatch(TEXT("native_struct_identity_layout"),NativeValuePath+TEXT(" size=")+LexToString(Size)+TEXT(" alignment=")+LexToString(Align),S->GetPathName()+TEXT(" size=")+LexToString(S->GetStructureSize())+TEXT(" alignment=")+LexToString(S->GetMinAlignment()));
        NativeValueFlags=S->StructFlags;NativeValueSize=S->GetStructureSize();NativeValueAlignment=S->GetMinAlignment();NativeValueSuper=S->GetSuperStruct();
        const FName NamesV[]={TEXT("X"),TEXT("Y"),TEXT("Z")},NamesR[]={TEXT("Pitch"),TEXT("Yaw"),TEXT("Roll")};
        const int32 OffsetsV[]={STRUCT_OFFSET(FVector,X),STRUCT_OFFSET(FVector,Y),STRUCT_OFFSET(FVector,Z)},OffsetsR[]={STRUCT_OFFSET(FRotator,Pitch),STRUCT_OFFSET(FRotator,Yaw),STRUCT_OFFSET(FRotator,Roll)};
        TSet<FName> Seen;
        for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::None);It;++It)
        {
            FProperty* P=*It;int32 Axis=INDEX_NONE;for(int32 I=0;I<3;++I)if(P->GetFName()==(NativeValueVector?NamesV[I]:NamesR[I]))Axis=I;
            if(NativeValueFields.Num()>=3||Axis==INDEX_NONE||Seen.Contains(P->GetFName())||P->GetOwnerVariant()!=FFieldVariant(S)||P->GetClass()!=FDoubleProperty::StaticClass()||P->ArrayDim!=1||P->GetOffset_ForInternal()!=(NativeValueVector?OffsetsV[Axis]:OffsetsR[Axis])||P->GetSize()!=sizeof(double)||P->GetMinAlignment()!=alignof(double)||P->HasSetterOrGetter())
                return NativeValueMismatch(TEXT("native_struct_field_layout"),TEXT("three directly owned X/Y/Z or Pitch/Yaw/Roll double fields with installed C++ offsets"),P->GetName()+TEXT(" class=")+P->GetClass()->GetName()+TEXT(" offset=")+LexToString(P->GetOffset_ForInternal())+TEXT(" size=")+LexToString(P->GetSize()));
            Seen.Add(P->GetFName());FNativeValueField F;F.P=P;F.Class=P->GetClass();F.Owner=P->GetOwnerVariant();F.Name=P->GetFName();F.Flags=uint64(P->GetPropertyFlags());F.Offset=P->GetOffset_ForInternal();F.Size=P->GetSize();F.Dim=P->ArrayDim;F.Alignment=P->GetMinAlignment();if(const auto* M=P->GetMetaDataMap()){if(M->Num()>256)return Stop(TEXT("Native field metadata exceeds bounded source scope."));int32 Chars=0;for(const auto& E:*M){Chars+=E.Key.ToString().Len()+E.Value.Len();if(Chars>1048576)return Stop(TEXT("Native field metadata text exceeds bounded source scope."));F.Metadata.Add(E.Key,E.Value);}}NativeValueFields.Add(MoveTemp(F));
        }
        if(NativeValueFields.Num()!=3)return NativeValueMismatch(TEXT("native_struct_field_count"),TEXT("3"),LexToString(NativeValueFields.Num()));
        // Struct metadata capture is deferred until every initial physical pin is frozen.
        return true;
    }
    bool NativeValueCurrent()
    {
        if(ScalarUdsMode){const auto* SelectedStruct=static_cast<const FStructProperty*>(PrimitiveArrayMode?PrimitiveArrayInner:PrimitiveField);if(!SelectedStruct->Struct.IsResolved()||SelectedStruct->Struct!=NativeValueSource)return Stop(TEXT("Current scalar UDS direct function property binding differs."));return !ScalarUds.IsValid()||ScalarUds->PlainCurrent();}

        if(NativeValueTransform)
        {
            if(!TransformSourceCurrent()||NativeValueSource!=TransformSourceParts[0].Type.Get())return false;
            const auto* SelectedStruct=static_cast<const FStructProperty*>(PrimitiveArrayMode?PrimitiveArrayInner:PrimitiveField);if(!SelectedStruct->Struct.IsResolved()||SelectedStruct->Struct!=NativeValueSource)return Stop(TEXT("Current selected local Transform Struct binding differs."));return true;
        }
        auto* S=NativeValueSource;
        if(!Globals()||!Live(S)||S->GetClass()!=UScriptStruct::StaticClass()||S->GetOuter()!=Core||S->GetPackage()!=Core||!S->GetPathName().Equals(NativeValuePath,ESearchCase::CaseSensitive)||S!=(NativeValueVector?TBaseStructure<FVector>::Get():TBaseStructure<FRotator>::Get())||S->StructFlags!=NativeValueFlags||S->GetStructureSize()!=NativeValueSize||S->GetMinAlignment()!=NativeValueAlignment||S->GetSuperStruct()!=NativeValueSuper)
            return NativeValueMismatch(TEXT("native_struct_source_current"),TEXT("captured native identity/flags/layout/super"),TEXT("current source differs"));
        TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::None);It;++It)Now.Add(*It);
        TArray<FProperty*> Was;for(const auto& F:NativeValueFields)Was.Add(F.P);
        if(Now!=Was)return NativeValueMismatch(TEXT("native_struct_field_inventory"),TEXT("captured complete current field pointer inventory"),TEXT("current native field routing differs"));
        for(const auto& F:NativeValueFields)
        {
            const auto* P=F.P;
            if(P->GetClass()!=F.Class||P->GetOwnerVariant()!=F.Owner||P->GetFName()!=F.Name||uint64(P->GetPropertyFlags())!=F.Flags||P->GetOffset_ForInternal()!=F.Offset||P->GetSize()!=F.Size||P->ArrayDim!=F.Dim||P->GetMinAlignment()!=F.Alignment||P->HasSetterOrGetter())
                return NativeValueMismatch(TEXT("native_struct_field_current"),F.Name.ToString()+TEXT(" offset=")+LexToString(F.Offset)+TEXT(" size=")+LexToString(F.Size)+TEXT(" flags=")+LexToString(F.Flags),P->GetFName().ToString()+TEXT(" offset=")+LexToString(P->GetOffset_ForInternal())+TEXT(" size=")+LexToString(P->GetSize())+TEXT(" flags=")+LexToString(uint64(P->GetPropertyFlags())));
            const auto* M=P->GetMetaDataMap();if((M?M->Num():0)!=F.Metadata.Num())return NativeValueMismatch(TEXT("native_field_metadata_count"),LexToString(F.Metadata.Num()),LexToString(M?M->Num():0));for(const auto& E:F.Metadata){const auto* V=M?M->Find(E.Key):nullptr;if(!V||!V->Equals(E.Value,ESearchCase::CaseSensitive))return NativeValueMismatch(TEXT("native_field_metadata_value"),E.Value,V?*V:TEXT("missing"));}
        }
        // Struct metadata is checked only by the fenced Current wrapper.
        // The own function inventory is checked before this method is called;
        // only then may the selected local FStructProperty be dereferenced.
        const auto* SP=static_cast<const FStructProperty*>(PrimitiveArrayMode?PrimitiveArrayInner:PrimitiveField);const auto Handle=SP->Struct;
        if(!Handle.IsResolved()||Handle!=S)return NativeValueMismatch(TEXT("local_struct_current_reference"),FString::Printf(TEXT("%p"),S),Handle.IsResolved()?FString::Printf(TEXT("%p"),Handle.Get()):TEXT("unresolved"));
        return true;
    }
    bool CaptureNativeValueMetadata()
    {
        if(NativeValueMetadataCaptured)return Stop(TEXT("Native metadata capture is not repeatable."));
        if(ScalarUdsMode){if(!CurrentPlain()||!ScalarUds->Query())return false;NativeValueMetadataCaptured=true;return true;}
        if(NativeValueTransform){if(!CurrentPlain()||!TransformSourceMetadata(true))return false;NativeValueMetadataCaptured=true;return true;}
        if(!CurrentPlain())return false;
        const auto* M=FMetaData::GetMapForObject(NativeValueSource);
        // UPackage::GetMetaData may ConditionalPreload deprecated metadata.
        // Close every held/source/function/physical role before using its result.
        if(!CurrentPlain())return false;
        if(M){if(M->Num()>2048)return Stop(TEXT("Native struct metadata exceeds bounded source scope."));int32 Chars=0;for(const auto& E:*M){Chars+=E.Key.ToString().Len()+E.Value.Len();if(Chars>1048576)return Stop(TEXT("Native struct metadata text exceeds bounded source scope."));NativeValueMetadata.Add(E.Key,E.Value);}}
        NativeValueMetadataCaptured=true;return true;
    }
    bool NativeValueMetadataCurrent()
    {
        if(!NativeValueMetadataCaptured)return Stop(TEXT("Native metadata baseline is not captured; no ordinary current check may bypass it."));
        if(ScalarUdsMode)return ScalarUds->Query();
        if(NativeValueTransform)return TransformSourceMetadata(false);
        const auto* M=FMetaData::GetMapForObject(NativeValueSource);
        if(!CurrentPlain())return false;
        if((M?M->Num():0)!=NativeValueMetadata.Num())return NativeValueMismatch(TEXT("native_struct_metadata_count"),LexToString(NativeValueMetadata.Num()),LexToString(M?M->Num():0));for(const auto& E:NativeValueMetadata){const auto* V=M?M->Find(E.Key):nullptr;if(!V||!V->Equals(E.Value,ESearchCase::CaseSensitive))return NativeValueMismatch(TEXT("native_struct_metadata_value"),E.Value,V?*V:TEXT("missing"));}
        return true;
    }
    TSharedRef<FJsonObject> NativeValueObservation()
    {
        if(ScalarUdsMode){auto Source=ScalarUds->Snapshot();auto Binding=MakeShared<FJsonObject>();Binding->SetStringField(TEXT("property_path"),PrimitiveField->GetPathName());Binding->SetStringField(TEXT("property_class"),TEXT("StructProperty"));Binding->SetStringField(TEXT("owner_function_path"),LocalFunction->GetPathName());Binding->SetStringField(TEXT("struct_path"),NativeValuePath);Binding->SetBoolField(TEXT("source_handle_resolved"),true);Binding->SetBoolField(TEXT("current_verified"),true);Source->SetObjectField(TEXT("property_binding"),Binding);return Source;}

        const FProperty* StructField=PrimitiveArrayMode?PrimitiveArrayInner:PrimitiveField;
        if(NativeValueTransform)
        {
            auto J=TransformSourcePartJson(0);J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("type_kind"),TEXT("transform"));J->SetStringField(TEXT("package_path"),Core->GetName());J->SetStringField(TEXT("package_flags_string"),LexToString(uint32(Core->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Core->IsDirty());J->SetStringField(TEXT("scalar_property_class"),StructField->GetClass()->GetName());J->SetStringField(TEXT("scalar_category"),D.VarType.PinCategory.ToString());J->SetStringField(TEXT("scalar_subcategory"),D.VarType.PinSubCategory.ToString());J->SetNumberField(TEXT("size"),StructField->GetSize());J->SetNumberField(TEXT("alignment"),StructField->GetMinAlignment());TArray<TSharedPtr<FJsonValue>> Nested;for(int32 Index=1;Index<3;++Index)Nested.Add(MakeShared<FJsonValueObject>(TransformSourcePartJson(Index)));J->SetArrayField(TEXT("nested_structs"),Nested);return J;
        }
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("type_kind"),NativeValueVector?TEXT("vector"):TEXT("rotator"));J->SetStringField(TEXT("struct_path"),NativeValuePath);J->SetStringField(TEXT("type_path"),NativeValuePath);J->SetStringField(TEXT("package_path"),Core->GetName());J->SetStringField(TEXT("package_flags_string"),LexToString(uint32(Core->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Core->IsDirty());J->SetStringField(TEXT("type_flags_string"),LexToString(uint32(NativeValueFlags)));J->SetNumberField(TEXT("type_structure_size"),NativeValueSize);J->SetNumberField(TEXT("type_alignment"),NativeValueAlignment);J->SetStringField(TEXT("scalar_property_class"),StructField->GetClass()->GetName());J->SetStringField(TEXT("scalar_category"),D.VarType.PinCategory.ToString());J->SetStringField(TEXT("scalar_subcategory"),D.VarType.PinSubCategory.ToString());J->SetNumberField(TEXT("size"),StructField->GetSize());J->SetNumberField(TEXT("alignment"),StructField->GetMinAlignment());
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& F:NativeValueFields){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),F.Name.ToString());V->SetStringField(TEXT("class"),F.Class->GetName());V->SetStringField(TEXT("owner"),NativeValuePath);V->SetStringField(TEXT("path"),F.P->GetPathName());V->SetNumberField(TEXT("offset"),F.Offset);V->SetNumberField(TEXT("size"),F.Size);V->SetNumberField(TEXT("array_dim"),F.Dim);V->SetNumberField(TEXT("alignment"),F.Alignment);V->SetStringField(TEXT("flags"),LexToString(F.Flags));Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("properties"),Rows);
        auto Meta=MakeShared<FJsonObject>();for(const auto& E:NativeValueMetadata)Meta->SetStringField(E.Key.ToString(),E.Value);J->SetObjectField(TEXT("metadata"),Meta);return J;
    }
    static FFieldClass* ReferencedTypeClass(const FEdGraphPinType& T)
    {
        UObject* O=T.PinSubCategoryObject.Get();FEdGraphPinType E;E.PinCategory=T.PinCategory;E.PinSubCategoryObject=O;FFieldClass* C=nullptr;
        if(T.PinCategory==UEdGraphSchema_K2::PC_Byte&&Live(O)&&O->GetClass()==UUserDefinedEnum::StaticClass())C=FByteProperty::StaticClass();
        else if((T.PinCategory==UEdGraphSchema_K2::PC_Object||T.PinCategory==UEdGraphSchema_K2::PC_Class)&&(O==UObject::StaticClass()||O==AActor::StaticClass()))C=T.PinCategory==UEdGraphSchema_K2::PC_Class?FClassProperty::StaticClass():FObjectProperty::StaticClass();
        return C&&PrimitiveTypeSame(T,E)?C:nullptr;
    }
    bool ReferencedMismatch(const TCHAR* Property,const FString& Expected,const FString& Actual)
    {
        if(!R->HasField(TEXT("local_referenced_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R->HasField(TEXT("stage"))?R->GetStringField(TEXT("stage")):TEXT("source_preflight"));J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("actual_observed_in_guard"),true);R->SetObjectField(TEXT("local_referenced_first_mismatch"),J);}
        return Stop(TEXT("Current enum/native class local/source differs; actual prefix and first copied mismatch retained, no next SDK or retry."));
    }
    const FProperty* ReferencedField()const{return PrimitiveArrayMode?PrimitiveArrayInner:PrimitiveField;}
    bool InitReferencedSource()
    {
        FEdGraphPinType Descriptor=D.VarType;if(PrimitiveArrayMode)Descriptor.ContainerType=EPinContainerType::None;
        if(!ReferencedTypeClass(Descriptor)||ReferencedField()->GetClass()!=ReferencedTypeClass(Descriptor))return ReferencedMismatch(TEXT("property_class"),TEXT("actual exact byte/object/class descriptor field"),ReferencedField()->GetClass()->GetName());
        ReferencedEnum=D.VarType.PinCategory==UEdGraphSchema_K2::PC_Byte;ReferencedClass=D.VarType.PinCategory==UEdGraphSchema_K2::PC_Class;
        ReferencedSource=D.VarType.PinSubCategoryObject.Get();if(!Globals()||!Live(ReferencedSource)||!Hold(ReferencedSource))return ReferencedMismatch(TEXT("source_lifetime"),TEXT("current held descriptor source"),TEXT("unavailable"));
        ReferencedPath=ReferencedSource->GetPathName();ReferencedPackage=ReferencedSource->GetPackage();if(!Hold(ReferencedPackage)||!Hold(ReferencedSource->GetClass()))return ReferencedMismatch(TEXT("source_package_class"),TEXT("current held source package and class"),TEXT("unavailable"));
        ReferencedPackageBits=uint32(ReferencedPackage->GetPackageFlags());ReferencedPackageDirty=ReferencedPackage->IsDirty();
        if(ReferencedEnum)
        {
            auto* E=static_cast<UEnum*>(ReferencedSource);const FString Low=ReferencedPath.ToLower();
            if(E->GetClass()!=UUserDefinedEnum::StaticClass()||E->GetOuter()!=ReferencedPackage||(!ReferencedPath.StartsWith(TEXT("/Game/"))&&!ReferencedPath.StartsWith(TEXT("/Engine/")))||!E->HasAnyFlags(RF_Public)||E->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||ReferencedPackage->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly))return ReferencedMismatch(TEXT("enum_source_admission"),TEXT("original public runtime Game/Engine UserDefinedEnum source"),ReferencedPath);
            for(const TCHAR* Token:{TEXT("cubelessmcp"),TEXT("cubelesscontrolrigtools"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal")})if(Low.Contains(Token))return ReferencedMismatch(TEXT("enum_authoring_dependency"),TEXT("source without authoring plugin dependency"),ReferencedPath);
            if(E->NumEnums()<2||E->NumEnums()>257)return ReferencedMismatch(TEXT("enum_entry_count"),TEXT("2..257 including terminal entry"),LexToString(E->NumEnums()));
            for(int32 I=0;I<E->NumEnums();++I){const auto N=E->GetNameByIndex(I);const int64 V=E->GetValueByIndex(I);if(I<E->NumEnums()-1&&(V<0||V>255))return ReferencedMismatch(TEXT("enum_byte_value"),TEXT("0..255 for authored entries"),LexToString(V));ReferencedEnumNames.Add(N);ReferencedEnumValues.Add(V);}
            auto* C=E->GetClass();for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::IncludeSuper);It;++It){auto* P=*It;ReferencedEnumFieldInventory.Add(P);}
            for(const FName StoredFieldName:{FName(TEXT("UniqueNameIndex")),FName(TEXT("EnumDescription")),FName(TEXT("DisplayNameMap"))})
            {
                FProperty* P=C->FindPropertyByName(StoredFieldName);if(!P||!ReferencedEnumFieldInventory.Contains(P)||P->GetOwnerVariant()!=FFieldVariant(C)||P->ArrayDim!=1||P->HasSetterOrGetter())return ReferencedMismatch(TEXT("enum_stored_field"),StoredFieldName.ToString()+TEXT(" directly owned actual storage"),TEXT("missing/different owner/accessor"));
                ReferencedEnumFields.Add({P,P->GetClass(),P->GetOwnerVariant(),P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim});
            }
        }
        else
        {
            UClass* C=static_cast<UClass*>(ReferencedSource);
            for(UClass* K=C;K;K=K->GetSuperClass())
            {
                if(ReferencedClasses.Num()>=2||!Live(K)||K->GetClass()!=UClass::StaticClass()||(K!=AActor::StaticClass()&&K!=UObject::StaticClass())||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy!=nullptr||!K->ClassWithin.IsResolved()||!K->ClassDefaultObject.IsResolved()||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)||!Hold(K)||!Hold(K->GetPackage()))return ReferencedMismatch(TEXT("native_class_hierarchy"),TEXT("exact resolved runtime native Actor/Object hierarchy"),TEXT("different/unavailable class role"));
                UPackage* P=K->GetPackage();if(K->GetOuter()!=P||!P->HasAnyPackageFlags(PKG_CompiledIn)||P->HasAnyPackageFlags(PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||P->IsDirty())return ReferencedMismatch(TEXT("native_class_package"),TEXT("clean compiled-in runtime package"),P->GetName());
                FReferencedClass Role;Role.C=K;Role.Super=K->GetSuperClass();Role.Within=K->ClassWithin.Get();Role.CDO=K->ClassDefaultObject;Role.Flags=uint32(K->GetClassFlags());Role.Size=K->GetStructureSize();Role.Alignment=K->GetMinAlignment();Role.Package=P;Role.PackageBits=uint32(P->GetPackageFlags());Role.Dirty=P->IsDirty();
                if(Role.Within&&!Hold(Role.Within))return ReferencedMismatch(TEXT("native_class_within"),TEXT("current held within class"),TEXT("unavailable"));
                if(Role.CDO!=nullptr){UObject* ExistingDefault=Role.CDO.Get();if(!Hold(ExistingDefault)||ExistingDefault->GetClass()!=K||ExistingDefault->GetPackage()!=P||!ExistingDefault->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return ReferencedMismatch(TEXT("native_existing_cdo"),TEXT("current existing native CDO; no creation"),TEXT("different role"));}
                ReferencedClasses.Add(Role);
            }
            if(!Hold(UClass::StaticClass()))return ReferencedMismatch(TEXT("native_class_meta"),TEXT("held native UClass"),TEXT("unavailable"));
        }
        return ReferencedCurrentPlain();
    }
    bool ReferencedCurrentPlain()
    {
        if(!Globals()||!Live(ReferencedSource)||!Live(ReferencedPackage)||ReferencedSource->GetPathName()!=ReferencedPath||ReferencedSource->GetPackage()!=ReferencedPackage||uint32(ReferencedPackage->GetPackageFlags())!=ReferencedPackageBits||ReferencedPackage->IsDirty()!=ReferencedPackageDirty)return ReferencedMismatch(TEXT("source_identity_package"),ReferencedPath+TEXT(" package_bits=")+LexToString(ReferencedPackageBits)+TEXT(" dirty=")+LexToString(ReferencedPackageDirty),TEXT("source/package lifetime or identity differs"));
        // The enclosing CurrentPlain closes all own function/field inventories
        // before this selected local property and its referenced handle are read.
        if(ReferencedEnum)
        {
            if(ReferencedField()->GetClass()!=FByteProperty::StaticClass())return ReferencedMismatch(TEXT("byte_property_class"),TEXT("ByteProperty"),ReferencedField()->GetClass()->GetName());
            const auto H=static_cast<const FByteProperty*>(ReferencedField())->Enum;if(!H.IsResolved()||H!=ReferencedSource)return ReferencedMismatch(TEXT("FByteProperty.Enum"),ReferencedPath,H.IsResolved()?FString::Printf(TEXT("%p"),H.Get()):TEXT("unresolved"));
            auto* E=static_cast<UEnum*>(ReferencedSource);if(E->GetClass()!=UUserDefinedEnum::StaticClass()||E->NumEnums()!=ReferencedEnumNames.Num())return ReferencedMismatch(TEXT("enum_entry_count_class"),LexToString(ReferencedEnumNames.Num()),LexToString(E->NumEnums()));
            for(int32 I=0;I<ReferencedEnumNames.Num();++I)if(E->GetNameByIndex(I)!=ReferencedEnumNames[I]||E->GetValueByIndex(I)!=ReferencedEnumValues[I])return ReferencedMismatch(TEXT("enum_name_value"),LexToString(I)+TEXT(":")+ReferencedEnumNames[I].ToString()+TEXT("=")+LexToString(ReferencedEnumValues[I]),LexToString(I)+TEXT(":")+E->GetNameByIndex(I).ToString()+TEXT("=")+LexToString(E->GetValueByIndex(I)));
            TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(E->GetClass(),EFieldIterationFlags::IncludeSuper);It;++It)Now.Add(*It);if(Now!=ReferencedEnumFieldInventory)return ReferencedMismatch(TEXT("enum_stored_field_inventory"),TEXT("captured current native field inventory"),TEXT("field routing changed"));
            for(const auto& F:ReferencedEnumFields)if(F.P->GetClass()!=F.Class||F.P->GetOwnerVariant()!=F.Owner||F.P->GetFName()!=F.Name||uint64(F.P->GetPropertyFlags())!=F.Flags||F.P->GetOffset_ForInternal()!=F.Offset||F.P->GetSize()!=F.Size||F.P->ArrayDim!=F.Dim||F.P->HasSetterOrGetter())return ReferencedMismatch(TEXT("enum_stored_field_layout"),F.Name.ToString()+TEXT(" size=")+LexToString(F.Size)+TEXT(" offset=")+LexToString(F.Offset)+TEXT(" flags=")+LexToString(F.Flags),F.P->GetName()+TEXT(" size=")+LexToString(F.P->GetSize())+TEXT(" offset=")+LexToString(F.P->GetOffset_ForInternal())+TEXT(" flags=")+LexToString(uint64(F.P->GetPropertyFlags())));
        }
        else
        {
            if(ReferencedField()->GetClass()!=(ReferencedClass?FClassProperty::StaticClass():FObjectProperty::StaticClass()))return ReferencedMismatch(TEXT("object_property_class"),ReferencedClass?TEXT("ClassProperty"):TEXT("ObjectProperty"),ReferencedField()->GetClass()->GetName());
            const auto H=static_cast<const FObjectProperty*>(ReferencedField())->PropertyClass;
            if(!H.IsResolved()||H!=(ReferencedClass?UClass::StaticClass():static_cast<UClass*>(ReferencedSource)))return ReferencedMismatch(TEXT("FObjectProperty.PropertyClass"),ReferencedClass?TEXT("/Script/CoreUObject.Class"):ReferencedPath,H.IsResolved()?FString::Printf(TEXT("%p"),H.Get()):TEXT("unresolved"));
            if(ReferencedClass){const auto M=static_cast<const FClassProperty*>(ReferencedField())->MetaClass;if(!M.IsResolved()||M!=ReferencedSource)return ReferencedMismatch(TEXT("FClassProperty.MetaClass"),ReferencedPath,M.IsResolved()?FString::Printf(TEXT("%p"),M.Get()):TEXT("unresolved"));}
            for(const auto& V:ReferencedClasses)
            {
                UClass* C=V.C;if(!Live(C)||!Live(V.Package))return ReferencedMismatch(TEXT("native_hierarchy_lifetime"),TEXT("current held class/package"),TEXT("class/package no longer live"));
                if(!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=nullptr)return ReferencedMismatch(TEXT("native_ClassGeneratedBy"),TEXT("resolved null"),C->ClassGeneratedBy.IsResolved()?FString::Printf(TEXT("%p"),C->ClassGeneratedBy.Get()):TEXT("unresolved"));
                if(!C->ClassWithin.IsResolved()||C->ClassWithin!=V.Within)return ReferencedMismatch(TEXT("native_ClassWithin"),FString::Printf(TEXT("%p"),V.Within),C->ClassWithin.IsResolved()?FString::Printf(TEXT("%p"),C->ClassWithin.Get()):TEXT("unresolved"));
                if(!C->ClassDefaultObject.IsResolved()||C->ClassDefaultObject!=V.CDO)return ReferencedMismatch(TEXT("native_existing_cdo"),FString::Printf(TEXT("%p"),V.CDO.Get()),C->ClassDefaultObject.IsResolved()?FString::Printf(TEXT("%p"),C->ClassDefaultObject.Get()):TEXT("unresolved"));
                if(C->GetSuperClass()!=V.Super)return ReferencedMismatch(TEXT("native_super"),FString::Printf(TEXT("%p"),V.Super),FString::Printf(TEXT("%p"),C->GetSuperClass()));
                if(uint32(C->GetClassFlags())!=V.Flags)return ReferencedMismatch(TEXT("native_class_flags"),LexToString(V.Flags),LexToString(uint32(C->GetClassFlags())));
                if(C->GetStructureSize()!=V.Size)return ReferencedMismatch(TEXT("native_class_size"),LexToString(V.Size),LexToString(C->GetStructureSize()));
                if(C->GetMinAlignment()!=V.Alignment)return ReferencedMismatch(TEXT("native_class_alignment"),LexToString(V.Alignment),LexToString(C->GetMinAlignment()));
                if(C->GetPackage()!=V.Package)return ReferencedMismatch(TEXT("native_class_package"),FString::Printf(TEXT("%p"),V.Package),FString::Printf(TEXT("%p"),C->GetPackage()));
                if(uint32(V.Package->GetPackageFlags())!=V.PackageBits)return ReferencedMismatch(TEXT("native_package_flags"),LexToString(V.PackageBits),LexToString(uint32(V.Package->GetPackageFlags())));
                if(V.Package->IsDirty()!=V.Dirty)return ReferencedMismatch(TEXT("native_package_dirty"),LexToString(V.Dirty),LexToString(V.Package->IsDirty()));
            }
        }
        return true;
    }
    bool ReferencedMetadata(UObject* Source,TSharedRef<FJsonObject>& J)
    {
        if(!CurrentPlain())return false;const auto* M=FMetaData::GetMapForObject(Source);TMap<FName,FString> ArrayMetadataCopy;if(PrimitiveArrayMode&&M){if(M->Num()>2048)return Stop(TEXT("Referenced Array source metadata exceeds2048 entries."));ArrayMetadataCopy=*M;M=&ArrayMetadataCopy;}if(!CurrentPlain())return false;
        if(M){if(M->Num()>2048)return Stop(TEXT("Referenced source metadata exceeds2048 entries."));int32 Chars=0;for(const auto& E:*M){Chars+=E.Key.ToString().Len()+E.Value.Len();if(E.Key.ToString().Len()>512||E.Value.Len()>16384||Chars>1048576)return Stop(TEXT("Referenced source metadata text exceeds existing finite bounds."));J->SetStringField(E.Key.ToString(),E.Value);}}return true;
    }
    bool ReferencedSnapshot(TSharedRef<FJsonObject>& J)
    {
        if(!CurrentPlain())return false;
        J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("source_path"),ReferencedPath);J->SetStringField(TEXT("source_kind"),ReferencedEnum?TEXT("user_defined_enum"):TEXT("native_class"));J->SetStringField(TEXT("scalar_property_class"),ReferencedField()->GetClass()->GetName());J->SetStringField(TEXT("scalar_category"),D.VarType.PinCategory.ToString());J->SetStringField(TEXT("scalar_subcategory"),D.VarType.PinSubCategory.ToString());J->SetNumberField(TEXT("size"),ReferencedField()->GetSize());J->SetNumberField(TEXT("alignment"),ReferencedField()->GetMinAlignment());J->SetStringField(TEXT("package_path"),ReferencedPackage->GetName());J->SetStringField(TEXT("package_flags_string"),LexToString(ReferencedPackageBits));J->SetBoolField(TEXT("package_dirty"),ReferencedPackageDirty);
        J->SetStringField(TEXT("source_class_path"),ReferencedSource->GetClass()->GetPathName());J->SetStringField(TEXT("source_outer_path"),GetPathNameSafe(ReferencedSource->GetOuter()));J->SetStringField(TEXT("source_flags_string"),LexToString(uint32(ReferencedSource->GetFlags())));J->SetStringField(TEXT("package_object_flags_string"),LexToString(uint32(ReferencedPackage->GetFlags())));
        auto Binding=MakeShared<FJsonObject>();Binding->SetStringField(TEXT("property_path"),ReferencedField()->GetPathName());Binding->SetStringField(TEXT("property_class"),ReferencedField()->GetClass()->GetName());Binding->SetStringField(TEXT("owner_function_path"),LocalFunction->GetPathName());if(PrimitiveArrayMode)Binding->SetStringField(TEXT("owner_property_path"),PrimitiveField->GetPathName());Binding->SetStringField(TEXT("enum_path"),ReferencedEnum?ReferencedPath:FString());Binding->SetStringField(TEXT("property_class_path"),ReferencedEnum?FString():ReferencedClass?TEXT("/Script/CoreUObject.Class"):ReferencedPath);Binding->SetStringField(TEXT("meta_class_path"),ReferencedClass?ReferencedPath:FString());Binding->SetBoolField(TEXT("source_handle_resolved"),true);Binding->SetBoolField(TEXT("current_verified"),true);J->SetObjectField(TEXT("property_binding"),Binding);
        if(ReferencedEnum)
        {
            auto* E=static_cast<UEnum*>(ReferencedSource);auto Enum=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
            for(int32 I=0;I<ReferencedEnumNames.Num();++I)
            {
                auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),I);Row->SetStringField(TEXT("name"),E->GetNameStringByIndex(I));Row->SetStringField(TEXT("full_name"),ReferencedEnumNames[I].ToString());Row->SetStringField(TEXT("value"),LexToString(ReferencedEnumValues[I]));
                const FText DisplayText=E->GetDisplayNameTextByIndex(I);if(!CurrentPlain())return false;const FString Display=DisplayText.ToString();if(!CurrentPlain())return false;
                if(Display.Len()>4096||Row->GetStringField(TEXT("name")).Len()>512)return Stop(TEXT("Referenced enum entry text exceeds existing bounds."));Row->SetStringField(TEXT("display"),Display);
                const bool Hidden=E->HasMetaData(TEXT("Hidden"),I);if(!CurrentPlain())return false;const bool Spacer=E->HasMetaData(TEXT("Spacer"),I);if(!CurrentPlain())return false;Row->SetBoolField(TEXT("hidden"),Hidden);Row->SetBoolField(TEXT("spacer"),Spacer);Rows.Add(MakeShared<FJsonValueObject>(Row));
            }
            Enum->SetArrayField(TEXT("entries"),Rows);Enum->SetNumberField(TEXT("cpp_form"),int32(E->GetCppForm()));Enum->SetNumberField(TEXT("underlying"),int32(E->GetUnderlyingType()));
            auto Stored=MakeShared<FJsonObject>();for(const auto& F:ReferencedEnumFields)
            {
                if(!CurrentPlain())return false;FString StoredExport;const bool Exported=F.P->ExportText_Direct(StoredExport,F.P->ContainerPtrToValuePtr<void>(E),F.P->ContainerPtrToValuePtr<void>(E),E,PPF_None);if(!CurrentPlain())return false;
                if(!Exported||StoredExport.Len()>262144)return Stop(TEXT("Referenced enum stored-field export is incomplete or exceeds existing bounds."));Stored->SetStringField(F.Name.ToString(),StoredExport);
            }Enum->SetObjectField(TEXT("stored_fields"),Stored);auto Meta=MakeShared<FJsonObject>();if(!ReferencedMetadata(E,Meta))return false;Enum->SetObjectField(TEXT("metadata"),Meta);
            FString File;const bool Filename=FPackageName::TryConvertLongPackageNameToFilename(ReferencedPackage->GetName(),File,FPackageName::GetAssetPackageExtension());if(!CurrentPlain())return false;if(!Filename)return Stop(TEXT("Referenced enum disk filename unavailable."));
            const int64 Size=IFileManager::Get().FileSize(*File);if(!CurrentPlain())return false;auto Disk=MakeShared<FJsonObject>();Disk->SetStringField(TEXT("path"),File);Disk->SetBoolField(TEXT("exists"),Size>=0);Disk->SetNumberField(TEXT("bytes"),Size);Disk->SetStringField(TEXT("sha1"),FString());
            if(Size>=0){if(Size>16*1024*1024)return Stop(TEXT("Referenced enum source disk exceeds existing16MiB bound."));TArray<uint8> Bytes;const bool Read=FFileHelper::LoadFileToArray(Bytes,*File);if(!CurrentPlain())return false;if(!Read||Bytes.Num()!=Size)return Stop(TEXT("Referenced enum disk read incomplete."));uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);Disk->SetStringField(TEXT("sha1"),BytesToHex(Digest,FSHA1::DigestSize));}Enum->SetObjectField(TEXT("file"),Disk);J->SetObjectField(TEXT("enum"),Enum);
        }
        else
        {
            TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& V:ReferencedClasses)
            {
                if(!CurrentPlain())return false;auto Row=MakeShared<FJsonObject>();auto* C=V.C;UObject* CDO=V.CDO.Get();Row->SetStringField(TEXT("path"),C->GetPathName());Row->SetStringField(TEXT("class"),C->GetClass()->GetPathName());Row->SetStringField(TEXT("outer"),C->GetOuter()->GetPathName());Row->SetStringField(TEXT("super"),GetPathNameSafe(V.Super));Row->SetStringField(TEXT("within"),GetPathNameSafe(V.Within));Row->SetStringField(TEXT("flags"),LexToString(uint32(C->GetFlags())));Row->SetStringField(TEXT("class_flags"),LexToString(V.Flags));Row->SetStringField(TEXT("package"),V.Package->GetName());Row->SetStringField(TEXT("package_flags"),LexToString(V.PackageBits));Row->SetBoolField(TEXT("package_dirty"),V.Dirty);Row->SetStringField(TEXT("existing_cdo"),GetPathNameSafe(CDO));Row->SetStringField(TEXT("existing_cdo_class"),CDO?CDO->GetClass()->GetPathName():FString());Row->SetStringField(TEXT("existing_cdo_outer"),CDO&&CDO->GetOuter()?CDO->GetOuter()->GetPathName():FString());Row->SetStringField(TEXT("existing_cdo_flags"),LexToString(CDO?uint32(CDO->GetFlags()):0));auto Meta=MakeShared<FJsonObject>();if(!ReferencedMetadata(C,Meta))return false;Row->SetObjectField(TEXT("metadata"),Meta);Rows.Add(MakeShared<FJsonValueObject>(Row));
            }J->SetArrayField(TEXT("native_classes"),Rows);
        }
        FString Text;TSharedRef<TJsonWriter<>> Writer=TJsonWriterFactory<>::Create(&Text);if(!FJsonSerializer::Serialize(J,Writer)||Text.Len()>1048576)return Stop(TEXT("Referenced source whole snapshot exceeds existing1Mi UTF16 bound."));return CurrentPlain();
    }
    static bool ReferencedDifference(const TSharedPtr<FJsonValue>& Expected,const TSharedPtr<FJsonValue>& Actual,const FString& Path,FString& Property,TSharedPtr<FJsonValue>& E,TSharedPtr<FJsonValue>& A)
    {
        if(FJsonValue::CompareEqual(*Expected,*Actual))return false;
        if(Expected->Type==EJson::Object&&Actual->Type==EJson::Object){const auto X=Expected->AsObject(),Y=Actual->AsObject();TArray<FString> Keys;for(const auto& Pair:X->Values)Keys.Add(FString(Pair.Key.ToView()));for(const auto& Pair:Y->Values)Keys.AddUnique(FString(Pair.Key.ToView()));Keys.Sort();for(const auto& Key:Keys){const bool XHas=X->HasField(Key),YHas=Y->HasField(Key);const auto XV=X->TryGetField(Key),YV=Y->TryGetField(Key);if(!XHas||!YHas){Property=Path+TEXT(".")+Key;E=XHas?XV:MakeShared<FJsonValueNull>();A=YHas?YV:MakeShared<FJsonValueNull>();return true;}if(ReferencedDifference(XV,YV,Path+TEXT(".")+Key,Property,E,A))return true;}}
        else if(Expected->Type==EJson::Array&&Actual->Type==EJson::Array){const auto& X=Expected->AsArray();const auto& Y=Actual->AsArray();if(X.Num()==Y.Num()){for(int32 I=0;I<X.Num();++I)if(ReferencedDifference(X[I],Y[I],Path+TEXT("[")+LexToString(I)+TEXT("]"),Property,E,A))return true;}}
        Property=Path;E=Expected;A=Actual;return true;
    }
    bool CaptureReferencedSnapshot()
    {
        if(ReferencedBefore.IsValid())return Stop(TEXT("Referenced source baseline cannot be recaptured."));auto J=MakeShared<FJsonObject>();if(!ReferencedSnapshot(J))return false;ReferencedBefore=J;return true;
    }
    bool ReferencedSnapshotCurrent()
    {
        if(!ReferencedBefore.IsValid())return Stop(TEXT("Referenced source baseline is not captured; ordinary current cannot bypass it."));auto J=MakeShared<FJsonObject>();if(!ReferencedSnapshot(J))return false;
        FString Property;TSharedPtr<FJsonValue> Expected,Actual;if(ReferencedDifference(MakeShared<FJsonValueObject>(ReferencedBefore.ToSharedRef()),MakeShared<FJsonValueObject>(J),TEXT("source"),Property,Expected,Actual))
        {
            if(!R->HasField(TEXT("local_referenced_first_mismatch"))){auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("stage"),R->HasField(TEXT("stage"))?R->GetStringField(TEXT("stage")):TEXT("source_current"));M->SetStringField(TEXT("property"),Property);M->SetField(TEXT("expected"),Expected);M->SetField(TEXT("actual"),Actual);M->SetBoolField(TEXT("actual_observed_in_guard"),true);R->SetObjectField(TEXT("local_referenced_first_mismatch"),M);}return Stop(TEXT("Read-only referenced source whole snapshot changed; no next SDK or retry."));
        }return true;
    }
    static FFieldClass* PrimitiveArrayTypeClass(const FEdGraphPinType& T)
    {
        if(!T.IsArray())return nullptr;FEdGraphPinType Element=T;Element.ContainerType=EPinContainerType::None;return NativeValueType(Element)||TransformArrayElementType(Element)||ScalarUdsType(Element)?FStructProperty::StaticClass():ReferencedTypeClass(Element)?ReferencedTypeClass(Element):PrimitiveTypeClass(Element);
    }
    bool PrimitiveArrayMismatch(const TCHAR* Property,const FString& Expected,const FString& Actual)
    {
        if(!R->HasField(TEXT("local_primitive_array_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R->HasField(TEXT("stage"))?R->GetStringField(TEXT("stage")):TEXT("array_source_preflight"));J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("actual_observed_in_guard"),true);R->SetObjectField(TEXT("local_primitive_array_first_mismatch"),J);}return Stop(TEXT("Current own primitive Array/Inner differs; first actual mismatch and partial prefix retained, no next SDK or retry."));
    }
    bool InitPrimitiveArraySource()
    {
        if(PrimitiveField->GetClass()!=FArrayProperty::StaticClass()||!PrimitiveArrayTypeClass(D.VarType))return PrimitiveArrayMismatch(TEXT("array_property_class"),TEXT("exact primitive8 or Vector/Rotator FArrayProperty descriptor"),PrimitiveField->GetClass()->GetName());
        const auto* A=static_cast<const FArrayProperty*>(PrimitiveField);PrimitiveArrayInner=A->Inner;
        if(!PrimitiveArrayInner)return PrimitiveArrayMismatch(TEXT("array_Inner"),TEXT("nonnull actual Inner"),TEXT("null"));
        auto* P=PrimitiveArrayInner;if(P->GetClass()!=PrimitiveArrayTypeClass(D.VarType)||P->GetOwnerVariant()!=FFieldVariant(A)||P->GetOwnerStruct()!=LocalFunction||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetSize()<=0||P->GetMinAlignment()<=0)return PrimitiveArrayMismatch(TEXT("array_Inner_owner_type_layout"),A->GetPathName()+TEXT(" directly owned exact scalar element; recursive function owner; positive observed layout"),P->GetFName().ToString()+TEXT(" class=")+P->GetClass()->GetName()+TEXT(" owner_address=")+FString::Printf(TEXT("%p"),P->GetOwnerVariant().GetRawPointer())+TEXT(" size=")+LexToString(P->GetSize())+TEXT(" alignment=")+LexToString(P->GetMinAlignment()));
        PrimitiveArrayAlignment=A->GetMinAlignment();PrimitiveArrayInnerAlignment=P->GetMinAlignment();Fields.Add({P,P->GetClass(),P->GetOwnerVariant(),P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim});return ReferencedMode?InitReferencedSource():!NativeValueStructMode||InitNativeValueSource();
    }
    bool PrimitiveArrayRoutingCurrent()
    {
        // The enclosing plain guard has closed all own-function direct fields;
        // validate Array.Inner routing before the retained Inner can be read.
        if(PrimitiveField->GetClass()!=FArrayProperty::StaticClass())return PrimitiveArrayMismatch(TEXT("array_class"),TEXT("ArrayProperty"),PrimitiveField->GetClass()->GetName());
        const auto* A=static_cast<const FArrayProperty*>(PrimitiveField);if(A->Inner!=PrimitiveArrayInner)return PrimitiveArrayMismatch(TEXT("array_Inner_address"),FString::Printf(TEXT("%p"),PrimitiveArrayInner),FString::Printf(TEXT("%p"),A->Inner));return true;
    }
    bool PrimitiveArrayCurrent()
    {
        const auto* A=static_cast<const FArrayProperty*>(PrimitiveField);const auto* P=PrimitiveArrayInner;
        if(A->GetOwnerVariant()!=FFieldVariant(LocalFunction)||A->HasSetterOrGetter()||A->GetMinAlignment()!=PrimitiveArrayAlignment)return PrimitiveArrayMismatch(TEXT("array_owner_accessor_alignment"),LocalFunction->GetPathName()+TEXT(" no accessor alignment=")+LexToString(PrimitiveArrayAlignment),A->GetOwnerVariant().GetPathName()+TEXT(" accessor=")+LexToString(A->HasSetterOrGetter())+TEXT(" alignment=")+LexToString(A->GetMinAlignment()));
        if(P->GetOwnerVariant()!=FFieldVariant(A)||P->GetOwnerStruct()!=LocalFunction||P->HasSetterOrGetter()||P->GetMinAlignment()!=PrimitiveArrayInnerAlignment)return PrimitiveArrayMismatch(TEXT("Inner_owner_accessor_alignment"),A->GetPathName()+TEXT(" no accessor alignment=")+LexToString(PrimitiveArrayInnerAlignment),P->GetOwnerVariant().GetPathName()+TEXT(" accessor=")+LexToString(P->HasSetterOrGetter())+TEXT(" alignment=")+LexToString(P->GetMinAlignment()));return true;
    }
    TSharedRef<FJsonObject> PrimitiveArrayObservation()
    {
        const auto* A=static_cast<const FArrayProperty*>(PrimitiveField);const auto* P=PrimitiveArrayInner;FEdGraphPinType Element=D.VarType;Element.ContainerType=EPinContainerType::None;
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("type_kind"),Element.PinCategory==UEdGraphSchema_K2::PC_Real?Element.PinSubCategory.ToString():Element.PinCategory.ToString());J->SetStringField(TEXT("package_path"),Core->GetName());J->SetStringField(TEXT("package_flags_string"),LexToString(uint32(Core->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Core->IsDirty());J->SetStringField(TEXT("scalar_property_class"),P->GetClass()->GetName());J->SetStringField(TEXT("scalar_category"),Element.PinCategory.ToString());J->SetStringField(TEXT("scalar_subcategory"),Element.PinSubCategory.ToString());J->SetNumberField(TEXT("size"),P->GetSize());J->SetNumberField(TEXT("alignment"),P->GetMinAlignment());J->SetStringField(TEXT("array_property_class"),A->GetClass()->GetName());J->SetStringField(TEXT("array_property_path"),A->GetPathName());J->SetStringField(TEXT("array_owner_function_path"),A->GetOwnerVariant().GetPathName());J->SetStringField(TEXT("array_flags_string"),LexToString(uint64(A->GetPropertyFlags())));J->SetNumberField(TEXT("array_size"),A->GetSize());J->SetNumberField(TEXT("array_alignment"),A->GetMinAlignment());J->SetObjectField(TEXT("element_type"),PrimitivePinTypeState(Element));
        auto Inner=MakeShared<FJsonObject>();Inner->SetStringField(TEXT("property_path"),P->GetPathName());Inner->SetStringField(TEXT("property_class"),P->GetClass()->GetName());Inner->SetStringField(TEXT("owner_property_path"),P->GetOwnerVariant().GetPathName());Inner->SetStringField(TEXT("owner_function_path"),P->GetOwnerStruct()->GetPathName());Inner->SetStringField(TEXT("flags_string"),LexToString(uint64(P->GetPropertyFlags())));Inner->SetNumberField(TEXT("array_dim"),P->ArrayDim);Inner->SetNumberField(TEXT("offset"),P->GetOffset_ForInternal());Inner->SetNumberField(TEXT("size"),P->GetSize());Inner->SetNumberField(TEXT("alignment"),P->GetMinAlignment());Inner->SetBoolField(TEXT("current_verified"),true);if(ScalarUdsMode){Inner->SetStringField(TEXT("struct_path"),NativeValuePath);Inner->SetBoolField(TEXT("source_handle_resolved"),true);J->SetObjectField(TEXT("scalar_uds_source"),ScalarUds->Snapshot());J->SetStringField(TEXT("type_kind"),TEXT("scalar_uds"));}else if(NativeValueStructMode){Inner->SetStringField(TEXT("struct_path"),NativeValuePath);const auto StructSource=NativeValueObservation();for(const TCHAR* SourceKey:{TEXT("struct_path"),TEXT("type_path"),TEXT("type_flags_string"),TEXT("type_structure_size"),TEXT("type_alignment"),TEXT("properties"),TEXT("metadata")})J->SetField(SourceKey,StructSource->TryGetField(SourceKey));if(NativeValueTransform)J->SetArrayField(TEXT("nested_structs"),StructSource->GetArrayField(TEXT("nested_structs")));J->SetStringField(TEXT("type_kind"),NativeValueTransform?TEXT("transform"):NativeValueVector?TEXT("vector"):TEXT("rotator"));}if(ReferencedMode){const auto Source=ReferencedBefore.ToSharedRef();J->SetObjectField(TEXT("referenced_source"),Source);J->SetStringField(TEXT("type_kind"),ReferencedEnum?TEXT("enum"):ReferencedClass?TEXT("class"):TEXT("object"));const auto Binding=Source->GetObjectField(TEXT("property_binding"));for(const TCHAR* SourceKey:{TEXT("enum_path"),TEXT("property_class_path"),TEXT("meta_class_path"),TEXT("source_handle_resolved")})Inner->SetField(SourceKey,Binding->TryGetField(SourceKey));}J->SetObjectField(TEXT("inner"),Inner);return J;
    }
    bool InitPrimitiveField()
    {
        for(const auto& F:Functions)if(F.Owner==Skeleton&&F.F->GetFName()==G->GetFName())
        {
            const FProperty* P=F.F->FindPropertyByName(Name);
            if(P&&P->GetClass()==(PrimitiveArrayMode?FArrayProperty::StaticClass():ReferencedMode?ReferencedTypeClass(D.VarType):NativeValueStructMode?FStructProperty::StaticClass():PrimitiveTypeClass(D.VarType))){PrimitiveField=P;LocalFunction=F.F;break;}
        }
        if(!PrimitiveField||!LocalFunction||PrimitiveField->GetOwnerVariant()!=FFieldVariant(LocalFunction)||PrimitiveField->ArrayDim!=1||PrimitiveField->HasAnyPropertyFlags(CPF_Parm)||PrimitiveField->HasSetterOrGetter()||PrimitiveField->GetSize()<=0||PrimitiveField->GetMinAlignment()<=0)
            return Stop(TEXT("A current own primitive local property with the exact descriptor type is required; no class-member or descriptor fallback."));
        auto* P=PrimitiveField;Fields.Add({const_cast<FProperty*>(P),P->GetClass(),P->GetOwnerVariant(),P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim});return PrimitiveArrayMode?InitPrimitiveArraySource():ReferencedMode?InitReferencedSource():!NativeValueStructMode||InitNativeValueSource();
    }
    bool PrimitiveFieldMismatch(const FFieldRole& F)
    {
        if(!R->HasField(TEXT("local_primitive_first_field_mismatch")))
        {
            // Function/field inventories were closed immediately before this
            // comparison. Copy operands only; do not dereference a changed owner.
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R->HasField(TEXT("stage"))?R->GetStringField(TEXT("stage")):TEXT("current_guard"));J->SetStringField(TEXT("property_name"),F.Name.ToString());
            auto Expected=MakeShared<FJsonObject>();Expected->SetStringField(TEXT("class_address"),FString::Printf(TEXT("%p"),F.Class));Expected->SetStringField(TEXT("owner_address"),FString::Printf(TEXT("%p"),F.Owner.GetRawPointer()));Expected->SetStringField(TEXT("name"),F.Name.ToString());Expected->SetStringField(TEXT("flags"),LexToString(F.Flags));Expected->SetNumberField(TEXT("offset"),F.Offset);Expected->SetNumberField(TEXT("size"),F.Size);Expected->SetNumberField(TEXT("array_dim"),F.Dim);
            auto Actual=MakeShared<FJsonObject>();Actual->SetStringField(TEXT("class_address"),FString::Printf(TEXT("%p"),F.P->GetClass()));Actual->SetStringField(TEXT("owner_address"),FString::Printf(TEXT("%p"),F.P->GetOwnerVariant().GetRawPointer()));Actual->SetStringField(TEXT("name"),F.P->GetFName().ToString());Actual->SetStringField(TEXT("flags"),LexToString(uint64(F.P->GetPropertyFlags())));Actual->SetNumberField(TEXT("offset"),F.P->GetOffset_ForInternal());Actual->SetNumberField(TEXT("size"),F.P->GetSize());Actual->SetNumberField(TEXT("array_dim"),F.P->ArrayDim);J->SetObjectField(TEXT("expected"),Expected);J->SetObjectField(TEXT("actual"),Actual);R->SetObjectField(TEXT("local_primitive_first_field_mismatch"),J);
        }
        return Stop(TEXT("Current primitive local field differs; first copied operands retained, no next SDK or retry."));
    }
    bool PrimitiveNode(UK2Node_Variable* N,bool InGraph=true,bool Empty=false,bool Plain=false)
    {
        if(!(Plain?CurrentPlain():Current())||!N||!Live(N)||N->GetClass()!=(SetterMode?UK2Node_VariableSet::StaticClass():UK2Node_VariableGet::StaticClass())||N->GetOuter()!=G||(InGraph&&!G->Nodes.Contains(N)))return Stop(TEXT("Current exact primitive local variable node required."));
        FMemberReference Expected;Expected.SetLocalMember(Name,G->GetName(),D.VarGuid);
        if(!N->VariableReference.IsSameReference(Expected)||!N->VariableReference.IsLocalScope()||N->VariableReference.IsDeprecated()||N->VariableReference.GetMemberGuid()!=D.VarGuid||!N->VariableReference.GetMemberScopeName().Equals(G->GetName(),ESearchCase::CaseSensitive)||N->Pins.Num()!=(SetterMode?4:1))return Stop(TEXT("Primitive local GUID/scope/reference and complete stock pin inventory required."));
        TSet<FName> Names;TSet<FGuid> Ids;
        for(auto* P:N->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||Names.Contains(P->PinName)||P->bOrphanedPin||P->bDeprecated||P->bNotConnectable||P->ParentPin||!P->SubPins.IsEmpty()||!P->DefaultObject.IsResolved()||(!ReferencedMode&&P->DefaultObject!=nullptr)||(ReferencedMode&&P->DefaultObject!=nullptr&&!(Plain?Live(P->DefaultObject.Get()):Objects.ContainsByPredicate([&](const FObjectRole& O){return O.P==P->DefaultObject;})))||P->DefaultValue.Len()>1048576||P->AutogeneratedDefaultValue.Len()>1048576||(Empty&&!P->LinkedTo.IsEmpty()))return Stop(TEXT("Whole current primitive local pin metadata/default/link roles required."));
            Names.Add(P->PinName);Ids.Add(P->PinId);
            if(SetterMode&&(P->PinName==UEdGraphSchema_K2::PN_Execute||P->PinName==UEdGraphSchema_K2::PN_Then))
            {FEdGraphPinType E;E.PinCategory=UEdGraphSchema_K2::PC_Exec;if(P->Direction!=(P->PinName==UEdGraphSchema_K2::PN_Execute?EGPD_Input:EGPD_Output)||!PrimitiveTypeSame(P->PinType,E))return Stop(TEXT("Primitive local setter execute/then type or direction differs."));}
            else if(P->PinName==Name||(SetterMode&&P->PinName==TEXT("Output_Get")))
            {const bool Input=SetterMode&&P->PinName==Name;if(P->Direction!=(Input?EGPD_Input:EGPD_Output)||!PrimitiveTypeSame(P->PinType,D.VarType))return Stop(TEXT("Primitive local value/Output_Get complete type differs from the captured declaration."));}
            else return Stop(TEXT("Unexpected primitive local pin; locals do not acquire a self pin or seeded fallback."));
        }
        // Raw/Auto/FText values and Output_Get flags are copied from stock allocation.
        // A nonempty declaration default is a separate preserved descriptor value.
        return Names.Contains(Name)&&(!SetterMode||(Names.Contains(UEdGraphSchema_K2::PN_Execute)&&Names.Contains(UEdGraphSchema_K2::PN_Then)&&Names.Contains(TEXT("Output_Get"))))?true:Stop(TEXT("Missing primitive local logical pin role."));
    }
    bool PrimitiveNodeJson(UK2Node_Variable* N,TSharedRef<FJsonObject>& Row)
    {
        if(!PrimitiveNode(N))return false;
        const FProperty* Resolved=N->GetPropertyForVariable();if(!Current()||!PrimitiveNode(N))return false;
        if(Resolved!=PrimitiveField)return Stop(TEXT("Stock local property resolution differs from the held current skeleton field."));
        Row->SetStringField(TEXT("node_id"),N->GetPathName());Row->SetStringField(TEXT("object_path"),N->GetPathName());Row->SetStringField(TEXT("node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("node_class_path"),N->GetClass()->GetPathName());Row->SetStringField(TEXT("graph_path"),G->GetPathName());Row->SetStringField(TEXT("variable_name"),D.VarName.ToString());Row->SetStringField(TEXT("variable_guid"),D.VarGuid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("local_scope_name"),G->GetName());Row->SetStringField(TEXT("property_path"),PrimitiveField->GetPathName());Row->SetObjectField(TEXT("pin_type"),PrimitivePinTypeState(D.VarType));
        Row->SetStringField(TEXT("member_name"),N->VariableReference.GetMemberName().ToString());Row->SetStringField(TEXT("member_guid"),N->VariableReference.GetMemberGuid().ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("member_local_scope"),N->VariableReference.GetMemberScopeName());Row->SetBoolField(TEXT("member_self_context"),N->VariableReference.IsSelfContext());Row->SetBoolField(TEXT("setter"),SetterMode);
        TArray<TSharedPtr<FJsonValue>> Rows;
        const auto CurrentPins=N->Pins;
        for(auto* P:CurrentPins)
        {
            if(!Current()||!PrimitiveNode(N)||N->Pins!=CurrentPins||!N->Pins.Contains(P))return false;
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),P->PinName.ToString());J->SetStringField(TEXT("direction"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetStringField(TEXT("pin_id"),P->PinId.ToString(EGuidFormats::Digits));J->SetObjectField(TEXT("pin_type"),PrimitivePinTypeState(P->PinType));J->SetNumberField(TEXT("flags"),Flags(P));J->SetStringField(TEXT("raw_default_value"),P->DefaultValue);J->SetStringField(TEXT("autogenerated_default_value"),P->AutogeneratedDefaultValue);J->SetStringField(TEXT("default_object_path"),ReferencedMode&&P->DefaultObject!=nullptr?P->DefaultObject->GetPathName():FString());
            const FText CopiedText=P->DefaultTextValue;const bool TextEmpty=CopiedText.IsEmpty();if(!Current()||!PrimitiveNode(N)||N->Pins!=CurrentPins||!N->Pins.Contains(P))return false;J->SetBoolField(TEXT("default_text_empty"),TextEmpty);
            if(P->PinName==Name)Row->SetStringField(TEXT("value_pin_id"),P->PinId.ToString(EGuidFormats::Digits));if(SetterMode&&P->PinName==TEXT("Output_Get"))Row->SetStringField(TEXT("output_pin_id"),P->PinId.ToString(EGuidFormats::Digits));
            TArray<TSharedPtr<FJsonValue>> Links;
            for(auto* L:P->LinkedTo)
            {
                if(!Current()||!Pins.ContainsByPredicate([&](const FPinRole& Q){return Q.P==L;}))return Stop(TEXT("Primitive local peer is not a current captured own graph pin."));
                auto Link=MakeShared<FJsonObject>();Link->SetStringField(TEXT("node_id"),L->GetOwningNodeUnchecked()->GetPathName());Link->SetStringField(TEXT("pin_name"),L->PinName.ToString());Link->SetStringField(TEXT("direction"),L->Direction==EGPD_Input?TEXT("input"):TEXT("output"));Links.Add(MakeShared<FJsonValueObject>(Link));
            }
            J->SetArrayField(TEXT("links"),Links);Rows.Add(MakeShared<FJsonValueObject>(J));
        }
        Row->SetArrayField(TEXT("pins"),Rows);Row->SetBoolField(TEXT("local_reference_current_verified"),true);Row->SetBoolField(TEXT("physical_pin_current_verified"),true);return Current();
    }
    TSharedRef<FJsonObject> PrimitivePlan()
    {
        if(!Current())return R;
        R->SetStringField(TEXT("target_context"),PresetLibrary?TEXT("preset_loader_library"):TEXT("normal_actor"));R->SetStringField(TEXT("function_library_profile"),PresetLibrary?TEXT("preset_loader"):TEXT(""));R->SetStringField(TEXT("target_parent_class_path"),PresetLibrary?TEXT("/Script/Engine.BlueprintFunctionLibrary"):TEXT("/Script/Engine.Actor"));R->SetStringField(TEXT("function_entry_flags_string"),LexToString(EntryFlags));
        R->SetStringField(TEXT("variable_name"),D.VarName.ToString());R->SetStringField(TEXT("variable_guid"),D.VarGuid.ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("setter"),SetterMode);R->SetStringField(TEXT("graph_path"),G->GetPathName());R->SetStringField(TEXT("graph_id"),GraphGuid.ToString(EGuidFormats::Digits));R->SetStringField(TEXT("local_scope_name"),G->GetName());R->SetStringField(TEXT("function_entry_path"),Entry->GetPathName());R->SetStringField(TEXT("function_entry_guid"),EntryGuid.ToString(EGuidFormats::Digits));R->SetStringField(TEXT("property_path"),PrimitiveField->GetPathName());R->SetStringField(TEXT("property_flags_string"),LexToString(uint64(PrimitiveField->GetPropertyFlags())));R->SetStringField(TEXT("property_owner_function_path"),LocalFunction->GetPathName());R->SetStringField(TEXT("property_declaring_role"),LocalFunction->GetOuter()==Skeleton?TEXT("skeleton"):TEXT("generated"));
        const FString Kind=ReferencedMode?(ReferencedEnum?TEXT("enum"):ReferencedClass?TEXT("class"):TEXT("object")):NativeValueStructMode?(ScalarUdsMode?TEXT("scalar_uds"):NativeValueTransform?TEXT("transform"):NativeValueVector?TEXT("vector"):TEXT("rotator")):D.VarType.PinCategory==UEdGraphSchema_K2::PC_Real?D.VarType.PinSubCategory.ToString():D.VarType.PinCategory.ToString();R->SetStringField(TEXT("value_kind"),Kind);R->SetStringField(TEXT("native_type_path"),ReferencedMode?ReferencedPath:NativeValueStructMode?NativeValuePath:FString());R->SetObjectField(TEXT("pin_type"),PrimitivePinTypeState(D.VarType));
        auto Source=MakeShared<FJsonObject>();Source->SetBoolField(TEXT("observations_complete"),true);Source->SetBoolField(TEXT("native_identity_verified"),PrimitiveField->GetClass()==PrimitiveTypeClass(D.VarType));Source->SetStringField(TEXT("type_kind"),Kind);Source->SetStringField(TEXT("package_path"),Core->GetName());Source->SetStringField(TEXT("package_flags_string"),LexToString(uint32(Core->GetPackageFlags())));Source->SetBoolField(TEXT("package_dirty"),Core->IsDirty());Source->SetStringField(TEXT("scalar_property_class"),PrimitiveField->GetClass()->GetName());Source->SetStringField(TEXT("scalar_category"),D.VarType.PinCategory.ToString());Source->SetStringField(TEXT("scalar_subcategory"),D.VarType.PinSubCategory.ToString());Source->SetNumberField(TEXT("size"),PrimitiveField->GetSize());Source->SetNumberField(TEXT("alignment"),PrimitiveField->GetMinAlignment());if(PrimitiveArrayMode)Source=PrimitiveArrayObservation();else if(ReferencedMode)Source=ReferencedBefore.ToSharedRef();else if(NativeValueStructMode)Source=NativeValueObservation();R->SetObjectField(TEXT("native_value_source_before"),Source);R->SetObjectField(TEXT("native_value_source_after"),Source);R->SetBoolField(TEXT("native_value_source_observed_unchanged"),true);
        FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&D.VarType,&D.VarType,nullptr,PPF_None,nullptr);if(!Current())return R;R->SetStringField(TEXT("expected_pin_type_text"),TypeText);R->SetBoolField(TEXT("local_descriptor_current_verified"),true);R->SetBoolField(TEXT("local_property_current_verified"),true);R->SetBoolField(TEXT("runtime_value_read_attempted"),false);R->SetBoolField(TEXT("runtime_evaluation_verified"),false);R->SetBoolField(TEXT("default_write_attempted"),false);R->SetStringField(TEXT("stored_default_before"),D.DefaultValue);R->SetStringField(TEXT("stored_default_after"),D.DefaultValue);R->SetBoolField(TEXT("stored_default_observed_unchanged"),true);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& N:Nodes)if(N.N->GetClass()==(SetterMode?UK2Node_VariableSet::StaticClass():UK2Node_VariableGet::StaticClass()))
        {
            auto* V=static_cast<UK2Node_Variable*>(N.N);if(V->VariableReference.GetMemberName()!=Name||!V->VariableReference.IsLocalScope())continue;
            auto Row=MakeShared<FJsonObject>();if(!PrimitiveNodeJson(V,Row))return R;Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        R->SetArrayField(PrimitiveArrayMode?TEXT("native_local_primitive_array_nodes"):ReferencedMode?TEXT("native_local_referenced_nodes"):NativeValueStructMode?TEXT("native_local_value_struct_nodes"):TEXT("native_local_primitive_nodes"),Rows);if(!Current()||(ScalarUdsMode&&!ScalarUdsWritePlan&&!ScalarUds->FinalFile()))return R;const bool DirtyAfter=Package->IsDirty();R->SetBoolField(TEXT("dirty_after"),DirtyAfter);if(DirtyAfter!=PlanDirtyBefore){Stop(TEXT("Primitive local plan changed target dirty state."));return R;}if(Current()){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));}return R;
    }
    bool TargetContext()const
    {
        if(!BP->ParentClass.GetGCPtr().IsResolved())return false;
        if(!PresetLibrary)return BP->BlueprintType==BPTYPE_Normal&&BP->ParentClass.GetGCPtr()==AActor::StaticClass();
        return BP->BlueprintType==BPTYPE_FunctionLibrary&&BP->ParentClass.GetGCPtr()==UBlueprintFunctionLibrary::StaticClass()&&G->GetFName()==TEXT("PresetLoader")&&BP->SimpleConstructionScript.IsResolved()&&BP->SimpleConstructionScript==nullptr&&BP->NewVariables.IsEmpty()&&BP->GeneratedVariables.IsEmpty()&&BP->ImplementedInterfaces.IsEmpty()&&BP->CachedUDSDependencies.IsEmpty();
    }
    bool EntryWorld()const
    {
        if(!PresetLibrary)return (Entry->GetExtraFlags()&FUNC_Static)==0;
        if(!(Entry->GetExtraFlags()&FUNC_Static))return false;int32 Count=0;for(UEdGraphPin* P:Entry->Pins)if(P&&P->PinName==TEXT("__WorldContext")){FEdGraphPinType Expected;Expected.PinCategory=UEdGraphSchema_K2::PC_Object;Expected.PinSubCategoryObject=UObject::StaticClass();if(P->WasTrashed()||P->GetOwningNodeUnchecked()!=Entry||!P->PinId.IsValid()||P->Direction!=EGPD_Output||P->PinType!=Expected||P->PinType.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat||!P->bHidden||P->bOrphanedPin||P->bDeprecated||P->ParentPin||!P->SubPins.IsEmpty()||!P->DefaultObject.IsResolved()||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return false;++Count;}return Count==1;
    }
    bool Basic()const
    {
        if(!Globals()||GUndo!=Undo||!Live(BP)||!Live(G)||!Live(Entry)||!TargetContext()||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified||BP->GetFlags()!=BPFlags||G->GetFlags()!=GraphFlags||BP->Status.GetValue()!=Status||BP->GetPackage()!=Package||G->GetOuter()!=BP||G->GraphGuid!=GraphGuid||BP->FunctionGraphs!=FunctionGraphs||!BP->FunctionGraphs.Contains(G)||!G->bEditable)return false;
        if(!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=(PresetLibrary?UBlueprintFunctionLibrary::StaticClass():AActor::StaticClass())||!BP->GeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr()!=Generated||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->SkeletonGeneratedClass.GetGCPtr()!=Skeleton||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=UEdGraphSchema_K2::StaticClass())return false;
        if(Generated->GetClassFlags()!=GeneratedBits||Skeleton->GetClassFlags()!=SkeletonBits||Generated->GetStructureSize()!=GeneratedSize||Skeleton->GetStructureSize()!=SkeletonSize||!Generated->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject!=GeneratedCDO||!Skeleton->ClassDefaultObject.IsResolved()||Skeleton->ClassDefaultObject!=SkeletonCDO)return false;
        for(UClass* C:{Generated,Skeleton})if(!Live(C)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=BP||C->GetPackage()!=Package)return false;
        if(!UEdGraphSchema_K2::StaticClass()->ClassDefaultObject.IsResolved()||UEdGraphSchema_K2::StaticClass()->ClassDefaultObject!=Schema)return false;
        return true;
    }
    bool ObjectMismatch(const FObjectRole& O,const TCHAR* Property,const FString& Expected,const FString& Actual)
    {
        if(!R->HasField(TEXT("local_map_first_object_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),ObservePackageDirtyTransaction?TEXT("post_graph_Modify"):TEXT("current_guard"));
            J->SetStringField(TEXT("object_path"),O.Path);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);
            J->SetBoolField(TEXT("actual_observed_in_guard"),true);J->SetBoolField(TEXT("diagnostic_followup_SDK_calls"),false);R->SetObjectField(TEXT("local_map_first_object_mismatch"),J);
        }
        return Stop(TEXT("Captured object role changed; first copied property/expected/actual retained."));
    }
    bool CurrentPlain()
    {
        if(!Basic())return Stop(TEXT("Current normal Actor/function/class/schema/editor roles changed."));
        for(auto& O:Objects)
        {
            const bool CurrentLive=Live(O.P);if(!CurrentLive)return ObjectMismatch(O,TEXT("loaded_alive"),TEXT("true"),TEXT("false"));
            auto* CurrentClass=O.P->GetClass();if(CurrentClass!=O.Class)return ObjectMismatch(O,TEXT("class_address"),FString::Printf(TEXT("%p"),O.Class),FString::Printf(TEXT("%p"),CurrentClass));
            auto* CurrentOuter=O.P->GetOuter();if(CurrentOuter!=O.Outer)return ObjectMismatch(O,TEXT("outer_address"),FString::Printf(TEXT("%p"),O.Outer),FString::Printf(TEXT("%p"),CurrentOuter));
            const FString CurrentPath=O.P->GetPathName();if(!CurrentPath.Equals(O.Path,ESearchCase::CaseSensitive))return ObjectMismatch(O,TEXT("path"),O.Path,CurrentPath);
            const EObjectFlags CurrentFlags=O.P->GetFlags();
            if(CurrentFlags!=O.Flags)
            {
                // UE5.8 UPackage::SetDirtyFlag adds exactly RF_Transactional
                // to the target package under the captured non-null GUndo.
                const bool NormalPackageTransition=ObservePackageDirtyTransaction&&O.P==Package&&Undo!=nullptr&&GUndo==Undo&&!PlanDirtyBefore
                    &&(O.Flags&RF_Transactional)==0&&CurrentFlags==(O.Flags|RF_Transactional)
                    &&Package!=GetTransientPackage()&&!Package->HasAnyPackageFlags(PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn)&&Package->IsDirty();
                if(!NormalPackageTransition)return ObjectMismatch(O,TEXT("ObjectFlags"),LexToString(uint32(O.Flags)),LexToString(uint32(CurrentFlags)));
                auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),TEXT("post_graph_Modify"));J->SetStringField(TEXT("object_path"),O.Path);J->SetStringField(TEXT("property"),TEXT("ObjectFlags"));
                J->SetStringField(TEXT("expected_before"),LexToString(uint32(O.Flags)));J->SetStringField(TEXT("actual"),LexToString(uint32(CurrentFlags)));J->SetStringField(TEXT("allowed_delta"),TEXT("RF_Transactional only"));
                J->SetBoolField(TEXT("actual_observed_in_guard"),true);J->SetBoolField(TEXT("captured_GUndo_nonnull_unchanged"),true);J->SetBoolField(TEXT("dirty_false_to_true"),true);
                R->SetObjectField(TEXT("local_map_expected_package_transaction"),J);O.Flags=CurrentFlags;
            }
        }
        if(Core->IsDirty()||uint32(Core->GetPackageFlags())!=16||!Core->GetName().Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive))return Stop(TEXT("Native double source package changed."));
        if(Entry->NodeGuid!=EntryGuid||!Entry->FunctionReference.IsSameReference(EntryReference)||Entry->FunctionReference.IsDeprecated()!=EntryReference.IsDeprecated()||Entry->GetExtraFlags()!=EntryFlags||!EntryWorld()||!SameDescriptions(Entry->LocalVariables,Locals)||!SameDescriptions(BP->NewVariables,Members))return Stop(TEXT("Entry/local/member descriptor scope changed."));
        // Compare all own function/field inventories before dereferencing any stored field.
        for(UClass* C:{Skeleton,Generated}){TArray<UFunction*> Now;for(TFieldIterator<UFunction> I(C,EFieldIterationFlags::None);I;++I)Now.Add(*I);TArray<UFunction*> Was;for(const auto& F:Functions)if(F.Owner==C)Was.Add(F.F);if(Now!=Was)return Stop(TEXT("Current class own-function binding changed."));}
        for(const auto& F:Functions){TArray<FProperty*> Now;for(TFieldIterator<FProperty> I(F.F,EFieldIterationFlags::None);I;++I)Now.Add(*I);if(F.F->GetOuter()!=F.Owner||uint32(F.F->FunctionFlags)!=F.Flags||Now!=F.Fields)return Stop(TEXT("Current function/local property inventory changed."));}
        if(!PrimitiveMode&&(Map->KeyProp!=Key||Map->ValueProp!=Value))return Stop(TEXT("Map key/value identities changed."));
        if(PrimitiveArrayMode&&!PrimitiveArrayRoutingCurrent())return false;
        for(const auto& F:Fields)if(F.P->GetClass()!=F.Class||F.P->GetOwnerVariant()!=F.Owner||F.P->GetFName()!=F.Name||uint64(F.P->GetPropertyFlags())!=F.Flags||F.P->GetOffset_ForInternal()!=F.Offset||F.P->GetSize()!=F.Size||F.P->ArrayDim!=F.Dim)return PrimitiveMode?PrimitiveFieldMismatch(F):Stop(TEXT("Selected Map property layout changed."));
        if(PrimitiveArrayMode&&!PrimitiveArrayCurrent())return false;
        if(NativeValueStructMode&&!NativeValueCurrent())return false;
        if(ReferencedMode&&!ReferencedCurrentPlain())return false;
        if(G->Nodes.Num()!=NodeInventory.Num()+(AddedInGraph?1:0))return Stop(TEXT("Current graph node inventory changed."));for(int32 I=0;I<NodeInventory.Num();++I)if(!G->Nodes[I].IsResolved()||G->Nodes[I]!=NodeInventory[I])return Stop(TEXT("Prior graph node identity changed."));
        if(AddedInGraph&&(!G->Nodes.Last().IsResolved()||G->Nodes.Last()!=Added))return Stop(TEXT("Returned getter is no longer the appended current graph node."));
        for(const auto& N:Nodes)if(!Live(N.N)||N.N->GetOuter()!=G||N.N->NodeGuid!=N.Guid||N.N->Pins!=N.Pins||(N.Variable&&(!static_cast<UK2Node_Variable*>(N.N)->VariableReference.IsSameReference(N.Reference)||static_cast<UK2Node_Variable*>(N.N)->VariableReference.IsDeprecated()!=N.Reference.IsDeprecated()||!static_cast<UK2Node_Variable*>(N.N)->VariableReference.GetMemberScopeName().Equals(N.Reference.GetMemberScopeName(),ESearchCase::CaseSensitive))))return Stop(TEXT("Prior physical node/pin/reference inventory changed."));
        for(const auto& P:Pins){auto* Q=P.P;if(Q->WasTrashed()||Q->GetOwningNodeUnchecked()!=P.N||Q->PinId!=P.Id||Q->PinName!=P.Name||Q->Direction!=P.Direction||Q->PinType!=P.Type||Q->PinType.bIsUObjectWrapper!=P.Type.bIsUObjectWrapper||Q->PinType.bSerializeAsSinglePrecisionFloat!=P.Type.bSerializeAsSinglePrecisionFloat||Q->PinType.PinValueType.bTerminalIsUObjectWrapper!=P.Type.PinValueType.bTerminalIsUObjectWrapper||Flags(Q)!=P.Flags||!Q->DefaultValue.Equals(P.Default,ESearchCase::CaseSensitive)||!Q->AutogeneratedDefaultValue.Equals(P.Auto,ESearchCase::CaseSensitive)||Q->DefaultObject!=P.Object||!Q->DefaultTextValue.IdenticalTo(P.Text)||Q->LinkedTo!=P.Links)return Stop(TEXT("Prior physical pin values/links changed."));}
        if(Added&&(!Live(Added)||Added->GetClass()!=(SetterMode?UK2Node_VariableSet::StaticClass():UK2Node_VariableGet::StaticClass())||Added->GetOuter()!=G||(AddedGuid.IsValid()&&Added->NodeGuid!=AddedGuid)||(AddedPosition&&(Added->NodePosX!=AddedX||Added->NodePosY!=AddedY))||(AddedBound&&(!Added->VariableReference.IsSameReference(AddedReference)||Added->VariableReference.IsDeprecated()))))return Stop(TEXT("Returned local getter identity/reference changed."));
        return true;
    }
    bool Current()
    {
        if(!CurrentPlain())return false;
        return ReferencedMode?ReferencedSnapshotCurrent():!NativeValueStructMode||NativeValueMetadataCurrent();
    }
    bool Init(UEdGraph* Graph,const FString& Variable)
    {
        Editor=GEditor;World=static_cast<UWorld*>(GWorld);Undo=GUndo;G=Graph;Name=FName(*Variable);R->SetStringField(TEXT("member_scope"),PrimitiveArrayMode?TEXT("local_primitive_array"):ReferencedMode?TEXT("local_referenced_value"):NativeValueStructMode?TEXT("local_native_value_struct"):PrimitiveMode?TEXT("local_primitive"):TEXT("local_native_string_double_map"));
        if(!Globals()||!Live(G)||!SafeSelector(Variable))return Stop(TEXT("Idle loaded own function graph required."));
        BP=FBlueprintEditorUtils::FindBlueprintForGraph(G);if(!Hold(BP)||!Hold(G)||!Hold(Editor)||!Hold(World))return Stop(TEXT("Loaded current target roles required."));Package=BP->GetPackage();if(!Hold(Package))return Stop(TEXT("Loaded target package required for local plan observation."));PlanDirtyBefore=Package->IsDirty();bool ResolvedDirtyBefore=false;if(R->HasField(TEXT("dirty_before"))&&(!R->TryGetBoolField(TEXT("dirty_before"),ResolvedDirtyBefore)||ResolvedDirtyBefore!=PlanDirtyBefore))return Stop(TEXT("Resolved graph package dirty state changed before local observation."));R->SetBoolField(TEXT("dirty_before"),PlanDirtyBefore);PresetLibrary=BP->BlueprintType==BPTYPE_FunctionLibrary;
        if(!BP->ParentClass.GetGCPtr().IsResolved()||!TargetContext()||G->GetOuter()!=BP||!BP->FunctionGraphs.Contains(G)||!G->bEditable||!G->GraphGuid.IsValid()||BP->NewVariables.ContainsByPredicate([&](const auto& X){return X.VarName==Name;}))return Stop(TEXT("Only an unambiguous local of a directly owned native Actor function is supported."));
        const auto AdoptClass=[&](const TObjectPtr<UClass>& H,const FString& Path,UClass*& Out){if(!H.IsResolved()||H==nullptr)return false;UObject* O=StaticFindObject(nullptr,nullptr,*Path);if(!Globals()||!Live(O)||H!=O||O->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Hold(O))return false;Out=static_cast<UClass*>(O);return true;};
        const FString Asset=BP->GetPathName(),PackageName=Package->GetName();if(!AdoptClass(BP->GeneratedClass.GetGCPtr(),Asset+TEXT("_C"),Generated)||!AdoptClass(BP->SkeletonGeneratedClass.GetGCPtr(),PackageName+TEXT(".SKEL_")+BP->GetName()+TEXT("_C"),Skeleton))return Stop(TEXT("Exact current resolved own generated/skeleton classes required."));
        if(!Generated->ClassDefaultObject.IsResolved()||!Skeleton->ClassDefaultObject.IsResolved())return Stop(TEXT("Resolved existing class CDO handles required; no creation."));GeneratedCDO=Generated->ClassDefaultObject;SkeletonCDO=Skeleton->ClassDefaultObject;GeneratedBits=uint32(Generated->GetClassFlags());SkeletonBits=uint32(Skeleton->GetClassFlags());GeneratedSize=Generated->GetStructureSize();SkeletonSize=Skeleton->GetStructureSize();
        if(!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=UEdGraphSchema_K2::StaticClass())return Stop(TEXT("Exact standard K2 graph required."));
        const auto CDO=UEdGraphSchema_K2::StaticClass()->ClassDefaultObject;if(!CDO.IsResolved()||CDO==nullptr)return Stop(TEXT("Existing resolved schema CDO required."));UObject* S=StaticFindObject(nullptr,nullptr,TEXT("/Script/BlueprintGraph.Default__EdGraphSchema_K2"));if(!Globals()||CDO!=S||!Hold(S)||S->GetClass()!=UEdGraphSchema_K2::StaticClass())return Stop(TEXT("Exact existing schema CDO required."));Schema=static_cast<UEdGraphSchema_K2*>(S);
        Core=FindPackage(nullptr,TEXT("/Script/CoreUObject"));if(!Globals()||!Hold(Core)||Core->IsDirty()||uint32(Core->GetPackageFlags())!=16)return Stop(TEXT("Exact already-loaded native double package required."));
        NodeInventory=G->Nodes;FunctionGraphs=BP->FunctionGraphs;GraphGuid=G->GraphGuid;BPFlags=BP->GetFlags();GraphFlags=G->GetFlags();Status=BP->Status;Members=BP->NewVariables;
        for(const auto& H:NodeInventory){if(!H.IsResolved()||H==nullptr)return Stop(TEXT("Resolved graph node inventory required."));UEdGraphNode* N=H.Get();if(!Hold(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid())return Stop(TEXT("Current owned node role required."));Nodes.Add({N,N->NodeGuid,N->Pins});if(N->GetClass()==UK2Node_VariableGet::StaticClass()||N->GetClass()==UK2Node_VariableSet::StaticClass()){Nodes.Last().Variable=true;Nodes.Last().Reference=static_cast<UK2Node_Variable*>(N)->VariableReference;}if(N->GetClass()==UK2Node_FunctionEntry::StaticClass()){if(Entry)return Stop(TEXT("One exact own FunctionEntry required."));Entry=static_cast<UK2Node_FunctionEntry*>(N);}}
        if(!Entry)return Stop(TEXT("One exact own FunctionEntry required."));EntryGuid=Entry->NodeGuid;EntryReference=Entry->FunctionReference;EntryFlags=Entry->GetExtraFlags();Locals=Entry->LocalVariables;FMemberReference OwnReference;OwnReference.SetExternalMember(G->GetFName(),nullptr,EntryReference.GetMemberGuid());if(!EntryReference.IsSameReference(OwnReference)||EntryReference.IsSelfContext()||!Entry->CustomGeneratedFunctionName.IsNone()||EntryReference.GetMemberName()!=G->GetFName()||EntryReference.IsLocalScope()||EntryReference.IsDeprecated()||((EntryFlags&FUNC_Static)!=0)!=PresetLibrary||!EntryWorld()||Locals.Num()>256)return Stop(TEXT("An own nonstatic user function Entry is required."));
        const auto* Desc=Locals.FindByPredicate([&](const auto& X){return X.VarName==Name;});if(!Desc||!Desc->VarGuid.IsValid()||(PrimitiveArrayMode?!PrimitiveArrayTypeClass(Desc->VarType):ReferencedMode?!ReferencedTypeClass(Desc->VarType):NativeValueStructMode?!(NativeValueType(Desc->VarType)||ScalarUdsType(Desc->VarType)):PrimitiveMode?!PrimitiveTypeClass(Desc->VarType):(!Type(Desc->VarType)||!Desc->DefaultValue.IsEmpty()))||Locals.FilterByPredicate([&](const auto& X){return X.VarName==Name||X.VarGuid==Desc->VarGuid;}).Num()!=1)return Stop(TEXT("One exact GUID/String-double Map local descriptor with empty default required."));D=*Desc;if(PrimitiveMode)Name=D.VarName;if(NativeValueStructMode&&!PrimitiveArrayMode){ScalarUdsMode=ScalarUdsType(D.VarType);NativeValueTransform=TransformArrayElementType(D.VarType);if(ScalarUdsMode)R->SetBoolField(TEXT("native_scalar_uds_local_plan"),true);}if(PrimitiveArrayMode){FEdGraphPinType Element=D.VarType;Element.ContainerType=EPinContainerType::None;NativeValueTransform=TransformArrayElementType(Element);ScalarUdsMode=ScalarUdsType(Element);NativeValueStructMode=NativeValueType(Element)||NativeValueTransform||ScalarUdsMode;if(ScalarUdsMode)R->SetBoolField(TEXT("native_scalar_uds_array_local_plan"),true);ReferencedMode=ReferencedTypeClass(Element)!=nullptr;if(ReferencedMode)R->SetBoolField(TEXT("native_referenced_array_local_plan"),true);if(NativeValueStructMode)R->SetBoolField(TEXT("native_value_struct_array_local_plan"),true);}
        for(UClass* C:{Skeleton,Generated})for(TFieldIterator<UFunction> I(C,EFieldIterationFlags::None);I;++I){UFunction* F=*I;if(!Hold(F))return Stop(TEXT("Loaded own function inventory required."));FFunctionRole Role;Role.F=F;Role.Owner=C;Role.Flags=uint32(F->FunctionFlags);for(TFieldIterator<FProperty> P(F,EFieldIterationFlags::None);P;++P)Role.Fields.Add(*P);Functions.Add(MoveTemp(Role));}
        if(PrimitiveMode){if(!InitPrimitiveField())return false;}else
        {
        // Prefer the skeleton role used by the public local member resolver; permit compiled only when it contains the actual field.
        for(const auto& F:Functions)if(F.Owner==Skeleton&&F.F->GetFName()==G->GetFName()){const FProperty* P=F.F->FindPropertyByName(Name);if(P&&P->GetClass()==FMapProperty::StaticClass()){Map=static_cast<const FMapProperty*>(P);LocalFunction=F.F;break;}}
        if(!Map||!LocalFunction||Map->GetOwnerStruct()!=LocalFunction||Map->ArrayDim!=1||Map->HasAnyPropertyFlags(CPF_Parm)||!Map->KeyProp||Map->KeyProp->GetClass()!=FStrProperty::StaticClass()||!Map->ValueProp||Map->ValueProp->GetClass()!=FDoubleProperty::StaticClass())return Stop(TEXT("A currently compiled own local FMapProperty is required; prepare an explicit compile, no descriptor fallback."));Key=static_cast<const FStrProperty*>(Map->KeyProp);Value=static_cast<const FDoubleProperty*>(Map->ValueProp);
        for(const FProperty* P:{static_cast<const FProperty*>(Map),static_cast<const FProperty*>(Key),static_cast<const FProperty*>(Value)}){if(P->ArrayDim!=1)return Stop(TEXT("Scalar local Map/key/value fields required."));Fields.Add({const_cast<FProperty*>(P),P->GetClass(),P->GetOwnerVariant(),P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim});}
        if(Key->GetOwnerProperty()!=Map||Value->GetOwnerProperty()!=Map||Value->GetSize()!=sizeof(double)||Value->GetMinAlignment()!=alignof(double))return Stop(TEXT("Exact String/double local child layout required."));
        }
        if(!((NativeValueStructMode||ReferencedMode)?CurrentPlain():Current()))return false;TSet<UEdGraphPin*> All;for(const auto& N:Nodes)for(auto* P:N.Pins)All.Add(P);
        for(const auto& N:Nodes)for(auto* P:N.Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N.N||!P->PinId.IsValid()||P->ParentPin||!P->SubPins.IsEmpty()||P->bOrphanedPin||P->bDeprecated||!P->DefaultObject.IsResolved())return Stop(TEXT("Whole current physical pins required."));for(auto* L:P->LinkedTo)if(!All.Contains(L)||!L||L->WasTrashed()||!L->LinkedTo.Contains(P))return Stop(TEXT("Direct reciprocal own graph links required."));if(ReferencedMode&&P->DefaultObject!=nullptr&&!Hold(P->DefaultObject.Get()))return Stop(TEXT("Existing loaded pin default object lifetime cannot be captured."));Pins.Add({P,N.N,P->PinId,P->PinName,P->Direction,P->PinType,Flags(P),P->DefaultValue,P->AutogeneratedDefaultValue,P->DefaultObject,P->DefaultTextValue,P->LinkedTo});}
        if(NativeValueStructMode&&!CaptureNativeValueMetadata())return false;
        if(ReferencedMode&&!CaptureReferencedSnapshot())return false;
        if(!Current())return false;FEdGraphPinType Actual;const bool Converted=Schema->ConvertPropertyToPinType(PrimitiveMode?PrimitiveField:static_cast<const FProperty*>(Map),Actual);if(!Current()||!Converted||(PrimitiveMode?!PrimitiveTypeSame(Actual,D.VarType):!Type(Actual)))return Stop(TEXT("Public current local property conversion must match the descriptor exactly."));return true;
    }
    TSharedRef<FJsonObject> Plan()
    {
        if(PrimitiveMode)return PrimitivePlan();
        R->SetStringField(TEXT("target_context"),PresetLibrary?TEXT("preset_loader_library"):TEXT("normal_actor"));R->SetStringField(TEXT("function_library_profile"),PresetLibrary?TEXT("preset_loader"):TEXT(""));R->SetStringField(TEXT("target_parent_class_path"),PresetLibrary?TEXT("/Script/Engine.BlueprintFunctionLibrary"):TEXT("/Script/Engine.Actor"));R->SetStringField(TEXT("function_entry_flags_string"),LexToString(EntryFlags));
        R->SetStringField(TEXT("variable_name"),D.VarName.ToString());R->SetStringField(TEXT("variable_guid"),D.VarGuid.ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("setter"),SetterMode);R->SetStringField(TEXT("graph_path"),G->GetPathName());R->SetStringField(TEXT("graph_id"),GraphGuid.ToString(EGuidFormats::Digits));R->SetStringField(TEXT("local_scope_name"),G->GetName());R->SetStringField(TEXT("function_entry_path"),Entry->GetPathName());R->SetStringField(TEXT("function_entry_guid"),EntryGuid.ToString(EGuidFormats::Digits));R->SetStringField(TEXT("property_path"),Map->GetPathName());R->SetStringField(TEXT("property_flags_string"),LexToString(uint64(Map->GetPropertyFlags())));R->SetStringField(TEXT("key_property_path"),Key->GetPathName());R->SetStringField(TEXT("key_property_flags_string"),LexToString(uint64(Key->GetPropertyFlags())));R->SetStringField(TEXT("value_property_path"),Value->GetPathName());R->SetStringField(TEXT("value_property_flags_string"),LexToString(uint64(Value->GetPropertyFlags())));R->SetStringField(TEXT("property_owner_function_path"),LocalFunction->GetPathName());R->SetStringField(TEXT("property_declaring_role"),LocalFunction->GetOuter()==Skeleton?TEXT("skeleton"):TEXT("generated"));auto Native=MakeShared<FJsonObject>();Native->SetBoolField(TEXT("observations_complete"),true);Native->SetBoolField(TEXT("native_identity_verified"),true);Native->SetStringField(TEXT("type_kind"),TEXT("string_double_map"));Native->SetStringField(TEXT("package_path"),TEXT("/Script/CoreUObject"));Native->SetStringField(TEXT("package_flags_string"),TEXT("16"));Native->SetBoolField(TEXT("package_dirty"),false);Native->SetStringField(TEXT("scalar_property_class"),TEXT("DoubleProperty"));Native->SetStringField(TEXT("scalar_category"),TEXT("real"));Native->SetStringField(TEXT("scalar_subcategory"),TEXT("double"));Native->SetNumberField(TEXT("size"),sizeof(double));Native->SetNumberField(TEXT("alignment"),alignof(double));R->SetObjectField(TEXT("native_value_source_before"),Native);R->SetObjectField(TEXT("native_value_source_after"),Native);R->SetBoolField(TEXT("native_value_source_observed_unchanged"),true);R->SetStringField(TEXT("native_type_path"),TEXT(""));R->SetStringField(TEXT("value_kind"),TEXT("string_double_map"));R->SetObjectField(TEXT("pin_type"),PinTypeObservation(D.VarType));FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&D.VarType,&D.VarType,nullptr,PPF_None,nullptr);if(!Current())return R;R->SetStringField(TEXT("expected_pin_type_text"),TypeText);R->SetBoolField(TEXT("local_descriptor_current_verified"),true);R->SetBoolField(TEXT("local_property_current_verified"),true);R->SetBoolField(TEXT("runtime_value_read_attempted"),false);R->SetBoolField(TEXT("runtime_evaluation_verified"),false);R->SetBoolField(TEXT("default_write_attempted"),false);R->SetStringField(TEXT("stored_default_before"),D.DefaultValue);R->SetStringField(TEXT("stored_default_after"),D.DefaultValue);R->SetBoolField(TEXT("stored_default_observed_unchanged"),true);TArray<TSharedPtr<FJsonValue>> GetterRows;for(const auto& Node:Nodes)if(Node.N->GetClass()==UK2Node_VariableGet::StaticClass()){auto* V=static_cast<UK2Node_VariableGet*>(Node.N);if(V->VariableReference.GetMemberName()!=Name||!V->VariableReference.IsLocalScope())continue;if(!Getter(V))return R;const FProperty* Resolved=V->GetPropertyForVariable();if(!Current()||Resolved!=Map){Stop(TEXT("Current getter property resolution differs from the selected local field."));return R;}auto* P=V->Pins[0];auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("object_path"),V->GetPathName());Row->SetStringField(TEXT("node_guid"),V->NodeGuid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("graph_path"),G->GetPathName());Row->SetStringField(TEXT("node_class_path"),V->GetClass()->GetPathName());Row->SetStringField(TEXT("member_name"),V->VariableReference.GetMemberName().ToString());Row->SetStringField(TEXT("member_guid"),V->VariableReference.GetMemberGuid().ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("member_local_scope"),V->VariableReference.GetMemberScopeName());Row->SetBoolField(TEXT("member_self_context"),V->VariableReference.IsSelfContext());Row->SetStringField(TEXT("property_path"),Map->GetPathName());Row->SetStringField(TEXT("value_pin_name"),P->PinName.ToString());Row->SetStringField(TEXT("value_pin_direction"),TEXT("output"));Row->SetStringField(TEXT("value_pin_id"),P->PinId.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("value_pin_owner_path"),V->GetPathName());Row->SetNumberField(TEXT("value_pin_flags"),Flags(P));Row->SetObjectField(TEXT("pin_type"),PinTypeObservation(P->PinType));Row->SetBoolField(TEXT("value_pin_whole_current_verified"),true);GetterRows.Add(MakeShared<FJsonValueObject>(Row));}R->SetArrayField(TEXT("native_value_member_nodes"),GetterRows);if(SetterMode){TArray<TSharedPtr<FJsonValue>> SetterNodeRows;if(!SetterRows(SetterNodeRows))return R;R->SetArrayField(TEXT("native_local_setter_nodes"),SetterNodeRows);}if(!Current())return R;const bool PlanDirtyAfter=Package->IsDirty();R->SetBoolField(TEXT("dirty_after"),PlanDirtyAfter);if(PlanDirtyAfter!=PlanDirtyBefore){Stop(TEXT("Local plan observation changed the target package dirty state."));return R;}if(Current()){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));}return R;
    }
    bool Setter(UK2Node_VariableSet* N,bool InGraph=true,bool Empty=false)
    {
        if(!Current()||!N||!Live(N)||N->GetClass()!=UK2Node_VariableSet::StaticClass()||N->GetOuter()!=G||(InGraph&&!G->Nodes.Contains(N)))return Stop(TEXT("Current exact local setter node required."));
        FMemberReference Expected;Expected.SetLocalMember(Name,G->GetName(),D.VarGuid);
        if(!N->VariableReference.IsSameReference(Expected)||!N->VariableReference.IsLocalScope()||N->VariableReference.IsDeprecated()||N->VariableReference.GetMemberGuid()!=D.VarGuid||!N->VariableReference.GetMemberScopeName().Equals(G->GetName(),ESearchCase::CaseSensitive)||N->Pins.Num()!=4)return Stop(TEXT("Local setter reference and complete four-pin inventory required."));
        TSet<FName> Names;TSet<FGuid> Ids;
        for(auto* P:N->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||Names.Contains(P->PinName)||P->bOrphanedPin||P->bDeprecated||P->bNotConnectable||P->ParentPin||!P->SubPins.IsEmpty()||!P->DefaultObject.IsResolved()||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty()||P->DefaultValue.Len()>1048576||P->AutogeneratedDefaultValue.Len()>1048576||(Empty&&!P->LinkedTo.IsEmpty()))return Stop(TEXT("Whole current local setter pin role/default inventory required."));
            Names.Add(P->PinName);Ids.Add(P->PinId);
            if(P->PinName==UEdGraphSchema_K2::PN_Execute||P->PinName==UEdGraphSchema_K2::PN_Then)
            {FEdGraphPinType E;E.PinCategory=UEdGraphSchema_K2::PC_Exec;const bool Input=P->PinName==UEdGraphSchema_K2::PN_Execute;if(P->Direction!=(Input?EGPD_Input:EGPD_Output)||P->PinType!=E||P->PinType.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat)return Stop(TEXT("Local setter execute/then pin descriptor differs."));}
            else if(P->PinName==Name||P->PinName==TEXT("Output_Get"))
            {if(P->Direction!=(P->PinName==Name?EGPD_Input:EGPD_Output)||!Type(P->PinType))return Stop(TEXT("Local setter input/Output_Get full Map descriptor differs."));}
            else return Stop(TEXT("Unexpected local setter pin; no self or seeded fallback is admitted."));
        }
        return Names.Contains(UEdGraphSchema_K2::PN_Execute)&&Names.Contains(UEdGraphSchema_K2::PN_Then)&&Names.Contains(Name)&&Names.Contains(TEXT("Output_Get"))?true:Stop(TEXT("Missing local setter logical pin role."));
    }
    bool SetterJson(UK2Node_VariableSet* N,TSharedRef<FJsonObject>& Row)
    {
        if(!Setter(N))return false;
        const FProperty* Resolved=N->GetPropertyForVariable();if(!Current())return false;
        if(Resolved!=Map)return Stop(TEXT("Current setter resolver differs from held skeleton local Map property."));
        Row->SetStringField(TEXT("node_id"),N->GetPathName());Row->SetStringField(TEXT("object_path"),N->GetPathName());Row->SetStringField(TEXT("node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("node_class_path"),N->GetClass()->GetPathName());Row->SetStringField(TEXT("graph_path"),G->GetPathName());
        Row->SetStringField(TEXT("variable_name"),D.VarName.ToString());Row->SetStringField(TEXT("variable_guid"),D.VarGuid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("local_scope_name"),G->GetName());Row->SetStringField(TEXT("property_path"),Map->GetPathName());Row->SetObjectField(TEXT("pin_type"),PinTypeObservation(D.VarType));
        Row->SetStringField(TEXT("member_name"),N->VariableReference.GetMemberName().ToString());Row->SetStringField(TEXT("member_guid"),N->VariableReference.GetMemberGuid().ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("member_local_scope"),N->VariableReference.GetMemberScopeName());Row->SetBoolField(TEXT("member_self_context"),N->VariableReference.IsSelfContext());Row->SetBoolField(TEXT("setter"),true);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(auto* P:N->Pins)
        {
            if(!Current())return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),P->PinName.ToString());J->SetStringField(TEXT("direction"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetStringField(TEXT("pin_id"),P->PinId.ToString(EGuidFormats::Digits));J->SetObjectField(TEXT("pin_type"),PinTypeObservation(P->PinType));J->SetNumberField(TEXT("flags"),Flags(P));J->SetStringField(TEXT("raw_default_value"),P->DefaultValue);J->SetStringField(TEXT("autogenerated_default_value"),P->AutogeneratedDefaultValue);J->SetStringField(TEXT("default_object_path"),FString());J->SetBoolField(TEXT("default_text_empty"),P->DefaultTextValue.IsEmpty());
            if(P->PinName==Name&&P->Direction==EGPD_Input)Row->SetStringField(TEXT("value_pin_id"),P->PinId.ToString(EGuidFormats::Digits));if(P->PinName==TEXT("Output_Get")&&P->Direction==EGPD_Output)Row->SetStringField(TEXT("output_pin_id"),P->PinId.ToString(EGuidFormats::Digits));
            TArray<TSharedPtr<FJsonValue>> Links;
            for(auto* L:P->LinkedTo)
            {
                if(!Current()||!Pins.ContainsByPredicate([&](const FPinRole& Q){return Q.P==L;}))return Stop(TEXT("Setter receipt peer is not a current captured own graph pin."));
                auto Link=MakeShared<FJsonObject>();Link->SetStringField(TEXT("node_id"),L->GetOwningNodeUnchecked()->GetPathName());Link->SetStringField(TEXT("pin_name"),L->PinName.ToString());Link->SetStringField(TEXT("direction"),L->Direction==EGPD_Input?TEXT("input"):TEXT("output"));Links.Add(MakeShared<FJsonValueObject>(Link));
            }
            J->SetArrayField(TEXT("links"),Links);Rows.Add(MakeShared<FJsonValueObject>(J));
        }
        Row->SetArrayField(TEXT("pins"),Rows);Row->SetBoolField(TEXT("local_reference_current_verified"),true);Row->SetBoolField(TEXT("physical_pin_current_verified"),true);return Current();
    }
    bool SetterRows(TArray<TSharedPtr<FJsonValue>>& Rows)
    {
        if(!Current())return false;
        for(const auto& Node:Nodes)if(Node.N->GetClass()==UK2Node_VariableSet::StaticClass())
        {
            auto* N=static_cast<UK2Node_VariableSet*>(Node.N);if(N->VariableReference.GetMemberName()!=Name||!N->VariableReference.IsLocalScope())continue;
            auto Row=MakeShared<FJsonObject>();if(!SetterJson(N,Row))return false;Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        return Current();
    }
    bool Getter(UK2Node_VariableGet* N)
    {
        FMemberReference ExpectedLocal;ExpectedLocal.SetLocalMember(Name,G->GetName(),D.VarGuid);
        if(!Current()||!N||!N->VariableReference.IsSameReference(ExpectedLocal)||N->GetClass()!=UK2Node_VariableGet::StaticClass()||N->GetOuter()!=G||!G->Nodes.Contains(N)||!N->VariableReference.IsLocalScope()||N->VariableReference.IsDeprecated()||N->VariableReference.GetMemberName()!=Name||N->VariableReference.GetMemberGuid()!=D.VarGuid||!N->VariableReference.GetMemberScopeName().Equals(G->GetName(),ESearchCase::CaseSensitive)||N->Pins.Num()!=1)return Stop(TEXT("Current exact local getter reference/physical inventory required."));
        auto* P=N->Pins[0];if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||P->PinName!=Name||P->Direction!=EGPD_Output||!Type(P->PinType)||P->bHidden||P->bNotConnectable||P->bOrphanedPin||P->bDeprecated||P->ParentPin||!P->SubPins.IsEmpty()||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return Stop(TEXT("Current whole local Map value output required."));return true;
    }
};
TSharedRef<FJsonObject> ReadLocalDoubleMapPlan(UEdGraph* G,const FString& Name,bool Setter,const TSharedRef<FJsonObject>& R)
{R->SetStringField(TEXT("member_scope"),TEXT("local_native_string_double_map"));FLocalDoubleMap S(R,Setter);if(!S.Init(G,Name))return R;return S.Plan();}
TSharedRef<FJsonObject> ReadPrimitiveLocalPlan(UEdGraph* G,const FString& Name,bool Setter,const TSharedRef<FJsonObject>& R)
{FLocalDoubleMap S(R,Setter,true);if(!S.Init(G,Name))return R;return S.Plan();}
TSharedRef<FJsonObject> ReadNativeValueLocalPlan(UEdGraph* G,const FString& Name,bool Setter,const TSharedRef<FJsonObject>& R)
{FLocalDoubleMap S(R,Setter,false,true);if(!S.Init(G,Name))return R;return S.Plan();}
TSharedRef<FJsonObject> ReadReferencedLocalPlan(UEdGraph* G,const FString& Name,bool Setter,const TSharedRef<FJsonObject>& R)
{FLocalDoubleMap S(R,Setter,false,false,true);if(!S.Init(G,Name))return R;return S.Plan();}
TSharedRef<FJsonObject> ReadPrimitiveArrayLocalPlan(UEdGraph* G,const FString& Name,bool Setter,const TSharedRef<FJsonObject>& R)
{FLocalDoubleMap S(R,Setter,false,false,false,true);if(!S.Init(G,Name))return R;return S.Plan();}
}

Function::FResult AddStringDoubleLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{
    Function::FResult Out;auto Observation=Result();Out.Data->SetObjectField(TEXT("local_preflight"),Observation);Out.Data->SetBoolField(TEXT("native_string_double_map_local_getter"),true);Out.Data->SetBoolField(TEXT("automatic_retry_safe"),false);
    const auto Bad=[&](const TCHAR* Code,const TCHAR* Why){Out.bSuccess=Out.bPostconditionPassed=false;Out.ErrorCode=Code;Out.Error=Why;return Out;};
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||FMath::Abs(int64(X))>1000000||FMath::Abs(int64(Y))>1000000)return Bad(TEXT("graph_local_map_input_invalid"),TEXT("Idle editor and bounded positions required before loaded lookup."));
    UEdGraph* G=ResolveLoadedGraph(GraphPath,Observation);if(!G)return Bad(TEXT("graph_local_map_scope_unavailable"),TEXT("Loaded graph resolution failed; native observation retained."));FLocalDoubleMap S(Observation);if(!S.Init(G,VariableName))return Bad(TEXT("graph_local_map_scope_unavailable"),TEXT("Current local Map declaration/property preflight failed; no node allocated."));
    if(!S.D.VarGuid.ToString(EGuidFormats::Digits).Equals(ExpectedLocalGuid,ESearchCase::CaseSensitive))return Bad(TEXT("graph_local_map_guid_changed"),TEXT("Expected local GUID differs from the currently observed descriptor."));S.Plan();if(!Observation->GetBoolField(TEXT("success")))return Bad(TEXT("graph_local_map_scope_changed"),TEXT("Local plan changed during copied type export."));
    const auto Check=[&](){if(S.Current())return true;Out.bChangeStateKnown=false;Out.ErrorCode=TEXT("graph_local_map_scope_changed");Out.Error=TEXT("Current local/source/target changed after the retained native prefix; no later SDK or retry.");return false;};
    Out.Stage=TEXT("modify_graph");if(!Check())return Out;Out.bMutationAttempted=true;Out.bChangeStateKnown=false;Out.Data->SetBoolField(TEXT("graph_modify_attempted"),true);const bool Modified=G->Modify();Out.Data->SetBoolField(TEXT("graph_modify_returned"),true);Out.Data->SetBoolField(TEXT("graph_modify_result"),Modified);S.ObservePackageDirtyTransaction=true;if(!Check())return Out;S.ObservePackageDirtyTransaction=false;
    Out.Stage=TEXT("allocate_local_getter");Out.Data->SetBoolField(TEXT("allocation_attempted"),true);auto* N=NewObject<UK2Node_VariableGet>(G,NAME_None,RF_Transactional);Out.Data->SetBoolField(TEXT("allocation_returned"),true);Out.Data->SetBoolField(TEXT("allocation_returned_nonnull"),N!=nullptr);
    // Minimum safe returned identity precedes broad Editor/target checks.
    if(IsEditorGameThread()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GExitPurge&&!IsEngineExitRequested()&&FLocalDoubleMap::Live(N)){S.Held.Emplace(N);Out.Data->SetStringField(TEXT("allocated_node_path"),N->GetPathName());Out.Data->SetStringField(TEXT("allocated_node_class"),N->GetClass()->GetPathName());Out.Data->SetStringField(TEXT("allocated_node_outer"),N->GetOuter()?N->GetOuter()->GetPathName():FString());Out.Data->SetBoolField(TEXT("returned_identity_observed"),true);}else Out.Data->SetBoolField(TEXT("returned_identity_observed"),false);
    S.Added=N;if(!Check())return Out;if(!N)return Bad(TEXT("graph_local_map_allocation_failed"),TEXT("NewObject returned null; modify/allocation prefix retained."));
    Out.Stage=TEXT("create_local_getter_guid");N->CreateNewGuid();Out.Data->SetBoolField(TEXT("create_guid_returned"),true);S.AddedGuid=N->NodeGuid;Out.Data->SetStringField(TEXT("created_node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));if(!Check())return Out;if(!N->NodeGuid.IsValid()||S.Nodes.ContainsByPredicate([&](const auto& Old){return Old.Guid==N->NodeGuid;}))return Bad(TEXT("graph_local_map_guid_invalid"),TEXT("Returned new node GUID is invalid or duplicated; no subsequent SDK or repair."));
    Out.Stage=TEXT("bind_local_reference");N->NodePosX=X;N->NodePosY=Y;S.AddedPosition=true;S.AddedX=X;S.AddedY=Y;N->VariableReference.SetLocalMember(S.Name,G->GetName(),S.D.VarGuid);S.AddedReference=N->VariableReference;S.AddedBound=true;Out.Data->SetBoolField(TEXT("set_local_member_returned"),true);if(!Check())return Out;
    Out.Stage=TEXT("allocate_default_pins");N->AllocateDefaultPins();Out.Data->SetBoolField(TEXT("allocate_default_pins_returned"),true);if(!Check())return Out;
    // Freeze only the freshly reacquired physical inventory after the allocation callback.
    if(N->Pins.Num()!=1)return Bad(TEXT("graph_local_map_getter_postcondition_failed"),TEXT("Stock local allocation did not return one output; retained partial prefix."));
    {auto* Q=N->Pins[0];if(!Q||Q->WasTrashed()||Q->GetOwningNodeUnchecked()!=N||!Q->PinId.IsValid()||Q->PinName!=S.Name||Q->Direction!=EGPD_Output||!FLocalDoubleMap::Type(Q->PinType)||Q->bHidden||Q->bNotConnectable||Q->bOrphanedPin||Q->bDeprecated||Q->ParentPin||!Q->SubPins.IsEmpty()||!Q->LinkedTo.IsEmpty()||Q->DefaultObject!=nullptr||!Q->DefaultTextValue.IsEmpty())return Bad(TEXT("graph_local_map_getter_postcondition_failed"),TEXT("Stock local output shape is unavailable; no descriptor seed or repair."));S.Nodes.Add({N,N->NodeGuid,N->Pins});S.Pins.Add({Q,N,Q->PinId,Q->PinName,Q->Direction,Q->PinType,FLocalDoubleMap::Flags(Q),Q->DefaultValue,Q->AutogeneratedDefaultValue,Q->DefaultObject,Q->DefaultTextValue,Q->LinkedTo});}
    const FProperty* Resolved=N->GetPropertyForVariable();Out.Data->SetBoolField(TEXT("local_property_resolution_returned"),true);if(!Check())return Out;if(Resolved!=S.Map)return Bad(TEXT("graph_local_map_getter_postcondition_failed"),TEXT("Stock local resolver differs from the held current skeleton field."));
    Out.Stage=TEXT("post_placed");N->PostPlacedNewNode();Out.Data->SetBoolField(TEXT("post_placed_returned"),true);if(!Check())return Out;
    Out.Stage=TEXT("add_local_getter_to_graph");G->AddNode(N,false,false);S.AddedInGraph=true;Out.Data->SetBoolField(TEXT("add_node_returned"),true);if(!Check())return Out;
    if(!S.Getter(N))return Bad(TEXT("graph_local_map_getter_postcondition_failed"),TEXT("Returned local getter reference or physical pin differs; retained partial graph, no repair."));
    Out.Stage=TEXT("mark_modified");FBlueprintEditorUtils::MarkBlueprintAsModified(S.BP);Out.Data->SetBoolField(TEXT("mark_modified_returned"),true);S.Status=BS_Dirty;if(!Check()||!S.Getter(N))return Out;
    auto* P=N->Pins[0];auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("node_id"),N->GetPathName());Row->SetStringField(TEXT("object_path"),N->GetPathName());Row->SetStringField(TEXT("node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("variable_name"),S.D.VarName.ToString());Row->SetStringField(TEXT("variable_guid"),S.D.VarGuid.ToString(EGuidFormats::Digits));Row->SetStringField(TEXT("local_scope_name"),G->GetName());Row->SetStringField(TEXT("value_pin_id"),P->PinId.ToString(EGuidFormats::Digits));Row->SetObjectField(TEXT("pin_type"),PinTypeObservation(P->PinType));Row->SetStringField(TEXT("property_path"),S.Map->GetPathName());Row->SetBoolField(TEXT("local_reference_current_verified"),true);Row->SetBoolField(TEXT("physical_pin_current_verified"),true);Out.Data->SetObjectField(TEXT("node"),Row);
    if(!Check())return Out;Out.bChanged=Out.bChangeStateKnown=Out.bPostconditionPassed=Out.bSuccess=true;Out.Stage=TEXT("complete");return Out;
}

Function::FResult AddStringDoubleLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{
    Function::FResult Out;auto Observation=Result();Out.Data->SetObjectField(TEXT("local_preflight"),Observation);Out.Data->SetBoolField(TEXT("native_string_double_map_local_setter"),true);Out.Data->SetBoolField(TEXT("automatic_retry_safe"),false);Out.Data->SetBoolField(TEXT("atomic_rollback_available"),false);
    const auto Bad=[&](const TCHAR* Code,const TCHAR* Why){Out.bSuccess=Out.bPostconditionPassed=false;Out.ErrorCode=Code;Out.Error=Why;return Out;};
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||FMath::Abs(int64(X))>1000000||FMath::Abs(int64(Y))>1000000)return Bad(TEXT("graph_local_map_input_invalid"),TEXT("Idle editor and bounded positions required before loaded lookup."));
    UEdGraph* G=ResolveLoadedGraph(GraphPath,Observation);if(!G)return Bad(TEXT("graph_local_map_scope_unavailable"),TEXT("Loaded graph resolution failed; native observation retained."));FLocalDoubleMap S(Observation,true);if(!S.Init(G,VariableName))return Bad(TEXT("graph_local_map_scope_unavailable"),TEXT("Current local Map declaration/property preflight failed; no node allocated."));
    if(!S.D.VarGuid.ToString(EGuidFormats::Digits).Equals(ExpectedLocalGuid,ESearchCase::CaseSensitive))return Bad(TEXT("graph_local_map_guid_changed"),TEXT("Expected local GUID differs from the currently observed descriptor."));S.Plan();if(!Observation->GetBoolField(TEXT("success")))return Bad(TEXT("graph_local_map_scope_changed"),TEXT("Local plan changed during copied type export."));
    const auto Check=[&](){Observation->SetStringField(TEXT("stage"),Out.Stage);if(S.Current())return true;Out.bChangeStateKnown=false;Out.ErrorCode=TEXT("graph_local_map_scope_changed");Out.Error=TEXT("Current local/source/target changed after the retained native setter prefix; no later SDK or retry.");return false;};
    Out.Stage=TEXT("modify_graph");if(!Check())return Out;Out.bMutationAttempted=true;Out.bChangeStateKnown=false;Out.Data->SetBoolField(TEXT("graph_modify_attempted"),true);const bool Modified=G->Modify();Out.Data->SetBoolField(TEXT("graph_modify_returned"),true);Out.Data->SetBoolField(TEXT("graph_modify_result"),Modified);S.ObservePackageDirtyTransaction=true;if(!Check())return Out;S.ObservePackageDirtyTransaction=false;
    Out.Stage=TEXT("allocate_local_setter");Out.Data->SetBoolField(TEXT("allocation_attempted"),true);auto* N=NewObject<UK2Node_VariableSet>(G,NAME_None,RF_Transactional);Out.Data->SetBoolField(TEXT("allocation_returned"),true);Out.Data->SetBoolField(TEXT("allocation_returned_nonnull"),N!=nullptr);
    // Observe only the safe returned object before a broader callback-current gate.
    if(IsEditorGameThread()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GExitPurge&&!IsEngineExitRequested()&&FLocalDoubleMap::Live(N)){S.Held.Emplace(N);Out.Data->SetStringField(TEXT("allocated_node_path"),N->GetPathName());Out.Data->SetStringField(TEXT("allocated_node_class"),N->GetClass()->GetPathName());Out.Data->SetStringField(TEXT("allocated_node_outer"),N->GetOuter()?N->GetOuter()->GetPathName():FString());Out.Data->SetBoolField(TEXT("returned_identity_observed"),true);}else Out.Data->SetBoolField(TEXT("returned_identity_observed"),false);
    S.Added=N;if(!Check())return Out;if(!N)return Bad(TEXT("graph_local_map_allocation_failed"),TEXT("NewObject returned null; modify/allocation prefix retained."));
    Out.Stage=TEXT("create_local_setter_guid");N->CreateNewGuid();Out.Data->SetBoolField(TEXT("create_guid_returned"),true);if(!Check())return Out;S.AddedGuid=N->NodeGuid;Out.Data->SetStringField(TEXT("created_node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));if(!N->NodeGuid.IsValid()||S.Nodes.ContainsByPredicate([&](const auto& Old){return Old.Guid==N->NodeGuid;}))return Bad(TEXT("graph_local_map_guid_invalid"),TEXT("Returned node GUID invalid or duplicated; no next SDK or repair."));
    Out.Stage=TEXT("bind_local_reference");N->NodePosX=X;N->NodePosY=Y;S.AddedPosition=true;S.AddedX=X;S.AddedY=Y;N->VariableReference.SetLocalMember(S.Name,G->GetName(),S.D.VarGuid);S.AddedReference=N->VariableReference;S.AddedBound=true;Out.Data->SetBoolField(TEXT("set_local_member_returned"),true);if(!Check())return Out;
    Out.Stage=TEXT("allocate_default_pins");N->AllocateDefaultPins();Out.Data->SetBoolField(TEXT("allocate_default_pins_returned"),true);if(!Check())return Out;
    Out.Data->SetNumberField(TEXT("expected_local_setter_pin_count"),4);Out.Data->SetNumberField(TEXT("actual_local_setter_pin_count"),N->Pins.Num());
    if(!S.Setter(N,false,true))return Bad(TEXT("graph_local_map_setter_postcondition_failed"),TEXT("Stock local setter did not return its complete four roles; partial prefix retained, no seeding or retry."));
    // Fresh physical storage is acquired after the callback, then frozen before
    // property resolution/PostPlaced/AddNode/MarkModified can invoke more work.
    FLocalDoubleMap::FNodeRole Role;Role.N=N;Role.Guid=N->NodeGuid;Role.Pins=N->Pins;Role.Variable=true;Role.Reference=N->VariableReference;S.Nodes.Add(MoveTemp(Role));
    for(auto* P:N->Pins)S.Pins.Add({P,N,P->PinId,P->PinName,P->Direction,P->PinType,FLocalDoubleMap::Flags(P),P->DefaultValue,P->AutogeneratedDefaultValue,P->DefaultObject,P->DefaultTextValue,P->LinkedTo});
    const FProperty* Resolved=N->GetPropertyForVariable();Out.Data->SetBoolField(TEXT("local_property_resolution_returned"),true);if(!Check())return Out;if(Resolved!=S.Map)return Bad(TEXT("graph_local_map_setter_postcondition_failed"),TEXT("Stock local resolver differs from the held current skeleton field."));
    Out.Stage=TEXT("post_placed");N->PostPlacedNewNode();Out.Data->SetBoolField(TEXT("post_placed_returned"),true);if(!Check())return Out;
    Out.Stage=TEXT("add_local_setter_to_graph");G->AddNode(N,false,false);S.AddedInGraph=true;Out.Data->SetBoolField(TEXT("add_node_returned"),true);if(!Check())return Out;
    if(!S.Setter(N,true,true))return Bad(TEXT("graph_local_map_setter_postcondition_failed"),TEXT("Returned local setter reference/whole physical pins differ; partial graph retained."));
    Out.Stage=TEXT("mark_modified");FBlueprintEditorUtils::MarkBlueprintAsModified(S.BP);Out.Data->SetBoolField(TEXT("mark_modified_returned"),true);S.Status=BS_Dirty;if(!Check())return Out;
    auto Row=MakeShared<FJsonObject>();if(!S.SetterJson(N,Row))return Bad(TEXT("graph_local_map_setter_postcondition_failed"),TEXT("Current setter receipt could not be completed; no later SDK, save or retry."));Out.Data->SetObjectField(TEXT("node"),Row);
    if(!Check())return Out;Out.bChanged=Out.bChangeStateKnown=Out.bPostconditionPassed=Out.bSuccess=true;Out.Stage=TEXT("complete");return Out;
}

namespace
{
Function::FResult AddPrimitiveLocalNode(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y,bool Setter,bool NativeValue=false,bool Referenced=false,bool PrimitiveArray=false)
{
    Function::FResult Out;auto Observation=Result();Out.Data->SetObjectField(TEXT("local_preflight"),Observation);Out.Data->SetBoolField(PrimitiveArray?(Setter?TEXT("native_primitive_array_local_setter"):TEXT("native_primitive_array_local_getter")):Referenced?(Setter?TEXT("native_referenced_local_setter"):TEXT("native_referenced_local_getter")):NativeValue?(Setter?TEXT("native_value_struct_local_setter"):TEXT("native_value_struct_local_getter")):(Setter?TEXT("native_primitive_local_setter"):TEXT("native_primitive_local_getter")),true);Out.Data->SetBoolField(TEXT("automatic_retry_safe"),false);Out.Data->SetBoolField(TEXT("atomic_rollback_available"),false);
    const auto Bad=[&](const TCHAR* Code,const TCHAR* Why){Out.bSuccess=Out.bPostconditionPassed=false;Out.ErrorCode=Code;Out.Error=Why;bool Known=true;if(Observation->TryGetBoolField(TEXT("change_state_known"),Known)&&!Known)Out.bChangeStateKnown=false;return Out;};
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||FMath::Abs(int64(X))>1000000||FMath::Abs(int64(Y))>1000000)return Bad(TEXT("graph_local_primitive_input_invalid"),TEXT("Idle editor and bounded positions required before loaded lookup."));
    UEdGraph* G=ResolveLoadedGraph(GraphPath,Observation);if(!G)return Bad(TEXT("graph_local_primitive_scope_unavailable"),TEXT("Loaded graph resolution failed; native observation retained."));FLocalDoubleMap S(Observation,Setter,true,NativeValue,Referenced,PrimitiveArray);if(!S.Init(G,VariableName))return Bad(TEXT("graph_local_primitive_scope_unavailable"),TEXT("Current primitive local declaration/property preflight failed; no node allocated."));if(S.PrimitiveArrayMode&&S.ReferencedMode)Out.Data->SetBoolField(Setter?TEXT("native_referenced_array_local_setter"):TEXT("native_referenced_array_local_getter"),true);if(S.PrimitiveArrayMode&&S.NativeValueStructMode)Out.Data->SetBoolField(Setter?TEXT("native_value_struct_array_local_setter"):TEXT("native_value_struct_array_local_getter"),true);
    if(S.ScalarUdsMode){Out.Data->SetBoolField(S.PrimitiveArrayMode?(Setter?TEXT("native_scalar_uds_array_local_setter"):TEXT("native_scalar_uds_array_local_getter")):(Setter?TEXT("native_scalar_uds_local_setter"):TEXT("native_scalar_uds_local_getter")),true);S.ScalarUdsWritePlan=true;}
    if(!S.D.VarGuid.ToString(EGuidFormats::Digits).Equals(ExpectedLocalGuid,ESearchCase::CaseSensitive))return Bad(TEXT("graph_local_primitive_guid_changed"),TEXT("Expected local GUID differs from the currently observed descriptor."));S.Plan();if(!Observation->GetBoolField(TEXT("success")))return Bad(TEXT("graph_local_primitive_scope_changed"),TEXT("Local plan changed during copied type export."));
    const auto Check=[&](bool Plain=false){Observation->SetStringField(TEXT("stage"),Out.Stage);if(Plain?S.CurrentPlain():S.Current())return true;Out.bChangeStateKnown=false;Out.ErrorCode=TEXT("graph_local_primitive_scope_changed");Out.Error=TEXT("Current local/source/target changed after the retained native prefix; no later SDK or retry.");return false;};
    Out.Stage=TEXT("modify_graph");if(!Check())return Out;Out.bMutationAttempted=true;Out.bChangeStateKnown=false;Out.Data->SetBoolField(TEXT("graph_modify_attempted"),true);const bool Modified=G->Modify();Out.Data->SetBoolField(TEXT("graph_modify_returned"),true);Out.Data->SetBoolField(TEXT("graph_modify_result"),Modified);S.ObservePackageDirtyTransaction=true;if(!Check())return Out;S.ObservePackageDirtyTransaction=false;
    Out.Stage=Setter?TEXT("allocate_local_setter"):TEXT("allocate_local_getter");Out.Data->SetBoolField(TEXT("allocation_attempted"),true);
    UK2Node_Variable* N=Setter?static_cast<UK2Node_Variable*>(NewObject<UK2Node_VariableSet>(G,NAME_None,RF_Transactional)):static_cast<UK2Node_Variable*>(NewObject<UK2Node_VariableGet>(G,NAME_None,RF_Transactional));Out.Data->SetBoolField(TEXT("allocation_returned"),true);Out.Data->SetBoolField(TEXT("allocation_returned_nonnull"),N!=nullptr);
    if(IsEditorGameThread()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GExitPurge&&!IsEngineExitRequested()&&FLocalDoubleMap::Live(N)){S.Held.Emplace(N);Out.Data->SetStringField(TEXT("allocated_node_path"),N->GetPathName());Out.Data->SetStringField(TEXT("allocated_node_class"),N->GetClass()->GetPathName());Out.Data->SetStringField(TEXT("allocated_node_outer"),N->GetOuter()?N->GetOuter()->GetPathName():FString());Out.Data->SetBoolField(TEXT("returned_identity_observed"),true);}else Out.Data->SetBoolField(TEXT("returned_identity_observed"),false);
    S.Added=N;if(!Check())return Out;if(!N)return Bad(TEXT("graph_local_primitive_allocation_failed"),TEXT("NewObject returned null; modify/allocation prefix retained."));
    Out.Stage=TEXT("create_local_node_guid");N->CreateNewGuid();Out.Data->SetBoolField(TEXT("create_guid_returned"),true);if(!Check())return Out;S.AddedGuid=N->NodeGuid;Out.Data->SetStringField(TEXT("created_node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));if(!N->NodeGuid.IsValid()||S.Nodes.ContainsByPredicate([&](const auto& Old){return Old.Guid==N->NodeGuid;}))return Bad(TEXT("graph_local_primitive_guid_invalid"),TEXT("Returned new node GUID is invalid or duplicated; no subsequent SDK or repair."));
    Out.Stage=TEXT("bind_local_reference");N->NodePosX=X;N->NodePosY=Y;S.AddedPosition=true;S.AddedX=X;S.AddedY=Y;N->VariableReference.SetLocalMember(S.Name,G->GetName(),S.D.VarGuid);S.AddedReference=N->VariableReference;S.AddedBound=true;Out.Data->SetBoolField(TEXT("set_local_member_returned"),true);if(!Check())return Out;
    Out.Stage=TEXT("allocate_default_pins");N->AllocateDefaultPins();Out.Data->SetBoolField(TEXT("allocate_default_pins_returned"),true);if(!Check(Referenced))return Out;
    Out.Data->SetNumberField(TEXT("expected_local_pin_count"),Setter?4:1);Out.Data->SetNumberField(TEXT("actual_local_pin_count"),N->Pins.Num());
    if(!S.PrimitiveNode(N,false,true,Referenced))return Bad(TEXT("graph_local_primitive_postcondition_failed"),TEXT("Stock allocation did not return complete local roles; actual partial prefix retained, no seeding or retry."));
    FLocalDoubleMap::FNodeRole Role;Role.N=N;Role.Guid=N->NodeGuid;Role.Pins=N->Pins;Role.Variable=true;Role.Reference=N->VariableReference;S.Nodes.Add(MoveTemp(Role));
    for(auto* P:N->Pins)S.Pins.Add({P,N,P->PinId,P->PinName,P->Direction,P->PinType,FLocalDoubleMap::Flags(P),P->DefaultValue,P->AutogeneratedDefaultValue,P->DefaultObject,P->DefaultTextValue,P->LinkedTo});
    if(Referenced){for(auto* P:N->Pins)if(P->DefaultObject!=nullptr&&!S.Hold(P->DefaultObject.Get()))return Bad(TEXT("graph_local_referenced_default_object_unavailable"),TEXT("Actual returned default object lifetime cannot be captured; partial node retained, no next SDK."));if(!Check())return Out;}
    const FProperty* Resolved=N->GetPropertyForVariable();Out.Data->SetBoolField(TEXT("local_property_resolution_returned"),true);if(!Check())return Out;if(Resolved!=S.PrimitiveField)return Bad(TEXT("graph_local_primitive_postcondition_failed"),TEXT("Stock local resolver differs from the held current skeleton field."));
    Out.Stage=TEXT("post_placed");N->PostPlacedNewNode();Out.Data->SetBoolField(TEXT("post_placed_returned"),true);if(!Check())return Out;
    Out.Stage=TEXT("add_local_node_to_graph");G->AddNode(N,false,false);S.AddedInGraph=true;Out.Data->SetBoolField(TEXT("add_node_returned"),true);if(!Check())return Out;
    if(!S.PrimitiveNode(N,true,true))return Bad(TEXT("graph_local_primitive_postcondition_failed"),TEXT("Returned local reference or whole pins differ; partial graph retained."));
    Out.Stage=TEXT("mark_modified");FBlueprintEditorUtils::MarkBlueprintAsModified(S.BP);Out.Data->SetBoolField(TEXT("mark_modified_returned"),true);S.Status=BS_Dirty;if(!Check())return Out;
    auto Row=MakeShared<FJsonObject>();if(!S.PrimitiveNodeJson(N,Row))return Bad(TEXT("graph_local_primitive_postcondition_failed"),TEXT("Current whole node receipt could not be completed; no later SDK, save or retry."));Out.Data->SetObjectField(TEXT("node"),Row);
    if(!Check()||(S.ScalarUdsMode&&!S.ScalarUds->FinalFile()))return Out;Out.bChanged=Out.bChangeStateKnown=Out.bPostconditionPassed=Out.bSuccess=true;Out.Stage=TEXT("complete");return Out;
}
}
Function::FResult AddPrimitiveLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{return AddPrimitiveLocalNode(GraphPath,VariableName,ExpectedLocalGuid,X,Y,false);}
Function::FResult AddPrimitiveLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{return AddPrimitiveLocalNode(GraphPath,VariableName,ExpectedLocalGuid,X,Y,true);}

Function::FResult AddNativeValueLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{return AddPrimitiveLocalNode(GraphPath,VariableName,ExpectedLocalGuid,X,Y,false,true);}
Function::FResult AddNativeValueLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{return AddPrimitiveLocalNode(GraphPath,VariableName,ExpectedLocalGuid,X,Y,true,true);}

Function::FResult AddReferencedLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{return AddPrimitiveLocalNode(GraphPath,VariableName,ExpectedLocalGuid,X,Y,false,false,true);}
Function::FResult AddReferencedLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{return AddPrimitiveLocalNode(GraphPath,VariableName,ExpectedLocalGuid,X,Y,true,false,true);}

Function::FResult AddPrimitiveArrayLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{return AddPrimitiveLocalNode(GraphPath,VariableName,ExpectedLocalGuid,X,Y,false,false,false,true);}
Function::FResult AddPrimitiveArrayLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{return AddPrimitiveLocalNode(GraphPath,VariableName,ExpectedLocalGuid,X,Y,true,false,false,true);}

TSharedRef<FJsonObject> ReadMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter)
{
    auto R = Result();
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())
    {Fail(R,TEXT("graph_member_editor_state_invalid"),TEXT("Member planning requires the idle editor game thread before the first loaded-graph lookup."));return R;}
    UEdGraph* Graph = ResolveLoadedGraph(GraphPath, R); if (!Graph) { return R; }
    if (!SafeSelector(VariableName))
    { Fail(R, TEXT("variable_name_invalid"), TEXT("Use one exact bounded member name.")); return R; }
    UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    // Only an actual selected own local Map descriptor selects this new route.
    if(BP&&BP->FunctionGraphs.Contains(Graph))for(const auto& H:Graph->Nodes)
    {
        if(!H.IsResolved())break;
        UEdGraphNode* N=H.Get();if(N&&N->GetClass()==UK2Node_FunctionEntry::StaticClass())
        {const auto* D=static_cast<UK2Node_FunctionEntry*>(N)->LocalVariables.FindByPredicate([&](const auto& X){return X.VarName==FName(*VariableName);});if(D&&FLocalDoubleMap::Type(D->VarType))return ReadLocalDoubleMapPlan(Graph,VariableName,bSetter,R);if(D&&FLocalDoubleMap::PrimitiveTypeClass(D->VarType))return ReadPrimitiveLocalPlan(Graph,VariableName,bSetter,R);if(D&&(FLocalDoubleMap::NativeValueType(D->VarType)||FLocalDoubleMap::ScalarUdsType(D->VarType)))return ReadNativeValueLocalPlan(Graph,VariableName,bSetter,R);if(D&&FLocalDoubleMap::ReferencedTypeClass(D->VarType))return ReadReferencedLocalPlan(Graph,VariableName,bSetter,R);if(D&&FLocalDoubleMap::PrimitiveArrayTypeClass(D->VarType))return ReadPrimitiveArrayLocalPlan(Graph,VariableName,bSetter,R);}
    }
    const int32 Index = FBlueprintEditorUtils::FindNewVariableIndex(BP, FName(*VariableName));
    // LP dispatch precedes the legacy TSubclassOf reads and uses only copied own descriptor selectors.
    if(BP->NewVariables.IsValidIndex(Index))
    {
        const auto Type=BP->NewVariables[Index].VarType;
        if(Type.IsMap()&&Type.PinCategory==UEdGraphSchema_K2::PC_String&&Type.PinValueType.TerminalCategory==UEdGraphSchema_K2::PC_Real&&Type.PinValueType.TerminalSubCategory==UEdGraphSchema_K2::PC_Double&&Type.PinValueType.TerminalSubCategoryObject==nullptr)
            return ReadNativeVectorArrayPlan(Graph,VariableName,bSetter,nullptr,R,true,true);
    if(Type.IsMap()&&Type.PinCategory==UEdGraphSchema_K2::PC_String&&Type.PinValueType.TerminalCategory==UEdGraphSchema_K2::PC_Struct&&Type.PinValueType.TerminalSubCategoryObject.Get()==TBaseStructure<FVector>::Get())
            return ReadNativeVectorArrayPlan(Graph,VariableName,bSetter,nullptr,R,true);
        if(Type.PinCategory==UEdGraphSchema_K2::PC_Object&&Type.IsArray()&&Type.PinSubCategoryObject.Get()==USkeletalMeshComponent::StaticClass())
            return ReadOwnNativeObjectArrayPlan(Graph,VariableName,bSetter,R,true);
    }
    if (!BP->SkeletonGeneratedClass || (Index == INDEX_NONE && bSetter))
    { Fail(R, TEXT("graph_member_not_declared"), TEXT("Setters require an own declared member; getters also support an audited generated component or exact native SplineComponent/InstancedStaticMeshComponent in the reachable local SCS. A current skeleton is required.")); return R; }
    const FProperty* InheritedProperty = Index == INDEX_NONE && !bSetter
        ? BP->SkeletonGeneratedClass->FindPropertyByName(FName(*VariableName)) : nullptr;
    UClass* DeclaringClass = InheritedProperty ? InheritedProperty->GetOwnerClass() : nullptr;
    UBlueprint* DeclaringBP = DeclaringClass && DeclaringClass != BP->SkeletonGeneratedClass
        ? Cast<UBlueprint>(DeclaringClass->ClassGeneratedBy) : nullptr;
    const bool bInheritedGetter = DeclaringBP && DeclaringBP != BP;
    const bool bComponentGetter = Index == INDEX_NONE && !bInheritedGetter;
    const FBPVariableDescription* Description = Index != INDEX_NONE ? &BP->NewVariables[Index] : nullptr;
    if(Description&&!Description->VarType.IsContainer()&&Description->VarType.PinCategory==UEdGraphSchema_K2::PC_Object
        &&Description->VarType.PinSubCategoryObject.Get()==UTextureRenderTarget2D::StaticClass())
    {return ReadOwnNativeMaterialValuePlan(Graph,VariableName,bSetter,R,false,true);}
    if(Description&&!Description->VarType.IsContainer()&&Description->VarType.PinCategory==UEdGraphSchema_K2::PC_Object
        &&Description->VarType.PinSubCategoryObject.Get()==UNiagaraSystem::StaticClass())
    {return ReadOwnNativeMaterialValuePlan(Graph,VariableName,bSetter,R,true);}
    if(Description&&!Description->VarType.IsContainer())
    {
        UObject* T=Description->VarType.PinSubCategoryObject.Get();
        if((Description->VarType.PinCategory==UEdGraphSchema_K2::PC_Object&&(T==UMaterialInterface::StaticClass()||T==UMaterialInstanceDynamic::StaticClass()||T==UTexture::StaticClass()))
            ||(Description->VarType.PinCategory==UEdGraphSchema_K2::PC_Struct&&T==TBaseStructure<FLinearColor>::Get()))
        {return ReadOwnNativeMaterialValuePlan(Graph,VariableName,bSetter,R);}
    }
    if(Description&&Description->VarType.PinCategory==UEdGraphSchema_K2::PC_Struct&&Description->VarType.IsArray()
        &&Description->VarType.PinSubCategoryObject.Get()==TBaseStructure<FVector4>::Get())
    {return ReadNativeVectorArrayPlan(Graph,VariableName,bSetter,nullptr,R);}
    if(Description&&Description->VarType.PinCategory==UEdGraphSchema_K2::PC_Object&&Description->VarType.IsArray())
    {return ReadOwnNativeObjectArrayPlan(Graph,VariableName,bSetter,R);}
    USCS_Node* ComponentNode = nullptr;
    bool bNativeSplineGetter = false, bNativeIsmGetter = false;
    TSharedPtr<FJsonObject> NativeSplineBefore, NativeIsmBefore;
    FName MemberName(*VariableName);
    if (bInheritedGetter)
    {
        const auto Inspection = GeneratedComponent::InspectSavedActorParent(BP->ParentClass.Get());
        R->SetObjectField(TEXT("declaring_parent_validation"), Inspection);
        const int32 ParentIndex = FBlueprintEditorUtils::FindNewVariableIndex(DeclaringBP, MemberName);
        if (!Inspection->GetBoolField(TEXT("success")) || !DeclaringBP->GeneratedClass || !BP->ParentClass
            || !BP->ParentClass->IsChildOf(DeclaringBP->GeneratedClass) || ParentIndex == INDEX_NONE
            || (DeclaringClass != DeclaringBP->GeneratedClass && DeclaringClass != DeclaringBP->SkeletonGeneratedClass))
        { Fail(R, TEXT("graph_inherited_member_unsupported"), TEXT("Inherited getter requires an exact declared variable on the saved clean compiled Actor Blueprint parent chain.")); return R; }
        Description = &DeclaringBP->NewVariables[ParentIndex];
        const FProperty* CompiledProperty = DeclaringBP->GeneratedClass->FindPropertyByName(MemberName);
        FEdGraphPinType CompiledType;
        if (Description->VarName.ToString() != VariableName || !Description->VarGuid.IsValid()
            || Description->VarType.PinCategory != UEdGraphSchema_K2::PC_Boolean || Description->VarType.IsContainer()
            || Description->VarType.bIsReference || !CompiledProperty || CompiledProperty->GetOwnerClass() != DeclaringBP->GeneratedClass
            || !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(CompiledProperty, CompiledType) || CompiledType != Description->VarType
            || FBlueprintEditorUtils::IsPropertyPrivate(InheritedProperty)
            || FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP, InheritedProperty) != FBlueprintEditorUtils::EPropertyReadableState::Readable)
        { Fail(R, TEXT("graph_inherited_member_type_pending"), TEXT("This slice admits readable scalar Boolean declarations only; inherited native/SCS/object/other-type getters and all inherited setters remain unsupported.")); return R; }
        MemberName = Description->VarName;
        R->SetStringField(TEXT("member_scope"), TEXT("inherited_actor_boolean"));
        R->SetStringField(TEXT("declaring_blueprint_path"), DeclaringBP->GetPathName());
        R->SetStringField(TEXT("declaring_class_path"), DeclaringBP->GeneratedClass->GetPathName());
    }
    else if (bComponentGetter)
    {
        Component::FReadResult Components; FString Code, Error;
        if (!Component::ReadLocalComponents(BP->GetPathName(), VariableName, FString(), false, 10000, 65536, Components, Code, Error))
        { R->SetObjectField(TEXT("local_component_observation"), Components.Data); Fail(R, TEXT("graph_member_not_declared"), TEXT("No unambiguous reachable local SCS component matched the member; inherited and external members remain unsupported.")); return R; }
        const TWeakObjectPtr<USCS_Node>* Found = Components.LocalNodes.Find(VariableName);
        ComponentNode = Found ? Found->Get() : nullptr;
        if (!IsValid(ComponentNode) || !ComponentNode->VariableGuid.IsValid() || Components.bLoadedDuringRead
            || !Components.bDirtyObserved || !Components.bDirtyAfterObserved
            || Components.bDirtyBefore != Components.bDirtyAfter || Components.StatusBefore != Components.StatusAfter)
        { Fail(R, TEXT("graph_component_observation_invalid"), TEXT("Local component identity/read-only baseline was not preserved; no getter was spawned.")); return R; }
        bNativeSplineGetter = ComponentNode->ComponentClass.Get() == USplineComponent::StaticClass();
        bNativeIsmGetter = ComponentNode->ComponentClass.Get() == UInstancedStaticMeshComponent::StaticClass();
        const auto Inspection = bNativeSplineGetter ? NativeSplineSourceObservation(ComponentNode->ComponentClass.Get())
            : bNativeIsmGetter ? NativeIsmSourceObservation(ComponentNode->ComponentClass.Get())
            : GeneratedComponent::InspectSavedClass(ComponentNode->ComponentClass.Get());
        R->SetObjectField(TEXT("component_class_validation"), Inspection);
        if (!Inspection->GetBoolField(TEXT("success")))
        { Fail(R, TEXT("graph_component_class_unsupported"), TEXT("Local component class failed the saved generated ActorComponent or exact loaded native SplineComponent/InstancedStaticMeshComponent inspection.")); return R; }
        if (bNativeSplineGetter)
        {
            NativeSplineBefore = Inspection;
            R->SetObjectField(TEXT("native_spline_source_before"), Inspection);
            if (!IsValid(ComponentNode->ComponentTemplate.Get()) || ComponentNode->ComponentTemplate->GetClass() != ComponentNode->ComponentClass.Get()
                || ComponentNode->GetOuter() != BP->SimpleConstructionScript.Get()
                || !BP->GeneratedClass || ComponentNode->ComponentTemplate->GetOuter() != BP->GeneratedClass.Get())
            { Fail(R, TEXT("graph_native_spline_template_mismatch"), TEXT("Local SCS Spline template must have the exact native class; no getter was spawned.")); return R; }
            R->SetStringField(TEXT("component_template_path"), ComponentNode->ComponentTemplate->GetPathName());
            R->SetStringField(TEXT("component_template_class_path"), ComponentNode->ComponentTemplate->GetClass()->GetPathName());
            R->SetStringField(TEXT("component_template_outer_path"), ComponentNode->ComponentTemplate->GetOuter()->GetPathName());
            R->SetStringField(TEXT("scs_outer_path"), ComponentNode->GetOuter()->GetPathName());
            R->SetBoolField(TEXT("native_spline_local_template_identity_verified"), true);
        }
        if (bNativeIsmGetter)
        {
            NativeIsmBefore = Inspection;
            R->SetObjectField(TEXT("native_ism_source_before"), Inspection);
            if (!IsValid(ComponentNode->ComponentTemplate.Get()) || ComponentNode->ComponentTemplate->GetClass() != ComponentNode->ComponentClass.Get()
                || ComponentNode->GetOuter() != BP->SimpleConstructionScript.Get()
                || !BP->GeneratedClass || ComponentNode->ComponentTemplate->GetOuter() != BP->GeneratedClass.Get())
            { Fail(R, TEXT("graph_native_ism_template_mismatch"), TEXT("Local SCS ISM template must have the exact native class; no getter was spawned.")); return R; }
            R->SetStringField(TEXT("component_template_path"), ComponentNode->ComponentTemplate->GetPathName());
            R->SetStringField(TEXT("component_template_class_path"), ComponentNode->ComponentTemplate->GetClass()->GetPathName());
            R->SetStringField(TEXT("component_template_outer_path"), ComponentNode->ComponentTemplate->GetOuter()->GetPathName());
            R->SetStringField(TEXT("scs_outer_path"), ComponentNode->GetOuter()->GetPathName());
            R->SetBoolField(TEXT("native_ism_local_template_identity_verified"), true);
        }
        MemberName = ComponentNode->GetVariableName();
        R->SetStringField(TEXT("member_scope"), bNativeSplineGetter ? TEXT("local_scs_native_spline_component") : bNativeIsmGetter ? TEXT("local_scs_native_ism_component") : TEXT("local_scs_generated_component"));
        R->SetStringField(TEXT("scs_node_path"), ComponentNode->GetPathName());
        R->SetStringField(TEXT("scs_variable_guid"), ComponentNode->VariableGuid.ToString(EGuidFormats::Digits));
        R->SetBoolField(TEXT("scs_guid_equals_member_reference_guid_verified"), false);
        R->SetStringField(TEXT("component_class_path"), ComponentNode->ComponentClass->GetPathName());
    }
    else { MemberName = Description->VarName; }
    const FProperty* Property = BP->SkeletonGeneratedClass->FindPropertyByName(MemberName);
    FEdGraphPinType ActualType;
    if (!Property || (!bInheritedGetter && Property->GetOwnerClass() != BP->SkeletonGeneratedClass) ||
        !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, ActualType)
        || (!bComponentGetter && ActualType != Description->VarType))
    { Fail(R, TEXT("graph_member_skeleton_mismatch"), TEXT("Declared member and skeleton property do not agree; no node was spawned or implicit compile requested.")); return R; }
    if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) || (bSetter && Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly)))
    { Fail(R, TEXT("graph_member_access_rejected"), TEXT("The member is not Blueprint-visible or is read-only for a setter.")); return R; }
    // Local SCS generated/native Spline/ISM components and audited native object declarations
    // are separate from arbitrary external generated/object member types.
    const FName Category = ActualType.PinCategory;
    const bool Primitive = Category == UEdGraphSchema_K2::PC_Boolean || Category == UEdGraphSchema_K2::PC_Int ||
        Category == UEdGraphSchema_K2::PC_Int64 || Category == UEdGraphSchema_K2::PC_Real ||
        Category == UEdGraphSchema_K2::PC_String || Category == UEdGraphSchema_K2::PC_Name || Category == UEdGraphSchema_K2::PC_Text;
    const UObject* TypeObject = ActualType.PinSubCategoryObject.Get();
    const bool BuiltinStruct = Category == UEdGraphSchema_K2::PC_Struct &&
        (TypeObject == TBaseStructure<FVector>::Get() || TypeObject == TBaseStructure<FRotator>::Get() || TypeObject == TBaseStructure<FTransform>::Get());
    // One own native value type for the original seeded-offset algorithm. This
    // does not admit arbitrary structs, inherited members or RandomStream arrays.
    const bool RandomStreamRequested = TypeObject && TypeObject->GetPathName() == TEXT("/Script/CoreUObject.RandomStream");
    TSharedRef<FJsonObject> StreamBefore = MakeShared<FJsonObject>();
    bool OwnRandomStream = false;
    if (RandomStreamRequested)
    {
        StreamBefore = RandomStreamSourceObservation(); R->SetObjectField(TEXT("native_random_stream_source_before"), StreamBefore);
        const FStructProperty* Stream = CastField<FStructProperty>(Property);
        OwnRandomStream = !bComponentGetter && !bInheritedGetter && Description && Description->VarGuid.IsValid()
            && Category == UEdGraphSchema_K2::PC_Struct && ActualType.PinSubCategory.IsNone()
            && ActualType.ContainerType == EPinContainerType::None && !ActualType.bIsReference && !ActualType.bIsConst
            && !ActualType.bIsWeakPointer && !ActualType.bIsUObjectWrapper && !ActualType.bSerializeAsSinglePrecisionFloat
            && Stream && Stream->Struct == TypeObject && Stream->GetCPPType(nullptr, 0) == TEXT("FRandomStream") && Stream->ArrayDim == 1
            && StreamBefore->GetBoolField(TEXT("observations_complete")) && StreamBefore->GetBoolField(TEXT("native_identity_verified"));
        if (!OwnRandomStream)
        { Fail(R, TEXT("graph_random_stream_member_type_unsupported"), TEXT("RandomStream access requires one own current Blueprint-visible scalar member of the exact loaded runtime CoreUObject.RandomStream; arrays, inherited/external contexts and other structs remain unsupported.")); return R; }
        R->SetStringField(TEXT("member_scope"), TEXT("own_native_random_stream"));
        R->SetBoolField(TEXT("native_random_stream_struct_identity_verified"), true);
        R->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
        R->SetBoolField(TEXT("native_random_stream_runtime_invocation_verified"), false);
    }
    // A separate own Class getter slice. The object-scalar whitelist remains
    // unchanged: Class/container/internal-function admission is never inferred.
    const bool OwnClassRequested = Category == UEdGraphSchema_K2::PC_Class;
    bool OwnClassGetter = false;
    TSharedPtr<FJsonObject> ClassSourceBefore;
    FString ClassStoredDefaultBefore;
    if (OwnClassRequested)
    {
        const FClassProperty* ClassProperty = CastField<FClassProperty>(Property);
        ClassStoredDefaultBefore = Description ? Description->DefaultValue : FString();
        ClassSourceBefore = NativeClassConstraintObservation(ActualType);
        R->SetObjectField(TEXT("native_class_constraint_before"), ClassSourceBefore);
        OwnClassGetter = !bSetter && !bComponentGetter && !bInheritedGetter && Description && Description->VarGuid.IsValid()
            && Description->VarName == MemberName && Property->GetOwnerClass() == BP->SkeletonGeneratedClass
            && ClassProperty && ClassProperty->ArrayDim == 1 && ClassProperty->MetaClass.Get() == TypeObject
            && ClassProperty->PropertyClass.Get() == UClass::StaticClass()
            && RuntimeObject::IsSupportedClassPin(ActualType)
            && ActualType.bIsUObjectWrapper == Property->HasAllPropertyFlags(CPF_UObjectWrapper)
            && !Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Parm)
            && (ClassStoredDefaultBefore.IsEmpty() || ClassStoredDefaultBefore == TEXT("None"))
            && ClassSourceBefore->GetBoolField(TEXT("observations_complete"))
            && ClassSourceBefore->GetBoolField(TEXT("native_identity_verified"));
        if (!OwnClassGetter)
        { Fail(R, TEXT("graph_own_class_getter_unsupported"), TEXT("Class access requires one own current Blueprint-visible scalar Class getter constrained to exact loaded native Engine.Actor or Engine.ActorComponent with a null stored default; setters, arrays, inherited/external/custom constraints and concrete class defaults remain unsupported.")); return R; }
        R->SetStringField(TEXT("member_scope"), TEXT("own_native_class"));
        R->SetBoolField(TEXT("native_class_constraint_identity_verified"), true);
        R->SetStringField(TEXT("class_constraint_path"), TypeObject->GetPathName());
        R->SetStringField(TEXT("skeleton_class_path"), BP->SkeletonGeneratedClass->GetPathName());
        R->SetStringField(TEXT("property_owner_class_path"), Property->GetOwnerClass()->GetPathName());
        R->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
        R->SetNumberField(TEXT("property_array_dim"), Property->ArrayDim);
        R->SetObjectField(TEXT("pin_type"), PinTypeObservation(ActualType));
        R->SetBoolField(TEXT("class_property_uobject_wrapper_matches"), true);
        R->SetStringField(TEXT("stored_default_before"), ClassStoredDefaultBefore);
        R->SetBoolField(TEXT("class_default_write_attempted"), false);
        R->SetBoolField(TEXT("native_class_runtime_evaluation_verified"), false);
    }
    const FObjectProperty* ComponentProperty = bComponentGetter ? CastField<FObjectProperty>(Property) : nullptr;
    const bool AuditedComponent = bComponentGetter && ComponentProperty && ComponentProperty->PropertyClass == ComponentNode->ComponentClass.Get()
        && Category == UEdGraphSchema_K2::PC_Object && TypeObject == ComponentNode->ComponentClass.Get()
        && ActualType.ContainerType == EPinContainerType::None && !ActualType.bIsReference;
    if (bNativeSplineGetter)
    {
        const bool ExactSplineProperty = AuditedComponent && ComponentProperty->ArrayDim == 1
            && ActualType.PinSubCategory.IsNone() && !ActualType.bIsConst && !ActualType.bIsWeakPointer
            && !ActualType.bIsUObjectWrapper && !ActualType.bSerializeAsSinglePrecisionFloat
            && !Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Parm);
        R->SetBoolField(TEXT("native_spline_skeleton_property_verified"), ExactSplineProperty);
        R->SetStringField(TEXT("skeleton_class_path"), BP->SkeletonGeneratedClass->GetPathName());
        R->SetStringField(TEXT("property_owner_class_path"), Property->GetOwnerClass()->GetPathName());
        R->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
        R->SetNumberField(TEXT("property_array_dim"), Property->ArrayDim);
        R->SetObjectField(TEXT("pin_type"), PinTypeObservation(ActualType));
        if (!ExactSplineProperty)
        { Fail(R, TEXT("graph_native_spline_property_unsupported"), TEXT("Local Spline getter requires one own Blueprint-visible scalar object property of the exact native class, without reference/const/weak/editor-only/parameter flags; no node was spawned.")); return R; }
    }
    if (bNativeIsmGetter)
    {
        const bool ExactIsmProperty = AuditedComponent && ComponentProperty->ArrayDim == 1
            && ActualType.PinSubCategory.IsNone() && !ActualType.bIsConst && !ActualType.bIsWeakPointer
            && !ActualType.bIsUObjectWrapper && !ActualType.bSerializeAsSinglePrecisionFloat
            && !Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Parm);
        R->SetBoolField(TEXT("native_ism_skeleton_property_verified"), ExactIsmProperty);
        R->SetStringField(TEXT("skeleton_class_path"), BP->SkeletonGeneratedClass->GetPathName());
        R->SetStringField(TEXT("property_owner_class_path"), Property->GetOwnerClass()->GetPathName());
        R->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
        R->SetNumberField(TEXT("property_array_dim"), Property->ArrayDim);
        R->SetObjectField(TEXT("pin_type"), PinTypeObservation(ActualType));
        if (!ExactIsmProperty)
        { Fail(R, TEXT("graph_native_ism_property_unsupported"), TEXT("Local ISM getter requires one own Blueprint-visible scalar object property of the exact native class, without reference/const/weak/editor-only/parameter flags; no node was spawned.")); return R; }
    }
    const bool AuditedObject = RuntimeObject::IsSupportedPin(ActualType);
    if (AuditedObject && !bComponentGetter && !bInheritedGetter) { R->SetStringField(TEXT("member_scope"), TEXT("own_runtime_object")); }
    if (bComponentGetter ? !AuditedComponent : ((!Primitive && !BuiltinStruct && !AuditedObject && !OwnRandomStream && !OwnClassGetter) || ActualType.IsMap() || ActualType.IsSet() || (Primitive && TypeObject)))
    { Fail(R, TEXT("graph_member_type_pending"), TEXT("Supports own scalar/array primitive and Vector/Rotator/Transform members, exact own scalar CoreUObject.RandomStream, and scalar audited native runtime object references; other objects, class, enum, user-struct, set/map and delegate members remain pending.")); return R; }
    auto Parent = InspectBlueprintParent(BP->ParentClass ? BP->ParentClass->GetPathName() : FString());
    bool ParentOK = false;
    if (!Parent->TryGetBoolField(TEXT("success"), ParentOK) || !ParentOK)
    { R->SetObjectField(TEXT("parent_inspection"), Parent); Fail(R, TEXT("graph_member_parent_unsupported"), TEXT("Blueprint parent chain failed the existing runtime-class guard.")); return R; }
    if (OwnClassGetter)
    {
        const auto ClassSourceAfter = NativeClassConstraintObservation(ActualType);
        const bool Preserved = FJsonValue::CompareEqual(FJsonValueObject(ClassSourceBefore), FJsonValueObject(ClassSourceAfter));
        R->SetObjectField(TEXT("native_class_constraint_after"), ClassSourceAfter);
        R->SetBoolField(TEXT("native_class_constraint_observed_unchanged"), Preserved);
        R->SetStringField(TEXT("stored_default_after"), Description->DefaultValue);
        R->SetBoolField(TEXT("stored_default_observed_unchanged"), Description->DefaultValue == ClassStoredDefaultBefore);
        if (!Preserved || !ClassSourceAfter->GetBoolField(TEXT("native_identity_verified")) || Description->DefaultValue != ClassStoredDefaultBefore)
        { Fail(R, TEXT("graph_own_class_source_changed"), TEXT("The exact loaded Class constraint metadata or own stored default changed during read-only planning; no getter was spawned.")); return R; }
    }
    if (OwnRandomStream)
    {
        const auto StreamAfter = RandomStreamSourceObservation();
        const bool Preserved = SameRandomStreamSource(StreamBefore, StreamAfter);
        R->SetObjectField(TEXT("native_random_stream_source_after"), StreamAfter);
        R->SetBoolField(TEXT("native_random_stream_source_observed_unchanged"), Preserved);
        if (!Preserved || !StreamAfter->GetBoolField(TEXT("native_identity_verified")))
        { Fail(R, TEXT("graph_random_stream_source_changed"), TEXT("The exact loaded native RandomStream source changed during read-only member planning; no getter/setter node was spawned.")); return R; }
    }
    if (bNativeSplineGetter)
    {
        const auto NativeSplineAfter = NativeSplineSourceObservation(ComponentNode->ComponentClass.Get());
        const bool Preserved = FJsonValue::CompareEqual(FJsonValueObject(NativeSplineBefore), FJsonValueObject(NativeSplineAfter));
        R->SetObjectField(TEXT("native_spline_source_after"), NativeSplineAfter);
        R->SetBoolField(TEXT("native_spline_source_observed_unchanged"), Preserved);
        R->SetBoolField(TEXT("native_spline_runtime_evaluation_verified"), false);
        if (!Preserved || !NativeSplineAfter->GetBoolField(TEXT("native_identity_verified")))
        { Fail(R, TEXT("graph_native_spline_source_changed"), TEXT("Exact native Spline class/package/CDO metadata changed during read-only member planning; no getter was spawned.")); return R; }
    }
    if (bNativeIsmGetter)
    {
        const auto NativeIsmAfter = NativeIsmSourceObservation(ComponentNode->ComponentClass.Get());
        const bool Preserved = FJsonValue::CompareEqual(FJsonValueObject(NativeIsmBefore), FJsonValueObject(NativeIsmAfter));
        R->SetObjectField(TEXT("native_ism_source_after"), NativeIsmAfter);
        R->SetBoolField(TEXT("native_ism_source_observed_unchanged"), Preserved);
        R->SetBoolField(TEXT("native_ism_runtime_instance_operations_verified"), false);
        if (!Preserved || !NativeIsmAfter->GetBoolField(TEXT("native_identity_verified")))
        { Fail(R, TEXT("graph_native_ism_source_changed"), TEXT("Exact native ISM class/package/CDO metadata changed during read-only member planning; no getter was spawned.")); return R; }
    }
    R->SetStringField(TEXT("variable_name"), MemberName.ToString());
    if (!bComponentGetter) { R->SetStringField(TEXT("variable_guid"), Description->VarGuid.ToString(EGuidFormats::Digits)); }
    R->SetStringField(TEXT("property_path"), Property->GetPathName());
    R->SetStringField(TEXT("pin_category"), Category.ToString());
    R->SetStringField(TEXT("pin_subcategory"), ActualType.PinSubCategory.ToString());
    R->SetBoolField(TEXT("setter"), bSetter);
    FString TypeText; FEdGraphPinType::StaticStruct()->ExportText(TypeText, &ActualType, &ActualType, nullptr, PPF_None, nullptr);
    R->SetStringField(TEXT("expected_pin_type_text"), TypeText);
    R->SetStringField(TEXT("scope"), bInheritedGetter
        ? TEXT("saved generated Actor ancestor's readable scalar Boolean getter only; no inherited setters, native/SCS/object members or load/compile/save") : bComponentGetter
        ? (bNativeSplineGetter ? TEXT("reachable local SCS exact native Engine.SplineComponent getter; own skeleton object property/type/name and clean native metadata; SCS GUID observed separately; no load/compile/save")
            : bNativeIsmGetter ? TEXT("reachable local SCS exact native Engine.InstancedStaticMeshComponent getter; own skeleton object property/type/name and clean native metadata; SCS GUID observed separately; no load/compile/save")
            : TEXT("reachable local SCS generated non-scene ActorComponent getter; exact skeleton property/type/name, SCS GUID observed separately; no load/compile/save"))
        : OwnClassGetter ? TEXT("own current scalar Class getter constrained to exact loaded native Engine.Actor or Engine.ActorComponent; null stored default preserved; no class load/default write/compile/save or runtime evaluation")
        : TEXT("own declared runtime scalar/array member only; no compile/save or external member resolution"));
    Complete(R, Graph); return R;
}

namespace { TSharedRef<FJsonObject> QXExternalActorPlan(UEdGraph*,const FString&,bool,UClass*,const TSharedRef<FJsonObject>&); }
TSharedRef<FJsonObject> ReadExternalMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath)
{
    auto R = Result();
    if((TargetClassPath==TEXT("/Script/Engine.ChildActorComponent")||TargetClassPath==TEXT("/Script/Engine.Texture")||TargetClassPath==TEXT("/Script/Engine.TextureRenderTarget2D")||TargetClassPath.StartsWith(TEXT("/Game/")))&&
        (!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()))
    {Fail(R,TEXT("graph_editor_state_invalid"),TEXT("Native CAC planning refuses import/exit/purge/GC/saving before the first loaded-graph lookup."));return R;}
    UEdGraph* G = ResolveLoadedGraph(GraphPath, R); if (!G) { return R; }
    if(TargetClassPath==TEXT("/Script/Engine.Texture")||TargetClassPath==TEXT("/Script/Engine.TextureRenderTarget2D"))return ReadNativeTextureMemberPlan(G,VariableName,bSetter,TargetClassPath,R);
    if (TargetClassPath == TEXT("/Script/Engine.Pawn")) { return ReadNativePawnMemberPlan(G, VariableName, bSetter, R); }
    if (TargetClassPath == TEXT("/Script/Engine.ChildActorComponent")) { return ReadNativeChildActorMemberPlan(G, VariableName, bSetter, R); }
    if (!SafeSelector(VariableName) || !SafeSelector(TargetClassPath) || !TargetClassPath.StartsWith(TEXT("/Game/")))
    { Fail(R, TEXT("graph_external_member_selector_invalid"), TEXT("Requires an exact bounded member and loaded /Game generated class path.")); return R; }
    UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(G);
    UClass* Class = FindObject<UClass>(nullptr, *TargetClassPath);
    // QX direct Actor source only. Other/native/component/own paths stay below unchanged.
    if(IsValid(Class)&&!Class->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)&&Class->ClassGeneratedBy.IsResolved())
    {
        UObject* GeneratedBy=Class->ClassGeneratedBy.Get();TStrongObjectPtr<UObject> HeldGeneratedBy(GeneratedBy);
        if(IsValid(GeneratedBy)&&!GeneratedBy->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)&&GeneratedBy->GetClass()==UBlueprint::StaticClass())
        {
            auto* ActorSource=static_cast<UBlueprint*>(GeneratedBy);
            if(ActorSource->ParentClass.GetGCPtr().IsResolved()&&ActorSource->ParentClass.GetGCPtr().Get()==AActor::StaticClass())
                return QXExternalActorPlan(G,VariableName,bSetter,Class,R);
        }
    }

    // Select only the new finite array scope before the legacy external observer.
    // A shallow resolved handle and lookup-only path avoid tracked source reads here.
    if(IsValid(Class)&&Class->ClassGeneratedBy.IsResolved()&&Class->ClassGeneratedBy!=nullptr)
    {
        const auto H=Class->ClassGeneratedBy;UBlueprint* Candidate=FindObject<UBlueprint>(nullptr,*H.GetPathName());
        if(IsValid(Candidate)&&H==Candidate)
        {
            const int32 Slot=FBlueprintEditorUtils::FindNewVariableIndex(Candidate,FName(*VariableName));
            if(Slot!=INDEX_NONE)
            {
                const auto Type=Candidate->NewVariables[Slot].VarType;const UObject* Element=Type.PinSubCategoryObject.Get();
                if(Type.PinCategory==UEdGraphSchema_K2::PC_Struct&&Type.IsArray()&&(Element==TBaseStructure<FVector>::Get()||Element==TBaseStructure<FVector4>::Get()))
                    return ReadNativeVectorArrayPlan(G,VariableName,bSetter,Class,R);
            }
        }
    }
    UBlueprint* Source = IsValid(Class) ? Cast<UBlueprint>(Class->ClassGeneratedBy) : nullptr;
    if (!IsValid(Source) || Source == BP || !BP->SkeletonGeneratedClass || BP->SkeletonGeneratedClass->IsChildOf(Class)
        || BP->BlueprintType != BPTYPE_Normal || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bIsRegeneratingOnLoad
        || BP->bSuppressStructurallyModified || G->GetSchema()->GetClass() != UEdGraphSchema_K2::StaticClass())
    { Fail(R, TEXT("graph_external_member_scope_unsupported"), TEXT("Requires a distinct already-loaded component class and idle standard K2 target; inherited/self contexts use the own member route.")); return R; }
    const bool SourceDirty = Source->GetOutermost()->IsDirty(); const EBlueprintStatus SourceStatus = Source->Status.GetValue();
    auto SourceState = [&]()
    {
        auto State = MakeShared<FJsonObject>(); State->SetStringField(TEXT("blueprint_path"), Source->GetPathName());
        State->SetBoolField(TEXT("dirty"), Source->GetOutermost()->IsDirty()); State->SetNumberField(TEXT("status"), Source->Status.GetValue());
        State->SetStringField(TEXT("generated_class"), Source->GeneratedClass ? Source->GeneratedClass->GetPathName() : FString());
        State->SetStringField(TEXT("skeleton_class"), Source->SkeletonGeneratedClass ? Source->SkeletonGeneratedClass->GetPathName() : FString());
        State->SetNumberField(TEXT("declared_variable_count"), Source->NewVariables.Num()); return State;
    };
    R->SetObjectField(TEXT("external_source_before"), SourceState());
    const auto Inspection = GeneratedComponent::InspectSavedClass(Class); R->SetObjectField(TEXT("external_source_validation"), Inspection);
    R->SetObjectField(TEXT("external_source_after"), SourceState());
    const bool Unchanged = SourceDirty == Source->GetOutermost()->IsDirty() && SourceStatus == Source->Status && Source->GeneratedClass == Class;
    R->SetBoolField(TEXT("external_source_observed_unchanged"), Unchanged);
    if (!Inspection->GetBoolField(TEXT("success")) || !Unchanged || Class->GetPathName() != TargetClassPath || !Source->SkeletonGeneratedClass)
    { Fail(R, TEXT("graph_external_member_class_unsupported"), TEXT("External class must remain saved, clean, compiled and current under the shared non-scene runtime ActorComponent inspection.")); return R; }
    const FName Name(*VariableName); const int32 Index = FBlueprintEditorUtils::FindNewVariableIndex(Source, Name);

    const FProperty* Compiled = Class->FindPropertyByName(Name);
    const FProperty* Property = Source->SkeletonGeneratedClass->FindPropertyByName(Name);
    FEdGraphPinType Type, CompiledType;
    if (Index == INDEX_NONE || !Compiled || !Property || Compiled->GetOwnerClass() != Class
        || Property->GetOwnerClass() != Source->SkeletonGeneratedClass || !Source->NewVariables[Index].VarGuid.IsValid()
        || Source->NewVariables[Index].VarName.ToString() != VariableName
        || !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, Type)
        || !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Compiled, CompiledType)
        || Type != CompiledType || Type != Source->NewVariables[Index].VarType)
    { Fail(R, TEXT("graph_external_member_declaration_mismatch"), TEXT("External member must be own-declared with an exact matching GUID/name/current skeleton and compiled type.")); return R; }
    FEdGraphPinType ExpectedIntType; ExpectedIntType.PinCategory = UEdGraphSchema_K2::PC_Int;
    const auto ExactIntType = [&](const FEdGraphPinType& Value)
    {
        return Value == ExpectedIntType && !Value.bIsUObjectWrapper && !Value.bSerializeAsSinglePrecisionFloat
            && !Value.PinValueType.bTerminalIsUObjectWrapper;
    };
    const bool CallbackCountGetter = !bSetter && VariableName == TEXT("CallbackCount")
        && CastField<FIntProperty>(Compiled) && CastField<FIntProperty>(Property)
        && Compiled->ArrayDim == 1 && Property->ArrayDim == 1 && Compiled->GetCPPType() == TEXT("int32") && Property->GetCPPType() == TEXT("int32")
        && !Compiled->HasAnyPropertyFlags(CPF_Parm | CPF_OutParm | CPF_ReferenceParm | CPF_ConstParm | CPF_EditorOnly)
        && !Property->HasAnyPropertyFlags(CPF_Parm | CPF_OutParm | CPF_ReferenceParm | CPF_ConstParm | CPF_EditorOnly)
        && ExactIntType(Type) && ExactIntType(CompiledType) && ExactIntType(Source->NewVariables[Index].VarType);
    const bool AllowedType = !Type.IsContainer() && !Type.bIsReference && !Type.bIsWeakPointer
        && ((Type.PinCategory == UEdGraphSchema_K2::PC_Boolean && !Type.PinSubCategoryObject.IsValid())
            || (Type.PinCategory == UEdGraphSchema_K2::PC_Real && !Type.PinSubCategoryObject.IsValid()
                && (Type.PinSubCategory == UEdGraphSchema_K2::PC_Float || Type.PinSubCategory == UEdGraphSchema_K2::PC_Double))
            || (Type.PinCategory == UEdGraphSchema_K2::PC_Struct && Type.PinSubCategoryObject.Get() == TBaseStructure<FVector>::Get())
            || RuntimeObject::IsSupportedPin(Type) || CallbackCountGetter);
    if (!AllowedType)
    { Fail(R, TEXT("graph_external_member_type_pending"), TEXT("External own members support scalar Boolean/float/double/Vector and audited native runtime objects, plus only the exact scalar int CallbackCount getter; other int getters/setters and other types remain pending.")); return R; }
    auto Accessible = [&](const FProperty* P)
    {
        return !FBlueprintEditorUtils::IsPropertyPrivate(P)
            && FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP, P) == FBlueprintEditorUtils::EPropertyReadableState::Readable
            && (!bSetter || FBlueprintEditorUtils::IsPropertyWritableInBlueprint(BP, P) == FBlueprintEditorUtils::EPropertyWritableState::Writable);
    };
    if (!Accessible(Property) || !Accessible(Compiled))
    { Fail(R, TEXT("graph_member_access_rejected"), TEXT("The external property is private, not Blueprint-readable or not writable for this setter.")); return R; }
    const auto Parent = InspectBlueprintParent(BP->ParentClass ? BP->ParentClass->GetPathName() : FString());
    if (!Parent->GetBoolField(TEXT("success")))
    { R->SetObjectField(TEXT("parent_inspection"), Parent); Fail(R, TEXT("graph_member_parent_unsupported"), TEXT("Target parent failed the runtime-class inspection.")); return R; }
    FEdGraphPinType SelfType; SelfType.PinCategory = UEdGraphSchema_K2::PC_Object; SelfType.PinSubCategoryObject = Class;
    FString TypeText, SelfText;
    FEdGraphPinType::StaticStruct()->ExportText(TypeText, &Type, &Type, nullptr, PPF_None, nullptr);
    FEdGraphPinType::StaticStruct()->ExportText(SelfText, &SelfType, &SelfType, nullptr, PPF_None, nullptr);
    R->SetStringField(TEXT("member_scope"), TEXT("external_generated_component")); R->SetStringField(TEXT("variable_name"), VariableName);
    R->SetStringField(TEXT("variable_guid"), Source->NewVariables[Index].VarGuid.ToString(EGuidFormats::Digits)); R->SetBoolField(TEXT("setter"), bSetter);
    R->SetStringField(TEXT("target_class_path"), Class->GetPathName()); R->SetStringField(TEXT("declaring_class_path"), Class->GetPathName());
    R->SetStringField(TEXT("declaring_blueprint_path"), Source->GetPathName()); R->SetStringField(TEXT("property_path"), Property->GetPathName());
    R->SetStringField(TEXT("compiled_property_path"), Compiled->GetPathName()); R->SetStringField(TEXT("pin_category"), Type.PinCategory.ToString());
    R->SetStringField(TEXT("pin_subcategory"), Type.PinSubCategory.ToString()); R->SetStringField(TEXT("expected_pin_type_text"), TypeText);
    R->SetStringField(TEXT("expected_self_pin_type_text"), SelfText); R->SetObjectField(TEXT("external_source_after"), SourceState());
    if (CallbackCountGetter)
    {
        R->SetBoolField(TEXT("callback_count_int_getter_verified"), true);
        R->SetStringField(TEXT("callback_count_property_class"), Property->GetClass()->GetName());
        R->SetStringField(TEXT("callback_count_compiled_property_class"), Compiled->GetClass()->GetName());
        R->SetStringField(TEXT("callback_count_property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
        R->SetStringField(TEXT("callback_count_compiled_property_flags_string"), LexToString(static_cast<uint64>(Compiled->GetPropertyFlags())));
    }
    R->SetStringField(TEXT("scope"), TEXT("loaded saved non-scene ActorComponent own declared readable/writable member; no load, spawn, compile or save"));
    Complete(R, G); return R;
}


// Read-only exact String-double/Vector Map8 thunks and own assignment.
// Each invocation reacquires physical pins; no pin from a connection callback is reused.
TSharedRef<FJsonObject> ReadStringVectorMapConnection(UEdGraph* Graph,UK2Node* Source,UK2Node* Target,const FString& SourcePinName,const FString& TargetPinName,const TSharedRef<FJsonObject>& R)
{
    // Infer only from a real current Map endpoint; an unlinked thunk request is not typed proof.
    UK2Node_CallFunction* ArraySourceCall=Source&&Source->GetClass()==UK2Node_CallFunction::StaticClass()?static_cast<UK2Node_CallFunction*>(Source):nullptr;
    const FName SourceFunction=ArraySourceCall?ArraySourceCall->FunctionReference.GetMemberName():NAME_None;
    const bool ArraySource=(SourceFunction==TEXT("Map_Keys")&&FName(*SourcePinName)==TEXT("Keys"))||(SourceFunction==TEXT("Map_Values")&&FName(*SourcePinName)==TEXT("Values"));
    const bool LengthSource=SourceFunction==TEXT("Map_Length")&&FName(*SourcePinName)==TEXT("ReturnValue"),ThunkSource=ArraySource||LengthSource;
    const UEdGraphPin* SourceMap=ThunkSource?ArraySourceCall->FindPin(TEXT("TargetMap"),EGPD_Input):nullptr;
    const UEdGraphPin* RequestedSource=Source?Source->FindPin(FName(*SourcePinName),EGPD_Output):nullptr;
    const UEdGraphPin* RequestedMap=Target?Target->FindPin(TEXT("TargetMap"),EGPD_Input):nullptr;
    const auto DoubleTerminal=[](const FEdGraphPinType& T){return T.IsMap()&&T.PinCategory==UEdGraphSchema_K2::PC_String&&T.PinValueType.TerminalCategory==UEdGraphSchema_K2::PC_Real&&T.PinValueType.TerminalSubCategory==UEdGraphSchema_K2::PC_Double&&T.PinValueType.TerminalSubCategoryObject==nullptr;};
    const bool DoubleMap=(RequestedSource&&DoubleTerminal(RequestedSource->PinType))||(RequestedMap&&DoubleTerminal(RequestedMap->PinType))||(SourceMap&&DoubleTerminal(SourceMap->PinType));
    R->SetStringField(TEXT("connection_scope"),DoubleMap?TEXT("native_string_double_map"):TEXT("native_string_vector_map"));
    auto O=MakeShared<FJsonObject>();R->SetObjectField(TEXT("map_connection_observation"),O);
    const auto Loaded=[](const UObject* P){return IsValid(P)&&!P->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    auto Stop=[&](const TCHAR* Message){Fail(R,TEXT("graph_map_connection_scope_changed"),Message);return R;};
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Loaded(Graph)||!Loaded(Source)||!Loaded(Target))return Stop(TEXT("Idle loaded current Map endpoints are required."));
    UBlueprint* BP=FBlueprintEditorUtils::FindBlueprintForGraph(Graph);if(!Loaded(BP))return Stop(TEXT("An exact standard K2 Blueprint graph is required."));
    TStrongObjectPtr<UBlueprint> HeldBP(BP);TStrongObjectPtr<UEdGraph> HeldGraph(Graph);TStrongObjectPtr<UEditorEngine> HeldEditor(GEditor);auto* Undo=GUndo;
    struct FRole{UObject* Object;UClass* Class;UObject* Outer;FString Path;EObjectFlags Flags;uint32 ClassFlags;int32 ClassSize;};TArray<FRole> Roles;TArray<TStrongObjectPtr<UObject>> Held;
    auto Hold=[&](UObject* P){if(!Loaded(P))return false;for(const auto& V:Roles)if(V.Object==P)return true;Held.Emplace(P);const UClass* C=P->GetClass()==UClass::StaticClass()?static_cast<UClass*>(P):nullptr;Roles.Add({P,P->GetClass(),P->GetOuter(),P->GetPathName(),P->GetFlags(),C?uint32(C->GetClassFlags()):0,C?C->GetStructureSize():0});return true;};
    if(!Hold(BP)||!Hold(Graph)||!Hold(Source)||!Hold(Target)||!Hold(BP->GetPackage()))return Stop(TEXT("Endpoint roles are unavailable."));
    UClass* SchemaClass=UEdGraphSchema_K2::StaticClass();const auto SchemaHandle=Graph->Schema.GetGCPtr();const auto SchemaCDO=MaterialValueCDOHandle(SchemaClass);
    if(!SchemaHandle.IsResolved()||SchemaHandle!=SchemaClass||!SchemaCDO.IsResolved()||SchemaCDO==nullptr||!Hold(SchemaClass))return Stop(TEXT("Exact resolved K2 schema class and existing CDO are required."));
    UObject* SchemaObject=FindObject<UObject>(nullptr,*SchemaCDO.GetPathName());if(SchemaCDO!=SchemaObject||!Hold(SchemaObject)||SchemaObject->GetClass()!=SchemaClass||!SchemaObject->HasAnyFlags(RF_ClassDefaultObject))return Stop(TEXT("An existing exact loaded K2 schema CDO is required."));
    const auto* Schema=static_cast<const UEdGraphSchema_K2*>(SchemaObject);
    UPackage* HostPackage=BP->GetPackage();const auto HostFlags=HostPackage->GetPackageFlags();const bool HostDirty=HostPackage->IsDirty();
    UScriptStruct* Vector=DoubleMap?nullptr:FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.Vector"));UPackage* Core=DoubleMap?FindPackage(nullptr,TEXT("/Script/CoreUObject")):Vector?Vector->GetPackage():nullptr;
    if(DoubleMap?(!Hold(Core)||Core->GetName()!=TEXT("/Script/CoreUObject")||uint32(Core->GetPackageFlags())!=16||Core->IsDirty()):(Vector!=TBaseStructure<FVector>::Get()||!Hold(Vector)||!Hold(Core)||Vector->GetOuter()!=Core||Core->GetName()!=TEXT("/Script/CoreUObject")||uint32(Core->GetPackageFlags())!=16||Core->IsDirty()||Vector->GetStructureSize()!=sizeof(FVector)||Vector->GetMinAlignment()!=alignof(FVector)||!(Vector->StructFlags&STRUCT_Native)))return Stop(TEXT("Exact native Vector layout is required."));
    const auto StructFlags=DoubleMap?STRUCT_NoFlags:Vector->StructFlags;struct FAxis{FProperty* P;FName Name;uint64 Flags;int32 Offset,Size;};TArray<FAxis> Axes;
    if(!DoubleMap)for(TFieldIterator<FProperty> It(Vector,EFieldIterationFlags::None);It;++It)
    {FProperty* P=*It;const int32 N=Axes.Num();if(N>=3||P->GetClass()!=FDoubleProperty::StaticClass()||P->GetOwnerStruct()!=Vector||P->ArrayDim!=1||P->GetSize()!=sizeof(double)||P->GetOffset_ForInternal()!=int32(N*sizeof(double))||P->GetFName()!=FName(N==0?TEXT("X"):N==1?TEXT("Y"):TEXT("Z")))return Stop(TEXT("Native Vector fields differ."));Axes.Add({P,P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize()});}
    if(Axes.Num()!=(DoubleMap?0:3))return Stop(TEXT("Native Vector requires three exact fields."));
    UK2Node_CallFunction* Call=ThunkSource?ArraySourceCall:Target->GetClass()==UK2Node_CallFunction::StaticClass()?static_cast<UK2Node_CallFunction*>(Target):nullptr;
    const FName FunctionName=Call?Call->FunctionReference.GetMemberName():NAME_None;const bool Length=FunctionName==TEXT("Map_Length"),Clear=FunctionName==TEXT("Map_Clear"),Find=FunctionName==TEXT("Map_Find"),Add=FunctionName==TEXT("Map_Add"),Contains=FunctionName==TEXT("Map_Contains"),Remove=FunctionName==TEXT("Map_Remove"),KeyOnly=Contains||Remove,Keys=FunctionName==TEXT("Map_Keys"),ArrayResult=Keys||FunctionName==TEXT("Map_Values");
    const bool Thunk=Call&&(Length||Clear||Find||Add||KeyOnly||ArrayResult);UClass* Owner=nullptr;UFunction* Function=nullptr;FMemberReference Reference;uint32 FunctionFlags=0;TSharedRef<FJsonObject> FunctionBefore=MakeShared<FJsonObject>();
    TArray<TPair<FName,FString>> KeyMetadata;TArray<bool> KeyMetadataPresent;
    if(Thunk)
    {
        Owner=FindObject<UClass>(nullptr,TEXT("/Script/Engine.BlueprintMapLibrary"));if(Owner!=UBlueprintMapLibrary::StaticClass()||!Hold(Owner)||!Hold(Owner->GetPackage())||Owner->GetPackage()->GetName()!=TEXT("/Script/Engine")||Owner->GetPackage()->IsDirty()||uint32(Owner->GetPackage()->GetPackageFlags())!=16||!Owner->HasAnyClassFlags(CLASS_Native))return Stop(TEXT("Exact native BlueprintMapLibrary is required."));
        Reference.SetExternalMember(FunctionName,Owner);if(Call->FunctionReference.IsDeprecated()||!Call->FunctionReference.IsSameReference(Reference))return Stop(TEXT("Only the exact external native Map reference is admitted."));
        Function=Owner->FindFunctionByName(FunctionName);if(!Hold(Function)||Function->GetOuter()!=Owner||!Function->HasAllFunctionFlags(FUNC_Static|FUNC_Native|FUNC_Public)||Function->GetMetaData(TEXT("MapParam"))!=TEXT("TargetMap")||(ArrayResult?(Function->HasMetaData(TEXT("MapKeyParam"))!=Keys||Function->HasMetaData(TEXT("MapValueParam"))==Keys||!Function->GetMetaData(Keys?TEXT("MapKeyParam"):TEXT("MapValueParam")).Equals(Keys?TEXT("Keys"):TEXT("Values"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("AutoCreateRefTerm")).Equals(Keys?TEXT("Keys"):TEXT("Values"),ESearchCase::CaseSensitive)):KeyOnly?(Function->GetMetaData(TEXT("MapKeyParam"))!=TEXT("Key")||Function->HasMetaData(TEXT("MapValueParam"))||Function->GetMetaData(TEXT("AutoCreateRefTerm"))!=TEXT("Key")):(!(Clear||Length)&&(Function->GetMetaData(TEXT("MapKeyParam"))!=TEXT("Key")||Function->GetMetaData(TEXT("MapValueParam"))!=TEXT("Value")||Function->GetMetaData(TEXT("AutoCreateRefTerm"))!=TEXT("Key, Value")))))return Stop(TEXT("Native finite Map metadata differs."));
        FunctionFlags=uint32(Function->FunctionFlags);FunctionBefore->SetStringField(TEXT("function_path"),Function->GetPathName());FunctionBefore->SetStringField(TEXT("function_flags_string"),LexToString(FunctionFlags));FunctionBefore->SetStringField(TEXT("map_param"),TEXT("TargetMap"));FunctionBefore->SetStringField(TEXT("key_param"),ArrayResult?(Keys?TEXT("Keys"):TEXT("")):(Clear||Length)?TEXT(""):TEXT("Key"));FunctionBefore->SetStringField(TEXT("value_param"),ArrayResult?(Keys?TEXT(""):TEXT("Values")):(Clear||KeyOnly||Length)?TEXT(""):TEXT("Value"));
        if(Length){
            const uint32 Expected=uint32(FUNC_Final|FUNC_RequiredAPI|FUNC_Native|FUNC_Static|FUNC_Public|FUNC_HasOutParms|FUNC_BlueprintCallable|FUNC_BlueprintPure);
            if(FunctionFlags!=Expected||Function->HasMetaData(TEXT("MapKeyParam"))||Function->HasMetaData(TEXT("MapValueParam"))||Function->HasMetaData(TEXT("AutoCreateRefTerm"))||!Function->GetMetaData(TEXT("Category")).Equals(TEXT("Utilities|Map"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("CustomThunk")).Equals(TEXT("true"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("DisplayName")).Equals(TEXT("Length"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("CompactNodeTitle")).Equals(TEXT("LENGTH"),ESearchCase::CaseSensitive)||!Function->HasMetaData(TEXT("BlueprintThreadSafe"))||!Function->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty()||!Owner->HasMetaData(TEXT("BlueprintThreadSafe"))||!Owner->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())return Stop(TEXT("Native pure Map Length flags/metadata differ."));
            for(const TCHAR* MetaName:{TEXT("Category"),TEXT("CustomThunk"),TEXT("MapParam"),TEXT("MapKeyParam"),TEXT("MapValueParam"),TEXT("AutoCreateRefTerm"),TEXT("BlueprintThreadSafe"),TEXT("DisplayName"),TEXT("CompactNodeTitle"),TEXT("Keywords"),TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")}){const FName MetadataName(MetaName);KeyMetadata.Emplace(MetadataName,Function->GetMetaData(MetadataName));KeyMetadataPresent.Add(Function->HasMetaData(MetadataName));}
            for(const TCHAR* MetaName:{TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")})if(Function->HasMetaData(MetaName))return Stop(TEXT("Unexpected Map Length execution metadata."));
        }
        if(ArrayResult){
            const uint32 Expected=uint32(FUNC_Final|FUNC_RequiredAPI|FUNC_Native|FUNC_Static|FUNC_Public|FUNC_HasOutParms|FUNC_BlueprintCallable);const TCHAR* OutputName=Keys?TEXT("Keys"):TEXT("Values");
            if(FunctionFlags!=Expected||!Function->GetMetaData(TEXT("MapParam")).Equals(TEXT("TargetMap"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("Category")).Equals(TEXT("Utilities|Map"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("CustomThunk")).Equals(TEXT("true"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("DisplayName")).Equals(OutputName,ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("CompactNodeTitle")).Equals(Keys?TEXT("KEYS"):TEXT("VALUES"),ESearchCase::CaseSensitive)||Function->HasMetaData(TEXT("BlueprintThreadSafe"))||!Owner->HasMetaData(TEXT("BlueprintThreadSafe"))||!Owner->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())return Stop(TEXT("Native array-output Map function flags/metadata differ."));
            for(const TCHAR* Name:{TEXT("Category"),TEXT("CustomThunk"),TEXT("MapParam"),TEXT("MapKeyParam"),TEXT("MapValueParam"),TEXT("AutoCreateRefTerm"),TEXT("BlueprintThreadSafe"),TEXT("DisplayName"),TEXT("CompactNodeTitle"),TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")}){const FName K(Name);KeyMetadata.Emplace(K,Function->GetMetaData(K));KeyMetadataPresent.Add(Function->HasMetaData(K));}
            for(const TCHAR* Name:{TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")})if(Function->HasMetaData(Name))return Stop(TEXT("Unexpected array-output Map execution metadata."));
        }
        if(KeyOnly){
            const uint32 Expected=uint32(FUNC_Final|FUNC_RequiredAPI|FUNC_Native|FUNC_Static|FUNC_Public|FUNC_HasOutParms|FUNC_BlueprintCallable)|(Contains?uint32(FUNC_BlueprintPure):0U);
            if(FunctionFlags!=Expected||!Function->GetMetaData(TEXT("MapParam")).Equals(TEXT("TargetMap"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("MapKeyParam")).Equals(TEXT("Key"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("AutoCreateRefTerm")).Equals(TEXT("Key"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("Category")).Equals(TEXT("Utilities|Map"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("CustomThunk")).Equals(TEXT("true"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("DisplayName")).Equals(Contains?TEXT("Contains"):TEXT("Remove"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("CompactNodeTitle")).Equals(Contains?TEXT("CONTAINS"):TEXT("REMOVE"),ESearchCase::CaseSensitive)||Function->HasMetaData(TEXT("BlueprintThreadSafe"))!=Contains||(Contains&&!Function->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())||!Owner->HasMetaData(TEXT("BlueprintThreadSafe"))||!Owner->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())return Stop(TEXT("Native key-only Map function flags/metadata differ."));
            for(const TCHAR* Name:{TEXT("Category"),TEXT("CustomThunk"),TEXT("MapParam"),TEXT("MapKeyParam"),TEXT("MapValueParam"),TEXT("AutoCreateRefTerm"),TEXT("BlueprintThreadSafe"),TEXT("DisplayName"),TEXT("CompactNodeTitle"),TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")}){const FName K(Name);KeyMetadata.Emplace(K,Function->GetMetaData(K));KeyMetadataPresent.Add(Function->HasMetaData(K));}
            for(const TCHAR* Name:{TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")})if(Function->HasMetaData(Name))return Stop(TEXT("Unexpected key-only Map execution metadata."));
        }
    }
    else if(Target->GetClass()!=UK2Node_VariableSet::StaticClass()&&!(DoubleMap&&Target->GetClass()==UK2Node_FunctionResult::StaticClass()))return Stop(TEXT("Only the finite native Map thunks or an own Map setter are admitted."));
    const auto Member=[&](const UEdGraphNode* N){int32 Count=0;for(const auto& H:Graph->Nodes){if(!H.IsResolved())return false;if(H==N)++Count;}return Count==1;};
    struct FThunkField{const FProperty* P;const FMapProperty* Parent;FName Name;FFieldClass* Class;uint64 Flags;int32 Offset,Size,Dim;const FArrayProperty* ArrayParent=nullptr;};TArray<FThunkField> ThunkFields;
    if(Thunk)for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)
    {
        const FProperty* P=*It;ThunkFields.Add({P,nullptr,P->GetFName(),P->GetClass(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim});
        if(const auto* M=CastField<FMapProperty>(P))for(const FProperty* Child:{M->KeyProp,M->ValueProp}){if(!Child)return Stop(TEXT("Native Map parameter child is unavailable."));ThunkFields.Add({Child,M,Child->GetFName(),Child->GetClass(),uint64(Child->GetPropertyFlags()),Child->GetOffset_ForInternal(),Child->GetSize(),Child->ArrayDim});}
    }
    if(ArrayResult)for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)if(const auto* A=CastField<FArrayProperty>(*It)){const FProperty* Child=A->Inner;if(!Child)return Stop(TEXT("Native array output Inner unavailable."));ThunkFields.Add({Child,nullptr,Child->GetFName(),Child->GetClass(),uint64(Child->GetPropertyFlags()),Child->GetOffset_ForInternal(),Child->GetSize(),Child->ArrayDim,A});}
    const FMapProperty* KeyMap=nullptr;const FProperty *KeyMapKey=nullptr,*KeyMapValue=nullptr;TArray<const FProperty*> KeyParameters;int32 KeyFunctionSize=0;TObjectPtr<UObject> KeyCDO;
    if(KeyOnly||Length){
        for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)KeyParameters.Add(*It);
        KeyFunctionSize=Function->GetStructureSize();KeyCDO=MaterialValueCDOHandle(Owner);
        if(!KeyCDO.IsResolved()||KeyCDO==nullptr)return Stop(TEXT("Exact existing resolved Map CDO required."));
        if(KeyParameters.Num()!=(Length?2:3)||Function->NumParms!=KeyParameters.Num())return Stop(TEXT("Key-only Map requires exactly TargetMap, Key, ReturnValue."));
        constexpr uint64 DirectionBits=uint64(CPF_Parm|CPF_OutParm|CPF_ReferenceParm|CPF_ConstParm|CPF_ReturnParm);
        for(int32 I=0;I<KeyParameters.Num();++I){const FProperty* P=KeyParameters[I];const bool Return=I==(Length?1:2);const uint64 Bits=uint64(CPF_Parm|CPF_OutParm)|(Return?uint64(CPF_ReturnParm):uint64(CPF_ReferenceParm|CPF_ConstParm));
            if(P->GetFName()!=FName(I==0?TEXT("TargetMap"):Length||I==2?TEXT("ReturnValue"):TEXT("Key"))||P->GetOwnerStruct()!=Function||P->GetOwner<FProperty>()!=nullptr||P->ArrayDim!=1||P->HasSetterOrGetter()||(uint64(P->GetPropertyFlags())&DirectionBits)!=Bits||P->HasAnyPropertyFlags(CPF_EditorOnly|CPF_AdvancedDisplay|CPF_RequiredParm)||P->GetOffset_ForInternal()<0||P->GetSize()<=0||P->GetOffset_ForInternal()+P->GetSize()>Function->GetStructureSize()||Function->HasMetaData(FName(*(TEXT("CPP_Default_")+P->GetName()))))return Stop(TEXT("Key-only Map ordered property role differs."));
            if(I==0){if(P->GetClass()!=FMapProperty::StaticClass())return Stop(TEXT("Exact Map property required."));KeyMap=static_cast<const FMapProperty*>(P);KeyMapKey=KeyMap->KeyProp;KeyMapValue=KeyMap->ValueProp;
                for(const FProperty* Child:{KeyMapKey,KeyMapValue})if(!Child||Child->GetClass()!=FIntProperty::StaticClass()||Child->GetOwner<FProperty>()!=KeyMap||Child->GetOwnerProperty()!=KeyMap||Child->ArrayDim!=1||Child->GetSize()!=sizeof(int32)||Child->HasSetterOrGetter())return Stop(TEXT("Exact owned int32 placeholder children required."));}
            else if(P->GetClass()!=((Return&&!Length)?FBoolProperty::StaticClass():FIntProperty::StaticClass())||P->GetSize()!=((Return&&!Length)?sizeof(bool):sizeof(int32)))return Stop(TEXT("Key-only Map scalar property class/size differs."));
            const FName DefaultKey(*(TEXT("CPP_Default_")+P->GetName()));KeyMetadata.Emplace(DefaultKey,Function->GetMetaData(DefaultKey));KeyMetadataPresent.Add(Function->HasMetaData(DefaultKey));
        }
    }
    const FMapProperty* ArrayMap=nullptr;const FProperty *ArrayMapKey=nullptr,*ArrayMapValue=nullptr,*OutInner=nullptr;const FArrayProperty* OutArray=nullptr;TArray<const FProperty*> ArrayParameters;
    if(ArrayResult){
        for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)ArrayParameters.Add(*It);
        KeyFunctionSize=Function->GetStructureSize();KeyCDO=MaterialValueCDOHandle(Owner);if(!KeyCDO.IsResolved()||KeyCDO==nullptr||ArrayParameters.Num()!=2||Function->NumParms!=2)return Stop(TEXT("Array-output Map requires existing CDO and exactly TargetMap/array output."));
        constexpr uint64 DirectionBits=uint64(CPF_Parm|CPF_OutParm|CPF_ReferenceParm|CPF_ConstParm|CPF_ReturnParm);
        for(int32 I=0;I<2;++I){const FProperty* P=ArrayParameters[I];const uint64 Bits=uint64(CPF_Parm|CPF_OutParm)|(I==0?uint64(CPF_ReferenceParm|CPF_ConstParm):0ULL);
            if(P->GetFName()!=FName(I==0?TEXT("TargetMap"):Keys?TEXT("Keys"):TEXT("Values"))||P->GetOwnerStruct()!=Function||P->GetOwner<FProperty>()!=nullptr||P->ArrayDim!=1||P->HasSetterOrGetter()||(uint64(P->GetPropertyFlags())&DirectionBits)!=Bits||P->HasAnyPropertyFlags(CPF_EditorOnly|CPF_AdvancedDisplay|CPF_RequiredParm)||P->GetOffset_ForInternal()<0||P->GetSize()<=0||P->GetOffset_ForInternal()+P->GetSize()>KeyFunctionSize||Function->HasMetaData(FName(*(TEXT("CPP_Default_")+P->GetName()))))return Stop(TEXT("Array-output Map ordered property role differs."));
            if(I==0){if(P->GetClass()!=FMapProperty::StaticClass())return Stop(TEXT("Exact Map input property required."));ArrayMap=static_cast<const FMapProperty*>(P);ArrayMapKey=ArrayMap->KeyProp;ArrayMapValue=ArrayMap->ValueProp;for(const FProperty* C:{ArrayMapKey,ArrayMapValue})if(!C||C->GetClass()!=FIntProperty::StaticClass()||C->GetOwner<FProperty>()!=ArrayMap||C->GetOwnerProperty()!=ArrayMap||C->ArrayDim!=1||C->GetSize()!=sizeof(int32)||C->HasSetterOrGetter())return Stop(TEXT("Owned int32 native Map placeholders required."));}
            else {if(P->GetClass()!=FArrayProperty::StaticClass())return Stop(TEXT("Exact array output property required."));OutArray=static_cast<const FArrayProperty*>(P);OutInner=OutArray->Inner;if(!OutInner||OutInner->GetClass()!=FIntProperty::StaticClass()||OutInner->GetOwner<FProperty>()!=OutArray||OutInner->GetOwnerProperty()!=OutArray||OutInner->GetOwnerStruct()!=Function||OutInner->ArrayDim!=1||OutInner->GetSize()!=sizeof(int32)||OutInner->GetOffset_ForInternal()<0||OutInner->HasSetterOrGetter())return Stop(TEXT("Owned native int32 Array.Inner placeholder required; typed output comes from the Map link."));}
            const FName DefaultKey(*(TEXT("CPP_Default_")+P->GetName()));KeyMetadata.Emplace(DefaultKey,Function->GetMetaData(DefaultKey));KeyMetadataPresent.Add(Function->HasMetaData(DefaultKey));
        }
    }
    struct FNode{UEdGraphNode* N;FGuid Guid;TArray<UEdGraphPin*> Pins;};TArray<FNode> Nodes;
    auto Node=[&](UEdGraphNode* N){for(const auto& V:Nodes)if(V.N==N)return true;if(!Hold(N)||N->GetOuter()!=Graph||!N->NodeGuid.IsValid()||!Member(N)||N->Pins.Num()>256)return false;Nodes.Add({N,N->NodeGuid,N->Pins});return true;};
    if(!Node(Source)||!Node(Target))return Stop(TEXT("Current endpoint node membership is required."));
    // Capture linked owners before using peer pin identities. No SDK callback is dispatched here.
    for(int32 I=0;I<Nodes.Num();++I)
    {const auto CurrentPins=Nodes[I].Pins;for(UEdGraphPin* P:CurrentPins)
    {if(!P||P->WasTrashed()||P->GetOwningNode()!=Nodes[I].N)return Stop(TEXT("A physical pin is unavailable."));for(UEdGraphPin* Peer:P->LinkedTo){if(!Peer||Peer->WasTrashed()||!Peer->GetOwningNode()||Peer->GetOwningNode()->GetClass()->HasAnyClassFlags(CLASS_Deprecated))return Stop(TEXT("A linked pin is unavailable."));UEdGraphNode* N=Peer->GetOwningNode();if(!Node(N)||Nodes.Num()>2048)return Stop(TEXT("A linked owner is outside the current graph."));}}}
    const auto SameType=[](const FEdGraphPinType& A,const FEdGraphPinType& B){return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;};
    auto Flags=[](const UEdGraphPin* P){return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)|(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)|(uint32(P->ShouldSavePinIfOrphaned())<<9);};
    struct FPin{UEdGraphPin* P;UEdGraphNode* N;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;uint32 Flags;FString Default,Auto;TObjectPtr<UObject> Object;FText Text;TArray<UEdGraphPin*> Links;};TArray<FPin> Pins;
    for(const auto& N:Nodes)for(UEdGraphPin* P:N.Pins)
    {if(!P||P->WasTrashed()||!P->PinId.IsValid()||P->GetOwningNode()!=N.N||P->ParentPin||!P->SubPins.IsEmpty()||P->bOrphanedPin||!P->DefaultObject.IsResolved())return Stop(TEXT("Whole current physical pins with resolved defaults are required."));Pins.Add({P,N.N,P->PinId,P->PinName,P->Direction,P->PinType,Flags(P),P->DefaultValue,P->AutogeneratedDefaultValue,P->DefaultObject,P->DefaultTextValue,P->LinkedTo});}
    struct FOwn{UClass* Parent;UClass* Skeleton;UClass* Generated;FBPVariableDescription D;const FMapProperty* Map;const FStrProperty* Key;const FProperty* Value;uint64 MapFlags,KeyFlags,ValueFlags;int32 MapOffset,MapSize,KeyOffset,KeySize,ValueOffset,ValueSize;TArray<uint32> ClassFlags;TArray<int32> ClassSizes;UK2Node_Variable* Node;FMemberReference Reference;};TArray<FOwn> OwnRoles;
    const auto SameDescription=[&](const FBPVariableDescription& X,const FBPVariableDescription& Y){if(X.VarName!=Y.VarName||X.VarGuid!=Y.VarGuid||!SameType(X.VarType,Y.VarType)||X.FriendlyName!=Y.FriendlyName||!X.Category.IdenticalTo(Y.Category)||X.PropertyFlags!=Y.PropertyFlags||X.RepNotifyFunc!=Y.RepNotifyFunc||X.ReplicationCondition!=Y.ReplicationCondition||X.DefaultValue!=Y.DefaultValue||X.MetaDataArray.Num()!=Y.MetaDataArray.Num())return false;for(int32 I=0;I<X.MetaDataArray.Num();++I)if(X.MetaDataArray[I].DataKey!=Y.MetaDataArray[I].DataKey||X.MetaDataArray[I].DataValue!=Y.MetaDataArray[I].DataValue)return false;return true;};
    TArray<TSharedPtr<FLocalDoubleMap>> LocalRoles;
    struct FResultMapRole{UK2Node_FunctionResult* Node;UK2Node_FunctionEntry* Entry;FMemberReference ResultReference,EntryReference;TArray<TSharedPtr<FUserPinInfo>> Users;TArray<FUserPinInfo> Copies;};TArray<FResultMapRole> ResultRoles;
    auto Current=[&]()
    {
        for(const auto& Local:LocalRoles)if(!Local->Current())return false;
        for(const auto& V:ResultRoles){if(!V.Node->FunctionReference.IsSameReference(V.ResultReference)||V.Node->FunctionReference.IsDeprecated()!=V.ResultReference.IsDeprecated()||!V.Entry->FunctionReference.IsSameReference(V.EntryReference)||V.Node->UserDefinedPins!=V.Users)return false;for(int32 I=0;I<V.Users.Num();++I)if(!V.Users[I].IsValid()||V.Users[I]->PinName!=V.Copies[I].PinName||!SameType(V.Users[I]->PinType,V.Copies[I].PinType)||V.Users[I]->DesiredPinDirection!=V.Copies[I].DesiredPinDirection||!V.Users[I]->PinDefaultValue.Equals(V.Copies[I].PinDefaultValue,ESearchCase::CaseSensitive))return false;}
        if(!IsEditorGameThread()||!Loaded(HeldEditor.Get())||GEditor!=HeldEditor.Get()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GUndo!=Undo)return false;
        for(const auto& V:Roles)if(!Loaded(V.Object)||V.Object->GetClass()!=V.Class||V.Object->GetOuter()!=V.Outer||V.Object->GetPathName()!=V.Path||V.Object->GetFlags()!=V.Flags||(V.Class==UClass::StaticClass()&&(uint32(static_cast<UClass*>(V.Object)->GetClassFlags())!=V.ClassFlags||static_cast<UClass*>(V.Object)->GetStructureSize()!=V.ClassSize)))return false;
        const auto CDO=MaterialValueCDOHandle(SchemaClass);if(!Graph->Schema.GetGCPtr().IsResolved()||Graph->Schema.GetGCPtr()!=SchemaClass||!CDO.IsResolved()||CDO!=SchemaObject||HostPackage->GetPackageFlags()!=HostFlags||HostPackage->IsDirty()!=HostDirty||Graph->GetOuter()!=BP)return false;
        for(const auto& V:OwnRoles)
        {
            if(!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=V.Parent||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->SkeletonGeneratedClass.GetGCPtr()!=V.Skeleton||!BP->GeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr()!=V.Generated||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified)return false;
            if(!V.Node->VariableReference.IsSameReference(V.Reference)||V.Node->VariableReference.IsDeprecated()!=V.Reference.IsDeprecated())return false;
            int32 ClassIndex=0;for(UClass* C:{V.Parent,V.Skeleton,V.Generated}){if(uint32(C->GetClassFlags())!=V.ClassFlags[ClassIndex]||C->GetStructureSize()!=V.ClassSizes[ClassIndex]||!C->ClassGeneratedBy.IsResolved()||(C!=V.Parent&&C->ClassGeneratedBy!=BP))return false;++ClassIndex;}
            const FBPVariableDescription* D=BP->NewVariables.FindByPredicate([&](const auto& X){return X.VarName==V.D.VarName;});if(!D||!SameDescription(*D,V.D))return false;
            const FProperty* P=V.Skeleton->FindPropertyByName(V.D.VarName);if(P!=V.Map||P->GetClass()!=FMapProperty::StaticClass()||P->GetOwnerClass()!=V.Skeleton||P->ArrayDim!=1||uint64(P->GetPropertyFlags())!=V.MapFlags||P->GetOffset_ForInternal()!=V.MapOffset||P->GetSize()!=V.MapSize||V.Map->KeyProp!=V.Key||V.Map->ValueProp!=V.Value)return false;
            if(V.Key->GetClass()!=FStrProperty::StaticClass()||V.Value->GetClass()!=(DoubleMap?FDoubleProperty::StaticClass():FStructProperty::StaticClass())||V.Key->GetOwnerProperty()!=V.Map||V.Value->GetOwnerProperty()!=V.Map||V.Key->ArrayDim!=1||V.Value->ArrayDim!=1||uint64(V.Key->GetPropertyFlags())!=V.KeyFlags||uint64(V.Value->GetPropertyFlags())!=V.ValueFlags||V.Key->GetOffset_ForInternal()!=V.KeyOffset||V.Key->GetSize()!=V.KeySize||V.Value->GetOffset_ForInternal()!=V.ValueOffset||V.Value->GetSize()!=V.ValueSize||(!DoubleMap&&(!static_cast<const FStructProperty*>(V.Value)->Struct.IsResolved()||static_cast<const FStructProperty*>(V.Value)->Struct!=Vector)))return false;
        }
        if((!DoubleMap&&(Vector->StructFlags!=StructFlags||Vector->GetStructureSize()!=sizeof(FVector)||Vector->GetMinAlignment()!=alignof(FVector)))||Core->IsDirty()||uint32(Core->GetPackageFlags())!=16)return false;
        for(const auto& A:Axes)if(FindFProperty<FProperty>(Vector,A.Name)!=A.P||A.P->GetClass()!=FDoubleProperty::StaticClass()||A.P->GetOwnerStruct()!=Vector||A.P->ArrayDim!=1||uint64(A.P->GetPropertyFlags())!=A.Flags||A.P->GetOffset_ForInternal()!=A.Offset||A.P->GetSize()!=A.Size)return false;
        if(Thunk&&(uint32(Function->FunctionFlags)!=FunctionFlags||Owner->GetPackage()->IsDirty()||uint32(Owner->GetPackage()->GetPackageFlags())!=16||Owner->FindFunctionByName(FunctionName)!=Function||!Call->FunctionReference.IsSameReference(Reference)||Call->FunctionReference.IsDeprecated()||Function->GetMetaData(TEXT("MapParam"))!=TEXT("TargetMap")||(ArrayResult?(Function->HasMetaData(TEXT("MapKeyParam"))!=Keys||Function->HasMetaData(TEXT("MapValueParam"))==Keys||!Function->GetMetaData(Keys?TEXT("MapKeyParam"):TEXT("MapValueParam")).Equals(Keys?TEXT("Keys"):TEXT("Values"),ESearchCase::CaseSensitive)||!Function->GetMetaData(TEXT("AutoCreateRefTerm")).Equals(Keys?TEXT("Keys"):TEXT("Values"),ESearchCase::CaseSensitive)):KeyOnly?(Function->GetMetaData(TEXT("MapKeyParam"))!=TEXT("Key")||Function->HasMetaData(TEXT("MapValueParam"))||Function->GetMetaData(TEXT("AutoCreateRefTerm"))!=TEXT("Key")):(!(Clear||Length)&&(Function->GetMetaData(TEXT("MapKeyParam"))!=TEXT("Key")||Function->GetMetaData(TEXT("MapValueParam"))!=TEXT("Value")||Function->GetMetaData(TEXT("AutoCreateRefTerm"))!=TEXT("Key, Value"))))))return false;
        if(KeyOnly||Length){
            TArray<const FProperty*> CurrentParameters;for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)CurrentParameters.Add(*It);
            if(CurrentParameters!=KeyParameters||Function->NumParms!=(Length?2:3)||Function->GetStructureSize()!=KeyFunctionSize||!MaterialValueCDOHandle(Owner).IsResolved()||MaterialValueCDOHandle(Owner)!=KeyCDO||KeyMap->KeyProp!=KeyMapKey||KeyMap->ValueProp!=KeyMapValue||!Owner->HasMetaData(TEXT("BlueprintThreadSafe"))||!Owner->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())return false;
            for(int32 I=0;I<KeyMetadata.Num();++I)if(Function->HasMetaData(KeyMetadata[I].Key)!=KeyMetadataPresent[I]||!Function->GetMetaData(KeyMetadata[I].Key).Equals(KeyMetadata[I].Value,ESearchCase::CaseSensitive))return false;
        }
        if(ArrayResult){
            TArray<const FProperty*> Now;for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)Now.Add(*It);
            if(Now!=ArrayParameters||Function->NumParms!=2||Function->GetStructureSize()!=KeyFunctionSize||!MaterialValueCDOHandle(Owner).IsResolved()||MaterialValueCDOHandle(Owner)!=KeyCDO||ArrayMap->KeyProp!=ArrayMapKey||ArrayMap->ValueProp!=ArrayMapValue||OutArray->Inner!=OutInner||ArrayMapKey->GetOwner<FProperty>()!=ArrayMap||ArrayMapValue->GetOwner<FProperty>()!=ArrayMap||OutInner->GetOwner<FProperty>()!=OutArray||!Owner->HasMetaData(TEXT("BlueprintThreadSafe"))||!Owner->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())return false;
            for(int32 I=0;I<KeyMetadata.Num();++I)if(Function->HasMetaData(KeyMetadata[I].Key)!=KeyMetadataPresent[I]||!Function->GetMetaData(KeyMetadata[I].Key).Equals(KeyMetadata[I].Value,ESearchCase::CaseSensitive))return false;
        }
        for(const auto& V:ThunkFields)
        {
            const FProperty* P=V.ArrayParent?V.ArrayParent->Inner:V.Parent?(V.Parent->KeyProp==V.P?V.Parent->KeyProp:V.Parent->ValueProp==V.P?V.Parent->ValueProp:nullptr):Function->FindPropertyByName(V.Name);
            if(P!=V.P||P->GetClass()!=V.Class||P->GetFName()!=V.Name||uint64(P->GetPropertyFlags())!=V.Flags||P->GetOffset_ForInternal()!=V.Offset||P->GetSize()!=V.Size||P->ArrayDim!=V.Dim||(V.ArrayParent?P->GetOwner<FProperty>()!=V.ArrayParent:V.Parent?P->GetOwnerProperty()!=V.Parent:P->GetOwnerStruct()!=Function))return false;
        }
        for(const auto& N:Nodes){if(N.N->GetOuter()!=Graph||N.N->NodeGuid!=N.Guid||!Member(N.N)||N.N->Pins!=N.Pins)return false;}
        for(const auto& V:Pins){const UEdGraphPin* P=V.P;if(P->WasTrashed()||P->GetOwningNode()!=V.N||P->PinId!=V.Id||P->PinName!=V.Name||P->Direction!=V.Direction||P->ParentPin||!P->SubPins.IsEmpty()||!SameType(P->PinType,V.Type)||Flags(P)!=V.Flags||P->DefaultValue!=V.Default||P->AutogeneratedDefaultValue!=V.Auto||!P->DefaultObject.IsResolved()||P->DefaultObject!=V.Object||!P->DefaultTextValue.IdenticalTo(V.Text)||P->LinkedTo!=V.Links)return false;for(UEdGraphPin* Q:V.Links)if(!Pins.ContainsByPredicate([&](const FPin& Other){return Other.P==Q;})||!Q->LinkedTo.Contains(V.P))return false;}
        return true;
    };
    if(!Current())return Stop(TEXT("Current Map node/source/pin roles changed during capture."));
    if(KeyOnly||ArrayResult||Length){
        UObject* Existing=FindObject<UObject>(nullptr,TEXT("/Script/Engine.Default__BlueprintMapLibrary"));TStrongObjectPtr<UObject> ReturnedHold(Loaded(Existing)?Existing:nullptr);
        if(!Current()||KeyCDO!=Existing||!Hold(Existing)||Existing->GetClass()!=Owner||!Existing->HasAnyFlags(RF_ClassDefaultObject)||!Existing->GetPathName().Equals(TEXT("/Script/Engine.Default__BlueprintMapLibrary"),ESearchCase::CaseSensitive))return Stop(TEXT("Exact current held Map CDO required after lookup."));
    }
    UEdGraphPin* Output=Source->FindPin(FName(*SourcePinName),EGPD_Output);UEdGraphPin* Input=Target->FindPin(FName(*TargetPinName),EGPD_Input);
    if(!Output||!Input||Output->bHidden||Input->bHidden||Output->bNotConnectable||Input->bNotConnectable)return Stop(TEXT("Visible connectable whole endpoints are required."));
    auto Own=[&](UK2Node* N,bool Setter,UEdGraphPin* P)
    {
        if(N->GetClass()!=(Setter?UK2Node_VariableSet::StaticClass():UK2Node_VariableGet::StaticClass()))return false;
        auto* V=static_cast<UK2Node_Variable*>(N);
        if(DoubleMap&&V->VariableReference.IsLocalScope())
        {
            if(Setter)
            {
                auto Plan=Result();auto Local=MakeShared<FLocalDoubleMap>(Plan,true);
                if(!Local->Init(Graph,V->VariableReference.GetMemberName().ToString())||!Local->Setter(static_cast<UK2Node_VariableSet*>(N)))return false;
                Local->Plan();R->SetObjectField(TEXT("map_assignment_member_plan"),Plan);if(!Plan->GetBoolField(TEXT("success"))||!Current())return false;
                LocalRoles.Add(Local);if(!Current())return false;
                const auto* ValuePin=N->FindPin(Local->Name,EGPD_Input);return Current()&&P==ValuePin;
            }
            auto Plan=Result();auto Local=MakeShared<FLocalDoubleMap>(Plan);if(!Local->Init(Graph,V->VariableReference.GetMemberName().ToString())||!Local->Getter(static_cast<UK2Node_VariableGet*>(N)))return false;
            Local->Plan();R->SetObjectField(TEXT("map_producer_member_plan"),Plan);if(!Plan->GetBoolField(TEXT("success"))||!Current())return false;LocalRoles.Add(Local);return Current()&&P==N->Pins[0];
        }
        auto Plan=Result();ReadNativeVectorArrayPlan(Graph,V->VariableReference.GetMemberName().ToString(),Setter,nullptr,Plan,true,DoubleMap);
        R->SetObjectField(Setter?TEXT("map_assignment_member_plan"):TEXT("map_producer_member_plan"),Plan);
        if(!Plan->GetBoolField(TEXT("success"))||!Current())return false;
        const auto Class=[&](const TObjectPtr<UClass>& H)->UClass*{if(!H.IsResolved()||H==nullptr)return nullptr;UClass* C=FindObject<UClass>(nullptr,*H.GetPathName());return H==C&&Hold(C)?C:nullptr;};
        UClass* Parent=Class(BP->ParentClass.GetGCPtr()),*Skeleton=Class(BP->SkeletonGeneratedClass.GetGCPtr()),*Generated=Class(BP->GeneratedClass.GetGCPtr());
        const auto* D=BP->NewVariables.FindByPredicate([&](const auto& X){return X.VarName==V->VariableReference.GetMemberName();});
        const auto* M=Skeleton?CastField<FMapProperty>(Skeleton->FindPropertyByName(V->VariableReference.GetMemberName())):nullptr;const auto* K=M?CastField<FStrProperty>(M->KeyProp):nullptr;const FProperty* W=M?M->ValueProp:nullptr;
        if(!Parent||!Skeleton||!Generated||!D||!M||!K||!W||!Current())return false;
        OwnRoles.Add({Parent,Skeleton,Generated,*D,M,K,W,uint64(M->GetPropertyFlags()),uint64(K->GetPropertyFlags()),uint64(W->GetPropertyFlags()),M->GetOffset_ForInternal(),M->GetSize(),K->GetOffset_ForInternal(),K->GetSize(),W->GetOffset_ForInternal(),W->GetSize(),{uint32(Parent->GetClassFlags()),uint32(Skeleton->GetClassFlags()),uint32(Generated->GetClassFlags())},{Parent->GetStructureSize(),Skeleton->GetStructureSize(),Generated->GetStructureSize()},V,V->VariableReference});
        if(!Current())return false;
        const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!Plan->TryGetArrayField(TEXT("native_value_member_nodes"),Rows))return false;
        return Rows->ContainsByPredicate([&](const TSharedPtr<FJsonValue>& Value){const auto J=Value->AsObject();return J.IsValid()&&J->GetStringField(TEXT("object_path"))==N->GetPathName()&&J->GetStringField(TEXT("value_pin_name"))==P->PinName.ToString()&&J->GetStringField(TEXT("value_pin_id"))==P->PinId.ToString(EGuidFormats::Digits);});
    };
    const auto Plain=[](const FEdGraphPinType& T){return T.PinSubCategory.IsNone()&&!T.bIsWeakPointer&&!T.bIsUObjectWrapper&&!T.bSerializeAsSinglePrecisionFloat&&!T.PinValueType.bTerminalIsConst&&!T.PinValueType.bTerminalIsWeakPointer&&!T.PinValueType.bTerminalIsUObjectWrapper;};
    const auto MapType=[&](const FEdGraphPinType& T){return Plain(T)&&T.IsMap()&&T.PinCategory==UEdGraphSchema_K2::PC_String&&T.PinSubCategoryObject==nullptr&&(DoubleMap?(T.PinValueType.TerminalCategory==UEdGraphSchema_K2::PC_Real&&T.PinValueType.TerminalSubCategory==UEdGraphSchema_K2::PC_Double&&T.PinValueType.TerminalSubCategoryObject==nullptr):(T.PinValueType.TerminalCategory==UEdGraphSchema_K2::PC_Struct&&T.PinValueType.TerminalSubCategory.IsNone()&&T.PinValueType.TerminalSubCategoryObject==Vector));};
    const auto ResultInput=[&](UK2Node* N,UEdGraphPin* P)
    {
        if(!DoubleMap||N->GetClass()!=UK2Node_FunctionResult::StaticClass()||P->Direction!=EGPD_Input||!MapType(P->PinType)||P->PinType.bIsReference||P->PinType.bIsConst)return false;
        auto* ResultNode=static_cast<UK2Node_FunctionResult*>(N);UK2Node_FunctionEntry* Entry=nullptr;
        for(const auto& H:Graph->Nodes){if(!H.IsResolved())return false;if(H!=nullptr){UEdGraphNode* Candidate=H.Get();if(Candidate->GetClass()==UK2Node_FunctionEntry::StaticClass()){if(Entry)return false;Entry=static_cast<UK2Node_FunctionEntry*>(Candidate);}}}
        FMemberReference OwnReference;if(Entry)OwnReference.SetExternalMember(Graph->GetFName(),nullptr,Entry->FunctionReference.GetMemberGuid());
        if(!Entry||!Entry->FunctionReference.IsSameReference(OwnReference)||Entry->FunctionReference.IsSelfContext()||!Node(Entry)||!Current()||!ResultNode->FunctionReference.IsSameReference(Entry->FunctionReference)||ResultNode->FunctionReference.IsDeprecated()||!BP->FunctionGraphs.Contains(Graph)||!BP->ParentClass.GetGCPtr().IsResolved()||!(BP->ParentClass.GetGCPtr()==AActor::StaticClass()||(BP->BlueprintType==BPTYPE_FunctionLibrary&&BP->ParentClass.GetGCPtr()==UBlueprintFunctionLibrary::StaticClass()&&Graph->GetFName()==TEXT("PresetLoader")&&P->PinName==TEXT("PresetMap")&&BP->SimpleConstructionScript.IsResolved()&&BP->SimpleConstructionScript==nullptr&&BP->NewVariables.IsEmpty()&&BP->GeneratedVariables.IsEmpty()&&!LocalRoles.IsEmpty()&&LocalRoles[0]->PresetLibrary)))return false;
        FResultMapRole Role;Role.Node=ResultNode;Role.Entry=Entry;Role.ResultReference=ResultNode->FunctionReference;Role.EntryReference=Entry->FunctionReference;Role.Users=ResultNode->UserDefinedPins;int32 Found=0;
        for(const auto& U:Role.Users){if(!U.IsValid())return false;Role.Copies.Add(*U);if(U->PinName==P->PinName){if(U->DesiredPinDirection!=EGPD_Input||!FLocalDoubleMap::Type(U->PinType)||!U->PinDefaultValue.IsEmpty())return false;++Found;}}
        if(Found!=1)return false;ResultRoles.Add(MoveTemp(Role));R->SetBoolField(TEXT("map_output_result_current_verified"),true);return Current();
    };
    bool Pending=false,TypedPending=false;TArray<UEdGraphPin*> DataPins;
    if(Thunk)
    {
        UEdGraphPin* Map=Call->FindPin(TEXT("TargetMap"),EGPD_Input);UEdGraphPin* Key=(Clear||ArrayResult||Length)?nullptr:Call->FindPin(TEXT("Key"),EGPD_Input);UEdGraphPin* Value=(Clear||KeyOnly||ArrayResult||Length)?nullptr:Call->FindPin(TEXT("Value"),Find?EGPD_Output:EGPD_Input);UEdGraphPin* ArrayOut=ArrayResult?Call->FindPin(Keys?TEXT("Keys"):TEXT("Values"),EGPD_Output):nullptr;
        if(!Map||(!Clear&&!ArrayResult&&!Length&&!Key)||(!Clear&&!KeyOnly&&!ArrayResult&&!Length&&!Value)||(ArrayResult&&!ArrayOut)||Map->LinkedTo.Num()>1)return Stop(TEXT("The exact finite Map pin inventory is required."));
        Pending=Map->LinkedTo.IsEmpty();TypedPending=Pending&&MapType(Map->PinType);DataPins.Add(Map);if(Key)DataPins.Add(Key);if(Value)DataPins.Add(Value);if(ArrayOut)DataPins.Add(ArrayOut);
        for(UEdGraphPin* P:DataPins)
        {
            const bool M=P==Map,K=P==Key,A=P==ArrayOut;const auto& T=P->PinType;
            const bool PinReference=M||K||Add,Const=M||K||Add;
            const bool Category=Pending&&!TypedPending?(T.PinCategory==UEdGraphSchema_K2::PC_Wildcard&&T.PinSubCategoryObject==nullptr&&(!M||(T.PinValueType.TerminalCategory==UEdGraphSchema_K2::PC_Wildcard&&T.PinValueType.TerminalSubCategoryObject==nullptr))):(M?MapType(T):(K||(A&&Keys))?(T.PinCategory==UEdGraphSchema_K2::PC_String&&T.PinSubCategoryObject==nullptr):(DoubleMap?(T.PinCategory==UEdGraphSchema_K2::PC_Real&&T.PinSubCategory==UEdGraphSchema_K2::PC_Double&&T.PinSubCategoryObject==nullptr):(T.PinCategory==UEdGraphSchema_K2::PC_Struct&&T.PinSubCategoryObject==Vector)));
            if((KeyOnly||ArrayResult)&&(P->bHidden||P->bNotConnectable||P->bDefaultValueIsReadOnly||P->bAdvancedView||P->bDeprecated))return Stop(TEXT("Key-only Map data pins must be visible whole stock inputs."));
            if(!(DoubleMap&&(!Pending||TypedPending)&&!M&&!K&&(!A||!Keys)?(T.PinSubCategory==UEdGraphSchema_K2::PC_Double&&!T.bIsWeakPointer&&!T.bIsUObjectWrapper&&!T.bSerializeAsSinglePrecisionFloat&&!T.PinValueType.bTerminalIsConst&&!T.PinValueType.bTerminalIsWeakPointer&&!T.PinValueType.bTerminalIsUObjectWrapper):Plain(T))||(!M&&T.PinValueType!=FEdGraphTerminalType())||T.bIsReference!=PinReference||T.bIsConst!=Const||T.ContainerType!=(M?EPinContainerType::Map:A?EPinContainerType::Array:EPinContainerType::None)||!Category||P->bDefaultValueIsIgnored!=M||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty()||(Pending&&!P->LinkedTo.IsEmpty()))return Stop(TEXT("Only initial wildcard or connected exact String/Vector Map pin roles are admitted."));
        }
        if(TypedPending){
            if(Clear||ArrayResult||Length||!Key||(!Find&&!KeyOnly&&!Value)||!Map->DefaultValue.IsEmpty()||!Map->AutogeneratedDefaultValue.IsEmpty())return Stop(TEXT("Unlinked typed Map literal profile requires Key-bearing existing operation and an empty ignored TargetMap default."));
            // Type shape above is exact and every data pin is still unlinked.
            // Empty Key or zero Value are valid literals even when equal to Auto.
            for(UEdGraphPin* P:DataPins){if(P==Map||P->Direction!=EGPD_Input)continue;
                const FString DefaultError=Schema->IsCurrentPinDefaultValid(P);if(!Current())return Stop(TEXT("Typed unlinked Map literal roles changed during schema validation."));
                if(!DefaultError.IsEmpty())return Stop(TEXT("Typed unlinked Map Key/Value literal failed current schema validation."));
            }
        }
        if(ThunkSource&&Pending)return Stop(TEXT("Array output requires an actual typed TargetMap link before consumer wiring."));
        if(Pending&&TargetPinName!=TEXT("TargetMap")&&Input->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec)return Stop(TEXT("Connect the exact own Map producer to TargetMap before Key or Value."));
        if(!ThunkSource&&TargetPinName==TEXT("TargetMap")&&(!MapType(Output->PinType)||!Own(Source,false,Output)))return Stop(TEXT("TargetMap requires the current exact own String/Vector Map getter producer."));
        if((KeyOnly||ArrayResult||Length)&&!Pending&&(ThunkSource||TargetPinName!=TEXT("TargetMap"))){
            UEdGraphPin* Peer=Map->LinkedTo[0];UEdGraphNode* Producer=Peer?Peer->GetOwningNodeUnchecked():nullptr;
            if(!Producer||Producer->GetClass()!=UK2Node_VariableGet::StaticClass()||!MapType(Peer->PinType)||!Own(static_cast<UK2Node*>(Producer),false,Peer))return Stop(TEXT("A current exact own String-Vector or own/local String-double getter must remain on TargetMap before key/exec wiring."));
        }
        if(ArrayResult){
            if(Call->Pins.Num()!=5||Call->IsNodePure()||Call->FindPin(TEXT("Key"))||Call->FindPin(TEXT("Value"))||Call->FindPin(TEXT("ReturnValue")))return Stop(TEXT("Map Keys/Values require whole5 imperative void/array-output shape."));
            FEdGraphPinType SelfType;SelfType.PinCategory=UEdGraphSchema_K2::PC_Object;SelfType.PinSubCategoryObject=Owner;const auto* Self=Call->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
            if(!Self||!SameType(Self->PinType,SelfType)||!Self->bHidden||!Self->DefaultObject.IsResolved()||Self->DefaultObject!=KeyCDO||!Self->LinkedTo.IsEmpty())return Stop(TEXT("Array-output Map hidden static self differs."));
            FEdGraphPinType Exec;Exec.PinCategory=UEdGraphSchema_K2::PC_Exec;for(const FName Name:{UEdGraphSchema_K2::PN_Execute,UEdGraphSchema_K2::PN_Then}){const auto* P=Call->FindPin(Name,Name==UEdGraphSchema_K2::PN_Execute?EGPD_Input:EGPD_Output);if(!P||!SameType(P->PinType,Exec)||P->bHidden||P->bNotConnectable)return Stop(TEXT("Keys/Values require the exact imperative exec pair."));}
            if(ArraySource){FEdGraphPinType Expected=ArrayOut->PinType;Expected.bIsReference=Input->PinType.bIsReference;Expected.bIsConst=Input->PinType.bIsConst;if(Output!=ArrayOut||!SameType(Input->PinType,Expected))return Stop(TEXT("Typed map array output requires an exact matching Array consumer; ref/const remain schema endpoint roles."));}
        }
        if(KeyOnly){
            if(Call->Pins.Num()!=(Contains?4:6)||Call->IsNodePure()!=Contains||Call->FindPin(TEXT("Value")))return Stop(TEXT("Key-only Map full pin count/purity/no-Value differs."));
            FEdGraphPinType SelfType;SelfType.PinCategory=UEdGraphSchema_K2::PC_Object;SelfType.PinSubCategoryObject=Owner;const auto* Self=Call->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
            if(!Self||!SameType(Self->PinType,SelfType)||!Self->bHidden||!Self->DefaultObject.IsResolved()||Self->DefaultObject!=KeyCDO||!Self->LinkedTo.IsEmpty())return Stop(TEXT("Key-only Map hidden static self differs."));
            if(Remove){FEdGraphPinType Exec;Exec.PinCategory=UEdGraphSchema_K2::PC_Exec;for(const FName Name:{UEdGraphSchema_K2::PN_Execute,UEdGraphSchema_K2::PN_Then}){const auto* P=Call->FindPin(Name,Name==UEdGraphSchema_K2::PN_Execute?EGPD_Input:EGPD_Output);if(!P||!SameType(P->PinType,Exec)||P->bHidden||P->bNotConnectable)return Stop(TEXT("Remove requires the exact imperative exec pair."));}}
        }
        if(Length){
            if(Call->Pins.Num()!=3||!Call->IsNodePure()||Call->FindPin(TEXT("Key"))||Call->FindPin(TEXT("Value"))||Call->FindPin(UEdGraphSchema_K2::PN_Execute)||Call->FindPin(UEdGraphSchema_K2::PN_Then))return Stop(TEXT("Map Length requires pure whole3 TargetMap/self/int ReturnValue with no key/value/exec."));
            FEdGraphPinType LengthSelf;LengthSelf.PinCategory=UEdGraphSchema_K2::PC_Object;LengthSelf.PinSubCategoryObject=Owner;const auto* HiddenSelf=Call->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
            if(!HiddenSelf||!SameType(HiddenSelf->PinType,LengthSelf)||!HiddenSelf->bHidden||!HiddenSelf->DefaultObject.IsResolved()||HiddenSelf->DefaultObject!=KeyCDO||!HiddenSelf->LinkedTo.IsEmpty())return Stop(TEXT("Map Length hidden static self differs."));
            FEdGraphPinType LengthResult;LengthResult.PinCategory=UEdGraphSchema_K2::PC_Int;const auto* ActualResult=Call->FindPin(TEXT("ReturnValue"),EGPD_Output);
            if(!ActualResult||!SameType(ActualResult->PinType,LengthResult)||ActualResult->bHidden||ActualResult->bNotConnectable||ActualResult->bDefaultValueIsIgnored||ActualResult->bDefaultValueIsReadOnly||ActualResult->bAdvancedView||ActualResult->bDeprecated||ActualResult->DefaultObject!=nullptr||!ActualResult->DefaultTextValue.IsEmpty()||ActualResult->DefaultValue!=TEXT("0")||ActualResult->AutogeneratedDefaultValue!=TEXT("0"))return Stop(TEXT("Map Length actual int ReturnValue role/default differs."));
            if(LengthSource){FEdGraphPinType Endpoint=ActualResult->PinType;Endpoint.bIsReference=Input->PinType.bIsReference;Endpoint.bIsConst=Input->PinType.bIsConst;if(Output!=ActualResult||!SameType(Input->PinType,Endpoint))return Stop(TEXT("Map Length output requires an exact scalar int consumer; ref/const remain endpoint roles."));}
        }
        if(Find||KeyOnly){auto* B=Call->FindPin(TEXT("ReturnValue"),EGPD_Output);if(!B||B->PinType.PinCategory!=UEdGraphSchema_K2::PC_Boolean||B->PinType.IsContainer()||!Plain(B->PinType)||!B->PinType.PinSubCategory.IsNone()||B->PinType.PinSubCategoryObject!=nullptr||B->PinType.PinValueType!=FEdGraphTerminalType()||B->PinType.bIsReference||B->PinType.bIsConst)return Stop(TEXT("Map Find requires its exact Boolean ReturnValue output."));}
    }
    else if(!MapType(Output->PinType)||!MapType(Input->PinType)||!Own(Source,false,Output)||!(Target->GetClass()==UK2Node_FunctionResult::StaticClass()?ResultInput(Target,Input):Own(Target,true,Input)))return Stop(TEXT("Map assignment requires current exact own Map getter and setter value pins."));
    if(KeyOnly){
        auto* B=Call->FindPin(TEXT("ReturnValue"),EGPD_Output);
        if(!B||B->bHidden||B->bNotConnectable||B->bDefaultValueIsIgnored||B->bDefaultValueIsReadOnly||B->bAdvancedView||B->bDeprecated||B->DefaultObject!=nullptr||!B->DefaultTextValue.IsEmpty()||!B->DefaultValue.Equals(TEXT("false"),ESearchCase::CaseSensitive)||!B->AutogeneratedDefaultValue.Equals(TEXT("false"),ESearchCase::CaseSensitive))return Stop(TEXT("Key-only Map bool output defaults/flags differ."));
        if(Input->PinName==TEXT("Key")){FEdGraphPinType KeyType=Input->PinType;KeyType.bIsReference=Output->PinType.bIsReference;KeyType.bIsConst=Output->PinType.bIsConst;if(!SameType(Output->PinType,KeyType))return Stop(TEXT("Key-only Map Key requires the exact scalar String wire type; source ref/const are schema roles."));}
    }
    if(DoubleMap&&Input->PinName==TEXT("Value")&&Input->PinType.PinCategory==UEdGraphSchema_K2::PC_Real&&(Output->PinType.PinCategory!=UEdGraphSchema_K2::PC_Real||Output->PinType.PinSubCategory!=UEdGraphSchema_K2::PC_Double||Output->PinType.IsContainer()||Output->PinType.PinSubCategoryObject!=nullptr))return Stop(TEXT("Double Map Value requires an exact scalar double wire; no float coercion is admitted."));
    if(!Current())return Stop(TEXT("Map roles changed after own-member resolution."));
    const UEdGraphSchema* ReturnedSchema=Graph->GetSchema();if(!Current()||ReturnedSchema!=Schema)return Stop(TEXT("Graph schema changed on its public getter boundary."));
    const auto Plan=Schema->CanCreateConnection(Output,Input);if(!Current())return Stop(TEXT("Map roles changed after the schema decision."));
    O->SetBoolField(TEXT("eligible"),true);O->SetBoolField(TEXT("native_source_current_verified"),true);O->SetStringField(TEXT("node_path"),(ThunkSource?Source:Target)->GetPathName());O->SetStringField(TEXT("node_guid"),(ThunkSource?Source:Target)->NodeGuid.ToString(EGuidFormats::Digits));O->SetStringField(TEXT("function_name"),Thunk?FunctionName.ToString():FString());O->SetStringField(TEXT("operation"),Clear?TEXT("clear"):Find?TEXT("find"):Add?TEXT("add"):Contains?TEXT("contains"):Remove?TEXT("remove"):Length?TEXT("length"):ArrayResult?(Keys?TEXT("keys"):TEXT("values")):TEXT("assignment"));O->SetBoolField(TEXT("specialization_pending"),Pending);if(TypedPending)O->SetBoolField(TEXT("typed_unlinked_map_profile"),true);O->SetBoolField(TEXT("specialized_from_actual_target_link"),Thunk&&!Pending);O->SetBoolField(TEXT("first_target_map_connection"),Thunk&&Pending&&!ThunkSource&&TargetPinName==TEXT("TargetMap"));O->SetObjectField(TEXT("native_function"),FunctionBefore);
    const auto FullType=[](const FEdGraphPinType& T){auto J=PinTypeObservation(T);J->SetBoolField(TEXT("is_weak_pointer"),T.bIsWeakPointer);J->SetBoolField(TEXT("is_uobject_wrapper"),T.bIsUObjectWrapper);J->SetBoolField(TEXT("serialize_single_precision"),T.bSerializeAsSinglePrecisionFloat);auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("category"),T.PinValueType.TerminalCategory.ToString());V->SetStringField(TEXT("subcategory"),T.PinValueType.TerminalSubCategory.ToString());V->SetStringField(TEXT("type_object"),T.PinValueType.TerminalSubCategoryObject.IsValid()?T.PinValueType.TerminalSubCategoryObject->GetPathName():FString());V->SetBoolField(TEXT("is_const"),T.PinValueType.bTerminalIsConst);V->SetBoolField(TEXT("is_weak_pointer"),T.PinValueType.bTerminalIsWeakPointer);V->SetBoolField(TEXT("is_uobject_wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);J->SetObjectField(TEXT("terminal"),V);return J;};
    TArray<TSharedPtr<FJsonValue>> Shape;
    for(const auto& V:Pins)if(V.N==(ThunkSource?Source:Target))
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),V.Name.ToString());J->SetStringField(TEXT("direction"),V.Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetStringField(TEXT("pin_id"),V.Id.ToString(EGuidFormats::Digits));J->SetNumberField(TEXT("flags"),V.Flags);J->SetStringField(TEXT("raw_default_value"),V.Default);J->SetStringField(TEXT("autogenerated_default_value"),V.Auto);J->SetStringField(TEXT("default_object_path"),V.Object==nullptr?FString():V.Object.GetPathName());J->SetBoolField(TEXT("default_text_empty"),V.Text.IsEmpty());J->SetObjectField(TEXT("pin_type"),FullType(V.Type));
        // Export may call reflected accessors; consume only copied text after a complete current barrier.
        FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&V.Type,&V.Type,nullptr,PPF_None,nullptr);if(!Current())return Stop(TEXT("Map roles changed after copied pin type export."));J->SetStringField(TEXT("pin_type_text"),TypeText);const FString Display=UEdGraphSchema_K2::TypeToText(V.Type).ToString();if(!Current())return Stop(TEXT("Map roles changed after copied pin type display."));J->SetStringField(TEXT("pin_type_display"),Display);if(DataPins.Contains(V.P))J->SetStringField(TEXT("projected_category"),V.Type.IsArray()?TEXT("array"):V.Type.IsMap()?(V.Type.PinCategory==UEdGraphSchema_K2::PC_String?TEXT("object"):TEXT("unknown")):V.Type.PinCategory==UEdGraphSchema_K2::PC_String?TEXT("string"):V.Type.PinCategory==UEdGraphSchema_K2::PC_Struct?TEXT("object"):V.Type.PinCategory==UEdGraphSchema_K2::PC_Real?TEXT("number"):TEXT("integer"));Shape.Add(MakeShared<FJsonValueObject>(J));
    }
    O->SetArrayField(TEXT("pins"),Shape);O->SetBoolField(TEXT("runtime_map_operation_verified"),false);
    if(Length){O->SetBoolField(TEXT("length_contract_verified"),true);O->SetBoolField(TEXT("map_value_pin_absent"),true);O->SetBoolField(TEXT("pure"),true);O->SetStringField(TEXT("map_connection_role"),LengthSource?TEXT("scalar_output"):TEXT("target_input"));}
    if(KeyOnly){O->SetBoolField(TEXT("key_only_contract_verified"),true);O->SetBoolField(TEXT("map_value_pin_absent"),true);O->SetBoolField(TEXT("pure"),Contains);}
    if(ArrayResult){O->SetBoolField(TEXT("array_output_contract_verified"),true);O->SetStringField(TEXT("array_output_name"),Keys?TEXT("Keys"):TEXT("Values"));O->SetStringField(TEXT("map_connection_role"),ArraySource?TEXT("array_output"):TEXT("target_input"));O->SetBoolField(TEXT("pure"),false);}
    auto Native=MakeShared<FJsonObject>();Native->SetBoolField(TEXT("observations_complete"),true);Native->SetBoolField(TEXT("native_identity_verified"),true);Native->SetStringField(TEXT("struct_path"),DoubleMap?FString():Vector->GetPathName());Native->SetStringField(TEXT("struct_flags_string"),LexToString(DoubleMap?0U:uint32(Vector->StructFlags)));Native->SetNumberField(TEXT("size"),DoubleMap?sizeof(double):Vector->GetStructureSize());Native->SetNumberField(TEXT("alignment"),DoubleMap?alignof(double):Vector->GetMinAlignment());if(DoubleMap){Native->SetStringField(TEXT("scalar_property_class"),TEXT("DoubleProperty"));Native->SetStringField(TEXT("scalar_category"),TEXT("real"));Native->SetStringField(TEXT("scalar_subcategory"),TEXT("double"));}TArray<TSharedPtr<FJsonValue>> AxisRows;for(const auto& A:Axes){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),A.Name.ToString());J->SetStringField(TEXT("flags_string"),LexToString(A.Flags));J->SetNumberField(TEXT("offset"),A.Offset);J->SetNumberField(TEXT("size"),A.Size);AxisRows.Add(MakeShared<FJsonValueObject>(J));}Native->SetArrayField(TEXT("fields"),AxisRows);
    TArray<TSharedPtr<FJsonValue>> ParameterRows;for(const auto& V:ThunkFields){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),V.Name.ToString());J->SetStringField(TEXT("field_class"),V.Class->GetName());J->SetStringField(TEXT("owner"),V.ArrayParent?V.ArrayParent->GetName():V.Parent?V.Parent->GetName():Function->GetName());J->SetStringField(TEXT("flags_string"),LexToString(V.Flags));J->SetNumberField(TEXT("offset"),V.Offset);J->SetNumberField(TEXT("size"),V.Size);J->SetNumberField(TEXT("array_dim"),V.Dim);ParameterRows.Add(MakeShared<FJsonValueObject>(J));}Native->SetArrayField(TEXT("parameters"),ParameterRows);
    if(KeyOnly||ArrayResult||Length){auto M=MakeShared<FJsonObject>();for(int32 I=0;I<KeyMetadata.Num();++I){auto Row=MakeShared<FJsonObject>();Row->SetBoolField(TEXT("present"),KeyMetadataPresent[I]);Row->SetStringField(TEXT("value"),KeyMetadata[I].Value);M->SetObjectField(KeyMetadata[I].Key.ToString(),Row);}Native->SetObjectField(TEXT("function_metadata"),M);Native->SetNumberField(TEXT("function_size"),KeyFunctionSize);Native->SetNumberField(TEXT("parameter_count"),ArrayResult?ArrayParameters.Num():KeyParameters.Num());if(ArrayResult){Native->SetStringField(TEXT("array_output_property"),OutArray->GetPathName());Native->SetStringField(TEXT("array_inner_property"),OutInner->GetPathName());Native->SetStringField(TEXT("array_inner_class"),TEXT("IntProperty"));Native->SetBoolField(TEXT("array_inner_direct_owner_slot_verified"),true);}}
    if(Thunk){Native->SetStringField(TEXT("owner_class_path"),Owner->GetPathName());Native->SetStringField(TEXT("owner_class_flags_string"),LexToString(uint32(Owner->GetClassFlags())));Native->SetNumberField(TEXT("owner_class_size"),Owner->GetStructureSize());}O->SetObjectField(TEXT("native_source"),Native);
    R->SetBoolField(TEXT("direct_connection_allowed"),Plan.Response==CONNECT_RESPONSE_MAKE);R->SetNumberField(TEXT("schema_response_code"),int32(Plan.Response));R->SetStringField(TEXT("schema_response_name"),Connection::SchemaResponseName(Plan.Response));R->SetBoolField(TEXT("exact_standard_k2_schema"),true);R->SetStringField(TEXT("schema_message"),Plan.Message.ToString());R->SetBoolField(TEXT("already_linked"),Output->LinkedTo.Contains(Input)&&Input->LinkedTo.Contains(Output));R->SetNumberField(TEXT("source_link_count"),Output->LinkedTo.Num());R->SetNumberField(TEXT("target_link_count"),Input->LinkedTo.Num());R->SetBoolField(TEXT("type_exactly_equal"),SameType(Output->PinType,Input->PinType));R->SetObjectField(TEXT("source_pin_type"),PinTypeObservation(Output->PinType));R->SetObjectField(TEXT("target_pin_type"),PinTypeObservation(Input->PinType));
    auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("raw_default_value"),Input->DefaultValue);D->SetStringField(TEXT("autogenerated_default_value"),Input->AutogeneratedDefaultValue);D->SetStringField(TEXT("default_object_path"),Input->DefaultObject==nullptr?FString():Input->DefaultObject.GetPathName());D->SetBoolField(TEXT("default_text_empty"),Input->DefaultTextValue.IsEmpty());D->SetBoolField(TEXT("is_orphaned"),Input->bOrphanedPin);D->SetBoolField(TEXT("default_value_is_ignored"),Input->bDefaultValueIsIgnored);D->SetBoolField(TEXT("default_value_is_read_only"),Input->bDefaultValueIsReadOnly);R->SetObjectField(TEXT("target_pin_default_state"),D);
    if(!Current())return Stop(TEXT("Map roles changed before observation completion."));R->SetBoolField(TEXT("dirty_after"),HostDirty);R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return R;
}

TSharedRef<FJsonObject> ReadConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName)
{
    auto R = Result();
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())
    {Fail(R,TEXT("graph_editor_state_invalid"),TEXT("Connection planning refuses import/exit/purge/GC/saving before the first loaded lookup."));return R;}
    UEdGraph* Graph = ResolveLoadedGraph(GraphPath, R); if (!Graph) { return R; }
    if (!SafeSelector(SourceNodePath) || !SafeSelector(TargetNodePath) || !SafeSelector(SourcePinName) || !SafeSelector(TargetPinName))
    { Fail(R, TEXT("graph_pin_selector_invalid"), TEXT("Node paths and pin names must be nonempty bounded selectors without control characters.")); return R; }
    UK2Node* Source = FindObject<UK2Node>(nullptr, *SourceNodePath);
    UK2Node* Target = FindObject<UK2Node>(nullptr, *TargetNodePath);
    // ML: route only the exact existing RowNames producer/pin pair. No tracked
    // member-parent or macro-reference getter is used before the Loop current scope.
    if(IsValid(Source)&&IsValid(Target)&&Source->GetClass()==UK2Node_CallFunction::StaticClass()
        &&Target->GetClass()==UK2Node_MacroInstance::StaticClass()
        &&FName(*SourcePinName)==TEXT("OutRowNames")&&FName(*TargetPinName)==TEXT("Array"))
    {
        FMemberReference ExpectedRowNames;ExpectedRowNames.SetExternalMember(TEXT("GetDataTableRowNames"),UDataTableFunctionLibrary::StaticClass());
        const auto& Reference=static_cast<UK2Node_CallFunction*>(Source)->FunctionReference;
        if(!Reference.IsDeprecated()&&Reference.IsSameReference(ExpectedRowNames))
        {
            const auto NamePlan=Loop::ReadNameForEachConnection(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);
            bool Applicable=true,Success=false,Current=false;FString ErrorCode;
            const bool NonStock=NamePlan->TryGetBoolField(TEXT("applicable"),Applicable)&&!Applicable;
            bool ReadOnly=true;for(const TCHAR* Key:{TEXT("mutation_attempted"),TEXT("saved"),TEXT("compiled"),TEXT("loaded_during_read")})
            {bool Value=true;ReadOnly=NamePlan->TryGetBoolField(Key,Value)&&!Value&&ReadOnly;}
            NamePlan->TryGetStringField(TEXT("error_code"),ErrorCode);
            const bool ContinueGeneric=NonStock&&ReadOnly&&ErrorCode.IsEmpty()
                &&NamePlan->TryGetBoolField(TEXT("success"),Success)&&Success
                &&NamePlan->TryGetBoolField(TEXT("current_verified"),Current)&&Current;
            // Only an explicitly fenced non-stock result continues the legacy path.
            // Failure/unknown/incomplete payload returns immediately, with no next SDK.
            if(!ContinueGeneric)return NamePlan;
            R->SetObjectField(TEXT("name_foreach_nonstock_dispatch"),NamePlan);
        }
    }
    // MR: only an already typed native Name MakeArray output opts into the
    // existing finite Name ForEach reader/writer. Other producers stay generic.
    if(IsValid(Source)&&IsValid(Target)&&Source->GetClass()==UK2Node_MakeArray::StaticClass()
        &&Target->GetClass()==UK2Node_MacroInstance::StaticClass()
        &&FName(*SourcePinName)==TEXT("Array")&&FName(*TargetPinName)==TEXT("Array"))
    {
        const auto* OutputArray=Source->FindPin(TEXT("Array"),EGPD_Output);
        if(OutputArray&&OutputArray->PinType.IsArray()&&OutputArray->PinType.PinCategory==UEdGraphSchema_K2::PC_Name)
        {
            const auto NamePlan=Loop::ReadNameForEachConnection(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);
            bool Applicable=true,Success=false,Current=false;FString ErrorCode;
            const bool NonStock=NamePlan->TryGetBoolField(TEXT("applicable"),Applicable)&&!Applicable;
            bool ReadOnly=true;for(const TCHAR* Key:{TEXT("mutation_attempted"),TEXT("saved"),TEXT("compiled"),TEXT("loaded_during_read")})
            {bool Value=true;ReadOnly=NamePlan->TryGetBoolField(Key,Value)&&!Value&&ReadOnly;}
            NamePlan->TryGetStringField(TEXT("error_code"),ErrorCode);
            const bool ContinueGeneric=NonStock&&ReadOnly&&ErrorCode.IsEmpty()&&NamePlan->TryGetBoolField(TEXT("success"),Success)&&Success&&NamePlan->TryGetBoolField(TEXT("current_verified"),Current)&&Current;
            if(!ContinueGeneric)return NamePlan;
            R->SetObjectField(TEXT("name_make_array_foreach_nonstock_dispatch"),NamePlan);
        }
    }
    // MO: only inspect macro references inside the own PresetLoader BFL target.
    // The finite reader fences public reference reads and explicitly hands non-stock
    // combinations back to the unchanged generic route.
    if(IsValid(Source)&&IsValid(Target)&&(Source->GetClass()==UK2Node_MacroInstance::StaticClass()||Target->GetClass()==UK2Node_MacroInstance::StaticClass())
        &&Graph->GetOuter()&&Graph->GetOuter()->GetClass()==UBlueprint::StaticClass()
        &&static_cast<UBlueprint*>(Graph->GetOuter())->BlueprintType==BPTYPE_FunctionLibrary&&Graph->GetFName()==TEXT("PresetLoader"))
    {
        const auto MacroPlan=Function::ReadIsValidExecMacroConnection(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);
        bool Applicable=true,Success=false,Current=false;FString ErrorCode;
        const bool NonStock=MacroPlan->TryGetBoolField(TEXT("applicable"),Applicable)&&!Applicable;
        bool ReadOnly=true;for(const TCHAR* Key:{TEXT("mutation_attempted"),TEXT("saved"),TEXT("compiled"),TEXT("loaded_during_read")})
        {bool Value=true;ReadOnly=MacroPlan->TryGetBoolField(Key,Value)&&!Value&&ReadOnly;}
        MacroPlan->TryGetStringField(TEXT("error_code"),ErrorCode);
        if(!(NonStock&&ReadOnly&&ErrorCode.IsEmpty()&&MacroPlan->TryGetBoolField(TEXT("success"),Success)&&Success&&MacroPlan->TryGetBoolField(TEXT("current_verified"),Current)&&Current))return MacroPlan;
        R->SetObjectField(TEXT("is_valid_exec_macro_nonstock_dispatch"),MacroPlan);
    }
    if(IsValid(Source)&&IsValid(Target))
    {
        const UK2Node_CallFunction* MapCall=Target->GetClass()==UK2Node_CallFunction::StaticClass()?static_cast<UK2Node_CallFunction*>(Target):nullptr;
        const FName MapName=MapCall?MapCall->FunctionReference.GetMemberName():NAME_None;
        const UEdGraphPin* Out=Source->FindPin(FName(*SourcePinName),EGPD_Output);const UEdGraphPin* In=Target->FindPin(FName(*TargetPinName),EGPD_Input);
        const UK2Node_CallFunction* MapArraySource=Source->GetClass()==UK2Node_CallFunction::StaticClass()?static_cast<UK2Node_CallFunction*>(Source):nullptr;const FName MapArrayName=MapArraySource?MapArraySource->FunctionReference.GetMemberName():NAME_None;
        const bool MapArrayOutput=(MapArrayName==TEXT("Map_Keys")&&FName(*SourcePinName)==TEXT("Keys"))||(MapArrayName==TEXT("Map_Values")&&FName(*SourcePinName)==TEXT("Values"));
        const bool MapLengthOutput=MapArrayName==TEXT("Map_Length")&&FName(*SourcePinName)==TEXT("ReturnValue");
        if(MapArrayOutput||MapLengthOutput||(Out&&Out->PinType.IsMap())||(In&&In->PinType.IsMap())||(MapCall&&(MapName==TEXT("Map_Clear")||MapName==TEXT("Map_Find")||MapName==TEXT("Map_Add")||MapName==TEXT("Map_Contains")||MapName==TEXT("Map_Remove")||MapName==TEXT("Map_Keys")||MapName==TEXT("Map_Values")||MapName==TEXT("Map_Length"))))
        {return ReadStringVectorMapConnection(Graph,Source,Target,SourcePinName,TargetPinName,R);}
    }
    if(IsValid(Source)&&IsValid(Target)&&(Source->GetClass()==UK2Node_Select::StaticClass()||Target->GetClass()==UK2Node_Select::StaticClass()))
    {return Function::ReadDataTableBoolSelectConnection(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);}
    if(IsValid(Source)&&IsValid(Target)&&((Source->GetClass()==UK2Node_BreakStruct::StaticClass()&&static_cast<UK2Node_BreakStruct*>(Source)->StructType.IsResolved()&&static_cast<UK2Node_BreakStruct*>(Source)->StructType==FAssetData::StaticStruct())||(Target->GetClass()==UK2Node_BreakStruct::StaticClass()&&static_cast<UK2Node_BreakStruct*>(Target)->StructType.IsResolved()&&static_cast<UK2Node_BreakStruct*>(Target)->StructType==FAssetData::StaticStruct())))
    {return Function::ReadAssetDataBreakConnection(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);}
    if(IsValid(Source)&&IsValid(Target)&&((Source->GetClass()==UK2Node_GetDataTableRow::StaticClass()&&Target->GetClass()==UK2Node_BreakStruct::StaticClass()&&FName(*SourcePinName)==UEdGraphSchema_K2::PN_ReturnValue)||(Target->GetClass()==UK2Node_GetDataTableRow::StaticClass()&&FName(*TargetPinName)==TEXT("DataTable"))))
    {return DataTable::ReadStringRowConnection(GraphPath,SourceNodePath,SourcePinName,TargetNodePath,TargetPinName);}
    if (!Source || !Target || Source->GetGraph() != Graph || Target->GetGraph() != Graph || !Graph->Nodes.Contains(Source) || !Graph->Nodes.Contains(Target))
    { Fail(R, TEXT("graph_node_not_found"), TEXT("Both exact loaded K2 nodes must belong to the requested graph.")); return R; }
    // MP: fixed typed Name array endpoints retain the ordinary connection DTO
    // and schema writer. The exact target/source lifetime remains held until Complete.
    Function::FResult MPResult;TSharedPtr<Function::FPresetLoaderNameArrayScope> MPNameArray;
    if(Source->GetClass()==UK2Node_MakeArray::StaticClass()&&Target->GetClass()==UK2Node_CallArrayFunction::StaticClass()
        &&FName(*SourcePinName)==TEXT("Array")&&FName(*TargetPinName)==TEXT("TargetArray")
        &&Graph->GetOuter()&&Graph->GetOuter()->GetClass()==UBlueprint::StaticClass()
        &&static_cast<UBlueprint*>(Graph->GetOuter())->BlueprintType==BPTYPE_FunctionLibrary)
    {
        FMemberReference Length;Length.SetExternalMember(TEXT("Array_Length"),UKismetArrayLibrary::StaticClass());
        const auto* Array=Source->FindPin(TEXT("Array"),EGPD_Output);const auto* InputArray=Target->FindPin(TEXT("TargetArray"),EGPD_Input);
        if(static_cast<UK2Node_CallArrayFunction*>(Target)->FunctionReference.IsSameReference(Length)
            &&Array&&InputArray&&Array->PinType.PinCategory==UEdGraphSchema_K2::PC_Name&&Array->PinType.IsArray()
            &&InputArray->PinType.PinCategory==UEdGraphSchema_K2::PC_Name&&InputArray->PinType.IsArray())
        {
            MPNameArray=Function::HoldPresetLoaderNameArrayConnection(MPResult,Graph,Source,SourcePinName,Target,TargetPinName);
            if(!MPNameArray.IsValid()){R->SetObjectField(TEXT("bfl_name_array_context_failure"),MPResult.Data);Fail(R,TEXT("graph_name_array_bfl_context_invalid"),TEXT("Exact current PresetLoader Name MakeArray/Length roles are unavailable; no next SDK."));return R;}
            R->SetObjectField(TEXT("bfl_name_array_context"),MPResult.Data);
        }
    }
    TSharedPtr<FJsonObject> MJCastContext;
    TStrongObjectPtr<UEdGraph> MJGraph;TStrongObjectPtr<UEdGraphNode> MJSource,MJTarget;TStrongObjectPtr<UBlueprint> MJBlueprint;
    if(Target->GetClass()==UK2Node_DynamicCast::StaticClass()&&Graph->GetOuter()&&Graph->GetOuter()->GetClass()==UBlueprint::StaticClass()&&static_cast<UBlueprint*>(Graph->GetOuter())->BlueprintType==BPTYPE_FunctionLibrary)
    {
        auto* C=static_cast<UK2Node_DynamicCast*>(Target);if(C->TargetType.GetGCPtr().IsResolved()&&C->TargetType.GetGCPtr()==UDataTable::StaticClass())
        {MJGraph.Reset(Graph);MJSource.Reset(Source);MJTarget.Reset(Target);MJBlueprint.Reset(static_cast<UBlueprint*>(Graph->GetOuter()));MJCastContext=Function::ReadPresetLoaderDataTableCastContext(GraphPath,TargetNodePath);R->SetObjectField(TEXT("bfl_target_context"),MJCastContext.ToSharedRef());if(!MJCastContext->GetBoolField(TEXT("success"))){Fail(R,TEXT("graph_datatable_bfl_context_invalid"),TEXT("Exact PresetLoader current context is unavailable; no next SDK."));return R;}}
    }
    auto MJCurrent=[&](){if(MPNameArray.IsValid()&&!MPNameArray->Check()){R->SetObjectField(TEXT("bfl_name_array_context_failure"),MPResult.Data);return Fail(R,TEXT("graph_name_array_bfl_context_changed"),TEXT("Held Name array source/target roles changed during read; no next SDK."));}if(!MJCastContext.IsValid())return true;const auto Now=Function::ReadPresetLoaderDataTableCastContext(GraphPath,TargetNodePath);if(!Now->GetBoolField(TEXT("success"))||!FJsonValue::CompareEqual(FJsonValueObject(MJCastContext.ToSharedRef()),FJsonValueObject(Now))){R->SetObjectField(TEXT("bfl_target_context_failure"),Now);return Fail(R,TEXT("graph_datatable_bfl_context_changed"),TEXT("Exact PresetLoader context or physical graph changed during read; no next SDK."));}return true;};
    UEdGraphPin* Output = Source->FindPin(FName(*SourcePinName), EGPD_Output);
    UEdGraphPin* Input = Target->FindPin(FName(*TargetPinName), EGPD_Input);
    if (!Output || !Input || Output->bOrphanedPin || Input->bOrphanedPin)
    { Fail(R, TEXT("graph_pin_not_found"), TEXT("A non-orphaned output/input pin with the requested name was not found.")); return R; }
    const UEdGraphSchema* MPReturnedSchema=nullptr;if(MPNameArray.IsValid()){MPReturnedSchema=Graph->GetSchema();if(!MJCurrent())return R;}
    const FPinConnectionResponse Plan = MPNameArray.IsValid()?MPReturnedSchema->CanCreateConnection(Output,Input):Graph->GetSchema()->CanCreateConnection(Output, Input);
    if(!MJCurrent())return R;
    // The reflected bool CanCreateConnection also admits conversions and link
    // replacement. Expose the exact public schema decision without executing it.
    R->SetBoolField(TEXT("direct_connection_allowed"), Plan.Response == CONNECT_RESPONSE_MAKE);
    R->SetNumberField(TEXT("schema_response_code"), static_cast<int32>(Plan.Response));
    R->SetStringField(TEXT("schema_response_name"), Connection::SchemaResponseName(Plan.Response));
    R->SetBoolField(TEXT("exact_standard_k2_schema"), Graph->GetSchema()->GetClass() == UEdGraphSchema_K2::StaticClass());
    if(!MJCurrent())return R;R->SetStringField(TEXT("schema_message"), Plan.Message.ToString());
    R->SetBoolField(TEXT("already_linked"), Output->LinkedTo.Contains(Input) && Input->LinkedTo.Contains(Output));
    R->SetNumberField(TEXT("source_link_count"), Output->LinkedTo.Num()); R->SetNumberField(TEXT("target_link_count"), Input->LinkedTo.Num());
    R->SetBoolField(TEXT("type_exactly_equal"), Output->PinType == Input->PinType);
    R->SetObjectField(TEXT("source_pin_type"), PinTypeObservation(Output->PinType));
            if(!MJCurrent())return R;
    R->SetObjectField(TEXT("target_pin_type"), PinTypeObservation(Input->PinType));
            if(!MJCurrent())return R;
    // Public UEdGraphPin state, observed without resetting values or invoking
    // connection callbacks. K2 resets a newly connected input to this default.
    auto DefaultState = MakeShared<FJsonObject>();
    DefaultState->SetStringField(TEXT("raw_default_value"), Input->DefaultValue);
    DefaultState->SetStringField(TEXT("autogenerated_default_value"), Input->AutogeneratedDefaultValue);
    DefaultState->SetStringField(TEXT("default_object_path"), Input->DefaultObject ? Input->DefaultObject->GetPathName() : FString());
            if(!MJCurrent())return R;
    DefaultState->SetBoolField(TEXT("default_text_empty"), Input->DefaultTextValue.IsEmpty());
    DefaultState->SetBoolField(TEXT("is_orphaned"), Input->bOrphanedPin);
    DefaultState->SetBoolField(TEXT("default_value_is_ignored"), Input->bDefaultValueIsIgnored);
    DefaultState->SetBoolField(TEXT("default_value_is_read_only"), Input->bDefaultValueIsReadOnly);
    R->SetObjectField(TEXT("target_pin_default_state"), DefaultState);
    // The installed cast callback can retype only this exact Object input.
    // This observation authorizes no conversion/link operation by itself.
    if (Target->GetClass() == UK2Node_DynamicCast::StaticClass())
    {
        const UK2Node_DynamicCast* CastNode = CastChecked<UK2Node_DynamicCast>(Target);
        bool AllPinsReadable = !CastNode->Pins.IsEmpty();
        for (const UEdGraphPin* Pin : CastNode->Pins)
        { AllPinsReadable &= Pin && !Pin->WasTrashed() && !Pin->bOrphanedPin && Pin->GetOwningNode() == Target; }
        int32 SourcePinCount = 0;
        for (const UEdGraphPin* Pin : CastNode->Pins)
        { if (Pin && Pin->PinName == UEdGraphSchema_K2::PN_ObjectToCast && Pin->Direction == EGPD_Input) { ++SourcePinCount; } }
        if (AllPinsReadable && SourcePinCount == 1 && Input->PinName == UEdGraphSchema_K2::PN_ObjectToCast
            && Input == CastNode->GetCastSourcePin())
        {
            auto Observation = MakeShared<FJsonObject>();
            auto DataTableTargetType=CastNode->TargetType;
            const bool DataTableTarget=DataTableTargetType.GetGCPtr().IsResolved()&&DataTableTargetType.GetGCPtr()==UDataTable::StaticClass();
            const UClass* CastClass = DataTableTarget?UDataTable::StaticClass():CastNode->TargetType.Get();
            const auto NativeBefore = DynamicCastTargetSourceObservation(CastClass);
            if(!MJCurrent())return R;
            // Installed NotifyPinConnectionListChanged leaves a new unlinked MID
            // Object input wildcard/null, then assigns object/UObject on its first link.
            const bool MidUnlinkedWildcard = CastClass == UMaterialInstanceDynamic::StaticClass()
                && CastClass->GetPathName() == TEXT("/Script/Engine.MaterialInstanceDynamic")
                && Input->LinkedTo.IsEmpty() && Input->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard
                && Input->PinType.PinSubCategoryObject.Get() == nullptr;
            const bool DataTableUnlinkedWildcard=DataTableTarget&&Input->LinkedTo.IsEmpty()&&Input->PinType.PinCategory==UEdGraphSchema_K2::PC_Wildcard&&Input->PinType.PinSubCategoryObject.Get()==nullptr;
            const bool ScalarInput = Input->PinType.PinSubCategory.IsNone() && !Input->PinType.IsContainer()
                && !Input->PinType.bIsReference && !Input->PinType.bIsConst && !Input->PinType.bIsWeakPointer
                && !Input->PinType.bIsUObjectWrapper && !Input->PinType.bSerializeAsSinglePrecisionFloat
                && (Input->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard || Input->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
                && (Input->PinType.PinSubCategoryObject.Get() == UObject::StaticClass() || MidUnlinkedWildcard || DataTableUnlinkedWildcard);
            const bool MidTarget = CastClass == UMaterialInstanceDynamic::StaticClass()
                && CastClass->GetPathName() == TEXT("/Script/Engine.MaterialInstanceDynamic");
            const auto MaterialBefore = MidTarget ? MaterialCastSourceObservation(Output->PinType) : MakeShared<FJsonObject>();
            const bool ExactMaterialOutput = MidTarget && Output->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
                && Output->PinType.PinSubCategory.IsNone() && !Output->PinType.IsContainer()
                && !Output->PinType.bIsReference && !Output->PinType.bIsConst && !Output->PinType.bIsWeakPointer
                && !Output->PinType.bIsUObjectWrapper && !Output->PinType.bSerializeAsSinglePrecisionFloat
                && MaterialBefore->GetBoolField(TEXT("native_identity_verified"));
            const bool ScalarOutput = (RuntimeObject::IsSupportedPin(Output->PinType) || ExactMaterialOutput)
                && !Output->PinType.bIsConst && !Output->PinType.bSerializeAsSinglePrecisionFloat;
            bool Reciprocal = Input->LinkedTo.Num() <= 1;
            for (const UEdGraphPin* Linked : Input->LinkedTo)
            { Reciprocal &= Linked && !Linked->WasTrashed() && !Linked->bOrphanedPin && Linked == Output && Linked->LinkedTo.Contains(Input); }
            const bool LinkedSourcePresent = Output->LinkedTo.Contains(Input) && Input->LinkedTo.Contains(Output);
            const bool CategoryState = Input->LinkedTo.IsEmpty() ? Input->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard
                : LinkedSourcePresent && Input->PinType.PinCategory == UEdGraphSchema_K2::PC_Object;
            const bool EmptyDefault = Input->DefaultObject == nullptr && Input->DefaultTextValue.IsEmpty()
                && (Input->DefaultValue.IsEmpty() || Input->DefaultValue == TEXT("None"))
                && (Input->AutogeneratedDefaultValue.IsEmpty() || Input->AutogeneratedDefaultValue == TEXT("None"));
            Observation->SetStringField(TEXT("node_path"), Target->GetPathName());
            Observation->SetStringField(TEXT("node_guid"), Target->NodeGuid.ToString(EGuidFormats::Digits));
            Observation->SetStringField(TEXT("node_class_path"), Target->GetClass()->GetPathName());
            Observation->SetStringField(TEXT("target_class_path"), IsValid(CastClass) ? CastClass->GetPathName() : FString());
            Observation->SetBoolField(TEXT("pure"), CastNode->IsNodePure());
            Observation->SetStringField(TEXT("pin_name"), Input->PinName.ToString());
            Observation->SetStringField(TEXT("pin_id"), Input->PinId.ToString(EGuidFormats::Digits));
            Observation->SetStringField(TEXT("persistent_guid"), Input->PersistentGuid.ToString(EGuidFormats::Digits));
            FString InputTypeText, OutputTypeText;
            FEdGraphPinType::StaticStruct()->ExportText(InputTypeText, &Input->PinType, &Input->PinType, nullptr, PPF_None, nullptr);
            if(!MJCurrent())return R;
            FEdGraphPinType::StaticStruct()->ExportText(OutputTypeText, &Output->PinType, &Output->PinType, nullptr, PPF_None, nullptr);
            if(!MJCurrent())return R;
            Observation->SetStringField(TEXT("pin_type_text"), InputTypeText);
            // Mirrors the official pin display function without creating the
            // transient FProperty used by PinTypeToJsonSchema. The two schema
            // projection tokens below are finite installed-source mappings.
            FEdGraphPinType ExpectedObjectType = Input->PinType;
            ExpectedObjectType.PinCategory = UEdGraphSchema_K2::PC_Object;
            if (MidUnlinkedWildcard || DataTableUnlinkedWildcard) { ExpectedObjectType.PinSubCategoryObject = UObject::StaticClass(); }
            FString ExpectedObjectTypeText;
            FEdGraphPinType::StaticStruct()->ExportText(ExpectedObjectTypeText, &ExpectedObjectType, &ExpectedObjectType, nullptr, PPF_None, nullptr);
            if(!MJCurrent())return R;
            Observation->SetStringField(TEXT("pin_type_display"), UEdGraphSchema_K2::TypeToText(Input->PinType).ToString());
            if(!MJCurrent())return R;
            Observation->SetStringField(TEXT("expected_object_pin_type_text"), ExpectedObjectTypeText);
            Observation->SetStringField(TEXT("expected_object_pin_type_display"), UEdGraphSchema_K2::TypeToText(ExpectedObjectType).ToString());
            if(!MJCurrent())return R;
            Observation->SetStringField(TEXT("projected_category"), Input->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard ? TEXT("integer")
                : Input->PinType.PinCategory == UEdGraphSchema_K2::PC_Object ? TEXT("string") : TEXT(""));
            Observation->SetStringField(TEXT("expected_object_projected_category"), TEXT("string"));
            Observation->SetStringField(TEXT("projected_category_basis"), TEXT("finite installed PinTypeToJsonSchema/CreatePrimitiveProperty/JsonSchemaGenerator scalar wildcard-to-FIntProperty and nonpersistent UObject-to-FObjectProperty mapping; no property allocation dispatched"));
            auto TypeObservation = PinTypeObservation(Input->PinType);
            TypeObservation->SetBoolField(TEXT("is_weak_pointer"), Input->PinType.bIsWeakPointer);
            TypeObservation->SetBoolField(TEXT("is_uobject_wrapper"), Input->PinType.bIsUObjectWrapper);
            TypeObservation->SetBoolField(TEXT("serialize_as_single_precision_float"), Input->PinType.bSerializeAsSinglePrecisionFloat);
            Observation->SetObjectField(TEXT("pin_type"), TypeObservation);
            Observation->SetStringField(TEXT("source_output_node_path"), Source->GetPathName());
            Observation->SetStringField(TEXT("source_output_pin_name"), Output->PinName.ToString());
            Observation->SetStringField(TEXT("source_output_pin_id"), Output->PinId.ToString(EGuidFormats::Digits));
            Observation->SetStringField(TEXT("source_output_pin_type_text"), OutputTypeText);
            Observation->SetBoolField(TEXT("all_pins_readable"), AllPinsReadable);
            Observation->SetBoolField(TEXT("cast_source_pin_identity_verified"), true);
            Observation->SetNumberField(TEXT("linked_source_count"), Input->LinkedTo.Num());
            Observation->SetBoolField(TEXT("all_links_reciprocal"), Reciprocal);
            Observation->SetBoolField(TEXT("source_output_link_present"), LinkedSourcePresent);
            auto CastDefault = MakeShared<FJsonObject>(*DefaultState);
            CastDefault->SetStringField(TEXT("pin_friendly_name"), Input->PinFriendlyName.ToString());
            CastDefault->SetBoolField(TEXT("hidden"), Input->bHidden);
            CastDefault->SetBoolField(TEXT("not_connectable"), Input->bNotConnectable);
            Observation->SetObjectField(TEXT("default_state"), CastDefault);
            const auto NativeAfter = DynamicCastTargetSourceObservation(CastClass);
            if(!MJCurrent())return R;
            const auto MaterialAfter = MidTarget ? MaterialCastSourceObservation(Output->PinType) : MakeShared<FJsonObject>();
            const bool MaterialPreserved = FJsonValue::CompareEqual(FJsonValueObject(MaterialBefore), FJsonValueObject(MaterialAfter));
            if (MidTarget)
            {
                Observation->SetObjectField(TEXT("mid_material_source_before"), MaterialBefore);
                Observation->SetObjectField(TEXT("mid_material_source_after"), MaterialAfter);
                Observation->SetBoolField(TEXT("mid_material_source_preserved"), MaterialPreserved);
            }
            const bool Preserved = FJsonValue::CompareEqual(FJsonValueObject(NativeBefore), FJsonValueObject(NativeAfter));
            Observation->SetObjectField(TEXT("native_source_before"), NativeBefore);
            Observation->SetObjectField(TEXT("native_source_after"), NativeAfter);
            Observation->SetBoolField(TEXT("native_source_preserved"), Preserved);
            Observation->SetBoolField(TEXT("eligible"), Target->NodeGuid.IsValid() && Input->PinId.IsValid()
                && Target->GetOuter() == Graph && ScalarInput && ScalarOutput && CategoryState && EmptyDefault && Reciprocal
                && !Input->bNotConnectable && Input->PinFriendlyName.IsEmpty() && NativeBefore->GetBoolField(TEXT("native_identity_verified"))
                && NativeAfter->GetBoolField(TEXT("native_identity_verified")) && Preserved
                && (!MidTarget || (ExactMaterialOutput && MaterialAfter->GetBoolField(TEXT("native_identity_verified")) && MaterialPreserved)));
            if(MJCastContext.IsValid())Observation->SetObjectField(TEXT("bfl_target_context"),MJCastContext->GetObjectField(TEXT("bfl_target_context")));
            R->SetObjectField(TEXT("dynamic_cast_source_observation"), Observation);
        }
    }

    R->SetStringField(TEXT("scope"), TEXT("current schema decision only; conversion, promotion and link replacement are not dispatched"));
    if(!MJCurrent())return R;Complete(R, Graph);if(!MJCurrent())return R;return R;
}
namespace
{
bool QQFlowRunning=false;
bool QQFlowFail(Function::FResult& R,const TCHAR* Code,const TCHAR* Message)
{R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Message;R.Stage=TEXT("verification");return false;}
bool QQPosition(const FString& Json,int32& X,int32& Y,Function::FResult& R)
{
 TSharedPtr<FJsonValue> V;const auto Reader=TJsonReaderFactory<>::Create(Json);if(Json.Len()>4096||!FJsonSerializer::Deserialize(Reader,V)||!V)return QQFlowFail(R,TEXT("flow_position_invalid"),TEXT("Bounded JSON position required."));
 X=Y=0;if(V->Type==EJson::Null||(V->Type==EJson::Array&&V->AsArray().IsEmpty()))return true;
 if(V->Type!=EJson::Array||V->AsArray().Num()!=2)return QQFlowFail(R,TEXT("flow_position_pending"),TEXT("First position scope accepts null/origin or two finite numbers; string/extra-array coercion remains incomplete."));
 int32 XY[2];for(int32 I=0;I<2;++I){const auto& N=V->AsArray()[I];if(!N||N->Type!=EJson::Number||!FMath::IsFinite(N->AsNumber()))return QQFlowFail(R,TEXT("flow_position_invalid"),TEXT("Finite numeric coordinates required."));const float Quantized=static_cast<float>(N->AsNumber());if(!FMath::IsFinite(Quantized)||Quantized<-1000000.0f||Quantized>1000000.0f)return QQFlowFail(R,TEXT("flow_position_out_of_scope"),TEXT("First shared position scope is within +/-1000000 after original float32 projection."));XY[I]=static_cast<int32>(Quantized);}
 X=XY[0];Y=XY[1];R.Data->SetField(TEXT("requested_position"),V);R.Data->SetStringField(TEXT("position_projection"),TEXT("original number to float32 then int32 toward-zero; bounded first profile, no string coercion"));return true;
}
// QQ exact Branch/Sequence adapter. Captured field comparisons are pure; the
// supplied PT creation fence is called only at SDK/observer return boundaries.
struct FQQFlowContext
{
 Function::FResult& R;TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TArray<TStrongObjectPtr<UObject>> Holds;
 TObjectPtr<UClass> Parent,Generated,Skeleton;TObjectPtr<UObject> GenCDO,SkelCDO;UClass* Meta=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FGuid GraphGuid;FString GraphPath;TObjectPtr<UClass> Schema;TFunction<bool(UEdGraphNode*)> Creation;
 TArray<TObjectPtr<UEdGraph>> Functions,Events,Macros,Delegates;EBlueprintType CapturedBlueprintType=BPTYPE_Normal;bool AllowReadOnlyLibrary=false;bool Stopped=false;UEdGraphNode* Selected=nullptr;FGuid SelectedGuid;FString SelectedPath,SelectedComment;bool CommentVisible=false,CommentPinned=false;UClass* SelectedClass=nullptr;int32 X=0,Y=0;bool SelectedPinsFixed=false;
 struct FPin{UEdGraphPin* P=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;FString Raw,Auto;FText Text,Friendly;uint32 TextFlags=0,FriendlyFlags=0,Flags=0;TObjectPtr<UObject> Default;UObject* TypeObject=nullptr;UObject* TerminalObject=nullptr;UEdGraphPin* Parent=nullptr;TArray<UEdGraphPin*> Children,Links;};
 struct FNode{TStrongObjectPtr<UEdGraphNode> Hold;UClass* Class=nullptr;FGuid Guid;int32 X=0,Y=0;FString Path,Comment;TArray<FPin> Pins;FMemberReference Ref;bool HasRef=false;TArray<FUserPinInfo> UserPins;};TArray<FNode> Nodes;TArray<FPin> SelectedPins;
 explicit FQQFlowContext(Function::FResult& In):R(In){}
 static bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects);}
 static bool TypeSame(const FEdGraphPinType& A,const FEdGraphPinType& B){return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
 static uint32 Flags(const UEdGraphPin* P){return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bOrphanedPin)<<5)|(uint32(P->bDeprecated)<<6)|(uint32(P->bDisplayAsMutableRef)<<7);}
 bool Bad(const TCHAR* Condition){if(!Stopped){Stopped=true;R.bChangeStateKnown=false;R.Data->SetStringField(TEXT("flow_first_failed_condition"),Condition);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);QQFlowFail(R,TEXT("flow_current_unknown"),TEXT("Captured flow authoring role changed; no next SDK, cleanup or retry."));}return false;}
 bool Pins(UEdGraphNode* N,TArray<FPin>& Out){if(!Live(N))return Bad(TEXT("node_unavailable"));for(auto* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||!P->DefaultObject.IsResolved())return Bad(TEXT("pin_unavailable"));FPin E;E.P=P;E.Id=P->PinId;E.Name=P->PinName;E.Direction=P->Direction;E.Type=P->PinType;E.Raw=P->DefaultValue;E.Auto=P->AutogeneratedDefaultValue;E.Text=P->DefaultTextValue;E.Friendly=P->PinFriendlyName;E.TextFlags=FTextInspector::GetFlags(E.Text);E.FriendlyFlags=FTextInspector::GetFlags(E.Friendly);E.Flags=Flags(P);E.Default=P->DefaultObject;if(P->PinType.PinSubCategoryObject.IsStale()||P->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(TEXT("pin_source_stale"));E.TypeObject=P->PinType.PinSubCategoryObject.Get();E.TerminalObject=P->PinType.PinValueType.TerminalSubCategoryObject.Get();for(UObject* Source:{E.TypeObject,E.TerminalObject,E.Default.Get()})if(Source){if(!Live(Source))return Bad(TEXT("pin_source_unavailable"));Holds.Emplace(Source);}E.Parent=P->ParentPin;E.Children=P->SubPins;E.Links=P->LinkedTo;if(E.Default)Holds.Emplace(E.Default.Get());Out.Add(MoveTemp(E));}return true;}
 bool PinCurrent(UEdGraphNode* N,const TArray<FPin>& Pins)const{if(N->Pins.Num()!=Pins.Num())return false;for(int32 I=0;I<Pins.Num();++I){const auto& E=Pins[I];const auto* P=N->Pins[I];if(P!=E.P||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=E.Id||P->PinName!=E.Name||P->Direction!=E.Direction||!TypeSame(P->PinType,E.Type)||P->PinType.PinSubCategoryObject.IsStale()||P->PinType.PinValueType.TerminalSubCategoryObject.IsStale()||P->PinType.PinSubCategoryObject.Get()!=E.TypeObject||P->PinType.PinValueType.TerminalSubCategoryObject.Get()!=E.TerminalObject||(E.TypeObject&&!Live(E.TypeObject))||(E.TerminalObject&&!Live(E.TerminalObject))||(E.Default&&!Live(E.Default.Get()))||P->DefaultValue!=E.Raw||P->AutogeneratedDefaultValue!=E.Auto||!P->DefaultObject.IsResolved()||P->DefaultObject!=E.Default||FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(E.Text)||FTextInspector::GetFlags(P->DefaultTextValue)!=E.TextFlags||FTextInspector::GetSharedDataId(P->PinFriendlyName)!=FTextInspector::GetSharedDataId(E.Friendly)||FTextInspector::GetFlags(P->PinFriendlyName)!=E.FriendlyFlags||Flags(P)!=E.Flags||P->ParentPin!=E.Parent||P->SubPins!=E.Children||P->LinkedTo!=E.Links)return false;}return true;}
 // TM read verifier: only List opts in; authoring/delete contexts retain Normal.
 bool BlueprintTypeCurrent() const
 {
  return BP->BlueprintType==CapturedBlueprintType&&(CapturedBlueprintType==BPTYPE_Normal||(AllowReadOnlyLibrary&&CapturedBlueprintType==BPTYPE_FunctionLibrary&&Meta==UBlueprint::StaticClass()&&Parent.Get()==UBlueprintFunctionLibrary::StaticClass()&&Functions.Contains(Graph.Get())));
 }
 bool Bind(UBlueprint* B,UEdGraph* G,int32 InX,int32 InY)
 {
  BP.Reset(B);Graph.Reset(G);Editor.Reset(GEditor);World.Reset(GWorld);X=InX;Y=InY;if(!Live(B)||!Live(G)||!B->ParentClass.GetGCPtr().IsResolved()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!G->Schema.GetGCPtr().IsResolved())return Bad(TEXT("target_slots_unavailable"));
  Parent=B->ParentClass.GetGCPtr();Generated=B->GeneratedClass.GetGCPtr();Skeleton=B->SkeletonGeneratedClass.GetGCPtr();Schema=G->Schema.GetGCPtr();if(!Live(Generated.Get())||!Live(Skeleton.Get())||!Generated->ClassDefaultObject.IsResolved()||!Skeleton->ClassDefaultObject.IsResolved())return Bad(TEXT("target_class_slots_unavailable"));GenCDO=Generated->ClassDefaultObject;SkelCDO=Skeleton->ClassDefaultObject;for(UObject* O:{static_cast<UObject*>(Parent.Get()),static_cast<UObject*>(Generated.Get()),static_cast<UObject*>(Skeleton.Get()),GenCDO.Get(),SkelCDO.Get(),static_cast<UObject*>(Schema.Get())})if(O)Holds.Emplace(O);
  Meta=B->GetClass();Outer=B->GetOuter();Package=B->GetPackage();CapturedBlueprintType=B->BlueprintType;GraphGuid=G->GraphGuid;GraphPath=G->GetPathName();Functions=B->FunctionGraphs;Events=B->UbergraphPages;Macros=B->MacroGraphs;Delegates=B->DelegateSignatureGraphs;
  for(const auto& H:G->Nodes){if(!H.IsResolved()||!Live(H.Get()))return Bad(TEXT("initial_node_inventory"));FNode N;N.Hold.Reset(H.Get());N.Class=H->GetClass();N.Guid=H->NodeGuid;N.X=H->NodePosX;N.Y=H->NodePosY;N.Path=H->GetPathName();N.Comment=H->NodeComment;if(!Pins(H.Get(),N.Pins))return false;if(auto* Call=Cast<UK2Node_CallFunction>(H.Get())){N.HasRef=true;N.Ref=Call->FunctionReference;}else if(auto* Event=Cast<UK2Node_Event>(H.Get())){N.HasRef=true;N.Ref=Event->EventReference;}else if(auto* Entry=Cast<UK2Node_FunctionTerminator>(H.Get())){N.HasRef=true;N.Ref=Entry->FunctionReference;}if(auto* Editable=Cast<UK2Node_EditablePinBase>(H.Get()))for(const auto& P:Editable->UserDefinedPins){if(!P)return Bad(TEXT("initial_user_pin_null"));N.UserPins.Add(*P);}Nodes.Add(MoveTemp(N));}return Current(nullptr);
 }
 bool Current(UEdGraphNode* N)
 {
  if(Stopped)return false;if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()||GEditor!=Editor.Get()||GWorld!=World.Get()||!Live(Editor.Get())||!Live(World.Get())||!Live(BP.Get())||!Live(Graph.Get())||BP->GetClass()!=Meta||BP->GetOuter()!=Outer||BP->GetPackage()!=Package||!BlueprintTypeCurrent()||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=Parent||!BP->GeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr()!=Generated||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->SkeletonGeneratedClass.GetGCPtr()!=Skeleton||!Live(Parent.Get())||!Live(Schema.Get())||!Live(Generated.Get())||!Live(Skeleton.Get())||(GenCDO&&!Live(GenCDO.Get()))||(SkelCDO&&!Live(SkelCDO.Get()))||!Generated->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject!=GenCDO||!Skeleton->ClassDefaultObject.IsResolved()||Skeleton->ClassDefaultObject!=SkelCDO||Graph->GetOuter()!=BP.Get()||Graph->GraphGuid!=GraphGuid||Graph->GetPathName()!=GraphPath||!Graph->Schema.GetGCPtr().IsResolved()||Graph->Schema.GetGCPtr()!=Schema||BP->FunctionGraphs!=Functions||BP->UbergraphPages!=Events||BP->MacroGraphs!=Macros||BP->DelegateSignatureGraphs!=Delegates)return Bad(TEXT("target_role_changed"));
  if(N&&!Selected){if(!Live(N)||N->GetOuter()!=Graph.Get()||!Graph->Nodes.Contains(N)||!N->NodeGuid.IsValid()||N->NodePosX!=X||N->NodePosY!=Y)return Bad(TEXT("selected_return_role"));Selected=N;SelectedGuid=N->NodeGuid;SelectedPath=N->GetPathName();SelectedClass=N->GetClass();SelectedComment=N->NodeComment;CommentVisible=N->bCommentBubbleVisible;CommentPinned=N->bCommentBubblePinned;Holds.Emplace(N);}
  if(N&&N!=Selected)return Bad(TEXT("selected_return_replaced"));if(Creation&&!Creation(Selected))return Bad(TEXT("creation_current_changed"));if(Graph->Nodes.Num()!=Nodes.Num()+(Selected?1:0))return Bad(TEXT("node_count_changed"));
  for(int32 I=0;I<Nodes.Num();++I){const auto& E=Nodes[I];auto* Node=E.Hold.Get();if(!Graph->Nodes[I].IsResolved()||Graph->Nodes[I].Get()!=Node||!Live(Node)||Node->GetClass()!=E.Class||Node->GetOuter()!=Graph.Get()||Node->NodeGuid!=E.Guid||Node->NodePosX!=E.X||Node->NodePosY!=E.Y||Node->NodeComment!=E.Comment||Node->GetPathName()!=E.Path||!PinCurrent(Node,E.Pins))return Bad(TEXT("prior_node_or_pin_changed"));if(E.HasRef){const FMemberReference* Ref=nullptr;if(auto* C=Cast<UK2Node_CallFunction>(Node))Ref=&C->FunctionReference;else if(auto* V=Cast<UK2Node_Event>(Node))Ref=&V->EventReference;else if(auto* T=Cast<UK2Node_FunctionTerminator>(Node))Ref=&T->FunctionReference;if(!Ref||!Ref->IsSameReference(E.Ref))return Bad(TEXT("prior_reference_changed"));}if(auto* Editable=Cast<UK2Node_EditablePinBase>(Node)){if(Editable->UserDefinedPins.Num()!=E.UserPins.Num())return Bad(TEXT("prior_user_pin_count_changed"));for(int32 J=0;J<E.UserPins.Num();++J){const auto& P=Editable->UserDefinedPins[J];const auto& Q=E.UserPins[J];if(!P||P->PinName!=Q.PinName||P->DesiredPinDirection!=Q.DesiredPinDirection||!TypeSame(P->PinType,Q.PinType)||P->PinDefaultValue!=Q.PinDefaultValue)return Bad(TEXT("prior_user_pin_changed"));}}}
  if(Selected&&(!Graph->Nodes.Last().IsResolved()||Graph->Nodes.Last().Get()!=Selected||!Live(Selected)||Selected->GetClass()!=SelectedClass||Selected->GetOuter()!=Graph.Get()||Selected->NodeGuid!=SelectedGuid||Selected->NodePosX!=X||Selected->NodePosY!=Y||Selected->GetPathName()!=SelectedPath||Selected->NodeComment!=SelectedComment||Selected->bCommentBubbleVisible!=CommentVisible||Selected->bCommentBubblePinned!=CommentPinned||(SelectedPinsFixed&&!PinCurrent(Selected,SelectedPins))))return Bad(TEXT("selected_node_or_pin_changed"));return true;
 }
 bool Freeze(UEdGraphNode* N){if(!Current(N)||SelectedPinsFixed)return !Stopped;for(auto* P:N->Pins)if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->LinkedTo.IsEmpty()||P->ParentPin||!P->SubPins.IsEmpty())return Bad(TEXT("selected_fresh_pin_scope"));if(!Pins(N,SelectedPins))return false;SelectedPinsFixed=true;return Current(N);}
};
TSharedRef<FJsonObject> QQBranch(FQQFlowContext& C,int32 X,int32 Y)
{
 auto R=MakeShared<FJsonObject>();auto D=MakeShared<FJsonObject>();R->SetObjectField(TEXT("data"),D);R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("mutation_attempted"),false);R->SetBoolField(TEXT("changed"),false);R->SetBoolField(TEXT("postcondition_passed"),false);R->SetStringField(TEXT("stage"),TEXT("add_branch"));
 const FString Path=C.GraphPath;const auto Before=ReadNodeIdentities(Path);D->SetObjectField(TEXT("before"),Before);if(!C.Current(nullptr)||!Before->GetBoolField(TEXT("success")))return R;
 R->SetBoolField(TEXT("mutation_attempted"),true);R->SetBoolField(TEXT("change_state_known"),false);C.R.bMutationAttempted=true;
 D->SetBoolField(TEXT("spawner_create_entered"),true);auto* Spawner=UBlueprintNodeSpawner::Create(UK2Node_IfThenElse::StaticClass());TStrongObjectPtr<UBlueprintNodeSpawner> HeldSpawner(Spawner);D->SetBoolField(TEXT("spawner_create_returned"),true);if(!C.Current(nullptr)||!FQQFlowContext::Live(Spawner))return R;
 C.BP->Modify();if(!C.Current(nullptr))return R;D->SetBoolField(TEXT("spawner_invoke_entered"),true);UEdGraphNode* Returned=Spawner->Invoke(C.Graph.Get(),{},FVector2D(X,Y));TStrongObjectPtr<UEdGraphNode> HeldReturned(Returned);D->SetBoolField(TEXT("spawner_invoke_returned"),true);if(!C.Current(Returned)||!Returned||Returned->GetClass()!=UK2Node_IfThenElse::StaticClass()||!C.Freeze(Returned))return R;
 FBlueprintEditorUtils::MarkBlueprintAsModified(C.BP.Get());if(!C.Current(Returned))return R;
 bool Shape=Returned->Pins.Num()==4;TSet<FName> Names;TSet<FGuid> PinIds;for(const auto& E:C.SelectedPins){const bool Execute=E.Name==UEdGraphSchema_K2::PN_Execute&&E.Direction==EGPD_Input;const bool Condition=E.Name==UEdGraphSchema_K2::PN_Condition&&E.Direction==EGPD_Input;const bool Output=(E.Name==UEdGraphSchema_K2::PN_Then||E.Name==UEdGraphSchema_K2::PN_Else)&&E.Direction==EGPD_Output;FEdGraphPinType Expected;Expected.PinCategory=Condition?UEdGraphSchema_K2::PC_Boolean:UEdGraphSchema_K2::PC_Exec;Shape&=(Execute||Condition||Output)&&!Names.Contains(E.Name)&&E.Id.IsValid()&&!PinIds.Contains(E.Id)&&FQQFlowContext::TypeSame(E.Type,Expected)&&E.Links.IsEmpty()&&E.Parent==nullptr&&E.Children.IsEmpty();Names.Add(E.Name);PinIds.Add(E.Id);}Shape&=Names.Num()==4&&PinIds.Num()==4;
 const auto After=ReadNodeIdentities(Path);D->SetObjectField(TEXT("after"),After);if(!C.Current(Returned))return R;D->SetStringField(TEXT("node_id"),C.SelectedGuid.ToString(EGuidFormats::Digits));D->SetStringField(TEXT("node_guid"),C.SelectedGuid.ToString(EGuidFormats::Digits));D->SetStringField(TEXT("object_path"),C.SelectedPath);D->SetStringField(TEXT("node_class"),Returned->GetClass()->GetPathName());D->SetNumberField(TEXT("x"),X);D->SetNumberField(TEXT("y"),Y);D->SetStringField(TEXT("node_name"),Returned->GetName());TArray<TSharedPtr<FJsonValue>> PinRows;for(const auto& E:C.SelectedPins){auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),E.Name.ToString());P->SetStringField(TEXT("pin_id"),E.Id.ToString(EGuidFormats::Digits));P->SetStringField(TEXT("direction"),E.Direction==EGPD_Input?TEXT("input"):TEXT("output"));P->SetStringField(TEXT("category"),E.Type.PinCategory.ToString());P->SetStringField(TEXT("default_value"),E.Raw);P->SetStringField(TEXT("autogenerated_default_value"),E.Auto);P->SetArrayField(TEXT("linked_to"),{});PinRows.Add(MakeShared<FJsonValueObject>(P));}D->SetArrayField(TEXT("pins"),PinRows);auto Node=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("node_id"),TEXT("node_guid"),TEXT("object_path"),TEXT("node_class"),TEXT("node_name"),TEXT("x"),TEXT("y"),TEXT("pins")})Node->SetField(Key,D->Values.FindChecked(Key));D->SetObjectField(TEXT("node"),Node);D->SetBoolField(TEXT("existing_nodes_preserved"),true);D->SetBoolField(TEXT("stock_spawner_invoke_used"),true);R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("changed"),true);R->SetBoolField(TEXT("postcondition_passed"),Shape&&After->GetBoolField(TEXT("success")));R->SetBoolField(TEXT("success"),Shape&&After->GetBoolField(TEXT("success")));if(!Shape){R->SetStringField(TEXT("error_code"),TEXT("branch_postcondition_failed"));R->SetStringField(TEXT("error"),TEXT("Actual stock Branch pin shape differed; actual prefix retained."));}return R;
}
Function::FResult QQAddFlow(const FString& Path,const FString& PositionJson,int32 OutputCount,const FString& Name,const FString& Id,const FString& Type,bool Create,bool Sequence)
{
 Function::FResult R;if(QQFlowRunning){QQFlowFail(R,TEXT("flow_request_reentrant"),TEXT("Branch/Sequence compound request is already active."));return R;}TGuardValue<bool> Request(QQFlowRunning,true);int32 X=0,Y=0;if(!QQPosition(PositionJson,X,Y,R))return R;if(Sequence&&(OutputCount<2||OutputCount>64)){QQFlowFail(R,TEXT("sequence_argument_out_of_range"),TEXT("Current Sequence output_count remains strict2..64; original rounding/min2 is incomplete."));return R;}
 const auto Position=R.Data;R=Function::Resolve(Path,Name,Id,Type,false);for(const auto& Pair:Position->Values)R.Data->SetField(Pair.Key,Pair.Value);UEdGraph* G=nullptr;UBlueprint* BP=nullptr;TFunction<bool(UEdGraphNode*)> Creation;
 auto Receipt=MakeShared<FJsonObject>();Receipt->SetBoolField(TEXT("requested_create_if_missing"),Create);Receipt->SetBoolField(TEXT("effective_node_create_if_missing"),false);Receipt->SetBoolField(TEXT("created"),false);Receipt->SetBoolField(TEXT("reused_existing"),R.bSuccess);Receipt->SetNumberField(TEXT("explicit_compile_calls"),0);Receipt->SetNumberField(TEXT("explicit_save_calls"),0);R.Data->SetObjectField(TEXT("graph_resolution"),Receipt);
 if(R.bSuccess){const auto Graph=R.Data->GetObjectField(TEXT("graph"));G=FindObject<UEdGraph>(nullptr,*Graph->GetStringField(TEXT("object_path")));BP=G?FBlueprintEditorUtils::FindBlueprintForGraph(G):nullptr;}
 else {if(!Create||R.ErrorCode!=TEXT("function_graph_not_found"))return R;FString Blueprint;R.Data->TryGetStringField(TEXT("blueprint"),Blueprint);BP=FindObject<UBlueprint>(nullptr,*Blueprint);if(!FQQFlowContext::Live(BP)||!Id.IsEmpty()||Name.IsEmpty()||!Type.Equals(TEXT("function"),ESearchCase::IgnoreCase)||BP->GetClass()!=UBlueprint::StaticClass()||BP->BlueprintType!=BPTYPE_Normal||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=AActor::StaticClass()){QQFlowFail(R,TEXT("flow_graph_creation_pending"),TEXT("Missing first profile needs a normal direct Actor, exact function name/type and no graph_id; existing graph reuse is separate."));return R;}R.ErrorCode.Empty();R.Error.Empty();if(!Function::CreateFunctionGraphForNode(BP,Name,R,G,Creation))return R;Receipt=R.Data->GetObjectField(TEXT("graph_resolution")).ToSharedRef();Receipt->SetBoolField(TEXT("requested_create_if_missing"),true);Receipt->SetBoolField(TEXT("effective_node_create_if_missing"),false);}
 TStrongObjectPtr<UBlueprint> HeldBP(BP);TStrongObjectPtr<UEdGraph> HeldGraph(G);R.bSuccess=R.bPostconditionPassed=false;FQQFlowContext C(R);C.Creation=MoveTemp(Creation);if(!C.Bind(BP,G,X,Y))return R;
 TFunction<bool(UEdGraphNode*)> Current=[&](UEdGraphNode* N){return C.Current(N);};TFunction<bool(UEdGraphNode*)> Freeze=[&](UEdGraphNode* N){return C.Freeze(N);};const auto Leaf=Sequence?Cubeless::UE58::Sequence::AddGuarded(G->GetPathName(),X,Y,OutputCount,Current,Freeze):QQBranch(C,X,Y);R.Data->SetObjectField(TEXT("flow_node_result"),Leaf);
 const bool LeafAttempted=Leaf->GetBoolField(TEXT("mutation_attempted"));R.bMutationAttempted|=LeafAttempted;if(C.Stopped)return R;R.bChangeStateKnown&=Leaf->GetBoolField(TEXT("change_state_known"));if(!R.bChangeStateKnown){QQFlowFail(R,TEXT("flow_writer_unknown"),TEXT("Native flow writer returned unknown; no further observer, cleanup or retry."));return R;}if(!C.Current(C.Selected))return R;
 bool Changed=false;Leaf->TryGetBoolField(TEXT("changed"),Changed);R.bChanged|=Changed;const auto LeafData=Leaf->GetObjectField(TEXT("data"));for(const auto& Pair:LeafData->Values)R.Data->SetField(Pair.Key,Pair.Value);auto GraphInfo=MakeShared<FJsonObject>();GraphInfo->SetStringField(TEXT("graph_id"),C.GraphGuid.ToString(EGuidFormats::Digits));GraphInfo->SetStringField(TEXT("graph_name"),G->GetName());GraphInfo->SetStringField(TEXT("graph_type"),BP->FunctionGraphs.Contains(G)?TEXT("function"):BP->MacroGraphs.Contains(G)?TEXT("macro"):TEXT("event"));GraphInfo->SetStringField(TEXT("object_path"),C.GraphPath);GraphInfo->SetStringField(TEXT("schema"),C.Schema->GetName());GraphInfo->SetNumberField(TEXT("node_count"),G->Nodes.Num());R.Data->SetObjectField(TEXT("graph"),GraphInfo);if(C.Selected&&Leaf->GetBoolField(TEXT("success"))){FString NodeName,ClassPath;R.Data->TryGetStringField(TEXT("node_name"),NodeName);R.Data->TryGetStringField(TEXT("node_class"),ClassPath);R.Data->SetStringField(TEXT("name"),NodeName);R.Data->SetStringField(TEXT("class"),FPackageName::ObjectPathToObjectName(ClassPath));R.Data->SetStringField(TEXT("legacy_node_id"),C.SelectedGuid.ToString());R.Data->SetStringField(TEXT("node_comment"),C.SelectedComment);R.Data->SetBoolField(TEXT("comment_bubble_visible"),C.CommentVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),C.CommentPinned);const FText CopiedTitle=C.Selected->GetNodeTitle(ENodeTitleType::FullTitle);if(!C.Current(C.Selected))return R;const FString Title=CopiedTitle.ToString();if(!C.Current(C.Selected))return R;R.Data->SetStringField(TEXT("title"),Title);const TSharedPtr<FJsonObject>* Node=nullptr;if(R.Data->TryGetObjectField(TEXT("node"),Node))for(const TCHAR* Key:{TEXT("name"),TEXT("class"),TEXT("title"),TEXT("legacy_node_id"),TEXT("node_comment"),TEXT("comment_bubble_visible"),TEXT("comment_bubble_pinned")})(*Node)->SetField(Key,R.Data->Values.FindChecked(Key));}R.Data->SetBoolField(TEXT("requested_create_graph_if_missing"),Create);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"),Receipt->HasField(TEXT("created"))&&Receipt->GetBoolField(TEXT("created")));R.Data->SetBoolField(TEXT("runtime_flow_executed"),false);
 R.bSuccess=Leaf->GetBoolField(TEXT("success"));R.bPostconditionPassed=R.bSuccess&&Leaf->GetBoolField(TEXT("postcondition_passed"));R.Stage=R.bSuccess?TEXT("complete"):TEXT("verification");if(!R.bSuccess){FString Code,Error;Leaf->TryGetStringField(TEXT("error_code"),Code);Leaf->TryGetStringField(TEXT("error"),Error);R.ErrorCode=Code.IsEmpty()?TEXT("flow_node_failed"):Code;R.Error=Error.IsEmpty()?TEXT("Flow node authoring failed; original creation prefix retained."):Error;}return R;
}
}
Function::FResult AddBlueprintBranchNode(const FString& Path,const FString& Position,const FString& Name,const FString& Id,const FString& Type,bool Create){return QQAddFlow(Path,Position,2,Name,Id,Type,Create,false);}
Function::FResult AddBlueprintSequenceNode(const FString& Path,const FString& Position,int32 Count,const FString& Name,const FString& Id,const FString& Type,bool Create){return QQAddFlow(Path,Position,Count,Name,Id,Type,Create,true);}

// QW: original List7/Delete12. Existing Delete5 and QQ writers are separate.
namespace
{
bool QWRunning=false;
bool QWText(const FString& S,int32 Limit=1024)
{if(S.Len()>Limit)return false;for(TCHAR C:S)if(!C)return false;return true;}
bool QWFail(Function::FResult& R,const TCHAR* Code,const FString& Message)
{R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Message;return false;}
struct FQWNodes : FQQFlowContext
{
 using FPin=FQQFlowContext::FPin;using FNode=FQQFlowContext::FNode;
 struct FExtra{UEdGraphNode* Key=nullptr;FString Name,ClassName,ClassPath,Title;bool Visible=false,Pinned=false;bool Variable=false,Custom=false;FMemberReference VariableRef;FName CustomName;int32 CustomFlags=0;bool CallInEditor=false;};
 TArray<FExtra> Extra;TMap<UEdGraphPin*,FGuid> OriginalIds;TMap<UEdGraphPin*,int32> OriginalOwners;int32 SelectedIndex=INDEX_NONE;bool Broken=false,Retired=false,InitialDirty=false;
 TFunction<bool(const TCHAR*)> CompositeCurrent;TFunction<bool()> CompositeReturned;TFunction<bool(const TCHAR*)> TimelineCurrent;
 explicit FQWNodes(Function::FResult& In):FQQFlowContext(In){}
 bool Error(const TCHAR* Stage,const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual)
 {if(!Stopped){auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("stage"),Stage);D->SetStringField(TEXT("object"),Object);D->SetStringField(TEXT("property"),Property);D->SetStringField(TEXT("expected"),Expected);D->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("first_mismatch"),D);Stopped=true;R.bChangeStateKnown=false;R.Stage=Stage;QWFail(R,TEXT("blueprint_node_current_unknown"),TEXT("Captured node/graph role changed; no next SDK, cleanup, save or automatic retry."));}return false;}
 bool Bound(UBlueprint* B,UEdGraph* G)
 {
  if(!Bind(B,G,0,0))return false;InitialDirty=B->GetPackage()->IsDirty();
  if(Nodes.Num()>1024)return Error(TEXT("initial_capture"),GraphPath,TEXT("node_count"),TEXT("<=1024"),FString::FromInt(Nodes.Num()));
  TSet<FGuid> NodeIds;for(int32 I=0;I<Nodes.Num();++I){const auto& N=Nodes[I];if(!N.Guid.IsValid()||NodeIds.Contains(N.Guid))return Error(TEXT("initial_capture"),N.Path,TEXT("node_guid"),TEXT("valid unique"),N.Guid.ToString());NodeIds.Add(N.Guid);if(!Live(N.Class))return Error(TEXT("initial_capture"),N.Path,TEXT("node_class"),TEXT("live"),TEXT("unavailable"));FExtra E;E.Key=N.Hold.Get();E.Name=E.Key->GetName();E.ClassName=N.Class->GetName();E.ClassPath=N.Class->GetPathName();E.Visible=E.Key->bCommentBubbleVisible;E.Pinned=E.Key->bCommentBubblePinned;if(auto* V=Cast<UK2Node_Variable>(E.Key)){E.Variable=true;E.VariableRef=V->VariableReference;}if(auto* C=Cast<UK2Node_CustomEvent>(E.Key)){E.Custom=true;E.CustomName=C->CustomFunctionName;E.CustomFlags=C->FunctionFlags;E.CallInEditor=C->bCallInEditor;}Extra.Add(MoveTemp(E));
   for(const auto& P:N.Pins){if(OriginalIds.Num()>=8192||OriginalIds.Contains(P.P))return Error(TEXT("initial_capture"),N.Path,TEXT("pin_inventory"),TEXT("unique bounded pins"),TEXT("duplicate/over budget"));for(const auto& Pair:OriginalIds)if(Pair.Value==P.Id)return Error(TEXT("initial_capture"),N.Path,TEXT("pin_id"),TEXT("unique graph pin id"),P.Id.ToString());OriginalIds.Add(P.P,P.Id);OriginalOwners.Add(P.P,I);}}
  if(!Membership(Nodes,OriginalIds,TEXT("initial_capture"))||!CaptureRoles())return false;return Check(TEXT("initial_capture"));
 }
 bool Membership(const TArray<FNode>& Rows,const TMap<UEdGraphPin*,FGuid>& Ids,const TCHAR* Stage)
 {
  for(const auto& N:Rows)for(const auto& P:N.Pins){if(P.Parent&&!Ids.Contains(P.Parent))return Error(Stage,N.Path,TEXT("parent_pin"),TEXT("own graph pin"),TEXT("foreign"));TSet<UEdGraphPin*> Seen;
   for(auto* S:P.Children){if(!Ids.Contains(S)||Seen.Contains(S)||S->ParentPin!=P.P)return Error(Stage,N.Path,TEXT("subpin"),TEXT("unique reciprocal owned child"),TEXT("invalid"));Seen.Add(S);}if(P.Parent&&!P.Parent->SubPins.Contains(P.P))return Error(Stage,N.Path,TEXT("parent_pin"),TEXT("reciprocal"),TEXT("missing child"));Seen.Reset();
   for(auto* L:P.Links){if(!Ids.Contains(L)||Seen.Contains(L)||!L->LinkedTo.Contains(P.P))return Error(Stage,N.Path,TEXT("linked_to"),TEXT("unique reciprocal owned graph pin"),TEXT("invalid"));Seen.Add(L);}}
  return true;
 }
 struct FClassRole{UClass* Class=nullptr;UClass* Meta=nullptr;UClass* Super=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;UObject* CDO=nullptr;UObject* CDOOuter=nullptr;UPackage* CDOPackage=nullptr;FString CDOPath;};
 TArray<FClassRole> ClassRoles;
 bool CaptureRoles()
 {
  for(UClass* K:{Generated.Get(),Skeleton.Get()}){if(!Live(K)||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=BP.Get()||K->GetSuperClass()!=Parent.Get()||!K->ClassDefaultObject.IsResolved())return Error(TEXT("class_capture"),GraphPath,TEXT("class_role"),TEXT("current own generated/skeleton"),TEXT("invalid"));FClassRole E;E.Class=K;E.Meta=K->GetClass();E.Super=K->GetSuperClass();E.Outer=K->GetOuter();E.Package=K->GetPackage();E.Path=K->GetPathName();E.CDO=K->ClassDefaultObject.Get();if(E.CDO){if(!Live(E.CDO)||E.CDO->GetClass()!=K||!E.CDO->HasAnyFlags(RF_ClassDefaultObject))return Error(TEXT("class_capture"),E.Path,TEXT("nullable_cdo"),TEXT("null or live exact class CDO"),TEXT("invalid"));E.CDOOuter=E.CDO->GetOuter();E.CDOPackage=E.CDO->GetPackage();E.CDOPath=E.CDO->GetPathName();}for(UObject* O:{static_cast<UObject*>(E.Meta),E.Outer,static_cast<UObject*>(E.Package),E.CDOOuter,static_cast<UObject*>(E.CDOPackage)})if(O){if(!Live(O))return Error(TEXT("class_capture"),E.Path,TEXT("owner"),TEXT("live"),TEXT("unavailable"));Holds.Emplace(O);}ClassRoles.Add(MoveTemp(E));}
  for(const auto& N:Nodes){if(!Live(N.Class))return Error(TEXT("class_capture"),N.Path,TEXT("node_class"),TEXT("live"),TEXT("unavailable"));Holds.Emplace(N.Class);}return true;
 }
 bool Roles(const TCHAR* Stage)
 {
  for(const auto& E:ClassRoles){auto* K=E.Class;if(!Live(K)||K->GetClass()!=E.Meta||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=BP.Get()||K->GetSuperClass()!=E.Super||K->GetOuter()!=E.Outer||K->GetPackage()!=E.Package||K->GetPathName()!=E.Path||!K->ClassDefaultObject.IsResolved()||K->ClassDefaultObject.Get()!=E.CDO)return Error(Stage,E.Path,TEXT("class_role"),TEXT("captured own class"),TEXT("changed"));if(E.CDO&&(!Live(E.CDO)||E.CDO->GetClass()!=K||!E.CDO->HasAnyFlags(RF_ClassDefaultObject)||E.CDO->GetOuter()!=E.CDOOuter||E.CDO->GetPackage()!=E.CDOPackage||E.CDO->GetPathName()!=E.CDOPath))return Error(Stage,E.Path,TEXT("nullable_cdo"),TEXT("captured actual CDO"),TEXT("changed"));}return true;
 }
 bool Anchors(const TCHAR* Stage)
 {
  if(Stopped)return false;
  if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()||GEditor!=Editor.Get()||GWorld!=World.Get()||!Live(Editor.Get())||!Live(World.Get())||!Live(BP.Get())||!Live(Graph.Get()))return Error(Stage,GraphPath,TEXT("context"),TEXT("same live editor/target"),TEXT("unavailable"));
  if(BP->GetClass()!=Meta||BP->GetOuter()!=Outer||BP->GetPackage()!=Package||!BlueprintTypeCurrent()||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=Parent||!BP->GeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr()!=Generated||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->SkeletonGeneratedClass.GetGCPtr()!=Skeleton||!Live(Parent.Get())||!Live(Schema.Get())||!Live(Generated.Get())||!Live(Skeleton.Get())||(GenCDO&&!Live(GenCDO.Get()))||(SkelCDO&&!Live(SkelCDO.Get()))||!Generated->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject!=GenCDO||!Skeleton->ClassDefaultObject.IsResolved()||Skeleton->ClassDefaultObject!=SkelCDO||Graph->GetOuter()!=BP.Get()||Graph->GraphGuid!=GraphGuid||Graph->GetPathName()!=GraphPath||!Graph->Schema.GetGCPtr().IsResolved()||Graph->Schema.GetGCPtr()!=Schema||BP->FunctionGraphs!=Functions||BP->UbergraphPages!=Events||BP->MacroGraphs!=Macros||BP->DelegateSignatureGraphs!=Delegates)return Error(Stage,GraphPath,TEXT("target_roles"),TEXT("captured BP/class/CDO/graph roles"),TEXT("changed"));return true;
 }
 static bool TextSame(const FText& A,const FText& B)
 {return FTextInspector::GetSharedDataId(A)==FTextInspector::GetSharedDataId(B)&&FTextInspector::GetFlags(A)==FTextInspector::GetFlags(B);}
 bool PinSame(const FPin& A,const FPin& B,const TCHAR* Stage,const FString& Path)
 {
  if(A.Id!=B.Id||A.Name!=B.Name||A.Direction!=B.Direction||!TypeSame(A.Type,B.Type)||A.TypeObject!=B.TypeObject||A.TerminalObject!=B.TerminalObject||A.Raw!=B.Raw||A.Auto!=B.Auto||A.Default!=B.Default||!TextSame(A.Text,B.Text)||!TextSame(A.Friendly,B.Friendly)||A.Flags!=B.Flags)return Error(Stage,Path,TEXT("pin_semantics"),A.Id.ToString()+TEXT("/")+A.Name.ToString()+TEXT("/")+A.Raw+TEXT("/")+A.Auto,B.Id.ToString()+TEXT("/")+B.Name.ToString()+TEXT("/")+B.Raw+TEXT("/")+B.Auto);return true;
 }
 TArray<FGuid> Keys(const TArray<UEdGraphPin*>& P,const TMap<UEdGraphPin*,FGuid>& Map)const
 {TArray<FGuid> Out;for(auto* V:P)Out.Add(Map.FindChecked(V));return Out;}
 bool Check(const TCHAR* Stage)
 {
  if(CompositeCurrent)return CompositeCurrent(Stage);
  if(TimelineCurrent&&!TimelineCurrent(Stage))return false;
  if(!Anchors(Stage)||!Roles(Stage))return false;
  const int32 Count=Nodes.Num()-(Retired?1:0);if(Graph->Nodes.Num()!=Count)return Error(Stage,GraphPath,TEXT("node_count"),FString::FromInt(Count),FString::FromInt(Graph->Nodes.Num()));
  // Reacquire all current pins from the owning nodes. Old pins below are keys only.
  TArray<FNode> Candidate;TMap<UEdGraphPin*,FGuid> CandidateIds;int32 PhysicalIndex=0;
  for(int32 I=0;I<Nodes.Num();++I){if(Retired&&I==SelectedIndex)continue;const auto& E=Nodes[I];auto* N=E.Hold.Get();if(!Graph->Nodes[PhysicalIndex].IsResolved()||Graph->Nodes[PhysicalIndex].Get()!=N||!Live(N)||!Live(E.Class)||N->GetClass()!=E.Class||N->GetOuter()!=Graph.Get())return Error(Stage,E.Path,TEXT("node_owner"),TEXT("same live owned node"),TEXT("changed"));++PhysicalIndex;
   const auto& QWNodeExtra=Extra[I];if(N->NodeGuid!=E.Guid||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->NodeComment!=E.Comment||N->GetPathName()!=E.Path||N->bCommentBubbleVisible!=QWNodeExtra.Visible||N->bCommentBubblePinned!=QWNodeExtra.Pinned)return Error(Stage,E.Path,TEXT("node_fields"),E.Guid.ToString(),N->NodeGuid.ToString());
   if(E.HasRef){const FMemberReference* Ref=nullptr;if(auto* C=Cast<UK2Node_CallFunction>(N))Ref=&C->FunctionReference;else if(auto* V=Cast<UK2Node_Event>(N))Ref=&V->EventReference;else if(auto* T=Cast<UK2Node_FunctionTerminator>(N))Ref=&T->FunctionReference;if(!Ref||!Ref->IsSameReference(E.Ref))return Error(Stage,E.Path,TEXT("member_reference"),TEXT("captured"),TEXT("changed"));}
   if(QWNodeExtra.Variable){auto* V=Cast<UK2Node_Variable>(N);if(!V||!V->VariableReference.IsSameReference(QWNodeExtra.VariableRef))return Error(Stage,E.Path,TEXT("variable_reference"),TEXT("captured"),TEXT("changed"));}
   if(QWNodeExtra.Custom){auto* C=Cast<UK2Node_CustomEvent>(N);if(!C||C->CustomFunctionName!=QWNodeExtra.CustomName||C->FunctionFlags!=QWNodeExtra.CustomFlags||C->bCallInEditor!=QWNodeExtra.CallInEditor)return Error(Stage,E.Path,TEXT("custom_event"),QWNodeExtra.CustomName.ToString(),C?C->CustomFunctionName.ToString():TEXT("missing"));}
   if(auto* Editable=Cast<UK2Node_EditablePinBase>(N)){if(Editable->UserDefinedPins.Num()!=E.UserPins.Num())return Error(Stage,E.Path,TEXT("user_pin_count"),FString::FromInt(E.UserPins.Num()),FString::FromInt(Editable->UserDefinedPins.Num()));for(int32 J=0;J<E.UserPins.Num();++J){const auto& U=Editable->UserDefinedPins[J];const auto& V=E.UserPins[J];if(!U||U->PinName!=V.PinName||U->DesiredPinDirection!=V.DesiredPinDirection||!TypeSame(U->PinType,V.PinType)||U->PinDefaultValue!=V.PinDefaultValue)return Error(Stage,E.Path,TEXT("user_pin"),V.PinName.ToString(),TEXT("changed"));}}
   FNode C;C.Hold.Reset(N);C.Path=E.Path;if(!Pins(N,C.Pins))return false;if(C.Pins.Num()!=E.Pins.Num())return Error(Stage,E.Path,TEXT("pin_count"),FString::FromInt(E.Pins.Num()),FString::FromInt(C.Pins.Num()));
   for(const auto& P:C.Pins){if(CandidateIds.Contains(P.P))return Error(Stage,E.Path,TEXT("pin_membership"),TEXT("unique"),TEXT("duplicate"));CandidateIds.Add(P.P,P.Id);}Candidate.Add(MoveTemp(C));}
  if(!Membership(Candidate,CandidateIds,Stage))return false;int32 CIndex=0;
  for(int32 I=0;I<Nodes.Num();++I){if(Retired&&I==SelectedIndex)continue;const auto& E=Nodes[I];const auto& C=Candidate[CIndex++];for(int32 J=0;J<E.Pins.Num();++J){const auto& P=E.Pins[J];const auto& A=C.Pins[J];if(!PinSame(P,A,Stage,E.Path))return false;
    const FGuid ParentExpected=P.Parent?OriginalIds.FindChecked(P.Parent):FGuid();const FGuid ParentActual=A.Parent?CandidateIds.FindChecked(A.Parent):FGuid();if(ParentExpected!=ParentActual||Keys(P.Children,OriginalIds)!=Keys(A.Children,CandidateIds))return Error(Stage,E.Path,TEXT("split_pin"),TEXT("captured parent/children"),TEXT("changed"));
    TArray<FGuid> Expected;for(auto* L:P.Links)if(!Broken||(I!=SelectedIndex&&OriginalOwners.FindChecked(L)!=SelectedIndex))Expected.Add(OriginalIds.FindChecked(L));const auto Actual=Keys(A.Links,CandidateIds);if(Expected!=Actual)return Error(Stage,E.Path,TEXT("linked_to"),FString::FromInt(Expected.Num()),FString::FromInt(Actual.Num()));}}
  return true;
 }
 bool Titles()
 {
  for(int32 I=0;I<Nodes.Num();++I){const FText T=Nodes[I].Hold->GetNodeTitle(ENodeTitleType::FullTitle);if(!Check(TEXT("node_title_return")))return false;const FString S=T.ToString();if(!Check(TEXT("node_title_text_return")))return false;Extra[I].Title=S;}return true;
 }
 TSharedRef<FJsonObject> GraphJson(int32 Count)const
 {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("graph_id"),GraphGuid.ToString());J->SetStringField(TEXT("graph_name"),Graph->GetName());J->SetStringField(TEXT("object_path"),GraphPath);J->SetStringField(TEXT("graph_type"),Functions.Contains(Graph.Get())?TEXT("function"):Macros.Contains(Graph.Get())?TEXT("macro"):Delegates.Contains(Graph.Get())?TEXT("delegate"):TEXT("event"));J->SetStringField(TEXT("schema"),Schema->GetName());J->SetNumberField(TEXT("node_count"),Count);return J;}
 TSharedRef<FJsonObject> NodeJson(int32 I,bool IncludePins)const
 {
  const auto& N=Nodes[I];const auto& QWNodeExtra=Extra[I];auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_id"),N.Guid.ToString());J->SetStringField(TEXT("node_guid"),N.Guid.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("name"),QWNodeExtra.Name);J->SetStringField(TEXT("class"),QWNodeExtra.ClassName);J->SetStringField(TEXT("node_class"),QWNodeExtra.ClassPath);J->SetStringField(TEXT("object_path"),N.Path);J->SetStringField(TEXT("title"),QWNodeExtra.Title);J->SetNumberField(TEXT("x"),N.X);J->SetNumberField(TEXT("y"),N.Y);J->SetStringField(TEXT("node_comment"),N.Comment);J->SetBoolField(TEXT("comment_bubble_visible"),QWNodeExtra.Visible);J->SetBoolField(TEXT("comment_bubble_pinned"),QWNodeExtra.Pinned);
  if(IncludePins){TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& P:N.Pins){auto K=MakeShared<FJsonObject>();K->SetStringField(TEXT("name"),P.Name.ToString());K->SetStringField(TEXT("pin_id"),P.Id.ToString());K->SetStringField(TEXT("direction"),P.Direction==EGPD_Input?TEXT("input"):TEXT("output"));K->SetStringField(TEXT("category"),P.Type.PinCategory.ToString());K->SetStringField(TEXT("subcategory"),P.Type.PinSubCategory.ToString());K->SetBoolField(TEXT("is_array"),P.Type.IsArray());K->SetStringField(TEXT("default_value"),P.Raw);auto Type=MakeShared<FJsonObject>();Type->SetStringField(TEXT("category"),P.Type.PinCategory.ToString());Type->SetStringField(TEXT("subcategory"),P.Type.PinSubCategory.ToString());Type->SetBoolField(TEXT("is_array"),P.Type.IsArray());if(P.TypeObject){const FString Path=P.TypeObject->GetPathName();Type->SetStringField(TEXT("subcategory_object"),Path);K->SetStringField(TEXT("subcategory_object"),Path);}K->SetObjectField(TEXT("pin_type"),Type);if(P.Default)K->SetStringField(TEXT("default_object"),P.Default->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:P.Links){const int32 O=OriginalOwners.FindChecked(L);const FPin* Peer=nullptr;for(const auto& Q:Nodes[O].Pins)if(Q.P==L){Peer=&Q;break;}auto Link=MakeShared<FJsonObject>();Link->SetStringField(TEXT("node_id"),Nodes[O].Guid.ToString());Link->SetStringField(TEXT("node_name"),Extra[O].Name);Link->SetStringField(TEXT("pin_name"),Peer->Name.ToString());Links.Add(MakeShared<FJsonValueObject>(Link));}K->SetArrayField(TEXT("linked_to"),Links);Rows.Add(MakeShared<FJsonValueObject>(K));}J->SetArrayField(TEXT("pins"),Rows);}return J;
 }
 static bool StableClass(const UClass* K)
 {return K==UK2Node_IfThenElse::StaticClass()||K==UK2Node_ExecutionSequence::StaticClass()||K==UK2Node_CallFunction::StaticClass()||K==UK2Node_Event::StaticClass()||K==UK2Node_CustomEvent::StaticClass()||K==UK2Node_VariableGet::StaticClass()||K==UK2Node_VariableSet::StaticClass()||K==UK2Node_FunctionEntry::StaticClass()||K==UK2Node_FunctionResult::StaticClass()||K->GetPathName()==TEXT("/Script/BlueprintGraph.K2Node_Self");}
 bool DeleteProfile()
 {
  // These exact classes inherit the empty NodeConnectionListChanged callback.
  // PinConnectionListChanged's reset/conform behavior is a different SDK seam.
  if(!StableClass(Nodes[SelectedIndex].Class))return QWFail(R,TEXT("delete_node_callback_pending"),TEXT("First deletion scope admits exact stock Event/CustomEvent/Branch/Sequence/plain Call/Variable/Self; structural terminators remain protected. Other node-specific Destroy/NodeConnection projections remain incomplete."));
  for(const auto& P:Nodes[SelectedIndex].Pins)for(auto* L:P.Links){const int32 O=OriginalOwners.FindChecked(L);if(!StableClass(Nodes[O].Class))return QWFail(R,TEXT("delete_peer_callback_pending"),TEXT("A connected peer has a node-specific NodeConnectionListChanged projection outside the first stock callback scope; no Modify or link deletion."));}return true;
 }
};
// RC: one exclusively owned collapsed graph. The original Delete12 writer stays shared.
struct FRCCompositeDelete
{
 FQWNodes& C;Function::FResult& R;TStrongObjectPtr<UK2Node_Composite> Composite;TStrongObjectPtr<UEdGraph> Bound;
 TStrongObjectPtr<UClass> CurrentGen,CurrentSkel;TStrongObjectPtr<UObject> CurrentGenCDO,CurrentSkelCDO;
 TArray<TStrongObjectPtr<UObject>> Holds;bool Ready=false,Rebound=false;FString BoundPath,GeneratedName,SkeletonName;FGuid BoundGuid;
 TArray<TObjectPtr<UEdGraph>> RootSubGraphs;TArray<TObjectPtr<UEdGraph>> Functions,Events,Macros,Delegates;
 struct FNodeRow{FQQFlowContext::FNode Node;FString Name;bool Visible=false,Pinned=false;FMemberReference Variable;bool HasVariable=false;FName CustomName;int32 CustomFlags=0;bool CallInEditor=false;};
 struct FGraphRow{TStrongObjectPtr<UEdGraph> Graph;FGuid Guid;FString Path;UObject* Outer=nullptr;UClass* Schema=nullptr;TArray<FNodeRow> Nodes;TMap<UEdGraphPin*,FGuid> Ids;TMap<UEdGraphPin*,FGuid> Owners;};
 TArray<FGraphRow> Graphs;FGraphRow Inner;
 struct FTunnel{UK2Node_Tunnel* Node=nullptr;UK2Node_Tunnel* Input=nullptr;UK2Node_Tunnel* Output=nullptr;bool Inputs=false,Outputs=false;};TArray<FTunnel> Tunnels;
 TStrongObjectPtr<UObject> MathCDO;TStrongObjectPtr<UFunction> Math;TStrongObjectPtr<UClass> MathOwner;FName MathName;EFunctionFlags MathFlags=FUNC_None;
 struct FField{FProperty* Property=nullptr;FName Name;FFieldClass* Kind=nullptr;uint64 Flags=0;int32 Offset=0,Size=0,Dim=0,Align=0;};TArray<FField> Fields;
 TStrongObjectPtr<USimpleConstructionScript> SCS;TStrongObjectPtr<USCS_Node> RootNode;TStrongObjectPtr<USceneComponent> Template;
 FGuid RootGuid;FName RootName,TemplateName;EComponentCreationMethod TemplateMethod=EComponentCreationMethod::Native;FTransform TemplateTransform;TArray<FName> TemplateTags;EComponentMobility::Type TemplateMobility=EComponentMobility::Static;
 explicit FRCCompositeDelete(FQWNodes& In):C(In),R(In.R){}
 bool Bad(const TCHAR* Stage,const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual){return C.Error(Stage,Object,Property,Expected,Actual);}
 bool Pending(const TCHAR* Code,const TCHAR* Message){return QWFail(R,Code,Message);}
 static bool Live(const UObject* O){return FQQFlowContext::Live(O);}
 static FString TextSource(const FText& T){const FString* S=FTextInspector::GetSourceString(T);return S?*S:FString();}
 static bool OptionalTextSame(const TOptional<FString>& A,const TOptional<FString>& B){return A.IsSet()==B.IsSet()&&(!A.IsSet()||A.GetValue().Equals(B.GetValue(),ESearchCase::CaseSensitive));}
 static bool TextSame(const FText& A,const FText& B){return TextSource(A).Equals(TextSource(B),ESearchCase::CaseSensitive)&&FTextInspector::GetFlags(A)==FTextInspector::GetFlags(B)&&OptionalTextSame(FTextInspector::GetNamespace(A),FTextInspector::GetNamespace(B))&&OptionalTextSame(FTextInspector::GetKey(A),FTextInspector::GetKey(B));}

 UObject* Project(UObject* O)const{if(!Rebound)return O;if(O==C.Generated.Get())return CurrentGen.Get();if(O==C.Skeleton.Get())return CurrentSkel.Get();if(O&&O==C.GenCDO.Get())return CurrentGenCDO.Get();if(O&&O==C.SkelCDO.Get())return CurrentSkelCDO.Get();return O;}
 bool Context(const TCHAR* Stage,bool Returning=false)
 {
  if(C.Stopped)return false;
  if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()||GEditor!=C.Editor.Get()||GWorld!=C.World.Get()||!Live(C.Editor.Get())||!Live(C.World.Get())||!Live(C.BP.Get())||!Live(C.Graph.Get()))return Bad(Stage,C.GraphPath,TEXT("context"),TEXT("captured live idle editor/Blueprint/graph"),TEXT("unavailable"));
  auto* B=C.BP.Get();auto* G=C.Graph.Get();
  if(B->GetClass()!=C.Meta||B->GetOuter()!=C.Outer||B->GetPackage()!=C.Package||B->BlueprintType!=BPTYPE_Normal||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()!=C.Parent||!Live(C.Parent.Get())||G->GetOuter()!=B||G->GraphGuid!=C.GraphGuid||G->GetPathName()!=C.GraphPath||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=C.Schema||B->FunctionGraphs!=Functions||B->UbergraphPages!=Events||B->MacroGraphs!=Macros||B->DelegateSignatureGraphs!=Delegates||!B->NewVariables.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->Timelines.IsEmpty()||!B->ComponentTemplates.IsEmpty())return Bad(Stage,C.GraphPath,TEXT("authored_owner_roles"),TEXT("captured own direct Actor graphs and declarations"),TEXT("changed"));
  if(!Live(SCS.Get())||SCS->GetClass()!=USimpleConstructionScript::StaticClass()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()!=SCS.Get()||SCS->GetRootNodes().Num()!=1||SCS->GetRootNodes()[0]!=RootNode.Get()||SCS->GetAllNodes().Num()!=1||!Live(RootNode.Get())||RootNode->GetClass()!=USCS_Node::StaticClass()||RootNode->GetOuter()!=SCS.Get()||!RootNode->ComponentClass.IsResolved()||RootNode->ComponentClass.Get()!=USceneComponent::StaticClass()||!RootNode->ChildNodes.IsEmpty()||!RootNode->ParentComponentOrVariableName.IsNone()||RootNode->VariableGuid!=RootGuid||RootNode->GetVariableName()!=RootName)return Bad(Stage,C.GraphPath,TEXT("SCS_source_roles"),TEXT("captured authored Scene root/template"),TEXT("changed"));
  if(!Returning&&(!RootNode->ComponentTemplate.IsResolved()||RootNode->ComponentTemplate.Get()!=Template.Get()||!TemplateRole(Template.Get(),CurrentGen.Get(),Stage)))return false;
  TArray<TObjectPtr<UEdGraph>> Expected=RootSubGraphs;if(C.Retired)Expected.Remove(Bound.Get());if(G->SubGraphs!=Expected)return Bad(Stage,C.GraphPath,TEXT("SubGraphs"),C.Retired?TEXT("selected owned BoundGraph removed only"):TEXT("captured"),TEXT("changed"));
  if(!Returning)
  {
   if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr()!=CurrentGen.Get()||B->SkeletonGeneratedClass.GetGCPtr()!=CurrentSkel.Get()||!Class(CurrentGen.Get(),false,Stage)||!Class(CurrentSkel.Get(),true,Stage))return Bad(Stage,C.GraphPath,TEXT("class_slots"),TEXT("current adopted own generated/skeleton roles"),TEXT("changed/unresolved"));
   if(CurrentGen->ClassDefaultObject.Get()!=CurrentGenCDO.Get()||CurrentSkel->ClassDefaultObject.Get()!=CurrentSkelCDO.Get()||!CDO(CurrentGenCDO.Get(),CurrentGen.Get(),Stage)||!CDO(CurrentSkelCDO.Get(),CurrentSkel.Get(),Stage))return Bad(Stage,C.GraphPath,TEXT("nullable_CDO_slots"),TEXT("current nullable adopted roles"),TEXT("changed"));
  }
  return Native(Stage);
 }
 bool Class(UClass* K,bool Skeleton,const TCHAR* Stage)
 {
  const FString& Expected=Skeleton?SkeletonName:GeneratedName;
  if(!Live(K)||K->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=C.BP.Get()||K->GetSuperClass()!=AActor::StaticClass()||K->GetOuter()!=C.Package||K->GetPackage()!=C.Package||K->GetName()!=Expected||!K->ClassDefaultObject.IsResolved()||K->HasAnyClassFlags(CLASS_NewerVersionExists|CLASS_Deprecated))return Bad(Stage,C.GraphPath,Skeleton?TEXT("skeleton_role"):TEXT("generated_role"),Expected,TEXT("unavailable/wrong owner or role"));return true;
 }
 bool TemplateRole(USceneComponent* T,UClass* Owner,const TCHAR* Stage)
 {return (Live(T)&&T->GetClass()==USceneComponent::StaticClass()&&T->GetOuter()==Owner&&T->GetFName()==TemplateName&&T->CreationMethod==TemplateMethod&&T->GetRelativeTransform().Equals(TemplateTransform,0.0)&&T->ComponentTags==TemplateTags&&T->Mobility==TemplateMobility)||Bad(Stage,C.GraphPath,TEXT("SCS_template_logical_role"),TEXT("captured name/Scene type/relative/tags/mobility on actual generated class"),TEXT("changed"));}
 bool CDO(UObject* O,UClass* K,const TCHAR* Stage)
 {
  if(!O)return true;
  return (Live(O)&&O->GetClass()==K&&O->GetOuter()==K->GetOuter()&&O->GetPackage()==C.Package&&O->HasAnyFlags(RF_ClassDefaultObject)&&O->GetFName()==FName(*(FString(TEXT("Default__"))+K->GetName())))||Bad(Stage,C.GraphPath,TEXT("CDO_role"),TEXT("nullable actual default of current own class"),TEXT("wrong role"));
 }
 bool Native(const TCHAR* Stage)
 {
  if(!Math.Get())return !Ready;
  if(!Live(MathOwner.Get())||MathOwner.Get()!=UKismetMathLibrary::StaticClass()||!Live(Math.Get())||Math->GetOuter()!=MathOwner.Get()||Math->GetFName()!=MathName||MathName!=TEXT("Add_IntInt")||Math->FunctionFlags!=MathFlags||!Math->HasAnyFunctionFlags(FUNC_Native))return Bad(Stage,BoundPath,TEXT("native_function_role"),TEXT("captured native KismetMath Add_IntInt"),TEXT("changed"));
  if(!MathOwner->ClassDefaultObject.IsResolved()||MathOwner->ClassDefaultObject.Get()!=MathCDO.Get()||(MathCDO.Get()&&(!Live(MathCDO.Get())||MathCDO->GetClass()!=MathOwner.Get()||MathCDO->GetOuter()!=MathOwner->GetOuter()||!MathCDO->HasAnyFlags(RF_ClassDefaultObject))))return Bad(Stage,BoundPath,TEXT("native_CDO_role"),TEXT("captured nullable current native CDO"),TEXT("changed/unresolved"));
  bool Member=false;for(TFieldIterator<UFunction> It(MathOwner.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It)Member|=*It==Math.Get();if(!Member)return Bad(Stage,BoundPath,TEXT("native_function_membership"),TEXT("own current function"),TEXT("missing"));
  TArray<FProperty*> Actual;for(TFieldIterator<FProperty> It(Math.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It)Actual.Add(*It);
  if(Actual.Num()!=Fields.Num())return Bad(Stage,BoundPath,TEXT("native_field_inventory"),LexToString(Fields.Num()),LexToString(Actual.Num()));
  for(const auto& E:Fields){if(!Actual.Contains(E.Property))return Bad(Stage,BoundPath,TEXT("native_field_membership"),E.Name.ToString(),TEXT("missing"));auto* P=E.Property;if(P->GetOwnerStruct()!=Math.Get()||P->GetClass()!=E.Kind||P->GetFName()!=E.Name||P->PropertyFlags!=E.Flags||P->GetOffset_ForInternal()!=E.Offset||P->GetSize()!=E.Size||P->ArrayDim!=E.Dim||P->GetMinAlignment()!=E.Align)return Bad(Stage,BoundPath,TEXT("native_field_binding"),E.Name.ToString(),TEXT("changed"));}return true;
 }
 bool HoldType(const FEdGraphPinType& T,const TCHAR* Stage,const FString& Path)
 {if(T.PinSubCategoryObject.IsStale()||T.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(Stage,Path,TEXT("UserPinInfo_type_source"),TEXT("nonstale nullable type roles"),TEXT("stale"));for(UObject* O:{T.PinSubCategoryObject.Get(),T.PinValueType.TerminalSubCategoryObject.Get()})if(O){Holds.Emplace(O);if(!Live(O))return Bad(Stage,Path,TEXT("UserPinInfo_type_source"),TEXT("live captured source"),TEXT("unavailable"));}return true;}
 bool CaptureGraph(UEdGraph* G,FGraphRow& Out)
 {
  if(!Live(G)||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=C.Schema)return Bad(TEXT("rc_capture"),C.GraphPath,TEXT("graph_role"),TEXT("live captured K2 graph"),TEXT("unavailable"));
  Out.Graph.Reset(G);Out.Guid=G->GraphGuid;Out.Path=G->GetPathName();Out.Outer=G->GetOuter();Out.Schema=C.Schema.Get();if(G->Nodes.Num()>1024)return Pending(TEXT("composite_delete_budget_pending"),TEXT("Bounded own graph inventory required."));
  TSet<FGuid> NodeIds;
  for(const auto& Slot:G->Nodes)
  {
   if(!Slot.IsResolved()||!Live(Slot.Get()))return Bad(TEXT("rc_capture"),Out.Path,TEXT("node_slot"),TEXT("resolved live"),TEXT("unavailable"));auto* N=Slot.Get();Holds.Emplace(N);auto* K=N->GetClass();if(!Live(K)||NodeIds.Contains(N->NodeGuid)||!N->NodeGuid.IsValid())return Bad(TEXT("rc_capture"),Out.Path,TEXT("node_class_GUID"),TEXT("live class unique GUID"),TEXT("invalid"));Holds.Emplace(K);NodeIds.Add(N->NodeGuid);
   FNodeRow E;E.Node.Hold.Reset(N);E.Node.Class=K;E.Node.Guid=N->NodeGuid;E.Node.Path=N->GetPathName();E.Node.X=N->NodePosX;E.Node.Y=N->NodePosY;E.Node.Comment=N->NodeComment;E.Name=N->GetName();E.Visible=N->bCommentBubbleVisible;E.Pinned=N->bCommentBubblePinned;
   if(!C.Pins(N,E.Node.Pins))return false;
   if(auto* RCCall=Cast<UK2Node_CallFunction>(N)){E.Node.HasRef=true;E.Node.Ref=RCCall->FunctionReference;}else if(auto* RCEvent=Cast<UK2Node_Event>(N)){E.Node.HasRef=true;E.Node.Ref=RCEvent->EventReference;}else if(auto* RCTerm=Cast<UK2Node_FunctionTerminator>(N)){E.Node.HasRef=true;E.Node.Ref=RCTerm->FunctionReference;}
   if(auto* V=Cast<UK2Node_Variable>(N)){E.HasVariable=true;E.Variable=V->VariableReference;}
   if(auto* V=Cast<UK2Node_CustomEvent>(N)){E.CustomName=V->CustomFunctionName;E.CustomFlags=V->FunctionFlags;E.CallInEditor=V->bCallInEditor;}
   if(auto* V=Cast<UK2Node_EditablePinBase>(N))for(const auto& P:V->UserDefinedPins){if(!P)return Bad(TEXT("rc_capture"),E.Node.Path,TEXT("UserPinInfo"),TEXT("nonnull"),TEXT("null"));if(!HoldType(P->PinType,TEXT("rc_capture"),E.Node.Path))return false;E.Node.UserPins.Add(*P);}
   for(const auto& P:E.Node.Pins){if(Out.Ids.Num()>=8192||Out.Ids.Contains(P.P))return Pending(TEXT("composite_delete_pin_budget_pending"),TEXT("Bounded unique pins required."));for(const auto& Pair:Out.Ids)if(Pair.Value==P.Id)return Bad(TEXT("rc_capture"),Out.Path,TEXT("pin_GUID"),TEXT("unique"),P.Id.ToString());Out.Ids.Add(P.P,P.Id);Out.Owners.Add(P.P,E.Node.Guid);}Out.Nodes.Add(MoveTemp(E));
  }
  for(const auto& E:Out.Nodes)for(const auto& P:E.Node.Pins){if(P.Parent||!P.Children.IsEmpty())return Pending(TEXT("composite_delete_split_pending"),TEXT("First collapsed removal scope has unsplit pins."));for(auto* L:P.Links){if(!Out.Ids.Contains(L))return Bad(TEXT("rc_capture"),E.Node.Path,TEXT("link_membership"),TEXT("same owned graph inventory"),TEXT("external"));const auto Owner=Out.Owners.FindChecked(L);const FQQFlowContext::FPin* Peer=nullptr;for(const auto& N:Out.Nodes)if(N.Node.Guid==Owner)for(const auto& Q:N.Node.Pins)if(Q.P==L)Peer=&Q;if(!Peer||!Peer->Links.Contains(P.P))return Bad(TEXT("rc_capture"),E.Node.Path,TEXT("reciprocal_link"),P.Id.ToString(),TEXT("asymmetric"));}}return true;
 }
 bool GraphCurrent(const FGraphRow& E,const TCHAR* Stage,bool Root,bool AllowClassProjection)
 {
  auto* G=E.Graph.Get();if(!Live(G)||G->GetOuter()!=E.Outer||G->GraphGuid!=E.Guid||G->GetPathName()!=E.Path||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=E.Schema||(G!=C.Graph.Get()&&!G->SubGraphs.IsEmpty()))return Bad(Stage,E.Path,TEXT("surviving_graph"),TEXT("captured owner/GUID/schema/path"),TEXT("changed"));
  const bool Removed=Root&&C.Retired;const int32 ExpectedCount=E.Nodes.Num()-(Removed?1:0);if(G->Nodes.Num()!=ExpectedCount)return Bad(Stage,E.Path,TEXT("node_count"),LexToString(ExpectedCount),LexToString(G->Nodes.Num()));
  TArray<FNodeRow> Candidates;TMap<UEdGraphPin*,FGuid> Ids;TMap<UEdGraphPin*,FGuid> Owners;TSet<FGuid> Seen;
  for(const auto& Slot:G->Nodes){if(!Slot.IsResolved()||!Live(Slot.Get()))return Bad(Stage,E.Path,TEXT("survivor_slot"),TEXT("resolved live"),TEXT("unavailable"));auto* N=Slot.Get();Holds.Emplace(N);if(!N->NodeGuid.IsValid()||Seen.Contains(N->NodeGuid))return Bad(Stage,E.Path,TEXT("survivor_GUID"),TEXT("unique valid"),TEXT("invalid"));Seen.Add(N->NodeGuid);const FNodeRow* Old=nullptr;for(const auto& Q:E.Nodes)if(Q.Node.Guid==N->NodeGuid)Old=&Q;if(!Old||(Removed&&Old->Node.Guid==C.Nodes[C.SelectedIndex].Guid))return Bad(Stage,E.Path,TEXT("survivor_membership"),TEXT("prior survivors only"),N->NodeGuid.ToString());
   if(N->GetOuter()!=G||N->GetClass()!=Old->Node.Class||N->GetPathName()!=Old->Node.Path||N->GetName()!=Old->Name||N->NodePosX!=Old->Node.X||N->NodePosY!=Old->Node.Y||N->NodeComment!=Old->Node.Comment||N->bCommentBubbleVisible!=Old->Visible||N->bCommentBubblePinned!=Old->Pinned)return Bad(Stage,Old->Node.Path,TEXT("survivor_logical_role"),TEXT("captured GUID/name/type/position/comment"),TEXT("changed"));
   if(Old->Node.HasRef){const FMemberReference* Ref=nullptr;if(auto* RCCall=Cast<UK2Node_CallFunction>(N))Ref=&RCCall->FunctionReference;else if(auto* RCEvent=Cast<UK2Node_Event>(N))Ref=&RCEvent->EventReference;else if(auto* RCTerm=Cast<UK2Node_FunctionTerminator>(N))Ref=&RCTerm->FunctionReference;if(!Ref||!Ref->IsSameReference(Old->Node.Ref))return Bad(Stage,Old->Node.Path,TEXT("member_reference"),TEXT("captured reference"),TEXT("changed"));}
   if(Old->HasVariable){auto* V=Cast<UK2Node_Variable>(N);if(!V||!V->VariableReference.IsSameReference(Old->Variable))return Bad(Stage,Old->Node.Path,TEXT("variable_reference"),TEXT("captured"),TEXT("changed"));}
   if(auto* V=Cast<UK2Node_CustomEvent>(N))if(V->CustomFunctionName!=Old->CustomName||V->FunctionFlags!=Old->CustomFlags||V->bCallInEditor!=Old->CallInEditor)return Bad(Stage,Old->Node.Path,TEXT("custom_event_role"),Old->CustomName.ToString(),V->CustomFunctionName.ToString());
   if(auto* V=Cast<UK2Node_EditablePinBase>(N)){if(V->UserDefinedPins.Num()!=Old->Node.UserPins.Num())return Bad(Stage,Old->Node.Path,TEXT("UserPinInfo_count"),LexToString(Old->Node.UserPins.Num()),LexToString(V->UserDefinedPins.Num()));for(const auto& Q:Old->Node.UserPins){const FUserPinInfo* A=nullptr;for(const auto& P:V->UserDefinedPins)if(P&&P->PinName==Q.PinName)A=P.Get();FEdGraphPinType Planned=Q.PinType;if(AllowClassProjection){Planned.PinSubCategoryObject=Project(Q.PinType.PinSubCategoryObject.Get());Planned.PinValueType.TerminalSubCategoryObject=Project(Q.PinType.PinValueType.TerminalSubCategoryObject.Get());}if(A&&!HoldType(A->PinType,Stage,Old->Node.Path))return false;if(!A||A->DesiredPinDirection!=Q.DesiredPinDirection||!FQQFlowContext::TypeSame(A->PinType,Planned)||A->PinDefaultValue!=Q.PinDefaultValue)return Bad(Stage,Old->Node.Path,TEXT("UserPinInfo"),Q.PinName.ToString(),TEXT("changed"));}}
   FNodeRow A;A.Node.Guid=N->NodeGuid;if(!C.Pins(N,A.Node.Pins))return false;if(A.Node.Pins.Num()!=Old->Node.Pins.Num())return Bad(Stage,Old->Node.Path,TEXT("pin_count"),LexToString(Old->Node.Pins.Num()),LexToString(A.Node.Pins.Num()));for(const auto& P:A.Node.Pins){if(Ids.Contains(P.P))return Bad(Stage,E.Path,TEXT("owning_pin_inventory"),TEXT("unique"),TEXT("duplicate"));Ids.Add(P.P,P.Id);Owners.Add(P.P,A.Node.Guid);}Candidates.Add(MoveTemp(A));
  }
  for(const auto& N:Candidates){const FNodeRow* Old=nullptr;for(const auto& Q:E.Nodes)if(Q.Node.Guid==N.Node.Guid)Old=&Q;TSet<FGuid> SeenPins;
   for(const auto& A:N.Node.Pins){if(SeenPins.Contains(A.Id))return Bad(Stage,Old->Node.Path,TEXT("pin_GUID"),TEXT("unique"),A.Id.ToString());SeenPins.Add(A.Id);const FQQFlowContext::FPin* P=nullptr;for(const auto& Q:Old->Node.Pins)if(Q.Id==A.Id)P=&Q;if(!P)return Bad(Stage,Old->Node.Path,TEXT("pin_GUID"),TEXT("captured"),A.Id.ToString());FEdGraphPinType Expected=P->Type;UObject* Type=P->TypeObject;UObject* Terminal=P->TerminalObject;UObject* Default=P->Default.Get();if(AllowClassProjection){Type=Project(Type);Terminal=Project(Terminal);Default=Project(Default);if(P->Default.Get()&&!Default)return Bad(Stage,Old->Node.Path,TEXT("pin_default_object_role"),TEXT("captured nonnull role has current counterpart"),TEXT("null"));Expected.PinSubCategoryObject=Type;Expected.PinValueType.TerminalSubCategoryObject=Terminal;}
    if(A.Name!=P->Name||A.Direction!=P->Direction||!FQQFlowContext::TypeSame(A.Type,Expected)||A.TypeObject!=Type||A.TerminalObject!=Terminal||A.Default.Get()!=Default||A.Raw!=P->Raw||A.Auto!=P->Auto||A.Flags!=P->Flags||!TextSame(A.Text,P->Text)||!TextSame(A.Friendly,P->Friendly)||A.Parent||!A.Children.IsEmpty())return Bad(Stage,Old->Node.Path,TEXT("pin_semantics"),P->Name.ToString()+TEXT("/")+P->Raw+TEXT("/")+P->Auto,A.Name.ToString()+TEXT("/")+A.Raw+TEXT("/")+A.Auto);
    TSet<FGuid> ExpectedLinks,ActualLinks;for(auto* L:P->Links){const bool SelectedOwner=E.Owners.FindChecked(L)==C.Nodes[C.SelectedIndex].Guid;const bool SelectedNode=Old->Node.Guid==C.Nodes[C.SelectedIndex].Guid;if(!Root||!C.Broken||(!SelectedNode&&!SelectedOwner))ExpectedLinks.Add(E.Ids.FindChecked(L));}for(auto* L:A.Links){if(!Ids.Contains(L)||ActualLinks.Contains(Ids.FindChecked(L)))return Bad(Stage,Old->Node.Path,TEXT("link_membership"),TEXT("unique current graph pins"),TEXT("external/duplicate"));ActualLinks.Add(Ids.FindChecked(L));const FQQFlowContext::FPin* Peer=nullptr;for(const auto& Q:Candidates)for(const auto& B:Q.Node.Pins)if(B.P==L)Peer=&B;if(!Peer||!Peer->Links.Contains(A.P))return Bad(Stage,Old->Node.Path,TEXT("reciprocal_link"),A.Id.ToString(),TEXT("asymmetric"));}if(ExpectedLinks.Num()!=ActualLinks.Num())return Bad(Stage,Old->Node.Path,TEXT("link_count"),LexToString(ExpectedLinks.Num()),LexToString(ActualLinks.Num()));for(const auto& Id:ExpectedLinks)if(!ActualLinks.Contains(Id))return Bad(Stage,Old->Node.Path,TEXT("link_GUID"),Id.ToString(),TEXT("missing"));
   }
  }return true;
 }
 bool Current(const TCHAR* Stage)
 {
  if(!Context(Stage))return false;for(int32 I=0;I<Graphs.Num();++I)if(!GraphCurrent(Graphs[I],Stage,Graphs[I].Graph.Get()==C.Graph.Get(),Rebound))return false;
  if(!C.Retired)
  {
   if(!Live(Composite.Get())||!Composite->BoundGraph.IsResolved()||Composite->BoundGraph.Get()!=Bound.Get()||!Live(Bound.Get())||Bound->GetOuter()!=Composite.Get()||!Bound->SubGraphs.IsEmpty()||!GraphCurrent(Inner,Stage,false,false))return Bad(Stage,BoundPath,TEXT("owned_BoundGraph"),TEXT("same exclusive closed graph"),TEXT("changed"));
   for(const auto& E:Tunnels){auto* N=E.Node;if(!Live(N)||!N->InputSinkNode.IsResolved()||!N->OutputSourceNode.IsResolved()||N->InputSinkNode.Get()!=E.Input||N->OutputSourceNode.Get()!=E.Output||N->bCanHaveInputs!=E.Inputs||N->bCanHaveOutputs!=E.Outputs)return Bad(Stage,BoundPath,TEXT("tunnel_pair"),TEXT("captured resolved role"),TEXT("changed"));}
  }
  R.Data->SetBoolField(TEXT("composite_survivors_current"),true);return true;
 }
 bool ClosedShape(UK2Node_Tunnel* Entry,UK2Node_Tunnel* Exit)
 {
  const FNodeRow *EntryRow=nullptr,*ExitRow=nullptr,*MathRow=nullptr;for(const auto& E:Inner.Nodes){if(E.Node.Hold.Get()==Entry)EntryRow=&E;else if(E.Node.Hold.Get()==Exit)ExitRow=&E;else MathRow=&E;}
  if(!EntryRow||!ExitRow||!MathRow||EntryRow->Node.Pins.Num()!=1||ExitRow->Node.Pins.Num()!=2||MathRow->Node.Pins.Num()!=4||C.Nodes[C.SelectedIndex].Pins.Num()!=3)return Pending(TEXT("composite_delete_closed_shape_pending"),TEXT("Closed entry exec1/exit exec+int2/Math hidden-self+A+B+ReturnValue4/gateway3 first scope required."));
  const FQQFlowContext::FPin *EntryExec=nullptr,*ExitExec=nullptr,*ExitInt=nullptr,*Result=nullptr;int32 Inputs=0,Self=0;TSet<FName> MathInputNames;
  for(const auto& E:Inner.Nodes)for(const auto& P:E.Node.Pins)
  {
   if(&E==MathRow&&P.Name==UEdGraphSchema_K2::PN_Self)
   {
    FEdGraphPinType ExpectedSelf;ExpectedSelf.PinCategory=UEdGraphSchema_K2::PC_Object;ExpectedSelf.PinSubCategoryObject=MathOwner.Get();
    if(P.Direction!=EGPD_Input||!FQQFlowContext::TypeSame(P.Type,ExpectedSelf)||P.TypeObject!=MathOwner.Get()||P.Default.Get()!=MathCDO.Get()||!(P.Flags&1)||!P.Links.IsEmpty())return Pending(TEXT("composite_delete_math_self_pending"),TEXT("QZ stock hidden static self with captured native class/nullable CDO role required."));++Self;continue;
   }
   FEdGraphPinType Expected;Expected.PinCategory=P.Type.PinCategory;
   if((P.Type.PinCategory!=UEdGraphSchema_K2::PC_Exec&&P.Type.PinCategory!=UEdGraphSchema_K2::PC_Int)||!FQQFlowContext::TypeSame(P.Type,Expected))return Pending(TEXT("composite_delete_pin_shape_pending"),TEXT("First closed graph uses whole by-value exec/int32 only; broader valid graphs remain incomplete."));
   if(&E==EntryRow){if(P.Direction!=EGPD_Output||P.Type.PinCategory!=UEdGraphSchema_K2::PC_Exec)return Pending(TEXT("composite_delete_pin_shape_pending"),TEXT("Entry exec output required."));EntryExec=&P;}
   else if(&E==ExitRow){if(P.Direction!=EGPD_Input)return Pending(TEXT("composite_delete_pin_shape_pending"),TEXT("Exit input roles required."));if(P.Type.PinCategory==UEdGraphSchema_K2::PC_Exec)ExitExec=&P;else ExitInt=&P;}
   else if(P.Name==TEXT("ReturnValue")&&P.Direction==EGPD_Output&&P.Type.PinCategory==UEdGraphSchema_K2::PC_Int)Result=&P;
   else if((P.Name==TEXT("A")||P.Name==TEXT("B"))&&P.Direction==EGPD_Input&&P.Type.PinCategory==UEdGraphSchema_K2::PC_Int){if(MathInputNames.Contains(P.Name))return Pending(TEXT("composite_delete_math_pins_pending"),TEXT("Unique native parameter names required."));MathInputNames.Add(P.Name);int32 Value=0;if(!P.Links.IsEmpty()||!LexTryParseString(Value,*P.Raw)||LexToString(Value)!=P.Raw)return Pending(TEXT("composite_delete_math_defaults_pending"),TEXT("Unlinked canonical scalar int32 A/B defaults required."));++Inputs;}
   else return Pending(TEXT("composite_delete_math_pins_pending"),TEXT("Exact Add_IntInt A/B/ReturnValue pins required."));
  }
  if(!EntryExec||!ExitExec||!ExitInt||!Result||Inputs!=2||Self!=1||EntryExec->Links.Num()!=1||EntryExec->Links[0]!=ExitExec->P||ExitExec->Links.Num()!=1||ExitExec->Links[0]!=EntryExec->P||Result->Links.Num()!=1||Result->Links[0]!=ExitInt->P||ExitInt->Links.Num()!=1||ExitInt->Links[0]!=Result->P)return Pending(TEXT("composite_delete_closed_links_pending"),TEXT("Closed reciprocal entry-to-exit exec and Math ReturnValue-to-exit int links required."));
  for(const auto& P:C.Nodes[C.SelectedIndex].Pins){const FQQFlowContext::FPin* Port=nullptr;for(const auto* V:{EntryExec,ExitExec,ExitInt})if(V->Name==P.Name&&V->Direction!=P.Direction&&FQQFlowContext::TypeSame(V->Type,P.Type))Port=V;if(!Port)return Pending(TEXT("composite_delete_gateway_projection_pending"),TEXT("Gateway names/directions/full types must project the two Tunnel roles."));}
  R.Data->SetNumberField(TEXT("captured_closed_internal_links"),2);return true;
 }
 bool Begin()
 {
  Composite.Reset(Cast<UK2Node_Composite>(C.Nodes[C.SelectedIndex].Hold.Get()));if(!Composite.Get()||Composite->GetClass()!=UK2Node_Composite::StaticClass())return Pending(TEXT("composite_delete_exact_pending"),TEXT("Exact native Composite required."));
  auto* B=C.BP.Get();Functions=B->FunctionGraphs;Events=B->UbergraphPages;Macros=B->MacroGraphs;Delegates=B->DelegateSignatureGraphs;RootSubGraphs=C.Graph->SubGraphs;
  if(C.Parent.Get()!=AActor::StaticClass()||!B->NewVariables.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->Timelines.IsEmpty()||!B->ComponentTemplates.IsEmpty()||!Macros.IsEmpty()||!Delegates.IsEmpty())return Pending(TEXT("composite_delete_actor_profile_pending"),TEXT("First composite deletion scope uses direct Actor with no own variables/interfaces/timelines/component templates/macros/dispatchers."));
  if(!B->SimpleConstructionScript.IsResolved())return Bad(TEXT("rc_bind"),C.GraphPath,TEXT("SCS_slot"),TEXT("resolved"),TEXT("unresolved"));SCS.Reset(B->SimpleConstructionScript.Get());if(!Live(SCS.Get())||SCS->GetClass()!=USimpleConstructionScript::StaticClass()||SCS->GetAllNodes().Num()!=1||SCS->GetRootNodes().Num()!=1)return Pending(TEXT("composite_delete_SCS_pending"),TEXT("One captured authored native Scene root required."));RootNode.Reset(SCS->GetRootNodes()[0]);if(!Live(RootNode.Get())||RootNode->GetClass()!=USCS_Node::StaticClass()||RootNode->GetOuter()!=SCS.Get())return Bad(TEXT("rc_bind"),C.GraphPath,TEXT("SCS_root"),TEXT("live"),TEXT("unavailable"));if(!RootNode->ComponentTemplate.IsResolved()||!RootNode->ComponentClass.IsResolved())return Bad(TEXT("rc_bind"),C.GraphPath,TEXT("SCS_template_class_slots"),TEXT("resolved"),TEXT("unresolved"));TStrongObjectPtr<UActorComponent> InitialTemplate(RootNode->ComponentTemplate.Get());if(!Live(InitialTemplate.Get())||InitialTemplate->GetClass()!=USceneComponent::StaticClass())return Pending(TEXT("composite_delete_SCS_pending"),TEXT("Live exact Scene template required."));Template.Reset(static_cast<USceneComponent*>(InitialTemplate.Get()));if(!Live(Template.Get())||Template->GetClass()!=USceneComponent::StaticClass()||Template->GetOuter()!=C.Generated.Get())return Pending(TEXT("composite_delete_SCS_pending"),TEXT("Exact native Scene template role required."));RootGuid=RootNode->VariableGuid;RootName=RootNode->GetVariableName();TemplateTransform=Template->GetRelativeTransform();TemplateTags=Template->ComponentTags;TemplateMobility=Template->Mobility;TemplateName=Template->GetFName();TemplateMethod=Template->CreationMethod;GeneratedName=C.Generated->GetName();SkeletonName=C.Skeleton->GetName();
  if(!Composite->BoundGraph.IsResolved())return Bad(TEXT("rc_bind"),C.GraphPath,TEXT("BoundGraph_slot"),TEXT("resolved"),TEXT("unresolved"));Bound.Reset(Composite->BoundGraph.Get());if(!Live(Bound.Get())||Bound->GetOuter()!=Composite.Get()||!Bound->SubGraphs.IsEmpty()||RootSubGraphs.Num()!=1||RootSubGraphs[0]!=Bound.Get()||Bound->Nodes.Num()!=3)return Pending(TEXT("composite_delete_owned_graph_pending"),TEXT("Composite1/exclusively owned BoundGraph1/Tunnel2/Add_IntInt1 only; nested graphs remain incomplete."));BoundPath=Bound->GetPathName();BoundGuid=Bound->GraphGuid;
  TArray<UEdGraph*> All;for(const auto& G:Functions)All.Add(G.Get());for(const auto& G:Events)All.Add(G.Get());if(All.Num()>16||!All.Contains(C.Graph.Get()))return Pending(TEXT("composite_delete_graph_inventory_pending"),TEXT("Bounded captured own direct graphs required."));TSet<UEdGraph*> Unique;int32 TotalNodes=0;
  for(auto* G:All){if(!Live(G)||Unique.Contains(G)||G->GetOuter()!=B||(!G->SubGraphs.IsEmpty()&&G!=C.Graph.Get()))return Pending(TEXT("composite_delete_graph_inventory_pending"),TEXT("One selected collapsed graph and other flat direct graphs only."));Unique.Add(G);TotalNodes+=G->Nodes.Num();if(TotalNodes>1024)return Pending(TEXT("composite_delete_graph_budget_pending"),TEXT("At most 1024 own direct graph nodes in total required."));FGraphRow Row;if(!CaptureGraph(G,Row))return false;for(const auto& N:Row.Nodes)if(N.Node.Hold.Get()!=Composite.Get()&&!FQWNodes::StableClass(N.Node.Class))return Pending(TEXT("composite_delete_peer_pending"),TEXT("Other root nodes use the existing exact stock callback scope."));Graphs.Add(MoveTemp(Row));}
  if(!CaptureGraph(Bound.Get(),Inner))return false;int32 TunnelCount=0,MathCount=0;UK2Node_Tunnel* Entry=nullptr;UK2Node_Tunnel* Exit=nullptr;
  for(const auto& E:Inner.Nodes){auto* N=E.Node.Hold.Get();if(N->GetClass()==UK2Node_Tunnel::StaticClass()){++TunnelCount;auto* T=static_cast<UK2Node_Tunnel*>(N);if(!T->InputSinkNode.IsResolved()||!T->OutputSourceNode.IsResolved())return Bad(TEXT("rc_bind"),BoundPath,TEXT("tunnel_role"),TEXT("resolved"),TEXT("unresolved"));FTunnel V;V.Node=T;V.Input=T->InputSinkNode.Get();V.Output=T->OutputSourceNode.Get();V.Inputs=T->bCanHaveInputs;V.Outputs=T->bCanHaveOutputs;Tunnels.Add(V);if(V.Outputs&&!V.Inputs){if(Entry)return Pending(TEXT("composite_delete_tunnel_pending"),TEXT("Unique entry/exit required."));Entry=T;}if(V.Inputs&&!V.Outputs){if(Exit)return Pending(TEXT("composite_delete_tunnel_pending"),TEXT("Unique entry/exit required."));Exit=T;}if(!((V.Inputs&&!V.Outputs&&V.Input==Composite.Get()&&!V.Output)||(!V.Inputs&&V.Outputs&&!V.Input&&V.Output==Composite.Get())))return Pending(TEXT("composite_delete_tunnel_pending"),TEXT("Stock entry OutputSource/exit InputSink to own Composite required."));}else if(N->GetClass()==UK2Node_CallFunction::StaticClass()){++MathCount;auto* Call=static_cast<UK2Node_CallFunction*>(N);Math.Reset(Call->GetTargetFunction());if(!Live(Math.Get()))return Bad(TEXT("rc_bind"),BoundPath,TEXT("native_function"),TEXT("loaded"),TEXT("unavailable"));UObject* ReturnedOwner=Math->GetOuter();if(ReturnedOwner)Holds.Emplace(ReturnedOwner);if(!Live(ReturnedOwner)||ReturnedOwner!=UKismetMathLibrary::StaticClass())return Pending(TEXT("composite_delete_math_pending"),TEXT("Exact held native Math owner required."));MathOwner.Reset(static_cast<UClass*>(ReturnedOwner));if(MathOwner.Get()!=UKismetMathLibrary::StaticClass()||Math->GetFName()!=TEXT("Add_IntInt")||!Math->HasAnyFunctionFlags(FUNC_Native))return Pending(TEXT("composite_delete_math_pending"),TEXT("Exact loaded native KismetMath Add_IntInt required."));if(!MathOwner->ClassDefaultObject.IsResolved())return Bad(TEXT("rc_bind"),BoundPath,TEXT("native_CDO_slot"),TEXT("resolved nullable actual"),TEXT("unresolved"));MathCDO.Reset(MathOwner->ClassDefaultObject.Get());if(MathCDO.Get()&&(!Live(MathCDO.Get())||MathCDO->GetClass()!=MathOwner.Get()||MathCDO->GetOuter()!=MathOwner->GetOuter()||!MathCDO->HasAnyFlags(RF_ClassDefaultObject)))return Bad(TEXT("rc_bind"),BoundPath,TEXT("native_CDO_role"),TEXT("current native default"),TEXT("unavailable/wrong role"));MathName=Math->GetFName();MathFlags=Math->FunctionFlags;for(TFieldIterator<FProperty> It(Math.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It){auto* P=*It;if(P->GetClass()!=FIntProperty::StaticClass()||P->GetOwnerStruct()!=Math.Get()||!P->HasAnyPropertyFlags(CPF_Parm)||P->ArrayDim!=1)return Pending(TEXT("composite_delete_math_field_pending"),TEXT("Exact own scalar int32 A/B/ReturnValue prototype required."));FField V;V.Property=P;V.Name=P->GetFName();V.Kind=P->GetClass();V.Flags=P->PropertyFlags;V.Offset=P->GetOffset_ForInternal();V.Size=P->GetSize();V.Dim=P->ArrayDim;V.Align=P->GetMinAlignment();Fields.Add(V);}}else return Pending(TEXT("composite_delete_inner_pending"),TEXT("Only Tunnel2/Add_IntInt1 in selected BoundGraph admitted."));}
  if(!Entry||!Exit||!Composite->InputSinkNode.IsResolved()||!Composite->OutputSourceNode.IsResolved()||Composite->InputSinkNode.Get()!=Entry||Composite->OutputSourceNode.Get()!=Exit)return Pending(TEXT("composite_delete_gateway_pending"),TEXT("Exact public gateway roles required."));FTunnel Gateway;Gateway.Node=Composite.Get();Gateway.Input=Entry;Gateway.Output=Exit;Gateway.Inputs=Composite->bCanHaveInputs;Gateway.Outputs=Composite->bCanHaveOutputs;Tunnels.Add(Gateway);
  if(TunnelCount!=2||MathCount!=1||Fields.Num()!=3)return Pending(TEXT("composite_delete_shape_pending"),TEXT("Exact closed Tunnel2/Add_IntInt1 required."));if(!ClosedShape(Entry,Exit))return false;
  CurrentGen.Reset(C.Generated.Get());CurrentSkel.Reset(C.Skeleton.Get());CurrentGenCDO.Reset(C.GenCDO.Get());CurrentSkelCDO.Reset(C.SkelCDO.Get());Ready=true;C.CompositeCurrent=[this](const TCHAR* Stage){return Current(Stage);};C.CompositeReturned=[this](){return Returned();};R.Data->SetBoolField(TEXT("composite_delete_contract"),true);R.Data->SetStringField(TEXT("retired_bound_graph_path"),BoundPath);R.Data->SetStringField(TEXT("retired_bound_graph_guid"),BoundGuid.ToString());R.Data->SetNumberField(TEXT("planned_owned_graph_removals"),1);R.Data->SetNumberField(TEXT("planned_owned_node_retirements"),Inner.Nodes.Num()+1);R.Data->SetBoolField(TEXT("implicit_structural_possible"),true);return Current(TEXT("rc_bind_complete"));
 }
 bool Returned()
 {
  R.Data->SetBoolField(TEXT("destroy_node_returned"),true);R.Data->SetBoolField(TEXT("selected_and_subtree_getters_retired"),true);
  // Do not consult any retired Composite/BoundGraph/inner-node getters, or old class/CDO fields.
  if(!Context(TEXT("composite_destroy_return_context"),true))return false;
  auto* B=C.BP.Get();if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("composite_destroy_return"),C.GraphPath,TEXT("class_slots"),TEXT("resolved actual returns"),TEXT("unresolved"));
  TStrongObjectPtr<UClass> Gen(B->GeneratedClass.GetGCPtr().Get()),Skel(B->SkeletonGeneratedClass.GetGCPtr().Get());if(!Class(Gen.Get(),false,TEXT("composite_destroy_return"))||!Class(Skel.Get(),true,TEXT("composite_destroy_return")))return false;
  TStrongObjectPtr<UObject> GenDefault(Gen->ClassDefaultObject.Get()),SkelDefault(Skel->ClassDefaultObject.Get());if(!CDO(GenDefault.Get(),Gen.Get(),TEXT("composite_destroy_return"))||!CDO(SkelDefault.Get(),Skel.Get(),TEXT("composite_destroy_return")))return false;
  if(!RootNode->ComponentTemplate.IsResolved())return Bad(TEXT("composite_destroy_return"),C.GraphPath,TEXT("SCS_template_slot"),TEXT("resolved actual role"),TEXT("unresolved"));
  TStrongObjectPtr<UActorComponent> ReturnedTemplate(RootNode->ComponentTemplate.Get());if(!Live(ReturnedTemplate.Get())||ReturnedTemplate->GetClass()!=USceneComponent::StaticClass())return Bad(TEXT("composite_destroy_return"),C.GraphPath,TEXT("SCS_template_role"),TEXT("live exact Scene template"),TEXT("unavailable/wrong class"));TStrongObjectPtr<USceneComponent> NewTemplate(static_cast<USceneComponent*>(ReturnedTemplate.Get()));if(!TemplateRole(NewTemplate.Get(),Gen.Get(),TEXT("composite_destroy_return")))return false;
  Template.Reset(NewTemplate.Get());
  CurrentGen.Reset(Gen.Get());CurrentSkel.Reset(Skel.Get());CurrentGenCDO.Reset(GenDefault.Get());CurrentSkelCDO.Reset(SkelDefault.Get());Rebound=true;
  if(!Current(TEXT("composite_destroy_return_current")))return false;R.Data->SetBoolField(TEXT("owned_graph_membership_removed_verified"),true);R.Data->SetBoolField(TEXT("surviving_graph_roles_verified"),true);R.Data->SetBoolField(TEXT("actual_class_roles_reacquired"),true);R.Data->SetBoolField(TEXT("retired_UObjects_immediate_destruction_proven"),false);return true;
 }
};

// SW: original Delete12 virtual destruction of one native Timeline and its template.
// A retired template (and its owned descendants) is held, never observed after
// DestroyNode. Its Transient rename is the installed SDK's operation, not a new
// adapter writer or a claimed post-return object-path observation.
struct FSWTimelineDelete
{
 FQWNodes& C;Function::FResult& R;TStrongObjectPtr<UK2Node_Timeline> Node;
 TStrongObjectPtr<UBlueprintGeneratedClass> Generated,Skeleton;TStrongObjectPtr<USimpleConstructionScript> SCS;
 TArray<TObjectPtr<UTimelineTemplate>> BlueprintSlots,GeneratedSlots,SkeletonSlots;
 struct FTemplate{TStrongObjectPtr<UTimelineTemplate> Hold;TStrongObjectPtr<UObject> Outer;TStrongObjectPtr<UClass> Class;FString Path;uint32 Flags=0;FString Authored;bool Selected=false;};
 struct FCurve{TStrongObjectPtr<UCurveBase> Hold;TStrongObjectPtr<UObject> Outer,ImportData;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString Path,Authored,Filename;FMD5Hash Hash;int64 Bytes=-1;uint32 Flags=0;bool Retire=false,ExternalPackage=false,Dirty=false;};
 struct FTimelineNode{TStrongObjectPtr<UK2Node_Timeline> Hold;FName Name;FGuid Guid;uint32 Flags=0;bool Selected=false;};
 TArray<FTemplate> Templates;TArray<FCurve> Curves;TArray<FTimelineNode> TimelineNodes;UTimelineTemplate* SelectedTemplate=nullptr;int32 SelectedSlot=INDEX_NONE;bool Ready=false;
 explicit FSWTimelineDelete(FQWNodes& In):C(In),R(In.R){}
 bool Bad(const TCHAR* Stage,const FString& Object,const TCHAR* Field,const FString& Expected,const FString& Actual){return C.Error(Stage,Object,Field,Expected,Actual);}
 bool Pending(const TCHAR* Code,const TCHAR* Message){return QWFail(R,Code,Message);}
 static bool Live(const UObject* O){return FQQFlowContext::Live(O);}
 static FString Bits(float V){uint32 B=0;FMemory::Memcpy(&B,&V,sizeof(B));return FString::Printf(TEXT("%08x"),B);}
 static uint32 NodeFlags(const UK2Node_Timeline* N){return uint32(N->bAutoPlay)|(uint32(N->bLoop)<<1)|(uint32(N->bReplicated)<<2)|(uint32(N->bIgnoreTimeDilation)<<3);}
 static FString Rich(const FRichCurve& V)
 {
  FString S=Bits(V.DefaultValue)+TEXT("/")+FString::FromInt(V.PreInfinityExtrap)+TEXT("/")+FString::FromInt(V.PostInfinityExtrap);
  // RichCurve's ordinary equality omits inactive tangents and tangent weights.
  // Copy every authored scalar explicitly; allocator/key handles are not values.
  for(const auto& K:V.GetConstRefOfKeys())S+=TEXT("|")+FString::FromInt(K.InterpMode)+TEXT(",")+FString::FromInt(K.TangentMode)+TEXT(",")+FString::FromInt(K.TangentWeightMode)+TEXT(",")+Bits(K.Time)+TEXT(",")+Bits(K.Value)+TEXT(",")+Bits(K.ArriveTangent)+TEXT(",")+Bits(K.ArriveTangentWeight)+TEXT(",")+Bits(K.LeaveTangent)+TEXT(",")+Bits(K.LeaveTangentWeight);
  return S;
 }
 FString CurveValues(UCurveBase* V)
 {
  FString S;if(V->GetClass()==UCurveFloat::StaticClass()){const auto* F=static_cast<UCurveFloat*>(V);S=FString::FromInt(F->bIsEventCurve)+TEXT(":")+Rich(F->FloatCurve);}
  else if(V->GetClass()==UCurveVector::StaticClass()){const auto* F=static_cast<UCurveVector*>(V);for(const auto& K:F->FloatCurves)S+=TEXT(";")+Rich(K);}
  else{const auto* F=static_cast<UCurveLinearColor*>(V);for(const auto& K:F->FloatCurves)S+=TEXT(";")+Rich(K);for(float A:{F->AdjustHue,F->AdjustSaturation,F->AdjustBrightness,F->AdjustBrightnessCurve,F->AdjustVibrance,F->AdjustMinAlpha,F->AdjustMaxAlpha})S+=TEXT(";")+Bits(A);}
  return S;
 }
 bool AddCurve(UCurveBase* V)
 {
  if(!V)return true;for(const auto& E:Curves)if(E.Hold.Get()==V)return true;
  if(Curves.Num()>=256||!Live(V)||(V->GetClass()!=UCurveFloat::StaticClass()&&V->GetClass()!=UCurveVector::StaticClass()&&V->GetClass()!=UCurveLinearColor::StaticClass()))return Pending(TEXT("timeline_curve_source_pending"),TEXT("Bounded exact loaded native Float/Vector/LinearColor curves required; wider valid sources remain incomplete."));
  FCurve E;E.Hold.Reset(V);E.Class.Reset(V->GetClass());E.Outer.Reset(V->GetOuter());E.Package.Reset(V->GetPackage());if(!Live(E.Class.Get())||!Live(E.Outer.Get())||!Live(E.Package.Get()))return Bad(TEXT("timeline_capture"),C.GraphPath,TEXT("curve_owner"),TEXT("live captured class/outer/package"),TEXT("unavailable"));
  if(!V->AssetImportData.IsResolved())return Pending(TEXT("timeline_curve_import_source_pending"),TEXT("Existing curve import-data slot must be resolved; no implicit load."));E.ImportData.Reset(V->AssetImportData.Get());if(E.ImportData.Get()&&!Live(E.ImportData.Get()))return Bad(TEXT("timeline_capture"),C.GraphPath,TEXT("curve_import_data"),TEXT("nullable live"),TEXT("unavailable"));
  int32 Keys=0;if(auto* Float=Cast<UCurveFloat>(V))Keys=Float->FloatCurve.GetNumKeys();else if(auto* Vector=Cast<UCurveVector>(V))for(const auto& K:Vector->FloatCurves)Keys+=K.GetNumKeys();else for(const auto& K:static_cast<UCurveLinearColor*>(V)->FloatCurves)Keys+=K.GetNumKeys();
  if(Keys>4096)return Pending(TEXT("timeline_curve_keys_pending"),TEXT("At most4096 authored keys per captured native curve; larger valid curves remain incomplete."));
  E.Path=V->GetPathName();E.Flags=static_cast<uint32>(V->GetFlags());E.Retire=V->IsIn(SelectedTemplate);E.ExternalPackage=E.Package.Get()!=C.Package;E.Dirty=E.Package->IsDirty();E.Authored=CurveValues(V);
  if(E.ExternalPackage){if(!FPackageName::TryConvertLongPackageNameToFilename(E.Package->GetName(),E.Filename,FPackageName::GetAssetPackageExtension()))return Pending(TEXT("timeline_curve_file_pending"),TEXT("Readonly curve package filename must be observable."));E.Filename=FPaths::ConvertRelativePathToFull(E.Filename);E.Bytes=IFileManager::Get().FileSize(*E.Filename);if(E.Bytes>=0){E.Hash=FMD5Hash::HashFile(*E.Filename);if(!E.Hash.IsValid())return Bad(TEXT("timeline_capture"),E.Path,TEXT("file"),TEXT("readable initial bytes"),TEXT("read failed"));}}
  Curves.Add(MoveTemp(E));return true;
 }
 template<class T> bool TrackCurve(const T& Slot,UCurveBase*& Out,bool Capture)
 {
  if(!Slot.IsResolved())return Capture?Pending(TEXT("timeline_curve_slot_pending"),TEXT("Actual curve slot must be resolved before observer access; no load.")):Bad(TEXT("timeline_current"),C.GraphPath,TEXT("curve_slot"),TEXT("captured resolved slot"),TEXT("unresolved"));Out=Slot.Get();if(Capture)return AddCurve(Out);if(!Out)return true;
  for(const auto& E:Curves)if(E.Hold.Get()==Out)return true;return Bad(TEXT("timeline_current"),C.GraphPath,TEXT("curve_slot"),TEXT("captured curve identity"),TEXT("new/foreign curve"));
 }
 bool TemplateValues(UTimelineTemplate* T,FString& Out,bool Capture)
 {
  if(T->EventTracks.Num()>256||T->FloatTracks.Num()>256||T->VectorTracks.Num()>256||T->LinearColorTracks.Num()>256||T->MetaDataArray.Num()>1024)return Capture?Pending(TEXT("timeline_authored_inventory_pending"),TEXT("Bounded native track/metadata inventory required; larger valid templates remain incomplete.")):Bad(TEXT("timeline_current"),C.GraphPath,TEXT("track_metadata_inventory"),TEXT("captured bounded inventory"),TEXT("grew beyond captured bounds"));
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("length_bits"),Bits(T->TimelineLength));J->SetNumberField(TEXT("length_mode"),T->LengthMode);J->SetNumberField(TEXT("flags"),uint32(T->bAutoPlay)|(uint32(T->bLoop)<<1)|(uint32(T->bReplicated)<<2)|(uint32(T->bIgnoreTimeDilation)<<3));J->SetStringField(TEXT("guid"),T->TimelineGuid.ToString());J->SetNumberField(TEXT("tick_group"),T->TimelineTickGroup);
  J->SetStringField(TEXT("variable"),T->GetVariableName().ToString());J->SetStringField(TEXT("direction"),T->GetDirectionPropertyName().ToString());J->SetStringField(TEXT("update"),T->GetUpdateFunctionName().ToString());J->SetStringField(TEXT("finished"),T->GetFinishedFunctionName().ToString());
  auto Base=[](const FTTTrackBase& V){auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),V.GetTrackName().ToString());Q->SetBoolField(TEXT("external"),V.bIsExternalCurve);Q->SetBoolField(TEXT("expanded"),V.bIsExpanded);Q->SetBoolField(TEXT("synchronized"),V.bIsCurveViewSynchronized);return Q;};
  TArray<TSharedPtr<FJsonValue>> Rows;UCurveBase* V=nullptr;
  for(const auto& E:T->EventTracks){if(!TrackCurve(E.CurveKeys,V,Capture))return false;auto Q=Base(E);Q->SetStringField(TEXT("function"),E.GetFunctionName().ToString());Q->SetStringField(TEXT("curve"),V?V->GetPathName():FString());Rows.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("events"),Rows);Rows.Reset();
  for(const auto& E:T->FloatTracks){if(!TrackCurve(E.CurveFloat,V,Capture))return false;auto Q=Base(E);Q->SetStringField(TEXT("property"),E.GetPropertyName().ToString());Q->SetStringField(TEXT("curve"),V?V->GetPathName():FString());Rows.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("floats"),Rows);Rows.Reset();
  for(const auto& E:T->VectorTracks){if(!TrackCurve(E.CurveVector,V,Capture))return false;auto Q=Base(E);Q->SetStringField(TEXT("property"),E.GetPropertyName().ToString());Q->SetStringField(TEXT("curve"),V?V->GetPathName():FString());Rows.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("vectors"),Rows);Rows.Reset();
  for(const auto& E:T->LinearColorTracks){if(!TrackCurve(E.CurveLinearColor,V,Capture))return false;auto Q=Base(E);Q->SetStringField(TEXT("property"),E.GetPropertyName().ToString());Q->SetStringField(TEXT("curve"),V?V->GetPathName():FString());Rows.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("colors"),Rows);Rows.Reset();
  for(const auto& E:T->MetaDataArray){auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("key"),E.DataKey.ToString());Q->SetStringField(TEXT("value"),E.DataValue);Rows.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("metadata"),Rows);Rows.Reset();
  const int32 Display=T->GetNumDisplayTracks();if(Display<0||Display>1024)return Capture?Pending(TEXT("timeline_display_inventory_pending"),TEXT("Bounded native display-track inventory required.")):Bad(TEXT("timeline_current"),C.GraphPath,TEXT("display_count"),TEXT("captured bounded count"),FString::FromInt(Display));for(int32 I=0;I<Display;++I){const auto D=T->GetDisplayTrackId(I);auto Q=MakeShared<FJsonObject>();Q->SetNumberField(TEXT("type"),D.TrackType);Q->SetNumberField(TEXT("index"),D.TrackIndex);Rows.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("display_order"),Rows);
  return FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out))||Bad(TEXT("timeline_values"),C.GraphPath,TEXT("copied_JSON"),TEXT("serialized native scalar snapshot"),TEXT("serialization failed"));
 }
 bool Current(const TCHAR* Stage)
 {
  if(!C.Anchors(Stage)||!C.Roles(Stage))return false;
  if(!Ready)return true;
  auto* B=C.BP.Get();TArray<TObjectPtr<UTimelineTemplate>> Expected=BlueprintSlots;if(C.Retired)Expected.RemoveAt(SelectedSlot);
  if(B->Timelines!=Expected)return Bad(Stage,C.GraphPath,TEXT("blueprint_timelines"),FString::FromInt(Expected.Num()),FString::FromInt(B->Timelines.Num()));
  if(Generated->Timelines!=GeneratedSlots||Skeleton->Timelines!=SkeletonSlots)return Bad(Stage,C.GraphPath,TEXT("compiled_timeline_slots"),TEXT("unchanged compiler cache until separate compile"),TEXT("changed"));
  if(!B->NewVariables.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->ComponentTemplates.IsEmpty()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()!=SCS.Get())return Bad(Stage,C.GraphPath,TEXT("other_authored_declarations"),TEXT("captured empty declarations and SCS identity"),TEXT("changed"));
  for(const auto& E:TimelineNodes){if(C.Retired&&E.Selected)continue;auto* N=E.Hold.Get();if(!Live(N)||N->GetClass()!=UK2Node_Timeline::StaticClass()||N->TimelineName!=E.Name||N->TimelineGuid!=E.Guid||NodeFlags(N)!=E.Flags)return Bad(Stage,C.GraphPath,TEXT("timeline_node_role"),E.Name.ToString()+TEXT("/")+E.Guid.ToString(),N&&Live(N)?N->TimelineName.ToString()+TEXT("/")+N->TimelineGuid.ToString():TEXT("unavailable"));}
  // Check surviving curve identities before any template observer reads their paths.
  for(const auto& E:Curves){if(C.Retired&&E.Retire)continue;auto* V=E.Hold.Get();if(!Live(V)||!Live(E.Class.Get())||!Live(E.Outer.Get())||!Live(E.Package.Get())||V->GetClass()!=E.Class.Get()||V->GetOuter()!=E.Outer.Get()||V->GetPackage()!=E.Package.Get()||V->GetPathName()!=E.Path||static_cast<uint32>(V->GetFlags())!=E.Flags||!V->AssetImportData.IsResolved()||V->AssetImportData.Get()!=E.ImportData.Get()||(E.ImportData.Get()&&!Live(E.ImportData.Get())))return Bad(Stage,E.Path,TEXT("curve_role"),TEXT("captured native identity/import slot"),TEXT("changed"));if(E.ExternalPackage&&E.Package->IsDirty()!=E.Dirty)return Bad(Stage,E.Path,TEXT("readonly_package_dirty"),E.Dirty?TEXT("true"):TEXT("false"),E.Package->IsDirty()?TEXT("true"):TEXT("false"));const FString Actual=CurveValues(V);if(Actual!=E.Authored)return Bad(Stage,E.Path,TEXT("authored_curve_channels"),E.Authored,Actual);}
  for(const auto& E:Templates){if(C.Retired&&E.Selected)continue;auto* T=E.Hold.Get();if(!Live(T)||!Live(E.Outer.Get())||!Live(E.Class.Get())||T->GetClass()!=E.Class.Get()||T->GetOuter()!=E.Outer.Get()||T->GetPackage()!=C.Package||T->GetPathName()!=E.Path||static_cast<uint32>(T->GetFlags())!=E.Flags)return Bad(Stage,E.Path,TEXT("timeline_template_role"),TEXT("captured native owner/path/flags"),TEXT("changed"));FString Actual;if(!TemplateValues(T,Actual,false))return false;if(Actual!=E.Authored)return Bad(Stage,E.Path,TEXT("authored_timeline"),E.Authored,Actual);}
  return true;
 }
 bool Begin()
 {
  if(!C.Anchors(TEXT("timeline_bind"))||!C.Roles(TEXT("timeline_bind")))return false;auto* B=C.BP.Get();Node.Reset(Cast<UK2Node_Timeline>(C.Nodes[C.SelectedIndex].Hold.Get()));
  if(!Node.Get()||Node->GetClass()!=UK2Node_Timeline::StaticClass()||C.Parent.Get()!=AActor::StaticClass()||B->GetClass()!=UBlueprint::StaticClass()||!B->NewVariables.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->ComponentTemplates.IsEmpty())return Pending(TEXT("timeline_delete_owner_profile_pending"),TEXT("First Timeline removal uses normal direct Actor without own variables/interfaces/component templates; wider valid owners remain incomplete."));
  for(const auto& P:C.Nodes[C.SelectedIndex].Pins)for(auto* L:P.Links)if(!FQWNodes::StableClass(C.Nodes[C.OriginalOwners.FindChecked(L)].Class))return Pending(TEXT("timeline_delete_peer_callback_pending"),TEXT("Connected peers use existing exact stock callback scope; wider valid peers remain incomplete."));
  Generated.Reset(Cast<UBlueprintGeneratedClass>(C.Generated.Get()));Skeleton.Reset(Cast<UBlueprintGeneratedClass>(C.Skeleton.Get()));if(!Generated.IsValid()||!Skeleton.IsValid()||!B->SimpleConstructionScript.IsResolved())return Bad(TEXT("timeline_bind"),C.GraphPath,TEXT("compiled_owner"),TEXT("held native generated/skeleton and resolved SCS"),TEXT("unavailable"));SCS.Reset(B->SimpleConstructionScript.Get());if(SCS.Get()&&!Live(SCS.Get()))return Bad(TEXT("timeline_bind"),C.GraphPath,TEXT("SCS"),TEXT("nullable live captured"),TEXT("unavailable"));
  BlueprintSlots=B->Timelines;GeneratedSlots=Generated->Timelines;SkeletonSlots=Skeleton->Timelines;if(BlueprintSlots.IsEmpty()||BlueprintSlots.Num()>64||GeneratedSlots.Num()>64||SkeletonSlots.Num()>64)return Pending(TEXT("timeline_delete_inventory_pending"),TEXT("Bounded own Timeline template inventories required."));
  for(const auto& Slot:BlueprintSlots){if(!Slot.IsResolved()||!Live(Slot.Get())||Slot->GetClass()!=UTimelineTemplate::StaticClass())return Pending(TEXT("timeline_delete_template_pending"),TEXT("Each current Blueprint Timeline slot must resolve to an exact native live template."));C.Holds.Emplace(Slot.Get());}
  // Match the installed FindTimelineTemplateByVariableName rule: first exact
  // template object name with its postfix, then the legacy object-name fallback.
  // The cached VariableName is captured as an actual value, never a lookup substitute.
  const FName WantedTemplate(*UTimelineTemplate::TimelineVariableNameToTemplateName(Node->TimelineName));
  for(FName Wanted:{WantedTemplate,Node->TimelineName}){for(int32 I=0;I<BlueprintSlots.Num();++I)if(BlueprintSlots[I]->GetFName()==Wanted){SelectedTemplate=BlueprintSlots[I].Get();SelectedSlot=I;break;}if(SelectedTemplate)break;}
  if(SelectedTemplate){int32 Count=0;for(const auto& Slot:BlueprintSlots)Count+=Slot.Get()==SelectedTemplate;if(Count!=1)return Pending(TEXT("timeline_delete_template_ambiguous"),TEXT("Exactly one selected template membership required before destruction."));}
  if(!SelectedTemplate)return Pending(TEXT("timeline_delete_template_missing"),TEXT("Selected Timeline's actual native template is absent; orphan Timeline deletion remains incomplete."));
  for(const auto* Slots:{&BlueprintSlots,&GeneratedSlots,&SkeletonSlots})for(const auto& Slot:*Slots){if(!Slot.IsResolved()||!Live(Slot.Get())||Slot->GetClass()!=UTimelineTemplate::StaticClass())return Pending(TEXT("timeline_delete_template_pending"),TEXT("Resolved exact native live class/Blueprint template slots required."));bool Seen=false;for(const auto& E:Templates)Seen|=E.Hold.Get()==Slot.Get();if(Seen)continue;auto* T=Slot.Get();FTemplate E;E.Hold.Reset(T);E.Outer.Reset(T->GetOuter());E.Class.Reset(T->GetClass());if(!Live(E.Outer.Get())||T->GetPackage()!=C.Package||(E.Outer.Get()!=Generated.Get()&&E.Outer.Get()!=Skeleton.Get()&&E.Outer.Get()!=B))return Pending(TEXT("timeline_delete_template_owner_pending"),TEXT("Templates must be owned by this Blueprint or its actual generated/skeleton class; inherited/foreign ownership remains incomplete."));E.Selected=T==SelectedTemplate;E.Path=T->GetPathName();E.Flags=static_cast<uint32>(T->GetFlags());Templates.Add(MoveTemp(E));}
  for(auto& E:Templates)if(!TemplateValues(E.Hold.Get(),E.Authored,true))return false;
  // A selected-template-owned curve moves with the retired outer. Do not turn
  // that normal rename into a survivor-invariant, nor read it after retirement.
  for(const auto& E:Templates)if(!E.Selected){auto Uses=[&](UCurveBase* V){for(const auto& K:Curves)if(K.Hold.Get()==V&&K.Retire)return true;return false;};auto* T=E.Hold.Get();for(const auto& K:T->EventTracks)if(Uses(K.CurveKeys.Get()))return Pending(TEXT("timeline_delete_shared_retired_curve_pending"),TEXT("A surviving template references a curve owned by the retiring template; this wider valid ownership transition remains incomplete."));for(const auto& K:T->FloatTracks)if(Uses(K.CurveFloat.Get()))return Pending(TEXT("timeline_delete_shared_retired_curve_pending"),TEXT("Surviving template shares a retiring-owned curve."));for(const auto& K:T->VectorTracks)if(Uses(K.CurveVector.Get()))return Pending(TEXT("timeline_delete_shared_retired_curve_pending"),TEXT("Surviving template shares a retiring-owned curve."));for(const auto& K:T->LinearColorTracks)if(Uses(K.CurveLinearColor.Get()))return Pending(TEXT("timeline_delete_shared_retired_curve_pending"),TEXT("Surviving template shares a retiring-owned curve."));}
  for(const auto& E:C.Nodes)if(E.Class==UK2Node_Timeline::StaticClass()){auto* N=static_cast<UK2Node_Timeline*>(E.Hold.Get());FTimelineNode V;V.Hold.Reset(N);V.Name=N->TimelineName;V.Guid=N->TimelineGuid;V.Flags=NodeFlags(N);V.Selected=N==Node.Get();if(!V.Selected&&V.Name==Node->TimelineName)return Pending(TEXT("timeline_delete_shared_template_node_pending"),TEXT("A surviving Timeline node shares the selected template; wider valid shared ownership remains incomplete."));TimelineNodes.Add(MoveTemp(V));}
  R.Data->SetBoolField(TEXT("timeline_delete_contract"),true);R.Data->SetStringField(TEXT("retired_template_path_before"),SelectedTemplate->GetPathName());R.Data->SetStringField(TEXT("retired_timeline_name"),Node->TimelineName.ToString());R.Data->SetNumberField(TEXT("timeline_template_count_before"),BlueprintSlots.Num());R.Data->SetNumberField(TEXT("captured_curve_count"),Curves.Num());R.Data->SetBoolField(TEXT("retired_template_actual_path_observed"),false);R.Data->SetBoolField(TEXT("SDK_Transient_rename_return_value_observed"),false);R.Data->SetBoolField(TEXT("global_Undo_debugger_cache_preservation_proven"),false);
  Ready=true;C.TimelineCurrent=[this](const TCHAR* Stage){return Current(Stage);};return C.Check(TEXT("timeline_bind_complete"));
 }
 bool Finish()
 {
  if(!C.Check(TEXT("timeline_final_source_check")))return false;
  for(const auto& E:Curves){if(E.Retire||!E.ExternalPackage)continue;const int64 Bytes=IFileManager::Get().FileSize(*E.Filename);const FMD5Hash Hash=Bytes>=0?FMD5Hash::HashFile(*E.Filename):FMD5Hash();if(Bytes!=E.Bytes||Hash!=E.Hash||(Bytes>=0&&!Hash.IsValid()))return Bad(TEXT("timeline_final_source_check"),E.Path,TEXT("readonly_curve_file"),FString::Printf(TEXT("%lld/%s"),E.Bytes,*LexToString(E.Hash)),FString::Printf(TEXT("%lld/%s"),Bytes,*LexToString(Hash)));}
  if(!C.Check(TEXT("timeline_publication")))return false;R.Data->SetBoolField(TEXT("timeline_membership_removed_verified"),true);R.Data->SetNumberField(TEXT("timeline_template_count_after"),BlueprintSlots.Num()-1);R.Data->SetBoolField(TEXT("retired_template_post_getters_skipped"),true);R.Data->SetBoolField(TEXT("nonselected_timeline_values_preserved"),true);R.Data->SetBoolField(TEXT("surviving_curve_channels_preserved"),true);R.Data->SetBoolField(TEXT("readonly_curve_files_preserved"),true);R.Data->SetBoolField(TEXT("retired_UObjects_immediate_destruction_proven"),false);return true;
 }
};


void QWInit(Function::FResult& R,const TCHAR* Operation)
{R.Data->SetStringField(TEXT("operation"),Operation);R.Data->SetNumberField(TEXT("explicit_compile_calls"),0);R.Data->SetNumberField(TEXT("explicit_save_calls"),0);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("node_cleanup_contract"),true);R.Data->SetStringField(TEXT("captured_fields"),TEXT("selected graph/node identity, XY/comment, common references/user pins and full pin type/default identity/flags/split/order/reciprocal links; unobserved node-specific caches are not full memory proof"));for(const TCHAR* K:{TEXT("graph_modify_calls"),TEXT("node_modify_calls"),TEXT("break_all_links_calls"),TEXT("destroy_node_calls"),TEXT("notify_graph_changed_calls"),TEXT("mark_modified_calls")})R.Data->SetNumberField(K,0);}
bool QWResolve(const FString& B,const FString& GN,const FString& GI,const FString& GT,Function::FResult& R,UBlueprint*& BP,UEdGraph*& G)
{
 if(!QWText(B)||!QWText(GN)||!QWText(GI)||!QWText(GT))return QWFail(R,TEXT("node_selector_invalid"),TEXT("Bounded strings without NUL required."));if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested())return QWFail(R,TEXT("node_editor_context_unavailable"),TEXT("Idle editor game thread required."));
 R=Function::Resolve(B,GN,GI,GT,false);if(!R.bSuccess)return false;const auto Info=R.Data->GetObjectField(TEXT("graph"));const FString GraphPath=Info->GetStringField(TEXT("object_path"));G=FindObject<UEdGraph>(nullptr,*GraphPath);if(!FQQFlowContext::Live(G)){R.Stage=TEXT("graph_resolve_return");R.bChangeStateKnown=false;R.Data->SetStringField(TEXT("resolved_graph_path"),GraphPath);return QWFail(R,TEXT("node_graph_resolve_unknown"),TEXT("Resolved graph unavailable after SDK return; no owner getter, retry or cleanup."));}BP=FBlueprintEditorUtils::FindBlueprintForGraph(G);if(!FQQFlowContext::Live(BP)){R.Stage=TEXT("graph_owner_return");R.bChangeStateKnown=false;R.Data->SetStringField(TEXT("resolved_graph_path"),GraphPath);return QWFail(R,TEXT("node_graph_resolve_unknown"),TEXT("Resolved Blueprint unavailable; no further observation."));}return true;
}
}
Function::FResult ListBlueprintNodes(const FString& B,const FString& NT,const FString& Title,bool IncludePins,const FString& GN,const FString& GI,const FString& GT)
{
 Function::FResult R;if(QWRunning){QWFail(R,TEXT("node_request_reentrant"),TEXT("List/Delete request already active."));return R;}TGuardValue<bool> Running(QWRunning,true);if(!QWText(NT)||!QWText(Title)){QWFail(R,TEXT("node_filter_invalid"),TEXT("Bounded class/title strings required."));return R;}UBlueprint* BP=nullptr;UEdGraph* G=nullptr;if(!QWResolve(B,GN,GI,GT,R,BP,G))return R;QWInit(R,TEXT("list_blueprint_nodes"));R.Stage=TEXT("initial_graph_capture");R.bSuccess=R.bPostconditionPassed=false;FQWNodes C(R);C.AllowReadOnlyLibrary=true;if(!C.Bound(BP,G)||!C.Titles())return R;
 TArray<TSharedPtr<FJsonValue>> Rows;for(int32 I=0;I<C.Nodes.Num();++I){const auto& X=C.Extra[I];if((NT.IsEmpty()||X.ClassName.Contains(NT)||X.Title.Contains(NT))&&(Title.IsEmpty()||X.Title.Contains(Title)))Rows.Add(MakeShared<FJsonValueObject>(C.NodeJson(I,IncludePins)));}
 if(!C.Check(TEXT("list_publication")))return R;auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("blueprint"),BP->GetPathName());Legacy->SetStringField(TEXT("graph_name"),G->GetName());Legacy->SetObjectField(TEXT("graph"),C.GraphJson(C.Nodes.Num()));Legacy->SetArrayField(TEXT("nodes"),Rows);for(const auto& Pair:Legacy->Values)R.Data->SetField(Pair.Key,Pair.Value);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
Function::FResult DeleteBlueprintNode(const FString& B,const FString& Id,const FString& GN,const FString& GI,const FString& GT,const FString& ExpectedName,const FString& ExpectedClass,const FString& ExpectedTitle,bool AllowData,bool AllowExec,bool AllowAny,bool AllowOrphan)
{
 Function::FResult R;if(QWRunning){QWFail(R,TEXT("node_request_reentrant"),TEXT("List/Delete request already active."));return R;}TGuardValue<bool> Running(QWRunning,true);FGuid Guid;if(!QWText(Id)||!FGuid::Parse(Id,Guid)||!Guid.IsValid()||!QWText(ExpectedName)||!QWText(ExpectedClass)||!QWText(ExpectedTitle)){QWFail(R,TEXT("node_delete_argument_invalid"),TEXT("Valid node GUID and bounded expected strings required."));return R;}UBlueprint* BP=nullptr;UEdGraph* G=nullptr;if(!QWResolve(B,GN,GI,GT,R,BP,G))return R;QWInit(R,TEXT("delete_blueprint_node"));R.Stage=TEXT("initial_graph_capture");R.bSuccess=R.bPostconditionPassed=false;FQWNodes C(R);if(!C.Bound(BP,G)||!C.Titles())return R;
 for(int32 I=0;I<C.Nodes.Num();++I)if(C.Nodes[I].Guid==Guid)C.SelectedIndex=I;if(C.SelectedIndex==INDEX_NONE){QWFail(R,TEXT("blueprint_node_not_found"),TEXT("Node not found in selected graph."));return R;}const int32 Index=C.SelectedIndex;const auto& N=C.Nodes[Index];const auto& X=C.Extra[Index];R.Stage=TEXT("delete_preflight");
 if((!ExpectedName.IsEmpty()&&!X.Name.Equals(ExpectedName,ESearchCase::IgnoreCase))||(!ExpectedClass.IsEmpty()&&!X.ClassName.Equals(ExpectedClass,ESearchCase::IgnoreCase)&&!X.ClassName.EndsWith(ExpectedClass,ESearchCase::IgnoreCase))||(!ExpectedTitle.IsEmpty()&&!X.Title.Contains(ExpectedTitle,ESearchCase::IgnoreCase))){auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("object"),N.Path);const bool NameMismatch=!ExpectedName.IsEmpty()&&!X.Name.Equals(ExpectedName,ESearchCase::IgnoreCase);const bool ClassMismatch=!ExpectedClass.IsEmpty()&&!X.ClassName.Equals(ExpectedClass,ESearchCase::IgnoreCase)&&!X.ClassName.EndsWith(ExpectedClass,ESearchCase::IgnoreCase);M->SetStringField(TEXT("property"),NameMismatch?TEXT("name"):ClassMismatch?TEXT("class"):TEXT("title_contains"));M->SetStringField(TEXT("expected"),NameMismatch?ExpectedName:ClassMismatch?ExpectedClass:ExpectedTitle);M->SetStringField(TEXT("actual"),NameMismatch?X.Name:ClassMismatch?X.ClassName:X.Title);R.Data->SetObjectField(TEXT("first_mismatch"),M);QWFail(R,TEXT("blueprint_node_expected_mismatch"),TEXT("Expected node name/class/title did not match."));return R;}
 int32 Exec=0,Data=0;TArray<TSharedPtr<FJsonValue>> BrokenLinks;for(const auto& P:N.Pins)for(auto* L:P.Links){const bool IsExec=P.Type.PinCategory==UEdGraphSchema_K2::PC_Exec;if(IsExec)++Exec;else ++Data;const int32 O=C.OriginalOwners.FindChecked(L);const FQWNodes::FPin* Peer=nullptr;for(const auto& Q:C.Nodes[O].Pins)if(Q.P==L){Peer=&Q;break;}auto Link=MakeShared<FJsonObject>();Link->SetStringField(TEXT("pin_name"),P.Name.ToString());Link->SetStringField(TEXT("pin_direction"),P.Direction==EGPD_Input?TEXT("input"):TEXT("output"));Link->SetBoolField(TEXT("is_exec"),IsExec);Link->SetStringField(TEXT("linked_node_name"),C.Extra[O].Name);Link->SetStringField(TEXT("linked_node_class"),C.Extra[O].ClassName);Link->SetStringField(TEXT("linked_pin_name"),Peer->Name.ToString());BrokenLinks.Add(MakeShared<FJsonValueObject>(Link));}
 R.Data->SetNumberField(TEXT("observed_exec_link_count"),Exec);R.Data->SetNumberField(TEXT("observed_non_exec_link_count"),Data);auto* Selected=N.Hold.Get();const bool Event=Selected->IsA<UK2Node_Event>();if(Event&&(!AllowOrphan||ExpectedName.IsEmpty()||ExpectedClass.IsEmpty()||ExpectedTitle.IsEmpty()||Exec+Data!=0)){QWFail(R,TEXT("blueprint_structural_event_delete_rejected"),TEXT("Event deletion requires explicit orphan permission, all three expected guards and no links; Any does not override this rule."));return R;}if(!Event&&(Selected->IsA<UK2Node_FunctionEntry>()||Selected->IsA<UK2Node_FunctionResult>())){QWFail(R,TEXT("blueprint_structural_node_delete_rejected"),TEXT("Function Entry/Result are protected."));return R;}
 if((Exec>0&&!AllowAny&&!AllowExec)||(Data>0&&!AllowAny&&!AllowData)){QWFail(R,TEXT("blueprint_linked_node_delete_rejected"),TEXT("Each existing exec/nonexec link category requires its corresponding permission or allow_any_linked_delete."));return R;}TUniquePtr<FRCCompositeDelete> RC;TUniquePtr<FSWTimelineDelete> SW;if(N.Class==UK2Node_Composite::StaticClass()){RC=MakeUnique<FRCCompositeDelete>(C);if(!RC->Begin())return R;}else if(N.Class==UK2Node_Timeline::StaticClass()){SW=MakeUnique<FSWTimelineDelete>(C);if(!SW->Begin())return R;}else if(!C.DeleteProfile())return R;
 const auto Deleted=C.NodeJson(Index,true);auto Before=MakeShared<FJsonObject>();Before->SetBoolField(TEXT("exists"),true);Before->SetNumberField(TEXT("graph_node_count"),C.Nodes.Num());Before->SetObjectField(TEXT("node"),Deleted);R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetObjectField(TEXT("deleted_node"),Deleted);R.Data->SetArrayField(TEXT("broken_links"),BrokenLinks);R.Data->SetNumberField(TEXT("total_link_count"),Exec+Data);R.Data->SetNumberField(TEXT("exec_link_count"),Exec);R.Data->SetNumberField(TEXT("non_exec_link_count"),Data);R.Data->SetBoolField(TEXT("deleted"),false);
 if(!C.Check(TEXT("before_modify")))return R;R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Stage=TEXT("graph_modify");R.Data->SetNumberField(TEXT("graph_modify_calls"),1);G->Modify();if(!C.Check(TEXT("graph_modify_return")))return R;R.Data->SetNumberField(TEXT("node_modify_calls"),1);Selected->Modify();if(!C.Check(TEXT("node_modify_return")))return R;
 R.Stage=TEXT("break_links");C.Broken=true;R.Data->SetNumberField(TEXT("break_all_links_calls"),1);Selected->BreakAllNodeLinks();if(!C.Check(TEXT("break_links_return")))return R;R.bChanged=Exec+Data>0||BP->GetPackage()->IsDirty()!=C.InitialDirty;
 R.Stage=TEXT("destroy_node");C.Retired=true;R.Data->SetNumberField(TEXT("destroy_node_calls"),1);Selected->DestroyNode();Selected=nullptr;if(C.CompositeReturned&&!C.CompositeReturned())return R;if(!C.Check(TEXT("destroy_node_return")))return R;R.bChanged=true;R.Data->SetBoolField(TEXT("deleted"),true);
 R.Stage=TEXT("notify_graph_changed");R.Data->SetNumberField(TEXT("notify_graph_changed_calls"),1);G->NotifyGraphChanged();if(!C.Check(TEXT("notify_graph_changed_return")))return R;
 R.Stage=TEXT("mark_modified");R.Data->SetNumberField(TEXT("mark_modified_calls"),1);FBlueprintEditorUtils::MarkBlueprintAsModified(BP);if(!C.Check(TEXT("mark_modified_return")))return R;
 if(SW&&!SW->Finish())return R;
 auto After=MakeShared<FJsonObject>();After->SetBoolField(TEXT("exists"),false);After->SetNumberField(TEXT("graph_node_count"),C.Nodes.Num()-1);After->SetStringField(TEXT("graph_error"),TEXT(""));R.Data->SetObjectField(TEXT("after"),After);R.Data->SetObjectField(TEXT("graph"),C.GraphJson(C.Nodes.Num()-1));R.Data->SetStringField(TEXT("node_id"),Id);R.Data->SetStringField(TEXT("node_name"),X.Name);R.Data->SetStringField(TEXT("node_class"),X.ClassName);R.Data->SetStringField(TEXT("node_title"),X.Title);R.Data->SetBoolField(TEXT("orphaned_event_delete_explicitly_allowed"),Event&&AllowOrphan);R.Data->SetBoolField(TEXT("postcondition_verified"),true);auto Delta=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("affected_count"),TEXT("removed_count")})Delta->SetNumberField(K,1);for(const TCHAR* K:{TEXT("created_count"),TEXT("updated_count"),TEXT("skipped_count"),TEXT("failed_count")})Delta->SetNumberField(K,0);R.Data->SetObjectField(TEXT("delta"),Delta);R.Data->SetStringField(TEXT("contract_version"),TEXT("mutation_result_v1"));R.Data->SetStringField(TEXT("mode"),TEXT("apply"));R.Data->SetBoolField(TEXT("dry_run"),false);R.Data->SetBoolField(TEXT("DRY_RUN"),false);R.Data->SetBoolField(TEXT("changed"),true);R.Data->SetBoolField(TEXT("mutation_performed"),true);auto Mutation=MakeShared<FJsonObject>();Mutation->SetBoolField(TEXT("attempted"),true);Mutation->SetBoolField(TEXT("performed"),true);Mutation->SetStringField(TEXT("state"),TEXT("performed"));R.Data->SetObjectField(TEXT("mutation"),Mutation);auto Verification=MakeShared<FJsonObject>();Verification->SetStringField(TEXT("status"),TEXT("passed"));Verification->SetStringField(TEXT("method"),TEXT("blueprint_graph_node_guid_relookup"));Verification->SetBoolField(TEXT("before_observed"),true);Verification->SetBoolField(TEXT("after_observed"),true);Verification->SetBoolField(TEXT("strict"),true);TArray<TSharedPtr<FJsonValue>> Checks;for(const TCHAR* K:{TEXT("node_existed_before"),TEXT("node_absent_after"),TEXT("node_count_decremented")})Checks.Add(MakeShared<FJsonValueString>(K));Verification->SetArrayField(TEXT("checks"),Checks);R.Data->SetObjectField(TEXT("verification"),Verification);R.Data->SetBoolField(TEXT("verified_success"),true);R.Data->SetStringField(TEXT("success_semantics"),TEXT("verified_postcondition"));R.Data->SetBoolField(TEXT("success"),true);auto Legacy=MakeShared<FJsonObject>(*R.Data);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.bChangeStateKnown=true;R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}


// QX: distinct saved direct Actor source; read-only planner, existing member-node ABI.
namespace
{
bool QXActorPlanning=false;
struct FQXActorMember
{
 TSharedRef<FJsonObject> ResultJson;Function::FResult Probe;TUniquePtr<FQWNodes> Target;TArray<TUniquePtr<FQWNodes>> SourceGraphs;
 TStrongObjectPtr<UBlueprint> Source;TStrongObjectPtr<UClass> Requested;TStrongObjectPtr<UPackage> SourcePackage;TStrongObjectPtr<UEdGraphSchema_K2> SchemaObject;TArray<TStrongObjectPtr<UObject>> Holds;
 TArray<FBPVariableDescription> Variables;TArray<TObjectPtr<UEdGraph>> FunctionSlots,EventSlots,MacroSlots,DelegateSlots;TObjectPtr<USimpleConstructionScript> SCS;TObjectPtr<UClass> SourceParent;
 FString SourcePath,ClassPath,Filename,FileHash;int64 FileBytes=0;uint32 SourceFlags=0,PackageFlags=0;EBlueprintStatus Status=BS_Unknown;bool Stopped=false,ValuesReady=false;int32 Selected=INDEX_NONE;
 struct FField{FProperty* P=nullptr;FFieldClass* Kind=nullptr;FName Name;FString Path;uint64 Flags=0;int32 Offset=0,Size=0,Dim=0,Align=0;bool Accessors=false;UScriptStruct* Struct=nullptr;uint32 StructFlags=0;TMap<FName,FString> Metadata;};
 struct FClass{TStrongObjectPtr<UClass> Hold;UClass* Meta=nullptr;UObject* Outer=nullptr;FString Path;uint32 Flags=0,ObjectFlags=0;int32 Size=0;TStrongObjectPtr<UObject> CDO;UObject* CDOOuter=nullptr;FString CDOPath;uint32 CDOFlags=0;TArray<FField> Fields;};TArray<FClass> Classes;
 struct FValue{FString Text;int64 Integer=0;double Real=0;bool Boolean=false;};TArray<TArray<FValue>> Defaults;
 FQXActorMember(const TSharedRef<FJsonObject>& R):ResultJson(R){}
 static bool Live(const UObject* O){return FQQFlowContext::Live(O);}
 static bool TypeSame(const FEdGraphPinType& A,const FEdGraphPinType& B){return FQQFlowContext::TypeSame(A,B);}
 bool Bad(const TCHAR* Stage,const FString& Object,const FString& Property,const FString& Expected,const FString& Actual,bool Unknown=true)
 {if(!Stopped){Stopped=true;ResultJson->SetBoolField(TEXT("success"),false);ResultJson->SetBoolField(TEXT("observation_incomplete"),Unknown);ResultJson->SetBoolField(TEXT("change_state_known"),!Unknown);ResultJson->SetStringField(TEXT("stage"),Stage);auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("object"),Object);D->SetStringField(TEXT("property"),Property);D->SetStringField(TEXT("expected"),Expected);D->SetStringField(TEXT("actual"),Actual);ResultJson->SetObjectField(TEXT("first_mismatch"),D);Fail(ResultJson,Unknown?TEXT("graph_external_actor_current_unknown"):TEXT("graph_external_actor_scope_pending"),TEXT("External Actor source/current cannot certify this member. No next SDK, node write, source repair or automatic retry."));}return false;}
 bool TextBounded(const FString& S,int32 Limit=1048576)const
 {if(S.Len()>Limit)return false;for(TCHAR C:S)if(!C)return false;return true;}
 static bool MetadataSame(const TMap<FName,FString>& A,const TMap<FName,FString>& B)
 {if(A.Num()!=B.Num())return false;for(const auto& E:A){const auto* V=B.Find(E.Key);if(!V||!V->Equals(E.Value,ESearchCase::CaseSensitive))return false;}return true;}
 static bool VarSame(const FBPVariableDescription& A,const FBPVariableDescription& B)
 {if(!A.VarName.ToString().Equals(B.VarName.ToString(),ESearchCase::CaseSensitive)||A.VarGuid!=B.VarGuid||!TypeSame(A.VarType,B.VarType)||A.PropertyFlags!=B.PropertyFlags||A.DefaultValue!=B.DefaultValue||A.FriendlyName!=B.FriendlyName||A.RepNotifyFunc!=B.RepNotifyFunc||A.ReplicationCondition!=B.ReplicationCondition||!FQWNodes::TextSame(A.Category,B.Category)||A.MetaDataArray.Num()!=B.MetaDataArray.Num())return false;for(int32 I=0;I<A.MetaDataArray.Num();++I)if(A.MetaDataArray[I].DataKey!=B.MetaDataArray[I].DataKey||A.MetaDataArray[I].DataValue!=B.MetaDataArray[I].DataValue)return false;return true;}
 int32 Kind(const FProperty* P)const
 {const auto* K=P->GetClass();return K==FIntProperty::StaticClass()?1:K==FInt64Property::StaticClass()?2:K==FFloatProperty::StaticClass()?3:K==FDoubleProperty::StaticClass()?4:K==FBoolProperty::StaticClass()?5:K==FNameProperty::StaticClass()?6:K==FStrProperty::StaticClass()?7:0;}
 bool Primitive(const FEdGraphPinType& T)const
 {if(T.PinSubCategoryObject.IsStale()||T.PinSubCategoryObject.Get()!=nullptr||T.PinValueType.TerminalSubCategoryObject.IsStale()||T.PinValueType.TerminalSubCategoryObject.Get()!=nullptr||T.IsContainer()||T.bIsReference||T.bIsConst||T.bIsWeakPointer||T.bIsUObjectWrapper)return false;return T.PinCategory==UEdGraphSchema_K2::PC_Int||T.PinCategory==UEdGraphSchema_K2::PC_Int64||T.PinCategory==UEdGraphSchema_K2::PC_Boolean||T.PinCategory==UEdGraphSchema_K2::PC_Name||T.PinCategory==UEdGraphSchema_K2::PC_String||(T.PinCategory==UEdGraphSchema_K2::PC_Real&&(T.PinSubCategory==UEdGraphSchema_K2::PC_Float||T.PinSubCategory==UEdGraphSchema_K2::PC_Double));}
 bool Read(FProperty* P,UObject* Object,FValue& V)
 {
  const void* A=P->ContainerPtrToValuePtr<void>(Object);switch(Kind(P)){case 1:V.Integer=static_cast<FIntProperty*>(P)->GetPropertyValue(A);break;case 2:V.Integer=static_cast<FInt64Property*>(P)->GetPropertyValue(A);break;case 3:V.Real=static_cast<FFloatProperty*>(P)->GetPropertyValue(A);if(!FMath::IsFinite(V.Real))return false;break;case 4:V.Real=static_cast<FDoubleProperty*>(P)->GetPropertyValue(A);if(!FMath::IsFinite(V.Real))return false;break;case 5:V.Boolean=static_cast<FBoolProperty*>(P)->GetPropertyValue(A);break;case 6:V.Text=static_cast<FNameProperty*>(P)->GetPropertyValue(A).ToString();break;case 7:V.Text=static_cast<FStrProperty*>(P)->GetPropertyValue(A);break;default:return false;}return TextBounded(V.Text);
 }
 static bool ValueSame(const FValue& A,const FValue& B)
 {return A.Text.Equals(B.Text,ESearchCase::CaseSensitive)&&A.Integer==B.Integer&&A.Real==B.Real&&A.Boolean==B.Boolean;}
 FProperty* Own(const FClass& C,FName Name)const
 {for(const auto& F:C.Fields)if(F.Name==Name)return F.P;return nullptr;}
 bool Base(const TCHAR* Stage)
 {
  if(Stopped)return false;for(const auto& H:Holds)if(!Live(H.Get()))return Bad(Stage,SourcePath,TEXT("held_source"),TEXT("same live captured source/owner"),TEXT("unavailable"));if(!Target||!Target->Check(Stage))return Bad(Stage,ClassPath,TEXT("target_current"),TEXT("captured graph/owner"),TEXT("changed"));
  if(!Live(Source.Get())||!Live(SourcePackage.Get())||Source->GetClass()!=UBlueprint::StaticClass()||Source->GetOuter()!=SourcePackage.Get()||Source->GetPackage()!=SourcePackage.Get()||Source->GetPathName()!=SourcePath||uint32(Source->GetFlags())!=SourceFlags||SourcePackage->IsDirty()||uint32(SourcePackage->GetPackageFlags())!=PackageFlags||Source->Status!=Status||Source->BlueprintType!=BPTYPE_Normal||Source->bBeingCompiled||Source->bQueuedForCompilation||Source->bIsRegeneratingOnLoad||Source->bSuppressStructurallyModified||!Source->ParentClass.GetGCPtr().IsResolved()||Source->ParentClass.GetGCPtr()!=SourceParent||!Live(SourceParent.Get())||SourceParent.Get()!=AActor::StaticClass()||!Source->GeneratedClass.GetGCPtr().IsResolved()||!Source->SkeletonGeneratedClass.GetGCPtr().IsResolved()||Classes.Num()!=2||Source->GeneratedClass.GetGCPtr().Get()!=Classes[0].Hold.Get()||Source->SkeletonGeneratedClass.GetGCPtr().Get()!=Classes[1].Hold.Get()||Source->GeneratedClass.GetGCPtr().Get()!=Requested.Get()||Source->NewVariables.Num()!=Variables.Num()||Source->FunctionGraphs!=FunctionSlots||Source->UbergraphPages!=EventSlots||Source->MacroGraphs!=MacroSlots||Source->DelegateSignatureGraphs!=DelegateSlots||!Source->SimpleConstructionScript.IsResolved()||Source->SimpleConstructionScript.Get()!=SCS.Get()||(SCS&&!Live(SCS.Get())))return Bad(Stage,SourcePath,TEXT("source_role"),TEXT("captured saved-clean direct Actor"),TEXT("changed"));
  for(int32 I=0;I<Variables.Num();++I)if(!VarSame(Source->NewVariables[I],Variables[I]))return Bad(Stage,SourcePath,TEXT("authored_variable"),Variables[I].VarName.ToString(),Source->NewVariables[I].VarName.ToString());
  for(const auto& C:Classes){auto* K=C.Hold.Get();if(!Live(K)||K->GetClass()!=C.Meta||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=Source.Get()||K->GetSuperClass()!=AActor::StaticClass()||K->GetOuter()!=C.Outer||K->GetPackage()!=SourcePackage.Get()||K->GetPathName()!=C.Path||uint32(K->GetClassFlags())!=C.Flags||uint32(K->GetFlags())!=C.ObjectFlags||K->GetPropertiesSize()!=C.Size||!K->ClassDefaultObject.IsResolved()||K->ClassDefaultObject.Get()!=C.CDO.Get())return Bad(Stage,C.Path,TEXT("class_role"),TEXT("captured generated/skeleton"),TEXT("changed"));if(C.CDO.Get()&&(!Live(C.CDO.Get())||C.CDO->GetClass()!=K||!C.CDO->HasAnyFlags(RF_ClassDefaultObject)||C.CDO->GetOuter()!=C.CDOOuter||C.CDO->GetPackage()!=SourcePackage.Get()||C.CDO->GetPathName()!=C.CDOPath||uint32(C.CDO->GetFlags())!=C.CDOFlags))return Bad(Stage,C.Path,TEXT("nullable_cdo"),TEXT("captured actual role"),TEXT("changed"));
   TArray<FProperty*> Physical;for(TFieldIterator<FProperty> It(K,EFieldIterationFlags::IncludeDeprecated);It;++It)Physical.Add(*It);if(Physical.Num()!=C.Fields.Num())return Bad(Stage,C.Path,TEXT("property_count"),FString::FromInt(C.Fields.Num()),FString::FromInt(Physical.Num()));
   for(int32 I=0;I<C.Fields.Num();++I){const auto& F=C.Fields[I];if(Physical[I]!=F.P)return Bad(Stage,C.Path,TEXT("property_membership"),F.Path,TEXT("replaced"));auto* P=Physical[I];if(!P||P->GetClass()!=F.Kind||P->GetOwnerStruct()!=K||P->GetFName()!=F.Name||uint64(P->GetPropertyFlags())!=F.Flags||P->GetOffset_ForInternal()!=F.Offset||P->GetSize()!=F.Size||P->ArrayDim!=F.Dim||P->HasSetterOrGetter()!=F.Accessors)return Bad(Stage,F.Path,TEXT("property_descriptor"),TEXT("captured own class/layout/flags"),TEXT("changed"));if(F.Struct){auto* SP=static_cast<FStructProperty*>(P);if(!SP->Struct.IsResolved()||SP->Struct.Get()!=F.Struct||!Live(F.Struct)||F.Struct->GetClass()!=UScriptStruct::StaticClass()||uint32(F.Struct->StructFlags)!=F.StructFlags||F.Struct->GetStructureSize()!=int32(sizeof(FPointerToUberGraphFrame))||F.Struct->GetMinAlignment()!=int32(alignof(FPointerToUberGraphFrame)))return Bad(Stage,F.Path,TEXT("compiler_frame_binding"),TEXT("same held native frame layout"),TEXT("changed"));}if(P->GetMinAlignment()!=F.Align)return Bad(Stage,F.Path,TEXT("alignment"),FString::FromInt(F.Align),FString::FromInt(P->GetMinAlignment()));const auto* M=P->GetMetaDataMap();if((M?M->Num():0)!=F.Metadata.Num()||(M&&!MetadataSame(*M,F.Metadata)))return Bad(Stage,F.Path,TEXT("property_metadata"),TEXT("captured"),TEXT("changed"));}}
  if(!Live(SchemaObject.Get())||SchemaObject->GetClass()!=UEdGraphSchema_K2::StaticClass()||!Target->Schema->ClassDefaultObject.IsResolved()||Target->Schema->ClassDefaultObject.Get()!=SchemaObject.Get())return Bad(Stage,Target->GraphPath,TEXT("schema_cdo"),TEXT("same existing resolved K2 CDO"),TEXT("changed"));
  for(const auto& G:SourceGraphs)if(!G->Check(Stage))return Bad(Stage,G->GraphPath,TEXT("source_graph_current"),TEXT("captured authored graph/pins"),TEXT("changed"));return true;
 }
 bool Current(const TCHAR* Stage)
 {
  if(!Base(Stage))return false;if(ValuesReady)for(int32 C=0;C<Classes.Num();++C)if(Classes[C].CDO.Get())for(int32 I=0;I<Variables.Num();++I){FValue V;if(!Read(Own(Classes[C],Variables[I].VarName),Classes[C].CDO.Get(),V)||!ValueSame(V,Defaults[C][I]))return Bad(Stage,Classes[C].CDOPath,Variables[I].VarName.ToString(),TEXT("captured own primitive default"),TEXT("changed"));}return true;
 }
 bool Disk(FString& Hash,int64& Bytes)
 {
  if(!Current(TEXT("before_source_file")))return false;TArray<uint8> Data;Bytes=IFileManager::Get().FileSize(*Filename);if(Bytes<=0||Bytes>64*1024*1024||!FFileHelper::LoadFileToArray(Data,*Filename)||Data.Num()!=Bytes)return Bad(TEXT("source_file"),Filename,TEXT("file"),TEXT("persisted nonempty <=64Mi"),TEXT("unavailable"),false);if(!Current(TEXT("source_file_return")))return false;uint8 Digest[20];FSHA1::HashBuffer(Data.GetData(),Data.Num(),Digest);Hash=BytesToHex(Digest,20);return true;
 }
 bool Begin(UEdGraph* G,UClass* K,const FString& Name)
 {
  Requested.Reset(K);if(!Live(K)||!K->ClassGeneratedBy.IsResolved())return Bad(TEXT("source_admission"),TEXT(""),TEXT("class"),TEXT("loaded generated class"),TEXT("unavailable"),false);UObject* ReturnedSource=K->ClassGeneratedBy.Get();TStrongObjectPtr<UObject> HeldSource(ReturnedSource);if(!Live(ReturnedSource)||ReturnedSource->GetClass()!=UBlueprint::StaticClass())return Bad(TEXT("source_admission"),TEXT(""),TEXT("source_class_generated_by"),TEXT("live exact Blueprint"),TEXT("unsupported"),false);Source.Reset(static_cast<UBlueprint*>(ReturnedSource));if(!Live(Source.Get()))return Bad(TEXT("source_admission"),TEXT(""),TEXT("blueprint"),TEXT("loaded own blueprint"),TEXT("unavailable"),false);
  UBlueprint* B=FBlueprintEditorUtils::FindBlueprintForGraph(G);if(!Live(B)||B==Source.Get()||B->GetPackage()==Source->GetPackage()||B->BlueprintType!=BPTYPE_Normal)return Bad(TEXT("source_admission"),K->GetPathName(),TEXT("distinct_owner"),TEXT("distinct normal target and source packages"),TEXT("unsupported"),false);const FString TargetGraphPath=G->GetPathName();Target=MakeUnique<FQWNodes>(Probe);if(!Target->Bound(B,G))return Bad(TEXT("target_capture"),TargetGraphPath,TEXT("target"),TEXT("current concrete graph"),TEXT("unavailable"));
  SourcePackage.Reset(Source->GetPackage());SourcePath=Source->GetPathName();ClassPath=K->GetPathName();if(Source->GetClass()!=UBlueprint::StaticClass()||Source->GetOuter()!=SourcePackage.Get()||!SourcePath.StartsWith(TEXT("/Game/"))||SourcePackage->IsDirty()||Source->BlueprintType!=BPTYPE_Normal||(Source->Status!=BS_UpToDate&&Source->Status!=BS_UpToDateWithWarnings)||!Source->ParentClass.GetGCPtr().IsResolved()||Source->ParentClass.GetGCPtr().Get()!=AActor::StaticClass()||!Source->GeneratedClass.GetGCPtr().IsResolved()||Source->GeneratedClass.GetGCPtr().Get()!=K||!Source->SkeletonGeneratedClass.GetGCPtr().IsResolved()||Source->NewVariables.IsEmpty()||Source->NewVariables.Num()>32||!Source->SimpleConstructionScript.IsResolved())return Bad(TEXT("source_admission"),SourcePath,TEXT("scope"),TEXT("saved-clean compiled direct Actor own1..32 scalar declarations"),TEXT("unsupported"),false);
  SourceFlags=uint32(Source->GetFlags());PackageFlags=uint32(SourcePackage->GetPackageFlags());Status=Source->Status;Variables=Source->NewVariables;SourceParent=Source->ParentClass.GetGCPtr();SCS=Source->SimpleConstructionScript;for(UObject* O:{static_cast<UObject*>(SourceParent.Get()),static_cast<UObject*>(SCS.Get())})if(O){if(!Live(O))return Bad(TEXT("source_capture"),SourcePath,TEXT("owner"),TEXT("live"),TEXT("unavailable"));Holds.Emplace(O);}
  TSet<FGuid> Guids;TSet<FName> Names;for(int32 I=0;I<Variables.Num();++I){const auto& D=Variables[I];if(D.VarName.ToString()==Name)Selected=I;if(!D.VarGuid.IsValid()||Guids.Contains(D.VarGuid)||Names.Contains(D.VarName)||!Primitive(D.VarType)||!TextBounded(D.DefaultValue)||D.MetaDataArray.Num()>128)return Bad(TEXT("source_admission"),SourcePath,TEXT("declaration"),TEXT("unique primitive value/GUID"),D.VarName.ToString(),false);Guids.Add(D.VarGuid);Names.Add(D.VarName);}
  if(Selected==INDEX_NONE)return Bad(TEXT("source_admission"),SourcePath,TEXT("variable_name"),Name,TEXT("missing own declaration"),false);
  for(UClass* C:{Source->GeneratedClass.GetGCPtr().Get(),Source->SkeletonGeneratedClass.GetGCPtr().Get()}){if(!Live(C)||!Live(C->GetClass())||!C->GetClass()->IsChildOf(UBlueprintGeneratedClass::StaticClass())||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=Source.Get()||C->GetSuperClass()!=AActor::StaticClass()||!C->ClassDefaultObject.IsResolved())return Bad(TEXT("source_class_capture"),ClassPath,TEXT("class"),TEXT("current own generated class role"),TEXT("unsupported"),false);FClass E;E.Hold.Reset(C);E.Meta=C->GetClass();E.Outer=C->GetOuter();for(UObject* O:{static_cast<UObject*>(E.Meta),E.Outer})if(O){if(!Live(O))return Bad(TEXT("source_class_capture"),ClassPath,TEXT("owner"),TEXT("live captured actual outer/meta"),TEXT("unavailable"));Holds.Emplace(O);}E.Path=C->GetPathName();E.Flags=uint32(C->GetClassFlags());E.ObjectFlags=uint32(C->GetFlags());E.Size=C->GetPropertiesSize();E.CDO.Reset(C->ClassDefaultObject.Get());if(E.CDO.Get()){if(!Live(E.CDO.Get())||E.CDO->GetClass()!=C||E.CDO->GetPackage()!=SourcePackage.Get()||!E.CDO->HasAnyFlags(RF_ClassDefaultObject))return Bad(TEXT("source_class_capture"),E.Path,TEXT("nullable_cdo"),TEXT("null or current exact class CDO"),TEXT("unavailable"));E.CDOOuter=E.CDO->GetOuter();if(E.CDOOuter){if(!Live(E.CDOOuter))return Bad(TEXT("source_class_capture"),E.Path,TEXT("cdo_outer"),TEXT("live captured actual outer"),TEXT("unavailable"));Holds.Emplace(E.CDOOuter);}E.CDOPath=E.CDO->GetPathName();E.CDOFlags=uint32(E.CDO->GetFlags());}
   for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::IncludeDeprecated);It;++It){auto* P=*It;if(!P||P->GetOwnerStruct()!=C||E.Fields.Num()>=256)return Bad(TEXT("source_property_capture"),E.Path,TEXT("fields"),TEXT("bounded own property inventory"),TEXT("unsupported"),false);FField F;F.P=P;F.Kind=P->GetClass();F.Name=P->GetFName();F.Path=P->GetPathName();F.Flags=uint64(P->GetPropertyFlags());F.Offset=P->GetOffset_ForInternal();F.Size=P->GetSize();F.Dim=P->ArrayDim;if(F.Kind==FStructProperty::StaticClass()){auto* SP=static_cast<FStructProperty*>(P);if(!SP->Struct.IsResolved())return Bad(TEXT("source_property_capture"),F.Path,TEXT("compiler_frame_binding"),TEXT("resolved native frame struct"),TEXT("unresolved"),false);F.Struct=SP->Struct.Get();if(!Live(F.Struct)||F.Struct->GetClass()!=UScriptStruct::StaticClass()||!(F.Struct->StructFlags&STRUCT_Native)||F.Struct!=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.PointerToUberGraphFrame"))||F.Struct->GetStructureSize()!=int32(sizeof(FPointerToUberGraphFrame))||F.Struct->GetMinAlignment()!=int32(alignof(FPointerToUberGraphFrame))||!P->HasAnyPropertyFlags(CPF_Transient))return Bad(TEXT("source_property_capture"),F.Path,TEXT("physical_struct_role"),TEXT("stock transient native PointerToUberGraphFrame; authored variables remain scalar"),TEXT("unsupported"),false);F.StructFlags=uint32(F.Struct->StructFlags);Holds.Emplace(F.Struct);}F.Align=P->GetMinAlignment();F.Accessors=P->HasSetterOrGetter();if(const auto* M=P->GetMetaDataMap()){if(M->Num()>256)return Bad(TEXT("source_property_capture"),F.Path,TEXT("metadata"),TEXT("bounded"),TEXT("too many"),false);F.Metadata=*M;}E.Fields.Add(MoveTemp(F));}
   for(const auto& D:Variables){auto* P=Own(E,D.VarName);if(!P||Kind(P)==0||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetOffset_ForInternal()<0||P->GetSize()<=0||int64(P->GetOffset_ForInternal())+P->GetSize()>E.Size||P->HasAnyPropertyFlags(CPF_Parm|CPF_OutParm|CPF_ReferenceParm|CPF_ConstParm|CPF_EditorOnly))return Bad(TEXT("source_property_capture"),E.Path,D.VarName.ToString(),TEXT("direct scalar layout"),TEXT("unsupported"),false);}Classes.Add(MoveTemp(E));}
  if(!Target->Schema->ClassDefaultObject.IsResolved())return Bad(TEXT("schema_admission"),Target->GraphPath,TEXT("schema_cdo"),TEXT("existing resolved K2 CDO"),TEXT("unresolved"),false);UObject* ReturnedSchema=Target->Schema->ClassDefaultObject.Get();TStrongObjectPtr<UObject> HeldSchema(ReturnedSchema);if(!Live(ReturnedSchema)||ReturnedSchema->GetClass()!=UEdGraphSchema_K2::StaticClass())return Bad(TEXT("schema_admission"),Target->GraphPath,TEXT("schema_cdo"),TEXT("existing exact K2 CDO"),TEXT("missing"),false);SchemaObject.Reset(static_cast<UEdGraphSchema_K2*>(ReturnedSchema));
  FunctionSlots=Source->FunctionGraphs;EventSlots=Source->UbergraphPages;MacroSlots=Source->MacroGraphs;DelegateSlots=Source->DelegateSignatureGraphs;
  TSet<UEdGraph*> Seen;for(const auto* Slots:{&FunctionSlots,&EventSlots,&MacroSlots,&DelegateSlots})for(const auto& H:*Slots){if(!H.IsResolved()||!Live(H.Get())||Seen.Contains(H.Get())||Seen.Num()>=64)return Bad(TEXT("source_graph_capture"),SourcePath,TEXT("graphs"),TEXT("bounded unique resolved own graphs"),TEXT("unsupported"),false);Seen.Add(H.Get());const FString CopiedGraphPath=H->GetPathName();auto Scope=MakeUnique<FQWNodes>(Probe);if(!Scope->Bound(Source.Get(),H.Get()))return Bad(TEXT("source_graph_capture"),CopiedGraphPath,TEXT("graph"),TEXT("capturable authored roles"),TEXT("unavailable"));for(const auto& N:Scope->Nodes)for(const auto& U:N.UserPins){if(U.PinType.PinSubCategoryObject.IsStale()||U.PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(TEXT("source_graph_capture"),CopiedGraphPath,TEXT("user_pin_source"),TEXT("nonstale nullable type binding"),TEXT("stale"));for(UObject* O:{U.PinType.PinSubCategoryObject.Get(),U.PinType.PinValueType.TerminalSubCategoryObject.Get()})if(O){if(!Live(O))return Bad(TEXT("source_graph_capture"),CopiedGraphPath,TEXT("user_pin_source"),TEXT("live captured type binding"),TEXT("unavailable"));Holds.Emplace(O);}}SourceGraphs.Add(MoveTemp(Scope));}
  if(!Base(TEXT("before_defaults")))return false;Defaults.SetNum(Classes.Num());for(int32 C=0;C<Classes.Num();++C){if(Classes[C].CDO.Get())for(const auto& D:Variables){FValue V;if(!Read(Own(Classes[C],D.VarName),Classes[C].CDO.Get(),V))return Bad(TEXT("source_default_capture"),Classes[C].CDOPath,D.VarName.ToString(),TEXT("finite primitive value"),TEXT("unsupported"),false);Defaults[C].Add(MoveTemp(V));}}ValuesReady=true;
  if(!FPackageName::TryConvertLongPackageNameToFilename(SourcePackage->GetName(),Filename,FPackageName::GetAssetPackageExtension()))return Bad(TEXT("source_file"),SourcePath,TEXT("package_filename"),TEXT("saved package"),TEXT("unavailable"),false);return Disk(FileHash,FileBytes);
 }
 TSharedRef<FJsonObject> FullType(const FEdGraphPinType& T)
 {auto J=PinTypeObservation(T);J->SetBoolField(TEXT("is_weak_pointer"),T.bIsWeakPointer);J->SetBoolField(TEXT("is_uobject_wrapper"),T.bIsUObjectWrapper);J->SetBoolField(TEXT("serialize_single_precision"),T.bSerializeAsSinglePrecisionFloat);auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("category"),T.PinValueType.TerminalCategory.ToString());V->SetStringField(TEXT("subcategory"),T.PinValueType.TerminalSubCategory.ToString());V->SetBoolField(TEXT("is_const"),T.PinValueType.bTerminalIsConst);V->SetBoolField(TEXT("is_weak_pointer"),T.PinValueType.bTerminalIsWeakPointer);V->SetBoolField(TEXT("is_uobject_wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);V->SetStringField(TEXT("type_object"),T.PinValueType.TerminalSubCategoryObject.IsValid()?T.PinValueType.TerminalSubCategoryObject->GetPathName():FString());J->SetObjectField(TEXT("terminal"),V);return J;}
 bool Reference(const FMemberReference& Ref,TSharedRef<FJsonObject> J,const TCHAR* Key)
 {auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),Ref.GetMemberName().ToString());V->SetStringField(TEXT("guid"),Ref.GetMemberGuid().ToString(EGuidFormats::Digits));V->SetBoolField(TEXT("self"),Ref.IsSelfContext());V->SetBoolField(TEXT("local"),Ref.IsLocalScope());V->SetBoolField(TEXT("deprecated"),Ref.IsDeprecated());V->SetStringField(TEXT("scope"),Ref.GetMemberScopeName());UClass* Returned=Ref.GetMemberParentClass();TStrongObjectPtr<UClass> Held(Returned);if(!Current(TEXT("copied_reference_parent_return")))return false;if(Returned&&!Live(Returned))return Bad(TEXT("reference_parent"),SourcePath,TEXT("parent"),TEXT("live or null"),TEXT("unavailable"));V->SetStringField(TEXT("parent_class"),Returned?Returned->GetPathName():FString());J->SetObjectField(Key,V);return true;}
 TSharedRef<FJsonObject> Snapshot()
 {
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("blueprint_path"),SourcePath);J->SetStringField(TEXT("class_path"),ClassPath);J->SetBoolField(TEXT("dirty"),false);J->SetNumberField(TEXT("status"),int32(Status));J->SetStringField(TEXT("file_hash_algorithm"),TEXT("SHA1"));J->SetStringField(TEXT("file_hash"),FileHash);J->SetNumberField(TEXT("file_bytes"),double(FileBytes));J->SetBoolField(TEXT("cdo_created"),false);
  TArray<TSharedPtr<FJsonValue>> VarRows;for(const auto& D:Variables){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),D.VarName.ToString());V->SetStringField(TEXT("guid"),D.VarGuid.ToString(EGuidFormats::Digits));V->SetObjectField(TEXT("type"),FullType(D.VarType));const FText Category=D.Category;const FString CategoryText=Category.ToString();if(!Current(TEXT("copied_category_return")))return J;V->SetStringField(TEXT("category"),CategoryText);V->SetStringField(TEXT("declared_default"),D.DefaultValue);V->SetStringField(TEXT("descriptor_flags"),LexToString(uint64(D.PropertyFlags)));V->SetStringField(TEXT("friendly_name"),D.FriendlyName);auto M=MakeShared<FJsonObject>();for(const auto& E:D.MetaDataArray)M->SetStringField(E.DataKey.ToString(),E.DataValue);V->SetObjectField(TEXT("metadata"),M);VarRows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("declarations"),VarRows);
  TArray<TSharedPtr<FJsonValue>> ClassRows;for(int32 I=0;I<Classes.Num();++I){const auto& C=Classes[I];auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),C.Path);V->SetStringField(TEXT("meta_class"),C.Meta->GetPathName());V->SetStringField(TEXT("outer"),C.Outer->GetPathName());V->SetNumberField(TEXT("properties_size"),C.Size);V->SetStringField(TEXT("class_flags"),LexToString(C.Flags));V->SetStringField(TEXT("object_flags"),LexToString(C.ObjectFlags));V->SetBoolField(TEXT("cdo_present"),C.CDO.Get()!=nullptr);V->SetStringField(TEXT("cdo_path"),C.CDOPath);V->SetStringField(TEXT("cdo_outer"),C.CDOOuter?C.CDOOuter->GetPathName():FString());
   TArray<TSharedPtr<FJsonValue>> FieldRows;for(const auto& F:C.Fields){auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("path"),F.Path);E->SetStringField(TEXT("kind"),F.Kind->GetName());E->SetStringField(TEXT("flags"),LexToString(F.Flags));E->SetNumberField(TEXT("offset"),F.Offset);E->SetNumberField(TEXT("size"),F.Size);E->SetNumberField(TEXT("alignment"),F.Align);E->SetNumberField(TEXT("array_dim"),F.Dim);E->SetStringField(TEXT("compiler_struct"),F.Struct?TEXT("/Script/Engine.PointerToUberGraphFrame"):TEXT(""));auto M=MakeShared<FJsonObject>();for(const auto& A:F.Metadata)M->SetStringField(A.Key.ToString(),A.Value);E->SetObjectField(TEXT("metadata"),M);FieldRows.Add(MakeShared<FJsonValueObject>(E));}V->SetArrayField(TEXT("physical_properties"),FieldRows);
   TArray<TSharedPtr<FJsonValue>> ValueRows;for(int32 X=0;X<Defaults[I].Num();++X){const auto& A=Defaults[I][X];auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("name"),Variables[X].VarName.ToString());E->SetStringField(TEXT("integer"),LexToString(A.Integer));E->SetNumberField(TEXT("real"),A.Real);E->SetBoolField(TEXT("bool"),A.Boolean);E->SetStringField(TEXT("string"),A.Text);ValueRows.Add(MakeShared<FJsonValueObject>(E));}V->SetArrayField(TEXT("own_primitive_defaults"),ValueRows);ClassRows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("class_roles"),ClassRows);
  TArray<TSharedPtr<FJsonValue>> GraphRows;for(const auto& G:SourceGraphs){auto V=G->GraphJson(G->Nodes.Num());TArray<TSharedPtr<FJsonValue>> NodeRows;for(int32 I=0;I<G->Nodes.Num();++I){auto N=G->NodeJson(I,true);const auto& E=G->Nodes[I];const auto& X=G->Extra[I];if(E.HasRef&&!Reference(E.Ref,N,TEXT("member_reference")))return J;if(X.Variable&&!Reference(X.VariableRef,N,TEXT("variable_reference")))return J;const auto& PinRows=N->GetArrayField(TEXT("pins"));for(int32 P=0;P<E.Pins.Num();++P){const auto& A=E.Pins[P];auto Value=PinRows[P]->AsObject();Value->SetObjectField(TEXT("full_type"),FullType(A.Type));Value->SetStringField(TEXT("autogenerated_default"),A.Auto);Value->SetNumberField(TEXT("flags"),A.Flags);const FText Copied=A.Text;const FString Text=Copied.ToString();if(!Current(TEXT("copied_pin_text_return")))return J;Value->SetStringField(TEXT("default_text"),Text);const FText Friendly=A.Friendly;const FString FriendlyText=Friendly.ToString();if(!Current(TEXT("copied_pin_friendly_return")))return J;Value->SetStringField(TEXT("friendly_name"),FriendlyText);Value->SetStringField(TEXT("parent_pin_id"),A.Parent?G->OriginalIds.FindChecked(A.Parent).ToString():FString());TArray<TSharedPtr<FJsonValue>> Children;for(auto* Child:A.Children)Children.Add(MakeShared<FJsonValueString>(G->OriginalIds.FindChecked(Child).ToString()));Value->SetArrayField(TEXT("sub_pin_ids"),Children);}N->SetStringField(TEXT("reference_name"),E.HasRef?E.Ref.GetMemberName().ToString():FString());N->SetStringField(TEXT("reference_guid"),E.HasRef?E.Ref.GetMemberGuid().ToString(EGuidFormats::Digits):FString());N->SetBoolField(TEXT("reference_self"),E.HasRef&&E.Ref.IsSelfContext());N->SetStringField(TEXT("variable_reference_name"),X.Variable?X.VariableRef.GetMemberName().ToString():FString());N->SetStringField(TEXT("variable_reference_guid"),X.Variable?X.VariableRef.GetMemberGuid().ToString(EGuidFormats::Digits):FString());N->SetStringField(TEXT("custom_event_name"),X.Custom?X.CustomName.ToString():FString());N->SetNumberField(TEXT("custom_event_flags"),X.CustomFlags);TArray<TSharedPtr<FJsonValue>> User;for(const auto& U:E.UserPins){auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),U.PinName.ToString());P->SetNumberField(TEXT("direction"),int32(U.DesiredPinDirection));P->SetObjectField(TEXT("type"),FullType(U.PinType));P->SetStringField(TEXT("default"),U.PinDefaultValue);User.Add(MakeShared<FJsonValueObject>(P));}N->SetArrayField(TEXT("user_pins"),User);NodeRows.Add(MakeShared<FJsonValueObject>(N));}V->SetArrayField(TEXT("nodes"),NodeRows);GraphRows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("captured_graphs"),GraphRows);return J;
 }
};
TSharedRef<FJsonObject> QXExternalActorPlan(UEdGraph* G,const FString& Name,bool Setter,UClass* Class,const TSharedRef<FJsonObject>& R)
{
 R->SetStringField(TEXT("member_scope"),TEXT("external_generated_actor"));R->SetBoolField(TEXT("external_actor_member"),true);R->SetBoolField(TEXT("observation_incomplete"),false);R->SetBoolField(TEXT("change_state_known"),true);if(QXActorPlanning){Fail(R,TEXT("external_actor_plan_reentrant"),TEXT("Actor member planner is already active."));return R;}TGuardValue<bool> Planning(QXActorPlanning,true);FQXActorMember C(R);if(!C.Begin(G,Class,Name))return R;
 auto* P=C.Own(C.Classes[1],C.Variables[C.Selected].VarName);auto* Compiled=C.Own(C.Classes[0],C.Variables[C.Selected].VarName);FEdGraphPinType Type,CompiledType;
 const bool Converted=C.SchemaObject->ConvertPropertyToPinType(P,Type);if(!C.Current(TEXT("skeleton_type_return")))return R;const bool CompiledConverted=C.SchemaObject->ConvertPropertyToPinType(Compiled,CompiledType);if(!C.Current(TEXT("generated_type_return")))return R;
 if(!Converted||!CompiledConverted||!C.Primitive(Type)||!C.TypeSame(Type,CompiledType)||!C.TypeSame(Type,C.Variables[C.Selected].VarType)){C.Bad(TEXT("member_type"),C.SourcePath,Name,TEXT("exact authored/skeleton/generated primitive pin type"),TEXT("mismatch"),false);return R;}
 FMemberReference CompiledRef,SkeletonRef;CompiledRef.SetFromField<FProperty>(Compiled,false);if(!C.Current(TEXT("compiled_member_reference_return")))return R;SkeletonRef.SetFromField<FProperty>(P,false);if(!C.Current(TEXT("skeleton_member_reference_return")))return R;UClass* CompiledOwner=CompiledRef.GetMemberParentClass();TStrongObjectPtr<UClass> HeldCompiledOwner(CompiledOwner);if(!C.Current(TEXT("compiled_authoritative_owner_return")))return R;UClass* SkeletonOwner=SkeletonRef.GetMemberParentClass();TStrongObjectPtr<UClass> HeldSkeletonOwner(SkeletonOwner);if(!C.Current(TEXT("skeleton_authoritative_owner_return")))return R;if(CompiledOwner!=Class||SkeletonOwner!=Class||CompiledRef.IsSelfContext()||SkeletonRef.IsSelfContext()||!CompiledRef.IsSameReference(SkeletonRef)||CompiledRef.GetMemberGuid()!=C.Variables[C.Selected].VarGuid||SkeletonRef.GetMemberGuid()!=C.Variables[C.Selected].VarGuid){C.Bad(TEXT("member_guid_binding"),C.SourcePath,Name,C.Variables[C.Selected].VarGuid.ToString(),CompiledRef.GetMemberGuid().ToString()+TEXT("/")+SkeletonRef.GetMemberGuid().ToString(),false);return R;}
 for(auto* F:{P,Compiled}){const bool Private=FBlueprintEditorUtils::IsPropertyPrivate(F);if(!C.Current(TEXT("property_private_return")))return R;const auto Readable=FBlueprintEditorUtils::IsPropertyReadableInBlueprint(C.Target->BP.Get(),F);if(!C.Current(TEXT("property_readable_return")))return R;const auto Writable=Setter?FBlueprintEditorUtils::IsPropertyWritableInBlueprint(C.Target->BP.Get(),F):FBlueprintEditorUtils::EPropertyWritableState::Writable;if(!C.Current(TEXT("property_writable_return")))return R;if(Private||Readable!=FBlueprintEditorUtils::EPropertyReadableState::Readable||Writable!=FBlueprintEditorUtils::EPropertyWritableState::Writable){C.Bad(TEXT("property_access"),F->GetPathName(),TEXT("read/write"),TEXT("public Blueprint access"),TEXT("denied"),false);return R;}}
 FEdGraphPinType Self;Self.PinCategory=UEdGraphSchema_K2::PC_Object;Self.PinSubCategoryObject=Class;FString TypeText,SelfText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&Type,&Type,nullptr,PPF_None,nullptr);if(!C.Current(TEXT("value_type_export_return")))return R;FEdGraphPinType::StaticStruct()->ExportText(SelfText,&Self,&Self,nullptr,PPF_None,nullptr);if(!C.Current(TEXT("self_type_export_return")))return R;
 const auto Snapshot=C.Snapshot();if(!C.Current(TEXT("snapshot_publication")))return R;FString FinalHash;int64 FinalBytes=0;if(!C.Disk(FinalHash,FinalBytes))return R;if(FinalHash!=C.FileHash||FinalBytes!=C.FileBytes){C.Bad(TEXT("source_file_final"),C.Filename,TEXT("file"),C.FileHash,FinalHash);return R;}
 auto Validation=MakeShared<FJsonObject>();Validation->SetBoolField(TEXT("success"),true);Validation->SetStringField(TEXT("scope"),TEXT("saved_distinct_direct_actor_own_primitive_member"));Validation->SetObjectField(TEXT("source_snapshot"),Snapshot);
 R->SetStringField(TEXT("compiled_member_guid"),CompiledRef.GetMemberGuid().ToString(EGuidFormats::Digits));R->SetStringField(TEXT("skeleton_member_guid"),SkeletonRef.GetMemberGuid().ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("member_authoritative_owner_verified"),true);R->SetObjectField(TEXT("external_actor_source_snapshot"),Snapshot);R->SetObjectField(TEXT("external_source_validation"),Validation);R->SetObjectField(TEXT("external_source_before"),Snapshot);R->SetObjectField(TEXT("external_source_after"),Snapshot);R->SetBoolField(TEXT("external_source_observed_unchanged"),true);R->SetBoolField(TEXT("dirty_before"),C.Target->InitialDirty);R->SetBoolField(TEXT("dirty_after"),C.Target->BP->GetPackage()->IsDirty());R->SetStringField(TEXT("variable_name"),Name);R->SetStringField(TEXT("variable_guid"),C.Variables[C.Selected].VarGuid.ToString(EGuidFormats::Digits));R->SetBoolField(TEXT("setter"),Setter);R->SetStringField(TEXT("target_class_path"),C.ClassPath);R->SetStringField(TEXT("declaring_class_path"),C.ClassPath);R->SetStringField(TEXT("declaring_blueprint_path"),C.SourcePath);R->SetStringField(TEXT("property_path"),P->GetPathName());R->SetStringField(TEXT("compiled_property_path"),Compiled->GetPathName());R->SetStringField(TEXT("pin_category"),Type.PinCategory.ToString());R->SetStringField(TEXT("pin_subcategory"),Type.PinSubCategory.ToString());R->SetStringField(TEXT("expected_pin_type_text"),TypeText);R->SetStringField(TEXT("expected_self_pin_type_text"),SelfText);R->SetStringField(TEXT("scope"),TEXT("Saved distinct direct Actor source own primitive member declaration/property/GUID, current nullable CDO own values and captured graph/pin/file roles. Existing reflected member writer remains separate; no runtime source instance/value write, loader/CDO warm, explicit compile/save or full private/cache proof."));
 if(!C.Current(TEXT("member_plan_publication")))return R;Complete(R,G);if(!C.Current(TEXT("member_complete_return")))return R;return R;
}
}


}
