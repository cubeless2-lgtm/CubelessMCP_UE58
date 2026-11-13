#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMocapWindowLibrary.generated.h"
class UCubelessMocapSettings;

// Editor-only semantic controls. No OS input injection or MCP dependency.
UCLASS()
class CBMEDITOR_API UCubelessMocapWindowLibrary : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 // Creates the tool window once if necessary; settings belong to that window.
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap|Window") static UCubelessMocapSettings* GetSettings();
 // Commands return JSON: success, action, message, state. Opening/seeking is async;
 // poll GetState until media_ready and !seek_pending before inspecting a frame.
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap|Window") static FString OpenVideo(const FString& FilePath);
 // source, result, play, pause, stop, seek (absolute source seconds), step
 // (signed integer source frames), rate (preview only, 0.1..4).
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap|Window") static FString Control(const FString& Action, double Value=0);
 // End is exclusive. Switches to source review; shares UI snapping/bounds.
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap|Window") static FString SetRange(double StartSeconds, double EndSeconds);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap|Window") static FString SelectResult(bool HandFlow, bool FingerCorrection);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap|Window") static FString SetLayout(float PreviewFraction, float VideoFraction, FVector2D WindowSize);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap|Window") static FString GetState();
 // Closes only the tool window, refuses while a mocap/save job is running.
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap|Window") static FString Close();
};
