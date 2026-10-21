#include "Compatibility/UE58/CubelessVolumeTextureApi.h"
#include "Compatibility/UE58/CubelessVolumeTextureJob.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/VolumeTexture.h"
#include "Engine/World.h"
#include "TextureCompiler.h"
#include "ObjectCacheContext.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::VolumeTexture
{
namespace
{
constexpr int64 MaxBytes = 64LL * 1024 * 1024;
constexpr int32 MaxCompositeTextures = 64;
constexpr int32 MaxVolumeDepth = 512; // Installed VolumeTexture.cpp SDK bound, not a private symbol access.
constexpr double DeadlineSeconds = 60.0;
using FResult = Variable::FResult;
struct FRequest
{
    bool Raw=false, Dry=false, Overwrite=false, Save=true, SRGB=false, HasSRGB=true, Never=true, Virtual=false;
    FString RawPath, SourcePath, VolumePath, SourceCompression, VolumeCompression, SourceMips, VolumeMips, Address;
    int64 SX=0, SY=0, TX=128, TY=128;
};
bool Active=false;
TSharedPtr<FJobState> Pending;
TArray<TSharedPtr<FJobState>> Recovery;
bool Loaded(const UObject* O)
{ return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed); }
bool Text(const FString& S,int32 Max=131072)
{
    if(S.Len()>Max) { return false; }
    for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }
    return true;
}
bool Positive32(int64 V) { return V>0&&V<=MAX_int32; }
bool StockTextureClass(UTexture* T)
{ return Loaded(T)&&Loaded(T->GetClass())&&T->GetClass()->IsNative()&&T->GetClass()->GetOutermost()->GetName()==TEXT("/Script/Engine"); }
bool SourceFormat(const FTextureSource& S)
{ const auto F=S.GetFormat();return F>TSF_Invalid&&F<TSF_MAX&&F!=TSF_RGBA8_DEPRECATED&&F!=TSF_RGBE8_DEPRECATED; }
FString DefaultString(const FString& Value,const TCHAR* Default)
{ return Value.IsEmpty()?FString(Default):Value; }
// Original package normalization, enum parsing and direct SavePackage helpers are inserted below.
FString NormalizeContentRoot(FString Path)
{
    Path.TrimStartAndEndInline();
    while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
    {
        Path.LeftChopInline(1);
    }
    return Path;
}

FString NormalizePackagePathParam(FString Path)
{
    Path.TrimStartAndEndInline();
    Path.TrimQuotesInline();

    FString LongPackageName;
    if (FPackageName::TryConvertFilenameToLongPackageName(Path, LongPackageName))
    {
        return NormalizeContentRoot(LongPackageName);
    }

    if (Path.Contains(TEXT(".")))
    {
        Path = FPackageName::ObjectPathToPackageName(Path);
    }
    return NormalizeContentRoot(Path);
}

bool SaveLoadedAssetForMCP(UObject* Asset)
{
    if (!IsValid(Asset))
    {
        return false;
    }

    UPackage* Package = Asset->GetOutermost();
    if (!Package || Package == GetTransientPackage())
    {
        return false;
    }

    const FString PackageName = Package->GetName();
    if (!FPackageName::IsValidLongPackageName(PackageName, false))
    {
        return false;
    }

    UObject* TopLevelAsset = Asset;
    FString PackageExtension = FPackageName::GetAssetPackageExtension();
    if (UWorld* World = Cast<UWorld>(Asset))
    {
        TopLevelAsset = World;
        PackageExtension = FPackageName::GetMapPackageExtension();
    }
    else if (UWorld* WorldInPackage = UWorld::FindWorldInPackage(Package))
    {
        TopLevelAsset = WorldInPackage;
        PackageExtension = FPackageName::GetMapPackageExtension();
    }

    FString PackageFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilename, PackageExtension))
    {
        return false;
    }

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    SaveArgs.bSlowTask = false;

    const bool bSaved = UPackage::SavePackage(Package, TopLevelAsset, *PackageFilename, SaveArgs);
    if (bSaved)
    {
        Package->SetDirtyFlag(false);
    }
    return bSaved;
}

