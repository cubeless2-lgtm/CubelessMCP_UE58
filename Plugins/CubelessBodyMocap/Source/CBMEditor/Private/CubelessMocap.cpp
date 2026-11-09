#include "CubelessMocapSettings.h"
#include "MetaHumanSMPLX.h"
#include "OfflineBodyTracker.h"
#include "CameraCalibration.h"
#include "BodyTrackingOptimizer.h"
#include "NNEModelData.h"
#include "NNE.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Async/Async.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformMemory.h"
#include "HAL/Event.h"
#include "Misc/ScopeLock.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/SecureHash.h"
#include "CBMPreparedModel.h"
#include "CBMCancellation.h"
#include "PackageTools.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Containers/Ticker.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/MetaData.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "Misc/EngineVersion.h"
THIRD_PARTY_INCLUDES_START
#include <Eigen/Dense>
THIRD_PARTY_INCLUDES_END
DEFINE_LOG_CATEGORY_STATIC(LogCubelessMocap, Log, All);
static TAutoConsoleVariable<int32> CVarCBMCompactSAM(TEXT("cbm.CompactSAM"),1,TEXT("Enable compact SAM2 and row-parallel resize for the next fast-mode job; 0 is the previous fast baseline."),ECVF_Default);
static TAutoConsoleVariable<int32> CVarCBMDumpStages(TEXT("cbm.DumpStages"),0,TEXT("Write numeric stage diagnostics for the next job to its Saved folder."),ECVF_Default);

struct FSolveData { TArray64<float> Shape,Pose,Translation,Keypoints,Boxes,Contacts;TArray64<bool> ValidFrame;TFunction<bool()> ShouldCancel; };
#include "CBMImageAlignment.inl"
#include "CBMGround.inl"
struct FCBMJob : TSharedFromThis<FCBMJob,ESPMode::ThreadSafe> {
 FCriticalSection Mutex;TAtomic<bool> Cancel{false},Done{false};TFuture<void> Worker;
 FString Stage=TEXT("Queued"),Message,Run,Dir,Video,FFmpeg,OutputFolder,CacheDir;
 double Start=FPlatformTime::Seconds(),Finished=0,Progress=0;int32 FPS=30,FrameCount=0;float StartSeconds=0,Duration=10;bool ImageLimbAlignment=false,FootLocking=true,RefineBody=false,Reuse=true,Success=false,FusedCHMR=false,CompactSAM=false,DumpStages=false;
 uint64 PeakProcess=0;TMap<FString,double> Timings;TMap<FString,uint64> Memory;TMap<FString,FString> StageHashes;float CameraFocal=0,CameraPitch=0,CameraRoll=0;
 // Accessed and destroyed only on the game thread. Global job retains ownership until its worker is joined.
 TStrongObjectPtr<USkeletalMesh> Target,Source;TStrongObjectPtr<UIKRetargeter> Retargeter;
 TStrongObjectPtr<UAnimSequence> Original;FGuid OriginalGuid;FString HandPython,WiLoRRepo,HandScript,HandBackend,HandModel,HandFlowRoot;int32 HandFlowSteps=3,HandFlowBatch=2,HandFlowSeed=0;bool HandDiagnostics=false;float HandStrength=1;
 TWeakObjectPtr<UCubelessMocapSettings> Settings;
 bool AutoHands=false,BodySaved=false,HandsStarted=false,SharedHandFrames=false,HandKeypointsCacheHit=false;
 FString HandKeypointsCacheDir,HandKeypointsCode;
 TSharedPtr<FStreamableHandle> LoadHandle;TArray<FSoftObjectPath> StageAssets;FTSTicker::FDelegateHandle SampleTicker;
 void Update(const FString& InStage,double P=-1) {FScopeLock L(&Mutex);Stage=(AutoHands?(HandsStarted?TEXT("Hands 2/2: "):TEXT("Body 1/2: ")):TEXT(""))+InStage;if(P>=0)Progress=AutoHands?(HandsStarted?.7+.3*FMath::Clamp(P,0.,1.):.7*FMath::Clamp(P,0.,1.)):P;PeakProcess=FMath::Max(PeakProcess,FPlatformMemory::GetStats().UsedPhysical);}
 void Profile() { // Worker only; copy shared fields under lock.
  auto O=MakeShared<FJsonObject>();{FScopeLock L(&Mutex);O->SetStringField(TEXT("stage"),Stage);O->SetStringField(TEXT("message"),Message);O->SetNumberField(TEXT("peak_process_mib"),PeakProcess/1048576.0);}
  O->SetStringField(TEXT("tracking_model"),CompactSAM?TEXT("compact_features_v1"):TEXT("reference"));O->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Start);O->SetNumberField(TEXT("frames"),FrameCount);O->SetStringField(TEXT("chmr_model"),FusedCHMR?TEXT("fused_attention_v1"):TEXT("reference"));
  O->SetStringField(TEXT("pipeline_version"),TEXT("v5_compact_sam"));O->SetNumberField(TEXT("camera_focal_pixels"),CameraFocal);O->SetNumberField(TEXT("camera_pitch_radians"),CameraPitch);O->SetNumberField(TEXT("camera_roll_radians"),CameraRoll);auto H=MakeShared<FJsonObject>();for(auto& X:StageHashes)H->SetStringField(X.Key,X.Value);O->SetObjectField(TEXT("stage_numeric_md5"),H);
  O->SetBoolField(TEXT("image_limb_alignment_requested"),ImageLimbAlignment);O->SetBoolField(TEXT("refine_body_requested"),RefineBody);O->SetBoolField(TEXT("refine_body_enabled"),RefineBody&&FootLocking&&(AutoHands||HandBackend.IsEmpty()));O->SetStringField(TEXT("body_refinement_version"),TEXT("video_support_depth_v1"));
  auto T=MakeShared<FJsonObject>();for(auto& X:Timings)T->SetNumberField(X.Key,X.Value);O->SetObjectField(TEXT("stage_seconds"),T);
  auto M=MakeShared<FJsonObject>();for(auto& X:Memory)M->SetNumberField(X.Key,X.Value/1048576.0);O->SetObjectField(TEXT("process_mib_at_stage_end"),M);
  if(!HandBackend.IsEmpty()){O->SetStringField(TEXT("hand_backend"),HandBackend);if(!AutoHands){O->SetStringField(TEXT("pipeline_version"),TEXT("finger_only_v1"));O->SetStringField(TEXT("chmr_model"),TEXT("not_run"));O->SetStringField(TEXT("tracking_model"),TEXT("not_run"));}}
  O->SetBoolField(TEXT("auto_handflow"),AutoHands);O->SetBoolField(TEXT("body_saved"),BodySaved);O->SetBoolField(TEXT("hand_frames_reused"),SharedHandFrames);O->SetBoolField(TEXT("hand_keypoints_cache_hit"),HandKeypointsCacheHit);
  if(AutoHands)O->SetStringField(TEXT("pipeline_version"),TEXT("body_then_handflow_v1"));
  if(HandBackend==TEXT("JointCorrection"))O->SetStringField(TEXT("pipeline_version"),TEXT("finger_joint_post_v1"));
  FString Text;auto W=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(O,W);FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/Run/TEXT("profile.json")));
 }
};
static TSharedPtr<FCBMJob,ESPMode::ThreadSafe> CurrentJob;
struct FCBMStage {
 TSharedRef<FCBMJob,ESPMode::ThreadSafe> J;FString Name;double Start=FPlatformTime::Seconds();
 FCBMStage(TSharedRef<FCBMJob,ESPMode::ThreadSafe> In,const TCHAR* N,double P):J(In),Name(N){J->Update(Name,P);UE_LOG(LogCubelessMocap,Display,TEXT("[CBM] stage %s begin"),*Name);}
 ~FCBMStage(){const double S=FPlatformTime::Seconds()-Start;J->Timings.Add(Name,S);auto Mem=FPlatformMemory::GetStats().UsedPhysical;J->Memory.Add(Name,Mem);{FScopeLock L(&J->Mutex);J->PeakProcess=FMath::Max(J->PeakProcess,Mem);}UE_LOG(LogCubelessMocap,Display,TEXT("[CBM] stage %s %.3fs process=%.1fMiB"),*Name,S,Mem/1048576.0);J->Profile();}
};
template<class F> static void OnGame(F&& Fn) {
 check(!IsInGameThread());FEvent* Gate=FPlatformProcess::GetSynchEventFromPool(true);
 AsyncTask(ENamedThreads::GameThread,[&Fn,Gate](){Fn();Gate->Trigger();});Gate->Wait();FPlatformProcess::ReturnSynchEventToPool(Gate);
}
// Async package IO; all UObject access and NNE model preparation is confined to the completion on the game thread.
template<class F> static bool Loaded(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,const TArray<FSoftObjectPath>& Paths,F&& Prepare) {
 check(!IsInGameThread());FEvent* Gate=FPlatformProcess::GetSynchEventFromPool(true);bool Ready=false;
 OnGame([&](){J->LoadHandle=UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,FStreamableDelegate::CreateLambda([&](){if(!J->Cancel.Load()){Ready=true;Prepare();J->StageAssets.Append(Paths);}if(J->LoadHandle)J->LoadHandle->ReleaseHandle();J->LoadHandle.Reset();Gate->Trigger();}));if(!J->LoadHandle){Gate->Trigger();}});
 Gate->Wait();FPlatformProcess::ReturnSynchEventToPool(Gate);return Ready&&!J->Cancel.Load();
}
static void ReleasePackages() {OnGame([](){
 if(!CurrentJob)return;TArray<UPackage*> Packages;
 for(const auto& P:CurrentJob->StageAssets){FString ObjectPath=P.ToString();if(!ObjectPath.Contains(TEXT(".")))ObjectPath+=TEXT(".")+FPackageName::GetLongPackageAssetName(ObjectPath);auto* O=FindObject<UObject>(nullptr,*ObjectPath);if(!O||!(O->IsA<UNNEModelData>()||O->IsA<UDataTable>()||O->IsA<UMetaHumanSMPLXData>()))continue;auto* Package=O->GetOutermost();if(!Package->GetName().StartsWith(TEXT("/CubelessBodyMocap/"))||Package->IsDirty())continue;if(GEditor&&GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(O,false))continue;Packages.AddUnique(Package);}
 CurrentJob->StageAssets.Empty();if(Packages.Num()){UPackageTools::FUnloadPackageParams Params(Packages);Params.bResetTransBuffer=false;Params.bUnloadDirtyPackages=false;UPackageTools::UnloadPackages(Params);UE_LOG(LogCubelessMocap,Display,TEXT("[CBM] released %d clean model packages; process=%.1fMiB"),Packages.Num(),FPlatformMemory::GetStats().UsedPhysical/1048576.0);}
 });}
