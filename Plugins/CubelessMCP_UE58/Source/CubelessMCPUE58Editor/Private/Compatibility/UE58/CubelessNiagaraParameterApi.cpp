#include "Compatibility/UE58/CubelessNiagaraParameterApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.h"
#include "NiagaraMeshRendererMeshProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/NoExportTypes.h"
#include "UObject/PropertyAccessUtil.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::NiagaraParameter
{
namespace
{
using Function::FResult;
constexpr int32 MaxParameters = 512, MaxRows = 128, MaxBytesPerParameter = 4096, MaxStoreBytes = 262144;
enum class EKind { Unsupported, Float, Int, Bool, Color, Vec2, Vec3, Vec4, Position, Object, Material, ArrayDI };

struct FArrayDIType { const TCHAR* ClassName; const TCHAR* Canonical; const TCHAR* Aliases; };
const FArrayDIType ArrayDITypes[] = {
    {TEXT("NiagaraDataInterfaceArrayPosition"), TEXT("arrayposition"), TEXT("arrayposition|positionarray")},
    {TEXT("NiagaraDataInterfaceArrayFloat3"), TEXT("arrayfloat3"), TEXT("arrayfloat3|float3array|arrayvector|vectorarray")},
    {TEXT("NiagaraDataInterfaceArrayFloat2"), TEXT("arrayfloat2"), TEXT("arrayfloat2|float2array|arrayvector2|vector2array")},
    {TEXT("NiagaraDataInterfaceArrayFloat"), TEXT("arrayfloat"), TEXT("arrayfloat|floatarray")},
    {TEXT("NiagaraDataInterfaceArrayInt32"), TEXT("arrayint32"), TEXT("arrayint32|int32array|arrayint|intarray")},
    {TEXT("NiagaraDataInterfaceArrayMesh"), TEXT("arraymesh"), TEXT("arraymesh|mesharray|staticmesharray|arraystaticmesh")}
};

FString Path(const UObject* Object) { return Object ? Object->GetPathName() : FString(); }
bool Loaded(const UObject* Object)
{ return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects); }
FString Text(const TSharedRef<FJsonObject>& J)
{ FString Out; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out)); return Out; }
FString ValueText(const TSharedPtr<FJsonValue>& V)
{ FString Out; TArray<TSharedPtr<FJsonValue>> Values{V}; FJsonSerializer::Serialize(Values, TJsonWriterFactory<>::Create(&Out)); return Out; }
bool Fail(FResult& R, const TCHAR* Code, const TCHAR* Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
TSharedPtr<FJsonValue> Null() { return MakeShared<FJsonValueNull>(); }
TSharedPtr<FJsonValue> Number(double Value) { return MakeShared<FJsonValueNumber>(Value); }
TSharedPtr<FJsonValue> Array(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (double V : Values) { Out.Add(Number(V)); }
    return MakeShared<FJsonValueArray>(Out);
}

// The unannotated derived overrides are not direct external module symbols.
// Base NIAGARA_API virtual dispatch still reaches User redirection/Position.
FORCENOINLINE bool Add(FNiagaraParameterStore& Store, const FNiagaraVariable& V, int32& Offset)
{ return Store.AddParameter(V, true, true, &Offset); }
FORCENOINLINE bool SetPosition(FNiagaraParameterStore& Store, const FVector& V, FName Name)
{ return Store.SetPositionParameterValue(V, Name, false); }
FORCENOINLINE const FVector* GetPosition(const FNiagaraParameterStore& Store, FName Name)
{ return Store.GetPositionParameterValue(Name); }

bool RuntimeClass(const UClass* Class, bool bArrayDIAncestors = false);
const FArrayDIType* ArrayType(const UClass* Class)
{
    if (!Loaded(Class) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
        || !RuntimeClass(Class, true) || !Class->IsChildOf(UNiagaraDataInterface::StaticClass())
        || !Class->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn) || !Class->GetSuperClass()
        || Class->GetSuperClass()->GetPathName() != TEXT("/Script/Niagara.NiagaraDataInterfaceArray")) { return nullptr; }
    for (const auto& Type : ArrayDITypes)
    { if (Class->GetPathName() == FString(TEXT("/Script/Niagara.")) + Type.ClassName) { return &Type; } }
    return nullptr;
}

EKind Kind(const FNiagaraTypeDefinition& T)
{
    if (!T.IsValid() || T.GetFlags() != 0 || T.IsEnum()) { return EKind::Unsupported; }
    if (T == FNiagaraTypeDefinition::GetFloatDef() && T.GetSize() == sizeof(float)) { return EKind::Float; }
    if (T == FNiagaraTypeDefinition::GetIntDef() && T.GetSize() == sizeof(int32)) { return EKind::Int; }
    if (T == FNiagaraTypeDefinition::GetBoolDef() && T.GetSize() == sizeof(FNiagaraBool)) { return EKind::Bool; }
    if (T == FNiagaraTypeDefinition::GetColorDef() && T.GetSize() == sizeof(FLinearColor)) { return EKind::Color; }
    if (T == FNiagaraTypeDefinition::GetVec2Def() && T.GetSize() == sizeof(FVector2f)) { return EKind::Vec2; }
    if (T == FNiagaraTypeDefinition::GetVec3Def() && T.GetSize() == sizeof(FVector3f)) { return EKind::Vec3; }
    if (T == FNiagaraTypeDefinition::GetVec4Def() && T.GetSize() == sizeof(FVector4f)) { return EKind::Vec4; }
    if (T == FNiagaraTypeDefinition::GetPositionDef() && T.GetSize() == sizeof(FNiagaraPosition)) { return EKind::Position; }
    if (T == FNiagaraTypeDefinition::GetUObjectDef() && T.GetClass() == UObject::StaticClass()) { return EKind::Object; }
    if (T == FNiagaraTypeDefinition::GetUMaterialDef() && T.GetClass() == UMaterialInterface::StaticClass()) { return EKind::Material; }
    if (T.IsDataInterface() && T.GetSize() == 0 && ArrayType(T.GetClass())) { return EKind::ArrayDI; }
    return EKind::Unsupported;
}
FString KindName(EKind K)
{
    switch (K)
    {
    case EKind::Float: return TEXT("float"); case EKind::Int: return TEXT("int"); case EKind::Bool: return TEXT("bool");
    case EKind::Color: return TEXT("color"); case EKind::Vec2: return TEXT("vec2"); case EKind::Vec3: return TEXT("vec3");
    case EKind::Vec4: return TEXT("vec4"); case EKind::Position: return TEXT("position");
    case EKind::Object: return TEXT("object"); case EKind::Material: return TEXT("materialinterface");
    case EKind::ArrayDI: return TEXT("array_data_interface");
    default: return TEXT("unsupported");
    }
}
FString CanonicalType(const FNiagaraTypeDefinition& T)
{ const auto* A = Kind(T) == EKind::ArrayDI ? ArrayType(T.GetClass()) : nullptr; return A ? FString(A->Canonical) : KindName(Kind(T)); }
bool ValueSettable(const FNiagaraTypeDefinition& T)
{ const EKind K = Kind(T); return K != EKind::Unsupported && (K != EKind::ArrayDI || CanonicalType(T) == TEXT("arraymesh")); }
bool ParseType(const FString& Input, FNiagaraTypeDefinition& T)
{
    FString N = Input.TrimStartAndEnd().ToLower(); N.ReplaceInline(TEXT("_"), TEXT(""));
    N.ReplaceInline(TEXT("-"), TEXT("")); N.ReplaceInline(TEXT(" "), TEXT(""));
    if (N == TEXT("float") || N == TEXT("scalar")) { T = FNiagaraTypeDefinition::GetFloatDef(); }
    else if (N == TEXT("int") || N == TEXT("integer")) { T = FNiagaraTypeDefinition::GetIntDef(); }
    else if (N == TEXT("bool") || N == TEXT("boolean")) { T = FNiagaraTypeDefinition::GetBoolDef(); }
    else if (N == TEXT("color") || N == TEXT("linearcolor")) { T = FNiagaraTypeDefinition::GetColorDef(); }
    else if (N == TEXT("vec2") || N == TEXT("vector2") || N == TEXT("vector2d")) { T = FNiagaraTypeDefinition::GetVec2Def(); }
    else if (N == TEXT("vec3") || N == TEXT("vector3") || N == TEXT("vector")) { T = FNiagaraTypeDefinition::GetVec3Def(); }
    else if (N == TEXT("vec4") || N == TEXT("vector4")) { T = FNiagaraTypeDefinition::GetVec4Def(); }
    else if (N == TEXT("position")) { T = FNiagaraTypeDefinition::GetPositionDef(); }
    else if (N == TEXT("object") || N == TEXT("uobject")) { T = FNiagaraTypeDefinition::GetUObjectDef(); }
    else if (N == TEXT("material") || N == TEXT("materialinterface")) { T = FNiagaraTypeDefinition::GetUMaterialDef(); }
    else
    {
        const bool bClassPath = N.StartsWith(TEXT("/script/niagara."));
        if (bClassPath) { N.RightChopInline(16); }
        const FArrayDIType* Match = nullptr;
        for (const auto& A : ArrayDITypes)
        {
            TArray<FString> Aliases; FString(A.Aliases).ParseIntoArray(Aliases, TEXT("|"));
            if (N == FString(A.ClassName).ToLower() || (!bClassPath && Aliases.Contains(N))) { Match = &A; break; }
        }
        if (!Match) { return false; }
        UClass* Class = FindObject<UClass>(nullptr, *(FString(TEXT("/Script/Niagara.")) + Match->ClassName));
        if (ArrayType(Class) != Match) { return false; }
        T = FNiagaraTypeDefinition(Class);
    }
    return Kind(T) != EKind::Unsupported;
}
bool CanonicalName(const FString& Input, FString& Out)
{
    FString N = Input.TrimStartAndEnd(); if (N.StartsWith(TEXT("User."), ESearchCase::IgnoreCase)) { N.RightChopInline(5); }
    if (N.IsEmpty() || N.Len() > 128 || N.StartsWith(TEXT(".")) || N.EndsWith(TEXT(".")) || N.Contains(TEXT(".."))) { return false; }
    for (TCHAR C : N) { if (!(FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('.'))) { return false; } }
    Out = TEXT("User.") + N; return true;
}
bool RuntimeClass(const UClass* Class, bool bArrayDIAncestors)
{
    for (const UClass* C = Class; C; C = C->GetSuperClass())
    {
        if (!Loaded(C) || !C->HasAnyClassFlags(CLASS_Native) || C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)) { return false; }
        const FString Package = C->GetOutermost()->GetName();
        // UE58 data interfaces inherit these two native NiagaraCore classes.
        // This exception is only used by the exact six-class ArrayType guard;
        // UObject value admission keeps its original module restriction.
        const bool bKnownArrayDIBase = bArrayDIAncestors && Package == TEXT("/Script/NiagaraCore")
            && (C->GetPathName() == TEXT("/Script/NiagaraCore.NiagaraDataInterfaceBase")
                || C->GetPathName() == TEXT("/Script/NiagaraCore.NiagaraNotifyOnChanged"));
        if (Package != TEXT("/Script/CoreUObject") && Package != TEXT("/Script/Engine")
            && Package != TEXT("/Script/Niagara") && !bKnownArrayDIBase) { return false; }
    }
    return Class != nullptr;
}
TSharedRef<FJsonObject> CompileState(UNiagaraSystem* System)
{
    auto J = MakeShared<FJsonObject>();
    const bool Outstanding = System->HasOutstandingCompilationRequests(true), Needs = System->NeedsRequestCompile();
    J->SetBoolField(TEXT("outstanding_including_gpu"), Outstanding); J->SetBoolField(TEXT("needs_request_compile"), Needs);
    J->SetBoolField(TEXT("ready_to_run"), System->IsReadyToRun());
    J->SetBoolField(TEXT("ready_to_save"), !Outstanding && !Needs);
    J->SetBoolField(TEXT("compile_success_verified"), false); J->SetBoolField(TEXT("poll_or_flush_invoked"), false); return J;
}
TSharedRef<FJsonObject> PackageState(UNiagaraSystem* System)
{
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("system_path"), Path(System)); J->SetStringField(TEXT("system_class_path"), Path(System->GetClass()));
    J->SetStringField(TEXT("package_name"), System->GetOutermost()->GetName());
    J->SetStringField(TEXT("package_flags_string"), LexToString(static_cast<uint32>(System->GetOutermost()->GetPackageFlags())));
    J->SetBoolField(TEXT("package_dirty"), System->GetOutermost()->IsDirty()); return J;
}

