#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::Graph
{
Function::FResult ListBlueprintNodes(const FString& B,const FString& NT,const FString& Title,bool IncludePins,const FString& GN,const FString& GI,const FString& GT);
Function::FResult DeleteBlueprintNode(const FString& B,const FString& Id,const FString& GN,const FString& GI,const FString& GT,const FString& ExpectedName,const FString& ExpectedClass,const FString& ExpectedTitle,bool AllowData,bool AllowExec,bool AllowAny,bool AllowOrphan);
Function::FResult AddBlueprintBranchNode(const FString& BlueprintPath,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
Function::FResult AddBlueprintSequenceNode(const FString& BlueprintPath,const FString& PositionJson,int32 OutputCount,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
Function::FResult AddStringDoubleLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddStringDoubleLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddPrimitiveLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddPrimitiveLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddNativeValueLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddNativeValueLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddReferencedLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddReferencedLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddPrimitiveArrayLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
Function::FResult AddPrimitiveArrayLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
TSharedRef<FJsonObject> ReadNodeIdentities(const FString& GraphPath);
TSharedRef<FJsonObject> ReadMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter);
TSharedRef<FJsonObject> ReadExternalMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath);
TSharedRef<FJsonObject> ReadConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName);
}
