// UE56 Blueprint add/set contracts, adapted to UE58 public editor APIs.
// Original: UnrealMCPBlueprintCommands.cpp HandleAddComponentToBlueprint /
// HandleSetComponentProperty. Original source/license remain in archive/ue56.
#include "Compatibility/UE58/CubelessComponentMutationApi.h"
#include "Compatibility/UE58/CubelessComponentApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
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
#include "UObject/UnrealType.h"

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
UClass* SupportedClass(const FString& Name)
{
    UClass* Classes[] = {USceneComponent::StaticClass(), UStaticMeshComponent::StaticClass(), UCameraComponent::StaticClass(), USpringArmComponent::StaticClass(), USphereComponent::StaticClass(), UBoxComponent::StaticClass(), UCapsuleComponent::StaticClass()};
    for (UClass* Class : Classes)
    { if (Name == Class->GetName() || Name == TEXT("U") + Class->GetName() || Name == Class->GetPathName() || Name + TEXT("Component") == Class->GetName()) { return Class; } }
    return nullptr;
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
    ~FPreparedProperty() { if (Value) { Property->DestroyValue(Value); FMemory::Free(Value); } }
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
    if (!bSocket && (PropertyAccessUtil::CanSetPropertyValue(Property, PropertyAccessUtil::EditorReadOnlyFlags, true) != EPropertyAccessResultFlags::Success || !Template->CanEditChange(Property)))
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

FMutationResult Mutate(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName,
    const FString& TransformJson, const FString& ParentComponent, const FString& PropertiesJson, bool bAdd, bool bCompile, const FString& PropertyName)
{
    FMutationResult R; R.bCompileRequested = bAdd && bCompile;
    R.Data->SetStringField(TEXT("blueprint_name"), BlueprintPath); R.Data->SetStringField(TEXT("component_name"), ComponentName);
    R.Data->SetStringField(TEXT("scope"), TEXT("local reachable SCS; seven existing Engine scene classes with typed properties, or add a saved clean generated non-scene ActorComponent with unchanged defaults; no inherited template edits"));
    R.Data->SetBoolField(TEXT("implicit_postload_work_possible"), true); R.Data->SetBoolField(TEXT("explicit_skeleton_compile_attempted"), false);
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("reachable local SCS identities/selected properties, Blueprint dirty/status and dirty package set; arbitrary PostLoad/notification state is not exhaustively observed"));
    TArray<FString> DirtyBefore; FReadResult Before, After;
    TStrongObjectPtr<UBlueprint> Blueprint;
    TStrongObjectPtr<UBlueprint> ComponentBlueprint;
    TStrongObjectPtr<UClass> AddedClass;
    TArray<TUniquePtr<FPreparedProperty>> Properties;
    FString ActualParent; bool bNoOp = false, bCreationObserved = false, bCreationIncomplete = false, bDirtyBaseline = false;
    const bool bGeneratedRequested = bAdd && ComponentType.StartsWith(TEXT("/Game/"));
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
            PropertyObject = Input->AsObject();
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
                else { PropertyObject->SetField(Fields[I], Value); }
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
        UClass* Class = bAdd ? SupportedClass(ComponentType) : nullptr;
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
            if (!Class) { return Fail(R, TEXT("component_class_unsupported"), TEXT("Supported: seven existing native Engine classes, or an exact saved clean generated non-scene ActorComponent class.")); }
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
            if (!bGeneratedRequested && ActualParent.IsEmpty() && Before.NodeVisits > 0)
            {
                if (SCS->GetRootNodes().Num() != 1) { return Fail(R, TEXT("component_parent_ambiguous"), TEXT("Specify one exact local scene parent when the SCS has multiple roots.")); }
                ActualParent = SCS->GetRootNodes()[0]->GetVariableName().ToString();
            }
            if (!ActualParent.IsEmpty()) { Parent = Before.LocalNodes.FindRef(ActualParent).Get(); if (!Parent || !Cast<USceneComponent>(Parent->ComponentTemplate)) { return Fail(R, TEXT("component_parent_invalid"), TEXT("Parent must be an existing exact local scene component.")); } }
            Template = CastChecked<UActorComponent>(Class->GetDefaultObject());
        }
        else
        {
            Node = Before.LocalNodes.FindRef(ComponentName).Get();
            if (!Node || !Node->ComponentTemplate) { return Fail(R, TEXT("component_not_found"), TEXT("No exact local reachable component exists.")); }
            Template = Node->ComponentTemplate; Class = Template->GetClass();
            if (SupportedClass(Class->GetPathName()) != Class) { return Fail(R, TEXT("component_class_unsupported"), TEXT("Only the seven supported exact native Engine component classes can be modified.")); }
        }
        TSet<FName> PreparedNames;
        for (const auto& Pair : PropertyObject->Values)
        {
            TUniquePtr<FPreparedProperty> Prepared;
            if (!PrepareProperty(Template, FString(Pair.Key.ToView()), Pair.Value, Prepared, R)) { return false; }
            if (PreparedNames.Contains(Prepared->Property->GetFName())) { return Fail(R, TEXT("component_property_duplicate"), TEXT("Duplicate case-insensitive property names are not accepted.")); }
            PreparedNames.Add(Prepared->Property->GetFName()); Properties.Add(MoveTemp(Prepared));
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
        Blueprint->Modify(); SCS->Modify();
        if (bAdd)
        {
            bCreationIncomplete = true;
            Node = SCS->CreateNode(Class, FName(*ComponentName));
            if (Node)
            {
                bCreationObserved = true; R.Data->SetStringField(TEXT("created_node_path"), Node->GetPathName());
                if (Node->ComponentTemplate) { R.Data->SetStringField(TEXT("created_template_path"), Node->ComponentTemplate->GetPathName()); }
            }
            if (!Node || Node->GetVariableName().ToString() != ComponentName || !Node->ComponentTemplate)
            { return Fail(R, TEXT("component_create_failed"), TEXT("CreateNode did not return the requested identity; inspect partial state, no automatic cleanup/retry.")); }
            if (Parent) { Parent->Modify(); Parent->AddChildNode(Node); } else { SCS->AddNode(Node); }
            bCreationIncomplete = false;
            Template = Node->ComponentTemplate;
            if (!CheckPropagation(Template, Properties, Blueprint->GetOutermost(), R)) { return false; }
        }
        Node->Modify(); Template->Modify();
        for (const auto& P : Properties)
        {
            // Reacquire after every notification: it can replace component templates.
            FReadResult Current;
            if (!ReadLocalComponents(Before.BlueprintPath, ComponentName, FString(), false, MaxNodes, MaxJsonChars, Current, Code, Error)) { return Fail(R, TEXT("component_mutation_readback_failed"), Error); }
            Node = Current.LocalNodes.FindRef(ComponentName).Get(); Template = Node ? Node->ComponentTemplate.Get() : nullptr;
            if (!Template || Template->GetClass() != Class || !ApplyProperty(Template, Node, *P)) { return Fail(R, TEXT("component_property_apply_failed"), TEXT("Property notification/application failed after mutation began.")); }
        }
        if (bGeneratedRequested)
        {
            // Public structural refresh creates the SCS object property in the
            // skeleton so the next batch child can author its getter. This is
            // not a full compile, and the manager excludes skeleton-only saves.
            R.Data->SetBoolField(TEXT("explicit_skeleton_compile_attempted"), true);
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint.Get());
            const FObjectPropertyBase* Member = Blueprint->SkeletonGeneratedClass
                ? FindFProperty<FObjectPropertyBase>(Blueprint->SkeletonGeneratedClass, FName(*ComponentName)) : nullptr;
            const bool Matches = Member && Member->GetOwnerClass() == Blueprint->SkeletonGeneratedClass
                && Member->PropertyClass == AddedClass.Get() && Member->HasAnyPropertyFlags(CPF_BlueprintVisible);
            R.Data->SetBoolField(TEXT("skeleton_component_property_verified"), Matches);
            if (Member) { R.Data->SetStringField(TEXT("skeleton_component_property_path"), Member->GetPathName()); }
            if (!Matches) { return Fail(R, TEXT("generated_component_skeleton_mismatch"), TEXT("Component was added but its current skeleton object property does not match; inspect partial state before continuing.")); }
        }
        else { FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint.Get()); }
        if (R.bCompileRequested)
        {
            R.Stage = TEXT("compile"); R.bCompileAttempted = true;
            auto Compile = Cubeless::UE58::CompileBlueprint(Before.BlueprintPath, false, false);
            R.Data->SetObjectField(TEXT("compile"), Compile);
            bool Compiled = false; Compile->TryGetBoolField(TEXT("compiled"), Compiled); R.Compiled = Compiled;
            if (!Compiled) { return Fail(R, TEXT("component_compile_failed"), TEXT("Component was changed but full Blueprint compile failed; inspect diagnostics. Not saved.")); }
        }
        return true;
    }();
    bool bVerified = false;
    if (!Before.BlueprintPath.IsEmpty())
    {
        FString Code, Error;
        if (ReadLocalComponents(Before.BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, After, Code, Error))
        {
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
    if (bCreationIncomplete) { R.bChangeStateKnown = false; }
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
{ return Mutate(BlueprintPath, FString(), ComponentName, FString(), FString(), ValueJson, false, false, PropertyName); }

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
    bool Read(UStaticMeshComponent* Component)
    {
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
bool IsMeshMaterialTemplate(UStaticMeshComponent* Component)
{ return IsValid(Component) && Component->GetClass() == UStaticMeshComponent::StaticClass() && !Component->IsRegistered() && !Component->IsPhysicsStateCreated() && !Component->IsWelded() && !Component->HasBegunPlay(); }
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
    FMutationResult R; R.Data->SetStringField(TEXT("blueprint_name"), BlueprintPath); R.Data->SetStringField(TEXT("component_name"), ComponentName); R.Data->SetStringField(TEXT("component"), ComponentName);
    R.Data->SetStringField(TEXT("scope"), TEXT("local exact StaticMeshComponent; optional mesh and slot-0 material only; empty native path leaves that binding unchanged"));
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
        if (!IsMeshMaterialTemplate(Template)) { return Fail(R, TEXT("component_mesh_template_unsupported"), TEXT("Requires an exact local StaticMeshComponent template, unregistered with no live/welded physics.")); }
        MeshProperty = FindProperty(Template->GetClass(), TEXT("StaticMesh")); MaterialsProperty = FindProperty(Template->GetClass(), TEXT("OverrideMaterials"));
        if ((!StaticMeshPath.IsEmpty() && (!MeshProperty || !Template->CanEditChange(MeshProperty))) || (!MaterialPath.IsEmpty() && (!MaterialsProperty || !Template->CanEditChange(MaterialsProperty))))
        { return Fail(R, TEXT("component_mesh_property_unavailable"), TEXT("Requested mesh/material property is unavailable or not editable on this template.")); }
        if (!Initial.Read(Template)) { return Fail(R, TEXT("component_mesh_before_unreadable"), TEXT("Material overrides are invalid or exceed 1024 entries.")); }
        bObserved = true; Expected = Initial; R.Data->SetObjectField(TEXT("static_mesh_material_before"), Initial.Json());
        AttachmentBefore = Attachment(Template, Node); R.Data->SetObjectField(TEXT("attachment_before"), AttachmentBefore.ToSharedRef());
        // Complete both asset loads and recursive dependency checks before any
        // component write. A missing material must not leave a new mesh behind.
        if (!StaticMeshPath.IsEmpty() && !LoadStaticMesh(MakeShared<FJsonValueString>(StaticMeshPath), MeshAsset, R)) { return false; }
        if (!MaterialPath.IsEmpty() && !LoadSlotMaterial(MaterialPath, MaterialAsset, R)) { return false; }
        if (MeshAsset.Reference.IsValid()) { Expected.Mesh = MeshAsset.Reference->GetPathName(); }
        if (MaterialAsset.Reference.IsValid()) { if (Expected.Overrides.IsEmpty()) { Expected.Overrides.Add(FString()); } Expected.Overrides[0] = MaterialAsset.Reference->GetPathName(); }
        bNoOp = Initial.SameStored(Expected); if (bNoOp) { return true; }
        if (!RejectExternalInstances(Blueprint.Get(), R)) { return false; }
        TArray<UObject*> Instances; Template->GetArchetypeInstances(Instances); Instances.Insert(Template, 0); TSet<FString> Paths;
        for (UObject* Object : Instances)
        {
            UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Object);
            if (!IsMeshMaterialTemplate(Component) || Component->GetOutermost() != Blueprint->GetOutermost()) { return Fail(R, TEXT("component_mesh_archetype_unsupported"), TEXT("All affected archetypes must be same-package native StaticMeshComponent templates without live physics.")); }
            if (Paths.Contains(Component->GetPathName())) { continue; }
            FMeshMaterialTarget Target; Target.Path = Component->GetPathName();
            if (!Target.Before.Read(Component)) { return Fail(R, TEXT("component_mesh_before_unreadable"), TEXT("An archetype override array is invalid or exceeds its observation budget.")); }
            Target.Expected = Target.Before; Target.bMesh = MeshAsset.Reference.IsValid() && Target.Before.Mesh == Initial.Mesh; Target.bMaterial = MaterialAsset.Reference.IsValid() && Target.Before.SameSlot0(Initial);
            TSet<const UObject*> Seen; UObject* Ancestor = Component;
            while (Ancestor != Template)
            {
                if (!IsValid(Ancestor) || Seen.Contains(Ancestor) || Seen.Num() >= 128) { return Fail(R, TEXT("component_mesh_archetype_unsupported"), TEXT("Mesh/material inheritance chain is invalid, cyclic or exceeds 128 components.")); }
                Seen.Add(Ancestor); Ancestor = Ancestor->GetArchetype(); if (Ancestor == Template) { break; }
                auto* Parent = Cast<UStaticMeshComponent>(Ancestor); FMeshMaterialState State;
                if (!IsMeshMaterialTemplate(Parent) || Parent->GetOutermost() != Blueprint->GetOutermost() || !State.Read(Parent)) { return Fail(R, TEXT("component_mesh_archetype_unsupported"), TEXT("Mesh/material chain must reach the selected same-package template.")); }
                Target.bMesh &= State.Mesh == Initial.Mesh; Target.bMaterial &= State.SameSlot0(Initial);
            }
            if (Target.bMesh) { Target.Expected.Mesh = Expected.Mesh; }
            if (Target.bMaterial) { if (Target.Expected.Overrides.IsEmpty()) { Target.Expected.Overrides.Add(FString()); } Target.Expected.Overrides[0] = Expected.Overrides[0]; }
            UStaticMesh* FinalMesh = Target.bMesh ? Cast<UStaticMesh>(MeshAsset.Reference.Get()) : Component->GetStaticMesh().Get();
            if (!PreserveMaterialSlots(Component, FinalMesh, Target.Expected.Overrides.Num(), Target.Before.Mesh != Target.Expected.Mesh, !Target.Before.SameSlot0(Target.Expected), R)) { return false; }
            Paths.Add(Target.Path); Targets.Add(MoveTemp(Target));
        }
        R.Stage = TEXT("mutation"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
        Blueprint->Modify(); Blueprint->SimpleConstructionScript->Modify(); Node->Modify();
        for (const FMeshMaterialTarget& Target : Targets)
        {
            auto* Component = FindObject<UStaticMeshComponent>(nullptr, *Target.Path);
            if (!IsMeshMaterialTemplate(Component)) { return Fail(R, TEXT("component_mesh_apply_failed"), TEXT("A template changed eligibility before applying its binding; inspect partial progress.")); }
            Component->Modify();
            if (Target.bMesh && Component->GetStaticMesh() != MeshAsset.Reference.Get())
            {
                R.Stage = TEXT("mesh"); R.Data->SetBoolField(TEXT("mesh_set_attempted"), true);
                auto Notify = PropertyAccessUtil::BuildBasicChangeNotify(MeshProperty, Component, EPropertyAccessChangeNotifyMode::Default, EPropertyChangeType::ValueSet);
                PropertyAccessUtil::EmitPreChangeNotify(Notify.Get(), false); const bool bSet = Component->SetStaticMesh(CastChecked<UStaticMesh>(MeshAsset.Reference.Get())); PropertyAccessUtil::EmitPostChangeNotify(Notify.Get(), false);
                if (!bSet) { return Fail(R, TEXT("component_mesh_apply_failed"), TEXT("Public SetStaticMesh declined the requested mesh; no rollback was attempted.")); }
            }
            Component = FindObject<UStaticMeshComponent>(nullptr, *Target.Path);
            if (Target.bMaterial && (!IsValid(Component) || Component->OverrideMaterials.IsEmpty() || Component->OverrideMaterials[0] != MaterialAsset.Reference.Get()))
            {
                R.Stage = TEXT("material_slot0");
                if (!IsMeshMaterialTemplate(Component) || !RejectExternalInstances(Blueprint.Get(), R)) { return Fail(R, TEXT("component_material_apply_failed"), TEXT("Template/instance state changed before slot-0 assignment; an earlier mesh assignment may remain.")); }
                R.Data->SetBoolField(TEXT("material_set_attempted"), true);
                auto Notify = PropertyAccessUtil::BuildBasicChangeNotify(MaterialsProperty, Component, EPropertyAccessChangeNotifyMode::Default, EPropertyChangeType::ValueSet);
                if (Notify) { Notify->ElementIndicesMap.Add(TEXT("OverrideMaterials"), 0); }
                PropertyAccessUtil::EmitPreChangeNotify(Notify.Get(), false); Component->SetMaterial(0, CastChecked<UMaterialInterface>(MaterialAsset.Reference.Get())); PropertyAccessUtil::EmitPostChangeNotify(Notify.Get(), false);
            }
        }
        return true;
    }();
    // Keep partially applied mesh changes in the Blueprint's edit/compile state.
    if (R.bMutationAttempted && Blueprint.IsValid()) { FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint.Get()); }
    bool bVerified = bApplied;
    if (!Before.BlueprintPath.IsEmpty())
    {
        FString Code, Error;
        if (ReadLocalComponents(Before.BlueprintPath, FString(), FString(), false, MaxNodes, MaxJsonChars, After, Code, Error))
        {
            R.Data->SetObjectField(TEXT("after"), After.Data); R.Data->SetBoolField(TEXT("dirty_before"), Before.bDirtyBefore); R.Data->SetBoolField(TEXT("dirty_after"), After.bDirtyAfter);
            R.Data->SetNumberField(TEXT("blueprint_status_before"), Before.StatusBefore); R.Data->SetNumberField(TEXT("blueprint_status_after"), After.StatusAfter);
            R.bChanged = Before.bDirtyBefore != After.bDirtyAfter || Before.StatusBefore != After.StatusAfter; R.bChangeStateKnown = true;
            const bool bSCSUnchanged = SCSMatchesExceptSelectedMesh(Before, After, ComponentName); R.Data->SetBoolField(TEXT("scs_structure_unchanged"), bSCSUnchanged); bVerified &= bSCSUnchanged; R.bChanged |= !bSCSUnchanged;
            if (bObserved)
            {
                USCS_Node* Node = After.LocalNodes.FindRef(ComponentName).Get(); auto* Template = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr; FMeshMaterialState Actual;
                if (Template && Actual.Read(Template))
                {
                    R.Data->SetObjectField(TEXT("static_mesh_material_after"), Actual.Json()); R.bChanged |= !Initial.SameStored(Actual); bVerified &= Actual.SameStored(Expected) && IsMeshMaterialTemplate(Template);
                    const auto CurrentAttachment = Attachment(Template, Node); R.Data->SetObjectField(TEXT("attachment_after"), CurrentAttachment);
                    const bool bSameAttachment = FJsonValue::CompareEqual(*MakeShared<FJsonValueObject>(AttachmentBefore), *MakeShared<FJsonValueObject>(CurrentAttachment)); R.Data->SetBoolField(TEXT("attachment_unchanged"), bSameAttachment); bVerified &= bSameAttachment; R.bChanged |= !bSameAttachment;
                }
                else { bVerified = false; R.bChangeStateKnown = false; }
            }
        }
        else { R.Data->SetStringField(TEXT("after_observation_error"), Error); bVerified = false; if (R.bMutationAttempted || Before.bLoadedDuringRead) { R.bChangeStateKnown = false; } }
    }
    TArray<TSharedPtr<FJsonValue>> Observations;
    for (const FMeshMaterialTarget& Target : Targets)
    {
        auto Observation = MakeShared<FJsonObject>(); Observation->SetStringField(TEXT("path"), Target.Path); Observation->SetObjectField(TEXT("before"), Target.Before.Json()); Observation->SetObjectField(TEXT("expected_stored"), Target.Expected.Json(false));
        auto* Component = FindObject<UStaticMeshComponent>(nullptr, *Target.Path); FMeshMaterialState Actual;
        if (Actual.Read(Component))
        {
            Observation->SetObjectField(TEXT("after"), Actual.Json()); const bool bMatches = Actual.SameStored(Target.Expected) && IsMeshMaterialTemplate(Component) && (!Target.bMaterial || Actual.EditorEffective == Target.Expected.Overrides[0]);
            Observation->SetBoolField(TEXT("postcondition_passed"), bMatches); bVerified &= bMatches; R.bChanged |= !Actual.SameStored(Target.Before);
        }
        else { bVerified = false; R.bChangeStateKnown = false; Observation->SetBoolField(TEXT("postcondition_passed"), false); }
        Observations.Add(MakeShared<FJsonValueObject>(Observation));
    }
    R.Data->SetArrayField(TEXT("mesh_material_target_observations"), Observations);
    if (bDirtyBaseline)
    {
        const auto DirtyAfter = DirtyPackages(); R.Data->SetArrayField(TEXT("dirty_packages_after"), Strings(DirtyAfter)); R.bChanged |= DirtyBefore != DirtyAfter;
        for (const FString& Package : DirtyAfter) { if (!DirtyBefore.Contains(Package) && (!Blueprint.IsValid() || Package != Blueprint->GetOutermost()->GetName())) { bVerified = false; R.Data->SetBoolField(TEXT("unexpected_external_dirty_package"), true); } }
    }
    R.Data->SetBoolField(TEXT("no_op"), bNoOp); R.bSuccess = bApplied && bVerified && R.bChangeStateKnown; R.bPostconditionPassed = R.bSuccess;
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
    else if (R.ErrorCode.IsEmpty()) { R.Stage = TEXT("verification"); Fail(R, TEXT("component_mesh_material_postcondition_failed"), TEXT("Stored bindings, other slots, archetypes or attachment identity did not match; inspect actual changes before retry.")); }
    return R;
}
}