FString MeshSpaceName(ENiagaraMeshPivotOffsetSpace Space)
{
    switch (Space)
    {
    case ENiagaraMeshPivotOffsetSpace::Mesh: return TEXT("Mesh");
    case ENiagaraMeshPivotOffsetSpace::Simulation: return TEXT("Simulation");
    case ENiagaraMeshPivotOffsetSpace::World: return TEXT("World");
    case ENiagaraMeshPivotOffsetSpace::Local: return TEXT("Local");
    default: return FString();
    }
}
bool EditableMeshField(const FProperty* Property, const UStruct* Owner)
{
    return Property && Property->GetOwnerStruct() == Owner && Property->ArrayDim == 1
        && Property->HasAllPropertyFlags(CPF_Edit | CPF_NativeAccessSpecifierPublic)
        && !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance)
        && PropertyAccessUtil::CanGetPropertyValue(Property) == EPropertyAccessResultFlags::Success
        && PropertyAccessUtil::CanSetPropertyValue(Property, PropertyAccessUtil::EditorReadOnlyFlags, false)
            == EPropertyAccessResultFlags::Success;
}
// The ArrayMesh class is installed under Internal. Admit only its exact loaded
// public property and public base struct; no Internal header, derived symbol,
// proxy access, unexported enum/struct constructor or arbitrary field write.
bool MeshProperty(UClass* Class, FArrayProperty*& Out)
{
    Out = nullptr;
    if (!ArrayType(Class) || Class->GetPathName() != TEXT("/Script/Niagara.NiagaraDataInterfaceArrayMesh")) { return false; }
    FArrayProperty* Property = FindFProperty<FArrayProperty>(Class, TEXT("MeshData"));
    FStructProperty* Inner = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
    UScriptStruct* Struct = Inner ? Inner->Struct : nullptr;
    if (!EditableMeshField(Property, Class) || !Inner || Inner->ArrayDim != 1 || !Loaded(Struct)
        || Struct->GetPathName() != TEXT("/Script/Niagara.NiagaraMeshRendererMeshPropertiesBase")
        || Struct->GetOutermost()->GetName() != TEXT("/Script/Niagara")
        || !Struct->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)
        || Struct->GetStructureSize() != sizeof(FNiagaraMeshRendererMeshPropertiesBase)
        || Struct->GetMinAlignment() != alignof(FNiagaraMeshRendererMeshPropertiesBase)) { return false; }
    FObjectPropertyBase* Mesh = FindFProperty<FObjectPropertyBase>(Struct, TEXT("Mesh"));
    FStructProperty* Scale = FindFProperty<FStructProperty>(Struct, TEXT("Scale"));
    FStructProperty* Rotation = FindFProperty<FStructProperty>(Struct, TEXT("Rotation"));
    FStructProperty* Pivot = FindFProperty<FStructProperty>(Struct, TEXT("PivotOffset"));
    FEnumProperty* Space = FindFProperty<FEnumProperty>(Struct, TEXT("PivotOffsetSpace"));
    if (!EditableMeshField(Mesh, Struct) || Mesh->PropertyClass != UStaticMesh::StaticClass()
        || !EditableMeshField(Scale, Struct) || Scale->Struct != TBaseStructure<FVector>::Get()
        || !EditableMeshField(Rotation, Struct) || Rotation->Struct != TBaseStructure<FRotator>::Get()
        || !EditableMeshField(Pivot, Struct) || Pivot->Struct != TBaseStructure<FVector>::Get()
        || !EditableMeshField(Space, Struct) || !CastField<FByteProperty>(Space->GetUnderlyingProperty())
        || !Loaded(Space->GetEnum()) || Space->GetEnum()->GetPathName() != TEXT("/Script/Niagara.ENiagaraMeshPivotOffsetSpace")) { return false; }
    int32 Fields = 0;
    for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::ExcludeSuper); It; ++It) { if (++Fields > 5) { return false; } }
    if (Fields != 5) { return false; }
    for (const auto Value : {ENiagaraMeshPivotOffsetSpace::Mesh, ENiagaraMeshPivotOffsetSpace::Simulation,
        ENiagaraMeshPivotOffsetSpace::World, ENiagaraMeshPivotOffsetSpace::Local})
    {
        const int32 Index = Space->GetEnum()->GetIndexByValue(static_cast<int64>(Value));
        if (Index == INDEX_NONE || Space->GetEnum()->HasMetaData(TEXT("Hidden"), Index)
            || Space->GetEnum()->GetNameStringByIndex(Index) != MeshSpaceName(Value)) { return false; }
    }
    Out = Property; return true;
}
TSharedRef<FJsonObject> MeshContract(FArrayProperty* Property)
{
    auto J = MakeShared<FJsonObject>(); auto* Inner = CastField<FStructProperty>(Property->Inner);
    J->SetStringField(TEXT("data_interface_class_path"), Property->GetOwnerStruct()->GetPathName());
    J->SetStringField(TEXT("property_path"), Property->GetPathName());
    J->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
    J->SetStringField(TEXT("inner_struct_path"), Inner->Struct->GetPathName());
    J->SetNumberField(TEXT("struct_size"), Inner->Struct->GetStructureSize());
    J->SetNumberField(TEXT("struct_alignment"), Inner->Struct->GetMinAlignment());
    TArray<TSharedPtr<FJsonValue>> Fields;
    for (TFieldIterator<FProperty> It(Inner->Struct, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        auto F = MakeShared<FJsonObject>(); F->SetStringField(TEXT("name"), It->GetName());
        F->SetStringField(TEXT("property_path"), It->GetPathName()); F->SetStringField(TEXT("cpp_type"), It->GetCPPType());
        F->SetStringField(TEXT("flags_string"), LexToString(static_cast<uint64>(It->GetPropertyFlags())));
        F->SetBoolField(TEXT("editor_edit_permission_verified"), EditableMeshField(*It, Inner->Struct));
        Fields.Add(MakeShared<FJsonValueObject>(F));
    }
    J->SetArrayField(TEXT("fields"), Fields); J->SetBoolField(TEXT("finite_public_contract_verified"), true);
    J->SetStringField(TEXT("setter_api"), TEXT("PropertyAccessUtil::SetPropertyValue_Object(EditorReadOnlyFlags,Always)")); return J;
}
bool MeshAsset(UStaticMesh* Mesh)
{
    const FString ObjectPath = Path(Mesh);
    return Loaded(Mesh) && Mesh->GetClass() == UStaticMesh::StaticClass() && Mesh->IsAsset() && !Mesh->IsEditorOnly()
        && RuntimeClass(Mesh->GetClass()) && !Mesh->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn)
        && (ObjectPath.StartsWith(TEXT("/Game/")) || ObjectPath.StartsWith(TEXT("/Engine/")));
}
TSharedRef<FJsonObject> MeshAssetState(UStaticMesh* Mesh)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("object_path"), Path(Mesh));
    J->SetStringField(TEXT("class_path"), Mesh ? Path(Mesh->GetClass()) : FString());
    J->SetBoolField(TEXT("loaded_runtime_static_mesh"), MeshAsset(Mesh));
    if (Mesh)
    {
        J->SetStringField(TEXT("package_name"), Mesh->GetOutermost()->GetName());
        J->SetStringField(TEXT("package_flags_string"), LexToString(static_cast<uint32>(Mesh->GetOutermost()->GetPackageFlags())));
        J->SetBoolField(TEXT("package_dirty"), Mesh->GetOutermost()->IsDirty());
    }
    return J;
}
bool MeshValues(const TArray<FNiagaraMeshRendererMeshPropertiesBase>& Meshes, TSharedPtr<FJsonValue>& Out)
{
    if (Meshes.Num() < 0 || Meshes.Num() > MaxParameters) { return false; }
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const auto& Mesh : Meshes)
    {
        const FString Space = MeshSpaceName(Mesh.PivotOffsetSpace);
        if (!MeshAsset(Mesh.Mesh.Get()) || Mesh.Scale.ContainsNaN() || Mesh.Rotation.ContainsNaN()
            || Mesh.PivotOffset.ContainsNaN() || Space.IsEmpty()) { return false; }
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("mesh"), Path(Mesh.Mesh.Get()));
        J->SetField(TEXT("scale"), Array({Mesh.Scale.X, Mesh.Scale.Y, Mesh.Scale.Z}));
        J->SetField(TEXT("rotation"), Array({Mesh.Rotation.Pitch, Mesh.Rotation.Yaw, Mesh.Rotation.Roll}));
        J->SetField(TEXT("pivot_offset"), Array({Mesh.PivotOffset.X, Mesh.PivotOffset.Y, Mesh.PivotOffset.Z}));
        J->SetStringField(TEXT("pivot_offset_space"), Space); Values.Add(MakeShared<FJsonValueObject>(J));
    }
    Out = MakeShared<FJsonValueArray>(Values); return ValueText(Out).Len() <= 65536;
}
bool ReadMesh(UNiagaraDataInterface* DI, TSharedPtr<FJsonValue>& Out)
{
    FArrayProperty* Property = nullptr;
    if (!Loaded(DI) || !MeshProperty(DI->GetClass(), Property)) { return false; }
    // This layout is the exact public native TArray/base-struct shape checked
    // above. Reading through the public type does not require ArrayMesh linkage.
    const auto* Meshes = Property->ContainerPtrToValuePtr<TArray<FNiagaraMeshRendererMeshPropertiesBase>>(DI);
    return Meshes && MeshValues(*Meshes, Out);
}

