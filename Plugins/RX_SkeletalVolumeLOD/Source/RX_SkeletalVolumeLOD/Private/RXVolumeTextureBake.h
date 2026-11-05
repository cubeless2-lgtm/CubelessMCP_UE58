#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
struct FMeshDescription;
namespace UE::Geometry
{
class FDynamicMesh3;
}

namespace RXVolumeLOD
{
struct FTextureBake
{
	TArray<FColor> BaseColor;
	TArray<FColor> Normal;
	TArray<FColor> Roughness;
	TArray<FColor> Metallic;
	TArray<FColor> OpacityMask;
	TOptional<float> OpacityMaskClipValue;
	int32 CoveredTexels = 0;
	int32 MatchedTexels = 0;
};

bool BakeTextures(
	const FMeshDescription& SourceDescription,
	const UE::Geometry::FDynamicMesh3& TargetMesh,
	TConstArrayView<UMaterialInterface*> SourceMaterials,
	TConstArrayView<int32> PolygonGroupMaterialIndices,
	int32 TextureSize,
	FTextureBake& Out,
	FText& Error);
}
