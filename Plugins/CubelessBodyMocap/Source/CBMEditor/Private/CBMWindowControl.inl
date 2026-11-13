// Included after the editor module. All calls run on the editor game thread.
static FString CBMWindowJson(const TSharedRef<FJsonObject>& Object){FString Out;FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Out));return Out;}
static FCubelessMocapModule& CBMWindowModule(){check(IsInGameThread());return FModuleManager::LoadModuleChecked<FCubelessMocapModule>(TEXT("CBMEditor"));}

UCubelessMocapSettings* FCubelessMocapModule::GetWindowSettings(){if(!Settings.IsValid()||!ExistingWindow.IsValid())Open();return Settings.Get();}
bool FCubelessMocapModule::SetVideo(const FString& Path,FString& Message){
 if(UCubelessMocapLibrary::IsBusy()){Message=TEXT("busy");return false;}
 FString Full=FPaths::ConvertRelativePathToFull(Path);FPaths::NormalizeFilename(Full);FPaths::CollapseRelativeDirectories(Full);
 if(Path.IsEmpty()||!FPaths::FileExists(Full)){Message=TEXT("video_file_not_found");return false;}
 auto* S=GetWindowSettings();S->Video.FilePath=Full;S->FrameDirectory.Path.Empty();SaveNotice.Empty();
 Comparison.Pin()->OpenSourceVideo();Message=TEXT("opening; poll media_ready");return true;
}
TSharedRef<FJsonObject> FCubelessMocapModule::ReadWindowState()const{
 auto P=Comparison.Pin();auto O=P?P->ReadState():MakeShared<FJsonObject>();
 O->SetStringField(TEXT("schema"),TEXT("cbm_window_state_v1"));O->SetBoolField(TEXT("window_open"),ExistingWindow.IsValid());
 O->SetBoolField(TEXT("busy"),UCubelessMocapLibrary::IsBusy());O->SetStringField(TEXT("job_status"),UCubelessMocapLibrary::GetJobStatus());
 if(auto W=ExistingWindow.Pin()){const auto Size=W->GetClientSizeInScreen();O->SetNumberField(TEXT("window_width"),Size.X);O->SetNumberField(TEXT("window_height"),Size.Y);}
 if(auto L=LayoutSplitter.Pin())O->SetNumberField(TEXT("preview_fraction"),L->SlotAt(0).GetSizeValue());
 if(Settings.IsValid()){
  const auto* S=Settings.Get();auto Path=[](const UObject* A){return A?A->GetPathName():FString();};
  O->SetStringField(TEXT("video"),S->Video.FilePath);O->SetNumberField(TEXT("analysis_fps"),S->FrameRate);
  O->SetBoolField(TEXT("save_hand_flow"),S->SaveHandFlow);O->SetBoolField(TEXT("save_finger_correction"),S->SaveBoneCorrection);
  O->SetNumberField(TEXT("preview_rate"),S->PreviewPlayRate);O->SetNumberField(TEXT("save_rate"),S->SavePlayRate);
  O->SetStringField(TEXT("preview_animation"),Path(S->PreviewAnimation));O->SetStringField(TEXT("exported_animation"),Path(S->ExportedSequence));
  O->SetStringField(TEXT("output_folder"),S->OutputFolder);O->SetStringField(TEXT("sequence_name"),S->SequenceName);
  TArray<TSharedPtr<FJsonValue>> Results;
  for(int32 I=0;I<4;++I){const bool H=(I&2)!=0,C=(I&1)!=0;const UAnimSequence* Base=H?S->HandFlowAnimation.Get():S->OriginalAnimation.Get();
   const UAnimSequence* A=C?(H?S->CorrectedHandAnimation.Get():S->CorrectedBodyAnimation.Get()):Base;
   auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("key"),H?(C?TEXT("hands_corrected"):TEXT("hands")):(C?TEXT("body_corrected"):TEXT("body")));
   R->SetBoolField(TEXT("source_available"),Base!=nullptr);R->SetBoolField(TEXT("ready"),A!=nullptr);R->SetBoolField(TEXT("selected"),H==S->SaveHandFlow&&C==S->SaveBoneCorrection);R->SetStringField(TEXT("animation"),Path(A));Results.Add(MakeShared<FJsonValueObject>(R));
  }O->SetArrayField(TEXT("results"),Results);
 }return O;
}
FString FCubelessMocapModule::Reply(const FString& Action,bool Success,const FString& Message)const{
 auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Success);O->SetStringField(TEXT("action"),Action);O->SetStringField(TEXT("message"),Message);O->SetObjectField(TEXT("state"),ReadWindowState());return CBMWindowJson(O);
}
FString FCubelessMocapModule::ChangeLayout(float PreviewFraction,float VideoFraction,FVector2D Size){
 if(!ExistingWindow.IsValid()||!Comparison.IsValid()||!LayoutSplitter.IsValid())return Reply(TEXT("layout"),false,TEXT("window_closed"));
 if(UCubelessMocapLibrary::IsBusy())return Reply(TEXT("layout"),false,TEXT("busy"));
 if(!FMath::IsFinite(PreviewFraction)||!FMath::IsFinite(VideoFraction)||!FMath::IsFinite(Size.X)||!FMath::IsFinite(Size.Y)||PreviewFraction<.2||PreviewFraction>.85||VideoFraction<.2||VideoFraction>.8||Size.X<1000||Size.X>3840||Size.Y<720||Size.Y>2160||Size.X*PreviewFraction<640||Size.X*(1-PreviewFraction)<250)return Reply(TEXT("layout"),false,TEXT("invalid_layout: window 1000..3840 x 720..2160, preview >=640, settings >=250, video fraction .2.. .8"));
 auto L=LayoutSplitter.Pin();L->SlotAt(0).SetSizeValue(PreviewFraction);L->SlotAt(1).SetSizeValue(1-PreviewFraction);Comparison.Pin()->SetVideoFraction(VideoFraction);ExistingWindow.Pin()->Resize(Size);
 return Reply(TEXT("layout"),true,TEXT("layout_applied"));
}
FString FCubelessMocapModule::CloseWindow(){
 if(UCubelessMocapLibrary::IsBusy())return Reply(TEXT("close"),false,TEXT("busy; cancel and wait before closing"));
 if(auto W=ExistingWindow.Pin())W->RequestDestroyWindow();return Reply(TEXT("close"),true,TEXT("close_requested; poll window_open"));
}