bool ReadValue(const FNiagaraParameterStore& Store, const FNiagaraVariable& V, TSharedPtr<FJsonValue>& Out)
{
    const EKind K = Kind(V.GetType()); const int32 Offset = Store.IndexOf(V), Size = V.GetSizeInBytes(); Out = Null();
    if (K == EKind::Unsupported) { return true; }
    if (K == EKind::ArrayDI)
    {
        if (CanonicalType(V.GetType()) != TEXT("arraymesh")) { return true; }
        if (!Store.GetDataInterfaces().IsValidIndex(Offset)) { return false; }
        return ReadMesh(Store.GetDataInterface(Offset), Out);
    }
    if (K == EKind::Object || K == EKind::Material)
    {
        if (!Store.GetUObjects().IsValidIndex(Offset)) { return false; }
        UObject* Object = Store.GetUObject(Offset).Get();
        if (Object && (!Loaded(Object) || !Object->IsA(V.GetType().GetClass()))) { return false; }
        if (Object) { Out = MakeShared<FJsonValueString>(Path(Object)); } return true;
    }
    if (Offset < 0 || Size <= 0 || Size > MaxBytesPerParameter || Offset > Store.GetParameterDataArray().Num() - Size) { return false; }
    if (K == EKind::Position)
    {
        const FVector* P = GetPosition(Store, V.GetName());
        if (!P || !FMath::IsFinite(P->X) || !FMath::IsFinite(P->Y) || !FMath::IsFinite(P->Z)) { return false; }
        Out = Array({P->X, P->Y, P->Z}); return true;
    }
    switch (K)
    {
    case EKind::Float:
    { const float X = Store.GetParameterValue<float>(V); if (!FMath::IsFinite(X)) { return false; } Out = Number(X); break; }
    case EKind::Int: Out = Number(Store.GetParameterValue<int32>(V)); break;
    case EKind::Bool:
    { const FNiagaraBool X = Store.GetParameterValue<FNiagaraBool>(V); if (!X.IsValid()) { return false; } Out = MakeShared<FJsonValueBoolean>(X.GetValue()); break; }
    case EKind::Vec2:
    { const FVector2f X = Store.GetParameterValue<FVector2f>(V); if (!FMath::IsFinite(X.X) || !FMath::IsFinite(X.Y)) { return false; } Out = Array({X.X, X.Y}); break; }
    case EKind::Vec3:
    { const FVector3f X = Store.GetParameterValue<FVector3f>(V); if (X.ContainsNaN()) { return false; } Out = Array({X.X, X.Y, X.Z}); break; }
    case EKind::Vec4:
    { const FVector4f X = Store.GetParameterValue<FVector4f>(V); if (!FMath::IsFinite(X.X) || !FMath::IsFinite(X.Y) || !FMath::IsFinite(X.Z) || !FMath::IsFinite(X.W)) { return false; } Out = Array({X.X, X.Y, X.Z, X.W}); break; }
    case EKind::Color:
    { const FLinearColor X = Store.GetParameterValue<FLinearColor>(V); if (!FMath::IsFinite(X.R) || !FMath::IsFinite(X.G) || !FMath::IsFinite(X.B) || !FMath::IsFinite(X.A)) { return false; } Out = Array({X.R, X.G, X.B, X.A}); break; }
    default: return false;
    }
    return true;
}

struct FSnapshot
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    TMap<FString, FString> ParameterStates;
    TMap<FString, FNiagaraVariable> Users;
    TArray<TSharedPtr<FJsonValue>> UserRows;
    bool bComplete = true;
};
// Read only public reflected storage for the exact six declaration classes.
// Mesh payload writes use the separate exact permission-checked property route.
// Float2/3 expose editor and runtime arrays: both must be empty for a new DI.
bool ArrayPayload(UNiagaraDataInterface* DI, TSharedRef<FJsonObject>& Out)
{
    const auto* Type = DI ? ArrayType(DI->GetClass()) : nullptr;
    if (!Type || !Loaded(DI)) { return false; }
    TArray<TPair<FString, FString>> Expected;
    const FString C = Type->Canonical;
    if (C == TEXT("arrayfloat")) { Expected.Emplace(TEXT("FloatData"), TEXT("float")); }
    else if (C == TEXT("arrayint32")) { Expected.Emplace(TEXT("IntData"), TEXT("int32")); }
    else if (C == TEXT("arrayposition")) { Expected.Emplace(TEXT("PositionData"), TEXT("FNiagaraPosition")); }
    else if (C == TEXT("arrayfloat2"))
    { Expected.Emplace(TEXT("FloatData"), TEXT("FVector2D")); Expected.Emplace(TEXT("InternalFloatData"), TEXT("FVector2f")); }
    else if (C == TEXT("arrayfloat3"))
    { Expected.Emplace(TEXT("FloatData"), TEXT("FVector")); Expected.Emplace(TEXT("InternalFloatData"), TEXT("FVector3f")); }
    else if (C == TEXT("arraymesh")) { Expected.Emplace(TEXT("MeshData"), TEXT("FNiagaraMeshRendererMeshPropertiesBase")); }
    else { return false; }
    bool bEmpty = true; TArray<TSharedPtr<FJsonValue>> Fields;
    for (const auto& E : Expected)
    {
        FArrayProperty* A = FindFProperty<FArrayProperty>(DI->GetClass(), FName(*E.Key));
        if (!A || A->GetOwnerStruct() != DI->GetClass() || A->ArrayDim != 1 || !A->Inner || A->Inner->ArrayDim != 1
            || !A->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic) || A->Inner->GetCPPType() != E.Value) { return false; }
        const void* Data = A->ContainerPtrToValuePtr<void>(DI); FScriptArrayHelper H(A, Data);
        if (H.Num() < 0 || H.Num() > MaxParameters) { return false; }
        FString Export; if (!A->ExportText_Direct(Export, Data, Data, DI, PPF_None) || Export.Len() > 65536) { return false; }
        auto F = MakeShared<FJsonObject>(); F->SetStringField(TEXT("property_name"), E.Key);
        F->SetStringField(TEXT("property_path"), A->GetPathName()); F->SetStringField(TEXT("inner_cpp_type"), A->Inner->GetCPPType());
        F->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(A->GetPropertyFlags())));
        F->SetNumberField(TEXT("element_count"), H.Num()); F->SetBoolField(TEXT("empty"), H.Num() == 0);
        F->SetStringField(TEXT("export_text"), Export); Fields.Add(MakeShared<FJsonValueObject>(F)); bEmpty &= H.Num() == 0;
    }
    Out->SetArrayField(TEXT("fields"), Fields); Out->SetBoolField(TEXT("all_empty"), bEmpty);
    const bool bMesh = C == TEXT("arraymesh");
    Out->SetBoolField(TEXT("payload_write_supported"), bMesh);
    Out->SetStringField(TEXT("scope"), bMesh ? TEXT("exact public MeshData five-field value/identity readback; finite permission-checked Mesh payload setter")
        : TEXT("bounded public reflected storage read for declaration emptiness/preservation only; numeric payload setters remain unsupported"));
    return true;
}
FSnapshot Capture(UNiagaraSystem* System)
{
    FSnapshot S; if (!Loaded(System)) { S.bComplete = false; return S; }
    const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
    S.Json->SetObjectField(TEXT("package"), PackageState(System)); S.Json->SetObjectField(TEXT("compile_state"), CompileState(System));
    S.Json->SetStringField(TEXT("store_owner"), Path(Store.GetOwner()));
    S.Json->SetNumberField(TEXT("layout_version"), Store.GetLayoutVersion());
    S.Json->SetBoolField(TEXT("parameters_dirty"), Store.GetParametersDirty()); S.Json->SetBoolField(TEXT("uobjects_dirty"), Store.GetUObjectsDirty());
    S.Json->SetBoolField(TEXT("interfaces_dirty"), Store.GetInterfacesDirty()); S.Json->SetBoolField(TEXT("position_data_dirty"), Store.GetPositionDataDirty());
    TArray<FNiagaraVariable> Variables; Store.GetParameters(Variables);
    Variables.Sort([](const FNiagaraVariable& A, const FNiagaraVariable& B) { return A.GetName().ToString() < B.GetName().ToString(); });
    if (Variables.Num() > MaxParameters || Store.GetParameterDataArray().Num() > MaxStoreBytes
        || Store.GetUObjects().Num() > MaxParameters || Store.GetDataInterfaces().Num() > MaxParameters || Store.GetOwner() != System)
    { S.bComplete = false; return S; }
    TArray<TSharedPtr<FJsonValue>> AllRows;
    for (const FNiagaraVariable& V : Variables)
    {
        auto Row = MakeShared<FJsonObject>(); const auto& T = V.GetType();
        if (!T.IsValid()) { S.bComplete = false; break; }
        const FString Name = V.GetName().ToString(); const int32 Offset = Store.IndexOf(V), Size = V.GetSizeInBytes();
        Row->SetStringField(TEXT("name"), Name); Row->SetStringField(TEXT("type"), T.GetName());
        Row->SetStringField(TEXT("canonical_type"), CanonicalType(T)); Row->SetNumberField(TEXT("size_bytes"), Size);
        Row->SetStringField(TEXT("type_object_path"), T.GetClass() ? Path(T.GetClass()) : Path(T.GetScriptStruct()));
        Row->SetStringField(TEXT("type_flags_string"), LexToString(T.GetFlags())); Row->SetNumberField(TEXT("offset"), Offset);
        Row->SetBoolField(TEXT("settable_by_mcp"), ValueSettable(T));
        TSharedPtr<FJsonValue> Value;
        if (!ReadValue(Store, V, Value)) { S.bComplete = false; break; }
        Row->SetField(TEXT("value"), Value);
        if (T.IsDataInterface())
        {
            if (!Store.GetDataInterfaces().IsValidIndex(Offset)) { S.bComplete = false; break; }
            UObject* DI = Store.GetDataInterface(Offset); Row->SetStringField(TEXT("reference_path"), Path(DI));
            Row->SetStringField(TEXT("reference_class"), DI ? Path(DI->GetClass()) : FString());
            if (Kind(T) == EKind::ArrayDI)
            {
                auto Payload = MakeShared<FJsonObject>(); auto* Interface = Cast<UNiagaraDataInterface>(DI);
                if (!Loaded(Interface) || Interface->GetClass() != T.GetClass() || Interface->GetOuter() != System
                    || Interface->GetOutermost() != System->GetOutermost() || Interface->IsTemplate() || Interface->IsEditorOnly()
                    || !ArrayPayload(Interface, Payload)) { S.bComplete = false; break; }
                Row->SetStringField(TEXT("reference_owner_path"), Path(Interface->GetOuter()));
                Row->SetBoolField(TEXT("data_interface_identity_verified"), true);
                Row->SetBoolField(TEXT("array_declaration_supported"), true); Row->SetObjectField(TEXT("array_payload"), Payload);
            }
        }
        else if (T.IsUObject())
        {
            if (!Store.GetUObjects().IsValidIndex(Offset)) { S.bComplete = false; break; }
            UObject* Object = Store.GetUObject(Offset).Get(); Row->SetStringField(TEXT("reference_path"), Path(Object));
            Row->SetStringField(TEXT("reference_class"), Object ? Path(Object->GetClass()) : FString());
        }
        else
        {
            if (Size < 0 || Size > MaxBytesPerParameter || Offset < 0 || Offset > Store.GetParameterDataArray().Num() - Size) { S.bComplete = false; break; }
            FString Bytes; const uint8* Data = Store.GetParameterDataArray().GetData() + Offset;
            for (int32 I = 0; I < Size; ++I) { Bytes += FString::Printf(TEXT("%02x"), Data[I]); }
            Row->SetStringField(TEXT("raw_bytes_hex"), Bytes);
        }
        // Offsets may move when a different declaration is inserted. Preserve
        // each previous parameter's type/value/reference, rather than its slot.
        auto Identity = MakeShared<FJsonObject>(); Identity->Values = Row->Values; Identity->RemoveField(TEXT("offset"));
        const FString Key = Name.ToLower();
        if (S.ParameterStates.Contains(Key)) { S.bComplete = false; break; }
        S.ParameterStates.Add(Key, Text(Identity)); AllRows.Add(MakeShared<FJsonValueObject>(Row));
        if (Name.StartsWith(TEXT("User."))) { S.Users.Add(Key, V); S.UserRows.Add(MakeShared<FJsonValueObject>(Row)); }
    }
    TArray<FNiagaraVariable> Redirects; Store.GetUserParameters(Redirects);
    if (Redirects.Num() != S.Users.Num()) { S.bComplete = false; }
    for (const auto& V : Redirects)
    {
        FNiagaraVariableBase Canonical(V);
        if (!Store.RedirectUserVariable(Canonical)) { S.bComplete = false; break; }
        const FNiagaraVariable* Stored = S.Users.Find(Canonical.GetName().ToString().ToLower());
        if (!Stored || Stored->GetType() != Canonical.GetType()) { S.bComplete = false; break; }
    }
    S.Json->SetArrayField(TEXT("parameters"), S.UserRows); S.Json->SetArrayField(TEXT("all_parameters"), AllRows);
    S.Json->SetBoolField(TEXT("observation_complete"), S.bComplete);
    S.Json->SetStringField(TEXT("data_interface_observation_scope"), TEXT("identity for all DIs; exact six Array-DI reflected storage, Mesh five-field structured values, numeric declaration emptiness/preservation only"));
    return S;
}
bool PreservedExcept(const FSnapshot& Before, const FSnapshot& After, const FString& Except)
{
    if (!Before.bComplete || !After.bComplete) { return false; }
    for (const auto& P : Before.ParameterStates)
    {
        if (P.Key == Except) { continue; }
        const FString* Current = After.ParameterStates.Find(P.Key);
        if (!Current || *Current != P.Value) { return false; }
    }
    for (const auto& P : After.ParameterStates)
    { if (P.Key != Except && !Before.ParameterStates.Contains(P.Key)) { return false; } }
    return true;
}
FString StableState(const FSnapshot& S)
{
    auto J = MakeShared<FJsonObject>(); J->Values = S.Json->Values; J->RemoveField(TEXT("compile_state")); return Text(J);
}

