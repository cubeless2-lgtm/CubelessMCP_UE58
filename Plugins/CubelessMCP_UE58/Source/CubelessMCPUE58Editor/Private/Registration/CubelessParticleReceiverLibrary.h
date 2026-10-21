#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessParticleReceiverLibrary.generated.h"

/** Internal Editor evidence adapter; never reference from game assets. */
UCLASS()
class UCubelessParticleReceiverLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Reads only already-loaded exact native particle callback/struct and
     * Array_Length, Array_Get and Add_VectorVector signatures. Includes raw
     * flags and public schema types; no calls, nodes, loading or modifications.
     * This AIIgnore method is internal and is not registered as an MCP tool.
     */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Diagnostics", meta=(AIIgnore))
    static FString InspectNativeSignatures();

    // Exact loaded native ActorComponent receiver operations. Python owns
    // named public tools, validated selectors and durable mutation journals.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString ReadParticleReceiverPlan(const FString& GraphPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString ImplementParticleCallbackInterface(const FString& GraphPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString ImplementBlueprintInterface(const FString& BlueprintPath,const FString& InterfaceClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddParticleDataEventNode(const FString& GraphPath, int32 X, int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddParticleDataArrayLengthNode(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddParticleDataArrayGetCopyNode(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBasicParticleDataBreakNode(const FString& GraphPath, int32 X, int32 Y);
};
