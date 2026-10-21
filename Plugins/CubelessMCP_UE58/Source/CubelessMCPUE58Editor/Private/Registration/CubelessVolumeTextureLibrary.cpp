#include "Registration/CubelessVolumeTextureLibrary.h"
#include "Compatibility/UE58/CubelessVolumeTextureApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Registration/CubelessJsonConverters.h"
#include "Serialization/JsonSerializer.h"
UCubelessVolumeTextureJob* UCubelessVolumeTextureLibrary::CreateVolumeTextureFrom2DSheetMCP(const FString& S,const FString& V,int64 X,int64 Y,bool D,bool O,bool Save,bool SRGB,bool HasSRGB,const FString& C,const FString& M,bool N,bool VT,const FString& A)
{ return Cubeless::UE58::VolumeTexture::BeginSheet(S,V,X,Y,D,O,Save,SRGB,HasSRGB,C,M,N,VT,A); }
UCubelessVolumeTextureJob* UCubelessVolumeTextureLibrary::CreateVolumeTextureFromRawRGBA16SheetMCP(const FString& Raw,const FString& S,const FString& V,int64 SX,int64 SY,int64 TX,int64 TY,bool D,bool O,bool Save,bool SRGB,const FString& SC,const FString& VC,const FString& SM,const FString& VM,bool N,bool VT,const FString& A)
{ return Cubeless::UE58::VolumeTexture::BeginRaw(Raw,S,V,SX,SY,TX,TY,D,O,Save,SRGB,SC,VC,SM,VM,N,VT,A); }
FString UCubelessVolumeTextureLibrary::VerifyVolumeTextureToolsetDefaults(UClass* Class)
{
    using Cubeless::UE58::Variable::FResult;FResult R;R.Stage=TEXT("volume_texture_schema_verification");auto& J=R.Data;J->SetStringField(TEXT("operation"),TEXT("verify_volume_texture_toolset_defaults"));J->SetStringField(TEXT("class_path"),IsValid(Class)?Class->GetPathName():FString());
    for(const TCHAR* K:{TEXT("default_verified"),TEXT("exact_converter_active"),TEXT("nullable_schema_verified"),TEXT("actual_call_packing_verified"),TEXT("metadata_mutation_attempted"),TEXT("metadata_changed")}) { J->SetBoolField(K,false); }
    for(const TCHAR* K:{TEXT("verified_property_count"),TEXT("metadata_write_attempt_count"),TEXT("metadata_changed_count"),TEXT("helper_attempt_count"),TEXT("helper_return_count")}) { J->SetNumberField(K,0); }
    J->SetObjectField(TEXT("defaults"),MakeShared<FJsonObject>());J->SetArrayField(TEXT("property_observations"),TArray<TSharedPtr<FJsonValue>>{});
    if(!Cubeless::UE58::IsEditorGameThread()||!IsValid(Class)||Class->GetPathName()!=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/volume_texture_toolset_PY.CubelessVolumeTextureTools")) { R.ErrorCode=TEXT("volume_texture_schema_class_invalid");R.Error=TEXT("Exact unregistered Editor VolumeTextureTools class required"); }
    else { J->SetNumberField(TEXT("helper_attempt_count"),1);R.bSuccess=Cubeless::Registration::VerifyVolumeTextureToolset(Class,J,R.Error);J->SetNumberField(TEXT("helper_return_count"),1);R.bPostconditionPassed=R.bSuccess;if(!R.bSuccess) { R.ErrorCode=TEXT("volume_texture_schema_verification_failed"); } }
    return Cubeless::Operations::EncodeVariableResult(R,TEXT("verify_volume_texture_toolset_defaults"));
}
FString UCubelessVolumeTextureLibrary::VolumeTextureJobsStatus() { FString S;FJsonSerializer::Serialize(Cubeless::UE58::VolumeTexture::Status(),TJsonWriterFactory<>::Create(&S));return S; }
FString UCubelessVolumeTextureLibrary::ActivateVolumeTextureJobs() { Cubeless::UE58::VolumeTexture::Activate();return VolumeTextureJobsStatus(); }
FString UCubelessVolumeTextureLibrary::DeactivateVolumeTextureJobs(const FString& Reason) { Cubeless::UE58::VolumeTexture::Deactivate(Reason);return VolumeTextureJobsStatus(); }
