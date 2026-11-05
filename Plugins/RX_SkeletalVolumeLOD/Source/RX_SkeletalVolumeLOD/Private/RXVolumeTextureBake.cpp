#include "RXVolumeTextureBake.h"

#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshToMeshDescription.h"
#include "IMaterialBakingModule.h"
#include "MaterialBakingStructures.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Parameterization/DynamicMeshUVEditor.h"
#include "ParameterizationOps/ParameterizeMeshOp.h"
#include "Sampling/MeshMapBaker.h"
#include "Sampling/MeshNormalMapEvaluator.h"
#include "Sampling/MeshResampleImageEvaluator.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

#include <atomic>

#define LOCTEXT_NAMESPACE "RXVolumeTextureBake"

DEFINE_LOG_CATEGORY_STATIC(LogRXVolumeTextureBake, Log, All);

namespace RXVolumeLOD
{
using namespace UE::Geometry;

namespace
{
constexpr EMaterialProperty ColorAndScalarProperties[] = {MP_BaseColor, MP_Roughness, MP_Metallic};
const FColor EmptySourceColor(255, 0, 255, 0);

class FVolumeBakeDetailSampler final : public FMeshBakerDynamicMeshSampler
{
public:
	FVolumeBakeDetailSampler(const FDynamicMesh3& InMesh, const FDynamicMeshAABBTree3& Spatial)
		: FMeshBakerDynamicMeshSampler(&InMesh, &Spatial)
		, Mesh(InMesh)
		, MaxDistanceSquared(FMath::Square(Spatial.GetBoundingBox().MaxDim() * 0.05))
	{
	}

