#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
struct FMeshDescription;

namespace UE::Geometry
{
class FDynamicMesh3;
}

namespace RXVolumeLOD
{
bool BuildVolumeMesh(
    const USkeletalMesh& SourceMesh,
    const FMeshDescription& SourceDescription,
    int32 VoxelCount,
    int32 TargetTriangleCount,
    UE::Geometry::FDynamicMesh3& OutTargetMesh,
    FMeshDescription& OutLODDescription,
    FText& Error);
}
