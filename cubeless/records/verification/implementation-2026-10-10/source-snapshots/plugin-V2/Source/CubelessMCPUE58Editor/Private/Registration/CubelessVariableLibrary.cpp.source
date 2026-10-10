#include "Registration/CubelessVariableLibrary.h"
#include "Operations/CubelessVariableOperations.h"
using namespace Cubeless;
FString UCubelessVariableLibrary::AddBlueprintVariable(const FString& Path, const FString& Name, const FString& Type,
    bool Exposed, const FString& Default, const FString& Category, const FString& Tooltip, const FString& Friendly,
    const FString& TypeObject, bool Array, const FString& Metadata, const FString& Guid, bool Editable)
{ return Operations::EncodeVariableResult(UE58::Variable::Add(Path, Name, Type, Exposed, Default, Category, Tooltip, Friendly, TypeObject, Array, Metadata, Guid, Editable), TEXT("add_blueprint_variable")); }
FString UCubelessVariableLibrary::InspectBlueprintVariables(const FString& Path)
{ return Operations::EncodeVariableResult(UE58::Variable::Inspect(Path, TEXT(""), false), TEXT("inspect_blueprint_variables")); }
FString UCubelessVariableLibrary::InspectBlueprintVariableMetadata(const FString& Path, const FString& Name)
{ return Operations::EncodeVariableResult(UE58::Variable::Inspect(Path, Name, true), TEXT("inspect_blueprint_variable_metadata")); }
FString UCubelessVariableLibrary::SetBlueprintVariableDefault(const FString& Path, const FString& Name, const FString& Json, bool Compile, bool Save, bool DryRun)
{ return Operations::EncodeVariableResult(UE58::Variable::SetDefault(Path, Name, Json, Compile, Save, DryRun), TEXT("set_blueprint_variable_default")); }
