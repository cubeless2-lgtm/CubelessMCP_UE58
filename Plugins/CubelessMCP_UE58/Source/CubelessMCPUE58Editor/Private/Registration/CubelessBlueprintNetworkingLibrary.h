#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintNetworkingLibrary.generated.h"
UCLASS()
class UCubelessBlueprintNetworkingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Private readonly duplicate-source guard, not a public Toolset contract.
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="audit_loaded_blueprint_duplicate_source"))
    static FString AuditLoadedBlueprintDuplicateSource(UObject* SourceObject);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="inspect_blueprint_networking_settings"))
    static FString InspectBlueprintNetworkingSettings(const FString& BlueprintName);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="set_blueprint_networking_settings"))
    static FString SetBlueprintNetworkingSettings(const FString& BlueprintName,const FString& ReplicatesJson,const FString& ReplicateMovementJson,const FString& NetDormancyJson,const FString& AlwaysRelevantJson,const FString& OnlyRelevantToOwnerJson,const FString& NetUpdateFrequencyJson,const FString& MinNetUpdateFrequencyJson,const FString& NetPriorityJson,const FString& NetCullDistanceSquaredJson,bool bDryRun);
    // Private compatibility leaves for original batch parameter objects; not registered public Toolsets.
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="inspect_blueprint_networking_settings_raw_input"))
    static FString InspectBlueprintNetworkingSettingsRawInput(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="set_blueprint_networking_settings_raw_input"))
    static FString SetBlueprintNetworkingSettingsRawInput(const FString& ParamsJson);

};
