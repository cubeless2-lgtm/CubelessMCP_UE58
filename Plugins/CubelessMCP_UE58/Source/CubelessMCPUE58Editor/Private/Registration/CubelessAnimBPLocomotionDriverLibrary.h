#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimBPLocomotionDriverLibrary.generated.h"
UCLASS(meta=(AIIgnore))
class UCubelessAnimBPLocomotionDriverLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_anim_bp_event_graph_locomotion_driver"))
    static FString EnsureAnimBPEventGraphLocomotionDriver(FString SourceAnimBPPath,FString TargetAnimBPPath,FString TargetRoot,FString VariantName,FString SpeedVariable,bool bCreateVariableIfMissing,bool bIncludeDirection,FString DirectionVariable,bool bCreateDirectionVariableIfMissing,bool bIncludeIsInAir,FString IsInAirVariable,bool bCreateIsInAirVariableIfMissing,bool bIncludeAcceleration,FString AccelerationVariable,bool bCreateAccelerationVariableIfMissing,bool bIncludeIsAccelerating,FString IsAcceleratingVariable,bool bCreateIsAcceleratingVariableIfMissing,double IsAcceleratingThreshold,bool bGuardPawnOwner,bool bReplaceExistingLinks,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,FString PrivateContextJson);
};
