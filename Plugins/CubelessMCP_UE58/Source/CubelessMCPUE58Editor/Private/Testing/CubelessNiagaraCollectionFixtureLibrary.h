#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraCollectionFixtureLibrary.generated.h"

// Development fixture only. This is not a migrated Toolset and must never be used by game assets.
UCLASS(Transient)
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraCollectionFixtureLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Development Fixture", meta=(DevelopmentOnly, ScriptName="seed_missing_vec4_declarations"))
    static FString SeedMissingVec4Declarations(const FString& CollectionPath);
};
