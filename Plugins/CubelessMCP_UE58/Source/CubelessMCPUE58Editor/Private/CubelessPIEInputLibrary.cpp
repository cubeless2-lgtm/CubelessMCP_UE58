#include "CubelessPIEInputLibrary.h"
#include "Compatibility/UE58/CubelessPIEInputApi.h"
FString UCubelessPIEInputLibrary::InjectPIEInputKey(const FString& P){return Cubeless::UE58::PIEInput::InjectKey(P);}
