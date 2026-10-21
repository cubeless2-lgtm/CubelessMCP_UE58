#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPIEInputLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPIEInputLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore="true"))
 static FString InjectPIEInputKey(const FString& ParamsJson);
};
