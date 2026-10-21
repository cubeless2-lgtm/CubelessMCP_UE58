#include "Registration/CubelessInputMappingLibrary.h"
#include "Compatibility/UE58/CubelessInputMappingApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessInputMappingLibrary::CreateInputMapping(const FString& ActionName, const FString& Key,
    const FString& InputType, bool bShift, bool bCtrl, bool bAlt, bool bCmd)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::InputMapping::Create(
        ActionName, Key, InputType, bShift, bCtrl, bAlt, bCmd), TEXT("create_input_mapping"));
}
