#include "CubelessLocalApiLibrary.h"
#include "Registration/CubelessLocalFacadeDefaults.h"
#include "Compatibility/UE58/CubelessLocalApiStateApi.h"

FString UCubelessLocalApiLibrary::ReadLocalApiState()
{
    return Cubeless::UE58::ReadLocalApiState();
}

FString UCubelessLocalApiLibrary::InstallLocalFacadeDefaults(UClass* ToolsetClass)
{ return Cubeless::Registration::InstallLocalFacadeDefaults(ToolsetClass); }
FString UCubelessLocalApiLibrary::VerifyLocalFacadeDefaults(UClass* ToolsetClass)
{ return Cubeless::Registration::VerifyLocalFacadeDefaults(ToolsetClass); }
FString UCubelessLocalApiLibrary::RemoveLocalFacadeDefaults(UClass* ToolsetClass, bool RestoreOriginal)
{ return Cubeless::Registration::RemoveLocalFacadeDefaults(ToolsetClass, RestoreOriginal); }
