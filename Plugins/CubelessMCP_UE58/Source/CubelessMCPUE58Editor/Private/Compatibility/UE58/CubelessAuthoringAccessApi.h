#pragma once
#include "CoreMinimal.h"
class UUserWidget;
namespace Cubeless::UE58::AuthoringAccess
{
UUserWidget* CreatePreviewWidget(UObject* Owner,UClass* WidgetClass,const FString& Name,bool Execute,FString& Result);
FString EditorAuthoringDetails(UObject* Object);
FString EmbeddedRichCurves(UObject* Object);
FString RigVMLibraryFunctions(UObject* Object);
FString EditorViewportConfiguration();
FString RigVMTemplateCatalog(const FString& Filter,int32 Offset,int32 Limit);
FString DeactivateNiagara(UObject* Object,bool Execute);
FString EditorActorMetadata(UObject* Object);
FString ComponentRegistrationState(UObject* Object);
FString ObjectClassHierarchy(UObject* Object);
FString PreviewMeshReference(UObject* Object);
FString SkeletalMeshAuthoringData(UObject* Object);
FString RigVMUnitCatalog(const FString& Filter,bool IncludeDeprecated,int32 Offset,int32 Limit);
FString EditorState(UObject* Object);
FString Curve(UObject* Object);
FString SetCurve(UObject* Object,const FString& Keys,bool AllowReplace,bool Execute,bool Save,bool AllowDirty);
FString Timelines(UObject* Object);
FString ImportSources(UObject* Object);
FString AnimNodes(UObject* Object);
FString Animation(UObject* Object);
FString TextureSource(UObject* Object,int32 Mip,int32 Start,int32 Count);
FString RefreshTexture(UObject* Object,bool Execute);
FString SkeletalComponent(UObject* Object,const FString& Action,bool Disabled,double Delta,bool Refresh,bool Execute);
FString RemoveBones(UObject* Object,const FString& Names,bool Children,bool AllowRemove,bool AllowDependentUpdates,bool Execute,bool Save,bool AllowDirty);
FString Optimus(UObject* Object,bool Compile,bool Execute);
}
