#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessUserDefinedEnumLibrary.generated.h"

UCLASS()
class UCubelessUserDefinedEnumLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="inspect_user_defined_enum"))
    static FString InspectUserDefinedEnum(const FString& AssetPath);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="edit_user_defined_enum"))
    static FString EditUserDefinedEnum(const FString& AssetPath,const FString& EditsJson,
        bool bDryRun=true,bool bSave=false,bool bAllowRemove=false);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="create_user_defined_enum"))
    static FString CreateUserDefinedEnum(const FString& AssetPath, const FString& DisplayNamesJson,
        const FString& TooltipsJson, bool bDryRun, bool bSave, bool bSkipIfExists);
};
