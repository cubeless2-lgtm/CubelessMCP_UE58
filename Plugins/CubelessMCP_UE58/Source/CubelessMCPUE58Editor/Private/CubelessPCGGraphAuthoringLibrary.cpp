#include "CubelessPCGGraphAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessPCGGraphAuthoringApi.h"
FString UCubelessPCGGraphAuthoringLibrary::AddPcgNode(const FString& ParamsJson){return Cubeless::UE58::PCGGraphAuthoring::AddNode(ParamsJson);}
FString UCubelessPCGGraphAuthoringLibrary::ConnectPcgNodes(const FString& ParamsJson){return Cubeless::UE58::PCGGraphAuthoring::ConnectNodes(ParamsJson);}
FString UCubelessPCGGraphAuthoringLibrary::SetPcgNodeSetting(const FString& ParamsJson){return Cubeless::UE58::PCGGraphAuthoring::SetNodeSetting(ParamsJson);}

FString UCubelessPCGGraphAuthoringLibrary::CreateGraphFromSpecJson(const FString& ParamsJson){return Cubeless::UE58::PCGGraphAuthoring::CreateGraphFromSpec(ParamsJson);}
