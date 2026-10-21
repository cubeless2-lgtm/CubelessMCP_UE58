#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialCompilationLibrary.generated.h"
/** Hidden Editor-only compiler operation; canonical Python tool/domain owns registration/journaling. */
UCLASS()
class UCubelessMaterialCompilationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore, ScriptName="compile_and_save_material"))
    static FString CompileAndSaveMaterial(const FString& MaterialPath, const FString& GraphType, bool bSave);
};
