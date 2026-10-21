#include "Compatibility/UE58/CubelessInputMappingApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "GameFramework/InputSettings.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::InputMapping
{
namespace
{
using Variable::FResult;
constexpr int32 MaxMappings = 16384;
constexpr int64 MaxConfigBytes = 8 * 1024 * 1024;
constexpr const TCHAR* Section = TEXT("/Script/Engine.InputSettings");
struct FFileObservation
{
    bool bKnown = false, bExists = false, bReadOnly = false;
    TArray<uint8> Bytes;
};
void Count(FResult& R, const TCHAR* Key)
{
    auto Counts = R.Data->GetObjectField(TEXT("native_call_counts"));
    Counts->SetNumberField(Key, Counts->GetNumberField(Key) + 1);
}
FString Absolute(const FString& Path)
{
    FString Full = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Full); FPaths::CollapseRelativeDirectories(Full);
    return Full;
}
FFileObservation ObserveFile(const FString& Filename, FResult& R)
{
    Count(R, TEXT("file_observation_calls")); FFileObservation O;
    IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
    O.bExists = File.FileExists(*Filename); O.bReadOnly = O.bExists && File.IsReadOnly(*Filename);
    if (!O.bExists) { O.bKnown = true; return O; }
    const int64 Size = File.FileSize(*Filename);
    if (Size < 0 || Size > MaxConfigBytes) { return O; }
    O.bKnown = FFileHelper::LoadFileToArray(O.Bytes, *Filename) && O.Bytes.Num() == Size;
    return O;
}
void FileReceipt(FResult& R, const TCHAR* Key, const FFileObservation& O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("known"), O.bKnown);
    J->SetBoolField(TEXT("exists"), O.bExists); J->SetBoolField(TEXT("read_only"), O.bReadOnly);
    J->SetNumberField(TEXT("bytes_observed"), O.Bytes.Num()); R.Data->SetObjectField(Key, J);
}
bool Current(UInputSettings* Settings, FConfigCacheIni* Config, const FString& DefaultFilename, const FString& GeneratedFilename)
{
    if (IsPlaySessionActive() || !IsValid(Settings) || GConfig != Config || !Config || Settings->GetClass() != UInputSettings::StaticClass()
        || Settings->GetClass()->GetDefaultObject(false) != Settings || !Settings->HasAnyFlags(RF_ClassDefaultObject)) { return false; }
    FConfigBranch* Branch = Config->FindBranchWithNoReload(FName(TEXT("Input")), FString());
    return Branch && !Branch->bIsSafeUnloaded && Absolute(Branch->IniPath) == GeneratedFilename
        && Absolute(Settings->GetDefaultConfigFilename()) == DefaultFilename;
}
template<typename T>
int32 Matches(const TArray<T>& Rows, const T& Wanted)
{
    int32 N = 0; for (const auto& Row : Rows) { N += Row == Wanted ? 1 : 0; } return N;
}
bool ReadDiskMappings(const FString& ExpectedFilename, const FString& ExpectedDefaultFilename, const FString& SourceEngineConfigDir, const FString& SourceProjectConfigDir,
    TArray<FInputActionKeyMapping>& Actions, TArray<FInputAxisKeyMapping>& Axes, FResult& R, FString& Error)
{
    Count(R, TEXT("independent_config_read_calls"));
    // Temporary caches are load-only: neither Context nor cache teardown writes the physical config.
    // Unlike ReadIntoLocalFile, ExistingFile remains null so saved-layer command semantics are merged.
    FConfigCacheIni Local(EConfigCacheType::Temporary);
    FConfigBranch* LocalBranch = Local.FindBranchWithNoReload(FName(TEXT("Input")), FString());
    if (!LocalBranch) { Error = TEXT("Fresh temporary Input config branch is unavailable"); return false; }
    FConfigContext Context = FConfigContext::ReadIntoConfigSystem(&Local, FString());
    Context.DestIniFilename = ExpectedFilename;
    Context.bForceReload = true; Context.bUseHierarchyCache = false;
    Context.bWriteDestIni = false; Context.bAllowGeneratedIniWhenCooked = false;
    if (!SourceEngineConfigDir.IsEmpty()) { Context.EngineConfigDir = SourceEngineConfigDir; }
    if (!SourceProjectConfigDir.IsEmpty()) { Context.ProjectConfigDir = SourceProjectConfigDir; }
    const bool bLoaded = Context.Load(TEXT("Input"), LocalBranch->IniPath);
    FConfigFile* Disk = Local.FindConfigFileWithBaseName(FName(TEXT("Input")));
    R.Data->SetStringField(TEXT("disk_readback_config_identifier"), LocalBranch->IniPath);
    R.Data->SetStringField(TEXT("disk_readback_physical_filename"), Context.DestIniFilename);
    R.Data->SetStringField(TEXT("independent_config_read_scope"), TEXT("fresh_temporary_authored_default_static_and_generated_saved_Input_hierarchy"));
    bool bDefaultLayerObserved = false;
    for (const auto& Layer : LocalBranch->Hierarchy)
    { if (Absolute(FString(Layer.Value)) == ExpectedDefaultFilename) { bDefaultLayerObserved = true; break; } }
    R.Data->SetBoolField(TEXT("authored_default_layer_in_fresh_hierarchy"), bDefaultLayerObserved);
    R.Data->SetStringField(TEXT("disk_readback_authored_default_filename"), ExpectedDefaultFilename);
    R.Data->SetBoolField(TEXT("independent_read_global_cache_used"), false);
    R.Data->SetBoolField(TEXT("independent_read_config_write_enabled"), false);
    if (!bLoaded || !Disk || !bDefaultLayerObserved || Absolute(Context.DestIniFilename) != ExpectedFilename || Absolute(LocalBranch->IniPath) != ExpectedFilename)
    { Error = TEXT("Fresh temporary Input hierarchy did not resolve to the same physical config target"); return false; }
    TArray<FString> ActionRows, AxisRows;
    Disk->GetArray(Section, TEXT("ActionMappings"), ActionRows); Disk->GetArray(Section, TEXT("AxisMappings"), AxisRows);
    if (ActionRows.Num() > MaxMappings || AxisRows.Num() > MaxMappings)
    { Error = TEXT("Independent Input mapping readback exceeds the finite mapping bound"); return false; }
    for (const FString& Row : ActionRows)
    {
        if (Row.Len() > NAME_SIZE * 16) { Error = TEXT("Action mapping text exceeds finite import bound"); return false; }
        FInputActionKeyMapping Mapping;
        const TCHAR* End = FInputActionKeyMapping::StaticStruct()->ImportText(*Row, &Mapping, nullptr, PPF_None, nullptr, TEXT("FInputActionKeyMapping"));
        if (!End || !FString(End).TrimStartAndEnd().IsEmpty()) { Error = TEXT("Disk ActionMappings could not be completely imported through the public script struct"); return false; }
        Actions.Add(Mapping);
    }
    for (const FString& Row : AxisRows)
    {
        if (Row.Len() > NAME_SIZE * 16) { Error = TEXT("Axis mapping text exceeds finite import bound"); return false; }
        FInputAxisKeyMapping Mapping;
        const TCHAR* End = FInputAxisKeyMapping::StaticStruct()->ImportText(*Row, &Mapping, nullptr, PPF_None, nullptr, TEXT("FInputAxisKeyMapping"));
        if (!End || !FString(End).TrimStartAndEnd().IsEmpty()) { Error = TEXT("Disk AxisMappings could not be completely imported through the public script struct"); return false; }
        Axes.Add(Mapping);
    }
    return true;
}
FResult Fail(FResult R, const FString& Error)
{
    R.bSuccess = false; R.bPostconditionPassed = false; R.ErrorCode = TEXT("input_mapping_failed"); R.Error = Error;
    R.Data->SetStringField(TEXT("message"), Error);
    R.Data->SetBoolField(TEXT("mutation_state_unknown"), !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("partial_mutation"), R.bChanged || !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("do_not_retry"), R.bMutationAttempted || !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("workflow_recovery_required"), R.bChanged || !R.bChangeStateKnown);
    return R;
}
}
Variable::FResult Create(const FString& ActionName, const FString& Key, const FString& InputType,
    bool bShift, bool bCtrl, bool bAlt, bool bCmd)
{
    FResult R; R.Stage = TEXT("parameter_validation");
    auto Counts = MakeShared<FJsonObject>();
    for (const TCHAR* K : { TEXT("get_input_settings_calls"), TEXT("legacy_append_calls"), TEXT("save_config_calls"),
        TEXT("save_config_returns"), TEXT("default_config_update_calls"), TEXT("default_config_update_returns"), TEXT("file_observation_calls"), TEXT("independent_config_read_calls"),
        TEXT("transaction_scope_calls"), TEXT("explicit_modify_calls"), TEXT("keymap_rebuild_calls"), TEXT("compile_calls"), TEXT("asset_save_calls") }) { Counts->SetNumberField(K, 0); }
    R.Data->SetObjectField(TEXT("native_call_counts"), Counts);
    R.Data->SetStringField(TEXT("operation"), TEXT("create_input_mapping"));
    R.Data->SetStringField(TEXT("action_name"), ActionName.Left(NAME_SIZE)); R.Data->SetStringField(TEXT("key"), Key.Left(NAME_SIZE));
    R.Data->SetStringField(TEXT("requested_input_type"), InputType.Left(NAME_SIZE));
    R.Data->SetNumberField(TEXT("action_name_input_length"), ActionName.Len()); R.Data->SetNumberField(TEXT("key_input_length"), Key.Len());
    R.Data->SetNumberField(TEXT("input_type_input_length"), InputType.Len());
    R.Data->SetBoolField(TEXT("diagnostic_input_truncated"), ActionName.Len() > NAME_SIZE || Key.Len() > NAME_SIZE || InputType.Len() > NAME_SIZE);
    R.Data->SetBoolField(TEXT("input_type_ignored_as_in_original"), false);
    R.Data->SetBoolField(TEXT("original_axis_contract_defect_corrected"), true);
    R.Data->SetBoolField(TEXT("enhanced_input_asset_created"), false); R.Data->SetBoolField(TEXT("axis_mapping_created"), false);
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("idempotent"), false);
    R.Data->SetBoolField(TEXT("automatic_retry"), false); R.Data->SetBoolField(TEXT("runtime_binding_verified"), false);
    R.Data->SetBoolField(TEXT("cold_editor_reload_verified"), false); R.Data->SetBoolField(TEXT("default_input_file_updated"), false);
    R.Data->SetBoolField(TEXT("whole_input_settings_persistence_verified"), false); R.Data->SetBoolField(TEXT("total_SDK_callback_effects_verified"), false);
    R.Data->SetStringField(TEXT("config_save_scope"), TEXT("public TryUpdateDefaultConfigFile1 authors the actual default InputSettings section; SDK temporary SaveConfig/physical writes/generated flush/reload are separate unmeasured effects"));
    R.Data->SetStringField(TEXT("legacy_config_save_scope"), TEXT("Original direct SaveConfig1 flushed the generated Input branch without verified InputSettings section persistence; same UE56 unchecked defect"));
    R.Data->SetNumberField(TEXT("SDK_source_expected_internal_save_config_calls"), 0);
    R.Data->SetBoolField(TEXT("SDK_implicit_physical_write_and_reload_counts_measured"), false);
    R.Data->SetBoolField(TEXT("duplicate_RAM_append_persistence_equivalence_verified"), false);
    R.Data->SetStringField(TEXT("duplicate_risk"), TEXT("Every accepted retry appends another identical legacy mapping; no deduplication or automatic replay"));
    if (!IsEditorGameThread()) { return Fail(R, TEXT("Input mapping authoring requires the editor game thread")); }
    if (IsPlaySessionActive()) { return Fail(R, TEXT("Input settings/config authoring is blocked during PIE or SIE; no settings acquisition or mutation was attempted")); }
    if (ActionName.Len() >= NAME_SIZE || Key.Len() >= NAME_SIZE)
    { return Fail(R, TEXT("action_name and key must fit the actual engine FName bound; original whitespace/None semantics are retained")); }
    const bool bAxis = InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase);
    if (!bAxis && !InputType.Equals(TEXT("Action"), ESearchCase::IgnoreCase))
    { return Fail(R, TEXT("input_type must be Action or Axis (case-insensitive exact value; whitespace is not trimmed)")); }
    R.Data->SetStringField(TEXT("actual_input_type"), bAxis ? TEXT("Axis") : TEXT("Action"));
    R.Data->SetBoolField(TEXT("shift"), bShift); R.Data->SetBoolField(TEXT("ctrl"), bCtrl); R.Data->SetBoolField(TEXT("alt"), bAlt); R.Data->SetBoolField(TEXT("cmd"), bCmd);
    if (bAxis && (bShift || bCtrl || bAlt || bCmd))
    { return Fail(R, TEXT("Legacy Axis mappings have no modifier fields; an Axis request with any enabled internal modifier is rejected before settings acquisition or mutation")); }
    if (!GConfig) { return Fail(R, TEXT("Input configuration system is unavailable")); }
    Count(R, TEXT("transaction_scope_calls")); FScopedTransaction Transaction(NSLOCTEXT("Cubeless", "CreateInputMapping", "Cubeless create legacy input mapping"));
    R.Stage = TEXT("acquire_input_settings");
    UObject* PriorSettings = UInputSettings::StaticClass()->GetDefaultObject(false);
    R.Data->SetBoolField(TEXT("input_settings_preexisting"), PriorSettings != nullptr);
    R.bChangeStateKnown = false;
    Count(R, TEXT("get_input_settings_calls")); UInputSettings* Settings = UInputSettings::GetInputSettings();
    if (!IsValid(Settings)) { return Fail(R, TEXT("Failed to get input settings; acquisition effects are not verified")); }
    R.bChangeStateKnown = Settings == PriorSettings;
    R.Data->SetBoolField(TEXT("input_settings_acquisition_effects_verified"), R.bChangeStateKnown);
    TStrongObjectPtr<UInputSettings> Hold(Settings); FConfigCacheIni* Config = GConfig;
    FConfigBranch* Branch = Config->FindBranchWithNoReload(FName(TEXT("Input")), FString());
    if (!Branch || Branch->bIsSafeUnloaded || Branch->IniPath.IsEmpty()) { return Fail(R, TEXT("An already loaded physical Input config branch is required")); }
    const FString GeneratedFilename = Absolute(Branch->IniPath);
    const FString Filename = Absolute(Settings->GetDefaultConfigFilename());
    const FString SourceEngineConfigDir = Branch->SourceEngineConfigDir;
    const FString SourceProjectConfigDir = Branch->SourceProjectConfigDir;
    R.Data->SetStringField(TEXT("config_filename"), Filename);
    R.Data->SetStringField(TEXT("authored_default_input_filename"), Filename);
    R.Data->SetStringField(TEXT("generated_input_config_filename"), GeneratedFilename);
    R.Data->SetStringField(TEXT("config_identifier"), Settings->GetClass()->GetConfigName());
    R.Data->SetBoolField(TEXT("dynamic_input_layers_present"), Branch->DynamicLayers.GetHead() != nullptr);
    if (!FPaths::IsUnderDirectory(Filename, Absolute(FPaths::ProjectDir()))
        || !FPaths::IsUnderDirectory(GeneratedFilename, Absolute(FPaths::ProjectDir())) || Filename == GeneratedFilename)
    { return Fail(R, TEXT("Distinct authored default and generated Input config targets inside the current project are required; no mapping/config mutation was attempted")); }
    if (!Current(Settings, Config, Filename, GeneratedFilename)) { return Fail(R, TEXT("InputSettings/default/generated config target identity is unavailable")); }
    const auto& ExistingActions = Settings->GetActionMappings(); const auto& ExistingAxes = Settings->GetAxisMappings();
    if (ExistingActions.Num() > MaxMappings || ExistingAxes.Num() > MaxMappings
        || (!bAxis && ExistingActions.Num() >= MaxMappings) || (bAxis && ExistingAxes.Num() >= MaxMappings))
    { return Fail(R, TEXT("Input mappings exceed the finite authoring snapshot bound")); }
    const TArray<FInputActionKeyMapping> Before = ExistingActions; const TArray<FInputAxisKeyMapping> AxesBefore = ExistingAxes;
    FInputActionKeyMapping Wanted(FName(*ActionName), FKey(*Key), bShift, bCtrl, bAlt, bCmd);
    // The public three-input Axis contract has no scale argument; preserve the SDK default scale of 1.f.
    FInputAxisKeyMapping WantedAxis{FName(*ActionName), FKey(*Key)};
    R.Data->SetBoolField(TEXT("key_valid_at_admission"), Wanted.Key.IsValid()); R.Data->SetBoolField(TEXT("action_name_is_none"), Wanted.ActionName.IsNone());
    if (bAxis) { R.Data->SetNumberField(TEXT("axis_scale"), WantedAxis.Scale); }
    R.Data->SetNumberField(TEXT("action_mapping_count_before"), Before.Num()); R.Data->SetNumberField(TEXT("axis_mapping_count_before"), AxesBefore.Num());
    R.Data->SetNumberField(TEXT("matching_mapping_count_before"), bAxis ? Matches(AxesBefore, WantedAxis) : Matches(Before, Wanted));
    const FFileObservation FileBefore = ObserveFile(Filename, R); FileReceipt(R, TEXT("config_file_before"), FileBefore);
    const FFileObservation GeneratedBefore = ObserveFile(GeneratedFilename, R); FileReceipt(R, TEXT("generated_config_file_before"), GeneratedBefore);
    if (!FileBefore.bKnown) { return Fail(R, TEXT("Physical Input config is unreadable or exceeds the finite file bound; no mapping/config mutation was attempted")); }
    if (!Current(Settings, Config, Filename, GeneratedFilename) || Settings->GetActionMappings() != Before || Settings->GetAxisMappings() != AxesBefore)
    { R.bChangeStateKnown = false; return Fail(R, TEXT("InputSettings changed before explicit append; no request replay")); }
    R.Stage = bAxis ? TEXT("append_legacy_axis_mapping") : TEXT("append_legacy_action_mapping");
    R.bMutationAttempted = true; R.bChangeStateKnown = false;
    Count(R, TEXT("legacy_append_calls"));
    // Both exported non-const overloads append exact duplicates and do not rebuild keymaps.
    if (bAxis)
    {
        const auto Append = static_cast<void (UInputSettings::*)(FInputAxisKeyMapping&)>(&UInputSettings::AddAxisMapping);
        (Settings->*Append)(WantedAxis);
    }
    else
    {
        const auto Append = static_cast<void (UInputSettings::*)(FInputActionKeyMapping&)>(&UInputSettings::AddActionMapping);
        (Settings->*Append)(Wanted);
    }
    if (!Current(Settings, Config, Filename, GeneratedFilename)) { return Fail(R, TEXT("InputSettings/config identity changed during append; actual state must be inspected")); }
    TArray<FInputActionKeyMapping> Expected = Before; TArray<FInputAxisKeyMapping> ExpectedAxes = AxesBefore;
    if (bAxis) { ExpectedAxes.Add(WantedAxis); } else { Expected.Add(Wanted); }
    const bool bExactAppend = Settings->GetActionMappings() == Expected && Settings->GetAxisMappings() == ExpectedAxes;
    R.Data->SetBoolField(TEXT("exact_append_observed"), bExactAppend);
    R.Data->SetNumberField(TEXT("action_mapping_count_after_append"), Settings->GetActionMappings().Num());
    R.Data->SetNumberField(TEXT("axis_mapping_count_after_append"), Settings->GetAxisMappings().Num());
    R.Data->SetNumberField(TEXT("matching_mapping_count_after_append"), bAxis ? Matches(Settings->GetAxisMappings(), WantedAxis) : Matches(Settings->GetActionMappings(), Wanted));
    if (!bExactAppend) { return Fail(R, TEXT("Exactly one appended mapping and preservation of prior Action/Axis order could not be observed")); }
    R.bChanged = true; R.bChangeStateKnown = true; R.Data->SetBoolField(TEXT("mapping_appended"), true);
    R.Data->SetBoolField(TEXT("axis_mapping_created"), bAxis);
    R.Data->SetStringField(TEXT("actual_action_name"), Wanted.ActionName.ToString()); R.Data->SetStringField(TEXT("actual_key"), Wanted.Key.ToString());
    const FFileObservation BeforeSave = ObserveFile(Filename, R);
    if (!BeforeSave.bKnown || BeforeSave.bExists != FileBefore.bExists || BeforeSave.Bytes != FileBefore.Bytes)
    { R.bChangeStateKnown = false; return Fail(R, TEXT("Authored default Input file changed or became unreadable before default update; appended RAM state is retained and no update was dispatched")); }
    if (!Current(Settings, Config, Filename, GeneratedFilename) || Settings->GetActionMappings() != Expected || Settings->GetAxisMappings() != ExpectedAxes)
    { R.bChangeStateKnown = false; return Fail(R, TEXT("Exact InputSettings/default/generated/append prefix changed before default update; no update was dispatched")); }
    const bool bFileOpsDisabled = Config->AreFileOperationsDisabled(); R.Data->SetBoolField(TEXT("config_file_operations_disabled"), bFileOpsDisabled);
    R.Stage = TEXT("author_default_input_config"); R.bSaveRequested = true;
    if (bFileOpsDisabled) { return Fail(R, TEXT("Config file operations are disabled; no default update was dispatched and appended RAM mapping remains")); }
    R.bSaveAttempted = true;
    Count(R, TEXT("default_config_update_calls"));
    const bool bDefaultUpdateEligible = Settings->TryUpdateDefaultConfigFile();
    Count(R, TEXT("default_config_update_returns"));
    R.Data->SetBoolField(TEXT("default_config_update_returned"), bDefaultUpdateEligible);
    R.Data->SetStringField(TEXT("default_config_update_bool_scope"), TEXT("read-only eligibility only; physical write/reload success is not propagated by this SDK bool"));
    R.Data->SetNumberField(TEXT("SDK_source_expected_internal_save_config_calls"), bDefaultUpdateEligible ? 1 : 0);
    R.Data->SetBoolField(TEXT("direct_save_config_dispatched"), false);
    if (!Current(Settings, Config, Filename, GeneratedFilename)) { R.bChangeStateKnown = false; return Fail(R, TEXT("InputSettings/default/generated identity changed during default update; RAM/cache/disk outcome is uncertain")); }
    const FFileObservation FileAfter = ObserveFile(Filename, R); FileReceipt(R, TEXT("config_file_after"), FileAfter);
    const FFileObservation GeneratedAfter = ObserveFile(GeneratedFilename, R); FileReceipt(R, TEXT("generated_config_file_after"), GeneratedAfter);
    const bool bDefaultFileChanged = FileAfter.bKnown && (FileBefore.bExists != FileAfter.bExists || FileBefore.Bytes != FileAfter.Bytes);
    R.Data->SetBoolField(TEXT("config_file_byte_change_observed"), bDefaultFileChanged);
    R.Data->SetBoolField(TEXT("default_input_file_updated"), bDefaultFileChanged);
    R.Data->SetBoolField(TEXT("generated_config_file_byte_change_observed"), GeneratedBefore.bKnown && GeneratedAfter.bKnown
        && (GeneratedBefore.bExists != GeneratedAfter.bExists || GeneratedBefore.Bytes != GeneratedAfter.Bytes));
    R.Stage = TEXT("independent_config_readback"); TArray<FInputActionKeyMapping> DiskActions; TArray<FInputAxisKeyMapping> DiskAxes; FString Error;
    const bool bDiskReadable = FileAfter.bKnown && FileAfter.bExists
        && ReadDiskMappings(GeneratedFilename, Filename, SourceEngineConfigDir, SourceProjectConfigDir, DiskActions, DiskAxes, R, Error);
    R.Data->SetBoolField(TEXT("independent_config_readback_passed"), bDiskReadable);
    if (bDiskReadable) { R.Data->SetNumberField(TEXT("matching_mapping_count_on_disk"), bAxis ? Matches(DiskAxes, WantedAxis) : Matches(DiskActions, Wanted)); }
    if (!Current(Settings, Config, Filename, GeneratedFilename) || Settings->GetActionMappings() != Expected || Settings->GetAxisMappings() != ExpectedAxes)
    { R.bChangeStateKnown = false; return Fail(R, TEXT("Current RAM/default/generated targets changed during update/readback; append/update observations are preserved without replay")); }
    const bool bArraysPersisted = bDiskReadable && DiskActions == Expected && DiskAxes == ExpectedAxes;
    R.bSaved = bDefaultUpdateEligible && FileAfter.bKnown && FileAfter.bExists && GeneratedAfter.bKnown && bArraysPersisted;
    R.Data->SetBoolField(TEXT("saved"), R.bSaved); R.Data->SetBoolField(TEXT("persisted_mapping_arrays_equal_ram"), bArraysPersisted);
    const bool bRequestedDuplicate = (bAxis ? Matches(ExpectedAxes, WantedAxis) : Matches(Expected, Wanted)) > 1;
    R.Data->SetBoolField(TEXT("requested_mapping_is_duplicate_after_append"), bRequestedDuplicate);
    R.Data->SetBoolField(TEXT("duplicate_RAM_append_persistence_equivalence_verified"), bRequestedDuplicate && bArraysPersisted);
    R.Data->SetBoolField(TEXT("previous_ram_mappings_and_order_preserved"), true);
    R.Data->SetStringField(TEXT("remaining_duplicate_persistence_difference"), TEXT("SDK default +array persistence may deduplicate exact mappings; RAM append/order are retained and any complete fresh-array mismatch fails without repair/replay"));
    if (!R.bSaved)
    {
        if (!FileAfter.bKnown || !GeneratedAfter.bKnown) { R.bChangeStateKnown = false; }
        if (Error.IsEmpty()) { Error = !bDefaultUpdateEligible ? TEXT("Default Input authoring was rejected as read-only; appended RAM mapping remains")
            : TEXT("Public default update was eligible but exact authored-default/static/generated mapping persistence was not confirmed; appended RAM and actual file/cache prefix are retained without replay"); }
        return Fail(R, Error);
    }
    R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("native_readback");
    return R;
}
}
