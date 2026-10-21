#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMetaSoundLibrary.generated.h"
/** Hidden Editor-only adapter for the original continuous basic MetaSound source. */
UCLASS()
class UCubelessMetaSoundLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Audio", meta=(AIIgnore, ScriptName="create_basic_metasound_source"))
    static FString CreateBasicMetaSoundSource(const FString& Name, const FString& PackagePath, const FString& Oscillator, double Frequency, const FString& OutputFormat);
};
