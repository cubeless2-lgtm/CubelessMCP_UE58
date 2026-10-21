#include "CubelessControlRigRecipePlanningAttachmentLibrary.h"
#include "Registration/CubelessControlRigRecipePlanningToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FControlRigRecipePlanningAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ControlRigRecipePlanningAttachmentReceipt(UClass* ToolsetClass, FControlRigRecipePlanningAttachment Function, const TCHAR* Operation)
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
FString UCubelessControlRigRecipePlanningAttachmentLibrary::InstallControlRigRecipePlanningToolset(UClass* ToolsetClass)
{
    return ControlRigRecipePlanningAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallControlRigRecipePlanningToolset, TEXT("install_controlrig_recipe_planning_toolset"));
}
FString UCubelessControlRigRecipePlanningAttachmentLibrary::VerifyControlRigRecipePlanningToolset(UClass* ToolsetClass)
{
    return ControlRigRecipePlanningAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyControlRigRecipePlanningToolset, TEXT("verify_controlrig_recipe_planning_toolset"));
}
FString UCubelessControlRigRecipePlanningAttachmentLibrary::RemoveControlRigRecipePlanningToolset(UClass* ToolsetClass)
{
    return ControlRigRecipePlanningAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveControlRigRecipePlanningToolset, TEXT("remove_controlrig_recipe_planning_toolset"));
}
