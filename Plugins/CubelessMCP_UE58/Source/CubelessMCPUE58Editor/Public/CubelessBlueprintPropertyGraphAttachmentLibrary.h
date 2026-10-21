#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintPropertyGraphAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintPropertyGraphAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_blueprint_property_graph_toolset"))
    static FString InstallBlueprintPropertyGraphToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_blueprint_property_graph_toolset"))
    static FString VerifyBlueprintPropertyGraphToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_blueprint_property_graph_toolset"))
    static FString RemoveBlueprintPropertyGraphToolset(UClass* ToolsetClass);
};
