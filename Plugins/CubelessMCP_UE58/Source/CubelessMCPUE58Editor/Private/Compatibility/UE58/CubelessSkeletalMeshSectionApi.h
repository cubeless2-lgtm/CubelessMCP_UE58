#pragma once

#include "CoreMinimal.h"

namespace Cubeless::UE58::SkeletalMeshSections
{
    FString SetSectionEnabled(const FString& SkeletalMeshPath, int32 LODIndex, int32 SectionIndex,
        bool bEnabled, bool bSave, bool bDryRun);
}
