#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAuthoringAccessLibrary.generated.h"
class UUserWidget;

// Editor authoring only; never insert these calls in generated game graphs.
UCLASS()
class UCubelessAuthoringAccessLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static UUserWidget* CreateEditorPreviewWidget(UObject* Owner,UClass* WidgetClass,const FString& Name,bool Execute,FString& Result);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectEditorViewportConfiguration();
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectRigVMTemplateCatalog(const FString& Filter,int32 Offset,int32 Limit);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString DeactivateEditorNiagaraComponent(UObject* Object,bool Execute);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectEditorAuthoringDetails(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectEmbeddedRichCurves(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectRigVMLibraryFunctions(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectEditorActorMetadata(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectComponentRegistrationState(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectObjectClassHierarchy(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectPreviewMeshReference(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectSkeletalMeshAuthoringData(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectRigVMUnitCatalog(const FString& Filter,bool IncludeDeprecated,int32 Offset,int32 Limit);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectAssetEditingState(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectFloatCurve(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString SetFloatCurveKeys(UObject* Object,const FString& Keys,bool AllowReplace,bool Execute,bool Save,bool AllowDirty);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectBlueprintTimelines(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectAssetImportSources(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectSpecializedAnimGraph(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectAnimationNativeData(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectTextureSourcePixels(UObject* Object,int32 Mip,int32 Start,int32 Count);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString RefreshTextureEditorData(UObject* Object,bool Execute);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectSkeletalComponentState(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString SetSkeletalPostProcessDisabled(UObject* Object,bool Disabled,bool Execute);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString TickSkeletalAnimation(UObject* Object,double Delta,bool Refresh,bool Execute);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString RemoveSkeletonBones(UObject* Object,const FString& Names,bool Children,bool AllowRemove,bool AllowDependentUpdates,bool Execute,bool Save,bool AllowDirty);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString InspectOptimusDeformer(UObject* Object);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly),Category="Cubeless|Internal")
 static FString CompileOptimusDeformer(UObject* Object,bool Execute);
};
