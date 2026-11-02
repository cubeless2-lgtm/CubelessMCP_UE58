#include "RXLODStats.h"
#include "StaticMeshResources.h"

FRXSectionStats RXLODStats::Section(const FStaticMeshLODResources& LOD, int32 SectionIndex)
{
    FRXSectionStats Result;
    if (!LOD.Sections.IsValidIndex(SectionIndex)) { return Result; }
    const auto& Section = LOD.Sections[SectionIndex];
    Result.Triangles = Section.NumTriangles;
    const auto Indices = LOD.IndexBuffer.GetArrayView();
    TSet<uint32> Referenced;
    const uint64 End = FMath::Min<uint64>(uint64(Section.FirstIndex) + uint64(Section.NumTriangles) * 3, Indices.Num());
    for (uint64 Index = Section.FirstIndex; Index < End; ++Index) { Referenced.Add(Indices[int32(Index)]); }
    Result.Vertices = Referenced.Num();
    return Result;
}
