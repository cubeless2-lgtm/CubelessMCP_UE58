#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraParameterLibrary.generated.h"

/** Internal Editor-only typed system defaults. Public Python registration is
 * separate. No simulation, generic executor, private store API or game module. */
UCLASS()
class UCubelessNiagaraParameterLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Find-only offset-backed observation; does not poll/flush a compile. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString InspectNiagaraUserParameters(const FString& SystemPath);
    /** Existing finite parameter only. Requests one asynchronous compile after
     * a successful write, including equal writes. Pending compilation defers
     * requested saving with a known partial error; never waits on game thread. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString SetNiagaraUserParameter(const FString& SystemPath, const FString& ParameterName,
        const FString& ValueJson, bool bAllowSourceEdit, bool bSave);
    /** Ordered finite declarations/writes. Earlier successful rows remain on
     * failure; no rollback. At most one compile request and one safe save. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString UpsertNiagaraUserParameters(const FString& SystemPath, const FString& ParametersJson,
        bool bContinueOnError, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
};
