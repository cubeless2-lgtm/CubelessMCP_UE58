#include "Registration/CubelessEnvQueryLibrary.h"
#include "Compatibility/UE58/CubelessEnvQueryApi.h"

FString UCubelessEnvQueryLibrary::CreateAIEQSQuery(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset)
{ return Cubeless::UE58::EnvQuery::CreateAsset(AssetName, AssetRoot, bSaveAsset); }
FString UCubelessEnvQueryLibrary::InspectAIAssetStructure(const FString& AssetPath)
{ return Cubeless::UE58::EnvQuery::InspectAsset(AssetPath); }