	bool IsValidCorrespondence(const FMeshMapEvaluator::FCorrespondenceSample& Sample) const override
	{
		if (!FMeshBakerDynamicMeshSampler::IsValidCorrespondence(Sample))
		{
			return false;
		}
		const FVector3d& Weights = Sample.DetailBaryCoords;
		const FVector3d SourcePoint = Mesh.GetTriBaryPoint(Sample.DetailTriID, Weights.X, Weights.Y, Weights.Z);
		return FVector3d::DistSquared(SourcePoint, Sample.BaseSample.SurfacePoint) <= MaxDistanceSquared;
	}

private:
	const FDynamicMesh3& Mesh;
	const double MaxDistanceSquared;
};

FLinearColor DecodeChannel(const FColor& Color, bool bIsLinear)
{
	return bIsLinear ? Color.ReinterpretAsLinear() : FLinearColor::FromSRGBColor(Color);
}

bool HasBakeAttributes(const FDynamicMesh3& Mesh, bool bRequireNormals)
{
	const FDynamicMeshAttributeSet* Attributes = Mesh.Attributes();
	const FDynamicMeshUVOverlay* UVs = Attributes ? Attributes->PrimaryUV() : nullptr;
	const FDynamicMeshNormalOverlay* Normals = Attributes ? Attributes->PrimaryNormals() : nullptr;
	if (!UVs || (bRequireNormals && !Normals) || Mesh.TriangleCount() == 0)
	{
		return false;
	}
	for (const int32 TriangleID : Mesh.TriangleIndicesItr())
	{
		if (!UVs->IsSetTriangle(TriangleID) || (bRequireNormals && !Normals->IsSetTriangle(TriangleID)))
		{
			return false;
		}
	}
	return true;
}

bool IsUsableDirection(const FVector3f& Vector)
{
	return !Vector.ContainsNaN() && Vector.SizeSquared() > 0.5f;
}

bool HasNormalBasis(const FDynamicMesh3& Mesh)
{
	const FDynamicMeshAttributeSet* Attributes = Mesh.Attributes();
	const FDynamicMeshNormalOverlay* Normals = Attributes ? Attributes->PrimaryNormals() : nullptr;
	const FDynamicMeshNormalOverlay* Tangents = Attributes ? Attributes->PrimaryTangents() : nullptr;
	const FDynamicMeshNormalOverlay* Bitangents = Attributes ? Attributes->PrimaryBiTangents() : nullptr;
	if (!Normals || !Tangents || !Bitangents)
	{
		return false;
	}
	for (const int32 TriangleID : Mesh.TriangleIndicesItr())
	{
		if (!Normals->IsSetTriangle(TriangleID) || !Tangents->IsSetTriangle(TriangleID) || !Bitangents->IsSetTriangle(TriangleID))
		{
			return false;
		}
		const FIndex3i NormalIDs = Normals->GetTriangle(TriangleID);
		const FIndex3i TangentIDs = Tangents->GetTriangle(TriangleID);
		const FIndex3i BitangentIDs = Bitangents->GetTriangle(TriangleID);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			if (!IsUsableDirection(Normals->GetElement(NormalIDs[Corner])) ||
				!IsUsableDirection(Tangents->GetElement(TangentIDs[Corner])) ||
				!IsUsableDirection(Bitangents->GetElement(BitangentIDs[Corner])))
			{
				return false;
			}
		}
	}
	return true;
}

bool RepairSourceBasis(FMeshDescription& RasterDescription, FDynamicMesh3& Original, FString& Reason)
{
	// Mikk compacts its input when necessary. The raster conversion must keep
	// polygon group IDs and corner order for the material and UV correspondence.
	if (RasterDescription.NeedsCompact())
	{
		Reason = TEXT("the raster description is not compact");
		return false;
	}

	FMeshDescription Repaired = RasterDescription;
	FStaticMeshAttributes Attributes(Repaired);
	const auto OriginalAttributes = FStaticMeshConstAttributes(RasterDescription);
	auto Normals = Attributes.GetVertexInstanceNormals();
	auto Tangents = Attributes.GetVertexInstanceTangents();
	auto Signs = Attributes.GetVertexInstanceBinormalSigns();
	const auto UVs = Attributes.GetVertexInstanceUVs();
	if (UVs.GetNumChannels() == 0)
	{
		Reason = TEXT("the original mesh has no UV0");
		return false;
	}

	TSet<FVertexInstanceID> MissingFrames;
	bool bRepairNormals = false;
	for (const FVertexInstanceID Corner : Repaired.VertexInstances().GetElementIDs())
	{
		const bool bValidNormal = IsUsableDirection(Normals[Corner]);
		if (!bValidNormal)
		{
			Normals[Corner] = FVector3f::ZeroVector;
			bRepairNormals = true;
		}
		if (!bValidNormal || !IsUsableDirection(Tangents[Corner]) ||
			!FMath::IsNearlyEqual(FMath::Abs(Signs[Corner]), 1.0f) ||
			!IsUsableDirection(FVector3f::CrossProduct(Normals[Corner], Tangents[Corner])))
		{
			MissingFrames.Add(Corner);
			Tangents[Corner] = FVector3f::ZeroVector;
			Signs[Corner] = 0.0f;
		}
	}
	for (const FTriangleID Triangle : Repaired.Triangles().GetElementIDs())
	{
		const auto Corners = Repaired.GetTriangleVertexInstances(Triangle);
		if (!MissingFrames.Contains(Corners[0]) && !MissingFrames.Contains(Corners[1]) && !MissingFrames.Contains(Corners[2]))
		{
			continue;
		}
		const FVector2d A(UVs.Get(Corners[0], 0));
		const FVector2d AB = FVector2d(UVs.Get(Corners[1], 0)) - A;
		const FVector2d AC = FVector2d(UVs.Get(Corners[2], 0)) - A;
		const double Area = AB.X * AC.Y - AB.Y * AC.X;
		if (!FMath::IsFinite(Area) || FMath::Abs(Area) <= UE_DOUBLE_SMALL_NUMBER)
		{
			Reason = TEXT("a triangle that needs a tangent has degenerate original UV0");
			return false;
		}
	}

	if (bRepairNormals)
	{
		FStaticMeshOperations::ComputeTriangleTangentsAndNormals(Repaired);
		FStaticMeshOperations::ComputeTangentsAndNormals(Repaired,
			EComputeNTBsFlags::Tangents | EComputeNTBsFlags::UseMikkTSpace | EComputeNTBsFlags::IgnoreDegenerateTriangles);
	}
	else
	{
		FStaticMeshOperations::ComputeMikktTangents(Repaired, true);
	}

	const auto AuthoredTangents = OriginalAttributes.GetVertexInstanceTangents();
	const auto AuthoredSigns = OriginalAttributes.GetVertexInstanceBinormalSigns();
	for (const FVertexInstanceID Corner : Repaired.VertexInstances().GetElementIDs())
	{
		if (!MissingFrames.Contains(Corner))
		{
			Tangents[Corner] = AuthoredTangents[Corner];
			Signs[Corner] = AuthoredSigns[Corner];
		}
	}

	FDynamicMesh3 RepairedMesh;
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(&Repaired, RepairedMesh, true);
	if (!HasNormalBasis(RepairedMesh) || RepairedMesh.TriangleCount() != Original.TriangleCount())
	{
		Reason = TEXT("MikkTSpace could not establish a valid basis on every original triangle");
		return false;
	}
	RasterDescription = MoveTemp(Repaired);
	Original = MoveTemp(RepairedMesh);
	UE_LOG(LogRXVolumeTextureBake, Display, TEXT("Reconstructed %d LOD0 tangent frames on original UV0 for both source rasterization and projection."), MissingFrames.Num());
	return true;
}

bool BuildIntermediateAtlas(
	const FMeshDescription& SourceDescription,
	int32 AtlasSize,
	FDynamicMesh3& SourceMesh,
	FMeshDescription& RasterDescription,
	TArray<FVector2D>& RasterUVs,
	FText& Error)
{
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> Original = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>();
	{
		// Skeletal rendering uses vertex-instance UVs. Remove only the shared UV
		// representation from this copy so the converter cannot prefer stale data.
		FMeshDescription RasterSource = SourceDescription;
		RasterSource.SetNumUVChannels(0);
		FMeshDescriptionToDynamicMesh ReadSource;
		ReadSource.Convert(&RasterSource, *Original, true);
	}
	if (Original->TriangleCount() == 0 || !Original->HasAttributes() || !Original->Attributes()->HasMaterialID()
		|| Original->Attributes()->NumUVLayers() == 0)
	{
		Error = LOCTEXT("InvalidSource", "LOD0 needs triangles, section material IDs and UV0 for material baking.");
		return false;
	}
	if (Original->TriangleCount() != SourceDescription.Triangles().Num())
	{
		Error = LOCTEXT("UnmappedSource", "Some LOD0 triangles cannot be represented for surface baking.");
		return false;
	}

	// MaterialBaking receives one instance per corner and uses CustomTextureCoordinates for
	// raster positions while the material still reads every original texture-coordinate channel.
	FStaticMeshAttributes(RasterDescription).Register();
	FDynamicMeshToMeshDescription WriteRasterMesh;
	WriteRasterMesh.Convert(Original.Get(), RasterDescription, true);
	if (!HasNormalBasis(*Original))
	{
		FString Reason;
		if (!RepairSourceBasis(RasterDescription, *Original, Reason))
		{
			UE_LOG(LogRXVolumeTextureBake, Warning, TEXT("LOD0 normal bake basis unavailable: %s."), *Reason);
		}
	}

	FParameterizeMeshOp Parameterize;
	Parameterize.InputMesh = Original;
	Parameterize.Method = EParamOpBackend::XAtlas;
	Parameterize.CalculateResult(nullptr);
	if (!Parameterize.GetResultInfo().HasResult())
	{
		Error = LOCTEXT("SourceAtlasFailed", "Could not generate a unique intermediate UV layout for LOD0.");
		return false;
	}
	TUniquePtr<FDynamicMesh3> Layout = Parameterize.ExtractResult();
	FDynamicMeshUVEditor LayoutEditor(Layout.Get(), 0, false);
	if (!HasBakeAttributes(*Layout, false) || !LayoutEditor.QuickPack(AtlasSize, 4.0f))
	{
		Error = LOCTEXT("SourceAtlasPackFailed", "Could not pack all LOD0 UV charts for material baking.");
		return false;
	}

	RasterUVs.Reserve(3 * Layout->TriangleCount());
	const FDynamicMeshUVOverlay* UVs = Layout->Attributes()->PrimaryUV();
	for (const int32 TriangleID : Layout->TriangleIndicesItr())
	{
		const FIndex3i CornerUVs = UVs->GetTriangle(TriangleID);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			RasterUVs.Add(FVector2D(UVs->GetElement(CornerUVs[Corner])));
		}
	}
	if (RasterUVs.Num() != RasterDescription.VertexInstances().GetArraySize()
		|| Layout->TriangleCount() != RasterDescription.Triangles().Num())
	{
		Error = LOCTEXT("SourceRasterMismatch", "LOD0 rasterization corners do not match the intermediate UV layout.");
		return false;
	}
	SourceMesh = MoveTemp(*Layout);
	return true;
}

