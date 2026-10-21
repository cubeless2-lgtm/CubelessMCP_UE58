#include "Registration/CubelessChaosCacheSessionAttachmentLibrary.h"
#include "Registration/CubelessChaosCacheSessionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosCacheSessionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosCacheSessionAttachmentReceipt(UClass* ToolsetClass, FChaosCacheSessionAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosCacheSessionAttachmentLibrary::InstallChaosCacheSessionToolset(UClass* ToolsetClass)
{
    return ChaosCacheSessionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosCacheSessionToolset, TEXT("install_chaos_cache_sessions_toolset"));
}
FString UCubelessChaosCacheSessionAttachmentLibrary::VerifyChaosCacheSessionToolset(UClass* ToolsetClass)
{
    return ChaosCacheSessionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosCacheSessionToolset, TEXT("verify_chaos_cache_sessions_toolset"));
}
FString UCubelessChaosCacheSessionAttachmentLibrary::RemoveChaosCacheSessionToolset(UClass* ToolsetClass)
{
    return ChaosCacheSessionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosCacheSessionToolset, TEXT("remove_chaos_cache_sessions_toolset"));
}
