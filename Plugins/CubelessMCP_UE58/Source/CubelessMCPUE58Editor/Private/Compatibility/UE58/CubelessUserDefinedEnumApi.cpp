#include "Compatibility/UE58/CubelessUserDefinedEnumApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/UserDefinedEnum.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace Cubeless::UE58::UserDefinedEnum
{
namespace
{
constexpr int32 MaxJsonChars = 1024 * 1024;
constexpr int32 MaxExistingEntries = 4096;

bool Forbidden(const FString& Value)
{
    // Preserve legacy MCP authoring-reference refusals and include this Editor extension.
    for (const TCHAR* Token : {TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"),
        TEXT("CubelessControlRigTools"), TEXT("CubelessMCP_UE58"), TEXT("CubelessMCPUE58Editor"),
        TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry")})
    { if (Value.Contains(Token, ESearchCase::IgnoreCase)) { return true; } }
    return false;
}

FString Normalize(FString Path)
{
    Path.TrimStartAndEndInline(); Path.TrimQuotesInline();
    FString Package;
    if (FPackageName::TryConvertFilenameToLongPackageName(Path, Package)) { Path = Package; }
    else if (Path.Contains(TEXT("."))) { Path = FPackageName::ObjectPathToPackageName(Path); }
    Path.TrimStartAndEndInline();
    while (Path.Len() > 1 && Path.EndsWith(TEXT("/"))) { Path.LeftChopInline(1); }
    return Path;
}

bool JsonValue(const FString& Text, TSharedPtr<FJsonValue>& Value)
{
    if (Text.Len() > MaxJsonChars) { return false; }
    // The installed standalone scalar parser does not accept null. Parse one
    // public JSON array element instead, retaining null versus provided [].
    TArray<TSharedPtr<FJsonValue>> One;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Text + TEXT("]")), One)
        || One.Num() != 1 || !One[0].IsValid()) { return false; }
    Value = One[0]; return true;
}

void Count(const TSharedRef<FJsonObject>& Counts, const TCHAR* Key)
{
    Counts->SetNumberField(Key, Counts->GetNumberField(Key) + 1);
}

bool NoForeignAssets(UPackage* Package, const UObject* Allowed, int32& Exports)
{
    if (!IsValid(Package) || !Package->GetExternalPackages().IsEmpty()) { return false; }
    TArray<UObject*> Objects;
    GetObjectsWithPackage(Package, Objects, EGetObjectsFlags::IncludeNestedObjects, RF_Transient, EInternalObjectFlags::Garbage);
    Exports = Objects.Num(); if (Exports > 4096) { return false; }
    for (const UObject* Object : Objects)
    { if (IsValid(Object) && Object != Allowed && Object->IsAsset()) { return false; } }
    return true;
}

TArray<TSharedPtr<FJsonValue>> Entries(const UUserDefinedEnum* Enum, bool bIncludeMax = false)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    const int32 N = FMath::Max(Enum->NumEnums() - (bIncludeMax ? 0 : 1), 0);
    for (int32 I = 0; I < N; ++I)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("index"), I);
        Row->SetNumberField(TEXT("value"), static_cast<double>(Enum->GetValueByIndex(I)));
        Row->SetStringField(TEXT("internal_name"), Enum->GetNameStringByIndex(I));
        Row->SetStringField(TEXT("full_name"), Enum->GetNameByIndex(I).ToString());
        Row->SetStringField(TEXT("display_name"), Enum->GetDisplayNameTextByIndex(I).ToString());
#if WITH_METADATA
        Row->SetStringField(TEXT("tooltip"), Enum->HasMetaData(TEXT("ToolTip"), I) ? Enum->GetMetaData(TEXT("ToolTip"), I, false) : FString());
#else
        Row->SetStringField(TEXT("tooltip"), FString());
#endif
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    return Rows;
}

