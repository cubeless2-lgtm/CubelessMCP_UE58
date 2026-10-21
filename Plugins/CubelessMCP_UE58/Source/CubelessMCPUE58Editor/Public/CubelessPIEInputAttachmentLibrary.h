#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPIEInputAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPIEInputAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_pie_input_toolset"))
    static FString InstallPIEInputToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_pie_input_toolset"))
    static FString VerifyPIEInputToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_pie_input_toolset"))
    static FString RemovePIEInputToolset(UClass* ToolsetClass);
};
