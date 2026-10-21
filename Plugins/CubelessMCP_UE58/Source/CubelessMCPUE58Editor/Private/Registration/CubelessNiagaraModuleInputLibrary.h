#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraModuleInputLibrary.generated.h"

/** Hidden Editor-only public-API module input leaves. Python owns the durable
 * journal and official Toolset registration; assets never reference this class. */
UCLASS()
class UCubelessNiagaraModuleInputLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString InspectNiagaraModuleInputTarget(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& ModuleIndexJson, const FString& ModuleName, const FString& ModuleNodeGuid, const FString& InputName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString SetNiagaraModuleInputLiteralOverride(const FString& SystemPath, const FString& InputName, const FString& ValueJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString SetNiagaraDataInterfaceModuleInput(const FString& SystemPath, const FString& InputName, const FString& PropertiesJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString SetNiagaraModuleInputLinkedParameter(const FString& SystemPath, const FString& InputName, const FString& LinkedParameterName,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
};
