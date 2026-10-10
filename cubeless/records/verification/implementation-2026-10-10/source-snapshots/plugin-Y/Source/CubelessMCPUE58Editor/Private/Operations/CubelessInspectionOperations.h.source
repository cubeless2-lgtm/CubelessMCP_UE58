#pragma once

#include "CoreMinimal.h"

namespace Cubeless::Operations
{
    FString GetEditorDiagnostics();
    FString AuditPackageDependencies(const TArray<FString>& PackageNames, bool bRecursive, int32 MaxPackages);
    FString GetSkeletalMeshSections(const FString& SkeletalMeshPath, int32 LODIndex);
    FString CompileBlueprintValidation(const FString& BlueprintPath, bool bSave, bool bRefreshNodes);
    FString InspectBlueprintParent(const FString& ClassPath);
}
