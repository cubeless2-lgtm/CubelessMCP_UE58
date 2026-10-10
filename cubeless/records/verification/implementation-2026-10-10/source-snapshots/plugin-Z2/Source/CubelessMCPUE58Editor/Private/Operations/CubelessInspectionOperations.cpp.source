#include "Operations/CubelessInspectionOperations.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString Encode(const TSharedRef<FJsonObject>& Result, const TCHAR* Operation, double Start)
    {
        Result->SetStringField(TEXT("operation"), Operation);
        Result->SetStringField(TEXT("schema_version"), TEXT("1.0"));
        if (!Result->HasField(TEXT("mutation_performed"))) { Result->SetBoolField(TEXT("mutation_performed"), false); }
        if (!Result->HasField(TEXT("saved"))) { Result->SetBoolField(TEXT("saved"), false); }
        Result->SetBoolField(TEXT("atomic_rollback_available"), false);
        if (!Result->HasField(TEXT("retry_policy"))) { Result->SetStringField(TEXT("retry_policy"), TEXT("safe_read_only")); }
        if (!Result->HasField(TEXT("compile_action"))) { Result->SetStringField(TEXT("compile_action"), TEXT("not_requested")); }
        Result->SetNumberField(TEXT("elapsed_ms"), (FPlatformTime::Seconds() - Start) * 1000.0);
        FString Text;
        FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text));
        return Text;
    }

    TSharedRef<FJsonObject> MakeOperationError(const TCHAR* Message)
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("stage"), TEXT("preflight"));
        Result->SetStringField(TEXT("error"), Message);
        return Result;
    }
}

namespace Cubeless::Operations
{
    FString GetEditorDiagnostics()
    {
        const double Start = FPlatformTime::Seconds();
        return Encode(UE58::IsEditorGameThread() ? UE58::ReadEditorDiagnostics() : MakeOperationError(TEXT("Requires the editor game thread.")), TEXT("get_editor_diagnostics"), Start);
    }

    FString AuditPackageDependencies(const TArray<FString>& PackageNames, bool bRecursive, int32 MaxPackages)
    {
        const double Start = FPlatformTime::Seconds();
        const TCHAR* Operation = TEXT("audit_package_dependencies");
        if (!UE58::IsEditorGameThread()) { return Encode(MakeOperationError(TEXT("Requires the editor game thread.")), Operation, Start); }
        if (PackageNames.IsEmpty() || PackageNames.Num() > 256 || MaxPackages < 1 || MaxPackages > 4096)
        {
            return Encode(MakeOperationError(TEXT("Provide 1..256 package names and MaxPackages in 1..4096.")), Operation, Start);
        }
        for (const FString& Name : PackageNames)
        {
            if (Name.Len() >= NAME_SIZE)
            {
                return Encode(MakeOperationError(TEXT("PackageNames entries must be shorter than NAME_SIZE (1024 UTF-16 code units); rejected before FName construction.")), Operation, Start);
            }
            if (!UE58::IsLongPackageName(Name) || Name.StartsWith(TEXT("/Script/")))
            {
                return Encode(MakeOperationError(TEXT("PackageNames must contain asset long package names, not object paths or /Script modules.")), Operation, Start);
            }
        }
        return Encode(UE58::ReadPackageDependencies(PackageNames, bRecursive, MaxPackages), Operation, Start);
    }

    FString GetSkeletalMeshSections(const FString& SkeletalMeshPath, int32 LODIndex)
    {
        const double Start = FPlatformTime::Seconds();
        const TCHAR* Operation = TEXT("get_skeletal_mesh_sections");
        if (!UE58::IsEditorGameThread() || UE58::IsPlaySessionActive())
        {
            return Encode(MakeOperationError(TEXT("Requires the editor game thread outside PIE/SIE.")), Operation, Start);
        }
        if (SkeletalMeshPath.IsEmpty() || LODIndex < 0)
        {
            return Encode(MakeOperationError(TEXT("SkeletalMeshPath is required and LODIndex must be nonnegative.")), Operation, Start);
        }
        return Encode(UE58::ReadSkeletalMeshSections(SkeletalMeshPath, LODIndex), Operation, Start);
    }

    FString CompileBlueprintValidation(const FString& BlueprintPath, bool bSave, bool bRefreshNodes)
    {
        const double Start = FPlatformTime::Seconds();
        const TCHAR* Operation = TEXT("compile_blueprint_validation");
        if (!UE58::IsEditorGameThread() || UE58::IsPlaySessionActive())
        {
            return Encode(MakeOperationError(TEXT("Requires the editor game thread outside PIE/SIE.")), Operation, Start);
        }
        if (BlueprintPath.IsEmpty()) { return Encode(MakeOperationError(TEXT("BlueprintPath is required.")), Operation, Start); }
        return Encode(UE58::CompileBlueprint(BlueprintPath, bSave, bRefreshNodes), Operation, Start);
    }

    FString InspectBlueprintParent(const FString& ClassPath)
    {
        const double Start = FPlatformTime::Seconds();
        const TCHAR* Operation = TEXT("inspect_blueprint_parent");
        if (!UE58::IsEditorGameThread()) { return Encode(MakeOperationError(TEXT("Requires the editor game thread.")), Operation, Start); }
        if (ClassPath.IsEmpty()) { return Encode(MakeOperationError(TEXT("ClassPath is required.")), Operation, Start); }
        return Encode(UE58::InspectBlueprintParent(ClassPath), Operation, Start);
    }
}
