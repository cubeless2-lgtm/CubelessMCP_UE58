#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessViewportCaptureLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessViewportCaptureLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="list_viewport_bookmarks"))
    static FString ListViewportBookmarks();
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="set_active_viewport_realtime"))
    static FString SetActiveViewportRealtime(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="capture_viewport_bookmark_screenshot"))
    static FString CaptureViewportBookmarkScreenshot(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="take_screenshot"))
    static FString TakeScreenshot(FString ParamsJson);
};
