#include "Operations/CubelessPCGOperations.h"
#include "Compatibility/UE58/CubelessPCGApi.h"

FString CubelessPCGOperations::InspectSettingsClass(const FString& ClassPath)
{
    return CubelessPCGUE58::InspectSettingsClass(ClassPath);
}

FString CubelessPCGOperations::InspectGraphSnapshot(const FString& GraphPath)
{
    return CubelessPCGUE58::InspectGraphSnapshot(GraphPath);
}

FString CubelessPCGOperations::InspectSetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName)
{
    return CubelessPCGUE58::InspectSetting(GraphPath, NodeId, PropertyName);
}

FString CubelessPCGOperations::ApplySetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& ValueJson)
{
    return CubelessPCGUE58::ApplySetting(GraphPath, NodeId, PropertyName, ValueJson);
}

FString CubelessPCGOperations::InspectAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName)
{
    return CubelessPCGUE58::InspectAttributeSelector(GraphPath, NodeId, PropertyName);
}

FString CubelessPCGOperations::PreviewAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& RequestJson)
{
    return CubelessPCGUE58::PreviewAttributeSelector(GraphPath, NodeId, PropertyName, RequestJson);
}

FString CubelessPCGOperations::InspectConnection(const FString& GraphPath, const FString& FromNode, const FString& FromPin, const FString& ToNode, const FString& ToPin)
{
    return CubelessPCGUE58::InspectConnection(GraphPath, FromNode, FromPin, ToNode, ToPin);
}

FString CubelessPCGOperations::NotifyAndRecompile(const FString& GraphPath, bool bAllowDirty)
{
    return CubelessPCGUE58::NotifyAndRecompile(GraphPath, bAllowDirty);
}

FString CubelessPCGOperations::PreflightAttributeSelectorAssignment(const FString& ParamsJson)
{
    return CubelessPCGUE58::PreflightAttributeSelectorAssignment(ParamsJson);
}

FString CubelessPCGOperations::CommitAttributeSelectorAssignment(const FString& ParamsJson)
{
    return CubelessPCGUE58::CommitAttributeSelectorAssignment(ParamsJson);
}
