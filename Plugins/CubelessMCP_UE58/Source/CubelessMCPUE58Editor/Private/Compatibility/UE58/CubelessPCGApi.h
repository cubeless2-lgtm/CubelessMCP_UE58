#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UPCGGraph;
class UPCGNode;
class FProperty;

namespace CubelessPCGUE58
{
    FString InspectSettingsClass(const FString& ClassPath);
    FString InspectGraphSnapshot(const FString& GraphPath);
    FString InspectSetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName);
    FString ApplySetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& ValueJson);
    FString InspectAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName);
    FString PreviewAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& RequestJson);
    FString PreflightAttributeSelectorAssignment(const FString& ParamsJson);
    FString CommitAttributeSelectorAssignment(const FString& ParamsJson);
    FString InspectConnection(const FString& GraphPath, const FString& FromNode, const FString& FromPin, const FString& ToNode, const FString& ToPin);
    FString NotifyAndRecompile(const FString& GraphPath, bool bAllowDirty);

    // Exact editor-only DensityFilter debug bool; no other Transient field.
    bool IsDensityFilterEditorDebugProperty(const UClass* Class, FProperty* Property);

    // Internal fresh-constructor leaves. The caller owns the exact newly
    // returned graph/nodes and supplies its current-role fence; no public gate.
    // Prepared point ImportText is opt-in for standalone only; old callers remain unchanged.
    bool PrepareFreshSettings(UClass* Class, const TSharedPtr<FJsonObject>& Input,
        TSharedPtr<FJsonObject>& Plans, TFunctionRef<bool()> Current, bool bPreparePointImport = false);
    bool ApplyFreshSetting(UPCGGraph* Owner, UPCGNode* Member, const FString& Name,
        const TSharedPtr<FJsonValue>& Input, TSharedPtr<FJsonObject>& Receipt,
        TFunctionRef<bool()> Current);
    bool VerifyFreshSettings(UPCGGraph* Owner, UPCGNode* Member,
        const TSharedPtr<FJsonObject>& Input, TSharedPtr<FJsonObject>& Receipt,
        TFunctionRef<bool()> Current);
}
