#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraEmitterLibrary.generated.h"

/** Internal Editor-only operations; generated Niagara assets never call this class. */
UCLASS()
class UCubelessNiagaraEmitterLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Find-only bounded source/target/private-script observation; no load/compile/save. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString InspectNiagaraEmitterAttachTarget(const FString& TargetSystemPath,
        const FString& SourceEmitterPath, const FString& SourceSystemPath,
        const FString& SourceEmitterIndexJson, const FString& SourceEmitterName, const FString& NewEmitterName);
    /** One public native attach plus explicit parent detachment/private script localization.
     * CopySourceUserParameters=true is incomplete and rejected before mutation. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString DuplicateOrAttachEmitterFromSource(const FString& TargetSystemPath,
        const FString& SourceEmitterPath, const FString& SourceSystemPath,
        const FString& SourceEmitterIndexJson, const FString& SourceEmitterName, const FString& NewEmitterName,
        const FString& EnabledJson, bool bCopySourceUserParameters, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
};
