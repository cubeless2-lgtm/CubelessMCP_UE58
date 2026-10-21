#include "Registration/CubelessParticleReceiverLibrary.h"
#include "Compatibility/UE58/CubelessParticleReceiverApi.h"
#include "Compatibility/UE58/CubelessParticleReceiverConstructionApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Operations/CubelessParticleReceiverOperations.h"

FString UCubelessParticleReceiverLibrary::InspectNativeSignatures()
{
    return Cubeless::Operations::EncodeFunctionResult(
        Cubeless::UE58::ParticleReceiver::InspectNativeSignatures(), TEXT("inspect_particle_receiver_native_signatures"));
}

FString UCubelessParticleReceiverLibrary::ReadParticleReceiverPlan(const FString& GraphPath)
{ return Cubeless::Operations::ReadParticleReceiverPlan(GraphPath); }
FString UCubelessParticleReceiverLibrary::ImplementParticleCallbackInterface(const FString& GraphPath)
{ return Cubeless::Operations::ImplementParticleCallbackInterface(GraphPath); }
FString UCubelessParticleReceiverLibrary::ImplementBlueprintInterface(const FString& BlueprintPath,const FString& InterfaceClass)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::ParticleReceiver::ImplementBlueprintInterface(BlueprintPath,InterfaceClass),TEXT("add_blueprint_interface")); }
FString UCubelessParticleReceiverLibrary::AddParticleDataEventNode(const FString& GraphPath, int32 X, int32 Y)
{ return Cubeless::Operations::AddParticleDataEventNode(GraphPath, X, Y); }
FString UCubelessParticleReceiverLibrary::AddParticleDataArrayLengthNode(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return Cubeless::Operations::AddParticleDataArrayLengthNode(GraphPath, EventNodePath, X, Y); }
FString UCubelessParticleReceiverLibrary::AddParticleDataArrayGetCopyNode(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return Cubeless::Operations::AddParticleDataArrayGetCopyNode(GraphPath, EventNodePath, X, Y); }
FString UCubelessParticleReceiverLibrary::AddBasicParticleDataBreakNode(const FString& GraphPath, int32 X, int32 Y)
{ return Cubeless::Operations::AddBasicParticleDataBreakNode(GraphPath, X, Y); }
