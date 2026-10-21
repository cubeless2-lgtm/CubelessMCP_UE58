#include "Registration/CubelessPCGLibrary.h"
#include "Operations/CubelessPCGOperations.h"

FString UCubelessPCGLibrary::InspectSettingsClass(const FString& ClassPath)
{
    return CubelessPCGOperations::InspectSettingsClass(ClassPath);
}

FString UCubelessPCGLibrary::InspectGraphSnapshot(const FString& GraphPath)
{
    return CubelessPCGOperations::InspectGraphSnapshot(GraphPath);
}

FString UCubelessPCGLibrary::InspectSetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName)
{
    return CubelessPCGOperations::InspectSetting(GraphPath, NodeId, PropertyName);
}

FString UCubelessPCGLibrary::ApplySetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& ValueJson)
{
    return CubelessPCGOperations::ApplySetting(GraphPath, NodeId, PropertyName, ValueJson);
}

FString UCubelessPCGLibrary::InspectAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName)
{
    return CubelessPCGOperations::InspectAttributeSelector(GraphPath, NodeId, PropertyName);
}

FString UCubelessPCGLibrary::PreviewAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& RequestJson)
{
    return CubelessPCGOperations::PreviewAttributeSelector(GraphPath, NodeId, PropertyName, RequestJson);
}

FString UCubelessPCGLibrary::InspectConnection(const FString& GraphPath, const FString& FromNode, const FString& FromPin, const FString& ToNode, const FString& ToPin)
{
    return CubelessPCGOperations::InspectConnection(GraphPath, FromNode, FromPin, ToNode, ToPin);
}

FString UCubelessPCGLibrary::NotifyAndRecompile(const FString& GraphPath, bool bAllowDirty)
{
    return CubelessPCGOperations::NotifyAndRecompile(GraphPath, bAllowDirty);
}

FString UCubelessPCGLibrary::PreflightAttributeSelectorAssignment(const FString& ParamsJson)
{
    return CubelessPCGOperations::PreflightAttributeSelectorAssignment(ParamsJson);
}

FString UCubelessPCGLibrary::CommitAttributeSelectorAssignment(const FString& ParamsJson)
{
    return CubelessPCGOperations::CommitAttributeSelectorAssignment(ParamsJson);
}
