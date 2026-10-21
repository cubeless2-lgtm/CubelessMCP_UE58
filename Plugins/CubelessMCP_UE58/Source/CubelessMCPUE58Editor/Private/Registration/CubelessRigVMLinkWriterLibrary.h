#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMLinkWriterLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMLinkWriterLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="add_controlrig_rigvm_links"))
    static FString AddControlRigRigVMLinks(const FString& ControlRigPath, const FString& ControlRigAssetPath, const FString& ControlRigClassPath,
    const FString& LinksJson, const FString& UnitLinksJson, bool bAllowTypeCasting, bool bDryRun,
    bool bCompile, bool bSaveAsset, int64 MaxLinks);
};
