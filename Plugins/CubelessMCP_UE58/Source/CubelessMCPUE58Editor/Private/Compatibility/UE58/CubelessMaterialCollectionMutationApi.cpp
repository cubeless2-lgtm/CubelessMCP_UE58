// UE56 MPC values/mirror contracts through public, Editor-only UE58 APIs.
#include "Compatibility/UE58/CubelessMaterialCollectionMutationApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::MaterialCollectionMutation
{
namespace
{
using FResult = Function::FResult;
constexpr int32 MaxParameters = 1024, MaxRequestedValues = 1024, MaxLoadedPackages = 4096;
constexpr int64 MaxDiskBytes = 8 * 1024 * 1024;

bool Fail(FResult& R, const TCHAR* Code, const FString& Message)
{ R.bSuccess = false; R.ErrorCode = Code; R.Error = Message; return false; }
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects); }
bool ColorFinite(const FLinearColor& C)
{ return FMath::IsFinite(C.R) && FMath::IsFinite(C.G) && FMath::IsFinite(C.B) && FMath::IsFinite(C.A); }
TSharedRef<FJsonObject> Color(const FLinearColor& C)
{
    auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("r"), C.R); J->SetNumberField(TEXT("g"), C.G);
    J->SetNumberField(TEXT("b"), C.B); J->SetNumberField(TEXT("a"), C.A); return J;
}
FString Json(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool NameValid(const FString& S)
{
    if (S.IsEmpty() || S.Len() > 128 || FName(*S).IsNone()) { return false; }
    for (const TCHAR C : S) { if (C < 32 || C == 127) { return false; } } return true;
}
void Init(FResult& R, bool bSave)
{
    R.Data->SetBoolField(TEXT("save_requested"), bSave); R.Data->SetBoolField(TEXT("save_attempted"), false);
    R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetBoolField(TEXT("loaded_during_operation"), false);
    R.Data->SetBoolField(TEXT("asset_load_dispatched"), false); R.Data->SetBoolField(TEXT("explicit_compile_requested"), false);
    R.Data->SetBoolField(TEXT("post_edit_change_invoked"), false); R.Data->SetBoolField(TEXT("implicit_engine_work_measured"), false);
    R.Data->SetBoolField(TEXT("rendered_runtime_verified"), false); R.Data->SetBoolField(TEXT("runtime_defaults_inheritance_supported"), false);
    R.Data->SetBoolField(TEXT("runtime_instance_lookup_dispatched"), false);
    R.Data->SetNumberField(TEXT("runtime_instance_lookup_count"), 0);
    R.Data->SetBoolField(TEXT("runtime_lazy_creation_possible"), false);
    R.Data->SetStringField(TEXT("changed_observation_scope"), TEXT("selected MPC declarations/effective getter values/dirty/disk after the initial optional runtime-instance lookup; lazy world allocation/render work is not instrumented"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("already loaded exact base-free native MPC; local defaults/name/ID arrays and optional Editor-world instance API; its public world lookup may lazily create an instance; no PIE, inherited overrides, ticker, rendered-output proof or asset loading"));
}
bool EditorState(FResult& R)
{
    return (IsEditorGameThread() && !IsPlaySessionActive() && GEditor)
        || Fail(R, TEXT("material_collection_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE."));
}
UMaterialParameterCollection* Resolve(const FString& Path, bool bTarget, bool bSave, FResult& R)
{
    FString PackageName, ObjectName;
    if (!FPackageName::IsValidObjectPath(Path) || !Path.Split(TEXT("."), &PackageName, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
        || ObjectName.IsEmpty() || !(PackageName.StartsWith(TEXT("/Game/")) || (!bTarget && PackageName.StartsWith(TEXT("/Engine/")))))
    { Fail(R, TEXT("material_collection_exact_loaded_path_required"), TEXT("Use an exact already loaded /Game MPC object path; mirror sources may also use /Engine. Names, auto-load and other mounts remain pending.")); return nullptr; }
    auto* C = FindObject<UMaterialParameterCollection>(nullptr, *Path);
    UClass* Class = UMaterialParameterCollection::StaticClass();
    if (!Loaded(C) || C->GetPathName() != Path || C->GetClass() != Class || !Loaded(Class)
        || Class->GetPathName() != TEXT("/Script/Engine.MaterialParameterCollection") || !Class->HasAnyClassFlags(CLASS_Native)
        || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || Class->IsEditorOnly()
        || Class->GetOutermost()->GetName() != TEXT("/Script/Engine") || !Class->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)
        || C->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) || C->IsEditorOnly()
        || C->GetOuter() != C->GetOutermost() || C->GetOutermost()->GetName() != PackageName
        || C->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn))
    { Fail(R, TEXT("material_collection_loaded_native_target_required"), TEXT("Requires exact loaded idle native runtime MPC/class/package identity; no inherited class or transient asset.")); return nullptr; }
    if (C->GetBaseParameterCollection())
    { Fail(R, TEXT("material_collection_inherited_write_pending"), TEXT("MPC base inheritance/effective overrides are outside this local-declaration mutation cohort; no private override map is edited.")); return nullptr; }
    if (bTarget && bSave && C->GetOutermost()->IsDirty() && !C->GetOutermost()->HasAnyPackageFlags(PKG_NewlyCreated))
    { Fail(R, TEXT("material_collection_dirty_save_pending"), TEXT("A previously saved dirty target may contain unrelated changes. Use save=false and a separately reviewed exact save; newly created unsaved targets are admitted.")); return nullptr; }
    return C;
}
bool ValidateArrays(const TArray<FCollectionScalarParameter>& Scalars, const TArray<FCollectionVectorParameter>& Vectors, FResult& R)
{
    if (Scalars.Num() + Vectors.Num() > MaxParameters)
    { return Fail(R, TEXT("material_collection_parameter_budget_exceeded"), TEXT("At most 1024 local scalar/vector declarations are admitted.")); }
    TSet<FName> Names; TSet<FGuid> Ids;
    auto Base = [&](const FCollectionParameterBase& P)
    {
        if (!NameValid(P.ParameterName.ToString()) || !P.Id.IsValid() || Names.Contains(P.ParameterName) || Ids.Contains(P.Id)) { return false; }
        Names.Add(P.ParameterName); Ids.Add(P.Id); return true;
    };
    for (const auto& P : Scalars) { if (!Base(P) || !FMath::IsFinite(P.DefaultValue)) { return Fail(R, TEXT("material_collection_declarations_invalid"), TEXT("Local names/IDs must be finite, unique and unambiguous across scalar/vector declarations.")); } }
    for (const auto& P : Vectors) { if (!Base(P) || !ColorFinite(P.DefaultValue)) { return Fail(R, TEXT("material_collection_declarations_invalid"), TEXT("Local names/IDs must be finite, unique and unambiguous across scalar/vector declarations.")); } }
    return true;
}
TSharedRef<FJsonObject> Declarations(const TArray<FCollectionScalarParameter>& Scalars, const TArray<FCollectionVectorParameter>& Vectors)
{
    auto J = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> S, V;
    for (const auto& P : Scalars)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), P.ParameterName.ToString());
        Row->SetStringField(TEXT("id"), P.Id.ToString(EGuidFormats::DigitsWithHyphens)); Row->SetNumberField(TEXT("default_value"), P.DefaultValue);
        S.Add(MakeShared<FJsonValueObject>(Row));
    }
    for (const auto& P : Vectors)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), P.ParameterName.ToString());
        Row->SetStringField(TEXT("id"), P.Id.ToString(EGuidFormats::DigitsWithHyphens)); Row->SetObjectField(TEXT("default_value"), Color(P.DefaultValue));
        V.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("scalars"), S); J->SetArrayField(TEXT("vectors"), V); return J;
}
bool Disk(UMaterialParameterCollection* C, TSharedRef<FJsonObject>& J, FResult& R)
{
    const FString Filename = FPackageName::LongPackageNameToFilename(C->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const int64 Bytes = IFileManager::Get().FileSize(*Filename); J->SetStringField(TEXT("filename"), Filename);
    J->SetBoolField(TEXT("exists"), Bytes >= 0); J->SetNumberField(TEXT("bytes"), Bytes >= 0 ? Bytes : 0);
    J->SetStringField(TEXT("digest_algorithm"), TEXT("SHA1")); J->SetStringField(TEXT("sha1"), TEXT(""));
    if (Bytes < 0) { return true; }
    TArray<uint8> Data;
    if (Bytes > MaxDiskBytes || !FFileHelper::LoadFileToArray(Data, *Filename) || Data.Num() != Bytes)
    { return Fail(R, TEXT("material_collection_disk_unobservable"), TEXT("Selected asset disk bytes could not be read completely within 8 MiB.")); }
    uint8 Digest[FSHA1::DigestSize]; FSHA1::HashBuffer(Data.GetData(), Data.Num(), Digest);
    J->SetStringField(TEXT("sha1"), BytesToHex(Digest, FSHA1::DigestSize).ToLower()); return true;
}
bool OtherDirty(UMaterialParameterCollection* Target, TSharedRef<FJsonObject>& J, FResult& R)
{
    TArray<FString> Names; int32 Count = 0;
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        if (++Count > MaxLoadedPackages) { return Fail(R, TEXT("material_collection_package_budget_exceeded"), TEXT("Unrelated dirty-package observation exceeds 4096 loaded packages.")); }
        if (*It != Target->GetOutermost() && It->GetName().StartsWith(TEXT("/Game/")) && It->IsDirty()) { Names.Add(It->GetName()); }
    }
    Names.Sort(); TArray<TSharedPtr<FJsonValue>> Rows; for (const auto& Name : Names) { Rows.Add(MakeShared<FJsonValueString>(Name)); }
    J->SetArrayField(TEXT("other_dirty_game_packages"), Rows); return true;
}
struct FObservation
{
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> DiskState = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> OtherPackages = MakeShared<FJsonObject>();
    UWorld* World = nullptr;
    UMaterialParameterCollectionInstance* Instance = nullptr;
    bool bRuntimeRequested = false;
};
bool Observe(UMaterialParameterCollection* C, FObservation& O, FResult& R, bool bRuntime = false)
{
    if (!Loaded(C) || C->GetClass() != UMaterialParameterCollection::StaticClass() || C->GetBaseParameterCollection()
        || !ValidateArrays(C->ScalarParameters, C->VectorParameters, R)) { return Fail(R, TEXT("material_collection_observation_failed"), TEXT("Current exact native collection/declaration observation is invalid or incomplete.")); }
    O.State->SetStringField(TEXT("object_path"), C->GetPathName()); O.State->SetStringField(TEXT("class_path"), C->GetClass()->GetPathName());
    O.State->SetStringField(TEXT("package_path"), C->GetOutermost()->GetName()); O.State->SetBoolField(TEXT("dirty"), C->GetOutermost()->IsDirty());
    O.State->SetStringField(TEXT("state_id"), C->StateId.ToString(EGuidFormats::DigitsWithHyphens));
    O.State->SetObjectField(TEXT("declarations"), Declarations(C->ScalarParameters, C->VectorParameters));
    O.bRuntimeRequested = bRuntime;
    O.State->SetBoolField(TEXT("runtime_observation_requested"), bRuntime);
    O.World = bRuntime && GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (O.World && (!Loaded(O.World) || O.World->WorldType != EWorldType::Editor))
    { return Fail(R, TEXT("material_collection_editor_world_required"), TEXT("Runtime observation is limited to the actual Editor world outside PIE.")); }
    if (O.World)
    {
        // World.cpp:1936-1953 lazily creates the instance on a miss. This is
        // deliberately a disclosed engine dispatch, never a read-only claim.
        R.bMutationAttempted = true;
        R.Data->SetBoolField(TEXT("runtime_instance_lookup_dispatched"), true);
        R.Data->SetBoolField(TEXT("runtime_lazy_creation_possible"), true);
        R.Data->SetBoolField(TEXT("implicit_world_render_update_possible"), true);
        R.Data->SetNumberField(TEXT("runtime_instance_lookup_count"), R.Data->GetNumberField(TEXT("runtime_instance_lookup_count")) + 1);
        O.Instance = O.World->GetParameterCollectionInstance(C);
        if (!O.Instance) { return Fail(R, TEXT("material_collection_runtime_identity_invalid"), TEXT("The actual Editor-world collection lookup returned no instance.")); }
    }
    if (O.Instance && (!Loaded(O.Instance) || O.Instance->GetClass() != UMaterialParameterCollectionInstance::StaticClass()
        || O.Instance->GetOutermost() != GetTransientPackage() || O.Instance->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
        || O.Instance->GetCollection() != C))
    { return Fail(R, TEXT("material_collection_runtime_identity_invalid"), TEXT("The existing Editor-world instance must have exact native transient class/collection identity.")); }
    O.State->SetBoolField(TEXT("has_world"), O.World != nullptr); O.State->SetStringField(TEXT("world_path"), O.World ? O.World->GetPathName() : FString());
    O.State->SetStringField(TEXT("world_type"), O.World ? TEXT("Editor") : TEXT("none"));
    O.State->SetBoolField(TEXT("has_runtime_instance"), O.Instance != nullptr);
    O.State->SetStringField(TEXT("runtime_instance_path"), O.Instance ? O.Instance->GetPathName() : FString());
    TArray<TSharedPtr<FJsonValue>> RuntimeScalars, RuntimeVectors;
    for (const auto& P : C->ScalarParameters)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), P.ParameterName.ToString()); float Value = 0;
        const bool Resolved = O.Instance && O.Instance->GetScalarParameterValue(P.ParameterName, Value);
        if (O.Instance && (!Resolved || !FMath::IsFinite(Value))) { return Fail(R, TEXT("material_collection_runtime_unobservable"), TEXT("Runtime scalar getter did not resolve a finite declared value.")); }
        Row->SetBoolField(TEXT("resolved"), Resolved); if (Resolved) { Row->SetNumberField(TEXT("value"), Value); }
        RuntimeScalars.Add(MakeShared<FJsonValueObject>(Row));
    }
    for (const auto& P : C->VectorParameters)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), P.ParameterName.ToString()); FLinearColor Value;
        const bool Resolved = O.Instance && O.Instance->GetVectorParameterValue(P.ParameterName, Value);
        if (O.Instance && (!Resolved || !ColorFinite(Value))) { return Fail(R, TEXT("material_collection_runtime_unobservable"), TEXT("Runtime color getter did not resolve a finite declared value.")); }
        Row->SetBoolField(TEXT("resolved"), Resolved); if (Resolved) { Row->SetObjectField(TEXT("value"), Color(Value)); }
        RuntimeVectors.Add(MakeShared<FJsonValueObject>(Row));
    }
    O.State->SetArrayField(TEXT("runtime_scalars"), RuntimeScalars); O.State->SetArrayField(TEXT("runtime_vectors"), RuntimeVectors);
    return Disk(C, O.DiskState, R) && OtherDirty(C, O.OtherPackages, R);
}
void StoreObservation(FResult& R, const TCHAR* Key, const FObservation& O)
{
    auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("disk"), O.DiskState);
    J->SetObjectField(TEXT("package_observation"), O.OtherPackages); R.Data->SetObjectField(Key, J);
}
bool CaptureAfter(UMaterialParameterCollection* C, const FObservation& Before, FObservation& After, FResult& R)
{
    if (!Observe(C, After, R, Before.bRuntimeRequested)) { R.bChangeStateKnown = false; return false; }
    StoreObservation(R, TEXT("after"), After);
    R.bChanged = Json(Before.State) != Json(After.State) || Json(Before.DiskState) != Json(After.DiskState);
    R.Data->SetBoolField(TEXT("other_dirty_packages_preserved"), Json(Before.OtherPackages) == Json(After.OtherPackages));
    R.Data->SetBoolField(TEXT("target_disk_preserved_before_save"), Json(Before.DiskState) == Json(After.DiskState));
    R.Data->SetBoolField(TEXT("dirty_after_mutation"), C->GetOutermost()->IsDirty());
    return true;
}
bool Finish(UMaterialParameterCollection* C, const FObservation& Before, const TSharedRef<FJsonObject>& Expected,
    bool bDefaultsChanged, bool bSave, FResult& R, const TSharedRef<FJsonObject>* SourceBefore = nullptr,
    UMaterialParameterCollection* Source = nullptr)
{
    R.Stage = TEXT("postcondition"); FObservation After;
    if (!CaptureAfter(C, Before, After, R)) { return false; }
    const bool Target = Json(Declarations(C->ScalarParameters, C->VectorParameters)) == Json(Expected)
        && Before.State->GetStringField(TEXT("object_path")) == After.State->GetStringField(TEXT("object_path"))
        && Before.State->GetStringField(TEXT("class_path")) == After.State->GetStringField(TEXT("class_path"))
        && Before.State->GetStringField(TEXT("package_path")) == After.State->GetStringField(TEXT("package_path"))
        && Before.World == After.World;
    bool SourcePreserved = true;
    if (Source && SourceBefore)
    {
        FObservation Current;
        if (!Observe(Source, Current, R, false)) { R.bChangeStateKnown = false; return false; }
        auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), Current.State); J->SetObjectField(TEXT("disk"), Current.DiskState);
        R.Data->SetObjectField(TEXT("source_after"), J); SourcePreserved = Json(*SourceBefore) == Json(J);
        R.Data->SetBoolField(TEXT("source_preserved"), SourcePreserved);
    }
    const bool PriorDisk = Json(Before.DiskState) == Json(After.DiskState);
    const bool Other = Json(Before.OtherPackages) == Json(After.OtherPackages);
    R.Data->SetBoolField(TEXT("declarations_postcondition_verified"), Target);
    if (!Target || !SourcePreserved || !PriorDisk || !Other)
    { return Fail(R, TEXT("material_collection_postcondition_failed"), TEXT("Actual retained declarations/source/disk/unrelated dirty state failed its planned preservation proof; no save or automatic retry.")); }
    R.bPostconditionPassed = true;
    if (bSave && bDefaultsChanged)
    {
        R.Stage = TEXT("save"); R.Data->SetBoolField(TEXT("save_attempted"), true);
        const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(C, false); R.Data->SetBoolField(TEXT("saved"), Saved);
        FObservation SavedState;
        if (!Observe(C, SavedState, R, Before.bRuntimeRequested)) { R.bChangeStateKnown = false; R.bPostconditionPassed = false; return false; }
        StoreObservation(R, TEXT("after_save"), SavedState);
        R.Data->SetBoolField(TEXT("dirty_after_save"), C->GetOutermost()->IsDirty());
        bool Preserved = Json(Declarations(C->ScalarParameters, C->VectorParameters)) == Json(Expected)
            && Json(After.OtherPackages) == Json(SavedState.OtherPackages)
            && After.State->GetStringField(TEXT("object_path")) == SavedState.State->GetStringField(TEXT("object_path"))
            && After.World == SavedState.World
            && Json(After.State->GetObjectField(TEXT("declarations")).ToSharedRef()) == Json(SavedState.State->GetObjectField(TEXT("declarations")).ToSharedRef());
        if (Before.bRuntimeRequested)
        {
            auto OldRuntime = MakeShared<FJsonObject>(); auto NewRuntime = MakeShared<FJsonObject>();
            OldRuntime->SetArrayField(TEXT("scalars"), After.State->GetArrayField(TEXT("runtime_scalars")));
            OldRuntime->SetArrayField(TEXT("vectors"), After.State->GetArrayField(TEXT("runtime_vectors")));
            NewRuntime->SetArrayField(TEXT("scalars"), SavedState.State->GetArrayField(TEXT("runtime_scalars")));
            NewRuntime->SetArrayField(TEXT("vectors"), SavedState.State->GetArrayField(TEXT("runtime_vectors")));
            const bool RuntimePreserved = Json(OldRuntime) == Json(NewRuntime);
            R.Data->SetBoolField(TEXT("save_runtime_effective_values_preserved"), RuntimePreserved); Preserved &= RuntimePreserved;
        }
        if (Source && SourceBefore)
        {
            FObservation Current;
            if (!Observe(Source, Current, R, false)) { R.bChangeStateKnown = false; R.bPostconditionPassed = false; return false; }
            auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), Current.State); J->SetObjectField(TEXT("disk"), Current.DiskState);
            R.Data->SetObjectField(TEXT("source_after_save"), J);
            const bool StillPreserved = Json(*SourceBefore) == Json(J);
            R.Data->SetBoolField(TEXT("source_preserved_after_save"), StillPreserved); Preserved &= StillPreserved;
        }
        R.Data->SetBoolField(TEXT("save_declarations_and_other_packages_preserved"), Preserved);
        if (!Saved)
        {
            R.bChangeStateKnown = false; R.bPostconditionPassed = false;
            R.Data->SetBoolField(TEXT("save_disk_outcome_known"), false);
            return Fail(R, TEXT("material_collection_save_failed"), TEXT("Exact save returned false; RAM prefix is retained and disk transaction outcome is not certified. Do not resend mutation or save automatically."));
        }
        if (!Preserved || C->GetOutermost()->IsDirty() || !SavedState.DiskState->GetBoolField(TEXT("exists")))
        { R.bPostconditionPassed = false; return Fail(R, TEXT("material_collection_save_postcondition_failed"), TEXT("Exact save returned true but selected bytes/clean/declaration preservation postconditions failed.")); }
        R.Data->SetBoolField(TEXT("save_disk_outcome_known"), true);
    }
    R.Data->SetBoolField(TEXT("dirty_after_operation"), C->GetOutermost()->IsDirty()); R.Stage = TEXT("complete"); R.bSuccess = true; return true;
}
bool ParseObject(const FString& Text, TSharedPtr<FJsonObject>& J, FResult& R)
{
    if (Text.Len() > 262144 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()
        || J->Values.Num() > MaxRequestedValues)
    { return Fail(R, TEXT("material_collection_values_invalid"), TEXT("Scalar/vector JSON must be objects with at most 1024 rows and at most 256 KiB each.")); }
    return true;
}
bool Float(const TSharedPtr<FJsonValue>& V, float& Value)
{
    if (!V.IsValid() || V->Type != EJson::Number || !FMath::IsFinite(V->AsNumber())) { return false; }
    Value = static_cast<float>(V->AsNumber()); return FMath::IsFinite(Value);
}
bool Component(const TSharedPtr<FJsonObject>& J, const TCHAR* C, const TCHAR* Axis, float& Value)
{ return Float(J->TryGetField(C), Value) || Float(J->TryGetField(Axis), Value); }
struct FScalarWrite { FString Name; int32 Index = INDEX_NONE; float Value = 0, Old = 0; bool bRuntimeReturned = false; };
struct FVectorWrite { FString Name; int32 Index = INDEX_NONE; FLinearColor Value, Old; bool bRuntimeReturned = false; };
}

