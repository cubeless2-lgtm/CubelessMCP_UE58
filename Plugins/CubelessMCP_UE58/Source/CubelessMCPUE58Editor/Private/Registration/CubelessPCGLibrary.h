#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGLibrary.generated.h"

/** Hidden editor authoring leaves. Generated assets never call this library. */
UCLASS()
class UCubelessPCGLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString InspectSettingsClass(const FString& ClassPath);

    /** Bounded, find-only JSON observation; does not construct or modify a graph. */
    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString InspectGraphSnapshot(const FString& GraphPath);

    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString InspectSetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName);

    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString ApplySetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& ValueJson);

    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString InspectAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName);

    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString PreviewAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& RequestJson);

    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString InspectConnection(const FString& GraphPath, const FString& FromNode, const FString& FromPin, const FString& ToNode, const FString& ToPin);

    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString NotifyAndRecompile(const FString& GraphPath, bool bAllowDirty);
    /** Exact finite selector native route; no public tool registration. */
    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString PreflightAttributeSelectorAssignment(const FString& ParamsJson);

    /** Exact finite selector native route; no public tool registration. */
    UFUNCTION(BlueprintCallable, meta = (AIIgnore), Category = "Cubeless|Editor|PCG")
    static FString CommitAttributeSelectorAssignment(const FString& ParamsJson);

};
