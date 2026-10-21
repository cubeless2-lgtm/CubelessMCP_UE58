#include "CubelessControlRigRecipeAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessControlRigRecipeAuthoringApi.h"
FString UCubelessControlRigRecipeAuthoringLibrary::ValidateControlRigRecipe(const FString& ParamsJson){return Cubeless::UE58::ControlRigRecipeAuthoring::ValidateRecipe(ParamsJson);}
FString UCubelessControlRigRecipeAuthoringLibrary::DryRunControlRigRecipe(const FString& ParamsJson){return Cubeless::UE58::ControlRigRecipeAuthoring::DryRunRecipe(ParamsJson);}
FString UCubelessControlRigRecipeAuthoringLibrary::BuildControlRigRecipe(const FString& ParamsJson){return Cubeless::UE58::ControlRigRecipeAuthoring::BuildRecipe(ParamsJson);}
