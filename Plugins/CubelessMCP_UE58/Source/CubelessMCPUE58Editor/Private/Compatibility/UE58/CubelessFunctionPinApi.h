#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "EdGraph/EdGraphPin.h"
namespace Cubeless::UE58::Function::Pin
{
bool SafeName(const FString& Text, bool bEmpty = false);
bool Identifier(const FString& Text);
// Existing callers retain the exact-integer-range numeric policy. Only explicitly opted-in typed value JSON admits all finite binary64 numbers.
bool ParseJson(const FString& Text, TSharedPtr<FJsonValue>& Value, FResult& Result,bool bAllowFiniteDouble=false);
bool PrimitiveType(const FString& Name, FEdGraphPinType& Type, FResult& Result);
bool IsPrimitive(const FEdGraphPinType& Type);
// Exact scalar runtime value structures for owned ordinary function signatures.
// This does not admit arbitrary native/user structures, references or containers.
bool ScalarParameterType(const FString& Name, FEdGraphPinType& Type, FResult& Result);
bool IsScalarValueStruct(const FEdGraphPinType& Type);
TSharedRef<FJsonObject> ScalarValueStructSource(const FEdGraphPinType& Type);
// JM: exact currently registered Object/Actor scalar constraints only. No asset
// load, CDO creation, generated/plugin class or RuntimeObject-wide admission.
// PH: only the referenced-array descriptor opts in exact ActorComponent/CAC/Skeletal constraints; scalar observers stay unchanged.
bool NativeClassParameterType(const FString& Name,const FString& TypeObject,FEdGraphPinType& Type,FResult& Result,bool bReferencedArray=false);
TSharedRef<FJsonObject> NativeClassSource(const FEdGraphPinType& Type);
bool NativeClassDefaultText(const FEdGraphPinType& Type,const TSharedPtr<FJsonValue>& Value,FString& Text,FResult& Result);
bool ScalarValueDefaultText(const FEdGraphPinType& Type, const TSharedPtr<FJsonValue>& Value, FString& Text, FResult& Result);
bool PrimitiveArrayDefaultText(const FEdGraphPinType& Type,const TSharedPtr<FJsonValue>& Value,FString& Text,FResult& Result);
// Called only after the caller has bound the finite scalar UDS source; one declaration string, no field/buffer import.
bool ScalarUdsDeclarationDefaultText(const TSharedPtr<FJsonValue>& Value,FString& Text,FResult& Result);
bool DefaultText(const FEdGraphPinType& Type, const TSharedPtr<FJsonValue>& Value, FString& Text, FResult& Result);
// Called only after the exact audited PlayerController.IsInputKeyDown.Key contract.
bool InputKeyDefaultText(const FEdGraphPinType& Type, const TSharedPtr<FJsonValue>& Value, FString& Text, FResult& Result);
TSharedRef<FJsonObject> Describe(const FEdGraphPinType& Type);
}
