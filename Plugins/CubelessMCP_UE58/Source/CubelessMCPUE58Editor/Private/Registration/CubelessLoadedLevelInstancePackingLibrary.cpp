#include "Registration/CubelessLoadedLevelInstancePackingLibrary.h"
#include "Compatibility/UE58/CubelessLoadedLevelInstancePackingApi.h"

FString UCubelessLoadedLevelInstancePackingLibrary::ExecutePackingRequest(const FString& RequestJson)
{
    return Cubeless::UE58::PackLoadedLevelInstance(RequestJson);
}
