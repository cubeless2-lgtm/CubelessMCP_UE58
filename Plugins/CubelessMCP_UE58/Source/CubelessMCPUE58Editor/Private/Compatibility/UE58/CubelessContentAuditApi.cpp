#include "Compatibility/UE58/CubelessContentAuditApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonValue.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace Cubeless::UE58::ContentAudit
{
namespace
{
using Variable::FResult;
constexpr int32 MaxAssets = 16384, MaxParameters = 4096, MaxDependencyNames = 262144, MaxDirtyPackages = 32768;
constexpr int32 MaxReportBytes = 8 * 1024 * 1024, MaxCarrierChars = 1024 * 1024, MaxFilenameChars = 240;
constexpr double CooperativeBudgetSeconds = 5.0;
void Count(FResult& R, const TCHAR* Key)
{
    auto Counts = R.Data->GetObjectField(TEXT("native_call_counts"));
    Counts->SetNumberField(Key, Counts->GetNumberField(Key) + 1);
}
void Strings(FResult& R, const TCHAR* Key, const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Out; Out.Reserve(Values.Num());
    for (const auto& Value : Values) { Out.Add(MakeShared<FJsonValueString>(Value)); }
    R.Data->SetArrayField(Key, Out);
}
TSharedRef<FJsonObject> CountsJson(const TMap<FString, int32>& Values)
{
    auto Out = MakeShared<FJsonObject>(); TArray<FString> Keys; Values.GetKeys(Keys); Keys.Sort();
    for (const auto& Key : Keys) { Out->SetNumberField(Key, Values[Key]); } return Out;
}
FString Root(FString Path)
{
    Path.TrimStartAndEndInline(); while (Path.Len() > 1 && Path.EndsWith(TEXT("/"))) { Path.LeftChopInline(1); } return Path;
}
FString PackagePath(FString Path)
{
    Path = FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();
    Path.TrimQuotesInline(); Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    FString Converted;
    if (FPackageName::TryConvertFilenameToLongPackageName(Path, Converted)) { return Root(Converted); }
    if (Path.Contains(TEXT("."))) { Path = FPackageName::ObjectPathToPackageName(Path); }
    return Root(Path);
}
bool Under(const FString& Package, const FString& Parent) { return Package.Equals(Parent) || Package.StartsWith(Parent + TEXT("/")); }
FString Absolute(FString Path)
{
    Path = FPaths::ConvertRelativePathToFull(Path); FPaths::NormalizeFilename(Path); FPaths::CollapseRelativeDirectories(Path); return Path;
}
FResult Fail(FResult R, const FString& Error)
{
    R.bSuccess = false; R.bPostconditionPassed = false; R.ErrorCode = TEXT("content_root_audit_failed"); R.Error = Error;
    R.Data->SetBoolField(TEXT("success"), false); R.Data->SetStringField(TEXT("message"), Error);
    R.Data->SetBoolField(TEXT("mutation_state_unknown"), !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("partial_mutation"), R.bChanged || !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("automatic_retry"), false); R.Data->SetBoolField(TEXT("do_not_retry"), R.bMutationAttempted || !R.bChangeStateKnown);
    return R;
}
bool Decode(const FString& Text, TSharedPtr<FJsonValue>& Value, FString& Error)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    // Public SDK default policy refuses standalone scalar roots. Extract exactly one
    // bounded array element so null/number and []/{} retain the same logical meaning.
    if (Text.Len() > MaxCarrierChars
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Text + TEXT("]")), Values)
        || Values.Num() != 1 || !Values[0].IsValid())
    { Error = TEXT("Hidden nullable JSON carrier is invalid or exceeds finite input bound"); return false; }
    Value = Values[0]; return true;
}
bool Paths(const FString& Text, const TArray<FString>& Defaults, TArray<FString>& Out, FString& Error)
{
    TSharedPtr<FJsonValue> Value; if (!Decode(Text, Value, Error)) { return false; }
    if (Value->Type == EJson::Null) { Out = Defaults; }
    else if (Value->Type == EJson::Array)
    {
        if (Value->AsArray().Num() > MaxParameters) { Error = TEXT("Path array exceeds finite input bound"); return false; }
        for (const auto& Item : Value->AsArray())
        {
            if (!Item.IsValid() || Item->Type != EJson::String) { Error = TEXT("Path array must contain strings"); return false; }
            const FString Input = Item->AsString();
            if (Input.Len() >= NAME_SIZE) { Error = TEXT("Path array value exceeds engine name bound"); return false; }
            FString Trimmed = Input.TrimStartAndEnd(); if (!Trimmed.IsEmpty()) { Out.Add(Trimmed); }
        }
    }
    else { Error = TEXT("Path carrier must be an array or null"); return false; }
    TSet<FString> Seen; TArray<FString> Normalized;
    for (const auto& Item : Out)
    {
        FString Path = PackagePath(Item); if (Path.IsEmpty() || Seen.Contains(Path)) { continue; }
        if (Path.Len() >= NAME_SIZE) { Error = TEXT("Normalized package path exceeds engine name bound"); return false; }
        Seen.Add(Path); Normalized.Add(Path);
    }
    Normalized.Sort(); Out = MoveTemp(Normalized); return true;
}
bool Integer(const TSharedPtr<FJsonValue>& Value, int32& Out, FString& Error)
{
    if (!Value.IsValid() || Value->Type != EJson::Number || !FMath::IsFinite(Value->AsNumber()) || Value->AsNumber() > MAX_int32)
    { Error = TEXT("Expected count must be a finite number within signed-int32 engine bound"); return false; }
    Out = Value->AsNumber() <= 0 ? 0 : FMath::Max(0, FMath::RoundToInt(Value->AsNumber())); return true;
}
bool ExpectedCounts(const FString& CountText, const FString& ClassText, bool& HasCount, int32& ExpectedCount,
    TMap<FString, int32>& Classes, FString& Error)
{
    TSharedPtr<FJsonValue> Value;
    if (!Decode(CountText, Value, Error)) { return false; }
    HasCount = Value->Type != EJson::Null;
    if (HasCount && !Integer(Value, ExpectedCount, Error)) { return false; }
    if (!Decode(ClassText, Value, Error)) { return false; }
    if (Value->Type == EJson::Null) { return true; }
    if (Value->Type != EJson::Object || Value->AsObject()->Values.Num() > MaxParameters)
    { Error = TEXT("Expected class counts must be an object or null within finite input bound"); return false; }
    for (const auto& Pair : Value->AsObject()->Values)
    {
        if (Pair.Key.Len() >= NAME_SIZE) { Error = TEXT("Expected class name exceeds finite name bound"); return false; }
        int32 Number = 0; if (!Integer(Pair.Value, Number, Error)) { return false; } Classes.Add(FString(Pair.Key.ToView()), Number);
    }
    return true;
}
bool Dirty(FResult& R, TArray<FString>& Out, FString& Error)
{
    Count(R, TEXT("dirty_snapshot_calls")); TArray<UPackage*> Packages; FEditorFileUtils::GetDirtyPackages(Packages);
    if (Packages.Num() > MaxDirtyPackages) { Error = TEXT("Project dirty package snapshot exceeds finite bound"); return false; }
    TSet<FString> Seen;
    for (UPackage* Package : Packages)
    {
        if (!IsValid(Package)) { continue; } FString Name = Package->GetName();
        if (!Name.IsEmpty() && !Seen.Contains(Name)) { Seen.Add(Name); Out.Add(Name); }
    }
    Out.Sort(); return true;
}
FString SafeStem(FString Stem)
{
    Stem.TrimStartAndEndInline(); if (Stem.IsEmpty()) { Stem = TEXT("content"); }
    for (TCHAR& C : Stem) { if (!FChar::IsAlnum(C) && C != TEXT('_') && C != TEXT('-') && C != TEXT('.')) { C = TEXT('_'); } }
    return Stem;
}
bool WriteReport(FResult& R, const FString& RootPath, FString& Error)
{
    R.Stage = TEXT("report_path_admission");
    const FString LegacyFilename = SafeStem(FPackageName::GetShortName(RootPath)) + TEXT("_content_root_audit_report.json");
    R.Data->SetStringField(TEXT("legacy_report_filename"), LegacyFilename);
    R.Data->SetStringField(TEXT("legacy_report_directory"), FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP")));
    const FString Directory = Absolute(FPaths::Combine(FPaths::ProjectDir(), TEXT("cubeless/records/verification/mcp-audits")));
    const FString Filename = SafeStem(FPackageName::GetShortName(RootPath)) + TEXT("_content_root_audit_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("_report.json");
    const FString Path = Absolute(FPaths::Combine(Directory, Filename));
    if (Filename.Len() > MaxFilenameChars || Path.Len() > 4096 || !FPaths::IsUnderDirectory(Path, Directory)
        || Absolute(FPaths::GetPath(Path)) != Directory || IFileManager::Get().FileExists(*Path))
    { Error = TEXT("Unique bounded report filename/parent admission failed; no writer dispatched"); return false; }
    R.Data->SetStringField(TEXT("report_path"), Path);
    R.Data->SetStringField(TEXT("report_directory"), Directory);
    R.Data->SetBoolField(TEXT("report_snapshot_prefix_only"), true);
    R.Data->SetStringField(TEXT("report_snapshot_scope"), TEXT("completed diagnostic before file writer receipt; actual write/readback receipt is returned separately"));
    R.Data->SetBoolField(TEXT("existing_report_overwrite_requested"), false);
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    const bool bDirectoryBefore = Files.DirectoryExists(*Directory);
    R.Stage = TEXT("report_directory_create"); R.bMutationAttempted = true;
    Count(R, TEXT("report_directory_calls"));
    const bool bDirectoryCall = IFileManager::Get().MakeDirectory(*Directory, true);
    const bool bDirectoryAfter = Files.DirectoryExists(*Directory);
    R.Data->SetBoolField(TEXT("report_directory_returned"), bDirectoryCall);
    R.Data->SetBoolField(TEXT("report_directory_created_observed"), !bDirectoryBefore && bDirectoryAfter);
    R.bChanged |= !bDirectoryBefore && bDirectoryAfter;
    if (!bDirectoryCall || !bDirectoryAfter) { Error = TEXT("Report directory creation failed; observed directory prefix is retained"); return false; }
    FString Json;
    if (!FJsonSerializer::Serialize(R.Data, TJsonWriterFactory<>::Create(&Json))) { Error = TEXT("Report JSON serialization failed"); return false; }
    FTCHARToUTF8 Utf8(*Json); const int32 ByteCount = Utf8.Length();
    if (ByteCount > MaxReportBytes - 3) { Error = TEXT("Full report exceeds finite byte bound; no truncated success report is written"); return false; }
    TArray<uint8> Expected; Expected.Reserve(ByteCount + 3);
    Expected.Add(0xef); Expected.Add(0xbb); Expected.Add(0xbf);
    Expected.Append(reinterpret_cast<const uint8*>(Utf8.Get()), ByteCount);
    R.Stage = TEXT("report_file_write"); R.bSaveAttempted = true;
    Count(R, TEXT("report_write_calls"));
    const bool bWritten = FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), FILEWRITE_NoReplaceExisting);
    Count(R, TEXT("report_write_returns")); R.Data->SetBoolField(TEXT("report_write_returned"), bWritten);
    const bool bExists = Files.FileExists(*Path); const int64 Size = bExists ? Files.FileSize(*Path) : -1;
    R.Data->SetBoolField(TEXT("report_file_exists_after"), bExists); R.Data->SetNumberField(TEXT("report_file_size_after"), Size);
    R.Data->SetBoolField(TEXT("report_file_created_observed"), bExists); R.bChanged |= bExists;
    R.Stage = TEXT("report_file_readback"); TArray<uint8> Actual;
    Count(R, TEXT("report_readback_calls"));
    const bool bReadable = bExists && Size >= 0 && Size <= MaxReportBytes && FFileHelper::LoadFileToArray(Actual, *Path) && Actual.Num() == Size;
    R.Data->SetBoolField(TEXT("report_file_readable_after"), bReadable);
    R.Data->SetNumberField(TEXT("report_expected_bytes"), Expected.Num());
    R.Data->SetBoolField(TEXT("report_bytes_equal_snapshot"), bReadable && Actual == Expected);
    R.bSaved = bWritten && bReadable && Actual == Expected;
    R.Data->SetBoolField(TEXT("report_saved"), R.bSaved);
    R.Data->SetBoolField(TEXT("report_failure_cleanup_attempted"), false);
    if (!R.bSaved) { Error = TEXT("Report write or exact fresh byte readback failed; partial file/directory prefix is retained without rollback/retry"); return false; }
    return true;
}
}
Variable::FResult Audit(const FString& RootPath, bool bRecursive, bool bScanPaths, bool bIncludeDependencies,
    bool bIncludeAssets, bool bWriteReport, const FString& ForbiddenPrefixesJson, const FString& RequiredAssetPathsJson,
    const FString& ExpectedAssetCountJson, const FString& ExpectedClassCountsJson, int64 MaxSamples,
    bool bFailOnDirtyPackages, bool bFailOnRedirectors, bool bFailOnForbiddenDependencies)
{
    FResult R; R.Stage = TEXT("parameter_validation"); R.bSaveRequested = bWriteReport;
    auto Calls = MakeShared<FJsonObject>();
    for (const TCHAR* K : {TEXT("module_acquire_calls"), TEXT("scan_paths_calls"), TEXT("inventory_calls"), TEXT("dependency_query_calls"),
        TEXT("dirty_snapshot_calls"), TEXT("explicit_asset_load_calls"), TEXT("explicit_asset_modify_calls"), TEXT("compile_calls"), TEXT("asset_save_calls"),
        TEXT("report_directory_calls"), TEXT("report_write_calls"), TEXT("report_write_returns"), TEXT("report_readback_calls")}) { Calls->SetNumberField(K, 0); }
    R.Data->SetObjectField(TEXT("native_call_counts"), Calls);
    R.Data->SetStringField(TEXT("operation"), TEXT("audit_content_root_mcp"));
    R.Data->SetBoolField(TEXT("success"), false); R.Data->SetBoolField(TEXT("audit_completed"), false);
    R.Data->SetBoolField(TEXT("validation_pass"), false); R.Data->SetBoolField(TEXT("inventory_complete"), false);
    R.Data->SetBoolField(TEXT("write_report"), bWriteReport); R.Data->SetBoolField(TEXT("report_saved"), false);
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("automatic_retry"), false);
    R.Data->SetBoolField(TEXT("explicit_asset_mutation_attempted"), false);
    R.Data->SetBoolField(TEXT("total_SDK_effects_verified"), false);
    R.Data->SetStringField(TEXT("save_scope"), TEXT("optional report file only; no UE asset compile/save"));
    R.Data->SetNumberField(TEXT("inventory_bound"), MaxAssets); R.Data->SetNumberField(TEXT("dependency_name_bound"), MaxDependencyNames);
    R.Data->SetNumberField(TEXT("cooperative_budget_seconds"), CooperativeBudgetSeconds);
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("Audit requires idle editor game thread outside PIE/SIE")); }
    const FString NormalizedRoot = Root(RootPath);
    if (NormalizedRoot.Len() >= NAME_SIZE || !FPackageName::IsValidLongPackageName(NormalizedRoot) || !NormalizedRoot.StartsWith(TEXT("/Game/")))
    { return Fail(R, TEXT("Invalid root_path; a valid /Game/ child content root is required (not /Game itself)")); }
    if (MaxSamples > MaxParameters) { return Fail(R, TEXT("max_samples exceeds finite diagnostic output bound; inventory coverage is separate and is not silently truncated")); }
    const int32 Samples = static_cast<int32>(FMath::Max<int64>(1, MaxSamples));
    R.Data->SetStringField(TEXT("root_path"), NormalizedRoot); R.Data->SetBoolField(TEXT("recursive"), bRecursive);
    R.Data->SetBoolField(TEXT("scan_paths"), bScanPaths); R.Data->SetBoolField(TEXT("include_dependencies"), bIncludeDependencies);
    R.Data->SetBoolField(TEXT("include_assets"), bIncludeAssets); R.Data->SetNumberField(TEXT("max_samples"), Samples);
    R.Data->SetBoolField(TEXT("fail_on_dirty_packages"), bFailOnDirtyPackages); R.Data->SetBoolField(TEXT("fail_on_redirectors"), bFailOnRedirectors);
    R.Data->SetBoolField(TEXT("fail_on_forbidden_dependencies"), bFailOnForbiddenDependencies);
    TArray<FString> Forbidden, Required; TMap<FString, int32> ExpectedClasses; bool HasExpected = false; int32 ExpectedAssetCount = 0; FString Error;
    if (!Paths(ForbiddenPrefixesJson, {TEXT("/Game/_MCP_Temp")}, Forbidden, Error) || !Paths(RequiredAssetPathsJson, {}, Required, Error)
        || !ExpectedCounts(ExpectedAssetCountJson, ExpectedClassCountsJson, HasExpected, ExpectedAssetCount, ExpectedClasses, Error)) { return Fail(R, Error); }
    Strings(R, TEXT("forbidden_dependency_prefixes"), Forbidden); Strings(R, TEXT("required_asset_paths"), Required);
    R.Data->SetObjectField(TEXT("expected_class_counts"), CountsJson(ExpectedClasses)); R.Data->SetBoolField(TEXT("has_expected_asset_count"), HasExpected);
    if (HasExpected) { R.Data->SetNumberField(TEXT("expected_asset_count"), ExpectedAssetCount); }
    TArray<FString> BeforeDirty;
    if (!Dirty(R, BeforeDirty, Error)) { return Fail(R, Error); } Strings(R, TEXT("dirty_packages_before_registry"), BeforeDirty);
    R.Stage = TEXT("registry_acquisition"); R.bChangeStateKnown = false;
    R.Data->SetBoolField(TEXT("registry_and_tag_callback_effects_unknown"), true);
    R.Data->SetBoolField(TEXT("implicit_asset_load_or_PostLoad_effects_verified"), false);
    Count(R, TEXT("module_acquire_calls"));
    FAssetRegistryModule* Module = FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if (!Module) { return Fail(R, TEXT("AssetRegistry module is unavailable; module/callback prefix is uncertain")); }
    IAssetRegistry& Registry = Module->Get(); const double Started = FPlatformTime::Seconds();
    if (bScanPaths)
    {
        R.Stage = TEXT("registry_forced_scan"); R.bMutationAttempted = true;
        R.Data->SetBoolField(TEXT("registry_cache_mutation_attempted"), true);
        R.Data->SetStringField(TEXT("scan_execution"), TEXT("explicit forced ScanPathsSynchronous; nonpreemptible SDK call; async SearchAllAssets has different scope/force contract"));
        Count(R, TEXT("scan_paths_calls")); const double ScanStarted = FPlatformTime::Seconds();
        Registry.ScanPathsSynchronous({NormalizedRoot}, true);
        R.Data->SetNumberField(TEXT("scan_seconds"), FPlatformTime::Seconds() - ScanStarted);
    }
    if (IsPlaySessionActive()) { return Fail(R, TEXT("PIE/SIE began during Registry work; no further audit/report work dispatched")); }
    R.Stage = TEXT("registry_inventory"); TArray<FAssetData> Assets; bool Bounded = true;
    FARFilter Filter; Filter.PackagePaths.Add(FName(*NormalizedRoot)); Filter.bRecursivePaths = bRecursive; Filter.bIncludeOnlyOnDiskAssets = false;
    Count(R, TEXT("inventory_calls"));
    const bool Query = Registry.EnumerateAssets(Filter, [&](const FAssetData& Asset)
    {
        if (Assets.Num() >= MaxAssets || FPlatformTime::Seconds() - Started > CooperativeBudgetSeconds) { Bounded = false; return false; }
        Assets.Add(Asset); return true;
    }, UE::AssetRegistry::EEnumerateAssetsFlags::None);
    R.Data->SetBoolField(TEXT("registry_query_returned"), Query); R.Data->SetNumberField(TEXT("inventory_rows_observed"), Assets.Num());
    if (!Bounded) { return Fail(R, TEXT("Inventory count/cooperative time guard stopped before complete coverage; no success or truncated validation is reported")); }
    // Like the original GetAssetsByPath handler, a false query with an empty inventory is a completed empty diagnostic.
    Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
    TMap<FString, int32> Classes, Folders; TSet<FString> Packages; TArray<FString> RedirectorSamples;
    TArray<TSharedPtr<FJsonValue>> AssetValues; int32 Redirectors = 0;
    for (const FAssetData& Asset : Assets)
    {
        const FString Name = Asset.PackageName.ToString(), Class = Asset.AssetClassPath.GetAssetName().ToString();
        Classes.FindOrAdd(Class)++; Folders.FindOrAdd(Asset.PackagePath.ToString())++; Packages.Add(Name);
        if (Class == TEXT("ObjectRedirector")) { ++Redirectors; if (RedirectorSamples.Num() < Samples) { RedirectorSamples.Add(Name); } }
        if (bIncludeAssets)
        {
            auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("package_name"), Name); Row->SetStringField(TEXT("asset_name"), Asset.AssetName.ToString());
            Row->SetStringField(TEXT("object_path"), FString::Printf(TEXT("%s.%s"), *Name, *Asset.AssetName.ToString()));
            Row->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString()); Row->SetStringField(TEXT("class"), Class);
            AssetValues.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    R.Data->SetNumberField(TEXT("asset_count"), Assets.Num()); R.Data->SetObjectField(TEXT("class_counts"), CountsJson(Classes));
    R.Data->SetObjectField(TEXT("folder_counts"), CountsJson(Folders)); R.Data->SetNumberField(TEXT("redirector_count"), Redirectors);
    Strings(R, TEXT("redirector_samples"), RedirectorSamples); if (bIncludeAssets) { R.Data->SetArrayField(TEXT("assets"), AssetValues); }
    R.Stage = TEXT("dependency_audit"); TSet<FName> ForbiddenPackages; TArray<FString> DependencySamples;
    int32 Scanned = 0, Hits = 0, DependencyNames = 0, FailedDependencyQueries = 0;
    if (bIncludeDependencies && !Forbidden.IsEmpty())
    {
        for (const FAssetData& Asset : Assets)
        {
            if (FPlatformTime::Seconds() - Started > CooperativeBudgetSeconds || IsPlaySessionActive())
            { return Fail(R, TEXT("Dependency audit cooperative/PIE guard stopped before complete coverage; current inventory prefix retained")); }
            ++Scanned; TArray<FName> Dependencies; Count(R, TEXT("dependency_query_calls"));
            FailedDependencyQueries += Registry.GetDependencies(Asset.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package) ? 0 : 1;
            if (Dependencies.Num() > MaxDependencyNames - DependencyNames) { return Fail(R, TEXT("Direct dependency name guard stopped before complete coverage")); }
            DependencyNames += Dependencies.Num();
            for (FName Dependency : Dependencies)
            {
                const FString Name = Dependency.ToString(); bool Match = false;
                for (const auto& Prefix : Forbidden) { if (!Prefix.IsEmpty() && Under(Name, Prefix)) { Match = true; break; } }
                if (Match) { ++Hits; ForbiddenPackages.Add(Asset.PackageName); if (DependencySamples.Num() < Samples) { DependencySamples.Add(Asset.PackageName.ToString() + TEXT(" -> ") + Name); } }
            }
            R.Data->SetNumberField(TEXT("dependency_scanned_asset_count"), Scanned);
            R.Data->SetNumberField(TEXT("forbidden_dependency_asset_count"), ForbiddenPackages.Num());
            R.Data->SetNumberField(TEXT("forbidden_dependency_hit_count"), Hits); Strings(R, TEXT("forbidden_dependency_samples"), DependencySamples);
        }
    }
    R.Data->SetNumberField(TEXT("dependency_scanned_asset_count"), Scanned); R.Data->SetNumberField(TEXT("dependency_query_false_count"), FailedDependencyQueries);
    R.Data->SetNumberField(TEXT("dependency_names_observed"), DependencyNames); R.Data->SetStringField(TEXT("dependency_scope"), TEXT("direct on-disk Package dependencies only, not transitive or current UObject-reference closure"));
    R.Data->SetNumberField(TEXT("forbidden_dependency_asset_count"), ForbiddenPackages.Num()); R.Data->SetNumberField(TEXT("forbidden_dependency_hit_count"), Hits);
    Strings(R, TEXT("forbidden_dependency_samples"), DependencySamples);
    TArray<FString> Missing, Mismatches, DirtyPackages, DirtyRoot;
    for (const auto& Path : Required) { if (!Packages.Contains(Path)) { Missing.Add(Path); } }
    for (const auto& Pair : ExpectedClasses) { const int32 Actual = Classes.FindRef(Pair.Key); if (Actual != Pair.Value) { Mismatches.Add(FString::Printf(TEXT("%s expected=%d actual=%d"), *Pair.Key, Pair.Value, Actual)); } }
    Mismatches.Sort(); R.Stage = TEXT("dirty_package_snapshot");
    if (!Dirty(R, DirtyPackages, Error)) { return Fail(R, Error); }
    for (const auto& Path : DirtyPackages) { if (Under(Path, NormalizedRoot)) { DirtyRoot.Add(Path); } }
    Strings(R, TEXT("dirty_packages"), DirtyPackages); R.Data->SetNumberField(TEXT("dirty_package_count"), DirtyPackages.Num());
    Strings(R, TEXT("dirty_packages_under_root"), DirtyRoot); R.Data->SetNumberField(TEXT("dirty_package_under_root_count"), DirtyRoot.Num());
    R.Data->SetBoolField(TEXT("observed_project_dirty_snapshot_changed"), BeforeDirty != DirtyPackages);
    Strings(R, TEXT("missing_required_assets"), Missing); R.Data->SetNumberField(TEXT("required_asset_count"), Required.Num()); R.Data->SetNumberField(TEXT("missing_required_asset_count"), Missing.Num());
    const bool AssetCountMatches = !HasExpected || Assets.Num() == ExpectedAssetCount;
    R.Data->SetBoolField(TEXT("asset_count_matches_expected"), AssetCountMatches); R.Data->SetBoolField(TEXT("expected_class_counts_match"), Mismatches.IsEmpty());
    Strings(R, TEXT("expected_class_count_mismatches"), Mismatches);
    const bool Pass = (!bFailOnRedirectors || Redirectors == 0) && (!bFailOnForbiddenDependencies || Hits == 0)
        && (!bFailOnDirtyPackages || DirtyRoot.IsEmpty()) && Missing.IsEmpty() && AssetCountMatches && Mismatches.IsEmpty();
    R.Data->SetBoolField(TEXT("validation_pass"), Pass); R.Data->SetBoolField(TEXT("success"), true);
    R.Data->SetBoolField(TEXT("audit_completed"), true); R.Data->SetBoolField(TEXT("inventory_complete"), true);
    R.Data->SetNumberField(TEXT("audit_seconds_before_report"), FPlatformTime::Seconds() - Started);
    if (bWriteReport && !WriteReport(R, NormalizedRoot, Error)) { return Fail(R, Error); }
    R.Data->SetBoolField(TEXT("mutation_state_unknown"), !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("observed_report_or_directory_changed"), R.bChanged);
    R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("audit_complete");
    return R;
}
}
