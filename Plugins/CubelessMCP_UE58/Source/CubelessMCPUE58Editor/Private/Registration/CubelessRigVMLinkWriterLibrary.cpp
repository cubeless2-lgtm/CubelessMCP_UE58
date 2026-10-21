#include "Registration/CubelessRigVMLinkWriterLibrary.h"
#include "Compatibility/UE58/CubelessRigVMLinkWriterApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessRigVMLinkWriterLibrary::AddControlRigRigVMLinks(const FString& ControlRigPath, const FString& ControlRigAssetPath, const FString& ControlRigClassPath,
    const FString& LinksJson, const FString& UnitLinksJson, bool bAllowTypeCasting, bool bDryRun,
    bool bCompile, bool bSaveAsset, int64 MaxLinks)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::RigVMLinkWriter::Add(
        ControlRigPath,ControlRigAssetPath,ControlRigClassPath,LinksJson,UnitLinksJson,bAllowTypeCasting,bDryRun,bCompile,bSaveAsset,MaxLinks),
        TEXT("add_controlrig_rigvm_links"));
}
