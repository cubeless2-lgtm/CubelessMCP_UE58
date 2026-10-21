#include "Registration/CubelessRigVMLinkValidationLibrary.h"
#include "Compatibility/UE58/CubelessRigVMLinkValidationApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessRigVMLinkValidationLibrary::ValidateControlRigRigVMLinks(const FString& ControlRigPath,
    const FString& ControlRigAssetPath, const FString& ControlRigClassPath, const FString& LinksJson,
    const FString& UnitLinksJson, bool bAllowTypeCasting, int64 MaxLinks)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::RigVMLinkValidation::Validate(
        ControlRigPath, ControlRigAssetPath, ControlRigClassPath, LinksJson, UnitLinksJson,
        bAllowTypeCasting, MaxLinks), TEXT("validate_controlrig_rigvm_links"));
}
