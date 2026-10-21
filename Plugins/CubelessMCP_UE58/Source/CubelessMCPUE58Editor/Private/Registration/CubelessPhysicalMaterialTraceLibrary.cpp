#include "Registration/CubelessPhysicalMaterialTraceLibrary.h"
#include "Compatibility/UE58/CubelessPhysicalMaterialTraceApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessPhysicalMaterialTraceLibrary::LineTracePhysicalMaterial(const FString& StartJson,
    const FString& EndJson, const FString& TraceChannel, bool bTraceComplex, bool bReturnPhysicalMaterial)
{
    return Cubeless::Operations::EncodeVariableResult(
        Cubeless::UE58::PhysicalMaterialTrace::Trace(StartJson, EndJson, TraceChannel,
            bTraceComplex, bReturnPhysicalMaterial), TEXT("line_trace_physical_material"));
}
