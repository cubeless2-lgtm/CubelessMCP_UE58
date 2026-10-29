#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::MaterialComments
{
FString Run(UObject* Asset,const FString& Action,const FString& Text,int32 X,int32 Y,int32 Width,int32 Height,bool Execute,bool Save);
}
