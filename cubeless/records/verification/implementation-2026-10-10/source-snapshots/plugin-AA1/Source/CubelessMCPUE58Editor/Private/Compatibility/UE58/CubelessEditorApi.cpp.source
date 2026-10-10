#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessCompilerDiagnostics.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
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
            TArray<FName> Dependencies;
            if (!Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package))
            {
                Incomplete.Add(FString::Printf(TEXT("Dependency data unavailable: %s"), *PackageString));
                continue;
            }
            Dependencies.Sort(FNameLexicalLess());
            for (const FName Dependency : Dependencies)
            {
                const FString Name = Dependency.ToString();
                TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
                Edge->SetStringField(TEXT("from"), PackageString);
                Edge->SetStringField(TEXT("to"), Name);
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
}
