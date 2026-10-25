#pragma once
#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "CubelessDomainExpansionLibrary.generated.h"

/** Editor-only primitive binding; public MCP tools each expose their own contract. */
UCLASS()
class UCubelessDomainExpansionLibrary : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, DevelopmentOnly, ScriptName="execute_domain_request"))
    static FString ExecuteDomainRequest(const FString& RequestJson);
};
