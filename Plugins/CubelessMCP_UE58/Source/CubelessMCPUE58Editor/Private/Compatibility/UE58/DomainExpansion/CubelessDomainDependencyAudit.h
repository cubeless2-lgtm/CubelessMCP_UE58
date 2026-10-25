#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::DomainExpansion
{
enum class EDomainAuditMode : uint8 { InMemoryOnly, RequirePersisted };
// Full serialized/live and persisted dependency proof; no registry-only fallback.
struct FDomainDependencyAudit
{
    static bool IsMCPDependencyReference(const FString& Value);
    static void CollectMCPDependencyAuditIssues(const FString& FieldName,const UObject* Object,EDomainAuditMode Mode,TArray<FString>& Issues);
    static bool ValidateMCPDependencyIndependence(const FString& FieldName,const UObject* Object,EDomainAuditMode Mode,FString& Error);
};
}
