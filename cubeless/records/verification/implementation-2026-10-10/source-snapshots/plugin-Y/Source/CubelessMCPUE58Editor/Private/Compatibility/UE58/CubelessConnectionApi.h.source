#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphSchema.h"

namespace Cubeless::UE58::Connection
{
const TCHAR* SchemaResponseName(ECanCreateConnectionResponse Response);
TSharedRef<FJsonObject> ReplaceExecLink(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName,
    const FString& ExpectedOldTargetNodePath, const FString& ExpectedOldTargetPinName,
    int32 ExpectedSourceLinkCount, const FString& ExpectedSchemaResponse);
}