void Initialize(FResult& R, const FString& SystemPath, bool bSave = false)
{
    R.Data->SetStringField(TEXT("requested_system_path"), SystemPath);
    R.Data->SetBoolField(TEXT("asset_load_attempted"), false); R.Data->SetBoolField(TEXT("full_legacy_niagara_parity"), false);
    R.Data->SetArrayField(TEXT("mesh_asset_loads"), {});
    R.Data->SetBoolField(TEXT("mesh_asset_post_load_side_effects_uninstrumented"), false);
    R.Data->SetBoolField(TEXT("explicit_mesh_property_set_api_invoked"), false);
    R.Data->SetBoolField(TEXT("mesh_property_notifications_requested"), false);
    R.Data->SetBoolField(TEXT("compile_requested"), false); R.Data->SetBoolField(TEXT("compile_attempted"), false);
    R.Data->SetField(TEXT("compile_request_returned"), Null());
    R.Data->SetBoolField(TEXT("save_requested"), bSave); R.Data->SetBoolField(TEXT("save_attempted"), false);
    R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetBoolField(TEXT("save_deferred"), false);
    R.Data->SetBoolField(TEXT("blocking_wait_invoked"), false);
    R.Data->SetBoolField(TEXT("explicit_post_edit_change_api_invoked"), false);
    R.Data->SetBoolField(TEXT("explicit_user_script_variable_metadata_api_invoked"), false);
    R.Data->SetBoolField(TEXT("implicit_store_notification_callbacks_possible"), true);
    R.Data->SetBoolField(TEXT("metadata_callback_effect_observed"), false);
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("auto_retry_safe"), false);
    R.Data->SetStringField(TEXT("reference_audit_scope"), TEXT("direct typed value/class; recursive asset dependency audit belongs to caller"));
}
UNiagaraSystem* Resolve(FResult& R, const FString& Input, bool bWrite, bool bAllowSourceEdit)
{
    const FEngineVersion& Version = FEngineVersion::Current();
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("niagara_editor_state_invalid"), TEXT("Requires Editor game thread outside PIE/SIE.")); return nullptr; }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8 || Version.GetPatch() != 3) { Fail(R, TEXT("niagara_version_unsupported"), TEXT("Only UE 5.8.3 is implemented.")); return nullptr; }
    FString ObjectPath = Input.TrimStartAndEnd();
    if (!ObjectPath.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(ObjectPath))
    { ObjectPath += TEXT(".") + FPackageName::GetLongPackageAssetName(ObjectPath); }
    if (!FPackageName::IsValidObjectPath(ObjectPath)) { Fail(R, TEXT("niagara_system_path_invalid"), TEXT("Expected an exact system asset/package path.")); return nullptr; }
    UNiagaraSystem* System = FindObject<UNiagaraSystem>(nullptr, *ObjectPath);
    if (!Loaded(System) || System->GetClass() != UNiagaraSystem::StaticClass() || !System->IsAsset()
        || System->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn)
        || System->GetExposedParameters().GetOwner() != System)
    { Fail(R, TEXT("niagara_loaded_system_required"), TEXT("Load the exact native NiagaraSystem asset with official tools before this find-only operation.")); return nullptr; }
    const FString Package = System->GetOutermost()->GetName();
    const bool bTemporary = Package.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/")) || Package.StartsWith(TEXT("/Game/CubelessMCPTests/"));
    R.Data->SetStringField(TEXT("system_path"), Path(System));
    R.Data->SetStringField(TEXT("write_scope"), bTemporary ? TEXT("isolated_generated") : TEXT("source"));
    R.Data->SetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (bWrite && (!Package.StartsWith(TEXT("/Game/")) || (!bTemporary && !bAllowSourceEdit)))
    { Fail(R, TEXT("niagara_write_scope_forbidden"), TEXT("Writes require the legacy generated root/current isolated test root, or explicit source edit on /Game.")); return nullptr; }
    if (bWrite)
    {
        TArray<TSharedPtr<FJsonValue>> Instances;
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            UNiagaraComponent* C = *It;
            if (IsValid(C) && !C->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && C->GetAsset() == System && C->GetWorld())
            { Instances.Add(MakeShared<FJsonValueString>(Path(C))); if (Instances.Num() > 32) { break; } }
        }
        R.Data->SetArrayField(TEXT("loaded_world_component_references"), Instances);
        if (!Instances.IsEmpty()) { Fail(R, TEXT("niagara_system_has_world_instances"), TEXT("Finite saved-default authoring refuses systems with live world components; compile/rebind may update them.")); return nullptr; }
    }
    return System;
}

