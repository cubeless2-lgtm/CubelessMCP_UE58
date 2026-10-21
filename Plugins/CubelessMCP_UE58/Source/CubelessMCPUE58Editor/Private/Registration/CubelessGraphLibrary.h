#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessGraphLibrary.generated.h"

/** Internal editor helpers; never reference this class from game assets. */
UCLASS()
class UCubelessGraphLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Graph",meta=(AIIgnore,ScriptName="list_blueprint_nodes"))
    static FString ListBlueprintNodes(const FString& B,const FString& NT,const FString& Title,bool IncludePins,const FString& GN,const FString& GI,const FString& GT);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Graph",meta=(AIIgnore,ScriptName="delete_blueprint_node"))
    static FString DeleteBlueprintNode(const FString& B,const FString& Id,const FString& GN,const FString& GI,const FString& GT,const FString& ExpectedName,const FString& ExpectedClass,const FString& ExpectedTitle,bool AllowData,bool AllowExec,bool AllowAny,bool AllowOrphan);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddStringDoubleLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddStringDoubleLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddPrimitiveLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddPrimitiveLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddNativeValueLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddNativeValueLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddReferencedLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddReferencedLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddPrimitiveArrayLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddPrimitiveArrayLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReplaceExecLink(const FString& GraphPath, const FString& SourceNodePath,
        const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName,
        const FString& ExpectedOldTargetNodePath, const FString& ExpectedOldTargetPinName,
        int32 ExpectedSourceLinkCount, const FString& ExpectedSchemaResponse);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddBlueprintBranchNode(const FString& BlueprintPath,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddBlueprintSequenceNode(const FString& BlueprintPath,const FString& PositionJson,int32 OutputCount,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddSequenceNode(const FString& GraphPath, int32 X, int32 Y, int32 OutputCount);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReadNodeIdentities(const FString& GraphPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReadMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReadExternalMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString SetBlueprintPinDefault(const FString& GraphPath, const FString& NodePath, const FString& PinName, const FString& ValueJson, const FString& Direction);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReadConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
        const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName);
};