bool BakeSourceMaterials(
	const FMeshDescription& RasterDescription,
	const TArray<FVector2D>& RasterUVs,
	TConstArrayView<UMaterialInterface*> SourceMaterials,
	TConstArrayView<int32> PolygonGroupMaterialIndices,
	int32 AtlasSize,
	TArray<EMaterialProperty, TInlineAllocator<5>>& Properties,
	TBitArray<>& OpaqueGroups,
	TOptional<float>& OpacityMaskClipValue,
	FBakeOutput& Out,
	FText& Error)
{
	TSet<int32> UsedGroups;
	for (const FTriangleID TriangleID : RasterDescription.Triangles().GetElementIDs())
	{
		UsedGroups.Add(RasterDescription.GetTrianglePolygonGroup(TriangleID).GetValue());
	}
	TArray<int32> OrderedGroups = UsedGroups.Array();
	OrderedGroups.Sort();
	TArray<FMeshData> MeshSettings;
	TArray<FMaterialData> MaterialSettings;
	TMap<int32, int32> MaterialToSettings;
	for (const int32 Group : OrderedGroups)
	{
		const int32 MaterialIndex = PolygonGroupMaterialIndices.IsValidIndex(Group) ? PolygonGroupMaterialIndices[Group] : INDEX_NONE;
		UMaterialInterface* Material = SourceMaterials.IsValidIndex(MaterialIndex) ? SourceMaterials[MaterialIndex] : nullptr;
		if (!IsValid(Material) || (Material->GetBlendMode() != BLEND_Opaque && Material->GetBlendMode() != BLEND_Masked))
		{
			Error = FText::Format(
				LOCTEXT("UnsupportedMaterial", "LOD0 section {0} requires an assigned opaque or masked material."),
				FText::AsNumber(Group));
			return false;
		}
		if (Material->GetBlendMode() == BLEND_Masked)
		{
			const float ClipValue = Material->GetOpacityMaskClipValue();
			if (OpacityMaskClipValue.IsSet() && !FMath::IsNearlyEqual(OpacityMaskClipValue.GetValue(), ClipValue))
			{
				Error = LOCTEXT("DifferentMaskClips", "The source masked materials require the same opacity mask clip value to share one LOD1 material.");
				return false;
			}
			OpacityMaskClipValue = ClipValue;
		}
		else
		{
			OpaqueGroups[Group] = true;
		}

		if (const int32* SettingsIndex = MaterialToSettings.Find(MaterialIndex))
		{
			MeshSettings[*SettingsIndex].MaterialIndices.Add(Group);
			continue;
		}

		MaterialToSettings.Add(MaterialIndex, MeshSettings.Num());
		FMeshData& MeshData = MeshSettings.AddDefaulted_GetRef();
		MeshData.MeshDescription = &RasterDescription;
		MeshData.CustomTextureCoordinates = RasterUVs;
		MeshData.MaterialIndices.Add(Group);

		FMaterialData& MaterialData = MaterialSettings.AddDefaulted_GetRef();
		MaterialData.Material = Material;
		MaterialData.BackgroundColor = EmptySourceColor;
		MaterialData.bTangentSpaceNormal = true;
		// Rasterize the raw mask without clipping the intermediate atlas.
		MaterialData.BlendMode = BLEND_Opaque;
	}
	if (OpacityMaskClipValue.IsSet())
	{
		Properties.Add(MP_OpacityMask);
	}
	TArray<FMeshData*> MeshSettingsPtrs;
	TArray<FMaterialData*> MaterialSettingsPtrs;
	for (int32 Index = 0; Index < MeshSettings.Num(); ++Index)
	{
		for (const EMaterialProperty Property : Properties)
		{
			MaterialSettings[Index].PropertySizes.Add(Property, FIntPoint(AtlasSize, AtlasSize));
		}
		MeshSettingsPtrs.Add(&MeshSettings[Index]);
		MaterialSettingsPtrs.Add(&MaterialSettings[Index]);
	}
	IMaterialBakingModule* Module = FModuleManager::LoadModulePtr<IMaterialBakingModule>(TEXT("MaterialBaking"));
	if (!Module)
	{
		Error = LOCTEXT("MissingModule", "The Unreal MaterialBaking module is unavailable.");
		return false;
	}
	Module->BakeMaterials(MaterialSettingsPtrs, MeshSettingsPtrs, Out);
	for (const EMaterialProperty Property : Properties)
	{
		const FIntPoint* Size = Out.PropertySizes.Find(Property);
		const TArray<FColor>* Pixels = Out.PropertyData.Find(Property);
		if (!Size || Size->X < 1 || Size->Y < 1 || !Pixels || Pixels->Num() != Size->X * Size->Y
			|| !Out.PropertyIsLinearColor.Contains(Property))
		{
			Error = LOCTEXT("MaterialBakeFailed", "Unreal MaterialBaking did not return complete material channel data.");
			return false;
		}
		// The material baker shrinks an entirely untouched render target to its clear pixel.
		if (Pixels->Num() == 1 && (*Pixels)[0] == EmptySourceColor)
		{
			Error = LOCTEXT("EmptySourceBake", "An intermediate material channel did not rasterize any LOD0 pixels.");
			return false;
		}
	}
	return true;
}