struct FPrepared
{
    EKind K = EKind::Unsupported; double N[4] = {0, 0, 0, 1}; int32 I = 0; bool B = false;
    UObject* Object = nullptr; TSharedPtr<FJsonValue> Expected = Null();
    TArray<FNiagaraMeshRendererMeshPropertiesBase> Meshes;
    TArray<TStrongObjectPtr<UStaticMesh>> KeepMeshes;
    TArray<TSharedPtr<FJsonValue>> MeshSources;
};
bool FloatNumber(const TSharedPtr<FJsonValue>& V, double& Out, bool bPosition = false)
{
    if (!V.IsValid() || V->Type != EJson::Number || !V->TryGetNumber(Out) || !FMath::IsFinite(Out)
        || FMath::Abs(Out) > static_cast<double>(TNumericLimits<float>::Max())) { return false; }
    if (!bPosition) { Out = static_cast<float>(Out); } return true;
}
bool MeshTriple(const TSharedPtr<FJsonValue>& V, double* Out)
{
    if (!V.IsValid() || V->Type != EJson::Array || V->AsArray().Num() != 3) { return false; }
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!V->AsArray()[Index].IsValid() || V->AsArray()[Index]->Type != EJson::Number
            || !V->AsArray()[Index]->TryGetNumber(Out[Index]) || !FMath::IsFinite(Out[Index])) { return false; }
    }
    return true;
}
bool PrepareMesh(FResult& R, const FNiagaraTypeDefinition& T, const TSharedPtr<FJsonValue>& V, FPrepared& P)
{
    FArrayProperty* Property = nullptr;
    if (!MeshProperty(T.GetClass(), Property))
    { return Fail(R, TEXT("niagara_mesh_property_contract_unsupported"), TEXT("Exact loaded public ArrayMesh/MeshData/five editable native struct fields are required; no Internal API or arbitrary property route.")); }
    R.Data->SetObjectField(TEXT("mesh_native_property_contract"), MeshContract(Property));
    if (V->Type != EJson::Array || V->AsArray().Num() > MaxParameters)
    { return Fail(R, TEXT("niagara_mesh_array_value_invalid"), TEXT("Mesh value requires an ordered JSON array of at most 512 asset strings or mesh objects; explicit [] clears and null is not omission.")); }
    TArray<FString> Paths; int32 IgnoredFields = 0;
    for (const auto& Item : V->AsArray())
    {
        if (!Item.IsValid()) { return Fail(R, TEXT("niagara_mesh_array_entry_invalid"), TEXT("Every Mesh entry requires an asset path string or object, never null.")); }
        FNiagaraMeshRendererMeshPropertiesBase Mesh; FString AssetPath;
        if (Item->Type == EJson::String) { AssetPath = Item->AsString(); }
        else if (Item->Type == EJson::Object)
        {
            const auto& Object = Item->AsObject();
            if (!Object.IsValid() || (!Object->TryGetStringField(TEXT("mesh"), AssetPath)
                && !Object->TryGetStringField(TEXT("mesh_path"), AssetPath) && !Object->TryGetStringField(TEXT("static_mesh"), AssetPath)))
            { return Fail(R, TEXT("niagara_mesh_array_entry_invalid"), TEXT("Mesh object requires mesh/mesh_path/static_mesh string; first string alias has legacy precedence.")); }
            for (const auto& Field : {TEXT("scale"), TEXT("rotation"), TEXT("pivot_offset")})
            {
                const auto Value = Object->TryGetField(Field);
                // Preserve the original TryGetArrayField behavior: absent or
                // non-array optional fields retain defaults, not a new coercion.
                if (!Value.IsValid() || Value->Type != EJson::Array)
                { if (Value.IsValid()) { ++IgnoredFields; } continue; }
                double N[3]; if (!MeshTriple(Value, N))
                { return Fail(R, TEXT("niagara_mesh_array_vector_invalid"), TEXT("Present array Scale/Rotation/PivotOffset fields require exactly three finite numbers; Rotation uses pitch,yaw,roll.")); }
                if (FCString::Strcmp(Field, TEXT("scale")) == 0) { Mesh.Scale = FVector(N[0], N[1], N[2]); }
                else if (FCString::Strcmp(Field, TEXT("rotation")) == 0) { Mesh.Rotation = FRotator(N[0], N[1], N[2]); }
                else { Mesh.PivotOffset = FVector(N[0], N[1], N[2]); }
            }
            const auto Space = Object->TryGetField(TEXT("pivot_offset_space"));
            if (Space.IsValid())
            {
                bool Matched = false;
                for (const auto Candidate : {ENiagaraMeshPivotOffsetSpace::Mesh, ENiagaraMeshPivotOffsetSpace::Simulation,
                    ENiagaraMeshPivotOffsetSpace::World, ENiagaraMeshPivotOffsetSpace::Local})
                {
                    if ((Space->Type == EJson::String && Space->AsString().TrimStartAndEnd().Equals(MeshSpaceName(Candidate), ESearchCase::IgnoreCase))
                        || (Space->Type == EJson::Number && FMath::IsFinite(Space->AsNumber())
                            && Space->AsNumber() == static_cast<double>(static_cast<uint8>(Candidate))))
                    { Mesh.PivotOffsetSpace = Candidate; Matched = true; break; }
                }
                if (!Matched) { return Fail(R, TEXT("niagara_mesh_pivot_space_invalid"), TEXT("PivotOffsetSpace requires Mesh/Simulation/World/Local or an exact finite integer enum value; null/fraction/unknown value is rejected.")); }
            }
            const TSet<FString> Known{TEXT("mesh"), TEXT("mesh_path"), TEXT("static_mesh"), TEXT("scale"), TEXT("rotation"), TEXT("pivot_offset"), TEXT("pivot_offset_space")};
            for (const auto& Field : Object->Values) { if (!Known.Contains(FString(Field.Key.ToView()))) { ++IgnoredFields; } }
        }
        else { return Fail(R, TEXT("niagara_mesh_array_entry_invalid"), TEXT("Every Mesh entry requires an asset path string or object, never null/number/bool/array.")); }
        AssetPath = FPackageName::ExportTextPathToObjectPath(AssetPath).TrimStartAndEnd(); AssetPath.TrimQuotesInline();
        if (!AssetPath.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(AssetPath))
        { AssetPath += TEXT(".") + FPackageName::GetLongPackageAssetName(AssetPath); }
        if (AssetPath.Len() > 1023 || !FPackageName::IsValidObjectPath(AssetPath) || AssetPath.Contains(TEXT(":"))
            || (!AssetPath.StartsWith(TEXT("/Game/")) && !AssetPath.StartsWith(TEXT("/Engine/"))))
        { return Fail(R, TEXT("niagara_mesh_asset_path_forbidden"), TEXT("Mesh references require bounded exact Game/Engine StaticMesh asset/package paths; subobjects, Script, plugin/MCP and transient paths are forbidden.")); }
        Paths.Add(AssetPath); P.Meshes.Add(Mesh);
    }
    R.Data->SetNumberField(TEXT("mesh_legacy_ignored_optional_field_count"), IgnoredFields);
    for (int32 Index = 0; Index < Paths.Num(); ++Index)
    {
        UObject* Existing = FindObject<UObject>(nullptr, *Paths[Index]);
        if (Existing && Existing->GetClass() != UStaticMesh::StaticClass())
        { return Fail(R, TEXT("niagara_mesh_asset_class_invalid"), TEXT("A Mesh path resolves to a different loaded native class; no declaration/write was dispatched.")); }
        UStaticMesh* Mesh = Cast<UStaticMesh>(Existing);
        if (!Loaded(Mesh))
        {
            auto Load = MakeShared<FJsonObject>(); Load->SetStringField(TEXT("requested_object_path"), Paths[Index]);
            Load->SetBoolField(TEXT("load_attempted"), true); Load->SetBoolField(TEXT("already_present"), Existing != nullptr);
            R.Data->SetBoolField(TEXT("asset_load_attempted"), true);
            R.Data->SetBoolField(TEXT("mesh_asset_post_load_side_effects_uninstrumented"), true);
            Mesh = LoadObject<UStaticMesh>(nullptr, *Paths[Index]);
            Load->SetStringField(TEXT("returned_object_path"), Path(Mesh)); Load->SetObjectField(TEXT("after"), MeshAssetState(Mesh));
            auto Loads = R.Data->GetArrayField(TEXT("mesh_asset_loads")); Loads.Add(MakeShared<FJsonValueObject>(Load));
            R.Data->SetArrayField(TEXT("mesh_asset_loads"), Loads);
        }
        if (!MeshAsset(Mesh) || Path(Mesh) != Paths[Index])
        { return Fail(R, TEXT("niagara_mesh_asset_class_or_state_invalid"), TEXT("Every loaded Mesh must be the exact runtime native StaticMesh asset with a completed load; no declaration/write was dispatched.")); }
        P.KeepMeshes.Emplace(Mesh); P.MeshSources.Add(MakeShared<FJsonValueObject>(MeshAssetState(Mesh))); P.Meshes[Index].Mesh = Mesh;
    }
    if (!MeshValues(P.Meshes, P.Expected))
    { return Fail(R, TEXT("niagara_mesh_payload_bound_exceeded"), TEXT("Canonical ordered five-field Mesh payload exceeds the 65536-character observation bound.")); }
    return true;
}
bool Prepare(FResult& R, const FNiagaraTypeDefinition& T, const TSharedPtr<FJsonValue>& V, FPrepared& P)
{
    P.K = Kind(T);
    // YM: original user store setters consume the SDK JSON conversion contract,
    // not exact JSON types. Keep finite/storage bounds before native narrowing.
    const auto UserNumber=[](const TSharedPtr<FJsonValue>& Value,double& Out,bool Position=false)
    {
        if(!Value.IsValid()||!Value->TryGetNumber(Out)||!FMath::IsFinite(Out)
            ||FMath::Abs(Out)>static_cast<double>(TNumericLimits<float>::Max()))return false;
        if(!Position)Out=static_cast<float>(Out);return true;
    };
    if (!V.IsValid() || P.K == EKind::Unsupported) { return Fail(R, TEXT("niagara_type_unsupported"), TEXT("Literal values support finite primitives, Position, UObject and MaterialInterface; arbitrary types remain incomplete.")); }
    if (P.K == EKind::ArrayDI)
    {
        if (CanonicalType(T) == TEXT("arraymesh")) { return PrepareMesh(R, T, V, P); }
        return Fail(R, TEXT("niagara_array_di_value_unsupported"),
            TEXT("Numeric Array-DI classes support omitted-value declaration/reuse only. Explicit value/default_value, including null or [], is rejected before declaration; component runtime overrides are a separate Engine API contract."));
    }
    if (P.K == EKind::Object || P.K == EKind::Material)
    {
        if (V->Type == EJson::Null) { P.Expected = Null(); return true; }
        FString AssetPath;
        if (V->Type != EJson::String || !V->TryGetString(AssetPath) || !FPackageName::IsValidObjectPath(AssetPath))
        { return Fail(R, TEXT("niagara_object_value_invalid"), TEXT("Object values require explicit JSON null or an exact loaded asset object path.")); }
        P.Object = FindObject<UObject>(nullptr, *AssetPath);
        if (!Loaded(P.Object) || !P.Object->IsAsset() || !P.Object->IsA(T.GetClass()) || P.Object->IsEditorOnly()
            || !RuntimeClass(P.Object->GetClass()) || P.Object->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor)
            || (!AssetPath.StartsWith(TEXT("/Game/")) && !AssetPath.StartsWith(TEXT("/Engine/"))))
        { return Fail(R, TEXT("niagara_object_class_or_reference_forbidden"), TEXT("The loaded asset must match the exact finite native runtime class; Editor/Cubeless/MCP references are not permitted.")); }
        P.Expected = MakeShared<FJsonValueString>(Path(P.Object)); return true;
    }
    if (P.K == EKind::Bool)
    {
        if (!V->TryGetBool(P.B)) { return Fail(R, TEXT("niagara_bool_value_invalid"), TEXT("Bool requires an SDK TryGetBool-convertible JSON value.")); }
        P.Expected = MakeShared<FJsonValueBoolean>(P.B); return true;
    }
    if (P.K == EKind::Int)
    {
        double X = 0;
        if (!V->TryGetNumber(X) || !FMath::IsFinite(X))
        { return Fail(R, TEXT("niagara_int_value_invalid"), TEXT("Int requires a finite SDK TryGetNumber-convertible value.")); }
        const double Truncated=FMath::TruncToDouble(X);
        if(Truncated<MIN_int32||Truncated>MAX_int32)
        { return Fail(R, TEXT("niagara_int_value_invalid"), TEXT("Original truncation must fit int32 storage; unsafe narrowing is rejected before mutation.")); }
        P.I = static_cast<int32>(X); P.Expected = Number(P.I); return true;
    }
    if (P.K == EKind::Float)
    {
        if (!UserNumber(V, P.N[0])) { return Fail(R, TEXT("niagara_float_value_invalid"), TEXT("Float requires a finite float32-representable SDK TryGetNumber value.")); }
        P.Expected = Number(P.N[0]); return true;
    }
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    const int32 Count = P.K == EKind::Vec2 ? 2 : P.K == EKind::Vec4 || P.K == EKind::Color ? 4 : 3;
    const int32 Minimum=P.K==EKind::Color?3:Count;
    if (V->Type != EJson::Array || !V->TryGetArray(Items) || !Items || Items->Num()<Minimum)
    { return Fail(R, TEXT("niagara_vector_value_invalid"), TEXT("Numeric arrays require at least the type arity (Color accepts RGB with alpha 1); object-form is not an original implemented native contract.")); }
    // JsonArrayToDoubles validates every item, including unused trailing values.
    // Do not silently ignore an invalid extra item or narrow unused numbers.
    for(const auto& Item:*Items)
    {
        double Ignored=0.;if(!Item.IsValid()||!Item->TryGetNumber(Ignored)||!FMath::IsFinite(Ignored))
            return Fail(R,TEXT("niagara_vector_value_invalid"),TEXT("Every array item, including unused trailing values, requires a finite SDK numeric conversion."));
    }
    TArray<TSharedPtr<FJsonValue>> Expected;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Index < Items->Num() && !UserNumber((*Items)[Index], P.N[Index], P.K == EKind::Position))
        { return Fail(R, TEXT("niagara_vector_value_invalid"), TEXT("Components must be finite and float32-representable; Position retains original world double precision within representable raw range.")); }
        Expected.Add(Number(P.N[Index]));
    }
    P.Expected = MakeShared<FJsonValueArray>(Expected); return true;
}
bool Write(FNiagaraParameterStore& Store, const FNiagaraVariable& V, const FPrepared& P, FResult& R,
    const TSharedRef<FJsonObject>& Row)
{
    const int32 Offset = Store.IndexOf(V), Size = V.GetSizeInBytes();
    if (P.K == EKind::ArrayDI)
    {
        if (CanonicalType(V.GetType()) != TEXT("arraymesh") || !Store.GetDataInterfaces().IsValidIndex(Offset)) { return false; }
        UNiagaraDataInterface* DI = Store.GetDataInterface(Offset); FArrayProperty* Property = nullptr;
        if (!Loaded(DI) || DI->GetClass() != V.GetType().GetClass() || DI->GetOuter() != Store.GetOwner()
            || DI->GetOutermost() != Store.GetOwner()->GetOutermost() || DI->IsTemplate() || DI->IsEditorOnly()
            || !MeshProperty(DI->GetClass(), Property)) { return false; }
        TStrongObjectPtr<UNiagaraDataInterface> Keep(DI); const FString Identity = Path(DI);
        Row->SetArrayField(TEXT("mesh_source_assets_before_property_set"), P.MeshSources);
        Row->SetStringField(TEXT("mesh_data_property_path"), Property->GetPathName());
        Row->SetNumberField(TEXT("mesh_payload_count"), P.Meshes.Num());
        R.Data->SetBoolField(TEXT("explicit_mesh_property_set_api_invoked"), true);
        R.Data->SetBoolField(TEXT("mesh_property_notifications_requested"), true);
        Row->SetBoolField(TEXT("mesh_property_set_attempted"), true);
        DI->Modify();
        const auto Result = PropertyAccessUtil::SetPropertyValue_Object(Property, DI, Property, &P.Meshes,
            INDEX_NONE, PropertyAccessUtil::EditorReadOnlyFlags, EPropertyAccessChangeNotifyMode::Always);
        Row->SetStringField(TEXT("mesh_property_set_result_flags_string"), LexToString(static_cast<uint8>(Result)));
        Row->SetBoolField(TEXT("mesh_property_set_returned_success"), Result == EPropertyAccessResultFlags::Success);
        Row->SetBoolField(TEXT("mesh_property_notifications_requested"), true);
        TArray<TSharedPtr<FJsonValue>> Sources;
        for (const auto& Mesh : P.Meshes) { Sources.Add(MakeShared<FJsonValueObject>(MeshAssetState(Mesh.Mesh.Get()))); }
        Row->SetArrayField(TEXT("mesh_source_assets_after_property_set"), Sources);
        const bool SourcesPreserved = ValueText(MakeShared<FJsonValueArray>(P.MeshSources)) == ValueText(MakeShared<FJsonValueArray>(Sources));
        Row->SetBoolField(TEXT("mesh_source_asset_identity_package_state_preserved"), SourcesPreserved);
        const bool IdentityPreserved = Store.GetDataInterface(Offset) == DI && DI->GetClass() == V.GetType().GetClass()
            && Path(DI) == Identity && DI->GetOuter() == Store.GetOwner() && DI->GetOutermost() == Store.GetOwner()->GetOutermost();
        Row->SetBoolField(TEXT("mesh_data_interface_identity_preserved"), IdentityPreserved);
        return Result == EPropertyAccessResultFlags::Success && SourcesPreserved && IdentityPreserved;
    }
    if (P.K == EKind::Object || P.K == EKind::Material)
    {
        if (!Store.GetUObjects().IsValidIndex(Offset)) { return false; }
        Store.SetUObject(TObjectPtr<UObject>(P.Object), Offset); return true;
    }
    if (Offset < 0 || Size <= 0 || Offset > Store.GetParameterDataArray().Num() - Size || Kind(V.GetType()) != P.K) { return false; }
    switch (P.K)
    {
    case EKind::Float: return Store.SetParameterValue(static_cast<float>(P.N[0]), V, false);
    case EKind::Int: return Store.SetParameterValue(P.I, V, false);
    case EKind::Bool: return Store.SetParameterValue(FNiagaraBool(P.B), V, false);
    case EKind::Vec2: return Store.SetParameterValue(FVector2f(P.N[0], P.N[1]), V, false);
    case EKind::Vec3: return Store.SetParameterValue(FVector3f(P.N[0], P.N[1], P.N[2]), V, false);
    case EKind::Vec4: return Store.SetParameterValue(FVector4f(P.N[0], P.N[1], P.N[2], P.N[3]), V, false);
    case EKind::Color: return Store.SetParameterValue(FLinearColor(P.N[0], P.N[1], P.N[2], P.N[3]), V, false);
    case EKind::Position: return SetPosition(Store, FVector(P.N[0], P.N[1], P.N[2]), V.GetName());
    default: return false;
    }
}
bool ParseJson(const FString& Input, TSharedPtr<FJsonValue>& Out)
{
    // JsonSerializer's root object/array state does not accept a standalone
    // null/number/string. One array wrapper preserves every JSON value type.
    TArray<TSharedPtr<FJsonValue>> Values;
    if (Input.Len() > 1048576 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Input + TEXT("]")), Values)
        || Values.Num() != 1 || !Values[0].IsValid()) { return false; }
    Out = Values[0]; return true;
}
TSharedPtr<FJsonValue> Alias(const TSharedRef<FJsonObject>& Row, const TCHAR* First, const TCHAR* Second)
{ if (Row->HasField(First)) { return Row->TryGetField(First); } return Row->TryGetField(Second); }

