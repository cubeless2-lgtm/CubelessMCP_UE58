// Full-frame hand cues are deliberately separate from the cropped body keypoints.
// Hash inputs in bounded chunks; model/code changes and corrupt/incomplete entries miss.
static FString CBMHandFileHash(const FString& Path,const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J){
 TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));if(!Reader)return {};
 FSHA1 Hash;TArray<uint8> Buffer;Buffer.SetNumUninitialized(1024*1024);
 while(Reader->Tell()<Reader->TotalSize()){
  if(J->Cancel.Load())return {};
  const int64 Count=FMath::Min<int64>(Buffer.Num(),Reader->TotalSize()-Reader->Tell());
  Reader->Serialize(Buffer.GetData(),Count);if(Reader->IsError())return {};Hash.Update(Buffer.GetData(),uint32(Count));
 }
 Hash.Final();uint8 Digest[20];Hash.GetHash(Digest);return BytesToHex(Digest,20);
}
static void CBMPrepareHandCueCache(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,const TArray<FString>& Frames){
 J->HandKeypointsCacheDir.Empty();if(!J->Reuse)return;
 FSHA1 Hash;
 auto Add=[&](const FString& Value){FTCHARToUTF8 Bytes(*(Value+TEXT("\n")));Hash.Update(reinterpret_cast<const uint8*>(Bytes.Get()),Bytes.Length());};
 Add(TEXT("full_frame_vitpose_896_v1"));Add(FEngineVersion::Current().ToString());Add(FString::Printf(TEXT("%d/%d"),J->FPS,J->FrameCount));
 TArray<FString> Code;J->HandKeypointsCode.ParseIntoArray(Code,TEXT("|"));if(Code.IsEmpty())return;
 for(const FString& Path:Code){const FString H=CBMHandFileHash(Path,J);if(H.IsEmpty())return;Add(H);}
 for(const TCHAR* Name:{TEXT("ViTPose"),TEXT("ViTPosePost")}){
  const FString Package=FString(TEXT("/CubelessBodyMocap/Models/Offline/"))+Name;
  for(const TCHAR* Extension:{TEXT(".uasset"),TEXT(".uexp"),TEXT(".ubulk")}){
   const FString Path=FPackageName::LongPackageNameToFilename(Package,Extension);
   if(!FPaths::FileExists(Path)){if(FString(Extension)==TEXT(".uasset"))return;Add(TEXT("absent"));continue;}
   const FString H=CBMHandFileHash(Path,J);if(H.IsEmpty())return;Add(H);
  }
 }
 for(const FString& File:Frames){const FString H=CBMHandFileHash(J->Dir/File,J);if(H.IsEmpty())return;Add(H);}
 Hash.Final();uint8 Digest[20];Hash.GetHash(Digest);
 J->HandKeypointsCacheDir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap/HandCueCache")/BytesToHex(Digest,20);
}
static bool CBMHandCueCache(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,TArray64<float>& Keys,bool Save){
 if(J->HandKeypointsCacheDir.IsEmpty()||J->Cancel.Load())return false;
 const FString Path=J->HandKeypointsCacheDir/TEXT("keypoints.f32");const int64 Expected=int64(J->FrameCount)*399*sizeof(float);
 if(Save){
  if(Keys.Num()!=int64(J->FrameCount)*399)return false;
  IFileManager::Get().MakeDirectory(*J->HandKeypointsCacheDir,true);
  if(!FFileHelper::SaveArrayToFile(MakeArrayView(reinterpret_cast<const uint8*>(Keys.GetData()),Expected),*(Path+TEXT(".tmp"))))return false;
  if(!IFileManager::Get().Move(*Path,*(Path+TEXT(".tmp")),true,true))return false;
  return FFileHelper::SaveStringToFile(FMD5::HashBytes(reinterpret_cast<const uint8*>(Keys.GetData()),Expected),*(Path+TEXT(".md5")));
 }
 FString Digest;TArray<uint8> Data;
 if(IFileManager::Get().FileSize(*Path)!=Expected||!FFileHelper::LoadFileToString(Digest,*(Path+TEXT(".md5")))||!FFileHelper::LoadFileToArray(Data,*Path)||Digest!=FMD5::HashBytes(Data.GetData(),Data.Num()))return false;
 Keys.SetNumUninitialized(int64(J->FrameCount)*399);FMemory::Memcpy(Keys.GetData(),Data.GetData(),Expected);
 for(float V:Keys)if(!FMath::IsFinite(V)){Keys.Empty();return false;}
 J->HandKeypointsCacheHit=true;UE_LOG(LogCubelessMocap,Display,TEXT("[CBM] exact full-frame hand keypoints cache hit"));return true;
}
static FString CBMHandCueCodePaths(){
 // Capture module paths on the game thread, hash their contents on the worker.
 return FModuleManager::Get().GetModuleFilename(TEXT("CBMEditor"))+TEXT("|")+FModuleManager::Get().GetModuleFilename(TEXT("CBMInference"))+TEXT("|")+FModuleManager::Get().GetModuleFilename(TEXT("CBMSegmentation"))+TEXT("|")+FModuleManager::Get().GetModuleFilename(TEXT("NNERuntimeORT"));
}