Function::FResult SetValues(const FString& Path, const FString& ScalarsJson, const FString& VectorsJson,
    bool bSetAssetDefaults, bool bSetRuntime, bool bSave)
{
    FResult R; Init(R, bSave); R.Data->SetBoolField(TEXT("set_asset_defaults"), bSetAssetDefaults); R.Data->SetBoolField(TEXT("set_runtime"), bSetRuntime);
    TSharedPtr<FJsonObject> Scalars, Vectors;
    if (!ParseObject(ScalarsJson, Scalars, R) || !ParseObject(VectorsJson, Vectors, R) || !EditorState(R)) { return R; }
    auto* C = Resolve(Path, true, bSave && bSetAssetDefaults, R); if (!C) { return R; } const TStrongObjectPtr<UMaterialParameterCollection> Keep(C);
    if (!ValidateArrays(C->ScalarParameters, C->VectorParameters, R)) { return R; }
    TArray<FScalarWrite> SW; TArray<FVectorWrite> VW; TArray<TSharedPtr<FJsonValue>> Errors, MissingS, MissingV, UpdatedS, UpdatedV;
    TSet<FName> ScalarNames, VectorNames;
    for (const auto& Pair : Scalars->Values)
    {
        const FString Name(Pair.Key.ToView()); const FName N(*Name); FScalarWrite W; W.Name = Name;
        if (!NameValid(Name) || ScalarNames.Contains(N)) { Fail(R, TEXT("material_collection_value_name_invalid"), TEXT("Requested scalar names must be valid and case-insensitively unique.")); return R; } ScalarNames.Add(N);
        if (!Float(Pair.Value, W.Value)) { Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Scalar '%s' requires a finite float-range number value"), *Name))); continue; }
        W.Index = C->ScalarParameters.IndexOfByPredicate([&](const auto& P) { return P.ParameterName == N; });
        if (W.Index == INDEX_NONE) { MissingS.Add(MakeShared<FJsonValueString>(Name)); continue; }
        W.Old = C->ScalarParameters[W.Index].DefaultValue; SW.Add(W);
    }
    for (const auto& Pair : Vectors->Values)
    {
        const FString Name(Pair.Key.ToView()); const FName N(*Name); FVectorWrite W; W.Name = Name;
        if (!NameValid(Name) || VectorNames.Contains(N)) { Fail(R, TEXT("material_collection_value_name_invalid"), TEXT("Requested vector names must be valid and case-insensitively unique.")); return R; } VectorNames.Add(N);
        if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Object)
        { Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Vector '%s' requires an object value"), *Name))); continue; }
        const auto Value = Pair.Value->AsObject(); W.Value = FLinearColor::Transparent;
        if (!Component(Value, TEXT("r"), TEXT("x"), W.Value.R) || !Component(Value, TEXT("g"), TEXT("y"), W.Value.G)
            || !Component(Value, TEXT("b"), TEXT("z"), W.Value.B))
        { Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Vector '%s' requires finite numeric r/x, g/y and b/z components"), *Name))); continue; }
        // UE56 initialized Transparent and assigned A only for numeric a/w.
        // Its local AlphaValue=1 did not supply an absent/non-numeric default.
        float Alpha = 0;
        const bool AlphaNumeric = (Value->TryGetField(TEXT("a")).IsValid() && Value->TryGetField(TEXT("a"))->Type == EJson::Number)
            || (Value->TryGetField(TEXT("w")).IsValid() && Value->TryGetField(TEXT("w"))->Type == EJson::Number);
        if (Component(Value, TEXT("a"), TEXT("w"), Alpha)) { W.Value.A = Alpha; }
        else if (AlphaNumeric) { Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Vector '%s' optional alpha exceeds finite float range"), *Name))); continue; }
        W.Index = C->VectorParameters.IndexOfByPredicate([&](const auto& P) { return P.ParameterName == N; });
        if (W.Index == INDEX_NONE) { MissingV.Add(MakeShared<FJsonValueString>(Name)); continue; }
        W.Old = C->VectorParameters[W.Index].DefaultValue; VW.Add(W);
    }
    // Validate every requested name/value before the public world lookup,
    // which can lazily allocate an instance. Invalid value rows alone retain
    // the legacy continuation contract; malformed names reject known zero.
    FObservation Before; if (!Observe(C, Before, R, bSetRuntime)) { if (R.bMutationAttempted) { R.bChangeStateKnown = false; } return R; } StoreObservation(R, TEXT("before"), Before);
    const TStrongObjectPtr<UWorld> KeepWorld(Before.World); const TStrongObjectPtr<UMaterialParameterCollectionInstance> KeepInstance(Before.Instance);
    R.Data->SetStringField(TEXT("collection"), C->GetPathName()); R.Data->SetBoolField(TEXT("was_dirty_before_set"), C->GetOutermost()->IsDirty());
    R.Data->SetBoolField(TEXT("has_world"), Before.World != nullptr); R.Data->SetStringField(TEXT("world_type"), Before.World ? TEXT("editor") : TEXT("none"));
    R.Data->SetBoolField(TEXT("has_runtime_instance"), Before.Instance != nullptr);
    auto ExpectedScalars = C->ScalarParameters; auto ExpectedVectors = C->VectorParameters;
    int32 AS = 0, AV = 0, RS = 0, RV = 0;
    if (bSetAssetDefaults)
    {
        for (const auto& W : SW) { if (!FMath::IsNearlyEqual(W.Old, W.Value)) { ExpectedScalars[W.Index].DefaultValue = W.Value; ++AS; } }
        for (const auto& W : VW) { if (!W.Old.Equals(W.Value)) { ExpectedVectors[W.Index].DefaultValue = W.Value; ++AV; } }
    }
    const bool DefaultsChanged = AS > 0 || AV > 0; const auto Expected = Declarations(ExpectedScalars, ExpectedVectors);
    R.Data->SetArrayField(TEXT("errors"), Errors); R.Data->SetArrayField(TEXT("missing_scalars"), MissingS); R.Data->SetArrayField(TEXT("missing_vectors"), MissingV);
    R.Data->SetNumberField(TEXT("asset_scalar_update_count"), AS); R.Data->SetNumberField(TEXT("asset_vector_update_count"), AV);
    R.Data->SetBoolField(TEXT("asset_defaults_changed"), DefaultsChanged); R.Data->SetBoolField(TEXT("missing_parameter_runtime_setter_dispatched"), false);
    R.Stage = TEXT("values");
    if (DefaultsChanged)
    {
        R.bMutationAttempted = true; C->Modify(); C->ScalarParameters = ExpectedScalars; C->VectorParameters = ExpectedVectors;
    }
    for (auto& W : SW)
    {
        if (bSetRuntime && Before.Instance) { R.bMutationAttempted = true; W.bRuntimeReturned = Before.Instance->SetScalarParameterValue(FName(*W.Name), W.Value); if (W.bRuntimeReturned) { ++RS; } }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), W.Name); Row->SetNumberField(TEXT("value"), W.Value);
        Row->SetNumberField(TEXT("old_asset_default"), W.Old); Row->SetBoolField(TEXT("runtime_updated"), W.bRuntimeReturned); UpdatedS.Add(MakeShared<FJsonValueObject>(Row));
    }
    for (auto& W : VW)
    {
        if (bSetRuntime && Before.Instance) { R.bMutationAttempted = true; W.bRuntimeReturned = Before.Instance->SetVectorParameterValue(FName(*W.Name), W.Value); if (W.bRuntimeReturned) { ++RV; } }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), W.Name); Row->SetObjectField(TEXT("value"), Color(W.Value));
        Row->SetObjectField(TEXT("old_asset_default"), Color(W.Old)); Row->SetBoolField(TEXT("runtime_updated"), W.bRuntimeReturned); UpdatedV.Add(MakeShared<FJsonValueObject>(Row));
    }
    R.Data->SetArrayField(TEXT("updated_scalars"), UpdatedS); R.Data->SetArrayField(TEXT("updated_vectors"), UpdatedV);
    R.Data->SetNumberField(TEXT("runtime_scalar_update_count"), RS); R.Data->SetNumberField(TEXT("runtime_vector_update_count"), RV);
    if (DefaultsChanged)
    {
        R.Stage = TEXT("notification"); R.Data->SetBoolField(TEXT("post_edit_change_invoked"), true);
        C->PostEditChange(); C->MarkPackageDirty(); R.Data->SetBoolField(TEXT("implicit_world_render_update_possible"), true);
    }
    // Observe the actual current instance after notification; never assume it
    // retained identity or values merely because an earlier setter returned.
    FObservation Check; bool RuntimeVerified = Observe(C, Check, R, bSetRuntime);
    if (!RuntimeVerified) { R.bChangeStateKnown = false; return R; }
    RuntimeVerified &= Check.World == Before.World && (Check.Instance != nullptr) == (Before.Instance != nullptr);
    bool UnselectedPreserved = true;
    if (Before.Instance)
    {
        const auto& OldS = Before.State->GetArrayField(TEXT("runtime_scalars"));
        const auto& NewS = Check.State->GetArrayField(TEXT("runtime_scalars"));
        const auto& OldV = Before.State->GetArrayField(TEXT("runtime_vectors"));
        const auto& NewV = Check.State->GetArrayField(TEXT("runtime_vectors"));
        for (int32 I = 0; I < OldS.Num(); ++I)
        {
            const auto* W = SW.FindByPredicate([&](const auto& Item) { return Item.Index == I; });
            const double ExpectedValue = W && W->bRuntimeReturned ? W->Value : OldS[I]->AsObject()->GetNumberField(TEXT("value"));
            const bool Match = NewS.IsValidIndex(I) && NewS[I]->AsObject()->GetBoolField(TEXT("resolved"))
                && NewS[I]->AsObject()->GetNumberField(TEXT("value")) == ExpectedValue;
            RuntimeVerified &= Match; if (!W || !W->bRuntimeReturned) { UnselectedPreserved &= Match; }
        }
        for (int32 I = 0; I < OldV.Num(); ++I)
        {
            const auto* W = VW.FindByPredicate([&](const auto& Item) { return Item.Index == I; });
            const TSharedRef<FJsonObject> ExpectedValue = W && W->bRuntimeReturned ? Color(W->Value) : OldV[I]->AsObject()->GetObjectField(TEXT("value")).ToSharedRef();
            const bool Match = NewV.IsValidIndex(I) && NewV[I]->AsObject()->GetBoolField(TEXT("resolved"))
                && Json(NewV[I]->AsObject()->GetObjectField(TEXT("value")).ToSharedRef()) == Json(ExpectedValue);
            RuntimeVerified &= Match; if (!W || !W->bRuntimeReturned) { UnselectedPreserved &= Match; }
        }
    }
    R.Data->SetBoolField(TEXT("runtime_successful_write_values_verified"), RuntimeVerified);
    R.Data->SetBoolField(TEXT("runtime_effective_value_verification_applicable"), Before.Instance != nullptr);
    R.Data->SetBoolField(TEXT("runtime_unselected_and_unsuccessful_values_preserved"), UnselectedPreserved);
    R.Data->SetBoolField(TEXT("runtime_world_identity_preserved"), Check.World == Before.World);
    R.Data->SetBoolField(TEXT("runtime_instance_identity_preserved"), Check.Instance == Before.Instance);
    if (!RuntimeVerified)
    {
        FObservation After; CaptureAfter(C, Before, After, R);
        Fail(R, TEXT("material_collection_runtime_postcondition_failed"), TEXT("Actual current instance did not preserve requested successful writes or prior unselected/unsuccessful effective values; RAM prefix retained and save suppressed.")); return R;
    }
    if (!Finish(C, Before, Expected, DefaultsChanged, bSave, R)) { return R; }
    R.Data->SetBoolField(TEXT("dirty_after_set"), C->GetOutermost()->IsDirty());
    if (!Errors.IsEmpty())
    {
        R.Stage = TEXT("complete_with_errors");
        Fail(R, TEXT("material_collection_value_rows_invalid"), TEXT("Valid rows were processed and observed; invalid value rows were skipped with legacy errors/counts retained. Do not resend the whole request."));
    }
    return R;
}