void Finish(FResult& R, UNiagaraSystem* System, const FSnapshot& Before, bool bHadWrites,
    bool bRequestCompile, bool bSave, int32 Failed)
{
    const FSnapshot BeforeFinalize = Capture(System);
    if (!BeforeFinalize.bComplete)
    {
        R.bChangeStateKnown = false;
        Fail(R, TEXT("niagara_finalize_before_unobservable"), TEXT("Pre-compile/save store observation is incomplete; no further compile/save dispatched."));
    }
    if (bHadWrites && R.bChangeStateKnown)
    {
        if (bRequestCompile)
        {
            R.Stage = TEXT("request_compile"); R.Data->SetBoolField(TEXT("compile_requested"), true);
            R.Data->SetBoolField(TEXT("compile_attempted"), true); const bool Returned = System->RequestCompile(false);
            R.Data->SetBoolField(TEXT("compile_request_returned"), Returned);
        }
        if (bSave)
        {
            auto State = CompileState(System); R.Data->SetObjectField(TEXT("save_compile_state"), State);
            if (!State->GetBoolField(TEXT("ready_to_save")))
            {
                R.Stage = TEXT("save_deferred"); R.Data->SetBoolField(TEXT("save_deferred"), true);
                Fail(R, TEXT("niagara_save_deferred_compile_pending"), TEXT("Written parameters remain. Compilation is outstanding/required; saving would block PreSave. Poll the read-only inspector and explicitly save the exact asset when ready; do not replay writes."));
            }
            else
            {
                R.Stage = TEXT("save"); R.Data->SetBoolField(TEXT("save_attempted"), true);
                const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(System, false); R.Data->SetBoolField(TEXT("saved"), Saved);
                if (!Saved) { Fail(R, TEXT("niagara_save_failed"), TEXT("Exact save returned false; written parameters remain and no rollback/retry was performed.")); }
                else if (System->GetOutermost()->IsDirty()) { Fail(R, TEXT("niagara_save_postcondition_failed"), TEXT("Save returned true but the exact package remains dirty.")); }
            }
        }
    }
    const FSnapshot After = Capture(System); R.Data->SetObjectField(TEXT("after"), After.Json);
    R.Data->SetObjectField(TEXT("compile_state"), CompileState(System));
    R.Data->SetArrayField(TEXT("parameters"), After.UserRows); R.Data->SetNumberField(TEXT("parameter_count"), After.UserRows.Num());
    if (!After.bComplete)
    {
        R.bChangeStateKnown = false; R.bPostconditionPassed = false; R.Stage = TEXT("observe_after");
        Fail(R, TEXT("niagara_after_state_unobservable"), TEXT("A mutation boundary completed but bounded store observation is incomplete; no automatic retry."));
    }
    else
    {
        R.bChanged = StableState(Before) != StableState(After);
        R.Data->SetBoolField(TEXT("values_changed"), Before.ParameterStates.OrderIndependentCompareEqual(After.ParameterStates) == false);
        const bool ParametersPreserved = BeforeFinalize.bComplete
            && BeforeFinalize.ParameterStates.OrderIndependentCompareEqual(After.ParameterStates);
        R.Data->SetBoolField(TEXT("parameters_preserved_across_compile_save"), ParametersPreserved);
        if (R.bChangeStateKnown && !ParametersPreserved)
        { R.Stage = TEXT("observe_after"); Fail(R, TEXT("niagara_compile_save_parameter_changed"), TEXT("A parameter changed across the compile/save boundary; observed state is retained.")); }
        R.bPostconditionPassed = R.bChangeStateKnown && R.ErrorCode.IsEmpty() && Failed == 0;
    }
    R.Data->SetBoolField(TEXT("compile_and_save_suppressed_unknown_outcome"), !R.bChangeStateKnown);
    R.bSuccess = R.bPostconditionPassed;
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
}

