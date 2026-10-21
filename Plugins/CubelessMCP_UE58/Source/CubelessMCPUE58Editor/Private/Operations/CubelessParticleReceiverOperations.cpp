#include "Operations/CubelessParticleReceiverOperations.h"
#include "Compatibility/UE58/CubelessParticleReceiverConstructionApi.h"
#include "Operations/CubelessFunctionOperations.h"

namespace Cubeless::Operations
{
FString ReadParticleReceiverPlan(const FString& GraphPath)
{ return EncodeFunctionResult(UE58::ParticleReceiver::ReadPlan(GraphPath), TEXT("read_particle_receiver_plan")); }
FString ImplementParticleCallbackInterface(const FString& GraphPath)
{ return EncodeFunctionResult(UE58::ParticleReceiver::ImplementInterface(GraphPath), TEXT("implement_particle_callback_interface")); }
FString AddParticleDataEventNode(const FString& GraphPath, int32 X, int32 Y)
{ return EncodeFunctionResult(UE58::ParticleReceiver::AddEvent(GraphPath, X, Y), TEXT("add_particle_data_event_node")); }
FString AddParticleDataArrayLengthNode(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return EncodeFunctionResult(UE58::ParticleReceiver::AddArrayLength(GraphPath, EventNodePath, X, Y), TEXT("add_particle_data_array_length_node")); }
FString AddParticleDataArrayGetCopyNode(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return EncodeFunctionResult(UE58::ParticleReceiver::AddArrayGetCopy(GraphPath, EventNodePath, X, Y), TEXT("add_particle_data_array_get_copy_node")); }
FString AddBasicParticleDataBreakNode(const FString& GraphPath, int32 X, int32 Y)
{ return EncodeFunctionResult(UE58::ParticleReceiver::AddBreak(GraphPath, X, Y), TEXT("add_basic_particle_data_break_node")); }
}
