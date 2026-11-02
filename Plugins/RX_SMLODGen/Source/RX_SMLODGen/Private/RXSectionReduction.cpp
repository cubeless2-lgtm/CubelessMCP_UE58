#include "RXSectionReduction.h"

#include "IMeshReductionInterfaces.h"
#include "IMeshReductionManagerModule.h"
#include "Modules/ModuleManager.h"
#include "OverlappingCorners.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "RXSilhouetteReduction.h"

bool RXSectionReduction::Reduce(const FMeshDescription& Source, const FMeshReductionSettings& Settings,
    const TArray<FRXSectionWeight>& Sections, FMeshDescription& Output,
    TArray<FRXSectionReductionResult>& Results, FString& Error)
{
    Error.Reset();
    if (Settings.TerminationCriterion != EStaticMeshReductionTerimationCriterion::Triangles ||
        Settings.MaxNumOfTriangles != MAX_uint32 || Settings.MaxDeviation != 0.0f ||
        !FMath::IsFinite(Settings.PercentTriangles) || Settings.PercentTriangles < 0.0f || Settings.PercentTriangles > 1.0f ||
        !FMath::IsFinite(Settings.WeldingThreshold) || Settings.WeldingThreshold < 0.0f)
    {
        Error = TEXT("Weighted reduction currently requires triangle percentage without a maximum count or deviation limit.");
        return false;
    }
    if (Sections.Num() == 0 || Sections.Num() != Source.PolygonGroups().Num())
    {
        Error = TEXT("Provide exactly one weight for each source polygon group.");
        return false;
    }
    if (Settings.WeldingThreshold > 0 && Sections.ContainsByPredicate([](const FRXSectionWeight& Section){ return Section.bPreserveSilhouette; }))
    { Error = TEXT("외곽 실루엣 보존은 경계 정점을 이동하지 않도록 결합 한계치 0이 필요합니다."); return false; }
    TSet<FPolygonGroupID> Seen;
    for (const FRXSectionWeight& Section : Sections)
    {
        if (!Source.IsPolygonGroupValid(Section.Group) || Seen.Contains(Section.Group) ||
            !FMath::IsFinite(Section.Weight) || Section.Weight < 0.0f || Section.Weight > 1.0f ||
            Source.GetPolygonGroupTriangles(Section.Group).Num() == 0)
        {
            Error = TEXT("A section is missing, duplicated, empty, or has an invalid weight.");
            return false;
        }
        Seen.Add(Section.Group);
    }

    bool bAnyReduction = false;
    for (const FRXSectionWeight& Section : Sections)
    {
        bAnyReduction |= !Section.bExcluded && Section.Weight < 1.0f && Settings.PercentTriangles < 1.0f;
    }
    if (!bAnyReduction)
    {
        TArray<FRXSectionReductionResult> Unchanged;
        for (const FRXSectionWeight& Section : Sections)
        {
            FRXSectionReductionResult Record;
            Record.SourceGroup = Section.Group;
            Record.InputTriangles = Source.GetPolygonGroupTriangles(Section.Group).Num();
            Record.OutputTriangles = Record.InputTriangles;
            Unchanged.Add(Record);
        }
        Output = Source;
        Results = MoveTemp(Unchanged);
        return true;
    }

    FStaticMeshConstAttributes SourceAttributes(Source);
    const TVertexAttributesConstRef<FVector3f> Positions = SourceAttributes.GetVertexPositions();
    if (!Positions.IsValid() || !SourceAttributes.GetVertexInstanceNormals().IsValid() ||
        !SourceAttributes.GetVertexInstanceTangents().IsValid() ||
        !SourceAttributes.GetVertexInstanceBinormalSigns().IsValid() ||
        !SourceAttributes.GetVertexInstanceColors().IsValid() ||
        !SourceAttributes.GetVertexInstanceUVs().IsValid() ||
        !SourceAttributes.GetPolygonGroupMaterialSlotNames().IsValid())
    {
        Error = TEXT("Source is missing required static mesh attributes.");
        return false;
    }
    for (const FVertexID Vertex : Source.Vertices().GetElementIDs())
    {
        if (Positions[Vertex].ContainsNaN())
        {
            Error = TEXT("Source contains a non-finite position.");
            return false;
        }
    }
    IMeshReductionManagerModule& Manager = FModuleManager::LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
    IMeshReduction* Reducer = Manager.GetStaticMeshReductionInterface();
    if (!Reducer || !Reducer->IsSupported())
    {
        Error = TEXT("The configured static mesh reduction interface is unavailable.");
        return false;
    }

    // A uniform weight is exactly the engine-wide reduction case. Reducing
    // the intact mesh preserves the engine's normal section-boundary behavior.
    const float FirstWeight = Sections[0].bExcluded ? 1.0f : Sections[0].Weight;
    const bool bSilhouette = Sections[0].bPreserveSilhouette;
    const bool bUniformWeight = Sections.ContainsByPredicate([FirstWeight, bSilhouette](const FRXSectionWeight& Item)
    {
        return (Item.bExcluded ? 1.0f : Item.Weight) != FirstWeight || Item.bPreserveSilhouette != bSilhouette;
    }) == false;
    if (bUniformWeight)
    {
        FMeshReductionSettings Uniform = Settings;
        Uniform.PercentTriangles = Settings.PercentTriangles + (1.0f - Settings.PercentTriangles) * FirstWeight;
        FOverlappingCorners Corners;
        FStaticMeshOperations::FindOverlappingCorners(Corners, Source, Uniform.WeldingThreshold);
        FMeshDescription Reduced;
        FStaticMeshAttributes(Reduced).Register();
        float MaxDeviation = 0.0f;
        if (bSilhouette)
        { if (!RXSilhouetteReduction::Reduce(Source, Uniform.PercentTriangles, Reduced, Error)) { return false; } }
        else { Reducer->ReduceMeshDescription(Reduced, MaxDeviation, Source, Corners, Uniform); }
        if (Reduced.Triangles().Num() == 0)
        {
            Error = TEXT("The reducer returned an empty mesh.");
            return false;
        }
        FStaticMeshConstAttributes OriginalAttributes(Source);
        FStaticMeshConstAttributes ReducedAttributes(Reduced);
        const auto SourceNames = OriginalAttributes.GetPolygonGroupMaterialSlotNames();
        const auto ReducedNames = ReducedAttributes.GetPolygonGroupMaterialSlotNames();
        if (!SourceNames.IsValid() || !ReducedNames.IsValid())
        {
            Error = TEXT("The reducer returned a mesh without material slot metadata.");
            return false;
        }
        TArray<FRXSectionReductionResult> UniformResults;
        for (const FRXSectionWeight& Section : Sections)
        {
            FRXSectionReductionResult Record;
            Record.SourceGroup = Section.Group;
            Record.TargetKeepFraction = Uniform.PercentTriangles;
            Record.bGlobalTarget = true;
            Record.InputTriangles = Source.GetPolygonGroupTriangles(Section.Group).Num();
            if (Reduced.IsPolygonGroupValid(Section.Group))
            {
                if (SourceNames[Section.Group] != ReducedNames[Section.Group])
                {
                    Error = TEXT("The reducer changed section identities or material slot names.");
                    return false;
                }
                Record.OutputTriangles = Reduced.GetPolygonGroupTriangles(Section.Group).Num();
            }
            UniformResults.Add(Record);
        }
        for (FPolygonGroupID Group : Reduced.PolygonGroups().GetElementIDs())
        {
            if (!Seen.Contains(Group))
            {
                Error = TEXT("The reducer returned an unknown section identity.");
                return false;
            }
        }
        Output = MoveTemp(Reduced);
        Results = MoveTemp(UniformResults);
        return true;
    }
    if (Settings.WeldingThreshold > 0.0f)
    {
        Error = TEXT("Different section weights require a welding threshold of zero until welded boundaries are supported.");
        return false;
    }

    // Splitting and reducing touching groups independently can produce cracks.
    // Reject identical positions across groups; this is deliberately conservative.
    TMap<FVector3f, FPolygonGroupID> PositionGroups;
    for (const FTriangleID Triangle : Source.Triangles().GetElementIDs())
    {
        const FPolygonGroupID Group = Source.GetTrianglePolygonGroup(Triangle);
        for (const FVertexID Vertex : Source.GetTriangleVertices(Triangle))
        {
            const FVector3f Position = Positions[Vertex];
            if (Position.ContainsNaN())
            {
                Error = TEXT("Source contains a non-finite position.");
                return false;
            }
            if (const FPolygonGroupID* Existing = PositionGroups.Find(Position))
            {
                if (*Existing != Group)
                {
                    Error = TEXT("Sections touch at a shared position; boundary-safe reduction is not implemented yet.");
                    return false;
                }
            }
            else
            {
                PositionGroups.Add(Position, Group);
            }
        }
    }

    FMeshDescription Combined;
    FStaticMeshAttributes(Combined).Register();
    TArray<FRXSectionReductionResult> LocalResults;
    for (const FRXSectionWeight& Section : Sections)
    {
        FMeshDescription Isolated = Source;
        TArray<FPolygonID> Remove;
        for (const FPolygonID Polygon : Isolated.Polygons().GetElementIDs())
        {
            if (Isolated.GetPolygonPolygonGroup(Polygon) != Section.Group)
            {
                Remove.Add(Polygon);
            }
        }
        Isolated.DeletePolygons(Remove);
        FRXSectionReductionResult Record;
        Record.SourceGroup = Section.Group;
        Record.InputTriangles = Isolated.Triangles().Num();
        const float Weight = Section.bExcluded ? 1.0f : Section.Weight;
        Record.TargetKeepFraction = Settings.PercentTriangles + (1.0f - Settings.PercentTriangles) * Weight;
        FMeshDescription Reduced;
        if (Weight == 1.0f || Record.TargetKeepFraction >= 1.0f)
        {
            Reduced = MoveTemp(Isolated);
        }
        else
        {
            FMeshReductionSettings PerSection = Settings;
            PerSection.PercentTriangles = Record.TargetKeepFraction;
            // The engine reducer writes standard static-mesh attributes into the
            // destination; an unregistered MeshDescription can crash in that code.
            FStaticMeshAttributes(Reduced).Register();
            FOverlappingCorners Corners;
            FStaticMeshOperations::FindOverlappingCorners(Corners, Isolated, PerSection.WeldingThreshold);
            float MaxDeviation = 0.0f;
            if (Section.bPreserveSilhouette)
            { if (!RXSilhouetteReduction::Reduce(Isolated, PerSection.PercentTriangles, Reduced, Error)) { return false; } }
            else { Reducer->ReduceMeshDescription(Reduced, MaxDeviation, Isolated, Corners, PerSection); }
        }
        Record.OutputTriangles = Reduced.Triangles().Num();
        if (Record.OutputTriangles <= 0 || Reduced.PolygonGroups().Num() != 1)
        {
            Error = TEXT("The reducer returned an empty or ambiguous section.");
            return false;
        }
        FStaticMeshOperations::FAppendSettings Append;
        Append.PolygonGroupsDelegate.BindLambda([](const FMeshDescription& In, FMeshDescription& Out, PolygonGroupMap& Mapping)
        {
            FStaticMeshConstAttributes InAttrs(In);
            FStaticMeshAttributes OutAttrs(Out);
            const auto InNames = InAttrs.GetPolygonGroupMaterialSlotNames();
            auto OutNames = OutAttrs.GetPolygonGroupMaterialSlotNames();
            for (const FPolygonGroupID Group : In.PolygonGroups().GetElementIDs())
            {
                const FPolygonGroupID NewGroup = Out.CreatePolygonGroup();
                OutNames[NewGroup] = InNames[Group];
                Mapping.Add(Group, NewGroup);
            }
        });
        FStaticMeshOperations::AppendMeshDescription(Reduced, Combined, Append);
        LocalResults.Add(Record);
    }
    Output = MoveTemp(Combined);
    Results = MoveTemp(LocalResults);
    return true;
}