FResult Mutate(const FString& SystemPath, const TArray<TSharedPtr<FJsonValue>>& Rows, bool bSetOnly,
    bool bContinueOnError, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    FResult R; Initialize(R, SystemPath, bSave); R.Data->SetNumberField(TEXT("requested_count"), Rows.Num());
    R.Data->SetNumberField(TEXT("processed_count"), 0); R.Data->SetNumberField(TEXT("added_count"), 0);
    R.Data->SetNumberField(TEXT("updated_count"), 0); R.Data->SetNumberField(TEXT("failed_count"), 0);
    R.Data->SetNumberField(TEXT("data_interface_created_count"), 0);
    R.Data->SetNumberField(TEXT("store_initialized_data_interface_count"), 0);
    R.Data->SetNumberField(TEXT("observed_data_interface_creation_count"), 0);
    R.Data->SetStringField(TEXT("data_interface_created_accounting"), TEXT("legacy manual fallback only; AddParameter implicit initialization is reported separately, no manual fallback is dispatched"));
    R.Data->SetArrayField(TEXT("results"), {});
    R.Data->SetBoolField(TEXT("continue_on_error"), bContinueOnError); R.Data->SetBoolField(TEXT("request_compile"), bRequestCompile);
    R.Data->SetBoolField(TEXT("equal_value_writes_preserved"), true); R.Data->SetBoolField(TEXT("new_value_prevalidated_before_declaration"), true);
    int32 Processed = 0, Added = 0, Updated = 0, Failed = 0, StoreInitializedDI = 0; bool HadWrites = false;
    TArray<TSharedPtr<FJsonValue>> Results;
    UNiagaraSystem* System = Resolve(R, SystemPath, true, bAllowSourceEdit); if (!System) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(System); const FSnapshot Before = Capture(System);
    R.Data->SetObjectField(TEXT("before"), Before.Json);
    if (!Before.bComplete) { Fail(R, TEXT("niagara_before_state_unobservable"), TEXT("Bounded actual store/redirect observation is incomplete; no mutation dispatched.")); return R; }
    auto& Store = System->GetExposedParameters();
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        ++Processed; auto RowResult = MakeShared<FJsonObject>(); RowResult->SetNumberField(TEXT("index"), Index);
        RowResult->SetBoolField(TEXT("success"), false); RowResult->SetBoolField(TEXT("created"), false);
        RowResult->SetBoolField(TEXT("value_set"), false); RowResult->SetBoolField(TEXT("data_interface_created"), false);
        RowResult->SetBoolField(TEXT("store_initialized_data_interface"), false);
        RowResult->SetBoolField(TEXT("observed_data_interface_creation"), false);
        RowResult->SetBoolField(TEXT("data_interface_reused"), false);
        RowResult->SetBoolField(TEXT("no_op"), false); RowResult->SetBoolField(TEXT("mutation_attempted"), false);
        RowResult->SetBoolField(TEXT("change_state_known"), true); R.ErrorCode.Empty(); R.Error.Empty(); R.Stage = TEXT("row_preflight");
        FSnapshot RowBefore = Capture(System); FString Name; FNiagaraVariable Variable; FPrepared Prepared;
        bool Created = false, ValuePresent = false, RowOk = true;
        TSharedPtr<FJsonValue> Value; const TSharedPtr<FJsonObject> Row = Rows[Index].IsValid() && Rows[Index]->Type == EJson::Object ? Rows[Index]->AsObject() : nullptr;
        if (!Row.IsValid()) { RowOk = Fail(R, TEXT("niagara_parameter_row_invalid"), TEXT("Every ordered row must be a JSON object.")); }
        else if (!RowBefore.bComplete) { RowOk = Fail(R, TEXT("niagara_row_before_unobservable"), TEXT("Current store observation is incomplete; no next row mutation dispatched.")); }
        else
        {
            const auto NameValue = Alias(Row.ToSharedRef(), TEXT("name"), TEXT("parameter_name")); FString RawName;
            if (!NameValue.IsValid() || NameValue->Type != EJson::String || !NameValue->TryGetString(RawName) || !CanonicalName(RawName, Name))
            { RowOk = Fail(R, TEXT("niagara_parameter_name_invalid"), TEXT("A finite nonempty bare/User-prefixed parameter name is required.")); }
            else
            {
                const FNiagaraVariable* Existing = RowBefore.Users.Find(Name.ToLower());
                if (Existing) { Variable = *Existing; Name = Existing->GetName().ToString(); }
                FNiagaraTypeDefinition Type;
                if (bSetOnly)
                {
                    if (!Existing) { RowOk = Fail(R, TEXT("niagara_parameter_not_found"), TEXT("Set is existing-only; no declaration was created.")); }
                    else { Type = Existing->GetType(); if (Kind(Type) == EKind::Unsupported) { RowOk = Fail(R, TEXT("niagara_type_unsupported"), TEXT("Existing parameter type is outside the finite cohort.")); } }
                }
                else
                {
                    const auto TypeValue = Alias(Row.ToSharedRef(), TEXT("type"), TEXT("type_name")); FString TypeName;
                    if (!TypeValue.IsValid() || TypeValue->Type != EJson::String || !TypeValue->TryGetString(TypeName) || !ParseType(TypeName, Type))
                    { RowOk = Fail(R, TEXT("niagara_type_unsupported"), TEXT("Every upsert row requires a supported finite primitive/object type or one of the six exact loaded native Array-DI classes; arbitrary DI/array types remain incomplete.")); }
                    else if (Existing && Existing->GetType() != Type) { RowOk = Fail(R, TEXT("niagara_parameter_type_conflict"), TEXT("Existing canonical name has a different exact type; it was not converted.")); }
                }
                if (RowOk)
                {
                    Value = Alias(Row.ToSharedRef(), TEXT("value"), TEXT("default_value")); ValuePresent = Value.IsValid();
                    RowResult->SetBoolField(TEXT("value_present"), ValuePresent);
                    if (bSetOnly && !ValuePresent) { RowOk = Fail(R, TEXT("niagara_value_required"), TEXT("Set requires an explicit JSON value; null is distinct from omission.")); }
                    else if (ValuePresent && !Prepare(R, Type, Value, Prepared)) { RowOk = false; }
                    if (RowOk && !Existing)
                    {
                        Variable = FNiagaraVariable(Type, FName(*Name));
                        R.Stage = TEXT("add_parameter"); R.bMutationAttempted = true; RowResult->SetBoolField(TEXT("mutation_attempted"), true);
                        System->Modify(); int32 Offset = INDEX_NONE; const bool Returned = Add(Store, Variable, Offset);
                        RowResult->SetBoolField(TEXT("add_returned"), Returned); RowResult->SetNumberField(TEXT("add_offset"), Offset);
                        const FSnapshot Declared = Capture(System); const FNiagaraVariable* Actual = Declared.Users.Find(Name.ToLower());
                        Created = Actual && Actual->GetType() == Type;
                        if (Created) { ++Added; HadWrites = true; RowResult->SetBoolField(TEXT("created"), true); Variable = *Actual; }
                        if (!Declared.bComplete) { R.bChangeStateKnown = false; RowResult->SetBoolField(TEXT("change_state_known"), false); RowOk = Fail(R, TEXT("niagara_declaration_state_unobservable"), TEXT("Declaration was dispatched but its actual state cannot be bounded; stop without retry.")); }
                        else if (!Returned || !Created || !PreservedExcept(RowBefore, Declared, Name.ToLower()))
                        { RowOk = Fail(R, TEXT("niagara_declaration_postcondition_failed"), TEXT("Declaration return/readback or unrelated-parameter preservation failed; any observed new declaration remains.")); }
                        if (RowOk && Kind(Type) == EKind::ArrayDI)
                        {
                            bool Empty = false;
                            for (const auto& StateValue : Declared.UserRows)
                            {
                                const auto State = StateValue->AsObject();
                                if (State.IsValid() && State->GetStringField(TEXT("name")) == Name)
                                { Empty = State->GetObjectField(TEXT("array_payload"))->GetBoolField(TEXT("all_empty")); break; }
                            }
                            RowResult->SetBoolField(TEXT("array_empty_after_declaration_verified"), Empty);
                            if (!Empty) { RowOk = Fail(R, TEXT("niagara_array_di_default_not_empty"), TEXT("The retained new exact Array-DI declaration is unexpectedly populated; no payload write follows.")); }
                        }
                    }
                    if (RowOk && ValuePresent)
                    {
                        TStrongObjectPtr<UObject> KeepObject(Prepared.Object);
                        R.Stage = TEXT("set_parameter_value"); R.bMutationAttempted = true; RowResult->SetBoolField(TEXT("mutation_attempted"), true);
                        System->Modify(); const bool Returned = Write(Store, Variable, Prepared, R, RowResult);
                        RowResult->SetBoolField(TEXT("write_returned"), Returned); TSharedPtr<FJsonValue> Actual;
                        const bool Read = ReadValue(Store, Variable, Actual);
                        const bool Verified = Read && ValueText(Actual) == ValueText(Prepared.Expected);
                        RowResult->SetBoolField(TEXT("value_readback_verified"), Verified);
                        if (Prepared.K == EKind::ArrayDI) { RowResult->SetBoolField(TEXT("mesh_payload_fields_verified"), Verified); }
                        if (Read) { RowResult->SetField(TEXT("value"), Actual); }
                        if (Returned && Verified) { RowResult->SetBoolField(TEXT("value_set"), true); if (!Created) { ++Updated; } HadWrites = true; }
                        else { RowOk = Fail(R, TEXT("niagara_value_postcondition_failed"), TEXT("Write returned false or exact offset-backed typed readback differs; prior changes remain.")); }
                    }
                    if (RowResult->GetBoolField(TEXT("mutation_attempted"))) { System->MarkPackageDirty(); }
                }
            }
        }
        const FSnapshot RowAfter = Capture(System);
        if (!RowAfter.bComplete)
        {
            R.bChangeStateKnown = false; RowResult->SetBoolField(TEXT("change_state_known"), false);
            RowOk = Fail(R, TEXT("niagara_row_after_unobservable"), TEXT("Actual store after a row is incomplete; no further mutation/compile/save dispatched."));
        }
        else if (RowBefore.bComplete && !PreservedExcept(RowBefore, RowAfter, Name.ToLower()))
        { RowOk = Fail(R, TEXT("niagara_unrelated_parameter_changed"), TEXT("An unrelated parameter's exact type/value/reference changed; partial state retained.")); }
        if (RowAfter.bComplete && Variable.IsValid() && Kind(Variable.GetType()) == EKind::ArrayDI)
        {
            for (const auto& V : RowAfter.UserRows)
            {
                const auto State = V->AsObject();
                if (!State.IsValid() || State->GetStringField(TEXT("name")) != Name) { continue; }
                const bool Identity = State->GetBoolField(TEXT("data_interface_identity_verified"));
                const bool Empty = State->GetObjectField(TEXT("array_payload"))->GetBoolField(TEXT("all_empty"));
                RowResult->SetBoolField(TEXT("data_interface_identity_verified"), Identity);
                RowResult->SetStringField(TEXT("data_interface_class"), State->GetStringField(TEXT("reference_class")));
                RowResult->SetStringField(TEXT("data_interface_path"), State->GetStringField(TEXT("reference_path")));
                RowResult->SetStringField(TEXT("data_interface_owner_path"), State->GetStringField(TEXT("reference_owner_path")));
                RowResult->SetObjectField(TEXT("array_payload"), State->GetObjectField(TEXT("array_payload")));
                RowResult->SetBoolField(TEXT("array_default_empty_verified"), Empty);
                RowResult->SetBoolField(TEXT("array_payload_empty_after_row"), Empty);
                // Add(true) owns initialization. The legacy manual-fallback flag
                // deliberately remains false even when a new object is observed.
                RowResult->SetBoolField(TEXT("store_initialized_data_interface"), Created && Identity);
                RowResult->SetBoolField(TEXT("observed_data_interface_creation"), Created && Identity);
                RowResult->SetBoolField(TEXT("data_interface_reused"), !Created && Identity);
                if (Created && Identity) { ++StoreInitializedDI; }
                if (Created && !ValuePresent && !Empty)
                { RowOk = Fail(R, TEXT("niagara_array_di_default_not_empty"), TEXT("New exact Array-DI declaration exists but its observed payload is not empty; retained known state, no rollback/retry.")); }
                break;
            }
        }
        RowResult->SetStringField(TEXT("parameter_name"), Name);
        if (Variable.IsValid()) { RowResult->SetStringField(TEXT("type"), Variable.GetType().GetName()); RowResult->SetStringField(TEXT("canonical_type"), CanonicalType(Variable.GetType())); }
        RowResult->SetBoolField(TEXT("success"), RowOk); RowResult->SetStringField(TEXT("stage"), R.Stage);
        RowResult->SetBoolField(TEXT("no_op"), RowOk && !Created && !ValuePresent);
        RowResult->SetStringField(TEXT("error_code"), R.ErrorCode); RowResult->SetStringField(TEXT("error"), R.Error);
        if (RowBefore.bComplete && RowAfter.bComplete)
        {
            RowResult->SetBoolField(TEXT("changed"), StableState(RowBefore) != StableState(RowAfter));
            RowResult->SetBoolField(TEXT("values_changed"), !RowBefore.ParameterStates.OrderIndependentCompareEqual(RowAfter.ParameterStates));
        }
        else { RowResult->SetField(TEXT("changed"), Null()); }
        Results.Add(MakeShared<FJsonValueObject>(RowResult));
        if (!RowOk) { ++Failed; if (!bContinueOnError || !R.bChangeStateKnown || RowResult->GetBoolField(TEXT("mutation_attempted"))) { break; } }
    }
    R.Data->SetNumberField(TEXT("processed_count"), Processed); R.Data->SetNumberField(TEXT("added_count"), Added);
    R.Data->SetNumberField(TEXT("updated_count"), Updated); R.Data->SetNumberField(TEXT("failed_count"), Failed);
    R.Data->SetNumberField(TEXT("store_initialized_data_interface_count"), StoreInitializedDI);
    R.Data->SetNumberField(TEXT("observed_data_interface_creation_count"), StoreInitializedDI);
    R.Data->SetArrayField(TEXT("results"), Results); R.Data->SetBoolField(TEXT("legacy_changed_work_flag"), HadWrites);
    if (Failed && R.ErrorCode.IsEmpty()) { Fail(R, TEXT("niagara_parameter_rows_failed"), TEXT("One or more known rows failed; see ordered results and retained prior modifications.")); }
    Finish(R, System, Before, HadWrites, bRequestCompile, bSave, Failed); return R;
}
}