Function::FResult Mirror(const FString& SourcePath, const FString& TargetPath, const FString& NamesJson,
    bool bCopyDefaults, bool bPreserveIds, bool bSave)
{
    FResult R; Init(R, bSave); R.Data->SetBoolField(TEXT("copy_defaults"), bCopyDefaults); R.Data->SetBoolField(TEXT("preserve_ids"), bPreserveIds);
    TArray<TSharedPtr<FJsonValue>> Names; TSet<FName> Filter;
    if (NamesJson.Len() > 262144 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(NamesJson), Names) || Names.Num() > MaxRequestedValues)
    { Fail(R, TEXT("material_collection_filter_invalid"), TEXT("parameter_names must be a JSON string array with at most 1024 names and 256 KiB.")); return R; }
    for (const auto& Name : Names)
    {
        if (!Name.IsValid() || Name->Type != EJson::String)
        { Fail(R, TEXT("material_collection_filter_invalid"), TEXT("Parameter filter entries must be strings; implicit non-string coercion remains unsupported.")); return R; }
        const FString NameText = Name->AsString().TrimStartAndEnd(); if (NameText.IsEmpty()) { continue; }
        if (!NameValid(NameText)) { Fail(R, TEXT("material_collection_filter_invalid"), TEXT("Nonempty parameter filters must be valid bounded name strings.")); return R; }
        Filter.Add(FName(*NameText));
    }
    if (!EditorState(R)) { return R; }
    auto* Source = Resolve(SourcePath, false, false, R); if (!Source) { return R; } const TStrongObjectPtr<UMaterialParameterCollection> KeepSource(Source);
    auto* Target = Resolve(TargetPath, true, bSave, R); if (!Target) { return R; } const TStrongObjectPtr<UMaterialParameterCollection> KeepTarget(Target);
    FObservation Before, SourceObservation;
    if (!Observe(Target, Before, R) || !Observe(Source, SourceObservation, R, false)) { return R; }
    StoreObservation(R, TEXT("before"), Before); auto SourceBefore = MakeShared<FJsonObject>();
    SourceBefore->SetObjectField(TEXT("state"), SourceObservation.State); SourceBefore->SetObjectField(TEXT("disk"), SourceObservation.DiskState); R.Data->SetObjectField(TEXT("source_before"), SourceBefore);
    const TStrongObjectPtr<UWorld> KeepWorld(Before.World); const TStrongObjectPtr<UMaterialParameterCollectionInstance> KeepInstance(Before.Instance);
    auto Scalars = Target->ScalarParameters; auto Vectors = Target->VectorParameters;
    TArray<TSharedPtr<FJsonValue>> MirroredS, MirroredV, Warnings; int32 AddedS = 0, DefaultS = 0, IdS = 0, AddedV = 0, DefaultV = 0, IdV = 0;
    auto Selected = [&](FName Name) { return Filter.IsEmpty() || Filter.Contains(Name); };
    for (const auto& P : Source->ScalarParameters)
    {
        if (!Selected(P.ParameterName)) { continue; }
        int32 I = Scalars.IndexOfByPredicate([&](const auto& T) { return T.ParameterName == P.ParameterName; }); const bool Added = I == INDEX_NONE;
        if (Added)
        {
            FCollectionScalarParameter New = P; if (!bPreserveIds) { New.Id = FGuid::NewGuid(); }
            I = Scalars.Add(New); ++AddedS; // Legacy copies defaults for NEW rows even if copy_defaults=false.
        }
        else
        {
            if (bPreserveIds && Scalars[I].Id != P.Id)
            {
                if (Scalars.ContainsByPredicate([&](const auto& T) { return T.Id == P.Id && T.ParameterName != P.ParameterName; }))
                { Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Skipped scalar id preserve for '%s' because the source id is already used by another target scalar."), *P.ParameterName.ToString()))); }
                else { Scalars[I].Id = P.Id; ++IdS; }
            }
            if (bCopyDefaults && !FMath::IsNearlyEqual(Scalars[I].DefaultValue, P.DefaultValue)) { Scalars[I].DefaultValue = P.DefaultValue; ++DefaultS; }
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), P.ParameterName.ToString()); Row->SetStringField(TEXT("source_id"), P.Id.ToString(EGuidFormats::DigitsWithHyphens));
        Row->SetStringField(TEXT("target_id"), Scalars[I].Id.ToString(EGuidFormats::DigitsWithHyphens)); Row->SetNumberField(TEXT("default_value"), Scalars[I].DefaultValue);
        Row->SetBoolField(TEXT("added"), Added); MirroredS.Add(MakeShared<FJsonValueObject>(Row));
    }
    for (const auto& P : Source->VectorParameters)
    {
        if (!Selected(P.ParameterName)) { continue; }
        int32 I = Vectors.IndexOfByPredicate([&](const auto& T) { return T.ParameterName == P.ParameterName; }); const bool Added = I == INDEX_NONE;
        if (Added) { FCollectionVectorParameter New = P; if (!bPreserveIds) { New.Id = FGuid::NewGuid(); } I = Vectors.Add(New); ++AddedV; }
        else
        {
            if (bPreserveIds && Vectors[I].Id != P.Id)
            {
                if (Vectors.ContainsByPredicate([&](const auto& T) { return T.Id == P.Id && T.ParameterName != P.ParameterName; }))
                { Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Skipped vector id preserve for '%s' because the source id is already used by another target vector."), *P.ParameterName.ToString()))); }
                else { Vectors[I].Id = P.Id; ++IdV; }
            }
            if (bCopyDefaults && !Vectors[I].DefaultValue.Equals(P.DefaultValue)) { Vectors[I].DefaultValue = P.DefaultValue; ++DefaultV; }
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), P.ParameterName.ToString()); Row->SetStringField(TEXT("source_id"), P.Id.ToString(EGuidFormats::DigitsWithHyphens));
        Row->SetStringField(TEXT("target_id"), Vectors[I].Id.ToString(EGuidFormats::DigitsWithHyphens)); Row->SetObjectField(TEXT("default_value"), Color(Vectors[I].DefaultValue));
        Row->SetBoolField(TEXT("added"), Added); MirroredV.Add(MakeShared<FJsonValueObject>(Row));
    }
    // Source defaults/flags and all resulting IDs/types are validated before
    // any target declaration. Legacy ambiguous cross-kind/new-ID collisions
    // are known unsupported, not repaired after allocation.
    if (!ValidateArrays(Scalars, Vectors, R)) { return R; }
    const bool Changed = AddedS + DefaultS + IdS + AddedV + DefaultV + IdV > 0; const auto Expected = Declarations(Scalars, Vectors);
    R.Data->SetStringField(TEXT("source_collection"), Source->GetPathName()); R.Data->SetStringField(TEXT("target_collection"), Target->GetPathName());
    R.Data->SetBoolField(TEXT("changed"), Changed); R.Data->SetBoolField(TEXT("was_dirty_before_mirror"), Target->GetOutermost()->IsDirty());
    R.Data->SetNumberField(TEXT("added_scalar_count"), AddedS); R.Data->SetNumberField(TEXT("updated_scalar_default_count"), DefaultS); R.Data->SetNumberField(TEXT("updated_scalar_id_count"), IdS);
    R.Data->SetNumberField(TEXT("added_vector_count"), AddedV); R.Data->SetNumberField(TEXT("updated_vector_default_count"), DefaultV); R.Data->SetNumberField(TEXT("updated_vector_id_count"), IdV);
    R.Data->SetArrayField(TEXT("mirrored_scalars"), MirroredS); R.Data->SetArrayField(TEXT("mirrored_vectors"), MirroredV); R.Data->SetArrayField(TEXT("warnings"), Warnings);
    if (Changed)
    {
        R.Stage = TEXT("mirror"); R.bMutationAttempted = true; Target->Modify(); Target->ScalarParameters = Scalars; Target->VectorParameters = Vectors;
        R.Stage = TEXT("notification"); R.Data->SetBoolField(TEXT("post_edit_change_invoked"), true);
        Target->PostEditChange(); Target->MarkPackageDirty(); R.Data->SetBoolField(TEXT("implicit_world_render_update_possible"), true);
    }
    if (!Finish(Target, Before, Expected, Changed, bSave, R, &SourceBefore, Source)) { return R; }
    R.Data->SetBoolField(TEXT("dirty_after_mirror"), Target->GetOutermost()->IsDirty()); return R;
}
}
