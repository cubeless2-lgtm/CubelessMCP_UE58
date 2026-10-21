#include "CubelessNiagaraScratchStackAuthoringAttachmentLibrary.h"
#include "Registration/CubelessNiagaraScratchStackAuthoringToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraScratchStackAuthoringAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraScratchStackAuthoringAttachmentReceipt(UClass* ToolsetClass, FNiagaraScratchStackAuthoringAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraScratchStackAuthoringAttachmentLibrary::InstallNiagaraScratchStackAuthoringToolset(UClass* ToolsetClass)
{
    return NiagaraScratchStackAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraScratchStackAuthoringToolset, TEXT("install_niagara_scratch_stack_authoring_toolset"));
}
FString UCubelessNiagaraScratchStackAuthoringAttachmentLibrary::VerifyNiagaraScratchStackAuthoringToolset(UClass* ToolsetClass)
{
    return NiagaraScratchStackAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraScratchStackAuthoringToolset, TEXT("verify_niagara_scratch_stack_authoring_toolset"));
}
FString UCubelessNiagaraScratchStackAuthoringAttachmentLibrary::RemoveNiagaraScratchStackAuthoringToolset(UClass* ToolsetClass)
{
    return NiagaraScratchStackAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraScratchStackAuthoringToolset, TEXT("remove_niagara_scratch_stack_authoring_toolset"));
}
