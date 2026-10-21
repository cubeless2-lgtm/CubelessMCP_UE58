#include "Registration/CubelessEngineFunctionInspectionLibrary.h"
#include "Compatibility/UE58/CubelessEngineFunctionInspectionApi.h"
#include "Operations/CubelessFunctionOperations.h"

FString UCubelessEngineFunctionInspectionLibrary::InspectCaptureSignatures()
{
    return Cubeless::Operations::EncodeFunctionResult(
        Cubeless::UE58::EngineFunctionInspection::InspectCaptureSignatures(), TEXT("inspect_capture_signatures"));
}
