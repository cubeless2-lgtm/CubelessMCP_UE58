#include "Registration/CubelessAnimationProductionLibrary.h"
#include "Compatibility/UE58/CubelessAnimationProductionApi.h"
#include "Compatibility/UE58/CubelessAnimationStateEditingApi.h"
FString UCubelessAnimationProductionLibrary::CreateAnimationProductionAsset(const FString& K,const FString& P,UObject* R,bool E,bool S){return Cubeless::UE58::AnimationProduction::Create(K,P,R,E,S);}
FString UCubelessAnimationProductionLibrary::InspectAnimationProductionAsset(UObject* T){return Cubeless::UE58::AnimationProduction::Inspect(T);}
FString UCubelessAnimationProductionLibrary::EditAnimationProductionAsset(UObject* T,const FString& A,const FString& J,bool E,bool S,bool D,bool R){return Cubeless::UE58::AnimationProduction::Edit(T,A,J,E,S,D,R);}
FString UCubelessAnimationProductionLibrary::AnimationProductionIndex(UObject* T,const FString& J,bool S){return Cubeless::UE58::AnimationProduction::Index(T,J,S);}
FString UCubelessAnimationProductionLibrary::BindAnimationProductionNode(UObject* B,const FString& A,const FString& G,const FString& N,const FString& S,bool E,bool C,bool V,bool D,bool R){return Cubeless::UE58::AnimationProduction::BindNode(B,A,G,N,S,E,C,V,D,R);}
FString UCubelessAnimationProductionLibrary::EvaluateAnimationProductionChooser(UObject* T,UObject* C,const FString& J){return Cubeless::UE58::AnimationProduction::EvaluateChooser(T,{C},{J});}
FString UCubelessAnimationProductionLibrary::EvaluateAnimationProductionChooserContexts(UObject* Target,const TArray<UObject*>& Contexts,const TArray<FString>& StructJsons)
{
 return Cubeless::UE58::AnimationProduction::EvaluateChooser(Target,Contexts,StructJsons);
}

FString UCubelessAnimationProductionLibrary::BindAnimationProductionChooser(UObject* B,const FString& G,const FString& N,const FString& P,const FString& F,bool E,bool C,bool S,bool D,bool R){return Cubeless::UE58::AnimationProduction::BindChooser(B,G,N,P,F,E,C,S,D,R);}

FString UCubelessAnimationProductionLibrary::CreateAnimationProductionLayer(UObject* B,const FString& N,bool E,bool C,bool S,bool D){return Cubeless::UE58::AnimationProduction::CreateLayer(B,N,E,C,S,D);}

FString UCubelessAnimationProductionLibrary::InspectAnimationStateGraph(UObject* B,const FString& G){return Cubeless::UE58::AnimationStateEditing::Inspect(B,G);}
FString UCubelessAnimationProductionLibrary::EditAnimationStateGraph(UObject* B,const FString& A,const FString& G,const FString& J,bool E,bool C,bool S,bool D,bool R){return Cubeless::UE58::AnimationStateEditing::Edit(B,A,G,J,E,C,S,D,R);}
