#include "Registration/CubelessBlueprintAnnotationLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintAnnotationApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintAnnotationLibrary::AddBlueprintCommentBox(const FString& BlueprintName,const FString& CommentText,const FString& PositionJson,const FString& SizeJson,const FString& ColorJson,int32 FontSize,const FString& MoveMode,bool bCommentBubbleVisible,bool bColorCommentBubble,const FString& NodeDetails,const FString& NodeIdsJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintAnnotation::AddCommentBox(BlueprintName,CommentText,PositionJson,SizeJson,ColorJson,FontSize,MoveMode,bCommentBubbleVisible,bColorCommentBubble,NodeDetails,NodeIdsJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_comment_box"));
}
FString UCubelessBlueprintAnnotationLibrary::SetBlueprintNodeComment(const FString& BlueprintName,const FString& NodeId,const FString& NodeComment,bool bCommentBubbleVisible,bool bCommentBubblePinned,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintAnnotation::SetNodeComment(BlueprintName,NodeId,NodeComment,bCommentBubbleVisible,bCommentBubblePinned,GraphName,GraphId,GraphType),TEXT("set_blueprint_node_comment"));
}
FString UCubelessBlueprintAnnotationLibrary::SetBlueprintFunctionDescription(const FString& BlueprintName,const FString& Description,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintAnnotation::SetFunctionDescription(BlueprintName,Description,GraphName,GraphId,GraphType),TEXT("set_blueprint_function_description"));
}
