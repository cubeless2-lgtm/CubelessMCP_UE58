#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UTexture2D;
class UMaterial;

namespace RXVolumeLOD
{
struct FOptions
{
    int32 VoxelCount = 96;
    int32 TargetTriangleCount = 12000;
    int32 TextureSize = 1024;
};

struct FResult
{
    USkeletalMesh* Mesh = nullptr;
    UTexture2D* BaseColor = nullptr;
    UTexture2D* Normal = nullptr;
    UTexture2D* Roughness = nullptr;
    UTexture2D* Metallic = nullptr;
    UTexture2D* OpacityMask = nullptr;
    UMaterial* Material = nullptr;
    int32 SourceTriangles = 0;
    int32 TargetTriangles = 0;
    int32 CoveredTexels = 0;
    int32 MatchedTexels = 0;
};

// Creates unsaved assets under a new /Game package path. The source asset is read-only.
bool GenerateLOD1(USkeletalMesh* Source, const FString& DestinationPackagePath, const FOptions& Options,
    FResult& Out, FText& Error);
}
