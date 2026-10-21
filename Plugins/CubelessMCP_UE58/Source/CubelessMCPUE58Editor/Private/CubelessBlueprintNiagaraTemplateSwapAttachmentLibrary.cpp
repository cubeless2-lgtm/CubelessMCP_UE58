#include "CubelessBlueprintNiagaraTemplateSwapAttachmentLibrary.h"
#include "Registration/CubelessBlueprintNiagaraTemplateSwapToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FBlueprintNiagaraTemplateSwapAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString BlueprintNiagaraTemplateSwapAttachmentReceipt(UClass* ToolsetClass, FBlueprintNiagaraTemplateSwapAttachment Function, const TCHAR* Operation)
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
FString UCubelessBlueprintNiagaraTemplateSwapAttachmentLibrary::InstallBlueprintNiagaraTemplateSwapToolset(UClass* ToolsetClass)
{
    return BlueprintNiagaraTemplateSwapAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallBlueprintNiagaraTemplateSwapToolset, TEXT("install_blueprint_niagara_template_swap_toolset"));
}
FString UCubelessBlueprintNiagaraTemplateSwapAttachmentLibrary::VerifyBlueprintNiagaraTemplateSwapToolset(UClass* ToolsetClass)
{
    return BlueprintNiagaraTemplateSwapAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyBlueprintNiagaraTemplateSwapToolset, TEXT("verify_blueprint_niagara_template_swap_toolset"));
}
FString UCubelessBlueprintNiagaraTemplateSwapAttachmentLibrary::RemoveBlueprintNiagaraTemplateSwapToolset(UClass* ToolsetClass)
{
    return BlueprintNiagaraTemplateSwapAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveBlueprintNiagaraTemplateSwapToolset, TEXT("remove_blueprint_niagara_template_swap_toolset"));
}