Function::FResult Inspect(const FString& SystemPath)
{
    FResult R; Initialize(R, SystemPath); UNiagaraSystem* System = Resolve(R, SystemPath, false, false); if (!System) { return R; }
    TStrongObjectPtr<UNiagaraSystem> Keep(System); const FSnapshot Before = Capture(System), After = Capture(System);
    R.Data->SetObjectField(TEXT("before"), Before.Json); R.Data->SetObjectField(TEXT("after"), After.Json);
    R.Data->SetObjectField(TEXT("compile_state"), CompileState(System)); R.Data->SetArrayField(TEXT("parameters"), After.UserRows);
    R.Data->SetNumberField(TEXT("parameter_count"), After.UserRows.Num()); int32 Settable = 0;
    for (const auto& V : After.Users) { if (ValueSettable(V.Value.GetType())) { ++Settable; } }
    R.Data->SetNumberField(TEXT("settable_count"), Settable);
    R.bChangeStateKnown = Before.bComplete && After.bComplete;
    R.bChanged = R.bChangeStateKnown && StableState(Before) != StableState(After);
    R.Data->SetBoolField(TEXT("background_compile_state_changed"), Before.bComplete && After.bComplete
        && Text(Before.Json->GetObjectField(TEXT("compile_state")).ToSharedRef()) != Text(After.Json->GetObjectField(TEXT("compile_state")).ToSharedRef()));
    R.bPostconditionPassed = R.bChangeStateKnown && !R.bChanged;
    R.bSuccess = R.bPostconditionPassed; R.Stage = R.bSuccess ? TEXT("complete") : TEXT("observe");
    if (!R.bSuccess) { Fail(R, TEXT("niagara_inspection_not_preserved"), TEXT("Bounded store/package observation is incomplete or changed; no mutation was dispatched.")); }
    return R;
}
Function::FResult Set(const FString& SystemPath, const FString& ParameterName, const FString& ValueJson, bool bAllowSourceEdit, bool bSave)
{
    TSharedPtr<FJsonValue> Value;
    if (!ParseJson(ValueJson, Value)) { FResult R; Initialize(R, SystemPath, bSave); Fail(R, TEXT("niagara_value_json_invalid"), TEXT("Expected a bounded valid JSON value, including explicit null.")); return R; }
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), ParameterName); Row->SetField(TEXT("value"), Value);
    FResult R = Mutate(SystemPath, {MakeShared<FJsonValueObject>(Row)}, true, false, bAllowSourceEdit, bSave, true);
    const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
    if (R.Data->TryGetArrayField(TEXT("results"), Results) && Results && Results->Num() == 1)
    {
        auto Item = (*Results)[0]->AsObject();
        if (Item.IsValid()) { for (const TCHAR* Key : {TEXT("parameter_name"), TEXT("type"), TEXT("value")}) { if (Item->HasField(Key)) { R.Data->SetField(Key, Item->TryGetField(Key)); } } }
    }
    return R;
}
Function::FResult Upsert(const FString& SystemPath, const FString& ParametersJson, bool bContinueOnError,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    TSharedPtr<FJsonValue> Value; const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!ParseJson(ParametersJson, Value) || Value->Type != EJson::Array || !Value->TryGetArray(Rows) || !Rows || Rows->Num() > MaxRows)
    { FResult R; Initialize(R, SystemPath, bSave); Fail(R, TEXT("niagara_parameters_json_invalid"), TEXT("Expected an ordered JSON array of at most 128 rows; no mutation dispatched.")); return R; }
    return Mutate(SystemPath, *Rows, false, bContinueOnError, bAllowSourceEdit, bSave, bRequestCompile);
}
}
