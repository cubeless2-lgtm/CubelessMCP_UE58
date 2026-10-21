#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintAnnotationLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintAnnotationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_comment_box"))
    static FString AddBlueprintCommentBox(const FString& BlueprintName,const FString& CommentText,const FString& PositionJson,const FString& SizeJson,const FString& ColorJson,int32 FontSize,const FString& MoveMode,bool bCommentBubbleVisible,bool bColorCommentBubble,const FString& NodeDetails,const FString& NodeIdsJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="set_blueprint_node_comment"))
    static FString SetBlueprintNodeComment(const FString& BlueprintName,const FString& NodeId,const FString& NodeComment,bool bCommentBubbleVisible,bool bCommentBubblePinned,const FString& GraphName,const FString& GraphId,const FString& GraphType);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="set_blueprint_function_description"))
    static FString SetBlueprintFunctionDescription(const FString& BlueprintName,const FString& Description,const FString& GraphName,const FString& GraphId,const FString& GraphType);
};
