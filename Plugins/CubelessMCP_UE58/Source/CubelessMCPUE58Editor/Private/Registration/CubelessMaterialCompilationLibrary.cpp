#include "Registration/CubelessMaterialCompilationLibrary.h"
#include "Compatibility/UE58/CubelessMaterialCompilationApi.h"
FString UCubelessMaterialCompilationLibrary::CompileAndSaveMaterial(const FString& MaterialPath, const FString& GraphType, bool bSave)
{ return Cubeless::UE58::MaterialCompilation::CompileAndSave(MaterialPath, GraphType, bSave); }