bool Verify(UUserDefinedEnum* Enum, const TArray<FString>& Labels, const TArray<FString>& Tooltips)
{
    const int32 N = Labels.Num();
    if (!IsValid(Enum) || Enum->GetClass() != UUserDefinedEnum::StaticClass() || Enum->NumEnums() != N + 1
        || Enum->GetCppForm() != UEnum::ECppForm::Namespaced || Enum->GetUnderlyingType() != UEnum::EUnderlyingType::uint8
        || Enum->GetValueByIndex(N) != N || !Enum->GetNameStringByIndex(N).EndsWith(TEXT("_MAX"), ESearchCase::CaseSensitive)) { return false; }
    for (int32 I = 0; I < N; ++I)
    {
        if (Enum->GetValueByIndex(I) != I || Enum->GetNameStringByIndex(I) != FString::Printf(TEXT("NewEnumerator%d"), I)
            || Enum->GetDisplayNameTextByIndex(I).ToString() != Labels[I]) { return false; }
#if WITH_METADATA
        const FString Tooltip = Enum->HasMetaData(TEXT("ToolTip"), I) ? Enum->GetMetaData(TEXT("ToolTip"), I, false) : FString();
        if (Tooltip != Tooltips[I]) { return false; }
#else
        if (!Tooltips[I].IsEmpty()) { return false; }
#endif
    }
    return true;
}
}

