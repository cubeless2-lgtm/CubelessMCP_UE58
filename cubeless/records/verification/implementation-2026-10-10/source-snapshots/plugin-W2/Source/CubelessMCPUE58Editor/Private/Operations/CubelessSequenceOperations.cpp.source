#include "Operations/CubelessSequenceOperations.h"
#include "Compatibility/UE58/CubelessSequenceApi.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
namespace Cubeless::Operations
{
FString AddSequenceNode(const FString& GraphPath, int32 X, int32 Y, int32 OutputCount)
{
    FString Json;
    FJsonSerializer::Serialize(UE58::Sequence::Add(GraphPath, X, Y, OutputCount), TJsonWriterFactory<>::Create(&Json));
    return Json;
}
}
