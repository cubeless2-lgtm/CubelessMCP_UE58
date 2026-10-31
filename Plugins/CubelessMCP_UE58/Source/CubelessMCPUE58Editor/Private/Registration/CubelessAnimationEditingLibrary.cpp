#include "Registration/CubelessAnimationEditingLibrary.h"
#include "Compatibility/UE58/CubelessAnimationEditingApi.h"
FString UCubelessAnimationEditingLibrary::EditAnimationAsset(UObject* Asset,const FString& Action,const FString& Json,bool Execute,bool Save,bool AllowDirty,const FString& Token,bool UnobservedAssetLoad)
{return Cubeless::UE58::AnimationEditing::Edit(Asset,Action,Json,Execute,Save,AllowDirty,Token,UnobservedAssetLoad);}
FString UCubelessAnimationEditingLibrary::TakeAnimationEditReceipt(const FString& Token)
{return Cubeless::UE58::AnimationEditing::TakeReceipt(Token);}
