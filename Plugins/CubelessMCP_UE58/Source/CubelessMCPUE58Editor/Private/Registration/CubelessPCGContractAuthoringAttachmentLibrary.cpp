#include "CubelessPCGContractAuthoringAttachmentLibrary.h"
#include "Registration/CubelessPCGContractAuthoringToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FPCGContractAuthoringAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString PCGContractAuthoringAttachmentReceipt(UClass* ToolsetClass, FPCGContractAuthoringAttachment Function, const TCHAR* Operation)
{
    auto Report = MakeShared<FJsonObject>(); FString Error;
    const bool Ok = Function(ToolsetClass, Report, Error);
    Report->SetStringField(TEXT("operation"), Operation);
    // Admission-only Verify deliberately returns false; do not promote it to general readiness.
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok);
    Report->SetStringField(TEXT("error"), Error);
    Report->SetBoolField(TEXT("asset_mutation"), false); Report->SetBoolField(TEXT("metadata_mutation"), false);
    Report->SetBoolField(TEXT("actual_named_wire_verified"), false);
    FString Out; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
FString UCubelessPCGContractAuthoringAttachmentLibrary::InstallPCGContractAuthoringToolset(UClass* ToolsetClass)
{
    return PCGContractAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallPCGContractAuthoringToolset, TEXT("install_pcg_contract_authoring_toolset"));
}
FString UCubelessPCGContractAuthoringAttachmentLibrary::VerifyPCGContractAuthoringToolset(UClass* ToolsetClass)
{
    return PCGContractAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyPCGContractAuthoringToolset, TEXT("verify_pcg_contract_authoring_toolset"));
}
FString UCubelessPCGContractAuthoringAttachmentLibrary::RemovePCGContractAuthoringToolset(UClass* ToolsetClass)
{
    return PCGContractAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemovePCGContractAuthoringToolset, TEXT("remove_pcg_contract_authoring_toolset"));
}
