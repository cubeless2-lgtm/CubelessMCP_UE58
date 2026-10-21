#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosCacheSessionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosCacheSessionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_cache_sessions_toolset"))
    static FString InstallChaosCacheSessionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_cache_sessions_toolset"))
    static FString VerifyChaosCacheSessionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_cache_sessions_toolset"))
    static FString RemoveChaosCacheSessionToolset(UClass* ToolsetClass);
};
