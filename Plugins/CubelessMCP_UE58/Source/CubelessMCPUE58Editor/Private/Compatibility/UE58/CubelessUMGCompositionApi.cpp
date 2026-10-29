#include "Compatibility/UE58/CubelessUMGCompositionApi.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/SafeZone.h"
#include "Components/BackgroundBlur.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/MovieScene2DTransformSection.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::UMGComposition
{
namespace
{
bool Busy=false;
void Error(const TSharedRef<FJsonObject>& R,const TCHAR* E) { R->SetStringField(TEXT("error"),E);R->SetBoolField(TEXT("success"),false); }
bool EditorOK() { return IsInGameThread()&&GEditor&&!GEditor->PlayWorld&&!GEditor->bIsSimulatingInEditor; }
UWidgetBlueprint* Load(const FString& P)
{
 if(!EditorOK()||!P.StartsWith(TEXT("/Game/")))return nullptr;
 auto BP=LoadObject<UWidgetBlueprint>(nullptr,*(P+TEXT(".")+FPackageName::GetLongPackageAssetName(P)));
 return IsValid(BP)&&BP->GetOutermost()->GetName()==P&&BP->ParentClass==UUserWidget::StaticClass()&&IsValid(BP->WidgetTree)&&BP->WidgetTree->GetOuter()==BP?BP:nullptr;
}
UClass* Type(const FString& N)
{
 if(N==TEXT("CanvasPanel"))return UCanvasPanel::StaticClass();if(N==TEXT("Overlay"))return UOverlay::StaticClass();
 if(N==TEXT("Border"))return UBorder::StaticClass();if(N==TEXT("Image"))return UImage::StaticClass();if(N==TEXT("TextBlock"))return UTextBlock::StaticClass();
 if(N==TEXT("Button"))return UButton::StaticClass();if(N==TEXT("ProgressBar"))return UProgressBar::StaticClass();if(N==TEXT("SizeBox"))return USizeBox::StaticClass();
 if(N==TEXT("HorizontalBox"))return UHorizontalBox::StaticClass();if(N==TEXT("VerticalBox"))return UVerticalBox::StaticClass();
 if(N==TEXT("SafeZone"))return USafeZone::StaticClass();if(N==TEXT("BackgroundBlur"))return UBackgroundBlur::StaticClass();return nullptr;
}
bool Current(UWidgetBlueprint* BP,UWidgetTree* Tree,UPackage* Package,UWorld* World)
{
 return EditorOK()&&GEditor->GetEditorWorldContext().World()==World&&IsValid(BP)&&IsValid(Tree)&&IsValid(Package)&&BP->WidgetTree==Tree&&Tree->GetOuter()==BP&&BP->GetOutermost()==Package;
}
void State(UWidgetBlueprint* BP,const TSharedRef<FJsonObject>& R)
{
 if(!IsValid(BP))return;R->SetStringField(TEXT("asset"),BP->GetPathName());R->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());
 R->SetNumberField(TEXT("compile_status"),static_cast<int32>(BP->Status));R->SetBoolField(TEXT("compiled"),BP->Status==BS_UpToDate||BP->Status==BS_UpToDateWithWarnings);
}
}
void Compose(const FString& P,const TArray<FNode>& Nodes,const TSharedRef<FJsonObject>& R)
{
 if(!EditorOK()||Busy){Error(R,TEXT("Editor game thread outside PIE/SIE and nonreentrant operation required"));return;}TGuardValue<bool> Guard(Busy,true);
 if(UEditorAssetLibrary::DoesAssetExist(P)||FindPackage(nullptr,*P)){Error(R,TEXT("Target exists: composition is create-only; no existing asset changed"));return;}
 for(const FNode& N:Nodes)if(!Type(N.Type)||FindFProperty<FProperty>(UUserWidget::StaticClass(),FName(*N.Name))||UUserWidget::StaticClass()->FindFunctionByName(FName(*N.Name))){Error(R,TEXT("Widget name collides with native members or type is unavailable"));return;}
 R->SetStringField(TEXT("stage"),TEXT("create_package"));R->SetBoolField(TEXT("mutation_attempted"),true);
 UPackage* Package=CreatePackage(*P);if(!IsValid(Package)){Error(R,TEXT("Package creation failed"));return;}TStrongObjectPtr<UPackage> KeepPackage(Package);R->SetBoolField(TEXT("changed"),true);
 auto BP=Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UUserWidget::StaticClass(),Package,FName(*FPackageName::GetLongPackageAssetName(P)),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
 if(!IsValid(BP)||!IsValid(BP->WidgetTree)||BP->WidgetTree->GetOuter()!=BP){Error(R,TEXT("Source WidgetBlueprint/tree creation failed; created package retained"));return;}
 TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);UWidgetTree* Tree=BP->WidgetTree;UWorld* World=GEditor->GetEditorWorldContext().World();
 TMap<FName,UWidget*> Created;R->SetStringField(TEXT("stage"),TEXT("construct_tree"));TArray<TSharedPtr<FJsonValue>> Names;
 for(const FNode& N:Nodes)
 {
  if(!Current(BP,Tree,Package,World)){Error(R,TEXT("Source identity changed; partial tree retained, inspect before retry"));return;}
  UWidget* W=Tree->ConstructWidget<UWidget>(Type(N.Type),FName(*N.Name));if(!IsValid(W)||W->GetOuter()!=Tree||W->GetFName()!=FName(*N.Name)){Error(R,TEXT("Widget construction/name postcondition failed; partial tree retained"));return;}
  TStrongObjectPtr<UWidget> KeepWidget(W);Created.Add(FName(*N.Name),W);W->bIsVariable=true;BP->OnVariableAdded(W->GetFName());
  if(!Current(BP,Tree,Package,World)||!IsValid(W)||W->GetOuter()!=Tree){Error(R,TEXT("Source/widget changed in variable notification; partial tree retained"));return;}
  if(N.Parent.IsEmpty())Tree->RootWidget=W;
  else
  {
   UPanelWidget* Parent=Cast<UPanelWidget>(Created.FindRef(FName(*N.Parent)));if(!Parent||!Parent->AddChild(W)){Error(R,TEXT("Child attachment failed; partial tree retained"));return;}
  }
  Names.Add(MakeShared<FJsonValueString>(W->GetPathName()));R->SetArrayField(TEXT("created_widgets"),Names);
 }
 if(!Current(BP,Tree,Package,World)){Error(R,TEXT("Source changed after construction"));return;}
 FAssetRegistryModule::AssetCreated(BP);
 if(!Current(BP,Tree,Package,World)){Error(R,TEXT("Source changed in asset registry callback; inspect created package"));return;}
 BP->MarkPackageDirty();
 if(!Current(BP,Tree,Package,World)){Error(R,TEXT("Source changed in dirty notification; no further authoring"));return;}
 FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
 if(!Current(BP,Tree,Package,World)){Error(R,TEXT("Source changed in Blueprint notification; inspect created package"));return;}
 R->SetStringField(TEXT("stage"),TEXT("composed_unsaved"));R->SetBoolField(TEXT("needs_finalize"),true);R->SetBoolField(TEXT("success"),true);State(BP,R);
}
UWidget* SourceWidget(const FString& P,const FString& Name)
{
 UWidgetBlueprint* BP=Load(P);if(!BP)return nullptr;UWidget* W=BP->WidgetTree->FindWidget(FName(*Name));return IsValid(W)&&W->GetOuter()==BP->WidgetTree?W:nullptr;
}
void Inspect(const FString& P,const TSharedRef<FJsonObject>& R)
{
 UWidgetBlueprint* BP=Load(P);if(!BP){Error(R,TEXT("Native UserWidget source Blueprint with an owned tree required"));return;}
 TArray<UWidget*> Widgets;BP->WidgetTree->GetAllWidgets(Widgets);TArray<TSharedPtr<FJsonValue>> A;
 for(UWidget* W:Widgets)
 {
  if(!IsValid(W)||W->GetOuter()!=BP->WidgetTree){Error(R,TEXT("Unexpected widget ownership"));return;}
  auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),W->GetName());O->SetStringField(TEXT("class"),W->GetClass()->GetPathName());O->SetStringField(TEXT("parent"),W->GetParent()?W->GetParent()->GetName():TEXT(""));O->SetBoolField(TEXT("is_variable"),W->bIsVariable);A.Add(MakeShared<FJsonValueObject>(O));
 }
 TArray<TSharedPtr<FJsonValue>> Animations;for(UWidgetAnimation* Anim:BP->Animations)if(IsValid(Anim)){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Anim->GetName());O->SetNumberField(TEXT("end_seconds"),Anim->GetEndTime());O->SetNumberField(TEXT("bindings"),Anim->AnimationBindings.Num());Animations.Add(MakeShared<FJsonValueObject>(O));}
 R->SetArrayField(TEXT("widgets"),A);R->SetArrayField(TEXT("animations"),Animations);R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("inspected"));State(BP,R);
}
void Animate(const FString& P,const FString& Name,double Duration,const TArray<FTrack>& Tracks,const TSharedRef<FJsonObject>& R)
{
 if(!EditorOK()||Busy){Error(R,TEXT("Clean Editor execution outside PIE/SIE and no reentry required"));return;}TGuardValue<bool> Guard(Busy,true);
 UWidgetBlueprint* BP=Load(P);if(!BP){Error(R,TEXT("Native UserWidget Blueprint source required"));return;}
 if(FindObject<UObject>(BP,*Name)||BP->WidgetTree->FindWidget(FName(*Name))||FindFProperty<FProperty>(UUserWidget::StaticClass(),FName(*Name))||UUserWidget::StaticClass()->FindFunctionByName(FName(*Name))){Error(R,TEXT("Animation/member name already exists; create-only animation refused"));return;}
 const FName MemberName(*Name);
 if((BP->SkeletonGeneratedClass&&FindFProperty<FProperty>(BP->SkeletonGeneratedClass,MemberName))
  ||(BP->GeneratedClass&&FindFProperty<FProperty>(BP->GeneratedClass,MemberName))
  ||BP->NewVariables.ContainsByPredicate([MemberName](const FBPVariableDescription& V){return V.VarName==MemberName;}))
 {Error(R,TEXT("Animation name collides with an existing Blueprint member; no mutation"));return;}
 for(UWidgetAnimation* A:BP->Animations)if(IsValid(A)&&A->GetFName()==FName(*Name)){Error(R,TEXT("Animation already exists"));return;}
 for(const FTrack& T:Tracks)if(!SourceWidget(P,T.Widget)){Error(R,TEXT("Track widget must exist in the source tree"));return;}
 TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);UPackage* Package=BP->GetOutermost();UWorld* World=GEditor->GetEditorWorldContext().World();
 R->SetBoolField(TEXT("mutation_attempted"),true);R->SetStringField(TEXT("stage"),TEXT("create_animation"));BP->Modify();
 if(!Current(BP,KeepTree.Get(),Package,World)){Error(R,TEXT("Source changed in Modify callback; no animation allocation"));return;}
 UWidgetAnimation* Anim=NewObject<UWidgetAnimation>(BP,FName(*Name),RF_Transactional);if(!IsValid(Anim)){Error(R,TEXT("Animation allocation failed"));return;}
 if(Anim->GetFName()!=FName(*Name)){R->SetBoolField(TEXT("changed"),true);Error(R,TEXT("Animation allocation name postcondition failed; inspect partial source"));return;}
 TStrongObjectPtr<UWidgetAnimation> KeepAnimation(Anim);R->SetBoolField(TEXT("changed"),true);BP->Animations.Add(Anim);BP->OnVariableAdded(Anim->GetFName());
 if(!Current(BP,KeepTree.Get(),Package,World)||!IsValid(Anim)||Anim->GetOuter()!=BP||!BP->Animations.Contains(Anim)){Error(R,TEXT("Source/animation changed in variable notification; inspect partial animation"));return;}
 Anim->SetDisplayLabel(Name);
 Anim->MovieScene=NewObject<UMovieScene>(Anim,TEXT("MovieScene"),RF_Transactional);UMovieScene* MS=Anim->MovieScene;
 MS->SetTickResolutionDirectly(FFrameRate(24000,1));MS->SetDisplayRate(FFrameRate(30,1));const int32 End=FMath::RoundToInt(Duration*24000)+1;MS->SetPlaybackRange(0,End);
 TMap<FString,FGuid> Bindings;
 for(const FTrack& T:Tracks)
 {
  if(!Current(BP,KeepTree.Get(),Package,World)){Error(R,TEXT("Source identity changed during animation authoring"));return;}
  FGuid* Found=Bindings.Find(T.Widget);FGuid Guid;
  if(Found)Guid=*Found;
  else
  {
   UWidget* W=SourceWidget(P,T.Widget);if(!W){Error(R,TEXT("Track widget changed"));return;}Guid=MS->AddPossessable(T.Widget,W->GetClass());Bindings.Add(T.Widget,Guid);
   FWidgetAnimationBinding B;B.WidgetName=FName(*T.Widget);B.AnimationGuid=Guid;B.bIsRootWidget=false;Anim->AnimationBindings.Add(B);
  }
  if(T.Property==TEXT("opacity"))
  {
   auto Track=MS->AddTrack<UMovieSceneFloatTrack>(Guid);Track->SetPropertyNameAndPath(TEXT("RenderOpacity"),TEXT("RenderOpacity"));auto Section=Cast<UMovieSceneFloatSection>(Track->CreateNewSection());if(!Section){Error(R,TEXT("Float section creation failed"));return;}
   Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0),FFrameNumber(End)));Track->AddSection(*Section);
   for(const FKey& K:T.Keys)Section->GetChannel().AddCubicKey(FFrameNumber(static_cast<int32>(FMath::RoundToInt(K.Time*24000))),K.X,RCTM_Auto);
  }
  else
  {
   UMovieSceneTrack* Base=MS->AddTrack(UMovieScene2DTransformTrack::StaticClass(),Guid);auto Track=Cast<UMovieScenePropertyTrack>(Base);Track->SetPropertyNameAndPath(TEXT("RenderTransform"),TEXT("RenderTransform"));auto Section=Cast<UMovieScene2DTransformSection>(Base->CreateNewSection());if(!Section){Error(R,TEXT("Transform section creation failed"));return;}
   Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0),FFrameNumber(End)));Section->SetMask(FMovieScene2DTransformMask(EMovieScene2DTransformChannel::Translation));Base->AddSection(*Section);
   auto Channels=Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();if(Channels.Num()!=7){Error(R,TEXT("Installed transform channel layout differs"));return;}
   for(const FKey& K:T.Keys){const FFrameNumber F(static_cast<int32>(FMath::RoundToInt(K.Time*24000)));Channels[0]->AddCubicKey(F,K.X,RCTM_Auto);Channels[1]->AddCubicKey(F,K.Y,RCTM_Auto);}
  }
 }
 if(!Current(BP,KeepTree.Get(),Package,World)){Error(R,TEXT("Source changed after animation authoring"));return;}
 BP->MarkPackageDirty();
 if(!Current(BP,KeepTree.Get(),Package,World)){Error(R,TEXT("Source changed in dirty notification; partial animation retained"));return;}
 FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
 if(!Current(BP,KeepTree.Get(),Package,World)){Error(R,TEXT("Source changed in Blueprint notification; inspect partial animation"));return;}
 R->SetStringField(TEXT("animation"),Anim->GetPathName());R->SetBoolField(TEXT("needs_finalize"),true);R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("animation_authored_unsaved"));State(BP,R);
}
void Finalize(const FString& P,bool Save,const TSharedRef<FJsonObject>& R)
{
 if(!EditorOK()||Busy){Error(R,TEXT("Editor outside PIE/SIE and no reentry required"));return;}TGuardValue<bool> Guard(Busy,true);
 UWidgetBlueprint* BP=Load(P);if(!BP){Error(R,TEXT("Native UserWidget Blueprint source required"));return;}
 TStrongObjectPtr<UWidgetBlueprint> KeepBP(BP);TStrongObjectPtr<UWidgetTree> KeepTree(BP->WidgetTree);UPackage* Package=BP->GetOutermost();UWorld* World=GEditor->GetEditorWorldContext().World();
 R->SetBoolField(TEXT("mutation_attempted"),true);R->SetBoolField(TEXT("changed"),true);R->SetBoolField(TEXT("compile_attempted"),true);R->SetStringField(TEXT("stage"),TEXT("compile"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 if(!Current(BP,KeepTree.Get(),Package,World)){Error(R,TEXT("Source changed during structural notification/compile; no further compile or save"));return;}
 FKismetEditorUtilities::CompileBlueprint(BP);
 if(!Current(BP,KeepTree.Get(),Package,World)){Error(R,TEXT("Source identity changed after compile; no save attempted"));return;}State(BP,R);
 if(BP->Status!=BS_UpToDate&&BP->Status!=BS_UpToDateWithWarnings){Error(R,TEXT("Blueprint compilation failed; dirty source retained"));return;}
 if(Save){R->SetStringField(TEXT("stage"),TEXT("save"));R->SetBoolField(TEXT("save_attempted"),true);const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(BP,false);R->SetBoolField(TEXT("saved"),Saved);if(!Current(BP,KeepTree.Get(),Package,World)||!Saved){Error(R,TEXT("Save failed or source changed; inspect actual state"));return;}}
 R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),Save?TEXT("compiled_saved"):TEXT("compiled_unsaved"));State(BP,R);
}
}
