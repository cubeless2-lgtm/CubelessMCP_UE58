#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::NiagaraParameter
{
Function::FResult Inspect(const FString& SystemPath);
Function::FResult Set(const FString& SystemPath, const FString& ParameterName, const FString& ValueJson,
    bool bAllowSourceEdit, bool bSave);
Function::FResult Upsert(const FString& SystemPath, const FString& ParametersJson, bool bContinueOnError,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
}
