#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessCompilerDiagnostics.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetIdentifier.h"
#include "Misc/AssetRegistryInterface.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Factories/BlueprintFunctionLibraryFactory.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/SkeletalMesh.h"
#include "FileHelpers.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"
#include "Rendering/SkeletalMeshModel.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Items)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FString& Item : Items) { Result.Add(MakeShared<FJsonValueString>(Item)); }
        return Result;
    }

    TArray<FString> DirtyPackages()
    {
        TArray<UPackage*> Packages;
        FEditorFileUtils::GetDirtyWorldPackages(Packages);
        FEditorFileUtils::GetDirtyContentPackages(Packages);
        TArray<FString> Names;
        for (const UPackage* Package : Packages) { if (Package) { Names.AddUnique(Package->GetName()); } }
        Names.Sort();
        return Names;
    }

    TSharedRef<FJsonObject> Failure(const TCHAR* Stage, const TCHAR* Message)
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("stage"), Stage);
        Result->SetStringField(TEXT("error"), Message);
        return Result;
    }

    TArray<FString> ForbiddenPrefixes()
    {
        TArray<FString> Prefixes = {
            TEXT("/UnrealMCP/"), TEXT("/MCPUnreal/"), TEXT("/MCP_Unreal/"), TEXT("/CubelessControlRigTools/"),
            TEXT("/Script/UnrealMCP"), TEXT("/Script/MCPUnreal"), TEXT("/Script/MCP_Unreal"), TEXT("/Script/CubelessControlRigTools")
        };
        for (const TCHAR* PluginName : {TEXT("CubelessMCP_UE58"), TEXT("ModelContextProtocol"), TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin"), TEXT("EditorScriptingUtilities")})
        {
            Prefixes.Add(FString::Printf(TEXT("/%s/"), PluginName));
            if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
            {
                for (const FModuleDescriptor& Module : Plugin->GetDescriptor().Modules)
                {
                    Prefixes.Add(FString::Printf(TEXT("/Script/%s"), *Module.Name.ToString()));
                }
            }
        }
        Prefixes.Sort();
        return Prefixes;
    }

    bool IsForbidden(const FString& PackageName, const TArray<FString>& Prefixes)
    {
        for (const FString& Prefix : Prefixes)
        {
            if (Prefix.EndsWith(TEXT("/")) ? PackageName.StartsWith(Prefix) : PackageName.Equals(Prefix)) { return true; }
        }
        return false;
    }
}

namespace Cubeless::UE58
{
    bool IsEditorGameThread() { return IsInGameThread() && GEditor != nullptr; }
    bool IsPlaySessionActive() { return GEditor && (GEditor->PlayWorld != nullptr || GEditor->GetPIEWorldContext() != nullptr); }
    bool IsLongPackageName(const FString& Name) { return FPackageName::IsValidLongPackageName(Name); }

    TSharedRef<FJsonObject> ReadEditorDiagnostics()
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("stage"), TEXT("complete"));
        Result->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
        Result->SetStringField(TEXT("project_name"), FApp::GetProjectName());
        Result->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
        Result->SetStringField(TEXT("instance_id"), FApp::GetInstanceId().ToString(EGuidFormats::DigitsWithHyphens));
        Result->SetStringField(TEXT("session_id"), FApp::GetSessionId().ToString(EGuidFormats::DigitsWithHyphens));
        Result->SetStringField(TEXT("project_file"), FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
        Result->SetBoolField(TEXT("game_thread"), IsInGameThread());
        Result->SetBoolField(TEXT("pie_or_sie_active"), IsPlaySessionActive());
        Result->SetBoolField(TEXT("is_commandlet"), IsRunningCommandlet());
        Result->SetArrayField(TEXT("dirty_packages"), Strings(DirtyPackages()));
        TArray<TSharedPtr<FJsonValue>> Plugins;
        for (const TCHAR* Name : {TEXT("CubelessMCP_UE58"), TEXT("ModelContextProtocol"), TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin")})
        {
            const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Name);
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("name"), Name);
            Row->SetBoolField(TEXT("found"), Plugin.IsValid());
            Row->SetBoolField(TEXT("enabled"), Plugin && Plugin->IsEnabled());
            if (Plugin) { Row->SetStringField(TEXT("version"), Plugin->GetDescriptor().VersionName); }
            Plugins.Add(MakeShared<FJsonValueObject>(Row));
        }
        Result->SetArrayField(TEXT("plugins"), Plugins);
        return Result;
    }

    TSharedRef<FJsonObject> ReadPackageDependencies(const TArray<FString>& PackageNames, bool bRecursive, int32 MaxPackages)
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        if (Registry.IsLoadingAssets()) { return Failure(TEXT("registry"), TEXT("Asset Registry discovery is still running; retry this read after discovery completes.")); }

        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        const TArray<FString> Prefixes = ForbiddenPrefixes();
        const TArray<FString> Dirty = DirtyPackages();
        TArray<FName> Queue;
        TSet<FName> Seen;
        TArray<FString> Incomplete;
        TArray<FString> DirtyScanned;
        TArray<TSharedPtr<FJsonValue>> Edges;
        TArray<TSharedPtr<FJsonValue>> Violations;
        for (const FString& Name : PackageNames) { const FName Key(*Name); if (!Seen.Contains(Key)) { Seen.Add(Key); Queue.Add(Key); } }
        for (const FName Root : Queue)
        {
            if (IsForbidden(Root.ToString(), Prefixes))
            {
                TSharedRef<FJsonObject> Violation = MakeShared<FJsonObject>();
                Violation->SetStringField(TEXT("from"), Root.ToString());
                Violation->SetStringField(TEXT("to"), Root.ToString());
                Violation->SetStringField(TEXT("reason"), TEXT("root_package_owned_by_editor_tool_plugin"));
                Violations.Add(MakeShared<FJsonValueObject>(Violation));
            }
        }

        int32 Processed = 0;
        while (Processed < Queue.Num() && Processed < MaxPackages)
        {
            const FName Package = Queue[Processed++];
            const FString PackageString = Package.ToString();
            const UPackage* LoadedPackage = FindPackage(nullptr, *PackageString);
            if (Dirty.Contains(PackageString) || (LoadedPackage && LoadedPackage->IsDirty())) { DirtyScanned.Add(PackageString); }
            TArray<FAssetData> Assets;
            Registry.GetAssetsByPackageName(Package, Assets, true);
            if (Assets.IsEmpty()) { Incomplete.Add(FString::Printf(TEXT("No on-disk asset data: %s"), *PackageString)); continue; }
            // Unfiltered Package dependencies preserve both hard/soft and game/editor-only
            // variants. FName-only GetDependencies discards these properties.
            TArray<FAssetDependency> Dependencies;
            if (!Registry.GetDependencies(FAssetIdentifier(Package), Dependencies, UE::AssetRegistry::EDependencyCategory::Package))
            {
                Incomplete.Add(FString::Printf(TEXT("Dependency data unavailable: %s"), *PackageString));
                continue;
            }
            struct FPackageDependencyFlags
            {
                bool bPropertiesObserved = true;
                bool bAnyGame = false;
                bool bAnyEditorOnly = false;
                bool bAnyHard = false;
                bool bAnySoft = false;
                bool bAnyBuild = false;
                TArray<TSharedPtr<FJsonValue>> Variants;
            };
            TMap<FName, FPackageDependencyFlags> ByPackage;
            Dependencies.Sort([](const FAssetDependency& A, const FAssetDependency& B) { return A.LexicalLess(B); });
            for (const FAssetDependency& Dependency : Dependencies)
            {
                if (Dependency.Category != UE::AssetRegistry::EDependencyCategory::Package || !Dependency.AssetId.IsPackage())
                {
                    Incomplete.Add(FString::Printf(TEXT("Unexpected non-package dependency identifier: %s -> %s"), *PackageString, *Dependency.AssetId.ToString()));
                    continue;
                }
                const auto Properties = Dependency.Properties;
                auto& Flags = ByPackage.FindOrAdd(Dependency.AssetId.PackageName);
                const bool bKnownProperties = (static_cast<uint8>(Properties) & ~static_cast<uint8>(UE::AssetRegistry::EDependencyProperty::PackageMask)) == 0;
                const bool bGame = EnumHasAnyFlags(Properties, UE::AssetRegistry::EDependencyProperty::Game);
                const bool bHard = EnumHasAnyFlags(Properties, UE::AssetRegistry::EDependencyProperty::Hard);
                const bool bBuild = EnumHasAnyFlags(Properties, UE::AssetRegistry::EDependencyProperty::Build);
                Flags.bPropertiesObserved &= bKnownProperties;
                Flags.bAnyGame |= bGame;
                Flags.bAnyEditorOnly |= !bGame;
                Flags.bAnyHard |= bHard;
                Flags.bAnySoft |= !bHard;
                Flags.bAnyBuild |= bBuild;
                TSharedRef<FJsonObject> Variant = MakeShared<FJsonObject>();
                Variant->SetNumberField(TEXT("properties"), static_cast<uint8>(Properties));
                Variant->SetBoolField(TEXT("properties_observed"), bKnownProperties);
                Variant->SetBoolField(TEXT("game"), bGame);
                Variant->SetBoolField(TEXT("editor_only"), !bGame);
                Variant->SetBoolField(TEXT("hard"), bHard);
                Variant->SetBoolField(TEXT("soft"), !bHard);
                Variant->SetBoolField(TEXT("build"), bBuild);
                Flags.Variants.Add(MakeShared<FJsonValueObject>(Variant));
            }
            TArray<FName> DependencyPackages;
            ByPackage.GetKeys(DependencyPackages);
            DependencyPackages.Sort(FNameLexicalLess());
            for (const FName Dependency : DependencyPackages)
            {
                const auto& Flags = ByPackage.FindChecked(Dependency);
                const FString Name = Dependency.ToString();
                TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
                Edge->SetStringField(TEXT("from"), PackageString);
                Edge->SetStringField(TEXT("to"), Name);
                Edge->SetBoolField(TEXT("dependency_properties_observed"), Flags.bPropertiesObserved);
                Edge->SetBoolField(TEXT("any_game"), Flags.bAnyGame);
                Edge->SetBoolField(TEXT("any_editor_only"), Flags.bAnyEditorOnly);
                Edge->SetBoolField(TEXT("any_hard"), Flags.bAnyHard);
                Edge->SetBoolField(TEXT("any_soft"), Flags.bAnySoft);
                Edge->SetBoolField(TEXT("any_build"), Flags.bAnyBuild);
                Edge->SetArrayField(TEXT("property_variants"), Flags.Variants);
                const bool bForbidden = IsForbidden(Name, Prefixes);
                Edge->SetBoolField(TEXT("forbidden"), bForbidden);
                Edges.Add(MakeShared<FJsonValueObject>(Edge));
                if (bForbidden) { Violations.Add(MakeShared<FJsonValueObject>(Edge)); }
                if (bRecursive && !Name.StartsWith(TEXT("/Script/")) && !Seen.Contains(Dependency))
                {
                    Seen.Add(Dependency);
                    Queue.Add(Dependency);
                }
            }
        }
        if (Processed < Queue.Num()) { Incomplete.Add(TEXT("MaxPackages budget exhausted; increase budget or split the root package batch.")); }
        if (!DirtyScanned.IsEmpty()) { Incomplete.Add(TEXT("Loaded dirty packages have unsaved state that is not represented by the on-disk dependency graph.")); }
        Result->SetBoolField(TEXT("success"), Incomplete.IsEmpty());
        Result->SetStringField(TEXT("stage"), Incomplete.IsEmpty() ? TEXT("complete") : TEXT("incomplete"));
        Result->SetBoolField(TEXT("passed"), Incomplete.IsEmpty() && Violations.IsEmpty());
        Result->SetBoolField(TEXT("recursive"), bRecursive);
        Result->SetBoolField(TEXT("packaging_verified"), false);
        Result->SetStringField(TEXT("scope"), TEXT("saved_asset_registry_hard_and_soft_package_dependencies"));
        Result->SetNumberField(TEXT("packages_processed"), Processed);
        Result->SetNumberField(TEXT("packages_queued"), Queue.Num());
        Result->SetNumberField(TEXT("max_packages"), MaxPackages);
        Result->SetArrayField(TEXT("root_packages"), Strings(PackageNames));
        Result->SetArrayField(TEXT("forbidden_prefixes"), Strings(Prefixes));
        Result->SetArrayField(TEXT("edges"), Edges);
        Result->SetArrayField(TEXT("violations"), Violations);
        Result->SetArrayField(TEXT("incomplete_reasons"), Strings(Incomplete));
        Result->SetArrayField(TEXT("dirty_scanned_packages"), Strings(DirtyScanned));
        return Result;
    }

    TSharedRef<FJsonObject> ReadSkeletalMeshSections(const FString& SkeletalMeshPath, int32 LODIndex)
    {
        USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
        if (!Mesh) { return Failure(TEXT("load"), TEXT("SkeletalMeshPath must resolve to a SkeletalMesh asset.")); }
        if (Mesh->IsCompiling())
        {
            TSharedRef<FJsonObject> Result = Failure(TEXT("compile_pending"), TEXT("Skeletal mesh compilation is in progress; retry the read when the engine finishes."));
            Result->SetBoolField(TEXT("retryable"), true);
            return Result;
        }
        const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
        if (!Model || !Model->LODModels.IsValidIndex(LODIndex)) { return Failure(TEXT("read"), TEXT("Invalid LOD index or missing imported model.")); }
        const FSkeletalMeshLODModel& LOD = Model->LODModels[LODIndex];
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("stage"), TEXT("complete"));
        Result->SetStringField(TEXT("skeletal_mesh_path"), Mesh->GetPathName());
        Result->SetNumberField(TEXT("lod_index"), LODIndex);
        Result->SetBoolField(TEXT("package_dirty"), Mesh->GetOutermost()->IsDirty());
        TArray<TSharedPtr<FJsonValue>> Sections;
        for (int32 Index = 0; Index < LOD.Sections.Num(); ++Index)
        {
            const FSkelMeshSection& Section = LOD.Sections[Index];
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("section_index"), Index);
            Row->SetNumberField(TEXT("material_index"), Section.MaterialIndex);
            Row->SetNumberField(TEXT("original_data_section_index"), Section.OriginalDataSectionIndex);
            Row->SetNumberField(TEXT("chunked_parent_section_index"), Section.ChunkedParentSectionIndex);
            Row->SetBoolField(TEXT("enabled"), !Section.bDisabled);
            if (const FSkelMeshSourceSectionUserData* UserData = LOD.UserSectionsData.Find(Section.OriginalDataSectionIndex))
            {
                Row->SetBoolField(TEXT("source_enabled"), !UserData->bDisabled);
            }
            Sections.Add(MakeShared<FJsonValueObject>(Row));
        }
        Result->SetArrayField(TEXT("sections"), Sections);
        return Result;
    }

    TSharedRef<FJsonObject> CompileBlueprint(const FString& BlueprintPath, bool bSave, bool bRefreshNodes)
    {
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint) { return Failure(TEXT("load"), TEXT("BlueprintPath must resolve to a Blueprint asset.")); }
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("name"), Blueprint->GetName());
        Result->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
        Result->SetBoolField(TEXT("package_dirty_before"), Blueprint->GetOutermost()->IsDirty());
        Result->SetBoolField(TEXT("was_dirty_before_compile"), Blueprint->GetOutermost()->IsDirty());
        Result->SetBoolField(TEXT("refresh_nodes_requested"), bRefreshNodes);
        Result->SetBoolField(TEXT("refreshed_nodes"), bRefreshNodes);
        Result->SetBoolField(TEXT("save_requested"), bSave);
        Result->SetBoolField(TEXT("requested_save"), bSave);
        Result->SetBoolField(TEXT("mutation_performed"), true);
        Result->SetStringField(TEXT("retry_policy"), TEXT("inspect_result_before_explicit_reexecution"));
        Result->SetStringField(TEXT("compile_action"), TEXT("performed"));
        Result->SetStringField(TEXT("validation_scope"), TEXT("compiler_messages_and_generated_class_only"));
        Result->SetBoolField(TEXT("reload_verified"), false);
        Result->SetBoolField(TEXT("runtime_verified"), false);
        if (bRefreshNodes) { FBlueprintEditorUtils::RefreshAllNodes(Blueprint); }
        FCompilerResultsLog Log;
        Log.SetSourcePath(Blueprint->GetPathName());
        // SaveOnCompile editor preferences must not silently save this asset
        // (or compiled dependencies). The explicit validated save below owns it.
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Log);
        TArray<TSharedPtr<FJsonValue>> Messages;
        TArray<TSharedPtr<FJsonValue>> Errors;
        TArray<TSharedPtr<FJsonValue>> Warnings;
        TArray<TSharedPtr<FJsonValue>> Diagnostics;
        for (const TSharedRef<FTokenizedMessage>& Message : Log.Messages)
        {
            TSharedRef<FJsonObject> Row = CompilerDiagnostic(Message, Log);
            Row->SetNumberField(TEXT("severity_code"), static_cast<int32>(Message->GetSeverity()));
            Messages.Add(MakeShared<FJsonValueObject>(Row));
            if (static_cast<int32>(Message->GetSeverity()) <= static_cast<int32>(EMessageSeverity::Error))
            {
                Errors.Add(MakeShared<FJsonValueString>(Message->ToText().ToString()));
                Diagnostics.Add(MakeShared<FJsonValueObject>(Row));
            }
            else if (Message->GetSeverity() == EMessageSeverity::Warning || Message->GetSeverity() == EMessageSeverity::PerformanceWarning)
            {
                Warnings.Add(MakeShared<FJsonValueString>(Message->ToText().ToString()));
                Diagnostics.Add(MakeShared<FJsonValueObject>(Row));
            }
        }
        const bool bStatusPassed = Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;
        auto AddFallback = [&Diagnostics](TArray<TSharedPtr<FJsonValue>>& Summary, const TCHAR* Severity, const TCHAR* Text)
        {
            Summary.Add(MakeShared<FJsonValueString>(Text));
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("severity"), Severity);
            Row->SetStringField(TEXT("message"), Text);
            Diagnostics.Add(MakeShared<FJsonValueObject>(Row));
        };
        if ((Log.NumErrors > 0 || !bStatusPassed || !Blueprint->GeneratedClass) && Errors.IsEmpty())
        {
            AddFallback(Errors, TEXT("error"), TEXT("Compiler status or generated class failed validation without tokenized error details; inspect the editor Message Log."));
        }
        if (Log.NumWarnings > 0 && Warnings.IsEmpty()) { AddFallback(Warnings, TEXT("warning"), TEXT("Compiler reported warnings without tokenized warning details.")); }
        const int32 ErrorCount = FMath::Max(Log.NumErrors, Errors.Num());
        const int32 WarningCount = FMath::Max(Log.NumWarnings, Warnings.Num());
        const bool bCompiled = ErrorCount == 0 && bStatusPassed && Blueprint->GeneratedClass != nullptr;
        Result->SetBoolField(TEXT("compiled"), bCompiled);
        Result->SetBoolField(TEXT("validation_pass"), bCompiled);
        Result->SetBoolField(TEXT("has_warnings"), WarningCount > 0 || Blueprint->Status == BS_UpToDateWithWarnings);
        Result->SetStringField(TEXT("compile_status"), bCompiled ? (WarningCount ? TEXT("passed_with_warnings") : TEXT("passed")) : TEXT("failed"));
        const TCHAR* StatusName = TEXT("unknown");
        switch (Blueprint->Status.GetValue())
        {
            case BS_Dirty: StatusName = TEXT("dirty"); break;
            case BS_Error: StatusName = TEXT("error"); break;
            case BS_UpToDate: StatusName = TEXT("up_to_date"); break;
            case BS_UpToDateWithWarnings: StatusName = TEXT("up_to_date_with_warnings"); break;
            case BS_BeingCreated: StatusName = TEXT("being_created"); break;
            default: break;
        }
        Result->SetStringField(TEXT("status"), StatusName);
        Result->SetNumberField(TEXT("blueprint_status_code"), static_cast<int32>(Blueprint->Status.GetValue()));
        Result->SetNumberField(TEXT("error_count"), ErrorCount);
        Result->SetNumberField(TEXT("warning_count"), WarningCount);
        Result->SetNumberField(TEXT("compile_error_count"), ErrorCount);
        Result->SetNumberField(TEXT("compile_warning_count"), WarningCount);
        Result->SetArrayField(TEXT("compile_errors"), Errors);
        Result->SetArrayField(TEXT("compile_warnings"), Warnings);
        Result->SetArrayField(TEXT("diagnostics"), Diagnostics);
        Result->SetNumberField(TEXT("diagnostic_count"), Diagnostics.Num());
        Result->SetArrayField(TEXT("messages"), Messages);
        const bool bSaved = bSave && bCompiled && UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetBoolField(TEXT("package_dirty_after"), Blueprint->GetOutermost()->IsDirty());
        Result->SetBoolField(TEXT("dirty_after_compile"), Blueprint->GetOutermost()->IsDirty());
        Result->SetBoolField(TEXT("success"), bCompiled && (!bSave || bSaved));
        Result->SetStringField(TEXT("stage"), !bCompiled ? TEXT("compile") : (bSave && !bSaved ? TEXT("save") : TEXT("complete")));
        if (!bCompiled) { Result->SetStringField(TEXT("error"), TEXT("Blueprint compilation failed; inspect compiler messages before retrying. Not saved.")); }
        else if (bSave && !bSaved) { Result->SetStringField(TEXT("error"), TEXT("Compilation passed but save failed; generated state may have changed. Inspect the dirty package before retrying.")); }
        return Result;
    }

    TSharedRef<FJsonObject> InspectBlueprintParent(const FString& ClassPath)
    {
        UClass* Class = LoadObject<UClass>(nullptr, *ClassPath);
        if (!Class) { return Failure(TEXT("load"), TEXT("ClassPath must resolve to a reflected UClass.")); }
        const TArray<FString> Forbidden = ForbiddenPrefixes();
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        TArray<FString> Paths;
        TArray<FString> Reasons;
        TArray<TSharedPtr<FJsonValue>> Checks;
        TSet<const UClass*> Seen;
        for (const UClass* Current = Class; Current; Current = Current->GetSuperClass())
        {
            if (Seen.Contains(Current) || Seen.Num() >= 256)
            {
                Reasons.Add(TEXT("Superclass traversal is cyclic or exceeded 256 classes."));
                break;
            }
            Seen.Add(Current);
            Paths.Add(Current->GetPathName());
            const UPackage* Package = Current->GetOutermost();
            const FString PackageName = Package->GetName();
            const UObject* DefaultObject = Current->GetDefaultObject(false);
            const bool bEditorPackage = Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly);
            const bool bForbiddenPackage = IsForbidden(PackageName, Forbidden) || PackageName.StartsWith(TEXT("/Engine/PythonTypes"));
            const bool bEditorObject = Current->IsEditorOnly() || (DefaultObject && DefaultObject->IsEditorOnly());
            // Native UClasses intentionally carry RF_Transient: they are reconstructed
            // from compiled code, not serialized as asset exports. See UE 5.8
            // UObjectConstructInternal.h:114 and Class.cpp's static UClass constructor.
            const bool bTransientFlag = Current->HasAnyFlags(RF_Transient);
            const bool bCompiledNativeScriptClass = Current->HasAnyClassFlags(CLASS_Native)
                && Package->HasAnyPackageFlags(PKG_CompiledIn) && PackageName.StartsWith(TEXT("/Script/"));
            const bool bTransientGeneratedClass = bTransientFlag && !bCompiledNativeScriptClass;
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("class_path"), Current->GetPathName());
            Row->SetStringField(TEXT("package_name"), PackageName);
            Row->SetBoolField(TEXT("editor_developer_or_uncooked_package"), bEditorPackage);
            Row->SetBoolField(TEXT("forbidden_tool_package"), bForbiddenPackage);
            Row->SetBoolField(TEXT("editor_only_class_or_default_object"), bEditorObject);
            Row->SetBoolField(TEXT("transient_class"), bTransientFlag);
            Row->SetBoolField(TEXT("compiled_native_script_class"), bCompiledNativeScriptClass);
            Row->SetBoolField(TEXT("transient_generated_class"), bTransientGeneratedClass);
            Row->SetBoolField(TEXT("default_object_available"), DefaultObject != nullptr);
            Checks.Add(MakeShared<FJsonValueObject>(Row));
            if (bEditorPackage || bForbiddenPackage || bEditorObject || bTransientGeneratedClass)
            {
                TArray<FString> RejectedFlags;
                if (bEditorPackage) { RejectedFlags.Add(TEXT("editor_developer_or_uncooked_package")); }
                if (bForbiddenPackage) { RejectedFlags.Add(TEXT("forbidden_tool_package")); }
                if (bEditorObject) { RejectedFlags.Add(TEXT("editor_only_class_or_default_object")); }
                if (bTransientGeneratedClass) { RejectedFlags.Add(TEXT("transient_generated_class")); }
                Reasons.Add(FString::Printf(TEXT("Unsupported persistent game Blueprint parent %s: %s"), *Current->GetPathName(), *FString::Join(RejectedFlags, TEXT(", "))));
            }
            if (!DefaultObject) { Reasons.Add(FString::Printf(TEXT("Default-object editor-only state is unavailable: %s"), *Current->GetPathName())); }
        }
        Result->SetBoolField(TEXT("success"), Reasons.IsEmpty());
        Result->SetStringField(TEXT("stage"), Reasons.IsEmpty() ? TEXT("complete") : TEXT("parent_validation"));
        Result->SetArrayField(TEXT("parent_chain"), Strings(Paths));
        Result->SetArrayField(TEXT("checks"), Checks);
        Result->SetArrayField(TEXT("reasons"), Strings(Reasons));
        Result->SetStringField(TEXT("reason"), FString::Join(Reasons, TEXT("; ")));
        Result->SetStringField(TEXT("validation_scope"), TEXT("superclass_package_flags_and_existing_default_object_editor_only_state"));
        Result->SetBoolField(TEXT("complete_dependency_scan"), false);
        if (!Reasons.IsEmpty()) { Result->SetStringField(TEXT("error"), FString::Join(Reasons, TEXT("; "))); }
        return Result;
    }
    // CreationFix1: public C++ fields only; Python property protection is not bypassed.
    FString InspectBlueprintFunctionLibraryCreation(UObject* FactoryObject, UObject* BlueprintObject, const FString& ExpectedObjectPath)
    {
        auto Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);Result->SetBoolField(TEXT("read_only"), true);
        Result->SetBoolField(TEXT("mutation_performed"), false);Result->SetBoolField(TEXT("observations_complete"), false);
        Result->SetBoolField(TEXT("load_requested"), false);Result->SetBoolField(TEXT("creation_requested"), false);
        Result->SetBoolField(TEXT("compile_requested"), false);Result->SetBoolField(TEXT("save_requested"), false);
        Result->SetBoolField(TEXT("cdo_creation_requested"), false);Result->SetBoolField(TEXT("cdo_values_sampled"), false);
        const auto Encode = [&]() { FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));return Text; };
        const auto Bad = [&](const TCHAR* Boundary,const TCHAR* Message)
        {Result->SetStringField(TEXT("stage"),Boundary);Result->SetStringField(TEXT("error_code"),TEXT("function_library_creation_observation_failed"));Result->SetStringField(TEXT("error"),Message);return Encode();};
        const auto Loaded = [](const UObject* Object)
        {return IsValid(Object)&&!Object->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects);};
        if(!IsEditorGameThread()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Loaded(GEditor)||!GEditor->PlayWorld.IsResolved()||IsPlaySessionActive())
            return Bad(TEXT("context"),TEXT("Read-only BFL creation observation requires the current idle editor outside import, exit, GC or saving."));
        UWorld* const WorldBefore=GWorld;if(WorldBefore&&!Loaded(WorldBefore))return Bad(TEXT("context"),TEXT("Current optional editor world is unavailable."));
        TStrongObjectPtr<UEditorEngine> EditorHold(GEditor);TStrongObjectPtr<UWorld> WorldHold(WorldBefore);auto* const UndoBefore=GUndo;
        const auto Context = [&]()
        {return IsEditorGameThread()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&Loaded(EditorHold.Get())&&EditorHold->PlayWorld.IsResolved()&&GEditor==EditorHold.Get()&&GUndo==UndoBefore&&GWorld==WorldHold.Get()&&(!WorldHold.IsValid()||(Loaded(WorldHold.Get())&&WorldHold->WorldType==EWorldType::Editor&&!WorldHold->bIsTearingDown))&&!IsPlaySessionActive();};
        if(!Context())return Bad(TEXT("context"),TEXT("Captured editor/world/transaction role is not current."));
        if(ExpectedObjectPath.IsEmpty()||ExpectedObjectPath.Len()>=NAME_SIZE||ExpectedObjectPath.Contains(TEXT(":"))||!ExpectedObjectPath.StartsWith(TEXT("/Game/")))
            return Bad(TEXT("selector"),TEXT("Use an exact bounded /Game Blueprint object path, not an alias or subobject."));
        for(TCHAR Character:ExpectedObjectPath)if(Character==0||FChar::IsControl(Character))return Bad(TEXT("selector"),TEXT("Object path contains control/NUL data."));
        if(!FPackageName::IsValidObjectPath(ExpectedObjectPath))return Bad(TEXT("selector"),TEXT("Expected object path is invalid."));
        const FString PackagePath=FPackageName::ObjectPathToPackageName(ExpectedObjectPath);const FString AssetName=FPackageName::GetShortName(PackagePath);
        if(!ExpectedObjectPath.Equals(PackagePath+TEXT(".")+AssetName,ESearchCase::CaseSensitive)||(FactoryObject==nullptr)==(BlueprintObject==nullptr))
            return Bad(TEXT("selector"),TEXT("Exactly one factory or Blueprint pointer and a canonical Package.Asset target are required."));
        UClass* const FactoryClass=FindObject<UClass>(nullptr,TEXT("/Script/UnrealEd.BlueprintFunctionLibraryFactory"));
        UClass* const ParentClass=FindObject<UClass>(nullptr,TEXT("/Script/Engine.BlueprintFunctionLibrary"));
        UClass* const BlueprintClass=FindObject<UClass>(nullptr,TEXT("/Script/Engine.Blueprint"));
        if(!Context()||!Loaded(FactoryClass)||!Loaded(ParentClass)||!Loaded(BlueprintClass)||FactoryClass!=UBlueprintFunctionLibraryFactory::StaticClass()||ParentClass!=UBlueprintFunctionLibrary::StaticClass()||BlueprintClass!=UBlueprint::StaticClass())
            return Bad(TEXT("native_class"),TEXT("Exact already registered stock factory/BFL/Blueprint classes are required; no class load was requested."));
        struct FRole
        {TStrongObjectPtr<UObject> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;bool Dirty=false,IsClass=false;uint32 ClassFlags=0;int32 ClassSize=0;const UStruct* Super=nullptr;TObjectPtr<UObject> GeneratedBy;};
        TArray<FRole> Roles;
        const auto HoldRole = [&](UObject* Object,bool IsClass=false)
        {
            if(!Context()||!Loaded(Object)||!Loaded(Object->GetClass())||!Loaded(Object->GetPackage()))return false;
            for(const auto& Role:Roles)if(Role.Hold.Get()==Object)return true;
            FRole Role;Role.Hold.Reset(Object);Role.Class=Object->GetClass();Role.Outer=Object->GetOuter();Role.Package=Object->GetPackage();Role.Path=Object->GetPathName();Role.Flags=Object->GetFlags();Role.PackageFlags=uint32(Role.Package->GetPackageFlags());Role.Dirty=Role.Package->IsDirty();Role.IsClass=IsClass;
            if(IsClass){auto* Class=static_cast<UClass*>(Object);if(!Class->ClassGeneratedBy.IsResolved())return false;Role.GeneratedBy=Class->ClassGeneratedBy;Role.ClassFlags=uint32(Class->GetClassFlags());Role.ClassSize=Class->GetStructureSize();auto Super=Class->GetSuperStructIterator();++Super;Role.Super=Super?*Super:nullptr;}
            Roles.Add(MoveTemp(Role));return true;
        };
        const auto Current = [&]()
        {
            if(!Context())return false;
            for(const auto& Role:Roles)
            {const UObject* Object=Role.Hold.Get();if(!Loaded(Object)||Object->GetClass()!=Role.Class||Object->GetOuter()!=Role.Outer||Object->GetPackage()!=Role.Package||!Loaded(Role.Package)||!Object->GetPathName().Equals(Role.Path,ESearchCase::CaseSensitive)||Object->GetFlags()!=Role.Flags||uint32(Role.Package->GetPackageFlags())!=Role.PackageFlags||Role.Package->IsDirty()!=Role.Dirty||FindObject<UObject>(nullptr,*Role.Path)!=Object)return false;
                if(Role.IsClass){const auto* Class=static_cast<const UClass*>(Object);auto Super=Class->GetSuperStructIterator();++Super;if(!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy!=Role.GeneratedBy||uint32(Class->GetClassFlags())!=Role.ClassFlags||Class->GetStructureSize()!=Role.ClassSize||(Super?*Super:nullptr)!=Role.Super)return false;}}
            return Context();
        };
        for(UClass* Class:{FactoryClass,ParentClass,BlueprintClass})if(!Class->HasAnyClassFlags(CLASS_Native)||!HoldRole(Class,true)||!HoldRole(Class->GetPackage()))return Bad(TEXT("native_class"),TEXT("Current loaded stock native identity is unavailable."));
        UObject* const Selected=FactoryObject?FactoryObject:BlueprintObject;
        if(!HoldRole(Selected)||!Current())return Bad(TEXT("input_identity"),TEXT("Passed factory/Blueprint object identity is unavailable; no lookup fallback was used."));
        Result->SetStringField(TEXT("observed_object_path"),Selected->GetPathName());Result->SetStringField(TEXT("observed_class_path"),Selected->GetClass()->GetPathName());
        if(FactoryObject)
        {
            Result->SetStringField(TEXT("observation_kind"),TEXT("factory"));
            if(FactoryObject->GetClass()!=FactoryClass)return Bad(TEXT("factory_class"),TEXT("Only the exact stock BlueprintFunctionLibraryFactory is admitted."));
            auto* Factory=static_cast<UBlueprintFunctionLibraryFactory*>(FactoryObject);const auto ParentHandle=Factory->ParentClass.GetGCPtr();const EBlueprintType Type=Factory->BlueprintType;
            Result->SetNumberField(TEXT("blueprint_type_value"),int32(Type));Result->SetBoolField(TEXT("parent_resolved"),ParentHandle.IsResolved());
            if(!ParentHandle.IsResolved()||ParentHandle!=ParentClass||Type!=BPTYPE_FunctionLibrary||!Current())return Bad(TEXT("factory_fields"),TEXT("Stock factory public parent/type fields or current identity differ."));
            if(FindObject<UObject>(nullptr,*ExpectedObjectPath)!=nullptr||!Current())return Bad(TEXT("fresh_target"),TEXT("Expected fresh target is already loaded, or current factory state changed."));
            Result->SetStringField(TEXT("factory_class_path"),TEXT("/Script/UnrealEd.BlueprintFunctionLibraryFactory"));Result->SetStringField(TEXT("parent_class_path"),TEXT("/Script/Engine.BlueprintFunctionLibrary"));
            Result->SetStringField(TEXT("blueprint_type"),TEXT("BPTYPE_FunctionLibrary"));Result->SetBoolField(TEXT("target_loaded_before_factory"),false);
        }
        else
        {
            Result->SetStringField(TEXT("observation_kind"),TEXT("blueprint"));
            if(BlueprintObject->GetClass()!=BlueprintClass||!BlueprintObject->GetPathName().Equals(ExpectedObjectPath,ESearchCase::CaseSensitive)||FindObject<UObject>(nullptr,*ExpectedObjectPath)!=BlueprintObject)
                return Bad(TEXT("blueprint_identity"),TEXT("Passed Blueprint is not the exact current returned object."));
            auto* Blueprint=static_cast<UBlueprint*>(BlueprintObject);UPackage* const Package=Blueprint->GetPackage();
            if(Blueprint->GetOuter()!=Package||!Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)||Package->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||!HoldRole(Package))
                return Bad(TEXT("blueprint_package"),TEXT("Returned Blueprint package/outer must remain the exact runtime target."));
            const auto ParentHandle=Blueprint->ParentClass.GetGCPtr();const auto GeneratedHandle=Blueprint->GeneratedClass.GetGCPtr();const auto SkeletonHandle=Blueprint->SkeletonGeneratedClass.GetGCPtr();
            const EBlueprintType Type=Blueprint->BlueprintType;const auto Status=Blueprint->Status;Result->SetNumberField(TEXT("blueprint_status_value"),int32(Status.GetValue()));
            Result->SetNumberField(TEXT("blueprint_type_value"),int32(Type));Result->SetBoolField(TEXT("parent_resolved"),ParentHandle.IsResolved());Result->SetBoolField(TEXT("scs_resolved"),Blueprint->SimpleConstructionScript.IsResolved());
            Result->SetBoolField(TEXT("simple_construction_script_is_null"),Blueprint->SimpleConstructionScript.IsResolved()&&Blueprint->SimpleConstructionScript==nullptr);Result->SetNumberField(TEXT("own_member_count"),Blueprint->NewVariables.Num());
            if(!Current()||!ParentHandle.IsResolved()||ParentHandle!=ParentClass||Type!=BPTYPE_FunctionLibrary||Blueprint->bBeingCompiled||Blueprint->bQueuedForCompilation||Blueprint->bIsRegeneratingOnLoad||Blueprint->bSuppressStructurallyModified||Status==BS_BeingCreated||!Blueprint->SimpleConstructionScript.IsResolved()||Blueprint->SimpleConstructionScript!=nullptr||!Blueprint->NewVariables.IsEmpty()||!GeneratedHandle.IsResolved()||!SkeletonHandle.IsResolved())
                return Bad(TEXT("blueprint_fields"),TEXT("Exact idle BFL parent/type, null SCS, member-free and resolved optional class fields are required."));
            const auto ObserveClass = [&](const TCHAR* RoleName,const auto& Handle,const FString& ClassPath)
            {
                const bool Available=Handle!=nullptr;Result->SetBoolField(FString(RoleName)+TEXT("_available"),Available);Result->SetStringField(FString(RoleName)+TEXT("_path"),Available?ClassPath:FString());
                if(!Available)return Current();
                UClass* Class=FindObject<UClass>(nullptr,*ClassPath);
                if(!Current()||!Loaded(Class)||Handle!=Class||Class->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Class->GetOuter()!=Package||Class->GetPackage()!=Package||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy!=Blueprint||Class->HasAnyClassFlags(CLASS_Native|CLASS_Interface|CLASS_Deprecated|CLASS_NewerVersionExists)||!HoldRole(Class,true))return false;
                auto Super=Class->GetSuperStructIterator();++Super;if(!Super||*Super!=ParentClass)return false;
                Result->SetStringField(FString(RoleName)+TEXT("_pointer"),FString::Printf(TEXT("%p"),Class));return Current();
            };
            if(!ObserveClass(TEXT("generated_class"),GeneratedHandle,ExpectedObjectPath+TEXT("_C"))||!ObserveClass(TEXT("skeleton_class"),SkeletonHandle,PackagePath+TEXT(".SKEL_")+AssetName+TEXT("_C")))
                return Bad(TEXT("blueprint_classes"),TEXT("Optional generated/skeleton role is not the exact currently held target class; no class/CDO load or creation was requested."));
            if(!Current()||!Blueprint->ParentClass.GetGCPtr().IsResolved()||Blueprint->ParentClass.GetGCPtr()!=ParentHandle||!Blueprint->GeneratedClass.GetGCPtr().IsResolved()||Blueprint->GeneratedClass.GetGCPtr()!=GeneratedHandle||!Blueprint->SkeletonGeneratedClass.GetGCPtr().IsResolved()||Blueprint->SkeletonGeneratedClass.GetGCPtr()!=SkeletonHandle||Blueprint->BlueprintType!=Type||Blueprint->Status!=Status||!Blueprint->SimpleConstructionScript.IsResolved()||Blueprint->SimpleConstructionScript!=nullptr||!Blueprint->NewVariables.IsEmpty())
                return Bad(TEXT("blueprint_current_fields"),TEXT("Public BFL roles changed during read-only observation."));
            Result->SetStringField(TEXT("parent_class_path"),TEXT("/Script/Engine.BlueprintFunctionLibrary"));Result->SetStringField(TEXT("blueprint_type"),TEXT("BPTYPE_FunctionLibrary"));TArray<TSharedPtr<FJsonValue>> NoMembers;Result->SetArrayField(TEXT("own_member_names"),NoMembers);
            Result->SetBoolField(TEXT("no_scs_verified"),true);Result->SetBoolField(TEXT("no_own_members_verified"),true);Result->SetBoolField(TEXT("native_parent_verified"),true);
        }
        if(!Current())return Bad(TEXT("final_current"),TEXT("Held target/native/context roles changed; no follow-up action was performed."));
        TArray<TSharedPtr<FJsonValue>> RoleRows;
        for(const auto& Role:Roles){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),Role.Path);Row->SetStringField(TEXT("pointer"),FString::Printf(TEXT("%p"),Role.Hold.Get()));Row->SetStringField(TEXT("class_pointer"),FString::Printf(TEXT("%p"),Role.Class));Row->SetStringField(TEXT("outer_pointer"),FString::Printf(TEXT("%p"),Role.Outer));Row->SetStringField(TEXT("package_pointer"),FString::Printf(TEXT("%p"),Role.Package));Row->SetStringField(TEXT("object_flags"),LexToString(uint32(Role.Flags)));Row->SetNumberField(TEXT("package_flags"),Role.PackageFlags);Row->SetBoolField(TEXT("package_dirty"),Role.Dirty);Row->SetBoolField(TEXT("is_class"),Role.IsClass);if(Role.IsClass){Row->SetNumberField(TEXT("class_flags"),Role.ClassFlags);Row->SetNumberField(TEXT("class_size"),Role.ClassSize);Row->SetStringField(TEXT("super_pointer"),FString::Printf(TEXT("%p"),Role.Super));}RoleRows.Add(MakeShared<FJsonValueObject>(Row));}Result->SetArrayField(TEXT("identity_roles"),RoleRows);
        Result->SetStringField(TEXT("expected_object_path"),ExpectedObjectPath);Result->SetBoolField(TEXT("current_identity_verified"),true);Result->SetBoolField(TEXT("observations_complete"),true);Result->SetBoolField(TEXT("success"),true);Result->SetStringField(TEXT("stage"),TEXT("complete"));
        Result->SetStringField(TEXT("scope"),TEXT("Read-only exact BFL factory/returned asset public fields and held identity; no private reflection, loaders, constructors, CDO reads, compiler, save or runtime execution. Not global callback/GC purity proof."));return Encode();
    }

}
