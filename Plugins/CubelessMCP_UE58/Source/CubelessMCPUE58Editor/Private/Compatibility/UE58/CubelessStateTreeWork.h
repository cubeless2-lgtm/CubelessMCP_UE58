#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
class UStateTree;
namespace Cubeless::UE58::StateTreeWork
{
bool Loaded(const UObject* Object);
void Count(Variable::FResult& Result, const TCHAR* Key, int32 Amount = 1);
bool Forbidden(const FString& Value);
bool AuditLive(UStateTree* Tree, Variable::FResult& Result, const TCHAR* Label);
bool AuditPersisted(UStateTree* Tree, Variable::FResult& Result);
bool Identity(UStateTree* Tree, const FString& ObjectPath, UClass* SchemaClass);
bool ReadReady(UStateTree* Tree, Variable::FResult& Result, const TCHAR* Label);
}
