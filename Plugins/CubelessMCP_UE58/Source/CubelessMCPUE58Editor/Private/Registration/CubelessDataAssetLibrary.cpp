#include "Registration/CubelessDataAssetLibrary.h"
#include "Compatibility/UE58/CubelessDataAssetApi.h"
FString UCubelessDataAssetLibrary::GetDataAsset(const FString& Path)
{ return Cubeless::UE58::DataAsset::Get(Path); }
FString UCubelessDataAssetLibrary::SetDataAssetProperties(const FString& Path, const FString& PropertiesJson, bool bSave)
{ return Cubeless::UE58::DataAsset::Set(Path, PropertiesJson, bSave); }
FString UCubelessDataAssetLibrary::CreateDataAsset(const FString& Path, const FString& ClassPath, bool bSave)
{ return Cubeless::UE58::DataAsset::Create(Path, ClassPath, bSave); }
