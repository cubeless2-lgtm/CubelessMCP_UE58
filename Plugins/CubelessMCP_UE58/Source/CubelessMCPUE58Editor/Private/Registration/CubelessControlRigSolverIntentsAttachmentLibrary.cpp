#include "Registration/CubelessControlRigSolverIntentsAttachmentLibrary.h"
#include "Registration/CubelessControlRigSolverIntentsToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FControlRigSolverIntentsAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ControlRigSolverIntentsAttachmentReceipt(UClass* ToolsetClass, FControlRigSolverIntentsAttachment Function, const TCHAR* Operation)
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
FString UCubelessControlRigSolverIntentsAttachmentLibrary::InstallControlRigSolverIntentsToolset(UClass* ToolsetClass)
{
    return ControlRigSolverIntentsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallControlRigSolverIntentsToolset, TEXT("install_controlrig_solver_intents_toolset"));
}
FString UCubelessControlRigSolverIntentsAttachmentLibrary::VerifyControlRigSolverIntentsToolset(UClass* ToolsetClass)
{
    return ControlRigSolverIntentsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyControlRigSolverIntentsToolset, TEXT("verify_controlrig_solver_intents_toolset"));
}
FString UCubelessControlRigSolverIntentsAttachmentLibrary::RemoveControlRigSolverIntentsToolset(UClass* ToolsetClass)
{
    return ControlRigSolverIntentsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveControlRigSolverIntentsToolset, TEXT("remove_controlrig_solver_intents_toolset"));
}
