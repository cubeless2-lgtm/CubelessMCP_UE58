#include "CubelessBlueprintNiagaraTemplateSwapLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintNiagaraTemplateSwapApi.h"
FString UCubelessBlueprintNiagaraTemplateSwapLibrary::InspectBlueprintNiagaraComponentTemplate(const FString& ParamsJson) { return Cubeless::UE58::BlueprintNiagaraTemplateSwap::InspectTemplate(ParamsJson); }
FString UCubelessBlueprintNiagaraTemplateSwapLibrary::DryRunBlueprintNiagaraComponentTemplateAssetSwap(const FString& ParamsJson) { return Cubeless::UE58::BlueprintNiagaraTemplateSwap::DryRunSwap(ParamsJson); }
FString UCubelessBlueprintNiagaraTemplateSwapLibrary::ExecuteBlueprintNiagaraComponentTemplateAssetSwapCanary(const FString& ParamsJson) { return Cubeless::UE58::BlueprintNiagaraTemplateSwap::ExecuteCanary(ParamsJson); }
FString UCubelessBlueprintNiagaraTemplateSwapLibrary::ExecuteBlueprintNiagaraComponentTemplateAssetSwapDurable(const FString& ParamsJson) { return Cubeless::UE58::BlueprintNiagaraTemplateSwap::ExecuteDurable(ParamsJson); }
