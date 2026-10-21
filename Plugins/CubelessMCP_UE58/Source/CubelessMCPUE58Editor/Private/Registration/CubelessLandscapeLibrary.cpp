#include "Registration/CubelessLandscapeLibrary.h"
#include "Compatibility/UE58/CubelessLandscapeApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessLandscapeLibrary::ListLandscapes()
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::LandscapeAuthoring::List(),TEXT("list_landscapes"));
}
FString UCubelessLandscapeLibrary::GetLandscapeInfo(FString ActorLabel,FString PrivateContextJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::LandscapeAuthoring::Inspect(ActorLabel,PrivateContextJson),TEXT("get_landscape_info"));
}
FString UCubelessLandscapeLibrary::CreateLandscape(FString ActorLabel,FString MaterialPath,FString LocationJson,FString ScaleJson,
    int64 ComponentCountX,int64 ComponentCountY,int64 SectionsPerComponent,int64 QuadsPerSection,int64 HeightValue,
    bool bAllowNonTempLevel,FString RequiredRoot,FString PrivateContextJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::LandscapeAuthoring::Create(ActorLabel,MaterialPath,LocationJson,ScaleJson,
        ComponentCountX,ComponentCountY,SectionsPerComponent,QuadsPerSection,HeightValue,bAllowNonTempLevel,RequiredRoot,PrivateContextJson),TEXT("create_landscape"));
}
FString UCubelessLandscapeLibrary::SetLandscapeMaterial(FString MaterialPath,FString ActorLabel,FString PrivateContextJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::LandscapeAuthoring::SetMaterial(MaterialPath,ActorLabel,PrivateContextJson),TEXT("set_landscape_material"));
}
