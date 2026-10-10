#include "Registration/CubelessNativeToolset.h"
#include "Operations/CubelessInspectionOperations.h"

FString UCubelessNativeToolset::GetEditorDiagnostics()
{
    return Cubeless::Operations::GetEditorDiagnostics();
}

FString UCubelessNativeToolset::AuditPackageDependencies(const TArray<FString>& PackageNames, bool bRecursive, int32 MaxPackages)
{
    return Cubeless::Operations::AuditPackageDependencies(PackageNames, bRecursive, MaxPackages);
}

FString UCubelessNativeToolset::GetSkeletalMeshSections(const FString& SkeletalMeshPath, int32 LODIndex)
{
    return Cubeless::Operations::GetSkeletalMeshSections(SkeletalMeshPath, LODIndex);
}

FString UCubelessNativeToolset::CompileBlueprintValidation(const FString& BlueprintPath, bool bSave, bool bRefreshNodes)
{
    return Cubeless::Operations::CompileBlueprintValidation(BlueprintPath, bSave, bRefreshNodes);
}

FString UCubelessNativeToolset::InspectBlueprintParent(const FString& ClassPath)
{
    return Cubeless::Operations::InspectBlueprintParent(ClassPath);
}