TSharedPtr<TImageBuilder<FVector4f>, ESPMode::ThreadSafe> MakeLinearImage(const FBakeOutput& SourceBake, EMaterialProperty Property)
{
	const FIntPoint Size = SourceBake.PropertySizes.FindChecked(Property);
	const bool bIsLinear = SourceBake.PropertyIsLinearColor.FindChecked(Property);
	const TArray<FColor>& Colors = SourceBake.PropertyData.FindChecked(Property);
	TSharedPtr<TImageBuilder<FVector4f>, ESPMode::ThreadSafe> Image = MakeShared<TImageBuilder<FVector4f>, ESPMode::ThreadSafe>();
	Image->SetDimensions(FImageDimensions(Size.X, Size.Y));
	for (int32 Index = 0; Index < Colors.Num(); ++Index)
	{
		const FLinearColor Linear = DecodeChannel(Colors[Index], bIsLinear);
		Image->SetPixel(Index, FVector4f(Linear.R, Linear.G, Linear.B, 1.0f));
	}
	return Image;
}

bool ReadBakedPixels(FMeshMapBaker& Baker, int32 EvaluatorIndex, int32 PixelCount, bool bSRGB, TArray<FColor>& Out)
{
	const auto Results = Baker.GetBakeResults(EvaluatorIndex);
	if (Results.Num() != 1 || !Results[0] || Results[0]->GetDimensions().Num() != PixelCount)
	{
		return false;
	}
	Out.SetNumUninitialized(PixelCount);
	for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
	{
		const FVector4f Pixel = Results[0]->GetPixel(PixelIndex);
		Out[PixelIndex] = FLinearColor(Pixel.X, Pixel.Y, Pixel.Z, 1.0f).ToFColor(bSRGB);
	}
	return true;
}
}

