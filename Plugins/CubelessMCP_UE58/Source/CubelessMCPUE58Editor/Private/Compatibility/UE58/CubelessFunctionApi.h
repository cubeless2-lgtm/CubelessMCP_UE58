#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
class UBlueprint; class UEdGraph; class UEdGraphNode; class UEdGraphPin; class UEdGraphSchema_K2; class UClass; class UFunction; class UObject; struct FEdGraphPinType;
namespace Cubeless::UE58::Function
{
struct FResult
{
    bool bSuccess = false, bMutationAttempted = false, bChanged = false, bChangeStateKnown = true, bPostconditionPassed = false;
    FString Stage = TEXT("preflight"), ErrorCode, Error;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
// Internal finite PresetLoader lifetime adapter; no public authoring selector is added.
class FPresetLoaderNameArrayScope
{
public:
    virtual ~FPresetLoaderNameArrayScope()=default;
    virtual bool Check()=0;
    virtual bool NativeLength(UClass*& Class,UFunction*& Function,UObject*& ExistingCDO)=0;
    virtual bool Adopt(UEdGraphNode* Node)=0;
    virtual bool Place(int32 X,int32 Y,int32 Inputs)=0;
    virtual bool Attach()=0;
    virtual bool FixGuid()=0;
    virtual bool Freeze()=0;
    virtual bool ExpectType(const UEdGraphPin* Pin,const FEdGraphPinType& Type)=0;
    virtual bool ExpectDefault(const UEdGraphPin* Pin,const FString& Value,const FString& Auto)=0;
    virtual TSharedPtr<FJsonObject> Context()=0;
};
TSharedPtr<FPresetLoaderNameArrayScope> BeginPresetLoaderNameArray(FResult& Result,UBlueprint* Blueprint,
    const FString& GraphName,const FString& GraphId,const FString& GraphType,bool Length,
    UEdGraph*& Graph,const UEdGraphSchema_K2*& Schema);
TSharedPtr<FPresetLoaderNameArrayScope> HoldPresetLoaderNameArrayConnection(FResult& Result,UEdGraph* Graph,
    UEdGraphNode* Source,const FString& SourcePinName,UEdGraphNode* Target,const FString& TargetPinName);
// PT concrete node-only missing function creation; no loader, public selector or explicit compile/save.
bool CreateFunctionGraphForNode(UBlueprint* Blueprint,const FString& Name,FResult& Result,UEdGraph*& Graph,
    TFunction<bool(UEdGraphNode*)>& Current,UBlueprint* ReadonlySource=nullptr);
// Internal original default EventGraph continuation; no new public selector.
bool CreateEventGraphForNode(UBlueprint* Blueprint,FResult& Result,UEdGraph*& Graph,TFunction<bool(UEdGraphNode*)>& Current,bool bRequestedCreate,bool bDefaultSelector);
// Branch/Sequence defer only the default allocation to keep their R/lease alive.
FResult Resolve(const FString& BlueprintPath, const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreate,bool bDeferDefaultEventCreation=false);
FResult InspectCompiledFunction(const FString& BlueprintPath, const FString& FunctionClass, const FString& FunctionName);
FResult AddParameter(const FString& BlueprintPath, const FString& Name, const FString& Type, const FString& Direction,
    const FString& DefaultJson, const FString& TypeObject, bool bArray, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool bCreateGraph);
FResult AddReturn(const FString& BlueprintPath, const FString& PositionJson, const FString& GraphName,
    const FString& GraphId, const FString& GraphType, bool bCreateGraph);
FResult AddDynamicCast(const FString& BlueprintPath, const FString& TargetClass, const FString& PositionJson, bool bPure,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraph);
FResult AddCall(const FString& BlueprintPath, const FString& FunctionClass, const FString& FunctionName,
    const FString& DefaultsJson, const FString& PositionJson, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool bCreateGraph, bool bLatent, bool bEditorOnly, bool bWildcard, bool bDeprecated, bool bInternal);
FResult AddBreakAssetData(const FString& BlueprintPath,const FString& StructType,const FString& PositionJson,
    const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
TSharedRef<FJsonObject> ReadAssetDataBreakConnection(const FString& GraphPath,const FString& SourceNodePath,
    const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName);
TSharedRef<FJsonObject> ReadPresetLoaderDataTableCastContext(const FString& GraphPath,const FString& NodePath);
TSharedRef<FJsonObject> ConnectDataTableBoolSelect(const FString& GraphPath,const FString& SourceNodePath,
    const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName);
FResult AddIsValidExecMacro(const FString& BlueprintPath,const FString& PositionJson,const FString& GraphName,
    const FString& GraphId,const FString& GraphType,bool bCreateGraph);
TSharedRef<FJsonObject> ReadIsValidExecMacroConnection(const FString& GraphPath,const FString& SourceNodePath,
    const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName);
FResult AddDataTableBoolSelect(const FString& BlueprintPath,const FString& PositionJson,const FString& GraphName,
    const FString& GraphId,const FString& GraphType,bool bCreateGraph);
TSharedRef<FJsonObject> ReadDataTableBoolSelectConnection(const FString& GraphPath,const FString& SourceNodePath,
    const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName);
FResult AddMakeStruct(const FString& BlueprintPath, const FString& StructType, const FString& PositionJson,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraph);
}
