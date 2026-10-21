#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessLandscapeLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessLandscapeLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="list_landscapes"))
    static FString ListLandscapes();
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="get_landscape_info"))
    static FString GetLandscapeInfo(FString ActorLabel,FString PrivateContextJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="create_landscape"))
    static FString CreateLandscape(FString ActorLabel,FString MaterialPath,FString LocationJson,FString ScaleJson,
        int64 ComponentCountX,int64 ComponentCountY,int64 SectionsPerComponent,int64 QuadsPerSection,int64 HeightValue,
        bool bAllowNonTempLevel,FString RequiredRoot,FString PrivateContextJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="set_landscape_material"))
    static FString SetLandscapeMaterial(FString MaterialPath,FString ActorLabel,FString PrivateContextJson);
};
