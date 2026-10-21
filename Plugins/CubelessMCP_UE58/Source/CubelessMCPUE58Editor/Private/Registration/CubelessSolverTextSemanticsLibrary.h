#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessSolverTextSemanticsLibrary.generated.h"

/** Scalar text compatibility only. Solver policy and validation remain Python. */
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessSolverTextSemanticsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Compatibility", meta=(AIIgnore, ScriptName="evaluate_solver_text_semantics"))
    static FString EvaluateSolverTextSemantics(const FString& ScalarsJson);
};