static FSoftObjectPath ModelPath(const TCHAR* N){return FSoftObjectPath(FString(TEXT("/CubelessBodyMocap/Models/Offline/"))+N+TEXT(".")+N);}
static bool ReadFrame(const FString& File,Cubeless::FImageData& Out){FImage Image;if(!FImageUtils::LoadImage(*File,Image))return false;Image.ChangeFormat(ERawImageFormat::BGRA8,EGammaSpace::sRGB);Out.Width=Image.SizeX;Out.Height=Image.SizeY;Out.Data.SetNumUninitialized(Image.RawData.Num());FMemory::Memcpy(Out.Data.GetData(),Image.RawData.GetData(),Image.RawData.Num());return true;}
// Checkpoints hold only numeric intermediate results. A content hash of all frames + algorithm version prevents stale reuse.
static bool Checkpoint(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,const TCHAR* Name,TFunctionRef<void(FArchive&)> IO,bool Save){
 if(!J->Reuse)return false;const FString Path=J->CacheDir/(FString(Name)+TEXT(".bin"));
 if(Save){FBufferArchive A;uint32 Magic=0x43424d32;A<<Magic;IO(A);const FString Hash=FMD5::HashBytes(A.GetData(),A.Num());if(!FFileHelper::SaveArrayToFile(A,*(Path+TEXT(".tmp"))))return false;IFileManager::Get().Move(*Path,*(Path+TEXT(".tmp")),true,true);return FFileHelper::SaveStringToFile(Hash,*(Path+TEXT(".md5")));}
 if(!FPaths::FileExists(Path)||!FPaths::FileExists(Path+TEXT(".md5")))return false;
 TArray<uint8> Bytes;FString Hash;if(!FFileHelper::LoadFileToArray(Bytes,*Path)||!FFileHelper::LoadFileToString(Hash,*(Path+TEXT(".md5")))||Hash!=FMD5::HashBytes(Bytes.GetData(),Bytes.Num()))return false;
 FMemoryReader A(Bytes);uint32 Magic=0;A<<Magic;if(Magic!=0x43424d32)return false;IO(A);if(!A.IsError())UE_LOG(LogCubelessMocap,Display,TEXT("[CBM] checkpoint hit %s"),Name);return !A.IsError();
}
static void DumpFloats(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,const TCHAR* Name,const TArray64<float>& Values){if(J->DumpStages)FFileHelper::SaveArrayToFile(MakeArrayView(reinterpret_cast<const uint8*>(Values.GetData()),Values.Num()*int64(sizeof(float))),*(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run/(FString(Name)+TEXT(".f32"))));}
static bool Solve(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,FString& Result){
 auto Fail=[&](const FString& Why){Result=Why;return false;};
 auto Cancelled=[&](){return J->Cancel.Load();};
 ON_SCOPE_EXIT{ReleasePackages();};
 if(J->Dir.IsEmpty()){
  FCBMStage Stage(J,TEXT("Decode"),0.01);
  if(!FPaths::FileExists(J->Video)||!FPaths::FileExists(J->FFmpeg)||J->Video.Contains(TEXT("\"")))return Fail(TEXT("Select an existing video and FFmpeg executable, or a PNG directory."));
  J->Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run/TEXT("Frames"));IFileManager::Get().MakeDirectory(*J->Dir,true);
  const FString Args=FString::Printf(TEXT("-nostdin -v error -ss %.6f -i \"%s\" -t %.3f -vf fps=%d -start_number 0 \"%s/%%08d.png\""),J->StartSeconds,*J->Video,J->Duration,J->FPS,*J->Dir);
  FProcHandle Proc=FPlatformProcess::CreateProc(*J->FFmpeg,*Args,false,true,true,nullptr,0,nullptr,nullptr);if(!Proc.IsValid())return Fail(TEXT("FFmpeg could not start."));
  ON_SCOPE_EXIT{FPlatformProcess::CloseProc(Proc);};
  while(FPlatformProcess::IsProcRunning(Proc)){if(Cancelled()){FPlatformProcess::TerminateProc(Proc,true);FPlatformProcess::WaitForProc(Proc);return Fail(TEXT("Cancelled during decode."));}FPlatformProcess::Sleep(.05f);}
  int32 Code=-1;FPlatformProcess::GetProcReturnCode(Proc,&Code);if(Code)return Fail(TEXT("FFmpeg decode failed."));
 }
 TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(J->Dir/TEXT("*.png")),true,false);Files.Sort();const int32 N=Files.Num();J->FrameCount=N;
 if(N<2||N>14400)return Fail(TEXT("Expected 2..14400 sequential PNG frames."));
 Cubeless::FImageData Frame;if(!ReadFrame(J->Dir/Files[0],Frame))return Fail(TEXT("Cannot read first frame."));const FIntPoint Size(Frame.Width,Frame.Height);const FIntPoint Raster=UE::BodyTracker::Utils::CalculateAdjustedSize(Size.X,Size.Y,896);
 auto FrameAt=[&](int32 I){return !Cancelled()&&ReadFrame(J->Dir/Files[I],Frame)&&Frame.Width==Size.X&&Frame.Height==Size.Y;};
 FSHA1 Hasher;const FString Version=FString::Printf(TEXT("CBM_pipeline_v5_compact_sam_fps%d_chmr%d_sam%d"),J->FPS,int32(J->FusedCHMR),int32(J->CompactSAM));FTCHARToUTF8 VersionBytes(*Version);Hasher.Update((const uint8*)VersionBytes.Get(),VersionBytes.Length());
 if(J->Reuse){FCBMStage Stage(J,TEXT("Fingerprint"),.02);TArray<uint8> Bytes;for(const auto& F:Files){if(Cancelled())return Fail(TEXT("Cancelled."));if(!FFileHelper::LoadFileToArray(Bytes,*(J->Dir/F)))return Fail(TEXT("Cannot read frame for cache."));Hasher.Update(Bytes.GetData(),Bytes.Num());}Hasher.Final();uint8 Hash[20];Hasher.GetHash(Hash);J->CacheDir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap/Cache")/BytesToHex(Hash,20);IFileManager::Get().MakeDirectory(*J->CacheDir,true);}
 float Focal=0,Pitch=0,Roll=0;
 {FCBMStage Stage(J,TEXT("Camera"),.03);
  auto IO=[&](FArchive& A){A<<Focal<<Pitch<<Roll;};if(!Checkpoint(J,TEXT("Camera"),IO,false)){
   Cubeless::FModelFactory Factory;const auto P=ModelPath(TEXT("camera_calib_seeded"));if(!Loaded(J,{P},[&](){Factory=Cubeless::PrepareModel(Cast<UNNEModelData>(P.ResolveObject()));}))return Fail(TEXT("Cancelled loading camera."));
   auto Camera=Factory?UE::BodyTracker::FCameraCalibration::FromInstances(Factory()):nullptr;Factory.Reset();if(!Camera)return Fail(TEXT("Camera initialization failed."));Camera->Run(Frame);DumpFloats(J,TEXT("Camera_Input"),Camera->PreprocessedImage);Focal=Camera->Fov;Pitch=Camera->Pitch;Roll=Camera->Roll;Camera.Reset();Checkpoint(J,TEXT("Camera"),IO,true);
  }}ReleasePackages();if(Cancelled())return Fail(TEXT("Cancelled."));
 if(!FMath::IsFinite(Focal)||Focal<=0||!FMath::IsFinite(Pitch)||!FMath::IsFinite(Roll))return Fail(TEXT("Invalid camera calibration."));J->CameraFocal=Focal;J->CameraPitch=Pitch;J->CameraRoll=Roll;
 TArray64<bool> Valid;TArray64<FIntRect> Boxes;
 {FCBMStage Stage(J,TEXT("DetectionTracking"),.05);auto IO=[&](FArchive& A){A<<Valid<<Boxes;};if(!Checkpoint(J,TEXT("Tracking"),IO,false)){
  // Detector lifetime ends before tracker sessions are created. One detector result initializes SAM2.
  TUniqueFunction<TUniquePtr<UE::NNE::Segmentation::IDetector>()> Factory;
  TArray<FSoftObjectPath> P={FSoftObjectPath(TEXT("/CubelessBodyMocap/Detectron2/detectron2_backbone_rpn")),FSoftObjectPath(TEXT("/CubelessBodyMocap/Detectron2/detectron2_roi_heads_box")),FSoftObjectPath(TEXT("/CubelessBodyMocap/Detectron2/detectron2_roi_heads_keypoint_patched"))};
  if(!Loaded(J,P,[&](){Factory=UE::NNE::Segmentation::Detectron2::Prepare();}))return Fail(TEXT("Cancelled loading detector."));
  auto Detector=Factory?Factory():nullptr;Factory.Reset();if(!Detector)return Fail(TEXT("Detector initialization failed."));
  int32 First=INDEX_NONE;UE::NNE::Segmentation::FDetection Detection;
  for(int32 I=0;I<N;++I){if(!FrameAt(I))return Fail(TEXT("Cancelled or invalid frame."));FImage Image(Frame.Width,Frame.Height,ERawImageFormat::BGRA8);Image.RawData=Frame.Data;auto D=Detector->Detect(Image);if(D.Num()&&D[0].Score>=.7f){First=I;Detection=D[0];break;}}
  Detector.Reset();ReleasePackages();if(First==INDEX_NONE)return Fail(TEXT("No person detected."));if(Cancelled())return Fail(TEXT("Cancelled."));
  TUniqueFunction<TUniquePtr<UE::NNE::Segmentation::ITracker>()> MakeTracker;
  P={FSoftObjectPath(J->CompactSAM?TEXT("/CubelessBodyMocap/SAM2/sam2_image_encoder_compact"):TEXT("/CubelessBodyMocap/SAM2/sam2_image_encoder")),FSoftObjectPath(J->CompactSAM?TEXT("/CubelessBodyMocap/SAM2/sam2_mask_decoder_compact"):TEXT("/CubelessBodyMocap/SAM2/sam2_mask_decoder")),FSoftObjectPath(TEXT("/CubelessBodyMocap/SAM2/sam2_memory_encoder")),FSoftObjectPath(TEXT("/CubelessBodyMocap/SAM2/sam2_memory_attention")),FSoftObjectPath(TEXT("/CubelessBodyMocap/SAM2/sam2_weights"))};
  if(!Loaded(J,P,[&](){UE::NNE::Segmentation::SAM2::FMakeParams Params;Params.CompactFeatures=J->CompactSAM;Params.ImageEncoderData=LoadObject<UNNEModelData>(nullptr,*P[0].ToString());Params.MaskDecoderData=LoadObject<UNNEModelData>(nullptr,*P[1].ToString());MakeTracker=UE::NNE::Segmentation::SAM2::Prepare(Params);}))return Fail(TEXT("Cancelled loading tracker."));auto Tracker=MakeTracker?MakeTracker():nullptr;MakeTracker.Reset();if(!Tracker)return Fail(TEXT("Tracker initialization failed."));
  Valid.Init(false,N);Boxes.SetNumZeroed(N);
  const float ImageScale=FMath::Min(896.f/Size.Y,1.f);
  for(int32 I=First;I<N;++I){J->Update(FString::Printf(TEXT("Tracking %d/%d"),I+1,N),.05+.20*I/N);if(!FrameAt(I))return Fail(TEXT("Cancelled or invalid frame."));FImage Image(Frame.Width,Frame.Height,ERawImageFormat::BGRA8);Image.RawData=Frame.Data;
   auto T=I==First?Tracker->Initialize(Image,MakeArrayView(&Detection,1)):Tracker->Track(Image);
   if(T.Num()&&T[0].Score>=.5f){Valid[I]=true;Boxes[I]=T[0].Box.Scale(ImageScale);Boxes[I].InflateRect(10);}
  }for(const auto& Timing:Tracker->GetProfileTimings())J->Timings.Add(TEXT("SAM2_")+Timing.Key,Timing.Value);Tracker.Reset();Checkpoint(J,TEXT("Tracking"),IO,true);
 }}ReleasePackages();if(Cancelled())return Fail(TEXT("Cancelled."));
 if(Valid.Num()!=N||Boxes.Num()!=N)return Fail(TEXT("Tracking cache frame count mismatch."));
 J->StageHashes.Add(TEXT("TrackingBoxes"),FMD5::HashBytes(reinterpret_cast<const uint8*>(Boxes.GetData()),Boxes.Num()*sizeof(FIntRect)));
 TArray64<float> Keys;
 {FCBMStage Stage(J,TEXT("Keypoints"),.25);auto IO=[&](FArchive& A){A<<Keys;};if(!Checkpoint(J,TEXT("Keypoints"),IO,false)){
  Cubeless::FModelFactory A,B;const auto PA=ModelPath(TEXT("ViTPose")),PB=ModelPath(TEXT("ViTPosePost"));if(!Loaded(J,{PA,PB},[&](){A=Cubeless::PrepareModel(Cast<UNNEModelData>(PA.ResolveObject()),true);B=Cubeless::PrepareModel(Cast<UNNEModelData>(PB.ResolveObject()),true);}))return Fail(TEXT("Cancelled loading keypoints."));
  auto Est=UE::BodyTracker::FEstimateKeypoints::FromInstances(A?A():nullptr,B?B():nullptr);A.Reset();B.Reset();if(!Est)return Fail(TEXT("Keypoint initialization failed."));
  for(int32 I=0;I<N;++I){J->Update(FString::Printf(TEXT("Keypoints %d/%d"),I+1,N),.25+.20*I/N);if(Cancelled())return Fail(TEXT("Cancelled."));if(Valid[I]){if(!FrameAt(I))return Fail(TEXT("Invalid frame."));Est->Run(Frame,Boxes[I],Keys);}else Keys.AddZeroed(399);}
  Est.Reset();Checkpoint(J,TEXT("Keypoints"),IO,true);
 }}ReleasePackages();if(Cancelled())return Fail(TEXT("Cancelled."));if(Keys.Num()!=N*399)return Fail(TEXT("Keypoint frame count mismatch."));
 J->StageHashes.Add(TEXT("Keypoints"),FMD5::HashBytes(reinterpret_cast<const uint8*>(Keys.GetData()),Keys.Num()*sizeof(float)));
 TArray64<float> Tokens;
 {FCBMStage Stage(J,TEXT("CHMR"),.45);auto IO=[&](FArchive& A){A<<Tokens;};if(!Checkpoint(J,TEXT("Tokens"),IO,false)){
  Cubeless::FModelFactory A,B;const auto PA=ModelPath(J->FusedCHMR?TEXT("chmr_backbone_fused"):TEXT("chmr_backbone")),PB=ModelPath(TEXT("chmr_head"));if(!Loaded(J,{PA,PB},[&](){A=Cubeless::PrepareModel(Cast<UNNEModelData>(PA.ResolveObject()),true);B=Cubeless::PrepareModel(Cast<UNNEModelData>(PB.ResolveObject()),true);}))return Fail(TEXT("Cancelled loading CHMR."));
  auto Est=UE::BodyTracker::FCHMR::FromInstances(A?A():nullptr,B?B():nullptr);A.Reset();B.Reset();if(!Est)return Fail(TEXT("CHMR initialization failed."));
  for(int32 I=0;I<N;++I){J->Update(FString::Printf(TEXT("CHMR %d/%d"),I+1,N),.45+.20*I/N);if(Cancelled())return Fail(TEXT("Cancelled."));if(Valid[I]){if(!FrameAt(I))return Fail(TEXT("Invalid frame."));if(!Est->Run(Frame,Boxes[I],MakeArrayView(Keys.GetData()+I*399,399),Focal,Tokens))return Fail(TEXT("CHMR inference failed or returned non-finite tokens."));}else Tokens.AddZeroed(8192);}
  J->Timings.Add(TEXT("CHMR_Preprocess"),Est->PreprocessSeconds);J->Timings.Add(TEXT("CHMR_Backbone"),Est->BackboneSeconds);J->Timings.Add(TEXT("CHMR_Head"),Est->HeadSeconds);
  Est.Reset();Checkpoint(J,TEXT("Tokens"),IO,true);
 }}ReleasePackages();if(Cancelled())return Fail(TEXT("Cancelled."));if(Tokens.Num()!=N*8192)return Fail(TEXT("CHMR token count mismatch."));
 DumpFloats(J,TEXT("CHMR_Tokens"),Tokens);J->StageHashes.Add(TEXT("CHMR_Tokens"),FMD5::HashBytes(reinterpret_cast<const uint8*>(Tokens.GetData()),Tokens.Num()*sizeof(float)));
 FSolveData Data;Data.ValidFrame=Valid;Data.ShouldCancel=Cancelled;
 for(int32 I=0;I<N;++I){if(Valid[I]){Data.Boxes.Append({float(Boxes[I].Min.X),float(Boxes[I].Min.Y),float(Boxes[I].Max.X),float(Boxes[I].Max.Y)});UE::BodyTracker::FEstimateKeypoints::Op25FromKeypoints(MakeArrayView(Keys.GetData()+I*399,399),Data.Keypoints);}else {Data.Boxes.AddZeroed(4);Data.Keypoints.AddZeroed(75);}}
 {FCBMStage Stage(J,TEXT("Hue"),.65);auto IO=[&](FArchive& A){A<<Data.Pose<<Data.Translation<<Data.Shape<<Data.Contacts;};if(!Checkpoint(J,TEXT("Hue"),IO,false)){
  Cubeless::FModelFactory A,B;const auto PA=ModelPath(TEXT("hue_step_simplified")),PB=ModelPath(TEXT("hue_finalStep_simplified"));if(!Loaded(J,{PA,PB},[&](){A=Cubeless::PrepareModel(Cast<UNNEModelData>(PA.ResolveObject()));B=Cubeless::PrepareModel(Cast<UNNEModelData>(PB.ResolveObject()),false,true);}))return Fail(TEXT("Cancelled loading Hue."));
  auto Est=UE::BodyTracker::FHue::FromInstances(A?A():nullptr,B?B():nullptr);A.Reset();B.Reset();if(!Est)return Fail(TEXT("Hue initialization failed."));Est->ShouldCancel=Cancelled;if(J->DumpStages)Est->DiagnosticDirectory=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run;
  auto Drain=[&](){return Est->Run(Data.Pose,Data.Translation,Data.Shape,Data.Contacts).LastWindow;};
  for(int32 I=0;I<N;++I){if(Cancelled())return Fail(TEXT("Cancelled."));if(Valid[I]){if(Est->EnqueValidFrame(Focal,FIntVector2(Size),MakeArrayView(Tokens.GetData()+I*8192,8192),Boxes[I],MakeArrayView(Keys.GetData()+I*399,399)))Drain();}else Est->EnqueInvalidFrame();}
  for(int32 Flush=0;;++Flush){J->Update(TEXT("Hue temporal solve (50 steps per window)"),.70);if(Cancelled())return Fail(TEXT("Cancelled."));if(Flush>N+16)return Fail(TEXT("Temporal solver did not finish."));if(Drain())break;}
  if(Cancelled())return Fail(TEXT("Cancelled."));Est.Reset();Checkpoint(J,TEXT("Hue"),IO,true);
 }}DumpFloats(J,TEXT("Hue_Pose"),Data.Pose);DumpFloats(J,TEXT("Hue_Translation"),Data.Translation);DumpFloats(J,TEXT("Hue_Shape"),Data.Shape);Tokens.Empty();Keys.Empty();Boxes.Empty();ReleasePackages();if(Cancelled())return Fail(TEXT("Cancelled."));
 if(Data.Pose.Num()!=N*165||Data.Translation.Num()!=N*3||Data.Shape.Num()!=N*10||Data.Contacts.Num()!=N*6)return Fail(TEXT("Inconsistent temporal result sizes."));
 for(const auto* Buffer:{&Data.Shape,&Data.Pose,&Data.Translation,&Data.Keypoints,&Data.Contacts})for(float X:*Buffer)if(!FMath::IsFinite(X))return Fail(TEXT("Non-finite solver result."));
 TUniquePtr<FCBMBodyData> BodyData;TArray<FName> BoneNames;TArray<FTransform> RefPose;
 const FSoftObjectPath PS(TEXT("/CubelessBodyMocap/SKM_SMPL.SKM_SMPL")),PD(TEXT("/CubelessBodyMocap/SMPLX_NEUTRAL_2020_locked_head_array_f32.SMPLX_NEUTRAL_2020_locked_head_array_f32"));
 if(!Loaded(J,{PS,PD},[&](){J->Source.Reset(Cast<USkeletalMesh>(PS.ResolveObject()));if(auto* D=Cast<UMetaHumanSMPLXData>(PD.ResolveObject())){BodyData=MakeUnique<FCBMBodyData>();BodyData->Verts=D->Verts;BodyData->Faces=D->Faces;BodyData->JointReg=D->JointReg;BodyData->Weights=D->Weights;BodyData->BlendShapes=D->BlendShapes;}if(J->Source.IsValid()){const auto& Ref=J->Source->GetRefSkeleton();RefPose=Ref.GetRefBonePose();for(int32 I=0;I<Ref.GetNum();++I)BoneNames.Add(Ref.GetBoneName(I));}}))return Fail(TEXT("Cancelled loading body data."));
 FMetaHumanSMPLX Body;if(!BodyData||!BoneNames.Num()||!Body.InitData(MoveTemp(BodyData)))return Fail(TEXT("SMPL body data failed to initialize."));Body.SetAccountForHeight(false);ReleasePackages();
 TArray<float> Average;Average.Init(0,10);int32 ValidCount=0;for(int32 I=0;I<N;++I)if(Valid[I]){++ValidCount;for(int32 B=0;B<10;++B)Average[B]+=Data.Shape[I*10+B];}if(!ValidCount)return Fail(TEXT("No tracked frames."));for(float& V:Average)V/=ValidCount;for(int32 I=0;I<Data.Shape.Num();++I)Data.Shape[I]=Average[I%10];
 if(J->DumpStages){Body.SetShape(Average);Body.SetPose(TArray<float>());TArray64<float> Bind;for(const auto& B:Body.GetBoneHierarchyUE()){const FVector P=FMetaHumanSMPLX::UE2SMPL(B.Get<2>().GetTranslation());Bind.Append({float(P.X),float(P.Y),float(P.Z)});}DumpFloats(J,TEXT("Shape_Bind_Local_SMPL"),Bind);}
 Body.SetShape(Average);
 // Compact debug observations: FK only, no extra inference or mesh skinning.
 TArray<float> DebugPoints;DebugPoints.Reserve(N*13*6);
 {const auto& H=Body.GetBoneHierarchyUE();const int IDs[]={0,1,4,7,2,5,8,16,18,20,17,19,21};const int KP[]={8,12,13,14,9,10,11,5,6,7,2,3,4};
 for(int F=0;F<N;++F){Eigen::Matrix3f R[22];Eigen::Vector3f P[22];
 for(int B=0;B<22;++B){Eigen::Vector3f V(Data.Pose[F*165+B*3],Data.Pose[F*165+B*3+1],Data.Pose[F*165+B*3+2]);float A=V.norm();Eigen::Matrix3f L=Eigen::Matrix3f::Identity();if(A>1.e-8f)L=Eigen::AngleAxisf(A,V/A).toRotationMatrix();int Parent=H[B+1].Get<1>()-1;FVector X=FMetaHumanSMPLX::UE2SMPL(H[B+1].Get<2>().GetTranslation());Eigen::Vector3f T(X.X,X.Y,X.Z);if(Parent<0){R[B]=L;P[B]=T;}else{R[B]=R[Parent]*L;P[B]=P[Parent]+R[Parent]*T;}}
 for(int I=0;I<13;++I){int K=F*75+KP[I]*3;DebugPoints.Append({Data.Keypoints[K],Data.Keypoints[K+1],Valid[F]?Data.Keypoints[K+2]:0.f});Eigen::Vector3f X=P[IDs[I]]+Eigen::Vector3f(Data.Translation[F*3],Data.Translation[F*3+1],Data.Translation[F*3+2]);DebugPoints.Append({X.z()>.01f?Focal*X.x()/X.z()+Raster.X*.5f:0.f,X.z()>.01f?Focal*X.y()/X.z()+Raster.Y*.5f:0.f,Valid[F]&&X.z()>.01f?1.f:0.f});}}
 }
 float DebugCameraHeight=0;TArray<float> DebugOptimizedCamera;
 float GroundY=0;{FCBMStage Stage(J,TEXT("Ground"),.80);if(!Ground(Data,Body,Average,Pitch,Roll,GroundY))return Fail(TEXT("Ground conversion failed."));if(Cancelled())return Fail(TEXT("Cancelled."));}
 DumpFloats(J,TEXT("Ground_Pose"),Data.Pose);DumpFloats(J,TEXT("Ground_Translation"),Data.Translation);DumpFloats(J,TEXT("Camera_Ground"),TArray64<float>{Focal,float(Raster.X)/2,float(Raster.Y)/2,Pitch,Roll,GroundY});
 { FCBMStage OptStage(J,TEXT("Optimizer"),.83);Cubeless::SetOptimizerCancellation(Cancelled);ON_SCOPE_EXIT{Cubeless::SetOptimizerCancellation({});};FBodyTrackingOptimizer Opt;const auto* D=Body.GetSmplxData();if(!Opt.Initialize(D->Verts,D->Faces,D->JointReg,D->Weights,D->BlendShapes))return Fail(TEXT("Optimizer initialization failed."));Opt.SetCamera(Focal,FVector2f(Raster)/2,Pitch,Roll,FVector(0,GroundY,0));Opt.EnableFootlocking(J->FootLocking);Opt.EnableEvidenceRefinement(J->RefineBody);
  FBodyTrackingOptimizerFrameData O;O.Pose=Data.Pose;O.Translation=Data.Translation;O.Shape=Data.Shape;O.Keypoints=Data.Keypoints;O.BoundingBoxes=Data.Boxes;O.StaticContactLogits=Data.Contacts;O.ValidFrame=Data.ValidFrame;O.NumFrames=N;O.Fps=J->FPS;if(!Opt.Run(O)||!Opt.ApplyResult(Data.Pose,Data.Translation,N))return Fail(TEXT("Body optimization failed."));DebugCameraHeight=Opt.GetCameraHeightOffset();DebugOptimizedCamera=Opt.GetOptimizedCamera();DumpFloats(J,TEXT("Optimizer_CameraHeight"),TArray64<float>{Opt.GetCameraHeightOffset()});
 }if(Cancelled())return Fail(TEXT("Cancelled after optimization; no assets created."));
 DumpFloats(J,TEXT("Optimizer_Pose"),Data.Pose);DumpFloats(J,TEXT("Optimizer_Translation"),Data.Translation);DumpFloats(J,TEXT("Contacts"),Data.Contacts);DumpFloats(J,TEXT("Keypoints25"),Data.Keypoints);
 int32 First=0;while(!Valid[First])++First;int32 Last=First;for(int32 I=0;I<N;++I){if(Valid[I])Last=I;else {for(int32 B=0;B<165;++B)Data.Pose[I*165+B]=Data.Pose[Last*165+B];for(int32 B=0;B<3;++B)Data.Translation[I*3+B]=Data.Translation[Last*3+B];}}
 // Persist camera/framing for the synchronized comparison view independently of diagnostics.
 const FString PreviewDir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run;
 auto SavePreviewFloats=[&](const TCHAR* Name,const TArray64<float>& A){return FFileHelper::SaveArrayToFile(MakeArrayView(reinterpret_cast<const uint8*>(A.GetData()),A.Num()*int64(sizeof(float))),*(PreviewDir/Name));};
 SavePreviewFloats(TEXT("Preview_Boxes.f32"),Data.Boxes);
 SavePreviewFloats(TEXT("Preview_Camera.f32"),TArray64<float>{Focal,float(Raster.X)/2,float(Raster.Y)/2,Pitch,Roll});
 // Solving uses native SMPL coordinates; export uses the grounded SMPL mesh bind space.
 // Root translation below subtracts HeightOffset, so pelvis must include it exactly once.
 Body.SetShape(Average);Body.SetAccountForHeight(true);const int32 Bones=BoneNames.Num();TArray<TArray<FVector3f>> Pos,Scale;TArray<TArray<FQuat4f>> Rot;Pos.SetNum(Bones);Scale.SetNum(Bones);Rot.SetNum(Bones);const FVector Origin(Data.Translation[First*3],0,Data.Translation[First*3+2]);
 {FCBMStage Stage(J,TEXT("BoneBake"),.90);for(int32 I=0;I<N;++I){if(Cancelled())return Fail(TEXT("Cancelled before export."));TArray<float> Pose;Pose.Append(Data.Pose.GetData()+I*165,165);Body.SetPose(Pose);TMap<FName,FTransform> Map;for(const auto& B:Body.GetBoneHierarchyUE())Map.Add(FName(B.Get<0>()),B.Get<2>());const FVector T=FMetaHumanSMPLX::SMPL2UE(FVector(Data.Translation[I*3],Data.Translation[I*3+1],Data.Translation[I*3+2])-Origin);for(int32 B=0;B<Bones;++B){auto X=Map.Contains(BoneNames[B])?Map[BoneNames[B]]:RefPose[B];if(!B)X.AddToTranslation(T-FMetaHumanSMPLX::SMPL2UE(FVector(0,Body.GetHeightOffset(),0)));Pos[B].Add(FVector3f(X.GetTranslation()));Rot[B].Add(FQuat4f(X.GetRotation()));Scale[B].Add(FVector3f(X.GetScale3D()));}}}
 if(J->DumpStages){TArray64<float> Q;for(int32 I=0;I<N;++I)for(int32 B=0;B<Bones;++B){const auto& R=Rot[B][I];Q.Append({R.X,R.Y,R.Z,R.W});}DumpFloats(J,TEXT("Baked_Quaternions"),Q);}
 // Camera transform for exact video projection of exported UE bones.
 {auto M=MakeShared<FJsonObject>();M->SetNumberField(TEXT("version"),2);M->SetNumberField(TEXT("frames"),N);M->SetNumberField(TEXT("fps"),J->FPS);M->SetNumberField(TEXT("start"),J->StartSeconds);M->SetStringField(TEXT("video"),FPaths::ConvertRelativePathToFull(J->Video));M->SetNumberField(TEXT("video_size"),IFileManager::Get().FileSize(*J->Video));M->SetStringField(TEXT("video_stamp"),IFileManager::Get().GetTimeStamp(*J->Video).ToIso8601());
 const float Values[]={Focal,float(Raster.X),float(Raster.Y),Pitch,Roll,GroundY,DebugCameraHeight,float(Origin.X),float(Origin.Y),float(Origin.Z)};TArray<TSharedPtr<FJsonValue>> A;for(float X:Values)A.Add(MakeShared<FJsonValueNumber>(X));M->SetArrayField(TEXT("camera"),A);TArray<TSharedPtr<FJsonValue>> Fit;for(float X:DebugOptimizedCamera)Fit.Add(MakeShared<FJsonValueNumber>(X));M->SetArrayField(TEXT("world_to_camera"),Fit);
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run;FFileHelper::SaveArrayToFile(MakeArrayView(reinterpret_cast<const uint8*>(DebugOptimizedCamera.GetData()),DebugOptimizedCamera.Num()*int64(4)),*(Dir/TEXT("Preview_OptimizedCamera.f32")));FString Json;FJsonSerializer::Serialize(M,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveArrayToFile(MakeArrayView(reinterpret_cast<const uint8*>(DebugPoints.GetData()),DebugPoints.Num()*int64(4)),*(Dir/TEXT("DebugBones.f32")));FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("DebugBones.json")));}
 bool Exported=false;
 {FCBMStage Stage(J,TEXT("Export"),.95);OnGame([&](){
  if(Cancelled()||(GEditor&&GEditor->PlayWorld)){Result=TEXT("Cancelled or PIE started; no output exported.");return;}
  auto Export=[&]()->bool {
 const FString Folder=J->OutputFolder/TEXT("Take_")+J->Run;
 const FString Name=TEXT("Body_")+J->Run;
 TStrongObjectPtr<UAnimSequence> Anim(NewObject<UAnimSequence>(CreatePackage(*(Folder/TEXT("Source")/Name)),*Name,RF_Public|RF_Standalone));
 ON_SCOPE_EXIT { Anim->GetOutermost()->SetDirtyFlag(false); Anim->ClearFlags(RF_Public|RF_Standalone); Anim->SetFlags(RF_Transient); };
 Anim->SetSkeleton(J->Source->GetSkeleton());auto& Controller=Anim->GetController();Controller.InitializeModel();Controller.OpenBracket(FText::FromString(TEXT("Cubeless Body Mocap")),false);Controller.SetFrameRate(FFrameRate(J->FPS,1),false);Controller.SetNumberOfFrames(N-1,false);
 for(int32 B=0;B<Bones;++B){Controller.AddBoneCurve(BoneNames[B],false);Controller.SetBoneTrackKeys(BoneNames[B],Pos[B],Rot[B],Scale[B],false);}Controller.NotifyPopulated();Controller.CloseBracket(false);
 
 // The solved translations contain the subject's shape, not the neutral mesh's
 // limb lengths. IK compression and pelvis scaling must use that same bind.
 // Keep the asset mesh/skeleton untouched; this mesh is only used during bake.
 // AnimSequence rejects RF_Transient retarget-source assets. Use an unsaved,
 // run-owned package while baking, then release it exactly like the source anim.
 const FString BindName=Name+TEXT("_RetargetBind");
 TStrongObjectPtr<USkeletalMesh> RetargetSource(DuplicateObject<USkeletalMesh>(J->Source.Get(),CreatePackage(*(Folder/TEXT("Source")/BindName)),*BindName));
 ON_SCOPE_EXIT { Anim->ClearRetargetSourceAsset();RetargetSource->GetOutermost()->SetDirtyFlag(false);RetargetSource->ClearFlags(RF_Public|RF_Standalone);RetargetSource->SetFlags(RF_Transient); };
 FReferenceSkeleton ShapedRef=RetargetSource->GetRefSkeleton();
 {
  FReferenceSkeletonModifier RefEditor(ShapedRef,RetargetSource->GetSkeleton());
  for(int32 B=1;B<ShapedRef.GetRawBoneNum();++B){FTransform Bind=RefPose[B];Bind.SetTranslation(FVector(Pos[B][First]));RefEditor.UpdateRefPoseTransform(B,Bind);}
 }
 RetargetSource->SetRefSkeleton(ShapedRef);
 RetargetSource->GetRefBasesInvMatrix().Reset();RetargetSource->CalculateInvRefMatrices();
 // Otherwise AnimationScaled applies the neutral-to-shaped ratio a second time.
 Anim->SetRetargetSourceAsset(RetargetSource.Get());Anim->UpdateRetargetSourceAssetData();
 if(Anim->GetRetargetSourceAsset().Get()!=RetargetSource.Get()||Anim->GetRetargetTransforms().Num()<2||!Anim->GetRetargetTransforms()[1].GetTranslation().Equals(FVector(Pos[1][First]),.001))return Fail(TEXT("Subject-shaped retarget bind was not applied."));
 FIKRetargetBatchOperationInputs Inputs;Inputs.AssetsToRetarget.Add(FAssetData(Anim.Get()));Inputs.SourceMesh=RetargetSource.Get();Inputs.TargetMesh=J->Target.Get();Inputs.IKRetargetAsset=J->Retargeter.Get();Inputs.TargetPath=Folder/TEXT("Animation");Inputs.bIncludeReferencedAssets=false;Inputs.bOverwriteExistingFiles=false;
 const TArray<FAssetData> Outputs=UIKRetargetBatchOperation::RunBatchRetarget(Inputs);
 if(Outputs.IsEmpty())return Fail(TEXT("Retargeting produced no assets. Source animation remains unsaved in ")+Folder);
 for(const FAssetData& A:Outputs){auto* Asset=Cast<UAnimSequence>(A.GetAsset());if(!Asset || Asset->GetSkeleton()!=J->Target.Get()->GetSkeleton())return Fail(TEXT("Retargeting returned an unexpected skeleton. Inspect the unsaved output."));if(J->ImageLimbAlignment){FCBMStage AlignStage(J,TEXT("ImageLimbAlignment"),.97);FString AlignmentReport;if(!CBMAlignTargetLimbPlanes(Asset,J->Target.Get(),Data,DebugOptimizedCamera,Origin,Focal,FVector2f(Raster)/2,J->FPS,AlignmentReport))return Fail(TEXT("Target alignment failed: ")+AlignmentReport);FFileHelper::SaveStringToFile(AlignmentReport,*(PreviewDir/TEXT("ImageAlignment.json")));}UPackage* Pkg=Asset->GetOutermost();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;const FString File=FPackageName::LongPackageNameToFilename(Pkg->GetName(),FPackageName::GetAssetPackageExtension());if(!UPackage::SavePackage(Pkg,Asset,*File,Args))return Fail(TEXT("Could not save ")+Pkg->GetName());}
 J->Original.Reset(Cast<UAnimSequence>(Outputs[0].GetAsset()));J->OriginalGuid=J->Original->GetDataModel()->GenerateGuid();J->BodySaved=true;
 if(J->Settings.IsValid()){J->Settings->PreviewAnimation=J->Original.Get();J->Settings->OriginalAnimation=J->Settings->PreviewAnimation;J->Settings->JointComparisonSource=nullptr;J->Settings->JointCorrectedAnimation=nullptr;J->Settings->JointCorrectionReport.FilePath.Empty();J->Settings->JointCorrectionSource=nullptr;J->Settings->HandEnhancedAnimation=nullptr;J->Settings->HandComparisonBase=nullptr;J->Settings->WiLoRAnimation=nullptr;J->Settings->HandFlowAnimation=nullptr;J->Settings->MediaPipeAnimation=nullptr;}
  Result=TEXT("Saved animation in ")+Folder+FString::Printf(TEXT(" (%d/%d tracked frames)."),ValidCount,N);return true;};Exported=Export();
 });}return Exported;
}
#include "CBMHands.inl"
#include "CBMJointPost.inl"
static double CBMPipelineStart=0,CBMPipelineEnd=0;
static bool CBMPipelineActive=false,CBMPipelineDispatch=false,CBMPipelineCancel=false,CBMExportActive=false;
bool UCubelessMocapLibrary::IsBusy(){check(IsInGameThread());return CBMExportActive||(CBMPipelineActive&&!CBMPipelineDispatch)||(CurrentJob&&!CurrentJob->Done.Load());}
void UCubelessMocapLibrary::Cancel(){check(IsInGameThread());CBMPipelineCancel=true;if(CurrentJob&&!CurrentJob->Done.Load()){CurrentJob->Cancel.Store(true);FScopeLock L(&CurrentJob->Mutex);CurrentJob->Message=TEXT("Cancellation requested; waiting for the current native call to return.");}}
FString UCubelessMocapLibrary::GetJobStatus(){check(IsInGameThread());if(!CurrentJob)return TEXT("Idle");auto J=CurrentJob;FScopeLock L(&J->Mutex);return (CBMPipelineStart>0?FString::Printf(TEXT("전체 처리 %.1f초 | "),(CBMPipelineEnd>0?CBMPipelineEnd:FPlatformTime::Seconds())-CBMPipelineStart):FString())+FString::Printf(TEXT("%s | %.0f%% | %.1fs | peak RAM %.0f MiB\n%s"),*J->Stage,J->Progress*100,(J->Finished>0?J->Finished:FPlatformTime::Seconds())-J->Start,J->PeakProcess/1048576.0,*J->Message);}
bool UCubelessMocapLibrary::Extract(UCubelessMocapSettings* S,FString& Result){
 if(!IsInGameThread()||!S){Result=TEXT("Start on the Editor game thread.");return false;}if(IsBusy()){Result=TEXT("Another job is running.");return false;}if(GEditor&&GEditor->PlayWorld){Result=TEXT("End PIE/SIE first.");return false;}
 if(!S->TargetMesh||!S->Retargeter||!S->OutputFolder.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(S->OutputFolder)||S->FrameRate<1||S->FrameRate>120||!FMath::IsFinite(S->StartSeconds)||!FMath::IsFinite(S->DurationSeconds)||S->StartSeconds<0||S->DurationSeconds<=0||S->DurationSeconds>120){Result=TEXT("Invalid target, output folder, time range or FPS.");return false;}
 if(CurrentJob&&CurrentJob->Worker.IsValid())CurrentJob->Worker.Wait();
 auto J=MakeShared<FCBMJob,ESPMode::ThreadSafe>();J->Run=FGuid::NewGuid().ToString(EGuidFormats::Digits);J->Dir=S->FrameDirectory.Path;J->Video=S->Video.FilePath;J->FFmpeg=S->FFmpeg.FilePath;J->FPS=S->FrameRate;J->Duration=S->DurationSeconds;J->StartSeconds=S->StartSeconds;J->ImageLimbAlignment=S->ImageLimbAlignment;J->FootLocking=S->FootLocking;J->RefineBody=S->RefineBody;J->Reuse=S->ReuseCheckpoints;J->DumpStages=CVarCBMDumpStages.GetValueOnGameThread()!=0;J->FusedCHMR=S->UseFusedCHMR&&UE::NNE::GetRuntime<INNERuntimeGPU>(TEXT("NNERuntimeORTDml")).IsValid()&&FPackageName::DoesPackageExist(TEXT("/CubelessBodyMocap/Models/Offline/chmr_backbone_fused"));J->CompactSAM=J->FusedCHMR&&CVarCBMCompactSAM.GetValueOnGameThread()!=0&&FPackageName::DoesPackageExist(TEXT("/CubelessBodyMocap/SAM2/sam2_image_encoder_compact"))&&FPackageName::DoesPackageExist(TEXT("/CubelessBodyMocap/SAM2/sam2_mask_decoder_compact"));J->OutputFolder=S->OutputFolder;J->Target.Reset(S->TargetMesh);J->Retargeter.Reset(S->Retargeter);J->Settings=S;J->AutoHands=S->EnhanceHands;
 if(J->AutoHands&&!CBMConfigureAutoHands(J,S,Result))return false;
 CurrentJob=J;
 J->SampleTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([J](float){FScopeLock L(&J->Mutex);J->PeakProcess=FMath::Max(J->PeakProcess,FPlatformMemory::GetStats().UsedPhysical);return !J->Done.Load();}),.25f);
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run),true);
 J->Worker=Async(EAsyncExecution::Thread,[J](){FString Message;bool OK=Solve(J,Message);
  if(OK&&J->AutoHands){
   if(J->Cancel.Load()){OK=false;Message=TEXT("Cancelled after body save; HandFlow not started.");}
   else {J->HandsStarted=true;J->HandBackend=TEXT("HandFlow");J->SharedHandFrames=true;OK=CBMSolveHands(J,Message);}
  }
  if(!OK){if(J->Cancel.Load())Message=TEXT("Cancelled. Completed checkpoints were preserved.");if(J->BodySaved)Message+=TEXT(" Body animation was saved and remains available; hand enhancement was not completed.");}{FScopeLock L(&J->Mutex);J->Success=OK;J->Finished=FPlatformTime::Seconds();J->Message=Message;J->Stage=OK?TEXT("Completed"):J->Cancel.Load()?TEXT("Cancelled"):TEXT("Failed");if(OK)J->Progress=1;}J->Profile();UE_LOG(LogCubelessMocap,Display,TEXT("[CBM] %s: %s"),OK?TEXT("Completed"):TEXT("Stopped"),*Message);J->Done.Store(true);});
 Result=TEXT("Started job ")+J->Run;return true;
}

void UCubelessMocapLibrary::ShutdownJobs(){check(IsInGameThread());check(!IsBusy());if(CurrentJob){if(CurrentJob->Worker.IsValid())CurrentJob->Worker.Wait();FTSTicker::GetCoreTicker().RemoveTicker(CurrentJob->SampleTicker);}CurrentJob.Reset();}
#include "CBMWorkflow.inl"
