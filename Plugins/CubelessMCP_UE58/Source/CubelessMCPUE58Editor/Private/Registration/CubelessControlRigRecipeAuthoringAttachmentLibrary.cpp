#include "CubelessControlRigRecipeAuthoringAttachmentLibrary.h"
#include "Registration/CubelessControlRigRecipeAuthoringToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FControlRigRecipeAuthoringAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ControlRigRecipeAuthoringAttachmentReceipt(UClass* ToolsetClass, FControlRigRecipeAuthoringAttachment Function, const TCHAR* Operation)
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
FString UCubelessControlRigRecipeAuthoringAttachmentLibrary::InstallControlRigRecipeAuthoringToolset(UClass* ToolsetClass)
{
    return ControlRigRecipeAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallControlRigRecipeAuthoringToolset, TEXT("install_controlrig_recipe_authoring_toolset"));
}
FString UCubelessControlRigRecipeAuthoringAttachmentLibrary::VerifyControlRigRecipeAuthoringToolset(UClass* ToolsetClass)
{
    return ControlRigRecipeAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyControlRigRecipeAuthoringToolset, TEXT("verify_controlrig_recipe_authoring_toolset"));
}
FString UCubelessControlRigRecipeAuthoringAttachmentLibrary::RemoveControlRigRecipeAuthoringToolset(UClass* ToolsetClass)
{
    return ControlRigRecipeAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveControlRigRecipeAuthoringToolset, TEXT("remove_controlrig_recipe_authoring_toolset"));
}
