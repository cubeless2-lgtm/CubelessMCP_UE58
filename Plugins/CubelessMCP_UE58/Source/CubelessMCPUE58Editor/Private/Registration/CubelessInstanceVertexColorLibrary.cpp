#include "Registration/CubelessInstanceVertexColorLibrary.h"
#include "Compatibility/UE58/CubelessInstanceVertexColorsApi.h"
FString UCubelessInstanceVertexColorLibrary::InspectInstanceVertexColors(UObject* C,int32 L,int32 S,int32 N,bool P){return Cubeless::UE58::InstanceVertexColors::Inspect(C,L,S,N,P);}
FString UCubelessInstanceVertexColorLibrary::SetInstanceVertexColors(UObject* C,const FString& J,int32 L,bool E,bool R,bool S,bool D){return Cubeless::UE58::InstanceVertexColors::Apply(C,L,J,false,E,R,S,D);}
FString UCubelessInstanceVertexColorLibrary::ClearInstanceVertexColors(UObject* C,int32 L,bool E,bool R,bool S,bool D){return Cubeless::UE58::InstanceVertexColors::Apply(C,L,TEXT(""),true,E,R,S,D);}
