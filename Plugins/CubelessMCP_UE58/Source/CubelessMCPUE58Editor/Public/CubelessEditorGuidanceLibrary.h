#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessEditorGuidanceLibrary.generated.h"

/** Transport adapter only. Never reference this editor class from authored content. */
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessEditorGuidanceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor", meta=(AIIgnore="true"))
    static FString EditorGuidanceRequest(const FString& Action, const FString& Arguments);
};
