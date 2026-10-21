#include "CubelessContentReconstructionLibrary.h"
#include "Compatibility/UE58/CubelessContentReconstructionApi.h"

FString UCubelessContentReconstructionLibrary::RecreateContentFolderMCP(const FString& ParamsJson) { return Cubeless::UE58::ContentReconstruction::Recreate(ParamsJson); }
FString UCubelessContentReconstructionLibrary::PostprocessContentFolderMCP(const FString& ParamsJson) { return Cubeless::UE58::ContentReconstruction::Postprocess(ParamsJson); }
FString UCubelessContentReconstructionLibrary::RepairWorldActorInstancesMCP(const FString& ParamsJson) { return Cubeless::UE58::ContentReconstruction::RepairWorld(ParamsJson); }
FString UCubelessContentReconstructionLibrary::RunContentValidationPipelineMCP(const FString& ParamsJson) { return Cubeless::UE58::ContentReconstruction::RunValidationPipeline(ParamsJson); }
