#include "Registration/CubelessUserDefinedEnumLibrary.h"
#include "Compatibility/UE58/CubelessUserDefinedEnumApi.h"
#include "Compatibility/UE58/CubelessUserDefinedEnumEditingApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessUserDefinedEnumLibrary::CreateUserDefinedEnum(const FString& AssetPath,
    const FString& DisplayNamesJson, const FString& TooltipsJson, bool bDryRun, bool bSave, bool bSkipIfExists)
{
    return Cubeless::Operations::EncodeVariableResult(
        Cubeless::UE58::UserDefinedEnum::Create(AssetPath, DisplayNamesJson, TooltipsJson,
            bDryRun, bSave, bSkipIfExists), TEXT("create_user_defined_enum"));
}

FString UCubelessUserDefinedEnumLibrary::InspectUserDefinedEnum(const FString& AssetPath)
{return Cubeless::UE58::UserDefinedEnumEditing::Inspect(AssetPath);}
FString UCubelessUserDefinedEnumLibrary::EditUserDefinedEnum(const FString& AssetPath,const FString& EditsJson,bool bDryRun,bool bSave,bool bAllowRemove)
{return Cubeless::UE58::UserDefinedEnumEditing::Edit(AssetPath,EditsJson,bDryRun,bSave,bAllowRemove);}
