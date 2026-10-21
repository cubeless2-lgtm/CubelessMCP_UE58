#pragma once
#include "CoreMinimal.h"
namespace Cubeless::Operations
{
FString InspectNiagaraUserParameters(const FString& SystemPath);
FString SetNiagaraUserParameter(const FString& SystemPath, const FString& ParameterName, const FString& ValueJson,
    bool bAllowSourceEdit, bool bSave);
FString UpsertNiagaraUserParameters(const FString& SystemPath, const FString& ParametersJson, bool bContinueOnError,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
}