bool TryParseTextureCompressionSettingsForMCP(const FString& RawValue, TextureCompressionSettings& OutValue, FString& OutError)
{
    FString Value = RawValue;
    Value.TrimStartAndEndInline();

    if (Value.IsEmpty() || Value.Equals(TEXT("TC_VectorDisplacementmap"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TC_VECTOR_DISPLACEMENTMAP"), ESearchCase::IgnoreCase))
    {
        OutValue = TC_VectorDisplacementmap;
        return true;
    }
    if (Value.Equals(TEXT("TC_Default"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TC_DEFAULT"), ESearchCase::IgnoreCase))
    {
        OutValue = TC_Default;
        return true;
    }
    if (Value.Equals(TEXT("TC_HDR"), ESearchCase::IgnoreCase))
    {
        OutValue = TC_HDR;
        return true;
    }
    if (Value.Equals(TEXT("TC_Masks"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TC_MASKS"), ESearchCase::IgnoreCase))
    {
        OutValue = TC_Masks;
        return true;
    }
    if (Value.Equals(TEXT("TC_Normalmap"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TC_NORMALMAP"), ESearchCase::IgnoreCase))
    {
        OutValue = TC_Normalmap;
        return true;
    }

    OutError = FString::Printf(TEXT("Unsupported compression_settings: %s"), *RawValue);
    return false;
}

bool TryParseTextureMipGenSettingsForMCP(const FString& RawValue, TextureMipGenSettings& OutValue, FString& OutError)
{
    FString Value = RawValue;
    Value.TrimStartAndEndInline();

    if (Value.IsEmpty() || Value.Equals(TEXT("TMGS_NoMipmaps"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TMGS_NOMIPMAPS"), ESearchCase::IgnoreCase))
    {
        OutValue = TMGS_NoMipmaps;
        return true;
    }
    if (Value.Equals(TEXT("TMGS_FromTextureGroup"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TMGS_FROMTEXTUREGROUP"), ESearchCase::IgnoreCase))
    {
        OutValue = TMGS_FromTextureGroup;
        return true;
    }

    OutError = FString::Printf(TEXT("Unsupported mip_gen_settings: %s"), *RawValue);
    return false;
}

bool TryParseTextureAddressForMCP(const FString& RawValue, TextureAddress& OutValue, FString& OutError)
{
    FString Value = RawValue;
    Value.TrimStartAndEndInline();

    if (Value.IsEmpty() || Value.Equals(TEXT("TA_Wrap"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TA_WRAP"), ESearchCase::IgnoreCase))
    {
        OutValue = TA_Wrap;
        return true;
    }
    if (Value.Equals(TEXT("TA_Clamp"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TA_CLAMP"), ESearchCase::IgnoreCase))
    {
        OutValue = TA_Clamp;
        return true;
    }
    if (Value.Equals(TEXT("TA_Mirror"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TA_MIRROR"), ESearchCase::IgnoreCase))
    {
        OutValue = TA_Mirror;
        return true;
    }

    OutError = FString::Printf(TEXT("Unsupported address_mode: %s"), *RawValue);
    return false;
}
bool PackagePath(const FString& Raw,FString& Out,bool Game)
{
    if(!Text(Raw,NAME_SIZE-1)||Raw.TrimStartAndEnd().IsEmpty()) { return false; }
    Out=NormalizePackagePathParam(Raw);
    const FString ObjectPath=Out+TEXT(".")+FPackageName::GetLongPackageAssetName(Out);
    return Out.Len()<NAME_SIZE&&ObjectPath.Len()<NAME_SIZE&&FPackageName::IsValidLongPackageName(Out)&&(!Game||Out.StartsWith(TEXT("/Game/")));
}
FString ObjectPath(const FString& Package)
{ return Package+TEXT(".")+FPackageName::GetLongPackageAssetName(Package); }
}

struct FJobState : TSharedFromThis<FJobState>
{
    enum class EPhase : uint8 { Preflight, DryCompletion, SourceWrite, SourceUpdate, VolumeWrite, VolumeUpdate, Completion, SourceSave, VolumeSave, Verify, Terminal };
    FRequest Q;
    FResult R;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();
    TStrongObjectPtr<UCubelessVolumeTextureJob> Job;
    TStrongObjectPtr<UTexture2D> Source;
    TStrongObjectPtr<UVolumeTexture> Volume;
    FTextureCompilingManager* Manager=nullptr;
    FTSTicker::FDelegateHandle Ticker;
    FDelegateHandle PostCompile;
    TArray<uint8> Bytes;
    EPhase Phase=EPhase::Preflight;
    double Started=FPlatformTime::Seconds();
    uint64 CreatedFrame=GFrameCounter;
    uint64 LastTickFrame=CreatedFrame;
    int64 Revision=0, SourceBytes=0, PixelBytes=0;
    int32 TilesX=0,TilesY=0,Slices=0,Depth=0;
    TextureCompressionSettings SC=TC_Default,VC=TC_VectorDisplacementmap;
    TextureMipGenSettings SM=TMGS_FromTextureGroup,VM=TMGS_NoMipmaps;
    TextureAddress Address=TA_Wrap;
    FGuid SheetSourceId;
    bool SourceExists=false,VolumeExists=false,CreatedSource=false,CreatedVolume=false;
    bool Updated=false,SavedSource=false,SavedVolume=false,SourceWritten=false,VolumeWritten=false;
    bool HasReadiness=false,LastReady=false,EventSignal=false,PostCompileObserved=false;
    TMap<TWeakObjectPtr<UTexture>,bool> LastTextureReady;

    explicit FJobState(FRequest&& Request):Q(MoveTemp(Request)),Job(NewObject<UCubelessVolumeTextureJob>())
    {
        R.bSaveRequested=Q.Save;R.bCompileRequested=!Q.Dry;
        R.Data->SetStringField(TEXT("operation"),Operation());
        R.Data->SetStringField(TEXT("native_job_id"),FGuid::NewGuid().ToString(EGuidFormats::Digits));
        R.Data->SetBoolField(TEXT("dry_run"),Q.Dry);
        R.Data->SetBoolField(TEXT("source_mutation_requested"),Q.Raw&&!Q.Dry);
        R.Data->SetStringField(TEXT("compile_scope"),TEXT("Editor texture cache/registered build/default-resource readiness; no shader-error, GPU, cook or cold-load proof"));
        R.Data->SetBoolField(TEXT("explicit_blocking_compile_wait_called"),false);
        R.Data->SetBoolField(TEXT("texture_compiler_error_status_known"),false);
        R.Data->SetBoolField(TEXT("async_texture_compilation_required"),true);
        R.Data->SetStringField(TEXT("change_scope"),TEXT("Direct owned texture/package operations; SDK loading, dependent composite/cache/render notifications remain transitive effects, not an all-object no-effect or rollback proof"));
        R.Data->SetStringField(TEXT("settings_scope"),TEXT("Requested fields are written once; stock ValidateSettingsAfterImportOrEdit may normalize them. Current values are reported separately and are not silently restated as requested values."));
        R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);
        R.Data->SetBoolField(TEXT("auto_retry_safe"),false);
        for(const TCHAR* K:{TEXT("job_start"),TEXT("ticker_add"),TEXT("ticker_remove"),TEXT("post_compile_add"),TEXT("post_compile_remove"),
            TEXT("source_load_attempt"),TEXT("source_load_return"),TEXT("volume_load_attempt"),TEXT("volume_load_return"),TEXT("raw_open_attempt"),TEXT("raw_read_attempt"),TEXT("raw_read_return"),
            TEXT("package_create_attempt"),TEXT("package_create_return"),TEXT("texture2d_create_attempt"),TEXT("texture2d_create_return"),TEXT("volume_create_attempt"),TEXT("volume_create_return"),TEXT("asset_created_calls"),
            TEXT("source_modify_calls"),TEXT("source_pre_edit_calls"),TEXT("source_init_calls"),TEXT("source_property_write_count"),TEXT("source_post_edit_calls"),TEXT("source_update_resource_calls"),TEXT("source_mark_dirty_calls"),
            TEXT("volume_modify_calls"),TEXT("volume_pre_edit_calls"),TEXT("volume_property_write_count"),TEXT("volume_update_source_attempt"),TEXT("volume_update_source_return"),TEXT("volume_post_edit_calls"),TEXT("volume_update_resource_calls"),TEXT("volume_mark_dirty_calls"),
            TEXT("source_save_attempt"),TEXT("source_save_return"),TEXT("volume_save_attempt"),TEXT("volume_save_return"),TEXT("explicit_finish_compilation_calls"),TEXT("explicit_block_on_build_calls"),TEXT("cancel_calls")})
        { Counts->SetNumberField(K,0); }
        R.Data->SetObjectField(TEXT("native_call_counts"),Counts);
        R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
        Bump(TEXT("job_start"));
    }
    const TCHAR* Operation() const
    { return Q.Raw?TEXT("create_volume_texture_from_raw_rgba16_sheet_mcp"):TEXT("create_volume_texture_from_2d_sheet_mcp"); }
    void Bump(const TCHAR* K,int32 N=1)
    { double V=0;Counts->TryGetNumberField(K,V);Counts->SetNumberField(K,V+N); }
    void Publish()
    {
        ++Revision;
        R.Data->SetNumberField(TEXT("native_checkpoint_revision"),Revision);
        R.Data->SetBoolField(TEXT("native_job_terminal"),Phase==EPhase::Terminal);
        R.Data->SetBoolField(TEXT("actual_direct_changed_prefix"),R.bChanged);
        R.Data->SetBoolField(TEXT("source_write_returned"),SourceWritten);
        R.Data->SetBoolField(TEXT("volume_write_returned"),VolumeWritten);
        R.Data->SetBoolField(TEXT("saved_source_texture"),SavedSource);
        R.Data->SetBoolField(TEXT("saved_volume_texture"),SavedVolume);
        R.Data->SetBoolField(TEXT("updated_from_source"),Updated);
        R.Data->SetBoolField(TEXT("created_source_texture"),CreatedSource);
        R.Data->SetBoolField(TEXT("created_volume_texture"),CreatedVolume);
        R.Data->SetBoolField(TEXT("post_compile_signal_observed"),PostCompileObserved);
        R.Data->SetBoolField(TEXT("owned_texture_readiness_observed"),HasReadiness);
        if(HasReadiness) { R.Data->SetBoolField(TEXT("owned_textures_ready"),LastReady); }
        else { R.Data->SetField(TEXT("owned_textures_ready"),MakeShared<FJsonValueNull>()); }
        if(Job.IsValid()) { Job->SnapshotJson=Cubeless::Operations::EncodeVariableResult(R,Operation());Job->bIsTerminal=Phase==EPhase::Terminal; }
    }
    void Next(EPhase P,const TCHAR* Stage)
    { Phase=P;R.Stage=Stage;HasReadiness=false;LastTextureReady.Reset();Publish(); }
    bool ReadyTexture(UTexture* T)
    { return Loaded(T)&&!Manager->IsCompilingTexture(T)&&T->IsAsyncCacheComplete()&&!T->IsDefaultTexture()&&!T->HasPendingInitOrStreaming(); }
    bool RequireAsync(UTexture* T=nullptr)
    {
        const bool Allowed=Manager&&Manager->IsAsyncTextureCompilationEnabled()&&(!T||Manager->IsAsyncCompilationAllowed(T));
        R.Data->SetBoolField(TEXT("last_async_compilation_enabled"),Allowed);
        if(!Allowed) { Fail(TEXT("texture_async_compilation_unavailable"),TEXT("Public texture manager disallows asynchronous builds; no synchronous build fallback or cvar change attempted"),R.bMutationAttempted); }
        return Allowed;
    }
    bool Readiness(UTexture* T)
    {
        if(!T) { return true; }
        if(!RequireAsync(T)) { return false; }
        bool Ready=ReadyTexture(T);int32 Related=0;bool TooMany=false,Cycle=false,AsyncDenied=false;
        TArray<UTexture*> Work;Work.Add(T);TMap<UTexture*,TArray<UTexture*>> Edges;
        // No allocation or dependency writes inside this SDK read scope.
        { FObjectCacheContextScope Cache;
          for(int32 I=0;I<Work.Num()&&!TooMany;++I)
          { UTexture* Current=Work[I];if(!Loaded(Current)) { Ready=false;continue; }
            for(UTexture* Other:Cache.GetContext().GetTexturesAffectedByTexture(Current))
            { Edges.FindOrAdd(Current).Add(Other);AsyncDenied=(Loaded(Other)&&!Manager->IsAsyncCompilationAllowed(Other))||AsyncDenied;Ready=ReadyTexture(Other)&&Ready;
              if(!Work.Contains(Other)) { if(Work.Num()>=MaxCompositeTextures+1) { TooMany=true;break; }Work.Add(Other); } }
          }
        }
        Related=Work.Num()-1;
        if(AsyncDenied) { Fail(TEXT("composite_texture_async_unavailable"),TEXT("Affected composite texture does not admit asynchronous build"),R.bMutationAttempted);return false; }
        TSet<UTexture*> Visiting,Done;
        TFunction<bool(UTexture*)> Visit=[&](UTexture* Current)
        { if(Visiting.Contains(Current)) { return false; }if(Done.Contains(Current)) { return true; }Visiting.Add(Current);
          if(const auto* Children=Edges.Find(Current)) { for(UTexture* Child:*Children) { if(!Visit(Child)) { return false; } } }
          Visiting.Remove(Current);Done.Add(Current);return true; };
        if(!TooMany) { Cycle=!Visit(T); }
        if(TooMany||Cycle) { Fail(TEXT("composite_texture_scope_unsafe"),TEXT("Affected composite closure exceeds64 textures or contains a cycle; no blocking edit attempted"),R.bMutationAttempted);return false; }
        const TWeakObjectPtr<UTexture> Key(T);const bool* Previous=LastTextureReady.Find(Key);
        if(!Previous||*Previous!=Ready||EventSignal)
        { LastTextureReady.Add(Key,Ready);EventSignal=false;
          const bool* A=Source.IsValid()?LastTextureReady.Find(TWeakObjectPtr<UTexture>(Source.Get())):nullptr;
          const bool* B=Volume.IsValid()?LastTextureReady.Find(TWeakObjectPtr<UTexture>(Volume.Get())):nullptr;
          HasReadiness=(!Source.IsValid()||A)&&(!Volume.IsValid()||B);LastReady=HasReadiness&&(!A||*A)&&(!B||*B);
          if(!Ready&&R.Compiled.IsSet()) { R.Compiled.Reset(); }
          R.Data->SetStringField(TEXT("last_readiness_texture"),T->GetPathName());
          R.Data->SetBoolField(TEXT("last_texture_ready"),Ready);R.Data->SetNumberField(TEXT("last_composite_texture_count"),Related);Publish(); }
        return Ready;
    }
    bool BothReady()
    { const bool A=Readiness(Source.Get());if(Phase==EPhase::Terminal) { return false; }const bool B=Readiness(Volume.Get());return A&&B; }
    void RemoveHandles()
    {
        if(Ticker.IsValid()) { FTSTicker::RemoveTicker(Ticker);Ticker.Reset();Bump(TEXT("ticker_remove")); }
        if(Manager&&PostCompile.IsValid()) { Manager->OnTexturePostCompileEvent().Remove(PostCompile);PostCompile.Reset();Bump(TEXT("post_compile_remove")); }
    }
    bool OwnedWorkPending()
    {
        if(!Source.IsValid()&&!Volume.IsValid()) { return false; }
        // No object-cache discovery during GC/exit. Borrowed loaded state is then conservatively uncertain.
        if(!Manager||IsGarbageCollecting()||IsEngineExitRequested()) { return true; }
        TArray<UTexture*> Work;if(Source.IsValid()) { Work.Add(Source.Get()); }if(Volume.IsValid()) { Work.AddUnique(Volume.Get()); }
        FObjectCacheContextScope Cache;
        for(int32 I=0;I<Work.Num();++I)
        {
            UTexture* T=Work[I];if(!ReadyTexture(T)) { return true; }
            for(UTexture* Other:Cache.GetContext().GetTexturesAffectedByTexture(T))
            { if(!Work.Contains(Other)) { if(Work.Num()>=MaxCompositeTextures+2) { return true; }Work.Add(Other); } }
        }
        return false;
    }
    void Terminal(bool Success,bool Proof,bool Unknown=false)
    {
        const bool PendingWork=OwnedWorkPending();Unknown=Unknown||PendingWork;
        R.Data->SetBoolField(TEXT("owned_engine_work_pending_at_terminal"),PendingWork);
        if(Success&&Unknown) { R.ErrorCode=TEXT("texture_terminal_work_pending");R.Error=TEXT("Owned loaded texture work remains pending; strong state retained without cancel or replay");R.Stage=TEXT("failed"); }
        RemoveHandles();Phase=EPhase::Terminal;R.bSuccess=Success&&!Unknown;R.bPostconditionPassed=Proof&&!Unknown;
        R.bChangeStateKnown=!Unknown;if(Unknown) { R.Compiled.Reset(); }R.Data->SetBoolField(TEXT("engine_work_may_continue"),Unknown);
        for(const TCHAR* K:{TEXT("source_texture_path"),TEXT("target_volume_texture_path"),TEXT("raw_file_path"),TEXT("raw_file_bytes"),
            TEXT("source_size_x"),TEXT("source_size_y"),TEXT("sheet_size_x"),TEXT("sheet_size_y"),TEXT("tile_size_x"),TEXT("tile_size_y"),
            TEXT("tile_count_x"),TEXT("tile_count_y"),TEXT("requested_slice_count"),TEXT("volume_size_x"),TEXT("volume_size_y"),TEXT("volume_size_z")})
        { if(auto Value=R.Data->TryGetField(K)) { Legacy->SetField(K,Value); } }
        if(!Q.Raw) { Legacy->SetBoolField(TEXT("srgb"),Q.SRGB);Legacy->SetBoolField(TEXT("never_stream"),Q.Never);Legacy->SetBoolField(TEXT("virtual_texture_streaming"),Q.Virtual); }
        Legacy->SetBoolField(TEXT("success"),Updated&&(!Q.Save||(SavedVolume&&(!Q.Raw||SavedSource))));
        Publish();
        if(Pending.Get()==this)
        { TSharedPtr<FJobState> Keep=Pending;Pending.Reset();if(Unknown) { Recovery.Add(MoveTemp(Keep)); } }
        Bytes.Reset();
    }
    void Fail(const TCHAR* Code,const FString& Error,bool Unknown=false)
    { R.ErrorCode=Code;R.Error=Error;R.Stage=TEXT("failed");Terminal(false,false,Unknown); }
    bool Cancel(const FString& Reason)
    {
        if(Phase==EPhase::Terminal) { return false; }
        Bump(TEXT("cancel_calls"));Fail(TEXT("volume_texture_job_cancelled"),Reason.Left(4096),R.bMutationAttempted);return true;
    }
    bool Dimensions(int64 SX,int64 SY,int64 BPP)
    {
        if(!Positive32(SX)||!Positive32(SY)||!Positive32(Q.TX)||!Positive32(Q.TY)||SX%Q.TX||SY%Q.TY||BPP<=0||BPP>16)
        { Fail(TEXT("texture_dimensions_invalid"),TEXT("Positive int32 divisible sheet/tile dimensions and supported bounded editor-source pixel size required"));return false; }
        // Divide before multiplying; reject before SDK int32 allocation expressions.
        if(SX>MaxBytes/BPP/SY) { Fail(TEXT("texture_source_memory_bound"),TEXT("Editor source exceeds64MiB bound"));return false; }
        SourceBytes=SX*SY*BPP;PixelBytes=BPP;
        const int64 Count=(SX/Q.TX)*(SY/Q.TY);
        if(Count<=0||Count>MAX_int32) { Fail(TEXT("texture_slice_count_invalid"),TEXT("Slice count exceeds checked int32 scope"));return false; }
        Q.SX=SX;Q.SY=SY;TilesX=static_cast<int32>(SX/Q.TX);TilesY=static_cast<int32>(SY/Q.TY);Slices=static_cast<int32>(Count);Depth=FMath::Min(Slices,MaxVolumeDepth);
        if(Q.TX>MaxBytes/BPP/Depth/Q.TY) { Fail(TEXT("volume_conversion_memory_bound"),TEXT("Volume conversion exceeds64MiB bound"));return false; }
        R.Data->SetNumberField(TEXT("source_size_x"),SX);R.Data->SetNumberField(TEXT("source_size_y"),SY);
        R.Data->SetNumberField(TEXT("sheet_size_x"),SX);R.Data->SetNumberField(TEXT("sheet_size_y"),SY);
        R.Data->SetNumberField(TEXT("tile_size_x"),Q.TX);R.Data->SetNumberField(TEXT("tile_size_y"),Q.TY);
        R.Data->SetNumberField(TEXT("tile_count_x"),TilesX);R.Data->SetNumberField(TEXT("tile_count_y"),TilesY);
        R.Data->SetNumberField(TEXT("requested_slice_count"),Slices);R.Data->SetNumberField(TEXT("sdk_clamped_slice_count"),Depth);
        return true;
    }
    bool Enums()
    {
        FString Error;
        Q.SourceCompression=DefaultString(Q.SourceCompression,TEXT("TC_Default"));Q.VolumeCompression=DefaultString(Q.VolumeCompression,TEXT("TC_VectorDisplacementmap"));
        Q.SourceMips=DefaultString(Q.SourceMips,TEXT("TMGS_FromTextureGroup"));Q.VolumeMips=DefaultString(Q.VolumeMips,TEXT("TMGS_NoMipmaps"));Q.Address=DefaultString(Q.Address,TEXT("TA_Wrap"));
        if((Q.Raw&&!TryParseTextureCompressionSettingsForMCP(Q.SourceCompression,SC,Error))||
            !TryParseTextureCompressionSettingsForMCP(Q.VolumeCompression,VC,Error)||
            (Q.Raw&&!TryParseTextureMipGenSettingsForMCP(Q.SourceMips,SM,Error))||
            !TryParseTextureMipGenSettingsForMCP(Q.VolumeMips,VM,Error)||!TryParseTextureAddressForMCP(Q.Address,Address,Error))
        { Fail(TEXT("texture_enum_invalid"),Error);return false; }
        return true;
    }
    bool SaveSettings(UTexture* T)
    {
        if(!Q.Save||!T) { return true; }
        if(!Loaded(T)||T->DeferCompression)
        { Fail(TEXT("texture_presave_restart_unsupported"),TEXT("Current DeferCompression would restart a texture build inside SavePackage; no settings clear or synchronous save fallback"),R.bMutationAttempted);return false; }
        if(UTexture2D* Texture2D=Cast<UTexture2D>(T))
        {
            // Read the actual reflected private bitfield through the public property API; never write it.
            const FBoolProperty* P=CastField<FBoolProperty>(UTexture2D::StaticClass()->FindPropertyByName(TEXT("bTemporarilyDisableStreaming")));
            if(!P||P->ArrayDim!=1||P->GetOwnerClass()!=UTexture2D::StaticClass())
            { Fail(TEXT("texture_presave_flag_unavailable"),TEXT("Exact reflected temporary-streaming boolean required for nonblocking SavePackage admission"),R.bMutationAttempted);return false; }
            if(P->GetPropertyValue_InContainer(Texture2D))
            { Fail(TEXT("texture_presave_restart_unsupported"),TEXT("Temporary streaming disable would restart and finish a build inside SavePackage; no private setting write"),R.bMutationAttempted);return false; }
        }
        return true;
    }
    bool Preflight()
    {
        if(!Text(Q.RawPath)||!Text(Q.SourcePath)||!Text(Q.VolumePath)||!Text(Q.SourceCompression)||!Text(Q.VolumeCompression)||!Text(Q.SourceMips)||!Text(Q.VolumeMips)||!Text(Q.Address))
        { Fail(TEXT("texture_input_invalid"),TEXT("Bounded strings without interiorNUL required"));return false; }
        if(Q.Raw)
        {
            Q.RawPath=Q.RawPath.TrimStartAndEnd();if(Q.RawPath.IsEmpty()) { Fail(TEXT("raw_file_missing"),TEXT("raw_file_path required"));return false; }
            Q.RawPath=FPaths::ConvertRelativePathToFull(Q.RawPath);
            if(!FPaths::FileExists(Q.RawPath)) { Fail(TEXT("raw_file_missing"),TEXT("raw_file_path does not exist"));return false; }
        }
        FString S,V;
        if(!PackagePath(Q.SourcePath,S,Q.Raw)||!PackagePath(Q.VolumePath,V,true)||S==V)
        { Fail(TEXT("texture_path_invalid"),TEXT("Distinct valid bounded source/volume package and object paths required; target and rawsource must be under/Game"));return false; }
        Q.SourcePath=S;Q.VolumePath=V;R.Data->SetStringField(TEXT("source_texture_path"),S);R.Data->SetStringField(TEXT("target_volume_texture_path"),V);
        if(!Positive32(Q.TX)||!Positive32(Q.TY)) { Fail(TEXT("texture_dimensions_invalid"),TEXT("tile dimensions must be positive int32"));return false; }
        if(Q.Raw)
        {
            if(!Dimensions(Q.SX,Q.SY,8)) { return false; }
            IPlatformFile& File=FPlatformFileManager::Get().GetPlatformFile();Bump(TEXT("raw_open_attempt"));
            TUniquePtr<IFileHandle> Handle(File.OpenRead(*Q.RawPath));
            if(!Handle||Handle->Size()!=SourceBytes) { Fail(TEXT("raw_file_size_invalid"),TEXT("Exact RGBA16 sheet byte count required within64MiB"));return false; }
            Bytes.SetNumUninitialized(static_cast<int32>(SourceBytes));Bump(TEXT("raw_read_attempt"));const bool Read=Handle->Read(Bytes.GetData(),SourceBytes);Bump(TEXT("raw_read_return"));
            if(!Read||Handle->Size()!=SourceBytes) { Fail(TEXT("raw_file_read_failed"),TEXT("Bounded exact raw read failed or size changed"));return false; }
            R.Data->SetStringField(TEXT("raw_file_path"),Q.RawPath);R.Data->SetNumberField(TEXT("raw_file_bytes"),Bytes.Num());
            if(!Enums()) { return false; }
        }
        Bump(TEXT("source_load_attempt"));UObject* SO=UEditorAssetLibrary::LoadAsset(S);Bump(TEXT("source_load_return"));Source.Reset(Cast<UTexture2D>(SO));SourceExists=SO!=nullptr;
        if(SO&&!StockTextureClass(Source.Get())) { Fail(TEXT("source_texture_wrong_type"),TEXT("Source must be a usable stock Engine-class Texture2D; no Editor/Cubeless class reference introduced"));return false; }
        if(!Q.Raw)
        {
            if(!Loaded(Source.Get())||!Source->Source.IsValid()) { Fail(TEXT("source_texture_invalid"),TEXT("Source Texture2D with valid editor-source data required"));return false; }
            // Stock UpdateSource reads a single first-layer 2D mip; unusual tiled/multilayer sources remain an explicit finite-admission gap.
            if(Source->Source.GetNumBlocks()!=1||Source->Source.GetNumLayers()!=1||Source->Source.GetNumSlices()!=1||!SourceFormat(Source->Source))
            { Fail(TEXT("source_layout_unsupported"),TEXT("Singleblock singlelayer single-slice Texture2D editor source required"));return false; }
            if(!Dimensions(Source->Source.GetSizeX(),Source->Source.GetSizeY(),Source->Source.GetBytesPerPixel())) { return false; }
            SheetSourceId=Source->Source.GetId();if(!Q.HasSRGB) { Q.SRGB=Source->SRGB; }
            if(!Enums()) { return false; }
        }
        Bump(TEXT("volume_load_attempt"));UObject* VO=UEditorAssetLibrary::LoadAsset(V);Bump(TEXT("volume_load_return"));Volume.Reset(Cast<UVolumeTexture>(VO));VolumeExists=VO!=nullptr;
        if(VO&&!StockTextureClass(Volume.Get())) { Fail(TEXT("volume_texture_wrong_type"),TEXT("Target must be a usable stock Engine-class VolumeTexture"));return false; }
        if((VolumeExists||(Q.Raw&&SourceExists))&&!Q.Overwrite)
        {
            Legacy->SetBoolField(TEXT("success"),Q.Dry);Legacy->SetBoolField(TEXT("can_create"),false);Legacy->SetBoolField(TEXT("dry_run"),Q.Dry);
            Legacy->SetBoolField(TEXT("source_exists"),SourceExists);Legacy->SetBoolField(TEXT("target_exists"),VolumeExists);
            Legacy->SetStringField(TEXT("message"),TEXT("Source or target already exists; set overwrite_existing=true"));
            R.Data->SetBoolField(TEXT("can_create"),false);R.Stage=TEXT("existing_refusal");
            if(!Q.Dry) { R.ErrorCode=TEXT("texture_exists");R.Error=TEXT("Existing texture requires overwrite_existing=true"); }
            if(Q.Dry) { Next(EPhase::DryCompletion,TEXT("waiting_for_dry_loaded_texture_completion")); }
            else { Terminal(false,true);Legacy->SetBoolField(TEXT("success"),false);Publish(); }return false;
        }
        if(Q.Dry)
        { R.Data->SetBoolField(TEXT("can_create"),true);Legacy->SetBoolField(TEXT("success"),true);Legacy->SetBoolField(TEXT("can_create"),true);Legacy->SetBoolField(TEXT("dry_run"),true);
          Legacy->SetBoolField(TEXT("target_exists"),VolumeExists);if(!Q.Raw) { Legacy->SetStringField(TEXT("compression_settings"),Q.VolumeCompression);Legacy->SetStringField(TEXT("mip_gen_settings"),Q.VolumeMips);Legacy->SetStringField(TEXT("address_mode"),Q.Address); }
          Next(EPhase::DryCompletion,TEXT("waiting_for_dry_loaded_texture_completion"));return false; }
        if((Q.Raw&&!SaveSettings(Source.Get()))||!SaveSettings(Volume.Get())) { return false; }
        R.Data->SetBoolField(TEXT("can_create"),true);Next(Q.Raw?EPhase::SourceWrite:EPhase::VolumeWrite,Q.Raw?TEXT("waiting_before_source_write"):TEXT("waiting_before_volume_write"));return true;
    }
    void CompleteDry()
    {
        if(!BothReady()||Phase==EPhase::Terminal) { return; }
        R.Stage=TEXT("dry_run_complete");Terminal(true,true);Legacy->SetBoolField(TEXT("success"),true);Publish();
    }
    template<class T> T* CreateTexture(const FString& Path,const TCHAR* Attempt,const TCHAR* Returned)
    {
        const FString Name=FPackageName::GetLongPackageAssetName(Path);const FString OP=ObjectPath(Path);
        // Avoid same-outer FName collisions which NewObject cannot safely overwrite.
        if(FindObject<UObject>(nullptr,*OP)) { Fail(TEXT("texture_name_collision"),TEXT("Current object collision before texture allocation"),R.bMutationAttempted);return nullptr; }
        UPackage* Before=FindPackage(nullptr,*Path);
        R.bMutationAttempted=true;Bump(TEXT("package_create_attempt"));UPackage* P=CreatePackage(*Path);Bump(TEXT("package_create_return"));
        const bool NewPackage=P&&!Before;R.bChanged=NewPackage||R.bChanged;R.Data->SetBoolField(Path==Q.SourcePath?TEXT("created_source_package"):TEXT("created_volume_package"),NewPackage);Publish();
        if(!Loaded(P)) { Fail(TEXT("texture_package_create_failed"),TEXT("CreatePackage returned unusable package"),true);return nullptr; }
        Bump(Attempt);T* TObj=NewObject<T>(P,*Name,RF_Public|RF_Standalone|RF_Transactional);Bump(Returned);R.bChanged=TObj!=nullptr||R.bChanged;Publish();
        if(!Loaded(TObj)) { Fail(TEXT("texture_create_failed"),TEXT("Texture allocation returned unusable object"),true);return nullptr; }
        Bump(TEXT("asset_created_calls"));FAssetRegistryModule::AssetCreated(TObj);Publish();return TObj;
    }
    void WriteSource()
    {
        if(Source.IsValid()&&!Readiness(Source.Get())) { return; }
        if(!Source.IsValid()) { Source.Reset(CreateTexture<UTexture2D>(Q.SourcePath,TEXT("texture2d_create_attempt"),TEXT("texture2d_create_return")));if(!Source.IsValid()) { return; }CreatedSource=true; }
        else { R.bMutationAttempted=true;Bump(TEXT("source_modify_calls"));Source->Modify();Publish(); }
        if(!RequireAsync(Source.Get())) { return; }
        R.bMutationAttempted=true;Bump(TEXT("source_pre_edit_calls"));Source->PreEditChange(nullptr);Publish();
        Bump(TEXT("source_init_calls"));Source->Source.Init(static_cast<int32>(Q.SX),static_cast<int32>(Q.SY),1,1,TSF_RGBA16,Bytes.GetData());R.bChanged=true;SourceWritten=true;Publish();
        Source->SRGB=Q.SRGB;Source->CompressionSettings=SC;Source->MipGenSettings=SM;Bump(TEXT("source_property_write_count"),3);Publish();
        if(!RequireAsync(Source.Get())) { return; }
        R.bCompileAttempted=true;Bump(TEXT("source_post_edit_calls"));Source->PostEditChange();Publish();
        Next(EPhase::SourceUpdate,TEXT("waiting_before_source_update_resource"));
    }
    void UpdateSourceResource()
    {
        if(!Readiness(Source.Get())||Phase==EPhase::Terminal) { return; }
        Bump(TEXT("source_update_resource_calls"));Source->UpdateResource();Publish();
        Bump(TEXT("source_mark_dirty_calls"));Source->MarkPackageDirty();Publish();
        Next(EPhase::VolumeWrite,TEXT("waiting_before_volume_write"));
    }
    bool SourceCurrent() const
    {
        return Loaded(Source.Get())&&Source->Source.IsValid()&&SourceFormat(Source->Source)&&Source->Source.GetNumBlocks()==1&&Source->Source.GetNumLayers()==1&&Source->Source.GetNumSlices()==1&&
            Source->Source.GetSizeX()==Q.SX&&Source->Source.GetSizeY()==Q.SY&&Source->Source.GetBytesPerPixel()==PixelBytes&&
            (Q.Raw?Source->Source.GetFormat()==TSF_RGBA16:Source->Source.GetId()==SheetSourceId);
    }
    void WriteVolume()
    {
        if(!BothReady()||Phase==EPhase::Terminal) { return; }
        if(!SourceCurrent()) { Fail(TEXT("source_changed_before_volume"),TEXT("Current source changed outside the admitted source layout before volume mutation"),R.bMutationAttempted);return; }
        if(!Volume.IsValid()) { Volume.Reset(CreateTexture<UVolumeTexture>(Q.VolumePath,TEXT("volume_create_attempt"),TEXT("volume_create_return")));if(!Volume.IsValid()) { return; }CreatedVolume=true; }
        else { R.bMutationAttempted=true;Bump(TEXT("volume_modify_calls"));Volume->Modify();Publish(); }
        if(!RequireAsync(Volume.Get())) { return; }
        R.bMutationAttempted=true;Bump(TEXT("volume_pre_edit_calls"));Volume->PreEditChange(nullptr);Publish();
        Volume->Source2DTexture=Source.Get();Volume->Source2DTileSizeX=static_cast<int32>(Q.TX);Volume->Source2DTileSizeY=static_cast<int32>(Q.TY);
        Volume->SRGB=Q.SRGB;Volume->CompressionSettings=VC;Volume->MipGenSettings=VM;Volume->NeverStream=Q.Never;Volume->VirtualTextureStreaming=Q.Virtual;Volume->AddressMode=Address;
        Bump(TEXT("volume_property_write_count"),9);R.bChanged=true;Publish();
        if(!SourceCurrent()) { Fail(TEXT("source_changed_before_volume_copy"),TEXT("Current source changed before SDK volume copy"),true);return; }
        if(!RequireAsync(Volume.Get())) { return; }
        Bump(TEXT("volume_update_source_attempt"));Updated=Volume->UpdateSourceFromSourceTexture();Bump(TEXT("volume_update_source_return"));VolumeWritten=true;Publish();
        if(!RequireAsync(Volume.Get())) { return; }
        R.bCompileAttempted=true;Bump(TEXT("volume_post_edit_calls"));Volume->PostEditChange();Publish();
        Next(EPhase::VolumeUpdate,TEXT("waiting_before_volume_update_resource"));
    }
    void UpdateVolumeResource()
    {
        if(!BothReady()||Phase==EPhase::Terminal) { return; }
        Bump(TEXT("volume_update_resource_calls"));Volume->UpdateResource();Publish();
        Bump(TEXT("volume_mark_dirty_calls"));Volume->MarkPackageDirty();Publish();
        Next(EPhase::Completion,TEXT("waiting_for_owned_texture_completion"));
    }
    void CompleteBuild()
    {
        if(!BothReady()||Phase==EPhase::Terminal) { return; }
        R.Compiled=true;R.Data->SetBoolField(TEXT("texture_build_completion_verified"),true);
        Next(Q.Save?(Q.Raw?EPhase::SourceSave:EPhase::VolumeSave):EPhase::Verify,Q.Save?TEXT("waiting_before_ordered_save"):TEXT("current_texture_verification"));
    }
    void SaveSource()
    {
        if(!BothReady()||Phase==EPhase::Terminal) { return; }
        if(!SaveSettings(Source.Get())) { return; }
        R.bSaveAttempted=true;Bump(TEXT("source_save_attempt"));Publish();SavedSource=SaveLoadedAssetForMCP(Source.Get());Bump(TEXT("source_save_return"));Publish();
        // The volume attempt remains required even when source SavePackage returned false.
        Next(EPhase::VolumeSave,TEXT("waiting_before_volume_save"));
    }
    void SaveVolume()
    {
        if(!BothReady()||Phase==EPhase::Terminal) { return; }
        if(!SaveSettings(Volume.Get())) { return; }
        R.bSaveAttempted=true;Bump(TEXT("volume_save_attempt"));Publish();SavedVolume=SaveLoadedAssetForMCP(Volume.Get());Bump(TEXT("volume_save_return"));R.bSaved=SavedVolume&&(!Q.Raw||SavedSource);Publish();
        Next(EPhase::Verify,TEXT("current_texture_verification"));
    }
    void Verify()
    {
        if(!BothReady()||Phase==EPhase::Terminal) { return; }
        if(!Loaded(Volume.Get())||!Loaded(Source.Get())) { Fail(TEXT("texture_verification_unavailable"),TEXT("Owned texture readback unavailable"),true);return; }
        R.Compiled=true;
        const int32 X=Volume->GetSizeX(),Y=Volume->GetSizeY(),Z=Volume->GetSizeZ();
        R.Data->SetStringField(TEXT("actual_source_object_path"),Source->GetPathName());R.Data->SetStringField(TEXT("actual_volume_object_path"),Volume->GetPathName());
        R.Data->SetNumberField(TEXT("volume_size_x"),X);R.Data->SetNumberField(TEXT("volume_size_y"),Y);R.Data->SetNumberField(TEXT("volume_size_z"),Z);
        auto Current=MakeShared<FJsonObject>();Current->SetBoolField(TEXT("srgb"),Volume->SRGB);Current->SetBoolField(TEXT("never_stream"),Volume->NeverStream);Current->SetBoolField(TEXT("virtual_texture_streaming"),Volume->VirtualTextureStreaming);
        Current->SetNumberField(TEXT("compression_settings"),static_cast<int32>(Volume->CompressionSettings));Current->SetNumberField(TEXT("mip_gen_settings"),static_cast<int32>(Volume->MipGenSettings));Current->SetNumberField(TEXT("address_mode"),static_cast<int32>(Volume->AddressMode));R.Data->SetObjectField(TEXT("current_volume_settings"),Current);
        const bool SourceShape=SourceCurrent();
        const bool SourceProof=SourceShape&&(Q.Raw?(Source->Source.GetFormat()==TSF_RGBA16&&Source->SRGB==Q.SRGB&&Source->CompressionSettings==SC&&Source->MipGenSettings==SM):Source->Source.GetId()==SheetSourceId);
        const bool VolumeShape=Updated&&Volume->Source.IsValid()&&Volume->Source.GetNumBlocks()==1&&Volume->Source.GetNumLayers()==1&&Volume->Source.GetFormat()==Source->Source.GetFormat()&&Volume->Source2DTexture==Source.Get()&&Volume->Source2DTileSizeX==Q.TX&&Volume->Source2DTileSizeY==Q.TY&&
            Volume->Source.GetSizeX()==Q.TX&&Volume->Source.GetSizeY()==Q.TY&&Volume->Source.GetNumSlices()==Depth&&X==Q.TX&&Y==Q.TY&&Z==Depth;
        bool Content=false;
        if(SourceShape&&VolumeShape&&Source->Source.CalcMipSize(0)==SourceBytes&&Volume->Source.CalcMipSize(0)==Q.TX*Q.TY*Depth*PixelBytes)
        {
            TArray64<uint8> SD,VD;
            if(Source->Source.GetMipData(SD,0)&&Volume->Source.GetMipData(VD,0)&&SD.Num()==SourceBytes&&VD.Num()==Q.TX*Q.TY*Depth*PixelBytes)
            {
                Content=!Q.Raw||(Bytes.Num()==SD.Num()&&FMemory::Memcmp(Bytes.GetData(),SD.GetData(),SD.Num())==0);
                const int64 RowBytes=Q.TX*PixelBytes;
                for(int32 Slice=0;Content&&Slice<Depth;++Slice)
                {
                    const int64 TileX=Slice%TilesX,TileY=Slice/TilesX;
                    for(int64 Row=0;Content&&Row<Q.TY;++Row)
                    {
                        const int64 SourceOffset=((TileY*Q.TY+Row)*Q.SX+TileX*Q.TX)*PixelBytes;
                        const int64 VolumeOffset=(static_cast<int64>(Slice)*Q.TY+Row)*RowBytes;
                        Content=FMemory::Memcmp(SD.GetData()+SourceOffset,VD.GetData()+VolumeOffset,RowBytes)==0;
                    }
                }
            }
        }
        const bool VolumeProof=VolumeShape&&Content;
        R.Data->SetBoolField(TEXT("source_volume_tile_bytes_verified"),Content);
        R.Data->SetBoolField(TEXT("source_postcondition_passed"),SourceProof);R.Data->SetBoolField(TEXT("volume_postcondition_passed"),VolumeProof);
        Legacy->SetBoolField(TEXT("created"),CreatedVolume);Legacy->SetBoolField(TEXT("updated_existing"),!CreatedVolume);Legacy->SetBoolField(TEXT("saved"),SavedVolume);
        Legacy->SetBoolField(TEXT("created_source_texture"),CreatedSource);Legacy->SetBoolField(TEXT("created_volume_texture"),CreatedVolume);
        Legacy->SetBoolField(TEXT("updated_existing_source_texture"),Q.Raw&&!CreatedSource);Legacy->SetBoolField(TEXT("updated_existing_volume_texture"),!CreatedVolume);
        Legacy->SetBoolField(TEXT("saved_source_texture"),SavedSource);Legacy->SetBoolField(TEXT("saved_volume_texture"),SavedVolume);Legacy->SetBoolField(TEXT("updated_from_source"),Updated);
        Legacy->SetStringField(TEXT("source_texture_path"),Q.SourcePath);Legacy->SetStringField(TEXT("target_volume_texture_path"),Q.VolumePath);
        if(!Updated) { Legacy->SetStringField(TEXT("message"),TEXT("VolumeTexture was created/updated, but UpdateSourceFromSourceTexture returned false")); }
        if(!SourceProof||!VolumeProof) { Fail(TEXT("texture_postcondition_failed"),TEXT("Current editor source/volume readback did not match the completed finite operation"));return; }
        if(Q.Save&&!R.bSaved) { Fail(TEXT("texture_save_failed"),TEXT("Ordered direct package save returned false; actual source and volume returns retained"));return; }
        R.Stage=TEXT("complete");Terminal(true,true);
    }
    bool Tick(float)
    {
        if(Phase==EPhase::Terminal) { return false; }
        if(!IsInGameThread()) { return true; }
        if(!Cubeless::UE58::IsEditorGameThread()||!Active||IsEngineExitRequested()||IsGarbageCollecting()||GEditor->IsPlaySessionInProgress())
        { Cancel(TEXT("Editor texture controller inactive/exiting, GC or PIE/SIE boundary"));return false; }
        if(GFrameCounter<=LastTickFrame) { return true; }LastTickFrame=GFrameCounter;
        if(FPlatformTime::Seconds()-Started>=DeadlineSeconds) { Fail(TEXT("volume_texture_job_deadline"),TEXT("60second job bound reached; no compiler cancel/save/replay performed"),R.bMutationAttempted);return false; }
        if(!RequireAsync()) { return false; }
        switch(Phase)
        {
        case EPhase::Preflight:Preflight();break;
        case EPhase::DryCompletion:CompleteDry();break;
        case EPhase::SourceWrite:WriteSource();break;
        case EPhase::SourceUpdate:UpdateSourceResource();break;
        case EPhase::VolumeWrite:WriteVolume();break;
        case EPhase::VolumeUpdate:UpdateVolumeResource();break;
        case EPhase::Completion:CompleteBuild();break;
        case EPhase::SourceSave:SaveSource();break;
        case EPhase::VolumeSave:SaveVolume();break;
        case EPhase::Verify:Verify();break;
        default:break;
        }
        return Phase!=EPhase::Terminal;
    }
};

namespace
{
UCubelessVolumeTextureJob* Begin(FRequest&& Q)
{
    if(!IsInGameThread()||IsGarbageCollecting()||IsEngineExitRequested()) { return nullptr; }
    auto State=MakeShared<FJobState>(MoveTemp(Q));
    if(!Cubeless::UE58::IsEditorGameThread()||!Active||Pending.IsValid()||!Recovery.IsEmpty()||IsEngineExitRequested()||IsGarbageCollecting()||GEditor->IsPlaySessionInProgress())
    { State->Fail(TEXT("volume_texture_controller_unavailable"),TEXT("Active game-thread Editor controller with no pending/quarantined job required"));return State->Job.Get(); }
    Pending=State;State->Manager=&FTextureCompilingManager::Get();TWeakPtr<FJobState> Weak=State;
    State->PostCompile=State->Manager->OnTexturePostCompileEvent().AddLambda([Weak](const TArrayView<UTexture* const>& Textures)
    { if(auto S=Weak.Pin()) { if(S->Phase==FJobState::EPhase::Terminal) { return; }for(UTexture* T:Textures) { if(T==S->Source.Get()||T==S->Volume.Get()) { S->EventSignal=true;S->PostCompileObserved=true;break; } } } });
    State->Bump(TEXT("post_compile_add"));
    State->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta)
    { if(auto S=Weak.Pin()) { return S->Tick(Delta); }return false; }),0.0f);State->Bump(TEXT("ticker_add"));
    if(!State->Ticker.IsValid()||!State->PostCompile.IsValid()) { State->Fail(TEXT("volume_texture_hooks_unavailable"),TEXT("Native owned ticker/event registration unavailable")); }
    else { State->R.Stage=TEXT("queued_next_editor_frame");State->Publish(); }
    return State->Job.Get();
}
}
bool Activate()
{ if(!Cubeless::UE58::IsEditorGameThread()||IsEngineExitRequested()) { return false; }Active=true;return true; }
void Deactivate(const FString& Reason)
{ if(!IsInGameThread()) { return; }Active=false;if(auto S=Pending) { S->Cancel(Reason); } }
TSharedRef<FJsonObject> Status()
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("active"),Active);J->SetNumberField(TEXT("pending_count"),Pending.IsValid()?1:0);
    J->SetNumberField(TEXT("recovery_count"),Recovery.Num());J->SetNumberField(TEXT("max_pending"),1);J->SetNumberField(TEXT("deadline_seconds"),DeadlineSeconds);return J;
}
bool Cancel(UCubelessVolumeTextureJob* Job,const FString& Reason)
{ if(!IsInGameThread()||!Job||!Pending.IsValid()||Pending->Job.Get()!=Job) { return false; }auto S=Pending;return S->Cancel(Reason); }
UCubelessVolumeTextureJob* BeginSheet(const FString& S,const FString& V,int64 X,int64 Y,bool D,bool O,bool Save,bool SRGB,bool HasSRGB,const FString& C,const FString& M,bool N,bool VT,const FString& A)
{ FRequest Q;Q.SourcePath=S;Q.VolumePath=V;Q.TX=X;Q.TY=Y;Q.Dry=D;Q.Overwrite=O;Q.Save=Save;Q.SRGB=SRGB;Q.HasSRGB=HasSRGB;Q.VolumeCompression=C;Q.VolumeMips=M;Q.Never=N;Q.Virtual=VT;Q.Address=A;return Begin(MoveTemp(Q)); }
UCubelessVolumeTextureJob* BeginRaw(const FString& Raw,const FString& S,const FString& V,int64 SX,int64 SY,int64 TX,int64 TY,bool D,bool O,bool Save,bool SRGB,const FString& SC,const FString& VC,const FString& SM,const FString& VM,bool N,bool VT,const FString& A)
{ FRequest Q;Q.Raw=true;Q.RawPath=Raw;Q.SourcePath=S;Q.VolumePath=V;Q.SX=SX;Q.SY=SY;Q.TX=TX;Q.TY=TY;Q.Dry=D;Q.Overwrite=O;Q.Save=Save;Q.SRGB=SRGB;Q.SourceCompression=SC;Q.VolumeCompression=VC;Q.SourceMips=SM;Q.VolumeMips=VM;Q.Never=N;Q.Virtual=VT;Q.Address=A;return Begin(MoveTemp(Q)); }
}
