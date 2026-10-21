#include "Registration/CubelessBehaviorTreeLibrary.h"
#include "Compatibility/UE58/CubelessBehaviorTreeApi.h"

FString UCubelessBehaviorTreeLibrary::CreateAIBehaviorTreeAsset(const FString& AssetName, const FString& AssetRoot, const FString& BlackboardPath, bool bSaveAsset)
{ return Cubeless::UE58::BehaviorTree::CreateAsset(AssetName, AssetRoot, BlackboardPath, bSaveAsset); }
FString UCubelessBehaviorTreeLibrary::SetAIBehaviorTreeBlackboard(const FString& BehaviorTreePath, const FString& BlackboardPath, bool bSaveAsset)
{ return Cubeless::UE58::BehaviorTree::SetBlackboard(BehaviorTreePath, BlackboardPath, bSaveAsset); }
FString UCubelessBehaviorTreeLibrary::InspectAIAssetStructure(const FString& AssetPath)
{ return Cubeless::UE58::BehaviorTree::InspectAsset(AssetPath); }
