#include "Registration/CubelessGraphLibrary.h"
#include "Operations/CubelessGraphOperations.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Operations/CubelessSequenceOperations.h"
#include "Operations/CubelessConnectionOperations.h"
FString UCubelessGraphLibrary::AddPrimitiveArrayLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddPrimitiveArrayLocalGetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_get_node")); }
FString UCubelessGraphLibrary::AddPrimitiveArrayLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddPrimitiveArrayLocalSetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_set_node")); }
FString UCubelessGraphLibrary::AddReferencedLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddReferencedLocalGetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_get_node")); }
FString UCubelessGraphLibrary::AddReferencedLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddReferencedLocalSetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_set_node")); }
FString UCubelessGraphLibrary::AddNativeValueLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddNativeValueLocalGetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_get_node")); }
FString UCubelessGraphLibrary::AddNativeValueLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddNativeValueLocalSetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_set_node")); }
FString UCubelessGraphLibrary::AddPrimitiveLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddPrimitiveLocalGetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_get_node")); }
FString UCubelessGraphLibrary::AddPrimitiveLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddPrimitiveLocalSetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_set_node")); }
FString UCubelessGraphLibrary::ReplaceExecLink(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName,
    const FString& ExpectedOldTargetNodePath, const FString& ExpectedOldTargetPinName,
    int32 ExpectedSourceLinkCount, const FString& ExpectedSchemaResponse)
{ return Cubeless::Operations::ReplaceExecLink(GraphPath, SourceNodePath, SourcePinName, TargetNodePath,
    TargetPinName, ExpectedOldTargetNodePath, ExpectedOldTargetPinName, ExpectedSourceLinkCount, ExpectedSchemaResponse); }
FString UCubelessGraphLibrary::AddBlueprintBranchNode(const FString& B,const FString& P,const FString& GN,const FString& GI,const FString& GT,bool CG)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddBlueprintBranchNode(B,P,GN,GI,GT,CG),TEXT("add_blueprint_branch_node")); }
FString UCubelessGraphLibrary::AddBlueprintSequenceNode(const FString& B,const FString& P,int32 Count,const FString& GN,const FString& GI,const FString& GT,bool CG)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddBlueprintSequenceNode(B,P,Count,GN,GI,GT,CG),TEXT("add_blueprint_sequence_node")); }
FString UCubelessGraphLibrary::AddSequenceNode(const FString& GraphPath, int32 X, int32 Y, int32 OutputCount)
{ return Cubeless::Operations::AddSequenceNode(GraphPath, X, Y, OutputCount); }
FString UCubelessGraphLibrary::ReadNodeIdentities(const FString& GraphPath)
{ return Cubeless::Operations::ReadGraphNodeIdentities(GraphPath); }
FString UCubelessGraphLibrary::ReadMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter)
{ return Cubeless::Operations::ReadGraphMemberVariablePlan(GraphPath, VariableName, bSetter); }
FString UCubelessGraphLibrary::ReadExternalMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath)
{ return Cubeless::Operations::ReadExternalGraphMemberVariablePlan(GraphPath, VariableName, bSetter, TargetClassPath); }
FString UCubelessGraphLibrary::SetBlueprintPinDefault(const FString& GraphPath, const FString& NodePath, const FString& PinName, const FString& ValueJson, const FString& Direction)
{ return Cubeless::Operations::SetBlueprintPinDefault(GraphPath, NodePath, PinName, ValueJson, Direction); }
FString UCubelessGraphLibrary::ReadConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName)
{ return Cubeless::Operations::ReadGraphConnectionPlan(GraphPath, SourceNodePath, SourcePinName, TargetNodePath, TargetPinName); }

FString UCubelessGraphLibrary::AddStringDoubleLocalGetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddStringDoubleLocalGetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_get_node")); }
FString UCubelessGraphLibrary::AddStringDoubleLocalSetter(const FString& GraphPath,const FString& VariableName,const FString& ExpectedLocalGuid,int32 X,int32 Y)
{ return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::AddStringDoubleLocalSetter(GraphPath,VariableName,ExpectedLocalGuid,X,Y),TEXT("add_blueprint_variable_set_node")); }

FString UCubelessGraphLibrary::ListBlueprintNodes(const FString& B,const FString& NT,const FString& Title,bool IncludePins,const FString& GN,const FString& GI,const FString& GT)
{return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::ListBlueprintNodes(B,NT,Title,IncludePins,GN,GI,GT),TEXT("list_blueprint_nodes"));}
FString UCubelessGraphLibrary::DeleteBlueprintNode(const FString& B,const FString& Id,const FString& GN,const FString& GI,const FString& GT,const FString& ExpectedName,const FString& ExpectedClass,const FString& ExpectedTitle,bool AllowData,bool AllowExec,bool AllowAny,bool AllowOrphan)
{return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::Graph::DeleteBlueprintNode(B,Id,GN,GI,GT,ExpectedName,ExpectedClass,ExpectedTitle,AllowData,AllowExec,AllowAny,AllowOrphan),TEXT("delete_blueprint_node"));}
