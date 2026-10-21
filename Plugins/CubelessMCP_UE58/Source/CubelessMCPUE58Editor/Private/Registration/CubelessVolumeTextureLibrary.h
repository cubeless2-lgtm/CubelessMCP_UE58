#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Compatibility/UE58/CubelessVolumeTextureJob.h"
#include "CubelessVolumeTextureLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessVolumeTextureLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Texture",meta=(AIIgnore,ScriptName="create_volume_texture_from_2d_sheet_mcp"))
    static UCubelessVolumeTextureJob* CreateVolumeTextureFrom2DSheetMCP(const FString& SourceTexturePath,const FString& TargetVolumeTexturePath,int64 TileSizeX,int64 TileSizeY,bool bDryRun,bool bOverwriteExisting,bool bSaveAsset,bool bSRGB,bool bHasSRGB,const FString& CompressionSettings,const FString& MipGenSettings,bool bNeverStream,bool bVirtualTextureStreaming,const FString& AddressMode);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Texture",meta=(AIIgnore,ScriptName="create_volume_texture_from_raw_rgba16_sheet_mcp"))
    static UCubelessVolumeTextureJob* CreateVolumeTextureFromRawRGBA16SheetMCP(const FString& RawFilePath,const FString& SourceTexturePath,const FString& TargetVolumeTexturePath,int64 SheetSizeX,int64 SheetSizeY,int64 TileSizeX,int64 TileSizeY,bool bDryRun,bool bOverwriteExisting,bool bSaveAssets,bool bSRGB,const FString& SourceCompressionSettings,const FString& VolumeCompressionSettings,const FString& SourceMipGenSettings,const FString& VolumeMipGenSettings,bool bNeverStreamVolume,bool bVirtualTextureStreaming,const FString& AddressMode);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Texture",meta=(AIIgnore,ScriptName="verify_volume_texture_toolset_defaults"))
    static FString VerifyVolumeTextureToolsetDefaults(UClass* Class);
    UFUNCTION(BlueprintPure,Category="Cubeless|Texture",meta=(AIIgnore,ScriptName="volume_texture_jobs_status"))
    static FString VolumeTextureJobsStatus();
    UFUNCTION(BlueprintCallable,Category="Cubeless|Texture",meta=(AIIgnore,ScriptName="activate_volume_texture_jobs"))
    static FString ActivateVolumeTextureJobs();
    UFUNCTION(BlueprintCallable,Category="Cubeless|Texture",meta=(AIIgnore,ScriptName="deactivate_volume_texture_jobs"))
    static FString DeactivateVolumeTextureJobs(const FString& Reason);
};
