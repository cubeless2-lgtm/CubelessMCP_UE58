#include "Registration/CubelessBlackboardLibrary.h"
#include "Compatibility/UE58/CubelessBlackboardApi.h"

FString UCubelessBlackboardLibrary::CreateAIBlackboardAsset(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset)
{ return Cubeless::UE58::Blackboard::CreateAsset(AssetName, AssetRoot, bSaveAsset); }
FString UCubelessBlackboardLibrary::AddAIBlackboardKey(const FString& BlackboardPath, const FString& KeyName, const FString& KeyType,
    const FString& BaseClass, const FString& EnumPath, bool bInstanceSynced, bool bSaveAsset)
{ return Cubeless::UE58::Blackboard::AddKey(BlackboardPath, KeyName, KeyType, BaseClass, EnumPath, bInstanceSynced, bSaveAsset); }
FString UCubelessBlackboardLibrary::RemoveAIBlackboardKey(const FString& BlackboardPath, const FString& KeyName, bool bSaveAsset)
{ return Cubeless::UE58::Blackboard::RemoveKey(BlackboardPath, KeyName, bSaveAsset); }
FString UCubelessBlackboardLibrary::InspectAIAssetStructure(const FString& AssetPath)
{ return Cubeless::UE58::Blackboard::InspectAsset(AssetPath); }
