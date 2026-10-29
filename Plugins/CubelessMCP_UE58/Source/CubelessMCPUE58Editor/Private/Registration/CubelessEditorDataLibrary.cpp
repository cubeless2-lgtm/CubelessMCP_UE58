#include "Registration/CubelessEditorDataLibrary.h"
#include "Compatibility/UE58/CubelessEditorDataApi.h"
#include "Compatibility/UE58/CubelessNiagaraParticlePositionsApi.h"
#include "Compatibility/UE58/CubelessMaterialCommentsApi.h"
using namespace Cubeless::UE58::EditorData;
FString UCubelessEditorDataLibrary::InspectBlueprintDefinition(UObject* O){return Blueprint(O);}
FString UCubelessEditorDataLibrary::InspectEditorWorldConfiguration(){return World();}
FString UCubelessEditorDataLibrary::InspectStaticMeshMetadata(UObject* O){return Mesh(O);}
FString UCubelessEditorDataLibrary::InspectComponentAuthoringData(UObject* O){return Component(O);}
FString UCubelessEditorDataLibrary::InspectWidgetAnimationData(UObject* O){return Widget(O);}
FString UCubelessEditorDataLibrary::InspectPCGExtractionSettings(UObject* O){return PCG(O);}
FString UCubelessEditorDataLibrary::InspectEditorPerformanceSettings(){return Performance();}
FString UCubelessEditorDataLibrary::RerunActorConstruction(UObject* O,bool E){return RefreshActor(O,E);}
FString UCubelessEditorDataLibrary::RefreshComponentRenderState(UObject* O,bool E){return RefreshComponent(O,E);}
FString UCubelessEditorDataLibrary::SetAssetPackageDirty(UObject* O,bool D,bool C){return PackageDirty(O,D,C);}
FString UCubelessEditorDataLibrary::SpawnEditorActorDeferred(UClass* C,const FString& N,const FTransform& T,bool E){return Spawn(C,N,T,E);}

FString UCubelessEditorDataLibrary::InspectNiagaraParticlePositions(const FString& P,int32 E,int32 M){return Cubeless::UE58::NiagaraParticlePositions::Inspect(P,E,M);}
FString UCubelessEditorDataLibrary::MaterialComments(UObject* A,const FString& Op,const FString& T,int32 X,int32 Y,int32 W,int32 H,bool E,bool S){return Cubeless::UE58::MaterialComments::Run(A,Op,T,X,Y,W,H,E,S);}
