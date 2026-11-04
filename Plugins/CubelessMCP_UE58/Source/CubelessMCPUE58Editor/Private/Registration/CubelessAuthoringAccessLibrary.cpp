#include "Registration/CubelessAuthoringAccessLibrary.h"
#include "Compatibility/UE58/CubelessAuthoringAccessApi.h"
using namespace Cubeless::UE58::AuthoringAccess;
FString UCubelessAuthoringAccessLibrary::InspectAssetEditingState(UObject* O){return EditorState(O);}
FString UCubelessAuthoringAccessLibrary::InspectFloatCurve(UObject* O){return Curve(O);}
FString UCubelessAuthoringAccessLibrary::SetFloatCurveKeys(UObject* O,const FString& K,bool R,bool E,bool S,bool D){return SetCurve(O,K,R,E,S,D);}
FString UCubelessAuthoringAccessLibrary::InspectBlueprintTimelines(UObject* O){return Timelines(O);}
FString UCubelessAuthoringAccessLibrary::InspectAssetImportSources(UObject* O){return ImportSources(O);}
FString UCubelessAuthoringAccessLibrary::InspectSpecializedAnimGraph(UObject* O){return AnimNodes(O);}
FString UCubelessAuthoringAccessLibrary::InspectAnimationNativeData(UObject* O){return Animation(O);}
FString UCubelessAuthoringAccessLibrary::InspectTextureSourcePixels(UObject* O,int32 M,int32 S,int32 C){return TextureSource(O,M,S,C);}
FString UCubelessAuthoringAccessLibrary::RefreshTextureEditorData(UObject* O,bool E){return RefreshTexture(O,E);}
FString UCubelessAuthoringAccessLibrary::InspectSkeletalComponentState(UObject* O){return SkeletalComponent(O,TEXT("inspect"),false,0,false,false);}
FString UCubelessAuthoringAccessLibrary::SetSkeletalPostProcessDisabled(UObject* O,bool D,bool E){return SkeletalComponent(O,TEXT("post_process"),D,0,false,E);}
FString UCubelessAuthoringAccessLibrary::TickSkeletalAnimation(UObject* O,double D,bool R,bool E){return SkeletalComponent(O,TEXT("tick"),false,D,R,E);}
FString UCubelessAuthoringAccessLibrary::RemoveSkeletonBones(UObject* O,const FString& N,bool C,bool R,bool U,bool E,bool S,bool D){return RemoveBones(O,N,C,R,U,E,S,D);}
FString UCubelessAuthoringAccessLibrary::InspectOptimusDeformer(UObject* O){return Optimus(O,false,false);}
FString UCubelessAuthoringAccessLibrary::CompileOptimusDeformer(UObject* O,bool E){return Optimus(O,true,E);}
