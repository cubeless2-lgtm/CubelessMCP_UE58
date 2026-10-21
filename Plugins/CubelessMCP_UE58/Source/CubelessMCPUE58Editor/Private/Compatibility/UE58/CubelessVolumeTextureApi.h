#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
class UCubelessVolumeTextureJob;
namespace Cubeless::UE58::VolumeTexture
{
bool Activate();
void Deactivate(const FString& Reason);
TSharedRef<FJsonObject> Status();
bool Cancel(UCubelessVolumeTextureJob* Job,const FString& Reason);
UCubelessVolumeTextureJob* BeginSheet(const FString& SourcePath,const FString& TargetPath,int64 TileX,int64 TileY,bool Dry,bool Overwrite,bool Save,bool SRGB,bool HasSRGB,const FString& Compression,const FString& Mips,bool NeverStream,bool Virtual,const FString& Address);
UCubelessVolumeTextureJob* BeginRaw(const FString& RawPath,const FString& SourcePath,const FString& TargetPath,int64 SheetX,int64 SheetY,int64 TileX,int64 TileY,bool Dry,bool Overwrite,bool Save,bool SRGB,const FString& SourceCompression,const FString& VolumeCompression,const FString& SourceMips,const FString& VolumeMips,bool NeverStream,bool Virtual,const FString& Address);
struct FJobState;
}