UCubelessMocapSettings* UCubelessMocapWindowLibrary::GetSettings(){return CBMWindowModule().GetWindowSettings();}
FString UCubelessMocapWindowLibrary::OpenVideo(const FString& FilePath){auto& M=CBMWindowModule();FString Text;const bool OK=M.SetVideo(FilePath,Text);return M.Reply(TEXT("open_video"),OK,Text);}
FString UCubelessMocapWindowLibrary::Control(const FString& Action,double Value){auto& M=CBMWindowModule();auto P=M.Comparison.Pin();FString Text=TEXT("window_closed");const bool OK=P&&P->Control(Action,Value,Text);return M.Reply(Action,OK,Text);}
FString UCubelessMocapWindowLibrary::SetRange(double StartSeconds,double EndSeconds){auto& M=CBMWindowModule();auto P=M.Comparison.Pin();FString Text=TEXT("window_closed");const bool OK=P&&P->EditRange(StartSeconds,EndSeconds,Text);return M.Reply(TEXT("range"),OK,Text);}
FString UCubelessMocapWindowLibrary::SelectResult(bool HandFlow,bool FingerCorrection){auto& M=CBMWindowModule();FString Text;const bool OK=M.SelectResult(HandFlow,FingerCorrection,&Text);return M.Reply(TEXT("select_result"),OK,Text);}
FString UCubelessMocapWindowLibrary::SetLayout(float PreviewFraction,float VideoFraction,FVector2D WindowSize){return CBMWindowModule().ChangeLayout(PreviewFraction,VideoFraction,WindowSize);}
FString UCubelessMocapWindowLibrary::GetState(){return CBMWindowJson(CBMWindowModule().ReadWindowState());}
FString UCubelessMocapWindowLibrary::Close(){return CBMWindowModule().CloseWindow();}
