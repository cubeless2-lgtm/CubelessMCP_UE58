#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintAnnotation
{
Function::FResult AddCommentBox(const FString& BlueprintName,const FString& CommentText,const FString& PositionJson,const FString& SizeJson,const FString& ColorJson,int32 FontSize,const FString& MoveMode,bool bCommentBubbleVisible,bool bColorCommentBubble,const FString& NodeDetails,const FString& NodeIdsJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
Function::FResult SetNodeComment(const FString& BlueprintName,const FString& NodeId,const FString& NodeComment,bool bCommentBubbleVisible,bool bCommentBubblePinned,const FString& GraphName,const FString& GraphId,const FString& GraphType);
Function::FResult SetFunctionDescription(const FString& BlueprintName,const FString& Description,const FString& GraphName,const FString& GraphId,const FString& GraphType);
}