Variable::FResult Create(const FString& RawAssetPath, const FString& DisplayNamesJson,
    const FString& TooltipsJson, bool bDryRun, bool bSave, bool bSkipIfExists)
{
    Variable::FResult R; auto J = R.Data;
    const double Began = FPlatformTime::Seconds();
    R.bSaveRequested = bSave;
    J->SetStringField(TEXT("operation"), TEXT("create_user_defined_enum"));
    J->SetBoolField(TEXT("dry_run"), bDryRun); J->SetBoolField(TEXT("save_requested"), bSave);
    J->SetBoolField(TEXT("skip_if_exists"), bSkipIfExists);
    for (const TCHAR* Key : {TEXT("created"), TEXT("skipped_existing"), TEXT("saved"), TEXT("verification_pass"),
        TEXT("observed_explicit_change"), TEXT("cleanup_attempted"), TEXT("atomic_rollback_available"),
        TEXT("auto_retry_safe"), TEXT("cold_reload_verified"), TEXT("packaged_runtime_verified")}) { J->SetBoolField(Key, false); }
    J->SetBoolField(TEXT("total_change_state_known"), true);
    J->SetBoolField(TEXT("explicit_compile_flags_prove_all_implicit_compiles"), false);
    auto Counts = MakeShared<FJsonObject>();
    for (const TCHAR* Key : {TEXT("asset_exists_calls"), TEXT("asset_load_calls"), TEXT("package_create_calls"), TEXT("package_create_returns"),
        TEXT("enum_factory_calls"), TEXT("enum_factory_returns"), TEXT("set_enums_calls"), TEXT("ensure_display_names_calls"),
        TEXT("post_edit_change_calls"), TEXT("mark_package_dirty_calls"), TEXT("asset_created_calls"),
        TEXT("explicit_compile_calls"), TEXT("explicit_save_calls"), TEXT("explicit_save_returns"),
        TEXT("cleanup_rename_calls"), TEXT("cleanup_set_dirty_calls"), TEXT("file_delete_calls")}) { Counts->SetNumberField(Key, 0); }
    J->SetObjectField(TEXT("native_call_counts"), Counts);
    auto Failure = [&](const TCHAR* Code, const FString& Error) -> Variable::FResult
    {
        R.bSuccess = false; R.bPostconditionPassed = false; R.ErrorCode = Code; R.Error = Error;
        J->SetBoolField(TEXT("total_change_state_known"), R.bChangeStateKnown);
        J->SetBoolField(TEXT("mutation_state_unknown"), !R.bChangeStateKnown);
        J->SetBoolField(TEXT("partial_mutation"), R.bChanged || !R.bChangeStateKnown);
        J->SetBoolField(TEXT("do_not_retry"), R.bMutationAttempted || !R.bChangeStateKnown);
        J->SetStringField(TEXT("recovery_guidance"), TEXT("Inspect retained object/package/entries/file/save/cleanup receipts before a new request. No automatic retry or atomic rollback."));
        FString TargetObject, TargetPackage, TargetFile;
        if (J->TryGetStringField(TEXT("expected_object_path"), TargetObject))
        {
            const UObject* Actual = FindObject<UObject>(nullptr, *TargetObject);
            J->SetBoolField(TEXT("actual_target_object_observed_on_failure"), IsValid(Actual));
            if (IsValid(Actual))
            { J->SetStringField(TEXT("actual_target_class_on_failure"), Actual->GetClass()->GetPathName()); J->SetNumberField(TEXT("actual_target_flags_on_failure"), static_cast<double>(Actual->GetFlags())); }
        }
        if (J->TryGetStringField(TEXT("asset_path"), TargetPackage))
        {
            const UPackage* Actual = FindPackage(nullptr, *TargetPackage);
            J->SetBoolField(TEXT("actual_package_observed_on_failure"), IsValid(Actual));
            if (IsValid(Actual)) { J->SetBoolField(TEXT("actual_package_dirty_on_failure"), Actual->IsDirty()); }
        }
        if (J->TryGetStringField(TEXT("exact_package_filename"), TargetFile))
        { J->SetBoolField(TEXT("actual_file_exists_on_failure"), FPaths::FileExists(TargetFile)); J->SetNumberField(TEXT("actual_file_bytes_on_failure"), static_cast<double>(IFileManager::Get().FileSize(*TargetFile))); }
        J->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Began);
        return R;
    };
    if (!IsEditorGameThread() || IsEngineExitRequested() || IsPlaySessionActive())
    { return Failure(TEXT("user_defined_enum_editor_unavailable"), TEXT("A live idle Editor game thread outside PIE/SIE is required")); }
    if (RawAssetPath.IsEmpty() || RawAssetPath.Len() > MaxJsonChars || Forbidden(RawAssetPath))
    { return Failure(TEXT("user_defined_enum_path_invalid"), TEXT("A valid non-MCP /Game enum asset path is required")); }
    const FString Path = Normalize(RawAssetPath);
    const FString Name = FPackageName::GetLongPackageAssetName(Path);
    FText NameError;
    if (Path.Len() >= NAME_SIZE || !FPackageName::IsValidLongPackageName(Path) || !Path.StartsWith(TEXT("/Game/"))
        || FPackageName::GetLongPackagePath(Path).IsEmpty() || Name.IsEmpty() || FName(*Name).IsNone()
        || !FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS, &NameError) || Forbidden(Path))
    { return Failure(TEXT("user_defined_enum_path_invalid"), FString::Printf(TEXT("asset_path must be a valid /Game package path including an asset name: %s"), *Path)); }
    J->SetStringField(TEXT("asset_path"), Path);
    TSharedPtr<FJsonValue> LabelValue, TooltipValue;
    if (!JsonValue(DisplayNamesJson, LabelValue) || LabelValue->Type != EJson::Array
        || LabelValue->AsArray().Num() < 1 || LabelValue->AsArray().Num() > 32)
    { return Failure(TEXT("user_defined_enum_labels_invalid"), TEXT("display_names must be an array of 1..32 strings")); }
    TArray<FString> Labels, Tooltips; TSet<FString> Seen;
    for (int32 I = 0; I < LabelValue->AsArray().Num(); ++I)
    {
        const auto& Value = LabelValue->AsArray()[I];
        if (!Value.IsValid() || Value->Type != EJson::String)
        { return Failure(TEXT("user_defined_enum_labels_invalid"), FString::Printf(TEXT("display_names[%d] must be a string"), I)); }
        FString Label = Value->AsString().TrimStartAndEnd();
        if (Label.IsEmpty() || Seen.Contains(Label.ToLower()))
        { return Failure(TEXT("user_defined_enum_labels_invalid"), FString::Printf(TEXT("display_names[%d] must be nonempty and unique after trim, case-insensitively"), I)); }
        Seen.Add(Label.ToLower()); Labels.Add(MoveTemp(Label));
    }
    Tooltips.SetNum(Labels.Num());
    if (!JsonValue(TooltipsJson, TooltipValue) || (TooltipValue->Type != EJson::Null && TooltipValue->Type != EJson::Array))
    { return Failure(TEXT("user_defined_enum_tooltips_invalid"), TEXT("tooltips must be null or an array when provided")); }
    J->SetBoolField(TEXT("tooltips_were_null"), TooltipValue->Type == EJson::Null);
    if (TooltipValue->Type == EJson::Array)
    {
        if (TooltipValue->AsArray().Num() != Labels.Num())
        { return Failure(TEXT("user_defined_enum_tooltips_invalid"), TEXT("tooltips must contain exactly the same number of entries as display_names")); }
        for (int32 I = 0; I < Tooltips.Num(); ++I)
        {
            const auto& Value = TooltipValue->AsArray()[I];
            if (!Value.IsValid() || Value->Type != EJson::String)
            { return Failure(TEXT("user_defined_enum_tooltips_invalid"), FString::Printf(TEXT("tooltips[%d] must be a string"), I)); }
            Tooltips[I] = Value->AsString();
        }
    }
    FString Filename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(Path, Filename, FPackageName::GetAssetPackageExtension()))
    { return Failure(TEXT("user_defined_enum_filename_invalid"), TEXT("Exact package filename could not be resolved")); }
    Filename = FPaths::ConvertRelativePathToFull(Filename); FPaths::NormalizeFilename(Filename);
    FString Content = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()); FPaths::NormalizeDirectoryName(Content);
    if (!FPaths::IsUnderDirectory(Filename, Content))
    { return Failure(TEXT("user_defined_enum_filename_invalid"), TEXT("Target /Game filename must remain under this project's Content directory")); }
    const bool FileBefore = FPaths::FileExists(Filename);
    const FString ObjectPath = Path + TEXT(".") + Name;
    J->SetStringField(TEXT("expected_object_path"), ObjectPath); J->SetStringField(TEXT("exact_package_filename"), Filename);
    J->SetBoolField(TEXT("file_existed_before"), FileBefore);
    TStrongObjectPtr<UEditorEngine> EditorPin(GEditor);
    UEditorAssetSubsystem* Assets = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!IsValid(Assets)) { return Failure(TEXT("user_defined_enum_editor_unavailable"), TEXT("EditorAssetSubsystem unavailable")); }
    TStrongObjectPtr<UEditorAssetSubsystem> AssetsPin(Assets);
    auto CurrentEditor = [&]() { return IsEditorGameThread() && !IsEngineExitRequested() && !IsPlaySessionActive()
        && IsValid(EditorPin.Get()) && GEditor == EditorPin.Get()
        && IsValid(AssetsPin.Get()) && GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() == AssetsPin.Get(); };

    // AssetRegistry/load/factory/notification effects are not an atomic or pure operation.
    R.bChangeStateKnown = false; J->SetBoolField(TEXT("total_change_state_known"), false);
    R.Stage = TEXT("existing_asset_resolution"); Count(Counts, TEXT("asset_exists_calls"));
    const bool RegistryExists = AssetsPin->DoesAssetExist(Path);
    J->SetBoolField(TEXT("registry_asset_existed"), RegistryExists);
    if (!CurrentEditor()) { return Failure(TEXT("user_defined_enum_editor_changed"), TEXT("Editor ownership changed during existence query")); }
    UObject* Existing = FindObject<UObject>(nullptr, *ObjectPath);
    if (RegistryExists)
    {
        Count(Counts, TEXT("asset_load_calls")); Existing = AssetsPin->LoadAsset(Path);
        if (!CurrentEditor()) { return Failure(TEXT("user_defined_enum_editor_changed"), TEXT("Editor ownership changed during existing asset load")); }
    }
    if (RegistryExists || Existing)
    {
        if (!bSkipIfExists) { return Failure(TEXT("user_defined_enum_already_exists"), TEXT("Asset already exists; create_user_defined_enum never overwrites existing assets: ") + Path); }
        UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(Existing);
        if (!IsValid(Enum) || Enum->GetPathName() != ObjectPath || Enum->NumEnums() < 1 || Enum->NumEnums() - 1 > MaxExistingEntries)
        { return Failure(TEXT("user_defined_enum_existing_type_invalid"), TEXT("Existing asset is not a bounded live UserDefinedEnum at the exact target; skip_if_exists cannot satisfy the contract")); }
        TStrongObjectPtr<UUserDefinedEnum> ExistingPin(Enum);
        J->SetBoolField(TEXT("skipped_existing"), true); J->SetStringField(TEXT("object_path"), Enum->GetPathName());
        J->SetStringField(TEXT("class"), Enum->GetClass()->GetName()); J->SetNumberField(TEXT("entry_count"), Enum->NumEnums() - 1);
        J->SetArrayField(TEXT("entries"), Entries(Enum)); J->SetBoolField(TEXT("verification_pass"), true);
        J->SetStringField(TEXT("readback_source"), TEXT("existing_asset; requested_labels_not_compared_as_in_original"));
        if (!CurrentEditor() || !IsValid(Enum) || FindObject<UObject>(nullptr, *ObjectPath) != Enum)
        { return Failure(TEXT("user_defined_enum_existing_changed"), TEXT("Existing enum identity changed while reading; no save or retry")); }
        R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("existing_user_defined_enum_readback");
        J->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Began); return R;
    }
    if (FileBefore || FPaths::FileExists(Filename))
    { return Failure(TEXT("user_defined_enum_unregistered_file_collision"), TEXT("A target package file exists without a resolvable asset; no overwrite or automatic repair")); }
    if (bDryRun)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 I = 0; I < Labels.Num(); ++I)
        {
            auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("index"), I); Row->SetNumberField(TEXT("value"), I);
            const FString Internal = FString::Printf(TEXT("NewEnumerator%d"), I);
            Row->SetStringField(TEXT("internal_name"), Internal); Row->SetStringField(TEXT("full_name"), Name + TEXT("::") + Internal);
            Row->SetStringField(TEXT("display_name"), Labels[I]); Row->SetStringField(TEXT("tooltip"), Tooltips[I]); Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        J->SetBoolField(TEXT("can_create"), true); J->SetNumberField(TEXT("entry_count"), Labels.Num());
        J->SetArrayField(TEXT("entries"), Rows); J->SetStringField(TEXT("readback_source"), TEXT("planned")); J->SetBoolField(TEXT("verification_pass"), true);
        R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("user_defined_enum_dry_run_complete");
        J->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Began); return R;
    }

    R.Stage = TEXT("create_enum_package");
    UPackage* BeforePackage = FindPackage(nullptr, *Path);
    const bool PackageExisted = BeforePackage != nullptr;
    J->SetBoolField(TEXT("package_existed_before_create"), PackageExisted);
    if (BeforePackage && !IsValid(BeforePackage))
    { return Failure(TEXT("user_defined_enum_package_invalid"), TEXT("Target package exists in an invalid state")); }
    TStrongObjectPtr<UPackage> BeforePackagePin(BeforePackage);
    if (IsValid(BeforePackage)) { J->SetBoolField(TEXT("package_dirty_before_create"), BeforePackage->IsDirty()); }
    int32 Exports = 0;
    if (BeforePackage && !NoForeignAssets(BeforePackage, nullptr, Exports))
    { return Failure(TEXT("user_defined_enum_package_collision"), TEXT("Existing package has another asset or external packages; no overwrite")); }
    J->SetNumberField(TEXT("package_exports_before_create"), Exports);
    R.bMutationAttempted = true; Count(Counts, TEXT("package_create_calls"));
    UPackage* Package = CreatePackage(*Path); Count(Counts, TEXT("package_create_returns"));
    UPackage* ObservedPackage = FindPackage(nullptr, *Path);
    J->SetBoolField(TEXT("package_observed_after_create_call"), IsValid(ObservedPackage));
    if (!CurrentEditor() || !IsValid(Package) || Package != ObservedPackage || Package->GetName() != Path || (PackageExisted && Package != BeforePackagePin.Get()))
    { J->SetBoolField(TEXT("package_cleanup_attempted"), false); return Failure(TEXT("user_defined_enum_package_create_failed"), TEXT("Package creation/identity was not confirmed; actual lookup retained, no guessed destruction or retry")); }
    TStrongObjectPtr<UPackage> PackagePin(Package);
    R.bChanged = !PackageExisted; J->SetBoolField(TEXT("observed_explicit_change"), R.bChanged);
    auto Current = [&](UUserDefinedEnum* Enum) { return CurrentEditor() && IsValid(PackagePin.Get())
        && Package->GetName() == Path && FindPackage(nullptr, *Path) == Package && IsValid(Enum)
        && Package->GetExternalPackages().IsEmpty() && Enum->GetPackage() == Package
        && Enum->GetOuter() == Package && Enum->GetPathName() == ObjectPath && FindObject<UObject>(nullptr, *ObjectPath) == Enum; };
    if (!NoForeignAssets(Package, nullptr, Exports) || FindObject<UObject>(nullptr, *ObjectPath) || FPaths::FileExists(Filename))
    { return Failure(TEXT("user_defined_enum_package_changed"), TEXT("Package contents or target file changed before factory; no overwrite")); }
    R.Stage = TEXT("create_enum_asset"); Count(Counts, TEXT("enum_factory_calls"));
    UEnum* FactoryResult = FEnumEditorUtils::CreateUserDefinedEnum(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
    Count(Counts, TEXT("enum_factory_returns"));
    J->SetBoolField(TEXT("factory_returned_valid_object"), IsValid(FactoryResult));
    if (IsValid(FactoryResult))
    { J->SetStringField(TEXT("factory_result_path"), FactoryResult->GetPathName()); J->SetStringField(TEXT("factory_result_class"), FactoryResult->GetClass()->GetPathName()); }
    UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(FactoryResult);
    if (!Current(Enum) || Enum->GetClass() != UUserDefinedEnum::StaticClass())
    { J->SetBoolField(TEXT("package_retained_after_factory_failure"), IsValid(Package)); J->SetBoolField(TEXT("package_cleanup_attempted"), false); return Failure(TEXT("user_defined_enum_factory_failed"), TEXT("Factory did not return the exact new stock UserDefinedEnum; package/object prefix retained without blind cleanup")); }
    TStrongObjectPtr<UUserDefinedEnum> EnumPin(Enum);
    R.bChanged = true; J->SetBoolField(TEXT("created"), true); J->SetBoolField(TEXT("observed_explicit_change"), true);
    J->SetStringField(TEXT("object_path"), ObjectPath); J->SetStringField(TEXT("class"), Enum->GetClass()->GetName());
    J->SetStringField(TEXT("stock_class_path"), Enum->GetClass()->GetPathName()); J->SetBoolField(TEXT("stock_engine_class_verified"), true);
    auto Cleanup = [&]()
    {
        auto Receipt = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("cleanup_receipt"), Receipt); J->SetBoolField(TEXT("cleanup_attempted"), true);
        Receipt->SetBoolField(TEXT("atomic_rollback"), false); Receipt->SetBoolField(TEXT("succeeded"), false);
        if (!Current(Enum) || Enum->HasAnyFlags(RF_WasLoaded) || Enum->IsRooted() || !NoForeignAssets(Package, Enum, Exports)
            || FPaths::FileExists(Filename))
        { Receipt->SetStringField(TEXT("error"), TEXT("Fresh exact unpublished asset-only cleanup conditions unavailable; object retained")); return; }
        Receipt->SetStringField(TEXT("object_path_before"), Enum->GetPathName());
        Receipt->SetNumberField(TEXT("object_flags_before"), static_cast<double>(Enum->GetFlags()));
        Receipt->SetNumberField(TEXT("raw_entry_count_before_cleanup"), Enum->NumEnums());
        if (Enum->NumEnums() <= 33) { Receipt->SetArrayField(TEXT("raw_entries_before_cleanup"), Entries(Enum, true)); }
        Enum->ClearFlags(RF_Public | RF_Standalone); Enum->SetFlags(RF_Transient);
        Count(Counts, TEXT("cleanup_rename_calls"));
        const bool Renamed = Enum->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
        Receipt->SetBoolField(TEXT("rename_returned"), Renamed);
        if (IsValid(Enum))
        { Receipt->SetStringField(TEXT("object_path_after"), Enum->GetPathName()); Receipt->SetNumberField(TEXT("object_flags_after"), static_cast<double>(Enum->GetFlags())); }
        const bool Moved = CurrentEditor() && IsValid(Enum) && Enum->GetOutermost() == GetTransientPackage()
            && FindObject<UObject>(nullptr, *ObjectPath) != Enum;
        Receipt->SetBoolField(TEXT("new_enum_moved_to_transient_observed"), Moved);
        if (!Renamed || !Moved || !IsValid(Package) || FindPackage(nullptr, *Path) != Package || !NoForeignAssets(Package, nullptr, Exports))
        { Receipt->SetStringField(TEXT("error"), TEXT("Transient rename/package ownership was not confirmed; no further cleanup")); return; }
        Receipt->SetNumberField(TEXT("package_nontransient_exports_after_rename"), Exports);
        // IsAsset() is false for non-public exports and nested subobjects. A
        // new package is not ours to mark clean unless all observed exports left.
        if (!PackageExisted && Exports != 0)
        {
            Receipt->SetBoolField(TEXT("package_dirty_clear_skipped_for_unowned_exports"), true);
            Receipt->SetBoolField(TEXT("empty_package_retained"), false);
            Receipt->SetBoolField(TEXT("package_dirty_after"), Package->IsDirty());
            Receipt->SetStringField(TEXT("error"), TEXT("Nontransient objects remain in the new package; dirty state retained without further cleanup"));
            return;
        }
        // The legacy helper blindly cleared dirty on every package. Only clear
        // our new, still-empty, unpersisted package; preserve a pre-existing one.
        if (!PackageExisted && !FPaths::FileExists(Filename))
        { Count(Counts, TEXT("cleanup_set_dirty_calls")); Package->SetDirtyFlag(false); }
        const bool Confirmed = CurrentEditor() && IsValid(Package) && FindPackage(nullptr, *Path) == Package
            && NoForeignAssets(Package, nullptr, Exports) && (PackageExisted || Exports == 0) && !FPaths::FileExists(Filename);
        Receipt->SetNumberField(TEXT("last_observed_package_nontransient_exports"), Exports);
        Receipt->SetBoolField(TEXT("succeeded"), Confirmed);
        Receipt->SetBoolField(TEXT("empty_package_retained"), Confirmed && Exports == 0);
        Receipt->SetBoolField(TEXT("package_dirty_clear_skipped_for_preexisting_package"), PackageExisted);
        if (IsValid(Package)) { Receipt->SetBoolField(TEXT("package_dirty_after"), Package->IsDirty()); }
        // Rename/notification/global enum tables may have effects beyond this
        // observed structural move. Total change remains unknown after cleanup.
    };
    R.Stage = TEXT("assign_enum_entries"); TArray<TPair<FName, int64>> Names;
    for (int32 I = 0; I < Labels.Num(); ++I)
    { Names.Emplace(FName(*Enum->GenerateFullEnumName(*FString::Printf(TEXT("NewEnumerator%d"), I))), I); }
    Count(Counts, TEXT("set_enums_calls"));
    const bool Assigned = Enum->SetEnums(Names, UEnum::ECppForm::Namespaced, UEnum::EUnderlyingType::uint8, EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::Yes);
    J->SetBoolField(TEXT("set_enums_returned"), Assigned);
    if (!Current(Enum)) { return Failure(TEXT("user_defined_enum_identity_changed"), TEXT("Enum/package identity changed while assigning entries")); }
    if (!Assigned) { Cleanup(); return Failure(TEXT("user_defined_enum_set_entries_failed"), TEXT("Failed to assign enumerators; conditional structural cleanup receipt retained")); }
    Enum->SetMetaData(TEXT("BlueprintType"), TEXT("true")); Enum->DisplayNameMap.Empty(Labels.Num());
    for (int32 I = 0; I < Labels.Num(); ++I)
    {
        Enum->DisplayNameMap.Add(FName(*Enum->GetNameStringByIndex(I)), FText::FromString(Labels[I]));
#if WITH_METADATA
        if (!Tooltips[I].IsEmpty()) { Enum->SetMetaData(TEXT("ToolTip"), *Tooltips[I], I); }
#endif
    }
    Count(Counts, TEXT("ensure_display_names_calls")); FEnumEditorUtils::EnsureAllDisplayNamesExist(Enum);
    R.Stage = TEXT("verify_enum_entries");
    if (!Current(Enum)) { return Failure(TEXT("user_defined_enum_identity_changed"), TEXT("Enum/package identity changed during display-name setup")); }
    J->SetNumberField(TEXT("raw_enum_entry_count"), Enum->NumEnums());
    J->SetNumberField(TEXT("entry_count"), FMath::Max(Enum->NumEnums() - 1, 0)); J->SetArrayField(TEXT("entries"), Entries(Enum));
    if (!Verify(Enum, Labels, Tooltips)) { Cleanup(); return Failure(TEXT("user_defined_enum_entry_readback_failed"), TEXT("Enum values/names/display/tooltips/auto-MAX readback differs; conditional cleanup prefix retained")); }
    J->SetBoolField(TEXT("verification_pass"), true); J->SetBoolField(TEXT("auto_max_verified"), true);
    Enum->SetFlags(RF_Public | RF_Standalone | RF_Transactional); Enum->ClearFlags(RF_Transient);
    R.Stage = TEXT("publish_enum_asset"); Count(Counts, TEXT("post_edit_change_calls")); Enum->PostEditChange();
    if (!Current(Enum)) { return Failure(TEXT("user_defined_enum_identity_changed"), TEXT("PostEditChange changed exact enum/package ownership; created prefix retained")); }
    Count(Counts, TEXT("mark_package_dirty_calls")); const bool DirtyReturn = Package->MarkPackageDirty();
    J->SetBoolField(TEXT("mark_package_dirty_returned"), DirtyReturn);
    if (!Current(Enum)) { return Failure(TEXT("user_defined_enum_identity_changed"), TEXT("MarkPackageDirty callback changed enum/package ownership")); }
    Count(Counts, TEXT("asset_created_calls")); FAssetRegistryModule::AssetCreated(Enum);
    if (!Current(Enum) || !Verify(Enum, Labels, Tooltips))
    { J->SetBoolField(TEXT("verification_pass"), false); return Failure(TEXT("user_defined_enum_publish_unverified"), TEXT("Registry/notification callbacks changed enum contract; no cleanup of a published asset")); }
    J->SetBoolField(TEXT("can_create"), true); J->SetStringField(TEXT("readback_source"), TEXT("created_asset"));
    J->SetArrayField(TEXT("entries"), Entries(Enum)); J->SetBoolField(TEXT("package_dirty"), Package->IsDirty());
    if (bSave)
    {
        R.Stage = TEXT("save_enum_package"); R.bSaveAttempted = true; Count(Counts, TEXT("explicit_save_calls"));
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError; Args.bSlowTask = false;
        const bool SavedReturn = UPackage::SavePackage(Package, Enum, *Filename, Args);
        Count(Counts, TEXT("explicit_save_returns")); J->SetBoolField(TEXT("save_returned"), SavedReturn);
        const bool FileAfter = FPaths::FileExists(Filename); J->SetBoolField(TEXT("file_exists_after_save"), FileAfter);
        const int64 SavedBytes = IFileManager::Get().FileSize(*Filename);
        J->SetNumberField(TEXT("saved_file_bytes_observed"), static_cast<double>(SavedBytes));
        if (!Current(Enum)) { return Failure(TEXT("user_defined_enum_save_outcome_unknown"), TEXT("Save returned but current enum/package identity is unknown; actual return/file prefix retained")); }
        const bool Verified = Verify(Enum, Labels, Tooltips); J->SetBoolField(TEXT("verification_pass"), Verified);
        J->SetBoolField(TEXT("package_dirty"), Package->IsDirty()); J->SetArrayField(TEXT("entries"), Entries(Enum));
        // SavePackage is synchronous (SAVE_Async not requested). Do not erase a
        // dirty callback after return or pretend file presence is a cold reload.
        R.bSaved = SavedReturn && FileAfter && SavedBytes > 0 && !Package->IsDirty() && Verified;
        J->SetBoolField(TEXT("saved"), R.bSaved);
        if (!R.bSaved) { return Failure(TEXT("user_defined_enum_save_failed"), TEXT("UserDefinedEnum was created in memory but a clean verified package save was not confirmed; no cleanup or replay")); }
    }
    R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("user_defined_enum_create_complete");
    J->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Began);
    return R;
}
}
