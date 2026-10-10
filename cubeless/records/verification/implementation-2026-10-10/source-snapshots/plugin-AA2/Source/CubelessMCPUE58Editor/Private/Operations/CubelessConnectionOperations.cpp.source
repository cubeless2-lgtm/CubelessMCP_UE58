#include "Operations/CubelessConnectionOperations.h"
#include "Compatibility/UE58/CubelessConnectionApi.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace Cubeless::Operations
{
FString ReplaceExecLink(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName,
    const FString& ExpectedOldTargetNodePath, const FString& ExpectedOldTargetPinName,
    int32 ExpectedSourceLinkCount, const FString& ExpectedSchemaResponse)
{
    FString Json;
    FJsonSerializer::Serialize(UE58::Connection::ReplaceExecLink(GraphPath, SourceNodePath, SourcePinName,
        TargetNodePath, TargetPinName, ExpectedOldTargetNodePath, ExpectedOldTargetPinName,
        ExpectedSourceLinkCount, ExpectedSchemaResponse), TJsonWriterFactory<>::Create(&Json));
    return Json;
}
}
