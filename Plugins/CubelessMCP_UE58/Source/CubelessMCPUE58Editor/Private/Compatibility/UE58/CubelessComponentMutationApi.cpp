// UE56 Blueprint add/set contracts, adapted to UE58 public editor APIs.
// Original: UnrealMCPBlueprintCommands.cpp HandleAddComponentToBlueprint /
// HandleSetComponentProperty. Original source/license remain in archive/ue56.
#include "Compatibility/UE58/CubelessComponentMutationApi.h"
#include "Compatibility/UE58/CubelessComponentApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Camera/CameraComponent.h"
#include "NiagaraComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Compatibility/UE58/CubelessPCGApi.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "NiagaraSystem.h"
#include "ScopedTransaction.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/TargetPoint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/TextureLightProfile.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/PropertyAccessUtil.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"

namespace Cubeless::UE58::Component
{
namespace
{
constexpr int32 MaxNodes = 10000, MaxJsonChars = 65536, MaxProperties = 64;
bool Fail(FMutationResult& R, const TCHAR* Code, const FString& Error)
{ R.ErrorCode = Code; R.Error = Error; return false; }
bool SafeName(const FString& Name, bool bEmpty = false)
{
    if ((!bEmpty && Name.IsEmpty()) || Name.Len() >= NAME_SIZE) { return false; }
    for (TCHAR C : Name) { if (C < 32 || C == 127) { return false; } }
    return true;
}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const FString& Value : Values) { Out.Add(MakeShared<FJsonValueString>(Value)); }
    return Out;
}
TArray<FString> DirtyPackages()
{
    TArray<FString> Out;
    ForEachObjectOfClass(UPackage::StaticClass(), [&Out](UObject* Object)
    { UPackage* Package = CastChecked<UPackage>(Object); if (Package->IsDirty()) { Out.Add(Package->GetName()); } });
    Out.Sort(); return Out;
}
bool BoundedJson(const TSharedPtr<FJsonValue>& Value, int32 Depth, int32& Count)
{
    if (!Value || Depth > 8 || ++Count > 1024) { return false; }
    if (Value->Type == EJson::Number) { return FMath::IsFinite(Value->AsNumber()) && FMath::Abs(Value->AsNumber()) <= 9007199254740991.0; }
    if (Value->Type == EJson::String) { for (TCHAR C : Value->AsString()) { if (C == 0) { return false; } } }
    if (Value->Type == EJson::Array) { for (const auto& Item : Value->AsArray()) { if (!BoundedJson(Item, Depth + 1, Count)) { return false; } } }
    if (Value->Type == EJson::Object) { for (const auto& Pair : Value->AsObject()->Values) { if (!SafeName(FString(Pair.Key.ToView())) || !BoundedJson(Pair.Value, Depth + 1, Count)) { return false; } } }
    return true;
}
bool ParseValue(const FString& Json, TSharedPtr<FJsonValue>& Value, FMutationResult& R)
{
    if (Json.IsEmpty() || Json.Len() > MaxJsonChars) { return Fail(R, TEXT("component_json_invalid"), TEXT("JSON text must contain 1..65536 UTF-16 characters.")); }
    // Reject duplicate keys before object deserialization can overwrite them.
    auto Reader = TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]"));
    TArray<TSet<FString>> Keys; TArray<bool> Objects; EJsonNotation Notation;
    while (Reader->ReadNext(Notation))
    {
        if (Notation == EJsonNotation::Error) { return Fail(R, TEXT("component_json_invalid"), TEXT("Malformed JSON.")); }
        if (Notation != EJsonNotation::ObjectEnd && Notation != EJsonNotation::ArrayEnd && !Objects.IsEmpty() && Objects.Last())
        { const FString& Key = Reader->GetIdentifier(); if (Keys.Last().Contains(Key)) { return Fail(R, TEXT("component_json_invalid"), TEXT("Duplicate JSON object key.")); } Keys.Last().Add(Key); }
        if (Notation == EJsonNotation::ObjectStart || Notation == EJsonNotation::ArrayStart)
        { Objects.Add(Notation == EJsonNotation::ObjectStart); Keys.AddDefaulted(); if (Objects.Num() > 10) { return Fail(R, TEXT("component_json_invalid"), TEXT("JSON nesting limit exceeded.")); } }
        else if (Notation == EJsonNotation::ObjectEnd || Notation == EJsonNotation::ArrayEnd) { Objects.Pop(); Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Values) || Values.Num() != 1)
    { return Fail(R, TEXT("component_json_invalid"), TEXT("Provide exactly one JSON value; string values require JSON quotes.")); }
    int32 Count = 0;
    if (!BoundedJson(Values[0], 0, Count)) { return Fail(R, TEXT("component_json_invalid"), TEXT("JSON exceeds depth/value/name/number bounds.")); }
    Value = Values[0]; return true;
}
bool NiagaraComponentName(const FString& Name)
{ return Name == TEXT("NiagaraComponent") || Name == TEXT("UNiagaraComponent") || Name == TEXT("Niagara") || Name == TEXT("/Script/Niagara.NiagaraComponent"); }
UClass* SupportedClass(const FString& Name)
{
    if (NiagaraComponentName(Name)) { return FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraComponent")); }
    UClass* Classes[] = {USceneComponent::StaticClass(), UStaticMeshComponent::StaticClass(), UCameraComponent::StaticClass(), USpringArmComponent::StaticClass(), USphereComponent::StaticClass(), UBoxComponent::StaticClass(), UCapsuleComponent::StaticClass(), USplineComponent::StaticClass(), UInstancedStaticMeshComponent::StaticClass(), UHierarchicalInstancedStaticMeshComponent::StaticClass(), USkeletalMeshComponent::StaticClass(), UPointLightComponent::StaticClass(), USpotLightComponent::StaticClass(), URectLightComponent::StaticClass(), UDirectionalLightComponent::StaticClass()};
    for (UClass* Class : Classes)
    { if (Name == Class->GetName() || Name == TEXT("U") + Class->GetName() || Name == Class->GetPathName() || Name + TEXT("Component") == Class->GetName()) { return Class; } }
    return nullptr;
}
bool NativeLightClass(const UClass* C)
{return C==UPointLightComponent::StaticClass()||C==USpotLightComponent::StaticClass()||C==URectLightComponent::StaticClass()||C==UDirectionalLightComponent::StaticClass();}
struct FLightScope;FLightScope* CurrentLightScope=nullptr;bool LightReadAdmission();
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
    State->SetBoolField(TEXT("native_identity_verified"), Complete && Class == USplineComponent::StaticClass()
        && Class->GetPathName() == TEXT("/Script/Engine.SplineComponent") && Class->GetSuperClass() == UPrimitiveComponent::StaticClass()
        && Class->HasAnyClassFlags(CLASS_Native) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
        && !Class->ClassGeneratedBy && !Class->IsEditorOnly() && !Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && !Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly)
        && IsValid(CDO) && CDO->GetClass() == Class && !CDO->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects));
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
    const bool Hism=Class==UHierarchicalInstancedStaticMeshComponent::StaticClass();
    const auto* HC=Hism&&IsValid(CDO)&&!CDO->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)?Cast<UHierarchicalInstancedStaticMeshComponent>(CDO):nullptr;
    if(Hism){State->SetNumberField(TEXT("hism_cdo_instance_count"),HC?HC->GetInstanceCount():-1);State->SetBoolField(TEXT("hism_cdo_async_building"),HC&&HC->IsAsyncBuilding());State->SetBoolField(TEXT("hism_cdo_tree_fully_built"),HC&&HC->IsTreeFullyBuilt());}
    State->SetBoolField(TEXT("native_identity_verified"), Complete && (Hism||(Class == UInstancedStaticMeshComponent::StaticClass()))
        && Class->GetPathName() == (Hism?TEXT("/Script/Engine.HierarchicalInstancedStaticMeshComponent"):TEXT("/Script/Engine.InstancedStaticMeshComponent")) && Class->GetSuperClass() == (Hism?UInstancedStaticMeshComponent::StaticClass():UStaticMeshComponent::StaticClass())
        &&(!Hism||(HC&&HC->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)&&!HC->IsRegistered()&&!HC->IsPhysicsStateCreated()&&!HC->GetOwner()&&!HC->GetWorld()&&HC->GetInstanceCount()==0&&!HC->IsAsyncBuilding()))
        && Class->HasAnyClassFlags(CLASS_Native) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
        && !Class->ClassGeneratedBy && !Class->IsEditorOnly() && !Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && !Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly)
        && IsValid(CDO) && CDO->GetClass() == Class && !CDO->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects));
    return State;
}
TSharedRef<FJsonObject> NativeSkeletalSourceObservation(const UClass* Class)
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
    State->SetBoolField(TEXT("native_identity_verified"), Complete && Class == USkeletalMeshComponent::StaticClass()
        && Class->GetPathName() == TEXT("/Script/Engine.SkeletalMeshComponent") && Class->GetSuperClass() == USkinnedMeshComponent::StaticClass()
        && Class->HasAnyClassFlags(CLASS_Native) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
        && !Class->ClassGeneratedBy && !Class->IsEditorOnly() && !Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && !Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly)
        && IsValid(CDO) && CDO->GetClass() == Class && CDO->GetOuter() == Class->GetOuter() && CDO->HasAnyFlags(RF_ClassDefaultObject) && !CDO->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects));
    return State;
}
FProperty* FindProperty(UClass* Class, const FString& Name)
{
    for (TFieldIterator<FProperty> It(Class); It; ++It)
    { if (It->GetName().Equals(Name, ESearchCase::IgnoreCase)) { return *It; } }
    return nullptr;
}
bool NativeRuntimeParents(UBlueprint* Blueprint, FMutationResult& R)
{
    auto Check = InspectBlueprintParent(Blueprint->ParentClass->GetPathName());
    R.Data->SetObjectField(TEXT("parent_validation"), Check);
    if (!Check->GetBoolField(TEXT("success"))) { return Fail(R, TEXT("component_parent_unsupported"), TEXT("Blueprint parent chain failed editor/runtime checks.")); }
    for (const UClass* Class = Blueprint->ParentClass; Class; Class = Class->GetSuperClass())
    {
        const FString Package = Class->GetOutermost()->GetName();
        if (Class->HasAnyClassFlags(CLASS_Native) && Package != TEXT("/Script/Engine") && Package != TEXT("/Script/CoreUObject"))
        { return Fail(R, TEXT("component_parent_unsupported"), TEXT("Native parents outside Engine/CoreUObject are not audited in this slice.")); }
    }
    return true;
}
bool RejectExternalInstances(UBlueprint* Blueprint, FMutationResult& R)
{
    TArray<UObject*> Instances; TArray<UClass*> Derived; TArray<FString> External;
    GetObjectsOfClass(Blueprint->GeneratedClass, Instances, true, RF_NoFlags);
    GetDerivedClasses(Blueprint->GeneratedClass, Derived, true);
    for (UObject* Object : Instances) { if (IsValid(Object) && Object->GetOutermost() != Blueprint->GetOutermost()) { External.AddUnique(Object->GetPathName()); } }
    for (UClass* Class : Derived) { if (IsValid(Class) && Class->GetOutermost() != Blueprint->GetOutermost()) { External.AddUnique(Class->GetPathName()); } }
    External.Sort(); R.Data->SetArrayField(TEXT("external_instances_or_derived_classes"), Strings(External));
    return External.IsEmpty() || Fail(R, TEXT("component_external_instances_unsupported"), TEXT("Loaded external instances/derived classes could be changed by notifications or compilation; no mutation dispatched."));
}

enum class EShapeSize : uint8 { None, SphereRadius, BoxExtent, CapsuleRadius, CapsuleHalfHeight };
struct FPreparedProperty
{
    FProperty* Property = nullptr;
    void* Value = nullptr;
    TStrongObjectPtr<UObject> Reference;
    bool bSocket = false;
    EShapeSize ShapeSize = EShapeSize::None;
    TSharedPtr<FJsonValue> Requested, Before;
    FString Encoding;
    // TG owns only native scalar/math/FString scratch storage. Its cleanup must
    // not dereference a retired generated FProperty after an unknown callback.
    bool bPlainGeneratedValue=false,bPlainGeneratedString=false;
    ~FPreparedProperty() { if (Value) { if(bPlainGeneratedValue){if(bPlainGeneratedString)static_cast<FString*>(Value)->~FString();}else Property->DestroyValue(Value); FMemory::Free(Value); } }
};
// These sizes are protected native fields, but are explicitly EditAnywhere.
// Use their public setters, retaining the public property-access notification and
// inherited-value propagation rules rather than writing protected storage.
EShapeSize ShapeSizeProperty(UActorComponent* Template, const FProperty* Property)
{
    const FString Name = Property->GetName();
    if (Template->IsA<USphereComponent>() && Name == TEXT("SphereRadius")) { return EShapeSize::SphereRadius; }
    if (Template->IsA<UBoxComponent>() && Name == TEXT("BoxExtent")) { return EShapeSize::BoxExtent; }
    if (Template->IsA<UCapsuleComponent>() && Name == TEXT("CapsuleRadius")) { return EShapeSize::CapsuleRadius; }
    if (Template->IsA<UCapsuleComponent>() && Name == TEXT("CapsuleHalfHeight")) { return EShapeSize::CapsuleHalfHeight; }
    return EShapeSize::None;
}
bool ValidateShapeSequence(UActorComponent* Template, const TArray<const FPreparedProperty*>& Properties, FMutationResult& R)
{
    const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Template);
    float Radius = Capsule ? Capsule->GetUnscaledCapsuleRadius() : 0.f;
    float HalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 0.f;
    for (const FPreparedProperty* P : Properties)
    {
        if (P->ShapeSize == EShapeSize::None) { continue; }
        if (Template->IsRegistered() || Template->IsPhysicsStateCreated()) { return Fail(R, TEXT("component_shape_state_unsupported"), TEXT("Shape size edits require unregistered component templates/archetypes without a live physics state; overlap updates are not requested.")); }
        if (P->ShapeSize == EShapeSize::CapsuleRadius) { Radius = *static_cast<const float*>(P->Value); }
        if (P->ShapeSize == EShapeSize::CapsuleHalfHeight) { HalfHeight = *static_cast<const float*>(P->Value); }
        if (Capsule && (!FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || Radius < 0.f || HalfHeight < Radius))
        { return Fail(R, TEXT("component_shape_size_invalid"), TEXT("CapsuleRadius must not exceed CapsuleHalfHeight, including inherited local values; no companion size is silently clamped or increased.")); }
    }
    return true;
}
bool NumericValue(FNumericProperty* Property, double Number, void* Address)
{
    if (!FMath::IsFinite(Number)) { return false; }
    if (Property->IsInteger())
    {
        if (FMath::Abs(Number) > 9007199254740991.0 || FMath::TruncToDouble(Number) != Number) { return false; }
        const int32 Bits = Property->GetElementSize() * 8;
        const bool bUnsigned = CastField<FByteProperty>(Property) || CastField<FUInt16Property>(Property) || CastField<FUInt32Property>(Property) || CastField<FUInt64Property>(Property);
        const double Lower = bUnsigned ? 0.0 : -FMath::Pow(2.0, Bits - 1);
        const double UpperExclusive = FMath::Pow(2.0, bUnsigned ? Bits : Bits - 1);
        if (Number < Lower || Number >= UpperExclusive) { return false; }
        Property->SetIntPropertyValue(Address, static_cast<uint64>(static_cast<int64>(Number))); return true;
    }
    if (CastField<FFloatProperty>(Property) && FMath::Abs(Number) > static_cast<double>(MAX_flt)) { return false; }
    Property->SetFloatingPointPropertyValue(Address, Number); return true;
}
bool EnumValue(UEnum* Enum, const TSharedPtr<FJsonValue>& Json, int64& Out)
{
    if (!IsValid(Enum)) { return false; }
    for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
    {
        const FString Name = Enum->GetNameStringByIndex(Index);
        if (Name.EndsWith(TEXT("_MAX"))) { continue; }
        if ((Json->Type == EJson::String && (Json->AsString() == Name || Json->AsString() == Enum->GetNameByIndex(Index).ToString()))
            || (Json->Type == EJson::Number && Json->AsNumber() == static_cast<double>(Enum->GetValueByIndex(Index))))
        { Out = Enum->GetValueByIndex(Index); return true; }
    }
    return false;
}
bool VectorValues(const TSharedPtr<FJsonValue>& Json, int32 Size, double* Out, bool bBroadcast = false)
{
    if (bBroadcast && Json->Type == EJson::Number)
    { for (int32 I = 0; I < Size; ++I) { Out[I] = Json->AsNumber(); } }
    else
    {
        if (Json->Type != EJson::Array || Json->AsArray().Num() != Size) { return false; }
        for (int32 I = 0; I < Size; ++I) { if (Json->AsArray()[I]->Type != EJson::Number) { return false; } Out[I] = Json->AsArray()[I]->AsNumber(); }
    }
    for (int32 I = 0; I < Size; ++I) { if (!FMath::IsFinite(Out[I]) || FMath::Abs(Out[I]) > 1.e12) { return false; } }
    return true;
}
bool AuditRuntimeReference(UObject* Asset, const FString& Kind, FMutationResult& R)
{
    auto Audit = ReadPackageDependencies({Asset->GetOutermost()->GetName()}, true, 4096);
    R.Data->SetObjectField(Kind == TEXT("mesh") ? TEXT("static_mesh_dependency_audit") : TEXT("material_dependency_audit"), Audit);
    bool bAuditPassed = false;
    if (!Audit->TryGetBoolField(TEXT("passed"), bAuditPassed) || !bAuditPassed)
    { return Fail(R, *(TEXT("component_") + Kind + TEXT("_dependency_unverified")), TEXT("Saved asset dependency graph is incomplete, dirty or contains tool dependencies.")); }
    // Engine Cube owns NavCollision: Runtime/NavigationSystem/Public/NavCollision.h.
    // NavigationSystem.Build.cs gates its UnrealEd dependencies on bBuildEditor.
    const TSet<FString> NativePackages = {TEXT("/Script/Engine"), TEXT("/Script/CoreUObject"), TEXT("/Script/PhysicsCore"), TEXT("/Script/MeshDescription"), TEXT("/Script/StaticMeshDescription"), TEXT("/Script/NavigationSystem")};
    for (const auto& Edge : Audit->GetArrayField(TEXT("edges")))
    {
        const FString Package = Edge->AsObject()->GetStringField(TEXT("to"));
        if (!Package.StartsWith(TEXT("/Game/")) && !Package.StartsWith(TEXT("/Engine/")) && !NativePackages.Contains(Package))
        { return Fail(R, *(TEXT("component_") + Kind + TEXT("_dependency_unsupported")), TEXT("Asset references an unaudited module or plugin mount: ") + Package); }
    }
    return true;
}
bool LoadStaticMesh(const TSharedPtr<FJsonValue>& Json, FPreparedProperty& Prepared, FMutationResult& R)
{
    if (Json->Type == EJson::Null) { return true; }
    if (Json->Type != EJson::String) { return Fail(R, TEXT("component_mesh_reference_invalid"), TEXT("StaticMesh requires a quoted asset path or null.")); }
    FString Path = Json->AsString();
    if (!SafeName(Path) || (!Path.StartsWith(TEXT("/Game/")) && !Path.StartsWith(TEXT("/Engine/"))))
    { return Fail(R, TEXT("component_mesh_reference_invalid"), TEXT("Only /Game or /Engine StaticMesh references are supported.")); }
    if (!Path.Contains(TEXT("."))) { Path += TEXT(".") + FPackageName::GetLongPackageAssetName(Path); }
    if (!SafeName(Path) || !FPackageName::IsValidObjectPath(Path)) { return Fail(R, TEXT("component_mesh_reference_invalid"), TEXT("StaticMesh asset path is invalid.")); }
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() || Mesh->IsEditorOnly() || Mesh->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
    { return Fail(R, TEXT("component_mesh_reference_invalid"), TEXT("Reference must resolve to an exact runtime UStaticMesh.")); }
    if (!AuditRuntimeReference(Mesh, TEXT("mesh"), R)) { return false; }
    Prepared.Reference.Reset(Mesh); return true;
}
bool LoadSlotMaterial(FString Path, FPreparedProperty& Prepared, FMutationResult& R)
{
    if (!SafeName(Path) || (!Path.StartsWith(TEXT("/Game/")) && !Path.StartsWith(TEXT("/Engine/"))))
    { return Fail(R, TEXT("component_material_reference_invalid"), TEXT("Material requires a /Game or /Engine asset path.")); }
    if (!Path.Contains(TEXT("."))) { Path += TEXT(".") + FPackageName::GetLongPackageAssetName(Path); }
    if (!SafeName(Path) || !FPackageName::IsValidObjectPath(Path)) { return Fail(R, TEXT("component_material_reference_invalid"), TEXT("Material asset path is invalid.")); }
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Path);
    if (!Material || (Material->GetClass() != UMaterial::StaticClass() && Material->GetClass() != UMaterialInstanceConstant::StaticClass()) || !Material->IsAsset()
        || Material->IsEditorOnly() || Material->HasAnyFlags(RF_Transient) || Material->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
    { return Fail(R, TEXT("component_material_reference_invalid"), TEXT("Material must resolve to an exact saved runtime UMaterial or UMaterialInstanceConstant asset.")); }
    if (!AuditRuntimeReference(Material, TEXT("material"), R)) { return false; }
    Prepared.Reference.Reset(Material); return true;
}
bool PreserveMaterialSlots(UStaticMeshComponent* Component, UStaticMesh* FinalMesh,
    int32 ExpectedOverrideCount, bool bMeshChanges, bool bMaterialChanges, FMutationResult& R)
{
    // Public PostEdit notifications clean up override/overlay tails. Reject the
    // destructive layout before dispatch; never restore arrays by direct writes.
    if (!bMeshChanges && !bMaterialChanges) { return true; }
    const int32 FinalSlots = FinalMesh ? FinalMesh->GetStaticMaterials().Num() : 0;
    if (ExpectedOverrideCount > FinalSlots || (bMeshChanges && Component->GetComponentMaterialSlotsOverlayMaterial().Num() > FinalSlots))
    {
        R.Data->SetStringField(TEXT("unsupported_material_slots_component"), Component->GetPathName());
        R.Data->SetNumberField(TEXT("final_mesh_slot_count"), FinalSlots);
        R.Data->SetNumberField(TEXT("expected_override_count"), ExpectedOverrideCount);
        R.Data->SetNumberField(TEXT("overlay_slot_count"), Component->GetComponentMaterialSlotsOverlayMaterial().Num());
        return Fail(R, TEXT("component_material_slots_unsupported"), TEXT("Editor notifications would trim existing override/overlay slots or the requested slot 0. Choose a mesh with enough material slots; inspect mutation_attempted for overall progress."));
    }
    return true;
}
bool PrepareLightReference(FObjectPropertyBase* Property,const TSharedPtr<FJsonValue>& Json,FPreparedProperty& Prepared);
bool PrepareProperty(UActorComponent* Template, const FString& Name, const TSharedPtr<FJsonValue>& Json,
    TUniquePtr<FPreparedProperty>& Out, FMutationResult& R)
{
    if (!SafeName(Name)) { return Fail(R, TEXT("component_property_name_invalid"), TEXT("Property name must be nonempty and shorter than NAME_SIZE.")); }
    FProperty* Property = FindProperty(Template->GetClass(), Name);
    if (!Property || Property->ArrayDim != 1 || Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
    { return Fail(R, TEXT("component_property_unsupported"), TEXT("Property is missing, transient, deprecated or a fixed array.")); }
    const bool bSocket = Property->GetName() == TEXT("AttachSocketName") && Template->IsA<USceneComponent>();
    if (bSocket)
    {
        const USceneComponent* Scene = CastChecked<USceneComponent>(Template);
        const USceneComponent* Parent = Scene->GetAttachParent();
        if (Scene->IsRegistered() || Parent == Scene || (Parent && (Parent->IsAttachedTo(Scene) || Parent->GetAttachChildren().Contains(Scene))))
        { return Fail(R, TEXT("component_socket_state_unsupported"), TEXT("Socket assignment requires an unregistered template that has not performed AttachTo; SetupAttachment preconditions failed.")); }
    }
    const bool Allowed=bSocket||(PropertyAccessUtil::CanSetPropertyValue(Property, PropertyAccessUtil::EditorReadOnlyFlags, true)==EPropertyAccessResultFlags::Success&&Template->CanEditChange(Property));
    if(!LightReadAdmission()){return false;}if (!Allowed)
    { return Fail(R, TEXT("component_property_readonly"), TEXT("Property is not editable on this component template.")); }
    auto Prepared = MakeUnique<FPreparedProperty>(); Prepared->Property = Property; Prepared->Requested = Json; Prepared->bSocket = bSocket;
    Prepared->Value = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment()); Property->InitializeValue(Prepared->Value);
    void* Value = Prepared->Value;
    bool bConverted = false;
    if (FBoolProperty* Bool = CastField<FBoolProperty>(Property); Bool && Json->Type == EJson::Boolean)
    { Bool->SetPropertyValue(Value, Json->AsBool()); bConverted = true; }
    else if (FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    { int64 V; if (Enum->GetUnderlyingProperty() && EnumValue(Enum->GetEnum(), Json, V)) { bConverted = NumericValue(Enum->GetUnderlyingProperty(), static_cast<double>(V), Value); } }
    else if (FByteProperty* Byte = CastField<FByteProperty>(Property); Byte && Byte->Enum)
    { int64 V; if (EnumValue(Byte->Enum, Json, V)) { bConverted = NumericValue(Byte, static_cast<double>(V), Value); } }
    else if (FNumericProperty* Number = CastField<FNumericProperty>(Property); Number && Json->Type == EJson::Number)
    { bConverted = NumericValue(Number, Json->AsNumber(), Value); }
    else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property); NameProperty && Json->Type == EJson::String && SafeName(Json->AsString(), true))
    { NameProperty->SetPropertyValue(Value, FName(*Json->AsString())); bConverted = true; }
    else if (FStrProperty* String = CastField<FStrProperty>(Property); String && Json->Type == EJson::String)
    { String->SetPropertyValue(Value, Json->AsString()); bConverted = true; }
    else if (FTextProperty* Text = CastField<FTextProperty>(Property); Text && Json->Type == EJson::String)
    { Text->SetPropertyValue(Value, FText::FromString(Json->AsString())); bConverted = true; }
    else if (FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        double V[4];
        if (Struct->Struct == TBaseStructure<FVector>::Get() && VectorValues(Json, 3, V, true)) { *static_cast<FVector*>(Value) = FVector(V[0], V[1], V[2]); bConverted = true; }
        else if (Struct->Struct == TBaseStructure<FRotator>::Get() && VectorValues(Json, 3, V)) { *static_cast<FRotator*>(Value) = FRotator(V[0], V[1], V[2]); bConverted = true; }
        else if (Struct->Struct == TBaseStructure<FLinearColor>::Get() && VectorValues(Json, 4, V)) { *static_cast<FLinearColor*>(Value) = FLinearColor(V[0], V[1], V[2], V[3]); bConverted = true; }
    }
    else if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property); Object && Template->GetClass() == UStaticMeshComponent::StaticClass() && Property->GetName() == TEXT("StaticMesh") && Object->PropertyClass == UStaticMesh::StaticClass())
    {
        if (!LoadStaticMesh(Json, *Prepared, R)) { return false; }
        auto* Component = CastChecked<UStaticMeshComponent>(Template); auto* Mesh = Cast<UStaticMesh>(Prepared->Reference.Get());
        if (!PreserveMaterialSlots(Component, Mesh, Component->OverrideMaterials.Num(), Component->GetStaticMesh() != Mesh, false, R)) { return false; }
        Object->SetObjectPropertyValue(Value, Prepared->Reference.Get()); bConverted = true;
    }
    else if(FObjectPropertyBase* LightObject=CastField<FObjectPropertyBase>(Property);LightObject&&NativeLightClass(Template->GetClass())
        &&((Property->GetFName()==TEXT("LightFunctionMaterial")&&LightObject->PropertyClass==UMaterialInterface::StaticClass())||(Property->GetFName()==TEXT("IESTexture")&&LightObject->PropertyClass==UTextureLightProfile::StaticClass())))
    {if(!PrepareLightReference(LightObject,Json,*Prepared))return false;LightObject->SetObjectPropertyValue(Value,Prepared->Reference.Get());bConverted=true;}
    if (!bConverted) { return Fail(R, TEXT("component_property_value_unsupported"), TEXT("Value/type is unsupported or out of range; no generic ImportText/object setter is used.")); }
    Prepared->ShapeSize = ShapeSizeProperty(Template, Property);
    if (Prepared->ShapeSize != EShapeSize::None)
    {
        if (Prepared->ShapeSize == EShapeSize::BoxExtent)
        {
            const FVector& Extent = *static_cast<const FVector*>(Value);
            if (Extent.X < 0.0 || Extent.Y < 0.0 || Extent.Z < 0.0) { return Fail(R, TEXT("component_shape_size_invalid"), TEXT("BoxExtent components must be finite values in [0, 1e12].")); }
        }
        else if (Json->Type != EJson::Number || Json->AsNumber() < 0.0 || Json->AsNumber() > 1.e12)
        { return Fail(R, TEXT("component_shape_size_invalid"), TEXT("Shape radius/half-height must be a finite number in [0, 1e12].")); }
    }
    FString Error;
    if (!ReadComponentValue(Property, Property->ContainerPtrToValuePtr<void>(Template), MaxJsonChars, Prepared->Before, Prepared->Encoding, Error))
    { return Fail(R, TEXT("component_property_before_unreadable"), Error); }
    Out = MoveTemp(Prepared); return true;
}
bool CheckPropagation(UActorComponent* Template, const TArray<TUniquePtr<FPreparedProperty>>& Properties, UPackage* Package, FMutationResult& R)
{
    TArray<FString> External, Local;
    TMap<UActorComponent*, TArray<const FPreparedProperty*>> ShapeTargets;
    for (const auto& Prepared : Properties)
    {
        if (Prepared->ShapeSize != EShapeSize::None) { ShapeTargets.FindOrAdd(Template).Add(Prepared.Get()); }
        TArray<UObject*> Instances;
        PropertyAccessUtil::GetArchetypeInstancesInheritingPropertyValue(Prepared->Property, Template, Instances);
        for (UObject* Object : Instances)
        {
            if (IsValid(Object))
            {
                (Object->GetOutermost() == Package ? Local : External).AddUnique(Object->GetPathName());
                if (Prepared->Property->GetName() == TEXT("StaticMesh") && Object->GetOutermost() == Package)
                {
                    auto* Component = Cast<UStaticMeshComponent>(Object); auto* Mesh = Cast<UStaticMesh>(Prepared->Reference.Get());
                    if (!Component || !PreserveMaterialSlots(Component, Mesh, Component->OverrideMaterials.Num(), Component->GetStaticMesh() != Mesh, false, R)) { return false; }
                }
                if (Prepared->ShapeSize != EShapeSize::None && Object->GetOutermost() == Package)
                {
                    UActorComponent* Component = Cast<UActorComponent>(Object);
                    if (!Component || Component->GetClass() != Template->GetClass()) { return Fail(R, TEXT("component_shape_state_unsupported"), TEXT("Shape archetype class differs from the exact supported native class.")); }
                    ShapeTargets.FindOrAdd(Component).Add(Prepared.Get());
                }
            }
        }
    }
    External.Sort(); Local.Sort();
    R.Data->SetArrayField(TEXT("external_property_archetypes"), Strings(External));
    R.Data->SetArrayField(TEXT("local_property_archetypes"), Strings(Local));
    if (!External.IsEmpty()) { return Fail(R, TEXT("component_external_archetypes_unsupported"), TEXT("External property archetypes are unsupported; inspect mutation_attempted and creation_observed for actual progress.")); }
    for (const auto& Pair : ShapeTargets) { if (!ValidateShapeSequence(Pair.Key, Pair.Value, R)) { return false; } }
    return true;
}
bool ApplyShapeSize(UActorComponent* Template, const FPreparedProperty& Prepared)
{
    const bool bIdentical = Prepared.Property->Identical(Prepared.Value, Prepared.Property->ContainerPtrToValuePtr<void>(Template));
    if (bIdentical) { return true; }
    TArray<UObject*> Instances;
    PropertyAccessUtil::GetArchetypeInstancesInheritingPropertyValue(Prepared.Property, Template, Instances);
    TArray<UActorComponent*> Targets{Template};
    for (UObject* Object : Instances)
    {
        UActorComponent* Component = Cast<UActorComponent>(Object);
        if (!IsValid(Component) || Component->GetClass() != Template->GetClass() || Component->GetOutermost() != Template->GetOutermost()) { return false; }
        Targets.AddUnique(Component);
    }
    // Recheck immediately before dispatch: preceding property notifications may
    // have changed the local archetype graph or the capsule companion dimension.
    FMutationResult Guard;
    for (UActorComponent* Target : Targets)
    { if (!ValidateShapeSequence(Target, {&Prepared}, Guard)) { return false; } }
    auto Notify = PropertyAccessUtil::BuildBasicChangeNotify(Prepared.Property, Template, EPropertyAccessChangeNotifyMode::Default, EPropertyChangeType::ValueSet);
    PropertyAccessUtil::EmitPreChangeNotify(Notify.Get(), bIdentical);
    for (UActorComponent* Target : Targets)
    {
        Target->Modify();
        switch (Prepared.ShapeSize)
        {
        case EShapeSize::SphereRadius: CastChecked<USphereComponent>(Target)->SetSphereRadius(*static_cast<const float*>(Prepared.Value), false); break;
        case EShapeSize::BoxExtent: CastChecked<UBoxComponent>(Target)->SetBoxExtent(*static_cast<const FVector*>(Prepared.Value), false); break;
        case EShapeSize::CapsuleRadius:
        { auto* Capsule = CastChecked<UCapsuleComponent>(Target); Capsule->SetCapsuleSize(*static_cast<const float*>(Prepared.Value), Capsule->GetUnscaledCapsuleHalfHeight(), false); break; }
        case EShapeSize::CapsuleHalfHeight:
        { auto* Capsule = CastChecked<UCapsuleComponent>(Target); Capsule->SetCapsuleSize(Capsule->GetUnscaledCapsuleRadius(), *static_cast<const float*>(Prepared.Value), false); break; }
        default: return false;
        }
    }
    PropertyAccessUtil::EmitPostChangeNotify(Notify.Get(), bIdentical);
    return true;
}
bool ApplyProperty(UActorComponent* Template, USCS_Node* Node, const FPreparedProperty& Prepared)
{
    if (Prepared.ShapeSize != EShapeSize::None) { return ApplyShapeSize(Template, Prepared); }
    if (Prepared.bSocket)
    {
        USceneComponent* Scene = Cast<USceneComponent>(Template);
        if (!Scene) { return false; }
        const FName Value = CastFieldChecked<FNameProperty>(Prepared.Property)->GetPropertyValue(Prepared.Value);
        Scene->PreEditChange(Prepared.Property);
        // SetAttachSocketName is private in UE58. Public SetupAttachment is
        // valid for this unregistered, not-yet-AttachTo template (preflight above).
        Scene->SetupAttachment(Scene->GetAttachParent(), Value);
        if (Scene->GetAttachSocketName() != Value) { return false; }
        Node->AttachToName = Value;
        FPropertyChangedEvent Event(Prepared.Property, EPropertyChangeType::ValueSet); Scene->PostEditChangeProperty(Event);
        return true;
    }
    return UKismetSystemLibrary::Generic_SetEditorProperty(Template, Prepared.Property->GetFName(), Prepared.Value, Prepared.Property, EPropertyAccessChangeNotifyMode::Default);
}

// JG/JH/JR: source/default subtree and current own SCS roles. SDK internal
// callbacks are not claimed atomic; the explicit return boundaries are fenced.
struct FLightScope
{
    FMutationResult& R;FLightScope* Previous;bool Active=false,Stopped=false,Attached=false;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    struct FIdentity{TStrongObjectPtr<UObject> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;};
    struct FNode{TStrongObjectPtr<USCS_Node> Hold;TStrongObjectPtr<UActorComponent> Template;UClass* Class=nullptr;FGuid Guid,OriginalLight,Light;FString Name,Fields;TArray<USCS_Node*> Children;};
    TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<USimpleConstructionScript> SCS;TStrongObjectPtr<UClass> Generated,Skeleton,NativeClass;
    TStrongObjectPtr<UActorComponent> Default;TArray<FIdentity> Source,Stable,CurrentObjects,SelectedChildren;TArray<UObject*> SourceChildren;
    TArray<FNode> Nodes;TArray<USCS_Node*> Roots;TStrongObjectPtr<USCS_Node> Selected;TStrongObjectPtr<ULightComponentBase> Light;
    // JH Set-only: loaded sources remain held/proved even after a selected ref is cleared.
    struct FAssetRole{TStrongObjectPtr<UObject> Asset;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UObject> CDO;TArray<FIdentity> Ids;TArray<UObject*> Nested;TArray<FString> Authored;FString File,SHA;int64 Bytes=-1;EClassFlags ClassFlags=CLASS_None;UClass* Super=nullptr;uint32 PkgFlags=0;};
    TArray<FAssetRole> Assets;bool ReferenceMode=false,ReferenceReady=false;FName ReferenceName;
    UObject* ExpectedFunction=nullptr;UObject* ExpectedIES=nullptr;UObject* ExpectedStash=nullptr;
    UObject* PlannedFunction=nullptr;UObject* PlannedIES=nullptr;UObject* PlannedStash=nullptr;
    // JR only: protect selected Directional authored fields outside Intensity and
    // the declared Add transforms. Other light field/reference scopes stay unchanged.
    bool DirectionalWritesFixed=false,DirectionalReady=false;TSet<FName> DirectionalWrites;FString DirectionalFields;
    FString ReferenceOtherFields,TargetFile,TargetSHA;int64 TargetBytes=-1;TArray<FString> ReferenceOtherDirty;int32 ReferenceLoadCalls=0;
    FString BPPath,Package,GeneratedPath,SkeletonPath,SelectedName,SourceFields,SelectedNodePath,SelectedTemplatePath;UClass* ParentClass=nullptr;UClass* SourceSuper=nullptr;UClass* SkeletonSuper=nullptr;EClassFlags GeneratedFlags=CLASS_None,SkeletonFlags=CLASS_None;EObjectFlags SelectedNodeFlags=RF_NoFlags,SelectedTemplateFlags=RF_NoFlags;FGuid SelectedNodeGuid;EClassFlags ClassFlags=CLASS_None;uint32 PackageFlags=0;FGuid SelectedOriginal,SelectedLight;
    explicit FLightScope(FMutationResult& In):R(In),Previous(CurrentLightScope){CurrentLightScope=this;}
    ~FLightScope(){CurrentLightScope=Previous;}
    bool Stop(const TCHAR* Code,const TCHAR* Why)
    {if(!Stopped){Fail(R,Code,Why);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("code"),Code);J->SetStringField(TEXT("stage"),R.Stage);R.Data->SetObjectField(TEXT("light_first_failure"),J);}Stopped=true;R.bChangeStateKnown=false;R.bPostconditionPassed=false;return false;}
    bool Context()
    {if(!Active)return true;if(Stopped)return false;if(!IsEditorGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()||!GEditor||GEditor!=Editor.Get()||!IsValid(World.Get())||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get())return Stop(TEXT("component_light_context_changed"),TEXT("Captured idle Editor/world changed; no next getter/writer/retry."));return true;}
    bool Loaded(const UObject* O)const{return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Pin(UObject* O,TArray<FIdentity>& Out)
    {if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter())))return Stop(TEXT("component_light_identity_unreadable"),TEXT("Current retained role must be readable."));FIdentity I;I.Hold.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();Out.Add(MoveTemp(I));return true;}
    bool Exact(const FIdentity& I)
    {auto* O=I.Hold.Get();return Loaded(O)&&Loaded(I.Class)&&Loaded(I.Package)&&(!I.Outer||Loaded(I.Outer))&&O->GetClass()==I.Class&&O->GetOuter()==I.Outer&&O->GetPackage()==I.Package&&O->GetFlags()==I.Flags&&O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive);}
    bool Anchors()
    {
        if(!Context())return false;
        for(const auto& I:Stable)if(!Exact(I))return Stop(TEXT("component_light_authoring_identity_changed"),TEXT("Immutable Blueprint/SCS/existing node identity changed."));
        for(const auto& I:Source)if(!Exact(I))return Stop(TEXT("component_light_source_identity_changed"),TEXT("Readonly class/CDO/default-subobject identity changed."));
        return true;
    }
    bool ClassRole(UClass* K,bool IsSkeleton)
    {
        return Loaded(K)&&K->GetClass()==UBlueprintGeneratedClass::StaticClass()&&K->GetOuter()==BP->GetPackage()&&K->GetPackage()==BP->GetPackage()&&K->ClassGeneratedBy==BP.Get()
            &&K->GetSuperClass()==(IsSkeleton?SkeletonSuper:ParentClass)&&K->GetPathName().Equals(IsSkeleton?SkeletonPath:GeneratedPath,ESearchCase::CaseSensitive)
            &&!K->HasAnyClassFlags(CLASS_Native|CLASS_Deprecated|CLASS_NewerVersionExists);
    }
    bool CaptureCurrent()
    {
        if(!ClassRole(Generated.Get(),false)||!ClassRole(Skeleton.Get(),true))return Stop(TEXT("component_light_current_class_invalid"),TEXT("Exact same-Blueprint generated/skeleton class ownership, path and parent required."));
        CurrentObjects.Reset();if(!Pin(Generated.Get(),CurrentObjects)||!Pin(Skeleton.Get(),CurrentObjects))return false;
        GeneratedFlags=Generated->GetClassFlags();SkeletonFlags=Skeleton->GetClassFlags();
        for(const auto& N:Nodes)if(!Pin(N.Template.Get(),CurrentObjects))return false;
        return true;
    }
    bool Raw()
    {
        if(!Active)return true;if(!Anchors())return false;
        for(const auto& I:CurrentObjects)if(!Exact(I))return Stop(TEXT("component_light_current_identity_changed"),TEXT("Current generated/skeleton/template identity changed outside explicit compiler return."));
        for(const auto& A:Assets)
        {for(const auto& I:A.Ids)if(!Exact(I))return Stop(TEXT("component_light_reference_identity_changed"),TEXT("Retained source identity changed."));if(A.Class->GetClassFlags()!=A.ClassFlags||A.Class->GetSuperClass()!=A.Super||A.Class->GetDefaultObject(false)!=A.CDO.Get()||!A.CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||A.Asset->GetPackage()->GetPackageFlags()!=A.PkgFlags||A.Asset->GetPackage()->IsDirty())return Stop(TEXT("component_light_reference_source_changed"),TEXT("Readonly source class/CDO/package role changed."));}
        if(ReferenceReady)
        {auto* L=Cast<ULightComponent>(Light.Get());if(!Loaded(L)||L->LightFunctionMaterial.Get()!=ExpectedFunction||L->IESTexture.Get()!=ExpectedIES||L->StashedLightFunctionMaterial.Get()!=ExpectedStash)return Stop(TEXT("component_light_reference_role_changed"),TEXT("Selected reference/stash differs from the exact planned SDK return."));}
        if(!Loaded(BP.Get())||!Loaded(SCS.Get())||!Loaded(Generated.Get())||!Loaded(Skeleton.Get())||!BP->GetPathName().Equals(BPPath,ESearchCase::CaseSensitive)||!BP->GetPackage()->GetName().Equals(Package,ESearchCase::CaseSensitive)||BP->ParentClass!=ParentClass||BP->SimpleConstructionScript!=SCS.Get()||BP->GeneratedClass!=Generated.Get()||BP->SkeletonGeneratedClass!=Skeleton.Get()
            ||!ClassRole(Generated.Get(),false)||!ClassRole(Skeleton.Get(),true)||Generated->GetClassFlags()!=GeneratedFlags||Skeleton->GetClassFlags()!=SkeletonFlags
            ||NativeClass->GetSuperClass()!=SourceSuper||NativeClass->GetClassFlags()!=ClassFlags||NativeClass->GetDefaultObject(false)!=Default.Get()||NativeClass->GetPackage()->GetPackageFlags()!=PackageFlags||NativeClass->GetPackage()->IsDirty())
            return Stop(TEXT("component_light_current_roles_changed"),TEXT("Current child/SCS/generated class or native source role changed."));
        const auto& Current=SCS->GetAllNodes();const bool ExistingSelected=Nodes.ContainsByPredicate([&](const FNode& N){return N.Hold.Get()==Selected.Get();});
        if(Current.Num()!=Nodes.Num()+(Selected.IsValid()&&Attached&&!ExistingSelected?1:0)||SCS->GetRootNodes()!=Roots)
            return Stop(TEXT("component_light_node_membership_changed"),TEXT("Current own node/root membership differs from the exact planned add."));
        for(const auto& N:Nodes)
        {
            auto* P=N.Hold.Get();if(!Loaded(P)||!Current.Contains(P)||P->GetOuter()!=SCS.Get()||!P->GetVariableName().ToString().Equals(N.Name,ESearchCase::CaseSensitive)||P->VariableGuid!=N.Guid||P->ComponentClass!=N.Class||P->ComponentTemplate!=N.Template.Get()||!Loaded(N.Template.Get())||N.Template->GetClass()!=N.Class||N.Template->GetOuter()!=Generated.Get()||P->GetChildNodes()!=N.Children)
                return Stop(TEXT("component_light_node_role_changed"),TEXT("Existing own node/template/class/GUID/attachment role changed."));
            if(P!=Selected.Get())if(auto* L=Cast<ULightComponentBase>(N.Template.Get());L&&(L->OriginalLightGuid!=N.OriginalLight||L->LightGuid!=N.Light))
                return Stop(TEXT("component_light_unselected_guid_changed"),TEXT("An unselected light GUID changed."));
        }
        if(Selected.IsValid())
        {
            if(!Loaded(Selected.Get())||!Loaded(Light.Get())||Selected->GetOuter()!=SCS.Get()||Selected->ComponentTemplate!=Light.Get()||Selected->ComponentClass!=NativeClass.Get()||Light->GetClass()!=NativeClass.Get()||Light->GetOuter()!=Generated.Get()||Light->GetPackage()!=BP->GetPackage()||!Selected->GetVariableName().ToString().Equals(SelectedName,ESearchCase::CaseSensitive)
                ||Selected->VariableGuid!=SelectedNodeGuid||!Selected->GetPathName().Equals(SelectedNodePath,ESearchCase::CaseSensitive)||!Light->GetPathName().Equals(SelectedTemplatePath,ESearchCase::CaseSensitive)||Selected->GetFlags()!=SelectedNodeFlags||Light->GetFlags()!=SelectedTemplateFlags||Light->IsRegistered()||Light->IsPreRegistering()||Light->GetOwner()||Light->GetWorld())return Stop(TEXT("component_light_selected_role_changed"),TEXT("Exact current own unregistered light template required."));
            for(const auto& I:SelectedChildren)if(!Exact(I))return Stop(TEXT("component_light_selected_subobject_changed"),TEXT("Selected default-subobject identity changed."));
        }return true;
    }
    bool ArmDirectional(const TSharedRef<FJsonObject>& Requested,bool ForAdd)
    {
        DirectionalWrites.Reset();
        for(const auto& Pair:Requested->Values)
        {
            const FName Name(*FString(Pair.Key.ToView()));
            if(Name!=TEXT("Intensity")&&!(ForAdd&&(Name==TEXT("RelativeLocation")||Name==TEXT("RelativeRotation")||Name==TEXT("RelativeScale3D"))))
            {return Fail(R,TEXT("component_directional_property_pending"),TEXT("Directional authoring supports Set Intensity and Add explicit transforms/optional Intensity only; other initial/inherited/shadow/reference writes remain pending."));}
            DirectionalWrites.Add(Name);
        }
        DirectionalWritesFixed=true;return true;
    }
    bool Fields(UObject* O,FString& Out,bool AuthoredOnly=false,bool ReferenceOthers=false,bool DirectionalOthers=false)
    {
        if(!Raw())return false;auto J=MakeShared<FJsonObject>();int32 Count=0,Chars=0;
        for(TFieldIterator<FProperty> It(O->GetClass());It;++It)
        {FProperty* P=*It;if(AuthoredOnly&&P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient))continue;
            if(DirectionalOthers&&(!P->HasAnyPropertyFlags(CPF_Edit)||DirectionalWrites.Contains(P->GetFName())||P->GetFName()==TEXT("OriginalLightGuid")||P->GetFName()==TEXT("LightGuid")))continue;
            if(ReferenceOthers&&(P->GetFName()==TEXT("LightFunctionMaterial")||P->GetFName()==TEXT("IESTexture")||P->GetFName()==TEXT("StashedLightFunctionMaterial")||P->GetFName()==TEXT("OriginalLightGuid")||P->GetFName()==TEXT("LightGuid")))continue;
            if(++Count>1024||P->ArrayDim<1||P->ArrayDim>16)return Stop(TEXT("component_light_field_bound"),TEXT("Bounded field observation exceeded."));
            TArray<TSharedPtr<FJsonValue>> Values;for(int32 I=0;I<P->ArrayDim;++I){FString V;const bool OK=P->ExportText_Direct(V,P->ContainerPtrToValuePtr<void>(O,I),P->ContainerPtrToValuePtr<void>(O,I),O,PPF_None);if(!Raw())return false;if(!OK||(Chars+=V.Len())>1048576)return Stop(TEXT("component_light_fields_incomplete"),TEXT("Current reflected fields are incomplete."));Values.Add(MakeShared<FJsonValueString>(V));}J->SetArrayField(P->GetName(),Values);}
        Out.Reset();FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return Raw();
    }
    bool DirectionalCheck()
    {
        if(!DirectionalReady)return true;if(!Raw())return false;FString Now;if(!Fields(Light.Get(),Now,true,false,true))return false;
        if(Now.Equals(DirectionalFields,ESearchCase::CaseSensitive)){R.Data->SetBoolField(TEXT("directional_nonselected_authored_fields_verified"),true);return Raw();}
        // Diagnostics/comparison use cached JSON only. Map-key order is not
        // authored state; each property array and its export strings stay exact.
        TSharedPtr<FJsonObject> Expected,Actual;auto Difference=MakeShared<FJsonObject>();Difference->SetStringField(TEXT("object"),SelectedTemplatePath);Difference->SetStringField(TEXT("stage"),R.Stage);
        if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DirectionalFields),Expected)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Now),Actual)||!Expected.IsValid()||!Actual.IsValid())
        {return Stop(TEXT("component_directional_observation_incomplete"),TEXT("Cached Directional authored field witness is incomplete; no next SDK or retry."));}
        bool Found=false;
        for(const auto& Pair:Expected->Values)
        {
            const auto Value=Actual->TryGetField(Pair.Key.ToView());FString BeforeValue,AfterValue;
            const bool BeforeOK=FJsonSerializer::Serialize(Pair.Value,TEXT(""),TJsonWriterFactory<>::Create(&BeforeValue));
            const bool AfterOK=Value.IsValid()&&FJsonSerializer::Serialize(Value,TEXT(""),TJsonWriterFactory<>::Create(&AfterValue));
            if(!BeforeOK||!AfterOK||!BeforeValue.Equals(AfterValue,ESearchCase::CaseSensitive))
            {Difference->SetStringField(TEXT("property"),FString(Pair.Key.ToView()));Difference->SetField(TEXT("expected"),Pair.Value);if(Value.IsValid())Difference->SetField(TEXT("actual"),Value);else Difference->SetField(TEXT("actual"),MakeShared<FJsonValueNull>());Found=true;break;}
        }
        if(!Found){for(const auto& Pair:Actual->Values)if(!Expected->HasField(Pair.Key.ToView())){Difference->SetStringField(TEXT("property"),FString(Pair.Key.ToView()));Difference->SetField(TEXT("expected"),MakeShared<FJsonValueNull>());Difference->SetField(TEXT("actual"),Pair.Value);Found=true;break;}}
        if(!Found){R.Data->SetBoolField(TEXT("directional_nonselected_authored_fields_verified"),true);return Raw();}
        if(!R.Data->HasField(TEXT("directional_first_mismatch")))R.Data->SetObjectField(TEXT("directional_first_mismatch"),Difference);
        R.Data->SetBoolField(TEXT("directional_nonselected_authored_fields_verified"),false);
        return Stop(TEXT("component_directional_unselected_fields_changed"),TEXT("Directional notification/structural/compiler return changed protected authored shadow/lightmass/other fields; actual prefix retained, no retry or blanket exemption."));
    }
    bool Check(bool GuidReturn=false)
    {
        if(!Active)return true;if(!Raw())return false;
        if(Light.IsValid())
        {
            const bool Same=Light->OriginalLightGuid==SelectedOriginal&&Light->LightGuid==SelectedLight;
            // LightComponent.cpp271-290: an unowned template has no static shadowing.
            // Only a documented property/compile return may adopt its exact zero reset.
            const bool Reset=GuidReturn&&!Light->GetOwner()&&!Light->OriginalLightGuid.IsValid()&&!Light->LightGuid.IsValid();
            if(!Same&&!Reset)return Stop(TEXT("component_light_guid_changed"),TEXT("Selected GUID changed outside documented owner-null reset."));
            if(!Same){SelectedOriginal=Light->OriginalLightGuid;SelectedLight=Light->LightGuid;R.Data->SetBoolField(TEXT("light_selected_guid_zero_reset_adopted"),true);}
            TArray<UObject*> Current;GetObjectsWithOuter(Light.Get(),Current,EGetObjectsFlags::IncludeNestedObjects);if(Current.Num()!=SelectedChildren.Num())return Stop(TEXT("component_light_selected_subobjects_changed"),TEXT("Selected default-subobject membership changed."));for(const auto& I:SelectedChildren)if(!Current.Contains(I.Hold.Get()))return Stop(TEXT("component_light_selected_subobjects_changed"),TEXT("Selected default-subobject left its role."));
        }
        TArray<UObject*> CurrentSource;GetObjectsWithOuter(Default.Get(),CurrentSource,EGetObjectsFlags::IncludeNestedObjects);if(CurrentSource.Num()!=SourceChildren.Num())return Stop(TEXT("component_light_source_subobjects_changed"),TEXT("Readonly native default subtree changed."));for(auto* O:SourceChildren)if(!CurrentSource.Contains(O))return Stop(TEXT("component_light_source_subobjects_changed"),TEXT("Readonly default subobject left its role."));
        FString Now;for(int32 I=1;I<Source.Num();++I){if(Source[I].Hold.Get()==NativeClass->GetPackage())continue;FString V;if(!Fields(Source[I].Hold.Get(),V))return false;Now+=Source[I].Path+TEXT("\n")+V+TEXT("\n");}
        if(!Now.Equals(SourceFields,ESearchCase::CaseSensitive))return Stop(TEXT("component_light_source_fields_changed"),TEXT("Readonly native default/subobject fields changed."));
        for(const auto& N:Nodes)if(N.Hold.Get()!=Selected.Get()){FString V;if(!Fields(N.Template.Get(),V))return false;if(!V.Equals(N.Fields,ESearchCase::CaseSensitive))return Stop(TEXT("component_light_unselected_fields_changed"),TEXT("Unselected component fields changed."));}
        if(ReferenceMode&&ReferenceReady&&!ReferenceCheck())return false;
        if(!DirectionalCheck())return false;
        R.Data->SetBoolField(TEXT("light_source_and_current_roles_verified"),true);return Raw();
    }
    bool Disk(const FString& File,int64& Bytes,FString& SHA)
    {
        Bytes=IFileManager::Get().FileSize(*File);SHA.Reset();if(Bytes<0)return true;
        TArray<uint8> Data;if(Bytes>64*1024*1024||!FFileHelper::LoadFileToArray(Data,*File)||Data.Num()!=Bytes)return false;
        uint8 Hash[FSHA1::DigestSize];FSHA1::HashBuffer(Data.GetData(),Data.Num(),Hash);SHA=BytesToHex(Hash,FSHA1::DigestSize);return true;
    }
    bool FileFor(const FString& PackageName,FString& File)
    {if(!FPackageName::TryConvertLongPackageNameToFilename(PackageName,File,FPackageName::GetAssetPackageExtension()))return false;File=FPaths::ConvertRelativePathToFull(File);return true;}
    TArray<FString> OtherDirty()
    {TArray<FString> V=DirtyPackages();V.RemoveAll([&](const FString& P){return P.Equals(Package,ESearchCase::CaseSensitive);});return V;}
    bool SameStrings(const TArray<FString>& A,const TArray<FString>& B)
    {if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(!A[I].Equals(B[I],ESearchCase::CaseSensitive))return false;return true;}
    bool ReferenceCheck()
    {
        if(!Raw())return false;FString V;if(!Fields(Light.Get(),V,false,true))return false;
        if(!V.Equals(ReferenceOtherFields,ESearchCase::CaseSensitive))return Stop(TEXT("component_light_related_fields_changed"),TEXT("Unselected current light fields changed; no generic notification effect is adopted."));
        int64 Size;FString Hash;if(!Disk(TargetFile,Size,Hash)||Size!=TargetBytes||!Hash.Equals(TargetSHA,ESearchCase::CaseSensitive)||!SameStrings(OtherDirty(),ReferenceOtherDirty))return Stop(TEXT("component_light_reference_fence_changed"),TEXT("Target disk or other-package dirty state changed; Set requests no saving."));
        for(const auto& A:Assets)
        {
            TArray<UObject*> Nested;GetObjectsWithOuter(A.Asset.Get(),Nested,EGetObjectsFlags::IncludeNestedObjects);
            if(Nested.Num()!=A.Nested.Num())return Stop(TEXT("component_light_reference_subobjects_changed"),TEXT("Source authored subtree membership changed."));
            for(auto* O:A.Nested)if(!Nested.Contains(O))return Stop(TEXT("component_light_reference_subobjects_changed"),TEXT("Retained source subobject left its role."));
            for(int32 I=0;I<A.Ids.Num();++I)
            {if(A.Ids[I].Hold.Get()==A.Class.Get()||A.Ids[I].Hold.Get()==A.Asset->GetPackage())continue;if(!Fields(A.Ids[I].Hold.Get(),V,true)||!V.Equals(A.Authored[I],ESearchCase::CaseSensitive))return Stop(TEXT("component_light_reference_authored_changed"),TEXT("Readonly source/CDO authored fields changed."));}
            if(!Disk(A.File,Size,Hash)||Size!=A.Bytes||!Hash.Equals(A.SHA,ESearchCase::CaseSensitive))return Stop(TEXT("component_light_reference_disk_changed"),TEXT("Readonly source disk changed."));
        }return Raw();
    }
    bool KeepReference(UObject* O)
    {
        if(!O)return true;for(const auto& A:Assets)if(A.Asset.Get()==O)return true;
        if(!Check())return false;if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage()))return Stop(TEXT("component_light_reference_unreadable"),TEXT("Current source object/class/package is unreadable."));
        UClass* K=O->GetClass();if((K!=UMaterial::StaticClass()&&K!=UMaterialInstanceConstant::StaticClass()&&K!=UTextureLightProfile::StaticClass())||O->GetOuter()!=O->GetPackage()||O->GetPackage()==BP->GetPackage()||O->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||O->GetPackage()->IsDirty()||O->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||K->GetClass()!=UClass::StaticClass()||!K->HasAnyClassFlags(CLASS_Native)||K->ClassGeneratedBy||K->GetPackage()->GetName()!=TEXT("/Script/Engine")||!K->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))
            return Stop(TEXT("component_light_reference_unsupported"),TEXT("Exact saved clean runtime Engine Material/MIC or TextureLightProfile source required."));
        FAssetRole A;A.Asset.Reset(O);A.Class.Reset(K);UObject* D=K->GetDefaultObject(false);
        if(!Loaded(D)||D->GetClass()!=K||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Stop(TEXT("component_light_reference_cdo_unavailable"),TEXT("Existing source CDO required; no warming/creation."));
        A.CDO.Reset(D);A.ClassFlags=K->GetClassFlags();A.Super=K->GetSuperClass();A.PkgFlags=O->GetPackage()->GetPackageFlags();
        if(!Pin(O,A.Ids)||!Pin(K,A.Ids)||!Pin(D,A.Ids)||!Pin(O->GetPackage(),A.Ids))return false;
        GetObjectsWithOuter(O,A.Nested,EGetObjectsFlags::IncludeNestedObjects);if(A.Nested.Num()>128)return Stop(TEXT("component_light_reference_bound"),TEXT("Source subtree bound exceeded."));
        for(auto* N:A.Nested)if(!Pin(N,A.Ids))return false;
        if(!FileFor(O->GetPackage()->GetName(),A.File)||!Disk(A.File,A.Bytes,A.SHA)||A.Bytes<=0)return Stop(TEXT("component_light_reference_disk_unavailable"),TEXT("Bounded nonempty saved source disk required."));
        Assets.Add(MoveTemp(A));auto& Added=Assets.Last();Added.Authored.SetNum(Added.Ids.Num());
        for(int32 I=0;I<Added.Ids.Num();++I)if(Added.Ids[I].Hold.Get()!=K&&Added.Ids[I].Hold.Get()!=O->GetPackage()&&!Fields(Added.Ids[I].Hold.Get(),Added.Authored[I],true))return false;
        auto Audit=ReadPackageDependencies({O->GetPackage()->GetName()},true,4096);if(!Check())return false;
        bool Passed=false;if(!Audit->TryGetBoolField(TEXT("passed"),Passed)||!Passed)return Stop(TEXT("component_light_reference_dependencies_unverified"),TEXT("Source dependency audit failed; no target setter dispatched."));
        const TSet<FString> Native={TEXT("/Script/Engine"),TEXT("/Script/CoreUObject"),TEXT("/Script/PhysicsCore"),TEXT("/Script/RenderCore"),TEXT("/Script/RHI")};
        for(const auto& Edge:Audit->GetArrayField(TEXT("edges"))){const FString P=Edge->AsObject()->GetStringField(TEXT("to"));if(!P.StartsWith(TEXT("/Game/"))&&!P.StartsWith(TEXT("/Engine/"))&&!Native.Contains(P))return Stop(TEXT("component_light_reference_dependency_unsupported"),TEXT("Source dependency uses an unaudited module or mount."));}
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("asset"),O->GetPathName());Row->SetStringField(TEXT("class"),K->GetPathName());Row->SetStringField(TEXT("sha1"),Added.SHA);Row->SetNumberField(TEXT("bytes"),Added.Bytes);Row->SetObjectField(TEXT("dependency_audit"),Audit);
        if(auto* M=Cast<UMaterial>(O))Row->SetNumberField(TEXT("material_domain"),static_cast<int32>(M->MaterialDomain));
        R.Data->SetObjectField(TEXT("light_reference_source_")+FString::FromInt(Assets.Num()),Row);return Check();
    }
    bool ArmReference(const FString& Name)
    {
        if(!Check())return false;auto* L=Cast<ULightComponent>(Light.Get());if(!Loaded(L))return false;
        ReferenceMode=true;ReferenceName=FName(*Name);ExpectedFunction=L->LightFunctionMaterial.Get();ExpectedIES=L->IESTexture.Get();ExpectedStash=L->StashedLightFunctionMaterial.Get();
        if(!Fields(L,ReferenceOtherFields,false,true)||!FileFor(Package,TargetFile)||!Disk(TargetFile,TargetBytes,TargetSHA))return Stop(TEXT("component_light_reference_baseline_incomplete"),TEXT("Target reference baseline is incomplete."));
        ReferenceOtherDirty=OtherDirty();ReferenceReady=true;
        if(!KeepReference(ExpectedFunction)||!KeepReference(ExpectedIES)||!KeepReference(ExpectedStash))return false;
        R.Data->SetStringField(TEXT("light_reference_scope"),TEXT("Set-only stock LoadObject original path/empty clear; saved source authored/disk proof after initial load; scoped stash transition; no source compile/save/CDO creation, render/runtime proof."));return Check();
    }
    bool LoadReference(FObjectPropertyBase* Property,const TSharedPtr<FJsonValue>& Json,FPreparedProperty& Prepared)
    {
        if(!ReferenceMode||Property->GetFName()!=ReferenceName)return Fail(R,TEXT("component_light_reference_set_only"),TEXT("Light references are supported by the Set API only."));
        if(Json->Type!=EJson::String)return Fail(R,TEXT("component_light_reference_invalid"),TEXT("UE56 object references require a quoted path; empty string clears. JSON null is not the legacy clear value."));
        const FString Path=Json->AsString();if(!Path.IsEmpty())
        {
            FString ObjectPath=FPackageName::ExportTextPathToObjectPath(Path);
            if(!SafeName(Path)||!SafeName(ObjectPath)||(!ObjectPath.StartsWith(TEXT("/Game/"))&&!ObjectPath.StartsWith(TEXT("/Engine/"))))return Fail(R,TEXT("component_light_reference_path_invalid"),TEXT("Bounded /Game or /Engine runtime object path required; no fallback is synthesized."));
            if(!ObjectPath.Contains(TEXT("."))){if(!FPackageName::IsValidLongPackageName(ObjectPath,true))return Fail(R,TEXT("component_light_reference_path_invalid"),TEXT("Invalid asset package path."));ObjectPath+=TEXT(".")+FPackageName::GetLongPackageAssetName(ObjectPath);}
            if(!SafeName(ObjectPath)||!FPackageName::IsValidObjectPath(ObjectPath))return Fail(R,TEXT("component_light_reference_path_invalid"),TEXT("Expanded object-path validation failed; original string has not been loaded."));
        }
        if(!Check())return false;FString BeforeFields;if(!Fields(Light.Get(),BeforeFields))return false;const bool DirtyBefore=BP->GetPackage()->IsDirty();
        auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("requested"),Path);Call->SetBoolField(TEXT("attempted"),!Path.IsEmpty());Call->SetBoolField(TEXT("implicit_postload_work_possible"),!Path.IsEmpty());R.Data->SetObjectField(TEXT("light_reference_load"),Call);
        UObject* Object=nullptr;if(!Path.IsEmpty()){++ReferenceLoadCalls;R.Data->SetNumberField(TEXT("light_reference_load_calls"),ReferenceLoadCalls);Object=StaticLoadObject(Property->PropertyClass,nullptr,*Path);}
        // Keep a returned source alive before any exporter/audit/notification.
        Call->SetBoolField(TEXT("returned_nonnull"),Object!=nullptr);if(!Context())return false;TStrongObjectPtr<UObject> Returned(Object);
        if(!Check())return false;FString AfterFields;if(!Fields(Light.Get(),AfterFields))return false;
        if(!BeforeFields.Equals(AfterFields,ESearchCase::CaseSensitive)||BP->GetPackage()->IsDirty()!=DirtyBefore)return Stop(TEXT("component_light_reference_load_changed_target"),TEXT("Initial source load changed the current target baseline."));
        if(!Path.IsEmpty()&&(!Loaded(Object)||!Object->IsA(Property->PropertyClass)))return Fail(R,TEXT("component_light_reference_load_failed"),TEXT("Stock typed LoadObject did not return a readable compatible source. Target setter/save not dispatched; loading effects outside the scoped proof remain unverified."));
        if(Object&&!KeepReference(Object))return false;Prepared.Reference.Reset(Object);return Check();
    }
    bool BeginReferenceApply(const FPreparedProperty& P)
    {
        if(!ReferenceMode)return true;if(!Check()||P.Property->GetFName()!=ReferenceName)return false;
        PlannedFunction=ExpectedFunction;PlannedIES=ExpectedIES;PlannedStash=ExpectedStash;
        if(ReferenceName==TEXT("LightFunctionMaterial")){PlannedFunction=P.Reference.Get();PlannedStash=nullptr;}
        else {PlannedIES=P.Reference.Get();if(PlannedStash)PlannedFunction=PlannedStash;}
        return true;
    }
    bool EndReferenceApply(bool Returned)
    {
        if(!ReferenceMode)return true;ExpectedFunction=PlannedFunction;ExpectedIES=PlannedIES;ExpectedStash=PlannedStash;
        R.Data->SetBoolField(TEXT("light_reference_setter_returned"),Returned);return Check(true);
    }
    bool Adopt(USCS_Node* Node,bool IsAttached)
    {
        if(!Context()||!Loaded(Node)||Node->GetClass()!=USCS_Node::StaticClass()||Node->GetOuter()!=SCS.Get())return Stop(TEXT("component_light_returned_node_invalid"),TEXT("Returned node must have exact own SCS ownership."));
        Selected.Reset(Node);auto* T=Cast<ULightComponentBase>(Node->ComponentTemplate.Get());if(!Loaded(T)||T->GetClass()!=NativeClass.Get()||T->GetOuter()!=Generated.Get()||T->GetPackage()!=BP->GetPackage()||Node->ComponentClass!=NativeClass.Get()||!Node->VariableGuid.IsValid()||!Node->GetVariableName().ToString().Equals(SelectedName,ESearchCase::CaseSensitive))return Stop(TEXT("component_light_returned_template_invalid"),TEXT("Exact current owned light template/class/GUID/name required."));
        Light.Reset(T);if(!Pin(T,CurrentObjects))return false;SelectedNodeGuid=Node->VariableGuid;SelectedNodePath=Node->GetPathName();SelectedTemplatePath=T->GetPathName();SelectedNodeFlags=Node->GetFlags();SelectedTemplateFlags=T->GetFlags();SelectedOriginal=T->OriginalLightGuid;SelectedLight=T->LightGuid;Attached=IsAttached;SelectedChildren.Reset();TArray<UObject*> Children;GetObjectsWithOuter(T,Children,EGetObjectsFlags::IncludeNestedObjects);if(Children.Num()>128)return Stop(TEXT("component_light_selected_subobject_bound"),TEXT("Selected default subtree bound exceeded."));for(auto* O:Children)if(!Pin(O,SelectedChildren))return false;
        if(!Raw())return false;
        if(NativeClass.Get()==UDirectionalLightComponent::StaticClass())
        {if(!DirectionalReady){if(!Fields(Light.Get(),DirectionalFields,true,false,true))return false;DirectionalReady=true;R.Data->SetStringField(TEXT("directional_property_scope"),TEXT("Intensity only; Add also applies explicit relative transforms. All other editable nontransient selected fields, including shadow/lightmass, stay literal protected. Native caches/render resources are not global purity proof."));}else if(!DirectionalCheck())return false;}
        return Raw();
    }
    bool Bind(UBlueprint* B,UClass* K,const FString& Name,USCS_Node* Existing)
    {
        Active=true;Editor=GEditor;World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!Context())return false;
        if(!Loaded(B)||!Loaded(K)||!NativeLightClass(K)||K->GetClass()!=UClass::StaticClass()||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||K->ClassGeneratedBy||K->GetPackage()->GetName()!=TEXT("/Script/Engine")||!K->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||K->GetPackage()->IsDirty())
            return Stop(TEXT("component_light_source_unsupported"),TEXT("Exact runtime compiled-in Engine Point/Spot/Rect/Directional light class required."));
        if(K==UDirectionalLightComponent::StaticClass()&&!DirectionalWritesFixed)return Stop(TEXT("component_directional_write_plan_missing"),TEXT("Exact selected property names must be fixed before Directional observation/adoption."));
        BP.Reset(B);SCS.Reset(B->SimpleConstructionScript);Generated.Reset(B->GeneratedClass);NativeClass.Reset(K);BPPath=B->GetPathName();Package=B->GetPackage()->GetName();ParentClass=B->ParentClass;SelectedName=Name;SourceSuper=K->GetSuperClass();ClassFlags=K->GetClassFlags();PackageFlags=K->GetPackage()->GetPackageFlags();
        UActorComponent* D=Cast<UActorComponent>(K->GetDefaultObject(false));if(!Loaded(D)||D->GetClass()!=K||D->GetOuter()!=K->GetOuter()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Stop(TEXT("component_light_cdo_unavailable"),TEXT("Existing native light CDO required; no hidden default creation."));
        Default.Reset(D);if(!Pin(K,Source)||!Pin(D,Source)||!Pin(K->GetPackage(),Source))return false;
        GetObjectsWithOuter(D,SourceChildren,EGetObjectsFlags::IncludeNestedObjects);if(SourceChildren.Num()>128)return Stop(TEXT("component_light_source_bound"),TEXT("Native default subtree bound exceeded."));for(auto* O:SourceChildren)if(!Pin(O,Source))return false;
        Skeleton.Reset(B->SkeletonGeneratedClass);
        // Kismet2.cpp475 creates SCS under NewClass; Blueprint.cpp1469/1476
        // requires that exact current GeneratedClass outer, not the UBlueprint.
        // This diagnostic uses only plain predicate results; no failure-side
        // UObject path/class/outer dereference or additional observation follows.
        auto TargetFailure=[&](const TCHAR* Predicate,const FString& Expected,const FString& Actual)
        {
            if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("predicate"),Predicate);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("light_target_role_failure"),J);}
            return Stop(TEXT("component_light_target_bound"),TEXT("Bounded exact current own SCS/generated/skeleton required."));
        };
        if(!Loaded(SCS.Get()))return TargetFailure(TEXT("scs_loaded"),TEXT("true"),TEXT("false"));
        if(SCS->GetClass()!=USimpleConstructionScript::StaticClass())return TargetFailure(TEXT("scs_exact_class"),TEXT("USimpleConstructionScript"),TEXT("different_class"));
        if(!Loaded(Generated.Get()))return TargetFailure(TEXT("generated_loaded"),TEXT("true"),TEXT("false"));
        if(SCS->GetOuter()!=Generated.Get())return TargetFailure(TEXT("scs_outer_is_current_generated"),TEXT("true"),TEXT("false"));
        if(SCS->GetPackage()!=B->GetPackage())return TargetFailure(TEXT("scs_package_is_selected_package"),TEXT("true"),TEXT("false"));
        if(!Loaded(Skeleton.Get()))return TargetFailure(TEXT("skeleton_loaded"),TEXT("true"),TEXT("false"));
        const int32 InitialNodeCount=SCS->GetAllNodes().Num();if(InitialNodeCount>256)return TargetFailure(TEXT("current_allnodes_count"),TEXT("<=256"),FString::FromInt(InitialNodeCount));
        GeneratedPath=Generated->GetPathName();SkeletonPath=Skeleton->GetPathName();SkeletonSuper=Skeleton->GetSuperClass();
        if(!Pin(B,Stable)||!Pin(SCS.Get(),Stable)||!ClassRole(Generated.Get(),false)||!ClassRole(Skeleton.Get(),true))return Stop(TEXT("component_light_target_identity_invalid"),TEXT("Exact same-Blueprint current authoring identities required."));
        Roots=SCS->GetRootNodes();for(auto* P:SCS->GetAllNodes())
        {if(!Loaded(P)||!Loaded(P->ComponentTemplate.Get())||!Pin(P,Stable))return Stop(TEXT("component_light_initial_node_unreadable"),TEXT("Initial own node/template unavailable."));FNode N;N.Hold.Reset(P);N.Template.Reset(P->ComponentTemplate);N.Class=P->ComponentClass;N.Guid=P->VariableGuid;N.Name=P->GetVariableName().ToString();N.Children=P->GetChildNodes();if(auto* L=Cast<ULightComponentBase>(N.Template.Get())){N.OriginalLight=L->OriginalLightGuid;N.Light=L->LightGuid;}Nodes.Add(MoveTemp(N));}
        if(!CaptureCurrent())return false;if(Existing&&!Adopt(Existing,true))return false;
        for(int32 I=1;I<Source.Num();++I){if(Source[I].Hold.Get()==K->GetPackage())continue;FString V;if(!Fields(Source[I].Hold.Get(),V))return false;SourceFields+=Source[I].Path+TEXT("\n")+V+TEXT("\n");}
        for(auto& N:Nodes)if(N.Hold.Get()!=Selected.Get()&&!Fields(N.Template.Get(),N.Fields))return false;
        R.Data->SetStringField(TEXT("light_authoring_scope"),TEXT("Exact native Point/Spot/Rect/Directional own local unregistered SCS template; native CDO/default subtree readonly; selected owner-null GUID zero reset only at SDK return. No FColor/texture/render/runtime/registration proof."));return Check();
    }
    bool Attach(USCS_Node* Parent)
    {if(!Context())return false;if(Parent){for(auto& N:Nodes)if(N.Hold.Get()==Parent)N.Children.Add(Selected.Get());}else Roots.Add(Selected.Get());Attached=true;return Check();}
    bool Reacquire()
    {
        // The compiler may replace generated/skeleton classes and templates, never
        // the retained Blueprint/SCS/node identities. Admit the new roles first.
        if(!Anchors()||!Loaded(BP.Get())||BP->ParentClass!=ParentClass||BP->SimpleConstructionScript!=SCS.Get()||!ClassRole(BP->GeneratedClass,false)||!ClassRole(BP->SkeletonGeneratedClass,true))return Stop(TEXT("component_light_reinstance_unobservable"),TEXT("Compiler returned without exact current own child class roles."));
        Generated.Reset(BP->GeneratedClass);Skeleton.Reset(BP->SkeletonGeneratedClass);for(auto& N:Nodes)
        {auto* P=N.Hold.Get();if(!Loaded(P)||!SCS->GetAllNodes().Contains(P)||P->ComponentClass!=N.Class||P->VariableGuid!=N.Guid||!P->GetVariableName().ToString().Equals(N.Name,ESearchCase::CaseSensitive)||!Loaded(P->ComponentTemplate.Get())||P->ComponentTemplate->GetClass()!=N.Class||P->ComponentTemplate->GetOuter()!=Generated.Get()||P->ComponentTemplate->GetPackage()!=BP->GetPackage())return Stop(TEXT("component_light_reinstance_role_changed"),TEXT("Compiler may replace only current own templates with same node/GUID/name/class."));N.Template.Reset(P->ComponentTemplate);}
        if(!CaptureCurrent())return false;
        if(Selected.IsValid())
        {const FGuid OldOriginal=SelectedOriginal,OldLight=SelectedLight;auto* P=Selected.Get();if(!Loaded(P)||P->VariableGuid!=SelectedNodeGuid||!P->GetPathName().Equals(SelectedNodePath,ESearchCase::CaseSensitive))return Stop(TEXT("component_light_compiled_node_changed"),TEXT("Compiler changed the exact selected node/GUID/path."));if(!Adopt(P,Attached))return false;const bool Same=SelectedOriginal==OldOriginal&&SelectedLight==OldLight;const bool Zero=!SelectedOriginal.IsValid()&&!SelectedLight.IsValid();if(!Same&&!Zero)return Stop(TEXT("component_light_compiled_guid_changed"),TEXT("Compiler changed selected GUID beyond owner-null duplicate reset."));}
        return Check(true);
    }
};
bool LightReadAdmission(){return !CurrentLightScope||!CurrentLightScope->Active||CurrentLightScope->Raw();}
bool PrepareLightReference(FObjectPropertyBase* Property,const TSharedPtr<FJsonValue>& Json,FPreparedProperty& Prepared)
{return CurrentLightScope&&CurrentLightScope->LoadReference(Property,Json,Prepared);}

// KF: exact empty native HISM authoring only. No instance/tree/runtime operation.
struct FHismScope
{
    FMutationResult& R;bool Active=false,Stopped=false,Attached=false;decltype(GUndo) Undo=nullptr;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<UBlueprint> BP;
    TStrongObjectPtr<USimpleConstructionScript> SCS;TStrongObjectPtr<UClass> Generated,Skeleton,Parent,Class;
    TStrongObjectPtr<USCS_Node> Selected;TStrongObjectPtr<UHierarchicalInstancedStaticMeshComponent> Template;
    TArray<TStrongObjectPtr<UObject>> Holds;TSharedPtr<FJsonObject> NativeBefore;
    struct FRole{TStrongObjectPtr<USCS_Node> Node;TStrongObjectPtr<UActorComponent> Template;UClass* Class=nullptr;FName Name;FGuid Guid;FString Path;};TArray<FRole> Old;
    FString Name,NodePath;FGuid NodeGuid;
    // PW opt-in is used only by the existing mesh/material leaf, never Add or generic property writes.
    bool AllowPopulated=false,Populated=false,DataReady=false;
    static constexpr int32 InstanceBound=256,CustomWidthBound=16,CustomValueBound=4096;
    TArray<FMatrix> InstanceMatrices;TArray<float> InstanceCustom;int32 CustomWidth=0,RandomSeed=0;
    TArray<FInstancedStaticMeshRandomSeed> RandomRanges;TStrongObjectPtr<UObject> ExistingNativeCDO;
    UStaticMesh* ExpectedMesh=nullptr;UStaticMesh* PlannedMesh=nullptr;bool MeshWindow=false,MaterialWindow=false;
    TArray<UMaterialInterface*> ExpectedMaterials,PlannedMaterials,ExpectedOverlays;
    bool BindingCurrent()
    {
        if(!Populated)return true;
        const auto& MeshSlot=Template->GetStaticMesh();if(!MeshSlot.IsResolved())return Stop(TEXT("populated_mesh_slot_unresolved"));
        UStaticMesh* M=MeshSlot.Get();if((M&&!Loaded(M))||(M!=ExpectedMesh&&(!MeshWindow||M!=PlannedMesh)))return Stop(TEXT("populated_mesh_binding"));
        const int32 MaterialCount=Template->OverrideMaterials.Num();
        if(MaterialCount>1024||(MaterialCount!=ExpectedMaterials.Num()&&(!MaterialWindow||MaterialCount!=PlannedMaterials.Num())))return Stop(TEXT("populated_material_count"));
        TArray<UMaterialInterface*> Actual;
        for(const auto& Slot:Template->OverrideMaterials) { if(!Slot.IsResolved())return Stop(TEXT("populated_material_slot_unresolved"));auto* P=Slot.Get();if(P&&!Loaded(P))return Stop(TEXT("populated_material_role"));Actual.Add(P); }
        if(Actual!=ExpectedMaterials&&(!MaterialWindow||Actual!=PlannedMaterials))return Stop(TEXT("populated_material_bindings"));
        const auto& Overlay=Template->GetComponentMaterialSlotsOverlayMaterial();if(Overlay.Num()!=ExpectedOverlays.Num())return Stop(TEXT("populated_overlay_count"));
        for(int32 I=0;I<Overlay.Num();++I)if(!Overlay[I].IsResolved()||Overlay[I].Get()!=ExpectedOverlays[I]||(ExpectedOverlays[I]&&!Loaded(ExpectedOverlays[I])))return Stop(TEXT("populated_overlay_binding"));
        return true;
    }
    bool CaptureBindings()
    {
        const auto& MeshSlot=Template->GetStaticMesh();if(!MeshSlot.IsResolved())return Stop(TEXT("populated_initial_mesh_unresolved"));ExpectedMesh=MeshSlot.Get();if(ExpectedMesh)Holds.Emplace(ExpectedMesh);
        if(Template->OverrideMaterials.Num()>1024||Template->GetComponentMaterialSlotsOverlayMaterial().Num()>1024)return Stop(TEXT("populated_initial_material_bound"));
        for(const auto& Slot:Template->OverrideMaterials) { if(!Slot.IsResolved())return Stop(TEXT("populated_initial_material_unresolved"));auto* M=Slot.Get();if(M)Holds.Emplace(M);ExpectedMaterials.Add(M); }
        for(const auto& Slot:Template->GetComponentMaterialSlotsOverlayMaterial()) { if(!Slot.IsResolved())return Stop(TEXT("populated_initial_overlay_unresolved"));auto* M=Slot.Get();if(M)Holds.Emplace(M);ExpectedOverlays.Add(M); }
        return BindingCurrent();
    }
    bool BeginMesh(UStaticMesh* Desired)
    {
        if(!Populated)return Check();if(!Check())return false;if(!Loaded(Desired))return Stop(TEXT("populated_desired_mesh_lost"));Holds.Emplace(Desired);PlannedMesh=Desired;MeshWindow=true;return true;
    }
    bool EndMesh(bool Returned)
    {
        if(!Populated)return Check();if(!Check())return false;
        const auto& Slot=Template->GetStaticMesh();if(!Slot.IsResolved()||Slot.Get()!=(Returned?PlannedMesh:ExpectedMesh))return Stop(TEXT("populated_mesh_returned_binding"));
        if(Returned)ExpectedMesh=PlannedMesh;MeshWindow=false;return Check();
    }
    bool BeginMaterial(UMaterialInterface* Desired)
    {
        if(!Populated)return Check();if(!Check())return false;if(!Loaded(Desired))return Stop(TEXT("populated_desired_material_lost"));Holds.Emplace(Desired);PlannedMaterials=ExpectedMaterials;if(PlannedMaterials.IsEmpty())PlannedMaterials.Add(nullptr);PlannedMaterials[0]=Desired;MaterialWindow=true;return true;
    }
    bool EndMaterial()
    {
        if(!Populated)return Check();if(!Check())return false;TArray<UMaterialInterface*> A;for(const auto& Slot:Template->OverrideMaterials){if(!Slot.IsResolved())return Stop(TEXT("populated_material_returned_unresolved"));A.Add(Slot.Get());}
        if(A!=PlannedMaterials)return Stop(TEXT("populated_material_returned_binding"));ExpectedMaterials=PlannedMaterials;MaterialWindow=false;return Check();
    }
    static bool PopulatedEligible(UActorComponent* C)
    {
        auto* H=Cast<UHierarchicalInstancedStaticMeshComponent>(C);
        return Loaded(H)&&H->GetClass()==UHierarchicalInstancedStaticMeshComponent::StaticClass()&&H->HasAnyFlags(RF_ArchetypeObject)&&!H->HasAnyFlags(RF_ClassDefaultObject)
            &&!H->IsRegistered()&&!H->IsPreRegistering()&&!H->IsPhysicsStateCreated()&&!H->HasBegunPlay()&&!H->GetOwner()&&!H->GetWorld()&&!H->IsWelded()&&!H->IsAsyncBuilding()
            &&H->GetInstanceCount()>0&&H->GetInstanceCount()<=InstanceBound&&H->NumCustomDataFloats>=0&&H->NumCustomDataFloats<=CustomWidthBound
            &&H->PerInstanceSMCustomData.Num()==H->GetInstanceCount()*H->NumCustomDataFloats&&H->PerInstanceSMCustomData.Num()<=CustomValueBound&&H->AdditionalRandomSeeds.Num()<=InstanceBound;
    }
    bool TemplateRole(UActorComponent* C)const { return Populated?PopulatedEligible(C):Empty(C); }
    bool Mismatch(const TCHAR* Field,int32 Index,const FString& Expected,const FString& Actual)
    {
        if(!R.Data->HasField(TEXT("hism_first_user_data_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object_path"),NodePath);J->SetStringField(TEXT("field"),Field);J->SetNumberField(TEXT("index"),Index);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("operands_observed"),true);R.Data->SetObjectField(TEXT("hism_first_user_data_mismatch"),J);
        }
        return Stop(TEXT("populated_instance_user_data"));
    }
    bool ResolvedPopulatedRoles()const
    {
        if(!Loaded(BP.Get())||!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript.Get()!=SCS.Get())return false;
        auto P=BP->ParentClass;auto G=BP->GeneratedClass;auto K=BP->SkeletonGeneratedClass;
        if(!P.GetGCPtr().IsResolved()||!G.GetGCPtr().IsResolved()||!K.GetGCPtr().IsResolved()||P.GetGCPtr().Get()!=Parent.Get()||G.GetGCPtr().Get()!=Generated.Get()||K.GetGCPtr().Get()!=Skeleton.Get())return false;
        if(!Loaded(Class.Get())||!Class->ClassDefaultObject.IsResolved()||Class->ClassDefaultObject.Get()!=ExistingNativeCDO.Get())return false;
        for(const auto& O:Old)
        {
            auto* N=O.Node.Get();if(!Loaded(N)||!N->ComponentTemplate.IsResolved())return false;auto C=N->ComponentClass;
            if(!C.IsResolved()||C.Get()!=O.Class||N->ComponentTemplate.Get()!=O.Template.Get())return false;
        }
        if(Selected.IsValid()) { auto* N=Selected.Get();if(!N->ComponentTemplate.IsResolved()||N->ComponentTemplate.Get()!=Template.Get())return false; }
        return true;
    }
    bool CapturePopulated(USCS_Node* N)
    {
        if(!Loaded(N)||!N->ComponentTemplate.IsResolved())return Stop(TEXT("populated_initial_node"));
        UActorComponent* Returned=N->ComponentTemplate.Get();const TStrongObjectPtr<UActorComponent> Immediate(Returned);
        if(!PopulatedEligible(Returned))return Stop(TEXT("populated_initial_profile"));
        Selected.Reset(N);Template.Reset(static_cast<UHierarchicalInstancedStaticMeshComponent*>(Returned));NodePath=N->GetPathName();NodeGuid=N->VariableGuid;Attached=true;
        CustomWidth=Template->NumCustomDataFloats;InstanceCustom=Template->PerInstanceSMCustomData;RandomSeed=Template->InstancingRandomSeed;RandomRanges=Template->AdditionalRandomSeeds;
        for(const auto& D:Template->PerInstanceSMData)
        {
            const FMatrix M=D.Transform;for(int32 Row=0;Row<4;++Row)for(int32 Col=0;Col<4;++Col)if(!FMath::IsFinite(M.M[Row][Col]))return Stop(TEXT("populated_nonfinite_local_matrix"));InstanceMatrices.Add(M);
        }
        for(float V:InstanceCustom)if(!FMath::IsFinite(V))return Stop(TEXT("populated_nonfinite_custom_data"));
        DataReady=true;return CaptureBindings();
    }
    bool UserDataCurrent()
    {
        if(!Populated)return true;if(!DataReady||!Loaded(Template.Get()))return Stop(TEXT("populated_user_data_not_bound"));
        if(Template->PerInstanceSMData.Num()!=InstanceMatrices.Num())return Mismatch(TEXT("instance_count"),INDEX_NONE,LexToString(InstanceMatrices.Num()),LexToString(Template->PerInstanceSMData.Num()));
        if(Template->NumCustomDataFloats!=CustomWidth)return Mismatch(TEXT("NumCustomDataFloats"),INDEX_NONE,LexToString(CustomWidth),LexToString(Template->NumCustomDataFloats));
        if(Template->PerInstanceSMCustomData.Num()!=InstanceCustom.Num())return Mismatch(TEXT("custom_data_count"),INDEX_NONE,LexToString(InstanceCustom.Num()),LexToString(Template->PerInstanceSMCustomData.Num()));
        for(int32 I=0;I<InstanceMatrices.Num();++I)for(int32 Row=0;Row<4;++Row)for(int32 Col=0;Col<4;++Col)
        {
            const double A=Template->PerInstanceSMData[I].Transform.M[Row][Col],E=InstanceMatrices[I].M[Row][Col];
            if(!FMath::IsFinite(A)||A!=E)return Mismatch(TEXT("local_matrix"),I*16+Row*4+Col,LexToString(E),LexToString(A));
        }
        for(int32 I=0;I<InstanceCustom.Num();++I) { const float A=Template->PerInstanceSMCustomData[I];if(!FMath::IsFinite(A)||A!=InstanceCustom[I])return Mismatch(TEXT("custom_data"),I,LexToString(InstanceCustom[I]),LexToString(A)); }
        if(Template->InstancingRandomSeed!=RandomSeed||Template->AdditionalRandomSeeds.Num()!=RandomRanges.Num())return Stop(TEXT("populated_random_seed_inventory"));
        for(int32 I=0;I<RandomRanges.Num();++I) { const auto& A=Template->AdditionalRandomSeeds[I];const auto& E=RandomRanges[I];if(A.StartInstanceIndex!=E.StartInstanceIndex||A.RandomSeed!=E.RandomSeed)return Stop(TEXT("populated_random_seed_range")); }
        return true;
    }
    TSharedRef<FJsonObject> UserDataReceipt()const
    {
        auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("instance_count"),InstanceMatrices.Num());J->SetNumberField(TEXT("num_custom_data_floats"),CustomWidth);J->SetNumberField(TEXT("random_seed"),RandomSeed);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& M:InstanceMatrices) { TArray<TSharedPtr<FJsonValue>> Values;for(int32 I=0;I<4;++I)for(int32 K=0;K<4;++K)Values.Add(MakeShared<FJsonValueNumber>(M.M[I][K]));Rows.Add(MakeShared<FJsonValueArray>(Values)); }J->SetArrayField(TEXT("local_instance_matrices"),Rows);
        Rows.Reset();for(float V:InstanceCustom)Rows.Add(MakeShared<FJsonValueNumber>(V));J->SetArrayField(TEXT("custom_data"),Rows);
        Rows.Reset();for(const auto& V:RandomRanges) { auto Q=MakeShared<FJsonObject>();Q->SetNumberField(TEXT("start_instance_index"),V.StartInstanceIndex);Q->SetNumberField(TEXT("random_seed"),V.RandomSeed);Rows.Add(MakeShared<FJsonValueObject>(Q)); }J->SetArrayField(TEXT("additional_random_seeds"),Rows);
        J->SetStringField(TEXT("authority"),TEXT("Original captured public PerInstanceSMData local matrices and custom-data arrays; every current check compares actual elements to these expectations. No FTransform roundtrip/reordering/late reseed."));return J;
    }
    bool FinishPopulated()
    {
        if(!Populated)return Check();if(!Check())return false;
        R.Data->SetObjectField(TEXT("hism_user_data_after"),UserDataReceipt());R.Data->SetBoolField(TEXT("hism_user_data_preserved"),true);
        R.Data->SetBoolField(TEXT("hism_async_building_after"),Template->IsAsyncBuilding());R.Data->SetBoolField(TEXT("hism_tree_fully_built_after"),Template->IsTreeFullyBuilt());
        R.Data->SetBoolField(TEXT("hism_derived_tree_identity_preserved"),false);R.Data->SetBoolField(TEXT("hism_render_physics_nav_completion_verified"),false);return Check();
    }

    explicit FHismScope(FMutationResult& In,bool InAllowPopulated=false):R(In),AllowPopulated(InAllowPopulated){}
    static bool Loaded(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Stop(const TCHAR* Boundary)
    {Stopped=true;R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=false;R.Data->SetStringField(TEXT("hism_failed_boundary"),Boundary);R.Data->SetBoolField(TEXT("hism_followup_sdk_stopped"),true);R.Stage=TEXT("verification");return Fail(R,TEXT("component_hism_current_scope_changed"),TEXT("HISM source/current editor/SCS/template or captured instance data changed across an SDK boundary; actual prefix retained without SDK follow-up, cleanup or automatic retry."));}
    bool Context()const
    {return IsEditorGameThread()&&!IsPlaySessionActive()&&!IsGarbageCollecting()&&!IsEngineExitRequested()&&!GIsRequestingExit&&!UE::IsSavingPackage()&&Loaded(Editor.Get())&&GEditor==Editor.Get()&&Loaded(World.Get())&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get();}
    static bool Empty(UActorComponent* C)
    {auto* H=Cast<UHierarchicalInstancedStaticMeshComponent>(C);return Loaded(H)&&H->GetClass()==UHierarchicalInstancedStaticMeshComponent::StaticClass()&&H->HasAnyFlags(RF_ArchetypeObject)&&!H->HasAnyFlags(RF_ClassDefaultObject)&&!H->IsRegistered()&&!H->IsPreRegistering()&&!H->IsPhysicsStateCreated()&&!H->HasBegunPlay()&&!H->GetOwner()&&!H->GetWorld()&&H->GetInstanceCount()==0&&!H->IsAsyncBuilding();}
    bool Roles()const
    {return Loaded(BP.Get())&&Loaded(SCS.Get())&&BP->SimpleConstructionScript.Get()==SCS.Get()&&BP->ParentClass.Get()==Parent.Get()&&Loaded(Parent.Get())&&BP->GeneratedClass.Get()==Generated.Get()&&BP->SkeletonGeneratedClass.Get()==Skeleton.Get()&&Loaded(Generated.Get())&&Loaded(Skeleton.Get())&&Generated->ClassGeneratedBy==BP.Get()&&Skeleton->ClassGeneratedBy==BP.Get()&&!BP->bBeingCompiled&&!BP->bQueuedForCompilation&&!BP->bSuppressStructurallyModified&&!BP->bIsRegeneratingOnLoad;}
    bool Source()
    {if(!Loaded(Class.Get())||(Populated&&(!ResolvedPopulatedRoles()||!Context()||!Roles())))return false;const auto V=NativeIsmSourceObservation(Class.Get());if(Populated&&(!ResolvedPopulatedRoles()||!Context()||!Roles()))return false;return V->GetBoolField(TEXT("native_identity_verified"))&&FJsonValue::CompareEqual(FJsonValueObject(NativeBefore),FJsonValueObject(V));}
    bool Check()
    {
        if(!Active)return true;if(Stopped)return false;if(Populated&&!ResolvedPopulatedRoles())return Stop(TEXT("populated_resolved_roles"));double Checks=0;R.Data->TryGetNumberField(TEXT("hism_current_scope_checks"),Checks);R.Data->SetNumberField(TEXT("hism_current_scope_checks"),Checks+1);if(Populated){if(!Context()||GUndo!=Undo||!Roles())return Stop(TEXT("populated_context_roles"));if(!UserDataCurrent()||!BindingCurrent())return false;}if(!Context()||GUndo!=Undo||!Roles()||!Source())return Stop(TEXT("context_source_roles"));
        const auto& All=SCS->GetAllNodes();if(All.Num()>10000)return Stop(TEXT("current_scs_bound"));TSet<USCS_Node*> Current;for(auto* N:All){if(!N||Current.Contains(N))return Stop(TEXT("current_scs_duplicates"));Current.Add(N);}const bool WasOld=Selected.IsValid()&&Old.ContainsByPredicate([&](const FRole& O){return O.Node.Get()==Selected.Get();});if(All.Num()!=Old.Num()+((Selected.IsValid()&&!WasOld&&Current.Contains(Selected.Get()))?1:0))return Stop(TEXT("current_scs_membership"));
        for(const auto& O:Old){auto* N=O.Node.Get();if(!Loaded(N)||N->GetOuter()!=SCS.Get()||!Current.Contains(N)||N->VariableGuid!=O.Guid||N->GetVariableName()!=O.Name||N->GetPathName()!=O.Path||N->ComponentClass.Get()!=O.Class||N->ComponentTemplate.Get()!=O.Template.Get()||!Loaded(O.Template.Get())||O.Template->GetClass()!=O.Class||O.Template->GetOuter()!=Generated.Get())return Stop(TEXT("old_scs_role"));}
        if(Selected.IsValid()){auto* N=Selected.Get();if(!Loaded(N)||N->GetOuter()!=SCS.Get()||N->GetPathName()!=NodePath||N->VariableGuid!=NodeGuid||N->GetVariableName()!=FName(*Name)||N->ComponentClass.Get()!=Class.Get()||N->ComponentTemplate.Get()!=Template.Get()||!TemplateRole(Template.Get())||Template->GetOuter()!=Generated.Get()||(Attached&&!Current.Contains(N)))return Stop(TEXT("selected_empty_template_role"));}
        if(!UserDataCurrent()||!BindingCurrent())return false;return Context()||Stop(TEXT("after_observation"));
    }
    bool Bind(UBlueprint* B,const FString& SelectedName,USCS_Node* N=nullptr)
    {
        Active=true;Undo=GUndo;Editor.Reset(GEditor);World.Reset(GEditor?GEditor->GetEditorWorldContext().World():nullptr);BP.Reset(B);Name=SelectedName;
        if(!Context()||!Loaded(B)||!Loaded(B->SimpleConstructionScript.Get())||!Loaded(B->GeneratedClass.Get())||!Loaded(B->SkeletonGeneratedClass.Get())||!Loaded(B->ParentClass.Get()))return Stop(TEXT("bind_roles"));
        SCS.Reset(B->SimpleConstructionScript);Generated.Reset(B->GeneratedClass);Skeleton.Reset(B->SkeletonGeneratedClass);Parent.Reset(B->ParentClass);Class.Reset(UHierarchicalInstancedStaticMeshComponent::StaticClass());
        int32 Depth=0;for(UClass* P=Parent.Get();P;P=P->GetSuperClass()){if(++Depth>64||!Loaded(P)||!P->HasAnyClassFlags(CLASS_Native)||P->ClassGeneratedBy)return Stop(TEXT("native_parent_scope"));Holds.Emplace(P);}
        auto CaptureOld=[&]()->bool
        {
            const auto& Nodes=SCS->GetAllNodes();if(Nodes.Num()>10000)return Stop(TEXT("scs_bound"));
            for(auto* P:Nodes)
            {
                const TStrongObjectPtr<USCS_Node> HoldNode(P);if(!Loaded(P)||(Populated&&!P->ComponentTemplate.IsResolved()))return Stop(TEXT("old_scs_unavailable"));
                UActorComponent* T=P->ComponentTemplate.Get();const TStrongObjectPtr<UActorComponent> HoldTemplate(T);if(!Loaded(T))return Stop(TEXT("old_scs_unavailable"));
                auto C=P->ComponentClass;if(Populated&&!C.IsResolved())return Stop(TEXT("old_scs_class_unresolved"));FRole O;O.Node.Reset(P);O.Template.Reset(T);O.Class=C.Get();O.Name=P->GetVariableName();O.Guid=P->VariableGuid;O.Path=P->GetPathName();Old.Add(MoveTemp(O));
            }
            return true;
        };
        if(AllowPopulated&&N)
        {
            const TStrongObjectPtr<USCS_Node> HoldNode(N);if(!Loaded(N)||!N->ComponentTemplate.IsResolved())return Stop(TEXT("initial_selected_slot"));
            UActorComponent* T=N->ComponentTemplate.Get();const TStrongObjectPtr<UActorComponent> HoldTemplate(T);
            auto* H=Cast<UHierarchicalInstancedStaticMeshComponent>(T);Populated=Loaded(H)&&H->GetInstanceCount()>0;
            if(Populated)
            {
                if(!Class->ClassDefaultObject.IsResolved())return Stop(TEXT("native_cdo_unresolved"));ExistingNativeCDO.Reset(Class->ClassDefaultObject.Get());
                if(!Loaded(ExistingNativeCDO.Get())||!CapturePopulated(N)||!CaptureOld())return false;
            }
        }
        NativeBefore=NativeIsmSourceObservation(Class.Get());if(!NativeBefore->GetBoolField(TEXT("native_identity_verified")))return Stop(TEXT("native_cdo_source"));Holds.Emplace(Class->GetDefaultObject(false));
        if(!Populated&&!CaptureOld())return false;
        R.Data->SetBoolField(TEXT("native_hism_source_shared_ism_observer"),true);R.Data->SetNumberField(TEXT("hism_instance_count"),0);R.Data->SetBoolField(TEXT("hism_async_building"),false);
        R.Data->SetStringField(TEXT("hism_scope"),TEXT("Exact own native Engine HISM empty unregistered archetype on a native-parent Actor BP; selected SCS/current class/CDO identities and inactive instance state only. No populated HISM, generic property, tree wait, runtime, global/disk/CDO-content purity claim."));
        if(Populated)
        {
            R.Data->SetBoolField(TEXT("native_populated_hism_mesh_material"),true);R.Data->SetNumberField(TEXT("hism_instance_count"),InstanceMatrices.Num());R.Data->SetObjectField(TEXT("hism_user_data_before"),UserDataReceipt());
            R.Data->SetBoolField(TEXT("hism_tree_fully_built_before"),Template->IsTreeFullyBuilt());R.Data->SetStringField(TEXT("hism_scope"),TEXT("Exact own inactive populated HISM archetype; 1..256 original local instance matrices in order, custom width0..16/flat4096, random seed ranges and source/SCS roles preserved. Render/physics/nav/tree caches are derived observations, not user data or tree identity invariants. No registered/runtime/welded/unknown async path."));return Check();
        }
        return N?Adopt(N,true):Check();
    }
    bool Adopt(USCS_Node* N,bool IsAttached)
    {if(!Context()||!Roles()||!Loaded(N)||!TemplateRole(N->ComponentTemplate.Get()))return Stop(TEXT("returned_node"));Selected.Reset(N);Template.Reset(Cast<UHierarchicalInstancedStaticMeshComponent>(N->ComponentTemplate.Get()));NodePath=N->GetPathName();NodeGuid=N->VariableGuid;Attached=IsAttached;return Check();}
    bool Attach(){Attached=true;return Check();}
    bool Reacquire()
    {
        if(!Active)return true;if(Stopped)return false;if(!Context()||GUndo!=Undo||!Loaded(BP.Get())||BP->SimpleConstructionScript.Get()!=SCS.Get()||BP->ParentClass.Get()!=Parent.Get()||!Source())return Stop(TEXT("compiler_source_context"));
        UClass* G=BP->GeneratedClass.Get();UClass* K=BP->SkeletonGeneratedClass.Get();if(!Loaded(G)||!Loaded(K)||G->ClassGeneratedBy!=BP.Get()||K->ClassGeneratedBy!=BP.Get()||G->GetPackage()!=BP->GetPackage()||K->GetPackage()!=BP->GetPackage()||G->GetSuperClass()!=Parent.Get()||K->GetSuperClass()!=Parent.Get())return Stop(TEXT("compiler_class_roles"));
        Holds.Emplace(Generated.Get());Holds.Emplace(Skeleton.Get());Generated.Reset(G);Skeleton.Reset(K);
        for(auto& O:Old){auto* N=O.Node.Get();if(!Loaded(N)||N->ComponentClass.Get()!=O.Class||!Loaded(N->ComponentTemplate.Get())||N->ComponentTemplate->GetClass()!=O.Class||N->ComponentTemplate->GetOuter()!=G)return Stop(TEXT("compiler_old_template_roles"));Holds.Emplace(O.Template.Get());O.Template.Reset(N->ComponentTemplate);}
        if(Selected.IsValid()){auto* N=Selected.Get();if(!Loaded(N)||N->GetPathName()!=NodePath||N->VariableGuid!=NodeGuid||!TemplateRole(N->ComponentTemplate.Get())||N->ComponentTemplate->GetOuter()!=G)return Stop(TEXT("compiler_selected_template"));Holds.Emplace(Template.Get());Template.Reset(Cast<UHierarchicalInstancedStaticMeshComponent>(N->ComponentTemplate.Get()));}
        return Check();
    }
};

// KN: the old Asset/FBool route is a raw store, not SetAsset or an editor
// property notification. Keep that distinction inside this finite template path.
struct FNiagaraTemplateScope
{
    struct FRole
    {
        TStrongObjectPtr<UObject> Hold; UClass* Class = nullptr; UObject* Outer = nullptr; UPackage* Package = nullptr;
        FString Path; EObjectFlags Flags = RF_NoFlags; EClassFlags NativeFlags = CLASS_None; UClass* Super = nullptr; UObject* CDO = nullptr;
    };
    struct FNodeRole
    {
        TStrongObjectPtr<USCS_Node> Node; TStrongObjectPtr<UActorComponent> Template;
        FString Name, Fields; FGuid Guid; UClass* Class = nullptr; TArray<USCS_Node*> Children;
    };
    struct FSystemRole
    {
        TStrongObjectPtr<UNiagaraSystem> System; TArray<FRole> Roles; TArray<FString> Fields;
        TArray<UObject*> Nested; FString File, Hash, ParameterState; int64 Bytes = -1;
    };
    FMutationResult& R; bool Stopped = false, Attached = false, Adding = false, FieldsReady = false;
    TStrongObjectPtr<UBlueprint> BP; TStrongObjectPtr<USimpleConstructionScript> SCS;
    TStrongObjectPtr<UClass> Generated, Skeleton; TStrongObjectPtr<USCS_Node> Selected;
    TStrongObjectPtr<UNiagaraComponent> Component, Default;
    TStrongObjectPtr<UWorld> World; TStrongObjectPtr<UEditorEngine> Editor;
    TArray<FRole> Fixed; TArray<FNodeRole> Old; TArray<FSystemRole> Systems;
    TArray<USCS_Node*> Roots; TStrongObjectPtr<USCS_Node> Parent; TArray<FString> OtherDirtyBefore;
    TMap<FName, FProperty*> Schema; TMap<FName, uint64> SchemaFlags;
    FString Name, SelectedFields, NativeFields, NativeStoreState, SelectedStoreState, SelectedPath, TemplatePath, TargetFile, TargetHash; int64 TargetBytes = -1;
    FGuid SelectedGuid; EObjectFlags SelectedFlags = RF_NoFlags, TemplateFlags = RF_NoFlags;
    UNiagaraSystem* ExpectedAsset = nullptr; bool ExpectedAuto = true, ExpectedVisible = true, ExpectedHidden = false;
    decltype(GUndo) Undo = nullptr; TArray<TSharedPtr<FJsonValue>> Applied;
    // KQ is only the nonidentity fresh Add path; the KN raw Set/identity path is unchanged.
    struct FAddTransformField { FStructProperty* Field=nullptr; uint64 Flags=0; int32 Offset=0,Size=0; UScriptStruct* Type=nullptr; };
    bool AddTransforms=false, TransformReady=false, RotationCanonicalPending=false;
    TStrongObjectPtr<UScriptStruct> TransformVector,TransformRotator; EStructFlags VectorFlags=STRUCT_NoFlags,RotatorFlags=STRUCT_NoFlags;
    TMap<FName,FAddTransformField> TransformFields;
    FBoolProperty* TransformUpdatedField=nullptr;uint64 TransformUpdatedFlags=0;int32 TransformUpdatedOffset=0,TransformUpdatedSize=0;
    uint8 TransformUpdatedByteOffset=0,TransformUpdatedByteMask=0,TransformUpdatedFieldMask=0;bool TransformUpdatedExpected=false;
    FVector TransformInput[3],ExpectedLocation=FVector::ZeroVector,ExpectedScale=FVector::OneVector; FRotator ExpectedRotation=FRotator::ZeroRotator;
    TArray<TSharedPtr<FJsonValue>> TransformRequests,TransformApplied;
    explicit FNiagaraTemplateScope(FMutationResult& In) : R(In) {}
    static bool Loaded(const UObject* O)
    { return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
    bool Stop(const TCHAR* Code, const TCHAR* Message)
    {
        if (Stopped) return false;
        Stopped = true; R.bChangeStateKnown = false; R.Data->SetBoolField(TEXT("niagara_followup_sdk_stopped"), true);
        R.Data->SetArrayField(TEXT("niagara_raw_applied_prefix"), Applied);
        if(AddTransforms)R.Data->SetArrayField(TEXT("niagara_transform_applied_prefix"),TransformApplied);
        R.Data->SetStringField(TEXT("niagara_failed_boundary_code"), Code); R.Data->SetStringField(TEXT("niagara_failed_boundary_error"), Message);
        if (R.ErrorCode.IsEmpty()) return Fail(R, Code, Message); return false;
    }
    bool Context()
    {
        if (Stopped) return false;
        if (!IsEditorGameThread() || GExitPurge || IsEngineExitRequested() || IsGarbageCollecting() || UE::IsSavingPackage()
            || IsPlaySessionActive() || !Loaded(Editor.Get()) || GEditor != Editor.Get() || !Loaded(World.Get()) || World->bIsTearingDown
            || GEditor->GetEditorWorldContext().World() != World.Get() || GUndo != Undo)
            return Stop(TEXT("component_niagara_context_changed"), TEXT("Captured idle Editor/world/transaction changed; no follow-up writer/read/retry."));
        return true;
    }
    bool Pin(UObject* O, TArray<FRole>& Roles)
    {
        if (!Context() || !Loaded(O) || !Loaded(O->GetClass()) || !Loaded(O->GetPackage()) || (O->GetOuter() && !Loaded(O->GetOuter())))
            return Stop(TEXT("component_niagara_role_unreadable"), TEXT("A retained native/source/target role is unreadable."));
        FRole A; A.Hold.Reset(O); A.Class = O->GetClass(); A.Outer = O->GetOuter(); A.Package = O->GetPackage(); A.Path = O->GetPathName(); A.Flags = O->GetFlags();
        if (const auto* K = Cast<UClass>(O)) { A.NativeFlags = K->GetClassFlags(); A.Super = K->GetSuperClass(); A.CDO = K->GetDefaultObject(false); }
        Roles.Add(MoveTemp(A)); return true;
    }
    bool Exact(const FRole& A) const
    { auto* O = A.Hold.Get(); return Loaded(O) && O->GetClass() == A.Class && O->GetOuter() == A.Outer && O->GetPackage() == A.Package && O->GetPathName() == A.Path && O->GetFlags() == A.Flags
        && (!Cast<UClass>(O) || (Cast<UClass>(O)->GetClassFlags() == A.NativeFlags && Cast<UClass>(O)->GetSuperClass() == A.Super && Cast<UClass>(O)->GetDefaultObject(false) == A.CDO)); }
    static bool Disk(const FString& File, int64& Bytes, FString& Hash)
    {
        Bytes = IFileManager::Get().FileSize(*File); Hash.Reset(); if (Bytes < 0) return true;
        if (Bytes > 67108864) return false; TArray<uint8> Data; if (!FFileHelper::LoadFileToArray(Data, *File) || Data.Num() != Bytes) return false;
        FSHAHash H; FSHA1::HashBuffer(Data.GetData(), Data.Num(), H.Hash); Hash = H.ToString(); return true;
    }
    static bool File(const UObject* O, FString& Out)
    { return FPackageName::TryConvertLongPackageNameToFilename(O->GetPackage()->GetName(), Out, FPackageName::GetAssetPackageExtension()); }
    bool Basic()
    {
        if (!Context()) return false;
        for (const auto& A : Fixed) if (!Exact(A)) return Stop(TEXT("component_niagara_fixed_role_changed"), TEXT("Captured native/source/Blueprint/SCS identity changed."));
        if (!Loaded(BP.Get()) || BP->ParentClass != AActor::StaticClass() || BP->SimpleConstructionScript != SCS.Get()
            || BP->GeneratedClass != Generated.Get() || BP->SkeletonGeneratedClass != Skeleton.Get()
            || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bIsRegeneratingOnLoad || BP->bSuppressStructurallyModified
            || BP->Status == BS_BeingCreated || !Loaded(Generated.Get()) || !Loaded(Skeleton.Get()))
            return Stop(TEXT("component_niagara_current_roles_changed"), TEXT("Current normal native Actor Blueprint/SCS/class roles changed outside an explicit compiler return."));
        return true;
    }
    bool FieldRoleCurrent(const FRole& Observed)
    {
        if (!Basic() || !Exact(Observed)) return Stop(TEXT("component_niagara_export_role_changed"),TEXT("The held exporter identity/class/outer/package/path/flags changed; no next property/element export."));
        auto* O=Observed.Hold.Get();
        if (O==Component.Get())
        {
            if(!Loaded(Selected.Get()) || Selected->GetOuter()!=SCS.Get() || Selected->ComponentTemplate.Get()!=O || O->GetOuter()!=Generated.Get()
                || Selected->VariableGuid!=SelectedGuid || Selected->GetVariableName().ToString()!=Name || Selected->ComponentClass!=UNiagaraComponent::StaticClass()
                || Selected->GetPathName()!=SelectedPath || Selected->GetFlags()!=SelectedFlags || SCS->GetAllNodes().Contains(Selected.Get())!=Attached
                || (Adding && !Selected->GetChildNodes().IsEmpty()))
                return Stop(TEXT("component_niagara_export_selected_changed"),TEXT("Selected exporter left its current own declaration/membership/template role."));
        }
        for(const auto& Item:Old) if(Item.Template.Get()==O)
        {
            TArray<USCS_Node*> Children=Item.Children; if(Adding && Attached && Parent.Get()==Item.Node.Get())Children.Add(Selected.Get());
            if(!Loaded(Item.Node.Get()) || Item.Node->GetOuter()!=SCS.Get() || Item.Node->ComponentTemplate.Get()!=O || !SCS->GetAllNodes().Contains(Item.Node.Get())
                || Item.Node->VariableGuid!=Item.Guid || Item.Node->GetVariableName().ToString()!=Item.Name || Item.Node->ComponentClass!=Item.Class || Item.Node->GetChildNodes()!=Children)
                return Stop(TEXT("component_niagara_export_old_changed"),TEXT("Old exporter left its current declaration/membership/child/template role."));
        }
        for(const auto& Source:Systems) if(Source.Roles.ContainsByPredicate([&](const FRole& Role){return Role.Hold.Get()==O;}))
            for(const auto& Role:Source.Roles) if(!Exact(Role)) return Stop(TEXT("component_niagara_export_source_changed"),TEXT("Readonly exporter source roles changed."));
        return true;
    }
    static bool ReadableRawStorage(const UObject* O, const FProperty* P, UEnum* ObservedEnum)
    {
        if (!P->HasSetterOrGetter()) return true;
        // KN3 observed this exact native byte enum with accessor wrappers.
        // ExportText_Direct(Data == Delta) reads its backing storage, never
        // GetLightmapType's world-dependent effective value or any setter.
        const auto* Enum = CastField<FEnumProperty>(P);
        const auto* Owner = UPrimitiveComponent::StaticClass();
        const FNumericProperty* Underlying = Enum ? Enum->GetUnderlyingProperty() : nullptr;
        const int32 Offset = P->GetOffset_ForInternal();
        return Loaded(O) && O->IsA(Owner) && P->GetOwnerClass() == Owner
            && P == FindFProperty<FProperty>(Owner, TEXT("LightmapType"))
            && P->GetPathName() == TEXT("/Script/Engine.PrimitiveComponent:LightmapType")
            && P->ArrayDim == 1 && P->ElementSize == 1 && P->HasGetter() && P->HasSetter()
            && static_cast<uint64>(P->GetPropertyFlags()) == 6759867280785941ULL
            && Enum && Loaded(ObservedEnum) && ObservedEnum == FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ELightmapType"))
            && Underlying && CastField<FByteProperty>(Underlying) && Underlying->ArrayDim == 1
            && Underlying->ElementSize == 1 && !Underlying->HasSetterOrGetter()
            && Offset >= 0 && static_cast<int64>(Offset) + P->GetSize() <= Owner->GetPropertiesSize()
            && Owner->GetPropertiesSize() <= O->GetClass()->GetPropertiesSize();
    }
    bool Fields(UObject* O, FString& Out, bool SkipSelected = false)
    {
        if (!Basic() || !Loaded(O)) return false;
        TArray<FRole> ExportRoles; if(!Pin(O,ExportRoles)) return false;
        const TStrongObjectPtr<UClass> KeepClass(O->GetClass()); const TStrongObjectPtr<UObject> KeepOuter(O->GetOuter()); const TStrongObjectPtr<UPackage> KeepPackage(O->GetPackage());
        if(!FieldRoleCurrent(ExportRoles[0]))return false; Out.Reset(); int32 Count = 0;
        for (TFieldIterator<FProperty> It(O->GetClass()); It; ++It)
        {
            const FProperty* P = *It;
            if (SkipSelected && (Schema.Contains(P->GetFName()) || (AddTransforms && (TransformFields.Contains(P->GetFName()) || P==TransformUpdatedField)))) continue;
            UEnum* ObservedEnum = nullptr; TStrongObjectPtr<UEnum> KeepEnum;
            if (Count < 2048 && P->ArrayDim == 1 && P->ElementSize == 1
                && P->HasGetter() && P->HasSetter() && P->GetOwnerClass() == UPrimitiveComponent::StaticClass()
                && P == FindFProperty<FProperty>(UPrimitiveComponent::StaticClass(), TEXT("LightmapType"))
                && P->GetPathName() == TEXT("/Script/Engine.PrimitiveComponent:LightmapType")
                && static_cast<uint64>(P->GetPropertyFlags()) == 6759867280785941ULL)
            if (const auto* EnumField = CastField<FEnumProperty>(P))
            {
                const FName FieldName = P->GetFName();
                ObservedEnum = EnumField->GetEnum();
                if (!FieldRoleCurrent(ExportRoles[0])) return false;
                if (FindFProperty<FProperty>(O->GetClass(), FieldName) != P)
                    return Stop(TEXT("component_niagara_export_schema_changed"), TEXT("Tracked enum read left its current reflected field; no follow-up export or retry."));
                if (Loaded(ObservedEnum)) KeepEnum.Reset(ObservedEnum);
            }
            const bool StorageReadable = ReadableRawStorage(O, P, ObservedEnum);
            if (!FieldRoleCurrent(ExportRoles[0])) return false;
            if (++Count > 2048 || P->ArrayDim < 1 || P->ArrayDim > 64 || !StorageReadable)
            {
                auto First = MakeShared<FJsonObject>();
                First->SetStringField(TEXT("object"), O->GetPathName());
                First->SetStringField(TEXT("object_class"), O->GetClass()->GetPathName());
                First->SetStringField(TEXT("property"), P->GetPathName());
                First->SetStringField(TEXT("property_type"), P->GetClass()->GetName());
                First->SetNumberField(TEXT("ordinal"), Count); First->SetNumberField(TEXT("array_dim"), P->ArrayDim);
                First->SetNumberField(TEXT("offset"), P->GetOffset_ForInternal()); First->SetNumberField(TEXT("element_size"), P->ElementSize);
                First->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
                First->SetBoolField(TEXT("has_getter"), P->HasGetter()); First->SetBoolField(TEXT("has_setter"), P->HasSetter());
                First->SetStringField(TEXT("reason"), Count > 2048 ? TEXT("field_count_bound") : (P->ArrayDim < 1 || P->ArrayDim > 64) ? TEXT("static_array_bound") : TEXT("custom_accessor"));
                R.Data->SetObjectField(TEXT("niagara_first_unobservable_field"), First);
                return Stop(TEXT("component_niagara_fields_unobservable"), TEXT("Bounded reflected fields require direct storage; only the observed exact native LightmapType accessor field is a readonly storage exception. Inspect niagara_first_unobservable_field."));
            }
            Out += P->GetPathName() + TEXT(":") + LexToString(static_cast<uint64>(P->GetPropertyFlags())) + TEXT("=");
            for (int32 Index = 0; Index < P->ArrayDim; ++Index)
            {
                FString V; const void* Data = P->ContainerPtrToValuePtr<void>(O, Index);
                const bool Exported = P->ExportText_Direct(V, Data, Data, O, PPF_IncludeTransient);
                if (!FieldRoleCurrent(ExportRoles[0])) return false;
                if (!Exported || Out.Len() + V.Len() > 1048576) return Stop(TEXT("component_niagara_fields_unobservable"), TEXT("Unconditional reflected export exceeded its observation bound."));
                Out += V + TEXT("\n");
            }
        }
        return true;
    }
    static FString ParameterState(const FNiagaraParameterStore& Store)
    { return FString::Printf(TEXT("%u:%d:%d:%d:%d"), Store.GetLayoutVersion(), Store.GetParametersDirty(), Store.GetInterfacesDirty(), Store.GetUObjectsDirty(), Store.GetPositionDataDirty()); }
    FString StoreState(const UNiagaraComponent* C) const { return ParameterState(C->GetOverrideParameters()); }
    bool EmptyStore(UNiagaraComponent* C)
    {
        if (!Loaded(C) || C->GetClass() != UNiagaraComponent::StaticClass() || C->IsRegistered() || C->IsPreRegistering()
            || C->IsRenderStateCreated() || C->IsPhysicsStateCreated() || C->GetOwner() || C->GetWorld() || C->HasBegunPlay()
            || C->GetSystemInstanceController().IsValid() || C->IsActive() || C->OnSystemInstanceChanged().IsBound() || C->OnSynchronizedWithAssetParameters().IsBound()) return false;
        const FNiagaraParameterStore& Store = static_cast<const UNiagaraComponent*>(C)->GetOverrideParameters();
        if (Store.GetOwner() != C || !Store.IsEmpty() || !Store.ReadParameterVariables().IsEmpty() || !Store.GetParameterDataArray().IsEmpty()
            || !Store.GetDataInterfaces().IsEmpty() || !Store.GetUObjects().IsEmpty() || !C->GetOverrideParameters().GetSourceParameterStores().IsEmpty()) return false;
        for (const TCHAR* Field : {TEXT("TemplateParameterOverrides"), TEXT("InstanceParameterOverrides"), TEXT("TemplateParameterOverridesCache"), TEXT("InstanceParameterOverridesCache")})
        {
            const FMapProperty* P = FindFProperty<FMapProperty>(C->GetClass(), Field);
            if (!P || P->GetOwnerClass() != UNiagaraComponent::StaticClass() || P->ArrayDim != 1 || P->HasSetterOrGetter()
                || FScriptMapHelper(P, P->ContainerPtrToValuePtr<void>(C)).Num() != 0) return false;
        }
        return true;
    }
    static TSharedRef<FJsonValue> TransformVectorJson(const FVector& Value)
    { return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Value.X),MakeShared<FJsonValueNumber>(Value.Y),MakeShared<FJsonValueNumber>(Value.Z)}); }
    bool TransformTemplate(UNiagaraComponent* C)
    {
        // public setters can update bounds/overlaps. Do not dispatch them into runtime attachment, movement or collision state.
        return EmptyStore(C)&&!C->HasAnyFlags(RF_NeedInitialization)&&!C->GetAsset()&&!C->bAutoManageAttachment&&C->AutoAttachParent.IsExplicitlyNull()
            &&!C->GetAttachParent()&&C->GetAttachChildren().IsEmpty()&&C->GetAttachSocketName().IsNone()
            &&!C->IsDeferringMovementUpdates()&&!C->HasGroupedMovementUpdates()&&!C->GetCurrentScopedMovement()
            &&C->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&!C->GetGenerateOverlapEvents()&&!C->TransformUpdated.IsBound();
    }
    bool BindAddTransforms()
    {
        if(!AddTransforms)return true;
        TransformVector.Reset(FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.Vector")));TransformRotator.Reset(FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.Rotator")));
        if(!Loaded(TransformVector.Get())||!Loaded(TransformRotator.Get())||TransformVector.Get()!=TBaseStructure<FVector>::Get()||TransformRotator.Get()!=TBaseStructure<FRotator>::Get()
            ||TransformVector->GetStructureSize()!=static_cast<int32>(sizeof(FVector))||TransformRotator->GetStructureSize()!=static_cast<int32>(sizeof(FRotator))||!TransformTemplate(Default.Get()))
            return Fail(R,TEXT("component_niagara_transform_source_rejected"),TEXT("Loaded exact native Vector/Rotator and an empty no-collision/no-attachment native Niagara CDO required; no setter dispatched."));
        VectorFlags=TransformVector->StructFlags;RotatorFlags=TransformRotator->StructFlags;
        if(!Pin(TransformVector.Get(),Fixed)||!Pin(TransformRotator.Get(),Fixed))return false;
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(const TCHAR* FieldKey:{TEXT("RelativeLocation"),TEXT("RelativeRotation"),TEXT("RelativeScale3D")})
        {
            auto* P=CastField<FStructProperty>(FindProperty(Default->GetClass(),FieldKey));UScriptStruct* ValueType=FString(FieldKey)==TEXT("RelativeRotation")?TransformRotator.Get():TransformVector.Get();
            if(!P||P->GetClass()!=FStructProperty::StaticClass()||P->GetOwnerClass()!=USceneComponent::StaticClass()||P->GetName()!=FieldKey||P->Struct!=ValueType
                ||P->ArrayDim!=1||P->HasSetterOrGetter()||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly)||!P->HasAllPropertyFlags(CPF_Edit)||P->GetSize()!=ValueType->GetStructureSize())
                return Fail(R,TEXT("component_niagara_transform_schema_rejected"),TEXT("Exact direct native Scene Vector/Rotator transform descriptor required; no custom accessor or schema repair."));
            FAddTransformField A;A.Field=P;A.Flags=static_cast<uint64>(P->GetPropertyFlags());A.Offset=P->GetOffset_ForInternal();A.Size=P->GetSize();A.Type=ValueType;TransformFields.Add(FName(FieldKey),A);
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property"),P->GetPathName());Row->SetStringField(TEXT("flags"),LexToString(A.Flags));Row->SetStringField(TEXT("value_struct"),ValueType->GetPathName());Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        auto* State=CastField<FBoolProperty>(FindProperty(Default->GetClass(),TEXT("bComponentToWorldUpdated")));
        if(!State||State->GetClass()!=FBoolProperty::StaticClass()||State->GetOwnerClass()!=USceneComponent::StaticClass()||State->GetName()!=TEXT("bComponentToWorldUpdated")
            ||State->ArrayDim!=1||State->HasSetterOrGetter()||!State->HasAllPropertyFlags(CPF_Transient)||State->HasAnyPropertyFlags(CPF_Deprecated|CPF_EditorOnly))
            return Fail(R,TEXT("component_niagara_transform_state_schema_rejected"),TEXT("Exact native transient ComponentToWorld update bool required; no broad cache exclusion."));
        TransformUpdatedField=State;TransformUpdatedFlags=static_cast<uint64>(State->GetPropertyFlags());TransformUpdatedOffset=State->GetOffset_ForInternal();TransformUpdatedSize=State->GetSize();
        TransformUpdatedByteOffset=State->GetByteOffset();TransformUpdatedByteMask=State->GetByteMask();TransformUpdatedFieldMask=State->GetFieldMask();
        auto StateRow=MakeShared<FJsonObject>();StateRow->SetStringField(TEXT("property"),State->GetPathName());StateRow->SetStringField(TEXT("flags"),LexToString(TransformUpdatedFlags));StateRow->SetStringField(TEXT("scope"),TEXT("Selected Add only: first public location setter plans native false-to-true or true-to-true; no bool store."));Rows.Add(MakeShared<FJsonValueObject>(StateRow));
        R.Data->SetArrayField(TEXT("niagara_transform_schema"),Rows);return true;
    }
    bool AddTransformCurrent()
    {
        if(!AddTransforms)return true;
        if(TransformFields.Num()!=3||!Loaded(TransformVector.Get())||!Loaded(TransformRotator.Get())||TransformVector->StructFlags!=VectorFlags||TransformRotator->StructFlags!=RotatorFlags
            ||TransformVector->GetStructureSize()!=static_cast<int32>(sizeof(FVector))||TransformRotator->GetStructureSize()!=static_cast<int32>(sizeof(FRotator))||!TransformTemplate(Default.Get()))
            return Stop(TEXT("component_niagara_transform_source_changed"),TEXT("Captured native transform source or no-collision template role changed; no next setter."));
        for(const auto& Pair:TransformFields)
        {
            const auto& A=Pair.Value;auto* P=FindProperty(Default->GetClass(),Pair.Key.ToString());
            if(P!=A.Field||P->GetClass()!=FStructProperty::StaticClass()||P->GetOwnerClass()!=USceneComponent::StaticClass()||P->ArrayDim!=1||P->HasSetterOrGetter()
                ||static_cast<uint64>(P->GetPropertyFlags())!=A.Flags||P->GetOffset_ForInternal()!=A.Offset||P->GetSize()!=A.Size||A.Field->Struct!=A.Type)
                return Stop(TEXT("component_niagara_transform_schema_changed"),TEXT("Current transform descriptor left its captured native owner/type/layout; no stale access."));
        }
        auto* State=FindProperty(Default->GetClass(),TEXT("bComponentToWorldUpdated"));
        if(State!=TransformUpdatedField||!State||State->GetClass()!=FBoolProperty::StaticClass()||State->GetOwnerClass()!=USceneComponent::StaticClass()||State->ArrayDim!=1||State->HasSetterOrGetter()
            ||static_cast<uint64>(State->GetPropertyFlags())!=TransformUpdatedFlags||State->GetOffset_ForInternal()!=TransformUpdatedOffset||State->GetSize()!=TransformUpdatedSize
            ||TransformUpdatedField->GetByteOffset()!=TransformUpdatedByteOffset||TransformUpdatedField->GetByteMask()!=TransformUpdatedByteMask||TransformUpdatedField->GetFieldMask()!=TransformUpdatedFieldMask)
            return Stop(TEXT("component_niagara_transform_state_schema_changed"),TEXT("Current exact ComponentToWorld bool owner/flags/layout changed; no stale state read."));
        if(!TransformReady)return true;
        if(!TransformTemplate(Component.Get()))return Stop(TEXT("component_niagara_transform_role_changed"),TEXT("Selected setter target is no longer an empty unregistered/noWorld/no-collision/no-attachment template."));
        const bool ActualUpdated=TransformUpdatedField->GetPropertyValue_InContainer(Component.Get());
        if(!TransformApplied.IsEmpty())
        {
            const auto Last=TransformApplied.Last()->AsObject();bool Returned=false,Observed=false;
            if(Last->TryGetBoolField(TEXT("returned"),Returned)&&Returned&&Last->TryGetBoolField(TEXT("component_to_world_updated_actual_observed"),Observed)&&!Observed)
            {Last->SetBoolField(TEXT("component_to_world_updated_actual_observed"),true);Last->SetBoolField(TEXT("component_to_world_updated_after"),ActualUpdated);}
        }
        if(ActualUpdated!=TransformUpdatedExpected)return Stop(TEXT("component_niagara_transform_state_changed"),TEXT("Native ComponentToWorld update flag differs from the exact selected setter stage; actual observed prefix retained."));
        const FVector ActualLocation=Component->GetRelativeLocation(),ActualScale=Component->GetRelativeScale3D();const FRotator ActualRotation=Component->GetRelativeRotation();
        const bool RotationMatches=RotationCanonicalPending?ActualRotation.Quaternion().Equals(ExpectedRotation.Quaternion(),1.e-6)
            :(ActualRotation.Pitch==ExpectedRotation.Pitch&&ActualRotation.Yaw==ExpectedRotation.Yaw&&ActualRotation.Roll==ExpectedRotation.Roll);
        if(!ActualLocation.Equals(ExpectedLocation,0.)||!ActualScale.Equals(ExpectedScale,0.)||!RotationMatches)
            return Stop(TEXT("component_niagara_transform_value_changed"),TEXT("Current selected transform differs from its planned setter prefix; raw rotation is frozen after the one native canonicalization boundary."));
        return true;
    }
    bool ApplyAddTransforms()
    {
        if(!AddTransforms)return true;if(!Check())return false;
        for(int32 OperationIndex=0;OperationIndex<3;++OperationIndex)
        {
            const TCHAR* FieldKey=OperationIndex==0?TEXT("RelativeLocation"):OperationIndex==1?TEXT("RelativeRotation"):TEXT("RelativeScale3D");
            const FVector BeforeValue=OperationIndex==0?ExpectedLocation:OperationIndex==1?FVector(ExpectedRotation.Pitch,ExpectedRotation.Yaw,ExpectedRotation.Roll):ExpectedScale;
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property_name"),FieldKey);Row->SetField(TEXT("before"),TransformVectorJson(BeforeValue));Row->SetField(TEXT("requested"),TransformRequests[OperationIndex]);Row->SetField(TEXT("legacy_float32_input"),TransformVectorJson(TransformInput[OperationIndex]));Row->SetBoolField(TEXT("attempted"),true);Row->SetBoolField(TEXT("returned"),false);Row->SetBoolField(TEXT("verified"),false);
            Row->SetBoolField(TEXT("component_to_world_updated_before"),TransformUpdatedExpected);Row->SetBoolField(TEXT("component_to_world_updated_expected_after"),true);Row->SetBoolField(TEXT("component_to_world_updated_actual_observed"),false);Row->SetField(TEXT("component_to_world_updated_after"),MakeShared<FJsonValueNull>());
            if(OperationIndex==0)TransformUpdatedExpected=true; // public ConditionalUpdateComponentToWorld, never a raw bool write.
            Row->SetStringField(TEXT("engine_api"),OperationIndex==0?TEXT("SetRelativeLocation"):OperationIndex==1?TEXT("SetRelativeRotation"):TEXT("SetRelativeScale3D"));Row->SetStringField(TEXT("return_kind"),TEXT("void"));
            TransformApplied.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("niagara_transform_applied_prefix"),TransformApplied);R.Stage=TEXT("niagara_transform_")+FString(FieldKey);
            if(OperationIndex==0){ExpectedLocation=TransformInput[0];Component->SetRelativeLocation(ExpectedLocation,false,nullptr,ETeleportType::None);}
            else if(OperationIndex==1){ExpectedRotation=FRotator(TransformInput[1].X,TransformInput[1].Y,TransformInput[1].Z);RotationCanonicalPending=true;Component->SetRelativeRotation(ExpectedRotation,false,nullptr,ETeleportType::None);}
            else{ExpectedScale=TransformInput[2];Component->SetRelativeScale3D(ExpectedScale);}
            Row->SetBoolField(TEXT("returned"),true);if(!Check())return false;
            if(OperationIndex==1){ExpectedRotation=Component->GetRelativeRotation();RotationCanonicalPending=false;}
            const FVector ActualValue=OperationIndex==0?Component->GetRelativeLocation():OperationIndex==1?FVector(ExpectedRotation.Pitch,ExpectedRotation.Yaw,ExpectedRotation.Roll):Component->GetRelativeScale3D();
            // Inline native value copies/JSON receipts below the SDK-return Check need no repeated disk/source scan.
            Row->SetField(TEXT("after"),TransformVectorJson(ActualValue));Row->SetBoolField(TEXT("verified"),true);
        }
        R.Stage=TEXT("mutation");return true;
    }
    bool SourceSystem(UNiagaraSystem* A)
    {
        if (!A) return true; for (const auto& S : Systems) if (S.System.Get() == A) return true;
        if(!Basic() || !Loaded(A)) return false;
        TArray<FRole> Initial; if(!Pin(A,Initial))return false;
        const TStrongObjectPtr<UClass> KeepClass(A->GetClass()); const TStrongObjectPtr<UObject> KeepOuter(A->GetOuter()); const TStrongObjectPtr<UPackage> KeepPackage(A->GetPackage());
        auto SourceCurrent=[&](){return FieldRoleCurrent(Initial[0]);};
        if(A->GetClass()!=UNiagaraSystem::StaticClass()) return Fail(R,TEXT("component_niagara_asset_rejected"),TEXT("An exact native NiagaraSystem is required."));
        const bool IsAsset=A->IsAsset(); if(!SourceCurrent())return false;
        if (!IsAsset || A->GetOuter() != A->GetPackage()
            || A->GetPackage() == BP->GetPackage() || A->GetPackage()->IsDirty() || A->HasAnyFlags(RF_Transient)
            || A->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
            return Fail(R, TEXT("component_niagara_asset_rejected"), TEXT("Asset must already be a loaded exact saved-clean runtime native NiagaraSystem outside the target package."));
        if (!A->GetEmitterHandles().IsEmpty()) return Fail(R,TEXT("component_niagara_system_state_pending"),TEXT("This raw-reference slice requires an empty System; populated emitter sources remain pending."));
        const bool Pending=A->HasOutstandingCompilationRequests(true); if(!SourceCurrent())return false;
        if (!A->bFullyLoaded || Pending || A->GetActiveInstancesCount() != 0)
            return Fail(R, TEXT("component_niagara_system_state_pending"), TEXT("This raw-reference slice requires an already fully-loaded empty System with no outstanding VM/GPU compilation or active instances; no ready/load/wait is invoked."));
        const auto& Store = A->GetExposedParameters();
        if (!Store.IsEmpty() || !Store.ReadParameterVariables().IsEmpty() || !Store.GetParameterDataArray().IsEmpty() || !Store.GetDataInterfaces().IsEmpty() || !Store.GetUObjects().IsEmpty())
            return Fail(R, TEXT("component_niagara_system_parameters_pending"), TEXT("Populated exposed parameters/DI/object sources require a later lifecycle contract."));
        FSystemRole S; S.System.Reset(A); S.ParameterState=ParameterState(Store);
        if (!Pin(A, S.Roles) || !Pin(A->GetClass(), S.Roles) || !Pin(A->GetPackage(), S.Roles) || !Pin(A->GetClass()->GetDefaultObject(false), S.Roles)) return false;
        GetObjectsWithOuter(A, S.Nested, EGetObjectsFlags::IncludeNestedObjects);
        if (S.Nested.Num() > 128) return Fail(R, TEXT("component_niagara_system_bound"), TEXT("Source subtree exceeds the finite read bound."));
        for (auto* O : S.Nested) if (!Pin(O, S.Roles)) return false;
        if (!File(A, S.File) || !Disk(S.File, S.Bytes, S.Hash) || S.Bytes <= 0) return Fail(R, TEXT("component_niagara_system_unsaved"), TEXT("A bounded nonempty saved source file is required."));
        Systems.Add(MoveTemp(S)); auto& Captured=Systems.Last(); Captured.Fields.SetNum(Captured.Roles.Num());
        for (int32 Index = 0; Index < Captured.Roles.Num(); ++Index) if (Index != 1 && Index != 2 && !Fields(Captured.Roles[Index].Hold.Get(), Captured.Fields[Index])) return false;
        auto Audit = ReadPackageDependencies({A->GetPackage()->GetName()}, true, 4096); if (!Check()) return false;
        bool Passed = false; if (!Audit->TryGetBoolField(TEXT("passed"), Passed) || !Passed) return Fail(R, TEXT("component_niagara_dependencies_rejected"), TEXT("Loaded source runtime dependency audit failed."));
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), A->GetPathName()); Row->SetBoolField(TEXT("fully_loaded"), true);
        Row->SetBoolField(TEXT("outstanding_compilation"), false); Row->SetNumberField(TEXT("emitters"), 0); Row->SetNumberField(TEXT("exposed_parameters"), 0);
        Row->SetStringField(TEXT("disk_sha1"), Systems.Last().Hash); Row->SetNumberField(TEXT("disk_bytes"), Systems.Last().Bytes); Row->SetObjectField(TEXT("dependencies"), Audit);
        R.Data->SetObjectField(TEXT("niagara_system_source_") + FString::FromInt(Systems.Num()), Row); return true;
    }
    bool Check()
    {
        if (!Basic() || !EmptyStore(Default.Get()) || StoreState(Default.Get()) != NativeStoreState) return Stop(TEXT("component_niagara_native_source_changed"), TEXT("Exact native CDO must remain empty, unregistered and without instance/consumer state."));
        TArray<FString> OtherDirty=DirtyPackages(); OtherDirty.Remove(BP->GetPackage()->GetName()); if (OtherDirty!=OtherDirtyBefore || Default->GetClass()->GetPackage()->IsDirty()) return Stop(TEXT("component_niagara_external_dirty_changed"),TEXT("Other-package dirty state or native source package changed."));
        FString V; if (!Fields(Default.Get(), V) || V != NativeFields) return Stop(TEXT("component_niagara_native_fields_changed"), TEXT("Readonly native CDO fields changed."));
        for (const auto& P : Schema)
            if (FindProperty(Default->GetClass(), P.Key.ToString()) != P.Value || P.Value->HasSetterOrGetter()
                || static_cast<uint64>(P.Value->GetPropertyFlags()) != SchemaFlags[P.Key])
                return Stop(TEXT("component_niagara_schema_changed"), TEXT("Exact retained direct-storage native property changed."));
        if(!AddTransformCurrent())return false;
        if (Adding && !Selected.IsValid() && FindObject<UObject>(Generated.Get(), *(Name + USimpleConstructionScript::ComponentTemplateNameSuffix)))
            return Stop(TEXT("component_niagara_template_slot_changed"),TEXT("Requested own template slot became occupied before CreateNode; no displaced-object rename or retry is dispatched."));
        const auto& Nodes = SCS->GetAllNodes(); const bool WasOld = Selected.IsValid() && Old.ContainsByPredicate([&](const FNodeRole& O) { return O.Node.Get() == Selected.Get(); });
        if (Nodes.Num() != Old.Num() + (Adding && Attached && !WasOld ? 1 : 0)) return Stop(TEXT("component_niagara_scs_membership_changed"), TEXT("Own SCS membership changed outside the selected add."));
        TArray<USCS_Node*> ExpectedRoots = Roots; if (Adding && Attached && !Parent.IsValid()) ExpectedRoots.Add(Selected.Get());
        if (SCS->GetRootNodes() != ExpectedRoots) return Stop(TEXT("component_niagara_scs_roots_changed"), TEXT("Own SCS root membership changed."));
        for (const auto& O : Old)
        {
            auto* N = O.Node.Get(); TArray<USCS_Node*> Children = O.Children; if (Adding && Attached && Parent.Get() == N) Children.Add(Selected.Get());
            if (!Loaded(N) || !Nodes.Contains(N) || N->GetOuter() != SCS.Get() || N->VariableGuid != O.Guid || N->GetVariableName().ToString() != O.Name
                || N->ComponentClass != O.Class || N->ComponentTemplate != O.Template.Get() || N->GetChildNodes() != Children)
                return Stop(TEXT("component_niagara_old_scs_changed"), TEXT("Existing local node/template/child role changed."));
            if (N != Selected.Get() && (!Fields(O.Template.Get(), V) || V != O.Fields)) return Stop(TEXT("component_niagara_old_fields_changed"), TEXT("Existing unselected component fields changed."));
        }
        if (Selected.IsValid())
        {
            if (!Loaded(Selected.Get()) || Selected->GetOuter() != SCS.Get() || Selected->GetVariableName().ToString() != Name
                || Selected->VariableGuid != SelectedGuid || Selected->GetPathName() != SelectedPath || Selected->GetFlags() != SelectedFlags
                || Selected->ComponentClass != UNiagaraComponent::StaticClass() || Selected->ComponentTemplate != Component.Get()
                || !Loaded(Component.Get()) || Component->GetOuter() != Generated.Get() || Component->GetPathName() != TemplatePath || Component->GetFlags() != TemplateFlags
                || !Component->HasAllFlags(RF_ArchetypeObject) || Component->HasAnyFlags(RF_ClassDefaultObject | RF_Transient | RF_DefaultSubObject)
                || !EmptyStore(Component.Get()) || StoreState(Component.Get()) != SelectedStoreState
                || (Attached && !Nodes.Contains(Selected.Get()))) return Stop(TEXT("component_niagara_selected_role_changed"), TEXT("Exact own unregistered fresh Niagara template role changed."));
            if (Component->GetAsset() != ExpectedAsset || CastFieldChecked<FBoolProperty>(Schema[TEXT("bAutoActivate")])->GetPropertyValue_InContainer(Component.Get()) != ExpectedAuto
                || CastFieldChecked<FBoolProperty>(Schema[TEXT("bVisible")])->GetPropertyValue_InContainer(Component.Get()) != ExpectedVisible
                || CastFieldChecked<FBoolProperty>(Schema[TEXT("bHiddenInGame")])->GetPropertyValue_InContainer(Component.Get()) != ExpectedHidden)
                return Stop(TEXT("component_niagara_selected_values_changed"), TEXT("Selected raw fields differ from the confirmed write prefix."));
            if (FieldsReady && (!Fields(Component.Get(), V, true) || V != SelectedFields)) return Stop(TEXT("component_niagara_unselected_fields_changed"), TEXT("Raw writes changed an unselected reflected field."));
            TArray<UObject*> Instances; Component->GetArchetypeInstances(Instances);
            if (!Instances.IsEmpty()) return Stop(TEXT("component_niagara_archetype_instances_pending"), TEXT("Selected template archetype instances are outside the fresh raw scope."));
        }
        for (const auto& S : Systems)
        {
            auto* A = S.System.Get(); for (const auto& I : S.Roles) if (!Exact(I)) return Stop(TEXT("component_niagara_system_role_changed"), TEXT("Readonly source identity changed."));
            if (!A->GetEmitterHandles().IsEmpty()) return Stop(TEXT("component_niagara_system_state_changed"),TEXT("Readonly source emitter state changed."));
            const bool Pending=A->HasOutstandingCompilationRequests(true); if(!FieldRoleCurrent(S.Roles[0]))return false;
            if (A->GetPackage()->IsDirty() || !A->bFullyLoaded || Pending || A->GetActiveInstancesCount() != 0
                || !A->GetExposedParameters().IsEmpty() || ParameterState(A->GetExposedParameters()) != S.ParameterState) return Stop(TEXT("component_niagara_system_state_changed"), TEXT("Readonly source compilation/instance/exposed state changed."));
            TArray<UObject*> Nested; GetObjectsWithOuter(A, Nested, EGetObjectsFlags::IncludeNestedObjects);
            if (Nested.Num() != S.Nested.Num()) return Stop(TEXT("component_niagara_system_subtree_changed"), TEXT("Readonly source subtree changed."));
            for (auto* O : S.Nested) if (!Nested.Contains(O)) return Stop(TEXT("component_niagara_system_subtree_changed"), TEXT("Readonly source subobject left its role."));
            for (int32 Index = 0; Index < S.Roles.Num(); ++Index) if (Index != 1 && Index != 2 && (!Fields(S.Roles[Index].Hold.Get(), V) || V != S.Fields[Index]))
                return Stop(TEXT("component_niagara_system_fields_changed"), TEXT("Readonly source authored reflected fields changed."));
            int64 Bytes; FString Hash; if (!Disk(S.File, Bytes, Hash) || Bytes != S.Bytes || Hash != S.Hash) return Stop(TEXT("component_niagara_system_disk_changed"), TEXT("Readonly source disk changed."));
        }
        TArray<UObject*> Exports; GetObjectsWithOuter(Generated.Get(),Exports,EGetObjectsFlags::IncludeNestedObjects);
        if (Exports.Num()>16384) return Stop(TEXT("component_niagara_export_bound"),TEXT("Current generated-class export observation exceeds its finite bound."));
        for(auto* O:Exports) if(Loaded(O)&&O->IsA<UNiagaraComponent>()&&O!=Component.Get()
            &&!Old.ContainsByPredicate([&](const FNodeRole& Item){return Item.Template.Get()==O;}))
            return Stop(TEXT("component_niagara_unlinked_export_pending"),TEXT("An unlinked Niagara export is outside the selected/old own SCS roles."));
        TArray<UObject*> Actors; GetObjectsOfClass(Generated.Get(), Actors, true, RF_NoFlags);
        for (auto* A : Actors) if (Loaded(A) && !A->HasAnyFlags(RF_ClassDefaultObject)) return Stop(TEXT("component_niagara_live_actor_pending"), TEXT("Loaded Actor instances are outside the fresh template scope."));
        TArray<UClass*> Derived; GetDerivedClasses(Generated.Get(), Derived, true); if (!Derived.IsEmpty()) return Stop(TEXT("component_niagara_derived_class_pending"), TEXT("Loaded derived Blueprint classes are outside this scope."));
        return true;
    }
    bool Begin(UBlueprint* B, USCS_Node* N, USCS_Node* P, const FString& InName, bool Add)
    {
        Editor.Reset(GEditor); if(!Loaded(Editor.Get()))return Stop(TEXT("component_niagara_editor_unreadable"),TEXT("A current loaded Editor is required before world access."));
        World.Reset(GEditor->GetEditorWorldContext().World()); Undo = GUndo;
        BP.Reset(B); SCS.Reset(B->SimpleConstructionScript); Generated.Reset(B->GeneratedClass); Skeleton.Reset(B->SkeletonGeneratedClass); Parent.Reset(P); Name = InName; Adding = Add;
        OtherDirtyBefore=DirtyPackages(); OtherDirtyBefore.Remove(B->GetPackage()->GetName());
        if (!Context() || !Loaded(Generated.Get()) || !Loaded(Skeleton.Get())) return false;
        auto* K = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraComponent"));
        if (K != UNiagaraComponent::StaticClass() || !Loaded(K) || K->ClassGeneratedBy || K->IsEditorOnly() || !K->HasAnyClassFlags(CLASS_Native)
            || !Loaded(K->GetSuperClass()) || K->GetSuperClass()->GetPathName() != TEXT("/Script/Engine.FXSystemComponent")
            || K->GetPackage()->GetName()!=TEXT("/Script/Niagara") || !K->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)
            || K->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly) || K->GetPackage()->IsDirty()) return Fail(R, TEXT("component_niagara_native_source_rejected"), TEXT("Exact already loaded clean runtime Niagara component source required."));
        Default.Reset(Cast<UNiagaraComponent>(K->GetDefaultObject(false)));
        if (!EmptyStore(Default.Get()) || Default->GetAsset() || !Default->HasAllFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return Fail(R, TEXT("component_niagara_native_cdo_rejected"), TEXT("Existing empty native Niagara CDO required; no CDO warming/creation."));
        if (!Pin(B, Fixed) || !Pin(SCS.Get(), Fixed) || !Pin(K, Fixed) || !Pin(K->GetPackage(), Fixed) || !Pin(Default.Get(), Fixed) || !Fields(Default.Get(), NativeFields)) return false;
        NativeStoreState=StoreState(Default.Get());
        for (const TCHAR* Key : {TEXT("Asset"), TEXT("bAutoActivate"), TEXT("bVisible"), TEXT("bHiddenInGame")})
        {
            FProperty* Q = FindProperty(K, Key); const bool Asset = FString(Key) == TEXT("Asset");
            const auto* O = CastField<FObjectProperty>(Q); const auto* Bool = CastField<FBoolProperty>(Q);
            UClass* Owner = Asset ? K : FString(Key) == TEXT("bAutoActivate") ? UActorComponent::StaticClass() : USceneComponent::StaticClass();
            if (!Q || Q->GetName() != Key || Q->GetOwnerClass() != Owner || Q->ArrayDim != 1 || Q->HasSetterOrGetter()
                || Q->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_EditorOnly) || !Q->HasAllPropertyFlags(CPF_Edit)
                || (Asset ? !O || Q->GetClass()!=FObjectProperty::StaticClass() || O->PropertyClass != UNiagaraSystem::StaticClass() : !Bool)) return Fail(R, TEXT("component_niagara_property_schema_mismatch"), TEXT("Exact raw native Asset/bool direct-storage schema required."));
            Schema.Add(FName(Key), Q); SchemaFlags.Add(FName(Key), static_cast<uint64>(Q->GetPropertyFlags()));
        }
        if(!BindAddTransforms())return false;
        auto Source=MakeShared<FJsonObject>(); Source->SetStringField(TEXT("class"),K->GetPathName()); Source->SetStringField(TEXT("existing_cdo"),Default->GetPathName()); Source->SetStringField(TEXT("class_flags_string"),LexToString(uint32(K->GetClassFlags())));
        TArray<TSharedPtr<FJsonValue>> SchemaRows;
        for(const auto& SchemaEntry:Schema){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property"),SchemaEntry.Value->GetPathName());Row->SetStringField(TEXT("field_class"),SchemaEntry.Value->GetClass()->GetName());Row->SetStringField(TEXT("property_flags_string"),LexToString(SchemaFlags[SchemaEntry.Key]));Row->SetBoolField(TEXT("has_getter_or_setter"),false);SchemaRows.Add(MakeShared<FJsonValueObject>(Row));}
        Source->SetArrayField(TEXT("schema"),SchemaRows); R.Data->SetObjectField(TEXT("niagara_native_source_before"),Source);
        Roots = SCS->GetRootNodes(); if (SCS->GetAllNodes().Num() > 10000) return Fail(R, TEXT("component_budget_exceeded"), TEXT("Local SCS bound exceeded."));
        for (auto* Q : SCS->GetAllNodes())
        {
            if (!Loaded(Q) || !Loaded(Q->ComponentTemplate.Get()) || Q->GetOuter() != SCS.Get()) return Stop(TEXT("component_niagara_old_role_unreadable"),TEXT("Existing local SCS role is unreadable."));
            FNodeRole O; O.Node.Reset(Q); O.Template.Reset(Q->ComponentTemplate); O.Name = Q->GetVariableName().ToString(); O.Guid = Q->VariableGuid; O.Class = Q->ComponentClass; O.Children = Q->GetChildNodes();
            Old.Add(MoveTemp(O)); if (!Fields(Old.Last().Template.Get(), Old.Last().Fields)) return false;
        }
        if (N && !Adopt(N, true)) return false;
        if (!File(B, TargetFile) || !Disk(TargetFile, TargetBytes, TargetHash)) return Fail(R, TEXT("component_niagara_target_disk_unobservable"), TEXT("Bounded target disk baseline required."));
        return Check();
    }
    bool Adopt(USCS_Node* N, bool Member)
    {
        // Returned identities are reported before full source/target admission can fail.
        R.Data->SetBoolField(Adding?TEXT("niagara_create_returned_nonnull"):TEXT("niagara_existing_selected_node_present"), N != nullptr);
        R.Data->SetBoolField(Adding?TEXT("niagara_created_node_identity_observed"):TEXT("niagara_selected_node_identity_observed"),false); R.Data->SetBoolField(Adding?TEXT("niagara_created_template_identity_observed"):TEXT("niagara_selected_template_identity_observed"),false);
        if (!IsEditorGameThread() || GExitPurge || IsEngineExitRequested() || IsGarbageCollecting() || !Loaded(N) || !Loaded(N->GetClass()))
            return Stop(TEXT("component_niagara_create_unreadable"), TEXT("CreateNode returned no safely readable node; no cleanup/retry."));
        Selected.Reset(N); R.Data->SetStringField(Adding?TEXT("created_node_path"):TEXT("niagara_selected_node_path"), N->GetPathName()); R.Data->SetStringField(Adding?TEXT("created_node_guid"):TEXT("niagara_selected_node_guid"), N->VariableGuid.ToString(EGuidFormats::Digits)); R.Data->SetBoolField(Adding?TEXT("niagara_created_node_identity_observed"):TEXT("niagara_selected_node_identity_observed"),true);
        auto* T = N->ComponentTemplate.Get(); if (Loaded(T)) { const TStrongObjectPtr<UActorComponent> KeepReturnedTemplate(T); R.Data->SetStringField(Adding?TEXT("created_template_path"):TEXT("niagara_selected_template_path"), T->GetPathName()); R.Data->SetBoolField(Adding?TEXT("niagara_created_template_identity_observed"):TEXT("niagara_selected_template_identity_observed"),true); }
        Component.Reset(Cast<UNiagaraComponent>(T)); Attached = Member; SelectedGuid = N->VariableGuid; SelectedPath = N->GetPathName(); SelectedFlags = N->GetFlags();
        if (!Context() || !Basic() || !Loaded(Component.Get()) || Component->GetClass()!=UNiagaraComponent::StaticClass() || !SelectedGuid.IsValid()) return Stop(TEXT("component_niagara_template_unreadable"), TEXT("Returned current template identity/GUID is not readable."));
        TemplatePath = Component->GetPathName(); TemplateFlags = Component->GetFlags(); SelectedStoreState = StoreState(Component.Get());
        ExpectedAsset = Component->GetAsset(); ExpectedAuto = CastFieldChecked<FBoolProperty>(Schema[TEXT("bAutoActivate")])->GetPropertyValue_InContainer(Component.Get());
        ExpectedVisible = CastFieldChecked<FBoolProperty>(Schema[TEXT("bVisible")])->GetPropertyValue_InContainer(Component.Get()); ExpectedHidden = CastFieldChecked<FBoolProperty>(Schema[TEXT("bHiddenInGame")])->GetPropertyValue_InContainer(Component.Get());
        if(AddTransforms)
        {
            if(!TransformTemplate(Component.Get()))return Stop(TEXT("component_niagara_transform_role_rejected"),TEXT("Returned fresh template must have no Asset, collision, live attachment or pending movement before transform authoring."));
            ExpectedLocation=Component->GetRelativeLocation();ExpectedRotation=Component->GetRelativeRotation();ExpectedScale=Component->GetRelativeScale3D();TransformUpdatedExpected=TransformUpdatedField->GetPropertyValue_InContainer(Component.Get());TransformReady=true;
        }
        if (!Fields(Component.Get(), SelectedFields, true)) return false; FieldsReady = true;
        if(Adding)
        {
            for(const auto& P:Schema)
            {
                const bool Matches=P.Value->Identical(P.Value->ContainerPtrToValuePtr<void>(Default.Get()),P.Value->ContainerPtrToValuePtr<void>(Component.Get()));
                if(!Check())return false;
                if(!Matches)return Stop(TEXT("component_niagara_created_defaults_changed"),TEXT("Returned fresh selected fields differ from the retained native defaults; actual creation identity remains without retry."));
            }
        }
        return SourceSystem(ExpectedAsset) && Check();
    }
    bool AfterCompile()
    {
        // Only an explicit compiler return may replace generated/skeleton/template roles.
        if (!Context() || !Loaded(BP.Get()) || BP->SimpleConstructionScript != SCS.Get() || !Loaded(BP->GeneratedClass) || !Loaded(BP->SkeletonGeneratedClass))
            return Stop(TEXT("component_niagara_compiler_roles_unreadable"), TEXT("Explicit compile returned no readable current Blueprint roles."));
        Generated.Reset(BP->GeneratedClass); Skeleton.Reset(BP->SkeletonGeneratedClass);
        for (auto& O : Old)
        {
            auto* Q = O.Node.Get(); if (!Loaded(Q) || !Loaded(Q->ComponentTemplate.Get()) || Q->VariableGuid != O.Guid || Q->ComponentClass != O.Class)
                return Stop(TEXT("component_niagara_compiler_node_changed"), TEXT("Compiler changed a retained node declaration."));
            O.Template.Reset(Q->ComponentTemplate);
        }
        if (!Loaded(Selected.Get()) || !Loaded(Selected->ComponentTemplate.Get())) return Stop(TEXT("component_niagara_compiler_template_unreadable"), TEXT("Compiler returned no selected current template."));
        Component.Reset(Cast<UNiagaraComponent>(Selected->ComponentTemplate.Get()));
        if (!Loaded(Component.Get()) || Component->GetPathName() != TemplatePath || Component->GetFlags() != TemplateFlags)
            return Stop(TEXT("component_niagara_compiler_identity_changed"), TEXT("Selected compiler template path/flags differs from the retained declaration."));
        return Check();
    }
};

bool MutateNativeNiagara(UBlueprint* BP, USCS_Node* Existing, USCS_Node* Parent, const FString& Name,
    const TSharedRef<FJsonObject>& Requested, const FReadResult& Before, bool Add, FMutationResult& R)
{
    if (BP->ParentClass != AActor::StaticClass()) return Fail(R,TEXT("component_niagara_native_actor_required"),TEXT("KN requires an own normal Blueprint with exact native Actor parent; generated/native subclasses are outside this fresh slice."));
    if (Before.bLoadedDuringRead) { R.bChangeStateKnown=false; return Fail(R,TEXT("component_niagara_target_preload_required"),TEXT("KN requires an already loaded target; the preflight read may have performed implicit load work, so no target writer was dispatched.")); }
    FNiagaraTemplateScope Scope(R); TArray<TUniquePtr<FPreparedProperty>> Prepared; TUniquePtr<FScopedTransaction> Transaction;
    const auto OuterUndo = GUndo; const bool DirtyBefore = BP->GetPackage()->IsDirty(); bool Created = false, Applied = false, Begun = false;
    R.Data->SetStringField(TEXT("niagara_scope"), TEXT("fresh own native Actor SCS Niagara template; identity Add transforms; loaded saved-clean empty System refs and exact Asset/3bool raw stores; no SetAsset/PreEdit/PostEdit/reinitialize/DI sync or hidden source load"));
    R.Data->SetBoolField(TEXT("niagara_ready_or_valid_queried"), false); R.Data->SetBoolField(TEXT("niagara_source_load_attempted"), false);
    if(Add)
    {
        const TCHAR* TransformKeys[]={TEXT("RelativeLocation"),TEXT("RelativeRotation"),TEXT("RelativeScale3D")};
        for(int32 AxisGroup=0;AxisGroup<3;++AxisGroup)
        {
            const auto Value=Requested->TryGetField(TransformKeys[AxisGroup]);double XYZ[3];if(!Value.IsValid()||!VectorValues(Value,3,XYZ))return Fail(R,TEXT("component_niagara_transform_invalid"),TEXT("Niagara Add transform requires three finite bounded triples before CreateNode."));
            const double IdentityAxis=AxisGroup==2?1.:0.;for(int32 AxisIndex=0;AxisIndex<3;++AxisIndex){Scope.AddTransforms|=XYZ[AxisIndex]!=IdentityAxis;XYZ[AxisIndex]=static_cast<float>(XYZ[AxisIndex]);}
            Scope.TransformInput[AxisGroup]=FVector(XYZ[0],XYZ[1],XYZ[2]);Scope.TransformRequests.Add(Value);
        }
        if(Scope.AddTransforms){R.Data->SetStringField(TEXT("niagara_scope"),TEXT("fresh own native Actor SCS Niagara Add with finite float32-adapted public relative setters before SCS attachment; empty/noWorld/noCollision/no live movement; existing raw Asset/3bool Set remains separate"));R.Data->SetStringField(TEXT("niagara_transform_scope"),TEXT("location then rotation then scale; public setter derived bounds/transform caches are allowed effects, not DI/simulation or global callback-purity proof"));}
    }
    const bool Run = [&]() -> bool
    {
        if (!Scope.Begin(BP, Existing, Parent, Name, Add)) return false; Begun = true;
        for (const auto& Pair : Requested->Values)
        {
            const FString Key(Pair.Key.ToView()); const FName FieldName(*Key);
            if (Add && (Key == TEXT("RelativeLocation") || Key == TEXT("RelativeRotation") || Key == TEXT("RelativeScale3D")))
            {
                // Transform triples were fully prepared before Scope.Begin; only the KQ Add path applies public setters.
                continue;
            }
            FProperty* P = Scope.Schema.FindRef(FieldName);
            if (!P || Prepared.ContainsByPredicate([&](const auto& Q) { return Q->Property == P; })) return Fail(R, TEXT("component_niagara_property_pending"), TEXT("Only distinct exact Asset/bAutoActivate/bVisible/bHiddenInGame fields are supported."));
            auto Q = MakeUnique<FPreparedProperty>(); Q->Property = P; Q->Requested = Pair.Value; Q->Value = FMemory::Malloc(P->GetSize(), P->GetMinAlignment()); P->InitializeValue(Q->Value);
            if (P->GetFName() == TEXT("Asset"))
            {
                if (Pair.Value->Type != EJson::String) return Fail(R, TEXT("component_niagara_asset_value_invalid"), TEXT("Asset accepts an exact loaded object/package path string or empty-string clear, matching the raw legacy branch."));
                FString Path = Pair.Value->AsString(); UNiagaraSystem* Asset = nullptr;
                if (!Path.IsEmpty())
                {
                    if (!SafeName(Path) || Path.Contains(TEXT(":")) || Path != FPackageName::ExportTextPathToObjectPath(Path)) return Fail(R, TEXT("component_niagara_asset_path_invalid"), TEXT("Use a bounded canonical package/object path without subobject or export-text syntax."));
                    if (!Path.Contains(TEXT("."))) { if (!FPackageName::IsValidLongPackageName(Path, true)) return Fail(R, TEXT("component_niagara_asset_path_invalid"), TEXT("Invalid source package path.")); Path += TEXT(".") + FPackageName::GetLongPackageAssetName(Path); }
                    if (!FPackageName::IsValidObjectPath(Path)) return Fail(R, TEXT("component_niagara_asset_path_invalid"), TEXT("Invalid source object path."));
                    UObject* O = FindObject<UObject>(nullptr, *Path); const TStrongObjectPtr<UObject> KeepFound(O); Asset = FNiagaraTemplateScope::Loaded(O)?Cast<UNiagaraSystem>(O):nullptr;
                    if (!Asset || Asset->GetClass() != UNiagaraSystem::StaticClass()) return Fail(R, TEXT("component_niagara_asset_not_loaded_or_class_mismatch"), TEXT("Exact native NiagaraSystem must already be loaded; no source load or target writer was dispatched."));
                    if (!Scope.SourceSystem(Asset)) return false;
                }
                if (Scope.ExpectedAsset && Asset && Scope.ExpectedAsset != Asset) return Fail(R, TEXT("component_niagara_asset_replacement_pending"), TEXT("General nonnull System replacement is outside the fresh raw path."));
                Q->Reference.Reset(Asset); CastFieldChecked<FObjectPropertyBase>(P)->SetObjectPropertyValue(Q->Value, Asset);
            }
            else
            {
                if (Pair.Value->Type != EJson::Boolean) return Fail(R, TEXT("component_niagara_bool_invalid"), TEXT("Niagara bool fields require JSON booleans."));
                CastFieldChecked<FBoolProperty>(P)->SetPropertyValue(Q->Value, Pair.Value->AsBool());
            }
            FString Error; auto* From = Add ? Scope.Default.Get() : Scope.Component.Get();
            if (!ReadComponentValue(P, P->ContainerPtrToValuePtr<void>(From), MaxJsonChars, Q->Before, Q->Encoding, Error) || !Scope.Check()) return false;
            Prepared.Add(MoveTemp(Q));
        }
        if (!Scope.Check()) return false; R.Stage = TEXT("mutation"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
        R.Data->SetBoolField(TEXT("niagara_transaction_begin_attempted"), true); Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("Cubeless", "NiagaraTemplateRaw", "Cubeless Niagara template raw properties")); Scope.Undo = GUndo;
        R.Data->SetBoolField(TEXT("niagara_transaction_begin_returned"), true); if (!Scope.Check()) return false;
        R.Data->SetBoolField(TEXT("niagara_blueprint_modify_attempted"), true); BP->Modify(); R.Data->SetBoolField(TEXT("niagara_blueprint_modify_returned"), true); if (!Scope.Check()) return false;
        R.Data->SetBoolField(TEXT("niagara_scs_modify_attempted"), true); Scope.SCS->Modify(); R.Data->SetBoolField(TEXT("niagara_scs_modify_returned"), true); if (!Scope.Check()) return false;
        if (Add)
        {
            R.Data->SetNumberField(TEXT("niagara_create_calls"), 1); USCS_Node* NewNode = Scope.SCS->CreateNode(UNiagaraComponent::StaticClass(), FName(*Name));
            Created = NewNode != nullptr; R.Data->SetBoolField(TEXT("creation_observed"), Created);
            if (!Scope.Adopt(NewNode, false)) return false;
            if(Scope.AddTransforms)
            {
                R.Data->SetBoolField(TEXT("niagara_node_modify_attempted"),true);Scope.Selected->Modify();R.Data->SetBoolField(TEXT("niagara_node_modify_returned"),true);if(!Scope.Check())return false;
                R.Data->SetBoolField(TEXT("niagara_template_modify_attempted"),true);Scope.Component->Modify();R.Data->SetBoolField(TEXT("niagara_template_modify_returned"),true);if(!Scope.ApplyAddTransforms())return false;
            }
            if (Parent) { Parent->Modify(); if (!Scope.Check()) return false; Parent->AddChildNode(NewNode); }
            else { Scope.SCS->AddNode(NewNode); }
            Scope.Attached = true; if (!Scope.Check()) return false;
        }
        if(!Scope.AddTransforms)
        {
            R.Data->SetBoolField(TEXT("niagara_node_modify_attempted"), true); Scope.Selected->Modify(); R.Data->SetBoolField(TEXT("niagara_node_modify_returned"), true); if (!Scope.Check()) return false;
            R.Data->SetBoolField(TEXT("niagara_template_modify_attempted"), true); Scope.Component->Modify(); R.Data->SetBoolField(TEXT("niagara_template_modify_returned"), true); if (!Scope.Check()) return false;
        }
        for (const auto& Q : Prepared)
        {
            if (!Scope.Check()) return false;
            const FName Key = Q->Property->GetFName(); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("property_name"), Key.ToString()); Row->SetField(TEXT("before"), Q->Before); Row->SetField(TEXT("requested"), Q->Requested); Row->SetBoolField(TEXT("verified"), false);
            if (Key == TEXT("Asset")) Scope.ExpectedAsset = Cast<UNiagaraSystem>(Q->Reference.Get());
            else if (Key == TEXT("bAutoActivate")) Scope.ExpectedAuto = Q->Requested->AsBool();
            else if (Key == TEXT("bVisible")) Scope.ExpectedVisible = Q->Requested->AsBool(); else Scope.ExpectedHidden = Q->Requested->AsBool();
            // Exact native direct-storage field; no accessor, propagation, notification or SetAsset.
            Q->Property->CopyCompleteValue(Q->Property->ContainerPtrToValuePtr<void>(Scope.Component.Get()), Q->Value);
            Scope.Applied.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("niagara_raw_applied_prefix"), Scope.Applied);
            if (!Scope.Check()) return false;
            const bool Matches=Q->Property->Identical(Q->Value, Q->Property->ContainerPtrToValuePtr<void>(Scope.Component.Get()));
            if(!Scope.Check())return false; Row->SetBoolField(TEXT("verified"),Matches);
            if (!Matches) return Scope.Stop(TEXT("component_niagara_raw_readback_failed"), TEXT("Raw returned prefix failed exact selected storage readback."));
        }
        R.Data->SetBoolField(TEXT("niagara_mark_modified_attempted"), true); FBlueprintEditorUtils::MarkBlueprintAsModified(BP); R.Data->SetBoolField(TEXT("niagara_mark_modified_returned"), true); if (!Scope.Check()) return false;
        if (R.bCompileRequested)
        {
            R.Stage = TEXT("compile"); R.bCompileAttempted = true; auto Compile = Cubeless::UE58::CompileBlueprint(Before.BlueprintPath, false, false); R.Data->SetObjectField(TEXT("compile"), Compile);
            bool Compiled = false; Compile->TryGetBoolField(TEXT("compiled"), Compiled); R.Compiled = Compiled;
            if (!Scope.AfterCompile()) return false;
            if (!Compiled) return Fail(R, TEXT("component_compile_failed"), TEXT("Niagara template prefix remains but explicit Blueprint compile failed; not saved or retried."));
        }
        return true;
    }();
    if (!Begun) { R.bSuccess = false; return false; }
    if (Transaction)
    {
        const bool SafeBeforeEnd = !Scope.Stopped && Scope.Check(); R.Data->SetBoolField(TEXT("niagara_transaction_end_attempted"), true);
        Transaction.Reset(); Scope.Undo = OuterUndo; R.Data->SetBoolField(TEXT("niagara_transaction_end_returned"), true);
        if (!SafeBeforeEnd || Scope.Stopped || !Scope.Check()) { R.bChangeStateKnown = false; return false; }
    }
    if (Scope.Stopped) { R.bChangeStateKnown = false; return false; }
    if (!Scope.Check()) return false;
    int64 TargetBytes; FString TargetHash;
    if (!Scope.Disk(Scope.TargetFile, TargetBytes, TargetHash) || TargetBytes != Scope.TargetBytes || TargetHash != Scope.TargetHash)
        return Scope.Stop(TEXT("component_niagara_unrequested_save"), TEXT("Target disk changed although Add/Set requests no save."));
    FReadResult After; FString Code, Error;
    if (!ReadLocalComponents(Before.BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, After, Code, Error) || !Scope.Check())
        return Scope.Stop(TEXT("component_niagara_after_unobservable"), TEXT("Current local SCS endpoint is unobservable; prefix remains without retry."));
    R.Data->SetObjectField(TEXT("after"), After.Data); R.Data->SetBoolField(TEXT("dirty_before"), DirtyBefore); R.Data->SetBoolField(TEXT("dirty_after"), BP->GetPackage()->IsDirty());
    R.bChanged = Created || DirtyBefore != BP->GetPackage()->IsDirty();
    TArray<TSharedPtr<FJsonValue>> Observations; bool ValuesMatch=true;
    for (const auto& Q : Prepared)
    {
        TSharedPtr<FJsonValue> V; FString Encoding;
        if (!ReadComponentValue(Q->Property, Q->Property->ContainerPtrToValuePtr<void>(Scope.Component.Get()), MaxJsonChars, V, Encoding, Error) || !Scope.Check()) return Scope.Stop(TEXT("component_niagara_after_unobservable"), TEXT("Raw endpoint value is unreadable."));
        R.bChanged |= !FJsonValue::CompareEqual(*Q->Before, *V); auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("property_name"),Q->Property->GetName());Row->SetField(TEXT("requested"),Q->Requested);Row->SetField(TEXT("before"),Q->Before);Row->SetField(TEXT("after"),V);Row->SetStringField(TEXT("encoding"),Encoding);const bool Matches=Q->Property->Identical(Q->Value,Q->Property->ContainerPtrToValuePtr<void>(Scope.Component.Get()));if(!Scope.Check())return false;Row->SetBoolField(TEXT("postcondition_passed"),Matches);ValuesMatch &= Matches;Observations.Add(MakeShared<FJsonValueObject>(Row));
    }
    R.Data->SetArrayField(TEXT("property_observations"),Observations);
    R.Data->SetStringField(TEXT("component_class"),TEXT("/Script/Niagara.NiagaraComponent"));R.Data->SetStringField(TEXT("template_path"),Scope.Component->GetPathName());
    if(Add)R.Data->SetStringField(TEXT("parent_component"),Scope.Parent.IsValid()?Scope.Parent->GetVariableName().ToString():FString());
    Applied = Run && ValuesMatch && (Add ? After.NodeVisits == Before.NodeVisits + 1 : After.NodeVisits == Before.NodeVisits);
    R.bChangeStateKnown = true; R.bSuccess = Applied; R.bPostconditionPassed = Applied; R.Data->SetBoolField(TEXT("creation_observed"), Created);
    R.Data->SetBoolField(TEXT("niagara_native_and_system_sources_before_after_preserved"),true); R.Data->SetBoolField(TEXT("niagara_target_disk_unchanged"),true);
    R.Data->SetArrayField(TEXT("dirty_packages_after"),Strings(DirtyPackages()));
    R.Data->SetBoolField(TEXT("automatic_cleanup_attempted"), false); R.Data->SetBoolField(TEXT("niagara_runtime_evaluation_verified"), false);
    R.Data->SetStringField(TEXT("niagara_unobserved_scope"), TEXT("private outgoing parameter-store bindings/AssetExposedParametersChangedHandle, private optimization/async and arbitrary callback/cache state are not fully observable; no readiness/validity/runtime or parameter synchronization proof"));
    if (Applied) R.Stage = TEXT("complete"); else if (R.ErrorCode.IsEmpty()) Fail(R, TEXT("component_postcondition_failed"), TEXT("Raw Niagara selected prefix did not meet the endpoint contract."));
    return R.bSuccess;
}

// KO/TG: selected own generated ActorComponent values; never source BPC defaults.
// TG uses current authored descriptors rather than a fixed list of property names.
struct FGeneratedValueScope
{
    struct FRole
    {
        TStrongObjectPtr<UObject> Hold, Outer; TStrongObjectPtr<UClass> Class; TStrongObjectPtr<UPackage> Package;
        FString Path; EObjectFlags Flags=RF_NoFlags; uint32 PackageFlags=0; EClassFlags ClassFlags=CLASS_None; UClass* Super=nullptr; UObject* CDO=nullptr;
    };
    struct FNode
    {
        TStrongObjectPtr<USCS_Node> Hold; TStrongObjectPtr<UActorComponent> Template; TStrongObjectPtr<UClass> Class;
        FGuid Guid; FString Name, Fields; TArray<USCS_Node*> Children;
    };
    FMutationResult& R; bool Stopped=false, Ready=false,TypesBound=false;int32 EnumGetterCalls=0;
    TStrongObjectPtr<UEditorEngine> Editor; TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UBlueprint> Host, Source; TStrongObjectPtr<USimpleConstructionScript> SCS;
    TStrongObjectPtr<UClass> Generated, Skeleton, SourceClass, SourceSkeleton;
    TStrongObjectPtr<USCS_Node> Selected; TStrongObjectPtr<UActorComponent> Component;
    TArray<FRole> Roles; TArray<FNode> Nodes; TArray<USCS_Node*> NodeList, Roots; TArray<UObject*> SourceObjects;
    TArray<FString> SourceFields, OtherDirty; TUniquePtr<FPreparedProperty> Original;
    FProperty* Property=nullptr; FProperty* SkeletonProperty=nullptr; FFieldClass* PropertyKind=nullptr; UScriptStruct* ValueStruct=nullptr; const void* Expected=nullptr;
    uint64 PropertyFlags=0, SkeletonFlags=0; int32 PropertyOffset=0, SkeletonOffset=0, PropertySize=0,PropertyAlignment=0,SkeletonAlignment=0;
    struct FValueType
    {
        FProperty* Field=nullptr; FFieldClass* Kind=nullptr; FProperty* Inner=nullptr; FFieldClass* InnerKind=nullptr;
        UScriptStruct* Struct=nullptr; UEnum* Enum=nullptr; uint64 InnerFlags=0; int32 InnerSize=0,InnerOffset=0,InnerAlign=0;
        int32 StructSize=0; EStructFlags StructFlags=STRUCT_NoFlags; UEnum::ECppForm EnumForm=UEnum::ECppForm::Regular;
        bool NativeBool=false; uint8 BoolSize=0,BoolOffset=0,BoolMask=0,BoolFieldMask=0; TArray<FString> EnumNames; TArray<int64> EnumValues;
    } RuntimeType, SkeletonType;
    FGuid VariableGuid; FString PropertyName, SourceFile, SourceHash, TargetFile, TargetHash; int64 SourceBytes=-1, TargetBytes=-1;
    EBlueprintStatus SourceStatus=BS_Unknown; decltype(GUndo) Undo=nullptr;
    explicit FGeneratedValueScope(FMutationResult& In):R(In){}
    static bool Loaded(const UObject* O){return FNiagaraTemplateScope::Loaded(O);}
    bool Stop(const TCHAR* Code,const TCHAR* Why)
    {
        if(Stopped)return false;Stopped=true;R.bChangeStateKnown=false;R.bPostconditionPassed=false;
        R.Data->SetBoolField(TEXT("generated_value_followup_stopped"),true);R.Data->SetStringField(TEXT("generated_value_failed_boundary"),Code);
        if(R.ErrorCode.IsEmpty())Fail(R,Code,Why);return false;
    }
    bool Context()
    {
        if(Stopped)return false;
        if(!IsEditorGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()
            ||!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Loaded(World.Get())||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get()||GUndo!=Undo)
            return Stop(TEXT("generated_value_context_changed"),TEXT("Captured idle Editor/world/transaction changed; no follow-up writer/read/retry."));
        return true;
    }
    bool Pin(UObject* O)
    {
        if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter())))
            return Stop(TEXT("generated_value_role_unreadable"),TEXT("A current retained source/target role is unreadable."));
        if(Roles.ContainsByPredicate([&](const FRole& A){return A.Hold.Get()==O;}))return true;
        FRole A;A.Hold.Reset(O);A.Outer.Reset(O->GetOuter());A.Class.Reset(O->GetClass());A.Package.Reset(O->GetPackage());A.Path=O->GetPathName();A.Flags=O->GetFlags();A.PackageFlags=O->GetPackage()->GetPackageFlags();
        if(auto* K=Cast<UClass>(O)){A.ClassFlags=K->GetClassFlags();A.Super=K->GetSuperClass();A.CDO=K->GetDefaultObject(false);}Roles.Add(MoveTemp(A));return true;
    }
    static TSharedPtr<FJsonValue> PointerValue(const UObject* O)
    {
        // Pointer identity only; diagnostics do not dereference a mismatching role.
        return MakeShared<FJsonValueString>(FString::Printf(TEXT("%p"),static_cast<const void*>(O)));
    }
    UPackage* HostPackageIdentity() const
    {
        for(const auto& A:Roles)if(A.Hold.Get()==Host.Get())return A.Package.Get();return nullptr;
    }
    bool IdentityMismatch(const FRole& A,int32 RoleIndex,const TCHAR* Name,const TSharedPtr<FJsonValue>& Before,const TSharedPtr<FJsonValue>& After,const TCHAR* Code,bool ActualObserved=true)
    {
        if(!R.Data->HasField(TEXT("first_generated_value_identity_failure")))
        {
            auto D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("role_index"),RoleIndex);D->SetStringField(TEXT("captured_object_path"),A.Path);D->SetStringField(TEXT("property"),Name);
            D->SetField(TEXT("expected"),Before);if(ActualObserved)D->SetField(TEXT("actual"),After);else D->SetField(TEXT("actual"),MakeShared<FJsonValueNull>());
            D->SetBoolField(TEXT("expected_observed"),true);D->SetBoolField(TEXT("actual_observed"),ActualObserved);D->SetBoolField(TEXT("current_object_path_observed"),false);D->SetField(TEXT("current_object_path"),MakeShared<FJsonValueNull>());
            D->SetStringField(TEXT("stage"),R.Stage);D->SetStringField(TEXT("boundary"),Code);D->SetBoolField(TEXT("is_selected_host"),A.Hold.Get()==Host.Get());D->SetBoolField(TEXT("is_captured_host_package"),A.Hold.Get()==HostPackageIdentity());
            D->SetStringField(TEXT("reference_encoding"),TEXT("class/outer/package/super/CDO values are pointer identities, not dereferenced current paths"));R.Data->SetObjectField(TEXT("first_generated_value_identity_failure"),D);
        }
        return Stop(Code,TEXT("Retained source/target identity or class changed; earliest mismatch is recorded without retry."));
    }
    bool RootCurrent()
    {
        if(!Context())return false;
        for(int32 RoleIndex=0;RoleIndex<Roles.Num();++RoleIndex)
        {
            const auto& A=Roles[RoleIndex];auto* O=A.Hold.Get();const TCHAR* IdentityCode=TEXT("generated_value_identity_changed");const TCHAR* ClassCode=TEXT("generated_value_class_changed");
            if(!Loaded(O))return IdentityMismatch(A,RoleIndex,TEXT("object_loaded"),MakeShared<FJsonValueBoolean>(true),MakeShared<FJsonValueBoolean>(false),IdentityCode);
            if(!Loaded(A.Class.Get()))return IdentityMismatch(A,RoleIndex,TEXT("captured_class_loaded"),MakeShared<FJsonValueBoolean>(true),MakeShared<FJsonValueBoolean>(false),IdentityCode);
            if(!Loaded(A.Package.Get()))return IdentityMismatch(A,RoleIndex,TEXT("captured_package_loaded"),MakeShared<FJsonValueBoolean>(true),MakeShared<FJsonValueBoolean>(false),IdentityCode);
            if(A.Outer.IsValid()&&!Loaded(A.Outer.Get()))return IdentityMismatch(A,RoleIndex,TEXT("captured_outer_loaded"),MakeShared<FJsonValueBoolean>(true),MakeShared<FJsonValueBoolean>(false),IdentityCode);
            auto* CurrentClass=O->GetClass();if(CurrentClass!=A.Class.Get())return IdentityMismatch(A,RoleIndex,TEXT("class"),PointerValue(A.Class.Get()),PointerValue(CurrentClass),IdentityCode);
            auto* CurrentOuter=O->GetOuter();if(CurrentOuter!=A.Outer.Get())return IdentityMismatch(A,RoleIndex,TEXT("outer"),PointerValue(A.Outer.Get()),PointerValue(CurrentOuter),IdentityCode);
            auto* CurrentPackage=O->GetPackage();if(CurrentPackage!=A.Package.Get())return IdentityMismatch(A,RoleIndex,TEXT("package"),PointerValue(A.Package.Get()),PointerValue(CurrentPackage),IdentityCode);
            const uint32 CurrentPackageFlags=A.Package->GetPackageFlags();if(CurrentPackageFlags!=A.PackageFlags)return IdentityMismatch(A,RoleIndex,TEXT("package_flags"),MakeShared<FJsonValueNumber>(A.PackageFlags),MakeShared<FJsonValueNumber>(CurrentPackageFlags),IdentityCode);
            const FString CurrentPath=O->GetPathName();if(!CurrentPath.Equals(A.Path,ESearchCase::CaseSensitive))return IdentityMismatch(A,RoleIndex,TEXT("object_path"),MakeShared<FJsonValueString>(A.Path),MakeShared<FJsonValueString>(CurrentPath),IdentityCode);
            const auto CurrentFlags=O->GetFlags();if(CurrentFlags!=A.Flags)return IdentityMismatch(A,RoleIndex,TEXT("object_flags"),MakeShared<FJsonValueNumber>(static_cast<uint32>(A.Flags)),MakeShared<FJsonValueNumber>(static_cast<uint32>(CurrentFlags)),IdentityCode);
            if(const auto* K=Cast<UClass>(O))
            {
                const auto CurrentClassFlags=K->GetClassFlags();if(CurrentClassFlags!=A.ClassFlags)return IdentityMismatch(A,RoleIndex,TEXT("class_flags"),MakeShared<FJsonValueNumber>(static_cast<uint32>(A.ClassFlags)),MakeShared<FJsonValueNumber>(static_cast<uint32>(CurrentClassFlags)),ClassCode);
                auto* CurrentSuper=K->GetSuperClass();if(CurrentSuper!=A.Super)return IdentityMismatch(A,RoleIndex,TEXT("super_class"),PointerValue(A.Super),PointerValue(CurrentSuper),ClassCode);
                auto* CurrentCDO=K->GetDefaultObject(false);if(CurrentCDO!=A.CDO)return IdentityMismatch(A,RoleIndex,TEXT("class_default_object"),PointerValue(A.CDO),PointerValue(CurrentCDO),ClassCode);
            }
        }
        if(!Loaded(Host.Get())||!Loaded(Source.Get())||!Loaded(SCS.Get())||!Loaded(Component.Get())||!Loaded(Selected.Get())
            ||Host->ParentClass!=AActor::StaticClass()||Host->SimpleConstructionScript!=SCS.Get()||Host->GeneratedClass!=Generated.Get()||Host->SkeletonGeneratedClass!=Skeleton.Get()
            ||Source->ParentClass!=UActorComponent::StaticClass()||Source->GeneratedClass!=SourceClass.Get()||Source->SkeletonGeneratedClass!=SourceSkeleton.Get()
            ||SourceClass->ClassGeneratedBy!=Source.Get()||SourceClass->GetAuthoritativeClass()!=SourceClass.Get()||SourceClass->GetSuperClass()!=UActorComponent::StaticClass()
            ||Host->bBeingCompiled||Host->bQueuedForCompilation||Host->bIsRegeneratingOnLoad||Host->bSuppressStructurallyModified||Host->Status==BS_BeingCreated
            ||Source->bBeingCompiled||Source->bQueuedForCompilation||Source->bIsRegeneratingOnLoad||Source->bSuppressStructurallyModified||Source->Status!=SourceStatus||Source->GetPackage()->IsDirty())
            return Stop(TEXT("generated_value_current_roles_changed"),TEXT("Current Actor host or saved-clean direct ActorComponent source roles changed."));
        if(SCS->GetAllNodes()!=NodeList||SCS->GetRootNodes()!=Roots)return Stop(TEXT("generated_value_scs_membership_changed"),TEXT("Existing SCS membership/roots changed."));
        for(const auto& N:Nodes)
            if(!Loaded(N.Hold.Get())||!Loaded(N.Template.Get())||N.Hold->GetOuter()!=SCS.Get()||N.Hold->GetVariableName().ToString()!=N.Name||N.Hold->VariableGuid!=N.Guid
                ||N.Hold->ComponentClass!=N.Class.Get()||N.Hold->ComponentTemplate!=N.Template.Get()||N.Hold->GetChildNodes()!=N.Children)
                return Stop(TEXT("generated_value_scs_declaration_changed"),TEXT("Existing own node/GUID/class/template/child role changed."));
        if(Selected->ComponentTemplate!=Component.Get()||Component->GetClass()!=SourceClass.Get()||Component->GetOuter()!=Generated.Get()
            ||!Component->HasAllFlags(RF_ArchetypeObject)||Component->HasAnyFlags(RF_ClassDefaultObject|RF_Transient|RF_DefaultSubObject)
            ||Component->IsRegistered()||Component->IsPreRegistering()||Component->IsActive()||Component->GetOwner()||Component->GetWorld()||Component->HasBegunPlay()
            ||Component->IsRenderStateCreated()||Component->IsPhysicsStateCreated())
            return Stop(TEXT("generated_value_template_role_changed"),TEXT("Selected own non-scene template is no longer fresh/inactive/unregistered/noWorld."));
        if(Property)
        {
            if(FindProperty(SourceClass.Get(),PropertyName)!=Property||FindProperty(SourceSkeleton.Get(),PropertyName)!=SkeletonProperty
                ||Property->GetOwnerClass()!=SourceClass.Get()||SkeletonProperty->GetOwnerClass()!=SourceSkeleton.Get()||Property->ArrayDim!=1||SkeletonProperty->ArrayDim!=1
                ||Property->GetPropertyFlags()!=PropertyFlags||SkeletonProperty->GetPropertyFlags()!=SkeletonFlags||Property->GetOffset_ForInternal()!=PropertyOffset
                ||SkeletonProperty->GetOffset_ForInternal()!=SkeletonOffset||Property->GetSize()!=PropertySize||SkeletonProperty->GetSize()!=PropertySize||Property->GetClass()!=PropertyKind||SkeletonProperty->GetClass()!=PropertyKind
                ||Property->GetElementSize()!=PropertySize||SkeletonProperty->GetElementSize()!=PropertySize||Property->GetMinAlignment()!=PropertyAlignment||SkeletonProperty->GetMinAlignment()!=SkeletonAlignment
                ||Property->HasSetterOrGetter()||SkeletonProperty->HasSetterOrGetter())
                return Stop(TEXT("generated_value_property_layout_changed"),TEXT("Current own runtime/skeleton descriptor changed; stale storage is not reused."));
            int32 Matches=0;for(const auto& V:Source->NewVariables)if(V.VarName==Property->GetFName()){++Matches;if(V.VarGuid!=VariableGuid)return Stop(TEXT("generated_value_variable_changed"),TEXT("Source variable GUID changed."));}
            if(Matches!=1)return Stop(TEXT("generated_value_variable_changed"),TEXT("Selected source own declaration changed."));
        }
        return true;
    }
    // Only built-in descriptor implementations are dispatched. The generated
    // component still has exact native UActorComponent ancestry, with no live
    // owner/world/archetype consumers and no selected accessor/instanced value.
    bool Current()
    {
        if(!RootCurrent())return false;if(!TypesBound)return true;
        if(!ValueTypeCurrent(RuntimeType)||!ValueTypeCurrent(SkeletonType))return Stop(TEXT("generated_value_type_role_changed"),TEXT("Current nested descriptor/type role changed; no follow-up SDK."));
        if(!RootCurrent())return false;const bool Same=Property->SameType(SkeletonProperty);
        if(!RootCurrent())return false;return Same||Stop(TEXT("generated_value_type_role_changed"),TEXT("Current generated/skeleton value types no longer agree."));
    }
    static bool PlainNumericKind(const FFieldClass* K)
    {
        return K==FByteProperty::StaticClass()||K==FInt8Property::StaticClass()||K==FInt16Property::StaticClass()
            ||K==FIntProperty::StaticClass()||K==FInt64Property::StaticClass()||K==FUInt16Property::StaticClass()
            ||K==FUInt32Property::StaticClass()||K==FUInt64Property::StaticClass()||K==FFloatProperty::StaticClass()||K==FDoubleProperty::StaticClass();
    }
    bool BindValueType(FProperty* P,FValueType& T)
    {
        T.Field=P;T.Kind=P->GetClass();
        if(T.Kind==FBoolProperty::StaticClass())
        {
            const auto* B=CastFieldChecked<FBoolProperty>(P);T.NativeBool=B->IsNativeBool();T.BoolSize=B->GetBoolFieldSize();T.BoolOffset=B->GetByteOffset();T.BoolMask=B->GetByteMask();T.BoolFieldMask=B->GetFieldMask();
            return T.BoolMask!=0&&T.BoolFieldMask!=0&&T.BoolSize>0&&int32(T.BoolOffset)<P->GetSize()&&T.BoolSize<=P->GetSize();
        }
        if(T.Kind==FNameProperty::StaticClass()||T.Kind==FStrProperty::StaticClass())return true;
        if(T.Kind==FStructProperty::StaticClass())
        {
            auto* S=CastFieldChecked<FStructProperty>(P);if(!S->Struct.IsResolved())return false;T.Struct=S->Struct.Get();if(!Pin(T.Struct))return false;
            if(T.Struct!=TBaseStructure<FVector>::Get()&&T.Struct!=TBaseStructure<FLinearColor>::Get())return false;
            if(T.Struct->GetClass()!=UScriptStruct::StaticClass()||!(T.Struct->StructFlags&STRUCT_Native)||!T.Struct->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return false;
            T.StructSize=T.Struct->GetStructureSize();T.StructFlags=T.Struct->StructFlags;return T.StructSize==P->GetElementSize();
        }
        if(T.Kind==FEnumProperty::StaticClass())
        {
            auto* E=CastFieldChecked<FEnumProperty>(P);R.Data->SetNumberField(TEXT("generated_value_enum_getter_calls"),++EnumGetterCalls);R.Data->SetBoolField(TEXT("generated_value_enum_getter_returned"),false);
            UEnum* ReturnedEnum=E->GetEnum();const TStrongObjectPtr<UEnum> HeldEnum(ReturnedEnum);R.Data->SetBoolField(TEXT("generated_value_enum_getter_returned"),true);
            if(!RootCurrent())return false;T.Enum=ReturnedEnum;if(!Pin(T.Enum))return false;T.Inner=E->GetUnderlyingProperty();
            if(!T.Inner||!PlainNumericKind(T.Inner->GetClass())||CastField<FFloatProperty>(T.Inner)||CastField<FDoubleProperty>(T.Inner)
                ||T.Inner->GetOwnerProperty()!=P||T.Inner->ArrayDim!=1||T.Inner->HasSetterOrGetter()||T.Inner->GetSize()!=P->GetSize()||T.Inner->GetElementSize()!=P->GetElementSize()||T.Inner->GetOffset_ForInternal()!=0)return false;
            T.InnerKind=T.Inner->GetClass();T.InnerFlags=T.Inner->GetPropertyFlags();T.InnerSize=T.Inner->GetSize();T.InnerOffset=T.Inner->GetOffset_ForInternal();T.InnerAlign=T.Inner->GetMinAlignment();
        }
        else if(PlainNumericKind(T.Kind))
        {
            if(auto* B=CastField<FByteProperty>(P)){if(!B->Enum.IsResolved())return false;T.Enum=B->Enum.Get();if(T.Enum&&!Pin(T.Enum))return false;}
        }
        else return false;
        if(T.Enum)
        {
            // UEnum's public raw names/values are used, never virtual display text.
            if(T.Enum->NumEnums()<1||T.Enum->NumEnums()>4096)return false;
            T.EnumForm=T.Enum->GetCppForm();
            for(int32 I=0;I<T.Enum->NumEnums();++I){T.EnumNames.Add(T.Enum->GetNameByIndex(I).ToString());T.EnumValues.Add(T.Enum->GetValueByIndex(I));}
        }
        return true;
    }
    bool ValueTypeCurrent(const FValueType& T)
    {
        // Current() has already re-found the exact own root descriptors and held
        // source/type identities before this nested descriptor is dereferenced.
        auto* P=T.Field;if(!P||P->GetClass()!=T.Kind)return false;
        if(T.Kind==FBoolProperty::StaticClass())
        {
            const auto* B=CastFieldChecked<FBoolProperty>(P);return B->IsNativeBool()==T.NativeBool&&B->GetBoolFieldSize()==T.BoolSize&&B->GetByteOffset()==T.BoolOffset&&B->GetByteMask()==T.BoolMask&&B->GetFieldMask()==T.BoolFieldMask;
        }
        if(T.Struct)
        {
            const auto* S=CastFieldChecked<FStructProperty>(P);return S->Struct.IsResolved()&&S->Struct==T.Struct&&Loaded(T.Struct)&&T.Struct->GetStructureSize()==T.StructSize&&T.Struct->StructFlags==T.StructFlags;
        }
        if(T.Kind==FEnumProperty::StaticClass())
        {
            const auto* E=CastFieldChecked<FEnumProperty>(P);R.Data->SetNumberField(TEXT("generated_value_enum_getter_calls"),++EnumGetterCalls);R.Data->SetBoolField(TEXT("generated_value_enum_getter_returned"),false);
            UEnum* ReturnedEnum=E->GetEnum();const TStrongObjectPtr<UEnum> HeldEnum(ReturnedEnum);R.Data->SetBoolField(TEXT("generated_value_enum_getter_returned"),true);
            if(!RootCurrent())return false;if(ReturnedEnum!=T.Enum||E->GetUnderlyingProperty()!=T.Inner)return false;
            if(!T.Inner||T.Inner->GetOwnerProperty()!=P||T.Inner->GetClass()!=T.InnerKind||T.Inner->GetPropertyFlags()!=T.InnerFlags
                ||T.Inner->GetSize()!=T.InnerSize||T.Inner->GetOffset_ForInternal()!=T.InnerOffset||T.Inner->GetMinAlignment()!=T.InnerAlign||T.Inner->ArrayDim!=1||T.Inner->HasSetterOrGetter())return false;
        }
        else if(T.Kind==FByteProperty::StaticClass()&&(!CastFieldChecked<FByteProperty>(P)->Enum.IsResolved()||CastFieldChecked<FByteProperty>(P)->Enum!=T.Enum))return false;
        if(T.Enum)
        {
            if(!Loaded(T.Enum)||T.Enum->NumEnums()!=T.EnumNames.Num()||T.Enum->GetCppForm()!=T.EnumForm)return false;
            for(int32 I=0;I<T.EnumNames.Num();++I)if(!T.Enum->GetNameByIndex(I).ToString().Equals(T.EnumNames[I],ESearchCase::CaseSensitive)||T.Enum->GetValueByIndex(I)!=T.EnumValues[I])return false;
        }
        return true;
    }
    // Only the exact selected Host package's Modify-driven clean-to-dirty transition is eligible.
    bool AfterHostModify(bool PackageWasDirty,decltype(GUndo) UndoBefore,bool SavedToTransaction)
    {
        if(!Context())return false;
        const FRole* HostRole=nullptr;for(const auto& A:Roles)if(A.Hold.Get()==Host.Get()){HostRole=&A;break;}
        FRole* PackageRole=nullptr;if(HostRole)for(auto& A:Roles)if(A.Hold.Get()==HostRole->Package.Get()){PackageRole=&A;break;}
        if(!HostRole||!PackageRole||!Loaded(Host.Get())||!Loaded(PackageRole->Hold.Get()))return Check();
        auto* P=HostRole->Package.Get();const auto& H=*HostRole;const auto& A=*PackageRole;
        // Reject a changed route/role before dirty/flag admission. The strict ordered guard diagnoses it.
        if(!Loaded(H.Class.Get())||(H.Outer.IsValid()&&!Loaded(H.Outer.Get()))||!Loaded(P)
            ||Host->GetClass()!=H.Class.Get()||Host->GetOuter()!=H.Outer.Get()||Host->GetPackage()!=P||P->GetPackageFlags()!=H.PackageFlags
            ||Host->GetPathName()!=H.Path||Host->GetFlags()!=H.Flags||!Loaded(A.Class.Get())||!Loaded(A.Package.Get())||(A.Outer.IsValid()&&!Loaded(A.Outer.Get()))
            ||P->GetClass()!=A.Class.Get()||P->GetOuter()!=A.Outer.Get()||P->GetPackage()!=A.Package.Get()||P->GetPathName()!=A.Path||P->GetPackageFlags()!=A.PackageFlags)return Check();
        const uint32 Before=static_cast<uint32>(A.Flags),After=static_cast<uint32>(P->GetFlags());if(Before==After)return Check();
        auto Observation=MakeShared<FJsonObject>();Observation->SetStringField(TEXT("captured_package_path"),A.Path);Observation->SetNumberField(TEXT("object_flags_before"),Before);Observation->SetNumberField(TEXT("object_flags_after"),After);
        Observation->SetBoolField(TEXT("dirty_before"),PackageWasDirty);Observation->SetBoolField(TEXT("dirty_after"),P->IsDirty());Observation->SetBoolField(TEXT("same_active_undo"),UndoBefore&&GUndo==UndoBefore);Observation->SetBoolField(TEXT("modify_saved_to_transaction"),SavedToTransaction);
        Observation->SetBoolField(TEXT("adopted"),false);Observation->SetStringField(TEXT("scope"),TEXT("New observation only; original failure object/flags and actual Modify bool were not recorded."));R.Data->SetObjectField(TEXT("generated_value_modify_package_transition"),Observation);
        const uint32 Transactional=static_cast<uint32>(RF_Transactional);
        if(PackageWasDirty||!UndoBefore||GUndo!=UndoBefore||!SavedToTransaction||!P->IsDirty()||(Before&Transactional)!=0||After!=(Before|Transactional)
            ||P==GetTransientPackage()||P->HasAnyFlags(RF_Transient)||P->HasAnyPackageFlags(PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn)||!Host->HasAnyFlags(RF_Transactional))return Check();
        const int32 PackageRoleIndex=static_cast<int32>(PackageRole-Roles.GetData());const EObjectFlags PreviousPackageRoleFlags=PackageRole->Flags;Roles[PackageRoleIndex].Flags=P->GetFlags();
        // Tentative local expectation only. Every other role, field, disk and transaction guard remains required.
        if(!Check()){Roles[PackageRoleIndex].Flags=PreviousPackageRoleFlags;return false;}
        Observation->SetBoolField(TEXT("adopted"),true);return true;
    }
    bool Fields(UObject* O,FString& Out,bool SkipSelected=false,bool AuthoredOnly=false)
    {
        if(!Current()||!Loaded(O))return false;const TStrongObjectPtr<UObject> Held(O);const TStrongObjectPtr<UClass> HeldClass(O->GetClass());
        const TStrongObjectPtr<UObject> HeldOuter(O->GetOuter());const TStrongObjectPtr<UPackage> HeldPackage(O->GetPackage());const auto Flags=O->GetFlags();const FString Path=O->GetPathName();
        Out.Reset();int32 Count=0;
        for(TFieldIterator<FProperty> It(HeldClass.Get());It;++It)
        {
            const FProperty* Field=*It;if((SkipSelected&&Field==Property)||(AuthoredOnly&&Field->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient)))continue;
            UClass* Owner=Field->GetOwnerClass();const FName FieldName=Field->GetFName();const FFieldClass* Kind=Field->GetClass();
            const uint64 FlagsBefore=Field->GetPropertyFlags();const int32 Offset=Field->GetOffset_ForInternal(),Size=Field->GetSize(),ElementSize=Field->GetElementSize(),Dim=Field->ArrayDim;
            const bool Getter=Field->HasGetter(),Setter=Field->HasSetter();const int32 ContainerSize=HeldClass->GetPropertiesSize();const int32 OwnerSize=Loaded(Owner)?Owner->GetPropertiesSize():0;
            if(++Count>2048||Dim<1||Dim>64||Offset<0||ElementSize<=0||Size<=0||int64(ElementSize)*Dim!=Size||int64(Offset)+Size>ContainerSize
                ||!Loaded(Owner)||!HeldClass->IsChildOf(Owner)||FindFProperty<FProperty>(Owner,FieldName)!=Field||int64(Offset)+Size>OwnerSize||OwnerSize>ContainerSize||!Pin(Owner))
                return Stop(TEXT("generated_value_fields_unobservable"),TEXT("Current property owner/layout/container storage bounds are incomplete."));
            if(Field->HasSetterOrGetter())
            {
                if(Owner!=UPrimitiveComponent::StaticClass()||FieldName!=TEXT("LightmapType")||Field->GetPathName()!=TEXT("/Script/Engine.PrimitiveComponent:LightmapType")
                    ||Kind!=FEnumProperty::StaticClass()||Dim!=1||ElementSize!=1||Size!=1||FlagsBefore!=6759867280785941ULL||!Getter||!Setter)
                    return Stop(TEXT("generated_value_accessor_pending"),TEXT("Only the exact observed native LightmapType readonly descriptor may precede an enum read; selected writers still reject accessors."));
                const auto* Enum=CastFieldChecked<FEnumProperty>(Field);const auto* Underlying=Enum->GetUnderlyingProperty();
                if(!Underlying||Underlying->GetClass()!=FByteProperty::StaticClass()||Underlying->ArrayDim!=1||Underlying->GetElementSize()!=1)return Stop(TEXT("generated_value_accessor_pending"),TEXT("Exact native uint8 enum storage is required before enum observation."));
                UEnum* EnumObject=Enum->GetEnum();const TStrongObjectPtr<UEnum> KeepEnum(EnumObject);
                if(!Current()||!Loaded(Owner)||FindFProperty<FProperty>(Owner,FieldName)!=Field||Field->GetOwnerClass()!=Owner||Field->GetClass()!=Kind||Field->GetPropertyFlags()!=FlagsBefore||Field->GetOffset_ForInternal()!=Offset||Field->GetSize()!=Size||Field->GetElementSize()!=ElementSize||Field->ArrayDim!=Dim
                    ||Field->HasGetter()!=Getter||Field->HasSetter()!=Setter||Owner->GetPropertiesSize()!=OwnerSize||HeldClass->GetPropertiesSize()!=ContainerSize)
                    return Stop(TEXT("generated_value_accessor_descriptor_changed"),TEXT("Readonly accessor descriptor changed during enum observation."));
                // KN3 actual metadata: the sole observed accessor is the exact native LightmapType enum.
                // Property.cpp1021-1036 selects Direct and does not invoke its declared getter/setter.
                if(!Loaded(EnumObject)||EnumObject->GetPathName()!=TEXT("/Script/Engine.ELightmapType")||Enum->GetUnderlyingProperty()!=Underlying||Underlying->GetClass()!=FByteProperty::StaticClass()||Underlying->ArrayDim!=1||Underlying->GetElementSize()!=1||!Pin(EnumObject))
                    return Stop(TEXT("generated_value_accessor_pending"),TEXT("Only the observed exact native LightmapType readonly direct-storage descriptor is admitted; selected writers still reject accessors."));
            }
            Out+=Field->GetPathName()+TEXT(":")+Kind->GetName()+TEXT(":")+Owner->GetPathName()+TEXT(":")+LexToString(FlagsBefore)+TEXT(":")+FString::Printf(TEXT("%d:%d:%d:%d:%d:%d="),Offset,Size,ElementSize,Dim,Getter,Setter);
            for(int32 Index=0;Index<Field->ArrayDim;++Index)
            {
                const void* Data=Field->ContainerPtrToValuePtr<void>(O,Index);FString Value;const bool Exported=Field->ExportText_Direct(Value,Data,Data,O,PPF_IncludeTransient);
                if(!Current()||!Loaded(O)||O->GetClass()!=HeldClass.Get()||O->GetOuter()!=HeldOuter.Get()||O->GetPackage()!=HeldPackage.Get()||O->GetFlags()!=Flags||O->GetPathName()!=Path
                    ||!Loaded(Owner)||FindFProperty<FProperty>(Owner,FieldName)!=Field||Field->GetOwnerClass()!=Owner||Field->GetClass()!=Kind||Field->GetPropertyFlags()!=FlagsBefore||Field->GetOffset_ForInternal()!=Offset
                    ||Field->GetSize()!=Size||Field->GetElementSize()!=ElementSize||Field->ArrayDim!=Dim||Field->HasGetter()!=Getter||Field->HasSetter()!=Setter||Owner->GetPropertiesSize()!=OwnerSize||HeldClass->GetPropertiesSize()!=ContainerSize)
                    return Stop(TEXT("generated_value_export_role_changed"),TEXT("Current exporter role changed; no next field export."));
                if(!Exported||Out.Len()+Value.Len()>1048576)return Stop(TEXT("generated_value_export_incomplete"),TEXT("Unconditional field export exceeded its finite bound."));Out+=Value+TEXT("\n");
            }
        }
        return true;
    }
    bool SameValue(const void* A,const void* B) const
    {
        if(PropertyKind==FStrProperty::StaticClass())
        {
            const auto* Text=CastFieldChecked<FStrProperty>(Property);
            return Text->GetPropertyValue(A).Equals(Text->GetPropertyValue(B),ESearchCase::CaseSensitive);
        }
        return Property->Identical(A,B);
    }
    bool Check()
    {
        if(!Current())return false;
        TArray<UObject*> Instances;Component->GetArchetypeInstances(Instances);if(!Current())return false;
        if(!Instances.IsEmpty())return Stop(TEXT("generated_value_archetype_consumers_pending"),TEXT("Selected archetype instances are outside this no-propagation slice; other source-class templates are not globally banned."));
        for(const TCHAR* Key:{TEXT("AssetUserData"),TEXT("AssetUserDataEditorOnly")})
        {
            const auto* A=FindFProperty<FArrayProperty>(UActorComponent::StaticClass(),Key);
            const auto* Inner=A?CastField<FObjectProperty>(A->Inner):nullptr;
            if(!A||A->GetOwnerClass()!=UActorComponent::StaticClass()||A->ArrayDim!=1||A->HasSetterOrGetter()||!Inner||!Loaded(Inner->PropertyClass)||Inner->PropertyClass->GetPathName()!=TEXT("/Script/Engine.AssetUserData")||FScriptArrayHelper(A,A->ContainerPtrToValuePtr<void>(Component.Get())).Num()!=0)
                return Stop(TEXT("generated_value_user_data_pending"),TEXT("Selected component must have empty native user-data callback arrays."));
        }
        if(!Ready)return true;
        TArray<UObject*> Inventory;GetObjectsWithOuter(Source->GetPackage(),Inventory,EGetObjectsFlags::IncludeNestedObjects);if(!Current())return false;
        if(Inventory.Num()!=SourceObjects.Num())return Stop(TEXT("generated_value_source_inventory_changed"),TEXT("Readonly source export membership changed."));
        for(auto* O:SourceObjects)if(!Inventory.Contains(O))return Stop(TEXT("generated_value_source_inventory_changed"),TEXT("Readonly source export left its role."));
        FString Now;int64 Total=0;
        for(int32 Index=0;Index<SourceObjects.Num();++Index)
        {
            if(!Fields(SourceObjects[Index],Now,false,SourceObjects[Index]!=SourceClass->GetDefaultObject(false))||!Now.Equals(SourceFields[Index],ESearchCase::CaseSensitive))
                return Stop(TEXT("generated_value_source_fields_changed"),TEXT("Readonly BPC/class/CDO/source fields changed."));
            if((Total+=Now.Len())>8388608)return Stop(TEXT("generated_value_source_bound"),TEXT("Source observation exceeds its finite total bound."));
        }
        for(const auto& N:Nodes)if(!Fields(N.Template.Get(),Now,N.Hold.Get()==Selected.Get())||!Now.Equals(N.Fields,ESearchCase::CaseSensitive))
            return Stop(TEXT("generated_value_other_fields_changed"),TEXT("Selected other or existing SCS component fields changed."));
        if(Expected&&!SameValue(Expected,Property->ContainerPtrToValuePtr<void>(Component.Get())))return Stop(TEXT("generated_value_selected_value_changed"),TEXT("Selected property differs from the observed/planned prefix."));
        TArray<FString> Dirty=DirtyPackages();Dirty.Remove(Host->GetPackage()->GetName());if(Dirty!=OtherDirty)return Stop(TEXT("generated_value_other_dirty_changed"),TEXT("A source/other package dirty state changed."));
        int64 Bytes;FString Hash;
        if(!FNiagaraTemplateScope::Disk(SourceFile,Bytes,Hash)||Bytes!=SourceBytes||Hash!=SourceHash)return Stop(TEXT("generated_value_source_disk_changed"),TEXT("Readonly source file changed."));
        if(!FNiagaraTemplateScope::Disk(TargetFile,Bytes,Hash)||Bytes!=TargetBytes||Hash!=TargetHash)return Stop(TEXT("generated_value_target_disk_changed"),TEXT("Target disk changed although this Set requests no save."));
        return Current();
    }
    bool ReadOwnValue(TSharedPtr<FJsonValue>& Out,FString& Encoding,FString& Error)
    {
        if(!Current())return false;
        if(PropertyKind!=FEnumProperty::StaticClass())return ReadComponentValue(Property,Property->ContainerPtrToValuePtr<void>(Component.Get()),MaxJsonChars,Out,Encoding,Error);
        // Use the just-validated held enum/underlying roles, not a second hidden
        // resolving GetEnum inside the generic reader after its value read.
        const auto* Numeric=static_cast<const FNumericProperty*>(RuntimeType.Inner);
        const int64 Value=Numeric->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Component.Get()));
        Out=MakeShared<FJsonValueString>(RuntimeType.Enum->GetNameStringByValue(Value));Encoding=TEXT("legacy_typed_json");return true;
    }
    bool Begin(UBlueprint* B,USCS_Node* N,const FString& InPropertyName);
};

bool FGeneratedValueScope::Begin(UBlueprint* B,USCS_Node* N,const FString& InPropertyName)
{
    Editor.Reset(GEditor);Undo=GUndo;if(!Loaded(Editor.Get()))return Stop(TEXT("generated_value_editor_unreadable"),TEXT("A loaded Editor is required."));World.Reset(Editor->GetEditorWorldContext().World());
    Host.Reset(B);SCS.Reset(B->SimpleConstructionScript);Generated.Reset(B->GeneratedClass);Skeleton.Reset(B->SkeletonGeneratedClass);Selected.Reset(N);Component.Reset(N->ComponentTemplate.Get());
    if(!Context()||!Loaded(Component.Get()))return false;SourceClass.Reset(Component->GetClass());Source.Reset(Cast<UBlueprint>(SourceClass->ClassGeneratedBy));
    SourceSkeleton.Reset(Loaded(Source.Get())?Source->SkeletonGeneratedClass.Get():nullptr);
    if(!Loaded(Source.Get())||!Loaded(SourceSkeleton.Get())||Host->ParentClass!=AActor::StaticClass()||Source->ParentClass!=UActorComponent::StaticClass()
        ||SourceClass->GetSuperClass()!=UActorComponent::StaticClass()||SourceClass->IsChildOf(USceneComponent::StaticClass())||!Cast<UBlueprintGeneratedClass>(SourceClass.Get())
        ||Source->GetPackage()==Host->GetPackage()||!Source->GetPackage()->GetName().StartsWith(TEXT("/Game/"))||Source->GetPackage()->IsDirty()
        ||Source->BlueprintType!=BPTYPE_Normal||(Source->Status!=BS_UpToDate&&Source->Status!=BS_UpToDateWithWarnings))
        return Fail(R,TEXT("generated_value_source_rejected"),TEXT("Use an already-loaded saved-clean compiled direct ActorComponent BPC and an own native Actor host template."));
    SourceStatus=Source->Status;NodeList=SCS->GetAllNodes();Roots=SCS->GetRootNodes();if(NodeList.Num()>MaxNodes)return Fail(R,TEXT("component_budget_exceeded"),TEXT("SCS node bound exceeded."));
    for(auto* Q:NodeList)
    {
        if(!Loaded(Q)||!Loaded(Q->ComponentTemplate.Get()))return Stop(TEXT("generated_value_scs_unreadable"),TEXT("Existing own SCS role is unreadable."));
        FNode A;A.Hold.Reset(Q);A.Template.Reset(Q->ComponentTemplate);A.Class.Reset(Q->ComponentClass);A.Name=Q->GetVariableName().ToString();A.Guid=Q->VariableGuid;A.Children=Q->GetChildNodes();Nodes.Add(MoveTemp(A));
    }
    OtherDirty=DirtyPackages();OtherDirty.Remove(Host->GetPackage()->GetName());
    for(UObject* O:{static_cast<UObject*>(Host.Get()),static_cast<UObject*>(SCS.Get()),static_cast<UObject*>(Generated.Get()),static_cast<UObject*>(Skeleton.Get()),static_cast<UObject*>(Host->GetPackage()),static_cast<UObject*>(Source->GetPackage()),static_cast<UObject*>(UActorComponent::StaticClass()),UActorComponent::StaticClass()->GetDefaultObject(false)})if(!Pin(O))return false;
    GetObjectsWithOuter(Source->GetPackage(),SourceObjects,EGetObjectsFlags::IncludeNestedObjects);if(SourceObjects.Num()>4096)return Fail(R,TEXT("generated_value_source_bound"),TEXT("Source inventory exceeds 4096 objects."));
    for(auto* O:SourceObjects)if(!Pin(O))return false;for(const auto& A:Nodes)if(!Pin(A.Hold.Get())||!Pin(A.Template.Get()))return false;
    if(!SourceObjects.Contains(Source.Get())||!SourceObjects.Contains(SourceClass.Get())||!SourceObjects.Contains(SourceSkeleton.Get())||!SourceObjects.Contains(SourceClass->GetDefaultObject(false)))return Fail(R,TEXT("generated_value_source_inventory_incomplete"),TEXT("Source BPC/runtime/skeleton/existing CDO must remain in the captured source exports."));
    if(!Loaded(SourceClass->GetDefaultObject(false))||!SourceClass->GetDefaultObject(false)->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Fail(R,TEXT("generated_value_source_cdo_required"),TEXT("Existing source CDO is required without allocation."));
    if(!Current())return false;
    Property=FindProperty(SourceClass.Get(),InPropertyName);PropertyName=Property?Property->GetName():InPropertyName;SkeletonProperty=FindProperty(SourceSkeleton.Get(),PropertyName);
    const auto RootLayout=[](const FProperty* P,const UClass* Owner)
    {return P&&P->GetOwnerClass()==Owner&&P->ArrayDim==1&&P->GetSize()>0&&P->GetSize()<=65536&&P->GetMinAlignment()>0&&P->GetMinAlignment()<=256&&FMath::IsPowerOfTwo(P->GetMinAlignment())&&P->GetElementSize()==P->GetSize()&&P->GetOffset_ForInternal()>=0
        &&int64(P->GetOffset_ForInternal())+P->GetSize()<=Owner->GetPropertiesSize()&&!P->HasSetterOrGetter();};
    if(!RootLayout(Property,SourceClass.Get())||!RootLayout(SkeletonProperty,SourceSkeleton.Get())||Property->GetClass()!=SkeletonProperty->GetClass()
        ||!Property->HasAllPropertyFlags(CPF_Edit)||Property->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly|CPF_EditConst|CPF_DisableEditOnTemplate|CPF_InstancedReference|CPF_ContainsInstancedReference))
        return Fail(R,TEXT("generated_value_property_unsupported"),TEXT("An exact own editable non-instanced scalar declaration and matching current generated/skeleton layout are required; inherited/accessor/container/custom values remain pending."));
    PropertyKind=Property->GetClass();
    PropertyFlags=Property->GetPropertyFlags();SkeletonFlags=SkeletonProperty->GetPropertyFlags();PropertyOffset=Property->GetOffset_ForInternal();SkeletonOffset=SkeletonProperty->GetOffset_ForInternal();PropertySize=Property->GetSize();PropertyAlignment=Property->GetMinAlignment();SkeletonAlignment=SkeletonProperty->GetMinAlignment();
    int32 Matches=0;for(const auto& V:Source->NewVariables)if(V.VarName==Property->GetFName()){++Matches;VariableGuid=V.VarGuid;}
    if(Matches!=1||!VariableGuid.IsValid())return Fail(R,TEXT("generated_value_own_declaration_required"),TEXT("A unique current source own declaration/GUID is required."));
    if(!RootCurrent())return false;
    if(!BindValueType(Property,RuntimeType)||!BindValueType(SkeletonProperty,SkeletonType))
        return Stopped?false:Fail(R,TEXT("generated_value_property_type_pending"),TEXT("Supported own declarations use built-in bool/numeric/name/string/enum descriptors or exact native Vector/LinearColor; other valid original types remain incomplete."));
    TypesBound=true;ValueStruct=RuntimeType.Struct;if(!Current())return false;
    Original=MakeUnique<FPreparedProperty>();Original->Property=Property;Original->bPlainGeneratedValue=true;Original->bPlainGeneratedString=PropertyKind==FStrProperty::StaticClass();Original->Value=FMemory::Malloc(Property->GetSize(),Property->GetMinAlignment());Property->InitializeValue(Original->Value);Property->CopyCompleteValue(Original->Value,Property->ContainerPtrToValuePtr<void>(Component.Get()));Expected=Original->Value;
    if(!FNiagaraTemplateScope::File(Source.Get(),SourceFile)||!FNiagaraTemplateScope::Disk(SourceFile,SourceBytes,SourceHash)||SourceBytes<=0
        ||!FNiagaraTemplateScope::File(Host.Get(),TargetFile)||!FNiagaraTemplateScope::Disk(TargetFile,TargetBytes,TargetHash))return Fail(R,TEXT("generated_value_disk_baseline_unobservable"),TEXT("Bounded source/target disk baseline is required."));
    SourceFields.SetNum(SourceObjects.Num());int64 Total=0;
    for(int32 Index=0;Index<SourceObjects.Num();++Index){if(!Fields(SourceObjects[Index],SourceFields[Index],false,SourceObjects[Index]!=SourceClass->GetDefaultObject(false)))return false;if((Total+=SourceFields[Index].Len())>8388608)return Fail(R,TEXT("generated_value_source_bound"),TEXT("Source fields exceed 8Mi characters."));}
    for(auto& A:Nodes)if(!Fields(A.Template.Get(),A.Fields,A.Hold.Get()==Selected.Get()))return false;Ready=true;if(!Check())return false;
    auto Inspection=GeneratedComponent::InspectSavedClass(SourceClass.Get());R.Data->SetObjectField(TEXT("generated_component_inspection"),Inspection);if(!Check())return false;
    if(!Inspection->GetBoolField(TEXT("success")))return Fail(R,TEXT("generated_value_source_inspection_failed"),TEXT("Existing saved class/dependency inspection rejected the source; no writer dispatched."));
    const bool Allowed=PropertyAccessUtil::CanSetPropertyValue(Property,PropertyAccessUtil::EditorReadOnlyFlags,true)==EPropertyAccessResultFlags::Success&&Component->CanEditChange(Property);if(!Check())return false;
    if(!Allowed)return Fail(R,TEXT("component_property_readonly"),TEXT("Current edit permission rejected the field."));
    auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("source_blueprint"),Source->GetPathName());Meta->SetStringField(TEXT("class"),SourceClass->GetPathName());Meta->SetStringField(TEXT("property"),Property->GetPathName());Meta->SetStringField(TEXT("field_class"),Property->GetClass()->GetName());Meta->SetStringField(TEXT("flags_string"),LexToString(PropertyFlags));Meta->SetStringField(TEXT("variable_guid"),VariableGuid.ToString(EGuidFormats::Digits));Meta->SetStringField(TEXT("source_disk_sha1"),SourceHash);R.Data->SetObjectField(TEXT("generated_value_source_before"),Meta);return true;
}

bool MutateGeneratedValue(UBlueprint* BP,USCS_Node* Node,const FString& Name,const TSharedPtr<FJsonValue>& Json,const FReadResult& Before,FMutationResult& R)
{
    if(Before.bLoadedDuringRead){R.bChangeStateKnown=false;return Fail(R,TEXT("generated_value_preload_required"),TEXT("Generated value Set requires an already loaded host; preflight may have performed load work, and no writer was dispatched."));}
    FGeneratedValueScope Scope(R);TUniquePtr<FPreparedProperty> Prepared;TUniquePtr<FScopedTransaction> Transaction;const auto OuterUndo=GUndo;
    const bool DirtyBefore=BP->GetPackage()->IsDirty();const EBlueprintStatus StatusBefore=BP->Status;bool Begun=false,Stored=false;
    R.Data->SetBoolField(TEXT("generated_value_authored_name_admission"),true);R.Data->SetBoolField(TEXT("generated_value_native_descriptor_dispatch"),true);
    R.Data->SetStringField(TEXT("generated_value_scope"),TEXT("own fresh non-scene direct ActorComponent generated template; arbitrary authored own field names with bounded built-in bool/numeric/name/string/enum or native Vector/LinearColor declarations; source BPC/class/CDO and SCS/other fields protected; no source-default mutation or propagation"));
    const bool Run=[&]() -> bool
    {
        if(!Scope.Begin(BP,Node,Name))return false;Begun=true;
        Prepared=MakeUnique<FPreparedProperty>();auto& Q=*Prepared;Q.Property=Scope.Property;Q.Requested=Json;Q.bPlainGeneratedValue=true;Q.bPlainGeneratedString=Scope.PropertyKind==FStrProperty::StaticClass();Q.Value=FMemory::Malloc(Q.Property->GetSize(),Q.Property->GetMinAlignment());Q.Property->InitializeValue(Q.Value);
        bool Converted=false;
        if(auto* B=CastField<FBoolProperty>(Q.Property);B&&Json->Type==EJson::Boolean){B->SetPropertyValue(Q.Value,Json->AsBool());Converted=true;}
        else if(auto* E=CastField<FEnumProperty>(Q.Property)){int64 Value=0;if(EnumValue(Scope.RuntimeType.Enum,Json,Value))Converted=NumericValue(static_cast<FNumericProperty*>(Scope.RuntimeType.Inner),static_cast<double>(Value),Q.Value);}
        else if(auto* Number=CastField<FNumericProperty>(Q.Property);Number&&Json->Type==EJson::Number)Converted=NumericValue(Number,Json->AsNumber(),Q.Value);
        else if(auto* N=CastField<FNameProperty>(Q.Property);N&&Json->Type==EJson::String&&SafeName(Json->AsString(),true)){N->SetPropertyValue(Q.Value,FName(*Json->AsString()));Converted=true;}
        else if(auto* Text=CastField<FStrProperty>(Q.Property);Text&&Json->Type==EJson::String){Text->SetPropertyValue(Q.Value,Json->AsString());Converted=true;}
        else if(auto* Vector=CastField<FStructProperty>(Q.Property);Vector&&Vector->Struct==TBaseStructure<FVector>::Get())
        {
            double XYZ[3];if(VectorValues(Json,3,XYZ,true)){if(Json->Type==EJson::Number){const float LegacyScalar=static_cast<float>(Json->AsNumber());XYZ[0]=XYZ[1]=XYZ[2]=LegacyScalar;}*static_cast<FVector*>(Q.Value)=FVector(XYZ[0],XYZ[1],XYZ[2]);Converted=true;}
        }
        else if(auto* Color=CastField<FStructProperty>(Q.Property);Color&&Color->Struct==TBaseStructure<FLinearColor>::Get())
        {double RGBA[4];if(VectorValues(Json,4,RGBA)){*static_cast<FLinearColor*>(Q.Value)=FLinearColor(RGBA[0],RGBA[1],RGBA[2],RGBA[3]);Converted=true;}}
        if(!Converted)return Fail(R,TEXT("component_property_value_unsupported"),TEXT("The selected own declaration requires its finite typed JSON value; numeric coercion/out-of-range, arbitrary structs/references/containers remain incomplete, with no writer dispatched."));
        FString Error;if(!Scope.ReadOwnValue(Q.Before,Q.Encoding,Error))return Scope.Stop(TEXT("generated_value_before_unobservable"),TEXT("Selected value preflight is unreadable."));if(!Scope.Check())return false;
        if(Scope.SameValue(Q.Value,Q.Property->ContainerPtrToValuePtr<void>(Scope.Component.Get()))){R.Data->SetBoolField(TEXT("no_op"),true);return true;}
        R.Stage=TEXT("mutation");R.bMutationAttempted=true;R.bChangeStateKnown=false;
        R.Data->SetBoolField(TEXT("generated_value_transaction_begin_attempted"),true);Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("Cubeless","GeneratedComponentValue","Cubeless generated component value"));Scope.Undo=GUndo;R.Data->SetBoolField(TEXT("generated_value_transaction_begin_returned"),true);if(!Scope.Check())return false;
        const bool PackageWasDirtyBeforeHostModify=BP->GetPackage()->IsDirty();const auto UndoBeforeHostModify=GUndo;
        R.Data->SetBoolField(TEXT("generated_value_blueprint_modify_attempted"),true);const bool HostSavedToTransaction=BP->Modify();R.Data->SetBoolField(TEXT("generated_value_blueprint_modify_returned"),true);
        R.Data->SetBoolField(TEXT("generated_value_blueprint_saved_to_transaction"),HostSavedToTransaction);if(!Scope.AfterHostModify(PackageWasDirtyBeforeHostModify,UndoBeforeHostModify,HostSavedToTransaction))return false;
        R.Data->SetBoolField(TEXT("generated_value_scs_modify_attempted"),true);Scope.SCS->Modify();R.Data->SetBoolField(TEXT("generated_value_scs_modify_returned"),true);if(!Scope.Check())return false;
        R.Data->SetBoolField(TEXT("generated_value_node_modify_attempted"),true);Scope.Selected->Modify();R.Data->SetBoolField(TEXT("generated_value_node_modify_returned"),true);if(!Scope.Check())return false;
        const bool Numeric=CastField<FNumericProperty>(Q.Property)!=nullptr||CastField<FEnumProperty>(Q.Property)!=nullptr;
        if(Numeric)
        {
            R.Data->SetBoolField(TEXT("generated_value_pre_edit_attempted"),true);Scope.Component->PreEditChange(Q.Property);R.Data->SetBoolField(TEXT("generated_value_pre_edit_returned"),true);if(!Scope.Check())return false;
        }
        else if(CastField<FNameProperty>(Q.Property))
        {
            R.Data->SetBoolField(TEXT("generated_value_template_modify_attempted"),true);Scope.Component->Modify();R.Data->SetBoolField(TEXT("generated_value_template_modify_returned"),true);if(!Scope.Check())return false;
        }
        // Numeric lifecycle returns through current descriptor validation before reacquiring storage.
        Scope.Expected=Q.Value;Q.Property->CopyCompleteValue(Q.Property->ContainerPtrToValuePtr<void>(Scope.Component.Get()),Q.Value);Stored=true;
        auto Prefix=MakeShared<FJsonObject>();Prefix->SetStringField(TEXT("property_name"),Q.Property->GetName());Prefix->SetField(TEXT("before"),Q.Before);Prefix->SetField(TEXT("requested"),Q.Requested);Prefix->SetBoolField(TEXT("store_returned"),true);R.Data->SetObjectField(TEXT("generated_value_applied_prefix"),Prefix);
        if(!Scope.Check())return false;
        if(Numeric)
        {
            FPropertyChangedEvent Event(Q.Property,EPropertyChangeType::ValueSet);Event.MemberProperty=Q.Property;R.Data->SetBoolField(TEXT("generated_value_post_edit_attempted"),true);Scope.Component->PostEditChangeProperty(Event);R.Data->SetBoolField(TEXT("generated_value_post_edit_returned"),true);if(!Scope.Check())return false;
        }
        R.Data->SetBoolField(TEXT("generated_value_mark_modified_attempted"),true);FBlueprintEditorUtils::MarkBlueprintAsModified(BP);R.Data->SetBoolField(TEXT("generated_value_mark_modified_returned"),true);return Scope.Check();
    }();
    if(!Begun)return false;
    if(Transaction)
    {
        const bool SafeBeforeEnd=!Scope.Stopped&&Scope.Check();R.Data->SetBoolField(TEXT("generated_value_transaction_end_attempted"),true);Transaction.Reset();Scope.Undo=OuterUndo;R.Data->SetBoolField(TEXT("generated_value_transaction_end_returned"),true);
        if(!SafeBeforeEnd||Scope.Stopped||!Scope.Check()){R.bChangeStateKnown=false;return false;}
    }
    if(Scope.Stopped||!Scope.Check())return false;
    if(!Prepared||!Prepared->Before){R.bSuccess=false;R.bChanged=DirtyBefore!=BP->GetPackage()->IsDirty()||StatusBefore!=BP->Status;R.bChangeStateKnown=true;return false;}FString Error,Encoding;TSharedPtr<FJsonValue> Value;
    if(!Scope.ReadOwnValue(Value,Encoding,Error)||!Scope.Check())return Scope.Stop(TEXT("generated_value_after_unobservable"),TEXT("Current endpoint is unreadable; prefix remains without retry."));
    const bool Matches=Scope.SameValue(Prepared->Value,Prepared->Property->ContainerPtrToValuePtr<void>(Scope.Component.Get()));if(!Scope.Check())return false;
    auto Observation=MakeShared<FJsonObject>();Observation->SetStringField(TEXT("property_name"),Prepared->Property->GetName());Observation->SetField(TEXT("requested"),Prepared->Requested);Observation->SetField(TEXT("before"),Prepared->Before);Observation->SetField(TEXT("after"),Value);Observation->SetStringField(TEXT("encoding"),Encoding);Observation->SetBoolField(TEXT("postcondition_passed"),Matches);R.Data->SetArrayField(TEXT("property_observations"),{MakeShared<FJsonValueObject>(Observation)});
    bool ValueChanged=!FJsonValue::CompareEqual(*Prepared->Before,*Value);
    if(Scope.PropertyKind==FStrProperty::StaticClass()){ValueChanged=!Scope.SameValue(Scope.Original->Value,Prepared->Property->ContainerPtrToValuePtr<void>(Scope.Component.Get()));if(!Scope.Check())return false;}
    R.bChanged=DirtyBefore!=BP->GetPackage()->IsDirty()||StatusBefore!=BP->Status||ValueChanged;R.bChangeStateKnown=true;R.bSuccess=Run&&Matches;R.bPostconditionPassed=R.bSuccess;
    R.Data->SetBoolField(TEXT("dirty_before"),DirtyBefore);R.Data->SetBoolField(TEXT("dirty_after"),BP->GetPackage()->IsDirty());R.Data->SetBoolField(TEXT("generated_value_store_returned"),Stored);R.Data->SetBoolField(TEXT("source_bpc_class_cdo_observations_preserved"),true);R.Data->SetBoolField(TEXT("source_and_target_disk_unchanged"),true);
    R.Data->SetStringField(TEXT("component_class"),Scope.SourceClass->GetPathName());R.Data->SetStringField(TEXT("template_path"),Scope.Component->GetPathName());R.Data->SetArrayField(TEXT("dirty_packages_after"),Strings(DirtyPackages()));
    R.Data->SetStringField(TEXT("generated_value_unobserved_scope"),TEXT("Immediate stored state and bounded source/reflected roles only; no arbitrary global callback/private-cache purity, source whole-pipeline parity, runtime or cold-persistence proof. No compile/save/propagation/rollback/retry requested."));
    if(R.bSuccess)R.Stage=TEXT("complete");else if(R.ErrorCode.IsEmpty())Fail(R,TEXT("component_postcondition_failed"),TEXT("Generated selected value failed the endpoint contract."));return R.bSuccess;
}

// NM: Add-only PCGComponent. Existing generic Set/class policy stays unchanged.
bool PCGComponentName(const FString& Name)
{ return Name == TEXT("PCGComponent") || Name == TEXT("UPCGComponent") || Name == TEXT("PCG") || Name == TEXT("/Script/PCG.PCGComponent"); }

struct FPCGTemplateAddScope
{
    struct FObjectRole { TStrongObjectPtr<UObject> Hold; UClass* Class; UObject* Outer; UPackage* Package; FString Path; EObjectFlags Flags; };
    struct FFieldRole { FProperty* P; FFieldClass* Kind; FFieldVariant Owner; uint64 Flags; int32 Offset, Size, Dim; };
    struct FClassRole { TStrongObjectPtr<UClass> Class; EClassFlags Flags; UClass* Super; UPackage* Package; uint32 PackageFlags; TArray<FProperty*> Inventory; TArray<FFieldRole> Fields; };
    struct FNodeRole { TStrongObjectPtr<USCS_Node> Node; TStrongObjectPtr<UActorComponent> Template; UClass* Class; FGuid Guid; FString Name, Path, Fields; TArray<USCS_Node*> Children; };
    FMutationResult& R; bool Stopped = false, Attached = false, SelectedReady = false, FieldsReady = false, SelectedValuesReady = false;
    TStrongObjectPtr<UEditorEngine> Editor; TStrongObjectPtr<UWorld> World; TStrongObjectPtr<UBlueprint> BP;
    TStrongObjectPtr<USimpleConstructionScript> SCS; TStrongObjectPtr<UClass> Generated, Skeleton, NativeClass;
    TStrongObjectPtr<UPCGComponent> Default, Component; TStrongObjectPtr<UPCGGraphInstance> DefaultInstance, Instance;
    TStrongObjectPtr<USCS_Node> Selected; TArray<FObjectRole> SourceRoles; TArray<FClassRole> Classes;
    TArray<TStrongObjectPtr<UObject>> Retained; TArray<FNodeRole> Old; TArray<USCS_Node*> Roots;
    TArray<FString> SourceValues, DirtyBefore; FString Name, SourcePath, TargetFile, TargetHash, SelectedFields, InstanceFields;
    int64 TargetBytes = -1; FGuid SelectedGuid; decltype(GUndo) Undo = nullptr;
    explicit FPCGTemplateAddScope(FMutationResult& In) : R(In) {}
    static bool Loaded(const UObject* O) { return FNiagaraTemplateScope::Loaded(O); }
    FString Address(const void* P) const { return FString::Printf(TEXT("%p"), P); }
    bool Stop(const TCHAR* Code, const TCHAR* Message, const FString& Object = FString(), const FString& Property = FString(), const FString& Expected = FString(), const FString& Actual = FString())
    {
        if (!Stopped)
        {
            Stopped = true; R.bChangeStateKnown = false; R.bSuccess = R.bPostconditionPassed = false;
            auto F = MakeShared<FJsonObject>(); F->SetStringField(TEXT("stage"), R.Stage); F->SetStringField(TEXT("object"), Object.IsEmpty() ? SourcePath : Object); F->SetStringField(TEXT("property"), Property.IsEmpty() ? Code : *Property); F->SetStringField(TEXT("expected"), Expected.IsEmpty() ? TEXT("captured scoped condition satisfied") : Expected); F->SetStringField(TEXT("actual"), Actual.IsEmpty() ? TEXT("condition failed; value not observed without post-failure SDK") : Actual); F->SetBoolField(TEXT("expected_actual_observed"), !Expected.IsEmpty() && !Actual.IsEmpty());
            R.Data->SetObjectField(TEXT("pcg_first_failure"), F); R.Data->SetBoolField(TEXT("pcg_followup_sdk_stopped"), true);
            if (R.ErrorCode.IsEmpty()) Fail(R, Code, Message);
        }
        return false;
    }
    bool Context()
    {
        if (Stopped) return false;
        if (!IsEditorGameThread() || GExitPurge || IsEngineExitRequested() || IsGarbageCollecting() || UE::IsSavingPackage() || IsPlaySessionActive() || !Loaded(Editor.Get()) || GEditor != Editor.Get() || !Loaded(World.Get()) || World->bIsTearingDown || GEditor->GetEditorWorldContext().World() != World.Get() || GUndo != Undo)
            return Stop(TEXT("component_pcg_context_changed"), TEXT("Captured safe Editor/world/transaction changed; no further SDK read/write/retry"));
        return true;
    }
    bool ClassSource(UClass* K)
    {
        if (!Loaded(K)) return false;
        if (Classes.ContainsByPredicate([&](const FClassRole& X) { return X.Class.Get() == K; })) return true;
        if (!Loaded(K->GetPackage()) || !K->HasAnyClassFlags(CLASS_Native) || K->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || K->ClassGeneratedBy || !K->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn) || K->GetPackage()->IsDirty()) return false;
        FClassRole C; C.Class.Reset(K); C.Flags = K->GetClassFlags(); C.Super = K->GetSuperClass(); C.Package = K->GetPackage(); C.PackageFlags = C.Package->GetPackageFlags(); Retained.Emplace(C.Package);
        for (TFieldIterator<FProperty> It(K); It; ++It) { FProperty* P = *It; C.Inventory.Add(P); C.Fields.Add({P, P->GetClass(), P->GetOwnerVariant(), P->GetPropertyFlags(), P->GetOffset_ForInternal(), P->GetSize(), P->ArrayDim}); Retained.Emplace(P->GetOwnerStruct()); }
        Classes.Add(MoveTemp(C)); return !K->GetSuperClass() || ClassSource(K->GetSuperClass());
    }
    bool SourceObject(UObject* O)
    {
        if (!Context() || !Loaded(O) || !ClassSource(O->GetClass())) return false;
        FObjectRole V; V.Hold.Reset(O); V.Class = O->GetClass(); V.Outer = O->GetOuter(); V.Package = O->GetPackage(); V.Path = O->GetPathName(); V.Flags = O->GetFlags(); Retained.Emplace(V.Package); SourceRoles.Add(MoveTemp(V)); return true;
    }
    bool Raw(bool AllowNewClassRoles = false, bool SkipSlotQuery = false)
    {
        if (!Context()) return false;
        for (const auto& C : Classes)
        {
            UClass* K = C.Class.Get(); if (!Loaded(K) || K->GetClassFlags() != C.Flags || K->GetSuperClass() != C.Super || K->GetPackage() != C.Package || C.Package->GetPackageFlags() != C.PackageFlags || C.Package->IsDirty()) return Stop(TEXT("component_pcg_native_source_changed"), TEXT("Captured native class/package flags or ancestry changed"));
            TArray<FProperty*> Inventory; for (TFieldIterator<FProperty> It(K); It; ++It) Inventory.Add(*It); if (Inventory != C.Inventory) return Stop(TEXT("component_pcg_native_fields_changed"), TEXT("Captured reflected field inventory changed"));
            for (const auto& P : C.Fields) if (P.P->GetClass() != P.Kind || P.P->GetOwnerVariant() != P.Owner || P.P->GetPropertyFlags() != P.Flags || P.P->GetOffset_ForInternal() != P.Offset || P.P->GetSize() != P.Size || P.P->ArrayDim != P.Dim) return Stop(TEXT("component_pcg_native_layout_changed"), TEXT("Captured property owner/layout changed"));
        }
        for (const auto& S : SourceRoles)
        { UObject* O = S.Hold.Get(); if (!Loaded(O) || O->GetClass() != S.Class || O->GetOuter() != S.Outer || O->GetPackage() != S.Package || O->GetFlags() != S.Flags || !O->GetPathName().Equals(S.Path, ESearchCase::CaseSensitive)) return Stop(TEXT("component_pcg_default_source_changed"), TEXT("Readonly default/default-subobject identity changed"), S.Path, TEXT("class/outer/package/path/flags")); }
        if (Default.IsValid() && DefaultInstance.IsValid())
        {
            if (!NativeClass->ClassDefaultObject.IsResolved() || NativeClass->ClassDefaultObject != Default.Get()) return Stop(TEXT("component_pcg_native_cdo_changed"), TEXT("Captured existing native CDO reference changed"));
            auto* P = FindFProperty<FObjectProperty>(NativeClass.Get(), TEXT("GraphInstance")); if (!P || P->GetOwnerClass() != NativeClass.Get() || P->ArrayDim != 1 || P->HasSetterOrGetter() || !P->HasAnyPropertyFlags(CPF_InstancedReference)) return Stop(TEXT("component_pcg_instance_field_changed"), TEXT("Exact native instanced GraphInstance descriptor unavailable"));
            const auto* H = P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(Default.Get()));
            if (!H->IsResolved() || *H != DefaultInstance.Get() || !DefaultInstance->Graph.IsResolved() || DefaultInstance->Graph != nullptr) return Stop(TEXT("component_pcg_default_instance_changed"), TEXT("Readonly native default graph-instance reference changed"));
        }
        if (!Loaded(BP.Get()) || BP->ParentClass != AActor::StaticClass() || BP->SimpleConstructionScript != SCS.Get() || !Loaded(SCS.Get()) || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->bIsRegeneratingOnLoad) return Stop(TEXT("component_pcg_target_roles_changed"), TEXT("Current own Blueprint/SCS role changed"));
        if (!AllowNewClassRoles && (BP->GeneratedClass != Generated.Get() || BP->SkeletonGeneratedClass != Skeleton.Get())) return Stop(TEXT("component_pcg_generated_role_changed"), TEXT("Current generated/skeleton roles changed outside a compiler return"));
        if (!Loaded(Generated.Get()) || !Loaded(Skeleton.Get()) || Generated->ClassGeneratedBy != BP.Get() || Skeleton->ClassGeneratedBy != BP.Get()) return Stop(TEXT("component_pcg_generated_role_invalid"), TEXT("Current own generated/skeleton class is unavailable"));
        const auto All = SCS->GetAllNodes(); TArray<USCS_Node*> ExpectedRoots = Roots; if (Attached) ExpectedRoots.Add(Selected.Get());
        if (All.Num() != Old.Num() + (Attached ? 1 : 0) || SCS->GetRootNodes() != ExpectedRoots) return Stop(TEXT("component_pcg_scs_membership_changed"), TEXT("Existing SCS membership/root order differs from exact append plan"));
        TSet<USCS_Node*> Seen; for (auto* N : All) if (!N || Seen.Contains(N)) return Stop(TEXT("component_pcg_scs_duplicate"), TEXT("Null/duplicate SCS node")); else Seen.Add(N);
        for (const auto& O : Old)
        { auto* N = O.Node.Get(); if (!Loaded(N) || !All.Contains(N) || N->GetOuter() != SCS.Get() || N->VariableGuid != O.Guid || N->ComponentClass != O.Class || !N->GetVariableName().ToString().Equals(O.Name, ESearchCase::CaseSensitive) || !N->GetPathName().Equals(O.Path, ESearchCase::CaseSensitive) || N->ComponentTemplate != O.Template.Get() || N->GetChildNodes() != O.Children || !Loaded(O.Template.Get()) || O.Template->GetClass() != O.Class || O.Template->GetOuter() != Generated.Get()) return Stop(TEXT("component_pcg_existing_scs_changed"), TEXT("Existing node/template/GUID/name/children changed"), O.Path, TEXT("existing_scs_role")); }
        if (SelectedReady)
        {
            if (!Loaded(Selected.Get()) || Selected->GetOuter() != SCS.Get() || Selected->VariableGuid != SelectedGuid || !Selected->GetVariableName().ToString().Equals(Name, ESearchCase::CaseSensitive) || Selected->ComponentClass != NativeClass.Get() || Selected->ComponentTemplate != Component.Get() || !Selected->GetChildNodes().IsEmpty() || !Loaded(Component.Get()) || Component->GetClass() != NativeClass.Get() || Component->GetOuter() != Generated.Get() || Component->GetPackage() != BP->GetPackage() || !Component->HasAnyFlags(RF_ArchetypeObject) || Component->HasAnyFlags(RF_ClassDefaultObject) || Component->GetOwner() || Component->GetWorld() || Component->IsRegistered() || Component->IsPreRegistering() || Component->HasBegunPlay()) return Stop(TEXT("component_pcg_template_role_changed"), TEXT("Current fresh non-scene template role changed"));
            auto* P = FindFProperty<FObjectProperty>(NativeClass.Get(), TEXT("GraphInstance")); if (!P || P->GetOwnerClass() != NativeClass.Get() || P->ArrayDim != 1 || P->HasSetterOrGetter()) return Stop(TEXT("component_pcg_instance_field_changed"), TEXT("Exact native GraphInstance descriptor unavailable"));
            const auto* H = P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(Component.Get()));
            if (!H->IsResolved() || *H != Instance.Get() || !Loaded(Instance.Get()) || Instance->GetClass() != UPCGGraphInstance::StaticClass() || Instance->GetOuter() != Component.Get() || Instance->GetPackage() != BP->GetPackage() || !Instance->HasAnyFlags(RF_DefaultSubObject) || Instance->HasAnyFlags(RF_ClassDefaultObject) || !Instance->Graph.IsResolved() || Instance->Graph != nullptr) return Stop(TEXT("component_pcg_instance_role_changed"), TEXT("Fresh own GraphInstance must remain its native unbound default subobject, not a CDO"));
        }
        if (!SkipSlotQuery)
        {
            UObject* Slot = FindObject<UObject>(Generated.Get(), *(Name + USimpleConstructionScript::ComponentTemplateNameSuffix)); const TStrongObjectPtr<UObject> KeepSlot(Slot);
            // Repeat callback-free held roles after the lookup, before the
            // next SDK. No new occupant is adopted or relocated.
            if (!Raw(AllowNewClassRoles, true)) return false;
            UObject* Expected = Component.Get();
            if (Slot != Expected) return Stop(TEXT("component_pcg_planned_slot_changed"), TEXT("Planned template slot is not empty/our exact returned template; no orphan eviction permitted"), SourcePath, Name + USimpleConstructionScript::ComponentTemplateNameSuffix, Address(Expected), Address(Slot));
        }
        return Context();
    }
    bool Fields(UObject* O, FString& Text)
    {
        if (!Raw() || !Loaded(O)) return false; const TStrongObjectPtr<UObject> Keep(O); const TStrongObjectPtr<UClass> KeepClass(O->GetClass());
        TArray<FProperty*> FieldList; for (TFieldIterator<FProperty> It(O->GetClass()); It; ++It) if (It->HasAnyPropertyFlags(CPF_Edit) && !It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) FieldList.Add(*It);
        Text.Reset(); if (FieldList.Num() > 2048) return Stop(TEXT("component_pcg_fields_bound"), TEXT("Editable field observation exceeds existing bound"));
        for (FProperty* P : FieldList)
        {
            if (!Raw()) return false; if (P->ArrayDim < 1 || P->ArrayDim > 64) return Stop(TEXT("component_pcg_field_unsupported"), TEXT("Static array observation outside existing bound"));
            UEnum* E = nullptr; TStrongObjectPtr<UEnum> KeepEnum;
            if (P->HasSetterOrGetter()) { const auto* EP = CastField<FEnumProperty>(P); E = EP ? EP->GetEnum() : nullptr; if (Loaded(E)) KeepEnum.Reset(E); if (!Raw() || !FNiagaraTemplateScope::ReadableRawStorage(O, P, E)) return Stop(TEXT("component_pcg_field_accessor_unsupported"), TEXT("Custom accessor observation is unsupported except existing exact LightmapType readonly storage")); }
            for (int32 I = 0; I < P->ArrayDim; ++I)
            { FString Value; const void* Data = P->ContainerPtrToValuePtr<void>(O, I); const bool Exported = P->ExportText_Direct(Value, Data, Data, O, PPF_None); if (!Raw()) return false; if (!Exported || Text.Len() + Value.Len() > 1048576) return Stop(TEXT("component_pcg_fields_unobservable"), TEXT("Editable value export unavailable/bounded; prefix retained")); Text += P->GetPathName() + TEXT("=") + Value + TEXT("\n"); }
        }
        return Raw();
    }
    bool Check()
    {
        if (!Raw()) return false; if (!FieldsReady) return true;
        for (int32 I = 0; I < SourceRoles.Num(); ++I) { FString Now; if (!Fields(SourceRoles[I].Hold.Get(), Now)) return false; if (!Now.Equals(SourceValues[I], ESearchCase::CaseSensitive)) return Stop(TEXT("component_pcg_source_values_changed"), TEXT("Readonly native CDO/default-subobject authored values changed"), SourceRoles[I].Path, TEXT("editable_fields"), SourceValues[I].Left(4096), Now.Left(4096)); }
        for (const auto& O : Old) { FString Now; if (!Fields(O.Template.Get(), Now)) return false; if (!Now.Equals(O.Fields, ESearchCase::CaseSensitive)) return Stop(TEXT("component_pcg_existing_values_changed"), TEXT("Existing component editable values changed"), O.Path, TEXT("editable_fields"), O.Fields.Left(4096), Now.Left(4096)); }
        if (SelectedReady && SelectedValuesReady) { FString A, B; if (!Fields(Component.Get(), A) || !Fields(Instance.Get(), B)) return false; if (!A.Equals(SelectedFields, ESearchCase::CaseSensitive) || !B.Equals(InstanceFields, ESearchCase::CaseSensitive)) return Stop(TEXT("component_pcg_new_values_changed"), TEXT("Fresh template/default-subobject authored values changed outside constructor result")); }
        return Raw();
    }
    bool Bind(UBlueprint* B, UClass* K, const FString& RequestedName)
    {
        Undo = GUndo; Editor.Reset(GEditor); World.Reset(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr); BP.Reset(B); Name = RequestedName; SourcePath = B->GetPathName();
        if (!Context() || B->ParentClass != AActor::StaticClass() || !Loaded(B->GeneratedClass.Get()) || !Loaded(B->SkeletonGeneratedClass.Get()) || !Loaded(B->SimpleConstructionScript.Get())) return Stop(TEXT("component_pcg_native_actor_required"), TEXT("Requires already loaded normal own Actor Blueprint with exact native Actor parent"));
        Generated.Reset(B->GeneratedClass); Skeleton.Reset(B->SkeletonGeneratedClass); SCS.Reset(B->SimpleConstructionScript); NativeClass.Reset(K); Roots = SCS->GetRootNodes();
        if (!Loaded(K) || !K->GetPathName().Equals(TEXT("/Script/PCG.PCGComponent"), ESearchCase::CaseSensitive) || K->GetSuperClass() != UActorComponent::StaticClass() || K->HasAnyClassFlags(CLASS_Abstract) || !ClassSource(K) || !ClassSource(UPCGGraphInstance::StaticClass())) return Stop(TEXT("component_pcg_native_source_invalid"), TEXT("Exact loaded native runtime PCGComponent/GraphInstance class required"));
        const auto Initial = SCS->GetAllNodes(); if (Initial.Num() > MaxNodes) return false;
        for (auto* N : Initial) { if (!Loaded(N) || !N->ComponentTemplate.IsResolved() || !Loaded(N->ComponentTemplate.Get())) return Stop(TEXT("component_pcg_initial_scs_unavailable"), TEXT("Current own component template unavailable")); FNodeRole O; O.Node.Reset(N); O.Template.Reset(N->ComponentTemplate.Get()); O.Class = N->ComponentClass.Get(); O.Name = N->GetVariableName().ToString(); O.Guid = N->VariableGuid; O.Path = N->GetPathName(); O.Children = N->GetChildNodes(); if (!ClassSource(O.Class)) return Stop(TEXT("component_pcg_existing_source_unsupported"), TEXT("Existing component class must be native and source-observable")); Old.Add(MoveTemp(O)); }
        if (!Raw()) return false;
        UObject* CDOReturned = K->GetDefaultObject(false); if (Loaded(CDOReturned)) Default.Reset(Cast<UPCGComponent>(CDOReturned)); if (!Raw() || !Default.IsValid() || Default->GetClass() != K || !Default->HasAnyFlags(RF_ClassDefaultObject)) return Stop(TEXT("component_pcg_native_cdo_unavailable"), TEXT("Existing native CDO required; no source default creation/warming"));
        UPCGGraphInstance* D = Default->GetGraphInstance(); if (Loaded(D)) DefaultInstance.Reset(D); if (!Raw() || !DefaultInstance.IsValid() || D->GetOuter() != Default.Get() || D->GetClass() != UPCGGraphInstance::StaticClass() || !D->HasAnyFlags(RF_DefaultSubObject) || D->HasAnyFlags(RF_ClassDefaultObject) || !D->Graph.IsResolved() || D->Graph != nullptr) return Stop(TEXT("component_pcg_default_instance_invalid"), TEXT("Native CDO must own its distinct unbound native default subobject"));
        if (!SourceObject(Default.Get()) || !SourceObject(DefaultInstance.Get())) return false;
        if (!Raw()) return false; for (const auto& S : SourceRoles) { FString V; if (!Fields(S.Hold.Get(), V)) return false; SourceValues.Add(V); } for (auto& O : Old) if (!Fields(O.Template.Get(), O.Fields)) return false;
        FieldsReady = true; DirtyBefore = DirtyPackages(); if (!FNiagaraTemplateScope::File(B, TargetFile) || !FNiagaraTemplateScope::Disk(TargetFile, TargetBytes, TargetHash)) return Stop(TEXT("component_pcg_target_disk_unavailable"), TEXT("Target file state unavailable before writer"));
        return Check();
    }
    bool Adopt(USCS_Node* N)
    {
        if (!Context() || !Loaded(N)) return Stop(TEXT("component_pcg_created_node_invalid"), TEXT("CreateNode returned no ordinary node")); Selected.Reset(N); Retained.Emplace(N);
        if (!N->ComponentTemplate.IsResolved()) return Stop(TEXT("component_pcg_created_template_unresolved"), TEXT("Returned component template handle unresolved"));
        UPCGComponent* T = Cast<UPCGComponent>(N->ComponentTemplate.Get()); if (Loaded(T)) Component.Reset(T); if (!Check()) return false;
        if (!Component.IsValid() || N->GetOuter() != SCS.Get() || N->ComponentClass != NativeClass.Get() || T->GetClass() != NativeClass.Get() || T->GetOuter() != Generated.Get() || !N->VariableGuid.IsValid() || !N->GetVariableName().ToString().Equals(Name, ESearchCase::CaseSensitive)) return Stop(TEXT("component_pcg_created_template_invalid"), TEXT("Returned template/declaration does not match exact planned class/name/ownership"));
        SelectedGuid = N->VariableGuid;
        auto* P = FindFProperty<FObjectProperty>(NativeClass.Get(), TEXT("GraphInstance")); if (!P || P->ArrayDim != 1 || P->HasSetterOrGetter()) return Stop(TEXT("component_pcg_created_instance_field_invalid"), TEXT("Returned instance descriptor unavailable")); const auto* H = P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(T));
        if (!H->IsResolved() || !Loaded(H->Get())) return Stop(TEXT("component_pcg_created_instance_unresolved"), TEXT("Returned self-owned subobject unavailable")); Instance.Reset(Cast<UPCGGraphInstance>(H->Get())); SelectedReady = true;
        if (!Check()) return false; UPCGGraphInstance* Returned = T->GetGraphInstance(); const TStrongObjectPtr<UPCGGraphInstance> KeepReturned(Returned); if (!Check() || Returned != Instance.Get()) return Stop(TEXT("component_pcg_created_instance_changed"), TEXT("Public getter differs from pre-captured self-owned instance"));
        if (!Raw()) return false; const bool Observed = Fields(T, SelectedFields) && Fields(Instance.Get(), InstanceFields); SelectedValuesReady = Observed;
        return Observed && Check();
    }
    bool Reacquire()
    {
        if (!Context()) return false;
        UClass* G = BP->GeneratedClass.Get(); UClass* K = BP->SkeletonGeneratedClass.Get(); if (!Loaded(G) || !Loaded(K) || G->ClassGeneratedBy != BP.Get() || K->ClassGeneratedBy != BP.Get() || G->GetSuperClass() != AActor::StaticClass() || K->GetSuperClass() != AActor::StaticClass() || G->GetPackage() != BP->GetPackage() || K->GetPackage() != BP->GetPackage()) return Stop(TEXT("component_pcg_compiler_classes_invalid"), TEXT("Compiler returned no current own Actor class roles"));
        Retained.Emplace(Generated.Get()); Retained.Emplace(Skeleton.Get()); Generated.Reset(G); Skeleton.Reset(K);
        for (auto& O : Old) { if (!Loaded(O.Node.Get()) || !O.Node->ComponentTemplate.IsResolved()) return Stop(TEXT("component_pcg_compiler_old_template_unavailable"), TEXT("Compiler current old template unavailable")); UActorComponent* T = O.Node->ComponentTemplate.Get(); if (!Loaded(T) || T->GetClass() != O.Class || T->GetOuter() != G) return Stop(TEXT("component_pcg_compiler_old_role_changed"), TEXT("Old declaration class/ownership changed")); Retained.Emplace(O.Template.Get()); O.Template.Reset(T); }
        if (!Selected->ComponentTemplate.IsResolved()) return Stop(TEXT("component_pcg_compiler_template_unresolved"), TEXT("Current selected template unresolved")); auto* T = Cast<UPCGComponent>(Selected->ComponentTemplate.Get()); if (!Loaded(T) || T->GetClass() != NativeClass.Get() || T->GetOuter() != G) return Stop(TEXT("component_pcg_compiler_template_invalid"), TEXT("Current selected class/ownership changed"));
        Retained.Emplace(Component.Get()); Retained.Emplace(Instance.Get()); Component.Reset(T);
        auto* P = FindFProperty<FObjectProperty>(NativeClass.Get(), TEXT("GraphInstance")); if (!P || P->ArrayDim != 1 || P->HasSetterOrGetter()) return Stop(TEXT("component_pcg_compiler_instance_field_invalid"), TEXT("Current GraphInstance descriptor unavailable")); const auto* H = P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(T)); if (!H->IsResolved() || !Loaded(H->Get())) return Stop(TEXT("component_pcg_compiler_instance_invalid"), TEXT("Current owned GraphInstance unavailable")); Instance.Reset(Cast<UPCGGraphInstance>(H->Get()));
        if (!Check()) return false; UPCGGraphInstance* GI = T->GetGraphInstance(); const TStrongObjectPtr<UPCGGraphInstance> KeepGI(GI); if (!Check() || GI != Instance.Get()) return Stop(TEXT("component_pcg_compiler_instance_changed"), TEXT("Current public instance getter differs from held declaration"));
        return Check();
    }
};

bool AddNativePCG(UBlueprint* B, UClass* K, const FString& Name, const FString& TransformJson, const FReadResult& Before, FMutationResult& R)
{
    if (Before.bLoadedDuringRead) { R.bChangeStateKnown = false; return Fail(R, TEXT("component_pcg_target_preload_required"), TEXT("Preload the own target explicitly; no component writer dispatched after implicit target load")); }
    FPCGTemplateAddScope S(R); if (!S.Bind(B, K, Name)) return false;
    if (!S.Check()) return false; const bool NoExternal = RejectExternalInstances(B, R); if (!S.Check() || !NoExternal) return false;
    R.Data->SetBoolField(TEXT("native_pcg_component_add"), true); R.Data->SetBoolField(TEXT("pcg_transform_ignored"), true); R.Data->SetStringField(TEXT("pcg_requested_transform_json"), TransformJson); R.Data->SetStringField(TEXT("pcg_transform_policy"), TEXT("UE56 non-scene UActorComponent: validated transform inputs are accepted but no transform setter is called")); R.Data->SetBoolField(TEXT("pcg_graph_assignment_attempted"), false); R.Data->SetBoolField(TEXT("pcg_generation_attempted"), false);
    bool Created = false, Run = false; const bool Dirty = B->GetPackage()->IsDirty();
    auto Step = [&](const TCHAR* Stage, auto&& Call) { if (!S.Check()) return false; R.Stage = Stage; R.Data->SetStringField(TEXT("pcg_last_sdk_attempted"), Stage); Call(); R.Data->SetStringField(TEXT("pcg_last_sdk_returned"), Stage); return S.Check(); };
    Run = [&]()
    {
        R.bMutationAttempted = true; R.bChangeStateKnown = false;
        if (!Step(TEXT("Blueprint_Modify"), [&] { B->Modify(); }) || !Step(TEXT("SCS_Modify"), [&] { S.SCS->Modify(); })) return false;
        if (!S.Check()) return false;
        R.Stage = TEXT("SCS_name_current"); const FName CurrentName = S.SCS->GenerateNewComponentName(K, FName(*Name)); if (!S.Raw()) return false; if (CurrentName != FName(*Name)) return S.Stop(TEXT("component_pcg_planned_name_changed"), TEXT("Exact requested name changed before CreateNode"), Before.BlueprintPath, TEXT("GenerateNewComponentName"), Name, CurrentName.ToString());
        if (!S.Raw()) return false; R.Stage = TEXT("SCS_CreateNode"); R.Data->SetBoolField(TEXT("pcg_create_node_attempted"), true);
        USCS_Node* N = S.SCS->CreateNode(K, FName(*Name)); R.Data->SetBoolField(TEXT("pcg_create_node_returned"), true); Created = N != nullptr; R.bChanged |= Created; R.Data->SetBoolField(TEXT("creation_observed"), Created);
        if (!S.Adopt(N)) return false; R.Data->SetStringField(TEXT("created_node_path"), N->GetPathName()); R.Data->SetStringField(TEXT("created_node_guid"), N->VariableGuid.ToString(EGuidFormats::Digits)); R.Data->SetStringField(TEXT("created_template_path"), S.Component->GetPathName());
        R.Stage = TEXT("SCS_AddNode"); R.Data->SetBoolField(TEXT("pcg_add_node_attempted"), true); S.SCS->AddNode(N); R.Data->SetBoolField(TEXT("pcg_add_node_returned"), true); S.Attached = true; if (!S.Check()) return false;
        if (!Step(TEXT("Node_Modify"), [&] { N->Modify(); }) || !Step(TEXT("Template_Modify"), [&] { S.Component->Modify(); })) return false;
        R.Stage = TEXT("structural_skeleton_refresh"); R.Data->SetBoolField(TEXT("explicit_skeleton_compile_attempted"), true); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B); R.Data->SetBoolField(TEXT("pcg_structural_refresh_returned"), true); if (!S.Reacquire()) return false;
        const FObjectPropertyBase* Member = FindFProperty<FObjectPropertyBase>(S.Skeleton.Get(), FName(*Name)); const bool Matches = Member && Member->GetOwnerClass() == S.Skeleton.Get() && Member->PropertyClass == K && Member->HasAnyPropertyFlags(CPF_BlueprintVisible); R.Data->SetBoolField(TEXT("skeleton_component_property_verified"), Matches); if (!Matches) return Fail(R, TEXT("component_pcg_skeleton_mismatch"), TEXT("Added component remains but current own skeleton property was not verified"));
        if (R.bCompileRequested) { if (!S.Check()) return false; R.Stage = TEXT("compile"); R.bCompileAttempted = true; auto C = Cubeless::UE58::CompileBlueprint(Before.BlueprintPath, false, false); bool Compiled = false; C->TryGetBoolField(TEXT("compiled"), Compiled); R.Compiled = Compiled; R.Data->SetObjectField(TEXT("compile"), C); if (!S.Reacquire()) return false; if (!Compiled) return Fail(R, TEXT("component_compile_failed"), TEXT("PCG SCS Add prefix retained; full compile failed, no save/rollback/retry")); }
        return true;
    }();
    if (S.Stopped || !S.Check()) return false;
    int64 Bytes; FString Hash; if (!FNiagaraTemplateScope::Disk(S.TargetFile, Bytes, Hash) || Bytes != S.TargetBytes || Hash != S.TargetHash) return S.Stop(TEXT("component_pcg_unrequested_save"), TEXT("Target file changed although component Add never requests save"));
    FReadResult After; FString Code, Error; if (!ReadLocalComponents(Before.BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, After, Code, Error) || !S.Check()) return S.Stop(TEXT("component_pcg_after_unobservable"), TEXT("Final local SCS readback unavailable; exact prefix retained"));
    R.Data->SetObjectField(TEXT("after"), After.Data); R.Data->SetBoolField(TEXT("dirty_before"), Dirty); R.Data->SetBoolField(TEXT("dirty_after"), B->GetPackage()->IsDirty()); R.Data->SetStringField(TEXT("component_class"), TEXT("/Script/PCG.PCGComponent")); R.Data->SetStringField(TEXT("template_path"), S.Component->GetPathName()); R.Data->SetStringField(TEXT("parent_component"), FString());
    auto Instance = MakeShared<FJsonObject>(); Instance->SetStringField(TEXT("path"), S.Instance->GetPathName()); Instance->SetStringField(TEXT("class"), S.Instance->GetClass()->GetPathName()); Instance->SetStringField(TEXT("outer"), S.Component->GetPathName()); Instance->SetBoolField(TEXT("owned_default_subobject"), true); Instance->SetBoolField(TEXT("class_default_object"), false); Instance->SetBoolField(TEXT("graph_bound"), false); R.Data->SetObjectField(TEXT("pcg_graph_instance"), Instance);
    const auto DirtyAfter = DirtyPackages(); bool OnlyTargetDirty = true; for (const auto& P : DirtyAfter) if (!S.DirtyBefore.Contains(P) && P != B->GetPackage()->GetName()) OnlyTargetDirty = false;
    R.Data->SetArrayField(TEXT("dirty_packages_after"), Strings(DirtyAfter)); R.Data->SetBoolField(TEXT("existing_scs_observations_preserved"), true); R.Data->SetBoolField(TEXT("pcg_source_default_subobjects_preserved"), true); R.Data->SetBoolField(TEXT("automatic_cleanup_attempted"), false); R.Data->SetStringField(TEXT("cleanup_policy"), TEXT("No automatic rollback/removal or retry; actual newly created node/template and compiler prefix remain observable"));
    R.bChanged = Created || Dirty != B->GetPackage()->IsDirty(); R.bChangeStateKnown = true; R.bSuccess = R.bPostconditionPassed = Run && Created && After.NodeVisits == Before.NodeVisits + 1 && OnlyTargetDirty;
    R.Data->SetStringField(TEXT("pcg_scope"), TEXT("Add-only exact native PCGComponent, own non-scene SCS root, empty initial properties, ignored transform triples, exact self-owned unbound native GraphInstance default subobject, current own skeleton property and optional full compile. Graph assignment/SetGraphLocal, refresh/cleanup/generation, live/component instances, arbitrary initial properties and original full PCG pipeline remain incomplete; no global cache/purity/runtime proof."));
    if (R.bSuccess) R.Stage = TEXT("complete"); else if (R.ErrorCode.IsEmpty()) Fail(R, TEXT("component_postcondition_failed"), TEXT("PCG Add final SCS/preservation condition failed; actual partial state retained"));
    return R.bSuccess;
}

// NP: one existing own SCS PCG template. NM Add and generic Set remain unchanged.
struct FPCGTemplateConfigureScope
{
    using FNodeRole = FPCGTemplateAddScope::FNodeRole;
    struct FValue
    {
        TStrongObjectPtr<UObject> Object; TStrongObjectPtr<UClass> Class;
        FProperty* Property = nullptr; FString Path, Name; void* Memory = nullptr;
        ~FValue() { if (Memory) Property->DestroyAndFreeValue(Memory); }
    };
    struct FMember { TStrongObjectPtr<UPCGNode> Node; TStrongObjectPtr<UPCGSettings> Settings; TArray<TObjectPtr<UPCGPin>> Inputs, Outputs; };
    struct FPin { TStrongObjectPtr<UPCGPin> Pin; UPCGNode* Node = nullptr; TArray<TObjectPtr<UPCGEdge>> Edges; };
    struct FEdge { TStrongObjectPtr<UPCGEdge> Edge; UPCGPin* Input = nullptr; UPCGPin* Output = nullptr; };
    FMutationResult& R; FPCGTemplateAddScope Source;
    TStrongObjectPtr<UBlueprint> BP; TStrongObjectPtr<USimpleConstructionScript> SCS;
    TStrongObjectPtr<UClass> Generated, Skeleton; TStrongObjectPtr<USCS_Node> Selected;
    TStrongObjectPtr<UPCGComponent> Component, Default; TStrongObjectPtr<UPCGGraphInstance> Instance, DefaultInstance;
    TStrongObjectPtr<UPCGGraph> Graph; TStrongObjectPtr<UEnum> TriggerEnum;
    FEnumProperty* TriggerField = nullptr; FBoolProperty* RegenerateField = nullptr; FBoolProperty* DropField = nullptr; FObjectProperty* InstanceField = nullptr; FProperty* TriggerChild = nullptr;
    TArray<TPair<FName, int64>> EnumRows; bool EnumIsFlags = false, EnumIsReplaced = false;
    TArray<FNodeRole> Old; TArray<USCS_Node*> Roots, Declarations; TArray<FMember> Members; TArray<FPin> Pins; TArray<FEdge> Edges; TArray<UPCGNode*> Authored;
    TArray<TUniquePtr<FValue>> Values; TArray<FBPVariableDescription> Variables;
    TArray<TSharedPtr<FJsonValue>> Prefix; TArray<FString> DirtyBefore; FString Name, BPPath, GraphPath, BPFile, GraphFile, BPHash, GraphHash, GraphSnapshot;
    int64 BPBytes = -1, GraphBytes = -1, SnapshotBytes = 0; bool Ready = false, ExpectedRegenerate = false, ExpectedDrop = false;
    EPCGComponentGenerationTrigger ExpectedTrigger = EPCGComponentGenerationTrigger::GenerateOnLoad;
    UPCGGraphInterface* ExpectedGraph = nullptr;
    // IsValid means a backing struct exists, not that the bag contains properties.
    struct FBagState { bool Valid = false; int32 Count = -1, Descriptors = -1; const UPropertyBag* Type = nullptr; FString TypePath; };
    FBagState SourceBag, ExpectedInstanceBag; FInstancedPropertyBag SourceEmptyParameters;
    TStrongObjectPtr<UPropertyBag> SourceBagType, InitialInstanceBagType;
    EStructFlags SourceBagFlags = STRUCT_NoFlags; UStruct* SourceBagSuper = nullptr; int32 SourceBagSize = 0, SourceBagAlignment = 0;
    bool ParametersReady = false; FValue* InstanceParameterSnapshot = nullptr;
    explicit FPCGTemplateConfigureScope(FMutationResult& In) : R(In), Source(In) {}
    static bool Loaded(const UObject* O) { return FPCGTemplateAddScope::Loaded(O); }
    FString Address(const void* P) const { return FString::Printf(TEXT("%p"), P); }
    bool Stop(const TCHAR* Code, const FString& Property, const FString& Expected, const FString& Actual, const FString& Object = FString())
    {
        const bool Answer = Source.Stop(Code, TEXT("Exact PCG template configure condition failed; actual prefix retained, no follow-up SDK/retry"), Object.IsEmpty() ? BPPath : Object, Property, Expected, Actual);
        const TSharedPtr<FJsonObject>* Failure = nullptr;
        if (R.Data->TryGetObjectField(TEXT("pcg_first_failure"), Failure) && Failure && Failure->IsValid())
        { (*Failure)->SetBoolField(TEXT("expected_actual_observed"), false); (*Failure)->SetStringField(TEXT("observation_scope"), TEXT("compound_guard_condition_not_independent_per_property_readback; retained expected/actual are diagnostic details")); }
        return Answer;
    }
    FString BagDetails(const FBagState& B) const
    { return FString::Printf(TEXT("IsValid=%s; property_count=%d; descriptor_count=%d; struct_path=%s; struct_address=%p"), B.Valid ? TEXT("true") : TEXT("false"), B.Count, B.Descriptors, *B.TypePath, B.Type); }
    TSharedRef<FJsonObject> BagReceipt(const FBagState& B, int32 OverrideCount = -1) const
    { auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("is_valid"), B.Valid); J->SetNumberField(TEXT("property_count"), B.Count); J->SetNumberField(TEXT("descriptor_count"), B.Descriptors); J->SetStringField(TEXT("struct_path"), B.TypePath); J->SetStringField(TEXT("struct_address"), Address(B.Type)); if (OverrideCount >= 0) J->SetNumberField(TEXT("override_id_count"), OverrideCount); return J; }
    bool ObserveBag(const FInstancedPropertyBag& Bag, FBagState& Out, const FString& Role)
    {
        if (!Source.Context()) return false;
        Out.Valid = Bag.IsValid(); Out.Type = Bag.GetPropertyBagStruct();
        if (Out.Type && !Loaded(Out.Type)) { Out.TypePath = TEXT("unavailable: struct is not safely loaded"); R.Data->SetObjectField(TEXT("parameter_bag_failure_observation"), BagReceipt(Out)); return Stop(TEXT("pcg_template_bag_struct_unavailable"), Role, TEXT("loaded empty bag struct or invalid/null bag"), BagDetails(Out)); }
        Out.TypePath = Out.Type ? Out.Type->GetPathName() : TEXT("None");
        Out.Count = Bag.GetNumPropertiesInBag(); Out.Descriptors = Out.Type ? Out.Type->GetPropertyDescs().Num() : 0;
        return Source.Context();
    }
    bool EmptyBag(const FBagState& B) const { return B.Count == 0 && B.Descriptors == 0 && B.Valid == (B.Type != nullptr); }
    bool SameBag(const FBagState& A, const FBagState& B) const { return A.Valid == B.Valid && A.Count == B.Count && A.Descriptors == B.Descriptors && A.Type == B.Type && A.TypePath == B.TypePath; }
    bool ObserveInstanceBag(FBagState& Out)
    {
        if (!ObserveBag(Instance->ParametersOverrides.Parameters, Out, TEXT("ParametersOverrides.Parameters"))) return false;
        const int32 IDs = Instance->ParametersOverrides.PropertiesIDsOverridden.Num();
        if (!EmptyBag(Out) || IDs != 0 || (ParametersReady && !SameBag(Out, ExpectedInstanceBag)))
        {
            auto Receipt = BagReceipt(Out, IDs); TArray<TSharedPtr<FJsonValue>> ObservedIDs; for (const FGuid& ID : Instance->ParametersOverrides.PropertiesIDsOverridden) ObservedIDs.Add(MakeShared<FJsonValueString>(ID.ToString())); Receipt->SetArrayField(TEXT("override_ids"), ObservedIDs); R.Data->SetObjectField(TEXT("parameter_bag_failure_observation"), Receipt);
            return Stop(TEXT("pcg_template_parameter_scope_changed"), TEXT("ParametersOverrides.Parameters/PropertiesIDsOverridden"), ParametersReady ? BagDetails(ExpectedInstanceBag) + TEXT("; override_id_count=0") : TEXT("zero properties and override IDs; valid empty and invalid bags are distinct allowed initial states"), BagDetails(Out) + FString::Printf(TEXT("; override_id_count=%d"), IDs));
        }
        return true;
    }
    bool BindEmptyParameters()
    {
        const FInstancedPropertyBag* Parameters = Graph->GetUserParametersStruct(); if (!Raw()) return false;
        if (!Parameters) return Stop(TEXT("pcg_template_graph_parameters_pending"), TEXT("graph user parameters"), TEXT("public parameter bag pointer"), TEXT("nullptr"), GraphPath);
        if (!ObserveBag(*Parameters, SourceBag, TEXT("graph.UserParameters"))) return false;
        R.Data->SetObjectField(TEXT("source_parameter_bag_before"), BagReceipt(SourceBag));
        if (!EmptyBag(SourceBag)) { R.Data->SetObjectField(TEXT("parameter_bag_failure_observation"), BagReceipt(SourceBag)); return Stop(TEXT("pcg_template_graph_parameters_pending"), TEXT("graph.UserParameters"), TEXT("property_count=0; descriptor_count=0; IsValid agrees with backing struct presence"), BagDetails(SourceBag), GraphPath); }
        if (SourceBag.Type)
        {
            SourceBagType.Reset(const_cast<UPropertyBag*>(SourceBag.Type)); if (!Remember(SourceBagType.Get())) return Stop(TEXT("pcg_template_source_bag_role_unavailable"), TEXT("graph.UserParameters.struct"), TEXT("held readonly source bag struct"), BagDetails(SourceBag), GraphPath);
            SourceBagFlags = SourceBagType->StructFlags; SourceBagSuper = SourceBagType->GetSuperStruct(); SourceBagSize = SourceBagType->GetStructureSize(); SourceBagAlignment = SourceBagType->GetMinAlignment();
        }
        SourceEmptyParameters = *Parameters; if (!Raw()) return false;
        if (!ObserveInstanceBag(ExpectedInstanceBag)) return false;
        if (ExpectedInstanceBag.Type) InitialInstanceBagType.Reset(const_cast<UPropertyBag*>(ExpectedInstanceBag.Type));
        R.Data->SetObjectField(TEXT("instance_parameter_bag_before"), BagReceipt(ExpectedInstanceBag, 0)); ParametersReady = true;
        return Raw();
    }
    bool ExpectedEmptyParameterTransition()
    {
        // This is a planned scratch expectation derived from the captured source,
        // never a post-callback baseline read. Preserve every other override member.
        if (Source.Stopped) return false;
        if (!ParametersReady || !InstanceParameterSnapshot || !InstanceParameterSnapshot->Memory) return Stop(TEXT("pcg_template_parameter_snapshot_missing"), TEXT("ParametersOverrides"), TEXT("captured whole override struct"), TEXT("missing"));
        auto* Expected = static_cast<FPCGOverrideInstancedPropertyBag*>(InstanceParameterSnapshot->Memory);
        Expected->Parameters = SourceEmptyParameters; ExpectedInstanceBag = SourceBag;
        R.Data->SetBoolField(TEXT("empty_parameter_bag_expected_transition"), true);
        return true;
    }
    bool Raw(bool Reconstructing = false)
    {
        if (!Source.Context()) return false;
        for (const auto& C : Source.Classes)
        {
            UClass* K = C.Class.Get();
            if (!Loaded(K) || K->GetClassFlags() != C.Flags || K->GetSuperClass() != C.Super || K->GetPackage() != C.Package || C.Package->GetPackageFlags() != C.PackageFlags || C.Package->IsDirty()) return Stop(TEXT("pcg_template_native_source_changed"), TEXT("native_class/package"), TEXT("captured flags/ancestry/clean native package"), TEXT("mismatch"));
            TArray<FProperty*> CurrentFields; for (TFieldIterator<FProperty> It(K); It; ++It) CurrentFields.Add(*It);
            if (CurrentFields != C.Inventory) return Stop(TEXT("pcg_template_native_inventory_changed"), TEXT("field_inventory.count"), LexToString(C.Inventory.Num()), LexToString(CurrentFields.Num()));
            for (const auto& P : C.Fields) if (P.P->GetClass() != P.Kind || P.P->GetOwnerVariant() != P.Owner || P.P->GetPropertyFlags() != P.Flags || P.P->GetOffset_ForInternal() != P.Offset || P.P->GetSize() != P.Size || P.P->ArrayDim != P.Dim) return Stop(TEXT("pcg_template_native_layout_changed"), TEXT("field_owner/layout"), TEXT("captured native descriptor"), TEXT("mismatch"));
        }
        for (const auto& O : Source.SourceRoles)
        { auto* V = O.Hold.Get(); if (!Loaded(V) || V->GetClass() != O.Class || V->GetOuter() != O.Outer || V->GetPackage() != O.Package || V->GetFlags() != O.Flags || !V->GetPathName().Equals(O.Path, ESearchCase::CaseSensitive)) return Stop(TEXT("pcg_template_readonly_role_changed"), TEXT("class/outer/package/path/flags"), TEXT("captured readonly role"), Address(V), O.Path); }
        if (!Loaded(BP.Get()) || BP->ParentClass != AActor::StaticClass() || BP->SimpleConstructionScript != SCS.Get() || !Loaded(SCS.Get()) || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->bIsRegeneratingOnLoad) return Stop(TEXT("pcg_template_blueprint_role_changed"), TEXT("Blueprint/SCS"), TEXT("same idle own Actor Blueprint"), TEXT("mismatch"));
        if (!Reconstructing && (BP->GeneratedClass != Generated.Get() || BP->SkeletonGeneratedClass != Skeleton.Get())) return Stop(TEXT("pcg_template_generated_role_changed"), TEXT("generated/skeleton"), TEXT("held roles outside reconstruction"), TEXT("mismatch"));
        const auto All = SCS->GetAllNodes(); if (All != Declarations || All.Num() != Old.Num() || SCS->GetRootNodes() != Roots) return Stop(TEXT("pcg_template_scs_membership_changed"), TEXT("SCS node/root count"), LexToString(Old.Num()), LexToString(All.Num()));
        TSet<USCS_Node*> Seen; for (auto* N : All) if (!N || Seen.Contains(N)) return Stop(TEXT("pcg_template_scs_duplicate"), TEXT("SCS membership"), TEXT("unique nonnull nodes"), Address(N)); else Seen.Add(N);
        for (const auto& O : Old)
        {
            auto* N = O.Node.Get(); if (!Loaded(N) || !All.Contains(N) || N->GetOuter() != SCS.Get() || N->VariableGuid != O.Guid || N->ComponentClass != O.Class || !N->GetVariableName().ToString().Equals(O.Name, ESearchCase::CaseSensitive) || N->GetChildNodes() != O.Children) return Stop(TEXT("pcg_template_scs_declaration_changed"), TEXT("SCS GUID/name/class/children"), O.Name, TEXT("mismatch"), O.Path);
            if (!Reconstructing && (N->ComponentTemplate != O.Template.Get() || !Loaded(O.Template.Get()) || O.Template->GetClass() != O.Class || O.Template->GetOuter() != Generated.Get())) return Stop(TEXT("pcg_template_scs_template_changed"), TEXT("ComponentTemplate"), Address(O.Template.Get()), TEXT("current role differs"), O.Path);
        }
        if (BP->NewVariables.Num() != Variables.Num()) return Stop(TEXT("pcg_template_variables_changed"), TEXT("NewVariables.count"), LexToString(Variables.Num()), LexToString(BP->NewVariables.Num()));
        for (int32 I = 0; I < Variables.Num(); ++I) if (!FBPVariableDescription::StaticStruct()->CompareScriptStruct(&BP->NewVariables[I], &Variables[I], 0)) return Stop(TEXT("pcg_template_variables_changed"), TEXT("NewVariables.slot"), LexToString(I), TEXT("descriptor differs"));
        if (!Source.Context()) return false;
        if (Graph->GetNodes() != Authored || Graph->GetPackage()->IsDirty()) return Stop(TEXT("pcg_template_source_graph_changed"), TEXT("authored_nodes/dirty"), TEXT("captured graph; dirty=false"), TEXT("mismatch"), GraphPath);
        if (ParametersReady)
        {
            const FInstancedPropertyBag* Parameters = Graph->GetUserParametersStruct(); FBagState Now;
            if (!Parameters) return Stop(TEXT("pcg_template_source_bag_changed"), TEXT("graph.UserParameters"), BagDetails(SourceBag), TEXT("nullptr"), GraphPath);
            if (!ObserveBag(*Parameters, Now, TEXT("graph.UserParameters"))) return false;
            if (!SameBag(Now, SourceBag) || !EmptyBag(Now)) { R.Data->SetObjectField(TEXT("parameter_bag_failure_observation"), BagReceipt(Now)); return Stop(TEXT("pcg_template_source_bag_changed"), TEXT("graph.UserParameters"), BagDetails(SourceBag), BagDetails(Now), GraphPath); }
            if (SourceBagType.IsValid() && (SourceBagType->StructFlags != SourceBagFlags || SourceBagType->GetSuperStruct() != SourceBagSuper || SourceBagType->GetStructureSize() != SourceBagSize || SourceBagType->GetMinAlignment() != SourceBagAlignment)) return Stop(TEXT("pcg_template_source_bag_layout_changed"), TEXT("graph.UserParameters.struct layout"), TEXT("captured flags/super/size/alignment and zero descriptors"), TEXT("native layout differs"), SourceBag.TypePath);
        }
        for (const auto& M : Members) if (M.Node->GetInputPins() != M.Inputs || M.Node->GetOutputPins() != M.Outputs) return Stop(TEXT("pcg_template_source_pins_changed"), TEXT("physical_pin_inventory"), TEXT("captured input/output membership"), TEXT("mismatch"), GraphPath);
        for (const auto& P : Pins) if (P.Pin->Node != P.Node || P.Pin->Edges != P.Edges) return Stop(TEXT("pcg_template_source_edges_changed"), TEXT("pin.Node/Edges"), LexToString(P.Edges.Num()), LexToString(P.Pin->Edges.Num()), GraphPath);
        for (const auto& E : Edges) if (E.Edge->InputPin != E.Input || E.Edge->OutputPin != E.Output || !E.Input->Edges.Contains(E.Edge.Get()) || !E.Output->Edges.Contains(E.Edge.Get())) return Stop(TEXT("pcg_template_source_edge_role_changed"), TEXT("reciprocal_edge"), TEXT("captured endpoint pair"), Address(E.Edge.Get()), GraphPath);
        if (TriggerEnum.IsValid())
        {
            if (TriggerField->GetUnderlyingProperty() != TriggerChild || TriggerChild->GetOwnerVariant() != FFieldVariant(TriggerField) || !CastField<FByteProperty>(TriggerChild) || TriggerChild->ArrayDim != 1 || TriggerChild->GetSize() != sizeof(uint8) || TriggerChild->HasSetterOrGetter()) return Stop(TEXT("pcg_template_trigger_child_changed"), TEXT("GenerationTrigger.UnderlyingProperty"), TEXT("same directly owned scalar byte child"), TEXT("mismatch"));
            if (TriggerEnum->HasAnyEnumFlags(EEnumFlags::Flags) != EnumIsFlags || TriggerEnum->HasAnyEnumFlags(EEnumFlags::NewerVersionExists) != EnumIsReplaced || TriggerEnum->NumEnums() != EnumRows.Num()) return Stop(TEXT("pcg_template_trigger_enum_changed"), TEXT("enum_flags/count"), LexToString(EnumRows.Num()), LexToString(TriggerEnum->NumEnums()));
            for (int32 I = 0; I < EnumRows.Num(); ++I) if (TriggerEnum->GetNameByIndex(I) != EnumRows[I].Key || TriggerEnum->GetValueByIndex(I) != EnumRows[I].Value) return Stop(TEXT("pcg_template_trigger_enum_changed"), TEXT("enum_slot"), LexToString(I), TEXT("name/value differs"));
        }
        if (Default.IsValid() && (!Source.NativeClass->ClassDefaultObject.IsResolved() || Source.NativeClass->ClassDefaultObject != Default.Get())) return Stop(TEXT("pcg_template_cdo_changed"), TEXT("ClassDefaultObject"), Address(Default.Get()), TEXT("current handle differs"));
        if (DefaultInstance.IsValid())
        {
            const auto* DefaultHandle = InstanceField->GetObjectPtrPropertyValuePtr(InstanceField->ContainerPtrToValuePtr<void>(Default.Get()));
            if (!DefaultHandle->IsResolved() || *DefaultHandle != DefaultInstance.Get()) return Stop(TEXT("pcg_template_cdo_instance_changed"), TEXT("ClassDefaultObject.GraphInstance"), Address(DefaultInstance.Get()), TEXT("current owned handle differs"));
        }
        if (Ready && !Reconstructing)
        {
            if (!Loaded(Component.Get()) || Selected->ComponentTemplate != Component.Get() || Component->GetClass() != Source.NativeClass.Get() || Component->GetOuter() != Generated.Get() || Component->GetPackage() != BP->GetPackage() || !Component->HasAnyFlags(RF_ArchetypeObject) || Component->HasAnyFlags(RF_ClassDefaultObject) || Component->GetOwner() || Component->GetWorld() || Component->IsRegistered() || Component->IsPreRegistering() || Component->HasBegunPlay() || Component->IsLocalComponent() || Component->IsGenerating() || Component->IsCleaningUp() || Component->bGenerated) return Stop(TEXT("pcg_template_component_role_changed"), TEXT("own_unregistered_template"), TEXT("same native template; no owner/world/live tasks"), Address(Component.Get()));
            const auto* H = InstanceField->GetObjectPtrPropertyValuePtr(InstanceField->ContainerPtrToValuePtr<void>(Component.Get()));
            if (!H->IsResolved() || *H != Instance.Get() || !Loaded(Instance.Get()) || Instance->GetClass() != UPCGGraphInstance::StaticClass() || Instance->GetOuter() != Component.Get() || !Instance->HasAnyFlags(RF_DefaultSubObject) || Instance->HasAnyFlags(RF_ClassDefaultObject) || !Instance->Graph.IsResolved() || Instance->Graph != ExpectedGraph) return Stop(TEXT("pcg_template_instance_role_changed"), TEXT("GraphInstance.Graph"), Address(ExpectedGraph), TEXT("owned handle/expected graph differs"));
            if (Component->GenerationTrigger != ExpectedTrigger || Component->bRegenerateInEditor != ExpectedRegenerate || Component->bGenerateOnDropWhenTriggerOnDemand != ExpectedDrop) return Stop(TEXT("pcg_template_policy_changed"), TEXT("GenerationTrigger/regenerate/drop"), FString::Printf(TEXT("%d/%d/%d"), int32(ExpectedTrigger), ExpectedRegenerate, ExpectedDrop), FString::Printf(TEXT("%d/%d/%d"), int32(Component->GenerationTrigger), Component->bRegenerateInEditor, Component->bGenerateOnDropWhenTriggerOnDemand));
            FBagState Now; if (!ObserveInstanceBag(Now)) return false;
        }
        return Source.Context();
    }
    bool Remember(UObject* O)
    { return Loaded(O) && Source.ClassSource(O->GetClass()) && Source.SourceObject(O); }
    bool CaptureValues(UObject* O, int32 Mode)
    {
        if (!Raw() || !Loaded(O)) return false; const TStrongObjectPtr<UObject> Hold(O); const TStrongObjectPtr<UClass> HoldClass(O->GetClass());
        TArray<FProperty*> Fields; for (TFieldIterator<FProperty> It(O->GetClass()); It; ++It)
        {
            FProperty* P = *It; if (!P->HasAnyPropertyFlags(CPF_Edit) || P->HasAnyPropertyFlags(CPF_Deprecated) || (Mode != 3 && P->HasAnyPropertyFlags(CPF_Transient))) continue;
            const FName N = P->GetFName(); if (Mode == 1 && (N == TEXT("GenerationTrigger") || N == TEXT("bRegenerateInEditor") || N == TEXT("bGenerateOnDropWhenTriggerOnDemand") || N == TEXT("GraphInstance"))) continue;
            if (Mode == 2 && N == TEXT("Graph")) continue; Fields.Add(P);
        }
        if (Fields.Num() > 2048) return Stop(TEXT("pcg_template_value_bound"), TEXT("editable_fields"), TEXT("<=2048"), LexToString(Fields.Num()));
        for (FProperty* P : Fields)
        {
            if (!Raw()) return false;
            UEnum* ObservedEnum = nullptr; TStrongObjectPtr<UEnum> KeepEnum;
            if (P->HasSetterOrGetter()) { const auto* Enum = CastField<FEnumProperty>(P); ObservedEnum = Enum ? Enum->GetEnum() : nullptr; if (Loaded(ObservedEnum)) KeepEnum.Reset(ObservedEnum); if (!Raw()) return false; }
            if (!FNiagaraTemplateScope::ReadableRawStorage(O, P, ObservedEnum) || P->GetSize() <= 0 || P->ArrayDim < 1 || P->ArrayDim > 64 || P->GetSize() > 1048576 || SnapshotBytes > 16777216 - P->GetSize()) return Stop(TEXT("pcg_template_field_storage_unsupported"), P->GetName(), TEXT("bounded readable native storage"), TEXT("unsupported"), O->GetPathName());
            if (auto* A = CastField<FArrayProperty>(P)) { FScriptArrayHelper Array(A, P->ContainerPtrToValuePtr<void>(O)); if (!A->Inner || Array.Num() > 10000 || Array.Num() < 0 || int64(Array.Num()) * A->Inner->GetSize() > 16777216 - SnapshotBytes) return Stop(TEXT("pcg_template_array_snapshot_pending"), P->GetName(), TEXT("bounded current array semantic copy"), LexToString(Array.Num()), O->GetPathName()); SnapshotBytes += int64(Array.Num()) * A->Inner->GetSize(); }
            auto V = MakeUnique<FValue>(); V->Object.Reset(O); V->Class.Reset(O->GetClass()); V->Property = P; V->Path = O->GetPathName(); V->Name = P->GetName();
            V->Memory = P->AllocateAndInitializeValue(); if (!Raw() || !V->Memory) return false;
            P->CopyCompleteValue(V->Memory, P->ContainerPtrToValuePtr<void>(O)); if (!Raw()) return false;
            if (Mode == 2 && P->GetFName() == TEXT("ParametersOverrides"))
            {
                const auto* StructField = CastField<FStructProperty>(P);
                if (!StructField || StructField->Struct != FPCGOverrideInstancedPropertyBag::StaticStruct() || P->ArrayDim != 1 || P->GetSize() != sizeof(FPCGOverrideInstancedPropertyBag)) return Stop(TEXT("pcg_template_parameter_descriptor_unsupported"), P->GetName(), TEXT("exact native scalar FPCGOverrideInstancedPropertyBag storage"), TEXT("descriptor differs"), V->Path);
                InstanceParameterSnapshot = V.Get();
            }
            SnapshotBytes += P->GetSize(); Values.Add(MoveTemp(V));
        }
        return Raw();
    }
    bool Check()
    {
        if (!Raw()) return false;
        for (const auto& V : Values)
        {
            if (!Loaded(V->Object.Get()) || V->Object->GetClass() != V->Class.Get()) return Stop(TEXT("pcg_template_value_owner_changed"), V->Name, TEXT("held native value owner class"), Address(V->Object.Get()), V->Path);
            const bool Equal = V->Property->Identical(V->Property->ContainerPtrToValuePtr<void>(V->Object.Get()), V->Memory, 0);
            if (!Raw()) return false;
            if (!Equal) return Stop(TEXT("pcg_template_unplanned_value_changed"), V->Name, TEXT("captured semantic value"), TEXT("FProperty::Identical=false; no post-failure live export"), V->Path);
        }
        return Raw();
    }
    bool Bind(UBlueprint* B, UPCGGraph* G, const FString& InName)
    {
        BP.Reset(B); Graph.Reset(G); Name = InName; BPPath = B->GetPathName(); GraphPath = G->GetPathName();
        Source.Undo = GUndo; Source.Editor.Reset(GEditor); Source.World.Reset(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr); Source.SourcePath = BPPath;
        if (!Source.Context() || B->ParentClass != AActor::StaticClass() || !Loaded(B->GeneratedClass.Get()) || !Loaded(B->SkeletonGeneratedClass.Get()) || !Loaded(B->SimpleConstructionScript.Get()) || B->GetPackage()->IsDirty() || G->GetPackage()->IsDirty() || B->GetOuter() != B->GetPackage() || G->GetOuter() != G->GetPackage() || B->GetPackage() == G->GetPackage()) return Stop(TEXT("pcg_template_target_scope_pending"), TEXT("target/graph"), TEXT("preloaded saved-clean own Actor BP and native graph"), TEXT("unavailable or dirty"));
        Generated.Reset(B->GeneratedClass); Skeleton.Reset(B->SkeletonGeneratedClass); SCS.Reset(B->SimpleConstructionScript); Roots = SCS->GetRootNodes(); Variables = B->NewVariables;
        for (UClass* OwnClass : {Generated.Get(), Skeleton.Get()})
        { if (!OwnClass->ClassDefaultObject.IsResolved()) return Stop(TEXT("pcg_template_own_cdo_unresolved"), TEXT("own ClassDefaultObject"), TEXT("resolved optional existing handle"), TEXT("unresolved")); if (UObject* Existing = OwnClass->ClassDefaultObject.Get()) Source.Retained.Emplace(Existing); }
        auto* K = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGComponent")); if (!Loaded(K) || K != UPCGComponent::StaticClass() || !Source.ClassSource(K) || !Source.ClassSource(UPCGGraphInstance::StaticClass())) return Stop(TEXT("pcg_template_native_source_unavailable"), TEXT("PCG native source"), TEXT("loaded exact native classes"), TEXT("unavailable")); Source.NativeClass.Reset(K);
        const auto Nodes = SCS->GetAllNodes(); Declarations = Nodes; if (Nodes.Num() > MaxNodes) return Stop(TEXT("pcg_template_scs_bound"), TEXT("SCS.count"), LexToString(MaxNodes), LexToString(Nodes.Num()));
        for (auto* N : Nodes)
        {
            if (!Loaded(N) || !N->ComponentTemplate.IsResolved() || !Loaded(N->ComponentTemplate.Get())) return Stop(TEXT("pcg_template_scs_unavailable"), TEXT("ComponentTemplate"), TEXT("loaded resolved template"), TEXT("unavailable"));
            if (N->ComponentTemplate->IsRegistered() || N->ComponentTemplate->HasBegunPlay()) return Stop(TEXT("pcg_template_existing_live_template_pending"), TEXT("existing template lifecycle"), TEXT("unregistered construction template"), TEXT("live registration"));
            FNodeRole O; O.Node.Reset(N); O.Template.Reset(N->ComponentTemplate.Get()); O.Class = O.Template->GetClass(); O.Guid = N->VariableGuid; O.Name = N->GetVariableName().ToString(); O.Path = N->GetPathName(); O.Children = N->GetChildNodes(); if (!Source.ClassSource(O.Class)) return Stop(TEXT("pcg_template_existing_class_pending"), TEXT("existing component class"), TEXT("native observable source"), O.Class->GetPathName()); Old.Add(MoveTemp(O));
            if (N->GetVariableName() == FName(*Name)) { if (Selected.IsValid()) return Stop(TEXT("pcg_template_name_ambiguous"), TEXT("component_name"), TEXT("one own declaration"), Name); Selected.Reset(N); }
        }
        if (!Selected.IsValid() || !Selected->VariableGuid.IsValid() || Selected->ComponentClass != K) return Stop(TEXT("pcg_template_component_missing"), TEXT("component_name/class"), TEXT("native PCGComponent declaration"), Name);
        Component.Reset(Cast<UPCGComponent>(Selected->ComponentTemplate.Get())); if (!Component.IsValid() || Component->GetClass() != K) return Stop(TEXT("pcg_template_component_class_invalid"), TEXT("template class"), TEXT("exact native PCGComponent"), Address(Component.Get()));
        InstanceField = FindFProperty<FObjectProperty>(K, TEXT("GraphInstance")); TriggerField = FindFProperty<FEnumProperty>(K, TEXT("GenerationTrigger")); RegenerateField = FindFProperty<FBoolProperty>(K, TEXT("bRegenerateInEditor")); DropField = FindFProperty<FBoolProperty>(K, TEXT("bGenerateOnDropWhenTriggerOnDemand"));
        if (!InstanceField || InstanceField->GetOwnerClass() != K || InstanceField->ArrayDim != 1 || InstanceField->HasSetterOrGetter() || !InstanceField->HasAnyPropertyFlags(CPF_InstancedReference) || !TriggerField || TriggerField->GetOwnerClass() != K || TriggerField->ArrayDim != 1 || TriggerField->GetSize() != sizeof(EPCGComponentGenerationTrigger) || TriggerField->HasSetterOrGetter() || !CastField<FByteProperty>(TriggerField->GetUnderlyingProperty()) || !RegenerateField || !DropField) return Stop(TEXT("pcg_template_policy_descriptor_invalid"), TEXT("policy/instance descriptor"), TEXT("native owned byte enum/bool/instanced reference"), TEXT("mismatch"));
        for (FBoolProperty* P : {RegenerateField, DropField}) if (P->GetOwnerClass() != K || P->ArrayDim != 1 || P->GetSize() != sizeof(bool) || !P->IsNativeBool() || P->HasSetterOrGetter() || !P->HasAnyPropertyFlags(CPF_Edit)) return Stop(TEXT("pcg_template_policy_descriptor_invalid"), P->GetName(), TEXT("native public editable bool"), TEXT("mismatch"));
        TriggerChild = TriggerField->GetUnderlyingProperty(); if (TriggerChild->GetOwnerVariant() != FFieldVariant(TriggerField) || TriggerChild->ArrayDim != 1 || TriggerChild->GetSize() != sizeof(uint8) || TriggerChild->HasSetterOrGetter()) return Stop(TEXT("pcg_template_trigger_child_invalid"), TEXT("GenerationTrigger.UnderlyingProperty"), TEXT("directly owned scalar byte child"), TEXT("mismatch"));
        TriggerEnum.Reset(TriggerField->GetEnum()); if (!Loaded(TriggerEnum.Get()) || !TriggerEnum->GetPathName().Equals(TEXT("/Script/PCG.EPCGComponentGenerationTrigger"), ESearchCase::CaseSensitive)) return Stop(TEXT("pcg_template_trigger_enum_invalid"), TEXT("GenerationTrigger.Enum"), TEXT("native EPCGComponentGenerationTrigger"), TEXT("mismatch"));
        if (!Remember(TriggerEnum.Get())) return Stop(TEXT("pcg_template_trigger_source_invalid"), TEXT("enum source"), TEXT("held native enum role"), TEXT("unavailable"));
        EnumIsFlags = TriggerEnum->HasAnyEnumFlags(EEnumFlags::Flags); EnumIsReplaced = TriggerEnum->HasAnyEnumFlags(EEnumFlags::NewerVersionExists); for (int32 I = 0; I < TriggerEnum->NumEnums(); ++I) EnumRows.Add({TriggerEnum->GetNameByIndex(I), TriggerEnum->GetValueByIndex(I)});
        if (TriggerEnum->GetValueByNameString(TEXT("GenerateOnDemand")) != int64(EPCGComponentGenerationTrigger::GenerateOnDemand)) return Stop(TEXT("pcg_template_trigger_enum_invalid"), TEXT("GenerateOnDemand.value"), LexToString(int64(EPCGComponentGenerationTrigger::GenerateOnDemand)), TEXT("mismatch"));
        const auto* H = InstanceField->GetObjectPtrPropertyValuePtr(InstanceField->ContainerPtrToValuePtr<void>(Component.Get())); if (!H->IsResolved() || !Loaded(H->Get())) return Stop(TEXT("pcg_template_instance_unavailable"), TEXT("GraphInstance"), TEXT("resolved owned instance"), TEXT("unavailable")); Instance.Reset(Cast<UPCGGraphInstance>(H->Get()));
        ExpectedTrigger = Component->GenerationTrigger; ExpectedRegenerate = Component->bRegenerateInEditor; ExpectedDrop = Component->bGenerateOnDropWhenTriggerOnDemand;
        if (!Instance.IsValid() || !Instance->Graph.IsResolved() || (ExpectedTrigger != EPCGComponentGenerationTrigger::GenerateOnLoad && ExpectedTrigger != EPCGComponentGenerationTrigger::GenerateOnDemand) || !Component->SchedulingPolicy.IsResolved() || Component->SchedulingPolicy != nullptr) return Stop(TEXT("pcg_template_policy_variant_pending"), TEXT("initial trigger/scheduling policy"), TEXT("OnLoad or OnDemand, no scheduling policy"), TEXT("pending variant"));
        ExpectedGraph = Instance->Graph.Get(); if (ExpectedGraph && ExpectedGraph != G) return Stop(TEXT("pcg_template_replacement_pending"), TEXT("GraphInstance.Graph"), TEXT("unbound or same requested graph"), Address(ExpectedGraph));
        Authored = G->GetNodes(); Ready = true; if (!Raw()) return false;
        if (!BindEmptyParameters()) return false;
        TArray<UPCGNode*> AllGraphNodes = Authored; AllGraphNodes.Add(G->GetInputNode()); AllGraphNodes.Add(G->GetOutputNode()); if (Authored.Num() > 128 || !Remember(G)) return Stop(TEXT("pcg_template_graph_scope_pending"), TEXT("graph source"), TEXT("<=128 loaded native nodes"), LexToString(Authored.Num()));
        TSet<UPCGNode*> UniqueNodes; TSet<UPCGPin*> UniquePins; TSet<UPCGEdge*> UniqueEdges;
        for (auto* N : AllGraphNodes)
        {
            if (!Loaded(N) || UniqueNodes.Contains(N) || N->GetOuter() != G || !Remember(N)) return Stop(TEXT("pcg_template_graph_node_invalid"), TEXT("graph node"), TEXT("unique own native node"), Address(N)); UniqueNodes.Add(N);
            FMember M; M.Node.Reset(N); UPCGSettings* Settings = N->GetSettings(); if (Loaded(Settings)) M.Settings.Reset(Settings); if (!Raw() || !M.Settings.IsValid() || !Remember(Settings)) return Stop(TEXT("pcg_template_graph_settings_invalid"), TEXT("node settings"), TEXT("loaded native settings"), Address(Settings));
            M.Inputs = N->GetInputPins(); M.Outputs = N->GetOutputPins(); TArray<TObjectPtr<UPCGPin>> Both = M.Inputs; Both.Append(M.Outputs);
            for (const auto& PinHandle : Both) { if (!PinHandle.IsResolved()) return Stop(TEXT("pcg_template_graph_pin_unresolved"), TEXT("pin handle"), TEXT("resolved"), TEXT("unresolved")); auto* P = PinHandle.Get(); if (!Loaded(P) || UniquePins.Contains(P) || P->Node != N || !Remember(P)) return Stop(TEXT("pcg_template_graph_pin_invalid"), TEXT("graph pin"), TEXT("unique owned pin"), Address(P)); UniquePins.Add(P); FPin V; V.Pin.Reset(P); V.Node = N; V.Edges = P->Edges; Pins.Add(MoveTemp(V)); }
            Members.Add(MoveTemp(M));
        }
        for (const auto& P : Pins) for (const auto& EdgeHandle : P.Edges) { if (!EdgeHandle.IsResolved()) return Stop(TEXT("pcg_template_graph_edge_unresolved"), TEXT("edge handle"), TEXT("resolved"), TEXT("unresolved")); auto* E = EdgeHandle.Get(); if (!UniqueEdges.Contains(E)) { if (!Loaded(E) || !E->InputPin.IsResolved() || !E->OutputPin.IsResolved() || !UniquePins.Contains(E->InputPin.Get()) || !UniquePins.Contains(E->OutputPin.Get()) || !Remember(E)) return Stop(TEXT("pcg_template_graph_edge_invalid"), TEXT("edge endpoints"), TEXT("owned reciprocal edge"), Address(E)); UniqueEdges.Add(E); FEdge V; V.Edge.Reset(E); V.Input = E->InputPin.Get(); V.Output = E->OutputPin.Get(); Edges.Add(MoveTemp(V)); } }
        if (Edges.Num() > 2048) return Stop(TEXT("pcg_template_graph_edge_bound"), TEXT("edge_count"), TEXT("<=2048"), LexToString(Edges.Num()));
        Ready = true; if (!Raw()) return false;
        UObject* D = K->GetDefaultObject(false); const TStrongObjectPtr<UObject> HoldDefault(D); if (!Raw() || !Loaded(D) || D->GetClass() != K || !D->HasAnyFlags(RF_ClassDefaultObject) || !Remember(D)) return Stop(TEXT("pcg_template_cdo_unavailable"), TEXT("native CDO"), TEXT("existing native CDO; no creation"), Address(D)); Default.Reset(Cast<UPCGComponent>(D)); if (!Raw()) return false;
        UPCGGraphInstance* DI = Default->GetGraphInstance(); const TStrongObjectPtr<UPCGGraphInstance> HoldDefaultInstance(DI); if (!Raw() || !Loaded(DI) || DI->GetOuter() != D || DI->GetClass() != UPCGGraphInstance::StaticClass() || !DI->HasAnyFlags(RF_DefaultSubObject) || DI->HasAnyFlags(RF_ClassDefaultObject) || !Remember(DI)) return Stop(TEXT("pcg_template_cdo_instance_invalid"), TEXT("CDO.GraphInstance"), TEXT("existing CDO-owned subobject"), Address(DI)); DefaultInstance.Reset(DI); if (!Raw()) return false;
        UPCGGraphInstance* PublicInstance = Component->GetGraphInstance(); const TStrongObjectPtr<UPCGGraphInstance> HoldPublic(PublicInstance); if (!Raw() || PublicInstance != Instance.Get()) return Stop(TEXT("pcg_template_public_instance_changed"), TEXT("GetGraphInstance"), Address(Instance.Get()), Address(PublicInstance));
        if (!CaptureValues(D, 0) || !CaptureValues(DI, 0)) return false;
        for (const auto& O : Old) if (!CaptureValues(O.Template.Get(), O.Node.Get() == Selected.Get() ? 1 : 0)) return false;
        if (!CaptureValues(Instance.Get(), 2) || !CaptureValues(G, 3)) return false;
        if (!InstanceParameterSnapshot) return Stop(TEXT("pcg_template_parameter_snapshot_missing"), TEXT("ParametersOverrides"), TEXT("whole struct snapshot including IDs and other members"), TEXT("not captured"));
        for (const auto& M : Members) if (!CaptureValues(M.Node.Get(), 3) || !CaptureValues(M.Settings.Get(), 3)) return false;
        for (const auto& P : Pins) if (!CaptureValues(P.Pin.Get(), 3)) return false;
        for (const auto& E : Edges) if (!CaptureValues(E.Edge.Get(), 3)) return false;
        DirtyBefore = DirtyPackages(); if (!Raw() || !FNiagaraTemplateScope::File(B, BPFile) || !FNiagaraTemplateScope::Disk(BPFile, BPBytes, BPHash) || BPBytes <= 0 || !FNiagaraTemplateScope::File(G, GraphFile) || !FNiagaraTemplateScope::Disk(GraphFile, GraphBytes, GraphHash) || GraphBytes <= 0) return Stop(TEXT("pcg_template_saved_sources_required"), TEXT("target/source disk"), TEXT("bounded nonempty saved files"), TEXT("unavailable"));
        if (!Check()) return false; const bool NoExternal = RejectExternalInstances(B, R); if (!Check() || !NoExternal) return false;
        return true;
    }
    void RemapValues(UObject* Was, UObject* Now) { if (Was == Now) return; for (auto& V : Values) if (V->Object.Get() == Was) { V->Object.Reset(Now); V->Path = Now->GetPathName(); } }
    bool Reacquire()
    {
        if (!Raw(true)) return false;
        auto* G = BP->GeneratedClass.Get(); auto* K = BP->SkeletonGeneratedClass.Get();
        if (!Loaded(G) || !Loaded(K) || G->ClassGeneratedBy != BP.Get() || K->ClassGeneratedBy != BP.Get() || G->GetSuperClass() != AActor::StaticClass() || K->GetSuperClass() != AActor::StaticClass() || G->GetPackage() != BP->GetPackage() || K->GetPackage() != BP->GetPackage()) return Stop(TEXT("pcg_template_reconstruction_roles_invalid"), TEXT("generated/skeleton"), TEXT("current own Actor class roles"), TEXT("mismatch"));
        for (UClass* OwnClass : {G, K}) { if (!OwnClass->ClassDefaultObject.IsResolved()) return Stop(TEXT("pcg_template_reconstruction_cdo_unresolved"), TEXT("own ClassDefaultObject"), TEXT("resolved optional handle"), TEXT("unresolved")); if (UObject* Existing = OwnClass->ClassDefaultObject.Get()) Source.Retained.Emplace(Existing); }
        Source.Retained.Emplace(Generated.Get()); Source.Retained.Emplace(Skeleton.Get()); Generated.Reset(G); Skeleton.Reset(K);
        for (auto& O : Old)
        {
            if (!O.Node->ComponentTemplate.IsResolved()) return Stop(TEXT("pcg_template_reconstruction_unresolved"), TEXT("ComponentTemplate"), TEXT("resolved current handle"), TEXT("unresolved"), O.Path);
            auto* T = O.Node->ComponentTemplate.Get(); const TStrongObjectPtr<UActorComponent> HoldNew(T);
            if (!Loaded(T) || T->GetClass() != O.Class || T->GetOuter() != G || T->GetPackage() != BP->GetPackage()) return Stop(TEXT("pcg_template_reconstruction_template_invalid"), TEXT("ComponentTemplate.class/outer"), TEXT("same declaration native class/current own generated outer"), Address(T), O.Path);
            Source.Retained.Emplace(O.Template.Get()); RemapValues(O.Template.Get(), T); O.Template.Reset(T);
        }
        auto* T = Cast<UPCGComponent>(Selected->ComponentTemplate.Get()); Source.Retained.Emplace(Component.Get()); Component.Reset(T);
        const auto* H = InstanceField->GetObjectPtrPropertyValuePtr(InstanceField->ContainerPtrToValuePtr<void>(T)); if (!H->IsResolved() || !Loaded(H->Get())) return Stop(TEXT("pcg_template_reconstruction_instance_invalid"), TEXT("GraphInstance"), TEXT("resolved current owned default subobject"), TEXT("unavailable"));
        auto* GI = Cast<UPCGGraphInstance>(H->Get()); const TStrongObjectPtr<UPCGGraphInstance> HoldNewInstance(GI); if (!Loaded(GI) || GI->GetClass() != UPCGGraphInstance::StaticClass() || GI->GetOuter() != T) return Stop(TEXT("pcg_template_reconstruction_instance_invalid"), TEXT("GraphInstance.class/outer"), TEXT("same native class, own current template"), Address(GI)); Source.Retained.Emplace(Instance.Get()); RemapValues(Instance.Get(), GI); Instance.Reset(GI);
        if (!Check()) return false; auto* Returned = T->GetGraphInstance(); const TStrongObjectPtr<UPCGGraphInstance> HoldReturned(Returned); if (!Check() || Returned != GI) return Stop(TEXT("pcg_template_reconstruction_getter_changed"), TEXT("GetGraphInstance"), Address(GI), Address(Returned));
        return true;
    }
    bool Enter(const TCHAR* Stage)
    {
        if (!Check()) return false; R.Stage = Stage; R.bMutationAttempted = true; R.bChangeStateKnown = false;
        auto C = MakeShared<FJsonObject>(); C->SetStringField(TEXT("stage"), Stage); C->SetBoolField(TEXT("attempted"), true); C->SetBoolField(TEXT("returned"), false); Prefix.Add(MakeShared<FJsonValueObject>(C)); return true;
    }
    void Returned() { Prefix.Last()->AsObject()->SetBoolField(TEXT("returned"), true); }
    template<typename F> bool Policy(FProperty* P, bool Needed, F&& Assign)
    {
        if (!Needed) return true;
        if (!Enter(TEXT("Template_PreEditChange"))) return false; static_cast<UObject*>(Component.Get())->PreEditChange(P); Returned(); if (!Check()) return false;
        if (!Enter(TEXT("Template_public_policy_assignment"))) return false; Assign(); Returned(); R.bChanged = true; if (!Check()) return false;
        if (!Enter(TEXT("Template_PostEditChangeProperty"))) return false; FPropertyChangedEvent Event(P, EPropertyChangeType::ValueSet); Event.MemberProperty = P; static_cast<UObject*>(Component.Get())->PostEditChangeProperty(Event); Returned();
        // Old template/instance addresses stay opaque until current declaration
        // handles are reacquired; no new value baseline is adopted.
        return Reacquire();
    }
    TSharedRef<FJsonObject> Projection()
    {
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("template_path"), Component->GetPathName()); J->SetStringField(TEXT("graph_instance_path"), Instance->GetPathName()); J->SetStringField(TEXT("graph_path"), ExpectedGraph ? GraphPath : FString()); J->SetStringField(TEXT("generation_trigger"), ExpectedTrigger == EPCGComponentGenerationTrigger::GenerateOnDemand ? TEXT("GenerateOnDemand") : TEXT("GenerateOnLoad")); J->SetBoolField(TEXT("regenerate_in_editor"), ExpectedRegenerate); J->SetBoolField(TEXT("generate_on_drop_when_on_demand"), ExpectedDrop); return J;
    }
    bool ObserveGraph(bool Initial)
    {
        if (!Check()) return false; const FString RawResult = CubelessPCGUE58::InspectGraphSnapshot(GraphPath); if (!Check()) return false;
        TSharedPtr<FJsonObject> Reply; if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RawResult), Reply) || !Reply.IsValid()) return Stop(TEXT("pcg_template_graph_observation_invalid"), TEXT("graph observer JSON"), TEXT("typed native snapshot receipt"), TEXT("malformed"), GraphPath);
        bool Success = false, Complete = false, Preserved = false; const TSharedPtr<FJsonObject>* Snapshot = nullptr;
        if (!Reply->TryGetBoolField(TEXT("success"), Success) || !Success || !Reply->TryGetBoolField(TEXT("observation_complete"), Complete) || !Complete || !Reply->TryGetBoolField(TEXT("state_preserved"), Preserved) || !Preserved || !Reply->TryGetObjectField(TEXT("snapshot"), Snapshot) || !Snapshot || !Snapshot->IsValid()) { R.Data->SetStringField(TEXT("source_graph_observation_failure"), RawResult); return Stop(TEXT("pcg_template_graph_observation_failed"), TEXT("graph snapshot proof"), TEXT("success/complete/preserved"), TEXT("native observer refused"), GraphPath); }
        FString Text; FJsonSerializer::Serialize(Snapshot->ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
        if (Initial) GraphSnapshot = MoveTemp(Text); else if (!GraphSnapshot.Equals(Text, ESearchCase::CaseSensitive)) return Stop(TEXT("pcg_template_source_snapshot_changed"), TEXT("whole topology snapshot"), TEXT("captured native snapshot"), TEXT("different"), GraphPath);
        return Check();
    }
    bool FinishDisk()
    {
        if (!Check()) return false; int64 Bytes; FString Hash;
        if (!FNiagaraTemplateScope::Disk(BPFile, Bytes, Hash) || Bytes != BPBytes || !Hash.Equals(BPHash, ESearchCase::CaseSensitive)) return Stop(TEXT("pcg_template_unrequested_save"), TEXT("Blueprint disk"), BPHash, Hash);
        if (!FNiagaraTemplateScope::Disk(GraphFile, Bytes, Hash) || Bytes != GraphBytes || !Hash.Equals(GraphHash, ESearchCase::CaseSensitive)) return Stop(TEXT("pcg_template_source_disk_changed"), TEXT("Graph disk"), GraphHash, Hash, GraphPath);
        const auto DirtyAfter = DirtyPackages(); for (const auto& P : DirtyAfter) if (!DirtyBefore.Contains(P) && P != BP->GetPackage()->GetName()) return Stop(TEXT("pcg_template_unrelated_dirty"), TEXT("new dirty package"), TEXT("target Blueprint only"), P);
        return Check();
    }
};

bool PCGTemplateConfigureActive = false;
FMutationResult ConfigureNativePCGTemplate(const FString& ParamsJson)
{
    FMutationResult R; R.Compiled = false; R.Data->SetBoolField(TEXT("pcg_followup_sdk_stopped"), false); R.Data->SetBoolField(TEXT("native_pcg_template_configure"), true);
    R.Data->SetBoolField(TEXT("automatic_retry_permitted"), false); R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("graph_assignment_attempted"), false);
    TSharedPtr<FJsonValue> Parsed; if (!ParseValue(ParamsJson, Parsed, R) || Parsed->Type != EJson::Object) { if (R.ErrorCode.IsEmpty()) Fail(R, TEXT("pcg_template_params_invalid"), TEXT("Exact object carrier required")); return R; }
    const auto P = Parsed->AsObject(); const TSet<FString> Keys{TEXT("blueprint_path"), TEXT("component_name"), TEXT("graph_path"), TEXT("regenerate_in_editor"), TEXT("generate_on_drop_when_on_demand"), TEXT("compile")};
    if (P->Values.Num() != Keys.Num()) { Fail(R, TEXT("pcg_template_params_invalid"), TEXT("All six explicit fields and no unknown keys required")); return R; }
    for (const auto& Pair : P->Values) if (!Keys.Contains(FString(Pair.Key.ToView()))) { Fail(R, TEXT("pcg_template_params_invalid"), TEXT("Unknown carrier key")); return R; }
    FString BPPath, Name, GraphPath; bool Regenerate = false, Drop = false, Compile = false;
    if (!P->TryGetStringField(TEXT("blueprint_path"), BPPath) || !P->TryGetStringField(TEXT("component_name"), Name) || !P->TryGetStringField(TEXT("graph_path"), GraphPath) || !SafeName(Name) || !P->HasTypedField<EJson::Boolean>(TEXT("regenerate_in_editor")) || !P->HasTypedField<EJson::Boolean>(TEXT("generate_on_drop_when_on_demand")) || !P->HasTypedField<EJson::Boolean>(TEXT("compile")) || !P->TryGetBoolField(TEXT("regenerate_in_editor"), Regenerate) || !P->TryGetBoolField(TEXT("generate_on_drop_when_on_demand"), Drop) || !P->TryGetBoolField(TEXT("compile"), Compile) || !BPPath.StartsWith(TEXT("/Game/")) || !GraphPath.StartsWith(TEXT("/Game/")) || BPPath.Len() > 1024 || GraphPath.Len() > 1024)
    { Fail(R, TEXT("pcg_template_params_invalid"), TEXT("Exact own /Game object paths, component name and explicit JSON booleans required")); return R; }
    R.bCompileRequested = Compile; R.Data->SetStringField(TEXT("blueprint_path"), BPPath); R.Data->SetStringField(TEXT("component_name"), Name); R.Data->SetStringField(TEXT("graph_path"), GraphPath);
    if (!IsEditorGameThread() || IsPlaySessionActive() || GExitPurge || IsEngineExitRequested() || IsGarbageCollecting() || UE::IsSavingPackage()) { Fail(R, TEXT("pcg_template_context_unavailable"), TEXT("Idle safe Editor game thread required; no template writer entered")); return R; }
    if (PCGTemplateConfigureActive) { Fail(R, TEXT("pcg_template_reentrant"), TEXT("Nested template configure refused before any target lookup/writer")); return R; }
    TGuardValue<bool> ActiveGuard(PCGTemplateConfigureActive, true);
    const TStrongObjectPtr<UEditorEngine> EntryEditor(GEditor); const TStrongObjectPtr<UWorld> EntryWorld(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr); auto BootstrapCurrent = [&] { return IsEditorGameThread() && !IsPlaySessionActive() && !GExitPurge && !IsEngineExitRequested() && !IsGarbageCollecting() && !UE::IsSavingPackage() && FPCGTemplateAddScope::Loaded(EntryEditor.Get()) && GEditor == EntryEditor.Get() && FPCGTemplateAddScope::Loaded(EntryWorld.Get()) && !EntryWorld->bIsTearingDown && GEditor->GetEditorWorldContext().World() == EntryWorld.Get(); };
    if (!BootstrapCurrent()) { R.bChangeStateKnown = false; Fail(R, TEXT("pcg_template_bootstrap_changed"), TEXT("Initial Editor/world unavailable")); return R; }
    auto* B = FindObject<UBlueprint>(nullptr, *BPPath); const TStrongObjectPtr<UBlueprint> HoldBP(B);
    if (!BootstrapCurrent()) { R.bChangeStateKnown = false; Fail(R, TEXT("pcg_template_bootstrap_changed"), TEXT("Editor/world changed after target lookup")); return R; }
    if (!FPCGTemplateAddScope::Loaded(B)) { Fail(R, TEXT("pcg_template_preload_required"), TEXT("Exact Blueprint must already be loaded; no implicit loading")); return R; }
    auto* G = FindObject<UPCGGraph>(nullptr, *GraphPath); const TStrongObjectPtr<UPCGGraph> HoldGraph(G);
    if (!BootstrapCurrent()) { R.bChangeStateKnown = false; Fail(R, TEXT("pcg_template_bootstrap_changed"), TEXT("Editor/world changed after graph lookup")); return R; }
    if (!FPCGTemplateAddScope::Loaded(G) || !FPCGTemplateAddScope::Loaded(B) || G->GetClass() != UPCGGraph::StaticClass() || !B->GetPathName().Equals(BPPath, ESearchCase::CaseSensitive) || !G->GetPathName().Equals(GraphPath, ESearchCase::CaseSensitive)) { Fail(R, TEXT("pcg_template_preload_required"), TEXT("Exact ordinary source graph must already be loaded; no implicit loading or creation")); return R; }
    FPCGTemplateConfigureScope W(R); if (!W.Bind(B, G, Name) || !W.ObserveGraph(true)) { if (R.ErrorCode.IsEmpty()) Fail(R, TEXT("pcg_template_preflight_failed"), TEXT("Template/source preparation failed before first writer")); return R; }
    R.Data->SetObjectField(TEXT("before"), W.Projection());
    const bool Needed = W.ExpectedGraph != G || W.ExpectedTrigger != EPCGComponentGenerationTrigger::GenerateOnDemand || W.ExpectedRegenerate != Regenerate || W.ExpectedDrop != Drop;
    const bool Run = [&]()
    {
        if (!Needed) return true;
        if (!W.Enter(TEXT("Blueprint_Modify"))) return false; const bool BlueprintModified = B->Modify(); W.Returned(); W.Prefix.Last()->AsObject()->SetBoolField(TEXT("engine_return"), BlueprintModified); if (!W.Check()) return false;
        if (!W.Enter(TEXT("Template_Modify"))) return false; const bool TemplateModified = W.Component->Modify(); W.Returned(); W.Prefix.Last()->AsObject()->SetBoolField(TEXT("engine_return"), TemplateModified); if (!W.Check()) return false;
        if (!W.Policy(W.RegenerateField, W.ExpectedRegenerate != Regenerate, [&] { W.Component->bRegenerateInEditor = Regenerate; W.ExpectedRegenerate = Regenerate; })) return false;
        if (!W.Policy(W.DropField, W.ExpectedDrop != Drop, [&] { W.Component->bGenerateOnDropWhenTriggerOnDemand = Drop; W.ExpectedDrop = Drop; })) return false;
        if (!W.Policy(W.TriggerField, W.ExpectedTrigger != EPCGComponentGenerationTrigger::GenerateOnDemand, [&] { W.Component->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand; W.ExpectedTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand; })) return false;
        if (W.ExpectedGraph != G)
        {
            if (!W.Enter(TEXT("Template_SetGraphLocal"))) return false; R.Data->SetBoolField(TEXT("graph_assignment_attempted"), true); W.Component->SetGraphLocal(G); W.Returned(); W.ExpectedGraph = G; R.bChanged = true; if (!W.ExpectedEmptyParameterTransition() || !W.Reacquire()) return false;
        }
        if (!W.Enter(TEXT("MarkBlueprintAsModified"))) return false; FBlueprintEditorUtils::MarkBlueprintAsModified(B); W.Returned(); return W.Reacquire();
    }();
    R.Data->SetArrayField(TEXT("actual_call_prefix"), W.Prefix);
    if (!Run || W.Source.Stopped) { R.Data->SetBoolField(TEXT("pcg_followup_sdk_stopped"), true); return R; }
    if (Compile)
    {
        if (!W.Enter(TEXT("compile"))) return R; R.bCompileAttempted = true; R.Compiled.Reset(); auto Reply = Cubeless::UE58::CompileBlueprint(BPPath, false, false); W.Returned(); bool Compiled = false; Reply->TryGetBoolField(TEXT("compiled"), Compiled); R.Compiled = Compiled; R.Data->SetObjectField(TEXT("compile"), Reply); R.Data->SetArrayField(TEXT("actual_call_prefix"), W.Prefix);
        if (!W.Reacquire()) return R; if (!Compiled) { Fail(R, TEXT("component_compile_failed"), TEXT("Template edit prefix remains; compile failed, no save/retry/rollback")); return R; }
    }
    if (!W.ObserveGraph(false) || !W.FinishDisk()) return R;
    R.Data->SetObjectField(TEXT("after"), W.Projection()); R.Data->SetObjectField(TEXT("source_parameter_bag_after"), W.BagReceipt(W.SourceBag)); R.Data->SetObjectField(TEXT("instance_parameter_bag_after"), W.BagReceipt(W.ExpectedInstanceBag, 0)); R.Data->SetBoolField(TEXT("source_graph_preserved"), true); R.Data->SetBoolField(TEXT("native_cdo_preserved"), true); R.Data->SetBoolField(TEXT("existing_scs_observations_preserved"), true); R.Data->SetBoolField(TEXT("operation_complete"), true); R.Data->SetBoolField(TEXT("observation_complete"), true); R.Data->SetBoolField(TEXT("generation_requested"), false); R.Data->SetBoolField(TEXT("cleanup_requested"), false); R.Data->SetBoolField(TEXT("graph_assignment_uses_public_setter"), true);
    R.Data->SetStringField(TEXT("scope"), TEXT("One named ownerless/unregistered native PCG SCS template; saved-clean preloaded Actor BP and native graph without user parameters. Planned policy values and public SetGraphLocal only. Readonly source semantic properties/topology preserved; not all private caches/global SDK purity. Original multi-template helper, live actors, runtime policies/parameters, replacement with another existing graph, generation/binding/generic Set remain incomplete. No save/rollback/cleanup/retry."));
    R.Data->SetBoolField(TEXT("dirty_before"), false); R.Data->SetBoolField(TEXT("dirty_after"), B->GetPackage()->IsDirty()); R.bChanged |= B->GetPackage()->IsDirty();
    R.bChangeStateKnown = true; R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}



// QE: concrete inactive native SCS ChildActorComponent authoring; no runtime child actor or private Engine utility.
bool CACName(const FString& S)
{return S==TEXT("ChildActorComponent")||S==TEXT("UChildActorComponent")||S==TEXT("ChildActor")||S==TEXT("/Script/Engine.ChildActorComponent");}
struct FCACWork
{
    FMutationResult& R;TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<USimpleConstructionScript> SCS;TStrongObjectPtr<UClass> Gen,Skel;TStrongObjectPtr<UObject> HostCDO;
    TStrongObjectPtr<USCS_Node> Selected,Parent;TStrongObjectPtr<UChildActorComponent> CAC;TStrongObjectPtr<AActor> CAT;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TArray<TStrongObjectPtr<UObject>> Holds,Retired;
    struct FRole{TStrongObjectPtr<UObject> Object,Outer;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString Path;EObjectFlags Flags=RF_NoFlags;};
    struct FSource{TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UObject> CDO;TArray<UObject*> Owned;TArray<FRole> Roles;TArray<FString> Fields;EClassFlags ClassFlags;UClass* Super=nullptr;uint32 PackageFlags=0;};
    struct FNode{TStrongObjectPtr<USCS_Node> Node;TStrongObjectPtr<UActorComponent> Template;UClass* Class=nullptr;FGuid Guid;FName Name;TArray<USCS_Node*> Children;FString Values;};
    struct FPin{UEdGraphPin* Pin=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;FText Text,Friendly;FString Raw,Auto,Wire;UObject* Object=nullptr;TArray<UEdGraphPin*> Links,Children;UEdGraphPin* ParentPin=nullptr;uint32 Flags=0;};
    struct FGraphNode{TStrongObjectPtr<UEdGraphNode> Node;FGuid Guid;UClass* Class=nullptr;FString Comment,Path;int32 X=0,Y=0;TArray<FPin> Pins;};
    struct FGraph{TStrongObjectPtr<UEdGraph> Graph;FGuid Guid;TArray<FGraphNode> Nodes;};
    TArray<FSource> Sources;TArray<FNode> Nodes;TArray<USCS_Node*> All,Roots;TArray<FGraph> Graphs,Candidates;TArray<TArray<TObjectPtr<UEdGraph>>> Categories;
    TArray<FRole> CATRoles;TArray<UObject*> CATOwned;TArray<FString> CATValues;TArray<FRole> Anchors;
    FString Name,GenPath,SkelPath,CDOPath,TargetFile,FileHash,OtherNodeValues,DeclarationBefore,GraphBefore,SelectedValues;TArray<TPair<FString,FString>> SelectedColumns;int64 FileBytes=-1;TArray<FString> OtherDirty;
    UClass* ExpectedClass=nullptr;bool Stopped=false,Add=false,Attached=false,Rebinding=false,ClassMutable=false,CATMutable=false,CandidatesReady=false,GraphsReady=false,SelectedReady=false,TransformMutable=false;
    FGuid NodeGuid;FVector ExpectedLocation,ExpectedScale;FRotator ExpectedRotation;TArray<TSharedPtr<FJsonValue>> Calls;decltype(GUndo) Undo=nullptr;
    explicit FCACWork(FMutationResult& In):R(In){}
    static bool Loaded(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Bad(const TCHAR* Field,const FString& E,const FString& A,const FString& Object=FString())
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object_captured_path"),Object);J->SetStringField(TEXT("property"),Field);J->SetStringField(TEXT("expected"),E);J->SetStringField(TEXT("actual"),A);J->SetBoolField(TEXT("later_conditions_unobserved"),true);R.Data->SetObjectField(TEXT("child_actor_first_mismatch"),J);Fail(R,TEXT("component_child_actor_current_unknown"),TEXT("Captured source/target/CAT current changed. Actual prefix retained; no next SDK, cleanup or retry."));}
        Stopped=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;R.Data->SetBoolField(TEXT("child_actor_followup_sdk_stopped"),true);return false;
    }
    bool Refuse(const TCHAR* Code,const TCHAR* Why){return Fail(R,Code,Why);}
    static FString Pointer(const void* P){return FString::Printf(TEXT("%p"),P);}
    bool Role(UObject* O,TArray<FRole>& Out)
    {
        if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage()))return Bad(TEXT("returned_object_lifetime"),TEXT("loaded object/class/package"),Pointer(O));
        FRole V;V.Object.Reset(O);V.Class.Reset(O->GetClass());V.Outer.Reset(O->GetOuter());V.Package.Reset(O->GetPackage());V.Path=O->GetPathName();V.Flags=O->GetFlags();Out.Add(MoveTemp(V));return true;
    }
    bool RolesCurrent(const TArray<FRole>& Rows)
    {
        for(const auto& V:Rows){UObject* O=V.Object.Get();if(!Loaded(O))return Bad(TEXT("lifetime"),TEXT("held loaded object"),TEXT("unavailable"),V.Path);if(O->GetClass()!=V.Class.Get())return Bad(TEXT("class"),Pointer(V.Class.Get()),Pointer(O->GetClass()),V.Path);if(O->GetOuter()!=V.Outer.Get())return Bad(TEXT("outer"),Pointer(V.Outer.Get()),Pointer(O->GetOuter()),V.Path);if(O->GetPackage()!=V.Package.Get())return Bad(TEXT("package"),Pointer(V.Package.Get()),Pointer(O->GetPackage()),V.Path);const FString Path=O->GetPathName();if(!Path.Equals(V.Path,ESearchCase::CaseSensitive))return Bad(TEXT("path"),V.Path,Path,V.Path);if(O->GetFlags()!=V.Flags)return Bad(TEXT("flags"),LexToString(uint32(V.Flags)),LexToString(uint32(O->GetFlags())),V.Path);}return true;
    }
    bool Context()
    {return !Stopped&&((IsEditorGameThread()&&GEditor==Editor.Get()&&Loaded(Editor.Get())&&Loaded(World.Get())&&GEditor->GetEditorWorldContext().World()==World.Get()&&!World->bIsTearingDown&&GUndo==Undo&&!IsPlaySessionActive()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage())||Bad(TEXT("editor_context"),TEXT("same idle Editor/world/Undo"),TEXT("changed")));}
    static TArray<UObject*> Owned(UObject* O)
    {TArray<UObject*> Out;GetObjectsWithOuter(O,Out,EGetObjectsFlags::IncludeNestedObjects);Out.Sort([](const UObject& A,const UObject& B){return A.GetPathName()<B.GetPathName();});return Out;}
    static uint32 PinFlags(const UEdGraphPin* P)
    {return uint32(P->bHidden)|(uint32(P->bOrphanedPin)<<1)|(uint32(P->bAdvancedView)<<2)|(uint32(P->bNotConnectable)<<3)|(uint32(P->bDefaultValueIsReadOnly)<<4)|(uint32(P->bDefaultValueIsIgnored)<<5)|(uint32(P->ShouldSavePinIfOrphaned())<<6)|(uint32(P->bDeprecated)<<7)|(uint32(P->bDisplayAsMutableRef)<<8)|(uint32(P->bAllowFriendlyName)<<9);}
    static bool SameType(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    bool GraphCurrent()
    {
        if(!GraphsReady)return true;const auto& Rows=CandidatesReady?Candidates:Graphs;
        for(const auto& G:Rows)
        {
            if(!Loaded(G.Graph.Get())||G.Graph->GetOuter()!=BP.Get()||G.Graph->GraphGuid!=G.Guid||!G.Graph->Schema.GetGCPtr().IsResolved()||G.Graph->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||!G.Graph->SubGraphs.IsEmpty()||G.Graph->Nodes.Num()!=G.Nodes.Num())return Bad(TEXT("graph_role"),TEXT("same own K2 graph/GUID/node inventory"),TEXT("changed"));
            TSet<UEdGraphPin*> Pins;for(int32 I=0;I<G.Nodes.Num();++I){const auto& N=G.Nodes[I];if(!G.Graph->Nodes[I].IsResolved()||G.Graph->Nodes[I].Get()!=N.Node.Get()||!Loaded(N.Node.Get())||N.Node->NodeGuid!=N.Guid||N.Node->GetClass()!=N.Class||N.Node->NodePosX!=N.X||N.Node->NodePosY!=N.Y||!N.Node->NodeComment.Equals(N.Comment,ESearchCase::CaseSensitive)||!N.Node->GetPathName().Equals(N.Path,ESearchCase::CaseSensitive)||N.Node->Pins.Num()!=N.Pins.Num())return Bad(TEXT("node_role"),N.Guid.ToString(),TEXT("changed"));for(int32 J=0;J<N.Pins.Num();++J){UEdGraphPin* P=N.Node->Pins[J];if(P!=N.Pins[J].Pin||!P||Pins.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N.Node.Get())return Bad(TEXT("pin_membership"),N.Pins[J].Id.ToString(),TEXT("changed"));Pins.Add(P);}}
            for(const auto& N:G.Nodes)for(const auto& V:N.Pins){const auto* P=V.Pin;if(P->PinId!=V.Id||P->PinName!=V.Name||P->Direction!=V.Direction||!SameType(P->PinType,V.Type)||!P->DefaultObject.IsResolved()||P->DefaultObject.Get()!=V.Object||!P->DefaultValue.Equals(V.Raw,ESearchCase::CaseSensitive)||!P->AutogeneratedDefaultValue.Equals(V.Auto,ESearchCase::CaseSensitive)||FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(V.Text)||FTextInspector::GetFlags(P->DefaultTextValue)!=FTextInspector::GetFlags(V.Text)||FTextInspector::GetSharedDataId(P->PinFriendlyName)!=FTextInspector::GetSharedDataId(V.Friendly)||FTextInspector::GetFlags(P->PinFriendlyName)!=FTextInspector::GetFlags(V.Friendly)||PinFlags(P)!=V.Flags||P->LinkedTo!=V.Links||P->ParentPin!=V.ParentPin||P->SubPins!=V.Children)return Bad(TEXT("pin_semantic_current"),V.Id.ToString(),TEXT("changed"));for(auto* L:V.Links)if(!Pins.Contains(L)||!L->LinkedTo.Contains(V.Pin))return Bad(TEXT("reciprocal_link"),V.Id.ToString(),TEXT("changed"));if(V.ParentPin&&(!Pins.Contains(V.ParentPin)||!V.ParentPin->SubPins.Contains(V.Pin)))return Bad(TEXT("split_parent_membership"),V.Id.ToString(),TEXT("changed"));for(auto* L:V.Children)if(!Pins.Contains(L)||L->ParentPin!=V.Pin)return Bad(TEXT("split_child_membership"),V.Id.ToString(),TEXT("changed"));}
        }return true;
    }
    bool Current()
    {
        if(!Context()||!RolesCurrent(Anchors))return false;
        if(!Loaded(BP.Get())||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||!BP->NewVariables.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()||!BP->Timelines.IsEmpty()||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.Get()!=AActor::StaticClass()||!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript.Get()!=SCS.Get())return Bad(TEXT("host_roles"),TEXT("same idle direct Actor/SCS"),TEXT("changed"));
        if(!Rebinding&&(!Loaded(Gen.Get())||!Loaded(Skel.Get())||!Loaded(HostCDO.Get())||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.Get()!=Gen.Get()||BP->SkeletonGeneratedClass.Get()!=Skel.Get()||!Gen->ClassDefaultObject.IsResolved()||Gen->ClassDefaultObject.Get()!=HostCDO.Get()))return Bad(TEXT("class_slots"),TEXT("same held generated/skeleton/CDO slots"),TEXT("changed"));
        if(!Rebinding&&(!Loaded(Gen.Get())||!Loaded(Skel.Get())||Gen->GetOuter()!=BP->GetPackage()||Skel->GetOuter()!=BP->GetPackage()||Gen->GetPackage()!=BP->GetPackage()||Skel->GetPackage()!=BP->GetPackage()||!Gen->GetPathName().Equals(GenPath,ESearchCase::CaseSensitive)||!Skel->GetPathName().Equals(SkelPath,ESearchCase::CaseSensitive)||HostCDO->GetOuter()!=BP->GetPackage()||HostCDO->GetPackage()!=BP->GetPackage()||!HostCDO->GetPathName().Equals(CDOPath,ESearchCase::CaseSensitive)||!HostCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||Gen->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Skel->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Gen->ClassGeneratedBy.IsResolved()||Gen->ClassGeneratedBy.Get()!=BP.Get()||!Skel->ClassGeneratedBy.IsResolved()||Skel->ClassGeneratedBy.Get()!=BP.Get()||Gen->GetSuperClass()!=AActor::StaticClass()||Skel->GetSuperClass()!=AActor::StaticClass()||!Loaded(HostCDO.Get())||HostCDO->GetClass()!=Gen.Get()))return Bad(TEXT("host_generated_roles"),TEXT("same verified own generated/skeleton/CDO roles"),TEXT("changed"));
        if(Categories.Num()==4){const TArray<TArray<TObjectPtr<UEdGraph>>> Now={BP->UbergraphPages,BP->FunctionGraphs,BP->MacroGraphs,BP->DelegateSignatureGraphs};for(int32 I=0;I<4;++I){if(Now[I].Num()!=Categories[I].Num())return Bad(TEXT("authored_graph_category"),TEXT("original ordered arrays"),TEXT("changed"));for(int32 J=0;J<Now[I].Num();++J)if(!Now[I][J].IsResolved()||!Categories[I][J].IsResolved()||Now[I][J].Get()!=Categories[I][J].Get())return Bad(TEXT("authored_graph_category"),TEXT("original resolved handles"),TEXT("changed"));}}
        for(const auto& Source:Sources){if(!RolesCurrent(Source.Roles)||!Source.Class->ClassGeneratedBy.IsResolved()||Source.Class->ClassGeneratedBy.Get()!=nullptr||Source.Class->GetClassFlags()!=Source.ClassFlags||Source.Class->GetSuperClass()!=Source.Super||!Source.Class->ClassDefaultObject.IsResolved()||Source.Class->ClassDefaultObject.Get()!=Source.CDO.Get()||Source.Class->GetPackage()->GetPackageFlags()!=Source.PackageFlags||Source.Class->GetPackage()->IsDirty()||Owned(Source.CDO.Get())!=Source.Owned)return Bad(TEXT("native_source_roles"),TEXT("same native class/CDO/owned inventory"),TEXT("changed"));}
        TArray<USCS_Node*> ExpectedAll=All;if(Add&&Attached)ExpectedAll.Add(Selected.Get());if(SCS->GetAllNodes()!=ExpectedAll||SCS->GetRootNodes()!=Roots)return Bad(TEXT("scs_membership"),TEXT("original plus selected appended node only"),TEXT("changed"));
        for(const auto& V:Nodes){auto* N=V.Node.Get();auto Children=V.Children;if(Add&&Attached&&N==Parent.Get())Children.Add(Selected.Get());if(!Loaded(N)||N->GetOuter()!=SCS.Get()||N->VariableGuid!=V.Guid||N->GetVariableName()!=V.Name||N->GetChildNodes()!=Children||!N->ComponentClass.IsResolved()||N->ComponentClass.Get()!=V.Class||!N->ComponentTemplate.IsResolved()||(!Rebinding&&N!=Selected.Get()&&N->ComponentTemplate.Get()!=V.Template.Get()))return Bad(TEXT("scs_node_role"),V.Name.ToString(),TEXT("changed"));}
        if(SelectedReady&&!Rebinding)
        {
            if(!Loaded(Selected.Get())||Selected->GetOuter()!=SCS.Get()||Selected->VariableGuid!=NodeGuid||Selected->GetVariableName()!=FName(*Name)||!Selected->GetChildNodes().IsEmpty()||!Selected->ComponentClass.IsResolved()||Selected->ComponentClass.Get()!=UChildActorComponent::StaticClass()||!Selected->ComponentTemplate.IsResolved()||Selected->ComponentTemplate.Get()!=CAC.Get()||!Loaded(CAC.Get())||CAC->GetClass()!=UChildActorComponent::StaticClass()||CAC->GetOuter()!=Gen.Get()||CAC->IsRegistered()||CAC->IsPreRegistering()||CAC->IsActive()||CAC->HasBegunPlay()||CAC->IsRenderStateCreated()||CAC->IsPhysicsStateCreated()||CAC->GetOwner()||CAC->GetWorld()||CAC->GetChildActor()||CAC->GetAttachParent()||!CAC->GetAttachChildren().IsEmpty())return Bad(TEXT("selected_inactive_cac"),Name,TEXT("changed"));
            auto ChildClass=CAC->GetChildActorClass();if(!ChildClass.GetGCPtr().IsResolved())return Bad(TEXT("child_class_resolved"),TEXT("resolved selected class slot"),TEXT("unresolved"));if(!ClassMutable&&ChildClass.Get()!=ExpectedClass)return Bad(TEXT("ChildActorClass"),Pointer(ExpectedClass),Pointer(ChildClass.Get()));
            if(!CATMutable&&CAC->GetChildActorTemplate()!=CAT.Get())return Bad(TEXT("ChildActorTemplate_slot"),Pointer(CAT.Get()),Pointer(CAC->GetChildActorTemplate()));
            if(!TransformMutable&&(CAC->GetRelativeLocation()!=ExpectedLocation||CAC->GetRelativeRotation()!=ExpectedRotation||CAC->GetRelativeScale3D()!=ExpectedScale))return Bad(TEXT("selected_relative_transform"),TEXT("planned relative transform"),TEXT("changed"));
        }
        if(!CATMutable&&!Rebinding&&CAT.IsValid()){if(!RolesCurrent(CATRoles))return false;if(Owned(CAT.Get())!=CATOwned)return Bad(TEXT("CAT_owned_membership"),TEXT("same captured owned inventory"),TEXT("different"));}
        return GraphCurrent();
    }
    bool Fields(UObject* O,const TSet<FName>& Skip,FString& Out,TArray<TPair<FString,FString>>* Columns=nullptr)
    {
        if(!Current()||!Loaded(O))return false;const TStrongObjectPtr<UObject> Hold(O);const TStrongObjectPtr<UClass> Type(O->GetClass());TArray<FProperty*> Layout;for(TFieldIterator<FProperty> I(Type.Get(),EFieldIteratorFlags::IncludeSuper);I;++I)Layout.Add(*I);if(Layout.Num()>2048)return Bad(TEXT("field_budget"),TEXT("<=2048"),LexToString(Layout.Num()));
        Out.Reset();if(Columns)Columns->Reset();for(FProperty* P:Layout)if(!Skip.Contains(P->GetFName())){if(P->ArrayDim<1||P->ArrayDim>64)return Bad(TEXT("field_array_dim"),TEXT("1..64"),LexToString(P->ArrayDim));for(int32 I=0;I<P->ArrayDim;++I){FString V;const bool OK=P->ExportText_Direct(V,P->ContainerPtrToValuePtr<void>(O,I),P->ContainerPtrToValuePtr<void>(O,I),O,PPF_None);if(!Current())return false;TArray<FProperty*> Now;for(TFieldIterator<FProperty> F(Type.Get(),EFieldIteratorFlags::IncludeSuper);F;++F)Now.Add(*F);if(Now!=Layout)return Bad(TEXT("field_membership"),TEXT("same retained fields"),TEXT("changed"));if(!OK||Out.Len()+V.Len()>1048576)return Bad(TEXT("field_export"),TEXT("bounded successful export"),TEXT("incomplete"));if(Columns)Columns->Emplace(P->GetPathName()+TEXT("[")+LexToString(I)+TEXT("]"),LexToString(uint64(P->GetPropertyFlags()))+TEXT("=")+V);Out+=P->GetName()+TEXT(":")+LexToString(uint64(P->GetPropertyFlags()))+TEXT("=")+V+TEXT("\n");}}return true;
    }
    bool SourceFields(FSource& Source,TArray<FString>& Out)
    {Out.Reset();FString V;if(!Fields(Source.CDO.Get(),{},V))return false;Out.Add(V);for(auto* O:Source.Owned){if(!Fields(O,{},V))return false;Out.Add(V);}return true;}
    bool Check()
    {
        if(!Current())return false;
        for(auto& Source:Sources){TArray<FString> Now;if(!SourceFields(Source,Now))return false;if(Now!=Source.Fields)return Bad(TEXT("native_source_values"),TEXT("original CDO/subobject fields"),TEXT("changed"));}
        for(const auto& V:Nodes)if(V.Node.Get()!=Selected.Get()){FString Now;if(!Fields(V.Template.Get(),{},Now))return false;if(!Now.Equals(V.Values,ESearchCase::CaseSensitive))return Bad(TEXT("unselected_template_values"),V.Name.ToString(),TEXT("changed"));}
        if(SelectedReady&&!Rebinding){FString Now;TArray<TPair<FString,FString>> Columns;if(!Fields(CAC.Get(),{TEXT("ChildActorClass"),TEXT("ChildActorTemplate"),TEXT("ChildActor"),TEXT("ChildActorName"),TEXT("ActorOuter"),TEXT("RelativeLocation"),TEXT("RelativeRotation"),TEXT("RelativeScale3D"),TEXT("bComponentToWorldUpdated")},Now,&Columns))return false;if(!Now.Equals(SelectedValues,ESearchCase::CaseSensitive)){const FString ObjectPath=CAC->GetPathName();if(Columns.Num()!=SelectedColumns.Num())return Bad(TEXT("selected_field_inventory"),LexToString(SelectedColumns.Num()),LexToString(Columns.Num()),ObjectPath);for(int32 I=0;I<Columns.Num();++I)if(Columns[I].Key!=SelectedColumns[I].Key||Columns[I].Value!=SelectedColumns[I].Value)return Bad(*SelectedColumns[I].Key,SelectedColumns[I].Value,Columns[I].Value,ObjectPath);return Bad(TEXT("selected_other_values"),SelectedValues,Now,ObjectPath);}}
        if(CAT.IsValid()&&!CATMutable&&!Rebinding){TArray<FString> Now;FString V;if(!Fields(CAT.Get(),{},V))return false;Now.Add(V);for(auto* O:CATOwned){if(!Fields(O,{},V))return false;Now.Add(V);}if(Now!=CATValues)return Bad(TEXT("CAT_captured_values"),TEXT("current canonical CAT captured fields"),TEXT("changed"));}
        return Current();
    }
    bool Enter(const TCHAR* Stage)
    {if(!Check())return false;R.Stage=Stage;R.bMutationAttempted=true;R.bChangeStateKnown=false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetBoolField(TEXT("entered"),true);J->SetBoolField(TEXT("returned"),false);Calls.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("actual_call_prefix"),Calls);return true;}
    void Returned(){Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);}
    bool CaptureGraphs(bool AfterCompile)
    {
        const bool Ready=GraphsReady;GraphsReady=false;if(!Current()){GraphsReady=Ready;return false;}GraphsReady=Ready;
        TArray<UEdGraph*> Raw;BP->GetAllGraphs(Raw);if(Raw.Num()>64)return Bad(TEXT("graph_bound"),TEXT("<=64"),LexToString(Raw.Num()));Candidates.Reset();int32 PinCount=0;
        for(auto* G:Raw){if(!Loaded(G)||G->GetOuter()!=BP.Get()||!G->SubGraphs.IsEmpty())return Bad(TEXT("graph_ownership"),TEXT("own flat graph"),TEXT("different"));FGraph Row;Row.Graph.Reset(G);Row.Guid=G->GraphGuid;
            for(const auto& H:G->Nodes){if(!H.IsResolved()||!Loaded(H.Get()))return Bad(TEXT("graph_node_handle"),TEXT("resolved live node"),TEXT("different"));FGraphNode N;N.Node.Reset(H.Get());N.Guid=H->NodeGuid;N.Class=H->GetClass();N.X=H->NodePosX;N.Y=H->NodePosY;N.Comment=H->NodeComment;N.Path=H->GetPathName();Holds.Emplace(N.Class);
                for(auto* Q:H->Pins){if(!Q||Q->WasTrashed()||Q->GetOwningNodeUnchecked()!=H.Get()||!Q->DefaultObject.IsResolved()||Q->PinType.PinSubCategoryObject.IsStale()||Q->PinType.PinValueType.TerminalSubCategoryObject.IsStale()||++PinCount>16384)return Bad(TEXT("graph_pin_handle"),TEXT("current bounded owned pin"),TEXT("different"));FPin V;V.Pin=Q;V.Id=Q->PinId;V.Name=Q->PinName;V.Direction=Q->Direction;V.Type=Q->PinType;V.Text=Q->DefaultTextValue;V.Friendly=Q->PinFriendlyName;V.Raw=Q->DefaultValue;V.Auto=Q->AutogeneratedDefaultValue;V.Object=Q->DefaultObject.Get();V.Links=Q->LinkedTo;V.Children=Q->SubPins;V.ParentPin=Q->ParentPin;V.Flags=PinFlags(Q);if(V.Object)Holds.Emplace(V.Object);if(V.Type.PinSubCategoryObject.Get())Holds.Emplace(V.Type.PinSubCategoryObject.Get());if(V.Type.PinValueType.TerminalSubCategoryObject.Get())Holds.Emplace(V.Type.PinValueType.TerminalSubCategoryObject.Get());N.Pins.Add(MoveTemp(V));}Row.Nodes.Add(MoveTemp(N));}Candidates.Add(MoveTemp(Row));}
        CandidatesReady=true;GraphsReady=true;if(!Current())return false;FString NewState;
        for(auto& G:Candidates){NewState+=G.Graph->GetPathName()+G.Guid.ToString();for(auto& N:G.Nodes){auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("guid"),N.Guid.ToString());Meta->SetStringField(TEXT("class"),N.Class->GetPathName());Meta->SetNumberField(TEXT("x"),N.X);Meta->SetNumberField(TEXT("y"),N.Y);Meta->SetStringField(TEXT("comment"),N.Comment);FString MetaText;FJsonSerializer::Serialize(Meta,TJsonWriterFactory<>::Create(&MetaText));NewState+=MetaText;
            for(auto& V:N.Pins){FString Type;FEdGraphPinType Copy=V.Type;FEdGraphPinType::StaticStruct()->ExportText(Type,&Copy,nullptr,N.Node.Get(),PPF_None,nullptr);const bool Exported=!Type.IsEmpty();if(!Current())return false;if(!Exported)return Bad(TEXT("pin_type_export"),TEXT("success"),TEXT("incomplete"));const FText Text=V.Text,Friendly=V.Friendly;const FString Display=Text.ToString();if(!Current())return false;const FString Label=Friendly.ToString();if(!Current())return false;
                auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("id"),V.Id.ToString());J->SetStringField(TEXT("name"),V.Name.ToString());J->SetNumberField(TEXT("direction"),V.Direction);J->SetStringField(TEXT("type"),Type);J->SetStringField(TEXT("raw"),V.Raw);J->SetStringField(TEXT("auto"),V.Auto);J->SetStringField(TEXT("text"),Display);J->SetStringField(TEXT("friendly"),Label);J->SetNumberField(TEXT("textflags"),FTextInspector::GetFlags(V.Text));J->SetNumberField(TEXT("friendlyflags"),FTextInspector::GetFlags(V.Friendly));J->SetNumberField(TEXT("flags"),V.Flags);J->SetStringField(TEXT("object"),V.Object?V.Object->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:V.Links)Links.Add(MakeShared<FJsonValueString>(L->PinId.ToString()));J->SetArrayField(TEXT("links"),Links);TArray<TSharedPtr<FJsonValue>> Children;for(auto* L:V.Children)Children.Add(MakeShared<FJsonValueString>(L->PinId.ToString()));J->SetArrayField(TEXT("children"),Children);J->SetStringField(TEXT("parent"),V.ParentPin?V.ParentPin->PinId.ToString():FString());FString Encoded;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Encoded));NewState+=Encoded;}}
        }
        if(AfterCompile&&!NewState.Equals(GraphBefore,ESearchCase::CaseSensitive))return Bad(TEXT("compiler_graph_semantics"),TEXT("original captured authored graph/pin semantic values"),TEXT("different"));
        Graphs=MoveTemp(Candidates);CandidatesReady=false;if(!AfterCompile)GraphBefore=NewState;return Current();
    }
    bool FileState()
    {FString NewHash;int64 Bytes=-1;if(!Current())return false;TArray<uint8> Data;if(!FFileHelper::LoadFileToArray(Data,*TargetFile)||!Current())return Bad(TEXT("target_file_read"),TEXT("same readable existing package file"),TEXT("incomplete"));Bytes=Data.Num();NewHash=FMD5::HashBytes(Data.GetData(),Data.Num());if(Bytes!=FileBytes||!NewHash.Equals(FileHash,ESearchCase::CaseSensitive))return Bad(TEXT("target_disk"),FileHash,NewHash);return Current();}
    bool Reacquire()
    {
        Rebinding=true;auto Generated=BP->GeneratedClass,Skeleton=BP->SkeletonGeneratedClass;if(!Generated.GetGCPtr().IsResolved()||!Skeleton.GetGCPtr().IsResolved())return Bad(TEXT("compiler_class_handles"),TEXT("resolved returned classes"),TEXT("unresolved"));TStrongObjectPtr<UClass> G(Generated.Get()),K(Skeleton.Get());
        if(!Loaded(G.Get())||!Loaded(K.Get())||G->GetClass()!=UBlueprintGeneratedClass::StaticClass()||K->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!G->GetPathName().Equals(GenPath,ESearchCase::CaseSensitive)||!K->GetPathName().Equals(SkelPath,ESearchCase::CaseSensitive)||G->GetOuter()!=BP->GetPackage()||K->GetOuter()!=BP->GetPackage()||!G->ClassGeneratedBy.IsResolved()||!K->ClassGeneratedBy.IsResolved()||G->ClassGeneratedBy.Get()!=BP.Get()||K->ClassGeneratedBy.Get()!=BP.Get()||G->GetSuperClass()!=AActor::StaticClass()||K->GetSuperClass()!=AActor::StaticClass()||!G->ClassDefaultObject.IsResolved())return Bad(TEXT("compiler_class_role"),TEXT("planned own generated/skeleton identity/path/parent"),TEXT("different"));TStrongObjectPtr<UObject> D(G->ClassDefaultObject.Get());
        if(!Loaded(D.Get())||D->GetClass()!=G.Get()||D->GetOuter()!=BP->GetPackage()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!D->GetPathName().Equals(CDOPath,ESearchCase::CaseSensitive))return Bad(TEXT("compiler_CDO_role"),CDOPath,TEXT("different"));
        Retired.Emplace(Gen.Get());Retired.Emplace(Skel.Get());Retired.Emplace(HostCDO.Get());Gen.Reset(G.Get());Skel.Reset(K.Get());HostCDO.Reset(D.Get());
        // Immediately hold the returned selected template before any exporter/current callback. Old CAT/default subobjects remain opaque retained roles.
        if(!Selected->ComponentTemplate.IsResolved())return Bad(TEXT("compiler_template_handle"),TEXT("resolved"),TEXT("unresolved"));TStrongObjectPtr<UActorComponent> T(Selected->ComponentTemplate.Get());if(!Loaded(T.Get())||T->GetClass()!=UChildActorComponent::StaticClass()||T->GetOuter()!=G.Get())return Bad(TEXT("compiler_CAC_role"),TEXT("same named native CAC under returned generated class"),TEXT("different"));Retired.Emplace(CAC.Get());CAC.Reset(static_cast<UChildActorComponent*>(T.Get()));
        for(auto& V:Nodes){if(!V.Node->ComponentTemplate.IsResolved())return Bad(TEXT("compiler_existing_template_handle"),TEXT("resolved"),TEXT("unresolved"));UActorComponent* Raw=V.Node->ComponentTemplate.Get();TStrongObjectPtr<UActorComponent> Hold(Raw);if(!Loaded(Raw)||Raw->GetClass()!=V.Class||Raw->GetOuter()!=G.Get())return Bad(TEXT("compiler_existing_template_role"),V.Name.ToString(),TEXT("different"));Retired.Emplace(V.Template.Get());V.Template.Reset(Raw);}
        if(!CaptureGraphs(true))return false;Rebinding=false;if(!Current())return false;return Check();
    }
    bool PinCAT(bool NewExpected)
    {
        // The public getter is a plain slot read. Hold its return before any observer/current callback.
        AActor* Raw=CAC->GetChildActorTemplate();TStrongObjectPtr<AActor> ReturnedCAT(Raw);
        if(!Raw){if(CAT.IsValid())Retired.Emplace(CAT.Get());for(auto& V:CATRoles)Retired.Add(MoveTemp(V.Object));CAT.Reset();CATRoles.Reset();CATOwned.Reset();CATValues.Reset();CATMutable=false;return Current();}
        if(!Loaded(Raw)||Raw->GetClass()!=ATargetPoint::StaticClass()||Raw->GetOuter()!=CAC.Get()||Raw->GetPackage()!=BP->GetPackage()||!Raw->HasAllFlags(RF_ArchetypeObject|RF_Transactional|RF_Public)||Raw->GetWorld()||Raw->GetOwner()||Raw->HasActorBegunPlay())return Bad(TEXT("returned_CAT_role"),TEXT("own inactive native TargetPoint archetype under current CAC"),Pointer(Raw));
        const FString ExpectedName=CAC->GetName()+TEXT("_")+ATargetPoint::StaticClass()->GetName()+TEXT("_CAT");if(Raw->GetName()!=ExpectedName)return Bad(TEXT("CAT_name"),ExpectedName,Raw->GetName());
        if(!NewExpected&&Raw!=CAT.Get())return Bad(TEXT("CAT_unplanned_replacement"),Pointer(CAT.Get()),Pointer(Raw));
        if(CAT.IsValid()&&Raw!=CAT.Get())Retired.Emplace(CAT.Get());CAT.Reset(Raw);CATRoles.Reset();CATOwned=Owned(Raw);if(CATOwned.Num()>16)return Bad(TEXT("CAT_owned_bound"),TEXT("<=16"),LexToString(CATOwned.Num()));if(!Role(Raw,CATRoles))return false;
        for(auto* O:CATOwned){Holds.Emplace(O);if(!Loaded(O)||!O->GetClass()->HasAnyClassFlags(CLASS_Native)||O->GetPackage()!=BP->GetPackage()||!O->IsIn(Raw)||!Role(O,CATRoles))return Bad(TEXT("CAT_owned_role"),TEXT("held native same-package owned subobjects"),TEXT("unavailable"));}
        auto* T=static_cast<ATargetPoint*>(Raw);USceneComponent* Root=T->GetRootComponent();UBillboardComponent* Sprite=T->GetSpriteComponent();UArrowComponent* Arrow=T->GetArrowComponent();
        if(!Loaded(Root)||!Loaded(Sprite)||!Loaded(Arrow)||Root->GetClass()!=USceneComponent::StaticClass()||Sprite->GetClass()!=UBillboardComponent::StaticClass()||Arrow->GetClass()!=UArrowComponent::StaticClass()||Root->GetOuter()!=Raw||Sprite->GetOuter()!=Raw||Arrow->GetOuter()!=Raw||Root->GetOwner()!=Raw||Sprite->GetOwner()!=Raw||Arrow->GetOwner()!=Raw||Root->GetAttachParent()||Sprite->GetAttachParent()!=Root||Arrow->GetAttachParent()!=Sprite||Root->IsRegistered()||Sprite->IsRegistered()||Arrow->IsRegistered())return Bad(TEXT("CAT_TargetPoint_components"),TEXT("native SceneComp->Sprite->Arrow constructor roles"),TEXT("different"));
        CATMutable=false;if(!Current())return false;CATValues.Reset();FString V;if(!Fields(Raw,{},V))return false;CATValues.Add(V);for(auto* O:CATOwned){if(!Fields(O,{},V))return false;CATValues.Add(V);}return Current();
    }
    bool Bind(UBlueprint* B,USCS_Node* Existing,const FString& Requested,const FString& ParentName,bool Adding)
    {
        BP.Reset(B);Add=Adding;Name=Requested;Editor.Reset(GEditor);Undo=GUndo;if(!GEditor)return Refuse(TEXT("component_child_actor_editor_required"),TEXT("Existing Editor required."));World.Reset(GEditor->GetEditorWorldContext().World());if(!Context())return false;
        if(!Loaded(B)||B->GetClass()!=UBlueprint::StaticClass()||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.Get()!=AActor::StaticClass()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->SimpleConstructionScript.IsResolved()||!B->NewVariables.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->Timelines.IsEmpty())return Refuse(TEXT("component_child_actor_host_pending"),TEXT("First CAC slice requires an already loaded direct Actor Blueprint with no own user variables/interfaces/timelines; wider original owners remain incomplete."));
        Gen.Reset(B->GeneratedClass.Get());Skel.Reset(B->SkeletonGeneratedClass.Get());SCS.Reset(B->SimpleConstructionScript.Get());if(!Loaded(Gen.Get())||!Loaded(Skel.Get())||!Loaded(SCS.Get())||Gen->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Skel->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Gen->ClassDefaultObject.IsResolved())return Refuse(TEXT("component_child_actor_host_class_pending"),TEXT("Loaded generated/skeleton/SCS and existing CDO required."));HostCDO.Reset(Gen->ClassDefaultObject.Get());if(!Loaded(HostCDO.Get())||HostCDO->GetClass()!=Gen.Get())return Refuse(TEXT("component_child_actor_host_CDO_pending"),TEXT("Current generated Actor CDO required; no implicit initialization."));GenPath=Gen->GetPathName();SkelPath=Skel->GetPathName();CDOPath=HostCDO->GetPathName();
        if(!Role(B,Anchors)||!Role(SCS.Get(),Anchors))return false;
        All=SCS->GetAllNodes();Roots=SCS->GetRootNodes();if(Roots.Num()!=1||All.Num()!=(Adding?1:2)||!Loaded(Roots[0])||!Roots[0]->ComponentClass.IsResolved()||Roots[0]->ComponentClass.Get()!=USceneComponent::StaticClass())return Refuse(TEXT("component_child_actor_scs_pending"),TEXT("First profile is one own Scene root with no other component for Add, or root plus selected leaf CAC for Set."));Parent.Reset(Roots[0]);if(!ParentName.IsEmpty()&&FName(*ParentName)!=Parent->GetVariableName())return Refuse(TEXT("component_child_actor_parent_pending"),TEXT("Exact existing own Scene root parent required."));
        for(auto* N:All){if(!Loaded(N)||N->GetOuter()!=SCS.Get()||!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved())return Refuse(TEXT("component_child_actor_node_pending"),TEXT("Current own resolved SCS nodes required."));FNode V;V.Node.Reset(N);V.Template.Reset(N->ComponentTemplate.Get());V.Class=N->ComponentClass.Get();V.Guid=N->VariableGuid;V.Name=N->GetVariableName();V.Children=N->GetChildNodes();if(!Loaded(V.Template.Get())||V.Template->GetClass()!=V.Class||V.Template->GetOuter()!=Gen.Get())return Refuse(TEXT("component_child_actor_template_pending"),TEXT("Own inactive native template required."));Nodes.Add(MoveTemp(V));}
        if(!Adding){Selected.Reset(Existing);if(!Existing||Existing==Parent.Get()||!All.Contains(Existing)||Parent->GetChildNodes()!=TArray<USCS_Node*>{Existing}||!Existing->ComponentClass.IsResolved()||Existing->ComponentClass.Get()!=UChildActorComponent::StaticClass()||!Existing->ComponentTemplate.IsResolved())return Refuse(TEXT("component_child_actor_leaf_pending"),TEXT("Own exact CAC leaf under the Scene root required."));CAC.Reset(Cast<UChildActorComponent>(Existing->ComponentTemplate.Get()));NodeGuid=Existing->VariableGuid;SelectedReady=true;Attached=true;auto ChildClass=CAC->GetChildActorClass();if(!ChildClass.GetGCPtr().IsResolved())return Refuse(TEXT("component_child_actor_class_pending"),TEXT("Already resolved ChildActorClass required."));ExpectedClass=ChildClass.Get();if(ExpectedClass&&ExpectedClass!=ATargetPoint::StaticClass())return Refuse(TEXT("component_child_actor_class_pending"),TEXT("First raw class slice is exact native TargetPoint or null; generated/broader child classes remain incomplete."));ExpectedLocation=CAC->GetRelativeLocation();ExpectedRotation=CAC->GetRelativeRotation();ExpectedScale=CAC->GetRelativeScale3D();CATMutable=true;}
        else if(!Parent->GetChildNodes().IsEmpty())return Refuse(TEXT("component_child_actor_scs_pending"),TEXT("First Add requires a lone own Scene root."));
        Categories={B->UbergraphPages,B->FunctionGraphs,B->MacroGraphs,B->DelegateSignatureGraphs};
        for(UClass* K:{UChildActorComponent::StaticClass(),ATargetPoint::StaticClass()})
        {
            if(!Loaded(K)||!K->HasAnyClassFlags(CLASS_Native)||K->GetPackage()->IsDirty()||!K->ClassDefaultObject.IsResolved())return Refuse(TEXT("component_child_actor_native_source_unavailable"),TEXT("Exact native CAC and TargetPoint classes plus existing CDOs must be explicitly prepared outside this API."));FSource Source;Source.Class.Reset(K);Source.CDO.Reset(K->ClassDefaultObject.Get());if(!Loaded(Source.CDO.Get())||Source.CDO->GetClass()!=K||Source.CDO->GetOuter()!=K->GetOuter()||!Source.CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Refuse(TEXT("component_child_actor_native_CDO_unavailable"),TEXT("Existing exact native CDO role required."));Source.Owned=Owned(Source.CDO.Get());if(Source.Owned.Num()>16)return Refuse(TEXT("component_child_actor_native_source_bound"),TEXT("Bounded native source default subtree required."));if(K==UChildActorComponent::StaticClass()){auto* Default=static_cast<UChildActorComponent*>(Source.CDO.Get());auto Child=Default->GetChildActorClass();if(!Child.GetGCPtr().IsResolved()||Child.Get()!=nullptr||Default->GetChildActorTemplate()!=nullptr||Default->GetChildActor()!=nullptr)return Refuse(TEXT("component_child_actor_native_defaults_pending"),TEXT("First CAC Add source requires actual empty native ChildActorClass/CAT/runtime-child defaults; modified native CDO variants remain incomplete."));}Source.ClassFlags=K->GetClassFlags();Source.Super=K->GetSuperClass();Source.PackageFlags=K->GetPackage()->GetPackageFlags();if(!Role(K,Source.Roles)||!Role(Source.CDO.Get(),Source.Roles))return false;for(auto* O:Source.Owned)if(!Role(O,Source.Roles))return false;Sources.Add(MoveTemp(Source));
        }
        if(!Current()||(SelectedReady&&!PinCAT(true))||!CaptureGraphs(false))return false;for(auto& Source:Sources)if(!SourceFields(Source,Source.Fields))return false;
        for(auto& V:Nodes)if(V.Node.Get()!=Selected.Get()&&!Fields(V.Template.Get(),{},V.Values))return false;
        if(SelectedReady){if(!Fields(CAC.Get(),{TEXT("ChildActorClass"),TEXT("ChildActorTemplate"),TEXT("ChildActor"),TEXT("ChildActorName"),TEXT("ActorOuter"),TEXT("RelativeLocation"),TEXT("RelativeRotation"),TEXT("RelativeScale3D"),TEXT("bComponentToWorldUpdated")},SelectedValues,&SelectedColumns))return false;}
        const FString Package=B->GetPackage()->GetName();TargetFile=FPackageName::LongPackageNameToFilename(Package,FPackageName::GetAssetPackageExtension());TArray<uint8> Data;if(!FFileHelper::LoadFileToArray(Data,*TargetFile)||!Current())return Refuse(TEXT("component_child_actor_saved_source_required"),TEXT("First CAC scope requires an existing readable Blueprint package file; unsaved targets remain incomplete."));FileBytes=Data.Num();FileHash=FMD5::HashBytes(Data.GetData(),Data.Num());OtherDirty=DirtyPackages();OtherDirty.Remove(Package);R.Data->SetBoolField(TEXT("child_actor_component_authoring"),true);R.Data->SetBoolField(TEXT("child_actor_eager_setter_synthesized"),false);return Check()&&FileState();
    }

};

FMutationResult MutateNativeCAC(UBlueprint* B,USCS_Node* Existing,const FString& Name,const FString& ParentName,const FString& TransformJson,const TSharedPtr<FJsonValue>& Input,const FReadResult& Before,bool Add,bool Compile)
{
    FMutationResult R;R.bCompileRequested=Add&&Compile;R.Data->SetStringField(TEXT("blueprint_name"),Before.BlueprintPath);R.Data->SetStringField(TEXT("component_name"),Name);R.Data->SetStringField(TEXT("property_name"),Add?FString():TEXT("ChildActorClass"));R.Data->SetObjectField(TEXT("before"),Before.Data);R.Data->SetBoolField(TEXT("explicit_save_attempted"),false);R.Data->SetBoolField(TEXT("automatic_cleanup_attempted"),false);R.Data->SetBoolField(TEXT("eager_SetChildActorClass_attempted"),false);R.Data->SetBoolField(TEXT("PostEditChangeProperty_synthesized"),false);
    if(Before.bLoadedDuringRead){R.bChangeStateKnown=false;Fail(R,TEXT("component_child_actor_loaded_source_required"),TEXT("Shared preflight load prefix retained; first CAC scope requires separately loaded source before authoring."));return R;}
    FCACWork W(R);UClass* Requested=nullptr;FVector Location=FVector::ZeroVector,Scale=FVector::OneVector;FRotator Rotation=FRotator::ZeroRotator;
    if(Add)
    {
        if(!Input.IsValid()||Input->Type!=EJson::Object||!Input->AsObject()->Values.IsEmpty()){Fail(R,TEXT("component_child_actor_initial_properties_pending"),TEXT("First CAC Add accepts {} initial properties; old native ignored arbitrary component_properties and no generic CAC edit is synthesized."));return R;}
        TSharedPtr<FJsonValue> Transform;if(!ParseValue(TransformJson,Transform,R)||Transform->Type!=EJson::Object)return R;
        for(const auto& P:Transform->AsObject()->Values){const FString Key(P.Key.ToView());if(Key!=TEXT("location")&&Key!=TEXT("rotation")&&Key!=TEXT("scale")){Fail(R,TEXT("component_transform_invalid"),TEXT("Unknown transform field."));return R;}}
        for(int32 I=0;I<3;++I){const TCHAR* Key=I==0?TEXT("location"):I==1?TEXT("rotation"):TEXT("scale");auto V=Transform->AsObject()->TryGetField(Key);if(!V)continue;double XYZ[3];if(!VectorValues(V,3,XYZ)){Fail(R,TEXT("component_transform_invalid"),TEXT("Three finite bounded transform axes required."));return R;}if(I==0)Location=FVector(XYZ[0],XYZ[1],XYZ[2]);else if(I==1)Rotation=FRotator(XYZ[0],XYZ[1],XYZ[2]);else Scale=FVector(XYZ[0],XYZ[1],XYZ[2]);}
    }
    else
    {
        if(!Input.IsValid()||Input->Type!=EJson::String){Fail(R,TEXT("component_child_actor_class_string_required"),TEXT("Original Class property requires a class path JSON string; only empty string clears. JSON null is not promoted to clear."));return R;}
        const FString Path=Input->AsString();if(!Path.IsEmpty()&&!Path.Equals(TEXT("/Script/Engine.TargetPoint"),ESearchCase::CaseSensitive)){Fail(R,TEXT("component_child_actor_class_pending"),TEXT("First class input is exact loaded /Script/Engine.TargetPoint or empty string. Other original class aliases/generated/load cases remain incomplete."));return R;}Requested=Path.IsEmpty()?nullptr:ATargetPoint::StaticClass();
    }
    if(!W.Bind(B,Existing,Name,ParentName,Add))return R;
    const UClass* BeforeClass=W.ExpectedClass;const bool DirtyBefore=B->GetPackage()->IsDirty();const FString CATBefore=W.CAT.IsValid()?W.CAT->GetPathName():FString();R.Data->SetStringField(TEXT("child_class_before"),BeforeClass?BeforeClass->GetPathName():FString());R.Data->SetStringField(TEXT("CAT_before"),CATBefore);
    const bool Run=[&]()->bool
    {
        if(!RejectExternalInstances(B,R))return false;if(!W.Check())return false;
        if(Add){if(!W.Enter(TEXT("Blueprint_Modify")))return false;B->Modify();W.Returned();if(!W.Check())return false;}
        if(Add)
        {
            if(!W.Enter(TEXT("SCS_Modify")))return false;W.SCS->Modify();W.Returned();if(!W.Check())return false;
            const FName NewName(*Name);if(!SafeName(Name)||Name.Len()>100||NewName.IsNone())return W.Refuse(TEXT("component_name_invalid"),TEXT("Bounded nonempty component identifier required."));
            if(W.SCS->GenerateNewComponentName(UChildActorComponent::StaticClass(),NewName)!=NewName||!W.Check())return W.Stopped?false:W.Refuse(TEXT("component_name_collision"),TEXT("Requested exact component name collides; no automatic rename."));
            if(!W.Enter(TEXT("SCS_CreateNode")))return false;if(FindObject<UObject>(W.Gen.Get(),*(Name+USimpleConstructionScript::ComponentTemplateNameSuffix)))return W.Bad(TEXT("template_name_absence"),TEXT("absent exact requested slot"),TEXT("occupied"));
            USCS_Node* Raw=W.SCS->CreateNode(UChildActorComponent::StaticClass(),NewName);W.Selected.Reset(Raw);W.Returned();R.Data->SetBoolField(TEXT("creation_observed"),Raw!=nullptr);if(!FCACWork::Loaded(Raw)||!Raw->ComponentTemplate.IsResolved())return W.Bad(TEXT("CreateNode_return"),TEXT("loaded returned own node/template"),TEXT("unavailable"));UActorComponent* RawTemplate=Raw->ComponentTemplate.Get();TStrongObjectPtr<UActorComponent> ReturnedTemplate(RawTemplate);
            if(!FCACWork::Loaded(RawTemplate)||RawTemplate->GetClass()!=UChildActorComponent::StaticClass()||RawTemplate->GetOuter()!=W.Gen.Get()||Raw->GetOuter()!=W.SCS.Get()||Raw->GetVariableName()!=NewName||!Raw->ComponentClass.IsResolved()||Raw->ComponentClass.Get()!=UChildActorComponent::StaticClass()||!Raw->VariableGuid.IsValid())return W.Bad(TEXT("created_CAC_role"),TEXT("exact named own native CAC"),TEXT("different"));W.CAC.Reset(static_cast<UChildActorComponent*>(RawTemplate));W.NodeGuid=Raw->VariableGuid;W.ExpectedLocation=W.CAC->GetRelativeLocation();W.ExpectedRotation=W.CAC->GetRelativeRotation();W.ExpectedScale=W.CAC->GetRelativeScale3D();W.ExpectedClass=nullptr;W.SelectedReady=true;
            if(!W.Current()||!W.Fields(W.CAC.Get(),{TEXT("ChildActorClass"),TEXT("ChildActorTemplate"),TEXT("ChildActor"),TEXT("ChildActorName"),TEXT("ActorOuter"),TEXT("RelativeLocation"),TEXT("RelativeRotation"),TEXT("RelativeScale3D"),TEXT("bComponentToWorldUpdated")},W.SelectedValues,&W.SelectedColumns)||!W.Check())return false;
            if(!W.Enter(TEXT("Parent_Modify")))return false;W.Parent->Modify();W.Returned();if(!W.Check())return false;
            if(!W.Enter(TEXT("Parent_AddChildNode")))return false;W.Parent->AddChildNode(Raw);W.Attached=true;W.Returned();if(!W.Check())return false;
            if(!W.Enter(TEXT("CAC_SetRelativeLocation")))return false;W.TransformMutable=true;W.CAC->SetRelativeLocation(Location);W.Returned();if(!W.Current())return false;if(W.CAC->GetRelativeLocation()!=Location)return W.Bad(TEXT("requested_location"),Location.ToString(),W.CAC->GetRelativeLocation().ToString());W.ExpectedLocation=Location;W.TransformMutable=false;if(!W.Check())return false;
            if(!W.Enter(TEXT("CAC_SetRelativeRotation")))return false;W.TransformMutable=true;W.CAC->SetRelativeRotation(Rotation);W.Returned();if(!W.Current())return false;const FRotator Actual=W.CAC->GetRelativeRotation();if(!Actual.Equals(Rotation))return W.Bad(TEXT("requested_rotation"),Rotation.ToString(),Actual.ToString());W.ExpectedRotation=Actual;W.TransformMutable=false;if(!W.Check())return false;
            if(!W.Enter(TEXT("CAC_SetRelativeScale3D")))return false;W.TransformMutable=true;W.CAC->SetRelativeScale3D(Scale);W.Returned();if(!W.Current())return false;if(W.CAC->GetRelativeScale3D()!=Scale)return W.Bad(TEXT("requested_scale"),Scale.ToString(),W.CAC->GetRelativeScale3D().ToString());W.ExpectedScale=Scale;W.TransformMutable=false;if(!W.Check())return false;
            // Existing concrete Add contract: structural skeleton exposure for subsequent getter authoring; explicit full compile remains separately requested.
            if(!W.Enter(TEXT("MarkBlueprintAsStructurallyModified")))return false;R.Data->SetBoolField(TEXT("explicit_skeleton_compile_attempted"),true);W.Rebinding=true;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);W.Returned();if(!W.Reacquire())return false;
            if(Compile){if(!W.Enter(TEXT("CompileBlueprint")))return false;R.bCompileAttempted=true;W.Rebinding=true;auto Result=Cubeless::UE58::CompileBlueprint(Before.BlueprintPath,false,false);W.Returned();R.Data->SetObjectField(TEXT("compile"),Result);bool Compiled=false,Known=true,Unknown=false;if(!Result->TryGetBoolField(TEXT("compiled"),Compiled))return W.Bad(TEXT("compile_result"),TEXT("typed returned compiler outcome"),TEXT("missing"));R.Compiled=Compiled;Result->TryGetBoolField(TEXT("change_state_known"),Known);Result->TryGetBoolField(TEXT("outcome_unknown"),Unknown);if(!Known||Unknown)return W.Bad(TEXT("compile_result"),TEXT("known compiler outcome"),TEXT("unknown"));if(!W.Reacquire())return false;if(!Compiled)return W.Refuse(TEXT("component_compile_failed"),TEXT("CAC creation prefix retained; compiler reported failure, no save/retry."));}
        }
        else
        {
            // Exact original FClassProperty path: no eager SetChildActorClass, no notification synthesized and no implicit full compile/save.
            FClassProperty* Field=FindFProperty<FClassProperty>(UChildActorComponent::StaticClass(),TEXT("ChildActorClass"));if(!Field||Field->GetOwnerClass()!=UChildActorComponent::StaticClass()||Field->ArrayDim!=1||!Field->MetaClass.IsResolved()||Field->MetaClass.Get()!=AActor::StaticClass()||Field->HasSetterOrGetter())return W.Refuse(TEXT("component_child_actor_class_field_pending"),TEXT("Exact native raw scalar ChildActorClass property binding required."));
            const int32 FieldOffset=Field->GetOffset_ForInternal(),FieldSize=Field->GetSize();const EPropertyFlags FieldFlags=Field->GetPropertyFlags();
            if(!W.Enter(TEXT("Raw_FClassProperty_Write")))return false;
            FClassProperty* CurrentField=FindFProperty<FClassProperty>(UChildActorComponent::StaticClass(),TEXT("ChildActorClass"));
            if(CurrentField!=Field||!CurrentField||CurrentField->GetClass()!=FClassProperty::StaticClass()||CurrentField->GetOwnerClass()!=UChildActorComponent::StaticClass()||CurrentField->GetOffset_ForInternal()!=FieldOffset||CurrentField->GetSize()!=FieldSize||CurrentField->GetPropertyFlags()!=FieldFlags||CurrentField->ArrayDim!=1||CurrentField->HasSetterOrGetter()||!CurrentField->MetaClass.IsResolved()||CurrentField->MetaClass.Get()!=AActor::StaticClass()||!CurrentField->PropertyClass.IsResolved()||CurrentField->PropertyClass.Get()!=UClass::StaticClass())return W.Bad(TEXT("ChildActorClass_field_binding"),TEXT("same selected native property descriptor/meta-class"),TEXT("changed"));
            W.ClassMutable=true;CurrentField->SetObjectPropertyValue_InContainer(W.CAC.Get(),Requested);W.ExpectedClass=Requested;W.ClassMutable=false;W.Returned();R.Data->SetBoolField(TEXT("raw_class_assignment_performed"),true);if(!W.Check())return false;
            if(!W.Enter(TEXT("MarkBlueprintAsModified")))return false;FBlueprintEditorUtils::MarkBlueprintAsModified(B);W.Returned();if(!W.Check())return false;
        }
        return W.Check()&&W.FileState();
    }();
    if(W.Stopped)return R;
    if(!Run)return R;
    R.Data->SetStringField(TEXT("child_class_after"),W.ExpectedClass?W.ExpectedClass->GetPathName():FString());R.Data->SetStringField(TEXT("CAT_after"),W.CAT.IsValid()?W.CAT->GetPathName():FString());R.Data->SetBoolField(TEXT("CAT_slot_changed"),!CATBefore.Equals(W.CAT.IsValid()?W.CAT->GetPathName():FString(),ESearchCase::CaseSensitive));R.Data->SetBoolField(TEXT("CAT_runtime_spawn_verified"),false);R.Data->SetBoolField(TEXT("source_preserved"),true);R.Data->SetBoolField(TEXT("existing_scs_observations_preserved"),true);R.Data->SetBoolField(TEXT("captured_graph_fields_preserved"),true);R.Data->SetBoolField(TEXT("disk_preserved"),true);R.Data->SetBoolField(TEXT("dirty_before"),DirtyBefore);R.Data->SetBoolField(TEXT("dirty_after"),B->GetPackage()->IsDirty());
    auto Dirty=DirtyPackages();Dirty.Remove(B->GetPackage()->GetName());if(!W.Current())return R;if(Dirty!=W.OtherDirty){W.Bad(TEXT("other_dirty_packages"),TEXT("captured original set"),TEXT("different"));return R;}
    R.bChanged=Add||BeforeClass!=W.ExpectedClass||DirtyBefore!=B->GetPackage()->IsDirty();R.bChangeStateKnown=true;R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");R.Data->SetStringField(TEXT("child_actor_scope"),TEXT("One own inactive native CAC under a lone native Scene root in a loaded direct Actor BP without own variables/interfaces/timelines. Add {} and requested transforms; raw ChildActorClass TargetPoint/empty string Set only. CAT cache is independent from raw class and is not eagerly rebuilt; existing native TargetPoint SceneComp/Sprite/Arrow roles and captured values stay protected. Captured graph roles/GUIDs/ordered pin types/defaults/text/links and node class/position/comment only; not a complete arbitrary node-field proof. No generated child class/runtime spawn/CAT overrides/full original parity."));return R;
}

FMutationResult Mutate(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName,
    const FString& TransformJson, const FString& ParentComponent, const FString& PropertiesJson, bool bAdd, bool bCompile, const FString& PropertyName)
{
    FMutationResult R; R.bCompileRequested = bAdd && bCompile;FLightScope LightGuard(R);FHismScope HismGuard(R);int32 InitialPropertyCount=0;bool NiagaraHandled=false,GeneratedValueHandled=false,PCGHandled=false,CACHandled=false;
    R.Data->SetStringField(TEXT("blueprint_name"), BlueprintPath); R.Data->SetStringField(TEXT("component_name"), ComponentName);
    R.Data->SetStringField(TEXT("scope"), TEXT("local reachable SCS; fourteen existing exact Engine scene classes plus bounded empty HISM Add including native Point/Spot/Rect/Directional/Spline/ISM/SkeletalMeshComponent with typed properties, or add a saved clean generated non-scene ActorComponent with unchanged defaults; no inherited template edits"));
    R.Data->SetBoolField(TEXT("implicit_postload_work_possible"), true); R.Data->SetBoolField(TEXT("explicit_skeleton_compile_attempted"), false);
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("reachable local SCS identities/selected properties, Blueprint dirty/status and dirty package set; arbitrary PostLoad/notification state is not exhaustively observed"));
    TArray<FString> DirtyBefore; FReadResult Before, After;
    TStrongObjectPtr<UBlueprint> Blueprint;
    TStrongObjectPtr<UBlueprint> ComponentBlueprint;
    TStrongObjectPtr<UClass> AddedClass;
    TArray<TUniquePtr<FPreparedProperty>> Properties;
    FString ActualParent; bool bNoOp = false, bCreationObserved = false, bCreationIncomplete = false, bDirtyBaseline = false;
    const bool bGeneratedRequested = bAdd && ComponentType.StartsWith(TEXT("/Game/"));
    const bool bPCGRequested = bAdd && PCGComponentName(ComponentType);
    bool bNativeSplineRequested = false, bNativeIsmRequested = false, bNativeSkeletalRequested = false,bNativeLightRequested=false;
    TSharedPtr<FJsonObject> NativeSplineBefore, NativeIsmBefore, NativeSkeletalBefore;
    bool bClassLoadFailed = false, bReferenceObserved = false, bReferenceDirty = false, bReferenceLoadChanged = false;
    EBlueprintStatus ReferenceStatus = BS_Unknown;
    const bool bApplied = [&]() -> bool
    {
        if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("component_editor_state_invalid"), TEXT("Requires editor game thread outside PIE/SIE.")); }
        DirtyBefore = DirtyPackages(); bDirtyBaseline = true; R.Data->SetArrayField(TEXT("dirty_packages_before_load"), Strings(DirtyBefore));
        if (!SafeName(BlueprintPath) || !BlueprintPath.StartsWith(TEXT("/Game/")) || !SafeName(ComponentName) || !SafeName(ParentComponent, true))
        { return Fail(R, TEXT("component_selector_invalid"), TEXT("Use a /Game Blueprint and bounded exact component/parent names.")); }
        TSharedPtr<FJsonValue> Input;
        if (!ParseValue(PropertiesJson, Input, R)) { return false; }
        TSharedPtr<FJsonObject> PropertyObject = MakeShared<FJsonObject>();
        if (bAdd)
        {
            if (Input->Type != EJson::Object || Input->AsObject()->Values.Num() > MaxProperties) { return Fail(R, TEXT("component_properties_invalid"), TEXT("component_properties_json must be an object with at most 64 properties.")); }
            PropertyObject = Input->AsObject();InitialPropertyCount=PropertyObject->Values.Num();
            if (bPCGRequested && !ParentComponent.IsEmpty()) return Fail(R,TEXT("component_pcg_parent_unsupported"),TEXT("PCGComponent is non-scene; explicit parent is rejected as in the original handler."));
            if (bPCGRequested && !PropertyObject->Values.IsEmpty()) return Fail(R,TEXT("component_pcg_initial_properties_pending"),TEXT("This Add-only native slice accepts {} initial properties; the legacy ignored nonempty property contract remains pending, not a generic setter."));
            if (bGeneratedRequested && (!PropertyObject->Values.IsEmpty() || !ParentComponent.IsEmpty()))
            { return Fail(R, TEXT("generated_component_options_unsupported"), TEXT("Generated non-scene component add currently preserves its saved defaults and requires no parent_component; custom initial property edits remain pending.")); }
            TSharedPtr<FJsonValue> Transform;
            if (!ParseValue(TransformJson, Transform, R) || Transform->Type != EJson::Object) { return Fail(R, TEXT("component_transform_invalid"), TEXT("TransformJson must be an object of location, rotation and scale arrays.")); }
            for (const auto& Pair : Transform->AsObject()->Values)
            { const FString Key(Pair.Key.ToView()); if (Key != TEXT("location") && Key != TEXT("rotation") && Key != TEXT("scale")) { return Fail(R, TEXT("component_transform_invalid"), TEXT("Unknown transform field.")); } }
            const TCHAR* Keys[] = {TEXT("location"), TEXT("rotation"), TEXT("scale")};
            const TCHAR* Fields[] = {TEXT("RelativeLocation"), TEXT("RelativeRotation"), TEXT("RelativeScale3D")};
            for (int32 I = 0; I < 3; ++I)
            {
                for (const auto& Pair : PropertyObject->Values) { if (FString(Pair.Key.ToView()).Equals(Fields[I], ESearchCase::IgnoreCase)) { return Fail(R, TEXT("component_transform_conflict"), TEXT("Use transform arguments for RelativeLocation/Rotation/Scale3D on add.")); } }
                TSharedPtr<FJsonValue> Value = Transform->AsObject()->TryGetField(Keys[I]);
                if (!Value) { const double D = I == 2 ? 1.0 : 0.0; Value = MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(D), MakeShared<FJsonValueNumber>(D), MakeShared<FJsonValueNumber>(D)}); }
                double XYZ[3]; if (!VectorValues(Value, 3, XYZ)) { return Fail(R, TEXT("component_transform_invalid"), TEXT("Transform arrays require exactly three finite numbers with magnitude <=1e12.")); }
                if (bGeneratedRequested)
                {
                    const double Expected = I == 2 ? 1.0 : 0.0;
                    if (XYZ[0] != Expected || XYZ[1] != Expected || XYZ[2] != Expected)
                    { return Fail(R, TEXT("generated_component_options_unsupported"), TEXT("Non-scene ActorComponents have no transform; use identity location/rotation/scale.")); }
                }
                else if (!bPCGRequested) { PropertyObject->SetField(Fields[I], Value); }
            }
        }
        else { if (!SafeName(PropertyName)) { return Fail(R, TEXT("component_property_name_invalid"), TEXT("Property name is invalid.")); } PropertyObject->SetField(PropertyName, Input); }
        FString Code, Error;
        if (!ReadLocalComponents(BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, Before, Code, Error))
        { R.bChangeStateKnown = !Before.bLoadedDuringRead; return Fail(R, *Code, Error); }
        R.Data->SetObjectField(TEXT("before"), Before.Data); R.Data->SetBoolField(TEXT("loaded_during_preflight"), Before.bLoadedDuringRead);
        Blueprint.Reset(FindObject<UBlueprint>(nullptr, *Before.BlueprintPath));
        if (!Blueprint.IsValid() || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(AActor::StaticClass()) || !Blueprint->SimpleConstructionScript || !Blueprint->GeneratedClass || Blueprint->BlueprintType != BPTYPE_Normal
            || Blueprint->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
        { return Fail(R, TEXT("component_blueprint_unsupported"), TEXT("Requires a normal Actor Blueprint with valid generated class and local SCS; unsaved /Game Blueprints are allowed.")); }
        if (Blueprint->bBeingCompiled || Blueprint->bQueuedForCompilation || Blueprint->bSuppressStructurallyModified || Blueprint->bIsRegeneratingOnLoad || Blueprint->Status == BS_BeingCreated)
        { return Fail(R, TEXT("component_blueprint_state_unsupported"), TEXT("Component changes require an idle Blueprint with no queued or suppressed structural work.")); }
        if (!NativeRuntimeParents(Blueprint.Get(), R)) { return false; }
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        USCS_Node* Node = nullptr; USCS_Node* Parent = nullptr; UActorComponent* Template = nullptr;
        UClass* Class = bAdd&&CACName(ComponentType)?UChildActorComponent::StaticClass():bAdd ? (bPCGRequested ? FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGComponent")) : SupportedClass(ComponentType)) : nullptr;
        if (bAdd)
        {
            if (bGeneratedRequested)
            {
                if (!SafeName(ComponentType) || !FPackageName::IsValidObjectPath(ComponentType) || ComponentType.Contains(TEXT(":")))
                { return Fail(R, TEXT("generated_component_class_path_invalid"), TEXT("Use the exact bounded /Game generated class object path, without subobjects or export-text syntax.")); }
                const FString SourcePackage = FPackageName::ObjectPathToPackageName(ComponentType);
                UBlueprint* LoadedSource = FindObject<UBlueprint>(nullptr, *(SourcePackage + TEXT(".") + FPackageName::GetShortName(SourcePackage)));
                const bool SourceWasLoaded = IsValid(LoadedSource);
                const bool SourceDirtyBefore = SourceWasLoaded && LoadedSource->GetOutermost()->IsDirty();
                const EBlueprintStatus SourceStatusBefore = SourceWasLoaded ? LoadedSource->Status.GetValue() : BS_Unknown;
                auto LoadBefore = MakeShared<FJsonObject>();
                LoadBefore->SetBoolField(TEXT("blueprint_was_loaded"), SourceWasLoaded);
                if (SourceWasLoaded)
                { LoadBefore->SetBoolField(TEXT("dirty"), SourceDirtyBefore); LoadBefore->SetNumberField(TEXT("status"), static_cast<int32>(SourceStatusBefore)); }
                R.Data->SetObjectField(TEXT("component_reference_before_load"), LoadBefore);
                R.Data->SetBoolField(TEXT("component_class_load_attempted"), true);
                R.Data->SetBoolField(TEXT("component_class_was_loaded"), FindObject<UClass>(nullptr, *ComponentType) != nullptr);
                Class = LoadObject<UClass>(nullptr, *ComponentType);
                bClassLoadFailed = !IsValid(Class);
                ComponentBlueprint.Reset(IsValid(Class) ? Cast<UBlueprint>(Class->ClassGeneratedBy) : FindObject<UBlueprint>(nullptr, *(SourcePackage + TEXT(".") + FPackageName::GetShortName(SourcePackage))));
                bClassLoadFailed |= !ComponentBlueprint.IsValid();
                if (ComponentBlueprint.IsValid())
                {
                    bReferenceObserved = true; bReferenceDirty = ComponentBlueprint->GetOutermost()->IsDirty(); ReferenceStatus = ComponentBlueprint->Status;
                    auto LoadAfter = MakeShared<FJsonObject>(); LoadAfter->SetStringField(TEXT("blueprint_path"), ComponentBlueprint->GetPathName());
                    LoadAfter->SetBoolField(TEXT("dirty"), bReferenceDirty); LoadAfter->SetNumberField(TEXT("status"), static_cast<int32>(ReferenceStatus));
                    R.Data->SetObjectField(TEXT("component_reference_after_load"), LoadAfter);
                    bReferenceLoadChanged = SourceWasLoaded && (SourceDirtyBefore != bReferenceDirty || SourceStatusBefore != ReferenceStatus);
                    R.Data->SetBoolField(TEXT("component_reference_load_changed_observed_state"), bReferenceLoadChanged);
                }
                const auto Inspection = GeneratedComponent::InspectSavedClass(Class); R.Data->SetObjectField(TEXT("generated_component_inspection"), Inspection);
                if (!IsValid(Class) || Class->GetPathName() != ComponentType || !Inspection->GetBoolField(TEXT("success")))
                { return Fail(R, TEXT("generated_component_class_rejected"), TEXT("Requested exact saved runtime component class failed inspection; inspect generated_component_inspection and load observations. No component was created.")); }
                if (bReferenceLoadChanged)
                { return Fail(R, TEXT("generated_component_load_changed_state"), TEXT("Reference load changed an already loaded component Blueprint's dirty/status state; inspect it before authoring.")); }
            }
            if(Class==UChildActorComponent::StaticClass()){CACHandled=true;if(InitialPropertyCount!=0)return Fail(R,TEXT("component_child_actor_initial_properties_pending"),TEXT("First CAC Add accepts original {} initial properties only; common transform injection is not a caller property."));const TSharedPtr<FJsonValue> OriginalEmpty=MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());R=MutateNativeCAC(Blueprint.Get(),nullptr,ComponentName,ParentComponent,TransformJson,OriginalEmpty,Before,true,bCompile);return R.bSuccess;}
            if (!Class) { return Fail(R, TEXT("component_class_unsupported"), TEXT("Supported: fourteen existing exact native Engine classes, bounded empty native HISM Add, or an exact saved clean generated non-scene ActorComponent class.")); }
            if(Class==UHierarchicalInstancedStaticMeshComponent::StaticClass()&&InitialPropertyCount!=0)return Fail(R,TEXT("component_hism_initial_properties_pending"),TEXT("HISM Add currently preserves native defaults and accepts {} initial properties plus explicit transforms only; generic HISM writes/instance data remain pending."));
            AddedClass.Reset(Class);
            if (Before.NodeVisits >= MaxNodes) { return Fail(R, TEXT("component_budget_exceeded"), TEXT("Add would exceed the 10000 local node observation budget.")); }
            if (ComponentName.Len() > 100 || ComponentName.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { return Fail(R, TEXT("component_name_invalid"), TEXT("New variable name must be 1..100 characters and cannot be None.")); }
            for (int32 I = 0; I < ComponentName.Len(); ++I)
            { const TCHAR C = ComponentName[I]; if (!(C == '_' || (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (I > 0 && C >= '0' && C <= '9'))) { return Fail(R, TEXT("component_name_invalid"), TEXT("This slice requires an ASCII identifier component name.")); } }
            const FName RequestedName(*ComponentName);
            if (FindObject<UObject>(Blueprint->GeneratedClass, *(ComponentName + USimpleConstructionScript::ComponentTemplateNameSuffix)))
            { return Fail(R, TEXT("component_template_slot_collision"), TEXT("A generated-class template already occupies this slot; no orphan relocation is permitted.")); }
            // Public naming utility may initialize the engine's legacy AllNodes cache.
            R.Data->SetBoolField(TEXT("name_preflight_may_initialize_scs_cache"), true);
            if (SCS->GenerateNewComponentName(Class, RequestedName) != RequestedName)
            { return Fail(R, TEXT("component_name_collision"), TEXT("The requested name conflicts with a component, variable, function or native subobject.")); }
            ActualParent = ParentComponent;
            if (!bGeneratedRequested && !bPCGRequested && ActualParent.IsEmpty() && Before.NodeVisits > 0)
            {
                if (SCS->GetRootNodes().Num() != 1) { return Fail(R, TEXT("component_parent_ambiguous"), TEXT("Specify one exact local scene parent when the SCS has multiple roots.")); }
                ActualParent = SCS->GetRootNodes()[0]->GetVariableName().ToString();
            }
            if (!ActualParent.IsEmpty()) { Parent = Before.LocalNodes.FindRef(ActualParent).Get(); if (!Parent || !Cast<USceneComponent>(Parent->ComponentTemplate)) { return Fail(R, TEXT("component_parent_invalid"), TEXT("Parent must be an existing exact local scene component.")); } }
            if (bPCGRequested) { PCGHandled=true; return AddNativePCG(Blueprint.Get(),Class,ComponentName,TransformJson,Before,R); }
            if (NiagaraComponentName(ComponentType)) { NiagaraHandled=true; if(InitialPropertyCount!=0)return Fail(R,TEXT("component_niagara_initial_properties_pending"),TEXT("Fresh Niagara Add accepts only empty initial properties; use the separate exact raw Set calls.")); return MutateNativeNiagara(Blueprint.Get(),nullptr,Parent,ComponentName,PropertyObject.ToSharedRef(),Before,true,R); }
            bNativeSplineRequested = Class == USplineComponent::StaticClass();
            bNativeIsmRequested = Class == UInstancedStaticMeshComponent::StaticClass()||Class==UHierarchicalInstancedStaticMeshComponent::StaticClass();
            bNativeSkeletalRequested = Class == USkeletalMeshComponent::StaticClass();
            if (bNativeSplineRequested)
            {
                NativeSplineBefore = NativeSplineSourceObservation(Class);
                R.Data->SetObjectField(TEXT("native_spline_source_before"), NativeSplineBefore);
                if (!NativeSplineBefore->GetBoolField(TEXT("native_identity_verified")))
                { return Fail(R, TEXT("component_native_spline_source_rejected"), TEXT("Requires the exact clean loaded runtime Engine.SplineComponent class and its already-created native CDO; no component or CDO was created.")); }
                Template = Cast<UActorComponent>(Class->GetDefaultObject(false));
            }
            else if (bNativeIsmRequested)
            {
                NativeIsmBefore = NativeIsmSourceObservation(Class);
                R.Data->SetObjectField(TEXT("native_ism_source_before"), NativeIsmBefore);
                if (!NativeIsmBefore->GetBoolField(TEXT("native_identity_verified")))
                { return Fail(R, TEXT("component_native_ism_source_rejected"), Class==UHierarchicalInstancedStaticMeshComponent::StaticClass()?TEXT("Requires the exact clean loaded Engine HISM class and existing empty inactive native CDO; no component or CDO was created."):TEXT("Requires the exact clean loaded runtime Engine.InstancedStaticMeshComponent class and its already-created native CDO; no component or CDO was created.")); }
                Template = Cast<UActorComponent>(Class->GetDefaultObject(false));
            }
            else if (bNativeSkeletalRequested)
            {
                NativeSkeletalBefore = NativeSkeletalSourceObservation(Class);
                R.Data->SetObjectField(TEXT("native_skeletal_source_before"), NativeSkeletalBefore);
                if (!NativeSkeletalBefore->GetBoolField(TEXT("native_identity_verified")))
                { return Fail(R, TEXT("component_native_skeletal_source_rejected"), TEXT("Requires the exact clean loaded runtime Engine.SkeletalMeshComponent class and existing normal native CDO; no component or CDO creation was dispatched.")); }
                Template = Cast<UActorComponent>(Class->GetDefaultObject(false));
            }
            else if(NativeLightClass(Class)){if(Class==UDirectionalLightComponent::StaticClass()&&!LightGuard.ArmDirectional(PropertyObject.ToSharedRef(),true))return false;bNativeLightRequested=true;if(!LightGuard.Bind(Blueprint.Get(),Class,ComponentName,nullptr))return false;Template=LightGuard.Default.Get();}
            else { Template = CastChecked<UActorComponent>(Class->GetDefaultObject()); }
        }
        else
        {
            Node = Before.LocalNodes.FindRef(ComponentName).Get();
            if (!Node || !Node->ComponentTemplate) { return Fail(R, TEXT("component_not_found"), TEXT("No exact local reachable component exists.")); }
            Template = Node->ComponentTemplate; Class = Template->GetClass();
            if(Cast<UBlueprintGeneratedClass>(Class)&&!Class->IsChildOf(USceneComponent::StaticClass())){GeneratedValueHandled=true;return MutateGeneratedValue(Blueprint.Get(),Node,PropertyName,Input,Before,R);}
            if(Class==UChildActorComponent::StaticClass()){CACHandled=true;if(!PropertyName.Equals(TEXT("ChildActorClass"),ESearchCase::IgnoreCase))return Fail(R,TEXT("component_child_actor_property_pending"),TEXT("First CAC Set supports ChildActorClass only; broader original properties remain incomplete."));R=MutateNativeCAC(Blueprint.Get(),Node,ComponentName,FString(),FString(),Input,Before,false,false);return R.bSuccess;}
            if (SupportedClass(Class->GetPathName()) != Class) { return Fail(R, TEXT("component_class_unsupported"), TEXT("Only the fourteen supported exact native Engine component classes can be modified.")); }
            if(Class==UHierarchicalInstancedStaticMeshComponent::StaticClass())return Fail(R,TEXT("component_hism_generic_property_pending"),TEXT("Use the bounded HISM mesh/material setter; generic HISM properties/instance arrays remain pending."));
            if (Class->GetPathName() == TEXT("/Script/Niagara.NiagaraComponent")) { NiagaraHandled=true; return MutateNativeNiagara(Blueprint.Get(),Node,nullptr,ComponentName,PropertyObject.ToSharedRef(),Before,false,R); }
            if(Class==UDirectionalLightComponent::StaticClass()&&!LightGuard.ArmDirectional(PropertyObject.ToSharedRef(),false))return false;
            if(NativeLightClass(Class)){bNativeLightRequested=true;if(!LightGuard.Bind(Blueprint.Get(),Class,ComponentName,Node))return false;
                if((PropertyName.Equals(TEXT("LightFunctionMaterial"),ESearchCase::IgnoreCase)||PropertyName.Equals(TEXT("IESTexture"),ESearchCase::IgnoreCase))&&!LightGuard.ArmReference(PropertyName))return false;}
            bNativeSplineRequested = Class == USplineComponent::StaticClass();
            bNativeIsmRequested = Class == UInstancedStaticMeshComponent::StaticClass()||Class==UHierarchicalInstancedStaticMeshComponent::StaticClass();
            bNativeSkeletalRequested = Class == USkeletalMeshComponent::StaticClass();
            if (bNativeSplineRequested)
            {
                NativeSplineBefore = NativeSplineSourceObservation(Class);
                R.Data->SetObjectField(TEXT("native_spline_source_before"), NativeSplineBefore);
                if (!NativeSplineBefore->GetBoolField(TEXT("native_identity_verified")))
                { return Fail(R, TEXT("component_native_spline_source_rejected"), TEXT("Requires the exact clean loaded runtime Engine.SplineComponent class and its already-created native CDO; no property was written.")); }
            }
            if (bNativeIsmRequested)
            {
                NativeIsmBefore = NativeIsmSourceObservation(Class);
                R.Data->SetObjectField(TEXT("native_ism_source_before"), NativeIsmBefore);
                if (!NativeIsmBefore->GetBoolField(TEXT("native_identity_verified")))
                { return Fail(R, TEXT("component_native_ism_source_rejected"), TEXT("Requires the exact clean loaded runtime Engine.InstancedStaticMeshComponent class and its already-created native CDO; no property was written.")); }
            }
            if (bNativeSkeletalRequested)
            {
                NativeSkeletalBefore = NativeSkeletalSourceObservation(Class);
                R.Data->SetObjectField(TEXT("native_skeletal_source_before"), NativeSkeletalBefore);
                if (!NativeSkeletalBefore->GetBoolField(TEXT("native_identity_verified")))
                { return Fail(R, TEXT("component_native_skeletal_source_rejected"), TEXT("Requires the exact clean loaded runtime Engine.SkeletalMeshComponent class and existing normal native CDO; no property was written.")); }
            }
        }
        if(Class==UHierarchicalInstancedStaticMeshComponent::StaticClass()&&!HismGuard.Bind(Blueprint.Get(),ComponentName))return false;
        TSet<FName> PreparedNames;
        for (const auto& Pair : PropertyObject->Values)
        {
            TUniquePtr<FPreparedProperty> Prepared;
            if (!PrepareProperty(Template, FString(Pair.Key.ToView()), Pair.Value, Prepared, R)) { return false; }
            if (PreparedNames.Contains(Prepared->Property->GetFName())) { return Fail(R, TEXT("component_property_duplicate"), TEXT("Duplicate case-insensitive property names are not accepted.")); }
            if(!LightGuard.Check()||!HismGuard.Check())return false;PreparedNames.Add(Prepared->Property->GetFName()); Properties.Add(MoveTemp(Prepared));
        }
        if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Template))
        {
            int32 RadiusIndex = INDEX_NONE, HeightIndex = INDEX_NONE;
            float Radius = Capsule->GetUnscaledCapsuleRadius(), HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
            for (int32 I = 0; I < Properties.Num(); ++I)
            {
                if (Properties[I]->ShapeSize == EShapeSize::CapsuleRadius) { RadiusIndex = I; Radius = *static_cast<const float*>(Properties[I]->Value); }
                if (Properties[I]->ShapeSize == EShapeSize::CapsuleHalfHeight) { HeightIndex = I; HalfHeight = *static_cast<const float*>(Properties[I]->Value); }
            }
            if (RadiusIndex != INDEX_NONE || HeightIndex != INDEX_NONE)
            {
                if (!FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || Radius < 0.f || HalfHeight < Radius)
                { return Fail(R, TEXT("component_shape_size_invalid"), TEXT("CapsuleRadius must be <= CapsuleHalfHeight; provide a valid final pair on add, or preserve the companion dimension on set.")); }
                // JSON objects are unordered. Growing beyond the old half-height
                // needs height first; shrinking below the old radius needs radius
                // first. Both dimensions were validated before creating a node.
                const bool bHeightFirst = Radius > Capsule->GetUnscaledCapsuleHalfHeight();
                if (RadiusIndex != INDEX_NONE && HeightIndex != INDEX_NONE && ((HeightIndex < RadiusIndex) != bHeightFirst)) { Properties.Swap(RadiusIndex, HeightIndex); }
            }
        }
        TArray<const FPreparedProperty*> ShapeProperties;
        for (const auto& P : Properties) { if (P->ShapeSize != EShapeSize::None) { ShapeProperties.Add(P.Get()); } }
        if (!ValidateShapeSequence(Template, ShapeProperties, R)) { return false; }
        if (!ShapeProperties.IsEmpty())
        {
            R.Data->SetStringField(TEXT("shape_size_api"), TEXT("public SetSphereRadius/SetBoxExtent/SetCapsuleSize plus editor property notifications and local inherited-value propagation"));
            R.Data->SetBoolField(TEXT("shape_update_overlaps_requested"), false);
            R.Data->SetStringField(TEXT("shape_verification_scope"), TEXT("unscaled stored dimensions; physics simulation/collision fallback geometry is not verified"));
        }
        if (!bAdd)
        {
            const auto& P = Properties[0];
            bNoOp = P->Property->Identical(P->Value, P->Property->ContainerPtrToValuePtr<void>(Template))
                && (!P->bSocket || Node->AttachToName == CastFieldChecked<FNameProperty>(P->Property)->GetPropertyValue(P->Value));
            if (bNoOp) { R.Data->SetBoolField(TEXT("no_op"), true); return true; }
        }
        if (!RejectExternalInstances(Blueprint.Get(), R) || (!bAdd && !CheckPropagation(Template, Properties, Blueprint->GetOutermost(), R))) { return false; }
        R.Stage = TEXT("mutation"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
        if(!LightGuard.Check()||!HismGuard.Check())return false;Blueprint->Modify();if(!LightGuard.Check()||!HismGuard.Check())return false;SCS->Modify();if(!LightGuard.Check()||!HismGuard.Check())return false;
        if (bAdd)
        {
            bCreationIncomplete = true;
            Node = SCS->CreateNode(Class, FName(*ComponentName));
            if(HismGuard.Active)
            {
                const bool Returned=Node!=nullptr;R.Data->SetBoolField(TEXT("hism_create_node_returned"),Returned);bCreationObserved|=Returned;R.Data->SetBoolField(TEXT("creation_observed"),bCreationObserved);
                R.Data->SetBoolField(TEXT("hism_created_identity_observed"),false);R.Data->SetBoolField(TEXT("hism_created_template_identity_observed"),false);
                // Observe only safe identities on this returned object, before admission can stop SDK follow-up.
                if(HismGuard.Context()&&FHismScope::Loaded(Node))
                {
                    const TStrongObjectPtr<USCS_Node> KeepReturned(Node);R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());R.Data->SetStringField(TEXT("created_node_guid"),Node->VariableGuid.ToString(EGuidFormats::Digits));R.Data->SetBoolField(TEXT("hism_created_identity_observed"),true);
                    UActorComponent* ReturnedTemplate=Node->ComponentTemplate.Get();if(FHismScope::Loaded(ReturnedTemplate)){const TStrongObjectPtr<UActorComponent> KeepReturnedTemplate(ReturnedTemplate);R.Data->SetStringField(TEXT("created_template_path"),ReturnedTemplate->GetPathName());R.Data->SetBoolField(TEXT("hism_created_template_identity_observed"),true);}
                }
                if(!HismGuard.Adopt(Node,false))return false;
            }
            if(bNativeLightRequested&&!LightGuard.Adopt(Node,false))return false;if(!LightGuard.Check()||!HismGuard.Check())return false;
            if (Node)
            {
                bCreationObserved = true; R.Data->SetStringField(TEXT("created_node_path"), Node->GetPathName());
                if (Node->ComponentTemplate) { R.Data->SetStringField(TEXT("created_template_path"), Node->ComponentTemplate->GetPathName()); }
            }
            if (!Node || Node->GetVariableName().ToString() != ComponentName || !Node->ComponentTemplate)
            { return Fail(R, TEXT("component_create_failed"), TEXT("CreateNode did not return the requested identity; inspect partial state, no automatic cleanup/retry.")); }
            if (Parent) { Parent->Modify();if(!LightGuard.Check()||!HismGuard.Check())return false; Parent->AddChildNode(Node); } else { SCS->AddNode(Node); }
            if(bNativeLightRequested&&!LightGuard.Attach(Parent))return false;
            if(HismGuard.Active&&!HismGuard.Attach())return false;
            bCreationIncomplete = false;
            Template = Node->ComponentTemplate;
            if (!CheckPropagation(Template, Properties, Blueprint->GetOutermost(), R)) { return false; }
        }
        if(!LightGuard.Check()||!HismGuard.Check())return false;Node->Modify();if(!LightGuard.Check()||!HismGuard.Check())return false;Template->Modify();if(!LightGuard.Check()||!HismGuard.Check())return false;
        for (const auto& P : Properties)
        {
            // Reacquire after every notification: it can replace component templates.
            FReadResult Current;
            if(!LightGuard.Check()||!HismGuard.Check())return false;
            if (!ReadLocalComponents(Before.BlueprintPath, ComponentName, FString(), false, MaxNodes, MaxJsonChars, Current, Code, Error)) { return Fail(R, TEXT("component_mutation_readback_failed"), Error); }
            if(!LightGuard.Check()||!HismGuard.Check())return false;Node = Current.LocalNodes.FindRef(ComponentName).Get(); Template = Node ? Node->ComponentTemplate.Get() : nullptr;
            if(!LightGuard.ReferenceMode)
            {if (!Template || Template->GetClass() != Class || !ApplyProperty(Template, Node, *P)) { return Fail(R, TEXT("component_property_apply_failed"), TEXT("Property notification/application failed after mutation began.")); }}
            else
            {
                if (!Template || Template->GetClass() != Class) { return Fail(R,TEXT("component_property_apply_failed"),TEXT("Current template class differs before application.")); }
                if(!LightGuard.BeginReferenceApply(*P))return false;const bool Applied=ApplyProperty(Template,Node,*P);if(!LightGuard.EndReferenceApply(Applied))return false;
                if(!Applied){return Fail(R,TEXT("component_property_apply_failed"),TEXT("Property notification/application failed after mutation began."));}
            }
            if(!LightGuard.Check(true)||!HismGuard.Check())return false;
        }
        if (bGeneratedRequested || (bAdd && (bNativeSplineRequested || bNativeIsmRequested || bNativeSkeletalRequested || bNativeLightRequested)))
        {
            // Public structural refresh creates the SCS object property in the
            // skeleton so the next batch child can author its getter. This is
            // not a full compile, and the manager excludes skeleton-only saves.
            R.Data->SetBoolField(TEXT("explicit_skeleton_compile_attempted"), true);
            if (bNativeIsmRequested) { R.Data->SetBoolField(TEXT("native_ism_structural_refresh_invoked"), true); }
            if (bNativeSkeletalRequested) { R.Data->SetBoolField(TEXT("native_skeletal_structural_refresh_invoked"), true); }
            if(!LightGuard.Check()||!HismGuard.Check())return false;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint.Get());
            if(bNativeLightRequested&&!LightGuard.Reacquire())return false;if(!HismGuard.Reacquire())return false;
            const FObjectPropertyBase* Member = Blueprint->SkeletonGeneratedClass
                ? FindFProperty<FObjectPropertyBase>(Blueprint->SkeletonGeneratedClass, FName(*ComponentName)) : nullptr;
            const bool Matches = Member && Member->GetOwnerClass() == Blueprint->SkeletonGeneratedClass
                && Member->PropertyClass == AddedClass.Get() && Member->HasAnyPropertyFlags(CPF_BlueprintVisible);
            R.Data->SetBoolField(TEXT("skeleton_component_property_verified"), Matches);
            if (bNativeIsmRequested) { R.Data->SetBoolField(TEXT("native_ism_skeleton_property_verified"), Matches); }
            if (bNativeSkeletalRequested) { R.Data->SetBoolField(TEXT("native_skeletal_skeleton_property_verified"), Matches); }
            if (Member) { R.Data->SetStringField(TEXT("skeleton_component_property_path"), Member->GetPathName()); }
            if (!Matches) { return Fail(R, bNativeIsmRequested ? TEXT("component_native_ism_skeleton_mismatch") : bNativeSplineRequested ? TEXT("component_native_spline_skeleton_mismatch") : bNativeSkeletalRequested ? TEXT("component_native_skeletal_skeleton_mismatch") : TEXT("generated_component_skeleton_mismatch"), TEXT("Component was added but its current skeleton object property does not match; inspect partial state before continuing.")); }
        }
        else { FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint.Get());if(!LightGuard.Check(true)||!HismGuard.Check())return false; }
        if (R.bCompileRequested)
        {
            R.Stage = TEXT("compile"); R.bCompileAttempted = true;
            if(!LightGuard.Check()||!HismGuard.Check())return false;auto Compile = Cubeless::UE58::CompileBlueprint(Before.BlueprintPath, false, false);
            R.Data->SetObjectField(TEXT("compile"), Compile);
            bool Compiled = false; Compile->TryGetBoolField(TEXT("compiled"), Compiled); R.Compiled = Compiled;
            if(bNativeLightRequested&&!LightGuard.Reacquire())return false;if(!HismGuard.Reacquire())return false;
            if (!Compiled) { return Fail(R, TEXT("component_compile_failed"), TEXT("Component was changed but full Blueprint compile failed; inspect diagnostics. Not saved.")); }
        }
        return true;
    }();
    if(NiagaraHandled||GeneratedValueHandled||PCGHandled||CACHandled)return R;
    if(HismGuard.Active&&(HismGuard.Stopped||!HismGuard.Check())){R.bSuccess=false;R.bChangeStateKnown=false;return R;}
    if(LightGuard.Active&&!LightGuard.Check()){R.bSuccess=false;R.bChangeStateKnown=false;return R;}
    bool bVerified = false;
    if (!Before.BlueprintPath.IsEmpty())
    {
        FString Code, Error;
        if (ReadLocalComponents(Before.BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, After, Code, Error))
        {
            if(!LightGuard.Check()||!HismGuard.Check()){R.bSuccess=false;R.bChangeStateKnown=false;return R;}
            R.Data->SetObjectField(TEXT("after"), After.Data);
            R.Data->SetBoolField(TEXT("dirty_before"), Before.bDirtyBefore); R.Data->SetBoolField(TEXT("dirty_after"), After.bDirtyAfter);
            R.Data->SetNumberField(TEXT("blueprint_status_before"), Before.StatusBefore); R.Data->SetNumberField(TEXT("blueprint_status_after"), After.StatusAfter);
            R.bChanged = bCreationObserved || Before.bDirtyBefore != After.bDirtyAfter || Before.StatusBefore != After.StatusAfter;
            R.bChangeStateKnown = true; bVerified = bApplied;
            USCS_Node* Node = After.LocalNodes.FindRef(ComponentName).Get();
            UActorComponent* Template = Node ? Node->ComponentTemplate.Get() : nullptr;
            TArray<TSharedPtr<FJsonValue>> Observations;
            for (const auto& P : Properties)
            {
                auto Observation = MakeShared<FJsonObject>();
                Observation->SetStringField(TEXT("property_name"), P->Property->GetName()); Observation->SetField(TEXT("requested"), P->Requested); Observation->SetField(TEXT("before"), P->Before);
                TSharedPtr<FJsonValue> Value; FString Encoding;
                if (Template && Template->IsA(P->Property->GetOwnerClass()) && ReadComponentValue(P->Property, P->Property->ContainerPtrToValuePtr<void>(Template), MaxJsonChars, Value, Encoding, Error))
                {
                    Observation->SetField(TEXT("after"), Value); Observation->SetStringField(TEXT("encoding"), Encoding);
                    const bool bMatches = P->Property->Identical(P->Value, P->Property->ContainerPtrToValuePtr<void>(Template))
                        && (!P->bSocket || Node->AttachToName == CastFieldChecked<FNameProperty>(P->Property)->GetPropertyValue(P->Value));
                    Observation->SetBoolField(TEXT("postcondition_passed"), bMatches); bVerified &= bMatches;
                    R.bChanged |= !FJsonValue::CompareEqual(*P->Before, *Value);
                }
                else { bVerified = false; if (R.bMutationAttempted) { R.bChangeStateKnown = false; } }
                Observations.Add(MakeShared<FJsonValueObject>(Observation));
            }
            R.Data->SetArrayField(TEXT("property_observations"), Observations);
            if (bAdd && bApplied)
            {
                bVerified &= Node && Before.NodeVisits + 1 == After.NodeVisits && Node->ComponentClass == AddedClass.Get();
                for (const auto& Row : After.Data->GetArrayField(TEXT("components")))
                { if (Row->AsObject()->GetStringField(TEXT("component_name")) == ComponentName) { bVerified &= Row->AsObject()->GetStringField(TEXT("parent_component_name")) == ActualParent; } }
                R.Data->SetStringField(TEXT("parent_component"), ActualParent);
                if (Node) { R.Data->SetStringField(TEXT("component_class"), Node->ComponentClass->GetPathName()); R.Data->SetStringField(TEXT("template_path"), Node->ComponentTemplate->GetPathName()); }
                if (bGeneratedRequested)
                {
                    bool ExistingPreserved = true;
                    for (const auto& OldRow : Before.Data->GetArrayField(TEXT("components")))
                    {
                        const FString OldName = OldRow->AsObject()->GetStringField(TEXT("component_name"));
                        const TSharedPtr<FJsonValue>* Current = After.Data->GetArrayField(TEXT("components")).FindByPredicate(
                            [&OldName](const TSharedPtr<FJsonValue>& Row) { return Row->AsObject()->GetStringField(TEXT("component_name")) == OldName; });
                        ExistingPreserved &= Current && FJsonValue::CompareEqual(*OldRow, **Current);
                    }
                    R.Data->SetBoolField(TEXT("existing_scs_observations_preserved"), ExistingPreserved); bVerified &= ExistingPreserved;
                }
            }
        }
        else { R.Data->SetStringField(TEXT("after_observation_error"), Error); if (R.bMutationAttempted) { R.bChangeStateKnown = false; } }
    }
    if(HismGuard.Active&&(HismGuard.Stopped||!HismGuard.Check())){R.bSuccess=false;R.bChangeStateKnown=false;return R;}
    if(LightGuard.Active&&!LightGuard.Check()){R.bSuccess=false;R.bChangeStateKnown=false;return R;}
    if (bCreationIncomplete) { R.bChangeStateKnown = false; }
    if (NativeSplineBefore.IsValid())
    {
        const auto NativeSplineAfter = NativeSplineSourceObservation(USplineComponent::StaticClass());
        const bool Preserved = FJsonValue::CompareEqual(FJsonValueObject(NativeSplineBefore), FJsonValueObject(NativeSplineAfter));
        R.Data->SetObjectField(TEXT("native_spline_source_after"), NativeSplineAfter);
        R.Data->SetBoolField(TEXT("native_spline_source_observed_unchanged"), Preserved);
        R.Data->SetStringField(TEXT("native_spline_source_scope"), TEXT("exact native class/package/existing CDO identities and flags only; no source asset load, CDO creation or runtime spline evaluation"));
        R.Data->SetBoolField(TEXT("native_spline_runtime_evaluation_verified"), false);
        if (!Preserved)
        {
            R.bChanged = true; bVerified = false;
            if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("component_native_spline_source_changed"), TEXT("Native Spline class/package/CDO metadata changed; the actual component/property prefix remains observable and was not rolled back.")); }
        }
        bVerified &= NativeSplineAfter->GetBoolField(TEXT("native_identity_verified"));
    }
    if (NativeIsmBefore.IsValid())
    {
        const auto NativeIsmAfter = NativeIsmSourceObservation(HismGuard.Active?UHierarchicalInstancedStaticMeshComponent::StaticClass():UInstancedStaticMeshComponent::StaticClass());
        const bool Preserved = FJsonValue::CompareEqual(FJsonValueObject(NativeIsmBefore), FJsonValueObject(NativeIsmAfter));
        R.Data->SetObjectField(TEXT("native_ism_source_after"), NativeIsmAfter);
        R.Data->SetBoolField(TEXT("native_ism_source_observed_unchanged"), Preserved);
        R.Data->SetStringField(TEXT("native_ism_source_scope"), TEXT("exact native class/package/existing CDO identities and flags only; no source asset load, CDO creation or runtime instance creation/evaluation"));
        R.Data->SetBoolField(TEXT("native_ism_runtime_instance_operations_verified"), false);
        if (!Preserved)
        {
            R.bChanged = true; bVerified = false;
            if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("component_native_ism_source_changed"), TEXT("Native ISM class/package/CDO metadata changed; the actual component/property prefix remains observable and was not rolled back.")); }
        }
        bVerified &= NativeIsmAfter->GetBoolField(TEXT("native_identity_verified"));
    }
    if (NativeSkeletalBefore.IsValid())
    {
        const auto NativeSkeletalAfter = NativeSkeletalSourceObservation(USkeletalMeshComponent::StaticClass());
        const bool Preserved = FJsonValue::CompareEqual(FJsonValueObject(NativeSkeletalBefore), FJsonValueObject(NativeSkeletalAfter));
        R.Data->SetObjectField(TEXT("native_skeletal_source_after"), NativeSkeletalAfter);
        R.Data->SetBoolField(TEXT("native_skeletal_source_observed_unchanged"), Preserved);
        R.Data->SetStringField(TEXT("native_skeletal_source_scope"), TEXT("exact native class/package/existing normal CDO identities and flags; no source asset load or CDO prewarming requested; template construction/property callbacks/structural skeleton refresh may dispatch implicit Engine work"));
        R.Data->SetBoolField(TEXT("native_skeletal_animation_physics_runtime_verified"), false);
        if (!Preserved)
        {
            R.bChanged = true; bVerified = false;
            if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("component_native_skeletal_source_changed"), TEXT("Native SkeletalMesh class/package/CDO metadata changed; actual component/property prefix retained without rollback.")); }
        }
        bVerified &= NativeSkeletalAfter->GetBoolField(TEXT("native_identity_verified"));
    }
    if (bGeneratedRequested)
    {
        R.bChanged |= bReferenceLoadChanged;
        R.Data->SetStringField(TEXT("generated_component_scope"), TEXT("non-scene local SCS root node; saved defaults retained; source load/PostLoad observation is separate from component authoring"));
        if (bReferenceObserved && ComponentBlueprint.IsValid())
        {
            auto ReferenceAfter = MakeShared<FJsonObject>();
            ReferenceAfter->SetStringField(TEXT("blueprint_path"), ComponentBlueprint->GetPathName());
            ReferenceAfter->SetBoolField(TEXT("dirty"), ComponentBlueprint->GetOutermost()->IsDirty());
            ReferenceAfter->SetNumberField(TEXT("status"), static_cast<int32>(ComponentBlueprint->Status.GetValue()));
            const bool Unchanged = bReferenceDirty == ComponentBlueprint->GetOutermost()->IsDirty() && ReferenceStatus == ComponentBlueprint->Status;
            ReferenceAfter->SetBoolField(TEXT("unchanged_since_load"), Unchanged); R.Data->SetObjectField(TEXT("component_reference_after"), ReferenceAfter);
            R.bChanged |= !Unchanged; bVerified &= Unchanged;
        }
        else if (R.bMutationAttempted) { R.bChangeStateKnown = false; }
        if (bClassLoadFailed) { R.bChangeStateKnown = false; }
    }
    if (bDirtyBaseline)
    {
        const auto DirtyAfter = DirtyPackages(); R.Data->SetArrayField(TEXT("dirty_packages_after"), Strings(DirtyAfter));
        R.bChanged |= DirtyBefore != DirtyAfter;
        for (const FString& Package : DirtyAfter)
        { if (!DirtyBefore.Contains(Package) && (!Blueprint.IsValid() || Package != Blueprint->GetOutermost()->GetName())) { bVerified = false; R.Data->SetBoolField(TEXT("unexpected_external_dirty_package"), true); } }
    }
    if(HismGuard.Active&&!HismGuard.Check())return R;
    R.Data->SetBoolField(TEXT("no_op"), bNoOp); R.Data->SetBoolField(TEXT("creation_observed"), bCreationObserved);
    R.Data->SetBoolField(TEXT("automatic_cleanup_attempted"), false);
    R.Data->SetStringField(TEXT("cleanup_policy"), TEXT("No automatic rollback/removal: newly created partial nodes/templates remain observable for explicit recovery."));
    R.bSuccess = bApplied && bVerified && R.bChangeStateKnown; R.bPostconditionPassed = R.bSuccess;
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
    else if (R.ErrorCode.IsEmpty()) { R.Stage = TEXT("verification"); Fail(R, TEXT("component_postcondition_failed"), TEXT("Independent local readback did not prove the requested postcondition; inspect actual changes before retry.")); }
    return R;
}
}

FMutationResult AddLocalComponent(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName,
    const FString& TransformJson, const FString& ParentComponent, const FString& ComponentPropertiesJson, bool bCompile)
{ return Mutate(BlueprintPath, ComponentType, ComponentName, TransformJson, ParentComponent, ComponentPropertiesJson, true, bCompile, FString()); }

FMutationResult SetLocalComponentProperty(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName, const FString& ValueJson)
{
    // Mesh-only Python compatibility uses this entry. Route only a current loaded populated HISM StaticMesh cell.
    if(IsEditorGameThread()&&!IsPlaySessionActive()&&PropertyName.Equals(TEXT("StaticMesh"),ESearchCase::IgnoreCase))
    {
        FString P=BlueprintPath;if(P.StartsWith(TEXT("/Game/"))&&!P.Contains(TEXT(".")))P+=TEXT(".")+FPackageName::GetShortName(P);
        UBlueprint* B=FindObject<UBlueprint>(nullptr,*P);const TStrongObjectPtr<UBlueprint> HeldBP(B);
        if(FHismScope::Loaded(B)&&B->SimpleConstructionScript.IsResolved())
        {
            USimpleConstructionScript* S=B->SimpleConstructionScript.Get();const TStrongObjectPtr<USimpleConstructionScript> HeldSCS(S);
            if(FHismScope::Loaded(S)&&S->GetAllNodes().Num()<=MaxNodes)for(USCS_Node* N:S->GetAllNodes())
            {
                const TStrongObjectPtr<USCS_Node> HeldNode(N);if(!FHismScope::Loaded(N)||N->GetVariableName()!=FName(*ComponentName)||!N->ComponentTemplate.IsResolved())continue;
                UActorComponent* C=N->ComponentTemplate.Get();const TStrongObjectPtr<UActorComponent> HeldComponent(C);auto* H=Cast<UHierarchicalInstancedStaticMeshComponent>(C);
                if(FHismScope::Loaded(H)&&H->GetClass()==UHierarchicalInstancedStaticMeshComponent::StaticClass()&&H->GetInstanceCount()>0)
                {
                    FMutationResult Parsed;TSharedPtr<FJsonValue> V;if(!ParseValue(ValueJson,V,Parsed))return Parsed;
                    if(V->Type==EJson::String&&!V->AsString().IsEmpty()) { auto R=SetLocalStaticMeshProperties(BlueprintPath,ComponentName,V->AsString(),FString());R.Data->SetBoolField(TEXT("hism_mesh_only_entry_adapter"),true);R.Data->SetStringField(TEXT("property_name"),PropertyName);return R; }
                    break;
                }
            }
        }
    }
    return Mutate(BlueprintPath, FString(), ComponentName, FString(), FString(), ValueJson, false, false, PropertyName);
}

namespace
{
struct FPhysicsDefaults
{
    bool bSimulate = false, bGravity = false, bOverrideMass = false;
    float Mass = 0.f, Linear = 0.f, Angular = 0.f, Blend = 0.f;
    bool Read(UPrimitiveComponent* Component)
    {
        if (!IsValid(Component)) { return false; }
        const FBodyInstance* Body = Component->GetBodyInstance(NAME_None, false);
        if (!Body) { return false; }
        bSimulate = Body->bSimulatePhysics; bGravity = Body->bEnableGravity; bOverrideMass = Body->bOverrideMass;
        Mass = Body->GetMassOverride(); Linear = Body->LinearDamping; Angular = Body->AngularDamping; Blend = Body->PhysicsBlendWeight;
        return FMath::IsFinite(Mass) && FMath::IsFinite(Linear) && FMath::IsFinite(Angular) && FMath::IsFinite(Blend);
    }
    bool Equals(const FPhysicsDefaults& Other) const
    { return bSimulate == Other.bSimulate && bGravity == Other.bGravity && bOverrideMass == Other.bOverrideMass && Mass == Other.Mass && Linear == Other.Linear && Angular == Other.Angular && Blend == Other.Blend; }
    TSharedRef<FJsonObject> Json() const
    {
        auto Out = MakeShared<FJsonObject>();
        Out->SetBoolField(TEXT("simulate_physics"), bSimulate); Out->SetBoolField(TEXT("gravity_enabled"), bGravity);
        Out->SetBoolField(TEXT("mass_override_enabled"), bOverrideMass); Out->SetNumberField(TEXT("mass"), Mass);
        Out->SetNumberField(TEXT("linear_damping"), Linear); Out->SetNumberField(TEXT("angular_damping"), Angular);
        Out->SetNumberField(TEXT("physics_blend_weight"), Blend); return Out;
    }
};
bool IsPhysicsTemplate(UPrimitiveComponent* Component)
{
    if (!IsValid(Component)) { return false; }
    UClass* Class = Component->GetClass();
    return (Class == UStaticMeshComponent::StaticClass() || Class == USphereComponent::StaticClass() || Class == UBoxComponent::StaticClass() || Class == UCapsuleComponent::StaticClass())
        && !Component->IsRegistered() && !Component->IsPhysicsStateCreated() && !Component->IsWelded()
        && Component->GetBodyInstance(NAME_None, false) == Component->GetBodyInstance();
}
struct FPhysicsTarget
{
    FString Path;
    FPhysicsDefaults Before, Expected;
    bool bSimulate = false, bGravity = false, bMass = false, bLinear = false, bAngular = false;
};
TSharedRef<FJsonObject> Attachment(UPrimitiveComponent* Component, USCS_Node* Node)
{
    auto Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("template_attach_parent"), Component->GetAttachParent() ? Component->GetAttachParent()->GetPathName() : FString());
    Out->SetStringField(TEXT("template_attach_socket"), Component->GetAttachSocketName().ToString());
    Out->SetStringField(TEXT("node_attach_socket"), Node->AttachToName.ToString()); return Out;
}
}

FMutationResult SetLocalPhysicsProperties(const FString& BlueprintPath, const FString& ComponentName,
    bool bSimulatePhysics, bool bGravityEnabled, double Mass, double LinearDamping, double AngularDamping)
{
    FMutationResult R;
    R.Data->SetStringField(TEXT("blueprint_name"), BlueprintPath); R.Data->SetStringField(TEXT("component"), ComponentName);
    R.Data->SetStringField(TEXT("component_name"), ComponentName);
    R.Data->SetStringField(TEXT("scope"), TEXT("local SCS StaticMesh/Sphere/Box/Capsule stored body defaults; no active/welded physics, inherited template edits or external instances"));
    R.Data->SetBoolField(TEXT("legacy_gravity_ignored_fixed"), true); R.Data->SetBoolField(TEXT("runtime_simulation_verified"), false);
    R.Data->SetBoolField(TEXT("implicit_postload_work_possible"), true);
    FReadResult Before, After; FPhysicsDefaults Initial, Requested;
    TStrongObjectPtr<UBlueprint> Blueprint; TArray<FPhysicsTarget> Targets; TArray<FString> DirtyBefore;
    TSharedPtr<FJsonObject> AttachmentBefore;
    bool bDirtyBaseline = false, bBodyObserved = false, bNoOp = false;
    const bool bApplied = [&]() -> bool
    {
        if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("component_editor_state_invalid"), TEXT("Requires editor game thread outside PIE/SIE.")); }
        if (!FMath::IsFinite(Mass) || Mass < 0.001 || Mass > 1.e12 || !FMath::IsFinite(LinearDamping) || LinearDamping < 0.0 || LinearDamping > 1.e12 || !FMath::IsFinite(AngularDamping) || AngularDamping < 0.0 || AngularDamping > 1.e12)
        { return Fail(R, TEXT("component_physics_value_invalid"), TEXT("Mass must be finite in [0.001, 1e12] kg; damping must be finite in [0, 1e12].")); }
        if (!SafeName(BlueprintPath) || !BlueprintPath.StartsWith(TEXT("/Game/")) || !SafeName(ComponentName))
        { return Fail(R, TEXT("component_selector_invalid"), TEXT("Use a /Game Blueprint and bounded exact local component name.")); }
        DirtyBefore = DirtyPackages(); bDirtyBaseline = true; R.Data->SetArrayField(TEXT("dirty_packages_before_load"), Strings(DirtyBefore));
        FString Code, Error;
        if (!ReadLocalComponents(BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, Before, Code, Error))
        { R.bChangeStateKnown = !Before.bLoadedDuringRead; return Fail(R, *Code, Error); }
        R.Data->SetObjectField(TEXT("before"), Before.Data); R.Data->SetBoolField(TEXT("loaded_during_preflight"), Before.bLoadedDuringRead);
        Blueprint.Reset(FindObject<UBlueprint>(nullptr, *Before.BlueprintPath));
        if (!Blueprint.IsValid() || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(AActor::StaticClass()) || !Blueprint->SimpleConstructionScript || !Blueprint->GeneratedClass || Blueprint->BlueprintType != BPTYPE_Normal
            || Blueprint->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
        { return Fail(R, TEXT("component_blueprint_unsupported"), TEXT("Requires a normal Actor Blueprint with valid generated class and local SCS.")); }
        if (!NativeRuntimeParents(Blueprint.Get(), R)) { return false; }
        USCS_Node* Node = Before.LocalNodes.FindRef(ComponentName).Get();
        UPrimitiveComponent* Template = Node ? Cast<UPrimitiveComponent>(Node->ComponentTemplate) : nullptr;
        if (!Node || !Template) { return Fail(R, TEXT("component_not_found"), TEXT("No exact local reachable primitive component exists.")); }
        if (!IsPhysicsTemplate(Template)) { return Fail(R, TEXT("component_physics_template_unsupported"), TEXT("Requires exact native StaticMesh/Sphere/Box/Capsule, unregistered without a live physics state or welded body.")); }
        if (!Initial.Read(Template)) { return Fail(R, TEXT("component_physics_before_unreadable"), TEXT("Stored body defaults are unavailable or non-finite.")); }
        bBodyObserved = true; R.Data->SetObjectField(TEXT("physics_before"), Initial.Json());
        AttachmentBefore = Attachment(Template, Node); R.Data->SetObjectField(TEXT("attachment_before"), AttachmentBefore.ToSharedRef());
        Requested = {bSimulatePhysics, bGravityEnabled, true, static_cast<float>(Mass), static_cast<float>(LinearDamping), static_cast<float>(AngularDamping), bSimulatePhysics ? 1.f : 0.f};
        R.Data->SetObjectField(TEXT("physics_requested_stored"), Requested.Json());
        bNoOp = Initial.Equals(Requested);
        if (bNoOp) { return true; }
        if (!RejectExternalInstances(Blueprint.Get(), R)) { return false; }
        TArray<UObject*> Instances; Template->GetArchetypeInstances(Instances); Instances.Insert(Template, 0);
        TArray<FString> Paths;
        for (UObject* Object : Instances)
        {
            UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);
            if (!IsPhysicsTemplate(Component) || Component->GetClass() != Template->GetClass() || Component->GetOutermost() != Blueprint->GetOutermost())
            { return Fail(R, TEXT("component_physics_archetype_unsupported"), TEXT("All affected archetypes must be same-package exact native templates without live or welded physics; external instances are unsupported.")); }
            if (Paths.Contains(Component->GetPathName())) { continue; }
            FPhysicsTarget Target; Target.Path = Component->GetPathName();
            if (!Target.Before.Read(Component)) { return Fail(R, TEXT("component_physics_before_unreadable"), TEXT("An archetype has unavailable or non-finite body defaults.")); }
            Target.Expected = Target.Before;
            // Preserve local overrides, treating each coupled public setter as
            // one unit (simulation+blend, override flag+mass).
            Target.bSimulate = Component == Template || (Target.Before.bSimulate == Initial.bSimulate && Target.Before.Blend == Initial.Blend);
            Target.bGravity = Component == Template || Target.Before.bGravity == Initial.bGravity;
            Target.bMass = Component == Template || (Target.Before.bOverrideMass == Initial.bOverrideMass && Target.Before.Mass == Initial.Mass);
            Target.bLinear = Component == Template || Target.Before.Linear == Initial.Linear;
            Target.bAngular = Component == Template || Target.Before.Angular == Initial.Angular;
            // Equal leaf values do not imply inheritance: an intermediate
            // archetype can override a value and a descendant can override it
            // back. Match the complete chain, as PropertyAccessUtil does.
            TSet<const UObject*> Seen;
            UObject* Ancestor = Component;
            while (Ancestor != Template)
            {
                if (!IsValid(Ancestor) || Seen.Contains(Ancestor) || Seen.Num() >= 128)
                { return Fail(R, TEXT("component_physics_archetype_unsupported"), TEXT("Physics inheritance chain is invalid, cyclic or exceeds 128 components.")); }
                Seen.Add(Ancestor); Ancestor = Ancestor->GetArchetype();
                if (Ancestor == Template) { break; }
                UPrimitiveComponent* Parent = Cast<UPrimitiveComponent>(Ancestor); FPhysicsDefaults ParentDefaults;
                if (!IsPhysicsTemplate(Parent) || Parent->GetClass() != Template->GetClass() || Parent->GetOutermost() != Blueprint->GetOutermost() || !ParentDefaults.Read(Parent))
                { return Fail(R, TEXT("component_physics_archetype_unsupported"), TEXT("Physics inheritance chain does not reach the selected template through supported same-package archetypes.")); }
                Target.bSimulate &= ParentDefaults.bSimulate == Initial.bSimulate && ParentDefaults.Blend == Initial.Blend;
                Target.bGravity &= ParentDefaults.bGravity == Initial.bGravity;
                Target.bMass &= ParentDefaults.bOverrideMass == Initial.bOverrideMass && ParentDefaults.Mass == Initial.Mass;
                Target.bLinear &= ParentDefaults.Linear == Initial.Linear;
                Target.bAngular &= ParentDefaults.Angular == Initial.Angular;
            }
            if (Target.bSimulate) { Target.Expected.bSimulate = Requested.bSimulate; Target.Expected.Blend = Requested.Blend; }
            if (Target.bGravity) { Target.Expected.bGravity = Requested.bGravity; }
            if (Target.bMass) { Target.Expected.bOverrideMass = true; Target.Expected.Mass = Requested.Mass; }
            if (Target.bLinear) { Target.Expected.Linear = Requested.Linear; }
            if (Target.bAngular) { Target.Expected.Angular = Requested.Angular; }
            Paths.Add(Target.Path); Targets.Add(MoveTemp(Target));
        }
        R.Data->SetArrayField(TEXT("local_physics_archetypes"), Strings(Paths));
        FProperty* BodyProperty = FindProperty(Template->GetClass(), TEXT("BodyInstance"));
        if (!BodyProperty || !Template->CanEditChange(BodyProperty)) { return Fail(R, TEXT("component_physics_property_unavailable"), TEXT("The public editable BodyInstance property is unavailable.")); }
        auto Notify = PropertyAccessUtil::BuildBasicChangeNotify(BodyProperty, Template, EPropertyAccessChangeNotifyMode::Default, EPropertyChangeType::ValueSet);
        R.Stage = TEXT("mutation"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
        Blueprint->Modify(); Blueprint->SimpleConstructionScript->Modify(); Node->Modify();
        PropertyAccessUtil::EmitPreChangeNotify(Notify.Get(), false);
        bool bAllApplied = true;
        for (const FPhysicsTarget& Target : Targets)
        {
            UPrimitiveComponent* Component = FindObject<UPrimitiveComponent>(nullptr, *Target.Path);
            if (!IsPhysicsTemplate(Component)) { bAllApplied = false; break; }
            Component->Modify();
            if (Target.bSimulate) { Component->SetSimulatePhysics(Requested.bSimulate); }
            if (Target.bGravity) { Component->SetEnableGravity(Requested.bGravity); }
            if (Target.bMass) { Component->SetMassOverrideInKg(NAME_None, Requested.Mass, true); }
            if (Target.bLinear) { Component->SetLinearDamping(Requested.Linear); }
            if (Target.bAngular) { Component->SetAngularDamping(Requested.Angular); }
        }
        PropertyAccessUtil::EmitPostChangeNotify(Notify.Get(), false);
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint.Get());
        return bAllApplied || Fail(R, TEXT("component_physics_apply_failed"), TEXT("A template changed eligibility after mutation began; inspect observed partial state, do not retry automatically."));
    }();
    bool bVerified = bApplied;
    if (!Before.BlueprintPath.IsEmpty())
    {
        FString Code, Error;
        if (ReadLocalComponents(Before.BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, After, Code, Error))
        {
            R.Data->SetObjectField(TEXT("after"), After.Data); R.Data->SetBoolField(TEXT("dirty_before"), Before.bDirtyBefore); R.Data->SetBoolField(TEXT("dirty_after"), After.bDirtyAfter);
            R.Data->SetNumberField(TEXT("blueprint_status_before"), Before.StatusBefore); R.Data->SetNumberField(TEXT("blueprint_status_after"), After.StatusAfter);
            R.bChanged = Before.bDirtyBefore != After.bDirtyAfter || Before.StatusBefore != After.StatusAfter;
            R.bChangeStateKnown = true;
            const bool bSCSUnchanged = Before.NodeVisits == After.NodeVisits && FJsonValue::CompareEqual(*MakeShared<FJsonValueArray>(Before.Data->GetArrayField(TEXT("components"))), *MakeShared<FJsonValueArray>(After.Data->GetArrayField(TEXT("components"))));
            R.Data->SetBoolField(TEXT("scs_unchanged"), bSCSUnchanged); bVerified &= bSCSUnchanged; R.bChanged |= !bSCSUnchanged;
            if (bBodyObserved)
            {
                USCS_Node* Node = After.LocalNodes.FindRef(ComponentName).Get();
                UPrimitiveComponent* Template = Node ? Cast<UPrimitiveComponent>(Node->ComponentTemplate) : nullptr; FPhysicsDefaults Actual;
                if (Template && Actual.Read(Template))
                {
                    R.Data->SetObjectField(TEXT("physics_after"), Actual.Json()); R.bChanged |= !Initial.Equals(Actual); bVerified &= Actual.Equals(Requested) && IsPhysicsTemplate(Template);
                    const auto AttachmentAfter = Attachment(Template, Node); R.Data->SetObjectField(TEXT("attachment_after"), AttachmentAfter);
                    const bool bSameAttachment = FJsonValue::CompareEqual(*MakeShared<FJsonValueObject>(AttachmentBefore), *MakeShared<FJsonValueObject>(AttachmentAfter));
                    R.Data->SetBoolField(TEXT("attachment_unchanged"), bSameAttachment); bVerified &= bSameAttachment; R.bChanged |= !bSameAttachment;
                }
                else { bVerified = false; R.bChangeStateKnown = false; }
            }
        }
        else { R.Data->SetStringField(TEXT("after_observation_error"), Error); bVerified = false; if (R.bMutationAttempted || Before.bLoadedDuringRead) { R.bChangeStateKnown = false; } }
    }
    TArray<TSharedPtr<FJsonValue>> Observations;
    for (const FPhysicsTarget& Target : Targets)
    {
        auto Observation = MakeShared<FJsonObject>(); Observation->SetStringField(TEXT("path"), Target.Path);
        Observation->SetObjectField(TEXT("before"), Target.Before.Json()); Observation->SetObjectField(TEXT("expected"), Target.Expected.Json());
        UPrimitiveComponent* Component = FindObject<UPrimitiveComponent>(nullptr, *Target.Path); FPhysicsDefaults Actual;
        if (Actual.Read(Component))
        {
            Observation->SetObjectField(TEXT("after"), Actual.Json()); const bool bMatches = Actual.Equals(Target.Expected) && IsPhysicsTemplate(Component);
            Observation->SetBoolField(TEXT("postcondition_passed"), bMatches); bVerified &= bMatches; R.bChanged |= !Actual.Equals(Target.Before);
        }
        else { bVerified = false; R.bChangeStateKnown = false; Observation->SetBoolField(TEXT("postcondition_passed"), false); }
        Observations.Add(MakeShared<FJsonValueObject>(Observation));
    }
    R.Data->SetArrayField(TEXT("physics_target_observations"), Observations);
    if (bDirtyBaseline)
    {
        const auto DirtyAfter = DirtyPackages(); R.Data->SetArrayField(TEXT("dirty_packages_after"), Strings(DirtyAfter)); R.bChanged |= DirtyBefore != DirtyAfter;
        for (const FString& Package : DirtyAfter)
        { if (!DirtyBefore.Contains(Package) && (!Blueprint.IsValid() || Package != Blueprint->GetOutermost()->GetName())) { bVerified = false; R.Data->SetBoolField(TEXT("unexpected_external_dirty_package"), true); } }
    }
    R.Data->SetBoolField(TEXT("no_op"), bNoOp);
    R.bSuccess = bApplied && bVerified && R.bChangeStateKnown; R.bPostconditionPassed = R.bSuccess;
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
    else if (R.ErrorCode.IsEmpty()) { R.Stage = TEXT("verification"); Fail(R, TEXT("component_physics_postcondition_failed"), TEXT("Stored body defaults, local archetypes or attachment identity did not match the requested postcondition; inspect actual changes.")); }
    return R;
}

namespace
{
struct FMeshMaterialState
{
    FString Mesh, EditorEffective, Effective;
    TArray<FString> Overrides, OverlaySlots;
    bool Read(UStaticMeshComponent* Component,TFunction<bool()> Fence={})
    {
        if(Fence)
        {
            if(!Fence()||!IsValid(Component))return false;
            const auto MeshSlot=Component->GetStaticMesh();if(!MeshSlot.IsResolved())return false;
            UStaticMesh* M=MeshSlot.Get();const TStrongObjectPtr<UStaticMesh> HM(M);if(!Fence())return false;Mesh=GetPathNameSafe(M);if(!M)Mesh.Reset();
            const auto MaterialSlots=Component->OverrideMaterials;const auto Overlay=Component->GetComponentMaterialSlotsOverlayMaterial();
            if(MaterialSlots.Num()>1024||Overlay.Num()>1024)return false;TArray<TStrongObjectPtr<UMaterialInterface>> Held;
            for(const auto& Slot:MaterialSlots){if(!Slot.IsResolved())return false;Held.Emplace(Slot.Get());}for(const auto& Slot:Overlay){if(!Slot.IsResolved())return false;Held.Emplace(Slot.Get());}
            if(!Fence())return false;Overrides.Reset();OverlaySlots.Reset();
            for(const auto& Slot:MaterialSlots){auto* P=Slot.Get();if(P&&!IsValid(P))return false;Overrides.Add(P?P->GetPathName():FString());}
            for(const auto& Slot:Overlay){auto* P=Slot.Get();if(P&&!IsValid(P))return false;OverlaySlots.Add(P?P->GetPathName():FString());}
            UMaterialInterface* E=Component->GetEditorMaterial(0);const TStrongObjectPtr<UMaterialInterface> HE(E);if(!Fence())return false;EditorEffective=E?E->GetPathName():FString();
            UMaterialInterface* A=Component->GetMaterial(0);const TStrongObjectPtr<UMaterialInterface> HA(A);if(!Fence())return false;Effective=A?A->GetPathName():FString();return Fence();
        }
        if (!IsValid(Component) || Component->OverrideMaterials.Num() > 1024 || Component->GetComponentMaterialSlotsOverlayMaterial().Num() > 1024) { return false; }
        Mesh = Component->GetStaticMesh() ? Component->GetStaticMesh()->GetPathName() : FString();
        Overrides.Reset();
        // Public read only: MeshComponent.h forbids direct writes to this array.
        for (UMaterialInterface* Material : Component->OverrideMaterials)
        { if (Material && !IsValid(Material)) { return false; } Overrides.Add(Material ? Material->GetPathName() : FString()); }
        OverlaySlots.Reset();
        for (UMaterialInterface* Material : Component->GetComponentMaterialSlotsOverlayMaterial())
        { if (Material && !IsValid(Material)) { return false; } OverlaySlots.Add(Material ? Material->GetPathName() : FString()); }
        UMaterialInterface* EditorMaterial = Component->GetEditorMaterial(0);
        UMaterialInterface* RenderMaterial = Component->GetMaterial(0);
        EditorEffective = EditorMaterial ? EditorMaterial->GetPathName() : FString();
        Effective = RenderMaterial ? RenderMaterial->GetPathName() : FString(); return true;
    }
    bool SameStored(const FMeshMaterialState& Other) const { return Mesh == Other.Mesh && Overrides == Other.Overrides && OverlaySlots == Other.OverlaySlots; }
    bool SameSlot0(const FMeshMaterialState& Other) const
    { return Overrides.IsEmpty() ? Other.Overrides.IsEmpty() : !Other.Overrides.IsEmpty() && Overrides[0] == Other.Overrides[0]; }
    TSharedRef<FJsonObject> Json(bool bIncludeEffective = true) const
    {
        auto Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("static_mesh"), Mesh);
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const FString& Path : Overrides) { if (Path.IsEmpty()) { Values.Add(MakeShared<FJsonValueNull>()); } else { Values.Add(MakeShared<FJsonValueString>(Path)); } }
        Out->SetArrayField(TEXT("override_materials"), Values); Out->SetNumberField(TEXT("override_count"), Overrides.Num());
        Values.Reset();
        for (const FString& Path : OverlaySlots) { if (Path.IsEmpty()) { Values.Add(MakeShared<FJsonValueNull>()); } else { Values.Add(MakeShared<FJsonValueString>(Path)); } }
        Out->SetArrayField(TEXT("overlay_material_slots"), Values); Out->SetNumberField(TEXT("overlay_slot_count"), OverlaySlots.Num());
        Out->SetBoolField(TEXT("slot0_override_present"), !Overrides.IsEmpty());
        if (Overrides.IsEmpty() || Overrides[0].IsEmpty()) { Out->SetField(TEXT("slot0_override"), MakeShared<FJsonValueNull>()); } else { Out->SetStringField(TEXT("slot0_override"), Overrides[0]); }
        if (bIncludeEffective) { Out->SetStringField(TEXT("slot0_editor_effective"), EditorEffective); Out->SetStringField(TEXT("slot0_effective"), Effective); } return Out;
    }
};
bool IsMeshMaterialTemplate(UStaticMeshComponent* Component,bool AllowPopulated=false)
{ if(IsValid(Component)&&Component->GetClass()==UHierarchicalInstancedStaticMeshComponent::StaticClass())return (FHismScope::Empty(Component)||(AllowPopulated&&FHismScope::PopulatedEligible(Component)))&&!Component->IsWelded();return IsValid(Component) && Component->GetClass() == UStaticMeshComponent::StaticClass() && !Component->IsRegistered() && !Component->IsPhysicsStateCreated() && !Component->IsWelded() && !Component->HasBegunPlay(); }
struct FMeshMaterialTarget
{
    FString Path;
    FMeshMaterialState Before, Expected;
    bool bMesh = false, bMaterial = false;
};
bool SCSMatchesExceptSelectedMesh(const FReadResult& Before, const FReadResult& After, const FString& Selected)
{
    const auto& A = Before.Data->GetArrayField(TEXT("components")); const auto& B = After.Data->GetArrayField(TEXT("components"));
    if (A.Num() != B.Num()) { return false; }
    for (int32 I = 0; I < A.Num(); ++I)
    {
        const auto& First = A[I]->AsObject(); const auto& Second = B[I]->AsObject();
        if (First->Values.Num() != Second->Values.Num()) { return false; }
        for (const auto& Pair : First->Values)
        {
            const FString Key(Pair.Key.ToView());
            if (Key == TEXT("static_mesh") && First->GetStringField(TEXT("component_name")) == Selected) { continue; }
            const TSharedPtr<FJsonValue> Value = Second->TryGetField(Key);
            if (!Value || !FJsonValue::CompareEqual(*Pair.Value, *Value)) { return false; }
        }
    }
    return true;
}
}

FMutationResult SetLocalStaticMeshProperties(const FString& BlueprintPath, const FString& ComponentName,
    const FString& StaticMeshPath, const FString& MaterialPath)
{
    FMutationResult R;FHismScope HismGuard(R,true); R.Data->SetStringField(TEXT("blueprint_name"), BlueprintPath); R.Data->SetStringField(TEXT("component_name"), ComponentName); R.Data->SetStringField(TEXT("component"), ComponentName);
    R.Data->SetStringField(TEXT("scope"), TEXT("local exact StaticMeshComponent or bounded empty or populated own native HISM archetype; optional mesh and slot-0 material only; empty native path leaves that binding unchanged"));
    R.Data->SetBoolField(TEXT("hidden_material_branch_supported"), true); R.Data->SetBoolField(TEXT("full_legacy_command_parity"), false);
    R.Data->SetBoolField(TEXT("mesh_set_attempted"), false); R.Data->SetBoolField(TEXT("material_set_attempted"), false);
    R.Data->SetNumberField(TEXT("material_slot"), 0); R.Data->SetBoolField(TEXT("shader_completion_verified"), false); R.Data->SetBoolField(TEXT("pso_completion_verified"), false);
    R.Data->SetBoolField(TEXT("implicit_postload_work_possible"), true); R.Data->SetBoolField(TEXT("automatic_cleanup_attempted"), false);
    FReadResult Before, After; FMeshMaterialState Initial, Expected;
    TStrongObjectPtr<UBlueprint> Blueprint; FPreparedProperty MeshAsset, MaterialAsset;
    FProperty* MeshProperty = nullptr; FProperty* MaterialsProperty = nullptr;
    TArray<FMeshMaterialTarget> Targets; TArray<FString> DirtyBefore; TSharedPtr<FJsonObject> AttachmentBefore;
    bool bDirtyBaseline = false, bObserved = false, bNoOp = false;
    const bool bApplied = [&]() -> bool
    {
        if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("component_editor_state_invalid"), TEXT("Requires editor game thread outside PIE/SIE.")); }
        if (!SafeName(BlueprintPath) || !BlueprintPath.StartsWith(TEXT("/Game/")) || !SafeName(ComponentName) || (StaticMeshPath.IsEmpty() && MaterialPath.IsEmpty()))
        { return Fail(R, TEXT("component_selector_invalid"), TEXT("Use a /Game Blueprint, exact local component name and at least one mesh/material asset path.")); }
        DirtyBefore = DirtyPackages(); bDirtyBaseline = true; R.Data->SetArrayField(TEXT("dirty_packages_before_load"), Strings(DirtyBefore));
        FString Code, Error;
        if (!ReadLocalComponents(BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, Before, Code, Error))
        { R.bChangeStateKnown = !Before.bLoadedDuringRead; return Fail(R, *Code, Error); }
        R.Data->SetObjectField(TEXT("before"), Before.Data); R.Data->SetBoolField(TEXT("loaded_during_preflight"), Before.bLoadedDuringRead);
        Blueprint.Reset(FindObject<UBlueprint>(nullptr, *Before.BlueprintPath));
        if (!Blueprint.IsValid() || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(AActor::StaticClass()) || !Blueprint->SimpleConstructionScript || !Blueprint->GeneratedClass || Blueprint->BlueprintType != BPTYPE_Normal
            || Blueprint->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
        { return Fail(R, TEXT("component_blueprint_unsupported"), TEXT("Requires a normal Actor Blueprint with valid generated class and local SCS.")); }
        if (!NativeRuntimeParents(Blueprint.Get(), R)) { return false; }
        USCS_Node* Node = Before.LocalNodes.FindRef(ComponentName).Get(); UStaticMeshComponent* Template = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr;
        if (!IsMeshMaterialTemplate(Template,true)) { return Fail(R, TEXT("component_mesh_template_unsupported"), TEXT("Requires an exact local StaticMeshComponent template, unregistered with no live/welded physics.")); }
        if(Template->GetClass()==UHierarchicalInstancedStaticMeshComponent::StaticClass()&&!HismGuard.Bind(Blueprint.Get(),ComponentName,Node))return false;
        MeshProperty = FindProperty(Template->GetClass(), TEXT("StaticMesh")); MaterialsProperty = FindProperty(Template->GetClass(), TEXT("OverrideMaterials"));
        if(HismGuard.Active)
        {
            if(!StaticMeshPath.IsEmpty())
            {
                if(!MeshProperty)return Fail(R,TEXT("component_mesh_property_unavailable"),TEXT("Requested mesh/material property is unavailable or not editable on this template."));
                const bool Editable=Template->CanEditChange(MeshProperty);if(!HismGuard.Check())return false;if(!Editable)return Fail(R,TEXT("component_mesh_property_unavailable"),TEXT("Requested mesh/material property is unavailable or not editable on this template."));
            }
            if(!MaterialPath.IsEmpty())
            {
                if(!MaterialsProperty)return Fail(R,TEXT("component_mesh_property_unavailable"),TEXT("Requested mesh/material property is unavailable or not editable on this template."));
                const bool Editable=Template->CanEditChange(MaterialsProperty);if(!HismGuard.Check())return false;if(!Editable)return Fail(R,TEXT("component_mesh_property_unavailable"),TEXT("Requested mesh/material property is unavailable or not editable on this template."));
            }
        }
        else if ((!StaticMeshPath.IsEmpty() && (!MeshProperty || !Template->CanEditChange(MeshProperty))) || (!MaterialPath.IsEmpty() && (!MaterialsProperty || !Template->CanEditChange(MaterialsProperty))))
        { return Fail(R, TEXT("component_mesh_property_unavailable"), TEXT("Requested mesh/material property is unavailable or not editable on this template.")); }
        if(!HismGuard.Check())return false;
        if (!Initial.Read(Template,HismGuard.Populated?TFunction<bool()>([&]{return HismGuard.Check();}):TFunction<bool()>())) { return Fail(R, TEXT("component_mesh_before_unreadable"), TEXT("Material overrides are invalid or exceed 1024 entries.")); }
        bObserved = true; Expected = Initial; R.Data->SetObjectField(TEXT("static_mesh_material_before"), Initial.Json());
        AttachmentBefore = Attachment(Template, Node); R.Data->SetObjectField(TEXT("attachment_before"), AttachmentBefore.ToSharedRef());
        // Complete both asset loads and recursive dependency checks before any
        // component write. A missing material must not leave a new mesh behind.
        if (!StaticMeshPath.IsEmpty() && !LoadStaticMesh(MakeShared<FJsonValueString>(StaticMeshPath), MeshAsset, R)) { return false; }
        if(HismGuard.Active&&!HismGuard.Check())return false;
        if (!MaterialPath.IsEmpty() && !LoadSlotMaterial(MaterialPath, MaterialAsset, R)) { return false; }
        if(!HismGuard.Check())return false;
        if (MeshAsset.Reference.IsValid()) { Expected.Mesh = MeshAsset.Reference->GetPathName(); }
        if (MaterialAsset.Reference.IsValid()) { if (Expected.Overrides.IsEmpty()) { Expected.Overrides.Add(FString()); } Expected.Overrides[0] = MaterialAsset.Reference->GetPathName(); }
        if(!HismGuard.Check())return false;
        bNoOp = Initial.SameStored(Expected); if (bNoOp) { return true; }
        if (!RejectExternalInstances(Blueprint.Get(), R)) { return false; }
        TArray<UObject*> Instances; Template->GetArchetypeInstances(Instances);if(HismGuard.Populated){for(UObject* Returned:Instances)if(Returned)HismGuard.Holds.Emplace(Returned);}if(!HismGuard.Check())return false;if(HismGuard.Active&&!Instances.IsEmpty())return Fail(R,TEXT("component_hism_external_archetypes_pending"),TEXT("HISM mesh/material authoring currently requires no additional archetype instances; no propagation was dispatched.")); Instances.Insert(Template, 0); TSet<FString> Paths;
        for (UObject* Object : Instances)
        {
            UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Object);
            if (!IsMeshMaterialTemplate(Component,HismGuard.Populated) || Component->GetOutermost() != Blueprint->GetOutermost()) { return Fail(R, TEXT("component_mesh_archetype_unsupported"), TEXT("All affected archetypes must be same-package native StaticMeshComponent templates without live physics.")); }
            if (Paths.Contains(Component->GetPathName())) { continue; }
            FMeshMaterialTarget Target; Target.Path = Component->GetPathName();
            if (!Target.Before.Read(Component,HismGuard.Populated?TFunction<bool()>([&]{return HismGuard.Check();}):TFunction<bool()>())) { return Fail(R, TEXT("component_mesh_before_unreadable"), TEXT("An archetype override array is invalid or exceeds its observation budget.")); }
            Target.Expected = Target.Before; Target.bMesh = MeshAsset.Reference.IsValid() && Target.Before.Mesh == Initial.Mesh; Target.bMaterial = MaterialAsset.Reference.IsValid() && Target.Before.SameSlot0(Initial);
            TSet<const UObject*> Seen; UObject* Ancestor = Component;
            while (Ancestor != Template)
            {
                if (!IsValid(Ancestor) || Seen.Contains(Ancestor) || Seen.Num() >= 128) { return Fail(R, TEXT("component_mesh_archetype_unsupported"), TEXT("Mesh/material inheritance chain is invalid, cyclic or exceeds 128 components.")); }
                Seen.Add(Ancestor); Ancestor = Ancestor->GetArchetype(); if (Ancestor == Template) { break; }
                auto* Parent = Cast<UStaticMeshComponent>(Ancestor); FMeshMaterialState State;
                if (!IsMeshMaterialTemplate(Parent,HismGuard.Populated) || Parent->GetOutermost() != Blueprint->GetOutermost() || !State.Read(Parent,HismGuard.Populated?TFunction<bool()>([&]{return HismGuard.Check();}):TFunction<bool()>())) { return Fail(R, TEXT("component_mesh_archetype_unsupported"), TEXT("Mesh/material chain must reach the selected same-package template.")); }
                Target.bMesh &= State.Mesh == Initial.Mesh; Target.bMaterial &= State.SameSlot0(Initial);
            }
            if (Target.bMesh) { Target.Expected.Mesh = Expected.Mesh; }
            if (Target.bMaterial) { if (Target.Expected.Overrides.IsEmpty()) { Target.Expected.Overrides.Add(FString()); } Target.Expected.Overrides[0] = Expected.Overrides[0]; }
            UStaticMesh* FinalMesh = Target.bMesh ? Cast<UStaticMesh>(MeshAsset.Reference.Get()) : Component->GetStaticMesh().Get();
            if (!PreserveMaterialSlots(Component, FinalMesh, Target.Expected.Overrides.Num(), Target.Before.Mesh != Target.Expected.Mesh, !Target.Before.SameSlot0(Target.Expected), R)) { return false; }
            Paths.Add(Target.Path); Targets.Add(MoveTemp(Target));
        }
        R.Stage = TEXT("mutation"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
        if(!HismGuard.Check())return false;Blueprint->Modify();if(!HismGuard.Check())return false; Blueprint->SimpleConstructionScript->Modify();if(!HismGuard.Check())return false; Node->Modify();if(!HismGuard.Check())return false;
        for (const FMeshMaterialTarget& Target : Targets)
        {
            auto* Component = FindObject<UStaticMeshComponent>(nullptr, *Target.Path);
            if (!IsMeshMaterialTemplate(Component,HismGuard.Populated)) { return Fail(R, TEXT("component_mesh_apply_failed"), TEXT("A template changed eligibility before applying its binding; inspect partial progress.")); }
            if(!HismGuard.Check())return false;Component->Modify();if(!HismGuard.Check())return false;
            if (Target.bMesh && Component->GetStaticMesh() != MeshAsset.Reference.Get())
            {
                R.Stage = TEXT("mesh"); R.Data->SetBoolField(TEXT("mesh_set_attempted"), true);
                auto Notify = PropertyAccessUtil::BuildBasicChangeNotify(MeshProperty, Component, EPropertyAccessChangeNotifyMode::Default, EPropertyChangeType::ValueSet);
                if(HismGuard.Populated&&!HismGuard.Check())return false;
                PropertyAccessUtil::EmitPreChangeNotify(Notify.Get(), false);if(!HismGuard.Check())return false;
                if(HismGuard.Populated&&!HismGuard.BeginMesh(CastChecked<UStaticMesh>(MeshAsset.Reference.Get())))return false;
                if(HismGuard.Populated)R.Data->SetBoolField(TEXT("hism_mesh_set_entered"),true);
                const bool bSet=Component->SetStaticMesh(CastChecked<UStaticMesh>(MeshAsset.Reference.Get()));
                if(HismGuard.Populated){R.Data->SetBoolField(TEXT("hism_mesh_set_returned"),true);R.Data->SetBoolField(TEXT("hism_mesh_set_return_value"),bSet);R.bChanged|=bSet;}
                if(HismGuard.Populated?!HismGuard.EndMesh(bSet):!HismGuard.Check())return false;PropertyAccessUtil::EmitPostChangeNotify(Notify.Get(), false);if(!HismGuard.Check())return false;
                if (!bSet) { return Fail(R, TEXT("component_mesh_apply_failed"), TEXT("Public SetStaticMesh declined the requested mesh; no rollback was attempted.")); }
            }
            Component = FindObject<UStaticMeshComponent>(nullptr, *Target.Path);
            if (Target.bMaterial && (!IsValid(Component) || Component->OverrideMaterials.IsEmpty() || Component->OverrideMaterials[0] != MaterialAsset.Reference.Get()))
            {
                R.Stage = TEXT("material_slot0");
                if (!IsMeshMaterialTemplate(Component,HismGuard.Populated) || !RejectExternalInstances(Blueprint.Get(), R)) { return Fail(R, TEXT("component_material_apply_failed"), TEXT("Template/instance state changed before slot-0 assignment; an earlier mesh assignment may remain.")); }
                R.Data->SetBoolField(TEXT("material_set_attempted"), true);
                auto Notify = PropertyAccessUtil::BuildBasicChangeNotify(MaterialsProperty, Component, EPropertyAccessChangeNotifyMode::Default, EPropertyChangeType::ValueSet);
                if (Notify) { Notify->ElementIndicesMap.Add(TEXT("OverrideMaterials"), 0); }
                if(HismGuard.Populated&&!HismGuard.Check())return false;
                PropertyAccessUtil::EmitPreChangeNotify(Notify.Get(), false);if(!HismGuard.Check())return false;
                if(HismGuard.Populated&&!HismGuard.BeginMaterial(CastChecked<UMaterialInterface>(MaterialAsset.Reference.Get())))return false;
                if(HismGuard.Populated)R.Data->SetBoolField(TEXT("hism_material_set_entered"),true);Component->SetMaterial(0,CastChecked<UMaterialInterface>(MaterialAsset.Reference.Get()));
                if(HismGuard.Populated)R.Data->SetBoolField(TEXT("hism_material_set_returned"),true);if(HismGuard.Populated?!HismGuard.EndMaterial():!HismGuard.Check())return false;
                if(HismGuard.Populated)R.bChanged=true;PropertyAccessUtil::EmitPostChangeNotify(Notify.Get(), false);if(!HismGuard.Check())return false;
            }
        }
        return true;
    }();
    if(HismGuard.Active&&(HismGuard.Stopped||!HismGuard.Check())){R.bSuccess=false;R.bChangeStateKnown=false;return R;}
    // Keep partially applied mesh changes in the Blueprint's edit/compile state.
    if (R.bMutationAttempted && Blueprint.IsValid()) { FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint.Get());if(!HismGuard.Check())return R; }
    bool bVerified = bApplied;
    if (!Before.BlueprintPath.IsEmpty())
    {
        FString Code, Error;
        if (ReadLocalComponents(Before.BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, After, Code, Error))
        {
            if(!HismGuard.Check())return R;
            R.Data->SetObjectField(TEXT("after"), After.Data); R.Data->SetBoolField(TEXT("dirty_before"), Before.bDirtyBefore); R.Data->SetBoolField(TEXT("dirty_after"), After.bDirtyAfter);
            R.Data->SetNumberField(TEXT("blueprint_status_before"), Before.StatusBefore); R.Data->SetNumberField(TEXT("blueprint_status_after"), After.StatusAfter);
            R.bChanged = Before.bDirtyBefore != After.bDirtyAfter || Before.StatusBefore != After.StatusAfter; R.bChangeStateKnown = true;
            const bool bSCSUnchanged = SCSMatchesExceptSelectedMesh(Before, After, ComponentName); R.Data->SetBoolField(TEXT("scs_structure_unchanged"), bSCSUnchanged); bVerified &= bSCSUnchanged; R.bChanged |= !bSCSUnchanged;
            if (bObserved)
            {
                USCS_Node* Node = After.LocalNodes.FindRef(ComponentName).Get(); auto* Template = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr; FMeshMaterialState Actual;
                if (Template && Actual.Read(Template,HismGuard.Populated?TFunction<bool()>([&]{return HismGuard.Check();}):TFunction<bool()>()))
                {
                    R.Data->SetObjectField(TEXT("static_mesh_material_after"), Actual.Json()); R.bChanged |= !Initial.SameStored(Actual); bVerified &= Actual.SameStored(Expected) && IsMeshMaterialTemplate(Template,true);
                    const auto CurrentAttachment = Attachment(Template, Node); R.Data->SetObjectField(TEXT("attachment_after"), CurrentAttachment);
                    const bool bSameAttachment = FJsonValue::CompareEqual(*MakeShared<FJsonValueObject>(AttachmentBefore), *MakeShared<FJsonValueObject>(CurrentAttachment)); R.Data->SetBoolField(TEXT("attachment_unchanged"), bSameAttachment); bVerified &= bSameAttachment; R.bChanged |= !bSameAttachment;
                }
                else { bVerified = false; R.bChangeStateKnown = false; }
            }
        }
        else { R.Data->SetStringField(TEXT("after_observation_error"), Error); bVerified = false; if (R.bMutationAttempted || Before.bLoadedDuringRead) { R.bChangeStateKnown = false; } }
    }
    if(HismGuard.Populated&&!R.bChangeStateKnown){HismGuard.Stop(TEXT("populated_after_observation_unknown"));return R;}
    TArray<TSharedPtr<FJsonValue>> Observations;
    for (const FMeshMaterialTarget& Target : Targets)
    {
        auto Observation = MakeShared<FJsonObject>(); Observation->SetStringField(TEXT("path"), Target.Path); Observation->SetObjectField(TEXT("before"), Target.Before.Json()); Observation->SetObjectField(TEXT("expected_stored"), Target.Expected.Json(false));
        auto* Component = FindObject<UStaticMeshComponent>(nullptr, *Target.Path); FMeshMaterialState Actual;
        if (Actual.Read(Component,HismGuard.Populated?TFunction<bool()>([&]{return HismGuard.Check();}):TFunction<bool()>()))
        {
            Observation->SetObjectField(TEXT("after"), Actual.Json()); const bool bMatches = Actual.SameStored(Target.Expected) && IsMeshMaterialTemplate(Component,HismGuard.Populated) && (!Target.bMaterial || Actual.EditorEffective == Target.Expected.Overrides[0]);
            Observation->SetBoolField(TEXT("postcondition_passed"), bMatches); bVerified &= bMatches; R.bChanged |= !Actual.SameStored(Target.Before);
        }
        else { bVerified = false; R.bChangeStateKnown = false; Observation->SetBoolField(TEXT("postcondition_passed"), false); }
        if(HismGuard.Stopped)return R;Observations.Add(MakeShared<FJsonValueObject>(Observation));
    }
    if(HismGuard.Stopped)return R;
    R.Data->SetArrayField(TEXT("mesh_material_target_observations"), Observations);
    if (bDirtyBaseline)
    {
        const auto DirtyAfter = DirtyPackages(); R.Data->SetArrayField(TEXT("dirty_packages_after"), Strings(DirtyAfter)); R.bChanged |= DirtyBefore != DirtyAfter;
        for (const FString& Package : DirtyAfter) { if (!DirtyBefore.Contains(Package) && (!Blueprint.IsValid() || Package != Blueprint->GetOutermost()->GetName())) { bVerified = false; R.Data->SetBoolField(TEXT("unexpected_external_dirty_package"), true); } }
    }
    if(!HismGuard.FinishPopulated())return R;
    R.Data->SetBoolField(TEXT("no_op"), bNoOp); R.bSuccess = bApplied && bVerified && R.bChangeStateKnown; R.bPostconditionPassed = R.bSuccess;
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
    else if (R.ErrorCode.IsEmpty()) { R.Stage = TEXT("verification"); Fail(R, TEXT("component_mesh_material_postcondition_failed"), TEXT("Stored bindings, other slots, archetypes or attachment identity did not match; inspect actual changes before retry.")); }
    return R;
}
}

namespace Cubeless::UE58::Component
{
FMutationResult ConfigureBlueprintPCGTemplate(const FString& ParamsJson)
{ return ConfigureNativePCGTemplate(ParamsJson); }
}
