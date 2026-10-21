#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGContractAuthoringLibrary.generated.h"

// Editor-only Engine API carriers. Public ReadPCGNodeContract is Python composition of Audit.
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessPCGContractAuthoringLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|UE58|PCG", meta=(AIIgnore, ScriptName="audit_pcg_graph_contract"))
    static FString AuditPCGGraphContract(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|UE58|PCG", meta=(AIIgnore, ScriptName="set_pcg_static_mesh_spawner_entries"))
    static FString SetPCGStaticMeshSpawnerEntries(const FString& ParamsJson);
};