bool BakeTextures(
	const FMeshDescription& SourceDescription,
	const FDynamicMesh3& TargetMesh,
	TConstArrayView<UMaterialInterface*> SourceMaterials,
	TConstArrayView<int32> PolygonGroupMaterialIndices,
	int32 TextureSize,
	FTextureBake& Out,
	FText& Error)
{
	Out = FTextureBake();
	Error = FText();
	if (!IsInGameThread() || !FApp::CanEverRender())
	{
		Error = LOCTEXT("RequiresRendering", "Texture baking must run on the editor game thread with rendering enabled.");
		return false;
	}
	if (TextureSize < 32 || TextureSize > 4096 || !HasBakeAttributes(TargetMesh, true))
	{
		Error = LOCTEXT("InvalidTarget", "Texture baking requires a 32 to 4096 texel atlas and UV0 and normals on every target triangle.");
		return false;
	}

	const int32 IntermediateSize = FMath::Max(1024, TextureSize);
	FDynamicMesh3 SourceMesh;
	FMeshDescription RasterDescription;
	TArray<FVector2D> RasterUVs;
	if (!BuildIntermediateAtlas(SourceDescription, IntermediateSize, SourceMesh, RasterDescription, RasterUVs, Error))
	{
		return false;
	}

	// The target basis was authored for the skeletal LOD. Use those exact tangents,
	// rather than computing a second basis after the UV unwrap.
	const bool bSourceNormalBasis = HasNormalBasis(SourceMesh);
	const bool bTargetNormalBasis = HasNormalBasis(TargetMesh);
	const bool bBakeNormal = bSourceNormalBasis && bTargetNormalBasis;
	if (!bBakeNormal)
	{
		UE_LOG(LogRXVolumeTextureBake, Warning, TEXT("Skipping Normal bake. Source LOD0 tangent basis valid=%d; target LOD1 tangent basis valid=%d."),
			bSourceNormalBasis, bTargetNormalBasis);
	}
	TArray<EMaterialProperty, TInlineAllocator<5>> Properties;
	Properties.Append(ColorAndScalarProperties, UE_ARRAY_COUNT(ColorAndScalarProperties));
	if (bBakeNormal)
	{
		Properties.Add(MP_Normal);
	}
	FBakeOutput Intermediate;
	TBitArray<> OpaqueGroups(false, PolygonGroupMaterialIndices.Num());
	if (!BakeSourceMaterials(RasterDescription, RasterUVs, SourceMaterials, PolygonGroupMaterialIndices,
		IntermediateSize, Properties, OpaqueGroups, Out.OpacityMaskClipValue, Intermediate, Error))
	{
		return false;
	}

	const FDynamicMeshAABBTree3 SourceSpatial(&SourceMesh);
	FVolumeBakeDetailSampler Sampler(SourceMesh, SourceSpatial);
	FMeshMapBaker Baker;
	Baker.SetDetailSampler(&Sampler);
	Baker.SetTargetMesh(&TargetMesh);
	Baker.SetTargetMeshUVLayer(0);
	Baker.SetDimensions(FImageDimensions(TextureSize, TextureSize));
	Baker.SetSamplesPerPixel(1);
	Baker.SetGutterSize(4);
	Baker.SetCorrespondenceStrategy(FMeshBaseBaker::ECorrespondenceStrategy::NearestPoint);

	// A single source atlas contains all materials, so every section samples the same
	// image for a given property. Keep each image resident through the parallel bake.
	TArray<int32, TInlineAllocator<4>> EvaluatorIndices;
	for (const EMaterialProperty Property : Properties)
	{
		if (Property == MP_Normal)
		{
			continue;
		}
		const auto Image = MakeLinearImage(Intermediate, Property);
		TSharedPtr<FMeshMultiResampleImageEvaluator> Evaluator = MakeShared<FMeshMultiResampleImageEvaluator>();
		const int32 MaterialSlots = PolygonGroupMaterialIndices.Num();
		Evaluator->MultiTextures.Init(Image, MaterialSlots);
		if (Property == MP_OpacityMask && OpaqueGroups.Contains(true))
		{
			const auto White = MakeShared<TImageBuilder<FVector4f>, ESPMode::ThreadSafe>();
			White->SetDimensions(FImageDimensions(1, 1));
			White->SetPixel(0, FVector4f(1, 1, 1, 1));
			for (TConstSetBitIterator<> Group(OpaqueGroups); Group; ++Group)
			{
				Evaluator->MultiTextures[Group.GetIndex()] = White;
			}
		}
		Evaluator->DetailUVLayer = 0;
		EvaluatorIndices.Add(Baker.AddEvaluator(Evaluator));
	}
	FMeshTangentsd SourceTangents(&SourceMesh);
	TSharedPtr<TImageBuilder<FVector4f>, ESPMode::ThreadSafe> NormalImage;
	int32 NormalEvaluatorIndex = INDEX_NONE;
	if (bBakeNormal)
	{
		auto TargetTangents = MakeShared<FMeshTangentsd, ESPMode::ThreadSafe>(&TargetMesh);
		if (!SourceTangents.CopyTriVertexTangents(SourceMesh) || !TargetTangents->CopyTriVertexTangents(TargetMesh))
		{
			Error = LOCTEXT("NormalBasisFailed", "Could not read the authored tangent bases for the normal bake.");
			return false;
		}
		NormalImage = MakeLinearImage(Intermediate, MP_Normal);
		Sampler.SetTangents(&SourceMesh, &SourceTangents);
		Sampler.SetNormalTextureMap(&SourceMesh, {NormalImage.Get(), 0, IMeshBakerDetailSampler::EBakeDetailNormalSpace::Tangent});
		Baker.SetTargetMeshTangents(TargetTangents);
		NormalEvaluatorIndex = Baker.AddEvaluator(MakeShared<FMeshNormalMapEvaluator>());
	}
	std::atomic<int32> CoveredTexels = 0;
	std::atomic<int32> MatchedTexels = 0;
	Baker.InteriorSampleCallback = [&CoveredTexels, &MatchedTexels](
		bool bValid, const FMeshMapEvaluator::FCorrespondenceSample&, const FVector2d&, const FVector2i&)
	{
		CoveredTexels.fetch_add(1, std::memory_order_relaxed);
		if (bValid)
		{
			MatchedTexels.fetch_add(1, std::memory_order_relaxed);
		}
	};
	Baker.Bake();
	Out.CoveredTexels = CoveredTexels.load(std::memory_order_relaxed);
	Out.MatchedTexels = MatchedTexels.load(std::memory_order_relaxed);
	if (Out.MatchedTexels == 0)
	{
		Error = LOCTEXT("NoSamples", "No covered proxy texels matched a nearby original surface.");
		return false;
	}

	const int32 PixelCount = TextureSize * TextureSize;
	TArray<FColor>* const Outputs[] = {&Out.BaseColor, &Out.Roughness, &Out.Metallic, &Out.OpacityMask};
	for (int32 Index = 0; Index < EvaluatorIndices.Num(); ++Index)
	{
		if (!ReadBakedPixels(Baker, EvaluatorIndices[Index], PixelCount, Index == 0, *Outputs[Index]))
		{
			Error = LOCTEXT("OutputBakeFailed", "The surface baker did not return a complete proxy texture.");
			return false;
		}
	}
	if (bBakeNormal && !ReadBakedPixels(Baker, NormalEvaluatorIndex, PixelCount, false, Out.Normal))
	{
		Error = LOCTEXT("NormalOutputFailed", "The surface baker did not return a complete tangent-space normal texture.");
		return false;
	}
	return true;
}
}

#undef LOCTEXT_NAMESPACE
