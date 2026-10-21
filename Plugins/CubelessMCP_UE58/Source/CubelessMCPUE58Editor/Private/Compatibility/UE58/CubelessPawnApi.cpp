// CF: ordered UE56 Pawn defaults contract, using public UE58 Engine fields/setter.
#include "Compatibility/UE58/CubelessPawnApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Pawn
{
namespace
{
using Variable::FResult;
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool HasNul(const FString& S) { for (int32 I = 0; I < S.Len(); ++I) { if (S[I] == TCHAR('\0')) { return true; } } return false; }
bool Clean(const FString& S)
{ for (const TCHAR* K : { TEXT("/Script/Cubeless"), TEXT("/Script/UnrealMCP"), TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry"), TEXT("/Engine/PythonTypes"), TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/") }) { if (S.Contains(K, ESearchCase::IgnoreCase)) { return false; } } return true; }
void Count(FResult& R, const TCHAR* Key) { R.Data->SetNumberField(Key, R.Data->GetNumberField(Key) + 1); }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error, bool Unknown = false)
{ R.bSuccess = false; R.bPostconditionPassed = false; R.ErrorCode = Code; R.Error = Error; if (Unknown) { R.bChangeStateKnown = false; } return false; }
void Init(FResult& R)
{
    for (const TCHAR* K : { TEXT("asset_load_count"), TEXT("observation_capture_calls"), TEXT("property_request_count"), TEXT("property_validation_failures"),
        TEXT("setter_attempt_count"), TEXT("setter_return_count"), TEXT("accepted_property_count"), TEXT("mark_blueprint_modified_calls"), TEXT("mark_blueprint_modified_returns"),
        TEXT("explicit_modify_calls"), TEXT("outer_transaction_calls"), TEXT("full_compile_attempts"), TEXT("save_attempt_count"), TEXT("explicit_cdo_creation_calls") }) { R.Data->SetNumberField(K, 0); }
    R.Data->SetArrayField(TEXT("asset_load_calls"), {}); R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetArrayField(TEXT("property_calls"), {});
    R.Data->SetObjectField(TEXT("results"), MakeShared<FJsonObject>());
    for (const TCHAR* K : { TEXT("observations_complete"), TEXT("unselected_state_preserved"), TEXT("source_preserved"), TEXT("disk_preserved"), TEXT("other_package_dirty_preserved"),
        TEXT("all_requested_properties_succeeded"), TEXT("partial_success"), TEXT("runtime_verified"), TEXT("cold_persistence_verified"), TEXT("implicit_engine_work_measured") }) { R.Data->SetBoolField(K, false); }
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: native Pawn/Character defaults and public Blueprint editor callback; no ServerRPC or runtime Cubeless class."));
    R.Data->SetStringField(TEXT("finite_scope"), TEXT("Normal own Game Blueprint directly parented to Engine Pawn or Character, existing current generated Pawn CDO, no loaded live/derived instances. Short-name registry search, other native/generated parents and unfinished class variants remain incomplete."));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("One original command-level transaction after finite admission, ordered five property applications, then MarkBlueprintAsModified once if any setter succeeded, including matching values. No explicit Modify/PostEdit/compile/save. Damage setter marks replication dirtiness; MarkBlueprintAsModified performs implicit graph/cache/post-construction-list/PostEdit/editor callbacks and dirty/status work. These are not claimed absent or counted as explicit fullcompile."));
    R.Data->SetStringField(TEXT("transaction_scope"), TEXT("Original HandleCommand opens one mutating-command transaction before handler admission. This finite adapter preserves one transaction for admitted ordered processing, including all-invalid property requests, while its known-zero preflight refusals and the original public no-properties shortcut do not open it."));
    R.Data->SetStringField(TEXT("observation_scope"), TEXT("Bounded nontransient reflected BP/CDO/owned graph/component/template fields, typed selected values, loaded native class/CDO fields, target disk and other package dirty flags after explicit asset resolution. Private caches/replication bytes, unloaded assets, runtime behavior and cold persistence are not observed."));
    R.Data->SetStringField(TEXT("legacy_result_scope"), TEXT("Terminal success is any accepted property, including partial failures and matching setters. Empty public request succeeds without Blueprint resolution; original direct native empty-request error remains a documented surface difference. No mutation_result_v1 existed on this route."));
}
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("valid"), Loaded(O)); J->SetStringField(TEXT("path"), Loaded(O) ? O->GetPathName() : FString());
    J->SetStringField(TEXT("class"), Loaded(O) ? O->GetClass()->GetPathName() : FString()); J->SetStringField(TEXT("outer"), Loaded(O) && O->GetOuter() ? O->GetOuter()->GetPathName() : FString());
    J->SetStringField(TEXT("object_flags"), Loaded(O) ? LexToString(static_cast<uint64>(O->GetFlags())) : FString()); return J;
}
bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, int32 Depth = 0)
{
    if (!A || !B || A->Type != B->Type || Depth > 32) { return false; }
    if (A->Type == EJson::Object) { const auto X = A->AsObject(), Y = B->AsObject(); if (!X || !Y || X->Values.Num() != Y->Values.Num()) { return false; } for (const auto& P : X->Values) { if (!Equal(P.Value, Y->TryGetField(P.Key.ToView()), Depth + 1)) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!Equal(X[I], Y[I], Depth + 1)) { return false; } } return true; }
    return A->Type == EJson::String ? A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive) : FJsonValue::CompareEqual(*A, *B);
}
bool Equal(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B) { return Equal(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
const TCHAR* PropertyNames[] = { TEXT("AutoPossessPlayer"), TEXT("bUseControllerRotationYaw"), TEXT("bUseControllerRotationPitch"), TEXT("bUseControllerRotationRoll"), TEXT("bCanBeDamaged") };
TSharedRef<FJsonObject> Values(const APawn* P)
{
    auto J = MakeShared<FJsonObject>(); J->SetNumberField(PropertyNames[0], static_cast<uint8>(P->AutoPossessPlayer)); J->SetBoolField(PropertyNames[1], P->bUseControllerRotationYaw);
    J->SetBoolField(PropertyNames[2], P->bUseControllerRotationPitch); J->SetBoolField(PropertyNames[3], P->bUseControllerRotationRoll); J->SetBoolField(PropertyNames[4], P->CanBeDamaged()); return J;
}
bool Fields(UObject* O, bool Blueprint, bool SelectedCDO, const TSharedRef<FJsonObject>& Out)
{
    int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (P->HasAnyPropertyFlags(CPF_Transient)) { continue; }
        if (Blueprint && (P->GetFName() == TEXT("Status") || P->GetFName() == TEXT("bCachedDependenciesUpToDate"))) { continue; }
        bool Selected = false; for (const TCHAR* Name : PropertyNames) { Selected |= SelectedCDO && P->GetFName() == Name; } if (Selected) { continue; }
        auto J = MakeShared<FJsonObject>(); Out->SetObjectField(P->GetName(), J); if (++Count > 2048 || P->ArrayDim < 1 || P->ArrayDim > 64 || !P->GetOwnerStruct()) { return false; }
        J->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); J->SetStringField(TEXT("property_class"), P->GetClass()->GetName()); J->SetStringField(TEXT("property_flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        TArray<TSharedPtr<FJsonValue>> Rows; J->SetArrayField(TEXT("values"), Rows);
        for (int32 I = 0; I < P->ArrayDim; ++I)
        {
            const void* A = P->ContainerPtrToValuePtr<void>(O, I); FString S;
            if (!P->ExportText_Direct(S, A, A, O, PPF_None) || S.Len() > 1048576 || Chars > 1048576 - S.Len() || !Clean(S)) { return false; }
            Chars += S.Len(); Rows.Add(MakeShared<FJsonValueString>(S)); J->SetArrayField(TEXT("values"), Rows);
        }
    } return true;
}
struct FContext { TStrongObjectPtr<UBlueprint> Blueprint; TStrongObjectPtr<APawn> CDO; FString Path, Package; };
struct FObservation { bool Complete = false; TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), Source = MakeShared<FJsonObject>(), Disk = MakeShared<FJsonObject>(), Other = MakeShared<FJsonObject>(); };
bool Capture(FContext& C, FObservation& O, FResult& R, const TCHAR* Phase)
{
    O = FObservation{}; Count(R, TEXT("observation_capture_calls")); auto J = MakeShared<FJsonObject>(); R.Data->SetObjectField(Phase, J);
    J->SetBoolField(TEXT("complete"), false); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_source"), O.Source); J->SetObjectField(TEXT("disk"), O.Disk); J->SetObjectField(TEXT("other_package_dirty"), O.Other);
    UBlueprint* B = C.Blueprint.Get(); APawn* P = C.CDO.Get();
    if (!Loaded(B) || !Loaded(P) || !Loaded(B->GeneratedClass) || B->GeneratedClass->ClassGeneratedBy != B || B->GeneratedClass->GetDefaultObject(false) != P || P->GetClass() != B->GeneratedClass || B->GetPathName() != C.Path) { return false; }
    O.State->SetObjectField(TEXT("blueprint_identity"), Identity(B)); O.State->SetObjectField(TEXT("cdo_identity"), Identity(P)); O.State->SetBoolField(TEXT("package_dirty"), B->GetOutermost()->IsDirty());
    O.State->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(B->Status)); O.State->SetBoolField(TEXT("cached_dependencies_up_to_date"), B->bCachedDependenciesUpToDate); O.State->SetObjectField(TEXT("selected_values"), Values(P));
    auto Objects = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("unselected_owned_fields"), Objects); TArray<UObject*> Owned, Children;
    Owned.Add(B); Owned.Add(P); GetObjectsWithOuter(B, Children, EGetObjectsFlags::IncludeNestedObjects); Owned.Append(Children); Children.Reset(); GetObjectsWithOuter(P, Children, EGetObjectsFlags::IncludeNestedObjects); Owned.Append(Children);
    if (B->SimpleConstructionScript)
    {
        Owned.AddUnique(B->SimpleConstructionScript); for (USCS_Node* N : B->SimpleConstructionScript->GetAllNodes()) { if (!Loaded(N) || !Loaded(N->ComponentTemplate)) { return false; } Owned.AddUnique(N); Owned.AddUnique(N->ComponentTemplate); }
    }
    if (Owned.Num() > 512) { return false; }
    for (UObject* X : Owned)
    {
        if (!Loaded(X) || X->GetOutermost() != B->GetOutermost() || !Clean(X->GetPathName()) || !Clean(X->GetClass()->GetPathName())) { return false; }
        auto Row = MakeShared<FJsonObject>(); Objects->SetObjectField(X->GetPathName(), Row); Row->SetObjectField(TEXT("identity"), Identity(X)); auto F = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("fields"), F);
        if (!Fields(X, X == B, X == P, F)) { return false; }
    }
    TArray<UClass*> Classes = { AActor::StaticClass(), APawn::StaticClass() }; Classes.AddUnique(B->ParentClass);
    for (UClass* Class : Classes)
    {
        if (!Loaded(Class) || Class->GetOutermost()->GetName() != TEXT("/Script/Engine") || !Class->HasAnyClassFlags(CLASS_Native)) { return false; }
        auto Row = Identity(Class); O.Source->SetObjectField(Class->GetPathName(), Row); Row->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint64>(Class->GetClassFlags()))); Row->SetBoolField(TEXT("package_dirty"), Class->GetOutermost()->IsDirty());
        UObject* Default = Class->GetDefaultObject(false); Row->SetBoolField(TEXT("existing_cdo_available"), Default != nullptr);
        if (Default) { if (!Loaded(Default)) { return false; } Row->SetObjectField(TEXT("cdo_identity"), Identity(Default)); auto F = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("cdo_fields"), F); if (!Fields(Default, false, false, F)) { return false; } }
    }
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(C.Package, Filename, FPackageName::GetAssetPackageExtension())) { return false; } Filename = FPaths::ConvertRelativePathToFull(Filename);
    const int64 Size = IFileManager::Get().FileSize(*Filename); O.Disk->SetStringField(TEXT("path"), Filename); O.Disk->SetBoolField(TEXT("exists"), Size >= 0); O.Disk->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size)); O.Disk->SetStringField(TEXT("sha1"), FString());
    if (Size > 67108864) { return false; } if (Size >= 0) { TArray<uint8> Bytes; if (!FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; } uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); O.Disk->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); }
    int32 Packages = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == C.Package) { continue; } if (++Packages > 8192) { return false; } O.Other->SetBoolField(It->GetName(), It->IsDirty()); }
    O.Complete = true; J->SetBoolField(TEXT("complete"), true); return true;
}
bool Prepare(const FString& Requested, FContext& C, FResult& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("pawn_editor_context_required"), TEXT("Editor game thread without PIE/SIE required.")); }
    if (Requested.IsEmpty() || Requested.Len() > 2048 || HasNul(Requested)) { return Fail(R, TEXT("pawn_blueprint_path_invalid"), TEXT("Bounded original Blueprint name or Game object/package path required.")); }
    C.Path = FPackageName::ExportTextPathToObjectPath(Requested).TrimStartAndEnd(); C.Path.TrimQuotesInline(); C.Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!C.Path.Contains(TEXT("/")) && !C.Path.Contains(TEXT("."))) { C.Path = TEXT("/Game/Blueprints/") + C.Path; }
    if (!C.Path.Contains(TEXT("."))) { C.Path += TEXT(".") + FPackageName::GetShortName(C.Path); } C.Package = FPackageName::ObjectPathToPackageName(C.Path);
    if (C.Path.Len() >= 1024 || !FPackageName::IsValidObjectPath(C.Path) || !C.Package.StartsWith(TEXT("/Game/")) || C.Path.Contains(TEXT(":")) || !Clean(C.Path)) { return Fail(R, TEXT("pawn_blueprint_path_invalid"), TEXT("Exact Game Blueprint identity required; global name search/level Blueprint resolution remains incomplete.")); }
    auto Call = MakeShared<FJsonObject>(); Call->SetStringField(TEXT("asset_path"), C.Path); Call->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::LoadAsset")); Call->SetBoolField(TEXT("attempted"), true); Call->SetBoolField(TEXT("returned"), false);
    R.Data->SetArrayField(TEXT("asset_load_calls"), { MakeShared<FJsonValueObject>(Call) }); Count(R, TEXT("asset_load_count")); R.Stage = TEXT("load_blueprint"); UObject* Asset = UEditorAssetLibrary::LoadAsset(C.Path); Call->SetBoolField(TEXT("returned"), true); Call->SetObjectField(TEXT("returned_object"), Identity(Asset)); C.Blueprint.Reset(Cast<UBlueprint>(Asset));
    UBlueprint* B = C.Blueprint.Get(); R.Data->SetStringField(TEXT("asset_path"), C.Path); R.Data->SetStringField(TEXT("change_accounting_scope"), TEXT("Selected authoring observations begin after LoadAsset; load/PostLoad and implicit library/class/default initialization are separately disclosed, not certified absent by changed=False."));
    if (!Loaded(B)) { return Fail(R, TEXT("pawn_blueprint_not_found"), FString::Printf(TEXT("Blueprint not found: %s"), *Requested), true); }
    if (B->GetClass() != UBlueprint::StaticClass() || B->BlueprintType != BPTYPE_Normal || B->GetOuter() != B->GetOutermost() || B->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        || B->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_PlayInEditor | PKG_EditorOnly) || !(B->ParentClass == APawn::StaticClass() || B->ParentClass == ACharacter::StaticClass())
        || !Loaded(B->GeneratedClass) || B->GeneratedClass->ClassGeneratedBy != B || B->GeneratedClass->GetSuperClass() != B->ParentClass || B->bBeingCompiled || B->bQueuedForCompilation || B->bSuppressStructurallyModified || B->bIsRegeneratingOnLoad || B->Status == BS_BeingCreated)
    { return Fail(R, TEXT("pawn_blueprint_scope_unsupported"), TEXT("Current normal direct Engine Pawn/Character Blueprint required; original broader native/generated/level variants remain incomplete.")); }
    APawn* P = Cast<APawn>(B->GeneratedClass->GetDefaultObject(false)); C.CDO.Reset(P);
    if (!Loaded(P) || P->GetClass() != B->GeneratedClass || P->GetOuter() != B->GeneratedClass->GetOuter() || !P->HasAnyFlags(RF_ClassDefaultObject)) { return Fail(R, TEXT("pawn_cdo_unavailable"), TEXT("Existing current own generated Pawn CDO required; no default-object warm/create.")); }
    for (TObjectIterator<UBlueprintGeneratedClass> It; It; ++It) { if (Loaded(*It) && *It != B->GeneratedClass && *It != B->SkeletonGeneratedClass && It->IsChildOf(B->GeneratedClass)) { return Fail(R, TEXT("pawn_derived_class_unsupported"), TEXT("Loaded derived class propagation is outside the selected callback scope.")); } }
    for (TObjectIterator<APawn> It; It; ++It) { if (Loaded(*It) && !It->IsTemplate() && It->IsA(B->GeneratedClass)) { return Fail(R, TEXT("pawn_live_instance_unsupported"), TEXT("Loaded selected live Pawn instance is outside defaults-only scope.")); } }
    const auto Auto = FindFProperty<FByteProperty>(P->GetClass(), PropertyNames[0]);
    if (!Auto || Auto->GetOwnerStruct() != APawn::StaticClass() || !Auto->GetIntPropertyEnum() || Auto->GetIntPropertyEnum()->GetPathName() != TEXT("/Script/Engine.EAutoReceiveInput")) { return Fail(R, TEXT("pawn_property_layout_unsupported"), TEXT("Exact public native Pawn enum layout required.")); }
    for (int32 I = 1; I < 5; ++I) { const auto Flag = FindFProperty<FBoolProperty>(P->GetClass(), PropertyNames[I]); if (!Flag || Flag->ArrayDim != 1 || Flag->GetOwnerStruct() != (I == 4 ? AActor::StaticClass() : APawn::StaticClass())) { return Fail(R, TEXT("pawn_property_layout_unsupported"), TEXT("Exact native Pawn/Actor boolean field layout required.")); } }
    return true;
}
struct FInput { bool Present = false, Valid = true, Flag = false; uint8 Byte = 0; FString Error; };
FInput OptionalBool(const FString& Json)
{
    FInput I; TArray<TSharedPtr<FJsonValue>> Values;
    if (Json.Len() > 64 || HasNul(Json) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Values) || Values.Num() != 1 || !Values[0].IsValid()) { I.Present = true; I.Valid = false; I.Error = TEXT("Optional Pawn flag requires one JSON null or exact boolean"); return I; }
    I.Present = Values[0]->Type != EJson::Null; I.Valid = !I.Present || Values[0]->Type == EJson::Boolean;
    if (!I.Valid) { I.Error = TEXT("Optional Pawn flag requires one JSON null or exact boolean"); } else if (I.Present) { I.Flag = Values[0]->AsBool(); } return I;
}
bool ResolveAuto(const FString& Text, UEnum* Enum, uint8& Out, FString& Error)
{
    if (Text.Len() > 128 || HasNul(Text)) { Error = TEXT("AutoPossessPlayer enum string exceeds the finite input bound"); return false; }
    if (Text.IsNumeric())
    {
        double Number = 0; if (!LexTryParseString(Number, *Text) || !FMath::IsFinite(Number) || Number < MIN_int32 || Number > MAX_int32) { Error = TEXT("Numeric enum string must remain within the original int32 conversion budget"); return false; }
        Out = static_cast<uint8>(FCString::Atoi(*Text)); return true; // Preserve original safe numeric-string byte wrapping/truncation.
    }
    FString Short = Text; if (Short.Contains(TEXT("::"))) { Short.Split(TEXT("::"), nullptr, &Short); }
    int64 Value = Enum->GetValueByNameString(Short); if (Value == INDEX_NONE) { Value = Enum->GetValueByNameString(Text); }
    if (Value == INDEX_NONE || Value < 0 || Value > MAX_uint8) { Error = FString::Printf(TEXT("Could not find enum value for '%s'"), *Short); return false; } Out = static_cast<uint8>(Value); return true;
}
}
Variable::FResult SetProperties(const FString& Path, const FString& Auto, const FString& Yaw, const FString& Pitch, const FString& Roll, const FString& Damage)
{
    FResult R; Init(R); R.Data->SetStringField(TEXT("blueprint"), Path); R.Data->SetStringField(TEXT("blueprint_name"), Path);
    FInput Inputs[5]; Inputs[0].Present = !Auto.IsEmpty(); Inputs[1] = OptionalBool(Yaw); Inputs[2] = OptionalBool(Pitch); Inputs[3] = OptionalBool(Roll); Inputs[4] = OptionalBool(Damage);
    int32 Requested = 0; for (const auto& I : Inputs) { Requested += I.Present ? 1 : 0; } R.Data->SetNumberField(TEXT("property_request_count"), Requested);
    if (!Requested) { R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("no_properties"); R.Data->SetStringField(TEXT("message"), TEXT("No properties specified to set")); R.Data->SetBoolField(TEXT("legacy_success"), true); return R; }
    FContext C; if (!Prepare(Path, C, R)) { return R; } FObservation Before, After;
    R.Stage = TEXT("before_observation"); if (!Capture(C, Before, R, TEXT("before_observation"))) { Fail(R, TEXT("pawn_before_unobservable"), TEXT("Bounded before prefix is incomplete; no property application."), true); return R; }
    auto Desired = Values(C.CDO.Get()); auto Results = R.Data->GetObjectField(TEXT("results")); TArray<TSharedPtr<FJsonValue>> Calls; int32 Accepted = 0, Failed = 0;
    const FScopedTransaction Outer(NSLOCTEXT("CubelessMCP", "PawnProperties", "MCP set_pawn_properties")); Count(R, TEXT("outer_transaction_calls"));
    for (int32 I = 0; I < 5; ++I)
    {
        if (!Inputs[I].Present) { continue; } auto Row = MakeShared<FJsonObject>(); Results->SetObjectField(PropertyNames[I], Row); Row->SetBoolField(TEXT("success"), false);
        auto Call = MakeShared<FJsonObject>(); Calls.Add(MakeShared<FJsonValueObject>(Call)); R.Data->SetArrayField(TEXT("property_calls"), Calls); Call->SetNumberField(TEXT("order"), I); Call->SetStringField(TEXT("property"), PropertyNames[I]); Call->SetBoolField(TEXT("setter_attempted"), false); Call->SetBoolField(TEXT("setter_returned"), false);
        if (!I) { Inputs[I].Valid = ResolveAuto(Auto, FindFProperty<FByteProperty>(C.CDO->GetClass(), PropertyNames[0])->GetIntPropertyEnum(), Inputs[I].Byte, Inputs[I].Error); Call->SetStringField(TEXT("requested"), Auto); }
        else { Call->SetStringField(TEXT("requested_json"), I == 1 ? Yaw : I == 2 ? Pitch : I == 3 ? Roll : Damage); }
        if (!Inputs[I].Valid) { ++Failed; Count(R, TEXT("property_validation_failures")); Row->SetStringField(TEXT("error"), Inputs[I].Error); Call->SetStringField(TEXT("error"), Inputs[I].Error); continue; }
        R.Stage = TEXT("set_properties"); R.bMutationAttempted = true; R.bChangeStateKnown = false; Call->SetBoolField(TEXT("setter_attempted"), true); Count(R, TEXT("setter_attempt_count"));
        if (I == 0) { C.CDO->AutoPossessPlayer = static_cast<EAutoReceiveInput::Type>(Inputs[I].Byte); Desired->SetNumberField(PropertyNames[I], Inputs[I].Byte); }
        else { const bool V = Inputs[I].Flag; if (I == 1) { C.CDO->bUseControllerRotationYaw = V; } else if (I == 2) { C.CDO->bUseControllerRotationPitch = V; } else if (I == 3) { C.CDO->bUseControllerRotationRoll = V; } else { C.CDO->SetCanBeDamaged(V); } Desired->SetBoolField(PropertyNames[I], V); }
        Call->SetBoolField(TEXT("setter_returned"), true); Count(R, TEXT("setter_return_count")); ++Accepted; Count(R, TEXT("accepted_property_count")); Row->SetBoolField(TEXT("success"), true); Call->SetObjectField(TEXT("selected_values_after_return"), Values(C.CDO.Get()));
    }
    R.Data->SetNumberField(TEXT("failed_property_count"), Failed); R.Data->SetBoolField(TEXT("legacy_success"), Accepted > 0); R.Data->SetBoolField(TEXT("partial_success"), Accepted > 0 && Failed > 0); R.Data->SetBoolField(TEXT("all_requested_properties_succeeded"), Failed == 0); R.Data->SetObjectField(TEXT("desired_selected_values"), Desired);
    if (Accepted)
    {
        R.Stage = TEXT("mark_blueprint_modified"); Count(R, TEXT("mark_blueprint_modified_calls")); FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint.Get()); Count(R, TEXT("mark_blueprint_modified_returns"));
    }
    R.Stage = TEXT("after_observation"); if (!Capture(C, After, R, TEXT("after_observation"))) { Fail(R, TEXT("pawn_after_unobservable"), TEXT("Actual ordered property/mark prefix retained; final bounded observation incomplete, no replay/rollback."), true); return R; }
    R.bChangeStateKnown = true; R.bChanged = !Equal(Before.State, After.State) || !Equal(Before.Source, After.Source) || !Equal(Before.Disk, After.Disk) || !Equal(Before.Other, After.Other);
    const bool State = Equal(Before.State->GetObjectField(TEXT("unselected_owned_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("unselected_owned_fields")).ToSharedRef())
        && Equal(Before.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef()) && Equal(Before.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef());
    const bool Source = Equal(Before.Source, After.Source), Disk = Equal(Before.Disk, After.Disk), Other = Equal(Before.Other, After.Other), Selected = Equal(Desired, After.State->GetObjectField(TEXT("selected_values")).ToSharedRef());
    const bool Callback = Accepted ? After.State->GetBoolField(TEXT("package_dirty")) && After.State->GetNumberField(TEXT("blueprint_status")) == BS_Dirty && !After.State->GetBoolField(TEXT("cached_dependencies_up_to_date"))
        : Before.State->GetBoolField(TEXT("package_dirty")) == After.State->GetBoolField(TEXT("package_dirty")) && Before.State->GetNumberField(TEXT("blueprint_status")) == After.State->GetNumberField(TEXT("blueprint_status")) && Before.State->GetBoolField(TEXT("cached_dependencies_up_to_date")) == After.State->GetBoolField(TEXT("cached_dependencies_up_to_date"));
    R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State); R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("disk_preserved"), Disk); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(TEXT("accepted_values_verified"), Selected);
    R.Data->SetBoolField(TEXT("mark_modified_state_verified"), Callback);
    if (!(State && Source && Disk && Other && Selected && Callback)) { Fail(R, TEXT("pawn_preservation_failed"), TEXT("Complete actual property/callback prefix differs from accepted values or declared preserved scope; no rollback/compile/save.")); return R; }
    if (!Accepted) { Fail(R, TEXT("pawn_no_property_applied"), TEXT("All specified properties failed; original per-property errors retained.")); return R; }
    R.bPostconditionPassed = true; R.bSuccess = true; R.Stage = TEXT("complete"); return R;
}
}
