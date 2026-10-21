#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Cubeless::UE58::DataTable
{
    FString GetSchema(const FString& Path);
    FString ListRows(const FString& Path, const FString& Contains, int32 MaxRows, bool IncludeValues);
    FString GetRow(const FString& Path, const FString& RowName);
    FString AddOrUpdateRow(const FString& Path, const FString& RowName, const FString& ValuesJson, bool Save);
    FString RemoveRow(const FString& Path, const FString& RowName, bool Save);
    FString Create(const FString& AssetPath, const FString& RowStructPath, bool Save);
FString AddGraphRowNode(const FString& BlueprintPath,const FString& RowStructPath,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
FString AddGraphStringBreakNode(const FString& BlueprintPath,const FString& RowStructPath,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
TSharedRef<FJsonObject> ReadStringRowConnection(const FString& GraphPath,const FString& SourceNodePath,const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName);
}
