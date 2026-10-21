#include "Registration/CubelessNativeToolset.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Operations/CubelessInspectionOperations.h"
#include "Compatibility/UE58/CubelessSkeletalMeshSectionApi.h"

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

FString UCubelessNativeToolset::SetSkeletalMeshSectionEnabled(const FString& SkeletalMeshPath, int32 LODIndex, int32 SectionIndex,
    bool bEnabled, bool bSave, bool bDryRun)
{
    return Cubeless::UE58::SkeletalMeshSections::SetSectionEnabled(SkeletalMeshPath, LODIndex, SectionIndex, bEnabled, bSave, bDryRun);
}

FString UCubelessNativeToolset::CompileBlueprintValidation(const FString& BlueprintPath, bool bSave, bool bRefreshNodes)
{
    return Cubeless::Operations::CompileBlueprintValidation(BlueprintPath, bSave, bRefreshNodes);
}

FString UCubelessNativeToolset::InspectBlueprintParent(const FString& ClassPath)
{
    return Cubeless::Operations::InspectBlueprintParent(ClassPath);
}

FString UCubelessNativeToolset::InspectBlueprintFunctionLibraryCreation(UObject* FactoryObject, UObject* BlueprintObject, const FString& ExpectedObjectPath)
{
    return Cubeless::UE58::InspectBlueprintFunctionLibraryCreation(FactoryObject, BlueprintObject, ExpectedObjectPath);
}
