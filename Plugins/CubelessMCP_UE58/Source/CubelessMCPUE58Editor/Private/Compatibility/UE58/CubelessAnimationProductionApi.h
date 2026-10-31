#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace Cubeless::UE58::AnimationProduction
{
bool AuditRuntimeInput(UObject* Object,FString& Error,TSharedPtr<FJsonObject> Observation=nullptr);
FString Create(const FString& Kind,const FString& Path,UObject* Reference,bool Execute,bool Save);
FString EvaluateChooser(UObject* Target,UObject* Context);
FString Inspect(UObject* Target);
FString InspectBindings(UObject* Target);
FString Edit(UObject* Target,const FString& Action,const FString& Arguments,bool Execute,bool Save,bool AllowDirty,bool AllowRemove);
FString BindNode(UObject* Blueprint,const FString& Action,const FString& Graph,const FString& NodeId,const FString& Source,bool Execute,bool Compile,bool Save,bool AllowDirty,bool AllowReplace);
FString BindChooser(UObject* Blueprint,const FString& Graph,const FString& NodeId,const FString& ChooserPath,const FString& FunctionName,bool Execute,bool Compile,bool Save,bool AllowDirty,bool AllowReplace);
FString CreateLayer(UObject* Blueprint,const FString& Name,bool Execute,bool Compile,bool Save,bool AllowDirty);
FString Index(UObject* Target,const FString& JobId,bool Start);
}
