#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Cubeless::UE58
{
    bool IsEditorGameThread();
    bool IsPlaySessionActive();
    bool IsLongPackageName(const FString& Name);
    TSharedRef<FJsonObject> ReadEditorDiagnostics();
    TSharedRef<FJsonObject> ReadPackageDependencies(const TArray<FString>& PackageNames, bool bRecursive, int32 MaxPackages);
    TSharedRef<FJsonObject> ReadSkeletalMeshSections(const FString& SkeletalMeshPath, int32 LODIndex);
    TSharedRef<FJsonObject> CompileBlueprint(const FString& BlueprintPath, bool bSave, bool bRefreshNodes);
    TSharedRef<FJsonObject> InspectBlueprintParent(const FString& ClassPath);
    FString InspectBlueprintFunctionLibraryCreation(UObject* FactoryObject, UObject* BlueprintObject, const FString& ExpectedObjectPath);
}
