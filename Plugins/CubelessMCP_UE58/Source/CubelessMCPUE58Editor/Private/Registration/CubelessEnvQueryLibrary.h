#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessEnvQueryLibrary.generated.h"

/** Hidden Editor bridge; the public AI operations retain their existing journal. */
UCLASS()
class UCubelessEnvQueryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|EQS", meta=(AIIgnore, ScriptName="create_ai_eqs_query"))
    static FString CreateAIEQSQuery(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset);
    UFUNCTION(BlueprintCallable, Category="Cubeless|EQS", meta=(AIIgnore))
    static FString InspectAIAssetStructure(const FString& AssetPath);
};
