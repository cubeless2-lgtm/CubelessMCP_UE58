#include "Registration/CubelessUserDefinedEnumLibrary.h"
#include "Compatibility/UE58/CubelessUserDefinedEnumApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessUserDefinedEnumLibrary::CreateUserDefinedEnum(const FString& AssetPath,
    const FString& DisplayNamesJson, const FString& TooltipsJson, bool bDryRun, bool bSave, bool bSkipIfExists)
{
    return Cubeless::Operations::EncodeVariableResult(
        Cubeless::UE58::UserDefinedEnum::Create(AssetPath, DisplayNamesJson, TooltipsJson,
            bDryRun, bSave, bSkipIfExists), TEXT("create_user_defined_enum"));
}
