#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMLinkValidationLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMLinkValidationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="validate_controlrig_rigvm_links"))
    static FString ValidateControlRigRigVMLinks(const FString& ControlRigPath, const FString& ControlRigAssetPath,
        const FString& ControlRigClassPath, const FString& LinksJson, const FString& UnitLinksJson,
        bool bAllowTypeCasting, int64 MaxLinks);
};
