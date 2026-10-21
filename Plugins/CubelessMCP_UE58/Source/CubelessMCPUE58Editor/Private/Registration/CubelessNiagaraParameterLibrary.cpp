#include "Registration/CubelessNiagaraParameterLibrary.h"
#include "Operations/CubelessNiagaraParameterOperations.h"
FString UCubelessNiagaraParameterLibrary::InspectNiagaraUserParameters(const FString& SystemPath)
{ return Cubeless::Operations::InspectNiagaraUserParameters(SystemPath); }
FString UCubelessNiagaraParameterLibrary::SetNiagaraUserParameter(const FString& SystemPath, const FString& ParameterName,
    const FString& ValueJson, bool bAllowSourceEdit, bool bSave)
{ return Cubeless::Operations::SetNiagaraUserParameter(SystemPath, ParameterName, ValueJson, bAllowSourceEdit, bSave); }
FString UCubelessNiagaraParameterLibrary::UpsertNiagaraUserParameters(const FString& SystemPath, const FString& ParametersJson,
    bool bContinueOnError, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{ return Cubeless::Operations::UpsertNiagaraUserParameters(SystemPath, ParametersJson, bContinueOnError, bAllowSourceEdit, bSave, bRequestCompile); }
